/* ccapi/server/ccs_callback.c */
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

struct ccs_callback_d {
    cc_int32 pending;
    cc_int32 invalid_object_err;
    ccs_pipe_t client_pipe;
    ccs_pipe_t reply_pipe;
    ccs_callback_owner_t owner; /* pointer to owner */
    ccs_callback_owner_invalidate_t owner_invalidate;
};

struct ccs_callback_d ccs_callback_initializer = { 1, 1, CCS_PIPE_NULL, CCS_PIPE_NULL, NULL, NULL };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_callback_new (ccs_callback_t                  *out_callback,
			   cc_int32                         in_invalid_object_err,
			   ccs_pipe_t                       in_client_pipe,
			   ccs_pipe_t                       in_reply_pipe,
			   ccs_callback_owner_t             in_owner,
			   ccs_callback_owner_invalidate_t  in_owner_invalidate_function)
{
    cc_int32 err = ccNoError;
    ccs_callback_t callback = NULL;
    ccs_client_t client = NULL;

    if (!out_callback                   ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_reply_pipe) ) { err = cci_check_error (ccErrBadParam); }
    if (!in_owner                       ) { err = cci_check_error (ccErrBadParam); }
    if (!in_owner_invalidate_function   ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        callback = malloc (sizeof (*callback));
        if (callback) {
            *callback = ccs_callback_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = ccs_server_client_for_pipe (in_client_pipe, &client);
    }

    if (!err) {
	err = ccs_pipe_copy (&callback->client_pipe, in_client_pipe);
    }

    if (!err) {
	err = ccs_pipe_copy (&callback->reply_pipe, in_reply_pipe);
    }

    if (!err) {
        callback->client_pipe = in_client_pipe;
        callback->reply_pipe = in_reply_pipe;
        callback->invalid_object_err = in_invalid_object_err;
        callback->owner = in_owner;
        callback->owner_invalidate = in_owner_invalidate_function;

        err = ccs_client_add_callback (client, callback);
    }

    if (!err) {
        *out_callback = callback;
        callback = NULL;
    }

    ccs_callback_release (callback);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_callback_release (ccs_callback_t io_callback)
{
    cc_int32 err = ccNoError;

    if (!err && io_callback) {
	ccs_client_t client = NULL;

	if (io_callback->pending) {
	    err = ccs_server_send_reply (io_callback->reply_pipe,
					 io_callback->invalid_object_err, NULL);

	    io_callback->pending = 0;
	}

	if (!err) {
	    err = ccs_server_client_for_pipe (io_callback->client_pipe, &client);
	}

	if (!err && client) {
	    /* if client object still has a reference to us, remove it */
	    err = ccs_client_remove_callback (client, io_callback);
	}

	if (!err) {
	    ccs_pipe_release (io_callback->client_pipe);
	    ccs_pipe_release (io_callback->reply_pipe);
	    free (io_callback);
	}
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_callback_invalidate (ccs_callback_t io_callback)
{
    cc_int32 err = ccNoError;

    if (!io_callback) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        io_callback->pending = 0; /* client is dead, don't try to talk to it */
	if (io_callback->owner_invalidate) {
	    err = io_callback->owner_invalidate (io_callback->owner, io_callback);
	} else {
	    cci_debug_printf ("WARNING %s() unable to notify callback owner!",
			      __FUNCTION__);
	}
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_callback_reply_to_client (ccs_callback_t io_callback,
				       k5_ipc_stream   in_stream)
{
    cc_int32 err = ccNoError;

    if (!io_callback) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        if (io_callback->pending) {
	    cci_debug_printf ("%s: callback %p replying to client.", __FUNCTION__, io_callback);

            err = ccs_server_send_reply (io_callback->reply_pipe, err, in_stream);

            if (err) {
                cci_debug_printf ("WARNING %s() called on a lock belonging to a dead client!",
                                  __FUNCTION__);
            }

            io_callback->pending = 0;
        } else {
            cci_debug_printf ("WARNING %s() called on non-pending callback!",
                              __FUNCTION__);
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_uint32 ccs_callback_is_pending (ccs_callback_t  in_callback,
				   cc_uint32      *out_pending)
{
    cc_int32 err = ccNoError;

    if (!in_callback) { err = cci_check_error (ccErrBadParam); }
    if (!out_pending) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_pending = in_callback->pending;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_callback_is_for_client_pipe (ccs_callback_t  in_callback,
					  ccs_pipe_t      in_client_pipe,
					  cc_uint32      *out_is_for_client_pipe)
{
    cc_int32 err = ccNoError;

    if (!in_callback                    ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_for_client_pipe         ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_pipe_compare (in_callback->client_pipe, in_client_pipe,
                                out_is_for_client_pipe);
    }

    return cci_check_error (err);
}


/* ------------------------------------------------------------------------ */

cc_int32 ccs_callback_client_pipe (ccs_callback_t  in_callback,
				   ccs_pipe_t     *out_client_pipe)
{
    cc_int32 err = ccNoError;

    if (!in_callback    ) { err = cci_check_error (ccErrBadParam); }
    if (!out_client_pipe) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_client_pipe = in_callback->client_pipe;
    }

    return cci_check_error (err);
}
