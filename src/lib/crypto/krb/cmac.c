/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/cmac.c */
/*
 * Copyright 2010 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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

#define BLOCK_SIZE 16

static unsigned char const_Rb[BLOCK_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

static void
xor_128(unsigned char *a, unsigned char *b, unsigned char *out)
{
    int z;

    for (z = 0; z < BLOCK_SIZE / 4; z++) {
        unsigned char *aptr = &a[z * 4];
        unsigned char *bptr = &b[z * 4];
        unsigned char *outptr = &out[z * 4];

        store_32_n(load_32_n(aptr) ^ load_32_n(bptr), outptr);
    }
}

static void
leftshift_onebit(unsigned char *input, unsigned char *output)
{
    int i;
    unsigned char overflow = 0;

    for (i = BLOCK_SIZE - 1; i >= 0; i--) {
        output[i] = input[i] << 1;
        output[i] |= overflow;
        overflow = (input[i] & 0x80) ? 1 : 0;
    }
}

/* Generate subkeys K1 and K2 as described in RFC 4493 figure 2.2. */
static krb5_error_code
generate_subkey(const struct krb5_enc_provider *enc,
                krb5_key key,
                unsigned char *K1,
                unsigned char *K2)
{
    unsigned char L[BLOCK_SIZE];
    unsigned char tmp[BLOCK_SIZE];
    krb5_data d;
    krb5_error_code ret;

    /* L := encrypt(K, const_Zero) */
    memset(L, 0, sizeof(L));
    d = make_data(L, BLOCK_SIZE);
    ret = encrypt_block(enc, key, &d);
    if (ret != 0)
        return ret;

    /* K1 := (MSB(L) == 0) ? L << 1 : (L << 1) XOR const_Rb */
    if ((L[0] & 0x80) == 0) {
        leftshift_onebit(L, K1);
    } else {
        leftshift_onebit(L, tmp);
        xor_128(tmp, const_Rb, K1);
    }

    /* K2 := (MSB(K1) == 0) ? K1 << 1 : (K1 << 1) XOR const_Rb */
    if ((K1[0] & 0x80) == 0) {
        leftshift_onebit(K1, K2);
    } else {
        leftshift_onebit(K1, tmp);
        xor_128(tmp, const_Rb, K2);
    }

    return 0;
}

/* Pad out lastb with a 1 bit followed by 0 bits, placing the result in pad. */
static void
padding(unsigned char *lastb, unsigned char *pad, int length)
{
    int j;

    /* original last block */
    for (j = 0; j < BLOCK_SIZE; j++) {
        if (j < length) {
            pad[j] = lastb[j];
        } else if (j == length) {
            pad[j] = 0x80;
        } else {
            pad[j] = 0x00;
        }
    }
}

/*
 * Implementation of CMAC algorithm. When used with AES, this function
 * is compatible with RFC 4493 figure 2.3.
 */
krb5_error_code
krb5int_cmac_checksum(const struct krb5_enc_provider *enc, krb5_key key,
                      const krb5_crypto_iov *data, size_t num_data,
                      krb5_data *output)
{
    unsigned char Y[BLOCK_SIZE], M_last[BLOCK_SIZE], padded[BLOCK_SIZE];
    unsigned char K1[BLOCK_SIZE], K2[BLOCK_SIZE];
    unsigned char input[BLOCK_SIZE];
    unsigned int n, i, flag;
    krb5_error_code ret;
    struct iov_cursor cursor;
    size_t length;
    krb5_crypto_iov iov[1];
    krb5_data d;

    assert(enc->cbc_mac != NULL);

    if (enc->block_size != BLOCK_SIZE)
        return KRB5_BAD_MSIZE;

    length = iov_total_length(data, num_data, TRUE);

    /* Step 1. */
    ret = generate_subkey(enc, key, K1, K2);
    if (ret != 0)
        return ret;

    /* Step 2. */
    n = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    /* Step 3. */
    if (n == 0) {
        n = 1;
        flag = 0;
    } else {
        flag = ((length % BLOCK_SIZE) == 0);
    }

    iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[0].data = make_data(input, BLOCK_SIZE);

    /* Step 5 (we'll do step 4 in a bit). */
    memset(Y, 0, BLOCK_SIZE);
    d = make_data(Y, BLOCK_SIZE);

    /* Step 6 (all but last block). */
    k5_iov_cursor_init(&cursor, data, num_data, BLOCK_SIZE, TRUE);
    for (i = 0; i < n - 1; i++) {
        k5_iov_cursor_get(&cursor, input);

        ret = enc->cbc_mac(key, iov, 1, &d, &d);
        if (ret != 0)
            return ret;
    }

    /* Step 4. */
    k5_iov_cursor_get(&cursor, input);
    if (flag) {
        /* last block is complete block */
        xor_128(input, K1, M_last);
    } else {
        padding(input, padded, length % BLOCK_SIZE);
        xor_128(padded, K2, M_last);
    }

    /* Step 6 (last block). */
    iov[0].data = make_data(M_last, BLOCK_SIZE);
    ret = enc->cbc_mac(key, iov, 1, &d, &d);
    if (ret != 0)
        return ret;

    assert(output->length >= d.length);

    output->length = d.length;
    memcpy(output->data, d.data, d.length);

    return 0;
}
