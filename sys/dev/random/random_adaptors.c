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
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/queue.h>
#include <sys/random.h>
#include <sys/sbuf.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>

/* These are the data structures and associated items that need to be locked against
 * "under-the-feet" changes.
 */
static struct sx random_adaptors_lock; /* need a sleepable lock */

LIST_HEAD(adaptors_head, random_adaptors);
static struct adaptors_head random_adaptors_list = LIST_HEAD_INITIALIZER(random_adaptors_list);
static struct random_adaptor *random_adaptor = NULL; /* Currently active adaptor */
/* End of data items requiring adaptor lock protection */

/* The rate mutex protects the consistency of the read-rate logic. */
struct mtx rate_mtx;
int random_adaptor_read_rate_cache;
/* End of data items requiring rate mutex protection */

MALLOC_DEFINE(M_ENTROPY, "entropy", "Entropy harvesting buffers and data structures");

static void random_adaptor_choose(void);

void
random_adaptor_register(const char *name, struct random_adaptor *ra)
{
	struct random_adaptors *rra;

	KASSERT(name != NULL && ra != NULL, ("invalid input to %s", __func__));

	rra = malloc(sizeof(struct random_adaptors), M_ENTROPY, M_WAITOK);
	rra->rra_name = name;
	rra->rra_ra = ra;

	sx_xlock(&random_adaptors_lock);

	/* XXX: FIX!! Make sure we are not inserting a duplicate */
	LIST_INSERT_HEAD(&random_adaptors_list, rra, rra_entries);

	random_adaptor_choose();

	sx_xunlock(&random_adaptors_lock);

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));
}

void
random_adaptor_deregister(const char *name)
{
	struct random_adaptors *rra;

	KASSERT(name != NULL, ("invalid input to %s", __func__));
	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	sx_xlock(&random_adaptors_lock);

	LIST_FOREACH(rra, &random_adaptors_list, rra_entries)
		if (strcmp(rra->rra_name, name) == 0) {
			LIST_REMOVE(rra, rra_entries);
			break;
		}

	random_adaptor_choose();
	/* It is conceivable that there is no active random adaptor here,
	 * e.g. at shutdown.
	 */

	sx_xunlock(&random_adaptors_lock);

	if (rra != NULL)
		free(rra, M_ENTROPY);
}

int
random_adaptor_block(int flag)
{
	int ret;

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	sx_slock(&random_adaptors_lock);

	ret = random_adaptor->ra_block(flag);

	sx_sunlock(&random_adaptors_lock);

	return ret;
}

int
random_adaptor_read(struct uio *uio, int flag)
{
	int c, error = 0;
	void *random_buf;

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	/* The read-rate stuff is a *VERY* crude measure of the instantaneous read rate, designed
	 * to increase the use of 'live' entropy sources when lots of reads are done.
	 */
	mtx_lock(&rate_mtx);
	random_adaptor_read_rate_cache += (int)((uio->uio_resid + PAGE_SIZE + 1)/PAGE_SIZE);
	mtx_unlock(&rate_mtx);

	sx_slock(&random_adaptors_lock);

	/* Blocking logic */
	if (random_adaptor->ra_seeded)
		error = (random_adaptor->ra_block)(flag);

	/* The actual read */
	if (!error) {

		random_buf = (void *)malloc(PAGE_SIZE, M_ENTROPY, M_WAITOK);

		while (uio->uio_resid > 0 && !error) {
			c = MIN(uio->uio_resid, PAGE_SIZE);
			c = (random_adaptor->ra_read)(random_buf, c);
			error = uiomove(random_buf, c, uio);
		}
		/* Finished reading; let the source know so it can do some
		 * optional housekeeping */
		(random_adaptor->ra_read)(NULL, 0);

		free(random_buf, M_ENTROPY);

	}

	sx_sunlock(&random_adaptors_lock);

	return (error);
}

int
random_adaptor_read_rate(void)
{
	int ret;

	mtx_lock(&rate_mtx);
	ret = random_adaptor_read_rate_cache = random_adaptor_read_rate_cache ? random_adaptor_read_rate_cache%32 + 1 : 1;
	mtx_unlock(&rate_mtx);

	return (ret);
}

int
random_adaptor_poll(int events, struct thread *td)
{
	int revents = 0;

	sx_slock(&random_adaptors_lock);

	if (events & (POLLIN | POLLRDNORM)) {
		if (random_adaptor->ra_seeded)
			revents = events & (POLLIN | POLLRDNORM);
		else
			revents = (random_adaptor->ra_poll)(events, td);
	}

	sx_sunlock(&random_adaptors_lock);

	return (revents);
}

/*
 * Walk a list of registered random(4) adaptors and pick either a requested
 * one or the highest priority one, whichever comes first. Panic on failure
 * as the fallback must be the "dummy" adaptor.
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
		if (random_adaptor_previous != NULL)
			(random_adaptor_previous->ra_deinit)();
		(random_adaptor->ra_init)();
	}
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

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	sx_slock(&random_adaptors_lock);

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

	sx_init(&random_adaptors_lock, "random_adaptors");
	mtx_init(&rate_mtx, "read rate mutex", NULL, MTX_DEF);

	/* This dummy "thing" is not a module by itself, but part of the
	 * randomdev module.
	 */
	random_adaptor_register("dummy", &randomdev_dummy);
}

/* ARGSUSED */
static void
random_adaptors_deinit(void *unused __unused)
{
	/* Don't do this! Panic will follow. */
	/* random_adaptor_deregister("dummy"); */

	mtx_destroy(&rate_mtx);
	sx_destroy(&random_adaptors_lock);
}

/*
 * First seed.
 *
 * NB! NB! NB!
 *
 * NB! NB! NB!
 *
 * It turns out this is bloody dangerous. I was fiddling with code elsewhere
 * and managed to get conditions where a safe (i.e. seeded) entropy device should
 * not have been possible. This managed to hide that by unblocking the device anyway.
 * As crap randomness is not directly distinguishable from good randomness, this
 * could have gone unnoticed for quite a while.
 *
 * Very luckily, the probe-time entropy is very nearly good enough to cause a
 * first seed all of the time, and the default settings for other entropy
 * harvesting causes a proper, safe, first seed (unblock) in short order after that.
 *
 * That said, the below would be useful where folks are more concerned with
 * a quick start than with extra paranoia in a low-entropy environment.
 *
 * markm - October 2013.
 */
#ifdef RANDOM_AUTOSEED
/* ARGSUSED */
static void
random_adaptors_seed(void *unused __unused)
{
 
	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	sx_slock(&random_adaptors_lock);

	random_adaptor->ra_reseed();

	sx_sunlock(&random_adaptors_lock);

	arc4rand(NULL, 0, 1);
}
SYSINIT(random_seed, SI_SUB_INTRINSIC_POST, SI_ORDER_LAST,
    random_adaptors_reseed, NULL);
#endif /*  RANDOM_AUTOSEED */

SYSINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_SECOND,
    random_adaptors_init, NULL);
SYSUNINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_SECOND,
    random_adaptors_deinit, NULL);
