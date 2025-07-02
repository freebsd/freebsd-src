/* ccapi/server/ccs_lock_state.c */
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

struct ccs_lock_state_d {
    cc_int32 invalid_object_err;
    cc_int32 pending_lock_err;
    cc_int32 no_lock_err;
    ccs_lock_array_t locks;
    cc_uint64 first_pending_lock_index;
};

struct ccs_lock_state_d ccs_lock_state_initializer = { 1, 1, 1, NULL, 0 };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_state_new (ccs_lock_state_t *out_lock_state,
                             cc_int32          in_invalid_object_err,
                             cc_int32          in_pending_lock_err,
                             cc_int32          in_no_lock_err)
{
    cc_int32 err = ccNoError;
    ccs_lock_state_t lock_state = NULL;

    if (!out_lock_state) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        lock_state = malloc (sizeof (*lock_state));
        if (lock_state) {
            *lock_state = ccs_lock_state_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = ccs_lock_array_new (&lock_state->locks);
    }

    if (!err) {
        lock_state->invalid_object_err = in_invalid_object_err;
        lock_state->pending_lock_err = in_pending_lock_err;
        lock_state->no_lock_err = in_no_lock_err;

        *out_lock_state = lock_state;
        lock_state = NULL;
    }

    ccs_lock_state_release (lock_state);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_state_release (ccs_lock_state_t io_lock_state)
{
    cc_int32 err = ccNoError;

    if (!err && io_lock_state) {
        ccs_lock_array_release (io_lock_state->locks);
        free (io_lock_state);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_lock_status_add_pending_lock (ccs_lock_state_t  io_lock_state,
                                                  ccs_pipe_t        in_client_pipe,
                                                  ccs_pipe_t        in_reply_pipe,
                                                  cc_uint32         in_lock_type,
                                                  cc_uint64        *out_lock_index)
{
    cc_int32 err = ccNoError;
    ccs_lock_t lock = NULL;

    if (!io_lock_state                  ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_reply_pipe) ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_lock_new (&lock, in_lock_type,
                            io_lock_state->invalid_object_err,
                            in_client_pipe, in_reply_pipe,
                            io_lock_state);
    }

    if (!err) {
        err = ccs_lock_array_insert (io_lock_state->locks, lock,
                                     ccs_lock_array_count (io_lock_state->locks));
        if (!err) { lock = NULL; /* take ownership */ }
    }

    if (!err) {
        *out_lock_index = ccs_lock_array_count (io_lock_state->locks) - 1;
    }

    ccs_lock_release (lock);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_lock_status_remove_lock (ccs_lock_state_t io_lock_state,
                                             cc_uint64        in_lock_index)
{
    cc_int32 err = ccNoError;

    if (!io_lock_state) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_lock_array_remove (io_lock_state->locks, in_lock_index);

        if (!err && in_lock_index < io_lock_state->first_pending_lock_index) {
            io_lock_state->first_pending_lock_index--;
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_lock_status_grant_lock (ccs_lock_state_t io_lock_state,
                                            cc_uint64        in_pending_lock_index)
{
    cc_int32 err = ccNoError;
    ccs_lock_t pending_lock = NULL;
    cc_uint32 type = 0;

    if (!io_lock_state) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        pending_lock = ccs_lock_array_object_at_index (io_lock_state->locks,
                                                       in_pending_lock_index);
        if (!pending_lock || in_pending_lock_index < io_lock_state->first_pending_lock_index) {
            err = cci_check_error (ccErrBadParam);
        }
    }

    if (!err) {
        err = ccs_lock_type (pending_lock, &type);
    }

    if (!err && (type == cc_lock_upgrade || type == cc_lock_downgrade)) {
        /* lock upgrades or downgrades.  Find the old lock and remove it. */
        ccs_pipe_t pending_client_pipe = CCS_PIPE_NULL;

        err = ccs_lock_client_pipe (pending_lock, &pending_client_pipe);

        if (!err) {
            cc_uint64 i;

            for (i = 0; !err && i < io_lock_state->first_pending_lock_index; i++) {
                ccs_lock_t lock = ccs_lock_array_object_at_index (io_lock_state->locks, i);
                cc_uint32 is_lock_for_client = 0;

                err = ccs_lock_is_for_client_pipe (lock, pending_client_pipe, &is_lock_for_client);

                if (!err && is_lock_for_client) {
                    cci_debug_printf ("%s: Removing old lock %p at index %d to replace with pending lock %p.",
                                      __FUNCTION__, lock, (int) i, pending_lock);
                    err = ccs_lock_status_remove_lock (io_lock_state, i);
                    if (!err) { i--; in_pending_lock_index--; /* We removed one so back up an index */ }
                    break;
                }
            }
        }
    }

    if (!err) {
        cc_uint64 new_lock_index = 0;

        err = ccs_lock_array_move (io_lock_state->locks,
                                   in_pending_lock_index,
                                   io_lock_state->first_pending_lock_index,
                                   &new_lock_index);
        if (!err) { io_lock_state->first_pending_lock_index++; }
    }

    if (!err) {
        err = ccs_lock_grant_lock (pending_lock);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_lock_state_check_pending_lock (ccs_lock_state_t  io_lock_state,
                                                   ccs_pipe_t        in_pending_lock_client_pipe,
                                                   cc_uint32         in_pending_lock_type,
                                                   cc_uint32        *out_grant_lock)
{
    cc_int32 err = ccNoError;
    cc_uint32 is_write_locked = 0;
    cc_uint32 client_has_lock = 0;
    cc_uint32 other_clients_have_locks = 0;
    cc_uint32 client_lock_type = 0;
    cc_uint64 client_lock_index = 0;
    cc_uint32 grant_lock = 0;

    if (!io_lock_state                               ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_pending_lock_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!out_grant_lock                              ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cc_uint64 i;
        cc_uint64 lock_count = io_lock_state->first_pending_lock_index;

        for (i = 0; !err && i < lock_count; i++) {
            ccs_lock_t lock = ccs_lock_array_object_at_index (io_lock_state->locks, i);
            cc_uint32 lock_type = 0;
            cc_uint32 lock_is_for_client = 0;

            err = ccs_lock_type (lock, &lock_type);

            if (!err) {
                err = ccs_lock_is_for_client_pipe (lock, in_pending_lock_client_pipe,
                                                   &lock_is_for_client);
            }

            if (!err) {
                if (lock_type == cc_lock_write || lock_type == cc_lock_upgrade) {
                    is_write_locked = 1;
                }

                if (!lock_is_for_client) {
                    other_clients_have_locks = 1;

                } else if (!client_has_lock) { /* only record type of 1st lock */
                    client_has_lock = 1;
                    client_lock_type = lock_type;
                    client_lock_index = i;
                }
            }
        }
    }

    if (!err) {
        cc_uint64 lock_count = io_lock_state->first_pending_lock_index;

        if (in_pending_lock_type == cc_lock_write) {
            if (client_has_lock) {
                err = cci_check_error (ccErrBadLockType);
            } else {
                grant_lock = (lock_count == 0);
            }

        } else if (in_pending_lock_type == cc_lock_read) {
            if (client_has_lock) {
                err = cci_check_error (ccErrBadLockType);
            } else {
                grant_lock = !is_write_locked;
            }

        } else if (in_pending_lock_type == cc_lock_upgrade) {
            if (!client_has_lock || (client_lock_type != cc_lock_read &&
                                     client_lock_type != cc_lock_downgrade)) {
                err = cci_check_error (ccErrBadLockType);
            } else {
                /* don't grant if other clients have read locks */
                grant_lock = !other_clients_have_locks;
            }

        } else if (in_pending_lock_type == cc_lock_downgrade) {
            if (!client_has_lock || (client_lock_type != cc_lock_write &&
                                     client_lock_type != cc_lock_upgrade)) {
                err = cci_check_error (ccErrBadLockType);
            } else {
                /* downgrades can never block */
                grant_lock = 1;
            }
        } else {
            err = cci_check_error (ccErrBadLockType);
        }
    }

    if (!err) {
        *out_grant_lock = grant_lock;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_lock_status_try_to_grant_pending_locks (ccs_lock_state_t io_lock_state)
{
    cc_int32 err = ccNoError;
    cc_uint32 done = 0;

    if (!io_lock_state) { err = cci_check_error (ccErrBadParam); }

    /* Look at the pending locks and see if we can grant them.
     * Note that downgrade locks mean we must check all pending locks each pass
     * since a downgrade lock might be last in the list. */

    while (!err && !done) {
        cc_uint64 i;
        cc_uint64 count = ccs_lock_array_count (io_lock_state->locks);
        cc_uint32 granted_lock = 0;

        for (i = io_lock_state->first_pending_lock_index; !err && i < count; i++) {
            ccs_lock_t lock = ccs_lock_array_object_at_index (io_lock_state->locks, i);
            cc_uint32 lock_type = 0;
            ccs_pipe_t client_pipe = CCS_PIPE_NULL;
            cc_uint32 can_grant_lock_now = 0;

            err = ccs_lock_client_pipe (lock, &client_pipe);

            if (!err) {
                err = ccs_lock_type (lock, &lock_type);
            }

            if (!err) {
                err = ccs_lock_state_check_pending_lock (io_lock_state, client_pipe,
                                                         lock_type, &can_grant_lock_now);
            }

            if (!err && can_grant_lock_now) {
                err = ccs_lock_status_grant_lock (io_lock_state, i);
                if (!err) { granted_lock = 1; }
            }
        }

        if (!err && !granted_lock) {
            /* we walked over all the locks and couldn't grant any of them */
            done = 1;
        }
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_state_add (ccs_lock_state_t  io_lock_state,
                             ccs_pipe_t        in_client_pipe,
                             ccs_pipe_t        in_reply_pipe,
                             cc_uint32         in_lock_type,
                             cc_uint32         in_block,
                             cc_uint32        *out_will_send_reply)
{
    cc_int32 err = ccNoError;
    cc_uint32 can_grant_lock_now = 0;

    if (!io_lock_state                  ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_reply_pipe) ) { err = cci_check_error (ccErrBadParam); }
    if (!out_will_send_reply            ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        /* Sanity check: if there are any pending locks for this client
         * the client must have timed out waiting for our reply.  Remove any
         * existing pending locks for the client. */
        cc_uint64 i;

        for (i = io_lock_state->first_pending_lock_index; !err && i < ccs_lock_array_count (io_lock_state->locks); i++) {
            ccs_lock_t lock = ccs_lock_array_object_at_index (io_lock_state->locks, i);
            cc_uint32 has_pending_lock_for_client = 0;

            err = ccs_lock_is_for_client_pipe (lock, in_client_pipe, &has_pending_lock_for_client);

            if (!err && has_pending_lock_for_client) {
                cci_debug_printf ("WARNING %s: Removing unexpected pending lock %p at index %d.",
                                  __FUNCTION__, lock, (int) i);
                err = ccs_lock_status_remove_lock (io_lock_state, i);
                if (!err) { i--;  /* We removed one so back up an index */ }
            }
        }
    }

    if (!err) {
        err = ccs_lock_state_check_pending_lock (io_lock_state, in_client_pipe,
                                                 in_lock_type, &can_grant_lock_now);
    }

    if (!err) {
        if (!can_grant_lock_now && (in_block == cc_lock_noblock)) {
            err = cci_check_error (io_lock_state->pending_lock_err);

        } else {
            cc_uint64 new_lock_index = 0;

            err = ccs_lock_status_add_pending_lock (io_lock_state,
                                                    in_client_pipe,
                                                    in_reply_pipe,
                                                    in_lock_type,
                                                    &new_lock_index);

            if (!err && can_grant_lock_now) {
                err = ccs_lock_status_grant_lock (io_lock_state, new_lock_index);

                if (!err && (in_lock_type == cc_lock_downgrade)) {
                    /* downgrades can allow us to grant other locks */
                    err = ccs_lock_status_try_to_grant_pending_locks (io_lock_state);
                }
            }
        }
    }

    if (!err) {
        /* ccs_lock_state_add sends its replies via callback so caller shouldn't */
        *out_will_send_reply = 1;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_state_remove (ccs_lock_state_t io_lock_state,
                                ccs_pipe_t       in_client_pipe)
{
    cc_int32 err = ccNoError;
    cc_uint32 found_lock = 0;

    if (!io_lock_state                  ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cc_uint64 i;

        /* Remove all locks for this client.
         * There should only be one so warn if there are multiple */
        for (i = 0; !err && i < io_lock_state->first_pending_lock_index; i++) {
            ccs_lock_t lock = ccs_lock_array_object_at_index (io_lock_state->locks, i);
            cc_uint32 is_for_client = 0;

            err = ccs_lock_is_for_client_pipe (lock, in_client_pipe, &is_for_client);

            if (!err && is_for_client) {
                if (found_lock) {
                    cci_debug_printf ("WARNING %s: Found multiple locks for client.",
                                      __FUNCTION__);
                }

                found_lock = 1;

                cci_debug_printf ("%s: Removing lock %p at index %d.", __FUNCTION__, lock, (int) i);
                err = ccs_lock_status_remove_lock (io_lock_state, i);
                if (!err) { i--;  /* We removed one so back up an index */ }
            }
        }
    }

    if (!err && !found_lock) {
        err = cci_check_error (io_lock_state->no_lock_err);
    }

    if (!err) {
        err = ccs_lock_status_try_to_grant_pending_locks (io_lock_state);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_state_invalidate_lock (ccs_lock_state_t io_lock_state,
                                         ccs_lock_t       in_lock)
{
    cc_int32 err = ccNoError;

    if (!io_lock_state) { err = ccErrBadParam; }

    if (!err) {
        cc_uint64 i;
        cc_uint64 count = ccs_lock_array_count (io_lock_state->locks);

        for (i = 0; !err && i < count; i++) {
            ccs_lock_t lock = ccs_lock_array_object_at_index (io_lock_state->locks, i);

            if (lock == in_lock) {
                err = ccs_lock_status_remove_lock (io_lock_state, i);

                if (!err) {
                    err = ccs_lock_status_try_to_grant_pending_locks (io_lock_state);
                    break;
                }
            }
        }
    }

    return cci_check_error (err);
}
