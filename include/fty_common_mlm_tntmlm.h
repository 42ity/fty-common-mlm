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

#pragma once

#include <czmq.h>
#include <malamute.h>
#include <string>
#include <vector>
#include <memory>

class MlmClient
{
public:
    static const std::string ENDPOINT;

    MlmClient(const std::string& addressPrefix = "rest.");
    virtual ~MlmClient();

    // timeout <0, 300> seconds, greater number trimmed
    // based on specified uuid returns expected message or NULL on expire/interrupt
    virtual zmsg_t* recv(const std::string& uuid, uint32_t timeout_s);

    // implements request reply pattern
    // method prepends two frames
    zmsg_t* requestreply(const std::string& address, const std::string& subject, uint32_t timeout_s, zmsg_t** content_p);
    virtual int sendto(const std::string& address, const std::string& subject, uint32_t timeout_s, zmsg_t** content_p);

    bool connected()
    {
        return _client ? mlm_client_connected(_client) : false;
    }
    const char* subject()
    {
        return _client ? mlm_client_subject(_client) : NULL;
    }
    const char* sender()
    {
        return _client ? mlm_client_sender(_client) : NULL;
    }

private:
    bool connect();

private:
    mlm_client_t* _client{nullptr};
    zuuid_t*      _uuid{nullptr};
    zpoller_t*    _poller{nullptr};
    std::string   _clientAddressPrefix;
};
