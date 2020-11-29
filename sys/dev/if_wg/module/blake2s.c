// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2012 Samuel Neves <sneves@dei.uc.pt>. All Rights Reserved.
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This is an implementation of the BLAKE2s hash and PRF functions.
 *
 * Information: https://blake2.net/
 *
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/endian.h>

#include <crypto/blake2s.h>

static inline uint32_t
ror32(uint32_t word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

typedef union {
	struct {
		uint8_t digest_length;
		uint8_t key_length;
		uint8_t fanout;
		uint8_t depth;
		uint32_t leaf_length;
		uint32_t node_offset;
		uint16_t xof_length;
		uint8_t node_depth;
		uint8_t inner_length;
		uint8_t salt[8];
		uint8_t personal[8];
	};
	uint32_t words[8];
} __packed blake2s_param;

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
	if (state->last_node)
		state->f[1] = -1;
	state->f[0] = -1;
}

static inline void blake2s_increment_counter(struct blake2s_state *state,
					     const uint32_t inc)
{
	state->t[0] += inc;
	state->t[1] += (state->t[0] < inc);
}

static inline void blake2s_init_param(struct blake2s_state *state,
				      const blake2s_param *param)
{
	int i;

	memset(state, 0, sizeof(*state));
	for (i = 0; i < 8; ++i)
		state->h[i] = blake2s_iv[i] ^ le32toh(param->words[i]);
}

void blake2s_init(struct blake2s_state *state, const size_t outlen)
{
	blake2s_param param __aligned(__alignof__(uint32_t)) = {
		.digest_length = outlen,
		.fanout = 1,
		.depth = 1
	};

	/*WARN_ON(IS_ENABLED(DEBUG) && (!outlen || outlen > BLAKE2S_HASH_SIZE));*/
	blake2s_init_param(state, &param);
}

void blake2s_init_key(struct blake2s_state *state, const size_t outlen,
		      const void *key, const size_t keylen)
{
	blake2s_param param = { .digest_length = outlen,
				.key_length = keylen,
				.fanout = 1,
				.depth = 1 };
	uint8_t block[BLAKE2S_BLOCK_SIZE] = { 0 };

	/*WARN_ON(IS_ENABLED(DEBUG) && (!outlen || outlen > BLAKE2S_HASH_SIZE ||
		!key || !keylen || keylen > BLAKE2S_KEY_SIZE));*/
	blake2s_init_param(state, &param);
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

	/*WARN_ON(IS_ENABLED(DEBUG) &&
		(nblocks > 1 && inc != BLAKE2S_BLOCK_SIZE));*/

	while (nblocks > 0) {
		blake2s_increment_counter(state, inc);
		memcpy(m, block, BLAKE2S_BLOCK_SIZE);
		for(i = 0; i < (sizeof(m)/sizeof(m[0])); i++)
			(m[i]) = le32toh((m[i]));
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
		const size_t nblocks =
			(inlen + BLAKE2S_BLOCK_SIZE - 1) / BLAKE2S_BLOCK_SIZE;
		/* Hash one less (full) block than strictly possible */
		blake2s_compress(state, in, nblocks - 1, BLAKE2S_BLOCK_SIZE);
		in += BLAKE2S_BLOCK_SIZE * (nblocks - 1);
		inlen -= BLAKE2S_BLOCK_SIZE * (nblocks - 1);
	}
	memcpy(state->buf + state->buflen, in, inlen);
	state->buflen += inlen;
}

void blake2s_final(struct blake2s_state *state, uint8_t *out, const size_t outlen)
{
	int i;
	/*WARN_ON(IS_ENABLED(DEBUG) &&
		(!out || !outlen || outlen > BLAKE2S_HASH_SIZE));*/
	blake2s_set_lastblock(state);
	memset(state->buf + state->buflen, 0,
	       BLAKE2S_BLOCK_SIZE - state->buflen); /* Padding */
	blake2s_compress(state, state->buf, 1, state->buflen);
	for(i = 0; i < (sizeof(state->h)/sizeof(state->h[0])); i++)
		(state->h[i]) = htole32((state->h[i]));

	memcpy(out, state->h, outlen);
	explicit_bzero(state, sizeof(*state));
}

void blake2s_hmac(uint8_t *out, const uint8_t *in, const uint8_t *key, const size_t outlen,
		  const size_t inlen, const size_t keylen)
{
	struct blake2s_state state;
	uint8_t x_key[BLAKE2S_BLOCK_SIZE] __aligned(__alignof__(uint32_t)) = { 0 };
	uint8_t i_hash[BLAKE2S_HASH_SIZE] __aligned(__alignof__(uint32_t));
	int i;

	if (keylen > BLAKE2S_BLOCK_SIZE) {
		blake2s_init(&state, BLAKE2S_HASH_SIZE);
		blake2s_update(&state, key, keylen);
		blake2s_final(&state, x_key, BLAKE2S_HASH_SIZE);
	} else
		memcpy(x_key, key, keylen);

	for (i = 0; i < BLAKE2S_BLOCK_SIZE; ++i)
		x_key[i] ^= 0x36;

	blake2s_init(&state, BLAKE2S_HASH_SIZE);
	blake2s_update(&state, x_key, BLAKE2S_BLOCK_SIZE);
	blake2s_update(&state, in, inlen);
	blake2s_final(&state, i_hash, BLAKE2S_HASH_SIZE);

	for (i = 0; i < BLAKE2S_BLOCK_SIZE; ++i)
		x_key[i] ^= 0x5c ^ 0x36;

	blake2s_init(&state, BLAKE2S_HASH_SIZE);
	blake2s_update(&state, x_key, BLAKE2S_BLOCK_SIZE);
	blake2s_update(&state, i_hash, BLAKE2S_HASH_SIZE);
	blake2s_final(&state, i_hash, BLAKE2S_HASH_SIZE);

	memcpy(out, i_hash, outlen);
	explicit_bzero(x_key, BLAKE2S_BLOCK_SIZE);
	explicit_bzero(i_hash, BLAKE2S_HASH_SIZE);
}
