/*-
 * Copyright (c) 2000-2014 Mark R V Murray
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

/*
 * Lockable FIFO ring buffer holding entropy events
 * If ring_in == ring_out,
 *     the buffer is empty.
 * If (ring_in + 1) == ring_out (MOD RANDOM_FIFO_MAX),
 *     the buffer is full.
 *
 * The ring_in variable needs locking as there are multiple
 * sources to the ring. Only the sources may change ring_in,
 * but the consumer may examine it.
 *
 * The ring_out variable does not need locking as there is
 * only one consumer. Only the consumer may change ring_out,
 * but the sources may examine it.
 */
static struct entropyfifo {
	struct harvest_event ring[RANDOM_FIFO_MAX];
	volatile u_int ring_in;
	volatile u_int ring_out;
} entropyfifo;

/* Round-robin destination cache. */
u_int harvest_destination[ENTROPYSOURCE];

/* Function called to process one harvested stochastic event */
void (*harvest_process_event)(struct harvest_event *);

/* Allow the sysadmin to select the broad category of
 * entropy types to harvest.
 */
static u_int harvest_source_mask = ((1U << RANDOM_ENVIRONMENTAL_END) - 1);

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
        u_int maxloop, ring_out;

	/*
	 * Process until told to stop.
	 *
	 * Locking is not needed as this is the only place we modify ring_out, and
	 * we only examine ring_in without changing it. Both of these are volatile,
	 * and this is a unique thread.
	 */
	while (random_kthread_control >= 0) {

		/* Deal with events, if any. Restrict the number we do in one go. */
		maxloop = RANDOM_FIFO_MAX;
		while (entropyfifo.ring_out != entropyfifo.ring_in) {

			ring_out = (entropyfifo.ring_out + 1)%RANDOM_FIFO_MAX;
			harvest_process_event(entropyfifo.ring + ring_out);
			/* Modifying ring_out here ONLY. Sufficient for atomicity? */
			entropyfifo.ring_out = ring_out;

			/* The ring may be filled quickly so don't loop forever.  */
			if (--maxloop)
				break;

		}

		/*
		 * Give the fast hardware sources a go
		 */
		live_entropy_sources_feed();

		/*
		 * If a queue flush was commanded, it has now happened,
		 * and we can mark this by resetting the command.
		 * A negative value, however, terminates the thread.
		 */

		if (random_kthread_control == 1)
			random_kthread_control = 0;

		/* Some work is done, so give the rest of the OS a chance. */
		tsleep_sbt(&random_kthread_control, 0, "-", SBT_1S/10, 0, C_PREL(1));

	}

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
}

/* ARGSUSED */
RANDOM_CHECK_UINT(harvestmask, 0, ((1U << RANDOM_ENVIRONMENTAL_END) - 1));

/* ARGSUSED */
static int
random_print_harvestmask(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	int error, i;

	error = sysctl_wire_old_buffer(req, 0);
	if (error == 0) {
		sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
		for (i = RANDOM_ENVIRONMENTAL_END - 1; i >= 0; i--)
			sbuf_cat(&sbuf, (harvest_source_mask & (1U << i)) ? "1" : "0");
		error = sbuf_finish(&sbuf);
		sbuf_delete(&sbuf);
	}

	return (error);
}

static const char *(random_source_descr[]) = {
	"CACHED",
	"ATTACH",
	"KEYBOARD",
	"MOUSE",
	"NET_TUN",
	"NET_ETHER",
	"NET_NG",
	"INTERRUPT",
	"SWI",
	"UMA_ALLOC",
	"", /* "ENVIRONMENTAL_END" */
	"PURE_OCTEON",
	"PURE_SAFE",
	"PURE_GLXSB",
	"PURE_UBSEC",
	"PURE_HIFN",
	"PURE_RDRAND",
	"PURE_NEHEMIAH",
	"PURE_RNDTEST",
	/* "ENTROPYSOURCE" */
};

/* ARGSUSED */
static int
random_print_harvestmask_symbolic(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	int error, i;

	error = sysctl_wire_old_buffer(req, 0);
	if (error == 0) {
		sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
		for (i = RANDOM_ENVIRONMENTAL_END - 1; i >= 0; i--) {
			sbuf_cat(&sbuf, (i == RANDOM_ENVIRONMENTAL_END - 1) ? "" : ",");
			sbuf_cat(&sbuf, (harvest_source_mask & (1U << i)) ? random_source_descr[i] : "");
		}
		error = sbuf_finish(&sbuf);
		sbuf_delete(&sbuf);
	}

	return (error);
}

void
random_harvestq_init(void (*event_processor)(struct harvest_event *), int poolcount)
{
	uint8_t *keyfile, *data;
	int error;
	size_t size, j;
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
	    &harvest_source_mask, ((1U << RANDOM_ENVIRONMENTAL_END) - 1),
	    random_check_uint_harvestmask, "IU",
	    "Entropy harvesting mask");

	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "mask_bin", CTLTYPE_STRING | CTLFLAG_RD,
	    NULL, 0, random_print_harvestmask, "A", "Entropy harvesting mask (printable)");

	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "mask_symbolic", CTLTYPE_STRING | CTLFLAG_RD,
	    NULL, 0, random_print_harvestmask_symbolic, "A", "Entropy harvesting mask (symbolic)");

	/* Point to the correct event_processing function */
	harvest_process_event = event_processor;

	/* Store the pool count (used by live source feed) */
	harvest_pool_count = poolcount;

	/* Initialise the harvesting mutex and in/out indexes. */
	mtx_init(&harvest_mtx, "entropy harvest mutex", NULL, MTX_SPIN);
	entropyfifo.ring_in = entropyfifo.ring_out = 0U;

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

#ifdef RANDOM_DEBUG
	printf("random: %s\n", __func__);
#endif

	/*
	 * Command the hash/reseed thread to end and wait for it to finish
	 */
	random_kthread_control = -1;
	tsleep(&random_kthread_control, 0, "term", 0);

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
	u_int ring_in;

	KASSERT(origin >= RANDOM_START && origin < ENTROPYSOURCE,
	    ("random_harvest_internal: origin %d invalid\n", origin));

	/* Mask out unwanted sources */
	if (!(harvest_source_mask & (1U << origin)))
		return;

	/* Lock ring_in against multi-thread contention */
	mtx_lock_spin(&harvest_mtx);
	ring_in = (entropyfifo.ring_in + 1)%RANDOM_FIFO_MAX;
	if (ring_in != entropyfifo.ring_out) {
		/* The ring is not full */
		event = entropyfifo.ring + ring_in;

		/* Stash the harvested stuff in the *event buffer */
		count = MIN(count, HARVESTSIZE);
		event->he_somecounter = get_cyclecount();
		event->he_size = count;
		event->he_bits = bits;
		event->he_source = origin;
		event->he_destination = harvest_destination[origin]++;
		memcpy(event->he_entropy, entropy, count);
		memset(event->he_entropy + count, 0, HARVESTSIZE - count);

		entropyfifo.ring_in = ring_in;
	}
	mtx_unlock_spin(&harvest_mtx);
}
