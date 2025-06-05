/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_preauth_ec.c - Encrypted challenge kdcpreauth module */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
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

/*
 * Implement Encrypted Challenge fast factor from
 * draft-ietf-krb-wg-preauth-framework
 */

#include <k5-int.h>
#include <krb5/kdcpreauth_plugin.h>
#include "kdc_util.h"

static void
ec_edata(krb5_context context, krb5_kdc_req *request,
         krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
         krb5_kdcpreauth_moddata moddata, krb5_preauthtype pa_type,
         krb5_kdcpreauth_edata_respond_fn respond, void *arg)
{
    krb5_keyblock *armor_key = cb->fast_armor(context, rock);

    /* Encrypted challenge only works with FAST, and requires a client key. */
    if (armor_key == NULL || !cb->have_client_keys(context, rock))
        (*respond)(arg, ENOENT, NULL);
    else
        (*respond)(arg, 0, NULL);
}

static void
ec_verify(krb5_context context, krb5_data *req_pkt, krb5_kdc_req *request,
          krb5_enc_tkt_part *enc_tkt_reply, krb5_pa_data *data,
          krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
          krb5_kdcpreauth_moddata moddata,
          krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    krb5_error_code ret;
    krb5_enc_data *enc = NULL;
    krb5_data der_enc_ts = empty_data(), der_enc_data;
    krb5_keyblock *armor_key = cb->fast_armor(context, rock);
    krb5_pa_enc_ts *ts = NULL;
    krb5_keyblock *client_keys = NULL;
    krb5_keyblock *challenge_key = NULL;
    krb5_keyblock *kdc_challenge_key;
    krb5_kdcpreauth_modreq modreq = NULL;
    int i = 0;
    char *ai = NULL, *realmstr = NULL;
    krb5_data realm = request->server->realm;

    if (armor_key == NULL) {
        ret = ENOENT;
        k5_setmsg(context, ret,
                  _("Encrypted Challenge used outside of FAST tunnel"));
        goto cleanup;
    }

    der_enc_data = make_data(data->contents, data->length);
    ret = decode_krb5_enc_data(&der_enc_data, &enc);
    if (ret)
        goto cleanup;

    ret = alloc_data(&der_enc_ts, enc->ciphertext.length);
    if (ret)
        goto cleanup;

    /* Check for a configured auth indicator. */
    realmstr = k5memdup0(realm.data, realm.length, &ret);
    if (realmstr == NULL)
        goto cleanup;
    ret = profile_get_string(context->profile, KRB5_CONF_REALMS, realmstr,
                             KRB5_CONF_ENCRYPTED_CHALLENGE_INDICATOR, NULL,
                             &ai);
    if (ret)
        goto cleanup;

    ret = cb->client_keys(context, rock, &client_keys);
    if (ret)
        goto cleanup;
    for (i = 0; client_keys[i].enctype != ENCTYPE_NULL; i++) {
        ret = krb5_c_fx_cf2_simple(context, armor_key, "clientchallengearmor",
                                   &client_keys[i], "challengelongterm",
                                   &challenge_key);
        if (ret)
            goto cleanup;
        ret = krb5_c_decrypt(context, challenge_key,
                             KRB5_KEYUSAGE_ENC_CHALLENGE_CLIENT, NULL, enc,
                             &der_enc_ts);
        krb5_free_keyblock(context, challenge_key);
        if (!ret)
            break;
    }

    if (client_keys[i].enctype == ENCTYPE_NULL) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        k5_setmsg(context, ret,
                  _("Incorrect password in encrypted challenge"));
        goto cleanup;
    }

    ret = decode_krb5_pa_enc_ts(&der_enc_ts, &ts);
    if (ret)
        goto cleanup;
    ret = krb5_check_clockskew(context, ts->patimestamp);
    if (ret)
        goto cleanup;

    enc_tkt_reply->flags |= TKT_FLG_PRE_AUTH;

    /*
     * If this fails, we won't generate a reply to the client.  That may cause
     * the client to fail, but at this point the KDC has considered this a
     * success, so the return value is ignored.
     */
    if (krb5_c_fx_cf2_simple(context, armor_key, "kdcchallengearmor",
                             &client_keys[i], "challengelongterm",
                             &kdc_challenge_key) == 0) {
        modreq = (krb5_kdcpreauth_modreq)kdc_challenge_key;
        if (ai != NULL)
            cb->add_auth_indicator(context, rock, ai);
    }

cleanup:
    cb->free_keys(context, rock, client_keys);
    free(der_enc_ts.data);
    krb5_free_enc_data(context, enc);
    krb5_free_pa_enc_ts(context, ts);
    free(realmstr);
    free(ai);

    (*respond)(arg, ret, modreq, NULL, NULL);
}

static krb5_error_code
ec_return(krb5_context context, krb5_pa_data *padata, krb5_data *req_pkt,
          krb5_kdc_req *request, krb5_kdc_rep *reply,
          krb5_keyblock *encrypting_key, krb5_pa_data **send_pa,
          krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
          krb5_kdcpreauth_moddata moddata, krb5_kdcpreauth_modreq modreq)
{
    krb5_error_code ret;
    krb5_keyblock *challenge_key = (krb5_keyblock *)modreq;
    krb5_pa_enc_ts ts;
    krb5_data *der_enc_ts = NULL, *der_enc_data = NULL;
    krb5_enc_data enc;
    krb5_pa_data *pa = NULL;

    enc.ciphertext.data = NULL;

    if (challenge_key == NULL)
        return 0;

    ret = krb5_us_timeofday(context, &ts.patimestamp, &ts.pausec);
    if (ret)
        goto cleanup;
    ret = encode_krb5_pa_enc_ts(&ts, &der_enc_ts);
    if (ret)
        goto cleanup;
    ret = krb5_encrypt_helper(context, challenge_key,
                              KRB5_KEYUSAGE_ENC_CHALLENGE_KDC, der_enc_ts,
                              &enc);
    if (ret)
        goto cleanup;
    ret = encode_krb5_enc_data(&enc, &der_enc_data);
    if (ret)
        goto cleanup;

    pa = k5alloc(sizeof(*pa), &ret);
    if (pa == NULL)
        goto cleanup;
    pa->pa_type = KRB5_PADATA_ENCRYPTED_CHALLENGE;
    pa->length = der_enc_data->length;
    /* Steal the data pointer from der_enc_data. */
    pa->contents = (unsigned char *)der_enc_data->data;
    der_enc_data->data = NULL;

    *send_pa = pa;

cleanup:
    krb5_free_keyblock(context, challenge_key);
    krb5_free_data(context, der_enc_data);
    krb5_free_data(context, der_enc_ts);
    krb5_free_data_contents(context, &enc.ciphertext);
    return ret;
}

static krb5_preauthtype ec_types[] = {
    KRB5_PADATA_ENCRYPTED_CHALLENGE, 0};

krb5_error_code
kdcpreauth_encrypted_challenge_initvt(krb5_context context, int maj_ver,
                                      int min_ver, krb5_plugin_vtable vtable)
{
    krb5_kdcpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_kdcpreauth_vtable)vtable;
    vt->name = "encrypted_challenge";
    vt->pa_type_list = ec_types;
    vt->edata = ec_edata;
    vt->verify = ec_verify;
    vt->return_padata = ec_return;
    return 0;
}
