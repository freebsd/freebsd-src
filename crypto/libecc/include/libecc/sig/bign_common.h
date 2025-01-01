/*
 *  Copyright (C) 2022 - This file is part of libecc project
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
#if defined(WITH_SIG_BIGN) || defined(WITH_SIG_DBIGN)

#ifndef __BIGN_COMMON_H__
#define __BIGN_COMMON_H__

#include <libecc/words/words.h>
#include <libecc/sig/ec_key.h>
#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>
#include <libecc/utils/utils.h>


/* NOTE: BIGN uses per its standard the BELT-HASH hash function as its "internal"
 * hash function, as well as the BELT encryption block cipher during the deterministic
 * computation of the nonce for the deterministic version of BIGN.
 * Hence the sanity check below.
 */
#if !defined(WITH_HASH_BELT_HASH)
#error "BIGN and DBIGN need BELT-HASH, please activate it!"
#endif

#define BIGN_S0_LEN(q_bit_len)	(BYTECEIL(q_bit_len) / 2)
#define BIGN_S1_LEN(q_bit_len)  (BYTECEIL(q_bit_len))
#define BIGN_SIGLEN(q_bit_len) (BIGN_S0_LEN(q_bit_len) + \
				 BIGN_S1_LEN(q_bit_len))
#define BIGN_MAX_SIGLEN BIGN_SIGLEN(CURVES_MAX_Q_BIT_LEN)

/*
 * Compute max signature length for all the mechanisms enabled
 * in the library (see lib_ecc_config.h). Having that done during
 * preprocessing sadly requires some verbosity.
 */
#ifndef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN 0
#endif
#if ((EC_MAX_SIGLEN) < (BIGN_MAX_SIGLEN))
#undef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN BIGN_MAX_SIGLEN
#endif


/* The additional data for bign are specific. We provide
 * helpers to extract them from an adata pointer.
 */
int bign_get_oid_from_adata(const u8 *adata, u16 adata_len, const u8 **oid_ptr, u16 *oid_len);

int bign_get_t_from_adata(const u8 *adata, u16 adata_len, const u8 **t_ptr, u16 *t_len);

int bign_set_adata(u8 *adata, u16 adata_len, const u8 *oid, u16 oid_len, const u8 *t, u16 t_len);


typedef struct {
	hash_context h_ctx;
	word_t magic;
} bign_sign_data;

struct ec_sign_context;

ATTRIBUTE_WARN_UNUSED_RET int __bign_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __bign_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen);

ATTRIBUTE_WARN_UNUSED_RET int __bign_sign_init(struct ec_sign_context *ctx, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __bign_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __bign_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen, ec_alg_type key_type);

typedef struct {
	u8 s0_sig[BIGN_S0_LEN(CURVES_MAX_Q_BIT_LEN)];
	nn s0;
	nn s1;
	hash_context h_ctx;
	word_t magic;
} bign_verify_data;

struct ec_verify_context;

ATTRIBUTE_WARN_UNUSED_RET int __bign_verify_init(struct ec_verify_context *ctx,
		       const u8 *sig, u8 siglen, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __bign_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen, ec_alg_type key_type);

ATTRIBUTE_WARN_UNUSED_RET int __bign_verify_finalize(struct ec_verify_context *ctx, ec_alg_type key_type);

#endif /* __BIGN_COMMON_H__ */
#endif /* defined(WITH_SIG_BIGN) || defined(WITH_SIG_DBIGN) */
