/* ccapi/common/cci_message.c */
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

/* ------------------------------------------------------------------------ */

cc_int32 cci_message_invalid_object_err (enum cci_msg_id_t in_request_name)
{
    cc_int32 err = ccNoError;

    if (in_request_name > cci_context_first_msg_id &&
        in_request_name < cci_context_last_msg_id) {
        err = ccErrInvalidContext;

    } else if (in_request_name > cci_ccache_first_msg_id &&
               in_request_name < cci_ccache_last_msg_id) {
        err = ccErrInvalidCCache;

    } else if (in_request_name > cci_ccache_iterator_first_msg_id &&
               in_request_name < cci_ccache_iterator_last_msg_id) {
        err = ccErrInvalidCCacheIterator;

    } else if (in_request_name > cci_credentials_iterator_first_msg_id &&
               in_request_name < cci_credentials_iterator_last_msg_id) {
        err = ccErrInvalidCredentialsIterator;

    } else {
        err = ccErrBadInternalMessage;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_message_new_request_header (k5_ipc_stream      *out_request,
                                         enum cci_msg_id_t  in_request_name,
                                         cci_identifier_t   in_identifier)
{
    cc_int32 err = ccNoError;
    k5_ipc_stream request = NULL;

    if (!out_request) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&request);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (request, in_request_name);
    }

    if (!err) {
        err = cci_identifier_write (in_identifier, request);
    }

    if (!err) {
        *out_request = request;
        request = NULL;
    }

    krb5int_ipc_stream_release (request);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_message_read_request_header (k5_ipc_stream       in_request,
                                          enum cci_msg_id_t *out_request_name,
                                          cci_identifier_t  *out_identifier)
{
    cc_int32 err = ccNoError;
    cc_uint32 request_name;
    cci_identifier_t identifier = NULL;

    if (!in_request      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_request_name) { err = cci_check_error (ccErrBadParam); }
    if (!out_identifier  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request, &request_name);
    }

    if (!err) {
        err = cci_identifier_read (&identifier, in_request);
    }

    if (!err) {
        *out_request_name = request_name;
        *out_identifier = identifier;
        identifier = NULL; /* take ownership */
    }

    cci_identifier_release (identifier);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_message_new_reply_header (k5_ipc_stream     *out_reply,
                                       cc_int32          in_error)
{
    cc_int32 err = ccNoError;
    k5_ipc_stream reply = NULL;

    if (!out_reply) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&reply);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_int32 (reply, in_error);
    }

    if (!err) {
        *out_reply = reply;
        reply = NULL;
    }

    krb5int_ipc_stream_release (reply);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_message_read_reply_header (k5_ipc_stream      in_reply,
                                        cc_int32         *out_reply_error)
{
    cc_int32 err = ccNoError;
    cc_int32 reply_err = 0;

    if (!in_reply       ) { err = cci_check_error (ccErrBadParam); }
    if (!out_reply_error) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_int32 (in_reply, &reply_err);
    }

    if (!err) {
        *out_reply_error = reply_err;
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_read_time (k5_ipc_stream  io_stream,
                                  cc_time_t     *out_time)
{
    int32_t err = 0;
    int64_t t = 0;

    if (!io_stream) { err = cci_check_error (ccErrBadParam); }
    if (!out_time ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_int64 (io_stream, &t);
    }

    if (!err) {
        *out_time = t;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_write_time (k5_ipc_stream io_stream,
					cc_time_t     in_time)
{
    int32_t err = 0;

    if (!io_stream) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_int64 (io_stream, in_time);
    }

    return cci_check_error (err);
}
