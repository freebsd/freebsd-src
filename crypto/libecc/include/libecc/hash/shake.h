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
#ifndef __SHAKE_H__
#define __SHAKE_H__

#include <libecc/hash/keccak.h>

typedef enum {
	SHAKE_LITTLE = 0,
	SHAKE_BIG = 1,
} shake_endianness;
/*
 * Generic context for all SHAKE instances. Only difference is digest size
 * value, initialized in init() call and used in finalize().
 */
typedef struct shake_context_ {
	u8 shake_digest_size;
	u8 shake_block_size;
	shake_endianness shake_endian;
	/* Local index, useful for the absorbing phase */
	u64 shake_idx;
	/* Keccak's state, viewed as a bi-dimensional array */
	u64 shake_state[KECCAK_SLICES * KECCAK_SLICES];
	/* Initialization magic value */
	word_t magic;
} shake_context;


ATTRIBUTE_WARN_UNUSED_RET int _shake_init(shake_context *ctx, u8 digest_size, u8 block_size);
ATTRIBUTE_WARN_UNUSED_RET int _shake_update(shake_context *ctx, const u8 *buf, u32 buflen);
ATTRIBUTE_WARN_UNUSED_RET int _shake_finalize(shake_context *ctx, u8 *output);

#endif /* __SHAKE_H__ */
