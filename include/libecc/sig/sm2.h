/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */

#include <libecc/lib_ecc_config.h>
#include <libecc/lib_ecc_types.h>
#ifdef WITH_SIG_SM2

#ifndef __SM2_H__
#define __SM2_H__

#include <libecc/words/words.h>
#include <libecc/sig/ec_key.h>
#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>
#include <libecc/utils/utils.h>

#define SM2_R_LEN(q_bit_len)  (BYTECEIL(q_bit_len))
#define SM2_S_LEN(q_bit_len)  (BYTECEIL(q_bit_len))
#define SM2_SIGLEN(q_bit_len) (SM2_R_LEN(q_bit_len) + \
				 SM2_S_LEN(q_bit_len))
#define SM2_MAX_SIGLEN SM2_SIGLEN(CURVES_MAX_Q_BIT_LEN)
#define SM2_MAX_ID_LEN 8191 /* SM2 user ID max byte length */

/*
 * Compute max signature length for all the mechanisms enabled
 * in the library (see lib_ecc_config.h). Having that done during
 * preprocessing sadly requires some verbosity.
 */
#ifndef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN 0
#endif
#if ((EC_MAX_SIGLEN) < (SM2_MAX_SIGLEN))
#undef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN SM2_MAX_SIGLEN
#endif

typedef struct {
	hash_context h_ctx;
	word_t magic;
} sm2_sign_data;

struct ec_sign_context;

ATTRIBUTE_WARN_UNUSED_RET int sm2_gen_priv_key(ec_priv_key *priv_key);

ATTRIBUTE_WARN_UNUSED_RET int sm2_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv);

ATTRIBUTE_WARN_UNUSED_RET int sm2_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen);

ATTRIBUTE_WARN_UNUSED_RET int _sm2_sign_init(struct ec_sign_context *ctx);

ATTRIBUTE_WARN_UNUSED_RET int _sm2_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _sm2_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen);

typedef struct {
	nn r;
	nn s;
	hash_context h_ctx;
	word_t magic;
} sm2_verify_data;

struct ec_verify_context;

ATTRIBUTE_WARN_UNUSED_RET int _sm2_verify_init(struct ec_verify_context *ctx,
		       const u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _sm2_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _sm2_verify_finalize(struct ec_verify_context *ctx);

#endif /* __SM2_H__ */
#endif /* WITH_SIG_SM2 */
