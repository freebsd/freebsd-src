/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/prf.c */
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

/*
 * This contains the implementation of krb5_c_prf, which will find the
 * enctype-specific PRF and then generate pseudo-random data.  This function
 * yields krb5_c_prf_length bytes of output.
 */

#include "crypto_int.h"

krb5_error_code KRB5_CALLCONV
krb5_c_prf_length(krb5_context context, krb5_enctype enctype, size_t *len)
{
    const struct krb5_keytypes *ktp;

    assert(len);
    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;
    *len = ktp->prf_length;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_k_prf(krb5_context context, krb5_key key,
           krb5_data *input, krb5_data *output)
{
    const struct krb5_keytypes *ktp;
    krb5_error_code ret;

    assert(input && output);
    assert(output->data);

    ktp = find_enctype(key->keyblock.enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;
    if (ktp->prf == NULL)
        return KRB5_CRYPTO_INTERNAL;

    output->magic = KV5M_DATA;
    if (ktp->prf_length != output->length)
        return KRB5_CRYPTO_INTERNAL;
    ret = ktp->prf(ktp, key, input, output);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_c_prf(krb5_context context, const krb5_keyblock *keyblock,
           krb5_data *input, krb5_data *output)
{
    krb5_key key;
    krb5_error_code ret;

    ret = krb5_k_create_key(context, keyblock, &key);
    if (ret != 0)
        return ret;
    ret = krb5_k_prf(context, key, input, output);
    krb5_k_free_key(context, key);
    return ret;
}
