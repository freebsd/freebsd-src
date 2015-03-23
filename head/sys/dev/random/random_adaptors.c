/*-
 * Copyright (c) 2013 Mark R V Murray
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
 * Copyright (c) 2013 David E. O'Brien <obrien@NUXI.org>
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

#include "opt_random.h"

#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/queue.h>
#include <sys/random.h>
#include <sys/sbuf.h>
#include <sys/selinfo.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/live_entropy_sources.h>

/* The random_adaptors_lock protects random_adaptors_list and friends and random_adaptor.
 * We need a sleepable lock for uiomove/block/poll/sbuf/sysctl.
 */
static struct sx random_adaptors_lock;
LIST_HEAD(adaptors_head, random_adaptors);
static struct adaptors_head random_adaptors_list = LIST_HEAD_INITIALIZER(random_adaptors_list);
static struct random_adaptor *random_adaptor = NULL; /* Currently active adaptor */
/* End of data items requiring random_adaptors_lock protection */

/* The random_readrate_mtx mutex protects the read-rate estimator.
 */
static struct mtx random_read_rate_mtx;
static int random_adaptor_read_rate_cache;
/* End of data items requiring random_readrate_mtx mutex protection */

static struct selinfo rsel;

/* Utility routine to change active adaptor when the random_adaptors_list
 * gets modified.
 *
 * Walk a list of registered random(4) adaptors and pick either a requested
 * one or the highest priority one, whichever comes first. Panic on failure
 * as the fallback must always be the "dummy" adaptor.
 */
static void
random_adaptor_choose(void)
{
	char			 rngs[128], *token, *cp;
	struct random_adaptors	*rra, *rrai;
	struct random_adaptor	*random_adaptor_previous;
	int			 primax;

	/* We are going to be messing with random_adaptor.
	 * Exclusive lock is mandatory.
	 */
	sx_assert(&random_adaptors_lock, SA_XLOCKED);

	random_adaptor_previous = random_adaptor;

	random_adaptor = NULL;
	if (TUNABLE_STR_FETCH("kern.random.active_adaptor", rngs, sizeof(rngs))) {
		cp = rngs;

		/* XXX: FIX!! (DES):
		 * - fetch tunable once, at boot
		 * - make sysctl r/w
		 * - when fetching tunable or processing a sysctl
		 *   write, parse into list of strings so we don't
		 *   have to do it here again and again
		 * - sysctl read should return a reconstructed string
		 */
		while ((token = strsep(&cp, ",")) != NULL) {
			LIST_FOREACH(rra, &random_adaptors_list, rra_entries)
				if (strcmp(rra->rra_name, token) == 0) {
					random_adaptor = rra->rra_ra;
					break;
				}
			if (random_adaptor != NULL) {
				printf("random: selecting requested adaptor <%s>\n",
				    random_adaptor->ra_ident);
				break;
			}
			else
				printf("random: requested adaptor <%s> not available\n",
				    token);
		}
	}

	primax = 0;
	if (random_adaptor == NULL) {
		/*
		 * Fall back to the highest priority item on the available
		 * RNG list.
		 */
		LIST_FOREACH(rrai, &random_adaptors_list, rra_entries) {
			if (rrai->rra_ra->ra_priority >= primax) {
				random_adaptor = rrai->rra_ra;
				primax = rrai->rra_ra->ra_priority;
			}
		}
		if (random_adaptor != NULL)
			printf("random: selecting highest priority adaptor <%s>\n",
			    random_adaptor->ra_ident);
	}

	KASSERT(random_adaptor != NULL, ("adaptor not found"));

	/* If we are changing adaptors, deinit the old and init the new. */
	if (random_adaptor != random_adaptor_previous) {
#ifdef RANDOM_DEBUG
		printf("random: %s - changing from %s to %s\n", __func__,
		    (random_adaptor_previous == NULL ? "NULL" : random_adaptor_previous->ra_ident),
		    random_adaptor->ra_ident);
#endif
		if (random_adaptor_previous != NULL) {
			randomdev_deinit_reader();
			(random_adaptor_previous->ra_deinit)();
		}
		(random_adaptor->ra_init)();
	}

	randomdev_init_reader(random_adaptor->ra_read);
}


/* XXX: FIX!! Make sure we are not inserting a duplicate */
void
random_adaptor_register(const char *name, struct random_adaptor *ra)
{
	struct random_adaptors *rra;

	KASSERT(name != NULL && ra != NULL, ("invalid input to %s", __func__));

	rra = malloc(sizeof(*rra), M_ENTROPY, M_WAITOK);
	rra->rra_name = name;
	rra->rra_ra = ra;

	sx_xlock(&random_adaptors_lock);
	LIST_INSERT_HEAD(&random_adaptors_list, rra, rra_entries);
	random_adaptor_choose();
	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));
	sx_xunlock(&random_adaptors_lock);
}

void
random_adaptor_deregister(const char *name)
{
	struct random_adaptors *rra;

	KASSERT(name != NULL, ("invalid input to %s", __func__));

	sx_xlock(&random_adaptors_lock);
	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));
	LIST_FOREACH(rra, &random_adaptors_list, rra_entries)
		if (strcmp(rra->rra_name, name) == 0) {
			LIST_REMOVE(rra, rra_entries);
			break;
		}
	random_adaptor_choose();
	sx_xunlock(&random_adaptors_lock);

	free(rra, M_ENTROPY);
}

/* ARGSUSED */
int
random_adaptor_read(struct cdev *dev __unused, struct uio *uio, int flags)
{
	void *random_buf;
	int c, error;
	ssize_t nbytes;

#ifdef RANDOM_DEBUG_VERBOSE
	printf("random: %s %ld\n", __func__, uio->uio_resid);
#endif

	random_buf = malloc(PAGE_SIZE, M_ENTROPY, M_WAITOK);

	sx_slock(&random_adaptors_lock);

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	/* Let the entropy source do any pre-read setup. */
	(random_adaptor->ra_read)(NULL, 0);

	/* (Un)Blocking logic */
	error = 0;
	while (!random_adaptor->ra_seeded() && error == 0) {
		if (flags & O_NONBLOCK)	{
			error = EWOULDBLOCK;
			break;
		}

		/* Sleep instead of going into a spin-frenzy */
		error = sx_sleep(&random_adaptor, &random_adaptors_lock,
		    PUSER | PCATCH, "randrd", hz/10);
		KASSERT(random_adaptor != NULL, ("No active random adaptor in %s",
		    __func__));

		/* keep tapping away at the pre-read until we seed/unblock. */
		(random_adaptor->ra_read)(NULL, 0);
	}

	mtx_lock(&random_read_rate_mtx);

	/* The read-rate stuff is a rough indication of the instantaneous read rate,
	 * used to increase the use of 'live' entropy sources when lots of reads are done.
	 */
	nbytes = (uio->uio_resid + 32 - 1)/32; /* Round up to units of 32 */
	random_adaptor_read_rate_cache += nbytes*32;
	random_adaptor_read_rate_cache = MIN(random_adaptor_read_rate_cache, 32);

	mtx_unlock(&random_read_rate_mtx);

	if (error == 0) {
		nbytes = uio->uio_resid;

		/* The actual read */
		while (uio->uio_resid && !error) {
			c = MIN(uio->uio_resid, PAGE_SIZE);
			(random_adaptor->ra_read)(random_buf, c);
			error = uiomove(random_buf, c, uio);
		}

		/* Let the entropy source do any post-read cleanup. */
		(random_adaptor->ra_read)(NULL, 1);

		if (nbytes != uio->uio_resid && (error == ERESTART ||
		    error == EINTR) )
			error = 0;	/* Return partial read, not error. */

	}
	sx_sunlock(&random_adaptors_lock);

	free(random_buf, M_ENTROPY);

	return (error);
}

int
random_adaptor_read_rate(void)
{
	int ret;

	mtx_lock(&random_read_rate_mtx);

	ret = random_adaptor_read_rate_cache;
	random_adaptor_read_rate_cache = 1;

	mtx_unlock(&random_read_rate_mtx);

	return (ret);
}

/* ARGSUSED */
int
random_adaptor_write(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{
	int c, error = 0;
	void *random_buf;
	ssize_t nbytes;

#ifdef RANDOM_DEBUG
	printf("random: %s %zd\n", __func__, uio->uio_resid);
#endif

	random_buf = malloc(PAGE_SIZE, M_ENTROPY, M_WAITOK);

	sx_slock(&random_adaptors_lock);

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	nbytes = uio->uio_resid;
	while (uio->uio_resid > 0 && error == 0) {
		c = MIN(uio->uio_resid, PAGE_SIZE);
		error = uiomove(random_buf, c, uio);
		if (error)
			break;
		(random_adaptor->ra_write)(random_buf, c);

		/* Introduce an annoying delay to stop swamping */
		error = sx_sleep(&random_adaptor, &random_adaptors_lock,
		    PUSER | PCATCH, "randwr", hz/10);
		KASSERT(random_adaptor != NULL, ("No active random adaptor in %s",
		    __func__));
	}

	sx_sunlock(&random_adaptors_lock);

	if (nbytes != uio->uio_resid && (error == ERESTART ||
	    error == EINTR) )
		error = 0;	/* Partial write, not error. */

	free(random_buf, M_ENTROPY);

	return (error);
}

/* ARGSUSED */
int
random_adaptor_poll(struct cdev *dev __unused, int events, struct thread *td __unused)
{

#ifdef RANDOM_DEBUG
	printf("random: %s\n", __func__);
#endif

	sx_slock(&random_adaptors_lock);

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	if (events & (POLLIN | POLLRDNORM)) {
		if (random_adaptor->ra_seeded())
			events &= (POLLIN | POLLRDNORM);
		else
			selrecord(td, &rsel);
	}

	sx_sunlock(&random_adaptors_lock);

	return (events);
}

/* This will be called by the entropy processor when it seeds itself and becomes secure */
void
random_adaptor_unblock(void)
{

	selwakeuppri(&rsel, PUSER);
	wakeup(&random_adaptor);
	printf("random: unblocking device.\n");

	/* Do arc4random(9) a favour while we are about it. */
	(void)atomic_cmpset_int(&arc4rand_iniseed_state, ARC4_ENTR_NONE, ARC4_ENTR_HAVE);
}

static int
random_sysctl_adaptors_handler(SYSCTL_HANDLER_ARGS)
{
	struct random_adaptors *rra;
	struct sbuf sbuf;
	int error, count;

	sx_slock(&random_adaptors_lock);
	sbuf_new_for_sysctl(&sbuf, NULL, 64, req);
	count = 0;
	LIST_FOREACH(rra, &random_adaptors_list, rra_entries)
		sbuf_printf(&sbuf, "%s%s(%d)",
		    (count++ ? "," : ""), rra->rra_name, rra->rra_ra->ra_priority);

	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	sx_sunlock(&random_adaptors_lock);

	return (error);
}

static int
random_sysctl_active_adaptor_handler(SYSCTL_HANDLER_ARGS)
{
	struct random_adaptors *rra;
	struct sbuf sbuf;
	int error;

	sx_slock(&random_adaptors_lock);
	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	sbuf_new_for_sysctl(&sbuf, NULL, 16, req);
	LIST_FOREACH(rra, &random_adaptors_list, rra_entries)
		if (rra->rra_ra == random_adaptor) {
			sbuf_cat(&sbuf, rra->rra_name);
			break;
		}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	sx_sunlock(&random_adaptors_lock);

	return (error);
}

void
random_adaptors_init(void)
{

#ifdef RANDOM_DEBUG
	printf("random: %s\n", __func__);
#endif

	SYSCTL_PROC(_kern_random, OID_AUTO, adaptors,
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, random_sysctl_adaptors_handler, "A",
	    "Random Number Generator adaptors");

	SYSCTL_PROC(_kern_random, OID_AUTO, active_adaptor,
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, random_sysctl_active_adaptor_handler, "A",
	    "Active Random Number Generator Adaptor");

	sx_init(&random_adaptors_lock, "random_adaptors");

	mtx_init(&random_read_rate_mtx, "read rate mutex", NULL, MTX_DEF);

	/* The dummy adaptor is not a module by itself, but part of the
	 * randomdev module.
	 */
	random_adaptor_register("dummy", &randomdev_dummy);

	live_entropy_sources_init();
}

void
random_adaptors_deinit(void)
{

#ifdef RANDOM_DEBUG
	printf("random: %s\n", __func__);
#endif

	live_entropy_sources_deinit();

	/* Don't do this! Panic will surely follow! */
	/* random_adaptor_deregister("dummy"); */

	mtx_destroy(&random_read_rate_mtx);

	sx_destroy(&random_adaptors_lock);
}

/*
 * Reseed the active adaptor shortly before starting init(8).
 */
/* ARGSUSED */
static void
random_adaptors_seed(void *unused __unused)
{
 
	sx_slock(&random_adaptors_lock);
	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	random_adaptor->ra_reseed();
	sx_sunlock(&random_adaptors_lock);

	arc4rand(NULL, 0, 1);
}
SYSINIT(random_seed, SI_SUB_KTHREAD_INIT, SI_ORDER_FIRST,
    random_adaptors_seed, NULL);
