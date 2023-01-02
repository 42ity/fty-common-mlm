/*  =========================================================================
    fty_common_mlm_basic_mailbox_server - Basic malamute synchronous mailbox server

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
    fty_common_mlm_basic_mailbox_server - Basic malamute synchronous mailbox server
@discuss
@end
*/

#include "fty_common_mlm_basic_mailbox_server.h"
#include "fty_common_mlm_guards.h"
#include <fty_log.h>
#include <stdexcept>

namespace mlm {

using namespace fty;

using Subject = std::string;

MlmBasicMailboxServer::MlmBasicMailboxServer(
    zsock_t* pipe, fty::SyncServer& server, const std::string& name, const std::string& endpoint)
    : mlm::MlmAgent(pipe)
    , m_server(server)
    , m_name(name)
    , m_endpoint(endpoint)
{
    connect(m_endpoint.c_str(), m_name.c_str());
}

bool MlmBasicMailboxServer::handleMailbox(zmsg_t* message)
{
    // try to address the request
    try {
        Subject subject(mlm_client_subject(client()));
        Sender  sender(mlm_client_sender(client()));

        // ignore non "REQUEST" message
        if (subject != "REQUEST") {
            log_warning("<%s> Received mailbox message with subject '%s' from '%s', ignoring",
                m_name.c_str(), subject.c_str(), sender.c_str());
            return true;
        }

        log_debug("<%s> Received mailbox message with subject '%s' from '%s' with %i frames",
            m_name.c_str(), subject.c_str(), sender.c_str(), zmsg_size(message));

        // Message is valid if the header contain at least one frame
        // frame 1 is the correlation id
        // next are the message payload

        // unstack correlation id
        ZstrGuard correlationId(zmsg_popstr(message));
        if (!(correlationId && (*correlationId))) {
            // invalid correlation id, it's a bad frame we ignore it
            throw std::runtime_error("<" + m_name + "> Correlation id frame is empty");
        }

        // unstack all the next
        // TODO define a maximum to avoid DOS
        Payload payload;
        for (char* s = zmsg_popstr(message); s; s = zmsg_popstr(message)) {
            payload.push_back(s);
            zstr_free(&s);
        }

        // Execute the request
        // extract the sender name from sender: <Sender>.[thread id in hexa]
        Sender senderName = sender.substr(0, (sender.size() - (sizeof(pid_t) * 2) - 1));
        Payload results = m_server.handleRequest(senderName, payload);

        // send the result **only if not empty**
        if (!results.empty()) {
            zmsg_t* reply = zmsg_new();
            zmsg_addstr(reply, correlationId); // repeat cid on reply
            for (auto& result : results) {
                zmsg_addstr(reply, result.c_str());
            }

            int r = mlm_client_sendto(client(), sender.c_str(), "REPLY", nullptr, 1000, &reply);
            zmsg_destroy(&reply);
            if (r != 0) {
                log_error("<%s> s_handle_mailbox: failed to send reply to %s ",
                    m_name.c_str(), sender.c_str());
            }
        }
    }
    catch (const std::exception& e) {
        log_error("<%s> Unexpected error: %s", m_name.c_str(), e.what());
    }
    catch (...) {
        log_error("<%s> Unexpected error: unknown", m_name.c_str());
    }

    return true;
}

} // namespace mlm
