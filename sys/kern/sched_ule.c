/*-
 * Copyright (c) 2002-2007, Jeffrey Roberson <jeff@freebsd.org>
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

#ifndef PREEMPTION
#error	"SCHED_ULE requires options PREEMPTION"
#endif

/*
 * TODO:
 *	Pick idle from affinity group or self group first.
 *	Implement pick_score.
 */

#define	KTR_ULE	0x0		/* Enable for pickpri debugging. */

/*
 * Thread scheduler specific section.
 */
struct td_sched {	
	TAILQ_ENTRY(td_sched) ts_procq;	/* (j/z) Run queue. */
	int		ts_flags;	/* (j) TSF_* flags. */
	struct thread	*ts_thread;	/* (*) Active associated thread. */
	u_char		ts_rqindex;	/* (j) Run queue index. */
	int		ts_slptime;
	int		ts_slice;
	struct runq	*ts_runq;
	u_char		ts_cpu;		/* CPU that we have affinity for. */
	/* The following variables are only used for pctcpu calculation */
	int		ts_ltick;	/* Last tick that we were running on */
	int		ts_ftick;	/* First tick that we were running on */
	int		ts_ticks;	/* Tick count */
#ifdef SMP
	int		ts_rltick;	/* Real last tick, for affinity. */
#endif

	/* originally from kg_sched */
	u_int	skg_slptime;		/* Number of ticks we vol. slept */
	u_int	skg_runtime;		/* Number of ticks we were running */
};
/* flags kept in ts_flags */
#define	TSF_BOUND	0x0001		/* Thread can not migrate. */
#define	TSF_XFERABLE	0x0002		/* Thread was added as transferable. */

static struct td_sched td_sched0;

/*
 * Cpu percentage computation macros and defines.
 *
 * SCHED_TICK_SECS:	Number of seconds to average the cpu usage across.
 * SCHED_TICK_TARG:	Number of hz ticks to average the cpu usage across.
 * SCHED_TICK_MAX:	Maximum number of ticks before scaling back.
 * SCHED_TICK_SHIFT:	Shift factor to avoid rounding away results.
 * SCHED_TICK_HZ:	Compute the number of hz ticks for a given ticks count.
 * SCHED_TICK_TOTAL:	Gives the amount of time we've been recording ticks.
 */
#define	SCHED_TICK_SECS		10
#define	SCHED_TICK_TARG		(hz * SCHED_TICK_SECS)
#define	SCHED_TICK_MAX		(SCHED_TICK_TARG + hz)
#define	SCHED_TICK_SHIFT	10
#define	SCHED_TICK_HZ(ts)	((ts)->ts_ticks >> SCHED_TICK_SHIFT)
#define	SCHED_TICK_TOTAL(ts)	(max((ts)->ts_ltick - (ts)->ts_ftick, hz))

/*
 * These macros determine priorities for non-interactive threads.  They are
 * assigned a priority based on their recent cpu utilization as expressed
 * by the ratio of ticks to the tick total.  NHALF priorities at the start
 * and end of the MIN to MAX timeshare range are only reachable with negative
 * or positive nice respectively.
 *
 * PRI_RANGE:	Priority range for utilization dependent priorities.
 * PRI_NRESV:	Number of nice values.
 * PRI_TICKS:	Compute a priority in PRI_RANGE from the ticks count and total.
 * PRI_NICE:	Determines the part of the priority inherited from nice.
 */
#define	SCHED_PRI_NRESV		(PRIO_MAX - PRIO_MIN)
#define	SCHED_PRI_NHALF		(SCHED_PRI_NRESV / 2)
#define	SCHED_PRI_MIN		(PRI_MIN_TIMESHARE + SCHED_PRI_NHALF)
#define	SCHED_PRI_MAX		(PRI_MAX_TIMESHARE - SCHED_PRI_NHALF)
#define	SCHED_PRI_RANGE		(SCHED_PRI_MAX - SCHED_PRI_MIN + 1)
#define	SCHED_PRI_TICKS(ts)						\
    (SCHED_TICK_HZ((ts)) /						\
    (roundup(SCHED_TICK_TOTAL((ts)), SCHED_PRI_RANGE) / SCHED_PRI_RANGE))
#define	SCHED_PRI_NICE(nice)	(nice)

/*
 * These determine the interactivity of a process.  Interactivity differs from
 * cpu utilization in that it expresses the voluntary time slept vs time ran
 * while cpu utilization includes all time not running.  This more accurately
 * models the intent of the thread.
 *
 * SLP_RUN_MAX:	Maximum amount of sleep time + run time we'll accumulate
 *		before throttling back.
 * SLP_RUN_FORK:	Maximum slp+run time to inherit at fork time.
 * INTERACT_MAX:	Maximum interactivity value.  Smaller is better.
 * INTERACT_THRESH:	Threshhold for placement on the current runq.
 */
#define	SCHED_SLP_RUN_MAX	((hz * 5) << SCHED_TICK_SHIFT)
#define	SCHED_SLP_RUN_FORK	((hz / 2) << SCHED_TICK_SHIFT)
#define	SCHED_INTERACT_MAX	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_MAX / 2)
#define	SCHED_INTERACT_THRESH	(30)

/*
 * tickincr:		Converts a stathz tick into a hz domain scaled by
 *			the shift factor.  Without the shift the error rate
 *			due to rounding would be unacceptably high.
 * realstathz:		stathz is sometimes 0 and run off of hz.
 * sched_slice:		Runtime of each thread before rescheduling.
 */
static int sched_interact = SCHED_INTERACT_THRESH;
static int realstathz;
static int tickincr;
static int sched_slice;

/*
 * tdq - per processor runqs and statistics.
 */
struct tdq {
	struct runq	tdq_idle;		/* Queue of IDLE threads. */
	struct runq	tdq_timeshare;		/* timeshare run queue. */
	struct runq	tdq_realtime;		/* real-time run queue. */
	u_char		tdq_idx;		/* Current insert index. */
	u_char		tdq_ridx;		/* Current removal index. */
	short		tdq_flags;		/* Thread queue flags */
	int		tdq_load;		/* Aggregate load. */
#ifdef SMP
	int		tdq_transferable;
	LIST_ENTRY(tdq)	tdq_siblings;		/* Next in tdq group. */
	struct tdq_group *tdq_group;		/* Our processor group. */
#else
	int		tdq_sysload;		/* For loadavg, !ITHD load. */
#endif
};

#define	TDQF_BUSY	0x0001			/* Queue is marked as busy */

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
	int	tdg_cpus;		/* Count of CPUs in this tdq group. */
	cpumask_t tdg_cpumask;		/* Mask of cpus in this group. */
	cpumask_t tdg_idlemask;		/* Idle cpus in this group. */
	cpumask_t tdg_mask;		/* Bit mask for first cpu. */
	int	tdg_load;		/* Total load of this group. */
	int	tdg_transferable;	/* Transferable load of this group. */
	LIST_HEAD(, tdq) tdg_members;	/* Linked list of all members. */
};

#define	SCHED_AFFINITY_DEFAULT	(hz / 100)
#define	SCHED_AFFINITY(ts)	((ts)->ts_rltick > ticks - affinity)

/*
 * Run-time tunables.
 */
static int rebalance = 0;
static int pick_pri = 0;
static int affinity;
static int tryself = 1;
static int tryselfidle = 1;
static int ipi_ast = 0;
static int ipi_preempt = 1;
static int ipi_thresh = PRI_MIN_KERN;
static int steal_htt = 1;
static int steal_busy = 1;
static int busy_thresh = 4;
static int topology = 0;

/*
 * One thread queue per processor.
 */
static volatile cpumask_t tdq_idle;
static volatile cpumask_t tdq_busy;
static int tdg_maxid;
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

#define	TDQ_ID(x)	(0)
#define	TDQ_SELF()	(&tdq_cpu)
#define	TDQ_CPU(x)	(&tdq_cpu)
#endif

static void sched_priority(struct thread *);
static void sched_thread_priority(struct thread *, u_char);
static int sched_interact_score(struct thread *);
static void sched_interact_update(struct thread *);
static void sched_interact_fork(struct thread *);
static void sched_pctcpu_update(struct td_sched *);
static inline void sched_pin_td(struct thread *td);
static inline void sched_unpin_td(struct thread *td);

/* Operations on per processor queues */
static struct td_sched * tdq_choose(struct tdq *);
static void tdq_setup(struct tdq *);
static void tdq_load_add(struct tdq *, struct td_sched *);
static void tdq_load_rem(struct tdq *, struct td_sched *);
static __inline void tdq_runq_add(struct tdq *, struct td_sched *, int);
static __inline void tdq_runq_rem(struct tdq *, struct td_sched *);
void tdq_print(int cpu);
static void runq_print(struct runq *rq);
#ifdef SMP
static int tdq_pickidle(struct tdq *, struct td_sched *);
static int tdq_pickpri(struct tdq *, struct td_sched *, int);
static struct td_sched *runq_steal(struct runq *);
static void sched_balance(void);
static void sched_balance_groups(void);
static void sched_balance_group(struct tdq_group *);
static void sched_balance_pair(struct tdq *, struct tdq *);
static void sched_smp_tick(struct thread *);
static void tdq_move(struct tdq *, int);
static int tdq_idled(struct tdq *);
static void tdq_notify(struct td_sched *);
static struct td_sched *tdq_steal(struct tdq *, int);

#define	THREAD_CAN_MIGRATE(td)	 ((td)->td_pinned == 0)
#endif

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

static void sched_initticks(void *dummy);
SYSINIT(sched_initticks, SI_SUB_CLOCKS, SI_ORDER_THIRD, sched_initticks, NULL)

static inline void
sched_pin_td(struct thread *td)
{
	td->td_pinned++;
}

static inline void
sched_unpin_td(struct thread *td)
{
	td->td_pinned--;
}

static void
runq_print(struct runq *rq)
{
	struct rqhead *rqh;
	struct td_sched *ts;
	int pri;
	int j;
	int i;

	for (i = 0; i < RQB_LEN; i++) {
		printf("\t\trunq bits %d 0x%zx\n",
		    i, rq->rq_status.rqb_bits[i]);
		for (j = 0; j < RQB_BPW; j++)
			if (rq->rq_status.rqb_bits[i] & (1ul << j)) {
				pri = j + (i << RQB_L2BPW);
				rqh = &rq->rq_queues[pri];
				TAILQ_FOREACH(ts, rqh, ts_procq) {
					printf("\t\t\ttd %p(%s) priority %d rqindex %d pri %d\n",
					    ts->ts_thread, ts->ts_thread->td_proc->p_comm, ts->ts_thread->td_priority, ts->ts_rqindex, pri);
				}
			}
	}
}

void
tdq_print(int cpu)
{
	struct tdq *tdq;

	tdq = TDQ_CPU(cpu);

	printf("tdq:\n");
	printf("\tload:           %d\n", tdq->tdq_load);
	printf("\ttimeshare idx: %d\n", tdq->tdq_idx);
	printf("\ttimeshare ridx: %d\n", tdq->tdq_ridx);
	printf("\trealtime runq:\n");
	runq_print(&tdq->tdq_realtime);
	printf("\ttimeshare runq:\n");
	runq_print(&tdq->tdq_timeshare);
	printf("\tidle runq:\n");
	runq_print(&tdq->tdq_idle);
#ifdef SMP
	printf("\tload transferable: %d\n", tdq->tdq_transferable);
#endif
}

static __inline void
tdq_runq_add(struct tdq *tdq, struct td_sched *ts, int flags)
{
#ifdef SMP
	if (THREAD_CAN_MIGRATE(ts->ts_thread)) {
		tdq->tdq_transferable++;
		tdq->tdq_group->tdg_transferable++;
		ts->ts_flags |= TSF_XFERABLE;
		if (tdq->tdq_transferable >= busy_thresh &&
		    (tdq->tdq_flags & TDQF_BUSY) == 0) {
			tdq->tdq_flags |= TDQF_BUSY;
			atomic_set_int(&tdq_busy, 1 << TDQ_ID(tdq));
		}
	}
#endif
	if (ts->ts_runq == &tdq->tdq_timeshare) {
		u_char pri;

		pri = ts->ts_thread->td_priority;
		KASSERT(pri <= PRI_MAX_TIMESHARE && pri >= PRI_MIN_TIMESHARE,
			("Invalid priority %d on timeshare runq", pri));
		/*
		 * This queue contains only priorities between MIN and MAX
		 * realtime.  Use the whole queue to represent these values.
		 */
#define	TS_RQ_PPQ	(((PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE) + 1) / RQ_NQS)
		if ((flags & SRQ_BORROWING) == 0) {
			pri = (pri - PRI_MIN_TIMESHARE) / TS_RQ_PPQ;
			pri = (pri + tdq->tdq_idx) % RQ_NQS;
			/*
			 * This effectively shortens the queue by one so we
			 * can have a one slot difference between idx and
			 * ridx while we wait for threads to drain.
			 */
			if (tdq->tdq_ridx != tdq->tdq_idx &&
			    pri == tdq->tdq_ridx)
				pri = (unsigned char)(pri - 1) % RQ_NQS;
		} else
			pri = tdq->tdq_ridx;
		runq_add_pri(ts->ts_runq, ts, pri, flags);
	} else
		runq_add(ts->ts_runq, ts, flags);
}

static __inline void
tdq_runq_rem(struct tdq *tdq, struct td_sched *ts)
{
#ifdef SMP
	if (ts->ts_flags & TSF_XFERABLE) {
		tdq->tdq_transferable--;
		tdq->tdq_group->tdg_transferable--;
		ts->ts_flags &= ~TSF_XFERABLE;
		if (tdq->tdq_transferable < busy_thresh && 
		    (tdq->tdq_flags & TDQF_BUSY)) {
			atomic_clear_int(&tdq_busy, 1 << TDQ_ID(tdq));
			tdq->tdq_flags &= ~TDQF_BUSY;
		}
	}
#endif
	if (ts->ts_runq == &tdq->tdq_timeshare) {
		if (tdq->tdq_idx != tdq->tdq_ridx)
			runq_remove_idx(ts->ts_runq, ts, &tdq->tdq_ridx);
		else
			runq_remove_idx(ts->ts_runq, ts, NULL);
		/*
		 * For timeshare threads we update the priority here so
		 * the priority reflects the time we've been sleeping.
		 */
		ts->ts_ltick = ticks;
		sched_pctcpu_update(ts);
		sched_priority(ts->ts_thread);
	} else
		runq_remove(ts->ts_runq, ts);
}

static void
tdq_load_add(struct tdq *tdq, struct td_sched *ts)
{
	int class;
	mtx_assert(&sched_lock, MA_OWNED);
	class = PRI_BASE(ts->ts_thread->td_pri_class);
	tdq->tdq_load++;
	CTR2(KTR_SCHED, "cpu %jd load: %d", TDQ_ID(tdq), tdq->tdq_load);
	if (class != PRI_ITHD &&
	    (ts->ts_thread->td_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		tdq->tdq_group->tdg_load++;
#else
		tdq->tdq_sysload++;
#endif
}

static void
tdq_load_rem(struct tdq *tdq, struct td_sched *ts)
{
	int class;
	mtx_assert(&sched_lock, MA_OWNED);
	class = PRI_BASE(ts->ts_thread->td_pri_class);
	if (class != PRI_ITHD &&
	    (ts->ts_thread->td_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		tdq->tdq_group->tdg_load--;
#else
		tdq->tdq_sysload--;
#endif
	tdq->tdq_load--;
	CTR1(KTR_SCHED, "load: %d", tdq->tdq_load);
	ts->ts_runq = NULL;
}

#ifdef SMP
static void
sched_smp_tick(struct thread *td)
{
	struct tdq *tdq;

	tdq = TDQ_SELF();
	if (rebalance) {
		if (ticks >= bal_tick)
			sched_balance();
		if (ticks >= gbal_tick && balance_groups)
			sched_balance_groups();
	}
	td->td_sched->ts_rltick = ticks;
}

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
	struct tdq_group *tdg;
	int cnt;
	int i;

	bal_tick = ticks + (random() % (hz * 2));
	if (smp_started == 0)
		return;
	low = high = NULL;
	i = random() % (tdg_maxid + 1);
	for (cnt = 0; cnt <= tdg_maxid; cnt++) {
		tdg = TDQ_GROUP(i);
		/*
		 * Find the CPU with the highest load that has some
		 * threads to transfer.
		 */
		if ((high == NULL || tdg->tdg_load > high->tdg_load)
		    && tdg->tdg_transferable)
			high = tdg;
		if (low == NULL || tdg->tdg_load < low->tdg_load)
			low = tdg;
		if (++i > tdg_maxid)
			i = 0;
	}
	if (low != NULL && high != NULL && high != low)
		sched_balance_pair(LIST_FIRST(&high->tdg_members),
		    LIST_FIRST(&low->tdg_members));
}

static void
sched_balance_groups(void)
{
	int i;

	gbal_tick = ticks + (random() % (hz * 2));
	mtx_assert(&sched_lock, MA_OWNED);
	if (smp_started)
		for (i = 0; i <= tdg_maxid; i++)
			sched_balance_group(TDQ_GROUP(i));
}

static void
sched_balance_group(struct tdq_group *tdg)
{
	struct tdq *tdq;
	struct tdq *high;
	struct tdq *low;
	int load;

	if (tdg->tdg_transferable == 0)
		return;
	low = NULL;
	high = NULL;
	LIST_FOREACH(tdq, &tdg->tdg_members, tdq_siblings) {
		load = tdq->tdq_load;
		if (high == NULL || load > high->tdq_load)
			high = tdq;
		if (low == NULL || load < low->tdq_load)
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
	if (high->tdq_group == low->tdq_group) {
		transferable = high->tdq_transferable;
		high_load = high->tdq_load;
		low_load = low->tdq_load;
	} else {
		transferable = high->tdq_group->tdg_transferable;
		high_load = high->tdq_group->tdg_load;
		low_load = low->tdq_group->tdg_load;
	}
	if (transferable == 0)
		return;
	/*
	 * Determine what the imbalance is and then adjust that to how many
	 * threads we actually have to give up (transferable).
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
		struct tdq_group *tdg;

		tdg = tdq->tdq_group;
		LIST_FOREACH(tdq, &tdg->tdg_members, tdq_siblings) {
			if (tdq == from || tdq->tdq_transferable == 0)
				continue;
			ts = tdq_steal(tdq, 1);
			break;
		}
		if (ts == NULL)
			panic("tdq_move: No threads available with a "
			    "transferable count of %d\n", 
			    tdg->tdg_transferable);
	}
	if (tdq == to)
		return;
	sched_rem(ts->ts_thread);
	ts->ts_cpu = cpu;
	sched_pin_td(ts->ts_thread);
	sched_add(ts->ts_thread, SRQ_YIELDING);
	sched_unpin_td(ts->ts_thread);
}

static int
tdq_idled(struct tdq *tdq)
{
	struct tdq_group *tdg;
	struct tdq *steal;
	struct td_sched *ts;

	tdg = tdq->tdq_group;
	/*
	 * If we're in a cpu group, try and steal threads from another cpu in
	 * the group before idling.
	 */
	if (steal_htt && tdg->tdg_cpus > 1 && tdg->tdg_transferable) {
		LIST_FOREACH(steal, &tdg->tdg_members, tdq_siblings) {
			if (steal == tdq || steal->tdq_transferable == 0)
				continue;
			ts = tdq_steal(steal, 0);
			if (ts)
				goto steal;
		}
	}
	if (steal_busy) {
		while (tdq_busy) {
			int cpu;

			cpu = ffs(tdq_busy);
			if (cpu == 0)
				break;
			cpu--;
			steal = TDQ_CPU(cpu);
			if (steal->tdq_transferable == 0)
				continue;
			ts = tdq_steal(steal, 1);
			if (ts == NULL)
				continue;
			CTR5(KTR_ULE,
			    "tdq_idled: stealing td %p(%s) pri %d from %d busy 0x%X",
			    ts->ts_thread, ts->ts_thread->td_proc->p_comm,
			    ts->ts_thread->td_priority, cpu, tdq_busy);
			goto steal;
		}
	}
	/*
	 * We only set the idled bit when all of the cpus in the group are
	 * idle.  Otherwise we could get into a situation where a thread bounces
	 * back and forth between two idle cores on seperate physical CPUs.
	 */
	tdg->tdg_idlemask |= PCPU_GET(cpumask);
	if (tdg->tdg_idlemask == tdg->tdg_cpumask)
		atomic_set_int(&tdq_idle, tdg->tdg_mask);
	return (1);
steal:
	sched_rem(ts->ts_thread);
	ts->ts_cpu = PCPU_GET(cpuid);
	sched_pin_td(ts->ts_thread);
	sched_add(ts->ts_thread, SRQ_YIELDING);
	sched_unpin_td(ts->ts_thread);

	return (0);
}

static void
tdq_notify(struct td_sched *ts)
{
	struct thread *ctd;
	struct pcpu *pcpu;
	int cpri;
	int pri;
	int cpu;

	cpu = ts->ts_cpu;
	pri = ts->ts_thread->td_priority;
	pcpu = pcpu_find(cpu);
	ctd = pcpu->pc_curthread;
	cpri = ctd->td_priority;

	/*
	 * If our priority is not better than the current priority there is
	 * nothing to do.
	 */
	if (pri > cpri)
		return;
	/*
	 * Always IPI idle.
	 */
	if (cpri > PRI_MIN_IDLE)
		goto sendipi;
	/*
	 * If we're realtime or better and there is timeshare or worse running
	 * send an IPI.
	 */
	if (pri < PRI_MAX_REALTIME && cpri > PRI_MAX_REALTIME)
		goto sendipi;
	/*
	 * Otherwise only IPI if we exceed the threshold.
	 */
	if (pri > ipi_thresh)
		return;
sendipi:
	ctd->td_flags |= TDF_NEEDRESCHED;
	if (cpri < PRI_MIN_IDLE) {
		if (ipi_ast)
			ipi_selected(1 << cpu, IPI_AST);
		else if (ipi_preempt)
			ipi_selected(1 << cpu, IPI_PREEMPT);
	} else 
		ipi_selected(1 << cpu, IPI_PREEMPT);
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
				if (THREAD_CAN_MIGRATE(ts->ts_thread))
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
	 * XXX Need to effect steal order for timeshare threads.
	 */
	if ((ts = runq_steal(&tdq->tdq_realtime)) != NULL)
		return (ts);
	if ((ts = runq_steal(&tdq->tdq_timeshare)) != NULL)
		return (ts);
	if (stealidle)
		return (runq_steal(&tdq->tdq_idle));
	return (NULL);
}

int
tdq_pickidle(struct tdq *tdq, struct td_sched *ts)
{
	struct tdq_group *tdg;
	int self;
	int cpu;

	self = PCPU_GET(cpuid);
	if (smp_started == 0)
		return (self);
	/*
	 * If the current CPU has idled, just run it here.
	 */
	if ((tdq->tdq_group->tdg_idlemask & PCPU_GET(cpumask)) != 0)
		return (self);
	/*
	 * Try the last group we ran on.
	 */
	tdg = TDQ_CPU(ts->ts_cpu)->tdq_group;
	cpu = ffs(tdg->tdg_idlemask);
	if (cpu)
		return (cpu - 1);
	/*
	 * Search for an idle group.
	 */
	cpu = ffs(tdq_idle);
	if (cpu) 
		return (cpu - 1);
	/*
	 * XXX If there are no idle groups, check for an idle core.
	 */
	/*
	 * No idle CPUs?
	 */
	return (self);
}

static int
tdq_pickpri(struct tdq *tdq, struct td_sched *ts, int flags)
{
	struct pcpu *pcpu;
	int lowpri;
	int lowcpu;
	int lowload;
	int load;
	int self;
	int pri;
	int cpu;

	self = PCPU_GET(cpuid);
	if (smp_started == 0)
		return (self);

	pri = ts->ts_thread->td_priority;
	/*
	 * Regardless of affinity, if the last cpu is idle send it there.
	 */
	pcpu = pcpu_find(ts->ts_cpu);
	if (pcpu->pc_curthread->td_priority > PRI_MIN_IDLE) {
		CTR5(KTR_ULE,
		    "ts_cpu %d idle, ltick %d ticks %d pri %d curthread %d",
		    ts->ts_cpu, ts->ts_rltick, ticks, pri,
		    pcpu->pc_curthread->td_priority);
		return (ts->ts_cpu);
	}
	/*
	 * If we have affinity, try to place it on the cpu we last ran on.
	 */
	if (SCHED_AFFINITY(ts) && pcpu->pc_curthread->td_priority > pri) {
		CTR5(KTR_ULE,
		    "affinity for %d, ltick %d ticks %d pri %d curthread %d",
		    ts->ts_cpu, ts->ts_rltick, ticks, pri,
		    pcpu->pc_curthread->td_priority);
		return (ts->ts_cpu);
	}
	/*
	 * Try ourself first; If we're running something lower priority this
	 * may have some locality with the waking thread and execute faster
	 * here.
	 */
	if (tryself) {
		/*
		 * If we're being awoken by an interrupt thread or the waker
		 * is going right to sleep run here as well.
		 */
		if ((TDQ_SELF()->tdq_load == 1) && (flags & SRQ_YIELDING ||
		    curthread->td_pri_class == PRI_ITHD)) {
			CTR2(KTR_ULE, "tryself load %d flags %d",
			    TDQ_SELF()->tdq_load, flags);
			return (self);
		}
	}
	/*
	 * Look for an idle group.
	 */
	CTR1(KTR_ULE, "tdq_idle %X", tdq_idle);
	cpu = ffs(tdq_idle);
	if (cpu)
		return (cpu - 1);
	if (tryselfidle && pri < curthread->td_priority) {
		CTR1(KTR_ULE, "tryself %d",
		    curthread->td_priority);
		return (self);
	}
	/*
 	 * Now search for the cpu running the lowest priority thread with
	 * the least load.
	 */
	lowload = 0;
	lowpri = lowcpu = 0;
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		pcpu = pcpu_find(cpu);
		pri = pcpu->pc_curthread->td_priority;
		CTR4(KTR_ULE,
		    "cpu %d pri %d lowcpu %d lowpri %d",
		    cpu, pri, lowcpu, lowpri);
		if (pri < lowpri)
			continue;
		load = TDQ_CPU(cpu)->tdq_load;
		if (lowpri && lowpri == pri && load > lowload)
			continue;
		lowpri = pri;
		lowcpu = cpu;
		lowload = load;
	}

	return (lowcpu);
}

#endif	/* SMP */

/*
 * Pick the highest priority task we have and return it.
 */

static struct td_sched *
tdq_choose(struct tdq *tdq)
{
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);

	ts = runq_choose(&tdq->tdq_realtime);
	if (ts != NULL) {
		KASSERT(ts->ts_thread->td_priority <= PRI_MAX_REALTIME,
		    ("tdq_choose: Invalid priority on realtime queue %d",
		    ts->ts_thread->td_priority));
		return (ts);
	}
	ts = runq_choose_from(&tdq->tdq_timeshare, tdq->tdq_ridx);
	if (ts != NULL) {
		KASSERT(ts->ts_thread->td_priority <= PRI_MAX_TIMESHARE &&
		    ts->ts_thread->td_priority >= PRI_MIN_TIMESHARE,
		    ("tdq_choose: Invalid priority on timeshare queue %d",
		    ts->ts_thread->td_priority));
		return (ts);
	}

	ts = runq_choose(&tdq->tdq_idle);
	if (ts != NULL) {
		KASSERT(ts->ts_thread->td_priority >= PRI_MIN_IDLE,
		    ("tdq_choose: Invalid priority on idle queue %d",
		    ts->ts_thread->td_priority));
		return (ts);
	}

	return (NULL);
}

static void
tdq_setup(struct tdq *tdq)
{
	runq_init(&tdq->tdq_realtime);
	runq_init(&tdq->tdq_timeshare);
	runq_init(&tdq->tdq_idle);
	tdq->tdq_load = 0;
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
	sched_slice = (realstathz/10);	/* ~100ms */
	tickincr = 1 << SCHED_TICK_SHIFT;

#ifdef SMP
	balance_groups = 0;
	/*
	 * Initialize the tdqs.
	 */
	for (i = 0; i < MAXCPU; i++) {
		struct tdq *tdq;

		tdq = &tdq_cpu[i];
		tdq_setup(&tdq_cpu[i]);
	}
	if (smp_topology == NULL) {
		struct tdq_group *tdg;
		struct tdq *tdq;
		int cpus;

		for (cpus = 0, i = 0; i < MAXCPU; i++) {
			if (CPU_ABSENT(i))
				continue;
			tdq = &tdq_cpu[i];
			tdg = &tdq_groups[cpus];
			/*
			 * Setup a tdq group with one member.
			 */
			tdq->tdq_transferable = 0;
			tdq->tdq_group = tdg;
			tdg->tdg_cpus = 1;
			tdg->tdg_idlemask = 0;
			tdg->tdg_cpumask = tdg->tdg_mask = 1 << i;
			tdg->tdg_load = 0;
			tdg->tdg_transferable = 0;
			LIST_INIT(&tdg->tdg_members);
			LIST_INSERT_HEAD(&tdg->tdg_members, tdq, tdq_siblings);
			cpus++;
		}
		tdg_maxid = cpus - 1;
	} else {
		struct tdq_group *tdg;
		struct cpu_group *cg;
		int j;

		topology = 1;
		for (i = 0; i < smp_topology->ct_count; i++) {
			cg = &smp_topology->ct_group[i];
			tdg = &tdq_groups[i];
			/*
			 * Initialize the group.
			 */
			tdg->tdg_idlemask = 0;
			tdg->tdg_load = 0;
			tdg->tdg_transferable = 0;
			tdg->tdg_cpus = cg->cg_count;
			tdg->tdg_cpumask = cg->cg_mask;
			LIST_INIT(&tdg->tdg_members);
			/*
			 * Find all of the group members and add them.
			 */
			for (j = 0; j < MAXCPU; j++) {
				if ((cg->cg_mask & (1 << j)) != 0) {
					if (tdg->tdg_mask == 0)
						tdg->tdg_mask = 1 << j;
					tdq_cpu[j].tdq_transferable = 0;
					tdq_cpu[j].tdq_group = tdg;
					LIST_INSERT_HEAD(&tdg->tdg_members,
					    &tdq_cpu[j], tdq_siblings);
				}
			}
			if (tdg->tdg_cpus > 1)
				balance_groups = 1;
		}
		tdg_maxid = smp_topology->ct_count - 1;
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
	sched_slice = (realstathz/10);	/* ~100ms */

	/*
	 * tickincr is shifted out by 10 to avoid rounding errors due to
	 * hz not being evenly divisible by stathz on all platforms.
	 */
	tickincr = (hz << SCHED_TICK_SHIFT) / realstathz;
	/*
	 * This does not work for values of stathz that are more than
	 * 1 << SCHED_TICK_SHIFT * hz.  In practice this does not happen.
	 */
	if (tickincr == 0)
		tickincr = 1;
#ifdef SMP
	affinity = SCHED_AFFINITY_DEFAULT;
#endif
	mtx_unlock_spin(&sched_lock);
}


/*
 * Scale the scheduling priority according to the "interactivity" of this
 * process.
 */
static void
sched_priority(struct thread *td)
{
	int score;
	int pri;

	if (td->td_pri_class != PRI_TIMESHARE)
		return;
	/*
	 * If the score is interactive we place the thread in the realtime
	 * queue with a priority that is less than kernel and interrupt
	 * priorities.  These threads are not subject to nice restrictions.
	 *
	 * Scores greater than this are placed on the normal realtime queue
	 * where the priority is partially decided by the most recent cpu
	 * utilization and the rest is decided by nice value.
	 */
	score = sched_interact_score(td);
	if (score < sched_interact) {
		pri = PRI_MIN_REALTIME;
		pri += ((PRI_MAX_REALTIME - PRI_MIN_REALTIME) / sched_interact)
		    * score;
		KASSERT(pri >= PRI_MIN_REALTIME && pri <= PRI_MAX_REALTIME,
		    ("sched_priority: invalid interactive priority %d score %d",
		    pri, score));
	} else {
		pri = SCHED_PRI_MIN;
		if (td->td_sched->ts_ticks)
			pri += SCHED_PRI_TICKS(td->td_sched);
		pri += SCHED_PRI_NICE(td->td_proc->p_nice);
		if (!(pri >= PRI_MIN_TIMESHARE && pri <= PRI_MAX_TIMESHARE)) {
			static int once = 1;
			if (once) {
				printf("sched_priority: invalid priority %d",
				    pri);
				printf("nice %d, ticks %d ftick %d ltick %d tick pri %d\n",
				    td->td_proc->p_nice,
				    td->td_sched->ts_ticks,
				    td->td_sched->ts_ftick,
				    td->td_sched->ts_ltick,
				    SCHED_PRI_TICKS(td->td_sched));
				once = 0;
			}
			pri = min(max(pri, PRI_MIN_TIMESHARE),
			    PRI_MAX_TIMESHARE);
		}
	}
	sched_user_prio(td, pri);

	return;
}

/*
 * This routine enforces a maximum limit on the amount of scheduling history
 * kept.  It is called after either the slptime or runtime is adjusted.
 */
static void
sched_interact_update(struct thread *td)
{
	struct td_sched *ts;
	u_int sum;

	ts = td->td_sched;
	sum = ts->skg_runtime + ts->skg_slptime;
	if (sum < SCHED_SLP_RUN_MAX)
		return;
	/*
	 * This only happens from two places:
	 * 1) We have added an unusual amount of run time from fork_exit.
	 * 2) We have added an unusual amount of sleep time from sched_sleep().
	 */
	if (sum > SCHED_SLP_RUN_MAX * 2) {
		if (ts->skg_runtime > ts->skg_slptime) {
			ts->skg_runtime = SCHED_SLP_RUN_MAX;
			ts->skg_slptime = 1;
		} else {
			ts->skg_slptime = SCHED_SLP_RUN_MAX;
			ts->skg_runtime = 1;
		}
		return;
	}
	/*
	 * If we have exceeded by more than 1/5th then the algorithm below
	 * will not bring us back into range.  Dividing by two here forces
	 * us into the range of [4/5 * SCHED_INTERACT_MAX, SCHED_INTERACT_MAX]
	 */
	if (sum > (SCHED_SLP_RUN_MAX / 5) * 6) {
		ts->skg_runtime /= 2;
		ts->skg_slptime /= 2;
		return;
	}
	ts->skg_runtime = (ts->skg_runtime / 5) * 4;
	ts->skg_slptime = (ts->skg_slptime / 5) * 4;
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
	}
	if (td->td_sched->skg_slptime > td->td_sched->skg_runtime) {
		div = max(1, td->td_sched->skg_slptime / SCHED_INTERACT_HALF);
		return (td->td_sched->skg_runtime / div);
	}
	/* runtime == slptime */
	if (td->td_sched->skg_runtime)
		return (SCHED_INTERACT_HALF);

	/*
	 * This can happen if slptime and runtime are 0.
	 */
	return (0);

}

/*
 * Called from proc0_init() to bootstrap the scheduler.
 */
void
schedinit(void)
{

	/*
	 * Set up the scheduler specific parts of proc0.
	 */
	proc0.p_sched = NULL; /* XXX */
	thread0.td_sched = &td_sched0;
	thread0.td_lock = &sched_lock;
	td_sched0.ts_ltick = ticks;
	td_sched0.ts_ftick = ticks;
	td_sched0.ts_thread = &thread0;
}

/*
 * This is only somewhat accurate since given many processes of the same
 * priority they will switch when their slices run out, which will be
 * at most sched_slice stathz ticks.
 */
int
sched_rr_interval(void)
{

	/* Convert sched_slice to hz */
	return (hz/(realstathz/sched_slice));
}

static void
sched_pctcpu_update(struct td_sched *ts)
{

	if (ts->ts_ticks == 0)
		return;
	if (ticks - (hz / 10) < ts->ts_ltick &&
	    SCHED_TICK_TOTAL(ts) < SCHED_TICK_MAX)
		return;
	/*
	 * Adjust counters and watermark for pctcpu calc.
	 */
	if (ts->ts_ltick > ticks - SCHED_TICK_TARG)
		ts->ts_ticks = (ts->ts_ticks / (ticks - ts->ts_ftick)) *
			    SCHED_TICK_TARG;
	else
		ts->ts_ticks = 0;
	ts->ts_ltick = ticks;
	ts->ts_ftick = ts->ts_ltick - SCHED_TICK_TARG;
}

static void
sched_thread_priority(struct thread *td, u_char prio)
{
	struct td_sched *ts;

	CTR6(KTR_SCHED, "sched_prio: %p(%s) prio %d newprio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, prio, curthread,
	    curthread->td_proc->p_comm);
	ts = td->td_sched;
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	if (td->td_priority == prio)
		return;

	if (TD_ON_RUNQ(td) && prio < td->td_priority) {
		/*
		 * If the priority has been elevated due to priority
		 * propagation, we may have to move ourselves to a new
		 * queue.  This could be optimized to not re-add in some
		 * cases.
		 */
		MPASS(td->td_lock == &sched_lock);
		sched_rem(td);
		td->td_priority = prio;
		sched_add(td, SRQ_BORROWING|SRQ_OURSELF);
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
	struct tdq *tdq;
	struct td_sched *ts;
	int preempt;

	THREAD_LOCK_ASSERT(td, MA_OWNED);

	preempt = flags & SW_PREEMPT;
	tdq = TDQ_SELF();
	ts = td->td_sched;
	td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;
	/*
	 * If the thread has been assigned it may be in the process of switching
	 * to the new cpu.  This is the case in sched_bind().
	 */
	/*
	 * Switch to the sched lock to fix things up and pick
	 * a new thread.
	 */
	if (td->td_lock != &sched_lock) {
		mtx_lock_spin(&sched_lock);
		thread_unlock(td);
	}
	if (TD_IS_IDLETHREAD(td)) {
		MPASS(td->td_lock == &sched_lock);
		TD_SET_CAN_RUN(td);
	} else if (TD_IS_RUNNING(td)) {
		/*
		 * Don't allow the thread to migrate
		 * from a preemption.
		 */
		tdq_load_rem(tdq, ts);
		if (preempt)
			sched_pin_td(td);
		sched_add(td, preempt ?
		    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
		    SRQ_OURSELF|SRQ_YIELDING);
		if (preempt)
			sched_unpin_td(td);
	} else
		tdq_load_rem(tdq, ts);
	mtx_assert(&sched_lock, MA_OWNED);
	if (newtd != NULL) {
		/*
		 * If we bring in a thread account for it as if it had been
		 * added to the run queue and then chosen.
		 */
		TD_SET_RUNNING(newtd);
		tdq_load_add(TDQ_SELF(), newtd->td_sched);
	} else
		newtd = choosethread();
	if (td != newtd) {
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_OUT);
#endif

		cpu_switch(td, newtd, td->td_lock);
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_IN);
#endif
	}
	sched_lock.mtx_lock = (uintptr_t)td;
	td->td_oncpu = PCPU_GET(cpuid);
	MPASS(td->td_lock == &sched_lock);
}

void
sched_nice(struct proc *p, int nice)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);

	p->p_nice = nice;
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		sched_priority(td);
		sched_prio(td, td->td_base_user_pri);
		thread_unlock(td);
	}
}

void
sched_sleep(struct thread *td)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);

	td->td_sched->ts_slptime = ticks;
}

void
sched_wakeup(struct thread *td)
{
	struct td_sched *ts;
	int slptime;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;
	/*
	 * If we slept for more than a tick update our interactivity and
	 * priority.
	 */
	slptime = ts->ts_slptime;
	ts->ts_slptime = 0;
	if (slptime && slptime != ticks) {
		u_int hzticks;

		hzticks = (ticks - slptime) << SCHED_TICK_SHIFT;
		ts->skg_slptime += hzticks;
		sched_interact_update(td);
		sched_pctcpu_update(ts);
		sched_priority(td);
	}
	/* Reset the slice value after we sleep. */
	ts->ts_slice = sched_slice;
	sched_add(td, SRQ_BORING);
}

/*
 * Penalize the parent for creating a new child and initialize the child's
 * priority.
 */
void
sched_fork(struct thread *td, struct thread *child)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	sched_fork_thread(td, child);
	/*
	 * Penalize the parent and child for forking.
	 */
	sched_interact_fork(child);
	sched_priority(child);
	td->td_sched->skg_runtime += tickincr;
	sched_interact_update(td);
	sched_priority(td);
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
	struct td_sched *ts;
	struct td_sched *ts2;

	/*
	 * Initialize child.
	 */
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	sched_newthread(child);
	child->td_lock = &sched_lock;
	ts = td->td_sched;
	ts2 = child->td_sched;
	ts2->ts_cpu = ts->ts_cpu;
	ts2->ts_runq = NULL;
	/*
	 * Grab our parents cpu estimation information and priority.
	 */
	ts2->ts_ticks = ts->ts_ticks;
	ts2->ts_ltick = ts->ts_ltick;
	ts2->ts_ftick = ts->ts_ftick;
	child->td_user_pri = td->td_user_pri;
	child->td_base_user_pri = td->td_base_user_pri;
	/*
	 * And update interactivity score.
	 */
	ts2->skg_slptime = ts->skg_slptime;
	ts2->skg_runtime = ts->skg_runtime;
	ts2->ts_slice = 1;	/* Attempt to quickly learn interactivity. */
}

void
sched_class(struct thread *td, int class)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	if (td->td_pri_class == class)
		return;

#ifdef SMP
	/*
	 * On SMP if we're on the RUNQ we must adjust the transferable
	 * count because could be changing to or from an interrupt
	 * class.
	 */
	if (TD_ON_RUNQ(td)) {
		struct tdq *tdq;

		tdq = TDQ_CPU(td->td_sched->ts_cpu);
		if (THREAD_CAN_MIGRATE(td)) {
			tdq->tdq_transferable--;
			tdq->tdq_group->tdg_transferable--;
		}
		td->td_pri_class = class;
		if (THREAD_CAN_MIGRATE(td)) {
			tdq->tdq_transferable++;
			tdq->tdq_group->tdg_transferable++;
		}
	}
#endif
	td->td_pri_class = class;
}

/*
 * Return some of the child's priority and interactivity to the parent.
 */
void
sched_exit(struct proc *p, struct thread *child)
{
	struct thread *td;
	
	CTR3(KTR_SCHED, "sched_exit: %p(%s) prio %d",
	    child, child->td_proc->p_comm, child->td_priority);

	PROC_SLOCK_ASSERT(p, MA_OWNED);
	td = FIRST_THREAD_IN_PROC(p);
	sched_exit_thread(td, child);
}

void
sched_exit_thread(struct thread *td, struct thread *child)
{

	CTR3(KTR_SCHED, "sched_exit_thread: %p(%s) prio %d",
	    child, child->td_proc->p_comm, child->td_priority);

	thread_lock(child);
	tdq_load_rem(TDQ_CPU(child->td_sched->ts_cpu), child->td_sched);
	thread_unlock(child);
#ifdef KSE
	/*
	 * KSE forks and exits so often that this penalty causes short-lived
	 * threads to always be non-interactive.  This causes mozilla to
	 * crawl under load.
	 */
	if ((td->td_pflags & TDP_SA) && td->td_proc == child->td_proc)
		return;
#endif
	/*
	 * Give the child's runtime to the parent without returning the
	 * sleep time as a penalty to the parent.  This causes shells that
	 * launch expensive things to mark their children as expensive.
	 */
	thread_lock(td);
	td->td_sched->skg_runtime += child->td_sched->skg_runtime;
	sched_interact_update(td);
	sched_priority(td);
	thread_unlock(td);
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
		thread_lock(td);
		td->td_priority = td->td_user_pri;
		td->td_base_pri = td->td_user_pri;
		thread_unlock(td);
        }
}

void
sched_clock(struct thread *td)
{
	struct tdq *tdq;
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
#ifdef SMP
	sched_smp_tick(td);
#endif
	tdq = TDQ_SELF();
	/*
	 * Advance the insert index once for each tick to ensure that all
	 * threads get a chance to run.
	 */
	if (tdq->tdq_idx == tdq->tdq_ridx) {
		tdq->tdq_idx = (tdq->tdq_idx + 1) % RQ_NQS;
		if (TAILQ_EMPTY(&tdq->tdq_timeshare.rq_queues[tdq->tdq_ridx]))
			tdq->tdq_ridx = tdq->tdq_idx;
	}
	ts = td->td_sched;
	/*
	 * We only do slicing code for TIMESHARE threads.
	 */
	if (td->td_pri_class != PRI_TIMESHARE)
		return;
	/*
	 * We used a tick; charge it to the thread so that we can compute our
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
	sched_priority(td);
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
	if (tdq_busy)
		goto out;
#endif
	if ((curthread->td_flags & TDF_IDLETD) != 0) {
		if (tdq->tdq_load > 0)
			goto out;
	} else
		if (tdq->tdq_load - 1 > 0)
			goto out;
	load = 0;
out:
	return (load);
}

struct thread *
sched_choose(void)
{
	struct tdq *tdq;
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	tdq = TDQ_SELF();
#ifdef SMP
restart:
#endif
	ts = tdq_choose(tdq);
	if (ts) {
#ifdef SMP
		if (ts->ts_thread->td_priority > PRI_MIN_IDLE)
			if (tdq_idled(tdq) == 0)
				goto restart;
#endif
		tdq_runq_rem(tdq, ts);
		return (ts->ts_thread);
	}
#ifdef SMP
	if (tdq_idled(tdq) == 0)
		goto restart;
#endif
	return (PCPU_GET(idlethread));
}

static int
sched_preempt(struct thread *td)
{
	struct thread *ctd;
	int cpri;
	int pri;

	ctd = curthread;
	pri = td->td_priority;
	cpri = ctd->td_priority;
	if (panicstr != NULL || pri >= cpri || cold || TD_IS_INHIBITED(ctd))
		return (0);
	/*
	 * Always preempt IDLE threads.  Otherwise only if the preempting
	 * thread is an ithread.
	 */
	if (pri > PRI_MAX_ITHD && cpri < PRI_MIN_IDLE)
		return (0);
	if (ctd->td_critnest > 1) {
		CTR1(KTR_PROC, "sched_preempt: in critical section %d",
		    ctd->td_critnest);
		ctd->td_owepreempt = 1;
		return (0);
	}
	/*
	 * Thread is runnable but not yet put on system run queue.
	 */
	MPASS(TD_ON_RUNQ(td));
	TD_SET_RUNNING(td);
	MPASS(ctd->td_lock == &sched_lock);
	MPASS(td->td_lock == &sched_lock);
	CTR3(KTR_PROC, "preempting to thread %p (pid %d, %s)\n", td,
	    td->td_proc->p_pid, td->td_proc->p_comm);
	/*
	 * We enter the switch with two runnable threads that both have
	 * the same lock.  When we return td may be sleeping so we need
	 * to switch locks to make sure he's locked correctly.
	 */
	SCHED_STAT_INC(switch_preempt);
	mi_switch(SW_INVOL|SW_PREEMPT, td);
	spinlock_enter();
	thread_unlock(ctd);
	thread_lock(td);
	spinlock_exit();

	return (1);
}

void
sched_add(struct thread *td, int flags)
{
	struct tdq *tdq;
	struct td_sched *ts;
	int preemptive;
	int class;
#ifdef SMP
	int cpuid;
	int cpumask;
#endif
	ts = td->td_sched;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	KASSERT((td->td_inhibitors == 0),
	    ("sched_add: trying to run inhibited thread"));
	KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	    ("sched_add: bad thread state"));
	KASSERT(td->td_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	/*
	 * Now that the thread is moving to the run-queue, set the lock
	 * to the scheduler's lock.
	 */
	if (td->td_lock != &sched_lock) {
		mtx_lock_spin(&sched_lock);
		thread_lock_set(td, &sched_lock);
	}
	mtx_assert(&sched_lock, MA_OWNED);
        TD_SET_RUNQ(td);
	tdq = TDQ_SELF();
	class = PRI_BASE(td->td_pri_class);
	preemptive = !(flags & SRQ_YIELDING);
	/*
	 * Recalculate the priority before we select the target cpu or
	 * run-queue.
	 */
	if (class == PRI_TIMESHARE)
		sched_priority(td);
	if (ts->ts_slice == 0)
		ts->ts_slice = sched_slice;
#ifdef SMP
	cpuid = PCPU_GET(cpuid);
	/*
	 * Pick the destination cpu and if it isn't ours transfer to the
	 * target cpu.
	 */
	if (THREAD_CAN_MIGRATE(td)) {
		if (td->td_priority <= PRI_MAX_ITHD) {
			CTR2(KTR_ULE, "ithd %d < %d",
			    td->td_priority, PRI_MAX_ITHD);
			ts->ts_cpu = cpuid;
		} else if (pick_pri)
			ts->ts_cpu = tdq_pickpri(tdq, ts, flags);
		else
			ts->ts_cpu = tdq_pickidle(tdq, ts);
	} else
		CTR1(KTR_ULE, "pinned %d", td->td_pinned);
	if (ts->ts_cpu != cpuid)
		preemptive = 0;
	tdq = TDQ_CPU(ts->ts_cpu);
	cpumask = 1 << ts->ts_cpu;
	/*
	 * If we had been idle, clear our bit in the group and potentially
	 * the global bitmap.
	 */
	if ((class != PRI_IDLE && class != PRI_ITHD) &&
	    (tdq->tdq_group->tdg_idlemask & cpumask) != 0) {
		/*
		 * Check to see if our group is unidling, and if so, remove it
		 * from the global idle mask.
		 */
		if (tdq->tdq_group->tdg_idlemask ==
		    tdq->tdq_group->tdg_cpumask)
			atomic_clear_int(&tdq_idle, tdq->tdq_group->tdg_mask);
		/*
		 * Now remove ourselves from the group specific idle mask.
		 */
		tdq->tdq_group->tdg_idlemask &= ~cpumask;
	}
#endif
	/*
	 * Pick the run queue based on priority.
	 */
	if (td->td_priority <= PRI_MAX_REALTIME)
		ts->ts_runq = &tdq->tdq_realtime;
	else if (td->td_priority <= PRI_MAX_TIMESHARE)
		ts->ts_runq = &tdq->tdq_timeshare;
	else
		ts->ts_runq = &tdq->tdq_idle;
	if (preemptive && sched_preempt(td))
		return;
	tdq_runq_add(tdq, ts, flags);
	tdq_load_add(tdq, ts);
#ifdef SMP
	if (ts->ts_cpu != cpuid) {
		tdq_notify(ts);
		return;
	}
#endif
	if (td->td_priority < curthread->td_priority)
		curthread->td_flags |= TDF_NEEDRESCHED;
}

void
sched_rem(struct thread *td)
{
	struct tdq *tdq;
	struct td_sched *ts;

	CTR5(KTR_SCHED, "sched_rem: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;
	KASSERT(TD_ON_RUNQ(td),
	    ("sched_rem: thread not on run queue"));

	tdq = TDQ_CPU(ts->ts_cpu);
	tdq_runq_rem(tdq, ts);
	tdq_load_rem(tdq, ts);
	TD_SET_CAN_RUN(td);
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

	thread_lock(td);
	if (ts->ts_ticks) {
		int rtick;

		sched_pctcpu_update(ts);
		/* How many rtick per second ? */
		rtick = min(SCHED_TICK_HZ(ts) / SCHED_TICK_SECS, hz);
		pctcpu = (FSCALE * ((FSCALE * rtick)/hz)) >> FSHIFT;
	}
	td->td_proc->p_swtime = ts->ts_ltick - ts->ts_ftick;
	thread_unlock(td);

	return (pctcpu);
}

void
sched_bind(struct thread *td, int cpu)
{
	struct td_sched *ts;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;
	if (ts->ts_flags & TSF_BOUND)
		sched_unbind(td);
	ts->ts_flags |= TSF_BOUND;
#ifdef SMP
	sched_pin();
	if (PCPU_GET(cpuid) == cpu)
		return;
	ts->ts_cpu = cpu;
	/* When we return from mi_switch we'll be on the correct cpu. */
	mi_switch(SW_VOL, NULL);
#endif
}

void
sched_unbind(struct thread *td)
{
	struct td_sched *ts;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;
	if ((ts->ts_flags & TSF_BOUND) == 0)
		return;
	ts->ts_flags &= ~TSF_BOUND;
#ifdef SMP
	sched_unpin();
#endif
}

int
sched_is_bound(struct thread *td)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	return (td->td_sched->ts_flags & TSF_BOUND);
}

void
sched_relinquish(struct thread *td)
{
	thread_lock(td);
	if (td->td_pri_class == PRI_TIMESHARE)
		sched_prio(td, PRI_MAX_TIMESHARE);
	SCHED_STAT_INC(switch_relinquish);
	mi_switch(SW_VOL, NULL);
	thread_unlock(td);
}

int
sched_load(void)
{
#ifdef SMP
	int total;
	int i;

	total = 0;
	for (i = 0; i <= tdg_maxid; i++)
		total += TDQ_GROUP(i)->tdg_load;
	return (total);
#else
	return (TDQ_SELF()->tdq_sysload);
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
	struct td_sched *ts;

	ts = curthread->td_sched;
	/* Adjust ticks for pctcpu */
	ts->ts_ticks += 1 << SCHED_TICK_SHIFT;
	ts->ts_ltick = ticks;
	/*
	 * Update if we've exceeded our desired tick threshhold by over one
	 * second.
	 */
	if (ts->ts_ftick + SCHED_TICK_MAX < ts->ts_ltick)
		sched_pctcpu_update(ts);
}

/*
 * The actual idle process.
 */
void
sched_idletd(void *dummy)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	p = td->td_proc;
	mtx_assert(&Giant, MA_NOTOWNED);
	/* ULE Relies on preemption for idle interruption. */
	for (;;)
		cpu_idle();
}

/*
 * A CPU is entering for the first time or a thread is exiting.
 */
void
sched_throw(struct thread *td)
{
	/*
	 * Correct spinlock nesting.  The idle thread context that we are
	 * borrowing was created so that it would start out with a single
	 * spin lock (sched_lock) held in fork_trampoline().  Since we've
	 * explicitly acquired locks in this function, the nesting count
	 * is now 2 rather than 1.  Since we are nested, calling
	 * spinlock_exit() will simply adjust the counts without allowing
	 * spin lock using code to interrupt us.
	 */
	if (td == NULL) {
		mtx_lock_spin(&sched_lock);
		spinlock_exit();
	} else {
		MPASS(td->td_lock == &sched_lock);
	}
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(curthread->td_md.md_spinlock_count == 1, ("invalid count"));
	PCPU_SET(switchtime, cpu_ticks());
	PCPU_SET(switchticks, ticks);
	cpu_throw(td, choosethread());	/* doesn't return */
}

void
sched_fork_exit(struct thread *ctd)
{
	struct thread *td;

	/*
	 * Finish setting up thread glue so that it begins execution in a
	 * non-nested critical section with sched_lock held but not recursed.
	 */
	ctd->td_oncpu = PCPU_GET(cpuid);
	sched_lock.mtx_lock = (uintptr_t)ctd;
	THREAD_LOCK_ASSERT(ctd, MA_OWNED | MA_NOTRECURSED);
	/*
	 * Processes normally resume in mi_switch() after being
	 * cpu_switch()'ed to, but when children start up they arrive here
	 * instead, so we must do much the same things as mi_switch() would.
	 */
	if ((td = PCPU_GET(deadthread))) {
		PCPU_SET(deadthread, NULL);
		thread_stash(td);
	}
	thread_unlock(ctd);
}

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0, "Scheduler");
SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "ule", 0,
    "Scheduler name");
SYSCTL_INT(_kern_sched, OID_AUTO, slice, CTLFLAG_RW, &sched_slice, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, interact, CTLFLAG_RW, &sched_interact, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, tickincr, CTLFLAG_RD, &tickincr, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, realstathz, CTLFLAG_RD, &realstathz, 0, "");
#ifdef SMP
SYSCTL_INT(_kern_sched, OID_AUTO, pick_pri, CTLFLAG_RW, &pick_pri, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, pick_pri_affinity, CTLFLAG_RW,
    &affinity, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, pick_pri_tryself, CTLFLAG_RW,
    &tryself, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, pick_pri_tryselfidle, CTLFLAG_RW,
    &tryselfidle, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, balance, CTLFLAG_RW, &rebalance, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, ipi_preempt, CTLFLAG_RW, &ipi_preempt, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, ipi_ast, CTLFLAG_RW, &ipi_ast, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, ipi_thresh, CTLFLAG_RW, &ipi_thresh, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, steal_htt, CTLFLAG_RW, &steal_htt, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, steal_busy, CTLFLAG_RW, &steal_busy, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, busy_thresh, CTLFLAG_RW, &busy_thresh, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, topology, CTLFLAG_RD, &topology, 0, "");
#endif

/* ps compat */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");


#define KERN_SWITCH_INCLUDE 1
#include "kern/kern_switch.c"
