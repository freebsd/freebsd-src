/*-
 * Copyright (c) 2000, 2001, 2002, 2003 Mark R V Murray
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
#include <sys/filio.h>
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

static d_close_t	random_close;
static d_read_t		random_read;
static d_write_t	random_write;
static d_ioctl_t	random_ioctl;
static d_poll_t		random_poll;

#define RANDOM_MINOR	0

#define RANDOM_FIFO_MAX	256	/* How many events to queue up */

static struct cdevsw random_cdevsw = {
	.d_close =	random_close,
	.d_read =	random_read,
	.d_write =	random_write,
	.d_ioctl =	random_ioctl,
	.d_poll =	random_poll,
	.d_name =	"random",
};

static void random_kthread(void *);
static void random_harvest_internal(u_int64_t, void *, u_int, u_int, u_int, enum esource);
static void random_write_internal(void *, int);

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers");

/* FIFO queues holding harvested entropy */
static struct harvestfifo {
	struct mtx lock;
	int count;
	STAILQ_HEAD(harvestlist, harvest) head;
} harvestfifo[ENTROPYSOURCE];

static struct random_systat {
	u_int		seeded;	/* 0 causes blocking 1 allows normal output */
	struct selinfo	rsel;	/* For poll(2) */
} random_systat;

/* <0 to end the kthread, 0 to let it run */
static int random_kthread_control = 0;

static struct proc *random_kthread_proc;

/* For use with make_dev(9)/destroy_dev(9). */
static dev_t	random_dev;
static dev_t	urandom_dev;

/* ARGSUSED */
static int
random_check_boolean(SYSCTL_HANDLER_ARGS)
{
	if (oidp->oid_arg1 != NULL && *(u_int *)(oidp->oid_arg1) != 0)
		*(u_int *)(oidp->oid_arg1) = 1;
        return sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
}

SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW,
	0, "Random Number Generator");
SYSCTL_NODE(_kern_random, OID_AUTO, sys, CTLFLAG_RW,
	0, "Entropy Device Parameters");
SYSCTL_PROC(_kern_random_sys, OID_AUTO, seeded,
	CTLTYPE_INT|CTLFLAG_RW, &random_systat.seeded, 1,
	random_check_boolean, "I", "Seeded State");
SYSCTL_NODE(_kern_random_sys, OID_AUTO, harvest, CTLFLAG_RW,
	0, "Entropy Sources");
SYSCTL_PROC(_kern_random_sys_harvest, OID_AUTO, ethernet,
	CTLTYPE_INT|CTLFLAG_RW, &harvest.ethernet, 0,
	random_check_boolean, "I", "Harvest NIC entropy");
SYSCTL_PROC(_kern_random_sys_harvest, OID_AUTO, point_to_point,
	CTLTYPE_INT|CTLFLAG_RW, &harvest.point_to_point, 0,
	random_check_boolean, "I", "Harvest serial net entropy");
SYSCTL_PROC(_kern_random_sys_harvest, OID_AUTO, interrupt,
	CTLTYPE_INT|CTLFLAG_RW, &harvest.interrupt, 0,
	random_check_boolean, "I", "Harvest IRQ entropy");
SYSCTL_PROC(_kern_random_sys_harvest, OID_AUTO, swi,
	CTLTYPE_INT|CTLFLAG_RW, &harvest.swi, 0,
	random_check_boolean, "I", "Harvest SWI entropy");

/* ARGSUSED */
static int
random_close(dev_t dev __unused, int flags, int fmt __unused, struct thread *td)
{
	if (flags & FWRITE) {
		if (suser(td) == 0 && securelevel_gt(td->td_ucred, 0) == 0)
			random_reseed();
	}
	return 0;
}

/* ARGSUSED */
static int
random_read(dev_t dev __unused, struct uio *uio, int flag)
{
	int	c, ret;
	int	error = 0;
	void	*random_buf;

	while (!random_systat.seeded) {
		if (flag & IO_NDELAY)
			error =  EWOULDBLOCK;
		else
			error = tsleep(&random_systat, PUSER|PCATCH,
				"block", 0);
		if (error != 0)
			return error;
	}
	c = uio->uio_resid < PAGE_SIZE ? uio->uio_resid : PAGE_SIZE;
	random_buf = (void *)malloc((u_long)c, M_TEMP, M_WAITOK);
	while (uio->uio_resid > 0 && error == 0) {
		ret = read_random_real(random_buf, c);
		error = uiomove(random_buf, ret, uio);
	}
	free(random_buf, M_TEMP);
	return error;
}

/* ARGSUSED */
static int
random_write(dev_t dev __unused, struct uio *uio, int flag __unused)
{
	int	c;
	int	error;
	void	*random_buf;

	error = 0;
	random_buf = (void *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
	while (uio->uio_resid > 0) {
		c = (int)(uio->uio_resid < PAGE_SIZE
		    ? uio->uio_resid
		    : PAGE_SIZE);
		error = uiomove(random_buf, c, uio);
		if (error)
			break;
		random_write_internal(random_buf, c);
	}
	free(random_buf, M_TEMP);
	return error;
}

/* ARGSUSED */
static int
random_ioctl(dev_t dev __unused, u_long cmd, caddr_t addr __unused,
    int flags __unused, struct thread *td __unused)
{
	switch (cmd) {
	/* Really handled in upper layer */
	case FIOASYNC:
	case FIONBIO:
		return 0;
	default:
		return ENOTTY;
	}
}

/* ARGSUSED */
static int
random_poll(dev_t dev __unused, int events, struct thread *td)
{
	int	revents;

	revents = 0;
	if (events & (POLLIN | POLLRDNORM)) {
		if (random_systat.seeded)
			revents = events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &random_systat.rsel);
	}
	return revents;
}

/* ARGSUSED */
static int
random_modevent(module_t mod __unused, int type, void *data __unused)
{
	int	error, i;
	struct harvest *np;

	switch(type) {
	case MOD_LOAD:
		random_init();

		/* This can be turned off by the very paranoid
		 * a reseed will turn it back on.
		 */
		random_systat.seeded = 1;

		/* Initialise the harvest fifos */
		for (i = 0; i < ENTROPYSOURCE; i++) {
			STAILQ_INIT(&harvestfifo[i].head);
			harvestfifo[i].count = 0;
			mtx_init(&harvestfifo[i].lock, "entropy harvest", NULL, MTX_DEF);
		}

		if (bootverbose)
			printf("random: <entropy source>\n");
		random_dev = make_dev(&random_cdevsw, RANDOM_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "random");
		urandom_dev = make_dev_alias(random_dev, "urandom");

		/* Start the hash/reseed thread */
		error = kthread_create(random_kthread, NULL,
			&random_kthread_proc, RFHIGHPID, 0, "random");
		if (error != 0)
			return error;

		/* Register the randomness harvesting routine */
		random_init_harvester(random_harvest_internal,
			read_random_real);

		return 0;

	case MOD_UNLOAD:
		/* Deregister the randomness harvesting routine */
		random_deinit_harvester();

		/* Command the hash/reseed thread to end and
		 * wait for it to finish
		 */
		random_kthread_control = -1;
		tsleep((void *)&random_kthread_control, PUSER, "term", 0);

		/* Destroy the harvest fifos */
		for (i = 0; i < ENTROPYSOURCE; i++) {
			while (!STAILQ_EMPTY(&harvestfifo[i].head)) {
				np = STAILQ_FIRST(&harvestfifo[i].head);
				STAILQ_REMOVE_HEAD(&harvestfifo[i].head, next);
				free(np, M_ENTROPY);
			}
			mtx_destroy(&harvestfifo[i].lock);
		}

		random_deinit();

		destroy_dev(random_dev);
		destroy_dev(urandom_dev);
		return 0;

	case MOD_SHUTDOWN:
		return 0;

	default:
		return EOPNOTSUPP;
	}
}

DEV_MODULE(random, random_modevent, NULL);

/* ARGSUSED */
static void
random_kthread(void *arg __unused)
{
	struct harvest	*event = NULL;
	int		found, active;
	enum esource	source;

	/* Process until told to stop */
	for (; random_kthread_control == 0;) {

		active = 0;

		/* Cycle through all the entropy sources */
		for (source = 0; source < ENTROPYSOURCE; source++) {

			found = 0;

			/* Lock up queue draining */
			mtx_lock(&harvestfifo[source].lock);

			if (!STAILQ_EMPTY(&harvestfifo[source].head)) {

				/* Get a harvested entropy event */
				harvestfifo[source].count--;
				event = STAILQ_FIRST(&harvestfifo[source].head);
				STAILQ_REMOVE_HEAD(&harvestfifo[source].head,
					next);
				active = found = 1;

			}

			/* Unlock the queue */
			mtx_unlock(&harvestfifo[source].lock);

			/* Deal with the event and dispose of it */
			if (found) {
				random_process_event(event);
				free(event, M_ENTROPY);
			}

		}

		/* Found nothing, so don't belabour the issue */
		if (!active)
			tsleep(&harvestfifo, PUSER, "-", hz/10);

	}

	random_set_wakeup_exit(&random_kthread_control);
	/* NOTREACHED */
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 */
static void
random_harvest_internal(u_int64_t somecounter, void *entropy, u_int count,
	u_int bits, u_int frac, enum esource origin)
{
	struct harvest	*event;

	/* Lock the particular fifo */
	mtx_lock(&harvestfifo[origin].lock);

	/* Don't make the harvest queues too big - memory is precious */
	if (harvestfifo[origin].count < RANDOM_FIFO_MAX) {
		
		event = malloc(sizeof(struct harvest), M_ENTROPY, M_NOWAIT);

		/* If we can't malloc() a buffer, tough */
		if (event) {

			/* Add the harvested data to the fifo */
			harvestfifo[origin].count++;
			event->somecounter = somecounter;
			event->size = count;
			event->bits = bits;
			event->frac = frac;
			event->source = origin;

			/* XXXX Come back and make this dynamic! */
			count = count > HARVESTSIZE ? HARVESTSIZE : count;
			memcpy(event->entropy, entropy, count);

			STAILQ_INSERT_TAIL(&harvestfifo[origin].head, event, next);
		}

	}

	mtx_unlock(&harvestfifo[origin].lock);

}

static void
random_write_internal(void *buf, int count)
{
	int	i;
	u_int	chunk;

	/* Break the input up into HARVESTSIZE chunks.
	 * The writer has too much control here, so "estimate" the
	 * the entropy as zero.
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
random_unblock(void)
{
	if (!random_systat.seeded) {
		random_systat.seeded = 1;
		selwakeuppri(&random_systat.rsel, PUSER);
		wakeup(&random_systat);
	}
}
