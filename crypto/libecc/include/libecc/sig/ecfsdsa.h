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
#ifdef WITH_SIG_ECFSDSA

#ifndef __ECFSDSA_H__
#define __ECFSDSA_H__

#include <libecc/sig/ec_key.h>
#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>
#include <libecc/utils/utils.h>

#define ECFSDSA_R_LEN(p_bit_len) (2 * (BYTECEIL(p_bit_len)))
#define ECFSDSA_S_LEN(q_bit_len) (BYTECEIL(q_bit_len))
#define ECFSDSA_SIGLEN(p_bit_len, q_bit_len) (ECFSDSA_R_LEN(p_bit_len) + \
					      ECFSDSA_S_LEN(q_bit_len))
#define ECFSDSA_MAX_SIGLEN ECFSDSA_SIGLEN(CURVES_MAX_P_BIT_LEN, \
					  CURVES_MAX_Q_BIT_LEN)

/*
 * Compute max signature length for all the mechanisms enabled
 * in the library (see lib_ecc_config.h). Having that done during
 * preprocessing sadly requires some verbosity.
 */
#ifndef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN 0
#endif
#if ((EC_MAX_SIGLEN) < (ECFSDSA_MAX_SIGLEN))
#undef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN ECFSDSA_MAX_SIGLEN
#endif

typedef struct {
	nn k;
	u8 r[2 * NN_MAX_BYTE_LEN];
	hash_context h_ctx;
	word_t magic;
} ecfsdsa_sign_data;

struct ec_sign_context;

ATTRIBUTE_WARN_UNUSED_RET int ecfsdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv);

ATTRIBUTE_WARN_UNUSED_RET int ecfsdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize,
		   u8 *siglen);

ATTRIBUTE_WARN_UNUSED_RET int _ecfsdsa_sign_init(struct ec_sign_context *ctx);

ATTRIBUTE_WARN_UNUSED_RET int _ecfsdsa_sign_update(struct ec_sign_context *ctx,
			 const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _ecfsdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen);

typedef struct {
	u8 r[2 * NN_MAX_BYTE_LEN];
	nn s;
	hash_context h_ctx;
	word_t magic;
} ecfsdsa_verify_data;

struct ec_verify_context;

ATTRIBUTE_WARN_UNUSED_RET int _ecfsdsa_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _ecfsdsa_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _ecfsdsa_verify_finalize(struct ec_verify_context *ctx);

/* Batch verification function */
ATTRIBUTE_WARN_UNUSED_RET int ecfsdsa_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
              const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
              hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
              verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len);


#endif /* __ECFSDSA_H__ */
#endif /* WITH_SIG_ECFSDSA */
