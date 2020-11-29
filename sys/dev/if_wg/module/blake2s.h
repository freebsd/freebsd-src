/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <sys/types.h>

#ifndef _BLAKE2S_H_
#define _BLAKE2S_H_

/*#define WARN_ON(a) if(a) printf("%s failed at %s:%d\n", #a, __FILE__, __LINE__)
#define IS_ENABLED(...) true*/


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
	size_t buflen;
	uint8_t last_node;
};

void blake2s_init(struct blake2s_state *state, const size_t outlen);
void blake2s_init_key(struct blake2s_state *state, const size_t outlen,
		      const void *key, const size_t keylen);
void blake2s_update(struct blake2s_state *state, const uint8_t *in, size_t inlen);
void blake2s_final(struct blake2s_state *state, uint8_t *out, const size_t outlen);

static inline void blake2s(uint8_t *out, const uint8_t *in, const uint8_t *key,
			   const size_t outlen, const size_t inlen,
			   const size_t keylen)
{
	struct blake2s_state state;

	/*WARN_ON(IS_ENABLED(DEBUG) && ((!in && inlen > 0) || !out || !outlen ||
		outlen > BLAKE2S_HASH_SIZE || keylen > BLAKE2S_KEY_SIZE ||
		(!key && keylen)));*/

	if (keylen)
		blake2s_init_key(&state, outlen, key, keylen);
	else
		blake2s_init(&state, outlen);

	blake2s_update(&state, in, inlen);
	blake2s_final(&state, out, outlen);
}

void blake2s_hmac(uint8_t *out, const uint8_t *in, const uint8_t *key,
		const size_t outlen, const size_t inlen, const size_t keylen);

#endif /* _BLAKE2S_H_ */
