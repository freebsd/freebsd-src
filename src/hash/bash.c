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
#include <libecc/lib_ecc_config.h>

#if defined(WITH_HASH_BASH224) || defined(WITH_HASH_BASH256) || defined(WITH_HASH_BASH384) || defined(WITH_HASH_BASH512)
#include <libecc/utils/utils.h>
#include <libecc/hash/bash.h>

/*
 * This is an implementation of the BASH hash functions family (for sizes 224, 256, 384 and 512)
 * following the standard STB 34.101.77-2020 (http://apmi.bsu.by/assets/files/std/bash-spec24.pdf).
 * An english version of the specifications exist here: https://eprint.iacr.org/2016/587.pdf
 */

int _bash_init(bash_context *ctx, u8 digest_size)
{
	int ret;
	u8 *state = NULL;

	/*
	 * Check given inpur digest size: we only consider BASH versions
	 * mapped on instances (224, 256, 384, 512).
	 */
	MUST_HAVE(((digest_size == (224/8)) || (digest_size == (256/8)) ||
		   (digest_size == (384/8)) || (digest_size == (512/8))), ret, err);
	MUST_HAVE((ctx != NULL), ret, err);

	state = (u8*)(ctx->bash_state);

	/* Zeroize the internal state */
	ret = local_memset(state, 0, sizeof(ctx->bash_state)); EG(ret, err);

	ctx->bash_total = 0;
	ctx->bash_digest_size = digest_size;
	ctx->bash_block_size = (u8)((BASH_SLICES_X * BASH_SLICES_Y * sizeof(u64)) - (u8)(2 * digest_size));

	/* Put <l / 4>64 at the end of the state */
	state[(BASH_SLICES_X * BASH_SLICES_Y * sizeof(u64)) - sizeof(u64)] = (u8)digest_size;

	/* Detect endianness */
	ctx->bash_endian = arch_is_big_endian() ? BASH_BIG : BASH_LITTLE;

	ret = 0;

err:
	return ret;
}

int _bash_update(bash_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;
	u8 *state = NULL;

	MUST_HAVE(((ctx != NULL) && ((input != NULL) || (ilen == 0))), ret, err);

	state = (u8*)(ctx->bash_state);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (u8)(ctx->bash_total % ctx->bash_block_size);
	fill = (u16)(ctx->bash_block_size - left);

	ctx->bash_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(state + left, data_ptr, fill); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
		BASHF(ctx->bash_state, ctx->bash_endian);
	}
	while (remain_ilen >= ctx->bash_block_size) {
		ret = local_memcpy(state, data_ptr, ctx->bash_block_size); EG(ret, err);
		BASHF(ctx->bash_state, ctx->bash_endian);
		data_ptr += ctx->bash_block_size;
		remain_ilen -= ctx->bash_block_size;
	}
	if (remain_ilen > 0) {
		ret = local_memcpy(state + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize hash function. Returns 0 on success, -1 on error. */
int _bash_finalize(bash_context *ctx, u8 *output)
{
	u8 pos;
	int ret;
	u8 *state = NULL;

	MUST_HAVE((ctx != NULL) && (output != NULL), ret, err);

	state = (u8*)(ctx->bash_state);

	/* Handle the padding */
	pos = (u8)(ctx->bash_total % ctx->bash_block_size);

	ret = local_memset(state + pos, 0, (u8)((ctx->bash_block_size) - pos)); EG(ret, err);
	state[pos] = 0x40;

	BASHF(ctx->bash_state, ctx->bash_endian);

	/* Output the digest */
	ret = local_memcpy(output, state, ctx->bash_digest_size); EG(ret, err);

	ret = 0;
err:
	return ret;
}

#else
/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif
