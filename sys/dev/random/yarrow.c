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
                    it is not yet cryptographically secure!! */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/linker.h>
#include <sys/libkern.h>
#include <sys/mbuf.h>
#include <sys/random.h>
#include <sys/time.h>
#include <sys/types.h>
#include <crypto/blowfish/blowfish.h>

#include <dev/randomdev/yarrow.h>

/* #define DEBUG */

static void generator_gate(void);
static void reseed(int);
static void random_harvest_internal(struct timespec *nanotime, u_int64_t entropy, u_int bits, u_int frac, enum esource source);

/* Structure holding the entropy state */
struct random_state random_state;

/* When enough entropy has been harvested, asynchronously "stir" it in */
/* The regate task is run at splsofttq()                               */
static struct task regate_task[2];

struct context {
	u_int pool;
} context[2] =	{
			{ 0 },
			{ 1 }
		};

static void
regate(void *context, int pending)
{
#ifdef DEBUG
	printf("Regate task\n");
#endif
	reseed(((struct context *)context)->pool);
}

void
random_init(void)
{
#ifdef DEBUG
	printf("Random init\n");
#endif
	random_state.gengateinterval = 10;
	random_state.bins = 10;
	random_state.pool[0].thresh = 100;
	random_state.pool[1].thresh = 160;
	random_state.slowoverthresh = 2;
	random_state.which = FAST;
	TASK_INIT(&regate_task[FAST], FAST, &regate, (void *)&context[FAST]);
	TASK_INIT(&regate_task[SLOW], SLOW, &regate, (void *)&context[SLOW]);
	random_init_harvester(random_harvest_internal);
}

void
random_deinit(void)
{
#ifdef DEBUG
	printf("Random deinit\n");
#endif
	random_deinit_harvester();
}

static void
reseed(int fastslow)
{
	/* Interrupt-context stack is a limited resource; make static */
	/* large structures; XXX Revisit - needs to move to the large */
	/* random_state structure.                                    */
	static unsigned char v[TIMEBIN][KEYSIZE];	/* v[i] */
	unsigned char hash[KEYSIZE];			/* h' */
	static BF_KEY hashkey;
	unsigned char ivec[8];
	unsigned char temp[KEYSIZE];
	struct entropy *bucket;
	int i, j;

#ifdef DEBUG
	printf("Reseed type %d\n", fastslow);
#endif

	/* 1. Hash the accumulated entropy into v[0] */

	bzero((void *)&v[0], KEYSIZE);
	if (fastslow == SLOW) {
		/* Feed a hash of the slow pool into the fast pool */
		for (i = 0; i < ENTROPYSOURCE; i++) {
			for (j = 0; j < ENTROPYBIN; j++) {
				bucket = &random_state.pool[SLOW].source[i].entropy[j];
				if(bucket->nanotime.tv_sec || bucket->nanotime.tv_nsec) {
					BF_set_key(&hashkey, sizeof(struct entropy),
						(void *)bucket);
					BF_cbc_encrypt(v[0], temp, KEYSIZE, &hashkey, ivec,
						BF_ENCRYPT);
					memcpy(&v[0], temp, KEYSIZE);
					bucket->nanotime.tv_sec = 0;
					bucket->nanotime.tv_nsec = 0;
				}
			}
		}
	}

	for (i = 0; i < ENTROPYSOURCE; i++) {
		for (j = 0; j < ENTROPYBIN; j++) {
			bucket = &random_state.pool[FAST].source[i].entropy[j];
			if(bucket->nanotime.tv_sec || bucket->nanotime.tv_nsec) {
				BF_set_key(&hashkey, sizeof(struct entropy), (void *)bucket);
				BF_cbc_encrypt(v[0], temp, KEYSIZE, &hashkey, ivec, BF_ENCRYPT);
				memcpy(&v[0], temp, KEYSIZE);
				bucket->nanotime.tv_sec = 0;
				bucket->nanotime.tv_nsec = 0;
			}
		}
	}

	/* 2. Compute hash values for all v. _Supposed_ to be computationally */
	/*    intensive.                                                      */

	if (random_state.bins > TIMEBIN)
		random_state.bins = TIMEBIN;
	for (i = 1; i < random_state.bins; i++) {
		bzero((void *)&v[i], KEYSIZE);
		/* v[i] #= h(v[i-1]) */
		BF_set_key(&hashkey, KEYSIZE, v[i - 1]);
		BF_cbc_encrypt(v[i], temp, KEYSIZE, &hashkey, ivec, BF_ENCRYPT);
		memcpy(&v[i], temp, KEYSIZE);
		/* v[i] #= h(v[0]) */
		BF_set_key(&hashkey, KEYSIZE, v[0]);
		BF_cbc_encrypt(v[i], temp, KEYSIZE, &hashkey, ivec, BF_ENCRYPT);
		memcpy(&v[i], temp, KEYSIZE);
		/* v[i] #= h(i) */
		BF_set_key(&hashkey, sizeof(int), (unsigned char *)&i);
		BF_cbc_encrypt(v[i], temp, KEYSIZE, &hashkey, ivec, BF_ENCRYPT);
		memcpy(&v[i], temp, KEYSIZE);
	}

	/* 3. Compute a new Key. */

	bzero((void *)hash, KEYSIZE);
	BF_set_key(&hashkey, KEYSIZE, (unsigned char *)&random_state.key);
	BF_cbc_encrypt(hash, temp, KEYSIZE, &hashkey, ivec, BF_ENCRYPT);
	memcpy(hash, temp, KEYSIZE);
	for (i = 1; i < random_state.bins; i++) {
		BF_set_key(&hashkey, KEYSIZE, v[i]);
		BF_cbc_encrypt(hash, temp, KEYSIZE, &hashkey, ivec, BF_ENCRYPT);
		memcpy(hash, temp, KEYSIZE);
	}
	BF_set_key(&random_state.key, KEYSIZE, hash);

	/* 4. Recompute the counter */

	random_state.counter = 0;
	BF_cbc_encrypt((unsigned char *)&random_state.counter, temp,
		sizeof(random_state.counter), &random_state.key,
		random_state.ivec, BF_ENCRYPT);
	memcpy(&random_state.counter, temp, random_state.counter);

	/* 5. Reset entropy estimate accumulators to zero */

	for (i = 0; i <= fastslow; i++) {
		for (j = 0; j < ENTROPYSOURCE; j++) {
			random_state.pool[i].source[j].bits = 0;
			random_state.pool[i].source[j].frac = 0;
		}
	}

	/* 6. Wipe memory of intermediate values */

	bzero((void *)v, sizeof(v));
	bzero((void *)temp, sizeof(temp));
	bzero((void *)hash, sizeof(hash));

	/* 7. Dump to seed file (XXX done by external process?) */

}

u_int
read_random(char *buf, u_int count)
{
	static int cur = 0;
	static int gate = 1;
	u_int i;
	u_int retval;
	u_int64_t genval;
	intrmask_t mask;

	/* The reseed task must not be jumped on */
	mask = splsofttq();

	if (gate) {
		generator_gate();
		random_state.outputblocks = 0;
		gate = 0;
	}
	if (count >= sizeof(random_state.counter)) {
		retval = 0;
		for (i = 0; i < count; i += sizeof(random_state.counter)) {
			random_state.counter++;
			BF_cbc_encrypt((unsigned char *)&random_state.counter,
				(unsigned char *)&genval,
				sizeof(random_state.counter),
				&random_state.key, random_state.ivec, BF_ENCRYPT);
			memcpy(&buf[i], &genval, sizeof(random_state.counter));
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
			BF_cbc_encrypt((unsigned char *)&random_state.counter,
				(unsigned char *)&genval,
				sizeof(random_state.counter),
				&random_state.key, random_state.ivec,
				BF_ENCRYPT);
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
				(char *)&random_state.counter +
					(sizeof(random_state.counter) - retval),
				retval);
			cur -= retval;
		}
	}
	splx(mask);
	return retval;
}

void
write_random(char *buf, u_int count)
{
	u_int i;
	intrmask_t mask;
	struct timespec nanotime;

	/* The reseed task must not be jumped on */
	mask = splsofttq();
	for (i = 0; i < count/sizeof(u_int64_t); i++) {
		getnanotime(&nanotime);
		random_harvest_internal(&nanotime,
			*(u_int64_t *)&buf[i*sizeof(u_int64_t)],
			0, 0, RANDOM_WRITE);
	}
	reseed(FAST);
	splx(mask);
}

static void
generator_gate(void)
{
	int i;
	unsigned char temp[KEYSIZE];
	intrmask_t mask;

#ifdef DEBUG
	printf("Generator gate\n");
#endif

	/* The reseed task must not be jumped on */
	mask = splsofttq();

	for (i = 0; i < KEYSIZE; i += sizeof(random_state.counter)) {
		random_state.counter++;
		BF_cbc_encrypt((unsigned char *)&random_state.counter,
			&(temp[i]), sizeof(random_state.counter),
			&random_state.key, random_state.ivec, BF_ENCRYPT);
	}

	BF_set_key(&random_state.key, KEYSIZE, temp);
	bzero((void *)temp, KEYSIZE);

	splx(mask);
}

/* Entropy harvesting routine. This is supposed to be fast; do */
/* not do anything slow in here!                               */

static void
random_harvest_internal(struct timespec *nanotime, u_int64_t entropy,
	u_int bits, u_int frac, enum esource origin)
{
	u_int insert;
	int which;	/* fast or slow */
	struct entropy *bucket;
	struct source *source;
	struct pool *pool;
	intrmask_t mask;

#ifdef DEBUG
	printf("Random harvest\n");
#endif
	if (origin < ENTROPYSOURCE) {

		/* Called inside irq handlers; protect from interference */
		mask = splhigh();

		which = random_state.which;
		pool = &random_state.pool[which];
		source = &pool->source[origin];

		insert = source->current + 1;
		if (insert >= ENTROPYBIN)
			insert = 0;

		bucket = &source->entropy[insert];

		if (!bucket->nanotime.tv_sec && !bucket->nanotime.tv_nsec) {

			/* nanotime provides clock jitter */
			bucket->nanotime = *nanotime;

			/* the harvested entropy */
			bucket->data = entropy;

			/* update the estimates - including "fractional bits" */
			source->bits += bits;
			source->frac += frac;
			if (source->frac >= 1024) {
				source->bits += source->frac / 1024;
				source->frac %= 1024;
			}
			if (source->bits >= pool->thresh) {
				/* XXX Slowoverthresh nees to be considered */
				taskqueue_enqueue(taskqueue_swi, &regate_task[which]);
			}

			/* bump the insertion point */
			source->current = insert;

			/* toggle the pool for next insertion */
			random_state.which = !random_state.which;

		}
		splx(mask);
	}
}
