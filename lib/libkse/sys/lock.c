/*-
 * Copyright (c) 2001, 2003 Daniel Eischen <deischen@freebsd.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
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

#include <sys/types.h>
#include <machine/atomic.h>
#include <assert.h>
#include <stdlib.h>

#include "atomic_ops.h"
#include "lock.h"

#ifdef _LOCK_DEBUG
#define	LCK_ASSERT(e)	assert(e)
#else
#define LCK_ASSERT(e)
#endif

#define	MAX_SPINS	500

void
_lock_destroy(struct lock *lck)
{
	if ((lck != NULL) && (lck->l_head != NULL)) {
		free(lck->l_head);
		lck->l_head = NULL;
		lck->l_tail = NULL;
	}
}

int
_lock_init(struct lock *lck, enum lock_type ltype,
    lock_handler_t *waitfunc, lock_handler_t *wakeupfunc)
{
	if (lck == NULL)
		return (-1);
	else if ((lck->l_head = malloc(sizeof(struct lockreq))) == NULL)
		return (-1);
	else {
		lck->l_type = ltype;
		lck->l_wait = waitfunc;
		lck->l_wakeup = wakeupfunc;
		lck->l_head->lr_locked = 0;
		lck->l_head->lr_watcher = NULL;
		lck->l_head->lr_owner = NULL;
		lck->l_head->lr_active = 1;
		lck->l_tail = lck->l_head;
	}
	return (0);
}

int
_lock_reinit(struct lock *lck, enum lock_type ltype,
    lock_handler_t *waitfunc, lock_handler_t *wakeupfunc)
{
	if (lck == NULL)
		return (-1);
	else if (lck->l_head == NULL)
		return (_lock_init(lck, ltype, waitfunc, wakeupfunc));
	else {
		lck->l_head->lr_locked = 0;
		lck->l_head->lr_watcher = NULL;
		lck->l_head->lr_owner = NULL;
		lck->l_head->lr_active = 1;
		lck->l_tail = lck->l_head;
	}
	return (0);
}

int
_lockuser_init(struct lockuser *lu, void *priv)
{
	if (lu == NULL)
		return (-1);
	else if ((lu->lu_myreq == NULL) &&
	    ((lu->lu_myreq = malloc(sizeof(struct lockreq))) == NULL))
		return (-1);
	else {
		lu->lu_myreq->lr_locked = 1;
		lu->lu_myreq->lr_watcher = NULL;
		lu->lu_myreq->lr_owner = lu;
		lu->lu_myreq->lr_active = 0;
		lu->lu_watchreq = NULL;
		lu->lu_priority = 0;
		lu->lu_private = priv;
		lu->lu_private2 = NULL;
	}
	return (0);
}

int
_lockuser_reinit(struct lockuser *lu, void *priv)
{
	if (lu == NULL)
		return (-1);

	if (lu->lu_watchreq != NULL) {
		/*
		 * In this case the lock is active.  All lockusers
		 * keep their watch request and drop their own
		 * (lu_myreq) request.  Their own request is either
		 * some other lockuser's watch request or is the
		 * head of the lock.
		 */
		lu->lu_myreq = lu->lu_watchreq;
		lu->lu_watchreq = NULL;
       }
       if (lu->lu_myreq == NULL)
		/*
		 * Oops, something isn't quite right.  Try to
		 * allocate one.
		 */
		return (_lockuser_init(lu, priv));
	else {
		lu->lu_myreq->lr_locked = 1;
		lu->lu_myreq->lr_watcher = NULL;
		lu->lu_myreq->lr_owner = lu;
		lu->lu_myreq->lr_active = 0;
		lu->lu_watchreq = NULL;
		lu->lu_priority = 0;
		lu->lu_private = priv;
		lu->lu_private2 = NULL;
	}
	return (0);
}

void
_lockuser_destroy(struct lockuser *lu)
{
	if ((lu != NULL) && (lu->lu_myreq != NULL))
		free(lu->lu_myreq);
}

/*
 * Acquire a lock waiting (spin or sleep) for it to become available.
 */
void
_lock_acquire(struct lock *lck, struct lockuser *lu, int prio)
{
	int i;
	int lval;

	/**
	 * XXX - We probably want to remove these checks to optimize
	 *       performance.  It is also a bug if any one of the 
	 *       checks fail, so it's probably better to just let it
	 *       SEGV and fix it.
	 */
#if 0
	if (lck == NULL || lu == NULL || lck->l_head == NULL)
		return;
#endif
	if ((lck->l_type & LCK_PRIORITY) != 0) {
		LCK_ASSERT(lu->lu_myreq->lr_locked == 1);
		LCK_ASSERT(lu->lu_myreq->lr_watcher == NULL);
		LCK_ASSERT(lu->lu_myreq->lr_owner == lu);
		LCK_ASSERT(lu->lu_watchreq == NULL);

		lu->lu_priority = prio;
	}
	/*
	 * Atomically swap the head of the lock request with
	 * this request.
	 */
	atomic_swap_ptr(&lck->l_head, lu->lu_myreq, &lu->lu_watchreq);

	if (lu->lu_watchreq->lr_locked != 0) {
		atomic_store_rel_ptr
		    ((volatile uintptr_t *)&lu->lu_watchreq->lr_watcher,
		    (uintptr_t)lu);
		if ((lck->l_wait == NULL) ||
		    ((lck->l_type & LCK_ADAPTIVE) == 0)) {
			while (lu->lu_watchreq->lr_locked != 0)
				;	/* spin, then yield? */
		} else {
			/*
			 * Spin for a bit before invoking the wait function.
			 *
			 * We should be a little smarter here.  If we're
			 * running on a single processor, then the lock
			 * owner got preempted and spinning will accomplish
			 * nothing but waste time.  If we're running on
			 * multiple processors, the owner could be running
			 * on another CPU and we might acquire the lock if
			 * we spin for a bit.
			 *
			 * The other thing to keep in mind is that threads
			 * acquiring these locks are considered to be in
			 * critical regions; they will not be preempted by
			 * the _UTS_ until they release the lock.  It is
			 * therefore safe to assume that if a lock can't
			 * be acquired, it is currently held by a thread
			 * running in another KSE.
			 */
			for (i = 0; i < MAX_SPINS; i++) {
				if (lu->lu_watchreq->lr_locked == 0)
					return;
				if (lu->lu_watchreq->lr_active == 0)
					break;
			}
			atomic_swap_int((int *)&lu->lu_watchreq->lr_locked,
			    2, &lval);
			if (lval == 0)
				lu->lu_watchreq->lr_locked = 0;
			else
				lck->l_wait(lck, lu);

		}
	}
	lu->lu_myreq->lr_active = 1;
}

/*
 * Release a lock.
 */
void
_lock_release(struct lock *lck, struct lockuser *lu)
{
	struct lockuser *lu_tmp, *lu_h;
	struct lockreq *myreq;
	int prio_h;
	int lval;

	/**
	 * XXX - We probably want to remove these checks to optimize
	 *       performance.  It is also a bug if any one of the 
	 *       checks fail, so it's probably better to just let it
	 *       SEGV and fix it.
	 */
#if 0
	if ((lck == NULL) || (lu == NULL))
		return;
#endif
	if ((lck->l_type & LCK_PRIORITY) != 0) {
		prio_h = 0;
		lu_h = NULL;

		/* Update tail if our request is last. */
		if (lu->lu_watchreq->lr_owner == NULL) {
			atomic_store_rel_ptr((volatile uintptr_t *)&lck->l_tail,
			    (uintptr_t)lu->lu_myreq);
			atomic_store_rel_ptr
			    ((volatile uintptr_t *)&lu->lu_myreq->lr_owner,
			    (uintptr_t)NULL);
		} else {
			/* Remove ourselves from the list. */
			atomic_store_rel_ptr((volatile uintptr_t *)
			    &lu->lu_myreq->lr_owner,
			    (uintptr_t)lu->lu_watchreq->lr_owner);
			atomic_store_rel_ptr((volatile uintptr_t *)
			    &lu->lu_watchreq->lr_owner->lu_myreq,
			    (uintptr_t)lu->lu_myreq);
		}
		/*
		 * The watch request now becomes our own because we've
		 * traded away our previous request.  Save our previous
		 * request so that we can grant the lock.
		 */
		myreq = lu->lu_myreq;
		lu->lu_myreq = lu->lu_watchreq;
		lu->lu_watchreq = NULL;
		lu->lu_myreq->lr_locked = 1;
		lu->lu_myreq->lr_owner = lu;
		lu->lu_myreq->lr_watcher = NULL;
		/*
		 * Traverse the list of lock requests in reverse order
		 * looking for the user with the highest priority.
		 */
		for (lu_tmp = lck->l_tail->lr_watcher; lu_tmp != NULL;
		     lu_tmp = lu_tmp->lu_myreq->lr_watcher) {
			if (lu_tmp->lu_priority > prio_h) {
				lu_h = lu_tmp;
				prio_h = lu_tmp->lu_priority;
			}
		}
		if (lu_h != NULL) {
			/* Give the lock to the highest priority user. */
			if (lck->l_wakeup != NULL) {
				atomic_swap_int(
				    (int *)&lu_h->lu_watchreq->lr_locked,
				    0, &lval);
				if (lval == 2)
					/* Notify the sleeper */
					lck->l_wakeup(lck,
					    lu_h->lu_myreq->lr_watcher);
			}
			else
				atomic_store_rel_int(
				    &lu_h->lu_watchreq->lr_locked, 0);
		} else {
			if (lck->l_wakeup != NULL) {
				atomic_swap_int((int *)&myreq->lr_locked,
				    0, &lval);
				if (lval == 2)
					/* Notify the sleeper */
					lck->l_wakeup(lck, myreq->lr_watcher);
			}
			else
				/* Give the lock to the previous request. */
				atomic_store_rel_int(&myreq->lr_locked, 0);
		}
	} else {
		/*
		 * The watch request now becomes our own because we've
		 * traded away our previous request.  Save our previous
		 * request so that we can grant the lock.
		 */
		myreq = lu->lu_myreq;
		lu->lu_myreq = lu->lu_watchreq;
		lu->lu_watchreq = NULL;
		lu->lu_myreq->lr_locked = 1;
		if (lck->l_wakeup) {
			atomic_swap_int((int *)&myreq->lr_locked, 0, &lval);
			if (lval == 2)
				/* Notify the sleeper */
				lck->l_wakeup(lck, myreq->lr_watcher);
		}
		else
			/* Give the lock to the previous request. */
			atomic_store_rel_int(&myreq->lr_locked, 0);
	}
	lu->lu_myreq->lr_active = 0;
}

void
_lock_grant(struct lock *lck /* unused */, struct lockuser *lu)
{
	atomic_store_rel_int(&lu->lu_watchreq->lr_locked, 3);
}

void
_lockuser_setactive(struct lockuser *lu, int active)
{
	lu->lu_myreq->lr_active = active;
}

