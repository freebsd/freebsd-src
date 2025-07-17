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
#ifndef __HASH_HASH_H__
#define __HASH_HASH_H__


/*
 * NOTE: we include libsig for the libecc
 * hash algorithms.
 */
#include <libecc/libec.h>

/* MD-2 */
#include "md2.h"
/* MD-4 */
#include "md4.h"
/* MD-5 */
#include "md5.h"
/* SHA-0 */
#include "sha0.h"
/* SHA-1 */
#include "sha1.h"
/* MDC-2 */
#include "mdc2.h"
/* GOSTR34-11-94 source code */
#include "gostr34_11_94.h"

/****************************************************/
/****************************************************/
/****************************************************/
typedef enum {
	/* libecc native hashes: we map our enum on them */
	HASH_UNKNOWN_HASH_ALG = UNKNOWN_HASH_ALG,
	HASH_SHA224 = SHA224,
	HASH_SHA256 = SHA256,
	HASH_SHA384 = SHA384,
	HASH_SHA512 = SHA512,
	HASH_SHA512_224 = SHA512_224,
	HASH_SHA512_256 = SHA512_256,
	HASH_SHA3_224 = SHA3_224,
	HASH_SHA3_256 = SHA3_256,
	HASH_SHA3_384 = SHA3_384,
	HASH_SHA3_512 = SHA3_512,
	HASH_SM3 = SM3,
	HASH_STREEBOG256 = STREEBOG256,
	HASH_STREEBOG512 = STREEBOG512,
	HASH_SHAKE256 = SHAKE256,
	HASH_RIPEMD160 = RIPEMD160,
	HASH_BELT_HASH = BELT_HASH,
	HASH_BASH224 = BASH224,
	HASH_BASH256 = BASH256,
	HASH_BASH384 = BASH384,
	HASH_BASH512 = BASH512,
	/* Deprecated hash algorithms not supported by libecc
	 * (for security reasons).
	 * XXX: NOTE: These algorithms are here as a playground e.g.
	 * to test some backward compatibility of cryptographic cipher suites,
	 * please DO NOT use them in production code!
	 */
	HASH_MD2,
	HASH_MD4,
	HASH_MD5,
	HASH_SHA0,
	HASH_SHA1,
	HASH_MDC2_PADDING1,
	HASH_MDC2_PADDING2,
	HASH_GOST34_11_94_NORM,
	HASH_GOST34_11_94_RFC4357,
} gen_hash_alg_type;

/* Our generic hash context */
typedef union {
	/* libecc native hashes */
	hash_context hctx;
	/* MD2 */
	md2_context md2ctx;
	/* MD4 */
	md4_context md4ctx;
	/* MD5 */
	md5_context md5ctx;
	/* SHA-0 */
	sha0_context sha0ctx;
	/* SHA-1 */
	sha1_context sha1ctx;
	/* MDC2 */
	mdc2_context mdc2ctx;
	/* GOSTR34-11-94 */
	gostr34_11_94_context gostr34_11_94ctx;
} gen_hash_context;

ATTRIBUTE_WARN_UNUSED_RET int gen_hash_get_hash_sizes(gen_hash_alg_type gen_hash_type, u8 *hlen, u8 *block_size);
ATTRIBUTE_WARN_UNUSED_RET int gen_hash_init(gen_hash_context *ctx, gen_hash_alg_type gen_hash_type);
ATTRIBUTE_WARN_UNUSED_RET int gen_hash_update(gen_hash_context *ctx, const u8 *chunk, u32 chunklen, gen_hash_alg_type gen_hash_type);
ATTRIBUTE_WARN_UNUSED_RET int gen_hash_final(gen_hash_context *ctx, u8 *output, gen_hash_alg_type gen_hash_type);
ATTRIBUTE_WARN_UNUSED_RET int gen_hash_hfunc(const u8 *input, u32 ilen, u8 *digest, gen_hash_alg_type gen_hash_type);
ATTRIBUTE_WARN_UNUSED_RET int gen_hash_hfunc_scattered(const u8 **input, const u32 *ilen, u8 *digest, gen_hash_alg_type gen_hash_type);

#endif /* __HASH_HASH_H__ */
