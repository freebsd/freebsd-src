/* ccapi/lib/ccapi_credentials_iterator.c */
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

#include "ccapi_credentials_iterator.h"
#include "ccapi_credentials.h"
#include "ccapi_ipc.h"

/* ------------------------------------------------------------------------ */

typedef struct cci_credentials_iterator_d {
    cc_credentials_iterator_f *functions;
#if TARGET_OS_MAC
    cc_credentials_iterator_f *vector_functions;
#endif
    cci_identifier_t identifier;
    cc_uint32 compat_version;
} *cci_credentials_iterator_t;

/* ------------------------------------------------------------------------ */

struct cci_credentials_iterator_d cci_credentials_iterator_initializer = {
    NULL
    VECTOR_FUNCTIONS_INITIALIZER,
    NULL,
    0
};

cc_credentials_iterator_f cci_credentials_iterator_f_initializer = {
    ccapi_credentials_iterator_release,
    ccapi_credentials_iterator_next,
    ccapi_credentials_iterator_clone
};

/* ------------------------------------------------------------------------ */

cc_int32 cci_credentials_iterator_new (cc_credentials_iterator_t *out_credentials_iterator,
                                       cci_identifier_t           in_identifier)
{
    cc_int32 err = ccNoError;
    cci_credentials_iterator_t credentials_iterator = NULL;

    if (!out_credentials_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier           ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        credentials_iterator = malloc (sizeof (*credentials_iterator));
        if (credentials_iterator) {
            *credentials_iterator = cci_credentials_iterator_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        credentials_iterator->functions = malloc (sizeof (*credentials_iterator->functions));
        if (credentials_iterator->functions) {
            *credentials_iterator->functions = cci_credentials_iterator_f_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = cci_identifier_copy (&credentials_iterator->identifier, in_identifier);
    }

    if (!err) {
        *out_credentials_iterator = (cc_credentials_iterator_t) credentials_iterator;
        credentials_iterator = NULL; /* take ownership */
    }

    if (credentials_iterator) { ccapi_credentials_iterator_release ((cc_credentials_iterator_t) credentials_iterator); }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_credentials_iterator_write (cc_credentials_iterator_t in_credentials_iterator,
                                         k5_ipc_stream              in_stream)
{
    cc_int32 err = ccNoError;
    cci_credentials_iterator_t credentials_iterator = (cci_credentials_iterator_t) in_credentials_iterator;

    if (!in_credentials_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!in_stream         ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_write (credentials_iterator->identifier, in_stream);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_credentials_iterator_release (cc_credentials_iterator_t io_credentials_iterator)
{
    cc_int32 err = ccNoError;
    cci_credentials_iterator_t credentials_iterator = (cci_credentials_iterator_t) io_credentials_iterator;

    if (!io_credentials_iterator) { err = ccErrBadParam; }

    if (!err) {
        err =  cci_ipc_send (cci_credentials_iterator_release_msg_id,
                             credentials_iterator->identifier,
                             NULL,
                             NULL);
        if (err) {
            cci_debug_printf ("%s: cci_ipc_send failed with error %d",
                             __FUNCTION__, err);
            err = ccNoError;
        }
    }

    if (!err) {
        free ((char *) credentials_iterator->functions);
        cci_identifier_release (credentials_iterator->identifier);
        free (credentials_iterator);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_credentials_iterator_next (cc_credentials_iterator_t  in_credentials_iterator,
                                          cc_credentials_t          *out_credentials)
{
    cc_int32 err = ccNoError;
    cci_credentials_iterator_t credentials_iterator = (cci_credentials_iterator_t) in_credentials_iterator;
    k5_ipc_stream reply = NULL;

    if (!in_credentials_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials        ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_credentials_iterator_next_msg_id,
                             credentials_iterator->identifier,
                             NULL,
                             &reply);
    }

    if (!err) {
        err = cci_credentials_read (out_credentials, reply);
    }

    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_credentials_iterator_clone (cc_credentials_iterator_t  in_credentials_iterator,
                                           cc_credentials_iterator_t *out_credentials_iterator)
{
    cc_int32 err = ccNoError;
    cci_credentials_iterator_t credentials_iterator = (cci_credentials_iterator_t) in_credentials_iterator;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_credentials_iterator ) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_credentials_iterator_next_msg_id,
                             credentials_iterator->identifier,
                             NULL,
                             &reply);
    }

    if (!err) {
        err =  cci_identifier_read (&identifier, reply);
    }

    if (!err) {
        err = cci_credentials_iterator_new (out_credentials_iterator, identifier);
    }

    krb5int_ipc_stream_release (reply);
    cci_identifier_release (identifier);

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 cci_credentials_iterator_get_compat_version (cc_credentials_iterator_t  in_credentials_iterator,
                                                      cc_uint32                 *out_compat_version)
{
    cc_int32 err = ccNoError;
    cci_credentials_iterator_t credentials_iterator = (cci_credentials_iterator_t) in_credentials_iterator;

    if (!in_credentials_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!out_compat_version     ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_compat_version = credentials_iterator->compat_version;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_credentials_iterator_set_compat_version (cc_credentials_iterator_t io_credentials_iterator,
                                                      cc_uint32                 in_compat_version)
{
    cc_int32 err = ccNoError;
    cci_credentials_iterator_t credentials_iterator = (cci_credentials_iterator_t) io_credentials_iterator;

    if (!io_credentials_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        credentials_iterator->compat_version = in_compat_version;
    }

    return cci_check_error (err);
}
