// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <asm/hwcap.h>
#include <asm/neon.h>

asmlinkage void poly1305_init_arm(void *ctx, const u8 key[16]);
asmlinkage void poly1305_blocks_arm(void *ctx, const u8 *inp, const size_t len,
				    const u32 padbit);
asmlinkage void poly1305_emit_arm(void *ctx, u8 mac[16], const u32 nonce[4]);
asmlinkage void poly1305_blocks_neon(void *ctx, const u8 *inp, const size_t len,
				     const u32 padbit);
asmlinkage void poly1305_emit_neon(void *ctx, u8 mac[16], const u32 nonce[4]);

static bool poly1305_use_neon __ro_after_init;
static bool *const poly1305_nobs[] __initconst = { &poly1305_use_neon };

static void __init poly1305_fpu_init(void)
{
#if defined(CONFIG_ZINC_ARCH_ARM64)
	poly1305_use_neon = cpu_have_named_feature(ASIMD);
#elif defined(CONFIG_ZINC_ARCH_ARM)
	poly1305_use_neon = elf_hwcap & HWCAP_NEON;
#endif
}

#if defined(CONFIG_ZINC_ARCH_ARM64)
struct poly1305_arch_internal {
	union {
		u32 h[5];
		struct {
			u64 h0, h1, h2;
		};
	};
	u64 is_base2_26;
	u64 r[2];
};
#elif defined(CONFIG_ZINC_ARCH_ARM)
struct poly1305_arch_internal {
	union {
		u32 h[5];
		struct {
			u64 h0, h1;
			u32 h2;
		} __packed;
	};
	u32 r[4];
	u32 is_base2_26;
};
#endif

/* The NEON code uses base 2^26, while the scalar code uses base 2^64 on 64-bit
 * and base 2^32 on 32-bit. If we hit the unfortunate situation of using NEON
 * and then having to go back to scalar -- because the user is silly and has
 * called the update function from two separate contexts -- then we need to
 * convert back to the original base before proceeding. The below function is
 * written for 64-bit integers, and so we have to swap words at the end on
 * big-endian 32-bit. It is possible to reason that the initial reduction below
 * is sufficient given the implementation invariants. However, for an avoidance
 * of doubt and because this is not performance critical, we do the full
 * reduction anyway.
 */
static void convert_to_base2_64(void *ctx)
{
	struct poly1305_arch_internal *state = ctx;
	u32 cy;

	if (!IS_ENABLED(CONFIG_KERNEL_MODE_NEON) || !state->is_base2_26)
		return;

	cy = state->h[0] >> 26; state->h[0] &= 0x3ffffff; state->h[1] += cy;
	cy = state->h[1] >> 26; state->h[1] &= 0x3ffffff; state->h[2] += cy;
	cy = state->h[2] >> 26; state->h[2] &= 0x3ffffff; state->h[3] += cy;
	cy = state->h[3] >> 26; state->h[3] &= 0x3ffffff; state->h[4] += cy;
	state->h0 = ((u64)state->h[2] << 52) | ((u64)state->h[1] << 26) | state->h[0];
	state->h1 = ((u64)state->h[4] << 40) | ((u64)state->h[3] << 14) | (state->h[2] >> 12);
	state->h2 = state->h[4] >> 24;
	if (IS_ENABLED(CONFIG_ZINC_ARCH_ARM) && IS_ENABLED(CONFIG_CPU_BIG_ENDIAN)) {
		state->h0 = rol64(state->h0, 32);
		state->h1 = rol64(state->h1, 32);
	}
#define ULT(a, b) ((a ^ ((a ^ b) | ((a - b) ^ b))) >> (sizeof(a) * 8 - 1))
	cy = (state->h2 >> 2) + (state->h2 & ~3ULL);
	state->h2 &= 3;
	state->h0 += cy;
	state->h1 += (cy = ULT(state->h0, cy));
	state->h2 += ULT(state->h1, cy);
#undef ULT
	state->is_base2_26 = 0;
}

static inline bool poly1305_init_arch(void *ctx,
				      const u8 key[POLY1305_KEY_SIZE])
{
	poly1305_init_arm(ctx, key);
	return true;
}

static inline bool poly1305_blocks_arch(void *ctx, const u8 *inp,
					size_t len, const u32 padbit,
					simd_context_t *simd_context)
{
	/* SIMD disables preemption, so relax after processing each page. */
	BUILD_BUG_ON(PAGE_SIZE < POLY1305_BLOCK_SIZE ||
		     PAGE_SIZE % POLY1305_BLOCK_SIZE);

	if (!IS_ENABLED(CONFIG_KERNEL_MODE_NEON) || !poly1305_use_neon ||
	    !simd_use(simd_context)) {
		convert_to_base2_64(ctx);
		poly1305_blocks_arm(ctx, inp, len, padbit);
		return true;
	}

	for (;;) {
		const size_t bytes = min_t(size_t, len, PAGE_SIZE);

		poly1305_blocks_neon(ctx, inp, bytes, padbit);
		len -= bytes;
		if (!len)
			break;
		inp += bytes;
		simd_relax(simd_context);
	}
	return true;
}

static inline bool poly1305_emit_arch(void *ctx, u8 mac[POLY1305_MAC_SIZE],
				      const u32 nonce[4],
				      simd_context_t *simd_context)
{
	if (!IS_ENABLED(CONFIG_KERNEL_MODE_NEON) || !poly1305_use_neon ||
	    !simd_use(simd_context)) {
		convert_to_base2_64(ctx);
		poly1305_emit_arm(ctx, mac, nonce);
	} else
		poly1305_emit_neon(ctx, mac, nonce);
	return true;
}
