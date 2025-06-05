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
krb5_c_encrypt_length(krb5_context context, krb5_enctype enctype,
                      size_t inputlen, size_t *length)
{
    const struct krb5_keytypes *ktp;
    unsigned int header_len = 0, padding_len = 0, trailer_len = 0;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    header_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_HEADER);
    padding_len = krb5int_c_padding_length(ktp, inputlen);
    trailer_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);

    *length = header_len + inputlen + padding_len + trailer_len;
    return 0;
}
