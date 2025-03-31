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
#ifndef __SHA3_H__
#define __SHA3_H__

#include <libecc/hash/keccak.h>

typedef enum {
	SHA3_LITTLE = 0,
	SHA3_BIG = 1,
} sha3_endianness;
/*
 * Generic context for all SHA3 instances. Only difference is digest size
 * value, initialized in init() call and used in finalize().
 */
typedef struct sha3_context_ {
	u8 sha3_digest_size;
	u8 sha3_block_size;
	sha3_endianness sha3_endian;
	/* Local index, useful for the absorbing phase */
	u64 sha3_idx;
	/* Keccak's state, viewed as a bi-dimensional array */
	u64 sha3_state[KECCAK_SLICES * KECCAK_SLICES];
	/* Initialization magic value */
	word_t magic;
} sha3_context;


ATTRIBUTE_WARN_UNUSED_RET int _sha3_init(sha3_context *ctx, u8 digest_size);
ATTRIBUTE_WARN_UNUSED_RET int _sha3_update(sha3_context *ctx, const u8 *buf, u32 buflen);
ATTRIBUTE_WARN_UNUSED_RET int _sha3_finalize(sha3_context *ctx, u8 *output);

#endif /* __SHA3_H__ */
