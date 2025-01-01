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
#ifdef WITH_SIG_ECOSDSA

#include <libecc/sig/ecsdsa_common.h>
#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECOSDSA"
#endif
#include <libecc/utils/dbg_sig.h>

/*
 * Initialize public key 'out_pub' from input private key 'in_priv'. The
 * function returns 0 on success, -1 on error.
 */
int ecosdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	return __ecsdsa_init_pub_key(out_pub, in_priv, ECOSDSA);
}

/*
 * Helper providing ECOSDSA signature length when exported to a buffer based on
 * hash algorithm digest and block size, generator point order bit length, and
 * underlying prime field order bit length. The function returns 0 on success,
 * -1 on error. On success, signature length is provided via 'siglen' out
 * parameter.
 */
int ecosdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize,
		   u8 *siglen)
{
	return __ecsdsa_siglen(p_bit_len, q_bit_len, hsize, blocksize, siglen);
}

/*
 * ECOSDSA signature initialization function. Returns 0 on success, -1 on
 * error.
 */
int _ecosdsa_sign_init(struct ec_sign_context *ctx)
{
	return __ecsdsa_sign_init(ctx, ECOSDSA, 1);
}

/* ECOSDSA signature update function. Returns 0 on success, -1 on error. */
int _ecosdsa_sign_update(struct ec_sign_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	return __ecsdsa_sign_update(ctx, chunk, chunklen);
}

/*
 * ECOSDSA signature finalization function. Returns 0 on success, -1 on error.
 */
int _ecosdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	return __ecsdsa_sign_finalize(ctx, sig, siglen);
}

/* ECOSDSA verify initialization function. Returns 0 on success, -1 on error. */
int _ecosdsa_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen)
{
	return __ecsdsa_verify_init(ctx, sig, siglen, ECOSDSA, 1);
}

/* ECOSDSA verify update function. Returns 0 on success, -1 on error. */
int _ecosdsa_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen)
{
	return __ecsdsa_verify_update(ctx, chunk, chunklen);
}

/* ECOSDSA verify finalization function. Returns 0 on success, -1 on error. */
int _ecosdsa_verify_finalize(struct ec_verify_context *ctx)
{
	return __ecsdsa_verify_finalize(ctx);
}

#else /* WITH_SIG_ECOSDSA */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECOSDSA */
