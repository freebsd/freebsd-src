/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/mk_priv.c - definition of krb5_mk_priv() */
/*
 * Copyright 1990,1991,2019 by the Massachusetts Institute of Technology.
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
 * Marshal a KRB-PRIV message into der_out, encrypted with key.  Store the
 * ciphertext in enc_out.  Use the timestamp and sequence number from rdata and
 * the addresses from local_addr and remote_addr (the second of which may be
 * NULL).  der_out and enc_out should be freed by the caller when finished.
 */
static krb5_error_code
create_krbpriv(krb5_context context, const krb5_data *userdata,
               krb5_key key, const krb5_replay_data *rdata,
               krb5_address *local_addr, krb5_address *remote_addr,
               krb5_data *cstate, krb5_data *der_out, krb5_enc_data *enc_out)
{
    krb5_enctype enctype = krb5_k_key_enctype(context, key);
    krb5_error_code ret;
    krb5_priv privmsg;
    krb5_priv_enc_part encpart;
    krb5_data *der_encpart = NULL, *der_krbpriv;
    size_t enclen;

    memset(&privmsg, 0, sizeof(privmsg));
    privmsg.enc_part.kvno = 0;
    privmsg.enc_part.enctype = enctype;
    encpart.user_data = *userdata;
    encpart.s_address = local_addr;
    encpart.r_address = remote_addr;
    encpart.timestamp = rdata->timestamp;
    encpart.usec = rdata->usec;
    encpart.seq_number = rdata->seq;

    /* Start by encoding the to-be-encrypted part of the message. */
    ret = encode_krb5_enc_priv_part(&encpart, &der_encpart);
    if (ret)
        return ret;

    /* put together an eblock for this encryption */
    ret = krb5_c_encrypt_length(context, enctype, der_encpart->length,
                                &enclen);
    if (ret)
        goto cleanup;

    ret = alloc_data(&privmsg.enc_part.ciphertext, enclen);
    if (ret)
        goto cleanup;

    ret = krb5_k_encrypt(context, key, KRB5_KEYUSAGE_KRB_PRIV_ENCPART,
                         (cstate->length > 0) ? cstate : NULL, der_encpart,
                         &privmsg.enc_part);
    if (ret)
        goto cleanup;

    ret = encode_krb5_priv(&privmsg, &der_krbpriv);
    if (ret)
        goto cleanup;

    *der_out = *der_krbpriv;
    free(der_krbpriv);

    *enc_out = privmsg.enc_part;
    memset(&privmsg.enc_part, 0, sizeof(privmsg.enc_part));

cleanup:
    zapfree(privmsg.enc_part.ciphertext.data,
            privmsg.enc_part.ciphertext.length);
    zapfreedata(der_encpart);
    return ret;
}


krb5_error_code KRB5_CALLCONV
krb5_mk_priv(krb5_context context, krb5_auth_context authcon,
             const krb5_data *userdata, krb5_data *der_out,
             krb5_replay_data *rdata_out)
{
    krb5_error_code ret;
    krb5_key key;
    krb5_replay_data rdata;
    krb5_data der_krbpriv = empty_data();
    krb5_enc_data enc;
    krb5_address *local_addr, *remote_addr, lstorage, rstorage;

    *der_out = empty_data();
    memset(&enc, 0, sizeof(enc));
    memset(&lstorage, 0, sizeof(lstorage));
    memset(&rstorage, 0, sizeof(rstorage));
    if (!authcon->local_addr)
        return KRB5_LOCAL_ADDR_REQUIRED;

    ret = k5_privsafe_gen_rdata(context, authcon, &rdata, rdata_out);
    if (ret)
        goto cleanup;

    ret = k5_privsafe_gen_addrs(context, authcon, &lstorage, &rstorage,
                                &local_addr, &remote_addr);
    if (ret)
        goto cleanup;

    key = (authcon->send_subkey != NULL) ? authcon->send_subkey : authcon->key;
    ret = create_krbpriv(context, userdata, key, &rdata, local_addr,
                         remote_addr, &authcon->cstate, &der_krbpriv, &enc);
    if (ret)
        goto cleanup;

    ret = k5_privsafe_check_replay(context, authcon, NULL, &enc, NULL);
    if (ret)
        goto cleanup;

    *der_out = der_krbpriv;
    der_krbpriv = empty_data();
    if ((authcon->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) ||
        (authcon->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE))
        authcon->local_seq_number++;

cleanup:
    krb5_free_data_contents(context, &der_krbpriv);
    zapfree(enc.ciphertext.data, enc.ciphertext.length);
    free(lstorage.contents);
    free(rstorage.contents);
    return ret;
}
