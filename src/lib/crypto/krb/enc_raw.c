/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/enc_raw.c */
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
krb5int_raw_crypto_length(const struct krb5_keytypes *ktp,
                          krb5_cryptotype type)
{
    switch (type) {
    case KRB5_CRYPTO_TYPE_PADDING:
        return ktp->enc->block_size;
    default:
        return 0;
    }
}

krb5_error_code
krb5int_raw_encrypt(const struct krb5_keytypes *ktp, krb5_key key,
                    krb5_keyusage usage, const krb5_data *ivec,
                    krb5_crypto_iov *data, size_t num_data)
{
    krb5_crypto_iov *padding;
    size_t i;
    unsigned int blocksize, plainlen = 0, padsize = 0;

    blocksize = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_PADDING);

    for (i = 0; i < num_data; i++) {
        krb5_crypto_iov *iov = &data[i];

        if (iov->flags == KRB5_CRYPTO_TYPE_DATA)
            plainlen += iov->data.length;
    }

    if (blocksize != 0) {
        /* Check that the input data is correctly padded */
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

    return ktp->enc->encrypt(key, ivec, data, num_data);
}

krb5_error_code
krb5int_raw_decrypt(const struct krb5_keytypes *ktp, krb5_key key,
                    krb5_keyusage usage, const krb5_data *ivec,
                    krb5_crypto_iov *data, size_t num_data)
{
    size_t i;
    unsigned int blocksize = 0; /* enc block size, not confounder len */
    unsigned int cipherlen = 0;

    /* E(Confounder | Plaintext | Pad) | Checksum */

    blocksize = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_PADDING);

    for (i = 0; i < num_data; i++) {
        const krb5_crypto_iov *iov = &data[i];

        if (ENCRYPT_DATA_IOV(iov))
            cipherlen += iov->data.length;
    }

    if (blocksize == 0) {
        /* Check for correct input length in CTS mode */
        if (ktp->enc->block_size != 0 && cipherlen < ktp->enc->block_size)
            return KRB5_BAD_MSIZE;
    } else {
        /* Check that the input data is correctly padded */
        if (cipherlen % blocksize != 0)
            return KRB5_BAD_MSIZE;
    }

    return ktp->enc->decrypt(key, ivec, data, num_data);
}
