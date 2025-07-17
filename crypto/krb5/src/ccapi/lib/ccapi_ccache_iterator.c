/* ccapi/lib/ccapi_ccache_iterator.c */
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

#include "ccapi_ccache_iterator.h"
#include "ccapi_ccache.h"
#include "ccapi_ipc.h"

/* ------------------------------------------------------------------------ */

typedef struct cci_ccache_iterator_d {
    cc_ccache_iterator_f *functions;
#if TARGET_OS_MAC
    cc_ccache_iterator_f *vector_functions;
#endif
    cci_identifier_t identifier;
    char *saved_ccache_name;
} *cci_ccache_iterator_t;

/* ------------------------------------------------------------------------ */

struct cci_ccache_iterator_d cci_ccache_iterator_initializer = {
    NULL
    VECTOR_FUNCTIONS_INITIALIZER,
    NULL,
    NULL
};

cc_ccache_iterator_f cci_ccache_iterator_f_initializer = {
    ccapi_ccache_iterator_release,
    ccapi_ccache_iterator_next,
    ccapi_ccache_iterator_clone
};

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_iterator_new (cc_ccache_iterator_t *out_ccache_iterator,
                                  cci_identifier_t      in_identifier)
{
    cc_int32 err = ccNoError;
    cci_ccache_iterator_t ccache_iterator = NULL;

    if (!in_identifier      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        ccache_iterator = malloc (sizeof (*ccache_iterator));
        if (ccache_iterator) {
            *ccache_iterator = cci_ccache_iterator_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        ccache_iterator->functions = malloc (sizeof (*ccache_iterator->functions));
        if (ccache_iterator->functions) {
            *ccache_iterator->functions = cci_ccache_iterator_f_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = cci_identifier_copy (&ccache_iterator->identifier, in_identifier);
    }

    if (!err) {
        *out_ccache_iterator = (cc_ccache_iterator_t) ccache_iterator;
        ccache_iterator = NULL; /* take ownership */
    }

    ccapi_ccache_iterator_release ((cc_ccache_iterator_t) ccache_iterator);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_iterator_write (cc_ccache_iterator_t in_ccache_iterator,
                                   k5_ipc_stream          in_stream)
{
    cc_int32 err = ccNoError;
    cci_ccache_iterator_t ccache_iterator = (cci_ccache_iterator_t) in_ccache_iterator;

    if (!in_ccache_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!in_stream         ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_identifier_write (ccache_iterator->identifier, in_stream);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_iterator_release (cc_ccache_iterator_t io_ccache_iterator)
{
    cc_int32 err = ccNoError;
    cci_ccache_iterator_t ccache_iterator = (cci_ccache_iterator_t) io_ccache_iterator;

    if (!io_ccache_iterator) { err = ccErrBadParam; }

    if (!err) {
        cc_uint32 initialized = 0;

        err = cci_identifier_is_initialized (ccache_iterator->identifier,
                                             &initialized);

        if (!err && initialized) {
            err =  cci_ipc_send (cci_ccache_iterator_release_msg_id,
                                 ccache_iterator->identifier,
                                 NULL,
                                 NULL);
            if (err) {
                cci_debug_printf ("%s: cci_ipc_send failed with error %d",
                                 __FUNCTION__, err);
                err = ccNoError;
            }
        }
    }

    if (!err) {
        free ((char *) ccache_iterator->functions);
        cci_identifier_release (ccache_iterator->identifier);
        free (ccache_iterator->saved_ccache_name);
        free (ccache_iterator);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_iterator_next (cc_ccache_iterator_t  in_ccache_iterator,
                                     cc_ccache_t          *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_ccache_iterator_t ccache_iterator = (cci_ccache_iterator_t) in_ccache_iterator;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_ccache_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache        ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cc_uint32 initialized = 0;

        err = cci_identifier_is_initialized (ccache_iterator->identifier,
                                             &initialized);

        if (!err && !initialized) {
            /* server doesn't actually exist.  Pretend we're empty. */
            err = cci_check_error (ccIteratorEnd);
        }
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_iterator_next_msg_id,
                             ccache_iterator->identifier,
                             NULL,
                             &reply);
    }

    if (!err) {
        err = cci_identifier_read (&identifier, reply);
    }

    if (!err) {
        err = cci_ccache_new (out_ccache, identifier);
    }

    krb5int_ipc_stream_release (reply);
    cci_identifier_release (identifier);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_iterator_clone (cc_ccache_iterator_t  in_ccache_iterator,
                                      cc_ccache_iterator_t *out_ccache_iterator)
{
    cc_int32 err = ccNoError;
    cci_ccache_iterator_t ccache_iterator = (cci_ccache_iterator_t) in_ccache_iterator;
    k5_ipc_stream reply = NULL;
    cc_uint32 initialized = 0;
    cci_identifier_t identifier = NULL;

    if (!in_ccache_iterator ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_is_initialized (ccache_iterator->identifier,
                                             &initialized);
    }

    if (!err) {
        if (initialized) {
            err =  cci_ipc_send (cci_ccache_iterator_next_msg_id,
                                 ccache_iterator->identifier,
                                 NULL,
                                 &reply);

            if (!err) {
                err =  cci_identifier_read (&identifier, reply);
            }

        } else {
            /* server doesn't actually exist.  Make another dummy one. */
            identifier = cci_identifier_uninitialized;
        }
    }

    if (!err) {
        err = cci_ccache_iterator_new (out_ccache_iterator, identifier);
    }

    cci_identifier_release (identifier);
    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_iterator_get_saved_ccache_name (cc_ccache_iterator_t   in_ccache_iterator,
                                                    const char           **out_saved_ccache_name)
{
    cc_int32 err = ccNoError;
    cci_ccache_iterator_t ccache_iterator = (cci_ccache_iterator_t) in_ccache_iterator;

    if (!in_ccache_iterator   ) { err = cci_check_error (ccErrBadParam); }
    if (!out_saved_ccache_name) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_saved_ccache_name = ccache_iterator->saved_ccache_name;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_iterator_set_saved_ccache_name (cc_ccache_iterator_t  io_ccache_iterator,
                                                    const char            *in_saved_ccache_name)
{
    cc_int32 err = ccNoError;
    cci_ccache_iterator_t ccache_iterator = (cci_ccache_iterator_t) io_ccache_iterator;
    char *new_saved_ccache_name = NULL;

    if (!io_ccache_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err && in_saved_ccache_name) {
        new_saved_ccache_name = strdup (in_saved_ccache_name);
        if (!new_saved_ccache_name) { err = ccErrNoMem; }
    }

    if (!err) {
        free (ccache_iterator->saved_ccache_name);

        ccache_iterator->saved_ccache_name = new_saved_ccache_name;
        new_saved_ccache_name = NULL; /* take ownership */
    }

    free (new_saved_ccache_name);

    return cci_check_error (err);
}
