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

static krb5_key
find_cached_dkey(struct derived_key *list, const krb5_data *constant)
{
    for (; list; list = list->next) {
        if (data_eq(list->constant, *constant)) {
            krb5_k_reference_key(NULL, list->dkey);
            return list->dkey;
        }
    }
    return NULL;
}

static krb5_error_code
add_cached_dkey(krb5_key key, const krb5_data *constant,
                const krb5_keyblock *dkeyblock, krb5_key *cached_dkey)
{
    krb5_key dkey;
    krb5_error_code ret;
    struct derived_key *dkent = NULL;
    char *data = NULL;

    /* Allocate fields for the new entry. */
    dkent = malloc(sizeof(*dkent));
    if (dkent == NULL)
        goto cleanup;
    data = k5memdup(constant->data, constant->length, &ret);
    if (data == NULL)
        goto cleanup;
    ret = krb5_k_create_key(NULL, dkeyblock, &dkey);
    if (ret != 0)
        goto cleanup;

    /* Add the new entry to the list. */
    dkent->dkey = dkey;
    dkent->constant.data = data;
    dkent->constant.length = constant->length;
    dkent->next = key->derived;
    key->derived = dkent;

    /* Return a "copy" of the cached key. */
    krb5_k_reference_key(NULL, dkey);
    *cached_dkey = dkey;
    return 0;

cleanup:
    free(dkent);
    free(data);
    return ENOMEM;
}

static krb5_error_code
derive_random_rfc3961(const struct krb5_enc_provider *enc,
                      krb5_key inkey, krb5_data *outrnd,
                      const krb5_data *in_constant)
{
    size_t blocksize, keybytes, n;
    krb5_error_code ret;
    krb5_data block = empty_data();

    blocksize = enc->block_size;
    keybytes = enc->keybytes;

    if (blocksize == 1)
        return KRB5_BAD_ENCTYPE;
    if (inkey->keyblock.length != enc->keylength || outrnd->length != keybytes)
        return KRB5_CRYPTO_INTERNAL;

    /* Allocate encryption data buffer. */
    ret = alloc_data(&block, blocksize);
    if (ret)
        return ret;

    /* Initialize the input block. */
    if (in_constant->length == blocksize) {
        memcpy(block.data, in_constant->data, blocksize);
    } else {
        krb5int_nfold(in_constant->length * 8,
                      (unsigned char *) in_constant->data,
                      blocksize * 8, (unsigned char *) block.data);
    }

    /* Loop encrypting the blocks until enough key bytes are generated. */
    n = 0;
    while (n < keybytes) {
        ret = encrypt_block(enc, inkey, &block);
        if (ret)
            goto cleanup;

        if ((keybytes - n) <= blocksize) {
            memcpy(outrnd->data + n, block.data, (keybytes - n));
            break;
        }

        memcpy(outrnd->data + n, block.data, blocksize);
        n += blocksize;
    }

cleanup:
    zapfree(block.data, blocksize);
    return ret;
}

/*
 * NIST SP800-108 KDF in feedback mode (section 5.2).
 * Parameters:
 *   - CMAC (with enc as the enc provider) is the PRF.
 *   - A block counter of four bytes is used.
 *   - Label is the key derivation constant.
 *   - Context is empty.
 *   - Four bytes are used to encode the output length in the PRF input.
 */
static krb5_error_code
derive_random_sp800_108_feedback_cmac(const struct krb5_enc_provider *enc,
                                      krb5_key inkey, krb5_data *outrnd,
                                      const krb5_data *in_constant)
{
    size_t blocksize, keybytes, n;
    krb5_crypto_iov iov[6];
    krb5_error_code ret;
    krb5_data prf;
    unsigned int i;
    unsigned char ibuf[4], Lbuf[4];

    blocksize = enc->block_size;
    keybytes = enc->keybytes;

    if (inkey->keyblock.length != enc->keylength || outrnd->length != keybytes)
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
    iov[2].data = *in_constant;
    /* 0x00: separator byte */
    iov[3].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[3].data = make_data("", 1);
    /* Context: (unused) */
    iov[4].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[4].data = empty_data();
    /* [L]2: four-byte big-endian binary string giving the output length */
    iov[5].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[5].data = make_data(Lbuf, sizeof(Lbuf));
    store_32_be(outrnd->length * 8, Lbuf);

    for (i = 1, n = 0; n < keybytes; i++) {
        /* Update the block counter. */
        store_32_be(i, ibuf);

        /* Compute a CMAC checksum, storing the result into K(i-1). */
        ret = krb5int_cmac_checksum(enc, inkey, iov, 6, &prf);
        if (ret)
            goto cleanup;

        /* Copy the result into the appropriate part of the output buffer. */
        if (keybytes - n <= blocksize) {
            memcpy(outrnd->data + n, prf.data, keybytes - n);
            break;
        }
        memcpy(outrnd->data + n, prf.data, blocksize);
        n += blocksize;
    }

cleanup:
    zapfree(prf.data, blocksize);
    return ret;
}

/*
 * NIST SP800-108 KDF in counter mode (section 5.1).
 * Parameters:
 *   - HMAC (with hash as the hash provider) is the PRF.
 *   - A block counter of four bytes is used.
 *   - Four bytes are used to encode the output length in the PRF input.
 *
 * There are no uses requiring more than a single PRF invocation.
 */
krb5_error_code
k5_sp800_108_counter_hmac(const struct krb5_hash_provider *hash,
                          krb5_key inkey, krb5_data *outrnd,
                          const krb5_data *label, const krb5_data *context)
{
    krb5_crypto_iov iov[5];
    krb5_error_code ret;
    krb5_data prf;
    unsigned char ibuf[4], lbuf[4];

    if (hash == NULL || outrnd->length > hash->hashsize)
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
    store_32_be(outrnd->length * 8, lbuf);

    ret = krb5int_hmac(hash, inkey, iov, 5, &prf);
    if (!ret)
        memcpy(outrnd->data, prf.data, outrnd->length);
    zapfree(prf.data, prf.length);
    return ret;
}

krb5_error_code
krb5int_derive_random(const struct krb5_enc_provider *enc,
                      const struct krb5_hash_provider *hash,
                      krb5_key inkey, krb5_data *outrnd,
                      const krb5_data *in_constant, enum deriv_alg alg)
{
    krb5_data empty = empty_data();

    switch (alg) {
    case DERIVE_RFC3961:
        return derive_random_rfc3961(enc, inkey, outrnd, in_constant);
    case DERIVE_SP800_108_CMAC:
        return derive_random_sp800_108_feedback_cmac(enc, inkey, outrnd,
                                                     in_constant);
    case DERIVE_SP800_108_HMAC:
        return k5_sp800_108_counter_hmac(hash, inkey, outrnd, in_constant,
                                         &empty);
    default:
        return EINVAL;
    }
}

/*
 * Compute a derived key into the keyblock outkey.  This variation on
 * krb5int_derive_key does not cache the result, as it is only used
 * directly in situations which are not expected to be repeated with
 * the same inkey and constant.
 */
krb5_error_code
krb5int_derive_keyblock(const struct krb5_enc_provider *enc,
                        const struct krb5_hash_provider *hash,
                        krb5_key inkey, krb5_keyblock *outkey,
                        const krb5_data *in_constant, enum deriv_alg alg)
{
    krb5_error_code ret;
    krb5_data rawkey = empty_data();

    /* Allocate a buffer for the raw key bytes. */
    ret = alloc_data(&rawkey, enc->keybytes);
    if (ret)
        goto cleanup;

    /* Derive pseudo-random data for the key bytes. */
    ret = krb5int_derive_random(enc, hash, inkey, &rawkey, in_constant, alg);
    if (ret)
        goto cleanup;

    /* Postprocess the key. */
    ret = krb5_c_random_to_key(NULL, inkey->keyblock.enctype, &rawkey, outkey);

cleanup:
    zapfree(rawkey.data, enc->keybytes);
    return ret;
}

krb5_error_code
krb5int_derive_key(const struct krb5_enc_provider *enc,
                   const struct krb5_hash_provider *hash,
                   krb5_key inkey, krb5_key *outkey,
                   const krb5_data *in_constant, enum deriv_alg alg)
{
    krb5_keyblock keyblock;
    krb5_error_code ret;
    krb5_key dkey;

    *outkey = NULL;

    /* Check for a cached result. */
    dkey = find_cached_dkey(inkey->derived, in_constant);
    if (dkey != NULL) {
        *outkey = dkey;
        return 0;
    }

    /* Derive into a temporary keyblock. */
    keyblock.length = enc->keylength;
    keyblock.contents = malloc(keyblock.length);
    keyblock.enctype = inkey->keyblock.enctype;
    if (keyblock.contents == NULL)
        return ENOMEM;
    ret = krb5int_derive_keyblock(enc, hash, inkey, &keyblock, in_constant,
                                  alg);
    if (ret)
        goto cleanup;

    /* Cache the derived key. */
    ret = add_cached_dkey(inkey, in_constant, &keyblock, &dkey);
    if (ret != 0)
        goto cleanup;

    *outkey = dkey;

cleanup:
    zapfree(keyblock.contents, keyblock.length);
    return ret;
}
