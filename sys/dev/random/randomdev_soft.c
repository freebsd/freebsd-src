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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/randomdev_soft.h>

#define RANDOM_FIFO_MAX	256	/* How many events to queue up */

static void random_kthread(void *);
static void 
random_harvest_internal(u_int64_t, const void *, u_int,
    u_int, u_int, enum esource);

struct random_systat random_yarrow = {
	.ident = "Software, Yarrow",
	.init = random_yarrow_init,
	.deinit = random_yarrow_deinit,
	.read = random_yarrow_read,
	.write = random_yarrow_write,
	.reseed = random_yarrow_reseed,
	.seeded = 1,
};

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers");

/* Lockable FIFO queue holding entropy buffers */
struct entropyfifo {
	struct mtx lock;
	int count;
	STAILQ_HEAD(harvestlist, harvest) head;
};

/* Empty entropy buffers */
static struct entropyfifo emptyfifo;

#define EMPTYBUFFERS	1024

/* Harvested entropy */
static struct entropyfifo harvestfifo[ENTROPYSOURCE];

/* <0 to end the kthread, 0 to let it run */
static int random_kthread_control = 0;

static struct proc *random_kthread_proc;

/* List for the dynamic sysctls */
struct sysctl_ctx_list random_clist;

/* ARGSUSED */
static int
random_check_boolean(SYSCTL_HANDLER_ARGS)
{
	if (oidp->oid_arg1 != NULL && *(u_int *)(oidp->oid_arg1) != 0)
		*(u_int *)(oidp->oid_arg1) = 1;
	return sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
}

/* ARGSUSED */
void
random_yarrow_init(void)
{
	int error, i;
	struct harvest *np;
	struct sysctl_oid *o, *random_o, *random_sys_o, *random_sys_harvest_o;
	enum esource e;

	o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_STATIC_CHILDREN(_kern),
	    OID_AUTO, "random", CTLFLAG_RW, 0,
	    "Software Random Number Generator");

	random_o = o;

	random_yarrow_init_alg(&random_clist, random_o);

	o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_CHILDREN(random_o),
	    OID_AUTO, "sys", CTLFLAG_RW, 0,
	    "Entropy Device Parameters");

	random_sys_o = o;

	o = SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "seeded", CTLTYPE_INT | CTLFLAG_RW,
	    &random_systat.seeded, 1, random_check_boolean, "I",
	    "Seeded State");

	o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "harvest", CTLFLAG_RW, 0,
	    "Entropy Sources");

	random_sys_harvest_o = o;

	o = SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "ethernet", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.ethernet, 1, random_check_boolean, "I",
	    "Harvest NIC entropy");
	o = SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "point_to_point", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.point_to_point, 1, random_check_boolean, "I",
	    "Harvest serial net entropy");
	o = SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "interrupt", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.interrupt, 1, random_check_boolean, "I",
	    "Harvest IRQ entropy");
	o = SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "swi", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.swi, 0, random_check_boolean, "I",
	    "Harvest SWI entropy");

	/* Initialise the harvest fifos */
	STAILQ_INIT(&emptyfifo.head);
	emptyfifo.count = 0;
	mtx_init(&emptyfifo.lock, "entropy harvest buffers", NULL, MTX_SPIN);
	for (i = 0; i < EMPTYBUFFERS; i++) {
		np = malloc(sizeof(struct harvest), M_ENTROPY, M_WAITOK);
		STAILQ_INSERT_TAIL(&emptyfifo.head, np, next);
	}
	for (e = RANDOM_START; e < ENTROPYSOURCE; e++) {
		STAILQ_INIT(&harvestfifo[e].head);
		harvestfifo[e].count = 0;
		mtx_init(&harvestfifo[e].lock, "entropy harvest", NULL,
		    MTX_SPIN);
	}

	/* Start the hash/reseed thread */
	error = kthread_create(random_kthread, NULL,
	    &random_kthread_proc, RFHIGHPID, 0, "yarrow");
	if (error != 0)
		panic("Cannot create entropy maintenance thread.");

	/* Register the randomness harvesting routine */
	random_yarrow_init_harvester(random_harvest_internal,
	    random_yarrow_read);
}

/* ARGSUSED */
void
random_yarrow_deinit(void)
{
	struct harvest *np;
	enum esource e;

	/* Deregister the randomness harvesting routine */
	random_yarrow_deinit_harvester();

	/*
	 * Command the hash/reseed thread to end and wait for it to finish
	 */
	random_kthread_control = -1;
	tsleep((void *)&random_kthread_control, curthread->td_priority, "term",
	    0);

	/* Destroy the harvest fifos */
	while (!STAILQ_EMPTY(&emptyfifo.head)) {
		np = STAILQ_FIRST(&emptyfifo.head);
		STAILQ_REMOVE_HEAD(&emptyfifo.head, next);
		free(np, M_ENTROPY);
	}
	mtx_destroy(&emptyfifo.lock);
	for (e = RANDOM_START; e < ENTROPYSOURCE; e++) {
		while (!STAILQ_EMPTY(&harvestfifo[e].head)) {
			np = STAILQ_FIRST(&harvestfifo[e].head);
			STAILQ_REMOVE_HEAD(&harvestfifo[e].head, next);
			free(np, M_ENTROPY);
		}
		mtx_destroy(&harvestfifo[e].lock);
	}

	random_yarrow_deinit_alg();

	sysctl_ctx_free(&random_clist);
}

/* ARGSUSED */
static void
random_kthread(void *arg __unused)
{
	struct harvest *event = NULL;
	int found, active;
	enum esource source;

	/* Process until told to stop */
	for (; random_kthread_control == 0;) {

		active = 0;

		/* Cycle through all the entropy sources */
		for (source = RANDOM_START; source < ENTROPYSOURCE; source++) {

			found = 0;

			/* Lock up queue draining */
			mtx_lock_spin(&harvestfifo[source].lock);

			if (!STAILQ_EMPTY(&harvestfifo[source].head)) {

				/* Get a harvested entropy event */
				harvestfifo[source].count--;
				event = STAILQ_FIRST(&harvestfifo[source].head);
				STAILQ_REMOVE_HEAD(&harvestfifo[source].head,
				    next);

				active = found = 1;

			}
			/* Unlock the queue */
			mtx_unlock_spin(&harvestfifo[source].lock);

			/* Deal with the event and dispose of it */
			if (found) {

				random_process_event(event);

				/* Lock the empty event buffer fifo */
				mtx_lock_spin(&emptyfifo.lock);

				STAILQ_INSERT_TAIL(&emptyfifo.head, event,
				    next);

				mtx_unlock_spin(&emptyfifo.lock);

			}
		}

		/* Found nothing, so don't belabour the issue */
		if (!active)
			tsleep(&harvestfifo, curthread->td_priority, "-",
			    hz / 10);

	}

	random_set_wakeup_exit(&random_kthread_control);
	/* NOTREACHED */
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 */
static void
random_harvest_internal(u_int64_t somecounter, const void *entropy,
    u_int count, u_int bits, u_int frac, enum esource origin)
{
	struct harvest *event;

	/* Lockless read to avoid lock operations if fifo is full. */
	if (harvestfifo[origin].count >= RANDOM_FIFO_MAX)
		return;

	/* Lock the particular fifo */
	mtx_lock_spin(&harvestfifo[origin].lock);

	/*
	 * Don't make the harvest queues too big - help to prevent low-grade
	 * entropy swamping
	 */
	if (harvestfifo[origin].count < RANDOM_FIFO_MAX) {

		/* Lock the empty event buffer fifo */
		mtx_lock_spin(&emptyfifo.lock);

		if (!STAILQ_EMPTY(&emptyfifo.head)) {
			event = STAILQ_FIRST(&emptyfifo.head);
			STAILQ_REMOVE_HEAD(&emptyfifo.head, next);
		} else
			event = NULL;

		mtx_unlock_spin(&emptyfifo.lock);

		/* If we didn't obtain a buffer, tough */
		if (event) {

			/* Add the harvested data to the fifo */
			harvestfifo[origin].count++;
			event->somecounter = somecounter;
			event->size = count;
			event->bits = bits;
			event->frac = frac;
			event->source = origin;

			/* XXXX Come back and make this dynamic! */
			count = MIN(count, HARVESTSIZE);
			memcpy(event->entropy, entropy, count);

			STAILQ_INSERT_TAIL(&harvestfifo[origin].head,
			    event, next);
		}
	}
	mtx_unlock_spin(&harvestfifo[origin].lock);

}

void
random_yarrow_write(void *buf, int count)
{
	int i;
	u_int chunk;

	/*
	 * Break the input up into HARVESTSIZE chunks. The writer has too
	 * much control here, so "estimate" the the entropy as zero.
	 */
	for (i = 0; i < count; i += HARVESTSIZE) {
		chunk = HARVESTSIZE;
		if (i + chunk >= count)
			chunk = (u_int)(count - i);
		random_harvest_internal(get_cyclecount(), (char *)buf + i,
		    chunk, 0, 0, RANDOM_WRITE);
	}
}

void
random_yarrow_unblock(void)
{
	if (!random_systat.seeded) {
		random_systat.seeded = 1;
		selwakeuppri(&random_systat.rsel, PUSER);
		wakeup(&random_systat);
	}
}
