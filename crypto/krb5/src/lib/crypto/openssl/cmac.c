/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/cmac.c - OpenSSL CMAC implementation */
/*
 * Copyright (C) 2021 by the Massachusetts Institute of Technology.
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

#ifdef K5_OPENSSL_CMAC

#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/core_names.h>

krb5_error_code
krb5int_cmac_checksum(const struct krb5_enc_provider *enc, krb5_key key,
                      const krb5_crypto_iov *data, size_t num_data,
                      krb5_data *output)
{
    int ok;
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2], *p = params;
    size_t i = 0, md_len;
    char *cipher;

    if (enc == &krb5int_enc_camellia128)
        cipher = "CAMELLIA-128-CBC";
    else if (enc == &krb5int_enc_camellia256)
        cipher = "CAMELLIA-256-CBC";
    else
        return KRB5_CRYPTO_INTERNAL;

    mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
    if (mac == NULL)
        return KRB5_CRYPTO_INTERNAL;

    ctx = EVP_MAC_CTX_new(mac);
    if (ctx == NULL) {
        ok = 0;
        goto cleanup;
    }

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_ALG_PARAM_CIPHER, cipher, 0);
    *p = OSSL_PARAM_construct_end();

    ok = EVP_MAC_init(ctx, key->keyblock.contents, key->keyblock.length,
                      params);
    for (i = 0; ok && i < num_data; i++) {
        const krb5_crypto_iov *iov = &data[i];
        if (!SIGN_IOV(iov))
            continue;
        ok = EVP_MAC_update(ctx, (uint8_t *)iov->data.data, iov->data.length);
    }
    ok = ok && EVP_MAC_final(ctx, (unsigned char *)output->data, &md_len,
                             output->length);
    if (!ok)
        goto cleanup;
    output->length = md_len;

cleanup:
    EVP_MAC_free(mac);
    EVP_MAC_CTX_free(ctx);
    return ok ? 0 : KRB5_CRYPTO_INTERNAL;
}

#endif /* K5_OPENSSL_CMAC */
