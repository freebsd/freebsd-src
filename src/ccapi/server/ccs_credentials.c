/* ccapi/server/ccs_credentials.c */
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

#include "ccs_common.h"

struct ccs_credentials_d {
    cc_credentials_union *cred_union;
    cci_identifier_t identifier;
};

struct ccs_credentials_d ccs_credentials_initializer = { NULL, NULL };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_new (ccs_credentials_t      *out_credentials,
                              k5_ipc_stream            in_stream,
                              cc_uint32               in_ccache_version,
                              ccs_credentials_list_t  io_credentials_list)
{
    cc_int32 err = ccNoError;
    ccs_credentials_t credentials = NULL;

    if (!out_credentials) { err = cci_check_error (ccErrBadParam); }
    if (!in_stream      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        credentials = malloc (sizeof (*credentials));
        if (credentials) {
            *credentials = ccs_credentials_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = cci_credentials_union_read (&credentials->cred_union, in_stream);
    }

    if (!err && !(credentials->cred_union->version & in_ccache_version)) {
        /* ccache does not have a principal set for this credentials version */
        err = cci_check_error (ccErrBadCredentialsVersion);
    }

    if (!err) {
        err = ccs_server_new_identifier (&credentials->identifier);
    }

    if (!err) {
        err = ccs_credentials_list_add (io_credentials_list, credentials);
    }

    if (!err) {
        *out_credentials = credentials;
        credentials = NULL;
    }

    ccs_credentials_release (credentials);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_release (ccs_credentials_t io_credentials)
{
    cc_int32 err = ccNoError;

    if (!err && io_credentials) {
        cci_credentials_union_release (io_credentials->cred_union);
        cci_identifier_release (io_credentials->identifier);
        free (io_credentials);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_write (ccs_credentials_t in_credentials,
                                k5_ipc_stream      io_stream)
{
    cc_int32 err = ccNoError;

    if (!in_credentials) { err = cci_check_error (ccErrBadParam); }
    if (!io_stream     ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_write (in_credentials->identifier, io_stream);
    }

    if (!err) {
        err = cci_credentials_union_write (in_credentials->cred_union, io_stream);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_compare_identifier (ccs_credentials_t  in_credentials,
                                             cci_identifier_t   in_identifier,
                                             cc_uint32         *out_equal)
{
    cc_int32 err = ccNoError;

    if (!in_credentials) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier ) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal     ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_compare (in_credentials->identifier,
                                      in_identifier,
                                      out_equal);
    }

    return cci_check_error (err);
}
