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
#ifdef WITH_HASH_SHA3_512

#ifndef __SHA3_512_H__
#define __SHA3_512_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include "sha3.h"

#define SHA3_512_BLOCK_SIZE   72
#define SHA3_512_DIGEST_SIZE  64
#define SHA3_512_DIGEST_SIZE_BITS  512

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE	0
#endif
#if (MAX_DIGEST_SIZE < SHA3_512_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE SHA3_512_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < SHA3_512_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS SHA3_512_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < SHA3_512_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE SHA3_512_BLOCK_SIZE
#endif

#define SHA3_512_HASH_MAGIC ((word_t)(0x9104729373982346ULL))
#define SHA3_512_HASH_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == SHA3_512_HASH_MAGIC), ret, err)

typedef sha3_context sha3_512_context;

ATTRIBUTE_WARN_UNUSED_RET int sha3_512_init(sha3_512_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int sha3_512_update(sha3_512_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int sha3_512_final(sha3_512_context *ctx, u8 output[SHA3_512_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha3_512_scattered(const u8 **inputs, const u32 *ilens,
		       u8 output[SHA3_512_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int sha3_512(const u8 *input, u32 ilen, u8 output[SHA3_512_DIGEST_SIZE]);

#endif /* __SHA3_512_H__ */
#endif /* WITH_HASH_SHA3_512 */
