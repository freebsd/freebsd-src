/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/securid_sam2/grail.c - Test method for SAM-2 preauth */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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

/*
 * This test method exists to exercise the client SAM-2 code and some of the
 * KDC SAM-2 code.  We make up a weakly random number and presents it to the
 * client in the prompt (in plain text), as well as encrypted in the track ID.
 * To verify, we compare the decrypted track ID to the entered value.
 *
 * Do not use this method in production; it is not secure.
 */

#ifdef GRAIL_PREAUTH

#include "k5-int.h"
#include <kdb.h>
#include <adm_proto.h>
#include <ctype.h>
#include "extern.h"

static krb5_error_code
get_grail_key(krb5_context context, krb5_db_entry *client,
              krb5_keyblock *key_out)
{
    krb5_db_entry *grail_entry = NULL;
    krb5_key_data *kd;
    int sam_type = PA_SAM_TYPE_GRAIL;
    krb5_error_code ret = 0;

    ret = sam_get_db_entry(context, client->princ, &sam_type, &grail_entry);
    if (ret)
        return KRB5_PREAUTH_NO_KEY;
    ret = krb5_dbe_find_enctype(context, grail_entry, -1, -1, -1, &kd);
    if (ret)
        goto cleanup;
    ret = krb5_dbe_decrypt_key_data(context, NULL, kd, key_out, NULL);
    if (ret)
        goto cleanup;

cleanup:
    if (grail_entry)
        krb5_db_free_principal(context, grail_entry);
    return ret;
}

static krb5_error_code
decrypt_track_data(krb5_context context, krb5_db_entry *client,
                   krb5_data *enc_track_data, krb5_data *output)
{
    krb5_error_code ret;
    krb5_keyblock sam_key;
    krb5_enc_data enc;
    krb5_data result = empty_data();

    sam_key.contents = NULL;
    *output = empty_data();

    ret = get_grail_key(context, client, &sam_key);
    if (ret != 0)
        return ret;
    enc.ciphertext = *enc_track_data;
    enc.enctype = ENCTYPE_UNKNOWN;
    enc.kvno = 0;
    ret = alloc_data(&result, enc_track_data->length);
    if (ret)
        goto cleanup;
    ret = krb5_c_decrypt(context, &sam_key,
                         KRB5_KEYUSAGE_PA_SAM_CHALLENGE_TRACKID, 0, &enc,
                         &result);
    if (ret)
        goto cleanup;

    *output = result;
    result = empty_data();

cleanup:
    krb5_free_keyblock_contents(context, &sam_key);
    krb5_free_data_contents(context, &result);
    return ret;
}

static krb5_error_code
encrypt_track_data(krb5_context context, krb5_db_entry *client,
                   krb5_data *track_data, krb5_data *output)
{
    krb5_error_code ret;
    size_t olen;
    krb5_keyblock sam_key;
    krb5_enc_data enc;

    *output = empty_data();
    enc.ciphertext = empty_data();
    sam_key.contents = NULL;

    ret = get_grail_key(context, client, &sam_key);
    if (ret != 0)
        return ret;

    ret = krb5_c_encrypt_length(context, sam_key.enctype,
                                   track_data->length, &olen);
    if (ret != 0)
        goto cleanup;
    assert(olen <= 65536);
    ret = alloc_data(&enc.ciphertext, olen);
    if (ret)
        goto cleanup;
    enc.enctype = sam_key.enctype;
    enc.kvno = 0;

    ret = krb5_c_encrypt(context, &sam_key,
                         KRB5_KEYUSAGE_PA_SAM_CHALLENGE_TRACKID, 0,
                         track_data, &enc);
    if (ret)
        goto cleanup;

    *output = enc.ciphertext;
    enc.ciphertext = empty_data();

cleanup:
    krb5_free_keyblock_contents(context, &sam_key);
    krb5_free_data_contents(context, &enc.ciphertext);
    return ret;
}

krb5_error_code
get_grail_edata(krb5_context context, krb5_db_entry *client,
                krb5_keyblock *client_key, krb5_sam_challenge_2 *sc2_out)
{
    krb5_error_code ret;
    krb5_data tmp_data, track_id = empty_data();
    int tval = time(NULL) % 77777;
    krb5_sam_challenge_2_body sc2b;
    char tval_string[256], prompt[256];

    snprintf(tval_string, sizeof(tval_string), "%d", tval);
    snprintf(prompt, sizeof(prompt), "Enter %d", tval);

    memset(&sc2b, 0, sizeof(sc2b));
    sc2b.magic = KV5M_SAM_CHALLENGE_2;
    sc2b.sam_track_id = empty_data();
    sc2b.sam_flags = KRB5_SAM_SEND_ENCRYPTED_SAD;
    sc2b.sam_type_name = empty_data();
    sc2b.sam_challenge_label = empty_data();
    sc2b.sam_challenge = empty_data();
    sc2b.sam_response_prompt = string2data(prompt);
    sc2b.sam_pk_for_sad = empty_data();
    sc2b.sam_type = PA_SAM_TYPE_GRAIL;
    sc2b.sam_etype = client_key->enctype;

    tmp_data = string2data(tval_string);
    ret = encrypt_track_data(context, client, &tmp_data, &track_id);
    if (ret)
        goto cleanup;
    sc2b.sam_track_id = track_id;

    tmp_data = make_data(&sc2b.sam_nonce, sizeof(sc2b.sam_nonce));
    ret = krb5_c_random_make_octets(context, &tmp_data);
    if (ret)
        goto cleanup;

    ret = sam_make_challenge(context, &sc2b, client_key, sc2_out);

cleanup:
    krb5_free_data_contents(context, &track_id);
    return ret;
}

krb5_error_code
verify_grail_data(krb5_context context, krb5_db_entry *client,
                  krb5_sam_response_2 *sr2, krb5_enc_tkt_part *enc_tkt_reply,
                  krb5_pa_data *pa, krb5_sam_challenge_2 **sc2_out)
{
    krb5_error_code ret;
    krb5_key_data *client_key_data = NULL;
    krb5_keyblock client_key;
    krb5_data scratch = empty_data(), track_id_data = empty_data();
    krb5_enc_sam_response_enc_2 *esre2 = NULL;

    *sc2_out = NULL;
    memset(&client_key, 0, sizeof(client_key));

    if ((sr2->sam_enc_nonce_or_sad.ciphertext.data == NULL) ||
        (sr2->sam_enc_nonce_or_sad.ciphertext.length <= 0))
        return KRB5KDC_ERR_PREAUTH_FAILED;

    ret = krb5_dbe_find_enctype(context, client,
                                sr2->sam_enc_nonce_or_sad.enctype, -1,
                                sr2->sam_enc_nonce_or_sad.kvno,
                                &client_key_data);
    if (ret)
        goto cleanup;

    ret = krb5_dbe_decrypt_key_data(context, NULL, client_key_data,
                                    &client_key, NULL);
    if (ret)
        goto cleanup;
    ret = alloc_data(&scratch, sr2->sam_enc_nonce_or_sad.ciphertext.length);
    if (ret)
        goto cleanup;
    ret = krb5_c_decrypt(context, &client_key, KRB5_KEYUSAGE_PA_SAM_RESPONSE,
                         NULL, &sr2->sam_enc_nonce_or_sad, &scratch);
    if (ret)
        goto cleanup;

    ret = decode_krb5_enc_sam_response_enc_2(&scratch, &esre2);
    if (ret)
        goto cleanup;

    if (sr2->sam_nonce != esre2->sam_nonce) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    if (esre2->sam_sad.length == 0 || esre2->sam_sad.data == NULL) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    ret = decrypt_track_data(context, client, &sr2->sam_track_id,
                             &track_id_data);
    if (ret)
        goto cleanup;

    /* Some enctypes aren't length-preserving; try to work anyway. */
    while (track_id_data.length > 0 &&
           !isdigit(track_id_data.data[track_id_data.length - 1]))
        track_id_data.length--;

    if (!data_eq(track_id_data, esre2->sam_sad)) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    enc_tkt_reply->flags |= (TKT_FLG_HW_AUTH | TKT_FLG_PRE_AUTH);

cleanup:
    krb5_free_keyblock_contents(context, &client_key);
    krb5_free_data_contents(context, &scratch);
    krb5_free_enc_sam_response_enc_2(context, esre2);
    return ret;
}

#endif /* GRAIL_PREAUTH */
