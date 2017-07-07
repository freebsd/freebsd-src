/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccapi/stdcc.c - ccache API support functions */
/*
 * Copyright 1998, 1999, 2006, 2008 by the Massachusetts Institute of
 * Technology.  All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
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

/*
 * Written by Frank Dabek July 1998
 * Updated by Jeffrey Altman June 2006
 */

#if defined(_WIN32) || defined(USE_CCAPI)

#include "k5-int.h"
#include "../cc-int.h"
#include "stdcc.h"
#include "stdcc_util.h"
#include "string.h"
#include <stdio.h>

#if defined(_WIN32)
#include "winccld.h"
#endif

#ifndef CC_API_VER2
#define CC_API_VER2
#endif

#ifdef DEBUG
#if defined(_WIN32)
#include <io.h>
#define SHOW_DEBUG(buf)   MessageBox((HWND)NULL, (buf), "ccapi debug", MB_OK)
#endif
/* XXX need macintosh debugging statement if we want to debug */
/* on the mac */
#else
#define SHOW_DEBUG(buf)
#endif

#ifdef USE_CCAPI_V3
cc_context_t gCntrlBlock = NULL;
cc_int32 gCCVersion = 0;
#else
apiCB *gCntrlBlock = NULL;
#endif

/*
 * declare our global object wanna-be
 * must be installed in ccdefops.c
 */

krb5_cc_ops krb5_cc_stdcc_ops = {
    0,
    "API",
#ifdef USE_CCAPI_V3
    krb5_stdccv3_get_name,
    krb5_stdccv3_resolve,
    krb5_stdccv3_generate_new,
    krb5_stdccv3_initialize,
    krb5_stdccv3_destroy,
    krb5_stdccv3_close,
    krb5_stdccv3_store,
    krb5_stdccv3_retrieve,
    krb5_stdccv3_get_principal,
    krb5_stdccv3_start_seq_get,
    krb5_stdccv3_next_cred,
    krb5_stdccv3_end_seq_get,
    krb5_stdccv3_remove,
    krb5_stdccv3_set_flags,
    krb5_stdccv3_get_flags,
    krb5_stdccv3_ptcursor_new,
    krb5_stdccv3_ptcursor_next,
    krb5_stdccv3_ptcursor_free,
    NULL, /* move */
    krb5_stdccv3_last_change_time, /* lastchange */
    NULL, /* wasdefault */
    krb5_stdccv3_lock,
    krb5_stdccv3_unlock,
    krb5_stdccv3_switch_to,
#else
    krb5_stdcc_get_name,
    krb5_stdcc_resolve,
    krb5_stdcc_generate_new,
    krb5_stdcc_initialize,
    krb5_stdcc_destroy,
    krb5_stdcc_close,
    krb5_stdcc_store,
    krb5_stdcc_retrieve,
    krb5_stdcc_get_principal,
    krb5_stdcc_start_seq_get,
    krb5_stdcc_next_cred,
    krb5_stdcc_end_seq_get,
    krb5_stdcc_remove,
    krb5_stdcc_set_flags,
    krb5_stdcc_get_flags,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#endif
};

#if defined(_WIN32)
/*
 * cache_changed be called after the cache changes.
 * A notification message is is posted out to all top level
 * windows so that they may recheck the cache based on the
 * changes made.  We register a unique message type with which
 * we'll communicate to all other processes.
 */
static void cache_changed()
{
    static unsigned int message = 0;

    if (message == 0)
        message = RegisterWindowMessage(WM_KERBEROS5_CHANGED);

    PostMessage(HWND_BROADCAST, message, 0, 0);
}
#else /* _WIN32 */

static void cache_changed()
{
    return;
}
#endif /* _WIN32 */

struct err_xlate
{
    int     cc_err;
    krb5_error_code krb5_err;
};

static const struct err_xlate err_xlate_table[] =
{
#ifdef USE_CCAPI_V3
    { ccIteratorEnd,                        KRB5_CC_END },
    { ccErrBadParam,                        KRB5_FCC_INTERNAL },
    { ccErrNoMem,                           KRB5_CC_NOMEM },
    { ccErrInvalidContext,                  KRB5_FCC_NOFILE },
    { ccErrInvalidCCache,                   KRB5_FCC_NOFILE },
    { ccErrInvalidString,                   KRB5_FCC_INTERNAL },
    { ccErrInvalidCredentials,              KRB5_FCC_INTERNAL },
    { ccErrInvalidCCacheIterator,           KRB5_FCC_INTERNAL },
    { ccErrInvalidCredentialsIterator,      KRB5_FCC_INTERNAL },
    { ccErrInvalidLock,                     KRB5_FCC_INTERNAL },
    { ccErrBadName,                         KRB5_CC_BADNAME },
    { ccErrBadCredentialsVersion,           KRB5_FCC_INTERNAL },
    { ccErrBadAPIVersion,                   KRB5_FCC_INTERNAL },
    { ccErrContextLocked,                   KRB5_FCC_INTERNAL },
    { ccErrContextUnlocked,                 KRB5_FCC_INTERNAL },
    { ccErrCCacheLocked,                    KRB5_FCC_INTERNAL },
    { ccErrCCacheUnlocked,                  KRB5_FCC_INTERNAL },
    { ccErrBadLockType,                     KRB5_FCC_INTERNAL },
    { ccErrNeverDefault,                    KRB5_FCC_INTERNAL },
    { ccErrCredentialsNotFound,             KRB5_CC_NOTFOUND },
    { ccErrCCacheNotFound,                  KRB5_FCC_NOFILE },
    { ccErrContextNotFound,                 KRB5_FCC_NOFILE },
    { ccErrServerUnavailable,               KRB5_CC_IO },
    { ccErrServerInsecure,                  KRB5_CC_IO },
    { ccErrServerCantBecomeUID,             KRB5_CC_IO },
    { ccErrTimeOffsetNotSet,                KRB5_FCC_INTERNAL },
    { ccErrBadInternalMessage,              KRB5_FCC_INTERNAL },
    { ccErrNotImplemented,                  KRB5_FCC_INTERNAL },
#else
    { CC_BADNAME,                           KRB5_CC_BADNAME },
    { CC_NOTFOUND,                          KRB5_CC_NOTFOUND },
    { CC_END,                               KRB5_CC_END },
    { CC_IO,                                KRB5_CC_IO },
    { CC_WRITE,                             KRB5_CC_WRITE },
    { CC_NOMEM,                             KRB5_CC_NOMEM },
    { CC_FORMAT,                            KRB5_CC_FORMAT },
    { CC_WRITE,                             KRB5_CC_WRITE },
    { CC_LOCKED,                            KRB5_FCC_INTERNAL /* XXX */ },
    { CC_BAD_API_VERSION,                   KRB5_FCC_INTERNAL /* XXX */ },
    { CC_NO_EXIST,                          KRB5_FCC_NOFILE },
    { CC_NOT_SUPP,                          KRB5_FCC_INTERNAL /* XXX */ },
    { CC_BAD_PARM,                          KRB5_FCC_INTERNAL /* XXX */ },
    { CC_ERR_CACHE_ATTACH,                  KRB5_FCC_INTERNAL /* XXX */ },
    { CC_ERR_CACHE_RELEASE,                 KRB5_FCC_INTERNAL /* XXX */ },
    { CC_ERR_CACHE_FULL,                    KRB5_FCC_INTERNAL /* XXX */ },
    { CC_ERR_CRED_VERSION,                  KRB5_FCC_INTERNAL /* XXX */ },
#endif
    { 0,                                    0 }
};

/* Note: cc_err_xlate is NOT idempotent.  Don't call it multiple times.  */
static krb5_error_code cc_err_xlate(int err)
{
    const struct err_xlate *p;

#ifdef USE_CCAPI_V3
    if (err == ccNoError)
        return 0;
#else
    if (err == CC_NOERROR)
        return 0;
#endif

    for (p = err_xlate_table; p->cc_err; p++) {
        if (err == p->cc_err)
            return p->krb5_err;
    }

    return KRB5_FCC_INTERNAL;
}


#ifdef USE_CCAPI_V3

static krb5_error_code stdccv3_get_timeoffset (krb5_context in_context,
                                               cc_ccache_t  in_ccache)
{
    krb5_error_code err = 0;

    if (gCCVersion >= ccapi_version_5) {
        krb5_os_context os_ctx = (krb5_os_context) &in_context->os_context;
        cc_time_t time_offset = 0;

        err = cc_ccache_get_kdc_time_offset (in_ccache, cc_credentials_v5,
                                             &time_offset);

        if (!err) {
            os_ctx->time_offset = time_offset;
            os_ctx->usec_offset = 0;
            os_ctx->os_flags = ((os_ctx->os_flags & ~KRB5_OS_TOFFSET_TIME) |
                                KRB5_OS_TOFFSET_VALID);
        }

        if (err == ccErrTimeOffsetNotSet) {
            err = 0;  /* okay if there is no time offset */
        }
    }

    return err; /* Don't translate.  Callers will translate for us */
}

static krb5_error_code stdccv3_set_timeoffset (krb5_context in_context,
                                               cc_ccache_t  in_ccache)
{
    krb5_error_code err = 0;

    if (gCCVersion >= ccapi_version_5) {
        krb5_os_context os_ctx = (krb5_os_context) &in_context->os_context;

        if (!err && os_ctx->os_flags & KRB5_OS_TOFFSET_VALID) {
            err = cc_ccache_set_kdc_time_offset (in_ccache,
                                                 cc_credentials_v5,
                                                 os_ctx->time_offset);
        }
    }

    return err; /* Don't translate.  Callers will translate for us */
}

static krb5_error_code stdccv3_setup (krb5_context context,
                                      stdccCacheDataPtr ccapi_data)
{
    krb5_error_code err = 0;

    if (!err && !gCntrlBlock) {
        err = cc_initialize (&gCntrlBlock, ccapi_version_max, &gCCVersion, NULL);
    }

    if (!err && ccapi_data && !ccapi_data->NamedCache) {
        /* ccache has not been opened yet.  open it. */
        err = cc_context_open_ccache (gCntrlBlock, ccapi_data->cache_name,
                                      &ccapi_data->NamedCache);
    }

    if (!err && ccapi_data && ccapi_data->NamedCache) {
        err = stdccv3_get_timeoffset (context, ccapi_data->NamedCache);
    }

    return err; /* Don't translate.  Callers will translate for us */
}

/* krb5_stdcc_shutdown is exported; use the old name */
void krb5_stdcc_shutdown()
{
    if (gCntrlBlock) { cc_context_release(gCntrlBlock); }
    gCntrlBlock = NULL;
    gCCVersion = 0;
}

/*
 * -- generate_new --------------------------------
 *
 * create a new cache with a unique name, corresponds to creating a
 * named cache initialize the API here if we have to.
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_generate_new (krb5_context context, krb5_ccache *id )
{
    krb5_error_code err = 0;
    krb5_ccache newCache = NULL;
    stdccCacheDataPtr ccapi_data = NULL;
    cc_ccache_t ccache = NULL;
    cc_string_t ccstring = NULL;
    char *name = NULL;

    if (!err) {
        err = stdccv3_setup(context, NULL);
    }

    if (!err) {
        newCache = (krb5_ccache) malloc (sizeof (*newCache));
        if (!newCache) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        ccapi_data = (stdccCacheDataPtr) malloc (sizeof (*ccapi_data));
        if (!ccapi_data) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        err = cc_context_create_new_ccache (gCntrlBlock, cc_credentials_v5, "",
                                            &ccache);
    }

    if (!err) {
        err = stdccv3_set_timeoffset (context, ccache);
    }

    if (!err) {
        err = cc_ccache_get_name (ccache, &ccstring);
    }

    if (!err) {
        name = strdup (ccstring->data);
        if (!name) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        ccapi_data->cache_name = name;
        name = NULL; /* take ownership */

        ccapi_data->NamedCache = ccache;
        ccache = NULL; /* take ownership */

        newCache->ops = &krb5_cc_stdcc_ops;
        newCache->data = ccapi_data;
        ccapi_data = NULL; /* take ownership */

        /* return a pointer to the new cache */
        *id = newCache;
        newCache = NULL;
    }

    if (ccstring)   { cc_string_release (ccstring); }
    if (name)       { free (name); }
    if (ccache)     { cc_ccache_release (ccache); }
    if (ccapi_data) { free (ccapi_data); }
    if (newCache)   { free (newCache); }

    return cc_err_xlate (err);
}

/*
 * resolve
 *
 * create a new cache with the name stored in residual
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_resolve (krb5_context context, krb5_ccache *id , const char *residual )
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = NULL;
    krb5_ccache ccache = NULL;
    char *name = NULL;
    cc_string_t defname = NULL;

    if (id == NULL) { err = KRB5_CC_NOMEM; }

    if (!err) {
        err = stdccv3_setup (context, NULL);
    }

    if (!err) {
        ccapi_data = (stdccCacheDataPtr) malloc (sizeof (*ccapi_data));
        if (!ccapi_data) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        ccache = (krb5_ccache ) malloc (sizeof (*ccache));
        if (!ccache) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        if ((residual == NULL) || (strlen(residual) == 0)) {
            err = cc_context_get_default_ccache_name(gCntrlBlock, &defname);
            if (defname)
                residual = defname->data;
        }
    }

    if (!err) {
        name = strdup (residual);
        if (!name) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        err = cc_context_open_ccache (gCntrlBlock, residual,
                                      &ccapi_data->NamedCache);
        if (err == ccErrCCacheNotFound) {
            ccapi_data->NamedCache = NULL;
            err = 0; /* ccache just doesn't exist yet */
        }
    }

    if (!err) {
        ccapi_data->cache_name = name;
        name = NULL; /* take ownership */

        ccache->ops = &krb5_cc_stdcc_ops;
        ccache->data = ccapi_data;
        ccapi_data = NULL; /* take ownership */

        *id = ccache;
        ccache = NULL; /* take ownership */
    }

    if (ccache)     { free (ccache); }
    if (ccapi_data) { free (ccapi_data); }
    if (name)       { free (name); }
    if (defname)    { cc_string_release(defname); }

    return cc_err_xlate (err);
}

/*
 * initialize
 *
 * initialize the cache, check to see if one already exists for this
 * principal if not set our principal to this principal. This
 * searching enables ticket sharing
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_initialize (krb5_context context,
                         krb5_ccache id,
                         krb5_principal princ)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    char *name = NULL;
    cc_ccache_t ccache = NULL;

    if (id == NULL) { err = KRB5_CC_NOMEM; }

    if (!err) {
        err = stdccv3_setup (context, NULL);
    }

    if (!err) {
        err = krb5_unparse_name(context, princ, &name);
    }

    if (!err) {
        err = cc_context_create_ccache (gCntrlBlock, ccapi_data->cache_name,
                                        cc_credentials_v5, name,
                                        &ccache);
    }

    if (!err) {
        err = stdccv3_set_timeoffset (context, ccache);
    }

    if (!err) {
        if (ccapi_data->NamedCache) {
            err = cc_ccache_release (ccapi_data->NamedCache);
        }
        ccapi_data->NamedCache = ccache;
        ccache = NULL; /* take ownership */
        cache_changed ();
    }

    if (ccache) { cc_ccache_release (ccache); }
    if (name  ) { krb5_free_unparsed_name(context, name); }

    return cc_err_xlate(err);
}

/*
 * store
 *
 * store some credentials in our cache
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_store (krb5_context context, krb5_ccache id, krb5_creds *creds )
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    cc_credentials_union *cred_union = NULL;

    if (!err) {
        err = stdccv3_setup (context, ccapi_data);
    }

    if (!err) {
        /* copy the fields from the almost identical structures */
        err = copy_krb5_creds_to_cc_cred_union (context, creds, &cred_union);
    }

    if (!err) {
        err = cc_ccache_store_credentials (ccapi_data->NamedCache, cred_union);
    }

    if (!err) {
        cache_changed();
    }

    if (cred_union) { cred_union_release (cred_union); }

    return cc_err_xlate (err);
}

/*
 * start_seq_get
 *
 * begin an iterator call to get all of the credentials in the cache
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_start_seq_get (krb5_context context,
                            krb5_ccache id,
                            krb5_cc_cursor *cursor )
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    cc_credentials_iterator_t iterator = NULL;

    if (!err) {
        err = stdccv3_setup (context, ccapi_data);
    }

    if (!err) {
        err = cc_ccache_new_credentials_iterator(ccapi_data->NamedCache,
                                                 &iterator);
    }

    if (!err) {
        *cursor = iterator;
    }

    return cc_err_xlate (err);
}

/*
 * next cred
 *
 * - get the next credential in the cache as part of an iterator call
 * - this maps to call to cc_seq_fetch_creds
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_next_cred (krb5_context context,
                        krb5_ccache id,
                        krb5_cc_cursor *cursor,
                        krb5_creds *creds)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    cc_credentials_t credentials = NULL;
    cc_credentials_iterator_t iterator = *cursor;

    if (!iterator) { err = KRB5_CC_END; }

    if (!err) {
        err = stdccv3_setup (context, ccapi_data);
    }

    /* Note: CCAPI v3 ccaches can contain both v4 and v5 creds */
    while (!err) {
        err = cc_credentials_iterator_next (iterator, &credentials);

        if (!err && (credentials->data->version == cc_credentials_v5)) {
            copy_cc_cred_union_to_krb5_creds(context, credentials->data, creds);
            break;
        }
    }

    if (credentials) { cc_credentials_release (credentials); }
    if (err == ccIteratorEnd) {
        cc_credentials_iterator_release (iterator);
        *cursor = 0;
    }

    return cc_err_xlate (err);
}


/*
 * retrieve
 *
 * - try to find a matching credential in the cache
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_retrieve (krb5_context context,
                       krb5_ccache id,
                       krb5_flags whichfields,
                       krb5_creds *mcreds,
                       krb5_creds *creds)
{
    return k5_cc_retrieve_cred_default(context, id, whichfields, mcreds,
                                       creds);
}

/*
 *  end seq
 *
 * just free up the storage assoicated with the cursor (if we can)
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_end_seq_get (krb5_context context,
                          krb5_ccache id,
                          krb5_cc_cursor *cursor)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    cc_credentials_iterator_t iterator = *cursor;

    if (!iterator) { return 0; }

    if (!err) {
        err = stdccv3_setup (context, ccapi_data);
    }

    if (!err) {
        err = cc_credentials_iterator_release(iterator);
    }

    return cc_err_xlate(err);
}

/*
 * close
 *
 * - free our pointers to the NC
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_close(krb5_context context,
                   krb5_ccache id)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;

    if (!err) {
        err = stdccv3_setup (context, NULL);
    }

    if (!err) {
        if (ccapi_data) {
            if (ccapi_data->cache_name) {
                free (ccapi_data->cache_name);
            }
            if (ccapi_data->NamedCache) {
                err = cc_ccache_release (ccapi_data->NamedCache);
            }
            free (ccapi_data);
            id->data = NULL;
        }
        free (id);
    }

    return cc_err_xlate(err);
}

/*
 * destroy
 *
 * - free our storage and the cache
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_destroy (krb5_context context,
                      krb5_ccache id)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;

    if (!err) {
        err = stdccv3_setup(context, ccapi_data);
    }

    if (!err) {
        if (ccapi_data) {
            if (ccapi_data->cache_name) {
                free(ccapi_data->cache_name);
            }
            if (ccapi_data->NamedCache) {
                /* destroy the named cache */
                err = cc_ccache_destroy(ccapi_data->NamedCache);
                if (err == ccErrCCacheNotFound) {
                    err = 0; /* ccache maybe already destroyed */
                }
                cache_changed();
            }
            free(ccapi_data);
            id->data = NULL;
        }
        free(id);
    }

    return cc_err_xlate(err);
}

/*
 *  getname
 *
 * - return the name of the named cache
 */
const char * KRB5_CALLCONV
krb5_stdccv3_get_name (krb5_context context,
                       krb5_ccache id )
{
    stdccCacheDataPtr ccapi_data = id->data;

    if (!ccapi_data) {
        return NULL;
    } else {
        return (ccapi_data->cache_name);
    }
}


/* get_principal
 *
 * - return the principal associated with the named cache
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_get_principal (krb5_context context,
                            krb5_ccache id ,
                            krb5_principal *princ)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    cc_string_t name = NULL;

    if (!err) {
        err = stdccv3_setup(context, ccapi_data);
    }

    if (!err) {
        err = cc_ccache_get_principal (ccapi_data->NamedCache, cc_credentials_v5, &name);
    }

    if (!err) {
        err = krb5_parse_name (context, name->data, princ);
    } else {
        err = cc_err_xlate (err);
    }

    if (name) { cc_string_release (name); }

    return err;
}

/*
 * set_flags
 *
 * - currently a NOP since we don't store any flags in the NC
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_set_flags (krb5_context context,
                        krb5_ccache id,
                        krb5_flags flags)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;

    err = stdccv3_setup (context, ccapi_data);

    return cc_err_xlate (err);
}

/*
 * get_flags
 *
 * - currently a NOP since we don't store any flags in the NC
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_get_flags (krb5_context context,
                        krb5_ccache id,
                        krb5_flags *flags)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;

    err = stdccv3_setup (context, ccapi_data);

    return cc_err_xlate (err);
}

/*
 * remove
 *
 * - remove the specified credentials from the NC
 */
krb5_error_code KRB5_CALLCONV
krb5_stdccv3_remove (krb5_context context,
                     krb5_ccache id,
                     krb5_flags whichfields,
                     krb5_creds *in_creds)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    cc_credentials_iterator_t iterator = NULL;
    int found = 0;

    if (!err) {
        err = stdccv3_setup(context, ccapi_data);
    }


    if (!err) {
        err = cc_ccache_new_credentials_iterator(ccapi_data->NamedCache,
                                                 &iterator);
    }

    /* Note: CCAPI v3 ccaches can contain both v4 and v5 creds */
    while (!err && !found) {
        cc_credentials_t credentials = NULL;

        err = cc_credentials_iterator_next (iterator, &credentials);

        if (!err && (credentials->data->version == cc_credentials_v5)) {
            krb5_creds creds;

            err = copy_cc_cred_union_to_krb5_creds(context,
                                                   credentials->data, &creds);

            if (!err) {
                found = krb5int_cc_creds_match_request(context,
                                                       whichfields,
                                                       in_creds,
                                                       &creds);
                krb5_free_cred_contents (context, &creds);
            }

            if (!err && found) {
                err = cc_ccache_remove_credentials (ccapi_data->NamedCache, credentials);
            }
        }

        if (credentials) { cc_credentials_release (credentials); }
    }
    if (err == ccIteratorEnd) { err = ccErrCredentialsNotFound; }

    if (iterator) {
        err = cc_credentials_iterator_release(iterator);
    }

    if (!err) {
        cache_changed ();
    }

    return cc_err_xlate (err);
}

krb5_error_code KRB5_CALLCONV
krb5_stdccv3_ptcursor_new(krb5_context context,
                          krb5_cc_ptcursor *cursor)
{
    krb5_error_code err = 0;
    krb5_cc_ptcursor ptcursor = NULL;
    cc_ccache_iterator_t iterator = NULL;

    ptcursor = malloc(sizeof(*ptcursor));
    if (ptcursor == NULL) {
        err = ccErrNoMem;
    }
    else {
        memset(ptcursor, 0, sizeof(*ptcursor));
    }

    if (!err) {
        err = stdccv3_setup(context, NULL);
    }
    if (!err) {
        ptcursor->ops = &krb5_cc_stdcc_ops;
        err = cc_context_new_ccache_iterator(gCntrlBlock, &iterator);
    }

    if (!err) {
        ptcursor->data = iterator;
    }

    if (err) {
        if (ptcursor) { krb5_stdccv3_ptcursor_free(context, &ptcursor); }
        // krb5_stdccv3_ptcursor_free sets ptcursor to NULL for us
    }

    *cursor = ptcursor;

    return cc_err_xlate(err);
}

krb5_error_code KRB5_CALLCONV
krb5_stdccv3_ptcursor_next(
    krb5_context context,
    krb5_cc_ptcursor cursor,
    krb5_ccache *ccache)
{
    krb5_error_code err = 0;
    cc_ccache_iterator_t iterator = NULL;

    krb5_ccache newCache = NULL;
    stdccCacheDataPtr ccapi_data = NULL;
    cc_ccache_t ccCache = NULL;
    cc_string_t ccstring = NULL;
    char *name = NULL;

    if (!cursor || !cursor->data) {
        err = ccErrInvalidContext;
    }

    *ccache = NULL;

    if (!err) {
        newCache = (krb5_ccache) malloc (sizeof (*newCache));
        if (!newCache) { err = ccErrNoMem; }
    }

    if (!err) {
        ccapi_data = (stdccCacheDataPtr) malloc (sizeof (*ccapi_data));
        if (!ccapi_data) { err = ccErrNoMem; }
    }

    if (!err) {
        iterator = cursor->data;
        err = cc_ccache_iterator_next(iterator, &ccCache);
    }

    if (!err) {
        err = cc_ccache_get_name (ccCache, &ccstring);
    }

    if (!err) {
        name = strdup (ccstring->data);
        if (!name) { err = ccErrNoMem; }
    }

    if (!err) {
        ccapi_data->cache_name = name;
        name = NULL; /* take ownership */

        ccapi_data->NamedCache = ccCache;
        ccCache = NULL; /* take ownership */

        newCache->ops = &krb5_cc_stdcc_ops;
        newCache->data = ccapi_data;
        ccapi_data = NULL; /* take ownership */

        /* return a pointer to the new cache */
        *ccache = newCache;
        newCache = NULL;
    }

    if (name)       { free (name); }
    if (ccstring)   { cc_string_release (ccstring); }
    if (ccCache)    { cc_ccache_release (ccCache); }
    if (ccapi_data) { free (ccapi_data); }
    if (newCache)   { free (newCache); }

    if (err == ccIteratorEnd) {
        err = ccNoError;
    }

    return cc_err_xlate(err);
}

krb5_error_code KRB5_CALLCONV
krb5_stdccv3_ptcursor_free(
    krb5_context context,
    krb5_cc_ptcursor *cursor)
{
    if (*cursor != NULL) {
        if ((*cursor)->data != NULL) {
            cc_ccache_iterator_release((cc_ccache_iterator_t)((*cursor)->data));
        }
        free(*cursor);
        *cursor = NULL;
    }
    return 0;
}

krb5_error_code KRB5_CALLCONV krb5_stdccv3_last_change_time
(krb5_context context, krb5_ccache id,
 krb5_timestamp *change_time)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;
    cc_time_t ccapi_change_time = 0;

    *change_time = 0;

    if (!err) {
        err = stdccv3_setup(context, ccapi_data);
    }
    if (!err) {
        err = cc_ccache_get_change_time (ccapi_data->NamedCache, &ccapi_change_time);
    }
    if (!err) {
        *change_time = ccapi_change_time;
    }

    return cc_err_xlate (err);
}

krb5_error_code KRB5_CALLCONV krb5_stdccv3_lock
(krb5_context context, krb5_ccache id)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;

    if (!err) {
        err = stdccv3_setup(context, ccapi_data);
    }
    if (!err) {
        err = cc_ccache_lock(ccapi_data->NamedCache, cc_lock_write, cc_lock_block);
    }
    return cc_err_xlate(err);
}

krb5_error_code KRB5_CALLCONV krb5_stdccv3_unlock
(krb5_context context, krb5_ccache id)
{
    krb5_error_code err = 0;
    stdccCacheDataPtr ccapi_data = id->data;

    if (!err) {
        err = stdccv3_setup(context, ccapi_data);
    }
    if (!err) {
        err = cc_ccache_unlock(ccapi_data->NamedCache);
    }
    return cc_err_xlate(err);
}

krb5_error_code KRB5_CALLCONV krb5_stdccv3_context_lock
(krb5_context context)
{
    krb5_error_code err = 0;

    if (!err && !gCntrlBlock) {
        err = cc_initialize (&gCntrlBlock, ccapi_version_max, &gCCVersion, NULL);
    }
    if (!err) {
        err = cc_context_lock(gCntrlBlock, cc_lock_write, cc_lock_block);
    }
    return cc_err_xlate(err);
}

krb5_error_code KRB5_CALLCONV krb5_stdccv3_context_unlock
(krb5_context context)
{
    krb5_error_code err = 0;

    if (!err && !gCntrlBlock) {
        err = cc_initialize (&gCntrlBlock, ccapi_version_max, &gCCVersion, NULL);
    }
    if (!err) {
        err = cc_context_unlock(gCntrlBlock);
    }
    return cc_err_xlate(err);
}

krb5_error_code KRB5_CALLCONV krb5_stdccv3_switch_to
(krb5_context context, krb5_ccache id)
{
    krb5_error_code retval;
    stdccCacheDataPtr ccapi_data = id->data;
    int err;

    retval = stdccv3_setup(context, ccapi_data);
    if (retval)
        return cc_err_xlate(retval);

    err = cc_ccache_set_default(ccapi_data->NamedCache);
    return cc_err_xlate(err);
}

#else /* !USE_CCAPI_V3 */

static krb5_error_code stdcc_setup(krb5_context context,
                                   stdccCacheDataPtr ccapi_data)
{
    int     err;

    /* make sure the API has been intialized */
    if (gCntrlBlock == NULL) {
#ifdef CC_API_VER2
        err = cc_initialize(&gCntrlBlock, CC_API_VER_2, NULL, NULL);
#else
        err = cc_initialize(&gCntrlBlock, CC_API_VER_1, NULL, NULL);
#endif
        if (err != CC_NOERROR)
            return cc_err_xlate(err);
    }

    /*
     * No ccapi_data structure, so we don't need to make sure the
     * ccache exists.
     */
    if (!ccapi_data)
        return 0;

    /*
     * The ccache already exists
     */
    if (ccapi_data->NamedCache)
        return 0;

    err = cc_open(gCntrlBlock, ccapi_data->cache_name,
                  CC_CRED_V5, 0L, &ccapi_data->NamedCache);
    if (err == CC_NOTFOUND)
        err = CC_NO_EXIST;
    if (err == CC_NOERROR)
        return 0;

    ccapi_data->NamedCache = NULL;
    return cc_err_xlate(err);
}

void krb5_stdcc_shutdown()
{
    if (gCntrlBlock)
        cc_shutdown(&gCntrlBlock);
    gCntrlBlock = NULL;
}

/*
 * -- generate_new --------------------------------
 *
 * create a new cache with a unique name, corresponds to creating a
 * named cache iniitialize the API here if we have to.
 */
krb5_error_code KRB5_CALLCONV  krb5_stdcc_generate_new
(krb5_context context, krb5_ccache *id )
{
    krb5_ccache             newCache = NULL;
    krb5_error_code         retval;
    stdccCacheDataPtr       ccapi_data = NULL;
    char                    *name = NULL;
    cc_time_t               change_time;
    int                     err;

    if ((retval = stdcc_setup(context, NULL)))
        return retval;

    retval = KRB5_CC_NOMEM;
    if (!(newCache = (krb5_ccache) malloc(sizeof(struct _krb5_ccache))))
        goto errout;
    if (!(ccapi_data = (stdccCacheDataPtr)malloc(sizeof(stdccCacheData))))
        goto errout;
    if (!(name = malloc(256)))
        goto errout;

    /* create a unique name */
    cc_get_change_time(gCntrlBlock, &change_time);
    snprintf(name, 256, "gen_new_cache%d", change_time);

    /* create the new cache */
    err = cc_create(gCntrlBlock, name, name, CC_CRED_V5, 0L,
                    &ccapi_data->NamedCache);
    if (err != CC_NOERROR) {
        retval = cc_err_xlate(err);
        goto errout;
    }

    /* setup some fields */
    newCache->ops = &krb5_cc_stdcc_ops;
    newCache->data = ccapi_data;
    ccapi_data->cache_name = name;

    /* return a pointer to the new cache */
    *id = newCache;

    return 0;

errout:
    if (newCache)
        free(newCache);
    if (ccapi_data)
        free(ccapi_data);
    if (name)
        free(name);
    return retval;
}

/*
 * resolve
 *
 * create a new cache with the name stored in residual
 */
krb5_error_code KRB5_CALLCONV  krb5_stdcc_resolve
(krb5_context context, krb5_ccache *id , const char *residual )
{
    krb5_ccache             newCache = NULL;
    stdccCacheDataPtr       ccapi_data = NULL;
    int                     err;
    krb5_error_code         retval;
    char                    *cName = NULL;

    if ((retval = stdcc_setup(context, NULL)))
        return retval;

    retval = KRB5_CC_NOMEM;
    if (!(newCache = (krb5_ccache) malloc(sizeof(struct _krb5_ccache))))
        goto errout;

    if (!(ccapi_data = (stdccCacheDataPtr)malloc(sizeof(stdccCacheData))))
        goto errout;

    if (!(cName = strdup(residual)))
        goto errout;

    newCache->ops = &krb5_cc_stdcc_ops;
    newCache->data = ccapi_data;
    ccapi_data->cache_name = cName;

    err = cc_open(gCntrlBlock, cName, CC_CRED_V5, 0L,
                  &ccapi_data->NamedCache);
    if (err != CC_NOERROR) {
        ccapi_data->NamedCache = NULL;
        if (err != CC_NO_EXIST) {
            retval = cc_err_xlate(err);
            goto errout;
        }
    }

    /* return new cache structure */
    *id = newCache;

    return 0;

errout:
    if (newCache)
        free(newCache);
    if (ccapi_data)
        free(ccapi_data);
    if (cName)
        free(cName);
    return retval;
}

/*
 * initialize
 *
 * initialize the cache, check to see if one already exists for this
 * principal if not set our principal to this principal. This
 * searching enables ticket sharing
 */
krb5_error_code KRB5_CALLCONV  krb5_stdcc_initialize
(krb5_context context, krb5_ccache id,  krb5_principal princ)
{
    stdccCacheDataPtr       ccapi_data = NULL;
    int                     err;
    char                    *cName = NULL;
    krb5_error_code         retval;

    if ((retval = stdcc_setup(context, NULL)))
        return retval;

    /* test id for null */
    if (id == NULL) return KRB5_CC_NOMEM;

    if ((retval = krb5_unparse_name(context, princ, &cName)))
        return retval;

    ccapi_data = id->data;


    if (ccapi_data->NamedCache)
        cc_close(gCntrlBlock, &ccapi_data->NamedCache);

    err = cc_create(gCntrlBlock, ccapi_data->cache_name, cName,
                    CC_CRED_V5, 0L, &ccapi_data->NamedCache);
    if (err != CC_NOERROR) {
        krb5_free_unparsed_name(context, cName);
        return cc_err_xlate(err);
    }

#if 0
    /*
     * Some implementations don't set the principal name
     * correctly, so we force set it to the correct value.
     */
    err = cc_set_principal(gCntrlBlock, ccapi_data->NamedCache,
                           CC_CRED_V5, cName);
#endif
    krb5_free_unparsed_name(context, cName);
    cache_changed();

    return cc_err_xlate(err);
}

/*
 * store
 *
 * store some credentials in our cache
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_store
(krb5_context context, krb5_ccache id, krb5_creds *creds )
{
    krb5_error_code retval;
    stdccCacheDataPtr       ccapi_data = id->data;
    cred_union *cu = NULL;
    int err;

    if ((retval = stdcc_setup(context, ccapi_data)))
        return retval;

    /* copy the fields from the almost identical structures */
    dupK5toCC(context, creds, &cu);

    /*
     * finally store the credential
     * store will copy (that is duplicate) everything
     */
    err = cc_store(gCntrlBlock,
                   ((stdccCacheDataPtr)(id->data))->NamedCache, *cu);
    if (err != CC_NOERROR)
        return cc_err_xlate(err);

    /* free the cred union using our local version of cc_free_creds()
       since we allocated it locally */
    err = krb5int_free_cc_cred_union(&cu);

    cache_changed();
    return err;
}

/*
 * start_seq_get
 *
 * begin an iterator call to get all of the credentials in the cache
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_start_seq_get
(krb5_context context, krb5_ccache id , krb5_cc_cursor *cursor )
{
    stdccCacheDataPtr       ccapi_data = id->data;
    krb5_error_code retval;
    int     err;
    ccache_cit      *iterator;

    if ((retval = stdcc_setup(context, ccapi_data)))
        return retval;

#ifdef CC_API_VER2
    err = cc_seq_fetch_creds_begin(gCntrlBlock, ccapi_data->NamedCache,
                                   &iterator);
    if (err != CC_NOERROR)
        return cc_err_xlate(err);
    *cursor = iterator;
#else
    /* all we have to do is initialize the cursor */
    *cursor = NULL;
#endif
    return 0;
}

/*
 * next cred
 *
 * - get the next credential in the cache as part of an iterator call
 * - this maps to call to cc_seq_fetch_creds
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_next_cred
(krb5_context context, krb5_ccache id,  krb5_cc_cursor *cursor,
 krb5_creds *creds)
{
    krb5_error_code retval;
    stdccCacheDataPtr       ccapi_data = id->data;
    int err;
    cred_union *credU = NULL;
    ccache_cit      *iterator;

    if ((retval = stdcc_setup(context, ccapi_data)))
        return retval;

#ifdef CC_API_VER2
    iterator = *cursor;
    if (iterator == 0)
        return KRB5_CC_END;
    err = cc_seq_fetch_creds_next(gCntrlBlock, &credU, iterator);

    if (err == CC_END) {
        cc_seq_fetch_creds_end(gCntrlBlock, &iterator);
        *cursor = 0;
    }
#else
    err = cc_seq_fetch_creds(gCntrlBlock, ccapi_data->NamedCache,
                             &credU, (ccache_cit **)cursor);
#endif

    if (err != CC_NOERROR)
        return cc_err_xlate(err);

    /* copy data    (with translation) */
    dupCCtoK5(context, credU->cred.pV5Cred, creds);

    /* free our version of the cred - okay to use cc_free_creds() here
       because we got it from the CCache library */
    cc_free_creds(gCntrlBlock, &credU);

    return 0;
}


/*
 * retreive
 *
 * - try to find a matching credential in the cache
 */
#if 0
krb5_error_code KRB5_CALLCONV krb5_stdcc_retrieve
(krb5_context context,
 krb5_ccache id,
 krb5_flags whichfields,
 krb5_creds *mcreds,
 krb5_creds *creds )
{
    krb5_error_code retval;
    krb5_cc_cursor curs = NULL;
    krb5_creds *fetchcreds;

    if ((retval = stdcc_setup(context, NULL)))
        return retval;

    fetchcreds = (krb5_creds *)malloc(sizeof(krb5_creds));
    if (fetchcreds == NULL) return KRB5_CC_NOMEM;

    /* we're going to use the iterators */
    krb5_stdcc_start_seq_get(context, id, &curs);

    while (!krb5_stdcc_next_cred(context, id, &curs, fetchcreds)) {
        /*
         * look at each credential for a match
         * use this match routine since it takes the
         * whichfields and the API doesn't
         */
        if (stdccCredsMatch(context, fetchcreds,
                            mcreds, whichfields)) {
            /* we found it, copy and exit */
            *creds = *fetchcreds;
            krb5_stdcc_end_seq_get(context, id, &curs);
            return 0;
        }
        /* free copy allocated by next_cred */
        krb5_free_cred_contents(context, fetchcreds);
    }

    /* no luck, end get and exit */
    krb5_stdcc_end_seq_get(context, id, &curs);

    /* we're not using this anymore so we should get rid of it! */
    free(fetchcreds);

    return KRB5_CC_NOTFOUND;
}
#else

krb5_error_code KRB5_CALLCONV
krb5_stdcc_retrieve(context, id, whichfields, mcreds, creds)
    krb5_context context;
    krb5_ccache id;
    krb5_flags whichfields;
    krb5_creds *mcreds;
    krb5_creds *creds;
{
    return k5_cc_retrieve_cred_default(context, id, whichfields, mcreds,
                                       creds);
}

#endif

/*
 *  end seq
 *
 * just free up the storage assoicated with the cursor (if we could)
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_end_seq_get
(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor)
{
    krb5_error_code         retval;
    stdccCacheDataPtr       ccapi_data = NULL;
    int                     err;
#ifndef CC_API_VER2
    cred_union              *credU = NULL;
#endif

    ccapi_data = id->data;

    if ((retval = stdcc_setup(context, ccapi_data)))
        return retval;

    if (*cursor == NULL)
        return 0;

#ifdef CC_API_VER2
    err = cc_seq_fetch_creds_end(gCntrlBlock, (ccache_cit **)cursor);
    if (err != CC_NOERROR)
        return cc_err_xlate(err);
#else
    /*
     * Finish calling cc_seq_fetch_creds to clear out the cursor
     */
    while (*cursor) {
        err = cc_seq_fetch_creds(gCntrlBlock, ccapi_data->NamedCache,
                                 &credU, (ccache_cit **)cursor);
        if (err)
            break;

        /* okay to call cc_free_creds() here because we got credU from CCache lib */
        cc_free_creds(gCntrlBlock, &credU);
    }
#endif

    return(0);
}

/*
 * close
 *
 * - free our pointers to the NC
 */
krb5_error_code KRB5_CALLCONV
krb5_stdcc_close(krb5_context context, krb5_ccache id)
{
    krb5_error_code retval;
    stdccCacheDataPtr       ccapi_data = id->data;

    if ((retval = stdcc_setup(context, NULL)))
        return retval;

    /* free it */

    if (ccapi_data) {
        if (ccapi_data->cache_name)
            free(ccapi_data->cache_name);
        if (ccapi_data->NamedCache)
            cc_close(gCntrlBlock, &ccapi_data->NamedCache);
        free(ccapi_data);
        id->data = NULL;
    }
    free(id);

    return 0;
}

/*
 * destroy
 *
 * - free our storage and the cache
 */
krb5_error_code KRB5_CALLCONV
krb5_stdcc_destroy (krb5_context context, krb5_ccache id)
{
    int err;
    krb5_error_code retval;
    stdccCacheDataPtr       ccapi_data = id->data;

    if ((retval = stdcc_setup(context, ccapi_data))) {
        return retval;
    }

    /* free memory associated with the krb5_ccache */
    if (ccapi_data) {
        if (ccapi_data->cache_name)
            free(ccapi_data->cache_name);
        if (ccapi_data->NamedCache) {
            /* destroy the named cache */
            err = cc_destroy(gCntrlBlock, &ccapi_data->NamedCache);
            retval = cc_err_xlate(err);
            cache_changed();
        }
        free(ccapi_data);
        id->data = NULL;
    }
    free(id);

    /* If the cache does not exist when we tried to destroy it,
       that's fine.  That means someone else destryoed it since
       we resolved it. */
    if (retval == KRB5_FCC_NOFILE)
        return 0;
    return retval;
}

/*
 *  getname
 *
 * - return the name of the named cache
 */
const char * KRB5_CALLCONV krb5_stdcc_get_name
(krb5_context context, krb5_ccache id )
{
    stdccCacheDataPtr       ccapi_data = id->data;

    if (!ccapi_data)
        return 0;

    return (ccapi_data->cache_name);
}


/* get_principal
 *
 * - return the principal associated with the named cache
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_get_principal
(krb5_context context, krb5_ccache id , krb5_principal *princ)
{
    int                     err;
    char                    *name = NULL;
    stdccCacheDataPtr       ccapi_data = id->data;
    krb5_error_code         retval;

    if ((retval = stdcc_setup(context, ccapi_data)))
        return retval;

    /* another wrapper */
    err = cc_get_principal(gCntrlBlock, ccapi_data->NamedCache,
                           &name);

    if (err != CC_NOERROR)
        return cc_err_xlate(err);

    /* turn it into a krb principal */
    err = krb5_parse_name(context, name, princ);

    cc_free_principal(gCntrlBlock, &name);

    return err;
}

/*
 * set_flags
 *
 * - currently a NOP since we don't store any flags in the NC
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_set_flags
(krb5_context context, krb5_ccache id , krb5_flags flags)
{
    stdccCacheDataPtr       ccapi_data = id->data;
    krb5_error_code         retval;

    if ((retval = stdcc_setup(context, ccapi_data)))
        return retval;

    return 0;
}

/*
 * get_flags
 *
 * - currently a NOP since we don't store any flags in the NC
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_get_flags
(krb5_context context, krb5_ccache id , krb5_flags *flags)
{
    stdccCacheDataPtr       ccapi_data = id->data;
    krb5_error_code         retval;

    if ((retval = stdcc_setup(context, ccapi_data)))
        return retval;

    return 0;
}

/*
 * remove
 *
 * - remove the specified credentials from the NC
 */
krb5_error_code KRB5_CALLCONV krb5_stdcc_remove
(krb5_context context, krb5_ccache id,
 krb5_flags flags, krb5_creds *creds)
{
    cred_union *cu = NULL;
    int err;
    stdccCacheDataPtr       ccapi_data = id->data;
    krb5_error_code         retval;

    if ((retval = stdcc_setup(context, ccapi_data))) {
        if (retval == KRB5_FCC_NOFILE)
            return 0;
        return retval;
    }

    /* convert to a cred union */
    dupK5toCC(context, creds, &cu);

    /* remove it */
    err = cc_remove_cred(gCntrlBlock, ccapi_data->NamedCache, *cu);
    if (err != CC_NOERROR)
        return cc_err_xlate(err);

    /* free the cred union using our local version of cc_free_creds()
       since we allocated it locally */
    err = krb5int_free_cc_cred_union(&cu);
    cache_changed();
    if (err != CC_NOERROR)
        return cc_err_xlate(err);

    return 0;
}
#endif /* !USE_CCAPI_V3 */

#endif /* defined(_WIN32) || defined(USE_CCAPI) */
