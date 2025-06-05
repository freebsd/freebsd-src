/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/rd_safe.c - krb5_rd_safe() */
/*
 * Copyright 1990,1991,2007,2008,2019 by the Massachusetts Institute of
 * Technology.  All Rights Reserved.
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
 * Unmarshal a KRB-SAFE message from der_krbsafe, placing the
 * integrity-protected user data in *userdata_out, replay data in *rdata_out,
 * and checksum in *cksum_out.  The caller should free *userdata_out and
 * *cksum_out when finished.
 */
static krb5_error_code
read_krbsafe(krb5_context context, krb5_auth_context ac,
             const krb5_data *der_krbsafe, krb5_key key,
             krb5_replay_data *rdata_out, krb5_data *userdata_out,
             krb5_checksum **cksum_out)
{
    krb5_error_code ret;
    krb5_safe *krbsafe;
    krb5_data *safe_body = NULL, *der_zerosafe = NULL;
    krb5_checksum zero_cksum, *safe_cksum = NULL;
    krb5_octet zero_octet = 0;
    krb5_boolean valid;
    struct krb5_safe_with_body swb;

    *userdata_out = empty_data();
    *cksum_out = NULL;
    if (!krb5_is_krb_safe(der_krbsafe))
        return KRB5KRB_AP_ERR_MSG_TYPE;

    ret = decode_krb5_safe_with_body(der_krbsafe, &krbsafe, &safe_body);
    if (ret)
        return ret;

    if (!krb5_c_valid_cksumtype(krbsafe->checksum->checksum_type)) {
        ret = KRB5_PROG_SUMTYPE_NOSUPP;
        goto cleanup;
    }
    if (!krb5_c_is_coll_proof_cksum(krbsafe->checksum->checksum_type) ||
        !krb5_c_is_keyed_cksum(krbsafe->checksum->checksum_type)) {
        ret = KRB5KRB_AP_ERR_INAPP_CKSUM;
        goto cleanup;
    }

    ret = k5_privsafe_check_addrs(context, ac, krbsafe->s_address,
                                  krbsafe->r_address);
    if (ret)
        goto cleanup;

    /* Regenerate the KRB-SAFE message without the checksum.  Save the message
     * checksum to verify. */
    safe_cksum = krbsafe->checksum;
    zero_cksum.length = 0;
    zero_cksum.checksum_type = 0;
    zero_cksum.contents = &zero_octet;
    krbsafe->checksum = &zero_cksum;
    swb.body = safe_body;
    swb.safe = krbsafe;
    ret = encode_krb5_safe_with_body(&swb, &der_zerosafe);
    krbsafe->checksum = NULL;
    if (ret)
        goto cleanup;

    /* Verify the checksum over the re-encoded message. */
    ret = krb5_k_verify_checksum(context, key, KRB5_KEYUSAGE_KRB_SAFE_CKSUM,
                                 der_zerosafe, safe_cksum, &valid);
    if (!valid) {
        /* Checksum over only the KRB-SAFE-BODY as specified in RFC 1510. */
        ret = krb5_k_verify_checksum(context, key,
                                     KRB5_KEYUSAGE_KRB_SAFE_CKSUM,
                                     safe_body, safe_cksum, &valid);
        if (!valid) {
            ret = KRB5KRB_AP_ERR_MODIFIED;
            goto cleanup;
        }
    }

    rdata_out->timestamp = krbsafe->timestamp;
    rdata_out->usec = krbsafe->usec;
    rdata_out->seq = krbsafe->seq_number;

    *userdata_out = krbsafe->user_data;
    krbsafe->user_data.data = NULL;

    *cksum_out = safe_cksum;
    safe_cksum = NULL;

cleanup:
    zapfreedata(der_zerosafe);
    krb5_free_data(context, safe_body);
    krb5_free_safe(context, krbsafe);
    krb5_free_checksum(context, safe_cksum);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_rd_safe(krb5_context context, krb5_auth_context authcon,
             const krb5_data *inbuf, krb5_data *userdata_out,
             krb5_replay_data *rdata_out)
{
    krb5_error_code ret;
    krb5_key key;
    krb5_replay_data rdata;
    krb5_data userdata = empty_data();
    krb5_checksum *cksum;
    const krb5_int32 flags = authcon->auth_context_flags;

    *userdata_out = empty_data();

    if (((flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
         (flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) && rdata_out == NULL)
        return KRB5_RC_REQUIRED;

    key = (authcon->recv_subkey != NULL) ? authcon->recv_subkey : authcon->key;
    memset(&rdata, 0, sizeof(rdata));
    ret = read_krbsafe(context, authcon, inbuf, key, &rdata, &userdata,
                       &cksum);
    if (ret)
        goto cleanup;

    ret = k5_privsafe_check_replay(context, authcon, &rdata, NULL, cksum);
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
    krb5_free_data_contents(context, &userdata);
    krb5_free_checksum(context, cksum);
    return ret;
}
