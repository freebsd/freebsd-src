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

/*
 * This random algorithm is derived in part from the "Windows 10 random number
 * generation infrastructure" whitepaper published by Niels Ferguson and
 * Microsoft: https://aka.ms/win10rng
 *
 * It is also inspired by DJB's writing on buffered key-erasure PRNGs:
 * https://blog.cr.yp.to/20170723-random.html
 *
 * The Windows 10 RNG bears some similarity to Fortuna, which Ferguson was also
 * involved with.  Notable differences include:
 *  - Extended to multi-CPU design
 *  - Extended to pre-buffer some PRNG output
 *  - Pool-based reseeding is solely time-based (rather than on-access w/
 *    pacing)
 *  - Extended to specify efficient userspace design
 *  - Always-available design (requires the equivalent of loader(8) for all
 *    boots; probably relatively easy given the limited platforms Windows 10
 *    supports)
 *
 * Some aspects of the design document I found confusing and may have
 * misinterpreted:
 *  - Relationship between root PRNG seed version and periodic reseed pool use.
 *    I interpreted these as separate sequences.  The root PRNG seed version is
 *    bumped both by the periodic pool based reseed, and also special
 *    conditions such as the first time an entropy source provides entropy.  I
 *    don't think first-time entropy sources should cause us to skip an entropy
 *    pool reseed.
 *  - Initial seeding.  The paper is pretty terse on the subject.  My
 *    interpretation of the document is that the Windows RNG infrastructure
 *    relies on the loader(8)-provided material for initial seeding and either
 *    ignores or doesn't start entropy sources until after that time.  So when
 *    the paper says that first-time entropy source material "bypasses the
 *    pools," the root PRNG state already has been keyed for the first time and
 *    can generate 256 bits, mix it with the first-time entropy, and reseed
 *    immediately.
 *
 * Some notable design choices in this implementation divergent from that
 * specified in the document above:
 *  - Blake2b instead of SHA-2 512 for entropy pooling
 *  - Chacha20 instead of AES-CTR DRBG for PRF
 *  - Initial seeding.  We treat the 0->1 seed version (brng_generation) edge
 *    as the transition from blocked to unblocked.  That edge is also the first
 *    time the key of the root BRNG's PRF is set.  We perform initial seeding
 *    when the first request for entropy arrives.
 *    • As a result: Entropy callbacks prior to this edge do not have a keyed
 *      root PRNG, so bypassing the pools is kind of meaningless.  Instead,
 *      they feed into pool0.  (They also do not set the root PRNG key or bump
 *      the root PRNG seed version.)
 *    • Entropy callbacks after the edge behave like the specification.
 *    • All one-off sources are fed into pool0 and the result used to seed the
 *      root BRNG during the initial seed step.
 *    • All memory needed for initial seeding must be preallocated or static or
 *      fit on the stack; random reads can occur in nonsleepable contexts and
 *      we cannot allocate M_WAITOK.  (We also cannot fail to incorporate any
 *      present one-off source, to the extent it is in the control of
 *      software.)
 * - Timer interval reseeding.  We also start the timer-based reseeding at
 *   initial seed, but unlike the design, our initial seed is some time after
 *   load (usually within the order of micro- or milliseconds due to
 *   stack_guard on x86, but conceivably later if nothing reads from random for
 *   a while).
 *
 * Not yet implemented, not in scope, or todo:
 *  - Various initial seeding sources we don't have yet
 *  - In particular, VM migration/copy detection
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/domainset.h>
#include <sys/fail.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sdt.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpu.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/uint128.h>

#include <dev/random/fenestrasX/fx_brng.h>
#include <dev/random/fenestrasX/fx_hash.h>
#include <dev/random/fenestrasX/fx_pool.h>
#include <dev/random/fenestrasX/fx_priv.h>
#include <dev/random/fenestrasX/fx_pub.h>
#include <dev/random/fenestrasX/fx_rng.h>

struct fxrng_buffered_rng fxrng_root;
uint64_t __read_mostly fxrng_root_generation;
DPCPU_DEFINE_STATIC(struct fxrng_buffered_rng *, fxrng_brng);

/*
 * Top-level read API from randomdev.  Responsible for NOWAIT-allocating
 * per-cpu NUMA-local BRNGs, if needed and satisfiable; subroutines handle
 * reseeding if the local BRNG is stale and rekeying when necessary.  In
 * low-memory conditions when a local BRNG cannot be allocated, the request is
 * simply forwarded to the root BRNG.
 *
 * It is a precondition is that the root BRNG initial seeding has completed and
 * the root generation number >0.
 */
static void
_fxrng_alg_read(uint8_t *output, size_t nbytes, uint64_t *seed_version_out)
{
	struct fxrng_buffered_rng **pcpu_brng_p, *rng, *tmp;
	struct pcpu *pcpu;

	pcpu = get_pcpu();

	/*
	 * The following statement directly accesses an implementation detail
	 * of DPCPU, but the macros cater only to pinned threads; we want to
	 * operate on our initial CPU, without pinning, *even if* we migrate.
	 */
	pcpu_brng_p = _DPCPU_PTR(pcpu->pc_dynamic, fxrng_brng);

	rng = (void *)atomic_load_acq_ptr((uintptr_t *)pcpu_brng_p);

	/*
	 * Usually the pcpu BRNG has already been allocated, but we do it
	 * on-demand and need to check first.  BRNGs are never deallocated and
	 * are valid as soon as the pointer is initialized.
	 */
	if (__predict_false(rng == NULL)) {
		uint8_t newkey[FX_CHACHA20_KEYSIZE];
		struct domainset *ds;
		int domain;

		domain = pcpu->pc_domain;

		/*
		 * Allocate pcpu BRNGs off-domain on weird NUMA machines like
		 * AMD Threadripper 2990WX, which has 2 NUMA nodes without
		 * local memory controllers.  The PREF policy is automatically
		 * converted to something appropriate when domains are empty.
		 * (FIXED is not.)
		 *
		 * Otherwise, allocate strictly CPU-local memory.  The
		 * rationale is this: if there is a memory shortage such that
		 * PREF policy would fallback to RR, we have no business
		 * wasting memory on a faster BRNG.  So, use a FIXED domainset
		 * policy.  If we cannot allocate, that's fine!  We fall back
		 * to invoking the root BRNG.
		 */
		if (VM_DOMAIN_EMPTY(domain))
			ds = DOMAINSET_PREF(domain);
		else
			ds = DOMAINSET_FIXED(domain);

		rng = malloc_domainset(sizeof(*rng), M_ENTROPY, ds,
		    M_NOWAIT | M_ZERO);
		if (rng == NULL) {
			/* Relatively easy case: fall back to root BRNG. */
			rng = &fxrng_root;
			goto have_valid_rng;
		}

		fxrng_brng_init(rng);

		/*
		 * The root BRNG is always up and available.  Requests are
		 * always satisfiable.  This is a design invariant.
		 */
		ASSERT_DEBUG(atomic_load_acq_64(&fxrng_root_generation) != 0,
		    "%s: attempting to seed child BRNG when root hasn't "
		    "been initialized yet.", __func__);

		FXRNG_BRNG_LOCK(&fxrng_root);
#ifdef WITNESS
		/* Establish lock order root->pcpu for WITNESS. */
		FXRNG_BRNG_LOCK(rng);
		FXRNG_BRNG_UNLOCK(rng);
#endif
		fxrng_brng_produce_seed_data_internal(&fxrng_root, newkey,
		    sizeof(newkey), &rng->brng_generation);
		FXRNG_BRNG_ASSERT_NOT(&fxrng_root);

		fxrng_rng_setkey(&rng->brng_rng, newkey, sizeof(newkey));
		explicit_bzero(newkey, sizeof(newkey));

		/*
		 * We have a valid RNG.  Try to install it, or grab the other
		 * one if we lost the race.
		 */
		tmp = NULL;
		while (tmp == NULL)
			if (atomic_fcmpset_ptr((uintptr_t *)pcpu_brng_p,
			    (uintptr_t *)&tmp, (uintptr_t)rng))
				goto have_valid_rng;

		/*
		 * We lost the race.  There's nothing sensitive about
		 * our BRNG's PRF state, because it will never be used
		 * for anything and the key doesn't expose any
		 * information about the parent (root) generator's
		 * state -- it has already rekeyed.  The generation
		 * number is public, and a zero counter isn't sensitive.
		 */
		free(rng, M_ENTROPY);
		/*
		 * Use the winner's PCPU BRNG.
		 */
		rng = tmp;
	}

have_valid_rng:
	/* At this point we have a valid, initialized and seeded rng pointer. */
	FXRNG_BRNG_LOCK(rng);
	if (seed_version_out != NULL)
		*seed_version_out = rng->brng_generation;
	fxrng_brng_read(rng, output, nbytes);
	FXRNG_BRNG_ASSERT_NOT(rng);
}

static void
fxrng_alg_read(uint8_t *output, size_t nbytes)
{
	_fxrng_alg_read(output, nbytes, NULL);
}

/*
 * External API for arc4random(9) to fetch new key material and associated seed
 * version in chacha20_randomstir().
 */
void
read_random_key(void *output, size_t nbytes, uint64_t *seed_version_out)
{
	/* Ensure _fxrng_alg_read invariant. */
	if (__predict_false(atomic_load_acq_64(&fxrng_root_generation) == 0))
		(void)fxrng_alg_seeded();

	_fxrng_alg_read(output, nbytes, seed_version_out);
}

static void
fxrng_init_alg(void *dummy __unused)
{
	DPCPU_ZERO(fxrng_brng);
	fxrng_brng_init(&fxrng_root);
	fxrng_pools_init();
}
SYSINIT(random_alg, SI_SUB_RANDOM, SI_ORDER_SECOND, fxrng_init_alg, NULL);

/*
 * Public visibility struct referenced directly by other parts of randomdev.
 */
const struct random_algorithm random_alg_context = {
	.ra_ident = "fenestrasX",
	.ra_pre_read = (void (*)(void))nullop,
	.ra_read = fxrng_alg_read,
	.ra_seeded = fxrng_alg_seeded,
	.ra_event_processor = fxrng_event_processor,
	.ra_poolcount = FXRNG_NPOOLS,
};
