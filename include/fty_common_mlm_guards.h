/*  =========================================================================
    fty_common_mlm_guards - C++ wrapper for czmq / malamute objects with automatic cleanup

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

#ifndef FTY_COMMON_MLM_GUARDS_H_INCLUDED
#define FTY_COMMON_MLM_GUARDS_H_INCLUDED

#include <malamute.h>
#include <czmq.h>

template<typename T, void (*destructor)(T**)>
class MlmObjGuard
{
public:
    MlmObjGuard()
        : ptr_(nullptr)
    {}
    explicit MlmObjGuard(T* ptr)
        : ptr_(ptr)
    {}
    MlmObjGuard(const MlmObjGuard&) = delete;
    ~MlmObjGuard()
    {
        destructor(&ptr_);
    }
    T* operator=(T* ptr)
    {
        destructor(&ptr_);
        ptr_ = ptr;
        return ptr_;
    }
    operator T*()
    {
        return ptr_;
    }
    T* get()
    {
        return ptr_;
    }
private:
    T* ptr_;
};

typedef MlmObjGuard<mlm_client_t, mlm_client_destroy> MlmClientGuard;
typedef MlmObjGuard<zpoller_t, zpoller_destroy> ZpollerGuard;
typedef MlmObjGuard<zmsg_t, zmsg_destroy> ZmsgGuard;
typedef MlmObjGuard<zuuid_t, zuuid_destroy> ZuuidGuard;
typedef MlmObjGuard<char, zstr_free> ZstrGuard;
typedef MlmObjGuard<zconfig_t, zconfig_destroy> ZconfigGuard;

#endif
