/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/crypto_length.c */
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
krb5_c_crypto_length(krb5_context context, krb5_enctype enctype,
                     krb5_cryptotype type, unsigned int *size)
{
    const struct krb5_keytypes *ktp;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    switch (type) {
    case KRB5_CRYPTO_TYPE_EMPTY:
    case KRB5_CRYPTO_TYPE_SIGN_ONLY:
        *size = 0;
        break;
    case KRB5_CRYPTO_TYPE_DATA:
        *size = (unsigned int)~0; /* match Heimdal */
        break;
    case KRB5_CRYPTO_TYPE_HEADER:
    case KRB5_CRYPTO_TYPE_PADDING:
    case KRB5_CRYPTO_TYPE_TRAILER:
    case KRB5_CRYPTO_TYPE_CHECKSUM:
        *size = ktp->crypto_length(ktp, type);
        break;
    default:
        return EINVAL;
    }

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_c_padding_length(krb5_context context, krb5_enctype enctype,
                      size_t data_length, unsigned int *pad_length)
{
    const struct krb5_keytypes *ktp;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    *pad_length = krb5int_c_padding_length(ktp, data_length);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_c_crypto_length_iov(krb5_context context, krb5_enctype enctype,
                         krb5_crypto_iov *data, size_t num_data)
{
    size_t i;
    const struct krb5_keytypes *ktp;
    unsigned int data_length = 0, pad_length;
    krb5_crypto_iov *padding = NULL;

    /*
     * XXX need to rejig internal interface so we can accurately
     * report variable header lengths.
     */

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    for (i = 0; i < num_data; i++) {
        krb5_crypto_iov *iov = &data[i];

        switch (iov->flags) {
        case KRB5_CRYPTO_TYPE_DATA:
            data_length += iov->data.length;
            break;
        case KRB5_CRYPTO_TYPE_PADDING:
            if (padding != NULL)
                return EINVAL;

            padding = iov;
            break;
        case KRB5_CRYPTO_TYPE_HEADER:
        case KRB5_CRYPTO_TYPE_TRAILER:
        case KRB5_CRYPTO_TYPE_CHECKSUM:
            iov->data.length = ktp->crypto_length(ktp, iov->flags);
            break;
        case KRB5_CRYPTO_TYPE_EMPTY:
        case KRB5_CRYPTO_TYPE_SIGN_ONLY:
        default:
            break;
        }
    }

    pad_length = krb5int_c_padding_length(ktp, data_length);
    if (pad_length != 0 && padding == NULL)
        return EINVAL;

    if (padding != NULL)
        padding->data.length = pad_length;

    return 0;
}
