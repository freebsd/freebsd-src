/* ccapi/lib/ccapi_context_change_time.c */
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

#include "ccapi_context_change_time.h"
#include "cci_common.h"

#include "k5-thread.h"

static cci_identifier_t g_change_time_identifer = NULL;
static cc_time_t g_change_time = 0;
static cc_time_t g_change_time_offset = 0;
static k5_mutex_t g_change_time_mutex = K5_MUTEX_PARTIAL_INITIALIZER;

/* ------------------------------------------------------------------------ */

cc_int32 cci_context_change_time_thread_init (void)
{
    return k5_mutex_finish_init(&g_change_time_mutex);
}

/* ------------------------------------------------------------------------ */

void cci_context_change_time_thread_fini (void)
{
    k5_mutex_destroy(&g_change_time_mutex);
}

/* ------------------------------------------------------------------------ */
/* WARNING!  Mutex must be locked when calling this!                        */

static cc_int32 cci_context_change_time_update_identifier (cci_identifier_t  in_new_identifier,
                                                           cc_uint32        *out_server_ids_match,
                                                           cc_uint32        *out_old_server_running,
                                                           cc_uint32        *out_new_server_running)
{
    cc_int32 err = ccNoError;
    cc_uint32 server_ids_match = 0;
    cc_uint32 old_server_running = 0;
    cc_uint32 new_server_running = 0;

    if (!in_new_identifier) { err = cci_check_error (err); }

    if (!err && !g_change_time_identifer) {
       g_change_time_identifer = cci_identifier_uninitialized;
    }

    if (!err) {
        err = cci_identifier_compare_server_id (g_change_time_identifer,
                                                in_new_identifier,
                                                &server_ids_match);
    }

    if (!err && out_old_server_running) {
        err = cci_identifier_is_initialized (g_change_time_identifer, &old_server_running);
    }

    if (!err && out_new_server_running) {
        err = cci_identifier_is_initialized (in_new_identifier, &new_server_running);
    }

    if (!err && !server_ids_match) {
        cci_identifier_t new_change_time_identifer = NULL;

        err = cci_identifier_copy (&new_change_time_identifer, in_new_identifier);

        if (!err) {
            /* Save the new identifier */
            if (g_change_time_identifer) {
                cci_identifier_release (g_change_time_identifer);
            }
            g_change_time_identifer = new_change_time_identifer;
        }
    }

    if (!err) {
        if (out_server_ids_match  ) { *out_server_ids_match = server_ids_match; }
        if (out_old_server_running) { *out_old_server_running = old_server_running; }
        if (out_new_server_running) { *out_new_server_running = new_server_running; }
    }


    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 cci_context_change_time_get (cc_time_t *out_change_time)
{
    cc_int32 err = ccNoError;

    k5_mutex_lock (&g_change_time_mutex);

    *out_change_time = g_change_time + g_change_time_offset;
    k5_mutex_unlock (&g_change_time_mutex);

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_context_change_time_update (cci_identifier_t in_identifier,
                                         cc_time_t        in_new_change_time)
{
    cc_int32 err = ccNoError;
    k5_mutex_lock (&g_change_time_mutex);

    if (!in_identifier) { err = cci_check_error (err); }

    if (!err) {
        if (g_change_time < in_new_change_time) {
            /* Only update if it increases the time.  May be a different server. */
            g_change_time = in_new_change_time;
            cci_debug_printf ("%s: setting change time to %d",
                              __FUNCTION__, in_new_change_time);
        }
    }

    if (!err) {
        err = cci_context_change_time_update_identifier (in_identifier,
                                                         NULL, NULL, NULL);
    }

    k5_mutex_unlock (&g_change_time_mutex);

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_context_change_time_sync (cci_identifier_t in_new_identifier)
{
    cc_int32 err = ccNoError;
    cc_uint32 server_ids_match = 0;
    cc_uint32 server_was_running = 0;
    cc_uint32 server_is_running = 0;

    k5_mutex_lock (&g_change_time_mutex);

    if (!in_new_identifier) { err = cci_check_error (err); }

    if (!err) {
        err = cci_context_change_time_update_identifier (in_new_identifier,
                                                         &server_ids_match,
                                                         &server_was_running,
                                                         &server_is_running);
    }

    if (!err && !server_ids_match) {
        /* Increment the change time so callers re-read */
        g_change_time_offset++;

        /* If the server died, absorb the offset */
        if (server_was_running && !server_is_running) {
            cc_time_t now = time (NULL);

            g_change_time += g_change_time_offset;
            g_change_time_offset = 0;

            /* Make sure the change time increases, ideally with the current time */
            g_change_time = (g_change_time < now) ? now : g_change_time;
        }

        cci_debug_printf ("%s noticed server changed ("
                          "server_was_running = %d; server_is_running = %d; "
                          "g_change_time = %d; g_change_time_offset = %d",
                          __FUNCTION__, server_was_running, server_is_running,
                          g_change_time, g_change_time_offset);
    }

    k5_mutex_unlock (&g_change_time_mutex);

    return err;
}
