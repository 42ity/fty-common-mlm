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

#include "fty_common_mlm_classes.h"

namespace mlm
{

    using namespace fty;

    using Subject = std::string;

    MlmBasicMailboxServer::MlmBasicMailboxServer(zsock_t *pipe,
                                                 fty::SyncServer & server,
                                                 const std::string & name,
                                                 const std::string & endpoint)
        :   mlm::MlmAgent(pipe),
            m_server(server),
            m_name(name),
            m_endpoint(endpoint)
    {
        connect(m_endpoint.c_str(), m_name.c_str());
    }

    bool MlmBasicMailboxServer::handleMailbox(zmsg_t *message)
    {
        std::string correlationId;

        //try to address the request
        try
        {
            Subject subject(mlm_client_subject(client()));
            Sender uniqueSender(mlm_client_sender(client()));

            //ignore none "REQUEST" message
            if (subject != "REQUEST")
            {
                log_warning ("<%s> Received mailbox message with subject '%s' from '%s', ignoring", m_name.c_str(), subject.c_str(),uniqueSender.c_str());
                return true;
            }

            //Get number of frame all the frame
            size_t numberOfFrame = zmsg_size(message);

            log_debug("<%s> Received mailbox message with subject '%s' from '%s' with %i frames", m_name.c_str(), subject.c_str(), uniqueSender.c_str(), numberOfFrame);


            /*  Message is valid if the header contain at least the following frame:
             * 0. Correlation id
             * 1. data
             */

            //TODO define a maximum to avoid DOS

            ZstrGuard ptrCorrelationId( zmsg_popstr(message) );

            Payload payload;

            //we unstack all the other starting by the 3rd one.
            for(size_t index = 1; index < numberOfFrame; index++)
            {
                ZstrGuard param( zmsg_popstr(message) );
                payload.push_back( std::string(param.get()) );
            }

            //Ensure the presence of data from the request
            if(ptrCorrelationId != nullptr)
            {
                correlationId = std::string(ptrCorrelationId.get());
            }

            if(correlationId.empty())
            {
                //no correlation id, it's a bad frame we ignore it
                throw std::runtime_error("<"+m_name+"> Correlation id frame is empty");
            }

            //extract the sender from unique sender id: <Sender>.[thread id in hexa]
            Sender sender = uniqueSender.substr(0, (uniqueSender.size()-(sizeof(pid_t)*2)-1));

            //Execute the request
            Payload results = m_server.handleRequest(sender, payload);

            //send the result if it's not empty
            if(!results.empty())
            {
                zmsg_t *reply = zmsg_new();

                zmsg_addstr (reply, correlationId.c_str());

                for(const std::string & result : results)
                {
                    zmsg_addstr (reply, result.c_str());
                }

                int rv = mlm_client_sendto (client(), mlm_client_sender (client()),
                                    "REPLY", NULL, 1000, &reply);
                if (rv != 0)
                {
                    log_error ("<%s> s_handle_mailbox: failed to send reply to %s ",
                            m_name.c_str(), mlm_client_sender (client()));
                }
            }

        }
        catch (std::exception &e)
        {
            log_error("<%s> Unexpected error: %s", m_name.c_str(), e.what());
        }
        catch (...) //show must go one => Log and ignore the unknown error
        {
            log_error("<%s> Unexpected error: unknown", m_name.c_str());
        }

        return true;
    }

} //namespace mlm

//  --------------------------------------------------------------------------
//  Self test of this class

// If your selftest reads SCMed fixture data, please keep it in
// src/selftest-ro; if your test creates filesystem objects, please
// do so under src/selftest-rw.
// The following pattern is suggested for C selftest code:
//    char *filename = NULL;
//    filename = zsys_sprintf ("%s/%s", SELFTEST_DIR_RO, "mytemplate.file");
//    assert (filename);
//    ... use the "filename" for I/O ...
//    zstr_free (&filename);
// This way the same "filename" variable can be reused for many subtests.
#define SELFTEST_DIR_RO "src/selftest-ro"
#define SELFTEST_DIR_RW "src/selftest-rw"

#include "fty_common_unit_tests.h"
#include "fty_common_mlm_sync_client.h"

static const char* testEndpoint = "inproc://fty_common_mlm_basic_mailbox_server_test";
static const char* testAgentName = "fty_common_mlm_basic_mailbox_server_test";

void fty_common_mlm_basic_mailbox_server_test_actor(zsock_t *pipe, void * /*args*/)
{
    
    fty::EchoServer server;
        
    mlm::MlmBasicMailboxServer agent(  pipe, 
                                        server,
                                        testAgentName,
                                        testEndpoint
                                    );
    agent.mainloop();
}

void
fty_common_mlm_basic_mailbox_server_test (bool verbose)
{
    printf (" * fty_common_mlm_basic_mailbox_server: ");

    zactor_t *broker = zactor_new (mlm_server, (void*) "Malamute");
    zstr_sendx (broker, "BIND", testEndpoint, NULL);
    if (verbose)
        zstr_send (broker, "VERBOSE");
    
    //create the server
    zactor_t *server = zactor_new (fty_common_mlm_basic_mailbox_server_test_actor, (void *)(NULL));
    
    //create a client
    {
        mlm::MlmSyncClient syncClient( "test_client",
                                       testAgentName,
                                       1000,
                                       testEndpoint);

        fty::Payload expectedPayload = {"This", "is", "a", "test"};

        fty::Payload receivedPayload = syncClient.syncRequestWithReply(expectedPayload);

        assert (expectedPayload == receivedPayload);
    }

    zstr_sendm (server, "$TERM");
    sleep(1);

    zactor_destroy (&server);
    zactor_destroy (&broker);
    
    printf ("Ok\n");
}
