/*  =========================================================================
    fty_common_common_fty_uuid - Calculates UUID for assets.

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

#include "fty_common_mlm_uuid.h"
#include <catch2/catch.hpp>
#include <czmq.h>

TEST_CASE("uuid")
{
    printf(" * fty_uuid: ");

    //  @selftest
    //  Simple create/destroy test

    fty_uuid_t* self = fty_uuid_new();
    CHECK(self);
    const char* id = fty_uuid_calculate(self, nullptr, nullptr, nullptr);
    CHECK(streq(id, "ffffffff-ffff-ffff-ffff-ffffffffffff"));
    id = fty_uuid_calculate(self, "EATON", "IPC3000", "LA71042052");
    CHECK(streq(id, "c7cc6ffe-f36f-55ee-b5d4-3e40a2fe08a3"));
    fty_uuid_destroy(&self);

    fty_uuid_t* fuuid = fty_uuid_new();
    zhash_t*    ext   = zhash_new();
    zhash_insert(ext, "model", const_cast<char*>("M1"));
    const char* uuid = fty_uuid_create(ext, "room", fuuid);
    CHECK(uuid);

    zhash_destroy(&ext);
    fty_uuid_destroy(&fuuid);
    //  @end
    printf("OK\n");
}
