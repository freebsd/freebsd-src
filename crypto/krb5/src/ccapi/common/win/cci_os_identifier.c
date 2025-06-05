/* ccapi/common/win/cci_os_identifier.c */
/*
 * Copyright 2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "cci_common.h"
#include "cci_os_identifier.h"

#include <rpc.h>

/* ------------------------------------------------------------------------ */

cc_int32 cci_os_identifier_new_uuid (cci_uuid_string_t *out_uuid_string) {
    cc_int32        err = ccNoError;
    UUID            uuid;
    char*           uuidStringTemp;

    err = UuidCreate(&uuid);

    if (!err) {
        err = UuidToString(&uuid, &uuidStringTemp);
        }

    if (!err) {
        *out_uuid_string = malloc(1+strlen(uuidStringTemp));

        if (*out_uuid_string) {
            strcpy(*out_uuid_string, uuidStringTemp);
            }

        RpcStringFree(&uuidStringTemp);
        }

    cci_debug_printf("cci_os_identifier_new_uuid returning %s", *out_uuid_string);

    return cci_check_error (err);
    }