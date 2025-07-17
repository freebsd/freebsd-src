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
#ifdef WITH_HASH_RIPEMD160

#include <libecc/hash/ripemd160.h>

/****************************************************/
/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_LE
#define GET_UINT32_LE(n, b, i)                          \
do {                                                    \
        (n) =     ( ((u32) (b)[(i) + 3]) << 24 )        \
                | ( ((u32) (b)[(i) + 2]) << 16 )        \
                | ( ((u32) (b)[(i) + 1]) <<  8 )        \
                | ( ((u32) (b)[(i)    ])       );       \
} while( 0 )
#endif

#ifndef PUT_UINT32_LE
#define PUT_UINT32_LE(n, b, i)                  \
do {                                            \
        (b)[(i) + 3] = (u8) ( (n) >> 24 );      \
        (b)[(i) + 2] = (u8) ( (n) >> 16 );      \
        (b)[(i) + 1] = (u8) ( (n) >>  8 );      \
        (b)[(i)    ] = (u8) ( (n)       );      \
} while( 0 )
#endif

/*
 * 64-bit integer manipulation macros (big endian)
 */
#ifndef PUT_UINT64_LE
#define PUT_UINT64_LE(n,b,i)            \
do {                                    \
    (b)[(i) + 7] = (u8) ( (n) >> 56 );  \
    (b)[(i) + 6] = (u8) ( (n) >> 48 );  \
    (b)[(i) + 5] = (u8) ( (n) >> 40 );  \
    (b)[(i) + 4] = (u8) ( (n) >> 32 );  \
    (b)[(i) + 3] = (u8) ( (n) >> 24 );  \
    (b)[(i) + 2] = (u8) ( (n) >> 16 );  \
    (b)[(i) + 1] = (u8) ( (n) >>  8 );  \
    (b)[(i)    ] = (u8) ( (n)       );  \
} while( 0 )
#endif /* PUT_UINT64_LE */

/* Elements related to RIPEMD160 */
#define ROTL_RIPEMD160(x, n)    ((((u32)(x)) << (n)) | (((u32)(x)) >> (32-(n))))

#define F1_RIPEMD160(x, y, z)   ((x) ^ (y) ^ (z))
#define F2_RIPEMD160(x, y, z)   (((x) & (y)) | ((~(x)) & (z)))
#define F3_RIPEMD160(x, y, z)   (((x) | (~(y))) ^ (z))
#define F4_RIPEMD160(x, y, z)   (((x) & (z)) | ((y) & (~(z))))
#define F5_RIPEMD160(x, y, z)   ((x) ^ ((y) | (~(z))))

/* Left constants */
static const u32 KL_RIPEMD160[5] = {
	0x00000000, 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xa953fd4e
};
/* Right constants */
static const u32 KR_RIPEMD160[5] = {
	0x50a28be6, 0x5c4dd124, 0x6d703ef3, 0x7a6d76e9, 0x00000000
};

/* Left line */
static const u8 RL_RIPEMD160[5][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5, 2, 14, 11, 8 },
	{ 3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12 },
	{ 1, 9, 11, 10, 0, 8, 12, 4, 13, 3, 7, 15, 14, 5, 6, 2 },
	{ 4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13 }
};
static const u8 SL_RIPEMD160[5][16] = {
	{ 11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8 },
	{ 7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15, 9, 11, 7, 13, 12 },
	{ 11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5 },
	{ 11, 12, 14, 15, 14, 15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12 },
	{ 9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6 }
};

/* Right line */
static const u8 RR_RIPEMD160[5][16] = {
	{ 5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12 },
	{ 6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12, 4, 9, 1, 2 },
	{ 15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13 },
	{ 8, 6, 4, 1, 3, 11, 15, 0, 5, 12, 2, 13, 9, 7, 10, 14 },
	{ 12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11 }
};
static const u8 SR_RIPEMD160[5][16] = {
	{ 8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6 },
	{ 9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12, 7, 6, 15, 13, 11 },
	{ 9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5 },
	{ 15, 5, 8, 11, 14, 14, 6, 14, 6, 9, 12, 9, 12, 5, 15, 8 },
	{ 8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11 }
};

#define RIPEMD160_CORE(a, b, c, d, e, round, idx, w, F, S, R, K) do { 				\
	u32 t = ROTL_RIPEMD160(a + F(b, c, d) + w[R[round][idx]] + K[round], S[round][idx]) + e;\
	a = e; e = d; d = ROTL_RIPEMD160(c, 10); c = b; b = t; 					\
} while(0)

/* RIPEMD160 core processing */
ATTRIBUTE_WARN_UNUSED_RET static int ripemd160_process(ripemd160_context *ctx,
			   const u8 data[RIPEMD160_BLOCK_SIZE])
{
	/* Left line */
	u32 al, bl, cl, dl, el;
	/* Right line */
	u32 ar, br, cr, dr, er;
	/* Temporary data */
	u32 tt;
	/* Data */
	u32 W[16];
	unsigned int i;
	int ret;

	MUST_HAVE((data != NULL), ret, err);
	RIPEMD160_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Init our inner variables */
	al = ar = ctx->ripemd160_state[0];
	bl = br = ctx->ripemd160_state[1];
	cl = cr = ctx->ripemd160_state[2];
	dl = dr = ctx->ripemd160_state[3];
	el = er = ctx->ripemd160_state[4];

	/* Load data */
	for (i = 0; i < 16; i++) {
		GET_UINT32_LE(W[i], data, (4 * i));
	}

	/* Round 1 */
	for(i = 0; i < 16; i++){
		RIPEMD160_CORE(al, bl, cl, dl, el, 0, i, W, F1_RIPEMD160, SL_RIPEMD160, RL_RIPEMD160, KL_RIPEMD160);
		RIPEMD160_CORE(ar, br, cr, dr, er, 0, i, W, F5_RIPEMD160, SR_RIPEMD160, RR_RIPEMD160, KR_RIPEMD160);
	}
	/* Round 2 */
	for(i = 0; i < 16; i++){
		RIPEMD160_CORE(al, bl, cl, dl, el, 1, i, W, F2_RIPEMD160, SL_RIPEMD160, RL_RIPEMD160, KL_RIPEMD160);
		RIPEMD160_CORE(ar, br, cr, dr, er, 1, i, W, F4_RIPEMD160, SR_RIPEMD160, RR_RIPEMD160, KR_RIPEMD160);
	}
	/* Round 3 */
	for(i = 0; i < 16; i++){
		RIPEMD160_CORE(al, bl, cl, dl, el, 2, i, W, F3_RIPEMD160, SL_RIPEMD160, RL_RIPEMD160, KL_RIPEMD160);
		RIPEMD160_CORE(ar, br, cr, dr, er, 2, i, W, F3_RIPEMD160, SR_RIPEMD160, RR_RIPEMD160, KR_RIPEMD160);
	}
	/* Round 4 */
	for(i = 0; i < 16; i++){
		RIPEMD160_CORE(al, bl, cl, dl, el, 3, i, W, F4_RIPEMD160, SL_RIPEMD160, RL_RIPEMD160, KL_RIPEMD160);
		RIPEMD160_CORE(ar, br, cr, dr, er, 3, i, W, F2_RIPEMD160, SR_RIPEMD160, RR_RIPEMD160, KR_RIPEMD160);
	}
	/* Round 5 */
	for(i = 0; i < 16; i++){
		RIPEMD160_CORE(al, bl, cl, dl, el, 4, i, W, F5_RIPEMD160, SL_RIPEMD160, RL_RIPEMD160, KL_RIPEMD160);
		RIPEMD160_CORE(ar, br, cr, dr, er, 4, i, W, F1_RIPEMD160, SR_RIPEMD160, RR_RIPEMD160, KR_RIPEMD160);
	}

	/* Mix the lines and update state */
	tt = (ctx->ripemd160_state[1] + cl + dr);
	ctx->ripemd160_state[1] = (ctx->ripemd160_state[2] + dl + er);
	ctx->ripemd160_state[2] = (ctx->ripemd160_state[3] + el + ar);
	ctx->ripemd160_state[3] = (ctx->ripemd160_state[4] + al + br);
	ctx->ripemd160_state[4] = (ctx->ripemd160_state[0] + bl + cr);
	ctx->ripemd160_state[0] = tt;

	ret = 0;

err:
	return ret;
}

/* Init hash function */
int ripemd160_init(ripemd160_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ctx->ripemd160_total = 0;
	ctx->ripemd160_state[0] = 0x67452301;
	ctx->ripemd160_state[1] = 0xefcdab89;
	ctx->ripemd160_state[2] = 0x98badcfe;
	ctx->ripemd160_state[3] = 0x10325476;
	ctx->ripemd160_state[4] = 0xc3d2e1f0;

	/* Tell that we are initialized */
	ctx->magic = RIPEMD160_HASH_MAGIC;

	ret = 0;

err:
	return ret;
}

/* Update hash function */
int ripemd160_update(ripemd160_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);
	RIPEMD160_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->ripemd160_total & 0x3F);
	fill = (u16)(RIPEMD160_BLOCK_SIZE - left);

	ctx->ripemd160_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->ripemd160_buffer + left, data_ptr, fill); EG(ret, err);
		ret = ripemd160_process(ctx, ctx->ripemd160_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= RIPEMD160_BLOCK_SIZE) {
		ret = ripemd160_process(ctx, data_ptr); EG(ret, err);
		data_ptr += RIPEMD160_BLOCK_SIZE;
		remain_ilen -= RIPEMD160_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->ripemd160_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize */
int ripemd160_final(ripemd160_context *ctx, u8 output[RIPEMD160_DIGEST_SIZE])
{
	unsigned int block_present = 0;
	u8 last_padded_block[2 * RIPEMD160_BLOCK_SIZE];
	int ret;

	MUST_HAVE((output != NULL), ret, err);
	RIPEMD160_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Fill in our last block with zeroes */
	ret = local_memset(last_padded_block, 0, sizeof(last_padded_block)); EG(ret, err);

	/* This is our final step, so we proceed with the padding */
	block_present = (ctx->ripemd160_total % RIPEMD160_BLOCK_SIZE);
	if (block_present != 0) {
		/* Copy what's left in our temporary context buffer */
		ret = local_memcpy(last_padded_block, ctx->ripemd160_buffer,
			     block_present); EG(ret, err);
	}

	/* Put the 0x80 byte, beginning of padding  */
	last_padded_block[block_present] = 0x80;

	/* Handle possible additional block */
	if (block_present > (RIPEMD160_BLOCK_SIZE - 1 - sizeof(u64))) {
		/* We need an additional block */
		PUT_UINT64_LE(8 * ctx->ripemd160_total, last_padded_block,
			      (2 * RIPEMD160_BLOCK_SIZE) - sizeof(u64));
		ret = ripemd160_process(ctx, last_padded_block); EG(ret, err);
		ret = ripemd160_process(ctx, last_padded_block + RIPEMD160_BLOCK_SIZE); EG(ret, err);
	} else {
		/* We do not need an additional block */
		PUT_UINT64_LE(8 * ctx->ripemd160_total, last_padded_block,
			      RIPEMD160_BLOCK_SIZE - sizeof(u64));
		ret = ripemd160_process(ctx, last_padded_block); EG(ret, err);
	}

	/* Output the hash result */
	PUT_UINT32_LE(ctx->ripemd160_state[0], output, 0);
	PUT_UINT32_LE(ctx->ripemd160_state[1], output, 4);
	PUT_UINT32_LE(ctx->ripemd160_state[2], output, 8);
	PUT_UINT32_LE(ctx->ripemd160_state[3], output, 12);
	PUT_UINT32_LE(ctx->ripemd160_state[4], output, 16);

	/* Tell that we are uninitialized */
	ctx->magic = WORD(0);

	ret = 0;

err:
	return ret;
}

int ripemd160_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[RIPEMD160_DIGEST_SIZE])
{
	ripemd160_context ctx;
	int ret, pos = 0;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = ripemd160_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = ripemd160_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = ripemd160_final(&ctx, output);

err:
	return ret;
}

int ripemd160(const u8 *input, u32 ilen, u8 output[RIPEMD160_DIGEST_SIZE])
{
	ripemd160_context ctx;
	int ret;

	ret = ripemd160_init(&ctx); EG(ret, err);
	ret = ripemd160_update(&ctx, input, ilen); EG(ret, err);
	ret = ripemd160_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_RIPEMD160 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_RIPEMD160 */
