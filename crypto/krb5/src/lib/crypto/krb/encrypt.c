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

krb5_error_code KRB5_CALLCONV
krb5_k_encrypt(krb5_context context, krb5_key key,
               krb5_keyusage usage, const krb5_data *cipher_state,
               const krb5_data *input, krb5_enc_data *output)
{
    const struct krb5_keytypes *ktp;
    krb5_crypto_iov iov[4];
    krb5_error_code ret;
    unsigned int header_len, padding_len, trailer_len, total_len;

    ktp = find_enctype(key->keyblock.enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    output->magic = KV5M_ENC_DATA;
    output->kvno = 0;
    output->enctype = key->keyblock.enctype;

    /* Get the lengths of the token parts and compute the total. */
    header_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_HEADER);
    padding_len = krb5int_c_padding_length(ktp, input->length);
    trailer_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);
    total_len = header_len + input->length + padding_len + trailer_len;
    if (output->ciphertext.length < total_len)
        return KRB5_BAD_MSIZE;

    /* Set up the iov structures for the token parts. */
    iov[0].flags = KRB5_CRYPTO_TYPE_HEADER;
    iov[0].data = make_data(output->ciphertext.data, header_len);

    iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[1].data = make_data(output->ciphertext.data + header_len,
                            input->length);
    if (input->length > 0)
        memcpy(iov[1].data.data, input->data, input->length);

    iov[2].flags = KRB5_CRYPTO_TYPE_PADDING;
    iov[2].data = make_data(iov[1].data.data + input->length, padding_len);

    iov[3].flags = KRB5_CRYPTO_TYPE_TRAILER;
    iov[3].data = make_data(iov[2].data.data + padding_len, trailer_len);

    ret = ktp->encrypt(ktp, key, usage, cipher_state, iov, 4);
    if (ret != 0)
        zap(iov[1].data.data, iov[1].data.length);
    else
        output->ciphertext.length = total_len;
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_c_encrypt(krb5_context context, const krb5_keyblock *keyblock,
               krb5_keyusage usage, const krb5_data *cipher_state,
               const krb5_data *input, krb5_enc_data *output)
{
    krb5_key key;
    krb5_error_code ret;

    ret = krb5_k_create_key(context, keyblock, &key);
    if (ret != 0)
        return ret;
    ret = krb5_k_encrypt(context, key, usage, cipher_state, input, output);
    krb5_k_free_key(context, key);
    return ret;
}
