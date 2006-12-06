/*-
 * Copyright (c) 2002-2005, Jeffrey Roberson <jeff@freebsd.org>
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

#include "opt_hwpmc_hooks.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/turnstile.h>
#include <sys/umtx.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
#endif

#include <machine/cpu.h>
#include <machine/smp.h>

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
/* XXX This is bogus compatability crap for ps */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

static void sched_initticks(void *dummy);
SYSINIT(sched_initticks, SI_SUB_CLOCKS, SI_ORDER_THIRD, sched_initticks, NULL)

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0, "Scheduler");

SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "ule", 0,
    "Scheduler name");

static int slice_min = 1;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_min, CTLFLAG_RW, &slice_min, 0, "");

static int slice_max = 10;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_max, CTLFLAG_RW, &slice_max, 0, "");

int realstathz;
int tickincr = 1 << 10;

/*
 * The following datastructures are allocated within their parent structure
 * but are scheduler specific.
 */
/*
 * Thread scheduler specific section.
 * fields int he thread structure that are specific to this scheduler.
 */
struct td_sched {	
	TAILQ_ENTRY(td_sched) ts_procq;	/* (j/z) Run queue. */
	int		ts_flags;	/* (j) TSF_* flags. */
	struct thread	*ts_thread;	/* (*) Active associated thread. */
	fixpt_t		ts_pctcpu;	/* (j) %cpu during p_swtime. */
	u_char		ts_rqindex;	/* (j) Run queue index. */
	enum {
		TSS_THREAD = 0x0,	/* slaved to thread state */
		TSS_ONRUNQ
	} ts_state;			/* (j) thread sched specific status. */
	int		ts_slptime;
	int		ts_slice;
	struct runq	*ts_runq;
	u_char		ts_cpu;		/* CPU that we have affinity for. */
	/* The following variables are only used for pctcpu calculation */
	int		ts_ltick;	/* Last tick that we were running on */
	int		ts_ftick;	/* First tick that we were running on */
	int		ts_ticks;	/* Tick count */

	/* originally from kg_sched */
	int	skg_slptime;		/* Number of ticks we vol. slept */
	int	skg_runtime;		/* Number of ticks we were running */
};
#define	ts_assign		ts_procq.tqe_next
/* flags kept in ts_flags */
#define	TSF_ASSIGNED	0x0001		/* Thread is being migrated. */
#define	TSF_BOUND	0x0002		/* Thread can not migrate. */
#define	TSF_XFERABLE	0x0004		/* Thread was added as transferable. */
#define	TSF_HOLD	0x0008		/* Thread is temporarily bound. */
#define	TSF_REMOVED	0x0010		/* Thread was removed while ASSIGNED */
#define	TSF_INTERNAL	0x0020		/* Thread added due to migration. */
#define	TSF_PREEMPTED	0x0040		/* Thread was preempted */
#define	TSF_DIDRUN	0x02000		/* Thread actually ran. */
#define	TSF_EXIT	0x04000		/* Thread is being killed. */

static struct td_sched td_sched0;

/*
 * The priority is primarily determined by the interactivity score.  Thus, we
 * give lower(better) priorities to kse groups that use less CPU.  The nice
 * value is then directly added to this to allow nice to have some effect
 * on latency.
 *
 * PRI_RANGE:	Total priority range for timeshare threads.
 * PRI_NRESV:	Number of nice values.
 * PRI_BASE:	The start of the dynamic range.
 */
#define	SCHED_PRI_RANGE		(PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1)
#define	SCHED_PRI_NRESV		((PRIO_MAX - PRIO_MIN) + 1)
#define	SCHED_PRI_NHALF		(SCHED_PRI_NRESV / 2)
#define	SCHED_PRI_BASE		(PRI_MIN_TIMESHARE)
#define	SCHED_PRI_INTERACT(score)					\
    ((score) * SCHED_PRI_RANGE / SCHED_INTERACT_MAX)

/*
 * These determine the interactivity of a process.
 *
 * SLP_RUN_MAX:	Maximum amount of sleep time + run time we'll accumulate
 *		before throttling back.
 * SLP_RUN_FORK:	Maximum slp+run time to inherit at fork time.
 * INTERACT_MAX:	Maximum interactivity value.  Smaller is better.
 * INTERACT_THRESH:	Threshhold for placement on the current runq.
 */
#define	SCHED_SLP_RUN_MAX	((hz * 5) << 10)
#define	SCHED_SLP_RUN_FORK	((hz / 2) << 10)
#define	SCHED_INTERACT_MAX	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_MAX / 2)
#define	SCHED_INTERACT_THRESH	(30)

/*
 * These parameters and macros determine the size of the time slice that is
 * granted to each thread.
 *
 * SLICE_MIN:	Minimum time slice granted, in units of ticks.
 * SLICE_MAX:	Maximum time slice granted.
 * SLICE_RANGE:	Range of available time slices scaled by hz.
 * SLICE_SCALE:	The number slices granted per val in the range of [0, max].
 * SLICE_NICE:  Determine the amount of slice granted to a scaled nice.
 * SLICE_NTHRESH:	The nice cutoff point for slice assignment.
 */
#define	SCHED_SLICE_MIN			(slice_min)
#define	SCHED_SLICE_MAX			(slice_max)
#define	SCHED_SLICE_INTERACTIVE		(slice_max)
#define	SCHED_SLICE_NTHRESH	(SCHED_PRI_NHALF - 1)
#define	SCHED_SLICE_RANGE		(SCHED_SLICE_MAX - SCHED_SLICE_MIN + 1)
#define	SCHED_SLICE_SCALE(val, max)	(((val) * SCHED_SLICE_RANGE) / (max))
#define	SCHED_SLICE_NICE(nice)						\
    (SCHED_SLICE_MAX - SCHED_SLICE_SCALE((nice), SCHED_SLICE_NTHRESH))

/*
 * This macro determines whether or not the thread belongs on the current or
 * next run queue.
 */
#define	SCHED_INTERACTIVE(td)						\
    (sched_interact_score(td) < SCHED_INTERACT_THRESH)
#define	SCHED_CURR(td, ts)						\
    ((ts->ts_thread->td_flags & TDF_BORROWING) ||			\
     (ts->ts_flags & TSF_PREEMPTED) || SCHED_INTERACTIVE(td))

/*
 * Cpu percentage computation macros and defines.
 *
 * SCHED_CPU_TIME:	Number of seconds to average the cpu usage across.
 * SCHED_CPU_TICKS:	Number of hz ticks to average the cpu usage across.
 */

#define	SCHED_CPU_TIME	10
#define	SCHED_CPU_TICKS	(hz * SCHED_CPU_TIME)

/*
 * tdq - per processor runqs and statistics.
 */
struct tdq {
	struct runq	ksq_idle;		/* Queue of IDLE threads. */
	struct runq	ksq_timeshare[2];	/* Run queues for !IDLE. */
	struct runq	*ksq_next;		/* Next timeshare queue. */
	struct runq	*ksq_curr;		/* Current queue. */
	int		ksq_load_timeshare;	/* Load for timeshare. */
	int		ksq_load;		/* Aggregate load. */
	short		ksq_nice[SCHED_PRI_NRESV]; /* threadss in each nice bin. */
	short		ksq_nicemin;		/* Least nice. */
#ifdef SMP
	int			ksq_transferable;
	LIST_ENTRY(tdq)	ksq_siblings;	/* Next in tdq group. */
	struct tdq_group	*ksq_group;	/* Our processor group. */
	volatile struct td_sched *ksq_assigned;	/* assigned by another CPU. */
#else
	int		ksq_sysload;		/* For loadavg, !ITHD load. */
#endif
};

#ifdef SMP
/*
 * tdq groups are groups of processors which can cheaply share threads.  When
 * one processor in the group goes idle it will check the runqs of the other
 * processors in its group prior to halting and waiting for an interrupt.
 * These groups are suitable for SMT (Symetric Multi-Threading) and not NUMA.
 * In a numa environment we'd want an idle bitmap per group and a two tiered
 * load balancer.
 */
struct tdq_group {
	int	ksg_cpus;		/* Count of CPUs in this tdq group. */
	cpumask_t ksg_cpumask;		/* Mask of cpus in this group. */
	cpumask_t ksg_idlemask;		/* Idle cpus in this group. */
	cpumask_t ksg_mask;		/* Bit mask for first cpu. */
	int	ksg_load;		/* Total load of this group. */
	int	ksg_transferable;	/* Transferable load of this group. */
	LIST_HEAD(, tdq)	ksg_members; /* Linked list of all members. */
};
#endif

/*
 * One kse queue per processor.
 */
#ifdef SMP
static cpumask_t tdq_idle;
static int ksg_maxid;
static struct tdq	tdq_cpu[MAXCPU];
static struct tdq_group tdq_groups[MAXCPU];
static int bal_tick;
static int gbal_tick;
static int balance_groups;

#define	TDQ_SELF()	(&tdq_cpu[PCPU_GET(cpuid)])
#define	TDQ_CPU(x)	(&tdq_cpu[(x)])
#define	TDQ_ID(x)	((x) - tdq_cpu)
#define	TDQ_GROUP(x)	(&tdq_groups[(x)])
#else	/* !SMP */
static struct tdq	tdq_cpu;

#define	TDQ_SELF()	(&tdq_cpu)
#define	TDQ_CPU(x)	(&tdq_cpu)
#endif

static struct td_sched *sched_choose(void);		/* XXX Should be thread * */
static void sched_slice(struct td_sched *);
static void sched_priority(struct thread *);
static void sched_thread_priority(struct thread *, u_char);
static int sched_interact_score(struct thread *);
static void sched_interact_update(struct thread *);
static void sched_interact_fork(struct thread *);
static void sched_pctcpu_update(struct td_sched *);

/* Operations on per processor queues */
static struct td_sched * tdq_choose(struct tdq *);
static void tdq_setup(struct tdq *);
static void tdq_load_add(struct tdq *, struct td_sched *);
static void tdq_load_rem(struct tdq *, struct td_sched *);
static __inline void tdq_runq_add(struct tdq *, struct td_sched *, int);
static __inline void tdq_runq_rem(struct tdq *, struct td_sched *);
static void tdq_nice_add(struct tdq *, int);
static void tdq_nice_rem(struct tdq *, int);
void tdq_print(int cpu);
#ifdef SMP
static int tdq_transfer(struct tdq *, struct td_sched *, int);
static struct td_sched *runq_steal(struct runq *);
static void sched_balance(void);
static void sched_balance_groups(void);
static void sched_balance_group(struct tdq_group *);
static void sched_balance_pair(struct tdq *, struct tdq *);
static void tdq_move(struct tdq *, int);
static int tdq_idled(struct tdq *);
static void tdq_notify(struct td_sched *, int);
static void tdq_assign(struct tdq *);
static struct td_sched *tdq_steal(struct tdq *, int);
#define	THREAD_CAN_MIGRATE(ts)						\
    ((ts)->ts_thread->td_pinned == 0 && ((ts)->ts_flags & TSF_BOUND) == 0)
#endif

void
tdq_print(int cpu)
{
	struct tdq *tdq;
	int i;

	tdq = TDQ_CPU(cpu);

	printf("tdq:\n");
	printf("\tload:           %d\n", tdq->ksq_load);
	printf("\tload TIMESHARE: %d\n", tdq->ksq_load_timeshare);
#ifdef SMP
	printf("\tload transferable: %d\n", tdq->ksq_transferable);
#endif
	printf("\tnicemin:\t%d\n", tdq->ksq_nicemin);
	printf("\tnice counts:\n");
	for (i = 0; i < SCHED_PRI_NRESV; i++)
		if (tdq->ksq_nice[i])
			printf("\t\t%d = %d\n",
			    i - SCHED_PRI_NHALF, tdq->ksq_nice[i]);
}

static __inline void
tdq_runq_add(struct tdq *tdq, struct td_sched *ts, int flags)
{
#ifdef SMP
	if (THREAD_CAN_MIGRATE(ts)) {
		tdq->ksq_transferable++;
		tdq->ksq_group->ksg_transferable++;
		ts->ts_flags |= TSF_XFERABLE;
	}
#endif
	if (ts->ts_flags & TSF_PREEMPTED)
		flags |= SRQ_PREEMPTED;
	runq_add(ts->ts_runq, ts, flags);
}

static __inline void
tdq_runq_rem(struct tdq *tdq, struct td_sched *ts)
{
#ifdef SMP
	if (ts->ts_flags & TSF_XFERABLE) {
		tdq->ksq_transferable--;
		tdq->ksq_group->ksg_transferable--;
		ts->ts_flags &= ~TSF_XFERABLE;
	}
#endif
	runq_remove(ts->ts_runq, ts);
}

static void
tdq_load_add(struct tdq *tdq, struct td_sched *ts)
{
	int class;
	mtx_assert(&sched_lock, MA_OWNED);
	class = PRI_BASE(ts->ts_thread->td_pri_class);
	if (class == PRI_TIMESHARE)
		tdq->ksq_load_timeshare++;
	tdq->ksq_load++;
	CTR1(KTR_SCHED, "load: %d", tdq->ksq_load);
	if (class != PRI_ITHD && (ts->ts_thread->td_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		tdq->ksq_group->ksg_load++;
#else
		tdq->ksq_sysload++;
#endif
	if (ts->ts_thread->td_pri_class == PRI_TIMESHARE)
		tdq_nice_add(tdq, ts->ts_thread->td_proc->p_nice);
}

static void
tdq_load_rem(struct tdq *tdq, struct td_sched *ts)
{
	int class;
	mtx_assert(&sched_lock, MA_OWNED);
	class = PRI_BASE(ts->ts_thread->td_pri_class);
	if (class == PRI_TIMESHARE)
		tdq->ksq_load_timeshare--;
	if (class != PRI_ITHD  && (ts->ts_thread->td_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		tdq->ksq_group->ksg_load--;
#else
		tdq->ksq_sysload--;
#endif
	tdq->ksq_load--;
	CTR1(KTR_SCHED, "load: %d", tdq->ksq_load);
	ts->ts_runq = NULL;
	if (ts->ts_thread->td_pri_class == PRI_TIMESHARE)
		tdq_nice_rem(tdq, ts->ts_thread->td_proc->p_nice);
}

static void
tdq_nice_add(struct tdq *tdq, int nice)
{
	mtx_assert(&sched_lock, MA_OWNED);
	/* Normalize to zero. */
	tdq->ksq_nice[nice + SCHED_PRI_NHALF]++;
	if (nice < tdq->ksq_nicemin || tdq->ksq_load_timeshare == 1)
		tdq->ksq_nicemin = nice;
}

static void
tdq_nice_rem(struct tdq *tdq, int nice) 
{
	int n;

	mtx_assert(&sched_lock, MA_OWNED);
	/* Normalize to zero. */
	n = nice + SCHED_PRI_NHALF;
	tdq->ksq_nice[n]--;
	KASSERT(tdq->ksq_nice[n] >= 0, ("Negative nice count."));

	/*
	 * If this wasn't the smallest nice value or there are more in
	 * this bucket we can just return.  Otherwise we have to recalculate
	 * the smallest nice.
	 */
	if (nice != tdq->ksq_nicemin ||
	    tdq->ksq_nice[n] != 0 ||
	    tdq->ksq_load_timeshare == 0)
		return;

	for (; n < SCHED_PRI_NRESV; n++)
		if (tdq->ksq_nice[n]) {
			tdq->ksq_nicemin = n - SCHED_PRI_NHALF;
			return;
		}
}

#ifdef SMP
/*
 * sched_balance is a simple CPU load balancing algorithm.  It operates by
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
static void
sched_balance(void)
{
	struct tdq_group *high;
	struct tdq_group *low;
	struct tdq_group *ksg;
	int cnt;
	int i;

	bal_tick = ticks + (random() % (hz * 2));
	if (smp_started == 0)
		return;
	low = high = NULL;
	i = random() % (ksg_maxid + 1);
	for (cnt = 0; cnt <= ksg_maxid; cnt++) {
		ksg = TDQ_GROUP(i);
		/*
		 * Find the CPU with the highest load that has some
		 * threads to transfer.
		 */
		if ((high == NULL || ksg->ksg_load > high->ksg_load)
		    && ksg->ksg_transferable)
			high = ksg;
		if (low == NULL || ksg->ksg_load < low->ksg_load)
			low = ksg;
		if (++i > ksg_maxid)
			i = 0;
	}
	if (low != NULL && high != NULL && high != low)
		sched_balance_pair(LIST_FIRST(&high->ksg_members),
		    LIST_FIRST(&low->ksg_members));
}

static void
sched_balance_groups(void)
{
	int i;

	gbal_tick = ticks + (random() % (hz * 2));
	mtx_assert(&sched_lock, MA_OWNED);
	if (smp_started)
		for (i = 0; i <= ksg_maxid; i++)
			sched_balance_group(TDQ_GROUP(i));
}

static void
sched_balance_group(struct tdq_group *ksg)
{
	struct tdq *tdq;
	struct tdq *high;
	struct tdq *low;
	int load;

	if (ksg->ksg_transferable == 0)
		return;
	low = NULL;
	high = NULL;
	LIST_FOREACH(tdq, &ksg->ksg_members, ksq_siblings) {
		load = tdq->ksq_load;
		if (high == NULL || load > high->ksq_load)
			high = tdq;
		if (low == NULL || load < low->ksq_load)
			low = tdq;
	}
	if (high != NULL && low != NULL && high != low)
		sched_balance_pair(high, low);
}

static void
sched_balance_pair(struct tdq *high, struct tdq *low)
{
	int transferable;
	int high_load;
	int low_load;
	int move;
	int diff;
	int i;

	/*
	 * If we're transfering within a group we have to use this specific
	 * tdq's transferable count, otherwise we can steal from other members
	 * of the group.
	 */
	if (high->ksq_group == low->ksq_group) {
		transferable = high->ksq_transferable;
		high_load = high->ksq_load;
		low_load = low->ksq_load;
	} else {
		transferable = high->ksq_group->ksg_transferable;
		high_load = high->ksq_group->ksg_load;
		low_load = low->ksq_group->ksg_load;
	}
	if (transferable == 0)
		return;
	/*
	 * Determine what the imbalance is and then adjust that to how many
	 * kses we actually have to give up (transferable).
	 */
	diff = high_load - low_load;
	move = diff / 2;
	if (diff & 0x1)
		move++;
	move = min(move, transferable);
	for (i = 0; i < move; i++)
		tdq_move(high, TDQ_ID(low));
	return;
}

static void
tdq_move(struct tdq *from, int cpu)
{
	struct tdq *tdq;
	struct tdq *to;
	struct td_sched *ts;

	tdq = from;
	to = TDQ_CPU(cpu);
	ts = tdq_steal(tdq, 1);
	if (ts == NULL) {
		struct tdq_group *ksg;

		ksg = tdq->ksq_group;
		LIST_FOREACH(tdq, &ksg->ksg_members, ksq_siblings) {
			if (tdq == from || tdq->ksq_transferable == 0)
				continue;
			ts = tdq_steal(tdq, 1);
			break;
		}
		if (ts == NULL)
			panic("tdq_move: No threads available with a "
			    "transferable count of %d\n", 
			    ksg->ksg_transferable);
	}
	if (tdq == to)
		return;
	ts->ts_state = TSS_THREAD;
	tdq_runq_rem(tdq, ts);
	tdq_load_rem(tdq, ts);
	tdq_notify(ts, cpu);
}

static int
tdq_idled(struct tdq *tdq)
{
	struct tdq_group *ksg;
	struct tdq *steal;
	struct td_sched *ts;

	ksg = tdq->ksq_group;
	/*
	 * If we're in a cpu group, try and steal kses from another cpu in
	 * the group before idling.
	 */
	if (ksg->ksg_cpus > 1 && ksg->ksg_transferable) {
		LIST_FOREACH(steal, &ksg->ksg_members, ksq_siblings) {
			if (steal == tdq || steal->ksq_transferable == 0)
				continue;
			ts = tdq_steal(steal, 0);
			if (ts == NULL)
				continue;
			ts->ts_state = TSS_THREAD;
			tdq_runq_rem(steal, ts);
			tdq_load_rem(steal, ts);
			ts->ts_cpu = PCPU_GET(cpuid);
			ts->ts_flags |= TSF_INTERNAL | TSF_HOLD;
			sched_add(ts->ts_thread, SRQ_YIELDING);
			return (0);
		}
	}
	/*
	 * We only set the idled bit when all of the cpus in the group are
	 * idle.  Otherwise we could get into a situation where a thread bounces
	 * back and forth between two idle cores on seperate physical CPUs.
	 */
	ksg->ksg_idlemask |= PCPU_GET(cpumask);
	if (ksg->ksg_idlemask != ksg->ksg_cpumask)
		return (1);
	atomic_set_int(&tdq_idle, ksg->ksg_mask);
	return (1);
}

static void
tdq_assign(struct tdq *tdq)
{
	struct td_sched *nts;
	struct td_sched *ts;

	do {
		*(volatile struct td_sched **)&ts = tdq->ksq_assigned;
	} while(!atomic_cmpset_ptr((volatile uintptr_t *)&tdq->ksq_assigned,
		(uintptr_t)ts, (uintptr_t)NULL));
	for (; ts != NULL; ts = nts) {
		nts = ts->ts_assign;
		tdq->ksq_group->ksg_load--;
		tdq->ksq_load--;
		ts->ts_flags &= ~TSF_ASSIGNED;
		if (ts->ts_flags & TSF_REMOVED) {
			ts->ts_flags &= ~TSF_REMOVED;
			continue;
		}
		ts->ts_flags |= TSF_INTERNAL | TSF_HOLD;
		sched_add(ts->ts_thread, SRQ_YIELDING);
	}
}

static void
tdq_notify(struct td_sched *ts, int cpu)
{
	struct tdq *tdq;
	struct thread *td;
	struct pcpu *pcpu;
	int class;
	int prio;

	tdq = TDQ_CPU(cpu);
	/* XXX */
	class = PRI_BASE(ts->ts_thread->td_pri_class);
	if ((class == PRI_TIMESHARE || class == PRI_REALTIME) &&
	    (tdq_idle & tdq->ksq_group->ksg_mask)) 
		atomic_clear_int(&tdq_idle, tdq->ksq_group->ksg_mask);
	tdq->ksq_group->ksg_load++;
	tdq->ksq_load++;
	ts->ts_cpu = cpu;
	ts->ts_flags |= TSF_ASSIGNED;
	prio = ts->ts_thread->td_priority;

	/*
	 * Place a thread on another cpu's queue and force a resched.
	 */
	do {
		*(volatile struct td_sched **)&ts->ts_assign = tdq->ksq_assigned;
	} while(!atomic_cmpset_ptr((volatile uintptr_t *)&tdq->ksq_assigned,
		(uintptr_t)ts->ts_assign, (uintptr_t)ts));
	/*
	 * Without sched_lock we could lose a race where we set NEEDRESCHED
	 * on a thread that is switched out before the IPI is delivered.  This
	 * would lead us to miss the resched.  This will be a problem once
	 * sched_lock is pushed down.
	 */
	pcpu = pcpu_find(cpu);
	td = pcpu->pc_curthread;
	if (ts->ts_thread->td_priority < td->td_priority ||
	    td == pcpu->pc_idlethread) {
		td->td_flags |= TDF_NEEDRESCHED;
		ipi_selected(1 << cpu, IPI_AST);
	}
}

static struct td_sched *
runq_steal(struct runq *rq)
{
	struct rqhead *rqh;
	struct rqbits *rqb;
	struct td_sched *ts;
	int word;
	int bit;

	mtx_assert(&sched_lock, MA_OWNED);
	rqb = &rq->rq_status;
	for (word = 0; word < RQB_LEN; word++) {
		if (rqb->rqb_bits[word] == 0)
			continue;
		for (bit = 0; bit < RQB_BPW; bit++) {
			if ((rqb->rqb_bits[word] & (1ul << bit)) == 0)
				continue;
			rqh = &rq->rq_queues[bit + (word << RQB_L2BPW)];
			TAILQ_FOREACH(ts, rqh, ts_procq) {
				if (THREAD_CAN_MIGRATE(ts))
					return (ts);
			}
		}
	}
	return (NULL);
}

static struct td_sched *
tdq_steal(struct tdq *tdq, int stealidle)
{
	struct td_sched *ts;

	/*
	 * Steal from next first to try to get a non-interactive task that
	 * may not have run for a while.
	 */
	if ((ts = runq_steal(tdq->ksq_next)) != NULL)
		return (ts);
	if ((ts = runq_steal(tdq->ksq_curr)) != NULL)
		return (ts);
	if (stealidle)
		return (runq_steal(&tdq->ksq_idle));
	return (NULL);
}

int
tdq_transfer(struct tdq *tdq, struct td_sched *ts, int class)
{
	struct tdq_group *nksg;
	struct tdq_group *ksg;
	struct tdq *old;
	int cpu;
	int idx;

	if (smp_started == 0)
		return (0);
	cpu = 0;
	/*
	 * If our load exceeds a certain threshold we should attempt to
	 * reassign this thread.  The first candidate is the cpu that
	 * originally ran the thread.  If it is idle, assign it there, 
	 * otherwise, pick an idle cpu.
	 *
	 * The threshold at which we start to reassign kses has a large impact
	 * on the overall performance of the system.  Tuned too high and
	 * some CPUs may idle.  Too low and there will be excess migration
	 * and context switches.
	 */
	old = TDQ_CPU(ts->ts_cpu);
	nksg = old->ksq_group;
	ksg = tdq->ksq_group;
	if (tdq_idle) {
		if (tdq_idle & nksg->ksg_mask) {
			cpu = ffs(nksg->ksg_idlemask);
			if (cpu) {
				CTR2(KTR_SCHED,
				    "tdq_transfer: %p found old cpu %X " 
				    "in idlemask.", ts, cpu);
				goto migrate;
			}
		}
		/*
		 * Multiple cpus could find this bit simultaneously
		 * but the race shouldn't be terrible.
		 */
		cpu = ffs(tdq_idle);
		if (cpu) {
			CTR2(KTR_SCHED, "tdq_transfer: %p found %X " 
			    "in idlemask.", ts, cpu);
			goto migrate;
		}
	}
	idx = 0;
#if 0
	if (old->ksq_load < tdq->ksq_load) {
		cpu = ts->ts_cpu + 1;
		CTR2(KTR_SCHED, "tdq_transfer: %p old cpu %X " 
		    "load less than ours.", ts, cpu);
		goto migrate;
	}
	/*
	 * No new CPU was found, look for one with less load.
	 */
	for (idx = 0; idx <= ksg_maxid; idx++) {
		nksg = TDQ_GROUP(idx);
		if (nksg->ksg_load /*+ (nksg->ksg_cpus  * 2)*/ < ksg->ksg_load) {
			cpu = ffs(nksg->ksg_cpumask);
			CTR2(KTR_SCHED, "tdq_transfer: %p cpu %X load less " 
			    "than ours.", ts, cpu);
			goto migrate;
		}
	}
#endif
	/*
	 * If another cpu in this group has idled, assign a thread over
	 * to them after checking to see if there are idled groups.
	 */
	if (ksg->ksg_idlemask) {
		cpu = ffs(ksg->ksg_idlemask);
		if (cpu) {
			CTR2(KTR_SCHED, "tdq_transfer: %p cpu %X idle in " 
			    "group.", ts, cpu);
			goto migrate;
		}
	}
	return (0);
migrate:
	/*
	 * Now that we've found an idle CPU, migrate the thread.
	 */
	cpu--;
	ts->ts_runq = NULL;
	tdq_notify(ts, cpu);

	return (1);
}

#endif	/* SMP */

/*
 * Pick the highest priority task we have and return it.
 */

static struct td_sched *
tdq_choose(struct tdq *tdq)
{
	struct runq *swap;
	struct td_sched *ts;
	int nice;

	mtx_assert(&sched_lock, MA_OWNED);
	swap = NULL;

	for (;;) {
		ts = runq_choose(tdq->ksq_curr);
		if (ts == NULL) {
			/*
			 * We already swapped once and didn't get anywhere.
			 */
			if (swap)
				break;
			swap = tdq->ksq_curr;
			tdq->ksq_curr = tdq->ksq_next;
			tdq->ksq_next = swap;
			continue;
		}
		/*
		 * If we encounter a slice of 0 the td_sched is in a
		 * TIMESHARE td_sched group and its nice was too far out
		 * of the range that receives slices. 
		 */
		nice = ts->ts_thread->td_proc->p_nice + (0 - tdq->ksq_nicemin);
#if 0
		if (ts->ts_slice == 0 || (nice > SCHED_SLICE_NTHRESH &&
		    ts->ts_thread->td_proc->p_nice != 0)) {
			runq_remove(ts->ts_runq, ts);
			sched_slice(ts);
			ts->ts_runq = tdq->ksq_next;
			runq_add(ts->ts_runq, ts, 0);
			continue;
		}
#endif
		return (ts);
	}

	return (runq_choose(&tdq->ksq_idle));
}

static void
tdq_setup(struct tdq *tdq)
{
	runq_init(&tdq->ksq_timeshare[0]);
	runq_init(&tdq->ksq_timeshare[1]);
	runq_init(&tdq->ksq_idle);
	tdq->ksq_curr = &tdq->ksq_timeshare[0];
	tdq->ksq_next = &tdq->ksq_timeshare[1];
	tdq->ksq_load = 0;
	tdq->ksq_load_timeshare = 0;
}

static void
sched_setup(void *dummy)
{
#ifdef SMP
	int i;
#endif

	/*
	 * To avoid divide-by-zero, we set realstathz a dummy value
	 * in case which sched_clock() called before sched_initticks().
	 */
	realstathz = hz;
	slice_min = (hz/100);	/* 10ms */
	slice_max = (hz/7);	/* ~140ms */

#ifdef SMP
	balance_groups = 0;
	/*
	 * Initialize the tdqs.
	 */
	for (i = 0; i < MAXCPU; i++) {
		struct tdq *ksq;

		ksq = &tdq_cpu[i];
		ksq->ksq_assigned = NULL;
		tdq_setup(&tdq_cpu[i]);
	}
	if (smp_topology == NULL) {
		struct tdq_group *ksg;
		struct tdq *ksq;
		int cpus;

		for (cpus = 0, i = 0; i < MAXCPU; i++) {
			if (CPU_ABSENT(i))
				continue;
			ksq = &tdq_cpu[i];
			ksg = &tdq_groups[cpus];
			/*
			 * Setup a tdq group with one member.
			 */
			ksq->ksq_transferable = 0;
			ksq->ksq_group = ksg;
			ksg->ksg_cpus = 1;
			ksg->ksg_idlemask = 0;
			ksg->ksg_cpumask = ksg->ksg_mask = 1 << i;
			ksg->ksg_load = 0;
			ksg->ksg_transferable = 0;
			LIST_INIT(&ksg->ksg_members);
			LIST_INSERT_HEAD(&ksg->ksg_members, ksq, ksq_siblings);
			cpus++;
		}
		ksg_maxid = cpus - 1;
	} else {
		struct tdq_group *ksg;
		struct cpu_group *cg;
		int j;

		for (i = 0; i < smp_topology->ct_count; i++) {
			cg = &smp_topology->ct_group[i];
			ksg = &tdq_groups[i];
			/*
			 * Initialize the group.
			 */
			ksg->ksg_idlemask = 0;
			ksg->ksg_load = 0;
			ksg->ksg_transferable = 0;
			ksg->ksg_cpus = cg->cg_count;
			ksg->ksg_cpumask = cg->cg_mask;
			LIST_INIT(&ksg->ksg_members);
			/*
			 * Find all of the group members and add them.
			 */
			for (j = 0; j < MAXCPU; j++) {
				if ((cg->cg_mask & (1 << j)) != 0) {
					if (ksg->ksg_mask == 0)
						ksg->ksg_mask = 1 << j;
					tdq_cpu[j].ksq_transferable = 0;
					tdq_cpu[j].ksq_group = ksg;
					LIST_INSERT_HEAD(&ksg->ksg_members,
					    &tdq_cpu[j], ksq_siblings);
				}
			}
			if (ksg->ksg_cpus > 1)
				balance_groups = 1;
		}
		ksg_maxid = smp_topology->ct_count - 1;
	}
	/*
	 * Stagger the group and global load balancer so they do not
	 * interfere with each other.
	 */
	bal_tick = ticks + hz;
	if (balance_groups)
		gbal_tick = ticks + (hz / 2);
#else
	tdq_setup(TDQ_SELF());
#endif
	mtx_lock_spin(&sched_lock);
	tdq_load_add(TDQ_SELF(), &td_sched0);
	mtx_unlock_spin(&sched_lock);
}

/* ARGSUSED */
static void
sched_initticks(void *dummy)
{
	mtx_lock_spin(&sched_lock);
	realstathz = stathz ? stathz : hz;
	slice_min = (realstathz/100);	/* 10ms */
	slice_max = (realstathz/7);	/* ~140ms */

	tickincr = (hz << 10) / realstathz;
	/*
	 * XXX This does not work for values of stathz that are much
	 * larger than hz.
	 */
	if (tickincr == 0)
		tickincr = 1;
	mtx_unlock_spin(&sched_lock);
}


/*
 * Scale the scheduling priority according to the "interactivity" of this
 * process.
 */
static void
sched_priority(struct thread *td)
{
	int pri;

	if (td->td_pri_class != PRI_TIMESHARE)
		return;

	pri = SCHED_PRI_INTERACT(sched_interact_score(td));
	pri += SCHED_PRI_BASE;
	pri += td->td_proc->p_nice;

	if (pri > PRI_MAX_TIMESHARE)
		pri = PRI_MAX_TIMESHARE;
	else if (pri < PRI_MIN_TIMESHARE)
		pri = PRI_MIN_TIMESHARE;

	sched_user_prio(td, pri);

	return;
}

/*
 * Calculate a time slice based on the properties of the process
 * and the runq that we're on.  This is only for PRI_TIMESHARE threads.
 */
static void
sched_slice(struct td_sched *ts)
{
	struct tdq *tdq;
	struct thread *td;

	td = ts->ts_thread;
	tdq = TDQ_CPU(ts->ts_cpu);

	if (td->td_flags & TDF_BORROWING) {
		ts->ts_slice = SCHED_SLICE_MIN;
		return;
	}

	/*
	 * Rationale:
	 * Threads in interactive procs get a minimal slice so that we
	 * quickly notice if it abuses its advantage.
	 *
	 * Threads in non-interactive procs are assigned a slice that is
	 * based on the procs nice value relative to the least nice procs
	 * on the run queue for this cpu.
	 *
	 * If the thread is less nice than all others it gets the maximum
	 * slice and other threads will adjust their slice relative to
	 * this when they first expire.
	 *
	 * There is 20 point window that starts relative to the least
	 * nice td_sched on the run queue.  Slice size is determined by
	 * the td_sched distance from the last nice thread.
	 *
	 * If the td_sched is outside of the window it will get no slice
	 * and will be reevaluated each time it is selected on the
	 * run queue.  The exception to this is nice 0 procs when
	 * a nice -20 is running.  They are always granted a minimum
	 * slice.
	 */
	if (!SCHED_INTERACTIVE(td)) {
		int nice;

		nice = td->td_proc->p_nice + (0 - tdq->ksq_nicemin);
		if (tdq->ksq_load_timeshare == 0 ||
		    td->td_proc->p_nice < tdq->ksq_nicemin)
			ts->ts_slice = SCHED_SLICE_MAX;
		else if (nice <= SCHED_SLICE_NTHRESH)
			ts->ts_slice = SCHED_SLICE_NICE(nice);
		else if (td->td_proc->p_nice == 0)
			ts->ts_slice = SCHED_SLICE_MIN;
		else
			ts->ts_slice = SCHED_SLICE_MIN; /* 0 */
	} else
		ts->ts_slice = SCHED_SLICE_INTERACTIVE;

	return;
}

/*
 * This routine enforces a maximum limit on the amount of scheduling history
 * kept.  It is called after either the slptime or runtime is adjusted.
 * This routine will not operate correctly when slp or run times have been
 * adjusted to more than double their maximum.
 */
static void
sched_interact_update(struct thread *td)
{
	int sum;

	sum = td->td_sched->skg_runtime + td->td_sched->skg_slptime;
	if (sum < SCHED_SLP_RUN_MAX)
		return;
	/*
	 * If we have exceeded by more than 1/5th then the algorithm below
	 * will not bring us back into range.  Dividing by two here forces
	 * us into the range of [4/5 * SCHED_INTERACT_MAX, SCHED_INTERACT_MAX]
	 */
	if (sum > (SCHED_SLP_RUN_MAX / 5) * 6) {
		td->td_sched->skg_runtime /= 2;
		td->td_sched->skg_slptime /= 2;
		return;
	}
	td->td_sched->skg_runtime = (td->td_sched->skg_runtime / 5) * 4;
	td->td_sched->skg_slptime = (td->td_sched->skg_slptime / 5) * 4;
}

static void
sched_interact_fork(struct thread *td)
{
	int ratio;
	int sum;

	sum = td->td_sched->skg_runtime + td->td_sched->skg_slptime;
	if (sum > SCHED_SLP_RUN_FORK) {
		ratio = sum / SCHED_SLP_RUN_FORK;
		td->td_sched->skg_runtime /= ratio;
		td->td_sched->skg_slptime /= ratio;
	}
}

static int
sched_interact_score(struct thread *td)
{
	int div;

	if (td->td_sched->skg_runtime > td->td_sched->skg_slptime) {
		div = max(1, td->td_sched->skg_runtime / SCHED_INTERACT_HALF);
		return (SCHED_INTERACT_HALF +
		    (SCHED_INTERACT_HALF - (td->td_sched->skg_slptime / div)));
	} if (td->td_sched->skg_slptime > td->td_sched->skg_runtime) {
		div = max(1, td->td_sched->skg_slptime / SCHED_INTERACT_HALF);
		return (td->td_sched->skg_runtime / div);
	}

	/*
	 * This can happen if slptime and runtime are 0.
	 */
	return (0);

}

/*
 * Very early in the boot some setup of scheduler-specific
 * parts of proc0 and of soem scheduler resources needs to be done.
 * Called from:
 *  proc0_init()
 */
void
schedinit(void)
{
	/*
	 * Set up the scheduler specific parts of proc0.
	 */
	proc0.p_sched = NULL; /* XXX */
	thread0.td_sched = &td_sched0;
	td_sched0.ts_thread = &thread0;
	td_sched0.ts_state = TSS_THREAD;
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

static void
sched_pctcpu_update(struct td_sched *ts)
{
	/*
	 * Adjust counters and watermark for pctcpu calc.
	 */
	if (ts->ts_ltick > ticks - SCHED_CPU_TICKS) {
		/*
		 * Shift the tick count out so that the divide doesn't
		 * round away our results.
		 */
		ts->ts_ticks <<= 10;
		ts->ts_ticks = (ts->ts_ticks / (ticks - ts->ts_ftick)) *
			    SCHED_CPU_TICKS;
		ts->ts_ticks >>= 10;
	} else
		ts->ts_ticks = 0;
	ts->ts_ltick = ticks;
	ts->ts_ftick = ts->ts_ltick - SCHED_CPU_TICKS;
}

void
sched_thread_priority(struct thread *td, u_char prio)
{
	struct td_sched *ts;

	CTR6(KTR_SCHED, "sched_prio: %p(%s) prio %d newprio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, prio, curthread,
	    curthread->td_proc->p_comm);
	ts = td->td_sched;
	mtx_assert(&sched_lock, MA_OWNED);
	if (td->td_priority == prio)
		return;
	if (TD_ON_RUNQ(td)) {
		/*
		 * If the priority has been elevated due to priority
		 * propagation, we may have to move ourselves to a new
		 * queue.  We still call adjustrunqueue below in case kse
		 * needs to fix things up.
		 */
		if (prio < td->td_priority && ts->ts_runq != NULL &&
		    (ts->ts_flags & TSF_ASSIGNED) == 0 &&
		    ts->ts_runq != TDQ_CPU(ts->ts_cpu)->ksq_curr) {
			runq_remove(ts->ts_runq, ts);
			ts->ts_runq = TDQ_CPU(ts->ts_cpu)->ksq_curr;
			runq_add(ts->ts_runq, ts, 0);
		}
		/*
		 * Hold this td_sched on this cpu so that sched_prio() doesn't
		 * cause excessive migration.  We only want migration to
		 * happen as the result of a wakeup.
		 */
		ts->ts_flags |= TSF_HOLD;
		adjustrunqueue(td, prio);
		ts->ts_flags &= ~TSF_HOLD;
	} else
		td->td_priority = prio;
}

/*
 * Update a thread's priority when it is lent another thread's
 * priority.
 */
void
sched_lend_prio(struct thread *td, u_char prio)
{

	td->td_flags |= TDF_BORROWING;
	sched_thread_priority(td, prio);
}

/*
 * Restore a thread's priority when priority propagation is
 * over.  The prio argument is the minimum priority the thread
 * needs to have to satisfy other possible priority lending
 * requests.  If the thread's regular priority is less
 * important than prio, the thread will keep a priority boost
 * of prio.
 */
void
sched_unlend_prio(struct thread *td, u_char prio)
{
	u_char base_pri;

	if (td->td_base_pri >= PRI_MIN_TIMESHARE &&
	    td->td_base_pri <= PRI_MAX_TIMESHARE)
		base_pri = td->td_user_pri;
	else
		base_pri = td->td_base_pri;
	if (prio >= base_pri) {
		td->td_flags &= ~TDF_BORROWING;
		sched_thread_priority(td, base_pri);
	} else
		sched_lend_prio(td, prio);
}

void
sched_prio(struct thread *td, u_char prio)
{
	u_char oldprio;

	/* First, update the base priority. */
	td->td_base_pri = prio;

	/*
	 * If the thread is borrowing another thread's priority, don't
	 * ever lower the priority.
	 */
	if (td->td_flags & TDF_BORROWING && td->td_priority < prio)
		return;

	/* Change the real priority. */
	oldprio = td->td_priority;
	sched_thread_priority(td, prio);

	/*
	 * If the thread is on a turnstile, then let the turnstile update
	 * its state.
	 */
	if (TD_ON_LOCK(td) && oldprio != prio)
		turnstile_adjust(td, oldprio);
}

void
sched_user_prio(struct thread *td, u_char prio)
{
	u_char oldprio;

	td->td_base_user_pri = prio;
	if (td->td_flags & TDF_UBORROWING && td->td_user_pri <= prio)
                return;
	oldprio = td->td_user_pri;
	td->td_user_pri = prio;

	if (TD_ON_UPILOCK(td) && oldprio != prio)
		umtx_pi_adjust(td, oldprio);
}

void
sched_lend_user_prio(struct thread *td, u_char prio)
{
	u_char oldprio;

	td->td_flags |= TDF_UBORROWING;

	oldprio = td->td_user_pri;
	td->td_user_pri = prio;

	if (TD_ON_UPILOCK(td) && oldprio != prio)
		umtx_pi_adjust(td, oldprio);
}

void
sched_unlend_user_prio(struct thread *td, u_char prio)
{
	u_char base_pri;

	base_pri = td->td_base_user_pri;
	if (prio >= base_pri) {
		td->td_flags &= ~TDF_UBORROWING;
		sched_user_prio(td, base_pri);
	} else
		sched_lend_user_prio(td, prio);
}

void
sched_switch(struct thread *td, struct thread *newtd, int flags)
{
	struct tdq *ksq;
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);

	ts = td->td_sched;
	ksq = TDQ_SELF();

	td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;

	/*
	 * If the thread has been assigned it may be in the process of switching
	 * to the new cpu.  This is the case in sched_bind().
	 */
	if (td == PCPU_GET(idlethread)) {
		TD_SET_CAN_RUN(td);
	} else if ((ts->ts_flags & TSF_ASSIGNED) == 0) {
		/* We are ending our run so make our slot available again */
		tdq_load_rem(ksq, ts);
		if (TD_IS_RUNNING(td)) {
			/*
			 * Don't allow the thread to migrate
			 * from a preemption.
			 */
			ts->ts_flags |= TSF_HOLD;
			setrunqueue(td, (flags & SW_PREEMPT) ?
			    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
			    SRQ_OURSELF|SRQ_YIELDING);
			ts->ts_flags &= ~TSF_HOLD;
		}
	}
	if (newtd != NULL) {
		/*
		 * If we bring in a thread account for it as if it had been
		 * added to the run queue and then chosen.
		 */
		newtd->td_sched->ts_flags |= TSF_DIDRUN;
		newtd->td_sched->ts_runq = ksq->ksq_curr;
		TD_SET_RUNNING(newtd);
		tdq_load_add(TDQ_SELF(), newtd->td_sched);
	} else
		newtd = choosethread();
	if (td != newtd) {
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_OUT);
#endif

		cpu_switch(td, newtd);
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_IN);
#endif
	}

	sched_lock.mtx_lock = (uintptr_t)td;

	td->td_oncpu = PCPU_GET(cpuid);
}

void
sched_nice(struct proc *p, int nice)
{
	struct td_sched *ts;
	struct thread *td;
	struct tdq *tdq;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	/*
	 * We need to adjust the nice counts for running threads.
	 */
	FOREACH_THREAD_IN_PROC(p, td) {
		if (td->td_pri_class == PRI_TIMESHARE) {
			ts = td->td_sched;
			if (ts->ts_runq == NULL)
				continue;
			tdq = TDQ_CPU(ts->ts_cpu);
			tdq_nice_rem(tdq, p->p_nice);
			tdq_nice_add(tdq, nice);
		}
	}
	p->p_nice = nice;
	FOREACH_THREAD_IN_PROC(p, td) {
		sched_priority(td);
		td->td_flags |= TDF_NEEDRESCHED;
	}
}

void
sched_sleep(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_sched->ts_slptime = ticks;
}

void
sched_wakeup(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);

	/*
	 * Let the procs know how long we slept for.  This is because process
	 * interactivity behavior is modeled in the procs.
	 */
	if (td->td_sched->ts_slptime) {
		int hzticks;

		hzticks = (ticks - td->td_sched->ts_slptime) << 10;
		if (hzticks >= SCHED_SLP_RUN_MAX) {
			td->td_sched->skg_slptime = SCHED_SLP_RUN_MAX;
			td->td_sched->skg_runtime = 1;
		} else {
			td->td_sched->skg_slptime += hzticks;
			sched_interact_update(td);
		}
		sched_priority(td);
		sched_slice(td->td_sched);
		td->td_sched->ts_slptime = 0;
	}
	setrunqueue(td, SRQ_BORING);
}

/*
 * Penalize the parent for creating a new child and initialize the child's
 * priority.
 */
void
sched_fork(struct thread *td, struct thread *child)
{
	mtx_assert(&sched_lock, MA_OWNED);
	sched_fork_thread(td, child);
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
	struct td_sched *ts;
	struct td_sched *ts2;

	child->td_sched->skg_slptime = td->td_sched->skg_slptime;
	child->td_sched->skg_runtime = td->td_sched->skg_runtime;
	child->td_user_pri = td->td_user_pri;
	child->td_base_user_pri = td->td_base_user_pri;
	sched_interact_fork(child);
	td->td_sched->skg_runtime += tickincr;
	sched_interact_update(td);

	sched_newthread(child);

	ts = td->td_sched;
	ts2 = child->td_sched;
	ts2->ts_slice = 1;	/* Attempt to quickly learn interactivity. */
	ts2->ts_cpu = ts->ts_cpu;
	ts2->ts_runq = NULL;

	/* Grab our parents cpu estimation information. */
	ts2->ts_ticks = ts->ts_ticks;
	ts2->ts_ltick = ts->ts_ltick;
	ts2->ts_ftick = ts->ts_ftick;
}

void
sched_class(struct thread *td, int class)
{
	struct tdq *tdq;
	struct td_sched *ts;
	int nclass;
	int oclass;

	mtx_assert(&sched_lock, MA_OWNED);
	if (td->td_pri_class == class)
		return;

	nclass = PRI_BASE(class);
	oclass = PRI_BASE(td->td_pri_class);
	ts = td->td_sched;
	if (!((ts->ts_state != TSS_ONRUNQ &&
	    ts->ts_state != TSS_THREAD) || ts->ts_runq == NULL)) {
		tdq = TDQ_CPU(ts->ts_cpu);

#ifdef SMP
		/*
		 * On SMP if we're on the RUNQ we must adjust the transferable
		 * count because could be changing to or from an interrupt
		 * class.
		 */
		if (ts->ts_state == TSS_ONRUNQ) {
			if (THREAD_CAN_MIGRATE(ts)) {
				tdq->ksq_transferable--;
				tdq->ksq_group->ksg_transferable--;
			}
			if (THREAD_CAN_MIGRATE(ts)) {
				tdq->ksq_transferable++;
				tdq->ksq_group->ksg_transferable++;
			}
		}
#endif
		if (oclass == PRI_TIMESHARE) {
			tdq->ksq_load_timeshare--;
			tdq_nice_rem(tdq, td->td_proc->p_nice);
		}
		if (nclass == PRI_TIMESHARE) {
			tdq->ksq_load_timeshare++;
			tdq_nice_add(tdq, td->td_proc->p_nice);
		}
	}

	td->td_pri_class = class;
}

/*
 * Return some of the child's priority and interactivity to the parent.
 */
void
sched_exit(struct proc *p, struct thread *child)
{
	
	CTR3(KTR_SCHED, "sched_exit: %p(%s) prio %d",
	    child, child->td_proc->p_comm, child->td_priority);

	sched_exit_thread(FIRST_THREAD_IN_PROC(p), child);
}

void
sched_exit_thread(struct thread *td, struct thread *child)
{
	CTR3(KTR_SCHED, "sched_exit_thread: %p(%s) prio %d",
	    child, childproc->p_comm, child->td_priority);

	td->td_sched->skg_runtime += child->td_sched->skg_runtime;
	sched_interact_update(td);
	tdq_load_rem(TDQ_CPU(child->td_sched->ts_cpu), child->td_sched);
}

void
sched_userret(struct thread *td)
{
	/*
	 * XXX we cheat slightly on the locking here to avoid locking in  
	 * the usual case.  Setting td_priority here is essentially an
	 * incomplete workaround for not setting it properly elsewhere.
	 * Now that some interrupt handlers are threads, not setting it
	 * properly elsewhere can clobber it in the window between setting
	 * it here and returning to user mode, so don't waste time setting
	 * it perfectly here.
	 */
	KASSERT((td->td_flags & TDF_BORROWING) == 0,
	    ("thread with borrowed priority returning to userland"));
	if (td->td_priority != td->td_user_pri) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = td->td_user_pri;
		td->td_base_pri = td->td_user_pri;
		mtx_unlock_spin(&sched_lock);
        }
}

void
sched_clock(struct thread *td)
{
	struct tdq *tdq;
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	tdq = TDQ_SELF();
#ifdef SMP
	if (ticks >= bal_tick)
		sched_balance();
	if (ticks >= gbal_tick && balance_groups)
		sched_balance_groups();
	/*
	 * We could have been assigned a non real-time thread without an
	 * IPI.
	 */
	if (tdq->ksq_assigned)
		tdq_assign(tdq);	/* Potentially sets NEEDRESCHED */
#endif
	ts = td->td_sched;

	/* Adjust ticks for pctcpu */
	ts->ts_ticks++;
	ts->ts_ltick = ticks;

	/* Go up to one second beyond our max and then trim back down */
	if (ts->ts_ftick + SCHED_CPU_TICKS + hz < ts->ts_ltick)
		sched_pctcpu_update(ts);

	if (td->td_flags & TDF_IDLETD)
		return;
	/*
	 * We only do slicing code for TIMESHARE threads.
	 */
	if (td->td_pri_class != PRI_TIMESHARE)
		return;
	/*
	 * We used a tick charge it to the thread so that we can compute our
	 * interactivity.
	 */
	td->td_sched->skg_runtime += tickincr;
	sched_interact_update(td);

	/*
	 * We used up one time slice.
	 */
	if (--ts->ts_slice > 0)
		return;
	/*
	 * We're out of time, recompute priorities and requeue.
	 */
	tdq_load_rem(tdq, ts);
	sched_priority(td);
	sched_slice(ts);
	if (SCHED_CURR(td, ts))
		ts->ts_runq = tdq->ksq_curr;
	else
		ts->ts_runq = tdq->ksq_next;
	tdq_load_add(tdq, ts);
	td->td_flags |= TDF_NEEDRESCHED;
}

int
sched_runnable(void)
{
	struct tdq *tdq;
	int load;

	load = 1;

	tdq = TDQ_SELF();
#ifdef SMP
	if (tdq->ksq_assigned) {
		mtx_lock_spin(&sched_lock);
		tdq_assign(tdq);
		mtx_unlock_spin(&sched_lock);
	}
#endif
	if ((curthread->td_flags & TDF_IDLETD) != 0) {
		if (tdq->ksq_load > 0)
			goto out;
	} else
		if (tdq->ksq_load - 1 > 0)
			goto out;
	load = 0;
out:
	return (load);
}

struct td_sched *
sched_choose(void)
{
	struct tdq *tdq;
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	tdq = TDQ_SELF();
#ifdef SMP
restart:
	if (tdq->ksq_assigned)
		tdq_assign(tdq);
#endif
	ts = tdq_choose(tdq);
	if (ts) {
#ifdef SMP
		if (ts->ts_thread->td_pri_class == PRI_IDLE)
			if (tdq_idled(tdq) == 0)
				goto restart;
#endif
		tdq_runq_rem(tdq, ts);
		ts->ts_state = TSS_THREAD;
		ts->ts_flags &= ~TSF_PREEMPTED;
		return (ts);
	}
#ifdef SMP
	if (tdq_idled(tdq) == 0)
		goto restart;
#endif
	return (NULL);
}

void
sched_add(struct thread *td, int flags)
{
	struct tdq *tdq;
	struct td_sched *ts;
	int preemptive;
	int canmigrate;
	int class;

	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	mtx_assert(&sched_lock, MA_OWNED);
	ts = td->td_sched;
	canmigrate = 1;
	preemptive = !(flags & SRQ_YIELDING);
	class = PRI_BASE(td->td_pri_class);
	tdq = TDQ_SELF();
	ts->ts_flags &= ~TSF_INTERNAL;
#ifdef SMP
	if (ts->ts_flags & TSF_ASSIGNED) {
		if (ts->ts_flags & TSF_REMOVED)
			ts->ts_flags &= ~TSF_REMOVED;
		return;
	}
	canmigrate = THREAD_CAN_MIGRATE(ts);
	/*
	 * Don't migrate running threads here.  Force the long term balancer
	 * to do it.
	 */
	if (ts->ts_flags & TSF_HOLD) {
		ts->ts_flags &= ~TSF_HOLD;
		canmigrate = 0;
	}
#endif
	KASSERT(ts->ts_state != TSS_ONRUNQ,
	    ("sched_add: thread %p (%s) already in run queue", td,
	    td->td_proc->p_comm));
	KASSERT(td->td_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	KASSERT(ts->ts_runq == NULL,
	    ("sched_add: thread %p is still assigned to a run queue", td));
	if (flags & SRQ_PREEMPTED)
		ts->ts_flags |= TSF_PREEMPTED;
	switch (class) {
	case PRI_ITHD:
	case PRI_REALTIME:
		ts->ts_runq = tdq->ksq_curr;
		ts->ts_slice = SCHED_SLICE_MAX;
		if (canmigrate)
			ts->ts_cpu = PCPU_GET(cpuid);
		break;
	case PRI_TIMESHARE:
		if (SCHED_CURR(td, ts))
			ts->ts_runq = tdq->ksq_curr;
		else
			ts->ts_runq = tdq->ksq_next;
		break;
	case PRI_IDLE:
		/*
		 * This is for priority prop.
		 */
		if (ts->ts_thread->td_priority < PRI_MIN_IDLE)
			ts->ts_runq = tdq->ksq_curr;
		else
			ts->ts_runq = &tdq->ksq_idle;
		ts->ts_slice = SCHED_SLICE_MIN;
		break;
	default:
		panic("Unknown pri class.");
		break;
	}
#ifdef SMP
	/*
	 * If this thread is pinned or bound, notify the target cpu.
	 */
	if (!canmigrate && ts->ts_cpu != PCPU_GET(cpuid) ) {
		ts->ts_runq = NULL;
		tdq_notify(ts, ts->ts_cpu);
		return;
	}
	/*
	 * If we had been idle, clear our bit in the group and potentially
	 * the global bitmap.  If not, see if we should transfer this thread.
	 */
	if ((class == PRI_TIMESHARE || class == PRI_REALTIME) &&
	    (tdq->ksq_group->ksg_idlemask & PCPU_GET(cpumask)) != 0) {
		/*
		 * Check to see if our group is unidling, and if so, remove it
		 * from the global idle mask.
		 */
		if (tdq->ksq_group->ksg_idlemask ==
		    tdq->ksq_group->ksg_cpumask)
			atomic_clear_int(&tdq_idle, tdq->ksq_group->ksg_mask);
		/*
		 * Now remove ourselves from the group specific idle mask.
		 */
		tdq->ksq_group->ksg_idlemask &= ~PCPU_GET(cpumask);
	} else if (canmigrate && tdq->ksq_load > 1 && class != PRI_ITHD)
		if (tdq_transfer(tdq, ts, class))
			return;
	ts->ts_cpu = PCPU_GET(cpuid);
#endif
	if (td->td_priority < curthread->td_priority &&
	    ts->ts_runq == tdq->ksq_curr)
		curthread->td_flags |= TDF_NEEDRESCHED;
	if (preemptive && maybe_preempt(td))
		return;
	ts->ts_state = TSS_ONRUNQ;

	tdq_runq_add(tdq, ts, flags);
	tdq_load_add(tdq, ts);
}

void
sched_rem(struct thread *td)
{
	struct tdq *tdq;
	struct td_sched *ts;

	CTR5(KTR_SCHED, "sched_rem: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	mtx_assert(&sched_lock, MA_OWNED);
	ts = td->td_sched;
	ts->ts_flags &= ~TSF_PREEMPTED;
	if (ts->ts_flags & TSF_ASSIGNED) {
		ts->ts_flags |= TSF_REMOVED;
		return;
	}
	KASSERT((ts->ts_state == TSS_ONRUNQ),
	    ("sched_rem: thread not on run queue"));

	ts->ts_state = TSS_THREAD;
	tdq = TDQ_CPU(ts->ts_cpu);
	tdq_runq_rem(tdq, ts);
	tdq_load_rem(tdq, ts);
}

fixpt_t
sched_pctcpu(struct thread *td)
{
	fixpt_t pctcpu;
	struct td_sched *ts;

	pctcpu = 0;
	ts = td->td_sched;
	if (ts == NULL)
		return (0);

	mtx_lock_spin(&sched_lock);
	if (ts->ts_ticks) {
		int rtick;

		/*
		 * Don't update more frequently than twice a second.  Allowing
		 * this causes the cpu usage to decay away too quickly due to
		 * rounding errors.
		 */
		if (ts->ts_ftick + SCHED_CPU_TICKS < ts->ts_ltick ||
		    ts->ts_ltick < (ticks - (hz / 2)))
			sched_pctcpu_update(ts);
		/* How many rtick per second ? */
		rtick = min(ts->ts_ticks / SCHED_CPU_TIME, SCHED_CPU_TICKS);
		pctcpu = (FSCALE * ((FSCALE * rtick)/realstathz)) >> FSHIFT;
	}

	td->td_proc->p_swtime = ts->ts_ltick - ts->ts_ftick;
	mtx_unlock_spin(&sched_lock);

	return (pctcpu);
}

void
sched_bind(struct thread *td, int cpu)
{
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	ts = td->td_sched;
	ts->ts_flags |= TSF_BOUND;
#ifdef SMP
	if (PCPU_GET(cpuid) == cpu)
		return;
	/* sched_rem without the runq_remove */
	ts->ts_state = TSS_THREAD;
	tdq_load_rem(TDQ_CPU(ts->ts_cpu), ts);
	tdq_notify(ts, cpu);
	/* When we return from mi_switch we'll be on the correct cpu. */
	mi_switch(SW_VOL, NULL);
#endif
}

void
sched_unbind(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);
	td->td_sched->ts_flags &= ~TSF_BOUND;
}

int
sched_is_bound(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);
	return (td->td_sched->ts_flags & TSF_BOUND);
}

void
sched_relinquish(struct thread *td)
{
	mtx_lock_spin(&sched_lock);
	if (td->td_pri_class == PRI_TIMESHARE)
		sched_prio(td, PRI_MAX_TIMESHARE);
	mi_switch(SW_VOL, NULL);
	mtx_unlock_spin(&sched_lock);
}

int
sched_load(void)
{
#ifdef SMP
	int total;
	int i;

	total = 0;
	for (i = 0; i <= ksg_maxid; i++)
		total += TDQ_GROUP(i)->ksg_load;
	return (total);
#else
	return (TDQ_SELF()->ksq_sysload);
#endif
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

void
sched_tick(void)
{
}
#define KERN_SWITCH_INCLUDE 1
#include "kern/kern_switch.c"
