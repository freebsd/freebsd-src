/*-
 * Copyright (c) 1998 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: mutex_witness.c,v 1.1.2.20 2000/04/27 03:10:27 cp Exp $
 *	and BSDI $Id: synch_machdep.c,v 2.3.2.39 2000/04/27 03:10:25 cp Exp $
 */

/*
 * Implementation of turnstiles used to hold queue of threads blocked on
 * non-sleepable locks.  Sleepable locks use condition variables to
 * implement their queues.  Turnstiles differ from a sleep queue in that
 * turnstile queue's are assigned to a lock held by an owning thread.  Thus,
 * when one thread is enqueued onto a turnstile, it can lend its priority
 * to the owning thread.
 *
 * We wish to avoid bloating locks with an embedded turnstile and we do not
 * want to use back-pointers in the locks for the same reason.  Thus, we
 * use a similar approach to that of Solaris 7 as described in Solaris
 * Internals by Jim Mauro and Richard McDougall.  Turnstiles are looked up
 * in a hash table based on the address of the lock.  Each entry in the
 * hash table is a linked-lists of turnstiles and is called a turnstile
 * chain.  Each chain contains a spin mutex that protects all of the
 * turnstiles in the chain.
 *
 * Each time a thread is created, a turnstile is malloc'd and attached to
 * that thread.  When a thread blocks on a lock, if it is the first thread
 * to block, it lends its turnstile to the lock.  If the lock already has
 * a turnstile, then it gives its turnstile to the lock's turnstile's free
 * list.  When a thread is woken up, it takes a turnstile from the free list
 * if there are any other waiters.  If it is the only thread blocked on the
 * lock, then it reclaims the turnstile associated with the lock and removes
 * it from the hash table.
 *
 * XXX: We should probably implement some sort of sleep queue that condition
 * variables and sleepqueue's share.  On Solaris condition variables are
 * implemented using a hash table of sleep queues similar to our current
 * sleep queues.  We might want to investigate doing that ourselves.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
#include <sys/turnstile.h>
#include <sys/sched.h>

/*
 * Constants for the hash table of turnstile chains.  TC_SHIFT is a magic
 * number chosen because the sleep queue's use the same value for the
 * shift.  Basically, we ignore the lower 8 bits of the address.
 * TC_TABLESIZE must be a power of two for TC_MASK to work properly.
 */
#define	TC_TABLESIZE	128			/* Must be power of 2. */
#define	TC_MASK		(TC_TABLESIZE - 1)
#define	TC_SHIFT	8
#define	TC_HASH(lock)	(((uintptr_t)(lock) >> TC_SHIFT) & TC_MASK)
#define	TC_LOOKUP(lock)	&turnstile_chains[TC_HASH(lock)]

/*
 * There are three different lists of turnstiles as follows.  The list
 * connected by ts_link entries is a per-thread list of all the turnstiles
 * attached to locks that we own.  This is used to fixup our priority when
 * a lock is released.  The other two lists use the ts_hash entries.  The
 * first of these two is turnstile chain list that a turnstile is on when
 * it is attached to a lock.  The second list to use ts_hash is the free
 * list hung off a turnstile that is attached to a lock.
 *
 * Each turnstile contains two lists of threads.  The ts_blocked list is
 * a linked list of threads blocked on the turnstile's lock.  The
 * ts_pending list is a linked list of threads previously awoken by
 * turnstile_signal() or turnstile_wait() that are waiting to be put on
 * the run queue.
 *
 * Locking key:
 *  c - turnstile chain lock
 *  q - td_contested lock
 */
struct turnstile {
	TAILQ_HEAD(, thread) ts_blocked;	/* (c + q) Blocked threads. */
	TAILQ_HEAD(, thread) ts_pending;	/* (c) Pending threads. */
	LIST_ENTRY(turnstile) ts_hash;		/* (c) Chain and free list. */
	LIST_ENTRY(turnstile) ts_link;		/* (q) Contested locks. */
	LIST_HEAD(, turnstile) ts_free;		/* (c) Free turnstiles. */
	struct lock_object *ts_lockobj;		/* (c) Lock we reference. */
	struct thread *ts_owner;		/* (c + q) Who owns the lock. */
};

struct turnstile_chain {
	LIST_HEAD(, turnstile) tc_turnstiles;	/* List of turnstiles. */
	struct mtx tc_lock;			/* Spin lock for this chain. */
};

static struct mtx td_contested_lock;
static struct turnstile_chain turnstile_chains[TC_TABLESIZE];

MALLOC_DEFINE(M_TURNSTILE, "turnstiles", "turnstiles");

/*
 * Prototypes for non-exported routines.
 */
static void	init_turnstile0(void *dummy);
static void	propagate_priority(struct thread *);
static void	turnstile_setowner(struct turnstile *ts, struct thread *owner);

/*
 * Walks the chain of turnstiles and their owners to propagate the priority
 * of the thread being blocked to all the threads holding locks that have to
 * release their locks before this thread can run again.
 */
static void
propagate_priority(struct thread *td)
{
	struct turnstile_chain *tc;
	struct turnstile *ts;
	struct thread *td1;
	int pri;

	mtx_assert(&sched_lock, MA_OWNED);
	pri = td->td_priority;
	ts = td->td_blocked;
	for (;;) {
		td = ts->ts_owner;

		if (td == NULL) {
			/*
			 * This really isn't quite right. Really
			 * ought to bump priority of thread that
			 * next acquires the lock.
			 */
			return;
		}

		MPASS(td->td_proc != NULL);
		MPASS(td->td_proc->p_magic == P_MAGIC);

		/*
		 * XXX: The owner of a turnstile can be stale if it is the
		 * first thread to grab a slock of a sx lock.  In that case
		 * it is possible for us to be at SSLEEP or some other
		 * weird state.  We should probably just return if the state
		 * isn't SRUN or SLOCK.
		 */
		KASSERT(!TD_IS_SLEEPING(td),
		    ("sleeping thread (pid %d) owns a non-sleepable lock",
		    td->td_proc->p_pid));

		/*
		 * If this thread already has higher priority than the
		 * thread that is being blocked, we are finished.
		 */
		if (td->td_priority <= pri)
			return;

		/*
		 * If lock holder is actually running, just bump priority.
		 */
		if (TD_IS_RUNNING(td)) {
			td->td_priority = pri;
			return;
		}

#ifndef SMP
		/*
		 * For UP, we check to see if td is curthread (this shouldn't
		 * ever happen however as it would mean we are in a deadlock.)
		 */
		KASSERT(td != curthread, ("Deadlock detected"));
#endif

		/*
		 * If on run queue move to new run queue, and quit.
		 * XXXKSE this gets a lot more complicated under threads
		 * but try anyhow.
		 */
		if (TD_ON_RUNQ(td)) {
			MPASS(td->td_blocked == NULL);
			sched_prio(td, pri);
			return;
		}

		/*
		 * Bump this thread's priority.
		 */
		td->td_priority = pri;

		/*
		 * If we aren't blocked on a lock, we should be.
		 */
		KASSERT(TD_ON_LOCK(td), (
		    "process %d(%s):%d holds %s but isn't blocked on a lock\n",
		    td->td_proc->p_pid, td->td_proc->p_comm, td->td_state,
		    ts->ts_lockobj->lo_name));

		/*
		 * Pick up the lock that td is blocked on.
		 */
		ts = td->td_blocked;
		MPASS(ts != NULL);
		tc = TC_LOOKUP(ts->ts_lockobj);
		mtx_lock_spin(&tc->tc_lock);

		/*
		 * This thread may not be blocked on this turnstile anymore
		 * but instead might already be woken up on another CPU
		 * that is waiting on sched_lock in turnstile_unpend() to
		 * finish waking this thread up.  We can detect this case
		 * by checking to see if this thread has been given a
		 * turnstile by either turnstile_signal() or
		 * turnstile_wakeup().  In this case, treat the thread as
		 * if it was already running.
		 */
		if (td->td_turnstile != NULL) {
			mtx_unlock_spin(&tc->tc_lock);
			return;
		}

		/*
		 * Check if the thread needs to be moved up on
		 * the blocked chain.  It doesn't need to be moved
		 * if it is already at the head of the list or if
		 * the item in front of it still has a higher priority.
		 */
		if (td == TAILQ_FIRST(&ts->ts_blocked)) {
			mtx_unlock_spin(&tc->tc_lock);
			continue;
		}

		td1 = TAILQ_PREV(td, threadqueue, td_lockq);
		if (td1->td_priority <= pri) {
			mtx_unlock_spin(&tc->tc_lock);
			continue;
		}

		/*
		 * Remove thread from blocked chain and determine where
		 * it should be moved up to.  Since we know that td1 has
		 * a lower priority than td, we know that at least one
		 * thread in the chain has a lower priority and that
		 * td1 will thus not be NULL after the loop.
		 */
		mtx_lock_spin(&td_contested_lock);
		TAILQ_REMOVE(&ts->ts_blocked, td, td_lockq);
		TAILQ_FOREACH(td1, &ts->ts_blocked, td_lockq) {
			MPASS(td1->td_proc->p_magic == P_MAGIC);
			if (td1->td_priority > pri)
				break;
		}

		MPASS(td1 != NULL);
		TAILQ_INSERT_BEFORE(td1, td, td_lockq);
		mtx_unlock_spin(&td_contested_lock);
		CTR4(KTR_LOCK,
		    "propagate_priority: td %p moved before %p on [%p] %s",
		    td, td1, ts->ts_lockobj, ts->ts_lockobj->lo_name);
		mtx_unlock_spin(&tc->tc_lock);
	}
}

/*
 * Early initialization of turnstiles.  This is not done via a SYSINIT()
 * since this needs to be initialized very early when mutexes are first
 * initialized.
 */
void
init_turnstiles(void)
{
	int i;

	for (i = 0; i < TC_TABLESIZE; i++) {
		LIST_INIT(&turnstile_chains[i].tc_turnstiles);
		mtx_init(&turnstile_chains[i].tc_lock, "turnstile chain",
		    NULL, MTX_SPIN);
	}
	mtx_init(&td_contested_lock, "td_contested", NULL, MTX_SPIN);
	thread0.td_turnstile = NULL;
}

static void
init_turnstile0(void *dummy)
{

	thread0.td_turnstile = turnstile_alloc();
}
SYSINIT(turnstile0, SI_SUB_LOCK, SI_ORDER_ANY, init_turnstile0, NULL);

/*
 * Set the owner of the lock this turnstile is attached to.
 */
static void
turnstile_setowner(struct turnstile *ts, struct thread *owner)
{

	mtx_assert(&td_contested_lock, MA_OWNED);
	MPASS(owner->td_proc->p_magic == P_MAGIC);
	MPASS(ts->ts_owner == NULL);
	ts->ts_owner = owner;
	LIST_INSERT_HEAD(&owner->td_contested, ts, ts_link);
}

/*
 * Malloc a turnstile for a new thread, initialize it and return it.
 */
struct turnstile *
turnstile_alloc(void)
{
	struct turnstile *ts;

	ts = malloc(sizeof(struct turnstile), M_TURNSTILE, M_WAITOK | M_ZERO);
	TAILQ_INIT(&ts->ts_blocked);
	TAILQ_INIT(&ts->ts_pending);
	LIST_INIT(&ts->ts_free);
	return (ts);
}

/*
 * Free a turnstile when a thread is destroyed.
 */
void
turnstile_free(struct turnstile *ts)
{

	MPASS(ts != NULL);
	MPASS(TAILQ_EMPTY(&ts->ts_blocked));
	MPASS(TAILQ_EMPTY(&ts->ts_pending));
	free(ts, M_TURNSTILE);
}

/*
 * Look up the turnstile for a lock in the hash table locking the associated
 * turnstile chain along the way.  Return with the turnstile chain locked.
 * If no turnstile is found in the hash table, NULL is returned.
 */
struct turnstile *
turnstile_lookup(struct lock_object *lock)
{
	struct turnstile_chain *tc;
	struct turnstile *ts;

	tc = TC_LOOKUP(lock);
	mtx_lock_spin(&tc->tc_lock);
	LIST_FOREACH(ts, &tc->tc_turnstiles, ts_hash)
		if (ts->ts_lockobj == lock)
			return (ts);
	return (NULL);
}

/*
 * Unlock the turnstile chain associated with a given lock.
 */
void
turnstile_release(struct lock_object *lock)
{
	struct turnstile_chain *tc;

	tc = TC_LOOKUP(lock);
	mtx_unlock_spin(&tc->tc_lock);
}

/*
 * Take ownership of a turnstile and adjust the priority of the new
 * owner appropriately.
 */
void
turnstile_claim(struct turnstile *ts)
{
	struct turnstile_chain *tc;
	struct thread *td, *owner;

	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);

	owner = curthread;
	mtx_lock_spin(&td_contested_lock);
	turnstile_setowner(ts, owner);
	mtx_unlock_spin(&td_contested_lock);

	td = TAILQ_FIRST(&ts->ts_blocked);
	MPASS(td != NULL);
	MPASS(td->td_proc->p_magic == P_MAGIC);
	mtx_unlock_spin(&tc->tc_lock);

	/*
	 * Update the priority of the new owner if needed.
	 */
	mtx_lock_spin(&sched_lock);
	if (td->td_priority < owner->td_priority)
		owner->td_priority = td->td_priority; 
	mtx_unlock_spin(&sched_lock);
}

/*
 * Block the current thread on the turnstile ts.  This function will context
 * switch and not return until this thread has been woken back up.  This
 * function must be called with the appropriate turnstile chain locked and
 * will return with it unlocked.
 */
void
turnstile_wait(struct turnstile *ts, struct lock_object *lock,
    struct thread *owner)
{
	struct turnstile_chain *tc;
	struct thread *td, *td1;

	td = curthread;
	tc = TC_LOOKUP(lock);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	MPASS(td->td_turnstile != NULL);
	MPASS(owner != NULL);
	MPASS(owner->td_proc->p_magic == P_MAGIC);

	/* If the passed in turnstile is NULL, use this thread's turnstile. */
	if (ts == NULL) {
		ts = td->td_turnstile;
		LIST_INSERT_HEAD(&tc->tc_turnstiles, ts, ts_hash);
		KASSERT(TAILQ_EMPTY(&ts->ts_pending),
		    ("thread's turnstile has pending threads"));
		KASSERT(TAILQ_EMPTY(&ts->ts_blocked),
		    ("thread's turnstile has a non-empty queue"));
		KASSERT(LIST_EMPTY(&ts->ts_free),
		    ("thread's turnstile has a non-empty free list"));
		KASSERT(ts->ts_lockobj == NULL, ("stale ts_lockobj pointer"));
		ts->ts_lockobj = lock;
		mtx_lock_spin(&td_contested_lock);
		TAILQ_INSERT_TAIL(&ts->ts_blocked, td, td_lockq);
		turnstile_setowner(ts, owner);
		mtx_unlock_spin(&td_contested_lock);
	} else {
		TAILQ_FOREACH(td1, &ts->ts_blocked, td_lockq)
			if (td1->td_priority > td->td_priority)
				break;
		mtx_lock_spin(&td_contested_lock);
		if (td1 != NULL)
			TAILQ_INSERT_BEFORE(td1, td, td_lockq);
		else
			TAILQ_INSERT_TAIL(&ts->ts_blocked, td, td_lockq);
		mtx_unlock_spin(&td_contested_lock);
		MPASS(td->td_turnstile != NULL);
		LIST_INSERT_HEAD(&ts->ts_free, td->td_turnstile, ts_hash);
		MPASS(owner == ts->ts_owner);
	}
	td->td_turnstile = NULL;
	mtx_unlock_spin(&tc->tc_lock);

	mtx_lock_spin(&sched_lock);
	/*
	 * Handle race condition where a thread on another CPU that owns
	 * lock 'lock' could have woken us in between us dropping the
	 * turnstile chain lock and acquiring the sched_lock.
	 */
	if (td->td_flags & TDF_TSNOBLOCK) {
		td->td_flags &= ~TDF_TSNOBLOCK;
		mtx_unlock_spin(&sched_lock);
		return;
	}
		
#ifdef notyet
	/*
	 * If we're borrowing an interrupted thread's VM context, we
	 * must clean up before going to sleep.
	 */
	if (td->td_ithd != NULL) {
		struct ithd *it = td->td_ithd;

		if (it->it_interrupted) {
			if (LOCK_LOG_TEST(lock, 0))
				CTR3(KTR_LOCK, "%s: %p interrupted %p",
				    __func__, it, it->it_interrupted);
			intr_thd_fixup(it);
		}
	}
#endif

	/* Save who we are blocked on and switch. */
	td->td_blocked = ts;
	td->td_lockname = lock->lo_name;
	TD_SET_LOCK(td);
	propagate_priority(td);

	if (LOCK_LOG_TEST(lock, 0))
		CTR4(KTR_LOCK, "%s: td %p blocked on [%p] %s", __func__, td,
		    lock, lock->lo_name);

	td->td_proc->p_stats->p_ru.ru_nvcsw++;
	mi_switch();

	if (LOCK_LOG_TEST(lock, 0))
		CTR4(KTR_LOCK, "%s: td %p free from blocked on [%p] %s",
		    __func__, td, lock, lock->lo_name);

	mtx_unlock_spin(&sched_lock);
}

/*
 * Pick the highest priority thread on this turnstile and put it on the
 * pending list.  This must be called with the turnstile chain locked.
 */
int
turnstile_signal(struct turnstile *ts)
{
	struct turnstile_chain *tc;
	struct thread *td;
	int empty;

	MPASS(ts != NULL);
	MPASS(curthread->td_proc->p_magic == P_MAGIC);
	MPASS(ts->ts_owner == curthread);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);

	/*
	 * Pick the highest priority thread blocked on this lock and
	 * move it to the pending list.
	 */
	td = TAILQ_FIRST(&ts->ts_blocked);
	MPASS(td->td_proc->p_magic == P_MAGIC);
	mtx_lock_spin(&td_contested_lock);
	TAILQ_REMOVE(&ts->ts_blocked, td, td_lockq);
	mtx_unlock_spin(&td_contested_lock);
	TAILQ_INSERT_TAIL(&ts->ts_pending, td, td_lockq);

	/*
	 * If the turnstile is now empty, remove it from its chain and
	 * give it to the about-to-be-woken thread.  Otherwise take a
	 * turnstile from the free list and give it to the thread.
	 */
	empty = TAILQ_EMPTY(&ts->ts_blocked);
	if (empty)
		MPASS(LIST_EMPTY(&ts->ts_free));
	else
		ts = LIST_FIRST(&ts->ts_free);
	LIST_REMOVE(ts, ts_hash);
	td->td_turnstile = ts;

	return (empty);
}
	
/*
 * Put all blocked threads on the pending list.  This must be called with
 * the turnstile chain locked.
 */
void
turnstile_wakeup(struct turnstile *ts)
{
	struct turnstile_chain *tc;
	struct turnstile *ts1;
	struct thread *td;

	MPASS(ts != NULL);
	MPASS(curthread->td_proc->p_magic == P_MAGIC);
	MPASS(ts->ts_owner == curthread);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);

	/*
	 * Transfer the blocked list to the pending list.
	 */
	mtx_lock_spin(&td_contested_lock);
	TAILQ_CONCAT(&ts->ts_pending, &ts->ts_blocked, td_lockq);
	mtx_unlock_spin(&td_contested_lock);

	/*
	 * Give a turnstile to each thread.  The last thread gets
	 * this turnstile.
	 */
	TAILQ_FOREACH(td, &ts->ts_pending, td_lockq) {
		if (LIST_EMPTY(&ts->ts_free)) {
			MPASS(TAILQ_NEXT(td, td_lockq) == NULL);
			ts1 = ts;
		} else
			ts1 = LIST_FIRST(&ts->ts_free);
		LIST_REMOVE(ts1, ts_hash);
		td->td_turnstile = ts1;
	}
}

/*
 * Wakeup all threads on the pending list and adjust the priority of the
 * current thread appropriately.  This must be called with the turnstile
 * chain locked.
 */
void
turnstile_unpend(struct turnstile *ts)
{
	TAILQ_HEAD( ,thread) pending_threads;
	struct turnstile_chain *tc;
	struct thread *td;
	int cp, pri;

	MPASS(ts != NULL);
	MPASS(ts->ts_owner == curthread);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	MPASS(!TAILQ_EMPTY(&ts->ts_pending));

	/*
	 * Move the list of pending threads out of the turnstile and
	 * into a local variable.
	 */
	TAILQ_INIT(&pending_threads);
	TAILQ_CONCAT(&pending_threads, &ts->ts_pending, td_lockq);
#ifdef INVARIANTS
	if (TAILQ_EMPTY(&ts->ts_blocked))
		ts->ts_lockobj = NULL;
#endif

	/*
	 * Remove the turnstile from this thread's list of contested locks
	 * since this thread doesn't own it anymore.  New threads will
	 * not be blocking on the turnstile until it is claimed by a new
	 * owner.
	 */
	mtx_lock_spin(&td_contested_lock);
	ts->ts_owner = NULL;
	LIST_REMOVE(ts, ts_link);
	mtx_unlock_spin(&td_contested_lock);
	mtx_unlock_spin(&tc->tc_lock);

	/*
	 * Adjust the priority of curthread based on other contested
	 * locks it owns.  Don't lower the priority below the base
	 * priority however.
	 */
	td = curthread;
	pri = PRI_MAX;
	mtx_lock_spin(&sched_lock);
	mtx_lock_spin(&td_contested_lock);
	LIST_FOREACH(ts, &td->td_contested, ts_link) {
		cp = TAILQ_FIRST(&ts->ts_blocked)->td_priority;
		if (cp < pri)
			pri = cp;
	}
	mtx_unlock_spin(&td_contested_lock);
	if (pri > td->td_base_pri)
		pri = td->td_base_pri;
	td->td_priority = pri;

	/*
	 * Wake up all the pending threads.  If a thread is not blocked
	 * on a lock, then it is currently executing on another CPU in
	 * turnstile_wait().  Set a flag to force it to try to acquire
	 * the lock again instead of blocking.
	 */
	while (!TAILQ_EMPTY(&pending_threads)) {
		td = TAILQ_FIRST(&pending_threads);
		TAILQ_REMOVE(&pending_threads, td, td_lockq);
		MPASS(td->td_proc->p_magic == P_MAGIC);
		if (TD_ON_LOCK(td)) {
			td->td_blocked = NULL;
			td->td_lockname = NULL;
			TD_CLR_LOCK(td);
			MPASS(TD_CAN_RUN(td));
			setrunqueue(td);
		} else {
			td->td_flags |= TDF_TSNOBLOCK;
			MPASS(TD_IS_RUNNING(td));
		}
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Return the first thread in a turnstile.
 */
struct thread *
turnstile_head(struct turnstile *ts)
{
#ifdef INVARIANTS
	struct turnstile_chain *tc;

	MPASS(ts != NULL);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
#endif
	return (TAILQ_FIRST(&ts->ts_blocked));
}

/*
 * Returns true if a turnstile is empty.
 */
int
turnstile_empty(struct turnstile *ts)
{
#ifdef INVARIANTS
	struct turnstile_chain *tc;

	MPASS(ts != NULL);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
#endif
	return (TAILQ_EMPTY(&ts->ts_blocked));
}
