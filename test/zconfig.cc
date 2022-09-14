/*  =========================================================================
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

#include "fty_common_mlm_zconfig.h"
#include <catch2/catch.hpp>
#include <czmq.h>

#define SELFTEST_DIR_RO "./test/conf"

TEST_CASE("zconfig")
{
    printf(" * fty_common_mlm_zconfig: \n");

    printf("zconfig #1\n");
    {
        // open a non existing file
        bool except = false;
        try {
            mlm::ZConfig(SELFTEST_DIR_RO "/do_not_exist.conf");
        } catch (const std::exception& e) {
            printf("exception: %s\n", e.what());
            except = true;
        }
        REQUIRE(except);
    }

    printf("zconfig #2\n");
    {
        // open/get/set/write an existing file
        try {
            mlm::ZConfig config(SELFTEST_DIR_RO "/test.conf");

            CHECK(config.getEntry("server/timeout") == "10000");
            CHECK(config.getEntry("server/endpoint") == "ipc://@/malamute");
            CHECK(config.getEntry("log/config") == "/etc/fty/ftylog.cfg");
            CHECK(config.getEntry("data") == "lotOfData");

            CHECK(config.getEntry("none/existing/entry") == "");
            CHECK(config.getEntry("none/existing/entry", "defaultvalue") == "defaultvalue");

            config.setEntry("data", "lotOfMoreData");
            CHECK(config.getEntry("data") == "lotOfMoreData");

            config.setEntry("none/existing/entry", "lotOfMoreData");
            CHECK(config.getEntry("none/existing/entry") == "lotOfMoreData");

            config.save(SELFTEST_DIR_RO "/test.conf2");

        } catch (const std::exception& e) {
            printf("exception: %s\n", e.what());
            CHECK(false);
        }
    }
}
