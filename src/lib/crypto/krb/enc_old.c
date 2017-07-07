/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/enc_old.c */
/*
 * Copyright 2008 by the Massachusetts Institute of Technology.
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

unsigned int
krb5int_old_crypto_length(const struct krb5_keytypes *ktp,
                          krb5_cryptotype type)
{
    switch (type) {
    case KRB5_CRYPTO_TYPE_HEADER:
        return ktp->enc->block_size + ktp->hash->hashsize;
    case KRB5_CRYPTO_TYPE_PADDING:
        return ktp->enc->block_size;
    case KRB5_CRYPTO_TYPE_TRAILER:
        return 0;
    case KRB5_CRYPTO_TYPE_CHECKSUM:
        return ktp->hash->hashsize;
    default:
        assert(0 && "invalid cryptotype passed to krb5int_old_crypto_length");
        return 0;
    }
}

krb5_error_code
krb5int_old_encrypt(const struct krb5_keytypes *ktp, krb5_key key,
                    krb5_keyusage usage, const krb5_data *ivec,
                    krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    const struct krb5_hash_provider *hash = ktp->hash;
    krb5_error_code ret;
    krb5_crypto_iov *header, *trailer, *padding;
    krb5_data checksum, confounder, crcivec = empty_data();
    unsigned int plainlen, padsize;
    size_t i;

    /* E(Confounder | Checksum | Plaintext | Pad) */

    plainlen = enc->block_size + hash->hashsize;
    for (i = 0; i < num_data; i++) {
        krb5_crypto_iov *iov = &data[i];

        if (iov->flags == KRB5_CRYPTO_TYPE_DATA)
            plainlen += iov->data.length;
    }

    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL ||
        header->data.length < enc->block_size + hash->hashsize)
        return KRB5_BAD_MSIZE;

    /* Trailer may be absent. */
    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer != NULL)
        trailer->data.length = 0;

    /* Check that the input data is correctly padded. */
    padsize = krb5_roundup(plainlen, enc->block_size) - plainlen;
    padding = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_PADDING);
    if (padsize > 0 && (padding == NULL || padding->data.length < padsize))
        return KRB5_BAD_MSIZE;
    if (padding) {
        padding->data.length = padsize;
        memset(padding->data.data, 0, padsize);
    }

    /* Generate a confounder in the header block. */
    confounder = make_data(header->data.data, enc->block_size);
    ret = krb5_c_random_make_octets(0, &confounder);
    if (ret != 0)
        goto cleanup;
    checksum = make_data(header->data.data + enc->block_size, hash->hashsize);
    memset(checksum.data, 0, hash->hashsize);

    /* Checksum the plaintext with zeroed checksum and padding. */
    ret = hash->hash(data, num_data, &checksum);
    if (ret != 0)
        goto cleanup;

    /* Use the key as the ivec for des-cbc-crc if none was provided. */
    if (key->keyblock.enctype == ENCTYPE_DES_CBC_CRC && ivec == NULL) {
        ret = alloc_data(&crcivec, key->keyblock.length);
        if (ret != 0)
            goto cleanup;
        memcpy(crcivec.data, key->keyblock.contents, key->keyblock.length);
        ivec = &crcivec;
    }

    ret = enc->encrypt(key, ivec, data, num_data);
    if (ret != 0)
        goto cleanup;

cleanup:
    zapfree(crcivec.data, crcivec.length);
    return ret;
}

krb5_error_code
krb5int_old_decrypt(const struct krb5_keytypes *ktp, krb5_key key,
                    krb5_keyusage usage, const krb5_data *ivec,
                    krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    const struct krb5_hash_provider *hash = ktp->hash;
    krb5_error_code ret;
    krb5_crypto_iov *header, *trailer;
    krb5_data checksum, crcivec = empty_data();
    char *saved_checksum = NULL;

    /* Check that the input data is correctly padded. */
    if (iov_total_length(data, num_data, FALSE) % enc->block_size != 0)
        return KRB5_BAD_MSIZE;

    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL ||
        header->data.length != enc->block_size + hash->hashsize)
        return KRB5_BAD_MSIZE;

    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer != NULL && trailer->data.length != 0)
        return KRB5_BAD_MSIZE;

    /* Use the key as the ivec for des-cbc-crc if none was provided. */
    if (key->keyblock.enctype == ENCTYPE_DES_CBC_CRC && ivec == NULL) {
        ret = alloc_data(&crcivec, key->keyblock.length);
        memcpy(crcivec.data, key->keyblock.contents, key->keyblock.length);
        ivec = &crcivec;
    }

    /* Decrypt the ciphertext. */
    ret = enc->decrypt(key, ivec, data, num_data);
    if (ret != 0)
        goto cleanup;

    /* Save the checksum, then zero it out in the plaintext. */
    checksum = make_data(header->data.data + enc->block_size, hash->hashsize);
    saved_checksum = k5memdup(checksum.data, checksum.length, &ret);
    if (saved_checksum == NULL)
        goto cleanup;
    memset(checksum.data, 0, checksum.length);

    /*
     * Checksum the plaintext (with zeroed checksum field), storing the result
     * back into the plaintext field we just zeroed out.  Then compare it to
     * the saved checksum.
     */
    ret = hash->hash(data, num_data, &checksum);
    if (k5_bcmp(checksum.data, saved_checksum, checksum.length) != 0) {
        ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
        goto cleanup;
    }

cleanup:
    zapfree(crcivec.data, crcivec.length);
    zapfree(saved_checksum, hash->hashsize);
    return ret;
}
