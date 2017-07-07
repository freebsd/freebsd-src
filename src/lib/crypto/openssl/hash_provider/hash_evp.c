/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/hash_provider/hash_evp.c - OpenSSL hash providers */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "crypto_int.h"
#include <openssl/evp.h>

static krb5_error_code
hash_evp(const EVP_MD *type, const krb5_crypto_iov *data, size_t num_data,
         krb5_data *output)
{
    EVP_MD_CTX *ctx;
    const krb5_data *d;
    size_t i;
    int ok;

    if (output->length != (unsigned int)EVP_MD_size(type))
        return KRB5_CRYPTO_INTERNAL;

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        return ENOMEM;

    ok = EVP_DigestInit_ex(ctx, type, NULL);
    for (i = 0; i < num_data; i++) {
        if (!SIGN_IOV(&data[i]))
            continue;
        d = &data[i].data;
        ok = ok && EVP_DigestUpdate(ctx, d->data, d->length);
    }
    ok = ok && EVP_DigestFinal_ex(ctx, (uint8_t *)output->data, NULL);
    EVP_MD_CTX_free(ctx);
    return ok ? 0 : ENOMEM;
}

static krb5_error_code
hash_md4(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    return hash_evp(EVP_md4(), data, num_data, output);
}

static krb5_error_code
hash_md5(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    return hash_evp(EVP_md5(), data, num_data, output);
}

static krb5_error_code
hash_sha1(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    return hash_evp(EVP_sha1(), data, num_data, output);
}

static krb5_error_code
hash_sha256(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    return hash_evp(EVP_sha256(), data, num_data, output);
}

static krb5_error_code
hash_sha384(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    return hash_evp(EVP_sha384(), data, num_data, output);
}

const struct krb5_hash_provider krb5int_hash_md4 = {
    "MD4", 16, 64, hash_md4
};

const struct krb5_hash_provider krb5int_hash_md5 = {
    "MD5", 16, 64, hash_md5
};

const struct krb5_hash_provider krb5int_hash_sha1 = {
    "SHA1", 20, 64, hash_sha1
};

const struct krb5_hash_provider krb5int_hash_sha256 = {
    "SHA-256", 32, 64, hash_sha256
};

const struct krb5_hash_provider krb5int_hash_sha384 = {
    "SHA-384", 48, 128, hash_sha384
};
