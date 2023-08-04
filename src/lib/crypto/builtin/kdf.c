/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "crypto_int.h"

#ifdef K5_BUILTIN_KDF

krb5_error_code
k5_sp800_108_counter_hmac(const struct krb5_hash_provider *hash,
                          krb5_key key, const krb5_data *label,
                          const krb5_data *context, krb5_data *rnd_out)
{
    krb5_crypto_iov iov[5];
    krb5_error_code ret;
    krb5_data prf;
    unsigned char ibuf[4], lbuf[4];

    if (hash == NULL || rnd_out->length > hash->hashsize)
        return KRB5_CRYPTO_INTERNAL;

    /* Allocate encryption data buffer. */
    ret = alloc_data(&prf, hash->hashsize);
    if (ret)
        return ret;

    /* [i]2: four-byte big-endian binary string giving the block counter (1) */
    iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[0].data = make_data(ibuf, sizeof(ibuf));
    store_32_be(1, ibuf);
    /* Label */
    iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[1].data = *label;
    /* 0x00: separator byte */
    iov[2].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[2].data = make_data("", 1);
    /* Context */
    iov[3].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[3].data = *context;
    /* [L]2: four-byte big-endian binary string giving the output length */
    iov[4].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[4].data = make_data(lbuf, sizeof(lbuf));
    store_32_be(rnd_out->length * 8, lbuf);

    ret = krb5int_hmac(hash, key, iov, 5, &prf);
    if (!ret)
        memcpy(rnd_out->data, prf.data, rnd_out->length);
    zapfree(prf.data, prf.length);
    return ret;
}

krb5_error_code
k5_sp800_108_feedback_cmac(const struct krb5_enc_provider *enc, krb5_key key,
                           const krb5_data *label, krb5_data *rnd_out)
{
    size_t blocksize, keybytes, n;
    krb5_crypto_iov iov[6];
    krb5_error_code ret;
    krb5_data prf;
    unsigned int i;
    unsigned char ibuf[4], Lbuf[4];

    blocksize = enc->block_size;
    keybytes = enc->keybytes;

    if (key->keyblock.length != enc->keylength || rnd_out->length != keybytes)
        return KRB5_CRYPTO_INTERNAL;

    /* Allocate encryption data buffer. */
    ret = alloc_data(&prf, blocksize);
    if (ret)
        return ret;

    /* K(i-1): the previous block of PRF output, initially all-zeros. */
    iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[0].data = prf;
    /* [i]2: four-byte big-endian binary string giving the block counter */
    iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[1].data = make_data(ibuf, sizeof(ibuf));
    /* Label: the fixed derived-key input */
    iov[2].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[2].data = *label;
    /* 0x00: separator byte */
    iov[3].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[3].data = make_data("", 1);
    /* Context: (unused) */
    iov[4].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[4].data = empty_data();
    /* [L]2: four-byte big-endian binary string giving the output length */
    iov[5].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[5].data = make_data(Lbuf, sizeof(Lbuf));
    store_32_be(rnd_out->length * 8, Lbuf);

    for (i = 1, n = 0; n < keybytes; i++) {
        /* Update the block counter. */
        store_32_be(i, ibuf);

        /* Compute a CMAC checksum, storing the result into K(i-1). */
        ret = krb5int_cmac_checksum(enc, key, iov, 6, &prf);
        if (ret)
            goto cleanup;

        /* Copy the result into the appropriate part of the output buffer. */
        if (keybytes - n <= blocksize) {
            memcpy(rnd_out->data + n, prf.data, keybytes - n);
            break;
        }
        memcpy(rnd_out->data + n, prf.data, blocksize);
        n += blocksize;
    }

cleanup:
    zapfree(prf.data, blocksize);
    return ret;
}

krb5_error_code
k5_derive_random_rfc3961(const struct krb5_enc_provider *enc, krb5_key key,
                         const krb5_data *constant, krb5_data *rnd_out)
{
    size_t blocksize, keybytes, n;
    krb5_error_code ret;
    krb5_data block = empty_data();

    blocksize = enc->block_size;
    keybytes = enc->keybytes;

    if (blocksize == 1)
        return KRB5_BAD_ENCTYPE;
    if (key->keyblock.length != enc->keylength || rnd_out->length != keybytes)
        return KRB5_CRYPTO_INTERNAL;

    /* Allocate encryption data buffer. */
    ret = alloc_data(&block, blocksize);
    if (ret)
        return ret;

    /* Initialize the input block. */
    if (constant->length == blocksize) {
        memcpy(block.data, constant->data, blocksize);
    } else {
        krb5int_nfold(constant->length * 8, (uint8_t *)constant->data,
                      blocksize * 8, (uint8_t *)block.data);
    }

    /* Loop encrypting the blocks until enough key bytes are generated. */
    n = 0;
    while (n < keybytes) {
        ret = encrypt_block(enc, key, &block);
        if (ret)
            goto cleanup;

        if ((keybytes - n) <= blocksize) {
            memcpy(rnd_out->data + n, block.data, (keybytes - n));
            break;
        }

        memcpy(rnd_out->data + n, block.data, blocksize);
        n += blocksize;
    }

cleanup:
    zapfree(block.data, blocksize);
    return ret;
}

#endif /* K5_BUILTIN_KDF */
