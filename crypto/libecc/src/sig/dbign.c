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
#ifdef WITH_SIG_DBIGN

#if !defined(WITH_HMAC)
#error "DBIGN signature needs HMAC, please activate it!"
#endif
#include <libecc/hash/hmac.h>

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "DBIGN"
#endif
#include <libecc/utils/dbg_sig.h>

int dbign_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	return __bign_init_pub_key(out_pub, in_priv, DBIGN);
}

int dbign_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	return __bign_siglen(p_bit_len, q_bit_len, hsize, blocksize, siglen);
}

int _dbign_sign_init(struct ec_sign_context *ctx)
{
	int ret;

	/* Override our random source with NULL since we want a deterministic
	 * generation.
	 */
	MUST_HAVE((ctx != NULL), ret, err);

	ctx->rand = NULL;
	ret =  __bign_sign_init(ctx, DBIGN);

err:
	return ret;
}

int _dbign_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen)
{
	int ret;

	/* NOTE: for deterministic ECDSA, the random source MUST be NULL, hence
	 * the following check.
	 */
	MUST_HAVE((ctx != NULL) && (ctx->rand == NULL), ret, err);

	ret = __bign_sign_update(ctx, chunk, chunklen, DBIGN);

err:
	return ret;
}

int _dbign_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	int ret;

	/* NOTE: for deterministic ECDSA, the random source MUST be NULL, hence
	 * the following check.
	 */
	MUST_HAVE((ctx != NULL) && (ctx->rand == NULL), ret, err);

	ret =  __bign_sign_finalize(ctx, sig, siglen, DBIGN);

err:
	return ret;
}

int _dbign_verify_init(struct ec_verify_context *ctx, const u8 *sig, u8 siglen)
{
	return __bign_verify_init(ctx, sig, siglen, DBIGN);
}

int _dbign_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	return __bign_verify_update(ctx, chunk, chunklen, DBIGN);
}

int _dbign_verify_finalize(struct ec_verify_context *ctx)
{
	return __bign_verify_finalize(ctx, DBIGN);
}

#else /* WITH_SIG_DBIGN */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_DBIGN */
