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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sx.h>

/*
 * INVERSE_ESTCPU_WEIGHT is only suitable for statclock() frequencies in
 * the range 100-256 Hz (approximately).
 */
#define	ESTCPULIM(e) \
    min((e), INVERSE_ESTCPU_WEIGHT * (NICE_WEIGHT * (PRIO_MAX - PRIO_MIN) - \
    RQ_PPQ) + INVERSE_ESTCPU_WEIGHT - 1)
#define	INVERSE_ESTCPU_WEIGHT	8	/* 1 / (priorities per estcpu level). */
#define	NICE_WEIGHT		1	/* Priorities per nice level. */

struct ke_sched {
	int	ske_cpticks;	/* (j) Ticks of cpu time. */
};

static struct ke_sched ke_sched;

struct ke_sched *kse0_sched = &ke_sched;
struct kg_sched *ksegrp0_sched = NULL;
struct p_sched *proc0_sched = NULL;
struct td_sched *thread0_sched = NULL;

static int	sched_quantum;	/* Roundrobin scheduling quantum in ticks. */
#define	SCHED_QUANTUM	(hz / 10)	/* Default sched quantum */

static struct callout schedcpu_callout;
static struct callout roundrobin_callout;

static void	roundrobin(void *arg);
static void	schedcpu(void *arg);
static void	sched_setup(void *dummy);
static void	maybe_resched(struct thread *td);
static void	updatepri(struct ksegrp *kg);
static void	resetpriority(struct ksegrp *kg);

SYSINIT(sched_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, sched_setup, NULL)

/*
 * Global run queue.
 */
static struct runq runq;
SYSINIT(runq, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, runq_init, &runq)

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

SYSCTL_PROC(_kern, OID_AUTO, quantum, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof sched_quantum, sysctl_kern_quantum, "I",
	"Roundrobin scheduling quantum in microseconds");

/*
 * Arrange to reschedule if necessary, taking the priorities and
 * schedulers into account.
 */
static void
maybe_resched(struct thread *td)
{

	mtx_assert(&sched_lock, MA_OWNED);
	if (td->td_priority < curthread->td_priority && curthread->td_kse)
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
schedcpu(void *arg)
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
			FOREACH_KSE_IN_GROUP(kg, ke) {
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
				    (TD_IS_RUNNING(ke->ke_thread))) {
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
				if (ke->ke_sched->ske_cpticks == 0)
					continue;
#if	(FSHIFT >= CCPU_SHIFT)
				ke->ke_pctcpu += (realstathz == 100)
				    ? ((fixpt_t) ke->ke_sched->ske_cpticks) <<
				    (FSHIFT - CCPU_SHIFT) :
				    100 * (((fixpt_t) ke->ke_sched->ske_cpticks)
				    << (FSHIFT - CCPU_SHIFT)) / realstathz;
#else
				ke->ke_pctcpu += ((FSCALE - ccpu) *
				    (ke->ke_sched->ske_cpticks *
				    FSCALE / realstathz)) >> FSHIFT;
#endif
				ke->ke_sched->ske_cpticks = 0;
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
	callout_reset(&schedcpu_callout, hz, schedcpu, NULL);
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
		    NICE_WEIGHT * (kg->kg_nice - PRIO_MIN);
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

	if (sched_quantum == 0)
		sched_quantum = SCHED_QUANTUM;
	hogticks = 2 * sched_quantum;

	callout_init(&schedcpu_callout, CALLOUT_MPSAFE);
	callout_init(&roundrobin_callout, 0);

	/* Kick off timeout driven events by calling first time. */
	roundrobin(NULL);
	schedcpu(NULL);
}

/* External interfaces start here */
int
sched_runnable(void)
{
        return runq_check(&runq);
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

	ke->ke_sched->ske_cpticks++;
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
sched_exit(struct proc *p, struct proc *p1)
{
	sched_exit_kse(FIRST_KSE_IN_PROC(p), FIRST_KSE_IN_PROC(p1));
	sched_exit_ksegrp(FIRST_KSEGRP_IN_PROC(p), FIRST_KSEGRP_IN_PROC(p1));
	sched_exit_thread(FIRST_THREAD_IN_PROC(p), FIRST_THREAD_IN_PROC(p1));
}

void
sched_exit_kse(struct kse *ke, struct kse *child)
{
}

void
sched_exit_ksegrp(struct ksegrp *kg, struct ksegrp *child)
{

	mtx_assert(&sched_lock, MA_OWNED);
	kg->kg_estcpu = ESTCPULIM(kg->kg_estcpu + child->kg_estcpu);
}

void
sched_exit_thread(struct thread *td, struct thread *child)
{
}

void
sched_fork(struct proc *p, struct proc *p1)
{
	sched_fork_kse(FIRST_KSE_IN_PROC(p), FIRST_KSE_IN_PROC(p1));
	sched_fork_ksegrp(FIRST_KSEGRP_IN_PROC(p), FIRST_KSEGRP_IN_PROC(p1));
	sched_fork_thread(FIRST_THREAD_IN_PROC(p), FIRST_THREAD_IN_PROC(p1));
}

void
sched_fork_kse(struct kse *ke, struct kse *child)
{
	child->ke_sched->ske_cpticks = 0;
}

void
sched_fork_ksegrp(struct ksegrp *kg, struct ksegrp *child)
{
	mtx_assert(&sched_lock, MA_OWNED);
	child->kg_estcpu = kg->kg_estcpu;
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
}

void
sched_nice(struct ksegrp *kg, int nice)
{

	PROC_LOCK_ASSERT(kg->kg_proc, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	kg->kg_nice = nice;
	resetpriority(kg);
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

	mtx_assert(&sched_lock, MA_OWNED);
	if (TD_ON_RUNQ(td)) {
		adjustrunqueue(td, prio);
	} else {
		td->td_priority = prio;
	}
}

void
sched_sleep(struct thread *td, u_char prio)
{

	mtx_assert(&sched_lock, MA_OWNED);
	td->td_ksegrp->kg_slptime = 0;
	td->td_priority = prio;
}

void
sched_switch(struct thread *td)
{
	struct thread *newtd;
	u_long sched_nest;
	struct kse *ke;
	struct proc *p;

	ke = td->td_kse;
	p = td->td_proc;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_state == KES_THREAD), ("mi_switch: kse state?"));

	td->td_lastcpu = td->td_oncpu;
	td->td_last_kse = ke;
	td->td_oncpu = NOCPU;
	td->td_flags &= ~TDF_NEEDRESCHED;
	/*
	 * At the last moment, if this thread is still marked RUNNING,
	 * then put it back on the run queue as it has not been suspended
	 * or stopped or any thing else similar.
	 */
	if (TD_IS_RUNNING(td)) {
		/* Put us back on the run queue (kse and all). */
		setrunqueue(td);
	} else if (p->p_flag & P_SA) {
		/*
		 * We will not be on the run queue. So we must be
		 * sleeping or similar. As it's available,
		 * someone else can use the KSE if they need it.
		 */
		kse_reassign(ke);
	}
	sched_nest = sched_lock.mtx_recurse;
	newtd = choosethread();
	if (td != newtd)
		cpu_switch(td, newtd);
	sched_lock.mtx_recurse = sched_nest;
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
	setrunqueue(td);
	maybe_resched(td);
}

void
sched_add(struct thread *td)
{
	struct kse *ke;

	ke = td->td_kse;
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_thread != NULL), ("runq_add: No thread on KSE"));
	KASSERT((ke->ke_thread->td_kse != NULL),
	    ("runq_add: No KSE on thread"));
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("runq_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("runq_add: process swapped out"));
	ke->ke_ksegrp->kg_runq_kses++;
	ke->ke_state = KES_ONRUNQ;

	runq_add(&runq, ke);
}

void
sched_rem(struct thread *td)
{
	struct kse *ke;

	ke = td->td_kse;
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("runq_remove: process swapped out"));
	KASSERT((ke->ke_state == KES_ONRUNQ), ("KSE not on run queue"));
	mtx_assert(&sched_lock, MA_OWNED);

	runq_remove(&runq, ke);
	ke->ke_state = KES_THREAD;
	ke->ke_ksegrp->kg_runq_kses--;
}

struct kse *
sched_choose(void)
{
	struct kse *ke;

	ke = runq_choose(&runq);

	if (ke != NULL) {
		runq_remove(&runq, ke);
		ke->ke_state = KES_THREAD;

		KASSERT((ke->ke_thread != NULL),
		    ("runq_choose: No thread on KSE"));
		KASSERT((ke->ke_thread->td_kse != NULL),
		    ("runq_choose: No KSE on thread"));
		KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
		    ("runq_choose: process swapped out"));
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

int
sched_sizeof_kse(void)
{
	return (sizeof(struct kse) + sizeof(struct ke_sched));
}
int
sched_sizeof_ksegrp(void)
{
	return (sizeof(struct ksegrp));
}
int
sched_sizeof_proc(void)
{
	return (sizeof(struct proc));
}
int
sched_sizeof_thread(void)
{
	return (sizeof(struct thread));
}

fixpt_t
sched_pctcpu(struct thread *td)
{
	return (td->td_kse->ke_pctcpu);
}
