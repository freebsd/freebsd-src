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
#include <libecc/lib_ecc_config.h>
#ifdef WITH_HASH_SHAKE256

#include <libecc/hash/shake256.h>

int shake256_init(shake256_context *ctx)
{
	int ret;

	ret = _shake_init(ctx, SHAKE256_DIGEST_SIZE, SHAKE256_BLOCK_SIZE); EG(ret, err);

	/* Tell that we are initialized */
	ctx->magic = SHAKE256_HASH_MAGIC;

err:
	return ret;
}

int shake256_update(shake256_context *ctx, const u8 *input, u32 ilen)
{
	int ret;

	SHAKE256_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = _shake_update((shake_context *)ctx, input, ilen);

err:
	return ret;
}

int shake256_final(shake256_context *ctx, u8 output[SHAKE256_DIGEST_SIZE])
{
	int ret;

	SHAKE256_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = _shake_finalize((shake_context *)ctx, output);

	/* Tell that we are uninitialized */
	ctx->magic = WORD(0);

err:
	return ret;
}

int shake256_scattered(const u8 **inputs, const u32 *ilens,
			u8 output[SHAKE256_DIGEST_SIZE])
{
	shake256_context ctx;
	int pos = 0, ret;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = shake256_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = shake256_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = shake256_final(&ctx, output);

err:
	return ret;
}

int shake256(const u8 *input, u32 ilen, u8 output[SHAKE256_DIGEST_SIZE])
{
	int ret;
	shake256_context ctx;

	ret = shake256_init(&ctx); EG(ret, err);
	ret = shake256_update(&ctx, input, ilen); EG(ret, err);
	ret = shake256_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SHAKE256 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHAKE256 */
