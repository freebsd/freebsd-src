/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2021 by Red Hat, Inc.
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

#ifdef K5_OPENSSL_KDF

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

static char *
hash_name(const struct krb5_hash_provider *hash)
{
    if (hash == &krb5int_hash_sha1)
        return "SHA1";
    if (hash == &krb5int_hash_sha256)
        return "SHA256";
    if (hash == &krb5int_hash_sha384)
        return "SHA384";
    return NULL;
}

static char *
enc_name(const struct krb5_enc_provider *enc)
{
    if (enc == &krb5int_enc_camellia128)
        return "CAMELLIA-128-CBC";
    if (enc == &krb5int_enc_camellia256)
        return "CAMELLIA-256-CBC";
    if (enc == &krb5int_enc_aes128)
        return "AES-128-CBC";
    if (enc == &krb5int_enc_aes256)
        return "AES-256-CBC";
    if (enc == &krb5int_enc_des3)
        return "DES-EDE3-CBC";
    return NULL;
}

krb5_error_code
k5_sp800_108_counter_hmac(const struct krb5_hash_provider *hash,
                          krb5_key key, const krb5_data *label,
                          const krb5_data *context, krb5_data *rnd_out)
{
    krb5_error_code ret;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    OSSL_PARAM params[6], *p = params;
    char *digest;

    digest = hash_name(hash);
    if (digest == NULL) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    kdf = EVP_KDF_fetch(NULL, "KBKDF", NULL);
    if (!kdf) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    kctx = EVP_KDF_CTX_new(kdf);
    if (!kctx) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0);
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MAC,
                                            OSSL_MAC_NAME_HMAC, 0);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                             key->keyblock.contents,
                                             key->keyblock.length);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                             context->data, context->length);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                             label->data, label->length);
    *p = OSSL_PARAM_construct_end();
    if (EVP_KDF_derive(kctx, (uint8_t *)rnd_out->data, rnd_out->length,
                       params) <= 0) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    ret = 0;
done:
    if (ret)
        zap(rnd_out->data, rnd_out->length);
    EVP_KDF_free(kdf);
    EVP_KDF_CTX_free(kctx);
    return ret;
}

krb5_error_code
k5_sp800_108_feedback_cmac(const struct krb5_enc_provider *enc, krb5_key key,
                           const krb5_data *label, krb5_data *rnd_out)
{
    krb5_error_code ret;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    OSSL_PARAM params[7], *p = params;
    char *cipher;
    static uint8_t zeroes[16];

    memset(zeroes, 0, sizeof(zeroes));

    cipher = enc_name(enc);
    if (cipher == NULL) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    kdf = EVP_KDF_fetch(NULL, "KBKDF", NULL);
    if (!kdf) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    kctx = EVP_KDF_CTX_new(kdf);
    if (!kctx) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE,
                                            "FEEDBACK", 0);
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MAC,
                                            OSSL_MAC_NAME_CMAC, 0);
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_CIPHER, cipher, 0);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                             key->keyblock.contents,
                                             key->keyblock.length);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                             label->data, label->length);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED,
                                             zeroes, sizeof(zeroes));
    *p = OSSL_PARAM_construct_end();
    if (EVP_KDF_derive(kctx, (uint8_t *)rnd_out->data, rnd_out->length,
                       params) <= 0) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    ret = 0;
done:
    if (ret)
        zap(rnd_out->data, rnd_out->length);
    EVP_KDF_free(kdf);
    EVP_KDF_CTX_free(kctx);
    return ret;
}

krb5_error_code
k5_derive_random_rfc3961(const struct krb5_enc_provider *enc, krb5_key key,
                         const krb5_data *constant, krb5_data *rnd_out)
{
    krb5_error_code ret;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    OSSL_PARAM params[4], *p = params;
    char *cipher;

    if (key->keyblock.length != enc->keylength ||
        rnd_out->length != enc->keybytes) {
        return KRB5_CRYPTO_INTERNAL;
    }

    cipher = enc_name(enc);
    if (cipher == NULL) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    kdf = EVP_KDF_fetch(NULL, "KRB5KDF", NULL);
    if (kdf == NULL) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    kctx = EVP_KDF_CTX_new(kdf);
    if (kctx == NULL) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_CIPHER, cipher, 0);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                             key->keyblock.contents,
                                             key->keyblock.length);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_CONSTANT,
                                             constant->data, constant->length);
    *p = OSSL_PARAM_construct_end();
    if (EVP_KDF_derive(kctx, (uint8_t *)rnd_out->data, rnd_out->length,
                       params) <= 0) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto done;
    }

    ret = 0;
done:
    if (ret)
        zap(rnd_out->data, rnd_out->length);
    EVP_KDF_free(kdf);
    EVP_KDF_CTX_free(kctx);
    return ret;
}

#endif /* K5_OPENSSL_KDF */
