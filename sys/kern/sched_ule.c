/*-
 * Copyright (c) 2003, Jeffrey Roberson <jeff@freebsd.org>
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
 *
 * $FreeBSD$
 */

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

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
/* XXX This is bogus compatability crap for ps */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

int realstathz;

#define	SCHED_STRICT_RESCHED 1

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
	int	std_schedflag;
};
#define	td_slptime	td_sched->std_slptime
#define	td_schedflag	td_sched->std_schedflag

#define	TD_SCHED_BLOAD	0x0001		/*
					 * thread was counted as being in short
					 * term sleep.
					 */
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
 * DYN_HALF:	Half of DYN_RANGE for convenience elsewhere.
 * PRI_DYN:	The dynamic priority which is derived from the number of ticks
 *		running vs the total number of ticks.
 */
#define	SCHED_PRI_RANGE		(PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1)
#define	SCHED_PRI_NRESV		PRIO_TOTAL
#define	SCHED_PRI_NHALF		(PRIO_TOTAL >> 2)
#define	SCHED_PRI_BASE		((SCHED_PRI_NRESV / 2) + PRI_MIN_TIMESHARE)
#define	SCHED_DYN_RANGE		(SCHED_PRI_RANGE - SCHED_PRI_NRESV)
#define	SCHED_DYN_HALF		(SCHED_DYN_RANGE / 2)
#define	SCHED_PRI_DYN(run, total)	(((run) * SCHED_DYN_RANGE) / (total))


/*
 * These determine the interactivity of a process.
 *
 * SLP_RUN_MAX:	Maximum amount of sleep time + run time we'll accumulate
 *		before throttling back.
 * SLP_RUN_THROTTLE:	Divisor for reducing slp/run time.
 * INTERACT_RANGE:	Range of interactivity values.  Smaller is better.
 * INTERACT_HALF:	Convenience define, half of the interactivity range.
 * INTERACT_THRESH:	Threshhold for placement on the current runq.
 */
#define	SCHED_SLP_RUN_MAX	((hz * 30) << 10)
#define	SCHED_SLP_RUN_THROTTLE	(10)
#define	SCHED_INTERACT_RANGE	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_RANGE / 2)
#define	SCHED_INTERACT_THRESH	(10)

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
#define	SCHED_SLICE_MIN			(hz / 100)
#define	SCHED_SLICE_MAX			(hz / 10)
#define	SCHED_SLICE_RANGE		(SCHED_SLICE_MAX - SCHED_SLICE_MIN + 1)
#define	SCHED_SLICE_SCALE(val, max)	(((val) * SCHED_SLICE_RANGE) / (max))
#define	SCHED_SLICE_NICE(nice)						\
    (SCHED_SLICE_MAX - SCHED_SLICE_SCALE((nice), SCHED_PRI_NHALF))

/*
 * This macro determines whether or not the kse belongs on the current or
 * next run queue.
 * 
 * XXX nice value should effect how interactive a kg is.
 */
#define	SCHED_CURR(kg)	(sched_interact_score(kg) < SCHED_INTERACT_THRESH)

/*
 * Cpu percentage computation macros and defines.
 *
 * SCHED_CPU_TIME:	Number of seconds to average the cpu usage across.
 * SCHED_CPU_TICKS:	Number of hz ticks to average the cpu usage across.
 */

#define	SCHED_CPU_TIME	60
#define	SCHED_CPU_TICKS	(hz * SCHED_CPU_TIME)

/*
 * kseq - pair of runqs per processor
 */

struct kseq {
	struct runq	ksq_runqs[2];
	struct runq	*ksq_curr;
	struct runq	*ksq_next;
	int		ksq_load;	/* Total runnable */
#ifdef SMP
	unsigned int	ksq_rslices;	/* Slices on run queue */
	unsigned int	ksq_bload;	/* Threads waiting on IO */
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
static int sched_priority(struct ksegrp *kg);
static int sched_interact_score(struct ksegrp *kg);
void sched_pctcpu_update(struct kse *ke);
int sched_pickcpu(void);

/* Operations on per processor queues */
static struct kse * kseq_choose(struct kseq *kseq);
static int kseq_nice_min(struct kseq *kseq);
static void kseq_setup(struct kseq *kseq);
static __inline void kseq_add(struct kseq *kseq, struct kse *ke);
static __inline void kseq_rem(struct kseq *kseq, struct kse *ke);
#ifdef SMP
static __inline void kseq_sleep(struct kseq *kseq, struct kse *ke);
static __inline void kseq_wakeup(struct kseq *kseq, struct kse *ke);
struct kseq * kseq_load_highest(void);
#endif

static __inline void
kseq_add(struct kseq *kseq, struct kse *ke)
{
	runq_add(ke->ke_runq, ke);
	kseq->ksq_load++;
#ifdef SMP
	kseq->ksq_rslices += ke->ke_slice;
#endif
}
static __inline void
kseq_rem(struct kseq *kseq, struct kse *ke)
{
	kseq->ksq_load--;
	runq_remove(ke->ke_runq, ke);
#ifdef SMP
	kseq->ksq_rslices -= ke->ke_slice;
#endif
}

#ifdef SMP
static __inline void
kseq_sleep(struct kseq *kseq, struct kse *ke)
{
	kseq->ksq_bload++;
}

static __inline void
kseq_wakeup(struct kseq *kseq, struct kse *ke)
{
	kseq->ksq_bload--;
}

struct kseq *
kseq_load_highest(void)
{
	struct kseq *kseq;
	int load;
	int cpu;
	int i;

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
	if (load)
		return (KSEQ_CPU(cpu));

	return (NULL);
}
#endif

struct kse *
kseq_choose(struct kseq *kseq)
{
	struct kse *ke;
	struct runq *swap;

	if ((ke = runq_choose(kseq->ksq_curr)) == NULL) {
		swap = kseq->ksq_curr;
		kseq->ksq_curr = kseq->ksq_next;
		kseq->ksq_next = swap;
		ke = runq_choose(kseq->ksq_curr);
	}

	return (ke);
}

static int
kseq_nice_min(struct kseq *kseq)
{
	struct kse *ke0;
	struct kse *ke1;

	if (kseq->ksq_load == 0)
		return (0);

	ke0 = runq_choose(kseq->ksq_curr);
	ke1 = runq_choose(kseq->ksq_next);

	if (ke0 == NULL)
		return (ke1->ke_ksegrp->kg_nice);

	if (ke1 == NULL)
		return (ke0->ke_ksegrp->kg_nice);

	return (min(ke0->ke_ksegrp->kg_nice, ke1->ke_ksegrp->kg_nice));
}

static void
kseq_setup(struct kseq *kseq)
{
	kseq->ksq_curr = &kseq->ksq_runqs[0];
	kseq->ksq_next = &kseq->ksq_runqs[1];
	runq_init(kseq->ksq_curr);
	runq_init(kseq->ksq_next);
	kseq->ksq_load = 0;
#ifdef SMP
	kseq->ksq_rslices = 0;
	kseq->ksq_bload = 0;
#endif
}

static void
sched_setup(void *dummy)
{
	int i;

	realstathz = stathz ? stathz : hz;

	mtx_lock_spin(&sched_lock);
	/* init kseqs */
	for (i = 0; i < MAXCPU; i++)
		kseq_setup(KSEQ_CPU(i));
	mtx_unlock_spin(&sched_lock);
}

/*
 * Scale the scheduling priority according to the "interactivity" of this
 * process.
 */
static int
sched_priority(struct ksegrp *kg)
{
	int pri;

	if (kg->kg_pri_class != PRI_TIMESHARE)
		return (kg->kg_user_pri);

	pri = sched_interact_score(kg) * SCHED_DYN_RANGE / SCHED_INTERACT_RANGE;
	pri += SCHED_PRI_BASE;
	pri += kg->kg_nice;

	if (pri > PRI_MAX_TIMESHARE)
		pri = PRI_MAX_TIMESHARE;
	else if (pri < PRI_MIN_TIMESHARE)
		pri = PRI_MIN_TIMESHARE;

	kg->kg_user_pri = pri;

	return (kg->kg_user_pri);
}

/*
 * Calculate a time slice based on the properties of the kseg and the runq
 * that we're on.
 */
static void
sched_slice(struct kse *ke)
{
	struct ksegrp *kg;

	kg = ke->ke_ksegrp;

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

	if (!SCHED_CURR(kg)) {
		struct kseq *kseq;
		int nice_base;
		int nice;

		kseq = KSEQ_CPU(ke->ke_cpu);
		nice_base = kseq_nice_min(kseq);
		nice = kg->kg_nice + (0 - nice_base);

		if (kseq->ksq_load == 0 || kg->kg_nice < nice_base)
			ke->ke_slice = SCHED_SLICE_MAX;
		else if (nice <= SCHED_PRI_NHALF)
			ke->ke_slice = SCHED_SLICE_NICE(nice);
		else
			ke->ke_slice = 0;
	} else
		ke->ke_slice = SCHED_SLICE_MIN;

	/*
	 * Every time we grant a new slice check to see if we need to scale
	 * back the slp and run time in the kg.  This will cause us to forget
	 * old interactivity while maintaining the current ratio.
	 */
	if ((kg->kg_runtime + kg->kg_slptime) >  SCHED_SLP_RUN_MAX) {
		kg->kg_runtime /= SCHED_SLP_RUN_THROTTLE;
		kg->kg_slptime /= SCHED_SLP_RUN_THROTTLE;
	}

	return;
}

static int
sched_interact_score(struct ksegrp *kg)
{
	int big;
	int small;
	int base;

	if (kg->kg_runtime > kg->kg_slptime) {
		big = kg->kg_runtime;
		small = kg->kg_slptime;
		base = SCHED_INTERACT_HALF;
	} else {
		big = kg->kg_slptime;
		small = kg->kg_runtime;
		base = 0;
	}

	big /= SCHED_INTERACT_HALF;
	if (big != 0)
		small /= big;
	else
		small = 0;

	small += base;
	/* XXX Factor in nice */
	return (small);
}

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
        td->td_lastcpu = ke->ke_oncpu;
	ke->ke_oncpu = NOCPU;
        td->td_flags &= ~TDF_NEEDRESCHED;

	if (TD_IS_RUNNING(td)) {
		setrunqueue(td);
		return;
	}
	td->td_kse->ke_runq = NULL;

	/*
	 * We will not be on the run queue. So we must be
	 * sleeping or similar.
	 */
	if (td->td_proc->p_flag & P_THREADED)
		kse_reassign(ke);
}

void
sched_switchin(struct thread *td)
{
	/* struct kse *ke = td->td_kse; */
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_kse->ke_oncpu = PCPU_GET(cpuid);
#if SCHED_STRICT_RESCHED
	if (td->td_ksegrp->kg_pri_class == PRI_TIMESHARE &&
	    td->td_priority != td->td_ksegrp->kg_user_pri)
		curthread->td_flags |= TDF_NEEDRESCHED;
#endif
}

void
sched_nice(struct ksegrp *kg, int nice)
{
	struct thread *td;

	kg->kg_nice = nice;
	sched_priority(kg);
	FOREACH_THREAD_IN_GROUP(kg, td) {
		td->td_flags |= TDF_NEEDRESCHED;
	}
}

void
sched_sleep(struct thread *td, u_char prio)
{
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_slptime = ticks;
	td->td_priority = prio;

#ifdef SMP
	if (td->td_priority < PZERO) {
		kseq_sleep(KSEQ_CPU(td->td_kse->ke_cpu), td->td_kse);
		td->td_schedflag |= TD_SCHED_BLOAD;
	}
#endif
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

		kg = td->td_ksegrp;
		kg->kg_slptime += (ticks - td->td_slptime) << 10;
		sched_priority(kg);
		td->td_slptime = 0;
	}
#ifdef SMP
	if (td->td_priority < PZERO && td->td_schedflag & TD_SCHED_BLOAD) {
		kseq_wakeup(KSEQ_CPU(td->td_kse->ke_cpu), td->td_kse);
		td->td_schedflag &= ~TD_SCHED_BLOAD;
	}
#endif
	setrunqueue(td);
#if SCHED_STRICT_RESCHED
        if (td->td_priority < curthread->td_priority)
                curthread->td_flags |= TDF_NEEDRESCHED;
#endif
}

/*
 * Penalize the parent for creating a new child and initialize the child's
 * priority.
 */
void
sched_fork(struct ksegrp *kg, struct ksegrp *child)
{
	struct kse *ckse;
	struct kse *pkse;

	mtx_assert(&sched_lock, MA_OWNED);
	ckse = FIRST_KSE_IN_KSEGRP(child);
	pkse = FIRST_KSE_IN_KSEGRP(kg);

	/* XXX Need something better here */
	if (kg->kg_slptime > kg->kg_runtime) {
		child->kg_slptime = SCHED_DYN_RANGE;
		child->kg_runtime = kg->kg_slptime / SCHED_DYN_RANGE;
	} else {
		child->kg_runtime = SCHED_DYN_RANGE;
		child->kg_slptime = kg->kg_runtime / SCHED_DYN_RANGE;
	}
#if 0
	child->kg_slptime = kg->kg_slptime;
	child->kg_runtime = kg->kg_runtime;
#endif
	child->kg_user_pri = kg->kg_user_pri;

#if 0
	if (pkse->ke_cpu != PCPU_GET(cpuid)) {
		printf("pkse->ke_cpu = %d\n", pkse->ke_cpu);
		printf("cpuid = %d", PCPU_GET(cpuid));
		Debugger("stop");
	}
#endif

	ckse->ke_slice = pkse->ke_slice;
	ckse->ke_cpu = pkse->ke_cpu; /* sched_pickcpu(); */
	ckse->ke_runq = NULL;
	/*
	 * Claim that we've been running for one second for statistical
	 * purposes.
	 */
	ckse->ke_ticks = 0;
	ckse->ke_ltick = ticks;
	ckse->ke_ftick = ticks - hz;
}

/*
 * Return some of the child's priority and interactivity to the parent.
 */
void
sched_exit(struct ksegrp *kg, struct ksegrp *child)
{
	/* XXX Need something better here */
	mtx_assert(&sched_lock, MA_OWNED);
	kg->kg_slptime = child->kg_slptime;
	kg->kg_runtime = child->kg_runtime;
	sched_priority(kg);
}

void
sched_clock(struct thread *td)
{
	struct kse *ke;
#if SCHED_STRICT_RESCHED
	struct kse *nke;
	struct kseq *kseq;
#endif
	struct ksegrp *kg;


	ke = td->td_kse;
	kg = td->td_ksegrp;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((td != NULL), ("schedclock: null thread pointer"));

	/* Adjust ticks for pctcpu */
	ke->ke_ticks++;
	ke->ke_ltick = ticks;
	/* Go up to one second beyond our max and then trim back down */
	if (ke->ke_ftick + SCHED_CPU_TICKS + hz < ke->ke_ltick)
		sched_pctcpu_update(ke);

	if (td->td_kse->ke_flags & KEF_IDLEKSE)
		return;

	/*
	 * Check for a higher priority task on the run queue.  This can happen
	 * on SMP if another processor woke up a process on our runq.
	 */
#if SCHED_STRICT_RESCHED
	kseq = KSEQ_SELF();
	nke = runq_choose(kseq->ksq_curr);

	if (nke && nke->ke_thread &&
	    nke->ke_thread->td_priority < td->td_priority)
		td->td_flags |= TDF_NEEDRESCHED;
#endif
	/*
	 * We used a tick charge it to the ksegrp so that we can compute our
	 * "interactivity".
	 */
	kg->kg_runtime += 1 << 10;

	/*
	 * We used up one time slice.
	 */
	ke->ke_slice--;
	/*
	 * We're out of time, recompute priorities and requeue
	 */
	if (ke->ke_slice <= 0) {
		sched_priority(kg);
		sched_slice(ke);
		td->td_flags |= TDF_NEEDRESCHED;
		ke->ke_runq = NULL;
	}
}

int
sched_runnable(void)
{
	struct kseq *kseq;

	kseq = KSEQ_SELF();

	if (kseq->ksq_load)
		return (1);
#ifdef SMP
	/*
	 * For SMP we may steal other processor's KSEs.  Just search until we
	 * verify that at least on other cpu has a runnable task.
	 */
	if (smp_started) {
		int i;

#if 0
		if (kseq->ksq_bload)
			return (0);
#endif

		for (i = 0; i < mp_maxid; i++) {
			if (CPU_ABSENT(i))
				continue;
			kseq = KSEQ_CPU(i);
			if (kseq->ksq_load)
				return (1);
		}
	}
#endif
	return (0);
}

void
sched_userret(struct thread *td)
{
	struct ksegrp *kg;
	
	kg = td->td_ksegrp;

	if (td->td_priority != kg->kg_user_pri) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = kg->kg_user_pri;
		mtx_unlock_spin(&sched_lock);
	}
}

struct kse *
sched_choose(void)
{
	struct kseq *kseq;
	struct kse *ke;

	kseq = KSEQ_SELF();
retry:
	ke = kseq_choose(kseq);

	if (ke) {
		ke->ke_state = KES_THREAD;
		kseq_rem(kseq, ke);

		/*
		 * If we dequeue a kse with a slice of zero it was below the
		 * nice threshold to acquire a slice.  Recalculate the slice
		 * to see if the situation has changed and then requeue.
		 */
		if (ke->ke_slice == 0) {
			sched_slice(ke);
			ke->ke_runq = kseq->ksq_next;
			kseq_add(kseq, ke);
			goto retry;
		}
	}

#ifdef SMP
	if (ke == NULL && smp_started) {
#if 0
		if (kseq->ksq_bload)
			return (NULL);
#endif
		/*
		 * Find the cpu with the highest load and steal one proc.
		 */
		kseq = kseq_load_highest();
		if (kseq == NULL)
			return (NULL);
		ke = kseq_choose(kseq);
		kseq_rem(kseq, ke);

		ke->ke_state = KES_THREAD;
		ke->ke_runq = NULL;
		ke->ke_cpu = PCPU_GET(cpuid);
	}
#endif
	return (ke);
}

void
sched_add(struct kse *ke)
{
	struct kseq *kseq;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_thread != NULL), ("sched_add: No thread on KSE"));
	KASSERT((ke->ke_thread->td_kse != NULL),
	    ("sched_add: No KSE on thread"));
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));

	/*
	 * Timeshare threads get placed on the appropriate queue on their
	 * bound cpu.
	 */
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE) {
		kseq = KSEQ_CPU(ke->ke_cpu);

		if (ke->ke_runq == NULL) {
			if (SCHED_CURR(ke->ke_ksegrp))
				ke->ke_runq = kseq->ksq_curr;
			else
				ke->ke_runq = kseq->ksq_next;
		}
	/*
	 * If we're a real-time or interrupt thread place us on the curr
	 * queue for the current processor.  Hopefully this will yield the
	 * lowest latency response.
	 */
	} else {
		kseq = KSEQ_SELF();
		ke->ke_runq = kseq->ksq_curr;
	}
	ke->ke_ksegrp->kg_runq_kses++;
	ke->ke_state = KES_ONRUNQ;

	kseq_add(kseq, ke);
}

void
sched_rem(struct kse *ke)
{
	mtx_assert(&sched_lock, MA_OWNED);
	/* KASSERT((ke->ke_state == KES_ONRUNQ), ("KSE not on run queue")); */

	ke->ke_runq = NULL;
	ke->ke_state = KES_THREAD;
	ke->ke_ksegrp->kg_runq_kses--;

	kseq_rem(KSEQ_CPU(ke->ke_cpu), ke);
}

fixpt_t
sched_pctcpu(struct kse *ke)
{
	fixpt_t pctcpu;
	int realstathz;

	pctcpu = 0;
	realstathz = stathz ? stathz : hz;

	if (ke->ke_ticks) {
		int rtick;

		/* Update to account for time potentially spent sleeping */
		ke->ke_ltick = ticks;
		sched_pctcpu_update(ke);

		/* How many rtick per second ? */
		rtick = ke->ke_ticks / SCHED_CPU_TIME;
		pctcpu = (FSCALE * ((FSCALE * rtick)/realstathz)) >> FSHIFT;
	}

	ke->ke_proc->p_swtime = ke->ke_ltick - ke->ke_ftick;

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
