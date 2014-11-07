/*-
 * Copyright (c) 2000-2013 Mark R V Murray
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
#include "opt_random.h"

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
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/uint128.h>
#include <dev/random/yarrow.h>
#else /* !_KERNEL */
#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
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

#if !defined(RANDOM_YARROW) && !defined(RANDOM_FORTUNA)
#define RANDOM_YARROW
#elif defined(RANDOM_YARROW) && defined(RANDOM_FORTUNA)
#error "Must define either RANDOM_YARROW or RANDOM_FORTUNA"
#endif

#if defined(RANDOM_YARROW)

#define TIMEBIN		16	/* max value for Pt/t */

#define FAST		0
#define SLOW		1

/* This algorithm (and code) presumes that KEYSIZE is twice as large as BLOCKSIZE */
CTASSERT(BLOCKSIZE == sizeof(uint128_t));
CTASSERT(KEYSIZE == 2*BLOCKSIZE);

/* This is the beastie that needs protecting. It contains all of the
 * state that we are excited about.
 * Exactly one is instantiated.
 */
static struct yarrow_state {
	union {
		uint8_t byte[BLOCKSIZE];
		uint128_t whole;
	} counter;			/* C */
	struct randomdev_key key;	/* K */
	u_int gengateinterval;		/* Pg */
	u_int bins;			/* Pt/t */
	u_int outputblocks;		/* count output blocks for gates */
	u_int slowoverthresh;		/* slow pool overthreshhold reseed count */
	struct pool {
		struct source {
			u_int bits;	/* estimated bits of entropy */
		} source[ENTROPYSOURCE];/* ... per source */
		u_int thresh;		/* pool reseed threshhold */
		struct randomdev_hash hash;	/* accumulated entropy */
	} pool[2];			/* pool[0] is fast, pool[1] is slow */
	int seeded;

	struct start_cache {
		uint8_t junk[KEYSIZE];
		struct randomdev_hash hash;
	} start_cache;
} yarrow_state;

/* The random_reseed_mtx mutex protects seeding and polling/blocking.  */
static mtx_t random_reseed_mtx;

#ifdef _KERNEL
static struct sysctl_ctx_list random_clist;
RANDOM_CHECK_UINT(gengateinterval, 4, 64);
RANDOM_CHECK_UINT(bins, 2, 16);
RANDOM_CHECK_UINT(fastthresh, (BLOCKSIZE*8)/4, (BLOCKSIZE*8)); /* Bit counts */
RANDOM_CHECK_UINT(slowthresh, (BLOCKSIZE*8)/4, (BLOCKSIZE*8)); /* Bit counts */
RANDOM_CHECK_UINT(slowoverthresh, 1, 5);
#else /* !_KERNEL */
static u_int harvest_destination[ENTROPYSOURCE];
#endif /* _KERNEL */

static void generator_gate(void);
static void reseed(u_int);

void
random_yarrow_init_alg(void)
{
	int i, j;
#ifdef _KERNEL
	struct sysctl_oid *random_yarrow_o;
#endif /* _KERNEL */

	memset(yarrow_state.start_cache.junk, 0, KEYSIZE);
	randomdev_hash_init(&yarrow_state.start_cache.hash);

	/* Set up the lock for the reseed/gate state */
#ifdef _KERNEL
	mtx_init(&random_reseed_mtx, "reseed mutex", NULL, MTX_DEF);
#else /* !_KERNEL */
	mtx_init(&random_reseed_mtx, mtx_plain);
#endif /* _KERNEL */

	/* Start unseeded, therefore blocked. */
	yarrow_state.seeded = 0;

#ifdef _KERNEL
	/* Yarrow parameters. Do not adjust these unless you have
	 * have a very good clue about what they do!
	 */
	random_yarrow_o = SYSCTL_ADD_NODE(&random_clist,
		SYSCTL_STATIC_CHILDREN(_kern_random),
		OID_AUTO, "yarrow", CTLFLAG_RW, 0,
		"Yarrow Parameters");

	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"gengateinterval", CTLTYPE_INT|CTLFLAG_RW,
		&yarrow_state.gengateinterval, 10,
		random_check_uint_gengateinterval, "I",
		"Generation gate interval");

	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"bins", CTLTYPE_INT|CTLFLAG_RW,
		&yarrow_state.bins, 10,
		random_check_uint_bins, "I",
		"Execution time tuner");

	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"fastthresh", CTLTYPE_INT|CTLFLAG_RW,
		&yarrow_state.pool[0].thresh, (3*(BLOCKSIZE*8))/4,
		random_check_uint_fastthresh, "I",
		"Fast reseed threshold");

	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"slowthresh", CTLTYPE_INT|CTLFLAG_RW,
		&yarrow_state.pool[1].thresh, (BLOCKSIZE*8),
		random_check_uint_slowthresh, "I",
		"Slow reseed threshold");

	SYSCTL_ADD_PROC(&random_clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"slowoverthresh", CTLTYPE_INT|CTLFLAG_RW,
		&yarrow_state.slowoverthresh, 2,
		random_check_uint_slowoverthresh, "I",
		"Slow over-threshold reseed");
#endif /* _KERNEL */

	yarrow_state.gengateinterval = 10;
	yarrow_state.bins = 10;
	yarrow_state.pool[FAST].thresh = (3*(BLOCKSIZE*8))/4;
	yarrow_state.pool[SLOW].thresh = (BLOCKSIZE*8);
	yarrow_state.slowoverthresh = 2;

	/* Ensure that the first time we read, we are gated. */
	yarrow_state.outputblocks = yarrow_state.gengateinterval;

	/* Initialise the fast and slow entropy pools */
	for (i = FAST; i <= SLOW; i++) {
		randomdev_hash_init(&yarrow_state.pool[i].hash);
		for (j = RANDOM_START; j < ENTROPYSOURCE; j++)
			yarrow_state.pool[i].source[j].bits = 0U;
	}

	/* Clear the counter */
	uint128_clear(&yarrow_state.counter.whole);
}

void
random_yarrow_deinit_alg(void)
{

	mtx_destroy(&random_reseed_mtx);
	memset(&yarrow_state, 0, sizeof(yarrow_state));

#ifdef _KERNEL
	sysctl_ctx_free(&random_clist);
#endif
}

static __inline void
random_yarrow_post_insert(void)
{
	u_int pl, overthreshhold[2];
	enum random_entropy_source src;

#ifdef _KERNEL
	mtx_assert(&random_reseed_mtx, MA_OWNED);
#endif
	/* Count the over-threshold sources in each pool */
	for (pl = 0; pl < 2; pl++) {
		overthreshhold[pl] = 0;
		for (src = RANDOM_START; src < ENTROPYSOURCE; src++) {
			if (yarrow_state.pool[pl].source[src].bits > yarrow_state.pool[pl].thresh)
				overthreshhold[pl]++;
		}
	}

	/* If enough slow sources are over threshhold, then slow reseed
	 * else if any fast source over threshhold, then fast reseed.
	 */
	if (overthreshhold[SLOW] >= yarrow_state.slowoverthresh)
		reseed(SLOW);
	else if (overthreshhold[FAST] > 0 && yarrow_state.seeded)
		reseed(FAST);
}

/* Process a single stochastic event off the harvest queue */
void
random_yarrow_process_event(struct harvest_event *event)
{
	u_int pl;

	mtx_lock(&random_reseed_mtx);

	/* Accumulate the event into the appropriate pool
	 * where each event carries the destination information.
	 * We lock against pool state modification which can happen
	 * during accumulation/reseeding and reading/regating
	 */
	pl = event->he_destination % 2;
	randomdev_hash_iterate(&yarrow_state.pool[pl].hash, event, sizeof(*event));
	yarrow_state.pool[pl].source[event->he_source].bits += event->he_bits;

	random_yarrow_post_insert();

	mtx_unlock(&random_reseed_mtx);
}

/* Process a block of data suspected to be slightly stochastic */
static void
random_yarrow_process_buffer(uint8_t *buf, u_int length)
{
	static struct harvest_event event;
	u_int i, pl;

	/* Accumulate the data into the appropriate pools
	 * where each event carries the destination information.
	 * We lock against pool state modification which can happen
	 * during accumulation/reseeding and reading/regating
	 */
	memset(event.he_entropy + sizeof(uint32_t), 0, HARVESTSIZE - sizeof(uint32_t));
	for (i = 0; i < length/sizeof(uint32_t); i++) {
		event.he_somecounter = get_cyclecount();
		event.he_bits = 0; /* Fake */
		event.he_source = RANDOM_CACHED;
		event.he_destination = harvest_destination[RANDOM_CACHED]++;
		event.he_size = sizeof(uint32_t);
		*((uint32_t *)event.he_entropy) = *((uint32_t *)buf + i);

		/* Do the actual entropy insertion */
		pl = event.he_destination % 2;
		randomdev_hash_iterate(&yarrow_state.pool[pl].hash, &event, sizeof(event));
#ifdef DONT_DO_THIS_HERE
		/* Don't do this here - do it in bulk at the end */
		yarrow_state.pool[pl].source[RANDOM_CACHED].bits += bits;
#endif
	}
	for (pl = FAST; pl <= SLOW; pl++)
		yarrow_state.pool[pl].source[RANDOM_CACHED].bits += (length >> 4);

	random_yarrow_post_insert();
}

static void
reseed(u_int fastslow)
{
	/* Interrupt-context stack is a limited resource; make large
	 * structures static.
	 */
	static uint8_t v[TIMEBIN][KEYSIZE];	/* v[i] */
	static uint8_t hash[KEYSIZE];		/* h' */
	static uint8_t temp[KEYSIZE];
	static struct randomdev_hash context;
	u_int i;
	enum random_entropy_source j;

	KASSERT(yarrow_state.pool[FAST].thresh > 0, ("random: Yarrow fast threshold = 0"));
	KASSERT(yarrow_state.pool[SLOW].thresh > 0, ("random: Yarrow slow threshold = 0"));

#ifdef RANDOM_DEBUG
#ifdef RANDOM_DEBUG_VERBOSE
	printf("random: %s %s\n", __func__, (fastslow == FAST ? "FAST" : "SLOW"));
#endif
	if (!yarrow_state.seeded) {
		printf("random: %s - fast - thresh %d,1 - ", __func__, yarrow_state.pool[FAST].thresh);
		for (i = RANDOM_START; i < ENTROPYSOURCE; i++)
			printf(" %d", yarrow_state.pool[FAST].source[i].bits);
		printf("\n");
		printf("random: %s - slow - thresh %d,%d - ", __func__, yarrow_state.pool[SLOW].thresh, yarrow_state.slowoverthresh);
		for (i = RANDOM_START; i < ENTROPYSOURCE; i++)
			printf(" %d", yarrow_state.pool[SLOW].source[i].bits);
		printf("\n");
	}
#endif
#ifdef _KERNEL
	mtx_assert(&random_reseed_mtx, MA_OWNED);
#endif

	/* 1. Hash the accumulated entropy into v[0] */

	randomdev_hash_init(&context);
	/* Feed the slow pool hash in if slow */
	if (fastslow == SLOW) {
		randomdev_hash_finish(&yarrow_state.pool[SLOW].hash, temp);
		randomdev_hash_iterate(&context, temp, sizeof(temp));
	}
	randomdev_hash_finish(&yarrow_state.pool[FAST].hash, temp);
	randomdev_hash_iterate(&context, temp, sizeof(temp));
	randomdev_hash_finish(&context, v[0]);

	/* 2. Compute hash values for all v. _Supposed_ to be computationally
	 *    intensive.
	 */

	if (yarrow_state.bins > TIMEBIN)
		yarrow_state.bins = TIMEBIN;
	for (i = 1; i < yarrow_state.bins; i++) {
		randomdev_hash_init(&context);
		/* v[i] #= h(v[i - 1]) */
		randomdev_hash_iterate(&context, v[i - 1], KEYSIZE);
		/* v[i] #= h(v[0]) */
		randomdev_hash_iterate(&context, v[0], KEYSIZE);
		/* v[i] #= h(i) */
		randomdev_hash_iterate(&context, &i, sizeof(i));
		/* Return the hashval */
		randomdev_hash_finish(&context, v[i]);
	}

	/* 3. Compute a new key; h' is the identity function here;
	 *    it is not being ignored!
	 */

	randomdev_hash_init(&context);
	randomdev_hash_iterate(&context, &yarrow_state.key, KEYSIZE);
	for (i = 1; i < yarrow_state.bins; i++)
		randomdev_hash_iterate(&context, v[i], KEYSIZE);
	randomdev_hash_finish(&context, temp);
	randomdev_encrypt_init(&yarrow_state.key, temp);

	/* 4. Recompute the counter */

	uint128_clear(&yarrow_state.counter.whole);
	randomdev_encrypt(&yarrow_state.key, yarrow_state.counter.byte, temp, BLOCKSIZE);
	memcpy(yarrow_state.counter.byte, temp, BLOCKSIZE);

	/* 5. Reset entropy estimate accumulators to zero */

	for (i = 0; i <= fastslow; i++)
		for (j = RANDOM_START; j < ENTROPYSOURCE; j++)
			yarrow_state.pool[i].source[j].bits = 0;

	/* 6. Wipe memory of intermediate values */

	memset(v, 0, sizeof(v));
	memset(temp, 0, sizeof(temp));
	memset(hash, 0, sizeof(hash));
	memset(&context, 0, sizeof(context));

#ifdef RANDOM_RWFILE_WRITE_IS_OK /* Not defined so writes ain't gonna happen */
	/* 7. Dump to seed file */

	/* This pseudo-code is documentation. Please leave it alone. */
	seed_file = "<some file>";
	error = randomdev_write_file(seed_file, <generated entropy>, PAGE_SIZE);
	if (error == 0)
		printf("random: entropy seed file '%s' successfully written\n", seed_file);
#endif

	/* Unblock the device if it was blocked due to being unseeded */
	if (!yarrow_state.seeded) {
		yarrow_state.seeded = 1;
		random_adaptor_unblock();
	}
}

/* Internal function to return processed entropy from the PRNG */
void
random_yarrow_read(uint8_t *buf, u_int bytecount)
{
	u_int blockcount, i;

	/* Check for initial/final read requests */
	if (buf == NULL)
		return;

	/* The reseed task must not be jumped on */
	mtx_lock(&random_reseed_mtx);

	blockcount = (bytecount + BLOCKSIZE - 1)/BLOCKSIZE;
	for (i = 0; i < blockcount; i++) {
		if (yarrow_state.outputblocks++ >= yarrow_state.gengateinterval) {
			generator_gate();
			yarrow_state.outputblocks = 0;
		}
		uint128_increment(&yarrow_state.counter.whole);
		randomdev_encrypt(&yarrow_state.key, yarrow_state.counter.byte, buf, BLOCKSIZE);
		buf += BLOCKSIZE;
	}

	mtx_unlock(&random_reseed_mtx);
}

/* Internal function to hand external entropy to the PRNG */
void
random_yarrow_write(uint8_t *buf, u_int count)
{
	uintmax_t timestamp;

	/* We must be locked for all this as plenty of state gets messed with */
	mtx_lock(&random_reseed_mtx);

	timestamp = get_cyclecount();
	randomdev_hash_iterate(&yarrow_state.start_cache.hash, &timestamp, sizeof(timestamp));
	randomdev_hash_iterate(&yarrow_state.start_cache.hash, buf, count);
	timestamp = get_cyclecount();
	randomdev_hash_iterate(&yarrow_state.start_cache.hash, &timestamp, sizeof(timestamp));
	randomdev_hash_finish(&yarrow_state.start_cache.hash, yarrow_state.start_cache.junk);
	randomdev_hash_init(&yarrow_state.start_cache.hash);

#ifdef RANDOM_DEBUG_VERBOSE
	{
		int i;

		printf("random: %s - ", __func__);
		for (i = 0; i < KEYSIZE; i++)
			printf("%02X", yarrow_state.start_cache.junk[i]);
		printf("\n");
	}
#endif

	random_yarrow_process_buffer(yarrow_state.start_cache.junk, KEYSIZE);
	memset(yarrow_state.start_cache.junk, 0, KEYSIZE);

	mtx_unlock(&random_reseed_mtx);
}

static void
generator_gate(void)
{
	u_int i;
	uint8_t temp[KEYSIZE];

	for (i = 0; i < KEYSIZE; i += BLOCKSIZE) {
		uint128_increment(&yarrow_state.counter.whole);
		randomdev_encrypt(&yarrow_state.key, yarrow_state.counter.byte, temp + i, BLOCKSIZE);
	}

	randomdev_encrypt_init(&yarrow_state.key, temp);
	memset(temp, 0, KEYSIZE);
}

void
random_yarrow_reseed(void)
{

	mtx_lock(&random_reseed_mtx);
	reseed(SLOW);
	mtx_unlock(&random_reseed_mtx);
}

int
random_yarrow_seeded(void)
{

	return (yarrow_state.seeded);
}

#endif /* RANDOM_YARROW */
