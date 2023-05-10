/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Conrad Meyer <cem@FreeBSD.org>
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
 *
 * $FreeBSD$
 */
#pragma once

#include <dev/random/fenestrasX/fx_priv.h>
#define	blake2b_init	blake2b_init_ref
#define	blake2b_update	blake2b_update_ref
#define	blake2b_final	blake2b_final_ref
#include <contrib/libb2/blake2.h>

#define	FXRNG_HASH_SZ	BLAKE2B_OUTBYTES	/* 64 */

/*
 * Wrappers for hash function abstraction.
 */
struct fxrng_hash {
	blake2b_state	state;
};

static inline void
fxrng_hash_init(struct fxrng_hash *h)
{
	int rc;

	rc = blake2b_init(&h->state, FXRNG_HASH_SZ);
	ASSERT(rc == 0, "blake2b_init");
}

static inline void
fxrng_hash_update(struct fxrng_hash *h, const void *buf, size_t sz)
{
	int rc;

	rc = blake2b_update(&h->state, buf, sz);
	ASSERT(rc == 0, "blake2b_update");
}

static inline void
fxrng_hash_finish(struct fxrng_hash *h, uint8_t buf[static FXRNG_HASH_SZ], size_t sz)
{
	int rc;

	rc = blake2b_final(&h->state, buf, sz);
	ASSERT(rc == 0, "blake2b_final(sz=%zu)", sz);
	explicit_bzero(h, sizeof(*h));
}
