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
    _poller = zpoller_new(mlm_client_msgpipe(_client), NULL);
    _uuid   = zuuid_new();

    connect();
}

MlmClient::~MlmClient()
{
    zuuid_destroy(&_uuid);
    zpoller_destroy(&_poller);
    mlm_client_destroy(&_client);
}

zmsg_t* MlmClient::recv(const std::string& uuid, uint32_t timeout_s)
{
    if (!connected() && !connect()) {
        return NULL;
    }

    if (!_poller) {
        log_debug("poller not initialized");
        return NULL;
    }

    if (timeout_s > 300) { // 300 s. max
        timeout_s = 300;
    }

    int64_t quit_ms = zclock_mono() + (timeout_s * 1000);

    while (true) {
        int timeleft_ms = int(quit_ms - zclock_mono());
        if (timeleft_ms <= 0) {
            log_debug("timeleft_ms: %dms", timeleft_ms);
            break;
        }

        void* which = zpoller_wait(_poller, timeleft_ms);
        if (!which) {
            log_debug("zpoller_wait() returned NULL. (timeleft_ms: %dms, expired: %s, terminated: %s)",
                timeleft_ms,
                (zpoller_expired(_poller) ? "true" : "false"),
                (zpoller_terminated(_poller) ? "true" : "false"));
            break;
        }

        zmsg_t* msg = mlm_client_recv(_client);
        if (msg) {
            char* uuid_recv = zmsg_popstr(msg);
            if (!(uuid_recv && streq(uuid_recv, uuid.c_str()))) {
                zmsg_destroy(&msg);
            }
            zstr_free(&uuid_recv);
            if (msg) {
                return msg; // msg received
            }
        }
    }

    return NULL; // timedout, terminated, ...
}

int MlmClient::sendto(const std::string& address, const std::string& subject, uint32_t timeout_s, zmsg_t** content_p)
{
    if (!connected() && !connect()) {
        return -1;
    }

    int r = mlm_client_sendto(_client, address.c_str(), subject.c_str(), NULL, timeout_s * 1000, content_p);
    return r;
}

zmsg_t* MlmClient::requestreply(
    const std::string& address, const std::string& subject, uint32_t timeout_s, zmsg_t** content_p)
{
    if (!(content_p && *content_p)) {
        log_debug("content_p is invalid");
        return NULL;
    }

    if (address.empty()) {
        log_debug("address is empty");
        zmsg_destroy(content_p);
        return NULL;
    }

    if (!connected() && !connect()) {
        zmsg_destroy(content_p);
        return NULL;
    }
    if (!_poller) {
        log_debug("poller not initialized");
        zmsg_destroy(content_p);
        return NULL;
    }

    std::string uid;
    {
        zuuid_t* zuuid = zuuid_new();
        uid = zuuid ? zuuid_str(zuuid) : std::to_string(random());
        zuuid_destroy(&zuuid);

        if (uid.empty()) {
            log_debug("uid is empty");
            zmsg_destroy(content_p);
            return NULL;
        }
    }

    if (timeout_s > 300) { // 300 s. max
        timeout_s = 300;
    }

    {
        const int sendto_timeout_ms = 5000;

        // prepend 'REQUEST'/uid to message and send it
        zmsg_pushstr(*content_p, uid.c_str());
        zmsg_pushstr(*content_p, "REQUEST");
        int r = mlm_client_sendto(_client, address.c_str(), subject.c_str(), NULL, sendto_timeout_ms, content_p);
        zmsg_destroy(content_p);

        if (r != 0) {
            log_error("Sending request to %s (subject: %s) failed with result %d.", address.c_str(), subject.c_str(), r);
            return NULL;
        }
    }

    int64_t quit_ms = zclock_mono() + (timeout_s * 1000);

    // wait for reply with the right uid
    while (true) {
        int timeleft_ms = int(quit_ms - zclock_mono());
        if (timeleft_ms <= 0) {
            log_debug("timeleft_ms: %dms", timeleft_ms);
            break;
        }

        void* which = zpoller_wait(_poller, timeleft_ms);
        if (!which) {
            log_debug("zpoller_wait() returned NULL. (timeleft_ms: %dms, expired: %s, terminated: %s)",
                timeleft_ms,
                (zpoller_expired(_poller) ? "true" : "false"),
                (zpoller_terminated(_poller) ? "true" : "false"));
            break;
        }

        zmsg_t* reply = mlm_client_recv(_client);
        if (reply) {
            char* msg_uuid = zmsg_popstr(reply);
            char* msg_cmd  = zmsg_popstr(reply);
            if (!(msg_cmd && streq(msg_cmd, "REPLY"))
                || !(msg_uuid && streq(msg_uuid, uid.c_str()))
            ){
                // this is not the message we are waiting for
                log_info("Discarting unexpected/invalid message from %s (topic %s)", sender(), this->subject());
                zmsg_destroy(&reply);
            }
            zstr_free(&msg_cmd);
            zstr_free(&msg_uuid);

            if (reply) {
                return reply; // msg received
            }
        }
    }

    return NULL; // timedout, terminated, ...
}

bool MlmClient::connect()
{
    if (!_client) {
        log_error("client not initialized");
        return false;
    }

    std::string id{_uuid ? zuuid_str_canonical(_uuid) : std::to_string(random())};
    std::string name{"rest." + id};

    const int timeout_ms = 5000;
    int r = mlm_client_connect(_client, ENDPOINT.c_str(), timeout_ms, name.c_str());
    if (r == -1) {
        log_error("mlm_client_connect (endpoint = '%s', timeout = %d, address = '%s') failed",
            ENDPOINT.c_str(),
            timeout_ms,
            name.c_str());
        return false;
    }
    return true;
}

