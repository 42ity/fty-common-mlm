/*  =========================================================================
    fty-common-mlm - Provides common Malamute and ZeroMQ tools for agents

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

typedef struct _fty_common_mlm_subprocess_t fty_common_mlm_subprocess_t;
#define FTY_COMMON_MLM_SUBPROCESS_T_DEFINED
typedef struct _fty_common_mlm_agent_t fty_common_mlm_agent_t;
#define FTY_COMMON_MLM_AGENT_T_DEFINED
typedef struct _fty_common_mlm_uuid_t fty_common_mlm_uuid_t;
#define FTY_COMMON_MLM_UUID_T_DEFINED
typedef struct _fty_common_mlm_tntmlm_t fty_common_mlm_tntmlm_t;
#define FTY_COMMON_MLM_TNTMLM_T_DEFINED
typedef struct _fty_common_mlm_utils_t fty_common_mlm_utils_t;
#define FTY_COMMON_MLM_UTILS_T_DEFINED
typedef struct _fty_common_mlm_zconfig_t fty_common_mlm_zconfig_t;
#define FTY_COMMON_MLM_ZCONFIG_T_DEFINED
typedef struct _fty_common_mlm_sync_client_t fty_common_mlm_sync_client_t;
#define FTY_COMMON_MLM_SYNC_CLIENT_T_DEFINED
typedef struct _fty_common_mlm_stream_client_t fty_common_mlm_stream_client_t;
#define FTY_COMMON_MLM_STREAM_CLIENT_T_DEFINED
typedef struct _fty_common_mlm_basic_mailbox_server_t fty_common_mlm_basic_mailbox_server_t;
#define FTY_COMMON_MLM_BASIC_MAILBOX_SERVER_T_DEFINED


//  Public classes, each with its own header file
#include "fty_common_mlm_agent.h"
#include "fty_common_mlm_basic_mailbox_server.h"
#include "fty_common_mlm_guards.h"
#include "fty_common_mlm_stream_client.h"
#include "fty_common_mlm_sync_client.h"
#include "fty_common_mlm_tntmlm.h"
#include "fty_common_mlm_utils.h"
#include "fty_common_mlm_uuid.h"
#include "fty_common_mlm_zconfig.h"
