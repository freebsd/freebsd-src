/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef CURVE25519_H
#define CURVE25519_H

#include <stdint.h>
#include <sys/types.h>

enum curve25519_lengths {
	CURVE25519_KEY_SIZE = 32
};

void curve25519(uint8_t mypublic[static CURVE25519_KEY_SIZE], const uint8_t secret[static CURVE25519_KEY_SIZE], const uint8_t basepoint[static CURVE25519_KEY_SIZE]);
void curve25519_generate_public(uint8_t pub[static CURVE25519_KEY_SIZE], const uint8_t secret[static CURVE25519_KEY_SIZE]);
static inline void curve25519_clamp_secret(uint8_t secret[static CURVE25519_KEY_SIZE])
{
	secret[0] &= 248;
	secret[31] = (secret[31] & 127) | 64;
}

#endif
