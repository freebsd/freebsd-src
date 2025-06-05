/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/prf_dk.c - RFC 3961 simplified profile PRF */
/*
 * Copyright (C) 2004 by the Massachusetts Institute of Technology.
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

#include "crypto_int.h"

krb5_error_code
krb5int_dk_prf(const struct krb5_keytypes *ktp, krb5_key key,
               const krb5_data *in, krb5_data *out)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    const struct krb5_hash_provider *hash = ktp->hash;
    krb5_crypto_iov iov;
    krb5_data cksum = empty_data(), prfconst = make_data("prf", 3);
    krb5_key kp = NULL;
    krb5_error_code ret;

    /* Hash the input data into an allocated buffer. */
    ret = alloc_data(&cksum, hash->hashsize);
    if (ret != 0)
        goto cleanup;
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = *in;
    ret = hash->hash(&iov, 1, &cksum);
    if (ret != 0)
        goto cleanup;

    /* Derive a key using the PRF constant. */
    ret = krb5int_derive_key(ktp->enc, NULL, key, &kp, &prfconst,
                             DERIVE_RFC3961);
    if (ret != 0)
        goto cleanup;

    /* Truncate the hash to the closest multiple of the block size. */
    iov.data.data = cksum.data;
    iov.data.length = (hash->hashsize / enc->block_size) * enc->block_size;

    /* Encrypt the truncated hash in the derived key to get the output. */
    ret = ktp->enc->encrypt(kp, NULL, &iov, 1);
    if (ret != 0)
        goto cleanup;
    memcpy(out->data, iov.data.data, out->length);

cleanup:
    zapfree(cksum.data, hash->hashsize);
    krb5_k_free_key(NULL, kp);
    return ret;
}
