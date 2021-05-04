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
    // if (NULL != _uuid) zuuid_destroy (&_uuid);
    // if (NULL != _poller) zpoller_destroy (&_poller);
    // if (NULL != _client) mlm_client_destroy (&_client);
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

#if 0
void fty_common_mlm_tntmlm_test(bool verbose)
{
    std::string& ref = const_cast<std::string&>(MlmClient::ENDPOINT);
    ref              = "inproc://tntmlm-catch-1";
    assert(MlmClient::ENDPOINT == "inproc://tntmlm-catch-1");

    zactor_t* server = zactor_new(mlm_server, (void*)"Malamute");
    zstr_sendx(server, "BIND", MlmClient::ENDPOINT.c_str(), nullptr);

    { // Invariant: malamute MUST be destroyed last

        // tntmlm.h already has one pool but we can't use that one
        // for this test because valgrind will detect memory leak
        // which occurs because malamute server is destroyed only
        // after clients allocated in the pool of the header file
        MlmClientPool mlm_pool_test{3};

        mlm_client_t* agent1 = mlm_client_new();
        mlm_client_connect(agent1, MlmClient::ENDPOINT.c_str(), 1000, "AGENT1");

        mlm_client_t* agent2 = mlm_client_new();
        mlm_client_connect(agent2, MlmClient::ENDPOINT.c_str(), 1000, "AGENT2");


        printf("\n ---- max pool ----\n");
        {
            assert(mlm_pool_test.getMaximumSize() == 3);
            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            assert(ui_client.getPointer() != NULL);

            MlmClientPool::Ptr ui_client2 = mlm_pool_test.get();
            assert(ui_client2.getPointer() != NULL);

            MlmClientPool::Ptr ui_client3 = mlm_pool_test.get();
            assert(ui_client3.getPointer() != NULL);

            MlmClientPool::Ptr ui_client4 = mlm_pool_test.get();
            assert(ui_client4.getPointer() != NULL);

            MlmClientPool::Ptr ui_client5 = mlm_pool_test.get();
            assert(ui_client5.getPointer() != NULL);
        }
        assert(mlm_pool_test.getMaximumSize() == 3); // this tests the policy that excess clients
                                                     // that were created are dropped
        printf("OK");

        printf("\n ---- no reply ----\n");
        {
            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            assert(ui_client.getPointer() != NULL);

            zmsg_t* reply = ui_client->recv("uuid1", 1);
            assert(reply == NULL);

            reply = ui_client->recv("uuid1", 0);
            assert(reply == NULL);
        }
        printf("OK\n");

        printf("\n ---- send - correct reply ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid1");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            assert(ui_client.getPointer() != NULL);
            MlmClientPool::Ptr ui_client2 = mlm_pool_test.get();
            assert(ui_client2.getPointer() != NULL);
            int rv = ui_client2->sendto("AGENT1", "TEST", 1000, &msg);
            assert(rv == 0);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            assert(agent1_msg != NULL);
            char* agent1_msg_uuid = zmsg_popstr(agent1_msg);
            zmsg_destroy(&agent1_msg);
            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, agent1_msg_uuid);
            zstr_free(&agent1_msg_uuid);
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);
            assert(rv == 0);

            zmsg_t* reply = ui_client2->recv("uuid1", 1);
            assert(reply != NULL);
            char* tmp = zmsg_popstr(reply);
            assert(streq(tmp, "abc"));
            zstr_free(&tmp);
            tmp = zmsg_popstr(reply);
            assert(streq(tmp, "def"));
            zstr_free(&tmp);
            zmsg_destroy(&reply);
        }
        printf("OK\n");

        printf("\n ---- send - reply with bad uuid ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid1");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            assert(ui_client.getPointer() != NULL);
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            assert(agent1_msg != NULL);
            zmsg_destroy(&agent1_msg);
            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID");
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            mlm_client_sendto(agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);

            zmsg_t* reply = ui_client->recv("uuid1", 1);
            assert(reply == NULL);
        }
        printf("OK");

        printf("\n ---- send - correct reply - expect bad uuid ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid1");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            assert(ui_client.getPointer() != NULL);
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            assert(agent1_msg != NULL);
            zmsg_destroy(&agent1_msg);
            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "uuid1");
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            mlm_client_sendto(agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);

            zmsg_t* reply = ui_client->recv("BAD-UUID", 1);
            assert(reply == NULL);
        }
        printf("OK");

        printf("\n ---- send - reply 3x with bad uuid first - then correct reply ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid34");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            assert(ui_client.getPointer() != NULL);
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            assert(agent1_msg != NULL);
            zmsg_destroy(&agent1_msg);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID1");
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            int rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);
            assert(rv == 0);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID2");
            zmsg_addstr(agent1_msg, "abc");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);
            assert(rv == 0);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID3");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);
            assert(rv == 0);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "uuid34");
            zmsg_addstr(agent1_msg, "OK");
            zmsg_addstr(agent1_msg, "element");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);
            assert(rv == 0);

            zmsg_t* reply = ui_client->recv("uuid34", 2);
            assert(reply != NULL);
            char* tmp = zmsg_popstr(reply);
            assert(streq(tmp, "OK"));
            zstr_free(&tmp);
            tmp = zmsg_popstr(reply);
            assert(streq(tmp, "element"));
            zstr_free(&tmp);
            zmsg_destroy(&reply);
        }
        printf("OK");
        printf("\n ---- Simulate thread switch ----\n");
        {
            MlmClientPool::Ptr ui_client = mlm_pool_test.get();

            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid25");
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            msg = zmsg_new();
            zmsg_addstr(msg, "uuid33");
            ui_client->sendto("AGENT2", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            assert(agent1_msg != NULL);
            zmsg_destroy(&agent1_msg);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "uuid25");
            zmsg_addstr(agent1_msg, "ABRAKADABRA");
            int rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), NULL, 1000, &agent1_msg);
            assert(rv == 0);

            zmsg_t* agent2_msg = mlm_client_recv(agent2);
            assert(agent2_msg != NULL);
            zmsg_destroy(&agent2_msg);

            agent2_msg = zmsg_new();
            zmsg_addstr(agent2_msg, "uuid33");
            zmsg_addstr(agent2_msg, "HABAKUKA");
            rv = mlm_client_sendto(
                agent2, mlm_client_sender(agent2), mlm_client_subject(agent2), NULL, 1000, &agent2_msg);
            assert(rv == 0);

            zmsg_t* reply = ui_client->recv("uuid33", 1);
            assert(reply != NULL);
            char* tmp = zmsg_popstr(reply);
            assert(streq(tmp, "HABAKUKA"));
            zstr_free(&tmp);
            zmsg_destroy(&reply);
        }
        // TODO: test for requestreply
        mlm_client_destroy(&agent1);
        mlm_client_destroy(&agent2);
    }

    zactor_destroy(&server);
    printf("OK");
}
#endif
