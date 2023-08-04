/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/spake/spake_client.c - SPAKE clpreauth module */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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
#include "k5-spake.h"
#include "trace.h"
#include "util.h"
#include "iana.h"
#include "groups.h"
#include <krb5/clpreauth_plugin.h>

typedef struct reqstate_st {
    krb5_pa_spake *msg;         /* set in prep_questions, used in process */
    krb5_keyblock *initial_key;
    krb5_data *support;
    krb5_data thash;
    krb5_data spakeresult;
} reqstate;

/* Return true if SF-NONE is present in factors. */
static krb5_boolean
contains_sf_none(krb5_spake_factor **factors)
{
    int i;

    for (i = 0; factors != NULL && factors[i] != NULL; i++) {
        if (factors[i]->type == SPAKE_SF_NONE)
            return TRUE;
    }
    return FALSE;
}

static krb5_error_code
spake_init(krb5_context context, krb5_clpreauth_moddata *moddata_out)
{
    krb5_error_code ret;
    groupstate *gstate;

    ret = group_init_state(context, FALSE, &gstate);
    if (ret)
        return ret;
    *moddata_out = (krb5_clpreauth_moddata)gstate;
    return 0;
}

static void
spake_fini(krb5_context context, krb5_clpreauth_moddata moddata)
{
    group_free_state((groupstate *)moddata);
}

static void
spake_request_init(krb5_context context, krb5_clpreauth_moddata moddata,
                   krb5_clpreauth_modreq *modreq_out)
{
    *modreq_out = calloc(1, sizeof(reqstate));
}

static void
spake_request_fini(krb5_context context, krb5_clpreauth_moddata moddata,
                   krb5_clpreauth_modreq modreq)
{
    reqstate *st = (reqstate *)modreq;

    k5_free_pa_spake(context, st->msg);
    krb5_free_keyblock(context, st->initial_key);
    krb5_free_data(context, st->support);
    krb5_free_data_contents(context, &st->thash);
    zapfree(st->spakeresult.data, st->spakeresult.length);
    free(st);
}

static krb5_error_code
spake_prep_questions(krb5_context context, krb5_clpreauth_moddata moddata,
                     krb5_clpreauth_modreq modreq,
                     krb5_get_init_creds_opt *opt, krb5_clpreauth_callbacks cb,
                     krb5_clpreauth_rock rock, krb5_kdc_req *req,
                     krb5_data *enc_req, krb5_data *enc_prev_req,
                     krb5_pa_data *pa_data)
{
    krb5_error_code ret;
    groupstate *gstate = (groupstate *)moddata;
    reqstate *st = (reqstate *)modreq;
    krb5_data in_data;
    krb5_spake_challenge *ch;

    if (st == NULL)
        return ENOMEM;

    /* We don't need to ask any questions to send a support message. */
    if (pa_data->length == 0)
        return 0;

    /* Decode the incoming message, replacing any previous one in the request
     * state.  If we can't decode it, we have no questions to ask. */
    k5_free_pa_spake(context, st->msg);
    st->msg = NULL;
    in_data = make_data(pa_data->contents, pa_data->length);
    ret = decode_krb5_pa_spake(&in_data, &st->msg);
    if (ret)
        return (ret == ENOMEM) ? ENOMEM : 0;

    if (st->msg->choice == SPAKE_MSGTYPE_CHALLENGE) {
        ch = &st->msg->u.challenge;
        if (!group_is_permitted(gstate, ch->group))
            return 0;
        /* When second factor support is implemented, we should ask questions
         * based on the factors in the challenge. */
        if (!contains_sf_none(ch->factors))
            return 0;
        /* We will need the AS key to respond to the challenge. */
        cb->need_as_key(context, rock);
    } else if (st->msg->choice == SPAKE_MSGTYPE_ENCDATA) {
        /* When second factor support is implemented, we should decrypt the
         * encdata message and ask questions based on the factor data. */
    }
    return 0;
}

/*
 * Output a PA-SPAKE support message indicating which groups we support.  This
 * may be done for optimistic preauth, in response to an empty message, or in
 * response to a challenge using a group we do not support.  Save the support
 * message in st->support.
 */
static krb5_error_code
send_support(krb5_context context, groupstate *gstate, reqstate *st,
             krb5_pa_data ***pa_out)
{
    krb5_error_code ret;
    krb5_data *support;
    krb5_pa_spake msg;

    msg.choice = SPAKE_MSGTYPE_SUPPORT;
    group_get_permitted(gstate, &msg.u.support.groups, &msg.u.support.ngroups);
    ret = encode_krb5_pa_spake(&msg, &support);
    if (ret)
        return ret;

    /* Save the support message for later use in the transcript hash. */
    ret = krb5_copy_data(context, support, &st->support);
    if (ret) {
        krb5_free_data(context, support);
        return ret;
    }

    TRACE_SPAKE_SEND_SUPPORT(context);
    return convert_to_padata(support, pa_out);
}

static krb5_error_code
process_challenge(krb5_context context, groupstate *gstate, reqstate *st,
                  krb5_spake_challenge *ch, const krb5_data *der_msg,
                  krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                  krb5_prompter_fct prompter, void *prompter_data,
                  const krb5_data *der_req, krb5_pa_data ***pa_out)
{
    krb5_error_code ret;
    krb5_keyblock *k0 = NULL, *k1 = NULL, *as_key;
    krb5_spake_factor factor;
    krb5_pa_spake msg;
    krb5_data *der_factor = NULL, *response;
    krb5_data clpriv = empty_data(), clpub = empty_data();
    krb5_data wbytes = empty_data();
    krb5_enc_data enc_factor;

    enc_factor.ciphertext = empty_data();

    /* Not expected if we processed a challenge and didn't reject it. */
    if (st->initial_key != NULL)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    if (!group_is_permitted(gstate, ch->group)) {
        TRACE_SPAKE_REJECT_CHALLENGE(context, ch->group);
        /* No point in sending a second support message. */
        if (st->support != NULL)
            return KRB5KDC_ERR_PREAUTH_FAILED;
        return send_support(context, gstate, st, pa_out);
    }

    /* Initialize and update the transcript with the concatenation of the
     * support message (if we sent one) and the received challenge. */
    ret = update_thash(context, gstate, ch->group, &st->thash, st->support,
                       der_msg);
    if (ret)
        return ret;

    TRACE_SPAKE_RECEIVE_CHALLENGE(context, ch->group, &ch->pubkey);

    /* When second factor support is implemented, we should check for a
     * supported factor type instead of just checking for SF-NONE. */
    if (!contains_sf_none(ch->factors))
        return KRB5KDC_ERR_PREAUTH_FAILED;

    ret = cb->get_as_key(context, rock, &as_key);
    if (ret)
        goto cleanup;
    ret = krb5_copy_keyblock(context, as_key, &st->initial_key);
    if (ret)
        goto cleanup;
    ret = derive_wbytes(context, ch->group, st->initial_key, &wbytes);
    if (ret)
        goto cleanup;
    ret = group_keygen(context, gstate, ch->group, &wbytes, &clpriv, &clpub);
    if (ret)
        goto cleanup;
    ret = group_result(context, gstate, ch->group, &wbytes, &clpriv,
                       &ch->pubkey, &st->spakeresult);
    if (ret)
        goto cleanup;

    ret = update_thash(context, gstate, ch->group, &st->thash, &clpub, NULL);
    if (ret)
        goto cleanup;
    TRACE_SPAKE_CLIENT_THASH(context, &st->thash);

    /* Replace the reply key with K'[0]. */
    ret = derive_key(context, gstate, ch->group, st->initial_key, &wbytes,
                     &st->spakeresult, &st->thash, der_req, 0, &k0);
    if (ret)
        goto cleanup;
    ret = cb->set_as_key(context, rock, k0);
    if (ret)
        goto cleanup;

    /* Encrypt a SPAKESecondFactor message with K'[1]. */
    ret = derive_key(context, gstate, ch->group, st->initial_key, &wbytes,
                     &st->spakeresult, &st->thash, der_req, 1, &k1);
    if (ret)
        goto cleanup;
    /* When second factor support is implemented, we should construct an
     * appropriate factor here instead of hardcoding SF-NONE. */
    factor.type = SPAKE_SF_NONE;
    factor.data = NULL;
    ret = encode_krb5_spake_factor(&factor, &der_factor);
    if (ret)
        goto cleanup;
    ret = krb5_encrypt_helper(context, k1, KRB5_KEYUSAGE_SPAKE, der_factor,
                              &enc_factor);
    if (ret)
        goto cleanup;

    /* Encode and output a response message. */
    msg.choice = SPAKE_MSGTYPE_RESPONSE;
    msg.u.response.pubkey = clpub;
    msg.u.response.factor = enc_factor;
    ret = encode_krb5_pa_spake(&msg, &response);
    if (ret)
        goto cleanup;
    TRACE_SPAKE_SEND_RESPONSE(context);
    ret = convert_to_padata(response, pa_out);
    if (ret)
        goto cleanup;

    cb->disable_fallback(context, rock);

cleanup:
    krb5_free_keyblock(context, k0);
    krb5_free_keyblock(context, k1);
    krb5_free_data_contents(context, &enc_factor.ciphertext);
    krb5_free_data_contents(context, &clpub);
    zapfree(clpriv.data, clpriv.length);
    zapfree(wbytes.data, wbytes.length);
    if (der_factor != NULL) {
        zapfree(der_factor->data, der_factor->length);
        free(der_factor);
    }
    return ret;
}

static krb5_error_code
process_encdata(krb5_context context, reqstate *st, krb5_enc_data *enc,
                krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                krb5_prompter_fct prompter, void *prompter_data,
                const krb5_data *der_prev_req, const krb5_data *der_req,
                krb5_pa_data ***pa_out)
{
    /* Not expected if we haven't sent a response yet. */
    if (st->initial_key == NULL || st->spakeresult.length == 0)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    /*
     * When second factor support is implemented, we should process encdata
     * messages according to the factor type.  We should make sure to re-derive
     * K'[0] and replace the reply key again, in case the request has changed.
     * We should use der_prev_req to derive K'[n] to decrypt factor from the
     * KDC.  We should use der_req to derive K'[n+1] for the next message to
     * send to the KDC.
     */
    return KRB5_PLUGIN_OP_NOTSUPP;
}

static krb5_error_code
spake_process(krb5_context context, krb5_clpreauth_moddata moddata,
              krb5_clpreauth_modreq modreq, krb5_get_init_creds_opt *opt,
              krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
              krb5_kdc_req *req, krb5_data *der_req, krb5_data *der_prev_req,
              krb5_pa_data *pa_in, krb5_prompter_fct prompter,
              void *prompter_data, krb5_pa_data ***pa_out)
{
    krb5_error_code ret;
    groupstate *gstate = (groupstate *)moddata;
    reqstate *st = (reqstate *)modreq;
    krb5_data in_data;

    if (st == NULL)
        return ENOMEM;

    if (pa_in->length == 0) {
        /* Not expected if we already sent a support message. */
        if (st->support != NULL)
            return KRB5KDC_ERR_PREAUTH_FAILED;
        return send_support(context, gstate, st, pa_out);
    }

    if (st->msg == NULL) {
        /* The message failed to decode in spake_prep_questions(). */
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
    } else if (st->msg->choice == SPAKE_MSGTYPE_CHALLENGE) {
        in_data = make_data(pa_in->contents, pa_in->length);
        ret = process_challenge(context, gstate, st, &st->msg->u.challenge,
                                &in_data, cb, rock, prompter, prompter_data,
                                der_req, pa_out);
    } else if (st->msg->choice == SPAKE_MSGTYPE_ENCDATA) {
        ret = process_encdata(context, st, &st->msg->u.encdata, cb, rock,
                              prompter, prompter_data, der_prev_req, der_req,
                              pa_out);
    } else {
        /* Unexpected message type */
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
    }

    return ret;
}

krb5_error_code
clpreauth_spake_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable);

krb5_error_code
clpreauth_spake_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    krb5_clpreauth_vtable vt;
    static krb5_preauthtype pa_types[] = { KRB5_PADATA_SPAKE, 0 };

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_clpreauth_vtable)vtable;
    vt->name = "spake";
    vt->pa_type_list = pa_types;
    vt->init = spake_init;
    vt->fini = spake_fini;
    vt->request_init = spake_request_init;
    vt->request_fini = spake_request_fini;
    vt->process = spake_process;
    vt->prep_questions = spake_prep_questions;
    return 0;
}
