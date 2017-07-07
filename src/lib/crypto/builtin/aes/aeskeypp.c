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

/*
 * Issue Date: 21/01/2002
 *
 * This file contains the code for implementing the key schedule for AES
 * (Rijndael) for block and key sizes of 16, 20, 24, 28 and 32 bytes.
 */

#include "aesopt.h"

/* Subroutine to set the block size (if variable) in bytes, legal
   values being 16, 24 and 32.
*/

#if !defined(BLOCK_SIZE) && defined(SET_BLOCK_LENGTH)

/* Subroutine to set the block size (if variable) in bytes, legal
   values being 16, 24 and 32.
*/

aes_rval aes_blk_len(unsigned int blen, aes_ctx cx[1])
{
#if !defined(FIXED_TABLES)
    if(!tab_init) gen_tabs();
#endif

    if((blen & 3) || blen < 16 || blen > 32)
    {
        cx->n_blk = 0; return aes_bad;
    }

    cx->n_blk = blen;
    return aes_good;
}

#endif

/* Initialise the key schedule from the user supplied key. The key
   length is now specified in bytes - 16, 24 or 32 as appropriate.
   This corresponds to bit lengths of 128, 192 and 256 bits, and
   to Nk values of 4, 6 and 8 respectively.

   The following macros implement a single cycle in the key
   schedule generation process. The number of cycles needed
   for each cx->n_blk and nk value is:

    nk =             4  5  6  7  8
    ------------------------------
    cx->n_blk = 4   10  9  8  7  7
    cx->n_blk = 5   14 11 10  9  9
    cx->n_blk = 6   19 15 12 11 11
    cx->n_blk = 7   21 19 16 13 14
    cx->n_blk = 8   29 23 19 17 14
*/

/* Initialise the key schedule from the user supplied key. The key
   length is now specified in bytes - 16, 20, 24, 28 or 32 as
   appropriate. This corresponds to bit lengths of 128, 160, 192,
   224 and 256 bits, and to Nk values of 4, 5, 6, 7 & 8 respectively.
 */

#define mx(t,f) (*t++ = inv_mcol(*f),f++)
#define cp(t,f) *t++ = *f++

#if   BLOCK_SIZE == 16
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s)
#elif BLOCK_SIZE == 20
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s); \
                    cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s); \
                    mx(d,s)
#elif BLOCK_SIZE == 24
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s); \
                    cp(d,s); cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s); \
                    mx(d,s); mx(d,s)
#elif BLOCK_SIZE == 28
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s); \
                    cp(d,s); cp(d,s); cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s); \
                    mx(d,s); mx(d,s); mx(d,s)
#elif BLOCK_SIZE == 32
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s); \
                    cp(d,s); cp(d,s); cp(d,s); cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s); \
                    mx(d,s); mx(d,s); mx(d,s); mx(d,s)
#else

#define cpy(d,s) \
switch(nc) \
{   case 8: cp(d,s); \
    case 7: cp(d,s); \
    case 6: cp(d,s); \
    case 5: cp(d,s); \
    case 4: cp(d,s); cp(d,s); \
            cp(d,s); cp(d,s); \
}

#define mix(d,s) \
switch(nc) \
{   case 8: mx(d,s); \
    case 7: mx(d,s); \
    case 6: mx(d,s); \
    case 5: mx(d,s); \
    case 4: mx(d,s); mx(d,s); \
            mx(d,s); mx(d,s); \
}

#endif

/*  The following macros implement a single cycle in the key
    schedule generation process. The number of cycles needed
    for each cx->n_blk and nk value is:

    nk =      4  5  6  7  8
    -----------------------
    cx->n_blk = 4   10  9  8  7  7
    cx->n_blk = 5   14 11 10  9  9
    cx->n_blk = 6   19 15 12 11 11
    cx->n_blk = 7   21 19 16 13 14
    cx->n_blk = 8   29 23 19 17 14
*/

#define ks4(i) \
{   p ^= ls_box(s,3) ^ rcon_tab[i]; q ^= p; r ^= q; s ^= r; \
    cx->k_sch[4*(i)+4] = p; \
    cx->k_sch[4*(i)+5] = q; \
    cx->k_sch[4*(i)+6] = r; \
    cx->k_sch[4*(i)+7] = s; \
}

#define ks5(i) \
{   p ^= ls_box(t,3) ^ rcon_tab[i]; q ^= p; \
    r ^= q; s ^= r; t ^= s; \
    cx->k_sch[5*(i)+ 5] = p; \
    cx->k_sch[5*(i)+ 6] = q; \
    cx->k_sch[5*(i)+ 7] = r; \
    cx->k_sch[5*(i)+ 8] = s; \
    cx->k_sch[5*(i)+ 9] = t; \
}

#define ks6(i) \
{   p ^= ls_box(u,3) ^ rcon_tab[i]; q ^= p; \
    r ^= q; s ^= r; t ^= s; u ^= t; \
    cx->k_sch[6*(i)+ 6] = p; \
    cx->k_sch[6*(i)+ 7] = q; \
    cx->k_sch[6*(i)+ 8] = r; \
    cx->k_sch[6*(i)+ 9] = s; \
    cx->k_sch[6*(i)+10] = t; \
    cx->k_sch[6*(i)+11] = u; \
}

#define ks7(i) \
{   p ^= ls_box(v,3) ^ rcon_tab[i]; q ^= p; r ^= q; s ^= r; \
    t ^= ls_box(s,0); u ^= t; v ^= u; \
    cx->k_sch[7*(i)+ 7] = p; \
    cx->k_sch[7*(i)+ 8] = q; \
    cx->k_sch[7*(i)+ 9] = r; \
    cx->k_sch[7*(i)+10] = s; \
    cx->k_sch[7*(i)+11] = t; \
    cx->k_sch[7*(i)+12] = u; \
    cx->k_sch[7*(i)+13] = v; \
}

#define ks8(i) \
{   p ^= ls_box(w,3) ^ rcon_tab[i]; q ^= p; r ^= q; s ^= r; \
    t ^= ls_box(s,0); u ^= t; v ^= u; w ^= v; \
    cx->k_sch[8*(i)+ 8] = p; \
    cx->k_sch[8*(i)+ 9] = q; \
    cx->k_sch[8*(i)+10] = r; \
    cx->k_sch[8*(i)+11] = s; \
    cx->k_sch[8*(i)+12] = t; \
    cx->k_sch[8*(i)+13] = u; \
    cx->k_sch[8*(i)+14] = v; \
    cx->k_sch[8*(i)+15] = w; \
}

#if defined(ENCRYPTION_KEY_SCHEDULE)

aes_rval aes_enc_key(const unsigned char in_key[], unsigned int klen, aes_ctx cx[1])
{   uint32_t    i,p,q,r,s,t,u,v,w;

#if !defined(FIXED_TABLES)
    if(!tab_init) gen_tabs();
#endif

#if !defined(BLOCK_SIZE)
    if(!cx->n_blk) cx->n_blk = 16;
#else
    cx->n_blk = BLOCK_SIZE;
#endif

    cx->n_blk = (cx->n_blk & ~3) | 1;
    cx->n_rnd = ((klen >> 2) > nc ? (klen >> 2) : nc) + 6;

    cx->k_sch[0] = p = word_in(in_key     );
    cx->k_sch[1] = q = word_in(in_key +  4);
    cx->k_sch[2] = r = word_in(in_key +  8);
    cx->k_sch[3] = s = word_in(in_key + 12);

#if BLOCK_SIZE == 16 && defined(UNROLL)

    switch(klen >> 2)
    {
    case 4: ks4(0); ks4(1); ks4(2); ks4(3);
            ks4(4); ks4(5); ks4(6); ks4(7);
            ks4(8); ks4(9);
            cx->n_rnd = 10; break;
    case 5: cx->k_sch[4] = t = word_in(in_key + 16);
            ks5(0); ks5(1); ks5(2); ks5(3);
            ks5(4); ks5(5); ks5(6); ks5(7);
            ks5(8);
            cx->n_rnd = 11; break;
    case 6: cx->k_sch[4] = t = word_in(in_key + 16);
            cx->k_sch[5] = u = word_in(in_key + 20);
            ks6(0); ks6(1); ks6(2); ks6(3);
            ks6(4); ks6(5); ks6(6); ks6(7);
            cx->n_rnd = 12; break;
    case 7: cx->k_sch[4] = t = word_in(in_key + 16);
            cx->k_sch[5] = u = word_in(in_key + 20);
            cx->k_sch[6] = v = word_in(in_key + 24);
            ks7(0); ks7(1); ks7(2); ks7(3);
            ks7(4); ks7(5); ks7(6);
            cx->n_rnd = 13; break;
    case 8: cx->k_sch[4] = t = word_in(in_key + 16);
            cx->k_sch[5] = u = word_in(in_key + 20);
            cx->k_sch[6] = v = word_in(in_key + 24);
            cx->k_sch[7] = w = word_in(in_key + 28);
            ks8(0); ks8(1); ks8(2); ks8(3);
            ks8(4); ks8(5); ks8(6);
            cx->n_rnd = 14; break;
    default:cx->n_rnd = 0; return aes_bad;
    }
#else
    cx->n_rnd = ((klen >> 2) > nc ? (klen >> 2) : nc) + 6;
    {
        uint32_t l = (nc * (cx->n_rnd + 1) - 1) / (klen >> 2);
        switch(klen >> 2)
        {
        case 4: for(i = 0; i < l; ++i)
                    ks4(i);
                break;
        case 5: cx->k_sch[4] = t = word_in(in_key + 16);
                for(i = 0; i < l; ++i)
                    ks5(i);
                break;
        case 6: cx->k_sch[4] = t = word_in(in_key + 16);
                cx->k_sch[5] = u = word_in(in_key + 20);
                for(i = 0; i < l; ++i)
                    ks6(i);
                break;
        case 7: cx->k_sch[4] = t = word_in(in_key + 16);
                cx->k_sch[5] = u = word_in(in_key + 20);
                cx->k_sch[6] = v = word_in(in_key + 24);
                for(i = 0; i < l; ++i)
                    ks7(i);
                break;
        case 8: cx->k_sch[4] = t = word_in(in_key + 16);
                cx->k_sch[5] = u = word_in(in_key + 20);
                cx->k_sch[6] = v = word_in(in_key + 24);
                cx->k_sch[7] = w = word_in(in_key + 28);
                for(i = 0; i < l; ++i)
                    ks8(i);
                break;
        }
    }
#endif

    return aes_good;
}

#endif

#if defined(DECRYPTION_KEY_SCHEDULE)

aes_rval aes_dec_key(const unsigned char in_key[], unsigned int klen, aes_ctx cx[1])
{   uint32_t    i,p,q,r,s,t,u,v,w;
    dec_imvars

#if !defined(FIXED_TABLES)
    if(!tab_init) gen_tabs();
#endif

#if !defined(BLOCK_SIZE)
    if(!cx->n_blk) cx->n_blk = 16;
#else
    cx->n_blk = BLOCK_SIZE;
#endif

    cx->n_blk = (cx->n_blk & ~3) | 2;
    cx->n_rnd = ((klen >> 2) > nc ? (klen >> 2) : nc) + 6;

    cx->k_sch[0] = p = word_in(in_key     );
    cx->k_sch[1] = q = word_in(in_key +  4);
    cx->k_sch[2] = r = word_in(in_key +  8);
    cx->k_sch[3] = s = word_in(in_key + 12);

#if BLOCK_SIZE == 16 && defined(UNROLL)

    switch(klen >> 2)
    {
    case 4: ks4(0); ks4(1); ks4(2); ks4(3);
            ks4(4); ks4(5); ks4(6); ks4(7);
            ks4(8); ks4(9);
            cx->n_rnd = 10; break;
    case 5: cx->k_sch[4] = t = word_in(in_key + 16);
            ks5(0); ks5(1); ks5(2); ks5(3);
            ks5(4); ks5(5); ks5(6); ks5(7);
            ks5(8);
            cx->n_rnd = 11; break;
    case 6: cx->k_sch[4] = t = word_in(in_key + 16);
            cx->k_sch[5] = u = word_in(in_key + 20);
            ks6(0); ks6(1); ks6(2); ks6(3);
            ks6(4); ks6(5); ks6(6); ks6(7);
            cx->n_rnd = 12; break;
    case 7: cx->k_sch[4] = t = word_in(in_key + 16);
            cx->k_sch[5] = u = word_in(in_key + 20);
            cx->k_sch[6] = v = word_in(in_key + 24);
            ks7(0); ks7(1); ks7(2); ks7(3);
            ks7(4); ks7(5); ks7(6);
            cx->n_rnd = 13; break;
    case 8: cx->k_sch[4] = t = word_in(in_key + 16);
            cx->k_sch[5] = u = word_in(in_key + 20);
            cx->k_sch[6] = v = word_in(in_key + 24);
            cx->k_sch[7] = w = word_in(in_key + 28);
            ks8(0); ks8(1); ks8(2); ks8(3);
            ks8(4); ks8(5); ks8(6);
            cx->n_rnd = 14; break;
    default:cx->n_rnd = 0; return aes_bad;
    }
#else
    cx->n_rnd = ((klen >> 2) > nc ? (klen >> 2) : nc) + 6;
    {
        uint32_t l = (nc * (cx->n_rnd + 1) - 1) / (klen >> 2);
        switch(klen >> 2)
        {
        case 4: for(i = 0; i < l; ++i)
                    ks4(i);
                break;
        case 5: cx->k_sch[4] = t = word_in(in_key + 16);
                for(i = 0; i < l; ++i)
                    ks5(i);
                break;
        case 6: cx->k_sch[4] = t = word_in(in_key + 16);
                cx->k_sch[5] = u = word_in(in_key + 20);
                for(i = 0; i < l; ++i)
                    ks6(i);
                break;
        case 7: cx->k_sch[4] = t = word_in(in_key + 16);
                cx->k_sch[5] = u = word_in(in_key + 20);
                cx->k_sch[6] = v = word_in(in_key + 24);
                for(i = 0; i < l; ++i)
                    ks7(i);
                break;
        case 8: cx->k_sch[4] = t = word_in(in_key + 16);
                cx->k_sch[5] = u = word_in(in_key + 20);
                cx->k_sch[6] = v = word_in(in_key + 24);
                cx->k_sch[7] = w = word_in(in_key + 28);
                for(i = 0; i < l; ++i)
                    ks8(i);
                break;
        }
    }
#endif

#if (DEC_ROUND != NO_TABLES)
    for(i = nc; i < nc * cx->n_rnd; ++i)
        cx->k_sch[i] = inv_mcol(cx->k_sch[i]);
#endif

    return aes_good;
}

#endif
