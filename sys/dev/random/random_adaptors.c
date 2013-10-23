/*-
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/libkern.h>

#include <dev/random/random_adaptors.h>
#include <dev/random/randomdev.h>

LIST_HEAD(adaptors_head, random_adaptors);
static struct adaptors_head adaptors = LIST_HEAD_INITIALIZER(adaptors);
static struct sx adaptors_lock; /* need a sleepable lock */

/* List for the dynamic sysctls */
static struct sysctl_ctx_list random_clist;

MALLOC_DEFINE(M_RANDOM_ADAPTORS, "random_adaptors", "Random adaptors buffers");

int
random_adaptor_register(const char *name, struct random_adaptor *rsp)
{
	struct random_adaptors *rpp;

	KASSERT(name != NULL && rsp != NULL, ("invalid input to %s", __func__));

	rpp = malloc(sizeof(struct random_adaptors), M_RANDOM_ADAPTORS, M_WAITOK);
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

static void
random_adaptors_deinit(void *unused)
{

	sx_destroy(&adaptors_lock);
	sysctl_ctx_free(&random_clist);
}

#define	NO_ADAPTORS	"<no loaded adaptors>"
static int
random_sysctl_adaptors_handler(SYSCTL_HANDLER_ARGS)
{
	struct random_adaptors	*rpp;
	int error;

	error = 0;

	sx_slock(&adaptors_lock);

	if (LIST_EMPTY(&adaptors))
		error = SYSCTL_OUT(req, NO_ADAPTORS, strlen(NO_ADAPTORS));

	LIST_FOREACH(rpp, &adaptors, entries) {
		error = SYSCTL_OUT(req, " ", 1);
		if (!error)
			error = SYSCTL_OUT(req, rpp->name, strlen(rpp->name));
		if (error)
			break;
	}

	sx_sunlock(&adaptors_lock);

	return (error);
}

static void
random_adaptors_init(void *unused)
{

	SYSCTL_PROC(_kern_random, OID_AUTO, adaptors,
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, random_sysctl_adaptors_handler, "",
	    "Random Number Generator adaptors");

	sx_init(&adaptors_lock, "random_adaptors");
}

SYSCTL_NODE(_kern, OID_AUTO, random, CTLFLAG_RW, 0, "Random Number Generator");

SYSINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_FIRST, random_adaptors_init,
    NULL);
SYSUNINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    random_adaptors_deinit, NULL);
