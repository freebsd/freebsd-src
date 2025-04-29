/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/lib_ecc_config.h>
#ifdef WITH_HASH_SM3

#include <libecc/hash/sm3.h>

/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n, b, i)				\
do {							\
	(n) =	  ( ((u32) (b)[(i)   ])  << 24 )	\
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



static const u32 SM3_Tj_low  = 0x79cc4519;
static const u32 SM3_Tj_high = 0x7a879d8a;

/* Boolean functions FF_j and GG_j for 0 <= j <= 15 */
#define FF_j_low(X, Y, Z) (((u32)(X)) ^ ((u32)(Y)) ^ ((u32)(Z)))
#define GG_j_low(X, Y, Z) (((u32)(X)) ^ ((u32)(Y)) ^ ((u32)(Z)))

/* Boolean functions FF_j and GG_j for 16 <= j <= 63 */
#define FF_j_high(X, Y, Z) ((((u32)(X)) & ((u32)(Y))) | \
			    (((u32)(X)) & ((u32)(Z))) | \
			    (((u32)(Y)) & ((u32)(Z))))
#define GG_j_high(X, Y, Z) ((((u32)(X)) & ((u32)(Y))) | \
			    ((~((u32)(X))) & ((u32)(Z))))

/* 32-bit bitwise cyclic shift. Only support shifts value y < 32 */
#define _SM3_ROTL_(x, y) ((((u32)(x)) << (y)) | \
			(((u32)(x)) >> ((sizeof(u32) * 8) - (y))))

#define SM3_ROTL(x, y) ((((y) < (sizeof(u32) * 8)) && ((y) > 0)) ? (_SM3_ROTL_(x, y)) : (x))

/* Permutation Functions P_0 and P_1 */
#define SM3_P_0(X) (((u32)X) ^ SM3_ROTL((X),  9) ^ SM3_ROTL((X), 17))
#define SM3_P_1(X) (((u32)X) ^ SM3_ROTL((X), 15) ^ SM3_ROTL((X), 23))

/* SM3 Iterative Compression Process
 * NOTE: ctx and data sanity checks are performed by the caller (this is an internal function)
 */
ATTRIBUTE_WARN_UNUSED_RET static int sm3_process(sm3_context *ctx, const u8 data[SM3_BLOCK_SIZE])
{
	u32 A, B, C, D, E, F, G, H;
	u32 SS1, SS2, TT1, TT2;
	u32 W[68 + 64];
	unsigned int j;
	int ret;

	/* Message Expansion Function ME */

	for (j = 0; j < 16; j++) {
		GET_UINT32_BE(W[j], data, 4 * j);
	}

	for (j = 16; j < 68; j++) {
		W[j] = SM3_P_1(W[j - 16] ^ W[j - 9] ^ (SM3_ROTL(W[j - 3], 15))) ^
		       (SM3_ROTL(W[j - 13], 7)) ^ W[j - 6];
	}

	for (j = 0; j < 64; j++) {
	   W[j + 68] = W[j] ^ W[j + 4];
	}

	/* Compression Function CF */

	A = ctx->sm3_state[0];
	B = ctx->sm3_state[1];
	C = ctx->sm3_state[2];
	D = ctx->sm3_state[3];
	E = ctx->sm3_state[4];
	F = ctx->sm3_state[5];
	G = ctx->sm3_state[6];
	H = ctx->sm3_state[7];

	/*
	 * Note: in a previous version of the code, we had two loops for j from
	 * 0 to 15 and then from 16 to 63 with SM3_ROTL(SM3_Tj_low, (j & 0x1F))
	 * inside but clang-12 was smart enough to detect cases where SM3_ROTL
	 * macro is useless. On the other side, clang address sanitizer does not
	 * allow to remove the check for too high shift values in the macro
	 * itself. Creating 3 distinct loops instead of 2 to remove the & 0x1F
	 * is sufficient to satisfy everyone.
	 */

	for (j = 0; j < 16; j++) {
		SS1 = SM3_ROTL(SM3_ROTL(A, 12) + E + SM3_ROTL(SM3_Tj_low, j),7);
		SS2 = SS1 ^ SM3_ROTL(A, 12);
		TT1 = FF_j_low(A, B, C) + D + SS2 + W[j + 68];
		TT2 = GG_j_low(E, F, G) + H + SS1 + W[j];
		D = C;
		C = SM3_ROTL(B, 9);
		B = A;
		A = TT1;
		H = G;
		G = SM3_ROTL(F, 19);
		F = E;
		E = SM3_P_0(TT2);
	}

	for (j = 16; j < 32; j++) {
		SS1 = SM3_ROTL(SM3_ROTL(A, 12) + E + SM3_ROTL(SM3_Tj_high, j), 7);
		SS2 = SS1 ^ SM3_ROTL(A, 12);
		TT1 = FF_j_high(A, B, C) + D + SS2 + W[j + 68];
		TT2 = GG_j_high(E, F, G) + H + SS1 + W[j];
		D = C;
		C = SM3_ROTL(B, 9);
		B = A;
		A = TT1;
		H = G;
		G = SM3_ROTL(F, 19);
		F = E;
		E = SM3_P_0(TT2);
	}

	for (j = 32; j < 64; j++) {
		SS1 = SM3_ROTL(SM3_ROTL(A, 12) + E + SM3_ROTL(SM3_Tj_high, (j - 32)), 7);
		SS2 = SS1 ^ SM3_ROTL(A, 12);
		TT1 = FF_j_high(A, B, C) + D + SS2 + W[j + 68];
		TT2 = GG_j_high(E, F, G) + H + SS1 + W[j];
		D = C;
		C = SM3_ROTL(B, 9);
		B = A;
		A = TT1;
		H = G;
		G = SM3_ROTL(F, 19);
		F = E;
		E = SM3_P_0(TT2);
	}

	ctx->sm3_state[0] ^= A;
	ctx->sm3_state[1] ^= B;
	ctx->sm3_state[2] ^= C;
	ctx->sm3_state[3] ^= D;
	ctx->sm3_state[4] ^= E;
	ctx->sm3_state[5] ^= F;
	ctx->sm3_state[6] ^= G;
	ctx->sm3_state[7] ^= H;

	ret = 0;

	return ret;
}

/* Init hash function. Initialize state to SM3 defined IV. */
int sm3_init(sm3_context *ctx)
{
	int ret;

	MUST_HAVE(ctx != NULL, ret, err);

	ctx->sm3_total = 0;
	ctx->sm3_state[0] = 0x7380166F;
	ctx->sm3_state[1] = 0x4914B2B9;
	ctx->sm3_state[2] = 0x172442D7;
	ctx->sm3_state[3] = 0xDA8A0600;
	ctx->sm3_state[4] = 0xA96F30BC;
	ctx->sm3_state[5] = 0x163138AA;
	ctx->sm3_state[6] = 0xE38DEE4D;
	ctx->sm3_state[7] = 0xB0FB0E4E;

	/* Tell that we are initialized */
	ctx->magic = SM3_HASH_MAGIC;

	ret = 0;

err:
	return ret;
}

/* Update hash function */
int sm3_update(sm3_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);
	SM3_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->sm3_total & 0x3F);
	fill = (u16)(SM3_BLOCK_SIZE - left);

	ctx->sm3_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->sm3_buffer + left, data_ptr, fill); EG(ret, err);
		ret = sm3_process(ctx, ctx->sm3_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= SM3_BLOCK_SIZE) {
		ret = sm3_process(ctx, data_ptr); EG(ret, err);
		data_ptr += SM3_BLOCK_SIZE;
		remain_ilen -= SM3_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->sm3_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize */
int sm3_final(sm3_context *ctx, u8 output[SM3_DIGEST_SIZE])
{
	unsigned int block_present = 0;
	u8 last_padded_block[2 * SM3_BLOCK_SIZE];
	int ret;

	MUST_HAVE((output != NULL), ret, err);
	SM3_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Fill in our last block with zeroes */
	ret = local_memset(last_padded_block, 0, sizeof(last_padded_block)); EG(ret, err);

	/* This is our final step, so we proceed with the padding */
	block_present = (ctx->sm3_total % SM3_BLOCK_SIZE);
	if (block_present != 0) {
		/* Copy what's left in our temporary context buffer */
		ret = local_memcpy(last_padded_block, ctx->sm3_buffer,
			     block_present); EG(ret, err);
	}

	/* Put the 0x80 byte, beginning of padding  */
	last_padded_block[block_present] = 0x80;

	/* Handle possible additional block */
	if (block_present > (SM3_BLOCK_SIZE - 1 - sizeof(u64))) {
		/* We need an additional block */
		PUT_UINT64_BE(8 * ctx->sm3_total, last_padded_block,
			      (2 * SM3_BLOCK_SIZE) - sizeof(u64));
		ret = sm3_process(ctx, last_padded_block); EG(ret, err);
		ret = sm3_process(ctx, last_padded_block + SM3_BLOCK_SIZE); EG(ret, err);
	} else {
		/* We do not need an additional block */
		PUT_UINT64_BE(8 * ctx->sm3_total, last_padded_block,
			      SM3_BLOCK_SIZE - sizeof(u64));
		ret = sm3_process(ctx, last_padded_block); EG(ret, err);
	}

	/* Output the hash result */
	PUT_UINT32_BE(ctx->sm3_state[0], output, 0);
	PUT_UINT32_BE(ctx->sm3_state[1], output, 4);
	PUT_UINT32_BE(ctx->sm3_state[2], output, 8);
	PUT_UINT32_BE(ctx->sm3_state[3], output, 12);
	PUT_UINT32_BE(ctx->sm3_state[4], output, 16);
	PUT_UINT32_BE(ctx->sm3_state[5], output, 20);
	PUT_UINT32_BE(ctx->sm3_state[6], output, 24);
	PUT_UINT32_BE(ctx->sm3_state[7], output, 28);

	/* Tell that we are uninitialized */
	ctx->magic = WORD(0);

	ret = 0;

err:
	return ret;
}

int sm3_scattered(const u8 **inputs, const u32 *ilens,
		  u8 output[SM3_DIGEST_SIZE])
{
	sm3_context ctx;
	int pos = 0, ret;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = sm3_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = sm3_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = sm3_final(&ctx, output);

err:
	return ret;
}

int sm3(const u8 *input, u32 ilen, u8 output[SM3_DIGEST_SIZE])
{
	sm3_context ctx;
	int ret;

	ret = sm3_init(&ctx); EG(ret, err);
	ret = sm3_update(&ctx, input, ilen); EG(ret, err);
	ret = sm3_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SM3 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SM3 */
