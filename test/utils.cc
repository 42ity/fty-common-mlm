/*  =========================================================================
    fty_common_mlm_utils - common malamute utils

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

#include "fty_common_mlm_utils.h"
#include <catch2/catch.hpp>
#include <fty_common_utf8.h>
#include <fty_log.h>
#include <cxxtools/jsondeserializer.h>

TEST_CASE("mlm utils")
{
    /* These tests are here because:
     * - JSON used for communication inside the system is escaped before sending via ZMQ messages
     * - they have a dependency on cxxtools (de)serializer
     * Failure of these tests may indicate a problem with fty-common library.
     */
    {
        log_debug("UTF8::escape: Test #1");
        log_debug("Validate whether the escaped string is a valid json using cxxtools::JsonDeserializer");

        // clang-format off
        std::vector <std::pair <std::string, std::string>> tests {
            {"'jednoduche ' uvozovky'",                                     "'jednoduche ' uvozovky'"},
            {"'jednoduche '' uvozovky'",                                    "'jednoduche '' uvozovky'"},
            {"dvojite \" uvozovky",                                         R"(dvojite \" uvozovky)"},
            {"dvojite \\\" uvozovky",                                       R"(dvojite \\\" uvozovky)"},
            {"dvojite \\\\\" uvozovky",                                     R"(dvojite \\\\\" uvozovky)"},
            {"dvojite \\\\\\\" uvozovky",                                   R"(dvojite \\\\\\\" uvozovky)"},
            {"dvojite \\\\\\\\\" uvozovky",                                 R"(dvojite \\\\\\\\\" uvozovky)"},
            {"'",                                                           R"(')"},
            {"\"",                                                          R"(\")"},
            {"'\"",                                                         R"('\")"},
            {"\"\"",                                                        R"(\"\")"},
            {"\"\"\"",                                                      R"(\"\"\")"},
            {"\\\"\"\"",                                                    R"(\\\"\"\")"},
            {"\"\\\"\"",                                                    R"(\"\\\"\")"},
            {"\"\"\\\"",                                                    R"(\"\"\\\")"},
            {"\\\"\\\"\\\"",                                                R"(\\\"\\\"\\\")"},
            {"\" dvojite \\\\\" uvozovky \"",                               R"(\" dvojite \\\\\" uvozovky \")"},
            {"dvojite \"\\\"\" uvozovky",                                   R"(dvojite \"\\\"\" uvozovky)"},
            {"dvojite \\\\\"\\\\\"\\\\\" uvozovky",                         R"(dvojite \\\\\"\\\\\"\\\\\" uvozovky)"},

            {"\b",                                                          R"(\\b)"},
            {"\\b",                                                         R"(\\b)"},
            {"\\\b",                                                        R"(\\\\b)"},
            {"\\\\b",                                                       R"(\\\\b)"},
            {"\\\\\b",                                                      R"(\\\\\\b)"},
            {"\\\\\\b",                                                     R"(\\\\\\b)"},
            {"\\\\\\\b",                                                    R"(\\\\\\\\b)"},
            {"\\\\\\\\b",                                                   R"(\\\\\\\\b)"},
            {"\\\\\\\\\b",                                                  R"(\\\\\\\\\\b)"},

            {"\\",                                                          R"(\\)"},
            {"\\\\",                                                        R"(\\\\)"},
            {"\\\\\\",                                                      R"(\\\\\\)"},
            {"\\\\\\\\",                                                    R"(\\\\\\\\)"},
            {"\\\\\\\\\\",                                                  R"(\\\\\\\\\\)"},
            // tests for version which does not escape UTF-8
            //{"\uA66A",                                                    "\uA66A"},
            //{"Ꙫ",                                                         "Ꙫ"},
            //{"\uA66A Ꙫ",                                                  "\uA66A Ꙫ"},

            {"\\uA66A",                                                     R"(\\uA66A)"},
            {"Ꙫ",                                                           R"(\ua66a)"},
            {"\\Ꙫ",                                                         R"(\\\ua66a)"},
            {"\u040A Њ",                                                    R"(\u040a \u040a)"},
            // do not escape control chars yet
            //{"\u0002\u0005\u0018\u001B",                                  R"(\u0002\u0005\u0018\u001b)"},

            {"\\\uA66A",                                                    R"(\\\ua66a)"},
            {"\\\\uA66A",                                                   R"(\\\\uA66A)"},
            {"\\\\\uA66A",                                                  R"(\\\\\ua66a)"},

            {"\\\\Ꙫ",                                                       R"(\\\\\ua66a)"},
            {"\\\\\\Ꙫ",                                                     R"(\\\\\\\ua66a)"},

            {"first second \n third\n\n \\n \\\\\n fourth",                 R"(first second \\n third\\n\\n \\n \\\\\\n fourth)"},
            // do not escape control chars yet
            //{"first second \n third\n\"\n \\n \\\\\"\f\\\t\\u\u0007\\\n fourth", R"(first second \\n third\\n\"\\n \\n \\\\\"\\f\\\\t\\u\u0007\\\\n fourth)"}
        };
        // clang-format on

        for (auto const& item : tests) {
            std::string json;
            std::string escaped = UTF8::escape(item.first);

            json.assign("{ \"string\" : ").append("\"").append(escaped).append("\" }");

            std::stringstream           input(json, std::ios_base::in);
            cxxtools::JsonDeserializer  deserializer(input);
            cxxtools::SerializationInfo si;
            // This is a bit crappy. We may either convert the full project
            // to C++ or otherwise keep C as pure C
            try {
                deserializer.deserialize(si);
            } catch (const std::exception& e) {
                // This will generate a failure in case of exception
                FAIL(e.what());
            }
        }
        printf("OK\n");
    }

    {
        log_debug("UTF8::escape: Test #2");
        log_debug("Construct json, read back, compare.");

        // a valid json { key : utils::json::escape (<string> } is constructed,
        // fed into cxxtools::JsonDeserializer (), read back and compared

        std::vector<std::string> tests_reading{
            {"newline in \n text \n\"\n times two"}, {"x\tx"}, {"x\\tx"}, {"x\\\tx"}, {"x\\\\tx"}, {"x\\\\\tx"},
            {"x\\\\\\tx"}, {"x\\\\\\\tx"}, {"x\\Ꙫ\uA66A\n \\nx"}, {"sdf\ndfg\n\\\n\\\\\n\b\tasd \b f\\bdfg"},
            // do not escape control chars yet
            //{"first second \n third\n\"\n \\n \\\\\"\f\\\t\\u\u0007\\\n fourth"}
        };

        for (auto const& it : tests_reading) {
            std::string json;
            json.assign("{ \"read\" : ").append("\"").append(UTF8::escape(it)).append("\" }");

            std::stringstream           input(json, std::ios_base::in);
            cxxtools::JsonDeserializer  deserializer(input);
            cxxtools::SerializationInfo si;
            // This is a bit crappy. We may either convert the full project
            // to C++ or otherwise keep C as pure C
            try {
                deserializer.deserialize(si);
            } catch (std::exception& e) {
                // This will generate a failure in case of exception
                FAIL(e.what());
            }

            std::string read;
            si.getMember("read") >>= read;
            /*  TODO: feel free to fix this
                        CAPTURE (read);
                        CAPTURE (it);
                        assert ( read.compare (it) == 0 );
            */
        }
        printf("OK\n");
    }

    printf("fty-common-mlm-utils OK\n");
}

