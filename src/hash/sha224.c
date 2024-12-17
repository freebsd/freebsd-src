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
#ifdef WITH_HASH_SHA224

#include <libecc/hash/sha224.h>

/* SHA-2 core processing. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET static int sha224_process(sha224_context *ctx,
			   const u8 data[SHA224_BLOCK_SIZE])
{
	u32 a, b, c, d, e, f, g, h;
	u32 W[64];
	unsigned int i;
	int ret;

	MUST_HAVE((data != NULL), ret, err);
	SHA224_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Init our inner variables */
	a = ctx->sha224_state[0];
	b = ctx->sha224_state[1];
	c = ctx->sha224_state[2];
	d = ctx->sha224_state[3];
	e = ctx->sha224_state[4];
	f = ctx->sha224_state[5];
	g = ctx->sha224_state[6];
	h = ctx->sha224_state[7];

	for (i = 0; i < 16; i++) {
		GET_UINT32_BE(W[i], data, 4 * i);
		SHA2CORE_SHA256(a, b, c, d, e, f, g, h, W[i], K_SHA256[i]);
	}

	for (i = 16; i < 64; i++) {
		SHA2CORE_SHA256(a, b, c, d, e, f, g, h, UPDATEW_SHA256(W, i),
				K_SHA256[i]);
	}

	/* Update state */
	ctx->sha224_state[0] += a;
	ctx->sha224_state[1] += b;
	ctx->sha224_state[2] += c;
	ctx->sha224_state[3] += d;
	ctx->sha224_state[4] += e;
	ctx->sha224_state[5] += f;
	ctx->sha224_state[6] += g;
	ctx->sha224_state[7] += h;

	ret = 0;

err:
	return ret;
}

/* Init hash function. Returns 0 on success, -1 on error. */
int sha224_init(sha224_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ctx->sha224_total = 0;
	ctx->sha224_state[0] = 0xC1059ED8;
	ctx->sha224_state[1] = 0x367CD507;
	ctx->sha224_state[2] = 0x3070DD17;
	ctx->sha224_state[3] = 0xF70E5939;
	ctx->sha224_state[4] = 0xFFC00B31;
	ctx->sha224_state[5] = 0x68581511;
	ctx->sha224_state[6] = 0x64F98FA7;
	ctx->sha224_state[7] = 0xBEFA4FA4;

	/* Tell that we are initialized */
	ctx->magic = SHA224_HASH_MAGIC;

	ret = 0;

err:
	return ret;
}

/* Update hash function. Returns 0 on success, -1 on error. */
int sha224_update(sha224_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);
	SHA224_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->sha224_total & 0x3F);
	fill = (u16)(SHA224_BLOCK_SIZE - left);

	ctx->sha224_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->sha224_buffer + left, data_ptr, fill); EG(ret, err);
		ret = sha224_process(ctx, ctx->sha224_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= SHA224_BLOCK_SIZE) {
		ret = sha224_process(ctx, data_ptr); EG(ret, err);
		data_ptr += SHA224_BLOCK_SIZE;
		remain_ilen -= SHA224_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->sha224_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize. Returns 0 on success, -1 on error.*/
int sha224_final(sha224_context *ctx, u8 output[SHA224_DIGEST_SIZE])
{
	unsigned int block_present = 0;
	u8 last_padded_block[2 * SHA224_BLOCK_SIZE];
	int ret;

	MUST_HAVE((output != NULL), ret, err);
	SHA224_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Fill in our last block with zeroes */
	ret = local_memset(last_padded_block, 0, sizeof(last_padded_block)); EG(ret, err);

	/* This is our final step, so we proceed with the padding */
	block_present = ctx->sha224_total % SHA224_BLOCK_SIZE;
	if (block_present != 0) {
		/* Copy what's left in our temporary context buffer */
		ret = local_memcpy(last_padded_block, ctx->sha224_buffer,
			     block_present); EG(ret, err);
	}

	/* Put the 0x80 byte, beginning of padding  */
	last_padded_block[block_present] = 0x80;

	/* Handle possible additional block */
	if (block_present > (SHA224_BLOCK_SIZE - 1 - sizeof(u64))) {
		/* We need an additional block */
		PUT_UINT64_BE(8 * ctx->sha224_total, last_padded_block,
			      (2 * SHA224_BLOCK_SIZE) - sizeof(u64));
		ret = sha224_process(ctx, last_padded_block); EG(ret, err);
		ret = sha224_process(ctx, last_padded_block + SHA224_BLOCK_SIZE); EG(ret, err);
	} else {
		/* We do not need an additional block */
		PUT_UINT64_BE(8 * ctx->sha224_total, last_padded_block,
			      SHA224_BLOCK_SIZE - sizeof(u64));
		ret = sha224_process(ctx, last_padded_block); EG(ret, err);
	}

	/* Output the hash result */
	PUT_UINT32_BE(ctx->sha224_state[0], output, 0);
	PUT_UINT32_BE(ctx->sha224_state[1], output, 4);
	PUT_UINT32_BE(ctx->sha224_state[2], output, 8);
	PUT_UINT32_BE(ctx->sha224_state[3], output, 12);
	PUT_UINT32_BE(ctx->sha224_state[4], output, 16);
	PUT_UINT32_BE(ctx->sha224_state[5], output, 20);
	PUT_UINT32_BE(ctx->sha224_state[6], output, 24);

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
int sha224_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[SHA224_DIGEST_SIZE])
{
	sha224_context ctx;
	int ret, pos = 0;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = sha224_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = sha224_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = sha224_final(&ctx, output);

err:
	return ret;
}

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
int sha224(const u8 *input, u32 ilen, u8 output[SHA224_DIGEST_SIZE])
{
	sha224_context ctx;
	int ret;

	ret = sha224_init(&ctx); EG(ret, err);
	ret = sha224_update(&ctx, input, ilen); EG(ret, err);
	ret = sha224_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SHA224 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHA224 */
