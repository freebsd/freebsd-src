/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Jake Burkholder <jake@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * Portions of this software were developed by Olivier Certner
 * <olce.freebsd@certner.fr> at Kumacom SARL under sponsorship from the FreeBSD
 * Foundation.
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

/* Status words' individual bit manipulators' internals. */
typedef uintptr_t	runq_sw_op(int idx, int sw_idx, rqsw_t sw_bit,
			    rqsw_t *swp);
static inline uintptr_t	runq_sw_apply(struct runq *rq, int idx,
			    runq_sw_op *op);

static inline uintptr_t	runq_sw_set_not_empty_op(int idx, int sw_idx,
			    rqsw_t sw_bit, rqsw_t *swp);
static inline uintptr_t	runq_sw_set_empty_op(int idx, int sw_idx,
			    rqsw_t sw_bit, rqsw_t *swp);
static inline uintptr_t	runq_sw_is_empty_op(int idx, int sw_idx,
			    rqsw_t sw_bit, rqsw_t *swp);

/* Status words' individual bit manipulators. */
static inline void	runq_sw_set_not_empty(struct runq *rq, int idx);
static inline void	runq_sw_set_empty(struct runq *rq, int idx);
static inline bool	runq_sw_is_empty(struct runq *rq, int idx);

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
 * Helper to implement functions operating on a particular status word bit.
 *
 * The operator is passed the initial 'idx', the corresponding status word index
 * in 'rq_status' in 'sw_idx', a status word with only that bit set in 'sw_bit'
 * and a pointer to the corresponding status word in 'swp'.
 */
static inline uintptr_t
runq_sw_apply(struct runq *rq, int idx, runq_sw_op *op)
{
	rqsw_t *swp;
	rqsw_t sw_bit;
	int sw_idx;

	CHECK_IDX(idx);

	sw_idx = RQSW_IDX(idx);
	sw_bit = RQSW_BIT(idx);
	swp = &rq->rq_status.rq_sw[sw_idx];

	return (op(idx, sw_idx, sw_bit, swp));
}

static inline uintptr_t
runq_sw_set_not_empty_op(int idx, int sw_idx, rqsw_t sw_bit, rqsw_t *swp)
{
	rqsw_t old_sw __unused = *swp;

	*swp |= sw_bit;
	CTR4(KTR_RUNQ,
	    "runq_sw_set_not_empty: idx=%d sw_idx=%d "
	    "bits=" RQSW_PRI "->" RQSW_PRI,
	    idx, sw_idx, old_sw, *swp);
	return (0);
}

/*
 * Modify the status words to indicate that some queue is not empty.
 *
 * Sets the status bit corresponding to the queue at index 'idx'.
 */
static inline void
runq_sw_set_not_empty(struct runq *rq, int idx)
{

	(void)runq_sw_apply(rq, idx, &runq_sw_set_not_empty_op);
}

static inline uintptr_t
runq_sw_set_empty_op(int idx, int sw_idx, rqsw_t sw_bit, rqsw_t *swp)
{
	rqsw_t old_sw __unused = *swp;

	*swp &= ~sw_bit;
	CTR4(KTR_RUNQ,
	    "runq_sw_set_empty: idx=%d sw_idx=%d "
	    "bits=" RQSW_PRI "->" RQSW_PRI,
	    idx, sw_idx, old_sw, *swp);
	return (0);
}

/*
 * Modify the status words to indicate that some queue is empty.
 *
 * Clears the status bit corresponding to the queue at index 'idx'.
 */
static inline void
runq_sw_set_empty(struct runq *rq, int idx)
{

	(void)runq_sw_apply(rq, idx, &runq_sw_set_empty_op);
}

static inline uintptr_t
runq_sw_is_empty_op(int idx, int sw_idx, rqsw_t sw_bit, rqsw_t *swp)
{
	return ((*swp & sw_bit) == 0);
}

/*
 * Returns whether the status words indicate that some queue is empty.
 */
static inline bool
runq_sw_is_empty(struct runq *rq, int idx)
{
	return (runq_sw_apply(rq, idx, &runq_sw_is_empty_op));
}

/*
 * Returns whether a particular queue is empty.
 */
bool
runq_is_queue_empty(struct runq *rq, int idx)
{

	return (runq_sw_is_empty(rq, idx));
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
	 * runq_sw_*() functions assert that 'idx' is non-negative and below
	 * 'RQ_NQS', and a static assert earlier in this file ensures that
	 * 'RQ_NQS' is no more than 256.
	 */
	td->td_rqindex = idx;
	runq_sw_set_not_empty(rq, idx);
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
		runq_sw_set_empty(rq, idx);
		CTR1(KTR_RUNQ, "runq_remove: queue at idx=%d now empty", idx);
		return (true);
	}
	return (false);
}

static inline int
runq_findq_status_word(struct runq *const rq, const int w_idx,
    const rqsw_t w, runq_pred_t *const pred, void *const pred_data)
{
	struct rq_queue *q;
	rqsw_t tw = w;
	int idx, b_idx;

	while (tw != 0) {
		b_idx = RQSW_BSF(tw);
		idx = RQSW_TO_QUEUE_IDX(w_idx, b_idx);
		q = &rq->rq_queues[idx];
		KASSERT(!TAILQ_EMPTY(q),
		    ("runq_findq(): No thread on non-empty queue with idx=%d",
		    idx));
		if (pred(idx, q, pred_data))
			return (idx);
		tw &= ~RQSW_BIT(idx);
	}

	return (-1);
}

/*
 * Find in the passed range (bounds included) the index of the first (i.e.,
 * having lower index) non-empty queue that passes pred().
 *
 * Considered queues are those with index 'lvl_min' up to 'lvl_max' (bounds
 * included).  If no queue matches, returns -1.
 *
 * This is done by scanning the status words (a set bit indicates a non-empty
 * queue) and calling pred() with corresponding queue indices.  pred() must
 * return whether the corresponding queue is accepted.  It is passed private
 * data through 'pred_data', which can be used both for extra input and output.
 */
int
runq_findq(struct runq *const rq, const int lvl_min, const int lvl_max,
    runq_pred_t *const pred, void *const pred_data)
{
	const rqsw_t (*const rqsw)[RQSW_NB] = &rq->rq_status.rq_sw;
	rqsw_t w;
	int i, last, idx;

	CHECK_IDX(lvl_min);
	CHECK_IDX(lvl_max);
	KASSERT(lvl_min <= lvl_max,
	    ("lvl_min: %d > lvl_max: %d!", lvl_min, lvl_max));

	i = RQSW_IDX(lvl_min);
	last = RQSW_IDX(lvl_max);
	/* Clear bits for runqueues below 'lvl_min'. */
	w = (*rqsw)[i] & ~(RQSW_BIT(lvl_min) - 1);
	if (i == last)
		goto last_mask;
	idx = runq_findq_status_word(rq, i, w, pred, pred_data);
	if (idx != -1)
		goto return_idx;

	for (++i; i < last; ++i) {
		w = (*rqsw)[i];
		idx = runq_findq_status_word(rq, i, w, pred, pred_data);
		if (idx != -1)
			goto return_idx;
	}

	MPASS(i == last);
	w = (*rqsw)[i];
last_mask:
	/* Clear bits for runqueues above 'lvl_max'. */
	w &= (RQSW_BIT(lvl_max) - 1) | RQSW_BIT(lvl_max);
	idx = runq_findq_status_word(rq, i, w, pred, pred_data);
	if (idx != -1)
		goto return_idx;
	return (-1);
return_idx:
	CTR4(KTR_RUNQ,
	    "runq_findq: bits=" RQSW_PRI "->" RQSW_PRI " i=%d idx=%d",
	    (*rqsw)[i], w, i, idx);
	return (idx);
}

static bool
runq_first_thread_pred(const int idx, struct rq_queue *const q, void *const data)
{
	struct thread **const tdp = data;
	struct thread *const td = TAILQ_FIRST(q);

	*tdp = td;
	return (true);
}

/*
 * Inline this function for the benefit of this file's internal uses, but make
 * sure it has an external definition as it is exported.
 */
extern inline struct thread *
runq_first_thread_range(struct runq *const rq, const int lvl_min,
    const int lvl_max)
{
	struct thread *td = NULL;

	(void)runq_findq(rq, lvl_min, lvl_max, runq_first_thread_pred, &td);
	return (td);
}

static inline struct thread *
runq_first_thread(struct runq *const rq)
{

	return (runq_first_thread_range(rq, 0, RQ_NQS - 1));
}

/*
 * Return true if there are some processes of any priority on the run queue,
 * false otherwise.  Has no side effects.  Supports racy lookups (required by
 * 4BSD).
 */
bool
runq_not_empty(struct runq *rq)
{
	const rqsw_t (*const rqsw)[RQSW_NB] = &rq->rq_status.rq_sw;
	int sw_idx;

	for (sw_idx = 0; sw_idx < RQSW_NB; ++sw_idx) {
		const rqsw_t w = (*rqsw)[sw_idx];

		if (w != 0) {
			CTR3(KTR_RUNQ, "runq_not_empty: not empty; "
			    "rq=%p, sw_idx=%d, bits=" RQSW_PRI,
			    rq, sw_idx, w);
			return (true);
		}
	}

	CTR1(KTR_RUNQ, "runq_not_empty: empty; rq=%p", rq);
	return (false);
}

/*
 * Find the highest priority process on the run queue.
 */
struct thread *
runq_choose(struct runq *rq)
{
	struct thread *td;

	td = runq_first_thread(rq);
	if (td != NULL) {
		CTR2(KTR_RUNQ, "runq_choose: idx=%d td=%p", td->td_rqindex, td);
		return (td);
	}

	CTR0(KTR_RUNQ, "runq_choose: idlethread");
	return (NULL);
}

struct runq_fuzz_pred_data {
	int fuzz;
	struct thread *td;
};

static bool
runq_fuzz_pred(const int idx, struct rq_queue *const q, void *const data)
{
	struct runq_fuzz_pred_data *const d = data;
	const int fuzz = d->fuzz;
	struct thread *td;

	td = TAILQ_FIRST(q);

	if (fuzz > 1) {
		/*
		 * In the first couple of entries, check if
		 * there is one for our CPU as a preference.
		 */
		struct thread *td2 = td;
		int count = fuzz;
		int cpu = PCPU_GET(cpuid);

		while (count-- != 0 && td2 != NULL) {
			if (td2->td_lastcpu == cpu) {
				td = td2;
				break;
			}
			td2 = TAILQ_NEXT(td2, td_runq);
		}
	}

	d->td = td;
	return (true);
}

/*
 * Find the highest priority process on the run queue.
 */
struct thread *
runq_choose_fuzz(struct runq *rq, int fuzz)
{
	struct runq_fuzz_pred_data data = {
		.fuzz = fuzz,
		.td = NULL
	};
	int idx;

	idx = runq_findq(rq, 0, RQ_NQS - 1, runq_fuzz_pred, &data);
	if (idx != -1) {
		MPASS(data.td != NULL);
		CTR2(KTR_RUNQ, "runq_choose_fuzz: idx=%d td=%p", idx, data.td);
		return (data.td);
	}

	MPASS(data.td == NULL);
	CTR0(KTR_RUNQ, "runq_choose_fuzz: idlethread");
	return (NULL);
}
