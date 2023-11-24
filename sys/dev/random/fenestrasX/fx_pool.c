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
#include <sys/domainset.h>
#include <sys/fail.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/random.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <machine/atomic.h>
#include <machine/smp.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>

#include <dev/random/fenestrasX/fx_brng.h>
#include <dev/random/fenestrasX/fx_hash.h>
#include <dev/random/fenestrasX/fx_pool.h>
#include <dev/random/fenestrasX/fx_priv.h>
#include <dev/random/fenestrasX/fx_pub.h>

/*
 * Timer-based reseed interval growth factor and limit in seconds. (§ 3.2)
 */
#define	FXENT_RESSED_INTVL_GFACT	3
#define	FXENT_RESEED_INTVL_MAX		3600

/*
 * Pool reseed schedule.  Initially, only pool 0 is active.  Until the timer
 * interval reaches INTVL_MAX, only pool 0 is used.
 *
 * After reaching INTVL_MAX, pool k is either activated (if inactive) or used
 * (if active) every 3^k timer reseeds.  (§ 3.3)
 *
 * (Entropy harvesting only round robins across active pools.)
 */
#define	FXENT_RESEED_BASE		3

/*
 * Number of bytes from high quality sources to allocate to pool 0 before
 * normal round-robin allocation after each timer reseed. (§ 3.4)
 */
#define	FXENT_HI_SRC_POOL0_BYTES	32

/*
 * § 3.1
 *
 * Low sources provide unconditioned entropy, such as mouse movements; high
 * sources are assumed to provide high-quality random bytes.  Pull sources are
 * those which can be polled, i.e., anything randomdev calls a "random_source."
 *
 * In the whitepaper, low sources are pull.  For us, at least in the existing
 * design, low-quality sources push into some global ring buffer and then get
 * forwarded into the RNG by a thread that continually polls.  Presumably their
 * design batches low entopy signals in some way (SHA512?) and only requests
 * them dynamically on reseed.  I'm not sure what the benefit is vs feeding
 * into the pools directly.
 */
enum fxrng_ent_access_cls {
	FXRNG_PUSH,
	FXRNG_PULL,
};
enum fxrng_ent_source_cls {
	FXRNG_HI,
	FXRNG_LO,
	FXRNG_GARBAGE,
};
struct fxrng_ent_cls {
	enum fxrng_ent_access_cls	entc_axx_cls;
	enum fxrng_ent_source_cls	entc_src_cls;
};

static const struct fxrng_ent_cls fxrng_hi_pull = {
	.entc_axx_cls = FXRNG_PULL,
	.entc_src_cls = FXRNG_HI,
};
static const struct fxrng_ent_cls fxrng_hi_push = {
	.entc_axx_cls = FXRNG_PUSH,
	.entc_src_cls = FXRNG_HI,
};
static const struct fxrng_ent_cls fxrng_lo_push = {
	.entc_axx_cls = FXRNG_PUSH,
	.entc_src_cls = FXRNG_LO,
};
static const struct fxrng_ent_cls fxrng_garbage = {
	.entc_axx_cls = FXRNG_PUSH,
	.entc_src_cls = FXRNG_GARBAGE,
};

/*
 * This table is a mapping of randomdev's current source abstractions to the
 * designations above; at some point, if the design seems reasonable, it would
 * make more sense to pull this up into the abstraction layer instead.
 */
static const struct fxrng_ent_char {
	const struct fxrng_ent_cls	*entc_cls;
} fxrng_ent_char[ENTROPYSOURCE] = {
	[RANDOM_CACHED] = {
		.entc_cls = &fxrng_hi_push,
	},
	[RANDOM_ATTACH] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_KEYBOARD] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_MOUSE] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_NET_TUN] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_NET_ETHER] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_NET_NG] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_INTERRUPT] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_SWI] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_FS_ATIME] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_UMA] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_CALLOUT] = {
		.entc_cls = &fxrng_lo_push,
	},
	[RANDOM_PURE_OCTEON] = {
		.entc_cls = &fxrng_hi_push,	/* Could be made pull. */
	},
	[RANDOM_PURE_SAFE] = {
		.entc_cls = &fxrng_hi_push,
	},
	[RANDOM_PURE_GLXSB] = {
		.entc_cls = &fxrng_hi_push,
	},
	[RANDOM_PURE_HIFN] = {
		.entc_cls = &fxrng_hi_push,
	},
	[RANDOM_PURE_RDRAND] = {
		.entc_cls = &fxrng_hi_pull,
	},
	[RANDOM_PURE_NEHEMIAH] = {
		.entc_cls = &fxrng_hi_pull,
	},
	[RANDOM_PURE_RNDTEST] = {
		.entc_cls = &fxrng_garbage,
	},
	[RANDOM_PURE_VIRTIO] = {
		.entc_cls = &fxrng_hi_pull,
	},
	[RANDOM_PURE_BROADCOM] = {
		.entc_cls = &fxrng_hi_push,
	},
	[RANDOM_PURE_CCP] = {
		.entc_cls = &fxrng_hi_pull,
	},
	[RANDOM_PURE_DARN] = {
		.entc_cls = &fxrng_hi_pull,
	},
	[RANDOM_PURE_TPM] = {
		.entc_cls = &fxrng_hi_push,
	},
	[RANDOM_PURE_VMGENID] = {
		.entc_cls = &fxrng_hi_push,
	},
};

/* Useful for single-bit-per-source state. */
BITSET_DEFINE(fxrng_bits, ENTROPYSOURCE);

/* XXX Borrowed from not-yet-committed D22702. */
#ifndef BIT_TEST_SET_ATOMIC_ACQ
#define	BIT_TEST_SET_ATOMIC_ACQ(_s, n, p)	\
	(atomic_testandset_acq_long(		\
	    &(p)->__bits[__bitset_word((_s), (n))], (n)) != 0)
#endif
#define	FXENT_TEST_SET_ATOMIC_ACQ(n, p) \
	BIT_TEST_SET_ATOMIC_ACQ(ENTROPYSOURCE, n, p)

/* For special behavior on first-time entropy sources. (§ 3.1) */
static struct fxrng_bits __read_mostly fxrng_seen;

/* For special behavior for high-entropy sources after a reseed. (§ 3.4) */
_Static_assert(FXENT_HI_SRC_POOL0_BYTES <= UINT8_MAX, "");
static uint8_t __read_mostly fxrng_reseed_seen[ENTROPYSOURCE];

/* Entropy pools.  Lock order is ENT -> RNG(root) -> RNG(leaf). */
static struct mtx fxent_pool_lk;
MTX_SYSINIT(fx_pool, &fxent_pool_lk, "fx entropy pool lock", MTX_DEF);
#define	FXENT_LOCK()		mtx_lock(&fxent_pool_lk)
#define	FXENT_UNLOCK()		mtx_unlock(&fxent_pool_lk)
#define	FXENT_ASSERT(rng)	mtx_assert(&fxent_pool_lk, MA_OWNED)
#define	FXENT_ASSERT_NOT(rng)	mtx_assert(&fxent_pool_lk, MA_NOTOWNED)
static struct fxrng_hash fxent_pool[FXRNG_NPOOLS];
static unsigned __read_mostly fxent_nactpools = 1;
static struct timeout_task fxent_reseed_timer;
static int __read_mostly fxent_timer_ready;

/*
 * Track number of bytes of entropy harvested from high-quality sources prior
 * to initial keying.  The idea is to collect more jitter entropy when fewer
 * high-quality bytes were available and less if we had other good sources.  We
 * want to provide always-on availability but don't necessarily have *any*
 * great sources on some platforms.
 *
 * Like fxrng_ent_char: at some point, if the design seems reasonable, it would
 * make more sense to pull this up into the abstraction layer instead.
 *
 * Jitter entropy is unimplemented for now.
 */
static unsigned long fxrng_preseed_ent;

void
fxrng_pools_init(void)
{
	size_t i;

	for (i = 0; i < nitems(fxent_pool); i++)
		fxrng_hash_init(&fxent_pool[i]);
}

static inline bool
fxrng_hi_source(enum random_entropy_source src)
{
	return (fxrng_ent_char[src].entc_cls->entc_src_cls == FXRNG_HI);
}

/*
 * A racy check that this high-entropy source's event should contribute to
 * pool0 on the basis of per-source byte count.  The check is racy for two
 * reasons:
 *   - Performance: The vast majority of the time, we've already taken 32 bytes
 *     from any present high quality source and the racy check lets us avoid
 *     dirtying the cache for the global array.
 *   - Correctness: It's fine that the check is racy.  The failure modes are:
 *     • False positive: We will detect when we take the lock.
 *     • False negative: We still collect the entropy; it just won't be
 *       preferentially placed in pool0 in this case.
 */
static inline bool
fxrng_hi_pool0_eligible_racy(enum random_entropy_source src)
{
	return (atomic_load_acq_8(&fxrng_reseed_seen[src]) <
	    FXENT_HI_SRC_POOL0_BYTES);
}

/*
 * Top level entropy processing API from randomdev.
 *
 * Invoked by the core randomdev subsystem both for preload entropy, "push"
 * sources (like interrupts, keyboard, etc) and pull sources (RDRAND, etc).
 */
void
fxrng_event_processor(struct harvest_event *event)
{
	enum random_entropy_source src;
	unsigned pool;
	bool first_time, first_32;

	src = event->he_source;

	ASSERT_DEBUG(event->he_size <= sizeof(event->he_entropy),
	    "%s: he_size: %u > sizeof(he_entropy): %zu", __func__,
	    (unsigned)event->he_size, sizeof(event->he_entropy));

	/*
	 * Zero bytes of source entropy doesn't count as observing this source
	 * for the first time.  We still harvest the counter entropy.
	 */
	first_time = event->he_size > 0 &&
	    !FXENT_TEST_SET_ATOMIC_ACQ(src, &fxrng_seen);
	if (__predict_false(first_time)) {
		/*
		 * "The first time [any source] provides entropy, it is used to
		 * directly reseed the root PRNG.  The entropy pools are
		 * bypassed." (§ 3.1)
		 *
		 * Unlike Windows, we cannot rely on loader(8) seed material
		 * being present, so we perform initial keying in the kernel.
		 * We use brng_generation 0 to represent an unkeyed state.
		 *
		 * Prior to initial keying, it doesn't make sense to try to mix
		 * the entropy directly with the root PRNG state, as the root
		 * PRNG is unkeyed.  Instead, we collect pre-keying dynamic
		 * entropy in pool0 and do not bump the root PRNG seed version
		 * or set its key.  Initial keying will incorporate pool0 and
		 * bump the brng_generation (seed version).
		 *
		 * After initial keying, we do directly mix in first-time
		 * entropy sources.  We use the root BRNG to generate 32 bytes
		 * and use fxrng_hash to mix it with the new entropy source and
		 * re-key with the first 256 bits of hash output.
		 */
		FXENT_LOCK();
		FXRNG_BRNG_LOCK(&fxrng_root);
		if (__predict_true(fxrng_root.brng_generation > 0)) {
			/* Bypass the pools: */
			FXENT_UNLOCK();
			fxrng_brng_src_reseed(event);
			FXRNG_BRNG_ASSERT_NOT(&fxrng_root);
			return;
		}

		/*
		 * Keying the root PRNG requires both FXENT_LOCK and the PRNG's
		 * lock, so we only need to hold on to the pool lock to prevent
		 * initial keying without this entropy.
		 */
		FXRNG_BRNG_UNLOCK(&fxrng_root);

		/* Root PRNG hasn't been keyed yet, just accumulate event. */
		fxrng_hash_update(&fxent_pool[0], &event->he_somecounter,
		    sizeof(event->he_somecounter));
		fxrng_hash_update(&fxent_pool[0], event->he_entropy,
		    event->he_size);

		if (fxrng_hi_source(src)) {
			/* Prevent overflow. */
			if (fxrng_preseed_ent <= ULONG_MAX - event->he_size)
				fxrng_preseed_ent += event->he_size;
		}
		FXENT_UNLOCK();
		return;
	}
	/* !first_time */

	/*
	 * "The first 32 bytes produced by a high entropy source after a reseed
	 * from the pools is always put in pool 0." (§ 3.4)
	 *
	 * The first-32-byte tracking data in fxrng_reseed_seen is reset in
	 * fxent_timer_reseed_npools() below.
	 */
	first_32 = event->he_size > 0 &&
	    fxrng_hi_source(src) &&
	    atomic_load_acq_int(&fxent_nactpools) > 1 &&
	    fxrng_hi_pool0_eligible_racy(src);
	if (__predict_false(first_32)) {
		unsigned rem, seen;

		FXENT_LOCK();
		seen = fxrng_reseed_seen[src];
		if (seen == FXENT_HI_SRC_POOL0_BYTES)
			goto round_robin;

		rem = FXENT_HI_SRC_POOL0_BYTES - seen;
		rem = MIN(rem, event->he_size);

		fxrng_reseed_seen[src] = seen + rem;

		/*
		 * We put 'rem' bytes in pool0, and any remaining bytes are
		 * round-robin'd across other pools.
		 */
		fxrng_hash_update(&fxent_pool[0],
		    ((uint8_t *)event->he_entropy) + event->he_size - rem,
		    rem);
		if (rem == event->he_size) {
			fxrng_hash_update(&fxent_pool[0], &event->he_somecounter,
			    sizeof(event->he_somecounter));
			FXENT_UNLOCK();
			return;
		}

		/*
		 * If fewer bytes were needed than this even provied, We only
		 * take the last rem bytes of the entropy buffer and leave the
		 * timecounter to be round-robin'd with the remaining entropy.
		 */
		event->he_size -= rem;
		goto round_robin;
	}
	/* !first_32 */

	FXENT_LOCK();

round_robin:
	FXENT_ASSERT();
	pool = event->he_destination % fxent_nactpools;
	fxrng_hash_update(&fxent_pool[pool], event->he_entropy,
	    event->he_size);
	fxrng_hash_update(&fxent_pool[pool], &event->he_somecounter,
	    sizeof(event->he_somecounter));

	if (__predict_false(fxrng_hi_source(src) &&
	    atomic_load_acq_64(&fxrng_root_generation) == 0)) {
		/* Prevent overflow. */
		if (fxrng_preseed_ent <= ULONG_MAX - event->he_size)
			fxrng_preseed_ent += event->he_size;
	}
	FXENT_UNLOCK();
}

/*
 * Top level "seeded" API/signal from randomdev.
 *
 * This is our warning that a request is coming: we need to be seeded.  In
 * fenestrasX, a request for random bytes _never_ fails.  "We (ed: ditto) have
 * observed that there are many callers that never check for the error code,
 * even if they are generating cryptographic key material." (§ 1.6)
 *
 * If we returned 'false', both read_random(9) and chacha20_randomstir()
 * (arc4random(9)) will blindly charge on with something almost certainly worse
 * than what we've got, or are able to get quickly enough.
 */
bool
fxrng_alg_seeded(void)
{
	uint8_t hash[FXRNG_HASH_SZ];
	sbintime_t sbt;

	/* The vast majority of the time, we expect to already be seeded. */
	if (__predict_true(atomic_load_acq_64(&fxrng_root_generation) != 0))
		return (true);

	/*
	 * Take the lock and recheck; only one thread needs to do the initial
	 * seeding work.
	 */
	FXENT_LOCK();
	if (atomic_load_acq_64(&fxrng_root_generation) != 0) {
		FXENT_UNLOCK();
		return (true);
	}
	/* XXX Any one-off initial seeding goes here. */

	fxrng_hash_finish(&fxent_pool[0], hash, sizeof(hash));
	fxrng_hash_init(&fxent_pool[0]);

	fxrng_brng_reseed(hash, sizeof(hash));
	FXENT_UNLOCK();

	randomdev_unblock();
	explicit_bzero(hash, sizeof(hash));

	/*
	 * This may be called too early for taskqueue_thread to be initialized.
	 * fxent_pool_timer_init will detect if we've already unblocked and
	 * queue the first timer reseed at that point.
	 */
	if (atomic_load_acq_int(&fxent_timer_ready) != 0) {
		sbt = SBT_1S;
		taskqueue_enqueue_timeout_sbt(taskqueue_thread,
		    &fxent_reseed_timer, -sbt, (sbt / 3), C_PREL(2));
	}
	return (true);
}

/*
 * Timer-based reseeds and pool expansion.
 */
static void
fxent_timer_reseed_npools(unsigned n)
{
	/*
	 * 64 * 8 => moderately large 512 bytes.  Could be static, as we are
	 * only used in a static context.  On the other hand, this is in
	 * threadqueue TASK context and we're likely nearly at top of stack
	 * already.
	 */
	uint8_t hash[FXRNG_HASH_SZ * FXRNG_NPOOLS];
	unsigned i;

	ASSERT_DEBUG(n > 0 && n <= FXRNG_NPOOLS, "n:%u", n);

	FXENT_ASSERT();
	/*
	 * Collect entropy from pools 0..n-1 by concatenating the output hashes
	 * and then feeding them into fxrng_brng_reseed, which will hash the
	 * aggregate together with the current root PRNG keystate to produce a
	 * new key.  It will also bump the global generation counter
	 * appropriately.
	 */
	for (i = 0; i < n; i++) {
		fxrng_hash_finish(&fxent_pool[i], hash + i * FXRNG_HASH_SZ,
		    FXRNG_HASH_SZ);
		fxrng_hash_init(&fxent_pool[i]);
	}

	fxrng_brng_reseed(hash, n * FXRNG_HASH_SZ);
	explicit_bzero(hash, n * FXRNG_HASH_SZ);

	/*
	 * "The first 32 bytes produced by a high entropy source after a reseed
	 * from the pools is always put in pool 0." (§ 3.4)
	 *
	 * So here we reset the tracking (somewhat naively given the majority
	 * of sources on most machines are not what we consider "high", but at
	 * 32 bytes it's smaller than a cache line), so the next 32 bytes are
	 * prioritized into pool0.
	 *
	 * See corresponding use of fxrng_reseed_seen in fxrng_event_processor.
	 */
	memset(fxrng_reseed_seen, 0, sizeof(fxrng_reseed_seen));
	FXENT_ASSERT();
}

static void
fxent_timer_reseed(void *ctx __unused, int pending __unused)
{
	static unsigned reseed_intvl_sec = 1;
	/* Only reseeds after FXENT_RESEED_INTVL_MAX is achieved. */
	static uint64_t reseed_number = 1;

	unsigned next_ival, i, k;
	sbintime_t sbt;

	if (reseed_intvl_sec < FXENT_RESEED_INTVL_MAX) {
		next_ival = FXENT_RESSED_INTVL_GFACT * reseed_intvl_sec;
		if (next_ival > FXENT_RESEED_INTVL_MAX)
			next_ival = FXENT_RESEED_INTVL_MAX;
		FXENT_LOCK();
		fxent_timer_reseed_npools(1);
		FXENT_UNLOCK();
	} else {
		/*
		 * The creation of entropy pools beyond 0 is enabled when the
		 * reseed interval hits the maximum. (§ 3.3)
		 */
		next_ival = reseed_intvl_sec;

		/*
		 * Pool 0 is used every reseed; pool 1..0 every 3rd reseed; and in
		 * general, pool n..0 every 3^n reseeds.
		 */
		k = reseed_number;
		reseed_number++;

		/* Count how many pools, from [0, i), to use for reseed. */
		for (i = 1; i < MIN(fxent_nactpools + 1, FXRNG_NPOOLS); i++) {
			if ((k % FXENT_RESEED_BASE) != 0)
				break;
			k /= FXENT_RESEED_BASE;
		}

		/*
		 * If we haven't activated pool i yet, activate it and only
		 * reseed from [0, i-1).  (§ 3.3)
		 */
		FXENT_LOCK();
		if (i == fxent_nactpools + 1) {
			fxent_timer_reseed_npools(fxent_nactpools);
			fxent_nactpools++;
		} else {
			/* Just reseed from [0, i). */
			fxent_timer_reseed_npools(i);
		}
		FXENT_UNLOCK();
	}

	/* Schedule the next reseed. */
	sbt = next_ival * SBT_1S;
	taskqueue_enqueue_timeout_sbt(taskqueue_thread, &fxent_reseed_timer,
	    -sbt, (sbt / 3), C_PREL(2));

	reseed_intvl_sec = next_ival;
}

static void
fxent_pool_timer_init(void *dummy __unused)
{
	sbintime_t sbt;

	TIMEOUT_TASK_INIT(taskqueue_thread, &fxent_reseed_timer, 0,
	    fxent_timer_reseed, NULL);

	if (atomic_load_acq_64(&fxrng_root_generation) != 0) {
		sbt = SBT_1S;
		taskqueue_enqueue_timeout_sbt(taskqueue_thread,
		    &fxent_reseed_timer, -sbt, (sbt / 3), C_PREL(2));
	}
	atomic_store_rel_int(&fxent_timer_ready, 1);
}
/* After taskqueue_thread is initialized in SI_SUB_TASKQ:SI_ORDER_SECOND. */
SYSINIT(fxent_pool_timer_init, SI_SUB_TASKQ, SI_ORDER_ANY,
    fxent_pool_timer_init, NULL);
