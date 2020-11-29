// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */
#ifdef __linux__
#include <asm/fpu/api.h>
#include <asm/cpufeature.h>
#include <asm/processor.h>
#include <asm/intel-family.h>
#else
#include <sys/simd-x86_64.h>
#endif

asmlinkage void hchacha20_ssse3(u32 *derived_key, const u8 *nonce,
				const u8 *key);
asmlinkage void chacha20_ssse3(u8 *out, const u8 *in, const size_t len,
			       const u32 key[8], const u32 counter[4]);
asmlinkage void chacha20_avx2(u8 *out, const u8 *in, const size_t len,
			      const u32 key[8], const u32 counter[4]);
asmlinkage void chacha20_avx512(u8 *out, const u8 *in, const size_t len,
				const u32 key[8], const u32 counter[4]);
asmlinkage void chacha20_avx512vl(u8 *out, const u8 *in, const size_t len,
				  const u32 key[8], const u32 counter[4]);

static bool chacha20_use_ssse3 __ro_after_init;
static bool chacha20_use_avx2 __ro_after_init;
static bool chacha20_use_avx512 __ro_after_init;
static bool chacha20_use_avx512vl __ro_after_init;
static bool *const chacha20_nobs[] __initconst = {
	&chacha20_use_ssse3, &chacha20_use_avx2, &chacha20_use_avx512,
	&chacha20_use_avx512vl };

static void __init chacha20_fpu_init(void)
{
#ifdef __linux__
	chacha20_use_ssse3 = boot_cpu_has(X86_FEATURE_SSSE3);
	chacha20_use_avx2 =
		boot_cpu_has(X86_FEATURE_AVX) &&
		boot_cpu_has(X86_FEATURE_AVX2) &&
		cpu_has_xfeatures(XFEATURE_MASK_SSE | XFEATURE_MASK_YMM, NULL);
#ifndef COMPAT_CANNOT_USE_AVX512
	chacha20_use_avx512 =
		boot_cpu_has(X86_FEATURE_AVX) &&
		boot_cpu_has(X86_FEATURE_AVX2) &&
		boot_cpu_has(X86_FEATURE_AVX512F) &&
		cpu_has_xfeatures(XFEATURE_MASK_SSE | XFEATURE_MASK_YMM |
				  XFEATURE_MASK_AVX512, NULL) &&
		/* Skylake downclocks unacceptably much when using zmm. */
		boot_cpu_data.x86_model != INTEL_FAM6_SKYLAKE_X;
	chacha20_use_avx512vl =
		boot_cpu_has(X86_FEATURE_AVX) &&
		boot_cpu_has(X86_FEATURE_AVX2) &&
		boot_cpu_has(X86_FEATURE_AVX512F) &&
		boot_cpu_has(X86_FEATURE_AVX512VL) &&
		cpu_has_xfeatures(XFEATURE_MASK_SSE | XFEATURE_MASK_YMM |
				  XFEATURE_MASK_AVX512, NULL);
#endif
#else
	chacha20_use_ssse3 = !!(cpu_feature2 & CPUID2_SSSE3);
	chacha20_use_avx2 = !!(cpu_feature2 & CPUID2_AVX) &&
		!!(cpu_stdext_feature & CPUID_STDEXT_AVX2) &&
		__ymm_enabled();
	chacha20_use_avx512 = chacha20_use_avx2 &&
		!!(cpu_stdext_feature & CPUID_STDEXT_AVX512F)  &&
		__zmm_enabled();
	chacha20_use_avx512vl = chacha20_use_avx512 &&
		!!(cpu_stdext_feature & CPUID_STDEXT_AVX512F)  &&
		!!(cpu_stdext_feature & CPUID_STDEXT_AVX512VL);
#endif
	if (bootverbose)
		printf("ssse3: %d  avx2: %d avx512: %d avx512vl: %d\n",
		   chacha20_use_ssse3,
		   chacha20_use_avx2,
		   chacha20_use_avx512,
		   chacha20_use_avx512vl);
}

static inline bool chacha20_arch(struct chacha20_ctx *ctx, u8 *dst,
				 const u8 *src, size_t len,
				 simd_context_t *simd_context)
{
	/* SIMD disables preemption, so relax after processing each page. */
	BUILD_BUG_ON(PAGE_SIZE < CHACHA20_BLOCK_SIZE ||
		     PAGE_SIZE % CHACHA20_BLOCK_SIZE);

	if (!chacha20_use_ssse3) {
		return false;
	}
	if (len <= CHACHA20_BLOCK_SIZE) {
		return false;
	}
	if  (!simd_use(simd_context)) {
		return false;
	}
	for (;;) {
		const size_t bytes = min_t(size_t, len, PAGE_SIZE);

		if (chacha20_use_avx512 &&
		    len >= CHACHA20_BLOCK_SIZE * 8)
			chacha20_avx512(dst, src, bytes, ctx->key, ctx->counter);
		else if (chacha20_use_avx512vl &&
			 len >= CHACHA20_BLOCK_SIZE * 4)
			chacha20_avx512vl(dst, src, bytes, ctx->key, ctx->counter);
		else if (chacha20_use_avx2 &&
			 len >= CHACHA20_BLOCK_SIZE * 4)
			chacha20_avx2(dst, src, bytes, ctx->key, ctx->counter);
		else
			chacha20_ssse3(dst, src, bytes, ctx->key, ctx->counter);
		ctx->counter[0] += (bytes + 63) / 64;
		len -= bytes;
		if (!len)
			break;
		dst += bytes;
		src += bytes;
		simd_relax(simd_context);
	}

	return true;
}

static inline bool hchacha20_arch(u32 derived_key[CHACHA20_KEY_WORDS],
				  const u8 nonce[HCHACHA20_NONCE_SIZE],
				  const u8 key[HCHACHA20_KEY_SIZE],
				  simd_context_t *simd_context)
{
	if (IS_ENABLED(CONFIG_AS_SSSE3) && chacha20_use_ssse3 &&
	    simd_use(simd_context)) {
		hchacha20_ssse3(derived_key, nonce, key);
		return true;
	}
	return false;
}
