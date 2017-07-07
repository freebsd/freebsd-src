/*
 * Copyright (c) 2001, Dr Brian Gladman <brg@gladman.uk.net>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explcit or implied warranties
 * in respect of any properties, including, but not limited to, correctness
 * and fitness for purpose.
 */

/* Issue Date: 07/02/2002 */

#include "aesopt.h"

#if defined(FIXED_TABLES) || !defined(FF_TABLES)

/*  finite field arithmetic operations */

#define f2(x)   ((x<<1) ^ (((x>>7) & 1) * WPOLY))
#define f4(x)   ((x<<2) ^ (((x>>6) & 1) * WPOLY) ^ (((x>>6) & 2) * WPOLY))
#define f8(x)   ((x<<3) ^ (((x>>5) & 1) * WPOLY) ^ (((x>>5) & 2) * WPOLY) \
                        ^ (((x>>5) & 4) * WPOLY))
#define f3(x)   (f2(x) ^ x)
#define f9(x)   (f8(x) ^ x)
#define fb(x)   (f8(x) ^ f2(x) ^ x)
#define fd(x)   (f8(x) ^ f4(x) ^ x)
#define fe(x)   (f8(x) ^ f4(x) ^ f2(x))

#endif

#if defined(FIXED_TABLES)

#define sb_data(w) \
    w(0x63), w(0x7c), w(0x77), w(0x7b), w(0xf2), w(0x6b), w(0x6f), w(0xc5),\
    w(0x30), w(0x01), w(0x67), w(0x2b), w(0xfe), w(0xd7), w(0xab), w(0x76),\
    w(0xca), w(0x82), w(0xc9), w(0x7d), w(0xfa), w(0x59), w(0x47), w(0xf0),\
    w(0xad), w(0xd4), w(0xa2), w(0xaf), w(0x9c), w(0xa4), w(0x72), w(0xc0),\
    w(0xb7), w(0xfd), w(0x93), w(0x26), w(0x36), w(0x3f), w(0xf7), w(0xcc),\
    w(0x34), w(0xa5), w(0xe5), w(0xf1), w(0x71), w(0xd8), w(0x31), w(0x15),\
    w(0x04), w(0xc7), w(0x23), w(0xc3), w(0x18), w(0x96), w(0x05), w(0x9a),\
    w(0x07), w(0x12), w(0x80), w(0xe2), w(0xeb), w(0x27), w(0xb2), w(0x75),\
    w(0x09), w(0x83), w(0x2c), w(0x1a), w(0x1b), w(0x6e), w(0x5a), w(0xa0),\
    w(0x52), w(0x3b), w(0xd6), w(0xb3), w(0x29), w(0xe3), w(0x2f), w(0x84),\
    w(0x53), w(0xd1), w(0x00), w(0xed), w(0x20), w(0xfc), w(0xb1), w(0x5b),\
    w(0x6a), w(0xcb), w(0xbe), w(0x39), w(0x4a), w(0x4c), w(0x58), w(0xcf),\
    w(0xd0), w(0xef), w(0xaa), w(0xfb), w(0x43), w(0x4d), w(0x33), w(0x85),\
    w(0x45), w(0xf9), w(0x02), w(0x7f), w(0x50), w(0x3c), w(0x9f), w(0xa8),\
    w(0x51), w(0xa3), w(0x40), w(0x8f), w(0x92), w(0x9d), w(0x38), w(0xf5),\
    w(0xbc), w(0xb6), w(0xda), w(0x21), w(0x10), w(0xff), w(0xf3), w(0xd2),\
    w(0xcd), w(0x0c), w(0x13), w(0xec), w(0x5f), w(0x97), w(0x44), w(0x17),\
    w(0xc4), w(0xa7), w(0x7e), w(0x3d), w(0x64), w(0x5d), w(0x19), w(0x73),\
    w(0x60), w(0x81), w(0x4f), w(0xdc), w(0x22), w(0x2a), w(0x90), w(0x88),\
    w(0x46), w(0xee), w(0xb8), w(0x14), w(0xde), w(0x5e), w(0x0b), w(0xdb),\
    w(0xe0), w(0x32), w(0x3a), w(0x0a), w(0x49), w(0x06), w(0x24), w(0x5c),\
    w(0xc2), w(0xd3), w(0xac), w(0x62), w(0x91), w(0x95), w(0xe4), w(0x79),\
    w(0xe7), w(0xc8), w(0x37), w(0x6d), w(0x8d), w(0xd5), w(0x4e), w(0xa9),\
    w(0x6c), w(0x56), w(0xf4), w(0xea), w(0x65), w(0x7a), w(0xae), w(0x08),\
    w(0xba), w(0x78), w(0x25), w(0x2e), w(0x1c), w(0xa6), w(0xb4), w(0xc6),\
    w(0xe8), w(0xdd), w(0x74), w(0x1f), w(0x4b), w(0xbd), w(0x8b), w(0x8a),\
    w(0x70), w(0x3e), w(0xb5), w(0x66), w(0x48), w(0x03), w(0xf6), w(0x0e),\
    w(0x61), w(0x35), w(0x57), w(0xb9), w(0x86), w(0xc1), w(0x1d), w(0x9e),\
    w(0xe1), w(0xf8), w(0x98), w(0x11), w(0x69), w(0xd9), w(0x8e), w(0x94),\
    w(0x9b), w(0x1e), w(0x87), w(0xe9), w(0xce), w(0x55), w(0x28), w(0xdf),\
    w(0x8c), w(0xa1), w(0x89), w(0x0d), w(0xbf), w(0xe6), w(0x42), w(0x68),\
    w(0x41), w(0x99), w(0x2d), w(0x0f), w(0xb0), w(0x54), w(0xbb), w(0x16)

#define isb_data(w) \
    w(0x52), w(0x09), w(0x6a), w(0xd5), w(0x30), w(0x36), w(0xa5), w(0x38),\
    w(0xbf), w(0x40), w(0xa3), w(0x9e), w(0x81), w(0xf3), w(0xd7), w(0xfb),\
    w(0x7c), w(0xe3), w(0x39), w(0x82), w(0x9b), w(0x2f), w(0xff), w(0x87),\
    w(0x34), w(0x8e), w(0x43), w(0x44), w(0xc4), w(0xde), w(0xe9), w(0xcb),\
    w(0x54), w(0x7b), w(0x94), w(0x32), w(0xa6), w(0xc2), w(0x23), w(0x3d),\
    w(0xee), w(0x4c), w(0x95), w(0x0b), w(0x42), w(0xfa), w(0xc3), w(0x4e),\
    w(0x08), w(0x2e), w(0xa1), w(0x66), w(0x28), w(0xd9), w(0x24), w(0xb2),\
    w(0x76), w(0x5b), w(0xa2), w(0x49), w(0x6d), w(0x8b), w(0xd1), w(0x25),\
    w(0x72), w(0xf8), w(0xf6), w(0x64), w(0x86), w(0x68), w(0x98), w(0x16),\
    w(0xd4), w(0xa4), w(0x5c), w(0xcc), w(0x5d), w(0x65), w(0xb6), w(0x92),\
    w(0x6c), w(0x70), w(0x48), w(0x50), w(0xfd), w(0xed), w(0xb9), w(0xda),\
    w(0x5e), w(0x15), w(0x46), w(0x57), w(0xa7), w(0x8d), w(0x9d), w(0x84),\
    w(0x90), w(0xd8), w(0xab), w(0x00), w(0x8c), w(0xbc), w(0xd3), w(0x0a),\
    w(0xf7), w(0xe4), w(0x58), w(0x05), w(0xb8), w(0xb3), w(0x45), w(0x06),\
    w(0xd0), w(0x2c), w(0x1e), w(0x8f), w(0xca), w(0x3f), w(0x0f), w(0x02),\
    w(0xc1), w(0xaf), w(0xbd), w(0x03), w(0x01), w(0x13), w(0x8a), w(0x6b),\
    w(0x3a), w(0x91), w(0x11), w(0x41), w(0x4f), w(0x67), w(0xdc), w(0xea),\
    w(0x97), w(0xf2), w(0xcf), w(0xce), w(0xf0), w(0xb4), w(0xe6), w(0x73),\
    w(0x96), w(0xac), w(0x74), w(0x22), w(0xe7), w(0xad), w(0x35), w(0x85),\
    w(0xe2), w(0xf9), w(0x37), w(0xe8), w(0x1c), w(0x75), w(0xdf), w(0x6e),\
    w(0x47), w(0xf1), w(0x1a), w(0x71), w(0x1d), w(0x29), w(0xc5), w(0x89),\
    w(0x6f), w(0xb7), w(0x62), w(0x0e), w(0xaa), w(0x18), w(0xbe), w(0x1b),\
    w(0xfc), w(0x56), w(0x3e), w(0x4b), w(0xc6), w(0xd2), w(0x79), w(0x20),\
    w(0x9a), w(0xdb), w(0xc0), w(0xfe), w(0x78), w(0xcd), w(0x5a), w(0xf4),\
    w(0x1f), w(0xdd), w(0xa8), w(0x33), w(0x88), w(0x07), w(0xc7), w(0x31),\
    w(0xb1), w(0x12), w(0x10), w(0x59), w(0x27), w(0x80), w(0xec), w(0x5f),\
    w(0x60), w(0x51), w(0x7f), w(0xa9), w(0x19), w(0xb5), w(0x4a), w(0x0d),\
    w(0x2d), w(0xe5), w(0x7a), w(0x9f), w(0x93), w(0xc9), w(0x9c), w(0xef),\
    w(0xa0), w(0xe0), w(0x3b), w(0x4d), w(0xae), w(0x2a), w(0xf5), w(0xb0),\
    w(0xc8), w(0xeb), w(0xbb), w(0x3c), w(0x83), w(0x53), w(0x99), w(0x61),\
    w(0x17), w(0x2b), w(0x04), w(0x7e), w(0xba), w(0x77), w(0xd6), w(0x26),\
    w(0xe1), w(0x69), w(0x14), w(0x63), w(0x55), w(0x21), w(0x0c), w(0x7d),

#define mm_data(w) \
    w(0x00), w(0x01), w(0x02), w(0x03), w(0x04), w(0x05), w(0x06), w(0x07),\
    w(0x08), w(0x09), w(0x0a), w(0x0b), w(0x0c), w(0x0d), w(0x0e), w(0x0f),\
    w(0x10), w(0x11), w(0x12), w(0x13), w(0x14), w(0x15), w(0x16), w(0x17),\
    w(0x18), w(0x19), w(0x1a), w(0x1b), w(0x1c), w(0x1d), w(0x1e), w(0x1f),\
    w(0x20), w(0x21), w(0x22), w(0x23), w(0x24), w(0x25), w(0x26), w(0x27),\
    w(0x28), w(0x29), w(0x2a), w(0x2b), w(0x2c), w(0x2d), w(0x2e), w(0x2f),\
    w(0x30), w(0x31), w(0x32), w(0x33), w(0x34), w(0x35), w(0x36), w(0x37),\
    w(0x38), w(0x39), w(0x3a), w(0x3b), w(0x3c), w(0x3d), w(0x3e), w(0x3f),\
    w(0x40), w(0x41), w(0x42), w(0x43), w(0x44), w(0x45), w(0x46), w(0x47),\
    w(0x48), w(0x49), w(0x4a), w(0x4b), w(0x4c), w(0x4d), w(0x4e), w(0x4f),\
    w(0x50), w(0x51), w(0x52), w(0x53), w(0x54), w(0x55), w(0x56), w(0x57),\
    w(0x58), w(0x59), w(0x5a), w(0x5b), w(0x5c), w(0x5d), w(0x5e), w(0x5f),\
    w(0x60), w(0x61), w(0x62), w(0x63), w(0x64), w(0x65), w(0x66), w(0x67),\
    w(0x68), w(0x69), w(0x6a), w(0x6b), w(0x6c), w(0x6d), w(0x6e), w(0x6f),\
    w(0x70), w(0x71), w(0x72), w(0x73), w(0x74), w(0x75), w(0x76), w(0x77),\
    w(0x78), w(0x79), w(0x7a), w(0x7b), w(0x7c), w(0x7d), w(0x7e), w(0x7f),\
    w(0x80), w(0x81), w(0x82), w(0x83), w(0x84), w(0x85), w(0x86), w(0x87),\
    w(0x88), w(0x89), w(0x8a), w(0x8b), w(0x8c), w(0x8d), w(0x8e), w(0x8f),\
    w(0x90), w(0x91), w(0x92), w(0x93), w(0x94), w(0x95), w(0x96), w(0x97),\
    w(0x98), w(0x99), w(0x9a), w(0x9b), w(0x9c), w(0x9d), w(0x9e), w(0x9f),\
    w(0xa0), w(0xa1), w(0xa2), w(0xa3), w(0xa4), w(0xa5), w(0xa6), w(0xa7),\
    w(0xa8), w(0xa9), w(0xaa), w(0xab), w(0xac), w(0xad), w(0xae), w(0xaf),\
    w(0xb0), w(0xb1), w(0xb2), w(0xb3), w(0xb4), w(0xb5), w(0xb6), w(0xb7),\
    w(0xb8), w(0xb9), w(0xba), w(0xbb), w(0xbc), w(0xbd), w(0xbe), w(0xbf),\
    w(0xc0), w(0xc1), w(0xc2), w(0xc3), w(0xc4), w(0xc5), w(0xc6), w(0xc7),\
    w(0xc8), w(0xc9), w(0xca), w(0xcb), w(0xcc), w(0xcd), w(0xce), w(0xcf),\
    w(0xd0), w(0xd1), w(0xd2), w(0xd3), w(0xd4), w(0xd5), w(0xd6), w(0xd7),\
    w(0xd8), w(0xd9), w(0xda), w(0xdb), w(0xdc), w(0xdd), w(0xde), w(0xdf),\
    w(0xe0), w(0xe1), w(0xe2), w(0xe3), w(0xe4), w(0xe5), w(0xe6), w(0xe7),\
    w(0xe8), w(0xe9), w(0xea), w(0xeb), w(0xec), w(0xed), w(0xee), w(0xef),\
    w(0xf0), w(0xf1), w(0xf2), w(0xf3), w(0xf4), w(0xf5), w(0xf6), w(0xf7),\
    w(0xf8), w(0xf9), w(0xfa), w(0xfb), w(0xfc), w(0xfd), w(0xfe), w(0xff)

#define h0(x)   (x)

/*  These defines are used to ensure tables are generated in the
    right format depending on the internal byte order required
*/

#define w0(p)   bytes2word(p, 0, 0, 0)
#define w1(p)   bytes2word(0, p, 0, 0)
#define w2(p)   bytes2word(0, 0, p, 0)
#define w3(p)   bytes2word(0, 0, 0, p)

/*  Number of elements required in this table for different
    block and key lengths is:

    Rcon Table      key length (bytes)
    Length          16  20  24  28  32
                ---------------------
    block     16 |  10   9   8   7   7
    length    20 |  14  11  10   9   9
    (bytes)   24 |  19  15  12  11  11
              28 |  24  19  16  13  13
              32 |  29  23  19  17  14

    this table can be a table of bytes if the key schedule
    code is adjusted accordingly
*/

#define u0(p)   bytes2word(f2(p), p, p, f3(p))
#define u1(p)   bytes2word(f3(p), f2(p), p, p)
#define u2(p)   bytes2word(p, f3(p), f2(p), p)
#define u3(p)   bytes2word(p, p, f3(p), f2(p))

#define v0(p)   bytes2word(fe(p), f9(p), fd(p), fb(p))
#define v1(p)   bytes2word(fb(p), fe(p), f9(p), fd(p))
#define v2(p)   bytes2word(fd(p), fb(p), fe(p), f9(p))
#define v3(p)   bytes2word(f9(p), fd(p), fb(p), fe(p))

const uint32_t rcon_tab[29] =
{
    w0(0x01), w0(0x02), w0(0x04), w0(0x08),
    w0(0x10), w0(0x20), w0(0x40), w0(0x80),
    w0(0x1b), w0(0x36), w0(0x6c), w0(0xd8),
    w0(0xab), w0(0x4d), w0(0x9a), w0(0x2f),
    w0(0x5e), w0(0xbc), w0(0x63), w0(0xc6),
    w0(0x97), w0(0x35), w0(0x6a), w0(0xd4),
    w0(0xb3), w0(0x7d), w0(0xfa), w0(0xef),
    w0(0xc5)
};

#ifdef  SBX_SET
const uint8_t s_box[256] = { sb_data(h0) };
#endif
#ifdef  ISB_SET
const uint8_t inv_s_box[256] = { isb_data(h0) };
#endif

#ifdef  FT1_SET
const uint32_t ft_tab[256] = { sb_data(u0) };
#endif
#ifdef  FT4_SET
const uint32_t ft_tab[4][256] =
    { {  sb_data(u0) }, {  sb_data(u1) }, {  sb_data(u2) }, {  sb_data(u3) } };
#endif

#ifdef  FL1_SET
const uint32_t fl_tab[256] = { sb_data(w0) };
#endif
#ifdef  FL4_SET
const uint32_t fl_tab[4][256] =
    { {  sb_data(w0) }, {  sb_data(w1) }, {  sb_data(w2) }, {  sb_data(w3) } };
#endif

#ifdef  IT1_SET
const uint32_t it_tab[256] = { isb_data(v0) };
#endif
#ifdef  IT4_SET
const uint32_t it_tab[4][256] =
    { { isb_data(v0) }, { isb_data(v1) }, { isb_data(v2) }, { isb_data(v3) } };
#endif

#ifdef  IL1_SET
const uint32_t il_tab[256] = { isb_data(w0) };
#endif
#ifdef  IL4_SET
const uint32_t il_tab[4][256] =
    { { isb_data(w0) }, { isb_data(w1) }, { isb_data(w2) }, { isb_data(w3) } };
#endif

#ifdef  LS1_SET
const uint32_t ls_tab[256] = { sb_data(w0) };
#endif
#ifdef  LS4_SET
const uint32_t ls_tab[4][256] =
    { {  sb_data(w0) }, {  sb_data(w1) }, {  sb_data(w2) }, {  sb_data(w3) } };
#endif

#ifdef  IM1_SET
const uint32_t im_tab[256] = { mm_data(v0) };
#endif
#ifdef  IM4_SET
const uint32_t im_tab[4][256] =
    { {  mm_data(v0) }, {  mm_data(v1) }, {  mm_data(v2) }, {  mm_data(v3) } };
#endif

#else   /* dynamic table generation */

uint8_t tab_init = 0;

#define const

uint32_t  rcon_tab[RC_LENGTH];

#ifdef  SBX_SET
uint8_t s_box[256];
#endif
#ifdef  ISB_SET
uint8_t inv_s_box[256];
#endif

#ifdef  FT1_SET
uint32_t ft_tab[256];
#endif
#ifdef  FT4_SET
uint32_t ft_tab[4][256];
#endif

#ifdef  FL1_SET
uint32_t fl_tab[256];
#endif
#ifdef  FL4_SET
uint32_t fl_tab[4][256];
#endif

#ifdef  IT1_SET
uint32_t it_tab[256];
#endif
#ifdef  IT4_SET
uint32_t it_tab[4][256];
#endif

#ifdef  IL1_SET
uint32_t il_tab[256];
#endif
#ifdef  IL4_SET
uint32_t il_tab[4][256];
#endif

#ifdef  LS1_SET
uint32_t ls_tab[256];
#endif
#ifdef  LS4_SET
uint32_t ls_tab[4][256];
#endif

#ifdef  IM1_SET
uint32_t im_tab[256];
#endif
#ifdef  IM4_SET
uint32_t im_tab[4][256];
#endif

#if !defined(FF_TABLES)

/*  Generate the tables for the dynamic table option

    It will generally be sensible to use tables to compute finite
    field multiplies and inverses but where memory is scarse this
    code might sometimes be better. But it only has effect during
    initialisation so its pretty unimportant in overall terms.
*/

/*  return 2 ^ (n - 1) where n is the bit number of the highest bit
    set in x with x in the range 1 < x < 0x00000200.   This form is
    used so that locals within fi can be bytes rather than words
*/

static uint8_t hibit(const uint32_t x)
{   uint8_t r = (uint8_t)((x >> 1) | (x >> 2));

    r |= (r >> 2);
    r |= (r >> 4);
    return (r + 1) >> 1;
}

/* return the inverse of the finite field element x */

static uint8_t fi(const uint8_t x)
{   uint8_t p1 = x, p2 = BPOLY, n1 = hibit(x), n2 = 0x80, v1 = 1, v2 = 0;

    if(x < 2) return x;

    for(;;)
    {
        if(!n1) return v1;

        while(n2 >= n1)
        {
            n2 /= n1; p2 ^= p1 * n2; v2 ^= v1 * n2; n2 = hibit(p2);
        }

        if(!n2) return v2;

        while(n1 >= n2)
        {
            n1 /= n2; p1 ^= p2 * n1; v1 ^= v2 * n1; n1 = hibit(p1);
        }
    }
}

#else

/* define the finite field multiplies required for Rijndael */

#define f2(x) ((x) ? pow[log[x] + 0x19] : 0)
#define f3(x) ((x) ? pow[log[x] + 0x01] : 0)
#define f9(x) ((x) ? pow[log[x] + 0xc7] : 0)
#define fb(x) ((x) ? pow[log[x] + 0x68] : 0)
#define fd(x) ((x) ? pow[log[x] + 0xee] : 0)
#define fe(x) ((x) ? pow[log[x] + 0xdf] : 0)
#define fi(x) ((x) ?   pow[255 - log[x]]: 0)

#endif

/* The forward and inverse affine transformations used in the S-box */

#define fwd_affine(x) \
    (w = (uint32_t)x, w ^= (w<<1)^(w<<2)^(w<<3)^(w<<4), 0x63^(uint8_t)(w^(w>>8)))

#define inv_affine(x) \
    (w = (uint32_t)x, w = (w<<1)^(w<<3)^(w<<6), 0x05^(uint8_t)(w^(w>>8)))

void gen_tabs(void)
{   uint32_t  i, w;

#if defined(FF_TABLES)

    uint8_t  pow[512], log[256];

    /*  log and power tables for GF(2^8) finite field with
        WPOLY as modular polynomial - the simplest primitive
        root is 0x03, used here to generate the tables
    */

    i = 0; w = 1;
    do
    {
        pow[i] = (uint8_t)w;
        pow[i + 255] = (uint8_t)w;
        log[w] = (uint8_t)i++;
        w ^=  (w << 1) ^ (w & 0x80 ? WPOLY : 0);
    }
    while (w != 1);

#endif

    for(i = 0, w = 1; i < RC_LENGTH; ++i)
    {
        rcon_tab[i] = bytes2word(w, 0, 0, 0);
        w = f2(w);
    }

    for(i = 0; i < 256; ++i)
    {   uint8_t    b;

        b = fwd_affine(fi((uint8_t)i));
        w = bytes2word(f2(b), b, b, f3(b));

#ifdef  SBX_SET
        s_box[i] = b;
#endif

#ifdef  FT1_SET                 /* tables for a normal encryption round */
        ft_tab[i] = w;
#endif
#ifdef  FT4_SET
        ft_tab[0][i] = w;
        ft_tab[1][i] = upr(w,1);
        ft_tab[2][i] = upr(w,2);
        ft_tab[3][i] = upr(w,3);
#endif
        w = bytes2word(b, 0, 0, 0);

#ifdef  FL1_SET                 /* tables for last encryption round (may also   */
        fl_tab[i] = w;          /* be used in the key schedule)                 */
#endif
#ifdef  FL4_SET
        fl_tab[0][i] = w;
        fl_tab[1][i] = upr(w,1);
        fl_tab[2][i] = upr(w,2);
        fl_tab[3][i] = upr(w,3);
#endif

#ifdef  LS1_SET                 /* table for key schedule if fl_tab above is    */
        ls_tab[i] = w;          /* not of the required form                     */
#endif
#ifdef  LS4_SET
        ls_tab[0][i] = w;
        ls_tab[1][i] = upr(w,1);
        ls_tab[2][i] = upr(w,2);
        ls_tab[3][i] = upr(w,3);
#endif

        b = fi(inv_affine((uint8_t)i));
        w = bytes2word(fe(b), f9(b), fd(b), fb(b));

#ifdef  IM1_SET                 /* tables for the inverse mix column operation  */
        im_tab[b] = w;
#endif
#ifdef  IM4_SET
        im_tab[0][b] = w;
        im_tab[1][b] = upr(w,1);
        im_tab[2][b] = upr(w,2);
        im_tab[3][b] = upr(w,3);
#endif

#ifdef  ISB_SET
        inv_s_box[i] = b;
#endif
#ifdef  IT1_SET                 /* tables for a normal decryption round */
        it_tab[i] = w;
#endif
#ifdef  IT4_SET
        it_tab[0][i] = w;
        it_tab[1][i] = upr(w,1);
        it_tab[2][i] = upr(w,2);
        it_tab[3][i] = upr(w,3);
#endif
        w = bytes2word(b, 0, 0, 0);
#ifdef  IL1_SET                 /* tables for last decryption round */
        il_tab[i] = w;
#endif
#ifdef  IL4_SET
        il_tab[0][i] = w;
        il_tab[1][i] = upr(w,1);
        il_tab[2][i] = upr(w,2);
        il_tab[3][i] = upr(w,3);
#endif
    }

    tab_init = 1;
}

#endif
