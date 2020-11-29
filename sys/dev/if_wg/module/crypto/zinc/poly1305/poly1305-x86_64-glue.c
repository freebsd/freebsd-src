// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifdef __linux__
#include <asm/cpufeature.h>
#include <asm/processor.h>
#include <asm/intel-family.h>
#else
#include <sys/simd-x86_64.h>
#endif

asmlinkage void poly1305_init_x86_64(void *ctx,
				     const u8 key[POLY1305_KEY_SIZE]);
asmlinkage void poly1305_blocks_x86_64(void *ctx, const u8 *inp,
				       const size_t len, const u32 padbit);
asmlinkage void poly1305_emit_x86_64(void *ctx, u8 mac[POLY1305_MAC_SIZE],
				     const u32 nonce[4]);
asmlinkage void poly1305_emit_avx(void *ctx, u8 mac[POLY1305_MAC_SIZE],
				  const u32 nonce[4]);
asmlinkage void poly1305_blocks_avx(void *ctx, const u8 *inp, const size_t len,
				    const u32 padbit);
asmlinkage void poly1305_blocks_avx2(void *ctx, const u8 *inp, const size_t len,
				     const u32 padbit);
asmlinkage void poly1305_blocks_avx512(void *ctx, const u8 *inp,
				       const size_t len, const u32 padbit);

static bool poly1305_use_avx __ro_after_init;
static bool poly1305_use_avx2 __ro_after_init;
static bool poly1305_use_avx512 __ro_after_init;
static bool *const poly1305_nobs[] __initconst = {
	&poly1305_use_avx, &poly1305_use_avx2, &poly1305_use_avx512 };

static void __init poly1305_fpu_init(void)
{
#ifdef __linux__
	poly1305_use_avx =
		boot_cpu_has(X86_FEATURE_AVX) &&
		cpu_has_xfeatures(XFEATURE_MASK_SSE | XFEATURE_MASK_YMM, NULL);
	poly1305_use_avx2 =
		boot_cpu_has(X86_FEATURE_AVX) &&
		boot_cpu_has(X86_FEATURE_AVX2) &&
		cpu_has_xfeatures(XFEATURE_MASK_SSE | XFEATURE_MASK_YMM, NULL);
#ifndef COMPAT_CANNOT_USE_AVX512
	poly1305_use_avx512 =
		boot_cpu_has(X86_FEATURE_AVX) &&
		boot_cpu_has(X86_FEATURE_AVX2) &&
		boot_cpu_has(X86_FEATURE_AVX512F) &&
		cpu_has_xfeatures(XFEATURE_MASK_SSE | XFEATURE_MASK_YMM |
				  XFEATURE_MASK_AVX512, NULL) &&
		/* Skylake downclocks unacceptably much when using zmm. */
		boot_cpu_data.x86_model != INTEL_FAM6_SKYLAKE_X;
#endif
#else

	poly1305_use_avx = !!(cpu_feature2 & CPUID2_AVX) &&
		__ymm_enabled();
	poly1305_use_avx2 = poly1305_use_avx &&
		!!(cpu_stdext_feature & CPUID_STDEXT_AVX2);
	poly1305_use_avx512 = poly1305_use_avx2 &&
		!!(cpu_stdext_feature & CPUID_STDEXT_AVX512F)  &&
		__zmm_enabled();
#endif
}

static inline bool poly1305_init_arch(void *ctx,
				      const u8 key[POLY1305_KEY_SIZE])
{
	poly1305_init_x86_64(ctx, key);
	return true;
}

struct poly1305_arch_internal {
	union {
		struct {
			u32 h[5];
			u32 is_base2_26;
		};
		u64 hs[3];
	};
	u64 r[2];
	u64 pad;
	struct { u32 r2, r1, r4, r3; } rn[9];
};

/* The AVX code uses base 2^26, while the scalar code uses base 2^64. If we hit
 * the unfortunate situation of using AVX and then having to go back to scalar
 * -- because the user is silly and has called the update function from two
 * separate contexts -- then we need to convert back to the original base before
 * proceeding. It is possible to reason that the initial reduction below is
 * sufficient given the implementation invariants. However, for an avoidance of
 * doubt and because this is not performance critical, we do the full reduction
 * anyway.
 */
static void convert_to_base2_64(void *ctx)
{
	struct poly1305_arch_internal *state = ctx;
	u32 cy;

	if (!state->is_base2_26)
		return;

	cy = state->h[0] >> 26; state->h[0] &= 0x3ffffff; state->h[1] += cy;
	cy = state->h[1] >> 26; state->h[1] &= 0x3ffffff; state->h[2] += cy;
	cy = state->h[2] >> 26; state->h[2] &= 0x3ffffff; state->h[3] += cy;
	cy = state->h[3] >> 26; state->h[3] &= 0x3ffffff; state->h[4] += cy;
	state->hs[0] = ((u64)state->h[2] << 52) | ((u64)state->h[1] << 26) | state->h[0];
	state->hs[1] = ((u64)state->h[4] << 40) | ((u64)state->h[3] << 14) | (state->h[2] >> 12);
	state->hs[2] = state->h[4] >> 24;
#define ULT(a, b) ((a ^ ((a ^ b) | ((a - b) ^ b))) >> (sizeof(a) * 8 - 1))
	cy = (state->hs[2] >> 2) + (state->hs[2] & ~3ULL);
	state->hs[2] &= 3;
	state->hs[0] += cy;
	state->hs[1] += (cy = ULT(state->hs[0], cy));
	state->hs[2] += ULT(state->hs[1], cy);
#undef ULT
	state->is_base2_26 = 0;
}

static inline bool poly1305_blocks_arch(void *ctx, const u8 *inp,
					size_t len, const u32 padbit,
					simd_context_t *simd_context)
{
	struct poly1305_arch_internal *state = ctx;

	/* SIMD disables preemption, so relax after processing each page. */
	BUILD_BUG_ON(PAGE_SIZE < POLY1305_BLOCK_SIZE ||
		     PAGE_SIZE % POLY1305_BLOCK_SIZE);

	if (!poly1305_use_avx ||
	    (len < (POLY1305_BLOCK_SIZE * 18) && !state->is_base2_26) ||
	    !simd_use(simd_context)) {
		convert_to_base2_64(ctx);
		poly1305_blocks_x86_64(ctx, inp, len, padbit);
		return true;
	}

	for (;;) {
		const size_t bytes = min_t(size_t, len, PAGE_SIZE);

		if (poly1305_use_avx512)
			poly1305_blocks_avx512(ctx, inp, bytes, padbit);
		else if (poly1305_use_avx2)
			poly1305_blocks_avx2(ctx, inp, bytes, padbit);
		else
			poly1305_blocks_avx(ctx, inp, bytes, padbit);
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
	struct poly1305_arch_internal *state = ctx;

	if (!IS_ENABLED(CONFIG_AS_AVX) || !poly1305_use_avx ||
	    !state->is_base2_26 || !simd_use(simd_context)) {
		convert_to_base2_64(ctx);
		poly1305_emit_x86_64(ctx, mac, nonce);
	} else
		poly1305_emit_avx(ctx, mac, nonce);
	return true;
}
