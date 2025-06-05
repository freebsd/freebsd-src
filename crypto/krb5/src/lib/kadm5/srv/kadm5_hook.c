/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/srv/kadm5_hook.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
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
/* Consumer interface for kadm5_hook plugins. */

#include "k5-int.h"
#include "server_internal.h"
#include <krb5/kadm5_hook_plugin.h>
#include <adm_proto.h>
#include <syslog.h>

struct kadm5_hook_handle_st {
    kadm5_hook_vftable_1 vt;
    kadm5_hook_modinfo *data;
};

krb5_error_code
k5_kadm5_hook_load(krb5_context context,
                   kadm5_hook_handle **handles_out)
{
    krb5_error_code ret;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    size_t count;
    kadm5_hook_handle *list = NULL, handle = NULL;

    *handles_out = NULL;

    ret = k5_plugin_load_all(context, PLUGIN_INTERFACE_KADM5_HOOK, &modules);
    if (ret != 0)
        goto cleanup;

    /* Allocate a large enough list of handles. */
    for (count = 0; modules[count] != NULL; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        goto cleanup;

    /* For each module, allocate a handle, initialize its vtable, and
     * initialize the module. */
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        handle = k5alloc(sizeof(*handle), &ret);
        if (handle == NULL)
            goto cleanup;
        ret = (*mod)(context, 1, 2, (krb5_plugin_vtable)&handle->vt);
        if (ret != 0) {         /* Failed vtable init is non-fatal. */
            free(handle);
            handle = NULL;
            continue;
        }
        handle->data = NULL;
        if (handle->vt.init != NULL) {
            ret = handle->vt.init(context, &handle->data);
            if (ret != 0)       /* Failed initialization is fatal. */
                goto cleanup;
        }
        list[count++] = handle;
        list[count] = NULL;
        handle = NULL;
    }
    list[count] = NULL;

    ret = 0;
    *handles_out = list;
    list = NULL;

cleanup:
    free(handle);
    k5_plugin_free_modules(context, modules);
    k5_kadm5_hook_free_handles(context, list);
    return ret;
}

void
k5_kadm5_hook_free_handles(krb5_context context, kadm5_hook_handle *handles)
{
    kadm5_hook_handle *hp, handle;

    if (handles == NULL)
        return;
    for (hp = handles; *hp != NULL; hp++) {
        handle = *hp;
        if (handle->vt.fini != NULL)
            handle->vt.fini(context, handle->data);
        free(handle);
    }
    free(handles);
}

static void
log_failure(krb5_context context,
            const char *name,
            const char *function,
            krb5_error_code ret)
{
    const char *e = krb5_get_error_message(context, ret);

    krb5_klog_syslog(LOG_ERR, _("kadm5_hook %s failed postcommit %s: %s"),
                     name, function, e);
    krb5_free_error_message(context, e);
}

#define ITERATE(operation, params)                                      \
    for (; *handles; handles++) {                                       \
        kadm5_hook_handle h = *handles;                                 \
        krb5_error_code ret = 0;                                        \
        if (h->vt.operation) {                                          \
            ret = h->vt.operation params;                               \
        }                                                               \
        if (ret) {                                                      \
            if (stage == KADM5_HOOK_STAGE_PRECOMMIT)                    \
                return ret;                                             \
            else                                                        \
                log_failure(context, h->vt.name, #operation, ret);      \
        }                                                               \
    }


kadm5_ret_t
k5_kadm5_hook_chpass(krb5_context context, kadm5_hook_handle *handles,
                     int stage, krb5_principal princ, krb5_boolean keepold,
                     int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                     const char *newpass)
{
    ITERATE(chpass, (context, h->data,
                     stage, princ, keepold,
                     n_ks_tuple, ks_tuple, newpass));
    return 0;
}

kadm5_ret_t
k5_kadm5_hook_create(krb5_context context, kadm5_hook_handle *handles,
                     int stage, kadm5_principal_ent_t princ, long mask,
                     int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                     const char *newpass)
{
    ITERATE(create, (context, h->data,
                     stage, princ, mask, n_ks_tuple, ks_tuple, newpass));
    return 0;
}

kadm5_ret_t
k5_kadm5_hook_modify(krb5_context context, kadm5_hook_handle *handles,
                     int stage, kadm5_principal_ent_t princ, long mask)
{
    ITERATE(modify, (context, h->data, stage, princ, mask));
    return 0;
}

kadm5_ret_t
k5_kadm5_hook_rename(krb5_context context, kadm5_hook_handle *handles,
                     int stage, krb5_principal oprinc, krb5_principal nprinc)
{
    ITERATE(rename, (context, h->data, stage, oprinc, nprinc));
    return 0;
}

kadm5_ret_t
k5_kadm5_hook_remove(krb5_context context, kadm5_hook_handle *handles,
                     int stage, krb5_principal princ)
{
    ITERATE(remove, (context, h->data, stage, princ));
    return 0;
}
