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
#if defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448)

#ifndef __EDDSA_H__
#define __EDDSA_H__

#include <libecc/words/words.h>
#include <libecc/sig/ec_key.h>
#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>
#include <libecc/utils/utils.h>

/*
 * EDDSA exported encoded public keys are of fixed known sizes depending
 * on the EDDSA variant
 */
#if defined(WITH_SIG_EDDSA25519)
#define EDDSA25519_PUB_KEY_ENCODED_LEN 32
#endif
#if defined(WITH_SIG_EDDSA448)
#define EDDSA448_PUB_KEY_ENCODED_LEN   57
#endif

/* Maximum size depending on what is defined */
#if defined(WITH_SIG_EDDSA25519) && defined(WITH_SIG_EDDSA448)
#define EDDSA_MAX_PUB_KEY_ENCODED_LEN LOCAL_MAX(EDDSA25519_PUB_KEY_ENCODED_LEN, EDDSA448_PUB_KEY_ENCODED_LEN)
#endif

#if defined(WITH_SIG_EDDSA25519) && !defined(WITH_SIG_EDDSA448)
#define EDDSA_MAX_PUB_KEY_ENCODED_LEN EDDSA25519_PUB_KEY_ENCODED_LEN
#endif

#if !defined(WITH_SIG_EDDSA25519) && defined(WITH_SIG_EDDSA448)
#define EDDSA_MAX_PUB_KEY_ENCODED_LEN EDDSA448_PUB_KEY_ENCODED_LEN
#endif


/*
 * NOTE: for EDDSA, the signature length is twice the encoding of integers,
 * which corresponds to half the hash size.
 */
#define EDDSA_R_LEN(hsize)  (hsize / 2)
#define EDDSA_S_LEN(hsize)  (hsize / 2)
#define EDDSA_SIGLEN(hsize) (EDDSA_R_LEN(hsize) + EDDSA_S_LEN(hsize))
#define EDDSA_MAX_SIGLEN EDDSA_SIGLEN(MAX_DIGEST_SIZE)

/*
 * Compute max signature length for all the mechanisms enabled
 * in the library (see lib_ecc_config.h). Having that done during
 * preprocessing sadly requires some verbosity.
 */
#ifndef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN 0
#endif
#if ((EC_MAX_SIGLEN) < (EDDSA_MAX_SIGLEN))
#undef EC_MAX_SIGLEN
#define EC_MAX_SIGLEN EDDSA_MAX_SIGLEN
#endif

typedef struct {
	hash_context h_ctx;
	word_t magic;
} eddsa_sign_data;

struct ec_sign_context;

ATTRIBUTE_WARN_UNUSED_RET int eddsa_gen_priv_key(ec_priv_key *priv_key);
ATTRIBUTE_WARN_UNUSED_RET int eddsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv);

ATTRIBUTE_WARN_UNUSED_RET int eddsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen);

ATTRIBUTE_WARN_UNUSED_RET int _eddsa_sign_init_pre_hash(struct ec_sign_context *ctx);

ATTRIBUTE_WARN_UNUSED_RET int _eddsa_sign_update_pre_hash(struct ec_sign_context *ctx,
				const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _eddsa_sign_finalize_pre_hash(struct ec_sign_context *ctx,
				  u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _eddsa_sign(u8 *sig, u8 siglen, const ec_key_pair *key_pair,
		const u8 *m, u32 mlen, int (*rand) (nn_t out, nn_src_t q),
		ec_alg_type sig_type, hash_alg_type hash_type,
		const u8 *adata, u16 adata_len);

typedef struct {
	prj_pt _R;
	nn S;
	hash_context h_ctx;
	hash_context h_ctx_pre_hash;
	word_t magic;
} eddsa_verify_data;

struct ec_verify_context;

ATTRIBUTE_WARN_UNUSED_RET int _eddsa_verify_init(struct ec_verify_context *ctx,
		       const u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _eddsa_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _eddsa_verify_finalize(struct ec_verify_context *ctx);

/* Functions specific to EdDSA */
ATTRIBUTE_WARN_UNUSED_RET int eddsa_derive_priv_key(ec_priv_key *priv_key);
ATTRIBUTE_WARN_UNUSED_RET int eddsa_import_priv_key(ec_priv_key *priv_key, const u8 *buf, u16 buflen,
			  const ec_params *shortw_curve_params,
			  ec_alg_type sig_type);
ATTRIBUTE_WARN_UNUSED_RET int eddsa_import_pub_key(ec_pub_key *out_pub, const u8 *buf, u16 buflen,
			 const ec_params *shortw_curve_params,
			 ec_alg_type sig_type);
ATTRIBUTE_WARN_UNUSED_RET int eddsa_export_pub_key(const ec_pub_key *in_pub, u8 *buf, u16 buflen);
ATTRIBUTE_WARN_UNUSED_RET int eddsa_import_key_pair_from_priv_key_buf(ec_key_pair *kp,
					    const u8 *buf, u16 buflen,
					    const ec_params *shortw_curve_params,
					    ec_alg_type sig_type);
/* Batch verification function */
ATTRIBUTE_WARN_UNUSED_RET int eddsa_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
              const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
              hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
	      verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len);

#endif /* __EDDSA_H__ */
#endif /* defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448) */
