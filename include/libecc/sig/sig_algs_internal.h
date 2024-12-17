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
#ifndef __SIG_ALGS_INTERNAL_H__
#define __SIG_ALGS_INTERNAL_H__

#include <libecc/hash/hash_algs.h>
#include <libecc/curves/curves.h>
#include <libecc/sig/ec_key.h>
#include <libecc/sig/ecdsa.h>
#include <libecc/sig/eckcdsa.h>
#include <libecc/sig/ecsdsa.h>
#include <libecc/sig/ecosdsa.h>
#include <libecc/sig/ecfsdsa.h>
#include <libecc/sig/ecgdsa.h>
#include <libecc/sig/ecrdsa.h>
#include <libecc/sig/sm2.h>
#include <libecc/sig/eddsa.h>
#include <libecc/sig/decdsa.h>
#include <libecc/sig/bign.h>
#include <libecc/sig/dbign.h>
#include <libecc/sig/bip0340.h>
/* Includes for fuzzing */
#ifdef USE_CRYPTOFUZZ
#include <libecc/sig/fuzzing_ecdsa.h>
#include <libecc/sig/fuzzing_ecgdsa.h>
#include <libecc/sig/fuzzing_ecrdsa.h>
#endif

#if (EC_MAX_SIGLEN == 0)
#error "It seems you disabled all signature schemes in lib_ecc_config.h"
#endif

/*
 * All the signature algorithms we support are abstracted using the following
 * structure (and following map) which provides for each hash alg its
 * digest size, its block size and the associated scattered function.
 */
typedef struct {
	ec_alg_type type;
	const char *name;

	ATTRIBUTE_WARN_UNUSED_RET int (*siglen) (u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen);

	ATTRIBUTE_WARN_UNUSED_RET int (*gen_priv_key) (ec_priv_key *priv_key);
	ATTRIBUTE_WARN_UNUSED_RET int (*init_pub_key) (ec_pub_key *pub_key, const ec_priv_key *priv_key);

	ATTRIBUTE_WARN_UNUSED_RET int (*sign_init) (struct ec_sign_context * ctx);
	ATTRIBUTE_WARN_UNUSED_RET int (*sign_update) (struct ec_sign_context * ctx,
			    const u8 *chunk, u32 chunklen);
	ATTRIBUTE_WARN_UNUSED_RET int (*sign_finalize) (struct ec_sign_context * ctx,
			      u8 *sig, u8 siglen);
	ATTRIBUTE_WARN_UNUSED_RET int (*sign) (u8 *sig, u8 siglen, const ec_key_pair *key_pair,
		     const u8 *m, u32 mlen, int (*rand) (nn_t out, nn_src_t q),
		     ec_alg_type sig_type, hash_alg_type hash_type,
		     const u8 *adata, u16 adata_len);

	ATTRIBUTE_WARN_UNUSED_RET int (*verify_init) (struct ec_verify_context * ctx,
			    const u8 *sig, u8 siglen);
	ATTRIBUTE_WARN_UNUSED_RET int (*verify_update) (struct ec_verify_context * ctx,
			      const u8 *chunk, u32 chunklen);
	ATTRIBUTE_WARN_UNUSED_RET int (*verify_finalize) (struct ec_verify_context * ctx);
	ATTRIBUTE_WARN_UNUSED_RET int (*verify) (const u8 *sig, u8 siglen, const ec_pub_key *pub_key,
	      const u8 *m, u32 mlen, ec_alg_type sig_type,
	      hash_alg_type hash_type, const u8 *adata, u16 adata_len);
	ATTRIBUTE_WARN_UNUSED_RET int (*verify_batch) (const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
              const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
              hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
	      verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len);

} ec_sig_mapping;

/* Sanity check to ensure our sig mapping does not contain
 * NULL pointers
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int sig_mapping_sanity_check(const ec_sig_mapping *sm)
{
	int ret;

	MUST_HAVE(((sm != NULL) && (sm->name != NULL) && (sm->siglen != NULL) &&
		    (sm->gen_priv_key != NULL) && (sm->init_pub_key != NULL) &&
		    (sm->sign_init != NULL) && (sm->sign_update != NULL) &&
		    (sm->sign_finalize != NULL) && (sm->sign != NULL) &&
		    (sm->verify_init != NULL) && (sm->verify_update != NULL) &&
		    (sm->verify_finalize != NULL) && (sm->verify != NULL) &&
		    (sm->verify_batch != NULL)),
		   ret, err);

	ret = 0;

err:
	return ret;
}

/*
 * Each specific signature scheme need to maintain some specific
 * data between calls to init()/update()/finalize() functions.
 *
 * Each scheme provides a specific structure for that purpose
 * (in its .h file) which we include in the union below. A field
 * of that type (.sign_data) is then included in the generic
 * struct ec_sign_context below.
 *
 * The purpose of that work is to allow static declaration and
 * allocation of common struct ec_sign_context with enough room
 * available for all supported signature types.
 */

typedef union {
#if defined(WITH_SIG_ECDSA) || defined(WITH_SIG_DECDSA)		/* ECDSA and DECDSA */
	ecdsa_sign_data ecdsa;
#endif
#ifdef WITH_SIG_ECKCDSA		/* ECKCDSA */
	eckcdsa_sign_data eckcdsa;
#endif
#if (defined(WITH_SIG_ECSDSA) || defined(WITH_SIG_ECOSDSA))	/* EC[O]SDSA  */
	ecsdsa_sign_data ecsdsa;
#endif
#ifdef WITH_SIG_ECFSDSA		/* ECFSDSA */
	ecfsdsa_sign_data ecfsdsa;
#endif
#ifdef WITH_SIG_ECGDSA		/* ECGDSA  */
	ecgdsa_sign_data ecgdsa;
#endif
#ifdef WITH_SIG_ECRDSA		/* ECRDSA  */
	ecrdsa_sign_data ecrdsa;
#endif
#ifdef WITH_SIG_SM2		/* SM2	*/
	sm2_sign_data sm2;
#endif
#if defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448)	/* EDDSA25519, EDDSA448	 */
	eddsa_sign_data eddsa;
#endif
#if defined(WITH_SIG_BIGN) || defined(WITH_SIG_DBIGN)	/* BIGN and DBIGN */
	bign_sign_data bign;
#endif
} sig_sign_data;

/*
 * The 'struct ec_sign_context' below provides a persistent state
 * between successive calls to ec_sign_{init,update,finalize}().
 */
struct ec_sign_context {
	word_t ctx_magic;
	const ec_key_pair *key_pair;
	ATTRIBUTE_WARN_UNUSED_RET int (*rand) (nn_t out, nn_src_t q);
	const hash_mapping *h;
	const ec_sig_mapping *sig;

	sig_sign_data sign_data;

	/* Optional ancillary data. This data is
	 * optionnally used by the signature algorithm.
	 */
	const u8 *adata;
	u16 adata_len;
};

#define SIG_SIGN_MAGIC ((word_t)(0x4ed73cfe4594dfd3ULL))
ATTRIBUTE_WARN_UNUSED_RET static inline int sig_sign_check_initialized(struct ec_sign_context *ctx)
{
	return (((ctx == NULL) || (ctx->ctx_magic != SIG_SIGN_MAGIC)) ? -1 : 0);
}

typedef union {
#if defined(WITH_SIG_ECDSA) || defined(WITH_SIG_DECDSA)		/* ECDSA and DECDSA */
	ecdsa_verify_data ecdsa;
#endif
#ifdef WITH_SIG_ECKCDSA		/* ECKCDSA */
	eckcdsa_verify_data eckcdsa;
#endif
#if (defined(WITH_SIG_ECSDSA) || defined(WITH_SIG_ECOSDSA))	/* EC[O]SDSA  */
	ecsdsa_verify_data ecsdsa;
#endif
#ifdef WITH_SIG_ECFSDSA		/* ECFSDSA */
	ecfsdsa_verify_data ecfsdsa;
#endif
#ifdef WITH_SIG_ECGDSA		/* ECGDSA */
	ecgdsa_verify_data ecgdsa;
#endif
#ifdef WITH_SIG_ECRDSA		/* ECRDSA */
	ecrdsa_verify_data ecrdsa;
#endif
#ifdef WITH_SIG_SM2		/* SM2 */
	sm2_verify_data sm2;
#endif
#if defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448)	/* EDDSA25519, EDDSA448	 */
	eddsa_verify_data eddsa;
#endif
#if defined(WITH_SIG_BIGN) || defined(WITH_SIG_DBIGN)	/* BIGN and DBIGN */
	bign_verify_data bign;
#endif
#if defined(WITH_SIG_BIP0340)
	bip0340_verify_data bip0340;
#endif
} sig_verify_data;

/*
 * The 'struct ec_verify_context' below provides a persistent state
 * between successive calls to ec_verify_{init,update,finalize}().
 */
struct ec_verify_context {
	word_t ctx_magic;
	const ec_pub_key *pub_key;
	const hash_mapping *h;
	const ec_sig_mapping *sig;

	sig_verify_data verify_data;

	/* Optional ancillary data. This data is
	 * optionnally used by the signature algorithm.
	 */
	const u8 *adata;
	u16 adata_len;
};

#define SIG_VERIFY_MAGIC ((word_t)(0x7e0d42d13e3159baULL))
ATTRIBUTE_WARN_UNUSED_RET static inline int sig_verify_check_initialized(struct ec_verify_context *ctx)
{
	return (((ctx == NULL) || (ctx->ctx_magic != SIG_VERIFY_MAGIC)) ? -1 : 0);
}

/* Generic signature and verification APIs that will in fact call init / update / finalize in
 * backend. Used for signature and verification functions that support these streaming APIs.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET int generic_ec_sign(u8 *sig, u8 siglen, const ec_key_pair *key_pair,
	     const u8 *m, u32 mlen, int (*rand) (nn_t out, nn_src_t q),
	     ec_alg_type sig_type, hash_alg_type hash_type, const u8 *adata, u16 adata_len);
ATTRIBUTE_WARN_UNUSED_RET int generic_ec_verify(const u8 *sig, u8 siglen, const ec_pub_key *pub_key,
	      const u8 *m, u32 mlen, ec_alg_type sig_type,
	      hash_alg_type hash_type, const u8 *adata, u16 adata_len);

/* Generic init / update / finalize functions returning an error and telling that they are
 * unsupported.
 */
ATTRIBUTE_WARN_UNUSED_RET int unsupported_sign_init(struct ec_sign_context * ctx);
ATTRIBUTE_WARN_UNUSED_RET int unsupported_sign_update(struct ec_sign_context * ctx,
		    const u8 *chunk, u32 chunklen);
ATTRIBUTE_WARN_UNUSED_RET int unsupported_sign_finalize(struct ec_sign_context * ctx,
		      u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int is_sign_streaming_mode_supported(ec_alg_type sig_type, int *check);

ATTRIBUTE_WARN_UNUSED_RET int unsupported_verify_init(struct ec_verify_context * ctx,
		    const u8 *sig, u8 siglen);
ATTRIBUTE_WARN_UNUSED_RET int unsupported_verify_update(struct ec_verify_context * ctx,
		      const u8 *chunk, u32 chunklen);
ATTRIBUTE_WARN_UNUSED_RET int unsupported_verify_finalize(struct ec_verify_context * ctx);

ATTRIBUTE_WARN_UNUSED_RET int is_verify_streaming_mode_supported(ec_alg_type sig_type, int *check);

ATTRIBUTE_WARN_UNUSED_RET int is_sign_deterministic(ec_alg_type sig_type, int *check);

ATTRIBUTE_WARN_UNUSED_RET int is_verify_batch_mode_supported(ec_alg_type sig_type, int *check);

ATTRIBUTE_WARN_UNUSED_RET int unsupported_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
              const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
              hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
	      verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len);

/*
 * Each signature algorithm supported by the library and implemented
 * in ec{,ck,s,fs,g,r}dsa.{c,h} is referenced below.
 */
#define MAX_SIG_ALG_NAME_LEN	0
static const ec_sig_mapping ec_sig_maps[] = {
#ifdef WITH_SIG_ECDSA
	{.type = ECDSA,
	 .name = "ECDSA",
	 .siglen = ecdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = ecdsa_init_pub_key,
	 .sign_init = _ecdsa_sign_init,
	 .sign_update = _ecdsa_sign_update,
	 .sign_finalize = _ecdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _ecdsa_verify_init,
	 .verify_update = _ecdsa_verify_update,
	 .verify_finalize = _ecdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 6)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 6
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_ECDSA */
#ifdef WITH_SIG_ECKCDSA
	{.type = ECKCDSA,
	 .name = "ECKCDSA",
	 .siglen = eckcdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = eckcdsa_init_pub_key,
	 .sign_init = _eckcdsa_sign_init,
	 .sign_update = _eckcdsa_sign_update,
	 .sign_finalize = _eckcdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _eckcdsa_verify_init,
	 .verify_update = _eckcdsa_verify_update,
	 .verify_finalize = _eckcdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 8)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 8
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_ECKCDSA */
#ifdef WITH_SIG_ECSDSA
	{.type = ECSDSA,
	 .name = "ECSDSA",
	 .siglen = ecsdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = ecsdsa_init_pub_key,
	 .sign_init = _ecsdsa_sign_init,
	 .sign_update = _ecsdsa_sign_update,
	 .sign_finalize = _ecsdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _ecsdsa_verify_init,
	 .verify_update = _ecsdsa_verify_update,
	 .verify_finalize = _ecsdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 7)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 7
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_ECSDSA */
#ifdef WITH_SIG_ECOSDSA
	{.type = ECOSDSA,
	 .name = "ECOSDSA",
	 .siglen = ecosdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = ecosdsa_init_pub_key,
	 .sign_init = _ecosdsa_sign_init,
	 .sign_update = _ecosdsa_sign_update,
	 .sign_finalize = _ecosdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _ecosdsa_verify_init,
	 .verify_update = _ecosdsa_verify_update,
	 .verify_finalize = _ecosdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 8)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 8
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_ECOSDSA */
#ifdef WITH_SIG_ECFSDSA
	{.type = ECFSDSA,
	 .name = "ECFSDSA",
	 .siglen = ecfsdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = ecfsdsa_init_pub_key,
	 .sign_init = _ecfsdsa_sign_init,
	 .sign_update = _ecfsdsa_sign_update,
	 .sign_finalize = _ecfsdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _ecfsdsa_verify_init,
	 .verify_update = _ecfsdsa_verify_update,
	 .verify_finalize = _ecfsdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = ecfsdsa_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 8)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 8
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_ECFSDSA */
#ifdef WITH_SIG_ECGDSA
	{.type = ECGDSA,
	 .name = "ECGDSA",
	 .siglen = ecgdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = ecgdsa_init_pub_key,
	 .sign_init = _ecgdsa_sign_init,
	 .sign_update = _ecgdsa_sign_update,
	 .sign_finalize = _ecgdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _ecgdsa_verify_init,
	 .verify_update = _ecgdsa_verify_update,
	 .verify_finalize = _ecgdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 7)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 7
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_ECGDSA */
#ifdef WITH_SIG_ECRDSA
	{.type = ECRDSA,
	 .name = "ECRDSA",
	 .siglen = ecrdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = ecrdsa_init_pub_key,
	 .sign_init = _ecrdsa_sign_init,
	 .sign_update = _ecrdsa_sign_update,
	 .sign_finalize = _ecrdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _ecrdsa_verify_init,
	 .verify_update = _ecrdsa_verify_update,
	 .verify_finalize = _ecrdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 7)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 7
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_ECRDSA */
#ifdef WITH_SIG_SM2
	{.type = SM2,
	 .name = "SM2",
	 .siglen = sm2_siglen,
	 .gen_priv_key = sm2_gen_priv_key,
	 .init_pub_key = sm2_init_pub_key,
	 .sign_init = _sm2_sign_init,
	 .sign_update = _sm2_sign_update,
	 .sign_finalize = _sm2_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _sm2_verify_init,
	 .verify_update = _sm2_verify_update,
	 .verify_finalize = _sm2_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 4)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 4
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_SM2 */
#ifdef WITH_SIG_EDDSA25519
	{.type = EDDSA25519,
	 .name = "EDDSA25519",
	 .siglen = eddsa_siglen,
	 .gen_priv_key = eddsa_gen_priv_key,
	 .init_pub_key = eddsa_init_pub_key,
	 /* NOTE: for "pure" EdDSA, streaming mode is not supported */
	 .sign_init = unsupported_sign_init,
	 .sign_update = unsupported_sign_update,
	 .sign_finalize = unsupported_sign_finalize,
	 .sign = _eddsa_sign,
	 .verify_init = _eddsa_verify_init,
	 .verify_update = _eddsa_verify_update,
	 .verify_finalize = _eddsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = eddsa_verify_batch,
	 },
	{.type = EDDSA25519CTX,
	 .name = "EDDSA25519CTX",
	 .siglen = eddsa_siglen,
	 .gen_priv_key = eddsa_gen_priv_key,
	 .init_pub_key = eddsa_init_pub_key,
	 /* NOTE: for "ctx" EdDSA, streaming mode is not supported */
	 .sign_init = unsupported_sign_init,
	 .sign_update = unsupported_sign_update,
	 .sign_finalize = unsupported_sign_finalize,
	 .sign = _eddsa_sign,
	 .verify_init = _eddsa_verify_init,
	 .verify_update = _eddsa_verify_update,
	 .verify_finalize = _eddsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = eddsa_verify_batch,
	 },
	{.type = EDDSA25519PH,
	 .name = "EDDSA25519PH",
	 .siglen = eddsa_siglen,
	 .gen_priv_key = eddsa_gen_priv_key,
	 .init_pub_key = eddsa_init_pub_key,
	 .sign_init = _eddsa_sign_init_pre_hash,
	 .sign_update = _eddsa_sign_update_pre_hash,
	 .sign_finalize = _eddsa_sign_finalize_pre_hash,
	 .sign = _eddsa_sign,
	 .verify_init = _eddsa_verify_init,
	 .verify_update = _eddsa_verify_update,
	 .verify_finalize = _eddsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = eddsa_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 14)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 14
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_EDDSA25519 */
#ifdef WITH_SIG_EDDSA448
	{.type = EDDSA448,
	 .name = "EDDSA448",
	 .siglen = eddsa_siglen,
	 .gen_priv_key = eddsa_gen_priv_key,
	 .init_pub_key = eddsa_init_pub_key,
	 /* NOTE: for "pure" EdDSA, streaming mode is not supported */
	 .sign_init = unsupported_sign_init,
	 .sign_update = unsupported_sign_update,
	 .sign_finalize = unsupported_sign_finalize,
	 .sign = _eddsa_sign,
	 .verify_init = _eddsa_verify_init,
	 .verify_update = _eddsa_verify_update,
	 .verify_finalize = _eddsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = eddsa_verify_batch,
	 },
	{.type = EDDSA448PH,
	 .name = "EDDSA448PH",
	 .siglen = eddsa_siglen,
	 .gen_priv_key = eddsa_gen_priv_key,
	 .init_pub_key = eddsa_init_pub_key,
	 .sign_init = _eddsa_sign_init_pre_hash,
	 .sign_update = _eddsa_sign_update_pre_hash,
	 .sign_finalize = _eddsa_sign_finalize_pre_hash,
	 .sign = _eddsa_sign,
	 .verify_init = _eddsa_verify_init,
	 .verify_update = _eddsa_verify_update,
	 .verify_finalize = _eddsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = eddsa_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 11)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 11
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_EDDSA448 */
#ifdef WITH_SIG_DECDSA
	{.type = DECDSA,
	 .name = "DECDSA",
	 .siglen = decdsa_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = decdsa_init_pub_key,
	 .sign_init = _decdsa_sign_init,
	 .sign_update = _decdsa_sign_update,
	 .sign_finalize = _decdsa_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _decdsa_verify_init,
	 .verify_update = _decdsa_verify_update,
	 .verify_finalize = _decdsa_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 7)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 7
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_DECDSA */
#ifdef WITH_SIG_BIGN
	{.type = BIGN,
	 .name = "BIGN",
	 .siglen = bign_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = bign_init_pub_key,
	 .sign_init = _bign_sign_init,
	 .sign_update = _bign_sign_update,
	 .sign_finalize = _bign_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _bign_verify_init,
	 .verify_update = _bign_verify_update,
	 .verify_finalize = _bign_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 5)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 5
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_BIGN */
#ifdef WITH_SIG_DBIGN
	{.type = DBIGN,
	 .name = "DBIGN",
	 .siglen = dbign_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = dbign_init_pub_key,
	 .sign_init = _dbign_sign_init,
	 .sign_update = _dbign_sign_update,
	 .sign_finalize = _dbign_sign_finalize,
	 .sign = generic_ec_sign,
	 .verify_init = _dbign_verify_init,
	 .verify_update = _dbign_verify_update,
	 .verify_finalize = _dbign_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = unsupported_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 6)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 6
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_DBIGN */
#ifdef WITH_SIG_BIP0340
	{.type = BIP0340,
	 .name = "BIP0340",
	 .siglen = bip0340_siglen,
	 .gen_priv_key = generic_gen_priv_key,
	 .init_pub_key = bip0340_init_pub_key,
	 .sign_init = unsupported_sign_init,
	 .sign_update = unsupported_sign_update,
	 .sign_finalize = unsupported_sign_finalize,
	 .sign = _bip0340_sign,
	 .verify_init = _bip0340_verify_init,
	 .verify_update = _bip0340_verify_update,
	 .verify_finalize = _bip0340_verify_finalize,
	 .verify = generic_ec_verify,
	 .verify_batch = bip0340_verify_batch,
	 },
#if (MAX_SIG_ALG_NAME_LEN < 8)
#undef MAX_SIG_ALG_NAME_LEN
#define MAX_SIG_ALG_NAME_LEN 8
#endif /* MAX_SIG_ALG_NAME_LEN */
#endif /* WITH_SIG_BIP0340 */
	{.type = UNKNOWN_ALG,	/* Needs to be kept last */
	 .name = "UNKNOWN",
	 .siglen = 0,
	 .gen_priv_key = NULL,
	 .init_pub_key = NULL,
	 .sign_init = NULL,
	 .sign_update = NULL,
	 .sign_finalize = NULL,
	 .sign = NULL,
	 .verify_init = NULL,
	 .verify_update = NULL,
	 .verify_finalize = NULL,
	 .verify = NULL,
	 .verify_batch = NULL,
	 },
};

/*
 * For a given raw signature, the structured version is produced by prepending
 * three bytes providing specific sig alg, hash alg and curve.
 */
#define EC_STRUCTURED_SIG_EXPORT_SIZE(siglen)  (u8)((siglen) + (u8)(3 * sizeof(u8)))
#define EC_STRUCTURED_SIG_MAX_EXPORT_SIZE (EC_MAX_SIGLEN + 3)

/* Sanity check */
#if EC_STRUCTURED_SIG_MAX_EXPORT_SIZE > 255
#error "All structured signatures sizes are expected to fit on an u8."
#endif
#endif /* __SIG_ALGS_INTERNAL_H__ */
