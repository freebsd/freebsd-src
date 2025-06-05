/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/mk_safe.c - definition of krb5_mk_safe() */
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
 * Marshal a KRB-SAFE message into der_out, with a keyed checksum of type
 * sumtype.  Store the checksum in cksum_out.  Use the timestamp and sequence
 * number from rdata and the addresses from local_addr and remote_addr (the
 * second of which may be NULL).  der_out and cksum_out should be freed by the
 * caller when finished.
 */
static krb5_error_code
create_krbsafe(krb5_context context, const krb5_data *userdata, krb5_key key,
               const krb5_replay_data *rdata, krb5_address *local_addr,
               krb5_address *remote_addr, krb5_cksumtype sumtype,
               krb5_data *der_out, krb5_checksum *cksum_out)
{
    krb5_error_code ret;
    krb5_safe safemsg;
    krb5_octet zero_octet = 0;
    krb5_checksum safe_checksum;
    krb5_data *der_krbsafe;

    if (sumtype && !krb5_c_valid_cksumtype(sumtype))
        return KRB5_PROG_SUMTYPE_NOSUPP;
    if (sumtype && !krb5_c_is_keyed_cksum(sumtype))
        return KRB5KRB_AP_ERR_INAPP_CKSUM;

    safemsg.user_data = *userdata;
    safemsg.s_address = local_addr;
    safemsg.r_address = remote_addr;
    safemsg.timestamp = rdata->timestamp;
    safemsg.usec = rdata->usec;
    safemsg.seq_number = rdata->seq;

    /* Encode the message with a zero-length zero-type checksum. */
    safe_checksum.length = 0;
    safe_checksum.checksum_type = 0;
    safe_checksum.contents = &zero_octet;
    safemsg.checksum = &safe_checksum;
    ret = encode_krb5_safe(&safemsg, &der_krbsafe);
    if (ret)
        return ret;

    /* Checksum the encoding. */
    ret = krb5_k_make_checksum(context, sumtype, key,
                               KRB5_KEYUSAGE_KRB_SAFE_CKSUM, der_krbsafe,
                               &safe_checksum);
    zapfreedata(der_krbsafe);
    if (ret)
        return ret;

    /* Encode the message again with the real checksum. */
    safemsg.checksum = &safe_checksum;
    ret = encode_krb5_safe(&safemsg, &der_krbsafe);
    if (ret) {
        krb5_free_checksum_contents(context, &safe_checksum);
        return ret;
    }

    *der_out = *der_krbsafe;
    free(der_krbsafe);
    *cksum_out = safe_checksum;
    return 0;
}

/* Return the checksum type for the KRB-SAFE message, or 0 to use the enctype's
 * mandatory checksum. */
static krb5_cksumtype
safe_cksumtype(krb5_context context, krb5_auth_context auth_context,
               krb5_enctype enctype)
{
    krb5_error_code ret;
    unsigned int nsumtypes, i;
    krb5_cksumtype *sumtypes;

    /* Use the auth context's safe_cksumtype if it is valid for the enctype.
     * Otherwise return 0 for the mandatory checksum. */
    ret = krb5_c_keyed_checksum_types(context, enctype, &nsumtypes, &sumtypes);
    if (ret != 0)
        return 0;
    for (i = 0; i < nsumtypes; i++) {
        if (auth_context->safe_cksumtype == sumtypes[i])
            break;
    }
    krb5_free_cksumtypes(context, sumtypes);
    return (i == nsumtypes) ? 0 : auth_context->safe_cksumtype;
}

krb5_error_code KRB5_CALLCONV
krb5_mk_safe(krb5_context context, krb5_auth_context authcon,
             const krb5_data *userdata, krb5_data *der_out,
             krb5_replay_data *rdata_out)
{
    krb5_error_code ret;
    krb5_key key;
    krb5_replay_data rdata;
    krb5_data der_krbsafe = empty_data();
    krb5_checksum cksum;
    krb5_address *local_addr, *remote_addr, lstorage, rstorage;
    krb5_cksumtype sumtype;

    *der_out = empty_data();
    memset(&cksum, 0, sizeof(cksum));
    memset(&lstorage, 0, sizeof(lstorage));
    memset(&rstorage, 0, sizeof(rstorage));
    if (authcon->local_addr == NULL)
        return KRB5_LOCAL_ADDR_REQUIRED;

    ret = k5_privsafe_gen_rdata(context, authcon, &rdata, rdata_out);
    if (ret)
        goto cleanup;

    ret = k5_privsafe_gen_addrs(context, authcon, &lstorage, &rstorage,
                                &local_addr, &remote_addr);
    if (ret)
        goto cleanup;

    key = (authcon->send_subkey != NULL) ? authcon->send_subkey : authcon->key;
    sumtype = safe_cksumtype(context, authcon, key->keyblock.enctype);
    ret = create_krbsafe(context, userdata, key, &rdata, local_addr,
                         remote_addr, sumtype, &der_krbsafe, &cksum);
    if (ret)
        goto cleanup;

    ret = k5_privsafe_check_replay(context, authcon, NULL, NULL, &cksum);
    if (ret)
        goto cleanup;

    *der_out = der_krbsafe;
    der_krbsafe = empty_data();
    if ((authcon->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) ||
        (authcon->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE))
        authcon->local_seq_number++;

cleanup:
    krb5_free_data_contents(context, &der_krbsafe);
    krb5_free_checksum_contents(context, &cksum);
    free(lstorage.contents);
    free(rstorage.contents);
    return ret;
}
