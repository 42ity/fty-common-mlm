/*  =========================================================================
    fty_common_mlm_sync_client - Simple malamute client for syncronous request

    Copyright (C) 2014 - 2019 Eaton

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

#ifndef FTY_COMMON_MLM_SYNC_CLIENT_H_INCLUDED
#define FTY_COMMON_MLM_SYNC_CLIENT_H_INCLUDED

#include "fty_common_client.h"

#include <string>
#include <vector>
#include <functional>


namespace mlm
{
    // This class is thread safe.
    
    class MlmSyncClient
        : public fty::SyncClient //Implement interface for synchronous client
    {    
    public:
        explicit MlmSyncClient(   const std::string & clientId,
                                  const std::string & destination,
                                  uint32_t timeout = 1000,
                                  const std::string & endPoint = "ipc://@/malamute");
        
        //methods
        std::vector<std::string> syncRequestWithReply(const std::vector<std::string> & payload) override;
        
    private:
        //attributs
        std::string m_clientId;
        std::string m_destination;
        uint32_t m_timeout;
        std::string m_endpoint;           
    };
    
} //namespace mlm

//  Self test of this class
FTY_COMMON_MLM_EXPORT void
    fty_common_mlm_sync_client_test (bool verbose);

#endif
