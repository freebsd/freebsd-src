/*
 * Copyright (c) 1999 Peter Wemm <peter@FreeBSD.org>
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
#include <sys/proc.h>
#include <sys/rtprio.h>
#include <sys/queue.h>

#include <machine/mutex.h>

/*
 * We have NQS (32) run queues per scheduling class.  For the normal
 * class, there are 128 priorities scaled onto these 32 queues.  New
 * processes are added to the last entry in each queue, and processes
 * are selected for running by taking them from the head and maintaining
 * a simple FIFO arrangement.
 *
 * Interrupt, real time and idle priority processes have and explicit
 * 0-31 priority which maps directly onto their class queue index.
 * When a queue has something in it, the corresponding bit is set in
 * the queuebits variable, allowing a single read to determine the
 * state of all 32 queues and then a ffs() to find the first busy
 * queue.
 *
 * XXX This needs fixing.  First, we only have one idle process, so we
 * hardly need 32 queues for it.  Secondly, the number of classes
 * makes things unwieldy.  We should be able to merge them into a
 * single 96 or 128 entry queue.
 */
struct rq itqueues[NQS];		/* interrupt threads */
struct rq rtqueues[NQS];		/* real time processes */
struct rq queues[NQS];			/* time sharing processes */
struct rq idqueues[NQS];		/* idle process */
u_int32_t itqueuebits;
u_int32_t rtqueuebits;
u_int32_t queuebits;
u_int32_t idqueuebits;

/*
 * Initialize the run queues at boot time.
 */
static void
rqinit(void *dummy)
{
	int i;

	for (i = 0; i < NQS; i++) {
		TAILQ_INIT(&itqueues[i]);
		TAILQ_INIT(&rtqueues[i]);
		TAILQ_INIT(&queues[i]);
		TAILQ_INIT(&idqueues[i]);
	}
}
SYSINIT(runqueue, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, rqinit, NULL)

/*
 * setrunqueue() examines a process priority and class and inserts it on
 * the tail of it's appropriate run queue (based on class and priority).
 * This sets the queue busy bit.
 * The process must be runnable.
 * This must be called at splhigh().
 */
void
setrunqueue(struct proc *p)
{
	struct rq *q;
	u_int8_t pri;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(p->p_stat == SRUN, ("setrunqueue: proc %p (%s) not SRUN", p, \
	    p->p_comm));

	/*
	 * Decide which class we want to run.  We now have four
	 * queues, and this is becoming ugly.  We should be able to
	 * collapse the first three classes into a single contiguous
	 * queue.  XXX FIXME.
	 */
	CTR4(KTR_PROC, "setrunqueue: proc %p (pid %d, %s), schedlock %lx",
		p, p->p_pid, p->p_comm, (long)sched_lock.mtx_lock);
	if (p->p_rtprio.type == RTP_PRIO_ITHREAD) {	/* interrupt thread */
		pri = p->p_rtprio.prio;
		q = &itqueues[pri];
		itqueuebits |= 1 << pri;
	} else if (p->p_rtprio.type == RTP_PRIO_REALTIME || /* real time */
		   p->p_rtprio.type == RTP_PRIO_FIFO) {
		pri = p->p_rtprio.prio;
		q = &rtqueues[pri];
		rtqueuebits |= 1 << pri;
	} else if (p->p_rtprio.type == RTP_PRIO_NORMAL) {   /* time sharing */
		pri = p->p_priority >> 2;
		q = &queues[pri];
		queuebits |= 1 << pri;
	} else if (p->p_rtprio.type == RTP_PRIO_IDLE) {	    /* idle proc */
		pri = p->p_rtprio.prio;
		q = &idqueues[pri];
		idqueuebits |= 1 << pri;
	} else {
		panic("setrunqueue: invalid rtprio type %d", p->p_rtprio.type);
	}
	p->p_rqindex = pri;		/* remember the queue index */
	TAILQ_INSERT_TAIL(q, p, p_procq);
}

/*
 * remrunqueue() removes a given process from the run queue that it is on,
 * clearing the queue busy bit if it becomes empty.
 * This must be called at splhigh().
 */
void
remrunqueue(struct proc *p)
{
	struct rq *q;
	u_int32_t *which;
	u_int8_t pri;

	CTR4(KTR_PROC, "remrunqueue: proc %p (pid %d, %s), schedlock %lx",
		p, p->p_pid, p->p_comm, (long)sched_lock.mtx_lock);
	mtx_assert(&sched_lock, MA_OWNED);
	pri = p->p_rqindex;
	if (p->p_rtprio.type == RTP_PRIO_ITHREAD) {
		q = &itqueues[pri];
		which = &itqueuebits;
	} else if (p->p_rtprio.type == RTP_PRIO_REALTIME ||
		   p->p_rtprio.type == RTP_PRIO_FIFO) {
		q = &rtqueues[pri];
		which = &rtqueuebits;
	} else if (p->p_rtprio.type == RTP_PRIO_NORMAL) {
		q = &queues[pri];
		which = &queuebits;
	} else if (p->p_rtprio.type == RTP_PRIO_IDLE) {
		q = &idqueues[pri];
		which = &idqueuebits;
	} else {
		panic("remrunqueue: invalid rtprio type");
	}
	TAILQ_REMOVE(q, p, p_procq);
	if (TAILQ_EMPTY(q)) {
		KASSERT((*which & (1 << pri)) != 0,
			("remrunqueue: remove from empty queue"));
		*which &= ~(1 << pri);
	}
}

/*
 * procrunnable() returns a boolean true (non-zero) value if there are
 * any runnable processes.  This is intended to be called from the idle
 * loop to avoid the more expensive (and destructive) chooseproc().
 *
 * MP SAFE.  CALLED WITHOUT THE MP LOCK
 *
 * XXX I doubt this.  It's possibly fail-safe, but there's obviously
 * the case here where one of the bits words gets loaded, the
 * processor gets preempted, and by the time it returns from this
 * function, some other processor has picked the runnable process.
 * What am I missing?  (grog, 23 July 2000).
 */
u_int32_t
procrunnable(void)
{
	return (itqueuebits || rtqueuebits || queuebits || idqueuebits);
}

/*
 * chooseproc() selects the next process to run.  Ideally, cpu_switch()
 * would have determined that there is a process available before calling
 * this, but it is not a requirement.  The selected process is removed
 * from it's queue, and the queue busy bit is cleared if it becomes empty.
 * This must be called at splhigh().
 *
 * For SMP, trivial affinity is implemented by locating the first process
 * on the queue that has a matching lastcpu id.  Since normal priorities
 * are mapped four priority levels per queue, this may allow the cpu to
 * choose a slightly lower priority process in order to preserve the cpu
 * caches.
 */
struct proc *
chooseproc(void)
{
	struct proc *p;
	struct rq *q;
	u_int32_t *which;
	u_int32_t pri;
#ifdef SMP
	u_char id;
#endif

	mtx_assert(&sched_lock, MA_OWNED);
	if (itqueuebits) {
		pri = ffs(itqueuebits) - 1;
		q = &itqueues[pri];
		which = &itqueuebits;
	} else if (rtqueuebits) {
		pri = ffs(rtqueuebits) - 1;
		q = &rtqueues[pri];
		which = &rtqueuebits;
	} else if (queuebits) {
		pri = ffs(queuebits) - 1;
		q = &queues[pri];
		which = &queuebits;
	} else if (idqueuebits) {
		pri = ffs(idqueuebits) - 1;
		q = &idqueues[pri];
		which = &idqueuebits;
	} else {
		CTR1(KTR_PROC, "chooseproc: idleproc, schedlock %lx",
			(long)sched_lock.mtx_lock);
		return idleproc;
	}
	p = TAILQ_FIRST(q);
#ifdef SMP
	/* wander down the current run queue for this pri level for a match */
	id = cpuid;
	while (p->p_lastcpu != id) {
		p = TAILQ_NEXT(p, p_procq);
		if (p == NULL) {
			p = TAILQ_FIRST(q);
			break;
		}
	}
#endif
	CTR4(KTR_PROC, "chooseproc: proc %p (pid %d, %s), schedlock %lx",
		p, p->p_pid, p->p_comm, (long)sched_lock.mtx_lock);
	KASSERT(p, ("chooseproc: no proc on busy queue"));
	TAILQ_REMOVE(q, p, p_procq);
	if (TAILQ_EMPTY(q))
		*which &= ~(1 << pri);
	return p;
}
