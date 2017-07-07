/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccselect.c - krb5_cc_select API and module loader */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include "k5-int.h"
#include "cc-int.h"
#include <krb5/ccselect_plugin.h>
#include "../krb/int-proto.h"

struct ccselect_module_handle {
    struct krb5_ccselect_vtable_st vt;
    krb5_ccselect_moddata data;
    int priority;
};

static void
free_handles(krb5_context context, struct ccselect_module_handle **handles)
{
    struct ccselect_module_handle *h, **hp;

    if (handles == NULL)
        return;
    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.fini)
            h->vt.fini(context, h->data);
        free(h);
    }
    free(handles);
}

static krb5_error_code
load_modules(krb5_context context)
{
    krb5_error_code ret;
    struct ccselect_module_handle **list = NULL, *handle;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    size_t count;

#ifndef _WIN32
    ret = k5_plugin_register(context, PLUGIN_INTERFACE_CCSELECT, "k5identity",
                             ccselect_k5identity_initvt);
    if (ret != 0)
        goto cleanup;
#endif

    ret = k5_plugin_register(context, PLUGIN_INTERFACE_CCSELECT, "realm",
                             ccselect_realm_initvt);
    if (ret != 0)
        goto cleanup;

    ret = k5_plugin_load_all(context, PLUGIN_INTERFACE_CCSELECT, &modules);
    if (ret != 0)
        goto cleanup;

    /* Allocate a large enough list of handles. */
    for (count = 0; modules[count] != NULL; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        goto cleanup;

    /* Initialize each module, ignoring ones that fail. */
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        handle = k5alloc(sizeof(*handle), &ret);
        if (handle == NULL)
            goto cleanup;
        ret = (*mod)(context, 1, 1, (krb5_plugin_vtable)&handle->vt);
        if (ret != 0) {         /* Failed vtable init is non-fatal. */
            TRACE_CCSELECT_VTINIT_FAIL(context, ret);
            free(handle);
            continue;
        }
        handle->data = NULL;
        ret = handle->vt.init(context, &handle->data, &handle->priority);
        if (ret != 0) {         /* Failed initialization is non-fatal. */
            TRACE_CCSELECT_INIT_FAIL(context, handle->vt.name, ret);
            free(handle);
            continue;
        }
        list[count++] = handle;
        list[count] = NULL;
    }
    list[count] = NULL;

    ret = 0;
    context->ccselect_handles = list;
    list = NULL;

cleanup:
    k5_plugin_free_modules(context, modules);
    free_handles(context, list);
    return ret;
}

static krb5_error_code
choose(krb5_context context, struct ccselect_module_handle *h,
       krb5_principal server, krb5_ccache *cache_out,
       krb5_principal *princ_out)
{
    return h->vt.choose(context, h->data, server, cache_out, princ_out);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_select(krb5_context context, krb5_principal server,
               krb5_ccache *cache_out, krb5_principal *princ_out)
{
    krb5_error_code ret;
    int priority;
    struct ccselect_module_handle **hp, *h;
    krb5_ccache cache;
    krb5_principal princ;

    *cache_out = NULL;
    *princ_out = NULL;

    if (context->ccselect_handles == NULL) {
        ret = load_modules(context);
        if (ret)
            return ret;
    }

    /* Consult authoritative modules first, then heuristic ones. */
    for (priority = KRB5_CCSELECT_PRIORITY_AUTHORITATIVE;
         priority >= KRB5_CCSELECT_PRIORITY_HEURISTIC; priority--) {
        for (hp = context->ccselect_handles; *hp != NULL; hp++) {
            h = *hp;
            if (h->priority != priority)
                continue;
            ret = choose(context, h, server, &cache, &princ);
            if (ret == 0) {
                TRACE_CCSELECT_MODCHOICE(context, h->vt.name, server, cache,
                                         princ);
                *cache_out = cache;
                *princ_out = princ;
                return 0;
            } else if (ret == KRB5_CC_NOTFOUND) {
                TRACE_CCSELECT_MODNOTFOUND(context, h->vt.name, server, princ);
                *princ_out = princ;
                return ret;
            } else if (ret != KRB5_PLUGIN_NO_HANDLE) {
                TRACE_CCSELECT_MODFAIL(context, h->vt.name, ret, server);
                return ret;
            }
        }
    }

    TRACE_CCSELECT_NOTFOUND(context, server);
    return KRB5_CC_NOTFOUND;
}

void
k5_ccselect_free_context(krb5_context context)
{
    free_handles(context, context->ccselect_handles);
    context->ccselect_handles = NULL;
}
