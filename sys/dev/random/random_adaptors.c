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

#include "opt_random.h"

#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
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
#include <sys/selinfo.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_adaptors.h>

/* The random_adaptors_lock protects random_adaptors_list and friends and random_adaptor.
 * We need a sleepable lock for uiomove/block/poll/sbuf/sysctl.
 */
static struct sx random_adaptors_lock;
LIST_HEAD(adaptors_head, random_adaptors);
static struct adaptors_head random_adaptors_list = LIST_HEAD_INITIALIZER(random_adaptors_list);
static struct random_adaptor *random_adaptor = NULL; /* Currently active adaptor */
/* End of data items requiring random_adaptors_lock protection */

/* The random_rate_mtx mutex protects the consistency of the read-rate logic. */
struct mtx random_rate_mtx;
int random_adaptor_read_rate_cache;
/* End of data items requiring random_rate_mtx mutex protection */

/* The random_reseed_mtx mutex protects seeding and polling/blocking.
 * This is passed into the software entropy hasher/processor.
 */
struct mtx random_reseed_mtx;
/* End of data items requiring random_reseed_mtx mutex protection */

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
		if (random_adaptor_previous != NULL)
			(random_adaptor_previous->ra_deinit)();
		(random_adaptor->ra_init)(&random_reseed_mtx);
	}
}


/* XXX: FIX!! Make sure we are not inserting a duplicate */
void
random_adaptor_register(const char *name, struct random_adaptor *ra)
{
	struct random_adaptors *rra;

	KASSERT(name != NULL && ra != NULL, ("invalid input to %s", __func__));

	rra = malloc(sizeof(struct random_adaptors), M_ENTROPY, M_WAITOK);
	rra->rra_name = name;
	rra->rra_ra = ra;

	sx_xlock(&random_adaptors_lock);
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
	sx_xunlock(&random_adaptors_lock);

	free(rra, M_ENTROPY);
}

/*
 * Per-instance structure.
 *
 * List of locks
 * XXX: FIX!!
 */
struct random_adaptor_softc {
	int oink;
	int tweet;
};

static void
random_adaptor_dtor(void *data)
{
	struct random_adaptor_softc *ras = data;

	free(ras, M_ENTROPY);
}

/* ARGSUSED */
int
random_adaptor_open(struct cdev *dev __unused, int flags, int mode __unused, struct thread *td __unused)
{
	struct random_adaptor_softc *ras;

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	ras = malloc(sizeof(struct random_adaptor_softc), M_ENTROPY, M_WAITOK|M_ZERO);
	/* XXX: FIX!! Set up softc here */

	devfs_set_cdevpriv(ras, random_adaptor_dtor);

	/* Give the source a chance to do some pre-read/write startup */
	if (flags & FREAD) {
		sx_slock(&random_adaptors_lock);
		(random_adaptor->ra_read)(NULL, 0);
		sx_sunlock(&random_adaptors_lock);
	} else if (flags & FWRITE) {
		sx_slock(&random_adaptors_lock);
		(random_adaptor->ra_write)(NULL, 0);
		sx_sunlock(&random_adaptors_lock);
	}

	return (0);
}

/* ARGSUSED */
int
random_adaptor_close(struct cdev *dev __unused, int flags, int fmt __unused, struct thread *td __unused)
{

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	/* Give the source a chance to do some post-read/write shutdown */
	if (flags & FREAD) {
		sx_slock(&random_adaptors_lock);
		(random_adaptor->ra_read)(NULL, 1);
		sx_sunlock(&random_adaptors_lock);
	} else if (flags & FWRITE) {
		sx_slock(&random_adaptors_lock);
		(random_adaptor->ra_write)(NULL, 1);
		sx_sunlock(&random_adaptors_lock);
	}

	return (0);
}

/* ARGSUSED */
int
random_adaptor_read(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{
	void *random_buf;
	int c, error;
	u_int npages;
	struct random_adaptor_softc *ras;

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	error = devfs_get_cdevpriv((void **)&ras);
	if (error == 0) {

		sx_slock(&random_adaptors_lock);

		/* Blocking logic */
		mtx_lock(&random_reseed_mtx);
		while (!random_adaptor->ra_seeded() && !error) {
			if (flags & O_NONBLOCK)
				error = EWOULDBLOCK;
			else
				error = msleep(&random_adaptor, &random_reseed_mtx, PUSER | PCATCH, "block", 0);
		}
		mtx_unlock(&random_reseed_mtx);

		/* The actual read */
		if (!error) {
			/* The read-rate stuff is a *VERY* crude indication of the instantaneous read rate,
			 * designed to increase the use of 'live' entropy sources when lots of reads are done.
			 */
			mtx_lock(&random_rate_mtx);
			npages = (uio->uio_resid + PAGE_SIZE - 1)/PAGE_SIZE;
			random_adaptor_read_rate_cache += npages;
			random_adaptor_read_rate_cache = MIN(random_adaptor_read_rate_cache, 32);
			mtx_unlock(&random_rate_mtx);

			random_buf = (void *)malloc(npages*PAGE_SIZE, M_ENTROPY, M_WAITOK);
			while (uio->uio_resid > 0 && !error) {
				c = MIN(uio->uio_resid, npages*PAGE_SIZE);
				(random_adaptor->ra_read)(random_buf, c);
				error = uiomove(random_buf, c, uio);
			}
			free(random_buf, M_ENTROPY);
		}

		sx_sunlock(&random_adaptors_lock);

	}

	return (error);
}

int
random_adaptor_read_rate(void)
{
	int ret;

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	mtx_lock(&random_rate_mtx);
	ret = random_adaptor_read_rate_cache;
	mtx_unlock(&random_rate_mtx);

	return (ret);
}

/* ARGSUSED */
int
random_adaptor_write(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{
	int error;
	struct random_adaptor_softc *ras;

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	sx_slock(&random_adaptors_lock);
	error = devfs_get_cdevpriv((void **)&ras);
	if (error == 0) {
		/* We used to allow this to insert userland entropy.
		 * We don't any more because (1) this so-called entropy
		 * is usually lousy and (b) its vaguely possible to
		 * mess with entropy harvesting by overdoing a write.
		 * Now we just ignore input like /dev/null does.
		 */
		/* XXX: FIX!! Now that RWFILE is gone, we need to get this back.
		 * ALSO: See devfs_get_cdevpriv(9) and friends for ways to build per-session nodes.
		 */
		uio->uio_resid = 0;
		/* c = (random_adaptor->ra_write)(random_buf, c); */
	}
	sx_sunlock(&random_adaptors_lock);

	return (error);
}

/* ARGSUSED */
int
random_adaptor_poll(struct cdev *dev __unused, int events, struct thread *td __unused)
{
	struct random_adaptor_softc *ras;

	KASSERT(random_adaptor != NULL, ("No active random adaptor in %s", __func__));

	if (devfs_get_cdevpriv((void **)&ras) != 0)
		return (events &
		    (POLLHUP|POLLIN|POLLRDNORM|POLLOUT|POLLWRNORM));

	sx_slock(&random_adaptors_lock);
	mtx_lock(&random_reseed_mtx);
	if (events & (POLLIN | POLLRDNORM)) {
		if (random_adaptor->ra_seeded())
			events &= (POLLIN | POLLRDNORM);
		else
			selrecord(td, &rsel);
	}
	mtx_unlock(&random_reseed_mtx);
	sx_sunlock(&random_adaptors_lock);

	return (events);
}

/* This will be called by the entropy processor when it seeds itself and becomes secure */
void
random_adaptor_unblock(void)
{

	mtx_assert(&random_reseed_mtx, MA_OWNED);

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

	mtx_init(&random_rate_mtx, "read rate mutex", NULL, MTX_DEF);
	mtx_init(&random_reseed_mtx, "read rate mutex", NULL, MTX_DEF);

	/* The dummy adaptor is not a module by itself, but part of the
	 * randomdev module.
	 */
	random_adaptor_register("dummy", &randomdev_dummy);
}
SYSINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    random_adaptors_init, NULL);

/* ARGSUSED */
static void
random_adaptors_deinit(void *unused __unused)
{
	/* Don't do this! Panic will surely follow! */
	/* random_adaptor_deregister("dummy"); */

	mtx_destroy(&random_reseed_mtx);
	mtx_destroy(&random_rate_mtx);

	sx_destroy(&random_adaptors_lock);
}
SYSUNINIT(random_adaptors, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    random_adaptors_deinit, NULL);

/*
 * First seed.
 *
 * NB! NB! NB!
 * NB! NB! NB!
 *
 * It turns out this is bloody dangerous. I was fiddling with code elsewhere
 * and managed to get conditions where a safe (i.e. seeded) entropy device should
 * not have been possible. This managed to hide that by unblocking the device anyway.
 * As crap randomness is not directly distinguishable from good randomness, this
 * could have gone unnoticed for quite a while.
 *
 * NB! NB! NB!
 * NB! NB! NB!
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
