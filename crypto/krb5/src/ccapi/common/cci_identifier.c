/* ccapi/common/cci_identifier.c */
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

#include "cci_common.h"
#include "cci_os_identifier.h"

struct cci_identifier_d {
    cci_uuid_string_t server_id;
    cci_uuid_string_t object_id;
};

struct cci_identifier_d cci_identifier_initializer = { NULL, NULL };

#define cci_uninitialized_server_id "NEEDS_SYNC"
#define cci_uninitialized_object_id "NEEDS_SYNC"

struct cci_identifier_d cci_identifier_uninitialized_d = {
    cci_uninitialized_server_id,
    cci_uninitialized_object_id
};
const cci_identifier_t cci_identifier_uninitialized = &cci_identifier_uninitialized_d;

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_new_uuid (cci_uuid_string_t *out_uuid_string)
{
    return cci_os_identifier_new_uuid (out_uuid_string);
}

/* ------------------------------------------------------------------------ */

static cc_int32 cci_identifier_alloc (cci_identifier_t *out_identifier,
                                      cci_uuid_string_t in_server_id,
                                      cci_uuid_string_t in_object_id)
{
    cc_int32 err = ccNoError;
    cci_identifier_t identifier = NULL;

    if (!out_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!in_server_id  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_object_id  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        identifier = malloc (sizeof (*identifier));
        if (identifier) {
            *identifier = cci_identifier_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        identifier->server_id = strdup (in_server_id);
        if (!identifier->server_id) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        identifier->object_id = strdup (in_object_id);
        if (!identifier->object_id) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        *out_identifier = identifier;
        identifier = NULL; /* take ownership */
    }

    cci_identifier_release (identifier);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_new (cci_identifier_t *out_identifier,
                             cci_uuid_string_t in_server_id)
{
    cc_int32 err = ccNoError;
    cci_uuid_string_t object_id = NULL;

    if (!out_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!in_server_id  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_os_identifier_new_uuid (&object_id);
    }

    if (!err) {
        err = cci_identifier_alloc (out_identifier, in_server_id, object_id);
    }

    if (object_id) { free (object_id); }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_copy (cci_identifier_t *out_identifier,
                              cci_identifier_t  in_identifier)
{
    cc_int32 err = ccNoError;

    if (!out_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_alloc (out_identifier,
                                    in_identifier->server_id,
                                    in_identifier->object_id);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_release (cci_identifier_t in_identifier)
{
    cc_int32 err = ccNoError;

    /* Do not free the static "uninitialized" identifier */
    if (!err && in_identifier && in_identifier != cci_identifier_uninitialized) {
        free (in_identifier->server_id);
        free (in_identifier->object_id);
        free (in_identifier);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_compare (cci_identifier_t  in_identifier,
                                 cci_identifier_t  in_compare_to_identifier,
                                 cc_uint32        *out_equal)
{
    cc_int32 err = ccNoError;

    if (!in_identifier           ) { err = cci_check_error (ccErrBadParam); }
    if (!in_compare_to_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal               ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_equal = (!strcmp (in_identifier->object_id,
                               in_compare_to_identifier->object_id) &&
                      !strcmp (in_identifier->server_id,
                               in_compare_to_identifier->server_id));
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_is_for_server (cci_identifier_t  in_identifier,
                                       cci_uuid_string_t in_server_id,
                                       cc_uint32        *out_is_for_server)
{
    cc_int32 err = ccNoError;

    if (!in_identifier    ) { err = cci_check_error (ccErrBadParam); }
    if (!in_server_id     ) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_for_server) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_is_for_server = (!strcmp (in_identifier->server_id, in_server_id) ||
                              !strcmp (in_identifier->server_id, cci_uninitialized_server_id));
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_compare_server_id (cci_identifier_t  in_identifier,
                                           cci_identifier_t  in_compare_to_identifier,
                                           cc_uint32        *out_equal_server_id)
{
    cc_int32 err = ccNoError;

    if (!in_identifier           ) { err = cci_check_error (ccErrBadParam); }
    if (!in_compare_to_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal_server_id     ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_equal_server_id = (!strcmp (in_identifier->server_id,
                                         in_compare_to_identifier->server_id));
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_identifier_is_initialized (cci_identifier_t  in_identifier,
                                        cc_uint32        *out_is_initialized)
{
    cc_int32 err = ccNoError;

    if (!in_identifier     ) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_initialized) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_is_initialized = (strcmp (in_identifier->server_id,
                                       cci_uninitialized_server_id) != 0);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_uint32 cci_identifier_read (cci_identifier_t *out_identifier,
                               k5_ipc_stream      io_stream)
{
    cc_int32 err = ccNoError;
    cci_uuid_string_t server_id = NULL;
    cci_uuid_string_t object_id = NULL;

    if (!out_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!io_stream     ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_string (io_stream, &server_id);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (io_stream, &object_id);
    }

    if (!err) {
        err = cci_identifier_alloc (out_identifier, server_id, object_id);
    }

    krb5int_ipc_stream_free_string (server_id);
    krb5int_ipc_stream_free_string (object_id);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_uint32 cci_identifier_write (cci_identifier_t in_identifier,
                                k5_ipc_stream     io_stream)
{
    cc_int32 err = ccNoError;

    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!io_stream    ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_string (io_stream, in_identifier->server_id);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (io_stream, in_identifier->object_id);
    }

    return cci_check_error (err);
}
