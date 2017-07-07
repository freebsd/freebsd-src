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

static krb5_error_code
k5_crc32_hash(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    unsigned long c;
    unsigned int i;

    if (output->length != CRC32_CKSUM_LENGTH)
        return KRB5_CRYPTO_INTERNAL;

    c = 0;
    for (i = 0; i < num_data; i++) {
        const krb5_crypto_iov *iov = &data[i];

        if (SIGN_IOV(iov))
            mit_crc32(iov->data.data, iov->data.length, &c);
    }

    store_32_le(c, output->data);
    return 0;
}

const struct krb5_hash_provider krb5int_hash_crc32 = {
    "CRC32",
    CRC32_CKSUM_LENGTH,
    1,
    k5_crc32_hash
};
