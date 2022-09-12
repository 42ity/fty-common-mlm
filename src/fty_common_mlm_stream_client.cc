/*  =========================================================================
    fty_common_mlm_stream_client - Simple malamute client for stream

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
    fty_common_mlm_stream_client - Simple malamute client for stream
@discuss
@end
*/

#include "fty_common_mlm_stream_client.h"
#include <gnu/libc-version.h>
#include <sys/types.h>
#include <unistd.h>

// gettid() is available since glibc 2.30
#if ((__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 30))
#include <sys/syscall.h>
#define gettid() pid_t(syscall(SYS_gettid))
#endif

#include <czmq.h>
#include <fty_common_mlm.h>
#include <iomanip>
#include <malamute.h>
#include <sstream>

namespace mlm {
MlmStreamClient::MlmStreamClient(
    const std::string& clientId, const std::string& stream, uint32_t timeout, const std::string& endPoint)
    : m_clientId(clientId)
    , m_stream(stream)
    , m_timeout(timeout)
    , m_endpoint(endPoint)
{
    (void)m_timeout;
}

MlmStreamClient::~MlmStreamClient()
{
    // stop the thread
    if (!m_callbacks.empty()) {
        m_stopRequested = true;
        publishOnBus("SYNC", {});
        m_listenerThread.join();
    }
}

void MlmStreamClient::publish(const std::vector<std::string>& payload)
{
    publishOnBus("MESSAGE", payload);
}

void MlmStreamClient::publishOnBus(const std::string& type, const std::vector<std::string>& payload)
{
    if (zsys_interrupted) {
        return;
    }

    // std::cerr << "Publish on Bus <" << messageType << ">:" << payload << std::endl;

    mlm_client_t* client = mlm_client_new();

    if (client == nullptr) {
        mlm_client_destroy(&client);
        throw std::runtime_error("Malamute error: NULL client pointer");
    }

    // create a unique sender id: <m_cliendId>.PUB.<m_stream>[thread id in hexa]
    pid_t threadId = gettid();

    std::stringstream ss;
    ss << m_clientId << ".PUB." << m_stream << "." << std::setfill('0') << std::setw(sizeof(pid_t) * 2) << std::hex
       << threadId;

    std::string uniqueId = ss.str();

    int rc = mlm_client_connect(client, m_endpoint.c_str(), 1000, uniqueId.c_str());

    if (rc != 0) {
        mlm_client_destroy(&client);
        throw std::runtime_error("Malamute error: Error connecting to endpoint <" + m_endpoint + ">");
    }

    rc = mlm_client_set_producer(client, m_stream.c_str());
    if (rc != 0) {
        mlm_client_destroy(&client);
        throw std::runtime_error("Malamute error: Impossible to become publisher of stream <" + m_stream + ">");
    }

    zmsg_t* notification = zmsg_new();
    for (const std::string& frame : payload) {
        zmsg_addstr(notification, frame.c_str());
    }

    rc = mlm_client_send(client, type.c_str(), &notification);

    zmsg_destroy(&notification);
    mlm_client_destroy(&client);

    if (rc != 0) {
        throw std::runtime_error("Malamute error: Impossible to publish on stream <" + m_stream + ">");
    }
}

uint32_t MlmStreamClient::subscribe(Callback callback)
{
    m_stopRequested = false;
    m_counter++;

    std::unique_lock<std::mutex> lock(m_listenerCallbackMutex);
    m_callbacks[m_counter] = callback;

    // There is no subscriber - we create one
    if (!m_listenerThread.joinable()) {
        // start
        m_exPtr          = nullptr;
        m_listenerThread = std::thread(std::bind(&MlmStreamClient::listener, this));

        m_listenerStarted.wait(lock);

        // check that startup worked properly
        if (m_exPtr) {
            std::rethrow_exception(m_exPtr);
        }
    }

    return m_counter;
}

void MlmStreamClient::unsubscribe(uint32_t subId)
{
    if (m_callbacks.count(subId) > 0) {
        { // lock only to remove from the thread
            std::unique_lock<std::mutex> lock(m_listenerCallbackMutex);
            m_callbacks.erase(subId);
        }

        if (m_callbacks.empty()) {
            m_stopRequested = true;
            publishOnBus("SYNC", {});
            m_listenerThread.join();
        }
    }
}

void MlmStreamClient::listener()
{
    mlm_client_t* client = NULL;
    zpoller_t* poller = NULL;

    try {
        client = mlm_client_new();
        if (client == nullptr) {
            throw std::runtime_error("Malamute error: NULL client pointer");
        }

        // create a unique id: <m_clientId>-SECW_NOTIFICATIONS.[thread id in hexa]
        pid_t threadId = gettid();

        std::stringstream ss;
        ss << m_clientId << ".SUB." << m_stream << "." << std::setfill('0') << std::setw(sizeof(pid_t) * 2) << std::hex
           << threadId;

        std::string uniqueId = ss.str();

        int rc = mlm_client_connect(client, m_endpoint.c_str(), 1000, uniqueId.c_str());

        if (rc != 0) {
            throw std::runtime_error("Malamute error: Error connecting to endpoint <" + m_endpoint + ">");
        }

        rc = mlm_client_set_consumer(client, m_stream.c_str(), ".*");
        if (rc != 0) {
            throw std::runtime_error("Malamute error: Impossible to become consumer of stream <" + m_stream + ">");
        }

        poller = zpoller_new(mlm_client_msgpipe(client), NULL);
        if (!poller) {
            throw std::runtime_error("Malamute error: Error creating poller");
        }

        m_listenerStarted.notify_all();

        const int poller_timeout = 1000; //ms

        while (!zsys_interrupted) {
            void* which = zpoller_wait(poller, poller_timeout);

            if (which == nullptr) {
                if (zpoller_terminated(poller) || zsys_interrupted) {
                    break;
                }
            }

            // check if we need to leave the loop
            if (m_stopRequested) {
                break;
            }

            if (which == mlm_client_msgpipe(client)) {
                ZmsgGuard msg(mlm_client_recv(client));

                std::vector<std::string> payload;
                while(true) {
                    ZstrGuard frame(zmsg_popstr(msg));
                    if (!frame) {
                        break;
                    }
                    payload.push_back(std::string{frame});
                }

                // process the callbacks
                std::unique_lock<std::mutex> lock(m_listenerCallbackMutex);
                for (const auto& item : m_callbacks) {
                    try {
                        item.second(payload);
                    } catch (...) // Show Must Go On => Log errors and continue
                    {
                        // log_error("Error during processing callback [%i]: unknown error", item.first);
                    }
                }
            }
        }
    }
    catch (...) // Transfer the error to the main thread (only at startup)
    {
        // log_error("Error during starting the listener thread");
        m_exPtr = std::current_exception();
        m_listenerStarted.notify_all();
    }

    zpoller_destroy(&poller);
    mlm_client_destroy(&client);
}

} // namespace mlm
