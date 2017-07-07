/* ccapi/lib/ccapi_context.c */
/*
 * Copyright 2006, 2007 Massachusetts Institute of Technology.
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

#include "ccapi_context.h"

#include "k5-platform.h"

#include "ccapi_ccache.h"
#include "ccapi_ccache_iterator.h"
#include "ccapi_string.h"
#include "ccapi_ipc.h"
#include "ccapi_context_change_time.h"
#include "ccapi_err.h"

#include <CredentialsCache2.h>

typedef struct cci_context_d {
    cc_context_f *functions;
#if TARGET_OS_MAC
    cc_context_f *vector_functions;
#endif
    cci_identifier_t identifier;
    cc_uint32 synchronized;
    cc_time_t last_wait_for_change_time;
} *cci_context_t;

/* ------------------------------------------------------------------------ */

struct cci_context_d cci_context_initializer = {
    NULL
    VECTOR_FUNCTIONS_INITIALIZER,
    NULL,
    0,
    0
};

cc_context_f cci_context_f_initializer = {
    ccapi_context_release,
    ccapi_context_get_change_time,
    ccapi_context_get_default_ccache_name,
    ccapi_context_open_ccache,
    ccapi_context_open_default_ccache,
    ccapi_context_create_ccache,
    ccapi_context_create_default_ccache,
    ccapi_context_create_new_ccache,
    ccapi_context_new_ccache_iterator,
    ccapi_context_lock,
    ccapi_context_unlock,
    ccapi_context_compare,
    ccapi_context_wait_for_change
};

static cc_int32 cci_context_sync (cci_context_t in_context,
                                  cc_uint32     in_launch);

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

MAKE_INIT_FUNCTION(cci_process_init);
MAKE_FINI_FUNCTION(cci_process_fini);

/* ------------------------------------------------------------------------ */

static int cci_process_init (void)
{
    cc_int32 err = ccNoError;

    if (!err) {
        err = cci_context_change_time_thread_init ();
    }

    if (!err) {
        err = cci_ipc_process_init ();
    }

    if (!err) {
        add_error_table (&et_CAPI_error_table);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

static void cci_process_fini (void)
{
    if (!INITIALIZER_RAN (cci_process_init) || PROGRAM_EXITING ()) {
	return;
    }

    remove_error_table(&et_CAPI_error_table);
    cci_context_change_time_thread_fini ();
}


#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 cc_initialize (cc_context_t  *out_context,
                        cc_int32       in_version,
                        cc_int32      *out_supported_version,
                        char const   **out_vendor)
{
    cc_int32 err = ccNoError;
    cci_context_t context = NULL;
    static char *vendor_string = "MIT Kerberos CCAPI";

    if (!out_context) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = CALL_INIT_FUNCTION (cci_process_init);
    }

    if (!err) {
        switch (in_version) {
            case ccapi_version_2:
            case ccapi_version_3:
            case ccapi_version_4:
            case ccapi_version_5:
            case ccapi_version_6:
            case ccapi_version_7:
                break;

            default:
                err = ccErrBadAPIVersion;
                break;
        }
    }

    if (!err) {
        context = malloc (sizeof (*context));
        if (context) {
            *context = cci_context_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        context->functions = malloc (sizeof (*context->functions));
        if (context->functions) {
            *context->functions = cci_context_f_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        context->identifier = cci_identifier_uninitialized;

        *out_context = (cc_context_t) context;
        context = NULL; /* take ownership */

        if (out_supported_version) {
            *out_supported_version = ccapi_version_max;
        }

        if (out_vendor) {
            *out_vendor = vendor_string;
        }
    }

    ccapi_context_release ((cc_context_t) context);

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */
/*
 * Currently does not need to talk to the server since the server must
 * handle cleaning up resources from crashed clients anyway.
 *
 * NOTE: if server communication is ever added here, make sure that
 * krb5_stdcc_shutdown calls an internal function which does not talk to the
 * server.  krb5_stdcc_shutdown is called from thread fini functions and may
 * crash talking to the server depending on what order the OS calls the fini
 * functions (ie: if the ipc layer fini function is called first).
 */

cc_int32 ccapi_context_release (cc_context_t in_context)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;

    if (!in_context) { err = ccErrBadParam; }

    if (!err) {
        cci_identifier_release (context->identifier);
        free (context->functions);
        free (context);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_get_change_time (cc_context_t  in_context,
                                        cc_time_t    *out_change_time)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream reply = NULL;

    if (!in_context     ) { err = cci_check_error (ccErrBadParam); }
    if (!out_change_time) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_context_sync (context, 0);
    }

    if (!err) {
        err =  cci_ipc_send_no_launch (cci_context_get_change_time_msg_id,
                                       context->identifier,
                                       NULL, &reply);
    }

    if (!err && krb5int_ipc_stream_size (reply) > 0) {
        cc_time_t change_time = 0;

        /* got a response from the server */
        err = krb5int_ipc_stream_read_time (reply, &change_time);

        if (!err) {
            err = cci_context_change_time_update (context->identifier,
                                                  change_time);
        }
    }

    if (!err) {
        err = cci_context_change_time_get (out_change_time);
    }

    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_wait_for_change (cc_context_t  in_context)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (request, context->last_wait_for_change_time);
    }

    if (!err) {
        err = cci_context_sync (context, 1);
    }

    if (!err) {
        err = cci_ipc_send (cci_context_wait_for_change_msg_id,
			    context->identifier,
			    request,
			    &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (reply, &context->last_wait_for_change_time);
    }

    krb5int_ipc_stream_release (request);
    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_get_default_ccache_name (cc_context_t  in_context,
                                                cc_string_t  *out_name)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream reply = NULL;
    char *reply_name = NULL;
    char *name = NULL;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!out_name  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_context_sync (context, 0);
    }

    if (!err) {
        err =  cci_ipc_send_no_launch (cci_context_get_default_ccache_name_msg_id,
                                       context->identifier,
                                       NULL,
                                       &reply);
    }

    if (!err) {
        if (krb5int_ipc_stream_size (reply) > 0) {
            /* got a response from the server */
            err = krb5int_ipc_stream_read_string (reply, &reply_name);

            if (!err) {
                name = reply_name;
            }
        } else {
            name = k_cci_context_initial_ccache_name;
        }
    }

    if (!err) {
        err = cci_string_new (out_name, name);
    }

    krb5int_ipc_stream_release (reply);
    krb5int_ipc_stream_free_string (reply_name);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_open_ccache (cc_context_t  in_context,
                                    const char   *in_name,
                                    cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_name     ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (request, in_name);
    }

    if (!err) {
        err = cci_context_sync (context, 0);
    }

    if (!err) {
        err =  cci_ipc_send_no_launch (cci_context_open_ccache_msg_id,
                                       context->identifier,
                                       request,
                                       &reply);
    }

    if (!err && !(krb5int_ipc_stream_size (reply) > 0)) {
        err = ccErrCCacheNotFound;
    }

    if (!err) {
        err = cci_identifier_read (&identifier, reply);
    }

    if (!err) {
        err = cci_ccache_new (out_ccache, identifier);
    }

    cci_identifier_release (identifier);
    krb5int_ipc_stream_release (reply);
    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_open_default_ccache (cc_context_t  in_context,
                                            cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_context_sync (context, 0);
    }

    if (!err) {
        err =  cci_ipc_send_no_launch (cci_context_open_default_ccache_msg_id,
                                       context->identifier,
                                       NULL,
                                       &reply);
    }

    if (!err && !(krb5int_ipc_stream_size (reply) > 0)) {
        err = ccErrCCacheNotFound;
    }

    if (!err) {
        err = cci_identifier_read (&identifier, reply);
    }

    if (!err) {
        err = cci_ccache_new (out_ccache, identifier);
    }

    cci_identifier_release (identifier);
    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_create_ccache (cc_context_t  in_context,
                                      const char   *in_name,
                                      cc_uint32     in_cred_vers,
                                      const char   *in_principal,
                                      cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_name     ) { err = cci_check_error (ccErrBadParam); }
    if (!in_principal) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (request, in_name);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_cred_vers);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (request, in_principal);
    }

    if (!err) {
        err = cci_context_sync (context, 1);
    }

    if (!err) {
        err =  cci_ipc_send (cci_context_create_ccache_msg_id,
                             context->identifier,
                             request,
                             &reply);
    }

    if (!err) {
        err = cci_identifier_read (&identifier, reply);
    }

    if (!err) {
        err = cci_ccache_new (out_ccache, identifier);
    }

    cci_identifier_release (identifier);
    krb5int_ipc_stream_release (reply);
    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_create_default_ccache (cc_context_t  in_context,
                                              cc_uint32     in_cred_vers,
                                              const char   *in_principal,
                                              cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_principal) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_cred_vers);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (request, in_principal);
    }

    if (!err) {
        err = cci_context_sync (context, 1);
    }

    if (!err) {
        err =  cci_ipc_send (cci_context_create_default_ccache_msg_id,
                             context->identifier,
                             request,
                             &reply);
    }

    if (!err) {
        err = cci_identifier_read (&identifier, reply);
    }

    if (!err) {
        err = cci_ccache_new (out_ccache, identifier);
    }

    cci_identifier_release (identifier);
    krb5int_ipc_stream_release (reply);
    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_create_new_ccache (cc_context_t in_context,
                                          cc_uint32    in_cred_vers,
                                          const char  *in_principal,
                                          cc_ccache_t *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_principal) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_cred_vers);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (request, in_principal);
    }

    if (!err) {
        err = cci_context_sync (context, 1);
    }

    if (!err) {
        err =  cci_ipc_send (cci_context_create_new_ccache_msg_id,
                             context->identifier,
                             request,
                             &reply);
    }

    if (!err) {
        err = cci_identifier_read (&identifier, reply);
    }

    if (!err) {
        err = cci_ccache_new (out_ccache, identifier);
    }

    cci_identifier_release (identifier);
    krb5int_ipc_stream_release (reply);
    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_new_ccache_iterator (cc_context_t          in_context,
                                            cc_ccache_iterator_t *out_iterator)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!out_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_context_sync (context, 0);
    }

    if (!err) {
        err =  cci_ipc_send_no_launch (cci_context_new_ccache_iterator_msg_id,
                                       context->identifier,
                                       NULL,
                                       &reply);
    }

    if (!err) {
        if (krb5int_ipc_stream_size (reply) > 0) {
            err = cci_identifier_read (&identifier, reply);
        } else {
            identifier = cci_identifier_uninitialized;
        }
    }

    if (!err) {
        err = cci_ccache_iterator_new (out_iterator, identifier);
    }

    krb5int_ipc_stream_release (reply);
    cci_identifier_release (identifier);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_lock (cc_context_t in_context,
                             cc_uint32    in_lock_type,
                             cc_uint32    in_block)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream request = NULL;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_lock_type);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_block);
    }

    if (!err) {
        err = cci_context_sync (context, 1);
    }

    if (!err) {
        err =  cci_ipc_send (cci_context_lock_msg_id,
                             context->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_unlock (cc_context_t in_context)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_context_sync (context, 1);
    }

    if (!err) {
        err =  cci_ipc_send (cci_context_unlock_msg_id,
                             context->identifier,
                             NULL,
                             NULL);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_context_compare (cc_context_t  in_context,
                                cc_context_t  in_compare_to_context,
                                cc_uint32    *out_equal)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    cci_context_t compare_to_context = (cci_context_t) in_compare_to_context;

    if (!in_context           ) { err = cci_check_error (ccErrBadParam); }
    if (!in_compare_to_context) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal            ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_context_sync (context, 0);
    }

    if (!err) {
        err = cci_context_sync (compare_to_context, 0);
    }

    if (!err) {
        /* If both contexts can't talk to the server, then
         * we assume they are equivalent */
        err = cci_identifier_compare (context->identifier,
                                      compare_to_context->identifier,
                                      out_equal);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 cci_context_sync (cci_context_t in_context,
                                  cc_uint32     in_launch)
{
    cc_int32 err = ccNoError;
    cci_context_t context = (cci_context_t) in_context;
    k5_ipc_stream reply = NULL;
    cci_identifier_t new_identifier = NULL;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        /* Use the uninitialized identifier because we may be talking  */
        /* to a different server which would reject our identifier and */
        /* the point of this message is to sync with the server's id   */
        if (in_launch) {
            err =  cci_ipc_send (cci_context_sync_msg_id,
                                 cci_identifier_uninitialized,
                                 NULL,
                                 &reply);
        } else {
            err =  cci_ipc_send_no_launch (cci_context_sync_msg_id,
                                           cci_identifier_uninitialized,
                                           NULL,
                                           &reply);
        }
    }

    if (!err) {
        if (krb5int_ipc_stream_size (reply) > 0) {
            err = cci_identifier_read (&new_identifier, reply);
        } else {
            new_identifier = cci_identifier_uninitialized;
        }
    }

    if (!err) {
        cc_uint32 equal = 0;

        err = cci_identifier_compare (context->identifier, new_identifier, &equal);

        if (!err && !equal) {
            if (context->identifier) {
                cci_identifier_release (context->identifier);
            }
            context->identifier = new_identifier;
            new_identifier = NULL;  /* take ownership */
        }
    }

    if (!err && context->synchronized) {
        err = cci_context_change_time_sync (context->identifier);
    }

    if (!err && !context->synchronized) {
        /* Keep state about whether this is the first call to avoid always   */
        /* modifying the global change time on the context's first ipc call. */
        context->synchronized = 1;
    }

    cci_identifier_release (new_identifier);
    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}
