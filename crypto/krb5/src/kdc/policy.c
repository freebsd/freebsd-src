/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/policy.c - Policy decision routines for KDC */
/*
 * Copyright (C) 2017 by Red Hat, Inc.
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
#include "kdc_util.h"
#include "extern.h"
#include "policy.h"
#include "adm_proto.h"
#include <krb5/kdcpolicy_plugin.h>
#include <syslog.h>

typedef struct kdcpolicy_handle_st {
    struct krb5_kdcpolicy_vtable_st vt;
    krb5_kdcpolicy_moddata moddata;
} *kdcpolicy_handle;

static kdcpolicy_handle *handles;

static void
free_indicators(char **ais)
{
    size_t i;

    if (ais == NULL)
        return;
    for (i = 0; ais[i] != NULL; i++)
        free(ais[i]);
    free(ais);
}

/* Convert inds to a null-terminated list of C strings. */
static krb5_error_code
authind_strings(krb5_data *const *inds, char ***strs_out)
{
    krb5_error_code ret;
    char **list = NULL;
    size_t i, count;

    *strs_out = NULL;

    for (count = 0; inds != NULL && inds[count] != NULL; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        goto error;

    for (i = 0; i < count; i++) {
        list[i] = k5memdup0(inds[i]->data, inds[i]->length, &ret);
        if (list[i] == NULL)
            goto error;
    }

    *strs_out = list;
    return 0;

error:
    free_indicators(list);
    return ret;
}

/* Constrain times->endtime to life and times->renew_till to rlife, relative to
 * now. */
static void
update_ticket_times(krb5_ticket_times *times, krb5_timestamp now,
                    krb5_deltat life, krb5_deltat rlife)
{
    if (life)
        times->endtime = ts_min(ts_incr(now, life), times->endtime);
    if (rlife)
        times->renew_till = ts_min(ts_incr(now, rlife), times->renew_till);
}

/* Check an AS request against kdcpolicy modules, updating times with any
 * module endtime constraints.  Set an appropriate status string on error. */
krb5_error_code
check_kdcpolicy_as(krb5_context context, const krb5_kdc_req *request,
                   const krb5_db_entry *client, const krb5_db_entry *server,
                   krb5_data *const *auth_indicators, krb5_timestamp kdc_time,
                   krb5_ticket_times *times, const char **status)
{
    krb5_deltat life = 0, rlife = 0;
    krb5_error_code ret;
    kdcpolicy_handle *hp, h;
    char **ais = NULL;

    *status = NULL;

    ret = authind_strings(auth_indicators, &ais);
    if (ret)
        goto done;

    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.check_as == NULL)
            continue;

        ret = h->vt.check_as(context, h->moddata, request, client, server,
                             (const char **)ais, status, &life, &rlife);
        if (ret)
            goto done;

        update_ticket_times(times, kdc_time, life, rlife);
    }

done:
    free_indicators(ais);
    return ret;
}

/*
 * Check the TGS request against the local TGS policy.  Accepts an
 * authentication indicator for the module policy decisions.  Returns 0 and a
 * NULL status string on success.
 */
krb5_error_code
check_kdcpolicy_tgs(krb5_context context, const krb5_kdc_req *request,
                    const krb5_db_entry *server, const krb5_ticket *ticket,
                    krb5_data *const *auth_indicators, krb5_timestamp kdc_time,
                    krb5_ticket_times *times, const char **status)
{
    krb5_deltat life = 0, rlife = 0;
    krb5_error_code ret;
    kdcpolicy_handle *hp, h;
    char **ais = NULL;

    *status = NULL;

    ret = authind_strings(auth_indicators, &ais);
    if (ret)
        goto done;

    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.check_tgs == NULL)
            continue;

        ret = h->vt.check_tgs(context, h->moddata, request, server, ticket,
                              (const char **)ais, status, &life, &rlife);
        if (ret)
            goto done;

        update_ticket_times(times, kdc_time, life, rlife);
    }

done:
    free_indicators(ais);
    return ret;
}

void
unload_kdcpolicy_plugins(krb5_context context)
{
    kdcpolicy_handle *hp, h;

    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.fini != NULL)
            h->vt.fini(context, h->moddata);
        free(h);
    }
    free(handles);
    handles = NULL;
}

krb5_error_code
load_kdcpolicy_plugins(krb5_context context)
{
    krb5_error_code ret;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    kdcpolicy_handle h;
    size_t count;

    ret = k5_plugin_load_all(context, PLUGIN_INTERFACE_KDCPOLICY, &modules);
    if (ret)
        goto cleanup;

    for (count = 0; modules[count] != NULL; count++);
    handles = k5calloc(count + 1, sizeof(*handles), &ret);
    if (handles == NULL)
        goto cleanup;

    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        h = k5calloc(1, sizeof(*h), &ret);
        if (h == NULL)
            goto cleanup;

        ret = (*mod)(context, 1, 1, (krb5_plugin_vtable)&h->vt);
        if (ret) {              /* Version mismatch. */
            TRACE_KDCPOLICY_VTINIT_FAIL(context, ret);
            free(h);
            continue;
        }
        if (h->vt.init != NULL) {
            ret = h->vt.init(context, &h->moddata);
            if (ret == KRB5_PLUGIN_NO_HANDLE) {
                TRACE_KDCPOLICY_INIT_SKIP(context, h->vt.name);
                free(h);
                continue;
            }
            if (ret) {
                kdc_err(context, ret, _("while loading policy module %s"),
                        h->vt.name);
                free(h);
                goto cleanup;
            }
        }
        handles[count++] = h;
    }

    ret = 0;

cleanup:
    if (ret)
        unload_kdcpolicy_plugins(context);
    k5_plugin_free_modules(context, modules);
    return ret;
}
