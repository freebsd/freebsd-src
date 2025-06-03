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
#ifdef WITH_HASH_SHA512

#ifndef __SHA512_H__
#define __SHA512_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/sha2.h>
#include <libecc/hash/sha512_core.h>

#define SHA512_STATE_SIZE   SHA512_CORE_STATE_SIZE
#define SHA512_BLOCK_SIZE   SHA512_CORE_BLOCK_SIZE
#define SHA512_DIGEST_SIZE  SHA512_CORE_DIGEST_SIZE
#define SHA512_DIGEST_SIZE_BITS  512

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE 0
#endif
#if (MAX_DIGEST_SIZE < SHA512_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE SHA512_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < SHA512_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS SHA512_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE	0
#endif
#if (MAX_BLOCK_SIZE < SHA512_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE SHA512_BLOCK_SIZE
#endif

#define SHA512_HASH_MAGIC ((word_t)(0x5539012b32097312ULL))
#define SHA512_HASH_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == SHA512_HASH_MAGIC), ret, err)

typedef sha512_core_context sha512_context;

ATTRIBUTE_WARN_UNUSED_RET int sha512_init(sha512_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int sha512_update(sha512_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int sha512_final(sha512_context *ctx, u8 output[SHA512_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha512_scattered(const u8 **inputs, const u32 *ilens,
		     u8 output[SHA512_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha512(const u8 *input, u32 ilen, u8 output[SHA512_DIGEST_SIZE]);

#endif /* __SHA512_H__ */
#endif /* WITH_HASH_SHA512 */
