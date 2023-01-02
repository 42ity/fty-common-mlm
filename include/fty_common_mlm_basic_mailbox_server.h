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

#pragma once

#include "fty_common_mlm_agent.h"
#include <fty_common_sync_server.h>
#include <string>

namespace mlm {

/**
 * \brief Handler for basic mailbox server using object
 *        implementing fty::SyncServer interface.
 *
 * The server receive request using mailbox with the following protocol:
 *  - Requests have as subject "REQUEST" and replies "REPLY"
 *  - The first frame in request or reply is a correlation Id
 *  - The following frames are a payload frames
 *
 * \see fty_common_mlm_sync_client.h
 */

class MlmBasicMailboxServer : public mlm::MlmAgent
{
public:
    explicit MlmBasicMailboxServer(zsock_t* pipe, fty::SyncServer& server, const std::string& name,
        const std::string& endpoint = "ipc://@/malamute");

private:
    bool handleMailbox(zmsg_t* message) override;

private:
    // members
    fty::SyncServer& m_server;
    std::string      m_name;
    std::string      m_endpoint;
};

} // namespace mlm
