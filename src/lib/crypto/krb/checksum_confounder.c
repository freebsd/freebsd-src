/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/checksum_confounder.c */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

/*
 * Confounder checksum implementation, using tokens of the form:
 *   enc(xorkey, confounder | hash(confounder | data))
 * where xorkey is the key XOR'd with 0xf0 bytes.
 */

#include "crypto_int.h"

/* Derive a key by XOR with 0xF0 bytes. */
static krb5_error_code
mk_xorkey(krb5_key origkey, krb5_key *xorkey)
{
    krb5_error_code retval = 0;
    unsigned char *xorbytes;
    krb5_keyblock xorkeyblock;
    size_t i = 0;

    xorbytes = k5memdup(origkey->keyblock.contents, origkey->keyblock.length,
                        &retval);
    if (xorbytes == NULL)
        return retval;
    for (i = 0; i < origkey->keyblock.length; i++)
        xorbytes[i] ^= 0xf0;

    /* Do a shallow copy here. */
    xorkeyblock = origkey->keyblock;
    xorkeyblock.contents = xorbytes;

    retval = krb5_k_create_key(0, &xorkeyblock, xorkey);
    zapfree(xorbytes, origkey->keyblock.length);
    return retval;
}

krb5_error_code
krb5int_confounder_checksum(const struct krb5_cksumtypes *ctp,
                            krb5_key key, krb5_keyusage usage,
                            const krb5_crypto_iov *data, size_t num_data,
                            krb5_data *output)
{
    krb5_error_code ret;
    krb5_data conf, hashval;
    krb5_key xorkey = NULL;
    krb5_crypto_iov *hash_iov, iov;
    size_t blocksize = ctp->enc->block_size, hashsize = ctp->hash->hashsize;

    /* Partition the output buffer into confounder and hash. */
    conf = make_data(output->data, blocksize);
    hashval = make_data(output->data + blocksize, hashsize);

    /* Create the confounder. */
    ret = krb5_c_random_make_octets(NULL, &conf);
    if (ret != 0)
        return ret;

    ret = mk_xorkey(key, &xorkey);
    if (ret)
        return ret;

    /* Hash the confounder, then the input data. */
    hash_iov = k5calloc(num_data + 1, sizeof(krb5_crypto_iov), &ret);
    if (hash_iov == NULL)
        goto cleanup;
    hash_iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    hash_iov[0].data = conf;
    memcpy(hash_iov + 1, data, num_data * sizeof(krb5_crypto_iov));
    ret = ctp->hash->hash(hash_iov, num_data + 1, &hashval);
    if (ret != 0)
        goto cleanup;

    /* Confounder and hash are in output buffer; encrypt them in place. */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = *output;
    ret = ctp->enc->encrypt(xorkey, NULL, &iov, 1);

cleanup:
    free(hash_iov);
    krb5_k_free_key(NULL, xorkey);
    return ret;
}

krb5_error_code krb5int_confounder_verify(const struct krb5_cksumtypes *ctp,
                                          krb5_key key, krb5_keyusage usage,
                                          const krb5_crypto_iov *data,
                                          size_t num_data,
                                          const krb5_data *input,
                                          krb5_boolean *valid)
{
    krb5_error_code ret;
    unsigned char *plaintext = NULL;
    krb5_key xorkey = NULL;
    krb5_data computed = empty_data();
    krb5_crypto_iov *hash_iov = NULL, iov;
    size_t blocksize = ctp->enc->block_size, hashsize = ctp->hash->hashsize;

    plaintext = k5memdup(input->data, input->length, &ret);
    if (plaintext == NULL)
        return ret;

    ret = mk_xorkey(key, &xorkey);
    if (ret != 0)
        goto cleanup;

    /* Decrypt the input checksum. */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = make_data(plaintext, input->length);
    ret = ctp->enc->decrypt(xorkey, NULL, &iov, 1);
    if (ret != 0)
        goto cleanup;

    /* Hash the confounder, then the input data. */
    hash_iov = k5calloc(num_data + 1, sizeof(krb5_crypto_iov), &ret);
    if (hash_iov == NULL)
        goto cleanup;
    hash_iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    hash_iov[0].data = make_data(plaintext, blocksize);
    memcpy(hash_iov + 1, data, num_data * sizeof(krb5_crypto_iov));
    ret = alloc_data(&computed, hashsize);
    if (ret != 0)
        goto cleanup;
    ret = ctp->hash->hash(hash_iov, num_data + 1, &computed);
    if (ret != 0)
        goto cleanup;

    /* Compare the decrypted hash to the computed one. */
    *valid = (k5_bcmp(plaintext + blocksize, computed.data, hashsize) == 0);

cleanup:
    zapfree(plaintext, input->length);
    zapfree(computed.data, hashsize);
    free(hash_iov);
    krb5_k_free_key(NULL, xorkey);
    return ret;
}
