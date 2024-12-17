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
#ifdef WITH_HASH_SHA3_224

#include <libecc/hash/sha3-224.h>

/* Init hash function. Returns 0 on success, -1 on error. */
int sha3_224_init(sha3_224_context *ctx)
{
	int ret;

	ret = _sha3_init(ctx, SHA3_224_DIGEST_SIZE); EG(ret, err);

	/* Tell that we are initialized */
	ctx->magic = SHA3_224_HASH_MAGIC;

err:
	return ret;
}

/* Update hash function. Returns 0 on success, -1 on error. */
int sha3_224_update(sha3_224_context *ctx, const u8 *input, u32 ilen)
{
	int ret;

	SHA3_224_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = _sha3_update((sha3_context *)ctx, input, ilen);

err:
	return ret;
}

/* Finalize hash function. Returns 0 on success, -1 on error. */
int sha3_224_final(sha3_224_context *ctx, u8 output[SHA3_224_DIGEST_SIZE])
{
	int ret;

	SHA3_224_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = _sha3_finalize((sha3_context *)ctx, output); EG(ret, err);

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
int sha3_224_scattered(const u8 **inputs, const u32 *ilens,
			u8 output[SHA3_224_DIGEST_SIZE])
{
	sha3_224_context ctx;
	int ret, pos = 0;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = sha3_224_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = sha3_224_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = sha3_224_final(&ctx, output);

err:
	return ret;
}

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
int sha3_224(const u8 *input, u32 ilen, u8 output[SHA3_224_DIGEST_SIZE])
{
	sha3_224_context ctx;
	int ret;

	ret = sha3_224_init(&ctx); EG(ret, err);
	ret = sha3_224_update(&ctx, input, ilen); EG(ret, err);
	ret = sha3_224_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SHA3_224 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHA3_224 */
