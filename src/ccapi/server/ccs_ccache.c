/* ccapi/server/ccs_ccache.c */
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
#include "ccs_os_notify.h"

struct ccs_ccache_d {
    cci_identifier_t identifier;
    ccs_lock_state_t lock_state;
    cc_uint32 creds_version;
    char *name;
    char *v4_principal;
    char *v5_principal;
    cc_time_t last_default_time;
    cc_time_t last_changed_time;
    cc_uint32 kdc_time_offset_v4_valid;
    cc_time_t kdc_time_offset_v4;
    cc_uint32 kdc_time_offset_v5_valid;
    cc_time_t kdc_time_offset_v5;
    ccs_credentials_list_t credentials;
    ccs_callback_array_t change_callbacks;
};

struct ccs_ccache_d ccs_ccache_initializer = { NULL, NULL, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, NULL, NULL };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_new (ccs_ccache_t      *out_ccache,
                         cc_uint32          in_creds_version,
                         const char        *in_name,
                         const char        *in_principal,
                         ccs_ccache_list_t  io_ccache_list)
{
    cc_int32 err = ccNoError;
    ccs_ccache_t ccache = NULL;

    if (!out_ccache  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_name     ) { err = cci_check_error (ccErrBadParam); }
    if (!in_principal) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        ccache = malloc (sizeof (*ccache));
        if (ccache) {
            *ccache = ccs_ccache_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = ccs_server_new_identifier (&ccache->identifier);
    }

    if (!err) {
        err = ccs_lock_state_new (&ccache->lock_state,
                                  ccErrInvalidCCache,
                                  ccErrCCacheLocked,
                                  ccErrCCacheUnlocked);
    }

    if (!err) {
        ccache->name = strdup (in_name);
        if (!ccache->name) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        ccache->creds_version = in_creds_version;

        if (ccache->creds_version == cc_credentials_v4) {
            ccache->v4_principal = strdup (in_principal);
            if (!ccache->v4_principal) { err = cci_check_error (ccErrNoMem); }

        } else if (ccache->creds_version == cc_credentials_v5) {
            ccache->v5_principal = strdup (in_principal);
            if (!ccache->v5_principal) { err = cci_check_error (ccErrNoMem); }

        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    if (!err) {
        err = ccs_credentials_list_new (&ccache->credentials);
    }

    if (!err) {
        err = ccs_callback_array_new (&ccache->change_callbacks);
    }

    if (!err) {
        cc_uint64 now = time (NULL);
        cc_uint64 count = 0;

        err = ccs_ccache_list_count (io_ccache_list, &count);

        if (!err) {
            /* first cache is default */
            ccache->last_default_time = (count == 0) ? now : 0;
	    cci_debug_printf ("%s ccache->last_default_time is %d.",
			     __FUNCTION__, ccache->last_default_time);
            ccache->last_changed_time = now;
        }
    }

    if (!err) {
        /* Add self to the list of ccaches */
        err = ccs_ccache_list_add (io_ccache_list, ccache);
    }

    if (!err) {
        *out_ccache = ccache;
        ccache = NULL;
    }

    ccs_ccache_release (ccache);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_reset (ccs_ccache_t            io_ccache,
			   ccs_cache_collection_t  io_cache_collection,
                           cc_uint32               in_creds_version,
                           const char             *in_principal)
{
    cc_int32 err = ccNoError;
    char *v4_principal = NULL;
    char *v5_principal = NULL;
    ccs_credentials_list_t credentials = NULL;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_principal       ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        io_ccache->creds_version = in_creds_version;

        if (io_ccache->creds_version == cc_credentials_v4) {
            v4_principal = strdup (in_principal);
            if (!v4_principal) { err = cci_check_error (ccErrNoMem); }

        } else if (io_ccache->creds_version == cc_credentials_v5) {
            v5_principal = strdup (in_principal);
            if (!v5_principal) { err = cci_check_error (ccErrNoMem); }

        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    if (!err) {
        err = ccs_credentials_list_new (&credentials);
    }

    if (!err) {
        io_ccache->kdc_time_offset_v4 = 0;
        io_ccache->kdc_time_offset_v4_valid = 0;
        io_ccache->kdc_time_offset_v5 = 0;
        io_ccache->kdc_time_offset_v5_valid = 0;

        if (io_ccache->v4_principal) { free (io_ccache->v4_principal); }
        io_ccache->v4_principal = v4_principal;
        v4_principal = NULL; /* take ownership */

        if (io_ccache->v5_principal) { free (io_ccache->v5_principal); }
        io_ccache->v5_principal = v5_principal;
        v5_principal = NULL; /* take ownership */

        ccs_credentials_list_release (io_ccache->credentials);
        io_ccache->credentials = credentials;
        credentials = NULL; /* take ownership */

	err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }

    free (v4_principal);
    free (v5_principal);
    ccs_credentials_list_release (credentials);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_swap_contents (ccs_ccache_t           io_source_ccache,
                                   ccs_ccache_t           io_destination_ccache,
				   ccs_cache_collection_t io_cache_collection)
{
    cc_int32 err = ccNoError;

    if (!io_source_ccache     ) { err = cci_check_error (ccErrBadParam); }
    if (!io_destination_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        struct ccs_ccache_d temp_ccache = *io_destination_ccache;

        /* swap everything */
        *io_destination_ccache = *io_source_ccache;
        *io_source_ccache = temp_ccache;

        /* swap back the name and identifier */
        io_source_ccache->identifier = io_destination_ccache->identifier;
        io_destination_ccache->identifier = temp_ccache.identifier;

        io_source_ccache->name = io_destination_ccache->name;
        io_destination_ccache->name = temp_ccache.name;
    }

    if (!err) {
        err = ccs_ccache_changed (io_source_ccache, io_cache_collection);
    }

    if (!err) {
        err = ccs_ccache_changed (io_destination_ccache, io_cache_collection);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_release (ccs_ccache_t io_ccache)
{
    cc_int32 err = ccNoError;

    if (!err && io_ccache) {
        cci_identifier_release (io_ccache->identifier);
        ccs_lock_state_release (io_ccache->lock_state);
        free (io_ccache->name);
        free (io_ccache->v4_principal);
        free (io_ccache->v5_principal);
        ccs_credentials_list_release (io_ccache->credentials);
        ccs_callback_array_release (io_ccache->change_callbacks);
        free (io_ccache);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_compare_identifier (ccs_ccache_t      in_ccache,
                                        cci_identifier_t  in_identifier,
                                        cc_uint32        *out_equal)
{
    cc_int32 err = ccNoError;

    if (!in_ccache    ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal    ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_compare (in_ccache->identifier,
                                      in_identifier,
                                      out_equal);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_compare_name (ccs_ccache_t  in_ccache,
                                  const char   *in_name,
                                  cc_uint32    *out_equal)
{
    cc_int32 err = ccNoError;

    if (!in_ccache) { err = cci_check_error (ccErrBadParam); }
    if (!in_name  ) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_equal = (strcmp (in_ccache->name, in_name) == 0);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_changed (ccs_ccache_t           io_ccache,
                             ccs_cache_collection_t io_cache_collection)
{
    cc_int32 err = ccNoError;
    k5_ipc_stream reply_data = NULL;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        cc_time_t now = time (NULL);

        if (io_ccache->last_changed_time < now) {
            io_ccache->last_changed_time = now;
        } else {
            io_ccache->last_changed_time++;
        }
    }

    if (!err) {
        err = ccs_cache_collection_changed (io_cache_collection);
    }

    if (!err) {
        err = krb5int_ipc_stream_new (&reply_data);
    }

    if (!err) {
	err = krb5int_ipc_stream_write_time (reply_data, io_ccache->last_changed_time);
    }

    if (!err) {
	/* Loop over callbacks sending messages to them */
	cc_uint64 i;
        cc_uint64 count = ccs_callback_array_count (io_ccache->change_callbacks);

        for (i = 0; !err && i < count; i++) {
            ccs_callback_t callback = ccs_callback_array_object_at_index (io_ccache->change_callbacks, i);

	    err = ccs_callback_reply_to_client (callback, reply_data);

	    if (!err) {
		cci_debug_printf ("%s: Removing callback reference %p.", __FUNCTION__, callback);
		err = ccs_callback_array_remove (io_ccache->change_callbacks, i);
		break;
	    }
        }
    }

    if (!err) {
        err = ccs_os_notify_ccache_changed (io_cache_collection,
                                            io_ccache->name);
    }

    krb5int_ipc_stream_release (reply_data);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_invalidate_change_callback (ccs_callback_owner_t io_ccache,
						       ccs_callback_t       in_callback)
{
    cc_int32 err = ccNoError;

    if (!io_ccache  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_callback) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
	/* Remove callback */
	ccs_ccache_t ccache = (ccs_ccache_t) io_ccache;
	cc_uint64 i;
        cc_uint64 count = ccs_callback_array_count (ccache->change_callbacks);

        for (i = 0; !err && i < count; i++) {
            ccs_callback_t callback = ccs_callback_array_object_at_index (ccache->change_callbacks, i);

	    if (callback == in_callback) {
		cci_debug_printf ("%s: Removing callback reference %p.", __FUNCTION__, callback);
		err = ccs_callback_array_remove (ccache->change_callbacks, i);
		break;
	    }
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_notify_default_state_changed (ccs_ccache_t           io_ccache,
                                                  ccs_cache_collection_t io_cache_collection,
                                                  cc_uint32              in_new_default_state)
{
    cc_int32 err = ccNoError;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }

    if (!err && in_new_default_state) {
        cc_time_t now = time (NULL);

        if (io_ccache->last_default_time < now) {
            io_ccache->last_default_time = now;
        } else {
            io_ccache->last_default_time++;
        }
    }

    if (!err) {
        err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_find_credentials_iterator (ccs_ccache_t                in_ccache,
                                               cci_identifier_t            in_identifier,
                                               ccs_credentials_iterator_t *out_credentials_iterator)
{
    cc_int32 err = ccNoError;

    if (!in_ccache               ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier           ) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_credentials_list_find_iterator (in_ccache->credentials,
                                                  in_identifier,
                                                  out_credentials_iterator);
    }

    // Don't report ccErrInvalidCredentials to the log file.  Non-fatal.
    return (err == ccErrInvalidCredentials) ? err : cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_write (ccs_ccache_t in_ccache,
                           k5_ipc_stream io_stream)
{
    cc_int32 err = ccNoError;

    if (!in_ccache) { err = cci_check_error (ccErrBadParam); }
    if (!io_stream) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_write (in_ccache->identifier, io_stream);
    }

    return cci_check_error (err);
}


/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_write_name (ccs_ccache_t in_ccache,
                                k5_ipc_stream io_stream)
{
    cc_int32 err = ccNoError;

    if (!in_ccache) { err = cci_check_error (ccErrBadParam); }
    if (!io_stream) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_string (io_stream, in_ccache->name);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#pragma mark -- IPC Messages --
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_destroy (ccs_ccache_t           io_ccache,
                                    ccs_cache_collection_t io_cache_collection,
                                    k5_ipc_stream           in_request_data,
                                    k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_cache_collection_destroy_ccache (io_cache_collection,
                                                   io_ccache->identifier);
    }

    if (!err) {
        /* ccache has been destroyed so just mark the cache collection */
        err = ccs_cache_collection_changed (io_cache_collection);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_set_default (ccs_ccache_t           io_ccache,
                                        ccs_cache_collection_t io_cache_collection,
                                        k5_ipc_stream           in_request_data,
                                        k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_cache_collection_set_default_ccache (io_cache_collection,
                                                       io_ccache->identifier);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_get_credentials_version (ccs_ccache_t           io_ccache,
                                                    ccs_cache_collection_t io_cache_collection,
                                                    k5_ipc_stream           in_request_data,
                                                    k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_reply_data, io_ccache->creds_version);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_get_name (ccs_ccache_t           io_ccache,
                                     ccs_cache_collection_t io_cache_collection,
                                     k5_ipc_stream           in_request_data,
                                     k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_string (io_reply_data, io_ccache->name);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_get_principal (ccs_ccache_t           io_ccache,
                                          ccs_cache_collection_t io_cache_collection,
                                          k5_ipc_stream           in_request_data,
                                          k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 version = 0;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &version);
    }

    if (!err && version == cc_credentials_v4_v5) {
        err = cci_check_error (ccErrBadCredentialsVersion);
    }

    if (!err) {
        if (version == cc_credentials_v4) {
            err = krb5int_ipc_stream_write_string (io_reply_data, io_ccache->v4_principal);

        } else if (version == cc_credentials_v5) {
            err = krb5int_ipc_stream_write_string (io_reply_data, io_ccache->v5_principal);

        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_set_principal (ccs_ccache_t           io_ccache,
                                          ccs_cache_collection_t io_cache_collection,
                                          k5_ipc_stream           in_request_data,
                                          k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 version = 0;
    char *principal = NULL;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &version);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (in_request_data, &principal);
    }

    if (!err) {
        /* reset KDC time offsets because they are per-KDC */
        if (version == cc_credentials_v4) {
            io_ccache->kdc_time_offset_v4 = 0;
            io_ccache->kdc_time_offset_v4_valid = 0;

            if (io_ccache->v4_principal) { free (io_ccache->v4_principal); }
            io_ccache->v4_principal = principal;
            principal = NULL; /* take ownership */


        } else if (version == cc_credentials_v5) {
            io_ccache->kdc_time_offset_v5 = 0;
            io_ccache->kdc_time_offset_v5_valid = 0;

            if (io_ccache->v5_principal) { free (io_ccache->v5_principal); }
            io_ccache->v5_principal = principal;
            principal = NULL; /* take ownership */

        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    if (!err) {
        io_ccache->creds_version |= version;

        err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }

    krb5int_ipc_stream_free_string (principal);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_store_credentials (ccs_ccache_t           io_ccache,
                                              ccs_cache_collection_t io_cache_collection,
                                              k5_ipc_stream           in_request_data,
                                              k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    ccs_credentials_t credentials = NULL;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_credentials_new (&credentials, in_request_data,
                                   io_ccache->creds_version,
                                   io_ccache->credentials);
    }

    if (!err) {
        err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }


    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_remove_credentials (ccs_ccache_t           io_ccache,
                                               ccs_cache_collection_t io_cache_collection,
                                               k5_ipc_stream           in_request_data,
                                               k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cci_identifier_t credentials_identifier = NULL;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_read (&credentials_identifier, in_request_data);
    }

    if (!err) {
        err = ccs_credentials_list_remove (io_ccache->credentials, credentials_identifier);
    }

    if (!err) {
        err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }

    cci_identifier_release (credentials_identifier);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_new_credentials_iterator (ccs_ccache_t           io_ccache,
                                                     ccs_cache_collection_t io_cache_collection,
                                                     ccs_pipe_t             in_client_pipe,
                                                     k5_ipc_stream           in_request_data,
                                                     k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    ccs_credentials_iterator_t credentials_iterator = NULL;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_credentials_list_new_iterator (io_ccache->credentials,
                                                 in_client_pipe,
                                                 &credentials_iterator);
    }

    if (!err) {
        err = ccs_credentials_list_iterator_write (credentials_iterator, io_reply_data);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_move (ccs_ccache_t           io_ccache,
                                 ccs_cache_collection_t io_cache_collection,
                                 k5_ipc_stream           in_request_data,
                                 k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cci_identifier_t source_identifier = NULL;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        /* Note: message is sent as the destination ccache to avoid     */
        /* extra work on the server when deleting it the source ccache. */
        err = cci_identifier_read (&source_identifier, in_request_data);
    }

    if (!err) {
        err = ccs_ccache_collection_move_ccache (io_cache_collection,
                                                 source_identifier,
                                                 io_ccache);
    }

    if (!err) {
        err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }

    cci_identifier_release (source_identifier);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_lock (ccs_pipe_t             in_client_pipe,
                                 ccs_pipe_t             in_reply_pipe,
                                 ccs_ccache_t           io_ccache,
                                 ccs_cache_collection_t io_cache_collection,
                                 k5_ipc_stream           in_request_data,
                                 cc_uint32              *out_will_block,
                                 k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 lock_type;
    cc_uint32 block;

    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache                      ) { err = cci_check_error (ccErrBadParam); }
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
        err = ccs_lock_state_add (io_ccache->lock_state,
                                  in_client_pipe, in_reply_pipe,
                                  lock_type, block, out_will_block);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_unlock (ccs_pipe_t             in_client_pipe,
                                   ccs_ccache_t           io_ccache,
                                   ccs_cache_collection_t io_cache_collection,
                                   k5_ipc_stream           in_request_data,
                                   k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache                      ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection            ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data                ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data                  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_lock_state_remove (io_ccache->lock_state, in_client_pipe);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_get_last_default_time (ccs_ccache_t           io_ccache,
                                                  ccs_cache_collection_t io_cache_collection,
                                                  k5_ipc_stream           in_request_data,
                                                  k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err && io_ccache->last_default_time == 0) {
        err = cci_check_error (ccErrNeverDefault);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_reply_data, io_ccache->last_default_time);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_get_change_time (ccs_ccache_t           io_ccache,
                                            ccs_cache_collection_t io_cache_collection,
                                            k5_ipc_stream           in_request_data,
                                            k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_reply_data, io_ccache->last_changed_time);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_wait_for_change (ccs_pipe_t              in_client_pipe,
					    ccs_pipe_t              in_reply_pipe,
					    ccs_ccache_t            io_ccache,
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
    if (!io_ccache                      ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection            ) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data                ) { err = cci_check_error (ccErrBadParam); }
    if (!out_will_block                 ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_time (in_request_data, &last_wait_for_change_time);
    }

    if (!err) {
	if (last_wait_for_change_time < io_ccache->last_changed_time) {
	    cci_debug_printf ("%s returning immediately", __FUNCTION__);
	    err = krb5int_ipc_stream_write_time (io_reply_data, io_ccache->last_changed_time);

	} else {
	    ccs_callback_t callback = NULL;

	    err = ccs_callback_new (&callback,
				    ccErrInvalidCCache,
				    in_client_pipe,
				    in_reply_pipe,
				    (ccs_callback_owner_t) io_ccache,
				    ccs_ccache_invalidate_change_callback);

	    if (!err) {
		err = ccs_callback_array_insert (io_ccache->change_callbacks, callback,
						 ccs_callback_array_count (io_ccache->change_callbacks));
		if (!err) { callback = NULL; /* take ownership */ }

		cci_debug_printf ("%s blocking", __FUNCTION__);
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

static cc_int32 ccs_ccache_get_kdc_time_offset (ccs_ccache_t           io_ccache,
                                                ccs_cache_collection_t io_cache_collection,
                                                k5_ipc_stream           in_request_data,
                                                k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 cred_vers = 0;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &cred_vers);
    }

    if (!err) {
        if (cred_vers == cc_credentials_v4) {
            if (io_ccache->kdc_time_offset_v4_valid) {
                err = krb5int_ipc_stream_write_time (io_reply_data, io_ccache->kdc_time_offset_v4);
            } else {
                err = cci_check_error (ccErrTimeOffsetNotSet);
            }

        } else if (cred_vers == cc_credentials_v5) {
            if (io_ccache->kdc_time_offset_v5_valid) {
                err = krb5int_ipc_stream_write_time (io_reply_data, io_ccache->kdc_time_offset_v5);
            } else {
                err = cci_check_error (ccErrTimeOffsetNotSet);
            }

        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_set_kdc_time_offset (ccs_ccache_t           io_ccache,
                                                ccs_cache_collection_t io_cache_collection,
                                                k5_ipc_stream           in_request_data,
                                                k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 cred_vers = 0;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &cred_vers);
    }

    if (!err) {
        if (cred_vers == cc_credentials_v4) {
            err = krb5int_ipc_stream_read_time (in_request_data, &io_ccache->kdc_time_offset_v4);

            if (!err) {
                io_ccache->kdc_time_offset_v4_valid = 1;
            }
        } else if (cred_vers == cc_credentials_v5) {
            err = krb5int_ipc_stream_read_time (in_request_data, &io_ccache->kdc_time_offset_v5);

            if (!err) {
                io_ccache->kdc_time_offset_v5_valid = 1;
            }
        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    if (!err) {
        err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_clear_kdc_time_offset (ccs_ccache_t           io_ccache,
                                                  ccs_cache_collection_t io_cache_collection,
                                                  k5_ipc_stream           in_request_data,
                                                  k5_ipc_stream           io_reply_data)
{
    cc_int32 err = ccNoError;
    cc_uint32 cred_vers = 0;

    if (!io_ccache          ) { err = cci_check_error (ccErrBadParam); }
    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_request_data    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_reply_data      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (in_request_data, &cred_vers);
    }

    if (!err) {
        if (cred_vers == cc_credentials_v4) {
            io_ccache->kdc_time_offset_v4 = 0;
            io_ccache->kdc_time_offset_v4_valid = 0;

        } else if (cred_vers == cc_credentials_v5) {
            io_ccache->kdc_time_offset_v5 = 0;
            io_ccache->kdc_time_offset_v5_valid = 0;

        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    if (!err) {
        err = ccs_ccache_changed (io_ccache, io_cache_collection);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_handle_message (ccs_pipe_t              in_client_pipe,
                                    ccs_pipe_t              in_reply_pipe,
                                    ccs_ccache_t            io_ccache,
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
        if (in_request_name == cci_ccache_destroy_msg_id) {
            err = ccs_ccache_destroy (io_ccache, io_cache_collection,
                                      in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_set_default_msg_id) {
            err = ccs_ccache_set_default (io_ccache, io_cache_collection,
                                          in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_get_credentials_version_msg_id) {
            err = ccs_ccache_get_credentials_version (io_ccache, io_cache_collection,
                                                      in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_get_name_msg_id) {
            err = ccs_ccache_get_name (io_ccache, io_cache_collection,
                                       in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_get_principal_msg_id) {
            err = ccs_ccache_get_principal (io_ccache, io_cache_collection,
                                            in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_set_principal_msg_id) {
            err = ccs_ccache_set_principal (io_ccache, io_cache_collection,
                                            in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_store_credentials_msg_id) {
            err = ccs_ccache_store_credentials (io_ccache, io_cache_collection,
                                                in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_remove_credentials_msg_id) {
            err = ccs_ccache_remove_credentials (io_ccache, io_cache_collection,
                                                 in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_new_credentials_iterator_msg_id) {
            err = ccs_ccache_new_credentials_iterator (io_ccache,
                                                       io_cache_collection,
                                                       in_client_pipe,
                                                       in_request_data,
                                                       reply_data);

        } else if (in_request_name == cci_ccache_move_msg_id) {
            err = ccs_ccache_move (io_ccache, io_cache_collection,
                                   in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_lock_msg_id) {
            err = ccs_ccache_lock (in_client_pipe, in_reply_pipe,
                                   io_ccache, io_cache_collection,
                                   in_request_data,
                                   &will_block, reply_data);

        } else if (in_request_name == cci_ccache_unlock_msg_id) {
            err = ccs_ccache_unlock (in_client_pipe,
                                     io_ccache, io_cache_collection,
                                     in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_get_last_default_time_msg_id) {
            err = ccs_ccache_get_last_default_time (io_ccache, io_cache_collection,
                                                    in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_get_change_time_msg_id) {
            err = ccs_ccache_get_change_time (io_ccache, io_cache_collection,
                                              in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_wait_for_change_msg_id) {
            err = ccs_ccache_wait_for_change (in_client_pipe, in_reply_pipe,
					      io_ccache, io_cache_collection,
					      in_request_data, reply_data,
					      &will_block);

        } else if (in_request_name == cci_ccache_get_kdc_time_offset_msg_id) {
            err = ccs_ccache_get_kdc_time_offset (io_ccache, io_cache_collection,
                                                  in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_set_kdc_time_offset_msg_id) {
            err = ccs_ccache_set_kdc_time_offset (io_ccache, io_cache_collection,
                                                  in_request_data, reply_data);

        } else if (in_request_name == cci_ccache_clear_kdc_time_offset_msg_id) {
            err = ccs_ccache_clear_kdc_time_offset (io_ccache, io_cache_collection,
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
