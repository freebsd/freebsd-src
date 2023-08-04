/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/enc_provider/aes.c */
/*
 * Copyright (C) 2003, 2007, 2008 by the Massachusetts Institute of Technology.
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

#include "crypto_int.h"
#include "aes.h"

#ifdef K5_BUILTIN_AES

/*
 * Private per-key data to cache after first generation.  We don't
 * want to mess with the imported AES implementation too much, so
 * we'll just use two copies of its context, one for encryption and
 * one for decryption, and use the #rounds field as a flag for whether
 * we've initialized each half.
 */
struct aes_key_info_cache {
    aes_encrypt_ctx enc_ctx;
    aes_decrypt_ctx dec_ctx;
    krb5_boolean aesni;
};
#define CACHE(X) ((struct aes_key_info_cache *)((X)->cache))

#ifdef AESNI

/* Use AES-NI instructions (via assembly functions) when possible. */

#include <cpuid.h>

struct aes_data
{
    unsigned char *in_block;
    unsigned char *out_block;
    uint32_t *expanded_key;
    unsigned char *iv;
    size_t num_blocks;
};

void k5_iEncExpandKey128(unsigned char *key, uint32_t *expanded_key);
void k5_iEncExpandKey256(unsigned char *key, uint32_t *expanded_key);
void k5_iDecExpandKey256(unsigned char *key, uint32_t *expanded_key);
void k5_iDecExpandKey128(unsigned char *key, uint32_t *expanded_key);

void k5_iEnc128_CBC(struct aes_data *data);
void k5_iDec128_CBC(struct aes_data *data);
void k5_iEnc256_CBC(struct aes_data *data);
void k5_iDec256_CBC(struct aes_data *data);

static krb5_boolean
aesni_supported_by_cpu()
{
    unsigned int a, b, c, d;

    return __get_cpuid(1, &a, &b, &c, &d) && (c & (1 << 25));
}

static inline krb5_boolean
aesni_supported(krb5_key key)
{
    return CACHE(key)->aesni;
}

static void
aesni_expand_enc_key(krb5_key key)
{
    struct aes_key_info_cache *cache = CACHE(key);

    if (key->keyblock.length == 16)
        k5_iEncExpandKey128(key->keyblock.contents, cache->enc_ctx.ks);
    else
        k5_iEncExpandKey256(key->keyblock.contents, cache->enc_ctx.ks);
    cache->enc_ctx.inf.l = 1;
}

static void
aesni_expand_dec_key(krb5_key key)
{
    struct aes_key_info_cache *cache = CACHE(key);

    if (key->keyblock.length == 16)
        k5_iDecExpandKey128(key->keyblock.contents, cache->dec_ctx.ks);
    else
        k5_iDecExpandKey256(key->keyblock.contents, cache->dec_ctx.ks);
    cache->dec_ctx.inf.l = 1;
}

static inline void
aesni_enc(krb5_key key, unsigned char *data, size_t nblocks, unsigned char *iv)
{
    struct aes_key_info_cache *cache = CACHE(key);
    struct aes_data d;

    d.in_block = data;
    d.out_block = data;
    d.expanded_key = cache->enc_ctx.ks;
    d.iv = iv;
    d.num_blocks = nblocks;
    if (key->keyblock.length == 16)
        k5_iEnc128_CBC(&d);
    else
        k5_iEnc256_CBC(&d);
}

static inline void
aesni_dec(krb5_key key, unsigned char *data, size_t nblocks, unsigned char *iv)
{
    struct aes_key_info_cache *cache = CACHE(key);
    struct aes_data d;

    d.in_block = data;
    d.out_block = data;
    d.expanded_key = cache->dec_ctx.ks;
    d.iv = iv;
    d.num_blocks = nblocks;
    if (key->keyblock.length == 16)
        k5_iDec128_CBC(&d);
    else
        k5_iDec256_CBC(&d);
}

#else /* not AESNI */

#define aesni_supported_by_cpu() FALSE
#define aesni_supported(key) FALSE
#define aesni_expand_enc_key(key)
#define aesni_expand_dec_key(key)
#define aesni_enc(key, data, nblocks, iv)
#define aesni_dec(key, data, nblocks, iv)

#endif

/* out = out ^ in */
static inline void
xorblock(const unsigned char *in, unsigned char *out)
{
    size_t q;

    for (q = 0; q < AES_BLOCK_SIZE; q += 4)
        store_32_n(load_32_n(out + q) ^ load_32_n(in + q), out + q);
}

static inline krb5_error_code
init_key_cache(krb5_key key)
{
    if (key->cache != NULL)
        return 0;
    key->cache = malloc(sizeof(struct aes_key_info_cache));
    if (key->cache == NULL)
        return ENOMEM;
    CACHE(key)->enc_ctx.inf.l = CACHE(key)->dec_ctx.inf.l = 0;
    CACHE(key)->aesni = aesni_supported_by_cpu();
    return 0;
}

static inline void
expand_enc_key(krb5_key key)
{
    if (CACHE(key)->enc_ctx.inf.l != 0)
        return;
    if (aesni_supported(key))
        aesni_expand_enc_key(key);
    else if (aes_encrypt_key(key->keyblock.contents, key->keyblock.length,
                             &CACHE(key)->enc_ctx) != EXIT_SUCCESS)
        abort();
}

static inline void
expand_dec_key(krb5_key key)
{
    if (CACHE(key)->dec_ctx.inf.l != 0)
        return;
    if (aesni_supported(key))
        aesni_expand_dec_key(key);
    else if (aes_decrypt_key(key->keyblock.contents, key->keyblock.length,
                             &CACHE(key)->dec_ctx) != EXIT_SUCCESS)
        abort();
}

/* CBC encrypt nblocks blocks of data in place, using and updating iv. */
static inline void
cbc_enc(krb5_key key, unsigned char *data, size_t nblocks, unsigned char *iv)
{
    if (aesni_supported(key)) {
        aesni_enc(key, data, nblocks, iv);
        return;
    }
    for (; nblocks > 0; nblocks--, data += AES_BLOCK_SIZE) {
        xorblock(iv, data);
        if (aes_encrypt(data, data, &CACHE(key)->enc_ctx) != EXIT_SUCCESS)
            abort();
        memcpy(iv, data, AES_BLOCK_SIZE);
    }
}

/* CBC decrypt nblocks blocks of data in place, using and updating iv. */
static inline void
cbc_dec(krb5_key key, unsigned char *data, size_t nblocks, unsigned char *iv)
{
    unsigned char last_cipherblock[AES_BLOCK_SIZE];

    if (aesni_supported(key)) {
        aesni_dec(key, data, nblocks, iv);
        return;
    }
    assert(nblocks > 0);
    data += (nblocks - 1) * AES_BLOCK_SIZE;
    memcpy(last_cipherblock, data, AES_BLOCK_SIZE);
    for (; nblocks > 0; nblocks--, data -= AES_BLOCK_SIZE) {
        if (aes_decrypt(data, data, &CACHE(key)->dec_ctx) != EXIT_SUCCESS)
            abort();
        xorblock(nblocks == 1 ? iv : data - AES_BLOCK_SIZE, data);
    }
    memcpy(iv, last_cipherblock, AES_BLOCK_SIZE);
}

krb5_error_code
krb5int_aes_encrypt(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
                    size_t num_data)
{
    unsigned char iv[AES_BLOCK_SIZE], block[AES_BLOCK_SIZE];
    unsigned char blockN2[AES_BLOCK_SIZE], blockN1[AES_BLOCK_SIZE];
    size_t input_length, nblocks, ncontig;
    struct iov_cursor cursor;

    if (init_key_cache(key))
        return ENOMEM;
    expand_enc_key(key);

    k5_iov_cursor_init(&cursor, data, num_data, AES_BLOCK_SIZE, FALSE);

    input_length = iov_total_length(data, num_data, FALSE);
    nblocks = (input_length + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
    if (nblocks == 1) {
        k5_iov_cursor_get(&cursor, block);
        memset(iv, 0, AES_BLOCK_SIZE);
        cbc_enc(key, block, 1, iv);
        k5_iov_cursor_put(&cursor, block);
        return 0;
    }

    if (ivec != NULL)
        memcpy(iv, ivec->data, AES_BLOCK_SIZE);
    else
        memset(iv, 0, AES_BLOCK_SIZE);

    while (nblocks > 2) {
        ncontig = iov_cursor_contig_blocks(&cursor);
        if (ncontig > 0) {
            /* Encrypt a series of contiguous blocks in place if we can, but
             * don't touch the last two blocks. */
            ncontig = (ncontig > nblocks - 2) ? nblocks - 2 : ncontig;
            cbc_enc(key, iov_cursor_ptr(&cursor), ncontig, iv);
            iov_cursor_advance(&cursor, ncontig);
            nblocks -= ncontig;
        } else {
            k5_iov_cursor_get(&cursor, block);
            cbc_enc(key, block, 1, iv);
            k5_iov_cursor_put(&cursor, block);
            nblocks--;
        }
    }

    /* Encrypt the last two blocks and put them back in reverse order, possibly
     * truncating the encrypted second-to-last block. */
    k5_iov_cursor_get(&cursor, blockN2);
    k5_iov_cursor_get(&cursor, blockN1);
    cbc_enc(key, blockN2, 1, iv);
    cbc_enc(key, blockN1, 1, iv);
    k5_iov_cursor_put(&cursor, blockN1);
    k5_iov_cursor_put(&cursor, blockN2);

    if (ivec != NULL)
        memcpy(ivec->data, iv, AES_BLOCK_SIZE);

    return 0;
}

krb5_error_code
krb5int_aes_decrypt(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
                    size_t num_data)
{
    unsigned char iv[AES_BLOCK_SIZE], dummy_iv[AES_BLOCK_SIZE];
    unsigned char block[AES_BLOCK_SIZE];
    unsigned char blockN2[AES_BLOCK_SIZE], blockN1[AES_BLOCK_SIZE];
    size_t input_length, last_len, nblocks, ncontig;
    struct iov_cursor cursor;

    if (init_key_cache(key))
        return ENOMEM;
    expand_dec_key(key);

    k5_iov_cursor_init(&cursor, data, num_data, AES_BLOCK_SIZE, FALSE);

    input_length = iov_total_length(data, num_data, FALSE);
    nblocks = (input_length + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
    last_len = input_length - (nblocks - 1) * AES_BLOCK_SIZE;
    if (nblocks == 1) {
        k5_iov_cursor_get(&cursor, block);
        memset(iv, 0, AES_BLOCK_SIZE);
        cbc_dec(key, block, 1, iv);
        k5_iov_cursor_put(&cursor, block);
        return 0;
    }

    if (ivec != NULL)
        memcpy(iv, ivec->data, AES_BLOCK_SIZE);
    else
        memset(iv, 0, AES_BLOCK_SIZE);

    while (nblocks > 2) {
        ncontig = iov_cursor_contig_blocks(&cursor);
        if (ncontig > 0) {
            /* Decrypt a series of contiguous blocks in place if we can, but
             * don't touch the last two blocks. */
            ncontig = (ncontig > nblocks - 2) ? nblocks - 2 : ncontig;
            cbc_dec(key, iov_cursor_ptr(&cursor), ncontig, iv);
            iov_cursor_advance(&cursor, ncontig);
            nblocks -= ncontig;
        } else {
            k5_iov_cursor_get(&cursor, block);
            cbc_dec(key, block, 1, iv);
            k5_iov_cursor_put(&cursor, block);
            nblocks--;
        }
    }

    /* Get the last two ciphertext blocks.  Save the first as the new iv. */
    k5_iov_cursor_get(&cursor, blockN2);
    k5_iov_cursor_get(&cursor, blockN1);
    if (ivec != NULL)
        memcpy(ivec->data, blockN2, AES_BLOCK_SIZE);

    /* Decrypt the second-to-last ciphertext block, using the final ciphertext
     * block as the CBC IV.  This produces the final plaintext block. */
    memcpy(dummy_iv, blockN1, sizeof(dummy_iv));
    cbc_dec(key, blockN2, 1, dummy_iv);

    /* Use the final bits of the decrypted plaintext to pad the last ciphertext
     * block, and decrypt it to produce the second-to-last plaintext block. */
    memcpy(blockN1 + last_len, blockN2 + last_len, AES_BLOCK_SIZE - last_len);
    cbc_dec(key, blockN1, 1, iv);

    /* Put the last two plaintext blocks back into the iovec. */
    k5_iov_cursor_put(&cursor, blockN1);
    k5_iov_cursor_put(&cursor, blockN2);

    return 0;
}

static krb5_error_code
aes_init_state(const krb5_keyblock *key, krb5_keyusage usage,
               krb5_data *state)
{
    state->length = 16;
    state->data = malloc(16);
    if (state->data == NULL)
        return ENOMEM;
    memset(state->data, 0, state->length);
    return 0;
}

static void
aes_key_cleanup(krb5_key key)
{
    zapfree(key->cache, sizeof(struct aes_key_info_cache));
}

const struct krb5_enc_provider krb5int_enc_aes128 = {
    16,
    16, 16,
    krb5int_aes_encrypt,
    krb5int_aes_decrypt,
    NULL,
    aes_init_state,
    krb5int_default_free_state,
    aes_key_cleanup
};

const struct krb5_enc_provider krb5int_enc_aes256 = {
    16,
    32, 32,
    krb5int_aes_encrypt,
    krb5int_aes_decrypt,
    NULL,
    aes_init_state,
    krb5int_default_free_state,
    aes_key_cleanup
};

#endif /* K5_BUILTIN_AES */
