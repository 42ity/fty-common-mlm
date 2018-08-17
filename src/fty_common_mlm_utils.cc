/*  =========================================================================
    fty_common_mlm_utils - class description

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

/*
@header
    fty_common_mlm_utils -
@discuss
@end
*/
#include <fty_common_mlm_utils.h>

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
