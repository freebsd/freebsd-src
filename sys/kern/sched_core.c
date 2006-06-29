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
	struct thread	*ke_thread;	/* (*) Active associated thread. */
	TAILQ_ENTRY(kse) ke_procq;	/* (j/z) Run queue. */
	int		ke_flags;	/* (j) KEF_* flags. */
	fixpt_t		ke_pctcpu;	/* (j) %cpu during p_swtime. */
	u_char		ke_rqindex;	/* (j) Run queue index. */
	enum {
		KES_THREAD = 0x0,	/* slaved to thread state */
		KES_ONRUNQ
	} ke_state;			/* (j) thread sched specific status. */
	int		ke_slice;	/* Time slice in ticks */
	struct kseq	*ke_kseq;	/* Kseq the thread belongs to */
	struct krunq	*ke_runq;	/* Assiociated runqueue */
#ifdef SMP
	int		ke_cpu;		/* CPU that we have affinity for. */
	int		ke_wakeup_cpu;	/* CPU that has activated us. */
#endif
	int		ke_activated;	/* How is the thread activated. */
	uint64_t	ke_timestamp;	/* Last timestamp dependent on state.*/
	unsigned	ke_lastran;	/* Last timestamp the thread ran. */

	/* The following variables are only used for pctcpu calculation */
	int		ke_ltick;	/* Last tick that we were running on */
	int		ke_ftick;	/* First tick that we were running on */
	int		ke_ticks;	/* Tick count */
};

#define	td_kse			td_sched
#define ke_proc			ke_thread->td_proc
#define ke_ksegrp		ke_thread->td_ksegrp

/* flags kept in ke_flags */
#define	KEF_BOUND	0x0001		/* Thread can not migrate. */
#define	KEF_PREEMPTED	0x0002		/* Thread was preempted. */
#define KEF_MIGRATING	0x0004		/* Thread is migrating. */
#define	KEF_SLEEP	0x0008		/* Thread did sleep. */
#define	KEF_DIDRUN	0x0010		/* Thread actually ran. */
#define	KEF_EXIT	0x0020		/* Thread is being killed. */
#define KEF_NEXTRQ	0x0400		/* Thread should be in next queue. */
#define KEF_FIRST_SLICE	0x0800		/* Thread has first time slice left. */

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
	struct krunq	*ksq_curr;		/* Current queue. */
	struct krunq	*ksq_next;		/* Next timeshare queue. */
	struct krunq	ksq_timeshare[2];	/* Run queues for !IDLE. */
	struct krunq	ksq_idle;		/* Queue of IDLE threads. */
	int		ksq_load;
	uint64_t	ksq_last_timestamp;	/* Per-cpu last clock tick */
	unsigned	ksq_expired_tick;	/* First expired tick */
	signed char	ksq_expired_nice;	/* Lowest nice in nextq */
};

static struct kse kse0;
static struct kg_sched kg_sched0;

static int min_timeslice = 5;
static int def_timeslice = 100;
static int granularity = 10;
static int realstathz;
static int sched_tdcnt;
static struct kseq kseq_global;

/*
 * One kse queue per processor.
 */
#ifdef SMP
static struct kseq	kseq_cpu[MAXCPU];

#define	KSEQ_SELF()	(&kseq_cpu[PCPU_GET(cpuid)])
#define	KSEQ_CPU(x)	(&kseq_cpu[(x)])
#define	KSEQ_ID(x)	((x) - kseq_cpu)

static cpumask_t	cpu_sibling[MAXCPU];

#else	/* !SMP */

#define	KSEQ_SELF()	(&kseq_global)
#define	KSEQ_CPU(x)	(&kseq_global)
#endif

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL);

static void sched_initticks(void *dummy);
SYSINIT(sched_initticks, SI_SUB_CLOCKS, SI_ORDER_THIRD, sched_initticks, NULL)

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0, "Scheduler");

SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "CORE", 0,
    "Scheduler name");

#ifdef SMP
/* Enable forwarding of wakeups to all other cpus */
SYSCTL_NODE(_kern_sched, OID_AUTO, ipiwakeup, CTLFLAG_RD, NULL, "Kernel SMP");

static int runq_fuzz = 0;
SYSCTL_INT(_kern_sched, OID_AUTO, runq_fuzz, CTLFLAG_RW, &runq_fuzz, 0, "");

static int forward_wakeup_enabled = 1;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, enabled, CTLFLAG_RW,
	   &forward_wakeup_enabled, 0,
	   "Forwarding of wakeup to idle CPUs");

static int forward_wakeups_requested = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, requested, CTLFLAG_RD,
	   &forward_wakeups_requested, 0,
	   "Requests for Forwarding of wakeup to idle CPUs");

static int forward_wakeups_delivered = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, delivered, CTLFLAG_RD,
	   &forward_wakeups_delivered, 0,
	   "Completed Forwarding of wakeup to idle CPUs");

static int forward_wakeup_use_mask = 1;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, usemask, CTLFLAG_RW,
	   &forward_wakeup_use_mask, 0,
	   "Use the mask of idle cpus");

static int forward_wakeup_use_loop = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, useloop, CTLFLAG_RW,
	   &forward_wakeup_use_loop, 0,
	   "Use a loop to find idle cpus");

static int forward_wakeup_use_single = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, onecpu, CTLFLAG_RW,
	   &forward_wakeup_use_single, 0,
	   "Only signal one idle cpu");

static int forward_wakeup_use_htt = 0;
SYSCTL_INT(_kern_sched_ipiwakeup, OID_AUTO, htt2, CTLFLAG_RW,
	   &forward_wakeup_use_htt, 0,
	   "account for htt");
#endif

static void slot_fill(struct ksegrp *);

static void krunq_add(struct krunq *, struct kse *);
static struct kse *krunq_choose(struct krunq *);
static void krunq_clrbit(struct krunq *rq, int pri);
static int krunq_findbit(struct krunq *rq);
static void krunq_init(struct krunq *);
static void krunq_remove(struct krunq *, struct kse *);

static struct kse * kseq_choose(struct kseq *);
static void kseq_load_add(struct kseq *, struct kse *);
static void kseq_load_rem(struct kseq *, struct kse *);
static void kseq_runq_add(struct kseq *, struct kse *);
static void kseq_runq_rem(struct kseq *, struct kse *);
static void kseq_setup(struct kseq *);

static int sched_is_timeshare(struct ksegrp *kg);
static struct kse *sched_choose(void);
static int sched_calc_pri(struct ksegrp *kg);
static int sched_starving(struct kseq *, unsigned, struct kse *);
static void sched_pctcpu_update(struct kse *);
static void sched_thread_priority(struct thread *, u_char);
static uint64_t	sched_timestamp(void);
static int sched_recalc_pri(struct kse *ke, uint64_t now);
static int sched_timeslice(struct kse *ke);
static void sched_update_runtime(struct kse *ke, uint64_t now);
static void sched_commit_runtime(struct kse *ke);

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

static int
krunq_check(struct krunq *rq)
{
	struct krqbits *rqb;
	int i;

	rqb = &rq->rq_status;
	for (i = 0; i < KQB_LEN; i++) {
		if (rqb->rqb_bits[i])
			return (1);
	}
	return (0);
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
krunq_add(struct krunq *rq, struct kse *ke)
{
	struct krqhead *rqh;
	int pri;

	pri = ke->ke_thread->td_priority;
	ke->ke_rqindex = pri;
	krunq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	if (ke->ke_flags & KEF_PREEMPTED)
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
		KASSERT(ke != NULL, ("krunq_choose: no thread on busy queue"));
#ifdef SMP
		if (pri <= PRI_MAX_ITHD || runq_fuzz <= 0)
			return (ke);

		/*
		 * In the first couple of entries, check if
		 * there is one for our CPU as a preference.
		 */
		struct kse *ke2 = ke;
		const int mycpu = PCPU_GET(cpuid);
		const int mymask = 1 << mycpu;
		int count = runq_fuzz;

		while (count-- && ke2) {
			const int cpu = ke2->ke_wakeup_cpu;
			if (cpu_sibling[cpu] & mymask) {
				ke = ke2;
				break;
			}
			ke2 = TAILQ_NEXT(ke2, ke_procq);
		}
#endif
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

static inline void
kseq_runq_add(struct kseq *kseq, struct kse *ke)
{
	krunq_add(ke->ke_runq, ke);
	ke->ke_kseq = kseq;
}

static inline void
kseq_runq_rem(struct kseq *kseq, struct kse *ke)
{
	krunq_remove(ke->ke_runq, ke);
	ke->ke_kseq = NULL;
	ke->ke_runq = NULL;
}

static inline void
kseq_load_add(struct kseq *kseq, struct kse *ke)
{
	kseq->ksq_load++;
	if ((ke->ke_proc->p_flag & P_NOLOAD) == 0)
		sched_tdcnt++;
}

static inline void
kseq_load_rem(struct kseq *kseq, struct kse *ke)
{
	kseq->ksq_load--;
	if ((ke->ke_proc->p_flag & P_NOLOAD) == 0)
		sched_tdcnt++;
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

	kseq->ksq_expired_nice = PRIO_MAX + 1;
	kseq->ksq_expired_tick = 0;
	swap = kseq->ksq_curr;
	kseq->ksq_curr = kseq->ksq_next;
	kseq->ksq_next = swap;
	ke = krunq_choose(kseq->ksq_curr);
	if (ke != NULL)
		return (ke);

	return krunq_choose(&kseq->ksq_idle);
}

extern unsigned long long cycles_2_ns(unsigned long long cyc);

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
		return SCALE_USER_PRI(def_timeslice, PROC_USER_PRI(p));
}

static inline int
sched_is_timeshare(struct ksegrp *kg)
{
	return (kg->kg_pri_class == PRI_TIMESHARE);
}

static int
sched_calc_pri(struct ksegrp *kg)
{
	int score, pri;

	if (sched_is_timeshare(kg)) {
		score = CURRENT_SCORE(kg) - MAX_SCORE / 2;
		pri = PROC_PRI(kg->kg_proc) - score;
		if (pri < PUSER)
			pri = PUSER;
		if (pri > PUSER_MAX)
			pri = PUSER_MAX;
		return (pri);
	}
	return (kg->kg_user_pri);
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

static void
kseq_setup(struct kseq *kseq)
{
	krunq_init(&kseq->ksq_timeshare[0]);
	krunq_init(&kseq->ksq_timeshare[1]);
	krunq_init(&kseq->ksq_idle);
	kseq->ksq_curr = &kseq->ksq_timeshare[0];
	kseq->ksq_next = &kseq->ksq_timeshare[1];
	kseq->ksq_expired_nice = PRIO_MAX + 1;
	kseq->ksq_expired_tick = 0;
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
	realstathz	= hz;
	min_timeslice	= MAX(5 * hz / 1000, 1);
	def_timeslice	= MAX(100 * hz / 1000, 1);
	granularity	= MAX(10 * hz / 1000, 1);

	kseq_setup(&kseq_global);
#ifdef SMP
	runq_fuzz = MIN(mp_ncpus * 2, 8);
	/*
	 * Initialize the kseqs.
	 */
	for (i = 0; i < MAXCPU; i++) {
		struct kseq *ksq;

		ksq = &kseq_cpu[i];
		kseq_setup(&kseq_cpu[i]);
		cpu_sibling[i] = 1 << i;
	}
	if (smp_topology != NULL) {
		int i, j;
		cpumask_t visited;
		struct cpu_group *cg;

		visited = 0;
		for (i = 0; i < smp_topology->ct_count; i++) {
			cg = &smp_topology->ct_group[i];
			if (cg->cg_mask & visited)
				panic("duplicated cpumask in ct_group.");
			if (cg->cg_mask == 0)
				continue;
			visited |= cg->cg_mask;
			for (j = 0; j < MAXCPU; j++) {
				if ((cg->cg_mask & (1 << j)) != 0)
					cpu_sibling[j] |= cg->cg_mask;
			}
		}
	}
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

static void
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
		    ke->ke_runq != ke->ke_kseq->ksq_curr) {
			krunq_remove(ke->ke_runq, ke);
			ke->ke_runq = ke->ke_kseq->ksq_curr;
			krunq_add(ke->ke_runq, ke);
		}
		/*
		 * Hold this kse on this cpu so that sched_prio() doesn't
		 * cause excessive migration.  We only want migration to
		 * happen as the result of a wakeup.
		 */
		adjustrunqueue(td, prio);
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

	if (td->td_ksegrp->kg_pri_class == PRI_TIMESHARE)
		prio = MIN(prio, PUSER_MAX);

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

	ke = td->td_kse;
	kg = td->td_ksegrp;
	ksq = KSEQ_SELF();

	td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;

	if (td == PCPU_GET(idlethread)) {
		TD_SET_CAN_RUN(td);
	} else {
		/* We are ending our run so make our slot available again */
		SLOT_RELEASE(td->td_ksegrp);
		kseq_load_rem(ksq, ke);
		if (TD_IS_RUNNING(td)) {
			setrunqueue(td, (flags & SW_PREEMPT) ?
			    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
			    SRQ_OURSELF|SRQ_YIELDING);
		} else {
			if ((td->td_proc->p_flag & P_HADTHREADS) &&
			    (newtd == NULL ||
			     newtd->td_ksegrp != td->td_ksegrp)) {
				/*
				 * We will not be on the run queue.
				 * So we must be sleeping or similar.
				 * Don't use the slot if we will need it 
				 * for newtd.
				 */
				slot_fill(td->td_ksegrp);
			}
			ke->ke_flags &= ~KEF_NEXTRQ;
		}
	}

	if (newtd != NULL) {
		/*
		 * If we bring in a thread account for it as if it had been
		 * added to the run queue and then chosen.
		 */
		SLOT_USE(newtd->td_ksegrp);
		newtd->td_kse->ke_flags |= KEF_DIDRUN;
		TD_SET_RUNNING(newtd);
		kseq_load_add(ksq, newtd->td_kse);
		now = newtd->td_kse->ke_timestamp = sched_timestamp();
	} else {
		newtd = choosethread();
		/* sched_choose sets ke_timestamp, just reuse it */
		now = newtd->td_kse->ke_timestamp;
	}
	if (td != newtd) {
		sched_update_runtime(ke, now);
		ke->ke_lastran = tick;

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
	mykseq = KSEQ_SELF();
	if (ke->ke_flags & KEF_SLEEP) {
		ke->ke_flags &= ~KEF_SLEEP;
		if (sched_is_timeshare(kg)) {
			kseq = KSEQ_CPU(td->td_lastcpu);
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
	ke2->ke_slice = (ke->ke_slice + 1) >> 1;
	ke2->ke_flags |= KEF_FIRST_SLICE | (ke->ke_flags & KEF_NEXTRQ);
	ke2->ke_activated = 0;
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
	mtx_assert(&sched_lock, MA_OWNED);
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

	kseq_load_rem(KSEQ_SELF(), childke);
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
sched_starving(struct kseq *ksq, unsigned now, struct kse *ke)
{
	uint64_t delta;

	if (ke->ke_proc->p_nice > ksq->ksq_expired_nice)
		return (1);
	if (ksq->ksq_expired_tick == 0)
		return (0);
	delta = HZ_TO_NS((uint64_t)now - ksq->ksq_expired_tick);
	if (delta > STARVATION_TIME * ksq->ksq_load)
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
	kg = td->td_ksegrp;
	p = td->td_proc;
	class = PRI_BASE(kg->kg_pri_class);
	now = sched_timestamp();
	cpuid = PCPU_GET(cpuid);
	kseq = KSEQ_CPU(cpuid);
	kseq->ksq_last_timestamp = now;

	if (class == PRI_IDLE) {
		/*
		 * Processes of equal idle priority are run round-robin.
		 */
		if (td != PCPU_GET(idlethread) && --ke->ke_slice <= 0) {
			ke->ke_slice = def_timeslice;
			td->td_flags |= TDF_NEEDRESCHED;
		}
		return;
	}

	if (class == PRI_REALTIME) {
		/*
		 * Realtime scheduling, do round robin for RR class, FIFO
		 * is not affected.
		 */
		if (PRI_NEED_RR(kg->kg_pri_class) && --ke->ke_slice <= 0) {
			ke->ke_slice = def_timeslice;
			td->td_flags |= TDF_NEEDRESCHED;
		}
		return;
	}

	/*
	 * We skip kernel thread, though it may be classified as TIMESHARE.
	 */
	if (class != PRI_TIMESHARE || (p->p_flag & P_KTHREAD) != 0)
		return;

	if (--ke->ke_slice <= 0) {
		td->td_flags |= TDF_NEEDRESCHED;
		sched_update_runtime(ke, now);
		sched_commit_runtime(ke);
		kg->kg_user_pri = sched_calc_pri(kg);
		ke->ke_slice = sched_timeslice(ke);
		ke->ke_flags &= ~KEF_FIRST_SLICE;
		if (ke->ke_flags & KEF_BOUND || td->td_pinned) {
			if (kseq->ksq_expired_tick == 0)
				kseq->ksq_expired_tick = tick;
		} else {
			if (kseq_global.ksq_expired_tick == 0)
				kseq_global.ksq_expired_tick = tick;
		}
		if (!THREAD_IS_INTERACTIVE(ke) ||
		    sched_starving(kseq, tick, ke) ||
		    sched_starving(&kseq_global, tick, ke)) {
			/* The thead becomes cpu hog, schedule it off. */
			ke->ke_flags |= KEF_NEXTRQ;
			if (ke->ke_flags & KEF_BOUND || td->td_pinned) {
				if (p->p_nice < kseq->ksq_expired_nice)
					kseq->ksq_expired_nice = p->p_nice;
			} else {
				if (p->p_nice < kseq_global.ksq_expired_nice)
					kseq_global.ksq_expired_nice =
						p->p_nice;
			}
		}
	} else {
		/*
		 * Don't allow an interactive thread which has long timeslice
		 * to monopolize CPU, split the long timeslice into small
		 * chunks. This essentially does round-robin between
		 * interactive threads.
		 */
		if (THREAD_IS_INTERACTIVE(ke) && sched_timeslice_split(ke))
			td->td_flags |= TDF_NEEDRESCHED;
	}
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

static int
kseq_runnable(struct kseq *kseq)
{
	return (krunq_check(kseq->ksq_curr) ||
	        krunq_check(kseq->ksq_next) ||
		krunq_check(&kseq->ksq_idle));
}

int
sched_runnable(void)
{
#ifdef SMP
	return (kseq_runnable(&kseq_global) || kseq_runnable(KSEQ_SELF()));
#else
	return (kseq_runnable(&kseq_global));
#endif
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
	struct kse  *ke;
	struct kseq *kseq;

#ifdef SMP
	struct kse *kecpu;

	mtx_assert(&sched_lock, MA_OWNED);
	kseq = &kseq_global;
	ke = kseq_choose(&kseq_global);
	kecpu = kseq_choose(KSEQ_SELF());

	if (ke == NULL || 
	    (kecpu != NULL && 
	     kecpu->ke_thread->td_priority < ke->ke_thread->td_priority)) {
		ke = kecpu;
		kseq = KSEQ_SELF();
	}
#else
	kseq = &kseq_global;
	ke = kseq_choose(kseq);
#endif

	if (ke != NULL) {
		kseq_runq_rem(kseq, ke);
		ke->ke_state = KES_THREAD;
		ke->ke_flags &= ~KEF_PREEMPTED;
		ke->ke_timestamp = sched_timestamp();
	}

	return (ke);
}

#ifdef SMP
static int
forward_wakeup(int cpunum, cpumask_t me)
{
	cpumask_t map, dontuse;
	cpumask_t map2;
	struct pcpu *pc;
	cpumask_t id, map3;

	mtx_assert(&sched_lock, MA_OWNED);

	CTR0(KTR_RUNQ, "forward_wakeup()");

	if ((!forward_wakeup_enabled) ||
	     (forward_wakeup_use_mask == 0 && forward_wakeup_use_loop == 0))
		return (0);
	if (!smp_started || cold || panicstr)
		return (0);

	forward_wakeups_requested++;

	/*
	 * check the idle mask we received against what we calculated before
	 * in the old version.
	 */
	/* 
	 * don't bother if we should be doing it ourself..
	 */
	if ((me & idle_cpus_mask) && (cpunum == NOCPU || me == (1 << cpunum)))
		return (0);

	dontuse = me | stopped_cpus | hlt_cpus_mask;
	map3 = 0;
	if (forward_wakeup_use_loop) {
		SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
			id = pc->pc_cpumask;
			if ( (id & dontuse) == 0 &&
			    pc->pc_curthread == pc->pc_idlethread) {
				map3 |= id;
			}
		}
	}

	if (forward_wakeup_use_mask) {
		map = 0;
		map = idle_cpus_mask & ~dontuse;

		/* If they are both on, compare and use loop if different */
		if (forward_wakeup_use_loop) {
			if (map != map3) {
				printf("map (%02X) != map3 (%02X)\n",
						map, map3);
				map = map3;
			}
		}
	} else {
		map = map3;
	}
	/* If we only allow a specific CPU, then mask off all the others */
	if (cpunum != NOCPU) {
		KASSERT((cpunum <= mp_maxcpus),("forward_wakeup: bad cpunum."));
		map &= (1 << cpunum);
	} else {
		/* Try choose an idle die. */
		if (forward_wakeup_use_htt) {
			map2 =  (map & (map >> 1)) & 0x5555;
			if (map2) {
				map = map2;
			}
		}

		/* set only one bit */ 
		if (forward_wakeup_use_single) {
			map = map & ((~map) + 1);
		}
	}
	if (map) {
		forward_wakeups_delivered++;
		ipi_selected(map, IPI_AST);
		return (1);
	}
	return (0);
}
#endif

void
sched_add(struct thread *td, int flags)
{
	struct kseq *ksq;
	struct ksegrp *kg;
	struct kse *ke;
	struct thread *mytd;
	int class;
	int nextrq;
	int need_resched = 0;
#ifdef SMP
	int cpu;
	int mycpu;
	int pinned;
	struct kseq *myksq;
#endif

	mtx_assert(&sched_lock, MA_OWNED);
	mytd = curthread;
	ke = td->td_kse;
	kg = td->td_ksegrp;
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	KASSERT(ke->ke_runq == NULL,
	    ("sched_add: KSE %p is still assigned to a run queue", ke));

	class = PRI_BASE(kg->kg_pri_class);
#ifdef SMP
	mycpu = PCPU_GET(cpuid);
	myksq = KSEQ_CPU(mycpu);
	ke->ke_wakeup_cpu = mycpu;
#endif
	nextrq = (ke->ke_flags & KEF_NEXTRQ);
	ke->ke_flags &= ~KEF_NEXTRQ;
	if (flags & SRQ_PREEMPTED)
		ke->ke_flags |= KEF_PREEMPTED;
	ksq = &kseq_global;
#ifdef SMP
	if (td->td_pinned != 0) {
		cpu = td->td_lastcpu;
		ksq = KSEQ_CPU(cpu);
		pinned = 1;
	} else if ((ke)->ke_flags & KEF_BOUND) {
		cpu = ke->ke_cpu;
		ksq = KSEQ_CPU(cpu);
		pinned = 1;
	} else {
		pinned = 0;
		cpu = NOCPU;
	}
#endif
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

#ifdef SMP
	if ((ke->ke_runq == kseq_global.ksq_curr ||
	     ke->ke_runq == myksq->ksq_curr) &&
	     td->td_priority < mytd->td_priority) {
#else
	if (ke->ke_runq == kseq_global.ksq_curr &&
	    td->td_priority < mytd->td_priority) {
#endif
		struct krunq *rq;

		rq = ke->ke_runq;
		ke->ke_runq = NULL;
		if ((flags & SRQ_YIELDING) == 0 && maybe_preempt(td))
			return;
		ke->ke_runq = rq;
		need_resched = TDF_NEEDRESCHED;
	}

	SLOT_USE(kg);
	ke->ke_state = KES_ONRUNQ;
	kseq_runq_add(ksq, ke);
	kseq_load_add(ksq, ke);

#ifdef SMP
	if (pinned) {
		if (cpu != mycpu) {
			struct thread *running = pcpu_find(cpu)->pc_curthread;
			if (ksq->ksq_curr == ke->ke_runq &&
			    running->td_priority < td->td_priority) {
				if (td->td_priority < PRI_MAX_ITHD)
					ipi_selected(1 << cpu, IPI_PREEMPT);
				else {
					running->td_flags |= TDF_NEEDRESCHED;
					ipi_selected(1 << cpu, IPI_AST);
				}
			}
		} else
			curthread->td_flags |= need_resched;
	} else {
		cpumask_t me = 1 << mycpu;
		cpumask_t idle = idle_cpus_mask & me;
		int forwarded = 0;

		if (!idle && ((flags & SRQ_INTR) == 0) &&
		    (idle_cpus_mask & ~(hlt_cpus_mask | me)))
			forwarded = forward_wakeup(cpu, me);
		if (forwarded == 0)
			curthread->td_flags |= need_resched;
	}
#else
	mytd->td_flags |= need_resched;
#endif
}

void
sched_rem(struct thread *td)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	KASSERT((ke->ke_state == KES_ONRUNQ),
	    ("sched_rem: KSE not on run queue"));

	kseq = ke->ke_kseq;
	SLOT_RELEASE(td->td_ksegrp);
	kseq_runq_rem(kseq, ke);
	kseq_load_rem(kseq, ke);
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
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	ke->ke_flags |= KEF_BOUND;
#ifdef SMP
	ke->ke_cpu = cpu;
	if (PCPU_GET(cpuid) == cpu)
		return;
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
	return (sched_tdcnt);
}

void
sched_relinquish(struct thread *td)
{
	struct ksegrp *kg;

	kg = td->td_ksegrp;
	mtx_lock_spin(&sched_lock);
	if (sched_is_timeshare(kg)) {
		sched_prio(td, PRI_MAX_TIMESHARE);
		td->td_kse->ke_flags |= KEF_NEXTRQ;
	}
	mi_switch(SW_VOL, NULL);
	mtx_unlock_spin(&sched_lock);
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
