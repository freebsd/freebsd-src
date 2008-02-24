/*-
 * Copyright (c) 2000-2004 Mark R V Murray
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
__FBSDID("$FreeBSD: src/sys/dev/random/yarrow.c,v 1.47.2.1 2007/11/29 16:05:38 simon Exp $");

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
#include <dev/random/randomdev_soft.h>
#include <dev/random/yarrow.h>

RANDOM_CHECK_UINT(gengateinterval, 4, 64);
RANDOM_CHECK_UINT(bins, 2, 16);
RANDOM_CHECK_UINT(fastthresh, BLOCKSIZE/4, BLOCKSIZE);
RANDOM_CHECK_UINT(slowthresh, BLOCKSIZE/4, BLOCKSIZE);
RANDOM_CHECK_UINT(slowoverthresh, 1, 5);

/* Structure holding the entropy state */
static struct random_state random_state;

static void generator_gate(void);
static void reseed(u_int);

/* The reseed thread mutex */
struct mtx random_reseed_mtx;

/* Process a single stochastic event off the harvest queue */
void
random_process_event(struct harvest *event)
{
	u_int pl, overthreshhold[2];
	struct source *source;
	enum esource src;

	/* Unpack the event into the appropriate source accumulator */
	pl = random_state.which;
	source = &random_state.pool[pl].source[event->source];
	yarrow_hash_iterate(&random_state.pool[pl].hash, event->entropy,
		sizeof(event->entropy));
	yarrow_hash_iterate(&random_state.pool[pl].hash, &event->somecounter,
		sizeof(event->somecounter));
	source->frac += event->frac;
	source->bits += event->bits + source->frac/1024;
	source->frac %= 1024;

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
random_yarrow_init_alg(struct sysctl_ctx_list *clist, struct sysctl_oid *in_o)
{
	int i;
	struct sysctl_oid *random_yarrow_o;

	/* Yarrow parameters. Do not adjust these unless you have
	 * have a very good clue about what they do!
	 */
	random_yarrow_o = SYSCTL_ADD_NODE(clist,
		SYSCTL_CHILDREN(in_o),
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
		&random_state.pool[0].thresh, (3*BLOCKSIZE)/4,
		random_check_uint_fastthresh, "I",
		"Fast reseed threshold");

	SYSCTL_ADD_PROC(clist,
		SYSCTL_CHILDREN(random_yarrow_o), OID_AUTO,
		"slowthresh", CTLTYPE_INT|CTLFLAG_RW,
		&random_state.pool[1].thresh, BLOCKSIZE,
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
	random_state.pool[0].thresh = (3*BLOCKSIZE)/4;
	random_state.pool[1].thresh = BLOCKSIZE;
	random_state.slowoverthresh = 2;
	random_state.which = FAST;

	/* Initialise the fast and slow entropy pools */
	for (i = 0; i < 2; i++)
		yarrow_hash_init(&random_state.pool[i].hash);

	/* Clear the counter */
	for (i = 0; i < 4; i++)
		random_state.counter[i] = 0;

	/* Set up a lock for the reseed process */
	mtx_init(&random_reseed_mtx, "random reseed", NULL, MTX_DEF);
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
	static u_char v[TIMEBIN][KEYSIZE];	/* v[i] */
	static struct yarrowhash context;
	u_char hash[KEYSIZE];			/* h' */
	u_char temp[KEYSIZE];
	u_int i;
	enum esource j;

	/* The reseed task must not be jumped on */
	mtx_lock(&random_reseed_mtx);

	/* 1. Hash the accumulated entropy into v[0] */

	yarrow_hash_init(&context);
	/* Feed the slow pool hash in if slow */
	if (fastslow == SLOW)
		yarrow_hash_iterate(&context,
			&random_state.pool[SLOW].hash,
			sizeof(struct yarrowhash));
	yarrow_hash_iterate(&context,
		&random_state.pool[FAST].hash, sizeof(struct yarrowhash));
	yarrow_hash_finish(&context, v[0]);

	/* 2. Compute hash values for all v. _Supposed_ to be computationally
	 *    intensive.
	 */

	if (random_state.bins > TIMEBIN)
		random_state.bins = TIMEBIN;
	for (i = 1; i < random_state.bins; i++) {
		yarrow_hash_init(&context);
		/* v[i] #= h(v[i - 1]) */
		yarrow_hash_iterate(&context, v[i - 1], KEYSIZE);
		/* v[i] #= h(v[0]) */
		yarrow_hash_iterate(&context, v[0], KEYSIZE);
		/* v[i] #= h(i) */
		yarrow_hash_iterate(&context, &i, sizeof(u_int));
		/* Return the hashval */
		yarrow_hash_finish(&context, v[i]);
	}

	/* 3. Compute a new key; h' is the identity function here;
	 *    it is not being ignored!
	 */

	yarrow_hash_init(&context);
	yarrow_hash_iterate(&context, &random_state.key, KEYSIZE);
	for (i = 1; i < random_state.bins; i++)
		yarrow_hash_iterate(&context, &v[i], KEYSIZE);
	yarrow_hash_finish(&context, temp);
	yarrow_encrypt_init(&random_state.key, temp);

	/* 4. Recompute the counter */

	for (i = 0; i < 4; i++)
		random_state.counter[i] = 0;
	yarrow_encrypt(&random_state.key, random_state.counter, temp);
	memcpy(random_state.counter, temp, sizeof(random_state.counter));

	/* 5. Reset entropy estimate accumulators to zero */

	for (i = 0; i <= fastslow; i++) {
		for (j = RANDOM_START; j < ENTROPYSOURCE; j++) {
			random_state.pool[i].source[j].bits = 0;
			random_state.pool[i].source[j].frac = 0;
		}
	}

	/* 6. Wipe memory of intermediate values */

	memset((void *)v, 0, sizeof(v));
	memset((void *)temp, 0, sizeof(temp));
	memset((void *)hash, 0, sizeof(hash));

	/* 7. Dump to seed file */
	/* XXX Not done here yet */

	/* Unblock the device if it was blocked due to being unseeded */
	random_yarrow_unblock();

	/* Release the reseed mutex */
	mtx_unlock(&random_reseed_mtx);
}

/* Internal function to return processed entropy from the PRNG */
int
random_yarrow_read(void *buf, int count)
{
	static int cur = 0;
	static int gate = 1;
	static u_char genval[KEYSIZE];
	size_t tomove;
	int i;
	int retval;

	/* The reseed task must not be jumped on */
	mtx_lock(&random_reseed_mtx);

	if (gate) {
		generator_gate();
		random_state.outputblocks = 0;
		gate = 0;
	}
	if (count > 0 && (size_t)count >= sizeof(random_state.counter)) {
		retval = 0;
		for (i = 0; i < count; i += (int)sizeof(random_state.counter)) {
			random_state.counter[0]++;
			yarrow_encrypt(&random_state.key, random_state.counter,
				genval);
			tomove = min(count - i, sizeof(random_state.counter));
			memcpy((char *)buf + i, genval, tomove);
			if (++random_state.outputblocks >=
				random_state.gengateinterval) {
				generator_gate();
				random_state.outputblocks = 0;
			}
			retval += (int)tomove;
			cur = 0;
		}
	}
	else {
		if (!cur) {
			random_state.counter[0]++;
			yarrow_encrypt(&random_state.key, random_state.counter,
				genval);
			memcpy(buf, genval, (size_t)count);
			cur = (int)sizeof(random_state.counter) - count;
			if (++random_state.outputblocks >=
				random_state.gengateinterval) {
				generator_gate();
				random_state.outputblocks = 0;
			}
			retval = count;
		}
		else {
			retval = MIN(cur, count);
			memcpy(buf,
			    &genval[(int)sizeof(random_state.counter) - cur],
			    (size_t)retval);
			cur -= retval;
		}
	}
	mtx_unlock(&random_reseed_mtx);
	return retval;
}

static void
generator_gate(void)
{
	u_int i;
	u_char temp[KEYSIZE];

	for (i = 0; i < KEYSIZE; i += sizeof(random_state.counter)) {
		random_state.counter[0]++;
		yarrow_encrypt(&random_state.key, random_state.counter,
			&(temp[i]));
	}

	yarrow_encrypt_init(&random_state.key, temp);
	memset((void *)temp, 0, KEYSIZE);

}

/* Helper routine to perform explicit reseeds */
void
random_yarrow_reseed(void)
{
	reseed(SLOW);
}
