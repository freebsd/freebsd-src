// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <asm/hwcap.h>
#include <asm/neon.h>
#if defined(CONFIG_ZINC_ARCH_ARM)
#include <asm/system_info.h>
#include <asm/cputype.h>
#endif

asmlinkage void chacha20_arm(u8 *out, const u8 *in, const size_t len,
			     const u32 key[8], const u32 counter[4]);
asmlinkage void hchacha20_arm(const u32 state[16], u32 out[8]);
asmlinkage void chacha20_neon(u8 *out, const u8 *in, const size_t len,
			      const u32 key[8], const u32 counter[4]);

static bool chacha20_use_neon __ro_after_init;
static bool *const chacha20_nobs[] __initconst = { &chacha20_use_neon };
static void __init chacha20_fpu_init(void)
{
#if defined(CONFIG_ZINC_ARCH_ARM64)
	chacha20_use_neon = cpu_have_named_feature(ASIMD);
#elif defined(CONFIG_ZINC_ARCH_ARM)
	switch (read_cpuid_part()) {
	case ARM_CPU_PART_CORTEX_A7:
	case ARM_CPU_PART_CORTEX_A5:
		/* The Cortex-A7 and Cortex-A5 do not perform well with the NEON
		 * implementation but do incredibly with the scalar one and use
		 * less power.
		 */
		break;
	default:
		chacha20_use_neon = elf_hwcap & HWCAP_NEON;
	}
#endif
}

static inline bool chacha20_arch(struct chacha20_ctx *ctx, u8 *dst,
				 const u8 *src, size_t len,
				 simd_context_t *simd_context)
{
	/* SIMD disables preemption, so relax after processing each page. */
	BUILD_BUG_ON(PAGE_SIZE < CHACHA20_BLOCK_SIZE ||
		     PAGE_SIZE % CHACHA20_BLOCK_SIZE);

	for (;;) {
		if (IS_ENABLED(CONFIG_KERNEL_MODE_NEON) && chacha20_use_neon &&
		    len >= CHACHA20_BLOCK_SIZE * 3 && simd_use(simd_context)) {
			const size_t bytes = min_t(size_t, len, PAGE_SIZE);

			chacha20_neon(dst, src, bytes, ctx->key, ctx->counter);
			ctx->counter[0] += (bytes + 63) / 64;
			len -= bytes;
			if (!len)
				break;
			dst += bytes;
			src += bytes;
			simd_relax(simd_context);
		} else {
			chacha20_arm(dst, src, len, ctx->key, ctx->counter);
			ctx->counter[0] += (len + 63) / 64;
			break;
		}
	}

	return true;
}

static inline bool hchacha20_arch(u32 derived_key[CHACHA20_KEY_WORDS],
				  const u8 nonce[HCHACHA20_NONCE_SIZE],
				  const u8 key[HCHACHA20_KEY_SIZE],
				  simd_context_t *simd_context)
{
	if (IS_ENABLED(CONFIG_ZINC_ARCH_ARM)) {
		u32 x[] = { CHACHA20_CONSTANT_EXPA,
			    CHACHA20_CONSTANT_ND_3,
			    CHACHA20_CONSTANT_2_BY,
			    CHACHA20_CONSTANT_TE_K,
			    get_unaligned_le32(key + 0),
			    get_unaligned_le32(key + 4),
			    get_unaligned_le32(key + 8),
			    get_unaligned_le32(key + 12),
			    get_unaligned_le32(key + 16),
			    get_unaligned_le32(key + 20),
			    get_unaligned_le32(key + 24),
			    get_unaligned_le32(key + 28),
			    get_unaligned_le32(nonce + 0),
			    get_unaligned_le32(nonce + 4),
			    get_unaligned_le32(nonce + 8),
			    get_unaligned_le32(nonce + 12)
			  };
		hchacha20_arm(x, derived_key);
		return true;
	}
	return false;
}
