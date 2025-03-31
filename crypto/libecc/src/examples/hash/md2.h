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
#ifndef __MD2_H__
#define __MD2_H__

/* Include libec for useful types and macros */
#include <libecc/libec.h>

#define MD2_STATE_SIZE   16
#define MD2_BLOCK_SIZE   16
#define MD2_DIGEST_SIZE  16
#define MD2_DIGEST_SIZE_BITS  128

#define MD2_HASH_MAGIC ((word_t)(0x8432927137264770ULL))
#define MD2_HASH_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == MD2_HASH_MAGIC), ret, err)

typedef struct {
	/* Number of bytes processed */
	u64 md2_total;
	/* Internal state */
	u8 md2_state[MD2_STATE_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 md2_buffer[MD2_BLOCK_SIZE];
	/* Internal buffer to hold the checksum */
	u8 md2_checksum[MD2_BLOCK_SIZE];
	/* Initialization magic value */
	word_t magic;
} md2_context;


/* Init hash function. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET int md2_init(md2_context *ctx);

ATTRIBUTE_WARN_UNUSED_RET int md2_update(md2_context *ctx, const u8 *input, u32 ilen);

/* Finalize. Returns 0 on success, -1 on error.*/
ATTRIBUTE_WARN_UNUSED_RET int md2_final(md2_context *ctx, u8 output[MD2_DIGEST_SIZE]);

/*
 * Scattered version performing init/update/finalize on a vector of buffers
 * 'inputs' with the length of each buffer passed via 'ilens'. The function
 * loops on pointers in 'inputs' until it finds a NULL pointer. The function
 * returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int md2_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[MD2_DIGEST_SIZE]);

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int md2(const u8 *input, u32 ilen, u8 output[MD2_DIGEST_SIZE]);

#endif /* __MD2_H__ */
