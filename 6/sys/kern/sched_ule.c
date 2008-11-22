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

#define kse td_sched

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

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0, "Scheduler");

SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "ule", 0,
    "Scheduler name");

static int slice_min = 1;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_min, CTLFLAG_RW, &slice_min, 0, "");

static int slice_max = 10;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_max, CTLFLAG_RW, &slice_max, 0, "");

int realstathz;
int tickincr = 1;

/*
 * The following datastructures are allocated within their parent structure
 * but are scheduler specific.
 */
/*
 * The schedulable entity that can be given a context to run.  A process may
 * have several of these.
 */
struct kse {
	TAILQ_ENTRY(kse) ke_procq;	/* (j/z) Run queue. */
	int		ke_flags;	/* (j) KEF_* flags. */
	struct thread	*ke_thread;	/* (*) Active associated thread. */
	fixpt_t		ke_pctcpu;	/* (j) %cpu during p_swtime. */
	char		ke_rqindex;	/* (j) Run queue index. */
	enum {
		KES_THREAD = 0x0,	/* slaved to thread state */
		KES_ONRUNQ
	} ke_state;			/* (j) thread sched specific status. */
	int		ke_slptime;
	int		ke_slice;
	struct runq	*ke_runq;
	u_char		ke_cpu;		/* CPU that we have affinity for. */
	/* The following variables are only used for pctcpu calculation */
	int		ke_ltick;	/* Last tick that we were running on */
	int		ke_ftick;	/* First tick that we were running on */
	int		ke_ticks;	/* Tick count */

};
#define	td_kse			td_sched
#define	td_slptime		td_kse->ke_slptime
#define ke_proc			ke_thread->td_proc
#define ke_ksegrp		ke_thread->td_ksegrp
#define	ke_assign		ke_procq.tqe_next
/* flags kept in ke_flags */
#define	KEF_ASSIGNED	0x0001		/* Thread is being migrated. */
#define	KEF_BOUND	0x0002		/* Thread can not migrate. */
#define	KEF_XFERABLE	0x0004		/* Thread was added as transferable. */
#define	KEF_HOLD	0x0008		/* Thread is temporarily bound. */
#define	KEF_REMOVED	0x0010		/* Thread was removed while ASSIGNED */
#define	KEF_INTERNAL	0x0020		/* Thread added due to migration. */
#define	KEF_PREEMPTED	0x0040		/* Thread was preempted */
#define	KEF_DIDRUN	0x02000		/* Thread actually ran. */
#define	KEF_EXIT	0x04000		/* Thread is being killed. */

struct kg_sched {
	struct thread	*skg_last_assigned; /* (j) Last thread assigned to */
					   /* the system scheduler */
	int	skg_slptime;		/* Number of ticks we vol. slept */
	int	skg_runtime;		/* Number of ticks we were running */
	int	skg_avail_opennings;	/* (j) Num unfilled slots in group.*/
	int	skg_concurrency;	/* (j) Num threads requested in group.*/
};
#define kg_last_assigned	kg_sched->skg_last_assigned
#define kg_avail_opennings	kg_sched->skg_avail_opennings
#define kg_concurrency		kg_sched->skg_concurrency
#define kg_runtime		kg_sched->skg_runtime
#define kg_slptime		kg_sched->skg_slptime

#define SLOT_RELEASE(kg)	(kg)->kg_avail_opennings++
#define	SLOT_USE(kg)		(kg)->kg_avail_opennings--

static struct kse kse0;
static struct kg_sched kg_sched0;

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
#define	SCHED_INTERACTIVE(kg)						\
    (sched_interact_score(kg) < SCHED_INTERACT_THRESH)
#define	SCHED_CURR(kg, ke)						\
    ((ke->ke_thread->td_flags & TDF_BORROWING) ||			\
     (ke->ke_flags & KEF_PREEMPTED) || SCHED_INTERACTIVE(kg))

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
struct kseq {
	struct runq	ksq_idle;		/* Queue of IDLE threads. */
	struct runq	ksq_timeshare[2];	/* Run queues for !IDLE. */
	struct runq	*ksq_next;		/* Next timeshare queue. */
	struct runq	*ksq_curr;		/* Current queue. */
	int		ksq_load_timeshare;	/* Load for timeshare. */
	int		ksq_load;		/* Aggregate load. */
	short		ksq_nice[SCHED_PRI_NRESV]; /* KSEs in each nice bin. */
	short		ksq_nicemin;		/* Least nice. */
#ifdef SMP
	int			ksq_transferable;
	LIST_ENTRY(kseq)	ksq_siblings;	/* Next in kseq group. */
	struct kseq_group	*ksq_group;	/* Our processor group. */
	volatile struct kse	*ksq_assigned;	/* assigned by another CPU. */
#else
	int		ksq_sysload;		/* For loadavg, !ITHD load. */
#endif
};

#ifdef SMP
/*
 * kseq groups are groups of processors which can cheaply share threads.  When
 * one processor in the group goes idle it will check the runqs of the other
 * processors in its group prior to halting and waiting for an interrupt.
 * These groups are suitable for SMT (Symetric Multi-Threading) and not NUMA.
 * In a numa environment we'd want an idle bitmap per group and a two tiered
 * load balancer.
 */
struct kseq_group {
	int	ksg_cpus;		/* Count of CPUs in this kseq group. */
	cpumask_t ksg_cpumask;		/* Mask of cpus in this group. */
	cpumask_t ksg_idlemask;		/* Idle cpus in this group. */
	cpumask_t ksg_mask;		/* Bit mask for first cpu. */
	int	ksg_load;		/* Total load of this group. */
	int	ksg_transferable;	/* Transferable load of this group. */
	LIST_HEAD(, kseq)	ksg_members; /* Linked list of all members. */
};
#endif

/*
 * One kse queue per processor.
 */
#ifdef SMP
static cpumask_t kseq_idle;
static int ksg_maxid;
static struct kseq	kseq_cpu[MAXCPU];
static struct kseq_group kseq_groups[MAXCPU];
static int bal_tick;
static int gbal_tick;
static int balance_groups;

#define	KSEQ_SELF()	(&kseq_cpu[PCPU_GET(cpuid)])
#define	KSEQ_CPU(x)	(&kseq_cpu[(x)])
#define	KSEQ_ID(x)	((x) - kseq_cpu)
#define	KSEQ_GROUP(x)	(&kseq_groups[(x)])
#else	/* !SMP */
static struct kseq	kseq_cpu;

#define	KSEQ_SELF()	(&kseq_cpu)
#define	KSEQ_CPU(x)	(&kseq_cpu)
#endif

static void slot_fill(struct ksegrp *);
static struct kse *sched_choose(void);		/* XXX Should be thread * */
static void sched_slice(struct kse *);
static void sched_priority(struct ksegrp *);
static void sched_thread_priority(struct thread *, u_char);
static int sched_interact_score(struct ksegrp *);
static void sched_interact_update(struct ksegrp *);
static void sched_interact_fork(struct ksegrp *);
static void sched_pctcpu_update(struct kse *);

/* Operations on per processor queues */
static struct kse * kseq_choose(struct kseq *);
static void kseq_setup(struct kseq *);
static void kseq_load_add(struct kseq *, struct kse *);
static void kseq_load_rem(struct kseq *, struct kse *);
static __inline void kseq_runq_add(struct kseq *, struct kse *, int);
static __inline void kseq_runq_rem(struct kseq *, struct kse *);
static void kseq_nice_add(struct kseq *, int);
static void kseq_nice_rem(struct kseq *, int);
void kseq_print(int cpu);
#ifdef SMP
static int kseq_transfer(struct kseq *, struct kse *, int);
static struct kse *runq_steal(struct runq *);
static void sched_balance(void);
static void sched_balance_groups(void);
static void sched_balance_group(struct kseq_group *);
static void sched_balance_pair(struct kseq *, struct kseq *);
static void kseq_move(struct kseq *, int);
static int kseq_idled(struct kseq *);
static void kseq_notify(struct kse *, int);
static void kseq_assign(struct kseq *);
static struct kse *kseq_steal(struct kseq *, int);
#define	KSE_CAN_MIGRATE(ke)						\
    ((ke)->ke_thread->td_pinned == 0 && ((ke)->ke_flags & KEF_BOUND) == 0)
#endif

void
kseq_print(int cpu)
{
	struct kseq *kseq;
	int i;

	kseq = KSEQ_CPU(cpu);

	printf("kseq:\n");
	printf("\tload:           %d\n", kseq->ksq_load);
	printf("\tload TIMESHARE: %d\n", kseq->ksq_load_timeshare);
#ifdef SMP
	printf("\tload transferable: %d\n", kseq->ksq_transferable);
#endif
	printf("\tnicemin:\t%d\n", kseq->ksq_nicemin);
	printf("\tnice counts:\n");
	for (i = 0; i < SCHED_PRI_NRESV; i++)
		if (kseq->ksq_nice[i])
			printf("\t\t%d = %d\n",
			    i - SCHED_PRI_NHALF, kseq->ksq_nice[i]);
}

static __inline void
kseq_runq_add(struct kseq *kseq, struct kse *ke, int flags)
{
#ifdef SMP
	if (KSE_CAN_MIGRATE(ke)) {
		kseq->ksq_transferable++;
		kseq->ksq_group->ksg_transferable++;
		ke->ke_flags |= KEF_XFERABLE;
	}
#endif
	if (ke->ke_flags & KEF_PREEMPTED)
		flags |= SRQ_PREEMPTED;
	runq_add(ke->ke_runq, ke, flags);
}

static __inline void
kseq_runq_rem(struct kseq *kseq, struct kse *ke)
{
#ifdef SMP
	if (ke->ke_flags & KEF_XFERABLE) {
		kseq->ksq_transferable--;
		kseq->ksq_group->ksg_transferable--;
		ke->ke_flags &= ~KEF_XFERABLE;
	}
#endif
	runq_remove(ke->ke_runq, ke);
}

static void
kseq_load_add(struct kseq *kseq, struct kse *ke)
{
	int class;
	mtx_assert(&sched_lock, MA_OWNED);
	class = PRI_BASE(ke->ke_ksegrp->kg_pri_class);
	if (class == PRI_TIMESHARE)
		kseq->ksq_load_timeshare++;
	kseq->ksq_load++;
	CTR1(KTR_SCHED, "load: %d", kseq->ksq_load);
	if (class != PRI_ITHD && (ke->ke_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		kseq->ksq_group->ksg_load++;
#else
		kseq->ksq_sysload++;
#endif
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
		kseq_nice_add(kseq, ke->ke_proc->p_nice);
}

static void
kseq_load_rem(struct kseq *kseq, struct kse *ke)
{
	int class;
	mtx_assert(&sched_lock, MA_OWNED);
	class = PRI_BASE(ke->ke_ksegrp->kg_pri_class);
	if (class == PRI_TIMESHARE)
		kseq->ksq_load_timeshare--;
	if (class != PRI_ITHD  && (ke->ke_proc->p_flag & P_NOLOAD) == 0)
#ifdef SMP
		kseq->ksq_group->ksg_load--;
#else
		kseq->ksq_sysload--;
#endif
	kseq->ksq_load--;
	CTR1(KTR_SCHED, "load: %d", kseq->ksq_load);
	ke->ke_runq = NULL;
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
		kseq_nice_rem(kseq, ke->ke_proc->p_nice);
}

static void
kseq_nice_add(struct kseq *kseq, int nice)
{
	mtx_assert(&sched_lock, MA_OWNED);
	/* Normalize to zero. */
	kseq->ksq_nice[nice + SCHED_PRI_NHALF]++;
	if (nice < kseq->ksq_nicemin || kseq->ksq_load_timeshare == 1)
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
	    kseq->ksq_load_timeshare == 0)
		return;

	for (; n < SCHED_PRI_NRESV; n++)
		if (kseq->ksq_nice[n]) {
			kseq->ksq_nicemin = n - SCHED_PRI_NHALF;
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
	struct kseq_group *high;
	struct kseq_group *low;
	struct kseq_group *ksg;
	int cnt;
	int i;

	bal_tick = ticks + (random() % (hz * 2));
	if (smp_started == 0)
		return;
	low = high = NULL;
	i = random() % (ksg_maxid + 1);
	for (cnt = 0; cnt <= ksg_maxid; cnt++) {
		ksg = KSEQ_GROUP(i);
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
			sched_balance_group(KSEQ_GROUP(i));
}

static void
sched_balance_group(struct kseq_group *ksg)
{
	struct kseq *kseq;
	struct kseq *high;
	struct kseq *low;
	int load;

	if (ksg->ksg_transferable == 0)
		return;
	low = NULL;
	high = NULL;
	LIST_FOREACH(kseq, &ksg->ksg_members, ksq_siblings) {
		load = kseq->ksq_load;
		if (high == NULL || load > high->ksq_load)
			high = kseq;
		if (low == NULL || load < low->ksq_load)
			low = kseq;
	}
	if (high != NULL && low != NULL && high != low)
		sched_balance_pair(high, low);
}

static void
sched_balance_pair(struct kseq *high, struct kseq *low)
{
	int transferable;
	int high_load;
	int low_load;
	int move;
	int diff;
	int i;

	/*
	 * If we're transfering within a group we have to use this specific
	 * kseq's transferable count, otherwise we can steal from other members
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
		kseq_move(high, KSEQ_ID(low));
	return;
}

static void
kseq_move(struct kseq *from, int cpu)
{
	struct kseq *kseq;
	struct kseq *to;
	struct kse *ke;

	kseq = from;
	to = KSEQ_CPU(cpu);
	ke = kseq_steal(kseq, 1);
	if (ke == NULL) {
		struct kseq_group *ksg;

		ksg = kseq->ksq_group;
		LIST_FOREACH(kseq, &ksg->ksg_members, ksq_siblings) {
			if (kseq == from || kseq->ksq_transferable == 0)
				continue;
			ke = kseq_steal(kseq, 1);
			break;
		}
		if (ke == NULL)
			panic("kseq_move: No KSEs available with a "
			    "transferable count of %d\n", 
			    ksg->ksg_transferable);
	}
	if (kseq == to)
		return;
	ke->ke_state = KES_THREAD;
	kseq_runq_rem(kseq, ke);
	kseq_load_rem(kseq, ke);
	kseq_notify(ke, cpu);
}

static int
kseq_idled(struct kseq *kseq)
{
	struct kseq_group *ksg;
	struct kseq *steal;
	struct kse *ke;

	ksg = kseq->ksq_group;
	/*
	 * If we're in a cpu group, try and steal kses from another cpu in
	 * the group before idling.
	 */
	if (ksg->ksg_cpus > 1 && ksg->ksg_transferable) {
		LIST_FOREACH(steal, &ksg->ksg_members, ksq_siblings) {
			if (steal == kseq || steal->ksq_transferable == 0)
				continue;
			ke = kseq_steal(steal, 0);
			if (ke == NULL)
				continue;
			ke->ke_state = KES_THREAD;
			kseq_runq_rem(steal, ke);
			kseq_load_rem(steal, ke);
			ke->ke_cpu = PCPU_GET(cpuid);
			ke->ke_flags |= KEF_INTERNAL | KEF_HOLD;
			sched_add(ke->ke_thread, SRQ_YIELDING);
			return (0);
		}
	}
	/*
	 * We only set the idled bit when all of the cpus in the group are
	 * idle.  Otherwise we could get into a situation where a KSE bounces
	 * back and forth between two idle cores on seperate physical CPUs.
	 */
	ksg->ksg_idlemask |= PCPU_GET(cpumask);
	if (ksg->ksg_idlemask != ksg->ksg_cpumask)
		return (1);
	atomic_set_int(&kseq_idle, ksg->ksg_mask);
	return (1);
}

static void
kseq_assign(struct kseq *kseq)
{
	struct kse *nke;
	struct kse *ke;

	do {
		*(volatile struct kse **)&ke = kseq->ksq_assigned;
	} while(!atomic_cmpset_ptr((volatile uintptr_t *)&kseq->ksq_assigned,
		(uintptr_t)ke, (uintptr_t)NULL));
	for (; ke != NULL; ke = nke) {
		nke = ke->ke_assign;
		kseq->ksq_group->ksg_load--;
		kseq->ksq_load--;
		ke->ke_flags &= ~KEF_ASSIGNED;
		if (ke->ke_flags & KEF_REMOVED) {
			ke->ke_flags &= ~KEF_REMOVED;
			continue;
		}
		ke->ke_flags |= KEF_INTERNAL | KEF_HOLD;
		sched_add(ke->ke_thread, SRQ_YIELDING);
	}
}

static void
kseq_notify(struct kse *ke, int cpu)
{
	struct kseq *kseq;
	struct thread *td;
	struct pcpu *pcpu;
	int class;
	int prio;

	kseq = KSEQ_CPU(cpu);
	/* XXX */
	class = PRI_BASE(ke->ke_ksegrp->kg_pri_class);
	if ((class == PRI_TIMESHARE || class == PRI_REALTIME) &&
	    (kseq_idle & kseq->ksq_group->ksg_mask)) 
		atomic_clear_int(&kseq_idle, kseq->ksq_group->ksg_mask);
	kseq->ksq_group->ksg_load++;
	kseq->ksq_load++;
	ke->ke_cpu = cpu;
	ke->ke_flags |= KEF_ASSIGNED;
	prio = ke->ke_thread->td_priority;

	/*
	 * Place a KSE on another cpu's queue and force a resched.
	 */
	do {
		*(volatile struct kse **)&ke->ke_assign = kseq->ksq_assigned;
	} while(!atomic_cmpset_ptr((volatile uintptr_t *)&kseq->ksq_assigned,
		(uintptr_t)ke->ke_assign, (uintptr_t)ke));
	/*
	 * Without sched_lock we could lose a race where we set NEEDRESCHED
	 * on a thread that is switched out before the IPI is delivered.  This
	 * would lead us to miss the resched.  This will be a problem once
	 * sched_lock is pushed down.
	 */
	pcpu = pcpu_find(cpu);
	td = pcpu->pc_curthread;
	if (ke->ke_thread->td_priority < td->td_priority ||
	    td == pcpu->pc_idlethread) {
		td->td_flags |= TDF_NEEDRESCHED;
		ipi_selected(1 << cpu, IPI_AST);
	}
}

static struct kse *
runq_steal(struct runq *rq)
{
	struct rqhead *rqh;
	struct rqbits *rqb;
	struct kse *ke;
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
			TAILQ_FOREACH(ke, rqh, ke_procq) {
				if (KSE_CAN_MIGRATE(ke))
					return (ke);
			}
		}
	}
	return (NULL);
}

static struct kse *
kseq_steal(struct kseq *kseq, int stealidle)
{
	struct kse *ke;

	/*
	 * Steal from next first to try to get a non-interactive task that
	 * may not have run for a while.
	 */
	if ((ke = runq_steal(kseq->ksq_next)) != NULL)
		return (ke);
	if ((ke = runq_steal(kseq->ksq_curr)) != NULL)
		return (ke);
	if (stealidle)
		return (runq_steal(&kseq->ksq_idle));
	return (NULL);
}

int
kseq_transfer(struct kseq *kseq, struct kse *ke, int class)
{
	struct kseq_group *nksg;
	struct kseq_group *ksg;
	struct kseq *old;
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
	old = KSEQ_CPU(ke->ke_cpu);
	nksg = old->ksq_group;
	ksg = kseq->ksq_group;
	if (kseq_idle) {
		if (kseq_idle & nksg->ksg_mask) {
			cpu = ffs(nksg->ksg_idlemask);
			if (cpu) {
				CTR2(KTR_SCHED,
				    "kseq_transfer: %p found old cpu %X " 
				    "in idlemask.", ke, cpu);
				goto migrate;
			}
		}
		/*
		 * Multiple cpus could find this bit simultaneously
		 * but the race shouldn't be terrible.
		 */
		cpu = ffs(kseq_idle);
		if (cpu) {
			CTR2(KTR_SCHED, "kseq_transfer: %p found %X " 
			    "in idlemask.", ke, cpu);
			goto migrate;
		}
	}
	idx = 0;
#if 0
	if (old->ksq_load < kseq->ksq_load) {
		cpu = ke->ke_cpu + 1;
		CTR2(KTR_SCHED, "kseq_transfer: %p old cpu %X " 
		    "load less than ours.", ke, cpu);
		goto migrate;
	}
	/*
	 * No new CPU was found, look for one with less load.
	 */
	for (idx = 0; idx <= ksg_maxid; idx++) {
		nksg = KSEQ_GROUP(idx);
		if (nksg->ksg_load /*+ (nksg->ksg_cpus  * 2)*/ < ksg->ksg_load) {
			cpu = ffs(nksg->ksg_cpumask);
			CTR2(KTR_SCHED, "kseq_transfer: %p cpu %X load less " 
			    "than ours.", ke, cpu);
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
			CTR2(KTR_SCHED, "kseq_transfer: %p cpu %X idle in " 
			    "group.", ke, cpu);
			goto migrate;
		}
	}
	return (0);
migrate:
	/*
	 * Now that we've found an idle CPU, migrate the thread.
	 */
	cpu--;
	ke->ke_runq = NULL;
	kseq_notify(ke, cpu);

	return (1);
}

#endif	/* SMP */

/*
 * Pick the highest priority task we have and return it.
 */

static struct kse *
kseq_choose(struct kseq *kseq)
{
	struct runq *swap;
	struct kse *ke;
	int nice;

	mtx_assert(&sched_lock, MA_OWNED);
	swap = NULL;

	for (;;) {
		ke = runq_choose(kseq->ksq_curr);
		if (ke == NULL) {
			/*
			 * We already swapped once and didn't get anywhere.
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
		nice = ke->ke_proc->p_nice + (0 - kseq->ksq_nicemin);
#if 0
		if (ke->ke_slice == 0 || (nice > SCHED_SLICE_NTHRESH &&
		    ke->ke_proc->p_nice != 0)) {
			runq_remove(ke->ke_runq, ke);
			sched_slice(ke);
			ke->ke_runq = kseq->ksq_next;
			runq_add(ke->ke_runq, ke, 0);
			continue;
		}
#endif
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
	kseq->ksq_load = 0;
	kseq->ksq_load_timeshare = 0;
}

static void
sched_setup(void *dummy)
{
#ifdef SMP
	int i;
#endif

	slice_min = (hz/100);	/* 10ms */
	slice_max = (hz/7);	/* ~140ms */

#ifdef SMP
	balance_groups = 0;
	/*
	 * Initialize the kseqs.
	 */
	for (i = 0; i < MAXCPU; i++) {
		struct kseq *ksq;

		ksq = &kseq_cpu[i];
		ksq->ksq_assigned = NULL;
		kseq_setup(&kseq_cpu[i]);
	}
	if (smp_topology == NULL) {
		struct kseq_group *ksg;
		struct kseq *ksq;
		int cpus;

		for (cpus = 0, i = 0; i < MAXCPU; i++) {
			if (CPU_ABSENT(i))
				continue;
			ksq = &kseq_cpu[cpus];
			ksg = &kseq_groups[cpus];
			/*
			 * Setup a kseq group with one member.
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
		struct kseq_group *ksg;
		struct cpu_group *cg;
		int j;

		for (i = 0; i < smp_topology->ct_count; i++) {
			cg = &smp_topology->ct_group[i];
			ksg = &kseq_groups[i];
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
					kseq_cpu[j].ksq_transferable = 0;
					kseq_cpu[j].ksq_group = ksg;
					LIST_INSERT_HEAD(&ksg->ksg_members,
					    &kseq_cpu[j], ksq_siblings);
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
	kseq_setup(KSEQ_SELF());
#endif
	mtx_lock_spin(&sched_lock);
	kseq_load_add(KSEQ_SELF(), &kse0);
	mtx_unlock_spin(&sched_lock);
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
	pri += kg->kg_proc->p_nice;

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

	if (ke->ke_thread->td_flags & TDF_BORROWING) {
		ke->ke_slice = SCHED_SLICE_MIN;
		return;
	}

	/*
	 * Rationale:
	 * KSEs in interactive ksegs get a minimal slice so that we
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
	 * If the kse is outside of the window it will get no slice
	 * and will be reevaluated each time it is selected on the
	 * run queue.  The exception to this is nice 0 ksegs when
	 * a nice -20 is running.  They are always granted a minimum
	 * slice.
	 */
	if (!SCHED_INTERACTIVE(kg)) {
		int nice;

		nice = kg->kg_proc->p_nice + (0 - kseq->ksq_nicemin);
		if (kseq->ksq_load_timeshare == 0 ||
		    kg->kg_proc->p_nice < kseq->ksq_nicemin)
			ke->ke_slice = SCHED_SLICE_MAX;
		else if (nice <= SCHED_SLICE_NTHRESH)
			ke->ke_slice = SCHED_SLICE_NICE(nice);
		else if (kg->kg_proc->p_nice == 0)
			ke->ke_slice = SCHED_SLICE_MIN;
		else
			ke->ke_slice = SCHED_SLICE_MIN; /* 0 */
	} else
		ke->ke_slice = SCHED_SLICE_INTERACTIVE;

	return;
}

/*
 * This routine enforces a maximum limit on the amount of scheduling history
 * kept.  It is called after either the slptime or runtime is adjusted.
 * This routine will not operate correctly when slp or run times have been
 * adjusted to more than double their maximum.
 */
static void
sched_interact_update(struct ksegrp *kg)
{
	int sum;

	sum = kg->kg_runtime + kg->kg_slptime;
	if (sum < SCHED_SLP_RUN_MAX)
		return;
	/*
	 * If we have exceeded by more than 1/5th then the algorithm below
	 * will not bring us back into range.  Dividing by two here forces
	 * us into the range of [4/5 * SCHED_INTERACT_MAX, SCHED_INTERACT_MAX]
	 */
	if (sum > (SCHED_SLP_RUN_MAX / 5) * 6) {
		kg->kg_runtime /= 2;
		kg->kg_slptime /= 2;
		return;
	}
	kg->kg_runtime = (kg->kg_runtime / 5) * 4;
	kg->kg_slptime = (kg->kg_slptime / 5) * 4;
}

static void
sched_interact_fork(struct ksegrp *kg)
{
	int ratio;
	int sum;

	sum = kg->kg_runtime + kg->kg_slptime;
	if (sum > SCHED_SLP_RUN_FORK) {
		ratio = sum / SCHED_SLP_RUN_FORK;
		kg->kg_runtime /= ratio;
		kg->kg_slptime /= ratio;
	}
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
	ksegrp0.kg_sched = &kg_sched0;
	thread0.td_sched = &kse0;
	kse0.ke_thread = &thread0;
	kse0.ke_state = KES_THREAD;
	kg_sched0.skg_concurrency = 1;
	kg_sched0.skg_avail_opennings = 0; /* we are already running */
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
sched_pctcpu_update(struct kse *ke)
{
	/*
	 * Adjust counters and watermark for pctcpu calc.
	 */
	if (ke->ke_ltick > ticks - SCHED_CPU_TICKS) {
		/*
		 * Shift the tick count out so that the divide doesn't
		 * round away our results.
		 */
		ke->ke_ticks <<= 10;
		ke->ke_ticks = (ke->ke_ticks / (ticks - ke->ke_ftick)) *
			    SCHED_CPU_TICKS;
		ke->ke_ticks >>= 10;
	} else
		ke->ke_ticks = 0;
	ke->ke_ltick = ticks;
	ke->ke_ftick = ke->ke_ltick - SCHED_CPU_TICKS;
}

void
sched_thread_priority(struct thread *td, u_char prio)
{
	struct kse *ke;

	CTR6(KTR_SCHED, "sched_prio: %p(%s) prio %d newprio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, prio, curthread,
	    curthread->td_proc->p_comm);
	ke = td->td_kse;
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
		if (prio < td->td_priority && ke->ke_runq != NULL &&
		    (ke->ke_flags & KEF_ASSIGNED) == 0 &&
		    ke->ke_runq != KSEQ_CPU(ke->ke_cpu)->ksq_curr) {
			runq_remove(ke->ke_runq, ke);
			ke->ke_runq = KSEQ_CPU(ke->ke_cpu)->ksq_curr;
			runq_add(ke->ke_runq, ke, 0);
		}
		/*
		 * Hold this kse on this cpu so that sched_prio() doesn't
		 * cause excessive migration.  We only want migration to
		 * happen as the result of a wakeup.
		 */
		ke->ke_flags |= KEF_HOLD;
		adjustrunqueue(td, prio);
		ke->ke_flags &= ~KEF_HOLD;
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
		base_pri = td->td_ksegrp->kg_user_pri;
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
sched_switch(struct thread *td, struct thread *newtd, int flags)
{
	struct kseq *ksq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);

	ke = td->td_kse;
	ksq = KSEQ_SELF();

	td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;

	/*
	 * If the KSE has been assigned it may be in the process of switching
	 * to the new cpu.  This is the case in sched_bind().
	 */
	if (td == PCPU_GET(idlethread)) {
		TD_SET_CAN_RUN(td);
	} else if ((ke->ke_flags & KEF_ASSIGNED) == 0) {
		/* We are ending our run so make our slot available again */
		SLOT_RELEASE(td->td_ksegrp);
		kseq_load_rem(ksq, ke);
		if (TD_IS_RUNNING(td)) {
			/*
			 * Don't allow the thread to migrate
			 * from a preemption.
			 */
			ke->ke_flags |= KEF_HOLD;
			setrunqueue(td, (flags & SW_PREEMPT) ?
			    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
			    SRQ_OURSELF|SRQ_YIELDING);
			ke->ke_flags &= ~KEF_HOLD;
		} else if ((td->td_proc->p_flag & P_HADTHREADS) &&
		    (newtd == NULL || newtd->td_ksegrp != td->td_ksegrp))
			/*
			 * We will not be on the run queue.
			 * So we must be sleeping or similar.
			 * Don't use the slot if we will need it 
			 * for newtd.
			 */
			slot_fill(td->td_ksegrp);
	}
	if (newtd != NULL) {
		/*
		 * If we bring in a thread account for it as if it had been
		 * added to the run queue and then chosen.
		 */
		newtd->td_kse->ke_flags |= KEF_DIDRUN;
		newtd->td_kse->ke_runq = ksq->ksq_curr;
		TD_SET_RUNNING(newtd);
		kseq_load_add(KSEQ_SELF(), newtd->td_kse);
		/*
		 * XXX When we preempt, we've already consumed a slot because
		 * we got here through sched_add().  However, newtd can come
		 * from thread_switchout() which can't SLOT_USE() because
		 * the SLOT code is scheduler dependent.  We must use the
		 * slot here otherwise.
		 */
		if ((flags & SW_PREEMPT) == 0)
			SLOT_USE(newtd->td_ksegrp);
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
	struct ksegrp *kg;
	struct kse *ke;
	struct thread *td;
	struct kseq *kseq;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	/*
	 * We need to adjust the nice counts for running KSEs.
	 */
	FOREACH_KSEGRP_IN_PROC(p, kg) {
		if (kg->kg_pri_class == PRI_TIMESHARE) {
			FOREACH_THREAD_IN_GROUP(kg, td) {
				ke = td->td_kse;
				if (ke->ke_runq == NULL)
					continue;
				kseq = KSEQ_CPU(ke->ke_cpu);
				kseq_nice_rem(kseq, p->p_nice);
				kseq_nice_add(kseq, nice);
			}
		}
	}
	p->p_nice = nice;
	FOREACH_KSEGRP_IN_PROC(p, kg) {
		sched_priority(kg);
		FOREACH_THREAD_IN_GROUP(kg, td)
			td->td_flags |= TDF_NEEDRESCHED;
	}
}

void
sched_sleep(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_slptime = ticks;
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
		hzticks = (ticks - td->td_slptime) << 10;
		if (hzticks >= SCHED_SLP_RUN_MAX) {
			kg->kg_slptime = SCHED_SLP_RUN_MAX;
			kg->kg_runtime = 1;
		} else {
			kg->kg_slptime += hzticks;
			sched_interact_update(kg);
		}
		sched_priority(kg);
		sched_slice(td->td_kse);
		td->td_slptime = 0;
	}
	setrunqueue(td, SRQ_BORING);
}

/*
 * Penalize the parent for creating a new child and initialize the child's
 * priority.
 */
void
sched_fork(struct thread *td, struct thread *childtd)
{

	mtx_assert(&sched_lock, MA_OWNED);

	sched_fork_ksegrp(td, childtd->td_ksegrp);
	sched_fork_thread(td, childtd);
}

void
sched_fork_ksegrp(struct thread *td, struct ksegrp *child)
{
	struct ksegrp *kg = td->td_ksegrp;
	mtx_assert(&sched_lock, MA_OWNED);

	child->kg_slptime = kg->kg_slptime;
	child->kg_runtime = kg->kg_runtime;
	child->kg_user_pri = kg->kg_user_pri;
	sched_interact_fork(child);
	kg->kg_runtime += tickincr << 10;
	sched_interact_update(kg);
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
	struct kse *ke;
	struct kse *ke2;

	sched_newthread(child);
	ke = td->td_kse;
	ke2 = child->td_kse;
	ke2->ke_slice = 1;	/* Attempt to quickly learn interactivity. */
	ke2->ke_cpu = ke->ke_cpu;
	ke2->ke_runq = NULL;

	/* Grab our parents cpu estimation information. */
	ke2->ke_ticks = ke->ke_ticks;
	ke2->ke_ltick = ke->ke_ltick;
	ke2->ke_ftick = ke->ke_ftick;
}

void
sched_class(struct ksegrp *kg, int class)
{
	struct kseq *kseq;
	struct kse *ke;
	struct thread *td;
	int nclass;
	int oclass;

	mtx_assert(&sched_lock, MA_OWNED);
	if (kg->kg_pri_class == class)
		return;

	nclass = PRI_BASE(class);
	oclass = PRI_BASE(kg->kg_pri_class);
	FOREACH_THREAD_IN_GROUP(kg, td) {
		ke = td->td_kse;
		if ((ke->ke_state != KES_ONRUNQ &&
		    ke->ke_state != KES_THREAD) || ke->ke_runq == NULL)
			continue;
		kseq = KSEQ_CPU(ke->ke_cpu);

#ifdef SMP
		/*
		 * On SMP if we're on the RUNQ we must adjust the transferable
		 * count because could be changing to or from an interrupt
		 * class.
		 */
		if (ke->ke_state == KES_ONRUNQ) {
			if (KSE_CAN_MIGRATE(ke)) {
				kseq->ksq_transferable--;
				kseq->ksq_group->ksg_transferable--;
			}
			if (KSE_CAN_MIGRATE(ke)) {
				kseq->ksq_transferable++;
				kseq->ksq_group->ksg_transferable++;
			}
		}
#endif
		if (oclass == PRI_TIMESHARE) {
			kseq->ksq_load_timeshare--;
			kseq_nice_rem(kseq, kg->kg_proc->p_nice);
		}
		if (nclass == PRI_TIMESHARE) {
			kseq->ksq_load_timeshare++;
			kseq_nice_add(kseq, kg->kg_proc->p_nice);
		}
	}

	kg->kg_pri_class = class;
}

/*
 * Return some of the child's priority and interactivity to the parent.
 */
void
sched_exit(struct proc *p, struct thread *childtd)
{
	mtx_assert(&sched_lock, MA_OWNED);
	sched_exit_ksegrp(FIRST_KSEGRP_IN_PROC(p), childtd);
	sched_exit_thread(NULL, childtd);
}

void
sched_exit_ksegrp(struct ksegrp *kg, struct thread *td)
{
	/* kg->kg_slptime += td->td_ksegrp->kg_slptime; */
	kg->kg_runtime += td->td_ksegrp->kg_runtime;
	sched_interact_update(kg);
}

void
sched_exit_thread(struct thread *td, struct thread *childtd)
{
	CTR3(KTR_SCHED, "sched_exit_thread: %p(%s) prio %d",
	    childtd, childtd->td_proc->p_comm, childtd->td_priority);
	kseq_load_rem(KSEQ_CPU(childtd->td_kse->ke_cpu), childtd->td_kse);
}

void
sched_clock(struct thread *td)
{
	struct kseq *kseq;
	struct ksegrp *kg;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	kseq = KSEQ_SELF();
#ifdef SMP
	if (ticks >= bal_tick)
		sched_balance();
	if (ticks >= gbal_tick && balance_groups)
		sched_balance_groups();
	/*
	 * We could have been assigned a non real-time thread without an
	 * IPI.
	 */
	if (kseq->ksq_assigned)
		kseq_assign(kseq);	/* Potentially sets NEEDRESCHED */
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

	ke = td->td_kse;
	kg = ke->ke_ksegrp;

	/* Adjust ticks for pctcpu */
	ke->ke_ticks++;
	ke->ke_ltick = ticks;

	/* Go up to one second beyond our max and then trim back down */
	if (ke->ke_ftick + SCHED_CPU_TICKS + hz < ke->ke_ltick)
		sched_pctcpu_update(ke);

	if (td->td_flags & TDF_IDLETD)
		return;
	/*
	 * We only do slicing code for TIMESHARE ksegrps.
	 */
	if (kg->kg_pri_class != PRI_TIMESHARE)
		return;
	/*
	 * We used a tick charge it to the ksegrp so that we can compute our
	 * interactivity.
	 */
	kg->kg_runtime += tickincr << 10;
	sched_interact_update(kg);

	/*
	 * We used up one time slice.
	 */
	if (--ke->ke_slice > 0)
		return;
	/*
	 * We're out of time, recompute priorities and requeue.
	 */
	kseq_load_rem(kseq, ke);
	sched_priority(kg);
	sched_slice(ke);
	if (SCHED_CURR(kg, ke))
		ke->ke_runq = kseq->ksq_curr;
	else
		ke->ke_runq = kseq->ksq_next;
	kseq_load_add(kseq, ke);
	td->td_flags |= TDF_NEEDRESCHED;
}

int
sched_runnable(void)
{
	struct kseq *kseq;
	int load;

	load = 1;

	kseq = KSEQ_SELF();
#ifdef SMP
	if (kseq->ksq_assigned) {
		mtx_lock_spin(&sched_lock);
		kseq_assign(kseq);
		mtx_unlock_spin(&sched_lock);
	}
#endif
	if ((curthread->td_flags & TDF_IDLETD) != 0) {
		if (kseq->ksq_load > 0)
			goto out;
	} else
		if (kseq->ksq_load - 1 > 0)
			goto out;
	load = 0;
out:
	return (load);
}

void
sched_userret(struct thread *td)
{
	struct ksegrp *kg;

	KASSERT((td->td_flags & TDF_BORROWING) == 0,
	    ("thread with borrowed priority returning to userland"));
	kg = td->td_ksegrp;	
	if (td->td_priority != kg->kg_user_pri) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = kg->kg_user_pri;
		td->td_base_pri = kg->kg_user_pri;
		mtx_unlock_spin(&sched_lock);
	}
}

struct kse *
sched_choose(void)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	kseq = KSEQ_SELF();
#ifdef SMP
restart:
	if (kseq->ksq_assigned)
		kseq_assign(kseq);
#endif
	ke = kseq_choose(kseq);
	if (ke) {
#ifdef SMP
		if (ke->ke_ksegrp->kg_pri_class == PRI_IDLE)
			if (kseq_idled(kseq) == 0)
				goto restart;
#endif
		kseq_runq_rem(kseq, ke);
		ke->ke_state = KES_THREAD;
		ke->ke_flags &= ~KEF_PREEMPTED;
		return (ke);
	}
#ifdef SMP
	if (kseq_idled(kseq) == 0)
		goto restart;
#endif
	return (NULL);
}

void
sched_add(struct thread *td, int flags)
{
	struct kseq *kseq;
	struct ksegrp *kg;
	struct kse *ke;
	int preemptive;
	int canmigrate;
	int class;

	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	kg = td->td_ksegrp;
	canmigrate = 1;
	preemptive = !(flags & SRQ_YIELDING);
	class = PRI_BASE(kg->kg_pri_class);
	kseq = KSEQ_SELF();
	if ((ke->ke_flags & KEF_INTERNAL) == 0)
		SLOT_USE(td->td_ksegrp);
	ke->ke_flags &= ~KEF_INTERNAL;
#ifdef SMP
	if (ke->ke_flags & KEF_ASSIGNED) {
		if (ke->ke_flags & KEF_REMOVED)
			ke->ke_flags &= ~KEF_REMOVED;
		return;
	}
	canmigrate = KSE_CAN_MIGRATE(ke);
	/*
	 * Don't migrate running threads here.  Force the long term balancer
	 * to do it.
	 */
	if (ke->ke_flags & KEF_HOLD) {
		ke->ke_flags &= ~KEF_HOLD;
		canmigrate = 0;
	}
#endif
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	KASSERT(ke->ke_runq == NULL,
	    ("sched_add: KSE %p is still assigned to a run queue", ke));
	if (flags & SRQ_PREEMPTED)
		ke->ke_flags |= KEF_PREEMPTED;
	switch (class) {
	case PRI_ITHD:
	case PRI_REALTIME:
		ke->ke_runq = kseq->ksq_curr;
		ke->ke_slice = SCHED_SLICE_MAX;
		if (canmigrate)
			ke->ke_cpu = PCPU_GET(cpuid);
		break;
	case PRI_TIMESHARE:
		if (SCHED_CURR(kg, ke))
			ke->ke_runq = kseq->ksq_curr;
		else
			ke->ke_runq = kseq->ksq_next;
		break;
	case PRI_IDLE:
		/*
		 * This is for priority prop.
		 */
		if (ke->ke_thread->td_priority < PRI_MIN_IDLE)
			ke->ke_runq = kseq->ksq_curr;
		else
			ke->ke_runq = &kseq->ksq_idle;
		ke->ke_slice = SCHED_SLICE_MIN;
		break;
	default:
		panic("Unknown pri class.");
		break;
	}
#ifdef SMP
	/*
	 * If this thread is pinned or bound, notify the target cpu.
	 */
	if (!canmigrate && ke->ke_cpu != PCPU_GET(cpuid) ) {
		ke->ke_runq = NULL;
		kseq_notify(ke, ke->ke_cpu);
		return;
	}
	/*
	 * If we had been idle, clear our bit in the group and potentially
	 * the global bitmap.  If not, see if we should transfer this thread.
	 */
	if ((class == PRI_TIMESHARE || class == PRI_REALTIME) &&
	    (kseq->ksq_group->ksg_idlemask & PCPU_GET(cpumask)) != 0) {
		/*
		 * Check to see if our group is unidling, and if so, remove it
		 * from the global idle mask.
		 */
		if (kseq->ksq_group->ksg_idlemask ==
		    kseq->ksq_group->ksg_cpumask)
			atomic_clear_int(&kseq_idle, kseq->ksq_group->ksg_mask);
		/*
		 * Now remove ourselves from the group specific idle mask.
		 */
		kseq->ksq_group->ksg_idlemask &= ~PCPU_GET(cpumask);
	} else if (canmigrate && kseq->ksq_load > 1 && class != PRI_ITHD)
		if (kseq_transfer(kseq, ke, class))
			return;
	ke->ke_cpu = PCPU_GET(cpuid);
#endif
	if (td->td_priority < curthread->td_priority &&
	    ke->ke_runq == kseq->ksq_curr)
		curthread->td_flags |= TDF_NEEDRESCHED;
	if (preemptive && maybe_preempt(td))
		return;
	ke->ke_state = KES_ONRUNQ;

	kseq_runq_add(kseq, ke, flags);
	kseq_load_add(kseq, ke);
}

void
sched_rem(struct thread *td)
{
	struct kseq *kseq;
	struct kse *ke;

	CTR5(KTR_SCHED, "sched_rem: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	SLOT_RELEASE(td->td_ksegrp);
	ke->ke_flags &= ~KEF_PREEMPTED;
	if (ke->ke_flags & KEF_ASSIGNED) {
		ke->ke_flags |= KEF_REMOVED;
		return;
	}
	KASSERT((ke->ke_state == KES_ONRUNQ),
	    ("sched_rem: KSE not on run queue"));

	ke->ke_state = KES_THREAD;
	kseq = KSEQ_CPU(ke->ke_cpu);
	kseq_runq_rem(kseq, ke);
	kseq_load_rem(kseq, ke);
}

fixpt_t
sched_pctcpu(struct thread *td)
{
	fixpt_t pctcpu;
	struct kse *ke;

	pctcpu = 0;
	ke = td->td_kse;
	if (ke == NULL)
		return (0);

	mtx_lock_spin(&sched_lock);
	if (ke->ke_ticks) {
		int rtick;

		/*
		 * Don't update more frequently than twice a second.  Allowing
		 * this causes the cpu usage to decay away too quickly due to
		 * rounding errors.
		 */
		if (ke->ke_ftick + SCHED_CPU_TICKS < ke->ke_ltick ||
		    ke->ke_ltick < (ticks - (hz / 2)))
			sched_pctcpu_update(ke);
		/* How many rtick per second ? */
		rtick = min(ke->ke_ticks / SCHED_CPU_TIME, SCHED_CPU_TICKS);
		pctcpu = (FSCALE * ((FSCALE * rtick)/realstathz)) >> FSHIFT;
	}

	ke->ke_proc->p_swtime = ke->ke_ltick - ke->ke_ftick;
	mtx_unlock_spin(&sched_lock);

	return (pctcpu);
}

void
sched_bind(struct thread *td, int cpu)
{
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	ke->ke_flags |= KEF_BOUND;
#ifdef SMP
	if (PCPU_GET(cpuid) == cpu)
		return;
	/* sched_rem without the runq_remove */
	ke->ke_state = KES_THREAD;
	kseq_load_rem(KSEQ_CPU(ke->ke_cpu), ke);
	kseq_notify(ke, cpu);
	/* When we return from mi_switch we'll be on the correct cpu. */
	mi_switch(SW_VOL, NULL);
#endif
}

void
sched_unbind(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);
	td->td_kse->ke_flags &= ~KEF_BOUND;
}

int
sched_is_bound(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);
	return (td->td_kse->ke_flags & KEF_BOUND);
}

int
sched_load(void)
{
#ifdef SMP
	int total;
	int i;

	total = 0;
	for (i = 0; i <= ksg_maxid; i++)
		total += KSEQ_GROUP(i)->ksg_load;
	return (total);
#else
	return (KSEQ_SELF()->ksq_sysload);
#endif
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
#define KERN_SWITCH_INCLUDE 1
#include "kern/kern_switch.c"
