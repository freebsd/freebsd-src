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
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/sleepqueue.h>
#include <sys/sysctl.h>
#include <sys/smp.h>

#ifdef SMP
#include <machine/cpu.h>
#endif

SDT_PROVIDER_DEFINE(callout_execute);
SDT_PROBE_DEFINE(callout_execute, kernel, , callout_start, callout-start);
SDT_PROBE_ARGTYPE(callout_execute, kernel, , callout_start, 0,
    "struct callout *");
SDT_PROBE_DEFINE(callout_execute, kernel, , callout_end, callout-end); 
SDT_PROBE_ARGTYPE(callout_execute, kernel, , callout_end, 0,
    "struct callout *");

#ifdef CALLOUT_PROFILING
static int avg_depth;
SYSCTL_INT(_debug, OID_AUTO, to_avg_depth, CTLFLAG_RD, &avg_depth, 0,
    "Average number of items examined per softclock call. Units = 1/1000");
static int avg_gcalls;
SYSCTL_INT(_debug, OID_AUTO, to_avg_gcalls, CTLFLAG_RD, &avg_gcalls, 0,
    "Average number of Giant callouts made per softclock call. Units = 1/1000");
static int avg_lockcalls;
SYSCTL_INT(_debug, OID_AUTO, to_avg_lockcalls, CTLFLAG_RD, &avg_lockcalls, 0,
    "Average number of lock callouts made per softclock call. Units = 1/1000");
static int avg_mpcalls;
SYSCTL_INT(_debug, OID_AUTO, to_avg_mpcalls, CTLFLAG_RD, &avg_mpcalls, 0,
    "Average number of MP callouts made per softclock call. Units = 1/1000");
static int avg_depth_dir;
SYSCTL_INT(_debug, OID_AUTO, to_avg_depth_dir, CTLFLAG_RD, &avg_depth_dir, 0,
    "Average number of direct callouts examined per callout_process call. "
    "Units = 1/1000");
static int avg_lockcalls_dir;
SYSCTL_INT(_debug, OID_AUTO, to_avg_lockcalls_dir, CTLFLAG_RD,
    &avg_lockcalls_dir, 0, "Average number of lock direct callouts made per "
    "callout_process call. Units = 1/1000");
static int avg_mpcalls_dir;
SYSCTL_INT(_debug, OID_AUTO, to_avg_mpcalls_dir, CTLFLAG_RD, &avg_mpcalls_dir,
    0, "Average number of MP direct callouts made per callout_process call. "
    "Units = 1/1000");
#endif
/*
 * TODO:
 *	allocate more timeout table slots when table overflows.
 */
int callwheelsize, callwheelmask;

/*
 * The callout cpu exec entities represent informations necessary for
 * describing the state of callouts currently running on the CPU and the ones  
 * necessary for migrating callouts to the new callout cpu. In particular,
 * the first entry of the array cc_exec_entity holds informations for callout
 * running in SWI thread context, while the second one holds informations
 * for callout running directly from hardware interrupt context.
 * The cached informations are very important for deferring migration when
 * the migrating callout is already running.
 */
struct cc_exec {
	struct callout		*cc_next;
	struct callout		*cc_curr;
#ifdef SMP
	void			(*ce_migration_func)(void *);
	void			*ce_migration_arg;
	int			ce_migration_cpu;
	struct bintime		ce_migration_time;
#endif
	int			cc_cancel;
	int			cc_waiting;
};
	
/*
 * There is one struct callou_cpu per cpu, holding all relevant
 * state for the callout processing thread on the individual CPU.
 */
struct callout_cpu {
	struct cc_exec 		cc_exec_entity[2];
	struct mtx		cc_lock;
	struct callout		*cc_callout;
	struct callout_tailq	*cc_callwheel;
	struct callout_tailq	cc_expireq;		  
	struct callout_list	cc_callfree;
	struct bintime 		cc_firstevent;
	struct bintime 		cc_lastscan;
	void			*cc_cookie;
};

#define	cc_exec_curr		cc_exec_entity[0].cc_curr
#define	cc_exec_next		cc_exec_entity[0].cc_next
#define	cc_exec_cancel		cc_exec_entity[0].cc_cancel
#define	cc_exec_waiting		cc_exec_entity[0].cc_waiting
#define	cc_exec_curr_dir	cc_exec_entity[1].cc_curr
#define	cc_exec_next_dir	cc_exec_entity[1].cc_next
#define	cc_exec_cancel_dir	cc_exec_entity[1].cc_cancel
#define	cc_exec_waiting_dir	cc_exec_entity[1].cc_waiting

#ifdef SMP
#define	cc_migration_func	cc_exec_entity[0].ce_migration_func
#define	cc_migration_arg	cc_exec_entity[0].ce_migration_arg
#define	cc_migration_cpu	cc_exec_entity[0].ce_migration_cpu
#define	cc_migration_time	cc_exec_entity[0].ce_migration_time
#define	cc_migration_func_dir	cc_exec_entity[1].ce_migration_func
#define	cc_migration_arg_dir	cc_exec_entity[1].ce_migration_arg
#define	cc_migration_cpu_dir	cc_exec_entity[1].ce_migration_cpu
#define	cc_migration_time_dir	cc_exec_entity[1].ce_migration_time

struct callout_cpu cc_cpu[MAXCPU];
#define	CPUBLOCK	MAXCPU
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
#define	C_PRECISION	0x2

#define FREQ2BT(freq, bt)                                               \
{                                                                       \
        (bt)->sec = 0;                                                  \
        (bt)->frac = ((uint64_t)0x8000000000000000  / (freq)) << 1;     \
}

#define	TIME_T_MAX							\
	(sizeof(time_t) == (sizeof(int64_t)) ? INT64_MAX : INT32_MAX)		

static int timeout_cpu;
void (*callout_new_inserted)(int cpu, struct bintime bt) = NULL;
static struct callout *
softclock_call_cc(struct callout *c, struct callout_cpu *cc, int *mpcalls,
    int *lockcalls, int *gcalls, int direct);

static MALLOC_DEFINE(M_CALLOUT, "callout", "Callout datastructures");

/**
 * Locked by cc_lock:
 *   cc_curr         - If a callout is in progress, it is cc_curr.
 *                     If cc_curr is non-NULL, threads waiting in
 *                     callout_drain() will be woken up as soon as the
 *                     relevant callout completes.
 *   cc_cancel       - Changing to 1 with both callout_lock and cc_lock held
 *                     guarantees that the current callout will not run.
 *                     The softclock() function sets this to 0 before it
 *                     drops callout_lock to acquire c_lock, and it calls
 *                     the handler only if curr_cancelled is still 0 after
 *                     cc_lock is successfully acquired.
 *   cc_waiting      - If a thread is waiting in callout_drain(), then
 *                     callout_wait is nonzero.  Set only when
 *                     cc_curr is non-NULL.
 */

/*
 * Resets the migration entity tied to a specific callout cpu.
 */
static void
cc_cme_cleanup(struct callout_cpu *cc, int direct)
{
	
	cc->cc_exec_entity[direct].cc_curr = NULL;	
	cc->cc_exec_entity[direct].cc_next = NULL;
	cc->cc_exec_entity[direct].cc_cancel = 0;
	cc->cc_exec_entity[direct].cc_waiting = 0;
#ifdef SMP
	cc->cc_exec_entity[direct].ce_migration_cpu = CPUBLOCK;
	bintime_clear(&cc->cc_exec_entity[direct].ce_migration_time);
	cc->cc_exec_entity[direct].ce_migration_func = NULL;
	cc->cc_exec_entity[direct].ce_migration_arg = NULL;
#endif
}

/*
 * Checks if migration is requested by a specific callout cpu.
 */
static int
cc_cme_migrating(struct callout_cpu *cc, int direct)
{

#ifdef SMP
	
	return (cc->cc_exec_entity[direct].ce_migration_cpu != CPUBLOCK);
#else
	return (0);
#endif
}

/*
 * kern_timeout_callwheel_alloc() - kernel low level callwheel initialization 
 *
 *	This code is called very early in the kernel initialization sequence,
 *	and may be called more then once.
 */
caddr_t
kern_timeout_callwheel_alloc(caddr_t v)
{
	struct callout_cpu *cc;

	timeout_cpu = PCPU_GET(cpuid);
	cc = CC_CPU(timeout_cpu);
	/*
	 * Calculate callout wheel size
	 */
	callwheelsize = 1;
	while (callwheelsize < ncallout) 
		callwheelsize <<= 1;
	callwheelmask = callwheelsize - 1;

	cc->cc_callout = (struct callout *)v;
	v = (caddr_t)(cc->cc_callout + ncallout);
	cc->cc_callwheel = (struct callout_tailq *)v;
	v = (caddr_t)(cc->cc_callwheel + callwheelsize);
	return(v);
}

static void
callout_cpu_init(struct callout_cpu *cc)
{
	struct callout *c;
	int i;

	mtx_init(&cc->cc_lock, "callout", NULL, MTX_SPIN | MTX_RECURSE);
	SLIST_INIT(&cc->cc_callfree);
	for (i = 0; i < callwheelsize; i++) {
		TAILQ_INIT(&cc->cc_callwheel[i]);
	}
	TAILQ_INIT(&cc->cc_expireq);
	for (i = 0; i < 2; i++) 
		cc_cme_cleanup(cc, i);
	if (cc->cc_callout == NULL)
		return;
	for (i = 0; i < ncallout; i++) {
		c = &cc->cc_callout[i];
		callout_init(c, 0);
		c->c_flags = CALLOUT_LOCAL_ALLOC;
		SLIST_INSERT_HEAD(&cc->cc_callfree, c, c_links.sle);
	}
}

#ifdef SMP
/*
 * Switches the cpu tied to a specific callout.
 * The function expects a locked incoming callout cpu and returns with
 * locked outcoming callout cpu.
 */
static struct callout_cpu *
callout_cpu_switch(struct callout *c, struct callout_cpu *cc, int new_cpu)
{
	struct callout_cpu *new_cc;

	MPASS(c != NULL && cc != NULL);
	CC_LOCK_ASSERT(cc);

	/*
	 * Avoid interrupts and preemption firing after the callout cpu
	 * is blocked in order to avoid deadlocks as the new thread
	 * may be willing to acquire the callout cpu lock.
	 */
	c->c_cpu = CPUBLOCK;
	spinlock_enter();
	CC_UNLOCK(cc);
	new_cc = CC_CPU(new_cpu);
	CC_LOCK(new_cc);
	spinlock_exit();
	c->c_cpu = new_cpu;
	return (new_cc);
}
#endif

/*
 * kern_timeout_callwheel_init() - initialize previously reserved callwheel
 *				   space.
 *
 *	This code is called just once, after the space reserved for the
 *	callout wheel has been finalized.
 */
void
kern_timeout_callwheel_init(void)
{
	callout_cpu_init(CC_CPU(timeout_cpu));
}

/*
 * Start standard softclock thread.
 */
static void
start_softclock(void *dummy)
{
	struct callout_cpu *cc;
#ifdef SMP
	int cpu;
#endif

	cc = CC_CPU(timeout_cpu);
	if (swi_add(&clk_intr_event, "clock", softclock, cc, SWI_CLOCK,
	    INTR_MPSAFE, &cc->cc_cookie))
		panic("died while creating standard software ithreads");
#ifdef SMP
	CPU_FOREACH(cpu) {
		if (cpu == timeout_cpu)
			continue;
		cc = CC_CPU(cpu);
		if (swi_add(NULL, "clock", softclock, cc, SWI_CLOCK,
		    INTR_MPSAFE, &cc->cc_cookie))
			panic("died while creating standard software ithreads");
		cc->cc_callout = NULL;	/* Only cpu0 handles timeout(). */
		cc->cc_callwheel = malloc(
		    sizeof(struct callout_tailq) * callwheelsize, M_CALLOUT,
		    M_WAITOK);
		callout_cpu_init(cc);
	}
#endif
}

SYSINIT(start_softclock, SI_SUB_SOFTINTR, SI_ORDER_FIRST, start_softclock, NULL);

static inline int
callout_hash(struct bintime *bt) 
{

	return (int) ((bt->sec<<10)+(bt->frac>>54));
} 

static inline int
get_bucket(struct bintime *bt)
{

	return callout_hash(bt) & callwheelmask;
}

void
callout_process(struct bintime *now)
{
	struct bintime max, min, next, tmp_max, tmp_min;
	struct callout *tmp;
	struct callout_cpu *cc;
	struct callout_tailq *sc;
	int cpu, depth_dir, first, future, mpcalls_dir, last, lockcalls_dir,
	    need_softclock; 

	/*
	 * Process callouts at a very low cpu priority, so we don't keep the
	 * relatively high clock interrupt priority any longer than necessary.
	 */
	need_softclock = 0;
	depth_dir = 0;
	mpcalls_dir = 0;
	lockcalls_dir = 0;
	cc = CC_SELF();
	mtx_lock_spin_flags(&cc->cc_lock, MTX_QUIET);
	cpu = curcpu;
	first = callout_hash(&cc->cc_lastscan);
	last = callout_hash(now);
	/* 
	 * Check if we wrapped around the entire wheel from the last scan.
	 * In case, we need to scan entirely the wheel for pending callouts.
	 */
	last = (last - first >= callwheelsize) ? (first - 1) & callwheelmask : 
	    last & callwheelmask;
	first &= callwheelmask;
	for (;;) {	
		sc = &cc->cc_callwheel[first];
		tmp = TAILQ_FIRST(sc);
		while (tmp != NULL) {	
			next = tmp->c_time;
			bintime_sub(&next, &tmp->c_precision);
			if (bintime_cmp(&next, now, <=)) {
				/* 
				 * Consumer told us the callout may be run
				 * directly from hardware interrupt context.
				 */
				if (tmp->c_flags & CALLOUT_DIRECT) {
					++depth_dir;
					TAILQ_REMOVE(sc, tmp, c_links.tqe);
					tmp = softclock_call_cc(tmp, cc, 
					    &mpcalls_dir, &lockcalls_dir,
					     NULL, 1);
				} else {
					TAILQ_INSERT_TAIL(&cc->cc_expireq, 
					    tmp, c_staiter);
					TAILQ_REMOVE(sc, tmp, c_links.tqe);
					tmp->c_flags |= CALLOUT_PROCESSED;
					need_softclock = 1;
					tmp = TAILQ_NEXT(tmp, c_links.tqe);
				}
			}	
			else
				tmp = TAILQ_NEXT(tmp, c_links.tqe);
		}	
		if (first == last)
			break;
		first = (first + 1) & callwheelmask;
	}
	future = (last + hz / 4) & callwheelmask; 
	max.sec = min.sec = TIME_T_MAX;
	max.frac = min.frac = UINT64_MAX;
	/* 
	 * Look for the first bucket in the future that contains some event,
	 * up to some point,  so that we can look for aggregation. 
	 */ 
	for (;;) { 
		sc = &cc->cc_callwheel[last];
		TAILQ_FOREACH(tmp, sc, c_links.tqe) {
			tmp_max = tmp_min = tmp->c_time; 
			bintime_add(&tmp_max, &tmp->c_precision);
			bintime_sub(&tmp_min, &tmp->c_precision);
			/*
			 * This is the fist event we're going to process or 
			 * event maximal time is less than present minimal. 
			 * In both cases, take it.
			 */
			 if (bintime_cmp(&tmp_max, &min, <)) {
				max = tmp_max;
				min = tmp_min;
				continue;	
			}
			/*
			 * Event minimal time is bigger than present maximal  
		 	 * time, so it cannot be aggregated.
			 */
			if (bintime_cmp(&tmp_min, &max, >))
				continue;
			/*
			 * If neither of the two previous happened, just take 
			 * the intersection of events.
			 */	
			min = (bintime_cmp(&tmp_min, &min, >)) ? tmp_min : min;
			max = (bintime_cmp(&tmp_max, &max, >)) ? tmp_max : max;
		}		
		if (last == future || max.sec != TIME_T_MAX) 
			break;
		last = (last + 1) & callwheelmask;
	}
	if (max.sec == TIME_T_MAX) { 
		next.sec = 0;	
		next.frac = (uint64_t)1 << (64 - 2);	
		bintime_add(&next, now);
	} else {
		/*
		 * Now that we found something to aggregate, schedule an  
		 * interrupt in the middle of the previously calculated range.
	 	 */
		bintime_add(&max, &min);
		next = max;
		next.frac >>= 1;
		if (next.sec & 1)	
			next.frac |= ((uint64_t)1 << 63);
		next.sec >>= 1;
	}
	cc->cc_firstevent = next;
	if (callout_new_inserted != NULL) 
		(*callout_new_inserted)(cpu, next);
	cc->cc_lastscan = *now;
#ifdef CALLOUT_PROFILING
	avg_depth_dir += (depth_dir * 1000 - avg_depth_dir) >> 8;
	avg_mpcalls_dir += (mpcalls_dir * 1000 - avg_mpcalls_dir) >> 8;
	avg_lockcalls_dir += (lockcalls_dir * 1000 - avg_lockcalls_dir) >> 8;
#endif
	mtx_unlock_spin_flags(&cc->cc_lock, MTX_QUIET);
	/*
	 * swi_sched acquires the thread lock, so we don't want to call it
	 * with cc_lock held; incorrect locking order.
	 */
	if (need_softclock) {
		swi_sched(cc->cc_cookie, 0);
	}
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
			while (c->c_cpu == CPUBLOCK)
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

static void
callout_cc_add(struct callout *c, struct callout_cpu *cc, 
    struct bintime to_bintime, void (*func)(void *), void *arg, int cpu, 
    int flags)
{
	struct bintime bt;
	int bucket, r_shift, r_val;	
	
	CC_LOCK_ASSERT(cc);
	if (bintime_cmp(&to_bintime, &cc->cc_lastscan, <)) 
		to_bintime = cc->cc_lastscan;
	c->c_arg = arg;
	c->c_flags |= (CALLOUT_ACTIVE | CALLOUT_PENDING);
	if (flags & C_DIRECT_EXEC)
		c->c_flags |= CALLOUT_DIRECT;
	c->c_flags &= ~CALLOUT_PROCESSED;
	c->c_func = func;
	c->c_time = to_bintime;
	bintime_clear(&c->c_precision);
	if (flags & C_PRECISION) {  
		r_shift = ((flags >> 2) & PRECISION_RANGE);
		r_val = (r_shift != 0) ? (uint64_t)1 << (64 - r_shift) : 0;
		/* 
		 * Round as far as precision specified is coarse (up to 8ms).
		 * In order to play safe, round to to half of the interval and
		 * set half precision.
		 */	
		if (r_shift < 6) {
			r_val = (r_shift != 0) ? r_val >> 2 : 
			    ((uint64_t)1 << (64 - 1)) - 1;
			/*
			 * Round only if c_time is not a multiple of the
			 * rounding factor.
			 */
			if ((c->c_time.frac & r_val) != r_val) {
				c->c_time.frac |= r_val - 1;
				c->c_time.frac += 1;
				if (c->c_time.frac == 0)
					c->c_time.sec += 1;
			}
		}
		c->c_precision.frac = r_val;
		CTR6(KTR_CALLOUT, "rounding %d.%08x%08x to %d.%08x%08x", 
		    to_bintime.sec, (u_int) (to_bintime.frac >> 32), 
		    (u_int) (to_bintime.frac & 0xffffffff), c->c_time.sec, 
		    (u_int) (c->c_time.frac >> 32), 
		    (u_int) (c->c_time.frac & 0xffffffff)); 
	} 
	bucket = get_bucket(&c->c_time);
	TAILQ_INSERT_TAIL(&cc->cc_callwheel[bucket], c, c_links.tqe); 
	/*
	 * Inform the eventtimers(4) subsystem there's a new callout 
	 * that has been inserted, but only if really required.
	 */
	bt = c->c_time;
	bintime_add(&bt, &c->c_precision); 
	if (callout_new_inserted != NULL && 
	    (bintime_cmp(&bt, &cc->cc_firstevent, <) ||
	    (cc->cc_firstevent.sec == 0 && cc->cc_firstevent.frac == 0))) {
		cc->cc_firstevent = c->c_time;
		(*callout_new_inserted)(cpu, c->c_time);
	}
}

static void
callout_cc_del(struct callout *c, struct callout_cpu *cc, int direct)
{
	if (direct && cc->cc_exec_next_dir == c)
		cc->cc_exec_next_dir = TAILQ_NEXT(c, c_links.tqe);
	else if (!direct && cc->cc_exec_next == c)
		cc->cc_exec_next = TAILQ_NEXT(c, c_staiter);
	if (c->c_flags & CALLOUT_LOCAL_ALLOC) {
		c->c_func = NULL;
		SLIST_INSERT_HEAD(&cc->cc_callfree, c, c_links.sle);
	}
}

static struct callout *
softclock_call_cc(struct callout *c, struct callout_cpu *cc, int *mpcalls,
    int *lockcalls, int *gcalls, int direct)
{
	void (*c_func)(void *);
	void *c_arg;
	struct lock_class *class;
	struct lock_object *c_lock;
	int c_flags, flags, sharedlock;
#ifdef SMP
	struct callout_cpu *new_cc;
	void (*new_func)(void *);
	void *new_arg;
	int new_cpu;
	struct bintime new_time;
#endif
#ifdef DIAGNOSTIC
	struct bintime bt1, bt2;
	struct timespec ts2;
	static uint64_t maxdt = 36893488147419102LL;	/* 2 msec */
	static timeout_t *lastfunc;
#endif

	if (direct) 
		cc->cc_exec_next_dir = TAILQ_NEXT(c, c_links.tqe);
	else {
		if ((c->c_flags & CALLOUT_PROCESSED) == 0) 	
			cc->cc_exec_next = TAILQ_NEXT(c, c_links.tqe);
		else 
			cc->cc_exec_next = TAILQ_NEXT(c, c_staiter);
	}
	class = (c->c_lock != NULL) ? LOCK_CLASS(c->c_lock) : NULL;
	sharedlock = (c->c_flags & CALLOUT_SHAREDLOCK) ? 0 : 1;
	c_lock = c->c_lock;
	c_func = c->c_func;
	c_arg = c->c_arg;
	c_flags = c->c_flags;
	if (c->c_flags & CALLOUT_LOCAL_ALLOC)
		c->c_flags = CALLOUT_LOCAL_ALLOC;
	else
		c->c_flags &= ~CALLOUT_PENDING;
	cc->cc_exec_entity[direct].cc_curr = c;
	cc->cc_exec_entity[direct].cc_cancel = 0;
	CC_UNLOCK(cc);
	if (c_lock != NULL) {
		class->lc_lock(c_lock, sharedlock);
		/*
		 * The callout may have been cancelled
		 * while we switched locks.
		 */
		if (cc->cc_exec_entity[direct].cc_cancel) { 
			class->lc_unlock(c_lock);
			goto skip;
		}
		/* The callout cannot be stopped now. */
		cc->cc_exec_entity[direct].cc_cancel = 1;
		/* 
		 * In case we're processing a direct callout we
		 * can't hold giant because holding a sleep mutex
		 * from hardware interrupt context is not allowed.
		 */
		if ((c_lock == &Giant.lock_object) && gcalls != NULL) {
			(*gcalls)++;
			CTR3(KTR_CALLOUT, "callout %p func %p arg %p",
			    c, c_func, c_arg);
		} else {
			(*lockcalls)++;
			CTR3(KTR_CALLOUT, "callout lock %p func %p arg %p",
			    c, c_func, c_arg);
		}
	} else {
		(*mpcalls)++;
		CTR3(KTR_CALLOUT, "callout mpsafe %p func %p arg %p",
		    c, c_func, c_arg);
	}
#ifdef DIAGNOSTIC
	binuptime(&bt1);
#endif
	if (!direct)
		THREAD_NO_SLEEPING();
	SDT_PROBE(callout_execute, kernel, , callout_start, c, 0, 0, 0, 0);
	c_func(c_arg);
	SDT_PROBE(callout_execute, kernel, , callout_end, c, 0, 0, 0, 0);
	if (!direct)
		THREAD_SLEEPING_OK();
#ifdef DIAGNOSTIC
	binuptime(&bt2);
	bintime_sub(&bt2, &bt1);
	if (bt2.frac > maxdt) {
		if (lastfunc != c_func || bt2.frac > maxdt * 2) {
			bintime2timespec(&bt2, &ts2);
			printf(
		"Expensive timeout(9) function: %p(%p) %jd.%09ld s\n",
			    c_func, c_arg, (intmax_t)ts2.tv_sec, ts2.tv_nsec);
		}
		maxdt = bt2.frac;
		lastfunc = c_func;
	}
#endif
	CTR1(KTR_CALLOUT, "callout %p finished", c);
	if ((c_flags & CALLOUT_RETURNUNLOCKED) == 0)
		class->lc_unlock(c_lock);
skip:
	CC_LOCK(cc);
	/*
	 * If the current callout is locally allocated (from
	 * timeout(9)) then put it on the freelist.
	 *
	 * Note: we need to check the cached copy of c_flags because
	 * if it was not local, then it's not safe to deref the
	 * callout pointer.
	 */
	if (c_flags & CALLOUT_LOCAL_ALLOC) {
		KASSERT(c->c_flags == CALLOUT_LOCAL_ALLOC,
		    ("corrupted callout"));
		c->c_func = NULL;
		SLIST_INSERT_HEAD(&cc->cc_callfree, c, c_links.sle);
	}
	cc->cc_exec_entity[direct].cc_curr = NULL;
	if (cc->cc_exec_entity[direct].cc_waiting) { 
		/*
		 * There is someone waiting for the
		 * callout to complete.
		 * If the callout was scheduled for
		 * migration just cancel it.
		 */
		if (cc_cme_migrating(cc, direct))
			cc_cme_cleanup(cc, direct);
		cc->cc_exec_entity[direct].cc_waiting = 0;
		CC_UNLOCK(cc);
		wakeup(&cc->cc_exec_entity[direct].cc_waiting);
		CC_LOCK(cc);
	} else if (cc_cme_migrating(cc, direct)) {
#ifdef SMP
		/*
		 * If the callout was scheduled for
		 * migration just perform it now.
		 */
		new_cpu = cc->cc_exec_entity[direct].ce_migration_cpu;
		new_time = cc->cc_exec_entity[direct].ce_migration_time;
		new_func = cc->cc_exec_entity[direct].ce_migration_func;
		new_arg = cc->cc_exec_entity[direct].ce_migration_arg;
		cc_cme_cleanup(cc, direct);

		/*
		 * Handle deferred callout stops
		 */
		if ((c->c_flags & CALLOUT_DFRMIGRATION) == 0) {
			CTR3(KTR_CALLOUT,
			     "deferred cancelled %p func %p arg %p",
			     c, new_func, new_arg);
			callout_cc_del(c, cc, direct);
			goto nextc;
		}

		c->c_flags &= ~CALLOUT_DFRMIGRATION;

		/*
		 * It should be assert here that the
		 * callout is not destroyed but that
		 * is not easy.
		 */
		new_cc = callout_cpu_switch(c, cc, new_cpu);
		flags = (direct) ? C_DIRECT_EXEC : 0;
		callout_cc_add(c, new_cc, new_time, new_func, new_arg,
		    new_cpu, flags);
		CC_UNLOCK(new_cc);
		CC_LOCK(cc);
#else
		panic("migration should not happen");
#endif
	}
#ifdef SMP
nextc:
#endif
	return cc->cc_exec_entity[direct].cc_next;
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
	int depth, gcalls, lockcalls, mpcalls;

	depth = 0;
	mpcalls = 0;
	lockcalls = 0;
	gcalls = 0;
	cc = (struct callout_cpu *)arg;
	CC_LOCK(cc);
	c = TAILQ_FIRST(&cc->cc_expireq);
	while (c != NULL) {
		++depth;
		TAILQ_REMOVE(&cc->cc_expireq, c, c_staiter);
		c = softclock_call_cc(c, cc, &mpcalls,
		    &lockcalls, &gcalls, 0);
	}
#ifdef CALLOUT_PROFILING
	avg_depth += (depth * 1000 - avg_depth) >> 8;
	avg_mpcalls += (mpcalls * 1000 - avg_mpcalls) >> 8;
	avg_lockcalls += (lockcalls * 1000 - avg_lockcalls) >> 8;
	avg_gcalls += (gcalls * 1000 - avg_gcalls) >> 8;
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
timeout(ftn, arg, to_ticks)
	timeout_t *ftn;
	void *arg;
	int to_ticks;
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
	callout_reset(new, to_ticks, ftn, arg);
	handle.callout = new;
	CC_UNLOCK(cc);

	return (handle);
}

void
untimeout(ftn, arg, handle)
	timeout_t *ftn;
	void *arg;
	struct callout_handle handle;
{
	struct callout_cpu *cc;

	/*
	 * Check for a handle that was initialized
	 * by callout_handle_init, but never used
	 * for a real timeout.
	 */
	if (handle.callout == NULL)
		return;

	cc = callout_lock(handle.callout);
	if (handle.callout->c_func == ftn && handle.callout->c_arg == arg)
		callout_stop(handle.callout);
	CC_UNLOCK(cc);
}

void
callout_handle_init(struct callout_handle *handle)
{
	handle->callout = NULL;
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
_callout_reset_on(struct callout *c, struct bintime *bt, int to_ticks, 
    void (*ftn)(void *), void *arg, int cpu, int flags)
{
	struct bintime now, to_bt;
	struct callout_cpu *cc;
	int bucket, cancelled, direct;

	cancelled = 0;
	if (bt == NULL) {	
		FREQ2BT(hz,&to_bt);
		getbinuptime(&now);
		bintime_mul(&to_bt,to_ticks);
		bintime_add(&to_bt,&now);
	} else 
		to_bt = *bt;
	/*
	 * Don't allow migration of pre-allocated callouts lest they
	 * become unbalanced.
	 */
	if (c->c_flags & CALLOUT_LOCAL_ALLOC)
		cpu = c->c_cpu;
	direct = c->c_flags & CALLOUT_DIRECT;
	cc = callout_lock(c);
	if (cc->cc_exec_entity[direct].cc_curr == c) {	
		/*
		 * We're being asked to reschedule a callout which is
		 * currently in progress.  If there is a lock then we
		 * can cancel the callout if it has not really started.
		 */
		if (c->c_lock != NULL && 
		    !cc->cc_exec_entity[direct].cc_cancel) 
			cancelled = 
			    cc->cc_exec_entity[direct].cc_cancel = 1;
		if (cc->cc_exec_entity[direct].cc_waiting) { 
			/*
			 * Someone has called callout_drain to kill this
			 * callout.  Don't reschedule.
			 */
			CTR4(KTR_CALLOUT, "%s %p func %p arg %p",
			    cancelled ? "cancelled" : "failed to cancel",
			    c, c->c_func, c->c_arg);
			CC_UNLOCK(cc);
			return (cancelled);
		}
	}
	if (c->c_flags & CALLOUT_PENDING) {
		if ((c->c_flags & CALLOUT_PROCESSED) == 0) {	
			if (cc->cc_exec_next == c)
				cc->cc_exec_next = TAILQ_NEXT(c, c_links.tqe);
			else if (cc->cc_exec_next_dir == c) 
				cc->cc_exec_next_dir = TAILQ_NEXT(c, 
				    c_links.tqe);
			bucket = get_bucket(&c->c_time);
			TAILQ_REMOVE(&cc->cc_callwheel[bucket], c,
			    c_links.tqe);
		} else {
			if (cc->cc_exec_next == c)
				cc->cc_exec_next = TAILQ_NEXT(c, c_staiter);
			TAILQ_REMOVE(&cc->cc_expireq, c, c_staiter);  
		}
		cancelled = 1;
		c->c_flags &= ~(CALLOUT_ACTIVE | CALLOUT_PENDING);
	}

#ifdef SMP
	/*
	 * If the callout must migrate try to perform it immediately.
	 * If the callout is currently running, just defer the migration
	 * to a more appropriate moment.
	 */
	if (c->c_cpu != cpu) {
		if (cc->cc_exec_entity[direct].cc_curr == c) {
			cc->cc_exec_entity[direct].ce_migration_cpu = cpu;
			cc->cc_exec_entity[direct].ce_migration_time 
			    = to_bt;
			cc->cc_exec_entity[direct].ce_migration_func = ftn;
			cc->cc_exec_entity[direct].ce_migration_arg = arg;
			c->c_flags |= CALLOUT_DFRMIGRATION;
			CTR6(KTR_CALLOUT,
		    "migration of %p func %p arg %p in %d.%08x to %u deferred",
			    c, c->c_func, c->c_arg, (int)(to_bt.sec), 
			    (u_int)(to_bt.frac >> 32), cpu);
			CC_UNLOCK(cc);
			return (cancelled);
		}
		cc = callout_cpu_switch(c, cc, cpu);
	}
#endif

	callout_cc_add(c, cc, to_bt, ftn, arg, cpu, flags);
	CTR6(KTR_CALLOUT, "%sscheduled %p func %p arg %p in %d.%08x",
	    cancelled ? "re" : "", c, c->c_func, c->c_arg, (int)(to_bt.sec), 
	    (u_int)(to_bt.frac >> 32));
	CC_UNLOCK(cc);

	return (cancelled);
}

/*
 * Common idioms that can be optimized in the future.
 */
int
callout_schedule_on(struct callout *c, int to_ticks, int cpu)
{
	return callout_reset_on(c, to_ticks, c->c_func, c->c_arg, cpu);
}

int
callout_schedule(struct callout *c, int to_ticks)
{
	return callout_reset_on(c, to_ticks, c->c_func, c->c_arg, c->c_cpu);
}

int
_callout_stop_safe(c, safe)
	struct	callout *c;
	int	safe;
{
	struct callout_cpu *cc, *old_cc;
	struct lock_class *class;
	int bucket, direct, sq_locked, use_lock;

	/*
	 * Some old subsystems don't hold Giant while running a callout_stop(),
	 * so just discard this check for the moment.
	 */
	if (!safe && c->c_lock != NULL) {
		if (c->c_lock == &Giant.lock_object)
			use_lock = mtx_owned(&Giant);
		else {
			use_lock = 1;
			class = LOCK_CLASS(c->c_lock);
			class->lc_assert(c->c_lock, LA_XLOCKED);
		}
	} else
		use_lock = 0;
	direct = c->c_flags & CALLOUT_DIRECT;
	sq_locked = 0;
	old_cc = NULL;
again:
	cc = callout_lock(c);

	/*
	 * If the callout was migrating while the callout cpu lock was
	 * dropped,  just drop the sleepqueue lock and check the states
	 * again.
	 */
	if (sq_locked != 0 && cc != old_cc) {
#ifdef SMP
		CC_UNLOCK(cc);
		sleepq_release(&old_cc->cc_exec_entity[direct].cc_waiting);
		sq_locked = 0;
		old_cc = NULL;
		goto again;
#else
		panic("migration should not happen");
#endif
	}

	/*
	 * If the callout isn't pending, it's not on the queue, so
	 * don't attempt to remove it from the queue.  We can try to
	 * stop it by other means however.
	 */
	if (!(c->c_flags & CALLOUT_PENDING)) {
		c->c_flags &= ~CALLOUT_ACTIVE;

		/*
		 * If it wasn't on the queue and it isn't the current
		 * callout, then we can't stop it, so just bail.
		 */
		if (cc->cc_exec_entity[direct].cc_curr != c) {
			CTR3(KTR_CALLOUT, "failed to stop %p func %p arg %p",
			    c, c->c_func, c->c_arg);
			CC_UNLOCK(cc);
			if (sq_locked)
				sleepq_release(
				    &cc->cc_exec_entity[direct].cc_waiting);
			return (0);
		}

		if (safe) {
			/*
			 * The current callout is running (or just
			 * about to run) and blocking is allowed, so
			 * just wait for the current invocation to
			 * finish.
			 */
			while (cc->cc_exec_entity[direct].cc_curr == c) {
				/*
				 * Use direct calls to sleepqueue interface
				 * instead of cv/msleep in order to avoid
				 * a LOR between cc_lock and sleepqueue
				 * chain spinlocks.  This piece of code
				 * emulates a msleep_spin() call actually.
				 *
				 * If we already have the sleepqueue chain
				 * locked, then we can safely block.  If we
				 * don't already have it locked, however,
				 * we have to drop the cc_lock to lock
				 * it.  This opens several races, so we
				 * restart at the beginning once we have
				 * both locks.  If nothing has changed, then
				 * we will end up back here with sq_locked
				 * set.
				 */
				if (!sq_locked) {
					CC_UNLOCK(cc);
					sleepq_lock(
					&cc->cc_exec_entity[direct].cc_waiting);
					sq_locked = 1;
					old_cc = cc;
					goto again;
				}

				/*
				 * Migration could be cancelled here, but
				 * as long as it is still not sure when it
				 * will be packed up, just let softclock()
				 * take care of it.
				 */
				cc->cc_exec_entity[direct].cc_waiting = 1;
				DROP_GIANT();
				CC_UNLOCK(cc);
				sleepq_add(
				    &cc->cc_exec_entity[direct].cc_waiting,
			    	    &cc->cc_lock.lock_object, "codrain",
				    SLEEPQ_SLEEP, 0);
				sleepq_wait(
				    &cc->cc_exec_entity[direct].cc_waiting,
					     0);
				sq_locked = 0;
				old_cc = NULL;

				/* Reacquire locks previously released. */
				PICKUP_GIANT();
				CC_LOCK(cc);
			}
		} else if (use_lock && 
			    !cc->cc_exec_entity[direct].cc_cancel) { 
			/*
			 * The current callout is waiting for its
			 * lock which we hold.  Cancel the callout
			 * and return.  After our caller drops the
			 * lock, the callout will be skipped in
			 * softclock().
			 */
			cc->cc_exec_entity[direct].cc_cancel = 1;
			CTR3(KTR_CALLOUT, "cancelled %p func %p arg %p",
			    c, c->c_func, c->c_arg);
			KASSERT(!cc_cme_migrating(cc, direct),
			    ("callout wrongly scheduled for migration"));
			CC_UNLOCK(cc);
			KASSERT(!sq_locked, ("sleepqueue chain locked"));
			return (1);
		} else if ((c->c_flags & CALLOUT_DFRMIGRATION) != 0) {
			c->c_flags &= ~CALLOUT_DFRMIGRATION;
			CTR3(KTR_CALLOUT, "postponing stop %p func %p arg %p",
			    c, c->c_func, c->c_arg);
			CC_UNLOCK(cc);
			return (1);
		}
		CTR3(KTR_CALLOUT, "failed to stop %p func %p arg %p",
		    c, c->c_func, c->c_arg);
		CC_UNLOCK(cc);
		KASSERT(!sq_locked, ("sleepqueue chain still locked"));
		return (0);
	}
	if (sq_locked)
		sleepq_release(&cc->cc_exec_entity[direct].cc_waiting);
	c->c_flags &= ~(CALLOUT_ACTIVE | CALLOUT_PENDING);

	CTR3(KTR_CALLOUT, "cancelled %p func %p arg %p",
	    c, c->c_func, c->c_arg);
	if ((c->c_flags & CALLOUT_PROCESSED) == 0) {
		bucket = get_bucket(&c->c_time);
		TAILQ_REMOVE(&cc->cc_callwheel[bucket], c,
		    c_links.tqe);
	} else
		TAILQ_REMOVE(&cc->cc_expireq, c, c_staiter); 
	callout_cc_del(c, cc, direct);

	CC_UNLOCK(cc);
	return (1);
}

void
callout_init(c, mpsafe)
	struct	callout *c;
	int mpsafe;
{
	bzero(c, sizeof *c);
	if (mpsafe) {
		c->c_lock = NULL;
		c->c_flags = CALLOUT_RETURNUNLOCKED;
	} else {
		c->c_lock = &Giant.lock_object;
		c->c_flags = 0;
	}
	c->c_cpu = timeout_cpu;
}

void
_callout_init_lock(c, lock, flags)
	struct	callout *c;
	struct	lock_object *lock;
	int flags;
{
	bzero(c, sizeof *c);
	c->c_lock = lock;
	KASSERT((flags & ~(CALLOUT_RETURNUNLOCKED | CALLOUT_SHAREDLOCK)) == 0,
	    ("callout_init_lock: bad flags %d", flags));
	KASSERT(lock != NULL || (flags & CALLOUT_RETURNUNLOCKED) == 0,
	    ("callout_init_lock: CALLOUT_RETURNUNLOCKED with no lock"));
	KASSERT(lock == NULL || !(LOCK_CLASS(lock)->lc_flags &
	    (LC_SPINLOCK | LC_SLEEPABLE)), ("%s: invalid lock class",
	    __func__));
	c->c_flags = flags & (CALLOUT_RETURNUNLOCKED | CALLOUT_SHAREDLOCK);
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
 * firing, which seemed independant on whether the suspend was 2 hours or
 * 2 days.  Your milage may vary.   - Ken Key <key@cs.utk.edu>
 */
void
adjust_timeout_calltodo(time_change)
    struct timeval *time_change;
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
		delta_ticks = (time_change->tv_sec * 1000000 +
			       time_change->tv_usec + (tick - 1)) / tick + 1;
	else if (time_change->tv_sec <= LONG_MAX / hz)
		delta_ticks = time_change->tv_sec * hz +
			      (time_change->tv_usec + (tick - 1)) / tick + 1;
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
