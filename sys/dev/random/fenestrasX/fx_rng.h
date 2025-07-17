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
 */
#pragma once

#include <crypto/chacha20/chacha.h>
#include <dev/random/fenestrasX/fx_priv.h>

#define	FX_CHACHA20_KEYSIZE	32

struct fxrng_basic_rng {
	struct mtx		rng_lk;
#define	FXRNG_RNG_LOCK(rng)	mtx_lock(&(rng)->rng_lk)
#define	FXRNG_RNG_UNLOCK(rng)	mtx_unlock(&(rng)->rng_lk)
#define	FXRNG_RNG_ASSERT(rng)	mtx_assert(&(rng)->rng_lk, MA_OWNED)
#define	FXRNG_RNG_ASSERT_NOT(rng)	mtx_assert(&(rng)->rng_lk, MA_NOTOWNED)
	/* CTR-mode cipher state, including 128-bit embedded counter. */
	struct chacha_ctx	rng_prf;
};

/*
 * Initialize a basic rng instance (half of a buffered FX BRNG).
 * Memory should be zero initialized before this routine.
 */
static inline void
fxrng_rng_init(struct fxrng_basic_rng *rng, bool is_root_rng)
{
	if (is_root_rng)
		mtx_init(&rng->rng_lk, "fx root brng lk", NULL, MTX_DEF);
	else
		mtx_init(&rng->rng_lk, "fx pcpu brng lk", NULL, MTX_DEF);
}

static inline void
fxrng_rng_setkey(struct fxrng_basic_rng *rng, const void *newkey, size_t len)
{
	ASSERT_DEBUG(len == FX_CHACHA20_KEYSIZE, "chacha20 uses 256 bit keys");
	chacha_keysetup(&rng->rng_prf, newkey, len * 8);
}

void fxrng_rng_genrandom_internal(struct fxrng_basic_rng *, void *, size_t,
    bool return_unlocked);

void fxrng_rng_reseed(struct fxrng_basic_rng *, bool seeded, const void *,
    size_t);
void fxrng_rng_src_reseed(struct fxrng_basic_rng *,
    const struct harvest_event *);
