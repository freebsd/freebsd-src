/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/enc_provider/camellia.c */
/*
 * Copyright (C) 2003, 2007, 2008, 2009, 2010 by the Massachusetts Institute of
 * Technology.  All rights reserved.
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

#ifdef K5_OPENSSL_CAMELLIA

#include <openssl/evp.h>
#include <openssl/camellia.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#else
#include <openssl/modes.h>
#endif

static krb5_error_code
cbc_enc(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
        size_t num_data);
static krb5_error_code
cbc_decr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data);
static krb5_error_code
cts_encr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data, size_t dlen);
static krb5_error_code
cts_decr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data, size_t dlen);

#define BLOCK_SIZE 16
#define NUM_BITS 8
#define IV_CTS_BUF_SIZE 16 /* 16 - hardcoded in CRYPTO_cts128_en/decrypt */

static const EVP_CIPHER *
map_mode(unsigned int len)
{
    if (len==16)
        return EVP_camellia_128_cbc();
    if (len==32)
        return EVP_camellia_256_cbc();
    else
        return NULL;
}

/* Encrypt one block using CBC. */
static krb5_error_code
cbc_enc(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
        size_t num_data)
{
    int             ret, olen = BLOCK_SIZE;
    unsigned char   iblock[BLOCK_SIZE], oblock[BLOCK_SIZE];
    EVP_CIPHER_CTX  *ctx;
    struct iov_cursor cursor;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
        return ENOMEM;

    ret = EVP_EncryptInit_ex(ctx, map_mode(key->keyblock.length),
                             NULL, key->keyblock.contents, (ivec) ? (unsigned char*)ivec->data : NULL);
    if (ret == 0) {
        EVP_CIPHER_CTX_free(ctx);
        return KRB5_CRYPTO_INTERNAL;
    }

    k5_iov_cursor_init(&cursor, data, num_data, BLOCK_SIZE, FALSE);
    k5_iov_cursor_get(&cursor, iblock);
    EVP_CIPHER_CTX_set_padding(ctx,0);
    ret = EVP_EncryptUpdate(ctx, oblock, &olen, iblock, BLOCK_SIZE);
    if (ret == 1)
        k5_iov_cursor_put(&cursor, oblock);
    EVP_CIPHER_CTX_free(ctx);

    zap(iblock, BLOCK_SIZE);
    zap(oblock, BLOCK_SIZE);
    return (ret == 1) ? 0 : KRB5_CRYPTO_INTERNAL;
}

/* Decrypt one block using CBC. */
static krb5_error_code
cbc_decr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data)
{
    int              ret = 0, olen = BLOCK_SIZE;
    unsigned char    iblock[BLOCK_SIZE], oblock[BLOCK_SIZE];
    EVP_CIPHER_CTX   *ctx;
    struct iov_cursor cursor;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
        return ENOMEM;

    ret = EVP_DecryptInit_ex(ctx, map_mode(key->keyblock.length),
                             NULL, key->keyblock.contents, (ivec) ? (unsigned char*)ivec->data : NULL);
    if (ret == 0) {
        EVP_CIPHER_CTX_free(ctx);
        return KRB5_CRYPTO_INTERNAL;
    }

    k5_iov_cursor_init(&cursor, data, num_data, BLOCK_SIZE, FALSE);
    k5_iov_cursor_get(&cursor, iblock);
    EVP_CIPHER_CTX_set_padding(ctx,0);
    ret = EVP_DecryptUpdate(ctx, oblock, &olen, iblock, BLOCK_SIZE);
    if (ret == 1)
        k5_iov_cursor_put(&cursor, oblock);
    EVP_CIPHER_CTX_free(ctx);

    zap(iblock, BLOCK_SIZE);
    zap(oblock, BLOCK_SIZE);
    return (ret == 1) ? 0 : KRB5_CRYPTO_INTERNAL;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

static krb5_error_code
do_cts(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
       size_t num_data, size_t dlen, int encrypt)
{
    krb5_error_code ret;
    int outlen, len;
    unsigned char *oblock = NULL, *dbuf = NULL;
    unsigned char iv_cts[IV_CTS_BUF_SIZE];
    struct iov_cursor cursor;
    OSSL_PARAM params[2], *p = params;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER *cipher = NULL;

    memset(iv_cts, 0, sizeof(iv_cts));
    if (ivec != NULL && ivec->data != NULL){
        if (ivec->length != sizeof(iv_cts))
            return KRB5_CRYPTO_INTERNAL;
        memcpy(iv_cts, ivec->data, ivec->length);
    }

    if (key->keyblock.length == 16)
        cipher = EVP_CIPHER_fetch(NULL, "CAMELLIA-128-CBC-CTS", NULL);
    else if (key->keyblock.length == 32)
        cipher = EVP_CIPHER_fetch(NULL, "CAMELLIA-256-CBC-CTS", NULL);
    if (cipher == NULL)
        return KRB5_CRYPTO_INTERNAL;

    oblock = OPENSSL_malloc(dlen);
    dbuf = OPENSSL_malloc(dlen);
    ctx = EVP_CIPHER_CTX_new();
    if (oblock == NULL || dbuf == NULL || ctx == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

    k5_iov_cursor_init(&cursor, data, num_data, dlen, FALSE);
    k5_iov_cursor_get(&cursor, dbuf);

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_CIPHER_PARAM_CTS_MODE,
                                            "CS3", 0);
    *p = OSSL_PARAM_construct_end();
    if (!EVP_CipherInit_ex2(ctx, cipher, key->keyblock.contents, iv_cts,
                            encrypt, params) ||
        !EVP_CipherUpdate(ctx, oblock, &outlen, dbuf, dlen) ||
        !EVP_CipherFinal_ex(ctx, oblock + outlen, &len)) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto cleanup;
    }

    if (ivec != NULL && ivec->data != NULL &&
        !EVP_CIPHER_CTX_get_updated_iv(ctx, ivec->data, sizeof(iv_cts))) {
        ret = KRB5_CRYPTO_INTERNAL;
        goto cleanup;
    }

    k5_iov_cursor_put(&cursor, oblock);

    ret = 0;
cleanup:
    OPENSSL_clear_free(oblock, dlen);
    OPENSSL_clear_free(dbuf, dlen);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
    return ret;
}

static inline krb5_error_code
cts_encr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data, size_t dlen)
{
    return do_cts(key, ivec, data, num_data, dlen, 1);
}

static inline krb5_error_code
cts_decr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data, size_t dlen)
{
    return do_cts(key, ivec, data, num_data, dlen, 0);
}

#else /* OPENSSL_VERSION_NUMBER < 0x30000000L */

static krb5_error_code
cts_encr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data, size_t dlen)
{
    int                    ret = 0;
    size_t                 size = 0;
    unsigned char         *oblock = NULL, *dbuf = NULL;
    unsigned char          iv_cts[IV_CTS_BUF_SIZE];
    struct iov_cursor      cursor;
    CAMELLIA_KEY           enck;

    memset(iv_cts,0,sizeof(iv_cts));
    if (ivec && ivec->data){
        if (ivec->length != sizeof(iv_cts))
            return KRB5_CRYPTO_INTERNAL;
        memcpy(iv_cts, ivec->data,ivec->length);
    }

    oblock = OPENSSL_malloc(dlen);
    if (!oblock){
        return ENOMEM;
    }
    dbuf = OPENSSL_malloc(dlen);
    if (!dbuf){
        OPENSSL_free(oblock);
        return ENOMEM;
    }

    k5_iov_cursor_init(&cursor, data, num_data, dlen, FALSE);
    k5_iov_cursor_get(&cursor, dbuf);

    Camellia_set_key(key->keyblock.contents, NUM_BITS * key->keyblock.length,
                     &enck);

    size = CRYPTO_cts128_encrypt((unsigned char *)dbuf, oblock, dlen, &enck,
                                 iv_cts, (cbc128_f)Camellia_cbc_encrypt);
    if (size <= 0)
        ret = KRB5_CRYPTO_INTERNAL;
    else
        k5_iov_cursor_put(&cursor, oblock);

    if (!ret && ivec && ivec->data)
        memcpy(ivec->data, iv_cts, sizeof(iv_cts));

    zap(oblock, dlen);
    zap(dbuf, dlen);
    OPENSSL_free(oblock);
    OPENSSL_free(dbuf);

    return ret;
}

static krb5_error_code
cts_decr(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
         size_t num_data, size_t dlen)
{
    int                    ret = 0;
    size_t                 size = 0;
    unsigned char         *oblock = NULL;
    unsigned char         *dbuf = NULL;
    unsigned char          iv_cts[IV_CTS_BUF_SIZE];
    struct iov_cursor      cursor;
    CAMELLIA_KEY           deck;

    memset(iv_cts,0,sizeof(iv_cts));
    if (ivec && ivec->data){
        if (ivec->length != sizeof(iv_cts))
            return KRB5_CRYPTO_INTERNAL;
        memcpy(iv_cts, ivec->data,ivec->length);
    }

    oblock = OPENSSL_malloc(dlen);
    if (!oblock)
        return ENOMEM;
    dbuf = OPENSSL_malloc(dlen);
    if (!dbuf){
        OPENSSL_free(oblock);
        return ENOMEM;
    }

    Camellia_set_key(key->keyblock.contents, NUM_BITS * key->keyblock.length,
                     &deck);

    k5_iov_cursor_init(&cursor, data, num_data, dlen, FALSE);
    k5_iov_cursor_get(&cursor, dbuf);

    size = CRYPTO_cts128_decrypt((unsigned char *)dbuf, oblock,
                                 dlen, &deck,
                                 iv_cts, (cbc128_f)Camellia_cbc_encrypt);
    if (size <= 0)
        ret = KRB5_CRYPTO_INTERNAL;
    else
        k5_iov_cursor_put(&cursor, oblock);

    if (!ret && ivec && ivec->data)
        memcpy(ivec->data, iv_cts, sizeof(iv_cts));

    zap(oblock, dlen);
    zap(dbuf, dlen);
    OPENSSL_free(oblock);
    OPENSSL_free(dbuf);

    return ret;
}

#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

krb5_error_code
krb5int_camellia_encrypt(krb5_key key, const krb5_data *ivec,
                         krb5_crypto_iov *data, size_t num_data)
{
    int    ret = 0;
    size_t input_length, nblocks;

    input_length = iov_total_length(data, num_data, FALSE);
    nblocks = (input_length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (nblocks == 1) {
        if (input_length != BLOCK_SIZE)
            return KRB5_BAD_MSIZE;
        ret = cbc_enc(key, ivec, data, num_data);
    } else if (nblocks > 1) {
        ret = cts_encr(key, ivec, data, num_data, input_length);
    }

    return ret;
}

static krb5_error_code
krb5int_camellia_decrypt(krb5_key key, const krb5_data *ivec,
                         krb5_crypto_iov *data, size_t num_data)
{
    int    ret = 0;
    size_t input_length, nblocks;

    input_length = iov_total_length(data, num_data, FALSE);
    nblocks = (input_length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (nblocks == 1) {
        if (input_length != BLOCK_SIZE)
            return KRB5_BAD_MSIZE;
        ret = cbc_decr(key, ivec, data, num_data);
    } else if (nblocks > 1) {
        ret = cts_decr(key, ivec, data, num_data, input_length);
    }

    return ret;
}

#ifdef K5_BUILTIN_CMAC

static void
xorblock(uint8_t *out, const uint8_t *in)
{
    int z;

    for (z = 0; z < CAMELLIA_BLOCK_SIZE / 4; z++) {
        uint8_t *outptr = &out[z * 4];
        const uint8_t *inptr = &in[z * 4];

        store_32_n(load_32_n(outptr) ^ load_32_n(inptr), outptr);
    }
}

static krb5_error_code
krb5int_camellia_cbc_mac(krb5_key key, const krb5_crypto_iov *data,
                         size_t num_data, const krb5_data *iv,
                         krb5_data *output)
{
    CAMELLIA_KEY enck;
    unsigned char blockY[CAMELLIA_BLOCK_SIZE], blockB[CAMELLIA_BLOCK_SIZE];
    struct iov_cursor cursor;

    if (output->length < CAMELLIA_BLOCK_SIZE)
        return KRB5_BAD_MSIZE;

    Camellia_set_key(key->keyblock.contents,
                     NUM_BITS * key->keyblock.length, &enck);

    if (iv != NULL)
        memcpy(blockY, iv->data, CAMELLIA_BLOCK_SIZE);
    else
        memset(blockY, 0, CAMELLIA_BLOCK_SIZE);

    k5_iov_cursor_init(&cursor, data, num_data, CAMELLIA_BLOCK_SIZE, FALSE);
    while (k5_iov_cursor_get(&cursor, blockB)) {
        xorblock(blockB, blockY);
        Camellia_ecb_encrypt(blockB, blockY, &enck, 1);
    }

    output->length = CAMELLIA_BLOCK_SIZE;
    memcpy(output->data, blockY, CAMELLIA_BLOCK_SIZE);

    return 0;
}

#else
#define krb5int_camellia_cbc_mac NULL
#endif

static krb5_error_code
krb5int_camellia_init_state (const krb5_keyblock *key, krb5_keyusage usage,
                             krb5_data *state)
{
    state->length = 16;
    state->data = (void *) malloc(16);
    if (state->data == NULL)
        return ENOMEM;
    memset(state->data, 0, state->length);
    return 0;
}
const struct krb5_enc_provider krb5int_enc_camellia128 = {
    16,
    16, 16,
    krb5int_camellia_encrypt,
    krb5int_camellia_decrypt,
    krb5int_camellia_cbc_mac,   /* NULL if K5_BUILTIN_CMAC not defined */
    krb5int_camellia_init_state,
    krb5int_default_free_state
};

const struct krb5_enc_provider krb5int_enc_camellia256 = {
    16,
    32, 32,
    krb5int_camellia_encrypt,
    krb5int_camellia_decrypt,
    krb5int_camellia_cbc_mac,   /* NULL if K5_BUILTIN_CMAC not defined */
    krb5int_camellia_init_state,
    krb5int_default_free_state
};

#endif /* K5_OPENSSL_CAMELLIA */
