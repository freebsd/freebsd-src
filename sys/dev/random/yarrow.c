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
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/mutex.h>
#include <sys/select.h>
#include <sys/random.h>
#include <sys/types.h>
#include <sys/unistd.h>

#include <machine/atomic.h>
#include <machine/cpu.h>

#include <crypto/blowfish/blowfish.h>

#include <dev/random/hash.h>
#include <dev/random/yarrow.h>

/* #define DEBUG */
/* #define DEBUG1 */	/* Very noisy - prints plenty harvesting stats */

static void generator_gate(void);
static void reseed(int);
static void random_harvest_internal(u_int64_t, void *, u_int, u_int, u_int, enum esource);

static void random_kthread(void *);

/* Structure holding the entropy state */
struct random_state random_state;

/* These are used to queue harvested packets of entropy. The entropy
 * buffer size is pretty arbitrary.
 */
struct harvest {
	u_int64_t somecounter;		/* fast counter for clock jitter */
	u_char entropy[HARVESTSIZE];	/* the harvested entropy */
	u_int size, bits, frac;		/* stats about the entropy */
	enum esource source;		/* stats about the entropy */
	u_int pool;			/* which pool this goes into */
};

/* Ring buffer holding harvested entropy */
static struct harvestring {
	struct mtx	lockout_mtx;
	int		head;
	int		tail;
	struct harvest	data[HARVEST_RING_SIZE];
} harvestring;

/* The reseed thread mutex */
static struct mtx random_reseed_mtx;

/* <0 to end the kthread, 0 to let it run */
static int random_kthread_control = 0;

static struct proc *random_kthread_proc;

static void
random_kthread(void *arg /* NOTUSED */)
{
	int pl, src, overthreshhold[2], head, newtail;
	struct harvest *event;
	struct source *source;

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("OWNERSHIP Giant == %d sched_lock == %d\n",
		mtx_owned(&Giant), mtx_owned(&sched_lock));
	mtx_exit(&Giant, MTX_DEF);
#endif

	for (pl = 0; pl < 2; pl++)
		yarrow_hash_init(&random_state.pool[pl].hash, NULL, 0);

	for (;;) {

		head = atomic_load_acq_int(&harvestring.head);
		newtail = (harvestring.tail + 1) % HARVEST_RING_SIZE;
		if (harvestring.tail == head)
			tsleep(&harvestring.head, PUSER, "rndslp", hz/10);

		else {
#ifdef DEBUG1
			mtx_enter(&Giant, MTX_DEF);
			printf("HARVEST src=%d bits=%d/%d pool=%d count=%lld\n",
				event->source, event->bits, event->frac,
				event->pool, event->somecounter);
			mtx_exit(&Giant, MTX_DEF);
#endif

			/* Suck the harvested entropy out of the queue and hash
			 * it into the appropriate pool.
			 */

			event = &harvestring.data[harvestring.tail];
			harvestring.tail = newtail;

			source = &random_state.pool[event->pool].source[event->source];
			yarrow_hash_iterate(&random_state.pool[event->pool].hash,
				event->entropy, sizeof(event->entropy));
			yarrow_hash_iterate(&random_state.pool[event->pool].hash,
				&event->somecounter, sizeof(event->somecounter));
			source->frac += event->frac;
			source->bits += event->bits + source->frac/1024;
			source->frac %= 1024;

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
		if (random_kthread_control != 0) {
#ifdef DEBUG
			mtx_enter(&Giant, MTX_DEF);
			printf("Random kthread setting terminate\n");
			mtx_exit(&Giant, MTX_DEF);
#endif
			random_set_wakeup_exit(&random_kthread_control);
			/* NOTREACHED */
			break;
		}

	}

}

int
random_init(void)
{
	int error;

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Random initialise\n");
	mtx_exit(&Giant, MTX_DEF);
#endif

	random_state.gengateinterval = 10;
	random_state.bins = 10;
	random_state.pool[0].thresh = 100;
	random_state.pool[1].thresh = 160;
	random_state.slowoverthresh = 2;
	random_state.which = FAST;

	/* Initialise the mutexes */
	mtx_init(&random_reseed_mtx, "random reseed", MTX_DEF);
	mtx_init(&harvestring.lockout_mtx, "random harvest", MTX_DEF);

	harvestring.head = 0;
	harvestring.tail = 0;

	/* Start the hash/reseed thread */
	error = kthread_create(random_kthread, NULL,
		&random_kthread_proc, RFHIGHPID, "random");
	if (error != 0)
		return error;

	/* Register the randomness harvesting routine */
	random_init_harvester(random_harvest_internal, read_random_real);

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Random initalise finish\n");
	mtx_exit(&Giant, MTX_DEF);
#endif

	return 0;
}

void
random_deinit(void)
{
#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Random deinitalise\n");
	mtx_exit(&Giant, MTX_DEF);
#endif

	/* Deregister the randomness harvesting routine */
	random_deinit_harvester();

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Random deinitalise waiting for thread to terminate\n");
	mtx_exit(&Giant, MTX_DEF);
#endif

	/* Command the hash/reseed thread to end and wait for it to finish */
	mtx_enter(&harvestring.lockout_mtx, MTX_DEF);
	random_kthread_control = -1;
	msleep((void *)&random_kthread_control, &harvestring.lockout_mtx, PUSER,
		"rndend", 0);
	mtx_exit(&harvestring.lockout_mtx, MTX_DEF);

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Random deinitalise removing mutexes\n");
	mtx_exit(&Giant, MTX_DEF);
#endif

	/* Remove the mutexes */
	mtx_destroy(&random_reseed_mtx);
	mtx_destroy(&harvestring.lockout_mtx);

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Random deinitalise finish\n");
	mtx_exit(&Giant, MTX_DEF);
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
	mtx_enter(&Giant, MTX_DEF);
	printf("Reseed type %d\n", fastslow);
	mtx_exit(&Giant, MTX_DEF);
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
	mtx_enter(&Giant, MTX_DEF);
	printf("Reseed finish\n");
	mtx_exit(&Giant, MTX_DEF);
#endif

	if (!random_state.seeded) {
		random_state.seeded = 1;
		selwakeup(&random_state.rsel);
		wakeup(&random_state);
	}

}

u_int
read_random_real(void *buf, u_int count)
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

	/* Break the input up into HARVESTSIZE chunks.
	 * The writer has too much control here, so "estimate" the
	 * the entropy as zero.
	 */
	for (i = 0; i < count; i += HARVESTSIZE) {
		random_harvest_internal(get_cyclecount(), (char *)buf + i,
			HARVESTSIZE, 0, 0, RANDOM_WRITE);
	}

	/* Maybe the loop iterated at least once */
	if (i > count)
		i -= HARVESTSIZE;

	/* Get the last bytes even if the input length is not
	 * a multiple of HARVESTSIZE.
	 */
	count %= HARVESTSIZE;
	if (count) {
		random_harvest_internal(get_cyclecount(), (char *)buf + i,
			count, 0, 0, RANDOM_WRITE);
	}
}

static void
generator_gate(void)
{
	int i;
	u_char temp[KEYSIZE];

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Generator gate\n");
	mtx_exit(&Giant, MTX_DEF);
#endif

	for (i = 0; i < KEYSIZE; i += sizeof(random_state.counter)) {
		random_state.counter++;
		yarrow_encrypt(&random_state.key, &random_state.counter,
			&(temp[i]), sizeof(random_state.counter));
	}

	yarrow_encrypt_init(&random_state.key, temp, KEYSIZE);
	memset((void *)temp, 0, KEYSIZE);

#ifdef DEBUG
	mtx_enter(&Giant, MTX_DEF);
	printf("Generator gate finish\n");
	mtx_exit(&Giant, MTX_DEF);
#endif
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 */

static void
random_harvest_internal(u_int64_t somecounter, void *entropy, u_int count,
	u_int bits, u_int frac, enum esource origin)
{
	struct harvest *harvest;
	int newhead, tail;

#ifdef DEBUG1
	mtx_enter(&Giant, MTX_DEF);
	printf("Random harvest\n");
	mtx_exit(&Giant, MTX_DEF);
#endif
	if (origin < ENTROPYSOURCE) {

		/* Add the harvested data to the ring buffer, but
		 * do not block.
		 */
		if (mtx_try_enter(&harvestring.lockout_mtx, MTX_DEF)) {

			tail = atomic_load_acq_int(&harvestring.tail);
			newhead = (harvestring.head + 1) % HARVEST_RING_SIZE;

			if (newhead != tail) {

				harvest = &harvestring.data[harvestring.head];

				/* toggle the pool for next insertion */
				harvest->pool = random_state.which;
				random_state.which = !random_state.which;

				/* Stuff the harvested data into the ring */
				harvest->somecounter = somecounter;
				count = count > HARVESTSIZE ? HARVESTSIZE : count;
				memcpy(harvest->entropy, entropy, count);
				harvest->size = count;
				harvest->bits = bits;
				harvest->frac = frac;
				harvest->source = origin;

				/* Bump the ring counter and shake the reseed
				 * process
				 */
				harvestring.head = newhead;
				wakeup(&harvestring.head);

			}
			mtx_exit(&harvestring.lockout_mtx, MTX_DEF);

		}

	}
}

/* Helper routine to perform explicit reseeds */
void
random_reseed(void)
{
	reseed(FAST);
}
