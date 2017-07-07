/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/des/f_tables.h */
/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology.
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
 * DES implementation donated by Dennis Ferguson
 */

/*
 * des_tables.h - declarations to import the DES tables, used internally
 *                by some of the library routines.
 */
#ifndef __DES_TABLES_H__
#define __DES_TABLES_H__        /* nothing */

#include "k5-platform.h"
/*
 * These may be declared const if you wish.  Be sure to change the
 * declarations in des_tables.c as well.
 */
extern const unsigned DES_INT32 des_IP_table[256];
extern const unsigned DES_INT32 des_FP_table[256];
extern const unsigned DES_INT32 des_SP_table[8][64];

/*
 * Use standard shortforms to reference these to save typing
 */
#define IP      des_IP_table
#define FP      des_FP_table
#define SP      des_SP_table

#ifdef DEBUG
#define DEB(foofraw)    printf foofraw
#else
#define DEB(foofraw)    /* nothing */
#endif

/*
 * Code to do a DES round using the tables.  Note that the E expansion
 * is easy to compute algorithmically, especially if done out-of-order.
 * Take a look at its form and compare it to everything involving temp
 * below.  Since SP[0-7] don't have any bits in common set it is okay
 * to do the successive xor's.
 *
 * Note too that the SP table has been reordered to match the order of
 * the keys (if the original order of SP was 12345678, the reordered
 * table is 71354682).  This is unnecessary, but was done since some
 * compilers seem to like you going through the matrix from beginning
 * to end.
 *
 * There is a difference in the best way to do this depending on whether
 * one is encrypting or decrypting.  If encrypting we move forward through
 * the keys and hence should move forward through the table.  If decrypting
 * we go back.  Part of the need for this comes from trying to emulate
 * existing software which generates a single key schedule and uses it
 * both for encrypting and decrypting.  Generating separate encryption
 * and decryption key schedules would allow one to use the same code
 * for both.
 *
 * left, right and temp should be unsigned DES_INT32 values.  left and right
 * should be the high and low order parts of the cipher block at the
 * current stage of processing (this makes sense if you read the spec).
 * kp should be an unsigned DES_INT32 pointer which points at the current
 * set of subkeys in the key schedule.  It is advanced to the next set
 * (i.e. by 8 bytes) when this is done.
 *
 * This occurs in the innermost loop of the DES function.  The four
 * variables should really be in registers.
 *
 * When using this, the inner loop of the DES function might look like:
 *
 *      for (i = 0; i < 8; i++) {
 *              DES_SP_{EN,DE}CRYPT_ROUND(left, right, temp, kp);
 *              DES_SP_{EN,DE}CRYPT_ROUND(right, left, temp, kp);
 *      }
 *
 * Note the trick above.  You are supposed to do 16 rounds, swapping
 * left and right at the end of each round.  By doing two rounds at
 * a time and swapping left and right in the code we can avoid the
 * swaps altogether.
 */
#define DES_SP_ENCRYPT_ROUND(left, right, temp, kp) do {        \
        (temp) = (((right) >> 11) | ((right) << 21)) ^ *(kp)++; \
        (left) ^= SP[0][((temp) >> 24) & 0x3f]                  \
            | SP[1][((temp) >> 16) & 0x3f]                      \
            | SP[2][((temp) >>  8) & 0x3f]                      \
            | SP[3][((temp)      ) & 0x3f];                     \
        (temp) = (((right) >> 23) | ((right) << 9)) ^ *(kp)++;  \
        (left) ^= SP[4][((temp) >> 24) & 0x3f]                  \
            | SP[5][((temp) >> 16) & 0x3f]                      \
            | SP[6][((temp) >>  8) & 0x3f]                      \
            | SP[7][((temp)      ) & 0x3f];                     \
    } while(0);

#define DES_SP_DECRYPT_ROUND(left, right, temp, kp) do {                \
        (temp) = (((right) >> 23) | ((right) << 9)) ^ *(--(kp));        \
        (left) ^= SP[7][((temp)      ) & 0x3f]                          \
            | SP[6][((temp) >>  8) & 0x3f]                              \
            | SP[5][((temp) >> 16) & 0x3f]                              \
            | SP[4][((temp) >> 24) & 0x3f];                             \
        (temp) = (((right) >> 11) | ((right) << 21)) ^ *(--(kp));       \
        (left) ^= SP[3][((temp)      ) & 0x3f]                          \
            | SP[2][((temp) >>  8) & 0x3f]                              \
            | SP[1][((temp) >> 16) & 0x3f]                              \
            | SP[0][((temp) >> 24) & 0x3f];                             \
    } while (0);

/*
 * Macros to help deal with the initial permutation table.  Note
 * the IP table only deals with 32 bits at a time, allowing us to
 * collect the bits we need to deal with each half into an unsigned
 * DES_INT32.  By carefully selecting how the bits are ordered we also
 * take advantages of symmetries in the table so that we can use a
 * single table to compute the permutation of all bytes.  This sounds
 * complicated, but if you go through the process of designing the
 * table you'll find the symmetries fall right out.
 *
 * The follow macros compute the set of bits used to index the
 * table for produce the left and right permuted result.
 *
 * The inserted cast to unsigned DES_INT32 circumvents a bug in
 * the Macintosh MPW 3.2 C compiler which loses the unsignedness and
 * propagates the high-order bit in the shift.
 */
#define DES_IP_LEFT_BITS(left, right)                           \
    ((((left) & 0x55555555) << 1) | ((right) & 0x55555555))
#define DES_IP_RIGHT_BITS(left, right)                          \
    (((left) & 0xaaaaaaaa) |                                    \
     ( ( (unsigned DES_INT32) ((right) & 0xaaaaaaaa) ) >> 1))

/*
 * The following macro does an in-place initial permutation given
 * the current left and right parts of the block and a single
 * temporary.  Use this more as a guide for rolling your own, though.
 * The best way to do the IP depends on the form of the data you
 * are dealing with.  If you use this, though, try to make left,
 * right and temp register unsigned DES_INT32s.
 */
#define DES_INITIAL_PERM(left, right, temp) do {        \
        (temp) = DES_IP_RIGHT_BITS((left), (right));    \
        (right) = DES_IP_LEFT_BITS((left), (right));    \
        (left) = IP[((right) >> 24) & 0xff]             \
            | (IP[((right) >> 16) & 0xff] << 1)         \
            | (IP[((right) >>  8) & 0xff] << 2)         \
            | (IP[(right) & 0xff] << 3);                \
        (right) = IP[((temp) >> 24) & 0xff]             \
            | (IP[((temp) >> 16) & 0xff] << 1)          \
            | (IP[((temp) >>  8) & 0xff] << 2)          \
            | (IP[(temp) & 0xff] << 3);                 \
    } while(0);

/*
 * Now the final permutation stuff.  The same comments apply to
 * this as to the initial permutation, except that we use different
 * bits and shifts.
 *
 * The inserted cast to unsigned DES_INT32 circumvents a bug in
 * the Macintosh MPW 3.2 C compiler which loses the unsignedness and
 * propagates the high-order bit in the shift.
 */
#define DES_FP_LEFT_BITS(left, right)                           \
    ((((left) & 0x0f0f0f0f) << 4) | ((right) & 0x0f0f0f0f))
#define DES_FP_RIGHT_BITS(left, right)                          \
    (((left) & 0xf0f0f0f0) |                                    \
     ( ( (unsigned DES_INT32) ((right) & 0xf0f0f0f0) ) >> 4))


/*
 * Here is a sample final permutation.  Note that there is a trick
 * here.  DES requires swapping the left and right parts after the
 * last cipher round but before the final permutation.  We do this
 * swapping internally, which is why left and right are confused
 * at the beginning.
 */
#define DES_FINAL_PERM(left, right, temp) do {          \
        (temp) = DES_FP_RIGHT_BITS((right), (left));    \
        (right) = DES_FP_LEFT_BITS((right), (left));    \
        (left) = (FP[((right) >> 24) & 0xff] << 6)      \
            | (FP[((right) >> 16) & 0xff] << 4)         \
            | (FP[((right) >>  8) & 0xff] << 2)         \
            |  FP[(right) & 0xff];                      \
        (right) = (FP[((temp) >> 24) & 0xff] << 6)      \
            | (FP[((temp) >> 16) & 0xff] << 4)          \
            | (FP[((temp) >>  8) & 0xff] << 2)          \
            |  FP[temp & 0xff];                         \
    } while(0);


/*
 * Finally, as a sample of how all this might be held together, the
 * following two macros do in-place encryptions and decryptions.  left
 * and right are two unsigned DES_INT32 variables which at the beginning
 * are expected to hold the clear (encrypted) block in host byte order
 * (left the high order four bytes, right the low order).  At the end
 * they will contain the encrypted (clear) block.  temp is an unsigned DES_INT32
 * used as a temporary.  kp is an unsigned DES_INT32 pointer pointing at
 * the start of the key schedule.  All these should be in registers.
 *
 * You can probably do better than these by rewriting for particular
 * situations.  These aren't bad, though.
 *
 * The DEB macros enable debugging when this code breaks (typically
 * when a buggy compiler breaks it), by printing the intermediate values
 * at each stage of the encryption, so that by comparing the output to
 * a known good machine, the location of the first error can be found.
 */
#define DES_DO_ENCRYPT_1(left, right, kp)                               \
    do {                                                                \
        register int i;                                                 \
        register unsigned DES_INT32 temp1;                              \
        DEB (("do_encrypt %8lX %8lX \n", left, right));                 \
        DES_INITIAL_PERM((left), (right), (temp1));                     \
        DEB (("  after IP %8lX %8lX\n", left, right));                  \
        for (i = 0; i < 8; i++) {                                       \
            DES_SP_ENCRYPT_ROUND((left), (right), (temp1), (kp));       \
            DEB (("  round %2d %8lX %8lX \n", i*2, left, right));       \
            DES_SP_ENCRYPT_ROUND((right), (left), (temp1), (kp));       \
            DEB (("  round %2d %8lX %8lX \n", 1+i*2, left, right));     \
        }                                                               \
        DES_FINAL_PERM((left), (right), (temp1));                       \
        (kp) -= (2 * 16);                                               \
        DEB (("  after FP %8lX %8lX \n", left, right));                 \
    } while (0)

#define DES_DO_DECRYPT_1(left, right, kp)                               \
    do {                                                                \
        register int i;                                                 \
        register unsigned DES_INT32 temp2;                              \
        DES_INITIAL_PERM((left), (right), (temp2));                     \
        (kp) += (2 * 16);                                               \
        for (i = 0; i < 8; i++) {                                       \
            DES_SP_DECRYPT_ROUND((left), (right), (temp2), (kp));       \
            DES_SP_DECRYPT_ROUND((right), (left), (temp2), (kp));       \
        }                                                               \
        DES_FINAL_PERM((left), (right), (temp2));                       \
    } while (0)

#if defined(CONFIG_SMALL) && !defined(CONFIG_SMALL_NO_CRYPTO)
extern void krb5int_des_do_encrypt_2(unsigned DES_INT32 *l,
                                     unsigned DES_INT32 *r,
                                     const unsigned DES_INT32 *k);
extern void krb5int_des_do_decrypt_2(unsigned DES_INT32 *l,
                                     unsigned DES_INT32 *r,
                                     const unsigned DES_INT32 *k);
#define DES_DO_ENCRYPT(L,R,K) krb5int_des_do_encrypt_2(&(L), &(R), (K))
#define DES_DO_DECRYPT(L,R,K) krb5int_des_do_decrypt_2(&(L), &(R), (K))
#else
#define DES_DO_ENCRYPT DES_DO_ENCRYPT_1
#define DES_DO_DECRYPT DES_DO_DECRYPT_1
#endif

/*
 * These are handy dandy utility thingies for straightening out bytes.
 * Included here because they're used a couple of places.
 */
#define GET_HALF_BLOCK(lr, ip)  ((lr) = load_32_be(ip), (ip) += 4)
#define PUT_HALF_BLOCK(lr, op)  (store_32_be(lr, op), (op) += 4)

/* Shorthand that we'll need in several places, for creating values that
   really can hold 32 bits regardless of the prevailing int size.  */
#define FF_UINT32       ((unsigned DES_INT32) 0xFF)

#endif  /* __DES_TABLES_H__ */
