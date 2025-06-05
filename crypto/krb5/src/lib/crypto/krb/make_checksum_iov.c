/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/make_checksum_iov.c */
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

krb5_error_code KRB5_CALLCONV
krb5_k_make_checksum_iov(krb5_context context,
                         krb5_cksumtype cksumtype,
                         krb5_key key,
                         krb5_keyusage usage,
                         krb5_crypto_iov *data,
                         size_t num_data)
{
    krb5_error_code ret;
    krb5_data cksum_data;
    krb5_crypto_iov *checksum;
    const struct krb5_cksumtypes *ctp;

    if (cksumtype == 0) {
        ret = krb5int_c_mandatory_cksumtype(context, key->keyblock.enctype,
                                            &cksumtype);
        if (ret != 0)
            return ret;
    }
    ctp = find_cksumtype(cksumtype);
    if (ctp == NULL)
        return KRB5_BAD_ENCTYPE;

    ret = verify_key(ctp, key);
    if (ret != 0)
        return ret;

    checksum = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_CHECKSUM);
    if (checksum == NULL || checksum->data.length < ctp->output_size)
        return(KRB5_BAD_MSIZE);

    ret = alloc_data(&cksum_data, ctp->compute_size);
    if (ret != 0)
        return ret;

    ret = ctp->checksum(ctp, key, usage, data, num_data, &cksum_data);
    if (ret != 0)
        goto cleanup;

    memcpy(checksum->data.data, cksum_data.data, ctp->output_size);
    checksum->data.length = ctp->output_size;

cleanup:
    zapfree(cksum_data.data, ctp->compute_size);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_c_make_checksum_iov(krb5_context context,
                         krb5_cksumtype cksumtype,
                         const krb5_keyblock *keyblock,
                         krb5_keyusage usage,
                         krb5_crypto_iov *data,
                         size_t num_data)
{
    krb5_key key;
    krb5_error_code ret;

    ret = krb5_k_create_key(context, keyblock, &key);
    if (ret != 0)
        return ret;
    ret = krb5_k_make_checksum_iov(context, cksumtype, key, usage,
                                   data, num_data);
    krb5_k_free_key(context, key);
    return ret;
}
