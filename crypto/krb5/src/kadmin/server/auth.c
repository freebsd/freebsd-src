/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/server/auth.c - kadm5_auth pluggable interface consumer */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
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
#include <kadm5/admin.h>
#include <krb5/kadm5_auth_plugin.h>
#include "auth.h"

typedef struct {
    struct kadm5_auth_vtable_st vt;
    kadm5_auth_moddata data;
} *auth_handle;

static auth_handle *handles;

void
auth_fini(krb5_context context)
{
    auth_handle *hp, h;

    if (handles == NULL)
        return;
    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.fini != NULL)
            h->vt.fini(context, h->data);
        free(h);
    }
    free(handles);
    handles = NULL;
}

krb5_error_code
auth_init(krb5_context context, const char *acl_file)
{
    krb5_error_code ret;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    size_t count;
    auth_handle h = NULL;
    const int intf = PLUGIN_INTERFACE_KADM5_AUTH;

    ret = k5_plugin_register(context, intf, "acl", kadm5_auth_acl_initvt);
    if (ret)
        goto cleanup;
    ret = k5_plugin_register(context, intf, "self", kadm5_auth_self_initvt);
    if (ret)
        goto cleanup;
    ret = k5_plugin_load_all(context, PLUGIN_INTERFACE_KADM5_AUTH, &modules);
    if (ret)
        goto cleanup;

    /* Allocate a large enough list of handles. */
    for (count = 0; modules[count] != NULL; count++);
    handles = k5calloc(count + 1, sizeof(*handles), &ret);
    if (handles == NULL)
        goto cleanup;

    /* For each module, allocate a handle, initialize its vtable, and
     * initialize its module data. */
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        h = k5alloc(sizeof(*h), &ret);
        if (h == NULL)
            goto cleanup;
        ret = (*mod)(context, 1, 1, (krb5_plugin_vtable)&h->vt);
        if (ret) {              /* Failed vtable init is non-fatal. */
            TRACE_KADM5_AUTH_VTINIT_FAIL(context, ret);
            free(h);
            h = NULL;
            continue;
        }
        h->data = NULL;
        if (h->vt.init != NULL) {
            ret = h->vt.init(context, acl_file, &h->data);
            if (ret == KRB5_PLUGIN_NO_HANDLE) {
                TRACE_KADM5_AUTH_INIT_SKIP(context, h->vt.name);
                free(h);
                h = NULL;
                continue;
            }
            if (ret) {
                TRACE_KADM5_AUTH_INIT_FAIL(context, h->vt.name, ret);
                goto cleanup;
            }
        }
        handles[count++] = h;
        handles[count] = NULL;
        h = NULL;
    }

    ret = 0;

cleanup:
    if (ret)
        auth_fini(context);
    free(h);
    k5_plugin_free_modules(context, modules);
    return ret;
}

/* Invoke the appropriate method from h->vt for opcode, passing client and the
 * correct subset of p1, p2, s1, s2, polent, and mask for the method. */
static krb5_error_code
call_module(krb5_context context, auth_handle h, int opcode,
            krb5_const_principal client, krb5_const_principal p1,
            krb5_const_principal p2, const char *s1, const char *s2,
            const kadm5_policy_ent_rec *polent, long mask)
{
    /* addprinc and modprinc are handled through auth_restrict(). */
    assert(opcode != OP_ADDPRINC && opcode != OP_MODPRINC);

    if (opcode == OP_SETSTR && h->vt.setstr != NULL)
        return h->vt.setstr(context, h->data, client, p1, s1, s2);
    else if (opcode == OP_CPW && h->vt.cpw != NULL)
        return h->vt.cpw(context, h->data, client, p1);
    else if (opcode == OP_CHRAND && h->vt.chrand != NULL)
        return h->vt.chrand(context, h->data, client, p1);
    else if (opcode == OP_SETKEY && h->vt.setkey != NULL)
        return h->vt.setkey(context, h->data, client, p1);
    else if (opcode == OP_PURGEKEYS && h->vt.purgekeys != NULL)
        return h->vt.purgekeys(context, h->data, client, p1);
    else if (opcode == OP_DELPRINC && h->vt.delprinc != NULL)
        return h->vt.delprinc(context, h->data, client, p1);
    else if (opcode == OP_RENPRINC && h->vt.renprinc != NULL)
        return h->vt.renprinc(context, h->data, client, p1, p2);
    else if (opcode == OP_GETPRINC && h->vt.getprinc != NULL)
        return h->vt.getprinc(context, h->data, client, p1);
    else if (opcode == OP_GETSTRS && h->vt.getstrs != NULL)
        return h->vt.getstrs(context, h->data, client, p1);
    else if (opcode == OP_EXTRACT && h->vt.extract != NULL)
        return h->vt.extract(context, h->data, client, p1);
    else if (opcode == OP_LISTPRINCS && h->vt.listprincs != NULL)
        return h->vt.listprincs(context, h->data, client);
    else if (opcode == OP_ADDPOL && h->vt.addpol != NULL)
        return h->vt.addpol(context, h->data, client, s1, polent, mask);
    else if (opcode == OP_MODPOL && h->vt.modpol != NULL)
        return h->vt.modpol(context, h->data, client, s1, polent, mask);
    else if (opcode == OP_DELPOL && h->vt.delpol != NULL)
        return h->vt.delpol(context, h->data, client, s1);
    else if (opcode == OP_GETPOL && h->vt.getpol != NULL)
        return h->vt.getpol(context, h->data, client, s1, s2);
    else if (opcode == OP_LISTPOLS && h->vt.listpols != NULL)
        return h->vt.listpols(context, h->data, client);
    else if (opcode == OP_IPROP && h->vt.iprop != NULL)
        return h->vt.iprop(context, h->data, client);

    return KRB5_PLUGIN_NO_HANDLE;
}

krb5_boolean
auth(krb5_context context, int opcode, krb5_const_principal client,
     krb5_const_principal p1, krb5_const_principal p2, const char *s1,
     const char *s2, const kadm5_policy_ent_rec *polent, long mask)
{
    krb5_error_code ret;
    krb5_boolean authorized = FALSE;
    auth_handle *hp, h;

    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;

        ret = call_module(context, h, opcode, client, p1, p2, s1, s2,
                          polent, mask);
        if (!ret)
            authorized = TRUE;
        else if (ret != KRB5_PLUGIN_NO_HANDLE)
            return FALSE;
    }

    return authorized;
}

/* Impose restrictions, modifying *ent and *mask. */
static krb5_error_code
impose_restrictions(krb5_context context,
                    const struct kadm5_auth_restrictions *rs,
                    kadm5_principal_ent_t ent, long *mask)
{
    krb5_error_code ret;
    krb5_timestamp now;

    if (rs == NULL)
        return 0;
    if (rs->mask & (KADM5_PRINC_EXPIRE_TIME | KADM5_PW_EXPIRATION)) {
        ret = krb5_timeofday(context, &now);
        if (ret)
            return ret;
    }

    if (rs->mask & KADM5_ATTRIBUTES) {
        ent->attributes |= rs->require_attrs;
        ent->attributes &= rs->forbid_attrs;
        *mask |= KADM5_ATTRIBUTES;
    }
    if (rs->mask & KADM5_POLICY_CLR) {
        *mask &= ~KADM5_POLICY;
        *mask |= KADM5_POLICY_CLR;
    } else if (rs->mask & KADM5_POLICY) {
        if (ent->policy != NULL && strcmp(ent->policy, rs->policy) != 0) {
            free(ent->policy);
            ent->policy = NULL;
        }
        if (ent->policy == NULL) {
            ent->policy = strdup(rs->policy);
            if (ent->policy == NULL)
                return ENOMEM;
        }
        *mask |= KADM5_POLICY;
    }
    if (rs->mask & KADM5_PRINC_EXPIRE_TIME) {
        if (!(*mask & KADM5_PRINC_EXPIRE_TIME) ||
            ts_after(ent->princ_expire_time, ts_incr(now, rs->princ_lifetime)))
            ent->princ_expire_time = now + rs->princ_lifetime;
        *mask |= KADM5_PRINC_EXPIRE_TIME;
    }
    if (rs->mask & KADM5_PW_EXPIRATION) {
        if (!(*mask & KADM5_PW_EXPIRATION) ||
            ts_after(ent->pw_expiration, ts_incr(now, rs->pw_lifetime)))
            ent->pw_expiration = now + rs->pw_lifetime;
        *mask |= KADM5_PW_EXPIRATION;
    }
    if (rs->mask & KADM5_MAX_LIFE) {
        if (!(*mask & KADM5_MAX_LIFE) || ent->max_life > rs->max_life)
            ent->max_life = rs->max_life;
        *mask |= KADM5_MAX_LIFE;
    }
    if (rs->mask & KADM5_MAX_RLIFE) {
        if (!(*mask & KADM5_MAX_RLIFE) ||
            ent->max_renewable_life > rs->max_renewable_life)
            ent->max_renewable_life = rs->max_renewable_life;
        *mask |= KADM5_MAX_RLIFE;
    }
    return 0;
}

krb5_boolean
auth_restrict(krb5_context context, int opcode, krb5_const_principal client,
              kadm5_principal_ent_t ent, long *mask)
{
    auth_handle *hp, h;
    krb5_boolean authorized = FALSE;
    krb5_error_code ret, rs_ret;
    krb5_const_principal target = ent->principal;
    struct kadm5_auth_restrictions *rs;

    assert(opcode == OP_ADDPRINC || opcode == OP_MODPRINC);
    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;

        ret = KRB5_PLUGIN_NO_HANDLE;
        rs = NULL;
        if (opcode == OP_ADDPRINC && h->vt.addprinc != NULL) {
            ret = h->vt.addprinc(context, h->data, client, target, ent, *mask,
                                 &rs);
        } else if (opcode == OP_MODPRINC && h->vt.modprinc != NULL) {
            ret = h->vt.modprinc(context, h->data, client, target, ent, *mask,
                                 &rs);
        }
        if (rs != NULL) {
            rs_ret = impose_restrictions(context, rs, ent, mask);
            if (h->vt.free_restrictions != NULL)
                h->vt.free_restrictions(context, h->data, rs);
            if (rs_ret)
                return FALSE;
        }
        if (!ret)
            authorized = TRUE;
        else if (ret != KRB5_PLUGIN_NO_HANDLE)
            return FALSE;
    }

    return authorized;
}

void
auth_end(krb5_context context)
{
    auth_handle *hp, h;

    for (hp = handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.end != NULL)
            h->vt.end(context, h->data);
    }
}
