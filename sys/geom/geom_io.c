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

static struct g_bioq g_bio_run_down;
static struct g_bioq g_bio_run_up;
static struct g_bioq g_bio_idle;

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
	g_trace(G_T_BIO, "g_new_bio() = %p", bp);
	return (bp);
}

void
g_destroy_bio(struct bio *bp)
{

	g_trace(G_T_BIO, "g_destroy_bio(%p)", bp);
	bzero(bp, sizeof *bp);
	g_bioq_enqueue_tail(bp, &g_bio_idle);
}

struct bio *
g_clone_bio(struct bio *bp)
{
	struct bio *bp2;

	bp2 = g_new_bio();
	if (bp2 != NULL) {
		bp2->bio_linkage = bp;
		bp2->bio_cmd = bp->bio_cmd;
		bp2->bio_length = bp->bio_length;
		bp2->bio_offset = bp->bio_offset;
		bp2->bio_data = bp->bio_data;
		bp2->bio_attribute = bp->bio_attribute;
	}
	g_trace(G_T_BIO, "g_clone_bio(%p) = %p", bp, bp2);
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
	do {
		bp = g_new_bio();
		bp->bio_cmd = BIO_SETATTR;
		bp->bio_done = NULL;
		bp->bio_attribute = attr;
		bp->bio_length = len;
		bp->bio_data = ptr;
		g_io_request(bp, cp);
		error = biowait(bp, "gsetattr");
		g_destroy_bio(bp);
		if (error == EBUSY)
			tsleep(&error, 0, "setattr_busy", hz);
	} while(error == EBUSY);
	return (error);
}


int
g_io_getattr(const char *attr, struct g_consumer *cp, int *len, void *ptr)
{
	struct bio *bp;
	int error;

	g_trace(G_T_BIO, "bio_getattr(%s)", attr);
	do {
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
		if (error == EBUSY)
			tsleep(&error, 0, "getattr_busy", hz);

	} while(error == EBUSY);
	return (error);
}

void
g_io_fail(struct bio *bp, int error)
{

	bp->bio_error = error;

	g_trace(G_T_BIO,
	    "bio_fail(%p) from %p(%s) to %p(%s) cmd %d error %d\n",
	    bp, bp->bio_from, bp->bio_from->geom->name,
	    bp->bio_to, bp->bio_to->name, bp->bio_cmd, bp->bio_error);
	g_io_deliver(bp);
	return;
}

void
g_io_request(struct bio *bp, struct g_consumer *cp)
{
	int error;
	off_t excess;

	KASSERT(cp != NULL, ("bio_request on thin air"));
	error = 0;
	bp->bio_from = cp;
	bp->bio_to = cp->provider;
	bp->bio_error = 0;
	bp->bio_completed = 0;

	/* begin_stats(&bp->stats); */

	atomic_add_int(&cp->biocount, 1);
	/* Fail on unattached consumers */
	if (bp->bio_to == NULL)
		return (g_io_fail(bp, ENXIO));
	/* Fail if access doesn't allow operation */
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_GETATTR:
		if (cp->acr == 0)
			return (g_io_fail(bp, EPERM));
		break;
	case BIO_WRITE:
	case BIO_DELETE:
		if (cp->acw == 0)
			return (g_io_fail(bp, EPERM));
		break;
	case BIO_SETATTR:
		if ((cp->acw == 0) || (cp->ace == 0))
			return (g_io_fail(bp, EPERM));
		break;
	default:
		return (g_io_fail(bp, EPERM));
	}
	/* if provider is marked for error, don't disturb. */
	if (bp->bio_to->error)
		return (g_io_fail(bp, bp->bio_to->error));
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		/* Reject requests past the end of media. */
		if (bp->bio_offset > bp->bio_to->mediasize)
			return (g_io_fail(bp, EIO));
		/* Truncate requests to the end of providers media. */
		excess = bp->bio_offset + bp->bio_length;
		if (excess > bp->bio_to->mediasize) {
			excess -= bp->bio_to->mediasize;
			bp->bio_length -= excess;
		}
		/* Deliver zero length transfers right here. */
		if (bp->bio_length == 0) 
			return (g_io_deliver(bp));
		break;
	default:
		break;
	}
	/* Pass it on down. */
	g_trace(G_T_BIO, "bio_request(%p) from %p(%s) to %p(%s) cmd %d",
	    bp, bp->bio_from, bp->bio_from->geom->name,
	    bp->bio_to, bp->bio_to->name, bp->bio_cmd);
	g_bioq_enqueue_tail(bp, &g_bio_run_down);
	wakeup(&g_wait_down);
}

void
g_io_deliver(struct bio *bp)
{

	g_trace(G_T_BIO,
	    "g_io_deliver(%p) from %p(%s) to %p(%s) cmd %d error %d",
	    bp, bp->bio_from, bp->bio_from->geom->name,
	    bp->bio_to, bp->bio_to->name, bp->bio_cmd, bp->bio_error);
	/* finish_stats(&bp->stats); */

	g_bioq_enqueue_tail(bp, &g_bio_run_up);

	wakeup(&g_wait_up);
}

void
g_io_schedule_down(struct thread *tp __unused)
{
	struct bio *bp;

	for(;;) {
		bp = g_bioq_first(&g_bio_run_down);
		if (bp == NULL)
			break;
		bp->bio_to->geom->start(bp);
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

		atomic_add_int(&cp->biocount, -1);
		biodone(bp);
	}
}

void *
g_read_data(struct g_consumer *cp, off_t offset, off_t length, int *error)
{
	struct bio *bp;
	void *ptr;
	int errorc;

        do {
		bp = g_new_bio();
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
		if (errorc == EBUSY)
			tsleep(&errorc, 0, "g_read_data_busy", hz);
        } while (errorc == EBUSY);
	return (ptr);
}
