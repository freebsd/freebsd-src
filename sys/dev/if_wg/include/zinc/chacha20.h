/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef _ZINC_CHACHA20_H
#define _ZINC_CHACHA20_H

#include <sys/param.h>
#include <sys/support.h>

enum chacha20_lengths {
	CHACHA20_NONCE_SIZE = 16,
	CHACHA20_KEY_SIZE = 32,
	CHACHA20_KEY_WORDS = CHACHA20_KEY_SIZE / sizeof(u32),
	CHACHA20_BLOCK_SIZE = 64,
	CHACHA20_BLOCK_WORDS = CHACHA20_BLOCK_SIZE / sizeof(u32),
	HCHACHA20_NONCE_SIZE = CHACHA20_NONCE_SIZE,
	HCHACHA20_KEY_SIZE = CHACHA20_KEY_SIZE
};

enum chacha20_constants { /* expand 32-byte k */
	CHACHA20_CONSTANT_EXPA = 0x61707865U,
	CHACHA20_CONSTANT_ND_3 = 0x3320646eU,
	CHACHA20_CONSTANT_2_BY = 0x79622d32U,
	CHACHA20_CONSTANT_TE_K = 0x6b206574U
};

struct chacha20_ctx {
	union {
		u32 state[16];
		struct {
			u32 constant[4];
			u32 key[8];
			u32 counter[4];
		};
	};
};

static inline void chacha20_init(struct chacha20_ctx *ctx,
				 const u8 key[CHACHA20_KEY_SIZE],
				 const u64 nonce)
{
	ctx->constant[0] = CHACHA20_CONSTANT_EXPA;
	ctx->constant[1] = CHACHA20_CONSTANT_ND_3;
	ctx->constant[2] = CHACHA20_CONSTANT_2_BY;
	ctx->constant[3] = CHACHA20_CONSTANT_TE_K;
	ctx->key[0] = get_unaligned_le32(key + 0);
	ctx->key[1] = get_unaligned_le32(key + 4);
	ctx->key[2] = get_unaligned_le32(key + 8);
	ctx->key[3] = get_unaligned_le32(key + 12);
	ctx->key[4] = get_unaligned_le32(key + 16);
	ctx->key[5] = get_unaligned_le32(key + 20);
	ctx->key[6] = get_unaligned_le32(key + 24);
	ctx->key[7] = get_unaligned_le32(key + 28);
	ctx->counter[0] = 0;
	ctx->counter[1] = 0;
	ctx->counter[2] = nonce & U32_MAX;
	ctx->counter[3] = nonce >> 32;
}
void chacha20(struct chacha20_ctx *ctx, u8 *dst, const u8 *src, u32 len,
	      simd_context_t *simd_context);

void hchacha20(u32 derived_key[CHACHA20_KEY_WORDS],
	       const u8 nonce[HCHACHA20_NONCE_SIZE],
	       const u8 key[HCHACHA20_KEY_SIZE], simd_context_t *simd_context);

#endif /* _ZINC_CHACHA20_H */
