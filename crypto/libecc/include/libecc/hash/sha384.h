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
#ifdef WITH_HASH_SHA384

#ifndef __SHA384_H__
#define __SHA384_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include "sha2.h"

#define SHA384_STATE_SIZE   8
#define SHA384_BLOCK_SIZE   128
#define SHA384_DIGEST_SIZE  48
#define SHA384_DIGEST_SIZE_BITS  384

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE 0
#endif
#if (MAX_DIGEST_SIZE < SHA384_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE SHA384_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < SHA384_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS SHA384_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < SHA384_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE SHA384_BLOCK_SIZE
#endif

#define SHA384_HASH_MAGIC ((word_t)(0x9227239b32098412ULL))
#define SHA384_HASH_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == SHA384_HASH_MAGIC), ret, err)

typedef struct {
	/* Number of bytes processed on 128 bits */
	u64 sha384_total[2];
	/* Internal state */
	u64 sha384_state[SHA384_STATE_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 sha384_buffer[SHA384_BLOCK_SIZE];
	/* Initialization magic value */
	word_t magic;
} sha384_context;

ATTRIBUTE_WARN_UNUSED_RET int sha384_init(sha384_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int sha384_update(sha384_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int sha384_final(sha384_context *ctx, u8 output[SHA384_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha384_scattered(const u8 **inputs, const u32 *ilens,
		     u8 output[SHA384_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha384(const u8 *input, u32 ilen, u8 output[SHA384_DIGEST_SIZE]);

#endif /* __SHA384_H__ */
#endif /* WITH_HASH_SHA384 */
