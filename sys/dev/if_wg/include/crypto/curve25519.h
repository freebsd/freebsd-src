/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019-2020 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _CURVE25519_H_
#define _CURVE25519_H_

#include <sys/systm.h>

#define CURVE25519_KEY_SIZE 32

void curve25519_generic(u8 [CURVE25519_KEY_SIZE],
			const u8 [CURVE25519_KEY_SIZE],
			const u8 [CURVE25519_KEY_SIZE]);

static inline void curve25519_clamp_secret(u8 secret[CURVE25519_KEY_SIZE])
{
	secret[0] &= 248;
	secret[31] = (secret[31] & 127) | 64;
}

static const u8 null_point[CURVE25519_KEY_SIZE] = { 0 };

static inline int curve25519(u8 mypublic[CURVE25519_KEY_SIZE],
		const u8 secret[CURVE25519_KEY_SIZE],
		const u8 basepoint[CURVE25519_KEY_SIZE])
{
	curve25519_generic(mypublic, secret, basepoint);
	return timingsafe_bcmp(mypublic, null_point, CURVE25519_KEY_SIZE);
}

static inline int curve25519_generate_public(u8 pub[CURVE25519_KEY_SIZE],
				const u8 secret[CURVE25519_KEY_SIZE])
{
	static const u8 basepoint[CURVE25519_KEY_SIZE] __aligned(32) = { 9 };

	if (timingsafe_bcmp(secret, null_point, CURVE25519_KEY_SIZE) == 0)
		return 0;

	return curve25519(pub, secret, basepoint);
}

static inline void curve25519_generate_secret(u8 secret[CURVE25519_KEY_SIZE])
{
	arc4random_buf(secret, CURVE25519_KEY_SIZE);
	curve25519_clamp_secret(secret);
}

#endif /* _CURVE25519_H_ */
