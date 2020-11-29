// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This is an implementation of the ChaCha20Poly1305 AEAD construction.
 *
 * Information: https://tools.ietf.org/html/rfc8439
 */

#include <sys/support.h>
#include <zinc/chacha20poly1305.h>
#include <zinc/chacha20.h>
#include <zinc/poly1305.h>
#include "selftest/run.h"

static const u8 pad0[CHACHA20_BLOCK_SIZE] = { 0 };

static inline void
__chacha20poly1305_encrypt(u8 *dst, const u8 *src, const size_t src_len,
			   const u8 *ad, const size_t ad_len, const u64 nonce,
			   const u8 key[CHACHA20POLY1305_KEY_SIZE],
			   simd_context_t *simd_context)
{
	struct poly1305_ctx poly1305_state;
	struct chacha20_ctx chacha20_state;
	union {
		u8 block0[POLY1305_KEY_SIZE];
		__le64 lens[2];
	} b = { { 0 } };

	chacha20_init(&chacha20_state, key, nonce);
	chacha20(&chacha20_state, b.block0, b.block0, sizeof(b.block0),
		 simd_context);
	poly1305_init(&poly1305_state, b.block0);

	poly1305_update(&poly1305_state, ad, ad_len, simd_context);
	poly1305_update(&poly1305_state, pad0, (0x10 - ad_len) & 0xf,
			simd_context);

	chacha20(&chacha20_state, dst, src, src_len, simd_context);

	poly1305_update(&poly1305_state, dst, src_len, simd_context);
	poly1305_update(&poly1305_state, pad0, (0x10 - src_len) & 0xf,
			simd_context);

	b.lens[0] = cpu_to_le64(ad_len);
	b.lens[1] = cpu_to_le64(src_len);
	poly1305_update(&poly1305_state, (u8 *)b.lens, sizeof(b.lens),
			simd_context);

	poly1305_final(&poly1305_state, dst + src_len, simd_context);

	memzero_explicit(&chacha20_state, sizeof(chacha20_state));
	memzero_explicit(&b, sizeof(b));
}

void chacha20poly1305_encrypt(u8 *dst, const u8 *src, const size_t src_len,
			      const u8 *ad, const size_t ad_len,
			      const u64 nonce,
			      const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	simd_context_t simd_context;

	simd_get(&simd_context);
	__chacha20poly1305_encrypt(dst, src, src_len, ad, ad_len, nonce, key,
				   &simd_context);
	simd_put(&simd_context);
}
EXPORT_SYMBOL(chacha20poly1305_encrypt);
static inline bool
__chacha20poly1305_decrypt(u8 *dst, const u8 *src, const size_t src_len,
			   const u8 *ad, const size_t ad_len, const u64 nonce,
			   const u8 key[CHACHA20POLY1305_KEY_SIZE],
			   simd_context_t *simd_context)
{
	struct poly1305_ctx poly1305_state;
	struct chacha20_ctx chacha20_state;
	int ret;
	size_t dst_len;
	union {
		u8 block0[POLY1305_KEY_SIZE];
		u8 mac[POLY1305_MAC_SIZE];
		__le64 lens[2];
	} b = { { 0 } };

	if (unlikely(src_len < POLY1305_MAC_SIZE)) {
		printf("src_len too short\n");
		return false;
	}

	chacha20_init(&chacha20_state, key, nonce);
	chacha20(&chacha20_state, b.block0, b.block0, sizeof(b.block0),
		 simd_context);
	poly1305_init(&poly1305_state, b.block0);

	poly1305_update(&poly1305_state, ad, ad_len, simd_context);
	poly1305_update(&poly1305_state, pad0, (0x10 - ad_len) & 0xf,
			simd_context);

	dst_len = src_len - POLY1305_MAC_SIZE;
	poly1305_update(&poly1305_state, src, dst_len, simd_context);
	poly1305_update(&poly1305_state, pad0, (0x10 - dst_len) & 0xf,
			simd_context);

	b.lens[0] = cpu_to_le64(ad_len);
	b.lens[1] = cpu_to_le64(dst_len);
	poly1305_update(&poly1305_state, (u8 *)b.lens, sizeof(b.lens),
			simd_context);

	poly1305_final(&poly1305_state, b.mac, simd_context);

	ret = crypto_memneq(b.mac, src + dst_len, POLY1305_MAC_SIZE);
	if (likely(!ret))
		chacha20(&chacha20_state, dst, src, dst_len, simd_context);
	else {
		printf("calculated: %16D\n", b.mac, "");
		printf("sent      : %16D\n", src + dst_len, "");
	}
	memzero_explicit(&chacha20_state, sizeof(chacha20_state));
	memzero_explicit(&b, sizeof(b));

	return !ret;
}

bool chacha20poly1305_decrypt(u8 *dst, const u8 *src, const size_t src_len,
			      const u8 *ad, const size_t ad_len,
			      const u64 nonce,
			      const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	simd_context_t simd_context;
	bool ret;

	simd_get(&simd_context);
	ret = __chacha20poly1305_decrypt(dst, src, src_len, ad, ad_len, nonce,
					 key, &simd_context);
	simd_put(&simd_context);
	return ret;
}
EXPORT_SYMBOL(chacha20poly1305_decrypt);

void xchacha20poly1305_encrypt(u8 *dst, const u8 *src, const size_t src_len,
			       const u8 *ad, const size_t ad_len,
			       const u8 nonce[XCHACHA20POLY1305_NONCE_SIZE],
			       const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	simd_context_t simd_context;
	u32 derived_key[CHACHA20_KEY_WORDS] __aligned(16);

	simd_get(&simd_context);
	hchacha20(derived_key, nonce, key, &simd_context);
	cpu_to_le32_array(derived_key, ARRAY_SIZE(derived_key));
	__chacha20poly1305_encrypt(dst, src, src_len, ad, ad_len,
				   get_unaligned_le64(nonce + 16),
				   (u8 *)derived_key, &simd_context);
	memzero_explicit(derived_key, CHACHA20POLY1305_KEY_SIZE);
	simd_put(&simd_context);
}
EXPORT_SYMBOL(xchacha20poly1305_encrypt);

bool xchacha20poly1305_decrypt(u8 *dst, const u8 *src, const size_t src_len,
			       const u8 *ad, const size_t ad_len,
			       const u8 nonce[XCHACHA20POLY1305_NONCE_SIZE],
			       const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	bool ret;
	simd_context_t simd_context;
	u32 derived_key[CHACHA20_KEY_WORDS] __aligned(16);

	simd_get(&simd_context);
	hchacha20(derived_key, nonce, key, &simd_context);
	cpu_to_le32_array(derived_key, ARRAY_SIZE(derived_key));
	ret = __chacha20poly1305_decrypt(dst, src, src_len, ad, ad_len,
					 get_unaligned_le64(nonce + 16),
					 (u8 *)derived_key, &simd_context);
	memzero_explicit(derived_key, CHACHA20POLY1305_KEY_SIZE);
	simd_put(&simd_context);
	return ret;
}
EXPORT_SYMBOL(xchacha20poly1305_decrypt);

#include "selftest/chacha20poly1305.c"

static int __init mod_init(void)
{
	if (!selftest_run("chacha20poly1305", chacha20poly1305_selftest,
			  NULL, 0))
		return -ENOTRECOVERABLE;
	return 0;
}

static void __exit mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);
