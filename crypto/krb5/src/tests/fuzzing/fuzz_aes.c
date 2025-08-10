/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_aes.c - fuzzing harness for AES encryption/decryption */
/*
 * Copyright (C) 2024 by Arjun. All rights reserved.
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

#include "autoconf.h"
#include <k5-int.h>
#include <crypto_int.h>

#define kMinInputLength 48
#define kMaxInputLength 512

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void
fuzz_aes(const uint8_t *data, size_t size, size_t key_size, krb5_enctype etype)
{
    krb5_error_code ret;
    krb5_keyblock keyblock;
    krb5_crypto_iov iov;
    krb5_key key = NULL;
    char *aeskey = NULL, *data_in = NULL;
    char encivbuf[16] = { 0 }, decivbuf[16] = { 0 };
    krb5_data enciv = make_data(encivbuf, 16), deciv = make_data(decivbuf, 16);

    aeskey = k5memdup(data, key_size, &ret);
    if (ret)
        return;

    data_in = k5memdup(data + key_size, size - key_size, &ret);
    if (ret)
        goto cleanup;

    keyblock.contents = (krb5_octet *)aeskey;
    keyblock.length = key_size;
    keyblock.enctype = etype;

    ret = krb5_k_create_key(NULL, &keyblock, &key);
    if (ret)
        goto cleanup;

    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = make_data(data_in, size - key_size);

    /* iov.data.data is input and output buffer */
    ret = krb5int_aes_encrypt(key, &enciv, &iov, 1);
    if (ret)
        goto cleanup;

    ret = krb5int_aes_decrypt(key, &deciv, &iov, 1);
    if (ret)
        goto cleanup;

    /* Check that decryption result matches original plaintext. */
    ret = memcmp(data_in, data + key_size, size - key_size);
    if (ret)
        abort();

    (void)krb5int_aes_decrypt(key, &deciv, &iov, 1);

cleanup:
    free(aeskey);
    free(data_in);
    krb5_k_free_key(NULL, key);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    fuzz_aes(data, size, 16, ENCTYPE_AES128_CTS_HMAC_SHA1_96);
    fuzz_aes(data, size, 16, ENCTYPE_AES256_CTS_HMAC_SHA1_96);
    fuzz_aes(data, size, 32, ENCTYPE_AES128_CTS_HMAC_SHA1_96);
    fuzz_aes(data, size, 32, ENCTYPE_AES256_CTS_HMAC_SHA1_96);

    return 0;
}
