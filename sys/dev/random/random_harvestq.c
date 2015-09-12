/*-
 * Copyright (c) 2000-2015 Mark R V Murray
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/hash.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>

static void random_kthread(void);

/* List for the dynamic sysctls */
static struct sysctl_ctx_list random_clist;

/*
 * How many events to queue up. We create this many items in
 * an 'empty' queue, then transfer them to the 'harvest' queue with
 * supplied junk. When used, they are transferred back to the
 * 'empty' queue.
 */
#define	RANDOM_RING_MAX		1024
#define	RANDOM_ACCUM_MAX	8

/* 1 to let the kernel thread run, 0 to terminate */
volatile int random_kthread_control;

/*
 * Put all the harvest queue context stuff in one place.
 * this make is a bit easier to lock and protect.
 */
static struct harvest_context {
	/* The harvest mutex protects all of harvest_context and
	 * the related data.
	 */
	struct mtx hc_mtx;
	/* Round-robin destination cache. */
	u_int hc_destination[ENTROPYSOURCE];
	/* The context of the kernel thread processing harvested entropy */
	struct proc *hc_kthread_proc;
	/* Allow the sysadmin to select the broad category of
	 * entropy types to harvest.
	 */
	u_int hc_source_mask;
	/*
	 * Lockless ring buffer holding entropy events
	 * If ring.in == ring.out,
	 *     the buffer is empty.
	 * If ring.in != ring.out,
	 *     the buffer contains harvested entropy.
	 * If (ring.in + 1) == ring.out (mod RANDOM_RING_MAX),
	 *     the buffer is full.
	 *
	 * NOTE: ring.in points to the last added element,
	 * and ring.out points to the last consumed element.
	 *
	 * The ring.in variable needs locking as there are multiple
	 * sources to the ring. Only the sources may change ring.in,
	 * but the consumer may examine it.
	 *
	 * The ring.out variable does not need locking as there is
	 * only one consumer. Only the consumer may change ring.out,
	 * but the sources may examine it.
	 */
	struct entropy_ring {
		struct harvest_event ring[RANDOM_RING_MAX];
		volatile u_int in;
		volatile u_int out;
	} hc_entropy_ring;
	struct fast_entropy_accumulator {
		volatile u_int pos;
		uint32_t buf[RANDOM_ACCUM_MAX];
	} hc_entropy_fast_accumulator;
} harvest_context;

static struct kproc_desc random_proc_kp = {
	"rand_harvestq",
	random_kthread,
	&harvest_context.hc_kthread_proc,
};


/* Pass the given event straight through to Fortuna/Yarrow/Whatever. */
static __inline void
random_harvestq_fast_process_event(struct harvest_event *event)
{
	if (random_alg_context.ra_event_processor)
		random_alg_context.ra_event_processor(event);
}

static void
random_kthread(void)
{
        u_int maxloop, ring_out, i;

	/*
	 * Locking is not needed as this is the only place we modify ring.out, and
	 * we only examine ring.in without changing it. Both of these are volatile,
	 * and this is a unique thread.
	 */
	for (random_kthread_control = 1; random_kthread_control;) {
		/* Deal with events, if any. Restrict the number we do in one go. */
		maxloop = RANDOM_RING_MAX;
		while (harvest_context.hc_entropy_ring.out != harvest_context.hc_entropy_ring.in) {
			ring_out = (harvest_context.hc_entropy_ring.out + 1)%RANDOM_RING_MAX;
			random_harvestq_fast_process_event(harvest_context.hc_entropy_ring.ring + ring_out);
			harvest_context.hc_entropy_ring.out = ring_out;
			if (!--maxloop)
				break;
		}
		random_sources_feed();
		/* XXX: FIX!! Increase the high-performance data rate? Need some measurements first. */
		for (i = 0; i < RANDOM_ACCUM_MAX; i++) {
			if (harvest_context.hc_entropy_fast_accumulator.buf[i]) {
				random_harvest_direct(harvest_context.hc_entropy_fast_accumulator.buf + i, sizeof(harvest_context.hc_entropy_fast_accumulator.buf[0]), 4, RANDOM_FAST);
				harvest_context.hc_entropy_fast_accumulator.buf[i] = 0;
			}
		}
		/* XXX: FIX!! This is a *great* place to pass hardware/live entropy to random(9) */
		tsleep_sbt(&harvest_context.hc_kthread_proc, 0, "-", SBT_1S/10, 0, C_PREL(1));
	}
	wakeup(&harvest_context.hc_kthread_proc);
	kproc_exit(0);
	/* NOTREACHED */
}
SYSINIT(random_device_h_proc, SI_SUB_CREATE_INIT, SI_ORDER_ANY, kproc_start, &random_proc_kp);

/* ARGSUSED */
RANDOM_CHECK_UINT(harvestmask, 0, RANDOM_HARVEST_EVERYTHING_MASK);

/* ARGSUSED */
static int
random_print_harvestmask(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	int error, i;

	error = sysctl_wire_old_buffer(req, 0);
	if (error == 0) {
		sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
		for (i = RANDOM_ENVIRONMENTAL_END; i >= 0; i--)
			sbuf_cat(&sbuf, (harvest_context.hc_source_mask & (1 << i)) ? "1" : "0");
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
	"FS_ATIME",
	"HIGH_PERFORMANCE", /* ENVIRONMENTAL_END */
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
		for (i = RANDOM_ENVIRONMENTAL_END; i >= 0; i--) {
			sbuf_cat(&sbuf, (i == RANDOM_ENVIRONMENTAL_END) ? "" : ",");
			sbuf_cat(&sbuf, !(harvest_context.hc_source_mask & (1 << i)) ? "[" : "");
			sbuf_cat(&sbuf, random_source_descr[i]);
			sbuf_cat(&sbuf, !(harvest_context.hc_source_mask & (1 << i)) ? "]" : "");
		}
		error = sbuf_finish(&sbuf);
		sbuf_delete(&sbuf);
	}
	return (error);
}

/* ARGSUSED */
static void
random_harvestq_init(void *unused __unused)
{
	struct sysctl_oid *random_sys_o;

	random_sys_o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_STATIC_CHILDREN(_kern_random),
	    OID_AUTO, "harvest", CTLFLAG_RW, 0,
	    "Entropy Device Parameters");
	harvest_context.hc_source_mask = RANDOM_HARVEST_EVERYTHING_MASK;
	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "mask", CTLTYPE_UINT | CTLFLAG_RW,
	    &harvest_context.hc_source_mask, 0,
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
	RANDOM_HARVEST_INIT_LOCK();
	harvest_context.hc_entropy_ring.in = harvest_context.hc_entropy_ring.out = 0;
}
SYSINIT(random_device_h_init, SI_SUB_RANDOM, SI_ORDER_SECOND, random_harvestq_init, NULL);

/*
 * This is used to prime the RNG by grabbing any early random stuff
 * known to the kernel, and inserting it directly into the hashing
 * module, e.g. Fortuna or Yarrow.
 */
/* ARGSUSED */
static void
random_harvestq_prime(void *unused __unused)
{
	struct harvest_event event;
	size_t count, size, i;
	uint8_t *keyfile, *data;

	/*
	 * Get entropy that may have been preloaded by loader(8)
	 * and use it to pre-charge the entropy harvest queue.
	 */
	keyfile = preload_search_by_type(RANDOM_HARVESTQ_BOOT_ENTROPY_FILE);
	if (keyfile != NULL) {
		data = preload_fetch_addr(keyfile);
		size = preload_fetch_size(keyfile);
		/* Trim the size. If the admin has a file with a funny size, we lose some. Tough. */
		size -= (size % sizeof(event.he_entropy));
		if (data != NULL && size != 0) {
			for (i = 0; i < size; i += sizeof(event.he_entropy)) {
				count = sizeof(event.he_entropy);
				event.he_somecounter = (uint32_t)get_cyclecount();
				event.he_size = count;
				event.he_bits = count/4; /* Underestimate the size for Yarrow */
				event.he_source = RANDOM_CACHED;
				event.he_destination = harvest_context.hc_destination[0]++;
				memcpy(event.he_entropy, data + i, sizeof(event.he_entropy));
				random_harvestq_fast_process_event(&event);
				explicit_bzero(&event, sizeof(event));
			}
			explicit_bzero(data, size);
			if (bootverbose)
				printf("random: read %zu bytes from preloaded cache\n", size);
		} else
			if (bootverbose)
				printf("random: no preloaded entropy cache\n");
	}
}
SYSINIT(random_device_prime, SI_SUB_RANDOM, SI_ORDER_FOURTH, random_harvestq_prime, NULL);

/* ARGSUSED */
static void
random_harvestq_deinit(void *unused __unused)
{

	/* Command the hash/reseed thread to end and wait for it to finish */
	random_kthread_control = 0;
	tsleep(&harvest_context.hc_kthread_proc, 0, "harvqterm", 0);
	sysctl_ctx_free(&random_clist);
}
SYSUNINIT(random_device_h_init, SI_SUB_RANDOM, SI_ORDER_SECOND, random_harvestq_deinit, NULL);

/*-
 * Entropy harvesting queue routine.
 *
 * This is supposed to be fast; do not do anything slow in here!
 * It is also illegal (and morally reprehensible) to insert any
 * high-rate data here. "High-rate" is defined as a data source
 * that will usually cause lots of failures of the "Lockless read"
 * check a few lines below. This includes the "always-on" sources
 * like the Intel "rdrand" or the VIA Nehamiah "xstore" sources.
 */
/* XXXRW: get_cyclecount() is cheap on most modern hardware, where cycle
 * counters are built in, but on older hardware it will do a real time clock
 * read which can be quite expensive.
 */
void
random_harvest_queue(const void *entropy, u_int size, u_int bits, enum random_entropy_source origin)
{
	struct harvest_event *event;
	u_int ring_in;

	KASSERT(origin >= RANDOM_START && origin < ENTROPYSOURCE, ("%s: origin %d invalid\n", __func__, origin));
	if (!(harvest_context.hc_source_mask & (1 << origin)))
		return;
	RANDOM_HARVEST_LOCK();
	ring_in = (harvest_context.hc_entropy_ring.in + 1)%RANDOM_RING_MAX;
	if (ring_in != harvest_context.hc_entropy_ring.out) {
		/* The ring is not full */
		event = harvest_context.hc_entropy_ring.ring + ring_in;
		event->he_somecounter = (uint32_t)get_cyclecount();
		event->he_source = origin;
		event->he_destination = harvest_context.hc_destination[origin]++;
		event->he_bits = bits;
		if (size <= sizeof(event->he_entropy)) {
			event->he_size = size;
			memcpy(event->he_entropy, entropy, size);
		}
		else {
			/* Big event, so squash it */
			event->he_size = sizeof(event->he_entropy[0]);
			event->he_entropy[0] = jenkins_hash(entropy, size, (uint32_t)(uintptr_t)event);
		}
		harvest_context.hc_entropy_ring.in = ring_in;
	}
	RANDOM_HARVEST_UNLOCK();
}

/*-
 * Entropy harvesting fast routine.
 *
 * This is supposed to be very fast; do not do anything slow in here!
 * This is the right place for high-rate harvested data.
 */
void
random_harvest_fast(const void *entropy, u_int size, u_int bits, enum random_entropy_source origin)
{
	u_int pos;

	KASSERT(origin >= RANDOM_START && origin < ENTROPYSOURCE, ("%s: origin %d invalid\n", __func__, origin));
	/* XXX: FIX!! The above KASSERT is BS. Right now we ignore most structure and just accumulate the supplied data */
	if (!(harvest_context.hc_source_mask & (1 << origin)))
		return;
	pos = harvest_context.hc_entropy_fast_accumulator.pos;
	harvest_context.hc_entropy_fast_accumulator.buf[pos] ^= jenkins_hash(entropy, size, (uint32_t)get_cyclecount());
	harvest_context.hc_entropy_fast_accumulator.pos = (pos + 1)%RANDOM_ACCUM_MAX;
}

/*-
 * Entropy harvesting direct routine.
 *
 * This is not supposed to be fast, but will only be used during
 * (e.g.) booting when initial entropy is being gathered.
 */
void
random_harvest_direct(const void *entropy, u_int size, u_int bits, enum random_entropy_source origin)
{
	struct harvest_event event;

	KASSERT(origin >= RANDOM_START && origin < ENTROPYSOURCE, ("%s: origin %d invalid\n", __func__, origin));
	if (!(harvest_context.hc_source_mask & (1 << origin)))
		return;
	size = MIN(size, sizeof(event.he_entropy));
	event.he_somecounter = (uint32_t)get_cyclecount();
	event.he_size = size;
	event.he_bits = bits;
	event.he_source = origin;
	event.he_destination = harvest_context.hc_destination[origin]++;
	memcpy(event.he_entropy, entropy, size);
	random_harvestq_fast_process_event(&event);
	explicit_bzero(&event, sizeof(event));
}
