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
 *
 * $FreeBSD$
 */


#include <sys/param.h>
#include <sys/stdint.h>
#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#include <sched.h>
#else
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#endif

#include <sys/errno.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <geom/geom_stats.h>

static struct g_bioq g_bio_run_down;
static struct g_bioq g_bio_run_up;
static struct g_bioq g_bio_idle;

static u_int pace;

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

	g_bioq_lock(bq);
	bp = TAILQ_FIRST(&bq->bio_queue);
	if (bp != NULL) {
		TAILQ_REMOVE(&bq->bio_queue, bp, bio_queue);
		bq->bio_queue_length--;
	}
	g_bioq_unlock(bq);
	return (bp);
}

static void
g_bioq_enqueue_tail(struct bio *bp, struct g_bioq *rq)
{

	g_bioq_lock(rq);
	TAILQ_INSERT_TAIL(&rq->bio_queue, bp, bio_queue);
	rq->bio_queue_length++;
	g_bioq_unlock(rq);
}

struct bio *
g_new_bio(void)
{
	struct bio *bp;

	bp = g_bioq_first(&g_bio_idle);
	if (bp == NULL)
		bp = g_malloc(sizeof *bp, M_NOWAIT | M_ZERO);
	/* g_trace(G_T_BIO, "g_new_bio() = %p", bp); */
	return (bp);
}

void
g_destroy_bio(struct bio *bp)
{

	/* g_trace(G_T_BIO, "g_destroy_bio(%p)", bp); */
	bzero(bp, sizeof *bp);
	g_bioq_enqueue_tail(bp, &g_bio_idle);
}

struct bio *
g_clone_bio(struct bio *bp)
{
	struct bio *bp2;

	bp2 = g_new_bio();
	if (bp2 != NULL) {
		bp2->bio_parent = bp;
		bp2->bio_cmd = bp->bio_cmd;
		bp2->bio_length = bp->bio_length;
		bp2->bio_offset = bp->bio_offset;
		bp2->bio_data = bp->bio_data;
		bp2->bio_attribute = bp->bio_attribute;
		bp->bio_children++;
	}
	/* g_trace(G_T_BIO, "g_clone_bio(%p) = %p", bp, bp2); */
	return(bp2);
}

void
g_io_init()
{

	g_bioq_init(&g_bio_run_down);
	g_bioq_init(&g_bio_run_up);
	g_bioq_init(&g_bio_idle);
}

int
g_io_setattr(const char *attr, struct g_consumer *cp, int len, void *ptr)
{
	struct bio *bp;
	int error;

	g_trace(G_T_BIO, "bio_setattr(%s)", attr);
	bp = g_new_bio();
	bp->bio_cmd = BIO_SETATTR;
	bp->bio_done = NULL;
	bp->bio_attribute = attr;
	bp->bio_length = len;
	bp->bio_data = ptr;
	g_io_request(bp, cp);
	error = biowait(bp, "gsetattr");
	g_destroy_bio(bp);
	return (error);
}


int
g_io_getattr(const char *attr, struct g_consumer *cp, int *len, void *ptr)
{
	struct bio *bp;
	int error;

	g_trace(G_T_BIO, "bio_getattr(%s)", attr);
	bp = g_new_bio();
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
	case BIO_SETATTR:
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
		/* Reject I/O not on sector boundary */
		if (bp->bio_offset % pp->sectorsize)
			return (EINVAL);
		/* Reject I/O not integral sector long */
		if (bp->bio_length % pp->sectorsize)
			return (EINVAL);
		/* Reject requests past the end of media. */
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
	struct bintime bt;

	pp = cp->provider;
	KASSERT(cp != NULL, ("NULL cp in g_io_request"));
	KASSERT(bp != NULL, ("NULL bp in g_io_request"));
	KASSERT(bp->bio_data != NULL, ("NULL bp->data in g_io_request"));
	KASSERT(pp != NULL, ("consumer not attached in g_io_request"));

	bp->bio_from = cp;
	bp->bio_to = pp;
	bp->bio_error = 0;
	bp->bio_completed = 0;

	if (g_collectstats) {
		binuptime(&bt);
		bp->bio_t0 = bt;
		if (cp->stat->nop == cp->stat->nend)
			cp->stat->wentbusy = bt; /* Consumer is idle */
		if (pp->stat->nop == pp->stat->nend)
			pp->stat->wentbusy = bt; /* Provider is idle */
	}
	cp->stat->nop++;
	pp->stat->nop++;

	/* Pass it on down. */
	g_trace(G_T_BIO, "bio_request(%p) from %p(%s) to %p(%s) cmd %d",
	    bp, cp, cp->geom->name, pp, pp->name, bp->bio_cmd);
	g_bioq_enqueue_tail(bp, &g_bio_run_down);
	wakeup(&g_wait_down);
}

void
g_io_deliver(struct bio *bp, int error)
{
	struct g_consumer *cp;
	struct g_provider *pp;
	struct bintime t1, dt;
	int idx;

	cp = bp->bio_from;
	pp = bp->bio_to;
	KASSERT(bp != NULL, ("NULL bp in g_io_deliver"));
	KASSERT(cp != NULL, ("NULL bio_from in g_io_deliver"));
	KASSERT(cp->geom != NULL, ("NULL bio_from->geom in g_io_deliver"));
	KASSERT(pp != NULL, ("NULL bio_to in g_io_deliver"));

	g_trace(G_T_BIO,
"g_io_deliver(%p) from %p(%s) to %p(%s) cmd %d error %d off %jd len %jd",
	    bp, cp, cp->geom->name, pp, pp->name, bp->bio_cmd, error,
	    (intmax_t)bp->bio_offset, (intmax_t)bp->bio_length);

	if (g_collectstats) {
		switch (bp->bio_cmd) {
		case BIO_READ:    idx =  G_STAT_IDX_READ;    break;
		case BIO_WRITE:   idx =  G_STAT_IDX_WRITE;   break;
		case BIO_DELETE:  idx =  G_STAT_IDX_DELETE;  break;
		case BIO_GETATTR: idx =  -1; break;
		case BIO_SETATTR: idx =  -1; break;
		default:
			panic("unknown bio_cmd in g_io_deliver");
			break;
		}
		binuptime(&t1);
		/* Raise the "inconsistent" flag for userland */
		atomic_set_acq_int(&cp->stat->updating, 1);
		atomic_set_acq_int(&pp->stat->updating, 1);
		if (idx >= 0) {
			/* Account the service time */
			dt = t1;
			bintime_sub(&dt, &bp->bio_t0);
			bintime_add(&cp->stat->ops[idx].dt, &dt);
			bintime_add(&pp->stat->ops[idx].dt, &dt);
			/* ... and the metrics */
			pp->stat->ops[idx].nbyte += bp->bio_completed;
			cp->stat->ops[idx].nbyte += bp->bio_completed;
			pp->stat->ops[idx].nop++;
			cp->stat->ops[idx].nop++;
			/* ... and any errors */
			if (error == ENOMEM) {
				cp->stat->ops[idx].nmem++;
				pp->stat->ops[idx].nmem++;
			} else if (error != 0) {
				cp->stat->ops[idx].nerr++;
				pp->stat->ops[idx].nerr++;
			}
		}
		/* Account for busy time on the consumer */
		dt = t1;
		bintime_sub(&dt, &cp->stat->wentbusy);
		bintime_add(&cp->stat->bt, &dt);
		cp->stat->wentbusy = t1;
		/* Account for busy time on the provider */
		dt = t1;
		bintime_sub(&dt, &pp->stat->wentbusy);
		bintime_add(&pp->stat->bt, &dt);
		pp->stat->wentbusy = t1;
		/* Mark the structures as consistent again */
		atomic_store_rel_int(&cp->stat->updating, 0);
		atomic_store_rel_int(&pp->stat->updating, 0);
	}
	cp->stat->nend++;
	pp->stat->nend++;

	if (error == ENOMEM) {
		printf("ENOMEM %p on %p(%s)\n", bp, pp, pp->name);
		g_io_request(bp, cp);
		pace++;
		return;
	}
	bp->bio_error = error;
	g_bioq_enqueue_tail(bp, &g_bio_run_up);
	wakeup(&g_wait_up);
}

void
g_io_schedule_down(struct thread *tp __unused)
{
	struct bio *bp;
	off_t excess;
	int error;

	for(;;) {
		bp = g_bioq_first(&g_bio_run_down);
		if (bp == NULL)
			break;
		error = g_io_check(bp);
		if (error) {
			g_io_deliver(bp, error);
			continue;
		}
		/* Truncate requests to the end of providers media. */
		excess = bp->bio_offset + bp->bio_length;
		if (excess > bp->bio_to->mediasize) {
			excess -= bp->bio_to->mediasize;
			bp->bio_length -= excess;
		}
		/* Deliver zero length transfers right here. */
		if (bp->bio_length == 0) {
			g_io_deliver(bp, 0);
			continue;
		}
		bp->bio_to->geom->start(bp);
		if (pace) {
			pace--;
			break;
		}
	}
}

void
g_io_schedule_up(struct thread *tp __unused)
{
	struct bio *bp;
	struct g_consumer *cp;

	for(;;) {
		bp = g_bioq_first(&g_bio_run_up);
		if (bp == NULL)
			break;

		cp = bp->bio_from;
		biodone(bp);
	}
}

void *
g_read_data(struct g_consumer *cp, off_t offset, off_t length, int *error)
{
	struct bio *bp;
	void *ptr;
	int errorc;

	bp = g_new_bio();
	bp->bio_cmd = BIO_READ;
	bp->bio_done = NULL;
	bp->bio_offset = offset;
	bp->bio_length = length;
	ptr = g_malloc(length, 0);
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

	bp = g_new_bio();
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
