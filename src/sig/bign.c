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
#ifdef WITH_SIG_BIGN

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "BIGN"
#endif
#include <libecc/utils/dbg_sig.h>

int bign_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	return __bign_init_pub_key(out_pub, in_priv, BIGN);
}

int bign_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	return __bign_siglen(p_bit_len, q_bit_len, hsize, blocksize, siglen);
}

int _bign_sign_init(struct ec_sign_context *ctx)
{
	return __bign_sign_init(ctx, BIGN);
}

int _bign_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen)
{
	return __bign_sign_update(ctx, chunk, chunklen, BIGN);
}

int _bign_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	return __bign_sign_finalize(ctx, sig, siglen, BIGN);
}

int _bign_verify_init(struct ec_verify_context *ctx, const u8 *sig, u8 siglen)
{
	return __bign_verify_init(ctx, sig, siglen, BIGN);
}

int _bign_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	return __bign_verify_update(ctx, chunk, chunklen, BIGN);
}

int _bign_verify_finalize(struct ec_verify_context *ctx)
{
	return __bign_verify_finalize(ctx, BIGN);
}

#else /* WITH_SIG_BIGN */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_BIGN */
