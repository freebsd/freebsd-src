/*-
 * Copyright (c) 2005-2006, David Xu <yfxu@corp.netease.com>
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
#include <sys/kthread.h>
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
#include <sys/unistd.h>
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

/* get process's nice value, skip value 20 which is not supported */
#define	PROC_NICE(p)		MIN((p)->p_nice, 19)

/* convert nice to kernel thread priority */
#define	NICE_TO_PRI(nice)	(PUSER + 20 + (nice))

/* get process's static priority */
#define	PROC_PRI(p)		NICE_TO_PRI(PROC_NICE(p))

/* convert kernel thread priority to user priority */
#define	USER_PRI(pri)		MIN((pri) - PUSER, 39)

/* convert nice value to user priority */
#define	PROC_USER_PRI(p)	(PROC_NICE(p) + 20)

/* maximum user priority, highest prio + 1 */
#define	MAX_USER_PRI		40

/* maximum kernel priority its nice is 19 */
#define PUSER_MAX		(PUSER + 39)

/* ticks and nanosecond converters */
#define	NS_TO_HZ(n)		((n) / (1000000000 / hz))
#define	HZ_TO_NS(h)		((h) * (1000000000 / hz))

/* ticks and microsecond converters */
#define MS_TO_HZ(m)		((m) / (1000000 / hz))

#define	PRI_SCORE_RATIO		25
#define	MAX_SCORE		(MAX_USER_PRI * PRI_SCORE_RATIO / 100)
#define	MAX_SLEEP_TIME		(def_timeslice * MAX_SCORE)
#define	NS_MAX_SLEEP_TIME	(HZ_TO_NS(MAX_SLEEP_TIME))
#define	STARVATION_TIME		(MAX_SLEEP_TIME)

#define	CURRENT_SCORE(kg)	\
   (MAX_SCORE * NS_TO_HZ((kg)->kg_slptime) / MAX_SLEEP_TIME)

#define	SCALE_USER_PRI(x, upri)	\
    MAX(x * (upri + 1) / (MAX_USER_PRI/2), min_timeslice)

/*
 * For a thread whose nice is zero, the score is used to determine
 * if it is an interactive thread.
 */
#define	INTERACTIVE_BASE_SCORE	(MAX_SCORE * 20)/100

/*
 * Calculate a score which a thread must have to prove itself is
 * an interactive thread.
 */
#define	INTERACTIVE_SCORE(ke)		\
    (PROC_NICE((ke)->ke_proc) * MAX_SCORE / 40 + INTERACTIVE_BASE_SCORE)

/* Test if a thread is an interactive thread */
#define	THREAD_IS_INTERACTIVE(ke)	\
    ((ke)->ke_ksegrp->kg_user_pri <=	\
	PROC_PRI((ke)->ke_proc) - INTERACTIVE_SCORE(ke))

/*
 * Calculate how long a thread must sleep to prove itself is an
 * interactive sleep.
 */
#define	INTERACTIVE_SLEEP_TIME(ke)	\
    (HZ_TO_NS(MAX_SLEEP_TIME *		\
	(MAX_SCORE / 2 + INTERACTIVE_SCORE((ke)) + 1) / MAX_SCORE - 1))

#define	CHILD_WEIGHT	90
#define	PARENT_WEIGHT	90
#define	EXIT_WEIGHT	3

#define	SCHED_LOAD_SCALE	128UL

#define	IDLE		0
#define IDLE_IDLE	1
#define NOT_IDLE	2

#define KQB_LEN		(8)		/* Number of priority status words. */
#define KQB_L2BPW	(5)		/* Log2(sizeof(rqb_word_t) * NBBY)). */
#define KQB_BPW		(1<<KQB_L2BPW)	/* Bits in an rqb_word_t. */

#define KQB_BIT(pri)	(1 << ((pri) & (KQB_BPW - 1)))
#define KQB_WORD(pri)	((pri) >> KQB_L2BPW)
#define KQB_FFS(word)	(ffs(word) - 1)

#define	KQ_NQS		256

/*
 * Type of run queue status word.
 */
typedef u_int32_t	kqb_word_t;

/*
 * Head of run queues.
 */
TAILQ_HEAD(krqhead, kse);

/*
 * Bit array which maintains the status of a run queue.  When a queue is
 * non-empty the bit corresponding to the queue number will be set.
 */
struct krqbits {
	kqb_word_t	rqb_bits[KQB_LEN];
};

/*
 * Run queue structure. Contains an array of run queues on which processes
 * are placed, and a structure to maintain the status of each queue.
 */
struct krunq {
	struct krqbits	rq_status;
	struct krqhead	rq_queues[KQ_NQS];
};

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
	u_char		ke_rqindex;	/* (j) Run queue index. */
	enum {
		KES_THREAD = 0x0,	/* slaved to thread state */
		KES_ONRUNQ
	} ke_state;			/* (j) thread sched specific status. */
	int		ke_slice;
	struct krunq	*ke_runq;
	int		ke_cpu;		/* CPU that we have affinity for. */
	int		ke_activated;
	uint64_t	ke_timestamp;
	uint64_t	ke_lastran;
#ifdef SMP
	int		ke_tocpu;
#endif
	/* The following variables are only used for pctcpu calculation */
	int		ke_ltick;	/* Last tick that we were running on */
	int		ke_ftick;	/* First tick that we were running on */
	int		ke_ticks;	/* Tick count */
};

#define	td_kse			td_sched
#define ke_proc			ke_thread->td_proc
#define ke_ksegrp		ke_thread->td_ksegrp

/* flags kept in ke_flags */
#define	KEF_ASSIGNED	0x0001		/* Thread is being migrated. */
#define	KEF_BOUND	0x0002		/* Thread can not migrate. */
#define	KEF_XFERABLE	0x0004		/* Thread was added as transferable. */
#define	KEF_HOLD	0x0008		/* Thread is temporarily bound. */
#define	KEF_REMOVED	0x0010		/* Thread was removed while ASSIGNED */
#define	KEF_INTERNAL	0x0020		/* Thread added due to migration. */
#define	KEF_PREEMPTED	0x0040		/* Thread was preempted. */
#define KEF_MIGRATING	0x0080		/* Thread is migrating. */
#define	KEF_SLEEP	0x0100		/* Thread did sleep. */
#define	KEF_DIDRUN	0x2000		/* Thread actually ran. */
#define	KEF_EXIT	0x4000		/* Thread is being killed. */
#define KEF_NEXTRQ	0x8000		/* Thread should be in next queue. */
#define KEF_FIRST_SLICE	0x10000		/* Thread has first time slice left. */

struct kg_sched {
	struct thread	*skg_last_assigned; /* (j) Last thread assigned to */
					    /* the system scheduler */
	u_long	skg_slptime;		/* (j) Number of ticks we vol. slept */
	u_long	skg_runtime;		/* (j) Temp total run time. */
	int	skg_avail_opennings;	/* (j) Num unfilled slots in group.*/
	int	skg_concurrency;	/* (j) Num threads requested in group.*/
};
#define kg_last_assigned	kg_sched->skg_last_assigned
#define kg_avail_opennings	kg_sched->skg_avail_opennings
#define kg_concurrency		kg_sched->skg_concurrency
#define kg_slptime		kg_sched->skg_slptime
#define kg_runtime		kg_sched->skg_runtime

#define SLOT_RELEASE(kg)	(kg)->kg_avail_opennings++
#define	SLOT_USE(kg)		(kg)->kg_avail_opennings--

/*
 * Cpu percentage computation macros and defines.
 *
 * SCHED_CPU_TIME:	Number of seconds to average the cpu usage across.
 * SCHED_CPU_TICKS:	Number of hz ticks to average the cpu usage across.
 */

#define	SCHED_CPU_TIME		10
#define	SCHED_CPU_TICKS		(hz * SCHED_CPU_TIME)

/*
 * kseq - per processor runqs and statistics.
 */
struct kseq {
	struct krunq	ksq_idle;		/* Queue of IDLE threads. */
	struct krunq	ksq_timeshare[2];	/* Run queues for !IDLE. */
	struct krunq	*ksq_next;		/* Next timeshare queue. */
	struct krunq	*ksq_curr;		/* Current queue. */
	int		ksq_load_timeshare;	/* Load for timeshare. */
	int		ksq_load_idle;
	int		ksq_load;		/* Aggregate load. */
	int		ksq_sysload;		/* For loadavg, !P_NOLOAD */
	uint64_t	ksq_expired_timestamp;
	uint64_t	ksq_last_timestamp;
	signed char	ksq_best_expired_nice;
#ifdef SMP
	int			ksq_transferable;
	LIST_ENTRY(kseq)	ksq_siblings;	/* Next in kseq group. */
	struct kseq_group	*ksq_group;	/* Our processor group. */
	struct thread		*ksq_migrated;
	TAILQ_HEAD(,kse)	ksq_migrateq;
	int			ksq_avgload;
#endif
};

#ifdef SMP
/*
 * kseq groups are groups of processors which can cheaply share threads. When
 * one processor in the group goes idle it will check the runqs of the other
 * processors in its group prior to halting and waiting for an interrupt.
 * These groups are suitable for SMT (Symetric Multi-Threading) and not NUMA.
 * In a NUMA environment we'd want an idle bitmap per group and a two tiered
 * load balancer.
 */
struct kseq_group {
	int		ksg_cpus;	/* Count of CPUs in this kseq group. */
	cpumask_t	ksg_cpumask;	/* Mask of cpus in this group. */
	cpumask_t	ksg_idlemask;	/* Idle cpus in this group. */
	cpumask_t	ksg_mask;	/* Bit mask for first cpu. */
	int		ksg_transferable;	/* Transferable load of this group. */
	LIST_HEAD(, kseq)	ksg_members;	/* Linked list of all members. */
	int		ksg_balance_tick;
};
#endif

static struct kse kse0;
static struct kg_sched kg_sched0;

static int min_timeslice = 5;
static int def_timeslice = 100;
static int granularity = 10;
static int realstathz;

/*
 * One kse queue per processor.
 */
#ifdef SMP
static cpumask_t kseq_idle;
static int ksg_maxid;
static struct kseq kseq_cpu[MAXCPU];
static struct kseq_group kseq_groups[MAXCPU];
static int balance_tick;
static int balance_interval = 1;
static int balance_interval_max = 32;
static int balance_interval_min = 8;
static int balance_busy_factor = 32;
static int imbalance_pct = 25;
static int imbalance_pct2 = 50;
static int ignore_topology = 1;

#define	KSEQ_SELF()	(&kseq_cpu[PCPU_GET(cpuid)])
#define	KSEQ_CPU(x)	(&kseq_cpu[(x)])
#define	KSEQ_ID(x)	((x) - kseq_cpu)
#define	KSEQ_GROUP(x)	(&kseq_groups[(x)])
#else	/* !SMP */
static struct kseq	kseq_cpu;

#define	KSEQ_SELF()	(&kseq_cpu)
#define	KSEQ_CPU(x)	(&kseq_cpu)
#endif

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL);

static void sched_initticks(void *dummy);
SYSINIT(sched_initticks, SI_SUB_CLOCKS, SI_ORDER_THIRD, sched_initticks, NULL)

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0, "Scheduler");

SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "core", 0,
    "Scheduler name");

#ifdef SMP
SYSCTL_INT(_kern_sched, OID_AUTO, imbalance_pct, CTLFLAG_RW,
    &imbalance_pct, 0, "");

SYSCTL_INT(_kern_sched, OID_AUTO, imbalance_pct2, CTLFLAG_RW,
    &imbalance_pct2, 0, "");

SYSCTL_INT(_kern_sched, OID_AUTO, balance_interval_min, CTLFLAG_RW,
    &balance_interval_min, 0, "");

SYSCTL_INT(_kern_sched, OID_AUTO, balance_interval_max, CTLFLAG_RW,
    &balance_interval_max, 0, "");
#endif

static void slot_fill(struct ksegrp *);

static void krunq_add(struct krunq *, struct kse *, int flags);
static struct kse *krunq_choose(struct krunq *);
static void krunq_clrbit(struct krunq *rq, int pri);
static int krunq_findbit(struct krunq *rq);
static void krunq_init(struct krunq *);
static void krunq_remove(struct krunq *, struct kse *);
#ifdef SMP
static struct kse *krunq_steal(struct krunq *rq, int my_cpu);
#endif

static struct kse * kseq_choose(struct kseq *);
static void kseq_load_add(struct kseq *, struct kse *);
static void kseq_load_rem(struct kseq *, struct kse *);
static void kseq_runq_add(struct kseq *, struct kse *, int);
static void kseq_runq_rem(struct kseq *, struct kse *);
static void kseq_setup(struct kseq *);

static int sched_is_timeshare(struct ksegrp *kg);
static struct kse *sched_choose(void);
static int sched_calc_pri(struct ksegrp *kg);
static int sched_starving(struct kseq *, uint64_t, struct kse *);
static void sched_pctcpu_update(struct kse *);
static void sched_thread_priority(struct thread *, u_char);
static uint64_t	sched_timestamp(void);
static int sched_recalc_pri(struct kse *ke, uint64_t now);
static int sched_timeslice(struct kse *ke);
static void sched_update_runtime(struct kse *ke, uint64_t now);
static void sched_commit_runtime(struct kse *ke);

#ifdef SMP
static void sched_balance_tick(int my_cpu, int idle);
static int sched_balance_idle(int my_cpu, int idle);
static int sched_balance(int my_cpu, int idle);
struct kseq_group *sched_find_busiest_group(int my_cpu, int idle,
	int *imbalance);
static struct kseq *sched_find_busiest_queue(struct kseq_group *ksg);
static int sched_find_idlest_cpu(struct kse *ke, int cpu);
static int sched_pull_threads(struct kseq *high, struct kseq *myksq,
	int max_move, int idle);
static int sched_pull_one(struct kseq *from, struct kseq *myksq, int idle);
static struct kse *sched_steal(struct kseq *, int my_cpu, int stealidle);
static int sched_idled(struct kseq *, int idle);
static int sched_find_idle_cpu(int defcpu);
static void migrated_setup(void *dummy);
static void migrated(void *dummy);
SYSINIT(migrated_setup, SI_SUB_KTHREAD_IDLE, SI_ORDER_MIDDLE, migrated_setup,
	NULL);

#endif /* SMP */

static inline int
kse_pinned(struct kse *ke)
{
	if (ke->ke_thread->td_pinned)
		return (1);

	if (ke->ke_flags & KEF_BOUND)
		return (1);

	return (0);
}

#ifdef SMP
static inline int
kse_can_migrate(struct kse *ke)
{
	if (kse_pinned(ke))
		return (0);
	return (1);
}
#endif

/*
 * Initialize a run structure.
 */
static void
krunq_init(struct krunq *rq)
{
	int i;

	bzero(rq, sizeof *rq);
	for (i = 0; i < KQ_NQS; i++)
		TAILQ_INIT(&rq->rq_queues[i]);
}

/*
 * Clear the status bit of the queue corresponding to priority level pri,
 * indicating that it is empty.
 */
static inline void
krunq_clrbit(struct krunq *rq, int pri)
{
	struct krqbits *rqb;

	rqb = &rq->rq_status;
	rqb->rqb_bits[KQB_WORD(pri)] &= ~KQB_BIT(pri);
}

/*
 * Find the index of the first non-empty run queue.  This is done by
 * scanning the status bits, a set bit indicates a non-empty queue.
 */
static int
krunq_findbit(struct krunq *rq)
{
	struct krqbits *rqb;
	int pri;
	int i;

	rqb = &rq->rq_status;
	for (i = 0; i < KQB_LEN; i++) {
		if (rqb->rqb_bits[i]) {
			pri = KQB_FFS(rqb->rqb_bits[i]) + (i << KQB_L2BPW);
			return (pri);
		}
	}
	return (-1);
}

/*
 * Set the status bit of the queue corresponding to priority level pri,
 * indicating that it is non-empty.
 */
static inline void
krunq_setbit(struct krunq *rq, int pri)
{
	struct krqbits *rqb;

	rqb = &rq->rq_status;
	rqb->rqb_bits[KQB_WORD(pri)] |= KQB_BIT(pri);
}

/*
 * Add the KSE to the queue specified by its priority, and set the
 * corresponding status bit.
 */
static void
krunq_add(struct krunq *rq, struct kse *ke, int flags)
{
	struct krqhead *rqh;
	int pri;

	pri = ke->ke_thread->td_priority;
	ke->ke_rqindex = pri;
	krunq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	if (flags & SRQ_PREEMPTED)
		TAILQ_INSERT_HEAD(rqh, ke, ke_procq);
	else
		TAILQ_INSERT_TAIL(rqh, ke, ke_procq);
}

/*
 * Find the highest priority process on the run queue.
 */
static struct kse *
krunq_choose(struct krunq *rq)
{
	struct krqhead *rqh;
	struct kse *ke;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	if ((pri = krunq_findbit(rq)) != -1) {
		rqh = &rq->rq_queues[pri];
		ke = TAILQ_FIRST(rqh);
		KASSERT(ke != NULL, ("runq_choose: no proc on busy queue"));
		return (ke);
	}

	return (NULL);
}

/*
 * Remove the KSE from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 * Caller must set ke->ke_state afterwards.
 */
static void
krunq_remove(struct krunq *rq, struct kse *ke)
{
	struct krqhead *rqh;
	int pri;

	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
		("runq_remove: process swapped out"));
	pri = ke->ke_rqindex;
	rqh = &rq->rq_queues[pri];
	KASSERT(ke != NULL, ("krunq_remove: no proc on busy queue"));
	TAILQ_REMOVE(rqh, ke, ke_procq);
	if (TAILQ_EMPTY(rqh))
		krunq_clrbit(rq, pri);
}

#ifdef SMP
static struct kse *
krunq_steal(struct krunq *rq, int my_cpu)
{
	struct krqhead *rqh;
	struct krqbits *rqb;
	struct kse *ke;
	kqb_word_t word;
	int i, bit;

	(void)my_cpu;

	mtx_assert(&sched_lock, MA_OWNED);
	rqb = &rq->rq_status;
	for (i = 0; i < KQB_LEN; i++) {
		if ((word = rqb->rqb_bits[i]) == 0)
			continue;
		do {
			bit = KQB_FFS(word);
			rqh = &rq->rq_queues[bit + (i << KQB_L2BPW)];
			TAILQ_FOREACH(ke, rqh, ke_procq) {
				if (kse_can_migrate(ke))
					return (ke);
			}
			word &= ~((kqb_word_t)1 << bit);
		} while (word != 0);
	}
	return (NULL);
}
#endif

static inline void
kseq_runq_add(struct kseq *kseq, struct kse *ke, int flags)
{
#ifdef SMP
	if (kse_pinned(ke) == 0) {
		kseq->ksq_transferable++;
		kseq->ksq_group->ksg_transferable++;
		ke->ke_flags |= KEF_XFERABLE;
	}
#endif
	if (ke->ke_flags & KEF_PREEMPTED)
		flags |= SRQ_PREEMPTED;
	krunq_add(ke->ke_runq, ke, flags);
}

static inline void
kseq_runq_rem(struct kseq *kseq, struct kse *ke)
{
#ifdef SMP
	if (ke->ke_flags & KEF_XFERABLE) {
		kseq->ksq_transferable--;
		kseq->ksq_group->ksg_transferable--;
		ke->ke_flags &= ~KEF_XFERABLE;
	}
#endif
	krunq_remove(ke->ke_runq, ke);
	ke->ke_runq = NULL;
}

static void
kseq_load_add(struct kseq *kseq, struct kse *ke)
{
	int class;

	mtx_assert(&sched_lock, MA_OWNED);
#ifdef SMP
	if (__predict_false(ke->ke_thread == kseq->ksq_migrated))
		return;
#endif
	class = PRI_BASE(ke->ke_ksegrp->kg_pri_class);
	if (class == PRI_TIMESHARE)
		kseq->ksq_load_timeshare++;
	else if (class == PRI_IDLE)
		kseq->ksq_load_idle++;
	kseq->ksq_load++;
	if ((ke->ke_proc->p_flag & P_NOLOAD) == 0)
		kseq->ksq_sysload++;
}

static void
kseq_load_rem(struct kseq *kseq, struct kse *ke)
{
	int class;

	mtx_assert(&sched_lock, MA_OWNED);
#ifdef SMP
	if (__predict_false(ke->ke_thread == kseq->ksq_migrated))
		return;
#endif
	class = PRI_BASE(ke->ke_ksegrp->kg_pri_class);
	if (class == PRI_TIMESHARE)
		kseq->ksq_load_timeshare--;
	else if (class == PRI_IDLE)
		kseq->ksq_load_idle--;
	kseq->ksq_load--;
	if ((ke->ke_proc->p_flag & P_NOLOAD) == 0)
		kseq->ksq_sysload--;
}

/*
 * Pick the highest priority task we have and return it.
 */

static struct kse *
kseq_choose(struct kseq *kseq)
{
	struct krunq *swap;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);

	ke = krunq_choose(kseq->ksq_curr);
	if (ke != NULL)
		return (ke);

	kseq->ksq_best_expired_nice = 21;
	kseq->ksq_expired_timestamp = 0;
	swap = kseq->ksq_curr;
	kseq->ksq_curr = kseq->ksq_next;
	kseq->ksq_next = swap;
	ke = krunq_choose(kseq->ksq_curr);
	if (ke != NULL)
		return (ke);

	return krunq_choose(&kseq->ksq_idle);
}

static inline uint64_t
sched_timestamp(void)
{
	uint64_t now = cputick2usec(cpu_ticks()) * 1000;
	return (now);
}

static inline int
sched_timeslice(struct kse *ke)
{
	struct proc *p = ke->ke_proc;

	if (ke->ke_proc->p_nice < 0)
		return SCALE_USER_PRI(def_timeslice*4, PROC_USER_PRI(p));
        else
		return SCALE_USER_PRI(def_timeslice,   PROC_USER_PRI(p));
}

static inline int
sched_is_timeshare(struct ksegrp *kg)
{
	/*
	 * XXX P_KTHREAD should be checked, but unfortunately, the
	 * readonly flag resides in a volatile member p_flag, reading
	 * it could cause lots of cache line sharing and invalidating.
	 */
	return (kg->kg_pri_class != PRI_TIMESHARE);
}

static int
sched_calc_pri(struct ksegrp *kg)
{
	int score, pri;

	if (__predict_false(!sched_is_timeshare(kg)))
		return (kg->kg_user_pri);
	score = CURRENT_SCORE(kg) - MAX_SCORE / 2;
	pri = PROC_PRI(kg->kg_proc) - score;
	if (pri < PUSER)
		pri = PUSER;
	if (pri > PUSER_MAX)
		pri = PUSER_MAX;
	return (pri);
}

static int
sched_recalc_pri(struct kse *ke, uint64_t now)
{
	uint64_t	delta;
	unsigned int	sleep_time;
	struct ksegrp	*kg;

	kg = ke->ke_ksegrp;
	delta = now - ke->ke_timestamp;
	if (__predict_false(!sched_is_timeshare(kg)))
		return (kg->kg_user_pri);

	if (delta > NS_MAX_SLEEP_TIME)
		sleep_time = NS_MAX_SLEEP_TIME;
	else
		sleep_time = (unsigned int)delta;
	if (__predict_false(sleep_time == 0))
		goto out;

	if (ke->ke_activated != -1 &&
	    sleep_time > INTERACTIVE_SLEEP_TIME(ke)) {
		kg->kg_slptime = HZ_TO_NS(MAX_SLEEP_TIME - def_timeslice);
	} else {
		sleep_time *= (MAX_SCORE - CURRENT_SCORE(kg)) ? : 1;

		/*
		 * If thread is waking from uninterruptible sleep, it is
		 * unlikely an interactive sleep, limit its sleep time to
		 * prevent it from being an interactive thread.
		 */
		if (ke->ke_activated == -1) {
			if (kg->kg_slptime >= INTERACTIVE_SLEEP_TIME(ke))
				sleep_time = 0;
			else if (kg->kg_slptime + sleep_time >=
				INTERACTIVE_SLEEP_TIME(ke)) {
				kg->kg_slptime = INTERACTIVE_SLEEP_TIME(ke);
				sleep_time = 0;
			}
		}

		/*
		 * Thread gets priority boost here.
                 */
		kg->kg_slptime += sleep_time;

		/* Sleep time should never be larger than maximum */
		if (kg->kg_slptime > NS_MAX_SLEEP_TIME)
			kg->kg_slptime = NS_MAX_SLEEP_TIME;
	}

out:
	return (sched_calc_pri(kg));
}

static void
sched_update_runtime(struct kse *ke, uint64_t now)
{
	uint64_t runtime;
	struct ksegrp *kg = ke->ke_ksegrp;

	if (sched_is_timeshare(kg)) {
		if ((int64_t)(now - ke->ke_timestamp) < NS_MAX_SLEEP_TIME) {
			runtime = now - ke->ke_timestamp;
			if ((int64_t)(now - ke->ke_timestamp) < 0)
				runtime = 0;
		} else {
			runtime = NS_MAX_SLEEP_TIME;
		}
		runtime /= (CURRENT_SCORE(kg) ? : 1);
		kg->kg_runtime += runtime;
		ke->ke_timestamp = now;
	}
}

static void
sched_commit_runtime(struct kse *ke)
{
	struct ksegrp *kg = ke->ke_ksegrp;

	if (kg->kg_runtime > kg->kg_slptime)
		kg->kg_slptime = 0;
	else
		kg->kg_slptime -= kg->kg_runtime;
	kg->kg_runtime = 0;
}

#ifdef SMP

/* staged balancing operations between CPUs */
#define CPU_OFFSET(cpu) (hz * cpu / MAXCPU)

static void
sched_balance_tick(int my_cpu, int idle)
{
	struct kseq *kseq = KSEQ_CPU(my_cpu);
	unsigned t = ticks + CPU_OFFSET(my_cpu);
	int old_load, cur_load;
	int interval;

	old_load = kseq->ksq_avgload;
	cur_load = kseq->ksq_load * SCHED_LOAD_SCALE;
	if (cur_load > old_load)
		old_load++;
	kseq->ksq_avgload = (old_load + cur_load) / 2;

	interval = balance_interval;
	if (idle == NOT_IDLE)
		interval *= balance_busy_factor;
	interval = MS_TO_HZ(interval);
	if (interval == 0)
		interval = 1;
	if (t - balance_tick >= interval) {
		sched_balance(my_cpu, idle);
		balance_tick += interval;
	}
}

static int
sched_balance(int my_cpu, int idle)
{
	struct kseq_group *high_group;
	struct kseq *high_queue;
	int imbalance, pulled;

	mtx_assert(&sched_lock, MA_OWNED);
	high_group = sched_find_busiest_group(my_cpu, idle, &imbalance);
	if (high_group == NULL)
		goto out;
	high_queue = sched_find_busiest_queue(high_group);
	if (high_queue == NULL)
		goto out;
	pulled = sched_pull_threads(high_queue, KSEQ_CPU(my_cpu), imbalance,
		idle);
	if (pulled == 0) {
		if (balance_interval < balance_interval_max)
			balance_interval++;
	} else {
		balance_interval = balance_interval_min;
	}
	return (pulled);
out:
	if (balance_interval < balance_interval_max)
		balance_interval *= 2;
	return (0);
}

static int
sched_balance_idle(int my_cpu, int idle)
{
	struct kseq_group *high_group;
	struct kseq *high_queue;
	int imbalance, pulled;

	mtx_assert(&sched_lock, MA_OWNED);
	high_group = sched_find_busiest_group(my_cpu, idle, &imbalance);
	if (high_group == NULL)
		return (0);
	high_queue = sched_find_busiest_queue(high_group);
	if (high_queue == NULL)
		return (0);
	pulled = sched_pull_threads(high_queue, KSEQ_CPU(my_cpu), imbalance,
		idle);
	return (pulled);
}

static inline int
kseq_source_load(struct kseq *ksq)
{
	int load = ksq->ksq_load * SCHED_LOAD_SCALE;
	return (MIN(ksq->ksq_avgload, load));
}

static inline int
kseq_dest_load(struct kseq *ksq)
{
	int load = ksq->ksq_load * SCHED_LOAD_SCALE;
	return (MAX(ksq->ksq_avgload, load));
}

struct kseq_group * 
sched_find_busiest_group(int my_cpu, int idle, int *imbalance)
{
	static unsigned stage_cpu;
	struct kseq_group *high;
	struct kseq_group *ksg;
	struct kseq *my_ksq, *ksq;
	int my_load, high_load, avg_load, total_load, load;
	int diff, cnt, i;

	*imbalance = 0;
	if (__predict_false(smp_started == 0))
		return (NULL);

	my_ksq = KSEQ_CPU(my_cpu);
	high = NULL;
	high_load = total_load = my_load = 0;
	i = (stage_cpu++) % (ksg_maxid + 1);
	for (cnt = 0; cnt <= ksg_maxid; cnt++) {
		ksg = KSEQ_GROUP(i);
		/*
		 * Find the CPU with the highest load that has some
		 * threads to transfer.
		 */
		load = 0;
		LIST_FOREACH(ksq, &ksg->ksg_members, ksq_siblings) {
			if (ksg == my_ksq->ksq_group)
				load += kseq_dest_load(ksq);
			else
				load += kseq_source_load(ksq);
		}
		if (ksg == my_ksq->ksq_group) {
			my_load = load;
		} else if (load > high_load && ksg->ksg_transferable) {
			high = ksg;
			high_load = load;
		}
		total_load += load;
		if (++i > ksg_maxid)
			i = 0;
	}

	avg_load = total_load / (ksg_maxid + 1);

	if (high == NULL)
		return (NULL);

	if (my_load >= avg_load ||
	    (high_load - my_load) * 100 < imbalance_pct * my_load) {
		if (idle == IDLE_IDLE ||
		    (idle == IDLE && high_load > SCHED_LOAD_SCALE)) {
			*imbalance = 1;
			return (high);
		} else {
			return (NULL);
		}
	}

	/*
	 * Pick a minimum imbalance value, avoid raising our load
	 * higher than average and pushing busiest load under average.
	 */
	diff = MIN(high_load - avg_load, avg_load - my_load);
	if (diff < SCHED_LOAD_SCALE) {
		if (high_load - my_load >= SCHED_LOAD_SCALE * 2) {
			*imbalance = 1;
			return (high);
		}
	}

	*imbalance = diff / SCHED_LOAD_SCALE;
	return (high);
}

static struct kseq *
sched_find_busiest_queue(struct kseq_group *ksg)
{
	struct kseq *kseq, *high = NULL;
	int load, high_load = 0;

	LIST_FOREACH(kseq, &ksg->ksg_members, ksq_siblings) {
		load = kseq_source_load(kseq);
		if (load > high_load) {
			high_load = load;
			high = kseq;
		}
	}

	return (high);
}

static int
sched_pull_threads(struct kseq *high, struct kseq *myksq, int max_pull,
	int idle)
{
	int pulled, i;

	mtx_assert(&sched_lock, MA_OWNED);
	pulled = 0;
	for (i = 0; i < max_pull; i++) {
		if (sched_pull_one(high, myksq, idle))
			pulled++;
		else
			break;
	}
	return (pulled);
}

static int
sched_pull_one(struct kseq *from, struct kseq *myksq, int idle)
{
	struct kseq *kseq;
	struct kse *ke;
	struct krunq *destq;
	int class;

	mtx_assert(&sched_lock, MA_OWNED);
	kseq = from;
	ke = sched_steal(kseq, KSEQ_ID(myksq), idle);
	if (ke == NULL) {
		/* doing balance in same group */
		if (from->ksq_group == myksq->ksq_group)
			return (0);

		struct kseq_group *ksg;

		ksg = kseq->ksq_group;
		LIST_FOREACH(kseq, &ksg->ksg_members, ksq_siblings) {
			if (kseq == from || kseq == myksq ||
			    kseq->ksq_transferable == 0)
				continue;
			ke = sched_steal(kseq, KSEQ_ID(myksq), idle);
			break;
		}
		if (ke == NULL)
			return (0);
	}
	ke->ke_timestamp = ke->ke_timestamp + myksq->ksq_last_timestamp -
		kseq->ksq_last_timestamp;
	ke->ke_lastran = 0;
	if (ke->ke_runq == from->ksq_curr)
		destq = myksq->ksq_curr;
	else if (ke->ke_runq == from->ksq_next)
		destq = myksq->ksq_next;
	else
		destq = &myksq->ksq_idle;
	kseq_runq_rem(kseq, ke);
	kseq_load_rem(kseq, ke);
	ke->ke_cpu = KSEQ_ID(myksq);
	ke->ke_runq = destq;
	ke->ke_state = KES_ONRUNQ;
	kseq_runq_add(myksq, ke, 0);
	kseq_load_add(myksq, ke);
	class = PRI_BASE(ke->ke_ksegrp->kg_pri_class);
	if (class != PRI_IDLE) {
		if (kseq_idle & myksq->ksq_group->ksg_mask)
			kseq_idle &= ~myksq->ksq_group->ksg_mask;
		if (myksq->ksq_group->ksg_idlemask & PCPU_GET(cpumask))
			myksq->ksq_group->ksg_idlemask &= ~PCPU_GET(cpumask);
	}
	if (ke->ke_thread->td_priority < curthread->td_priority)
		curthread->td_flags |= TDF_NEEDRESCHED;
	return (1);
}

static struct kse *
sched_steal(struct kseq *kseq, int my_cpu, int idle)
{
	struct kse *ke;

	/*
	 * Steal from expired queue first to try to get a non-interactive
	 * task that may not have run for a while.
	 */
	if ((ke = krunq_steal(kseq->ksq_next, my_cpu)) != NULL)
		return (ke);
	if ((ke = krunq_steal(kseq->ksq_curr, my_cpu)) != NULL)
		return (ke);
	if (idle == IDLE_IDLE)
		return (krunq_steal(&kseq->ksq_idle, my_cpu));
	return (NULL);
}

static int
sched_idled(struct kseq *kseq, int idle)
{
	struct kseq_group *ksg;
	struct kseq *steal;

	mtx_assert(&sched_lock, MA_OWNED);
	ksg = kseq->ksq_group;
	/*
	 * If we're in a cpu group, try and steal kses from another cpu in
	 * the group before idling.
	 */
	if (ksg->ksg_cpus > 1 && ksg->ksg_transferable) {
		LIST_FOREACH(steal, &ksg->ksg_members, ksq_siblings) {
			if (steal == kseq || steal->ksq_transferable == 0)
				continue;
			if (sched_pull_one(steal, kseq, idle))
				return (0);
		}
	}

	if (sched_balance_idle(PCPU_GET(cpuid), idle))
		return (0);

	/*
	 * We only set the idled bit when all of the cpus in the group are
	 * idle.  Otherwise we could get into a situation where a KSE bounces
	 * back and forth between two idle cores on seperate physical CPUs.
	 */
	ksg->ksg_idlemask |= PCPU_GET(cpumask);
	if (ksg->ksg_idlemask != ksg->ksg_cpumask)
		return (1);
	kseq_idle |= ksg->ksg_mask;
	return (1);
}

static int
sched_find_idle_cpu(int defcpu)
{
	struct pcpu *pcpu;
	struct kseq_group *ksg;
	struct kseq *ksq;
	int cpu;

	mtx_assert(&sched_lock, MA_OWNED);
	ksq = KSEQ_CPU(defcpu);
	ksg = ksq->ksq_group;
	pcpu = pcpu_find(defcpu);
	if (ksg->ksg_idlemask & pcpu->pc_cpumask)
		return (defcpu);

	/* Try to find a fully idled cpu. */
	if (kseq_idle) {
		cpu = ffs(kseq_idle);
		if (cpu)
			goto migrate;
	}

	/*
	 * If another cpu in this group has idled, assign a thread over
	 * to them after checking to see if there are idled groups.
	 */
	if (ksg->ksg_idlemask) {
		cpu = ffs(ksg->ksg_idlemask);
		if (cpu)
			goto migrate;
	}
	return (defcpu);

migrate:
	/*
	 * Now that we've found an idle CPU, migrate the thread.
	 */
	cpu--;
	return (cpu);
}

static int
sched_find_idlest_cpu(struct kse *ke, int cpu)
{
	static unsigned stage_cpu;

	struct kseq_group *ksg;
	struct kseq *ksq;
	int load, min_load = INT_MAX;
	int first = 1;
	int idlest = -1;
	int i, cnt;

	(void)ke;

	if (__predict_false(smp_started == 0))
		return (cpu);

	first = 1;
	i = (stage_cpu++) % (ksg_maxid + 1);
	for (cnt = 0; cnt <= ksg_maxid; cnt++) {
		ksg  = KSEQ_GROUP(i);
		LIST_FOREACH(ksq, &ksg->ksg_members, ksq_siblings) {
			load = kseq_source_load(ksq);
			if (first || load < min_load) {
				first = 0;
				load = min_load;
				idlest = KSEQ_ID(ksq);
			}
		}
		if (++i > ksg_maxid)
			i = 0;
	}
        return (idlest);
}

static void
migrated_setup(void *dummy)
{
	struct kseq	*kseq;
	struct proc	*p;
	struct thread	*td;
	int		i, error;

	for (i = 0; i < MAXCPU; i++) {
		if (CPU_ABSENT(i))
			continue;
		kseq = &kseq_cpu[i];
		error = kthread_create(migrated, kseq, &p, RFSTOPPED, 0,
			"migrated%d", i);
		if (error)
			panic("can not create migration thread");
		PROC_LOCK(p);
		p->p_flag |= P_NOLOAD;
		mtx_lock_spin(&sched_lock);
		td = FIRST_THREAD_IN_PROC(p);
		td->td_kse->ke_flags |= KEF_BOUND;
		td->td_kse->ke_cpu = i;
		kseq->ksq_migrated = td;
		sched_class(td->td_ksegrp, PRI_ITHD);
		td->td_kse->ke_runq = kseq->ksq_curr;
		sched_prio(td, PRI_MIN);
		SLOT_USE(td->td_ksegrp);
		kseq_runq_add(kseq, td->td_kse, 0);
		td->td_kse->ke_state = KES_ONRUNQ;
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
	}
}

static void
migrated(void *dummy)
{
	struct thread	*td = curthread;
	struct kseq	*kseq = KSEQ_SELF();
	struct kse	*ke;

	mtx_lock_spin(&sched_lock);
	for (;;) {
		while ((ke = TAILQ_FIRST(&kseq->ksq_migrateq)) != NULL) {
			TAILQ_REMOVE(&kseq->ksq_migrateq, ke, ke_procq);
			kseq_load_rem(kseq, ke);
			ke->ke_flags &= ~KEF_MIGRATING;
			ke->ke_cpu = ke->ke_tocpu;
			setrunqueue(ke->ke_thread, SRQ_BORING);
		}
		TD_SET_IWAIT(td);
		mi_switch(SW_VOL, NULL);
	}
	mtx_unlock_spin(&sched_lock);
}
#else

static inline void
sched_balance_tick(int my_cpu, int idle)
{
}

#endif	/* SMP */


static void
kseq_setup(struct kseq *kseq)
{
	krunq_init(&kseq->ksq_timeshare[0]);
	krunq_init(&kseq->ksq_timeshare[1]);
	krunq_init(&kseq->ksq_idle);
	kseq->ksq_curr = &kseq->ksq_timeshare[0];
	kseq->ksq_next = &kseq->ksq_timeshare[1];
	kseq->ksq_best_expired_nice = 21;
#ifdef SMP
	TAILQ_INIT(&kseq->ksq_migrateq);
#endif
}

static void
sched_setup(void *dummy)
{
#ifdef SMP
	int i;
	int t;
#endif

	/*
	 * To avoid divide-by-zero, we set realstathz a dummy value
	 * in case which sched_clock() called before sched_initticks().
	 */
	realstathz	= hz;
	min_timeslice	= MAX(5 * hz / 1000, 1);
	def_timeslice	= MAX(100 * hz / 1000, 1);
	granularity	= MAX(10 * hz / 1000, 1);

#ifdef SMP
	t = ticks;
	balance_tick = t;
	/*
	 * Initialize the kseqs.
	 */
	for (i = 0; i < MAXCPU; i++) {
		struct kseq *ksq;

		ksq = &kseq_cpu[i];
		kseq_setup(&kseq_cpu[i]);
	}
	if (smp_topology == NULL || ignore_topology) {
		struct kseq_group *ksg;
		struct kseq *ksq;
		int cpus;

		for (cpus = 0, i = 0; i < MAXCPU; i++) {
			if (CPU_ABSENT(i))
				continue;
			ksq = &kseq_cpu[i];
			ksg = &kseq_groups[cpus];
			/*
			 * Setup a kseq group with one member.
			 */
			ksq->ksq_group = ksg;
			ksg->ksg_cpus = 1;
			ksg->ksg_idlemask = 0;
			ksg->ksg_cpumask = ksg->ksg_mask = 1 << i;
			ksg->ksg_balance_tick = t;
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
					kseq_cpu[j].ksq_group = ksg;
					LIST_INSERT_HEAD(&ksg->ksg_members,
					    &kseq_cpu[j], ksq_siblings);
				}
			}
			ksg->ksg_balance_tick = t;
		}
		ksg_maxid = smp_topology->ct_count - 1;
	}
#else
	kseq_setup(KSEQ_SELF());
#endif
	mtx_lock_spin(&sched_lock);
	kseq_load_add(KSEQ_SELF(), &kse0);
	mtx_unlock_spin(&sched_lock);
}

/* ARGSUSED */
static void
sched_initticks(void *dummy)
{
	mtx_lock_spin(&sched_lock);
	realstathz = stathz ? stathz : hz;
	mtx_unlock_spin(&sched_lock);
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
	kse0.ke_slice = 100;
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
	return (def_timeslice);
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

	ke = td->td_kse;
	mtx_assert(&sched_lock, MA_OWNED);
	if (__predict_false(td->td_priority == prio))
		return;

	if (TD_ON_RUNQ(td)) {
		/*
		 * If the priority has been elevated due to priority
		 * propagation, we may have to move ourselves to a new
		 * queue.  We still call adjustrunqueue below in case kse
		 * needs to fix things up.
		 */
		if (prio < td->td_priority && ke->ke_runq != NULL &&
		    ke->ke_runq != KSEQ_CPU(ke->ke_cpu)->ksq_curr) {
			krunq_remove(ke->ke_runq, ke);
			ke->ke_runq = KSEQ_CPU(ke->ke_cpu)->ksq_curr;
			krunq_add(ke->ke_runq, ke, 0);
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
	struct ksegrp *kg;
	uint64_t now;

	mtx_assert(&sched_lock, MA_OWNED);

	now = sched_timestamp();
	ke = td->td_kse;
	kg = td->td_ksegrp;
	ksq = KSEQ_SELF();

	td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;

	/*
	 * If the KSE has been assigned it may be in the process of switching
	 * to the new cpu.  This is the case in sched_bind().
	 */
	if (__predict_false(td == PCPU_GET(idlethread))) {
		TD_SET_CAN_RUN(td);
	} else if (__predict_false(ke->ke_flags & KEF_MIGRATING)) {
		SLOT_RELEASE(td->td_ksegrp);
	} else {
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
		newtd->td_kse->ke_timestamp = now;
	} else
		newtd = choosethread();
	if (td != newtd) {
		sched_update_runtime(ke, now);
		ke->ke_lastran = now;

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
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	p->p_nice = nice;
	FOREACH_KSEGRP_IN_PROC(p, kg) {
		if (kg->kg_pri_class == PRI_TIMESHARE) {
			kg->kg_user_pri = sched_calc_pri(kg);
			FOREACH_THREAD_IN_GROUP(kg, td)
				td->td_flags |= TDF_NEEDRESCHED;
		}
	}
}

void
sched_sleep(struct thread *td)
{
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	if (td->td_flags & TDF_SINTR)
		ke->ke_activated = 0;
	else
		ke->ke_activated = -1;
	ke->ke_flags |= KEF_SLEEP;
}

void
sched_wakeup(struct thread *td)
{
	struct kse *ke;
	struct ksegrp *kg;
	struct kseq *kseq, *mykseq;
	uint64_t now;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	kg = td->td_ksegrp;
	kseq = KSEQ_CPU(ke->ke_cpu);
	mykseq = KSEQ_SELF();
	if (ke->ke_flags & KEF_SLEEP) {
		ke->ke_flags &= ~KEF_SLEEP;
		if (sched_is_timeshare(kg)) {
			now = sched_timestamp();
			sched_commit_runtime(ke);
#ifdef SMP
			if (kseq != mykseq)
				now = now - mykseq->ksq_last_timestamp +
				    kseq->ksq_last_timestamp;
#endif
			kg->kg_user_pri = sched_recalc_pri(ke, now);
		}
	}
	ke->ke_flags &= ~KEF_NEXTRQ;
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
	child->kg_slptime = kg->kg_slptime * CHILD_WEIGHT / 100;
	if (child->kg_pri_class == PRI_TIMESHARE)
		child->kg_user_pri = sched_calc_pri(child);
	kg->kg_slptime = kg->kg_slptime * PARENT_WEIGHT / 100;
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
	struct kse *ke;
	struct kse *ke2;

	sched_newthread(child);

	ke = td->td_kse;
	ke2 = child->td_kse;
#ifdef SMP
	ke2->ke_cpu = sched_find_idlest_cpu(ke, PCPU_GET(cpuid));
#else
	ke2->ke_cpu = ke->ke_cpu;
#endif
	ke2->ke_slice = (ke->ke_slice + 1) >> 1;
	ke2->ke_flags |= KEF_FIRST_SLICE;
	ke2->ke_activated = 0;
	ke2->ke_timestamp = sched_timestamp();
	ke->ke_slice >>= 1;
        if (ke->ke_slice == 0) {
		ke->ke_slice = 1;
		sched_tick();
	}

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

		/* New thread does not have runq assigned */
		if (ke->ke_runq == NULL)
			continue;

		kseq = KSEQ_CPU(ke->ke_cpu);
		if (oclass == PRI_TIMESHARE)
			kseq->ksq_load_timeshare--;
		else if (oclass == PRI_IDLE)
			kseq->ksq_load_idle--;

		if (nclass == PRI_TIMESHARE)
			kseq->ksq_load_timeshare++;
		else if (nclass == PRI_IDLE)
			kseq->ksq_load_idle++;
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
	sched_exit_thread(FIRST_THREAD_IN_PROC(p), childtd);
	sched_exit_ksegrp(FIRST_KSEGRP_IN_PROC(p), childtd);
}

void
sched_exit_ksegrp(struct ksegrp *parentkg, struct thread *td)
{
	if (td->td_ksegrp->kg_slptime < parentkg->kg_slptime) {
		parentkg->kg_slptime = parentkg->kg_slptime /
			(EXIT_WEIGHT) * (EXIT_WEIGHT - 1) +
			td->td_ksegrp->kg_slptime / EXIT_WEIGHT;
	}
}

void
sched_exit_thread(struct thread *td, struct thread *childtd)
{
	struct kse *childke  = childtd->td_kse;
	struct kse *parentke = td->td_kse;

	kseq_load_rem(KSEQ_CPU(childke->ke_cpu), childke);
	sched_update_runtime(childke, sched_timestamp());
	sched_commit_runtime(childke);
	if ((childke->ke_flags & KEF_FIRST_SLICE) &&
	    td->td_proc == childtd->td_proc->p_pptr) {
		parentke->ke_slice += childke->ke_slice;
		if (parentke->ke_slice > sched_timeslice(parentke))
			parentke->ke_slice = sched_timeslice(parentke);
	}
}

static int
sched_starving(struct kseq *ksq, uint64_t now, struct kse *ke)
{
	uint64_t delta;

	if (PROC_NICE(ke->ke_proc) > ksq->ksq_best_expired_nice)
		return (1);
	if (ksq->ksq_expired_timestamp == 0)
		return (0);
	delta = now - ksq->ksq_expired_timestamp;
	if (delta > STARVATION_TIME * (ksq->ksq_load - ksq->ksq_load_idle))
		return (1);
	return (0);
}

/*
 * An interactive thread has smaller time slice granularity,
 * a cpu hog can have larger granularity.
 */
static inline int
sched_timeslice_split(struct kse *ke)
{
	int score, g;

	score = (int)(MAX_SCORE - CURRENT_SCORE(ke->ke_ksegrp));
	if (score == 0)
		score = 1;
#ifdef SMP
	g = granularity * ((1 << score) - 1) * smp_cpus;
#else
	g = granularity * ((1 << score) - 1);
#endif
	return (ke->ke_slice >= g && ke->ke_slice % g == 0);
}

void
sched_tick(void)
{
	struct thread	*td;
	struct proc	*p;
	struct kse	*ke;
	struct ksegrp	*kg;
	struct kseq	*kseq;
	uint64_t	now;
	int		cpuid;
	int		class;
	
	mtx_assert(&sched_lock, MA_OWNED);

	td = curthread;
	ke = td->td_kse;
	kg = ke->ke_ksegrp;
	p = ke->ke_proc;
	class = PRI_BASE(kg->kg_pri_class);
	now = sched_timestamp();
	cpuid = PCPU_GET(cpuid);
	kseq = KSEQ_CPU(cpuid);
	kseq->ksq_last_timestamp = now;

	if (class == PRI_IDLE) {
		int idle_td = (curthread == PCPU_GET(idlethread));
		/*
		 * Processes of equal idle priority are run round-robin.
		 */
		if (!idle_td && --ke->ke_slice <= 0) {
			ke->ke_slice = def_timeslice;
			td->td_flags |= TDF_NEEDRESCHED;
		}
		sched_balance_tick(cpuid, idle_td ?  IDLE_IDLE : IDLE);
		return;
	}

	if (ke->ke_flags & KEF_NEXTRQ) {
		/* The thread was already scheduled off. */
		curthread->td_flags |= TDF_NEEDRESCHED;
		goto out;
	}

	if (class == PRI_REALTIME) {
		/*
		 * Realtime scheduling, do round robin for RR class, FIFO
		 * is not affected.
		 */
		if (PRI_NEED_RR(kg->kg_pri_class) && --ke->ke_slice <= 0) {
			ke->ke_slice = def_timeslice;
			curthread->td_flags |= TDF_NEEDRESCHED;
		}
		goto out;
	}

	/*
	 * Current, we skip kernel thread, though it may be classified as
	 * TIMESHARE.
	 */
	if (class != PRI_TIMESHARE || (p->p_flag & P_KTHREAD) != 0)
		goto out;

	if (--ke->ke_slice <= 0) {
		curthread->td_flags |= TDF_NEEDRESCHED;
		sched_update_runtime(ke, now);
		sched_commit_runtime(ke);
		kg->kg_user_pri = sched_calc_pri(kg);
		ke->ke_slice = sched_timeslice(ke);
		ke->ke_flags &= ~KEF_FIRST_SLICE;
		if (!kseq->ksq_expired_timestamp)
			kseq->ksq_expired_timestamp = now;
		if (!THREAD_IS_INTERACTIVE(ke) ||
		    sched_starving(kseq, now, ke)) {
			/* The thead becomes cpu hog, schedule it off. */
			ke->ke_flags |= KEF_NEXTRQ;
			if (PROC_NICE(p) < kseq->ksq_best_expired_nice)
				kseq->ksq_best_expired_nice = PROC_NICE(p);
		}
	} else {
		/*
		 * Don't allow an interactive thread which has long timeslice
		 * to monopolize CPU, split the long timeslice into small
		 * chunks. This essentially does round-robin between
		 * interactive threads.
		 */
		if (THREAD_IS_INTERACTIVE(ke) && sched_timeslice_split(ke))
			curthread->td_flags |= TDF_NEEDRESCHED;
	}

out:
	sched_balance_tick(cpuid, NOT_IDLE);
}

void
sched_clock(struct thread *td)
{
	struct ksegrp *kg;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	kg = ke->ke_ksegrp;

	/* Adjust ticks for pctcpu */
	ke->ke_ticks++;
	ke->ke_ltick = ticks;

	/* Go up to one second beyond our max and then trim back down */
	if (ke->ke_ftick + SCHED_CPU_TICKS + hz < ke->ke_ltick)
		sched_pctcpu_update(ke);
}

int
sched_runnable(void)
{
	struct kseq *kseq;

	kseq = KSEQ_SELF();
	if (krunq_findbit(kseq->ksq_curr) != -1 ||
	    krunq_findbit(kseq->ksq_next) != -1 ||
	    krunq_findbit(&kseq->ksq_idle) != -1)
		return (1);
	return (0);
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
#endif
	ke = kseq_choose(kseq);
	if (ke) {
#ifdef SMP
		if (ke->ke_ksegrp->kg_pri_class == PRI_IDLE)
			if (sched_idled(kseq, IDLE) == 0)
				goto restart;
#endif
		kseq_runq_rem(kseq, ke);
		ke->ke_state = KES_THREAD;
		ke->ke_flags &= ~KEF_PREEMPTED;
		ke->ke_timestamp = sched_timestamp();
		return (ke);
	}
#ifdef SMP
	if (sched_idled(kseq, IDLE_IDLE) == 0)
		goto restart;
#endif
	return (NULL);
}

void
sched_add(struct thread *td, int flags)
{
	struct kseq *ksq, *my_ksq;
	struct ksegrp *kg;
	struct kse *ke;
	int preemptive;
	int canmigrate;
	int class;
	int my_cpu;
	int nextrq;
#ifdef SMP
	struct thread *td2;
	struct pcpu *pcpu;
	int cpu, new_cpu;
	int load, my_load;
#endif

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	kg = td->td_ksegrp;
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	KASSERT(ke->ke_runq == NULL,
	    ("sched_add: KSE %p is still assigned to a run queue", ke));

	canmigrate = 1;
	preemptive = !(flags & SRQ_YIELDING);
	class = PRI_BASE(kg->kg_pri_class);
	my_cpu = PCPU_GET(cpuid);
	my_ksq = KSEQ_CPU(my_cpu);
	if (flags & SRQ_PREEMPTED)
		ke->ke_flags |= KEF_PREEMPTED;
	if ((ke->ke_flags & KEF_INTERNAL) == 0)
		SLOT_USE(td->td_ksegrp);
	nextrq = (ke->ke_flags & KEF_NEXTRQ);
	ke->ke_flags &= ~(KEF_NEXTRQ | KEF_INTERNAL);

#ifdef SMP
	cpu = ke->ke_cpu;
	canmigrate = kse_can_migrate(ke);
	/*
	 * Don't migrate running threads here.  Force the long term balancer
	 * to do it.
	 */
	if (ke->ke_flags & KEF_HOLD) {
		ke->ke_flags &= ~KEF_HOLD;
		canmigrate = 0;
	}

	/*
	 * If this thread is pinned or bound, notify the target cpu.
	 */
	if (!canmigrate)
		goto activate_it;

	if (class == PRI_ITHD) {
		ke->ke_cpu = my_cpu;
		goto activate_it;
	}

	if (ke->ke_cpu == my_cpu)
		goto activate_it;

	if (my_ksq->ksq_group->ksg_idlemask & PCPU_GET(cpumask)) {
		ke->ke_cpu = my_cpu;
		goto activate_it;
	}

	new_cpu = my_cpu;

	load = kseq_source_load(KSEQ_CPU(cpu));
	my_load = kseq_dest_load(my_ksq);
	if ((my_load - load) * 100 < my_load * imbalance_pct2)
		goto try_idle_cpu;
	new_cpu = cpu;

try_idle_cpu:
	new_cpu = sched_find_idle_cpu(new_cpu);
	ke->ke_cpu = new_cpu;

activate_it:
	if (ke->ke_cpu != cpu)
		ke->ke_lastran = 0;
#endif
	ksq = KSEQ_CPU(ke->ke_cpu);
	switch (class) {
	case PRI_ITHD:
	case PRI_REALTIME:
		ke->ke_runq = ksq->ksq_curr;
		break;
	case PRI_TIMESHARE:
		if ((td->td_flags & TDF_BORROWING) == 0 && nextrq)
			ke->ke_runq = ksq->ksq_next;
		else
			ke->ke_runq = ksq->ksq_curr;
		break;
	case PRI_IDLE:
		/*
		 * This is for priority prop.
		 */
		if (td->td_priority < PRI_MIN_IDLE)
			ke->ke_runq = ksq->ksq_curr;
		else
			ke->ke_runq = &ksq->ksq_idle;
		break;
	default:
		panic("Unknown pri class.");
		break;
	}

	if (ke->ke_runq == my_ksq->ksq_curr &&
	    td->td_priority < curthread->td_priority) {
		curthread->td_flags |= TDF_NEEDRESCHED;
		if (preemptive && maybe_preempt(td))
			return;
		if (curthread->td_ksegrp->kg_pri_class == PRI_IDLE)
			td->td_owepreempt = 1;
	}
	ke->ke_state = KES_ONRUNQ;
	kseq_runq_add(ksq, ke, flags);
	kseq_load_add(ksq, ke);
#ifdef SMP
	pcpu = pcpu_find(ke->ke_cpu);
	if (class != PRI_IDLE) {
		if (kseq_idle & ksq->ksq_group->ksg_mask)
			kseq_idle &= ~ksq->ksq_group->ksg_mask;
		if (ksq->ksq_group->ksg_idlemask & pcpu->pc_cpumask)
			ksq->ksq_group->ksg_idlemask &= ~pcpu->pc_cpumask;
	}
	if (ke->ke_cpu != my_cpu) {
		td2 = pcpu->pc_curthread;
		if (__predict_false(td2 == pcpu->pc_idlethread)) {
			td2->td_flags |= TDF_NEEDRESCHED;
			ipi_selected(pcpu->pc_cpumask, IPI_AST);
		} else if (td->td_priority < td2->td_priority) {
			if (class == PRI_ITHD || class == PRI_REALTIME ||
			    td2->td_ksegrp->kg_pri_class == PRI_IDLE)
		                ipi_selected(pcpu->pc_cpumask, IPI_PREEMPT);
			else if ((td2->td_flags & TDF_NEEDRESCHED) == 0) {
				td2->td_flags |= TDF_NEEDRESCHED;
				ipi_selected(pcpu->pc_cpumask, IPI_AST);
			}
		}
	}
#endif
}

void
sched_rem(struct thread *td)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	SLOT_RELEASE(td->td_ksegrp);
	ke->ke_flags &= ~KEF_PREEMPTED;
	KASSERT((ke->ke_state == KES_ONRUNQ),
	    ("sched_rem: KSE not on run queue"));

	kseq = KSEQ_CPU(ke->ke_cpu);
#ifdef SMP
	if (ke->ke_flags & KEF_MIGRATING) {
		ke->ke_flags &= ~KEF_MIGRATING;
		kseq_load_rem(kseq, ke);
		TAILQ_REMOVE(&kseq->ksq_migrateq, ke, ke_procq);
		ke->ke_cpu = ke->ke_tocpu;
	} else
#endif
	{
		KASSERT((ke->ke_state == KES_ONRUNQ),
		    ("sched_rem: KSE not on run queue"));
		kseq_runq_rem(kseq, ke);
		kseq_load_rem(kseq, ke);
	}
	ke->ke_state = KES_THREAD;
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
		rtick = MIN(ke->ke_ticks / SCHED_CPU_TIME, SCHED_CPU_TICKS);
		pctcpu = (FSCALE * ((FSCALE * rtick)/realstathz)) >> FSHIFT;
	}

	ke->ke_proc->p_swtime = ke->ke_ltick - ke->ke_ftick;
	mtx_unlock_spin(&sched_lock);

	return (pctcpu);
}

void
sched_bind(struct thread *td, int cpu)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	ke->ke_flags |= KEF_BOUND;
#ifdef SMP
	if (PCPU_GET(cpuid) == cpu)
		return;
	kseq = KSEQ_SELF();
	ke->ke_flags |= KEF_MIGRATING;
	ke->ke_tocpu = cpu;
	TAILQ_INSERT_TAIL(&kseq->ksq_migrateq, ke, ke_procq);
	if (kseq->ksq_migrated) {
		if (TD_AWAITING_INTR(kseq->ksq_migrated)) {
			TD_CLR_IWAIT(kseq->ksq_migrated);
			setrunqueue(kseq->ksq_migrated, SRQ_YIELDING);
		}
	}
	/* When we return from mi_switch we'll be on the correct cpu. */
	mi_switch(SW_VOL, NULL);
#else
	(void)kseq;
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
	for (i = 0; i < MAXCPU; i++)
		total += KSEQ_CPU(i)->ksq_sysload;
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
