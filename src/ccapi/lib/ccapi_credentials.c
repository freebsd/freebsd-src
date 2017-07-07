/* ccapi/lib/ccapi_credentials.c */
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

#include "ccapi_credentials.h"

#include "ccapi_string.h"

/* ------------------------------------------------------------------------ */

typedef struct cci_credentials_d {
    cc_credentials_union *data;
    cc_credentials_f *functions;
#if TARGET_OS_MAC
    cc_credentials_f *vector_functions;
#endif
    cci_identifier_t identifier;
} *cci_credentials_t;

/* ------------------------------------------------------------------------ */

struct cci_credentials_d cci_credentials_initializer = {
    NULL,
    NULL
    VECTOR_FUNCTIONS_INITIALIZER,
    NULL
};

cc_credentials_f cci_credentials_f_initializer = {
    ccapi_credentials_release,
    ccapi_credentials_compare
};

cc_credentials_union cci_credentials_union_initializer = {
    0,
    { NULL }
};

/* ------------------------------------------------------------------------ */

cc_int32 cci_credentials_read (cc_credentials_t *out_credentials,
                               k5_ipc_stream      in_stream)
{
    cc_int32 err = ccNoError;
    cci_credentials_t credentials = NULL;

    if (!out_credentials) { err = cci_check_error (ccErrBadParam); }
    if (!in_stream      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        credentials = malloc (sizeof (*credentials));
        if (credentials) {
            *credentials = cci_credentials_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        credentials->functions = malloc (sizeof (*credentials->functions));
        if (credentials->functions) {
            *credentials->functions = cci_credentials_f_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = cci_identifier_read (&credentials->identifier, in_stream);
    }

    if (!err) {
        err = cci_credentials_union_read (&credentials->data, in_stream);
    }

    if (!err) {
        *out_credentials = (cc_credentials_t) credentials;
        credentials = NULL; /* take ownership */
    }

    if (credentials) { ccapi_credentials_release ((cc_credentials_t) credentials); }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_credentials_write (cc_credentials_t in_credentials,
                                k5_ipc_stream     in_stream)
{
    cc_int32 err = ccNoError;
    cci_credentials_t credentials = (cci_credentials_t) in_credentials;

    if (!in_credentials) { err = cci_check_error (ccErrBadParam); }
    if (!in_stream     ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_write (credentials->identifier, in_stream);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_credentials_compare (cc_credentials_t  in_credentials,
                                    cc_credentials_t  in_compare_to_credentials,
                                    cc_uint32        *out_equal)
{
    cc_int32 err = ccNoError;
    cci_credentials_t credentials = (cci_credentials_t) in_credentials;
    cci_credentials_t compare_to_credentials = (cci_credentials_t) in_compare_to_credentials;

    if (!in_credentials           ) { err = cci_check_error (ccErrBadParam); }
    if (!in_compare_to_credentials) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal                ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_compare (credentials->identifier,
                                      compare_to_credentials->identifier,
                                      out_equal);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_credentials_release (cc_credentials_t io_credentials)
{
    cc_int32 err = ccNoError;
    cci_credentials_t credentials = (cci_credentials_t) io_credentials;

    if (!io_credentials) { err = ccErrBadParam; }

    if (!err) {
        cci_credentials_union_release (credentials->data);
        free ((char *) credentials->functions);
        cci_identifier_release (credentials->identifier);
        free (credentials);
    }

    return err;
}
