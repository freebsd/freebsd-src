/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/enc_provider/camellia.c - Camellia enc provider */
/*
 * Copyright (C) 2009, 2010 by the Massachusetts Institute of Technology.
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
#include "camellia.h"

#ifdef K5_BUILTIN_CAMELLIA

/*
 * Private per-key data to cache after first generation.  We don't want to mess
 * with the imported Camellia implementation too much, so we'll just use two
 * copies of its context, one for encryption and one for decryption, and use
 * the keybitlen field as a flag for whether we've initialized each half.
 */
struct camellia_key_info_cache {
    camellia_ctx enc_ctx, dec_ctx;
};
#define CACHE(X) ((struct camellia_key_info_cache *)((X)->cache))

/* out = out ^ in */
static inline void
xorblock(const unsigned char *in, unsigned char *out)
{
    size_t q;

    for (q = 0; q < BLOCK_SIZE; q += 4)
        store_32_n(load_32_n(out + q) ^ load_32_n(in + q), out + q);
}

static inline krb5_error_code
init_key_cache(krb5_key key)
{
    if (key->cache != NULL)
        return 0;
    key->cache = malloc(sizeof(struct camellia_key_info_cache));
    if (key->cache == NULL)
        return ENOMEM;
    CACHE(key)->enc_ctx.keybitlen = CACHE(key)->dec_ctx.keybitlen = 0;
    return 0;
}

static inline void
expand_enc_key(krb5_key key)
{
    if (CACHE(key)->enc_ctx.keybitlen)
        return;
    if (camellia_enc_key(key->keyblock.contents, key->keyblock.length,
                         &CACHE(key)->enc_ctx) != camellia_good)
        abort();
}

static inline void
expand_dec_key(krb5_key key)
{
    if (CACHE(key)->dec_ctx.keybitlen)
        return;
    if (camellia_dec_key(key->keyblock.contents, key->keyblock.length,
                         &CACHE(key)->dec_ctx) != camellia_good)
        abort();
}

/* CBC encrypt nblocks blocks of data in place, using and updating iv. */
static inline void
cbc_enc(krb5_key key, unsigned char *data, size_t nblocks, unsigned char *iv)
{
    for (; nblocks > 0; nblocks--, data += BLOCK_SIZE) {
        xorblock(iv, data);
        if (camellia_enc_blk(data, data, &CACHE(key)->enc_ctx) !=
            camellia_good)
            abort();
        memcpy(iv, data, BLOCK_SIZE);
    }
}

/* CBC decrypt nblocks blocks of data in place, using and updating iv. */
static inline void
cbc_dec(krb5_key key, unsigned char *data, size_t nblocks, unsigned char *iv)
{
    unsigned char last_cipherblock[BLOCK_SIZE];

    assert(nblocks > 0);
    data += (nblocks - 1) * BLOCK_SIZE;
    memcpy(last_cipherblock, data, BLOCK_SIZE);
    for (; nblocks > 0; nblocks--, data -= BLOCK_SIZE) {
        if (camellia_dec_blk(data, data, &CACHE(key)->dec_ctx) !=
            camellia_good)
            abort();
        xorblock(nblocks == 1 ? iv : data - BLOCK_SIZE, data);
    }
    memcpy(iv, last_cipherblock, BLOCK_SIZE);
}

krb5_error_code
krb5int_camellia_encrypt(krb5_key key, const krb5_data *ivec,
                         krb5_crypto_iov *data, size_t num_data)
{
    unsigned char iv[BLOCK_SIZE], block[BLOCK_SIZE];
    unsigned char blockN2[BLOCK_SIZE], blockN1[BLOCK_SIZE];
    size_t input_length, nblocks, ncontig;
    struct iov_cursor cursor;

    if (init_key_cache(key))
        return ENOMEM;
    expand_enc_key(key);

    k5_iov_cursor_init(&cursor, data, num_data, BLOCK_SIZE, FALSE);

    input_length = iov_total_length(data, num_data, FALSE);
    nblocks = (input_length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (nblocks == 1) {
        k5_iov_cursor_get(&cursor, block);
        memset(iv, 0, BLOCK_SIZE);
        cbc_enc(key, block, 1, iv);
        k5_iov_cursor_put(&cursor, block);
        return 0;
    }

    if (ivec != NULL)
        memcpy(iv, ivec->data, BLOCK_SIZE);
    else
        memset(iv, 0, BLOCK_SIZE);

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
        memcpy(ivec->data, iv, BLOCK_SIZE);

    return 0;
}

static krb5_error_code
krb5int_camellia_decrypt(krb5_key key, const krb5_data *ivec,
                         krb5_crypto_iov *data, size_t num_data)
{
    unsigned char iv[BLOCK_SIZE], dummy_iv[BLOCK_SIZE], block[BLOCK_SIZE];
    unsigned char blockN2[BLOCK_SIZE], blockN1[BLOCK_SIZE];
    size_t input_length, last_len, nblocks, ncontig;
    struct iov_cursor cursor;

    if (init_key_cache(key))
        return ENOMEM;
    expand_dec_key(key);

    k5_iov_cursor_init(&cursor, data, num_data, BLOCK_SIZE, FALSE);

    input_length = iov_total_length(data, num_data, FALSE);
    nblocks = (input_length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    last_len = input_length - (nblocks - 1) * BLOCK_SIZE;
    if (nblocks == 1) {
        k5_iov_cursor_get(&cursor, block);
        memset(iv, 0, BLOCK_SIZE);
        cbc_dec(key, block, 1, iv);
        k5_iov_cursor_put(&cursor, block);
        return 0;
    }

    if (ivec != NULL)
        memcpy(iv, ivec->data, BLOCK_SIZE);
    else
        memset(iv, 0, BLOCK_SIZE);

    while (nblocks > 2) {
        ncontig = iov_cursor_contig_blocks(&cursor);
        if (ncontig > 0) {
            /* Encrypt a series of contiguous blocks in place if we can, but
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
        memcpy(ivec->data, blockN2, BLOCK_SIZE);

    /* Decrypt the second-to-last ciphertext block, using the final ciphertext
     * block as the CBC IV.  This produces the final plaintext block. */
    memcpy(dummy_iv, blockN1, sizeof(dummy_iv));
    cbc_dec(key, blockN2, 1, dummy_iv);

    /* Use the final bits of the decrypted plaintext to pad the last ciphertext
     * block, and decrypt it to produce the second-to-last plaintext block. */
    memcpy(blockN1 + last_len, blockN2 + last_len, BLOCK_SIZE - last_len);
    cbc_dec(key, blockN1, 1, iv);

    /* Put the last two plaintext blocks back into the iovec. */
    k5_iov_cursor_put(&cursor, blockN1);
    k5_iov_cursor_put(&cursor, blockN2);

    return 0;
}

static krb5_error_code
krb5int_camellia_cbc_mac(krb5_key key, const krb5_crypto_iov *data,
                         size_t num_data, const krb5_data *ivec,
                         krb5_data *output)
{
    unsigned char iv[BLOCK_SIZE], block[BLOCK_SIZE];
    struct iov_cursor cursor;

    if (output->length < BLOCK_SIZE)
        return KRB5_BAD_MSIZE;

    if (init_key_cache(key))
        return ENOMEM;
    expand_enc_key(key);

    if (ivec != NULL)
        memcpy(iv, ivec->data, BLOCK_SIZE);
    else
        memset(iv, 0, BLOCK_SIZE);

    k5_iov_cursor_init(&cursor, data, num_data, BLOCK_SIZE, FALSE);
    while (k5_iov_cursor_get(&cursor, block))
        cbc_enc(key, block, 1, iv);

    output->length = BLOCK_SIZE;
    memcpy(output->data, iv, BLOCK_SIZE);

    return 0;
}

static krb5_error_code
camellia_init_state(const krb5_keyblock *key, krb5_keyusage usage,
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
camellia_key_cleanup(krb5_key key)
{
    zapfree(key->cache, sizeof(struct camellia_key_info_cache));
}

const struct krb5_enc_provider krb5int_enc_camellia128 = {
    16,
    16, 16,
    krb5int_camellia_encrypt,
    krb5int_camellia_decrypt,
    krb5int_camellia_cbc_mac,
    camellia_init_state,
    krb5int_default_free_state,
    camellia_key_cleanup
};

const struct krb5_enc_provider krb5int_enc_camellia256 = {
    16,
    32, 32,
    krb5int_camellia_encrypt,
    krb5int_camellia_decrypt,
    krb5int_camellia_cbc_mac,
    camellia_init_state,
    krb5int_default_free_state,
    camellia_key_cleanup
};

#endif /* K5_BUILTIN_CAMELLIA */
