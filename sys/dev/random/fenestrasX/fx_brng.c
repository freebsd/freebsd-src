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

#include <sys/param.h>
#include <sys/fail.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vdso.h>

#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/uint128.h>

#include <dev/random/fenestrasX/fx_brng.h>
#include <dev/random/fenestrasX/fx_priv.h>
#include <dev/random/fenestrasX/fx_pub.h>
#include <dev/random/fenestrasX/fx_rng.h>

/*
 * Implementation of a buffered RNG, described in § 1.2-1.4 of the whitepaper.
 */

/*
 * Initialize a buffered rng instance (either the static root instance, or a
 * per-cpu instance on the heap.  Both should be zero initialized before this
 * routine.
 */
void
fxrng_brng_init(struct fxrng_buffered_rng *rng)
{
	fxrng_rng_init(&rng->brng_rng, rng == &fxrng_root);

	/* I.e., the buffer is empty. */
	rng->brng_avail_idx = sizeof(rng->brng_buffer);

	/*
	 * It is fine and correct for brng_generation and brng_buffer to be
	 * zero values.
	 *
	 * brng_prf and brng_generation must be initialized later.
	 * Initialization is special for the root BRNG.  PCPU child instances
	 * use fxrng_brng_produce_seed_data_internal() below.
	 */
}

/*
 * Directly reseed the root BRNG from a first-time entropy source,
 * incorporating the existing BRNG state.  The main motivation for doing so "is
 * to ensure that as soon as an entropy source produces data, PRNG output
 * depends on the data from that source." (§ 3.1)
 *
 * The root BRNG is locked on entry and initial keying (brng_generation > 0)
 * has already been performed.  The root BRNG is unlocked on return.
 */
void
fxrng_brng_src_reseed(const struct harvest_event *event)
{
	struct fxrng_buffered_rng *rng;

	rng = &fxrng_root;
	FXRNG_BRNG_ASSERT(rng);
	ASSERT_DEBUG(rng->brng_generation > 0, "root RNG not seeded");

	fxrng_rng_src_reseed(&rng->brng_rng, event);
	FXRNG_BRNG_ASSERT(rng);

	/*
	 * Bump root generation (which is costly) to force downstream BRNGs to
	 * reseed and quickly incorporate the new entropy.  The intuition is
	 * that this tradeoff is worth it because new sources show up extremely
	 * rarely (limiting cost) and if they can contribute any entropy to a
	 * weak state, we want to propagate it to all generators ASAP.
	 */
	rng->brng_generation++;
	atomic_store_rel_64(&fxrng_root_generation, rng->brng_generation);
	/* Update VDSO version. */
	fxrng_push_seed_generation(rng->brng_generation);
	FXRNG_BRNG_UNLOCK(rng);
}

/*
 * Reseed a brng from some amount of pooled entropy (determined in fx_pool.c by
 * fxent_timer_reseed_npools).  For initial seeding, we pool entropy in a
 * single pool and use this API as well (fxrng_alg_seeded).
 */
void
fxrng_brng_reseed(const void *entr, size_t sz)
{
	struct fxrng_buffered_rng *rng;

	rng = &fxrng_root;
	FXRNG_BRNG_LOCK(rng);

	fxrng_rng_reseed(&rng->brng_rng, (rng->brng_generation > 0), entr, sz);
	FXRNG_BRNG_ASSERT(rng);

	rng->brng_generation++;
	atomic_store_rel_64(&fxrng_root_generation, rng->brng_generation);
	/* Update VDSO version. */
	fxrng_push_seed_generation(rng->brng_generation);
	FXRNG_BRNG_UNLOCK(rng);
}

/*
 * Sysentvec and VDSO are initialized much later than SI_SUB_RANDOM.  When
 * they're online, go ahead and push an initial root seed version.
 * INIT_SYSENTVEC runs at SI_SUB_EXEC:SI_ORDER_ANY, and SI_ORDER_ANY is the
 * maximum value, so we must run at SI_SUB_EXEC+1.
 */
static void
fxrng_vdso_sysinit(void *dummy __unused)
{
	FXRNG_BRNG_LOCK(&fxrng_root);
	fxrng_push_seed_generation(fxrng_root.brng_generation);
	FXRNG_BRNG_UNLOCK(&fxrng_root);
}
SYSINIT(fxrng_vdso, SI_SUB_EXEC + 1, SI_ORDER_ANY, fxrng_vdso_sysinit, NULL);

/*
 * Grab some bytes off an initialized, current generation RNG.
 *
 * (Does not handle reseeding if our generation is stale.)
 *
 * Locking protocol is a bit odd.  The RNG is locked on entrance, but the lock
 * is dropped on exit.  This avoids holding a lock during expensive and slow
 * RNG generation.
 */
static void
fxrng_brng_getbytes_internal(struct fxrng_buffered_rng *rng, void *buf,
    size_t nbytes)
{

	FXRNG_BRNG_ASSERT(rng);

	/* Make the zero request impossible for the rest of the logic. */
	if (__predict_false(nbytes == 0)) {
		FXRNG_BRNG_UNLOCK(rng);
		goto out;
	}

	/* Fast/easy case: Use some bytes from the buffer. */
	if (rng->brng_avail_idx + nbytes <= sizeof(rng->brng_buffer)) {
		memcpy(buf, &rng->brng_buffer[rng->brng_avail_idx], nbytes);
		explicit_bzero(&rng->brng_buffer[rng->brng_avail_idx], nbytes);
		rng->brng_avail_idx += nbytes;
		FXRNG_BRNG_UNLOCK(rng);
		goto out;
	}

	/* Buffer case: */
	if (nbytes < sizeof(rng->brng_buffer)) {
		size_t rem;

		/* Drain anything left in the buffer first. */
		if (rng->brng_avail_idx < sizeof(rng->brng_buffer)) {
			rem = sizeof(rng->brng_buffer) - rng->brng_avail_idx;
			ASSERT_DEBUG(nbytes > rem, "invariant");

			memcpy(buf, &rng->brng_buffer[rng->brng_avail_idx], rem);

			buf = (uint8_t*)buf + rem;
			nbytes -= rem;
			ASSERT_DEBUG(nbytes != 0, "invariant");
		}

		/*
		 * Partial fill from first buffer, have to rekey and generate a
		 * new buffer to do the rest.
		 */
		fxrng_rng_genrandom_internal(&rng->brng_rng, rng->brng_buffer,
		    sizeof(rng->brng_buffer), false);
		FXRNG_BRNG_ASSERT(rng);
		rng->brng_avail_idx = 0;

		memcpy(buf, &rng->brng_buffer[rng->brng_avail_idx], nbytes);
		explicit_bzero(&rng->brng_buffer[rng->brng_avail_idx], nbytes);
		rng->brng_avail_idx += nbytes;
		FXRNG_BRNG_UNLOCK(rng);
		goto out;
	}

	/* Large request; skip the buffer. */
	fxrng_rng_genrandom_internal(&rng->brng_rng, buf, nbytes, true);

out:
	FXRNG_BRNG_ASSERT_NOT(rng);
	return;
}

/*
 * API to get a new key for a downstream RNG.  Returns the new key in 'buf', as
 * well as the generator's reseed_generation.
 *
 * 'rng' is locked on entry and unlocked on return.
 *
 * Only valid after confirming the caller's seed version or reseed_generation
 * matches roots (or we are root).  (For now, this is only used to reseed the
 * per-CPU generators from root.)
 */
void
fxrng_brng_produce_seed_data_internal(struct fxrng_buffered_rng *rng,
    void *buf, size_t keysz, uint64_t *seed_generation)
{
	FXRNG_BRNG_ASSERT(rng);
	ASSERT_DEBUG(keysz == FX_CHACHA20_KEYSIZE, "keysz: %zu", keysz);

	*seed_generation = rng->brng_generation;
	fxrng_brng_getbytes_internal(rng, buf, keysz);
	FXRNG_BRNG_ASSERT_NOT(rng);
}

/*
 * Read from an allocated and initialized buffered BRNG.  This a high-level
 * API, but doesn't handle PCPU BRNG allocation.
 *
 * BRNG is locked on entry.  It is unlocked on return.
 */
void
fxrng_brng_read(struct fxrng_buffered_rng *rng, void *buf, size_t nbytes)
{
	uint8_t newkey[FX_CHACHA20_KEYSIZE];

	FXRNG_BRNG_ASSERT(rng);

	/* Fast path: there hasn't been a global reseed since last read. */
	if (rng->brng_generation == atomic_load_acq_64(&fxrng_root_generation))
		goto done_reseeding;

	ASSERT(rng != &fxrng_root, "root rng inconsistent seed version");

	/*
	 * Slow path: We need to rekey from the parent BRNG to incorporate new
	 * entropy material.
	 *
	 * Lock order is always root -> percpu.
	 */
	FXRNG_BRNG_UNLOCK(rng);
	FXRNG_BRNG_LOCK(&fxrng_root);
	FXRNG_BRNG_LOCK(rng);

	/*
	 * If we lost the reseeding race when the lock was dropped, don't
	 * duplicate work.
	 */
	if (__predict_false(rng->brng_generation ==
	    atomic_load_acq_64(&fxrng_root_generation))) {
		FXRNG_BRNG_UNLOCK(&fxrng_root);
		goto done_reseeding;
	}

	fxrng_brng_produce_seed_data_internal(&fxrng_root, newkey,
	    sizeof(newkey), &rng->brng_generation);

	FXRNG_BRNG_ASSERT_NOT(&fxrng_root);
	FXRNG_BRNG_ASSERT(rng);

	fxrng_rng_setkey(&rng->brng_rng, newkey, sizeof(newkey));
	explicit_bzero(newkey, sizeof(newkey));

	/*
	 * A reseed invalidates any previous buffered contents.  Here, we
	 * forward the available index to the end of the buffer, i.e., empty.
	 * Requests that would use the buffer (< 128 bytes) will refill its
	 * contents on demand.
	 *
	 * It is explicitly ok that we do not zero out any remaining buffer
	 * bytes; they will never be handed out to callers, and they reveal
	 * nothing about the reseeded key (which came from the root BRNG).
	 * (§ 1.3)
	 */
	rng->brng_avail_idx = sizeof(rng->brng_buffer);

done_reseeding:
	if (rng != &fxrng_root)
		FXRNG_BRNG_ASSERT_NOT(&fxrng_root);
	FXRNG_BRNG_ASSERT(rng);

	fxrng_brng_getbytes_internal(rng, buf, nbytes);
	FXRNG_BRNG_ASSERT_NOT(rng);
}
