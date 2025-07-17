/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/sha256.c - k5_sha256() implementation */
/*
 * Copyright (C) 2016 by the Massachusetts Institute of Technology.
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

#ifdef K5_OPENSSL_SHA2

#include <openssl/evp.h>

/* 1.1 standardizes constructor and destructor names, renaming
 * EVP_MD_CTX_create and EVP_MD_CTX_destroy. */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

krb5_error_code
k5_sha256(const krb5_data *in, size_t n, uint8_t out[K5_SHA256_HASHLEN])
{
    EVP_MD_CTX *ctx;
    size_t i;
    int ok;

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        return ENOMEM;
    ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    for (i = 0; i < n; i++)
        ok = ok && EVP_DigestUpdate(ctx, in[i].data, in[i].length);
    ok = ok && EVP_DigestFinal_ex(ctx, out, NULL);
    EVP_MD_CTX_free(ctx);
    return ok ? 0 : KRB5_CRYPTO_INTERNAL;
}

#endif
