/*-
 * Copyright (c) 2000-2004 Mark R V Murray
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

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/randomdev_soft.h>

#define RANDOM_FIFO_MAX	256	/* How many events to queue up */

static void random_kthread(void *);
static void 
random_harvest_internal(u_int64_t, const void *, u_int,
    u_int, u_int, enum esource);
static int random_yarrow_poll(int event,struct thread *td);
static int random_yarrow_block(int flag);
static void random_yarrow_flush_reseed(void);

struct random_systat random_yarrow = {
	.ident = "Software, Yarrow",
	.init = random_yarrow_init,
	.deinit = random_yarrow_deinit,
	.block = random_yarrow_block,
	.read = random_yarrow_read,
	.write = random_yarrow_write,
	.poll = random_yarrow_poll,
	.reseed = random_yarrow_flush_reseed,
	.seeded = 1,
};

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
	struct sysctl_oid *random_o, *random_sys_o, *random_sys_harvest_o;
	enum esource e;

	random_o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_STATIC_CHILDREN(_kern),
	    OID_AUTO, "random", CTLFLAG_RW, 0,
	    "Software Random Number Generator");

	random_yarrow_init_alg(&random_clist, random_o);

	random_sys_o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_CHILDREN(random_o),
	    OID_AUTO, "sys", CTLFLAG_RW, 0,
	    "Entropy Device Parameters");

	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "seeded", CTLTYPE_INT | CTLFLAG_RW,
	    &random_systat.seeded, 1, random_check_boolean, "I",
	    "Seeded State");

	random_sys_harvest_o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "harvest", CTLFLAG_RW, 0,
	    "Entropy Sources");

	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "ethernet", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.ethernet, 1, random_check_boolean, "I",
	    "Harvest NIC entropy");
	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "point_to_point", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.point_to_point, 1, random_check_boolean, "I",
	    "Harvest serial net entropy");
	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "interrupt", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.interrupt, 1, random_check_boolean, "I",
	    "Harvest IRQ entropy");
	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_harvest_o),
	    OID_AUTO, "swi", CTLTYPE_INT | CTLFLAG_RW,
	    &harvest.swi, 0, random_check_boolean, "I",
	    "Harvest SWI entropy");

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
	error = kproc_create(random_kthread, NULL,
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
	tsleep((void *)&random_kthread_control, 0, "term", 0);

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

	random_yarrow_deinit_alg();

	mtx_destroy(&harvest_mtx);

	sysctl_ctx_free(&random_clist);
}

/* ARGSUSED */
static void
random_kthread(void *arg __unused)
{
	STAILQ_HEAD(, harvest) local_queue;
	struct harvest *event = NULL;
	int active, local_count;
	enum esource source;

	STAILQ_INIT(&local_queue);
	local_count = 0;

	/* Process until told to stop */
	for (; random_kthread_control >= 0;) {

		active = 0;

		/* Cycle through all the entropy sources */
		mtx_lock_spin(&harvest_mtx);
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
				random_process_event(event);
			mtx_lock_spin(&harvest_mtx);
			STAILQ_CONCAT(&emptyfifo.head, &local_queue);
			emptyfifo.count += local_count;
			local_count = 0;
		}
		mtx_unlock_spin(&harvest_mtx);

		KASSERT(local_count == 0, ("random_kthread: local_count %d",
		    local_count));

		/*
		 * If a queue flush was commanded, it has now happened,
		 * and we can mark this by resetting the command.
		 */
		if (random_kthread_control == 1)
			random_kthread_control = 0;

		/* Found nothing, so don't belabour the issue */
		if (!active)
			pause("-", hz / 10);

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

	KASSERT(origin == RANDOM_START || origin == RANDOM_WRITE ||
            origin == RANDOM_KEYBOARD || origin == RANDOM_MOUSE ||
            origin == RANDOM_NET || origin == RANDOM_INTERRUPT ||
            origin == RANDOM_PURE,
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

			STAILQ_INSERT_TAIL(&harvestfifo[origin].head,
			    event, next);
		}
	}
	mtx_unlock_spin(&harvest_mtx);
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

static int
random_yarrow_poll(int events, struct thread *td)
{
	int revents = 0;
	mtx_lock(&random_reseed_mtx);

	if (random_systat.seeded)
		revents = events & (POLLIN | POLLRDNORM);
	else
		selrecord(td, &random_systat.rsel);
	
	mtx_unlock(&random_reseed_mtx);
	return revents;
}

static int
random_yarrow_block(int flag)
{
	int error = 0;

	mtx_lock(&random_reseed_mtx);

	/* Blocking logic */
	while (random_systat.seeded && !error) {
		if (flag & O_NONBLOCK)
			error = EWOULDBLOCK;
		else {
			printf("Entropy device is blocking.\n");
			error = msleep(&random_systat,
			    &random_reseed_mtx,
			    PUSER | PCATCH, "block", 0);
		}
	}
	mtx_unlock(&random_reseed_mtx);

	return error;
}	

/* Helper routine to perform explicit reseeds */
static void
random_yarrow_flush_reseed(void)
{
	/* Command a entropy queue flush and wait for it to finish */
	random_kthread_control = 1;
	while (random_kthread_control)
		pause("-", hz / 10);

	random_yarrow_reseed();
}
