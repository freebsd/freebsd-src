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
#include <libecc/lib_ecc_types.h>
#ifdef WITH_SIG_ECOSDSA

#ifndef __ECOSDSA_H__
#define __ECOSDSA_H__
#include <libecc/sig/ecsdsa.h>
#include <libecc/words/words.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>

#define ECOSDSA_R_LEN(hsize) (hsize)
#define ECOSDSA_S_LEN(q_bit_len) (BYTECEIL(q_bit_len))
#define ECOSDSA_SIGLEN(hsize, q_bit_len) (ECOSDSA_R_LEN(hsize) + \
					 ECOSDSA_S_LEN(q_bit_len))
#define ECOSDSA_MAX_SIGLEN ECOSDSA_SIGLEN(MAX_DIGEST_SIZE, CURVES_MAX_Q_BIT_LEN)

/*
 * Compute max signature length for all the mechanisms enabled
 * in the library (see lib_ecc_config.h). Having that done during
 * preprocessing sadly requires some verbosity.
 */
#ifndef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN 0
#endif
#if ((EC_MAX_SIGLEN) < (ECOSDSA_MAX_SIGLEN))
#undef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN ECOSDSA_MAX_SIGLEN
#endif

ATTRIBUTE_WARN_UNUSED_RET int ecosdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv);

ATTRIBUTE_WARN_UNUSED_RET int ecosdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize,
		   u8 *siglen);

ATTRIBUTE_WARN_UNUSED_RET int _ecosdsa_sign_init(struct ec_sign_context *ctx);

ATTRIBUTE_WARN_UNUSED_RET int _ecosdsa_sign_update(struct ec_sign_context *ctx,
			 const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _ecosdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _ecosdsa_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _ecosdsa_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _ecosdsa_verify_finalize(struct ec_verify_context *ctx);

#endif /* __ECOSDSA_H__ */
#endif /* WITH_SIG_ECOSDSA */
