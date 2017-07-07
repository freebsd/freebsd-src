/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/otp/main.c - OTP kdcpreauth module definition */
/*
 * Copyright 2011 NORDUnet A/S.  All rights reserved.
 * Copyright 2013 Red Hat, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "k5-json.h"
#include <krb5/preauth_plugin.h>
#include "otp_state.h"

#include <errno.h>
#include <ctype.h>

static krb5_preauthtype otp_pa_type_list[] =
  { KRB5_PADATA_OTP_REQUEST, 0 };

struct request_state {
    krb5_context context;
    krb5_kdcpreauth_verify_respond_fn respond;
    void *arg;
    krb5_enc_tkt_part *enc_tkt_reply;
    krb5_kdcpreauth_callbacks preauth_cb;
    krb5_kdcpreauth_rock rock;
};

static krb5_error_code
decrypt_encdata(krb5_context context, krb5_keyblock *armor_key,
                krb5_pa_otp_req *req, krb5_data *out)
{
    krb5_error_code retval;
    krb5_data plaintext;

    if (req == NULL)
        return EINVAL;

    retval = alloc_data(&plaintext, req->enc_data.ciphertext.length);
    if (retval)
        return retval;

    retval = krb5_c_decrypt(context, armor_key, KRB5_KEYUSAGE_PA_OTP_REQUEST,
                            NULL, &req->enc_data, &plaintext);
    if (retval != 0) {
        com_err("otp", retval, "Unable to decrypt encData in PA-OTP-REQUEST");
        free(plaintext.data);
        return retval;
    }

    *out = plaintext;
    return 0;
}

static krb5_error_code
nonce_verify(krb5_context ctx, krb5_keyblock *armor_key,
             const krb5_data *nonce)
{
    krb5_error_code retval;
    krb5_timestamp ts;
    krb5_data *er = NULL;

    if (armor_key == NULL || nonce->data == NULL) {
        retval = EINVAL;
        goto out;
    }

    /* Decode the PA-OTP-ENC-REQUEST structure. */
    retval = decode_krb5_pa_otp_enc_req(nonce, &er);
    if (retval != 0)
        goto out;

    /* Make sure the nonce is exactly the same size as the one generated. */
    if (er->length != armor_key->length + sizeof(krb5_timestamp))
        goto out;

    /* Check to make sure the timestamp at the beginning is still valid. */
    ts = load_32_be(er->data);
    retval = krb5_check_clockskew(ctx, ts);

out:
    krb5_free_data(ctx, er);
    return retval;
}

static krb5_error_code
timestamp_verify(krb5_context ctx, const krb5_data *nonce)
{
    krb5_error_code retval = EINVAL;
    krb5_pa_enc_ts *et = NULL;

    if (nonce->data == NULL)
        goto out;

    /* Decode the PA-ENC-TS-ENC structure. */
    retval = decode_krb5_pa_enc_ts(nonce, &et);
    if (retval != 0)
        goto out;

    /* Check the clockskew. */
    retval = krb5_check_clockskew(ctx, et->patimestamp);

out:
    krb5_free_pa_enc_ts(ctx, et);
    return retval;
}

static krb5_error_code
nonce_generate(krb5_context ctx, unsigned int length, krb5_data *nonce_out)
{
    krb5_data nonce;
    krb5_error_code retval;
    krb5_timestamp now;

    retval = krb5_timeofday(ctx, &now);
    if (retval != 0)
        return retval;

    retval = alloc_data(&nonce, sizeof(now) + length);
    if (retval != 0)
        return retval;

    retval = krb5_c_random_make_octets(ctx, &nonce);
    if (retval != 0) {
        free(nonce.data);
        return retval;
    }

    store_32_be(now, nonce.data);
    *nonce_out = nonce;
    return 0;
}

static void
on_response(void *data, krb5_error_code retval, otp_response response,
            char *const *indicators)
{
    struct request_state rs = *(struct request_state *)data;
    char *const *ind;

    free(data);

    if (retval == 0 && response != otp_response_success)
        retval = KRB5_PREAUTH_FAILED;

    if (retval == 0)
        rs.enc_tkt_reply->flags |= TKT_FLG_PRE_AUTH;

    for (ind = indicators; ind != NULL && *ind != NULL && retval == 0; ind++)
        retval = rs.preauth_cb->add_auth_indicator(rs.context, rs.rock, *ind);

    rs.respond(rs.arg, retval, NULL, NULL, NULL);
}

static krb5_error_code
otp_init(krb5_context context, krb5_kdcpreauth_moddata *moddata_out,
         const char **realmnames)
{
    krb5_error_code retval;
    otp_state *state;

    retval = otp_state_new(context, &state);
    if (retval)
        return retval;
    *moddata_out = (krb5_kdcpreauth_moddata)state;
    return 0;
}

static void
otp_fini(krb5_context context, krb5_kdcpreauth_moddata moddata)
{
    otp_state_free((otp_state *)moddata);
}

static int
otp_flags(krb5_context context, krb5_preauthtype pa_type)
{
    return PA_REPLACES_KEY;
}

static void
otp_edata(krb5_context context, krb5_kdc_req *request,
          krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
          krb5_kdcpreauth_moddata moddata, krb5_preauthtype pa_type,
          krb5_kdcpreauth_edata_respond_fn respond, void *arg)
{
    krb5_otp_tokeninfo ti, *tis[2] = { &ti, NULL };
    krb5_keyblock *armor_key = NULL;
    krb5_pa_otp_challenge chl;
    krb5_pa_data *pa = NULL;
    krb5_error_code retval;
    krb5_data *encoding;
    char *config;

    /* Determine if otp is enabled for the user. */
    retval = cb->get_string(context, rock, "otp", &config);
    if (retval == 0 && config == NULL)
        retval = ENOENT;
    if (retval != 0)
        goto out;
    cb->free_string(context, rock, config);

    /* Get the armor key.  This indicates the length of random data to use in
     * the nonce. */
    armor_key = cb->fast_armor(context, rock);
    if (armor_key == NULL) {
        retval = ENOENT;
        goto out;
    }

    /* Build the (mostly empty) challenge. */
    memset(&ti, 0, sizeof(ti));
    memset(&chl, 0, sizeof(chl));
    chl.tokeninfo = tis;
    ti.format = -1;
    ti.length = -1;
    ti.iteration_count = -1;

    /* Generate the nonce. */
    retval = nonce_generate(context, armor_key->length, &chl.nonce);
    if (retval != 0)
        goto out;

    /* Build the output pa-data. */
    retval = encode_krb5_pa_otp_challenge(&chl, &encoding);
    if (retval != 0)
        goto out;
    pa = k5alloc(sizeof(krb5_pa_data), &retval);
    if (pa == NULL) {
        krb5_free_data(context, encoding);
        goto out;
    }
    pa->pa_type = KRB5_PADATA_OTP_CHALLENGE;
    pa->contents = (krb5_octet *)encoding->data;
    pa->length = encoding->length;
    free(encoding);

out:
    (*respond)(arg, retval, pa);
}

static void
otp_verify(krb5_context context, krb5_data *req_pkt, krb5_kdc_req *request,
           krb5_enc_tkt_part *enc_tkt_reply, krb5_pa_data *pa,
           krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
           krb5_kdcpreauth_moddata moddata,
           krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    krb5_keyblock *armor_key = NULL;
    krb5_pa_otp_req *req = NULL;
    struct request_state *rs;
    krb5_error_code retval;
    krb5_data d, plaintext;
    char *config;

    /* Get the FAST armor key. */
    armor_key = cb->fast_armor(context, rock);
    if (armor_key == NULL) {
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        com_err("otp", retval, "No armor key found when verifying padata");
        goto error;
    }

    /* Decode the request. */
    d = make_data(pa->contents, pa->length);
    retval = decode_krb5_pa_otp_req(&d, &req);
    if (retval != 0) {
        com_err("otp", retval, "Unable to decode OTP request");
        goto error;
    }

    /* Decrypt the nonce from the request. */
    retval = decrypt_encdata(context, armor_key, req, &plaintext);
    if (retval != 0) {
        com_err("otp", retval, "Unable to decrypt nonce");
        goto error;
    }

    /* Verify the nonce or timestamp. */
    retval = nonce_verify(context, armor_key, &plaintext);
    if (retval != 0)
        retval = timestamp_verify(context, &plaintext);
    krb5_free_data_contents(context, &plaintext);
    if (retval != 0) {
        com_err("otp", retval, "Unable to verify nonce or timestamp");
        goto error;
    }

    /* Create the request state.  Save the response callback, and the
     * enc_tkt_reply pointer so we can set the TKT_FLG_PRE_AUTH flag later. */
    rs = k5alloc(sizeof(struct request_state), &retval);
    if (rs == NULL)
        goto error;
    rs->context = context;
    rs->arg = arg;
    rs->respond = respond;
    rs->enc_tkt_reply = enc_tkt_reply;
    rs->preauth_cb = cb;
    rs->rock = rock;

    /* Get the principal's OTP configuration string. */
    retval = cb->get_string(context, rock, "otp", &config);
    if (retval == 0 && config == NULL)
        retval = KRB5_PREAUTH_FAILED;
    if (retval != 0) {
        free(rs);
        goto error;
    }

    /* Send the request. */
    otp_state_verify((otp_state *)moddata, cb->event_context(context, rock),
                     request->client, config, req, on_response, rs);
    cb->free_string(context, rock, config);

    k5_free_pa_otp_req(context, req);
    return;

error:
    k5_free_pa_otp_req(context, req);
    (*respond)(arg, retval, NULL, NULL, NULL);
}

static krb5_error_code
otp_return_padata(krb5_context context, krb5_pa_data *padata,
                  krb5_data *req_pkt, krb5_kdc_req *request,
                  krb5_kdc_rep *reply, krb5_keyblock *encrypting_key,
                  krb5_pa_data **send_pa_out, krb5_kdcpreauth_callbacks cb,
                  krb5_kdcpreauth_rock rock, krb5_kdcpreauth_moddata moddata,
                  krb5_kdcpreauth_modreq modreq)
{
    krb5_keyblock *armor_key = NULL;

    if (padata->length == 0)
        return 0;

    /* Get the armor key. */
    armor_key = cb->fast_armor(context, rock);
    if (!armor_key) {
      com_err("otp", ENOENT, "No armor key found when returning padata");
      return ENOENT;
    }

    /* Replace the reply key with the FAST armor key. */
    krb5_free_keyblock_contents(context, encrypting_key);
    return krb5_copy_keyblock_contents(context, armor_key, encrypting_key);
}

krb5_error_code
kdcpreauth_otp_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable);

krb5_error_code
kdcpreauth_otp_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    krb5_kdcpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;

    vt = (krb5_kdcpreauth_vtable)vtable;
    vt->name = "otp";
    vt->pa_type_list = otp_pa_type_list;
    vt->init = otp_init;
    vt->fini = otp_fini;
    vt->flags = otp_flags;
    vt->edata = otp_edata;
    vt->verify = otp_verify;
    vt->return_padata = otp_return_padata;

    com_err("otp", 0, "Loaded");

    return 0;
}
