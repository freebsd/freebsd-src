/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012, Jouni Malinen <j@w1.fi>
 * All rights reserved.
 *
 * Galois/Counter Mode (GCM) and GMAC with AES
 *
 * Originally sourced from hostapd 2.11 (src/crypto/aes-gcm.c).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <crypto/rijndael/rijndael.h>
#include <net80211/ieee80211_crypto_gcm.h>

#define AES_BLOCK_LEN 16

#define BIT(x)	(1U << (x))

static __inline void
xor_block(uint8_t *b, const uint8_t *a, size_t len)
{
	int i;
	for (i = 0; i < len; i++)
		b[i] ^= a[i];
}

static inline
void WPA_PUT_BE64(uint8_t *a, uint64_t val)
{
	a[0] = val >> 56;
	a[1] = val >> 48;
	a[2] = val >> 40;
	a[3] = val >> 32;
	a[4] = val >> 24;
	a[5] = val >> 16;
	a[6] = val >> 8;
	a[7] = val & 0xff;
}

static inline void
WPA_PUT_BE32(uint8_t *a, uint32_t val)
{
	a[0] = (val >> 24) & 0xff;
	a[1] = (val >> 16) & 0xff;
	a[2] = (val >> 8) & 0xff;
	a[3] = val & 0xff;
}

static inline uint32_t
WPA_GET_BE32(const uint8_t *a)
{
	return (((uint32_t) a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3]);
}

static void
inc32(uint8_t *block)
{
	uint32_t val;

	val = WPA_GET_BE32(block + AES_BLOCK_LEN - 4);
	val++;
	WPA_PUT_BE32(block + AES_BLOCK_LEN - 4, val);
}

static void
shift_right_block(uint8_t *v)
{
	uint32_t val;

	val = WPA_GET_BE32(v + 12);
	val >>= 1;
	if (v[11] & 0x01)
		val |= 0x80000000;
	WPA_PUT_BE32(v + 12, val);

	val = WPA_GET_BE32(v + 8);
	val >>= 1;
	if (v[7] & 0x01)
		val |= 0x80000000;
	WPA_PUT_BE32(v + 8, val);

	val = WPA_GET_BE32(v + 4);
	val >>= 1;
	if (v[3] & 0x01)
		val |= 0x80000000;
	WPA_PUT_BE32(v + 4, val);

	val = WPA_GET_BE32(v);
	val >>= 1;
	WPA_PUT_BE32(v, val);
}


/* Multiplication in GF(2^128) */
static void
gf_mult(const uint8_t *x, const uint8_t *y, uint8_t *z)
{
	uint8_t v[16];
	int i, j;

	memset(z, 0, 16); /* Z_0 = 0^128 */
	memcpy(v, y, 16); /* V_0 = Y */

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 8; j++) {
			if (x[i] & BIT(7 - j)) {
				/* Z_(i + 1) = Z_i XOR V_i */
				xor_block(z, v, AES_BLOCK_LEN);
			} else {
				/* Z_(i + 1) = Z_i */
			}

			if (v[15] & 0x01) {
				/* V_(i + 1) = (V_i >> 1) XOR R */
				shift_right_block(v);
				/* R = 11100001 || 0^120 */
				v[0] ^= 0xe1;
			} else {
				/* V_(i + 1) = V_i >> 1 */
				shift_right_block(v);
			}
		}
	}
}

static void
ghash_start(uint8_t *y)
{
	/* Y_0 = 0^128 */
	memset(y, 0, 16);
}

static void
ghash(const uint8_t *h, const uint8_t *x, size_t xlen, uint8_t *y)
{
	size_t m, i;
	const uint8_t *xpos = x;
	uint8_t tmp[16];

	m = xlen / 16;

	for (i = 0; i < m; i++) {
		/* Y_i = (Y^(i-1) XOR X_i) dot H */
		xor_block(y, xpos, AES_BLOCK_LEN);
		xpos += 16;

		/* dot operation:
		 * multiplication operation for binary Galois (finite) field of
		 * 2^128 elements */
		gf_mult(y, h, tmp);
		memcpy(y, tmp, 16);
	}

	if (x + xlen > xpos) {
		/* Add zero padded last block */
		size_t last = x + xlen - xpos;
		memcpy(tmp, xpos, last);
		memset(tmp + last, 0, sizeof(tmp) - last);

		/* Y_i = (Y^(i-1) XOR X_i) dot H */
		xor_block(y, tmp, AES_BLOCK_LEN);

		/* dot operation:
		 * multiplication operation for binary Galois (finite) field of
		 * 2^128 elements */
		gf_mult(y, h, tmp);
		memcpy(y, tmp, 16);
	}

	/* Return Y_m */
}

/*
 * Execute the GCTR call with the counter block icb
 * on payload x (size len), output into y.
 */
static void
aes_gctr(rijndael_ctx *aes, const uint8_t *icb,
    const uint8_t *x, size_t xlen, uint8_t *y)
{
	size_t i, n, last;
	uint8_t cb[AES_BLOCK_LEN], tmp[AES_BLOCK_LEN];
	const uint8_t *xpos = x;
	uint8_t *ypos = y;

	if (xlen == 0)
		return;

	n = xlen / 16;

	memcpy(cb, icb, AES_BLOCK_LEN);

	/* Full blocks */
	for (i = 0; i < n; i++) {
		rijndael_encrypt(aes, cb, ypos);
		xor_block(ypos, xpos, AES_BLOCK_LEN);
		xpos += AES_BLOCK_LEN;
		ypos += AES_BLOCK_LEN;
		inc32(cb);
	}

	last = x + xlen - xpos;
	if (last) {
		/* Last, partial block */
		rijndael_encrypt(aes, cb, tmp);
		for (i = 0; i < last; i++)
			*ypos++ = *xpos++ ^ tmp[i];
	}
}

static void
aes_gcm_init_hash_subkey(rijndael_ctx *aes, uint8_t *H)
{
	/* Generate hash subkey H = AES_K(0^128) */
	memset(H, 0, AES_BLOCK_LEN);

	rijndael_encrypt(aes, H, H);
}

static void
aes_gcm_prepare_j0(const uint8_t *iv, size_t iv_len, const uint8_t *H,
    uint8_t *J0)
{
	uint8_t len_buf[16];

	if (iv_len == 12) {
		/* Prepare block J_0 = IV || 0^31 || 1 [len(IV) = 96] */
		memcpy(J0, iv, iv_len);
		memset(J0 + iv_len, 0, AES_BLOCK_LEN - iv_len);
		J0[AES_BLOCK_LEN - 1] = 0x01;
	} else {
		/*
		 * s = 128 * ceil(len(IV)/128) - len(IV)
		 * J_0 = GHASH_H(IV || 0^(s+64) || [len(IV)]_64)
		 */
		ghash_start(J0);
		ghash(H, iv, iv_len, J0);
		WPA_PUT_BE64(len_buf, 0);
		WPA_PUT_BE64(len_buf + 8, iv_len * 8);
		ghash(H, len_buf, sizeof(len_buf), J0);
	}
}

static void
aes_gcm_gctr(rijndael_ctx *aes, const uint8_t *J0, const uint8_t *in,
    size_t len, uint8_t *out)
{
	uint8_t J0inc[AES_BLOCK_LEN];

	if (len == 0)
		return;

	memcpy(J0inc, J0, AES_BLOCK_LEN);
	inc32(J0inc);

	aes_gctr(aes, J0inc, in, len, out);
}

static void
aes_gcm_ghash(const uint8_t *H, const uint8_t *aad, size_t aad_len,
    const uint8_t *crypt, size_t crypt_len, uint8_t *S)
{
	uint8_t len_buf[16];

	/*
	 * u = 128 * ceil[len(C)/128] - len(C)
	 * v = 128 * ceil[len(A)/128] - len(A)
	 * S = GHASH_H(A || 0^v || C || 0^u || [len(A)]64 || [len(C)]64)
	 * (i.e., zero padded to block size A || C and lengths of each in bits)
	 */
	ghash_start(S);
	ghash(H, aad, aad_len, S);
	ghash(H, crypt, crypt_len, S);
	WPA_PUT_BE64(len_buf, aad_len * 8);
	WPA_PUT_BE64(len_buf + 8, crypt_len * 8);
	ghash(H, len_buf, sizeof(len_buf), S);
}

/**
 * aes_gcm_ae - GCM-AE_K(IV, P, A)
 */
void
ieee80211_crypto_aes_gcm_ae(rijndael_ctx *aes, const uint8_t *iv, size_t iv_len,
    const uint8_t *plain, size_t plain_len,
    const uint8_t *aad, size_t aad_len, uint8_t *crypt, uint8_t *tag)
{
	uint8_t H[AES_BLOCK_LEN];
	uint8_t J0[AES_BLOCK_LEN];
	uint8_t S[GCMP_MIC_LEN];

	aes_gcm_init_hash_subkey(aes, H);

	aes_gcm_prepare_j0(iv, iv_len, H, J0);

	/* C = GCTR_K(inc_32(J_0), P) */
	aes_gcm_gctr(aes, J0, plain, plain_len, crypt);

	aes_gcm_ghash(H, aad, aad_len, crypt, plain_len, S);

	/* T = MSB_t(GCTR_K(J_0, S)) */
	aes_gctr(aes, J0, S, sizeof(S), tag);

	/* Return (C, T) */
}

/**
 * aes_gcm_ad - GCM-AD_K(IV, C, A, T)
 *
 * Return 0 if OK, -1 if decrypt failure.
 */
int
ieee80211_crypto_aes_gcm_ad(rijndael_ctx *aes, const uint8_t *iv, size_t iv_len,
    const uint8_t *crypt, size_t crypt_len,
    const uint8_t *aad, size_t aad_len, const uint8_t *tag, uint8_t *plain)
{
	uint8_t H[AES_BLOCK_LEN];
	uint8_t J0[AES_BLOCK_LEN];
	uint8_t S[16], T[GCMP_MIC_LEN];

	aes_gcm_init_hash_subkey(aes, H);

	aes_gcm_prepare_j0(iv, iv_len, H, J0);

	/* P = GCTR_K(inc_32(J_0), C) */
	aes_gcm_gctr(aes, J0, crypt, crypt_len, plain);

	aes_gcm_ghash(H, aad, aad_len, crypt, crypt_len, S);

	/* T' = MSB_t(GCTR_K(J_0, S)) */
	aes_gctr(aes, J0, S, sizeof(S), T);

	if (memcmp(tag, T, 16) != 0) {
		return (-1);
	}

	return (0);
}
