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
krb5_k_decrypt(krb5_context context, krb5_key key,
               krb5_keyusage usage, const krb5_data *cipher_state,
               const krb5_enc_data *input, krb5_data *output)
{
    const struct krb5_keytypes *ktp;
    krb5_crypto_iov iov[4];
    krb5_error_code ret;
    unsigned int header_len, trailer_len, plain_len;
    char *scratch = NULL;

    ktp = find_enctype(key->keyblock.enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    if (input->enctype != ENCTYPE_UNKNOWN && ktp->etype != input->enctype)
        return KRB5_BAD_ENCTYPE;

    /* Verify the input and output lengths. */
    header_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_HEADER);
    trailer_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);
    if (input->ciphertext.length < header_len + trailer_len)
        return KRB5_BAD_MSIZE;
    plain_len = input->ciphertext.length - header_len - trailer_len;
    if (output->length < plain_len)
        return KRB5_BAD_MSIZE;

    scratch = k5alloc(header_len + trailer_len, &ret);
    if (scratch == NULL)
        return ret;

    iov[0].flags = KRB5_CRYPTO_TYPE_HEADER;
    iov[0].data = make_data(scratch, header_len);
    memcpy(iov[0].data.data, input->ciphertext.data, header_len);

    iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[1].data = make_data(output->data, plain_len);
    memcpy(iov[1].data.data, input->ciphertext.data + header_len, plain_len);

    /* Use empty padding since tokens don't indicate the padding length. */
    iov[2].flags = KRB5_CRYPTO_TYPE_PADDING;
    iov[2].data = empty_data();

    iov[3].flags = KRB5_CRYPTO_TYPE_TRAILER;
    iov[3].data = make_data(scratch + header_len, trailer_len);
    memcpy(iov[3].data.data, input->ciphertext.data + header_len + plain_len,
           trailer_len);

    ret = ktp->decrypt(ktp, key, usage, cipher_state, iov, 4);
    if (ret != 0)
        zap(output->data, plain_len);
    else
        output->length = plain_len;
    zapfree(scratch, header_len + trailer_len);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_c_decrypt(krb5_context context, const krb5_keyblock *keyblock,
               krb5_keyusage usage, const krb5_data *cipher_state,
               const krb5_enc_data *input, krb5_data *output)
{
    krb5_key key;
    krb5_error_code ret;

    ret = krb5_k_create_key(context, keyblock, &key);
    if (ret != 0)
        return ret;
    ret = krb5_k_decrypt(context, key, usage, cipher_state, input, output);
    krb5_k_free_key(context, key);
    return ret;
}
