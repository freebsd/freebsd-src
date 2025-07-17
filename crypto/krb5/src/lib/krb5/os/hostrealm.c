/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/hostrealm.c - realm-of-host and default-realm APIs */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "os-proto.h"
#include "fake-addrinfo.h"
#include <krb5/hostrealm_plugin.h>
#include <ctype.h>

#if defined(_WIN32) && !defined(__CYGWIN32__)
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif
#endif

struct hostrealm_module_handle {
    struct krb5_hostrealm_vtable_st vt;
    krb5_hostrealm_moddata data;
};

/* Release a list of hostrealm module handles. */
static void
free_handles(krb5_context context, struct hostrealm_module_handle **handles)
{
    struct hostrealm_module_handle *h, **hp;

    if (handles == NULL)
        return;
    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.fini != NULL)
            h->vt.fini(context, h->data);
        free(h);
    }
    free(handles);
}

/* Get the registered hostrealm modules including all built-in modules, in the
 * proper order. */
static krb5_error_code
get_modules(krb5_context context, krb5_plugin_initvt_fn **modules_out)
{
    krb5_error_code ret;
    const int intf = PLUGIN_INTERFACE_HOSTREALM;

    *modules_out = NULL;

    /* Register built-in modules. */
    ret = k5_plugin_register(context, intf, "registry",
                             hostrealm_registry_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "profile",
                             hostrealm_profile_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "dns", hostrealm_dns_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "domain", hostrealm_domain_initvt);
    if (ret)
        return ret;

    return k5_plugin_load_all(context, intf, modules_out);
}

/* Initialize context->hostrealm_handles with a list of module handles. */
static krb5_error_code
load_hostrealm_modules(krb5_context context)
{
    krb5_error_code ret;
    struct hostrealm_module_handle **list = NULL, *handle;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    size_t count;

    ret = get_modules(context, &modules);
    if (ret != 0)
        goto cleanup;

    /* Allocate a large enough list of handles. */
    for (count = 0; modules[count] != NULL; count++);
    list = k5alloc((count + 1) * sizeof(*list), &ret);
    if (list == NULL)
        goto cleanup;

    /* Initialize each module, ignoring ones that fail. */
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        handle = k5alloc(sizeof(*handle), &ret);
        if (handle == NULL)
            goto cleanup;
        ret = (*mod)(context, 1, 1, (krb5_plugin_vtable)&handle->vt);
        if (ret != 0) {
            TRACE_HOSTREALM_VTINIT_FAIL(context, ret);
            free(handle);
            continue;
        }

        handle->data = NULL;
        if (handle->vt.init != NULL) {
            ret = handle->vt.init(context, &handle->data);
            if (ret != 0) {
                TRACE_HOSTREALM_INIT_FAIL(context, handle->vt.name, ret);
                free(handle);
                continue;
            }
        }
        list[count++] = handle;
        list[count] = NULL;
    }
    list[count] = NULL;

    ret = 0;
    context->hostrealm_handles = list;
    list = NULL;

cleanup:
    k5_plugin_free_modules(context, modules);
    free_handles(context, list);
    return ret;
}

/* Invoke a module's host_realm method, if it has one. */
static krb5_error_code
host_realm(krb5_context context, struct hostrealm_module_handle *h,
          const char *host, char ***realms_out)
{
    if (h->vt.host_realm == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    return h->vt.host_realm(context, h->data, host, realms_out);
}

/* Invoke a module's fallback_realm method, if it has one. */
static krb5_error_code
fallback_realm(krb5_context context, struct hostrealm_module_handle *h,
               const char *host, char ***realms_out)
{
    if (h->vt.fallback_realm == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    return h->vt.fallback_realm(context, h->data, host, realms_out);
}

/* Invoke a module's default_realm method, if it has one. */
static krb5_error_code
default_realm(krb5_context context, struct hostrealm_module_handle *h,
              char ***realms_out)
{
    if (h->vt.default_realm == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    return h->vt.default_realm(context, h->data, realms_out);
}

/* Invoke a module's free_list method. */
static void
free_list(krb5_context context, struct hostrealm_module_handle *h,
          char **list)
{
    h->vt.free_list(context, h->data, list);
}

/* Copy a null-terminated list of strings. */
static krb5_error_code
copy_list(char **in, char ***out)
{
    size_t count, i;
    char **list;

    *out = NULL;
    for (count = 0; in[count] != NULL; count++);
    list = calloc(count + 1, sizeof(*list));
    if (list == NULL)
        return ENOMEM;
    for (i = 0; i < count; i++) {
        list[i] = strdup(in[i]);
        if (list[i] == NULL) {
            krb5_free_host_realm(NULL, list);
            return ENOMEM;
        }
    }
    *out = list;
    return 0;
}

static krb5_error_code
translate_gai_error(int num)
{
    switch (num) {
#ifdef EAI_ADDRFAMILY
    case EAI_ADDRFAMILY:
        return EAFNOSUPPORT;
#endif
    case EAI_AGAIN:
        return EAGAIN;
    case EAI_BADFLAGS:
        return EINVAL;
    case EAI_FAIL:
        return KRB5_EAI_FAIL;
    case EAI_FAMILY:
        return EAFNOSUPPORT;
    case EAI_MEMORY:
        return ENOMEM;
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
    case EAI_NODATA:
        return KRB5_EAI_NODATA;
#endif
    case EAI_NONAME:
        return KRB5_EAI_NONAME;
#if defined(EAI_OVERFLOW)
    case EAI_OVERFLOW:
        return EINVAL;          /* XXX */
#endif
    case EAI_SERVICE:
        return KRB5_EAI_SERVICE;
    case EAI_SOCKTYPE:
        return EINVAL;
#ifdef EAI_SYSTEM
    case EAI_SYSTEM:
        return errno;
#endif
    }
    abort();
    return -1;
}

/* Get the canonical form of the local host name, using forward
 * canonicalization only. */
krb5_error_code
krb5int_get_fq_local_hostname(char **hostname_out)
{
    struct addrinfo *ai, hints;
    char buf[MAXHOSTNAMELEN];
    int err;

    *hostname_out = NULL;

    if (gethostname(buf, sizeof(buf)) == -1)
        return SOCKET_ERRNO;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_CANONNAME | AI_ADDRCONFIG;
    err = getaddrinfo(buf, NULL, &hints, &ai);
    if (err)
        return translate_gai_error(err);
    if (ai->ai_canonname == NULL) {
        freeaddrinfo(ai);
        return KRB5_EAI_FAIL;
    }
    *hostname_out = strdup(ai->ai_canonname);
    freeaddrinfo(ai);
    return (*hostname_out == NULL) ? ENOMEM : 0;
}

static krb5_error_code
clean_hostname(krb5_context context, const char *host, char **cleanname_out)
{
    char *p, *cleanname;
    krb5_error_code ret;
    size_t l;

    *cleanname_out = NULL;

    if (host != NULL) {
        cleanname = strdup(host);
        if (cleanname == NULL)
            return ENOMEM;
    } else {
        ret = krb5int_get_fq_local_hostname(&cleanname);
        if (ret)
            return ret;
    }

    /* Fold to lowercase. */
    for (p = cleanname; *p; p++) {
        if (isupper((unsigned char)*p))
            *p = tolower((unsigned char)*p);
    }

    /* Strip off trailing dot. */
    l = strlen(cleanname);
    if (l > 0 && cleanname[l - 1] == '.')
        cleanname[l - 1] = '\0';

    *cleanname_out = cleanname;
    return 0;
}

/* Return true if name appears to be an IPv4 or IPv6 address. */
krb5_boolean
k5_is_numeric_address(const char *name)
{
    int ndots = 0;
    const char *p;

    /* If name contains only numbers and three dots, consider it to be an IPv4
     * address. */
    if (strspn(name, "01234567890.") == strlen(name)) {
        for (p = name; *p; p++) {
            if (*p == '.')
                ndots++;
        }
        if (ndots == 3)
            return TRUE;
    }

    /* If name contains a colon, consider it to be an IPv6 address. */
    if (strchr(name, ':') != NULL)
        return TRUE;

    return FALSE;
}

/* Construct a one-element realm list containing a copy of realm. */
krb5_error_code
k5_make_realmlist(const char *realm, char ***realms_out)
{
    char **realms;

    *realms_out = NULL;
    realms = calloc(2, sizeof(*realms));
    if (realms == NULL)
        return ENOMEM;
    realms[0] = strdup(realm);
    if (realms[0] == NULL) {
        free(realms);
        return ENOMEM;
    }
    *realms_out = realms;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_get_host_realm(krb5_context context, const char *host, char ***realms_out)
{
    krb5_error_code ret;
    struct hostrealm_module_handle **hp;
    char **realms, *cleanname = NULL;

    *realms_out = NULL;

    if (context->hostrealm_handles == NULL) {
        ret = load_hostrealm_modules(context);
        if (ret)
            goto cleanup;
    }

    ret = clean_hostname(context, host, &cleanname);
    if (ret)
        goto cleanup;

    /* Give each module a chance to determine the host's realms. */
    for (hp = context->hostrealm_handles; *hp != NULL; hp++) {
        ret = host_realm(context, *hp, cleanname, &realms);
        if (ret == 0) {
            ret = copy_list(realms, realms_out);
            free_list(context, *hp, realms);
            goto cleanup;
        } else if (ret != KRB5_PLUGIN_NO_HANDLE) {
            goto cleanup;
        }
    }

    /* Return a list containing the "referral realm" (an empty realm), as a
     * cue to try referrals. */
    ret = k5_make_realmlist(KRB5_REFERRAL_REALM, realms_out);

cleanup:
    free(cleanname);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_get_fallback_host_realm(krb5_context context, krb5_data *hdata,
                             char ***realms_out)
{
    krb5_error_code ret;
    struct hostrealm_module_handle **hp;
    char **realms, *defrealm, *host, *cleanname = NULL;

    *realms_out = NULL;

    /* Convert hdata into a string and clean it. */
    host = k5memdup0(hdata->data, hdata->length, &ret);
    if (host == NULL)
        goto cleanup;
    ret = clean_hostname(context, host, &cleanname);
    free(host);
    if (ret)
        goto cleanup;

    if (context->hostrealm_handles == NULL) {
        ret = load_hostrealm_modules(context);
        if (ret)
            goto cleanup;
    }

    /* Give each module a chance to determine the fallback realms. */
    for (hp = context->hostrealm_handles; *hp != NULL; hp++) {
        ret = fallback_realm(context, *hp, cleanname, &realms);
        if (ret == 0) {
            ret = copy_list(realms, realms_out);
            free_list(context, *hp, realms);
            goto cleanup;
        } else if (ret != KRB5_PLUGIN_NO_HANDLE) {
            goto cleanup;
        }
    }

    /* Return a list containing the default realm. */
    ret = krb5_get_default_realm(context, &defrealm);
    if (ret)
        goto cleanup;
    ret = k5_make_realmlist(defrealm, realms_out);
    krb5_free_default_realm(context, defrealm);

cleanup:
    free(cleanname);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_free_host_realm(krb5_context context, char *const *list)
{
    char *const *p;

    for (p = list; p != NULL && *p != NULL; p++)
        free(*p);
    free((char **)list);
    return 0;
}

/* Get the system default realm using hostrealm modules. */
static krb5_error_code
get_default_realm(krb5_context context, char **realm_out)
{
    krb5_error_code ret;
    struct hostrealm_module_handle **hp;
    char **realms;

    *realm_out = NULL;
    if (context->hostrealm_handles == NULL) {
        ret = load_hostrealm_modules(context);
        if (ret)
            return ret;
    }

    /* Give each module a chance to determine the default realm. */
    for (hp = context->hostrealm_handles; *hp != NULL; hp++) {
        ret = default_realm(context, *hp, &realms);
        if (ret == 0) {
            if (*realms == NULL) {
                ret = KRB5_CONFIG_NODEFREALM;
            } else {
                *realm_out = strdup(realms[0]);
                if (*realm_out == NULL)
                    ret = ENOMEM;
            }
            free_list(context, *hp, realms);
            return ret;
        } else if (ret != KRB5_PLUGIN_NO_HANDLE) {
            return ret;
        }
    }

    return KRB5_CONFIG_NODEFREALM;
}

krb5_error_code KRB5_CALLCONV
krb5_get_default_realm(krb5_context context, char **realm_out)
{
    krb5_error_code ret;

    *realm_out = NULL;

    if (context == NULL || context->magic != KV5M_CONTEXT)
        return KV5M_CONTEXT;

    if (context->default_realm == NULL) {
        ret = get_default_realm(context, &context->default_realm);
        if (ret)
            return ret;
    }
    *realm_out = strdup(context->default_realm);
    return (*realm_out == NULL) ? ENOMEM : 0;
}

krb5_error_code KRB5_CALLCONV
krb5_set_default_realm(krb5_context context, const char *realm)
{
    if (context == NULL || context->magic != KV5M_CONTEXT)
        return KV5M_CONTEXT;

    if (context->default_realm != NULL) {
        free(context->default_realm);
        context->default_realm = NULL;
    }

    /* Allow the caller to clear the default realm setting by passing NULL. */
    if (realm != NULL) {
        context->default_realm = strdup(realm);
        if (context->default_realm == NULL)
            return ENOMEM;
    }

    return 0;
}

void KRB5_CALLCONV
krb5_free_default_realm(krb5_context context, char *realm)
{
    free(realm);
}

void
k5_hostrealm_free_context(krb5_context context)
{
    free_handles(context, context->hostrealm_handles);
    context->hostrealm_handles = NULL;
}
