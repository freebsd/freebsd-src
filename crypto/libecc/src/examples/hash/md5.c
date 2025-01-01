/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include "md5.h"

/* All the inner MD-5 operations */
static const u32 K_MD5[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const u8 R_MD5[64] = {
	7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
	5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
	4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
	6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

#define F_MD5(x, y, z)   (((x) & (y)) | ((~(x)) & (z)))
#define G_MD5(x, y, z)   (((x) & (z)) | ((y) & (~(z))))
#define H_MD5(x, y, z)   ((x) ^ (y) ^ (z))
#define I_MD5(x, y, z)   ((y) ^ ((x) | ((~z))))

/* SHA-2 core processing. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET static inline int md5_process(md5_context *ctx,
			   const u8 data[MD5_BLOCK_SIZE])
{
	u32 A, B, C, D, tmp;
	u32 W[16];
	int ret;
	unsigned int i;

	MUST_HAVE((data != NULL), ret, err);
	MD5_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Init our inner variables */
	A = ctx->md5_state[0];
	B = ctx->md5_state[1];
	C = ctx->md5_state[2];
	D = ctx->md5_state[3];

	/* Load data */
	for (i = 0; i < 16; i++) {
		GET_UINT32_LE(W[i], data, (4 * i));
	}
	for (i = 0; i < 64; i++) {
		u32 f, g;
		if(i <= 15){
			f = F_MD5(B, C, D);
			g = i;
		}
		else if((i >= 16) && (i <= 31)){
			f = G_MD5(B, C, D);
			g = (((5 * i) + 1) % 16);
		}
		else if((i >= 32) && (i <= 47)){
			f = H_MD5(B, C, D);
			g = (((3 * i) + 5) % 16);
		}
		else{
			f = I_MD5(B, C, D);
			g = ((7 * i) % 16);
		}
		tmp = D;
		D = C;
		C = B;
		B += ROTL_MD5((A + f + K_MD5[i] + W[g]), R_MD5[i]);
		A = tmp;
	}

	/* Update state */
	ctx->md5_state[0] += A;
	ctx->md5_state[1] += B;
	ctx->md5_state[2] += C;
	ctx->md5_state[3] += D;

	ret = 0;

err:
	return ret;
}

/* Init hash function. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET int md5_init(md5_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

        /* Sanity check on size */
	MUST_HAVE((MD5_DIGEST_SIZE <= MAX_DIGEST_SIZE), ret, err);

	ctx->md5_total = 0;
	ctx->md5_state[0] = 0x67452301;
	ctx->md5_state[1] = 0xEFCDAB89;
	ctx->md5_state[2] = 0x98BADCFE;
	ctx->md5_state[3] = 0x10325476;

	/* Tell that we are initialized */
	ctx->magic = MD5_HASH_MAGIC;

	ret = 0;

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET int md5_update(md5_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);
	MD5_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->md5_total & 0x3F);
	fill = (u16)(MD5_BLOCK_SIZE - left);

	ctx->md5_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->md5_buffer + left, data_ptr, fill); EG(ret, err);
		ret = md5_process(ctx, ctx->md5_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= MD5_BLOCK_SIZE) {
		ret = md5_process(ctx, data_ptr); EG(ret, err);
		data_ptr += MD5_BLOCK_SIZE;
		remain_ilen -= MD5_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->md5_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize. Returns 0 on success, -1 on error.*/
ATTRIBUTE_WARN_UNUSED_RET int md5_final(md5_context *ctx, u8 output[MD5_DIGEST_SIZE])
{
	unsigned int block_present = 0;
	u8 last_padded_block[2 * MD5_BLOCK_SIZE];
	int ret;

	MUST_HAVE((output != NULL), ret, err);
	MD5_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Fill in our last block with zeroes */
	ret = local_memset(last_padded_block, 0, sizeof(last_padded_block)); EG(ret, err);

	/* This is our final step, so we proceed with the padding */
	block_present = ctx->md5_total % MD5_BLOCK_SIZE;
	if (block_present != 0) {
		/* Copy what's left in our temporary context buffer */
		ret = local_memcpy(last_padded_block, ctx->md5_buffer,
			     block_present); EG(ret, err);
	}

	/* Put the 0x80 byte, beginning of padding  */
	last_padded_block[block_present] = 0x80;

	/* Handle possible additional block */
	if (block_present > (MD5_BLOCK_SIZE - 1 - sizeof(u64))) {
		/* We need an additional block */
		PUT_UINT64_LE(8 * ctx->md5_total, last_padded_block,
			      (2 * MD5_BLOCK_SIZE) - sizeof(u64));
		ret = md5_process(ctx, last_padded_block); EG(ret, err);
		ret = md5_process(ctx, last_padded_block + MD5_BLOCK_SIZE); EG(ret, err);
	} else {
		/* We do not need an additional block */
		PUT_UINT64_LE(8 * ctx->md5_total, last_padded_block,
			      MD5_BLOCK_SIZE - sizeof(u64));
		ret = md5_process(ctx, last_padded_block); EG(ret, err);
	}

	/* Output the hash result */
	PUT_UINT32_LE(ctx->md5_state[0], output, 0);
	PUT_UINT32_LE(ctx->md5_state[1], output, 4);
	PUT_UINT32_LE(ctx->md5_state[2], output, 8);
	PUT_UINT32_LE(ctx->md5_state[3], output, 12);

	/* Tell that we are uninitialized */
	ctx->magic = WORD(0);

	ret = 0;

err:
	return ret;
}


/*
 * Scattered version performing init/update/finalize on a vector of buffers
 * 'inputs' with the length of each buffer passed via 'ilens'. The function
 * loops on pointers in 'inputs' until it finds a NULL pointer. The function
 * returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int md5_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[MD5_DIGEST_SIZE])
{
	md5_context ctx;
	int ret, pos = 0;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = md5_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = md5_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = md5_final(&ctx, output);

err:
	return ret;
}

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int md5(const u8 *input, u32 ilen, u8 output[MD5_DIGEST_SIZE])
{
	md5_context ctx;
	int ret;

	ret = md5_init(&ctx); EG(ret, err);
	ret = md5_update(&ctx, input, ilen); EG(ret, err);
	ret = md5_final(&ctx, output);

err:
	return ret;
}
