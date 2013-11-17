/*-
 * Copyright (c) 2000-2013 Mark R V Murray
 * Copyright (c) 2013 Arthur Mesh
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_random.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/live_entropy_sources.h>

/* List for the dynamic sysctls */
static struct sysctl_ctx_list random_clist;

/*
 * How many events to queue up. We create this many items in
 * an 'empty' queue, then transfer them to the 'harvest' queue with
 * supplied junk. When used, they are transferred back to the
 * 'empty' queue.
 */
#define RANDOM_FIFO_MAX	1024

/*
 * The harvest mutex protects the consistency of the entropy Fifos and
 * empty fifo and other associated structures.
 */
static struct mtx harvest_mtx;

/* Lockable FIFO queue holding entropy buffers */
struct entropyfifo {
	STAILQ_HEAD(harvestlist, harvest_event) head;
};

/* Empty entropy buffers */
static struct entropyfifo emptyfifo;

/* Harvested entropy */
static struct entropyfifo harvestfifo;

/* Round-robin destination cache. */
u_int harvest_destination[ENTROPYSOURCE];

/* Function called to process one harvested stochastic event */
void (*harvest_process_event)(struct harvest_event *);

/* Allow the sysadmin to select the broad category of
 * entropy types to harvest.
 */
static u_int harvest_source_mask = ((1<<RANDOM_ENVIRONMENTAL_END) - 1);

/* Pool count is used by anything needing to know how many entropy
 * pools are currently being maintained.
 * This is of use to (e.g.) the live source feed where we need to give
 * all the pools a top-up.
 */
int harvest_pool_count;

/* <0 to end the kthread, 0 to let it run, 1 to flush the harvest queues */
static int random_kthread_control = 0;

static struct proc *random_kthread_proc;

static void
random_kthread(void *arg __unused)
{
	struct entropyfifo local_queue;
	struct harvest_event *event = NULL;

	STAILQ_INIT(&local_queue.head);

	/* Process until told to stop */
	mtx_lock_spin(&harvest_mtx);
	for (; random_kthread_control >= 0;) {

		/*
		 * Grab all the entropy events.
		 * Drain entropy source records into a thread-local
		 * queue for processing while not holding the mutex.
		 */
		STAILQ_CONCAT(&local_queue.head, &harvestfifo.head);

		/*
		 * Deal with events, if any.
		 * Then transfer the used events back into the empty fifo.
		 */
		if (!STAILQ_EMPTY(&local_queue.head)) {
			mtx_unlock_spin(&harvest_mtx);
			STAILQ_FOREACH(event, &local_queue.head, he_next)
				harvest_process_event(event);
			mtx_lock_spin(&harvest_mtx);
			STAILQ_CONCAT(&emptyfifo.head, &local_queue.head);
		}

		/*
		 * Give the fast hardware sources a go
		 */
		mtx_unlock_spin(&harvest_mtx);
		live_entropy_sources_feed();
		mtx_lock_spin(&harvest_mtx);

		/*
		 * If a queue flush was commanded, it has now happened,
		 * and we can mark this by resetting the command.
		 */

		if (random_kthread_control == 1)
			random_kthread_control = 0;

		/* Work done, so don't belabour the issue */
		msleep_spin_sbt(&random_kthread_control, &harvest_mtx,
		    "-", SBT_1S/10, 0, C_PREL(1));

	}
	mtx_unlock_spin(&harvest_mtx);

	randomdev_set_wakeup_exit(&random_kthread_control);
	/* NOTREACHED */
}

void
random_harvestq_flush(void)
{

	/* Command a entropy queue flush and wait for it to finish */
	random_kthread_control = 1;
	while (random_kthread_control)
		pause("-", hz/10);

#if 0
#if defined(RANDOM_YARROW)
	random_yarrow_reseed();
#endif
#if defined(RANDOM_FORTUNA)
	random_fortuna_reseed();
#endif
#endif
}

/* ARGSUSED */
RANDOM_CHECK_UINT(harvestmask, 0, ((1<<RANDOM_ENVIRONMENTAL_END) - 1));

/* ARGSUSED */
static int
random_print_harvestmask(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	int error, i;

	error = sysctl_wire_old_buffer(req, 0);
	if (error == 0) {
		sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
		for (i = 31; i >= 0; i--)
			sbuf_cat(&sbuf, (harvest_source_mask & (1<<i)) ? "1" : "0");
		error = sbuf_finish(&sbuf);
		sbuf_delete(&sbuf);
	}

	return (error);
}

void
random_harvestq_init(void (*event_processor)(struct harvest_event *), int poolcount)
{
	uint8_t *keyfile, *data;
	int error, i;
	size_t size, j;
	struct harvest_event *np;
	struct sysctl_oid *random_sys_o;

#ifdef RANDOM_DEBUG
	printf("random: %s\n", __func__);
#endif

	random_sys_o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_STATIC_CHILDREN(_kern_random),
	    OID_AUTO, "harvest", CTLFLAG_RW, 0,
	    "Entropy Device Parameters");

	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "mask", CTLTYPE_UINT | CTLFLAG_RW,
	    &harvest_source_mask, ((1<<RANDOM_ENVIRONMENTAL_END) - 1),
	    random_check_uint_harvestmask, "IU",
	    "Entropy harvesting mask");

	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "mask_bin", CTLTYPE_STRING | CTLFLAG_RD,
	    NULL, 0, random_print_harvestmask, "A", "Entropy harvesting mask (printable)");

	/* Initialise the harvest fifos */

	/* Contains the currently unused event structs. */
	STAILQ_INIT(&emptyfifo.head);
	for (i = 0; i < RANDOM_FIFO_MAX; i++) {
		np = malloc(sizeof(struct harvest_event), M_ENTROPY, M_WAITOK);
		STAILQ_INSERT_TAIL(&emptyfifo.head, np, he_next);
	}

	/* Will contain the queued-up events. */
	STAILQ_INIT(&harvestfifo.head);

	/* Point to the correct event_processing function */
	harvest_process_event = event_processor;

	/* Store the pool count (used by live source feed) */
	harvest_pool_count = poolcount;

	mtx_init(&harvest_mtx, "entropy harvest mutex", NULL, MTX_SPIN);

	/* Start the hash/reseed thread */
	error = kproc_create(random_kthread, NULL,
	    &random_kthread_proc, RFHIGHPID, 0, "rand_harvestq");

	if (error != 0)
		panic("Cannot create entropy maintenance thread.");

	/* Get entropy that may have been preloaded by loader(8)
	 * and use it to pre-charge the entropy harvest queue.
	 */
	keyfile = preload_search_by_type("/boot/entropy");
	if (keyfile != NULL) {
		data = preload_fetch_addr(keyfile);
		size = preload_fetch_size(keyfile);
		if (data != NULL && size != 0) {
			for (j = 0; j < size; j += 16)
				random_harvestq_internal(data + j, 16, 16, RANDOM_CACHED);
			printf("random: read %zu bytes from preloaded cache\n", size);
			bzero(data, size);
		}
		else
			printf("random: no preloaded entropy cache\n");
	}

}

void
random_harvestq_deinit(void)
{
	struct harvest_event *np;

#ifdef RANDOM_DEBUG
	printf("random: %s\n", __func__);
#endif

	/*
	 * Command the hash/reseed thread to end and wait for it to finish
	 */
	random_kthread_control = -1;
	tsleep((void *)&random_kthread_control, 0, "term", 0);

	/* Destroy the harvest fifos */
	while (!STAILQ_EMPTY(&emptyfifo.head)) {
		np = STAILQ_FIRST(&emptyfifo.head);
		STAILQ_REMOVE_HEAD(&emptyfifo.head, he_next);
		free(np, M_ENTROPY);
	}
	while (!STAILQ_EMPTY(&harvestfifo.head)) {
		np = STAILQ_FIRST(&harvestfifo.head);
		STAILQ_REMOVE_HEAD(&harvestfifo.head, he_next);
		free(np, M_ENTROPY);
	}

	mtx_destroy(&harvest_mtx);

	sysctl_ctx_free(&random_clist);
}

/*
 * Entropy harvesting routine.
 * This is supposed to be fast; do not do anything slow in here!
 *
 * It is also illegal (and morally reprehensible) to insert any
 * high-rate data here. "High-rate" is define as a data source
 * that will usually cause lots of failures of the "Lockless read"
 * check a few lines below. This includes the "always-on" sources
 * like the Intel "rdrand" or the VIA Nehamiah "xstore" sources.
 */
/* XXXRW: get_cyclecount() is cheap on most modern hardware, where cycle
 * counters are built in, but on older hardware it will do a real time clock
 * read which can be quite expensive.
 */
void
random_harvestq_internal(const void *entropy, u_int count, u_int bits,
    enum random_entropy_source origin)
{
	struct harvest_event *event;
	size_t c;

	KASSERT(origin >= RANDOM_START && origin < ENTROPYSOURCE,
	    ("random_harvest_internal: origin %d invalid\n", origin));

	/* Mask out unwanted sources */
	if (!(harvest_source_mask & (1<<origin)))
		return;

	/* Lockless check to avoid lock operations if queue is empty. */
	if (STAILQ_EMPTY(&emptyfifo.head))
		return;

	mtx_lock_spin(&harvest_mtx);

	event = STAILQ_FIRST(&emptyfifo.head);
	if (event != NULL) {
		/* Add the harvested data to the fifo */
		STAILQ_REMOVE_HEAD(&emptyfifo.head, he_next);
		event->he_somecounter = get_cyclecount();
		event->he_size = count;
		event->he_bits = bits;
		event->he_source = origin;
		event->he_destination = harvest_destination[origin]++;
		c = MIN(count, HARVESTSIZE);
		memcpy(event->he_entropy, entropy, c);
		memset(event->he_entropy + c, 0, HARVESTSIZE - c);

		STAILQ_INSERT_TAIL(&harvestfifo.head,
		    event, he_next);
	}

	mtx_unlock_spin(&harvest_mtx);
}
