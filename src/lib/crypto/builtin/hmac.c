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

/*
 * Because our built-in HMAC implementation doesn't need to invoke any
 * encryption or keyed hash functions, it is simplest to define it in terms of
 * keyblocks, and then supply a simple wrapper for the "normal" krb5_key-using
 * interfaces.  The keyblock interfaces are useful for code which creates
 * intermediate keyblocks.
 */

/*
 * The HMAC transform looks like:
 *
 * H(K XOR opad, H(K XOR ipad, text))
 *
 * where H is a cryptographic hash
 * K is an n byte key
 * ipad is the byte 0x36 repeated blocksize times
 * opad is the byte 0x5c repeated blocksize times
 * and text is the data being protected
 */

krb5_error_code
krb5int_hmac_keyblock(const struct krb5_hash_provider *hash,
                      const krb5_keyblock *keyblock,
                      const krb5_crypto_iov *data, size_t num_data,
                      krb5_data *output)
{
    unsigned char *xorkey = NULL, *ihash = NULL;
    unsigned int i;
    krb5_crypto_iov *ihash_iov = NULL, ohash_iov[2];
    krb5_data hashout;
    krb5_error_code ret;

    if (keyblock->length > hash->blocksize)
        return KRB5_CRYPTO_INTERNAL;
    if (output->length < hash->hashsize)
        return KRB5_BAD_MSIZE;

    /* Allocate space for the xor key, hash input vector, and inner hash */
    xorkey = k5alloc(hash->blocksize, &ret);
    if (xorkey == NULL)
        goto cleanup;
    ihash = k5alloc(hash->hashsize, &ret);
    if (ihash == NULL)
        goto cleanup;
    ihash_iov = k5calloc(num_data + 1, sizeof(krb5_crypto_iov), &ret);
    if (ihash_iov == NULL)
        goto cleanup;

    /* Create the inner padded key. */
    memset(xorkey, 0x36, hash->blocksize);
    for (i = 0; i < keyblock->length; i++)
        xorkey[i] ^= keyblock->contents[i];

    /* Compute the inner hash over the inner key and input data. */
    ihash_iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    ihash_iov[0].data = make_data(xorkey, hash->blocksize);
    memcpy(ihash_iov + 1, data, num_data * sizeof(krb5_crypto_iov));
    hashout = make_data(ihash, hash->hashsize);
    ret = hash->hash(ihash_iov, num_data + 1, &hashout);
    if (ret != 0)
        goto cleanup;

    /* Create the outer padded key. */
    memset(xorkey, 0x5c, hash->blocksize);
    for (i = 0; i < keyblock->length; i++)
        xorkey[i] ^= keyblock->contents[i];

    /* Compute the outer hash over the outer key and inner hash value. */
    ohash_iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    ohash_iov[0].data = make_data(xorkey, hash->blocksize);
    ohash_iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
    ohash_iov[1].data = make_data(ihash, hash->hashsize);
    output->length = hash->hashsize;
    ret = hash->hash(ohash_iov, 2, output);
    if (ret != 0)
        memset(output->data, 0, output->length);

cleanup:
    zapfree(xorkey, hash->blocksize);
    zapfree(ihash, hash->hashsize);
    free(ihash_iov);
    return ret;
}

krb5_error_code
krb5int_hmac(const struct krb5_hash_provider *hash, krb5_key key,
             const krb5_crypto_iov *data, size_t num_data,
             krb5_data *output)
{
    return krb5int_hmac_keyblock(hash, &key->keyblock, data, num_data, output);
}
