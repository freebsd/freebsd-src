/* ccapi/common/mac/cci_os_identifier.c */
/*
 * Copyright 2006 Massachusetts Institute of Technology.
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

#include <CoreFoundation/CoreFoundation.h>

/* ------------------------------------------------------------------------ */

cc_int32 cci_os_identifier_new_uuid (cci_uuid_string_t *out_uuid_string)
{
    cc_int32 err = ccNoError;
    cci_uuid_string_t uuid_string = NULL;
    CFUUIDRef uuid = NULL;
    CFStringRef uuid_stringref = NULL;
    CFStringEncoding encoding = kCFStringEncodingUTF8;
    CFIndex length = 0;

    if (!out_uuid_string) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        uuid = CFUUIDCreate (kCFAllocatorDefault);
        if (!uuid) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        uuid_stringref = CFUUIDCreateString (kCFAllocatorDefault, uuid);
        if (!uuid_stringref) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (uuid_stringref),
                                                    encoding) + 1;

        uuid_string = malloc (length);
        if (!uuid_string) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        if (!CFStringGetCString (uuid_stringref, uuid_string, length, encoding)) {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        *out_uuid_string = uuid_string;
        uuid_string = NULL; /* take ownership */
    }

    if (uuid_string   ) { free (uuid_string); }
    if (uuid_stringref) { CFRelease (uuid_stringref); }
    if (uuid          ) { CFRelease (uuid); }

    return cci_check_error (err);
}
