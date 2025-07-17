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
#ifdef WITH_HASH_SHA384

#include <libecc/hash/sha384.h>

/* SHA-2 core processing. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET static int sha384_process(sha384_context *ctx,
			   const u8 data[SHA384_BLOCK_SIZE])
{
	u64 a, b, c, d, e, f, g, h;
	u64 W[80];
	unsigned int i;
	int ret;

	MUST_HAVE((data != NULL), ret, err);
	SHA384_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Init our inner variables */
	a = ctx->sha384_state[0];
	b = ctx->sha384_state[1];
	c = ctx->sha384_state[2];
	d = ctx->sha384_state[3];
	e = ctx->sha384_state[4];
	f = ctx->sha384_state[5];
	g = ctx->sha384_state[6];
	h = ctx->sha384_state[7];

	for (i = 0; i < 16; i++) {
		GET_UINT64_BE(W[i], data, 8 * i);
		SHA2CORE_SHA512(a, b, c, d, e, f, g, h, W[i], K_SHA512[i]);
	}

	for (i = 16; i < 80; i++) {
		SHA2CORE_SHA512(a, b, c, d, e, f, g, h, UPDATEW_SHA512(W, i),
				K_SHA512[i]);
	}

	/* Update state */
	ctx->sha384_state[0] += a;
	ctx->sha384_state[1] += b;
	ctx->sha384_state[2] += c;
	ctx->sha384_state[3] += d;
	ctx->sha384_state[4] += e;
	ctx->sha384_state[5] += f;
	ctx->sha384_state[6] += g;
	ctx->sha384_state[7] += h;

	ret = 0;

err:
	return ret;
}

/* Init hash function. Returns 0 on success, -1 on error. */
int sha384_init(sha384_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ctx->sha384_total[0] = ctx->sha384_total[1] = 0;
	ctx->sha384_state[0] = (u64)(0xCBBB9D5DC1059ED8);
	ctx->sha384_state[1] = (u64)(0x629A292A367CD507);
	ctx->sha384_state[2] = (u64)(0x9159015A3070DD17);
	ctx->sha384_state[3] = (u64)(0x152FECD8F70E5939);
	ctx->sha384_state[4] = (u64)(0x67332667FFC00B31);
	ctx->sha384_state[5] = (u64)(0x8EB44A8768581511);
	ctx->sha384_state[6] = (u64)(0xDB0C2E0D64F98FA7);
	ctx->sha384_state[7] = (u64)(0x47B5481DBEFA4FA4);

	/* Tell that we are initialized */
	ctx->magic = SHA384_HASH_MAGIC;
	ret = 0;

err:
	return ret;
}

/* Update hash function. Returns 0 on success, -1 on error. */
int sha384_update(sha384_context *ctx, const u8 *input, u32 ilen)
{
	u32 left;
	u32 fill;
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	int ret;

	MUST_HAVE((input != NULL), ret, err);
	SHA384_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->sha384_total[0] & 0x7F);
	fill = (SHA384_BLOCK_SIZE - left);

	ADD_UINT128_UINT64(ctx->sha384_total[0], ctx->sha384_total[1], ilen);

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->sha384_buffer + left, data_ptr, fill); EG(ret, err);
		ret = sha384_process(ctx, ctx->sha384_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= SHA384_BLOCK_SIZE) {
		ret = sha384_process(ctx, data_ptr); EG(ret, err);
		data_ptr += SHA384_BLOCK_SIZE;
		remain_ilen -= SHA384_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->sha384_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/*
 * Finalize hash function. Returns 0 on success, -1 on error. In all
 * cases (success or error), hash context is no more usable after the
 * call.
 */
int sha384_final(sha384_context *ctx, u8 output[SHA384_DIGEST_SIZE])
{
	unsigned int block_present = 0;
	u8 last_padded_block[2 * SHA384_BLOCK_SIZE];
	int ret;

	MUST_HAVE((output != NULL), ret, err);
	SHA384_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Fill in our last block with zeroes */
	ret = local_memset(last_padded_block, 0, sizeof(last_padded_block)); EG(ret, err);

	/* This is our final step, so we proceed with the padding */
	block_present = (ctx->sha384_total[0] % SHA384_BLOCK_SIZE);
	if (block_present != 0) {
		/* Copy what's left in our temporary context buffer */
		ret = local_memcpy(last_padded_block, ctx->sha384_buffer,
			     block_present); EG(ret, err);
	}

	/* Put the 0x80 byte, beginning of padding  */
	last_padded_block[block_present] = 0x80;

	/* Handle possible additional block */
	if (block_present > (SHA384_BLOCK_SIZE - 1 - (2 * sizeof(u64)))) {
		/* We need an additional block */
		PUT_MUL8_UINT128_BE(ctx->sha384_total[0], ctx->sha384_total[1],
				    last_padded_block,
				    2 * (SHA384_BLOCK_SIZE - sizeof(u64)));
		ret = sha384_process(ctx, last_padded_block); EG(ret, err);
		ret = sha384_process(ctx, last_padded_block + SHA384_BLOCK_SIZE); EG(ret, err);
	} else {
		/* We do not need an additional block */
		PUT_MUL8_UINT128_BE(ctx->sha384_total[0], ctx->sha384_total[1],
				    last_padded_block,
				    SHA384_BLOCK_SIZE - (2 * sizeof(u64)));
		ret = sha384_process(ctx, last_padded_block); EG(ret, err);
	}

	/* Output the hash result */
	PUT_UINT64_BE(ctx->sha384_state[0], output, 0);
	PUT_UINT64_BE(ctx->sha384_state[1], output, 8);
	PUT_UINT64_BE(ctx->sha384_state[2], output, 16);
	PUT_UINT64_BE(ctx->sha384_state[3], output, 24);
	PUT_UINT64_BE(ctx->sha384_state[4], output, 32);
	PUT_UINT64_BE(ctx->sha384_state[5], output, 40);

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
int sha384_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[SHA384_DIGEST_SIZE])
{
	sha384_context ctx;
	int pos = 0;
	int ret;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = sha384_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		const u8 *buf = inputs[pos];
		u32 buflen = ilens[pos];

		ret = sha384_update(&ctx, buf, buflen); EG(ret, err);
		pos += 1;
	}

	ret = sha384_final(&ctx, output);

err:
	return ret;
}

/* init/update/finalize on a single buffer 'input' of length 'ilen'. */
int sha384(const u8 *input, u32 ilen, u8 output[SHA384_DIGEST_SIZE])
{
	sha384_context ctx;
	int ret;

	ret = sha384_init(&ctx); EG(ret, err);
	ret = sha384_update(&ctx, input, ilen); EG(ret, err);
	ret = sha384_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SHA384 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHA384 */
