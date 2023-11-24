/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Ararat River Consulting, LLC under
 * sponsorship from the FreeBSD Foundation.
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
 */

#ifndef __CRYPTO_CURVE25519_H__
#define	__CRYPTO_CURVE25519_H__

#include <sys/libkern.h>

#define	CURVE25519_KEY_SIZE	32

bool	curve25519(uint8_t *public, const uint8_t *secret,
    const uint8_t *basepoint);
bool	curve25519_generate_public(uint8_t *public,
    const uint8_t *secret);

static __inline void
curve25519_clamp_secret(uint8_t *secret)
{
	secret[0] &= 248;
	secret[31] &= 127;
	secret[31] |= 64;
}

static __inline void
curve25519_generate_secret(uint8_t *secret)
{
	arc4random_buf(secret, CURVE25519_KEY_SIZE);
	curve25519_clamp_secret(secret);
}

#endif /* __CRYPTO_CURVE25519_H__ */
