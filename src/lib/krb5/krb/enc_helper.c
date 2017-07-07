/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/enc_helper.c */
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

#include "k5-int.h"

krb5_error_code
krb5_encrypt_helper(krb5_context context,
                    const krb5_keyblock *key, krb5_keyusage usage,
                    const krb5_data *plain, krb5_enc_data *cipher)
{
    krb5_error_code ret;
    size_t enclen;

    if ((ret = krb5_c_encrypt_length(context, key->enctype, plain->length,
                                     &enclen)))
        return(ret);

    cipher->ciphertext.length = enclen;
    if ((cipher->ciphertext.data = (char *) malloc(enclen)) == NULL)
        return(ENOMEM);
    ret = krb5_c_encrypt(context, key, usage, 0, plain, cipher);
    if (ret) {
        free(cipher->ciphertext.data);
        cipher->ciphertext.data = NULL;
    }

    return(ret);
}
