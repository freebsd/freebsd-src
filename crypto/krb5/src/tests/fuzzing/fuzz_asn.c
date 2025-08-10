/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_asn.c - fuzzing harness for ASN.1 encoding/decoding */
/*
 * Copyright (C) 2024 by Arjun. All rights reserved.
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

#include "autoconf.h"
#include <k5-spake.h>

#define kMinInputLength 2
#define kMaxInputLength 2048

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void
free_cred_enc_part_whole(krb5_context ctx, krb5_cred_enc_part *val)
{
    krb5_free_cred_enc_part(ctx, val);
    free(val);
}

static void
free_kkdcp_message(krb5_context context, krb5_kkdcp_message *val)
{
    if (val == NULL)
        return;
    free(val->kerb_message.data);
    free(val->target_domain.data);
    free(val);
}

#define FUZZ_ASAN(type, encoder, decoder, freefn) do {   \
        type *v;                                         \
        krb5_data *data_out = NULL;                      \
                                                         \
        if ((*decoder)(&data_in, &v) != 0)               \
            break;                                       \
                                                         \
        (*encoder)(v, &data_out);                        \
        krb5_free_data(context, data_out);               \
        (*freefn)(context, v);                           \
    } while (0)

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    krb5_context context;
    krb5_data data_in;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    if (krb5_init_context(&context))
        return 0;

    data_in = make_data((void *)data, size);

    /* Adapted from krb5_decode_leak.c */
    FUZZ_ASAN(krb5_authenticator, encode_krb5_authenticator,
              decode_krb5_authenticator, krb5_free_authenticator);
    FUZZ_ASAN(krb5_ticket, encode_krb5_ticket, decode_krb5_ticket,
              krb5_free_ticket);
    FUZZ_ASAN(krb5_keyblock, encode_krb5_encryption_key,
              decode_krb5_encryption_key, krb5_free_keyblock);
    FUZZ_ASAN(krb5_enc_tkt_part, encode_krb5_enc_tkt_part,
              decode_krb5_enc_tkt_part, krb5_free_enc_tkt_part);
    FUZZ_ASAN(krb5_enc_kdc_rep_part, encode_krb5_enc_kdc_rep_part,
              decode_krb5_enc_kdc_rep_part, krb5_free_enc_kdc_rep_part);
    FUZZ_ASAN(krb5_kdc_rep, encode_krb5_as_rep, decode_krb5_as_rep,
              krb5_free_kdc_rep);
    FUZZ_ASAN(krb5_kdc_rep, encode_krb5_tgs_rep, decode_krb5_tgs_rep,
              krb5_free_kdc_rep);
    FUZZ_ASAN(krb5_ap_req, encode_krb5_ap_req, decode_krb5_ap_req,
              krb5_free_ap_req);
    FUZZ_ASAN(krb5_ap_rep, encode_krb5_ap_rep, decode_krb5_ap_rep,
              krb5_free_ap_rep);
    FUZZ_ASAN(krb5_ap_rep_enc_part, encode_krb5_ap_rep_enc_part,
              decode_krb5_ap_rep_enc_part, krb5_free_ap_rep_enc_part);
    FUZZ_ASAN(krb5_kdc_req, encode_krb5_as_req, decode_krb5_as_req,
              krb5_free_kdc_req);
    FUZZ_ASAN(krb5_kdc_req, encode_krb5_tgs_req, decode_krb5_tgs_req,
              krb5_free_kdc_req);
    FUZZ_ASAN(krb5_kdc_req, encode_krb5_kdc_req_body, decode_krb5_kdc_req_body,
              krb5_free_kdc_req);
    FUZZ_ASAN(krb5_safe, encode_krb5_safe, decode_krb5_safe, krb5_free_safe);
    FUZZ_ASAN(krb5_priv, encode_krb5_priv, decode_krb5_priv, krb5_free_priv);
    FUZZ_ASAN(krb5_priv_enc_part, encode_krb5_enc_priv_part,
              decode_krb5_enc_priv_part, krb5_free_priv_enc_part);
    FUZZ_ASAN(krb5_cred, encode_krb5_cred, decode_krb5_cred, krb5_free_cred);
    FUZZ_ASAN(krb5_cred_enc_part, encode_krb5_enc_cred_part,
              decode_krb5_enc_cred_part, free_cred_enc_part_whole);
    FUZZ_ASAN(krb5_error, encode_krb5_error, decode_krb5_error,
              krb5_free_error);
    FUZZ_ASAN(krb5_authdata *, encode_krb5_authdata, decode_krb5_authdata,
              krb5_free_authdata);
    FUZZ_ASAN(krb5_pa_data *, encode_krb5_padata_sequence,
              decode_krb5_padata_sequence, krb5_free_pa_data);
    FUZZ_ASAN(krb5_pa_data *, encode_krb5_typed_data,
              decode_krb5_typed_data, krb5_free_pa_data);
    FUZZ_ASAN(krb5_etype_info_entry *, encode_krb5_etype_info,
              decode_krb5_etype_info, krb5_free_etype_info);
    FUZZ_ASAN(krb5_etype_info_entry *, encode_krb5_etype_info2,
              decode_krb5_etype_info2, krb5_free_etype_info);
    FUZZ_ASAN(krb5_pa_enc_ts, encode_krb5_pa_enc_ts, decode_krb5_pa_enc_ts,
              krb5_free_pa_enc_ts);
    FUZZ_ASAN(krb5_enc_data, encode_krb5_enc_data, decode_krb5_enc_data,
              krb5_free_enc_data);
    FUZZ_ASAN(krb5_sam_challenge_2, encode_krb5_sam_challenge_2,
              decode_krb5_sam_challenge_2, krb5_free_sam_challenge_2);
    FUZZ_ASAN(krb5_sam_challenge_2_body, encode_krb5_sam_challenge_2_body,
              decode_krb5_sam_challenge_2_body,
              krb5_free_sam_challenge_2_body);
    FUZZ_ASAN(krb5_sam_response_2, encode_krb5_sam_response_2,
              decode_krb5_sam_response_2, krb5_free_sam_response_2);
    FUZZ_ASAN(krb5_enc_sam_response_enc_2, encode_krb5_enc_sam_response_enc_2,
              decode_krb5_enc_sam_response_enc_2,
              krb5_free_enc_sam_response_enc_2);
    FUZZ_ASAN(krb5_pa_for_user, encode_krb5_pa_for_user,
              decode_krb5_pa_for_user, krb5_free_pa_for_user);
    FUZZ_ASAN(krb5_pa_s4u_x509_user, encode_krb5_pa_s4u_x509_user,
              decode_krb5_pa_s4u_x509_user, krb5_free_pa_s4u_x509_user);
    FUZZ_ASAN(krb5_ad_kdcissued, encode_krb5_ad_kdcissued,
              decode_krb5_ad_kdcissued, krb5_free_ad_kdcissued);
    FUZZ_ASAN(krb5_iakerb_header, encode_krb5_iakerb_header,
              decode_krb5_iakerb_header, krb5_free_iakerb_header);
    FUZZ_ASAN(krb5_iakerb_finished, encode_krb5_iakerb_finished,
              decode_krb5_iakerb_finished, krb5_free_iakerb_finished);
    FUZZ_ASAN(krb5_fast_response, encode_krb5_fast_response,
              decode_krb5_fast_response, krb5_free_fast_response);
    FUZZ_ASAN(krb5_enc_data, encode_krb5_pa_fx_fast_reply,
              decode_krb5_pa_fx_fast_reply, krb5_free_enc_data);

    /* Adapted from krb5_encode_test.c */
    FUZZ_ASAN(krb5_otp_tokeninfo, encode_krb5_otp_tokeninfo,
              decode_krb5_otp_tokeninfo, k5_free_otp_tokeninfo);
    FUZZ_ASAN(krb5_pa_otp_challenge, encode_krb5_pa_otp_challenge,
              decode_krb5_pa_otp_challenge, k5_free_pa_otp_challenge);
    FUZZ_ASAN(krb5_pa_otp_req, encode_krb5_pa_otp_req, decode_krb5_pa_otp_req,
              k5_free_pa_otp_req);
    FUZZ_ASAN(krb5_data, encode_krb5_pa_otp_enc_req,
              decode_krb5_pa_otp_enc_req, krb5_free_data);
    FUZZ_ASAN(krb5_kkdcp_message, encode_krb5_kkdcp_message,
              decode_krb5_kkdcp_message, free_kkdcp_message);
    FUZZ_ASAN(krb5_cammac, encode_krb5_cammac, decode_krb5_cammac,
              k5_free_cammac);
    FUZZ_ASAN(krb5_secure_cookie, encode_krb5_secure_cookie,
              decode_krb5_secure_cookie, k5_free_secure_cookie);
    FUZZ_ASAN(krb5_spake_factor, encode_krb5_spake_factor,
              decode_krb5_spake_factor, k5_free_spake_factor);
    FUZZ_ASAN(krb5_pa_spake, encode_krb5_pa_spake, decode_krb5_pa_spake,
              k5_free_pa_spake);

    /* Adapted from krb5_decode_test.c */
    {
        krb5_pa_pac_req *pa_pac_req = NULL;

        if (decode_krb5_pa_pac_req(&data_in, &pa_pac_req) == 0)
            free(pa_pac_req);
    }

    krb5_free_context(context);
    return 0;
}
