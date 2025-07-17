/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (c) 2022 The FreeBSD Foundation
 */

#ifndef _WG_CRYPTO
#define _WG_CRYPTO

#include <sys/param.h>
#include <sys/endian.h>
#include <crypto/chacha20_poly1305.h>
#include <crypto/curve25519.h>

struct mbuf;

int crypto_init(void);
void crypto_deinit(void);

enum chacha20poly1305_lengths {
	XCHACHA20POLY1305_NONCE_SIZE = 24,
	CHACHA20POLY1305_KEY_SIZE = 32,
	CHACHA20POLY1305_AUTHTAG_SIZE = 16
};

static inline void
chacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			 const uint8_t *ad, const size_t ad_len,
			 const uint64_t nonce,
			 const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	uint8_t nonce_bytes[8];

	le64enc(nonce_bytes, nonce);
	chacha20_poly1305_encrypt(dst, src, src_len, ad, ad_len,
				  nonce_bytes, sizeof(nonce_bytes), key);
}

static inline bool
chacha20poly1305_decrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			 const uint8_t *ad, const size_t ad_len,
			 const uint64_t nonce,
			 const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	uint8_t nonce_bytes[8];

	le64enc(nonce_bytes, nonce);
	return (chacha20_poly1305_decrypt(dst, src, src_len, ad, ad_len,
					  nonce_bytes, sizeof(nonce_bytes), key));
}

static inline void
xchacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src,
			  const size_t src_len, const uint8_t *ad,
			  const size_t ad_len,
			  const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
			  const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	xchacha20_poly1305_encrypt(dst, src, src_len, ad, ad_len, nonce, key);
}

static inline bool
xchacha20poly1305_decrypt(uint8_t *dst, const uint8_t *src,
			  const size_t src_len,  const uint8_t *ad,
			  const size_t ad_len,
			  const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
			  const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	return (xchacha20_poly1305_decrypt(dst, src, src_len, ad, ad_len, nonce, key));
}

int
chacha20poly1305_encrypt_mbuf(struct mbuf *, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);

int
chacha20poly1305_decrypt_mbuf(struct mbuf *, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE]);


enum blake2s_lengths {
	BLAKE2S_BLOCK_SIZE = 64,
	BLAKE2S_HASH_SIZE = 32,
	BLAKE2S_KEY_SIZE = 32
};

#ifdef COMPAT_NEED_BLAKE2S
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

static inline void blake2s(uint8_t *out, const uint8_t *in, const uint8_t *key,
			   const size_t outlen, const size_t inlen, const size_t keylen)
{
	struct blake2s_state state;

	if (keylen)
		blake2s_init_key(&state, outlen, key, keylen);
	else
		blake2s_init(&state, outlen);

	blake2s_update(&state, in, inlen);
	blake2s_final(&state, out);
}
#endif

#endif
