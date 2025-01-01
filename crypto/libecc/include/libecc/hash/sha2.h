/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __SHA2_H__
#define __SHA2_H__

#include <libecc/words/words.h>

/* Useful primitives for handling 128-bit */

/* Add a 128-bit to a 64-bit element and store the result
 * in the input
 */
#define ADD_UINT128_UINT64(low,high,toadd) do {\
	(low) += (toadd);\
	if((low) < (toadd)){\
		(high)++;\
	}\
} while(0)

/* Store a 128-bit element in big endian format */
#define PUT_UINT128_BE(low,high,b,i) do {\
	PUT_UINT64_BE((high), (b), (i));\
	PUT_UINT64_BE((low), (b), (i)+8);\
} while(0)

/* Multiply a 128-bit element by 8 and store it in big endian
 * format
 */
#define PUT_MUL8_UINT128_BE(low,high,b,i) do {\
	u64 reslow, reshigh;\
	reslow = (low) << 3;\
	reshigh = ((low) >> 61) ^ ((high) << 3);\
	PUT_UINT128_BE(reslow,reshigh,(b),(i));\
} while(0)

/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n, b, i)				\
do {							\
	(n) =     ( ((u32) (b)[(i)	   ]) << 24 )	\
		| ( ((u32) (b)[(i) + 1]) << 16 )	\
		| ( ((u32) (b)[(i) + 2]) <<  8 )	\
		| ( ((u32) (b)[(i) + 3])       );	\
} while( 0 )
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n, b, i)			\
do {						\
	(b)[(i)    ] = (u8) ( (n) >> 24 );	\
	(b)[(i) + 1] = (u8) ( (n) >> 16 );	\
	(b)[(i) + 2] = (u8) ( (n) >>  8 );	\
	(b)[(i) + 3] = (u8) ( (n)       );	\
} while( 0 )
#endif

/*
 * 64-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT64_BE
#define GET_UINT64_BE(n,b,i)				\
do {							\
    (n) = ( ((u64) (b)[(i)	   ]) << 56 )		\
	| ( ((u64) (b)[(i) + 1]) << 48 )		\
	| ( ((u64) (b)[(i) + 2]) << 40 )		\
	| ( ((u64) (b)[(i) + 3]) << 32 )		\
	| ( ((u64) (b)[(i) + 4]) << 24 )		\
	| ( ((u64) (b)[(i) + 5]) << 16 )		\
	| ( ((u64) (b)[(i) + 6]) <<  8 )		\
	| ( ((u64) (b)[(i) + 7])	    );		\
} while( 0 )
#endif /* GET_UINT64_BE */

#ifndef PUT_UINT64_BE
#define PUT_UINT64_BE(n,b,i)		\
do {					\
    (b)[(i)    ] = (u8) ( (n) >> 56 );	\
    (b)[(i) + 1] = (u8) ( (n) >> 48 );	\
    (b)[(i) + 2] = (u8) ( (n) >> 40 );	\
    (b)[(i) + 3] = (u8) ( (n) >> 32 );	\
    (b)[(i) + 4] = (u8) ( (n) >> 24 );	\
    (b)[(i) + 5] = (u8) ( (n) >> 16 );	\
    (b)[(i) + 6] = (u8) ( (n) >>  8 );	\
    (b)[(i) + 7] = (u8) ( (n)       );	\
} while( 0 )
#endif /* PUT_UINT64_BE */

/* Useful macros for the SHA-2 core function  */
#define CH(x, y, z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define MAJ(x, y, z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define UPDATEW(w, i, sha_type) ((w)[(i)] = SIGMA_MIN1_##sha_type((w)[(i)-2]) + (w)[(i)-7] + SIGMA_MIN0_##sha_type((w)[(i)-15]) + (w)[(i)-16])

#define SHA2CORE(a, b, c, d, e, f, g, h, w, k, sha_word_type, sha_type) do {\
	sha_word_type t1, t2;\
	t1 = (h) + SIGMA_MAJ1_##sha_type((e)) + CH((e), (f), (g)) + (k) + (w);\
	t2 = SIGMA_MAJ0_##sha_type((a)) + MAJ((a), (b), (c));\
	(h) = (g);\
	(g) = (f);\
	(f) = (e);\
	(e) = (d) + t1;\
	(d) = (c);\
	(c) = (b);\
	(b) = (a);\
	(a) = t1 + t2;\
} while(0)

#if (defined(WITH_HASH_SHA224) || defined(WITH_HASH_SHA256))

/**********************************************/

/* SHA-224 and SHA-256 */
#define SHR_SHA256(x, n)       (((u32)(x)) >> (n))
#define ROTR_SHA256(x, n)      ((((u32)(x)) >> (n)) | (((u32)(x)) << (32-(n))))
#define SIGMA_MAJ0_SHA256(x)   (ROTR_SHA256(x, 2)  ^ ROTR_SHA256(x, 13) ^ ROTR_SHA256(x, 22))
#define SIGMA_MAJ1_SHA256(x)   (ROTR_SHA256(x, 6)  ^ ROTR_SHA256(x, 11) ^ ROTR_SHA256(x, 25))
#define SIGMA_MIN0_SHA256(x)   (ROTR_SHA256(x, 7)  ^ ROTR_SHA256(x, 18) ^ SHR_SHA256(x, 3))
#define SIGMA_MIN1_SHA256(x)   (ROTR_SHA256(x, 17) ^ ROTR_SHA256(x, 19) ^ SHR_SHA256(x, 10))
#define SHA2CORE_SHA256(a, b, c, d, e, f, g, h, w, k) \
	SHA2CORE(a, b, c, d, e, f, g, h, w, k, u32, SHA256)
#define UPDATEW_SHA256(w, i) UPDATEW(w, i, SHA256)
static const u32 K_SHA256[] = {
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
	0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
	0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
	0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
	0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
	0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
	0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
	0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
	0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
	0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
	0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
	0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
	0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
	0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
	0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

/**********************************************/
#endif

#if (defined(WITH_HASH_SHA384) || defined(WITH_HASH_SHA512))

/**********************************************/

/* SHA-384 and SHA-512 */
#define SHR_SHA512(x, n)       (((u64)(x)) >> (n))
#define ROTR_SHA512(x, n)      ((((u64)(x)) >> (n)) | (((u64)(x)) << (64-(n))))
#define SIGMA_MAJ0_SHA512(x)   (ROTR_SHA512(x, 28) ^ ROTR_SHA512(x, 34) ^ ROTR_SHA512(x, 39))
#define SIGMA_MAJ1_SHA512(x)   (ROTR_SHA512(x, 14) ^ ROTR_SHA512(x, 18) ^ ROTR_SHA512(x, 41))
#define SIGMA_MIN0_SHA512(x)   (ROTR_SHA512(x, 1)  ^ ROTR_SHA512(x, 8)	^ SHR_SHA512(x, 7))
#define SIGMA_MIN1_SHA512(x)   (ROTR_SHA512(x, 19) ^ ROTR_SHA512(x, 61) ^ SHR_SHA512(x, 6))
#define SHA2CORE_SHA512(a, b, c, d, e, f, g, h, w, k) \
	SHA2CORE(a, b, c, d, e, f, g, h, w, k, u64, SHA512)
#define UPDATEW_SHA512(w, i) UPDATEW(w, i, SHA512)
static const u64 K_SHA512[] = {
	(u64)(0x428A2F98D728AE22), (u64)(0x7137449123EF65CD),
	(u64)(0xB5C0FBCFEC4D3B2F), (u64)(0xE9B5DBA58189DBBC),
	(u64)(0x3956C25BF348B538), (u64)(0x59F111F1B605D019),
	(u64)(0x923F82A4AF194F9B), (u64)(0xAB1C5ED5DA6D8118),
	(u64)(0xD807AA98A3030242), (u64)(0x12835B0145706FBE),
	(u64)(0x243185BE4EE4B28C), (u64)(0x550C7DC3D5FFB4E2),
	(u64)(0x72BE5D74F27B896F), (u64)(0x80DEB1FE3B1696B1),
	(u64)(0x9BDC06A725C71235), (u64)(0xC19BF174CF692694),
	(u64)(0xE49B69C19EF14AD2), (u64)(0xEFBE4786384F25E3),
	(u64)(0x0FC19DC68B8CD5B5), (u64)(0x240CA1CC77AC9C65),
	(u64)(0x2DE92C6F592B0275), (u64)(0x4A7484AA6EA6E483),
	(u64)(0x5CB0A9DCBD41FBD4), (u64)(0x76F988DA831153B5),
	(u64)(0x983E5152EE66DFAB), (u64)(0xA831C66D2DB43210),
	(u64)(0xB00327C898FB213F), (u64)(0xBF597FC7BEEF0EE4),
	(u64)(0xC6E00BF33DA88FC2), (u64)(0xD5A79147930AA725),
	(u64)(0x06CA6351E003826F), (u64)(0x142929670A0E6E70),
	(u64)(0x27B70A8546D22FFC), (u64)(0x2E1B21385C26C926),
	(u64)(0x4D2C6DFC5AC42AED), (u64)(0x53380D139D95B3DF),
	(u64)(0x650A73548BAF63DE), (u64)(0x766A0ABB3C77B2A8),
	(u64)(0x81C2C92E47EDAEE6), (u64)(0x92722C851482353B),
	(u64)(0xA2BFE8A14CF10364), (u64)(0xA81A664BBC423001),
	(u64)(0xC24B8B70D0F89791), (u64)(0xC76C51A30654BE30),
	(u64)(0xD192E819D6EF5218), (u64)(0xD69906245565A910),
	(u64)(0xF40E35855771202A), (u64)(0x106AA07032BBD1B8),
	(u64)(0x19A4C116B8D2D0C8), (u64)(0x1E376C085141AB53),
	(u64)(0x2748774CDF8EEB99), (u64)(0x34B0BCB5E19B48A8),
	(u64)(0x391C0CB3C5C95A63), (u64)(0x4ED8AA4AE3418ACB),
	(u64)(0x5B9CCA4F7763E373), (u64)(0x682E6FF3D6B2B8A3),
	(u64)(0x748F82EE5DEFB2FC), (u64)(0x78A5636F43172F60),
	(u64)(0x84C87814A1F0AB72), (u64)(0x8CC702081A6439EC),
	(u64)(0x90BEFFFA23631E28), (u64)(0xA4506CEBDE82BDE9),
	(u64)(0xBEF9A3F7B2C67915), (u64)(0xC67178F2E372532B),
	(u64)(0xCA273ECEEA26619C), (u64)(0xD186B8C721C0C207),
	(u64)(0xEADA7DD6CDE0EB1E), (u64)(0xF57D4F7FEE6ED178),
	(u64)(0x06F067AA72176FBA), (u64)(0x0A637DC5A2C898A6),
	(u64)(0x113F9804BEF90DAE), (u64)(0x1B710B35131C471B),
	(u64)(0x28DB77F523047D84), (u64)(0x32CAAB7B40C72493),
	(u64)(0x3C9EBE0A15C9BEBC), (u64)(0x431D67C49C100D4C),
	(u64)(0x4CC5D4BECB3E42B6), (u64)(0x597F299CFC657E2A),
	(u64)(0x5FCB6FAB3AD6FAEC), (u64)(0x6C44198C4A475817)
};

/**********************************************/
#endif

#endif /* __SHA2_H__ */
