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
 * This file contains the code for implementing encryption and decryption
 * for AES (Rijndael) for block and key sizes of 16, 24 and 32 bytes. It
 * can optionally be replaced by code written in assembler using NASM.
*/

#include "aesopt.h"

#if defined(BLOCK_SIZE) && (BLOCK_SIZE & 7)
#error An illegal block size has been specified.
#endif

#define unused  77  /* Sunset Strip */

#define si(y,x,k,c) s(y,c) = word_in(x + 4 * c) ^ k[c]
#define so(y,x,c)   word_out(y + 4 * c, s(x,c))

#if BLOCK_SIZE == 16

#if defined(ARRAYS)
#define locals(y,x)     x[4],y[4]
#else
#define locals(y,x)     x##0,x##1,x##2,x##3,y##0,y##1,y##2,y##3
 /*
   the following defines prevent the compiler requiring the declaration
   of generated but unused variables in the fwd_var and inv_var macros
 */
#define b04 unused
#define b05 unused
#define b06 unused
#define b07 unused
#define b14 unused
#define b15 unused
#define b16 unused
#define b17 unused
#endif
#define l_copy(y, x)    s(y,0) = s(x,0); s(y,1) = s(x,1); \
                        s(y,2) = s(x,2); s(y,3) = s(x,3);
#define state_in(y,x,k) si(y,x,k,0); si(y,x,k,1); si(y,x,k,2); si(y,x,k,3)
#define state_out(y,x)  so(y,x,0); so(y,x,1); so(y,x,2); so(y,x,3)
#define round(rm,y,x,k) rm(y,x,k,0); rm(y,x,k,1); rm(y,x,k,2); rm(y,x,k,3)

#elif BLOCK_SIZE == 24

#if defined(ARRAYS)
#define locals(y,x)     x[6],y[6]
#else
#define locals(y,x)     x##0,x##1,x##2,x##3,x##4,x##5, \
                        y##0,y##1,y##2,y##3,y##4,y##5
#define b06 unused
#define b07 unused
#define b16 unused
#define b17 unused
#endif
#define l_copy(y, x)    s(y,0) = s(x,0); s(y,1) = s(x,1); \
                        s(y,2) = s(x,2); s(y,3) = s(x,3); \
                        s(y,4) = s(x,4); s(y,5) = s(x,5);
#define state_in(y,x,k) si(y,x,k,0); si(y,x,k,1); si(y,x,k,2); \
                        si(y,x,k,3); si(y,x,k,4); si(y,x,k,5)
#define state_out(y,x)  so(y,x,0); so(y,x,1); so(y,x,2); \
                        so(y,x,3); so(y,x,4); so(y,x,5)
#define round(rm,y,x,k) rm(y,x,k,0); rm(y,x,k,1); rm(y,x,k,2); \
                        rm(y,x,k,3); rm(y,x,k,4); rm(y,x,k,5)
#else

#if defined(ARRAYS)
#define locals(y,x)     x[8],y[8]
#else
#define locals(y,x)     x##0,x##1,x##2,x##3,x##4,x##5,x##6,x##7, \
                        y##0,y##1,y##2,y##3,y##4,y##5,y##6,y##7
#endif
#define l_copy(y, x)    s(y,0) = s(x,0); s(y,1) = s(x,1); \
                        s(y,2) = s(x,2); s(y,3) = s(x,3); \
                        s(y,4) = s(x,4); s(y,5) = s(x,5); \
                        s(y,6) = s(x,6); s(y,7) = s(x,7);

#if BLOCK_SIZE == 32

#define state_in(y,x,k) si(y,x,k,0); si(y,x,k,1); si(y,x,k,2); si(y,x,k,3); \
                        si(y,x,k,4); si(y,x,k,5); si(y,x,k,6); si(y,x,k,7)
#define state_out(y,x)  so(y,x,0); so(y,x,1); so(y,x,2); so(y,x,3); \
                        so(y,x,4); so(y,x,5); so(y,x,6); so(y,x,7)
#define round(rm,y,x,k) rm(y,x,k,0); rm(y,x,k,1); rm(y,x,k,2); rm(y,x,k,3); \
                        rm(y,x,k,4); rm(y,x,k,5); rm(y,x,k,6); rm(y,x,k,7)
#else

#define state_in(y,x,k) \
switch(nc) \
{   case 8: si(y,x,k,7); si(y,x,k,6); \
    case 6: si(y,x,k,5); si(y,x,k,4); \
    case 4: si(y,x,k,3); si(y,x,k,2); \
            si(y,x,k,1); si(y,x,k,0); \
}

#define state_out(y,x) \
switch(nc) \
{   case 8: so(y,x,7); so(y,x,6); \
    case 6: so(y,x,5); so(y,x,4); \
    case 4: so(y,x,3); so(y,x,2); \
            so(y,x,1); so(y,x,0); \
}

#if defined(FAST_VARIABLE)

#define round(rm,y,x,k) \
switch(nc) \
{   case 8: rm(y,x,k,7); rm(y,x,k,6); \
            rm(y,x,k,5); rm(y,x,k,4); \
            rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
            break; \
    case 6: rm(y,x,k,5); rm(y,x,k,4); \
            rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
            break; \
    case 4: rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
            break; \
}
#else

#define round(rm,y,x,k) \
switch(nc) \
{   case 8: rm(y,x,k,7); rm(y,x,k,6); \
    case 6: rm(y,x,k,5); rm(y,x,k,4); \
    case 4: rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
}

#endif

#endif
#endif

#if defined(ENCRYPTION)

/* I am grateful to Frank Yellin for the following construction
   (and that for decryption) which, given the column (c) of the
   output state variable, gives the input state variables which
   are needed in its computation for each row (r) of the state.

   For the fixed block size options, compilers should be able to
   reduce this complex expression (and the equivalent one for
   decryption) to a static variable reference at compile time.
   But for variable block size code, there will be some limbs on
   which conditional clauses will be returned.
*/

/* y = output word, x = input word, r = row, c = column for r = 0,
   1, 2 and 3 = column accessed for row r.
*/

#define fwd_var(x,r,c) \
 ( r==0 ?           \
    ( c==0 ? s(x,0) \
    : c==1 ? s(x,1) \
    : c==2 ? s(x,2) \
    : c==3 ? s(x,3) \
    : c==4 ? s(x,4) \
    : c==5 ? s(x,5) \
    : c==6 ? s(x,6) \
    : s(x,7))       \
 : r==1 ?           \
    ( c==0 ? s(x,1) \
    : c==1 ? s(x,2) \
    : c==2 ? s(x,3) \
    : c==3 ? nc==4 ? s(x,0) : s(x,4) \
    : c==4 ? s(x,5) \
    : c==5 ? nc==8 ? s(x,6) : s(x,0) \
    : c==6 ? s(x,7) \
    : s(x,0))       \
 : r==2 ?           \
    ( c==0 ? nc==8 ? s(x,3) : s(x,2) \
    : c==1 ? nc==8 ? s(x,4) : s(x,3) \
    : c==2 ? nc==4 ? s(x,0) : nc==8 ? s(x,5) : s(x,4) \
    : c==3 ? nc==4 ? s(x,1) : nc==8 ? s(x,6) : s(x,5) \
    : c==4 ? nc==8 ? s(x,7) : s(x,0) \
    : c==5 ? nc==8 ? s(x,0) : s(x,1) \
    : c==6 ? s(x,1) \
    : s(x,2))       \
 :                  \
    ( c==0 ? nc==8 ? s(x,4) : s(x,3) \
    : c==1 ? nc==4 ? s(x,0) : nc==8 ? s(x,5) : s(x,4) \
    : c==2 ? nc==4 ? s(x,1) : nc==8 ? s(x,6) : s(x,5) \
    : c==3 ? nc==4 ? s(x,2) : nc==8 ? s(x,7) : s(x,0) \
    : c==4 ? nc==8 ? s(x,0) : s(x,1) \
    : c==5 ? nc==8 ? s(x,1) : s(x,2) \
    : c==6 ? s(x,2) \
    : s(x,3)))

#if defined(FT4_SET)
#undef  dec_fmvars
#define dec_fmvars
#define fwd_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ four_tables(x,ft_tab,fwd_var,rf1,c)
#elif defined(FT1_SET)
#undef  dec_fmvars
#define dec_fmvars
#define fwd_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ one_table(x,upr,ft_tab,fwd_var,rf1,c)
#else
#define fwd_rnd(y,x,k,c)    s(y,c) = fwd_mcol(no_table(x,s_box,fwd_var,rf1,c)) ^ (k)[c]
#endif

#if defined(FL4_SET)
#define fwd_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ four_tables(x,fl_tab,fwd_var,rf1,c)
#elif defined(FL1_SET)
#define fwd_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ one_table(x,ups,fl_tab,fwd_var,rf1,c)
#else
#define fwd_lrnd(y,x,k,c)   s(y,c) = no_table(x,s_box,fwd_var,rf1,c) ^ (k)[c]
#endif

aes_rval aes_enc_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1])
{   uint32_t        locals(b0, b1);
    const uint32_t  *kp = cx->k_sch;
    dec_fmvars  /* declare variables for fwd_mcol() if needed */

    if(!(cx->n_blk & 1)) return aes_bad;

    state_in(b0, in_blk, kp);

#if (ENC_UNROLL == FULL)

    kp += (cx->n_rnd - 9) * nc;

    switch(cx->n_rnd)
    {
    case 14:    round(fwd_rnd,  b1, b0, kp - 4 * nc);
                round(fwd_rnd,  b0, b1, kp - 3 * nc);
    case 12:    round(fwd_rnd,  b1, b0, kp - 2 * nc);
                round(fwd_rnd,  b0, b1, kp -     nc);
    case 10:    round(fwd_rnd,  b1, b0, kp         );
                round(fwd_rnd,  b0, b1, kp +     nc);
                round(fwd_rnd,  b1, b0, kp + 2 * nc);
                round(fwd_rnd,  b0, b1, kp + 3 * nc);
                round(fwd_rnd,  b1, b0, kp + 4 * nc);
                round(fwd_rnd,  b0, b1, kp + 5 * nc);
                round(fwd_rnd,  b1, b0, kp + 6 * nc);
                round(fwd_rnd,  b0, b1, kp + 7 * nc);
                round(fwd_rnd,  b1, b0, kp + 8 * nc);
                round(fwd_lrnd, b0, b1, kp + 9 * nc);
    }
#else

#if (ENC_UNROLL == PARTIAL)
    {   uint32_t    rnd;
        for(rnd = 0; rnd < (cx->n_rnd >> 1) - 1; ++rnd)
        {
            kp += nc;
            round(fwd_rnd, b1, b0, kp);
            kp += nc;
            round(fwd_rnd, b0, b1, kp);
        }
        kp += nc;
        round(fwd_rnd,  b1, b0, kp);
#else
    {   uint32_t    rnd, *p0 = b0, *p1 = b1, *pt;
        for(rnd = 0; rnd < cx->n_rnd - 1; ++rnd)
        {
            kp += nc;
            round(fwd_rnd, p1, p0, kp);
            pt = p0, p0 = p1, p1 = pt;
        }
#endif
        kp += nc;
        round(fwd_lrnd, b0, b1, kp);
    }
#endif

    state_out(out_blk, b0);
    return aes_good;
}

#endif

#if defined(DECRYPTION)

#define inv_var(x,r,c) \
 ( r==0 ?           \
    ( c==0 ? s(x,0) \
    : c==1 ? s(x,1) \
    : c==2 ? s(x,2) \
    : c==3 ? s(x,3) \
    : c==4 ? s(x,4) \
    : c==5 ? s(x,5) \
    : c==6 ? s(x,6) \
    : s(x,7))       \
 : r==1 ?           \
    ( c==0 ? nc==4 ? s(x,3) : nc==8 ? s(x,7) : s(x,5) \
    : c==1 ? s(x,0) \
    : c==2 ? s(x,1) \
    : c==3 ? s(x,2) \
    : c==4 ? s(x,3) \
    : c==5 ? s(x,4) \
    : c==6 ? s(x,5) \
    : s(x,6))       \
 : r==2 ?           \
    ( c==0 ? nc==4 ? s(x,2) : nc==8 ? s(x,5) : s(x,4) \
    : c==1 ? nc==4 ? s(x,3) : nc==8 ? s(x,6) : s(x,5) \
    : c==2 ? nc==8 ? s(x,7) : s(x,0) \
    : c==3 ? nc==8 ? s(x,0) : s(x,1) \
    : c==4 ? nc==8 ? s(x,1) : s(x,2) \
    : c==5 ? nc==8 ? s(x,2) : s(x,3) \
    : c==6 ? s(x,3) \
    : s(x,4))       \
 :                  \
    ( c==0 ? nc==4 ? s(x,1) : nc==8 ? s(x,4) : s(x,3) \
    : c==1 ? nc==4 ? s(x,2) : nc==8 ? s(x,5) : s(x,4) \
    : c==2 ? nc==4 ? s(x,3) : nc==8 ? s(x,6) : s(x,5) \
    : c==3 ? nc==8 ? s(x,7) : s(x,0) \
    : c==4 ? nc==8 ? s(x,0) : s(x,1) \
    : c==5 ? nc==8 ? s(x,1) : s(x,2) \
    : c==6 ? s(x,2) \
    : s(x,3)))

#if defined(IT4_SET)
#undef  dec_imvars
#define dec_imvars
#define inv_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ four_tables(x,it_tab,inv_var,rf1,c)
#elif defined(IT1_SET)
#undef  dec_imvars
#define dec_imvars
#define inv_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ one_table(x,upr,it_tab,inv_var,rf1,c)
#else
#define inv_rnd(y,x,k,c)    s(y,c) = inv_mcol(no_table(x,inv_s_box,inv_var,rf1,c) ^ (k)[c])
#endif

#if defined(IL4_SET)
#define inv_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ four_tables(x,il_tab,inv_var,rf1,c)
#elif defined(IL1_SET)
#define inv_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ one_table(x,ups,il_tab,inv_var,rf1,c)
#else
#define inv_lrnd(y,x,k,c)   s(y,c) = no_table(x,inv_s_box,inv_var,rf1,c) ^ (k)[c]
#endif

aes_rval aes_dec_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1])
{   uint32_t        locals(b0, b1);
    const uint32_t  *kp = cx->k_sch + nc * cx->n_rnd;
    dec_imvars  /* declare variables for inv_mcol() if needed */

    if(!(cx->n_blk & 2)) return aes_bad;

    state_in(b0, in_blk, kp);

#if (DEC_UNROLL == FULL)

    kp = cx->k_sch + 9 * nc;
    switch(cx->n_rnd)
    {
    case 14:    round(inv_rnd,  b1, b0, kp + 4 * nc);
                round(inv_rnd,  b0, b1, kp + 3 * nc);
    case 12:    round(inv_rnd,  b1, b0, kp + 2 * nc);
                round(inv_rnd,  b0, b1, kp + nc    );
    case 10:    round(inv_rnd,  b1, b0, kp         );
                round(inv_rnd,  b0, b1, kp -     nc);
                round(inv_rnd,  b1, b0, kp - 2 * nc);
                round(inv_rnd,  b0, b1, kp - 3 * nc);
                round(inv_rnd,  b1, b0, kp - 4 * nc);
                round(inv_rnd,  b0, b1, kp - 5 * nc);
                round(inv_rnd,  b1, b0, kp - 6 * nc);
                round(inv_rnd,  b0, b1, kp - 7 * nc);
                round(inv_rnd,  b1, b0, kp - 8 * nc);
                round(inv_lrnd, b0, b1, kp - 9 * nc);
    }
#else

#if (DEC_UNROLL == PARTIAL)
    {   uint32_t    rnd;
        for(rnd = 0; rnd < (cx->n_rnd >> 1) - 1; ++rnd)
        {
            kp -= nc;
            round(inv_rnd, b1, b0, kp);
            kp -= nc;
            round(inv_rnd, b0, b1, kp);
        }
        kp -= nc;
        round(inv_rnd, b1, b0, kp);
#else
    {   uint32_t    rnd, *p0 = b0, *p1 = b1, *pt;
        for(rnd = 0; rnd < cx->n_rnd - 1; ++rnd)
        {
            kp -= nc;
            round(inv_rnd, p1, p0, kp);
            pt = p0, p0 = p1, p1 = pt;
        }
#endif
        kp -= nc;
        round(inv_lrnd, b0, b1, kp);
    }
#endif

    state_out(out_blk, b0);
    return aes_good;
}

#endif
