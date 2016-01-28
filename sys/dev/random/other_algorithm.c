/*-
 * Copyright (c) 2015 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*-
 * This is a skeleton for folks who wish to build a loadable module
 * containing an alternative entropy-processing algorithm for random(4).
 *
 * The functions below should be completed with the appropriate code,
 * and the nearby yarrow.c and fortuna.c may be consulted for examples
 * of working code.
 *
 * The author is willing to provide reasonable help to those wishing to
 * write such a module for themselves. Please use the markm@ FreeBSD
 * email address, and ensure that you are developing this on a suitably
 * supported branch (This is currently 11-CURRENT, and will be no
 * older than 11-STABLE in the future).
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpu.h>

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha256.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/uint128.h>
#include <dev/random/other_algorithm.h>

static void random_other_pre_read(void);
static void random_other_read(uint8_t *, u_int);
static bool random_other_seeded(void);
static void random_other_process_event(struct harvest_event *);
static void random_other_init_alg(void *);
static void random_other_deinit_alg(void *);

/*
 * RANDOM_OTHER_NPOOLS is used when reading hardware random
 * number sources to ensure that each pool gets one read sample
 * per loop iteration. Yarrow has 2 such pools (FAST and SLOW),
 * and fortuna has 32 (0-31). The RNG used prior to Yarrow and
 * ported from Linux had just 1 pool.
 */
#define RANDOM_OTHER_NPOOLS 1

struct random_algorithm random_alg_context = {
	.ra_ident = "other",
	.ra_init_alg = random_other_init_alg,
	.ra_deinit_alg = random_other_deinit_alg,
	.ra_pre_read = random_other_pre_read,
	.ra_read = random_other_read,
	.ra_seeded = random_other_seeded,
	.ra_event_processor = random_other_process_event,
	.ra_poolcount = RANDOM_OTHER_NPOOLS,
};

/* Use a mutex to protect your reseed variables? */
static mtx_t other_mtx;

/*
 * void random_other_init_alg(void *unused __unused)
 *
 * Do algorithm-specific initialisation here.
 */
void
random_other_init_alg(void *unused __unused)
{

	RANDOM_RESEED_INIT_LOCK();
	/*
	 * Do set-up work here!
	 */
}

/*
 * void random_other_deinit_alg(void *unused __unused)
 *
 * Do algorithm-specific deinitialisation here.
 */
static void
random_other_deinit_alg(void *unused __unused)
{

	/*
	 * Do tear-down work here!
	 */
	RANDOM_RESEED_DEINIT_LOCK();
}

/*
 * void random_other_pre_read(void)
 *
 * Do any pre-read preparation you need to. This will be called
 * before >=1 calls to random_other_read() corresponding to one
 * read(2).
 *
 * This routine will be called periodically while the generator is
 * still blocked and a read is being attempted, giving you an
 * opportunity to unblock.
 */
static void
random_other_pre_read(void)
{

	RANDOM_RESEED_LOCK();
	/*
	 * Do pre-read housekeeping work here!
	 * You may use this as a chance to unblock the generator.
	 */
	RANDOM_RESEED_UNLOCK();
}

/*
 * void random_other_read(uint8_t *buf, u_int count)
 *
 * Generate <count> bytes of output into <*buf>.
 * You may use the fact that <count> will be a multiple of
 * RANDOM_BLOCKSIZE for optimization purposes.
 *
 * This function will always be called with your generator
 * unblocked and ready. If you are not ready to generate
 * output here, then feel free to KASSERT() or panic().
 */
static void
random_other_read(uint8_t *buf, u_int count)
{

	RANDOM_RESEED_LOCK();
	/*
	 * Do random-number generation work here!
	 */
	RANDOM_RESEED_UNLOCK();
}

/*
 * bool random_other_seeded(void)
 *
 * Return true if your generator is ready to generate
 * output, and false otherwise.
 */
static bool
random_other_seeded(void)
{
	bool seeded = false;

	/*
	 * Find out if your generator is seeded here!
	 */
	return (seeded);
}

/*
 * void random_other_process_event(struct harvest_event *event)
 *
 * Process one stochastic event <*event> into your entropy
 * processor.
 *
 * The structure of the event may change, so it is easier to
 * just grab the whole thing into your accumulation system.
 * You may pick-and-choose bits, but please don't complain
 * when/if these change.
 */
static void
random_other_process_event(struct harvest_event *event)
{

	RANDOM_RESEED_LOCK();
	/*
	 * Do entropy accumulation work here!
	 * You may use this as a chance to unblock the generator.
	 */
	RANDOM_RESEED_UNLOCK();
}
