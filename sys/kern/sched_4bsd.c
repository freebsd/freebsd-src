/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define kse td_sched

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <machine/smp.h>

/*
 * INVERSE_ESTCPU_WEIGHT is only suitable for statclock() frequencies in
 * the range 100-256 Hz (approximately).
 */
#define	ESTCPULIM(e) \
    min((e), INVERSE_ESTCPU_WEIGHT * (NICE_WEIGHT * (PRIO_MAX - PRIO_MIN) - \
    RQ_PPQ) + INVERSE_ESTCPU_WEIGHT - 1)
#ifdef SMP
#define	INVERSE_ESTCPU_WEIGHT	(8 * smp_cpus)
#else
#define	INVERSE_ESTCPU_WEIGHT	8	/* 1 / (priorities per estcpu level). */
#endif
#define	NICE_WEIGHT		1	/* Priorities per nice level. */

/*
 * The schedulable entity that can be given a context to run.
 * A process may have several of these. Probably one per processor
 * but posibly a few more. In this universe they are grouped
 * with a KSEG that contains the priority and niceness
 * for the group.
 */
struct kse {
	TAILQ_ENTRY(kse) ke_kglist;	/* (*) Queue of KSEs in ke_ksegrp. */
	TAILQ_ENTRY(kse) ke_kgrlist;	/* (*) Queue of KSEs in this state. */
	TAILQ_ENTRY(kse) ke_procq;	/* (j/z) Run queue. */
	struct thread	*ke_thread;	/* (*) Active associated thread. */
	fixpt_t		ke_pctcpu;	/* (j) %cpu during p_swtime. */
	char		ke_rqindex;	/* (j) Run queue index. */
	enum {
		KES_THREAD = 0x0,	/* slaved to thread state */
		KES_ONRUNQ
	} ke_state;			/* (j) KSE status. */
	int		ke_cpticks;	/* (j) Ticks of cpu time. */
	struct runq	*ke_runq;	/* runq the kse is currently on */
};

#define ke_proc		ke_thread->td_proc
#define ke_ksegrp	ke_thread->td_ksegrp

#define td_kse td_sched

/* flags kept in td_flags */
#define TDF_DIDRUN	TDF_SCHED0	/* KSE actually ran. */
#define TDF_EXIT	TDF_SCHED1	/* KSE is being killed. */
#define TDF_BOUND	TDF_SCHED2

#define ke_flags	ke_thread->td_flags
#define KEF_DIDRUN	TDF_DIDRUN /* KSE actually ran. */
#define KEF_EXIT	TDF_EXIT /* KSE is being killed. */
#define KEF_BOUND	TDF_BOUND /* stuck to one CPU */

#define SKE_RUNQ_PCPU(ke)						\
    ((ke)->ke_runq != 0 && (ke)->ke_runq != &runq)

struct kg_sched {
	struct thread	*skg_last_assigned; /* (j) Last thread assigned to */
					   /* the system scheduler. */
	int	skg_avail_opennings;	/* (j) Num KSEs requested in group. */
	int	skg_concurrency;	/* (j) Num KSEs requested in group. */
	int	skg_runq_kses;		/* (j) Num KSEs on runq. */
};
#define kg_last_assigned	kg_sched->skg_last_assigned
#define kg_avail_opennings	kg_sched->skg_avail_opennings
#define kg_concurrency		kg_sched->skg_concurrency
#define kg_runq_kses		kg_sched->skg_runq_kses

#define SLOT_RELEASE(kg)						\
do {									\
	kg->kg_avail_opennings++; 					\
	CTR3(KTR_RUNQ, "kg %p(%d) Slot released (->%d)",		\
	kg,								\
	kg->kg_concurrency,						\
	 kg->kg_avail_opennings);					\
/*	KASSERT((kg->kg_avail_opennings <= kg->kg_concurrency),		\
	    ("slots out of whack"));*/					\
} while (0)

#define SLOT_USE(kg)							\
do {									\
	kg->kg_avail_opennings--; 					\
	CTR3(KTR_RUNQ, "kg %p(%d) Slot used (->%d)",			\
	kg,								\
	kg->kg_concurrency,						\
	 kg->kg_avail_opennings);					\
/*	KASSERT((kg->kg_avail_opennings >= 0),				\
	    ("slots out of whack"));*/					\
} while (0)

/*
 * KSE_CAN_MIGRATE macro returns true if the kse can migrate between
 * cpus.
 */
#define KSE_CAN_MIGRATE(ke)						\
    ((ke)->ke_thread->td_pinned == 0 && ((ke)->ke_flags & KEF_BOUND) == 0)

static struct kse kse0;
static struct kg_sched kg_sched0;

static int	sched_tdcnt;	/* Total runnable threads in the system. */
static int	sched_quantum;	/* Roundrobin scheduling quantum in ticks. */
#define	SCHED_QUANTUM	(hz / 10)	/* Default sched quantum */

static struct callout roundrobin_callout;

static void	slot_fill(struct ksegrp *kg);
static struct kse *sched_choose(void);		/* XXX Should be thread * */

static void	setup_runqs(void);
static void	roundrobin(void *arg);
static void	schedcpu(void);
static void	schedcpu_thread(void);
static void	sched_setup(void *dummy);
static void	maybe_resched(struct thread *td);
static void	updatepri(struct ksegrp *kg);
static void	resetpriority(struct ksegrp *kg);
#ifdef SMP
static int	forward_wakeup(int  cpunum);
#endif

static struct kproc_desc sched_kp = {
        "schedcpu",
        schedcpu_thread,
        NULL
};
SYSINIT(schedcpu, SI_SUB_RUN_SCHEDULER, SI_ORDER_FIRST, kproc_start, &sched_kp)
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

/*
 * Global run queue.
 */
static struct runq runq;

#ifdef SMP
/*
 * Per-CPU run queues
 */
static struct runq runq_pcpu[MAXCPU];
#endif

static void
setup_runqs(void)
{
#ifdef SMP
	int i;

	for (i = 0; i < MAXCPU; ++i)
		runq_init(&runq_pcpu[i]);
#endif

	runq_init(&runq);
}

static int
sysctl_kern_quantum(SYSCTL_HANDLER_ARGS)
{
	int error, new_val;

	new_val = sched_quantum * tick;
	error = sysctl_handle_int(oidp, &new_val, 0, req);
        if (error != 0 || req->newptr == NULL)
		return (error);
	if (new_val < tick)
		return (EINVAL);
	sched_quantum = new_val / tick;
	hogticks = 2 * sched_quantum;
	return (0);
}

SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RD, 0, "Scheduler");

SYSCTL_STRING(_kern_sched, OID_AUTO, name, CTLFLAG_RD, "4BSD", 0,
    "Scheduler name");

SYSCTL_PROC(_kern_sched, OID_AUTO, quantum, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof sched_quantum, sysctl_kern_quantum, "I",
    "Roundrobin scheduling quantum in microseconds");

#ifdef SMP
/* Enable forwarding of wakeups to all other cpus */
SYSCTL_NODE(_kern_sched, OID_AUTO, ipiwakeup, CTLFLAG_RD, NULL, "Kernel SMP");

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
static int sched_followon = 0;
SYSCTL_INT(_kern_sched, OID_AUTO, followon, CTLFLAG_RW,
	   &sched_followon, 0,
	   "allow threads to share a quantum");

static int sched_pfollowons = 0;
SYSCTL_INT(_kern_sched, OID_AUTO, pfollowons, CTLFLAG_RD,
	   &sched_pfollowons, 0,
	   "number of followons done to a different ksegrp");

static int sched_kgfollowons = 0;
static __inline void
sched_load_add(void)
{
	sched_tdcnt++;
	CTR1(KTR_SCHED, "global load: %d", sched_tdcnt);
}

static __inline void
sched_load_rem(void)
{
	sched_tdcnt--;
	CTR1(KTR_SCHED, "global load: %d", sched_tdcnt);
}
SYSCTL_INT(_kern_sched, OID_AUTO, kgfollowons, CTLFLAG_RD,
	   &sched_kgfollowons, 0,
	   "number of followons done in a ksegrp");

/*
 * Arrange to reschedule if necessary, taking the priorities and
 * schedulers into account.
 */
static void
maybe_resched(struct thread *td)
{

	mtx_assert(&sched_lock, MA_OWNED);
	if (td->td_priority < curthread->td_priority)
		curthread->td_flags |= TDF_NEEDRESCHED;
}

/*
 * Force switch among equal priority processes every 100ms.
 * We don't actually need to force a context switch of the current process.
 * The act of firing the event triggers a context switch to softclock() and
 * then switching back out again which is equivalent to a preemption, thus
 * no further work is needed on the local CPU.
 */
/* ARGSUSED */
static void
roundrobin(void *arg)
{

#ifdef SMP
	mtx_lock_spin(&sched_lock);
	forward_roundrobin();
	mtx_unlock_spin(&sched_lock);
#endif

	callout_reset(&roundrobin_callout, sched_quantum, roundrobin, NULL);
}

/*
 * Constants for digital decay and forget:
 *	90% of (kg_estcpu) usage in 5 * loadav time
 *	95% of (ke_pctcpu) usage in 60 seconds (load insensitive)
 *          Note that, as ps(1) mentions, this can let percentages
 *          total over 100% (I've seen 137.9% for 3 processes).
 *
 * Note that schedclock() updates kg_estcpu and p_cpticks asynchronously.
 *
 * We wish to decay away 90% of kg_estcpu in (5 * loadavg) seconds.
 * That is, the system wants to compute a value of decay such
 * that the following for loop:
 * 	for (i = 0; i < (5 * loadavg); i++)
 * 		kg_estcpu *= decay;
 * will compute
 * 	kg_estcpu *= 0.1;
 * for all values of loadavg:
 *
 * Mathematically this loop can be expressed by saying:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * The system computes decay as:
 * 	decay = (2 * loadavg) / (2 * loadavg + 1)
 *
 * We wish to prove that the system's computation of decay
 * will always fulfill the equation:
 * 	decay ** (5 * loadavg) ~= .1
 *
 * If we compute b as:
 * 	b = 2 * loadavg
 * then
 * 	decay = b / (b + 1)
 *
 * We now need to prove two things:
 *	1) Given factor ** (5 * loadavg) ~= .1, prove factor == b/(b+1)
 *	2) Given b/(b+1) ** power ~= .1, prove power == (5 * loadavg)
 *
 * Facts:
 *         For x close to zero, exp(x) =~ 1 + x, since
 *              exp(x) = 0! + x**1/1! + x**2/2! + ... .
 *              therefore exp(-1/b) =~ 1 - (1/b) = (b-1)/b.
 *         For x close to zero, ln(1+x) =~ x, since
 *              ln(1+x) = x - x**2/2 + x**3/3 - ...     -1 < x < 1
 *              therefore ln(b/(b+1)) = ln(1 - 1/(b+1)) =~ -1/(b+1).
 *         ln(.1) =~ -2.30
 *
 * Proof of (1):
 *    Solve (factor)**(power) =~ .1 given power (5*loadav):
 *	solving for factor,
 *      ln(factor) =~ (-2.30/5*loadav), or
 *      factor =~ exp(-1/((5/2.30)*loadav)) =~ exp(-1/(2*loadav)) =
 *          exp(-1/b) =~ (b-1)/b =~ b/(b+1).                    QED
 *
 * Proof of (2):
 *    Solve (factor)**(power) =~ .1 given factor == (b/(b+1)):
 *	solving for power,
 *      power*ln(b/(b+1)) =~ -2.30, or
 *      power =~ 2.3 * (b + 1) = 4.6*loadav + 2.3 =~ 5*loadav.  QED
 *
 * Actual power values for the implemented algorithm are as follows:
 *      loadav: 1       2       3       4
 *      power:  5.68    10.32   14.94   19.55
 */

/* calculations for digital decay to forget 90% of usage in 5*loadav sec */
#define	loadfactor(loadav)	(2 * (loadav))
#define	decay_cpu(loadfac, cpu)	(((loadfac) * (cpu)) / ((loadfac) + FSCALE))

/* decay 95% of `ke_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
static fixpt_t	ccpu = 0.95122942450071400909 * FSCALE;	/* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

/*
 * If `ccpu' is not equal to `exp(-1/20)' and you still want to use the
 * faster/more-accurate formula, you'll have to estimate CCPU_SHIFT below
 * and possibly adjust FSHIFT in "param.h" so that (FSHIFT >= CCPU_SHIFT).
 *
 * To estimate CCPU_SHIFT for exp(-1/20), the following formula was used:
 *	1 - exp(-1/20) ~= 0.0487 ~= 0.0488 == 1 (fixed pt, *11* bits).
 *
 * If you don't want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	11

/*
 * Recompute process priorities, every hz ticks.
 * MP-safe, called without the Giant mutex.
 */
/* ARGSUSED */
static void
schedcpu(void)
{
	register fixpt_t loadfac = loadfactor(averunnable.ldavg[0]);
	struct thread *td;
	struct proc *p;
	struct kse *ke;
	struct ksegrp *kg;
	int awake, realstathz;

	realstathz = stathz ? stathz : hz;
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		/*
		 * Prevent state changes and protect run queue.
		 */
		mtx_lock_spin(&sched_lock);
		/*
		 * Increment time in/out of memory.  We ignore overflow; with
		 * 16-bit int's (remember them?) overflow takes 45 days.
		 */
		p->p_swtime++;
		FOREACH_KSEGRP_IN_PROC(p, kg) { 
			awake = 0;
			FOREACH_THREAD_IN_GROUP(kg, td) {
				ke = td->td_kse;
				/*
				 * Increment sleep time (if sleeping).  We
				 * ignore overflow, as above.
				 */
				/*
				 * The kse slptimes are not touched in wakeup
				 * because the thread may not HAVE a KSE.
				 */
				if (ke->ke_state == KES_ONRUNQ) {
					awake = 1;
					ke->ke_flags &= ~KEF_DIDRUN;
				} else if ((ke->ke_state == KES_THREAD) &&
				    (TD_IS_RUNNING(td))) {
					awake = 1;
					/* Do not clear KEF_DIDRUN */
				} else if (ke->ke_flags & KEF_DIDRUN) {
					awake = 1;
					ke->ke_flags &= ~KEF_DIDRUN;
				}

				/*
				 * ke_pctcpu is only for ps and ttyinfo().
				 * Do it per kse, and add them up at the end?
				 * XXXKSE
				 */
				ke->ke_pctcpu = (ke->ke_pctcpu * ccpu) >>
				    FSHIFT;
				/*
				 * If the kse has been idle the entire second,
				 * stop recalculating its priority until
				 * it wakes up.
				 */
				if (ke->ke_cpticks == 0)
					continue;
#if	(FSHIFT >= CCPU_SHIFT)
				ke->ke_pctcpu += (realstathz == 100)
				    ? ((fixpt_t) ke->ke_cpticks) <<
				    (FSHIFT - CCPU_SHIFT) :
				    100 * (((fixpt_t) ke->ke_cpticks)
				    << (FSHIFT - CCPU_SHIFT)) / realstathz;
#else
				ke->ke_pctcpu += ((FSCALE - ccpu) *
				    (ke->ke_cpticks *
				    FSCALE / realstathz)) >> FSHIFT;
#endif
				ke->ke_cpticks = 0;
			} /* end of kse loop */
			/* 
			 * If there are ANY running threads in this KSEGRP,
			 * then don't count it as sleeping.
			 */
			if (awake) {
				if (kg->kg_slptime > 1) {
					/*
					 * In an ideal world, this should not
					 * happen, because whoever woke us
					 * up from the long sleep should have
					 * unwound the slptime and reset our
					 * priority before we run at the stale
					 * priority.  Should KASSERT at some
					 * point when all the cases are fixed.
					 */
					updatepri(kg);
				}
				kg->kg_slptime = 0;
			} else
				kg->kg_slptime++;
			if (kg->kg_slptime > 1)
				continue;
			kg->kg_estcpu = decay_cpu(loadfac, kg->kg_estcpu);
		      	resetpriority(kg);
			FOREACH_THREAD_IN_GROUP(kg, td) {
				if (td->td_priority >= PUSER) {
					sched_prio(td, kg->kg_user_pri);
				}
			}
		} /* end of ksegrp loop */
		mtx_unlock_spin(&sched_lock);
	} /* end of process loop */
	sx_sunlock(&allproc_lock);
}

/*
 * Main loop for a kthread that executes schedcpu once a second.
 */
static void
schedcpu_thread(void)
{
	int nowake;

	for (;;) {
		schedcpu();
		tsleep(&nowake, curthread->td_priority, "-", hz);
	}
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max kg_estcpu of 255, sleeping for at
 * least six times the loadfactor will decay kg_estcpu to zero.
 */
static void
updatepri(struct ksegrp *kg)
{
	register fixpt_t loadfac;
	register unsigned int newcpu;

	loadfac = loadfactor(averunnable.ldavg[0]);
	if (kg->kg_slptime > 5 * loadfac)
		kg->kg_estcpu = 0;
	else {
		newcpu = kg->kg_estcpu;
		kg->kg_slptime--;	/* was incremented in schedcpu() */
		while (newcpu && --kg->kg_slptime)
			newcpu = decay_cpu(loadfac, newcpu);
		kg->kg_estcpu = newcpu;
	}
	resetpriority(kg);
}

/*
 * Compute the priority of a process when running in user mode.
 * Arrange to reschedule if the resulting priority is better
 * than that of the current process.
 */
static void
resetpriority(struct ksegrp *kg)
{
	register unsigned int newpriority;
	struct thread *td;

	if (kg->kg_pri_class == PRI_TIMESHARE) {
		newpriority = PUSER + kg->kg_estcpu / INVERSE_ESTCPU_WEIGHT +
		    NICE_WEIGHT * (kg->kg_proc->p_nice - PRIO_MIN);
		newpriority = min(max(newpriority, PRI_MIN_TIMESHARE),
		    PRI_MAX_TIMESHARE);
		kg->kg_user_pri = newpriority;
	}
	FOREACH_THREAD_IN_GROUP(kg, td) {
		maybe_resched(td);			/* XXXKSE silly */
	}
}

/* ARGSUSED */
static void
sched_setup(void *dummy)
{
	setup_runqs();

	if (sched_quantum == 0)
		sched_quantum = SCHED_QUANTUM;
	hogticks = 2 * sched_quantum;

	callout_init(&roundrobin_callout, CALLOUT_MPSAFE);

	/* Kick off timeout driven events by calling first time. */
	roundrobin(NULL);

	/* Account for thread0. */
	sched_load_add();
}

/* External interfaces start here */
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

int
sched_runnable(void)
{
#ifdef SMP
	return runq_check(&runq) + runq_check(&runq_pcpu[PCPU_GET(cpuid)]);
#else
	return runq_check(&runq);
#endif
}

int 
sched_rr_interval(void)
{
	if (sched_quantum == 0)
		sched_quantum = SCHED_QUANTUM;
	return (sched_quantum);
}

/*
 * We adjust the priority of the current process.  The priority of
 * a process gets worse as it accumulates CPU time.  The cpu usage
 * estimator (kg_estcpu) is increased here.  resetpriority() will
 * compute a different priority each time kg_estcpu increases by
 * INVERSE_ESTCPU_WEIGHT
 * (until MAXPRI is reached).  The cpu usage estimator ramps up
 * quite quickly when the process is running (linearly), and decays
 * away exponentially, at a rate which is proportionally slower when
 * the system is busy.  The basic principle is that the system will
 * 90% forget that the process used a lot of CPU time in 5 * loadav
 * seconds.  This causes the system to favor processes which haven't
 * run much recently, and to round-robin among other processes.
 */
void
sched_clock(struct thread *td)
{
	struct ksegrp *kg;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	kg = td->td_ksegrp;
	ke = td->td_kse;

	ke->ke_cpticks++;
	kg->kg_estcpu = ESTCPULIM(kg->kg_estcpu + 1);
	if ((kg->kg_estcpu % INVERSE_ESTCPU_WEIGHT) == 0) {
		resetpriority(kg);
		if (td->td_priority >= PUSER)
			td->td_priority = kg->kg_user_pri;
	}
}

/*
 * charge childs scheduling cpu usage to parent.
 *
 * XXXKSE assume only one thread & kse & ksegrp keep estcpu in each ksegrp.
 * Charge it to the ksegrp that did the wait since process estcpu is sum of
 * all ksegrps, this is strictly as expected.  Assume that the child process
 * aggregated all the estcpu into the 'built-in' ksegrp.
 */
void
sched_exit(struct proc *p, struct thread *td)
{
	sched_exit_ksegrp(FIRST_KSEGRP_IN_PROC(p), td);
	sched_exit_thread(FIRST_THREAD_IN_PROC(p), td);
}

void
sched_exit_ksegrp(struct ksegrp *kg, struct thread *childtd)
{

	mtx_assert(&sched_lock, MA_OWNED);
	kg->kg_estcpu = ESTCPULIM(kg->kg_estcpu + childtd->td_ksegrp->kg_estcpu);
}

void
sched_exit_thread(struct thread *td, struct thread *child)
{
	CTR3(KTR_SCHED, "sched_exit_thread: %p(%s) prio %d",
	    child, child->td_proc->p_comm, child->td_priority);
	if ((child->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_rem();
}

void
sched_fork(struct thread *td, struct thread *childtd)
{
	sched_fork_ksegrp(td, childtd->td_ksegrp);
	sched_fork_thread(td, childtd);
}

void
sched_fork_ksegrp(struct thread *td, struct ksegrp *child)
{
	mtx_assert(&sched_lock, MA_OWNED);
	child->kg_estcpu = td->td_ksegrp->kg_estcpu;
}

void
sched_fork_thread(struct thread *td, struct thread *childtd)
{
	sched_newthread(childtd);
}

void
sched_nice(struct proc *p, int nice)
{
	struct ksegrp *kg;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	p->p_nice = nice;
	FOREACH_KSEGRP_IN_PROC(p, kg) {
		resetpriority(kg);
	}
}

void
sched_class(struct ksegrp *kg, int class)
{
	mtx_assert(&sched_lock, MA_OWNED);
	kg->kg_pri_class = class;
}

/*
 * Adjust the priority of a thread.
 * This may include moving the thread within the KSEGRP,
 * changing the assignment of a kse to the thread,
 * and moving a KSE in the system run queue.
 */
void
sched_prio(struct thread *td, u_char prio)
{
	CTR6(KTR_SCHED, "sched_prio: %p(%s) prio %d newprio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, prio, curthread, 
	    curthread->td_proc->p_comm);

	mtx_assert(&sched_lock, MA_OWNED);
	if (TD_ON_RUNQ(td)) {
		adjustrunqueue(td, prio);
	} else {
		td->td_priority = prio;
	}
}

void
sched_sleep(struct thread *td)
{

	mtx_assert(&sched_lock, MA_OWNED);
	td->td_ksegrp->kg_slptime = 0;
	td->td_base_pri = td->td_priority;
}

static void remrunqueue(struct thread *td);

void
sched_switch(struct thread *td, struct thread *newtd, int flags)
{
	struct kse *ke;
	struct ksegrp *kg;
	struct proc *p;

	ke = td->td_kse;
	p = td->td_proc;

	mtx_assert(&sched_lock, MA_OWNED);

	if ((p->p_flag & P_NOLOAD) == 0)
		sched_load_rem();
	/* 
	 * We are volunteering to switch out so we get to nominate
	 * a successor for the rest of our quantum
	 * First try another thread in our ksegrp, and then look for 
	 * other ksegrps in our process.
	 */
	if (sched_followon &&
	    (p->p_flag & P_HADTHREADS) &&
	    (flags & SW_VOL) &&
	    newtd == NULL) {
		/* lets schedule another thread from this process */
		 kg = td->td_ksegrp;
		 if ((newtd = TAILQ_FIRST(&kg->kg_runq))) {
			remrunqueue(newtd);
			sched_kgfollowons++;
		 } else {
			FOREACH_KSEGRP_IN_PROC(p, kg) {
				if ((newtd = TAILQ_FIRST(&kg->kg_runq))) {
					sched_pfollowons++;
					remrunqueue(newtd);
					break;
				}
			}
		}
	}

	td->td_lastcpu = td->td_oncpu;
	td->td_flags &= ~TDF_NEEDRESCHED;
	td->td_pflags &= ~TDP_OWEPREEMPT;
	td->td_oncpu = NOCPU;
	/*
	 * At the last moment, if this thread is still marked RUNNING,
	 * then put it back on the run queue as it has not been suspended
	 * or stopped or any thing else similar.  We never put the idle
	 * threads on the run queue, however.
	 */
	if (td == PCPU_GET(idlethread))
		TD_SET_CAN_RUN(td);
	else {
		SLOT_RELEASE(td->td_ksegrp);
		if (TD_IS_RUNNING(td)) {
			/* Put us back on the run queue (kse and all). */
			setrunqueue(td, (flags & SW_PREEMPT) ?
			    SRQ_OURSELF|SRQ_YIELDING|SRQ_PREEMPTED :
			    SRQ_OURSELF|SRQ_YIELDING);
		} else if (p->p_flag & P_HADTHREADS) {
			/*
			 * We will not be on the run queue. So we must be
			 * sleeping or similar. As it's available,
			 * someone else can use the KSE if they need it.
			 * It's NOT available if we are about to need it
			 */
			if (newtd == NULL || newtd->td_ksegrp != td->td_ksegrp)
				slot_fill(td->td_ksegrp);
		}
	}
	if (newtd) {
		/* 
		 * The thread we are about to run needs to be counted
		 * as if it had been added to the run queue and selected.
		 * It came from:
		 * * A preemption
		 * * An upcall 
		 * * A followon
		 */
		KASSERT((newtd->td_inhibitors == 0),
			("trying to run inhibitted thread"));
		SLOT_USE(newtd->td_ksegrp);
		newtd->td_kse->ke_flags |= KEF_DIDRUN;
        	TD_SET_RUNNING(newtd);
		if ((newtd->td_proc->p_flag & P_NOLOAD) == 0)
			sched_load_add();
	} else {
		newtd = choosethread();
	}

	if (td != newtd)
		cpu_switch(td, newtd);
	sched_lock.mtx_lock = (uintptr_t)td;
	td->td_oncpu = PCPU_GET(cpuid);
}

void
sched_wakeup(struct thread *td)
{
	struct ksegrp *kg;

	mtx_assert(&sched_lock, MA_OWNED);
	kg = td->td_ksegrp;
	if (kg->kg_slptime > 1)
		updatepri(kg);
	kg->kg_slptime = 0;
	setrunqueue(td, SRQ_BORING);
}

#ifdef SMP
/* enable HTT_2 if you have a 2-way HTT cpu.*/
static int
forward_wakeup(int  cpunum)
{
	cpumask_t map, me, dontuse;
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
	me = PCPU_GET(cpumask);
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
	if (cpunum == NOCPU)
		printf("forward_wakeup: Idle processor not found\n");
	return (0);
}
#endif

#ifdef SMP
static void kick_other_cpu(int pri,int cpuid);

static void
kick_other_cpu(int pri,int cpuid)
{	
	struct pcpu * pcpu = pcpu_find(cpuid);
	int cpri = pcpu->pc_curthread->td_priority;

	if (idle_cpus_mask & pcpu->pc_cpumask) {
		forward_wakeups_delivered++;
		ipi_selected(pcpu->pc_cpumask, IPI_AST);
		return;
	}

	if (pri >= cpri)
		return;

#if defined(IPI_PREEMPTION) && defined(PREEMPTION)
#if !defined(FULL_PREEMPTION)
	if (pri <= PRI_MAX_ITHD)
#endif /* ! FULL_PREEMPTION */
	{
		ipi_selected(pcpu->pc_cpumask, IPI_PREEMPT);
		return;
	}
#endif /* defined(IPI_PREEMPTION) && defined(PREEMPTION) */

	pcpu->pc_curthread->td_flags |= TDF_NEEDRESCHED;
	ipi_selected( pcpu->pc_cpumask , IPI_AST);
	return;
}
#endif /* SMP */

void
sched_add(struct thread *td, int flags)
#ifdef SMP
{
	struct kse *ke;
	int forwarded = 0;
	int cpu;
	int single_cpu = 0;

	ke = td->td_kse;
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);


	if (td->td_pinned != 0) {
		cpu = td->td_lastcpu;
		ke->ke_runq = &runq_pcpu[cpu];
		single_cpu = 1;
		CTR3(KTR_RUNQ,
		    "sched_add: Put kse:%p(td:%p) on cpu%d runq", ke, td, cpu);
	} else if ((ke)->ke_flags & KEF_BOUND) {
		/* Find CPU from bound runq */
		KASSERT(SKE_RUNQ_PCPU(ke),("sched_add: bound kse not on cpu runq"));
		cpu = ke->ke_runq - &runq_pcpu[0];
		single_cpu = 1;
		CTR3(KTR_RUNQ,
		    "sched_add: Put kse:%p(td:%p) on cpu%d runq", ke, td, cpu);
	} else {	
		CTR2(KTR_RUNQ,
		    "sched_add: adding kse:%p (td:%p) to gbl runq", ke, td);
		cpu = NOCPU;
		ke->ke_runq = &runq;
	}
	
	if (single_cpu && (cpu != PCPU_GET(cpuid))) {
	        kick_other_cpu(td->td_priority,cpu);
	} else {
		
		if (!single_cpu) {
			cpumask_t me = PCPU_GET(cpumask);
			int idle = idle_cpus_mask & me;	

			if (!idle && ((flags & SRQ_INTR) == 0) &&
			    (idle_cpus_mask & ~(hlt_cpus_mask | me)))
				forwarded = forward_wakeup(cpu);
		}

		if (!forwarded) {
			if ((flags & SRQ_YIELDING) == 0 && maybe_preempt(td))
				return;
			else
				maybe_resched(td);
		}
	}
	
	if ((td->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_add();
	SLOT_USE(td->td_ksegrp);
	runq_add(ke->ke_runq, ke, flags);
	ke->ke_state = KES_ONRUNQ;
}
#else /* SMP */
{
	struct kse *ke;
	ke = td->td_kse;
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	CTR5(KTR_SCHED, "sched_add: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	CTR2(KTR_RUNQ, "sched_add: adding kse:%p (td:%p) to runq", ke, td);
	ke->ke_runq = &runq;

	/* 
	 * If we are yielding (on the way out anyhow) 
	 * or the thread being saved is US,
	 * then don't try be smart about preemption
	 * or kicking off another CPU
	 * as it won't help and may hinder.
	 * In the YIEDLING case, we are about to run whoever is 
	 * being put in the queue anyhow, and in the 
	 * OURSELF case, we are puting ourself on the run queue
	 * which also only happens when we are about to yield.
	 */
	if((flags & SRQ_YIELDING) == 0) {
		if (maybe_preempt(td))
			return;
	}	
	if ((td->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_add();
	SLOT_USE(td->td_ksegrp);
	runq_add(ke->ke_runq, ke, flags);
	ke->ke_ksegrp->kg_runq_kses++;
	ke->ke_state = KES_ONRUNQ;
	maybe_resched(td);
}
#endif /* SMP */

void
sched_rem(struct thread *td)
{
	struct kse *ke;

	ke = td->td_kse;
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_rem: process swapped out"));
	KASSERT((ke->ke_state == KES_ONRUNQ),
	    ("sched_rem: KSE not on run queue"));
	mtx_assert(&sched_lock, MA_OWNED);

	CTR5(KTR_SCHED, "sched_rem: %p(%s) prio %d by %p(%s)",
	    td, td->td_proc->p_comm, td->td_priority, curthread,
	    curthread->td_proc->p_comm);
	if ((td->td_proc->p_flag & P_NOLOAD) == 0)
		sched_load_rem();
	SLOT_RELEASE(td->td_ksegrp);
	runq_remove(ke->ke_runq, ke);

	ke->ke_state = KES_THREAD;
	td->td_ksegrp->kg_runq_kses--;
}

/*
 * Select threads to run.
 * Notice that the running threads still consume a slot.
 */
struct kse *
sched_choose(void)
{
	struct kse *ke;
	struct runq *rq;

#ifdef SMP
	struct kse *kecpu;

	rq = &runq;
	ke = runq_choose(&runq);
	kecpu = runq_choose(&runq_pcpu[PCPU_GET(cpuid)]);

	if (ke == NULL || 
	    (kecpu != NULL && 
	     kecpu->ke_thread->td_priority < ke->ke_thread->td_priority)) {
		CTR2(KTR_RUNQ, "choosing kse %p from pcpu runq %d", kecpu,
		     PCPU_GET(cpuid));
		ke = kecpu;
		rq = &runq_pcpu[PCPU_GET(cpuid)];
	} else { 
		CTR1(KTR_RUNQ, "choosing kse %p from main runq", ke);
	}

#else
	rq = &runq;
	ke = runq_choose(&runq);
#endif

	if (ke != NULL) {
		runq_remove(rq, ke);
		ke->ke_state = KES_THREAD;
		ke->ke_ksegrp->kg_runq_kses--;

		KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
		    ("sched_choose: process swapped out"));
	}
	return (ke);
}

void
sched_userret(struct thread *td)
{
	struct ksegrp *kg;
	/*
	 * XXX we cheat slightly on the locking here to avoid locking in
	 * the usual case.  Setting td_priority here is essentially an
	 * incomplete workaround for not setting it properly elsewhere.
	 * Now that some interrupt handlers are threads, not setting it
	 * properly elsewhere can clobber it in the window between setting
	 * it here and returning to user mode, so don't waste time setting
	 * it perfectly here.
	 */
	kg = td->td_ksegrp;
	if (td->td_priority != kg->kg_user_pri) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = kg->kg_user_pri;
		mtx_unlock_spin(&sched_lock);
	}
}

void
sched_bind(struct thread *td, int cpu)
{
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(TD_IS_RUNNING(td),
	    ("sched_bind: cannot bind non-running thread"));

	ke = td->td_kse;

	ke->ke_flags |= KEF_BOUND;
#ifdef SMP
	ke->ke_runq = &runq_pcpu[cpu];
	if (PCPU_GET(cpuid) == cpu)
		return;

	ke->ke_state = KES_THREAD;

	mi_switch(SW_VOL, NULL);
#endif
}

void
sched_unbind(struct thread* td)
{
	mtx_assert(&sched_lock, MA_OWNED);
	td->td_kse->ke_flags &= ~KEF_BOUND;
}

int
sched_load(void)
{
	return (sched_tdcnt);
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
	return (sizeof(struct thread) + sizeof(struct kse));
}

fixpt_t
sched_pctcpu(struct thread *td)
{
	struct kse *ke;

	ke = td->td_kse;
	return (ke->ke_pctcpu);

	return (0);
}
#define KERN_SWITCH_INCLUDE 1
#include "kern/kern_switch.c"
