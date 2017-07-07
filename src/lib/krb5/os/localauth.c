/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/localauth.c - Authorize access to local accounts */
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
#include <krb5/localauth_plugin.h>

struct localauth_module_handle {
    struct krb5_localauth_vtable_st vt;
    krb5_localauth_moddata data;
};

static krb5_error_code
localauth_auth_to_local_initvt(krb5_context context, int maj_ver, int min_ver,
                               krb5_plugin_vtable vtable);
static krb5_error_code
localauth_default_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable);

/* Release a list of localauth module handles. */
static void
free_handles(krb5_context context, struct localauth_module_handle **handles)
{
    struct localauth_module_handle *h, **hp;

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

/* Find a localauth module whose an2ln_types contains a match for type. */
static struct localauth_module_handle *
find_typed_module(struct localauth_module_handle **handles, const char *type)
{
    struct localauth_module_handle **hp, *h;
    const char **tp;

    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        for (tp = h->vt.an2ln_types; tp != NULL && *tp != NULL; tp++) {
            if (strcmp(*tp, type) == 0)
                return h;
        }
    }
    return NULL;
}

/* Generate a trace log and return 1 if the an2ln_types of handle conflicts
 * with any of the handles in list.  Return 0 otherwise. */
static int
check_conflict(krb5_context context, struct localauth_module_handle **list,
               struct localauth_module_handle *handle)
{
    struct localauth_module_handle *conflict;
    const char **tp;

    for (tp = handle->vt.an2ln_types; tp != NULL && *tp != NULL; tp++) {
        conflict = find_typed_module(list, *tp);
        if (conflict != NULL) {
            TRACE_LOCALAUTH_INIT_CONFLICT(context, *tp, handle->vt.name,
                                          conflict->vt.name);
            return 1;
        }
    }
    return 0;
}

/* Get the registered localauth modules including all built-in modules, in the
 * proper order. */
static krb5_error_code
get_modules(krb5_context context, krb5_plugin_initvt_fn **modules_out)
{
    krb5_error_code ret;
    const int intf = PLUGIN_INTERFACE_LOCALAUTH;

    *modules_out = NULL;

    /* Register built-in modules. */
    ret = k5_plugin_register(context, intf, "default",
                             localauth_default_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "rule", localauth_rule_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "names", localauth_names_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "auth_to_local",
                             localauth_auth_to_local_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "k5login",
                             localauth_k5login_initvt);
    if (ret)
        return ret;
    ret = k5_plugin_register(context, intf, "an2ln", localauth_an2ln_initvt);
    if (ret)
        return ret;

    ret = k5_plugin_load_all(context, intf, modules_out);
    if (ret)
        return ret;

    return 0;
}

/* Initialize context->localauth_handles with a list of module handles. */
static krb5_error_code
load_localauth_modules(krb5_context context)
{
    krb5_error_code ret;
    struct localauth_module_handle **list = NULL, *handle;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    size_t count;

    ret = get_modules(context, &modules);
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
        if (ret != 0) {
            TRACE_LOCALAUTH_VTINIT_FAIL(context, ret);
            free(handle);
            continue;
        }

        if (check_conflict(context, list, handle))
            continue;

        handle->data = NULL;
        if (handle->vt.init != NULL) {
            ret = handle->vt.init(context, &handle->data);
            if (ret != 0) {
                TRACE_LOCALAUTH_INIT_FAIL(context, handle->vt.name, ret);
                free(handle);
                continue;
            }
        }
        list[count++] = handle;
        list[count] = NULL;
    }
    list[count] = NULL;

    ret = 0;
    context->localauth_handles = list;
    list = NULL;

cleanup:
    k5_plugin_free_modules(context, modules);
    free_handles(context, list);
    return ret;
}

/* Invoke a module's userok method, if it has one. */
static krb5_error_code
userok(krb5_context context, struct localauth_module_handle *h,
       krb5_const_principal aname, const char *lname)
{
    if (h->vt.userok == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    return h->vt.userok(context, h->data, aname, lname);
}

/* Invoke a module's an2ln method, if it has one. */
static krb5_error_code
an2ln(krb5_context context, struct localauth_module_handle *h,
      const char *type, const char *residual, krb5_const_principal aname,
      char **lname_out)
{
    if (h->vt.an2ln == NULL)
        return KRB5_LNAME_NOTRANS;
    return h->vt.an2ln(context, h->data, type, residual, aname, lname_out);
}

/* Invoke a module's free_string method. */
static void
free_lname(krb5_context context, struct localauth_module_handle *h, char *str)
{
    h->vt.free_string(context, h->data, str);
}

/* Parse a TYPE or TYPE:residual string into its components. */
static krb5_error_code
parse_mapping_value(const char *value, char **type_out, char **residual_out)
{
    krb5_error_code ret;
    const char *p;
    char *type, *residual;

    *type_out = NULL;
    *residual_out = NULL;
    p = strchr(value, ':');
    if (p == NULL) {
        type = strdup(value);
        if (type == NULL)
            return ENOMEM;
        residual = NULL;
    } else {
        type = k5memdup0(value, p - value, &ret);
        if (type == NULL)
            return ret;
        residual = strdup(p + 1);
        if (residual == NULL) {
            free(type);
            return ENOMEM;
        }
    }
    *type_out = type;
    *residual_out = residual;
    return 0;
}

/* Apply the default an2ln method, which translates name@defaultrealm or
 * name/defaultrealm@defaultrealm to name. */
static krb5_error_code
an2ln_default(krb5_context context, krb5_localauth_moddata data,
              const char *type, const char *residual,
              krb5_const_principal aname, char **lname_out)
{
    krb5_error_code ret;
    char *def_realm;

    *lname_out = NULL;

    ret = krb5_get_default_realm(context, &def_realm);
    if (ret)
        return KRB5_LNAME_NOTRANS;

    if (!data_eq_string(aname->realm, def_realm)) {
        ret = KRB5_LNAME_NOTRANS;
    } else if (aname->length == 2) {
        /* Allow a second component if it is the local realm. */
        if (!data_eq_string(aname->data[1], def_realm))
            ret = KRB5_LNAME_NOTRANS;
    } else if (aname->length != 1) {
        ret = KRB5_LNAME_NOTRANS;
    }
    free(def_realm);
    if (ret)
        return ret;

    *lname_out = k5memdup0(aname->data[0].data, aname->data[0].length, &ret);
    return ret;
}

/*
 * Perform aname-to-lname translation using the auth_to_local values in the
 * default realm's profile section.  If no values exist, fall back to the
 * default method.
 */
static krb5_error_code
an2ln_auth_to_local(krb5_context context, krb5_localauth_moddata data,
                    const char *type_arg, const char *residual_arg,
                    krb5_const_principal aname, char **lname_out)
{
    krb5_error_code ret;
    struct localauth_module_handle *h;
    char *realm = NULL, **mapping_values = NULL, *type, *residual, *lname;
    const char *hierarchy[4];
    size_t i;

    *lname_out = NULL;

    /* Fetch the profile values for realms-><defaultrealm>->auth_to_local. */
    ret = krb5_get_default_realm(context, &realm);
    if (ret)
        return KRB5_LNAME_NOTRANS;
    hierarchy[0] = KRB5_CONF_REALMS;
    hierarchy[1] = realm;
    hierarchy[2] = KRB5_CONF_AUTH_TO_LOCAL;
    hierarchy[3] = NULL;
    ret = profile_get_values(context->profile, hierarchy, &mapping_values);
    if (ret) {
        /* Use default method if there are no auth_to_local values. */
        ret = an2ln_default(context, data, NULL, NULL, aname, lname_out);
        goto cleanup;
    }

    ret = KRB5_LNAME_NOTRANS;
    for (i = 0; mapping_values[i] != NULL && ret == KRB5_LNAME_NOTRANS; i++) {
        ret = parse_mapping_value(mapping_values[i], &type, &residual);
        if (ret)
            goto cleanup;
        h = find_typed_module(context->localauth_handles, type);
        if (h != NULL) {
            ret = an2ln(context, h, type, residual, aname, &lname);
            if (ret == 0) {
                *lname_out = strdup(lname);
                if (*lname_out == NULL)
                    ret = ENOMEM;
                free_lname(context, h, lname);
            }
        } else {
            ret = KRB5_CONFIG_BADFORMAT;
        }
        free(type);
        free(residual);
    }

cleanup:
    free(realm);
    profile_free_list(mapping_values);
    return ret;
}

static void
freestr(krb5_context context, krb5_localauth_moddata data, char *str)
{
    free(str);
}

static krb5_error_code
localauth_auth_to_local_initvt(krb5_context context, int maj_ver, int min_ver,
                               krb5_plugin_vtable vtable)
{
    krb5_localauth_vtable vt = (krb5_localauth_vtable)vtable;

    vt->name = "auth_to_local";
    vt->an2ln = an2ln_auth_to_local;
    vt->free_string = freestr;
    return 0;
}

static krb5_error_code
localauth_default_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_localauth_vtable vt = (krb5_localauth_vtable)vtable;
    static const char *types[] = { "DEFAULT", NULL };

    vt->name = "default";
    vt->an2ln_types = types;
    vt->an2ln = an2ln_default;
    vt->free_string = freestr;
    return 0;
}

krb5_boolean KRB5_CALLCONV
krb5_kuserok(krb5_context context, krb5_principal aname, const char *lname)
{
    krb5_error_code ret;
    struct localauth_module_handle **hp;
    krb5_boolean accepted = FALSE;

    if (context->localauth_handles == NULL && load_localauth_modules(context))
        return FALSE;

    /* If any module denies access, return false immediately.  Otherwise,
     * consult all modules and return true if one of them allows access. */
    for (hp = context->localauth_handles; *hp != NULL; hp++) {
        ret = userok(context, *hp, aname, lname);
        if (ret == 0)
            accepted = TRUE;
        else if (ret != KRB5_PLUGIN_NO_HANDLE)
            return FALSE;
    }
    return accepted;
}

krb5_error_code KRB5_CALLCONV
krb5_aname_to_localname(krb5_context context, krb5_const_principal aname,
                        int lnsize, char *lname_out)
{
    krb5_error_code ret;
    struct localauth_module_handle **hp;
    char *lname;
    size_t sz;

    if (context->localauth_handles == NULL) {
        ret = load_localauth_modules(context);
        if (ret)
            return ret;
    }

    for (hp = context->localauth_handles; *hp != NULL; hp++) {
        if ((*hp)->vt.an2ln_types != NULL)
            continue;
        ret = an2ln(context, *hp, NULL, NULL, aname, &lname);
        if (ret == 0) {
            sz = strlcpy(lname_out, lname, lnsize);
            free_lname(context, *hp, lname);
            return sz >= (size_t)lnsize ? KRB5_CONFIG_NOTENUFSPACE : 0;
        } else if (ret != KRB5_LNAME_NOTRANS) {
            return ret;
        }
    }
    return KRB5_LNAME_NOTRANS;
}

void
k5_localauth_free_context(krb5_context context)
{
    free_handles(context, context->localauth_handles);
    context->localauth_handles = NULL;
}
