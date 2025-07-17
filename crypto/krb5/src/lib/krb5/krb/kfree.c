/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/kfree.c */
/*
 * Copyright 1990-1998, 2009 by the Massachusetts Institute of Technology.
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
#include "k5-spake.h"
#include <assert.h>

void KRB5_CALLCONV
krb5_free_address(krb5_context context, krb5_address *val)
{
    if (val == NULL)
        return;
    free(val->contents);
    free(val);
}

void KRB5_CALLCONV
krb5_free_addresses(krb5_context context, krb5_address **val)
{
    krb5_address **temp;

    if (val == NULL)
        return;
    for (temp = val; *temp; temp++) {
        free((*temp)->contents);
        free(*temp);
    }
    free(val);
}

void KRB5_CALLCONV
krb5_free_ap_rep(krb5_context context, krb5_ap_rep *val)
{
    if (val == NULL)
        return;
    free(val->enc_part.ciphertext.data);
    free(val);
}

void KRB5_CALLCONV
krb5_free_ap_req(krb5_context context, krb5_ap_req *val)
{
    if (val == NULL)
        return;
    krb5_free_ticket(context, val->ticket);
    free(val->authenticator.ciphertext.data);
    free(val);
}

void KRB5_CALLCONV
krb5_free_ap_rep_enc_part(krb5_context context, krb5_ap_rep_enc_part *val)
{
    if (val == NULL)
        return;
    krb5_free_keyblock(context, val->subkey);
    free(val);
}

void KRB5_CALLCONV
krb5_free_authenticator_contents(krb5_context context, krb5_authenticator *val)
{
    if (val == NULL)
        return;
    krb5_free_checksum(context, val->checksum);
    val->checksum = 0;
    krb5_free_principal(context, val->client);
    val->client = 0;
    krb5_free_keyblock(context, val->subkey);
    val->subkey = 0;
    krb5_free_authdata(context, val->authorization_data);
    val->authorization_data = 0;
}

void KRB5_CALLCONV
krb5_free_authenticator(krb5_context context, krb5_authenticator *val)
{
    if (val == NULL)
        return;
    krb5_free_authenticator_contents(context, val);
    free(val);
}

void KRB5_CALLCONV
krb5_free_checksum(krb5_context context, krb5_checksum *val)
{
    if (val == NULL)
        return;
    krb5_free_checksum_contents(context, val);
    free(val);
}

void KRB5_CALLCONV
krb5_free_checksum_contents(krb5_context context, krb5_checksum *val)
{
    if (val == NULL)
        return;
    free(val->contents);
    val->contents = NULL;
    val->length = 0;
}

void KRB5_CALLCONV
krb5_free_cred(krb5_context context, krb5_cred *val)
{
    if (val == NULL)
        return;
    krb5_free_tickets(context, val->tickets);
    free(val->enc_part.ciphertext.data);
    free(val);
}

/*
 * krb5_free_cred_contents zeros out the session key, and then frees
 * the credentials structures
 */

void KRB5_CALLCONV
krb5_free_cred_contents(krb5_context context, krb5_creds *val)
{
    if (val == NULL)
        return;
    krb5_free_principal(context, val->client);
    val->client = 0;
    krb5_free_principal(context, val->server);
    val->server = 0;
    krb5_free_keyblock_contents(context, &val->keyblock);
    free(val->ticket.data);
    val->ticket.data = 0;
    free(val->second_ticket.data);
    val->second_ticket.data = 0;
    krb5_free_addresses(context, val->addresses);
    val->addresses = 0;
    krb5_free_authdata(context, val->authdata);
    val->authdata = 0;
}

void KRB5_CALLCONV
krb5_free_cred_enc_part(krb5_context context, krb5_cred_enc_part *val)
{
    krb5_cred_info **temp;

    if (val == NULL)
        return;
    krb5_free_address(context, val->r_address);
    val->r_address = 0;
    krb5_free_address(context, val->s_address);
    val->s_address = 0;

    if (val->ticket_info) {
        for (temp = val->ticket_info; *temp; temp++) {
            krb5_free_keyblock(context, (*temp)->session);
            krb5_free_principal(context, (*temp)->client);
            krb5_free_principal(context, (*temp)->server);
            krb5_free_addresses(context, (*temp)->caddrs);
            free(*temp);
        }
        free(val->ticket_info);
        val->ticket_info = 0;
    }
}


void KRB5_CALLCONV
krb5_free_creds(krb5_context context, krb5_creds *val)
{
    if (val == NULL)
        return;
    krb5_free_cred_contents(context, val);
    free(val);
}


void KRB5_CALLCONV
krb5_free_data(krb5_context context, krb5_data *val)
{
    if (val == NULL)
        return;
    free(val->data);
    free(val);
}


void KRB5_CALLCONV
krb5_free_octet_data(krb5_context context, krb5_octet_data *val)
{
    if (val == NULL)
        return;
    free(val->data);
    free(val);
}

void KRB5_CALLCONV
krb5_free_data_contents(krb5_context context, krb5_data *val)
{
    if (val == NULL)
        return;
    free(val->data);
    val->data = NULL;
    val->length = 0;
}

void KRB5_CALLCONV
krb5_free_enc_data(krb5_context context, krb5_enc_data *val)
{
    if (val == NULL)
        return;
    krb5_free_data_contents(context, &val->ciphertext);
    free(val);
}

void krb5_free_etype_info(krb5_context context, krb5_etype_info info)
{
    int i;

    if (info == NULL)
        return;
    for (i=0; info[i] != NULL; i++) {
        free(info[i]->salt);
        krb5_free_data_contents(context, &info[i]->s2kparams);
        free(info[i]);
    }
    free(info);
}


void KRB5_CALLCONV
krb5_free_enc_kdc_rep_part(krb5_context context, krb5_enc_kdc_rep_part *val)
{
    if (val == NULL)
        return;
    krb5_free_keyblock(context, val->session);
    krb5_free_last_req(context, val->last_req);
    krb5_free_principal(context, val->server);
    krb5_free_addresses(context, val->caddrs);
    krb5_free_pa_data(context, val->enc_padata);
    free(val);
}

void KRB5_CALLCONV
krb5_free_enc_tkt_part(krb5_context context, krb5_enc_tkt_part *val)
{
    if (val == NULL)
        return;
    krb5_free_keyblock(context, val->session);
    krb5_free_principal(context, val->client);
    free(val->transited.tr_contents.data);
    krb5_free_addresses(context, val->caddrs);
    krb5_free_authdata(context, val->authorization_data);
    free(val);
}


void KRB5_CALLCONV
krb5_free_error(krb5_context context, krb5_error *val)
{
    if (val == NULL)
        return;
    krb5_free_principal(context, val->client);
    krb5_free_principal(context, val->server);
    free(val->text.data);
    free(val->e_data.data);
    free(val);
}

void KRB5_CALLCONV
krb5_free_kdc_rep(krb5_context context, krb5_kdc_rep *val)
{
    if (val == NULL)
        return;
    krb5_free_pa_data(context, val->padata);
    krb5_free_principal(context, val->client);
    krb5_free_ticket(context, val->ticket);
    free(val->enc_part.ciphertext.data);
    krb5_free_enc_kdc_rep_part(context, val->enc_part2);
    free(val);
}


void KRB5_CALLCONV
krb5_free_kdc_req(krb5_context context, krb5_kdc_req *val)
{
    if (val == NULL)
        return;
    krb5_free_pa_data(context, val->padata);
    krb5_free_principal(context, val->client);
    krb5_free_principal(context, val->server);
    free(val->ktype);
    krb5_free_addresses(context, val->addresses);
    free(val->authorization_data.ciphertext.data);
    krb5_free_authdata(context, val->unenc_authdata);
    krb5_free_tickets(context, val->second_ticket);
    free(val);
}

void KRB5_CALLCONV
krb5_free_keyblock_contents(krb5_context context, krb5_keyblock *key)
{
    krb5int_c_free_keyblock_contents (context, key);
}

void KRB5_CALLCONV
krb5_free_keyblock(krb5_context context, krb5_keyblock *val)
{
    krb5int_c_free_keyblock (context, val);
}



void KRB5_CALLCONV
krb5_free_last_req(krb5_context context, krb5_last_req_entry **val)
{
    krb5_last_req_entry **temp;

    if (val == NULL)
        return;
    for (temp = val; *temp; temp++)
        free(*temp);
    free(val);
}

void
k5_zapfree_pa_data(krb5_pa_data **val)
{
    krb5_pa_data **pa;

    if (val == NULL)
        return;
    for (pa = val; *pa != NULL; pa++) {
        zapfree((*pa)->contents, (*pa)->length);
        zapfree(*pa, sizeof(**pa));
    }
    free(val);
}

void KRB5_CALLCONV
krb5_free_pa_data(krb5_context context, krb5_pa_data **val)
{
    krb5_pa_data **temp;

    if (val == NULL)
        return;
    for (temp = val; *temp; temp++) {
        free((*temp)->contents);
        free(*temp);
    }
    free(val);
}

void KRB5_CALLCONV
krb5_free_principal(krb5_context context, krb5_principal val)
{
    krb5_int32 i;

    if (!val)
        return;

    if (val->data) {
        i = val->length;
        while(--i >= 0)
            free(val->data[i].data);
        free(val->data);
    }
    free(val->realm.data);
    free(val);
}

void KRB5_CALLCONV
krb5_free_priv(krb5_context context, krb5_priv *val)
{
    if (val == NULL)
        return;
    free(val->enc_part.ciphertext.data);
    free(val);
}

void KRB5_CALLCONV
krb5_free_priv_enc_part(krb5_context context, krb5_priv_enc_part *val)
{
    if (val == NULL)
        return;
    free(val->user_data.data);
    krb5_free_address(context, val->r_address);
    krb5_free_address(context, val->s_address);
    free(val);
}

void KRB5_CALLCONV
krb5_free_safe(krb5_context context, krb5_safe *val)
{
    if (val == NULL)
        return;
    free(val->user_data.data);
    krb5_free_address(context, val->r_address);
    krb5_free_address(context, val->s_address);
    krb5_free_checksum(context, val->checksum);
    free(val);
}


void KRB5_CALLCONV
krb5_free_ticket(krb5_context context, krb5_ticket *val)
{
    if (val == NULL)
        return;
    krb5_free_principal(context, val->server);
    free(val->enc_part.ciphertext.data);
    krb5_free_enc_tkt_part(context, val->enc_part2);
    free(val);
}

void KRB5_CALLCONV
krb5_free_tickets(krb5_context context, krb5_ticket **val)
{
    krb5_ticket **temp;

    if (val == NULL)
        return;
    for (temp = val; *temp; temp++)
        krb5_free_ticket(context, *temp);
    free(val);
}


void KRB5_CALLCONV
krb5_free_tgt_creds(krb5_context context, krb5_creds **tgts)
{
    krb5_creds **tgtpp;
    if (tgts == NULL)
        return;
    for (tgtpp = tgts; *tgtpp; tgtpp++)
        krb5_free_creds(context, *tgtpp);
    free(tgts);
}

void KRB5_CALLCONV
krb5_free_tkt_authent(krb5_context context, krb5_tkt_authent *val)
{
    if (val == NULL)
        return;
    krb5_free_ticket(context, val->ticket);
    krb5_free_authenticator(context, val->authenticator);
    free(val);
}

void KRB5_CALLCONV
krb5_free_unparsed_name(krb5_context context, char *val)
{
    if (val != NULL)
        free(val);
}

void KRB5_CALLCONV
krb5_free_string(krb5_context context, char *val)
{
    free(val);
}

void KRB5_CALLCONV
krb5_free_sam_challenge_2(krb5_context ctx, krb5_sam_challenge_2 *sc2)
{
    if (!sc2)
        return;
    krb5_free_sam_challenge_2_contents(ctx, sc2);
    free(sc2);
}

void KRB5_CALLCONV
krb5_free_sam_challenge_2_contents(krb5_context ctx,
                                   krb5_sam_challenge_2 *sc2)
{
    krb5_checksum **cksump;

    if (!sc2)
        return;
    if (sc2->sam_challenge_2_body.data)
        krb5_free_data_contents(ctx, &sc2->sam_challenge_2_body);
    if (sc2->sam_cksum) {
        cksump = sc2->sam_cksum;
        while (*cksump) {
            krb5_free_checksum(ctx, *cksump);
            cksump++;
        }
        free(sc2->sam_cksum);
        sc2->sam_cksum = 0;
    }
}

void KRB5_CALLCONV
krb5_free_sam_challenge_2_body(krb5_context ctx,
                               krb5_sam_challenge_2_body *sc2)
{
    if (!sc2)
        return;
    krb5_free_sam_challenge_2_body_contents(ctx, sc2);
    free(sc2);
}

void KRB5_CALLCONV
krb5_free_sam_challenge_2_body_contents(krb5_context ctx,
                                        krb5_sam_challenge_2_body *sc2)
{
    if (!sc2)
        return;
    if (sc2->sam_type_name.data)
        krb5_free_data_contents(ctx, &sc2->sam_type_name);
    if (sc2->sam_track_id.data)
        krb5_free_data_contents(ctx, &sc2->sam_track_id);
    if (sc2->sam_challenge_label.data)
        krb5_free_data_contents(ctx, &sc2->sam_challenge_label);
    if (sc2->sam_challenge.data)
        krb5_free_data_contents(ctx, &sc2->sam_challenge);
    if (sc2->sam_response_prompt.data)
        krb5_free_data_contents(ctx, &sc2->sam_response_prompt);
    if (sc2->sam_pk_for_sad.data)
        krb5_free_data_contents(ctx, &sc2->sam_pk_for_sad);
}

void KRB5_CALLCONV
krb5_free_sam_response_2(krb5_context ctx, krb5_sam_response_2 *sr2)
{
    if (!sr2)
        return;
    krb5_free_sam_response_2_contents(ctx, sr2);
    free(sr2);
}

void KRB5_CALLCONV
krb5_free_sam_response_2_contents(krb5_context ctx, krb5_sam_response_2 *sr2)
{
    if (!sr2)
        return;
    if (sr2->sam_track_id.data)
        krb5_free_data_contents(ctx, &sr2->sam_track_id);
    if (sr2->sam_enc_nonce_or_sad.ciphertext.data)
        krb5_free_data_contents(ctx, &sr2->sam_enc_nonce_or_sad.ciphertext);
}

void KRB5_CALLCONV
krb5_free_enc_sam_response_enc_2(krb5_context ctx,
                                 krb5_enc_sam_response_enc_2 *esre2)
{
    if (!esre2)
        return;
    krb5_free_enc_sam_response_enc_2_contents(ctx, esre2);
    free(esre2);
}

void KRB5_CALLCONV
krb5_free_enc_sam_response_enc_2_contents(krb5_context ctx,
                                          krb5_enc_sam_response_enc_2 *esre2)
{
    if (!esre2)
        return;
    if (esre2->sam_sad.data)
        krb5_free_data_contents(ctx, &esre2->sam_sad);
}

void KRB5_CALLCONV
krb5_free_pa_enc_ts(krb5_context ctx, krb5_pa_enc_ts *pa_enc_ts)
{
    if (!pa_enc_ts)
        return;
    free(pa_enc_ts);
}

void KRB5_CALLCONV
krb5_free_pa_for_user(krb5_context context, krb5_pa_for_user *req)
{
    if (req == NULL)
        return;
    krb5_free_principal(context, req->user);
    req->user = NULL;
    krb5_free_checksum_contents(context, &req->cksum);
    krb5_free_data_contents(context, &req->auth_package);
    free(req);
}

void KRB5_CALLCONV
krb5_free_s4u_userid_contents(krb5_context context, krb5_s4u_userid *user_id)
{
    if (user_id == NULL)
        return;
    user_id->nonce = 0;
    krb5_free_principal(context, user_id->user);
    user_id->user = NULL;
    krb5_free_data_contents(context, &user_id->subject_cert);
    user_id->subject_cert.length = 0;
    user_id->subject_cert.data = NULL;
    user_id->options = 0;
}

void KRB5_CALLCONV
krb5_free_pa_s4u_x509_user(krb5_context context, krb5_pa_s4u_x509_user *req)
{
    if (req == NULL)
        return;
    krb5_free_s4u_userid_contents(context, &req->user_id);
    krb5_free_checksum_contents(context, &req->cksum);
    free(req);
}

void KRB5_CALLCONV
krb5_free_pa_pac_req(krb5_context context,
                     krb5_pa_pac_req *req)
{
    free(req);
}

void KRB5_CALLCONV
krb5_free_fast_req(krb5_context context, krb5_fast_req *val)
{
    if (val == NULL)
        return;
    krb5_free_kdc_req(context, val->req_body);
    free(val);
}

void KRB5_CALLCONV
krb5_free_fast_armor(krb5_context context, krb5_fast_armor *val)
{
    if (val == NULL)
        return;
    krb5_free_data_contents(context, &val->armor_value);
    free(val);
}

void KRB5_CALLCONV
krb5_free_fast_response(krb5_context context, krb5_fast_response *val)
{
    if (!val)
        return;
    krb5_free_pa_data(context, val->padata);
    krb5_free_fast_finished(context, val->finished);
    krb5_free_keyblock(context, val->strengthen_key);
    free(val);
}

void KRB5_CALLCONV
krb5_free_fast_finished(krb5_context context, krb5_fast_finished *val)
{
    if (!val)
        return;
    krb5_free_principal(context, val->client);
    krb5_free_checksum_contents(context, &val->ticket_checksum);
    free(val);
}

void KRB5_CALLCONV
krb5_free_fast_armored_req(krb5_context context, krb5_fast_armored_req *val)
{
    if (val == NULL)
        return;
    if (val->armor)
        krb5_free_fast_armor(context, val->armor);
    krb5_free_data_contents(context, &val->enc_part.ciphertext);
    if (val->req_checksum.contents)
        krb5_free_checksum_contents(context, &val->req_checksum);
    free(val);
}

void
k5_free_data_ptr_list(krb5_data **list)
{
    int i;

    for (i = 0; list != NULL && list[i] != NULL; i++)
        krb5_free_data(NULL, list[i]);
    free(list);
}

void KRB5_CALLCONV
krb5int_free_data_list(krb5_context context, krb5_data *data)
{
    int i;

    if (data == NULL)
        return;

    for (i = 0; data[i].data != NULL; i++)
        free(data[i].data);

    free(data);
}

void KRB5_CALLCONV
krb5_free_ad_kdcissued(krb5_context context, krb5_ad_kdcissued *val)
{
    if (val == NULL)
        return;

    krb5_free_checksum_contents(context, &val->ad_checksum);
    krb5_free_principal(context, val->i_principal);
    krb5_free_authdata(context, val->elements);
    free(val);
}

void KRB5_CALLCONV
krb5_free_iakerb_header(krb5_context context, krb5_iakerb_header *val)
{
    if (val == NULL)
        return ;

    krb5_free_data_contents(context, &val->target_realm);
    krb5_free_data(context, val->cookie);
    free(val);
}

void KRB5_CALLCONV
krb5_free_iakerb_finished(krb5_context context, krb5_iakerb_finished *val)
{
    if (val == NULL)
        return ;

    krb5_free_checksum_contents(context, &val->checksum);
    free(val);
}

void
k5_free_algorithm_identifier(krb5_context context,
                             krb5_algorithm_identifier *val)
{
    if (val == NULL)
        return;
    free(val->algorithm.data);
    free(val->parameters.data);
    free(val);
}

void
k5_free_otp_tokeninfo(krb5_context context, krb5_otp_tokeninfo *val)
{
    krb5_algorithm_identifier **alg;

    if (val == NULL)
        return;
    free(val->vendor.data);
    free(val->challenge.data);
    free(val->token_id.data);
    free(val->alg_id.data);
    for (alg = val->supported_hash_alg; alg != NULL && *alg != NULL; alg++)
        k5_free_algorithm_identifier(context, *alg);
    free(val->supported_hash_alg);
    free(val);
}

void
k5_free_pa_otp_challenge(krb5_context context, krb5_pa_otp_challenge *val)
{
    krb5_otp_tokeninfo **ti;

    if (val == NULL)
        return;
    free(val->nonce.data);
    free(val->service.data);
    for (ti = val->tokeninfo; *ti != NULL; ti++)
        k5_free_otp_tokeninfo(context, *ti);
    free(val->tokeninfo);
    free(val->salt.data);
    free(val->s2kparams.data);
    free(val);
}

void
k5_free_pa_otp_req(krb5_context context, krb5_pa_otp_req *val)
{
    if (val == NULL)
        return;
    val->flags = 0;
    free(val->nonce.data);
    free(val->enc_data.ciphertext.data);
    if (val->hash_alg != NULL)
        k5_free_algorithm_identifier(context, val->hash_alg);
    free(val->otp_value.data);
    free(val->pin.data);
    free(val->challenge.data);
    free(val->counter.data);
    free(val->token_id.data);
    free(val->alg_id.data);
    free(val->vendor.data);
    free(val);
}

void
k5_free_kkdcp_message(krb5_context context, krb5_kkdcp_message *val)
{
    if (val == NULL)
        return;
    free(val->target_domain.data);
    free(val->kerb_message.data);
    free(val);
}

static void
free_vmac(krb5_context context, krb5_verifier_mac *val)
{
    if (val == NULL)
        return;
    krb5_free_principal(context, val->princ);
    krb5_free_checksum_contents(context, &val->checksum);
    free(val);
}

void
k5_free_cammac(krb5_context context, krb5_cammac *val)
{
    krb5_verifier_mac **vp;

    if (val == NULL)
        return;
    krb5_free_authdata(context, val->elements);
    free_vmac(context, val->kdc_verifier);
    free_vmac(context, val->svc_verifier);
    for (vp = val->other_verifiers; vp != NULL && *vp != NULL; vp++)
        free_vmac(context, *vp);
    free(val->other_verifiers);
    free(val);
}

void
k5_free_secure_cookie(krb5_context context, krb5_secure_cookie *val)
{
    if (val == NULL)
        return;
    k5_zapfree_pa_data(val->data);
    free(val);
}

void
k5_free_spake_factor(krb5_context context, krb5_spake_factor *val)
{
    if (val == NULL)
        return;
    if (val->data != NULL)
        zapfree(val->data->data, val->data->length);
    free(val->data);
    free(val);
}

void
k5_free_pa_spake(krb5_context context, krb5_pa_spake *val)
{
    krb5_spake_factor **f;

    if (val == NULL)
        return;
    switch (val->choice) {
    case SPAKE_MSGTYPE_SUPPORT:
        free(val->u.support.groups);
        break;
    case SPAKE_MSGTYPE_CHALLENGE:
        krb5_free_data_contents(context, &val->u.challenge.pubkey);
        for (f = val->u.challenge.factors; f != NULL && *f != NULL; f++)
            k5_free_spake_factor(context, *f);
        free(val->u.challenge.factors);
        break;
    case SPAKE_MSGTYPE_RESPONSE:
        krb5_free_data_contents(context, &val->u.response.pubkey);
        krb5_free_data_contents(context, &val->u.response.factor.ciphertext);
        break;
    case SPAKE_MSGTYPE_ENCDATA:
        krb5_free_data_contents(context, &val->u.encdata.ciphertext);
        break;
    default:
        break;
    }
    free(val);
}
