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
 * Each time a thread is created, a turnstile is allocated from a UMA zone
 * and attached to that thread.  When a thread blocks on a lock, if it is the
 * first thread to block, it lends its turnstile to the lock.  If the lock
 * already has a turnstile, then it gives its turnstile to the lock's
 * turnstile's free list.  When a thread is woken up, it takes a turnstile from
 * the free list if there are any other waiters.  If it is the only thread
 * blocked on the lock, then it reclaims the turnstile associated with the lock
 * and removes it from the hash table.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_turnstile_profiling.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/turnstile.h>

#include <vm/uma.h>

#ifdef DDB
#include <sys/kdb.h>
#include <ddb/ddb.h>
#include <sys/lockmgr.h>
#include <sys/sx.h>
#endif

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
 * first of these two is the turnstile chain list that a turnstile is on
 * when it is attached to a lock.  The second list to use ts_hash is the
 * free list hung off of a turnstile that is attached to a lock.
 *
 * Each turnstile contains two lists of threads.  The ts_blocked list is
 * a linked list of threads blocked on the turnstile's lock.  The
 * ts_pending list is a linked list of threads previously awakened by
 * turnstile_signal() or turnstile_wait() that are waiting to be put on
 * the run queue.
 *
 * Locking key:
 *  c - turnstile chain lock
 *  q - td_contested lock
 */
struct turnstile {
	/* struct mtx ts_lock;	*/		/* Spin lock for self. */
	TAILQ_HEAD(, thread) ts_blocked[2];	/* (c + q) Blocked threads. */
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
#ifdef TURNSTILE_PROFILING
	u_int	tc_depth;			/* Length of tc_queues. */
	u_int	tc_max_depth;			/* Max length of tc_queues. */
#endif
};

#ifdef TURNSTILE_PROFILING
u_int turnstile_max_depth;
SYSCTL_NODE(_debug, OID_AUTO, turnstile, CTLFLAG_RD, 0, "turnstile profiling");
SYSCTL_NODE(_debug_turnstile, OID_AUTO, chains, CTLFLAG_RD, 0,
    "turnstile chain stats");
SYSCTL_UINT(_debug_turnstile, OID_AUTO, max_depth, CTLFLAG_RD,
    &turnstile_max_depth, 0, "maxmimum depth achieved of a single chain");
#endif
static struct mtx td_contested_lock;
static struct turnstile_chain turnstile_chains[TC_TABLESIZE];
static uma_zone_t turnstile_zone;

/*
 * Prototypes for non-exported routines.
 */
static void	init_turnstile0(void *dummy);
#ifdef TURNSTILE_PROFILING
static void	init_turnstile_profiling(void *arg);
#endif
static void	propagate_priority(struct thread *td);
static int	turnstile_adjust_thread(struct turnstile *ts,
		    struct thread *td);
static void	turnstile_setowner(struct turnstile *ts, struct thread *owner);
#ifdef INVARIANTS
static void	turnstile_dtor(void *mem, int size, void *arg);
#endif
static int	turnstile_init(void *mem, int size, int flags);

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
		 * If the thread is asleep, then we are probably about
		 * to deadlock.  To make debugging this easier, just
		 * panic and tell the user which thread misbehaved so
		 * they can hopefully get a stack trace from the truly
		 * misbehaving thread.
		 */
		if (TD_IS_SLEEPING(td)) {
			printf(
		"Sleeping thread (tid %d, pid %d) owns a non-sleepable lock\n",
			    td->td_tid, td->td_proc->p_pid);
#ifdef DDB
			db_trace_thread(td, -1);
#endif
			panic("sleeping thread");
		}

		/*
		 * If this thread already has higher priority than the
		 * thread that is being blocked, we are finished.
		 */
		if (td->td_priority <= pri)
			return;

		/*
		 * Bump this thread's priority.
		 */
		sched_lend_prio(td, pri);

		/*
		 * If lock holder is actually running or on the run queue
		 * then we are done.
		 */
		if (TD_IS_RUNNING(td) || TD_ON_RUNQ(td)) {
			MPASS(td->td_blocked == NULL);
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
		 * If we aren't blocked on a lock, we should be.
		 */
		KASSERT(TD_ON_LOCK(td), (
		    "thread %d(%s):%d holds %s but isn't blocked on a lock\n",
		    td->td_tid, td->td_proc->p_comm, td->td_state,
		    ts->ts_lockobj->lo_name));

		/*
		 * Pick up the lock that td is blocked on.
		 */
		ts = td->td_blocked;
		MPASS(ts != NULL);
		tc = TC_LOOKUP(ts->ts_lockobj);
		mtx_lock_spin(&tc->tc_lock);

		/* Resort td on the list if needed. */
		if (!turnstile_adjust_thread(ts, td)) {
			mtx_unlock_spin(&tc->tc_lock);
			return;
		}
		mtx_unlock_spin(&tc->tc_lock);
	}
}

/*
 * Adjust the thread's position on a turnstile after its priority has been
 * changed.
 */
static int
turnstile_adjust_thread(struct turnstile *ts, struct thread *td)
{
	struct turnstile_chain *tc;
	struct thread *td1, *td2;
	int queue;

	mtx_assert(&sched_lock, MA_OWNED);
	MPASS(TD_ON_LOCK(td));

	/*
	 * This thread may not be blocked on this turnstile anymore
	 * but instead might already be woken up on another CPU
	 * that is waiting on sched_lock in turnstile_unpend() to
	 * finish waking this thread up.  We can detect this case
	 * by checking to see if this thread has been given a
	 * turnstile by either turnstile_signal() or
	 * turnstile_broadcast().  In this case, treat the thread as
	 * if it was already running.
	 */
	if (td->td_turnstile != NULL)
		return (0);

	/*
	 * Check if the thread needs to be moved on the blocked chain.
	 * It needs to be moved if either its priority is lower than
	 * the previous thread or higher than the next thread.
	 */
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	td1 = TAILQ_PREV(td, threadqueue, td_lockq);
	td2 = TAILQ_NEXT(td, td_lockq);
	if ((td1 != NULL && td->td_priority < td1->td_priority) ||
	    (td2 != NULL && td->td_priority > td2->td_priority)) {

		/*
		 * Remove thread from blocked chain and determine where
		 * it should be moved to.
		 */
		queue = td->td_tsqueue;
		MPASS(queue == TS_EXCLUSIVE_QUEUE || queue == TS_SHARED_QUEUE);
		mtx_lock_spin(&td_contested_lock);
		TAILQ_REMOVE(&ts->ts_blocked[queue], td, td_lockq);
		TAILQ_FOREACH(td1, &ts->ts_blocked[queue], td_lockq) {
			MPASS(td1->td_proc->p_magic == P_MAGIC);
			if (td1->td_priority > td->td_priority)
				break;
		}

		if (td1 == NULL)
			TAILQ_INSERT_TAIL(&ts->ts_blocked[queue], td, td_lockq);
		else
			TAILQ_INSERT_BEFORE(td1, td, td_lockq);
		mtx_unlock_spin(&td_contested_lock);
		if (td1 == NULL)
			CTR3(KTR_LOCK,
		    "turnstile_adjust_thread: td %d put at tail on [%p] %s",
			    td->td_tid, ts->ts_lockobj, ts->ts_lockobj->lo_name);
		else
			CTR4(KTR_LOCK,
		    "turnstile_adjust_thread: td %d moved before %d on [%p] %s",
			    td->td_tid, td1->td_tid, ts->ts_lockobj,
			    ts->ts_lockobj->lo_name);
	}
	return (1);
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
	LIST_INIT(&thread0.td_contested);
	thread0.td_turnstile = NULL;
}

#ifdef TURNSTILE_PROFILING
static void
init_turnstile_profiling(void *arg)
{
	struct sysctl_oid *chain_oid;
	char chain_name[10];
	int i;

	for (i = 0; i < TC_TABLESIZE; i++) {
		snprintf(chain_name, sizeof(chain_name), "%d", i);
		chain_oid = SYSCTL_ADD_NODE(NULL, 
		    SYSCTL_STATIC_CHILDREN(_debug_turnstile_chains), OID_AUTO,
		    chain_name, CTLFLAG_RD, NULL, "turnstile chain stats");
		SYSCTL_ADD_UINT(NULL, SYSCTL_CHILDREN(chain_oid), OID_AUTO,
		    "depth", CTLFLAG_RD, &turnstile_chains[i].tc_depth, 0,
		    NULL);
		SYSCTL_ADD_UINT(NULL, SYSCTL_CHILDREN(chain_oid), OID_AUTO,
		    "max_depth", CTLFLAG_RD, &turnstile_chains[i].tc_max_depth,
		    0, NULL);
	}
}
SYSINIT(turnstile_profiling, SI_SUB_LOCK, SI_ORDER_ANY,
    init_turnstile_profiling, NULL);
#endif

static void
init_turnstile0(void *dummy)
{

	turnstile_zone = uma_zcreate("TURNSTILE", sizeof(struct turnstile),
#ifdef INVARIANTS
	    NULL, turnstile_dtor, turnstile_init, NULL, UMA_ALIGN_CACHE, 0);
#else
	    NULL, NULL, turnstile_init, NULL, UMA_ALIGN_CACHE, 0);
#endif
	thread0.td_turnstile = turnstile_alloc();
}
SYSINIT(turnstile0, SI_SUB_LOCK, SI_ORDER_ANY, init_turnstile0, NULL);

/*
 * Update a thread on the turnstile list after it's priority has been changed.
 * The old priority is passed in as an argument.
 */
void
turnstile_adjust(struct thread *td, u_char oldpri)
{
	struct turnstile_chain *tc;
	struct turnstile *ts;

	mtx_assert(&sched_lock, MA_OWNED);
	MPASS(TD_ON_LOCK(td));

	/*
	 * Pick up the lock that td is blocked on.
	 */
	ts = td->td_blocked;
	MPASS(ts != NULL);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_lock_spin(&tc->tc_lock);

	/* Resort the turnstile on the list. */
	if (!turnstile_adjust_thread(ts, td)) {
		mtx_unlock_spin(&tc->tc_lock);
		return;
	}

	/*
	 * If our priority was lowered and we are at the head of the
	 * turnstile, then propagate our new priority up the chain.
	 * Note that we currently don't try to revoke lent priorities
	 * when our priority goes up.
	 */
	MPASS(td->td_tsqueue == TS_EXCLUSIVE_QUEUE ||
	    td->td_tsqueue == TS_SHARED_QUEUE);
	if (td == TAILQ_FIRST(&ts->ts_blocked[td->td_tsqueue]) &&
	    td->td_priority < oldpri) {
		mtx_unlock_spin(&tc->tc_lock);
		critical_enter();
		propagate_priority(td);
		critical_exit();
	} else
		mtx_unlock_spin(&tc->tc_lock);
}

/*
 * Set the owner of the lock this turnstile is attached to.
 */
static void
turnstile_setowner(struct turnstile *ts, struct thread *owner)
{

	mtx_assert(&td_contested_lock, MA_OWNED);
	MPASS(ts->ts_owner == NULL);
	if (owner == NULL)
		return;

	MPASS(owner->td_proc->p_magic == P_MAGIC);
	ts->ts_owner = owner;
	LIST_INSERT_HEAD(&owner->td_contested, ts, ts_link);
}

#ifdef INVARIANTS
/*
 * UMA zone item deallocator.
 */
static void
turnstile_dtor(void *mem, int size, void *arg)
{
	struct turnstile *ts;

	ts = mem;
	MPASS(TAILQ_EMPTY(&ts->ts_blocked[TS_EXCLUSIVE_QUEUE]));
	MPASS(TAILQ_EMPTY(&ts->ts_blocked[TS_SHARED_QUEUE]));
	MPASS(TAILQ_EMPTY(&ts->ts_pending));
}
#endif

/*
 * UMA zone item initializer.
 */
static int
turnstile_init(void *mem, int size, int flags)
{
	struct turnstile *ts;

	bzero(mem, size);
	ts = mem;
	TAILQ_INIT(&ts->ts_blocked[TS_EXCLUSIVE_QUEUE]);
	TAILQ_INIT(&ts->ts_blocked[TS_SHARED_QUEUE]);
	TAILQ_INIT(&ts->ts_pending);
	LIST_INIT(&ts->ts_free);
	return (0);
}

/*
 * Get a turnstile for a new thread.
 */
struct turnstile *
turnstile_alloc(void)
{

	return (uma_zalloc(turnstile_zone, M_WAITOK));
}

/*
 * Free a turnstile when a thread is destroyed.
 */
void
turnstile_free(struct turnstile *ts)
{

	uma_zfree(turnstile_zone, ts);
}

/*
 * Lock the turnstile chain associated with the specified lock.
 */
void
turnstile_lock(struct lock_object *lock)
{
	struct turnstile_chain *tc;

	tc = TC_LOOKUP(lock);
	mtx_lock_spin(&tc->tc_lock);
}

/*
 * Look up the turnstile for a lock in the hash table locking the associated
 * turnstile chain along the way.  If no turnstile is found in the hash
 * table, NULL is returned.
 */
struct turnstile *
turnstile_lookup(struct lock_object *lock)
{
	struct turnstile_chain *tc;
	struct turnstile *ts;

	tc = TC_LOOKUP(lock);
	mtx_assert(&tc->tc_lock, MA_OWNED);
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
 * Return a pointer to the thread waiting on this turnstile with the
 * most important priority or NULL if the turnstile has no waiters.
 */
static struct thread *
turnstile_first_waiter(struct turnstile *ts)
{
	struct thread *std, *xtd;

	std = TAILQ_FIRST(&ts->ts_blocked[TS_SHARED_QUEUE]);
	xtd = TAILQ_FIRST(&ts->ts_blocked[TS_EXCLUSIVE_QUEUE]);
	if (xtd == NULL || (std != NULL && std->td_priority < xtd->td_priority))
		return (std);
	return (xtd);
}

/*
 * Take ownership of a turnstile and adjust the priority of the new
 * owner appropriately.
 */
void
turnstile_claim(struct lock_object *lock)
{
	struct turnstile_chain *tc;
	struct turnstile *ts;
	struct thread *td, *owner;

	tc = TC_LOOKUP(lock);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	ts = turnstile_lookup(lock);
	MPASS(ts != NULL);

	owner = curthread;
	mtx_lock_spin(&td_contested_lock);
	turnstile_setowner(ts, owner);
	mtx_unlock_spin(&td_contested_lock);

	td = turnstile_first_waiter(ts);
	MPASS(td != NULL);
	MPASS(td->td_proc->p_magic == P_MAGIC);
	mtx_unlock_spin(&tc->tc_lock);

	/*
	 * Update the priority of the new owner if needed.
	 */
	mtx_lock_spin(&sched_lock);
	if (td->td_priority < owner->td_priority)
		sched_lend_prio(owner, td->td_priority);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Block the current thread on the turnstile assicated with 'lock'.  This
 * function will context switch and not return until this thread has been
 * woken back up.  This function must be called with the appropriate
 * turnstile chain locked and will return with it unlocked.
 */
void
turnstile_wait_queue(struct lock_object *lock, struct thread *owner, int queue)
{
	struct turnstile_chain *tc;
	struct turnstile *ts;
	struct thread *td, *td1;

	td = curthread;
	tc = TC_LOOKUP(lock);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	MPASS(td->td_turnstile != NULL);
	if (queue == TS_SHARED_QUEUE)
		MPASS(owner != NULL);
	if (owner)
		MPASS(owner->td_proc->p_magic == P_MAGIC);
	MPASS(queue == TS_SHARED_QUEUE || queue == TS_EXCLUSIVE_QUEUE);

	/* Look up the turnstile associated with the lock 'lock'. */
	ts = turnstile_lookup(lock);

	/*
	 * If the lock does not already have a turnstile, use this thread's
	 * turnstile.  Otherwise insert the current thread into the
	 * turnstile already in use by this lock.
	 */
	if (ts == NULL) {
#ifdef TURNSTILE_PROFILING
		tc->tc_depth++;
		if (tc->tc_depth > tc->tc_max_depth) {
			tc->tc_max_depth = tc->tc_depth;
			if (tc->tc_max_depth > turnstile_max_depth)
				turnstile_max_depth = tc->tc_max_depth;
		}
#endif
		ts = td->td_turnstile;
		LIST_INSERT_HEAD(&tc->tc_turnstiles, ts, ts_hash);
		KASSERT(TAILQ_EMPTY(&ts->ts_pending),
		    ("thread's turnstile has pending threads"));
		KASSERT(TAILQ_EMPTY(&ts->ts_blocked[TS_EXCLUSIVE_QUEUE]),
		    ("thread's turnstile has exclusive waiters"));
		KASSERT(TAILQ_EMPTY(&ts->ts_blocked[TS_SHARED_QUEUE]),
		    ("thread's turnstile has shared waiters"));
		KASSERT(LIST_EMPTY(&ts->ts_free),
		    ("thread's turnstile has a non-empty free list"));
		KASSERT(ts->ts_lockobj == NULL, ("stale ts_lockobj pointer"));
		ts->ts_lockobj = lock;
		mtx_lock_spin(&td_contested_lock);
		TAILQ_INSERT_TAIL(&ts->ts_blocked[queue], td, td_lockq);
		turnstile_setowner(ts, owner);
		mtx_unlock_spin(&td_contested_lock);
	} else {
		TAILQ_FOREACH(td1, &ts->ts_blocked[queue], td_lockq)
			if (td1->td_priority > td->td_priority)
				break;
		mtx_lock_spin(&td_contested_lock);
		if (td1 != NULL)
			TAILQ_INSERT_BEFORE(td1, td, td_lockq);
		else
			TAILQ_INSERT_TAIL(&ts->ts_blocked[queue], td, td_lockq);
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
	td->td_tsqueue = queue;
	td->td_blocked = ts;
	td->td_lockname = lock->lo_name;
	TD_SET_LOCK(td);
	critical_enter();
	propagate_priority(td);
	critical_exit();

	if (LOCK_LOG_TEST(lock, 0))
		CTR4(KTR_LOCK, "%s: td %d blocked on [%p] %s", __func__,
		    td->td_tid, lock, lock->lo_name);

	mi_switch(SW_VOL, NULL);

	if (LOCK_LOG_TEST(lock, 0))
		CTR4(KTR_LOCK, "%s: td %d free from blocked on [%p] %s",
		    __func__, td->td_tid, lock, lock->lo_name);

	mtx_unlock_spin(&sched_lock);
}

/*
 * Pick the highest priority thread on this turnstile and put it on the
 * pending list.  This must be called with the turnstile chain locked.
 */
int
turnstile_signal_queue(struct turnstile *ts, int queue)
{
	struct turnstile_chain *tc;
	struct thread *td;
	int empty;

	MPASS(ts != NULL);
	MPASS(curthread->td_proc->p_magic == P_MAGIC);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	MPASS(ts->ts_owner == curthread ||
	    (queue == TS_EXCLUSIVE_QUEUE && ts->ts_owner == NULL));
	MPASS(queue == TS_SHARED_QUEUE || queue == TS_EXCLUSIVE_QUEUE);

	/*
	 * Pick the highest priority thread blocked on this lock and
	 * move it to the pending list.
	 */
	td = TAILQ_FIRST(&ts->ts_blocked[queue]);
	MPASS(td->td_proc->p_magic == P_MAGIC);
	mtx_lock_spin(&td_contested_lock);
	TAILQ_REMOVE(&ts->ts_blocked[queue], td, td_lockq);
	mtx_unlock_spin(&td_contested_lock);
	TAILQ_INSERT_TAIL(&ts->ts_pending, td, td_lockq);

	/*
	 * If the turnstile is now empty, remove it from its chain and
	 * give it to the about-to-be-woken thread.  Otherwise take a
	 * turnstile from the free list and give it to the thread.
	 */
	empty = TAILQ_EMPTY(&ts->ts_blocked[TS_EXCLUSIVE_QUEUE]) &&
	    TAILQ_EMPTY(&ts->ts_blocked[TS_SHARED_QUEUE]);
	if (empty) {
		MPASS(LIST_EMPTY(&ts->ts_free));
#ifdef TURNSTILE_PROFILING
		tc->tc_depth--;
#endif
	} else
		ts = LIST_FIRST(&ts->ts_free);
	MPASS(ts != NULL);
	LIST_REMOVE(ts, ts_hash);
	td->td_turnstile = ts;

	return (empty);
}
	
/*
 * Put all blocked threads on the pending list.  This must be called with
 * the turnstile chain locked.
 */
void
turnstile_broadcast_queue(struct turnstile *ts, int queue)
{
	struct turnstile_chain *tc;
	struct turnstile *ts1;
	struct thread *td;

	MPASS(ts != NULL);
	MPASS(curthread->td_proc->p_magic == P_MAGIC);
	MPASS(ts->ts_owner == curthread ||
		    (queue == TS_EXCLUSIVE_QUEUE && ts->ts_owner == NULL));
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	MPASS(queue == TS_SHARED_QUEUE || queue == TS_EXCLUSIVE_QUEUE);

	/*
	 * Transfer the blocked list to the pending list.
	 */
	mtx_lock_spin(&td_contested_lock);
	TAILQ_CONCAT(&ts->ts_pending, &ts->ts_blocked[queue], td_lockq);
	mtx_unlock_spin(&td_contested_lock);

	/*
	 * Give a turnstile to each thread.  The last thread gets
	 * this turnstile.
	 */
	TAILQ_FOREACH(td, &ts->ts_pending, td_lockq) {
		if (LIST_EMPTY(&ts->ts_free)) {
			MPASS(TAILQ_NEXT(td, td_lockq) == NULL);
			ts1 = ts;
#ifdef TURNSTILE_PROFILING
			tc->tc_depth--;
#endif
		} else
			ts1 = LIST_FIRST(&ts->ts_free);
		MPASS(ts1 != NULL);
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
turnstile_unpend_queue(struct turnstile *ts, int owner_type)
{
	TAILQ_HEAD( ,thread) pending_threads;
	struct turnstile_chain *tc;
	struct turnstile *nts;
	struct thread *td;
	u_char cp, pri;

	MPASS(ts != NULL);
	MPASS(ts->ts_owner == curthread ||
	    (owner_type == TS_SHARED_LOCK && ts->ts_owner == NULL));
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
	if (TAILQ_EMPTY(&ts->ts_blocked[TS_EXCLUSIVE_QUEUE]) &&
	    TAILQ_EMPTY(&ts->ts_blocked[TS_SHARED_QUEUE]))
		ts->ts_lockobj = NULL;
#endif

	/*
	 * Remove the turnstile from this thread's list of contested locks
	 * since this thread doesn't own it anymore.  New threads will
	 * not be blocking on the turnstile until it is claimed by a new
	 * owner.  There might not be a current owner if this is a shared
	 * lock.
	 */
	if (ts->ts_owner != NULL) {
		mtx_lock_spin(&td_contested_lock);
		ts->ts_owner = NULL;
		LIST_REMOVE(ts, ts_link);
		mtx_unlock_spin(&td_contested_lock);
	}
	critical_enter();
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
	LIST_FOREACH(nts, &td->td_contested, ts_link) {
		cp = turnstile_first_waiter(nts)->td_priority;
		if (cp < pri)
			pri = cp;
	}
	mtx_unlock_spin(&td_contested_lock);
	sched_unlend_prio(td, pri);

	/*
	 * Wake up all the pending threads.  If a thread is not blocked
	 * on a lock, then it is currently executing on another CPU in
	 * turnstile_wait() or sitting on a run queue waiting to resume
	 * in turnstile_wait().  Set a flag to force it to try to acquire
	 * the lock again instead of blocking.
	 */
	while (!TAILQ_EMPTY(&pending_threads)) {
		td = TAILQ_FIRST(&pending_threads);
		TAILQ_REMOVE(&pending_threads, td, td_lockq);
		MPASS(td->td_proc->p_magic == P_MAGIC);
		if (TD_ON_LOCK(td)) {
			td->td_blocked = NULL;
			td->td_lockname = NULL;
#ifdef INVARIANTS
			td->td_tsqueue = 0xff;
#endif
			TD_CLR_LOCK(td);
			MPASS(TD_CAN_RUN(td));
			setrunqueue(td, SRQ_BORING);
		} else {
			td->td_flags |= TDF_TSNOBLOCK;
			MPASS(TD_IS_RUNNING(td) || TD_ON_RUNQ(td));
		}
	}
	critical_exit();
	mtx_unlock_spin(&sched_lock);
}

/*
 * Give up ownership of a turnstile.  This must be called with the
 * turnstile chain locked.
 */
void
turnstile_disown(struct turnstile *ts)
{
	struct turnstile_chain *tc;
	struct thread *td;
	u_char cp, pri;

	MPASS(ts != NULL);
	MPASS(ts->ts_owner == curthread);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	MPASS(TAILQ_EMPTY(&ts->ts_pending));
	MPASS(!TAILQ_EMPTY(&ts->ts_blocked[TS_EXCLUSIVE_QUEUE]) ||
	    !TAILQ_EMPTY(&ts->ts_blocked[TS_SHARED_QUEUE]));

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
		cp = turnstile_first_waiter(ts)->td_priority;
		if (cp < pri)
			pri = cp;
	}
	mtx_unlock_spin(&td_contested_lock);
	sched_unlend_prio(td, pri);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Return the first thread in a turnstile.
 */
struct thread *
turnstile_head_queue(struct turnstile *ts, int queue)
{
#ifdef INVARIANTS
	struct turnstile_chain *tc;

	MPASS(ts != NULL);
	tc = TC_LOOKUP(ts->ts_lockobj);
	MPASS(queue == TS_SHARED_QUEUE || queue == TS_EXCLUSIVE_QUEUE);
	mtx_assert(&tc->tc_lock, MA_OWNED);
#endif
	return (TAILQ_FIRST(&ts->ts_blocked[queue]));
}

#ifdef DDB
static int db_pager_quit;

static void
print_thread(struct thread *td, const char *prefix)
{

	db_printf("%s%p (tid %d, pid %d, \"%s\")\n", prefix, td, td->td_tid,
	    td->td_proc->p_pid, td->td_proc->p_comm);
}

static void
print_queue(struct threadqueue *queue, const char *header, const char *prefix)
{
	struct thread *td;

	db_printf("%s:\n", header);
	if (TAILQ_EMPTY(queue)) {
		db_printf("%sempty\n", prefix);
		return;
	}
	TAILQ_FOREACH(td, queue, td_lockq) {
		print_thread(td, prefix);
	}
}

DB_SHOW_COMMAND(turnstile, db_show_turnstile)
{
	struct turnstile_chain *tc;
	struct turnstile *ts;
	struct lock_object *lock;
	int i;

	if (!have_addr)
		return;

	/*
	 * First, see if there is an active turnstile for the lock indicated
	 * by the address.
	 */
	lock = (struct lock_object *)addr;
	tc = TC_LOOKUP(lock);
	LIST_FOREACH(ts, &tc->tc_turnstiles, ts_hash)
		if (ts->ts_lockobj == lock)
			goto found;

	/*
	 * Second, see if there is an active turnstile at the address
	 * indicated.
	 */
	for (i = 0; i < TC_TABLESIZE; i++)
		LIST_FOREACH(ts, &turnstile_chains[i].tc_turnstiles, ts_hash) {
			if (ts == (struct turnstile *)addr)
				goto found;
		}

	db_printf("Unable to locate a turnstile via %p\n", (void *)addr);
	return;
found:
	db_pager_quit = 0;
	db_setup_paging(db_simple_pager, &db_pager_quit, db_lines_per_page);
	lock = ts->ts_lockobj;
	db_printf("Lock: %p - (%s) %s\n", lock, LOCK_CLASS(lock)->lc_name,
	    lock->lo_name);
	if (ts->ts_owner)
		print_thread(ts->ts_owner, "Lock Owner: ");
	else
		db_printf("Lock Owner: none\n");
	print_queue((struct threadqueue *)&ts->ts_blocked[TS_SHARED_QUEUE], "Shared Waiters", "\t");
	print_queue((struct threadqueue *)&ts->ts_blocked[TS_EXCLUSIVE_QUEUE], "Exclusive Waiters", "\t");
	print_queue((struct threadqueue *)&ts->ts_pending, "Pending Threads",
	    "\t");	
}

/*
 * Show all the threads a particular thread is waiting on based on
 * non-sleepable and non-spin locks.
 */
static void
print_lockchain(struct thread *td, const char *prefix)
{
	struct lock_object *lock;
	struct lock_class *class;
	struct turnstile *ts;

	/*
	 * Follow the chain.  We keep walking as long as the thread is
	 * blocked on a turnstile that has an owner.
	 */
	while (!db_pager_quit) {
		db_printf("%sthread %d (pid %d, %s) ", prefix, td->td_tid,
		    td->td_proc->p_pid, td->td_proc->p_comm);
		switch (td->td_state) {
		case TDS_INACTIVE:
			db_printf("is inactive\n");
			return;
		case TDS_CAN_RUN:
			db_printf("can run\n");
			return;
		case TDS_RUNQ:
			db_printf("is on a run queue\n");
			return;
		case TDS_RUNNING:
			db_printf("running on CPU %d\n", td->td_oncpu);
			return;
		case TDS_INHIBITED:
			if (TD_ON_LOCK(td)) {
				ts = td->td_blocked;
				lock = ts->ts_lockobj;
				class = LOCK_CLASS(lock);
				db_printf("blocked on lock %p (%s) \"%s\"\n",
				    lock, class->lc_name, lock->lo_name);
				if (ts->ts_owner == NULL)
					return;
				td = ts->ts_owner;
				break;
			}
			db_printf("inhibited\n");
			return;
		default:
			db_printf("??? (%#x)\n", td->td_state);
			return;
		}
	}
}

DB_SHOW_COMMAND(lockchain, db_show_lockchain)
{
	struct thread *td;

	/* Figure out which thread to start with. */
	if (have_addr)
		td = db_lookup_thread(addr, TRUE);
	else
		td = kdb_thread;

	db_pager_quit = 0;
	db_setup_paging(db_simple_pager, &db_pager_quit, db_lines_per_page);
	print_lockchain(td, "");
}

DB_SHOW_COMMAND(allchains, db_show_allchains)
{
	struct thread *td;
	struct proc *p;
	int i;

	i = 1;
	db_pager_quit = 0;
	db_setup_paging(db_simple_pager, &db_pager_quit, db_lines_per_page);
	LIST_FOREACH(p, &allproc, p_list) {
		FOREACH_THREAD_IN_PROC(p, td) {
			if (TD_ON_LOCK(td) && LIST_EMPTY(&td->td_contested)) {
				db_printf("chain %d:\n", i++);
				print_lockchain(td, " ");
			}
			if (db_pager_quit)
				return;
		}
	}
}

/*
 * Show all the threads a particular thread is waiting on based on
 * sleepable locks.
 */
static void
print_sleepchain(struct thread *td, const char *prefix)
{
	struct thread *owner;

	/*
	 * Follow the chain.  We keep walking as long as the thread is
	 * blocked on a sleep lock that has an owner.
	 */
	while (!db_pager_quit) {
		db_printf("%sthread %d (pid %d, %s) ", prefix, td->td_tid,
		    td->td_proc->p_pid, td->td_proc->p_comm);
		switch (td->td_state) {
		case TDS_INACTIVE:
			db_printf("is inactive\n");
			return;
		case TDS_CAN_RUN:
			db_printf("can run\n");
			return;
		case TDS_RUNQ:
			db_printf("is on a run queue\n");
			return;
		case TDS_RUNNING:
			db_printf("running on CPU %d\n", td->td_oncpu);
			return;
		case TDS_INHIBITED:
			if (TD_ON_SLEEPQ(td)) {
				if (lockmgr_chain(td, &owner) ||
				    sx_chain(td, &owner)) {
					if (owner == NULL)
						return;
					td = owner;
					break;
				}
				db_printf("sleeping on %p \"%s\"\n",
				    td->td_wchan, td->td_wmesg);
				return;
			}
			db_printf("inhibited\n");
			return;
		default:
			db_printf("??? (%#x)\n", td->td_state);
			return;
		}
	}
}

DB_SHOW_COMMAND(sleepchain, db_show_sleepchain)
{
	struct thread *td;

	/* Figure out which thread to start with. */
	if (have_addr)
		td = db_lookup_thread(addr, TRUE);
	else
		td = kdb_thread;

	db_pager_quit = 0;
	db_setup_paging(db_simple_pager, &db_pager_quit, db_lines_per_page);
	print_sleepchain(td, "");
}

static void	print_waiters(struct turnstile *ts, int indent);
	
static void
print_waiter(struct thread *td, int indent)
{
	struct turnstile *ts;
	int i;

	if (db_pager_quit)
		return;
	for (i = 0; i < indent; i++)
		db_printf(" ");
	print_thread(td, "thread ");
	LIST_FOREACH(ts, &td->td_contested, ts_link)
		print_waiters(ts, indent + 1);
}

static void
print_waiters(struct turnstile *ts, int indent)
{
	struct lock_object *lock;
	struct lock_class *class;
	struct thread *td;
	int i;

	if (db_pager_quit)
		return;
	lock = ts->ts_lockobj;
	class = LOCK_CLASS(lock);
	for (i = 0; i < indent; i++)
		db_printf(" ");
	db_printf("lock %p (%s) \"%s\"\n", lock, class->lc_name, lock->lo_name);

	TAILQ_FOREACH(td, &ts->ts_blocked[TS_EXCLUSIVE_QUEUE], td_lockq)
		print_waiter(td, indent + 1);
	TAILQ_FOREACH(td, &ts->ts_blocked[TS_SHARED_QUEUE], td_lockq)
		print_waiter(td, indent + 1);
	TAILQ_FOREACH(td, &ts->ts_pending, td_lockq)
		print_waiter(td, indent + 1);
}

DB_SHOW_COMMAND(locktree, db_show_locktree)
{
	struct lock_object *lock;
	struct lock_class *class;
	struct turnstile_chain *tc;
	struct turnstile *ts;

	if (!have_addr)
		return;
	db_pager_quit = 0;
	db_setup_paging(db_simple_pager, &db_pager_quit, db_lines_per_page);
	lock = (struct lock_object *)addr;
	tc = TC_LOOKUP(lock);
	LIST_FOREACH(ts, &tc->tc_turnstiles, ts_hash)
		if (ts->ts_lockobj == lock)
			break;
	if (ts == NULL) {
		class = LOCK_CLASS(lock);
		db_printf("lock %p (%s) \"%s\"\n", lock, class->lc_name,
		    lock->lo_name);
	} else
		print_waiters(ts, 0);
}
#endif

/*
 * Returns true if a turnstile is empty.
 */
int
turnstile_empty_queue(struct turnstile *ts, int queue)
{
#ifdef INVARIANTS
	struct turnstile_chain *tc;

	MPASS(ts != NULL);
	tc = TC_LOOKUP(ts->ts_lockobj);
	mtx_assert(&tc->tc_lock, MA_OWNED);
	MPASS(queue == TS_SHARED_QUEUE || queue == TS_EXCLUSIVE_QUEUE);
#endif
	return (TAILQ_EMPTY(&ts->ts_blocked[queue]));
}
