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
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/random.h>
#include <sys/vnode.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/sysctl.h>
#include <crypto/blowfish/blowfish.h>

#include <dev/random/hash.h>
#include <dev/random/yarrow.h>

#include "opt_noblockrandom.h"

static d_open_t random_open;
static d_read_t random_read;
static d_write_t random_write;
static d_ioctl_t random_ioctl;
static d_poll_t random_poll;

#define CDEV_MAJOR	2
#define RANDOM_MINOR	3
#define URANDOM_MINOR	4

static struct cdevsw random_cdevsw = {
	/* open */	random_open,
	/* close */	(d_close_t *)nullop,
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

/* For use with make_dev(9)/destroy_dev(9). */
static dev_t random_dev;
static dev_t urandom_dev; /* XXX Temporary */

SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW, 0, "Random Number Generator");
SYSCTL_NODE(_kern_random, OID_AUTO, yarrow, CTLFLAG_RW, 0, "Yarrow Parameters");
SYSCTL_INT(_kern_random_yarrow, OID_AUTO, gengateinterval, CTLFLAG_RW,
	&random_state.gengateinterval, 10, "Generator Gate Interval");
SYSCTL_INT(_kern_random_yarrow, OID_AUTO, bins, CTLFLAG_RW,
	&random_state.bins, 10, "Execution time tuner");
SYSCTL_INT(_kern_random_yarrow, OID_AUTO, fastthresh, CTLFLAG_RW,
	&random_state.pool[0].thresh, 100, "Fast pool reseed threshhold");
SYSCTL_INT(_kern_random_yarrow, OID_AUTO, slowthresh, CTLFLAG_RW,
	&random_state.pool[1].thresh, 160, "Slow pool reseed threshhold");
SYSCTL_INT(_kern_random_yarrow, OID_AUTO, slowoverthresh, CTLFLAG_RW,
	&random_state.slowoverthresh, 2, "Slow pool over-threshhold reseed");

static int
random_open(dev_t dev, int flags, int fmt, struct proc *p)
{
	if ((flags & FWRITE) && (securelevel > 0 || suser(p)))
		return EPERM;
	else
		return 0;
}

static int
random_read(dev_t dev, struct uio *uio, int flag)
{
	u_int c, ret;
	int error = 0;
	void *random_buf;

/* XXX Temporary ifndef to allow users to have a nonblocking device */
#ifndef NOBLOCKRANDOM
	while (!random_state.seeded) {
		if (flag & IO_NDELAY)
			error =  EWOULDBLOCK;
		else
			error = tsleep(&random_state, PUSER|PCATCH, "rndblk", 0);
		if (error != 0)
			return error;
	}
#endif
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
	u_int c;
	int error = 0;
	void *random_buf;

	random_buf = (void *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
	while (uio->uio_resid > 0) {
		c = min(uio->uio_resid, PAGE_SIZE);
		error = uiomove(random_buf, c, uio);
		if (error)
			break;
		write_random(random_buf, c);
	}
	free(random_buf, M_TEMP);
	return error;
}

static int
random_ioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	return ENOTTY;
}

static int
random_poll(dev_t dev, int events, struct proc *p)
{
	int revents;

	revents = 0;
	if (events & (POLLIN | POLLRDNORM)) {
		if (random_state.seeded)
			revents = events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &random_state.rsel);
	}
	return revents;
}

static int
random_modevent(module_t mod, int type, void *data)
{
	int error;

	switch(type) {
	case MOD_LOAD:
		error = random_init();
		if (error != 0)
			return error;
		if (bootverbose)
			printf("random: <entropy source>\n");
		random_dev = make_dev(&random_cdevsw, RANDOM_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "random");
		urandom_dev = make_dev(&random_cdevsw, URANDOM_MINOR, UID_ROOT,
			GID_WHEEL, 0666, "urandom"); /* XXX Temporary */
		return 0;

	case MOD_UNLOAD:
		random_deinit();
		destroy_dev(random_dev);
		destroy_dev(urandom_dev); /* XXX Temporary */
		return 0;

	case MOD_SHUTDOWN:
		return 0;

	default:
		return EOPNOTSUPP;
	}
}

DEV_MODULE(random, random_modevent, NULL);
