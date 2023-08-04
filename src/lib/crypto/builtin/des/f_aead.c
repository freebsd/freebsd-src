/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2008 by the Massachusetts Institute of Technology.
 * Copyright 1995 by Richard P. Basch.  All Rights Reserved.
 * Copyright 1995 by Lehman Brothers, Inc.  All Rights Reserved.
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
 * the name of Richard P. Basch, Lehman Brothers and M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Richard P. Basch,
 * Lehman Brothers and M.I.T. make no representations about the suitability
 * of this software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "crypto_int.h"
#include "des_int.h"
#include "f_tables.h"

#ifdef K5_BUILTIN_DES

const mit_des_cblock mit_des_zeroblock /* = all zero */;

void
krb5int_des_cbc_encrypt(krb5_crypto_iov *data, unsigned long num_data,
                        const mit_des_key_schedule schedule,
                        mit_des_cblock ivec)
{
    unsigned DES_INT32 left, right;
    const unsigned DES_INT32 *kp;
    const unsigned char *ip;
    struct iov_cursor cursor;
    unsigned char block[MIT_DES_BLOCK_LENGTH];

    /* Get key pointer here.  This won't need to be reinitialized. */
    kp = (const unsigned DES_INT32 *)schedule;

    /* Initialize left and right with the contents of the initial vector. */
    ip = (ivec != NULL) ? ivec : mit_des_zeroblock;
    left = load_32_be(ip);
    right = load_32_be(ip + 4);

    k5_iov_cursor_init(&cursor, data, num_data, MIT_DES_BLOCK_LENGTH, FALSE);
    while (k5_iov_cursor_get(&cursor, block)) {
        /* Decompose this block and xor it with the previous ciphertext. */
        left ^= load_32_be(block);
        right ^= load_32_be(block + 4);

        /* Encrypt what we have and put back into block. */
        DES_DO_ENCRYPT(left, right, kp);
        store_32_be(left, block);
        store_32_be(right, block + 4);

        k5_iov_cursor_put(&cursor, block);
    }

    if (ivec != NULL) {
        store_32_be(left, ivec);
        store_32_be(right, ivec + 4);
    }
}

void
krb5int_des_cbc_decrypt(krb5_crypto_iov *data, unsigned long num_data,
                        const mit_des_key_schedule schedule,
                        mit_des_cblock ivec)
{
    unsigned DES_INT32 left, right;
    const unsigned DES_INT32 *kp;
    const unsigned char *ip;
    unsigned DES_INT32 ocipherl, ocipherr;
    unsigned DES_INT32 cipherl, cipherr;
    struct iov_cursor cursor;
    unsigned char block[MIT_DES_BLOCK_LENGTH];

    /* Get key pointer here.  This won't need to be reinitialized. */
    kp = (const unsigned DES_INT32 *)schedule;

    /*
     * Decrypting is harder than encrypting because of
     * the necessity of remembering a lot more things.
     * Should think about this a little more...
     */

    /* Prime the old cipher with ivec. */
    ip = (ivec != NULL) ? ivec : mit_des_zeroblock;
    ocipherl = load_32_be(ip);
    ocipherr = load_32_be(ip + 4);

    k5_iov_cursor_init(&cursor, data, num_data, MIT_DES_BLOCK_LENGTH, FALSE);
    while (k5_iov_cursor_get(&cursor, block)) {
        /* Split this block into left and right. */
        cipherl = left = load_32_be(block);
        cipherr = right = load_32_be(block + 4);

        /* Decrypt and xor with the old cipher to get plain text. */
        DES_DO_DECRYPT(left, right, kp);
        left ^= ocipherl;
        right ^= ocipherr;

        /* Store the encrypted halves back into block. */
        store_32_be(left, block);
        store_32_be(right, block + 4);

        /* Save current cipher block halves. */
        ocipherl = cipherl;
        ocipherr = cipherr;

        k5_iov_cursor_put(&cursor, block);
    }

    if (ivec != NULL) {
        store_32_be(ocipherl, ivec);
        store_32_be(ocipherr, ivec + 4);
    }
}

void
krb5int_des_cbc_mac(const krb5_crypto_iov *data, unsigned long num_data,
                    const mit_des_key_schedule schedule, mit_des_cblock ivec,
                    mit_des_cblock out)
{
    unsigned DES_INT32 left, right;
    const unsigned DES_INT32 *kp;
    const unsigned char *ip;
    struct iov_cursor cursor;
    unsigned char block[MIT_DES_BLOCK_LENGTH];

    /* Get key pointer here.  This won't need to be reinitialized. */
    kp = (const unsigned DES_INT32 *)schedule;

    /* Initialize left and right with the contents of the initial vector. */
    ip = (ivec != NULL) ? ivec : mit_des_zeroblock;
    left = load_32_be(ip);
    right = load_32_be(ip + 4);

    k5_iov_cursor_init(&cursor, data, num_data, MIT_DES_BLOCK_LENGTH, TRUE);
    while (k5_iov_cursor_get(&cursor, block)) {
        /* Decompose this block and xor it with the previous ciphertext. */
        left ^= load_32_be(block);
        right ^= load_32_be(block + 4);

        /* Encrypt what we have. */
        DES_DO_ENCRYPT(left, right, kp);
    }

    /* Output the final ciphertext block. */
    store_32_be(left, out);
    store_32_be(right, out + 4);
}

#if defined(CONFIG_SMALL) && !defined(CONFIG_SMALL_NO_CRYPTO)
void krb5int_des_do_encrypt_2 (unsigned DES_INT32 *left,
                               unsigned DES_INT32 *right,
                               const unsigned DES_INT32 *kp)
{
    DES_DO_ENCRYPT_1 (*left, *right, kp);
}

void krb5int_des_do_decrypt_2 (unsigned DES_INT32 *left,
                               unsigned DES_INT32 *right,
                               const unsigned DES_INT32 *kp)
{
    DES_DO_DECRYPT_1 (*left, *right, kp);
}
#endif

#endif /* K5_BUILTIN_DES */
