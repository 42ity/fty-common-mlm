/*  =========================================================================
    fty_common_mlm_client - C++ class to help with malamute

    Copyright (C) 2014 - 2018 Eaton

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

#ifndef FTY_COMMON_MLM_CLIENT_H_INCLUDED
#define FTY_COMMON_MLM_CLIENT_H_INCLUDED

#include <exception>

namespace mlm
{
    /**
     * \brief Helper class to structure the writing of a mlm_client. 
     *
     * This class takes care of the basic lifecycle of a mlm_client agent (connecting
     * to malamute, dispatching zmsg_t messages, periodic callbacks...). Users are
     * expected to subclass this class and fill in the agent logic.
     */
    class MlmClient
    {
    public:
        /**
         * \brief Destructor.
         *
         * This will destroy the mlm_client_t object.
         */
        virtual ~MlmClient();

        /**
         * \brief Mainloop of the client.
         *
         * This method handles the infinite loop of reception messages from the
         * agent's pipe and malamute connection and forwards them to the appropriate
         * handle*() callbacks.
         */
        void mainloop();

    protected:
        /**
         * \brief Constructor.
         *
         * This will create and connect the mlm_client_t object. The agent must
         * handle the configuration of the consumption and production parts of the
         * connection.
         *
         * \param pipe Pipe of the zactor_t
         * \param endpoint Endpoint to connect to
         * \param address Name of the established connection
         * \param pollerTimeout Timeout of poller and rough interval betwenn tick() invocations (-1 to disable)
         * \param connectionTimeout Timeout for connection attempt
         */
        MlmClient(zsock_t *pipe, const char *endpoint = NULL, const char *address = NULL, int pollerTimeout = -1, int connectionTimeout = 5000);
        /**
         * \brief connect to broker malamute
         * \return false to stop the agent, true otherwise.
         * \param endpoint Endpoint to connect to
         * \param address Name of the established connection
         * \param connectionTimeout Timeout for connection attempt
         */
        virtual void connect(const char *endpoint, const char *address, int connectionTimeout = 5000);
        /**
         * \brief Periodic callback, if enabled.
         * \return false to stop the agent, true otherwise.
         */
        virtual bool tick() { return true; }

        /**
         * \brief Callback for stream messages. The callback DOESN'T take ownership of the message.
         * \return false to stop the agent, true otherwise.
         */
        virtual bool handleStream(zmsg_t *message) { return true; }

        /**
         * \brief Callback for pipe messages. The callback DOESN'T take ownership of the message.
         * \return false to stop the agent, true otherwise.
         */
        virtual bool handlePipe(zmsg_t *message);

        /**
         * \brief Callback for mailbox messages. The callback DOESN'T take ownership of the message.
         * \return false to stop the agent, true otherwise.
         */
        virtual bool handleMailbox(zmsg_t *message) { return true; }

        /**
         * \brief Callback for "other" messages. The callback DOESN'T take ownership of the message.
         * \return false to stop the agent, true otherwise.
         */
        virtual bool handleOther(zmsg_t *message, void *which) { return true; }

        /**
         * \brief Create/get the zpoller_t* object for the agent mainloop.
         * \return The mainloop poller object.
         */
        virtual zpoller_t* zpoller();

        mlm_client_t *m_client;
        zsock_t *m_pipe;
        int64_t m_lastTick;
        int m_pollerTimeout;
        int m_connectionTimeout;

    private:
        zpoller_t *m_defaultZpoller;
    };
    
} // namespace mlm

#endif
