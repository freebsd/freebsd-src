/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/des/f_sched.c */
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

/* DES implementation donated by Dennis Ferguson */

/*
 * des_make_sched.c - permute a DES key, returning the resulting key schedule
 */
#include "k5-int.h"
#include "des_int.h"

/*
 * Permuted choice 1 tables.  These are used to extract bits
 * from the left and right parts of the key to form Ci and Di.
 * The code that uses these tables knows which bits from which
 * part of each key are used to form Ci and Di.
 */
static const unsigned DES_INT32 PC1_CL[8] = {
    0x00000000, 0x00000010, 0x00001000, 0x00001010,
    0x00100000, 0x00100010, 0x00101000, 0x00101010
};

static const unsigned DES_INT32 PC1_DL[16] = {
    0x00000000, 0x00100000, 0x00001000, 0x00101000,
    0x00000010, 0x00100010, 0x00001010, 0x00101010,
    0x00000001, 0x00100001, 0x00001001, 0x00101001,
    0x00000011, 0x00100011, 0x00001011, 0x00101011
};

static const unsigned DES_INT32 PC1_CR[16] = {
    0x00000000, 0x00000001, 0x00000100, 0x00000101,
    0x00010000, 0x00010001, 0x00010100, 0x00010101,
    0x01000000, 0x01000001, 0x01000100, 0x01000101,
    0x01010000, 0x01010001, 0x01010100, 0x01010101
};

static const unsigned DES_INT32 PC1_DR[8] = {
    0x00000000, 0x01000000, 0x00010000, 0x01010000,
    0x00000100, 0x01000100, 0x00010100, 0x01010100
};


/*
 * At the start of some iterations of the key schedule we do
 * a circular left shift by one place, while for others we do a shift by
 * two places.  This has bits set for the iterations where we do 2 bit
 * shifts, starting at the low order bit.
 */
#define TWO_BIT_SHIFTS  0x7efc

/*
 * Permuted choice 2 tables.  The first actually produces the low order
 * 24 bits of the subkey Ki from the 28 bit value of Ci.  The second produces
 * the high order 24 bits from Di.  The tables are indexed by six bit
 * segments of Ci and Di respectively.  The code is handcrafted to compute
 * the appropriate 6 bit chunks.
 *
 * Note that for ease of computation, the 24 bit values are produced with
 * six bits going into each byte.  Note also that the table has been byte
 * rearranged to produce keys which match the order we will apply them
 * in in the des code.
 */
static const unsigned DES_INT32 PC2_C[4][64] = {
    {
        0x00000000, 0x00000004, 0x00010000, 0x00010004,
        0x00000400, 0x00000404, 0x00010400, 0x00010404,
        0x00000020, 0x00000024, 0x00010020, 0x00010024,
        0x00000420, 0x00000424, 0x00010420, 0x00010424,
        0x01000000, 0x01000004, 0x01010000, 0x01010004,
        0x01000400, 0x01000404, 0x01010400, 0x01010404,
        0x01000020, 0x01000024, 0x01010020, 0x01010024,
        0x01000420, 0x01000424, 0x01010420, 0x01010424,
        0x00020000, 0x00020004, 0x00030000, 0x00030004,
        0x00020400, 0x00020404, 0x00030400, 0x00030404,
        0x00020020, 0x00020024, 0x00030020, 0x00030024,
        0x00020420, 0x00020424, 0x00030420, 0x00030424,
        0x01020000, 0x01020004, 0x01030000, 0x01030004,
        0x01020400, 0x01020404, 0x01030400, 0x01030404,
        0x01020020, 0x01020024, 0x01030020, 0x01030024,
        0x01020420, 0x01020424, 0x01030420, 0x01030424,
    },
    {
        0x00000000, 0x02000000, 0x00000800, 0x02000800,
        0x00080000, 0x02080000, 0x00080800, 0x02080800,
        0x00000001, 0x02000001, 0x00000801, 0x02000801,
        0x00080001, 0x02080001, 0x00080801, 0x02080801,
        0x00000100, 0x02000100, 0x00000900, 0x02000900,
        0x00080100, 0x02080100, 0x00080900, 0x02080900,
        0x00000101, 0x02000101, 0x00000901, 0x02000901,
        0x00080101, 0x02080101, 0x00080901, 0x02080901,
        0x10000000, 0x12000000, 0x10000800, 0x12000800,
        0x10080000, 0x12080000, 0x10080800, 0x12080800,
        0x10000001, 0x12000001, 0x10000801, 0x12000801,
        0x10080001, 0x12080001, 0x10080801, 0x12080801,
        0x10000100, 0x12000100, 0x10000900, 0x12000900,
        0x10080100, 0x12080100, 0x10080900, 0x12080900,
        0x10000101, 0x12000101, 0x10000901, 0x12000901,
        0x10080101, 0x12080101, 0x10080901, 0x12080901,
    },
    {
        0x00000000, 0x00040000, 0x00002000, 0x00042000,
        0x00100000, 0x00140000, 0x00102000, 0x00142000,
        0x20000000, 0x20040000, 0x20002000, 0x20042000,
        0x20100000, 0x20140000, 0x20102000, 0x20142000,
        0x00000008, 0x00040008, 0x00002008, 0x00042008,
        0x00100008, 0x00140008, 0x00102008, 0x00142008,
        0x20000008, 0x20040008, 0x20002008, 0x20042008,
        0x20100008, 0x20140008, 0x20102008, 0x20142008,
        0x00200000, 0x00240000, 0x00202000, 0x00242000,
        0x00300000, 0x00340000, 0x00302000, 0x00342000,
        0x20200000, 0x20240000, 0x20202000, 0x20242000,
        0x20300000, 0x20340000, 0x20302000, 0x20342000,
        0x00200008, 0x00240008, 0x00202008, 0x00242008,
        0x00300008, 0x00340008, 0x00302008, 0x00342008,
        0x20200008, 0x20240008, 0x20202008, 0x20242008,
        0x20300008, 0x20340008, 0x20302008, 0x20342008,
    },
    {
        0x00000000, 0x00000010, 0x08000000, 0x08000010,
        0x00000200, 0x00000210, 0x08000200, 0x08000210,
        0x00000002, 0x00000012, 0x08000002, 0x08000012,
        0x00000202, 0x00000212, 0x08000202, 0x08000212,
        0x04000000, 0x04000010, 0x0c000000, 0x0c000010,
        0x04000200, 0x04000210, 0x0c000200, 0x0c000210,
        0x04000002, 0x04000012, 0x0c000002, 0x0c000012,
        0x04000202, 0x04000212, 0x0c000202, 0x0c000212,
        0x00001000, 0x00001010, 0x08001000, 0x08001010,
        0x00001200, 0x00001210, 0x08001200, 0x08001210,
        0x00001002, 0x00001012, 0x08001002, 0x08001012,
        0x00001202, 0x00001212, 0x08001202, 0x08001212,
        0x04001000, 0x04001010, 0x0c001000, 0x0c001010,
        0x04001200, 0x04001210, 0x0c001200, 0x0c001210,
        0x04001002, 0x04001012, 0x0c001002, 0x0c001012,
        0x04001202, 0x04001212, 0x0c001202, 0x0c001212
    },
};

static const unsigned DES_INT32 PC2_D[4][64] = {
    {
        0x00000000, 0x02000000, 0x00020000, 0x02020000,
        0x00000100, 0x02000100, 0x00020100, 0x02020100,
        0x00000008, 0x02000008, 0x00020008, 0x02020008,
        0x00000108, 0x02000108, 0x00020108, 0x02020108,
        0x00200000, 0x02200000, 0x00220000, 0x02220000,
        0x00200100, 0x02200100, 0x00220100, 0x02220100,
        0x00200008, 0x02200008, 0x00220008, 0x02220008,
        0x00200108, 0x02200108, 0x00220108, 0x02220108,
        0x00000200, 0x02000200, 0x00020200, 0x02020200,
        0x00000300, 0x02000300, 0x00020300, 0x02020300,
        0x00000208, 0x02000208, 0x00020208, 0x02020208,
        0x00000308, 0x02000308, 0x00020308, 0x02020308,
        0x00200200, 0x02200200, 0x00220200, 0x02220200,
        0x00200300, 0x02200300, 0x00220300, 0x02220300,
        0x00200208, 0x02200208, 0x00220208, 0x02220208,
        0x00200308, 0x02200308, 0x00220308, 0x02220308,
    },
    {
        0x00000000, 0x00001000, 0x00000020, 0x00001020,
        0x00100000, 0x00101000, 0x00100020, 0x00101020,
        0x08000000, 0x08001000, 0x08000020, 0x08001020,
        0x08100000, 0x08101000, 0x08100020, 0x08101020,
        0x00000004, 0x00001004, 0x00000024, 0x00001024,
        0x00100004, 0x00101004, 0x00100024, 0x00101024,
        0x08000004, 0x08001004, 0x08000024, 0x08001024,
        0x08100004, 0x08101004, 0x08100024, 0x08101024,
        0x00000400, 0x00001400, 0x00000420, 0x00001420,
        0x00100400, 0x00101400, 0x00100420, 0x00101420,
        0x08000400, 0x08001400, 0x08000420, 0x08001420,
        0x08100400, 0x08101400, 0x08100420, 0x08101420,
        0x00000404, 0x00001404, 0x00000424, 0x00001424,
        0x00100404, 0x00101404, 0x00100424, 0x00101424,
        0x08000404, 0x08001404, 0x08000424, 0x08001424,
        0x08100404, 0x08101404, 0x08100424, 0x08101424,
    },
    {
        0x00000000, 0x10000000, 0x00010000, 0x10010000,
        0x00000002, 0x10000002, 0x00010002, 0x10010002,
        0x00002000, 0x10002000, 0x00012000, 0x10012000,
        0x00002002, 0x10002002, 0x00012002, 0x10012002,
        0x00040000, 0x10040000, 0x00050000, 0x10050000,
        0x00040002, 0x10040002, 0x00050002, 0x10050002,
        0x00042000, 0x10042000, 0x00052000, 0x10052000,
        0x00042002, 0x10042002, 0x00052002, 0x10052002,
        0x20000000, 0x30000000, 0x20010000, 0x30010000,
        0x20000002, 0x30000002, 0x20010002, 0x30010002,
        0x20002000, 0x30002000, 0x20012000, 0x30012000,
        0x20002002, 0x30002002, 0x20012002, 0x30012002,
        0x20040000, 0x30040000, 0x20050000, 0x30050000,
        0x20040002, 0x30040002, 0x20050002, 0x30050002,
        0x20042000, 0x30042000, 0x20052000, 0x30052000,
        0x20042002, 0x30042002, 0x20052002, 0x30052002,
    },
    {
        0x00000000, 0x04000000, 0x00000001, 0x04000001,
        0x01000000, 0x05000000, 0x01000001, 0x05000001,
        0x00000010, 0x04000010, 0x00000011, 0x04000011,
        0x01000010, 0x05000010, 0x01000011, 0x05000011,
        0x00080000, 0x04080000, 0x00080001, 0x04080001,
        0x01080000, 0x05080000, 0x01080001, 0x05080001,
        0x00080010, 0x04080010, 0x00080011, 0x04080011,
        0x01080010, 0x05080010, 0x01080011, 0x05080011,
        0x00000800, 0x04000800, 0x00000801, 0x04000801,
        0x01000800, 0x05000800, 0x01000801, 0x05000801,
        0x00000810, 0x04000810, 0x00000811, 0x04000811,
        0x01000810, 0x05000810, 0x01000811, 0x05000811,
        0x00080800, 0x04080800, 0x00080801, 0x04080801,
        0x01080800, 0x05080800, 0x01080801, 0x05080801,
        0x00080810, 0x04080810, 0x00080811, 0x04080811,
        0x01080810, 0x05080810, 0x01080811, 0x05080811
    },
};



/*
 * Permute the key to give us our key schedule.
 */
int
mit_des_make_key_sched(mit_des_cblock key, mit_des_key_schedule schedule)
{
    register unsigned DES_INT32 c, d;

    {
        /*
         * Need a pointer for the keys and a temporary DES_INT32
         */
        const unsigned char *k;
        register unsigned DES_INT32 tmp;

        /*
         * Fetch the key into something we can work with
         */
        k = key;

        /*
         * The first permutted choice gives us the 28 bits for C0 and
         * 28 for D0.  C0 gets 12 bits from the left key and 16 from
         * the right, while D0 gets 16 from the left and 12 from the
         * right.  The code knows which bits go where.
         */
        tmp = load_32_be(k), k += 4;

        c =  PC1_CL[(tmp >> 29) & 0x7]
            | (PC1_CL[(tmp >> 21) & 0x7] << 1)
            | (PC1_CL[(tmp >> 13) & 0x7] << 2)
            | (PC1_CL[(tmp >>  5) & 0x7] << 3);
        d =  PC1_DL[(tmp >> 25) & 0xf]
            | (PC1_DL[(tmp >> 17) & 0xf] << 1)
            | (PC1_DL[(tmp >>  9) & 0xf] << 2)
            | (PC1_DL[(tmp >>  1) & 0xf] << 3);

        tmp = load_32_be(k), k += 4;

        c |= PC1_CR[(tmp >> 28) & 0xf]
            | (PC1_CR[(tmp >> 20) & 0xf] << 1)
            | (PC1_CR[(tmp >> 12) & 0xf] << 2)
            | (PC1_CR[(tmp >>  4) & 0xf] << 3);
        d |= PC1_DR[(tmp >> 25) & 0x7]
            | (PC1_DR[(tmp >> 17) & 0x7] << 1)
            | (PC1_DR[(tmp >>  9) & 0x7] << 2)
            | (PC1_DR[(tmp >>  1) & 0x7] << 3);
    }

    {
        /*
         * Need several temporaries in here
         */
        register unsigned DES_INT32 ltmp, rtmp;
        register unsigned DES_INT32 *k;
        register int two_bit_shifts;
        register int i;
        /*
         * Now iterate to compute the key schedule.  Note that we
         * record the entire set of subkeys in 6 bit chunks since
         * they are used that way.  At 6 bits/char, we need
         * 48/6 char's/subkey * 16 subkeys/encryption == 128 bytes.
         * The schedule must be this big.
         */
        k = (unsigned DES_INT32 *)schedule;
        two_bit_shifts = TWO_BIT_SHIFTS;
        for (i = 16; i > 0; i--) {
            /*
             * Do the rotation.  One bit and two bit rotations
             * are done separately.  Note C and D are 28 bits.
             */
            if (two_bit_shifts & 0x1) {
                c = ((c << 2) & 0xffffffc) | (c >> 26);
                d = ((d << 2) & 0xffffffc) | (d >> 26);
            } else {
                c = ((c << 1) & 0xffffffe) | (c >> 27);
                d = ((d << 1) & 0xffffffe) | (d >> 27);
            }
            two_bit_shifts >>= 1;

            /*
             * Apply permutted choice 2 to C to get the first
             * 24 bits worth of keys.  Note that bits 9, 18, 22
             * and 25 (using DES numbering) in C are unused.  The
             * shift-mask stuff is done to delete these bits from
             * the indices, since this cuts the table size in half.
             *
             * The table is torqued, by the way.  If the standard
             * byte order for this (high to low order) is 1234,
             * the table actually gives us 4132.
             */
            ltmp = PC2_C[0][((c >> 22) & 0x3f)]
                | PC2_C[1][((c >> 15) & 0xf) | ((c >> 16) & 0x30)]
                | PC2_C[2][((c >>  4) & 0x3) | ((c >>  9) & 0x3c)]
                | PC2_C[3][((c      ) & 0x7) | ((c >>  4) & 0x38)];
            /*
             * Apply permutted choice 2 to D to get the other half.
             * Here, bits 7, 10, 15 and 26 go unused.  The sqeezing
             * actually turns out to be cheaper here.
             *
             * This table is similarly torqued.  If the standard
             * byte order is 5678, the table has the bytes permuted
             * to give us 7685.
             */
            rtmp = PC2_D[0][((d >> 22) & 0x3f)]
                | PC2_D[1][((d >> 14) & 0xf) | ((d >> 15) & 0x30)]
                | PC2_D[2][((d >>  7) & 0x3f)]
                | PC2_D[3][((d      ) & 0x3) | ((d >>  1) & 0x3c)];

            /*
             * Make up two words of the key schedule, with a
             * byte order which is convenient for the DES
             * inner loop.  The high order (first) word will
             * hold bytes 7135 (high to low order) while the
             * second holds bytes 4682.
             */
            *k++ = (ltmp & 0x00ffff00) | (rtmp & 0xff0000ff);
            *k++ = (ltmp & 0xff0000ff) | (rtmp & 0x00ffff00);
        }
    }
    return (0);
}
