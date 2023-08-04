/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/rd_cred.c - definition of krb5_rd_cred() */
/*
 * Copyright 1994-2009,2014 by the Massachusetts Institute of Technology.
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
 * Decrypt and decode the enc_part of a krb5_cred using the receiving subkey or
 * the session key of authcon.  If neither key is present, ctext->ciphertext is
 * assumed to be unencrypted plain text (RFC 6448).
 */
static krb5_error_code
decrypt_encpart(krb5_context context, krb5_enc_data *ctext,
                krb5_auth_context authcon, krb5_cred_enc_part **encpart_out)
{
    krb5_error_code ret;
    krb5_data plain = empty_data();
    krb5_boolean decrypted = FALSE;

    *encpart_out = NULL;

    if (authcon->recv_subkey == NULL && authcon->key == NULL)
        return decode_krb5_enc_cred_part(&ctext->ciphertext, encpart_out);

    ret = alloc_data(&plain, ctext->ciphertext.length);
    if (ret)
        return ret;
    if (authcon->recv_subkey != NULL) {
        ret = krb5_k_decrypt(context, authcon->recv_subkey,
                             KRB5_KEYUSAGE_KRB_CRED_ENCPART, 0, ctext, &plain);
        decrypted = (ret == 0);
    }
    if (!decrypted && authcon->key != NULL) {
        ret = krb5_k_decrypt(context, authcon->key,
                             KRB5_KEYUSAGE_KRB_CRED_ENCPART, 0, ctext, &plain);
        decrypted = (ret == 0);
    }
    if (decrypted)
        ret = decode_krb5_enc_cred_part(&plain, encpart_out);
    zapfree(plain.data, plain.length);
    return ret;
}

/* Produce a list of credentials from a KRB-CRED message and its enc_part. */
static krb5_error_code
make_cred_list(krb5_context context, krb5_cred *krbcred,
               krb5_cred_enc_part *encpart, krb5_creds ***creds_out)
{
    krb5_error_code ret = 0;
    krb5_creds **list = NULL;
    krb5_cred_info *info;
    krb5_data *ticket_data;
    size_t i, count;

    *creds_out = NULL;

    /* Allocate the list of creds. */
    for (count = 0; krbcred->tickets[count] != NULL; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        goto cleanup;

    /* For each credential, create a structure in the list of credentials and
     * copy the information. */
    for (i = 0; i < count; i++) {
        list[i] = k5alloc(sizeof(*list[i]), &ret);
        if (list[i] == NULL)
            goto cleanup;

        info = encpart->ticket_info[i];
        ret = krb5_copy_principal(context, info->client, &list[i]->client);
        if (ret)
            goto cleanup;

        ret = krb5_copy_principal(context, info->server, &list[i]->server);
        if (ret)
            goto cleanup;

        ret = krb5_copy_keyblock_contents(context, info->session,
                                          &list[i]->keyblock);
        if (ret)
            goto cleanup;

        ret = krb5_copy_addresses(context, info->caddrs, &list[i]->addresses);
        if (ret)
            goto cleanup;

        ret = encode_krb5_ticket(krbcred->tickets[i], &ticket_data);
        if (ret)
            goto cleanup;
        list[i]->ticket = *ticket_data;
        free(ticket_data);

        list[i]->is_skey = FALSE;
        list[i]->magic = KV5M_CREDS;
        list[i]->times = info->times;
        list[i]->ticket_flags = info->flags;
        list[i]->authdata = NULL;
        list[i]->second_ticket = empty_data();
    }

    *creds_out = list;
    list = NULL;

cleanup:
    krb5_free_tgt_creds(context, list);
    return ret;
}

/* Validate a KRB-CRED message in creddata, and return a list of forwarded
 * credentials along with replay cache information. */
krb5_error_code KRB5_CALLCONV
krb5_rd_cred(krb5_context context, krb5_auth_context authcon,
             krb5_data *creddata, krb5_creds ***creds_out,
             krb5_replay_data *replaydata_out)
{
    krb5_error_code ret = 0;
    krb5_creds **credlist = NULL;
    krb5_cred *krbcred = NULL;
    krb5_cred_enc_part *encpart = NULL;
    krb5_replay_data rdata;
    const krb5_int32 flags = authcon->auth_context_flags;

    *creds_out = NULL;

    if (((flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
         (flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) &&
        replaydata_out == NULL)
        return KRB5_RC_REQUIRED;

    ret = decode_krb5_cred(creddata, &krbcred);
    if (ret)
        goto cleanup;

    ret = decrypt_encpart(context, &krbcred->enc_part, authcon, &encpart);
    if (ret)
        goto cleanup;

    ret = make_cred_list(context, krbcred, encpart, &credlist);
    if (ret)
        goto cleanup;

    if (authcon->recv_subkey != NULL || authcon->key != NULL) {
        rdata.timestamp = encpart->timestamp;
        ret = k5_privsafe_check_replay(context, authcon, &rdata,
                                       &krbcred->enc_part, NULL);
        if (ret)
            goto cleanup;
    }

    if (flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
        if (authcon->remote_seq_number != (uint32_t)encpart->nonce) {
            ret = KRB5KRB_AP_ERR_BADORDER;
            goto cleanup;
        }
        authcon->remote_seq_number++;
    }

    *creds_out = credlist;
    credlist = NULL;
    if ((flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
        (flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) {
        replaydata_out->timestamp = encpart->timestamp;
        replaydata_out->usec = encpart->usec;
        replaydata_out->seq = encpart->nonce;
    }

cleanup:
    krb5_free_tgt_creds(context, credlist);
    krb5_free_cred(context, krbcred);
    krb5_free_cred_enc_part(context, encpart);
    free(encpart);              /* krb5_free_cred_enc_part doesn't do this */
    return ret;
}
