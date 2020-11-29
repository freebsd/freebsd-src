/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef _ZINC_CURVE25519_H
#define _ZINC_CURVE25519_H

#include <sys/types.h>

enum curve25519_lengths {
	CURVE25519_KEY_SIZE = 32
};

bool curve25519(uint8_t mypublic[CURVE25519_KEY_SIZE],
			     const uint8_t secret[CURVE25519_KEY_SIZE],
			     const uint8_t basepoint[CURVE25519_KEY_SIZE]);
void curve25519_generate_secret(uint8_t secret[CURVE25519_KEY_SIZE]);
bool curve25519_generate_public(
	uint8_t pub[CURVE25519_KEY_SIZE], const uint8_t secret[CURVE25519_KEY_SIZE]);

static inline void curve25519_clamp_secret(uint8_t secret[CURVE25519_KEY_SIZE])
{
	secret[0] &= 248;
	secret[31] = (secret[31] & 127) | 64;
}

#endif /* _ZINC_CURVE25519_H */
