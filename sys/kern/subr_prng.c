/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Conrad Meyer <cem@FreeBSD.org>.  All rights reserved.
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/prng.h>
#include <sys/smp.h>
#include <sys/systm.h>

#if !PCG_HAS_128BIT_OPS
/* On 32-bit platforms, gang together two 32-bit generators. */
typedef struct {
	pcg32u_random_t states[2];
} pcg64u_random_t;

static inline void
pcg64u_srandom_r(pcg64u_random_t *state64, uint64_t seed)
{
	pcg32u_srandom_r(&state64->states[0], seed);
	pcg32u_srandom_r(&state64->states[1], seed);
}

static inline uint64_t
pcg64u_random_r(pcg64u_random_t *state64)
{
	return ((((uint64_t)pcg32u_random_r(&state64->states[0])) << 32) |
	    pcg32u_random_r(&state64->states[1]));
}

static inline uint64_t
pcg64u_boundedrand_r(pcg64u_random_t *state64, uint64_t bound)
{
	uint64_t threshold = -bound % bound;
	for (;;) {
		uint64_t r = pcg64u_random_r(state64);
		if (r >= threshold)
			return (r % bound);
	}
}
#endif

DPCPU_DEFINE_STATIC(pcg32u_random_t, pcpu_prng32_state);
DPCPU_DEFINE_STATIC(pcg64u_random_t, pcpu_prng64_state);

static void
prng_init(void *dummy __unused)
{
	pcg32u_random_t *state;
	pcg64u_random_t *state64;
	int i;

	CPU_FOREACH(i) {
		state = DPCPU_ID_PTR(i, pcpu_prng32_state);
		pcg32u_srandom_r(state, 1);
		state64 = DPCPU_ID_PTR(i, pcpu_prng64_state);
		pcg64u_srandom_r(state64, 1);
	}
}
SYSINIT(prng_init, SI_SUB_CPU, SI_ORDER_ANY, prng_init, NULL);

uint32_t
prng32(void)
{
	uint32_t r;

	critical_enter();
	r = pcg32u_random_r(DPCPU_PTR(pcpu_prng32_state));
	critical_exit();
	return (r);
}

uint32_t
prng32_bounded(uint32_t bound)
{
	uint32_t r;

	critical_enter();
	r = pcg32u_boundedrand_r(DPCPU_PTR(pcpu_prng32_state), bound);
	critical_exit();
	return (r);
}

uint64_t
prng64(void)
{
	uint64_t r;

	critical_enter();
	r = pcg64u_random_r(DPCPU_PTR(pcpu_prng64_state));
	critical_exit();
	return (r);
}

uint64_t
prng64_bounded(uint64_t bound)
{
	uint64_t r;

	critical_enter();
	r = pcg64u_boundedrand_r(DPCPU_PTR(pcpu_prng64_state), bound);
	critical_exit();
	return (r);
}
