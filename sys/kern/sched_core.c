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
#include <sys/umtx.h>
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

#define	CURRENT_SCORE(ts)	\
   (MAX_SCORE * NS_TO_HZ((ts)->ts_slptime) / MAX_SLEEP_TIME)

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
#define	INTERACTIVE_SCORE(ts)		\
    (PROC_NICE((ts)->ts_proc) * MAX_SCORE / 40 + INTERACTIVE_BASE_SCORE)

/* Test if a thread is an interactive thread */
#define	THREAD_IS_INTERACTIVE(ts)	\
    ((ts)->ts_thread->td_user_pri <=	\
	PROC_PRI((ts)->ts_proc) - INTERACTIVE_SCORE(ts))

/*
 * Calculate how long a thread must sleep to prove itself is an
 * interactive sleep.
 */
#define	INTERACTIVE_SLEEP_TIME(ts)	\
    (HZ_TO_NS(MAX_SLEEP_TIME *		\
	(MAX_SCORE / 2 + INTERACTIVE_SCORE((ts)) + 1) / MAX_SCORE - 1))

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
TAILQ_HEAD(krqhead, td_sched);

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
struct td_sched {
	struct thread	*ts_thread;	/* (*) Active associated thread. */
	TAILQ_ENTRY(td_sched) ts_procq;	/* (j/z) Run queue. */
	int		ts_flags;	/* (j) TSF_* flags. */
	fixpt_t		ts_pctcpu;	/* (j) %cpu during p_swtime. */
	u_char		ts_rqindex;	/* (j) Run queue index. */
	int		ts_slice;	/* Time slice in ticks */
	struct kseq	*ts_kseq;	/* Kseq the thread belongs to */
	struct krunq	*ts_runq;	/* Assiociated runqueue */
#ifdef SMP
	int		ts_cpu;		/* CPU that we have affinity for. */
	int		ts_wakeup_cpu;	/* CPU that has activated us. */
#endif
	int		ts_activated;	/* How is the thread activated. */
	uint64_t	ts_timestamp;	/* Last timestamp dependent on state.*/
	unsigned	ts_lastran;	/* Last timestamp the thread ran. */

	/* The following variables are only used for pctcpu calculation */
	int		ts_ltick;	/* Last tick that we were running on */
	int		ts_ftick;	/* First tick that we were running on */
	int		ts_ticks;	/* Tick count */

	u_long		ts_slptime;	/* (j) Number of ticks we vol. slept */
	u_long		ts_runtime;	/* (j) Temp total run time. */
};

#define	td_sched	td_sched
#define ts_proc		ts_thread->td_proc

/* flags kept in ts_flags */
#define	TSF_BOUND	0x0001		/* Thread can not migrate. */
#define	TSF_PREEMPTED	0x0002		/* Thread was preempted. */
#define TSF_MIGRATING	0x0004		/* Thread is migrating. */
#define	TSF_SLEEP	0x0008		/* Thread did sleep. */
#define	TSF_DIDRUN	0x0010		/* Thread actually ran. */
#define	TSF_EXIT	0x0020		/* Thread is being killed. */
#define TSF_NEXTRQ	0x0400		/* Thread should be in next queue. */
#define TSF_FIRST_SLICE	0x0800		/* Thread has first time slice left. */

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

static struct td_sched kse0;

static int min_timeslice = 5;
static int def_timeslice = 100;
static int granularity = 10;
static int realstathz;
static int sched_tdcnt;
static struct kseq kseq_global;

/*
 * One td_sched queue per processor.
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

static void krunq_add(struct krunq *, struct td_sched *);
static struct td_sched *krunq_choose(struct krunq *);
static void krunq_clrbit(struct krunq *rq, int pri);
static int krunq_findbit(struct krunq *rq);
static void krunq_init(struct krunq *);
static void krunq_remove(struct krunq *, struct td_sched *);

static struct td_sched * kseq_choose(struct kseq *);
static void kseq_load_add(struct kseq *, struct td_sched *);
static void kseq_load_rem(struct kseq *, struct td_sched *);
static void kseq_runq_add(struct kseq *, struct td_sched *);
static void kseq_runq_rem(struct kseq *, struct td_sched *);
static void kseq_setup(struct kseq *);

static int sched_is_timeshare(struct thread *td);
static int sched_calc_pri(struct td_sched *ts);
static int sched_starving(struct kseq *, unsigned, struct td_sched *);
static void sched_pctcpu_update(struct td_sched *);
static void sched_thread_priority(struct thread *, u_char);
static uint64_t	sched_timestamp(void);
static int sched_recalc_pri(struct td_sched *ts, uint64_t now);
static int sched_timeslice(struct td_sched *ts);
static void sched_update_runtime(struct td_sched *ts, uint64_t now);
static void sched_commit_runtime(struct td_sched *ts);

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
krunq_add(struct krunq *rq, struct td_sched *ts)
{
	struct krqhead *rqh;
	int pri;

	pri = ts->ts_thread->td_priority;
	ts->ts_rqindex = pri;
	krunq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	if (ts->ts_flags & TSF_PREEMPTED)
		TAILQ_INSERT_HEAD(rqh, ts, ts_procq);
	else
		TAILQ_INSERT_TAIL(rqh, ts, ts_procq);
}

/*
 * Find the highest priority process on the run queue.
 */
static struct td_sched *
krunq_choose(struct krunq *rq)
{
	struct krqhead *rqh;
	struct td_sched *ts;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	if ((pri = krunq_findbit(rq)) != -1) {
		rqh = &rq->rq_queues[pri];
		ts = TAILQ_FIRST(rqh);
		KASSERT(ts != NULL, ("krunq_choose: no thread on busy queue"));
#ifdef SMP
		if (pri <= PRI_MAX_ITHD || runq_fuzz <= 0)
			return (ts);

		/*
		 * In the first couple of entries, check if
		 * there is one for our CPU as a preference.
		 */
		struct td_sched *ts2 = ts;
		const int mycpu = PCPU_GET(cpuid);
		const int mymask = 1 << mycpu;
		int count = runq_fuzz;

		while (count-- && ts2) {
			const int cpu = ts2->ts_wakeup_cpu;
			if (cpu_sibling[cpu] & mymask) {
				ts = ts2;
				break;
			}
			ts2 = TAILQ_NEXT(ts2, ts_procq);
		}
#endif
		return (ts);
	}

	return (NULL);
}

/*
 * Remove the KSE from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 * Caller must set state afterwards.
 */
static void
krunq_remove(struct krunq *rq, struct td_sched *ts)
{
	struct krqhead *rqh;
	int pri;

	KASSERT(ts->ts_proc->p_sflag & PS_INMEM,
		("runq_remove: process swapped out"));
	pri = ts->ts_rqindex;
	rqh = &rq->rq_queues[pri];
	KASSERT(ts != NULL, ("krunq_remove: no proc on busy queue"));
	TAILQ_REMOVE(rqh, ts, ts_procq);
	if (TAILQ_EMPTY(rqh))
		krunq_clrbit(rq, pri);
}

static inline void
kseq_runq_add(struct kseq *kseq, struct td_sched *ts)
{
	krunq_add(ts->ts_runq, ts);
	ts->ts_kseq = kseq;
}

static inline void
kseq_runq_rem(struct kseq *kseq, struct td_sched *ts)
{
	krunq_remove(ts->ts_runq, ts);
	ts->ts_kseq = NULL;
	ts->ts_runq = NULL;
}

static inline void
kseq_load_add(struct kseq *kseq, struct td_sched *ts)
{
	kseq->ksq_load++;
	if ((ts->ts_proc->p_flag & P_NOLOAD) == 0)
		sched_tdcnt++;
}

static inline void
kseq_load_rem(struct kseq *kseq, struct td_sched *ts)
{
	kseq->ksq_load--;
	if ((ts->ts_proc->p_flag & P_NOLOAD) == 0)
		sched_tdcnt--;
}

/*
 * Pick the highest priority task we have and return it.
 */
static struct td_sched *
kseq_choose(struct kseq *kseq)
{
	struct krunq *swap;
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	ts = krunq_choose(kseq->ksq_curr);
	if (ts != NULL)
		return (ts);

	kseq->ksq_expired_nice = PRIO_MAX + 1;
	kseq->ksq_expired_tick = 0;
	swap = kseq->ksq_curr;
	kseq->ksq_curr = kseq->ksq_next;
	kseq->ksq_next = swap;
	ts = krunq_choose(kseq->ksq_curr);
	if (ts != NULL)
		return (ts);

	return krunq_choose(&kseq->ksq_idle);
}

static inline uint64_t
sched_timestamp(void)
{
	uint64_t now = cputick2usec(cpu_ticks()) * 1000;
	return (now);
}

static inline int
sched_timeslice(struct td_sched *ts)
{
	struct proc *p = ts->ts_proc;

	if (ts->ts_proc->p_nice < 0)
		return SCALE_USER_PRI(def_timeslice*4, PROC_USER_PRI(p));
        else
		return SCALE_USER_PRI(def_timeslice, PROC_USER_PRI(p));
}

static inline int
sched_is_timeshare(struct thread *td)
{
	return (td->td_pri_class == PRI_TIMESHARE);
}

static int
sched_calc_pri(struct td_sched *ts)
{
	int score, pri;

	if (sched_is_timeshare(ts->ts_thread)) {
		score = CURRENT_SCORE(ts) - MAX_SCORE / 2;
		pri = PROC_PRI(ts->ts_proc) - score;
		if (pri < PUSER)
			pri = PUSER;
		else if (pri > PUSER_MAX)
			pri = PUSER_MAX;
		return (pri);
	}
	return (ts->ts_thread->td_base_user_pri);
}

static int
sched_recalc_pri(struct td_sched *ts, uint64_t now)
{
	uint64_t	delta;
	unsigned int	sleep_time;

	delta = now - ts->ts_timestamp;
	if (__predict_false(!sched_is_timeshare(ts->ts_thread)))
		return (ts->ts_thread->td_base_user_pri);

	if (delta > NS_MAX_SLEEP_TIME)
		sleep_time = NS_MAX_SLEEP_TIME;
	else
		sleep_time = (unsigned int)delta;
	if (__predict_false(sleep_time == 0))
		goto out;

	if (ts->ts_activated != -1 &&
	    sleep_time > INTERACTIVE_SLEEP_TIME(ts)) {
		ts->ts_slptime = HZ_TO_NS(MAX_SLEEP_TIME - def_timeslice);
	} else {
		sleep_time *= (MAX_SCORE - CURRENT_SCORE(ts)) ? : 1;

		/*
		 * If thread is waking from uninterruptible sleep, it is
		 * unlikely an interactive sleep, limit its sleep time to
		 * prevent it from being an interactive thread.
		 */
		if (ts->ts_activated == -1) {
			if (ts->ts_slptime >= INTERACTIVE_SLEEP_TIME(ts))
				sleep_time = 0;
			else if (ts->ts_slptime + sleep_time >=
				INTERACTIVE_SLEEP_TIME(ts)) {
				ts->ts_slptime = INTERACTIVE_SLEEP_TIME(ts);
				sleep_time = 0;
			}
		}

		/*
		 * Thread gets priority boost here.
                 */
		ts->ts_slptime += sleep_time;

		/* Sleep time should never be larger than maximum */
		if (ts->ts_slptime > NS_MAX_SLEEP_TIME)
			ts->ts_slptime = NS_MAX_SLEEP_TIME;
	}

out:
	return (sched_calc_pri(ts));
}

static void
sched_update_runtime(struct td_sched *ts, uint64_t now)
{
	uint64_t runtime;

	if (sched_is_timeshare(ts->ts_thread)) {
		if ((int64_t)(now - ts->ts_timestamp) < NS_MAX_SLEEP_TIME) {
			runtime = now - ts->ts_timestamp;
			if ((int64_t)(now - ts->ts_timestamp) < 0)
				runtime = 0;
		} else {
			runtime = NS_MAX_SLEEP_TIME;
		}
		runtime /= (CURRENT_SCORE(ts) ? : 1);
		ts->ts_runtime += runtime;
		ts->ts_timestamp = now;
	}
}

static void
sched_commit_runtime(struct td_sched *ts)
{

	if (ts->ts_runtime > ts->ts_slptime)
		ts->ts_slptime = 0;
	else
		ts->ts_slptime -= ts->ts_runtime;
	ts->ts_runtime = 0;
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
	thread0.td_sched = &kse0;
	thread0.td_lock = &sched_lock;
	kse0.ts_thread = &thread0;
	kse0.ts_slice = 100;
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

static void
sched_thread_priority(struct thread *td, u_char prio)
{
	struct td_sched *ts;

	ts = td->td_sched;
	mtx_assert(&sched_lock, MA_OWNED);
	if (__predict_false(td->td_priority == prio))
		return;

	if (TD_ON_RUNQ(td)) {
		/*
		 * If the priority has been elevated due to priority
		 * propagation, we may have to move ourselves to a new
		 * queue.  We still call adjustrunqueue below in case td_sched
		 * needs to fix things up.
		 *
		 * XXX td_priority is never set here.
		 */
		if (prio < td->td_priority && ts->ts_runq != NULL &&
		    ts->ts_runq != ts->ts_kseq->ksq_curr) {
			krunq_remove(ts->ts_runq, ts);
			ts->ts_runq = ts->ts_kseq->ksq_curr;
			krunq_add(ts->ts_runq, ts);
		}
		if (ts->ts_rqindex != prio) {
			sched_rem(td);
			td->td_priority = prio;
			sched_add(td, SRQ_BORING);
		}
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

	if (td->td_pri_class == PRI_TIMESHARE)
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
	struct kseq *ksq;
	struct td_sched *ts;
	uint64_t now;

	mtx_assert(&sched_lock, MA_OWNED);

	now = sched_timestamp();
	ts = td->td_sched;
	ksq = KSEQ_SELF();

	td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_owepreempt = 0;

	if (TD_IS_IDLETHREAD(td)) {
		TD_SET_CAN_RUN(td);
	} else {
		sched_update_runtime(ts, now);
		/* We are ending our run so make our slot available again */
		kseq_load_rem(ksq, ts);
		if (TD_IS_RUNNING(td)) {
			sched_add(td, (flags & SW_PREEMPT) ?
			    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
			    SRQ_OURSELF|SRQ_YIELDING);
		} else {
			ts->ts_flags &= ~TSF_NEXTRQ;
		}
	}

	if (newtd != NULL) {
		/*
		 * If we bring in a thread account for it as if it had been
		 * added to the run queue and then chosen.
		 */
		newtd->td_sched->ts_flags |= TSF_DIDRUN;
		newtd->td_sched->ts_timestamp = now;
		TD_SET_RUNNING(newtd);
		kseq_load_add(ksq, newtd->td_sched);
	} else {
		newtd = choosethread();
		/* sched_choose sets ts_timestamp, just reuse it */
	}
	if (td != newtd) {
		ts->ts_lastran = tick;

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
}

void
sched_nice(struct proc *p, int nice)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	p->p_nice = nice;
	FOREACH_THREAD_IN_PROC(p, td) {
		if (td->td_pri_class == PRI_TIMESHARE) {
			sched_user_prio(td, sched_calc_pri(td->td_sched));
			td->td_flags |= TDF_NEEDRESCHED;
		}
	}
}

void
sched_sleep(struct thread *td)
{
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	ts = td->td_sched;
	if (td->td_flags & TDF_SINTR)
		ts->ts_activated = 0;
	else
		ts->ts_activated = -1;
	ts->ts_flags |= TSF_SLEEP;
}

void
sched_wakeup(struct thread *td)
{
	struct td_sched *ts;
	struct kseq *kseq, *mykseq;
	uint64_t now;

	mtx_assert(&sched_lock, MA_OWNED);
	ts = td->td_sched;
	mykseq = KSEQ_SELF();
	if (ts->ts_flags & TSF_SLEEP) {
		ts->ts_flags &= ~TSF_SLEEP;
		if (sched_is_timeshare(td)) {
			sched_commit_runtime(ts);
			now = sched_timestamp();
			kseq = KSEQ_CPU(td->td_lastcpu);
#ifdef SMP
			if (kseq != mykseq)
				now = now - mykseq->ksq_last_timestamp +
				    kseq->ksq_last_timestamp;
#endif
			sched_user_prio(td, sched_recalc_pri(ts, now));
		}
	}
	sched_add(td, SRQ_BORING);
}

/*
 * Penalize the parent for creating a new child and initialize the child's
 * priority.
 */
void
sched_fork(struct thread *td, struct thread *childtd)
{

	mtx_assert(&sched_lock, MA_OWNED);
	sched_fork_thread(td, childtd);
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
	struct td_sched *ts;
	struct td_sched *ts2;

	sched_newthread(child);

	ts = td->td_sched;
	ts2 = child->td_sched;

	child->td_lock = td->td_lock;
	ts2->ts_slptime = ts2->ts_slptime * CHILD_WEIGHT / 100;
	if (child->td_pri_class == PRI_TIMESHARE)
		sched_user_prio(child, sched_calc_pri(ts2));
	ts->ts_slptime = ts->ts_slptime * PARENT_WEIGHT / 100;
	ts2->ts_slice = (ts->ts_slice + 1) >> 1;
	ts2->ts_flags |= TSF_FIRST_SLICE | (ts->ts_flags & TSF_NEXTRQ);
	ts2->ts_activated = 0;
	ts->ts_slice >>= 1;
        if (ts->ts_slice == 0) {
		ts->ts_slice = 1;
		sched_tick();
	}

	/* Grab our parents cpu estimation information. */
	ts2->ts_ticks = ts->ts_ticks;
	ts2->ts_ltick = ts->ts_ltick;
	ts2->ts_ftick = ts->ts_ftick;
}

void
sched_class(struct thread *td, int class)
{
	mtx_assert(&sched_lock, MA_OWNED);
	td->td_pri_class = class;
}

/*
 * Return some of the child's priority and interactivity to the parent.
 */
void
sched_exit(struct proc *p, struct thread *childtd)
{

	PROC_SLOCK_ASSERT(p, MA_OWNED);
	sched_exit_thread(FIRST_THREAD_IN_PROC(p), childtd);
}

void
sched_exit_thread(struct thread *td, struct thread *childtd)
{
	struct td_sched *childke  = childtd->td_sched;
	struct td_sched *parentke = td->td_sched;

	if (childke->ts_slptime < parentke->ts_slptime) {
		parentke->ts_slptime = parentke->ts_slptime /
			(EXIT_WEIGHT) * (EXIT_WEIGHT - 1) +
			parentke->ts_slptime / EXIT_WEIGHT;
	}

	kseq_load_rem(KSEQ_SELF(), childke);
	sched_update_runtime(childke, sched_timestamp());
	sched_commit_runtime(childke);
	if ((childke->ts_flags & TSF_FIRST_SLICE) &&
	    td->td_proc == childtd->td_proc->p_pptr) {
		parentke->ts_slice += childke->ts_slice;
		if (parentke->ts_slice > sched_timeslice(parentke))
			parentke->ts_slice = sched_timeslice(parentke);
	}
}

static int
sched_starving(struct kseq *ksq, unsigned now, struct td_sched *ts)
{
	uint64_t delta;

	if (ts->ts_proc->p_nice > ksq->ksq_expired_nice)
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
sched_timeslice_split(struct td_sched *ts)
{
	int score, g;

	score = (int)(MAX_SCORE - CURRENT_SCORE(ts));
	if (score == 0)
		score = 1;
#ifdef SMP
	g = granularity * ((1 << score) - 1) * smp_cpus;
#else
	g = granularity * ((1 << score) - 1);
#endif
	return (ts->ts_slice >= g && ts->ts_slice % g == 0);
}

void
sched_tick(void)
{
	struct thread	*td;
	struct proc	*p;
	struct td_sched	*ts;
	struct kseq	*kseq;
	uint64_t	now;
	int		cpuid;
	int		class;
	
	mtx_assert(&sched_lock, MA_OWNED);

	td = curthread;
	ts = td->td_sched;
	p = td->td_proc;
	class = PRI_BASE(td->td_pri_class);
	now = sched_timestamp();
	cpuid = PCPU_GET(cpuid);
	kseq = KSEQ_CPU(cpuid);
	kseq->ksq_last_timestamp = now;

	if (class == PRI_IDLE) {
		/*
		 * Processes of equal idle priority are run round-robin.
		 */
		if (!TD_IS_IDLETHREAD(td) && --ts->ts_slice <= 0) {
			ts->ts_slice = def_timeslice;
			td->td_flags |= TDF_NEEDRESCHED;
		}
		return;
	}

	if (class == PRI_REALTIME) {
		/*
		 * Realtime scheduling, do round robin for RR class, FIFO
		 * is not affected.
		 */
		if (PRI_NEED_RR(td->td_pri_class) && --ts->ts_slice <= 0) {
			ts->ts_slice = def_timeslice;
			td->td_flags |= TDF_NEEDRESCHED;
		}
		return;
	}

	/*
	 * We skip kernel thread, though it may be classified as TIMESHARE.
	 */
	if (class != PRI_TIMESHARE || (p->p_flag & P_KTHREAD) != 0)
		return;

	if (--ts->ts_slice <= 0) {
		td->td_flags |= TDF_NEEDRESCHED;
		sched_update_runtime(ts, now);
		sched_commit_runtime(ts);
		sched_user_prio(td, sched_calc_pri(ts));
		ts->ts_slice = sched_timeslice(ts);
		ts->ts_flags &= ~TSF_FIRST_SLICE;
		if (ts->ts_flags & TSF_BOUND || td->td_pinned) {
			if (kseq->ksq_expired_tick == 0)
				kseq->ksq_expired_tick = tick;
		} else {
			if (kseq_global.ksq_expired_tick == 0)
				kseq_global.ksq_expired_tick = tick;
		}
		if (!THREAD_IS_INTERACTIVE(ts) ||
		    sched_starving(kseq, tick, ts) ||
		    sched_starving(&kseq_global, tick, ts)) {
			/* The thead becomes cpu hog, schedule it off. */
			ts->ts_flags |= TSF_NEXTRQ;
			if (ts->ts_flags & TSF_BOUND || td->td_pinned) {
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
		if (THREAD_IS_INTERACTIVE(ts) && sched_timeslice_split(ts))
			td->td_flags |= TDF_NEEDRESCHED;
	}
}

void
sched_clock(struct thread *td)
{
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	ts = td->td_sched;

	/* Adjust ticks for pctcpu */
	ts->ts_ticks++;
	ts->ts_ltick = ticks;

	/* Go up to one second beyond our max and then trim back down */
	if (ts->ts_ftick + SCHED_CPU_TICKS + hz < ts->ts_ltick)
		sched_pctcpu_update(ts);
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

	KASSERT((td->td_flags & TDF_BORROWING) == 0,
	    ("thread with borrowed priority returning to userland"));
	if (td->td_priority != td->td_user_pri) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = td->td_user_pri;
		td->td_base_pri = td->td_user_pri;
		mtx_unlock_spin(&sched_lock);
	}
}

struct thread *
sched_choose(void)
{
	struct td_sched  *ts;
	struct kseq *kseq;

#ifdef SMP
	struct td_sched *kecpu;

	mtx_assert(&sched_lock, MA_OWNED);
	kseq = &kseq_global;
	ts = kseq_choose(&kseq_global);
	kecpu = kseq_choose(KSEQ_SELF());

	if (ts == NULL || 
	    (kecpu != NULL && 
	     kecpu->ts_thread->td_priority < ts->ts_thread->td_priority)) {
		ts = kecpu;
		kseq = KSEQ_SELF();
	}
#else
	kseq = &kseq_global;
	ts = kseq_choose(kseq);
#endif

	if (ts != NULL) {
		kseq_runq_rem(kseq, ts);
		ts->ts_flags &= ~TSF_PREEMPTED;
		ts->ts_timestamp = sched_timestamp();
		return (ts->ts_thread);
	}
	return (PCPU_GET(idlethread));
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
	struct td_sched *ts;
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
	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	KASSERT((td->td_inhibitors == 0),
	    ("sched_add: trying to run inhibited thread"));
	KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	    ("sched_add: bad thread state"));
	TD_SET_RUNQ(td);
	mytd = curthread;
	ts = td->td_sched;
	KASSERT(ts->ts_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	KASSERT(ts->ts_runq == NULL,
	    ("sched_add: KSE %p is still assigned to a run queue", ts));

	class = PRI_BASE(td->td_pri_class);
#ifdef SMP
	mycpu = PCPU_GET(cpuid);
	myksq = KSEQ_CPU(mycpu);
	ts->ts_wakeup_cpu = mycpu;
#endif
	nextrq = (ts->ts_flags & TSF_NEXTRQ);
	ts->ts_flags &= ~TSF_NEXTRQ;
	if (flags & SRQ_PREEMPTED)
		ts->ts_flags |= TSF_PREEMPTED;
	ksq = &kseq_global;
#ifdef SMP
	if (td->td_pinned != 0) {
		cpu = td->td_lastcpu;
		ksq = KSEQ_CPU(cpu);
		pinned = 1;
	} else if ((ts)->ts_flags & TSF_BOUND) {
		cpu = ts->ts_cpu;
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
		ts->ts_runq = ksq->ksq_curr;
		break;
	case PRI_TIMESHARE:
		if ((td->td_flags & TDF_BORROWING) == 0 && nextrq)
			ts->ts_runq = ksq->ksq_next;
		else
			ts->ts_runq = ksq->ksq_curr;
		break;
	case PRI_IDLE:
		/*
		 * This is for priority prop.
		 */
		if (td->td_priority < PRI_MIN_IDLE)
			ts->ts_runq = ksq->ksq_curr;
		else
			ts->ts_runq = &ksq->ksq_idle;
		break;
	default:
		panic("Unknown pri class.");
		break;
	}

#ifdef SMP
	if ((ts->ts_runq == kseq_global.ksq_curr ||
	     ts->ts_runq == myksq->ksq_curr) &&
	     td->td_priority < mytd->td_priority) {
#else
	if (ts->ts_runq == kseq_global.ksq_curr &&
	    td->td_priority < mytd->td_priority) {
#endif
		struct krunq *rq;

		rq = ts->ts_runq;
		ts->ts_runq = NULL;
		if ((flags & SRQ_YIELDING) == 0 && maybe_preempt(td))
			return;
		ts->ts_runq = rq;
		need_resched = TDF_NEEDRESCHED;
	}

	kseq_runq_add(ksq, ts);
	kseq_load_add(ksq, ts);

#ifdef SMP
	if (pinned) {
		if (cpu != mycpu) {
			struct thread *running = pcpu_find(cpu)->pc_curthread;
			if (ksq->ksq_curr == ts->ts_runq &&
			    running->td_priority < td->td_priority) {
				if (td->td_priority <= PRI_MAX_ITHD)
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
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	ts = td->td_sched;
	KASSERT(TD_ON_RUNQ(td),
	    ("sched_rem: KSE not on run queue"));

	kseq = ts->ts_kseq;
	kseq_runq_rem(kseq, ts);
	kseq_load_rem(kseq, ts);
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
		rtick = MIN(ts->ts_ticks / SCHED_CPU_TIME, SCHED_CPU_TICKS);
		pctcpu = (FSCALE * ((FSCALE * rtick)/realstathz)) >> FSHIFT;
	}

	ts->ts_proc->p_swtime = ts->ts_ltick - ts->ts_ftick;
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
	ts->ts_cpu = cpu;
	if (PCPU_GET(cpuid) == cpu)
		return;
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

int
sched_load(void)
{
	return (sched_tdcnt);
}

void
sched_relinquish(struct thread *td)
{

	mtx_lock_spin(&sched_lock);
	if (sched_is_timeshare(td)) {
		sched_prio(td, PRI_MAX_TIMESHARE);
		td->td_sched->ts_flags |= TSF_NEXTRQ;
	}
	mi_switch(SW_VOL, NULL);
	mtx_unlock_spin(&sched_lock);
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
	struct proc *p;
	struct thread *td;
#ifdef SMP
	cpumask_t mycpu;
#endif

	td = curthread;
	p = td->td_proc;
#ifdef SMP
	mycpu = PCPU_GET(cpumask);
	mtx_lock_spin(&sched_lock);
	idle_cpus_mask |= mycpu;
	mtx_unlock_spin(&sched_lock);
#endif
	for (;;) {
		mtx_assert(&Giant, MA_NOTOWNED);

		while (sched_runnable() == 0)
			cpu_idle();

		mtx_lock_spin(&sched_lock);
#ifdef SMP
		idle_cpus_mask &= ~mycpu;
#endif
		mi_switch(SW_VOL, NULL);
#ifdef SMP
		idle_cpus_mask |= mycpu;
#endif
		mtx_unlock_spin(&sched_lock);
	}
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

#define KERN_SWITCH_INCLUDE 1
#include "kern/kern_switch.c"
