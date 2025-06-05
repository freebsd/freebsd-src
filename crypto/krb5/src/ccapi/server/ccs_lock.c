/* ccapi/server/ccs_lock.c */
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

struct ccs_lock_d {
    cc_uint32 type;
    ccs_lock_state_t lock_state_owner;
    ccs_callback_t callback;
};

struct ccs_lock_d ccs_lock_initializer = { 0, NULL, NULL };

static cc_int32 ccs_lock_invalidate_callback (ccs_callback_owner_t io_lock,
					      ccs_callback_t       in_callback);

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_new (ccs_lock_t       *out_lock,
                       cc_uint32         in_type,
                       cc_int32          in_invalid_object_err,
                       ccs_pipe_t        in_client_pipe,
                       ccs_pipe_t        in_reply_pipe,
                       ccs_lock_state_t  in_lock_state_owner)
{
    cc_int32 err = ccNoError;
    ccs_lock_t lock = NULL;

    if (!out_lock                       ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_reply_pipe) ) { err = cci_check_error (ccErrBadParam); }
    if (!in_lock_state_owner            ) { err = cci_check_error (ccErrBadParam); }

    if (in_type != cc_lock_read &&
        in_type != cc_lock_write &&
        in_type != cc_lock_upgrade &&
        in_type != cc_lock_downgrade) {
        err = cci_check_error (ccErrBadLockType);
    }

    if (!err) {
        lock = malloc (sizeof (*lock));
        if (lock) {
            *lock = ccs_lock_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        lock->type = in_type;
        lock->lock_state_owner = in_lock_state_owner;

        err = ccs_callback_new (&lock->callback,
				in_invalid_object_err,
				in_client_pipe,
				in_reply_pipe,
				(ccs_callback_owner_t) lock,
				ccs_lock_invalidate_callback);
    }

    if (!err) {
        *out_lock = lock;
        lock = NULL;
    }

    ccs_lock_release (lock);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_release (ccs_lock_t io_lock)
{
    cc_int32 err = ccNoError;

    if (!err && io_lock) {
	ccs_callback_release (io_lock->callback);
        free (io_lock);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_lock_invalidate_callback (ccs_callback_owner_t io_lock,
					      ccs_callback_t       in_callback)
{
    cc_int32 err = ccNoError;

    if (!io_lock    ) { err = cci_check_error (ccErrBadParam); }
    if (!in_callback) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
	ccs_lock_t lock = (ccs_lock_t) io_lock;

	err = ccs_lock_state_invalidate_lock (lock->lock_state_owner, lock);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_grant_lock (ccs_lock_t io_lock)
{
    cc_int32 err = ccNoError;

    if (!io_lock) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
	err = ccs_callback_reply_to_client (io_lock->callback, NULL);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_uint32 ccs_lock_is_pending (ccs_lock_t  in_lock,
                               cc_uint32  *out_pending)
{
    cc_int32 err = ccNoError;

    if (!in_lock    ) { err = cci_check_error (ccErrBadParam); }
    if (!out_pending) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
	err = ccs_callback_is_pending (in_lock->callback, out_pending);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_type (ccs_lock_t  in_lock,
                        cc_uint32  *out_lock_type)
{
    cc_int32 err = ccNoError;

    if (!in_lock      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_lock_type) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_lock_type = in_lock->type;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_is_read_lock (ccs_lock_t  in_lock,
                                cc_uint32  *out_is_read_lock)
{
    cc_int32 err = ccNoError;

    if (!in_lock         ) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_read_lock) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_is_read_lock = (in_lock->type == cc_lock_read ||
                             in_lock->type == cc_lock_downgrade);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_is_write_lock (ccs_lock_t  in_lock,
                                 cc_uint32  *out_is_write_lock)
{
    cc_int32 err = ccNoError;

    if (!in_lock          ) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_write_lock) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_is_write_lock = (in_lock->type == cc_lock_write ||
                              in_lock->type == cc_lock_upgrade);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_is_for_client_pipe (ccs_lock_t     in_lock,
                                      ccs_pipe_t     in_client_pipe,
                                      cc_uint32     *out_is_for_client_pipe)
{
    cc_int32 err = ccNoError;

    if (!in_lock                        ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_for_client_pipe         ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
	err = ccs_callback_is_for_client_pipe (in_lock->callback, in_client_pipe,
					       out_is_for_client_pipe);
    }

    return cci_check_error (err);
}


/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_client_pipe (ccs_lock_t  in_lock,
                               ccs_pipe_t *out_client_pipe)
{
    cc_int32 err = ccNoError;

    if (!in_lock        ) { err = cci_check_error (ccErrBadParam); }
    if (!out_client_pipe) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
	err = ccs_callback_client_pipe (in_lock->callback, out_client_pipe);
    }

    return cci_check_error (err);
}
