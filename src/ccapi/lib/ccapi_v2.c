/* ccapi/lib/ccapi_v2.c */
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
#include "ccapi_string.h"
#include "ccapi_context.h"
#include "ccapi_ccache.h"
#include "ccapi_ccache_iterator.h"
#include "ccapi_credentials.h"
#include "ccapi_credentials_iterator.h"
#include <CredentialsCache2.h>

infoNC infoNC_initializer = { NULL, NULL, CC_CRED_UNKNOWN };

/* ------------------------------------------------------------------------ */

static cc_int32 cci_remap_version (cc_int32   in_v2_version,
                                   cc_uint32 *out_v3_version)
{
    cc_result err = ccNoError;

    if (!out_v3_version) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        if (in_v2_version == CC_CRED_V4) {
            *out_v3_version = cc_credentials_v4;

        } else if (in_v2_version == CC_CRED_V5) {
            *out_v3_version = cc_credentials_v5;

        } else {
            err = ccErrBadCredentialsVersion;
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_result _cci_remap_error (cc_result   in_error,
                                   const char *in_function,
                                   const char *in_file,
                                   int         in_line)
{
    _cci_check_error (in_error, in_function, in_file, in_line);

    if (in_error >= CC_NOERROR && in_error <= CC_ERR_CRED_VERSION) {
        return in_error;
    }

    switch (in_error) {
        case ccNoError:
            return CC_NOERROR;

        case ccIteratorEnd:
            return CC_END;

        case ccErrBadParam:
        case ccErrContextNotFound:
        case ccErrInvalidContext:
        case ccErrInvalidCredentials:
        case ccErrInvalidCCacheIterator:
        case ccErrInvalidCredentialsIterator:
        case ccErrInvalidLock:
        case ccErrBadLockType:
            return CC_BAD_PARM;

        case ccErrNoMem:
            return CC_NOMEM;

        case ccErrInvalidCCache:
        case ccErrCCacheNotFound:
            return CC_NO_EXIST;

        case ccErrCredentialsNotFound:
            return CC_NOTFOUND;

        case ccErrBadName:
            return CC_BADNAME;

        case ccErrBadCredentialsVersion:
            return CC_ERR_CRED_VERSION;

        case ccErrBadAPIVersion:
            return CC_BAD_API_VERSION;

        case ccErrContextLocked:
        case ccErrContextUnlocked:
        case ccErrCCacheLocked:
        case ccErrCCacheUnlocked:
            return CC_LOCKED;

        case ccErrServerUnavailable:
        case ccErrServerInsecure:
        case ccErrServerCantBecomeUID:
        case ccErrBadInternalMessage:
        case ccErrClientNotFound:
            return CC_IO;

        case ccErrNotImplemented:
            return CC_NOT_SUPP;

        default:
            cci_debug_printf ("%s(): Unhandled error", __FUNCTION__);
            return CC_BAD_PARM;
    }
}
#define cci_remap_error(err) _cci_remap_error(err, __FUNCTION__, __FILE__, __LINE__)


#if TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_result cc_shutdown (apiCB **io_context)
{
    cc_result err = ccNoError;

    if (!io_context) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_context_release (*io_context);
    }

    if (!err) {
        *io_context = NULL;
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_get_change_time (apiCB     *in_context,
                              cc_time_t *out_change_time)
{
    cc_result err = ccNoError;

    if (!in_context     ) { err = cci_check_error (ccErrBadParam); }
    if (!out_change_time) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_context_get_change_time (in_context, out_change_time);
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_get_NC_info (apiCB    *in_context,
                          infoNC ***out_info)
{
    cc_result err = CC_NOERROR;
    infoNC **info = NULL;
    cc_uint64 count = 0; /* Preflight the size */
    cc_uint64 i;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!out_info  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        ccache_cit *iterator = NULL;

        err = cc_seq_fetch_NCs_begin (in_context, &iterator);

        while (!err) {
            ccache_p *ccache = NULL;

            err = cc_seq_fetch_NCs_next (in_context, &ccache, iterator);

            if (!err) { count++; }

            if (ccache) { cc_close (in_context, &ccache); }
        }
        if (err == CC_END) { err = CC_NOERROR; }

        if (!err) {
            err = cc_seq_fetch_NCs_end (in_context, &iterator);
        }
    }

    if (!err) {
        info = malloc (sizeof (*info) * (count + 1));
        if (info) {
            for (i = 0; i < count + 1; i++) { info[i] = NULL; }
        } else {
            err = cci_check_error (CC_NOMEM);
        }
    }

    if (!err) {
        ccache_cit *iterator = NULL;

        err = cc_seq_fetch_NCs_begin (in_context, &iterator);

        for (i = 0; !err && i < count; i++) {
            ccache_p *ccache = NULL;

            err = cc_seq_fetch_NCs_next (in_context, &ccache, iterator);

            if (!err) {
                info[i] = malloc (sizeof (*info[i]));
                if (info[i]) {
                    *info[i] = infoNC_initializer;
                } else {
                    err = cci_check_error (CC_NOMEM);
                }
            }

            if (!err) {
                err = cc_get_name (in_context, ccache, &info[i]->name);
            }

            if (!err) {
                err = cc_get_principal (in_context, ccache, &info[i]->principal);
            }

            if (!err) {
                err = cc_get_cred_version (in_context, ccache, &info[i]->vers);
            }

            if (ccache) { cc_close (in_context, &ccache); }
        }

        if (!err) {
            err = cc_seq_fetch_NCs_end (in_context, &iterator);
        }
    }

    if (!err) {
        *out_info = info;
        info = NULL;
    }

    if (info) { cc_free_NC_info (in_context, &info); }

    return cci_check_error (err);
}

#if TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 cc_open (apiCB       *in_context,
                  const char  *in_name,
                  cc_int32     in_version,
                  cc_uint32    in_flags,
                  ccache_p   **out_ccache)
{
    cc_result err = ccNoError;
    cc_ccache_t ccache = NULL;
    cc_uint32 compat_version;
    cc_uint32 real_version;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!in_name   ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_remap_version (in_version, &compat_version);
    }

    if (!err) {
        err = ccapi_context_open_ccache (in_context, in_name, &ccache);
    }

    /* We must not allow a CCAPI v2 caller to open a v5-only ccache
     as a v4 ccache and vice versa. Allowing that would break
     (valid) assumptions made by CCAPI v2 callers. */

    if (!err) {
        err = ccapi_ccache_get_credentials_version (ccache, &real_version);
    }

    if (!err) {
        /* check the version and set up the ccache to use it */
        if (compat_version & real_version) {
            err = cci_ccache_set_compat_version (ccache, compat_version);
        } else {
            err = ccErrBadCredentialsVersion;
        }
    }

    if (!err) {
        *out_ccache = ccache;
        ccache = NULL;
    }

    if (ccache) { ccapi_ccache_release (ccache); }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_create (apiCB       *in_context,
                     const char  *in_name,
                     const char  *in_principal,
                     cc_int32     in_version,
                     cc_uint32    in_flags,
                     ccache_p   **out_ccache)
{
    cc_result err = ccNoError;
    cc_ccache_t	ccache = NULL;
    cc_uint32 compat_version;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!in_name   ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_remap_version (in_version, &compat_version);
    }

    if (!err) {
        err = ccapi_context_create_ccache (in_context, in_name, compat_version,
                                           in_principal, &ccache);
    }

    if (!err) {
        err = cci_ccache_set_compat_version (ccache, compat_version);
    }

    if (!err) {
        *out_ccache = ccache;
        ccache = NULL;
    }

    if (ccache) { ccapi_ccache_release (ccache); }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_close (apiCB     *in_context,
                    ccache_p **io_ccache)
{
    cc_result err = ccNoError;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_ccache_release (*io_ccache);
    }

    if (!err) {
        *io_ccache = NULL;
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_destroy (apiCB     *in_context,
                      ccache_p **io_ccache)
{
    cc_result err = ccNoError;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_ccache_destroy (*io_ccache);
    }

    if (!err) {
        *io_ccache = NULL;
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_get_name (apiCB     *in_context,
                       ccache_p  *in_ccache,
                       char     **out_name)
{
    cc_result err = ccNoError;
    cc_string_t name = NULL;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccache ) { err = cci_check_error (ccErrBadParam); }
    if (!out_name  ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_ccache_get_name (in_ccache, &name);
    }

    if (!err) {
        char *string = strdup (name->data);
        if (string) {
            *out_name = string;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (name) { ccapi_string_release (name); }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_get_cred_version (apiCB    *in_context,
                               ccache_p *in_ccache,
                               cc_int32 *out_version)
{
    cc_result err = ccNoError;
    cc_uint32 compat_version;

    if (!in_context ) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccache  ) { err = cci_check_error (ccErrBadParam); }
    if (!out_version) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_ccache_get_compat_version (in_ccache, &compat_version);
    }

    if (!err) {
        if (compat_version == cc_credentials_v4) {
            *out_version = CC_CRED_V4;

        } else if (compat_version == cc_credentials_v5) {
            *out_version = CC_CRED_V5;

        } else {
            err = ccErrBadCredentialsVersion;
        }
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_set_principal (apiCB    *in_context,
                            ccache_p *io_ccache,
                            cc_int32  in_version,
                            char     *in_principal)
{
    cc_result err = ccNoError;
    cc_uint32 version;
    cc_uint32 compat_version;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache   ) { err = cci_check_error (ccErrBadParam); }
    if (!in_principal) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_remap_version (in_version, &version);
    }

    if (!err) {
        err = cci_ccache_get_compat_version (io_ccache, &compat_version);
    }

    if (!err && version != compat_version) {
        err = cci_check_error (ccErrBadCredentialsVersion);
    }

    if (!err) {
        err = ccapi_ccache_set_principal (io_ccache, version, in_principal);
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_get_principal (apiCB      *in_context,
                            ccache_p   *in_ccache,
                            char      **out_principal)
{
    cc_result err = ccNoError;
    cc_uint32 compat_version;
    cc_string_t principal = NULL;

    if (!in_context   ) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccache    ) { err = cci_check_error (ccErrBadParam); }
    if (!out_principal) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_ccache_get_compat_version (in_ccache, &compat_version);
    }

    if (!err) {
        err = ccapi_ccache_get_principal (in_ccache, compat_version, &principal);
    }

    if (!err) {
        char *string = strdup (principal->data);
        if (string) {
            *out_principal = string;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (principal) { ccapi_string_release (principal); }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_store (apiCB      *in_context,
                    ccache_p   *io_ccache,
                    cred_union  in_credentials)
{
    cc_result err = ccNoError;
    cc_credentials_union *creds_union = NULL;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccache ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_cred_union_to_credentials_union (&in_credentials,
                                                   &creds_union);
    }

    if (!err) {
        err = ccapi_ccache_store_credentials (io_ccache, creds_union);
    }

    if (creds_union) { cci_credentials_union_release (creds_union); }
    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_remove_cred (apiCB      *in_context,
                          ccache_p   *in_ccache,
                          cred_union  in_credentials)
{
    cc_result err = ccNoError;
    cc_credentials_iterator_t iterator = NULL;
    cc_uint32 found = 0;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccache ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_ccache_new_credentials_iterator (in_ccache, &iterator);
    }

    while (!err && !found) {
        cc_credentials_t creds = NULL;

        err = ccapi_credentials_iterator_next (iterator, &creds);

        if (!err) {
            err = cci_cred_union_compare_to_credentials_union (&in_credentials,
                                                               creds->data,
                                                               &found);
        }

        if (!err && found) {
            err = ccapi_ccache_remove_credentials (in_ccache, creds);
        }

        ccapi_credentials_release (creds);
    }
    if (err == ccIteratorEnd) { err = cci_check_error (ccErrCredentialsNotFound); }

    return cci_remap_error (err);
}

#if TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_result cc_seq_fetch_NCs_begin (apiCB       *in_context,
                                  ccache_cit **out_iterator)
{
    cc_result err = ccNoError;
    cc_ccache_iterator_t iterator = NULL;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!out_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_context_new_ccache_iterator (in_context, &iterator);
    }

    if (!err) {
        *out_iterator = (ccache_cit *) iterator;
        iterator = NULL; /* take ownership */
    }

    if (iterator) { ccapi_ccache_iterator_release (iterator); }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_seq_fetch_NCs_next (apiCB       *in_context,
                                 ccache_p   **out_ccache,
                                 ccache_cit  *in_iterator)
{
    cc_result err = ccNoError;
    cc_ccache_iterator_t iterator = (cc_ccache_iterator_t) in_iterator;
    cc_ccache_t ccache = NULL;
    const char *saved_ccache_name;

    if (!in_context ) { err = cci_check_error (ccErrBadParam); }
    if (!out_ccache ) { err = cci_check_error (ccErrBadParam); }
    if (!in_iterator) { err = cci_check_error (ccErrBadParam); }

    /* CCache iterators need to return some ccaches twice (when v3 ccache has
     * two kinds of credentials). To do that, we return such ccaches twice
     * v4 first, then v5. */

    if (!err) {
        err = cci_ccache_iterator_get_saved_ccache_name (iterator,
                                                         &saved_ccache_name);
    }

    if (!err) {
        if (saved_ccache_name) {
            err = ccapi_context_open_ccache (in_context, saved_ccache_name,
                                             &ccache);

            if (!err) {
                err = cci_ccache_set_compat_version (ccache, cc_credentials_v5);
            }

            if (!err) {
                err = cci_ccache_iterator_set_saved_ccache_name (iterator, NULL);
            }

        } else {
            cc_uint32 version = 0;

            err = ccapi_ccache_iterator_next (iterator, &ccache);

            if (!err) {
                err = ccapi_ccache_get_credentials_version (ccache, &version);
            }

            if (!err) {
                if (version == cc_credentials_v4_v5) {
                    cc_string_t name = NULL;

                    err = cci_ccache_set_compat_version (ccache, cc_credentials_v4);

                    if (!err) {
                        err = ccapi_ccache_get_name (ccache, &name);
                    }

                    if (!err) {
                        err = cci_ccache_iterator_set_saved_ccache_name (iterator,
                                                                         name->data);
                    }

                    if (name) { ccapi_string_release (name); }

                } else {
                    err = cci_ccache_set_compat_version (ccache, version);
                }
            }
        }
    }

    if (!err) {
        *out_ccache = ccache;
        ccache = NULL; /* take ownership */
    }

    if (ccache) { ccapi_ccache_release (ccache); }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_seq_fetch_NCs_end (apiCB       *in_context,
                                ccache_cit **io_iterator)
{
    cc_result err = ccNoError;
    cc_ccache_iterator_t iterator = (cc_ccache_iterator_t) *io_iterator;

    if (!in_context ) { err = cci_check_error (ccErrBadParam); }
    if (!io_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_ccache_iterator_release (iterator);
    }

    if (!err) {
        *io_iterator = NULL;
    }

    return cci_remap_error (err);
}

#if TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_result cc_seq_fetch_creds_begin (apiCB           *in_context,
                                    const ccache_p  *in_ccache,
                                    ccache_cit     **out_iterator)
{
    cc_result err = ccNoError;
    cc_credentials_iterator_t iterator = NULL;
    cc_uint32 compat_version;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccache   ) { err = cci_check_error (ccErrBadParam); }
    if (!out_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_ccache_get_compat_version ((cc_ccache_t) in_ccache,
                                             &compat_version);
    }

    if (!err) {
        err = ccapi_ccache_new_credentials_iterator ((cc_ccache_t) in_ccache,
                                                     &iterator);
    }

    if (!err) {
        err = cci_credentials_iterator_set_compat_version (iterator,
                                                           compat_version);
    }

    if (!err) {
        *out_iterator = (ccache_cit *) iterator;
        iterator = NULL; /* take ownership */
    }

    if (iterator) { ccapi_credentials_iterator_release (iterator); }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_seq_fetch_creds_next (apiCB       *in_context,
                                   cred_union **out_creds,
                                   ccache_cit  *in_iterator)
{
    cc_result err = ccNoError;
    cc_credentials_iterator_t iterator = (cc_credentials_iterator_t) in_iterator;
    cc_uint32 compat_version;

    if (!in_context ) { err = cci_check_error (ccErrBadParam); }
    if (!out_creds  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_credentials_iterator_get_compat_version (iterator,
                                                           &compat_version);
    }

    while (!err) {
        cc_credentials_t credentials = NULL;

        err = ccapi_credentials_iterator_next (iterator, &credentials);

        if (!err && (credentials->data->version & compat_version)) {
            /* got the next credentials for the correct version */
            err = cci_credentials_union_to_cred_union (credentials->data,
                                                       out_creds);
            break;
        }

        if (credentials) { ccapi_credentials_release (credentials); }
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_seq_fetch_creds_end (apiCB       *in_context,
                                  ccache_cit **io_iterator)
{
    cc_result err = ccNoError;
    cc_credentials_iterator_t iterator = (cc_credentials_iterator_t) *io_iterator;

    if (!in_context ) { err = cci_check_error (ccErrBadParam); }
    if (!io_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccapi_credentials_iterator_release (iterator);
    }

    if (!err) {
        *io_iterator = NULL;
    }

    return cci_remap_error (err);
}

#if TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_result cc_free_principal (apiCB  *in_context,
                             char  **io_principal)
{
    cc_result err = ccNoError;

    if (!in_context  ) { err = cci_check_error (ccErrBadParam); }
    if (!io_principal) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        free (*io_principal);
        *io_principal = NULL;
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_free_name (apiCB  *in_context,
                        char  **io_name)
{
    cc_result err = ccNoError;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!io_name   ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        free (*io_name);
        *io_name = NULL;
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_free_creds (apiCB       *in_context,
                         cred_union **io_credentials)
{
    cc_result err = ccNoError;

    if (!in_context    ) { err = cci_check_error (ccErrBadParam); }
    if (!io_credentials) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_cred_union_release (*io_credentials);
        if (!err) { *io_credentials = NULL; }
    }

    return cci_remap_error (err);
}

/* ------------------------------------------------------------------------ */

cc_result cc_free_NC_info (apiCB    *in_context,
                           infoNC ***io_info)
{
    cc_result err = ccNoError;

    if (!in_context) { err = cci_check_error (ccErrBadParam); }
    if (!io_info   ) { err = cci_check_error (ccErrBadParam); }

    if (!err && *io_info) {
        infoNC **data = *io_info;
        int i;

        for (i = 0; data[i] != NULL; i++) {
            cc_free_principal (in_context, &data[i]->principal);
            cc_free_name (in_context, &data[i]->name);
            free (data[i]);
        }
        free (data);

        *io_info = NULL;
    }

    return cci_remap_error (err);
}
