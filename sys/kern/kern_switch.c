/*-
 * Copyright (c) 2001 Jake Burkholder <jake@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_sched.h"

#ifndef KERN_SWITCH_INCLUDE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sched.h>
#else  /* KERN_SWITCH_INCLUDE */
#if defined(SMP) && (defined(__i386__) || defined(__amd64__))
#include <sys/smp.h>
#endif
#if defined(SMP) && defined(SCHED_4BSD)
#include <sys/sysctl.h>
#endif

/* Uncomment this to enable logging of critical_enter/exit. */
#if 0
#define	KTR_CRITICAL	KTR_SCHED
#else
#define	KTR_CRITICAL	0
#endif

#ifdef FULL_PREEMPTION
#ifndef PREEMPTION
#error "The FULL_PREEMPTION option requires the PREEMPTION option"
#endif
#endif

CTASSERT((RQB_BPW * RQB_LEN) == RQ_NQS);

/*
 * kern.sched.preemption allows user space to determine if preemption support
 * is compiled in or not.  It is not currently a boot or runtime flag that
 * can be changed.
 */
#ifdef PREEMPTION
static int kern_sched_preemption = 1;
#else
static int kern_sched_preemption = 0;
#endif
SYSCTL_INT(_kern_sched, OID_AUTO, preemption, CTLFLAG_RD,
    &kern_sched_preemption, 0, "Kernel preemption enabled");

/************************************************************************
 * Functions that manipulate runnability from a thread perspective.	*
 ************************************************************************/
/*
 * Select the thread that will be run next.
 */
struct thread *
choosethread(void)
{
	struct td_sched *ts;
	struct thread *td;

#if defined(SMP) && (defined(__i386__) || defined(__amd64__))
	if (smp_active == 0 && PCPU_GET(cpuid) != 0) {
		/* Shutting down, run idlethread on AP's */
		td = PCPU_GET(idlethread);
		ts = td->td_sched;
		CTR1(KTR_RUNQ, "choosethread: td=%p (idle)", td);
		ts->ts_flags |= TSF_DIDRUN;
		TD_SET_RUNNING(td);
		return (td);
	}
#endif

retry:
	ts = sched_choose();
	if (ts) {
		td = ts->ts_thread;
		CTR2(KTR_RUNQ, "choosethread: td=%p pri=%d",
		    td, td->td_priority);
	} else {
		/* Simulate runq_choose() having returned the idle thread */
		td = PCPU_GET(idlethread);
		ts = td->td_sched;
		CTR1(KTR_RUNQ, "choosethread: td=%p (idle)", td);
	}
	ts->ts_flags |= TSF_DIDRUN;

	/*
	 * If we are in panic, only allow system threads,
	 * plus the one we are running in, to be run.
	 */
	if (panicstr && ((td->td_proc->p_flag & P_SYSTEM) == 0 &&
	    (td->td_flags & TDF_INPANIC) == 0)) {
		/* note that it is no longer on the run queue */
		TD_SET_CAN_RUN(td);
		goto retry;
	}

	TD_SET_RUNNING(td);
	return (td);
}


#if 0
/*
 * currently not used.. threads remove themselves from the
 * run queue by running.
 */
static void
remrunqueue(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((TD_ON_RUNQ(td)), ("remrunqueue: Bad state on run queue"));
	CTR1(KTR_RUNQ, "remrunqueue: td%p", td);
	TD_SET_CAN_RUN(td);
	/* remove from sys run queue */
	sched_rem(td);
	return;
}
#endif

/*
 * Change the priority of a thread that is on the run queue.
 */
void
adjustrunqueue( struct thread *td, int newpri)
{
	struct td_sched *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((TD_ON_RUNQ(td)), ("adjustrunqueue: Bad state on run queue"));

	ts = td->td_sched;
	CTR1(KTR_RUNQ, "adjustrunqueue: td%p", td);
		/* We only care about the td_sched in the run queue. */
	td->td_priority = newpri;
#ifndef SCHED_CORE
	if (ts->ts_rqindex != (newpri / RQ_PPQ))
#else
	if (ts->ts_rqindex != newpri)
#endif
	{
		sched_rem(td);
		sched_add(td, SRQ_BORING);
	}
}

void
setrunqueue(struct thread *td, int flags)
{

	CTR2(KTR_RUNQ, "setrunqueue: td:%p pid:%d",
	    td, td->td_proc->p_pid);
	CTR5(KTR_SCHED, "setrunqueue: %p(%s) prio %d by %p(%s)",
            td, td->td_proc->p_comm, td->td_priority, curthread,
            curthread->td_proc->p_comm);
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((td->td_inhibitors == 0),
			("setrunqueue: trying to run inhibited thread"));
	KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	    ("setrunqueue: bad thread state"));
	TD_SET_RUNQ(td);
	sched_add(td, flags);
}

/*
 * Kernel thread preemption implementation.  Critical sections mark
 * regions of code in which preemptions are not allowed.
 */
void
critical_enter(void)
{
	struct thread *td;

	td = curthread;
	td->td_critnest++;
	CTR4(KTR_CRITICAL, "critical_enter by thread %p (%ld, %s) to %d", td,
	    (long)td->td_proc->p_pid, td->td_proc->p_comm, td->td_critnest);
}

void
critical_exit(void)
{
	struct thread *td;

	td = curthread;
	KASSERT(td->td_critnest != 0,
	    ("critical_exit: td_critnest == 0"));
#ifdef PREEMPTION
	if (td->td_critnest == 1) {
		td->td_critnest = 0;
		mtx_assert(&sched_lock, MA_NOTOWNED);
		if (td->td_owepreempt) {
			td->td_critnest = 1;
			mtx_lock_spin(&sched_lock);
			td->td_critnest--;
			mi_switch(SW_INVOL, NULL);
			mtx_unlock_spin(&sched_lock);
		}
	} else
#endif
		td->td_critnest--;

	CTR4(KTR_CRITICAL, "critical_exit by thread %p (%ld, %s) to %d", td,
	    (long)td->td_proc->p_pid, td->td_proc->p_comm, td->td_critnest);
}

/*
 * This function is called when a thread is about to be put on run queue
 * because it has been made runnable or its priority has been adjusted.  It
 * determines if the new thread should be immediately preempted to.  If so,
 * it switches to it and eventually returns true.  If not, it returns false
 * so that the caller may place the thread on an appropriate run queue.
 */
int
maybe_preempt(struct thread *td)
{
#ifdef PREEMPTION
	struct thread *ctd;
	int cpri, pri;
#endif

	mtx_assert(&sched_lock, MA_OWNED);
#ifdef PREEMPTION
	/*
	 * The new thread should not preempt the current thread if any of the
	 * following conditions are true:
	 *
	 *  - The kernel is in the throes of crashing (panicstr).
	 *  - The current thread has a higher (numerically lower) or
	 *    equivalent priority.  Note that this prevents curthread from
	 *    trying to preempt to itself.
	 *  - It is too early in the boot for context switches (cold is set).
	 *  - The current thread has an inhibitor set or is in the process of
	 *    exiting.  In this case, the current thread is about to switch
	 *    out anyways, so there's no point in preempting.  If we did,
	 *    the current thread would not be properly resumed as well, so
	 *    just avoid that whole landmine.
	 *  - If the new thread's priority is not a realtime priority and
	 *    the current thread's priority is not an idle priority and
	 *    FULL_PREEMPTION is disabled.
	 *
	 * If all of these conditions are false, but the current thread is in
	 * a nested critical section, then we have to defer the preemption
	 * until we exit the critical section.  Otherwise, switch immediately
	 * to the new thread.
	 */
	ctd = curthread;
	KASSERT ((ctd->td_sched != NULL && ctd->td_sched->ts_thread == ctd),
	  ("thread has no (or wrong) sched-private part."));
	KASSERT((td->td_inhibitors == 0),
			("maybe_preempt: trying to run inhibited thread"));
	pri = td->td_priority;
	cpri = ctd->td_priority;
	if (panicstr != NULL || pri >= cpri || cold /* || dumping */ ||
	    TD_IS_INHIBITED(ctd) || td->td_sched->ts_state != TSS_THREAD)
		return (0);
#ifndef FULL_PREEMPTION
	if (pri > PRI_MAX_ITHD && cpri < PRI_MIN_IDLE)
		return (0);
#endif

	if (ctd->td_critnest > 1) {
		CTR1(KTR_PROC, "maybe_preempt: in critical section %d",
		    ctd->td_critnest);
		ctd->td_owepreempt = 1;
		return (0);
	}

	/*
	 * Thread is runnable but not yet put on system run queue.
	 */
	MPASS(TD_ON_RUNQ(td));
	MPASS(td->td_sched->ts_state != TSS_ONRUNQ);
	TD_SET_RUNNING(td);
	CTR3(KTR_PROC, "preempting to thread %p (pid %d, %s)\n", td,
	    td->td_proc->p_pid, td->td_proc->p_comm);
	mi_switch(SW_INVOL|SW_PREEMPT, td);
	return (1);
#else
	return (0);
#endif
}

#if 0
#ifndef PREEMPTION
/* XXX: There should be a non-static version of this. */
static void
printf_caddr_t(void *data)
{
	printf("%s", (char *)data);
}
static char preempt_warning[] =
    "WARNING: Kernel preemption is disabled, expect reduced performance.\n";
SYSINIT(preempt_warning, SI_SUB_COPYRIGHT, SI_ORDER_ANY, printf_caddr_t,
    preempt_warning)
#endif
#endif

/************************************************************************
 * SYSTEM RUN QUEUE manipulations and tests				*
 ************************************************************************/
/*
 * Initialize a run structure.
 */
void
runq_init(struct runq *rq)
{
	int i;

	bzero(rq, sizeof *rq);
	for (i = 0; i < RQ_NQS; i++)
		TAILQ_INIT(&rq->rq_queues[i]);
}

/*
 * Clear the status bit of the queue corresponding to priority level pri,
 * indicating that it is empty.
 */
static __inline void
runq_clrbit(struct runq *rq, int pri)
{
	struct rqbits *rqb;

	rqb = &rq->rq_status;
	CTR4(KTR_RUNQ, "runq_clrbit: bits=%#x %#x bit=%#x word=%d",
	    rqb->rqb_bits[RQB_WORD(pri)],
	    rqb->rqb_bits[RQB_WORD(pri)] & ~RQB_BIT(pri),
	    RQB_BIT(pri), RQB_WORD(pri));
	rqb->rqb_bits[RQB_WORD(pri)] &= ~RQB_BIT(pri);
}

/*
 * Find the index of the first non-empty run queue.  This is done by
 * scanning the status bits, a set bit indicates a non-empty queue.
 */
static __inline int
runq_findbit(struct runq *rq)
{
	struct rqbits *rqb;
	int pri;
	int i;

	rqb = &rq->rq_status;
	for (i = 0; i < RQB_LEN; i++)
		if (rqb->rqb_bits[i]) {
			pri = RQB_FFS(rqb->rqb_bits[i]) + (i << RQB_L2BPW);
			CTR3(KTR_RUNQ, "runq_findbit: bits=%#x i=%d pri=%d",
			    rqb->rqb_bits[i], i, pri);
			return (pri);
		}

	return (-1);
}

static __inline int
runq_findbit_from(struct runq *rq, int start)
{
	struct rqbits *rqb;
	int bit;
	int pri;
	int i;

	rqb = &rq->rq_status;
	bit = start & (RQB_BPW -1);
	pri = 0;
	CTR1(KTR_RUNQ, "runq_findbit_from: start %d", start);
again:
	for (i = RQB_WORD(start); i < RQB_LEN; i++) {
		CTR3(KTR_RUNQ, "runq_findbit_from: bits %d = %#x bit = %d",
		    i, rqb->rqb_bits[i], bit);
		if (rqb->rqb_bits[i]) {
			if (bit != 0) {
				for (pri = bit; pri < RQB_BPW; pri++)
					if (rqb->rqb_bits[i] & (1ul << pri))
						break;
				bit = 0;
				if (pri >= RQB_BPW)
					continue;
			} else
				pri = RQB_FFS(rqb->rqb_bits[i]);
			pri += (i << RQB_L2BPW);
			CTR3(KTR_RUNQ, "runq_findbit_from: bits=%#x i=%d pri=%d",
			    rqb->rqb_bits[i], i, pri);
			return (pri);
		}
		bit = 0;
	}
	if (start != 0) {
		CTR0(KTR_RUNQ, "runq_findbit_from: restarting");
		start = 0;
		goto again;
	}

	return (-1);
}

/*
 * Set the status bit of the queue corresponding to priority level pri,
 * indicating that it is non-empty.
 */
static __inline void
runq_setbit(struct runq *rq, int pri)
{
	struct rqbits *rqb;

	rqb = &rq->rq_status;
	CTR4(KTR_RUNQ, "runq_setbit: bits=%#x %#x bit=%#x word=%d",
	    rqb->rqb_bits[RQB_WORD(pri)],
	    rqb->rqb_bits[RQB_WORD(pri)] | RQB_BIT(pri),
	    RQB_BIT(pri), RQB_WORD(pri));
	rqb->rqb_bits[RQB_WORD(pri)] |= RQB_BIT(pri);
}

/*
 * Add the thread to the queue specified by its priority, and set the
 * corresponding status bit.
 */
void
runq_add(struct runq *rq, struct td_sched *ts, int flags)
{
	struct rqhead *rqh;
	int pri;

	pri = ts->ts_thread->td_priority / RQ_PPQ;
	ts->ts_rqindex = pri;
	runq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	CTR5(KTR_RUNQ, "runq_add: td=%p ts=%p pri=%d %d rqh=%p",
	    ts->ts_thread, ts, ts->ts_thread->td_priority, pri, rqh);
	if (flags & SRQ_PREEMPTED) {
		TAILQ_INSERT_HEAD(rqh, ts, ts_procq);
	} else {
		TAILQ_INSERT_TAIL(rqh, ts, ts_procq);
	}
}

void
runq_add_pri(struct runq *rq, struct td_sched *ts, int pri, int flags)
{
	struct rqhead *rqh;

	KASSERT(pri < RQ_NQS, ("runq_add_pri: %d out of range", pri));
	ts->ts_rqindex = pri;
	runq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	CTR5(KTR_RUNQ, "runq_add_pri: td=%p ke=%p pri=%d idx=%d rqh=%p",
	    ts->ts_thread, ts, ts->ts_thread->td_priority, pri, rqh);
	if (flags & SRQ_PREEMPTED) {
		TAILQ_INSERT_HEAD(rqh, ts, ts_procq);
	} else {
		TAILQ_INSERT_TAIL(rqh, ts, ts_procq);
	}
}
/*
 * Return true if there are runnable processes of any priority on the run
 * queue, false otherwise.  Has no side effects, does not modify the run
 * queue structure.
 */
int
runq_check(struct runq *rq)
{
	struct rqbits *rqb;
	int i;

	rqb = &rq->rq_status;
	for (i = 0; i < RQB_LEN; i++)
		if (rqb->rqb_bits[i]) {
			CTR2(KTR_RUNQ, "runq_check: bits=%#x i=%d",
			    rqb->rqb_bits[i], i);
			return (1);
		}
	CTR0(KTR_RUNQ, "runq_check: empty");

	return (0);
}

#if defined(SMP) && defined(SCHED_4BSD)
int runq_fuzz = 1;
SYSCTL_INT(_kern_sched, OID_AUTO, runq_fuzz, CTLFLAG_RW, &runq_fuzz, 0, "");
#endif

/*
 * Find the highest priority process on the run queue.
 */
struct td_sched *
runq_choose(struct runq *rq)
{
	struct rqhead *rqh;
	struct td_sched *ts;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	while ((pri = runq_findbit(rq)) != -1) {
		rqh = &rq->rq_queues[pri];
#if defined(SMP) && defined(SCHED_4BSD)
		/* fuzz == 1 is normal.. 0 or less are ignored */
		if (runq_fuzz > 1) {
			/*
			 * In the first couple of entries, check if
			 * there is one for our CPU as a preference.
			 */
			int count = runq_fuzz;
			int cpu = PCPU_GET(cpuid);
			struct td_sched *ts2;
			ts2 = ts = TAILQ_FIRST(rqh);

			while (count-- && ts2) {
				if (ts->ts_thread->td_lastcpu == cpu) {
					ts = ts2;
					break;
				}
				ts2 = TAILQ_NEXT(ts2, ts_procq);
			}
		} else
#endif
			ts = TAILQ_FIRST(rqh);
		KASSERT(ts != NULL, ("runq_choose: no proc on busy queue"));
		CTR3(KTR_RUNQ,
		    "runq_choose: pri=%d td_sched=%p rqh=%p", pri, ts, rqh);
		return (ts);
	}
	CTR1(KTR_RUNQ, "runq_choose: idleproc pri=%d", pri);

	return (NULL);
}

struct td_sched *
runq_choose_from(struct runq *rq, int idx)
{
	struct rqhead *rqh;
	struct td_sched *ts;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	if ((pri = runq_findbit_from(rq, idx)) != -1) {
		rqh = &rq->rq_queues[pri];
		ts = TAILQ_FIRST(rqh);
		KASSERT(ts != NULL, ("runq_choose: no proc on busy queue"));
		CTR4(KTR_RUNQ,
		    "runq_choose_from: pri=%d kse=%p idx=%d rqh=%p",
		    pri, ts, ts->ts_rqindex, rqh);
		return (ts);
	}
	CTR1(KTR_RUNQ, "runq_choose_from: idleproc pri=%d", pri);

	return (NULL);
}
/*
 * Remove the thread from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 * Caller must set ts->ts_state afterwards.
 */
void
runq_remove(struct runq *rq, struct td_sched *ts)
{

	runq_remove_idx(rq, ts, NULL);
}

void
runq_remove_idx(struct runq *rq, struct td_sched *ts, int *idx)
{
	struct rqhead *rqh;
	int pri;

	KASSERT(ts->ts_thread->td_proc->p_sflag & PS_INMEM,
		("runq_remove_idx: process swapped out"));
	pri = ts->ts_rqindex;
	rqh = &rq->rq_queues[pri];
	CTR5(KTR_RUNQ, "runq_remove_idx: td=%p, ts=%p pri=%d %d rqh=%p",
	    ts->ts_thread, ts, ts->ts_thread->td_priority, pri, rqh);
	TAILQ_REMOVE(rqh, ts, ts_procq);
	if (TAILQ_EMPTY(rqh)) {
		CTR0(KTR_RUNQ, "runq_remove_idx: empty");
		runq_clrbit(rq, pri);
		if (idx != NULL && *idx == pri)
			*idx = (pri + 1) % RQ_NQS;
	}
}

/****** functions that are temporarily here ***********/
#include <vm/uma.h>
extern struct mtx kse_zombie_lock;

/*
 *  Allocate scheduler specific per-process resources.
 * The thread and proc have already been linked in.
 *
 * Called from:
 *  proc_init() (UMA init method)
 */
void
sched_newproc(struct proc *p, struct thread *td)
{
}

/*
 * thread is being either created or recycled.
 * Fix up the per-scheduler resources associated with it.
 * Called from:
 *  sched_fork_thread()
 *  thread_dtor()  (*may go away)
 *  thread_init()  (*may go away)
 */
void
sched_newthread(struct thread *td)
{
	struct td_sched *ts;

	ts = (struct td_sched *) (td + 1);
	bzero(ts, sizeof(*ts));
	td->td_sched     = ts;
	ts->ts_thread	= td;
	ts->ts_state	= TSS_THREAD;
}

/*
 * Called from:
 *  thr_create()
 *  proc_init() (UMA) via sched_newproc()
 */
void
sched_init_concurrency(struct proc *p)
{
}

/*
 * Change the concurrency of an existing proc to N
 * Called from:
 *  kse_create()
 *  kse_exit()
 *  thread_exit()
 *  thread_single()
 */
void
sched_set_concurrency(struct proc *p, int concurrency)
{
}

/*
 * Called from thread_exit() for all exiting thread
 *
 * Not to be confused with sched_exit_thread()
 * that is only called from thread_exit() for threads exiting
 * without the rest of the process exiting because it is also called from
 * sched_exit() and we wouldn't want to call it twice.
 * XXX This can probably be fixed.
 */
void
sched_thread_exit(struct thread *td)
{
}

#endif /* KERN_SWITCH_INCLUDE */
