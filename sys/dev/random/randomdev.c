/*-
 * Copyright (c) 2000-2013 Mark R V Murray
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
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

/*
 * NOTE NOTE NOTE
 *
 * This file is compiled into the kernel unconditionally. Any random(4)
 * infrastructure that needs to be in the kernel by default goes here!
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
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <machine/bus.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/random_harvestq.h>

#define RANDOM_MINOR	0

static d_read_t randomdev_read;
static d_write_t randomdev_write;
static d_ioctl_t randomdev_ioctl;
static d_poll_t randomdev_poll;

static struct cdevsw random_cdevsw = {
	.d_version = D_VERSION,
	.d_read = randomdev_read,
	.d_write = randomdev_write,
	.d_ioctl = randomdev_ioctl,
	.d_poll = randomdev_poll,
	.d_name = "random",
};

/* For use with make_dev(9)/destroy_dev(9). */
static struct cdev *random_dev;

/* Allow the sysadmin to select the broad category of
 * entropy types to harvest.
 * Here because the rest of the kernel checks these in random_harvest() calls.
 */
struct harvest_select harvest = { 1, 1, 1, 1 };

/* Set up the sysctl root node for the entropy device */
SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW, 0, "Random Number Generator");

/* ARGSUSED */
static int
randomdev_read(struct cdev *dev __unused, struct uio *uio, int flag)
{
	int c, error = 0;
	void *random_buf;

	/* Blocking logic */
	if (!random_adaptor->seeded)
		error = (*random_adaptor->block)(flag);

	/* The actual read */
	if (!error) {

		random_buf = (void *)malloc(PAGE_SIZE, M_ENTROPY, M_WAITOK);

		while (uio->uio_resid > 0 && !error) {
			c = MIN(uio->uio_resid, PAGE_SIZE);
			c = (*random_adaptor->read)(random_buf, c);
			error = uiomove(random_buf, c, uio);
		}
		/* Finished reading; let the source know so it can do some
		 * optional housekeeping */
		(*random_adaptor->read)(NULL, 0);

		free(random_buf, M_ENTROPY);

	}

	return (error);
}

/* ARGSUSED */
static int
randomdev_write(struct cdev *dev __unused, struct uio *uio, int flag __unused)
{

	/* We used to allow this to insert userland entropy.
	 * We don't any more because (1) this so-called entropy
	 * is usually lousy and (b) its vaguely possible to
	 * mess with entropy harvesting by overdoing a write.
	 * Now we just ignore input like /dev/null does.
	 */
	uio->uio_resid = 0;

	return (0);
}

/* ARGSUSED */
static int
randomdev_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t addr __unused,
    int flags __unused, struct thread *td __unused)
{
	int error = 0;

	switch (cmd) {
		/* Really handled in upper layer */
	case FIOASYNC:
	case FIONBIO:
		break;
	default:
		error = ENOTTY;
	}
	return (error);
}

/* ARGSUSED */
static int
randomdev_poll(struct cdev *dev __unused, int events, struct thread *td)
{
	int revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		if (random_adaptor->seeded)
			revents = events & (POLLIN | POLLRDNORM);
		else
			revents = (*random_adaptor->poll)(events, td);
	}
	return (revents);
}

/* ARGSUSED */
static int
randomdev_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		random_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD, &random_cdevsw,
		    RANDOM_MINOR, NULL, UID_ROOT, GID_WHEEL, 0666, "random");
		make_dev_alias(random_dev, "urandom"); /* compatibility */
		break;

	case MOD_UNLOAD:
		destroy_dev(random_dev);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

/* Order is SI_ORDER_MIDDLE */
DEV_MODULE(randomdev, randomdev_modevent, NULL);
MODULE_VERSION(randomdev, 1);

/* Internal stub/fake routines for when no entropy device is loaded */
static void random_harvest_phony(const void *, u_int, u_int, enum esource);
static int random_read_phony(void *, int);

/* hold the addresses of the routines which are actually called if
 * the random device is loaded.
 */
static void (*reap_func)(const void *, u_int, u_int, enum esource) = random_harvest_phony;
static int (*read_func)(void *, int) = random_read_phony;

/* If no entropy device is loaded, don't spam the console with warnings */
static int warned = 0;

/* Initialise the harvester when/if it is loaded */
void
randomdev_init_harvester(void (*reaper)(const void *, u_int, u_int, enum esource),
    int (*reader)(void *, int))
{
	reap_func = reaper;
	read_func = reader;
	warned = 1;
}

/* Deinitialise the harvester when/if it is unloaded */
void
randomdev_deinit_harvester(void)
{
	reap_func = random_harvest_phony;
	read_func = random_read_phony;
	warned = 0;
}

/* Entropy harvesting routine. This is supposed to be fast; do
 * not do anything slow in here!
 * Implemented as in indirect call to allow non-inclusion of
 * the entropy device.
 */
void
random_harvest(const void *entropy, u_int count, u_int bits, enum esource origin)
{
	(*reap_func)(entropy, count, bits, origin);
}

/* If the entropy device is not loaded, don't act on harvesting calls
 * and just return.
 */
/* ARGSUSED */
static void
random_harvest_phony(const void *entropy __unused, u_int count __unused,
    u_int bits __unused, enum esource origin __unused)
{
}

/* Kernel API version of read_random().
 * Implemented as in indirect call to allow non-inclusion of
 * the entropy device.
 */
int
read_random(void *buf, int count)
{
	return ((*read_func)(buf, count));
}

/* If the entropy device is not loaded, make a token effort to
 * provide _some_ kind of output. No warranty of the quality of
 * this output is made, mainly because its lousy. Caveat Emptor.
 */
/* XXX: FIX!! Move this to dummy_rng.c */
static int
random_read_phony(void *buf, int count)
{
	u_long randval;
	int size, i;

	if (!warned) {
		log(LOG_WARNING, "random device not loaded; using insecure pseudo-random number generator\n");
		warned = 1;
	}

	/* srandom() is called in kern/init_main.c:proc0_post() */

	/* Fill buf[] with random(9) output */
	for (i = 0; i < count; i+= (int)sizeof(u_long)) {
		randval = random();
		size = MIN(count - i, sizeof(u_long));
		memcpy(&((char *)buf)[i], &randval, (size_t)size);
	}

	return (count);
}

/* Helper routine to enable kproc_exit() to work while the module is
 * being (or has been) unloaded.
 * This routine is in this file because it is always linked into the kernel,
 * and will thus never be unloaded. This is critical for unloadable modules
 * that have threads.
 */
void
randomdev_set_wakeup_exit(void *control)
{
	wakeup(control);
	kproc_exit(0);
	/* NOTREACHED */
}
