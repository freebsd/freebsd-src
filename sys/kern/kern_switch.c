/*
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>

/*
 * Global run queue.
 */
static struct runq runq;
SYSINIT(runq, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, runq_init, &runq)

/*
 * Wrappers which implement old interface; act on global run queue.
 */

struct proc *
chooseproc(void)
{
	return runq_choose(&runq);
}

int
procrunnable(void)
{
	return runq_check(&runq);
}

void
remrunqueue(struct proc *p)
{
	runq_remove(&runq, p);
}

void
setrunqueue(struct proc *p)
{
	runq_add(&runq, p);
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
			pri = (RQB_FFS(rqb->rqb_bits[i]) - 1) +
			    (i << RQB_L2BPW);
			CTR3(KTR_RUNQ, "runq_findbit: bits=%#x i=%d pri=%d",
			    rqb->rqb_bits[i], i, pri);
			return (pri);
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

#ifdef INVARIANT_SUPPORT
/*
 * Return true if the specified process is already in the run queue.
 */
static __inline int
runq_find(struct runq *rq, struct proc *p)
{
	struct proc *p2;
	int i;

	mtx_assert(&sched_lock, MA_OWNED);
	for (i = 0; i < RQB_LEN; i++)
		TAILQ_FOREACH(p2, &rq->rq_queues[i], p_procq)
		    if (p2 == p)
			    return 1;
	return 0;
}
#endif

/*
 * Add the process to the queue specified by its priority, and set the
 * corresponding status bit.
 */
void
runq_add(struct runq *rq, struct proc *p)
{
	struct rqhead *rqh;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(p->p_stat == SRUN, ("runq_add: proc %p (%s) not SRUN",
	    p, p->p_comm));
	KASSERT(runq_find(rq, p) == 0,
	    ("runq_add: proc %p (%s) already in run queue", p, p->p_comm));
	pri = p->p_pri.pri_level / RQ_PPQ;
	p->p_rqindex = pri;
	runq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	CTR4(KTR_RUNQ, "runq_add: p=%p pri=%d %d rqh=%p",
	    p, p->p_pri.pri_level, pri, rqh);
	TAILQ_INSERT_TAIL(rqh, p, p_procq);
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
 * Find and remove the highest priority process from the run queue.
 * If there are no runnable processes, the per-cpu idle process is
 * returned.  Will not return NULL under any circumstances.
 */
struct proc *
runq_choose(struct runq *rq)
{
	struct rqhead *rqh;
	struct proc *p;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	if ((pri = runq_findbit(rq)) != -1) {
		rqh = &rq->rq_queues[pri];
		p = TAILQ_FIRST(rqh);
		KASSERT(p != NULL, ("runq_choose: no proc on busy queue"));
		KASSERT(p->p_stat == SRUN,
		    ("runq_choose: process %d(%s) in state %d", p->p_pid,
		    p->p_comm, p->p_stat));
		CTR3(KTR_RUNQ, "runq_choose: pri=%d p=%p rqh=%p", pri, p, rqh);
		TAILQ_REMOVE(rqh, p, p_procq);
		if (TAILQ_EMPTY(rqh)) {
			CTR0(KTR_RUNQ, "runq_choose: empty");
			runq_clrbit(rq, pri);
		}
		return (p);
	}
	CTR1(KTR_RUNQ, "runq_choose: idleproc pri=%d", pri);

	return (PCPU_GET(idleproc));
}

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
 * Remove the process from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 */
void
runq_remove(struct runq *rq, struct proc *p)
{
	struct rqhead *rqh;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	pri = p->p_rqindex;
	rqh = &rq->rq_queues[pri];
	CTR4(KTR_RUNQ, "runq_remove: p=%p pri=%d %d rqh=%p",
	    p, p->p_pri.pri_level, pri, rqh);
	KASSERT(p != NULL, ("runq_remove: no proc on busy queue"));
	TAILQ_REMOVE(rqh, p, p_procq);
	if (TAILQ_EMPTY(rqh)) {
		CTR0(KTR_RUNQ, "runq_remove: empty");
		runq_clrbit(rq, pri);
	}
}
