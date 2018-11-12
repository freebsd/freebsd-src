/*-
 * Copyright (c) 2000-2014 Mark R V Murray
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

/*
 * This is the loadable infrastructure base file for software CSPRNG
 * drivers such as Yarrow or Fortuna.
 *
 * It is anticipated that one instance of this file will be used
 * for _each_ invocation of a CSPRNG, but with different #defines
 * set. See below.
 *
 */

#include "opt_random.h"

#if !defined(RANDOM_YARROW) && !defined(RANDOM_FORTUNA)
#define RANDOM_YARROW
#elif defined(RANDOM_YARROW) && defined(RANDOM_FORTUNA)
#error "Must define either RANDOM_YARROW or RANDOM_FORTUNA"
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

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

static struct random_adaptor random_soft_processor = {
#if defined(RANDOM_YARROW)
#define RANDOM_CSPRNG_NAME	"yarrow"
	.ra_ident = "Yarrow",
	.ra_priority = 90, /* High priority, so top of the list. Fortuna may still win. */
	.ra_read = random_yarrow_read,
	.ra_write = random_yarrow_write,
	.ra_reseed = random_yarrow_reseed,
	.ra_seeded = random_yarrow_seeded,
#endif
#if defined(RANDOM_FORTUNA)
#define RANDOM_CSPRNG_NAME	"fortuna"
	.ra_ident = "Fortuna",
	.ra_priority = 100, /* High priority, so top of the list. Beat Yarrow. */
	.ra_read = random_fortuna_read,
	.ra_write = random_fortuna_write,
	.ra_reseed = random_fortuna_reseed,
	.ra_seeded = random_fortuna_seeded,
#endif
	.ra_init = randomdev_init,
	.ra_deinit = randomdev_deinit,
};

void
randomdev_init(void)
{

#if defined(RANDOM_YARROW)
	random_yarrow_init_alg();
	random_harvestq_init(random_yarrow_process_event, 2);
#endif
#if defined(RANDOM_FORTUNA)
	random_fortuna_init_alg();
	random_harvestq_init(random_fortuna_process_event, 32);
#endif

	/* Register the randomness harvesting routine */
	randomdev_init_harvester(random_harvestq_internal);
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
}

/* ARGSUSED */
static int
randomdev_soft_modevent(module_t mod __unused, int type, void *unused __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		printf("random: SOFT: %s init()\n", RANDOM_CSPRNG_NAME);
		random_adaptor_register(RANDOM_CSPRNG_NAME, &random_soft_processor);
		break;

	case MOD_UNLOAD:
		random_adaptor_deregister(RANDOM_CSPRNG_NAME);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

#if defined(RANDOM_YARROW)
DEV_MODULE(yarrow, randomdev_soft_modevent, NULL);
MODULE_VERSION(yarrow, 1);
MODULE_DEPEND(yarrow, randomdev, 1, 1, 1);
#endif
#if defined(RANDOM_FORTUNA)
DEV_MODULE(fortuna, randomdev_soft_modevent, NULL);
MODULE_VERSION(fortuna, 1);
MODULE_DEPEND(fortuna, randomdev, 1, 1, 1);
#endif
