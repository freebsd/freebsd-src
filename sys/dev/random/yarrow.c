/*-
 * Copyright (c) 2000-2015 Mark R V Murray
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpu.h>

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/uint128.h>
#include <dev/random/yarrow.h>
#else /* !_KERNEL */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <threads.h>

#include "unit_test.h"

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/uint128.h>
#include <dev/random/yarrow.h>
#endif /* _KERNEL */

#define	RANDOM_YARROW_TIMEBIN	16	/* max value for Pt/t */

#define	RANDOM_YARROW_FAST	0
#define	RANDOM_YARROW_SLOW	1
#define	RANDOM_YARROW_NPOOLS	2

/* This algorithm (and code) presumes that RANDOM_KEYSIZE is twice as large as RANDOM_BLOCKSIZE */
CTASSERT(RANDOM_BLOCKSIZE == sizeof(uint128_t));
CTASSERT(RANDOM_KEYSIZE == 2*RANDOM_BLOCKSIZE);

/*
 * This is the beastie that needs protecting. It contains all of the
 * state that we are excited about. Exactly one is instantiated.
 */
static struct yarrow_state {
	uint128_t ys_counter;		/* C */
	struct randomdev_key ys_key;	/* K */
	u_int ys_gengateinterval;	/* Pg */
	u_int ys_bins;			/* Pt/t */
	u_int ys_outputblocks;		/* count output blocks for gates */
	u_int ys_slowoverthresh;	/* slow pool overthreshhold reseed count */
	struct ys_pool {
		u_int ysp_source_bits[ENTROPYSOURCE];	/* estimated bits of entropy per source */
		u_int ysp_thresh;	/* pool reseed threshhold */
		struct randomdev_hash ysp_hash;	/* accumulated entropy */
	} ys_pool[RANDOM_YARROW_NPOOLS];/* pool[0] is fast, pool[1] is slow */
	bool ys_seeded;
	/* Reseed lock */
	mtx_t ys_mtx;
} yarrow_state;

#ifdef _KERNEL
static struct sysctl_ctx_list random_clist;
RANDOM_CHECK_UINT(gengateinterval, 4, 64);
RANDOM_CHECK_UINT(bins, RANDOM_YARROW_NPOOLS, 16);
RANDOM_CHECK_UINT(fastthresh, (RANDOM_BLOCKSIZE*8)/4, (RANDOM_BLOCKSIZE*8)); /* Bit counts */
RANDOM_CHECK_UINT(slowthresh, (RANDOM_BLOCKSIZE*8)/4, (RANDOM_BLOCKSIZE*8)); /* Bit counts */
RANDOM_CHECK_UINT(slowoverthresh, 1, 5);
#endif /* _KERNEL */

static void random_yarrow_pre_read(void);
static void random_yarrow_read(uint8_t *, u_int);
static bool random_yarrow_seeded(void);
static void random_yarrow_process_event(struct harvest_event *);
static void random_yarrow_init_alg(void *);
static void random_yarrow_deinit_alg(void *);

static void random_yarrow_reseed_internal(u_int);

struct random_algorithm random_alg_context = {
	.ra_ident = "Yarrow",
	.ra_init_alg = random_yarrow_init_alg,
	.ra_deinit_alg = random_yarrow_deinit_alg,
	.ra_pre_read = random_yarrow_pre_read,
	.ra_read = random_yarrow_read,
	.ra_seeded = random_yarrow_seeded,
	.ra_event_processor = random_yarrow_process_event,
	.ra_poolcount = RANDOM_YARROW_NPOOLS,
};

/* ARGSUSED */
static void
random_yarrow_init_alg(void *unused __unused)
{
	int i, j;
#ifdef _KERNEL
	struct sysctl_oid *random_yarrow_o;
#endif

	RANDOM_RESEED_INIT_LOCK();
	/* Start unseeded, therefore blocked. */
	yarrow_state.ys_seeded = false;
#ifdef _KERNEL
	/*
	 * Yarrow parameters. Do not adjust these unless you have
	 * have a very good clue about what they do!
	 */
	random_yarrow_o = SYSCTL_ADD_NODE(&random_clist,
		SYSCTL_STATIC_CHILDREN(_kern_random),
		OID_AUTO, "yarrow", CTLFLAG_RW, 0,
		"Yarrow Parameters");
	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"gengateinterval", CTLTYPE_UINT | CTLFLAG_RWTUN,
		&yarrow_state.ys_gengateinterval, 0,
		random_check_uint_gengateinterval, "UI",
		"Generation gate interval");
	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"bins", CTLTYPE_UINT | CTLFLAG_RWTUN,
		&yarrow_state.ys_bins, 0,
		random_check_uint_bins, "UI",
		"Execution time tuner");
	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"fastthresh", CTLTYPE_UINT | CTLFLAG_RWTUN,
		&yarrow_state.ys_pool[0].ysp_thresh, 0,
		random_check_uint_fastthresh, "UI",
		"Fast reseed threshold");
	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"slowthresh", CTLTYPE_UINT | CTLFLAG_RWTUN,
		&yarrow_state.ys_pool[1].ysp_thresh, 0,
		random_check_uint_slowthresh, "UI",
		"Slow reseed threshold");
	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"slowoverthresh", CTLTYPE_UINT | CTLFLAG_RWTUN,
		&yarrow_state.ys_slowoverthresh, 0,
		random_check_uint_slowoverthresh, "UI",
		"Slow over-threshold reseed");
#endif /* _KERNEL */
	yarrow_state.ys_gengateinterval = 10;
	yarrow_state.ys_bins = 10;
	yarrow_state.ys_pool[RANDOM_YARROW_FAST].ysp_thresh = (3*(RANDOM_BLOCKSIZE*8))/4;
	yarrow_state.ys_pool[RANDOM_YARROW_SLOW].ysp_thresh = (RANDOM_BLOCKSIZE*8);
	yarrow_state.ys_slowoverthresh = 2;
	/* Ensure that the first time we read, we are gated. */
	yarrow_state.ys_outputblocks = yarrow_state.ys_gengateinterval;
	/* Initialise the fast and slow entropy pools */
	for (i = RANDOM_YARROW_FAST; i <= RANDOM_YARROW_SLOW; i++) {
		randomdev_hash_init(&yarrow_state.ys_pool[i].ysp_hash);
		for (j = RANDOM_START; j < ENTROPYSOURCE; j++)
			yarrow_state.ys_pool[i].ysp_source_bits[j] = 0;
	}
	/* Clear the counter */
	yarrow_state.ys_counter = UINT128_ZERO;
}

/* ARGSUSED */
static void
random_yarrow_deinit_alg(void *unused __unused)
{

	RANDOM_RESEED_DEINIT_LOCK();
	explicit_bzero(&yarrow_state, sizeof(yarrow_state));
#ifdef _KERNEL
	sysctl_ctx_free(&random_clist);
#endif
}

/* Process a single stochastic event off the harvest queue */
static void
random_yarrow_process_event(struct harvest_event *event)
{
	u_int pl, overthreshhold[RANDOM_YARROW_NPOOLS];
	enum random_entropy_source src;

	RANDOM_RESEED_LOCK();
	/*
	 * Accumulate the event into the appropriate pool
	 * where each event carries the destination information.
	 * We lock against pool state modification which can happen
	 * during accumulation/reseeding and reading/regating
	 */
	pl = event->he_destination % RANDOM_YARROW_NPOOLS;
	randomdev_hash_iterate(&yarrow_state.ys_pool[pl].ysp_hash, event, sizeof(*event));
	yarrow_state.ys_pool[pl].ysp_source_bits[event->he_source] += event->he_bits;
	/* Count the over-threshold sources in each pool */
	for (pl = RANDOM_YARROW_FAST; pl <= RANDOM_YARROW_SLOW; pl++) {
		overthreshhold[pl] = 0;
		for (src = RANDOM_START; src < ENTROPYSOURCE; src++) {
			if (yarrow_state.ys_pool[pl].ysp_source_bits[src] > yarrow_state.ys_pool[pl].ysp_thresh)
				overthreshhold[pl]++;
		}
	}
	/*
	 * If enough slow sources are over threshhold, then slow reseed
	 * else if any fast source over threshhold, then fast reseed.
	 */
	if (overthreshhold[RANDOM_YARROW_SLOW] >= yarrow_state.ys_slowoverthresh)
		random_yarrow_reseed_internal(RANDOM_YARROW_SLOW);
	else if (overthreshhold[RANDOM_YARROW_FAST] > 0 && yarrow_state.ys_seeded)
		random_yarrow_reseed_internal(RANDOM_YARROW_FAST);
	explicit_bzero(event, sizeof(*event));
	RANDOM_RESEED_UNLOCK();
}

static void
random_yarrow_reseed_internal(u_int fastslow)
{
	/*
	 * Interrupt-context stack is a limited resource; make large
	 * structures static.
	 */
	static uint8_t v[RANDOM_YARROW_TIMEBIN][RANDOM_KEYSIZE];	/* v[i] */
	static uint128_t temp;
	static struct randomdev_hash context;
	u_int i;
	enum random_entropy_source j;

	KASSERT(yarrow_state.ys_pool[RANDOM_YARROW_FAST].ysp_thresh > 0, ("random: Yarrow fast threshold = 0"));
	KASSERT(yarrow_state.ys_pool[RANDOM_YARROW_SLOW].ysp_thresh > 0, ("random: Yarrow slow threshold = 0"));
	RANDOM_RESEED_ASSERT_LOCK_OWNED();
#ifdef RANDOM_DEBUG
	/* WARNING! This is dangerously tedious to do with mutexes held! */
	printf("random: %s ", __func__);
	printf("type/pool = %s ", fastslow == RANDOM_YARROW_FAST ? "RANDOM_YARROW_FAST" : "RANDOM_YARROW_SLOW");
	printf("seeded = %s\n", yarrow_state.ys_seeded ? "true" : "false");
	printf("random: fast - thresh %d,1 - ", yarrow_state.ys_pool[RANDOM_YARROW_FAST].ysp_thresh);
	for (i = RANDOM_START; i < ENTROPYSOURCE; i++)
		printf(" %d", yarrow_state.ys_pool[RANDOM_YARROW_FAST].ysp_source_bits[i]);
	printf("\n");
	printf("random: slow - thresh %d,%d - ", yarrow_state.ys_pool[RANDOM_YARROW_SLOW].ysp_thresh, yarrow_state.ys_slowoverthresh);
	for (i = RANDOM_START; i < ENTROPYSOURCE; i++)
		printf(" %d", yarrow_state.ys_pool[RANDOM_YARROW_SLOW].ysp_source_bits[i]);
	printf("\n");
#endif
	/* 1. Hash the accumulated entropy into v[0] */
	randomdev_hash_init(&context);
	/* Feed the slow pool hash in if slow */
	if (fastslow == RANDOM_YARROW_SLOW) {
		randomdev_hash_finish(&yarrow_state.ys_pool[RANDOM_YARROW_SLOW].ysp_hash, &temp);
		randomdev_hash_iterate(&context, &temp, sizeof(temp));
	}
	randomdev_hash_finish(&yarrow_state.ys_pool[RANDOM_YARROW_FAST].ysp_hash, &temp);
	randomdev_hash_iterate(&context, &temp, sizeof(temp));
	randomdev_hash_finish(&context, v[0]);
	/*-
	 * 2. Compute hash values for all v. _Supposed_ to be computationally
	 *    intensive.
	 */
	if (yarrow_state.ys_bins > RANDOM_YARROW_TIMEBIN)
		yarrow_state.ys_bins = RANDOM_YARROW_TIMEBIN;
	for (i = 1; i < yarrow_state.ys_bins; i++) {
		randomdev_hash_init(&context);
		/* v[i] #= h(v[i - 1]) */
		randomdev_hash_iterate(&context, v[i - 1], RANDOM_KEYSIZE);
		/* v[i] #= h(v[0]) */
		randomdev_hash_iterate(&context, v[0], RANDOM_KEYSIZE);
		/* v[i] #= h(i) */
		randomdev_hash_iterate(&context, &i, sizeof(i));
		/* Return the hashval */
		randomdev_hash_finish(&context, v[i]);
	}
	/*-
	 * 3. Compute a new key; h' is the identity function here;
	 *    it is not being ignored!
	 */
	randomdev_hash_init(&context);
	randomdev_hash_iterate(&context, &yarrow_state.ys_key, RANDOM_KEYSIZE);
	for (i = 1; i < yarrow_state.ys_bins; i++)
		randomdev_hash_iterate(&context, v[i], RANDOM_KEYSIZE);
	randomdev_hash_finish(&context, &temp);
	randomdev_encrypt_init(&yarrow_state.ys_key, &temp);
	/* 4. Recompute the counter */
	yarrow_state.ys_counter = UINT128_ZERO;
	randomdev_encrypt(&yarrow_state.ys_key, &yarrow_state.ys_counter, &temp, RANDOM_BLOCKSIZE);
	yarrow_state.ys_counter = temp;
	/* 5. Reset entropy estimate accumulators to zero */
	for (i = 0; i <= fastslow; i++)
		for (j = RANDOM_START; j < ENTROPYSOURCE; j++)
			yarrow_state.ys_pool[i].ysp_source_bits[j] = 0;
	/* 6. Wipe memory of intermediate values */
	explicit_bzero(v, sizeof(v));
	explicit_bzero(&temp, sizeof(temp));
	explicit_bzero(&context, sizeof(context));
/* Not defined so writes ain't gonna happen. Kept for documenting. */
#ifdef RANDOM_RWFILE_WRITE_IS_OK
	/*-
         * 7. Dump to seed file.
	 * This pseudo-code is documentation. Please leave it alone.
	 */
	seed_file = "<some file>";
	error = randomdev_write_file(seed_file, <generated entropy>, PAGE_SIZE);
	if (error == 0)
		printf("random: entropy seed file '%s' successfully written\n", seed_file);
#endif
	/* Unblock the device if it was blocked due to being unseeded */
	if (!yarrow_state.ys_seeded) {
		yarrow_state.ys_seeded = true;
		randomdev_unblock();
	}
}

static __inline void
random_yarrow_generator_gate(void)
{
	u_int i;
	uint8_t temp[RANDOM_KEYSIZE];

	RANDOM_RESEED_ASSERT_LOCK_OWNED();
	uint128_increment(&yarrow_state.ys_counter);
	for (i = 0; i < RANDOM_KEYSIZE; i += RANDOM_BLOCKSIZE)
		randomdev_encrypt(&yarrow_state.ys_key, &yarrow_state.ys_counter, temp + i, RANDOM_BLOCKSIZE);
	randomdev_encrypt_init(&yarrow_state.ys_key, temp);
	explicit_bzero(temp, sizeof(temp));
}

/*-
 * Used to return processed entropy from the PRNG. There is a pre_read
 * required to be present (but it can be a stub) in order to allow
 * specific actions at the begin of the read.
 * Yarrow does its reseeding in its own thread; _pre_read() is not used
 * by Yarrow but must be kept for completeness.
 */
void
random_yarrow_pre_read(void)
{
}

/*-
 * Main read from Yarrow.
 * The supplied buf MUST be a multiple (>=0) of RANDOM_BLOCKSIZE in size.
 * Lots of code presumes this for efficiency, both here and in other
 * routines. You are NOT allowed to break this!
 */
void
random_yarrow_read(uint8_t *buf, u_int bytecount)
{
	u_int blockcount, i;

	KASSERT((bytecount % RANDOM_BLOCKSIZE) == 0, ("%s(): bytecount (= %d) must be a multiple of %d", __func__, bytecount, RANDOM_BLOCKSIZE ));
	RANDOM_RESEED_LOCK();
	blockcount = (bytecount + RANDOM_BLOCKSIZE - 1)/RANDOM_BLOCKSIZE;
	for (i = 0; i < blockcount; i++) {
		if (yarrow_state.ys_outputblocks++ >= yarrow_state.ys_gengateinterval) {
			random_yarrow_generator_gate();
			yarrow_state.ys_outputblocks = 0;
		}
		uint128_increment(&yarrow_state.ys_counter);
		randomdev_encrypt(&yarrow_state.ys_key, &yarrow_state.ys_counter, buf, RANDOM_BLOCKSIZE);
		buf += RANDOM_BLOCKSIZE;
	}
	RANDOM_RESEED_UNLOCK();
}

bool
random_yarrow_seeded(void)
{

	return (yarrow_state.ys_seeded);
}
