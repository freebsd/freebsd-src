/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/spake/spake_kdc.c - SPAKE kdcpreauth module */
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
#include "k5-input.h"
#include "k5-spake.h"

#include "groups.h"
#include "trace.h"
#include "iana.h"
#include "util.h"

#include <krb5/kdcpreauth_plugin.h>

/*
 * The SPAKE kdcpreauth module uses a secure cookie containing the following
 * concatenated fields (all integer fields are big-endian):
 *
 *     version (16-bit unsigned integer)
 *     stage (16-bit unsigned integer)
 *     group (32-bit signed integer)
 *     SPAKE value (32-bit unsigned length, followed by data)
 *     Transcript hash (32-bit unsigned length, followed by data)
 *     Zero or more instances of:
 *         second-factor number (32-bit signed integer)
 *         second-factor data (32-bit unsigned length, followed by data)
 *
 * The only currently supported version is 1.  stage is 0 if the cookie was
 * sent with a challenge message.  stage is n>0 if the cookie was sent with an
 * encdata message encrypted in K'[2n].  group indicates the group number used
 * in the SPAKE challenge.  The SPAKE value is the KDC private key for a
 * stage-0 cookie, represented in the scalar marshalling form of the group; for
 * other cookies, the SPAKE value is the SPAKE result K, represented in the
 * group element marshalling form.  The transcript hash is the intermediate
 * hash after updating with the support and challenge messages for a stage-0
 * cookie, or the final hash for other cookies.  For a stage 0 cookie, there
 * may be any number of second-factor records, including none (no record is
 * generated for SF-NONE); for other cookies, there must be exactly one
 * second-factor record corresponding to the factor type chosen by the client.
 */

/* From a k5input structure representing the remainder of a secure cookie
 * plaintext, parse a four-byte length and data. */
static void
parse_data(struct k5input *in, krb5_data *out)
{
    out->length = k5_input_get_uint32_be(in);
    out->data = (char *)k5_input_get_bytes(in, out->length);
    out->magic = KV5M_DATA;
}

/* Parse a received cookie into its components.  The pointers stored in the
 * krb5_data outputs are aliases into cookie and should not be freed. */
static krb5_error_code
parse_cookie(const krb5_data *cookie, int *stage_out, int32_t *group_out,
             krb5_data *spake_out, krb5_data *thash_out,
             krb5_data *factors_out)
{
    struct k5input in;
    int version, stage;
    int32_t group;
    krb5_data thash, spake, factors;

    *spake_out = *thash_out = *factors_out = empty_data();
    k5_input_init(&in, cookie->data, cookie->length);

    /* Parse and check the version, and read the other integer fields. */
    version = k5_input_get_uint16_be(&in);
    if (version != 1)
        return KRB5KDC_ERR_PREAUTH_FAILED;
    stage = k5_input_get_uint16_be(&in);
    group = k5_input_get_uint32_be(&in);

    /* Parse the data fields.  The factor data is anything remaining after the
     * transcript hash. */
    parse_data(&in, &spake);
    parse_data(&in, &thash);
    if (in.status)
        return in.status;
    factors = make_data((char *)in.ptr, in.len);

    *stage_out = stage;
    *group_out = group;
    *spake_out = spake;
    *thash_out = thash;
    *factors_out = factors;
    return 0;
}

/* Marshal data into buf as a four-byte length followed by the contents. */
static void
marshal_data(struct k5buf *buf, const krb5_data *data)
{
    k5_buf_add_uint32_be(buf, data->length);
    k5_buf_add_len(buf, data->data, data->length);
}

/* Marshal components into a cookie. */
static krb5_error_code
make_cookie(int stage, int32_t group, const krb5_data *spake,
            const krb5_data *thash, krb5_data *cookie_out)
{
    struct k5buf buf;

    *cookie_out = empty_data();
    k5_buf_init_dynamic_zap(&buf);

    /* Marshal the version, stage, and group. */
    k5_buf_add_uint16_be(&buf, 1);
    k5_buf_add_uint16_be(&buf, stage);
    k5_buf_add_uint32_be(&buf, group);

    /* Marshal the data fields. */
    marshal_data(&buf, spake);
    marshal_data(&buf, thash);

    /* When second factor support is implemented, we should add factor data
     * here. */

    if (buf.data == NULL)
        return ENOMEM;
    *cookie_out = make_data(buf.data, buf.len);
    return 0;
}

/* Add authentication indicators if any are configured for SPAKE. */
static krb5_error_code
add_indicators(krb5_context context, const krb5_data *realm,
               krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock)
{
    krb5_error_code ret;
    const char *keys[4];
    char *realmstr, **indicators, **ind;

    realmstr = k5memdup0(realm->data, realm->length, &ret);
    if (realmstr == NULL)
        return ret;
    keys[0] = KRB5_CONF_REALMS;
    keys[1] = realmstr;
    keys[2] = KRB5_CONF_SPAKE_PREAUTH_INDICATOR;
    keys[3] = NULL;
    ret = profile_get_values(context->profile, keys, &indicators);
    free(realmstr);
    if (ret == PROF_NO_RELATION)
        return 0;
    if (ret)
        return ret;

    for (ind = indicators; *ind != NULL && !ret; ind++)
        ret = cb->add_auth_indicator(context, rock, *ind);

    profile_free_list(indicators);
    return ret;
}

/* Initialize a SPAKE module data object. */
static krb5_error_code
spake_init(krb5_context context, krb5_kdcpreauth_moddata *moddata_out,
           const char **realmnames)
{
    krb5_error_code ret;
    groupstate *gstate;

    ret = group_init_state(context, TRUE, &gstate);
    if (ret)
        return ret;
    *moddata_out = (krb5_kdcpreauth_moddata)gstate;
    return 0;
}

/* Release a SPAKE module data object. */
static void
spake_fini(krb5_context context, krb5_kdcpreauth_moddata moddata)
{
    group_free_state((groupstate *)moddata);
}

/*
 * Generate a SPAKE challenge message for the specified group.  Use cb and rock
 * to retrieve the initial reply key and to set a stage-0 cookie.  Invoke
 * either erespond or vrespond with the result.
 */
static void
send_challenge(krb5_context context, groupstate *gstate, int32_t group,
               krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
               const krb5_data *support,
               krb5_kdcpreauth_edata_respond_fn erespond,
               krb5_kdcpreauth_verify_respond_fn vrespond, void *arg)
{
    krb5_error_code ret;
    const krb5_keyblock *ikey;
    krb5_pa_data **padata = NULL, *pa;
    krb5_data kdcpriv = empty_data(), kdcpub = empty_data(), *der_msg = NULL;
    krb5_data thash = empty_data(), cookie = empty_data();
    krb5_data wbytes = empty_data();
    krb5_spake_factor f, *flist[2];
    krb5_pa_spake msg;

    ikey = cb->client_keyblock(context, rock);
    if (ikey == NULL) {
        ret = KRB5KDC_ERR_ETYPE_NOSUPP;
        goto cleanup;
    }

    ret = derive_wbytes(context, group, ikey, &wbytes);
    if (ret)
        goto cleanup;
    ret = group_keygen(context, gstate, group, &wbytes, &kdcpriv, &kdcpub);
    if (ret)
        goto cleanup;

    /* Encode the challenge.  When second factor support is implemented, we
     * should construct a factor list instead of hardcoding SF-NONE. */
    f.type = SPAKE_SF_NONE;
    f.data = NULL;
    flist[0] = &f;
    flist[1] = NULL;
    msg.choice = SPAKE_MSGTYPE_CHALLENGE;
    msg.u.challenge.group = group;
    msg.u.challenge.pubkey = kdcpub;
    msg.u.challenge.factors = flist;
    ret = encode_krb5_pa_spake(&msg, &der_msg);
    if (ret)
        goto cleanup;

    /* Initialize and update the transcript hash with the support message (if
     * we received one) and challenge message. */
    ret = update_thash(context, gstate, group, &thash, support, der_msg);
    if (ret)
        goto cleanup;

    /* Save the group, transcript hash, and private key in a stage-0 cookie.
     * When second factor support is implemented, also save factor state. */
    ret = make_cookie(0, group, &kdcpriv, &thash, &cookie);
    if (ret)
        goto cleanup;
    ret = cb->set_cookie(context, rock, KRB5_PADATA_SPAKE, &cookie);
    if (ret)
        goto cleanup;

    ret = convert_to_padata(der_msg, &padata);
    der_msg = NULL;
    TRACE_SPAKE_SEND_CHALLENGE(context, group);

cleanup:
    zapfree(wbytes.data, wbytes.length);
    zapfree(kdcpriv.data, kdcpriv.length);
    zapfree(cookie.data, cookie.length);
    krb5_free_data_contents(context, &kdcpub);
    krb5_free_data_contents(context, &thash);
    krb5_free_data(context, der_msg);

    if (erespond != NULL) {
        assert(vrespond == NULL);
        /* Grab the first pa-data element from the list, if we made one. */
        pa = (padata == NULL) ? NULL : padata[0];
        free(padata);
        (*erespond)(arg, ret, pa);
    } else {
        assert(vrespond != NULL);
        if (!ret)
            ret = KRB5KDC_ERR_MORE_PREAUTH_DATA_REQUIRED;
        (*vrespond)(arg, ret, NULL, padata, NULL);
    }
}

/* Generate the METHOD-DATA entry indicating support for SPAKE.  Include an
 * optimistic challenge if configured to do so. */
static void
spake_edata(krb5_context context, krb5_kdc_req *req,
            krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
            krb5_kdcpreauth_moddata moddata, krb5_preauthtype pa_type,
            krb5_kdcpreauth_edata_respond_fn respond, void *arg)
{
    const krb5_keyblock *ikey;
    groupstate *gstate = (groupstate *)moddata;
    krb5_data empty = empty_data();
    int32_t group;

    /* SPAKE requires a client key, which cannot be a single-DES key. */
    ikey = cb->client_keyblock(context, rock);
    if (ikey == NULL) {
        (*respond)(arg, KRB5KDC_ERR_ETYPE_NOSUPP, NULL);
        return;
    }

    group = group_optimistic_challenge(gstate);
    if (group) {
        send_challenge(context, gstate, group, cb, rock, &empty, respond, NULL,
                       arg);
    } else {
        /* No optimistic challenge configured; send an empty pa-data value. */
        (*respond)(arg, 0, NULL);
    }
}

/* Choose a group from the client's support message and generate a
 * challenge. */
static void
verify_support(krb5_context context, groupstate *gstate,
               krb5_spake_support *support, const krb5_data *der_msg,
               krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
               krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    krb5_error_code ret;
    int32_t i, group;

    for (i = 0; i < support->ngroups; i++) {
        if (group_is_permitted(gstate, support->groups[i]))
            break;
    }
    if (i == support->ngroups) {
        TRACE_SPAKE_REJECT_SUPPORT(context);
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto error;
    }
    group = support->groups[i];
    TRACE_SPAKE_RECEIVE_SUPPORT(context, group);

    send_challenge(context, gstate, group, cb, rock, der_msg, NULL, respond,
                   arg);
    return;

error:
    (*respond)(arg, ret, NULL, NULL, NULL);
}

/*
 * From the client's response message, compute the SPAKE result and decrypt the
 * factor reply.  On success, either mark the reply as pre-authenticated and
 * set a reply key in the pre-request module data, or generate an additional
 * factor challenge and ask for another round of pre-authentication.
 */
static void
verify_response(krb5_context context, groupstate *gstate,
                krb5_spake_response *resp, const krb5_data *realm,
                krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
                krb5_enc_tkt_part *enc_tkt_reply,
                krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    krb5_error_code ret;
    const krb5_keyblock *ikey;
    krb5_keyblock *k1 = NULL, *reply_key = NULL;
    krb5_data cookie, thash_in, kdcpriv, factors, *der_req;
    krb5_data thash = empty_data(), der_factor = empty_data();
    krb5_data wbytes = empty_data(), spakeresult = empty_data();
    krb5_spake_factor *factor = NULL;
    int stage;
    int32_t group;

    ikey = cb->client_keyblock(context, rock);
    if (ikey == NULL) {
        ret = KRB5KDC_ERR_ETYPE_NOSUPP;
        goto cleanup;
    }

    /* Fetch the stage-0 cookie and parse it.  (All of the krb5_data results
     * are aliases into memory owned by rock). */
    if (!cb->get_cookie(context, rock, KRB5_PADATA_SPAKE, &cookie)) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }
    ret = parse_cookie(&cookie, &stage, &group, &kdcpriv, &thash_in, &factors);
    if (ret)
        goto cleanup;
    if (stage != 0) {
        /* The received cookie wasn't sent with a challenge. */
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }
    TRACE_SPAKE_RECEIVE_RESPONSE(context, &resp->pubkey);

    /* Update the transcript hash with the client public key. */
    ret = krb5int_copy_data_contents(context, &thash_in, &thash);
    if (ret)
        goto cleanup;
    ret = update_thash(context, gstate, group, &thash, &resp->pubkey, NULL);
    if (ret)
        goto cleanup;
    TRACE_SPAKE_KDC_THASH(context, &thash);

    ret = derive_wbytes(context, group, ikey, &wbytes);
    if (ret)
        goto cleanup;
    ret = group_result(context, gstate, group, &wbytes, &kdcpriv,
                       &resp->pubkey, &spakeresult);
    if (ret)
        goto cleanup;

    /* Decrypt the response factor field using K'[1].  If the decryption
     * integrity check fails, the client probably used the wrong password. */
    der_req = cb->request_body(context, rock);
    ret = derive_key(context, gstate, group, ikey, &wbytes, &spakeresult,
                     &thash, der_req, 1, &k1);
    if (ret)
        goto cleanup;
    ret = alloc_data(&der_factor, resp->factor.ciphertext.length);
    if (ret)
        goto cleanup;
    ret = krb5_c_decrypt(context, k1, KRB5_KEYUSAGE_SPAKE, NULL, &resp->factor,
                         &der_factor);
    if (ret == KRB5KRB_AP_ERR_BAD_INTEGRITY)
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
    if (ret)
        goto cleanup;
    ret = decode_krb5_spake_factor(&der_factor, &factor);
    if (ret)
        goto cleanup;

    /*
     * When second factor support is implemented, we should verify the factor
     * data here, and possibly generate an encdata message for another hop.
     * This function may need to be split at this point to allow for
     * asynchronous verification of the second-factor value.  We might also
     * need to collect authentication indicators from the second-factor module;
     * alternatively the module could have access to cb and rock so that it can
     * add indicators itself.
     */
    if (factor->type != SPAKE_SF_NONE) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    ret = add_indicators(context, realm, cb, rock);
    if (ret)
        goto cleanup;

    enc_tkt_reply->flags |= TKT_FLG_PRE_AUTH;

    ret = derive_key(context, gstate, group, ikey, &wbytes, &spakeresult,
                     &thash, der_req, 0, &reply_key);
    if (ret)
        goto cleanup;

    ret = cb->replace_reply_key(context, rock, reply_key, TRUE);

cleanup:
    zapfree(wbytes.data, wbytes.length);
    zapfree(der_factor.data, der_factor.length);
    zapfree(spakeresult.data, spakeresult.length);
    krb5_free_data_contents(context, &thash);
    krb5_free_keyblock(context, k1);
    krb5_free_keyblock(context, reply_key);
    k5_free_spake_factor(context, factor);
    (*respond)(arg, ret, NULL, NULL, NULL);
}

/*
 * Decrypt and validate an additional second-factor reply.  On success, either
 * mark the reply as pre-authenticated and set a reply key in the pre-request
 * module data, or generate an additional factor challenge and ask for another
 * round of pre-authentication.
 */
static void
verify_encdata(krb5_context context, krb5_enc_data *enc,
               krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
               krb5_enc_tkt_part *enc_tkt_reply,
               krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    /*
     * When second factor support is implemented, we should process encdata
     * message according to the factor type recorded in the cookie.  If the
     * second factor exchange finishes successfully, we should set
     * TKT_FLG_PRE_AUTH, set the reply key to K'[0], and add any auth
     * indicators from configuration (with a call to add_indicators()) or the
     * second factor module (unless the module has access to cb and rock and
     * can add indicators itself).
     */
    (*respond)(arg, KRB5KDC_ERR_PREAUTH_FAILED, NULL, NULL, NULL);
}

/*
 * Respond to a client padata message, either by generating a SPAKE challenge,
 * generating an additional second-factor challenge, or marking the reply as
 * pre-authenticated and setting an additional reply key in the pre-request
 * module data.
 */
static void
spake_verify(krb5_context context, krb5_data *req_pkt, krb5_kdc_req *request,
             krb5_enc_tkt_part *enc_tkt_reply, krb5_pa_data *data,
             krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
             krb5_kdcpreauth_moddata moddata,
             krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    krb5_error_code ret;
    krb5_pa_spake *pa_spake = NULL;
    krb5_data in_data = make_data(data->contents, data->length);
    groupstate *gstate = (groupstate *)moddata;

    ret = decode_krb5_pa_spake(&in_data, &pa_spake);
    if (ret) {
        (*respond)(arg, ret, NULL, NULL, NULL);
    } else if (pa_spake->choice == SPAKE_MSGTYPE_SUPPORT) {
        verify_support(context, gstate, &pa_spake->u.support, &in_data, cb,
                       rock, respond, arg);
    } else if (pa_spake->choice == SPAKE_MSGTYPE_RESPONSE) {
        verify_response(context, gstate, &pa_spake->u.response,
                        &request->server->realm, cb, rock, enc_tkt_reply,
                        respond, arg);
    } else if (pa_spake->choice == SPAKE_MSGTYPE_ENCDATA) {
        verify_encdata(context, &pa_spake->u.encdata, cb, rock, enc_tkt_reply,
                       respond, arg);
    } else {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        k5_setmsg(context, ret, _("Unknown SPAKE request type"));
        (*respond)(arg, ret, NULL, NULL, NULL);
    }

    k5_free_pa_spake(context, pa_spake);
}

krb5_error_code
kdcpreauth_spake_initvt(krb5_context context, int maj_ver, int min_ver,
                        krb5_plugin_vtable vtable);

krb5_error_code
kdcpreauth_spake_initvt(krb5_context context, int maj_ver, int min_ver,
                        krb5_plugin_vtable vtable)
{
    krb5_kdcpreauth_vtable vt;
    static krb5_preauthtype pa_types[] = { KRB5_PADATA_SPAKE, 0 };

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_kdcpreauth_vtable)vtable;
    vt->name = "spake";
    vt->pa_type_list = pa_types;
    vt->init = spake_init;
    vt->fini = spake_fini;
    vt->edata = spake_edata;
    vt->verify = spake_verify;
    return 0;
}
