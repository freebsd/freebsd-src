/*-
 * Copyright (c) 2000 Mark R V Murray
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
 * $FreeBSD$
 */

/* NOTE NOTE NOTE - This is not finished! It will supply numbers, but
 *                  it is not yet cryptographically secure!!
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <machine/mutex.h>
#include <crypto/blowfish/blowfish.h>

#include <dev/randomdev/hash.h>
#include <dev/randomdev/yarrow.h>

/* #define DEBUG */
/* #define DEBUG1 */	/* Very noisy - prints plenty harvesting stats */

static void generator_gate(void);
static void reseed(int);
static void random_harvest_internal(struct timespec *, void *, u_int, u_int, u_int, enum esource);

static void random_kthread(void *);

/* Structure holding the entropy state */
struct random_state random_state;

/* Queue holding harvested entropy */
TAILQ_HEAD(harvestqueue, harvest) harvestqueue,
	initqueue = TAILQ_HEAD_INITIALIZER(harvestqueue);

/* These are used to queue harvested packets of entropy. The entropy
 * buffer size of 16 is pretty arbitrary.
 */
struct harvest {
	struct timespec time;		/* nanotime for clock jitter */
	u_char entropy[16];		/* the harvested entropy */
	u_int size, bits, frac;		/* stats about the entropy */
	enum esource source;		/* stats about the entropy */
	u_int pool;			/* which pool this goes into */
	TAILQ_ENTRY(harvest) harvest;	/* link to next */
};

/* The reseed thread mutex */
static mtx_t random_reseed_mtx;

/* The entropy harvest mutex */
static mtx_t random_harvest_mtx;

/* <0 until the kthread starts, 0 for running */
static int random_kthread_status = -1;

/* <0 to end the kthread, 0 to let it run */
static int random_kthread_control = 0;

static struct proc *random_kthread_proc;

static void
random_kthread(void *status)
{
	int pl, src, overthreshhold[2];
	struct harvest *event;
	struct source *source;
#ifdef DEBUG1
	int queuecount;
#endif

#ifdef DEBUG
	printf("At %s, line %d: mtx_owned(&Giant) == %d\n", __FILE__, __LINE__, mtx_owned(&Giant));
	printf("At %s, line %d: mtx_owned(&sched_lock) == %d\n", __FILE__, __LINE__, mtx_owned(&sched_lock));
#endif
	random_set_wakeup((int *)status, 0);

	for (pl = 0; pl < 2; pl++)
		yarrow_hash_init(&random_state.pool[pl].hash, NULL, 0);

	for (;;) {

		if (TAILQ_EMPTY(&harvestqueue)) {

			/* Sleep for a second to give the system a chance */
			mtx_enter(&Giant, MTX_DEF);
			tsleep(&harvestqueue, PUSER, "rndslp", hz);
			mtx_exit(&Giant, MTX_DEF);

		}
		else {

			/* Suck the harvested entropy out of the queue and hash
			 * it into the fast and slow pools.
			 */
#ifdef DEBUG1
			queuecount = 0;
#endif
			while (!TAILQ_EMPTY(&harvestqueue)) {
#ifdef DEBUG1
				queuecount++;
#endif
				mtx_enter(&random_harvest_mtx, MTX_DEF);

				event = TAILQ_FIRST(&harvestqueue);
				TAILQ_REMOVE(&harvestqueue, event, harvest);

				mtx_exit(&random_harvest_mtx, MTX_DEF);

				source = &random_state.pool[event->pool].source[event->source];
				yarrow_hash_iterate(&random_state.pool[event->pool].hash,
					event->entropy, sizeof(event->entropy));
				yarrow_hash_iterate(&random_state.pool[event->pool].hash,
					&event->time, sizeof(event->time));
				source->frac += event->frac;
				source->bits += event->bits + source->frac/1024;
				source->frac %= 1024;
				free(event, M_TEMP);

				/* XXX abuse tsleep() to get at mi_switch() */
				/* tsleep(&harvestqueue, PUSER, "rndprc", 1); */

			}
#ifdef DEBUG1
			printf("Harvested %d events\n", queuecount);
#endif

			/* Count the over-threshold sources in each pool */
			for (pl = 0; pl < 2; pl++) {
				overthreshhold[pl] = 0;
				for (src = 0; src < ENTROPYSOURCE; src++) {
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

		}

		/* Is the thread scheduled for a shutdown? */
		if (random_kthread_control < 0) {
			if (!TAILQ_EMPTY(&harvestqueue)) {
#ifdef DEBUG
				printf("Random cleaning extraneous events\n");
#endif
				mtx_enter(&random_harvest_mtx, MTX_DEF);
				TAILQ_FOREACH(event, &harvestqueue, harvest) {
					TAILQ_REMOVE(&harvestqueue, event, harvest);
					free(event, M_TEMP);
				}
				mtx_exit(&random_harvest_mtx, MTX_DEF);
			}
#ifdef DEBUG
			printf("Random kthread setting terminate\n");
#endif
			random_set_wakeup_exit((int *)status, -1, 0);
			break;
		}

	}

}

int
random_init(void)
{
	int error;

#ifdef DEBUG
	printf("Random initialise\n");
#endif

	random_state.gengateinterval = 10;
	random_state.bins = 10;
	random_state.pool[0].thresh = 100;
	random_state.pool[1].thresh = 160;
	random_state.slowoverthresh = 2;
	random_state.which = FAST;

	harvestqueue = initqueue;

	/* Initialise the mutexes */
	mtx_init(&random_reseed_mtx, "random reseed", MTX_DEF);
	mtx_init(&random_harvest_mtx, "random harvest", MTX_DEF);

	/* Start the hash/reseed thread */
	error = kthread_create(random_kthread, &random_kthread_status,
		&random_kthread_proc, RFHIGHPID, "random");
	if (error != 0)
		return error;

	/* Register the randomness harvesting routine */
	random_init_harvester(random_harvest_internal);

#ifdef DEBUG
	printf("Random initalise finish\n");
#endif

	return 0;
}

void
random_deinit(void)
{
#ifdef DEBUG
	printf("Random deinitalise\n");
#endif

	/* Deregister the randomness harvesting routine */
	random_deinit_harvester();

#ifdef DEBUG
	printf("Random deinitalise waiting for thread to terminate\n");
#endif

	/* Command the hash/reseed thread to end and wait for it to finish */
	random_kthread_control = -1;
	while (random_kthread_status != -1)
		tsleep(&random_kthread_status, PUSER, "rndend", hz);

#ifdef DEBUG
	printf("Random deinitalise removing mutexes\n");
#endif

	/* Remove the mutexes */
	mtx_destroy(&random_reseed_mtx);
	mtx_destroy(&random_harvest_mtx);

#ifdef DEBUG
	printf("Random deinitalise finish\n");
#endif
}

static void
reseed(int fastslow)
{
	/* Interrupt-context stack is a limited resource; make large
	 * structures static.
	 */
	static u_char v[TIMEBIN][KEYSIZE];	/* v[i] */
	static struct yarrowhash context;
	u_char hash[KEYSIZE];			/* h' */
	u_char temp[KEYSIZE];
	int i, j;

#ifdef DEBUG
	printf("Reseed type %d\n", fastslow);
#endif

	/* The reseed task must not be jumped on */
	mtx_enter(&random_reseed_mtx, MTX_DEF);

	/* 1. Hash the accumulated entropy into v[0] */

	yarrow_hash_init(&context, NULL, 0);
	/* Feed the slow pool hash in if slow */
	if (fastslow == SLOW)
		yarrow_hash_iterate(&context,
			&random_state.pool[SLOW].hash, sizeof(struct yarrowhash));

	yarrow_hash_iterate(&context,
		&random_state.pool[FAST].hash, sizeof(struct yarrowhash));

	/* 2. Compute hash values for all v. _Supposed_ to be computationally
	 *    intensive.
	 */

	if (random_state.bins > TIMEBIN)
		random_state.bins = TIMEBIN;
	for (i = 1; i < random_state.bins; i++) {
		yarrow_hash_init(&context, NULL, 0);
		/* v[i] #= h(v[i-1]) */
		yarrow_hash_iterate(&context, v[i - 1], KEYSIZE);
		/* v[i] #= h(v[0]) */
		yarrow_hash_iterate(&context, v[0], KEYSIZE);
		/* v[i] #= h(i) */
		yarrow_hash_iterate(&context, &i, sizeof(int));
		/* Return the hashval */
		yarrow_hash_finish(&context, v[i]);
	}

	/* 3. Compute a new key; h' is the identity function here;
	 *    it is not being ignored!
	 */

	yarrow_hash_init(&context, NULL, 0);
	yarrow_hash_iterate(&context, &random_state.key, KEYSIZE);
	for (i = 1; i < random_state.bins; i++)
		yarrow_hash_iterate(&context, &v[i], KEYSIZE);
	yarrow_hash_finish(&context, temp);
	yarrow_encrypt_init(&random_state.key, temp, KEYSIZE);

	/* 4. Recompute the counter */

	random_state.counter = 0;
	yarrow_encrypt(&random_state.key, &random_state.counter, temp,
		sizeof(random_state.counter));
	memcpy(&random_state.counter, temp, random_state.counter);

	/* 5. Reset entropy estimate accumulators to zero */

	for (i = 0; i <= fastslow; i++) {
		for (j = 0; j < ENTROPYSOURCE; j++) {
			if (random_state.pool[i].source[j].bits >
				random_state.pool[i].thresh) {
				random_state.pool[i].source[j].bits = 0;
				random_state.pool[i].source[j].frac = 0;
			}
		}
	}

	/* 6. Wipe memory of intermediate values */

	memset((void *)v, 0, sizeof(v));
	memset((void *)temp, 0, sizeof(temp));
	memset((void *)hash, 0, sizeof(hash));

	/* 7. Dump to seed file */
	/* XXX Not done here yet */

	/* Release the reseed mutex */
	mtx_exit(&random_reseed_mtx, MTX_DEF);

#ifdef DEBUG
	printf("Reseed finish\n");
#endif

}

u_int
read_random(struct proc *proc, void *buf, u_int count)
{
	static u_int64_t genval;
	static int cur = 0;
	static int gate = 1;
	u_int i;
	u_int retval;

	/* The reseed task must not be jumped on */
	mtx_enter(&random_reseed_mtx, MTX_DEF);

	if (gate) {
		generator_gate();
		random_state.outputblocks = 0;
		gate = 0;
	}
	if (count >= sizeof(random_state.counter)) {
		retval = 0;
		for (i = 0; i < count; i += sizeof(random_state.counter)) {
			random_state.counter++;
			yarrow_encrypt(&random_state.key, &random_state.counter,
				&genval, sizeof(random_state.counter));
			memcpy((char *)buf + i, &genval,
				sizeof(random_state.counter));
			if (++random_state.outputblocks >= random_state.gengateinterval) {
				generator_gate();
				random_state.outputblocks = 0;
			}
			retval += sizeof(random_state.counter);
		}
	}
	else {
		if (!cur) {
			random_state.counter++;
			yarrow_encrypt(&random_state.key, &random_state.counter,
				&genval, sizeof(random_state.counter));
			memcpy(buf, &genval, count);
			cur = sizeof(random_state.counter) - count;
			if (++random_state.outputblocks >= random_state.gengateinterval) {
				generator_gate();
				random_state.outputblocks = 0;
			}
			retval = count;
		}
		else {
			retval = cur < count ? cur : count;
			memcpy(buf,
				(char *)&genval +
					(sizeof(random_state.counter) - cur),
				retval);
			cur -= retval;
		}
	}
	mtx_exit(&random_reseed_mtx, MTX_DEF);
	return retval;
}

void
write_random(void *buf, u_int count)
{
	u_int i;
	struct timespec timebuf;

	/* arbitrarily break the input up into 8-byte chunks */
	for (i = 0; i < count; i += 8) {
		nanotime(&timebuf);
		random_harvest_internal(&timebuf, (char *)buf + i, 8, 0, 0,
			RANDOM_WRITE);
	}

	/* Maybe the loop iterated at least once */
	if (i > count)
		i -= 8;

	/* Get the last bytes even if the input length is not a multiple of 8 */
	count %= 8;
	if (count) {
		nanotime(&timebuf);
		random_harvest_internal(&timebuf, (char *)buf + i, count, 0, 0,
			RANDOM_WRITE);
	}

	/* Explicit reseed */
	reseed(FAST);
}

static void
generator_gate(void)
{
	int i;
	u_char temp[KEYSIZE];

#ifdef DEBUG
	printf("Generator gate\n");
#endif

	for (i = 0; i < KEYSIZE; i += sizeof(random_state.counter)) {
		random_state.counter++;
		yarrow_encrypt(&random_state.key, &random_state.counter,
			&(temp[i]), sizeof(random_state.counter));
	}

	yarrow_encrypt_init(&random_state.key, temp, KEYSIZE);
	memset((void *)temp, 0, KEYSIZE);

#ifdef DEBUG
	printf("Generator gate finish\n");
#endif
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 */

static void
random_harvest_internal(struct timespec *timep, void *entropy, u_int count,
	u_int bits, u_int frac, enum esource origin)
{
	struct harvest *event;
	u_int64_t entropy_buf;

#if 0
#ifdef DEBUG
	printf("Random harvest\n");
#endif
#endif
	event = malloc(sizeof(struct harvest), M_TEMP, M_NOWAIT);

	if (origin < ENTROPYSOURCE && event != NULL) {

		/* nanotime provides clock jitter */
		event->time = *timep;

		/* the harvested entropy */
		count = count > sizeof(entropy_buf)
			? sizeof(entropy_buf)
			: count;
		memcpy(event->entropy, entropy, count);

		event->size = count;
		event->bits = bits;
		event->frac = frac;
		event->source = origin;

		/* protect the queue from simultaneous updates */
		mtx_enter(&random_harvest_mtx, MTX_DEF);

		/* toggle the pool for next insertion */
		event->pool = random_state.which;
		random_state.which = !random_state.which;

		TAILQ_INSERT_TAIL(&harvestqueue, event, harvest);

		mtx_exit(&random_harvest_mtx, MTX_DEF);
	}
}
