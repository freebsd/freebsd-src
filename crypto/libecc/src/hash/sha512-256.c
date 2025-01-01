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
#ifdef WITH_HASH_SHA512_256

#include <libecc/hash/sha512-256.h>

/* Init hash function. Returns 0 on success, -1 on error. */
int sha512_256_init(sha512_256_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ctx->sha512_total[0] = ctx->sha512_total[1] = 0;
	ctx->sha512_state[0] = (u64)(0x22312194FC2BF72C);
	ctx->sha512_state[1] = (u64)(0x9F555FA3C84C64C2);
	ctx->sha512_state[2] = (u64)(0x2393B86B6F53B151);
	ctx->sha512_state[3] = (u64)(0x963877195940EABD);
	ctx->sha512_state[4] = (u64)(0x96283EE2A88EFFE3);
	ctx->sha512_state[5] = (u64)(0xBE5E1E2553863992);
	ctx->sha512_state[6] = (u64)(0x2B0199FC2C85B8AA);
	ctx->sha512_state[7] = (u64)(0x0EB72DDC81C52CA2);

	/* Tell that we are initialized */
	ctx->magic = SHA512_256_HASH_MAGIC;
	ret = 0;

err:
	return ret;
}

/* Update hash function. Returns 0 on success, -1 on error. */
int sha512_256_update(sha512_256_context *ctx, const u8 *input, u32 ilen)
{
	int ret;

	SHA512_256_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = sha512_core_update(ctx, input, ilen);

err:
	return ret;
}

/*
 * Finalize hash function. Returns 0 on success, -1 on error.
 */
int sha512_256_final(sha512_256_context *ctx, u8 output[SHA512_256_DIGEST_SIZE])
{
	int ret;

	SHA512_256_HASH_CHECK_INITIALIZED(ctx, ret, err);
	ret = sha512_core_final(ctx, output, SHA512_256_DIGEST_SIZE); EG(ret, err);

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
int sha512_256_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[SHA512_256_DIGEST_SIZE])
{
	sha512_256_context ctx;
	int pos = 0;
	int ret;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = sha512_256_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = sha512_256_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = sha512_256_final(&ctx, output);

err:
	return ret;
}

/* init/update/finalize on a single buffer 'input' of length 'ilen'. */
int sha512_256(const u8 *input, u32 ilen, u8 output[SHA512_256_DIGEST_SIZE])
{
	sha512_256_context ctx;
	int ret;

	ret = sha512_256_init(&ctx); EG(ret, err);
	ret = sha512_256_update(&ctx, input, ilen); EG(ret, err);
	ret = sha512_256_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SHA512_256 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHA512_256 */
