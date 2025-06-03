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
#ifdef WITH_HASH_SM3

#ifndef __SM3_H__
#define __SM3_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>

#define SM3_STATE_SIZE    8 /* in 32 bits word */
#define SM3_BLOCK_SIZE   64
#define SM3_DIGEST_SIZE  32
#define SM3_DIGEST_SIZE_BITS  256

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE	0
#endif
#if (MAX_DIGEST_SIZE < SM3_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE SM3_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < SM3_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS SM3_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < SM3_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE SM3_BLOCK_SIZE
#endif

#define SM3_HASH_MAGIC ((word_t)(0x2947510312849204ULL))
#define SM3_HASH_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == SM3_HASH_MAGIC), ret, err)

typedef struct {
	/* Number of bytes processed */
	u64 sm3_total;
	/* Internal state */
	u32 sm3_state[SM3_STATE_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 sm3_buffer[SM3_BLOCK_SIZE];
        /* Initialization magic value */
        word_t magic;
} sm3_context;

ATTRIBUTE_WARN_UNUSED_RET int sm3_init(sm3_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int sm3_update(sm3_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int sm3_final(sm3_context *ctx, u8 output[SM3_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sm3_scattered(const u8 **inputs, const u32 *ilens,
		   u8 output[SM3_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sm3(const u8 *input, u32 ilen, u8 output[SM3_DIGEST_SIZE]);

#endif /* __SM3_H__ */
#endif /* WITH_HASH_SM3 */
