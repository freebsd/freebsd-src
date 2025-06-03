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
#ifdef WITH_HASH_SHA512

#include <libecc/hash/sha512.h>

/* Init hash function. Returns 0 on success, -1 on error. */
int sha512_init(sha512_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ctx->sha512_total[0] = ctx->sha512_total[1] = 0;
	ctx->sha512_state[0] = (u64)(0x6A09E667F3BCC908);
	ctx->sha512_state[1] = (u64)(0xBB67AE8584CAA73B);
	ctx->sha512_state[2] = (u64)(0x3C6EF372FE94F82B);
	ctx->sha512_state[3] = (u64)(0xA54FF53A5F1D36F1);
	ctx->sha512_state[4] = (u64)(0x510E527FADE682D1);
	ctx->sha512_state[5] = (u64)(0x9B05688C2B3E6C1F);
	ctx->sha512_state[6] = (u64)(0x1F83D9ABFB41BD6B);
	ctx->sha512_state[7] = (u64)(0x5BE0CD19137E2179);

	/* Tell that we are initialized */
	ctx->magic = SHA512_HASH_MAGIC;
	ret = 0;

err:
	return ret;
}

/* Update hash function. Returns 0 on success, -1 on error. */
int sha512_update(sha512_context *ctx, const u8 *input, u32 ilen)
{
	int ret;

	SHA512_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = sha512_core_update(ctx, input, ilen);

err:
	return ret;
}

/*
 * Finalize hash function. Returns 0 on success, -1 on error. */
int sha512_final(sha512_context *ctx, u8 output[SHA512_DIGEST_SIZE])
{
	int ret;

	SHA512_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = sha512_core_final(ctx, output, SHA512_DIGEST_SIZE); EG(ret, err);

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
int sha512_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[SHA512_DIGEST_SIZE])
{
	sha512_context ctx;
	int pos = 0;
	int ret;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = sha512_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = sha512_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = sha512_final(&ctx, output);

err:
	return ret;
}

/* init/update/finalize on a single buffer 'input' of length 'ilen'. */
int sha512(const u8 *input, u32 ilen, u8 output[SHA512_DIGEST_SIZE])
{
	sha512_context ctx;
	int ret;

	ret = sha512_init(&ctx); EG(ret, err);
	ret = sha512_update(&ctx, input, ilen); EG(ret, err);
	ret = sha512_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SHA512 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHA512 */
