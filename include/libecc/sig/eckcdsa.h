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
#ifdef WITH_SIG_ECKCDSA

#ifndef __ECKCDSA_H__
#define __ECKCDSA_H__
#include <libecc/words/words.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>

#define ECKCDSA_R_LEN(hsize, q_bit_len) LOCAL_MIN(hsize, BYTECEIL(q_bit_len))
#define ECKCDSA_S_LEN(q_bit_len) (BYTECEIL(q_bit_len))
#define ECKCDSA_SIGLEN(hsize, q_bit_len) (ECKCDSA_R_LEN(hsize, q_bit_len) + \
					  ECKCDSA_S_LEN(q_bit_len))
#define ECKCDSA_MAX_SIGLEN ECKCDSA_SIGLEN(MAX_DIGEST_SIZE, CURVES_MAX_Q_BIT_LEN)

/*
 * Compute max signature length for all the mechanisms enabled
 * in the library (see lib_ecc_config.h). Having that done during
 * preprocessing sadly requires some verbosity.
 */
#ifndef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN 0
#endif
#if ((EC_MAX_SIGLEN) < (ECKCDSA_MAX_SIGLEN))
#undef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN ECKCDSA_MAX_SIGLEN
#endif

ATTRIBUTE_WARN_UNUSED_RET int eckcdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv);

ATTRIBUTE_WARN_UNUSED_RET int eckcdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize,
		   u8 *siglen);

typedef struct {
	hash_context h_ctx;
	word_t magic;
} eckcdsa_sign_data;

struct ec_sign_context;

ATTRIBUTE_WARN_UNUSED_RET int _eckcdsa_sign_init(struct ec_sign_context *ctx);

ATTRIBUTE_WARN_UNUSED_RET int _eckcdsa_sign_update(struct ec_sign_context *ctx,
			 const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _eckcdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen);

typedef struct {
	hash_context h_ctx;
	u8 r[MAX_DIGEST_SIZE];
	nn s;
	word_t magic;
} eckcdsa_verify_data;

struct ec_verify_context;

ATTRIBUTE_WARN_UNUSED_RET int _eckcdsa_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _eckcdsa_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _eckcdsa_verify_finalize(struct ec_verify_context *ctx);

#endif /* __ECKCDSA_H__ */
#endif /* WITH_SIG_ECKCDSA */
