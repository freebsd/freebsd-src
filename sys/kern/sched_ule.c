/*-
 * Copyright (c) 2002-2003, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>

#define KTR_ULE         KTR_NFS

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
/* XXX This is bogus compatability crap for ps */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0, "SCHED");

static int sched_strict;
SYSCTL_INT(_kern_sched, OID_AUTO, strict, CTLFLAG_RD, &sched_strict, 0, "");

static int slice_min = 1;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_min, CTLFLAG_RW, &slice_min, 0, "");

static int slice_max = 10;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_max, CTLFLAG_RW, &slice_max, 0, "");

int realstathz;
int tickincr = 1;

#ifdef SMP
/* Callout to handle load balancing SMP systems. */
static struct callout kseq_lb_callout;
#endif

/*
 * These datastructures are allocated within their parent datastructure but
 * are scheduler specific.
 */

struct ke_sched {
	int		ske_slice;
	struct runq	*ske_runq;
	/* The following variables are only used for pctcpu calculation */
	int		ske_ltick;	/* Last tick that we were running on */
	int		ske_ftick;	/* First tick that we were running on */
	int		ske_ticks;	/* Tick count */
	/* CPU that we have affinity for. */
	u_char		ske_cpu;
};
#define	ke_slice	ke_sched->ske_slice
#define	ke_runq		ke_sched->ske_runq
#define	ke_ltick	ke_sched->ske_ltick
#define	ke_ftick	ke_sched->ske_ftick
#define	ke_ticks	ke_sched->ske_ticks
#define	ke_cpu		ke_sched->ske_cpu

struct kg_sched {
	int	skg_slptime;		/* Number of ticks we vol. slept */
	int	skg_runtime;		/* Number of ticks we were running */
};
#define	kg_slptime	kg_sched->skg_slptime
#define	kg_runtime	kg_sched->skg_runtime

struct td_sched {
	int	std_slptime;
};
#define	td_slptime	td_sched->std_slptime

struct td_sched td_sched;
struct ke_sched ke_sched;
struct kg_sched kg_sched;

struct ke_sched *kse0_sched = &ke_sched;
struct kg_sched *ksegrp0_sched = &kg_sched;
struct p_sched *proc0_sched = NULL;
struct td_sched *thread0_sched = &td_sched;

/*
 * This priority range has 20 priorities on either end that are reachable
 * only through nice values.
 *
 * PRI_RANGE:	Total priority range for timeshare threads.
 * PRI_NRESV:	Reserved priorities for nice.
 * PRI_BASE:	The start of the dynamic range.
 * DYN_RANGE:	Number of priorities that are available int the dynamic
 *		priority range.
 */
#define	SCHED_PRI_RANGE		(PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1)
#define	SCHED_PRI_NRESV		PRIO_TOTAL
#define	SCHED_PRI_NHALF		(PRIO_TOTAL / 2)
#define	SCHED_PRI_NTHRESH	(SCHED_PRI_NHALF - 1)
#define	SCHED_PRI_BASE		((SCHED_PRI_NRESV / 2) + PRI_MIN_TIMESHARE)
#define	SCHED_DYN_RANGE		(SCHED_PRI_RANGE - SCHED_PRI_NRESV)
#define	SCHED_PRI_INTERACT(score)					\
    ((score) * SCHED_DYN_RANGE / SCHED_INTERACT_MAX)

/*
 * These determine the interactivity of a process.
 *
 * SLP_RUN_MAX:	Maximum amount of sleep time + run time we'll accumulate
 *		before throttling back.
 * SLP_RUN_THROTTLE:	Divisor for reducing slp/run time.
 * INTERACT_MAX:	Maximum interactivity value.  Smaller is better.
 * INTERACT_THRESH:	Threshhold for placement on the current runq.
 */
#define	SCHED_SLP_RUN_MAX	((hz / 10) << 10)
#define	SCHED_SLP_RUN_THROTTLE	(10)
#define	SCHED_INTERACT_MAX	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_MAX / 2)
#define	SCHED_INTERACT_THRESH	(20)

/*
 * These parameters and macros determine the size of the time slice that is
 * granted to each thread.
 *
 * SLICE_MIN:	Minimum time slice granted, in units of ticks.
 * SLICE_MAX:	Maximum time slice granted.
 * SLICE_RANGE:	Range of available time slices scaled by hz.
 * SLICE_SCALE:	The number slices granted per val in the range of [0, max].
 * SLICE_NICE:  Determine the amount of slice granted to a scaled nice.
 */
#define	SCHED_SLICE_MIN			(slice_min)
#define	SCHED_SLICE_MAX			(slice_max)
#define	SCHED_SLICE_RANGE		(SCHED_SLICE_MAX - SCHED_SLICE_MIN + 1)
#define	SCHED_SLICE_SCALE(val, max)	(((val) * SCHED_SLICE_RANGE) / (max))
#define	SCHED_SLICE_NICE(nice)						\
    (SCHED_SLICE_MAX - SCHED_SLICE_SCALE((nice), SCHED_PRI_NTHRESH))

/*
 * This macro determines whether or not the kse belongs on the current or
 * next run queue.
 * 
 * XXX nice value should effect how interactive a kg is.
 */
#define	SCHED_INTERACTIVE(kg)						\
    (sched_interact_score(kg) < SCHED_INTERACT_THRESH)
#define	SCHED_CURR(kg, ke)						\
    (ke->ke_thread->td_priority < PRI_MIN_TIMESHARE || SCHED_INTERACTIVE(kg))

/*
 * Cpu percentage computation macros and defines.
 *
 * SCHED_CPU_TIME:	Number of seconds to average the cpu usage across.
 * SCHED_CPU_TICKS:	Number of hz ticks to average the cpu usage across.
 */

#define	SCHED_CPU_TIME	10
#define	SCHED_CPU_TICKS	(hz * SCHED_CPU_TIME)

/*
 * kseq - per processor runqs and statistics.
 */

#define	KSEQ_NCLASS	(PRI_IDLE + 1)	/* Number of run classes. */

struct kseq {
	struct runq	ksq_idle;		/* Queue of IDLE threads. */
	struct runq	ksq_timeshare[2];	/* Run queues for !IDLE. */
	struct runq	*ksq_next;		/* Next timeshare queue. */
	struct runq	*ksq_curr;		/* Current queue. */
	int		ksq_loads[KSEQ_NCLASS];	/* Load for each class */
	int		ksq_load;		/* Aggregate load. */
	short		ksq_nice[PRIO_TOTAL + 1]; /* KSEs in each nice bin. */
	short		ksq_nicemin;		/* Least nice. */
#ifdef SMP
	unsigned int	ksq_rslices;	/* Slices on run queue */
#endif
};

/*
 * One kse queue per processor.
 */
#ifdef SMP
struct kseq	kseq_cpu[MAXCPU];
#define	KSEQ_SELF()	(&kseq_cpu[PCPU_GET(cpuid)])
#define	KSEQ_CPU(x)	(&kseq_cpu[(x)])
#else
struct kseq	kseq_cpu;
#define	KSEQ_SELF()	(&kseq_cpu)
#define	KSEQ_CPU(x)	(&kseq_cpu)
#endif

static void sched_slice(struct kse *ke);
static void sched_priority(struct ksegrp *kg);
static int sched_interact_score(struct ksegrp *kg);
void sched_pctcpu_update(struct kse *ke);
int sched_pickcpu(void);

/* Operations on per processor queues */
static struct kse * kseq_choose(struct kseq *kseq);
static void kseq_setup(struct kseq *kseq);
static void kseq_add(struct kseq *kseq, struct kse *ke);
static void kseq_rem(struct kseq *kseq, struct kse *ke);
static void kseq_nice_add(struct kseq *kseq, int nice);
static void kseq_nice_rem(struct kseq *kseq, int nice);
void kseq_print(int cpu);
#ifdef SMP
struct kseq * kseq_load_highest(void);
void kseq_balance(void *arg);
void kseq_move(struct kseq *from, int cpu);
#endif

void
kseq_print(int cpu)
{
	struct kseq *kseq;
	int i;

	kseq = KSEQ_CPU(cpu);

	printf("kseq:\n");
	printf("\tload:           %d\n", kseq->ksq_load);
	printf("\tload ITHD:      %d\n", kseq->ksq_loads[PRI_ITHD]);
	printf("\tload REALTIME:  %d\n", kseq->ksq_loads[PRI_REALTIME]);
	printf("\tload TIMESHARE: %d\n", kseq->ksq_loads[PRI_TIMESHARE]);
	printf("\tload IDLE:      %d\n", kseq->ksq_loads[PRI_IDLE]);
	printf("\tnicemin:\t%d\n", kseq->ksq_nicemin);
	printf("\tnice counts:\n");
	for (i = 0; i < PRIO_TOTAL + 1; i++)
		if (kseq->ksq_nice[i])
			printf("\t\t%d = %d\n",
			    i - SCHED_PRI_NHALF, kseq->ksq_nice[i]);
}

static void
kseq_add(struct kseq *kseq, struct kse *ke)
{
	mtx_assert(&sched_lock, MA_OWNED);
	kseq->ksq_loads[PRI_BASE(ke->ke_ksegrp->kg_pri_class)]++;
	kseq->ksq_load++;
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
	CTR6(KTR_ULE, "Add kse %p to %p (slice: %d, pri: %d, nice: %d(%d))",
	    ke, ke->ke_runq, ke->ke_slice, ke->ke_thread->td_priority,
	    ke->ke_ksegrp->kg_nice, kseq->ksq_nicemin);
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
		kseq_nice_add(kseq, ke->ke_ksegrp->kg_nice);
#ifdef SMP
	kseq->ksq_rslices += ke->ke_slice;
#endif
}

static void
kseq_rem(struct kseq *kseq, struct kse *ke)
{
	mtx_assert(&sched_lock, MA_OWNED);
	kseq->ksq_loads[PRI_BASE(ke->ke_ksegrp->kg_pri_class)]--;
	kseq->ksq_load--;
	ke->ke_runq = NULL;
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
		kseq_nice_rem(kseq, ke->ke_ksegrp->kg_nice);
#ifdef SMP
	kseq->ksq_rslices -= ke->ke_slice;
#endif
}

static void
kseq_nice_add(struct kseq *kseq, int nice)
{
	mtx_assert(&sched_lock, MA_OWNED);
	/* Normalize to zero. */
	kseq->ksq_nice[nice + SCHED_PRI_NHALF]++;
	if (nice < kseq->ksq_nicemin || kseq->ksq_loads[PRI_TIMESHARE] == 1)
		kseq->ksq_nicemin = nice;
}

static void
kseq_nice_rem(struct kseq *kseq, int nice) 
{
	int n;

	mtx_assert(&sched_lock, MA_OWNED);
	/* Normalize to zero. */
	n = nice + SCHED_PRI_NHALF;
	kseq->ksq_nice[n]--;
	KASSERT(kseq->ksq_nice[n] >= 0, ("Negative nice count."));

	/*
	 * If this wasn't the smallest nice value or there are more in
	 * this bucket we can just return.  Otherwise we have to recalculate
	 * the smallest nice.
	 */
	if (nice != kseq->ksq_nicemin ||
	    kseq->ksq_nice[n] != 0 ||
	    kseq->ksq_loads[PRI_TIMESHARE] == 0)
		return;

	for (; n < SCHED_PRI_NRESV + 1; n++)
		if (kseq->ksq_nice[n]) {
			kseq->ksq_nicemin = n - SCHED_PRI_NHALF;
			return;
		}
}

#ifdef SMP
/*
 * kseq_balance is a simple CPU load balancing algorithm.  It operates by
 * finding the least loaded and most loaded cpu and equalizing their load
 * by migrating some processes.
 *
 * Dealing only with two CPUs at a time has two advantages.  Firstly, most
 * installations will only have 2 cpus.  Secondly, load balancing too much at
 * once can have an unpleasant effect on the system.  The scheduler rarely has
 * enough information to make perfect decisions.  So this algorithm chooses
 * algorithm simplicity and more gradual effects on load in larger systems.
 *
 * It could be improved by considering the priorities and slices assigned to
 * each task prior to balancing them.  There are many pathological cases with
 * any approach and so the semi random algorithm below may work as well as any.
 *
 */
void
kseq_balance(void *arg)
{
	struct kseq *kseq;
	int high_load;
	int low_load;
	int high_cpu;
	int low_cpu;
	int move;
	int diff;
	int i;

	high_cpu = 0;
	low_cpu = 0;
	high_load = 0;
	low_load = -1;

	mtx_lock_spin(&sched_lock);
	for (i = 0; i < mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		kseq = KSEQ_CPU(i);
		if (kseq->ksq_load > high_load) {
			high_load = kseq->ksq_load;
			high_cpu = i;
		}
		if (low_load == -1 || kseq->ksq_load < low_load) {
			low_load = kseq->ksq_load;
			low_cpu = i;
		}
	}

	/*
	 * Nothing to do.
	 */
	if (high_load < 2 || low_load == high_load)
		goto out;

	diff = high_load - low_load;
	move = diff / 2;
	if (diff & 0x1)
		move++;

	for (i = 0; i < move; i++)
		kseq_move(KSEQ_CPU(high_cpu), low_cpu);

out:
	mtx_unlock_spin(&sched_lock);
	callout_reset(&kseq_lb_callout, hz, kseq_balance, NULL);

	return;
}

struct kseq *
kseq_load_highest(void)
{
	struct kseq *kseq;
	int load;
	int cpu;
	int i;

	mtx_assert(&sched_lock, MA_OWNED);
	cpu = 0;
	load = 0;

	for (i = 0; i < mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		kseq = KSEQ_CPU(i);
		if (kseq->ksq_load > load) {
			load = kseq->ksq_load;
			cpu = i;
		}
	}
	if (load > 1)
		return (KSEQ_CPU(cpu));

	return (NULL);
}

void
kseq_move(struct kseq *from, int cpu)
{
	struct kse *ke;

	ke = kseq_choose(from);
	runq_remove(ke->ke_runq, ke);
	ke->ke_state = KES_THREAD;
	kseq_rem(from, ke);

	ke->ke_cpu = cpu;
	sched_add(ke);
}
#endif

struct kse *
kseq_choose(struct kseq *kseq)
{
	struct kse *ke;
	struct runq *swap;

	mtx_assert(&sched_lock, MA_OWNED);
	swap = NULL;

	for (;;) {
		ke = runq_choose(kseq->ksq_curr);
		if (ke == NULL) {
			/*
			 * We already swaped once and didn't get anywhere.
			 */
			if (swap)
				break;
			swap = kseq->ksq_curr;
			kseq->ksq_curr = kseq->ksq_next;
			kseq->ksq_next = swap;
			continue;
		}
		/*
		 * If we encounter a slice of 0 the kse is in a
		 * TIMESHARE kse group and its nice was too far out
		 * of the range that receives slices. 
		 */
		if (ke->ke_slice == 0) {
			runq_remove(ke->ke_runq, ke);
			sched_slice(ke);
			ke->ke_runq = kseq->ksq_next;
			runq_add(ke->ke_runq, ke);
			continue;
		}
		return (ke);
	}

	return (runq_choose(&kseq->ksq_idle));
}

static void
kseq_setup(struct kseq *kseq)
{
	runq_init(&kseq->ksq_timeshare[0]);
	runq_init(&kseq->ksq_timeshare[1]);
	runq_init(&kseq->ksq_idle);

	kseq->ksq_curr = &kseq->ksq_timeshare[0];
	kseq->ksq_next = &kseq->ksq_timeshare[1];

	kseq->ksq_loads[PRI_ITHD] = 0;
	kseq->ksq_loads[PRI_REALTIME] = 0;
	kseq->ksq_loads[PRI_TIMESHARE] = 0;
	kseq->ksq_loads[PRI_IDLE] = 0;
	kseq->ksq_load = 0;
#ifdef SMP
	kseq->ksq_rslices = 0;
#endif
}

static void
sched_setup(void *dummy)
{
	int i;

	slice_min = (hz/100);
	slice_max = (hz/10);

	mtx_lock_spin(&sched_lock);
	/* init kseqs */
	for (i = 0; i < MAXCPU; i++)
		kseq_setup(KSEQ_CPU(i));

	kseq_add(KSEQ_SELF(), &kse0);
	mtx_unlock_spin(&sched_lock);
#ifdef SMP
	callout_init(&kseq_lb_callout, 1);
	kseq_balance(NULL);
#endif
}

/*
 * Scale the scheduling priority according to the "interactivity" of this
 * process.
 */
static void
sched_priority(struct ksegrp *kg)
{
	int pri;

	if (kg->kg_pri_class != PRI_TIMESHARE)
		return;

	pri = SCHED_PRI_INTERACT(sched_interact_score(kg));
	pri += SCHED_PRI_BASE;
	pri += kg->kg_nice;

	if (pri > PRI_MAX_TIMESHARE)
		pri = PRI_MAX_TIMESHARE;
	else if (pri < PRI_MIN_TIMESHARE)
		pri = PRI_MIN_TIMESHARE;

	kg->kg_user_pri = pri;

	return;
}

/*
 * Calculate a time slice based on the properties of the kseg and the runq
 * that we're on.  This is only for PRI_TIMESHARE ksegrps.
 */
static void
sched_slice(struct kse *ke)
{
	struct kseq *kseq;
	struct ksegrp *kg;

	kg = ke->ke_ksegrp;
	kseq = KSEQ_CPU(ke->ke_cpu);

	/*
	 * Rationale:
	 * KSEs in interactive ksegs get the minimum slice so that we
	 * quickly notice if it abuses its advantage.
	 *
	 * KSEs in non-interactive ksegs are assigned a slice that is
	 * based on the ksegs nice value relative to the least nice kseg
	 * on the run queue for this cpu.
	 *
	 * If the KSE is less nice than all others it gets the maximum
	 * slice and other KSEs will adjust their slice relative to
	 * this when they first expire.
	 *
	 * There is 20 point window that starts relative to the least
	 * nice kse on the run queue.  Slice size is determined by
	 * the kse distance from the last nice ksegrp.
	 *
	 * If you are outside of the window you will get no slice and
	 * you will be reevaluated each time you are selected on the
	 * run queue.
	 *	
	 */

	if (!SCHED_INTERACTIVE(kg)) {
		int nice;

		nice = kg->kg_nice + (0 - kseq->ksq_nicemin);
		if (kseq->ksq_loads[PRI_TIMESHARE] == 0 ||
		    kg->kg_nice < kseq->ksq_nicemin)
			ke->ke_slice = SCHED_SLICE_MAX;
		else if (nice <= SCHED_PRI_NTHRESH)
			ke->ke_slice = SCHED_SLICE_NICE(nice);
		else
			ke->ke_slice = 0;
	} else
		ke->ke_slice = SCHED_SLICE_MIN;

	CTR6(KTR_ULE,
	    "Sliced %p(%d) (nice: %d, nicemin: %d, load: %d, interactive: %d)",
	    ke, ke->ke_slice, kg->kg_nice, kseq->ksq_nicemin,
	    kseq->ksq_loads[PRI_TIMESHARE], SCHED_INTERACTIVE(kg));

	/*
	 * Check to see if we need to scale back the slp and run time
	 * in the kg.  This will cause us to forget old interactivity
	 * while maintaining the current ratio.
	 */
	if ((kg->kg_runtime + kg->kg_slptime) >  SCHED_SLP_RUN_MAX) {
		kg->kg_runtime /= SCHED_SLP_RUN_THROTTLE;
		kg->kg_slptime /= SCHED_SLP_RUN_THROTTLE;
	}
	CTR4(KTR_ULE, "Slp vs Run(2) %p (Slp %d, Run %d, Score %d)",
	    ke, kg->kg_slptime >> 10, kg->kg_runtime >> 10,
	    sched_interact_score(kg));

	return;
}

static int
sched_interact_score(struct ksegrp *kg)
{
	int div;

	if (kg->kg_runtime > kg->kg_slptime) {
		div = max(1, kg->kg_runtime / SCHED_INTERACT_HALF);
		return (SCHED_INTERACT_HALF +
		    (SCHED_INTERACT_HALF - (kg->kg_slptime / div)));
	} if (kg->kg_slptime > kg->kg_runtime) {
		div = max(1, kg->kg_slptime / SCHED_INTERACT_HALF);
		return (kg->kg_runtime / div);
	}

	/*
	 * This can happen if slptime and runtime are 0.
	 */
	return (0);

}

/*
 * This is only somewhat accurate since given many processes of the same
 * priority they will switch when their slices run out, which will be
 * at most SCHED_SLICE_MAX.
 */
int
sched_rr_interval(void)
{
	return (SCHED_SLICE_MAX);
}

void
sched_pctcpu_update(struct kse *ke)
{
	/*
	 * Adjust counters and watermark for pctcpu calc.
	 */

	/*
	 * Shift the tick count out so that the divide doesn't round away
	 * our results.
	 */
	ke->ke_ticks <<= 10;
	ke->ke_ticks = (ke->ke_ticks / (ke->ke_ltick - ke->ke_ftick)) *
		    SCHED_CPU_TICKS;
	ke->ke_ticks >>= 10;
	ke->ke_ltick = ticks;
	ke->ke_ftick = ke->ke_ltick - SCHED_CPU_TICKS;
}

#ifdef SMP
/* XXX Should be changed to kseq_load_lowest() */
int
sched_pickcpu(void)
{
	struct kseq *kseq;
	int load;
	int cpu;
	int i;

	mtx_assert(&sched_lock, MA_OWNED);
	if (!smp_started)
		return (0);

	load = 0;
	cpu = 0;

	for (i = 0; i < mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		kseq = KSEQ_CPU(i);
		if (kseq->ksq_load < load) {
			cpu = i;
			load = kseq->ksq_load;
		}
	}

	CTR1(KTR_RUNQ, "sched_pickcpu: %d", cpu);
	return (cpu);
}
#else
int
sched_pickcpu(void)
{
	return (0);
}
#endif

void
sched_prio(struct thread *td, u_char prio)
{
	struct kse *ke;
	struct runq *rq;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	td->td_priority = prio;

	if (TD_ON_RUNQ(td)) {
		rq = ke->ke_runq;

		runq_remove(rq, ke);
		runq_add(rq, ke);
	}
}

void
sched_switchout(struct thread *td)
{
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);

	ke = td->td_kse;

	td->td_last_kse = ke;
        td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
        td->td_flags &= ~TDF_NEEDRESCHED;

	if (TD_IS_RUNNING(td)) {
		/*
		 * This queue is always correct except for idle threads which
		 * have a higher priority due to priority propagation.
		 */
		if (ke->ke_ksegrp->kg_pri_class == PRI_IDLE &&
		    ke->ke_thread->td_priority > PRI_MIN_IDLE)
			ke->ke_runq = KSEQ_SELF()->ksq_curr;
		runq_add(ke->ke_runq, ke);
		/* setrunqueue(td); */
		return;
	}
	if (ke->ke_runq)
		kseq_rem(KSEQ_CPU(ke->ke_cpu), ke);
	/*
	 * We will not be on the run queue. So we must be
	 * sleeping or similar.
	 */
	if (td->td_proc->p_flag & P_SA)
		kse_reassign(ke);
}

void
sched_switchin(struct thread *td)
{
	/* struct kse *ke = td->td_kse; */
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_oncpu = PCPU_GET(cpuid);
}

void
sched_nice(struct ksegrp *kg, int nice)
{
	struct kse *ke;
	struct thread *td;
	struct kseq *kseq;

	PROC_LOCK_ASSERT(kg->kg_proc, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	/*
	 * We need to adjust the nice counts for running KSEs.
	 */
	if (kg->kg_pri_class == PRI_TIMESHARE)
		FOREACH_KSE_IN_GROUP(kg, ke) {
			if (ke->ke_state != KES_ONRUNQ &&
			    ke->ke_state != KES_THREAD)
				continue;
			kseq = KSEQ_CPU(ke->ke_cpu);
			kseq_nice_rem(kseq, kg->kg_nice);
			kseq_nice_add(kseq, nice);
		}
	kg->kg_nice = nice;
	sched_priority(kg);
	FOREACH_THREAD_IN_GROUP(kg, td)
		td->td_flags |= TDF_NEEDRESCHED;
}

void
sched_sleep(struct thread *td, u_char prio)
{
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_slptime = ticks;
	td->td_priority = prio;

	CTR2(KTR_ULE, "sleep kse %p (tick: %d)",
	    td->td_kse, td->td_slptime);
}

void
sched_wakeup(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);

	/*
	 * Let the kseg know how long we slept for.  This is because process
	 * interactivity behavior is modeled in the kseg.
	 */
	if (td->td_slptime) {
		struct ksegrp *kg;
		int hzticks;

		kg = td->td_ksegrp;
		hzticks = ticks - td->td_slptime;
		kg->kg_slptime += hzticks << 10;
		sched_priority(kg);
		CTR2(KTR_ULE, "wakeup kse %p (%d ticks)",
		    td->td_kse, hzticks);
		td->td_slptime = 0;
	}
	setrunqueue(td);
        if (td->td_priority < curthread->td_priority)
                curthread->td_flags |= TDF_NEEDRESCHED;
}

/*
 * Penalize the parent for creating a new child and initialize the child's
 * priority.
 */
void
sched_fork(struct proc *p, struct proc *p1)
{

	mtx_assert(&sched_lock, MA_OWNED);

	sched_fork_ksegrp(FIRST_KSEGRP_IN_PROC(p), FIRST_KSEGRP_IN_PROC(p1));
	sched_fork_kse(FIRST_KSE_IN_PROC(p), FIRST_KSE_IN_PROC(p1));
	sched_fork_thread(FIRST_THREAD_IN_PROC(p), FIRST_THREAD_IN_PROC(p1));
}

void
sched_fork_kse(struct kse *ke, struct kse *child)
{

	child->ke_slice = 1;	/* Attempt to quickly learn interactivity. */
	child->ke_cpu = ke->ke_cpu; /* sched_pickcpu(); */
	child->ke_runq = NULL;

	/*
	 * Claim that we've been running for one second for statistical
	 * purposes.
	 */
	child->ke_ticks = 0;
	child->ke_ltick = ticks;
	child->ke_ftick = ticks - hz;
}

void
sched_fork_ksegrp(struct ksegrp *kg, struct ksegrp *child)
{

	PROC_LOCK_ASSERT(child->kg_proc, MA_OWNED);
	/* XXX Need something better here */

#if 1
	child->kg_slptime = kg->kg_slptime;
	child->kg_runtime = kg->kg_runtime;
#else
	if (kg->kg_slptime > kg->kg_runtime) {
		child->kg_slptime = SCHED_DYN_RANGE;
		child->kg_runtime = kg->kg_slptime / SCHED_DYN_RANGE;
	} else {
		child->kg_runtime = SCHED_DYN_RANGE;
		child->kg_slptime = kg->kg_runtime / SCHED_DYN_RANGE;
	}
#endif

	child->kg_user_pri = kg->kg_user_pri;
	child->kg_nice = kg->kg_nice;
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
}

void
sched_class(struct ksegrp *kg, int class)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	if (kg->kg_pri_class == class)
		return;

	FOREACH_KSE_IN_GROUP(kg, ke) {
		if (ke->ke_state != KES_ONRUNQ &&
		    ke->ke_state != KES_THREAD)
			continue;
		kseq = KSEQ_CPU(ke->ke_cpu);

		kseq->ksq_loads[PRI_BASE(kg->kg_pri_class)]--;
		kseq->ksq_loads[PRI_BASE(class)]++;

		if (kg->kg_pri_class == PRI_TIMESHARE)
			kseq_nice_rem(kseq, kg->kg_nice);
		else if (class == PRI_TIMESHARE)
			kseq_nice_add(kseq, kg->kg_nice);
	}

	kg->kg_pri_class = class;
}

/*
 * Return some of the child's priority and interactivity to the parent.
 */
void
sched_exit(struct proc *p, struct proc *child)
{
	/* XXX Need something better here */
	mtx_assert(&sched_lock, MA_OWNED);
	sched_exit_kse(FIRST_KSE_IN_PROC(p), FIRST_KSE_IN_PROC(child));
	sched_exit_ksegrp(FIRST_KSEGRP_IN_PROC(p), FIRST_KSEGRP_IN_PROC(child));
}

void
sched_exit_kse(struct kse *ke, struct kse *child)
{
	kseq_rem(KSEQ_CPU(child->ke_cpu), child);
}

void
sched_exit_ksegrp(struct ksegrp *kg, struct ksegrp *child)
{
	kg->kg_slptime += child->kg_slptime;
	kg->kg_runtime += child->kg_runtime;
}

void
sched_exit_thread(struct thread *td, struct thread *child)
{
}

void
sched_clock(struct kse *ke)
{
	struct kseq *kseq;
	struct ksegrp *kg;
	struct thread *td;
#if 0
	struct kse *nke;
#endif

	/*
	 * sched_setup() apparently happens prior to stathz being set.  We
	 * need to resolve the timers earlier in the boot so we can avoid
	 * calculating this here.
	 */
	if (realstathz == 0) {
		realstathz = stathz ? stathz : hz;
		tickincr = hz / realstathz;
		/*
		 * XXX This does not work for values of stathz that are much
		 * larger than hz.
		 */
		if (tickincr == 0)
			tickincr = 1;
	}

	td = ke->ke_thread;
	kg = ke->ke_ksegrp;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((td != NULL), ("schedclock: null thread pointer"));

	/* Adjust ticks for pctcpu */
	ke->ke_ticks++;
	ke->ke_ltick = ticks;

	/* Go up to one second beyond our max and then trim back down */
	if (ke->ke_ftick + SCHED_CPU_TICKS + hz < ke->ke_ltick)
		sched_pctcpu_update(ke);

	if (td->td_flags & TDF_IDLETD)
		return;

	CTR4(KTR_ULE, "Tick kse %p (slice: %d, slptime: %d, runtime: %d)",
	    ke, ke->ke_slice, kg->kg_slptime >> 10, kg->kg_runtime >> 10);

	/*
	 * We only do slicing code for TIMESHARE ksegrps.
	 */
	if (kg->kg_pri_class != PRI_TIMESHARE)
		return;
	/*
	 * Check for a higher priority task on the run queue.  This can happen
	 * on SMP if another processor woke up a process on our runq.
	 */
	kseq = KSEQ_SELF();
#if 0
	if (kseq->ksq_load > 1 && (nke = kseq_choose(kseq)) != NULL) {
		if (sched_strict &&
		    nke->ke_thread->td_priority < td->td_priority)
			td->td_flags |= TDF_NEEDRESCHED;
		else if (nke->ke_thread->td_priority <
		    td->td_priority SCHED_PRIO_SLOP)
		    
		if (nke->ke_thread->td_priority < td->td_priority)
			td->td_flags |= TDF_NEEDRESCHED;
	}
#endif
	/*
	 * We used a tick charge it to the ksegrp so that we can compute our
	 * interactivity.
	 */
	kg->kg_runtime += tickincr << 10;

	/*
	 * We used up one time slice.
	 */
	ke->ke_slice--;
#ifdef SMP
	kseq->ksq_rslices--;
#endif

	if (ke->ke_slice > 0)
		return;
	/*
	 * We're out of time, recompute priorities and requeue.
	 */
	kseq_rem(kseq, ke);
	sched_priority(kg);
	sched_slice(ke);
	if (SCHED_CURR(kg, ke))
		ke->ke_runq = kseq->ksq_curr;
	else
		ke->ke_runq = kseq->ksq_next;
	kseq_add(kseq, ke);
	td->td_flags |= TDF_NEEDRESCHED;
}

int
sched_runnable(void)
{
	struct kseq *kseq;
	int load;

	load = 1;

	mtx_lock_spin(&sched_lock);
	kseq = KSEQ_SELF();

	if (kseq->ksq_load)
		goto out;
#ifdef SMP
	/*
	 * For SMP we may steal other processor's KSEs.  Just search until we
	 * verify that at least on other cpu has a runnable task.
	 */
	if (smp_started) {
		int i;

		for (i = 0; i < mp_maxid; i++) {
			if (CPU_ABSENT(i))
				continue;
			kseq = KSEQ_CPU(i);
			if (kseq->ksq_load > 1)
				goto out;
		}
	}
#endif
	load = 0;
out:
	mtx_unlock_spin(&sched_lock);
	return (load);
}

void
sched_userret(struct thread *td)
{
	struct ksegrp *kg;
	struct kseq *kseq;
	struct kse *ke;
	
	kg = td->td_ksegrp;

	if (td->td_priority != kg->kg_user_pri) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = kg->kg_user_pri;
		kseq = KSEQ_SELF();
		if (td->td_ksegrp->kg_pri_class == PRI_TIMESHARE &&
		    kseq->ksq_load > 1 &&
		    (ke = kseq_choose(kseq)) != NULL &&
		    ke->ke_thread->td_priority < td->td_priority)
			curthread->td_flags |= TDF_NEEDRESCHED;
		mtx_unlock_spin(&sched_lock);
	}
}

struct kse *
sched_choose(void)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
#ifdef SMP
retry:
#endif
	kseq = KSEQ_SELF();
	ke = kseq_choose(kseq);
	if (ke) {
		runq_remove(ke->ke_runq, ke);
		ke->ke_state = KES_THREAD;

		if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE) {
			CTR4(KTR_ULE, "Run kse %p from %p (slice: %d, pri: %d)",
			    ke, ke->ke_runq, ke->ke_slice,
			    ke->ke_thread->td_priority);
		}
		return (ke);
	}

#ifdef SMP
	if (smp_started) {
		/*
		 * Find the cpu with the highest load and steal one proc.
		 */
		if ((kseq = kseq_load_highest()) == NULL)
			return (NULL);

		/*
		 * Remove this kse from this kseq and runq and then requeue
		 * on the current processor.  Then we will dequeue it
		 * normally above.
		 */
		kseq_move(kseq, PCPU_GET(cpuid));
		goto retry;
	}
#endif

	return (NULL);
}

void
sched_add(struct kse *ke)
{
	struct kseq *kseq;
	struct ksegrp *kg;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_thread != NULL), ("sched_add: No thread on KSE"));
	KASSERT((ke->ke_thread->td_kse != NULL),
	    ("sched_add: No KSE on thread"));
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	KASSERT(ke->ke_runq == NULL,
	    ("sched_add: KSE %p is still assigned to a run queue", ke));

	kg = ke->ke_ksegrp;

	switch (PRI_BASE(kg->kg_pri_class)) {
	case PRI_ITHD:
	case PRI_REALTIME:
		kseq = KSEQ_SELF();
		ke->ke_runq = kseq->ksq_curr;
		ke->ke_slice = SCHED_SLICE_MAX;
		ke->ke_cpu = PCPU_GET(cpuid);
		break;
	case PRI_TIMESHARE:
		kseq = KSEQ_CPU(ke->ke_cpu);
		if (SCHED_CURR(kg, ke))
			ke->ke_runq = kseq->ksq_curr;
		else
			ke->ke_runq = kseq->ksq_next;
		break;
	case PRI_IDLE:
		kseq = KSEQ_CPU(ke->ke_cpu);
		/*
		 * This is for priority prop.
		 */
		if (ke->ke_thread->td_priority > PRI_MIN_IDLE)
			ke->ke_runq = kseq->ksq_curr;
		else
			ke->ke_runq = &kseq->ksq_idle;
		ke->ke_slice = SCHED_SLICE_MIN;
		break;
	default:
		panic("Unknown pri class.\n");
		break;
	}

	ke->ke_ksegrp->kg_runq_kses++;
	ke->ke_state = KES_ONRUNQ;

	runq_add(ke->ke_runq, ke);
	kseq_add(kseq, ke);
}

void
sched_rem(struct kse *ke)
{
	struct kseq *kseq;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_state == KES_ONRUNQ), ("KSE not on run queue"));

	ke->ke_state = KES_THREAD;
	ke->ke_ksegrp->kg_runq_kses--;
	kseq = KSEQ_CPU(ke->ke_cpu);
	runq_remove(ke->ke_runq, ke);
	kseq_rem(kseq, ke);
}

fixpt_t
sched_pctcpu(struct kse *ke)
{
	fixpt_t pctcpu;

	pctcpu = 0;

	mtx_lock_spin(&sched_lock);
	if (ke->ke_ticks) {
		int rtick;

		/* Update to account for time potentially spent sleeping */
		ke->ke_ltick = ticks;
		/*
		 * Don't update more frequently than twice a second.  Allowing
		 * this causes the cpu usage to decay away too quickly due to
		 * rounding errors.
		 */
		if (ke->ke_ltick < (ticks - (hz / 2)))
			sched_pctcpu_update(ke);

		/* How many rtick per second ? */
		rtick = min(ke->ke_ticks / SCHED_CPU_TIME, SCHED_CPU_TICKS);
		pctcpu = (FSCALE * ((FSCALE * rtick)/realstathz)) >> FSHIFT;
	}

	ke->ke_proc->p_swtime = ke->ke_ltick - ke->ke_ftick;
	mtx_unlock_spin(&sched_lock);

	return (pctcpu);
}

int
sched_sizeof_kse(void)
{
	return (sizeof(struct kse) + sizeof(struct ke_sched));
}

int
sched_sizeof_ksegrp(void)
{
	return (sizeof(struct ksegrp) + sizeof(struct kg_sched));
}

int
sched_sizeof_proc(void)
{
	return (sizeof(struct proc));
}

int
sched_sizeof_thread(void)
{
	return (sizeof(struct thread) + sizeof(struct td_sched));
}
