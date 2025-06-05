/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/rd_priv.c - krb5_rd_priv() */
/*
 * Copyright 1990,1991,2007,2019 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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
#include "int-proto.h"
#include "auth_con.h"

/*
 * Unmarshal a KRB-PRIV message from der_krbpriv, placing the confidential user
 * data in *userdata_out, ciphertext in *enc_out, and replay data in
 * *rdata_out.  The caller should free *userdata_out and *enc_out when
 * finished.
 */
static krb5_error_code
read_krbpriv(krb5_context context, krb5_auth_context authcon,
             const krb5_data *der_krbpriv, const krb5_key key,
             krb5_replay_data *rdata_out, krb5_data *userdata_out,
             krb5_enc_data *enc_out)
{
    krb5_error_code ret;
    krb5_priv *privmsg = NULL;
    krb5_data plaintext = empty_data();
    krb5_priv_enc_part *encpart = NULL;
    krb5_data *cstate;

    if (!krb5_is_krb_priv(der_krbpriv))
        return KRB5KRB_AP_ERR_MSG_TYPE;

    /* decode private message */
    ret = decode_krb5_priv(der_krbpriv, &privmsg);
    if (ret)
        return ret;

    ret = alloc_data(&plaintext, privmsg->enc_part.ciphertext.length);
    if (ret)
        goto cleanup;

    cstate = (authcon->cstate.length > 0) ? &authcon->cstate : NULL;
    ret = krb5_k_decrypt(context, key, KRB5_KEYUSAGE_KRB_PRIV_ENCPART, cstate,
                         &privmsg->enc_part, &plaintext);
    if (ret)
        goto cleanup;

    ret = decode_krb5_enc_priv_part(&plaintext, &encpart);
    if (ret)
        goto cleanup;

    ret = k5_privsafe_check_addrs(context, authcon, encpart->s_address,
                                  encpart->r_address);
    if (ret)
        goto cleanup;

    rdata_out->timestamp = encpart->timestamp;
    rdata_out->usec = encpart->usec;
    rdata_out->seq = encpart->seq_number;

    *userdata_out = encpart->user_data;
    encpart->user_data.data = NULL;

    *enc_out = privmsg->enc_part;
    memset(&privmsg->enc_part, 0, sizeof(privmsg->enc_part));

cleanup:
    krb5_free_priv_enc_part(context, encpart);
    krb5_free_priv(context, privmsg);
    zapfree(plaintext.data, plaintext.length);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_rd_priv(krb5_context context, krb5_auth_context authcon,
             const krb5_data *inbuf, krb5_data *userdata_out,
             krb5_replay_data *rdata_out)
{
    krb5_error_code ret;
    krb5_key key;
    krb5_replay_data rdata;
    krb5_enc_data enc;
    krb5_data userdata = empty_data();
    const krb5_int32 flags = authcon->auth_context_flags;

    *userdata_out = empty_data();
    memset(&enc, 0, sizeof(enc));

    if (((flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
         (flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) && rdata_out == NULL)
        return KRB5_RC_REQUIRED;

    key = (authcon->recv_subkey != NULL) ? authcon->recv_subkey : authcon->key;
    memset(&rdata, 0, sizeof(rdata));
    ret = read_krbpriv(context, authcon, inbuf, key, &rdata, &userdata, &enc);
    if (ret)
        goto cleanup;

    ret = k5_privsafe_check_replay(context, authcon, &rdata, &enc, NULL);
    if (ret)
        goto cleanup;

    if (flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
        if (!k5_privsafe_check_seqnum(context, authcon, rdata.seq)) {
            ret = KRB5KRB_AP_ERR_BADORDER;
            goto cleanup;
        }
        authcon->remote_seq_number++;
    }

    if ((flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
        (flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) {
        rdata_out->timestamp = rdata.timestamp;
        rdata_out->usec = rdata.usec;
        rdata_out->seq = rdata.seq;
    }

    *userdata_out = userdata;
    userdata = empty_data();

cleanup:
    krb5_free_data_contents(context, &enc.ciphertext);
    krb5_free_data_contents(context, &userdata);
    return ret;
}
