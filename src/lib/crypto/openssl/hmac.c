/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/hmac.c */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#ifdef K5_OPENSSL_HMAC

#include <openssl/evp.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/params.h>
#include <openssl/core_names.h>
#else
#include <openssl/hmac.h>
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L

/* OpenSSL 1.1 makes HMAC_CTX opaque, while 1.0 does not have pointer
 * constructors or destructors. */

#define HMAC_CTX_new compat_hmac_ctx_new
static HMAC_CTX *
compat_hmac_ctx_new()
{
    HMAC_CTX *ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx != NULL)
        HMAC_CTX_init(ctx);
    return ctx;
}

#define HMAC_CTX_free compat_hmac_ctx_free
static void
compat_hmac_ctx_free(HMAC_CTX *ctx)
{
    HMAC_CTX_cleanup(ctx);
    free(ctx);
}

#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

/*
 * the HMAC transform looks like:
 *
 * H(K XOR opad, H(K XOR ipad, text))
 *
 * where H is a cryptographic hash
 * K is an n byte key
 * ipad is the byte 0x36 repeated blocksize times
 * opad is the byte 0x5c repeated blocksize times
 * and text is the data being protected
 */

static const EVP_MD *
map_digest(const struct krb5_hash_provider *hash)
{
    if (hash == &krb5int_hash_sha1)
        return EVP_sha1();
    else if (hash == &krb5int_hash_sha256)
        return EVP_sha256();
    else if (hash == &krb5int_hash_sha384)
        return EVP_sha384();
    else if (hash == &krb5int_hash_md5)
        return EVP_md5();
    else if (hash == &krb5int_hash_md4)
        return EVP_md4();
    else
        return NULL;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

krb5_error_code
krb5int_hmac_keyblock(const struct krb5_hash_provider *hash,
                      const krb5_keyblock *keyblock,
                      const krb5_crypto_iov *data, size_t num_data,
                      krb5_data *output)
{
    int ok;
    const EVP_MD *md = map_digest(hash);
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2], *p = params;
    size_t i = 0, md_len;

    if (md == NULL || keyblock->length > hash->blocksize)
        return KRB5_CRYPTO_INTERNAL;
    if (output->length < hash->hashsize)
        return KRB5_BAD_MSIZE;

    mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (mac == NULL)
        return KRB5_CRYPTO_INTERNAL;

    ctx = EVP_MAC_CTX_new(mac);
    if (ctx == NULL) {
        ok = 0;
        goto cleanup;
    }

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_ALG_PARAM_DIGEST,
                                            (char *)EVP_MD_get0_name(md), 0);
    *p = OSSL_PARAM_construct_end();

    ok = EVP_MAC_init(ctx, keyblock->contents, keyblock->length, params);
    for (i = 0; ok && i < num_data; i++) {
        const krb5_crypto_iov *iov = &data[i];
        if (!SIGN_IOV(iov))
            continue;
        ok = EVP_MAC_update(ctx, (uint8_t *)iov->data.data, iov->data.length);
    }
    ok = ok && EVP_MAC_final(ctx, (uint8_t *)output->data, &md_len,
                             output->length);
    if (!ok)
        goto cleanup;
    output->length = md_len;

cleanup:
    EVP_MAC_free(mac);
    EVP_MAC_CTX_free(ctx);
    return ok ? 0 : KRB5_CRYPTO_INTERNAL;
}

#else /* OPENSSL_VERSION_NUMBER < 0x30000000L */

krb5_error_code
krb5int_hmac_keyblock(const struct krb5_hash_provider *hash,
                      const krb5_keyblock *keyblock,
                      const krb5_crypto_iov *data, size_t num_data,
                      krb5_data *output)
{
    unsigned int i = 0, md_len = 0, ok;
    unsigned char md[EVP_MAX_MD_SIZE];
    HMAC_CTX *ctx;
    size_t hashsize, blocksize;

    hashsize = hash->hashsize;
    blocksize = hash->blocksize;

    if (keyblock->length > blocksize)
        return(KRB5_CRYPTO_INTERNAL);
    if (output->length < hashsize)
        return(KRB5_BAD_MSIZE);

    if (!map_digest(hash))
        return(KRB5_CRYPTO_INTERNAL); // unsupported alg

    ctx = HMAC_CTX_new();
    if (ctx == NULL)
        return ENOMEM;

    ok = HMAC_Init_ex(ctx, keyblock->contents, keyblock->length,
                      map_digest(hash), NULL);
    for (i = 0; ok && i < num_data; i++) {
        const krb5_crypto_iov *iov = &data[i];

        if (SIGN_IOV(iov))
            ok = HMAC_Update(ctx, (uint8_t *)iov->data.data, iov->data.length);
    }
    if (ok)
        ok = HMAC_Final(ctx, md, &md_len);
    if (ok && md_len <= output->length) {
        output->length = md_len;
        memcpy(output->data, md, output->length);
    }
    HMAC_CTX_free(ctx);
    return ok ? 0 : KRB5_CRYPTO_INTERNAL;
}

#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

krb5_error_code
krb5int_hmac(const struct krb5_hash_provider *hash, krb5_key key,
             const krb5_crypto_iov *data, size_t num_data,
             krb5_data *output)
{
    return krb5int_hmac_keyblock(hash, &key->keyblock, data, num_data, output);
}

#endif /* K5_OPENSSL_HMAC */
