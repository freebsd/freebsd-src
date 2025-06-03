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
#ifdef WITH_HASH_SHA3_384

#ifndef __SHA3_384_H__
#define __SHA3_384_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/sha3.h>

#define SHA3_384_BLOCK_SIZE   104
#define SHA3_384_DIGEST_SIZE  48
#define SHA3_384_DIGEST_SIZE_BITS  384

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE	0
#endif
#if (MAX_DIGEST_SIZE < SHA3_384_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE SHA3_384_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < SHA3_384_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS SHA3_384_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < SHA3_384_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE SHA3_384_BLOCK_SIZE
#endif

#define SHA3_384_HASH_MAGIC ((word_t)(0x2233223273935643ULL))
#define SHA3_384_HASH_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == SHA3_384_HASH_MAGIC), ret, err)

typedef sha3_context sha3_384_context;

ATTRIBUTE_WARN_UNUSED_RET int sha3_384_init(sha3_384_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int sha3_384_update(sha3_384_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int sha3_384_final(sha3_384_context *ctx, u8 output[SHA3_384_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha3_384_scattered(const u8 **inputs, const u32 *ilens,
		       u8 output[SHA3_384_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha3_384(const u8 *input, u32 ilen, u8 output[SHA3_384_DIGEST_SIZE]);

#endif /* __SHA3_384_H__ */
#endif /* WITH_HASH_SHA3_384 */
