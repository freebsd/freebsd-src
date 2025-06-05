/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/mk_cred.c - definition of krb5_mk_ncred(), krb5_mk_1cred() */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
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
#include "int-proto.h"
#include "auth_con.h"

/* Encrypt the enc_part of krb5_cred.  key may be NULL to use the unencrypted
 * KRB-CRED form (RFC 6448). */
static krb5_error_code
encrypt_credencpart(krb5_context context, krb5_cred_enc_part *encpart,
                    krb5_key key, krb5_enc_data *encdata_out)
{
    krb5_error_code ret;
    krb5_data *der_enccred;

    /* Start by encoding to-be-encrypted part of the message. */
    ret = encode_krb5_enc_cred_part(encpart, &der_enccred);
    if (ret)
        return ret;

    if (key == NULL) {
        /* Just copy the encoded data to the ciphertext area. */
        encdata_out->enctype = ENCTYPE_NULL;
        encdata_out->ciphertext = *der_enccred;
        free(der_enccred);
        return 0;
    }

    ret = k5_encrypt_keyhelper(context, key, KRB5_KEYUSAGE_KRB_CRED_ENCPART,
                               der_enccred, encdata_out);

    zapfreedata(der_enccred);
    return ret;
}

/*
 * Marshal a KRB-CRED message into der_out, encrypted with key (or unencrypted
 * if key is NULL).  Store the ciphertext in enc_out.  Use the timestamp and
 * sequence number from rdata and the addresses from local_addr and remote_addr
 * (either of which may be NULL).  der_out and enc_out should be freed by the
 * caller when finished.
 */
static krb5_error_code
create_krbcred(krb5_context context, krb5_creds **creds, krb5_key key,
               const krb5_replay_data *rdata, krb5_address *local_addr,
               krb5_address *remote_addr, krb5_data **der_out,
               krb5_enc_data *enc_out)
{
    krb5_error_code ret;
    krb5_cred_enc_part credenc;
    krb5_cred cred;
    krb5_ticket **tickets = NULL;
    krb5_cred_info **ticket_info = NULL, *tinfos = NULL;
    krb5_enc_data enc;
    size_t i, ncreds;

    *der_out = NULL;
    memset(enc_out, 0, sizeof(*enc_out));
    memset(&enc, 0, sizeof(enc));

    for (ncreds = 0; creds[ncreds] != NULL; ncreds++);

    tickets = k5calloc(ncreds + 1, sizeof(*tickets), &ret);
    if (tickets == NULL)
        goto cleanup;

    ticket_info = k5calloc(ncreds + 1, sizeof(*ticket_info), &ret);
    if (ticket_info == NULL)
        goto cleanup;

    tinfos = k5calloc(ncreds, sizeof(*tinfos), &ret);
    if (tinfos == NULL)
        goto cleanup;

    /* For each credential in the list, decode the ticket and create a cred
     * info structure using alias pointers. */
    for (i = 0; i < ncreds; i++) {
        ret = decode_krb5_ticket(&creds[i]->ticket, &tickets[i]);
        if (ret)
            goto cleanup;

        tinfos[i].magic = KV5M_CRED_INFO;
        tinfos[i].times = creds[i]->times;
        tinfos[i].flags = creds[i]->ticket_flags;
        tinfos[i].session = &creds[i]->keyblock;
        tinfos[i].client = creds[i]->client;
        tinfos[i].server = creds[i]->server;
        tinfos[i].caddrs = creds[i]->addresses;
        ticket_info[i] = &tinfos[i];
    }

    /* Encrypt the credential encrypted part. */
    credenc.magic = KV5M_CRED_ENC_PART;
    credenc.s_address = local_addr;
    credenc.r_address = remote_addr;
    credenc.nonce = rdata->seq;
    credenc.usec = rdata->usec;
    credenc.timestamp = rdata->timestamp;
    credenc.ticket_info = ticket_info;
    ret = encrypt_credencpart(context, &credenc, key, &enc);
    if (ret)
        goto cleanup;

    /* Encode the KRB-CRED message. */
    cred.magic = KV5M_CRED;
    cred.tickets = tickets;
    cred.enc_part = enc;
    ret = encode_krb5_cred(&cred, der_out);
    if (ret)
        goto cleanup;

    *enc_out = enc;
    memset(&enc, 0, sizeof(enc));

cleanup:
    krb5_free_tickets(context, tickets);
    krb5_free_data_contents(context, &enc.ciphertext);
    free(tinfos);
    free(ticket_info);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_mk_ncred(krb5_context context, krb5_auth_context authcon,
              krb5_creds **creds, krb5_data **der_out,
              krb5_replay_data *rdata_out)
{
    krb5_error_code ret;
    krb5_key key;
    krb5_replay_data rdata;
    krb5_data *der_krbcred = NULL;
    krb5_enc_data enc;
    krb5_address *local_addr, *remote_addr, lstorage, rstorage;

    *der_out = NULL;
    memset(&enc, 0, sizeof(enc));
    memset(&lstorage, 0, sizeof(lstorage));
    memset(&rstorage, 0, sizeof(rstorage));

    if (creds == NULL)
        return KRB5KRB_AP_ERR_BADADDR;

    ret = k5_privsafe_gen_rdata(context, authcon, &rdata, rdata_out);
    if (ret)
        goto cleanup;
    /* Historically we always set the timestamp, so keep doing that. */
    if (rdata.timestamp == 0) {
        ret = krb5_us_timeofday(context, &rdata.timestamp, &rdata.usec);
        if (ret)
            goto cleanup;
    }

    ret = k5_privsafe_gen_addrs(context, authcon, &lstorage, &rstorage,
                                &local_addr, &remote_addr);
    if (ret)
        goto cleanup;

    key = (authcon->send_subkey != NULL) ? authcon->send_subkey : authcon->key;
    ret = create_krbcred(context, creds, key, &rdata, local_addr, remote_addr,
                         &der_krbcred, &enc);
    if (ret)
        goto cleanup;

    if (key != NULL) {
        ret = k5_privsafe_check_replay(context, authcon, NULL, &enc, NULL);
        if (ret)
            goto cleanup;
    }

    *der_out = der_krbcred;
    der_krbcred = NULL;
    if ((authcon->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) ||
        (authcon->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE))
        authcon->local_seq_number++;

cleanup:
    krb5_free_data_contents(context, &enc.ciphertext);
    free(lstorage.contents);
    free(rstorage.contents);
    zapfreedata(der_krbcred);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_mk_1cred(krb5_context context, krb5_auth_context authcon,
              krb5_creds *creds, krb5_data **der_out,
              krb5_replay_data *rdata_out)
{
    krb5_error_code retval;
    krb5_creds **list;

    list = calloc(2, sizeof(*list));
    if (list == NULL)
        return ENOMEM;

    list[0] = creds;
    list[1] = NULL;
    retval = krb5_mk_ncred(context, authcon, list, der_out, rdata_out);
    free(list);
    return retval;
}
