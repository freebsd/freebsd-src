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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>

#include <sys/errno.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <sys/devicestat.h>

#include <vm/uma.h>

static struct g_bioq g_bio_run_down;
static struct g_bioq g_bio_run_up;

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
		TAILQ_REMOVE(&bq->bio_queue, bp, bio_queue);
		bq->bio_queue_length--;
	}
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

	bp = uma_zalloc(biozone, M_NOWAIT | M_ZERO);
	return (bp);
}

void
g_destroy_bio(struct bio *bp)
{

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
	return(bp2);
}

void
g_io_init()
{

	g_bioq_init(&g_bio_run_down);
	g_bioq_init(&g_bio_run_up);
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
		/* Noisily reject zero size sectors */
		if (pp->sectorsize == 0) {
			printf("GEOM provider %s has zero sectorsize\n",
			    pp->name);
			return (EDOOFUS);
		}
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
		devstat_start_transaction_bio(cp->stat, bp);
		devstat_start_transaction_bio(pp->stat, bp);
	}
	cp->nstart++;
	pp->nstart++;

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

	bp->bio_bcount = bp->bio_length;
	if (g_collectstats) {
		bp->bio_resid = bp->bio_bcount - bp->bio_completed;
		devstat_end_transaction_bio(cp->stat, bp);
		devstat_end_transaction_bio(pp->stat, bp);
	}
	cp->nend++;
	pp->nend++;

	if (error == ENOMEM) {
		if (bootverbose)
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
	struct mtx mymutex;
 
	bzero(&mymutex, sizeof mymutex);
	mtx_init(&mymutex, "g_xdown", MTX_DEF, 0);

	for(;;) {
		g_bioq_lock(&g_bio_run_down);
		bp = g_bioq_first(&g_bio_run_down);
		if (bp == NULL) {
			msleep(&g_wait_down, &g_bio_run_down.bio_queue_lock,
			    PRIBIO | PDROP, "-", hz/10);
			continue;
		}
		g_bioq_unlock(&g_bio_run_down);
		if (pace > 0) {
			msleep(&error, NULL, PRIBIO, "g_down", hz/10);
			pace--;
		}
		error = g_io_check(bp);
		if (error) {
			g_io_deliver(bp, error);
			continue;
		}
		switch (bp->bio_cmd) {
		case BIO_READ:
		case BIO_WRITE:
		case BIO_DELETE:
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
			break;
		default:
			break;
		}
		mtx_lock(&mymutex);
		bp->bio_to->geom->start(bp);
		mtx_unlock(&mymutex);
	}
}

void
g_io_schedule_up(struct thread *tp __unused)
{
	struct bio *bp;
	struct mtx mymutex;
 
	bzero(&mymutex, sizeof mymutex);
	mtx_init(&mymutex, "g_xup", MTX_DEF, 0);
	for(;;) {
		g_bioq_lock(&g_bio_run_up);
		bp = g_bioq_first(&g_bio_run_up);
		if (bp != NULL) {
			g_bioq_unlock(&g_bio_run_up);
			mtx_lock(&mymutex);
			biodone(bp);
			mtx_unlock(&mymutex);
			continue;
		}
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
