/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From: @(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_callout_profiling.h"
#include "opt_ddb.h"
#if defined(__arm__)
#include "opt_timer.h"
#endif
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/file.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/sleepqueue.h>
#include <sys/sysctl.h>
#include <sys/smp.h>

#ifdef DDB
#include <ddb/ddb.h>
#include <machine/_inttypes.h>
#endif

#ifdef SMP
#include <machine/cpu.h>
#endif

#ifndef NO_EVENTTIMERS
DPCPU_DECLARE(sbintime_t, hardclocktime);
#endif

SDT_PROVIDER_DEFINE(callout_execute);
SDT_PROBE_DEFINE1(callout_execute, , , callout__start, "struct callout *");
SDT_PROBE_DEFINE1(callout_execute, , , callout__end, "struct callout *");

#ifdef CALLOUT_PROFILING
static int avg_depth[2];
SYSCTL_INT(_debug, OID_AUTO, to_avg_depth, CTLFLAG_RD, &avg_depth[0], 0,
    "Average number of items examined per softclock call. Units = 1/1000");
static int avg_gcalls[2];
SYSCTL_INT(_debug, OID_AUTO, to_avg_gcalls, CTLFLAG_RD, &avg_gcalls[0], 0,
    "Average number of Giant callouts made per softclock call. Units = 1/1000");
static int avg_lockcalls[2];
SYSCTL_INT(_debug, OID_AUTO, to_avg_lockcalls, CTLFLAG_RD, &avg_lockcalls[0], 0,
    "Average number of lock callouts made per softclock call. Units = 1/1000");
static int avg_mpcalls[2];
SYSCTL_INT(_debug, OID_AUTO, to_avg_mpcalls, CTLFLAG_RD, &avg_mpcalls[0], 0,
    "Average number of MP callouts made per softclock call. Units = 1/1000");
SYSCTL_INT(_debug, OID_AUTO, to_avg_depth_dir, CTLFLAG_RD, &avg_depth[1], 0,
    "Average number of direct callouts examined per callout_process call. "
    "Units = 1/1000");
SYSCTL_INT(_debug, OID_AUTO, to_avg_lockcalls_dir, CTLFLAG_RD,
    &avg_lockcalls[1], 0, "Average number of lock direct callouts made per "
    "callout_process call. Units = 1/1000");
SYSCTL_INT(_debug, OID_AUTO, to_avg_mpcalls_dir, CTLFLAG_RD, &avg_mpcalls[1],
    0, "Average number of MP direct callouts made per callout_process call. "
    "Units = 1/1000");
#endif

static int ncallout;
SYSCTL_INT(_kern, OID_AUTO, ncallout, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &ncallout, 0,
    "Number of entries in callwheel and size of timeout() preallocation");

#ifdef	RSS
static int pin_default_swi = 1;
static int pin_pcpu_swi = 1;
#else
static int pin_default_swi = 0;
static int pin_pcpu_swi = 0;
#endif

SYSCTL_INT(_kern, OID_AUTO, pin_default_swi, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &pin_default_swi,
    0, "Pin the default (non-per-cpu) swi (shared with PCPU 0 swi)");
SYSCTL_INT(_kern, OID_AUTO, pin_pcpu_swi, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &pin_pcpu_swi,
    0, "Pin the per-CPU swis (except PCPU 0, which is also default");

/*
 * TODO:
 *	allocate more timeout table slots when table overflows.
 */
u_int callwheelsize, callwheelmask;

struct callout_args {
	sbintime_t time;		/* absolute time for the event */
	sbintime_t precision;		/* delta allowed wrt opt */
	void	*arg;			/* function argument */
	callout_func_t *func;		/* function to call */
	int	flags;			/* flags passed to callout_reset() */
	int	cpu;			/* CPU we're scheduled on */
};

typedef void callout_mutex_op_t(void *, struct lock_object *);

struct callout_mutex_ops {
	callout_mutex_op_t *lock;
	callout_mutex_op_t *unlock;
};

enum {
	CALLOUT_LC_UNUSED_0,
	CALLOUT_LC_UNUSED_1,
	CALLOUT_LC_UNUSED_2,
	CALLOUT_LC_FUNCTION,
	CALLOUT_LC_SPIN,
	CALLOUT_LC_MUTEX,
	CALLOUT_LC_RW,
	CALLOUT_LC_RM,
};

static void
callout_mutex_op_none(void *arg, struct lock_object *lock)
{
}

static void
callout_function_lock(void *arg, struct lock_object *lock)
{

	((callout_lock_func_t *)lock)(arg, 1);
}

static void
callout_function_unlock(void *arg, struct lock_object *lock)
{

	((callout_lock_func_t *)lock)(arg, 0);
}

static void
callout_mutex_lock(void *arg, struct lock_object *lock)
{

	mtx_lock((struct mtx *)lock);
}

static void
callout_mutex_unlock(void *arg, struct lock_object *lock)
{

	mtx_unlock((struct mtx *)lock);
}

static void
callout_mutex_lock_spin(void *arg, struct lock_object *lock)
{

	mtx_lock_spin((struct mtx *)lock);
}

static void
callout_mutex_unlock_spin(void *arg, struct lock_object *lock)
{

	mtx_unlock_spin((struct mtx *)lock);
}

static void
callout_rm_wlock(void *arg, struct lock_object *lock)
{

	rm_wlock((struct rmlock *)lock);
}

static void
callout_rm_wunlock(void *arg, struct lock_object *lock)
{

	rm_wunlock((struct rmlock *)lock);
}

static void
callout_rw_wlock(void *arg, struct lock_object *lock)
{

	rw_wlock((struct rwlock *)lock);
}

static void
callout_rw_wunlock(void *arg, struct lock_object *lock)
{

	rw_wunlock((struct rwlock *)lock);
}

static const struct callout_mutex_ops callout_mutex_ops[8] = {
	[CALLOUT_LC_UNUSED_0] = {
		.lock = callout_mutex_op_none,
		.unlock = callout_mutex_op_none,
	},
	[CALLOUT_LC_UNUSED_1] = {
		.lock = callout_mutex_op_none,
		.unlock = callout_mutex_op_none,
	},
	[CALLOUT_LC_UNUSED_2] = {
		.lock = callout_mutex_op_none,
		.unlock = callout_mutex_op_none,
	},
	[CALLOUT_LC_FUNCTION] = {
		.lock = callout_function_lock,
		.unlock = callout_function_unlock,
	},
	[CALLOUT_LC_SPIN] = {
		.lock = callout_mutex_lock_spin,
		.unlock = callout_mutex_unlock_spin,
	},
	[CALLOUT_LC_MUTEX] = {
		.lock = callout_mutex_lock,
		.unlock = callout_mutex_unlock,
	},
	[CALLOUT_LC_RW] = {
		.lock = callout_rw_wlock,
		.unlock = callout_rw_wunlock,
	},
	[CALLOUT_LC_RM] = {
		.lock = callout_rm_wlock,
		.unlock = callout_rm_wunlock,
	},
};

static inline void
callout_lock_client(int c_flags, void *c_arg, struct lock_object *c_lock)
{

	callout_mutex_ops[CALLOUT_GET_LC(c_flags)].lock(c_arg, c_lock);
}

static inline void
callout_unlock_client(int c_flags, void *c_arg, struct lock_object *c_lock)
{

	callout_mutex_ops[CALLOUT_GET_LC(c_flags)].unlock(c_arg, c_lock);
}

/*
 * The callout CPU exec structure represent information necessary for
 * describing the state of callouts currently running on the CPU and
 * for handling deferred callout restarts.
 *
 * In particular, the first entry of the array cc_exec_entity holds
 * information for callouts running from the SWI thread context, while
 * the second one holds information for callouts running directly from
 * the hardware interrupt context.
 */
struct cc_exec {
	/*
	 * The "cc_curr" points to the currently executing callout and
	 * is protected by the "cc_lock" spinlock. If no callback is
	 * currently executing it is equal to "NULL".
	 */
	struct callout		*cc_curr;
	/*
	 * The "cc_restart_args" structure holds the argument for a
	 * deferred callback restart and is protected by the "cc_lock"
	 * spinlock. The structure is only valid if "cc_restart" is
	 * "true". If "cc_restart" is "false" the information in the
	 * "cc_restart_args" structure shall be ignored.
	 */
	struct callout_args	cc_restart_args;
	bool			cc_restart;
	/*
	 * The "cc_cancel" variable allows the currently pending
	 * callback to be atomically cancelled. This field is write
	 * protected by the "cc_lock" spinlock.
	 */
	bool cc_cancel;
	/*
	 * The "cc_drain_fn" points to a function which shall be
	 * called when an asynchronous drain is performed. This field
	 * is write protected by the "cc_lock" spinlock.
	 */
	callout_func_t *cc_drain_fn;
	/*
	 * The following fields are used for callout profiling only:
	 */
#ifdef CALLOUT_PROFILING
	int cc_depth;
	int cc_mpcalls;
	int cc_lockcalls;
	int cc_gcalls;
#endif
};

/*
 * There is one "struct callout_cpu" per CPU, holding all relevant
 * state for the callout processing thread on the individual CPU.
 */
struct callout_cpu {
	struct mtx_padalign	cc_lock;
	struct cc_exec 		cc_exec_entity[2];
	struct callout		*cc_callout;
	struct callout_list	*cc_callwheel;
	struct callout_list	cc_directlist;
	struct callout_tailq	cc_expireq;
	struct callout_slist	cc_callfree;
	sbintime_t		cc_firstevent;
	sbintime_t		cc_lastscan;
	void			*cc_cookie;
	char			cc_ktr_event_name[20];
};

#define	cc_exec_curr(cc, dir)		(cc)->cc_exec_entity[(dir)].cc_curr
#define	cc_exec_restart_args(cc, dir)	(cc)->cc_exec_entity[(dir)].cc_restart_args
#define	cc_exec_restart(cc, dir)	(cc)->cc_exec_entity[(dir)].cc_restart
#define	cc_exec_cancel(cc, dir)		(cc)->cc_exec_entity[(dir)].cc_cancel
#define	cc_exec_drain_fn(cc, dir)	(cc)->cc_exec_entity[(dir)].cc_drain_fn
#define	cc_exec_depth(cc, dir)		(cc)->cc_exec_entity[(dir)].cc_depth
#define	cc_exec_mpcalls(cc, dir)	(cc)->cc_exec_entity[(dir)].cc_mpcalls
#define	cc_exec_lockcalls(cc, dir)	(cc)->cc_exec_entity[(dir)].cc_lockcalls
#define	cc_exec_gcalls(cc, dir)		(cc)->cc_exec_entity[(dir)].cc_gcalls

#ifdef SMP
struct callout_cpu cc_cpu[MAXCPU];
#define	CPUBLOCK	-1
#define	CC_CPU(cpu)	(&cc_cpu[(cpu)])
#define	CC_SELF()	CC_CPU(PCPU_GET(cpuid))
#else
struct callout_cpu cc_cpu;
#define	CC_CPU(cpu)	&cc_cpu
#define	CC_SELF()	&cc_cpu
#endif
#define	CC_LOCK(cc)	mtx_lock_spin(&(cc)->cc_lock)
#define	CC_UNLOCK(cc)	mtx_unlock_spin(&(cc)->cc_lock)
#define	CC_LOCK_ASSERT(cc)	mtx_assert(&(cc)->cc_lock, MA_OWNED)

static int timeout_cpu;

static void	callout_cpu_init(struct callout_cpu *cc, int cpu);
static void	softclock_call_cc(struct callout *c, struct callout_cpu *cc, const int direct);

static MALLOC_DEFINE(M_CALLOUT, "callout", "Callout datastructures");

/*
 * Kernel low level callwheel initialization called from cpu0 during
 * kernel startup:
 */
static void
callout_callwheel_init(void *dummy)
{
	struct callout_cpu *cc;

	/*
	 * Calculate the size of the callout wheel and the preallocated
	 * timeout() structures.
	 * XXX: Clip callout to result of previous function of maxusers
	 * maximum 384.  This is still huge, but acceptable.
	 */
	memset(CC_CPU(0), 0, sizeof(cc_cpu));
	ncallout = imin(16 + maxproc + maxfiles, 18508);
	TUNABLE_INT_FETCH("kern.ncallout", &ncallout);

	/*
	 * Calculate callout wheel size, should be next power of two higher
	 * than 'ncallout'.
	 */
	callwheelsize = 1 << fls(ncallout);
	callwheelmask = callwheelsize - 1;

	/*
	 * Fetch whether we're pinning the swi's or not.
	 */
	TUNABLE_INT_FETCH("kern.pin_default_swi", &pin_default_swi);
	TUNABLE_INT_FETCH("kern.pin_pcpu_swi", &pin_pcpu_swi);

	/*
	 * Only cpu0 handles timeout(9) and receives a preallocation.
	 *
	 * XXX: Once all timeout(9) consumers are converted this can
	 * be removed.
	 */
	timeout_cpu = PCPU_GET(cpuid);
	cc = CC_CPU(timeout_cpu);
	cc->cc_callout = malloc(ncallout * sizeof(struct callout),
	    M_CALLOUT, M_WAITOK);
	callout_cpu_init(cc, timeout_cpu);
}
SYSINIT(callwheel_init, SI_SUB_CPU, SI_ORDER_ANY, callout_callwheel_init, NULL);

/*
 * Initialize the per-cpu callout structures.
 */
static void
callout_cpu_init(struct callout_cpu *cc, int cpu)
{
	struct callout *c;
	int i;

	mtx_init(&cc->cc_lock, "callout", NULL, MTX_SPIN | MTX_RECURSE);
	SLIST_INIT(&cc->cc_callfree);
	cc->cc_callwheel = malloc(sizeof(struct callout_list) * callwheelsize,
	    M_CALLOUT, M_WAITOK);
	for (i = 0; i < callwheelsize; i++)
		LIST_INIT(&cc->cc_callwheel[i]);
	TAILQ_INIT(&cc->cc_expireq);
	LIST_INIT(&cc->cc_directlist);
	cc->cc_firstevent = SBT_MAX;
	snprintf(cc->cc_ktr_event_name, sizeof(cc->cc_ktr_event_name),
	    "callwheel cpu %d", cpu);
	if (cc->cc_callout == NULL)	/* Only cpu0 handles timeout(9) */
		return;
	for (i = 0; i < ncallout; i++) {
		c = &cc->cc_callout[i];
		callout_init(c, 0);
		c->c_flags |= CALLOUT_LOCAL_ALLOC;
		SLIST_INSERT_HEAD(&cc->cc_callfree, c, c_links.sle);
	}
}

#ifdef CALLOUT_PROFILING
static inline void
callout_clear_stats(struct callout_cpu *cc, const int direct)
{
	cc_exec_depth(cc, direct) = 0;
	cc_exec_mpcalls(cc, direct) = 0;
	cc_exec_lockcalls(cc, direct) = 0;
	cc_exec_gcalls(cc, direct) = 0;
}
#endif

#ifdef CALLOUT_PROFILING
static inline void
callout_update_stats(struct callout_cpu *cc, const int direct)
{
	avg_depth[direct] +=
	    (cc_exec_depth(cc, direct) * 1000 -
	    avg_depth[direct]) >> 8;
	avg_mpcalls[direct] +=
	    (cc_exec_mpcalls(cc, direct) * 1000 -
	    avg_mpcalls[direct]) >> 8;
	avg_lockcalls[direct] +=
	    (cc_exec_lockcalls(cc, direct) * 1000 -
	    avg_lockcalls[direct]) >> 8;
	avg_gcalls[direct] +=
	    (cc_exec_gcalls(cc, direct) * 1000 -
	    avg_gcalls[direct]) >> 8;
}
#endif

/*
 * Start standard softclock thread.
 */
static void
start_softclock(void *dummy)
{
	struct callout_cpu *cc;
	char name[MAXCOMLEN];
#ifdef SMP
	int cpu;
	struct intr_event *ie;
#endif

	cc = CC_CPU(timeout_cpu);
	snprintf(name, sizeof(name), "clock (%d)", timeout_cpu);
	if (swi_add(&clk_intr_event, name, softclock, cc, SWI_CLOCK,
	    INTR_MPSAFE, &cc->cc_cookie))
		panic("died while creating standard software ithreads");
	if (pin_default_swi &&
	    (intr_event_bind(clk_intr_event, timeout_cpu) != 0)) {
		printf("%s: timeout clock couldn't be pinned to cpu %d\n",
		    __func__,
		    timeout_cpu);
	}

#ifdef SMP
	CPU_FOREACH(cpu) {
		if (cpu == timeout_cpu)
			continue;
		cc = CC_CPU(cpu);
		cc->cc_callout = NULL;	/* Only cpu0 handles timeout(9). */
		callout_cpu_init(cc, cpu);
		snprintf(name, sizeof(name), "clock (%d)", cpu);
		ie = NULL;
		if (swi_add(&ie, name, softclock, cc, SWI_CLOCK,
		    INTR_MPSAFE, &cc->cc_cookie))
			panic("died while creating standard software ithreads");
		if (pin_pcpu_swi && (intr_event_bind(ie, cpu) != 0)) {
			printf("%s: per-cpu clock couldn't be pinned to "
			    "cpu %d\n",
			    __func__,
			    cpu);
		}
	}
#endif
}
SYSINIT(start_softclock, SI_SUB_SOFTINTR, SI_ORDER_FIRST, start_softclock, NULL);

#define	CC_HASH_SHIFT	8

static inline u_int
callout_hash(sbintime_t sbt)
{

	return (sbt >> (32 - CC_HASH_SHIFT));
}

static inline u_int
callout_get_bucket(sbintime_t sbt)
{

	return (callout_hash(sbt) & callwheelmask);
}

void
callout_process(sbintime_t now)
{
	struct callout *tmp;
	struct callout *next;
	struct callout_cpu *cc;
	struct callout_list *sc;
	sbintime_t first, last, max, tmp_max;
	uint32_t lookahead;
	u_int firstb, lastb, nowb;

	cc = CC_SELF();
	CC_LOCK(cc);

#ifdef CALLOUT_PROFILING
	callout_clear_stats(cc, 1);
#endif
	/* Compute the buckets of the last scan and present times. */
	firstb = callout_hash(cc->cc_lastscan);
	cc->cc_lastscan = now;
	nowb = callout_hash(now);

	/* Compute the last bucket and minimum time of the bucket after it. */
	if (nowb == firstb)
		lookahead = (SBT_1S / 16);
	else if (nowb - firstb == 1)
		lookahead = (SBT_1S / 8);
	else
		lookahead = (SBT_1S / 2);
	first = last = now;
	first += (lookahead / 2);
	last += lookahead;
	last &= (0xffffffffffffffffLLU << (32 - CC_HASH_SHIFT));
	lastb = callout_hash(last) - 1;
	max = last;

	/*
	 * Check if we wrapped around the entire wheel from the last scan.
	 * In case, we need to scan entirely the wheel for pending callouts.
	 */
	if (lastb - firstb >= callwheelsize) {
		lastb = firstb + callwheelsize - 1;
		if (nowb - firstb >= callwheelsize)
			nowb = lastb;
	}

	/* Iterate callwheel from firstb to nowb and then up to lastb. */
	do {
		sc = &cc->cc_callwheel[firstb & callwheelmask];

		/* Iterate all callouts in the current bucket */
		LIST_FOREACH_SAFE(tmp, sc, c_links.le, next) {
			/* Run the callout if present time within allowed. */
			if (tmp->c_time <= now) {
				/* Remove callout from bucket */
				LIST_REMOVE(tmp, c_links.le);
				if (tmp->c_flags & CALLOUT_DIRECT) {
					/* Insert callout into direct list */
					LIST_INSERT_HEAD(&cc->cc_directlist, tmp, c_links.le);
				} else {
					/* Insert callout into expired list */
					TAILQ_INSERT_TAIL(&cc->cc_expireq, tmp, c_links.tqe);
					tmp->c_flags |= CALLOUT_PROCESSED;
				}
				continue;
			}

			/* Skip events from distant future. */
			if (tmp->c_time >= max)
				continue;

			/*
			 * Event minimal time is bigger than present maximal
			 * time, so it cannot be aggregated.
			 */
			if (tmp->c_time > last) {
				lastb = nowb;
				continue;
			}
			/* Update first and last time, respecting this event. */
			if (tmp->c_time < first)
				first = tmp->c_time;
			tmp_max = tmp->c_time + tmp->c_precision;
			if (tmp_max < last)
				last = tmp_max;
		}

		/* Proceed with the next bucket. */
		firstb++;

		/*
		 * Stop if we looked after present time and found
		 * some event we can't execute at now.
		 * Stop if we looked far enough into the future.
		 */
	} while (((int)(firstb - lastb)) <= 0);

	cc->cc_firstevent = last;
#ifndef NO_EVENTTIMERS
	cpu_new_callout(curcpu, last, first);
#endif
	/*
	 * Check for expired direct callouts, if any:
	 */
	while ((tmp = LIST_FIRST(&cc->cc_directlist)) != NULL) {
		LIST_REMOVE(tmp, c_links.le);
		softclock_call_cc(tmp, cc, 1);
	}
#ifdef CALLOUT_PROFILING
	callout_update_stats(cc, 1);
#endif
	CC_UNLOCK(cc);
	/*
	 * "swi_sched()" acquires the thread lock and we don't want to
	 * call it having cc_lock held because it leads to a locking
	 * order reversal issue.
	 */
	if (!TAILQ_EMPTY(&cc->cc_expireq))
		swi_sched(cc->cc_cookie, 0);
}

static struct callout_cpu *
callout_lock(struct callout *c)
{
	struct callout_cpu *cc;
	int cpu;

	for (;;) {
		cpu = c->c_cpu;
#ifdef SMP
		if (cpu == CPUBLOCK) {
			cpu_spinwait();
			continue;
		}
#endif
		cc = CC_CPU(cpu);
		CC_LOCK(cc);
		if (cpu == c->c_cpu)
			break;
		CC_UNLOCK(cc);
	}
	return (cc);
}

static struct callout_cpu *
callout_cc_add_locked(struct callout *c, struct callout_cpu *cc,
    struct callout_args *coa)
{
#ifndef NO_EVENTTIMERS
	sbintime_t sbt;
#endif
	u_int bucket;

	CC_LOCK_ASSERT(cc);

	/* update flags before swapping locks, if any */
	c->c_flags &= ~(CALLOUT_PROCESSED | CALLOUT_DIRECT);
	if (coa->flags & C_DIRECT_EXEC)
		c->c_flags |= (CALLOUT_ACTIVE | CALLOUT_PENDING | CALLOUT_DIRECT);
	else
		c->c_flags |= (CALLOUT_ACTIVE | CALLOUT_PENDING);

#ifdef SMP
	/* only set the "c_cpu" if the CPU number changed and is valid */
	if (c->c_cpu != coa->cpu && coa->cpu > CPUBLOCK &&
	    coa->cpu <= mp_maxid && !CPU_ABSENT(coa->cpu)) {
		/*
		 * Avoid interrupts and preemption firing after the
		 * callout CPU is blocked in order to avoid deadlocks
		 * as the new thread may be willing to acquire the
		 * callout CPU lock:
		 */
		c->c_cpu = CPUBLOCK;
		spinlock_enter();
		CC_UNLOCK(cc);
		cc = CC_CPU(coa->cpu);
		CC_LOCK(cc);
		spinlock_exit();
		c->c_cpu = coa->cpu;
	}
#endif
	if (coa->time < cc->cc_lastscan)
		coa->time = cc->cc_lastscan;
	c->c_arg = coa->arg;
	c->c_func = coa->func;
	c->c_time = coa->time;
	c->c_precision = coa->precision;

	bucket = callout_get_bucket(c->c_time);
	CTR3(KTR_CALLOUT, "precision set for %p: %d.%08x",
	    c, (int)(c->c_precision >> 32),
	    (u_int)(c->c_precision & 0xffffffff));
	LIST_INSERT_HEAD(&cc->cc_callwheel[bucket], c, c_links.le);

#ifndef NO_EVENTTIMERS
	/*
	 * Inform the eventtimers(4) subsystem there's a new callout
	 * that has been inserted, but only if really required.
	 */
	if (SBT_MAX - c->c_time < c->c_precision)
		c->c_precision = SBT_MAX - c->c_time;
	sbt = c->c_time + c->c_precision;
	if (sbt < cc->cc_firstevent) {
		cc->cc_firstevent = sbt;
		cpu_new_callout(c->c_cpu, sbt, c->c_time);
	}
#endif
	return (cc);
}

static inline void
callout_cc_del(struct callout *c, struct callout_cpu *cc)
{

	c->c_func = NULL;
	SLIST_INSERT_HEAD(&cc->cc_callfree, c, c_links.sle);
}

static inline void
softclock_call_cc(struct callout *c, struct callout_cpu *cc,
    const int direct)
{
	callout_func_t *c_func;
	void *c_arg;
	struct lock_object *c_lock;
	int c_flags;
#if defined(DIAGNOSTIC) || defined(CALLOUT_PROFILING) 
	sbintime_t sbt1, sbt2;
	struct timespec ts2;
	static sbintime_t maxdt = 2 * SBT_1MS;	/* 2 msec */
	static timeout_t *lastfunc;
#endif

	KASSERT((c->c_flags & (CALLOUT_PENDING | CALLOUT_ACTIVE)) ==
	    (CALLOUT_PENDING | CALLOUT_ACTIVE),
	    ("softclock_call_cc: pend|act %p %x", c, c->c_flags));

	c_lock = c->c_lock;
	c_func = c->c_func;
	c_arg = c->c_arg;
	c_flags = c->c_flags;

	/* remove pending bit */
	c->c_flags &= ~CALLOUT_PENDING;

	/* reset our local state */
	cc_exec_curr(cc, direct) = c;
	cc_exec_restart(cc, direct) = false;
	cc_exec_drain_fn(cc, direct) = NULL;

	if (c_lock != NULL) {
		cc_exec_cancel(cc, direct) = false;
		CC_UNLOCK(cc);

		/* unlocked region for switching locks */

		callout_lock_client(c_flags, c_arg, c_lock);

		/*
		 * Check if the callout may have been cancelled while
		 * we were switching locks. Even though the callout is
		 * specifying a lock, it might not be certain this
		 * lock is locked when starting and stopping callouts.
		 */
		CC_LOCK(cc);
		if (cc_exec_cancel(cc, direct)) {
			callout_unlock_client(c_flags, c_arg, c_lock);
			goto skip_cc_locked;
		}
		if (c_lock == &Giant.lock_object) {
#ifdef CALLOUT_PROFILING
			cc_exec_gcalls(cc, direct)++;
#endif
			CTR3(KTR_CALLOUT, "callout giant %p func %p arg %p",
			    c, c_func, c_arg);
		} else {
#ifdef CALLOUT_PROFILING
			cc_exec_lockcalls(cc, direct)++;
#endif
			CTR3(KTR_CALLOUT, "callout lock %p func %p arg %p",
			    c, c_func, c_arg);
		}
	} else {
#ifdef CALLOUT_PROFILING
		cc_exec_mpcalls(cc, direct)++;
#endif
		CTR3(KTR_CALLOUT, "callout %p func %p arg %p",
		    c, c_func, c_arg);
	}
	/* The callout cannot be stopped now! */
	cc_exec_cancel(cc, direct) = true;
	CC_UNLOCK(cc);

	/* unlocked region */
	KTR_STATE3(KTR_SCHED, "callout", cc->cc_ktr_event_name, "running",
	    "func:%p", c_func, "arg:%p", c_arg, "direct:%d", direct);
#if defined(DIAGNOSTIC) || defined(CALLOUT_PROFILING)
	sbt1 = sbinuptime();
#endif
	THREAD_NO_SLEEPING();
	SDT_PROBE1(callout_execute, , , callout__start, c);
	c_func(c_arg);
	SDT_PROBE1(callout_execute, , , callout__end, c);
	THREAD_SLEEPING_OK();
#if defined(DIAGNOSTIC) || defined(CALLOUT_PROFILING)
	sbt2 = sbinuptime();
	sbt2 -= sbt1;
	if (sbt2 > maxdt) {
		if (lastfunc != c_func || sbt2 > maxdt * 2) {
			ts2 = sbttots(sbt2);
			printf(
		"Expensive timeout(9) function: %p(%p) %jd.%09ld s\n",
			    c_func, c_arg, (intmax_t)ts2.tv_sec, ts2.tv_nsec);
		}
		maxdt = sbt2;
		lastfunc = c_func;
	}
#endif
	KTR_STATE0(KTR_SCHED, "callout", cc->cc_ktr_event_name, "idle");
	CTR1(KTR_CALLOUT, "callout %p finished", c);

	/*
	 * At this point the callback structure might have been freed,
	 * so we need to check the previously copied value of
	 * "c->c_flags":
	 */
	if ((c_flags & CALLOUT_RETURNUNLOCKED) == 0)
		callout_unlock_client(c_flags, c_arg, c_lock);

	CC_LOCK(cc);

skip_cc_locked:
	KASSERT(cc_exec_curr(cc, direct) == c, ("mishandled cc_curr"));
	cc_exec_curr(cc, direct) = NULL;

	/* Check if there is anything which needs draining */
	if (cc_exec_drain_fn(cc, direct) != NULL) {
		/*
		 * Unlock the CPU callout last, so that any use of
		 * structures belonging to the callout are complete:
		 */
		CC_UNLOCK(cc);
		/* call drain function unlocked */
		cc_exec_drain_fn(cc, direct)(c_arg);
		CC_LOCK(cc);
	} else if (c_flags & CALLOUT_LOCAL_ALLOC) {
		/* return callout back to freelist */
		callout_cc_del(c, cc);
	} else if (cc_exec_restart(cc, direct)) {
		struct callout_cpu *new_cc;
		/* [re-]schedule callout, if any */
		new_cc = callout_cc_add_locked(c, cc,
		    &cc_exec_restart_args(cc, direct));
		if (new_cc != cc) {
			/* switch locks back again */
			CC_UNLOCK(new_cc);
			CC_LOCK(cc);
		}
	}
}

/*
 * The callout mechanism is based on the work of Adam M. Costello and
 * George Varghese, published in a technical report entitled "Redesigning
 * the BSD Callout and Timer Facilities" and modified slightly for inclusion
 * in FreeBSD by Justin T. Gibbs.  The original work on the data structures
 * used in this implementation was published by G. Varghese and T. Lauck in
 * the paper "Hashed and Hierarchical Timing Wheels: Data Structures for
 * the Efficient Implementation of a Timer Facility" in the Proceedings of
 * the 11th ACM Annual Symposium on Operating Systems Principles,
 * Austin, Texas Nov 1987.
 */

/*
 * Software (low priority) clock interrupt.
 * Run periodic events from timeout queue.
 */
void
softclock(void *arg)
{
	struct callout_cpu *cc;
	struct callout *c;

	cc = (struct callout_cpu *)arg;
	CC_LOCK(cc);
#ifdef CALLOUT_PROFILING
	callout_clear_stats(cc, 0);
#endif
	while ((c = TAILQ_FIRST(&cc->cc_expireq)) != NULL) {
		TAILQ_REMOVE(&cc->cc_expireq, c, c_links.tqe);
		softclock_call_cc(c, cc, 0);
	}
#ifdef CALLOUT_PROFILING
	callout_update_stats(cc, 0);
#endif
	CC_UNLOCK(cc);
}

/*
 * timeout --
 *	Execute a function after a specified length of time.
 *
 * untimeout --
 *	Cancel previous timeout function call.
 *
 * callout_handle_init --
 *	Initialize a handle so that using it with untimeout is benign.
 *
 *	See AT&T BCI Driver Reference Manual for specification.  This
 *	implementation differs from that one in that although an
 *	identification value is returned from timeout, the original
 *	arguments to timeout as well as the identifier are used to
 *	identify entries for untimeout.
 */
struct callout_handle
timeout(timeout_t *ftn, void *arg, int to_ticks)
{
	struct callout_cpu *cc;
	struct callout *new;
	struct callout_handle handle;

	cc = CC_CPU(timeout_cpu);
	CC_LOCK(cc);
	/* Fill in the next free callout structure. */
	new = SLIST_FIRST(&cc->cc_callfree);
	if (new == NULL)
		/* XXX Attempt to malloc first */
		panic("timeout table full");
	SLIST_REMOVE_HEAD(&cc->cc_callfree, c_links.sle);
	handle.callout = new;
	CC_UNLOCK(cc);

	callout_reset(new, to_ticks, ftn, arg);

	return (handle);
}

void
untimeout(timeout_t *ftn, void *arg, struct callout_handle handle)
{
	struct callout_cpu *cc;
	bool match;

	/*
	 * Check for a handle that was initialized
	 * by callout_handle_init, but never used
	 * for a real timeout.
	 */
	if (handle.callout == NULL)
		return;

	cc = callout_lock(handle.callout);
	match = (handle.callout->c_func == ftn && handle.callout->c_arg == arg);
	CC_UNLOCK(cc);

	if (match)
		callout_stop(handle.callout);
}

void
callout_handle_init(struct callout_handle *handle)
{
	handle->callout = NULL;
}

#ifdef KTR
static const char *
callout_retvalstring(int retval)
{
	switch (retval) {
	case CALLOUT_RET_DRAINING:
		return ("callout cannot be stopped and needs drain");
	case CALLOUT_RET_CANCELLED:
		return ("callout was successfully stopped");
	case CALLOUT_RET_CANCELLED_AND_DRAINING:
		return ("callout was successfully stopped while being serviced");
	default:
		return ("callout was already stopped");
	}
}
#endif

static int
callout_restart_async(struct callout *c, struct callout_args *coa,
    callout_func_t *drain_fn)
{
	struct callout_cpu *cc;
	int retval;
	int direct;

	cc = callout_lock(c);

	/* Figure out if the callout is direct or not */
	direct = ((c->c_flags & CALLOUT_DIRECT) != 0);

	/*
	 * Check if the callback is currently scheduled for
	 * completion:
	 */
	if (cc_exec_curr(cc, direct) == c) {

		retval = CALLOUT_RET_DRAINING;

		/* set drain function, if any */
		if (drain_fn != NULL)
			cc_exec_drain_fn(cc, direct) = drain_fn;

		/*
		 * Try to prevent the callback from running by setting
		 * the "cc_exec_cancel()" variable to "true".
		 */
		if (cc_exec_cancel(cc, direct) == false ||
		    cc_exec_restart(cc, direct) == true) {
			cc_exec_cancel(cc, direct) = true;
			retval |= CALLOUT_RET_CANCELLED;
		}

		/*
		 * Prevent callback restart if "callout_drain_xxx()"
		 * is being called or we are stopping the callout or
		 * the callback was preallocated by us:
		 */
		if (cc_exec_drain_fn(cc, direct) != NULL ||
		    coa == NULL || (c->c_flags & CALLOUT_LOCAL_ALLOC) != 0) {
			CTR4(KTR_CALLOUT, "%s: %p func %p arg %p",
			    callout_retvalstring(retval),
			    c, c->c_func, c->c_arg);

			/* clear old flags, if any */
			c->c_flags &= ~(CALLOUT_ACTIVE | CALLOUT_PENDING |
			    CALLOUT_PROCESSED);

			/* clear restart flag, if any */
			cc_exec_restart(cc, direct) = false;
		} else {
			CTR4(KTR_CALLOUT, "%s: %p func %p arg %p",
			    callout_retvalstring(retval),
			    c, c->c_func, c->c_arg);

			/* get us back into the game */
			c->c_flags |= (CALLOUT_ACTIVE | CALLOUT_PENDING);
			c->c_flags &= ~CALLOUT_PROCESSED;

			/* enable deferred restart */
			cc_exec_restart(cc, direct) = true;

			/* store arguments for the deferred restart, if any */
			cc_exec_restart_args(cc, direct) = *coa;
		}
	} else {
		/* stop callout */
		if (c->c_flags & CALLOUT_PENDING) {
			/*
			 * The callback has not yet been executed, and
			 * we simply just need to unlink it:
			 */
			if ((c->c_flags & CALLOUT_PROCESSED) == 0) {
				LIST_REMOVE(c, c_links.le);
			} else {
				TAILQ_REMOVE(&cc->cc_expireq, c, c_links.tqe);
			}
			retval = CALLOUT_RET_CANCELLED;
		} else {
			retval = CALLOUT_RET_STOPPED;
		}

		CTR4(KTR_CALLOUT, "%s: %p func %p arg %p",
		    callout_retvalstring(retval),
		    c, c->c_func, c->c_arg);

		/* [re-]schedule callout, if any */
		if (coa != NULL) {
			cc = callout_cc_add_locked(c, cc, coa);
		} else {
			/* clear old flags, if any */
			c->c_flags &= ~(CALLOUT_ACTIVE | CALLOUT_PENDING |
			    CALLOUT_PROCESSED);

			/* return callback to pre-allocated list, if any */
			if ((c->c_flags & CALLOUT_LOCAL_ALLOC) &&
			    retval != CALLOUT_RET_STOPPED) {
				callout_cc_del(c, cc);
			}
		}
	}
	CC_UNLOCK(cc);
	return (retval);
}

void
callout_when(sbintime_t sbt, sbintime_t precision, int flags,
    sbintime_t *res, sbintime_t *prec_res)
{
	sbintime_t to_sbt, to_pr;

	if ((flags & (C_ABSOLUTE | C_PRECALC)) != 0) {
		*res = sbt;
		*prec_res = precision;
		return;
	}
	if ((flags & C_HARDCLOCK) != 0 && sbt < tick_sbt)
		sbt = tick_sbt;
	if ((flags & C_HARDCLOCK) != 0 ||
#ifdef NO_EVENTTIMERS
	    sbt >= sbt_timethreshold) {
		to_sbt = getsbinuptime();

		/* Add safety belt for the case of hz > 1000. */
		to_sbt += tc_tick_sbt - tick_sbt;
#else
	    sbt >= sbt_tickthreshold) {
		/*
		 * Obtain the time of the last hardclock() call on
		 * this CPU directly from the kern_clocksource.c.
		 * This value is per-CPU, but it is equal for all
		 * active ones.
		 */
#ifdef __LP64__
		to_sbt = DPCPU_GET(hardclocktime);
#else
		spinlock_enter();
		to_sbt = DPCPU_GET(hardclocktime);
		spinlock_exit();
#endif
#endif
		if ((flags & C_HARDCLOCK) == 0)
			to_sbt += tick_sbt;
	} else
		to_sbt = sbinuptime();
	if (SBT_MAX - to_sbt < sbt)
		to_sbt = SBT_MAX;
	else
		to_sbt += sbt;
	*res = to_sbt;
	to_pr = ((C_PRELGET(flags) < 0) ? sbt >> tc_precexp :
	    sbt >> C_PRELGET(flags));
	*prec_res = to_pr > precision ? to_pr : precision;
}

/*
 * New interface; clients allocate their own callout structures.
 *
 * callout_reset() - establish or change a timeout
 * callout_stop() - disestablish a timeout
 * callout_init() - initialize a callout structure so that it can
 *	safely be passed to callout_reset() and callout_stop()
 *
 * <sys/callout.h> defines three convenience macros:
 *
 * callout_active() - returns truth if callout has not been stopped,
 *	drained, or deactivated since the last time the callout was
 *	reset.
 * callout_pending() - returns truth if callout is still waiting for timeout
 * callout_deactivate() - marks the callout as having been serviced
 */
int
callout_reset_sbt_on(struct callout *c, sbintime_t sbt, sbintime_t prec,
    callout_func_t *ftn, void *arg, int cpu, int flags)
{
	struct callout_args coa;

	/* store arguments for callout add function */
	coa.func = ftn;
	coa.arg = arg;
	coa.flags = flags;
	coa.cpu = cpu;

	/* compute trigger time for callout */
	callout_when(sbt, prec, flags, &coa.time, &coa.precision);

	/* get callback started, if any */
	return (callout_restart_async(c, &coa, NULL));
}

/*
 * Common idioms that can be optimized in the future.
 */
int
callout_schedule_on(struct callout *c, int to_ticks, int cpu)
{
	return (callout_reset_on(c, to_ticks, c->c_func, c->c_arg, cpu));
}

int
callout_schedule(struct callout *c, int to_ticks)
{
	return (callout_reset_on(c, to_ticks, c->c_func, c->c_arg, c->c_cpu));
}

int
callout_stop(struct callout *c)
{
	/* get callback stopped, if any */
	return (callout_restart_async(c, NULL, NULL));
}

static void
callout_drain_function(void *arg)
{
	wakeup(&callout_drain_function);
}

int
callout_async_drain(struct callout *c, callout_func_t *fn)
{
	/* get callback stopped, if any */
	return (callout_restart_async(c, NULL, fn));
}

int
callout_drain(struct callout *c)
{
	int retval;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "Draining callout");

	/* at this point the "c->c_cpu" field is not changing */

	retval = callout_async_drain(c, &callout_drain_function);

	if (retval & CALLOUT_RET_DRAINING) {
		void *ident = &callout_drain_function;
		struct callout_cpu *cc;
		int direct;
		int busy;

		CTR3(KTR_CALLOUT, "need to drain %p func %p arg %p",
		    c, c->c_func, c->c_arg);

		do {
			/*
			 * The sleepq_lock() is lower rank than the
			 * callout_lock() and must be locked first:
			 */
			sleepq_lock(ident);
			cc = callout_lock(c);
			direct = ((c->c_flags & CALLOUT_DIRECT) != 0);
			busy = (cc_exec_curr(cc, direct) == c);
			CC_UNLOCK(cc);
			DROP_GIANT();

			if (busy && !SCHEDULER_STOPPED()) {
				/* Wait for drain to complete */
				sleepq_add(ident, &cc->cc_lock.lock_object, "codrain", SLEEPQ_SLEEP, 0);
				sleepq_wait(ident, 0);
			} else {
				sleepq_release(ident);
			}

			PICKUP_GIANT();
		} while (busy);
	}

	CTR4(KTR_CALLOUT, "%s: %p func %p arg %p",
	    callout_retvalstring(retval),
	    c, c->c_func, c->c_arg);

	return (retval);
}

void
callout_init(struct callout *c, int mpsafe)
{
	if (mpsafe) {
		callout_init_lock_object(c, NULL, CALLOUT_RETURNUNLOCKED);
	} else {
		callout_init_lock_object(c, &Giant.lock_object, 0);
	}
}

void
callout_init_lock_function(struct callout *c, callout_lock_func_t *lock_fn, int flags)
{
	bzero(c, sizeof *c);

	KASSERT((flags & ~CALLOUT_RETURNUNLOCKED) == 0,
	    ("callout_init_lock_function: bad flags 0x%08x", flags));
	KASSERT(lock_fn != NULL,
	    ("callout_init_lock_function: lock function is NULL"));
	flags &= CALLOUT_RETURNUNLOCKED;
	flags |= CALLOUT_SET_LC(CALLOUT_LC_FUNCTION);
	c->c_lock = (struct lock_object *)lock_fn;
	c->c_flags = flags;
	c->c_cpu = timeout_cpu;
}

void
callout_init_lock_object(struct callout *c, struct lock_object *lock, int flags)
{
	bzero(c, sizeof *c);
	KASSERT((flags & ~CALLOUT_RETURNUNLOCKED) == 0,
	    ("callout_init_lock_object: bad flags 0x%08x", flags));
	flags &= CALLOUT_RETURNUNLOCKED;
	if (lock != NULL) {
		struct lock_class *class = LOCK_CLASS(lock);
		if (class == &lock_class_mtx_sleep)
			flags |= CALLOUT_SET_LC(CALLOUT_LC_MUTEX);
		else if (class == &lock_class_mtx_spin)
			flags |= CALLOUT_SET_LC(CALLOUT_LC_SPIN);
		else if (class == &lock_class_rm)
			flags |= CALLOUT_SET_LC(CALLOUT_LC_RM);
		else if (class == &lock_class_rw)
			flags |= CALLOUT_SET_LC(CALLOUT_LC_RW);
		else
			panic("callout_init_lock_object: Unsupported lock class '%s'\n",
			    class->lc_name);
	} else {
		flags |= CALLOUT_SET_LC(CALLOUT_LC_UNUSED_0);
	}
	c->c_lock = lock;
	c->c_flags = flags;
	c->c_cpu = timeout_cpu;
}

#ifdef APM_FIXUP_CALLTODO
/* 
 * Adjust the kernel calltodo timeout list.  This routine is used after 
 * an APM resume to recalculate the calltodo timer list values with the 
 * number of hz's we have been sleeping.  The next hardclock() will detect 
 * that there are fired timers and run softclock() to execute them.
 *
 * Please note, I have not done an exhaustive analysis of what code this
 * might break.  I am motivated to have my select()'s and alarm()'s that
 * have expired during suspend firing upon resume so that the applications
 * which set the timer can do the maintanence the timer was for as close
 * as possible to the originally intended time.  Testing this code for a 
 * week showed that resuming from a suspend resulted in 22 to 25 timers 
 * firing, which seemed independent on whether the suspend was 2 hours or
 * 2 days.  Your milage may vary.   - Ken Key <key@cs.utk.edu>
 */
void
adjust_timeout_calltodo(struct timeval *time_change)
{
	register struct callout *p;
	unsigned long delta_ticks;

	/* 
	 * How many ticks were we asleep?
	 * (stolen from tvtohz()).
	 */

	/* Don't do anything */
	if (time_change->tv_sec < 0)
		return;
	else if (time_change->tv_sec <= LONG_MAX / 1000000)
		delta_ticks = howmany(time_change->tv_sec * 1000000 +
		    time_change->tv_usec, tick) + 1;
	else if (time_change->tv_sec <= LONG_MAX / hz)
		delta_ticks = time_change->tv_sec * hz +
		    howmany(time_change->tv_usec, tick) + 1;
	else
		delta_ticks = LONG_MAX;

	if (delta_ticks > INT_MAX)
		delta_ticks = INT_MAX;

	/* 
	 * Now rip through the timer calltodo list looking for timers
	 * to expire.
	 */

	/* don't collide with softclock() */
	CC_LOCK(cc);
	for (p = calltodo.c_next; p != NULL; p = p->c_next) {
		p->c_time -= delta_ticks;

		/* Break if the timer had more time on it than delta_ticks */
		if (p->c_time > 0)
			break;

		/* take back the ticks the timer didn't use (p->c_time <= 0) */
		delta_ticks = -p->c_time;
	}
	CC_UNLOCK(cc);

	return;
}
#endif /* APM_FIXUP_CALLTODO */

static int
flssbt(sbintime_t sbt)
{

	sbt += (uint64_t)sbt >> 1;
	if (sizeof(long) >= sizeof(sbintime_t))
		return (flsl(sbt));
	if (sbt >= SBT_1S)
		return (flsl(((uint64_t)sbt) >> 32) + 32);
	return (flsl(sbt));
}

/*
 * Dump immediate statistic snapshot of the scheduled callouts.
 */
static int
sysctl_kern_callout_stat(SYSCTL_HANDLER_ARGS)
{
	struct callout *tmp;
	struct callout_cpu *cc;
	struct callout_list *sc;
	sbintime_t maxpr, maxt, medpr, medt, now, spr, st, t;
	int ct[64], cpr[64], ccpbk[32];
	int error, val, i, count, tcum, pcum, maxc, c, medc;
#ifdef SMP
	int cpu;
#endif

	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	count = maxc = 0;
	st = spr = maxt = maxpr = 0;
	bzero(ccpbk, sizeof(ccpbk));
	bzero(ct, sizeof(ct));
	bzero(cpr, sizeof(cpr));
	now = sbinuptime();
#ifdef SMP
	CPU_FOREACH(cpu) {
		cc = CC_CPU(cpu);
#else
		cc = CC_CPU(timeout_cpu);
#endif
		CC_LOCK(cc);
		for (i = 0; i < callwheelsize; i++) {
			sc = &cc->cc_callwheel[i];
			c = 0;
			LIST_FOREACH(tmp, sc, c_links.le) {
				c++;
				t = tmp->c_time - now;
				if (t < 0)
					t = 0;
				st += t / SBT_1US;
				spr += tmp->c_precision / SBT_1US;
				if (t > maxt)
					maxt = t;
				if (tmp->c_precision > maxpr)
					maxpr = tmp->c_precision;
				ct[flssbt(t)]++;
				cpr[flssbt(tmp->c_precision)]++;
			}
			if (c > maxc)
				maxc = c;
			ccpbk[fls(c + c / 2)]++;
			count += c;
		}
		CC_UNLOCK(cc);
#ifdef SMP
	}
#endif

	for (i = 0, tcum = 0; i < 64 && tcum < count / 2; i++)
		tcum += ct[i];
	medt = (i >= 2) ? (((sbintime_t)1) << (i - 2)) : 0;
	for (i = 0, pcum = 0; i < 64 && pcum < count / 2; i++)
		pcum += cpr[i];
	medpr = (i >= 2) ? (((sbintime_t)1) << (i - 2)) : 0;
	for (i = 0, c = 0; i < 32 && c < count / 2; i++)
		c += ccpbk[i];
	medc = (i >= 2) ? (1 << (i - 2)) : 0;

	printf("Scheduled callouts statistic snapshot:\n");
	printf("  Callouts: %6d  Buckets: %6d*%-3d  Bucket size: 0.%06ds\n",
	    count, callwheelsize, mp_ncpus, 1000000 >> CC_HASH_SHIFT);
	printf("  C/Bk: med %5d         avg %6d.%06jd  max %6d\n",
	    medc,
	    count / callwheelsize / mp_ncpus,
	    (uint64_t)count * 1000000 / callwheelsize / mp_ncpus % 1000000,
	    maxc);
	printf("  Time: med %5jd.%06jds avg %6jd.%06jds max %6jd.%06jds\n",
	    medt / SBT_1S, (medt & 0xffffffff) * 1000000 >> 32,
	    (st / count) / 1000000, (st / count) % 1000000,
	    maxt / SBT_1S, (maxt & 0xffffffff) * 1000000 >> 32);
	printf("  Prec: med %5jd.%06jds avg %6jd.%06jds max %6jd.%06jds\n",
	    medpr / SBT_1S, (medpr & 0xffffffff) * 1000000 >> 32,
	    (spr / count) / 1000000, (spr / count) % 1000000,
	    maxpr / SBT_1S, (maxpr & 0xffffffff) * 1000000 >> 32);
	printf("  Distribution:       \tbuckets\t   time\t   tcum\t"
	    "   prec\t   pcum\n");
	for (i = 0, tcum = pcum = 0; i < 64; i++) {
		if (ct[i] == 0 && cpr[i] == 0)
			continue;
		t = (i != 0) ? (((sbintime_t)1) << (i - 1)) : 0;
		tcum += ct[i];
		pcum += cpr[i];
		printf("  %10jd.%06jds\t 2**%d\t%7d\t%7d\t%7d\t%7d\n",
		    t / SBT_1S, (t & 0xffffffff) * 1000000 >> 32,
		    i - 1 - (32 - CC_HASH_SHIFT),
		    ct[i], tcum, cpr[i], pcum);
	}
	return (error);
}
SYSCTL_PROC(_kern, OID_AUTO, callout_stat,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_kern_callout_stat, "I",
    "Dump immediate statistic snapshot of the scheduled callouts");

#ifdef DDB
static void
_show_callout(struct callout *c)
{

	db_printf("callout %p\n", c);
#define	C_DB_PRINTF(f, e)	db_printf("   %s = " f "\n", #e, c->e);
	db_printf("   &c_links = %p\n", &(c->c_links));
	C_DB_PRINTF("%" PRId64,	c_time);
	C_DB_PRINTF("%" PRId64,	c_precision);
	C_DB_PRINTF("%p",	c_arg);
	C_DB_PRINTF("%p",	c_func);
	C_DB_PRINTF("%p",	c_lock);
	C_DB_PRINTF("%#x",	c_flags);
	C_DB_PRINTF("%d",	c_cpu);
#undef	C_DB_PRINTF
}

DB_SHOW_COMMAND(callout, db_show_callout)
{

	if (!have_addr) {
		db_printf("usage: show callout <struct callout *>\n");
		return;
	}

	_show_callout((struct callout *)addr);
}
#endif /* DDB */
