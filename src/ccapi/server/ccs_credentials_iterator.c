/* ccapi/server/ccs_credentials_iterator.c */
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

#include "ccs_common.h"

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_credentials_iterator_release (ccs_credentials_iterator_t io_credentials_iterator,
						  ccs_ccache_t               io_ccache,
						  k5_ipc_stream               in_request_data,
						  k5_ipc_stream               io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_credentials_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache              ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data        ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data          ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_credentials_list_iterator_release (io_credentials_iterator);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_credentials_iterator_next (ccs_credentials_iterator_t io_credentials_iterator,
					       ccs_ccache_t               io_ccache,
					       k5_ipc_stream               in_request_data,
					       k5_ipc_stream               io_reply_data)
{
    cc_int32 err = ccNoError;
    ccs_credentials_t credentials = NULL;

    if (!io_credentials_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache              ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data        ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data          ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_credentials_list_iterator_next (io_credentials_iterator,
                                                  &credentials);
    }

    if (!err) {
        err = ccs_credentials_write (credentials, io_reply_data);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static  cc_int32 ccs_credentials_iterator_clone (ccs_credentials_iterator_t io_credentials_iterator,
                                                 ccs_ccache_t               io_ccache,
                                                 k5_ipc_stream               in_request_data,
                                                 k5_ipc_stream               io_reply_data)
{
    cc_int32 err = ccNoError;
    ccs_credentials_iterator_t credentials_iterator = NULL;

    if (!io_credentials_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache              ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data        ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data          ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_credentials_list_iterator_clone (io_credentials_iterator,
                                                   &credentials_iterator);
    }

    if (!err) {
        err = ccs_credentials_list_iterator_write (credentials_iterator,
                                                   io_reply_data);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

 cc_int32 ccs_credentials_iterator_handle_message (ccs_credentials_iterator_t  io_credentials_iterator,
                                                   ccs_ccache_t                io_ccache,
                                                   enum cci_msg_id_t           in_request_name,
                                                   k5_ipc_stream                in_request_data,
                                                   k5_ipc_stream               *out_reply_data)
{
    cc_int32 err = ccNoError;
    k5_ipc_stream reply_data = NULL;

    if (!in_request_data) { err = cci_check_error (ccErrBadParam); }
    if (!out_reply_data ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&reply_data);
    }

    if (!err) {
        if (in_request_name == cci_credentials_iterator_release_msg_id) {
            err = ccs_credentials_iterator_release (io_credentials_iterator,
                                                    io_ccache,
                                                    in_request_data,
                                                    reply_data);

        } else if (in_request_name == cci_credentials_iterator_next_msg_id) {
            err = ccs_credentials_iterator_next (io_credentials_iterator,
                                                 io_ccache,
                                                 in_request_data,
                                                 reply_data);

        } else if (in_request_name == cci_credentials_iterator_clone_msg_id) {
            err = ccs_credentials_iterator_clone (io_credentials_iterator,
                                                  io_ccache,
                                                  in_request_data,
                                                  reply_data);

        } else {
            err = ccErrBadInternalMessage;
        }
    }

    if (!err) {
        *out_reply_data = reply_data;
        reply_data = NULL; /* take ownership */
    }

    krb5int_ipc_stream_release (reply_data);

    return cci_check_error (err);
}
