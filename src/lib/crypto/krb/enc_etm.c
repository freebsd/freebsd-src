/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/enc_etm.c - encrypt-then-mac construction for aes-sha2 */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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

#include "crypto_int.h"

unsigned int
krb5int_aes2_crypto_length(const struct krb5_keytypes *ktp,
                           krb5_cryptotype type)
{
    switch (type) {
    case KRB5_CRYPTO_TYPE_HEADER:
        return ktp->enc->block_size;
    case KRB5_CRYPTO_TYPE_PADDING:
        return 0;
    case KRB5_CRYPTO_TYPE_TRAILER:
    case KRB5_CRYPTO_TYPE_CHECKSUM:
        return ktp->hash->hashsize / 2;
    default:
        assert(0 && "invalid cryptotype passed to krb5int_aes2_crypto_length");
        return 0;
    }
}

/* Derive encryption and integrity keys for CMAC-using enctypes. */
static krb5_error_code
derive_keys(const struct krb5_keytypes *ktp, krb5_key key,
            krb5_keyusage usage, krb5_key *ke_out, krb5_data *ki_out)
{
    krb5_error_code ret;
    uint8_t label[5];
    krb5_data label_data = make_data(label, 5), ki = empty_data();
    krb5_key ke = NULL;

    *ke_out = NULL;
    *ki_out = empty_data();

    /* Derive the encryption key. */
    store_32_be(usage, label);
    label[4] = 0xAA;
    ret = krb5int_derive_key(ktp->enc, ktp->hash, key, &ke, &label_data,
                             DERIVE_SP800_108_HMAC);
    if (ret)
        goto cleanup;

    /* Derive the integrity key. */
    label[4] = 0x55;
    ret = alloc_data(&ki, ktp->hash->hashsize / 2);
    if (ret)
        goto cleanup;
    ret = krb5int_derive_random(NULL, ktp->hash, key, &ki, &label_data,
                                DERIVE_SP800_108_HMAC);
    if (ret)
        goto cleanup;

    *ke_out = ke;
    ke = NULL;
    *ki_out = ki;
    ki = empty_data();

cleanup:
    krb5_k_free_key(NULL, ke);
    zapfree(ki.data, ki.length);
    return ret;
}

/* Compute an HMAC checksum over the cipher state and data.  Allocate enough
 * space in *out for the checksum. */
static krb5_error_code
hmac_ivec_data(const struct krb5_keytypes *ktp, const krb5_data *ki,
               const krb5_data *ivec, krb5_crypto_iov *data, size_t num_data,
               krb5_data *out)
{
    krb5_error_code ret;
    krb5_data zeroivec = empty_data();
    krb5_crypto_iov *iovs = NULL;
    krb5_keyblock kb = { 0 };

    if (ivec == NULL) {
        ret = ktp->enc->init_state(NULL, 0, &zeroivec);
        if (ret)
            goto cleanup;
        ivec = &zeroivec;
    }

    /* Make a copy of data with an extra iov at the beginning for the ivec. */
    iovs = k5calloc(num_data + 1, sizeof(*iovs), &ret);
    if (iovs == NULL)
        goto cleanup;
    iovs[0].flags = KRB5_CRYPTO_TYPE_DATA;
    iovs[0].data = *ivec;
    memcpy(iovs + 1, data, num_data * sizeof(*iovs));

    ret = alloc_data(out, ktp->hash->hashsize);
    if (ret)
        goto cleanup;
    kb.length = ki->length;
    kb.contents = (uint8_t *)ki->data;
    ret = krb5int_hmac_keyblock(ktp->hash, &kb, iovs, num_data + 1, out);

cleanup:
    if (zeroivec.data != NULL)
        ktp->enc->free_state(&zeroivec);
    free(iovs);
    return ret;
}

krb5_error_code
krb5int_etm_encrypt(const struct krb5_keytypes *ktp, krb5_key key,
                    krb5_keyusage usage, const krb5_data *ivec,
                    krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    krb5_error_code ret;
    krb5_data ivcopy = empty_data(), cksum = empty_data();
    krb5_crypto_iov *header, *trailer, *padding;
    krb5_key ke = NULL;
    krb5_data ki = empty_data();
    unsigned int trailer_len;

    /* E(Confounder | Plaintext) | Checksum(IV | ciphertext) */

    trailer_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);

    /* Validate header and trailer lengths, and zero out padding length. */
    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL || header->data.length < enc->block_size)
        return KRB5_BAD_MSIZE;
    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer == NULL || trailer->data.length < trailer_len)
        return KRB5_BAD_MSIZE;
    padding = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_PADDING);
    if (padding != NULL)
        padding->data.length = 0;

    if (ivec != NULL) {
        ret = alloc_data(&ivcopy, ivec->length);
        if (ret)
            goto cleanup;
        memcpy(ivcopy.data, ivec->data, ivec->length);
    }

    /* Derive the encryption and integrity keys. */
    ret = derive_keys(ktp, key, usage, &ke, &ki);
    if (ret)
        goto cleanup;

    /* Generate confounder. */
    header->data.length = enc->block_size;
    ret = krb5_c_random_make_octets(NULL, &header->data);
    if (ret)
        goto cleanup;

    /* Encrypt the plaintext (header | data). */
    ret = enc->encrypt(ke, (ivec == NULL) ? NULL : &ivcopy, data, num_data);
    if (ret)
        goto cleanup;

    /* HMAC the IV, confounder, and ciphertext with sign-only data. */
    ret = hmac_ivec_data(ktp, &ki, ivec, data, num_data, &cksum);
    if (ret)
        goto cleanup;

    /* Truncate the HMAC checksum to the trailer length. */
    assert(trailer_len <= cksum.length);
    memcpy(trailer->data.data, cksum.data, trailer_len);
    trailer->data.length = trailer_len;

    /* Copy out the updated ivec if desired. */
    if (ivec != NULL)
        memcpy(ivec->data, ivcopy.data, ivcopy.length);

cleanup:
    krb5_k_free_key(NULL, ke);
    zapfree(ki.data, ki.length);
    free(cksum.data);
    zapfree(ivcopy.data, ivcopy.length);
    return ret;
}

krb5_error_code
krb5int_etm_decrypt(const struct krb5_keytypes *ktp, krb5_key key,
                    krb5_keyusage usage, const krb5_data *ivec,
                    krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    krb5_error_code ret;
    krb5_data cksum = empty_data();
    krb5_crypto_iov *header, *trailer;
    krb5_key ke = NULL;
    krb5_data ki = empty_data();
    unsigned int trailer_len;

    trailer_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);

    /* Validate header and trailer lengths. */
    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL || header->data.length != enc->block_size)
        return KRB5_BAD_MSIZE;
    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer == NULL || trailer->data.length != trailer_len)
        return KRB5_BAD_MSIZE;

    /* Derive the encryption and integrity keys. */
    ret = derive_keys(ktp, key, usage, &ke, &ki);
    if (ret)
        goto cleanup;

    /* HMAC the IV, confounder, and ciphertext with sign-only data. */
    ret = hmac_ivec_data(ktp, &ki, ivec, data, num_data, &cksum);
    if (ret)
        goto cleanup;

    /* Compare only the possibly truncated length. */
    assert(trailer_len <= cksum.length);
    if (k5_bcmp(cksum.data, trailer->data.data, trailer_len) != 0) {
        ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
        goto cleanup;
    }

    /* Decrypt the ciphertext (header | data | padding). */
    ret = enc->decrypt(ke, ivec, data, num_data);

cleanup:
    krb5_k_free_key(NULL, ke);
    zapfree(ki.data, ki.length);
    zapfree(cksum.data, cksum.length);
    return ret;
}
