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
krb5_c_make_random_key(krb5_context context, krb5_enctype enctype,
                       krb5_keyblock *random_key)
{
    krb5_error_code ret;
    const struct krb5_keytypes *ktp;
    const struct krb5_enc_provider *enc;
    size_t keybytes, keylength;
    krb5_data random_data;
    unsigned char *bytes = NULL;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;
    enc = ktp->enc;

    keybytes = enc->keybytes;
    keylength = enc->keylength;

    bytes = k5alloc(keybytes, &ret);
    if (ret)
        return ret;
    random_key->contents = k5alloc(keylength, &ret);
    if (ret)
        goto cleanup;

    random_data.data = (char *) bytes;
    random_data.length = keybytes;

    ret = krb5_c_random_make_octets(context, &random_data);
    if (ret)
        goto cleanup;

    random_key->magic = KV5M_KEYBLOCK;
    random_key->enctype = enctype;
    random_key->length = keylength;

    ret = (*ktp->rand2key)(&random_data, random_key);

cleanup:
    if (ret) {
        zapfree(random_key->contents, keylength);
        random_key->contents = NULL;
    }
    zapfree(bytes, keybytes);
    return ret;
}
