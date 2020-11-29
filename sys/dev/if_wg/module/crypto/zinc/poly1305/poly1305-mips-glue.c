// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

asmlinkage void poly1305_init_mips(void *ctx, const u8 key[16]);
asmlinkage void poly1305_blocks_mips(void *ctx, const u8 *inp, const size_t len,
				     const u32 padbit);
asmlinkage void poly1305_emit_mips(void *ctx, u8 mac[16], const u32 nonce[4]);

static bool *const poly1305_nobs[] __initconst = { };
static void __init poly1305_fpu_init(void)
{
}

static inline bool poly1305_init_arch(void *ctx,
				      const u8 key[POLY1305_KEY_SIZE])
{
	poly1305_init_mips(ctx, key);
	return true;
}

static inline bool poly1305_blocks_arch(void *ctx, const u8 *inp,
					size_t len, const u32 padbit,
					simd_context_t *simd_context)
{
	poly1305_blocks_mips(ctx, inp, len, padbit);
	return true;
}

static inline bool poly1305_emit_arch(void *ctx, u8 mac[POLY1305_MAC_SIZE],
				      const u32 nonce[4],
				      simd_context_t *simd_context)
{
	poly1305_emit_mips(ctx, mac, nonce);
	return true;
}
