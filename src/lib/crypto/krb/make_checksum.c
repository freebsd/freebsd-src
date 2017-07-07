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

/* A 0 checksum type means use the mandatory checksum. */

krb5_error_code KRB5_CALLCONV
krb5_k_make_checksum(krb5_context context, krb5_cksumtype cksumtype,
                     krb5_key key, krb5_keyusage usage,
                     const krb5_data *input, krb5_checksum *cksum)
{
    const struct krb5_cksumtypes *ctp;
    krb5_crypto_iov iov;
    krb5_data cksum_data;
    krb5_octet *trunc;
    krb5_error_code ret;

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

    ret = alloc_data(&cksum_data, ctp->compute_size);
    if (ret != 0)
        return ret;

    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = *input;
    ret = ctp->checksum(ctp, key, usage, &iov, 1, &cksum_data);
    if (ret != 0)
        goto cleanup;

    cksum->magic = KV5M_CHECKSUM;
    cksum->checksum_type = cksumtype;
    cksum->length = ctp->output_size;
    cksum->contents = (krb5_octet *) cksum_data.data;
    cksum_data.data = NULL;
    if (ctp->output_size < ctp->compute_size) {
        trunc = realloc(cksum->contents, ctp->output_size);
        if (trunc != NULL)
            cksum->contents = trunc;
    }

cleanup:
    zapfree(cksum_data.data, ctp->compute_size);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_c_make_checksum(krb5_context context, krb5_cksumtype cksumtype,
                     const krb5_keyblock *keyblock, krb5_keyusage usage,
                     const krb5_data *input, krb5_checksum *cksum)
{
    krb5_key key = NULL;
    krb5_error_code ret;

    if (keyblock != NULL) {
        ret = krb5_k_create_key(context, keyblock, &key);
        if (ret != 0)
            return ret;
    }
    ret = krb5_k_make_checksum(context, cksumtype, key, usage, input, cksum);
    krb5_k_free_key(context, key);
    return ret;
}
