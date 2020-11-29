/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef _ZINC_POLY1305_H
#define _ZINC_POLY1305_H


enum poly1305_lengths {
	POLY1305_BLOCK_SIZE = 16,
	POLY1305_KEY_SIZE = 32,
	POLY1305_MAC_SIZE = 16
};

struct poly1305_ctx {
	u8 opaque[24 * sizeof(u64)];
	u32 nonce[4];
	u8 data[POLY1305_BLOCK_SIZE];
	size_t num;
} __aligned(8);

void poly1305_init(struct poly1305_ctx *ctx, const u8 key[POLY1305_KEY_SIZE]);
void poly1305_update(struct poly1305_ctx *ctx, const u8 *input, size_t len,
		     simd_context_t *simd_context);
void poly1305_final(struct poly1305_ctx *ctx, u8 mac[POLY1305_MAC_SIZE],
		    simd_context_t *simd_context);

#endif /* _ZINC_POLY1305_H */
