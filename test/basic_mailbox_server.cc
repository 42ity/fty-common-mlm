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

#include "fty_common_mlm_basic_mailbox_server.h"
#include "fty_common_mlm_guards.h"
#include "fty_common_mlm_sync_client.h"
#include <catch2/catch.hpp>
#include <fty_log.h>
#include <fty_common_unit_tests.h>

static const char* testEndpoint  = "inproc://fty_common_mlm_basic_mailbox_server_test";
static const char* testAgentName = "fty_common_mlm_basic_mailbox_server_test";

static void fty_common_mlm_basic_mailbox_server_test_actor(zsock_t* pipe, void* /*args*/)
{

    fty::EchoServer server;

    mlm::MlmBasicMailboxServer agent(pipe, server, testAgentName, testEndpoint);
    agent.mainloop();
}

TEST_CASE("Basic mailbox server")
{
    printf(" * fty_common_mlm_basic_mailbox_server: ");

    zactor_t* broker = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    zstr_sendx(broker, "BIND", testEndpoint, NULL);
    zstr_send(broker, "VERBOSE");

    // create the server
    zactor_t* server = zactor_new(fty_common_mlm_basic_mailbox_server_test_actor, nullptr);

    // create a client
    {
        mlm::MlmSyncClient syncClient("test_client", testAgentName, 1000, testEndpoint);

        fty::Payload expectedPayload = {"This", "is", "a", "test"};

        fty::Payload receivedPayload = syncClient.syncRequestWithReply(expectedPayload);

        CHECK(expectedPayload == receivedPayload);
    }

    zstr_sendm(server, "$TERM");
    sleep(1);

    zactor_destroy(&server);
    zactor_destroy(&broker);

    printf("Ok\n");
}

TEST_CASE("Basic mailbox server with timeout")
{
    printf(" * fty_common_mlm_basic_mailbox_server: ");

    zactor_t* broker = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    zstr_sendx(broker, "BIND", testEndpoint, NULL);
    zstr_send(broker, "VERBOSE");

    // create a client
    {
        mlm::MlmSyncClient syncClient("test_client", testAgentName, 1000, testEndpoint);

        fty::Payload expectedPayload = {"This", "is", "a", "test"};

        bool isExeption = false;
        try {
            // send request without server for response (timeout of 10 sec by default)
            fty::Payload receivedPayload = syncClient.syncRequestWithReply(expectedPayload);
        }
        catch (std::exception& e) {
           isExeption = true;
        }
        CHECK(isExeption);
    }
    zactor_destroy(&broker);

    printf("Ok\n");
}
