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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/unistd.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/resource.h>

#include <dev/random/randomdev.h>

static d_open_t		random_open;
static d_close_t	random_close;
static d_read_t		random_read;
static d_write_t	random_write;
static d_ioctl_t	random_ioctl;
static d_poll_t		random_poll;

#define CDEV_MAJOR	2
#define RANDOM_MINOR	3

static struct cdevsw random_cdevsw = {
	/* open */	random_open,
	/* close */	random_close,
	/* read */	random_read,
	/* write */	random_write,
	/* ioctl */	random_ioctl,
	/* poll */	random_poll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"random",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

static void random_kthread(void *);
static void random_harvest_internal(u_int64_t, void *, u_int, u_int, u_int, enum esource);
static void random_write_internal(void *, u_int);

/* Ring buffer holding harvested entropy */
static struct harvestring {
	volatile u_int	head;
	volatile u_int	tail;
	struct harvest	data[HARVEST_RING_SIZE];
} harvestring;

static struct random_systat {
	u_int		seeded;	/* 0 causes blocking 1 allows normal output */
	u_int		burst;	/* number of events to do before sleeping */
	struct selinfo	rsel;	/* For poll(2) */
} random_systat;

/* <0 to end the kthread, 0 to let it run */
static int random_kthread_control = 0;

static struct proc *random_kthread_proc;

/* For use with make_dev(9)/destroy_dev(9). */
static dev_t	random_dev;
static dev_t	urandom_dev;

static int
random_check_boolean(SYSCTL_HANDLER_ARGS)
{
	if (oidp->oid_arg1 != NULL && *(u_int *)(oidp->oid_arg1) != 0)
		*(u_int *)(oidp->oid_arg1) = 1;
        return sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
}

RANDOM_CHECK_UINT(burst, 0, 20);

SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW,
	0, "Random Number Generator");
SYSCTL_NODE(_kern_random, OID_AUTO, sys, CTLFLAG_RW,
	0, "Entropy Device Parameters");
SYSCTL_PROC(_kern_random_sys, OID_AUTO, seeded,
	CTLTYPE_INT|CTLFLAG_RW, &random_systat.seeded, 1,
	random_check_boolean, "I", "Seeded State");
SYSCTL_PROC(_kern_random_sys, OID_AUTO, burst,
	CTLTYPE_INT|CTLFLAG_RW, &random_systat.burst, 20,
	random_check_uint_burst, "I", "Harvest Burst Size");
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

static int
random_open(dev_t dev, int flags, int fmt, struct proc *p)
{
	if ((flags & FWRITE) && (securelevel > 0 || suser(p)))
		return EPERM;
	else
		return 0;
}

static int
random_close(dev_t dev, int flags, int fmt, struct proc *p)
{
	if ((flags & FWRITE) && !(securelevel > 0 || suser(p)))
		random_reseed();
	return 0;
}

static int
random_read(dev_t dev, struct uio *uio, int flag)
{
	u_int	c, ret;
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
	c = min(uio->uio_resid, PAGE_SIZE);
	random_buf = (void *)malloc(c, M_TEMP, M_WAITOK);
	while (uio->uio_resid > 0 && error == 0) {
		ret = read_random_real(random_buf, c);
		error = uiomove(random_buf, ret, uio);
	}
	free(random_buf, M_TEMP);
	return error;
}

static int
random_write(dev_t dev, struct uio *uio, int flag)
{
	u_int	c;
	int	error;
	void	*random_buf;

	error = 0;
	random_buf = (void *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
	while (uio->uio_resid > 0) {
		c = min(uio->uio_resid, PAGE_SIZE);
		error = uiomove(random_buf, c, uio);
		if (error)
			break;
		random_write_internal(random_buf, c);
	}
	free(random_buf, M_TEMP);
	return error;
}

static int
random_ioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
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

static int
random_poll(dev_t dev, int events, struct proc *p)
{
	int	revents;

	revents = 0;
	if (events & (POLLIN | POLLRDNORM)) {
		if (random_systat.seeded)
			revents = events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &random_systat.rsel);
	}
	return revents;
}

static int
random_modevent(module_t mod, int type, void *data)
{
	int	error;

	switch(type) {
	case MOD_LOAD:
		random_init();

		/* This can be turned off by the very paranoid
		 * a reseed will turn it back on.
		 */
		random_systat.seeded = 1;

		/* Number of envents to process off the harvest
		 * queue before giving it a break and sleeping
		 */
		random_systat.burst = 20;

		/* Initialise the harvest ringbuffer */
		harvestring.head = 0;
		harvestring.tail = 0;

		if (bootverbose)
			printf("random: <entropy source>\n");
		random_dev = make_dev(&random_cdevsw, RANDOM_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "random");
		urandom_dev = make_dev_alias(random_dev, "urandom");

		/* Start the hash/reseed thread */
		error = kthread_create(random_kthread, NULL,
			&random_kthread_proc, RFHIGHPID, "random");
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

static void
random_kthread(void *arg /* NOTUSED */)
{
	struct harvest	*event;
	int		newtail, burst;

	/* Drain the harvest queue (in 'burst' size chunks,
	 * if 'burst' > 0. If 'burst' == 0, then completely
	 * drain the queue.
	 */
	for (burst = 0; ; burst++) {

		if ((harvestring.tail == harvestring.head) ||
			(random_systat.burst && burst == random_systat.burst)) {
				tsleep(&harvestring, PUSER, "sleep", hz/10);
				burst = 0;

		}
		else {

			/* Suck a harvested entropy event out of the queue and
			 * hand it to the event processor
			 */

			newtail = (harvestring.tail + 1) & HARVEST_RING_MASK;
			event = &harvestring.data[harvestring.tail];

			/* Bump the ring counter. This action is assumed
			 * to be atomic.
			 */
			harvestring.tail = newtail;

			random_process_event(event);

		}

		/* Is the thread scheduled for a shutdown? */
		if (random_kthread_control != 0) {
#ifdef DEBUG
			mtx_lock(&Giant);
			printf("Random kthread setting terminate\n");
			mtx_unlock(&Giant);
#endif
			random_set_wakeup_exit(&random_kthread_control);
			/* NOTREACHED */
			break;
		}

	}

}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 */
static void
random_harvest_internal(u_int64_t somecounter, void *entropy, u_int count,
	u_int bits, u_int frac, enum esource origin)
{
	struct harvest	*harvest;
	int		newhead;

	newhead = (harvestring.head + 1) & HARVEST_RING_MASK;

	if (newhead != harvestring.tail) {

		/* Add the harvested data to the ring buffer */

		harvest = &harvestring.data[harvestring.head];

		/* Stuff the harvested data into the ring */
		harvest->somecounter = somecounter;
		count = count > HARVESTSIZE ? HARVESTSIZE : count;
		memcpy(harvest->entropy, entropy, count);
		harvest->size = count;
		harvest->bits = bits;
		harvest->frac = frac;
		harvest->source = origin < ENTROPYSOURCE ? origin : 0;

		/* Bump the ring counter. This action is assumed
		 * to be atomic.
		 */
		harvestring.head = newhead;

	}

}

static void
random_write_internal(void *buf, u_int count)
{
	u_int	i;

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

void
random_unblock(void)
{
	if (!random_systat.seeded) {
		random_systat.seeded = 1;
		selwakeup(&random_systat.rsel);
		wakeup(&random_systat);
	}
}
