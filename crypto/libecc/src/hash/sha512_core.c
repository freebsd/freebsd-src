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
#include <libecc/lib_ecc_config.h>
#if defined(WITH_HASH_SHA512) || defined(WITH_HASH_SHA512_224) || defined(WITH_HASH_SHA512_256)
#include <libecc/hash/sha512_core.h>

/* SHA-2 core processing. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET static int sha512_core_process(sha512_core_context *ctx,
			   const u8 data[SHA512_CORE_BLOCK_SIZE])
{
	u64 a, b, c, d, e, f, g, h;
	u64 W[80];
	unsigned int i;
	int ret;

	MUST_HAVE(((ctx != NULL) && (data != NULL)), ret, err);

	/* Init our inner variables */
	a = ctx->sha512_state[0];
	b = ctx->sha512_state[1];
	c = ctx->sha512_state[2];
	d = ctx->sha512_state[3];
	e = ctx->sha512_state[4];
	f = ctx->sha512_state[5];
	g = ctx->sha512_state[6];
	h = ctx->sha512_state[7];

	for (i = 0; i < 16; i++) {
		GET_UINT64_BE(W[i], data, 8 * i);
		SHA2CORE_SHA512(a, b, c, d, e, f, g, h, W[i], K_SHA512[i]);
	}

	for (i = 16; i < 80; i++) {
		SHA2CORE_SHA512(a, b, c, d, e, f, g, h, UPDATEW_SHA512(W, i),
				K_SHA512[i]);
	}

	/* Update state */
	ctx->sha512_state[0] += a;
	ctx->sha512_state[1] += b;
	ctx->sha512_state[2] += c;
	ctx->sha512_state[3] += d;
	ctx->sha512_state[4] += e;
	ctx->sha512_state[5] += f;
	ctx->sha512_state[6] += g;
	ctx->sha512_state[7] += h;

	ret = 0;

err:
	return ret;
}

/* Core update hash function. Returns 0 on success, -1 on error. */
int sha512_core_update(sha512_core_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE(((ctx != NULL) && ((input != NULL) || (ilen == 0))), ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = ctx->sha512_total[0] & 0x7F;
	fill = (u16)(SHA512_CORE_BLOCK_SIZE - left);

	ADD_UINT128_UINT64(ctx->sha512_total[0], ctx->sha512_total[1], ilen);

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->sha512_buffer + left, data_ptr, fill); EG(ret, err);
		ret = sha512_core_process(ctx, ctx->sha512_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= SHA512_CORE_BLOCK_SIZE) {
		ret = sha512_core_process(ctx, data_ptr); EG(ret, err);
		data_ptr += SHA512_CORE_BLOCK_SIZE;
		remain_ilen -= SHA512_CORE_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->sha512_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Core finalize. Returns 0 on success, -1 on error. */
int sha512_core_final(sha512_core_context *ctx, u8 *output, u32 output_size)
{
	unsigned int block_present = 0;
	u8 last_padded_block[2 * SHA512_CORE_BLOCK_SIZE];
	int ret;

	MUST_HAVE(((ctx != NULL) && (output != NULL)), ret, err);

	/* Fill in our last block with zeroes */
	ret = local_memset(last_padded_block, 0, sizeof(last_padded_block)); EG(ret, err);

	/* This is our final step, so we proceed with the padding */
	block_present = ctx->sha512_total[0] % SHA512_CORE_BLOCK_SIZE;
	if (block_present != 0) {
		/* Copy what's left in our temporary context buffer */
		ret = local_memcpy(last_padded_block, ctx->sha512_buffer,
			     block_present); EG(ret, err);
	}

	/* Put the 0x80 byte, beginning of padding  */
	last_padded_block[block_present] = 0x80;

	/* Handle possible additional block */
	if (block_present > (SHA512_CORE_BLOCK_SIZE - 1 - (2 * sizeof(u64)))) {
		/* We need an additional block */
		PUT_MUL8_UINT128_BE(ctx->sha512_total[0], ctx->sha512_total[1],
				    last_padded_block,
				    2 * (SHA512_CORE_BLOCK_SIZE - sizeof(u64)));
		ret = sha512_core_process(ctx, last_padded_block); EG(ret, err);
		ret = sha512_core_process(ctx, last_padded_block + SHA512_CORE_BLOCK_SIZE); EG(ret, err);
	} else {
		/* We do not need an additional block */
		PUT_MUL8_UINT128_BE(ctx->sha512_total[0], ctx->sha512_total[1],
				    last_padded_block,
				    SHA512_CORE_BLOCK_SIZE - (2 * sizeof(u64)));
		ret = sha512_core_process(ctx, last_padded_block); EG(ret, err);
	}

	/* Output the hash result truncated to the output size */
	if(output_size >= SHA512_CORE_DIGEST_SIZE){
		PUT_UINT64_BE(ctx->sha512_state[0], output, 0);
		PUT_UINT64_BE(ctx->sha512_state[1], output, 8);
		PUT_UINT64_BE(ctx->sha512_state[2], output, 16);
		PUT_UINT64_BE(ctx->sha512_state[3], output, 24);
		PUT_UINT64_BE(ctx->sha512_state[4], output, 32);
		PUT_UINT64_BE(ctx->sha512_state[5], output, 40);
		PUT_UINT64_BE(ctx->sha512_state[6], output, 48);
		PUT_UINT64_BE(ctx->sha512_state[7], output, 56);
	} else {
		u8 tmp_output[SHA512_CORE_DIGEST_SIZE] = { 0 };
		PUT_UINT64_BE(ctx->sha512_state[0], tmp_output, 0);
		PUT_UINT64_BE(ctx->sha512_state[1], tmp_output, 8);
		PUT_UINT64_BE(ctx->sha512_state[2], tmp_output, 16);
		PUT_UINT64_BE(ctx->sha512_state[3], tmp_output, 24);
		PUT_UINT64_BE(ctx->sha512_state[4], tmp_output, 32);
		PUT_UINT64_BE(ctx->sha512_state[5], tmp_output, 40);
		PUT_UINT64_BE(ctx->sha512_state[6], tmp_output, 48);
		PUT_UINT64_BE(ctx->sha512_state[7], tmp_output, 56);
		ret = local_memcpy(output, tmp_output, output_size); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

#else /* defined(WITH_HASH_SHA512) || defined(WITH_HASH_SHA512_224) || defined(WITH_HASH_SHA512_256) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHA512 */
