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

#include "opt_random.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/randomdev_soft.h>
#include <dev/random/yarrow.h>

#define TIMEBIN		16	/* max value for Pt/t */

#define FAST		0
#define SLOW		1

/* This is the beastie that needs protecting. It contains all of the
 * state that we are excited about.
 * Exactly one is instantiated.
 */
static struct random_state {
	union {
		uint8_t byte[BLOCKSIZE];
		uint64_t qword[BLOCKSIZE/sizeof(uint64_t)];
	} counter;		/* C */
	struct randomdev_key key; /* K */
	u_int gengateinterval;	/* Pg */
	u_int bins;		/* Pt/t */
	u_int outputblocks;	/* count output blocks for gates */
	u_int slowoverthresh;	/* slow pool overthreshhold reseed count */
	struct pool {
		struct source {
			u_int bits;	/* estimated bits of entropy */
		} source[ENTROPYSOURCE];
		u_int thresh;	/* pool reseed threshhold */
		struct randomdev_hash hash;	/* accumulated entropy */
	} pool[2];		/* pool[0] is fast, pool[1] is slow */
	u_int which;		/* toggle - sets the current insertion pool */
} random_state;

RANDOM_CHECK_UINT(gengateinterval, 4, 64);
RANDOM_CHECK_UINT(bins, 2, 16);
RANDOM_CHECK_UINT(fastthresh, (BLOCKSIZE*8)/4, (BLOCKSIZE*8)); /* Bit counts */
RANDOM_CHECK_UINT(slowthresh, (BLOCKSIZE*8)/4, (BLOCKSIZE*8)); /* Bit counts */
RANDOM_CHECK_UINT(slowoverthresh, 1, 5);

static void generator_gate(void);
static void reseed(u_int);

/* The reseed thread mutex */
struct mtx random_reseed_mtx;

/* 128-bit C = 0 */
/* Nothing to see here, folks, just an ugly mess. */
static void
clear_counter(void)
{
	random_state.counter.qword[0] = 0UL;
	random_state.counter.qword[1] = 0UL;
}

/* 128-bit C = C + 1 */
/* Nothing to see here, folks, just an ugly mess. */
/* TODO: Make a Galois counter instead? */
static void
increment_counter(void)
{
	random_state.counter.qword[0]++;
	if (!random_state.counter.qword[0])
		random_state.counter.qword[1]++;
}

/* Process a single stochastic event off the harvest queue */
void
random_process_event(struct harvest *event)
{
	u_int pl, overthreshhold[2];
	struct source *source;
	enum esource src;

#if 0
	/* Do this better with DTrace */
	{
		int i;

		printf("Harvest:%16jX ", event->somecounter);
		for (i = 0; i < event->size; i++)
			printf("%02X", event->entropy[i]);
		for (; i < 16; i++)
			printf("  ");
		printf(" %2d %2d %02X\n", event->size, event->bits, event->source);
	}
#endif

	/* Accumulate the event into the appropriate pool */
	pl = random_state.which;
	source = &random_state.pool[pl].source[event->source];
	randomdev_hash_iterate(&random_state.pool[pl].hash, event,
		sizeof(*event));
	source->bits += event->bits;

	/* Count the over-threshold sources in each pool */
	for (pl = 0; pl < 2; pl++) {
		overthreshhold[pl] = 0;
		for (src = RANDOM_START; src < ENTROPYSOURCE; src++) {
			if (random_state.pool[pl].source[src].bits
				> random_state.pool[pl].thresh)
				overthreshhold[pl]++;
		}
	}

	/* if any fast source over threshhold, reseed */
	if (overthreshhold[FAST])
		reseed(FAST);

	/* if enough slow sources are over threshhold, reseed */
	if (overthreshhold[SLOW] >= random_state.slowoverthresh)
		reseed(SLOW);

	/* Invert the fast/slow pool selector bit */
	random_state.which = !random_state.which;
}

void
random_yarrow_init_alg(struct sysctl_ctx_list *clist)
{
	int i;
	struct sysctl_oid *random_yarrow_o;

	/* Yarrow parameters. Do not adjust these unless you have
	 * have a very good clue about what they do!
	 */
	random_yarrow_o = SYSCTL_ADD_NODE(clist,
		SYSCTL_STATIC_CHILDREN(_kern_random),
		OID_AUTO, "yarrow", CTLFLAG_RW, 0,
		"Yarrow Parameters");

	SYSCTL_ADD_PROC(clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"gengateinterval", CTLTYPE_INT|CTLFLAG_RW,
		&random_state.gengateinterval, 10,
		random_check_uint_gengateinterval, "I",
		"Generation gate interval");

	SYSCTL_ADD_PROC(clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"bins", CTLTYPE_INT|CTLFLAG_RW,
		&random_state.bins, 10,
		random_check_uint_bins, "I",
		"Execution time tuner");

	SYSCTL_ADD_PROC(clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"fastthresh", CTLTYPE_INT|CTLFLAG_RW,
		&random_state.pool[0].thresh, (3*(BLOCKSIZE*8))/4,
		random_check_uint_fastthresh, "I",
		"Fast reseed threshold");

	SYSCTL_ADD_PROC(clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"slowthresh", CTLTYPE_INT|CTLFLAG_RW,
		&random_state.pool[1].thresh, (BLOCKSIZE*8),
		random_check_uint_slowthresh, "I",
		"Slow reseed threshold");

	SYSCTL_ADD_PROC(clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"slowoverthresh", CTLTYPE_INT|CTLFLAG_RW,
		&random_state.slowoverthresh, 2,
		random_check_uint_slowoverthresh, "I",
		"Slow over-threshold reseed");

	random_state.gengateinterval = 10;
	random_state.bins = 10;
	random_state.pool[0].thresh = (3*(BLOCKSIZE*8))/4;
	random_state.pool[1].thresh = (BLOCKSIZE*8);
	random_state.slowoverthresh = 2;
	random_state.which = FAST;

	/* Initialise the fast and slow entropy pools */
	for (i = 0; i < 2; i++)
		randomdev_hash_init(&random_state.pool[i].hash);

	/* Clear the counter */
	clear_counter();

	/* Set up a lock for the reseed process */
	mtx_init(&random_reseed_mtx, "Yarrow reseed", NULL, MTX_DEF);
}

void
random_yarrow_deinit_alg(void)
{
	mtx_destroy(&random_reseed_mtx);
}

static void
reseed(u_int fastslow)
{
	/* Interrupt-context stack is a limited resource; make large
	 * structures static.
	 */
	static uint8_t v[TIMEBIN][KEYSIZE];	/* v[i] */
	static struct randomdev_hash context;
	uint8_t hash[KEYSIZE];			/* h' */
	uint8_t temp[KEYSIZE];
	u_int i;
	enum esource j;

#if 0
	printf("Yarrow: %s reseed\n", fastslow == FAST ? "fast" : "slow");
#endif

	/* The reseed task must not be jumped on */
	mtx_lock(&random_reseed_mtx);

	/* 1. Hash the accumulated entropy into v[0] */

	randomdev_hash_init(&context);
	/* Feed the slow pool hash in if slow */
	if (fastslow == SLOW)
		randomdev_hash_iterate(&context,
			&random_state.pool[SLOW].hash,
			sizeof(struct randomdev_hash));
	randomdev_hash_iterate(&context,
		&random_state.pool[FAST].hash, sizeof(struct randomdev_hash));
	randomdev_hash_finish(&context, v[0]);

	/* 2. Compute hash values for all v. _Supposed_ to be computationally
	 *    intensive.
	 */

	if (random_state.bins > TIMEBIN)
		random_state.bins = TIMEBIN;
	for (i = 1; i < random_state.bins; i++) {
		randomdev_hash_init(&context);
		/* v[i] #= h(v[i - 1]) */
		randomdev_hash_iterate(&context, v[i - 1], KEYSIZE);
		/* v[i] #= h(v[0]) */
		randomdev_hash_iterate(&context, v[0], KEYSIZE);
		/* v[i] #= h(i) */
		randomdev_hash_iterate(&context, &i, sizeof(u_int));
		/* Return the hashval */
		randomdev_hash_finish(&context, v[i]);
	}

	/* 3. Compute a new key; h' is the identity function here;
	 *    it is not being ignored!
	 */

	randomdev_hash_init(&context);
	randomdev_hash_iterate(&context, &random_state.key, KEYSIZE);
	for (i = 1; i < random_state.bins; i++)
		randomdev_hash_iterate(&context, &v[i], KEYSIZE);
	randomdev_hash_finish(&context, temp);
	randomdev_encrypt_init(&random_state.key, temp);

	/* 4. Recompute the counter */

	clear_counter();
	randomdev_encrypt(&random_state.key, random_state.counter.byte, temp, BLOCKSIZE);
	memcpy(random_state.counter.byte, temp, BLOCKSIZE);

	/* 5. Reset entropy estimate accumulators to zero */

	for (i = 0; i <= fastslow; i++)
		for (j = RANDOM_START; j < ENTROPYSOURCE; j++)
			random_state.pool[i].source[j].bits = 0;

	/* 6. Wipe memory of intermediate values */

	memset((void *)v, 0, sizeof(v));
	memset((void *)temp, 0, sizeof(temp));
	memset((void *)hash, 0, sizeof(hash));

	/* 7. Dump to seed file */
	/* XXX Not done here yet */

	/* Unblock the device if it was blocked due to being unseeded */
	randomdev_unblock();

	/* Release the reseed mutex */
	mtx_unlock(&random_reseed_mtx);
}

/* Internal function to return processed entropy from the PRNG */
int
random_yarrow_read(void *buf, int count)
{
	static int cur = 0;
	static int gate = 1;
	static uint8_t genval[KEYSIZE];
	size_t tomove;
	int i;
	int retval;

	/* Check for final read request */
	if (buf == NULL && count == 0)
		return (0);

	/* The reseed task must not be jumped on */
	mtx_lock(&random_reseed_mtx);

	if (gate) {
		generator_gate();
		random_state.outputblocks = 0;
		gate = 0;
	}
	if (count > 0 && (size_t)count >= BLOCKSIZE) {
		retval = 0;
		for (i = 0; i < count; i += BLOCKSIZE) {
			increment_counter();
			randomdev_encrypt(&random_state.key, random_state.counter.byte, genval, BLOCKSIZE);
			tomove = MIN(count - i, BLOCKSIZE);
			memcpy((char *)buf + i, genval, tomove);
			if (++random_state.outputblocks >= random_state.gengateinterval) {
				generator_gate();
				random_state.outputblocks = 0;
			}
			retval += (int)tomove;
			cur = 0;
		}
	}
	else {
		if (!cur) {
			increment_counter();
			randomdev_encrypt(&random_state.key, random_state.counter.byte, genval, BLOCKSIZE);
			memcpy(buf, genval, (size_t)count);
			cur = BLOCKSIZE - count;
			if (++random_state.outputblocks >= random_state.gengateinterval) {
				generator_gate();
				random_state.outputblocks = 0;
			}
			retval = count;
		}
		else {
			retval = MIN(cur, count);
			memcpy(buf, &genval[BLOCKSIZE - cur], (size_t)retval);
			cur -= retval;
		}
	}
	mtx_unlock(&random_reseed_mtx);
	return (retval);
}

static void
generator_gate(void)
{
	u_int i;
	uint8_t temp[KEYSIZE];

	for (i = 0; i < KEYSIZE; i += BLOCKSIZE) {
		increment_counter();
		randomdev_encrypt(&random_state.key, random_state.counter.byte, temp + i, BLOCKSIZE);
	}

	randomdev_encrypt_init(&random_state.key, temp);
	memset((void *)temp, 0, KEYSIZE);
}

/* Helper routine to perform explicit reseeds */
void
random_yarrow_reseed(void)
{
#ifdef RANDOM_DEBUG
	int i;

	printf("%s(): fast:", __func__);
	for (i = RANDOM_START; i < ENTROPYSOURCE; ++i)
		printf(" %d", random_state.pool[FAST].source[i].bits);
	printf("\n");
	printf("%s(): slow:", __func__);
	for (i = RANDOM_START; i < ENTROPYSOURCE; ++i)
		printf(" %d", random_state.pool[SLOW].source[i].bits);
	printf("\n");
#endif
	reseed(SLOW);
}
