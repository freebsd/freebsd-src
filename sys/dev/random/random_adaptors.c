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
#include <dev/random/randomdev_soft.h>
#include <dev/random/random_adaptors.h>

LIST_HEAD(adaptors_head, random_adaptors);
static struct adaptors_head adaptors = LIST_HEAD_INITIALIZER(adaptors);
static struct sx adaptors_lock; /* need a sleepable lock */

/* List for the dynamic sysctls */
static struct sysctl_ctx_list random_clist;

struct random_adaptor *random_adaptor;

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers and data structures");

int
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

	return (0);
}

struct random_adaptor *
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
 * Walk a list of registered random(4) adaptors and pick the last non-selected
 * one.
 *
 * If none are selected, use yarrow if available.
 */
void
random_adaptor_choose(struct random_adaptor **adaptor)
{
	char			 rngs[128], *token, *cp;
	struct random_adaptors	*rppi, *ramax;
	unsigned		 primax;

	KASSERT(adaptor != NULL, ("pre-conditions failed"));

	*adaptor = NULL;
	if (TUNABLE_STR_FETCH("kern.random.active_adaptor", rngs, sizeof(rngs))) {
		cp = rngs;

		while ((token = strsep(&cp, ",")) != NULL)
			if ((*adaptor = random_adaptor_get(token)) != NULL)
				break;
			else if (bootverbose)
				printf("%s random adaptor is not available,"
				    " skipping\n", token);
	}

	primax = 0U;
	if (*adaptor == NULL) {
		/*
		 * Fall back to the highest priority item on the available
		 * RNG list.
		 */
		sx_slock(&adaptors_lock);

		ramax = NULL;
		LIST_FOREACH(rppi, &adaptors, entries) {
			if (rppi->rsp->priority >= primax) {
				ramax = rppi;
				primax = rppi->rsp->priority;
			}
		}
		if (ramax != NULL)
			*adaptor = ramax->rsp;

		sx_sunlock(&adaptors_lock);

		if (bootverbose && *adaptor)
			printf("Falling back to <%s> random adaptor\n",
			    (*adaptor)->ident);
	}
}

static void
random_adaptors_deinit(void *unused)
{

	sx_destroy(&adaptors_lock);
	sysctl_ctx_free(&random_clist);
}

static int
random_sysctl_adaptors_handler(SYSCTL_HANDLER_ARGS)
{
	struct random_adaptors	*rpp;
	int error, count;

	count = error = 0;

	sx_slock(&adaptors_lock);

	if (LIST_EMPTY(&adaptors))
		error = SYSCTL_OUT(req, "", 0);
	else {
		LIST_FOREACH(rpp, &adaptors, entries) {

			error = SYSCTL_OUT(req, ",", count++ ? 1 : 0);
			if (error)
				break;

			error = SYSCTL_OUT(req, rpp->name, strlen(rpp->name));
			if (error)
				break;
		}
	}

	sx_sunlock(&adaptors_lock);

	return (error);
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

static void
random_adaptors_init(void *unused)
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
}

SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW, 0, "Random Number Generator");

SYSINIT(random_adaptors, SI_SUB_RANDOM, SI_ORDER_FIRST, random_adaptors_init,
    NULL);
SYSUNINIT(random_adaptors, SI_SUB_RANDOM, SI_ORDER_FIRST,
    random_adaptors_deinit, NULL);

static void
random_adaptors_reseed(void *unused)
{

	(void)unused;
	if (random_adaptor != NULL)
		(*random_adaptor->reseed)();
	arc4rand(NULL, 0, 1);
}
SYSINIT(random_reseed, SI_SUB_INTRINSIC_POST, SI_ORDER_SECOND,
    random_adaptors_reseed, NULL);
