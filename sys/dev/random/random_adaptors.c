/*-
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
 * Copyright (c) 2013 David E. O'Brien <obrien@NUXI.org>
 * Copyright (c) 2013 Mark R V Murray
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
 */

#include <sys/param.h>
__FBSDID("$FreeBSD$");

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/random.h>
#include <sys/selinfo.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>

LIST_HEAD(adaptors_head, random_adaptors);
static struct adaptors_head adaptors = LIST_HEAD_INITIALIZER(adaptors);
static struct sx adaptors_lock; /* need a sleepable lock */

/* List for the dynamic sysctls */
static struct sysctl_ctx_list random_clist;

struct random_adaptor *random_adaptor = NULL;

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers and data structures");

void
random_adaptor_register(const char *name, struct random_adaptor *rsp)
{
	struct random_adaptors *rpp;

	KASSERT(name != NULL && rsp != NULL, ("invalid input to %s", __func__));

	rpp = malloc(sizeof(struct random_adaptors), M_ENTROPY, M_WAITOK);
	rpp->name = name;
	rpp->rsp = rsp;

	sx_xlock(&adaptors_lock);
	LIST_INSERT_HEAD(&adaptors, rpp, entries);
	sx_xunlock(&adaptors_lock);

	random_adaptor_choose();
}

void
random_adaptor_deregister(const char *name)
{
	struct random_adaptors *rpp;

	KASSERT(name != NULL, ("invalid input to %s", __func__));

	sx_xlock(&adaptors_lock);
	LIST_FOREACH(rpp, &adaptors, entries)
		if (strcmp(rpp->name, name) == 0) {
			LIST_REMOVE(rpp, entries);
			break;
		}
	sx_xunlock(&adaptors_lock);
	if (rpp != NULL)
		free(rpp, M_ENTROPY);

	random_adaptor_choose();
}

static struct random_adaptor *
random_adaptor_get(const char *name)
{
	struct random_adaptors	*rpp;
	struct random_adaptor	*rsp;

	rsp = NULL;

	sx_slock(&adaptors_lock);

	LIST_FOREACH(rpp, &adaptors, entries)
		if (strcmp(rpp->name, name) == 0)
			rsp = rpp->rsp;

	sx_sunlock(&adaptors_lock);

	return (rsp);
}

/*
 * Walk a list of registered random(4) adaptors and pick either a requested
 * one or the highest priority one, whichever comes first. Panic on failure
 * as the fallback must be the "dummy" adaptor.
 */
void
random_adaptor_choose(void)
{
	char			 rngs[128], *token, *cp;
	struct random_adaptors	*rppi;
	struct random_adaptor	*random_adaptor_previous;
	u_int			 primax;

	random_adaptor_previous = random_adaptor;

	random_adaptor = NULL;
	if (TUNABLE_STR_FETCH("kern.random.active_adaptor", rngs, sizeof(rngs))) {
		cp = rngs;

		while ((token = strsep(&cp, ",")) != NULL)
			if ((random_adaptor = random_adaptor_get(token)) != NULL) {
				printf("random: selecting requested adaptor <%s>\n",
				    random_adaptor->ident);
				break;
			}
			else
				printf("random: requested adaptor <%s> not available\n",
				    token);
	}

	primax = 0U;
	if (random_adaptor == NULL) {
		/*
		 * Fall back to the highest priority item on the available
		 * RNG list.
		 */
		sx_slock(&adaptors_lock);
		LIST_FOREACH(rppi, &adaptors, entries) {
			if (rppi->rsp->priority >= primax) {
				random_adaptor = rppi->rsp;
				primax = rppi->rsp->priority;
			}
		}
		sx_sunlock(&adaptors_lock);
		if (random_adaptor != NULL)
			printf("random: selecting highest priority adaptor <%s>\n",
			    random_adaptor->ident);
	}

	KASSERT(random_adaptor != NULL, ("adaptor not found"));

	/* If we are changing adaptors, deinit the old and init the new. */
	if (random_adaptor != random_adaptor_previous) {
		if (random_adaptor_previous != NULL)
			(random_adaptor_previous->deinit)();
		(random_adaptor->init)();
	}
}

static int
random_sysctl_adaptors_handler(SYSCTL_HANDLER_ARGS)
{
	/* XXX: FIX!! Fixed array size, but see below, this may be OK */
	char buf[128], *pbuf;
	struct random_adaptors *rpp;
	int count, snp;
	size_t lbuf;

	sx_slock(&adaptors_lock);

	buf[0] = '\0';
	pbuf = buf;
	lbuf = 256;
	count = 0;
	LIST_FOREACH(rpp, &adaptors, entries) {
		snp = snprintf(pbuf, lbuf, "%s%s(%d)",
		    (count++ ? "," : ""), rpp->name, rpp->rsp->priority);
		KASSERT(snp > 0, ("buffer overflow"));
		lbuf -= (size_t)snp;
		pbuf += snp;
	}

	sx_sunlock(&adaptors_lock);

	return (SYSCTL_OUT(req, buf, strlen(buf)));
}

static int
random_sysctl_active_adaptor_handler(SYSCTL_HANDLER_ARGS)
{
	struct random_adaptor	*rsp;
	struct random_adaptors	*rpp;
	const char		*name;
	int error;

	name = NULL;
	rsp = random_adaptor;

	if (rsp != NULL) {
		sx_slock(&adaptors_lock);

		LIST_FOREACH(rpp, &adaptors, entries)
			if (rpp->rsp == rsp)
				name = rpp->name;

		sx_sunlock(&adaptors_lock);
	}

	if (rsp == NULL || name == NULL)
		error = SYSCTL_OUT(req, "", 0);
	else
		error = SYSCTL_OUT(req, name, strlen(name));

	return (error);
}

/* ARGSUSED */
static void
random_adaptors_init(void *unused __unused)
{

	SYSCTL_PROC(_kern_random, OID_AUTO, adaptors,
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, random_sysctl_adaptors_handler, "",
	    "Random Number Generator adaptors");

	SYSCTL_PROC(_kern_random, OID_AUTO, active_adaptor,
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, random_sysctl_active_adaptor_handler, "",
	    "Active Random Number Generator Adaptor");

	sx_init(&adaptors_lock, "random_adaptors");

	/* This dummy "thing" is not a module by itself, but part of the
	 * randomdev module.
	 */
	random_adaptor_register("dummy", &dummy_random);
}

/* ARGSUSED */
static void
random_adaptors_deinit(void *unused __unused)
{
	/* Don't do this! Panic will follow. */
	/* random_adaptor_deregister("dummy"); */

	sx_destroy(&adaptors_lock);
	sysctl_ctx_free(&random_clist);
}

/* XXX: FIX!! Move this to where its not so well hidden, like randomdev[_soft].c, maybe. */
/*
 * First seed.
 *
 * It turns out this is bloody dangerous. I was fiddling with code elsewhere
 * and managed to get conditions where a safe (i.e. seeded) entropy device should
 * not have been possible. This managed to hide that by seeding the device anyway.
 * As crap randomness is not directly distinguishable from good randomness, this
 * could have gone unnoticed for quite a while.
 *
 * Very luckily, the probe-time entropy is very nearly good enough to cause a
 * first seed all of the time, and the default settings for interrupt- and SWI
 * entropy harvesting causes a proper, safe, first (re)seed in short order
 * after that.
 *
 * That said, the below would be useful where folks are more concerned with
 * a quick start than with extra paranoia.
 *
 * markm - October 2013.
 */
#ifdef RANDOM_AUTOSEED
/* ARGSUSED */
static void
random_adaptors_seed(void *unused __unused)
{
 
	if (random_adaptor != NULL)
		(*random_adaptor->reseed)();
	arc4rand(NULL, 0, 1);
}
SYSINIT(random_seed, SI_SUB_INTRINSIC_POST, SI_ORDER_LAST,
    random_adaptors_reseed, NULL);
#endif /*  RANDOM_AUTOSEED */

SYSINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_SECOND,
    random_adaptors_init, NULL);
SYSUNINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_SECOND,
    random_adaptors_deinit, NULL);
