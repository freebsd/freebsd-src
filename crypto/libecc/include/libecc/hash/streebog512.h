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
#ifdef WITH_HASH_STREEBOG512

#ifndef __STREEBOG512_H__
#define __STREEBOG512_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/streebog.h>

#define STREEBOG512_BLOCK_SIZE   STREEBOG_BLOCK_SIZE
#define STREEBOG512_DIGEST_SIZE  64
#define STREEBOG512_DIGEST_SIZE_BITS  512

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE	0
#endif
#if (MAX_DIGEST_SIZE < STREEBOG512_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE STREEBOG512_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < STREEBOG512_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS STREEBOG512_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < STREEBOG512_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE STREEBOG512_BLOCK_SIZE
#endif

#define STREEBOG512_HASH_MAGIC ((word_t)(0x3293187509128364ULL))
#define STREEBOG512_HASH_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == STREEBOG512_HASH_MAGIC) && \
                  ((A)->streebog_digest_size == STREEBOG512_DIGEST_SIZE) && ((A)->streebog_block_size == STREEBOG512_BLOCK_SIZE), ret, err)

typedef streebog_context streebog512_context;

ATTRIBUTE_WARN_UNUSED_RET int streebog512_init(streebog512_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int streebog512_update(streebog512_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int streebog512_final(streebog512_context *ctx, u8 output[STREEBOG512_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int streebog512_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[STREEBOG512_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int streebog512(const u8 *input, u32 ilen, u8 output[STREEBOG512_DIGEST_SIZE]);

#endif /* __STREEBOG512_H__ */
#endif /* WITH_HASH_STREEBOG512 */
