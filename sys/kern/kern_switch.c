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
 */

/***
Here is the logic..

If there are N processors, then there are at most N KSEs (kernel
schedulable entities) working to process threads that belong to a
KSEGROUP (kg). If there are X of these KSEs actually running at the
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
***/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_full_preemption.h"
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
#include <sys/sched.h>
#if defined(SMP) && (defined(__i386__) || defined(__amd64__))
#include <sys/smp.h>
#endif
#include <machine/critical.h>
#if defined(SMP) && defined(SCHED_4BSD)
#include <sys/sysctl.h>
#endif


CTASSERT((RQB_BPW * RQB_LEN) == RQ_NQS);

void panc(char *string1, char *string2);

#if 0
static void runq_readjust(struct runq *rq, struct kse *ke);
#endif
/************************************************************************
 * Functions that manipulate runnability from a thread perspective.	*
 ************************************************************************/
/*
 * Select the KSE that will be run next.  From that find the thread, and
 * remove it from the KSEGRP's run queue.  If there is thread clustering,
 * this will be what does it.
 */
struct thread *
choosethread(void)
{
	struct kse *ke;
	struct thread *td;
	struct ksegrp *kg;

#if defined(SMP) && (defined(__i386__) || defined(__amd64__))
	if (smp_active == 0 && PCPU_GET(cpuid) != 0) {
		/* Shutting down, run idlethread on AP's */
		td = PCPU_GET(idlethread);
		ke = td->td_kse;
		CTR1(KTR_RUNQ, "choosethread: td=%p (idle)", td);
		ke->ke_flags |= KEF_DIDRUN;
		TD_SET_RUNNING(td);
		return (td);
	}
#endif

retry:
	ke = sched_choose();
	if (ke) {
		td = ke->ke_thread;
		KASSERT((td->td_kse == ke), ("kse/thread mismatch"));
		kg = ke->ke_ksegrp;
		if (td->td_proc->p_flag & P_SA) {
			if (kg->kg_last_assigned == td) {
				kg->kg_last_assigned = TAILQ_PREV(td,
				    threadqueue, td_runq);
			}
			TAILQ_REMOVE(&kg->kg_runq, td, td_runq);
			kg->kg_runnable--;
		}
		CTR2(KTR_RUNQ, "choosethread: td=%p pri=%d",
		    td, td->td_priority);
	} else {
		/* Simulate runq_choose() having returned the idle thread */
		td = PCPU_GET(idlethread);
		ke = td->td_kse;
		CTR1(KTR_RUNQ, "choosethread: td=%p (idle)", td);
	}
	ke->ke_flags |= KEF_DIDRUN;

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
 * Given a surplus KSE, either assign a new runable thread to it
 * (and put it in the run queue) or put it in the ksegrp's idle KSE list.
 * Assumes that the original thread is not runnable.
 */
void
kse_reassign(struct kse *ke)
{
	struct ksegrp *kg;
	struct thread *td;
	struct thread *original;

	mtx_assert(&sched_lock, MA_OWNED);
	original = ke->ke_thread;
	KASSERT(original == NULL || TD_IS_INHIBITED(original),
    	    ("reassigning KSE with runnable thread"));
	kg = ke->ke_ksegrp;
	if (original)
		original->td_kse = NULL;

	/*
	 * Find the first unassigned thread
	 */
	if ((td = kg->kg_last_assigned) != NULL)
		td = TAILQ_NEXT(td, td_runq);
	else 
		td = TAILQ_FIRST(&kg->kg_runq);

	/*
	 * If we found one, assign it the kse, otherwise idle the kse.
	 */
	if (td) {
		kg->kg_last_assigned = td;
		td->td_kse = ke;
		ke->ke_thread = td;
		CTR2(KTR_RUNQ, "kse_reassign: ke%p -> td%p", ke, td);
		sched_add(td, SRQ_BORING);
		return;
	}

	ke->ke_state = KES_IDLE;
	ke->ke_thread = NULL;
	TAILQ_INSERT_TAIL(&kg->kg_iq, ke, ke_kgrlist);
	kg->kg_idle_kses++;
	CTR1(KTR_RUNQ, "kse_reassign: ke%p on idle queue", ke);
	return;
}

#if 0
/*
 * Remove a thread from its KSEGRP's run queue.
 * This in turn may remove it from a KSE if it was already assigned
 * to one, possibly causing a new thread to be assigned to the KSE
 * and the KSE getting a new priority.
 */
static void
remrunqueue(struct thread *td)
{
	struct thread *td2, *td3;
	struct ksegrp *kg;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((TD_ON_RUNQ(td)), ("remrunqueue: Bad state on run queue"));
	kg = td->td_ksegrp;
	ke = td->td_kse;
	CTR1(KTR_RUNQ, "remrunqueue: td%p", td);
	TD_SET_CAN_RUN(td);
	/*
	 * If it is not a threaded process, take the shortcut.
	 */
	if ((td->td_proc->p_flag & P_SA) == 0) {
		/* Bring its kse with it, leave the thread attached */
		sched_rem(td);
		ke->ke_state = KES_THREAD; 
		return;
	}
   	td3 = TAILQ_PREV(td, threadqueue, td_runq);
	TAILQ_REMOVE(&kg->kg_runq, td, td_runq);
	kg->kg_runnable--;
	if (ke) {
		/*
		 * This thread has been assigned to a KSE.
		 * We need to dissociate it and try assign the
		 * KSE to the next available thread. Then, we should
		 * see if we need to move the KSE in the run queues.
		 */
		sched_rem(td);
		ke->ke_state = KES_THREAD; 
		td2 = kg->kg_last_assigned;
		KASSERT((td2 != NULL), ("last assigned has wrong value"));
		if (td2 == td) 
			kg->kg_last_assigned = td3;
		kse_reassign(ke);
	}
}
#endif

/*
 * Change the priority of a thread that is on the run queue.
 */
void
adjustrunqueue( struct thread *td, int newpri) 
{
	struct ksegrp *kg;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((TD_ON_RUNQ(td)), ("adjustrunqueue: Bad state on run queue"));

	ke = td->td_kse;
	CTR1(KTR_RUNQ, "adjustrunqueue: td%p", td);
	/*
	 * If it is not a threaded process, take the shortcut.
	 */
	if ((td->td_proc->p_flag & P_SA) == 0) {
		/* We only care about the kse in the run queue. */
		td->td_priority = newpri;
		if (ke->ke_rqindex != (newpri / RQ_PPQ)) {
			sched_rem(td);
			sched_add(td, SRQ_BORING);
		}
		return;
	}

	/* It is a threaded process */
	kg = td->td_ksegrp;
	TD_SET_CAN_RUN(td);
	if (ke) {
		if (kg->kg_last_assigned == td) {
			kg->kg_last_assigned =
			    TAILQ_PREV(td, threadqueue, td_runq);
		}
		sched_rem(td);
	}
	TAILQ_REMOVE(&kg->kg_runq, td, td_runq);
	kg->kg_runnable--;
	td->td_priority = newpri;
	setrunqueue(td, SRQ_BORING);
}

void
setrunqueue(struct thread *td, int flags)
{
	struct kse *ke;
	struct ksegrp *kg;
	struct thread *td2;
	struct thread *tda;
	int count;

	CTR4(KTR_RUNQ, "setrunqueue: td:%p ke:%p kg:%p pid:%d",
	    td, td->td_kse, td->td_ksegrp, td->td_proc->p_pid);
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	    ("setrunqueue: bad thread state"));
	TD_SET_RUNQ(td);
	kg = td->td_ksegrp;
	if ((td->td_proc->p_flag & P_SA) == 0) {
		/*
		 * Common path optimisation: Only one of everything
		 * and the KSE is always already attached.
		 * Totally ignore the ksegrp run queue.
		 */
		sched_add(td, flags);
		return;
	}

	tda = kg->kg_last_assigned;
	if ((ke = td->td_kse) == NULL) {
		if (kg->kg_idle_kses) {
			/*
			 * There is a free one so it's ours for the asking..
			 */
			ke = TAILQ_FIRST(&kg->kg_iq);
			CTR2(KTR_RUNQ, "setrunqueue: kg:%p: Use free ke:%p",
			    kg, ke);
			TAILQ_REMOVE(&kg->kg_iq, ke, ke_kgrlist);
			ke->ke_state = KES_THREAD;
			kg->kg_idle_kses--;
		} else if (tda && (tda->td_priority > td->td_priority)) {
			/*
			 * None free, but there is one we can commandeer.
			 */
			ke = tda->td_kse;
			CTR3(KTR_RUNQ,
			    "setrunqueue: kg:%p: take ke:%p from td: %p",
			    kg, ke, tda);
			sched_rem(tda);
			tda->td_kse = NULL;
			ke->ke_thread = NULL;
			tda = kg->kg_last_assigned =
		    	    TAILQ_PREV(tda, threadqueue, td_runq);
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
	count = 0;
	TAILQ_FOREACH(td2, &kg->kg_runq, td_runq) {
		if (td2->td_priority > td->td_priority) {
			kg->kg_runnable++;
			TAILQ_INSERT_BEFORE(td2, td, td_runq);
			break;
		}
		/* XXX Debugging hack */
		if (++count > 10000) {
			printf("setrunqueue(): corrupt kq_runq, td= %p\n", td);
			panic("deadlock in setrunqueue");
		}
	}
	if (td2 == NULL) {
		/* We ran off the end of the TAILQ or it was empty. */
		kg->kg_runnable++;
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
		sched_add(ke->ke_thread, flags);
	} else {
		CTR3(KTR_RUNQ, "setrunqueue: held: td%p kg%p pid%d",
			td, td->td_ksegrp, td->td_proc->p_pid);
	}
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
	if (td->td_critnest == 0)
		cpu_critical_enter(td);
	td->td_critnest++;
}

void
critical_exit(void)
{
	struct thread *td;

	td = curthread;
	KASSERT(td->td_critnest != 0,
	    ("critical_exit: td_critnest == 0"));
	if (td->td_critnest == 1) {
#ifdef PREEMPTION
		mtx_assert(&sched_lock, MA_NOTOWNED);
		if (td->td_pflags & TDP_OWEPREEMPT) {
			mtx_lock_spin(&sched_lock);
			mi_switch(SW_INVOL, NULL);
			mtx_unlock_spin(&sched_lock);
		}
#endif
		td->td_critnest = 0;
		cpu_critical_exit(td);
	} else {
		td->td_critnest--;
	}
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
	if (ctd->td_kse == NULL || ctd->td_kse->ke_thread != ctd)
		return (0);
	pri = td->td_priority;
	cpri = ctd->td_priority;
	if (pri >= cpri || cold /* || dumping */ || TD_IS_INHIBITED(ctd) ||
	    td->td_kse->ke_state != KES_THREAD)
		return (0);
#ifndef FULL_PREEMPTION
	if (!(pri >= PRI_MIN_ITHD && pri <= PRI_MAX_ITHD) &&
	    !(cpri >= PRI_MIN_IDLE))
		return (0);
#endif
	if (ctd->td_critnest > 1) {
		CTR1(KTR_PROC, "maybe_preempt: in critical section %d",
		    ctd->td_critnest);
		ctd->td_pflags |= TDP_OWEPREEMPT;
		return (0);
	}

	/*
	 * Our thread state says that we are already on a run queue, so
	 * update our state as if we had been dequeued by choosethread().
	 */
	MPASS(TD_ON_RUNQ(td));
	TD_SET_RUNNING(td);
	CTR3(KTR_PROC, "preempting to thread %p (pid %d, %s)\n", td,
	    td->td_proc->p_pid, td->td_proc->p_comm);
	mi_switch(SW_INVOL, td);
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

	pri = ke->ke_thread->td_priority / RQ_PPQ;
	ke->ke_rqindex = pri;
	runq_setbit(rq, pri);
	rqh = &rq->rq_queues[pri];
	CTR5(KTR_RUNQ, "runq_add: td=%p ke=%p pri=%d %d rqh=%p",
	    ke->ke_thread, ke, ke->ke_thread->td_priority, pri, rqh);
	TAILQ_INSERT_TAIL(rqh, ke, ke_procq);
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
SYSCTL_DECL(_kern_sched);
SYSCTL_INT(_kern_sched, OID_AUTO, runq_fuzz, CTLFLAG_RW, &runq_fuzz, 0, "");
#endif

/*
 * Find the highest priority process on the run queue.
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
#if defined(SMP) && defined(SCHED_4BSD)
		/* fuzz == 1 is normal.. 0 or less are ignored */
		if (runq_fuzz > 1) {
			/*
			 * In the first couple of entries, check if
			 * there is one for our CPU as a preference.
			 */
			int count = runq_fuzz;
			int cpu = PCPU_GET(cpuid);
			struct kse *ke2;
			ke2 = ke = TAILQ_FIRST(rqh);

			while (count-- && ke2) {
				if (ke->ke_thread->td_lastcpu == cpu) {
					ke = ke2;
					break;
				}
				ke2 = TAILQ_NEXT(ke2, ke_procq);
			}
		} else 
#endif
			ke = TAILQ_FIRST(rqh);
		KASSERT(ke != NULL, ("runq_choose: no proc on busy queue"));
		CTR3(KTR_RUNQ,
		    "runq_choose: pri=%d kse=%p rqh=%p", pri, ke, rqh);
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

	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
		("runq_remove: process swapped out"));
	pri = ke->ke_rqindex;
	rqh = &rq->rq_queues[pri];
	CTR5(KTR_RUNQ, "runq_remove: td=%p, ke=%p pri=%d %d rqh=%p",
	    ke->ke_thread, ke, ke->ke_thread->td_priority, pri, rqh);
	KASSERT(ke != NULL, ("runq_remove: no proc on busy queue"));
	TAILQ_REMOVE(rqh, ke, ke_procq);
	if (TAILQ_EMPTY(rqh)) {
		CTR0(KTR_RUNQ, "runq_remove: empty");
		runq_clrbit(rq, pri);
	}
}

#if 0
void
panc(char *string1, char *string2)
{
	printf("%s", string1);
	kdb_enter(string2);
}

void
thread_sanity_check(struct thread *td, char *string)
{
	struct proc *p;
	struct ksegrp *kg;
	struct kse *ke;
	struct thread *td2 = NULL;
	unsigned int prevpri;
	int	saw_lastassigned = 0;
	int unassigned = 0;
	int assigned = 0;

	p = td->td_proc;
	kg = td->td_ksegrp;
	ke = td->td_kse;


	if (ke) {
		if (p != ke->ke_proc) {
			panc(string, "wrong proc");
		}
		if (ke->ke_thread != td) {
			panc(string, "wrong thread");
		}
	}
	
	if ((p->p_flag & P_SA) == 0) {
		if (ke == NULL) {
			panc(string, "non KSE thread lost kse");
		}
	} else {
		prevpri = 0;
		saw_lastassigned = 0;
		unassigned = 0;
		assigned = 0;
		TAILQ_FOREACH(td2, &kg->kg_runq, td_runq) {
			if (td2->td_priority < prevpri) {
				panc(string, "thread runqueue unosorted");
			}
			if ((td2->td_state == TDS_RUNQ) &&
			    td2->td_kse &&
			    (td2->td_kse->ke_state != KES_ONRUNQ)) {
				panc(string, "KSE wrong state");
			}
			prevpri = td2->td_priority;
			if (td2->td_kse) {
				assigned++;
				if (unassigned) {
					panc(string, "unassigned before assigned");
				}
 				if  (kg->kg_last_assigned == NULL) {
					panc(string, "lastassigned corrupt");
				}
				if (saw_lastassigned) {
					panc(string, "last assigned not last");
				}
				if (td2->td_kse->ke_thread != td2) {
					panc(string, "mismatched kse/thread");
				}
			} else {
				unassigned++;
			}
			if (td2 == kg->kg_last_assigned) {
				saw_lastassigned = 1;
				if (td2->td_kse == NULL) {
					panc(string, "last assigned not assigned");
				}
			}
		}
		if (kg->kg_last_assigned && (saw_lastassigned == 0)) {
			panc(string, "where on earth does lastassigned point?");
		}
#if 0
		FOREACH_THREAD_IN_GROUP(kg, td2) {
			if (((td2->td_flags & TDF_UNBOUND) == 0) && 
			    (TD_ON_RUNQ(td2))) {
				assigned++;
				if (td2->td_kse == NULL) {
					panc(string, "BOUND thread with no KSE");
				}
			}
		}
#endif
#if 0
		if ((unassigned + assigned) != kg->kg_runnable) {
			panc(string, "wrong number in runnable");
		}
#endif
	}
	if (assigned == 12345) {
		printf("%p %p %p %p %p %d, %d",
		    td, td2, ke, kg, p, assigned, saw_lastassigned);
	}
}
#endif

