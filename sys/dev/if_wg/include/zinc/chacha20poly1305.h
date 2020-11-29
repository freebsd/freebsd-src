/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef _ZINC_CHACHA20POLY1305_H
#define _ZINC_CHACHA20POLY1305_H

#include <sys/types.h>

struct scatterlist;

enum chacha20poly1305_lengths {
	XCHACHA20POLY1305_NONCE_SIZE = 24,
	CHACHA20POLY1305_KEY_SIZE = 32,
	CHACHA20POLY1305_AUTHTAG_SIZE = 16
};

void chacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			      const uint8_t *ad, const size_t ad_len,
			      const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

bool chacha20poly1305_encrypt_sg_inplace(
	struct scatterlist *src, const size_t src_len, const uint8_t *ad,
	const size_t ad_len, const uint64_t nonce,
	const uint8_t key[CHACHA20POLY1305_KEY_SIZE], simd_context_t *simd_context);

bool chacha20poly1305_decrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			 const uint8_t *ad, const size_t ad_len, const uint64_t nonce,
			 const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

bool chacha20poly1305_decrypt_sg_inplace(
	struct scatterlist *src, size_t src_len, const uint8_t *ad,
	const size_t ad_len, const uint64_t nonce,
	const uint8_t key[CHACHA20POLY1305_KEY_SIZE], simd_context_t *simd_context);

void xchacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			       const uint8_t *ad, const size_t ad_len,
			       const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
			       const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

bool xchacha20poly1305_decrypt(
	uint8_t *dst, const uint8_t *src, const size_t src_len, const uint8_t *ad,
	const size_t ad_len, const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
	const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

#endif /* _ZINC_CHACHA20POLY1305_H */
