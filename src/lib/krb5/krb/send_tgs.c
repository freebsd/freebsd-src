/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/send_tgs.c - Construct a TGS request */
/*
 * Copyright 1990,1991,2009,2013 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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

#include "k5-int.h"
#include "int-proto.h"
#include "fast.h"

/* Choose a random nonce for an AS or TGS request. */
krb5_error_code
k5_generate_nonce(krb5_context context, int32_t *out)
{
    krb5_error_code ret;
    unsigned char random_buf[4];
    krb5_data random_data = make_data(random_buf, 4);

    *out = 0;

    /* We and Heimdal incorrectly encode nonces as signed, so make sure we use
     * a non-negative value to avoid interoperability issues. */
    ret = krb5_c_random_make_octets(context, &random_data);
    if (ret)
        return ret;
    *out = 0x7FFFFFFF & load_32_n(random_buf);
    return 0;
}

/* Construct an AP-REQ message for a TGS request. */
static krb5_error_code
tgs_construct_ap_req(krb5_context context, krb5_data *checksum_data,
                     krb5_creds *tgt, krb5_keyblock *subkey,
                     krb5_data **ap_req_asn1_out)
{
    krb5_cksumtype cksumtype;
    krb5_error_code ret;
    krb5_checksum checksum;
    krb5_authenticator authent;
    krb5_ap_req ap_req;
    krb5_data *authent_asn1 = NULL;
    krb5_ticket *ticket = NULL;
    krb5_enc_data authent_enc;

    *ap_req_asn1_out = NULL;
    memset(&checksum, 0, sizeof(checksum));
    memset(&ap_req, 0, sizeof(ap_req));
    memset(&authent_enc, 0, sizeof(authent_enc));

    /* Determine the authenticator checksum type. */
    switch (tgt->keyblock.enctype) {
    case ENCTYPE_DES_CBC_CRC:
    case ENCTYPE_DES_CBC_MD4:
    case ENCTYPE_DES_CBC_MD5:
    case ENCTYPE_ARCFOUR_HMAC:
    case ENCTYPE_ARCFOUR_HMAC_EXP:
        cksumtype = context->kdc_req_sumtype;
        break;
    default:
        ret = krb5int_c_mandatory_cksumtype(context, tgt->keyblock.enctype,
                                            &cksumtype);
        if (ret)
            goto cleanup;
    }

    /* Generate checksum. */
    ret = krb5_c_make_checksum(context, cksumtype, &tgt->keyblock,
                               KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM, checksum_data,
                               &checksum);
    if (ret)
        goto cleanup;

    /* Construct, encode, and encrypt an authenticator. */
    authent.subkey = subkey;
    authent.seq_number = 0;
    authent.checksum = &checksum;
    authent.client = tgt->client;
    authent.authorization_data = tgt->authdata;
    ret = krb5_us_timeofday(context, &authent.ctime, &authent.cusec);
    if (ret)
        goto cleanup;
    ret = encode_krb5_authenticator(&authent, &authent_asn1);
    if (ret)
        goto cleanup;
    ret = krb5_encrypt_helper(context, &tgt->keyblock,
                              KRB5_KEYUSAGE_TGS_REQ_AUTH, authent_asn1,
                              &authent_enc);
    if (ret)
        goto cleanup;

    ret = decode_krb5_ticket(&tgt->ticket, &ticket);
    if (ret)
        goto cleanup;

    /* Encode the AP-REQ. */
    ap_req.authenticator = authent_enc;
    ap_req.ticket = ticket;
    ret = encode_krb5_ap_req(&ap_req, ap_req_asn1_out);

cleanup:
    free(checksum.contents);
    krb5_free_ticket(context, ticket);
    krb5_free_data_contents(context, &authent_enc.ciphertext);
    if (authent_asn1 != NULL)
        zapfree(authent_asn1->data, authent_asn1->length);
    free(authent_asn1);
    return ret;
}

/*
 * Construct a TGS request and return its ASN.1 encoding as well as the
 * timestamp, nonce, and subkey used.  The pacb_fn callback allows the caller
 * to amend the request padata after the nonce and subkey are determined.
 */
krb5_error_code
k5_make_tgs_req(krb5_context context,
                struct krb5int_fast_request_state *fast_state,
                krb5_creds *tgt, krb5_flags kdcoptions,
                krb5_address *const *addrs, krb5_pa_data **in_padata,
                krb5_creds *desired, k5_pacb_fn pacb_fn, void *pacb_data,
                krb5_data *req_asn1_out, krb5_timestamp *timestamp_out,
                krb5_int32 *nonce_out, krb5_keyblock **subkey_out)
{
    krb5_error_code ret;
    krb5_kdc_req req;
    krb5_data *authdata_asn1 = NULL, *req_body_asn1 = NULL;
    krb5_data *ap_req_asn1 = NULL, *tgs_req_asn1 = NULL;
    krb5_ticket *sec_ticket = NULL;
    krb5_ticket *sec_ticket_arr[2];
    krb5_timestamp time_now;
    krb5_pa_data **padata = NULL, *pa;
    krb5_keyblock *subkey = NULL;
    krb5_enc_data authdata_enc;
    krb5_enctype enctypes[2], *defenctypes = NULL;
    size_t count, i;

    *req_asn1_out = empty_data();
    *timestamp_out = 0;
    *nonce_out = 0;
    *subkey_out = NULL;
    memset(&req, 0, sizeof(req));
    memset(&authdata_enc, 0, sizeof(authdata_enc));

    /* tgt's client principal must match the desired client principal. */
    if (!krb5_principal_compare(context, tgt->client, desired->client))
        return KRB5_PRINC_NOMATCH;

    /* tgt must be an actual credential, not a template. */
    if (!tgt->ticket.length)
        return KRB5_NO_TKT_SUPPLIED;

    req.kdc_options = kdcoptions;
    req.server = desired->server;
    req.from = desired->times.starttime;
    req.till = desired->times.endtime ? desired->times.endtime :
        tgt->times.endtime;
    req.rtime = desired->times.renew_till;
    ret = k5_generate_nonce(context, &req.nonce);
    if (ret)
        return ret;
    *nonce_out = req.nonce;
    ret = krb5_timeofday(context, &time_now);
    if (ret)
        return ret;
    *timestamp_out = time_now;

    req.addresses = (krb5_address **)addrs;

    /* Generate subkey. */
    ret = krb5_generate_subkey(context, &tgt->keyblock, &subkey);
    if (ret)
        return ret;
    TRACE_SEND_TGS_SUBKEY(context, subkey);

    ret = krb5int_fast_tgs_armor(context, fast_state, subkey, &tgt->keyblock,
                                 NULL, NULL);
    if (ret)
        goto cleanup;

    if (desired->authdata != NULL) {
        ret = encode_krb5_authdata(desired->authdata, &authdata_asn1);
        if (ret)
            goto cleanup;
        ret = krb5_encrypt_helper(context, subkey,
                                  KRB5_KEYUSAGE_TGS_REQ_AD_SUBKEY,
                                  authdata_asn1, &authdata_enc);
        if (ret)
            goto cleanup;
        req.authorization_data = authdata_enc;
    }

    if (desired->keyblock.enctype != ENCTYPE_NULL) {
        if (!krb5_c_valid_enctype(desired->keyblock.enctype)) {
            ret = KRB5_PROG_ETYPE_NOSUPP;
            goto cleanup;
        }
        enctypes[0] = desired->keyblock.enctype;
        enctypes[1] = ENCTYPE_NULL;
        req.ktype = enctypes;
        req.nktypes = 1;
    } else {
        /* Get the default TGS enctypes. */
        ret = krb5_get_tgs_ktypes(context, desired->server, &defenctypes);
        if (ret)
            goto cleanup;
        for (count = 0; defenctypes[count]; count++);
        req.ktype = defenctypes;
        req.nktypes = count;
    }
    TRACE_SEND_TGS_ETYPES(context, req.ktype);

    if (kdcoptions & (KDC_OPT_ENC_TKT_IN_SKEY | KDC_OPT_CNAME_IN_ADDL_TKT)) {
        if (desired->second_ticket.length == 0) {
            ret = KRB5_NO_2ND_TKT;
            goto cleanup;
        }
        ret = decode_krb5_ticket(&desired->second_ticket, &sec_ticket);
        if (ret)
            goto cleanup;
        sec_ticket_arr[0] = sec_ticket;
        sec_ticket_arr[1] = NULL;
        req.second_ticket = sec_ticket_arr;
    }

    /* Encode the request body. */
    ret = krb5int_fast_prep_req_body(context, fast_state, &req,
                                     &req_body_asn1);
    if (ret)
        goto cleanup;

    ret = tgs_construct_ap_req(context, req_body_asn1, tgt, subkey,
                               &ap_req_asn1);
    if (ret)
        goto cleanup;

    for (count = 0; in_padata != NULL && in_padata[count] != NULL; count++);

    /* Construct a padata array for the request, beginning with the ap-req. */
    padata = k5calloc(count + 2, sizeof(krb5_pa_data *), &ret);
    if (padata == NULL)
        goto cleanup;
    padata[0] = k5alloc(sizeof(krb5_pa_data), &ret);
    if (padata[0] == NULL)
        goto cleanup;
    padata[0]->pa_type = KRB5_PADATA_AP_REQ;
    padata[0]->contents = k5memdup(ap_req_asn1->data, ap_req_asn1->length,
                                   &ret);
    if (padata[0] == NULL)
        goto cleanup;
    padata[0]->length = ap_req_asn1->length;

    /* Append copies of any other supplied padata. */
    for (i = 0; in_padata != NULL && in_padata[i] != NULL; i++) {
        pa = k5alloc(sizeof(krb5_pa_data), &ret);
        if (pa == NULL)
            goto cleanup;
        pa->pa_type = in_padata[i]->pa_type;
        pa->length = in_padata[i]->length;
        pa->contents = k5memdup(in_padata[i]->contents, in_padata[i]->length,
                                &ret);
        if (pa->contents == NULL)
            goto cleanup;
        padata[i + 1] = pa;
    }
    req.padata = padata;

    if (pacb_fn != NULL) {
        ret = (*pacb_fn)(context, subkey, &req, pacb_data);
        if (ret)
            goto cleanup;
    }

    /* Encode the TGS-REQ.  Discard the krb5_data container. */
    ret = krb5int_fast_prep_req(context, fast_state, &req, ap_req_asn1,
                                encode_krb5_tgs_req, &tgs_req_asn1);
    if (ret)
        goto cleanup;
    *req_asn1_out = *tgs_req_asn1;
    free(tgs_req_asn1);
    tgs_req_asn1 = NULL;

    *subkey_out = subkey;
    subkey = NULL;

cleanup:
    krb5_free_data(context, authdata_asn1);
    krb5_free_data(context, req_body_asn1);
    krb5_free_data(context, ap_req_asn1);
    krb5_free_pa_data(context, req.padata);
    krb5_free_ticket(context, sec_ticket);
    krb5_free_data_contents(context, &authdata_enc.ciphertext);
    krb5_free_keyblock(context, subkey);
    free(defenctypes);
    return ret;
}
