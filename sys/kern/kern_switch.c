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

/***

Here is the logic..

If there are N processors, then there are at most N KSEs (kernel
schedulable entities) working to process threads that belong to a
KSEGOUP (kg). If there are X of these KSEs actually running at the
moment in question, then there are at most M (N-X) of these KSEs on
the run queue, as running KSEs are not on the queue.

Runnable threads are queued off the KSEGROUP in priority order.
If there are M or more threads runnable, the top M threads
(by priority) are 'preassigned' to the M KSEs not running. The KSEs take
their priority from those threads and are put on the run queue.

The last thread that had a priority high enough to have a KSE associated
with it, AND IS ON THE RUN QUEUE is pointed to by
kg->kg_last_assigned. If no threads queued off the KSEGROUP have KSEs
assigned as all the available KSEs are activly running, or because there
are no threads queued, that pointer is NULL.

When a KSE is removed from the run queue to become runnable, we know
it was associated with the highest priority thread in the queue (at the head
of the queue). If it is also the last assigned we know M was 1 and must
now be 0. Since the thread is no longer queued that pointer must be
removed from it. Since we know there were no more KSEs available,
(M was 1 and is now 0) and since we are not FREEING our KSE
but using it, we know there are STILL no more KSEs available, we can prove
that the next thread in the ksegrp list will not have a KSE to assign to
it, so we can show that the pointer must be made 'invalid' (NULL).

The pointer exists so that when a new thread is made runnable, it can
have its priority compared with the last assigned thread to see if
it should 'steal' its KSE or not.. i.e. is it 'earlier'
on the list than that thread or later.. If it's earlier, then the KSE is
removed from the last assigned (which is now not assigned a KSE)
and reassigned to the new thread, which is placed earlier in the list.
The pointer is then backed up to the previous thread (which may or may not
be the new thread).

When a thread sleeps or is removed, the KSE becomes available and if there 
are queued threads that are not assigned KSEs, the highest priority one of
them is assigned the KSE, which is then placed back on the run queue at
the approipriate place, and the kg->kg_last_assigned pointer is adjusted down
to point to it.

The following diagram shows 2 KSEs and 3 threads from a single process.

 RUNQ: --->KSE---KSE--...    (KSEs queued at priorities from threads)
              \    \____   
               \        \
    KSEGROUP---thread--thread--thread    (queued in priority order)
        \                 / 
         \_______________/
          (last_assigned)

The result of this scheme is that the M available KSEs are always
queued at the priorities they have inherrited from the M highest priority
threads for that KSEGROUP. If this situation changes, the KSEs are 
reassigned to keep this true.
   
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <machine/critical.h>

CTASSERT((RQB_BPW * RQB_LEN) == RQ_NQS);

/*
 * Global run queue.
 */
static struct runq runq;
SYSINIT(runq, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, runq_init, &runq)

static void runq_readjust(struct runq *rq, struct kse *ke);
/************************************************************************
 * Functions that manipulate runnability from a thread perspective.	*
 ************************************************************************/

/*
 * Select the KSE that will be run next.  From that find the thread, and x
 * remove it from the KSEGRP's run queue.  If there is thread clustering,
 * this will be what does it.
 */
struct thread *
choosethread(void)
{
	struct kse *ke;
	struct thread *td;
	struct ksegrp *kg;

retry:
	if ((ke = runq_choose(&runq))) {
		td = ke->ke_thread;
		KASSERT((td->td_kse == ke), ("kse/thread mismatch"));
		kg = ke->ke_ksegrp;
		if (td->td_flags & TDF_UNBOUND) {
			TAILQ_REMOVE(&kg->kg_runq, td, td_runq);
			if (kg->kg_last_assigned == td) 
				if (TAILQ_PREV(td, threadqueue, td_runq)
				    != NULL)
					printf("Yo MAMA!\n");
				kg->kg_last_assigned = TAILQ_PREV(td,
				    threadqueue, td_runq);
			/*
			 *  If we have started running an upcall,
			 * Then TDF_UNBOUND WAS set because the thread was 
			 * created without a KSE. Now that we have one,
			 * and it is our time to run, we make sure
			 * that BOUND semantics apply for the rest of
			 * the journey to userland, and into the UTS.
			 */
#ifdef	NOTYET
			if (td->td_flags & TDF_UPCALLING) 
				tdf->td_flags &= ~TDF_UNBOUND;
#endif
		}
		kg->kg_runnable--;
		CTR2(KTR_RUNQ, "choosethread: td=%p pri=%d",
		    td, td->td_priority);
	} else {
		/* Simulate runq_choose() having returned the idle thread */
		td = PCPU_GET(idlethread);
		ke = td->td_kse;
		CTR1(KTR_RUNQ, "choosethread: td=%p (idle)", td);
	}
	ke->ke_flags |= KEF_DIDRUN;
	if (panicstr && ((td->td_proc->p_flag & P_SYSTEM) == 0 &&
	    (td->td_flags & TDF_INPANIC) == 0))
		goto retry;
	td->td_state = TDS_RUNNING;
	return (td);
}

/*
 * Given a KSE (now surplus), either assign a new runable thread to it
 * (and put it in the run queue) or put it in the ksegrp's idle KSE list.
 * Assumes the kse is not linked to any threads any more. (has been cleaned).
 */
void
kse_reassign(struct kse *ke)
{
	struct ksegrp *kg;
	struct thread *td;

	kg = ke->ke_ksegrp;

	/*
	 * Find the first unassigned thread
	 * If there is a 'last assigned' then see what's next.
	 * otherwise look at what is first.
	 */
	if ((td = kg->kg_last_assigned)) {
		td = TAILQ_NEXT(td, td_runq);
	} else {
		td = TAILQ_FIRST(&kg->kg_runq);
	}

	/*
	 * If we found one assign it the kse, otherwise idle the kse.
	 */
	if (td) {
		kg->kg_last_assigned = td;
		td->td_kse = ke;
		ke->ke_thread = td;
		runq_add(&runq, ke);
		CTR2(KTR_RUNQ, "kse_reassign: ke%p -> td%p", ke, td);
	} else {
		ke->ke_state = KES_IDLE;
		ke->ke_thread = NULL;
		TAILQ_INSERT_HEAD(&kg->kg_iq, ke, ke_kgrlist);
		kg->kg_idle_kses++;
		CTR1(KTR_RUNQ, "kse_reassign: ke%p idled", ke);
	}
}

int
kserunnable(void)
{
	return runq_check(&runq);
}

/*
 * Remove a thread from its KSEGRP's run queue.
 * This in turn may remove it from a KSE if it was already assigned
 * to one, possibly causing a new thread to be assigned to the KSE
 * and the KSE getting a new priority (unless it's a BOUND thread/KSE pair).
 */
void
remrunqueue(struct thread *td)
{
	struct thread *td2, *td3;
	struct ksegrp *kg;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT ((td->td_state == TDS_RUNQ),
		("remrunqueue: Bad state on run queue"));
	kg = td->td_ksegrp;
	ke = td->td_kse;
	/*
	 * If it's a bound thread/KSE pair, take the shortcut. All non-KSE
	 * threads are BOUND.
	 */
	CTR1(KTR_RUNQ, "remrunqueue: td%p", td);
	td->td_state = TDS_UNQUEUED;
	kg->kg_runnable--;
	if ((td->td_flags & TDF_UNBOUND) == 0)  {
		/* Bring its kse with it, leave the thread attached */
		runq_remove(&runq, ke);
		ke->ke_state = KES_THREAD; 
		return;
	}
	if (ke) {
		/*
		 * This thread has been assigned to a KSE.
		 * We need to dissociate it and try assign the
		 * KSE to the next available thread. Then, we should
		 * see if we need to move the KSE in the run queues.
		 */
		td2 = kg->kg_last_assigned;
		KASSERT((td2 != NULL), ("last assigned has wrong value "));
		td->td_kse = NULL;
		if ((td3 = TAILQ_NEXT(td2, td_runq))) {
			KASSERT(td3 != td, ("td3 somehow matched td"));
			/*
			 * Give the next unassigned thread to the KSE
			 * so the number of runnable KSEs remains
			 * constant.
			 */
			td3->td_kse = ke;
			ke->ke_thread = td3;
			kg->kg_last_assigned = td3;
			runq_readjust(&runq, ke);
		} else {
			/*
			 * There is no unassigned thread.
			 * If we were the last assigned one,
			 * adjust the last assigned pointer back
			 * one, which may result in NULL.
			 */
			if (td == td2) {
				kg->kg_last_assigned =
				    TAILQ_PREV(td, threadqueue, td_runq);
			}
			runq_remove(&runq, ke);
			KASSERT((ke->ke_state != KES_IDLE),
			    ("kse already idle"));
			ke->ke_state = KES_IDLE;
			ke->ke_thread = NULL;
			TAILQ_INSERT_HEAD(&kg->kg_iq, ke, ke_kgrlist);
			kg->kg_idle_kses++;
		}
	}
	TAILQ_REMOVE(&kg->kg_runq, td, td_runq);
}

void
setrunqueue(struct thread *td)
{
	struct kse *ke;
	struct ksegrp *kg;
	struct thread *td2;
	struct thread *tda;

	CTR1(KTR_RUNQ, "setrunqueue: td%p", td);
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((td->td_state != TDS_RUNQ), ("setrunqueue: bad thread state"));
	td->td_state = TDS_RUNQ;
	kg = td->td_ksegrp;
	kg->kg_runnable++;
	if ((td->td_flags & TDF_UNBOUND) == 0) {
		KASSERT((td->td_kse != NULL),
		    ("queueing BAD thread to run queue"));
		/*
		 * Common path optimisation: Only one of everything
		 * and the KSE is always already attached.
		 * Totally ignore the ksegrp run queue.
		 */
		runq_add(&runq, td->td_kse);
		return;
	}
	/* 
	 * Ok, so we are threading with this thread.
	 * We don't have a KSE, see if we can get one..
	 */
	tda = kg->kg_last_assigned;
	if ((ke = td->td_kse) == NULL) {
		/*
		 * We will need a KSE, see if there is one..
		 * First look for a free one, before getting desperate.
		 * If we can't get one, our priority is not high enough..
		 * that's ok..
		 */
		if (kg->kg_idle_kses) {
			/*
			 * There is a free one so it's ours for the asking..
			 */
			ke = TAILQ_FIRST(&kg->kg_iq);
			TAILQ_REMOVE(&kg->kg_iq, ke, ke_kgrlist);
			ke->ke_state = KES_THREAD;
			kg->kg_idle_kses--;
		} else if (tda && (tda->td_priority > td->td_priority)) {
			/*
			 * None free, but there is one we can commandeer.
			 */
			ke = tda->td_kse;
			tda->td_kse = NULL;
			ke->ke_thread = NULL;
			tda = kg->kg_last_assigned =
		    	    TAILQ_PREV(tda, threadqueue, td_runq);
			runq_remove(&runq, ke);
		}
	} else {
		/* 
		 * Temporarily disassociate so it looks like the other cases.
		 */
		ke->ke_thread = NULL;
		td->td_kse = NULL;
	}

	/*
	 * Add the thread to the ksegrp's run queue at
	 * the appropriate place.
	 */
	TAILQ_FOREACH(td2, &kg->kg_runq, td_runq) {
		if (td2->td_priority > td->td_priority) {
			TAILQ_INSERT_BEFORE(td2, td, td_runq);
			break;
		}
	}
	if (td2 == NULL) {
		/* We ran off the end of the TAILQ or it was empty. */
		TAILQ_INSERT_TAIL(&kg->kg_runq, td, td_runq);
	}

	/*
	 * If we have a ke to use, then put it on the run queue and
	 * If needed, readjust the last_assigned pointer.
	 */
	if (ke) {
		if (tda == NULL) {
			/*
			 * No pre-existing last assigned so whoever is first
			 * gets the KSE we brought in.. (maybe us)
			 */
			td2 = TAILQ_FIRST(&kg->kg_runq);
			KASSERT((td2->td_kse == NULL),
			    ("unexpected ke present"));
			td2->td_kse = ke;
			ke->ke_thread = td2;
			kg->kg_last_assigned = td2;
		} else if (tda->td_priority > td->td_priority) {
			/*
			 * It's ours, grab it, but last_assigned is past us
			 * so don't change it.
			 */
			td->td_kse = ke;
			ke->ke_thread = td;
		} else {
			/* 
			 * We are past last_assigned, so 
			 * put the new kse on whatever is next,
			 * which may or may not be us.
			 */
			td2 = TAILQ_NEXT(tda, td_runq);
			kg->kg_last_assigned = td2;
			td2->td_kse = ke;
			ke->ke_thread = td2;
		}
		runq_add(&runq, ke);
	}
}

/************************************************************************
 * Critical section marker functions					*
 ************************************************************************/
/* Critical sections that prevent preemption. */
void
critical_enter(void)
{
	struct thread *td;

	td = curthread;
	if (td->td_critnest == 0)
		cpu_critical_enter();
	td->td_critnest++;
}

void
critical_exit(void)
{
	struct thread *td;

	td = curthread;
	if (td->td_critnest == 1) {
		td->td_critnest = 0;
		cpu_critical_exit();
	} else {
		td->td_critnest--;
	}
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
 * Add the KSE to the queue specified by its priority, and set the
 * corresponding status bit.
 */
void
runq_add(struct runq *rq, struct kse *ke)
{
	struct rqhead *rqh;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_thread != NULL), ("runq_add: No thread on KSE"));
	KASSERT((ke->ke_thread->td_kse != NULL),
	    ("runq_add: No KSE on thread"));
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("runq_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
		("runq_add: process swapped out"));
	pri = ke->ke_thread->td_priority / RQ_PPQ;
	ke->ke_rqindex = pri;
	runq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	CTR4(KTR_RUNQ, "runq_add: p=%p pri=%d %d rqh=%p",
	    ke->ke_proc, ke->ke_thread->td_priority, pri, rqh);
	TAILQ_INSERT_TAIL(rqh, ke, ke_procq);
	ke->ke_ksegrp->kg_runq_kses++;
	ke->ke_state = KES_ONRUNQ;
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
struct kse *
runq_choose(struct runq *rq)
{
	struct rqhead *rqh;
	struct kse *ke;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	while ((pri = runq_findbit(rq)) != -1) {
		rqh = &rq->rq_queues[pri];
		ke = TAILQ_FIRST(rqh);
		KASSERT(ke != NULL, ("runq_choose: no proc on busy queue"));
		CTR3(KTR_RUNQ,
		    "runq_choose: pri=%d kse=%p rqh=%p", pri, ke, rqh);
		TAILQ_REMOVE(rqh, ke, ke_procq);
		ke->ke_ksegrp->kg_runq_kses--;
		if (TAILQ_EMPTY(rqh)) {
			CTR0(KTR_RUNQ, "runq_choose: empty");
			runq_clrbit(rq, pri);
		}

		ke->ke_state = KES_THREAD;
		KASSERT((ke->ke_thread != NULL),
		    ("runq_choose: No thread on KSE"));
		KASSERT((ke->ke_thread->td_kse != NULL),
		    ("runq_choose: No KSE on thread"));
		KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
			("runq_choose: process swapped out"));
		return (ke);
	}
	CTR1(KTR_RUNQ, "runq_choose: idleproc pri=%d", pri);

	return (NULL);
}

/*
 * Remove the KSE from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 * Caller must set ke->ke_state afterwards.
 */
void
runq_remove(struct runq *rq, struct kse *ke)
{
	struct rqhead *rqh;
	int pri;

	KASSERT((ke->ke_state == KES_ONRUNQ), ("KSE not on run queue"));
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
		("runq_remove: process swapped out"));
	pri = ke->ke_rqindex;
	rqh = &rq->rq_queues[pri];
	CTR4(KTR_RUNQ, "runq_remove: p=%p pri=%d %d rqh=%p",
	    ke, ke->ke_thread->td_priority, pri, rqh);
	KASSERT(ke != NULL, ("runq_remove: no proc on busy queue"));
	TAILQ_REMOVE(rqh, ke, ke_procq);
	if (TAILQ_EMPTY(rqh)) {
		CTR0(KTR_RUNQ, "runq_remove: empty");
		runq_clrbit(rq, pri);
	}
	ke->ke_state = KES_THREAD; 
	ke->ke_ksegrp->kg_runq_kses--;
}

static void 
runq_readjust(struct runq *rq, struct kse *ke)
{

	if (ke->ke_rqindex != (ke->ke_thread->td_priority / RQ_PPQ)) {
		runq_remove(rq, ke);
		runq_add(rq, ke);
	}
}

#if 0
void
thread_sanity_check(struct thread *td)
{
	struct proc *p;
	struct ksegrp *kg;
	struct kse *ke;
	struct thread *td2;
	unsigned int prevpri;
	int	saw_lastassigned;
	int unassigned;
	int assigned;

	p = td->td_proc;
	kg = td->td_ksegrp;
	ke = td->td_kse;

	if (kg != &p->p_ksegrp) {
		panic ("wrong ksegrp");
	}

	if (ke) {
		if (ke != &p->p_kse) {
			panic("wrong kse");
		}
		if (ke->ke_thread != td) {
			panic("wrong thread");
		}
	}
	
	if ((p->p_flag & P_KSES) == 0) {
		if (ke == NULL) {
			panic("non KSE thread lost kse");
		}
	} else {
		prevpri = 0;
		saw_lastassigned = 0;
		unassigned = 0;
		assigned = 0;
		TAILQ_FOREACH(td2, &kg->kg_runq, td_runq) {
			if (td2->td_priority < prevpri) {
				panic("thread runqueue unosorted");
			}
			prevpri = td2->td_priority;
			if (td2->td_kse) {
				assigned++;
				if (unassigned) {
					panic("unassigned before assigned");
				}
 				if  (kg->kg_last_assigned == NULL) {
					panic("lastassigned corrupt");
				}
				if (saw_lastassigned) {
					panic("last assigned not last");
				}
				if (td2->td_kse->ke_thread != td2) {
					panic("mismatched kse/thread");
				}
			} else {
				unassigned++;
			}
			if (td2 == kg->kg_last_assigned) {
				saw_lastassigned = 1;
				if (td2->td_kse == NULL) {
					panic("last assigned not assigned");
				}
			}
		}
		if (kg->kg_last_assigned && (saw_lastassigned == 0)) {
			panic("where on earth does lastassigned point?");
		}
		FOREACH_THREAD_IN_GROUP(kg, td2) {
			if (((td2->td_flags & TDF_UNBOUND) == 0) && 
			    (td2->td_state == TDS_RUNQ)) {
				assigned++;
				if (td2->td_kse == NULL) {
					panic ("BOUND thread with no KSE");
				}
			}
		}
#if 0
		if ((unassigned + assigned) != kg->kg_runnable) {
			panic("wrong number in runnable");
		}
#endif
	}
}
#endif

