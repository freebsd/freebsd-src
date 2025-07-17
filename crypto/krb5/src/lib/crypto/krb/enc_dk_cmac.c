/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/enc_dk_cmac.c - Derived-key enctype functions using CMAC */
/*
 * Copyright 2008, 2009, 2010 by the Massachusetts Institute of Technology.
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


#include "crypto_int.h"

#define K5CLENGTH 5 /* 32 bit net byte order integer + one byte seed */

/* AEAD */

unsigned int
krb5int_camellia_crypto_length(const struct krb5_keytypes *ktp,
                               krb5_cryptotype type)
{
    switch (type) {
    case KRB5_CRYPTO_TYPE_HEADER:
        return ktp->enc->block_size;
    case KRB5_CRYPTO_TYPE_PADDING:
        return 0;
    case KRB5_CRYPTO_TYPE_TRAILER:
    case KRB5_CRYPTO_TYPE_CHECKSUM:
        return ktp->enc->block_size;
    default:
        assert(0 && "bad type passed to krb5int_camellia_crypto_length");
        return 0;
    }
}

/* Derive encryption and integrity keys for CMAC-using enctypes. */
static krb5_error_code
derive_keys(const struct krb5_enc_provider *enc, krb5_key key,
            krb5_keyusage usage, krb5_key *ke_out, krb5_key *ki_out)
{
    krb5_error_code ret;
    unsigned char buf[K5CLENGTH];
    krb5_data constant = make_data(buf, K5CLENGTH);
    krb5_key ke, ki;

    *ke_out = *ki_out = NULL;

    /* Derive the encryption key. */
    store_32_be(usage, buf);
    buf[4] = 0xAA;
    ret = krb5int_derive_key(enc, NULL, key, &ke, &constant,
                             DERIVE_SP800_108_CMAC);
    if (ret != 0)
        return ret;

    /* Derive the integrity key. */
    buf[4] = 0x55;
    ret = krb5int_derive_key(enc, NULL, key, &ki, &constant,
                             DERIVE_SP800_108_CMAC);
    if (ret != 0) {
        krb5_k_free_key(NULL, ke);
        return ret;
    }

    *ke_out = ke;
    *ki_out = ki;
    return 0;
}

krb5_error_code
krb5int_dk_cmac_encrypt(const struct krb5_keytypes *ktp, krb5_key key,
                        krb5_keyusage usage, const krb5_data *ivec,
                        krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    krb5_error_code ret;
    krb5_crypto_iov *header, *trailer, *padding;
    krb5_key ke = NULL, ki = NULL;

    /* E(Confounder | Plaintext | Pad) | Checksum */

    /* Validate header and trailer lengths, and zero out padding length. */
    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL || header->data.length < enc->block_size)
        return KRB5_BAD_MSIZE;
    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer == NULL || trailer->data.length < enc->block_size)
        return KRB5_BAD_MSIZE;
    padding = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_PADDING);
    if (padding != NULL)
        padding->data.length = 0;

    /* Derive the encryption and integrity keys. */
    ret = derive_keys(enc, key, usage, &ke, &ki);
    if (ret != 0)
        goto cleanup;

    /* Generate confounder. */
    header->data.length = enc->block_size;
    ret = krb5_c_random_make_octets(NULL, &header->data);
    if (ret != 0)
        goto cleanup;

    /* Checksum the plaintext. */
    ret = krb5int_cmac_checksum(enc, ki, data, num_data, &trailer->data);
    if (ret != 0)
        goto cleanup;

    /* Encrypt the plaintext (header | data | padding) */
    ret = enc->encrypt(ke, ivec, data, num_data);
    if (ret != 0)
        goto cleanup;

cleanup:
    krb5_k_free_key(NULL, ke);
    krb5_k_free_key(NULL, ki);
    return ret;
}

krb5_error_code
krb5int_dk_cmac_decrypt(const struct krb5_keytypes *ktp, krb5_key key,
                        krb5_keyusage usage, const krb5_data *ivec,
                        krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    krb5_error_code ret;
    krb5_crypto_iov *header, *trailer;
    krb5_data cksum = empty_data();
    krb5_key ke = NULL, ki = NULL;

    /* E(Confounder | Plaintext | Pad) | Checksum */

    /* Validate header and trailer lengths. */
    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL || header->data.length != enc->block_size)
        return KRB5_BAD_MSIZE;
    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer == NULL || trailer->data.length != enc->block_size)
        return KRB5_BAD_MSIZE;

    /* Derive the encryption and integrity keys. */
    ret = derive_keys(enc, key, usage, &ke, &ki);
    if (ret != 0)
        goto cleanup;

    /* Decrypt the plaintext (header | data | padding). */
    ret = enc->decrypt(ke, ivec, data, num_data);
    if (ret != 0)
        goto cleanup;

    /* Verify the hash. */
    ret = alloc_data(&cksum, enc->block_size);
    if (ret != 0)
        goto cleanup;
    ret = krb5int_cmac_checksum(enc, ki, data, num_data, &cksum);
    if (ret != 0)
        goto cleanup;
    if (k5_bcmp(cksum.data, trailer->data.data, enc->block_size) != 0)
        ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;

cleanup:
    krb5_k_free_key(NULL, ke);
    krb5_k_free_key(NULL, ki);
    zapfree(cksum.data, cksum.length);
    return ret;
}
