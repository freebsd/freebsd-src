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
};
#define	ke_slice	ke_sched->ske_slice
#define	ke_runq		ke_sched->ske_runq
#define	ke_ltick	ke_sched->ske_ltick
#define	ke_ftick	ke_sched->ske_ftick
#define	ke_ticks	ke_sched->ske_ticks

struct kg_sched {
	int	skg_slptime;
};
#define	kg_slptime	kg_sched->skg_slptime

struct td_sched {
	int	std_slptime;
};
#define	td_slptime	td_sched->std_slptime

struct ke_sched ke_sched;
struct kg_sched kg_sched;
struct td_sched td_sched;

struct ke_sched *kse0_sched = &ke_sched;
struct kg_sched *ksegrp0_sched = &kg_sched;
struct p_sched *proc0_sched = NULL;
struct td_sched *thread0_sched = &td_sched;

/*
 * This priority range has 20 priorities on either end that are reachable
 * only through nice values.
 */
#define	SCHED_PRI_NRESV	40
#define	SCHED_PRI_RANGE	((PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1) - \
    SCHED_PRI_NRESV)

/*
 * These determine how sleep time effects the priority of a process.
 *
 * SLP_MAX:	Maximum amount of accrued sleep time.
 * SLP_SCALE:	Scale the number of ticks slept across the dynamic priority
 *		range.
 * SLP_TOPRI:	Convert a number of ticks slept into a priority value.
 * SLP_DECAY:	Reduce the sleep time to 50% for every granted slice.
 */
#define	SCHED_SLP_MAX	(hz * 2)
#define	SCHED_SLP_SCALE(slp)	(((slp) * SCHED_PRI_RANGE) / SCHED_SLP_MAX)
#define	SCHED_SLP_TOPRI(slp)	(SCHED_PRI_RANGE - SCHED_SLP_SCALE((slp)) + \
    SCHED_PRI_NRESV / 2)
#define	SCHED_SLP_DECAY(slp)	((slp) / 2)	/* XXX Multiple kses break */

/*
 * These parameters and macros determine the size of the time slice that is
 * granted to each thread.
 *
 * SLICE_MIN:	Minimum time slice granted, in units of ticks.
 * SLICE_MAX:	Maximum time slice granted.
 * SLICE_RANGE:	Range of available time slices scaled by hz.
 * SLICE_SCALE:	The number slices granted per unit of pri or slp.
 * PRI_TOSLICE:	Compute a slice size that is proportional to the priority.
 * SLP_TOSLICE:	Compute a slice size that is inversely proportional to the
 *		amount of time slept. (smaller slices for interactive ksegs)
 * PRI_COMP:	This determines what fraction of the actual slice comes from 
 *		the slice size computed from the priority.
 * SLP_COMP:	This determines what component of the actual slice comes from
 *		the slize size computed from the sleep time.
 */
#define	SCHED_SLICE_MIN		(hz / 100)
#define	SCHED_SLICE_MAX		(hz / 10)
#define	SCHED_SLICE_RANGE	(SCHED_SLICE_MAX - SCHED_SLICE_MIN + 1)
#define	SCHED_SLICE_SCALE(val, max)	(((val) * SCHED_SLICE_RANGE) / (max))
#define	SCHED_PRI_TOSLICE(pri)						\
    (SCHED_SLICE_MAX - SCHED_SLICE_SCALE((pri), SCHED_PRI_RANGE))
#define	SCHED_SLP_TOSLICE(slp)						\
    (SCHED_SLICE_MAX - SCHED_SLICE_SCALE((slp), SCHED_SLP_MAX))
#define	SCHED_SLP_COMP(slice)	(((slice) / 5) * 3)	/* 60% */
#define	SCHED_PRI_COMP(slice)	(((slice) / 5) * 2)	/* 40% */

/*
 * This macro determines whether or not the kse belongs on the current or
 * next run queue.
 */
#define	SCHED_CURR(kg)	((kg)->kg_slptime > (hz / 4) || \
    (kg)->kg_pri_class != PRI_TIMESHARE)

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

static int sched_slice(struct ksegrp *kg);
static int sched_priority(struct ksegrp *kg);
void sched_pctcpu_update(struct kse *ke);
int sched_pickcpu(void);

static struct kse * kseq_choose(struct kseq *kseq);
static void kseq_setup(struct kseq *kseq);

static void
kseq_setup(struct kseq *kseq)
{
	kseq->ksq_load = 0;
	kseq->ksq_curr = &kseq->ksq_runqs[0];
	kseq->ksq_next = &kseq->ksq_runqs[1];
	runq_init(kseq->ksq_curr);
	runq_init(kseq->ksq_next);
}

static void
sched_setup(void *dummy)
{
	int i;

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

	pri = SCHED_SLP_TOPRI(kg->kg_slptime);
	CTR2(KTR_RUNQ, "sched_priority: slptime: %d\tpri: %d",
	    kg->kg_slptime, pri);

	pri += PRI_MIN_TIMESHARE;
	pri += kg->kg_nice;

	if (pri > PRI_MAX_TIMESHARE)
		pri = PRI_MAX_TIMESHARE;
	else if (pri < PRI_MIN_TIMESHARE)
		pri = PRI_MIN_TIMESHARE;

	kg->kg_user_pri = pri;

	return (kg->kg_user_pri);
}

/*
 * Calculate a time slice based on the process priority.
 */
static int
sched_slice(struct ksegrp *kg)
{
	int pslice;
	int sslice;
	int slice;
	int pri;

	pri = kg->kg_user_pri;
	pri -= PRI_MIN_TIMESHARE;
	pslice = SCHED_PRI_TOSLICE(pri);
	sslice = SCHED_SLP_TOSLICE(kg->kg_slptime);	
	slice = SCHED_SLP_COMP(sslice) + SCHED_PRI_COMP(pslice);
	kg->kg_slptime = SCHED_SLP_DECAY(kg->kg_slptime);

	CTR4(KTR_RUNQ,
	    "sched_slice: pri: %d\tsslice: %d\tpslice: %d\tslice: %d",
	    pri, sslice, pslice, slice);

	if (slice < SCHED_SLICE_MIN)
		slice = SCHED_SLICE_MIN;
	else if (slice > SCHED_SLICE_MAX)
		slice = SCHED_SLICE_MAX;

	return (slice);
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
	ke->ke_ticks = (ke->ke_ticks / (ke->ke_ltick - ke->ke_ftick)) *
		    SCHED_CPU_TICKS;
	ke->ke_ltick = ticks;
	ke->ke_ftick = ke->ke_ltick - SCHED_CPU_TICKS;
}

#ifdef SMP
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
        ke->ke_flags &= ~KEF_NEEDRESCHED;

	if (TD_IS_RUNNING(td)) {
		setrunqueue(td);
		return;
	} else
		td->td_kse->ke_runq = NULL;

	/*
	 * We will not be on the run queue. So we must be
	 * sleeping or similar.
	 */
	if (td->td_proc->p_flag & P_KSES)
		kse_reassign(ke);
}

void
sched_switchin(struct thread *td)
{
	/* struct kse *ke = td->td_kse; */
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_kse->ke_oncpu = PCPU_GET(cpuid); /* XXX */
	if (td->td_ksegrp->kg_pri_class == PRI_TIMESHARE &&
	    td->td_priority != td->td_ksegrp->kg_user_pri)
		curthread->td_kse->ke_flags |= KEF_NEEDRESCHED;
}

void
sched_nice(struct ksegrp *kg, int nice)
{
	struct thread *td;

	kg->kg_nice = nice;
	sched_priority(kg);
	FOREACH_THREAD_IN_GROUP(kg, td) {
		td->td_kse->ke_flags |= KEF_NEEDRESCHED;
	}
}

void
sched_sleep(struct thread *td, u_char prio)
{
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_slptime = ticks;
	td->td_priority = prio;

	/*
	 * If this is an interactive task clear its queue so it moves back
	 * on to curr when it wakes up.  Otherwise let it stay on the queue
	 * that it was assigned to.
	 */
	if (SCHED_CURR(td->td_kse->ke_ksegrp))
		td->td_kse->ke_runq = NULL;
#if 0
	if (td->td_priority < PZERO)
		kseq_cpu[td->td_kse->ke_oncpu].ksq_load++;
#endif
}

void
sched_wakeup(struct thread *td)
{
	struct ksegrp *kg;

	mtx_assert(&sched_lock, MA_OWNED);

	/*
	 * Let the kseg know how long we slept for.  This is because process
	 * interactivity behavior is modeled in the kseg.
	 */
	kg = td->td_ksegrp;

	if (td->td_slptime) {
		kg->kg_slptime += ticks - td->td_slptime;
		if (kg->kg_slptime > SCHED_SLP_MAX)
			kg->kg_slptime = SCHED_SLP_MAX;
		td->td_priority = sched_priority(kg);
	}
	td->td_slptime = 0;
#if 0
	if (td->td_priority < PZERO)
		kseq_cpu[td->td_kse->ke_oncpu].ksq_load--;
#endif
	setrunqueue(td);
        if (td->td_priority < curthread->td_priority)
                curthread->td_kse->ke_flags |= KEF_NEEDRESCHED;
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
	child->kg_slptime = kg->kg_slptime;
	child->kg_user_pri = kg->kg_user_pri;

	if (pkse->ke_oncpu != PCPU_GET(cpuid)) {
		printf("pkse->ke_oncpu = %d\n", pkse->ke_oncpu);
		printf("cpuid = %d", PCPU_GET(cpuid));
		Debugger("stop");
	}

	ckse->ke_slice = pkse->ke_slice;
	ckse->ke_oncpu = pkse->ke_oncpu; /* sched_pickcpu(); */
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
	sched_priority(kg);
}

void
sched_clock(struct thread *td)
{
	struct kse *ke;
	struct kse *nke;
	struct kseq *kseq;
	struct ksegrp *kg;


	ke = td->td_kse;
	kg = td->td_ksegrp;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((td != NULL), ("schedclock: null thread pointer"));

	/* Adjust ticks for pctcpu */
	ke->ke_ticks += 10000;
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
	kseq = KSEQ_SELF();
	nke = runq_choose(kseq->ksq_curr);

	if (nke && nke->ke_thread &&
	    nke->ke_thread->td_priority < td->td_priority)
		ke->ke_flags |= KEF_NEEDRESCHED;
	/*
	 * We used a tick, decrease our total sleep time.  This decreases our
	 * "interactivity".
	 */
	if (kg->kg_slptime)
		kg->kg_slptime--;
	/*
	 * We used up one time slice.
	 */
	ke->ke_slice--;
	/*
	 * We're out of time, recompute priorities and requeue
	 */
	if (ke->ke_slice == 0) {
		td->td_priority = sched_priority(kg);
		ke->ke_slice = sched_slice(kg);
		ke->ke_flags |= KEF_NEEDRESCHED;
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

struct kse *
sched_choose(void)
{
	struct kseq *kseq;
	struct kse *ke;

	kseq = KSEQ_SELF();
	ke = kseq_choose(kseq);

	if (ke) {
		runq_remove(ke->ke_runq, ke);
		kseq->ksq_load--;
		ke->ke_state = KES_THREAD;
	}

#ifdef SMP
	if (ke == NULL && smp_started) {
		int load;
		int cpu;
		int i;

		load = 0;
		cpu = 0;

		/*
		 * Find the cpu with the highest load and steal one proc.
		 */
		for (i = 0; i < mp_maxid; i++) {
			if (CPU_ABSENT(i))
				continue;
			kseq = KSEQ_CPU(i);
			if (kseq->ksq_load > load) {
				load = kseq->ksq_load;
				cpu = i;
			}
		}
		if (load) {
			kseq = KSEQ_CPU(cpu);
			ke = kseq_choose(kseq);
			kseq->ksq_load--;
			ke->ke_state = KES_THREAD;
			runq_remove(ke->ke_runq, ke);
			ke->ke_runq = NULL;
			ke->ke_oncpu = PCPU_GET(cpuid);
		}

	}
#endif
	return (ke);
}

void
sched_add(struct kse *ke)
{

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_thread != NULL), ("runq_add: No thread on KSE"));
	KASSERT((ke->ke_thread->td_kse != NULL),
	    ("runq_add: No KSE on thread"));
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("runq_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("runq_add: process swapped out"));


	if (ke->ke_runq == NULL) {
		struct kseq *kseq;

		kseq = KSEQ_CPU(ke->ke_oncpu);
		if (SCHED_CURR(ke->ke_ksegrp))
			ke->ke_runq = kseq->ksq_curr;
		else
			ke->ke_runq = kseq->ksq_next;
	}
	ke->ke_ksegrp->kg_runq_kses++;
	ke->ke_state = KES_ONRUNQ;

	runq_add(ke->ke_runq, ke);
	KSEQ_CPU(ke->ke_oncpu)->ksq_load++;
}

void
sched_rem(struct kse *ke)
{
	mtx_assert(&sched_lock, MA_OWNED);
	/* KASSERT((ke->ke_state == KES_ONRUNQ), ("KSE not on run queue")); */

	runq_remove(ke->ke_runq, ke);
	ke->ke_runq = NULL;
	ke->ke_state = KES_THREAD;
	ke->ke_ksegrp->kg_runq_kses--;
	KSEQ_CPU(ke->ke_oncpu)->ksq_load--;
}

fixpt_t
sched_pctcpu(struct kse *ke)
{
	fixpt_t pctcpu;

	pctcpu = 0;

	if (ke->ke_ticks) {
		int rtick;

		/* Update to account for time potentially spent sleeping */
		ke->ke_ltick = ticks;
		sched_pctcpu_update(ke);

		/* How many rtick per second ? */
		rtick = ke->ke_ticks / (SCHED_CPU_TIME * 10000);
		pctcpu = (FSCALE * ((FSCALE * rtick)/stathz)) >> FSHIFT;
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
