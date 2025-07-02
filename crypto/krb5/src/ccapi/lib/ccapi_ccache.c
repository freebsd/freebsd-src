/* ccapi/lib/ccapi_ccache.c */
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

#include "ccapi_ccache.h"

#include "ccapi_string.h"
#include "ccapi_credentials.h"
#include "ccapi_credentials_iterator.h"
#include "ccapi_ipc.h"

/* ------------------------------------------------------------------------ */

typedef struct cci_ccache_d {
    cc_ccache_f *functions;
#if TARGET_OS_MAC
    cc_ccache_f *vector_functions;
#endif
    cci_identifier_t identifier;
    cc_time_t last_wait_for_change_time;
    cc_uint32 compat_version;
} *cci_ccache_t;

/* ------------------------------------------------------------------------ */

struct cci_ccache_d cci_ccache_initializer = {
    NULL
    VECTOR_FUNCTIONS_INITIALIZER,
    NULL,
    0
};

cc_ccache_f cci_ccache_f_initializer = {
    ccapi_ccache_release,
    ccapi_ccache_destroy,
    ccapi_ccache_set_default,
    ccapi_ccache_get_credentials_version,
    ccapi_ccache_get_name,
    ccapi_ccache_get_principal,
    ccapi_ccache_set_principal,
    ccapi_ccache_store_credentials,
    ccapi_ccache_remove_credentials,
    ccapi_ccache_new_credentials_iterator,
    ccapi_ccache_move,
    ccapi_ccache_lock,
    ccapi_ccache_unlock,
    ccapi_ccache_get_last_default_time,
    ccapi_ccache_get_change_time,
    ccapi_ccache_compare,
    ccapi_ccache_get_kdc_time_offset,
    ccapi_ccache_set_kdc_time_offset,
    ccapi_ccache_clear_kdc_time_offset,
    ccapi_ccache_wait_for_change
};

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_new (cc_ccache_t      *out_ccache,
                         cci_identifier_t  in_identifier)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = NULL;

    if (!out_ccache   ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        ccache = malloc (sizeof (*ccache));
        if (ccache) {
            *ccache = cci_ccache_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        ccache->functions = malloc (sizeof (*ccache->functions));
        if (ccache->functions) {
            *ccache->functions = cci_ccache_f_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = cci_identifier_copy (&ccache->identifier, in_identifier);
    }

    if (!err) {
        *out_ccache = (cc_ccache_t) ccache;
        ccache = NULL; /* take ownership */
    }

    ccapi_ccache_release ((cc_ccache_t) ccache);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_write (cc_ccache_t  in_ccache,
                           k5_ipc_stream in_stream)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;

    if (!in_ccache) { err = cci_check_error (ccErrBadParam); }
    if (!in_stream) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_write (ccache->identifier, in_stream);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_release (cc_ccache_t io_ccache)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;

    if (!io_ccache) { err = ccErrBadParam; }

    if (!err) {
        cci_identifier_release (ccache->identifier);

        free ((char *) ccache->functions);
        free (ccache);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_destroy (cc_ccache_t io_ccache)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;

    if (!io_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_destroy_msg_id,
                             ccache->identifier,
                             NULL,
                             NULL);
    }

    if (!err) {
        err = ccapi_ccache_release (io_ccache);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_set_default (cc_ccache_t io_ccache)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;

    if (!io_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_set_default_msg_id,
                             ccache->identifier,
                             NULL,
                             NULL);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_get_credentials_version (cc_ccache_t  in_ccache,
                                               cc_uint32   *out_credentials_version)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream reply = NULL;

    if (!in_ccache              ) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials_version) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_get_credentials_version_msg_id,
                             ccache->identifier,
                             NULL,
                             &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (reply, out_credentials_version);
    }

    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_get_name (cc_ccache_t  in_ccache,
                                cc_string_t *out_name)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream reply = NULL;
    char *name = NULL;

    if (!in_ccache) { err = cci_check_error (ccErrBadParam); }
    if (!out_name ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_get_name_msg_id,
                             ccache->identifier,
                             NULL,
                             &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (reply, &name);
    }

    if (!err) {
        err = cci_string_new (out_name, name);
    }

    krb5int_ipc_stream_release (reply);
    krb5int_ipc_stream_free_string (name);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_get_principal (cc_ccache_t  in_ccache,
                                     cc_uint32    in_credentials_version,
                                     cc_string_t *out_principal)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;
    char *principal = NULL;

    if (!in_ccache    ) { err = cci_check_error (ccErrBadParam); }
    if (!out_principal) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_credentials_version);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_get_principal_msg_id,
                             ccache->identifier,
                             request,
                             &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (reply, &principal);
    }

    if (!err) {
        err = cci_string_new (out_principal, principal);
    }

    krb5int_ipc_stream_release (request);
    krb5int_ipc_stream_release (reply);
    krb5int_ipc_stream_free_string (principal);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_set_principal (cc_ccache_t  io_ccache,
                                     cc_uint32    in_credentials_version,
                                     const char  *in_principal)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;
    k5_ipc_stream request = NULL;

    if (!io_ccache   ) { err = cci_check_error (ccErrBadParam); }
    if (!in_principal) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_credentials_version);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (request, in_principal);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_set_principal_msg_id,
                             ccache->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_store_credentials (cc_ccache_t                 io_ccache,
                                         const cc_credentials_union *in_credentials_union)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;
    k5_ipc_stream request = NULL;

    if (!io_ccache           ) { err = cci_check_error (ccErrBadParam); }
    if (!in_credentials_union) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = cci_credentials_union_write (in_credentials_union, request);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_store_credentials_msg_id,
                             ccache->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_remove_credentials (cc_ccache_t      io_ccache,
                                          cc_credentials_t in_credentials)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;
    k5_ipc_stream request = NULL;

    if (!io_ccache     ) { err = cci_check_error (ccErrBadParam); }
    if (!in_credentials) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = cci_credentials_write (in_credentials, request);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_remove_credentials_msg_id,
                             ccache->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_new_credentials_iterator (cc_ccache_t                in_ccache,
                                                cc_credentials_iterator_t *out_credentials_iterator)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream reply = NULL;
    cci_identifier_t identifier = NULL;

    if (!in_ccache               ) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_new_credentials_iterator_msg_id,
                             ccache->identifier,
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

/* ------------------------------------------------------------------------ */
/* Note: message is sent as the destination to avoid extra work on the      */
/* server when deleting it the source ccache.                               */

cc_int32 ccapi_ccache_move (cc_ccache_t io_source_ccache,
                            cc_ccache_t io_destination_ccache)
{
    cc_int32 err = ccNoError;
    cci_ccache_t source_ccache = (cci_ccache_t) io_source_ccache;
    cci_ccache_t destination_ccache = (cci_ccache_t) io_destination_ccache;
    k5_ipc_stream request = NULL;

    if (!io_source_ccache     ) { err = cci_check_error (ccErrBadParam); }
    if (!io_destination_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = cci_identifier_write (source_ccache->identifier, request);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_move_msg_id,
                             destination_ccache->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_lock (cc_ccache_t io_ccache,
                            cc_uint32   in_lock_type,
                            cc_uint32   in_block)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;
    k5_ipc_stream request = NULL;

    if (!io_ccache) { err = cci_check_error (ccErrBadParam); }

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
        err =  cci_ipc_send (cci_ccache_lock_msg_id,
                             ccache->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_unlock (cc_ccache_t io_ccache)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;

    if (!io_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_unlock_msg_id,
                             ccache->identifier,
                             NULL,
                             NULL);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_get_last_default_time (cc_ccache_t  in_ccache,
                                             cc_time_t   *out_last_default_time)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream reply = NULL;

    if (!in_ccache            ) { err = cci_check_error (ccErrBadParam); }
    if (!out_last_default_time) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_get_last_default_time_msg_id,
                             ccache->identifier,
                             NULL,
                             &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (reply, out_last_default_time);
    }

    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_get_change_time (cc_ccache_t  in_ccache,
                                       cc_time_t   *out_change_time)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream reply = NULL;

    if (!in_ccache      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_change_time) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_get_change_time_msg_id,
                             ccache->identifier,
                             NULL,
                             &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (reply, out_change_time);
    }

    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_wait_for_change (cc_ccache_t  in_ccache)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;

    if (!in_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (request, ccache->last_wait_for_change_time);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_wait_for_change_msg_id,
                             ccache->identifier,
                             request,
			     &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (reply, &ccache->last_wait_for_change_time);
    }

    krb5int_ipc_stream_release (request);
    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_compare (cc_ccache_t  in_ccache,
                               cc_ccache_t  in_compare_to_ccache,
                               cc_uint32   *out_equal)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    cci_ccache_t compare_to_ccache = (cci_ccache_t) in_compare_to_ccache;

    if (!in_ccache           ) { err = cci_check_error (ccErrBadParam); }
    if (!in_compare_to_ccache) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal           ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_compare (ccache->identifier,
                                      compare_to_ccache->identifier,
                                      out_equal);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_get_kdc_time_offset (cc_ccache_t  in_ccache,
                                           cc_uint32    in_credentials_version,
                                           cc_time_t   *out_time_offset)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;
    k5_ipc_stream request = NULL;
    k5_ipc_stream reply = NULL;

    if (!in_ccache      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_time_offset) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_credentials_version);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_get_kdc_time_offset_msg_id,
                             ccache->identifier,
                             request,
                             &reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (reply, out_time_offset);
    }

    krb5int_ipc_stream_release (request);
    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_set_kdc_time_offset (cc_ccache_t io_ccache,
                                           cc_uint32   in_credentials_version,
                                           cc_time_t   in_time_offset)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;
    k5_ipc_stream request = NULL;

    if (!io_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_credentials_version);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (request, in_time_offset);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_set_kdc_time_offset_msg_id,
                             ccache->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccapi_ccache_clear_kdc_time_offset (cc_ccache_t io_ccache,
                                             cc_uint32   in_credentials_version)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;
    k5_ipc_stream request = NULL;

    if (!io_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_credentials_version);
    }

    if (!err) {
        err =  cci_ipc_send (cci_ccache_clear_kdc_time_offset_msg_id,
                             ccache->identifier,
                             request,
                             NULL);
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_get_compat_version (cc_ccache_t  in_ccache,
                                        cc_uint32   *out_compat_version)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) in_ccache;

    if (!in_ccache         ) { err = cci_check_error (ccErrBadParam); }
    if (!out_compat_version) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_compat_version = ccache->compat_version;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_ccache_set_compat_version (cc_ccache_t io_ccache,
                                        cc_uint32   in_compat_version)
{
    cc_int32 err = ccNoError;
    cci_ccache_t ccache = (cci_ccache_t) io_ccache;

    if (!io_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        ccache->compat_version = in_compat_version;
    }

    return cci_check_error (err);
}
