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
#ifdef WITH_HASH_BASH224

#ifndef __BASH224_H__
#define __BASH224_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/bash.h>

#define BASH224_BLOCK_SIZE   136
#define BASH224_DIGEST_SIZE  28
#define BASH224_DIGEST_SIZE_BITS  224

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE	0
#endif
#if (MAX_DIGEST_SIZE < BASH224_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE BASH224_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < BASH224_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS BASH224_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < BASH224_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE BASH224_BLOCK_SIZE
#endif

#define BASH224_HASH_MAGIC ((word_t)(0xaf3456ff1200ba5bULL))
#define BASH224_HASH_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == BASH224_HASH_MAGIC), ret, err)

typedef bash_context bash224_context;

ATTRIBUTE_WARN_UNUSED_RET int bash224_init(bash224_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int bash224_update(bash224_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int bash224_final(bash224_context *ctx, u8 output[BASH224_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int bash224_scattered(const u8 **inputs, const u32 *ilens,
		       u8 output[BASH224_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int bash224(const u8 *input, u32 ilen, u8 output[BASH224_DIGEST_SIZE]);

#endif /* __BASH224_H__ */
#endif /* WITH_HASH_BASH224 */
