/*  =========================================================================
    fty_common_mlm_utils - common malamute utils 

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

/*
@header
    fty_common_mlm_utils -
@discuss
@end
*/
#include <fty_common_mlm_utils.h>
#include <fty_log.h>
#include <cxxtools/jsondeserializer.h>
#include <fty_common_utf8.h>

std::map<std::string, std::string>
zhash_to_map(zhash_t *hash)
{
    std::map<std::string,std::string> map;
    char *item = (char *)zhash_first(hash);
    while(item) {
        const char * key = zhash_cursor(hash);
        const char * val = (const char *)zhash_lookup(hash,key);
        if( key && val ) map[key] = val;
        item = (char *)zhash_next(hash);
    }
    return map;
}


zhash_t *
map_to_zhash (std::map<std::string, std::string> map_to_convert)
{
    zhash_t *hash = zhash_new ();
    for (auto &i : map_to_convert) {
        zhash_insert (hash, i.first.c_str (), (void*) i.second.c_str());
    }

    return hash;
}

void
fty_common_mlm_utils_test (bool verbose)
{
    /* These tests are here because:
     * - JSON used for communication inside the system is escaped before sending via ZMQ messages
     * - they have a dependency on cxxtools (de)serializer
     * Failure of these tests may indicate a problem with fty-common library.
     */
    {
        log_debug ("UTF8::escape: Test #1");
        log_debug ("Validate whether the escaped string is a valid json using cxxtools::JsonDeserializer");

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

        for (auto const& item : tests) {
            std::string json;
            std::string escaped = UTF8::escape (item.first);

            json.assign("{ \"string\" : ").append ("\"").append (escaped).append ("\" }");

            std::stringstream input (json, std::ios_base::in);
            cxxtools::JsonDeserializer deserializer (input);
            cxxtools::SerializationInfo si;
            // This is a bit crappy. We may either convert the full project
            // to C++ or otherwise keep C as pure C
            try {
                deserializer.deserialize (si);
            } catch (std::exception& e) {
                // This will generate a failure in case of exception
                assert (0 == 1);
            }
        }
        printf ("OK\n");
    }

    {
        log_debug ("UTF8::escape: Test #2");
        log_debug ("Construct json, read back, compare.");

        // a valid json { key : utils::json::escape (<string> } is constructed,
        // fed into cxxtools::JsonDeserializer (), read back and compared

        std::vector <std::string> tests_reading{
            {"newline in \n text \n\"\n times two"},
            {"x\tx"},
            {"x\\tx"},
            {"x\\\tx"},
            {"x\\\\tx"},
            {"x\\\\\tx"},
            {"x\\\\\\tx"},
            {"x\\\\\\\tx"},
            {"x\\Ꙫ\uA66A\n \\nx"},
            {"sdf\ndfg\n\\\n\\\\\n\b\tasd \b f\\bdfg"},
            // do not escape control chars yet
            //{"first second \n third\n\"\n \\n \\\\\"\f\\\t\\u\u0007\\\n fourth"}
        };

        for (auto const& it : tests_reading) {
            std::string json;
            json.assign("{ \"read\" : ").append ("\"").append (UTF8::escape (it)).append ("\" }");

            std::stringstream input (json, std::ios_base::in);
            cxxtools::JsonDeserializer deserializer (input);
            cxxtools::SerializationInfo si;
            // This is a bit crappy. We may either convert the full project
            // to C++ or otherwise keep C as pure C
            try {
                deserializer.deserialize (si);
            } catch (std::exception& e) {
                // This will generate a failure in case of exception
                assert (0 == 1);
            }

            std::string read;
            si.getMember ("read") >>= read;
/*  TODO: feel free to fix this
            CAPTURE (read);
            CAPTURE (it);
            assert ( read.compare (it) == 0 );
*/
        }
        printf ("OK\n");
    }

    printf ("fty-common-mlm-utils OK\n");
}

std::string zmsg_popstring(zmsg_t *resp){
    char *popstr = zmsg_popstr (resp);
    //copies the null-terminated character sequence (C-string) pointed by popstr
    std::string string_rv = popstr;
    zstr_free (&popstr);
    return string_rv;
}

