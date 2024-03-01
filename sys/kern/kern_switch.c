/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/runq.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

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

/*
 * Support for scheduler stats exported via kern.sched.stats.  All stats may
 * be reset with kern.sched.stats.reset = 1.  Stats may be defined elsewhere
 * with SCHED_STAT_DEFINE().
 */
#ifdef SCHED_STATS
SYSCTL_NODE(_kern_sched, OID_AUTO, stats, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "switch stats");

/* Switch reasons from mi_switch(9). */
DPCPU_DEFINE(long, sched_switch_stats[SWT_COUNT]);
SCHED_STAT_DEFINE_VAR(owepreempt,
    &DPCPU_NAME(sched_switch_stats[SWT_OWEPREEMPT]), "");
SCHED_STAT_DEFINE_VAR(turnstile,
    &DPCPU_NAME(sched_switch_stats[SWT_TURNSTILE]), "");
SCHED_STAT_DEFINE_VAR(sleepq,
    &DPCPU_NAME(sched_switch_stats[SWT_SLEEPQ]), "");
SCHED_STAT_DEFINE_VAR(relinquish, 
    &DPCPU_NAME(sched_switch_stats[SWT_RELINQUISH]), "");
SCHED_STAT_DEFINE_VAR(needresched,
    &DPCPU_NAME(sched_switch_stats[SWT_NEEDRESCHED]), "");
SCHED_STAT_DEFINE_VAR(idle,
    &DPCPU_NAME(sched_switch_stats[SWT_IDLE]), "");
SCHED_STAT_DEFINE_VAR(iwait,
    &DPCPU_NAME(sched_switch_stats[SWT_IWAIT]), "");
SCHED_STAT_DEFINE_VAR(suspend,
    &DPCPU_NAME(sched_switch_stats[SWT_SUSPEND]), "");
SCHED_STAT_DEFINE_VAR(remotepreempt,
    &DPCPU_NAME(sched_switch_stats[SWT_REMOTEPREEMPT]), "");
SCHED_STAT_DEFINE_VAR(remotewakeidle,
    &DPCPU_NAME(sched_switch_stats[SWT_REMOTEWAKEIDLE]), "");
SCHED_STAT_DEFINE_VAR(bind,
    &DPCPU_NAME(sched_switch_stats[SWT_BIND]), "");

static int
sysctl_stats_reset(SYSCTL_HANDLER_ARGS)
{
	struct sysctl_oid *p;
	uintptr_t counter;
        int error;
	int val;
	int i;

        val = 0;
        error = sysctl_handle_int(oidp, &val, 0, req);
        if (error != 0 || req->newptr == NULL)
                return (error);
        if (val == 0)
                return (0);
	/*
	 * Traverse the list of children of _kern_sched_stats and reset each
	 * to 0.  Skip the reset entry.
	 */
	RB_FOREACH(p, sysctl_oid_list, oidp->oid_parent) {
		if (p == oidp || p->oid_arg1 == NULL)
			continue;
		counter = (uintptr_t)p->oid_arg1;
		CPU_FOREACH(i) {
			*(long *)(dpcpu_off[i] + counter) = 0;
		}
	}
	return (0);
}

SYSCTL_PROC(_kern_sched_stats, OID_AUTO, reset,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_stats_reset, "I",
    "Reset scheduler statistics");
#endif

/************************************************************************
 * Functions that manipulate runnability from a thread perspective.	*
 ************************************************************************/
/*
 * Select the thread that will be run next.
 */

static __noinline struct thread *
choosethread_panic(struct thread *td)
{

	/*
	 * If we are in panic, only allow system threads,
	 * plus the one we are running in, to be run.
	 */
retry:
	if (((td->td_proc->p_flag & P_SYSTEM) == 0 &&
	    (td->td_flags & TDF_INPANIC) == 0)) {
		/* note that it is no longer on the run queue */
		TD_SET_CAN_RUN(td);
		td = sched_choose();
		goto retry;
	}

	TD_SET_RUNNING(td);
	return (td);
}

struct thread *
choosethread(void)
{
	struct thread *td;

	td = sched_choose();

	if (KERNEL_PANICKED())
		return (choosethread_panic(td));

	TD_SET_RUNNING(td);
	return (td);
}

/*
 * Kernel thread preemption implementation.  Critical sections mark
 * regions of code in which preemptions are not allowed.
 *
 * It might seem a good idea to inline critical_enter() but, in order
 * to prevent instructions reordering by the compiler, a __compiler_membar()
 * would have to be used here (the same as sched_pin()).  The performance
 * penalty imposed by the membar could, then, produce slower code than
 * the function call itself, for most cases.
 */
void
critical_enter_KBI(void)
{
#ifdef KTR
	struct thread *td = curthread;
#endif
	critical_enter();
	CTR4(KTR_CRITICAL, "critical_enter by thread %p (%ld, %s) to %d", td,
	    (long)td->td_proc->p_pid, td->td_name, td->td_critnest);
}

void __noinline
critical_exit_preempt(void)
{
	struct thread *td;
	int flags;

	/*
	 * If td_critnest is 0, it is possible that we are going to get
	 * preempted again before reaching the code below. This happens
	 * rarely and is harmless. However, this means td_owepreempt may
	 * now be unset.
	 */
	td = curthread;
	if (td->td_critnest != 0)
		return;
	if (kdb_active)
		return;

	/*
	 * Microoptimization: we committed to switch,
	 * disable preemption in interrupt handlers
	 * while spinning for the thread lock.
	 */
	td->td_critnest = 1;
	thread_lock(td);
	td->td_critnest--;
	flags = SW_INVOL | SW_PREEMPT;
	if (TD_IS_IDLETHREAD(td))
		flags |= SWT_IDLE;
	else
		flags |= SWT_OWEPREEMPT;
	mi_switch(flags);
}

void
critical_exit_KBI(void)
{
#ifdef KTR
	struct thread *td = curthread;
#endif
	critical_exit();
	CTR4(KTR_CRITICAL, "critical_exit by thread %p (%ld, %s) to %d", td,
	    (long)td->td_proc->p_pid, td->td_name, td->td_critnest);
}

/************************************************************************
 * SYSTEM RUN QUEUE manipulations and tests				*
 ************************************************************************/
_Static_assert(RQ_NQS <= 256,
    "'td_rqindex' must be turned into a bigger unsigned type");
/* A macro instead of a function to get the proper calling function's name. */
#define CHECK_IDX(idx) ({						\
	__typeof(idx) _idx __unused = (idx);					\
	KASSERT(0 <= _idx && _idx < RQ_NQS,				\
	    ("%s: %s out of range: %d", __func__, __STRING(idx), _idx)); \
})

/*
 * Initialize a run structure.
 */
void
runq_init(struct runq *rq)
{
	int i;

	bzero(rq, sizeof(*rq));
	for (i = 0; i < RQ_NQS; i++)
		TAILQ_INIT(&rq->rq_queues[i]);
}

/*
 * Set the status bit of the queue at index 'idx', indicating that it is
 * non-empty.
 */
static __inline void
runq_setbit(struct runq *rq, int idx)
{
	struct rq_status *rqs;

	CHECK_IDX(idx);
	rqs = &rq->rq_status;
	CTR4(KTR_RUNQ, "runq_setbit: bits=%#x %#x bit=%#x word=%d",
	    rqs->rq_sw[RQSW_IDX(idx)],
	    rqs->rq_sw[RQSW_IDX(idx)] | RQSW_BIT(idx),
	    RQSW_BIT(idx), RQSW_IDX(idx));
	rqs->rq_sw[RQSW_IDX(idx)] |= RQSW_BIT(idx);
}

/*
 * Clear the status bit of the queue at index 'idx', indicating that it is
 * empty.
 */
static __inline void
runq_clrbit(struct runq *rq, int idx)
{
	struct rq_status *rqs;

	CHECK_IDX(idx);
	rqs = &rq->rq_status;
	CTR4(KTR_RUNQ, "runq_clrbit: bits=%#x %#x bit=%#x word=%d",
	    rqs->rq_sw[RQSW_IDX(idx)],
	    rqs->rq_sw[RQSW_IDX(idx)] & ~RQSW_BIT(idx),
	    RQSW_BIT(idx), RQSW_IDX(idx));
	rqs->rq_sw[RQSW_IDX(idx)] &= ~RQSW_BIT(idx);
}

/*
 * Add the thread to the queue specified by its priority, and set the
 * corresponding status bit.
 */
void
runq_add(struct runq *rq, struct thread *td, int flags)
{

	runq_add_idx(rq, td, RQ_PRI_TO_QUEUE_IDX(td->td_priority), flags);
}

void
runq_add_idx(struct runq *rq, struct thread *td, int idx, int flags)
{
	struct rq_queue *rqq;

	/*
	 * runq_setbit() asserts 'idx' is non-negative and below 'RQ_NQS', and
	 * a static assert earlier in this file ensures that 'RQ_NQS' is no more
	 * than 256.
	 */
	td->td_rqindex = idx;
	runq_setbit(rq, idx);
	rqq = &rq->rq_queues[idx];
	CTR4(KTR_RUNQ, "runq_add_idx: td=%p pri=%d idx=%d rqq=%p",
	    td, td->td_priority, idx, rqq);
	if (flags & SRQ_PREEMPTED)
		TAILQ_INSERT_HEAD(rqq, td, td_runq);
	else
		TAILQ_INSERT_TAIL(rqq, td, td_runq);
}

/*
 * Remove the thread from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 *
 * Returns whether the corresponding queue is empty after removal.
 */
bool
runq_remove(struct runq *rq, struct thread *td)
{
	struct rq_queue *rqq;
	int idx;

	KASSERT(td->td_flags & TDF_INMEM, ("runq_remove: Thread swapped out"));
	idx = td->td_rqindex;
	CHECK_IDX(idx);
	rqq = &rq->rq_queues[idx];
	CTR4(KTR_RUNQ, "runq_remove: td=%p pri=%d idx=%d rqq=%p",
	    td, td->td_priority, idx, rqq);
	TAILQ_REMOVE(rqq, td, td_runq);
	if (TAILQ_EMPTY(rqq)) {
		runq_clrbit(rq, idx);
		CTR1(KTR_RUNQ, "runq_remove: queue at idx=%d now empty", idx);
		return (true);
	}
	return (false);
}

/*
 * Find the index of the first non-empty run queue.  This is done by
 * scanning the status bits, a set bit indicating a non-empty queue.
 */
static __inline int
runq_findbit(struct runq *rq)
{
	struct rq_status *rqs;
	int idx;

	rqs = &rq->rq_status;
	for (int i = 0; i < RQSW_NB; i++)
		if (rqs->rq_sw[i] != 0) {
			idx = RQSW_FIRST_QUEUE_IDX(i, rqs->rq_sw[i]);
			CHECK_IDX(idx);
			CTR3(KTR_RUNQ, "runq_findbit: bits=%#x i=%d idx=%d",
			    rqs->rq_sw[i], i, idx);
			return (idx);
		}

	return (-1);
}

static __inline int
runq_findbit_from(struct runq *rq, int idx)
{
	struct rq_status *rqs;
	rqsw_t mask;
	int i;

	CHECK_IDX(idx);
	/* Set the mask for the first word so we ignore indices before 'idx'. */
	mask = (rqsw_t)-1 << RQSW_BIT_IDX(idx);
	rqs = &rq->rq_status;
again:
	for (i = RQSW_IDX(idx); i < RQSW_NB; mask = -1, i++) {
		mask = rqs->rq_sw[i] & mask;
		if (mask == 0)
			continue;
		idx = RQSW_FIRST_QUEUE_IDX(i, mask);
		CTR3(KTR_RUNQ, "runq_findbit_from: bits=%#x i=%d idx=%d",
		    mask, i, idx);
		return (idx);
	}
	if (idx == 0)
		return (-1);
	/*
	 * Wrap back around to the beginning of the list just once so we
	 * scan the whole thing.
	 */
	idx = 0;
	goto again;
}

/*
 * Return true if there are some processes of any priority on the run queue,
 * false otherwise.  Has no side effects.
 */
bool
runq_not_empty(struct runq *rq)
{
	struct rq_status *rqs;

	rqs = &rq->rq_status;
	for (int i = 0; i < RQSW_NB; i++)
		if (rqs->rq_sw[i] != 0) {
			CTR2(KTR_RUNQ, "runq_not_empty: bits=%#x i=%d",
			    rqs->rq_sw[i], i);
			return (true);
		}
	CTR0(KTR_RUNQ, "runq_not_empty: empty");

	return (false);
}

/*
 * Find the highest priority process on the run queue.
 */
struct thread *
runq_choose(struct runq *rq)
{
	struct rq_queue *rqq;
	struct thread *td;
	int idx;

	idx = runq_findbit(rq);
	if (idx != -1) {
		rqq = &rq->rq_queues[idx];
		td = TAILQ_FIRST(rqq);
		KASSERT(td != NULL, ("runq_choose: no thread on busy queue"));
		CTR3(KTR_RUNQ,
		    "runq_choose: idx=%d thread=%p rqq=%p", idx, td, rqq);
		return (td);
	}
	CTR1(KTR_RUNQ, "runq_choose: idlethread idx=%d", idx);

	return (NULL);
}

/*
 * Find the highest priority process on the run queue.
 */
struct thread *
runq_choose_fuzz(struct runq *rq, int fuzz)
{
	struct rq_queue *rqq;
	struct thread *td;
	int idx;

	idx = runq_findbit(rq);
	if (idx != -1) {
		rqq = &rq->rq_queues[idx];
		/* fuzz == 1 is normal.. 0 or less are ignored */
		if (fuzz > 1) {
			/*
			 * In the first couple of entries, check if
			 * there is one for our CPU as a preference.
			 */
			int count = fuzz;
			int cpu = PCPU_GET(cpuid);
			struct thread *td2;
			td2 = td = TAILQ_FIRST(rqq);

			while (count-- && td2) {
				if (td2->td_lastcpu == cpu) {
					td = td2;
					break;
				}
				td2 = TAILQ_NEXT(td2, td_runq);
			}
		} else
			td = TAILQ_FIRST(rqq);
		KASSERT(td != NULL, ("runq_choose_fuzz: no proc on busy queue"));
		CTR3(KTR_RUNQ,
		    "runq_choose_fuzz: idx=%d thread=%p rqq=%p", idx, td, rqq);
		return (td);
	}
	CTR1(KTR_RUNQ, "runq_choose_fuzz: idleproc idx=%d", idx);

	return (NULL);
}

struct thread *
runq_choose_from(struct runq *rq, int from_idx)
{
	struct rq_queue *rqq;
	struct thread *td;
	int idx;

	if ((idx = runq_findbit_from(rq, from_idx)) != -1) {
		rqq = &rq->rq_queues[idx];
		td = TAILQ_FIRST(rqq);
		KASSERT(td != NULL, ("runq_choose: no thread on busy queue"));
		CTR4(KTR_RUNQ,
		    "runq_choose_from: idx=%d thread=%p idx=%d rqq=%p",
		    idx, td, td->td_rqindex, rqq);
		return (td);
	}
	CTR1(KTR_RUNQ, "runq_choose_from: idlethread idx=%d", idx);

	return (NULL);
}
