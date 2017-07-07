/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/pbkdf2.c */
/*
 * Copyright 2002, 2008, 2009 by the Massachusetts Institute of Technology.
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
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

krb5_error_code
krb5int_pbkdf2_hmac(const struct krb5_hash_provider *hash,
                    const krb5_data *out, unsigned long count,
                    const krb5_data *pass, const krb5_data *salt)
{
    const EVP_MD *md = NULL;

    /* Get the message digest handle corresponding to the hash. */
    if (hash == &krb5int_hash_sha1)
        md = EVP_sha1();
    else if (hash == &krb5int_hash_sha256)
        md = EVP_sha256();
    else if (hash == &krb5int_hash_sha384)
        md = EVP_sha384();
    if (md == NULL)
        return KRB5_CRYPTO_INTERNAL;

    PKCS5_PBKDF2_HMAC(pass->data, pass->length, (unsigned char *)salt->data,
                      salt->length, count, md, out->length,
                      (unsigned char *)out->data);
    return 0;
}
