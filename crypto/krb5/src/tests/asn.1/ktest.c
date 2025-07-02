/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/ktest.c */
/*
 * Copyright (C) 1994 by the Massachusetts Institute of Technology.
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

#include "ktest.h"
#include "utility.h"
#include <stdlib.h>

char *sample_principal_name = "hftsai/extra@ATHENA.MIT.EDU";

void
ktest_make_sample_authenticator(krb5_authenticator *a)
{
    ktest_make_sample_principal(&a->client);
    a->checksum = ealloc(sizeof(krb5_checksum));
    ktest_make_sample_checksum(a->checksum);
    a->cusec = SAMPLE_USEC;
    a->ctime = SAMPLE_TIME;
    a->subkey = ealloc(sizeof(krb5_keyblock));
    ktest_make_sample_keyblock(a->subkey);
    a->seq_number = SAMPLE_SEQ_NUMBER;
    ktest_make_sample_authorization_data(&a->authorization_data);
}

void
ktest_make_sample_principal(krb5_principal *p)
{
    if (krb5_parse_name(test_context, sample_principal_name, p))
        abort();
}

void
ktest_make_sample_checksum(krb5_checksum *cs)
{
    cs->checksum_type = 1;
    cs->length = 4;
    cs->contents = ealloc(4);
    memcpy(cs->contents,"1234",4);
}

void
ktest_make_sample_keyblock(krb5_keyblock *kb)
{
    kb->magic = KV5M_KEYBLOCK;
    kb->enctype = 1;
    kb->length = 8;
    kb->contents = ealloc(8);
    memcpy(kb->contents,"12345678",8);
}

void
ktest_make_sample_ticket(krb5_ticket *tkt)
{
    ktest_make_sample_principal(&tkt->server);
    ktest_make_sample_enc_data(&tkt->enc_part);
    tkt->enc_part2 = NULL;
}

void
ktest_make_sample_enc_data(krb5_enc_data *ed)
{
    ed->kvno = 5;
    ed->enctype = 0;
    krb5_data_parse(&ed->ciphertext, "krbASN.1 test message");
}

void
ktest_make_sample_enc_tkt_part(krb5_enc_tkt_part *etp)
{
    etp->flags = SAMPLE_FLAGS;
    etp->session = ealloc(sizeof(krb5_keyblock));
    ktest_make_sample_keyblock(etp->session);
    ktest_make_sample_principal(&etp->client);
    ktest_make_sample_transited(&etp->transited);
    ktest_make_sample_ticket_times(&etp->times);
    ktest_make_sample_addresses(&etp->caddrs);
    ktest_make_sample_authorization_data(&etp->authorization_data);
}

void
ktest_make_sample_addresses(krb5_address ***caddrs)
{
    int i;

    *caddrs = ealloc(3 * sizeof(krb5_address *));
    for (i = 0; i < 2; i++) {
        (*caddrs)[i] = ealloc(sizeof(krb5_address));
        ktest_make_sample_address((*caddrs)[i]);
    }
    (*caddrs)[2] = NULL;
}

void
ktest_make_sample_authorization_data(krb5_authdata ***ad)
{
    int i;

    *ad = ealloc(3 * sizeof(krb5_authdata *));
    for (i = 0; i <= 1; i++) {
        (*ad)[i] = ealloc(sizeof(krb5_authdata));
        ktest_make_sample_authdata((*ad)[i]);
    }
    (*ad)[2] = NULL;
}

void
ktest_make_sample_transited(krb5_transited *t)
{
    t->tr_type = 1;
    krb5_data_parse(&t->tr_contents, "EDU,MIT.,ATHENA.,WASHINGTON.EDU,CS.");
}

void
ktest_make_sample_ticket_times(krb5_ticket_times *tt)
{
    tt->authtime = SAMPLE_TIME;
    tt->starttime = SAMPLE_TIME;
    tt->endtime = SAMPLE_TIME;
    tt->renew_till = SAMPLE_TIME;
}

void
ktest_make_sample_address(krb5_address *a)
{
    a->addrtype = ADDRTYPE_INET;
    a->length = 4;
    a->contents = ealloc(4 * sizeof(krb5_octet));
    a->contents[0] = 18;
    a->contents[1] = 208;
    a->contents[2] = 0;
    a->contents[3] = 35;
}

void
ktest_make_sample_authdata(krb5_authdata *ad)
{
    ad->ad_type = 1;
    ad->length = 6;
    ad->contents = ealloc(6 * sizeof(krb5_octet));
    memcpy(ad->contents, "foobar", 6);
}

void
ktest_make_sample_enc_kdc_rep_part(krb5_enc_kdc_rep_part *ekr)
{
    ekr->session = ealloc(sizeof(krb5_keyblock));
    ktest_make_sample_keyblock(ekr->session);
    ktest_make_sample_last_req(&ekr->last_req);
    ekr->nonce = SAMPLE_NONCE;
    ekr->key_exp = SAMPLE_TIME;
    ekr->flags = SAMPLE_FLAGS;
    ekr->times.authtime = SAMPLE_TIME;
    ekr->times.starttime = SAMPLE_TIME;
    ekr->times.endtime = SAMPLE_TIME;
    ekr->times.renew_till = SAMPLE_TIME;
    ktest_make_sample_principal(&ekr->server);
    ktest_make_sample_addresses(&ekr->caddrs);
}

void
ktest_make_sample_last_req(krb5_last_req_entry ***lr)
{
    int i;

    *lr = ealloc(3 * sizeof(krb5_last_req_entry *));
    for (i = 0; i <= 1; i++)
        ktest_make_sample_last_req_entry(&(*lr)[i]);
    (*lr)[2] = NULL;
}

void
ktest_make_sample_last_req_entry(krb5_last_req_entry **lre)
{
    *lre = ealloc(sizeof(krb5_last_req_entry));
    (*lre)->lr_type = -5;
    (*lre)->value = SAMPLE_TIME;
}

void
ktest_make_sample_kdc_rep(krb5_kdc_rep *kdcr)
{
    ktest_make_sample_pa_data_array(&kdcr->padata);
    ktest_make_sample_principal(&kdcr->client);
    kdcr->ticket = ealloc(sizeof(krb5_ticket));
    ktest_make_sample_ticket(kdcr->ticket);
    ktest_make_sample_enc_data(&kdcr->enc_part);
    kdcr->enc_part2 = NULL;
}

void
ktest_make_sample_pa_data_array(krb5_pa_data ***pad)
{
    int i;

    *pad = ealloc(3 * sizeof(krb5_pa_data *));
    for (i = 0; i <= 1; i++) {
        (*pad)[i] = ealloc(sizeof(krb5_pa_data));
        ktest_make_sample_pa_data((*pad)[i]);
    }
    (*pad)[2] = NULL;
}

void
ktest_make_sample_empty_pa_data_array(krb5_pa_data ***pad)
{
    *pad = ealloc(sizeof(krb5_pa_data *));
    (*pad)[0] = NULL;
}

void
ktest_make_sample_pa_data(krb5_pa_data *pad)
{
    pad->pa_type = 13;
    pad->length = 7;
    pad->contents = ealloc(7);
    memcpy(pad->contents, "pa-data", 7);
}

void
ktest_make_sample_ap_req(krb5_ap_req *ar)
{
    ar->ap_options = SAMPLE_FLAGS;
    ar->ticket = ealloc(sizeof(krb5_ticket));
    ktest_make_sample_ticket(ar->ticket);
    ktest_make_sample_enc_data(&(ar->authenticator));
}

void
ktest_make_sample_ap_rep(krb5_ap_rep *ar)
{
    ktest_make_sample_enc_data(&ar->enc_part);
}

void
ktest_make_sample_ap_rep_enc_part(krb5_ap_rep_enc_part *arep)
{
    arep->ctime = SAMPLE_TIME;
    arep->cusec = SAMPLE_USEC;
    arep->subkey = ealloc(sizeof(krb5_keyblock));
    ktest_make_sample_keyblock(arep->subkey);
    arep->seq_number = SAMPLE_SEQ_NUMBER;
}

void
ktest_make_sample_kdc_req(krb5_kdc_req *kr)
{
    /* msg_type is left up to the calling procedure */
    ktest_make_sample_pa_data_array(&kr->padata);
    kr->kdc_options = SAMPLE_FLAGS;
    ktest_make_sample_principal(&(kr->client));
    ktest_make_sample_principal(&(kr->server));
    kr->from = SAMPLE_TIME;
    kr->till = SAMPLE_TIME;
    kr->rtime = SAMPLE_TIME;
    kr->nonce = SAMPLE_NONCE;
    kr->nktypes = 2;
    kr->ktype = ealloc(2 * sizeof(krb5_enctype));
    kr->ktype[0] = 0;
    kr->ktype[1] = 1;
    ktest_make_sample_addresses(&kr->addresses);
    ktest_make_sample_enc_data(&kr->authorization_data);
    ktest_make_sample_authorization_data(&kr->unenc_authdata);
    ktest_make_sample_sequence_of_ticket(&kr->second_ticket);
}

void
ktest_make_sample_kdc_req_body(krb5_kdc_req *krb)
{
    krb->kdc_options = SAMPLE_FLAGS;
    ktest_make_sample_principal(&krb->client);
    ktest_make_sample_principal(&krb->server);
    krb->from = SAMPLE_TIME;
    krb->till = SAMPLE_TIME;
    krb->rtime = SAMPLE_TIME;
    krb->nonce = SAMPLE_NONCE;
    krb->nktypes = 2;
    krb->ktype = (krb5_enctype*)calloc(2,sizeof(krb5_enctype));
    krb->ktype[0] = 0;
    krb->ktype[1] = 1;
    ktest_make_sample_addresses(&krb->addresses);
    ktest_make_sample_enc_data(&krb->authorization_data);
    ktest_make_sample_authorization_data(&krb->unenc_authdata);
    ktest_make_sample_sequence_of_ticket(&krb->second_ticket);
}

void
ktest_make_sample_safe(krb5_safe *s)
{
    ktest_make_sample_data(&s->user_data);
    s->timestamp = SAMPLE_TIME;
    s->usec = SAMPLE_USEC;
    s->seq_number = SAMPLE_SEQ_NUMBER;
    s->s_address = ealloc(sizeof(krb5_address));
    ktest_make_sample_address(s->s_address);
    s->r_address = ealloc(sizeof(krb5_address));
    ktest_make_sample_address(s->r_address);
    s->checksum = ealloc(sizeof(krb5_checksum));
    ktest_make_sample_checksum(s->checksum);
}

void
ktest_make_sample_priv(krb5_priv *p)
{
    ktest_make_sample_enc_data(&p->enc_part);
}

void
ktest_make_sample_priv_enc_part(krb5_priv_enc_part *pep)
{
    ktest_make_sample_data(&(pep->user_data));
    pep->timestamp = SAMPLE_TIME;
    pep->usec = SAMPLE_USEC;
    pep->seq_number = SAMPLE_SEQ_NUMBER;
    pep->s_address = ealloc(sizeof(krb5_address));
    ktest_make_sample_address(pep->s_address);
    pep->r_address = ealloc(sizeof(krb5_address));
    ktest_make_sample_address(pep->r_address);
}

void
ktest_make_sample_cred(krb5_cred *c)
{
    ktest_make_sample_sequence_of_ticket(&c->tickets);
    ktest_make_sample_enc_data(&c->enc_part);
}

void
ktest_make_sample_sequence_of_ticket(krb5_ticket ***sot)
{
    int i;

    *sot = ealloc(3 * sizeof(krb5_ticket *));
    for (i = 0; i < 2; i++) {
        (*sot)[i] = ealloc(sizeof(krb5_ticket));
        ktest_make_sample_ticket((*sot)[i]);
    }
    (*sot)[2] = NULL;
}

void
ktest_make_sample_cred_enc_part(krb5_cred_enc_part *cep)
{
    cep->nonce = SAMPLE_NONCE;
    cep->timestamp = SAMPLE_TIME;
    cep->usec = SAMPLE_USEC;
    cep->s_address = ealloc(sizeof(krb5_address));
    ktest_make_sample_address(cep->s_address);
    cep->r_address = ealloc(sizeof(krb5_address));
    ktest_make_sample_address(cep->r_address);
    ktest_make_sequence_of_cred_info(&cep->ticket_info);
}

void
ktest_make_sequence_of_cred_info(krb5_cred_info ***soci)
{
    int i;

    *soci = ealloc(3 * sizeof(krb5_cred_info *));
    for (i = 0; i < 2; i++) {
        (*soci)[i] = ealloc(sizeof(krb5_cred_info));
        ktest_make_sample_cred_info((*soci)[i]);
    }
    (*soci)[2] = NULL;
}

void
ktest_make_sample_cred_info(krb5_cred_info *ci)
{
    ci->session = ealloc(sizeof(krb5_keyblock));
    ktest_make_sample_keyblock(ci->session);
    ktest_make_sample_principal(&ci->client);
    ktest_make_sample_principal(&ci->server);
    ci->flags = SAMPLE_FLAGS;
    ci->times.authtime = SAMPLE_TIME;
    ci->times.starttime = SAMPLE_TIME;
    ci->times.endtime = SAMPLE_TIME;
    ci->times.renew_till = SAMPLE_TIME;
    ktest_make_sample_addresses(&ci->caddrs);
}

void
ktest_make_sample_error(krb5_error *kerr)
{
    kerr->ctime = SAMPLE_TIME;
    kerr->cusec = SAMPLE_USEC;
    kerr->susec = SAMPLE_USEC;
    kerr->stime = SAMPLE_TIME;
    kerr->error = SAMPLE_ERROR;
    ktest_make_sample_principal(&kerr->client);
    ktest_make_sample_principal(&kerr->server);
    ktest_make_sample_data(&kerr->text);
    ktest_make_sample_data(&kerr->e_data);
}

void
ktest_make_sample_data(krb5_data *d)
{
    krb5_data_parse(d, "krb5data");
}

void
ktest_make_sample_etype_info(krb5_etype_info_entry ***p)
{
    krb5_etype_info_entry **info;
    int i, len;
    char *str;

    info = ealloc(4 * sizeof(krb5_etype_info_entry *));
    for (i = 0; i < 3; i++) {
        info[i] = ealloc(sizeof(krb5_etype_info_entry));
        info[i]->etype = i;
        len = asprintf(&str, "Morton's #%d", i);
        if (len < 0)
            abort();
        info[i]->salt = (krb5_octet *)str;
        info[i]->length = len;
        info[i]->s2kparams.data = NULL;
        info[i]->s2kparams.length = 0;
        info[i]->magic = KV5M_ETYPE_INFO_ENTRY;
    }
    free(info[1]->salt);
    info[1]->length = KRB5_ETYPE_NO_SALT;
    info[1]->salt = 0;
    *p = info;
}


void
ktest_make_sample_etype_info2(krb5_etype_info_entry ***p)
{
    krb5_etype_info_entry **info;
    int i, len;
    char *str;

    info = ealloc(4 * sizeof(krb5_etype_info_entry *));
    for (i = 0; i < 3; i++) {
        info[i] = ealloc(sizeof(krb5_etype_info_entry));
        info[i]->etype = i;
        len = asprintf(&str, "Morton's #%d", i);
        if (len < 0)
            abort();
        info[i]->salt = (krb5_octet *)str;
        info[i]->length = (unsigned int)len;
        len = asprintf(&info[i]->s2kparams.data, "s2k: %d", i);
        if (len < 0)
            abort();
        info[i]->s2kparams.length = (unsigned int) len;
        info[i]->magic = KV5M_ETYPE_INFO_ENTRY;
    }
    free(info[1]->salt);
    info[1]->length = KRB5_ETYPE_NO_SALT;
    info[1]->salt = 0;
    *p = info;
}


void
ktest_make_sample_pa_enc_ts(krb5_pa_enc_ts *pa_enc)
{
    pa_enc->patimestamp = SAMPLE_TIME;
    pa_enc->pausec = SAMPLE_USEC;
}

void
ktest_make_sample_sam_challenge_2(krb5_sam_challenge_2 *p)
{
    /* Need a valid DER sequence encoding here; this one contains the OCTET
     * STRING "challenge". */
    krb5_data_parse(&p->sam_challenge_2_body, "\x30\x0B\x04\x09" "challenge");
    p->sam_cksum = ealloc(2 * sizeof(krb5_checksum *));
    p->sam_cksum[0] = ealloc(sizeof(krb5_checksum));
    ktest_make_sample_checksum(p->sam_cksum[0]);
    p->sam_cksum[1] = NULL;
}

void
ktest_make_sample_sam_challenge_2_body(krb5_sam_challenge_2_body *p)
{
    p->sam_type = 42;
    p->sam_flags = KRB5_SAM_USE_SAD_AS_KEY;
    krb5_data_parse(&p->sam_type_name, "type name");
    p->sam_track_id = empty_data();
    krb5_data_parse(&p->sam_challenge_label, "challenge label");
    krb5_data_parse(&p->sam_challenge, "challenge ipse");
    krb5_data_parse(&p->sam_response_prompt, "response_prompt ipse");
    p->sam_pk_for_sad = empty_data();
    p->sam_nonce = 0x543210;
    p->sam_etype = ENCTYPE_AES256_CTS_HMAC_SHA384_192;
}

void
ktest_make_sample_sam_response_2(krb5_sam_response_2 *p)
{
    p->magic = KV5M_SAM_RESPONSE;
    p->sam_type = 43; /* information */
    p->sam_flags = KRB5_SAM_USE_SAD_AS_KEY; /* KRB5_SAM_* values */
    krb5_data_parse(&p->sam_track_id, "track data");
    krb5_data_parse(&p->sam_enc_nonce_or_sad.ciphertext, "nonce or sad");
    p->sam_enc_nonce_or_sad.enctype = ENCTYPE_AES256_CTS_HMAC_SHA384_192;
    p->sam_enc_nonce_or_sad.kvno = 3382;
    p->sam_nonce = 0x543210;
}

void
ktest_make_sample_enc_sam_response_enc_2(krb5_enc_sam_response_enc_2 *p)
{
    p->magic = 83;
    p->sam_nonce = 88;
    krb5_data_parse(&p->sam_sad, "enc_sam_response_enc_2");
}

void
ktest_make_sample_pa_for_user(krb5_pa_for_user *p)
{
    ktest_make_sample_principal(&p->user);
    ktest_make_sample_checksum(&p->cksum);
    ktest_make_sample_data(&p->auth_package);
}

void
ktest_make_sample_pa_s4u_x509_user(krb5_pa_s4u_x509_user *p)
{
    krb5_s4u_userid *u = &p->user_id;

    u->nonce = 13243546;
    ktest_make_sample_principal(&u->user);
    krb5_data_parse(&u->subject_cert, "pa_s4u_x509_user");
    u->options = 0x80000000;
    ktest_make_sample_checksum(&p->cksum);
}

void
ktest_make_sample_ad_kdcissued(krb5_ad_kdcissued *p)
{
    ktest_make_sample_checksum(&p->ad_checksum);
    ktest_make_sample_principal(&p->i_principal);
    ktest_make_sample_authorization_data(&p->elements);
}

void
ktest_make_sample_iakerb_header(krb5_iakerb_header *ih)
{
    ktest_make_sample_data(&(ih->target_realm));
    ih->cookie = ealloc(sizeof(krb5_data));
    ktest_make_sample_data(ih->cookie);
}

void
ktest_make_sample_iakerb_finished(krb5_iakerb_finished *ih)
{
    ktest_make_sample_checksum(&ih->checksum);
}

static void
ktest_make_sample_fast_finished(krb5_fast_finished *p)
{
    p->timestamp = SAMPLE_TIME;
    p->usec = SAMPLE_USEC;
    ktest_make_sample_principal(&p->client);
    ktest_make_sample_checksum(&p->ticket_checksum);
}

void
ktest_make_sample_fast_response(krb5_fast_response *p)
{
    ktest_make_sample_pa_data_array(&p->padata);
    p->strengthen_key = ealloc(sizeof(krb5_keyblock));
    ktest_make_sample_keyblock(p->strengthen_key);
    p->finished = ealloc(sizeof(krb5_fast_finished));
    ktest_make_sample_fast_finished(p->finished);
    p->nonce = SAMPLE_NONCE;
}

void
ktest_make_sha256_alg(krb5_algorithm_identifier *p)
{
    /* { 2 16 840 1 101 3 4 2 1 } */
    krb5_data_parse(&p->algorithm, "\x60\x86\x48\x01\x65\x03\x04\x02\x01");
    p->parameters = empty_data();
}

void
ktest_make_sha1_alg(krb5_algorithm_identifier *p)
{
    /* { 1 3 14 3 2 26 } */
    krb5_data_parse(&p->algorithm, "\x2b\x0e\x03\x02\x1a");
    p->parameters = empty_data();
}

void
ktest_make_minimal_otp_tokeninfo(krb5_otp_tokeninfo *p)
{
    memset(p, 0, sizeof(*p));
    p->length = p->format = p->iteration_count = -1;
}

void
ktest_make_maximal_otp_tokeninfo(krb5_otp_tokeninfo *p)
{
    p->flags = KRB5_OTP_FLAG_NEXTOTP | KRB5_OTP_FLAG_COMBINE |
        KRB5_OTP_FLAG_COLLECT_PIN | KRB5_OTP_FLAG_ENCRYPT_NONCE |
        KRB5_OTP_FLAG_SEPARATE_PIN | KRB5_OTP_FLAG_CHECK_DIGIT;
    krb5_data_parse(&p->vendor, "Examplecorp");
    krb5_data_parse(&p->challenge, "hark!");
    p->length = 10;
    p->format = 2;
    krb5_data_parse(&p->token_id, "yourtoken");
    krb5_data_parse(&p->alg_id, "urn:ietf:params:xml:ns:keyprov:pskc:hotp");
    p->supported_hash_alg = ealloc(3 * sizeof(*p->supported_hash_alg));
    p->supported_hash_alg[0] = ealloc(sizeof(*p->supported_hash_alg[0]));
    ktest_make_sha256_alg(p->supported_hash_alg[0]);
    p->supported_hash_alg[1] = ealloc(sizeof(*p->supported_hash_alg[1]));
    ktest_make_sha1_alg(p->supported_hash_alg[1]);
    p->supported_hash_alg[2] = NULL;
    p->iteration_count = 1000;
}

void
ktest_make_minimal_pa_otp_challenge(krb5_pa_otp_challenge *p)
{
    memset(p, 0, sizeof(*p));
    krb5_data_parse(&p->nonce, "minnonce");
    p->tokeninfo = ealloc(2 * sizeof(*p->tokeninfo));
    p->tokeninfo[0] = ealloc(sizeof(*p->tokeninfo[0]));
    ktest_make_minimal_otp_tokeninfo(p->tokeninfo[0]);
    p->tokeninfo[1] = NULL;
}

void
ktest_make_maximal_pa_otp_challenge(krb5_pa_otp_challenge *p)
{
    krb5_data_parse(&p->nonce, "maxnonce");
    krb5_data_parse(&p->service, "testservice");
    p->tokeninfo = ealloc(3 * sizeof(*p->tokeninfo));
    p->tokeninfo[0] = ealloc(sizeof(*p->tokeninfo[0]));
    ktest_make_minimal_otp_tokeninfo(p->tokeninfo[0]);
    p->tokeninfo[1] = ealloc(sizeof(*p->tokeninfo[1]));
    ktest_make_maximal_otp_tokeninfo(p->tokeninfo[1]);
    p->tokeninfo[2] = NULL;
    krb5_data_parse(&p->salt, "keysalt");
    krb5_data_parse(&p->s2kparams, "1234");
}

void
ktest_make_minimal_pa_otp_req(krb5_pa_otp_req *p)
{
    memset(p, 0, sizeof(*p));
    p->iteration_count = -1;
    p->format = -1;
    ktest_make_sample_enc_data(&p->enc_data);
}

void
ktest_make_maximal_pa_otp_req(krb5_pa_otp_req *p)
{
    p->flags = KRB5_OTP_FLAG_NEXTOTP | KRB5_OTP_FLAG_COMBINE;
    krb5_data_parse(&p->nonce, "nonce");
    ktest_make_sample_enc_data(&p->enc_data);
    p->hash_alg = ealloc(sizeof(*p->hash_alg));
    ktest_make_sha256_alg(p->hash_alg);
    p->iteration_count = 1000;
    krb5_data_parse(&p->otp_value, "frogs");
    krb5_data_parse(&p->pin, "myfirstpin");
    krb5_data_parse(&p->challenge, "hark!");
    p->time = SAMPLE_TIME;
    krb5_data_parse(&p->counter, "346");
    p->format = 2;
    krb5_data_parse(&p->token_id, "yourtoken");
    krb5_data_parse(&p->alg_id, "urn:ietf:params:xml:ns:keyprov:pskc:hotp");
    krb5_data_parse(&p->vendor, "Examplecorp");
}

#ifndef DISABLE_PKINIT

static void
ktest_make_sample_pk_authenticator(krb5_pk_authenticator *p)
{
    p->cusec = SAMPLE_USEC;
    p->ctime = SAMPLE_TIME;
    p->nonce = SAMPLE_NONCE;
    ktest_make_sample_checksum(&p->paChecksum);
    /* We don't encode the checksum type, only the contents. */
    p->paChecksum.checksum_type = 0;
    p->freshnessToken = ealloc(sizeof(krb5_data));
    ktest_make_sample_data(p->freshnessToken);
}

static void
ktest_make_sample_oid(krb5_data *p)
{
    krb5_data_parse(p, "\052\206\110\206\367\022\001\002\002");
}

static void
ktest_make_sample_algorithm_identifier(krb5_algorithm_identifier *p)
{
    ktest_make_sample_oid(&p->algorithm);
    /* Need a valid DER encoding here; this is the OCTET STRING "params". */
    krb5_data_parse(&p->parameters, "\x04\x06" "params");
}

static void
ktest_make_sample_algorithm_identifier_no_params(krb5_algorithm_identifier *p)
{
    ktest_make_sample_oid(&p->algorithm);
    p->parameters = empty_data();
}

static void
ktest_make_sample_external_principal_identifier(
    krb5_external_principal_identifier *p)
{
    ktest_make_sample_data(&p->subjectName);
    ktest_make_sample_data(&p->issuerAndSerialNumber);
    ktest_make_sample_data(&p->subjectKeyIdentifier);
}

void
ktest_make_sample_pa_pk_as_req(krb5_pa_pk_as_req *p)
{
    ktest_make_sample_data(&p->signedAuthPack);
    p->trustedCertifiers =
        ealloc(2 * sizeof(krb5_external_principal_identifier *));
    p->trustedCertifiers[0] =
        ealloc(sizeof(krb5_external_principal_identifier));
    ktest_make_sample_external_principal_identifier(p->trustedCertifiers[0]);
    p->trustedCertifiers[1] = NULL;
    ktest_make_sample_data(&p->kdcPkId);
}

static void
ktest_make_sample_dh_rep_info(krb5_dh_rep_info *p)
{
    ktest_make_sample_data(&p->dhSignedData);
    ktest_make_sample_data(&p->serverDHNonce);
    p->kdfID = ealloc(sizeof(krb5_data));
    ktest_make_sample_data(p->kdfID);
}

void
ktest_make_sample_pa_pk_as_rep_dhInfo(krb5_pa_pk_as_rep *p)
{
    p->choice = choice_pa_pk_as_rep_dhInfo;
    ktest_make_sample_dh_rep_info(&p->u.dh_Info);
}

void
ktest_make_sample_pa_pk_as_rep_encKeyPack(krb5_pa_pk_as_rep *p)
{
    p->choice = choice_pa_pk_as_rep_encKeyPack;
    ktest_make_sample_data(&p->u.encKeyPack);
}

void
ktest_make_sample_auth_pack(krb5_auth_pack *p)
{
    ktest_make_sample_pk_authenticator(&p->pkAuthenticator);
    /* Need a valid DER encoding here; this is the OCTET STRING "pvalue". */
    krb5_data_parse(&p->clientPublicValue, "\x04\x06" "pvalue");
    p->supportedCMSTypes = ealloc(3 * sizeof(krb5_algorithm_identifier *));
    p->supportedCMSTypes[0] = ealloc(sizeof(krb5_algorithm_identifier));
    ktest_make_sample_algorithm_identifier(p->supportedCMSTypes[0]);
    p->supportedCMSTypes[1] = ealloc(sizeof(krb5_algorithm_identifier));
    ktest_make_sample_algorithm_identifier_no_params(p->supportedCMSTypes[1]);
    p->supportedCMSTypes[2] = NULL;
    ktest_make_sample_data(&p->clientDHNonce);
    p->supportedKDFs = ealloc(2 * sizeof(krb5_data *));
    p->supportedKDFs[0] = ealloc(sizeof(krb5_data));
    ktest_make_sample_data(p->supportedKDFs[0]);
    p->supportedKDFs[1] = NULL;
}

void
ktest_make_sample_kdc_dh_key_info(krb5_kdc_dh_key_info *p)
{
    ktest_make_sample_data(&p->subjectPublicKey);
    p->nonce = SAMPLE_NONCE;
    p->dhKeyExpiration = SAMPLE_TIME;
}

void
ktest_make_sample_reply_key_pack(krb5_reply_key_pack *p)
{
    ktest_make_sample_keyblock(&p->replyKey);
    ktest_make_sample_checksum(&p->asChecksum);
}

void
ktest_make_sample_sp80056a_other_info(krb5_sp80056a_other_info *p)
{
    ktest_make_sample_algorithm_identifier_no_params(&p->algorithm_identifier);
    ktest_make_sample_principal(&p->party_u_info);
    ktest_make_sample_principal(&p->party_v_info);
    ktest_make_sample_data(&p->supp_pub_info);
}

void
ktest_make_sample_pkinit_supp_pub_info(krb5_pkinit_supp_pub_info *p)
{
    p->enctype = ENCTYPE_AES256_CTS_HMAC_SHA384_192;
    ktest_make_sample_data(&p->as_req);
    ktest_make_sample_data(&p->pk_as_rep);
}

#endif /* not DISABLE_PKINIT */

#ifdef ENABLE_LDAP
static void
ktest_make_sample_key_data(krb5_key_data *p, int i)
{
    char *str;
    int len;

    len = asprintf(&str, "key%d", i);
    if (len < 0)
        abort();
    p->key_data_ver = 2;
    p->key_data_type[0] = 2;
    p->key_data_length[0] = (unsigned int) len;
    p->key_data_contents[0] = (krb5_octet *)str;
    len = asprintf(&str, "salt%d", i);
    if (len < 0)
        abort();
    p->key_data_type[1] = i;
    p->key_data_length[1] = (unsigned int) len;
    p->key_data_contents[1] = (krb5_octet *)str;
}

void
ktest_make_sample_ldap_seqof_key_data(ldap_seqof_key_data *p)
{
    int i;

    p->mkvno = 14;
    p->n_key_data = 3;
    p->key_data = calloc(3,sizeof(krb5_key_data));
    p->kvno = 42;
    for (i = 0; i < 3; i++)
        ktest_make_sample_key_data(&p->key_data[i], i);
}
#endif

void
ktest_make_sample_kkdcp_message(krb5_kkdcp_message *p)
{
    krb5_kdc_req req;
    krb5_data *message;

    ktest_make_sample_kdc_req(&req);
    req.msg_type = KRB5_AS_REQ;
    encode_krb5_as_req(&req, &message);
    p->kerb_message = *message;
    free(message);
    ktest_empty_kdc_req(&req);
    ktest_make_sample_data(&(p->target_domain));
    p->dclocator_hint = 0;
}

static krb5_authdata *
make_ad_element(krb5_authdatatype ad_type, const char *str)
{
    krb5_authdata *ad;

    ad = ealloc(sizeof(*ad));
    ad->ad_type = ad_type;
    ad->length = strlen(str);
    ad->contents = ealloc(ad->length);
    memcpy(ad->contents, str, ad->length);
    return ad;
}

static krb5_verifier_mac *
make_vmac(krb5_boolean include_princ, krb5_kvno kvno, krb5_enctype enctype,
          const char *cksumstr)
{
    krb5_verifier_mac *vmac;

    vmac = ealloc(sizeof(*vmac));
    if (include_princ) {
        ktest_make_sample_principal(&vmac->princ);
        (void)krb5_set_principal_realm(NULL, vmac->princ, "");
    } else {
        vmac->princ = NULL;
    }
    vmac->kvno = kvno;
    vmac->enctype = enctype;
    vmac->checksum.checksum_type = 1;
    vmac->checksum.length = strlen(cksumstr);
    vmac->checksum.contents = ealloc(vmac->checksum.length);
    memcpy(vmac->checksum.contents, cksumstr, vmac->checksum.length);
    return vmac;
}

void
ktest_make_minimal_cammac(krb5_cammac *p)
{
    memset(p, 0, sizeof(*p));
    p->elements = ealloc(2 * sizeof(*p->elements));
    p->elements[0] = make_ad_element(1, "ad1");
    p->elements[1] = NULL;
}

void
ktest_make_maximal_cammac(krb5_cammac *p)
{
    p->elements = ealloc(3 * sizeof(*p->elements));
    p->elements[0] = make_ad_element(1, "ad1");
    p->elements[1] = make_ad_element(2, "ad2");
    p->elements[2] = NULL;
    p->kdc_verifier = make_vmac(TRUE, 5, 16, "cksumkdc");
    p->svc_verifier = make_vmac(TRUE, 5, 16, "cksumsvc");
    p->other_verifiers = ealloc(3 * sizeof(*p->other_verifiers));
    p->other_verifiers[0] = make_vmac(FALSE, 0, 0, "cksum1");
    p->other_verifiers[1] = make_vmac(TRUE, 5, 16, "cksum2");
    p->other_verifiers[2] = NULL;
}

void
ktest_make_sample_secure_cookie(krb5_secure_cookie *p)
{
    ktest_make_sample_pa_data_array(&p->data);
    p->time = SAMPLE_TIME;
}

void
ktest_make_minimal_spake_factor(krb5_spake_factor *p)
{
    p->type = 1;
    p->data = NULL;
}

void
ktest_make_maximal_spake_factor(krb5_spake_factor *p)
{
    p->type = 2;
    p->data = ealloc(sizeof(*p->data));
    krb5_data_parse(p->data, "fdata");
}

void
ktest_make_support_pa_spake(krb5_pa_spake *p)
{
    krb5_spake_support *s = &p->u.support;

    s->ngroups = 2;
    s->groups = ealloc(s->ngroups * sizeof(*s->groups));
    s->groups[0] = 1;
    s->groups[1] = 2;
    p->choice = SPAKE_MSGTYPE_SUPPORT;
}

void
ktest_make_challenge_pa_spake(krb5_pa_spake *p)
{
    krb5_spake_challenge *c = &p->u.challenge;

    c->group = 1;
    krb5_data_parse(&c->pubkey, "T value");
    c->factors = ealloc(3 * sizeof(*c->factors));
    c->factors[0] = ealloc(sizeof(*c->factors[0]));
    ktest_make_minimal_spake_factor(c->factors[0]);
    c->factors[1] = ealloc(sizeof(*c->factors[1]));
    ktest_make_maximal_spake_factor(c->factors[1]);
    c->factors[2] = NULL;
    p->choice = SPAKE_MSGTYPE_CHALLENGE;
}

void
ktest_make_response_pa_spake(krb5_pa_spake *p)
{
    krb5_spake_response *r = &p->u.response;

    krb5_data_parse(&r->pubkey, "S value");
    ktest_make_sample_enc_data(&r->factor);
    p->choice = SPAKE_MSGTYPE_RESPONSE;
}

void
ktest_make_encdata_pa_spake(krb5_pa_spake *p)
{
    ktest_make_sample_enc_data(&p->u.encdata);
    p->choice = SPAKE_MSGTYPE_ENCDATA;
}

/****************************************************************/
/* destructors */

void
ktest_destroy_data(krb5_data **d)
{
    if (*d != NULL) {
        free((*d)->data);
        free(*d);
        *d = NULL;
    }
}

void
ktest_empty_data(krb5_data *d)
{
    if (d->data != NULL) {
        free(d->data);
        d->data = NULL;
        d->length = 0;
    }
}

static void
ktest_empty_checksum(krb5_checksum *cs)
{
    free(cs->contents);
    cs->contents = NULL;
}

void
ktest_destroy_checksum(krb5_checksum **cs)
{
    if (*cs != NULL) {
        free((*cs)->contents);
        free(*cs);
        *cs = NULL;
    }
}

void
ktest_empty_keyblock(krb5_keyblock *kb)
{
    if (kb != NULL) {
        if (kb->contents) {
            free(kb->contents);
            kb->contents = NULL;
        }
    }
}

void
ktest_destroy_keyblock(krb5_keyblock **kb)
{
    if (*kb != NULL) {
        free((*kb)->contents);
        free(*kb);
        *kb = NULL;
    }
}

void
ktest_empty_authorization_data(krb5_authdata **ad)
{
    int i;

    if (*ad != NULL) {
        for (i=0; ad[i] != NULL; i++)
            ktest_destroy_authdata(&ad[i]);
    }
}

void
ktest_destroy_authorization_data(krb5_authdata ***ad)
{
    ktest_empty_authorization_data(*ad);
    free(*ad);
    *ad = NULL;
}

void
ktest_destroy_authdata(krb5_authdata **ad)
{
    if (*ad != NULL) {
        free((*ad)->contents);
        free(*ad);
        *ad = NULL;
    }
}

void
ktest_empty_pa_data_array(krb5_pa_data **pad)
{
    int i;

    for (i=0; pad[i] != NULL; i++)
        ktest_destroy_pa_data(&pad[i]);
}

void
ktest_destroy_pa_data_array(krb5_pa_data ***pad)
{
    ktest_empty_pa_data_array(*pad);
    free(*pad);
    *pad = NULL;
}

void
ktest_destroy_pa_data(krb5_pa_data **pad)
{
    if (*pad != NULL) {
        free((*pad)->contents);
        free(*pad);
        *pad = NULL;
    }
}

void
ktest_destroy_address(krb5_address **a)
{
    if (*a != NULL) {
        free((*a)->contents);
        free(*a);
        *a = NULL;
    }
}

void
ktest_empty_addresses(krb5_address **a)
{
    int i;

    for (i=0; a[i] != NULL; i++)
        ktest_destroy_address(&a[i]);
}

void
ktest_destroy_addresses(krb5_address ***a)
{
    ktest_empty_addresses(*a);
    free(*a);
    *a = NULL;
}

void
ktest_destroy_principal(krb5_principal *p)
{
    int i;

    if (*p == NULL)
        return;
    for (i=0; i<(*p)->length; i++)
        ktest_empty_data(&(*p)->data[i]);
    ktest_empty_data(&(*p)->realm);
    free((*p)->data);
    free(*p);
    *p = NULL;
}

void
ktest_destroy_sequence_of_integer(long **soi)
{
    free(*soi);
    *soi = NULL;
}

void
ktest_destroy_sequence_of_ticket(krb5_ticket ***sot)
{
    int i;

    for (i=0; (*sot)[i] != NULL; i++)
        ktest_destroy_ticket(&(*sot)[i]);
    free(*sot);
    *sot = NULL;
}

void
ktest_destroy_ticket(krb5_ticket **tkt)
{
    ktest_destroy_principal(&(*tkt)->server);
    ktest_destroy_enc_data(&(*tkt)->enc_part);
    /*  ktest_empty_enc_tkt_part(((*tkt)->enc_part2));*/
    free(*tkt);
    *tkt = NULL;
}

void
ktest_empty_ticket(krb5_ticket *tkt)
{
    if (tkt->server)
        ktest_destroy_principal(&tkt->server);
    ktest_destroy_enc_data(&tkt->enc_part);
    if (tkt->enc_part2)
        ktest_destroy_enc_tkt_part(&tkt->enc_part2);
}

void
ktest_destroy_enc_data(krb5_enc_data *ed)
{
    ktest_empty_data(&ed->ciphertext);
    ed->kvno = 0;
}

void
ktest_destroy_etype_info_entry(krb5_etype_info_entry *i)
{
    if (i->salt)
        free(i->salt);
    ktest_empty_data(&i->s2kparams);
    free(i);
}

void
ktest_destroy_etype_info(krb5_etype_info_entry **info)
{
    int i;

    for (i = 0; info[i] != NULL; i++)
        ktest_destroy_etype_info_entry(info[i]);
    free(info);
}

void
ktest_empty_kdc_req(krb5_kdc_req *kr)
{
    if (kr->padata)
        ktest_destroy_pa_data_array(&kr->padata);

    if (kr->client)
        ktest_destroy_principal(&kr->client);

    if (kr->server)
        ktest_destroy_principal(&kr->server);
    free(kr->ktype);
    if (kr->addresses)
        ktest_destroy_addresses(&kr->addresses);
    ktest_destroy_enc_data(&kr->authorization_data);
    if (kr->unenc_authdata)
        ktest_destroy_authorization_data(&kr->unenc_authdata);
    if (kr->second_ticket)
        ktest_destroy_sequence_of_ticket(&kr->second_ticket);

}

void
ktest_empty_kdc_rep(krb5_kdc_rep *kr)
{
    if (kr->padata)
        ktest_destroy_pa_data_array(&kr->padata);

    if (kr->client)
        ktest_destroy_principal(&kr->client);

    if (kr->ticket)
        ktest_destroy_ticket(&kr->ticket);

    ktest_destroy_enc_data(&kr->enc_part);

    if (kr->enc_part2) {
        ktest_empty_enc_kdc_rep_part(kr->enc_part2);
        free(kr->enc_part2);
        kr->enc_part2 = NULL;
    }
}

void
ktest_empty_authenticator(krb5_authenticator *a)
{
    if (a->client)
        ktest_destroy_principal(&a->client);
    if (a->checksum)
        ktest_destroy_checksum(&a->checksum);
    if (a->subkey)
        ktest_destroy_keyblock(&a->subkey);
    if (a->authorization_data)
        ktest_destroy_authorization_data(&a->authorization_data);
}

void
ktest_empty_enc_tkt_part(krb5_enc_tkt_part *etp)
{
    if (etp->session)
        ktest_destroy_keyblock(&etp->session);
    if (etp->client)
        ktest_destroy_principal(&etp->client);
    if (etp->caddrs)
        ktest_destroy_addresses(&etp->caddrs);
    if (etp->authorization_data)
        ktest_destroy_authorization_data(&etp->authorization_data);
    ktest_destroy_transited(&etp->transited);
}

void
ktest_destroy_enc_tkt_part(krb5_enc_tkt_part **etp)
{
    if (*etp) {
        ktest_empty_enc_tkt_part(*etp);
        free(*etp);
        *etp = NULL;
    }
}

void
ktest_empty_enc_kdc_rep_part(krb5_enc_kdc_rep_part *ekr)
{
    if (ekr->session)
        ktest_destroy_keyblock(&ekr->session);

    if (ekr->server)
        ktest_destroy_principal(&ekr->server);

    if (ekr->caddrs)
        ktest_destroy_addresses(&ekr->caddrs);
    ktest_destroy_last_req(&ekr->last_req);
}

void
ktest_destroy_transited(krb5_transited *t)
{
    if (t->tr_contents.data)
        ktest_empty_data(&t->tr_contents);
}

void
ktest_empty_ap_rep(krb5_ap_rep *ar)
{
    ktest_destroy_enc_data(&ar->enc_part);
}

void
ktest_empty_ap_req(krb5_ap_req *ar)
{
    if (ar->ticket)
        ktest_destroy_ticket(&ar->ticket);
    ktest_destroy_enc_data(&ar->authenticator);
}

void
ktest_empty_cred_enc_part(krb5_cred_enc_part *cep)
{
    if (cep->s_address)
        ktest_destroy_address(&cep->s_address);
    if (cep->r_address)
        ktest_destroy_address(&cep->r_address);
    if (cep->ticket_info)
        ktest_destroy_sequence_of_cred_info(&cep->ticket_info);
}

void
ktest_destroy_cred_info(krb5_cred_info **ci)
{
    if ((*ci)->session)
        ktest_destroy_keyblock(&(*ci)->session);
    if ((*ci)->client)
        ktest_destroy_principal(&(*ci)->client);
    if ((*ci)->server)
        ktest_destroy_principal(&(*ci)->server);
    if ((*ci)->caddrs)
        ktest_destroy_addresses(&(*ci)->caddrs);
    free(*ci);
    *ci = NULL;
}

void
ktest_destroy_sequence_of_cred_info(krb5_cred_info ***soci)
{
    int i;

    for (i = 0; (*soci)[i] != NULL; i++)
        ktest_destroy_cred_info(&(*soci)[i]);
    free(*soci);
    *soci = NULL;
}

void
ktest_empty_safe(krb5_safe *s)
{
    ktest_empty_data(&s->user_data);
    ktest_destroy_address(&s->s_address);
    ktest_destroy_address(&s->r_address);
    ktest_destroy_checksum(&s->checksum);
}

void
ktest_empty_priv_enc_part(krb5_priv_enc_part *pep)
{
    ktest_empty_data(&pep->user_data);
    ktest_destroy_address(&pep->s_address);
    ktest_destroy_address(&pep->r_address);
}

void
ktest_empty_priv(krb5_priv *p)
{
    ktest_destroy_enc_data(&p->enc_part);
}

void
ktest_empty_cred(krb5_cred *c)
{
    ktest_destroy_sequence_of_ticket(&c->tickets);
    ktest_destroy_enc_data(&c->enc_part);
    /* enc_part2 */
}

void
ktest_destroy_last_req(krb5_last_req_entry ***lr)
{
    int i;

    if (*lr) {
        for (i=0; (*lr)[i] != NULL; i++)
            free((*lr)[i]);

        free(*lr);
    }
}

void
ktest_empty_error(krb5_error *kerr)
{
    if (kerr->client)
        ktest_destroy_principal(&kerr->client);
    if (kerr->server)
        ktest_destroy_principal(&kerr->server);
    ktest_empty_data(&kerr->text);
    ktest_empty_data(&kerr->e_data);
}

void
ktest_empty_ap_rep_enc_part(krb5_ap_rep_enc_part *arep)
{
    ktest_destroy_keyblock(&(arep)->subkey);
}

void
ktest_empty_sam_challenge_2(krb5_sam_challenge_2 *p)
{
    krb5_checksum **ck;

    ktest_empty_data(&p->sam_challenge_2_body);
    if (p->sam_cksum != NULL) {
        for (ck = p->sam_cksum; *ck != NULL; ck++)
            ktest_destroy_checksum(ck);
        free(p->sam_cksum);
        p->sam_cksum = NULL;
    }
}

void
ktest_empty_sam_challenge_2_body(krb5_sam_challenge_2_body *p)
{
    ktest_empty_data(&p->sam_type_name);
    ktest_empty_data(&p->sam_track_id);
    ktest_empty_data(&p->sam_challenge_label);
    ktest_empty_data(&p->sam_challenge);
    ktest_empty_data(&p->sam_response_prompt);
    ktest_empty_data(&p->sam_pk_for_sad);
}

void
ktest_empty_sam_response_2(krb5_sam_response_2 *p)
{
    ktest_empty_data(&p->sam_track_id);
    ktest_empty_data(&p->sam_enc_nonce_or_sad.ciphertext);
}

void
ktest_empty_enc_sam_response_enc_2(krb5_enc_sam_response_enc_2 *p)
{
    ktest_empty_data(&p->sam_sad);
}

void
ktest_empty_pa_for_user(krb5_pa_for_user *p)
{
    ktest_destroy_principal(&p->user);
    ktest_empty_checksum(&p->cksum);
    ktest_empty_data(&p->auth_package);
}

void
ktest_empty_pa_s4u_x509_user(krb5_pa_s4u_x509_user *p)
{
    ktest_destroy_principal(&p->user_id.user);
    ktest_empty_data(&p->user_id.subject_cert);
    free(p->cksum.contents);
}

void
ktest_empty_ad_kdcissued(krb5_ad_kdcissued *p)
{
    free(p->ad_checksum.contents);
    ktest_destroy_principal(&p->i_principal);
    ktest_destroy_authorization_data(&p->elements);
}

void
ktest_empty_iakerb_header(krb5_iakerb_header *p)
{
    krb5_free_data_contents(NULL, &p->target_realm);
    krb5_free_data(NULL, p->cookie);
}

void
ktest_empty_iakerb_finished(krb5_iakerb_finished *p)
{
    krb5_free_checksum_contents(NULL, &p->checksum);
}

static void
ktest_empty_fast_finished(krb5_fast_finished *p)
{
    ktest_destroy_principal(&p->client);
    ktest_empty_checksum(&p->ticket_checksum);
}

void
ktest_empty_fast_response(krb5_fast_response *p)
{
    ktest_destroy_pa_data_array(&p->padata);
    ktest_destroy_keyblock(&p->strengthen_key);
    if (p->finished != NULL) {
        ktest_empty_fast_finished(p->finished);
        free(p->finished);
        p->finished = NULL;
    }
}

static void
ktest_empty_algorithm_identifier(krb5_algorithm_identifier *p)
{
    ktest_empty_data(&p->algorithm);
    ktest_empty_data(&p->parameters);
}

void
ktest_empty_otp_tokeninfo(krb5_otp_tokeninfo *p)
{
    krb5_algorithm_identifier **alg;

    p->flags = 0;
    krb5_free_data_contents(NULL, &p->vendor);
    krb5_free_data_contents(NULL, &p->challenge);
    krb5_free_data_contents(NULL, &p->token_id);
    krb5_free_data_contents(NULL, &p->alg_id);
    for (alg = p->supported_hash_alg; alg != NULL && *alg != NULL; alg++) {
        ktest_empty_algorithm_identifier(*alg);
        free(*alg);
    }
    free(p->supported_hash_alg);
    p->supported_hash_alg = NULL;
    p->length = p->format = p->iteration_count = -1;
}

void
ktest_empty_pa_otp_challenge(krb5_pa_otp_challenge *p)
{
    krb5_otp_tokeninfo **ti;

    krb5_free_data_contents(NULL, &p->nonce);
    krb5_free_data_contents(NULL, &p->service);
    for (ti = p->tokeninfo; *ti != NULL; ti++) {
        ktest_empty_otp_tokeninfo(*ti);
        free(*ti);
    }
    free(p->tokeninfo);
    p->tokeninfo = NULL;
    krb5_free_data_contents(NULL, &p->salt);
    krb5_free_data_contents(NULL, &p->s2kparams);
}

void
ktest_empty_pa_otp_req(krb5_pa_otp_req *p)
{
    p->flags = 0;
    krb5_free_data_contents(NULL, &p->nonce);
    ktest_destroy_enc_data(&p->enc_data);
    if (p->hash_alg != NULL)
        ktest_empty_algorithm_identifier(p->hash_alg);
    free(p->hash_alg);
    p->hash_alg = NULL;
    p->iteration_count = -1;
    krb5_free_data_contents(NULL, &p->otp_value);
    krb5_free_data_contents(NULL, &p->pin);
    krb5_free_data_contents(NULL, &p->challenge);
    p->time = 0;
    krb5_free_data_contents(NULL, &p->counter);
    p->format = -1;
    krb5_free_data_contents(NULL, &p->token_id);
    krb5_free_data_contents(NULL, &p->alg_id);
    krb5_free_data_contents(NULL, &p->vendor);
}

#ifndef DISABLE_PKINIT

static void
ktest_empty_pk_authenticator(krb5_pk_authenticator *p)
{
    ktest_empty_checksum(&p->paChecksum);
    p->paChecksum.contents = NULL;
    krb5_free_data(NULL, p->freshnessToken);
    p->freshnessToken = NULL;
}

static void
ktest_empty_external_principal_identifier(
    krb5_external_principal_identifier *p)
{
    ktest_empty_data(&p->subjectName);
    ktest_empty_data(&p->issuerAndSerialNumber);
    ktest_empty_data(&p->subjectKeyIdentifier);
}

void
ktest_empty_pa_pk_as_req(krb5_pa_pk_as_req *p)
{
    krb5_external_principal_identifier **pi;

    ktest_empty_data(&p->signedAuthPack);
    for (pi = p->trustedCertifiers; *pi != NULL; pi++) {
        ktest_empty_external_principal_identifier(*pi);
        free(*pi);
    }
    free(p->trustedCertifiers);
    p->trustedCertifiers = NULL;
    ktest_empty_data(&p->kdcPkId);
}

static void
ktest_empty_dh_rep_info(krb5_dh_rep_info *p)
{
    ktest_empty_data(&p->dhSignedData);
    ktest_empty_data(&p->serverDHNonce);
    ktest_destroy_data(&p->kdfID);
}

void
ktest_empty_pa_pk_as_rep(krb5_pa_pk_as_rep *p)
{
    if (p->choice == choice_pa_pk_as_rep_dhInfo)
        ktest_empty_dh_rep_info(&p->u.dh_Info);
    else if (p->choice == choice_pa_pk_as_rep_encKeyPack)
        ktest_empty_data(&p->u.encKeyPack);
    p->choice = choice_pa_pk_as_rep_UNKNOWN;
}

void
ktest_empty_auth_pack(krb5_auth_pack *p)
{
    krb5_algorithm_identifier **ai;
    krb5_data **d;

    ktest_empty_pk_authenticator(&p->pkAuthenticator);
    ktest_empty_data(&p->clientPublicValue);
    if (p->supportedCMSTypes != NULL) {
        for (ai = p->supportedCMSTypes; *ai != NULL; ai++) {
            ktest_empty_algorithm_identifier(*ai);
            free(*ai);
        }
        free(p->supportedCMSTypes);
        p->supportedCMSTypes = NULL;
    }
    ktest_empty_data(&p->clientDHNonce);
    if (p->supportedKDFs != NULL) {
        for (d = p->supportedKDFs; *d != NULL; d++) {
            ktest_empty_data(*d);
            free(*d);
        }
        free(p->supportedKDFs);
        p->supportedKDFs = NULL;
    }
}

void
ktest_empty_kdc_dh_key_info(krb5_kdc_dh_key_info *p)
{
    ktest_empty_data(&p->subjectPublicKey);
}

void
ktest_empty_reply_key_pack(krb5_reply_key_pack *p)
{
    ktest_empty_keyblock(&p->replyKey);
    ktest_empty_checksum(&p->asChecksum);
}

void ktest_empty_sp80056a_other_info(krb5_sp80056a_other_info *p)
{
    ktest_empty_algorithm_identifier(&p->algorithm_identifier);
    ktest_destroy_principal(&p->party_u_info);
    ktest_destroy_principal(&p->party_v_info);
    ktest_empty_data(&p->supp_pub_info);
}

void ktest_empty_pkinit_supp_pub_info(krb5_pkinit_supp_pub_info *p)
{
    ktest_empty_data(&p->as_req);
    ktest_empty_data(&p->pk_as_rep);
}

#endif /* not DISABLE_PKINIT */

#ifdef ENABLE_LDAP
void
ktest_empty_ldap_seqof_key_data(krb5_context ctx, ldap_seqof_key_data *p)
{
    int i;

    for (i = 0; i < p->n_key_data; i++) {
        free(p->key_data[i].key_data_contents[0]);
        free(p->key_data[i].key_data_contents[1]);
    }
    free(p->key_data);
}
#endif

void
ktest_empty_kkdcp_message(krb5_kkdcp_message *p)
{
    ktest_empty_data(&p->kerb_message);
    ktest_empty_data(&p->target_domain);
    p->dclocator_hint = -1;
}

static void
destroy_verifier_mac(krb5_verifier_mac **vmac)
{
    if (*vmac == NULL)
        return;
    ktest_destroy_principal(&(*vmac)->princ);
    ktest_empty_checksum(&(*vmac)->checksum);
    free(*vmac);
    *vmac = NULL;
}

void
ktest_empty_cammac(krb5_cammac *p)
{
    krb5_verifier_mac **vmacp;

    ktest_destroy_authorization_data(&p->elements);
    destroy_verifier_mac(&p->kdc_verifier);
    destroy_verifier_mac(&p->svc_verifier);
    for (vmacp = p->other_verifiers; vmacp != NULL && *vmacp != NULL; vmacp++)
        destroy_verifier_mac(vmacp);
    free(p->other_verifiers);
    p->other_verifiers = NULL;
}

void
ktest_empty_secure_cookie(krb5_secure_cookie *p)
{
    ktest_empty_pa_data_array(p->data);
}

void
ktest_empty_spake_factor(krb5_spake_factor *p)
{
    krb5_free_data(NULL, p->data);
    p->data = NULL;
}

void
ktest_empty_pa_spake(krb5_pa_spake *p)
{
    krb5_spake_factor **f;

    switch (p->choice) {
    case SPAKE_MSGTYPE_SUPPORT:
        free(p->u.support.groups);
        break;
    case SPAKE_MSGTYPE_CHALLENGE:
        ktest_empty_data(&p->u.challenge.pubkey);
        for (f = p->u.challenge.factors; *f != NULL; f++) {
            ktest_empty_spake_factor(*f);
            free(*f);
        }
        free(p->u.challenge.factors);
        break;
    case SPAKE_MSGTYPE_RESPONSE:
        ktest_empty_data(&p->u.response.pubkey);
        ktest_destroy_enc_data(&p->u.response.factor);
        break;
    case SPAKE_MSGTYPE_ENCDATA:
        ktest_destroy_enc_data(&p->u.encdata);
        break;
    default:
        break;
    }
    p->choice = SPAKE_MSGTYPE_UNKNOWN;
}
