/*-
 * CAM IO Scheduler Interface
 *
 * Copyright (c) 2015 Netflix, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_cam.h"
#include "opt_ddb.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_iosched.h>

#include <ddb/ddb.h>

static MALLOC_DEFINE(M_CAMSCHED, "CAM I/O Scheduler",
    "CAM I/O Scheduler buffers");

/*
 * Default I/O scheduler for FreeBSD. This implementation is just a thin-vineer
 * over the bioq_* interface, with notions of separate calls for normal I/O and
 * for trims.
 */

#ifdef CAM_IOSCHED_DYNAMIC

static int do_netflix_iosched = 1;
TUNABLE_INT("kern.cam.do_netflix_iosched", &do_netflix_iosched);
SYSCTL_INT(_kern_cam, OID_AUTO, do_netflix_iosched, CTLFLAG_RD,
    &do_netflix_iosched, 1,
    "Enable Netflix I/O scheduler optimizations.");

static int alpha_bits = 9;
TUNABLE_INT("kern.cam.iosched_alpha_bits", &alpha_bits);
SYSCTL_INT(_kern_cam, OID_AUTO, iosched_alpha_bits, CTLFLAG_RW,
    &alpha_bits, 1,
    "Bits in EMA's alpha.");



struct iop_stats;
struct cam_iosched_softc;

int iosched_debug = 0;

typedef enum {
	none = 0,				/* No limits */
	queue_depth,			/* Limit how many ops we queue to SIM */
	iops,				/* Limit # of IOPS to the drive */
	bandwidth,			/* Limit bandwidth to the drive */
	limiter_max
} io_limiter;
	
static const char *cam_iosched_limiter_names[] =
    { "none", "queue_depth", "iops", "bandwidth" };

/*
 * Called to initialize the bits of the iop_stats structure relevant to the
 * limiter. Called just after the limiter is set.
 */
typedef int l_init_t(struct iop_stats *);

/*
 * Called every tick.
 */
typedef int l_tick_t(struct iop_stats *);

/*
 * Called to see if the limiter thinks this IOP can be allowed to
 * proceed. If so, the limiter assumes that the while IOP proceeded
 * and makes any accounting of it that's needed.
 */ 
typedef int l_iop_t(struct iop_stats *, struct bio *);

/*
 * Called when an I/O completes so the limiter can updates its
 * accounting. Pending I/Os may complete in any order (even when
 * sent to the hardware at the same time), so the limiter may not
 * make any assumptions other than this I/O has completed. If it
 * returns 1, then xpt_schedule() needs to be called again.
 */
typedef int l_iodone_t(struct iop_stats *, struct bio *);

static l_iop_t cam_iosched_qd_iop;
static l_iop_t cam_iosched_qd_caniop;
static l_iodone_t cam_iosched_qd_iodone;

static l_init_t cam_iosched_iops_init;
static l_tick_t cam_iosched_iops_tick;
static l_iop_t cam_iosched_iops_caniop;
static l_iop_t cam_iosched_iops_iop;

static l_init_t cam_iosched_bw_init;
static l_tick_t cam_iosched_bw_tick;
static l_iop_t cam_iosched_bw_caniop;
static l_iop_t cam_iosched_bw_iop;

struct limswitch 
{
	l_init_t	*l_init;
	l_tick_t	*l_tick;
	l_iop_t		*l_iop;
	l_iop_t		*l_caniop;
	l_iodone_t	*l_iodone;
} limsw[] =
{
	{	/* none */
		.l_init = NULL,
		.l_tick = NULL,
		.l_iop = NULL,
		.l_iodone= NULL,
	},
	{	/* queue_depth */
		.l_init = NULL,
		.l_tick = NULL,
		.l_caniop = cam_iosched_qd_caniop,
		.l_iop = cam_iosched_qd_iop,
		.l_iodone= cam_iosched_qd_iodone,
	},
	{	/* iops */
		.l_init = cam_iosched_iops_init,
		.l_tick = cam_iosched_iops_tick,
		.l_caniop = cam_iosched_iops_caniop,
		.l_iop = cam_iosched_iops_iop,
		.l_iodone= NULL,
	},
	{	/* bandwidth */
		.l_init = cam_iosched_bw_init,
		.l_tick = cam_iosched_bw_tick,
		.l_caniop = cam_iosched_bw_caniop,
		.l_iop = cam_iosched_bw_iop,
		.l_iodone= NULL,
	},
};

struct iop_stats 
{
	/*
	 * sysctl state for this subnode.
	 */
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;

	/*
	 * Information about the current rate limiters, if any
	 */
	io_limiter	limiter;	/* How are I/Os being limited */
	int		min;		/* Low range of limit */
	int		max;		/* High range of limit */
	int		current;	/* Current rate limiter */
	int		l_value1;	/* per-limiter scratch value 1. */
	int		l_value2;	/* per-limiter scratch value 2. */
	

	/*
	 * Debug information about counts of I/Os that have gone through the
	 * scheduler.
	 */
	int		pending;	/* I/Os pending in the hardware */
	int		queued;		/* number currently in the queue */
	int		total;		/* Total for all time -- wraps */
	int		in;		/* number queued all time -- wraps */
	int		out;		/* number completed all time -- wraps */
	
	/*
	 * Statistics on different bits of the process.
	 */
		/* Exp Moving Average, alpha = 1 / (1 << alpha_bits) */
	sbintime_t      ema;
	sbintime_t      emss;		/* Exp Moving sum of the squares */
	sbintime_t      sd;		/* Last computed sd */

	struct cam_iosched_softc *softc;
};


typedef enum {
	set_max = 0,			/* current = max */
	read_latency,			/* Steer read latency by throttling writes */
	cl_max				/* Keep last */
} control_type;

static const char *cam_iosched_control_type_names[] =
    { "set_max", "read_latency" };

struct control_loop
{
	/*
	 * sysctl state for this subnode.
	 */
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;

	sbintime_t	next_steer;		/* Time of next steer */
	sbintime_t	steer_interval;		/* How often do we steer? */
	sbintime_t	lolat;
	sbintime_t	hilat;
	int		alpha;
	control_type	type;			/* What type of control? */
	int		last_count;		/* Last I/O count */

	struct cam_iosched_softc *softc;
};

#endif

struct cam_iosched_softc
{
	struct bio_queue_head bio_queue;
	struct bio_queue_head trim_queue;
				/* scheduler flags < 16, user flags >= 16 */
	uint32_t	flags;
	int		sort_io_queue;
#ifdef CAM_IOSCHED_DYNAMIC
	int		read_bias;		/* Read bias setting */
	int		current_read_bias;	/* Current read bias state */
	int		total_ticks;

	struct bio_queue_head write_queue;
	struct iop_stats read_stats, write_stats, trim_stats;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;

	int		quanta;			/* Number of quanta per second */
	struct callout	ticker;			/* Callout for our quota system */
	struct cam_periph *periph;		/* cam periph associated with this device */
	uint32_t	this_frac;		/* Fraction of a second (1024ths) for this tick */
	sbintime_t	last_time;		/* Last time we ticked */
	struct control_loop cl;
#endif
};

#ifdef CAM_IOSCHED_DYNAMIC
/*
 * helper functions to call the limsw functions.
 */
static int
cam_iosched_limiter_init(struct iop_stats *ios)
{
	int lim = ios->limiter;

	/* maybe this should be a kassert */
	if (lim < none || lim >= limiter_max)
		return EINVAL;

	if (limsw[lim].l_init)
		return limsw[lim].l_init(ios);

	return 0;
}

static int
cam_iosched_limiter_tick(struct iop_stats *ios)
{
	int lim = ios->limiter;

	/* maybe this should be a kassert */
	if (lim < none || lim >= limiter_max)
		return EINVAL;

	if (limsw[lim].l_tick)
		return limsw[lim].l_tick(ios);

	return 0;
}

static int
cam_iosched_limiter_iop(struct iop_stats *ios, struct bio *bp)
{
	int lim = ios->limiter;

	/* maybe this should be a kassert */
	if (lim < none || lim >= limiter_max)
		return EINVAL;

	if (limsw[lim].l_iop)
		return limsw[lim].l_iop(ios, bp);

	return 0;
}

static int
cam_iosched_limiter_caniop(struct iop_stats *ios, struct bio *bp)
{
	int lim = ios->limiter;

	/* maybe this should be a kassert */
	if (lim < none || lim >= limiter_max)
		return EINVAL;

	if (limsw[lim].l_caniop)
		return limsw[lim].l_caniop(ios, bp);

	return 0;
}

static int
cam_iosched_limiter_iodone(struct iop_stats *ios, struct bio *bp)
{
	int lim = ios->limiter;

	/* maybe this should be a kassert */
	if (lim < none || lim >= limiter_max)
		return 0;

	if (limsw[lim].l_iodone)
		return limsw[lim].l_iodone(ios, bp);

	return 0;
}

/*
 * Functions to implement the different kinds of limiters
 */

static int
cam_iosched_qd_iop(struct iop_stats *ios, struct bio *bp)
{
		
	if (ios->current <= 0 || ios->pending < ios->current)
		return 0;

	return EAGAIN;
}

static int
cam_iosched_qd_caniop(struct iop_stats *ios, struct bio *bp)
{
		
	if (ios->current <= 0 || ios->pending < ios->current)
		return 0;

	return EAGAIN;
}

static int
cam_iosched_qd_iodone(struct iop_stats *ios, struct bio *bp)
{
		
	if (ios->current <= 0 || ios->pending != ios->current)
		return 0;

	return 1;
}

static int
cam_iosched_iops_init(struct iop_stats *ios)
{

	ios->l_value1 = ios->current / ios->softc->quanta;
	if (ios->l_value1 <= 0)
		ios->l_value1 = 1;

	return 0;
}

static int
cam_iosched_iops_tick(struct iop_stats *ios)
{

	ios->l_value1 = (int)((ios->current * (uint64_t)ios->softc->this_frac) >> 16);
	if (ios->l_value1 <= 0)
		ios->l_value1 = 1;

	return 0;
}

static int
cam_iosched_iops_caniop(struct iop_stats *ios, struct bio *bp)
{

	/*
	 * So if we have any more IOPs left, allow it,
	 * otherwise wait.
	 */
	if (ios->l_value1 <= 0)
		return EAGAIN;
	return 0;
}

static int
cam_iosched_iops_iop(struct iop_stats *ios, struct bio *bp)
{
	int rv;

	rv = cam_iosched_limiter_caniop(ios, bp);
	if (rv == 0)
		ios->l_value1--;

	return rv;
}

static int
cam_iosched_bw_init(struct iop_stats *ios)
{

	/* ios->current is in kB/s, so scale to bytes */
	ios->l_value1 = ios->current * 1000 / ios->softc->quanta;

	return 0;
}

static int
cam_iosched_bw_tick(struct iop_stats *ios)
{
	int bw;

	/*
	 * If we're in the hole for available quota from
	 * the last time, then add the quantum for this.
	 * If we have any left over from last quantum,
	 * then too bad, that's lost. Also, ios->current
	 * is in kB/s, so scale.
	 *
	 * We also allow up to 4 quanta of credits to
	 * accumulate to deal with burstiness. 4 is extremely
	 * arbitrary.
	 */
	bw = (int)((ios->current * 1000ull * (uint64_t)ios->softc->this_frac) >> 16);
	if (ios->l_value1 < bw * 4)
		ios->l_value1 += bw;

	return 0;
}

static int
cam_iosched_bw_caniop(struct iop_stats *ios, struct bio *bp)
{
	/*
	 * So if we have any more bw quota left, allow it,
	 * otherwise wait. Not, we'll go negative and that's
	 * OK. We'll just get a lettle less next quota.
	 *
	 * Note on going negative: that allows us to process
	 * requests in order better, since we won't allow
	 * shorter reads to get around the long one that we
	 * don't have the quota to do just yet. It also prevents
	 * starvation by being a little more permissive about
	 * what we let through this quantum (to prevent the
	 * starvation), at the cost of getting a little less
	 * next quantum.
	 */
	if (ios->l_value1 <= 0)
		return EAGAIN;


	return 0;
}

static int
cam_iosched_bw_iop(struct iop_stats *ios, struct bio *bp)
{
	int rv;

	rv = cam_iosched_limiter_caniop(ios, bp);
	if (rv == 0)
		ios->l_value1 -= bp->bio_length;

	return rv;
}

static void cam_iosched_cl_maybe_steer(struct control_loop *clp);

static void
cam_iosched_ticker(void *arg)
{
	struct cam_iosched_softc *isc = arg;
	sbintime_t now, delta;

	callout_reset(&isc->ticker, hz / isc->quanta - 1, cam_iosched_ticker, isc);

	now = sbinuptime();
	delta = now - isc->last_time;
	isc->this_frac = (uint32_t)delta >> 16;		/* Note: discards seconds -- should be 0 harmless if not */
	isc->last_time = now;

	cam_iosched_cl_maybe_steer(&isc->cl);

	cam_iosched_limiter_tick(&isc->read_stats);
	cam_iosched_limiter_tick(&isc->write_stats);
	cam_iosched_limiter_tick(&isc->trim_stats);

	cam_iosched_schedule(isc, isc->periph);

	isc->total_ticks++;
}


static void
cam_iosched_cl_init(struct control_loop *clp, struct cam_iosched_softc *isc)
{

	clp->next_steer = sbinuptime();
	clp->softc = isc;
	clp->steer_interval = SBT_1S * 5;	/* Let's start out steering every 5s */
	clp->lolat = 5 * SBT_1MS;
	clp->hilat = 15 * SBT_1MS;
	clp->alpha = 20;			/* Alpha == gain. 20 = .2 */
	clp->type = set_max;
}

static void
cam_iosched_cl_maybe_steer(struct control_loop *clp)
{
	struct cam_iosched_softc *isc;
	sbintime_t now, lat;
	int old;

	isc = clp->softc;
	now = isc->last_time;
	if (now < clp->next_steer)
		return;

	clp->next_steer = now + clp->steer_interval;
	switch (clp->type) {
	case set_max:
		if (isc->write_stats.current != isc->write_stats.max)
			printf("Steering write from %d kBps to %d kBps\n",
			    isc->write_stats.current, isc->write_stats.max);
		isc->read_stats.current = isc->read_stats.max;
		isc->write_stats.current = isc->write_stats.max;
		isc->trim_stats.current = isc->trim_stats.max;
		break;
	case read_latency:
		old = isc->write_stats.current;
		lat = isc->read_stats.ema;
		/*
		 * Simple PLL-like engine. Since we're steering to a range for
		 * the SP (set point) that makes things a little more
		 * complicated. In addition, we're not directly controlling our
		 * PV (process variable), the read latency, but instead are
		 * manipulating the write bandwidth limit for our MV
		 * (manipulation variable), analysis of this code gets a bit
		 * messy. Also, the MV is a very noisy control surface for read
		 * latency since it is affected by many hidden processes inside
		 * the device which change how responsive read latency will be
		 * in reaction to changes in write bandwidth. Unlike the classic
		 * boiler control PLL. this may result in over-steering while
		 * the SSD takes its time to react to the new, lower load. This
		 * is why we use a relatively low alpha of between .1 and .25 to
		 * compensate for this effect. At .1, it takes ~22 steering
		 * intervals to back off by a factor of 10. At .2 it only takes
		 * ~10. At .25 it only takes ~8. However some preliminary data
		 * from the SSD drives suggests a reasponse time in 10's of
		 * seconds before latency drops regardless of the new write
		 * rate. Careful observation will be reqiured to tune this
		 * effectively.
		 *
		 * Also, when there's no read traffic, we jack up the write
		 * limit too regardless of the last read latency.  10 is
		 * somewhat arbitrary.
		 */
		if (lat < clp->lolat || isc->read_stats.total - clp->last_count < 10)
			isc->write_stats.current = isc->write_stats.current *
			    (100 + clp->alpha) / 100;	/* Scale up */
		else if (lat > clp->hilat)
			isc->write_stats.current = isc->write_stats.current *
			    (100 - clp->alpha) / 100;	/* Scale down */
		clp->last_count = isc->read_stats.total;

		/*
		 * Even if we don't steer, per se, enforce the min/max limits as
		 * those may have changed.
		 */
		if (isc->write_stats.current < isc->write_stats.min)
			isc->write_stats.current = isc->write_stats.min;
		if (isc->write_stats.current > isc->write_stats.max)
			isc->write_stats.current = isc->write_stats.max;
		if (old != isc->write_stats.current && 	iosched_debug)
			printf("Steering write from %d kBps to %d kBps due to latency of %jdms\n",
			    old, isc->write_stats.current,
			    (uintmax_t)((uint64_t)1000000 * (uint32_t)lat) >> 32);
		break;
	case cl_max:
		break;
	}
}
#endif

			/* Trim or similar currently pending completion */
#define CAM_IOSCHED_FLAG_TRIM_ACTIVE	(1ul << 0)
			/* Callout active, and needs to be torn down */
#define CAM_IOSCHED_FLAG_CALLOUT_ACTIVE (1ul << 1)

			/* Periph drivers set these flags to indicate work */
#define CAM_IOSCHED_FLAG_WORK_FLAGS	((0xffffu) << 16)

#ifdef CAM_IOSCHED_DYNAMIC
static void
cam_iosched_io_metric_update(struct cam_iosched_softc *isc,
    sbintime_t sim_latency, int cmd, size_t size);
#endif

static inline int
cam_iosched_has_flagged_work(struct cam_iosched_softc *isc)
{
	return !!(isc->flags & CAM_IOSCHED_FLAG_WORK_FLAGS);
}

static inline int
cam_iosched_has_io(struct cam_iosched_softc *isc)
{
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_netflix_iosched) {
		struct bio *rbp = bioq_first(&isc->bio_queue);
		struct bio *wbp = bioq_first(&isc->write_queue);
		int can_write = wbp != NULL &&
		    cam_iosched_limiter_caniop(&isc->write_stats, wbp) == 0;
		int can_read = rbp != NULL &&
		    cam_iosched_limiter_caniop(&isc->read_stats, rbp) == 0;
		if (iosched_debug > 2) {
			printf("can write %d: pending_writes %d max_writes %d\n", can_write, isc->write_stats.pending, isc->write_stats.max);
			printf("can read %d: read_stats.pending %d max_reads %d\n", can_read, isc->read_stats.pending, isc->read_stats.max);
			printf("Queued reads %d writes %d\n", isc->read_stats.queued, isc->write_stats.queued);
		}
		return can_read || can_write;
	}
#endif
	return bioq_first(&isc->bio_queue) != NULL;
}

static inline int
cam_iosched_has_more_trim(struct cam_iosched_softc *isc)
{
	return !(isc->flags & CAM_IOSCHED_FLAG_TRIM_ACTIVE) &&
	    bioq_first(&isc->trim_queue);
}

#define cam_iosched_sort_queue(isc)	((isc)->sort_io_queue >= 0 ?	\
    (isc)->sort_io_queue : cam_sort_io_queues)


static inline int
cam_iosched_has_work(struct cam_iosched_softc *isc)
{
#ifdef CAM_IOSCHED_DYNAMIC
	if (iosched_debug > 2)
		printf("has work: %d %d %d\n", cam_iosched_has_io(isc),
		    cam_iosched_has_more_trim(isc),
		    cam_iosched_has_flagged_work(isc));
#endif

	return cam_iosched_has_io(isc) ||
		cam_iosched_has_more_trim(isc) ||
		cam_iosched_has_flagged_work(isc);
}

#ifdef CAM_IOSCHED_DYNAMIC
static void
cam_iosched_iop_stats_init(struct cam_iosched_softc *isc, struct iop_stats *ios)
{

	ios->limiter = none;
	cam_iosched_limiter_init(ios);
	ios->in = 0;
	ios->max = 300000;
	ios->min = 1;
	ios->out = 0;
	ios->pending = 0;
	ios->queued = 0;
	ios->total = 0;
	ios->ema = 0;
	ios->emss = 0;
	ios->sd = 0;
	ios->softc = isc;
}

static int
cam_iosched_limiter_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	struct iop_stats *ios;
	struct cam_iosched_softc *isc;
	int value, i, error, cantick;
	const char *p;
	
	ios = arg1;
	isc = ios->softc;
	value = ios->limiter;
	if (value < none || value >= limiter_max)
		p = "UNKNOWN";
	else
		p = cam_iosched_limiter_names[value];
	
	strlcpy(buf, p, sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return error;

	cam_periph_lock(isc->periph);

	for (i = none; i < limiter_max; i++) {
		if (strcmp(buf, cam_iosched_limiter_names[i]) != 0)
			continue;
		ios->limiter = i;
		error = cam_iosched_limiter_init(ios);
		if (error != 0) {
			ios->limiter = value;
			cam_periph_unlock(isc->periph);
			return error;
		}
		cantick = !!limsw[isc->read_stats.limiter].l_tick +
		    !!limsw[isc->write_stats.limiter].l_tick +
		    !!limsw[isc->trim_stats.limiter].l_tick +
		    1;	/* Control loop requires it */
		if (isc->flags & CAM_IOSCHED_FLAG_CALLOUT_ACTIVE) {
			if (cantick == 0) {
				callout_stop(&isc->ticker);
				isc->flags &= ~CAM_IOSCHED_FLAG_CALLOUT_ACTIVE;
			}
		} else {
			if (cantick != 0) {
				callout_reset(&isc->ticker, hz / isc->quanta - 1, cam_iosched_ticker, isc);
				isc->flags |= CAM_IOSCHED_FLAG_CALLOUT_ACTIVE;
			}
		}

		cam_periph_unlock(isc->periph);
		return 0;
	}

	cam_periph_unlock(isc->periph);
	return EINVAL;
}

static int
cam_iosched_control_type_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	struct control_loop *clp;
	struct cam_iosched_softc *isc;
	int value, i, error;
	const char *p;
	
	clp = arg1;
	isc = clp->softc;
	value = clp->type;
	if (value < none || value >= cl_max)
		p = "UNKNOWN";
	else
		p = cam_iosched_control_type_names[value];
	
	strlcpy(buf, p, sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return error;

	for (i = set_max; i < cl_max; i++) {
		if (strcmp(buf, cam_iosched_control_type_names[i]) != 0)
			continue;
		cam_periph_lock(isc->periph);
		clp->type = i;
		cam_periph_unlock(isc->periph);
		return 0;
	}

	return EINVAL;
}

static int
cam_iosched_sbintime_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	sbintime_t value;
	int error;
	uint64_t us;
	
	value = *(sbintime_t *)arg1;
	us = (uint64_t)value / SBT_1US;
	snprintf(buf, sizeof(buf), "%ju", (intmax_t)us);
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return error;
	us = strtoul(buf, NULL, 10);
	if (us == 0)
		return EINVAL;
	*(sbintime_t *)arg1 = us * SBT_1US;
	return 0;
}

static void
cam_iosched_iop_stats_sysctl_init(struct cam_iosched_softc *isc, struct iop_stats *ios, char *name)
{
	struct sysctl_oid_list *n;
	struct sysctl_ctx_list *ctx;

	ios->sysctl_tree = SYSCTL_ADD_NODE(&isc->sysctl_ctx,
	    SYSCTL_CHILDREN(isc->sysctl_tree), OID_AUTO, name,
	    CTLFLAG_RD, 0, name);
	n = SYSCTL_CHILDREN(ios->sysctl_tree);
	ctx = &ios->sysctl_ctx;

	SYSCTL_ADD_UQUAD(ctx, n,
	    OID_AUTO, "ema", CTLFLAG_RD,
	    &ios->ema,
	    "Fast Exponentially Weighted Moving Average");
	SYSCTL_ADD_UQUAD(ctx, n,
	    OID_AUTO, "emss", CTLFLAG_RD,
	    &ios->emss,
	    "Fast Exponentially Weighted Moving Sum of Squares (maybe wrong)");
	SYSCTL_ADD_UQUAD(ctx, n,
	    OID_AUTO, "sd", CTLFLAG_RD,
	    &ios->sd,
	    "Estimated SD for fast ema (may be wrong)");

	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "pending", CTLFLAG_RD,
	    &ios->pending, 0,
	    "Instantaneous # of pending transactions");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "count", CTLFLAG_RD,
	    &ios->total, 0,
	    "# of transactions submitted to hardware");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "queued", CTLFLAG_RD,
	    &ios->queued, 0,
	    "# of transactions in the queue");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "in", CTLFLAG_RD,
	    &ios->in, 0,
	    "# of transactions queued to driver");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "out", CTLFLAG_RD,
	    &ios->out, 0,
	    "# of transactions completed");

	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "limiter", CTLTYPE_STRING | CTLFLAG_RW,
	    ios, 0, cam_iosched_limiter_sysctl, "A",
	    "Current limiting type.");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "min", CTLFLAG_RW,
	    &ios->min, 0,
	    "min resource");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "max", CTLFLAG_RW,
	    &ios->max, 0,
	    "max resource");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "current", CTLFLAG_RW,
	    &ios->current, 0,
	    "current resource");

}

static void
cam_iosched_iop_stats_fini(struct iop_stats *ios)
{
	if (ios->sysctl_tree)
		if (sysctl_ctx_free(&ios->sysctl_ctx) != 0)
			printf("can't remove iosched sysctl stats context\n");
}

static void
cam_iosched_cl_sysctl_init(struct cam_iosched_softc *isc)
{
	struct sysctl_oid_list *n;
	struct sysctl_ctx_list *ctx;
	struct control_loop *clp;
	
	clp = &isc->cl;
	clp->sysctl_tree = SYSCTL_ADD_NODE(&isc->sysctl_ctx,
	    SYSCTL_CHILDREN(isc->sysctl_tree), OID_AUTO, "control",
	    CTLFLAG_RD, 0, "Control loop info");
	n = SYSCTL_CHILDREN(clp->sysctl_tree);
	ctx = &clp->sysctl_ctx;

	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "type", CTLTYPE_STRING | CTLFLAG_RW,
	    clp, 0, cam_iosched_control_type_sysctl, "A",
	    "Control loop algorithm");
	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "steer_interval", CTLTYPE_STRING | CTLFLAG_RW,
	    &clp->steer_interval, 0, cam_iosched_sbintime_sysctl, "A",
	    "How often to steer (in us)");
	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "lolat", CTLTYPE_STRING | CTLFLAG_RW,
	    &clp->lolat, 0, cam_iosched_sbintime_sysctl, "A",
	    "Low water mark for Latency (in us)");
	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "hilat", CTLTYPE_STRING | CTLFLAG_RW,
	    &clp->hilat, 0, cam_iosched_sbintime_sysctl, "A",
	    "Hi water mark for Latency (in us)");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "alpha", CTLFLAG_RW,
	    &clp->alpha, 0,
	    "Alpha for PLL (x100) aka gain");
}

static void
cam_iosched_cl_sysctl_fini(struct control_loop *clp)
{
	if (clp->sysctl_tree)
		if (sysctl_ctx_free(&clp->sysctl_ctx) != 0)
			printf("can't remove iosched sysctl control loop context\n");
}
#endif
	    
/*
 * Allocate the iosched structure. This also insulates callers from knowing
 * sizeof struct cam_iosched_softc.
 */
int
cam_iosched_init(struct cam_iosched_softc **iscp, struct cam_periph *periph)
{

	*iscp = malloc(sizeof(**iscp), M_CAMSCHED, M_NOWAIT | M_ZERO);
	if (*iscp == NULL)
		return ENOMEM;
#ifdef CAM_IOSCHED_DYNAMIC
	if (iosched_debug)
		printf("CAM IOSCHEDULER Allocating entry at %p\n", *iscp);
#endif
	(*iscp)->sort_io_queue = -1;
	bioq_init(&(*iscp)->bio_queue);
	bioq_init(&(*iscp)->trim_queue);
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_netflix_iosched) {
		bioq_init(&(*iscp)->write_queue);
		(*iscp)->read_bias = 100;
		(*iscp)->current_read_bias = 100;
		(*iscp)->quanta = 200;
		cam_iosched_iop_stats_init(*iscp, &(*iscp)->read_stats);
		cam_iosched_iop_stats_init(*iscp, &(*iscp)->write_stats);
		cam_iosched_iop_stats_init(*iscp, &(*iscp)->trim_stats);
		(*iscp)->trim_stats.max = 1;	/* Trims are special: one at a time for now */
		(*iscp)->last_time = sbinuptime();
		callout_init_mtx(&(*iscp)->ticker, cam_periph_mtx(periph), 0);
		(*iscp)->periph = periph;
		cam_iosched_cl_init(&(*iscp)->cl, *iscp);
		callout_reset(&(*iscp)->ticker, hz / (*iscp)->quanta - 1, cam_iosched_ticker, *iscp);
		(*iscp)->flags |= CAM_IOSCHED_FLAG_CALLOUT_ACTIVE;
	}
#endif

	return 0;
}

/*
 * Reclaim all used resources. This assumes that other folks have
 * drained the requests in the hardware. Maybe an unwise assumption.
 */
void
cam_iosched_fini(struct cam_iosched_softc *isc)
{
	if (isc) {
		cam_iosched_flush(isc, NULL, ENXIO);
#ifdef CAM_IOSCHED_DYNAMIC
		cam_iosched_iop_stats_fini(&isc->read_stats);
		cam_iosched_iop_stats_fini(&isc->write_stats);
		cam_iosched_iop_stats_fini(&isc->trim_stats);
		cam_iosched_cl_sysctl_fini(&isc->cl);
		if (isc->sysctl_tree)
			if (sysctl_ctx_free(&isc->sysctl_ctx) != 0)
				printf("can't remove iosched sysctl stats context\n");
		if (isc->flags & CAM_IOSCHED_FLAG_CALLOUT_ACTIVE) {
			callout_drain(&isc->ticker);
			isc->flags &= ~ CAM_IOSCHED_FLAG_CALLOUT_ACTIVE;
		}
			
#endif
		free(isc, M_CAMSCHED);
	}
}

/*
 * After we're sure we're attaching a device, go ahead and add
 * hooks for any sysctl we may wish to honor.
 */
void cam_iosched_sysctl_init(struct cam_iosched_softc *isc,
    struct sysctl_ctx_list *ctx, struct sysctl_oid *node)
{
#ifdef CAM_IOSCHED_DYNAMIC
	struct sysctl_oid_list *n;
#endif

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
		OID_AUTO, "sort_io_queue", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&isc->sort_io_queue, 0,
		"Sort IO queue to try and optimise disk access patterns");

#ifdef CAM_IOSCHED_DYNAMIC
	if (!do_netflix_iosched)
		return;

	isc->sysctl_tree = SYSCTL_ADD_NODE(&isc->sysctl_ctx,
	    SYSCTL_CHILDREN(node), OID_AUTO, "iosched",
	    CTLFLAG_RD, 0, "I/O scheduler statistics");
	n = SYSCTL_CHILDREN(isc->sysctl_tree);
	ctx = &isc->sysctl_ctx;

	cam_iosched_iop_stats_sysctl_init(isc, &isc->read_stats, "read");
	cam_iosched_iop_stats_sysctl_init(isc, &isc->write_stats, "write");
	cam_iosched_iop_stats_sysctl_init(isc, &isc->trim_stats, "trim");
	cam_iosched_cl_sysctl_init(isc);

	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "read_bias", CTLFLAG_RW,
	    &isc->read_bias, 100,
	    "How biased towards read should we be independent of limits");

	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "quanta", CTLFLAG_RW,
	    &isc->quanta, 200,
	    "How many quanta per second do we slice the I/O up into");

	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "total_ticks", CTLFLAG_RD,
	    &isc->total_ticks, 0,
	    "Total number of ticks we've done");
#endif
}

/*
 * Flush outstanding I/O. Consumers of this library don't know all the
 * queues we may keep, so this allows all I/O to be flushed in one
 * convenient call.
 */
void
cam_iosched_flush(struct cam_iosched_softc *isc, struct devstat *stp, int err)
{
	bioq_flush(&isc->bio_queue, stp, err);
	bioq_flush(&isc->trim_queue, stp, err);
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_netflix_iosched)
		bioq_flush(&isc->write_queue, stp, err);
#endif
}

#ifdef CAM_IOSCHED_DYNAMIC
static struct bio *
cam_iosched_get_write(struct cam_iosched_softc *isc)
{
	struct bio *bp;

	/*
	 * We control the write rate by controlling how many requests we send
	 * down to the drive at any one time. Fewer requests limits the
	 * effects of both starvation when the requests take a while and write
	 * amplification when each request is causing more than one write to
	 * the NAND media. Limiting the queue depth like this will also limit
	 * the write throughput and give and reads that want to compete to
	 * compete unfairly.
	 */
	bp = bioq_first(&isc->write_queue);
	if (bp == NULL) {
		if (iosched_debug > 3)
			printf("No writes present in write_queue\n");
		return NULL;
	}

	/*
	 * If pending read, prefer that based on current read bias
	 * setting.
	 */
	if (bioq_first(&isc->bio_queue) && isc->current_read_bias) {
		if (iosched_debug)
			printf("Reads present and current_read_bias is %d queued writes %d queued reads %d\n", isc->current_read_bias, isc->write_stats.queued, isc->read_stats.queued);
		isc->current_read_bias--;
		return NULL;
	}

	/*
	 * See if our current limiter allows this I/O.
	 */
	if (cam_iosched_limiter_iop(&isc->write_stats, bp) != 0) {
		if (iosched_debug)
			printf("Can't write because limiter says no.\n");
		return NULL;
	}

	/*
	 * Let's do this: We've passed all the gates and we're a go
	 * to schedule the I/O in the SIM.
	 */
	isc->current_read_bias = isc->read_bias;
	bioq_remove(&isc->write_queue, bp);
	if (bp->bio_cmd == BIO_WRITE) {
		isc->write_stats.queued--;
		isc->write_stats.total++;
		isc->write_stats.pending++;
	}
	if (iosched_debug > 9)
		printf("HWQ : %p %#x\n", bp, bp->bio_cmd);
	return bp;
}
#endif

/*
 * Put back a trim that you weren't able to actually schedule this time.
 */
void
cam_iosched_put_back_trim(struct cam_iosched_softc *isc, struct bio *bp)
{
	bioq_insert_head(&isc->trim_queue, bp);
#ifdef CAM_IOSCHED_DYNAMIC
	isc->trim_stats.queued++;
	isc->trim_stats.total--;		/* since we put it back, don't double count */
	isc->trim_stats.pending--;
#endif
}

/*
 * gets the next trim from the trim queue.
 *
 * Assumes we're called with the periph lock held.  It removes this
 * trim from the queue and the device must explicitly reinstert it
 * should the need arise.
 */
struct bio *
cam_iosched_next_trim(struct cam_iosched_softc *isc)
{
	struct bio *bp;

	bp  = bioq_first(&isc->trim_queue);
	if (bp == NULL)
		return NULL;
	bioq_remove(&isc->trim_queue, bp);
#ifdef CAM_IOSCHED_DYNAMIC
	isc->trim_stats.queued--;
	isc->trim_stats.total++;
	isc->trim_stats.pending++;
#endif
	return bp;
}

/*
 * gets the an available trim from the trim queue, if there's no trim
 * already pending. It removes this trim from the queue and the device
 * must explicitly reinstert it should the need arise.
 *
 * Assumes we're called with the periph lock held.
 */
struct bio *
cam_iosched_get_trim(struct cam_iosched_softc *isc)
{

	if (!cam_iosched_has_more_trim(isc))
		return NULL;

	return cam_iosched_next_trim(isc);
}

/*
 * Determine what the next bit of work to do is for the periph. The
 * default implementation looks to see if we have trims to do, but no
 * trims outstanding. If so, we do that. Otherwise we see if we have
 * other work. If we do, then we do that. Otherwise why were we called?
 */
struct bio *
cam_iosched_next_bio(struct cam_iosched_softc *isc)
{
	struct bio *bp;

	/*
	 * See if we have a trim that can be scheduled. We can only send one
	 * at a time down, so this takes that into account.
	 *
	 * XXX newer TRIM commands are queueable. Revisit this when we
	 * implement them.
	 */
	if ((bp = cam_iosched_get_trim(isc)) != NULL)
		return bp;

#ifdef CAM_IOSCHED_DYNAMIC
	/*
	 * See if we have any pending writes, and room in the queue for them,
	 * and if so, those are next.
	 */
	if (do_netflix_iosched) {
		if ((bp = cam_iosched_get_write(isc)) != NULL)
			return bp;
	}
#endif

	/*
	 * next, see if there's other, normal I/O waiting. If so return that.
	 */
	if ((bp = bioq_first(&isc->bio_queue)) == NULL)
		return NULL;

#ifdef CAM_IOSCHED_DYNAMIC
	/*
	 * For the netflix scheduler, bio_queue is only for reads, so enforce
	 * the limits here. Enforce only for reads.
	 */
	if (do_netflix_iosched) {
		if (bp->bio_cmd == BIO_READ &&
		    cam_iosched_limiter_iop(&isc->read_stats, bp) != 0)
			return NULL;
	}
#endif
	bioq_remove(&isc->bio_queue, bp);
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_netflix_iosched) {
		if (bp->bio_cmd == BIO_READ) {
			isc->read_stats.queued--;
			isc->read_stats.total++;
			isc->read_stats.pending++;
		} else
			printf("Found bio_cmd = %#x\n", bp->bio_cmd);
	}
	if (iosched_debug > 9)
		printf("HWQ : %p %#x\n", bp, bp->bio_cmd);
#endif
	return bp;
}
	
/*
 * Driver has been given some work to do by the block layer. Tell the
 * scheduler about it and have it queue the work up. The scheduler module
 * will then return the currently most useful bit of work later, possibly
 * deferring work for various reasons.
 */
void
cam_iosched_queue_work(struct cam_iosched_softc *isc, struct bio *bp)
{

	/*
	 * Put all trims on the trim queue sorted, since we know
	 * that the collapsing code requires this. Otherwise put
	 * the work on the bio queue.
	 */
	if (bp->bio_cmd == BIO_DELETE) {
		bioq_disksort(&isc->trim_queue, bp);
#ifdef CAM_IOSCHED_DYNAMIC
		isc->trim_stats.in++;
		isc->trim_stats.queued++;
#endif
	}
#ifdef CAM_IOSCHED_DYNAMIC
	else if (do_netflix_iosched &&
	    (bp->bio_cmd == BIO_WRITE || bp->bio_cmd == BIO_FLUSH)) {
		if (cam_iosched_sort_queue(isc))
			bioq_disksort(&isc->write_queue, bp);
		else
			bioq_insert_tail(&isc->write_queue, bp);
		if (iosched_debug > 9)
			printf("Qw  : %p %#x\n", bp, bp->bio_cmd);
		if (bp->bio_cmd == BIO_WRITE) {
			isc->write_stats.in++;
			isc->write_stats.queued++;
		}
	}
#endif
	else {
		if (cam_iosched_sort_queue(isc))
			bioq_disksort(&isc->bio_queue, bp);
		else
			bioq_insert_tail(&isc->bio_queue, bp);
#ifdef CAM_IOSCHED_DYNAMIC
		if (iosched_debug > 9)
			printf("Qr  : %p %#x\n", bp, bp->bio_cmd);
		if (bp->bio_cmd == BIO_READ) {
			isc->read_stats.in++;
			isc->read_stats.queued++;
		} else if (bp->bio_cmd == BIO_WRITE) {
			isc->write_stats.in++;
			isc->write_stats.queued++;
		}
#endif
	}
}

/* 
 * If we have work, get it scheduled. Called with the periph lock held.
 */
void
cam_iosched_schedule(struct cam_iosched_softc *isc, struct cam_periph *periph)
{

	if (cam_iosched_has_work(isc))
		xpt_schedule(periph, CAM_PRIORITY_NORMAL);
}

/*
 * Complete a trim request
 */
void
cam_iosched_trim_done(struct cam_iosched_softc *isc)
{

	isc->flags &= ~CAM_IOSCHED_FLAG_TRIM_ACTIVE;
}

/*
 * Complete a bio. Called before we release the ccb with xpt_release_ccb so we
 * might use notes in the ccb for statistics.
 */
int
cam_iosched_bio_complete(struct cam_iosched_softc *isc, struct bio *bp,
    union ccb *done_ccb)
{
	int retval = 0;
#ifdef CAM_IOSCHED_DYNAMIC
	if (!do_netflix_iosched)
		return retval;

	if (iosched_debug > 10)
		printf("done: %p %#x\n", bp, bp->bio_cmd);
	if (bp->bio_cmd == BIO_WRITE) {
		retval = cam_iosched_limiter_iodone(&isc->write_stats, bp);
		isc->write_stats.out++;
		isc->write_stats.pending--;
	} else if (bp->bio_cmd == BIO_READ) {
		retval = cam_iosched_limiter_iodone(&isc->read_stats, bp);
		isc->read_stats.out++;
		isc->read_stats.pending--;
	} else if (bp->bio_cmd == BIO_DELETE) {
		isc->trim_stats.out++;
		isc->trim_stats.pending--;
	} else if (bp->bio_cmd != BIO_FLUSH) {
		if (iosched_debug)
			printf("Completing command with bio_cmd == %#x\n", bp->bio_cmd);
	}

	if (!(bp->bio_flags & BIO_ERROR))
		cam_iosched_io_metric_update(isc, done_ccb->ccb_h.qos.sim_data,
		    bp->bio_cmd, bp->bio_bcount);
#endif
	return retval;
}

/*
 * Tell the io scheduler that you've pushed a trim down into the sim.
 * xxx better place for this?
 */
void
cam_iosched_submit_trim(struct cam_iosched_softc *isc)
{

	isc->flags |= CAM_IOSCHED_FLAG_TRIM_ACTIVE;
}

/*
 * Change the sorting policy hint for I/O transactions for this device.
 */
void
cam_iosched_set_sort_queue(struct cam_iosched_softc *isc, int val)
{

	isc->sort_io_queue = val;
}

int
cam_iosched_has_work_flags(struct cam_iosched_softc *isc, uint32_t flags)
{
	return isc->flags & flags;
}

void
cam_iosched_set_work_flags(struct cam_iosched_softc *isc, uint32_t flags)
{
	isc->flags |= flags;
}

void
cam_iosched_clr_work_flags(struct cam_iosched_softc *isc, uint32_t flags)
{
	isc->flags &= ~flags;
}

#ifdef CAM_IOSCHED_DYNAMIC
/*
 * After the method presented in Jack Crenshaw's 1998 article "Integer
 * Suqare Roots," reprinted at
 * http://www.embedded.com/electronics-blogs/programmer-s-toolbox/4219659/Integer-Square-Roots
 * and well worth the read. Briefly, we find the power of 4 that's the
 * largest smaller than val. We then check each smaller power of 4 to
 * see if val is still bigger. The right shifts at each step divide
 * the result by 2 which after successive application winds up
 * accumulating the right answer. It could also have been accumulated
 * using a separate root counter, but this code is smaller and faster
 * than that method. This method is also integer size invariant.
 * It returns floor(sqrt((float)val)), or the larget integer less than
 * or equal to the square root.
 */
static uint64_t
isqrt64(uint64_t val)
{
	uint64_t res = 0;
	uint64_t bit = 1ULL << (sizeof(uint64_t) * NBBY - 2);

	/*
	 * Find the largest power of 4 smaller than val.
	 */
	while (bit > val)
		bit >>= 2;

	/*
	 * Accumulate the answer, one bit at a time (we keep moving
	 * them over since 2 is the square root of 4 and we test
	 * powers of 4). We accumulate where we find the bit, but
	 * the successive shifts land the bit in the right place
	 * by the end.
	 */
	while (bit != 0) {
		if (val >= res + bit) {
			val -= res + bit;
			res = (res >> 1) + bit;
		} else
			res >>= 1;
		bit >>= 2;
	}
	
	return res;
}

/*
 * a and b are 32.32 fixed point stored in a 64-bit word.
 * Let al and bl be the .32 part of a and b.
 * Let ah and bh be the 32 part of a and b.
 * R is the radix and is 1 << 32
 *
 * a * b
 * (ah + al / R) * (bh + bl / R)
 * ah * bh + (al * bh + ah * bl) / R + al * bl / R^2
 *
 * After multiplicaiton, we have to renormalize by multiply by
 * R, so we wind up with
 *	ah * bh * R + al * bh + ah * bl + al * bl / R
 * which turns out to be a very nice way to compute this value
 * so long as ah and bh are < 65536 there's no loss of high bits
 * and the low order bits are below the threshold of caring for
 * this application.
 */
static uint64_t
mul(uint64_t a, uint64_t b)
{
	uint64_t al, ah, bl, bh;
	al = a & 0xffffffff;
	ah = a >> 32;
	bl = b & 0xffffffff;
	bh = b >> 32;
	return ((ah * bh) << 32) + al * bh + ah * bl + ((al * bl) >> 32);
}

static void
cam_iosched_update(struct iop_stats *iop, sbintime_t sim_latency)
{
	sbintime_t y, yy;
	uint64_t var;

	/* 
	 * Classic expoentially decaying average with a tiny alpha
	 * (2 ^ -alpha_bits). For more info see the NIST statistical
	 * handbook.
	 *
	 * ema_t = y_t * alpha + ema_t-1 * (1 - alpha)
	 * alpha = 1 / (1 << alpha_bits)
	 *
	 * Since alpha is a power of two, we can compute this w/o any mult or
	 * division.
	 */
	y = sim_latency;
	iop->ema = (y + (iop->ema << alpha_bits) - iop->ema) >> alpha_bits;

	yy = mul(y, y);
	iop->emss = (yy + (iop->emss << alpha_bits) - iop->emss) >> alpha_bits;

	/*
         * s_1 = sum of data
	 * s_2 = sum of data * data
	 * ema ~ mean (or s_1 / N)
	 * emss ~ s_2 / N
	 *
	 * sd = sqrt((N * s_2 - s_1 ^ 2) / (N * (N - 1)))
	 * sd = sqrt((N * s_2 / N * (N - 1)) - (s_1 ^ 2 / (N * (N - 1))))
	 *
	 * N ~ 2 / alpha - 1
	 * alpha < 1 / 16 (typically much less)
	 * N > 31 --> N large so N * (N - 1) is approx N * N
	 *
	 * substituting and rearranging:
	 * sd ~ sqrt(s_2 / N - (s_1 / N) ^ 2)
	 *    ~ sqrt(emss - ema ^ 2);
	 * which is the formula used here to get a decent estimate of sd which
	 * we use to detect outliers. Note that when first starting up, it
	 * takes a while for emss sum of squares estimator to converge on a
	 * good value.  during this time, it can be less than ema^2. We
	 * compute a sd of 0 in that case, and ignore outliers.
	 */
	var = iop->emss - mul(iop->ema, iop->ema);
	iop->sd = (int64_t)var < 0 ? 0 : isqrt64(var);
}

#ifdef CAM_IOSCHED_DYNAMIC
static void
cam_iosched_io_metric_update(struct cam_iosched_softc *isc,
    sbintime_t sim_latency, int cmd, size_t size)
{
	/* xxx Do we need to scale based on the size of the I/O ? */
	switch (cmd) {
	case BIO_READ:
		cam_iosched_update(&isc->read_stats, sim_latency);
		break;
	case BIO_WRITE:
		cam_iosched_update(&isc->write_stats, sim_latency);
		break;
	case BIO_DELETE:
		cam_iosched_update(&isc->trim_stats, sim_latency);
		break;
	default:
		break;
	}
}
#endif

#ifdef DDB
static int biolen(struct bio_queue_head *bq)
{
	int i = 0;
	struct bio *bp;

	TAILQ_FOREACH(bp, &bq->queue, bio_queue) {
		i++;
	}
	return i;
}

/*
 * Show the internal state of the I/O scheduler.
 */
DB_SHOW_COMMAND(iosched, cam_iosched_db_show)
{
	struct cam_iosched_softc *isc;

	if (!have_addr) {
		db_printf("Need addr\n");
		return;
	}
	isc = (struct cam_iosched_softc *)addr;
	db_printf("pending_reads:     %d\n", isc->read_stats.pending);
	db_printf("min_reads:         %d\n", isc->read_stats.min);
	db_printf("max_reads:         %d\n", isc->read_stats.max);
	db_printf("reads:             %d\n", isc->read_stats.total);
	db_printf("in_reads:          %d\n", isc->read_stats.in);
	db_printf("out_reads:         %d\n", isc->read_stats.out);
	db_printf("queued_reads:      %d\n", isc->read_stats.queued);
	db_printf("Current Q len      %d\n", biolen(&isc->bio_queue));
	db_printf("pending_writes:    %d\n", isc->write_stats.pending);
	db_printf("min_writes:        %d\n", isc->write_stats.min);
	db_printf("max_writes:        %d\n", isc->write_stats.max);
	db_printf("writes:            %d\n", isc->write_stats.total);
	db_printf("in_writes:         %d\n", isc->write_stats.in);
	db_printf("out_writes:        %d\n", isc->write_stats.out);
	db_printf("queued_writes:     %d\n", isc->write_stats.queued);
	db_printf("Current Q len      %d\n", biolen(&isc->write_queue));
	db_printf("pending_trims:     %d\n", isc->trim_stats.pending);
	db_printf("min_trims:         %d\n", isc->trim_stats.min);
	db_printf("max_trims:         %d\n", isc->trim_stats.max);
	db_printf("trims:             %d\n", isc->trim_stats.total);
	db_printf("in_trims:          %d\n", isc->trim_stats.in);
	db_printf("out_trims:         %d\n", isc->trim_stats.out);
	db_printf("queued_trims:      %d\n", isc->trim_stats.queued);
	db_printf("Current Q len      %d\n", biolen(&isc->trim_queue));
	db_printf("read_bias:         %d\n", isc->read_bias);
	db_printf("current_read_bias: %d\n", isc->current_read_bias);
	db_printf("Trim active?       %s\n", 
	    (isc->flags & CAM_IOSCHED_FLAG_TRIM_ACTIVE) ? "yes" : "no");
}
#endif
#endif
