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
 * Except ...
 *
 * The adaptor code all goes into random_adaptor.c, which is also compiled
 * the kernel by default. The module in that file is initialised before
 * this one.
 *
 * Other modules must be initialised after the above two, and are
 * software random processors which plug into random_adaptor.c.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_random.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/random_harvestq.h>

#define RANDOM_MINOR	0

static d_ioctl_t randomdev_ioctl;

static struct cdevsw random_cdevsw = {
	.d_name = "random",
	.d_version = D_VERSION,
	.d_read = random_adaptor_read,
	.d_write = random_adaptor_write,
	.d_poll = random_adaptor_poll,
	.d_ioctl = randomdev_ioctl,
};

/* For use with make_dev(9)/destroy_dev(9). */
static struct cdev *random_dev;

/* Set up the sysctl root node for the entropy device */
SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW, 0, "Random Number Generator");

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers and data structures");

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

/* ARGSUSED */
static int
randomdev_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		printf("random: entropy device infrastructure driver\n");
		random_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD, &random_cdevsw,
		    RANDOM_MINOR, NULL, UID_ROOT, GID_WHEEL, 0644, "random");
		make_dev_alias(random_dev, "urandom"); /* compatibility */
		random_adaptors_init();
		break;

	case MOD_UNLOAD:
		random_adaptors_deinit();
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

DEV_MODULE_ORDERED(randomdev, randomdev_modevent, NULL, SI_ORDER_SECOND);
MODULE_VERSION(randomdev, 1);

/* ================
 * Harvesting stubs
 * ================
 */

/* Internal stub/fake routine for when no entropy processor is loaded.
 * If the entropy device is not loaded, don't act on harvesting calls
 * and just return.
 */
/* ARGSUSED */
static void
random_harvest_phony(const void *entropy __unused, u_int count __unused,
    u_int bits __unused, enum random_entropy_source origin __unused)
{
}

/* Hold the address of the routine which is actually called */
static void (*reap_func)(const void *, u_int, u_int, enum random_entropy_source) = random_harvest_phony;

/* Initialise the harvester when/if it is loaded */
void
randomdev_init_harvester(void (*reaper)(const void *, u_int, u_int, enum random_entropy_source))
{

	reap_func = reaper;
}

/* Deinitialise the harvester when/if it is unloaded */
void
randomdev_deinit_harvester(void)
{

	reap_func = random_harvest_phony;
}

/* Entropy harvesting routine.
 * Implemented as in indirect call to allow non-inclusion of
 * the entropy device.
 */
void
random_harvest(const void *entropy, u_int count, u_int bits, enum random_entropy_source origin)
{

	(*reap_func)(entropy, count, bits, origin);
}

/* ================================
 * Internal reading stubs and fakes
 * ================================
 */

/* Hold the address of the routine which is actually called */
static u_int (*read_func)(uint8_t *, u_int) = dummy_random_read_phony;

/* Initialise the reader when/if it is loaded */
void
randomdev_init_reader(u_int (*reader)(uint8_t *, u_int))
{

	read_func = reader;
}

/* Deinitialise the reader when/if it is unloaded */
void
randomdev_deinit_reader(void)
{

	read_func = dummy_random_read_phony;
}

/* Kernel API version of read_random().
 * Implemented as in indirect call to allow non-inclusion of
 * the entropy device.
 */
int
read_random(void *buf, int count)
{

	return ((int)(*read_func)(buf, (u_int)count));
}
