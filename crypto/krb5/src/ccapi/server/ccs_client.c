/* ccapi/server/ccs_client.c */
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

struct ccs_client_d {
    ccs_pipe_t client_pipe;

    /* The following arrays do not own their contents */
    ccs_callbackref_array_t callbacks;
    ccs_iteratorref_array_t iterators;
};

struct ccs_client_d ccs_client_initializer = { CCS_PIPE_NULL, NULL, NULL };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_new (ccs_client_t *out_client,
                         ccs_pipe_t    in_client_pipe)
{
    cc_int32 err = ccNoError;
    ccs_client_t client = NULL;

    if (!out_client                     ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        client = malloc (sizeof (*client));
        if (client) {
            *client = ccs_client_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = ccs_callbackref_array_new (&client->callbacks);
    }

    if (!err) {
        err = ccs_iteratorref_array_new (&client->iterators);
    }

    if (!err) {
	err = ccs_pipe_copy (&client->client_pipe, in_client_pipe);
    }

    if (!err) {
        *out_client = client;
        client = NULL;
    }

    ccs_client_release (client);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_release (ccs_client_t io_client)
{
    cc_int32 err = ccNoError;

    if (!err && io_client) {
	cc_uint64 i;
	cc_uint64 callback_count = ccs_callbackref_array_count (io_client->callbacks);
	cc_uint64 iterator_count = ccs_iteratorref_array_count (io_client->iterators);

	for (i = 0; !err && i < callback_count; i++) {
	    ccs_callback_t callback = ccs_callbackref_array_object_at_index (io_client->callbacks, i);

	    cci_debug_printf ("%s: Invalidating callback reference %p.",
                              __FUNCTION__, callback);
	    ccs_callback_invalidate (callback);
	}

	for (i = 0; !err && i < iterator_count; i++) {
	    ccs_generic_list_iterator_t iterator = ccs_iteratorref_array_object_at_index (io_client->iterators, i);

	    cci_debug_printf ("%s: Invalidating iterator reference %p.",
                              __FUNCTION__, iterator);
	    ccs_generic_list_iterator_invalidate (iterator);
	}

	ccs_callbackref_array_release (io_client->callbacks);
	ccs_iteratorref_array_release (io_client->iterators);
	ccs_pipe_release (io_client->client_pipe);
	free (io_client);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_add_callback (ccs_client_t   io_client,
				  ccs_callback_t in_callback)
{
    cc_int32 err = ccNoError;

    if (!io_client  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_callback) { err = cci_check_error (ccErrBadParam); }

     if (!err) {
        err = ccs_callbackref_array_insert (io_client->callbacks, in_callback,
					    ccs_callbackref_array_count (io_client->callbacks));
     }

    return cci_check_error (err);
}


/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_remove_callback (ccs_client_t   io_client,
				     ccs_callback_t in_callback)
{
    cc_int32 err = ccNoError;
    cc_uint32 found_callback = 0;

    if (!io_client) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cc_uint64 i;
        cc_uint64 lock_count = ccs_callbackref_array_count (io_client->callbacks);

        for (i = 0; !err && i < lock_count; i++) {
            ccs_callback_t callback = ccs_callbackref_array_object_at_index (io_client->callbacks, i);

            if (callback == in_callback) {
		cci_debug_printf ("%s: Removing callback reference %p.", __FUNCTION__, callback);
		found_callback = 1;
		err = ccs_callbackref_array_remove (io_client->callbacks, i);
		break;
            }
        }
    }

    if (!err && !found_callback) {
        cci_debug_printf ("%s: WARNING! callback not found.", __FUNCTION__);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_add_iterator (ccs_client_t                io_client,
				  ccs_generic_list_iterator_t in_iterator)
{
    cc_int32 err = ccNoError;

    if (!io_client  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_iteratorref_array_insert (io_client->iterators, in_iterator,
                                            ccs_iteratorref_array_count (io_client->iterators));
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_remove_iterator (ccs_client_t                io_client,
				     ccs_generic_list_iterator_t in_iterator)
{
    cc_int32 err = ccNoError;
    cc_uint32 found_iterator = 0;

    if (!io_client) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cc_uint64 i;
        cc_uint64 lock_count = ccs_iteratorref_array_count (io_client->iterators);

        for (i = 0; !err && i < lock_count; i++) {
            ccs_generic_list_iterator_t iterator = ccs_iteratorref_array_object_at_index (io_client->iterators, i);

            if (iterator == in_iterator) {
		cci_debug_printf ("%s: Removing iterator reference %p.", __FUNCTION__, iterator);
		found_iterator = 1;
		err = ccs_iteratorref_array_remove (io_client->iterators, i);
		break;
            }
        }
    }

    if (!err && !found_iterator) {
        cci_debug_printf ("%s: WARNING! iterator not found.", __FUNCTION__);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_client_uses_pipe (ccs_client_t  in_client,
                               ccs_pipe_t    in_pipe,
                               cc_uint32    *out_uses_pipe)
{
    cc_int32 err = ccNoError;

    if (!in_client    ) { err = cci_check_error (ccErrBadParam); }
    if (!in_pipe      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_uses_pipe) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_pipe_compare (in_client->client_pipe, in_pipe, out_uses_pipe);
    }

    return cci_check_error (err);
}
