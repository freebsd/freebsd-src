/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1995, 2003, 2008, 2012 by the Massachusetts Institute of Technology.  All
 * Rights Reserved.
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
 *
 */

/*
 * This file contains routines for establishing, verifying, and any other
 * necessary functions, for utilizing the pre-authentication field of the
 * kerberos kdc request, with various hardware/software verification devices.
 */

#include "k5-int.h"
#include "k5-json.h"
#include "osconf.h"
#include <krb5/clpreauth_plugin.h>
#include "int-proto.h"
#include "os-proto.h"
#include "fast.h"
#include "init_creds_ctx.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

typedef struct {
    struct krb5_clpreauth_vtable_st vt;
    krb5_clpreauth_moddata data;
} *clpreauth_handle;

struct krb5_preauth_context_st {
    clpreauth_handle *handles;
};

struct krb5_preauth_req_context_st {
    krb5_context orig_context;
    krb5_preauthtype *failed;
    krb5_clpreauth_modreq *modreqs;
};

/* Release the memory used by a list of handles. */
static void
free_handles(krb5_context context, clpreauth_handle *handles)
{
    clpreauth_handle *hp, h;

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

/* Return an index into handles which can process pa_type, or -1 if none is
 * found found. */
static int
search_module_list(clpreauth_handle *handles, krb5_preauthtype pa_type)
{
    clpreauth_handle h;
    int i, j;

    for (i = 0; handles[i] != NULL; i++) {
        h = handles[i];
        for (j = 0; h->vt.pa_type_list[j] != 0; j++) {
            if (h->vt.pa_type_list[j] == pa_type)
                return i;
        }
    }
    return -1;
}

/* Find the handle which can process pa_type, or NULL if none is found.  On
 * success, set *modreq_out to the corresponding per-request module data. */
static clpreauth_handle
find_module(krb5_context context, krb5_init_creds_context ctx,
            krb5_preauthtype pa_type, krb5_clpreauth_modreq *modreq_out)
{
    krb5_preauth_context pctx = context->preauth_context;
    krb5_preauth_req_context reqctx = ctx->preauth_reqctx;
    int i;

    *modreq_out = NULL;
    if (pctx == NULL || reqctx == NULL)
        return NULL;

    i = search_module_list(pctx->handles, pa_type);
    if (i == -1)
        return NULL;

    *modreq_out = reqctx->modreqs[i];
    return pctx->handles[i];
}

/* Initialize the preauth state for a krb5 context. */
void
k5_init_preauth_context(krb5_context context)
{
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    clpreauth_handle *list = NULL, h;
    int i;
    size_t count;
    krb5_preauthtype *tp;

    /* Only do this once for each krb5_context */
    if (context->preauth_context != NULL)
        return;

    /* Auto-register built-in modules. */
    k5_plugin_register_dyn(context, PLUGIN_INTERFACE_CLPREAUTH, "pkinit",
                           "preauth");
    k5_plugin_register_dyn(context, PLUGIN_INTERFACE_CLPREAUTH, "spake",
                           "preauth");
    k5_plugin_register(context, PLUGIN_INTERFACE_CLPREAUTH,
                       "encrypted_challenge",
                       clpreauth_encrypted_challenge_initvt);
    k5_plugin_register(context, PLUGIN_INTERFACE_CLPREAUTH,
                       "encrypted_timestamp",
                       clpreauth_encrypted_timestamp_initvt);
    k5_plugin_register(context, PLUGIN_INTERFACE_CLPREAUTH, "sam2",
                       clpreauth_sam2_initvt);
    k5_plugin_register(context, PLUGIN_INTERFACE_CLPREAUTH, "otp",
                       clpreauth_otp_initvt);

    /* Get all available clpreauth vtables. */
    if (k5_plugin_load_all(context, PLUGIN_INTERFACE_CLPREAUTH, &modules))
        return;

    /* Allocate a large enough list of handles. */
    for (count = 0; modules[count] != NULL; count++);
    list = calloc(count + 1, sizeof(*list));
    if (list == NULL)
        goto cleanup;

    /* Create a handle for each module we can successfully initialize. */
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        h = calloc(1, sizeof(*h));
        if (h == NULL)
            goto cleanup;

        /* Initialize the handle vtable. */
        if ((*mod)(context, 1, 1, (krb5_plugin_vtable)&h->vt) != 0) {
            free(h);
            continue;
        }

        /* Check for a preauth type conflict with an existing module. */
        for (tp = h->vt.pa_type_list; *tp != 0; tp++) {
            i = search_module_list(list, *tp);
            if (i != -1) {
                TRACE_PREAUTH_CONFLICT(context, h->vt.name, list[i]->vt.name,
                                       *tp);
                break;
            }
        }
        if (*tp != 0)
            continue;

        /* Initialize the module data. */
        h->data = NULL;
        if (h->vt.init != NULL && h->vt.init(context, &h->data) != 0) {
            free(h);
            continue;
        }
        list[count++] = h;
        list[count] = NULL;
    }
    list[count] = NULL;

    /* Place the constructed preauth context into the krb5 context. */
    context->preauth_context = malloc(sizeof(*context->preauth_context));
    if (context->preauth_context == NULL)
        goto cleanup;
    context->preauth_context->handles = list;
    list = NULL;

cleanup:
    k5_plugin_free_modules(context, modules);
    free_handles(context, list);
}

/* Add pa_type to the list of types which has previously failed. */
krb5_error_code
k5_preauth_note_failed(krb5_init_creds_context ctx, krb5_preauthtype pa_type)
{
    krb5_preauth_req_context reqctx = ctx->preauth_reqctx;
    krb5_preauthtype *newptr;
    size_t i;

    for (i = 0; reqctx->failed != NULL && reqctx->failed[i] != 0; i++);
    newptr = realloc(reqctx->failed, (i + 2) * sizeof(*newptr));
    if (newptr == NULL)
        return ENOMEM;
    reqctx->failed = newptr;
    reqctx->failed[i] = pa_type;
    reqctx->failed[i + 1] = 0;
    return 0;
}

/* Free the per-krb5_context preauth_context. This means clearing any
 * plugin-specific context which may have been created, and then
 * freeing the context itself. */
void
k5_free_preauth_context(krb5_context context)
{
    krb5_preauth_context pctx = context->preauth_context;

    if (pctx == NULL)
        return;
    free_handles(context, pctx->handles);
    free(pctx);
    context->preauth_context = NULL;
}

/* Initialize the per-AS-REQ context. This means calling the client_req_init
 * function to give the plugin a chance to allocate a per-request context. */
void
k5_preauth_request_context_init(krb5_context context,
                                krb5_init_creds_context ctx)
{
    krb5_preauth_context pctx = context->preauth_context;
    clpreauth_handle h;
    krb5_preauth_req_context reqctx;
    size_t count, i;

    if (pctx == NULL) {
        k5_init_preauth_context(context);
        pctx = context->preauth_context;
        if (pctx == NULL)
            return;
    }

    reqctx = calloc(1, sizeof(*reqctx));
    if (reqctx == NULL)
        return;
    reqctx->orig_context = context;

    /* Create an array of per-request module data objects corresponding to the
     * preauth context's array of handles. */
    for (count = 0; pctx->handles[count] != NULL; count++);
    reqctx->modreqs = calloc(count, sizeof(*reqctx->modreqs));
    if (reqctx->modreqs == NULL) {
        free(reqctx);
        return;
    }
    for (i = 0; i < count; i++) {
        h = pctx->handles[i];
        if (h->vt.request_init != NULL)
            h->vt.request_init(context, h->data, &reqctx->modreqs[i]);
    }
    ctx->preauth_reqctx = reqctx;
}

/* Free the per-AS-REQ context. This means clearing any request-specific
 * context which the plugin may have created. */
void
k5_preauth_request_context_fini(krb5_context context,
                                krb5_init_creds_context ctx)
{
    krb5_preauth_context pctx = context->preauth_context;
    krb5_preauth_req_context reqctx = ctx->preauth_reqctx;
    size_t i;
    clpreauth_handle h;

    if (reqctx == NULL)
        return;
    if (reqctx->orig_context == context && pctx != NULL) {
        for (i = 0; pctx->handles[i] != NULL; i++) {
            h = pctx->handles[i];
            if (reqctx->modreqs[i] != NULL && h->vt.request_fini != NULL)
                h->vt.request_fini(context, h->data, reqctx->modreqs[i]);
        }
    } else {
        TRACE_PREAUTH_WRONG_CONTEXT(context);
    }
    free(reqctx->modreqs);
    free(reqctx->failed);
    free(reqctx);
    ctx->preauth_reqctx = NULL;
}

krb5_error_code
k5_preauth_check_context(krb5_context context, krb5_init_creds_context ctx)
{
    krb5_preauth_req_context reqctx = ctx->preauth_reqctx;

    if (reqctx != NULL && reqctx->orig_context != context) {
        k5_setmsg(context, EINVAL,
                  _("krb5_init_creds calls must use same library context"));
        return EINVAL;
    }
    return 0;
}

/* Return 1 if pa_type is a real preauthentication mechanism according to the
 * module h.  Return 0 if it is not. */
static int
clpreauth_is_real(krb5_context context, clpreauth_handle h,
                  krb5_preauthtype pa_type)
{
    if (h->vt.flags == NULL)
        return 1;
    return (h->vt.flags(context, pa_type) & PA_REAL) != 0;
}

static krb5_error_code
clpreauth_prep_questions(krb5_context context, clpreauth_handle h,
                         krb5_clpreauth_modreq modreq,
                         krb5_get_init_creds_opt *opt,
                         krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                         krb5_kdc_req *req, krb5_data *req_body,
                         krb5_data *prev_req, krb5_pa_data *pa_data)
{
    if (h->vt.prep_questions == NULL)
        return 0;
    return h->vt.prep_questions(context, h->data, modreq, opt, cb, rock, req,
                                req_body, prev_req, pa_data);
}

static krb5_error_code
clpreauth_process(krb5_context context, clpreauth_handle h,
                  krb5_clpreauth_modreq modreq, krb5_get_init_creds_opt *opt,
                  krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                  krb5_kdc_req *req, krb5_data *req_body, krb5_data *prev_req,
                  krb5_pa_data *pa_data, krb5_prompter_fct prompter,
                  void *prompter_data, krb5_pa_data ***pa_data_out)
{
    return h->vt.process(context, h->data, modreq, opt, cb, rock, req,
                         req_body, prev_req, pa_data, prompter, prompter_data,
                         pa_data_out);
}

static krb5_error_code
clpreauth_tryagain(krb5_context context, clpreauth_handle h,
                   krb5_clpreauth_modreq modreq, krb5_get_init_creds_opt *opt,
                   krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                   krb5_kdc_req *req, krb5_data *req_body, krb5_data *prev_req,
                   krb5_preauthtype pa_type, krb5_error *error,
                   krb5_pa_data **error_padata, krb5_prompter_fct prompter,
                   void *prompter_data, krb5_pa_data ***pa_data_out)
{
    if (h->vt.tryagain == NULL)
        return 0;
    return h->vt.tryagain(context, h->data, modreq, opt, cb, rock, req,
                          req_body, prev_req, pa_type, error, error_padata,
                          prompter, prompter_data, pa_data_out);
}

static krb5_error_code
clpreauth_gic_opts(krb5_context context, clpreauth_handle h,
                   krb5_get_init_creds_opt *opt, const char *attr,
                   const char *value)
{
    if (h->vt.gic_opts == NULL)
        return 0;
    return h->vt.gic_opts(context, h->data, opt, attr, value);
}

/* Add the named encryption type to the existing list of ktypes. */
static void
grow_ktypes(krb5_enctype **out_ktypes, int *out_nktypes, krb5_enctype ktype)
{
    int i;
    krb5_enctype *ktypes;

    for (i = 0; i < *out_nktypes; i++) {
        if ((*out_ktypes)[i] == ktype)
            return;
    }
    ktypes = realloc(*out_ktypes, (*out_nktypes + 2) * sizeof(ktype));
    if (ktypes != NULL) {
        *out_ktypes = ktypes;
        ktypes[(*out_nktypes)++] = ktype;
        ktypes[*out_nktypes] = 0;
    }
}

/* Add a list of new pa_data items to an existing list. */
static int
grow_pa_list(krb5_pa_data ***out_pa_list, int *out_pa_list_size,
             krb5_pa_data **addition, int num_addition)
{
    krb5_pa_data **pa_list;
    int i;

    /* Allocate space for new entries and a null terminator. */
    pa_list = realloc(*out_pa_list, (*out_pa_list_size + num_addition + 1) *
                      sizeof(*pa_list));
    if (pa_list == NULL)
        return ENOMEM;
    *out_pa_list = pa_list;
    for (i = 0; i < num_addition; i++)
        pa_list[(*out_pa_list_size)++] = addition[i];
    pa_list[*out_pa_list_size] = NULL;
    return 0;
}

static krb5_enctype
get_etype(krb5_context context, krb5_clpreauth_rock rock)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;

    if (ctx->reply != NULL)
        return ctx->reply->enc_part.enctype;
    return ctx->etype;
}

static krb5_keyblock *
fast_armor(krb5_context context, krb5_clpreauth_rock rock)
{
    return ((krb5_init_creds_context)rock)->fast_state->armor_key;
}

static krb5_error_code
get_as_key(krb5_context context, krb5_clpreauth_rock rock,
           krb5_keyblock **keyblock)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;
    krb5_error_code ret;
    krb5_data *salt;

    if (ctx->as_key.length == 0) {
        salt = ctx->default_salt ? NULL : &ctx->salt;
        ret = ctx->gak_fct(context, ctx->request->client, ctx->etype,
                           ctx->prompter, ctx->prompter_data, salt,
                           &ctx->s2kparams, &ctx->as_key, ctx->gak_data,
                           ctx->rctx.items);
        if (ret)
            return ret;
    }
    *keyblock = &ctx->as_key;
    return 0;
}

static krb5_error_code
set_as_key(krb5_context context, krb5_clpreauth_rock rock,
           const krb5_keyblock *keyblock)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;

    krb5_free_keyblock_contents(context, &ctx->as_key);
    return krb5_copy_keyblock_contents(context, keyblock, &ctx->as_key);
}

static krb5_error_code
get_preauth_time(krb5_context context, krb5_clpreauth_rock rock,
                 krb5_boolean allow_unauth_time, krb5_timestamp *time_out,
                 krb5_int32 *usec_out)
{
    return k5_init_creds_current_time(context, (krb5_init_creds_context)rock,
                                      allow_unauth_time, time_out, usec_out);
}

static krb5_error_code
responder_ask_question(krb5_context context, krb5_clpreauth_rock rock,
                       const char *question, const char *challenge)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;

    /* Force plugins to use need_as_key(). */
    if (strcmp(KRB5_RESPONDER_QUESTION_PASSWORD, question) == 0)
        return EINVAL;
    return k5_response_items_ask_question(ctx->rctx.items, question,
                                          challenge);
}

static const char *
responder_get_answer(krb5_context context, krb5_clpreauth_rock rock,
                     const char *question)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;

    /* Don't let plugins get the raw password. */
    if (strcmp(KRB5_RESPONDER_QUESTION_PASSWORD, question) == 0)
        return NULL;
    return k5_response_items_get_answer(ctx->rctx.items, question);
}

static void
need_as_key(krb5_context context, krb5_clpreauth_rock rock)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;

    /* Calling gac_fct() with NULL as_key indicates desire for the AS key. */
    ctx->gak_fct(context, ctx->request->client, ctx->etype, NULL, NULL, NULL,
                 NULL, NULL, ctx->gak_data, ctx->rctx.items);
}

static const char *
get_cc_config(krb5_context context, krb5_clpreauth_rock rock, const char *key)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;
    k5_json_value value;

    if (ctx->cc_config_in == NULL)
        return NULL;

    value = k5_json_object_get(ctx->cc_config_in, key);
    if (value == NULL)
        return NULL;

    if (k5_json_get_tid(value) != K5_JSON_TID_STRING)
        return NULL;

    return k5_json_string_utf8(value);
}

static krb5_error_code
set_cc_config(krb5_context context, krb5_clpreauth_rock rock,
              const char *key, const char *data)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;
    krb5_error_code ret;
    k5_json_string str;

    if (ctx->cc_config_out == NULL)
        return ENOENT;

    ret = k5_json_string_create(data, &str);
    if (ret)
        return ret;

    ret = k5_json_object_set(ctx->cc_config_out, key, str);
    k5_json_release(str);
    return ret;
}

static void
disable_fallback(krb5_context context, krb5_clpreauth_rock rock)
{
    ((krb5_init_creds_context)rock)->fallback_disabled = TRUE;
}

static struct krb5_clpreauth_callbacks_st callbacks = {
    3,
    get_etype,
    fast_armor,
    get_as_key,
    set_as_key,
    get_preauth_time,
    responder_ask_question,
    responder_get_answer,
    need_as_key,
    get_cc_config,
    set_cc_config,
    disable_fallback
};

/* Tweak the request body, for now adding any enctypes which the module claims
 * to add support for to the list, but in the future perhaps doing more
 * involved things. */
void
k5_preauth_prepare_request(krb5_context context, krb5_get_init_creds_opt *opt,
                           krb5_kdc_req *req)
{
    krb5_preauth_context pctx = context->preauth_context;
    clpreauth_handle *hp, h;
    krb5_enctype *ep;

    if (pctx == NULL)
        return;
    /* Don't modify the enctype list if it's specified in the gic opts. */
    if (opt != NULL && (opt->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST))
        return;
    for (hp = pctx->handles; *hp != NULL; hp++) {
        h = *hp;
        if (h->vt.enctype_list == NULL)
            continue;
        for (ep = h->vt.enctype_list; *ep != ENCTYPE_NULL; ep++)
            grow_ktypes(&req->ktype, &req->nktypes, *ep);
    }
}

const char * const * KRB5_CALLCONV
krb5_responder_list_questions(krb5_context ctx, krb5_responder_context rctx)
{
    return k5_response_items_list_questions(rctx->items);
}

const char * KRB5_CALLCONV
krb5_responder_get_challenge(krb5_context ctx, krb5_responder_context rctx,
                             const char *question)
{
    if (rctx == NULL)
        return NULL;

    return k5_response_items_get_challenge(rctx->items, question);
}

krb5_error_code KRB5_CALLCONV
krb5_responder_set_answer(krb5_context ctx, krb5_responder_context rctx,
                          const char *question, const char *answer)
{
    if (rctx == NULL)
        return EINVAL;

    return k5_response_items_set_answer(rctx->items, question, answer);
}

/* Return true if pa_type matches the specific preauth type allowed for this
 * authentication, or if there is no specific allowed type. */
static inline krb5_boolean
pa_type_allowed(krb5_init_creds_context ctx, krb5_preauthtype pa_type)
{
    return ctx->allowed_preauth_type == KRB5_PADATA_NONE ||
        pa_type == ctx->allowed_preauth_type;
}

/* Return true if pa_type previously failed during this authentication. */
static krb5_boolean
previously_failed(krb5_init_creds_context ctx, krb5_preauthtype pa_type)
{
    krb5_preauth_req_context reqctx = ctx->preauth_reqctx;
    size_t i;

    for (i = 0; reqctx->failed != NULL && reqctx->failed[i] != 0; i++) {
        if (reqctx->failed[i] == pa_type)
            return TRUE;
    }
    return FALSE;
}

/* Allow clpreauth modules to process in_pa_list and produce output padata. */
static krb5_error_code
process_pa_data(krb5_context context, krb5_init_creds_context ctx,
                krb5_pa_data **in_pa_list, krb5_boolean must_preauth,
                krb5_pa_data ***out_pa_list, int *out_pa_list_size,
                krb5_preauthtype *out_type)
{
    struct errinfo save = EMPTY_ERRINFO;
    krb5_pa_data *pa, **pa_ptr, **mod_pa;
    krb5_error_code ret = 0;
    krb5_clpreauth_modreq modreq;
    clpreauth_handle h;
    int real, i;

    /* Process all informational padata types, then the first real preauth type
     * we succeed on. */
    for (real = 0; real <= 1; real++) {
        for (pa_ptr = in_pa_list; *pa_ptr != NULL; pa_ptr++) {
            pa = *pa_ptr;
            /* Restrict real mechanisms to the chosen one if we have one. */
            if (real && !pa_type_allowed(ctx, pa->pa_type))
                continue;
            h = find_module(context, ctx, pa->pa_type, &modreq);
            if (h == NULL)
                continue;
            /* Make sure this type is for the current pass. */
            if (clpreauth_is_real(context, h, pa->pa_type) != real)
                continue;
            /* Don't try a real mechanism again after failure. */
            if (real && previously_failed(ctx, pa->pa_type))
                continue;
            mod_pa = NULL;
            ret = clpreauth_process(context, h, modreq, ctx->opt, &callbacks,
                                    (krb5_clpreauth_rock)ctx, ctx->request,
                                    ctx->inner_request_body,
                                    ctx->encoded_previous_request, pa,
                                    ctx->prompter, ctx->prompter_data,
                                    &mod_pa);
            TRACE_PREAUTH_PROCESS(context, h->vt.name, pa->pa_type, real,
                                  ret);
            if (mod_pa != NULL) {
                for (i = 0; mod_pa[i] != NULL; i++);
                ret = grow_pa_list(out_pa_list, out_pa_list_size, mod_pa, i);
                if (ret) {
                    krb5_free_pa_data(context, mod_pa);
                    goto cleanup;
                }
                free(mod_pa);
            }
            /* Don't continue to try mechanisms after a keyboard interrupt. */
            if (ret == KRB5_LIBOS_PWDINTR)
                goto cleanup;
            if (ret == 0 && real) {
                /* Stop now and record which real padata type we answered. */
                *out_type = pa->pa_type;
                goto cleanup;
            } else if (real && save.code == 0) {
                /* Save the first error we get from a real preauth type. */
                k5_save_ctx_error(context, ret, &save);
            }
            if (real && ret) {
                /* Don't try this mechanism again for this authentication. */
                ret = k5_preauth_note_failed(ctx, pa->pa_type);
                if (ret)
                    goto cleanup;
            }
        }
    }

    if (must_preauth) {
        /* No real preauth types succeeded and we needed to preauthenticate. */
        if (save.code != 0) {
            ret = k5_restore_ctx_error(context, &save);
            k5_wrapmsg(context, ret, KRB5_PREAUTH_FAILED,
                       _("Pre-authentication failed"));
        }
        ret = KRB5_PREAUTH_FAILED;
    }

cleanup:
    k5_clear_error(&save);
    return ret;
}

static inline krb5_data
padata2data(krb5_pa_data p)
{
    krb5_data d;
    d.magic = KV5M_DATA;
    d.length = p.length;
    d.data = (char *) p.contents;
    return d;
}

/* Set salt in rock based on pw-salt or afs3-salt elements in padata. */
static krb5_error_code
get_salt(krb5_context context, krb5_init_creds_context ctx,
         krb5_pa_data **padata)
{
    krb5_error_code ret;
    krb5_pa_data *pa;
    krb5_data d;
    const char *p;

    /* Look for a pw-salt or afs3-salt element. */
    pa = krb5int_find_pa_data(context, padata, KRB5_PADATA_PW_SALT);
    if (pa == NULL)
        pa = krb5int_find_pa_data(context, padata, KRB5_PADATA_AFS3_SALT);
    if (pa == NULL)
        return 0;

    /* Set ctx->salt based on the element we found. */
    krb5_free_data_contents(context, &ctx->salt);
    d = padata2data(*pa);
    ret = krb5int_copy_data_contents(context, &d, &ctx->salt);
    if (ret)
        return ret;

    /* Adjust the salt if we got it from an afs3-salt element. */
    if (pa->pa_type == KRB5_PADATA_AFS3_SALT) {
        /* Work around a (possible) old Heimdal KDC foible. */
        p = memchr(ctx->salt.data, '@', ctx->salt.length);
        if (p != NULL)
            ctx->salt.length = p - ctx->salt.data;
        /* Tolerate extra null in MIT KDC afs3-salt value. */
        if (ctx->salt.length > 0 &&
            ctx->salt.data[ctx->salt.length - 1] == '\0')
            ctx->salt.length--;
        /* Set an s2kparams value to indicate AFS string-to-key. */
        krb5_free_data_contents(context, &ctx->s2kparams);
        ret = alloc_data(&ctx->s2kparams, 1);
        if (ret)
            return ret;
        ctx->s2kparams.data[0] = '\1';
    }

    ctx->default_salt = FALSE;
    TRACE_PREAUTH_SALT(context, &ctx->salt, pa->pa_type);
    return 0;
}

/* Set etype info parameters in rock based on padata. */
krb5_error_code
k5_get_etype_info(krb5_context context, krb5_init_creds_context ctx,
                  krb5_pa_data **padata)
{
    krb5_error_code ret = 0;
    krb5_pa_data *pa;
    krb5_data d;
    krb5_etype_info etype_info = NULL, e;
    krb5_etype_info_entry *entry;
    krb5_boolean valid_found;
    int i;

    /* Find an etype-info2 or etype-info element in padata. */
    pa = krb5int_find_pa_data(context, padata, KRB5_PADATA_ETYPE_INFO2);
    if (pa != NULL) {
        d = padata2data(*pa);
        (void)decode_krb5_etype_info2(&d, &etype_info);
    } else {
        pa = krb5int_find_pa_data(context, padata, KRB5_PADATA_ETYPE_INFO);
        if (pa != NULL) {
            d = padata2data(*pa);
            (void)decode_krb5_etype_info(&d, &etype_info);
        }
    }

    /* Fall back to pw-salt/afs3-salt if no etype-info element is present. */
    if (etype_info == NULL)
        return get_salt(context, ctx, padata);

    /* Search entries in order of the request's enctype preference. */
    entry = NULL;
    valid_found = FALSE;
    for (i = 0; i < ctx->request->nktypes && entry == NULL; i++) {
        for (e = etype_info; *e != NULL && entry == NULL; e++) {
            if ((*e)->etype == ctx->request->ktype[i])
                entry = *e;
            if (krb5_c_valid_enctype((*e)->etype))
                valid_found = TRUE;
        }
    }
    if (entry == NULL) {
        ret = (valid_found) ? KRB5_CONFIG_ETYPE_NOSUPP :
            KRB5_PROG_ETYPE_NOSUPP;
        goto cleanup;
    }

    /* Set etype/salt/s2kparams fields based on the entry we selected. */
    ctx->etype = entry->etype;
    krb5_free_data_contents(context, &ctx->salt);
    if (entry->length != KRB5_ETYPE_NO_SALT) {
        ctx->salt = make_data(entry->salt, entry->length);
        entry->salt = NULL;
        ctx->default_salt = FALSE;
    } else {
        ctx->salt = empty_data();
        ctx->default_salt = TRUE;
    }
    krb5_free_data_contents(context, &ctx->s2kparams);
    ctx->s2kparams = entry->s2kparams;
    entry->s2kparams = empty_data();
    TRACE_PREAUTH_ETYPE_INFO(context, ctx->etype, &ctx->salt, &ctx->s2kparams);

cleanup:
    krb5_free_etype_info(context, etype_info);
    return ret;
}

/* Look for an fx-cookie element in in_padata and add it to out_pa_list. */
static krb5_error_code
copy_cookie(krb5_context context, krb5_pa_data **in_padata,
            krb5_pa_data ***out_pa_list, int *out_pa_list_size)
{
    krb5_error_code ret;
    krb5_pa_data *cookie, *pa = NULL;

    cookie = krb5int_find_pa_data(context, in_padata, KRB5_PADATA_FX_COOKIE);
    if (cookie == NULL)
        return 0;
    TRACE_PREAUTH_COOKIE(context, cookie->length, cookie->contents);
    pa = k5alloc(sizeof(*pa), &ret);
    if (pa == NULL)
        return ret;
    *pa = *cookie;
    pa->contents = k5memdup(cookie->contents, cookie->length, &ret);
    if (pa->contents == NULL)
        goto error;
    ret = grow_pa_list(out_pa_list, out_pa_list_size, &pa, 1);
    if (ret)
        goto error;
    return 0;

error:
    free(pa->contents);
    free(pa);
    return ENOMEM;
}

/*
 * If the module for pa_type can adjust its AS_REQ data using the contents of
 * err and err_padata, return 0 with *padata_out set to a padata list for the
 * next request.  If it's the sort of correction which requires that we ask the
 * user another question, we let the calling application deal with it.
 */
krb5_error_code
k5_preauth_tryagain(krb5_context context, krb5_init_creds_context ctx,
                    krb5_preauthtype pa_type, krb5_error *err,
                    krb5_pa_data **err_padata, krb5_pa_data ***padata_out)
{
    krb5_error_code ret;
    krb5_pa_data **mod_pa;
    krb5_clpreauth_modreq modreq;
    clpreauth_handle h;
    int count;

    *padata_out = NULL;

    TRACE_PREAUTH_TRYAGAIN_INPUT(context, pa_type, err_padata);

    h = find_module(context, ctx, pa_type, &modreq);
    if (h == NULL)
        return KRB5KRB_ERR_GENERIC;
    mod_pa = NULL;
    ret = clpreauth_tryagain(context, h, modreq, ctx->opt, &callbacks,
                             (krb5_clpreauth_rock)ctx, ctx->request,
                             ctx->inner_request_body,
                             ctx->encoded_previous_request, pa_type, err,
                             err_padata, ctx->prompter, ctx->prompter_data,
                             &mod_pa);
    TRACE_PREAUTH_TRYAGAIN(context, h->vt.name, pa_type, ret);
    if (!ret && mod_pa == NULL)
        ret = KRB5KRB_ERR_GENERIC;
    if (ret) {
        k5_preauth_note_failed(ctx, pa_type);
        return ret;
    }

    for (count = 0; mod_pa[count] != NULL; count++);
    ret = copy_cookie(context, err_padata, &mod_pa, &count);
    if (ret) {
        krb5_free_pa_data(context, mod_pa);
        return ret;
    }

    TRACE_PREAUTH_TRYAGAIN_OUTPUT(context, mod_pa);
    *padata_out = mod_pa;
    return 0;
}

/* Compile the set of response items for in_padata by invoke each module's
 * prep_questions method. */
static krb5_error_code
fill_response_items(krb5_context context, krb5_init_creds_context ctx,
                    krb5_pa_data **in_padata)
{
    krb5_error_code ret;
    krb5_pa_data *pa;
    krb5_clpreauth_modreq modreq;
    clpreauth_handle h;
    int i;

    k5_response_items_reset(ctx->rctx.items);
    for (i = 0; in_padata[i] != NULL; i++) {
        pa = in_padata[i];
        if (!pa_type_allowed(ctx, pa->pa_type))
            continue;
        h = find_module(context, ctx, pa->pa_type, &modreq);
        if (h == NULL)
            continue;
        ret = clpreauth_prep_questions(context, h, modreq, ctx->opt,
                                       &callbacks, (krb5_clpreauth_rock)ctx,
                                       ctx->request, ctx->inner_request_body,
                                       ctx->encoded_previous_request, pa);
        if (ret)
            return ret;
    }
    return 0;
}

krb5_error_code
k5_preauth(krb5_context context, krb5_init_creds_context ctx,
           krb5_pa_data **in_padata, krb5_boolean must_preauth,
           krb5_pa_data ***padata_out, krb5_preauthtype *pa_type_out)
{
    int out_pa_list_size = 0;
    krb5_pa_data **out_pa_list = NULL;
    krb5_error_code ret;
    krb5_responder_fn responder;
    void *responder_data;

    *padata_out = NULL;
    *pa_type_out = KRB5_PADATA_NONE;

    /* We should never invoke preauth modules when identifying the realm. */
    if (in_padata == NULL || ctx->identify_realm)
        return 0;

    TRACE_PREAUTH_INPUT(context, in_padata);

    /* Scan the padata list and process etype-info or salt elements. */
    ret = k5_get_etype_info(context, ctx, in_padata);
    if (ret)
        return ret;

    /* Copy the cookie if there is one. */
    ret = copy_cookie(context, in_padata, &out_pa_list, &out_pa_list_size);
    if (ret)
        goto error;

    /* If we can't initialize the preauth context, stop with what we have. */
    k5_init_preauth_context(context);
    if (context->preauth_context == NULL) {
        *padata_out = out_pa_list;
        out_pa_list = NULL;
        goto error;
    }

    /* Get a list of response items for in_padata from the preauth modules. */
    ret = fill_response_items(context, ctx, in_padata);
    if (ret)
        goto error;

    /* Call the responder to answer response items. */
    k5_gic_opt_get_responder(ctx->opt, &responder, &responder_data);
    if (responder != NULL && !k5_response_items_empty(ctx->rctx.items)) {
        ret = (*responder)(context, responder_data, &ctx->rctx);
        if (ret)
            goto error;
    }

    ret = process_pa_data(context, ctx, in_padata, must_preauth,
                          &out_pa_list, &out_pa_list_size, pa_type_out);
    if (ret)
        goto error;

    TRACE_PREAUTH_OUTPUT(context, out_pa_list);
    *padata_out = out_pa_list;
    return 0;

error:
    krb5_free_pa_data(context, out_pa_list);
    return ret;
}

/*
 * Give all the preauth plugins a look at the preauth option which
 * has just been set
 */
krb5_error_code
krb5_preauth_supply_preauth_data(krb5_context context,
                                 krb5_get_init_creds_opt *opt,
                                 const char *attr, const char *value)
{
    krb5_preauth_context pctx = context->preauth_context;
    clpreauth_handle *hp, h;
    krb5_error_code ret;

    if (pctx == NULL) {
        k5_init_preauth_context(context);
        pctx = context->preauth_context;
        if (pctx == NULL) {
            k5_setmsg(context, EINVAL,
                      _("Unable to initialize preauth context"));
            return EINVAL;
        }
    }

    /*
     * Go down the list of preauth modules, and supply them with the
     * attribute/value pair.
     */
    for (hp = pctx->handles; *hp != NULL; hp++) {
        h = *hp;
        ret = clpreauth_gic_opts(context, h, opt, attr, value);
        if (ret) {
            k5_prependmsg(context, ret, _("Preauth module %s"), h->vt.name);
            return ret;
        }
    }
    return 0;
}
