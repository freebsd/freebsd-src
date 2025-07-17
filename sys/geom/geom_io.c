/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/stack.h>
#include <sys/stdarg.h>
#include <sys/sysctl.h>
#include <sys/vmem.h>
#include <machine/stack.h>

#include <sys/errno.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <sys/devicestat.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

#define KTR_GEOM_ENABLED \
    ((KTR_COMPILE & KTR_GEOM) != 0 && (ktr_mask & KTR_GEOM) != 0)

static int	g_io_transient_map_bio(struct bio *bp);

static struct g_bioq g_bio_run_down;
static struct g_bioq g_bio_run_up;

static u_long nomem_count;
static u_long pause_count;

/*
 * Pace is a hint that we've had some trouble recently allocating
 * bios, so we should back off trying to send I/O down the stack
 * a bit to let the problem resolve. When pacing, we also turn
 * off direct dispatch to also reduce memory pressure from I/Os
 * there, at the expxense of some added latency while the memory
 * pressures exist. See g_io_schedule_down() for more details
 * and limitations.
 */
static volatile u_int __read_mostly pace;

static uma_zone_t __read_mostly biozone;

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
	if (KTR_GEOM_ENABLED) {
		struct stack st;

		CTR1(KTR_GEOM, "g_new_bio(): %p", bp);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3);
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
	if (KTR_GEOM_ENABLED) {
		struct stack st;

		CTR1(KTR_GEOM, "g_alloc_bio(): %p", bp);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3);
	}
#endif
	return (bp);
}

void
g_destroy_bio(struct bio *bp)
{
#ifdef KTR
	if (KTR_GEOM_ENABLED) {
		struct stack st;

		CTR1(KTR_GEOM, "g_destroy_bio(): %p", bp);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3);
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
		/*
		 *  BIO_ORDERED flag may be used by disk drivers to enforce
		 *  ordering restrictions, so this flag needs to be cloned.
		 *  BIO_UNMAPPED, BIO_VLIST, and BIO_SWAP should be inherited,
		 *  to properly indicate which way the buffer is passed.
		 *  Other bio flags are not suitable for cloning.
		 */
		bp2->bio_flags = bp->bio_flags &
		    (BIO_ORDERED | BIO_UNMAPPED | BIO_VLIST | BIO_SWAP);
		bp2->bio_length = bp->bio_length;
		bp2->bio_offset = bp->bio_offset;
		bp2->bio_data = bp->bio_data;
		bp2->bio_ma = bp->bio_ma;
		bp2->bio_ma_n = bp->bio_ma_n;
		bp2->bio_ma_offset = bp->bio_ma_offset;
		bp2->bio_attribute = bp->bio_attribute;
		if (bp->bio_cmd == BIO_ZONE)
			bcopy(&bp->bio_zone, &bp2->bio_zone,
			    sizeof(bp->bio_zone));
#if defined(BUF_TRACKING) || defined(FULL_BUF_TRACKING)
		bp2->bio_track_bp = bp->bio_track_bp;
#endif
		bp->bio_children++;
	}
#ifdef KTR
	if (KTR_GEOM_ENABLED) {
		struct stack st;

		CTR2(KTR_GEOM, "g_clone_bio(%p): %p", bp, bp2);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3);
	}
#endif
	return(bp2);
}

struct bio *
g_duplicate_bio(struct bio *bp)
{
	struct bio *bp2;

	bp2 = uma_zalloc(biozone, M_WAITOK | M_ZERO);
	bp2->bio_flags = bp->bio_flags & (BIO_UNMAPPED | BIO_VLIST | BIO_SWAP);
	bp2->bio_parent = bp;
	bp2->bio_cmd = bp->bio_cmd;
	bp2->bio_length = bp->bio_length;
	bp2->bio_offset = bp->bio_offset;
	bp2->bio_data = bp->bio_data;
	bp2->bio_ma = bp->bio_ma;
	bp2->bio_ma_n = bp->bio_ma_n;
	bp2->bio_ma_offset = bp->bio_ma_offset;
	bp2->bio_attribute = bp->bio_attribute;
	bp->bio_children++;
#ifdef KTR
	if (KTR_GEOM_ENABLED) {
		struct stack st;

		CTR2(KTR_GEOM, "g_duplicate_bio(%p): %p", bp, bp2);
		stack_save(&st);
		CTRSTACK(KTR_GEOM, &st, 3);
	}
#endif
	return(bp2);
}

void
g_reset_bio(struct bio *bp)
{

	bzero(bp, sizeof(*bp));
}

void
g_io_init(void)
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
g_io_zonecmd(struct disk_zone_args *zone_args, struct g_consumer *cp)
{
	struct bio *bp;
	int error;

	g_trace(G_T_BIO, "bio_zone(%d)", zone_args->zone_cmd);
	bp = g_alloc_bio();
	bp->bio_cmd = BIO_ZONE;
	bp->bio_done = NULL;
	/*
	 * XXX KDM need to handle report zone data.
	 */
	bcopy(zone_args, &bp->bio_zone, sizeof(*zone_args));
	if (zone_args->zone_cmd == DISK_ZONE_REPORT_ZONES)
		bp->bio_length =
		    zone_args->zone_params.report.entries_allocated *
		    sizeof(struct disk_zone_rep_entry);
	else
		bp->bio_length = 0;

	g_io_request(bp, cp);
	error = biowait(bp, "gzone");
	bcopy(&bp->bio_zone, zone_args, sizeof(*zone_args));
	g_destroy_bio(bp);
	return (error);
}

/*
 * Send a BIO_SPEEDUP down the stack. This is used to tell the lower layers that
 * the upper layers have detected a resource shortage. The lower layers are
 * advised to stop delaying I/O that they might be holding for performance
 * reasons and to schedule it (non-trims) or complete it successfully (trims) as
 * quickly as it can. bio_length is the amount of the shortage.  This call
 * should be non-blocking. bio_resid is used to communicate back if the lower
 * layers couldn't find bio_length worth of I/O to schedule or discard. A length
 * of 0 means to do as much as you can (schedule the h/w queues full, discard
 * all trims). flags are a hint from the upper layers to the lower layers what
 * operation should be done.
 */
int
g_io_speedup(off_t shortage, u_int flags, size_t *resid, struct g_consumer *cp)
{
	struct bio *bp;
	int error;

	KASSERT((flags & (BIO_SPEEDUP_TRIM | BIO_SPEEDUP_WRITE)) != 0,
	    ("Invalid flags passed to g_io_speedup: %#x", flags));
	g_trace(G_T_BIO, "bio_speedup(%s, %jd, %#x)", cp->provider->name,
	    (intmax_t)shortage, flags);
	bp = g_new_bio();
	if (bp == NULL)
		return (ENOMEM);
	bp->bio_cmd = BIO_SPEEDUP;
	bp->bio_length = shortage;
	bp->bio_done = NULL;
	bp->bio_flags |= flags;
	g_io_request(bp, cp);
	error = biowait(bp, "gflush");
	*resid = bp->bio_resid;
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
	bp->bio_flags |= BIO_ORDERED;
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
	off_t excess;
	int error;

	biotrack(bp, __func__);

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
	case BIO_SPEEDUP:
	case BIO_FLUSH:
		if (cp->acw == 0)
			return (EPERM);
		break;
	case BIO_ZONE:
		if ((bp->bio_zone.zone_cmd == DISK_ZONE_REPORT_ZONES) ||
		    (bp->bio_zone.zone_cmd == DISK_ZONE_GET_PARAMS)) {
			if (cp->acr == 0)
				return (EPERM);
		} else if (cp->acw == 0)
			return (EPERM);
		break;
	default:
		return (EPERM);
	}
	/* if provider is marked for error, don't disturb. */
	if (pp->error)
		return (pp->error);
	if (cp->flags & G_CF_ORPHAN)
		return (ENXIO);

	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		/* Zero sectorsize or mediasize is probably a lack of media. */
		if (pp->sectorsize == 0 || pp->mediasize == 0)
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

		/* Truncate requests to the end of providers media. */
		excess = bp->bio_offset + bp->bio_length;
		if (excess > bp->bio_to->mediasize) {
			KASSERT((bp->bio_flags & BIO_UNMAPPED) == 0 ||
			    round_page(bp->bio_ma_offset +
			    bp->bio_length) / PAGE_SIZE == bp->bio_ma_n,
			    ("excess bio %p too short", bp));
			excess -= bp->bio_to->mediasize;
			bp->bio_length -= excess;
			if ((bp->bio_flags & BIO_UNMAPPED) != 0) {
				bp->bio_ma_n = round_page(bp->bio_ma_offset +
				    bp->bio_length) / PAGE_SIZE;
			}
			if (excess > 0)
				CTR3(KTR_GEOM, "g_down truncated bio "
				    "%p provider %s by %d", bp,
				    bp->bio_to->name, excess);
		}

		/* Deliver zero length transfers right here. */
		if (bp->bio_length == 0) {
			CTR2(KTR_GEOM, "g_down terminated 0-length "
			    "bp %p provider %s", bp, bp->bio_to->name);
			return (0);
		}

		if ((bp->bio_flags & BIO_UNMAPPED) != 0 &&
		    (bp->bio_to->flags & G_PF_ACCEPT_UNMAPPED) == 0 &&
		    (bp->bio_cmd == BIO_READ || bp->bio_cmd == BIO_WRITE)) {
			if ((error = g_io_transient_map_bio(bp)) >= 0)
				return (error);
		}
		break;
	default:
		break;
	}
	return (EJUSTRETURN);
}

void
g_io_request(struct bio *bp, struct g_consumer *cp)
{
	struct g_provider *pp;
	int direct, error, first;
	uint8_t cmd;

	biotrack(bp, __func__);

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

	cmd = bp->bio_cmd;
	if (cmd == BIO_READ || cmd == BIO_WRITE || cmd == BIO_GETATTR) {
		KASSERT(bp->bio_data != NULL,
		    ("NULL bp->data in g_io_request(cmd=%hu)", bp->bio_cmd));
	}
	if (cmd == BIO_DELETE || cmd == BIO_FLUSH || cmd == BIO_SPEEDUP) {
		KASSERT(bp->bio_data == NULL,
		    ("non-NULL bp->data in g_io_request(cmd=%hu)",
		    bp->bio_cmd));
	}
	if (cmd == BIO_READ || cmd == BIO_WRITE || cmd == BIO_DELETE) {
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

	if ((g_collectstats & G_STATS_CONSUMERS) != 0 ||
	    ((g_collectstats & G_STATS_PROVIDERS) != 0 && pp->stat != NULL))
		binuptime(&bp->bio_t0);
	else
		getbinuptime(&bp->bio_t0);
	if (g_collectstats & G_STATS_CONSUMERS)
		devstat_start_transaction_bio_t0(cp->stat, bp);
	if (g_collectstats & G_STATS_PROVIDERS)
		devstat_start_transaction_bio_t0(pp->stat, bp);
#ifdef INVARIANTS
	atomic_add_int(&cp->nstart, 1);
#endif

	direct = (cp->flags & G_CF_DIRECT_SEND) != 0 &&
	    (pp->flags & G_PF_DIRECT_RECEIVE) != 0 &&
	    curthread != g_down_td &&
	    ((pp->flags & G_PF_ACCEPT_UNMAPPED) != 0 ||
	    (bp->bio_flags & BIO_UNMAPPED) == 0 || THREAD_CAN_SLEEP()) &&
	    pace == 0;
	if (direct) {
		/* Block direct execution if less then half of stack left. */
		size_t	st, su;
		GET_STACK_USAGE(st, su);
		if (su * 2 > st)
			direct = 0;
	}

	if (direct) {
		error = g_io_check(bp);
		if (error >= 0) {
			CTR3(KTR_GEOM, "g_io_request g_io_check on bp %p "
			    "provider %s returned %d", bp, bp->bio_to->name,
			    error);
			g_io_deliver(bp, error);
			return;
		}
		bp->bio_to->geom->start(bp);
	} else {
		g_bioq_lock(&g_bio_run_down);
		first = TAILQ_EMPTY(&g_bio_run_down.bio_queue);
		TAILQ_INSERT_TAIL(&g_bio_run_down.bio_queue, bp, bio_queue);
		bp->bio_flags |= BIO_ONQUEUE;
		g_bio_run_down.bio_queue_length++;
		g_bioq_unlock(&g_bio_run_down);
		/* Pass it on down. */
		if (first)
			wakeup(&g_wait_down);
	}
}

void
g_io_deliver(struct bio *bp, int error)
{
	struct bintime now;
	struct g_consumer *cp;
	struct g_provider *pp;
	struct mtx *mtxp;
	int direct, first;

	biotrack(bp, __func__);

	KASSERT(bp != NULL, ("NULL bp in g_io_deliver"));
	pp = bp->bio_to;
	KASSERT(pp != NULL, ("NULL bio_to in g_io_deliver"));
	cp = bp->bio_from;
	if (cp == NULL) {
		bp->bio_error = error;
		bp->bio_done(bp);
		return;
	}
	KASSERT(cp != NULL, ("NULL bio_from in g_io_deliver"));
	KASSERT(cp->geom != NULL, ("NULL bio_from->geom in g_io_deliver"));
#ifdef DIAGNOSTIC
	/*
	 * Some classes - GJournal in particular - can modify bio's
	 * private fields while the bio is in transit; G_GEOM_VOLATILE_BIO
	 * flag means it's an expected behaviour for that particular geom.
	 */
	if ((cp->geom->flags & G_GEOM_VOLATILE_BIO) == 0) {
		KASSERT(bp->bio_caller1 == bp->_bio_caller1,
		    ("bio_caller1 used by the provider %s", pp->name));
		KASSERT(bp->bio_caller2 == bp->_bio_caller2,
		    ("bio_caller2 used by the provider %s", pp->name));
		KASSERT(bp->bio_cflags == bp->_bio_cflags,
		    ("bio_cflags used by the provider %s", pp->name));
	}
#endif
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

	direct = (pp->flags & G_PF_DIRECT_SEND) &&
		 (cp->flags & G_CF_DIRECT_RECEIVE) &&
		 curthread != g_up_td;
	if (direct) {
		/* Block direct execution if less then half of stack left. */
		size_t	st, su;
		GET_STACK_USAGE(st, su);
		if (su * 2 > st)
			direct = 0;
	}

	/*
	 * The statistics collection is lockless, as such, but we
	 * can not update one instance of the statistics from more
	 * than one thread at a time, so grab the lock first.
	 */
	if ((g_collectstats & G_STATS_CONSUMERS) != 0 ||
	    ((g_collectstats & G_STATS_PROVIDERS) != 0 && pp->stat != NULL))
		binuptime(&now);
	mtxp = mtx_pool_find(mtxpool_sleep, pp);
	mtx_lock(mtxp);
	if (g_collectstats & G_STATS_PROVIDERS)
		devstat_end_transaction_bio_bt(pp->stat, bp, &now);
	if (g_collectstats & G_STATS_CONSUMERS)
		devstat_end_transaction_bio_bt(cp->stat, bp, &now);
#ifdef INVARIANTS
	cp->nend++;
#endif
	mtx_unlock(mtxp);

	if (error != ENOMEM) {
		bp->bio_error = error;
		if (direct) {
			biodone(bp);
		} else {
			g_bioq_lock(&g_bio_run_up);
			first = TAILQ_EMPTY(&g_bio_run_up.bio_queue);
			TAILQ_INSERT_TAIL(&g_bio_run_up.bio_queue, bp, bio_queue);
			bp->bio_flags |= BIO_ONQUEUE;
			g_bio_run_up.bio_queue_length++;
			g_bioq_unlock(&g_bio_run_up);
			if (first)
				wakeup(&g_wait_up);
		}
		return;
	}

	if (bootverbose)
		printf("ENOMEM %p on %p(%s)\n", bp, pp, pp->name);
	atomic_add_long(&nomem_count, 1);	/* Rare event, but no locks held */
	bp->bio_children = 0;
	bp->bio_inbed = 0;
	bp->bio_driver1 = NULL;
	bp->bio_driver2 = NULL;
	bp->bio_pflags = 0;
	g_io_request(bp, cp);
	pace = 1;
	return;
}

SYSCTL_DECL(_kern_geom);

static long transient_maps;
SYSCTL_LONG(_kern_geom, OID_AUTO, transient_maps, CTLFLAG_RD,
    &transient_maps, 0,
    "Total count of the transient mapping requests");
u_int transient_map_retries = 10;
SYSCTL_UINT(_kern_geom, OID_AUTO, transient_map_retries, CTLFLAG_RW,
    &transient_map_retries, 0,
    "Max count of retries used before giving up on creating transient map");
int transient_map_hard_failures;
SYSCTL_INT(_kern_geom, OID_AUTO, transient_map_hard_failures, CTLFLAG_RD,
    &transient_map_hard_failures, 0,
    "Failures to establish the transient mapping due to retry attempts "
    "exhausted");
int transient_map_soft_failures;
SYSCTL_INT(_kern_geom, OID_AUTO, transient_map_soft_failures, CTLFLAG_RD,
    &transient_map_soft_failures, 0,
    "Count of retried failures to establish the transient mapping");
int inflight_transient_maps;
SYSCTL_INT(_kern_geom, OID_AUTO, inflight_transient_maps, CTLFLAG_RD,
    &inflight_transient_maps, 0,
    "Current count of the active transient maps");
SYSCTL_ULONG(_kern_geom, OID_AUTO, nomem_count, CTLFLAG_RD,
    &nomem_count, 0,
    "Total count of requests completed with status of ENOMEM");
SYSCTL_ULONG(_kern_geom, OID_AUTO, pause_count, CTLFLAG_RD,
    &pause_count, 0,
    "Total count of requests stalled due to low memory in g_down");

static int
g_io_transient_map_bio(struct bio *bp)
{
	vm_offset_t addr;
	long size;
	u_int retried;

	KASSERT(unmapped_buf_allowed, ("unmapped disabled"));

	size = round_page(bp->bio_ma_offset + bp->bio_length);
	KASSERT(size / PAGE_SIZE == bp->bio_ma_n, ("Bio too short %p", bp));
	addr = 0;
	retried = 0;
	atomic_add_long(&transient_maps, 1);
retry:
	if (vmem_alloc(transient_arena, size, M_BESTFIT | M_NOWAIT, &addr)) {
		if (transient_map_retries != 0 &&
		    retried >= transient_map_retries) {
			CTR2(KTR_GEOM, "g_down cannot map bp %p provider %s",
			    bp, bp->bio_to->name);
			atomic_add_int(&transient_map_hard_failures, 1);
			return (EDEADLK/* XXXKIB */);
		} else {
			/*
			 * Naive attempt to quisce the I/O to get more
			 * in-flight requests completed and defragment
			 * the transient_arena.
			 */
			CTR3(KTR_GEOM, "g_down retrymap bp %p provider %s r %d",
			    bp, bp->bio_to->name, retried);
			pause("g_d_tra", hz / 10);
			retried++;
			atomic_add_int(&transient_map_soft_failures, 1);
			goto retry;
		}
	}
	atomic_add_int(&inflight_transient_maps, 1);
	pmap_qenter((vm_offset_t)addr, bp->bio_ma, OFF_TO_IDX(size));
	bp->bio_data = (caddr_t)addr + bp->bio_ma_offset;
	bp->bio_flags |= BIO_TRANSIENT_MAPPING;
	bp->bio_flags &= ~BIO_UNMAPPED;
	return (EJUSTRETURN);
}

void
g_io_schedule_down(struct thread *tp __unused)
{
	struct bio *bp;
	int error;

	for(;;) {
		g_bioq_lock(&g_bio_run_down);
		bp = g_bioq_first(&g_bio_run_down);
		if (bp == NULL) {
			CTR0(KTR_GEOM, "g_down going to sleep");
			msleep(&g_wait_down, &g_bio_run_down.bio_queue_lock,
			    PRIBIO | PDROP, "-", 0);
			continue;
		}
		CTR0(KTR_GEOM, "g_down has work to do");
		g_bioq_unlock(&g_bio_run_down);
		biotrack(bp, __func__);
		if (pace != 0) {
			/*
			 * There has been at least one memory allocation failure
			 * since the last I/O completed. Pause 1ms to give the
			 * system a chance to free up memory. Pause time is not
			 * scaled to the number of I/O failures since they tend
			 * to cluster and the number is not predictive of how
			 * long a pause is needed.
			 *
			 * Older versions had a longer pause, which limited the
			 * IOPS to 10, which prolonged memory shortages that could
			 * be alleviated by I/O completing since it eliminated
			 * direct dispatch as well.
			 *
			 * XXX This pacing is really lame. It needs to be solved
			 * by other methods. This is OK only because the worst
			 * case scenario is so rare. In the worst case scenario
			 * all memory is tied up waiting for I/O to complete
			 * which can never happen since we can't allocate bios
			 * for that I/O.
			 */
			CTR0(KTR_GEOM, "g_down pacing self");
			pause_count++;		/* g_down has only one thread */
			pause_sbt("g_down", SBT_1MS, 0, 0);
			pace = 0;
		}
		CTR2(KTR_GEOM, "g_down processing bp %p provider %s", bp,
		    bp->bio_to->name);
		error = g_io_check(bp);
		if (error >= 0) {
			CTR3(KTR_GEOM, "g_down g_io_check on bp %p provider "
			    "%s returned %d", bp, bp->bio_to->name, error);
			g_io_deliver(bp, error);
			continue;
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
g_io_schedule_up(struct thread *tp __unused)
{
	struct bio *bp;

	for(;;) {
		g_bioq_lock(&g_bio_run_up);
		bp = g_bioq_first(&g_bio_run_up);
		if (bp == NULL) {
			CTR0(KTR_GEOM, "g_up going to sleep");
			msleep(&g_wait_up, &g_bio_run_up.bio_queue_lock,
			    PRIBIO | PDROP, "-", 0);
			continue;
		}
		g_bioq_unlock(&g_bio_run_up);
		THREAD_NO_SLEEPING();
		CTR4(KTR_GEOM, "g_up biodone bp %p provider %s off "
		    "%jd len %ld", bp, bp->bio_to->name,
		    bp->bio_offset, bp->bio_length);
		biodone(bp);
		THREAD_SLEEPING_OK();
	}
}

void *
g_read_data(struct g_consumer *cp, off_t offset, off_t length, int *error)
{
	struct bio *bp;
	void *ptr;
	int errorc;

	KASSERT(length > 0 && length >= cp->provider->sectorsize &&
	    length <= maxphys, ("g_read_data(): invalid length %jd",
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
	if (errorc == 0 && bp->bio_completed != length)
		errorc = EIO;
	if (error != NULL)
		*error = errorc;
	g_destroy_bio(bp);
	if (errorc) {
		g_free(ptr);
		ptr = NULL;
	}
	return (ptr);
}

/*
 * A read function for use by ffs_sbget when used by GEOM-layer routines.
 */
int
g_use_g_read_data(void *devfd, off_t loc, void **bufp, int size)
{
	struct g_consumer *cp;

	KASSERT(*bufp == NULL,
	    ("g_use_g_read_data: non-NULL *bufp %p\n", *bufp));

	cp = (struct g_consumer *)devfd;
	/*
	 * Take care not to issue an invalid I/O request. The offset of
	 * the superblock candidate must be multiples of the provider's
	 * sector size, otherwise an FFS can't exist on the provider
	 * anyway.
	 */
	if (loc % cp->provider->sectorsize != 0)
		return (ENOENT);
	*bufp = g_read_data(cp, loc, size, NULL);
	if (*bufp == NULL)
		return (ENOENT);
	return (0);
}

int
g_write_data(struct g_consumer *cp, off_t offset, void *ptr, off_t length)
{
	struct bio *bp;
	int error;

	KASSERT(length > 0 && length >= cp->provider->sectorsize &&
	    length <= maxphys, ("g_write_data(): invalid length %jd",
	    (intmax_t)length));

	bp = g_alloc_bio();
	bp->bio_cmd = BIO_WRITE;
	bp->bio_done = NULL;
	bp->bio_offset = offset;
	bp->bio_length = length;
	bp->bio_data = ptr;
	g_io_request(bp, cp);
	error = biowait(bp, "gwrite");
	if (error == 0 && bp->bio_completed != length)
		error = EIO;
	g_destroy_bio(bp);
	return (error);
}

/*
 * A write function for use by ffs_sbput when used by GEOM-layer routines.
 */
int
g_use_g_write_data(void *devfd, off_t loc, void *buf, int size)
{

	return (g_write_data((struct g_consumer *)devfd, loc, buf, size));
}

int
g_delete_data(struct g_consumer *cp, off_t offset, off_t length)
{
	struct bio *bp;
	int error;

	KASSERT(length > 0 && length >= cp->provider->sectorsize,
	    ("g_delete_data(): invalid length %jd", (intmax_t)length));

	bp = g_alloc_bio();
	bp->bio_cmd = BIO_DELETE;
	bp->bio_done = NULL;
	bp->bio_offset = offset;
	bp->bio_length = length;
	bp->bio_data = NULL;
	g_io_request(bp, cp);
	error = biowait(bp, "gdelete");
	if (error == 0 && bp->bio_completed != length)
		error = EIO;
	g_destroy_bio(bp);
	return (error);
}

void
g_print_bio(const char *prefix, const struct bio *bp, const char *fmtsuffix,
    ...)
{
#ifndef PRINTF_BUFR_SIZE
#define PRINTF_BUFR_SIZE 64
#endif
	char bufr[PRINTF_BUFR_SIZE];
	struct sbuf sb, *sbp __unused;
	va_list ap;

	sbp = sbuf_new(&sb, bufr, sizeof(bufr), SBUF_FIXEDLEN);
	KASSERT(sbp != NULL, ("sbuf_new misused?"));

	sbuf_set_drain(&sb, sbuf_printf_drain, NULL);

	sbuf_cat(&sb, prefix);
	g_format_bio(&sb, bp);

	va_start(ap, fmtsuffix);
	sbuf_vprintf(&sb, fmtsuffix, ap);
	va_end(ap);

	sbuf_nl_terminate(&sb);

	sbuf_finish(&sb);
	sbuf_delete(&sb);
}

void
g_format_bio(struct sbuf *sb, const struct bio *bp)
{
	const char *pname, *cmd = NULL;

	if (bp->bio_to != NULL)
		pname = bp->bio_to->name;
	else if (bp->bio_parent != NULL && bp->bio_parent->bio_to != NULL)
		pname = bp->bio_parent->bio_to->name;
	else
		pname = "[unknown]";

	switch (bp->bio_cmd) {
	case BIO_GETATTR:
		cmd = "GETATTR";
		sbuf_printf(sb, "%s[%s(attr=%s)]", pname, cmd,
		    bp->bio_attribute);
		return;
	case BIO_FLUSH:
		cmd = "FLUSH";
		sbuf_printf(sb, "%s[%s]", pname, cmd);
		return;
	case BIO_ZONE: {
		char *subcmd = NULL;
		cmd = "ZONE";
		switch (bp->bio_zone.zone_cmd) {
		case DISK_ZONE_OPEN:
			subcmd = "OPEN";
			break;
		case DISK_ZONE_CLOSE:
			subcmd = "CLOSE";
			break;
		case DISK_ZONE_FINISH:
			subcmd = "FINISH";
			break;
		case DISK_ZONE_RWP:
			subcmd = "RWP";
			break;
		case DISK_ZONE_REPORT_ZONES:
			subcmd = "REPORT ZONES";
			break;
		case DISK_ZONE_GET_PARAMS:
			subcmd = "GET PARAMS";
			break;
		default:
			subcmd = "UNKNOWN";
			break;
		}
		sbuf_printf(sb, "%s[%s,%s]", pname, cmd, subcmd);
		return;
	}
	case BIO_READ:
		cmd = "READ";
		break;
	case BIO_WRITE:
		cmd = "WRITE";
		break;
	case BIO_DELETE:
		cmd = "DELETE";
		break;
	default:
		cmd = "UNKNOWN";
		sbuf_printf(sb, "%s[%s()]", pname, cmd);
		return;
	}
	sbuf_printf(sb, "%s[%s(offset=%jd, length=%jd)]", pname, cmd,
	    (intmax_t)bp->bio_offset, (intmax_t)bp->bio_length);
}
