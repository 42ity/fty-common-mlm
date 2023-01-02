/*  =========================================================================
    fty_common_mlm_agent - Helper C++ class to build a malamute agent (server)

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty_common_mlm_agent - Helper C++ class to build a malamute agent (server)
@discuss
@end
*/

#include "fty_common_mlm_agent.h"
#include "fty_common_mlm_guards.h"
#include <fty_log.h>
#include <stdexcept>

namespace mlm {
MlmAgent::~MlmAgent()
{
    zpoller_destroy(&m_defaultZpoller);
    mlm_client_destroy(&m_client);
}

MlmAgent::MlmAgent(zsock_t* pipe, const char* endpoint, const char* address, int pollerTimeout, int connectionTimeout)
    : m_client(mlm_client_new())
    , m_address(address ? address : "")
    , m_pipe(pipe)
    , m_lastTick(zclock_mono())
    , m_pollerTimeout(pollerTimeout)
    , m_defaultZpoller(nullptr)
{
    if (!m_client) {
        log_error("mlm_client_new() failed.");
        throw std::runtime_error("Can't create client");
    }
    if (endpoint && address) {
        connect(endpoint, address, connectionTimeout);
    }
}

void MlmAgent::connect(const char* endpoint, const char* address, int connectionTimeout)
{
    logDebug("{} endpoint: {}", m_address, (endpoint ? endpoint : "<null>"));
    int r = mlm_client_connect(m_client, endpoint, uint32_t(connectionTimeout), address);
    if (r != 0) {
        log_error("mlm_client_connect(endpoint = '%s', timeout = '%d', address = '%s') failed.",
            endpoint, connectionTimeout, address);
        throw std::runtime_error("Can't connect client");
    }
}

void MlmAgent::mainloop()
{
    logDebug("{} actor started", m_address);

    zsock_signal(m_pipe, 0);

    while (!zsys_interrupted) {
        void* which = zpoller_wait(zpoller(), m_pollerTimeout);

        if (which == nullptr) {
            if (zpoller_terminated(zpoller()) || zsys_interrupted) {
                logDebug("{} interrupted", m_address);
                break;
            }
        }

        // Handle periodic callback hook
        if (m_pollerTimeout > 0) {
            int64_t curClock = zclock_mono();
            if ((m_lastTick + m_pollerTimeout) < curClock) {
                if (!tick()) {
                    break;
                }
                m_lastTick = curClock;
            }
        }

        if (which == m_pipe) {
            ZmsgGuard message(zmsg_recv(m_pipe));
            if (message == nullptr) {
                logDebug("{} interrupted", m_address);
                break;
            }
            if (!handlePipe(message.get())) {
                break;
            }
        }
        else if (which == mlm_client_msgpipe(m_client)) {
            ZmsgGuard message(mlm_client_recv(m_client));
            if (message == nullptr) {
                logDebug("{} interrupted", m_address);
                break;
            } else if (streq(mlm_client_command(m_client), "MAILBOX DELIVER")) {
                if (!handleMailbox(message.get())) {
                    break;
                }
            } else if (streq(mlm_client_command(m_client), "STREAM DELIVER")) {
                if (!handleStream(message.get())) {
                    break;
                }
            } else {
                log_warning("Unknown malamute pattern: '%s'. Message subject: '%s', sender: '%s'.",
                    mlm_client_command(m_client), mlm_client_subject(m_client), mlm_client_sender(m_client));
            }
        }
        else if (which != nullptr) {
            ZmsgGuard message(zmsg_recv(which));
            if (message == nullptr) {
                logDebug("{} interrupted", m_address);
                break;
            }
            if (!handleOther(message.get(), which)) {
                break;
            }
        }
    }

    logInfo("{} actor ended", m_address);
}

bool MlmAgent::handlePipe(zmsg_t* message)
{
    ZstrGuard actor_command(zmsg_popstr(message));

    //  $TERM actor command implementation is required by zactor_t interface
    if (streq(actor_command, "$TERM")) {
        return false;
    }

    return true;
}

zpoller_t* MlmAgent::zpoller(void)
{
    if (!m_defaultZpoller) {
        m_defaultZpoller = zpoller_new(m_pipe, mlm_client_msgpipe(m_client), nullptr);
    }
    return m_defaultZpoller;
}

} // namespace mlm
