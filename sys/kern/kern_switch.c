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

#include <machine/cpu.h>

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

#ifdef SCHED_STATS
long switch_preempt;
long switch_owepreempt;
long switch_turnstile;
long switch_sleepq;
long switch_sleepqtimo;
long switch_relinquish;
long switch_needresched;
static SYSCTL_NODE(_kern_sched, OID_AUTO, stats, CTLFLAG_RW, 0, "switch stats");
SYSCTL_INT(_kern_sched_stats, OID_AUTO, preempt, CTLFLAG_RD, &switch_preempt, 0, "");
SYSCTL_INT(_kern_sched_stats, OID_AUTO, owepreempt, CTLFLAG_RD, &switch_owepreempt, 0, "");
SYSCTL_INT(_kern_sched_stats, OID_AUTO, turnstile, CTLFLAG_RD, &switch_turnstile, 0, "");
SYSCTL_INT(_kern_sched_stats, OID_AUTO, sleepq, CTLFLAG_RD, &switch_sleepq, 0, "");
SYSCTL_INT(_kern_sched_stats, OID_AUTO, sleepqtimo, CTLFLAG_RD, &switch_sleepqtimo, 0, "");
SYSCTL_INT(_kern_sched_stats, OID_AUTO, relinquish, CTLFLAG_RD, &switch_relinquish, 0, "");
SYSCTL_INT(_kern_sched_stats, OID_AUTO, needresched, CTLFLAG_RD, &switch_needresched, 0, "");
static int
sysctl_stats_reset(SYSCTL_HANDLER_ARGS)
{
        int error;
	int val;

        val = 0;
        error = sysctl_handle_int(oidp, &val, 0, req);
        if (error != 0 || req->newptr == NULL)
                return (error);
        if (val == 0)
                return (0);
	switch_preempt = 0;
	switch_owepreempt = 0;
	switch_turnstile = 0;
	switch_sleepq = 0;
	switch_sleepqtimo = 0;
	switch_relinquish = 0;
	switch_needresched = 0;

	return (0);
}

SYSCTL_PROC(_kern_sched_stats, OID_AUTO, reset, CTLTYPE_INT | CTLFLAG_WR, NULL,
    0, sysctl_stats_reset, "I", "Reset scheduler statistics");
#endif

/************************************************************************
 * Functions that manipulate runnability from a thread perspective.	*
 ************************************************************************/
/*
 * Select the thread that will be run next.
 */
struct thread *
choosethread(void)
{
	struct thread *td;

retry:
	td = sched_choose();

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
	    (long)td->td_proc->p_pid, td->td_name, td->td_critnest);
}

void
critical_exit(void)
{
	struct thread *td;

	td = curthread;
	KASSERT(td->td_critnest != 0,
	    ("critical_exit: td_critnest == 0"));

	if (td->td_critnest == 1) {
		td->td_critnest = 0;
		if (td->td_owepreempt) {
			td->td_critnest = 1;
			thread_lock(td);
			td->td_critnest--;
			SCHED_STAT_INC(switch_owepreempt);
			mi_switch(SW_INVOL|SW_PREEMPT, NULL);
			thread_unlock(td);
		}
	} else
		td->td_critnest--;

	CTR4(KTR_CRITICAL, "critical_exit by thread %p (%ld, %s) to %d", td,
	    (long)td->td_proc->p_pid, td->td_name, td->td_critnest);
}

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
runq_findbit_from(struct runq *rq, u_char pri)
{
	struct rqbits *rqb;
	rqb_word_t mask;
	int i;

	/*
	 * Set the mask for the first word so we ignore priorities before 'pri'.
	 */
	mask = (rqb_word_t)-1 << (pri & (RQB_BPW - 1));
	rqb = &rq->rq_status;
again:
	for (i = RQB_WORD(pri); i < RQB_LEN; mask = -1, i++) {
		mask = rqb->rqb_bits[i] & mask;
		if (mask == 0)
			continue;
		pri = RQB_FFS(mask) + (i << RQB_L2BPW);
		CTR3(KTR_RUNQ, "runq_findbit_from: bits=%#x i=%d pri=%d",
		    mask, i, pri);
		return (pri);
	}
	if (pri == 0)
		return (-1);
	/*
	 * Wrap back around to the beginning of the list just once so we
	 * scan the whole thing.
	 */
	pri = 0;
	goto again;
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
runq_add_pri(struct runq *rq, struct td_sched *ts, u_char pri, int flags)
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

/*
 * Find the highest priority process on the run queue.
 */
struct td_sched *
runq_choose_fuzz(struct runq *rq, int fuzz)
{
	struct rqhead *rqh;
	struct td_sched *ts;
	int pri;

	while ((pri = runq_findbit(rq)) != -1) {
		rqh = &rq->rq_queues[pri];
		/* fuzz == 1 is normal.. 0 or less are ignored */
		if (fuzz > 1) {
			/*
			 * In the first couple of entries, check if
			 * there is one for our CPU as a preference.
			 */
			int count = fuzz;
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
			ts = TAILQ_FIRST(rqh);
		KASSERT(ts != NULL, ("runq_choose_fuzz: no proc on busy queue"));
		CTR3(KTR_RUNQ,
		    "runq_choose_fuzz: pri=%d td_sched=%p rqh=%p", pri, ts, rqh);
		return (ts);
	}
	CTR1(KTR_RUNQ, "runq_choose_fuzz: idleproc pri=%d", pri);

	return (NULL);
}

/*
 * Find the highest priority process on the run queue.
 */
struct td_sched *
runq_choose(struct runq *rq)
{
	struct rqhead *rqh;
	struct td_sched *ts;
	int pri;

	while ((pri = runq_findbit(rq)) != -1) {
		rqh = &rq->rq_queues[pri];
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
runq_choose_from(struct runq *rq, u_char idx)
{
	struct rqhead *rqh;
	struct td_sched *ts;
	int pri;

	if ((pri = runq_findbit_from(rq, idx)) != -1) {
		rqh = &rq->rq_queues[pri];
		ts = TAILQ_FIRST(rqh);
		KASSERT(ts != NULL, ("runq_choose: no proc on busy queue"));
		CTR4(KTR_RUNQ,
		    "runq_choose_from: pri=%d td_sched=%p idx=%d rqh=%p",
		    pri, ts, ts->ts_rqindex, rqh);
		return (ts);
	}
	CTR1(KTR_RUNQ, "runq_choose_from: idleproc pri=%d", pri);

	return (NULL);
}
/*
 * Remove the thread from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 * Caller must set state afterwards.
 */
void
runq_remove(struct runq *rq, struct td_sched *ts)
{

	runq_remove_idx(rq, ts, NULL);
}

void
runq_remove_idx(struct runq *rq, struct td_sched *ts, u_char *idx)
{
	struct rqhead *rqh;
	u_char pri;

	KASSERT(ts->ts_thread->td_flags & TDF_INMEM,
		("runq_remove_idx: thread swapped out"));
	pri = ts->ts_rqindex;
	KASSERT(pri < RQ_NQS, ("runq_remove_idx: Invalid index %d\n", pri));
	rqh = &rq->rq_queues[pri];
	CTR5(KTR_RUNQ, "runq_remove_idx: td=%p, ts=%p pri=%d %d rqh=%p",
	    ts->ts_thread, ts, ts->ts_thread->td_priority, pri, rqh);
	{
		struct td_sched *nts;

		TAILQ_FOREACH(nts, rqh, ts_procq)
			if (nts == ts)
				break;
		if (ts != nts)
			panic("runq_remove_idx: ts %p not on rqindex %d",
			    ts, pri);
	}
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
}

#endif /* KERN_SWITCH_INCLUDE */
