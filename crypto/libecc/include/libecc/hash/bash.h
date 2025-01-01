/*
 *  Copyright (C) 2022 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __BASH_H__
#define __BASH_H__

#include <libecc/words/words.h>

/*
 * This is an implementation of the BASH hash functions family (for sizes 224, 256, 384 and 512)
 * following the standard STB 34.101.77-2020 (http://apmi.bsu.by/assets/files/std/bash-spec24.pdf).
 * An english version of the specifications exist here: https://eprint.iacr.org/2016/587.pdf
 */

#define _BASH_ROTHI_(x, y) (((x) << (y)) | ((x) >> ((sizeof(u64) * 8) - (y))))

/* We handle the case where one of the shifts is more than 64-bit: in this
 * case, behaviour is undefined as per ANSI C definition. In this case, we
 * return the untouched input.
 */
#define BASH_ROTHI(x, y) ((((y) < (sizeof(u64) * 8)) && ((y) > 0)) ? (_BASH_ROTHI_(x, y)) : (x))

/*
 * Round transformation of the state. Notations are the
 * same as the ones used in:
 * https://eprint.iacr.org/2016/587.pdf
 */
#define BASH_SLICES_X   3
#define BASH_SLICES_Y   8
#define BASH_ROT_ROUNDS 8
#define BASH_ROT_IDX    4
#define BASH_ROUNDS     24

#define BASH_L3_S3(W0, W1, W2, m1, n1, m2, n2) do {				\
	u64 T0, T1, T2;								\
	T0 = BASH_ROTHI(W0, m1);						\
	W0 = (W0 ^ W1 ^ W2);							\
	T1 = (W1 ^ BASH_ROTHI(W0, n1));						\
	W1 = (T0 ^ T1);								\
	W2 = (W2 ^ BASH_ROTHI(W2, m2) ^ BASH_ROTHI(T1, n2));			\
	T0 = (~(W2));								\
	T1 = (W0 | W2);								\
	T2 = (W0 & W1);								\
	T0 = (T0 | W1);								\
	W1 = (W1 ^ T1);								\
	W2 = (W2 ^ T2);								\
	W0 = (W0 ^ T0);								\
} while(0)

#define BASH_PERMUTE(S) do {							\
	u64 S_[BASH_SLICES_X * BASH_SLICES_Y];					\
	IGNORE_RET_VAL(local_memcpy(S_, S, sizeof(S_)));			\
	S[ 0] = S_[15]; S[ 1] = S_[10]; S[ 2] = S_[ 9]; S[ 3] = S_[12];		\
	S[ 4] = S_[11]; S[ 5] = S_[14]; S[ 6] = S_[13]; S[ 7] = S_[ 8];		\
	S[ 8] = S_[17]; S[ 9] = S_[16]; S[10] = S_[19]; S[11] = S_[18];		\
	S[12] = S_[21]; S[13] = S_[20]; S[14] = S_[23]; S[15] = S_[22];		\
	S[16] = S_[ 6]; S[17] = S_[ 3]; S[18] = S_[ 0]; S[19] = S_[ 5];		\
	S[20] = S_[ 2]; S[21] = S_[ 7]; S[22] = S_[ 4]; S[23] = S_[ 1];		\
} while(0)

static const u64 bash_rc[BASH_ROUNDS] =
{
	0x3bf5080ac8ba94b1ULL,
	0xc1d1659c1bbd92f6ULL,
	0x60e8b2ce0ddec97bULL,
	0xec5fb8fe790fbc13ULL,
	0xaa043de6436706a7ULL,
	0x8929ff6a5e535bfdULL,
	0x98bf1e2c50c97550ULL,
	0x4c5f8f162864baa8ULL,
	0x262fc78b14325d54ULL,
	0x1317e3c58a192eaaULL,
	0x098bf1e2c50c9755ULL,
	0xd8ee19681d669304ULL,
	0x6c770cb40eb34982ULL,
	0x363b865a0759a4c1ULL,
	0xc73622b47c4c0aceULL,
	0x639b115a3e260567ULL,
	0xede6693460f3da1dULL,
	0xaad8d5034f9935a0ULL,
	0x556c6a81a7cc9ad0ULL,
	0x2ab63540d3e64d68ULL,
	0x155b1aa069f326b4ULL,
	0x0aad8d5034f9935aULL,
	0x0556c6a81a7cc9adULL,
	0xde8082cd72debc78ULL,
};

static const u8 bash_rot[BASH_ROT_ROUNDS][BASH_ROT_IDX] =
{
	{  8, 53, 14,  1 },
	{ 56, 51, 34,  7 },
	{  8, 37, 46, 49 },
	{ 56,  3,  2, 23 },
	{  8, 21, 14, 33 },
	{ 56, 19, 34, 39 },
	{  8,  5, 46, 17 },
	{ 56, 35,  2, 55 },
};

/* Macro to handle endianness conversion */
#define SWAP64(A) do {								\
	A = ((A) << 56 | ((A) & 0xff00) << 40 | ((A) & 0xff0000) << 24 |	\
	    ((A) & 0xff000000) << 8 | ((A) >> 8 & 0xff000000) |			\
	    ((A) >> 24 & 0xff0000) | ((A) >> 40 & 0xff00) | (A) >> 56);		\
} while(0)

/* The main Bash-f core as descibed in the specification. */
#define BASHF(S, end) do {									\
	unsigned int round, i;									\
	/* Swap endianness if necessary */							\
	if(end == BASH_BIG){									\
		for(i = 0; i < (BASH_SLICES_X * BASH_SLICES_Y); i++){				\
			SWAP64(S[i]);								\
		}										\
	}											\
	for(round = 0; round < BASH_ROUNDS; round++){						\
		unsigned int v;									\
		for(v = 0; v < 8; v++){								\
			BASH_L3_S3(S[v], S[v+8], S[v+16], bash_rot[v][0], bash_rot[v][1],	\
							bash_rot[v][2], bash_rot[v][3]);	\
		}										\
		BASH_PERMUTE(S);								\
		S[23] ^= bash_rc[round];							\
	}											\
	/* Swap back endianness if necessary */							\
	if(end == BASH_BIG){									\
		for(i = 0; i < (BASH_SLICES_X * BASH_SLICES_Y); i++){				\
			SWAP64(S[i]);								\
		}										\
	}											\
} while(0)

typedef enum {
	BASH_LITTLE = 0,
	BASH_BIG = 1,
} bash_endianness;

typedef struct bash_context_ {
	u8 bash_digest_size;
	u8 bash_block_size;
	bash_endianness bash_endian;
	/* Local counter */
	u64 bash_total;
	/* Bash state */
	u64 bash_state[BASH_SLICES_X * BASH_SLICES_Y];
	/* Initialization magic value */
	u64 magic;
} bash_context;

ATTRIBUTE_WARN_UNUSED_RET int _bash_init(bash_context *ctx, uint8_t digest_size);
ATTRIBUTE_WARN_UNUSED_RET int _bash_update(bash_context *ctx, const uint8_t *buf, uint32_t buflen);
ATTRIBUTE_WARN_UNUSED_RET int _bash_finalize(bash_context *ctx, uint8_t *output);

#endif /* __BASH_H__ */
