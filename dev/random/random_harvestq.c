/*-
 * Copyright (c) 2013 Arthur Mesh
 * Copyright (c) 2000-2009 Mark R V Murray
 * Copyright (c) 2004 Robert N. M. Watson
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <dev/random/randomdev_soft.h>

#include "random_harvestq.h"

#define RANDOM_FIFO_MAX	256	/* How many events to queue up */

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers");

/*
 * The harvest mutex protects the consistency of the entropy fifos and
 * empty fifo.
 */
struct mtx	harvest_mtx;

/* Lockable FIFO queue holding entropy buffers */
struct entropyfifo {
	int count;
	STAILQ_HEAD(harvestlist, harvest) head;
};

/* Empty entropy buffers */
static struct entropyfifo emptyfifo;

#define EMPTYBUFFERS	1024

/* Harvested entropy */
static struct entropyfifo harvestfifo[ENTROPYSOURCE];

/* <0 to end the kthread, 0 to let it run, 1 to flush the harvest queues */
int random_kthread_control = 0;

static struct proc *random_kthread_proc;

static void
random_kthread(void *arg)
{
	STAILQ_HEAD(, harvest) local_queue;
	struct harvest *event = NULL;
	int local_count;
	enum esource source;
	event_proc_f func = arg;

	STAILQ_INIT(&local_queue);
	local_count = 0;

	/* Process until told to stop */
	mtx_lock_spin(&harvest_mtx);
	for (; random_kthread_control >= 0;) {

		/* Cycle through all the entropy sources */
		for (source = RANDOM_START; source < ENTROPYSOURCE; source++) {
			/*
			 * Drain entropy source records into a thread-local
			 * queue for processing while not holding the mutex.
			 */
			STAILQ_CONCAT(&local_queue, &harvestfifo[source].head);
			local_count += harvestfifo[source].count;
			harvestfifo[source].count = 0;
		}

		/*
		 * Deal with events, if any, dropping the mutex as we process
		 * each event.  Then push the events back into the empty
		 * fifo.
		 */
		if (!STAILQ_EMPTY(&local_queue)) {
			mtx_unlock_spin(&harvest_mtx);
			STAILQ_FOREACH(event, &local_queue, next)
				func(event);
			mtx_lock_spin(&harvest_mtx);
			STAILQ_CONCAT(&emptyfifo.head, &local_queue);
			emptyfifo.count += local_count;
			local_count = 0;
		}

		KASSERT(local_count == 0, ("random_kthread: local_count %d",
		    local_count));

		/*
		 * If a queue flush was commanded, it has now happened,
		 * and we can mark this by resetting the command.
		 */
		if (random_kthread_control == 1)
			random_kthread_control = 0;

		/* Work done, so don't belabour the issue */
		msleep_spin_sbt(&random_kthread_control, &harvest_mtx,
		    "-", SBT_1S / 10, 0, C_PREL(1));

	}
	mtx_unlock_spin(&harvest_mtx);

	random_set_wakeup_exit(&random_kthread_control);
	/* NOTREACHED */
}

void
random_harvestq_init(event_proc_f cb)
{
	int error, i;
	struct harvest *np;
	enum esource e;

	/* Initialise the harvest fifos */
	STAILQ_INIT(&emptyfifo.head);
	emptyfifo.count = 0;
	for (i = 0; i < EMPTYBUFFERS; i++) {
		np = malloc(sizeof(struct harvest), M_ENTROPY, M_WAITOK);
		STAILQ_INSERT_TAIL(&emptyfifo.head, np, next);
	}
	for (e = RANDOM_START; e < ENTROPYSOURCE; e++) {
		STAILQ_INIT(&harvestfifo[e].head);
		harvestfifo[e].count = 0;
	}

	mtx_init(&harvest_mtx, "entropy harvest mutex", NULL, MTX_SPIN);


	/* Start the hash/reseed thread */
	error = kproc_create(random_kthread, cb,
	    &random_kthread_proc, RFHIGHPID, 0, "rand_harvestq"); /* RANDOM_CSPRNG_NAME */

	if (error != 0)
		panic("Cannot create entropy maintenance thread.");
}

void
random_harvestq_deinit(void)
{
	struct harvest *np;
	enum esource e;

	/* Destroy the harvest fifos */
	while (!STAILQ_EMPTY(&emptyfifo.head)) {
		np = STAILQ_FIRST(&emptyfifo.head);
		STAILQ_REMOVE_HEAD(&emptyfifo.head, next);
		free(np, M_ENTROPY);
	}
	for (e = RANDOM_START; e < ENTROPYSOURCE; e++) {
		while (!STAILQ_EMPTY(&harvestfifo[e].head)) {
			np = STAILQ_FIRST(&harvestfifo[e].head);
			STAILQ_REMOVE_HEAD(&harvestfifo[e].head, next);
			free(np, M_ENTROPY);
		}
	}

	mtx_destroy(&harvest_mtx);
}

/*
 * Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 */
void
random_harvestq_internal(u_int64_t somecounter, const void *entropy,
    u_int count, u_int bits, u_int frac, enum esource origin)
{
	struct harvest *event;

	KASSERT(origin >= RANDOM_START && origin <= RANDOM_PURE,
	    ("random_harvest_internal: origin %d invalid\n", origin));

	/* Lockless read to avoid lock operations if fifo is full. */
	if (harvestfifo[origin].count >= RANDOM_FIFO_MAX)
		return;

	mtx_lock_spin(&harvest_mtx);

	/*
	 * Don't make the harvest queues too big - help to prevent low-grade
	 * entropy swamping
	 */
	if (harvestfifo[origin].count < RANDOM_FIFO_MAX) {
		event = STAILQ_FIRST(&emptyfifo.head);
		if (event != NULL) {
			/* Add the harvested data to the fifo */
			STAILQ_REMOVE_HEAD(&emptyfifo.head, next);
			harvestfifo[origin].count++;
			event->somecounter = somecounter;
			event->size = count;
			event->bits = bits;
			event->frac = frac;
			event->source = origin;

			/* XXXX Come back and make this dynamic! */
			count = MIN(count, HARVESTSIZE);
			memcpy(event->entropy, entropy, count);

#if 0
			{
			int i;
			printf("Harvest:%16jX ", event->somecounter);
			for (i = 0; i < event->size; i++)
				printf("%02X", event->entropy[i]);
			for (; i < 16; i++)
				printf("  ");
			printf(" %2d 0x%2X.%03X %02X\n", event->size, event->bits, event->frac, event->source);
			}
#endif

			STAILQ_INSERT_TAIL(&harvestfifo[origin].head,
			    event, next);
		}
	}
	mtx_unlock_spin(&harvest_mtx);
}

