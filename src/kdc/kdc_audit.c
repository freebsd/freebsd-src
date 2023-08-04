/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc_audit.c - Interface for KDC audit plugins. */
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
#include "kdc_util.h"
#include "kdc_audit.h"
/* for krb5_klog_syslog */
#include <syslog.h>
#include "adm_proto.h"

struct audit_module_handle_st {
    struct krb5_audit_vtable_st vt;
    krb5_audit_moddata auctx;
};
typedef struct audit_module_handle_st *audit_module_handle;

static audit_module_handle *handles = NULL;

static void
free_handles(audit_module_handle *list)
{
    audit_module_handle *hp, hdl;

    if (list == NULL)
        return;

    for (hp = list; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.close != NULL)
            hdl->vt.close(hdl->auctx);
        free(hdl);
    }
    free(list);
}

/*
 * Load all available audit plugin modules and prepare for logging. The list of
 * modules is stored as an array in handles. Use unload_audit_modules() to free
 * resources allocated by this function.
 */
krb5_error_code
load_audit_modules(krb5_context context)
{
    krb5_error_code ret = 0;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    struct krb5_audit_vtable_st vtable;
    audit_module_handle *list = NULL, hdl = NULL;
    krb5_audit_moddata auctx;
    int count = 0;

    if (context == NULL || handles != NULL)
        return EINVAL;

    /* Get audit plugin vtable. */
    ret = k5_plugin_load_all(context, PLUGIN_INTERFACE_AUDIT, &modules);
    if (ret)
        return ret;

    /* Allocate handle, initialize vtable. */
    for (count = 0; modules[count] != NULL; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        goto cleanup;
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        hdl = k5alloc(sizeof(*hdl), &ret);
        if (hdl == NULL)
            goto cleanup;
        ret = (*mod)(context, 1, 1, (krb5_plugin_vtable)&hdl->vt);
        if (ret) {
            free(hdl);
            hdl = NULL;
            continue;
        }

        vtable = hdl->vt;
        if (vtable.open != NULL) {
            ret = vtable.open(&auctx);
            if (ret) {
                krb5_klog_syslog(LOG_ERR,
                                 _("audit plugin %s failed to open. error=%i"),
                                 vtable.name, ret);
                goto cleanup;
            }
            hdl->auctx = auctx;
        }
        list[count++] = hdl;
        list[count] = NULL;
        hdl = NULL;
    }
    list[count] = NULL;
    handles = list;
    list = NULL;
    ret = 0;

cleanup:
    free(hdl);
    k5_plugin_free_modules(context, modules);
    free_handles(list);
    return ret;
}

/* Free resources allocated by load_audit_modules() function. */
void
unload_audit_modules(krb5_context context)
{
    free_handles(handles);
}

/*
 * Write the output ticket ID into newly-allocated buffer.
 * Returns 0 on success.
 */
krb5_error_code
kau_make_tkt_id(krb5_context context,
                const krb5_ticket *ticket, char **out)
{
    krb5_error_code ret = 0;
    char *hash = NULL, *ptr;
    uint8_t hashbytes[K5_SHA256_HASHLEN];
    unsigned int i;

    *out = NULL;

    if (ticket == NULL)
        return EINVAL;

    ret = k5_sha256(&ticket->enc_part.ciphertext, 1, hashbytes);
    if (ret)
        return ret;

    hash = k5alloc(sizeof(hashbytes) * 2 + 1, &ret);
    if (hash == NULL)
        return ret;

    for (i = 0, ptr = hash; i < sizeof(hashbytes); i++, ptr += 2)
        snprintf(ptr, 3, "%02X", hashbytes[i]);
    *ptr = '\0';
    *out = hash;

    return 0;
}

/*
 * Create and initialize krb5_audit_state structure.
 * Returns 0 on success.
 */
krb5_error_code
kau_init_kdc_req(krb5_context context,
                 krb5_kdc_req *request, const krb5_fulladdr *from,
                 krb5_audit_state **state_out)
{
    krb5_error_code ret = 0;
    krb5_audit_state *state = NULL;

    state = k5calloc(1, sizeof(*state), &ret);
    if (state == NULL)
        return ret;

    state->request = request;
    state->cl_addr = from->address;
    state->cl_port = from->port;
    state->stage = AUTHN_REQ_CL;
    ret = krb5int_random_string(context, state->req_id,
                                sizeof(state->req_id));
    if (ret) {
        free(state);
        return ret;
    }
    *state_out = state;

    return 0;
}

/* Free resources allocated by kau_init_kdc_req() and kau_make_tkt_id()
 * routines. */
void
kau_free_kdc_req(krb5_audit_state *state)
{
    if (state == NULL)
        return;
    free(state->tkt_in_id);
    free(state->tkt_out_id);
    free(state->evid_tkt_id);
    free(state);
}

/* Call the KDC start/stop audit plugin entry points. */

void
kau_kdc_stop(krb5_context context, const krb5_boolean ev_success)
{
    audit_module_handle *hp, hdl;

    if (handles == NULL)
        return;

    for (hp = handles; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.kdc_stop != NULL)
            hdl->vt.kdc_stop(hdl->auctx, ev_success);
    }
}

void
kau_kdc_start(krb5_context context, const krb5_boolean ev_success)
{
    audit_module_handle *hp, hdl;

    if (handles == NULL)
        return;

    for (hp = handles; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.kdc_start != NULL)
            hdl->vt.kdc_start(hdl->auctx, ev_success);
    }
}

/* Call the AS-REQ audit plugin entry point. */
void
kau_as_req(krb5_context context, const krb5_boolean ev_success,
           krb5_audit_state *state)
{
    audit_module_handle *hp, hdl;

    if (handles == NULL)
        return;

    for (hp = handles; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.as_req != NULL)
            hdl->vt.as_req(hdl->auctx, ev_success, state);
    }
}

/* Call the TGS-REQ audit plugin entry point. */
void
kau_tgs_req(krb5_context context, const krb5_boolean ev_success,
            krb5_audit_state *state)
{
    audit_module_handle *hp, hdl;

    if (handles == NULL)
        return;

    for (hp = handles; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.tgs_req != NULL)
            hdl->vt.tgs_req(hdl->auctx, ev_success, state);
    }
}

/* Call the S4U2Self audit plugin entry point. */
void
kau_s4u2self(krb5_context context, const krb5_boolean ev_success,
             krb5_audit_state *state)
{
    audit_module_handle *hp, hdl;

    if (handles == NULL)
        return;

    for (hp = handles; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.tgs_s4u2self != NULL)
            hdl->vt.tgs_s4u2self(hdl->auctx, ev_success, state);
    }
}

/* Call the S4U2Proxy audit plugin entry point. */
void
kau_s4u2proxy(krb5_context context,const krb5_boolean ev_success,
              krb5_audit_state *state)
{
    audit_module_handle *hp, hdl;

    if (handles == NULL)
        return;

    for (hp = handles; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.tgs_s4u2proxy != NULL)
            hdl->vt.tgs_s4u2proxy(hdl->auctx, ev_success, state);
    }
}

/* Call the U2U audit plugin entry point. */
void
kau_u2u(krb5_context context, const krb5_boolean ev_success,
        krb5_audit_state *state)
{
    audit_module_handle *hp, hdl;

    if (handles == NULL)
        return;

    for (hp = handles; *hp != NULL; hp++) {
        hdl = *hp;
        if (hdl->vt.tgs_u2u != NULL)
            hdl->vt.tgs_u2u(hdl->auctx, ev_success, state);
    }
}
