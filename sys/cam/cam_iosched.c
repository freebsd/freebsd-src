/*-
 * CAM IO Scheduler Interface
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h>
#include <cam/cam_iosched.h>

#include <ddb/ddb.h>

#include <geom/geom_disk.h>

static MALLOC_DEFINE(M_CAMSCHED, "CAM I/O Scheduler",
    "CAM I/O Scheduler buffers");

static SYSCTL_NODE(_kern_cam, OID_AUTO, iosched, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "CAM I/O Scheduler parameters");

/*
 * Default I/O scheduler for FreeBSD. This implementation is just a thin-vineer
 * over the bioq_* interface, with notions of separate calls for normal I/O and
 * for trims.
 *
 * When CAM_IOSCHED_DYNAMIC is defined, the scheduler is enhanced to dynamically
 * steer the rate of one type of traffic to help other types of traffic (eg
 * limit writes when read latency deteriorates on SSDs).
 */

#ifdef CAM_IOSCHED_DYNAMIC

static bool do_dynamic_iosched = true;
SYSCTL_BOOL(_kern_cam_iosched, OID_AUTO, dynamic, CTLFLAG_RDTUN,
    &do_dynamic_iosched, 1,
    "Enable Dynamic I/O scheduler optimizations.");

/*
 * For an EMA, with an alpha of alpha, we know
 * 	alpha = 2 / (N + 1)
 * or
 * 	N = 1 + (2 / alpha)
 * where N is the number of samples that 86% of the current
 * EMA is derived from.
 *
 * So we invent[*] alpha_bits:
 *	alpha_bits = -log_2(alpha)
 *	alpha = 2^-alpha_bits
 * So
 *	N = 1 + 2^(alpha_bits + 1)
 *
 * The default 9 gives a 1025 lookback for 86% of the data.
 * For a brief intro: https://en.wikipedia.org/wiki/Moving_average
 *
 * [*] Steal from the load average code and many other places.
 * Note: See computation of EMA and EMVAR for acceptable ranges of alpha.
 */
static int alpha_bits = 9;
SYSCTL_INT(_kern_cam_iosched, OID_AUTO, alpha_bits, CTLFLAG_RWTUN,
    &alpha_bits, 1,
    "Bits in EMA's alpha.");

/*
 * Different parameters for the buckets of latency we keep track of. These are all
 * published read-only since at present they are compile time constants.
 *
 * Bucket base is the upper bounds of the first latency bucket. It's currently 20us.
 * With 20 buckets (see below), that leads to a geometric progression with a max size
 * of 5.2s which is safeily larger than 1s to help diagnose extreme outliers better.
 */
#ifndef BUCKET_BASE
#define BUCKET_BASE ((SBT_1S / 50000) + 1)	/* 20us */
#endif
static sbintime_t bucket_base = BUCKET_BASE;
SYSCTL_SBINTIME_USEC(_kern_cam_iosched, OID_AUTO, bucket_base_us, CTLFLAG_RD,
    &bucket_base,
    "Size of the smallest latency bucket");

/*
 * Bucket ratio is the geometric progression for the bucket. For a bucket b_n
 * the size of bucket b_n+1 is b_n * bucket_ratio / 100.
 */
static int bucket_ratio = 200;	/* Rather hard coded at the moment */
SYSCTL_INT(_kern_cam_iosched, OID_AUTO, bucket_ratio, CTLFLAG_RD,
    &bucket_ratio, 200,
    "Latency Bucket Ratio for geometric progression.");

/*
 * Number of total buckets. Starting at BUCKET_BASE, each one is a power of 2.
 */
#ifndef LAT_BUCKETS
#define LAT_BUCKETS 20	/* < 20us < 40us ... < 2^(n-1)*20us >= 2^(n-1)*20us */
#endif
static int lat_buckets = LAT_BUCKETS;
SYSCTL_INT(_kern_cam_iosched, OID_AUTO, buckets, CTLFLAG_RD,
    &lat_buckets, LAT_BUCKETS,
    "Total number of latency buckets published");

/*
 * Read bias: how many reads do we favor before scheduling a write
 * when we have a choice.
 */
static int default_read_bias = 0;
SYSCTL_INT(_kern_cam_iosched, OID_AUTO, read_bias, CTLFLAG_RWTUN,
    &default_read_bias, 0,
    "Default read bias for new devices.");

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
 * proceed. If so, the limiter assumes that the IOP proceeded
 * and makes any accounting of it that's needed.
 */
typedef int l_iop_t(struct iop_stats *, struct bio *);

/*
 * Called when an I/O completes so the limiter can update its
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

struct limswitch {
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

struct iop_stats {
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
	int		errs;		/* Number of I/Os completed with error --  wraps */

	/*
	 * Statistics on different bits of the process.
	 */
		/* Exp Moving Average, see alpha_bits for more details */
	sbintime_t      ema;
	sbintime_t      emvar;
	sbintime_t      sd;		/* Last computed sd */

	uint64_t	too_long;	/* Number of I/Os greater than bad lat threshold */
	sbintime_t	bad_latency;	/* Latency threshold */

	uint32_t	state_flags;
#define IOP_RATE_LIMITED		1u

	uint64_t	latencies[LAT_BUCKETS];

	struct cam_iosched_softc *softc;
};

typedef enum {
	set_max = 0,			/* current = max */
	read_latency,			/* Steer read latency by throttling writes */
	cl_max				/* Keep last */
} control_type;

static const char *cam_iosched_control_type_names[] =
    { "set_max", "read_latency" };

struct control_loop {
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

struct cam_iosched_softc {
	struct bio_queue_head bio_queue;
	struct bio_queue_head trim_queue;
	const struct disk *disk;
	cam_iosched_schedule_t schedfnc;
				/* scheduler flags < 16, user flags >= 16 */
	uint32_t	flags;
	int		sort_io_queue;
	int		trim_goal;		/* # of trims to queue before sending */
	int		trim_ticks;		/* Max ticks to hold trims */
	int		last_trim_tick;		/* Last 'tick' time ld a trim */
	int		queued_trims;		/* Number of trims in the queue */
#ifdef CAM_IOSCHED_DYNAMIC
	int		read_bias;		/* Read bias setting */
	int		current_read_bias;	/* Current read bias state */
	int		total_ticks;
	int		load;			/* EMA of 'load average' of disk / 2^16 */

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
	sbintime_t	max_lat;		/* when != 0, if iop latency > max_lat, call max_lat_fcn */
	cam_iosched_latfcn_t	latfcn;
	void		*latarg;
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
	ios->l_value2 = 0;

	return 0;
}

static int
cam_iosched_iops_tick(struct iop_stats *ios)
{
	int new_ios;

	/*
	 * Allow at least one IO per tick until all
	 * the IOs for this interval have been spent.
	 */
	new_ios = (int)((ios->current * (uint64_t)ios->softc->this_frac) >> 16);
	if (new_ios < 1 && ios->l_value2 < ios->current) {
		new_ios = 1;
		ios->l_value2++;
	}

	/*
	 * If this a new accounting interval, discard any "unspent" ios
	 * granted in the previous interval.  Otherwise add the new ios to
	 * the previously granted ones that haven't been spent yet.
	 */
	if ((ios->softc->total_ticks % ios->softc->quanta) == 0) {
		ios->l_value1 = new_ios;
		ios->l_value2 = 1;
	} else {
		ios->l_value1 += new_ios;
	}

	return 0;
}

static int
cam_iosched_iops_caniop(struct iop_stats *ios, struct bio *bp)
{

	/*
	 * So if we have any more IOPs left, allow it,
	 * otherwise wait. If current iops is 0, treat that
	 * as unlimited as a failsafe.
	 */
	if (ios->current > 0 && ios->l_value1 <= 0)
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
	 * otherwise wait. Note, we'll go negative and that's
	 * OK. We'll just get a little less next quota.
	 *
	 * Note on going negative: that allows us to process
	 * requests in order better, since we won't allow
	 * shorter reads to get around the long one that we
	 * don't have the quota to do just yet. It also prevents
	 * starvation by being a little more permissive about
	 * what we let through this quantum (to prevent the
	 * starvation), at the cost of getting a little less
	 * next quantum.
	 *
	 * Also note that if the current limit is <= 0,
	 * we treat it as unlimited as a failsafe.
	 */
	if (ios->current > 0 && ios->l_value1 <= 0)
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
	int pending;

	callout_reset(&isc->ticker, hz / isc->quanta, cam_iosched_ticker, isc);

	now = sbinuptime();
	delta = now - isc->last_time;
	isc->this_frac = (uint32_t)delta >> 16;		/* Note: discards seconds -- should be 0 harmless if not */
	isc->last_time = now;

	cam_iosched_cl_maybe_steer(&isc->cl);

	cam_iosched_limiter_tick(&isc->read_stats);
	cam_iosched_limiter_tick(&isc->write_stats);
	cam_iosched_limiter_tick(&isc->trim_stats);

	isc->schedfnc(isc->periph);

	/*
	 * isc->load is an EMA of the pending I/Os at each tick. The number of
	 * pending I/Os is the sum of the I/Os queued to the hardware, and those
	 * in the software queue that could be queued to the hardware if there
	 * were slots.
	 *
	 * ios_stats.pending is a count of requests in the SIM right now for
	 * each of these types of I/O. So the total pending count is the sum of
	 * these I/Os and the sum of the queued I/Os still in the software queue
	 * for those operations that aren't being rate limited at the moment.
	 *
	 * The reason for the rate limiting bit is because those I/Os
	 * aren't part of the software queued load (since we could
	 * give them to hardware, but choose not to).
	 *
	 * Note: due to a bug in counting pending TRIM in the device, we
	 * don't include them in this count. We count each BIO_DELETE in
	 * the pending count, but the periph drivers collapse them down
	 * into one TRIM command. That one trim command gets the completion
	 * so the counts get off.
	 */
	pending = isc->read_stats.pending + isc->write_stats.pending /* + isc->trim_stats.pending */;
	pending += !!(isc->read_stats.state_flags & IOP_RATE_LIMITED) * isc->read_stats.queued +
	    !!(isc->write_stats.state_flags & IOP_RATE_LIMITED) * isc->write_stats.queued /* +
	    !!(isc->trim_stats.state_flags & IOP_RATE_LIMITED) * isc->trim_stats.queued */ ;
	pending <<= 16;
	pending /= isc->periph->path->device->ccbq.total_openings;

	isc->load = (pending + (isc->load << 13) - isc->load) >> 13; /* see above: 13 -> 16139 / 200/s = ~81s ~1 minute */

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
		 * rate. Careful observation will be required to tune this
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
			printf("Steering write from %d kBps to %d kBps due to latency of %jdus\n",
			    old, isc->write_stats.current,
			    (uintmax_t)((uint64_t)1000000 * (uint32_t)lat) >> 32);
		break;
	case cl_max:
		break;
	}
}
#endif

/*
 * Trim or similar currently pending completion. Should only be set for
 * those drivers wishing only one Trim active at a time.
 */
#define CAM_IOSCHED_FLAG_TRIM_ACTIVE	(1ul << 0)
			/* Callout active, and needs to be torn down */
#define CAM_IOSCHED_FLAG_CALLOUT_ACTIVE (1ul << 1)

			/* Periph drivers set these flags to indicate work */
#define CAM_IOSCHED_FLAG_WORK_FLAGS	((0xffffu) << 16)

#ifdef CAM_IOSCHED_DYNAMIC
static void
cam_iosched_io_metric_update(struct cam_iosched_softc *isc,
    sbintime_t sim_latency, const struct bio *bp);
#endif

static inline bool
cam_iosched_has_flagged_work(struct cam_iosched_softc *isc)
{
	return !!(isc->flags & CAM_IOSCHED_FLAG_WORK_FLAGS);
}

static inline bool
cam_iosched_has_io(struct cam_iosched_softc *isc)
{
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_dynamic_iosched) {
		struct bio *rbp = bioq_first(&isc->bio_queue);
		struct bio *wbp = bioq_first(&isc->write_queue);
		bool can_write = wbp != NULL &&
		    cam_iosched_limiter_caniop(&isc->write_stats, wbp) == 0;
		bool can_read = rbp != NULL &&
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

static inline bool
cam_iosched_has_more_trim(struct cam_iosched_softc *isc)
{
	struct bio *bp;

	bp = bioq_first(&isc->trim_queue);
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_dynamic_iosched) {
		/*
		 * If we're limiting trims, then defer action on trims
		 * for a bit.
		 */
		if (bp == NULL || cam_iosched_limiter_caniop(&isc->trim_stats, bp) != 0)
			return false;
	}
#endif

	/*
	 * If we've set a trim_goal, then if we exceed that allow trims
	 * to be passed back to the driver. If we've also set a tick timeout
	 * allow trims back to the driver. Otherwise, don't allow trims yet.
	 */
	if (isc->trim_goal > 0) {
		if (isc->queued_trims >= isc->trim_goal)
			return true;
		if (isc->queued_trims > 0 &&
		    isc->trim_ticks > 0 &&
		    ticks - isc->last_trim_tick > isc->trim_ticks)
			return true;
		return false;
	}

	/* NB: Should perhaps have a max trim active independent of I/O limiters */
	return !(isc->flags & CAM_IOSCHED_FLAG_TRIM_ACTIVE) && bp != NULL;
}

#define cam_iosched_sort_queue(isc)	((isc)->sort_io_queue >= 0 ?	\
    (isc)->sort_io_queue : cam_sort_io_queues)

static inline bool
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
	ios->in = 0;
	ios->max = ios->current = 300000;
	ios->min = 1;
	ios->out = 0;
	ios->errs = 0;
	ios->pending = 0;
	ios->queued = 0;
	ios->total = 0;
	ios->ema = 0;
	ios->emvar = 0;
	ios->bad_latency = SBT_1S / 2;	/* Default to 500ms */
	ios->softc = isc;
	cam_iosched_limiter_init(ios);
}

static int
cam_iosched_limiter_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	struct iop_stats *ios;
	struct cam_iosched_softc *isc;
	int value, i, error;
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
		/* Note: disk load averate requires ticker to be always running */
		callout_reset(&isc->ticker, hz / isc->quanta, cam_iosched_ticker, isc);
		isc->flags |= CAM_IOSCHED_FLAG_CALLOUT_ACTIVE;

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

static int
cam_iosched_sysctl_latencies(SYSCTL_HANDLER_ARGS)
{
	int i, error;
	struct sbuf sb;
	uint64_t *latencies;

	latencies = arg1;
	sbuf_new_for_sysctl(&sb, NULL, LAT_BUCKETS * 16, req);

	for (i = 0; i < LAT_BUCKETS - 1; i++)
		sbuf_printf(&sb, "%jd,", (intmax_t)latencies[i]);
	sbuf_printf(&sb, "%jd", (intmax_t)latencies[LAT_BUCKETS - 1]);
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);

	return (error);
}

static int
cam_iosched_quanta_sysctl(SYSCTL_HANDLER_ARGS)
{
	int *quanta;
	int error, value;

	quanta = (unsigned *)arg1;
	value = *quanta;

	error = sysctl_handle_int(oidp, (int *)&value, 0, req);
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	if (value < 1 || value > hz)
		return (EINVAL);

	*quanta = value;

	return (0);
}

static void
cam_iosched_iop_stats_sysctl_init(struct cam_iosched_softc *isc, struct iop_stats *ios, char *name)
{
	struct sysctl_oid_list *n;
	struct sysctl_ctx_list *ctx;

	ios->sysctl_tree = SYSCTL_ADD_NODE(&isc->sysctl_ctx,
	    SYSCTL_CHILDREN(isc->sysctl_tree), OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, name);
	n = SYSCTL_CHILDREN(ios->sysctl_tree);
	ctx = &ios->sysctl_ctx;

	SYSCTL_ADD_UQUAD(ctx, n,
	    OID_AUTO, "ema", CTLFLAG_RD,
	    &ios->ema,
	    "Fast Exponentially Weighted Moving Average");
	SYSCTL_ADD_UQUAD(ctx, n,
	    OID_AUTO, "emvar", CTLFLAG_RD,
	    &ios->emvar,
	    "Fast Exponentially Weighted Moving Variance");

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
	    "# of transactions completed (including with error)");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "errs", CTLFLAG_RD,
	    &ios->errs, 0,
	    "# of transactions completed with an error");
	SYSCTL_ADD_U64(ctx, n,
	    OID_AUTO, "too_long", CTLFLAG_RD,
	    &ios->too_long, 0,
	    "# of transactions completed took too long");
	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "bad_latency",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &ios->bad_latency, 0, cam_iosched_sbintime_sysctl, "A",
	    "Threshold for counting transactions that took too long (in us)");

	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "limiter",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
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

	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "latencies",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    &ios->latencies, 0,
	    cam_iosched_sysctl_latencies, "A",
	    "Array of latencies, a geometric progresson from\n"
	    "kern.cam.iosched.bucket_base_us with a ratio of\n"
	    "kern.cam.iosched.bucket_ration / 100 from one to\n"
	    "the next. By default 20 steps from 20us to 10.485s\n"
	    "by doubling.");

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
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Control loop info");
	n = SYSCTL_CHILDREN(clp->sysctl_tree);
	ctx = &clp->sysctl_ctx;

	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "type",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    clp, 0, cam_iosched_control_type_sysctl, "A",
	    "Control loop algorithm");
	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "steer_interval",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &clp->steer_interval, 0, cam_iosched_sbintime_sysctl, "A",
	    "How often to steer (in us)");
	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "lolat",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &clp->lolat, 0, cam_iosched_sbintime_sysctl, "A",
	    "Low water mark for Latency (in us)");
	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "hilat",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
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
cam_iosched_init(struct cam_iosched_softc **iscp, struct cam_periph *periph,
    const struct disk *dp, cam_iosched_schedule_t schedfnc)
{
	struct cam_iosched_softc *isc;

	isc = malloc(sizeof(*isc), M_CAMSCHED, M_NOWAIT | M_ZERO);
	if (isc == NULL)
		return ENOMEM;
	isc->disk = dp;
	isc->schedfnc = schedfnc;
#ifdef CAM_IOSCHED_DYNAMIC
	if (iosched_debug)
		printf("CAM IOSCHEDULER Allocating entry at %p\n", isc);
#endif
	isc->sort_io_queue = -1;
	bioq_init(&isc->bio_queue);
	bioq_init(&isc->trim_queue);
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_dynamic_iosched) {
		bioq_init(&isc->write_queue);
		isc->read_bias = default_read_bias;
		isc->current_read_bias = 0;
		isc->quanta = min(hz, 200);
		cam_iosched_iop_stats_init(isc, &isc->read_stats);
		cam_iosched_iop_stats_init(isc, &isc->write_stats);
		cam_iosched_iop_stats_init(isc, &isc->trim_stats);
		isc->trim_stats.max = 1;	/* Trims are special: one at a time for now */
		isc->last_time = sbinuptime();
		callout_init_mtx(&isc->ticker, cam_periph_mtx(periph), 0);
		isc->periph = periph;
		cam_iosched_cl_init(&isc->cl, isc);
		callout_reset(&isc->ticker, hz / isc->quanta, cam_iosched_ticker, isc);
		isc->flags |= CAM_IOSCHED_FLAG_CALLOUT_ACTIVE;
	}
#endif
	*iscp = isc;

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
	struct sysctl_oid_list *n;

	n = SYSCTL_CHILDREN(node);
	SYSCTL_ADD_INT(ctx, n,
		OID_AUTO, "sort_io_queue", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&isc->sort_io_queue, 0,
		"Sort IO queue to try and optimise disk access patterns");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "trim_goal", CTLFLAG_RW,
	    &isc->trim_goal, 0,
	    "Number of trims to try to accumulate before sending to hardware");
	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "trim_ticks", CTLFLAG_RW,
	    &isc->trim_goal, 0,
	    "IO Schedul qaunta to hold back trims for when accumulating");

#ifdef CAM_IOSCHED_DYNAMIC
	if (!do_dynamic_iosched)
		return;

	isc->sysctl_tree = SYSCTL_ADD_NODE(&isc->sysctl_ctx,
	    SYSCTL_CHILDREN(node), OID_AUTO, "iosched",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "I/O scheduler statistics");
	n = SYSCTL_CHILDREN(isc->sysctl_tree);
	ctx = &isc->sysctl_ctx;

	cam_iosched_iop_stats_sysctl_init(isc, &isc->read_stats, "read");
	cam_iosched_iop_stats_sysctl_init(isc, &isc->write_stats, "write");
	cam_iosched_iop_stats_sysctl_init(isc, &isc->trim_stats, "trim");
	cam_iosched_cl_sysctl_init(isc);

	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "read_bias", CTLFLAG_RW,
	    &isc->read_bias, default_read_bias,
	    "How biased towards read should we be independent of limits");

	SYSCTL_ADD_PROC(ctx, n,
	    OID_AUTO, "quanta", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &isc->quanta, 0, cam_iosched_quanta_sysctl, "I",
	    "How many quanta per second do we slice the I/O up into");

	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "total_ticks", CTLFLAG_RD,
	    &isc->total_ticks, 0,
	    "Total number of ticks we've done");

	SYSCTL_ADD_INT(ctx, n,
	    OID_AUTO, "load", CTLFLAG_RD,
	    &isc->load, 0,
	    "scaled load average / 100");

	SYSCTL_ADD_U64(ctx, n,
	    OID_AUTO, "latency_trigger", CTLFLAG_RW,
	    &isc->max_lat, 0,
	    "Latency treshold to trigger callbacks");
#endif
}

void
cam_iosched_set_latfcn(struct cam_iosched_softc *isc,
    cam_iosched_latfcn_t fnp, void *argp)
{
#ifdef CAM_IOSCHED_DYNAMIC
	isc->latfcn = fnp;
	isc->latarg = argp;
#endif
}

/*
 * Client drivers can set two parameters. "goal" is the number of BIO_DELETEs
 * that will be queued up before iosched will "release" the trims to the client
 * driver to wo with what they will (usually combine as many as possible). If we
 * don't get this many, after trim_ticks we'll submit the I/O anyway with
 * whatever we have.  We do need an I/O of some kind of to clock the deferred
 * trims out to disk. Since we will eventually get a write for the super block
 * or something before we shutdown, the trims will complete. To be safe, when a
 * BIO_FLUSH is presented to the iosched work queue, we set the ticks time far
 * enough in the past so we'll present the BIO_DELETEs to the client driver.
 * There might be a race if no BIO_DELETESs were queued, a BIO_FLUSH comes in
 * and then a BIO_DELETE is sent down. No know client does this, and there's
 * already a race between an ordered BIO_FLUSH and any BIO_DELETEs in flight,
 * but no client depends on the ordering being honored.
 *
 * XXX I'm not sure what the interaction between UFS direct BIOs and the BUF
 * flushing on shutdown. I think there's bufs that would be dependent on the BIO
 * finishing to write out at least metadata, so we'll be fine. To be safe, keep
 * the number of ticks low (less than maybe 10s) to avoid shutdown races.
 */

void
cam_iosched_set_trim_goal(struct cam_iosched_softc *isc, int goal)
{

	isc->trim_goal = goal;
}

void
cam_iosched_set_trim_ticks(struct cam_iosched_softc *isc, int trim_ticks)
{

	isc->trim_ticks = trim_ticks;
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
	if (do_dynamic_iosched)
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
			printf(
	"Reads present and current_read_bias is %d queued writes %d queued reads %d\n",
			    isc->current_read_bias, isc->write_stats.queued,
			    isc->read_stats.queued);
		isc->current_read_bias--;
		/* We're not limiting writes, per se, just doing reads first */
		return NULL;
	}

	/*
	 * See if our current limiter allows this I/O.
	 */
	if (cam_iosched_limiter_iop(&isc->write_stats, bp) != 0) {
		if (iosched_debug)
			printf("Can't write because limiter says no.\n");
		isc->write_stats.state_flags |= IOP_RATE_LIMITED;
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
	isc->write_stats.state_flags &= ~IOP_RATE_LIMITED;
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
	if (isc->queued_trims == 0)
		isc->last_trim_tick = ticks;
	isc->queued_trims++;
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
 * trim from the queue and the device must explicitly reinsert it
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
	isc->queued_trims--;
	isc->last_trim_tick = ticks;	/* Reset the tick timer when we take trims */
#ifdef CAM_IOSCHED_DYNAMIC
	isc->trim_stats.queued--;
	isc->trim_stats.total++;
	isc->trim_stats.pending++;
#endif
	return bp;
}

/*
 * gets an available trim from the trim queue, if there's no trim
 * already pending. It removes this trim from the queue and the device
 * must explicitly reinsert it should the need arise.
 *
 * Assumes we're called with the periph lock held.
 */
struct bio *
cam_iosched_get_trim(struct cam_iosched_softc *isc)
{
#ifdef CAM_IOSCHED_DYNAMIC
	struct bio *bp;
#endif

	if (!cam_iosched_has_more_trim(isc))
		return NULL;
#ifdef CAM_IOSCHED_DYNAMIC
	bp  = bioq_first(&isc->trim_queue);
	if (bp == NULL)
		return NULL;

	/*
	 * If pending read, prefer that based on current read bias setting. The
	 * read bias is shared for both writes and TRIMs, but on TRIMs the bias
	 * is for a combined TRIM not a single TRIM request that's come in.
	 */
	if (do_dynamic_iosched) {
		if (bioq_first(&isc->bio_queue) && isc->current_read_bias) {
			if (iosched_debug)
				printf(
		"Reads present and current_read_bias is %d queued trims %d queued reads %d\n",
				    isc->current_read_bias, isc->trim_stats.queued,
				    isc->read_stats.queued);
			isc->current_read_bias--;
			/* We're not limiting TRIMS, per se, just doing reads first */
			return NULL;
		}
		/*
		 * We're going to do a trim, so reset the bias.
		 */
		isc->current_read_bias = isc->read_bias;
	}

	/*
	 * See if our current limiter allows this I/O. Because we only call this
	 * here, and not in next_trim, the 'bandwidth' limits for trims won't
	 * work, while the iops or max queued limits will work. It's tricky
	 * because we want the limits to be from the perspective of the
	 * "commands sent to the device." To make iops work, we need to check
	 * only here (since we want all the ops we combine to count as one). To
	 * make bw limits work, we'd need to check in next_trim, but that would
	 * have the effect of limiting the iops as seen from the upper layers.
	 */
	if (cam_iosched_limiter_iop(&isc->trim_stats, bp) != 0) {
		if (iosched_debug)
			printf("Can't trim because limiter says no.\n");
		isc->trim_stats.state_flags |= IOP_RATE_LIMITED;
		return NULL;
	}
	isc->current_read_bias = isc->read_bias;
	isc->trim_stats.state_flags &= ~IOP_RATE_LIMITED;
	/* cam_iosched_next_trim below keeps proper book */
#endif
	return cam_iosched_next_trim(isc);
}


#ifdef CAM_IOSCHED_DYNAMIC
static struct bio *
bio_next(struct bio *bp)
{
	bp = TAILQ_NEXT(bp, bio_queue);
	/*
	 * After the first commands, the ordered bit terminates
	 * our search because BIO_ORDERED acts like a barrier.
	 */
	if (bp == NULL || bp->bio_flags & BIO_ORDERED)
		return NULL;
	return bp;
}

static bool
cam_iosched_rate_limited(struct iop_stats *ios)
{
	return ios->state_flags & IOP_RATE_LIMITED;
}
#endif

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
	 * See if we have any pending writes, room in the queue for them,
	 * and no pending reads (unless we've scheduled too many).
	 * if so, those are next.
	 */
	if (do_dynamic_iosched) {
		if ((bp = cam_iosched_get_write(isc)) != NULL)
			return bp;
	}
#endif
	/*
	 * next, see if there's other, normal I/O waiting. If so return that.
	 */
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_dynamic_iosched) {
		for (bp = bioq_first(&isc->bio_queue); bp != NULL;
		     bp = bio_next(bp)) {
			/*
			 * For the dynamic scheduler with a read bias, bio_queue
			 * is only for reads. However, without one, all
			 * operations are queued. Enforce limits here for any
			 * operation we find here.
			 */
			if (bp->bio_cmd == BIO_READ) {
				if (cam_iosched_rate_limited(&isc->read_stats) ||
				    cam_iosched_limiter_iop(&isc->read_stats, bp) != 0) {
					isc->read_stats.state_flags |= IOP_RATE_LIMITED;
					continue;
				}
				isc->read_stats.state_flags &= ~IOP_RATE_LIMITED;
			}
			/*
			 * There can only be write requests on the queue when
			 * the read bias is 0, but we need to process them
			 * here. We do not assert for read bias == 0, however,
			 * since it is dynamic and we can have WRITE operations
			 * in the queue after we transition from 0 to non-zero.
			 */
			if (bp->bio_cmd == BIO_WRITE) {
				if (cam_iosched_rate_limited(&isc->write_stats) ||
				    cam_iosched_limiter_iop(&isc->write_stats, bp) != 0) {
					isc->write_stats.state_flags |= IOP_RATE_LIMITED;
					continue;
				}
				isc->write_stats.state_flags &= ~IOP_RATE_LIMITED;
			}
			/*
			 * here we know we have a bp that's != NULL, that's not rate limited
			 * and can be the next I/O.
			 */
			break;
		}
	} else
#endif
		bp = bioq_first(&isc->bio_queue);

	if (bp == NULL)
		return (NULL);
	bioq_remove(&isc->bio_queue, bp);
#ifdef CAM_IOSCHED_DYNAMIC
	if (do_dynamic_iosched) {
		if (bp->bio_cmd == BIO_READ) {
			isc->read_stats.queued--;
			isc->read_stats.total++;
			isc->read_stats.pending++;
		} else if (bp->bio_cmd == BIO_WRITE) {
			isc->write_stats.queued--;
			isc->write_stats.total++;
			isc->write_stats.pending++;
		}
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
	 * A BIO_SPEEDUP from the upper layers means that they have a block
	 * shortage. At the present, this is only sent when we're trying to
	 * allocate blocks, but have a shortage before giving up. bio_length is
	 * the size of their shortage. We will complete just enough BIO_DELETEs
	 * in the queue to satisfy the need. If bio_length is 0, we'll complete
	 * them all. This allows the scheduler to delay BIO_DELETEs to improve
	 * read/write performance without worrying about the upper layers. When
	 * it's possibly a problem, we respond by pretending the BIO_DELETEs
	 * just worked. We can't do anything about the BIO_DELETEs in the
	 * hardware, though. We have to wait for them to complete.
	 */
	if (bp->bio_cmd == BIO_SPEEDUP) {
		off_t len;
		struct bio *nbp;

		len = 0;
		while (bioq_first(&isc->trim_queue) &&
		    (bp->bio_length == 0 || len < bp->bio_length)) {
			nbp = bioq_takefirst(&isc->trim_queue);
			len += nbp->bio_length;
			nbp->bio_error = 0;
			biodone(nbp);
		}
		if (bp->bio_length > 0) {
			if (bp->bio_length > len)
				bp->bio_resid = bp->bio_length - len;
			else
				bp->bio_resid = 0;
		}
		bp->bio_error = 0;
		biodone(bp);
		return;
	}

	/*
	 * If we get a BIO_FLUSH, and we're doing delayed BIO_DELETEs then we
	 * set the last tick time to one less than the current ticks minus the
	 * delay to force the BIO_DELETEs to be presented to the client driver.
	 */
	if (bp->bio_cmd == BIO_FLUSH && isc->trim_ticks > 0)
		isc->last_trim_tick = ticks - isc->trim_ticks - 1;

	/*
	 * Put all trims on the trim queue. Otherwise put the work on the bio
	 * queue.
	 */
	if (bp->bio_cmd == BIO_DELETE) {
		bioq_insert_tail(&isc->trim_queue, bp);
		if (isc->queued_trims == 0)
			isc->last_trim_tick = ticks;
		isc->queued_trims++;
#ifdef CAM_IOSCHED_DYNAMIC
		isc->trim_stats.in++;
		isc->trim_stats.queued++;
#endif
	}
#ifdef CAM_IOSCHED_DYNAMIC
	else if (do_dynamic_iosched && isc->read_bias != 0 &&
	    (bp->bio_cmd != BIO_READ)) {
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
 * Complete a trim request. Mark that we no longer have one in flight.
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
	if (!do_dynamic_iosched)
		return retval;

	if (iosched_debug > 10)
		printf("done: %p %#x\n", bp, bp->bio_cmd);
	if (bp->bio_cmd == BIO_WRITE) {
		retval = cam_iosched_limiter_iodone(&isc->write_stats, bp);
		if ((bp->bio_flags & BIO_ERROR) != 0)
			isc->write_stats.errs++;
		isc->write_stats.out++;
		isc->write_stats.pending--;
	} else if (bp->bio_cmd == BIO_READ) {
		retval = cam_iosched_limiter_iodone(&isc->read_stats, bp);
		if ((bp->bio_flags & BIO_ERROR) != 0)
			isc->read_stats.errs++;
		isc->read_stats.out++;
		isc->read_stats.pending--;
	} else if (bp->bio_cmd == BIO_DELETE) {
		if ((bp->bio_flags & BIO_ERROR) != 0)
			isc->trim_stats.errs++;
		isc->trim_stats.out++;
		isc->trim_stats.pending--;
	} else if (bp->bio_cmd != BIO_FLUSH) {
		if (iosched_debug)
			printf("Completing command with bio_cmd == %#x\n", bp->bio_cmd);
	}

	if ((bp->bio_flags & BIO_ERROR) == 0 && done_ccb != NULL &&
	    (done_ccb->ccb_h.status & CAM_QOS_VALID) != 0) {
		sbintime_t sim_latency;
		
		sim_latency = cam_iosched_sbintime_t(done_ccb->ccb_h.qos.periph_data);
		
		cam_iosched_io_metric_update(isc, sim_latency, bp);

		/*
		 * Debugging code: allow callbacks to the periph driver when latency max
		 * is exceeded. This can be useful for triggering external debugging actions.
		 */
		if (isc->latfcn && isc->max_lat != 0 && sim_latency > isc->max_lat)
			isc->latfcn(isc->latarg, sim_latency, bp);
	}
#endif
	return retval;
}

/*
 * Tell the io scheduler that you've pushed a trim down into the sim.
 * This also tells the I/O scheduler not to push any more trims down, so
 * some periphs do not call it if they can cope with multiple trims in flight.
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
 * Square Roots," reprinted at
 * http://www.embedded.com/electronics-blogs/programmer-s-toolbox/4219659/Integer-Square-Roots
 * and well worth the read. Briefly, we find the power of 4 that's the
 * largest smaller than val. We then check each smaller power of 4 to
 * see if val is still bigger. The right shifts at each step divide
 * the result by 2 which after successive application winds up
 * accumulating the right answer. It could also have been accumulated
 * using a separate root counter, but this code is smaller and faster
 * than that method. This method is also integer size invariant.
 * It returns floor(sqrt((float)val)), or the largest integer less than
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

static sbintime_t latencies[LAT_BUCKETS - 1] = {
	BUCKET_BASE <<  0,	/* 20us */
	BUCKET_BASE <<  1,
	BUCKET_BASE <<  2,
	BUCKET_BASE <<  3,
	BUCKET_BASE <<  4,
	BUCKET_BASE <<  5,
	BUCKET_BASE <<  6,
	BUCKET_BASE <<  7,
	BUCKET_BASE <<  8,
	BUCKET_BASE <<  9,
	BUCKET_BASE << 10,
	BUCKET_BASE << 11,
	BUCKET_BASE << 12,
	BUCKET_BASE << 13,
	BUCKET_BASE << 14,
	BUCKET_BASE << 15,
	BUCKET_BASE << 16,
	BUCKET_BASE << 17,
	BUCKET_BASE << 18	/* 5,242,880us */
};

#define CAM_IOSCHED_DEVD_MSG_SIZE	256

static void
cam_iosched_devctl_outlier(struct iop_stats *iop, sbintime_t sim_latency,
    const struct bio *bp)
{
	daddr_t lba = bp->bio_pblkno;
	daddr_t cnt = bp->bio_bcount / iop->softc->disk->d_sectorsize;
	char *sbmsg;
	struct sbuf sb;

	sbmsg = malloc(CAM_IOSCHED_DEVD_MSG_SIZE, M_CAMSCHED, M_NOWAIT);
	if (sbmsg == NULL)
		return;
	sbuf_new(&sb, sbmsg, CAM_IOSCHED_DEVD_MSG_SIZE, SBUF_FIXEDLEN);

	sbuf_printf(&sb, "device=%s%d lba=%jd blocks=%jd latency=%jd",
	    iop->softc->periph->periph_name,
	    iop->softc->periph->unit_number,
	    lba, cnt, sbttons(sim_latency));
	if (sbuf_finish(&sb) == 0)
		devctl_notify("CAM", "iosched", "latency", sbuf_data(&sb));
	sbuf_delete(&sb);
	free(sbmsg, M_CAMSCHED);
}

static void
cam_iosched_update(struct iop_stats *iop, sbintime_t sim_latency,
    const struct bio *bp)
{
	sbintime_t y, deltasq, delta;
	int i;

	/*
	 * Simple threshold: count the number of events that excede the
	 * configured threshold.
	 */
	if (sim_latency > iop->bad_latency) {
		cam_iosched_devctl_outlier(iop, sim_latency, bp);
		iop->too_long++;
	}

	/*
	 * Keep counts for latency. We do it by power of two buckets.
	 * This helps us spot outlier behavior obscured by averages.
	 */
	for (i = 0; i < LAT_BUCKETS - 1; i++) {
		if (sim_latency < latencies[i]) {
			iop->latencies[i]++;
			break;
		}
	}
	if (i == LAT_BUCKETS - 1)
		iop->latencies[i]++; 	 /* Put all > 8192ms values into the last bucket. */

	/*
	 * Classic exponentially decaying average with a tiny alpha
	 * (2 ^ -alpha_bits). For more info see the NIST statistical
	 * handbook.
	 *
	 * ema_t = y_t * alpha + ema_t-1 * (1 - alpha)		[nist]
	 * ema_t = y_t * alpha + ema_t-1 - alpha * ema_t-1
	 * ema_t = alpha * y_t - alpha * ema_t-1 + ema_t-1
	 * alpha = 1 / (1 << alpha_bits)
	 * sub e == ema_t-1, b == 1/alpha (== 1 << alpha_bits), d == y_t - ema_t-1
	 *	= y_t/b - e/b + be/b
	 *      = (y_t - e + be) / b
	 *	= (e + d) / b
	 *
	 * Since alpha is a power of two, we can compute this w/o any mult or
	 * division.
	 *
	 * Variance can also be computed. Usually, it would be expressed as follows:
	 *	diff_t = y_t - ema_t-1
	 *	emvar_t = (1 - alpha) * (emavar_t-1 + diff_t^2 * alpha)
	 *	  = emavar_t-1 - alpha * emavar_t-1 + delta_t^2 * alpha - (delta_t * alpha)^2
	 * sub b == 1/alpha (== 1 << alpha_bits), e == emavar_t-1, d = delta_t^2
	 *	  = e - e/b + dd/b + dd/bb
	 *	  = (bbe - be + bdd + dd) / bb
	 *	  = (bbe + b(dd-e) + dd) / bb (which is expanded below bb = 1<<(2*alpha_bits))
	 */
	/*
	 * XXX possible numeric issues
	 *	o We assume right shifted integers do the right thing, since that's
	 *	  implementation defined. You can change the right shifts to / (1LL << alpha).
	 *	o alpha_bits = 9 gives ema ceiling of 23 bits of seconds for ema and 14 bits
	 *	  for emvar. This puts a ceiling of 13 bits on alpha since we need a
	 *	  few tens of seconds of representation.
	 *	o We mitigate alpha issues by never setting it too high.
	 */
	y = sim_latency;
	delta = (y - iop->ema);					/* d */
	iop->ema = ((iop->ema << alpha_bits) + delta) >> alpha_bits;

	/*
	 * Were we to naively plow ahead at this point, we wind up with many numerical
	 * issues making any SD > ~3ms unreliable. So, we shift right by 12. This leaves
	 * us with microsecond level precision in the input, so the same in the
	 * output. It means we can't overflow deltasq unless delta > 4k seconds. It
	 * also means that emvar can be up 46 bits 40 of which are fraction, which
	 * gives us a way to measure up to ~8s in the SD before the computation goes
	 * unstable. Even the worst hard disk rarely has > 1s service time in the
	 * drive. It does mean we have to shift left 12 bits after taking the
	 * square root to compute the actual standard deviation estimate. This loss of
	 * precision is preferable to needing int128 types to work. The above numbers
	 * assume alpha=9. 10 or 11 are ok, but we start to run into issues at 12,
	 * so 12 or 13 is OK for EMA, EMVAR and SD will be wrong in those cases.
	 */
	delta >>= 12;
	deltasq = delta * delta;				/* dd */
	iop->emvar = ((iop->emvar << (2 * alpha_bits)) +	/* bbe */
	    ((deltasq - iop->emvar) << alpha_bits) +		/* b(dd-e) */
	    deltasq)						/* dd */
	    >> (2 * alpha_bits);				/* div bb */
	iop->sd = (sbintime_t)isqrt64((uint64_t)iop->emvar) << 12;
}

static void
cam_iosched_io_metric_update(struct cam_iosched_softc *isc,
    sbintime_t sim_latency, const struct bio *bp)
{
	switch (bp->bio_cmd) {
	case BIO_READ:
		cam_iosched_update(&isc->read_stats, sim_latency, bp);
		break;
	case BIO_WRITE:
		cam_iosched_update(&isc->write_stats, sim_latency, bp);
		break;
	case BIO_DELETE:
		cam_iosched_update(&isc->trim_stats, sim_latency, bp);
		break;
	default:
		break;
	}
}

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
	db_printf("Read Q len         %d\n", biolen(&isc->bio_queue));
	db_printf("pending_writes:    %d\n", isc->write_stats.pending);
	db_printf("min_writes:        %d\n", isc->write_stats.min);
	db_printf("max_writes:        %d\n", isc->write_stats.max);
	db_printf("writes:            %d\n", isc->write_stats.total);
	db_printf("in_writes:         %d\n", isc->write_stats.in);
	db_printf("out_writes:        %d\n", isc->write_stats.out);
	db_printf("queued_writes:     %d\n", isc->write_stats.queued);
	db_printf("Write Q len        %d\n", biolen(&isc->write_queue));
	db_printf("pending_trims:     %d\n", isc->trim_stats.pending);
	db_printf("min_trims:         %d\n", isc->trim_stats.min);
	db_printf("max_trims:         %d\n", isc->trim_stats.max);
	db_printf("trims:             %d\n", isc->trim_stats.total);
	db_printf("in_trims:          %d\n", isc->trim_stats.in);
	db_printf("out_trims:         %d\n", isc->trim_stats.out);
	db_printf("queued_trims:      %d\n", isc->trim_stats.queued);
	db_printf("Trim Q len         %d\n", biolen(&isc->trim_queue));
	db_printf("read_bias:         %d\n", isc->read_bias);
	db_printf("current_read_bias: %d\n", isc->current_read_bias);
	db_printf("Trim active?       %s\n",
	    (isc->flags & CAM_IOSCHED_FLAG_TRIM_ACTIVE) ? "yes" : "no");
}
#endif
#endif
