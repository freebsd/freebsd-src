/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/lib_ecc_config.h>
#ifdef WITH_HASH_RIPEMD160

#ifndef __RIPEMD160_H__
#define __RIPEMD160_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>

#define RIPEMD160_STATE_SIZE   5
#define RIPEMD160_BLOCK_SIZE   64
#define RIPEMD160_DIGEST_SIZE  20
#define RIPEMD160_DIGEST_SIZE_BITS  160

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE	0
#endif
#if (MAX_DIGEST_SIZE < RIPEMD160_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE RIPEMD160_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS	0
#endif
#if (MAX_DIGEST_SIZE_BITS < RIPEMD160_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS RIPEMD160_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < RIPEMD160_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE RIPEMD160_BLOCK_SIZE
#endif

#define RIPEMD160_HASH_MAGIC ((word_t)(0x7392018463926719ULL))
#define RIPEMD160_HASH_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == RIPEMD160_HASH_MAGIC), ret, err)

typedef struct {
	/* Number of bytes processed */
	u64 ripemd160_total;
	/* Internal state */
	u32 ripemd160_state[RIPEMD160_STATE_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 ripemd160_buffer[RIPEMD160_BLOCK_SIZE];
	/* Initialization magic value */
	word_t magic;
} ripemd160_context;

ATTRIBUTE_WARN_UNUSED_RET int ripemd160_init(ripemd160_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int ripemd160_update(ripemd160_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int ripemd160_final(ripemd160_context *ctx, u8 output[RIPEMD160_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int ripemd160_scattered(const u8 **inputs, const u32 *ilens,
		     u8 output[RIPEMD160_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int ripemd160(const u8 *input, u32 ilen, u8 output[RIPEMD160_DIGEST_SIZE]);

#endif /* __RIPEMD160_H__ */
#endif /* WITH_HASH_RIPEMD160 */
