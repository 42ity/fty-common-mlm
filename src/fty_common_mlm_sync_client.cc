/*  =========================================================================
    fty_common_mlm_sync_client - Simple malamute client for synchronous request

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
    fty_common_mlm_sync_client - Simple malamute client for synchronous request
@discuss
@end
*/

#include "fty_common_mlm_sync_client.h"
#include <czmq.h>
#include <fty_common_mlm.h>
#include <gnu/libc-version.h>
#include <iomanip>
#include <malamute.h>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

namespace mlm {
MlmSyncClient::MlmSyncClient(
    const std::string& clientId, const std::string& destination, uint32_t timeout, const std::string& endPoint)
    : m_clientId(clientId)
    , m_destination(destination)
    , m_timeout(timeout)
    , m_endpoint(endPoint)
{
}


std::vector<std::string> MlmSyncClient::syncRequestWithReply(const std::vector<std::string>& payload)
{
    mlm_client_t* client = mlm_client_new();

    if (client == nullptr) {
        mlm_client_destroy(&client);
        throw std::runtime_error("Malamute error: NULL client pointer");
    }

    // create a unique sender id: <clientId>.[thread id in hexa]
    pid_t threadId = gettid();

    std::stringstream ss;
    ss << m_clientId << "." << std::setfill('0') << std::setw(sizeof(pid_t) * 2) << std::hex << threadId;

    std::string uniqueId = ss.str();

    int rc = mlm_client_connect(client, m_endpoint.c_str(), m_timeout, uniqueId.c_str());

    if (rc != 0) {
        mlm_client_destroy(&client);
        throw std::runtime_error("Malamute error: Error connecting to endpoint <" + m_endpoint + ">");
    }

    // Prepare the request:
    zmsg_t*    request = zmsg_new();
    ZuuidGuard zuuid(zuuid_new());
    zmsg_addstr(request, zuuid_str_canonical(zuuid));


    // add all the payload
    for (const std::string& frame : payload) {
        zmsg_addstr(request, frame.c_str());
    }

    if (zsys_interrupted) {
        zmsg_destroy(&request);
        mlm_client_destroy(&client);
        throw std::runtime_error("Malamute error: zsys_interrupted");
    }

    // send the message
    mlm_client_sendto(client, m_destination.c_str(), "REQUEST", nullptr, m_timeout, &request);

    if (zsys_interrupted) {
        zmsg_destroy(&request);
        mlm_client_destroy(&client);
        throw std::runtime_error("Malamute error: zsys_interrupted");
    }

    // Get the reply
    ZmsgGuard recv(mlm_client_recv(client));
    mlm_client_destroy(&client);

    // Get number of frame all the frame
    size_t numberOfFrame = zmsg_size(recv);

    if (numberOfFrame == 0) {
        throw std::runtime_error("Malamute error: No correlation id");
    }

    // Check the message
    ZstrGuard str(zmsg_popstr(recv));
    if (!streq(str, zuuid_str_canonical(zuuid))) {
        throw std::runtime_error("Malamute error: Mismatch correlation id");
    }

    std::vector<std::string> receivedFrames;

    // we unstack all the other frame starting by the 1st one.
    for (size_t index = 1; index < numberOfFrame; index++) {
        ZstrGuard frame(zmsg_popstr(recv));
        receivedFrames.push_back(std::string(frame.get()));
    }

    return receivedFrames;
}

} // namespace mlm
