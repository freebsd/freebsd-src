/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (c) 2022 The FreeBSD Foundation
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <opencrypto/cryptodev.h>

#include "crypto.h"

static crypto_session_t chacha20_poly1305_sid;

#ifdef COMPAT_NEED_BLAKE2S
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

#define le32_to_cpup(a) le32toh(*(a))
#define cpu_to_le32(a) htole32(a)

static inline void cpu_to_le32_array(uint32_t *buf, unsigned int words)
{
        while (words--) {
		*buf = cpu_to_le32(*buf);
		++buf;
	}
}
static inline void le32_to_cpu_array(uint32_t *buf, unsigned int words)
{
        while (words--) {
		*buf = le32_to_cpup(buf);
		++buf;
        }
}
static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
	return (word >> (shift & 31)) | (word << ((-shift) & 31));
}

static const uint32_t blake2s_iv[8] = {
	0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
	0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t blake2s_sigma[10][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
	{ 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
	{ 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
	{ 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
	{ 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
	{ 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
	{ 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
	{ 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
	{ 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
};

static inline void blake2s_set_lastblock(struct blake2s_state *state)
{
	state->f[0] = -1;
}

static inline void blake2s_increment_counter(struct blake2s_state *state,
					     const uint32_t inc)
{
	state->t[0] += inc;
	state->t[1] += (state->t[0] < inc);
}

static inline void blake2s_init_param(struct blake2s_state *state,
				      const uint32_t param)
{
	int i;

	memset(state, 0, sizeof(*state));
	for (i = 0; i < 8; ++i)
		state->h[i] = blake2s_iv[i];
	state->h[0] ^= param;
}

void blake2s_init(struct blake2s_state *state, const size_t outlen)
{
	blake2s_init_param(state, 0x01010000 | outlen);
	state->outlen = outlen;
}

void blake2s_init_key(struct blake2s_state *state, const size_t outlen,
		      const uint8_t *key, const size_t keylen)
{
	uint8_t block[BLAKE2S_BLOCK_SIZE] = { 0 };

	blake2s_init_param(state, 0x01010000 | keylen << 8 | outlen);
	state->outlen = outlen;
	memcpy(block, key, keylen);
	blake2s_update(state, block, BLAKE2S_BLOCK_SIZE);
	explicit_bzero(block, BLAKE2S_BLOCK_SIZE);
}

static inline void blake2s_compress(struct blake2s_state *state,
				    const uint8_t *block, size_t nblocks,
				    const uint32_t inc)
{
	uint32_t m[16];
	uint32_t v[16];
	int i;

	while (nblocks > 0) {
		blake2s_increment_counter(state, inc);
		memcpy(m, block, BLAKE2S_BLOCK_SIZE);
		le32_to_cpu_array(m, ARRAY_SIZE(m));
		memcpy(v, state->h, 32);
		v[ 8] = blake2s_iv[0];
		v[ 9] = blake2s_iv[1];
		v[10] = blake2s_iv[2];
		v[11] = blake2s_iv[3];
		v[12] = blake2s_iv[4] ^ state->t[0];
		v[13] = blake2s_iv[5] ^ state->t[1];
		v[14] = blake2s_iv[6] ^ state->f[0];
		v[15] = blake2s_iv[7] ^ state->f[1];

#define G(r, i, a, b, c, d) do { \
	a += b + m[blake2s_sigma[r][2 * i + 0]]; \
	d = ror32(d ^ a, 16); \
	c += d; \
	b = ror32(b ^ c, 12); \
	a += b + m[blake2s_sigma[r][2 * i + 1]]; \
	d = ror32(d ^ a, 8); \
	c += d; \
	b = ror32(b ^ c, 7); \
} while (0)

#define ROUND(r) do { \
	G(r, 0, v[0], v[ 4], v[ 8], v[12]); \
	G(r, 1, v[1], v[ 5], v[ 9], v[13]); \
	G(r, 2, v[2], v[ 6], v[10], v[14]); \
	G(r, 3, v[3], v[ 7], v[11], v[15]); \
	G(r, 4, v[0], v[ 5], v[10], v[15]); \
	G(r, 5, v[1], v[ 6], v[11], v[12]); \
	G(r, 6, v[2], v[ 7], v[ 8], v[13]); \
	G(r, 7, v[3], v[ 4], v[ 9], v[14]); \
} while (0)
		ROUND(0);
		ROUND(1);
		ROUND(2);
		ROUND(3);
		ROUND(4);
		ROUND(5);
		ROUND(6);
		ROUND(7);
		ROUND(8);
		ROUND(9);

#undef G
#undef ROUND

		for (i = 0; i < 8; ++i)
			state->h[i] ^= v[i] ^ v[i + 8];

		block += BLAKE2S_BLOCK_SIZE;
		--nblocks;
	}
}

void blake2s_update(struct blake2s_state *state, const uint8_t *in, size_t inlen)
{
	const size_t fill = BLAKE2S_BLOCK_SIZE - state->buflen;

	if (!inlen)
		return;
	if (inlen > fill) {
		memcpy(state->buf + state->buflen, in, fill);
		blake2s_compress(state, state->buf, 1, BLAKE2S_BLOCK_SIZE);
		state->buflen = 0;
		in += fill;
		inlen -= fill;
	}
	if (inlen > BLAKE2S_BLOCK_SIZE) {
		const size_t nblocks = DIV_ROUND_UP(inlen, BLAKE2S_BLOCK_SIZE);
		/* Hash one less (full) block than strictly possible */
		blake2s_compress(state, in, nblocks - 1, BLAKE2S_BLOCK_SIZE);
		in += BLAKE2S_BLOCK_SIZE * (nblocks - 1);
		inlen -= BLAKE2S_BLOCK_SIZE * (nblocks - 1);
	}
	memcpy(state->buf + state->buflen, in, inlen);
	state->buflen += inlen;
}

void blake2s_final(struct blake2s_state *state, uint8_t *out)
{
	blake2s_set_lastblock(state);
	memset(state->buf + state->buflen, 0,
	       BLAKE2S_BLOCK_SIZE - state->buflen); /* Padding */
	blake2s_compress(state, state->buf, 1, state->buflen);
	cpu_to_le32_array(state->h, ARRAY_SIZE(state->h));
	memcpy(out, state->h, state->outlen);
	explicit_bzero(state, sizeof(*state));
}
#endif

static int
crypto_callback(struct cryptop *crp)
{
	return (0);
}

int
chacha20poly1305_encrypt_mbuf(struct mbuf *m, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	static const char blank_tag[POLY1305_HASH_LEN];
	struct cryptop crp;
	int ret;

	if (!m_append(m, POLY1305_HASH_LEN, blank_tag))
		return (ENOMEM);
	crypto_initreq(&crp, chacha20_poly1305_sid);
	crp.crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp.crp_flags = CRYPTO_F_IV_SEPARATE | CRYPTO_F_CBIMM;
	crypto_use_mbuf(&crp, m);
	crp.crp_payload_length = m->m_pkthdr.len - POLY1305_HASH_LEN;
	crp.crp_digest_start = crp.crp_payload_length;
	le64enc(crp.crp_iv, nonce);
	crp.crp_cipher_key = key;
	crp.crp_callback = crypto_callback;
	ret = crypto_dispatch(&crp);
	crypto_destroyreq(&crp);
	return (ret);
}

int
chacha20poly1305_decrypt_mbuf(struct mbuf *m, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	struct cryptop crp;
	int ret;

	if (m->m_pkthdr.len < POLY1305_HASH_LEN)
		return (EMSGSIZE);
	crypto_initreq(&crp, chacha20_poly1305_sid);
	crp.crp_op = CRYPTO_OP_DECRYPT | CRYPTO_OP_VERIFY_DIGEST;
	crp.crp_flags = CRYPTO_F_IV_SEPARATE | CRYPTO_F_CBIMM;
	crypto_use_mbuf(&crp, m);
	crp.crp_payload_length = m->m_pkthdr.len - POLY1305_HASH_LEN;
	crp.crp_digest_start = crp.crp_payload_length;
	le64enc(crp.crp_iv, nonce);
	crp.crp_cipher_key = key;
	crp.crp_callback = crypto_callback;
	ret = crypto_dispatch(&crp);
	crypto_destroyreq(&crp);
	if (ret)
		return (ret);
	m_adj(m, -POLY1305_HASH_LEN);
	return (0);
}

int
crypto_init(void)
{
	struct crypto_session_params csp = {
		.csp_mode = CSP_MODE_AEAD,
		.csp_ivlen = sizeof(uint64_t),
		.csp_cipher_alg = CRYPTO_CHACHA20_POLY1305,
		.csp_cipher_klen = CHACHA20POLY1305_KEY_SIZE,
		.csp_flags = CSP_F_SEPARATE_AAD | CSP_F_SEPARATE_OUTPUT
	};
	int ret = crypto_newsession(&chacha20_poly1305_sid, &csp, CRYPTOCAP_F_SOFTWARE);
	if (ret != 0)
		return (ret);
	return (0);
}

void
crypto_deinit(void)
{
	crypto_freesession(chacha20_poly1305_sid);
}
