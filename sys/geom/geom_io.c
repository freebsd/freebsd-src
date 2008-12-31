/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/geom/geom_io.c,v 1.75.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/stack.h>

#include <sys/errno.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <sys/devicestat.h>

#include <vm/uma.h>

static struct g_bioq g_bio_run_down;
static struct g_bioq g_bio_run_up;
static struct g_bioq g_bio_run_task;

static u_int pace;
static uma_zone_t	biozone;

#include <machine/atomic.h>

static void
g_bioq_lock(struct g_bioq *bq)
{

	mtx_lock(&bq->bio_queue_lock);
}

static void
g_bioq_unlock(struct g_bioq *bq)
{

	mtx_unlock(&bq->bio_queue_lock);
}

#if 0
static void
g_bioq_destroy(struct g_bioq *bq)
{

	mtx_destroy(&bq->bio_queue_lock);
}
#endif

static void
g_bioq_init(struct g_bioq *bq)
{

	TAILQ_INIT(&bq->bio_queue);
	mtx_init(&bq->bio_queue_lock, "bio queue", NULL, MTX_DEF);
}

static struct bio *
g_bioq_first(struct g_bioq *bq)
{
	struct bio *bp;

	bp = TAILQ_FIRST(&bq->bio_queue);
	if (bp != NULL) {
		KASSERT((bp->bio_flags & BIO_ONQUEUE),
		    ("Bio not on queue bp=%p target %p", bp, bq));
		bp->bio_flags &= ~BIO_ONQUEUE;
		TAILQ_REMOVE(&bq->bio_queue, bp, bio_queue);
		bq->bio_queue_length--;
	}
	return (bp);
}

struct bio *
g_new_bio(void)
{
	struct bio *bp;

	bp = uma_zalloc(biozone, M_NOWAIT | M_ZERO);
#ifdef KTR
	if (KTR_COMPILE & KTR_GEOM) {
		struct stack st;

		CTR1(KTR_GEOM, "g_new_bio(): %p", bp);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3, 0);
	}
#endif
	return (bp);
}

struct bio *
g_alloc_bio(void)
{
	struct bio *bp;

	bp = uma_zalloc(biozone, M_WAITOK | M_ZERO);
#ifdef KTR
	if (KTR_COMPILE & KTR_GEOM) {
		struct stack st;

		CTR1(KTR_GEOM, "g_alloc_bio(): %p", bp);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3, 0);
	}
#endif
	return (bp);
}

void
g_destroy_bio(struct bio *bp)
{
#ifdef KTR
	if (KTR_COMPILE & KTR_GEOM) {
		struct stack st;

		CTR1(KTR_GEOM, "g_destroy_bio(): %p", bp);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3, 0);
	}
#endif
	uma_zfree(biozone, bp);
}

struct bio *
g_clone_bio(struct bio *bp)
{
	struct bio *bp2;

	bp2 = uma_zalloc(biozone, M_NOWAIT | M_ZERO);
	if (bp2 != NULL) {
		bp2->bio_parent = bp;
		bp2->bio_cmd = bp->bio_cmd;
		bp2->bio_length = bp->bio_length;
		bp2->bio_offset = bp->bio_offset;
		bp2->bio_data = bp->bio_data;
		bp2->bio_attribute = bp->bio_attribute;
		bp->bio_children++;
	}
#ifdef KTR
	if (KTR_COMPILE & KTR_GEOM) {
		struct stack st;

		CTR2(KTR_GEOM, "g_clone_bio(%p): %p", bp, bp2);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3, 0);
	}
#endif
	return(bp2);
}

struct bio *
g_duplicate_bio(struct bio *bp)
{
	struct bio *bp2;

	bp2 = uma_zalloc(biozone, M_WAITOK | M_ZERO);
	bp2->bio_parent = bp;
	bp2->bio_cmd = bp->bio_cmd;
	bp2->bio_length = bp->bio_length;
	bp2->bio_offset = bp->bio_offset;
	bp2->bio_data = bp->bio_data;
	bp2->bio_attribute = bp->bio_attribute;
	bp->bio_children++;
#ifdef KTR
	if (KTR_COMPILE & KTR_GEOM) {
		struct stack st;

		CTR2(KTR_GEOM, "g_duplicate_bio(%p): %p", bp, bp2);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3, 0);
	}
#endif
	return(bp2);
}

void
g_io_init()
{

	g_bioq_init(&g_bio_run_down);
	g_bioq_init(&g_bio_run_up);
	g_bioq_init(&g_bio_run_task);
	biozone = uma_zcreate("g_bio", sizeof (struct bio),
	    NULL, NULL,
	    NULL, NULL,
	    0, 0);
}

int
g_io_getattr(const char *attr, struct g_consumer *cp, int *len, void *ptr)
{
	struct bio *bp;
	int error;

	g_trace(G_T_BIO, "bio_getattr(%s)", attr);
	bp = g_alloc_bio();
	bp->bio_cmd = BIO_GETATTR;
	bp->bio_done = NULL;
	bp->bio_attribute = attr;
	bp->bio_length = *len;
	bp->bio_data = ptr;
	g_io_request(bp, cp);
	error = biowait(bp, "ggetattr");
	*len = bp->bio_completed;
	g_destroy_bio(bp);
	return (error);
}

int
g_io_flush(struct g_consumer *cp)
{
	struct bio *bp;
	int error;

	g_trace(G_T_BIO, "bio_flush(%s)", cp->provider->name);
	bp = g_alloc_bio();
	bp->bio_cmd = BIO_FLUSH;
	bp->bio_done = NULL;
	bp->bio_attribute = NULL;
	bp->bio_offset = cp->provider->mediasize;
	bp->bio_length = 0;
	bp->bio_data = NULL;
	g_io_request(bp, cp);
	error = biowait(bp, "gflush");
	g_destroy_bio(bp);
	return (error);
}

static int
g_io_check(struct bio *bp)
{
	struct g_consumer *cp;
	struct g_provider *pp;

	cp = bp->bio_from;
	pp = bp->bio_to;

	/* Fail if access counters dont allow the operation */
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_GETATTR:
		if (cp->acr == 0)
			return (EPERM);
		break;
	case BIO_WRITE:
	case BIO_DELETE:
	case BIO_FLUSH:
		if (cp->acw == 0)
			return (EPERM);
		break;
	default:
		return (EPERM);
	}
	/* if provider is marked for error, don't disturb. */
	if (pp->error)
		return (pp->error);

	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		/* Zero sectorsize is a probably lack of media */
		if (pp->sectorsize == 0)
			return (ENXIO);
		/* Reject I/O not on sector boundary */
		if (bp->bio_offset % pp->sectorsize)
			return (EINVAL);
		/* Reject I/O not integral sector long */
		if (bp->bio_length % pp->sectorsize)
			return (EINVAL);
		/* Reject requests before or past the end of media. */
		if (bp->bio_offset < 0)
			return (EIO);
		if (bp->bio_offset > pp->mediasize)
			return (EIO);
		break;
	default:
		break;
	}
	return (0);
}

void
g_io_request(struct bio *bp, struct g_consumer *cp)
{
	struct g_provider *pp;

	KASSERT(cp != NULL, ("NULL cp in g_io_request"));
	KASSERT(bp != NULL, ("NULL bp in g_io_request"));
	pp = cp->provider;
	KASSERT(pp != NULL, ("consumer not attached in g_io_request"));
#ifdef DIAGNOSTIC
	KASSERT(bp->bio_driver1 == NULL,
	    ("bio_driver1 used by the consumer (geom %s)", cp->geom->name));
	KASSERT(bp->bio_driver2 == NULL,
	    ("bio_driver2 used by the consumer (geom %s)", cp->geom->name));
	KASSERT(bp->bio_pflags == 0,
	    ("bio_pflags used by the consumer (geom %s)", cp->geom->name));
	/*
	 * Remember consumer's private fields, so we can detect if they were
	 * modified by the provider.
	 */
	bp->_bio_caller1 = bp->bio_caller1;
	bp->_bio_caller2 = bp->bio_caller2;
	bp->_bio_cflags = bp->bio_cflags;
#endif

	if (bp->bio_cmd & (BIO_READ|BIO_WRITE|BIO_GETATTR)) {
		KASSERT(bp->bio_data != NULL,
		    ("NULL bp->data in g_io_request(cmd=%hhu)", bp->bio_cmd));
	}
	if (bp->bio_cmd & (BIO_DELETE|BIO_FLUSH)) {
		KASSERT(bp->bio_data == NULL,
		    ("non-NULL bp->data in g_io_request(cmd=%hhu)",
		    bp->bio_cmd));
	}
	if (bp->bio_cmd & (BIO_READ|BIO_WRITE|BIO_DELETE)) {
		KASSERT(bp->bio_offset % cp->provider->sectorsize == 0,
		    ("wrong offset %jd for sectorsize %u",
		    bp->bio_offset, cp->provider->sectorsize));
		KASSERT(bp->bio_length % cp->provider->sectorsize == 0,
		    ("wrong length %jd for sectorsize %u",
		    bp->bio_length, cp->provider->sectorsize));
	}

	g_trace(G_T_BIO, "bio_request(%p) from %p(%s) to %p(%s) cmd %d",
	    bp, cp, cp->geom->name, pp, pp->name, bp->bio_cmd);

	bp->bio_from = cp;
	bp->bio_to = pp;
	bp->bio_error = 0;
	bp->bio_completed = 0;

	KASSERT(!(bp->bio_flags & BIO_ONQUEUE),
	    ("Bio already on queue bp=%p", bp));
	bp->bio_flags |= BIO_ONQUEUE;

	binuptime(&bp->bio_t0);

	/*
	 * The statistics collection is lockless, as such, but we
	 * can not update one instance of the statistics from more
	 * than one thread at a time, so grab the lock first.
	 */
	g_bioq_lock(&g_bio_run_down);
	if (g_collectstats & 1)
		devstat_start_transaction(pp->stat, &bp->bio_t0);
	if (g_collectstats & 2)
		devstat_start_transaction(cp->stat, &bp->bio_t0);

	pp->nstart++;
	cp->nstart++;
	TAILQ_INSERT_TAIL(&g_bio_run_down.bio_queue, bp, bio_queue);
	g_bio_run_down.bio_queue_length++;
	g_bioq_unlock(&g_bio_run_down);

	/* Pass it on down. */
	wakeup(&g_wait_down);
}

void
g_io_deliver(struct bio *bp, int error)
{
	struct g_consumer *cp;
	struct g_provider *pp;

	KASSERT(bp != NULL, ("NULL bp in g_io_deliver"));
	pp = bp->bio_to;
	KASSERT(pp != NULL, ("NULL bio_to in g_io_deliver"));
#ifdef DIAGNOSTIC
	KASSERT(bp->bio_caller1 == bp->_bio_caller1,
	    ("bio_caller1 used by the provider %s", pp->name));
	KASSERT(bp->bio_caller2 == bp->_bio_caller2,
	    ("bio_caller2 used by the provider %s", pp->name));
	KASSERT(bp->bio_cflags == bp->_bio_cflags,
	    ("bio_cflags used by the provider %s", pp->name));
#endif
	cp = bp->bio_from;
	if (cp == NULL) {
		bp->bio_error = error;
		bp->bio_done(bp);
		return;
	}
	KASSERT(cp != NULL, ("NULL bio_from in g_io_deliver"));
	KASSERT(cp->geom != NULL, ("NULL bio_from->geom in g_io_deliver"));
	KASSERT(bp->bio_completed >= 0, ("bio_completed can't be less than 0"));
	KASSERT(bp->bio_completed <= bp->bio_length,
	    ("bio_completed can't be greater than bio_length"));

	g_trace(G_T_BIO,
"g_io_deliver(%p) from %p(%s) to %p(%s) cmd %d error %d off %jd len %jd",
	    bp, cp, cp->geom->name, pp, pp->name, bp->bio_cmd, error,
	    (intmax_t)bp->bio_offset, (intmax_t)bp->bio_length);

	KASSERT(!(bp->bio_flags & BIO_ONQUEUE),
	    ("Bio already on queue bp=%p", bp));

	/*
	 * XXX: next two doesn't belong here
	 */
	bp->bio_bcount = bp->bio_length;
	bp->bio_resid = bp->bio_bcount - bp->bio_completed;

	/*
	 * The statistics collection is lockless, as such, but we
	 * can not update one instance of the statistics from more
	 * than one thread at a time, so grab the lock first.
	 */
	g_bioq_lock(&g_bio_run_up);
	if (g_collectstats & 1)
		devstat_end_transaction_bio(pp->stat, bp);
	if (g_collectstats & 2)
		devstat_end_transaction_bio(cp->stat, bp);

	cp->nend++;
	pp->nend++;
	if (error != ENOMEM) {
		bp->bio_error = error;
		TAILQ_INSERT_TAIL(&g_bio_run_up.bio_queue, bp, bio_queue);
		bp->bio_flags |= BIO_ONQUEUE;
		g_bio_run_up.bio_queue_length++;
		g_bioq_unlock(&g_bio_run_up);
		wakeup(&g_wait_up);
		return;
	}
	g_bioq_unlock(&g_bio_run_up);

	if (bootverbose)
		printf("ENOMEM %p on %p(%s)\n", bp, pp, pp->name);
	bp->bio_children = 0;
	bp->bio_inbed = 0;
	g_io_request(bp, cp);
	pace++;
	return;
}

void
g_io_schedule_down(struct thread *tp __unused)
{
	struct bio *bp;
	off_t excess;
	int error;

	for(;;) {
		g_bioq_lock(&g_bio_run_down);
		bp = g_bioq_first(&g_bio_run_down);
		if (bp == NULL) {
			CTR0(KTR_GEOM, "g_down going to sleep");
			msleep(&g_wait_down, &g_bio_run_down.bio_queue_lock,
			    PRIBIO | PDROP, "-", hz/10);
			continue;
		}
		CTR0(KTR_GEOM, "g_down has work to do");
		g_bioq_unlock(&g_bio_run_down);
		if (pace > 0) {
			CTR1(KTR_GEOM, "g_down pacing self (pace %d)", pace);
			pause("g_down", hz/10);
			pace--;
		}
		error = g_io_check(bp);
		if (error) {
			CTR3(KTR_GEOM, "g_down g_io_check on bp %p provider "
			    "%s returned %d", bp, bp->bio_to->name, error);
			g_io_deliver(bp, error);
			continue;
		}
		CTR2(KTR_GEOM, "g_down processing bp %p provider %s", bp,
		    bp->bio_to->name);
		switch (bp->bio_cmd) {
		case BIO_READ:
		case BIO_WRITE:
		case BIO_DELETE:
			/* Truncate requests to the end of providers media. */
			/*
			 * XXX: What if we truncate because of offset being
			 * bad, not length?
			 */
			excess = bp->bio_offset + bp->bio_length;
			if (excess > bp->bio_to->mediasize) {
				excess -= bp->bio_to->mediasize;
				bp->bio_length -= excess;
				if (excess > 0)
					CTR3(KTR_GEOM, "g_down truncated bio "
					    "%p provider %s by %d", bp,
					    bp->bio_to->name, excess);
			}
			/* Deliver zero length transfers right here. */
			if (bp->bio_length == 0) {
				g_io_deliver(bp, 0);
				CTR2(KTR_GEOM, "g_down terminated 0-length "
				    "bp %p provider %s", bp, bp->bio_to->name);
				continue;
			}
			break;
		default:
			break;
		}
		THREAD_NO_SLEEPING();
		CTR4(KTR_GEOM, "g_down starting bp %p provider %s off %ld "
		    "len %ld", bp, bp->bio_to->name, bp->bio_offset,
		    bp->bio_length);
		bp->bio_to->geom->start(bp);
		THREAD_SLEEPING_OK();
	}
}

void
bio_taskqueue(struct bio *bp, bio_task_t *func, void *arg)
{
	bp->bio_task = func;
	bp->bio_task_arg = arg;
	/*
	 * The taskqueue is actually just a second queue off the "up"
	 * queue, so we use the same lock.
	 */
	g_bioq_lock(&g_bio_run_up);
	KASSERT(!(bp->bio_flags & BIO_ONQUEUE),
	    ("Bio already on queue bp=%p target taskq", bp));
	bp->bio_flags |= BIO_ONQUEUE;
	TAILQ_INSERT_TAIL(&g_bio_run_task.bio_queue, bp, bio_queue);
	g_bio_run_task.bio_queue_length++;
	wakeup(&g_wait_up);
	g_bioq_unlock(&g_bio_run_up);
}


void
g_io_schedule_up(struct thread *tp __unused)
{
	struct bio *bp;
	for(;;) {
		g_bioq_lock(&g_bio_run_up);
		bp = g_bioq_first(&g_bio_run_task);
		if (bp != NULL) {
			g_bioq_unlock(&g_bio_run_up);
			THREAD_NO_SLEEPING();
			CTR1(KTR_GEOM, "g_up processing task bp %p", bp);
			bp->bio_task(bp->bio_task_arg);
			THREAD_SLEEPING_OK();
			continue;
		}
		bp = g_bioq_first(&g_bio_run_up);
		if (bp != NULL) {
			g_bioq_unlock(&g_bio_run_up);
			THREAD_NO_SLEEPING();
			CTR4(KTR_GEOM, "g_up biodone bp %p provider %s off "
			    "%ld len %ld", bp, bp->bio_to->name,
			    bp->bio_offset, bp->bio_length);
			biodone(bp);
			THREAD_SLEEPING_OK();
			continue;
		}
		CTR0(KTR_GEOM, "g_up going to sleep");
		msleep(&g_wait_up, &g_bio_run_up.bio_queue_lock,
		    PRIBIO | PDROP, "-", hz/10);
	}
}

void *
g_read_data(struct g_consumer *cp, off_t offset, off_t length, int *error)
{
	struct bio *bp;
	void *ptr;
	int errorc;

	KASSERT(length > 0 && length >= cp->provider->sectorsize &&
	    length <= MAXPHYS, ("g_read_data(): invalid length %jd",
	    (intmax_t)length));

	bp = g_alloc_bio();
	bp->bio_cmd = BIO_READ;
	bp->bio_done = NULL;
	bp->bio_offset = offset;
	bp->bio_length = length;
	ptr = g_malloc(length, M_WAITOK);
	bp->bio_data = ptr;
	g_io_request(bp, cp);
	errorc = biowait(bp, "gread");
	if (error != NULL)
		*error = errorc;
	g_destroy_bio(bp);
	if (errorc) {
		g_free(ptr);
		ptr = NULL;
	}
	return (ptr);
}

int
g_write_data(struct g_consumer *cp, off_t offset, void *ptr, off_t length)
{
	struct bio *bp;
	int error;

	KASSERT(length > 0 && length >= cp->provider->sectorsize &&
	    length <= MAXPHYS, ("g_write_data(): invalid length %jd",
	    (intmax_t)length));

	bp = g_alloc_bio();
	bp->bio_cmd = BIO_WRITE;
	bp->bio_done = NULL;
	bp->bio_offset = offset;
	bp->bio_length = length;
	bp->bio_data = ptr;
	g_io_request(bp, cp);
	error = biowait(bp, "gwrite");
	g_destroy_bio(bp);
	return (error);
}

int
g_delete_data(struct g_consumer *cp, off_t offset, off_t length)
{
	struct bio *bp;
	int error;

	KASSERT(length > 0 && length >= cp->provider->sectorsize &&
	    length <= MAXPHYS, ("g_delete_data(): invalid length %jd",
	    (intmax_t)length));

	bp = g_alloc_bio();
	bp->bio_cmd = BIO_DELETE;
	bp->bio_done = NULL;
	bp->bio_offset = offset;
	bp->bio_length = length;
	bp->bio_data = NULL;
	g_io_request(bp, cp);
	error = biowait(bp, "gdelete");
	g_destroy_bio(bp);
	return (error);
}

void
g_print_bio(struct bio *bp)
{
	const char *pname, *cmd = NULL;

	if (bp->bio_to != NULL)
		pname = bp->bio_to->name;
	else
		pname = "[unknown]";

	switch (bp->bio_cmd) {
	case BIO_GETATTR:
		cmd = "GETATTR";
		printf("%s[%s(attr=%s)]", pname, cmd, bp->bio_attribute);
		return;
	case BIO_FLUSH:
		cmd = "FLUSH";
		printf("%s[%s]", pname, cmd);
		return;
	case BIO_READ:
		cmd = "READ";
	case BIO_WRITE:
		if (cmd == NULL)
			cmd = "WRITE";
	case BIO_DELETE:
		if (cmd == NULL)
			cmd = "DELETE";
		printf("%s[%s(offset=%jd, length=%jd)]", pname, cmd,
		    (intmax_t)bp->bio_offset, (intmax_t)bp->bio_length);
		return;
	default:
		cmd = "UNKNOWN";
		printf("%s[%s()]", pname, cmd);
		return;
	}
	/* NOTREACHED */
}
