/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/cc_api_macos.c - Native MacOS X ccache code */
/*
 * Copyright (C) 2022 United States Government as represented by the
 * Secretary of the Navy.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This ccache module provides compatibility with the default native ccache
 * type for macOS, by linking against the native Kerberos framework and calling
 * the CCAPI stubs.  Due to workarounds for specific behaviors of the CCAPI
 * stubs, this implementation is separate from the API ccache implementation
 * used on Windows.
 */

#include "k5-int.h"
#include "cc-int.h"
#include "ccapi_util.h"
#include <CredentialsCache.h>

#ifdef USE_CCAPI_MACOS

#include <sys/utsname.h>
#include <xpc/xpc.h>

const krb5_cc_ops krb5_api_macos_ops;

struct api_macos_cache_data {
    char *residual;
    cc_context_t cc_context;
    cc_ccache_t cache;
};

struct api_macos_ptcursor {
    krb5_boolean first;
    char *primary;
    cc_context_t cc_context;
    cc_ccache_iterator_t iter;
};

/* Map a CCAPI error code to a com_err code. */
static krb5_error_code
ccerr2mit(uint32_t err)
{
    switch (err) {
    case ccNoError:
        return 0;
    case ccIteratorEnd:
        return KRB5_CC_END;
    case ccErrNoMem:
        return ENOMEM;
    case ccErrCCacheNotFound:
        return KRB5_FCC_NOFILE;
    default:
        return KRB5_FCC_INTERNAL;
    }
}

/* Construct a ccache handle for residual.  Use cc_context if it is not null,
 * or initialize a new one if it is. */
static krb5_error_code
make_cache(const char *residual, cc_context_t cc_context,
           krb5_ccache *ccache_out)
{
    krb5_ccache cache = NULL;
    char *residual_copy = NULL;
    struct api_macos_cache_data *data = NULL;
    uint32_t err;

    *ccache_out = NULL;

    if (cc_context == NULL) {
        err = cc_initialize(&cc_context, ccapi_version_max, NULL, NULL);
        if (err != ccNoError)
            return KRB5_FCC_INTERNAL;
    }

    cache = malloc(sizeof(*cache));
    if (cache == NULL)
        goto oom;

    data = calloc(1, sizeof(*data));
    if (data == NULL)
        goto oom;

    residual_copy = strdup(residual);
    if (residual_copy == NULL)
        goto oom;

    data->residual = residual_copy;
    data->cc_context = cc_context;
    cache->ops = &krb5_api_macos_ops;
    cache->data = data;
    cache->magic = KV5M_CCACHE;
    *ccache_out = cache;
    return 0;

oom:
    free(cache);
    free(data);
    free(residual_copy);
    if (cc_context)
        cc_context_release(cc_context);
    return ENOMEM;
}

static uint32_t
open_cache(struct api_macos_cache_data *data)
{
    if (data->cache != NULL)
        return ccNoError;
    return cc_context_open_ccache(data->cc_context, data->residual,
                                  &data->cache);
}

static const char *
api_macos_get_name(krb5_context context, krb5_ccache ccache)
{
    struct api_macos_cache_data *data = ccache->data;

    return data->residual;
}

/*
 * We would like to use cc_context_get_default_ccache_name() for this, but that
 * doesn't work on macOS if the default cache name is set by the environment or
 * configuration.  So we have to do what the underlying macOS Heimdal API cache
 * type does to fetch the primary name.
 *
 * For macOS 11 (Darwin 20) and later, implement just enough of the XCACHE
 * protocol to fetch the primary UUID.  For earlier versions, query the KCM
 * daemon.
 */
static krb5_error_code
get_primary_name(krb5_context context, char **name_out)
{
    krb5_error_code ret;
    xpc_connection_t conn = NULL;
    xpc_object_t request = NULL, reply = NULL;
    const uint8_t *uuid;
    uint64_t flags = XPC_CONNECTION_MACH_SERVICE_PRIVILEGED;
    char uuidstr[37], *end;
    struct utsname un;
    long release;

    *name_out = NULL;

    if (uname(&un) == 0) {
        release = strtol(un.release, &end, 10);
        if (end != un.release && release < 20) {
            /* Query the KCM daemon for macOS 10 and earlier. */
            ret = k5_kcm_primary_name(context, name_out);
            goto cleanup;
        }
    }

    conn = xpc_connection_create_mach_service("com.apple.GSSCred", NULL,
                                              flags);
    if (conn == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    xpc_connection_set_event_handler(conn, ^(xpc_object_t o){ ; });
    xpc_connection_resume(conn);

    request = xpc_dictionary_create(NULL, NULL, 0);
    if (request == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    xpc_dictionary_set_string(request, "command", "default");
    xpc_dictionary_set_string(request, "mech", "kHEIMTypeKerberos");

    reply = xpc_connection_send_message_with_reply_sync(conn, request);
    if (reply == NULL || xpc_get_type(reply) == XPC_TYPE_ERROR) {
        ret = KRB5_CC_IO;
        goto cleanup;
    }

    uuid = xpc_dictionary_get_uuid(reply, "default");
    if (uuid == NULL) {
        ret = KRB5_CC_IO;
        goto cleanup;
    }
    uuid_unparse(uuid, uuidstr);

    *name_out = strdup(uuidstr);
    ret = (*name_out == NULL) ? ENOMEM : 0;

cleanup:
    if (request != NULL)
        xpc_release(request);
    if (reply != NULL)
        xpc_release(reply);
    if (conn != NULL)
        xpc_release(conn);
    return ret;
}

static krb5_error_code
api_macos_resolve(krb5_context context, krb5_ccache *cache_out,
                  const char *residual)
{
    krb5_error_code ret;
    char *primary = NULL;

    if (*residual == '\0') {
        ret = get_primary_name(context, &primary);
        if (ret)
            return ret;
        residual = primary;
    }
    ret = make_cache(residual, NULL, cache_out);
    free(primary);
    return ret;
}

static krb5_error_code
api_macos_gen_new(krb5_context context, krb5_ccache *cache_out)
{
    krb5_error_code ret;
    uint32_t err;
    cc_context_t cc_context = NULL;
    cc_ccache_t cc_ccache = NULL;
    cc_string_t cachename = NULL;
    struct api_macos_cache_data *data;

    *cache_out = NULL;

    err = cc_initialize(&cc_context, ccapi_version_max, NULL, NULL);
    if (err)
        goto cleanup;

    err = cc_context_create_new_ccache(cc_context, cc_credentials_v5, "",
                                       &cc_ccache);
    if (err)
        goto cleanup;

    err = cc_ccache_get_name(cc_ccache, &cachename);
    if (err)
        goto cleanup;

    ret = make_cache(cachename->data, cc_context, cache_out);
    cc_context = NULL;
    if (!ret) {
        data = (*cache_out)->data;
        data->cache = cc_ccache;
        cc_ccache = NULL;
    }

cleanup:
    if (cc_context != NULL)
        cc_context_release(cc_context);
    if (cc_ccache != NULL)
        cc_ccache_release(cc_ccache);
    return err ? KRB5_FCC_INTERNAL : 0;
}

static krb5_error_code
api_macos_initialize(krb5_context context, krb5_ccache cache,
                     krb5_principal princ)
{
    krb5_error_code ret;
    struct api_macos_cache_data *data = cache->data;
    uint32_t err;
    char *princstr = NULL, *prefix_name = NULL;

    /* Apple's cc_context_create_ccache() requires a name with type prefix. */
    if (asprintf(&prefix_name, "API:%s", data->residual) < 0)
        return ENOMEM;

    ret = krb5_unparse_name(context, princ, &princstr);
    if (ret) {
        free(prefix_name);
        return ret;
    }

    if (data->cache != NULL) {
        cc_ccache_release(data->cache);
        data->cache = NULL;
    }

    err = cc_context_create_ccache(data->cc_context, prefix_name,
                                   cc_credentials_v5, princstr,
                                   &data->cache);
    krb5_free_unparsed_name(context, princstr);
    free(prefix_name);
    return ccerr2mit(err);
}

static krb5_error_code
api_macos_close(krb5_context context, krb5_ccache cache)
{
    struct api_macos_cache_data *data = cache->data;

    if (data->cache != NULL)
        cc_ccache_release(data->cache);
    cc_context_release(data->cc_context);
    free(data->residual);
    free(data);
    free(cache);
    return 0;
}

static krb5_error_code
api_macos_destroy(krb5_context context, krb5_ccache cache)
{
    struct api_macos_cache_data *data = cache->data;

    open_cache(data);
    if (data->cache != NULL) {
        cc_ccache_destroy(data->cache);
        data->cache = NULL;
    }
    return api_macos_close(context, cache);
}

static krb5_error_code
api_macos_store(krb5_context context, krb5_ccache cache, krb5_creds *creds)
{
    struct api_macos_cache_data *data = cache->data;
    cc_credentials_union *c_un = NULL;
    krb5_error_code ret;
    uint32_t err;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    ret = k5_krb5_to_ccapi_creds(context, creds, &c_un);
    if (ret)
        return ret;
    err = cc_ccache_store_credentials(data->cache, c_un);
    k5_release_ccapi_cred(c_un);
    return ccerr2mit(err);
}

static krb5_error_code
api_macos_retrieve(krb5_context context, krb5_ccache cache,
                   krb5_flags whichfields, krb5_creds *mcreds,
                   krb5_creds *creds)
{
    return k5_cc_retrieve_cred_default(context, cache, whichfields,
                                       mcreds, creds);
}

static krb5_error_code
api_macos_get_princ(krb5_context context, krb5_ccache cache,
                    krb5_principal *princ)
{
    struct api_macos_cache_data *data = cache->data;
    krb5_error_code ret;
    uint32_t err;
    cc_string_t outprinc;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    err = cc_ccache_get_principal(data->cache, cc_credentials_v5, &outprinc);
    if (err)
        return ccerr2mit(err);
    ret = krb5_parse_name(context, outprinc->data, princ);
    cc_string_release(outprinc);
    return ret;
}

static krb5_error_code
api_macos_start_seq_get(krb5_context context, krb5_ccache cache,
                        krb5_cc_cursor *cursor)
{
    struct api_macos_cache_data *data = cache->data;
    uint32_t err;
    cc_credentials_iterator_t iter;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    err = cc_ccache_new_credentials_iterator(data->cache, &iter);
    if (err)
        return ccerr2mit(err);

    *cursor = (krb5_cc_cursor)iter;
    return 0;
}

static krb5_error_code
api_macos_next_cred(krb5_context context, krb5_ccache cache,
                    krb5_cc_cursor *cursor, krb5_creds *creds)
{
    struct api_macos_cache_data *data = cache->data;
    uint32_t err;
    krb5_error_code ret;
    cc_credentials_iterator_t iter = (cc_credentials_iterator_t) *cursor;
    cc_credentials_t acreds;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    err = cc_credentials_iterator_next(iter, &acreds);
    if (!err) {
        ret = k5_ccapi_to_krb5_creds(context, acreds->data, creds);
        cc_credentials_release(acreds);
    } else {
        ret = ccerr2mit(err);
    }
    return ret;
}

static krb5_error_code
api_macos_end_seq_get(krb5_context context, krb5_ccache cache,
                      krb5_cc_cursor *cursor)
{
    cc_credentials_iterator_t iter = *cursor;

    cc_credentials_iterator_release(iter);
    *cursor = NULL;
    return 0;
}

static krb5_error_code
api_macos_remove_cred(krb5_context context, krb5_ccache cache,
                      krb5_flags flags, krb5_creds *creds)
{
    struct api_macos_cache_data *data = cache->data;
    uint32_t err;
    krb5_error_code ret = 0;
    cc_credentials_iterator_t iter = NULL;
    cc_credentials_t acreds;
    krb5_creds mcreds;
    krb5_boolean match;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    err = cc_ccache_new_credentials_iterator(data->cache, &iter);
    if (err)
        return ccerr2mit(err);

    for (;;) {
        err = cc_credentials_iterator_next(iter, &acreds);
        if (err)
            break;

        ret = k5_ccapi_to_krb5_creds(context, acreds->data, &mcreds);
        if (ret) {
            cc_credentials_release(acreds);
            break;
        }

        match = krb5int_cc_creds_match_request(context, flags, creds, &mcreds);
        krb5_free_cred_contents(context, &mcreds);
        if (match)
            err = cc_ccache_remove_credentials(data->cache, acreds);
        cc_credentials_release(acreds);
        if (err)
            break;
    }

    cc_credentials_iterator_release(iter);

    if (ret)
        return ret;
    if (err != ccIteratorEnd)
        return ccerr2mit(err);
    return 0;
}

static krb5_error_code
api_macos_set_flags(krb5_context context, krb5_ccache cache, krb5_flags flags)
{
    return 0;
}

static krb5_error_code
api_macos_get_flags(krb5_context context, krb5_ccache cache, krb5_flags *flags)
{
    *flags = 0;
    return 0;
}

static krb5_error_code
api_macos_ptcursor_new(krb5_context context, krb5_cc_ptcursor *ptcursor_out)
{
    krb5_cc_ptcursor ptcursor = NULL;
    struct api_macos_ptcursor *apt = NULL;

    apt = malloc(sizeof(*apt));
    if (apt == NULL)
        return ENOMEM;
    apt->first = TRUE;
    apt->primary = NULL;
    apt->cc_context = NULL;
    apt->iter = NULL;

    ptcursor = malloc(sizeof(*ptcursor));
    if (ptcursor == NULL) {
        free(apt);
        return ENOMEM;
    }

    ptcursor->ops = &krb5_api_macos_ops;
    ptcursor->data = apt;
    *ptcursor_out = ptcursor;
    return 0;
}

/* Create a cache object and open it to ensure that it exists in the
 * collection.  If it does not, return success but set *cache_out to NULL. */
static krb5_error_code
make_open_cache(const char *residual, krb5_ccache *cache_out)
{
    krb5_error_code ret;
    krb5_ccache cache;
    uint32_t err;

    *cache_out = NULL;

    ret = make_cache(residual, NULL, &cache);
    if (ret)
        return ret;

    err = open_cache(cache->data);
    if (err) {
        api_macos_close(NULL, cache);
        return (err == ccErrCCacheNotFound) ? 0 : ccerr2mit(err);
    }

    *cache_out = cache;
    return 0;
}

static krb5_error_code
api_macos_ptcursor_next(krb5_context context, krb5_cc_ptcursor ptcursor,
                        krb5_ccache *cache_out)
{
    krb5_error_code ret;
    uint32_t err;
    struct api_macos_ptcursor *apt = ptcursor->data;
    const char *defname, *defresidual;
    cc_ccache_t cache;
    cc_string_t residual;
    struct api_macos_cache_data *data;

    *cache_out = NULL;

    defname = krb5_cc_default_name(context);
    if (defname == NULL || strncmp(defname, "API:", 4) != 0)
        return 0;
    defresidual = defname + 4;

    /* If the default cache name is a subsidiary cache, yield that cache if it
     * exists and stop. */
    if (*defresidual != '\0') {
        if (!apt->first)
            return 0;
        apt->first = FALSE;
        return make_open_cache(defresidual, cache_out);
    }

    if (apt->first) {
        apt->first = FALSE;

        /* Prepare to iterate over the collection. */
        err = cc_initialize(&apt->cc_context, ccapi_version_max, NULL, NULL);
        if (err)
            return KRB5_FCC_INTERNAL;
        err = cc_context_new_ccache_iterator(apt->cc_context, &apt->iter);
        if (err)
            return KRB5_FCC_INTERNAL;

        /* Yield the primary cache first if it exists. */
        ret = get_primary_name(context, &apt->primary);
        if (ret)
            return ret;
        ret = make_open_cache(apt->primary, cache_out);
        if (ret || *cache_out != NULL)
            return ret;
    }

    for (;;) {
        err = cc_ccache_iterator_next(apt->iter, &cache);
        if (err)
            return (err == ccIteratorEnd) ? 0 : ccerr2mit(err);

        err = cc_ccache_get_name(cache, &residual);
        if (err) {
            cc_ccache_release(cache);
            return ccerr2mit(err);
        }

        /* Skip the primary cache since we yielded it first. */
        if (strcmp(residual->data, apt->primary) != 0)
            break;
    }

    ret = make_cache(residual->data, NULL, cache_out);
    cc_string_release(residual);
    if (ret) {
        cc_ccache_release(cache);
        return ret;
    }
    data = (*cache_out)->data;
    data->cache = cache;
    return 0;
}

static krb5_error_code
api_macos_ptcursor_free(krb5_context context, krb5_cc_ptcursor *ptcursor)
{
    struct api_macos_ptcursor *apt = (*ptcursor)->data;

    if (apt != NULL) {
        if (apt->iter != NULL)
            cc_ccache_iterator_release(apt->iter);
        if (apt->cc_context != NULL)
            cc_context_release(apt->cc_context);
        free(apt->primary);
        free(apt);
    }

    free(*ptcursor);
    *ptcursor = NULL;

    return 0;
}

static krb5_error_code
api_macos_lock(krb5_context context, krb5_ccache cache)
{
    struct api_macos_cache_data *data = cache->data;
    uint32_t err;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    err = cc_ccache_lock(data->cache, cc_lock_write, cc_lock_block);
    return ccerr2mit(err);
}

static krb5_error_code
api_macos_unlock(krb5_context context, krb5_ccache cache)
{
    struct api_macos_cache_data *data = cache->data;
    uint32_t err;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    err = cc_ccache_unlock(data->cache);
    return ccerr2mit(err);
}

static krb5_error_code
api_macos_switch_to(krb5_context context, krb5_ccache cache)
{
    struct api_macos_cache_data *data = cache->data;
    uint32_t err;

    err = open_cache(data);
    if (err)
        return ccerr2mit(err);

    err = cc_ccache_set_default(data->cache);
    return ccerr2mit(err);
}

const krb5_cc_ops krb5_api_macos_ops = {
    0,
    "API",
    api_macos_get_name,
    api_macos_resolve,
    api_macos_gen_new,
    api_macos_initialize,
    api_macos_destroy,
    api_macos_close,
    api_macos_store,
    api_macos_retrieve,
    api_macos_get_princ,
    api_macos_start_seq_get,
    api_macos_next_cred,
    api_macos_end_seq_get,
    api_macos_remove_cred,
    api_macos_set_flags,
    api_macos_get_flags,
    api_macos_ptcursor_new,
    api_macos_ptcursor_next,
    api_macos_ptcursor_free,
    NULL, /* move */
    NULL, /* wasdefault */
    api_macos_lock,
    api_macos_unlock,
    api_macos_switch_to,
};

#endif /* TARGET_OS_MAC */
