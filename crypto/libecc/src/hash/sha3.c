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

#if defined(WITH_HASH_SHA3_224) || defined(WITH_HASH_SHA3_256) || defined(WITH_HASH_SHA3_384) || defined(WITH_HASH_SHA3_512)
#include <libecc/utils/utils.h>
#include <libecc/hash/sha3.h>

/* Init function depending on the digest size. Return 0 on success, -1 on error. */
int _sha3_init(sha3_context *ctx, u8 digest_size)
{
	int ret;

	/*
	 * Check given inpur digest size: we only consider KECCAK versions
	 * mapped on SHA-3 instances (224, 256, 384, 512).
	 */
	MUST_HAVE(((digest_size == (224/8)) || (digest_size == (256/8)) ||
		   (digest_size == (384/8)) || (digest_size == (512/8))), ret, err);
	MUST_HAVE((ctx != NULL), ret, err);

	/* Zeroize the internal state */
	ret = local_memset(ctx->sha3_state, 0, sizeof(ctx->sha3_state)); EG(ret, err);

	ctx->sha3_idx = 0;
	ctx->sha3_digest_size = digest_size;
	ctx->sha3_block_size = (u8)((KECCAK_SLICES * KECCAK_SLICES * sizeof(u64)) - (u8)(2 * digest_size));

	/* Detect endianness */
	ctx->sha3_endian = arch_is_big_endian() ? SHA3_BIG : SHA3_LITTLE;

err:
	return ret;
}

/* Update hash function. Returns 0 on sucess, -1 on error. */
int _sha3_update(sha3_context *ctx, const u8 *input, u32 ilen)
{
	u32 i;
	u8 *state;
	int ret;

	MUST_HAVE(((ctx != NULL) && ((input != NULL) || (ilen == 0))), ret, err);

	state = (u8*)(ctx->sha3_state);

	for (i = 0; i < ilen; i++) {
		u64 idx = (ctx->sha3_endian == SHA3_LITTLE) ? ctx->sha3_idx : SWAP64_Idx(ctx->sha3_idx);
		ctx->sha3_idx++;
		/* Update the state, and adapt endianness order */
		state[idx] ^= input[i];
		if(ctx->sha3_idx == ctx->sha3_block_size){
			KECCAKF(ctx->sha3_state);
			ctx->sha3_idx = 0;
		}
	}

	ret = 0;

err:
	return ret;
}

/* Finalize hash function. Returns 0 on success, -1 on error. */
int _sha3_finalize(sha3_context *ctx, u8 *output)
{
	unsigned int i;
	u8 *state;
	int ret;

	MUST_HAVE((output != NULL) && (ctx != NULL), ret, err);
	MUST_HAVE((ctx->sha3_digest_size <= sizeof(ctx->sha3_state)), ret, err);

	state = (u8*)(ctx->sha3_state);

	/* Proceed with the padding of the last block */
	/* Compute the index depending on the endianness */
	if (ctx->sha3_endian == SHA3_LITTLE) {
		/* Little endian case */
		state[ctx->sha3_idx] ^= 0x06;
		state[ctx->sha3_block_size - 1] ^= 0x80;
	} else {
		/* Big endian case */
		state[SWAP64_Idx(ctx->sha3_idx)] ^= 0x06;
		state[SWAP64_Idx(ctx->sha3_block_size - 1)] ^= 0x80;
	}
	KECCAKF(ctx->sha3_state);
	for(i = 0; i < ctx->sha3_digest_size; i++){
		output[i] = (ctx->sha3_endian == SHA3_LITTLE) ? state[i] : state[SWAP64_Idx(i)];
	}

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
