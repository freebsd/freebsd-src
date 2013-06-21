/*
 * This is work is derived from material Copyright RSA Data Security, Inc.
 *
 * The RSA copyright statement and Licence for that original material is
 * included below. This is followed by the Apache copyright statement and
 * licence for the modifications made to that material.
 */

/* MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * The apr_md5_encode() routine uses much code obtained from the FreeBSD 3.0
 * MD5 crypt() function, which is licenced as follows:
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/*
 * pseudo_md5.c:  md5-esque hash sum calculation for short data blocks.
 *                Code taken and adapted from the APR (see licenses above).
 */
#include "private/svn_pseudo_md5.h"

/* Constants for MD5 calculation.
 */

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#if defined(_MSC_VER) && _MSC_VER >= 1310
#pragma intrinsic(_rotl)
#define ROTATE_LEFT(x, n) (_rotl(x,n))
#else
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))
#endif

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
 * Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (apr_uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (apr_uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (apr_uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (apr_uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

/* The idea of the functions below is as follows:
 *
 * - The core MD5 algorithm does not assume that the "important" data
 *   is at the begin of the encryption block, followed by e.g. 0.
 *   Instead, all bits are equally relevant.
 *
 * - If some bytes in the input are known to be 0, we may hard-code them.
 *   With the previous property, it is safe to move them to the upper end
 *   of the encryption block to maximize the number of steps that can be
 *   pre-calculated.
 *
 * - Variable-length streams will use the upper 8 byte of the last
 *   encryption block to store the stream length in bits (to make 0, 00,
 *   000, ... etc. produce different hash sums).
 *
 * - We will hash at most 63 bytes, i.e. 504 bits.  In the standard stream
 *   implementation, the upper 6 bytes of the last encryption block would
 *   be 0.  We will put at least one non-NULL value in the last 4 bytes.
 *   Therefore, our input will always be different to a standard MD5 stream
 *   implementation in either block count, content or both.
 *
 * - Our length indicator also varies with the number bytes in the input.
 *   Hence, different pseudo-MD5 input length produces different output
 *   (with "cryptographic probability") even if the content is all 0 or
 *   otherwise identical.
 *
 * - Collisions between pseudo-MD5 and pseudo-MD5 as well as pseudo-MD5
 *   and standard MD5 are as likely as any other MD5 collision.
 */

void svn__pseudo_md5_15(apr_uint32_t digest[4],
                        const apr_uint32_t x[4])
{
    apr_uint32_t a = 0x67452301;
    apr_uint32_t b = 0xefcdab89;
    apr_uint32_t c = 0x98badcfe;
    apr_uint32_t d = 0x10325476;

    /* make sure byte 63 gets the marker independently of BE / LE */
    apr_uint32_t x3n = x[3] ^ 0xffffffff;

    /* Round 1 */
    FF(a, b, c, d, 0,    S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, 0,    S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, 0,    S13, 0x242070db); /* 3 */
    FF(b, c, d, a, 0,    S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, 0,    S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, 0,    S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, 0,    S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, 0,    S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, 0,    S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, 0,    S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, 0,    S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, 0,    S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[0], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[1], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[2], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x3n,  S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, 0,    S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, 0,    S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, 0,    S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, 0,    S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, 0,    S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, 0,    S22, 0x2441453);  /* 22 */
    GG(c, d, a, b, x3n,  S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, 0,    S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, 0,    S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[2], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, 0,    S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, 0,    S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[1], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, 0,    S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, 0,    S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[0], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, 0,    S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, 0,    S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, 0,    S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[2], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, 0,    S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, 0,    S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, 0,    S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, 0,    S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[1], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, 0,    S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, 0,    S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, 0,    S34, 0x4881d05);  /* 44 */
    HH(a, b, c, d, 0,    S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[0], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x3n,  S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, 0,    S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II(a, b, c, d, 0,    S41, 0xf4292244); /* 49 */
    II(d, a, b, c, 0,    S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[2], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, 0,    S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[0], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, 0,    S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, 0,    S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, 0,    S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, 0,    S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x3n,  S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, 0,    S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[1], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, 0,    S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, 0,    S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, 0,    S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, 0,    S44, 0xeb86d391); /* 64 */

    digest[0] = a;
    digest[1] = b;
    digest[2] = c;
    digest[3] = d;
}

void svn__pseudo_md5_31(apr_uint32_t digest[4],
                        const apr_uint32_t x[8])
{
    apr_uint32_t a = 0x67452301;
    apr_uint32_t b = 0xefcdab89;
    apr_uint32_t c = 0x98badcfe;
    apr_uint32_t d = 0x10325476;

    /* make sure byte 63 gets the marker independently of BE / LE */
    apr_uint32_t x7n = x[7] ^ 0xfefefefe;

    /* Round 1 */
    FF(a, b, c, d, 0,    S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, 0,    S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, 0,    S13, 0x242070db); /* 3 */
    FF(b, c, d, a, 0,    S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, 0,    S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, 0,    S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, 0,    S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, 0,    S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[0], S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[1], S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[2], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[3], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[4], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[5], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[6], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x7n,  S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, 0,    S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, 0,    S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[3], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, 0,    S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, 0,    S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[2], S22, 0x2441453);  /* 22 */
    GG(c, d, a, b, x7n,  S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, 0,    S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[1], S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[6], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, 0,    S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[0], S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[5], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, 0,    S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, 0,    S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[4], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, 0,    S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[0], S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[3], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[6], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, 0,    S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, 0,    S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, 0,    S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[2], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[5], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, 0,    S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, 0,    S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, 0,    S34, 0x4881d05);  /* 44 */
    HH(a, b, c, d, x[1], S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[4], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x7n,  S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, 0,    S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II(a, b, c, d, 0,    S41, 0xf4292244); /* 49 */
    II(d, a, b, c, 0,    S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[6], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, 0,    S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[4], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, 0,    S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[2], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, 0,    S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[0], S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x7n,  S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, 0,    S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[5], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, 0,    S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[3], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, 0,    S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[1], S44, 0xeb86d391); /* 64 */

    digest[0] = a;
    digest[1] = b;
    digest[2] = c;
    digest[3] = d;
}

void svn__pseudo_md5_63(apr_uint32_t digest[4],
                        const apr_uint32_t x[16])
{
    apr_uint32_t a = 0x67452301;
    apr_uint32_t b = 0xefcdab89;
    apr_uint32_t c = 0x98badcfe;
    apr_uint32_t d = 0x10325476;

    /* make sure byte 63 gets the marker independently of BE / LE */
    apr_uint32_t x15n = x[15] ^ 0xfcfcfcfc;

    /* Round 1 */
    FF(a, b, c, d, x[0],  S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, x[1],  S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, x[2],  S13, 0x242070db); /* 3 */
    FF(b, c, d, a, x[3],  S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, x[4],  S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, x[5],  S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, x[6],  S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, x[7],  S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[8],  S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[9],  S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x15n,  S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, x[1],  S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, x[6],  S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[0],  S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, x[5],  S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[10], S22, 0x2441453);  /* 22 */
    GG(c, d, a, b, x15n,  S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[4],  S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[9],  S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[3],  S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[8],  S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[2],  S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, x[7],  S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, x[5],  S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[8],  S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[1],  S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, x[4],  S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, x[7],  S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[0],  S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, x[3],  S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, x[6],  S34, 0x4881d05);  /* 44 */
    HH(a, b, c, d, x[9],  S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x15n,  S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[2],  S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II(a, b, c, d, x[0],  S41, 0xf4292244); /* 49 */
    II(d, a, b, c, x[7],  S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[5],  S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[3],  S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[1],  S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[8],  S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x15n,  S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[6],  S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[4],  S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[2],  S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[9],  S44, 0xeb86d391); /* 64 */

    digest[0] = a;
    digest[1] = b;
    digest[2] = c;
    digest[3] = d;
}
