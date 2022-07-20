/*  =========================================================================
    fty_common_mlm_tntmlm - malamute client to help MAILBOX REQUEST/REPLY with a backend Agent

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
    fty_common_mlm_tntmlm -
@discuss
@end
*/

#include "fty_common_mlm_tntmlm.h"
#include "fty_common_mlm_utils.h"
#include "fty_common_mlm_pool.h"

MlmClientPool mlm_pool{20};

const std::string MlmClient::ENDPOINT = MLM_ENDPOINT;

MlmClient::MlmClient()
{
    _client = mlm_client_new();
    _uuid   = zuuid_new();
    _poller = zpoller_new(mlm_client_msgpipe(_client), nullptr);
    connect();
}

MlmClient::~MlmClient()
{
    zuuid_destroy(&_uuid);
    zpoller_destroy(&_poller);
    mlm_client_destroy(&_client);
}

zmsg_t* MlmClient::recv(const std::string& uuid, uint32_t timeout)
{
    if (!connected()) {
        connect();
    }
    uint64_t wait = timeout;
    if (wait > 300) {
        wait = 300;
    }

    uint64_t start          = static_cast<uint64_t>(zclock_mono());
    uint64_t poller_timeout = wait * 1000;

    while (true) {
        void* which = zpoller_wait(_poller, int(poller_timeout));
        if (which == nullptr) {
            log_warning("zpoller_wait (timeout = '%" PRIu64
                        "') returned NULL. zpoller_expired == '%s', zpoller_terminated == '%s'",
                poller_timeout, zpoller_expired(_poller) ? "true" : "false",
                zpoller_terminated(_poller) ? "true" : "false");
            return nullptr;
        }
        uint64_t timestamp = static_cast<uint64_t>(zclock_mono());
        if (timestamp - start >= poller_timeout) {
            poller_timeout = 0;
        } else {
            poller_timeout = poller_timeout - (timestamp - start);
        }
        zmsg_t* msg = mlm_client_recv(_client);
        if (!msg)
            continue;
        char* uuid_recv = zmsg_popstr(msg);
        if (uuid_recv && uuid.compare(uuid_recv) == 0) {
            zstr_free(&uuid_recv);
            return msg;
        }
        zstr_free(&uuid_recv);
        zmsg_destroy(&msg);
    }
}

int MlmClient::sendto(const std::string& address, const std::string& subject, uint32_t timeout, zmsg_t** content_p)
{
    if (!connected()) {
        connect();
    }
    return mlm_client_sendto(_client, address.c_str(), subject.c_str(), nullptr, timeout, content_p);
}

zmsg_t* MlmClient::requestreply(
    const std::string& address, const std::string& subject, uint32_t timeout, zmsg_t** content_p)
{
    if (!content_p || !*content_p || address.empty())
        return nullptr;

    if (!connected()) {
        connect();
    }
    if (timeout > 300)
        timeout = 300;
    int64_t quit = zclock_mono() + timeout * 1000;

    std::string uid;
    {
        // prepend REQ/uuid to message and send it
        zmsg_t* msg = zmsg_dup(*content_p);

        zuuid_t* msguid = zuuid_new();
        uid             = zuuid_str(msguid);
        zuuid_destroy(&msguid);

        zmsg_pushstr(msg, uid.c_str());
        zmsg_pushstr(msg, "REQUEST");
        int sendresult = mlm_client_sendto(_client, address.c_str(), subject.c_str(), nullptr, timeout, &msg);
        if (sendresult != 0) {
            log_error("Sending request to %s (topic %s) failed with result %i.", address.c_str(), subject.c_str(),
                sendresult);
            zmsg_destroy(&msg);
            return nullptr;
        }
        zmsg_destroy(content_p);
    }
    {
        // wait for reply with the right uuid
        while (true) {
            int64_t poller_timeout = quit - zclock_mono();
            if (poller_timeout <= 0) {
                return nullptr;
            }
            void* which = zpoller_wait(_poller, int(poller_timeout));
            if (which == nullptr) {
                log_warning("zpoller_wait (timeout = '%" PRIu64
                            "') returned NULL. zpoller_expired == '%s', zpoller_terminated == '%s'",
                    poller_timeout, zpoller_expired(_poller) ? "true" : "false",
                    zpoller_terminated(_poller) ? "true" : "false");
                return nullptr;
            }
            zmsg_t* msg = mlm_client_recv(_client);
            if (!msg)
                continue;
            char* msg_uuid = zmsg_popstr(msg);
            char* msg_cmd  = zmsg_popstr(msg);
            if (!msg_cmd || !streq(msg_cmd, "REPLY") || !msg_uuid || !streq(msg_uuid, uid.c_str())) {
                // this is not the message we are waiting for
                log_info("Discarting unexpected/invalid message from %s (topic %s)", sender(), this->subject());
                zmsg_destroy(&msg);
            }
            zstr_free(&msg_cmd);
            zstr_free(&msg_uuid);
            if (msg)
                return msg;
        }
    }
}

void MlmClient::connect()
{
    std::string name("rest.");
    name.append(zuuid_str_canonical(_uuid));
    int rv = mlm_client_connect(_client, ENDPOINT.c_str(), 5000, name.c_str());
    if (rv == -1) {
        log_error("mlm_client_connect (endpoint = '%s', timeout = 5000, address = '%s') failed", ENDPOINT.c_str(),
            name.c_str());
    }
}

