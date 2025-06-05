/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/checksum_hmac_md5.c */
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
 * Microsoft HMAC-MD5 and MD5-HMAC checksums (see RFC 4757):
 *   HMAC(KS, hash(msusage || input))
 * KS is HMAC(key, "signaturekey\0") for HMAC-MD5, or just the key for
 * MD5-HMAC.
 */

#include "crypto_int.h"

krb5_error_code krb5int_hmacmd5_checksum(const struct krb5_cksumtypes *ctp,
                                         krb5_key key, krb5_keyusage usage,
                                         const krb5_crypto_iov *data,
                                         size_t num_data,
                                         krb5_data *output)
{
    krb5_keyusage ms_usage;
    krb5_error_code ret;
    krb5_keyblock ks, *keyblock;
    krb5_crypto_iov *hash_iov = NULL, iov;
    krb5_data ds = empty_data(), hashval = empty_data();
    char t[4];

    if (key == NULL || key->keyblock.length > ctp->hash->blocksize)
        return KRB5_BAD_ENCTYPE;
    if (ctp->ctype == CKSUMTYPE_HMAC_MD5_ARCFOUR) {
        /* Compute HMAC(key, "signaturekey\0") to get the signing key ks. */
        ret = alloc_data(&ds, ctp->hash->hashsize);
        if (ret != 0)
            goto cleanup;

        iov.flags = KRB5_CRYPTO_TYPE_DATA;
        iov.data = make_data("signaturekey", 13);
        ret = krb5int_hmac(ctp->hash, key, &iov, 1, &ds);
        if (ret)
            goto cleanup;
        ks.length = ds.length;
        ks.contents = (krb5_octet *) ds.data;
        keyblock = &ks;
    } else  /* For md5-hmac, just use the key. */
        keyblock = &key->keyblock;

    /* Compute the MD5 value of the input. */
    ms_usage = krb5int_arcfour_translate_usage(usage);
    store_32_le(ms_usage, t);
    hash_iov = k5calloc(num_data + 1, sizeof(krb5_crypto_iov), &ret);
    if (hash_iov == NULL)
        goto cleanup;
    hash_iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    hash_iov[0].data = make_data(t, 4);
    memcpy(hash_iov + 1, data, num_data * sizeof(krb5_crypto_iov));
    ret = alloc_data(&hashval, ctp->hash->hashsize);
    if (ret != 0)
        goto cleanup;
    ret = ctp->hash->hash(hash_iov, num_data + 1, &hashval);
    if (ret != 0)
        goto cleanup;

    /* Compute HMAC(ks, md5value). */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = hashval;
    ret = krb5int_hmac_keyblock(ctp->hash, keyblock, &iov, 1, output);

cleanup:
    zapfree(ds.data, ds.length);
    zapfree(hashval.data, hashval.length);
    free(hash_iov);
    return ret;
}
