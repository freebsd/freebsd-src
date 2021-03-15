/*
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WG_CRYPTO
#define _WG_CRYPTO

#include <sys/types.h>

enum chacha20poly1305_lengths {
	XCHACHA20POLY1305_NONCE_SIZE = 24,
	CHACHA20POLY1305_KEY_SIZE = 32,
	CHACHA20POLY1305_AUTHTAG_SIZE = 16
};

void
chacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			 const uint8_t *ad, const size_t ad_len,
			 const uint64_t nonce,
			 const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

bool
chacha20poly1305_decrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			 const uint8_t *ad, const size_t ad_len,
			 const uint64_t nonce,
			 const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

void
xchacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src,
			  const size_t src_len, const uint8_t *ad,
			  const size_t ad_len,
			  const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
			  const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

bool
xchacha20poly1305_decrypt(uint8_t *dst, const uint8_t *src,
			  const size_t src_len,  const uint8_t *ad,
			  const size_t ad_len,
			  const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
			  const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);


enum blake2s_lengths {
	BLAKE2S_BLOCK_SIZE = 64,
	BLAKE2S_HASH_SIZE = 32,
	BLAKE2S_KEY_SIZE = 32
};

struct blake2s_state {
	uint32_t h[8];
	uint32_t t[2];
	uint32_t f[2];
	uint8_t buf[BLAKE2S_BLOCK_SIZE];
	unsigned int buflen;
	unsigned int outlen;
};

void blake2s_init(struct blake2s_state *state, const size_t outlen);

void blake2s_init_key(struct blake2s_state *state, const size_t outlen,
		      const uint8_t *key, const size_t keylen);

void blake2s_update(struct blake2s_state *state, const uint8_t *in, size_t inlen);

void blake2s_final(struct blake2s_state *state, uint8_t *out);

void blake2s(uint8_t *out, const uint8_t *in, const uint8_t *key,
	     const size_t outlen, const size_t inlen, const size_t keylen);

void blake2s_hmac(uint8_t *out, const uint8_t *in, const uint8_t *key,
		  const size_t outlen, const size_t inlen, const size_t keylen);

enum curve25519_lengths {
        CURVE25519_KEY_SIZE = 32
};

bool curve25519(uint8_t mypublic[static CURVE25519_KEY_SIZE],
		const uint8_t secret[static CURVE25519_KEY_SIZE],
		const uint8_t basepoint[static CURVE25519_KEY_SIZE]);

static inline bool
curve25519_generate_public(uint8_t pub[static CURVE25519_KEY_SIZE],
			   const uint8_t secret[static CURVE25519_KEY_SIZE])
{
	static const uint8_t basepoint[CURVE25519_KEY_SIZE] = { 9 };

	return curve25519(pub, secret, basepoint);
}

static inline void curve25519_clamp_secret(uint8_t secret[static CURVE25519_KEY_SIZE])
{
        secret[0] &= 248;
        secret[31] = (secret[31] & 127) | 64;
}

static inline void curve25519_generate_secret(uint8_t secret[CURVE25519_KEY_SIZE])
{
	arc4random_buf(secret, CURVE25519_KEY_SIZE);
	curve25519_clamp_secret(secret);
}

#endif
