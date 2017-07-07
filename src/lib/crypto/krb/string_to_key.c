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
krb5_c_string_to_key(krb5_context context, krb5_enctype enctype,
                     const krb5_data *string, const krb5_data *salt,
                     krb5_keyblock *key)
{
    return krb5_c_string_to_key_with_params(context, enctype, string, salt,
                                            NULL, key);
}

krb5_error_code KRB5_CALLCONV
krb5_c_string_to_key_with_params(krb5_context context, krb5_enctype enctype,
                                 const krb5_data *string,
                                 const krb5_data *salt,
                                 const krb5_data *params, krb5_keyblock *key)
{
    krb5_error_code ret;
    const struct krb5_keytypes *ktp;
    size_t keylength;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;
    keylength = ktp->enc->keylength;

    /* Fail gracefully if someone is using the old AFS string-to-key hack. */
    if (salt != NULL && salt->length == SALT_TYPE_AFS_LENGTH)
        return EINVAL;

    key->contents = malloc(keylength);
    if (key->contents == NULL)
        return ENOMEM;

    key->magic = KV5M_KEYBLOCK;
    key->enctype = enctype;
    key->length = keylength;

    ret = (*ktp->str2key)(ktp, string, salt, params, key);
    if (ret) {
        zapfree(key->contents, keylength);
        key->length = 0;
        key->contents = NULL;
    }

    return ret;
}
