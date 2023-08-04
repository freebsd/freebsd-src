/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_preauth.c - Preauthentication routines for the KDC */
/*
 * Copyright 1995, 2003, 2007, 2009 by the Massachusetts Institute of
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
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
/*
 * Copyright (c) 2006-2008, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "kdc_util.h"
#include "extern.h"
#include <stdio.h>
#include "adm_proto.h"

#include <syslog.h>

#include <assert.h>
#include <krb5/kdcpreauth_plugin.h>

/* Let freshness tokens be valid for ten minutes. */
#define FRESHNESS_LIFETIME 600

typedef struct preauth_system_st {
    const char *name;
    int type;
    int flags;
    krb5_kdcpreauth_moddata moddata;
    krb5_kdcpreauth_init_fn init;
    krb5_kdcpreauth_fini_fn fini;
    krb5_kdcpreauth_edata_fn get_edata;
    krb5_kdcpreauth_verify_fn verify_padata;
    krb5_kdcpreauth_return_fn return_padata;
    krb5_kdcpreauth_free_modreq_fn free_modreq;
    krb5_kdcpreauth_loop_fn loop;
} preauth_system;

static preauth_system *preauth_systems;
static size_t n_preauth_systems;

static krb5_error_code
make_etype_info(krb5_context context, krb5_boolean etype_info2,
                krb5_principal client, krb5_key_data *client_key,
                krb5_enctype enctype, krb5_data **der_out);

/* Get all available kdcpreauth vtables and a count of preauth types they
 * support.  Return an empty list on failure. */
static void
get_plugin_vtables(krb5_context context,
                   struct krb5_kdcpreauth_vtable_st **vtables_out,
                   size_t *n_tables_out, size_t *n_systems_out)
{
    krb5_plugin_initvt_fn *plugins = NULL, *pl;
    struct krb5_kdcpreauth_vtable_st *vtables;
    size_t count, n_tables, n_systems, i;

    *vtables_out = NULL;
    *n_tables_out = *n_systems_out = 0;

    /* Auto-register encrypted challenge and (if possible) pkinit. */
    k5_plugin_register_dyn(context, PLUGIN_INTERFACE_KDCPREAUTH, "pkinit",
                           "preauth");
    k5_plugin_register_dyn(context, PLUGIN_INTERFACE_KDCPREAUTH, "otp",
                           "preauth");
    k5_plugin_register_dyn(context, PLUGIN_INTERFACE_KDCPREAUTH, "spake",
                           "preauth");
    k5_plugin_register(context, PLUGIN_INTERFACE_KDCPREAUTH,
                       "encrypted_challenge",
                       kdcpreauth_encrypted_challenge_initvt);
    k5_plugin_register(context, PLUGIN_INTERFACE_KDCPREAUTH,
                       "encrypted_timestamp",
                       kdcpreauth_encrypted_timestamp_initvt);

    if (k5_plugin_load_all(context, PLUGIN_INTERFACE_KDCPREAUTH, &plugins))
        return;
    for (count = 0; plugins[count]; count++);
    vtables = calloc(count + 1, sizeof(*vtables));
    if (vtables == NULL)
        goto cleanup;
    for (pl = plugins, n_tables = 0; *pl != NULL; pl++) {
        if ((*pl)(context, 1, 2, (krb5_plugin_vtable)&vtables[n_tables]) == 0)
            n_tables++;
    }
    for (i = 0, n_systems = 0; i < n_tables; i++) {
        for (count = 0; vtables[i].pa_type_list[count] != 0; count++);
        n_systems += count;
    }
    *vtables_out = vtables;
    *n_tables_out = n_tables;
    *n_systems_out = n_systems;

cleanup:
    k5_plugin_free_modules(context, plugins);
}

/* Make a list of realm names.  The caller should free the list container but
 * not the list elements (which are aliases into kdc_realmlist). */
static krb5_error_code
get_realm_names(struct server_handle *handle, const char ***list_out)
{
    const char **list;
    int i;

    list = calloc(handle->kdc_numrealms + 1, sizeof(*list));
    if (list == NULL)
        return ENOMEM;
    for (i = 0; i < handle->kdc_numrealms; i++)
        list[i] = handle->kdc_realmlist[i]->realm_name;
    list[i] = NULL;
    *list_out = list;
    return 0;
}

void
load_preauth_plugins(struct server_handle *handle, krb5_context context,
                     verto_ctx *ctx)
{
    krb5_error_code ret;
    struct krb5_kdcpreauth_vtable_st *vtables = NULL, *vt;
    size_t n_systems, n_tables, i, j;
    krb5_kdcpreauth_moddata moddata;
    const char **realm_names = NULL, *emsg;
    preauth_system *sys;

    /* Get all available kdcpreauth vtables. */
    get_plugin_vtables(context, &vtables, &n_tables, &n_systems);

    /* Allocate the list of static and plugin preauth systems. */
    preauth_systems = calloc(n_systems + 1, sizeof(preauth_system));
    if (preauth_systems == NULL)
        goto cleanup;

    if (get_realm_names(handle, &realm_names))
        goto cleanup;

    /* Add the dynamically-loaded mechanisms to the list. */
    n_systems = 0;
    for (i = 0; i < n_tables; i++) {
        /* Try to initialize this module. */
        vt = &vtables[i];
        moddata = NULL;
        if (vt->init) {
            ret = vt->init(context, &moddata, realm_names);
            if (ret) {
                emsg = krb5_get_error_message(context, ret);
                krb5_klog_syslog(LOG_ERR, _("preauth %s failed to "
                                            "initialize: %s"), vt->name, emsg);
                krb5_free_error_message(context, emsg);
                continue;
            }
        }

        if (vt->loop) {
            ret = vt->loop(context, moddata, ctx);
            if (ret) {
                emsg = krb5_get_error_message(context, ret);
                krb5_klog_syslog(LOG_ERR, _("preauth %s failed to setup "
                                            "loop: %s"), vt->name, emsg);
                krb5_free_error_message(context, emsg);
                if (vt->fini)
                    vt->fini(context, moddata);
                continue;
            }
        }

        /* Add this module to the systems list once for each pa type. */
        for (j = 0; vt->pa_type_list[j] != 0; j++) {
            sys = &preauth_systems[n_systems];
            sys->name = vt->name;
            sys->type = vt->pa_type_list[j];
            sys->flags = (vt->flags) ? vt->flags(context, sys->type) : 0;
            sys->moddata = moddata;
            sys->init = vt->init;
            /* Only call fini once for each plugin. */
            sys->fini = (j == 0) ? vt->fini : NULL;
            sys->get_edata = vt->edata;
            sys->verify_padata = vt->verify;
            sys->return_padata = vt->return_padata;
            sys->free_modreq = vt->free_modreq;
            sys->loop = vt->loop;
            n_systems++;
        }
    }
    n_preauth_systems = n_systems;
    /* Add the end-of-list marker. */
    preauth_systems[n_systems].name = "[end]";
    preauth_systems[n_systems].type = -1;

cleanup:
    free(vtables);
    free(realm_names);
}

void
unload_preauth_plugins(krb5_context context)
{
    size_t i;

    for (i = 0; i < n_preauth_systems; i++) {
        if (preauth_systems[i].fini)
            preauth_systems[i].fini(context, preauth_systems[i].moddata);
    }
    free(preauth_systems);
    preauth_systems = NULL;
    n_preauth_systems = 0;
}

/*
 * The make_padata_context() function creates a space for storing any
 * request-specific module data which will be needed by return_padata() later.
 * Each preauth type gets a storage location of its own.
 */
struct request_pa_context {
    int n_contexts;
    struct {
        preauth_system *pa_system;
        krb5_kdcpreauth_modreq modreq;
    } *contexts;
};

static krb5_error_code
make_padata_context(krb5_context context, void **padata_context)
{
    int i;
    struct request_pa_context *ret;

    ret = malloc(sizeof(*ret));
    if (ret == NULL) {
        return ENOMEM;
    }

    ret->n_contexts = n_preauth_systems;
    ret->contexts = malloc(sizeof(ret->contexts[0]) * ret->n_contexts);
    if (ret->contexts == NULL) {
        free(ret);
        return ENOMEM;
    }

    memset(ret->contexts, 0, sizeof(ret->contexts[0]) * ret->n_contexts);

    for (i = 0; i < ret->n_contexts; i++) {
        ret->contexts[i].pa_system = &preauth_systems[i];
        ret->contexts[i].modreq = NULL;
    }

    *padata_context = ret;

    return 0;
}

/*
 * The free_padata_context function frees any context information pointers
 * which the check_padata() function created but which weren't already cleaned
 * up by return_padata().
 */
void
free_padata_context(krb5_context kcontext, void *padata_context)
{
    struct request_pa_context *context = padata_context;
    preauth_system *sys;
    int i;

    if (context == NULL)
        return;
    for (i = 0; i < context->n_contexts; i++) {
        sys = context->contexts[i].pa_system;
        if (!sys->free_modreq || !context->contexts[i].modreq)
            continue;
        sys->free_modreq(kcontext, sys->moddata, context->contexts[i].modreq);
        context->contexts[i].modreq = NULL;
    }

    free(context->contexts);
    free(context);
}

static krb5_deltat
max_time_skew(krb5_context context, krb5_kdcpreauth_rock rock)
{
    return context->clockskew;
}

static krb5_error_code
client_keys(krb5_context context, krb5_kdcpreauth_rock rock,
            krb5_keyblock **keys_out)
{
    krb5_kdc_req *request = rock->request;
    krb5_db_entry *client = rock->client;
    krb5_keyblock *keys, key;
    krb5_key_data *entry_key;
    int i, k;

    keys = calloc(request->nktypes + 1, sizeof(krb5_keyblock));
    if (keys == NULL)
        return ENOMEM;

    k = 0;
    for (i = 0; i < request->nktypes; i++) {
        entry_key = NULL;
        if (krb5_dbe_find_enctype(context, client, request->ktype[i],
                                  -1, 0, &entry_key) != 0)
            continue;
        if (krb5_dbe_decrypt_key_data(context, NULL, entry_key,
                                      &key, NULL) != 0)
            continue;
        keys[k++] = key;
    }
    if (k == 0) {
        free(keys);
        return ENOENT;
    }
    *keys_out = keys;
    return 0;
}

static void free_keys(krb5_context context, krb5_kdcpreauth_rock rock,
                      krb5_keyblock *keys)
{
    krb5_keyblock *k;

    if (keys == NULL)
        return;
    for (k = keys; k->enctype != 0; k++)
        krb5_free_keyblock_contents(context, k);
    free(keys);
}

static krb5_data *
request_body(krb5_context context, krb5_kdcpreauth_rock rock)
{
    return rock->inner_body;
}

static krb5_keyblock *
fast_armor(krb5_context context, krb5_kdcpreauth_rock rock)
{
    return rock->rstate->armor_key;
}

static krb5_error_code
get_string(krb5_context context, krb5_kdcpreauth_rock rock, const char *key,
           char **value_out)
{
    return krb5_dbe_get_string(context, rock->client, key, value_out);
}

static void
free_string(krb5_context context, krb5_kdcpreauth_rock rock, char *string)
{
    krb5_dbe_free_string(context, string);
}

static void *
client_entry(krb5_context context, krb5_kdcpreauth_rock rock)
{
    return rock->client;
}

static verto_ctx *
event_context(krb5_context context, krb5_kdcpreauth_rock rock)
{
    return rock->vctx;
}

static krb5_boolean
have_client_keys(krb5_context context, krb5_kdcpreauth_rock rock)
{
    krb5_kdc_req *request = rock->request;
    krb5_key_data *kd;
    int i;

    for (i = 0; i < request->nktypes; i++) {
        if (krb5_dbe_find_enctype(context, rock->client, request->ktype[i],
                                  -1, 0, &kd) == 0)
            return TRUE;
    }
    return FALSE;
}

static const krb5_keyblock *
client_keyblock(krb5_context context, krb5_kdcpreauth_rock rock)
{
    if (rock->client_keyblock->enctype == ENCTYPE_NULL)
        return NULL;
    return rock->client_keyblock;
}

static krb5_error_code
add_auth_indicator(krb5_context context, krb5_kdcpreauth_rock rock,
                   const char *indicator)
{
    return authind_add(context, indicator, rock->auth_indicators);
}

static krb5_boolean
get_cookie(krb5_context context, krb5_kdcpreauth_rock rock,
           krb5_preauthtype pa_type, krb5_data *out)
{
    return kdc_fast_search_cookie(rock->rstate, pa_type, out);
}

static krb5_error_code
set_cookie(krb5_context context, krb5_kdcpreauth_rock rock,
           krb5_preauthtype pa_type, const krb5_data *data)
{
    return kdc_fast_set_cookie(rock->rstate, pa_type, data);
}

static krb5_boolean
match_client(krb5_context context, krb5_kdcpreauth_rock rock,
             krb5_principal princ)
{
    krb5_db_entry *ent;
    krb5_boolean match = FALSE;
    krb5_principal req_client = rock->request->client;
    krb5_principal client = rock->client->princ;

    /* Check for a direct match against the request principal or
     * the post-canon client principal. */
    if (krb5_principal_compare_flags(context, princ, req_client,
                                     KRB5_PRINCIPAL_COMPARE_ENTERPRISE) ||
        krb5_principal_compare(context, princ, client))
        return TRUE;

    if (krb5_db_get_principal(context, princ, KRB5_KDB_FLAG_CLIENT, &ent))
        return FALSE;
    match = krb5_principal_compare(context, ent->princ, client);
    krb5_db_free_principal(context, ent);
    return match;
}

static krb5_principal
client_name(krb5_context context, krb5_kdcpreauth_rock rock)
{
    return rock->client->princ;
}

static void
send_freshness_token(krb5_context context, krb5_kdcpreauth_rock rock)
{
    rock->send_freshness_token = TRUE;
}

static krb5_error_code
check_freshness_token(krb5_context context, krb5_kdcpreauth_rock rock,
                      const krb5_data *token)
{
    krb5_timestamp token_ts, now;
    krb5_key_data *kd;
    krb5_keyblock kb;
    krb5_kvno token_kvno;
    krb5_checksum cksum;
    krb5_data d;
    uint8_t *token_cksum;
    size_t token_cksum_len;
    krb5_boolean valid = FALSE;
    char ckbuf[4];

    memset(&kb, 0, sizeof(kb));

    if (krb5_timeofday(context, &now) != 0)
        goto cleanup;

    if (token->length <= 8)
        goto cleanup;
    token_ts = load_32_be(token->data);
    token_kvno = load_32_be(token->data + 4);
    token_cksum = (uint8_t *)token->data + 8;
    token_cksum_len = token->length - 8;

    /* Check if the token timestamp is too old. */
    if (ts_after(now, ts_incr(token_ts, FRESHNESS_LIFETIME)))
        goto cleanup;

    /* Fetch and decrypt the local krbtgt key of the token's kvno. */
    if (krb5_dbe_find_enctype(context, rock->local_tgt, -1, -1, token_kvno,
                              &kd) != 0)
        goto cleanup;
    if (krb5_dbe_decrypt_key_data(context, NULL, kd, &kb, NULL) != 0)
        goto cleanup;

    /* Verify the token checksum against the current KDC time.  The checksum
     * must use the mandatory checksum type of the krbtgt key's enctype. */
    store_32_be(token_ts, ckbuf);
    d = make_data(ckbuf, sizeof(ckbuf));
    cksum.magic = KV5M_CHECKSUM;
    cksum.checksum_type = 0;
    cksum.length = token_cksum_len;
    cksum.contents = token_cksum;
    (void)krb5_c_verify_checksum(context, &kb, KRB5_KEYUSAGE_PA_AS_FRESHNESS,
                                 &d, &cksum, &valid);

cleanup:
    krb5_free_keyblock_contents(context, &kb);
    return valid ? 0 : KRB5KDC_ERR_PREAUTH_EXPIRED;
}

static krb5_error_code
replace_reply_key(krb5_context context, krb5_kdcpreauth_rock rock,
                  const krb5_keyblock *key, krb5_boolean is_strengthen)
{
    krb5_keyblock copy;

    if (krb5_copy_keyblock_contents(context, key, &copy) != 0)
        return ENOMEM;
    krb5_free_keyblock_contents(context, rock->client_keyblock);
    *rock->client_keyblock = copy;
    if (!is_strengthen)
        rock->replaced_reply_key = TRUE;
    return 0;
}

static struct krb5_kdcpreauth_callbacks_st callbacks = {
    6,
    max_time_skew,
    client_keys,
    free_keys,
    request_body,
    fast_armor,
    get_string,
    free_string,
    client_entry,
    event_context,
    have_client_keys,
    client_keyblock,
    add_auth_indicator,
    get_cookie,
    set_cookie,
    match_client,
    client_name,
    send_freshness_token,
    check_freshness_token,
    replace_reply_key
};

static krb5_error_code
find_pa_system(int type, preauth_system **preauth)
{
    preauth_system *ap;

    if (preauth_systems == NULL)
        return KRB5_PREAUTH_BAD_TYPE;
    ap = preauth_systems;
    while ((ap->type != -1) && (ap->type != type))
        ap++;
    if (ap->type == -1)
        return(KRB5_PREAUTH_BAD_TYPE);
    *preauth = ap;
    return 0;
}

/* Find a pointer to the request-specific module data for pa_sys. */
static krb5_error_code
find_modreq(preauth_system *pa_sys, struct request_pa_context *context,
            krb5_kdcpreauth_modreq **modreq_out)
{
    int i;

    *modreq_out = NULL;
    if (context == NULL)
        return KRB5KRB_ERR_GENERIC;

    for (i = 0; i < context->n_contexts; i++) {
        if (context->contexts[i].pa_system == pa_sys) {
            *modreq_out = &context->contexts[i].modreq;
            return 0;
        }
    }

    return KRB5KRB_ERR_GENERIC;
}

/*
 * Create a list of indices into the preauth_systems array, sorted by order of
 * preference.
 */
static krb5_boolean
pa_list_includes(krb5_pa_data **pa_data, krb5_preauthtype pa_type)
{
    while (*pa_data != NULL) {
        if ((*pa_data)->pa_type == pa_type)
            return TRUE;
        pa_data++;
    }
    return FALSE;
}
static void
sort_pa_order(krb5_context context, krb5_kdc_req *request, int *pa_order)
{
    size_t i, j, k, n_repliers, n_key_replacers;

    /* First, set up the default order. */
    i = 0;
    for (j = 0; j < n_preauth_systems; j++) {
        if (preauth_systems[j].return_padata != NULL)
            pa_order[i++] = j;
    }
    n_repliers = i;
    pa_order[n_repliers] = -1;

    /* Reorder so that PA_REPLACES_KEY modules are listed first. */
    for (i = 0; i < n_repliers; i++) {
        /* If this module replaces the key, then it's okay to leave it where it
         * is in the order. */
        if (preauth_systems[pa_order[i]].flags & PA_REPLACES_KEY)
            continue;
        /* If not, search for a module which does, and swap in the first one we
         * find. */
        for (j = i + 1; j < n_repliers; j++) {
            if (preauth_systems[pa_order[j]].flags & PA_REPLACES_KEY) {
                k = pa_order[j];
                pa_order[j] = pa_order[i];
                pa_order[i] = k;
                break;
            }
        }
        /* If we didn't find one, we have moved all of the key-replacing
         * modules, and i is the count of those modules. */
        if (j == n_repliers)
            break;
    }
    n_key_replacers = i;

    if (request->padata != NULL) {
        /* Now reorder the subset of modules which replace the key,
         * bubbling those which handle pa_data types provided by the
         * client ahead of the others.
         */
        for (i = 0; i < n_key_replacers; i++) {
            if (pa_list_includes(request->padata,
                                 preauth_systems[pa_order[i]].type))
                continue;
            for (j = i + 1; j < n_key_replacers; j++) {
                if (pa_list_includes(request->padata,
                                     preauth_systems[pa_order[j]].type)) {
                    k = pa_order[j];
                    pa_order[j] = pa_order[i];
                    pa_order[i] = k;
                    break;
                }
            }
        }
    }
#ifdef DEBUG
    krb5_klog_syslog(LOG_DEBUG, "original preauth mechanism list:");
    for (i = 0; i < n_preauth_systems; i++) {
        if (preauth_systems[i].return_padata != NULL)
            krb5_klog_syslog(LOG_DEBUG, "... %s(%d)", preauth_systems[i].name,
                             preauth_systems[i].type);
    }
    krb5_klog_syslog(LOG_DEBUG, "sorted preauth mechanism list:");
    for (i = 0; pa_order[i] != -1; i++) {
        krb5_klog_syslog(LOG_DEBUG, "... %s(%d)",
                         preauth_systems[pa_order[i]].name,
                         preauth_systems[pa_order[i]].type);
    }
#endif
}

const char *missing_required_preauth(krb5_db_entry *client,
                                     krb5_db_entry *server,
                                     krb5_enc_tkt_part *enc_tkt_reply)
{
#ifdef DEBUG
    krb5_klog_syslog (
        LOG_DEBUG,
        "client needs %spreauth, %shw preauth; request has %spreauth, %shw preauth",
        isflagset (client->attributes, KRB5_KDB_REQUIRES_PRE_AUTH) ? "" : "no ",
        isflagset (client->attributes, KRB5_KDB_REQUIRES_HW_AUTH) ? "" : "no ",
        isflagset (enc_tkt_reply->flags, TKT_FLG_PRE_AUTH) ? "" : "no ",
        isflagset (enc_tkt_reply->flags, TKT_FLG_HW_AUTH) ? "" : "no ");
#endif

    if (isflagset(client->attributes, KRB5_KDB_REQUIRES_PRE_AUTH) &&
        !isflagset(enc_tkt_reply->flags, TKT_FLG_PRE_AUTH))
        return "NEEDED_PREAUTH";

    if (isflagset(client->attributes, KRB5_KDB_REQUIRES_HW_AUTH) &&
        !isflagset(enc_tkt_reply->flags, TKT_FLG_HW_AUTH))
        return "NEEDED_HW_PREAUTH";

    return 0;
}

/* Return true if request's enctypes indicate support for etype-info2. */
static krb5_boolean
requires_info2(const krb5_kdc_req *request)
{
    int i;

    for (i = 0; i < request->nktypes; i++) {
        if (enctype_requires_etype_info_2(request->ktype[i]))
            return TRUE;
    }
    return FALSE;
}

/* Add PA-ETYPE-INFO2 and possibly PA-ETYPE-INFO entries to pa_list as
 * appropriate for the request and client principal. */
static krb5_error_code
add_etype_info(krb5_context context, krb5_kdcpreauth_rock rock,
               krb5_pa_data ***pa_list)
{
    krb5_error_code ret;
    krb5_data *der;

    if (rock->client_key == NULL)
        return 0;

    if (!requires_info2(rock->request)) {
        /* Include PA-ETYPE-INFO only for old clients. */
        ret = make_etype_info(context, FALSE, rock->client->princ,
                              rock->client_key, rock->client_keyblock->enctype,
                              &der);
        if (ret)
            return ret;
        ret = k5_add_pa_data_from_data(pa_list, KRB5_PADATA_ETYPE_INFO, der);
        krb5_free_data(context, der);
        if (ret)
            return ret;
    }

    /* Always include PA-ETYPE-INFO2. */
    ret = make_etype_info(context, TRUE, rock->client->princ, rock->client_key,
                          rock->client_keyblock->enctype, &der);
    if (ret)
        return ret;
    ret = k5_add_pa_data_from_data(pa_list, KRB5_PADATA_ETYPE_INFO2, der);
    krb5_free_data(context, der);
    return ret;
}

/* Add PW-SALT entries to pa_list as appropriate for the request and client
 * principal. */
static krb5_error_code
add_pw_salt(krb5_context context, krb5_kdcpreauth_rock rock,
            krb5_pa_data ***pa_list)
{
    krb5_error_code ret;
    krb5_data *salt = NULL;
    krb5_int16 salttype;

    /* Only include this pa-data for old clients. */
    if (rock->client_key == NULL || requires_info2(rock->request))
        return 0;

    ret = krb5_dbe_compute_salt(context, rock->client_key,
                                rock->request->client, &salttype, &salt);
    if (ret)
        return 0;

    ret = k5_add_pa_data_from_data(pa_list, KRB5_PADATA_PW_SALT, salt);
    krb5_free_data(context, salt);
    return ret;
}

static krb5_error_code
add_freshness_token(krb5_context context, krb5_kdcpreauth_rock rock,
                    krb5_pa_data ***pa_list)
{
    krb5_error_code ret;
    krb5_timestamp now;
    krb5_keyblock kb;
    krb5_checksum cksum;
    krb5_data d;
    krb5_pa_data *pa = NULL;
    char ckbuf[4];

    memset(&cksum, 0, sizeof(cksum));
    memset(&kb, 0, sizeof(kb));

    if (!rock->send_freshness_token)
        return 0;
    if (krb5int_find_pa_data(context, rock->request->padata,
                             KRB5_PADATA_AS_FRESHNESS) == NULL)
        return 0;

    /* Compute a checksum over the current KDC time. */
    ret = krb5_timeofday(context, &now);
    if (ret)
        goto cleanup;
    store_32_be(now, ckbuf);
    d = make_data(ckbuf, sizeof(ckbuf));
    ret = krb5_c_make_checksum(context, 0, rock->local_tgt_key,
                               KRB5_KEYUSAGE_PA_AS_FRESHNESS, &d, &cksum);

    /* Compose a freshness token from the time, krbtgt kvno, and checksum. */
    ret = k5_alloc_pa_data(KRB5_PADATA_AS_FRESHNESS, 8 + cksum.length, &pa);
    if (ret)
        goto cleanup;
    store_32_be(now, pa->contents);
    store_32_be(current_kvno(rock->local_tgt), pa->contents + 4);
    memcpy(pa->contents + 8, cksum.contents, cksum.length);

    ret = k5_add_pa_data_element(pa_list, &pa);

cleanup:
    krb5_free_keyblock_contents(context, &kb);
    krb5_free_checksum_contents(context, &cksum);
    k5_free_pa_data_element(pa);
    return ret;
}

struct hint_state {
    kdc_hint_respond_fn respond;
    void *arg;
    krb5_context context;

    krb5_kdcpreauth_rock rock;
    krb5_kdc_req *request;
    krb5_pa_data ***e_data_out;

    int hw_only;
    preauth_system *ap;
    krb5_pa_data **pa_data;
    krb5_preauthtype pa_type;
};

static void
hint_list_finish(struct hint_state *state, krb5_error_code code)
{
    krb5_context context = state->context;
    kdc_hint_respond_fn oldrespond = state->respond;
    void *oldarg = state->arg;

    /* Add a freshness token if a preauth module requested it and the client
     * request indicates support for it. */
    if (!code)
        code = add_freshness_token(context, state->rock, &state->pa_data);

    if (!code) {
        if (state->pa_data == NULL) {
            krb5_klog_syslog(LOG_INFO,
                             _("%spreauth required but hint list is empty"),
                             state->hw_only ? "hw" : "");
        }

        *state->e_data_out = state->pa_data;
        state->pa_data = NULL;
    }

    krb5_free_pa_data(context, state->pa_data);
    free(state);
    (*oldrespond)(oldarg);
}

static void
hint_list_next(struct hint_state *arg);

static void
finish_get_edata(void *arg, krb5_error_code code, krb5_pa_data *pa)
{
    krb5_error_code ret;
    struct hint_state *state = arg;

    if (code == 0) {
        if (pa == NULL) {
            ret = k5_alloc_pa_data(state->pa_type, 0, &pa);
            if (ret)
                goto error;
        }
        ret = k5_add_pa_data_element(&state->pa_data, &pa);
        k5_free_pa_data_element(pa);
        if (ret)
            goto error;
    }

    state->ap++;
    hint_list_next(state);
    return;

error:
    hint_list_finish(state, ret);
}

static void
hint_list_next(struct hint_state *state)
{
    krb5_context context = state->context;
    preauth_system *ap = state->ap;

    if (ap->type == -1) {
        hint_list_finish(state, 0);
        return;
    }

    if (state->hw_only && !(ap->flags & PA_HARDWARE))
        goto next;
    if (ap->flags & PA_PSEUDO)
        goto next;

    state->pa_type = ap->type;
    if (ap->get_edata) {
        ap->get_edata(context, state->request, &callbacks, state->rock,
                      ap->moddata, ap->type, finish_get_edata, state);
    } else
        finish_get_edata(state, 0, NULL);
    return;

next:
    state->ap++;
    hint_list_next(state);
}

void
get_preauth_hint_list(krb5_kdc_req *request, krb5_kdcpreauth_rock rock,
                      krb5_pa_data ***e_data_out, kdc_hint_respond_fn respond,
                      void *arg)
{
    krb5_context context = rock->rstate->realm_data->realm_context;
    struct hint_state *state;

    *e_data_out = NULL;

    /* Allocate our state. */
    state = calloc(1, sizeof(*state));
    if (state == NULL)
        goto error;
    state->hw_only = isflagset(rock->client->attributes,
                               KRB5_KDB_REQUIRES_HW_AUTH);
    state->respond = respond;
    state->arg = arg;
    state->request = request;
    state->rock = rock;
    state->context = context;
    state->e_data_out = e_data_out;
    state->pa_data = NULL;
    state->ap = preauth_systems;

    /* Add an empty PA-FX-FAST element to advertise FAST support. */
    if (k5_add_empty_pa_data(&state->pa_data, KRB5_PADATA_FX_FAST) != 0)
        goto error;

    if (add_etype_info(context, rock, &state->pa_data) != 0)
        goto error;

    hint_list_next(state);
    return;

error:
    if (state != NULL)
        krb5_free_pa_data(context, state->pa_data);
    free(state);
    (*respond)(arg);
}

/*
 * Add authorization data returned from preauth modules to the ticket
 * It is assumed that ad is a "null-terminated" array of krb5_authdata ptrs
 */
static krb5_error_code
add_authorization_data(krb5_enc_tkt_part *enc_tkt_part, krb5_authdata **ad)
{
    krb5_authdata **newad;
    int oldones, newones;
    int i;

    if (enc_tkt_part == NULL || ad == NULL)
        return EINVAL;

    for (newones = 0; ad[newones] != NULL; newones++);
    if (newones == 0)
        return 0;   /* nothing to add */

    if (enc_tkt_part->authorization_data == NULL)
        oldones = 0;
    else
        for (oldones = 0;
             enc_tkt_part->authorization_data[oldones] != NULL; oldones++);

    newad = malloc((oldones + newones + 1) * sizeof(krb5_authdata *));
    if (newad == NULL)
        return ENOMEM;

    /* Copy any existing pointers */
    for (i = 0; i < oldones; i++)
        newad[i] = enc_tkt_part->authorization_data[i];

    /* Add the new ones */
    for (i = 0; i < newones; i++)
        newad[oldones+i] = ad[i];

    /* Terminate the new list */
    newad[oldones+i] = NULL;

    /* Free any existing list */
    if (enc_tkt_part->authorization_data != NULL)
        free(enc_tkt_part->authorization_data);

    /* Install our new list */
    enc_tkt_part->authorization_data = newad;

    return 0;
}

struct padata_state {
    kdc_preauth_respond_fn respond;
    void *arg;
    kdc_realm_t *realm;

    krb5_kdcpreauth_modreq *modreq_ptr;
    krb5_pa_data **padata;
    int pa_found;
    krb5_context context;
    krb5_kdcpreauth_rock rock;
    krb5_data *req_pkt;
    krb5_kdc_req *request;
    krb5_enc_tkt_part *enc_tkt_reply;
    void **padata_context;

    preauth_system *pa_sys;
    krb5_pa_data **pa_e_data;
    krb5_boolean typed_e_data_flag;
    int pa_ok;
    krb5_error_code saved_code;

    krb5_pa_data ***e_data_out;
    krb5_boolean *typed_e_data_out;
};

/* Return code if it is 0 or one of the codes we pass through to the client.
 * Otherwise return KRB5KDC_ERR_PREAUTH_FAILED. */
static krb5_error_code
filter_preauth_error(krb5_error_code code)
{
    /* The following switch statement allows us
     * to return some preauth system errors back to the client.
     */
    switch(code) {
    case 0:
    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
    case KRB5KRB_AP_ERR_SKEW:
    case KRB5KDC_ERR_PREAUTH_REQUIRED:
    case KRB5KDC_ERR_ETYPE_NOSUPP:
        /* rfc 4556 */
    case KRB5KDC_ERR_CLIENT_NOT_TRUSTED:
    case KRB5KDC_ERR_INVALID_SIG:
    case KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED:
    case KRB5KDC_ERR_CANT_VERIFY_CERTIFICATE:
    case KRB5KDC_ERR_INVALID_CERTIFICATE:
    case KRB5KDC_ERR_REVOKED_CERTIFICATE:
    case KRB5KDC_ERR_REVOCATION_STATUS_UNKNOWN:
    case KRB5KDC_ERR_CLIENT_NAME_MISMATCH:
    case KRB5KDC_ERR_INCONSISTENT_KEY_PURPOSE:
    case KRB5KDC_ERR_DIGEST_IN_CERT_NOT_ACCEPTED:
    case KRB5KDC_ERR_PA_CHECKSUM_MUST_BE_INCLUDED:
    case KRB5KDC_ERR_DIGEST_IN_SIGNED_DATA_NOT_ACCEPTED:
    case KRB5KDC_ERR_PUBLIC_KEY_ENCRYPTION_NOT_SUPPORTED:
        /* earlier drafts of what became rfc 4556 */
    case KRB5KDC_ERR_CERTIFICATE_MISMATCH:
    case KRB5KDC_ERR_KDC_NOT_TRUSTED:
    case KRB5KDC_ERR_REVOCATION_STATUS_UNAVAILABLE:
        /* This value is shared with
         *     KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED. */
        /* case KRB5KDC_ERR_KEY_TOO_WEAK: */
    case KRB5KDC_ERR_DISCARD:
        /* pkinit alg-agility */
    case KRB5KDC_ERR_NO_ACCEPTABLE_KDF:
        /* rfc 6113 */
    case KRB5KDC_ERR_MORE_PREAUTH_DATA_REQUIRED:
        return code;
    default:
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }
}

/*
 * If the client performed optimistic pre-authentication for a multi-round-trip
 * mechanism, it may need key information to complete the exchange, so send it
 * a PA-ETYPE-INFO2 element in addition to the pa-data from the module.
 */
static krb5_error_code
maybe_add_etype_info2(struct padata_state *state, krb5_error_code code)
{
    krb5_error_code ret;
    krb5_context context = state->context;
    krb5_kdcpreauth_rock rock = state->rock;
    krb5_data *der;

    /* Only add key information when requesting another preauth round trip. */
    if (code != KRB5KDC_ERR_MORE_PREAUTH_DATA_REQUIRED)
        return 0;

    /* Don't try to add key information when there is no key. */
    if (rock->client_key == NULL)
        return 0;

    /* If the client sent a cookie, it has already seen a KDC response with key
     * information. */
    if (krb5int_find_pa_data(context, state->request->padata,
                             KRB5_PADATA_FX_COOKIE) != NULL)
        return 0;

    ret = make_etype_info(context, TRUE, rock->client->princ, rock->client_key,
                          rock->client_keyblock->enctype, &der);
    if (ret)
        return ret;
    ret = k5_add_pa_data_from_data(&state->pa_e_data, KRB5_PADATA_ETYPE_INFO2,
                                   der);
    krb5_free_data(context, der);
    return ret;
}

/* Release state and respond to the AS-REQ processing code with the result of
 * checking pre-authentication data. */
static void
finish_check_padata(struct padata_state *state, krb5_error_code code)
{
    kdc_preauth_respond_fn respond;
    void *arg;

    if (state->pa_ok || !state->pa_found) {
        /* Return successfully.  If we didn't match a preauth system, we may
         * return PREAUTH_REQUIRED later, but we didn't fail to verify. */
        code = 0;
        goto cleanup;
    }

    /* Add key information to the saved error pa-data if required. */
    if (maybe_add_etype_info2(state, code) != 0) {
        code = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    /* Return any saved error pa-data, stealing the pointer from state. */
    *state->e_data_out = state->pa_e_data;
    *state->typed_e_data_out = state->typed_e_data_flag;
    state->pa_e_data = NULL;

cleanup:
    /* Discard saved error pa-data if we aren't returning it, free state, and
     * respond to the AS-REQ processing code. */
    respond = state->respond;
    arg = state->arg;
    krb5_free_pa_data(state->context, state->pa_e_data);
    free(state);
    (*respond)(arg, filter_preauth_error(code));
}

static void
next_padata(struct padata_state *state);

static void
finish_verify_padata(void *arg, krb5_error_code code,
                     krb5_kdcpreauth_modreq modreq, krb5_pa_data **e_data,
                     krb5_authdata **authz_data)
{
    struct padata_state *state = arg;
    const char *emsg;
    krb5_boolean typed_e_data_flag;

    assert(state);
    *state->modreq_ptr = modreq;

    if (code) {
        emsg = krb5_get_error_message(state->context, code);
        krb5_klog_syslog(LOG_INFO, "preauth (%s) verify failure: %s",
                         state->pa_sys->name, emsg);
        krb5_free_error_message(state->context, emsg);

        /* Ignore authorization data returned from modules that fail */
        if (authz_data != NULL) {
            krb5_free_authdata(state->context, authz_data);
            authz_data = NULL;
        }

        typed_e_data_flag = ((state->pa_sys->flags & PA_TYPED_E_DATA) != 0);

        /*
         * We'll return edata from either the first PA_REQUIRED module
         * that fails, or the first non-PA_REQUIRED module that fails.
         * Hang on to edata from the first non-PA_REQUIRED module.
         * If we've already got one saved, simply discard this one.
         */
        if (state->pa_sys->flags & PA_REQUIRED) {
            /* free up any previous edata we might have been saving */
            if (state->pa_e_data != NULL)
                krb5_free_pa_data(state->context, state->pa_e_data);
            state->pa_e_data = e_data;
            state->typed_e_data_flag = typed_e_data_flag;

            /* Make sure we use the current retval */
            state->pa_ok = 0;
            finish_check_padata(state, code);
            return;
        } else if (state->pa_e_data == NULL) {
            /* save the first error code and e-data */
            state->pa_e_data = e_data;
            state->typed_e_data_flag = typed_e_data_flag;
            state->saved_code = code;
        } else if (e_data != NULL) {
            /* discard this extra e-data from non-PA_REQUIRED module */
            krb5_free_pa_data(state->context, e_data);
        }
    } else {
#ifdef DEBUG
        krb5_klog_syslog (LOG_DEBUG, ".. .. ok");
#endif

        /* Ignore any edata returned on success */
        if (e_data != NULL)
            krb5_free_pa_data(state->context, e_data);

        /* Add any authorization data to the ticket */
        if (authz_data != NULL) {
            add_authorization_data(state->enc_tkt_reply, authz_data);
            free(authz_data);
        }

        state->pa_ok = 1;
        if (state->pa_sys->flags & PA_SUFFICIENT) {
            finish_check_padata(state, state->saved_code);
            return;
        }
    }

    next_padata(state);
}

static void
next_padata(struct padata_state *state)
{
    assert(state);
    if (!state->padata)
        state->padata = state->request->padata;
    else
        state->padata++;

    if (!*state->padata) {
        finish_check_padata(state, state->saved_code);
        return;
    }

#ifdef DEBUG
    krb5_klog_syslog (LOG_DEBUG, ".. pa_type 0x%x", (*state->padata)->pa_type);
#endif
    if (find_pa_system((*state->padata)->pa_type, &state->pa_sys))
        goto next;
    if (find_modreq(state->pa_sys, *state->padata_context, &state->modreq_ptr))
        goto next;
#ifdef DEBUG
    krb5_klog_syslog (LOG_DEBUG, ".. pa_type %s", state->pa_sys->name);
#endif
    if (state->pa_sys->verify_padata == 0)
        goto next;

    state->pa_found++;
    state->pa_sys->verify_padata(state->context, state->req_pkt,
                                 state->request, state->enc_tkt_reply,
                                 *state->padata, &callbacks, state->rock,
                                 state->pa_sys->moddata, finish_verify_padata,
                                 state);
    return;

next:
    next_padata(state);
}

/*
 * This routine is called to verify the preauthentication information
 * for a V5 request.
 *
 * Returns 0 if the pre-authentication is valid, non-zero to indicate
 * an error code of some sort.
 */

void
check_padata(krb5_context context, krb5_kdcpreauth_rock rock,
             krb5_data *req_pkt, krb5_kdc_req *request,
             krb5_enc_tkt_part *enc_tkt_reply, void **padata_context,
             krb5_pa_data ***e_data, krb5_boolean *typed_e_data,
             kdc_preauth_respond_fn respond, void *arg)
{
    struct padata_state *state;

    if (request->padata == 0) {
        (*respond)(arg, 0);
        return;
    }

    if (make_padata_context(context, padata_context) != 0) {
        (*respond)(arg, KRB5KRB_ERR_GENERIC);
        return;
    }

    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        (*respond)(arg, ENOMEM);
        return;
    }
    state->respond = respond;
    state->arg = arg;
    state->context = context;
    state->rock = rock;
    state->req_pkt = req_pkt;
    state->request = request;
    state->enc_tkt_reply = enc_tkt_reply;
    state->padata_context = padata_context;
    state->e_data_out = e_data;
    state->typed_e_data_out = typed_e_data;
    state->realm = rock->rstate->realm_data;

#ifdef DEBUG
    krb5_klog_syslog (LOG_DEBUG, "checking padata");
#endif

    next_padata(state);
}

/* Return true if k1 and k2 have the same type and contents. */
static krb5_boolean
keyblock_equal(const krb5_keyblock *k1, const krb5_keyblock *k2)
{
    if (k1->enctype != k2->enctype)
        return FALSE;
    if (k1->length != k2->length)
        return FALSE;
    return memcmp(k1->contents, k2->contents, k1->length) == 0;
}

/*
 * return_padata creates any necessary preauthentication
 * structures which should be returned by the KDC to the client
 */
krb5_error_code
return_padata(krb5_context context, krb5_kdcpreauth_rock rock,
              krb5_data *req_pkt, krb5_kdc_req *request, krb5_kdc_rep *reply,
              krb5_keyblock *encrypting_key, void **padata_context)
{
    krb5_error_code             retval;
    krb5_pa_data **             padata;
    krb5_pa_data **             send_pa_list = NULL;
    krb5_pa_data *              send_pa;
    krb5_pa_data *              pa = 0;
    krb5_pa_data null_item;
    preauth_system *            ap;
    int *                       pa_order = NULL;
    int *                       pa_type;
    int                         size = 0;
    krb5_kdcpreauth_modreq      *modreq_ptr;
    krb5_boolean                key_modified;
    krb5_keyblock               original_key;

    memset(&original_key, 0, sizeof(original_key));

    if ((!*padata_context) &&
        (make_padata_context(context, padata_context) != 0)) {
        return KRB5KRB_ERR_GENERIC;
    }

    for (ap = preauth_systems; ap->type != -1; ap++) {
        if (ap->return_padata)
            size++;
    }

    pa_order = k5calloc(size + 1, sizeof(int), &retval);
    if (pa_order == NULL)
        goto cleanup;
    sort_pa_order(context, request, pa_order);

    retval = krb5_copy_keyblock_contents(context, encrypting_key,
                                         &original_key);
    if (retval)
        goto cleanup;
    key_modified = FALSE;
    null_item.contents = NULL;
    null_item.length = 0;

    for (pa_type = pa_order; *pa_type != -1; pa_type++) {
        ap = &preauth_systems[*pa_type];
        if (key_modified && (ap->flags & PA_REPLACES_KEY))
            continue;
        if (ap->return_padata == 0)
            continue;
        if (find_modreq(ap, *padata_context, &modreq_ptr))
            continue;
        pa = &null_item;
        null_item.pa_type = ap->type;
        if (request->padata) {
            for (padata = request->padata; *padata; padata++) {
                if ((*padata)->pa_type == ap->type) {
                    pa = *padata;
                    break;
                }
            }
        }
        send_pa = NULL;
        retval = ap->return_padata(context, pa, req_pkt, request, reply,
                                   encrypting_key, &send_pa, &callbacks, rock,
                                   ap->moddata, *modreq_ptr);
        if (retval)
            goto cleanup;

        if (send_pa != NULL) {
            retval = k5_add_pa_data_element(&send_pa_list, &send_pa);
            k5_free_pa_data_element(send_pa);
            if (retval)
                goto cleanup;
        }

        if (!key_modified && !keyblock_equal(&original_key, encrypting_key))
            key_modified = TRUE;
    }

    /*
     * Add etype-info and pw-salt pa-data as needed.  If we replaced the reply
     * key, we can't send consistent etype-info; the salt from the client key
     * data doesn't correspond to the replaced reply key, and RFC 4120 section
     * 5.2.7.5 forbids us from sending etype-info describing the initial reply
     * key in an AS-REP if it doesn't have the same enctype as the replaced
     * reply key.  For all current and foreseeable preauth mechs, we can assume
     * the client received etype-info2 in an earlier step and already computed
     * the initial reply key if it needed it.  The client can determine the
     * enctype of the replaced reply key from the etype field of the enc-part
     * field of the AS-REP.
     */
    if (!key_modified) {
        retval = add_etype_info(context, rock, &send_pa_list);
        if (retval)
            goto cleanup;
        retval = add_pw_salt(context, rock, &send_pa_list);
        if (retval)
            goto cleanup;
    }

    if (send_pa_list != NULL) {
        reply->padata = send_pa_list;
        send_pa_list = 0;
    }

cleanup:
    krb5_free_keyblock_contents(context, &original_key);
    free(pa_order);
    krb5_free_pa_data(context, send_pa_list);

    return (retval);
}

static krb5_error_code
_make_etype_info_entry(krb5_context context,
                       krb5_principal client_princ, krb5_key_data *client_key,
                       krb5_enctype etype, krb5_etype_info_entry **entry_out,
                       int etype_info2)
{
    krb5_error_code retval;
    krb5_int16 salttype;
    krb5_data *salt = NULL;
    krb5_etype_info_entry *entry = NULL;

    *entry_out = NULL;
    entry = malloc(sizeof(*entry));
    if (entry == NULL)
        return ENOMEM;

    entry->magic = KV5M_ETYPE_INFO_ENTRY;
    entry->etype = etype;
    entry->length = KRB5_ETYPE_NO_SALT;
    entry->salt = NULL;
    entry->s2kparams = empty_data();
    retval = krb5_dbe_compute_salt(context, client_key, client_princ,
                                   &salttype, &salt);
    if (retval)
        goto cleanup;

    entry->length = salt->length;
    entry->salt = (unsigned char *)salt->data;
    salt->data = NULL;
    *entry_out = entry;
    entry = NULL;

cleanup:
    if (entry != NULL)
        krb5_free_data_contents(context, &entry->s2kparams);
    free(entry);
    krb5_free_data(context, salt);
    return retval;
}

/* Encode an etype-info or etype-info2 message for client_key with the given
 * enctype, using client to compute the salt if necessary. */
static krb5_error_code
make_etype_info(krb5_context context, krb5_boolean etype_info2,
                krb5_principal client, krb5_key_data *client_key,
                krb5_enctype enctype, krb5_data **der_out)
{
    krb5_error_code retval;
    krb5_etype_info_entry **entry = NULL;

    *der_out = NULL;

    entry = k5calloc(2, sizeof(*entry), &retval);
    if (entry == NULL)
        goto cleanup;
    retval = _make_etype_info_entry(context, client, client_key, enctype,
                                    &entry[0], etype_info2);
    if (retval != 0)
        goto cleanup;

    if (etype_info2)
        retval = encode_krb5_etype_info2(entry, der_out);
    else
        retval = encode_krb5_etype_info(entry, der_out);

cleanup:
    krb5_free_etype_info(context, entry);
    return retval;
}

/*
 * Returns TRUE if the PAC should be included
 */
krb5_boolean
include_pac_p(krb5_context context, krb5_kdc_req *request)
{
    krb5_error_code             code;
    krb5_pa_data                **padata;
    krb5_boolean                retval = TRUE; /* default is to return PAC */
    krb5_data                   data;
    krb5_pa_pac_req             *req = NULL;

    if (request->padata == NULL) {
        return retval;
    }

    for (padata = request->padata; *padata != NULL; padata++) {
        if ((*padata)->pa_type == KRB5_PADATA_PAC_REQUEST) {
            data.data = (char *)(*padata)->contents;
            data.length = (*padata)->length;

            code = decode_krb5_pa_pac_req(&data, &req);
            if (code == 0) {
                retval = req->include_pac;
                krb5_free_pa_pac_req(context, req);
                req = NULL;
            }
            break;
        }
    }

    return retval;
}

static krb5_error_code
return_referral_enc_padata( krb5_context context,
                            krb5_enc_kdc_rep_part *reply,
                            krb5_db_entry *server)
{
    krb5_error_code             code;
    krb5_tl_data                tl_data;
    krb5_pa_data                *pa;

    tl_data.tl_data_type = KRB5_TL_SVR_REFERRAL_DATA;
    code = krb5_dbe_lookup_tl_data(context, server, &tl_data);
    if (code || tl_data.tl_data_length == 0)
        return 0;

    code = k5_alloc_pa_data(KRB5_PADATA_SVR_REFERRAL_INFO,
                            tl_data.tl_data_length, &pa);
    if (code)
        return code;
    memcpy(pa->contents, tl_data.tl_data_contents, tl_data.tl_data_length);
    code = k5_add_pa_data_element(&reply->enc_padata, &pa);
    k5_free_pa_data_element(pa);
    return code;
}

krb5_error_code
return_enc_padata(krb5_context context, krb5_data *req_pkt,
                  krb5_kdc_req *request, krb5_keyblock *reply_key,
                  krb5_db_entry *server, krb5_enc_kdc_rep_part *reply_encpart,
                  krb5_boolean is_referral)
{
    krb5_error_code code = 0;
    /* This should be initialized and only used for Win2K compat and other
     * specific standardized uses such as FAST negotiation. */
    if (is_referral) {
        code = return_referral_enc_padata(context, reply_encpart, server);
        if (code)
            return code;
    }
    code = kdc_handle_protected_negotiation(context, req_pkt, request, reply_key,
                                            &reply_encpart->enc_padata);
    if (code)
        goto cleanup;

    code = kdc_add_pa_pac_options(context, request,
                                  &reply_encpart->enc_padata);
    if (code)
        goto cleanup;

    /*Add potentially other enc_padata providers*/
cleanup:
    return code;
}
