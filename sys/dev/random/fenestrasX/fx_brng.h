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

#include <dev/random/fenestrasX/fx_rng.h>

#define	FXRNG_BUFRNG_SZ	128

/*
 * An object representing a buffered random number generator with forward
 * secrecy (aka "fast-key-erasure").
 *
 * There is a single static root instance of this object associated with the
 * entropy harvester, as well as additional instances per CPU, lazily allocated
 * in NUMA-local memory, seeded from output of the root generator.
 */
struct fxrng_buffered_rng {
	struct fxrng_basic_rng	brng_rng;
#define	FXRNG_BRNG_LOCK(brng)	mtx_lock(&(brng)->brng_rng.rng_lk)
#define	FXRNG_BRNG_UNLOCK(brng)	mtx_unlock(&(brng)->brng_rng.rng_lk)
#define	FXRNG_BRNG_ASSERT(brng)	mtx_assert(&(brng)->brng_rng.rng_lk, MA_OWNED)
#define	FXRNG_BRNG_ASSERT_NOT(brng) \
	mtx_assert(&(brng)->brng_rng.rng_lk, MA_NOTOWNED)

	/* Entropy reseed generation ("seed version"). */
	uint64_t	brng_generation;

	/* Buffered output for quick access by small requests. */
	uint8_t		brng_buffer[FXRNG_BUFRNG_SZ];
	uint8_t		brng_avail_idx;
};

void fxrng_brng_init(struct fxrng_buffered_rng *);
void fxrng_brng_produce_seed_data_internal(struct fxrng_buffered_rng *, void *,
    size_t, uint64_t *seed_generation);
void fxrng_brng_read(struct fxrng_buffered_rng *, void *, size_t);

void fxrng_brng_reseed(const void *, size_t);
struct harvest_event;
void fxrng_brng_src_reseed(const struct harvest_event *);
