/* ccapi/lib/ccapi_string.c */
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

#include "ccapi_string.h"

/* ------------------------------------------------------------------------ */

cc_string_d cci_string_d_initializer = {
    NULL,
    NULL
    VECTOR_FUNCTIONS_INITIALIZER };

cc_string_f cci_string_f_initializer = {
    ccapi_string_release
};

/* ------------------------------------------------------------------------ */

cc_int32 cci_string_new (cc_string_t *out_string,
                         char        *in_cstring)
{
    cc_int32 err = ccNoError;
    cc_string_t string = NULL;

    if (!out_string) { err = cci_check_error (ccErrBadParam); }
    if (!in_cstring) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        string = malloc (sizeof (*string));
        if (string) {
            *string = cci_string_d_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        string->functions = malloc (sizeof (*string->functions));
        if (string->functions) {
            *((cc_string_f *) string->functions) = cci_string_f_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        string->data = strdup (in_cstring);
        if (!string->data) {
            err = cci_check_error (ccErrNoMem);
        }

    }

    if (!err) {
        *out_string = string;
        string = NULL; /* take ownership */
    }

    if (string) { ccapi_string_release (string); }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_string_release (cc_string_t in_string)
{
    cc_int32 err = ccNoError;

    if (!in_string) { err = ccErrBadParam; }

    if (!err) {
        free ((char *) in_string->data);
        free ((char *) in_string->functions);
        free (in_string);
    }

    return err;
}
