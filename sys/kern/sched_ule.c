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

/*
 * This file implements the ULE scheduler.  ULE supports independent CPU
 * run queues and fine grain locking.  It has superior interactive
 * performance under load even on uni-processor systems.
 *
 * etymology:
 *   ULE is the last three letters in schedule.  It owes it's name to a
 * generic user created for a scheduling system by Paul Mikesell at
 * Isilon Systems and a general lack of creativity on the part of the author.
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

#define	KTR_ULE	0

/*
 * Thread scheduler specific section.  All fields are protected
 * by the thread lock.
 */
struct td_sched {	
	TAILQ_ENTRY(td_sched) ts_procq;	/* Run queue. */
	struct thread	*ts_thread;	/* Active associated thread. */
	struct runq	*ts_runq;	/* Run-queue we're queued on. */
	short		ts_flags;	/* TSF_* flags. */
	u_char		ts_rqindex;	/* Run queue index. */
	u_char		ts_cpu;		/* CPU that we have affinity for. */
	int		ts_slptick;	/* Tick when we went to sleep. */
	int		ts_slice;	/* Ticks of slice remaining. */
	u_int		ts_slptime;	/* Number of ticks we vol. slept */
	u_int		ts_runtime;	/* Number of ticks we were running */
	/* The following variables are only used for pctcpu calculation */
	int		ts_ltick;	/* Last tick that we were running on */
	int		ts_ftick;	/* First tick that we were running on */
	int		ts_ticks;	/* Tick count */
#ifdef SMP
	int		ts_rltick;	/* Real last tick, for affinity. */
#endif
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
#define	SCHED_PRI_RANGE		(SCHED_PRI_MAX - SCHED_PRI_MIN)
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
 * preempt_thresh:	Priority threshold for preemption and remote IPIs.
 */
static int sched_interact = SCHED_INTERACT_THRESH;
static int realstathz;
static int tickincr;
static int sched_slice;
static int preempt_thresh = PRI_MIN_KERN;

/*
 * tdq - per processor runqs and statistics.  All fields are protected by the
 * tdq_lock.  The load and lowpri may be accessed without to avoid excess
 * locking in sched_pickcpu();
 */
struct tdq {
	struct mtx	*tdq_lock;		/* Pointer to group lock. */
	struct runq	tdq_realtime;		/* real-time run queue. */
	struct runq	tdq_timeshare;		/* timeshare run queue. */
	struct runq	tdq_idle;		/* Queue of IDLE threads. */
	int		tdq_load;		/* Aggregate load. */
	u_char		tdq_idx;		/* Current insert index. */
	u_char		tdq_ridx;		/* Current removal index. */
#ifdef SMP
	u_char		tdq_lowpri;		/* Lowest priority thread. */
	int		tdq_transferable;	/* Transferable thread count. */
	LIST_ENTRY(tdq)	tdq_siblings;		/* Next in tdq group. */
	struct tdq_group *tdq_group;		/* Our processor group. */
#else
	int		tdq_sysload;		/* For loadavg, !ITHD load. */
#endif
} __aligned(64);


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
	struct mtx	tdg_lock;	/* Protects all fields below. */
	int		tdg_cpus;	/* Count of CPUs in this tdq group. */
	cpumask_t 	tdg_cpumask;	/* Mask of cpus in this group. */
	cpumask_t 	tdg_idlemask;	/* Idle cpus in this group. */
	cpumask_t 	tdg_mask;	/* Bit mask for first cpu. */
	int		tdg_load;	/* Total load of this group. */
	int	tdg_transferable;	/* Transferable load of this group. */
	LIST_HEAD(, tdq) tdg_members;	/* Linked list of all members. */
	char		tdg_name[16];	/* lock name. */
} __aligned(64);

#define	SCHED_AFFINITY_DEFAULT	(max(1, hz / 300))
#define	SCHED_AFFINITY(ts)	((ts)->ts_rltick > ticks - affinity)

/*
 * Run-time tunables.
 */
static int rebalance = 1;
static int balance_secs = 1;
static int pick_pri = 1;
static int affinity;
static int tryself = 1;
static int steal_htt = 0;
static int steal_idle = 1;
static int steal_thresh = 2;
static int topology = 0;

/*
 * One thread queue per processor.
 */
static volatile cpumask_t tdq_idle;
static int tdg_maxid;
static struct tdq	tdq_cpu[MAXCPU];
static struct tdq_group tdq_groups[MAXCPU];
static struct callout balco;
static struct callout gbalco;

#define	TDQ_SELF()	(&tdq_cpu[PCPU_GET(cpuid)])
#define	TDQ_CPU(x)	(&tdq_cpu[(x)])
#define	TDQ_ID(x)	((int)((x) - tdq_cpu))
#define	TDQ_GROUP(x)	(&tdq_groups[(x)])
#define	TDG_ID(x)	((int)((x) - tdq_groups))
#else	/* !SMP */
static struct tdq	tdq_cpu;
static struct mtx	tdq_lock;

#define	TDQ_ID(x)	(0)
#define	TDQ_SELF()	(&tdq_cpu)
#define	TDQ_CPU(x)	(&tdq_cpu)
#endif

#define	TDQ_LOCK_ASSERT(t, type)	mtx_assert(TDQ_LOCKPTR((t)), (type))
#define	TDQ_LOCK(t)		mtx_lock_spin(TDQ_LOCKPTR((t)))
#define	TDQ_LOCK_FLAGS(t, f)	mtx_lock_spin_flags(TDQ_LOCKPTR((t)), (f))
#define	TDQ_UNLOCK(t)		mtx_unlock_spin(TDQ_LOCKPTR((t)))
#define	TDQ_LOCKPTR(t)		((t)->tdq_lock)

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
void tdq_print(int cpu);
static void runq_print(struct runq *rq);
static void tdq_add(struct tdq *, struct thread *, int);
#ifdef SMP
static void tdq_move(struct tdq *, struct tdq *);
static int tdq_idled(struct tdq *);
static void tdq_notify(struct td_sched *);
static struct td_sched *tdq_steal(struct tdq *, int);
static struct td_sched *runq_steal(struct runq *);
static int sched_pickcpu(struct td_sched *, int);
static void sched_balance(void *);
static void sched_balance_groups(void *);
static void sched_balance_group(struct tdq_group *);
static void sched_balance_pair(struct tdq *, struct tdq *);
static inline struct tdq *sched_setcpu(struct td_sched *, int, int);
static inline struct mtx *thread_block_switch(struct thread *);
static inline void thread_unblock_switch(struct thread *, struct mtx *);
static struct mtx *sched_switch_migrate(struct tdq *, struct thread *, int);

#define	THREAD_CAN_MIGRATE(td)	 ((td)->td_pinned == 0)
#endif

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

static void sched_initticks(void *dummy);
SYSINIT(sched_initticks, SI_SUB_CLOCKS, SI_ORDER_THIRD, sched_initticks, NULL)

/*
 * Print the threads waiting on a run-queue.
 */
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

/*
 * Print the status of a per-cpu thread queue.  Should be a ddb show cmd.
 */
void
tdq_print(int cpu)
{
	struct tdq *tdq;

	tdq = TDQ_CPU(cpu);

	printf("tdq %d:\n", TDQ_ID(tdq));
	printf("\tlockptr         %p\n", TDQ_LOCKPTR(tdq));
	printf("\tload:           %d\n", tdq->tdq_load);
	printf("\ttimeshare idx:  %d\n", tdq->tdq_idx);
	printf("\ttimeshare ridx: %d\n", tdq->tdq_ridx);
	printf("\trealtime runq:\n");
	runq_print(&tdq->tdq_realtime);
	printf("\ttimeshare runq:\n");
	runq_print(&tdq->tdq_timeshare);
	printf("\tidle runq:\n");
	runq_print(&tdq->tdq_idle);
#ifdef SMP
	printf("\tload transferable: %d\n", tdq->tdq_transferable);
	printf("\tlowest priority:   %d\n", tdq->tdq_lowpri);
	printf("\tgroup:             %d\n", TDG_ID(tdq->tdq_group));
	printf("\tLock name:         %s\n", tdq->tdq_group->tdg_name);
#endif
}

#define	TS_RQ_PPQ	(((PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE) + 1) / RQ_NQS)
/*
 * Add a thread to the actual run-queue.  Keeps transferable counts up to
 * date with what is actually on the run-queue.  Selects the correct
 * queue position for timeshare threads.
 */
static __inline void
tdq_runq_add(struct tdq *tdq, struct td_sched *ts, int flags)
{
	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	THREAD_LOCK_ASSERT(ts->ts_thread, MA_OWNED);
#ifdef SMP
	if (THREAD_CAN_MIGRATE(ts->ts_thread)) {
		tdq->tdq_transferable++;
		tdq->tdq_group->tdg_transferable++;
		ts->ts_flags |= TSF_XFERABLE;
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
		if ((flags & (SRQ_BORROWING|SRQ_PREEMPTED)) == 0) {
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

/* 
 * Remove a thread from a run-queue.  This typically happens when a thread
 * is selected to run.  Running threads are not on the queue and the
 * transferable count does not reflect them.
 */
static __inline void
tdq_runq_rem(struct tdq *tdq, struct td_sched *ts)
{
	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	KASSERT(ts->ts_runq != NULL,
	    ("tdq_runq_remove: thread %p null ts_runq", ts->ts_thread));
#ifdef SMP
	if (ts->ts_flags & TSF_XFERABLE) {
		tdq->tdq_transferable--;
		tdq->tdq_group->tdg_transferable--;
		ts->ts_flags &= ~TSF_XFERABLE;
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

/*
 * Load is maintained for all threads RUNNING and ON_RUNQ.  Add the load
 * for this thread to the referenced thread queue.
 */
static void
tdq_load_add(struct tdq *tdq, struct td_sched *ts)
{
	int class;

	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	THREAD_LOCK_ASSERT(ts->ts_thread, MA_OWNED);
	class = PRI_BASE(ts->ts_thread->td_pri_class);
	tdq->tdq_load++;
	CTR2(KTR_SCHED, "cpu %d load: %d", TDQ_ID(tdq), tdq->tdq_load);
	if (class != PRI_ITHD &&
	    (ts->ts_thread->td_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		tdq->tdq_group->tdg_load++;
#else
		tdq->tdq_sysload++;
#endif
}

/*
 * Remove the load from a thread that is transitioning to a sleep state or
 * exiting.
 */
static void
tdq_load_rem(struct tdq *tdq, struct td_sched *ts)
{
	int class;

	THREAD_LOCK_ASSERT(ts->ts_thread, MA_OWNED);
	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	class = PRI_BASE(ts->ts_thread->td_pri_class);
	if (class != PRI_ITHD &&
	    (ts->ts_thread->td_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		tdq->tdq_group->tdg_load--;
#else
		tdq->tdq_sysload--;
#endif
	KASSERT(tdq->tdq_load != 0,
	    ("tdq_load_rem: Removing with 0 load on queue %d", TDQ_ID(tdq)));
	tdq->tdq_load--;
	CTR1(KTR_SCHED, "load: %d", tdq->tdq_load);
	ts->ts_runq = NULL;
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
 * simplicity and more gradual effects on load in larger systems.
 *
 */
static void
sched_balance(void *arg)
{
	struct tdq_group *high;
	struct tdq_group *low;
	struct tdq_group *tdg;
	int cnt;
	int i;

	callout_reset(&balco, max(hz / 2, random() % (hz * balance_secs)),
	    sched_balance, NULL);
	if (smp_started == 0 || rebalance == 0)
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

/*
 * Balance load between CPUs in a group.  Will only migrate within the group.
 */
static void
sched_balance_groups(void *arg)
{
	int i;

	callout_reset(&gbalco, max(hz / 2, random() % (hz * balance_secs)),
	    sched_balance_groups, NULL);
	if (smp_started == 0 || rebalance == 0)
		return;
	for (i = 0; i <= tdg_maxid; i++)
		sched_balance_group(TDQ_GROUP(i));
}

/*
 * Finds the greatest imbalance between two tdqs in a group.
 */
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

/*
 * Lock two thread queues using their address to maintain lock order.
 */
static void
tdq_lock_pair(struct tdq *one, struct tdq *two)
{
	if (one < two) {
		TDQ_LOCK(one);
		TDQ_LOCK_FLAGS(two, MTX_DUPOK);
	} else {
		TDQ_LOCK(two);
		TDQ_LOCK_FLAGS(one, MTX_DUPOK);
	}
}

/*
 * Transfer load between two imbalanced thread queues.
 */
static void
sched_balance_pair(struct tdq *high, struct tdq *low)
{
	int transferable;
	int high_load;
	int low_load;
	int move;
	int diff;
	int i;

	tdq_lock_pair(high, low);
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
	/*
	 * Determine what the imbalance is and then adjust that to how many
	 * threads we actually have to give up (transferable).
	 */
	if (transferable != 0) {
		diff = high_load - low_load;
		move = diff / 2;
		if (diff & 0x1)
			move++;
		move = min(move, transferable);
		for (i = 0; i < move; i++)
			tdq_move(high, low);
	}
	TDQ_UNLOCK(high);
	TDQ_UNLOCK(low);
	return;
}

/*
 * Move a thread from one thread queue to another.
 */
static void
tdq_move(struct tdq *from, struct tdq *to)
{
	struct td_sched *ts;
	struct thread *td;
	struct tdq *tdq;
	int cpu;

	tdq = from;
	cpu = TDQ_ID(to);
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
			return;
	}
	if (tdq == to)
		return;
	td = ts->ts_thread;
	/*
	 * Although the run queue is locked the thread may be blocked.  Lock
	 * it to clear this.
	 */
	thread_lock(td);
	/* Drop recursive lock on from. */
	TDQ_UNLOCK(from);
	sched_rem(td);
	ts->ts_cpu = cpu;
	td->td_lock = TDQ_LOCKPTR(to);
	tdq_add(to, td, SRQ_YIELDING);
	tdq_notify(ts);
}

/*
 * This tdq has idled.  Try to steal a thread from another cpu and switch
 * to it.
 */
static int
tdq_idled(struct tdq *tdq)
{
	struct tdq_group *tdg;
	struct tdq *steal;
	struct td_sched *ts;
	struct thread *td;
	int highload;
	int highcpu;
	int load;
	int cpu;

	/* We don't want to be preempted while we're iterating over tdqs */
	spinlock_enter();
	tdg = tdq->tdq_group;
	/*
	 * If we're in a cpu group, try and steal threads from another cpu in
	 * the group before idling.
	 */
	if (steal_htt && tdg->tdg_cpus > 1 && tdg->tdg_transferable) {
		LIST_FOREACH(steal, &tdg->tdg_members, tdq_siblings) {
			if (steal == tdq || steal->tdq_transferable == 0)
				continue;
			TDQ_LOCK(steal);
			ts = tdq_steal(steal, 0);
			if (ts)
				goto steal;
			TDQ_UNLOCK(steal);
		}
	}
	for (;;) {
		if (steal_idle == 0)
			break;
		highcpu = 0;
		highload = 0;
		for (cpu = 0; cpu <= mp_maxid; cpu++) {
			if (CPU_ABSENT(cpu))
				continue;
			steal = TDQ_CPU(cpu);
			load = TDQ_CPU(cpu)->tdq_transferable;
			if (load < highload)
				continue;
			highload = load;
			highcpu = cpu;
		}
		if (highload < steal_thresh)
			break;
		steal = TDQ_CPU(highcpu);
		TDQ_LOCK(steal);
		if (steal->tdq_transferable >= steal_thresh &&
		    (ts = tdq_steal(steal, 1)) != NULL)
			goto steal;
		TDQ_UNLOCK(steal);
		break;
	}
	spinlock_exit();
	return (1);
steal:
	td = ts->ts_thread;
	thread_lock(td);
	spinlock_exit();
	MPASS(td->td_lock == TDQ_LOCKPTR(steal));
	TDQ_UNLOCK(steal);
	sched_rem(td);
	sched_setcpu(ts, PCPU_GET(cpuid), SRQ_YIELDING);
	tdq_add(tdq, td, SRQ_YIELDING);
	MPASS(td->td_lock == curthread->td_lock);
	mi_switch(SW_VOL, NULL);
	thread_unlock(curthread);

	return (0);
}

/*
 * Notify a remote cpu of new work.  Sends an IPI if criteria are met.
 */
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
	if (pri > preempt_thresh)
		return;
sendipi:
	ctd->td_flags |= TDF_NEEDRESCHED;
	ipi_selected(1 << cpu, IPI_PREEMPT);
}

/*
 * Steals load from a timeshare queue.  Honors the rotating queue head
 * index.
 */
static struct td_sched *
runq_steal_from(struct runq *rq, u_char start)
{
	struct td_sched *ts;
	struct rqbits *rqb;
	struct rqhead *rqh;
	int first;
	int bit;
	int pri;
	int i;

	rqb = &rq->rq_status;
	bit = start & (RQB_BPW -1);
	pri = 0;
	first = 0;
again:
	for (i = RQB_WORD(start); i < RQB_LEN; bit = 0, i++) {
		if (rqb->rqb_bits[i] == 0)
			continue;
		if (bit != 0) {
			for (pri = bit; pri < RQB_BPW; pri++)
				if (rqb->rqb_bits[i] & (1ul << pri))
					break;
			if (pri >= RQB_BPW)
				continue;
		} else
			pri = RQB_FFS(rqb->rqb_bits[i]);
		pri += (i << RQB_L2BPW);
		rqh = &rq->rq_queues[pri];
		TAILQ_FOREACH(ts, rqh, ts_procq) {
			if (first && THREAD_CAN_MIGRATE(ts->ts_thread))
				return (ts);
			first = 1;
		}
	}
	if (start != 0) {
		start = 0;
		goto again;
	}

	return (NULL);
}

/*
 * Steals load from a standard linear queue.
 */
static struct td_sched *
runq_steal(struct runq *rq)
{
	struct rqhead *rqh;
	struct rqbits *rqb;
	struct td_sched *ts;
	int word;
	int bit;

	rqb = &rq->rq_status;
	for (word = 0; word < RQB_LEN; word++) {
		if (rqb->rqb_bits[word] == 0)
			continue;
		for (bit = 0; bit < RQB_BPW; bit++) {
			if ((rqb->rqb_bits[word] & (1ul << bit)) == 0)
				continue;
			rqh = &rq->rq_queues[bit + (word << RQB_L2BPW)];
			TAILQ_FOREACH(ts, rqh, ts_procq)
				if (THREAD_CAN_MIGRATE(ts->ts_thread))
					return (ts);
		}
	}
	return (NULL);
}

/*
 * Attempt to steal a thread in priority order from a thread queue.
 */
static struct td_sched *
tdq_steal(struct tdq *tdq, int stealidle)
{
	struct td_sched *ts;

	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	if ((ts = runq_steal(&tdq->tdq_realtime)) != NULL)
		return (ts);
	if ((ts = runq_steal_from(&tdq->tdq_timeshare, tdq->tdq_ridx)) != NULL)
		return (ts);
	if (stealidle)
		return (runq_steal(&tdq->tdq_idle));
	return (NULL);
}

/*
 * Sets the thread lock and ts_cpu to match the requested cpu.  Unlocks the
 * current lock and returns with the assigned queue locked.  If this is
 * via sched_switch() we leave the thread in a blocked state as an
 * optimization.
 */
static inline struct tdq *
sched_setcpu(struct td_sched *ts, int cpu, int flags)
{
	struct thread *td;
	struct tdq *tdq;

	THREAD_LOCK_ASSERT(ts->ts_thread, MA_OWNED);

	tdq = TDQ_CPU(cpu);
	td = ts->ts_thread;
	ts->ts_cpu = cpu;

	/* If the lock matches just return the queue. */
	if (td->td_lock == TDQ_LOCKPTR(tdq))
		return (tdq);
#ifdef notyet
	/*
	 * If the thread isn't running it's lockptr is a
	 * turnstile or a sleepqueue.  We can just lock_set without
	 * blocking.
	 */
	if (TD_CAN_RUN(td)) {
		TDQ_LOCK(tdq);
		thread_lock_set(td, TDQ_LOCKPTR(tdq));
		return (tdq);
	}
#endif
	/*
	 * The hard case, migration, we need to block the thread first to
	 * prevent order reversals with other cpus locks.
	 */
	thread_lock_block(td);
	TDQ_LOCK(tdq);
	thread_lock_unblock(td, TDQ_LOCKPTR(tdq));
	return (tdq);
}

/*
 * Find the thread queue running the lowest priority thread.
 */
static int
tdq_lowestpri(void)
{
	struct tdq *tdq;
	int lowpri;
	int lowcpu;
	int lowload;
	int load;
	int cpu;
	int pri;

	lowload = 0;
	lowpri = lowcpu = 0;
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		tdq = TDQ_CPU(cpu);
		pri = tdq->tdq_lowpri;
		load = TDQ_CPU(cpu)->tdq_load;
		CTR4(KTR_ULE,
		    "cpu %d pri %d lowcpu %d lowpri %d",
		    cpu, pri, lowcpu, lowpri);
		if (pri < lowpri)
			continue;
		if (lowpri && lowpri == pri && load > lowload)
			continue;
		lowpri = pri;
		lowcpu = cpu;
		lowload = load;
	}

	return (lowcpu);
}

/*
 * Find the thread queue with the least load.
 */
static int
tdq_lowestload(void)
{
	struct tdq *tdq;
	int lowload;
	int lowpri;
	int lowcpu;
	int load;
	int cpu;
	int pri;

	lowcpu = 0;
	lowload = TDQ_CPU(0)->tdq_load;
	lowpri = TDQ_CPU(0)->tdq_lowpri;
	for (cpu = 1; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		tdq = TDQ_CPU(cpu);
		load = tdq->tdq_load;
		pri = tdq->tdq_lowpri;
		CTR4(KTR_ULE, "cpu %d load %d lowcpu %d lowload %d",
		    cpu, load, lowcpu, lowload);
		if (load > lowload)
			continue;
		if (load == lowload && pri < lowpri)
			continue;
		lowcpu = cpu;
		lowload = load;
		lowpri = pri;
	}

	return (lowcpu);
}

/*
 * Pick the destination cpu for sched_add().  Respects affinity and makes
 * a determination based on load or priority of available processors.
 */
static int
sched_pickcpu(struct td_sched *ts, int flags)
{
	struct tdq *tdq;
	int self;
	int pri;
	int cpu;

	cpu = self = PCPU_GET(cpuid);
	if (smp_started == 0)
		return (self);
	/*
	 * Don't migrate a running thread from sched_switch().
	 */
	if (flags & SRQ_OURSELF) {
		CTR1(KTR_ULE, "YIELDING %d",
		    curthread->td_priority);
		return (self);
	}
	pri = ts->ts_thread->td_priority;
	cpu = ts->ts_cpu;
	/*
	 * Regardless of affinity, if the last cpu is idle send it there.
	 */
	tdq = TDQ_CPU(cpu);
	if (tdq->tdq_lowpri > PRI_MIN_IDLE) {
		CTR5(KTR_ULE,
		    "ts_cpu %d idle, ltick %d ticks %d pri %d curthread %d",
		    ts->ts_cpu, ts->ts_rltick, ticks, pri,
		    tdq->tdq_lowpri);
		return (ts->ts_cpu);
	}
	/*
	 * If we have affinity, try to place it on the cpu we last ran on.
	 */
	if (SCHED_AFFINITY(ts) && tdq->tdq_lowpri > pri) {
		CTR5(KTR_ULE,
		    "affinity for %d, ltick %d ticks %d pri %d curthread %d",
		    ts->ts_cpu, ts->ts_rltick, ticks, pri,
		    tdq->tdq_lowpri);
		return (ts->ts_cpu);
	}
	/*
	 * Look for an idle group.
	 */
	CTR1(KTR_ULE, "tdq_idle %X", tdq_idle);
	cpu = ffs(tdq_idle);
	if (cpu)
		return (--cpu);
	/*
	 * If there are no idle cores see if we can run the thread locally.  This may
	 * improve locality among sleepers and wakers when there is shared data.
	 */
	if (tryself && pri < curthread->td_priority) {
		CTR1(KTR_ULE, "tryself %d",
		    curthread->td_priority);
		return (self);
	}
	/*
 	 * Now search for the cpu running the lowest priority thread with
	 * the least load.
	 */
	if (pick_pri)
		cpu = tdq_lowestpri();
	else
		cpu = tdq_lowestload();
	return (cpu);
}

#endif	/* SMP */

/*
 * Pick the highest priority task we have and return it.
 */
static struct td_sched *
tdq_choose(struct tdq *tdq)
{
	struct td_sched *ts;

	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	ts = runq_choose(&tdq->tdq_realtime);
	if (ts != NULL)
		return (ts);
	ts = runq_choose_from(&tdq->tdq_timeshare, tdq->tdq_ridx);
	if (ts != NULL) {
		KASSERT(ts->ts_thread->td_priority >= PRI_MIN_TIMESHARE,
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

/*
 * Initialize a thread queue.
 */
static void
tdq_setup(struct tdq *tdq)
{

	if (bootverbose)
		printf("ULE: setup cpu %d\n", TDQ_ID(tdq));
	runq_init(&tdq->tdq_realtime);
	runq_init(&tdq->tdq_timeshare);
	runq_init(&tdq->tdq_idle);
	tdq->tdq_load = 0;
}

#ifdef SMP
static void
tdg_setup(struct tdq_group *tdg)
{
	if (bootverbose)
		printf("ULE: setup cpu group %d\n", TDG_ID(tdg));
	snprintf(tdg->tdg_name, sizeof(tdg->tdg_name),
	    "sched lock %d", (int)TDG_ID(tdg));
	mtx_init(&tdg->tdg_lock, tdg->tdg_name, "sched lock",
	    MTX_SPIN | MTX_RECURSE);
	LIST_INIT(&tdg->tdg_members);
	tdg->tdg_load = 0;
	tdg->tdg_transferable = 0;
	tdg->tdg_cpus = 0;
	tdg->tdg_mask = 0;
	tdg->tdg_cpumask = 0;
	tdg->tdg_idlemask = 0;
}

static void
tdg_add(struct tdq_group *tdg, struct tdq *tdq)
{
	if (tdg->tdg_mask == 0)
		tdg->tdg_mask |= 1 << TDQ_ID(tdq);
	tdg->tdg_cpumask |= 1 << TDQ_ID(tdq);
	tdg->tdg_cpus++;
	tdq->tdq_group = tdg;
	tdq->tdq_lock = &tdg->tdg_lock;
	LIST_INSERT_HEAD(&tdg->tdg_members, tdq, tdq_siblings);
	if (bootverbose)
		printf("ULE: adding cpu %d to group %d: cpus %d mask 0x%X\n",
		    TDQ_ID(tdq), TDG_ID(tdg), tdg->tdg_cpus, tdg->tdg_cpumask);
}

static void
sched_setup_topology(void)
{
	struct tdq_group *tdg;
	struct cpu_group *cg;
	int balance_groups;
	struct tdq *tdq;
	int i;
	int j;

	topology = 1;
	balance_groups = 0;
	for (i = 0; i < smp_topology->ct_count; i++) {
		cg = &smp_topology->ct_group[i];
		tdg = &tdq_groups[i];
		/*
		 * Initialize the group.
		 */
		tdg_setup(tdg);
		/*
		 * Find all of the group members and add them.
		 */
		for (j = 0; j < MAXCPU; j++) { 
			if ((cg->cg_mask & (1 << j)) != 0) {
				tdq = TDQ_CPU(j);
				tdq_setup(tdq);
				tdg_add(tdg, tdq);
			}
		}
		if (tdg->tdg_cpus > 1)
			balance_groups = 1;
	}
	tdg_maxid = smp_topology->ct_count - 1;
	if (balance_groups)
		sched_balance_groups(NULL);
}

static void
sched_setup_smp(void)
{
	struct tdq_group *tdg;
	struct tdq *tdq;
	int cpus;
	int i;

	for (cpus = 0, i = 0; i < MAXCPU; i++) {
		if (CPU_ABSENT(i))
			continue;
		tdq = &tdq_cpu[i];
		tdg = &tdq_groups[i];
		/*
		 * Setup a tdq group with one member.
		 */
		tdg_setup(tdg);
		tdq_setup(tdq);
		tdg_add(tdg, tdq);
		cpus++;
	}
	tdg_maxid = cpus - 1;
}

/*
 * Fake a topology with one group containing all CPUs.
 */
static void
sched_fake_topo(void)
{
#ifdef SCHED_FAKE_TOPOLOGY
	static struct cpu_top top;
	static struct cpu_group group;

	top.ct_count = 1;
	top.ct_group = &group;
	group.cg_mask = all_cpus;
	group.cg_count = mp_ncpus;
	group.cg_children = 0;
	smp_topology = &top;
#endif
}
#endif

/*
 * Setup the thread queues and initialize the topology based on MD
 * information.
 */
static void
sched_setup(void *dummy)
{
	struct tdq *tdq;

	tdq = TDQ_SELF();
#ifdef SMP
	/*
	 * Initialize long-term cpu balancing algorithm.
	 */
	callout_init(&balco, CALLOUT_MPSAFE);
	callout_init(&gbalco, CALLOUT_MPSAFE);
	sched_fake_topo();
	/*
	 * Setup tdqs based on a topology configuration or vanilla SMP based
	 * on mp_maxid.
	 */
	if (smp_topology == NULL)
		sched_setup_smp();
	else 
		sched_setup_topology();
	sched_balance(NULL);
#else
	tdq_setup(tdq);
	mtx_init(&tdq_lock, "sched lock", "sched lock", MTX_SPIN | MTX_RECURSE);
	tdq->tdq_lock = &tdq_lock;
#endif
	/*
	 * To avoid divide-by-zero, we set realstathz a dummy value
	 * in case which sched_clock() called before sched_initticks().
	 */
	realstathz = hz;
	sched_slice = (realstathz/10);	/* ~100ms */
	tickincr = 1 << SCHED_TICK_SHIFT;

	/* Add thread0's load since it's running. */
	TDQ_LOCK(tdq);
	thread0.td_lock = TDQ_LOCKPTR(TDQ_SELF());
	tdq_load_add(tdq, &td_sched0);
	TDQ_UNLOCK(tdq);
}

/*
 * This routine determines the tickincr after stathz and hz are setup.
 */
/* ARGSUSED */
static void
sched_initticks(void *dummy)
{
	int incr;

	realstathz = stathz ? stathz : hz;
	sched_slice = (realstathz/10);	/* ~100ms */

	/*
	 * tickincr is shifted out by 10 to avoid rounding errors due to
	 * hz not being evenly divisible by stathz on all platforms.
	 */
	incr = (hz << SCHED_TICK_SHIFT) / realstathz;
	/*
	 * This does not work for values of stathz that are more than
	 * 1 << SCHED_TICK_SHIFT * hz.  In practice this does not happen.
	 */
	if (incr == 0)
		incr = 1;
	tickincr = incr;
#ifdef SMP
	affinity = SCHED_AFFINITY_DEFAULT;
#endif
}


/*
 * This is the core of the interactivity algorithm.  Determines a score based
 * on past behavior.  It is the ratio of sleep time to run time scaled to
 * a [0, 100] integer.  This is the voluntary sleep time of a process, which
 * differs from the cpu usage because it does not account for time spent
 * waiting on a run-queue.  Would be prettier if we had floating point.
 */
static int
sched_interact_score(struct thread *td)
{
	struct td_sched *ts;
	int div;

	ts = td->td_sched;
	/*
	 * The score is only needed if this is likely to be an interactive
	 * task.  Don't go through the expense of computing it if there's
	 * no chance.
	 */
	if (sched_interact <= SCHED_INTERACT_HALF &&
		ts->ts_runtime >= ts->ts_slptime)
			return (SCHED_INTERACT_HALF);

	if (ts->ts_runtime > ts->ts_slptime) {
		div = max(1, ts->ts_runtime / SCHED_INTERACT_HALF);
		return (SCHED_INTERACT_HALF +
		    (SCHED_INTERACT_HALF - (ts->ts_slptime / div)));
	}
	if (ts->ts_slptime > ts->ts_runtime) {
		div = max(1, ts->ts_slptime / SCHED_INTERACT_HALF);
		return (ts->ts_runtime / div);
	}
	/* runtime == slptime */
	if (ts->ts_runtime)
		return (SCHED_INTERACT_HALF);

	/*
	 * This can happen if slptime and runtime are 0.
	 */
	return (0);

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
	 * Scores greater than this are placed on the normal timeshare queue
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
		KASSERT(pri >= PRI_MIN_TIMESHARE && pri <= PRI_MAX_TIMESHARE,
		    ("sched_priority: invalid priority %d: nice %d, " 
		    "ticks %d ftick %d ltick %d tick pri %d",
		    pri, td->td_proc->p_nice, td->td_sched->ts_ticks,
		    td->td_sched->ts_ftick, td->td_sched->ts_ltick,
		    SCHED_PRI_TICKS(td->td_sched)));
	}
	sched_user_prio(td, pri);

	return;
}

/*
 * This routine enforces a maximum limit on the amount of scheduling history
 * kept.  It is called after either the slptime or runtime is adjusted.  This
 * function is ugly due to integer math.
 */
static void
sched_interact_update(struct thread *td)
{
	struct td_sched *ts;
	u_int sum;

	ts = td->td_sched;
	sum = ts->ts_runtime + ts->ts_slptime;
	if (sum < SCHED_SLP_RUN_MAX)
		return;
	/*
	 * This only happens from two places:
	 * 1) We have added an unusual amount of run time from fork_exit.
	 * 2) We have added an unusual amount of sleep time from sched_sleep().
	 */
	if (sum > SCHED_SLP_RUN_MAX * 2) {
		if (ts->ts_runtime > ts->ts_slptime) {
			ts->ts_runtime = SCHED_SLP_RUN_MAX;
			ts->ts_slptime = 1;
		} else {
			ts->ts_slptime = SCHED_SLP_RUN_MAX;
			ts->ts_runtime = 1;
		}
		return;
	}
	/*
	 * If we have exceeded by more than 1/5th then the algorithm below
	 * will not bring us back into range.  Dividing by two here forces
	 * us into the range of [4/5 * SCHED_INTERACT_MAX, SCHED_INTERACT_MAX]
	 */
	if (sum > (SCHED_SLP_RUN_MAX / 5) * 6) {
		ts->ts_runtime /= 2;
		ts->ts_slptime /= 2;
		return;
	}
	ts->ts_runtime = (ts->ts_runtime / 5) * 4;
	ts->ts_slptime = (ts->ts_slptime / 5) * 4;
}

/*
 * Scale back the interactivity history when a child thread is created.  The
 * history is inherited from the parent but the thread may behave totally
 * differently.  For example, a shell spawning a compiler process.  We want
 * to learn that the compiler is behaving badly very quickly.
 */
static void
sched_interact_fork(struct thread *td)
{
	int ratio;
	int sum;

	sum = td->td_sched->ts_runtime + td->td_sched->ts_slptime;
	if (sum > SCHED_SLP_RUN_FORK) {
		ratio = sum / SCHED_SLP_RUN_FORK;
		td->td_sched->ts_runtime /= ratio;
		td->td_sched->ts_slptime /= ratio;
	}
}

/*
 * Called from proc0_init() to setup the scheduler fields.
 */
void
schedinit(void)
{

	/*
	 * Set up the scheduler specific parts of proc0.
	 */
	proc0.p_sched = NULL; /* XXX */
	thread0.td_sched = &td_sched0;
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

/*
 * Update the percent cpu tracking information when it is requested or
 * the total history exceeds the maximum.  We keep a sliding history of
 * tick counts that slowly decays.  This is less precise than the 4BSD
 * mechanism since it happens with less regular and frequent events.
 */
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

/*
 * Adjust the priority of a thread.  Move it to the appropriate run-queue
 * if necessary.  This is the back-end for several priority related
 * functions.
 */
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
		sched_rem(td);
		td->td_priority = prio;
		sched_add(td, SRQ_BORROWING);
	} else {
#ifdef SMP
		struct tdq *tdq;

		tdq = TDQ_CPU(ts->ts_cpu);
		if (prio < tdq->tdq_lowpri)
			tdq->tdq_lowpri = prio;
#endif
		td->td_priority = prio;
	}
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

/*
 * Standard entry for setting the priority to an absolute value.
 */
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

/*
 * Set the base user priority, does not effect current running priority.
 */
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

/*
 * Add the thread passed as 'newtd' to the run queue before selecting
 * the next thread to run.  This is only used for KSE.
 */
static void
sched_switchin(struct tdq *tdq, struct thread *td)
{
#ifdef SMP
	spinlock_enter();
	TDQ_UNLOCK(tdq);
	thread_lock(td);
	spinlock_exit();
	sched_setcpu(td->td_sched, TDQ_ID(tdq), SRQ_YIELDING);
#else
	td->td_lock = TDQ_LOCKPTR(tdq);
#endif
	tdq_add(tdq, td, SRQ_YIELDING);
	MPASS(td->td_lock == TDQ_LOCKPTR(tdq));
}

/*
 * Handle migration from sched_switch().  This happens only for
 * cpu binding.
 */
static struct mtx *
sched_switch_migrate(struct tdq *tdq, struct thread *td, int flags)
{
	struct tdq *tdn;

	tdn = TDQ_CPU(td->td_sched->ts_cpu);
#ifdef SMP
	/*
	 * Do the lock dance required to avoid LOR.  We grab an extra
	 * spinlock nesting to prevent preemption while we're
	 * not holding either run-queue lock.
	 */
	spinlock_enter();
	thread_block_switch(td);	/* This releases the lock on tdq. */
	TDQ_LOCK(tdn);
	tdq_add(tdn, td, flags);
	tdq_notify(td->td_sched);
	/*
	 * After we unlock tdn the new cpu still can't switch into this
	 * thread until we've unblocked it in cpu_switch().  The lock
	 * pointers may match in the case of HTT cores.  Don't unlock here
	 * or we can deadlock when the other CPU runs the IPI handler.
	 */
	if (TDQ_LOCKPTR(tdn) != TDQ_LOCKPTR(tdq)) {
		TDQ_UNLOCK(tdn);
		TDQ_LOCK(tdq);
	}
	spinlock_exit();
#endif
	return (TDQ_LOCKPTR(tdn));
}

/*
 * Block a thread for switching.  Similar to thread_block() but does not
 * bump the spin count.
 */
static inline struct mtx *
thread_block_switch(struct thread *td)
{
	struct mtx *lock;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	lock = td->td_lock;
	td->td_lock = &blocked_lock;
	mtx_unlock_spin(lock);

	return (lock);
}

/*
 * Release a thread that was blocked with thread_block_switch().
 */
static inline void
thread_unblock_switch(struct thread *td, struct mtx *mtx)
{
	atomic_store_rel_ptr((volatile uintptr_t *)&td->td_lock,
	    (uintptr_t)mtx);
}

/*
 * Switch threads.  This function has to handle threads coming in while
 * blocked for some reason, running, or idle.  It also must deal with
 * migrating a thread from one queue to another as running threads may
 * be assigned elsewhere via binding.
 */
void
sched_switch(struct thread *td, struct thread *newtd, int flags)
{
	struct tdq *tdq;
	struct td_sched *ts;
	struct mtx *mtx;
	int srqflag;
	int cpuid;

	THREAD_LOCK_ASSERT(td, MA_OWNED);

	cpuid = PCPU_GET(cpuid);
	tdq = TDQ_CPU(cpuid);
	ts = td->td_sched;
	mtx = td->td_lock;
#ifdef SMP
	ts->ts_rltick = ticks;
	if (newtd && newtd->td_priority < tdq->tdq_lowpri)
		tdq->tdq_lowpri = newtd->td_priority;
#endif
	td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;
	/*
	 * The lock pointer in an idle thread should never change.  Reset it
	 * to CAN_RUN as well.
	 */
	if (TD_IS_IDLETHREAD(td)) {
		MPASS(td->td_lock == TDQ_LOCKPTR(tdq));
		TD_SET_CAN_RUN(td);
	} else if (TD_IS_RUNNING(td)) {
		MPASS(td->td_lock == TDQ_LOCKPTR(tdq));
		tdq_load_rem(tdq, ts);
		srqflag = (flags & SW_PREEMPT) ?
		    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
		    SRQ_OURSELF|SRQ_YIELDING;
		if (ts->ts_cpu == cpuid)
			tdq_add(tdq, td, srqflag);
		else
			mtx = sched_switch_migrate(tdq, td, srqflag);
	} else {
		/* This thread must be going to sleep. */
		TDQ_LOCK(tdq);
		mtx = thread_block_switch(td);
		tdq_load_rem(tdq, ts);
	}
	/*
	 * We enter here with the thread blocked and assigned to the
	 * appropriate cpu run-queue or sleep-queue and with the current
	 * thread-queue locked.
	 */
	TDQ_LOCK_ASSERT(tdq, MA_OWNED | MA_NOTRECURSED);
	/*
	 * If KSE assigned a new thread just add it here and let choosethread
	 * select the best one.
	 */
	if (newtd != NULL)
		sched_switchin(tdq, newtd);
	newtd = choosethread();
	/*
	 * Call the MD code to switch contexts if necessary.
	 */
	if (td != newtd) {
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_OUT);
#endif
		cpu_switch(td, newtd, mtx);
		/*
		 * We may return from cpu_switch on a different cpu.  However,
		 * we always return with td_lock pointing to the current cpu's
		 * run queue lock.
		 */
		cpuid = PCPU_GET(cpuid);
		tdq = TDQ_CPU(cpuid);
		TDQ_LOCKPTR(tdq)->mtx_lock = (uintptr_t)td;
#ifdef	HWPMC_HOOKS
		if (PMC_PROC_IS_USING_PMCS(td->td_proc))
			PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_IN);
#endif
	} else
		thread_unblock_switch(td, mtx);
	/*
	 * Assert that all went well and return.
	 */
#ifdef SMP
	/* We should always get here with the lowest priority td possible */
	tdq->tdq_lowpri = td->td_priority;
#endif
	TDQ_LOCK_ASSERT(tdq, MA_OWNED|MA_NOTRECURSED);
	MPASS(td->td_lock == TDQ_LOCKPTR(tdq));
	td->td_oncpu = cpuid;
}

/*
 * Adjust thread priorities as a result of a nice request.
 */
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

/*
 * Record the sleep time for the interactivity scorer.
 */
void
sched_sleep(struct thread *td)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);

	td->td_sched->ts_slptick = ticks;
}

/*
 * Schedule a thread to resume execution and record how long it voluntarily
 * slept.  We also update the pctcpu, interactivity, and priority.
 */
void
sched_wakeup(struct thread *td)
{
	struct td_sched *ts;
	int slptick;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;
	/*
	 * If we slept for more than a tick update our interactivity and
	 * priority.
	 */
	slptick = ts->ts_slptick;
	ts->ts_slptick = 0;
	if (slptick && slptick != ticks) {
		u_int hzticks;

		hzticks = (ticks - slptick) << SCHED_TICK_SHIFT;
		ts->ts_slptime += hzticks;
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
	td->td_sched->ts_runtime += tickincr;
	sched_interact_update(td);
	sched_priority(td);
}

/*
 * Fork a new thread, may be within the same process.
 */
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
	child->td_lock = TDQ_LOCKPTR(TDQ_SELF());
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
	ts2->ts_slptime = ts->ts_slptime;
	ts2->ts_runtime = ts->ts_runtime;
	ts2->ts_slice = 1;	/* Attempt to quickly learn interactivity. */
}

/*
 * Adjust the priority class of a thread.
 */
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

/*
 * Penalize another thread for the time spent on this one.  This helps to
 * worsen the priority and interactivity of processes which schedule batch
 * jobs such as make.  This has little effect on the make process itself but
 * causes new processes spawned by it to receive worse scores immediately.
 */
void
sched_exit_thread(struct thread *td, struct thread *child)
{

	CTR3(KTR_SCHED, "sched_exit_thread: %p(%s) prio %d",
	    child, child->td_proc->p_comm, child->td_priority);

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
	td->td_sched->ts_runtime += child->td_sched->ts_runtime;
	sched_interact_update(td);
	sched_priority(td);
	thread_unlock(td);
}

/*
 * Fix priorities on return to user-space.  Priorities may be elevated due
 * to static priorities in msleep() or similar.
 */
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

/*
 * Handle a stathz tick.  This is really only relevant for timeshare
 * threads.
 */
void
sched_clock(struct thread *td)
{
	struct tdq *tdq;
	struct td_sched *ts;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
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
	td->td_sched->ts_runtime += tickincr;
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

/*
 * Called once per hz tick.  Used for cpu utilization information.  This
 * is easier than trying to scale based on stathz.
 */
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
 * Return whether the current CPU has runnable tasks.  Used for in-kernel
 * cooperative idle threads.
 */
int
sched_runnable(void)
{
	struct tdq *tdq;
	int load;

	load = 1;

	tdq = TDQ_SELF();
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

/*
 * Choose the highest priority thread to run.  The thread is removed from
 * the run-queue while running however the load remains.  For SMP we set
 * the tdq in the global idle bitmask if it idles here.
 */
struct thread *
sched_choose(void)
{
#ifdef SMP
	struct tdq_group *tdg;
#endif
	struct td_sched *ts;
	struct tdq *tdq;

	tdq = TDQ_SELF();
	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	ts = tdq_choose(tdq);
	if (ts) {
		tdq_runq_rem(tdq, ts);
		return (ts->ts_thread);
	}
#ifdef SMP
	/*
	 * We only set the idled bit when all of the cpus in the group are
	 * idle.  Otherwise we could get into a situation where a thread bounces
	 * back and forth between two idle cores on seperate physical CPUs.
	 */
	tdg = tdq->tdq_group;
	tdg->tdg_idlemask |= PCPU_GET(cpumask);
	if (tdg->tdg_idlemask == tdg->tdg_cpumask)
		atomic_set_int(&tdq_idle, tdg->tdg_mask);
	tdq->tdq_lowpri = PRI_MAX_IDLE;
#endif
	return (PCPU_GET(idlethread));
}

/*
 * Set owepreempt if necessary.  Preemption never happens directly in ULE,
 * we always request it once we exit a critical section.
 */
static inline void
sched_setpreempt(struct thread *td)
{
	struct thread *ctd;
	int cpri;
	int pri;

	ctd = curthread;
	pri = td->td_priority;
	cpri = ctd->td_priority;
	if (td->td_priority < ctd->td_priority)
		curthread->td_flags |= TDF_NEEDRESCHED;
	if (panicstr != NULL || pri >= cpri || cold || TD_IS_INHIBITED(ctd))
		return;
	/*
	 * Always preempt IDLE threads.  Otherwise only if the preempting
	 * thread is an ithread.
	 */
	if (pri > preempt_thresh && cpri < PRI_MIN_IDLE)
		return;
	ctd->td_owepreempt = 1;
	return;
}

/*
 * Add a thread to a thread queue.  Initializes priority, slice, runq, and
 * add it to the appropriate queue.  This is the internal function called
 * when the tdq is predetermined.
 */
void
tdq_add(struct tdq *tdq, struct thread *td, int flags)
{
	struct td_sched *ts;
	int class;
#ifdef SMP
	int cpumask;
#endif

	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	KASSERT((td->td_inhibitors == 0),
	    ("sched_add: trying to run inhibited thread"));
	KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	    ("sched_add: bad thread state"));
	KASSERT(td->td_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));

	ts = td->td_sched;
	class = PRI_BASE(td->td_pri_class);
        TD_SET_RUNQ(td);
	if (ts->ts_slice == 0)
		ts->ts_slice = sched_slice;
	/*
	 * Pick the run queue based on priority.
	 */
	if (td->td_priority <= PRI_MAX_REALTIME)
		ts->ts_runq = &tdq->tdq_realtime;
	else if (td->td_priority <= PRI_MAX_TIMESHARE)
		ts->ts_runq = &tdq->tdq_timeshare;
	else
		ts->ts_runq = &tdq->tdq_idle;
#ifdef SMP
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
	if (td->td_priority < tdq->tdq_lowpri)
		tdq->tdq_lowpri = td->td_priority;
#endif
	tdq_runq_add(tdq, ts, flags);
	tdq_load_add(tdq, ts);
}

/*
 * Select the target thread queue and add a thread to it.  Request
 * preemption or IPI a remote processor if required.
 */
void
sched_add(struct thread *td, int flags)
{
	struct td_sched *ts;
	struct tdq *tdq;
#ifdef SMP
	int cpuid;
	int cpu;
#endif
	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	ts = td->td_sched;
	/*
	 * Recalculate the priority before we select the target cpu or
	 * run-queue.
	 */
	if (PRI_BASE(td->td_pri_class) == PRI_TIMESHARE)
		sched_priority(td);
#ifdef SMP
	cpuid = PCPU_GET(cpuid);
	/*
	 * Pick the destination cpu and if it isn't ours transfer to the
	 * target cpu.
	 */
	if (td->td_priority <= PRI_MAX_ITHD && THREAD_CAN_MIGRATE(td))
		cpu = cpuid;
	else if (!THREAD_CAN_MIGRATE(td))
		cpu = ts->ts_cpu;
	else
		cpu = sched_pickcpu(ts, flags);
	tdq = sched_setcpu(ts, cpu, flags);
	tdq_add(tdq, td, flags);
	if (cpu != cpuid) {
		tdq_notify(ts);
		return;
	}
#else
	tdq = TDQ_SELF();
	TDQ_LOCK(tdq);
	/*
	 * Now that the thread is moving to the run-queue, set the lock
	 * to the scheduler's lock.
	 */
	thread_lock_set(td, TDQ_LOCKPTR(tdq));
	tdq_add(tdq, td, flags);
#endif
	if (!(flags & SRQ_YIELDING))
		sched_setpreempt(td);
}

/*
 * Remove a thread from a run-queue without running it.  This is used
 * when we're stealing a thread from a remote queue.  Otherwise all threads
 * exit by calling sched_exit_thread() and sched_throw() themselves.
 */
void
sched_rem(struct thread *td)
{
	struct tdq *tdq;
	struct td_sched *ts;

	CTR5(KTR_SCHED, "sched_rem: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	ts = td->td_sched;
	tdq = TDQ_CPU(ts->ts_cpu);
	TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	MPASS(td->td_lock == TDQ_LOCKPTR(tdq));
	KASSERT(TD_ON_RUNQ(td),
	    ("sched_rem: thread not on run queue"));
	tdq_runq_rem(tdq, ts);
	tdq_load_rem(tdq, ts);
	TD_SET_CAN_RUN(td);
}

/*
 * Fetch cpu utilization information.  Updates on demand.
 */
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

/*
 * Bind a thread to a target cpu.
 */
void
sched_bind(struct thread *td, int cpu)
{
	struct td_sched *ts;

	THREAD_LOCK_ASSERT(td, MA_OWNED|MA_NOTRECURSED);
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

/*
 * Release a bound thread.
 */
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

/*
 * Basic yield call.
 */
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

/*
 * Return the total system load.
 */
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

/*
 * The actual idle process.
 */
void
sched_idletd(void *dummy)
{
	struct thread *td;
	struct tdq *tdq;

	td = curthread;
	tdq = TDQ_SELF();
	mtx_assert(&Giant, MA_NOTOWNED);
	/* ULE relies on preemption for idle interruption. */
	for (;;) {
#ifdef SMP
		if (tdq_idled(tdq))
			cpu_idle();
#else
		cpu_idle();
#endif
	}
}

/*
 * A CPU is entering for the first time or a thread is exiting.
 */
void
sched_throw(struct thread *td)
{
	struct tdq *tdq;

	tdq = TDQ_SELF();
	if (td == NULL) {
		/* Correct spinlock nesting and acquire the correct lock. */
		TDQ_LOCK(tdq);
		spinlock_exit();
	} else {
		MPASS(td->td_lock == TDQ_LOCKPTR(tdq));
		tdq_load_rem(tdq, td->td_sched);
	}
	KASSERT(curthread->td_md.md_spinlock_count == 1, ("invalid count"));
	PCPU_SET(switchtime, cpu_ticks());
	PCPU_SET(switchticks, ticks);
	cpu_throw(td, choosethread());	/* doesn't return */
}

/*
 * This is called from fork_exit().  Just acquire the correct locks and
 * let fork do the rest of the work.
 */
void
sched_fork_exit(struct thread *td)
{
	struct td_sched *ts;
	struct tdq *tdq;
	int cpuid;

	/*
	 * Finish setting up thread glue so that it begins execution in a
	 * non-nested critical section with the scheduler lock held.
	 */
	cpuid = PCPU_GET(cpuid);
	tdq = TDQ_CPU(cpuid);
	ts = td->td_sched;
	if (TD_IS_IDLETHREAD(td))
		td->td_lock = TDQ_LOCKPTR(tdq);
	MPASS(td->td_lock == TDQ_LOCKPTR(tdq));
	td->td_oncpu = cpuid;
	TDQ_LOCKPTR(tdq)->mtx_lock = (uintptr_t)td;
	THREAD_LOCK_ASSERT(td, MA_OWNED | MA_NOTRECURSED);
}

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0,
    "Scheduler");
SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "ULE", 0,
    "Scheduler name");
SYSCTL_INT(_kern_sched, OID_AUTO, slice, CTLFLAG_RW, &sched_slice, 0,
    "Slice size for timeshare threads");
SYSCTL_INT(_kern_sched, OID_AUTO, interact, CTLFLAG_RW, &sched_interact, 0,
     "Interactivity score threshold");
SYSCTL_INT(_kern_sched, OID_AUTO, preempt_thresh, CTLFLAG_RW, &preempt_thresh,
     0,"Min priority for preemption, lower priorities have greater precedence");
#ifdef SMP
SYSCTL_INT(_kern_sched, OID_AUTO, pick_pri, CTLFLAG_RW, &pick_pri, 0,
    "Pick the target cpu based on priority rather than load.");
SYSCTL_INT(_kern_sched, OID_AUTO, affinity, CTLFLAG_RW, &affinity, 0,
    "Number of hz ticks to keep thread affinity for");
SYSCTL_INT(_kern_sched, OID_AUTO, tryself, CTLFLAG_RW, &tryself, 0, "");
SYSCTL_INT(_kern_sched, OID_AUTO, balance, CTLFLAG_RW, &rebalance, 0,
    "Enables the long-term load balancer");
SYSCTL_INT(_kern_sched, OID_AUTO, balance_secs, CTLFLAG_RW, &balance_secs, 0,
    "Average frequence in seconds to run the long-term balancer");
SYSCTL_INT(_kern_sched, OID_AUTO, steal_htt, CTLFLAG_RW, &steal_htt, 0,
    "Steals work from another hyper-threaded core on idle");
SYSCTL_INT(_kern_sched, OID_AUTO, steal_idle, CTLFLAG_RW, &steal_idle, 0,
    "Attempts to steal work from other cores before idling");
SYSCTL_INT(_kern_sched, OID_AUTO, steal_thresh, CTLFLAG_RW, &steal_thresh, 0,
    "Minimum load on remote cpu before we'll steal");
SYSCTL_INT(_kern_sched, OID_AUTO, topology, CTLFLAG_RD, &topology, 0,
    "True when a topology has been specified by the MD code.");
#endif

/* ps compat */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");


#define KERN_SWITCH_INCLUDE 1
#include "kern/kern_switch.c"
