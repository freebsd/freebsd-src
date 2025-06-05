/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/hash_provider/sha2.c - SHA-2 hash providers */
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
#include "sha2.h"

#ifdef K5_BUILTIN_SHA2

static krb5_error_code
k5_sha256_hash(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    SHA256_CTX ctx;
    size_t i;
    const krb5_crypto_iov *iov;

    if (output->length != SHA256_DIGEST_LENGTH)
        return KRB5_CRYPTO_INTERNAL;

    k5_sha256_init(&ctx);
    for (i = 0; i < num_data; i++) {
        iov = &data[i];
        if (SIGN_IOV(iov))
            k5_sha256_update(&ctx, iov->data.data, iov->data.length);
    }
    k5_sha256_final(output->data, &ctx);
    return 0;
}

static krb5_error_code
k5_sha384_hash(const krb5_crypto_iov *data, size_t num_data, krb5_data *output)
{
    SHA384_CTX ctx;
    size_t i;
    const krb5_crypto_iov *iov;

    if (output->length != SHA384_DIGEST_LENGTH)
        return KRB5_CRYPTO_INTERNAL;

    k5_sha384_init(&ctx);
    for (i = 0; i < num_data; i++) {
        iov = &data[i];
        if (SIGN_IOV(iov))
            k5_sha384_update(&ctx, iov->data.data, iov->data.length);
    }
    k5_sha384_final(output->data, &ctx);
    return 0;
}

const struct krb5_hash_provider krb5int_hash_sha256 = {
    "SHA-256",
    SHA256_DIGEST_LENGTH,
    SHA256_BLOCK_SIZE,
    k5_sha256_hash
};

const struct krb5_hash_provider krb5int_hash_sha384 = {
    "SHA-384",
    SHA384_DIGEST_LENGTH,
    SHA384_BLOCK_SIZE,
    k5_sha384_hash
};

#endif /* K5_BUILTIN_SHA2 */
