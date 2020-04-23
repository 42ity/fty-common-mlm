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

#ifndef FTY_COMMON_MLM_STREAM_CLIENT_H_INCLUDED
#define FTY_COMMON_MLM_STREAM_CLIENT_H_INCLUDED

#include "fty_common_client.h"

#include <string>
#include <vector>
#include <functional>

#include <thread>
#include <exception>
#include <stdexcept>
#include <mutex>
#include <functional>
#include <condition_variable>

namespace mlm
{
    using Callback = std::function<void(const std::vector<std::string> &)>;
  
    class MlmStreamClient :
        public fty::StreamSubscriber, //Implement interface for listening on stream
        public fty::StreamPublisher //Implement interface for publishing on stream
            
    {    
    public:
        explicit MlmStreamClient( const std::string & clientId,
                                  const std::string & stream,
                                  uint32_t timeout = 1000,
                                  const std::string & endPoint = "ipc://@/malamute");
        
        
        ~MlmStreamClient();
        
        //method for publishing
        void publish(const std::vector<std::string> & payload) override;
        
        //methods for subcribing
        uint32_t subscribe(Callback callback) override;
        void unsubscribe(uint32_t subId) override;
        
    private:
        //Common attributs
        std::string m_clientId;
        std::string m_stream;
        uint32_t m_timeout;
        std::string m_endpoint;
        
        //Specific to StreamSubscriber
        std::thread m_listenerThread;
        
        std::mutex m_listenerCallbackMutex;
        std::condition_variable m_listenerStarted;
        std::exception_ptr m_exPtr = nullptr;
        bool m_stopRequested = false;
        
        uint32_t m_counter = 0;
        std::map<uint32_t, Callback> m_callbacks;

        
        //Private methods
        void publishOnBus(const std::string & type, const std::vector<std::string> & payload);
        void listener(); //function use by the thread to listen on the bus
        
    };
    
} //namespace mlm

//  Self test of this class
void
    fty_common_mlm_stream_client_test (bool verbose);

#endif
