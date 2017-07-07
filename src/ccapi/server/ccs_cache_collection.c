/* ccapi/server/ccs_cache_collection.c */
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

#include "k5-platform.h"	/* pull in asprintf decl/defn */
#include "ccs_common.h"
#include "ccs_os_notify.h"

struct ccs_cache_collection_d {
    cc_time_t last_changed_time;
    cc_uint64 next_unique_name;
    cci_identifier_t identifier;
    ccs_lock_state_t lock_state;
    ccs_ccache_list_t ccaches;
    ccs_callback_array_t change_callbacks;
};

struct ccs_cache_collection_d ccs_cache_collection_initializer = { 0, 0, NULL, NULL, NULL, NULL };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_new (ccs_cache_collection_t *out_cache_collection)
{
    cc_int32 err = ccNoError;
    ccs_cache_collection_t cache_collection = NULL;

    if (!out_cache_collection) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cache_collection = malloc (sizeof (*cache_collection));
        if (cache_collection) {
            *cache_collection = ccs_cache_collection_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = ccs_server_new_identifier (&cache_collection->identifier);
    }

    if (!err) {
        err = ccs_lock_state_new (&cache_collection->lock_state,
                                  ccErrInvalidContext,
                                  ccErrContextLocked,
                                  ccErrContextUnlocked);
    }

    if (!err) {
        err = ccs_ccache_list_new (&cache_collection->ccaches);
    }

    if (!err) {
        err = ccs_callback_array_new (&cache_collection->change_callbacks);
    }

    if (!err) {
        err = ccs_cache_collection_changed (cache_collection);
    }

    if (!err) {
        *out_cache_collection = cache_collection;
        cache_collection = NULL;
    }

    ccs_cache_collection_release (cache_collection);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_release (ccs_cache_collection_t io_cache_collection)
{
    cc_int32 err = ccNoError;

    if (!err && io_cache_collection) {
        cci_identifier_release (io_cache_collection->identifier);
        ccs_lock_state_release (io_cache_collection->lock_state);
        ccs_ccache_list_release (io_cache_collection->ccaches);
        ccs_callback_array_release (io_cache_collection->change_callbacks);
        free (io_cache_collection);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_compare_identifier (ccs_cache_collection_t  in_cache_collection,
                                                  cci_identifier_t        in_identifier,
                                                  cc_uint32              *out_equal)
{
    cc_int32 err = ccNoError;

    if (!in_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal          ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_compare (in_cache_collection->identifier,
                                      in_identifier,
                                      out_equal);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_changed (ccs_cache_collection_t io_cache_collection)
{
    cc_int32 err = ccNoError;
    k5_ipc_stream reply_data = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cc_time_t now = time (NULL);

        if (io_cache_collection->last_changed_time < now) {
            io_cache_collection->last_changed_time = now;
        } else {
            io_cache_collection->last_changed_time++;
        }
    }

    if (!err) {
        err = krb5int_ipc_stream_new (&reply_data);
    }

    if (!err) {
	err = krb5int_ipc_stream_write_time (reply_data, io_cache_collection->last_changed_time);
    }

    if (!err) {
	/* Loop over callbacks sending messages to them */
	cc_uint64 i;
        cc_uint64 count = ccs_callback_array_count (io_cache_collection->change_callbacks);

        for (i = 0; !err && i < count; i++) {
            ccs_callback_t callback = ccs_callback_array_object_at_index (io_cache_collection->change_callbacks, i);

	    err = ccs_callback_reply_to_client (callback, reply_data);

	    if (!err) {
		cci_debug_printf ("%s: Removing callback reference %p.", __FUNCTION__, callback);
		err = ccs_callback_array_remove (io_cache_collection->change_callbacks, i);
		break;
	    }
        }
    }

    if (!err) {
        err = ccs_os_notify_cache_collection_changed (io_cache_collection);
    }

    krb5int_ipc_stream_release (reply_data);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_invalidate_change_callback (ccs_callback_owner_t io_cache_collection,
								 ccs_callback_t       in_callback)
{
    cc_int32 err = ccNoError;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_callback        ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
	/* Remove callback */
	ccs_cache_collection_t cache_collection = (ccs_cache_collection_t) io_cache_collection;
	cc_uint64 i;
        cc_uint64 count = ccs_callback_array_count (cache_collection->change_callbacks);

        for (i = 0; !err && i < count; i++) {
            ccs_callback_t callback = ccs_callback_array_object_at_index (cache_collection->change_callbacks, i);

	    if (callback == in_callback) {
		cci_debug_printf ("%s: Removing callback reference %p.", __FUNCTION__, callback);
		err = ccs_callback_array_remove (cache_collection->change_callbacks, i);
		break;
	    }
        }
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_find_ccache_by_name (ccs_cache_collection_t  in_cache_collection,
                                                          const char             *in_name,
                                                          ccs_ccache_t           *out_ccache)
{
    cc_int32 err = ccNoError;
    ccs_ccache_list_iterator_t iterator = NULL;

    if (!in_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_name            ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache         ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_ccache_list_new_iterator (in_cache_collection->ccaches,
                                            CCS_PIPE_NULL,
                                            &iterator);
    }

    while (!err) {
        ccs_ccache_t ccache = NULL;

        err = ccs_ccache_list_iterator_next (iterator, &ccache);

        if (!err) {
            cc_uint32 equal = 0;

            err = ccs_ccache_compare_name (ccache, in_name, &equal);

            if (!err && equal) {
                *out_ccache = ccache;
                break;
            }
        }
    }
    if (err == ccIteratorEnd) { err = ccErrCCacheNotFound; }

    if (iterator) { ccs_ccache_list_iterator_release (iterator); }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_find_ccache (ccs_cache_collection_t  in_cache_collection,
                                           cci_identifier_t        in_identifier,
                                           ccs_ccache_t           *out_ccache)
{
    cc_int32 err = ccNoError;

    if (!in_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache         ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_ccache_list_find (in_cache_collection->ccaches,
                                    in_identifier, out_ccache);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_collection_move_ccache (ccs_cache_collection_t io_cache_collection,
                                            cci_identifier_t       in_source_identifier,
                                            ccs_ccache_t           io_destination_ccache)
{
    cc_int32 err = ccNoError;
    ccs_ccache_t source_ccache = NULL;

    if (!io_cache_collection  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_source_identifier ) { err = cci_check_error (ccErrBadParam); }
    if (!io_destination_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_cache_collection_find_ccache (io_cache_collection,
                                                in_source_identifier,
                                                &source_ccache);
    }

    if (!err) {
        err = ccs_ccache_swap_contents (source_ccache,
					io_destination_ccache,
					io_cache_collection);
    }

    if (!err) {
        err = ccs_cache_collection_destroy_ccache (io_cache_collection,
                                                   in_source_identifier);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_destroy_ccache (ccs_cache_collection_t  io_cache_collection,
                                              cci_identifier_t        in_identifier)
{
    cc_int32 err = ccNoError;
    ccs_ccache_t ccache = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_cache_collection_find_ccache (io_cache_collection,
                                                in_identifier,
                                                &ccache);
    }

    if (!err) {
        /* Notify before deletion because after deletion the ccache
         * will no longer exist (and won't know about its clients) */
        err = ccs_ccache_changed (ccache, io_cache_collection);
    }

    if (!err) {
        err = ccs_ccache_list_remove (io_cache_collection->ccaches,
                                      in_identifier);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_find_ccache_iterator (ccs_cache_collection_t  in_cache_collection,
                                                    cci_identifier_t        in_identifier,
                                                    ccs_ccache_iterator_t  *out_ccache_iterator)
{
    cc_int32 err = ccNoError;

    if (!in_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_ccache_list_find_iterator (in_cache_collection->ccaches,
                                             in_identifier,
                                             out_ccache_iterator);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_find_credentials_iterator (ccs_cache_collection_t      in_cache_collection,
                                                         cci_identifier_t            in_identifier,
                                                         ccs_ccache_t               *out_ccache,
                                                         ccs_credentials_iterator_t *out_credentials_iterator)
{
    cc_int32 err = ccNoError;
    ccs_ccache_list_iterator_t iterator = NULL;

    if (!in_cache_collection     ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier           ) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_ccache_list_new_iterator (in_cache_collection->ccaches,
                                            CCS_PIPE_NULL,
                                            &iterator);
    }

    while (!err) {
        ccs_ccache_t ccache = NULL;

        err = ccs_ccache_list_iterator_next (iterator, &ccache);

        if (!err) {
            cc_int32 terr = ccs_ccache_find_credentials_iterator (ccache,
                                                                  in_identifier,
                                                                  out_credentials_iterator);
            if (!terr) {
                *out_ccache = ccache;
                break;
            }
        }
    }
    if (err == ccIteratorEnd) { err = cci_check_error (ccErrInvalidCredentialsIterator); }

    if (iterator) { ccs_ccache_list_iterator_release (iterator); }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_get_next_unique_ccache_name (ccs_cache_collection_t   io_cache_collection,
                                                                  char                   **out_name)
{
    cc_int32 err = ccNoError;
    cc_uint64 count = 0;
    char *name = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!out_name           ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_cache_collection_list_count (io_cache_collection->ccaches, &count);
    }

    if (!err) {
        if (count > 0) {
            while (!err) {
                int ret = asprintf (&name, "%lld", io_cache_collection->next_unique_name++);
                if (ret < 0 || !name) { err = cci_check_error (ccErrNoMem); }

                if (!err) {
                    ccs_ccache_t ccache = NULL;  /* temporary to hold ccache pointer */
                    err = ccs_cache_collection_find_ccache_by_name (io_cache_collection,
                                                                    name, &ccache);
                }

                if (err == ccErrCCacheNotFound) {
                    err = ccNoError;
                    break;   /* found a unique one */
                }
            }
        } else {
            name = strdup (k_cci_context_initial_ccache_name);
            if (!name) { err = cci_check_error (ccErrNoMem); }
        }
    }

    if (!err) {
        *out_name = name;
        name = NULL;
    }

    free (name);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_get_default_ccache (ccs_cache_collection_t  in_cache_collection,
                                                         ccs_ccache_t           *out_ccache)
{
    cc_int32 err = ccNoError;
    cc_uint64 count = 0;

    if (!in_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache         ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_ccache_list_count (in_cache_collection->ccaches, &count);
    }

    if (!err) {
        if (count > 0) {
            /* First ccache is the default */
            ccs_ccache_list_iterator_t iterator = NULL;

            err = ccs_ccache_list_new_iterator (in_cache_collection->ccaches,
                                                CCS_PIPE_NULL,
                                                &iterator);

            if (!err) {
                err = ccs_ccache_list_iterator_next (iterator, out_ccache);
            }

            ccs_ccache_list_iterator_release (iterator);

        } else {
            err = cci_check_error (ccErrCCacheNotFound);
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_set_default_ccache (ccs_cache_collection_t  io_cache_collection,
                                                  cci_identifier_t        in_identifier)
{
    cc_int32 err = ccNoError;
    ccs_ccache_t old_default = NULL;
    ccs_ccache_t new_default = NULL;
    cc_uint32 equal = 0;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_cache_collection_get_default_ccache (io_cache_collection,
                                                       &old_default);
    }

    if (!err) {
	err = ccs_ccache_compare_identifier (old_default, in_identifier, &equal);
    }


    if (!err && !equal) {
        err = ccs_ccache_list_push_front (io_cache_collection->ccaches,
                                          in_identifier);

	if (!err) {
	    err = ccs_ccache_notify_default_state_changed (old_default,
							   io_cache_collection,
							   0 /* no longer default */);
	}

	if (!err) {
	    err = ccs_cache_collection_get_default_ccache (io_cache_collection,
							   &new_default);
	}

	if (!err) {
	    err = ccs_ccache_notify_default_state_changed (new_default,
							   io_cache_collection,
							   1 /* now default */);
	}

	if (!err) {
	    err = ccs_cache_collection_changed (io_cache_collection);
	}
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#pragma mark -- IPC Messages --
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_sync (ccs_cache_collection_t io_cache_collection,
                                            k5_ipc_stream           in_request_data,
                                            k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_write (io_cache_collection->identifier, io_reply_data);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_get_change_time (ccs_cache_collection_t io_cache_collection,
                                                       k5_ipc_stream           in_request_data,
                                                       k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_reply_data, io_cache_collection->last_changed_time);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_wait_for_change (ccs_pipe_t              in_client_pipe,
						      ccs_pipe_t              in_reply_pipe,
						      ccs_cache_collection_t  io_cache_collection,
						      k5_ipc_stream            in_request_data,
						      k5_ipc_stream            io_reply_data,
						      cc_uint32              *out_will_block)
{
    cc_int32 err = ccNoError;
    cc_time_t last_wait_for_change_time = 0;
    cc_uint32 will_block = 0;

    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_reply_pipe )) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection            ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data                ) { err = cci_check_error (ccErrBadParam); }
    if (!out_will_block                 ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_time (in_request_data, &last_wait_for_change_time);
    }

    if (!err) {
	if (last_wait_for_change_time < io_cache_collection->last_changed_time) {
	    err = krb5int_ipc_stream_write_time (io_reply_data, io_cache_collection->last_changed_time);

	} else {
	    ccs_callback_t callback = NULL;

	    err = ccs_callback_new (&callback,
				    ccErrInvalidContext,
				    in_client_pipe,
				    in_reply_pipe,
				    (ccs_callback_owner_t) io_cache_collection,
				    ccs_cache_collection_invalidate_change_callback);

	    if (!err) {
		err = ccs_callback_array_insert (io_cache_collection->change_callbacks, callback,
						 ccs_callback_array_count (io_cache_collection->change_callbacks));
		if (!err) { callback = NULL; /* take ownership */ }

		will_block = 1;
	    }

	    ccs_callback_release (callback);
	}
    }

    if (!err) {
	*out_will_block = will_block;
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_get_default_ccache_name (ccs_cache_collection_t io_cache_collection,
                                                               k5_ipc_stream           in_request_data,
                                                               k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint64 count = 0;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_cache_collection_list_count (io_cache_collection->ccaches, &count);
    }

    if (!err) {
        if (count > 0) {
            ccs_ccache_t ccache = NULL;

            err = ccs_cache_collection_get_default_ccache (io_cache_collection, &ccache);

            if (!err) {
                err = ccs_ccache_write_name (ccache, io_reply_data);
            }
        } else {
            err = krb5int_ipc_stream_write_string (io_reply_data,
                                           k_cci_context_initial_ccache_name);
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_open_ccache (ccs_cache_collection_t io_cache_collection,
                                                   k5_ipc_stream           in_request_data,
                                                   k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    char *name = NULL;
    ccs_ccache_t ccache = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_string (in_request_data, &name);
    }

    if (!err) {
        err = ccs_cache_collection_find_ccache_by_name (io_cache_collection,
                                                        name, &ccache);
    }

    if (!err) {
        err = ccs_ccache_write (ccache, io_reply_data);
    }

    krb5int_ipc_stream_free_string (name);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_open_default_ccache (ccs_cache_collection_t io_cache_collection,
                                                           k5_ipc_stream           in_request_data,
                                                           k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    ccs_ccache_t ccache = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
         err = ccs_cache_collection_get_default_ccache (io_cache_collection,
                                                        &ccache);
    }

    if (!err) {
        err = ccs_ccache_write (ccache, io_reply_data);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_create_ccache (ccs_cache_collection_t io_cache_collection,
                                                    k5_ipc_stream           in_request_data,
                                                    k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    char *name = NULL;
    cc_uint32 cred_vers;
    char *principal = NULL;
    ccs_ccache_t ccache = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_string (in_request_data, &name);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &cred_vers);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (in_request_data, &principal);
    }

    if (!err) {
        cc_int32 terr = ccs_cache_collection_find_ccache_by_name (io_cache_collection,
                                                                  name,
                                                                  &ccache);

        if (!terr) {
            err = ccs_ccache_reset (ccache, io_cache_collection, cred_vers, principal);

        } else {
            err = ccs_ccache_new (&ccache, cred_vers, name, principal,
                                  io_cache_collection->ccaches);
        }
    }

    if (!err) {
        err = ccs_ccache_write (ccache, io_reply_data);
    }

    if (!err) {
        err = ccs_cache_collection_changed (io_cache_collection);
    }

    krb5int_ipc_stream_free_string (name);
    krb5int_ipc_stream_free_string (principal);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_create_default_ccache (ccs_cache_collection_t io_cache_collection,
                                                            k5_ipc_stream           in_request_data,
                                                            k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 cred_vers;
    char *principal = NULL;
    ccs_ccache_t ccache = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &cred_vers);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (in_request_data, &principal);
    }

    if (!err) {
        err = ccs_cache_collection_get_default_ccache (io_cache_collection,
                                                       &ccache);

        if (!err) {
            err = ccs_ccache_reset (ccache, io_cache_collection, cred_vers, principal);

        } else if (err == ccErrCCacheNotFound) {
            char *name = NULL;

            err = ccs_cache_collection_get_next_unique_ccache_name (io_cache_collection,
                                                                    &name);

            if (!err) {
                err = ccs_ccache_new (&ccache, cred_vers, name, principal,
                                      io_cache_collection->ccaches);
            }

            free (name);
        }
    }

    if (!err) {
        err = ccs_ccache_write (ccache, io_reply_data);
    }

    if (!err) {
        err = ccs_cache_collection_changed (io_cache_collection);
    }

    krb5int_ipc_stream_free_string (principal);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_create_new_ccache (ccs_cache_collection_t io_cache_collection,
                                                        k5_ipc_stream           in_request_data,
                                                        k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 cred_vers;
    char *principal = NULL;
    char *name = NULL;
    ccs_ccache_t ccache = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &cred_vers);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (in_request_data, &principal);
    }

    if (!err) {
        err = ccs_cache_collection_get_next_unique_ccache_name (io_cache_collection,
                                                                &name);
    }

    if (!err) {
        err = ccs_ccache_new (&ccache, cred_vers, name, principal,
                              io_cache_collection->ccaches);
    }

    if (!err) {
        err = ccs_ccache_write (ccache, io_reply_data);
    }

    if (!err) {
        err = ccs_cache_collection_changed (io_cache_collection);
    }

    free (name);
    krb5int_ipc_stream_free_string (principal);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static  cc_int32 ccs_cache_collection_new_ccache_iterator (ccs_cache_collection_t io_cache_collection,
                                                           ccs_pipe_t             in_client_pipe,
                                                           k5_ipc_stream           in_request_data,
                                                           k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    ccs_ccache_iterator_t ccache_iterator = NULL;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_ccache_list_new_iterator (io_cache_collection->ccaches,
                                            in_client_pipe,
                                            &ccache_iterator);
    }

    if (!err) {
        err = ccs_ccache_list_iterator_write (ccache_iterator, io_reply_data);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_lock (ccs_pipe_t              in_client_pipe,
                                           ccs_pipe_t              in_reply_pipe,
                                           ccs_cache_collection_t  io_cache_collection,
                                           k5_ipc_stream            in_request_data,
                                           cc_uint32              *out_will_block,
                                           k5_ipc_stream            io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 lock_type;
    cc_uint32 block;

    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection            ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data                ) { err = cci_check_error (ccErrBadParam); }
    if (!out_will_block                 ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data                  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &lock_type);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &block);
    }

    if (!err) {
        err = ccs_lock_state_add (io_cache_collection->lock_state,
                                  in_client_pipe, in_reply_pipe,
                                  lock_type, block, out_will_block);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_unlock (ccs_pipe_t             in_client_pipe,
                                             ccs_cache_collection_t io_cache_collection,
                                             k5_ipc_stream           in_request_data,
                                             k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection            ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data                ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data                  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_lock_state_remove (io_cache_collection->lock_state,
                                     in_client_pipe);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

 cc_int32 ccs_cache_collection_handle_message (ccs_pipe_t              in_client_pipe,
                                               ccs_pipe_t              in_reply_pipe,
                                               ccs_cache_collection_t  io_cache_collection,
                                               enum cci_msg_id_t       in_request_name,
                                               k5_ipc_stream            in_request_data,
                                               cc_uint32              *out_will_block,
                                               k5_ipc_stream           *out_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 will_block = 0;
    k5_ipc_stream reply_data = NULL;

    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_reply_pipe) ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection            ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data                ) { err = cci_check_error (ccErrBadParam); }
    if (!out_will_block                 ) { err = cci_check_error (ccErrBadParam); }
    if (!out_reply_data                 ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_new (&reply_data);
    }

    if (!err) {
        if (in_request_name == cci_context_unused_release_msg_id) {
            /* Old release message.  Do nothing. */

        } else if (in_request_name == cci_context_sync_msg_id) {
            err = ccs_cache_collection_sync (io_cache_collection,
                                             in_request_data, reply_data);

        } else if (in_request_name == cci_context_get_change_time_msg_id) {
            err = ccs_cache_collection_get_change_time (io_cache_collection,
                                                        in_request_data, reply_data);

        } else if (in_request_name == cci_context_wait_for_change_msg_id) {
            err = ccs_cache_collection_wait_for_change (in_client_pipe, in_reply_pipe,
							io_cache_collection,
                                                        in_request_data, reply_data,
							&will_block);

        } else if (in_request_name == cci_context_get_default_ccache_name_msg_id) {
            err = ccs_cache_collection_get_default_ccache_name (io_cache_collection,
                                                                in_request_data, reply_data);

        } else if (in_request_name == cci_context_open_ccache_msg_id) {
            err = ccs_cache_collection_open_ccache (io_cache_collection,
                                                    in_request_data, reply_data);

        } else if (in_request_name == cci_context_open_default_ccache_msg_id) {
            err = ccs_cache_collection_open_default_ccache (io_cache_collection,
                                                            in_request_data, reply_data);

        } else if (in_request_name == cci_context_create_ccache_msg_id) {
            err = ccs_cache_collection_create_ccache (io_cache_collection,
                                                      in_request_data, reply_data);

        } else if (in_request_name == cci_context_create_default_ccache_msg_id) {
            err = ccs_cache_collection_create_default_ccache (io_cache_collection,
                                                              in_request_data, reply_data);

        } else if (in_request_name == cci_context_create_new_ccache_msg_id) {
            err = ccs_cache_collection_create_new_ccache (io_cache_collection,
                                                          in_request_data, reply_data);

        } else if (in_request_name == cci_context_new_ccache_iterator_msg_id) {
            err = ccs_cache_collection_new_ccache_iterator (io_cache_collection,
                                                            in_client_pipe,
                                                            in_request_data,
                                                            reply_data);

        } else if (in_request_name == cci_context_lock_msg_id) {
            err = ccs_cache_collection_lock (in_client_pipe, in_reply_pipe,
                                             io_cache_collection,
                                             in_request_data,
                                             &will_block, reply_data);

        } else if (in_request_name == cci_context_unlock_msg_id) {
            err = ccs_cache_collection_unlock (in_client_pipe, io_cache_collection,
                                               in_request_data, reply_data);

        } else {
            err = ccErrBadInternalMessage;
        }
    }

    if (!err) {
        *out_will_block = will_block;
        if (!will_block) {
            *out_reply_data = reply_data;
            reply_data = NULL; /* take ownership */
        } else {
            *out_reply_data = NULL;
        }
    }

    krb5int_ipc_stream_release (reply_data);

    return cci_check_error (err);
}
