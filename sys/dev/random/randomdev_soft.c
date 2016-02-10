/*-
 * Copyright (c) 2000-2013 Mark R V Murray
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

#include "opt_random.h"

#if !defined(RANDOM_YARROW) && !defined(RANDOM_FORTUNA)
#define RANDOM_YARROW
#elif defined(RANDOM_YARROW) && defined(RANDOM_FORTUNA)
#error "Must define either RANDOM_YARROW or RANDOM_FORTUNA"
#endif
#if defined(RANDOM_FORTUNA)
#error "Fortuna is not yet implemented"
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/random.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/random/randomdev.h>
#include <dev/random/randomdev_soft.h>
#include <dev/random/random_harvestq.h>
#include <dev/random/random_adaptors.h>
#if defined(RANDOM_YARROW)
#include <dev/random/yarrow.h>
#endif
#if defined(RANDOM_FORTUNA)
#include <dev/random/fortuna.h>
#endif


static int randomdev_poll(int event, struct thread *td);
static int randomdev_block(int flag);
static void randomdev_flush_reseed(void);

#if defined(RANDOM_YARROW)
static struct random_adaptor random_context = {
	.ident = "Software, Yarrow",
	.init = randomdev_init,
	.deinit = randomdev_deinit,
	.block = randomdev_block,
	.read = random_yarrow_read,
	.poll = randomdev_poll,
	.reseed = randomdev_flush_reseed,
	.seeded = 0, /* This will be seeded during entropy processing */
	.priority = 90, /* High priority, so top of the list. Fortuna may still win. */
};
#define RANDOM_MODULE_NAME	yarrow
#define RANDOM_CSPRNG_NAME	"yarrow"
#endif

#if defined(RANDOM_FORTUNA)
static struct random_adaptor random_context = {
	.ident = "Software, Fortuna",
	.init = randomdev_init,
	.deinit = randomdev_deinit,
	.block = randomdev_block,
	.read = random_fortuna_read,
	.poll = randomdev_poll,
	.reseed = randomdev_flush_reseed,
	.seeded = 0, /* This will be excplicitly seeded at startup when secured */
	.priority = 100, /* High priority, so top of the list. Beat Yarrow. */
};
#define RANDOM_MODULE_NAME	fortuna
#define RANDOM_CSPRNG_NAME	"fortuna"
#endif

TUNABLE_INT("kern.random.sys.seeded", &random_context.seeded);

/* List for the dynamic sysctls */
static struct sysctl_ctx_list random_clist;

/* ARGSUSED */
static int
random_check_boolean(SYSCTL_HANDLER_ARGS)
{
	if (oidp->oid_arg1 != NULL && *(u_int *)(oidp->oid_arg1) != 0)
		*(u_int *)(oidp->oid_arg1) = 1;
	return (sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req));
}

void
randomdev_init(void)
{
	struct sysctl_oid *random_sys_o, *random_sys_harvest_o;

#if defined(RANDOM_YARROW)
	random_yarrow_init_alg(&random_clist);
#endif
#if defined(RANDOM_FORTUNA)
	random_fortuna_init_alg(&random_clist);
#endif

	random_sys_o = SYSCTL_ADD_NODE(&random_clist,
	    SYSCTL_STATIC_CHILDREN(_kern_random),
	    OID_AUTO, "sys", CTLFLAG_RW, 0,
	    "Entropy Device Parameters");

	SYSCTL_ADD_PROC(&random_clist,
	    SYSCTL_CHILDREN(random_sys_o),
	    OID_AUTO, "seeded", CTLTYPE_INT | CTLFLAG_RW,
	    &random_context.seeded, 0, random_check_boolean, "I",
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
	    &harvest.swi, 1, random_check_boolean, "I",
	    "Harvest SWI entropy");

	random_harvestq_init(random_process_event);

	/* Register the randomness harvesting routine */
	randomdev_init_harvester(random_harvestq_internal,
	    random_context.read);
}

void
randomdev_deinit(void)
{
	/* Deregister the randomness harvesting routine */
	randomdev_deinit_harvester();

#if defined(RANDOM_YARROW)
	random_yarrow_deinit_alg();
#endif
#if defined(RANDOM_FORTUNA)
	random_fortuna_deinit_alg();
#endif

	sysctl_ctx_free(&random_clist);
}

void
randomdev_unblock(void)
{
	if (!random_context.seeded) {
		selwakeuppri(&random_context.rsel, PUSER);
		wakeup(&random_context);
                printf("random: unblocking device.\n");
		random_context.seeded = 1;
	}
	/* Do arc4random(9) a favour while we are about it. */
	(void)atomic_cmpset_int(&arc4rand_iniseed_state, ARC4_ENTR_NONE,
	    ARC4_ENTR_HAVE);
}

static int
randomdev_poll(int events, struct thread *td)
{
	int revents = 0;

	mtx_lock(&random_reseed_mtx);

	if (random_context.seeded)
		revents = events & (POLLIN | POLLRDNORM);
	else
		selrecord(td, &random_context.rsel);

	mtx_unlock(&random_reseed_mtx);
	return (revents);
}

static int
randomdev_block(int flag)
{
	int error = 0;

	mtx_lock(&random_reseed_mtx);

	/* Blocking logic */
	while (!random_context.seeded && !error) {
		if (flag & O_NONBLOCK)
			error = EWOULDBLOCK;
		else {
			printf("random: blocking on read.\n");
			error = msleep(&random_context,
			    &random_reseed_mtx,
			    PUSER | PCATCH, "block", 0);
		}
	}
	mtx_unlock(&random_reseed_mtx);

	return (error);
}

/* Helper routine to perform explicit reseeds */
static void
randomdev_flush_reseed(void)
{
	/* Command a entropy queue flush and wait for it to finish */
	random_kthread_control = 1;
	while (random_kthread_control)
		pause("-", hz / 10);

#if defined(RANDOM_YARROW)
	/* This ultimately calls randomdev_unblock() */
	random_yarrow_reseed();
#endif
#if defined(RANDOM_FORTUNA)
	/* This ultimately calls randomdev_unblock() */
	random_fortuna_reseed();
#endif
}

static int
randomdev_modevent(module_t mod __unused, int type, void *unused __unused)
{

	switch (type) {
	case MOD_LOAD:
		random_adaptor_register(RANDOM_CSPRNG_NAME, &random_context);
		/*
		 * For statically built kernels that contain both device
		 * random and options PADLOCK_RNG/RDRAND_RNG/etc..,
		 * this event handler will do nothing, since the random
		 * driver-specific handlers are loaded after these HW
		 * consumers, and hence hasn't yet registered for this event.
		 *
		 * In case where both the random driver and RNG's are built
		 * as seperate modules, random.ko is loaded prior to *_rng.ko's
		 * (by dependency). This event handler is there to delay
		 * creation of /dev/{u,}random and attachment of this *_rng.ko.
		 */
		EVENTHANDLER_INVOKE(random_adaptor_attach, &random_context);
		return (0);
	}

	return (EINVAL);
}

RANDOM_ADAPTOR_MODULE(RANDOM_MODULE_NAME, randomdev_modevent, 1);
