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
#if defined(WITH_SIG_ECDSA) || defined(WITH_SIG_DECDSA)

#ifndef __ECDSA_COMMON_H__
#define __ECDSA_COMMON_H__

#include <libecc/words/words.h>
#include <libecc/sig/ec_key.h>
#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>
#include <libecc/utils/utils.h>

#define ECDSA_R_LEN(q_bit_len)  (BYTECEIL(q_bit_len))
#define ECDSA_S_LEN(q_bit_len)  (BYTECEIL(q_bit_len))
#define ECDSA_SIGLEN(q_bit_len) (ECDSA_R_LEN(q_bit_len) + \
				 ECDSA_S_LEN(q_bit_len))
#define ECDSA_MAX_SIGLEN ECDSA_SIGLEN(CURVES_MAX_Q_BIT_LEN)

/*
 * Compute max signature length for all the mechanisms enabled
 * in the library (see lib_ecc_config.h). Having that done during
 * preprocessing sadly requires some verbosity.
 */
#ifndef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN 0
#endif
#if ((EC_MAX_SIGLEN) < (ECDSA_MAX_SIGLEN))
#undef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN ECDSA_MAX_SIGLEN
#endif

typedef struct {
	hash_context h_ctx;
	word_t magic;
} ecdsa_sign_data;

struct ec_sign_context;

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen);

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_sign_init(struct ec_sign_context *ctx, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen, ec_alg_type key_type);

typedef struct {
	nn r;
	nn s;
	hash_context h_ctx;
	word_t magic;
} ecdsa_verify_data;

struct ec_verify_context;

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_verify_init(struct ec_verify_context *ctx,
		       const u8 *sig, u8 siglen, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_verify_finalize(struct ec_verify_context *ctx, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __ecdsa_public_key_from_sig(ec_pub_key *out_pub1, ec_pub_key *out_pub2, const ec_params *params,
                                const u8 *sig, u8 siglen, const u8 *hash, u8 hsize,
                                ec_alg_type key_type);

#endif /* __ECDSA_COMMON_H__ */
#endif /* defined(WITH_SIG_ECDSA) || defined(WITH_SIG_DECDSA) */
