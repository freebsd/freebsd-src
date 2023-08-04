/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/md4/md4.c */

/*
 * Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.
 *
 * License to copy and use this software is granted provided that
 * it is identified as the "RSA Data Security, Inc. MD4 Message
 * Digest Algorithm" in all material mentioning or referencing this
 * software or this function.
 *
 * License is also granted to make and use derivative works
 * provided that such works are identified as "derived from the RSA
 * Data Security, Inc. MD4 Message Digest Algorithm" in all
 * material mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning
 * either the merchantability of this software or the suitability
 * of this software for any particular purpose.  It is provided "as
 * is" without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/*
**********************************************************************
** md4.c                                                            **
** RSA Data Security, Inc. MD4 Message Digest Algorithm             **
** Created: 2/17/90 RLR                                             **
** Revised: 1/91 SRD,AJ,BSK,JT Reference C Version                  **
**********************************************************************
*/

#include "crypto_int.h"
#include "rsa-md4.h"

#ifdef K5_BUILTIN_MD4

/* forward declaration */
static void Transform (krb5_ui_4 *, krb5_ui_4 *);

static const unsigned char PADDING[64] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* F, G and H are basic MD4 functions: selection, majority, parity */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) ((((x) << (n)) & 0xffffffff) | ((x) >> (32-(n))))

/* FF, GG and HH are MD4 transformations for rounds 1, 2 and 3 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s)                    \
    {(a) += F ((b), (c), (d)) + (x);            \
        (a) &= 0xffffffff;                      \
        (a) = ROTATE_LEFT ((a), (s));}
#define GG(a, b, c, d, x, s)                            \
    {(a) += G ((b), (c), (d)) + (x) + 013240474631UL;   \
        (a) &= 0xffffffff;                              \
        (a) = ROTATE_LEFT ((a), (s));}
#define HH(a, b, c, d, x, s)                            \
    {(a) += H ((b), (c), (d)) + (x) + 015666365641UL;   \
        (a) &= 0xffffffff;                              \
        (a) = ROTATE_LEFT ((a), (s));}

void
krb5int_MD4Init (krb5_MD4_CTX *mdContext)
{
    mdContext->i[0] = mdContext->i[1] = (krb5_ui_4)0;

    /* Load magic initialization constants.
     */
    mdContext->buf[0] = 0x67452301UL;
    mdContext->buf[1] = 0xefcdab89UL;
    mdContext->buf[2] = 0x98badcfeUL;
    mdContext->buf[3] = 0x10325476UL;
}

void
krb5int_MD4Update (krb5_MD4_CTX *mdContext, const unsigned char *inBuf, unsigned int inLen)
{
    krb5_ui_4 in[16];
    int mdi;
    unsigned int i, ii;

    /* compute number of bytes mod 64 */
    mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

    /* update number of bits */
    if ((mdContext->i[0] + ((krb5_ui_4)inLen << 3)) < mdContext->i[0])
        mdContext->i[1]++;
    mdContext->i[0] += ((krb5_ui_4)inLen << 3);
    mdContext->i[1] += ((krb5_ui_4)inLen >> 29);

    while (inLen--) {
        /* add new character to buffer, increment mdi */
        mdContext->in[mdi++] = *inBuf++;

        /* transform if necessary */
        if (mdi == 0x40) {
            for (i = 0, ii = 0; i < 16; i++, ii += 4) {
                in[i] = load_32_le(mdContext->in+ii);
            }
            Transform (mdContext->buf, in);
            mdi = 0;
        }
    }
}

void
krb5int_MD4Final (krb5_MD4_CTX *mdContext)
{
    krb5_ui_4 in[16];
    int mdi;
    unsigned int i, ii;
    unsigned int padLen;

    /* save number of bits */
    in[14] = mdContext->i[0];
    in[15] = mdContext->i[1];

    /* compute number of bytes mod 64 */
    mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

    /* pad out to 56 mod 64 */
    padLen = (mdi < 56) ? (56 - mdi) : (120 - mdi);
    krb5int_MD4Update (mdContext, PADDING, padLen);

    /* append length in bits and transform */
    for (i = 0, ii = 0; i < 14; i++, ii += 4)
        in[i] = load_32_le(mdContext->in+ii);
    Transform (mdContext->buf, in);


    /* store buffer in digest */
    for (i = 0, ii = 0; i < 4; i++, ii += 4) {
        store_32_le(mdContext->buf[i], mdContext->digest+ii);
    }
}

/* Basic MD4 step. Transform buf based on in.
 */
static void Transform (krb5_ui_4 *buf, krb5_ui_4 *in)
{
    krb5_ui_4 a = buf[0], b = buf[1], c = buf[2], d = buf[3];

#if defined(CONFIG_SMALL) && !defined(CONFIG_SMALL_NO_CRYPTO)
    int i;
#define ROTATE { krb5_ui_4 temp; temp = d, d = c, c = b, b = a, a = temp; }
    for (i = 0; i < 16; i++) {
        static const unsigned char round1consts[] = { 3, 7, 11, 19, };
        FF (a, b, c, d, in[i], round1consts[i%4]); ROTATE;
    }
    for (i = 0; i < 16; i++) {
        static const unsigned char round2indices[] = {
            0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15
        };
        static const unsigned char round2consts[] = { 3, 5, 9, 13 };
        GG (a, b, c, d, in[round2indices[i]], round2consts[i%4]); ROTATE;
    }
    for (i = 0; i < 16; i++) {
        static const unsigned char round3indices[] = {
            0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
        };
        static const unsigned char round3consts[] = { 3, 9, 11, 15 };
        HH (a, b, c, d, in[round3indices[i]], round3consts[i%4]); ROTATE;
    }
#else
    /* Round 1 */
    FF (a, b, c, d, in[ 0],  3);
    FF (d, a, b, c, in[ 1],  7);
    FF (c, d, a, b, in[ 2], 11);
    FF (b, c, d, a, in[ 3], 19);
    FF (a, b, c, d, in[ 4],  3);
    FF (d, a, b, c, in[ 5],  7);
    FF (c, d, a, b, in[ 6], 11);
    FF (b, c, d, a, in[ 7], 19);
    FF (a, b, c, d, in[ 8],  3);
    FF (d, a, b, c, in[ 9],  7);
    FF (c, d, a, b, in[10], 11);
    FF (b, c, d, a, in[11], 19);
    FF (a, b, c, d, in[12],  3);
    FF (d, a, b, c, in[13],  7);
    FF (c, d, a, b, in[14], 11);
    FF (b, c, d, a, in[15], 19);

    /* Round 2 */
    GG (a, b, c, d, in[ 0],  3);
    GG (d, a, b, c, in[ 4],  5);
    GG (c, d, a, b, in[ 8],  9);
    GG (b, c, d, a, in[12], 13);
    GG (a, b, c, d, in[ 1],  3);
    GG (d, a, b, c, in[ 5],  5);
    GG (c, d, a, b, in[ 9],  9);
    GG (b, c, d, a, in[13], 13);
    GG (a, b, c, d, in[ 2],  3);
    GG (d, a, b, c, in[ 6],  5);
    GG (c, d, a, b, in[10],  9);
    GG (b, c, d, a, in[14], 13);
    GG (a, b, c, d, in[ 3],  3);
    GG (d, a, b, c, in[ 7],  5);
    GG (c, d, a, b, in[11],  9);
    GG (b, c, d, a, in[15], 13);

    /* Round 3 */
    HH (a, b, c, d, in[ 0],  3);
    HH (d, a, b, c, in[ 8],  9);
    HH (c, d, a, b, in[ 4], 11);
    HH (b, c, d, a, in[12], 15);
    HH (a, b, c, d, in[ 2],  3);
    HH (d, a, b, c, in[10],  9);
    HH (c, d, a, b, in[ 6], 11);
    HH (b, c, d, a, in[14], 15);
    HH (a, b, c, d, in[ 1],  3);
    HH (d, a, b, c, in[ 9],  9);
    HH (c, d, a, b, in[ 5], 11);
    HH (b, c, d, a, in[13], 15);
    HH (a, b, c, d, in[ 3],  3);
    HH (d, a, b, c, in[11],  9);
    HH (c, d, a, b, in[ 7], 11);
    HH (b, c, d, a, in[15], 15);
#endif

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

/*
**********************************************************************
** End of md4.c                                                     **
******************************* (cut) ********************************
*/

#endif /* K5_BUILTIN_MD4 */
