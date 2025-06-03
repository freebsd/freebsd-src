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
#ifdef WITH_HASH_SHA512_224

#include <libecc/hash/sha512-224.h>

/* Init hash function. Returns 0 on success, -1 on error. */
int sha512_224_init(sha512_224_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ctx->sha512_total[0] = ctx->sha512_total[1] = 0;
	ctx->sha512_state[0] = (u64)(0x8C3D37C819544DA2);
	ctx->sha512_state[1] = (u64)(0x73E1996689DCD4D6);
	ctx->sha512_state[2] = (u64)(0x1DFAB7AE32FF9C82);
	ctx->sha512_state[3] = (u64)(0x679DD514582F9FCF);
	ctx->sha512_state[4] = (u64)(0x0F6D2B697BD44DA8);
	ctx->sha512_state[5] = (u64)(0x77E36F7304C48942);
	ctx->sha512_state[6] = (u64)(0x3F9D85A86A1D36C8);
	ctx->sha512_state[7] = (u64)(0x1112E6AD91D692A1);

	/* Tell that we are initialized */
	ctx->magic = SHA512_224_HASH_MAGIC;
	ret = 0;

err:
	return ret;
}

/* Update hash function. Returns 0 on success, -1 on error. */
int sha512_224_update(sha512_224_context *ctx, const u8 *input, u32 ilen)
{
	int ret;

	SHA512_224_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = sha512_core_update(ctx, input, ilen);

err:
	return ret;
}

/* Finalize hash function. Returns 0 on success, -1 on error. */
int sha512_224_final(sha512_224_context *ctx, u8 output[SHA512_224_DIGEST_SIZE])
{
	int ret;

	SHA512_224_HASH_CHECK_INITIALIZED(ctx, ret, err);

	ret = sha512_core_final(ctx, output, SHA512_224_DIGEST_SIZE); EG(ret, err);

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
int sha512_224_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[SHA512_224_DIGEST_SIZE])
{
	sha512_224_context ctx;
	int pos = 0;
	int ret;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = sha512_224_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = sha512_224_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = sha512_224_final(&ctx, output);

err:
	return ret;
}

/* init/update/finalize on a single buffer 'input' of length 'ilen'. */
int sha512_224(const u8 *input, u32 ilen, u8 output[SHA512_224_DIGEST_SIZE])
{
	sha512_224_context ctx;
	int ret;

	ret = sha512_224_init(&ctx); EG(ret, err);
	ret = sha512_224_update(&ctx, input, ilen); EG(ret, err);
	ret = sha512_224_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_SHA512_224 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HASH_SHA512_224 */
