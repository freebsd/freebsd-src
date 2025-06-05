/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/enc_dk_hmac.c */
/*
 * Copyright 2008, 2009 by the Massachusetts Institute of Technology.
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
krb5int_dk_crypto_length(const struct krb5_keytypes *ktp, krb5_cryptotype type)
{
    switch (type) {
    case KRB5_CRYPTO_TYPE_HEADER:
    case KRB5_CRYPTO_TYPE_PADDING:
        return ktp->enc->block_size;
    case KRB5_CRYPTO_TYPE_TRAILER:
    case KRB5_CRYPTO_TYPE_CHECKSUM:
        return ktp->hash->hashsize;
    default:
        assert(0 && "invalid cryptotype passed to krb5int_dk_crypto_length");
        return 0;
    }
}

unsigned int
krb5int_aes_crypto_length(const struct krb5_keytypes *ktp,
                          krb5_cryptotype type)
{
    switch (type) {
    case KRB5_CRYPTO_TYPE_HEADER:
        return ktp->enc->block_size;
    case KRB5_CRYPTO_TYPE_PADDING:
        return 0;
    case KRB5_CRYPTO_TYPE_TRAILER:
    case KRB5_CRYPTO_TYPE_CHECKSUM:
        return 96 / 8;
    default:
        assert(0 && "invalid cryptotype passed to krb5int_aes_crypto_length");
        return 0;
    }
}

krb5_error_code
krb5int_dk_encrypt(const struct krb5_keytypes *ktp, krb5_key key,
                   krb5_keyusage usage, const krb5_data *ivec,
                   krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    const struct krb5_hash_provider *hash = ktp->hash;
    krb5_error_code ret;
    unsigned char constantdata[K5CLENGTH];
    krb5_data d1, d2;
    krb5_crypto_iov *header, *trailer, *padding;
    krb5_key ke = NULL, ki = NULL;
    size_t i;
    unsigned int blocksize, hmacsize, plainlen = 0, padsize = 0;
    unsigned char *cksum = NULL;

    /* E(Confounder | Plaintext | Pad) | Checksum */

    blocksize = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_PADDING);
    hmacsize = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);

    for (i = 0; i < num_data; i++) {
        krb5_crypto_iov *iov = &data[i];

        if (iov->flags == KRB5_CRYPTO_TYPE_DATA)
            plainlen += iov->data.length;
    }

    /* Validate header and trailer lengths. */

    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL || header->data.length < enc->block_size)
        return KRB5_BAD_MSIZE;

    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer == NULL || trailer->data.length < hmacsize)
        return KRB5_BAD_MSIZE;

    if (blocksize != 0) {
        /* Check that the input data is correctly padded. */
        if (plainlen % blocksize)
            padsize = blocksize - (plainlen % blocksize);
    }

    padding = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_PADDING);
    if (padsize && (padding == NULL || padding->data.length < padsize))
        return KRB5_BAD_MSIZE;

    if (padding != NULL) {
        memset(padding->data.data, 0, padsize);
        padding->data.length = padsize;
    }

    cksum = k5alloc(hash->hashsize, &ret);
    if (ret != 0)
        goto cleanup;

    /* Derive the keys. */

    d1.data = (char *)constantdata;
    d1.length = K5CLENGTH;

    store_32_be(usage, constantdata);

    d1.data[4] = 0xAA;

    ret = krb5int_derive_key(enc, NULL, key, &ke, &d1, DERIVE_RFC3961);
    if (ret != 0)
        goto cleanup;

    d1.data[4] = 0x55;

    ret = krb5int_derive_key(enc, NULL, key, &ki, &d1, DERIVE_RFC3961);
    if (ret != 0)
        goto cleanup;

    /* Generate confounder. */

    header->data.length = enc->block_size;

    ret = krb5_c_random_make_octets(/* XXX */ NULL, &header->data);
    if (ret != 0)
        goto cleanup;

    /* Hash the plaintext. */
    d2.length = hash->hashsize;
    d2.data = (char *)cksum;

    ret = krb5int_hmac(hash, ki, data, num_data, &d2);
    if (ret != 0)
        goto cleanup;

    /* Encrypt the plaintext (header | data | padding) */
    ret = enc->encrypt(ke, ivec, data, num_data);
    if (ret != 0)
        goto cleanup;

    /* Possibly truncate the hash */
    assert(hmacsize <= d2.length);

    memcpy(trailer->data.data, cksum, hmacsize);
    trailer->data.length = hmacsize;

cleanup:
    krb5_k_free_key(NULL, ke);
    krb5_k_free_key(NULL, ki);
    free(cksum);
    return ret;
}

krb5_error_code
krb5int_dk_decrypt(const struct krb5_keytypes *ktp, krb5_key key,
                   krb5_keyusage usage, const krb5_data *ivec,
                   krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    const struct krb5_hash_provider *hash = ktp->hash;
    krb5_error_code ret;
    unsigned char constantdata[K5CLENGTH];
    krb5_data d1;
    krb5_crypto_iov *header, *trailer;
    krb5_key ke = NULL, ki = NULL;
    size_t i;
    unsigned int blocksize; /* enc block size, not confounder len */
    unsigned int hmacsize, cipherlen = 0;
    unsigned char *cksum = NULL;

    /* E(Confounder | Plaintext | Pad) | Checksum */

    blocksize = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_PADDING);
    hmacsize = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);

    if (blocksize != 0) {
        /* Check that the input data is correctly padded. */
        for (i = 0; i < num_data; i++) {
            const krb5_crypto_iov *iov = &data[i];

            if (ENCRYPT_DATA_IOV(iov))
                cipherlen += iov->data.length;
        }
        if (cipherlen % blocksize != 0)
            return KRB5_BAD_MSIZE;
    }

    /* Validate header and trailer lengths */

    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL || header->data.length != enc->block_size)
        return KRB5_BAD_MSIZE;

    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer == NULL || trailer->data.length != hmacsize)
        return KRB5_BAD_MSIZE;

    cksum = k5alloc(hash->hashsize, &ret);
    if (ret != 0)
        goto cleanup;

    /* Derive the keys. */

    d1.data = (char *)constantdata;
    d1.length = K5CLENGTH;

    store_32_be(usage, constantdata);

    d1.data[4] = 0xAA;

    ret = krb5int_derive_key(enc, NULL, key, &ke, &d1, DERIVE_RFC3961);
    if (ret != 0)
        goto cleanup;

    d1.data[4] = 0x55;

    ret = krb5int_derive_key(enc, NULL, key, &ki, &d1, DERIVE_RFC3961);
    if (ret != 0)
        goto cleanup;

    /* Decrypt the plaintext (header | data | padding). */
    ret = enc->decrypt(ke, ivec, data, num_data);
    if (ret != 0)
        goto cleanup;

    /* Verify the hash. */
    d1.length = hash->hashsize; /* non-truncated length */
    d1.data = (char *)cksum;

    ret = krb5int_hmac(hash, ki, data, num_data, &d1);
    if (ret != 0)
        goto cleanup;

    /* Compare only the possibly truncated length. */
    if (k5_bcmp(cksum, trailer->data.data, hmacsize) != 0) {
        ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
        goto cleanup;
    }

cleanup:
    krb5_k_free_key(NULL, ke);
    krb5_k_free_key(NULL, ki);
    free(cksum);
    return ret;
}
