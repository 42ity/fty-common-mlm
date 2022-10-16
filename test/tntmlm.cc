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

#include "fty_common_mlm_utils.h"
#include "fty_common_mlm_tntmlm.h"
#include "fty_common_mlm_pool.h"
#include <fty_log.h>
#include <catch2/catch.hpp>

#define TEST_SERVER_HELLO_RESPONSE "i'm here"

static void tntmlm_test_server(zsock_t* pipe, void* args)
{
    const char* serverAddress = static_cast<char*>(args);
    log_debug("%s: Starting...", serverAddress);

    mlm_client_t* mlm = mlm_client_new();
    zpoller_t* poller = zpoller_new(pipe, mlm_client_msgpipe(mlm), NULL);
    zsock_signal(pipe, 0);

    log_debug("%s: Started", serverAddress);

    // actor msg loop
    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, 1000);
        // No Rx
        if (!which) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
        }
        // Rx on pipe socket
        else if (which == pipe) {
            zmsg_t* msg = zmsg_recv(pipe);
            char* cmd = zmsg_popstr(msg);
            log_debug("%s: Rx pipe command '%s'", serverAddress, cmd);
            bool term = false;
            if (streq(cmd, "$TERM")) {
                term = true;
            }
            else if (streq(cmd, "CONNECT")) {
                char* endpoint = zmsg_popstr(msg);
                if (endpoint) {
                    int r = mlm_client_connect(mlm, endpoint, 5000, serverAddress);
                    if (r == -1) {
                        log_error("%s: CONNECT '%s' failed", serverAddress, endpoint);
                    }
                    else {
                        log_debug("%s: CONNECT '%s'", serverAddress, endpoint);
                    }
                }
                else {
                    log_error("%s: Missing endpoint", serverAddress);
                }
                zstr_free(&endpoint);
            }
            zstr_free(&cmd);
            zmsg_destroy(&msg);
            if (term) {
                break;
            }
        }
        // Rx on client socket
        else if (which == mlm_client_msgpipe(mlm)) {
            zmsg_t* msg = mlm_client_recv(mlm);
            const char* cmd = mlm_client_command(mlm);
            const char* subject = mlm_client_subject(mlm);
            const char* sender = mlm_client_sender(mlm);

            log_debug("%s: Rx %s/%s from %s", serverAddress, cmd, subject, sender);

            if (streq(cmd, "MAILBOX DELIVER")) {
                char* req = zmsg_popstr(msg); // enforced request
                char* uuid = zmsg_popstr(msg);

                log_debug("%s: Rx req: '%s', uuid: '%s'", serverAddress, req, uuid);

                if (req && uuid && streq(req, "REQUEST")) {
                    char* arg = zmsg_popstr(msg);

                    log_debug("%s: Rx arg: '%s'", serverAddress, arg);

                    if (arg && streq(arg, "hello")) {
                        zmsg_t* reply = zmsg_new();
                        zmsg_addstr(reply, uuid); // enforced reply
                        zmsg_addstr(reply, "REPLY");
                        zmsg_addstr(reply, TEST_SERVER_HELLO_RESPONSE);

                        log_debug("%s: Reply to %s", serverAddress, sender);

                        mlm_client_sendto(mlm, sender, subject, NULL, 1000, &reply);
                        zmsg_destroy(&reply);
                    }
                    zstr_free(&arg);
                }
                zstr_free(&uuid);
                zstr_free(&req);
            }
            zmsg_destroy(&msg);
        }
    }

    zpoller_destroy(&poller);
    mlm_client_destroy(&mlm);

    log_debug("%s: Ended", serverAddress);
}

#define MLM_ENDPOINT_TEST "inproc://tntmlm-test"
#define TEST_SERVER_ADDRESS "tntmlm_test_server"

TEST_CASE("tntmlm client")
{
    std::string& ref = const_cast<std::string&>(MlmClient::ENDPOINT);
    ref              = MLM_ENDPOINT_TEST;
    CHECK(MlmClient::ENDPOINT == MLM_ENDPOINT_TEST);

    zactor_t* server = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    REQUIRE(server);
    zstr_sendx(server, "BIND", MlmClient::ENDPOINT.c_str(), nullptr);

    zactor_t* tntmlm_test_actor = zactor_new(tntmlm_test_server, const_cast<char*>(TEST_SERVER_ADDRESS));
    REQUIRE(tntmlm_test_actor);
    zstr_sendx(tntmlm_test_actor, "CONNECT", MlmClient::ENDPOINT.c_str(), nullptr);

    const char* agent1Address = "AGENT1";
    mlm_client_t* agent1 = mlm_client_new();
    REQUIRE(agent1);
    mlm_client_connect(agent1, MlmClient::ENDPOINT.c_str(), 1000, agent1Address);

    printf("\n ---- MlmClient ctor ----\n");
    MlmClient* client = new MlmClient();
    REQUIRE(client);
    {
        CHECK(client->connected() == true); // auto connect in ctor
        CHECK(client->sender() == NULL);
        CHECK(client->subject() == NULL);
    }
    printf("OK\n");

    printf("\n ---- MlmClient sendto/recv ----\n");
    {
        {
          zmsg_t* reply = client->recv("uuid1", 0);
          CHECK(!reply);
          reply = client->recv("uuid1", 1);
          CHECK(!reply);
        }

        {
          zmsg_t* msg = zmsg_new();
          REQUIRE(msg);
          zmsg_addstr(msg, "uuid-1234");
          zmsg_addstr(msg, "hello");
          zmsg_addstr(msg, "world");
          int rv = client->sendto(agent1Address, "SUBJECT-HELLO", 1000, &msg);
          CHECK(rv == 0);
        }

        {
          zmsg_t* msg = mlm_client_recv(agent1);
          REQUIRE(msg);
          char* s0 = zmsg_popstr(msg);
          char* s1 = zmsg_popstr(msg);
          char* s2 = zmsg_popstr(msg);

          CHECK(mlm_client_sender(agent1) != NULL);
          CHECK(mlm_client_subject(agent1) != NULL);
          CHECK(streq(mlm_client_subject(agent1), "SUBJECT-HELLO"));
          CHECK((s0 && streq(s0, "uuid-1234")));
          CHECK((s1 && streq(s1, "hello")));
          CHECK((s2 && streq(s2, "world")));

          zstr_free(&s0);
          zstr_free(&s1);
          zstr_free(&s2);
          zmsg_destroy(&msg);
        }
    }
    printf("OK\n");

    printf("\n ---- MlmClient requestreply ----\n");
    {
        // -bad- hola request, msg consumed, no reply (timedout)
        {
            zmsg_t* msg = zmsg_new();
            REQUIRE(msg);
            zmsg_addstr(msg, "hola");
            zmsg_t* reply = client->requestreply(TEST_SERVER_ADDRESS, "hola-request", 1, &msg);
            CHECK(!msg);
            CHECK(!reply);
        }
        // -good- hello request, msg consumed, reply expected
        {
            zmsg_t* msg = zmsg_new();
            REQUIRE(msg);
            zmsg_addstr(msg, "hello");
            zmsg_t* reply = client->requestreply(TEST_SERVER_ADDRESS, "hello-request", 1, &msg);
            CHECK(!msg);
            CHECK(reply);
            char* s0 = zmsg_popstr(reply);
            CHECK((s0 && streq(s0, TEST_SERVER_HELLO_RESPONSE)));
            zstr_free(&s0);
            zmsg_destroy(&reply);
        }
    }

    printf("\n ---- MlmClient dtor ----\n");
    delete client;
    printf("OK\n");

    mlm_client_destroy(&agent1);
    zactor_destroy(&tntmlm_test_actor);
    zactor_destroy(&server);
}

TEST_CASE("tntmlm pool")
{
    std::string& ref = const_cast<std::string&>(MlmClient::ENDPOINT);
    ref              = MLM_ENDPOINT_TEST;
    CHECK(MlmClient::ENDPOINT == MLM_ENDPOINT_TEST);

    zactor_t* server = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    zstr_sendx(server, "BIND", MlmClient::ENDPOINT.c_str(), nullptr);

    zactor_t* tntmlm_test_actor = zactor_new(tntmlm_test_server, const_cast<char*>(TEST_SERVER_ADDRESS));
    REQUIRE(tntmlm_test_actor);
    zstr_sendx(tntmlm_test_actor, "CONNECT", MlmClient::ENDPOINT.c_str(), nullptr);

    mlm_client_t* agent1 = mlm_client_new();
    mlm_client_connect(agent1, MlmClient::ENDPOINT.c_str(), 1000, "AGENT1");

    mlm_client_t* agent2 = mlm_client_new();
    mlm_client_connect(agent2, MlmClient::ENDPOINT.c_str(), 1000, "AGENT2");

    { // Invariant: malamute MUST be destroyed last

        // tntmlm.h already has one pool but we can't use that one
        // for this test because valgrind will detect memory leak
        // which occurs because malamute server is destroyed only
        // after clients allocated in the pool of the header file
        const unsigned POOL_MAXSIZE = 3;
        MlmClientPool mlm_pool_test{POOL_MAXSIZE};

        CHECK(mlm_pool_test.getMaximumSize() == POOL_MAXSIZE);
        CHECK(mlm_pool_test.size() == 0);

        printf("\n ---- max pool ----\n");
        {
            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != NULL);

            MlmClientPool::Ptr ui_client2 = mlm_pool_test.get();
            CHECK(ui_client2.getPointer() != NULL);

            MlmClientPool::Ptr ui_client3 = mlm_pool_test.get();
            CHECK(ui_client3.getPointer() != NULL);

            MlmClientPool::Ptr ui_client4 = mlm_pool_test.get();
            CHECK(ui_client4.getPointer() != NULL);

            MlmClientPool::Ptr ui_client5 = mlm_pool_test.get();
            CHECK(ui_client5.getPointer() != NULL);

            printf("mlm_pool_test.getMaximumSize(): %u\n", mlm_pool_test.getMaximumSize());
            printf("mlm_pool_test.size(): %u\n", mlm_pool_test.size());
        }
        printf("OK\n");

        printf("\n ---- no reply ----\n");
        {
            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != NULL);

            zmsg_t* reply = ui_client->recv("uuid1", 1);
            CHECK(reply == NULL);

            reply = ui_client->recv("uuid1", 0);
            CHECK(reply == NULL);
        }
        printf("OK\n");

        printf("\n ---- send - correct reply ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid1");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != NULL);
            MlmClientPool::Ptr ui_client2 = mlm_pool_test.get();
            CHECK(ui_client2.getPointer() != NULL);
            int rv = ui_client2->sendto("AGENT1", "TEST", 1000, &msg);
            CHECK(rv == 0);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            CHECK(agent1_msg != NULL);
            char* agent1_msg_uuid = zmsg_popstr(agent1_msg);
            zmsg_destroy(&agent1_msg);
            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, agent1_msg_uuid);
            zstr_free(&agent1_msg_uuid);
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);
            CHECK(rv == 0);

            zmsg_t* reply = ui_client2->recv("uuid1", 1);
            CHECK(reply != NULL);
            char* tmp = zmsg_popstr(reply);
            CHECK(streq(tmp, "abc"));
            zstr_free(&tmp);
            tmp = zmsg_popstr(reply);
            CHECK(streq(tmp, "def"));
            zstr_free(&tmp);
            zmsg_destroy(&reply);
        }
        printf("OK\n");

        printf("\n ---- send - reply with bad uuid ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid1");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != nullptr);
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            CHECK(agent1_msg != nullptr);
            zmsg_destroy(&agent1_msg);
            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID");
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            mlm_client_sendto(agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);

            zmsg_t* reply = ui_client->recv("uuid1", 1);
            CHECK(reply == nullptr);

            reply = ui_client->recv("BAD-UUID", 1); // BAD-UUID message has been consumed
            CHECK(reply == nullptr);
        }
        printf("OK\n");

        printf("\n ---- send - correct reply - expect bad uuid ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid1");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != nullptr);
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            CHECK(agent1_msg != nullptr);
            zmsg_destroy(&agent1_msg);
            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "uuid1");
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            mlm_client_sendto(agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);

            zmsg_t* reply = ui_client->recv("BAD-UUID", 1);
            CHECK(reply == nullptr);
        }
        printf("OK\n");

        printf("\n ---- send - reply 3x with bad uuid first - then correct reply ----\n");
        {
            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid34");

            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != nullptr);
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            CHECK(agent1_msg != nullptr);
            zmsg_destroy(&agent1_msg);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID1");
            zmsg_addstr(agent1_msg, "abc");
            zmsg_addstr(agent1_msg, "def");
            int rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);
            CHECK(rv == 0);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID2");
            zmsg_addstr(agent1_msg, "abc");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);
            CHECK(rv == 0);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "BAD-UUID3");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);
            CHECK(rv == 0);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "uuid34");
            zmsg_addstr(agent1_msg, "OK");
            zmsg_addstr(agent1_msg, "element");
            rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);
            CHECK(rv == 0);

            zmsg_t* reply = ui_client->recv("uuid34", 2);
            CHECK(reply != nullptr);
            char* tmp = zmsg_popstr(reply);
            CHECK(streq(tmp, "OK"));
            zstr_free(&tmp);
            tmp = zmsg_popstr(reply);
            CHECK(streq(tmp, "element"));
            zstr_free(&tmp);
            zmsg_destroy(&reply);
        }
        printf("OK\n");

        printf("\n ---- Simulate thread switch ----\n");
        {
            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != NULL);

            zmsg_t* msg = zmsg_new();
            zmsg_addstr(msg, "uuid25");
            ui_client->sendto("AGENT1", "TEST", 1000, &msg);

            msg = zmsg_new();
            zmsg_addstr(msg, "uuid33");
            ui_client->sendto("AGENT2", "TEST", 1000, &msg);

            zmsg_t* agent1_msg = mlm_client_recv(agent1);
            CHECK(agent1_msg != nullptr);
            zmsg_destroy(&agent1_msg);

            agent1_msg = zmsg_new();
            zmsg_addstr(agent1_msg, "uuid25");
            zmsg_addstr(agent1_msg, "ABRAKADABRA");
            int rv = mlm_client_sendto(
                agent1, mlm_client_sender(agent1), mlm_client_subject(agent1), nullptr, 1000, &agent1_msg);
            CHECK(rv == 0);

            zmsg_t* agent2_msg = mlm_client_recv(agent2);
            CHECK(agent2_msg != nullptr);
            zmsg_destroy(&agent2_msg);

            agent2_msg = zmsg_new();
            zmsg_addstr(agent2_msg, "uuid33");
            zmsg_addstr(agent2_msg, "HABAKUKA");
            rv = mlm_client_sendto(
                agent2, mlm_client_sender(agent2), mlm_client_subject(agent2), nullptr, 1000, &agent2_msg);
            CHECK(rv == 0);

            zmsg_t* reply = ui_client->recv("uuid33", 1);
            CHECK(reply != nullptr);
            char* tmp = zmsg_popstr(reply);
            CHECK(streq(tmp, "HABAKUKA"));
            zstr_free(&tmp);
            zmsg_destroy(&reply);
        }
        printf("OK\n");

        printf("\n ---- requestreply ----\n");
        {
            MlmClientPool::Ptr ui_client = mlm_pool_test.get();
            CHECK(ui_client.getPointer() != NULL);

            // -bad- hola request, msg consumed, no reply (timedout)
            {
                zmsg_t* msg = zmsg_new();
                REQUIRE(msg);
                zmsg_addstr(msg, "hola");
                zmsg_t* reply = ui_client->requestreply(TEST_SERVER_ADDRESS, "hola-request", 1, &msg);
                CHECK(!msg);
                CHECK(!reply);
            }
            // -good- hello request, msg consumed, reply expected
            {
                zmsg_t* msg = zmsg_new();
                REQUIRE(msg);
                zmsg_addstr(msg, "hello");
                zmsg_t* reply = ui_client->requestreply(TEST_SERVER_ADDRESS, "hello-request", 1, &msg);
                CHECK(!msg);
                CHECK(reply);
                char* s0 = zmsg_popstr(reply);
                CHECK((s0 && streq(s0, TEST_SERVER_HELLO_RESPONSE)));
                zstr_free(&s0);
                zmsg_destroy(&reply);
            }
        }
        printf("OK\n");
    }

    printf("\n ---- cleanup ----\n");

    mlm_client_destroy(&agent2);
    mlm_client_destroy(&agent1);
    zactor_destroy(&tntmlm_test_actor);
    zactor_destroy(&server);

    printf("OK\n");
}
