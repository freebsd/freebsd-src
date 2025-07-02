/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.
 *
 * License to copy and use this software is granted provided that
 * it is identified as the "RSA Data Security, Inc. MD5 Message-
 * Digest Algorithm" in all material mentioning or referencing this
 * software or this function.
 *
 * License is also granted to make and use derivative works
 * provided that such works are identified as "derived from the RSA
 * Data Security, Inc. MD5 Message-Digest Algorithm" in all
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
***********************************************************************
** md5.c -- the source code for MD5 routines                         **
** RSA Data Security, Inc. MD5 Message-Digest Algorithm              **
** Created: 2/17/90 RLR                                              **
** Revised: 1/91 SRD,AJ,BSK,JT Reference C ver., 7/10 constant corr. **
***********************************************************************
*/

/*
 * Modified by John Carr, MIT, to use Kerberos 5 typedefs.
 */

#include "crypto_int.h"
#include "rsa-md5.h"

#ifdef K5_BUILTIN_MD5

/*
***********************************************************************
**  Message-digest routines:                                         **
**  To form the message digest for a message M                       **
**    (1) Initialize a context buffer mdContext using krb5int_MD5Init   **
**    (2) Call krb5int_MD5Update on mdContext and M                     **
**    (3) Call krb5int_MD5Final on mdContext                            **
**  The message digest is now in mdContext->digest[0...15]           **
***********************************************************************
*/

/* forward declaration */
static void Transform (krb5_ui_4 *buf, krb5_ui_4 *in);

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

/* F, G, H and I are basic MD5 functions */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) ((((x) << (n)) & 0xffffffff) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s, ac)                        \
    {(a) += F ((b), (c), (d)) + (x) + (krb5_ui_4)(ac);  \
        (a) &= 0xffffffff;                              \
        (a) = ROTATE_LEFT ((a), (s));                   \
        (a) += (b);                                     \
        (a) &= 0xffffffff;                              \
    }
#define GG(a, b, c, d, x, s, ac)                        \
    {(a) += G ((b), (c), (d)) + (x) + (krb5_ui_4)(ac);  \
        (a) &= 0xffffffff;                              \
        (a) = ROTATE_LEFT ((a), (s));                   \
        (a) += (b);                                     \
        (a) &= 0xffffffff;                              \
    }
#define HH(a, b, c, d, x, s, ac)                        \
    {(a) += H ((b), (c), (d)) + (x) + (krb5_ui_4)(ac);  \
        (a) &= 0xffffffff;                              \
        (a) = ROTATE_LEFT ((a), (s));                   \
        (a) += (b);                                     \
        (a) &= 0xffffffff;                              \
    }
#define II(a, b, c, d, x, s, ac)                        \
    {(a) += I ((b), (c), (d)) + (x) + (krb5_ui_4)(ac);  \
        (a) &= 0xffffffff;                              \
        (a) = ROTATE_LEFT ((a), (s));                   \
        (a) += (b);                                     \
        (a) &= 0xffffffff;                              \
    }

/* The routine krb5int_MD5Init initializes the message-digest context
   mdContext. All fields are set to zero.
*/
void
krb5int_MD5Init (krb5_MD5_CTX *mdContext)
{
    mdContext->i[0] = mdContext->i[1] = (krb5_ui_4)0;

    /* Load magic initialization constants.
     */
    mdContext->buf[0] = 0x67452301UL;
    mdContext->buf[1] = 0xefcdab89UL;
    mdContext->buf[2] = 0x98badcfeUL;
    mdContext->buf[3] = 0x10325476UL;
}

/* The routine krb5int_MD5Update updates the message-digest context to
   account for the presence of each of the characters inBuf[0..inLen-1]
   in the message whose digest is being computed.
*/
void
krb5int_MD5Update (krb5_MD5_CTX *mdContext, const unsigned char *inBuf, unsigned int inLen)
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
            for (i = 0, ii = 0; i < 16; i++, ii += 4)
                in[i] = load_32_le(mdContext->in+ii);
            Transform (mdContext->buf, in);
            mdi = 0;
        }
    }
}

/* The routine krb5int_MD5Final terminates the message-digest computation and
   ends with the desired message digest in mdContext->digest[0...15].
*/
void
krb5int_MD5Final (krb5_MD5_CTX *mdContext)
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
    krb5int_MD5Update (mdContext, PADDING, padLen);

    /* append length in bits and transform */
    for (i = 0, ii = 0; i < 14; i++, ii += 4)
        in[i] = load_32_le(mdContext->in+ii);
    Transform (mdContext->buf, in);

    /* store buffer in digest */
    for (i = 0, ii = 0; i < 4; i++, ii += 4) {
        store_32_le(mdContext->buf[i], mdContext->digest+ii);
    }
}

/* Basic MD5 step. Transforms buf based on in.
 */
static void Transform (krb5_ui_4 *buf, krb5_ui_4 *in)
{
    krb5_ui_4 a = buf[0], b = buf[1], c = buf[2], d = buf[3];

#if defined(CONFIG_SMALL) && !defined(CONFIG_SMALL_NO_CRYPTO)

    int i;
#define ROTATE { krb5_ui_4 temp; temp = d, d = c, c = b, b = a, a = temp; }
    for (i = 0; i < 16; i++) {
        const unsigned char round1s[] = { 7, 12, 17, 22 };
        const krb5_ui_4 round1consts[] = {
            3614090360UL,   3905402710UL,    606105819UL,   3250441966UL,
            4118548399UL,   1200080426UL,   2821735955UL,   4249261313UL,
            1770035416UL,   2336552879UL,   4294925233UL,   2304563134UL,
            1804603682UL,   4254626195UL,   2792965006UL,   1236535329UL,
        };
        FF (a, b, c, d, in[i], round1s[i%4], round1consts[i]);
        ROTATE;
    }
    for (i = 0; i < 16; i++) {
        const unsigned char round2s[] = { 5, 9, 14, 20 };
        const krb5_ui_4 round2consts[] = {
            4129170786UL,   3225465664UL,    643717713UL,   3921069994UL,
            3593408605UL,     38016083UL,   3634488961UL,   3889429448UL,
            568446438UL,   3275163606UL,   4107603335UL,   1163531501UL,
            2850285829UL,   4243563512UL,   1735328473UL,   2368359562UL,
        };
        int r2index = (1 + i * 5) % 16;
        GG (a, b, c, d, in[r2index], round2s[i%4], round2consts[i]);
        ROTATE;
    }
    for (i = 0; i < 16; i++) {
        static const unsigned char round3s[] = { 4, 11, 16, 23 };
        static const krb5_ui_4 round3consts[] = {
            4294588738UL,   2272392833UL,   1839030562UL,   4259657740UL,
            2763975236UL,   1272893353UL,   4139469664UL,   3200236656UL,
            681279174UL,   3936430074UL,   3572445317UL,     76029189UL,
            3654602809UL,   3873151461UL,    530742520UL,   3299628645UL,
        };
        int r3index = (5 + i * 3) % 16;
        HH (a, b, c, d, in[r3index], round3s[i%4], round3consts[i]);
        ROTATE;
    }
    for (i = 0; i < 16; i++) {
        static const unsigned char round4s[] = { 6, 10, 15, 21 };
        static const krb5_ui_4 round4consts[] = {
            4096336452UL,   1126891415UL,   2878612391UL,   4237533241UL,
            1700485571UL,   2399980690UL,   4293915773UL,   2240044497UL,
            1873313359UL,   4264355552UL,   2734768916UL,   1309151649UL,
            4149444226UL,   3174756917UL,    718787259UL,   3951481745UL,
        };
        int r4index = (7 * i) % 16;
        II (a, b, c, d, in[r4index], round4s[i%4], round4consts[i]);
        ROTATE;
    }

#else

    /* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
    FF ( a, b, c, d, in[ 0], S11, 3614090360UL); /* 1 */
    FF ( d, a, b, c, in[ 1], S12, 3905402710UL); /* 2 */
    FF ( c, d, a, b, in[ 2], S13,  606105819UL); /* 3 */
    FF ( b, c, d, a, in[ 3], S14, 3250441966UL); /* 4 */
    FF ( a, b, c, d, in[ 4], S11, 4118548399UL); /* 5 */
    FF ( d, a, b, c, in[ 5], S12, 1200080426UL); /* 6 */
    FF ( c, d, a, b, in[ 6], S13, 2821735955UL); /* 7 */
    FF ( b, c, d, a, in[ 7], S14, 4249261313UL); /* 8 */
    FF ( a, b, c, d, in[ 8], S11, 1770035416UL); /* 9 */
    FF ( d, a, b, c, in[ 9], S12, 2336552879UL); /* 10 */
    FF ( c, d, a, b, in[10], S13, 4294925233UL); /* 11 */
    FF ( b, c, d, a, in[11], S14, 2304563134UL); /* 12 */
    FF ( a, b, c, d, in[12], S11, 1804603682UL); /* 13 */
    FF ( d, a, b, c, in[13], S12, 4254626195UL); /* 14 */
    FF ( c, d, a, b, in[14], S13, 2792965006UL); /* 15 */
    FF ( b, c, d, a, in[15], S14, 1236535329UL); /* 16 */

    /* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
    GG ( a, b, c, d, in[ 1], S21, 4129170786UL); /* 17 */
    GG ( d, a, b, c, in[ 6], S22, 3225465664UL); /* 18 */
    GG ( c, d, a, b, in[11], S23,  643717713UL); /* 19 */
    GG ( b, c, d, a, in[ 0], S24, 3921069994UL); /* 20 */
    GG ( a, b, c, d, in[ 5], S21, 3593408605UL); /* 21 */
    GG ( d, a, b, c, in[10], S22,   38016083UL); /* 22 */
    GG ( c, d, a, b, in[15], S23, 3634488961UL); /* 23 */
    GG ( b, c, d, a, in[ 4], S24, 3889429448UL); /* 24 */
    GG ( a, b, c, d, in[ 9], S21,  568446438UL); /* 25 */
    GG ( d, a, b, c, in[14], S22, 3275163606UL); /* 26 */
    GG ( c, d, a, b, in[ 3], S23, 4107603335UL); /* 27 */
    GG ( b, c, d, a, in[ 8], S24, 1163531501UL); /* 28 */
    GG ( a, b, c, d, in[13], S21, 2850285829UL); /* 29 */
    GG ( d, a, b, c, in[ 2], S22, 4243563512UL); /* 30 */
    GG ( c, d, a, b, in[ 7], S23, 1735328473UL); /* 31 */
    GG ( b, c, d, a, in[12], S24, 2368359562UL); /* 32 */

    /* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
    HH ( a, b, c, d, in[ 5], S31, 4294588738UL); /* 33 */
    HH ( d, a, b, c, in[ 8], S32, 2272392833UL); /* 34 */
    HH ( c, d, a, b, in[11], S33, 1839030562UL); /* 35 */
    HH ( b, c, d, a, in[14], S34, 4259657740UL); /* 36 */
    HH ( a, b, c, d, in[ 1], S31, 2763975236UL); /* 37 */
    HH ( d, a, b, c, in[ 4], S32, 1272893353UL); /* 38 */
    HH ( c, d, a, b, in[ 7], S33, 4139469664UL); /* 39 */
    HH ( b, c, d, a, in[10], S34, 3200236656UL); /* 40 */
    HH ( a, b, c, d, in[13], S31,  681279174UL); /* 41 */
    HH ( d, a, b, c, in[ 0], S32, 3936430074UL); /* 42 */
    HH ( c, d, a, b, in[ 3], S33, 3572445317UL); /* 43 */
    HH ( b, c, d, a, in[ 6], S34,   76029189UL); /* 44 */
    HH ( a, b, c, d, in[ 9], S31, 3654602809UL); /* 45 */
    HH ( d, a, b, c, in[12], S32, 3873151461UL); /* 46 */
    HH ( c, d, a, b, in[15], S33,  530742520UL); /* 47 */
    HH ( b, c, d, a, in[ 2], S34, 3299628645UL); /* 48 */

    /* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
    II ( a, b, c, d, in[ 0], S41, 4096336452UL); /* 49 */
    II ( d, a, b, c, in[ 7], S42, 1126891415UL); /* 50 */
    II ( c, d, a, b, in[14], S43, 2878612391UL); /* 51 */
    II ( b, c, d, a, in[ 5], S44, 4237533241UL); /* 52 */
    II ( a, b, c, d, in[12], S41, 1700485571UL); /* 53 */
    II ( d, a, b, c, in[ 3], S42, 2399980690UL); /* 54 */
    II ( c, d, a, b, in[10], S43, 4293915773UL); /* 55 */
    II ( b, c, d, a, in[ 1], S44, 2240044497UL); /* 56 */
    II ( a, b, c, d, in[ 8], S41, 1873313359UL); /* 57 */
    II ( d, a, b, c, in[15], S42, 4264355552UL); /* 58 */
    II ( c, d, a, b, in[ 6], S43, 2734768916UL); /* 59 */
    II ( b, c, d, a, in[13], S44, 1309151649UL); /* 60 */
    II ( a, b, c, d, in[ 4], S41, 4149444226UL); /* 61 */
    II ( d, a, b, c, in[11], S42, 3174756917UL); /* 62 */
    II ( c, d, a, b, in[ 2], S43,  718787259UL); /* 63 */
    II ( b, c, d, a, in[ 9], S44, 3951481745UL); /* 64 */

#endif /* small? */

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

#endif /* K5_BUILTIN_MD5 */
