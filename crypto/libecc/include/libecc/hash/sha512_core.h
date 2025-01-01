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
#if defined(WITH_HASH_SHA512) || defined(WITH_HASH_SHA512_224) || defined(WITH_HASH_SHA512_256)

#ifndef __SHA512_CORE_H__
#define __SHA512_CORE_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/sha2.h>

#define SHA512_CORE_STATE_SIZE   8
#define SHA512_CORE_BLOCK_SIZE   128
#define SHA512_CORE_DIGEST_SIZE  64

typedef struct {
	/* Number of bytes processed on 128 bits */
	u64 sha512_total[2];
	/* Internal state */
	u64 sha512_state[SHA512_CORE_STATE_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 sha512_buffer[SHA512_CORE_BLOCK_SIZE];
	/* Initialization magic value */
	word_t magic;
} sha512_core_context;


ATTRIBUTE_WARN_UNUSED_RET int sha512_core_update(sha512_core_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int sha512_core_final(sha512_core_context *ctx, u8 *output, u32 output_size);

#endif /* __SHA512_CORE_H__ */
#endif /* WITH_HASH_SHA512 */
