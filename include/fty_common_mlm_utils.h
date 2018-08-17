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

#ifndef FTY_COMMON_MLM_UTILS_H_INCLUDED
#define FTY_COMMON_MLM_UTILS_H_INCLUDED

#ifdef __cplusplus
#include <map>
#include <czmq.h>

namespace MlmUtils {

    // convert zhash_t to std::map
    std::map<std::string, std::string>
    zhash_to_map(zhash_t *hash);

    // convert map to zhash_t and take responsobility for destorying it
    zhash_t *
    map_to_zhash (std::map<std::string, std::string> map_to_convert);

} //namespace

#endif
#endif //FTY_COMMON_MLM_UTILS_H_INCLUDED
