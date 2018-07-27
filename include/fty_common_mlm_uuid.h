/*  =========================================================================
    fty_common_common_fty_uuid - Calculates UUID for assets.

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

#ifndef FTY_COMMON_MLM_UUID_H_INCLUDED
#define FTY_COMMON_MLM_UUID_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
typedef struct _fty_uuid_t fty_uuid_t;

//  Create a new fty_uuid
FTY_COMMON_MLM_EXPORT fty_uuid_t *
    fty_uuid_new (void);

//  Destroy the fty_uuid
void
    fty_uuid_destroy (fty_uuid_t **self_p);

//  Calculate UUID v5 in EATON namespace base on manufacturer, model and serial number
const char*
    fty_uuid_calculate (fty_uuid_t *self, const char *mfr, const char *model, const char *serial);

//  Generate random UUID
const char*
    fty_uuid_generate (fty_uuid_t *self);

// Returns uuid.
// If we have all necessary information (serial nr, model, manufacturer) we
// calculate uuid. I we don't and asset type is device we use ffff-...
// otherwise some random uuid is generated for other asset types.
FTY_COMMON_MLM_EXPORT const char*
    fty_uuid_create (zhash_t *ext, const char *asset_type, fty_uuid_t *uuid);

//  Self test of this class
void
    fty_common_common_fty_uuid_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
