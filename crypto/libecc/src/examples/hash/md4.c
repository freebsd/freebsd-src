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
#include "md4.h"

/* All the inner MD-4 operations */
static const u32 C1_MD4[13] = {
	0, 4, 8, 12, 0, 1, 2, 3, 3, 7, 11, 19, 0
};
static const u32 C2_MD4[13] = {
	0, 1, 2, 3, 0, 4, 8, 12, 3, 5, 9, 13, 0x5a827999
};
static const u32 C3_MD4[13] = {
	0, 2, 1, 3, 0, 8, 4, 12, 3, 9, 11, 15, 0x6ed9eba1
};

#define F_MD4(x, y, z)   (((x) & (y)) | ((~(x)) & (z)))
#define G_MD4(x, y, z)   (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H_MD4(x, y, z)   ((x) ^ (y) ^ (z))

/* SHA-2 core processing. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET static inline int md4_process(md4_context *ctx,
			   const u8 data[MD4_BLOCK_SIZE])
{
	u32 A, B, C, D;
	u32 W[16];
	u32 idx;
	int ret;
	unsigned int i;

	MUST_HAVE((data != NULL), ret, err);
	MD4_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Init our inner variables */
	A = ctx->md4_state[0];
	B = ctx->md4_state[1];
	C = ctx->md4_state[2];
	D = ctx->md4_state[3];

	/* Load data */
	for (i = 0; i < 16; i++) {
		GET_UINT32_LE(W[i], data, (4 * i));
	}
	/* Proceed with the compression */
	for (i = 0; i < 4; i++) {
		idx = (C1_MD4[i] + C1_MD4[4]);
		A = ROTL_MD4((A + F_MD4(B, C, D) + W[idx] + C1_MD4[12]), C1_MD4[8]);
		idx = (C1_MD4[i] + C1_MD4[5]);
		D = ROTL_MD4((D + F_MD4(A, B, C) + W[idx] + C1_MD4[12]), C1_MD4[9]);
		idx = (C1_MD4[i] + C1_MD4[6]);
		C = ROTL_MD4((C + F_MD4(D, A, B) + W[idx] + C1_MD4[12]), C1_MD4[10]);
		idx = (C1_MD4[i] + C1_MD4[7]);
		B = ROTL_MD4((B + F_MD4(C, D, A) + W[idx] + C1_MD4[12]), C1_MD4[11]);
	}
	for (i = 0; i < 4; i++) {
		idx = (C2_MD4[i] + C2_MD4[4]);
		A = ROTL_MD4((A + G_MD4(B, C, D) + W[idx] + C2_MD4[12]), C2_MD4[8]);
		idx = (C2_MD4[i] + C2_MD4[5]);
		D = ROTL_MD4((D + G_MD4(A, B, C) + W[idx] + C2_MD4[12]), C2_MD4[9]);
		idx = (C2_MD4[i] + C2_MD4[6]);
		C = ROTL_MD4((C + G_MD4(D, A, B) + W[idx] + C2_MD4[12]), C2_MD4[10]);
		idx = (C2_MD4[i] + C2_MD4[7]);
		B = ROTL_MD4((B + G_MD4(C, D, A) + W[idx] + C2_MD4[12]), C2_MD4[11]);
	}
	for (i = 0; i < 4; i++) {
		idx = (C3_MD4[i] + C3_MD4[4]);
		A = ROTL_MD4((A + H_MD4(B, C, D) + W[idx] + C3_MD4[12]), C3_MD4[8]);
		idx = (C3_MD4[i] + C3_MD4[5]);
		D = ROTL_MD4((D + H_MD4(A, B, C) + W[idx] + C3_MD4[12]), C3_MD4[9]);
		idx = (C3_MD4[i] + C3_MD4[6]);
		C = ROTL_MD4((C + H_MD4(D, A, B) + W[idx] + C3_MD4[12]), C3_MD4[10]);
		idx = (C3_MD4[i] + C3_MD4[7]);
		B = ROTL_MD4((B + H_MD4(C, D, A) + W[idx] + C3_MD4[12]), C3_MD4[11]);
	}

	/* Update state */
	ctx->md4_state[0] += A;
	ctx->md4_state[1] += B;
	ctx->md4_state[2] += C;
	ctx->md4_state[3] += D;

	ret = 0;

err:
	return ret;
}

/* Init hash function. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET  int md4_init(md4_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	/* Sanity check on size */
	MUST_HAVE((MD4_DIGEST_SIZE <= MAX_DIGEST_SIZE), ret, err);

	ctx->md4_total = 0;
	ctx->md4_state[0] = 0x67452301;
	ctx->md4_state[1] = 0xEFCDAB89;
	ctx->md4_state[2] = 0x98BADCFE;
	ctx->md4_state[3] = 0x10325476;

	/* Tell that we are initialized */
	ctx->magic = MD4_HASH_MAGIC;

	ret = 0;

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET int md4_update(md4_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);
	MD4_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->md4_total & 0x3F);
	fill = (u16)(MD4_BLOCK_SIZE - left);

	ctx->md4_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->md4_buffer + left, data_ptr, fill); EG(ret, err);
		ret = md4_process(ctx, ctx->md4_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= MD4_BLOCK_SIZE) {
		ret = md4_process(ctx, data_ptr); EG(ret, err);
		data_ptr += MD4_BLOCK_SIZE;
		remain_ilen -= MD4_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->md4_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize. Returns 0 on success, -1 on error.*/
ATTRIBUTE_WARN_UNUSED_RET int md4_final(md4_context *ctx, u8 output[MD4_DIGEST_SIZE])
{
	unsigned int block_present = 0;
	u8 last_padded_block[2 * MD4_BLOCK_SIZE];
	int ret;

	MUST_HAVE((output != NULL), ret, err);
	MD4_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Fill in our last block with zeroes */
	ret = local_memset(last_padded_block, 0, sizeof(last_padded_block)); EG(ret, err);

	/* This is our final step, so we proceed with the padding */
	block_present = ctx->md4_total % MD4_BLOCK_SIZE;
	if (block_present != 0) {
		/* Copy what's left in our temporary context buffer */
		ret = local_memcpy(last_padded_block, ctx->md4_buffer,
			     block_present); EG(ret, err);
	}

	/* Put the 0x80 byte, beginning of padding  */
	last_padded_block[block_present] = 0x80;

	/* Handle possible additional block */
	if (block_present > (MD4_BLOCK_SIZE - 1 - sizeof(u64))) {
		/* We need an additional block */
		PUT_UINT64_LE(8 * ctx->md4_total, last_padded_block,
			      (2 * MD4_BLOCK_SIZE) - sizeof(u64));
		ret = md4_process(ctx, last_padded_block); EG(ret, err);
		ret = md4_process(ctx, last_padded_block + MD4_BLOCK_SIZE); EG(ret, err);
	} else {
		/* We do not need an additional block */
		PUT_UINT64_LE(8 * ctx->md4_total, last_padded_block,
			      MD4_BLOCK_SIZE - sizeof(u64));
		ret = md4_process(ctx, last_padded_block); EG(ret, err);
	}

	/* Output the hash result */
	PUT_UINT32_LE(ctx->md4_state[0], output, 0);
	PUT_UINT32_LE(ctx->md4_state[1], output, 4);
	PUT_UINT32_LE(ctx->md4_state[2], output, 8);
	PUT_UINT32_LE(ctx->md4_state[3], output, 12);

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
ATTRIBUTE_WARN_UNUSED_RET int md4_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[MD4_DIGEST_SIZE])
{
	md4_context ctx;
	int ret, pos = 0;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = md4_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = md4_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = md4_final(&ctx, output);

err:
	return ret;
}

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int md4(const u8 *input, u32 ilen, u8 output[MD4_DIGEST_SIZE])
{
	md4_context ctx;
	int ret;

	ret = md4_init(&ctx); EG(ret, err);
	ret = md4_update(&ctx, input, ilen); EG(ret, err);
	ret = md4_final(&ctx, output);

err:
	return ret;
}
