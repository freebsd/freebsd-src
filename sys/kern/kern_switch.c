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
#include <sys/proc.h>
#include <sys/rtprio.h>
#include <sys/queue.h>

/*
 * We have NQS (32) run queues per scheduling class.  For the normal
 * class, there are 128 priorities scaled onto these 32 queues.  New
 * processes are added to the last entry in each queue, and processes
 * are selected for running by taking them from the head and maintaining
 * a simple FIFO arrangement.  Realtime and Idle priority processes have
 * and explicit 0-31 priority which maps directly onto their class queue
 * index.  When a queue has something in it, the corresponding bit is
 * set in the queuebits variable, allowing a single read to determine
 * the state of all 32 queues and then a ffs() to find the first busy
 * queue.
 */
struct rq queues[NQS];
struct rq rtqueues[NQS];
struct rq idqueues[NQS];
u_int32_t queuebits;
u_int32_t rtqueuebits;
u_int32_t idqueuebits;

/*
 * Initialize the run queues at boot time.
 */
static void
rqinit(void *dummy)
{
	int i;

	for (i = 0; i < NQS; i++) {
		TAILQ_INIT(&queues[i]);
		TAILQ_INIT(&rtqueues[i]);
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

	KASSERT(p->p_stat == SRUN, ("setrunqueue: proc not SRUN"));
	if (p->p_rtprio.type == RTP_PRIO_NORMAL) {
		pri = p->p_priority >> 2;
		q = &queues[pri];
		queuebits |= 1 << pri;
	} else if (p->p_rtprio.type == RTP_PRIO_REALTIME ||
		   p->p_rtprio.type == RTP_PRIO_FIFO) {
		pri = p->p_rtprio.prio;
		q = &rtqueues[pri];
		rtqueuebits |= 1 << pri;
	} else if (p->p_rtprio.type == RTP_PRIO_IDLE) {
		pri = p->p_rtprio.prio;
		q = &idqueues[pri];
		idqueuebits |= 1 << pri;
	} else {
		panic("setrunqueue: invalid rtprio type");
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

	pri = p->p_rqindex;
	if (p->p_rtprio.type == RTP_PRIO_NORMAL) {
		q = &queues[pri];
		which = &queuebits;
	} else if (p->p_rtprio.type == RTP_PRIO_REALTIME ||
		   p->p_rtprio.type == RTP_PRIO_FIFO) {
		q = &rtqueues[pri];
		which = &rtqueuebits;
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
 */
u_int32_t
procrunnable(void)
{
	return (rtqueuebits || queuebits || idqueuebits);
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

	if (rtqueuebits) {
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
		return NULL;
	}
	p = TAILQ_FIRST(q);
	KASSERT(p, ("chooseproc: no proc on busy queue"));
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
	TAILQ_REMOVE(q, p, p_procq);
	if (TAILQ_EMPTY(q))
		*which &= ~(1 << pri);
	return p;
}
