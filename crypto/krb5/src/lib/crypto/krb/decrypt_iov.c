/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/decrypt_iov.c */
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
krb5_k_decrypt_iov(krb5_context context, krb5_key key, krb5_keyusage usage,
                   const krb5_data *cipher_state, krb5_crypto_iov *data,
                   size_t num_data)
{
    const struct krb5_keytypes *ktp;

    ktp = find_enctype(key->keyblock.enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    if (krb5int_c_locate_iov(data, num_data,
                             KRB5_CRYPTO_TYPE_STREAM) != NULL) {
        return krb5int_c_iov_decrypt_stream(ktp, key, usage, cipher_state,
                                            data, num_data);
    }

    return ktp->decrypt(ktp, key, usage, cipher_state, data, num_data);
}

krb5_error_code KRB5_CALLCONV
krb5_c_decrypt_iov(krb5_context context, const krb5_keyblock *keyblock,
                   krb5_keyusage usage, const krb5_data *cipher_state,
                   krb5_crypto_iov *data, size_t num_data)
{
    krb5_key key;
    krb5_error_code ret;

    ret = krb5_k_create_key(context, keyblock, &key);
    if (ret != 0)
        return ret;
    ret = krb5_k_decrypt_iov(context, key, usage, cipher_state, data,
                             num_data);
    krb5_k_free_key(context, key);
    return ret;
}
