/*
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Daniel Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
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
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>
#include <pthread.h>
#include "thr_private.h"

/* Prototypes: */
static void pq_insert_prio_list(pq_queue_t *pq, int prio);

#if defined(_PTHREADS_INVARIANTS)

static int _pq_active = 0;

#define _PQ_IN_SCHEDQ	(PTHREAD_FLAGS_IN_PRIOQ | PTHREAD_FLAGS_IN_WAITQ | PTHREAD_FLAGS_IN_WORKQ)

#define _PQ_SET_ACTIVE()		_pq_active = 1
#define _PQ_CLEAR_ACTIVE()		_pq_active = 0
#define _PQ_ASSERT_ACTIVE(msg)		do {		\
	if (_pq_active == 0)				\
		PANIC(msg);				\
} while (0)
#define _PQ_ASSERT_INACTIVE(msg)	do {		\
	if (_pq_active != 0)				\
		PANIC(msg);				\
} while (0)
#define _PQ_ASSERT_IN_WAITQ(thrd, msg)	do {		\
	if (((thrd)->flags & PTHREAD_FLAGS_IN_WAITQ) == 0) \
		PANIC(msg);				\
} while (0)
#define _PQ_ASSERT_IN_PRIOQ(thrd, msg)	do {		\
	if (((thrd)->flags & PTHREAD_FLAGS_IN_PRIOQ) == 0) \
		PANIC(msg);				\
} while (0)
#define _PQ_ASSERT_NOT_QUEUED(thrd, msg) do {		\
	if (((thrd)->flags & _PQ_IN_SCHEDQ) != 0)	\
		PANIC(msg);				\
} while (0)
#define _PQ_ASSERT_PROTECTED(msg)			\
	PTHREAD_ASSERT((_thread_kern_in_sched != 0) ||	\
	    ((_get_curthread())->sig_defer_count > 0) ||\
	    (_sig_in_handler != 0), msg);

#else

#define _PQ_SET_ACTIVE()
#define _PQ_CLEAR_ACTIVE()
#define _PQ_ASSERT_ACTIVE(msg)
#define _PQ_ASSERT_INACTIVE(msg)
#define _PQ_ASSERT_IN_WAITQ(thrd, msg)
#define _PQ_ASSERT_IN_PRIOQ(thrd, msg)
#define _PQ_ASSERT_NOT_QUEUED(thrd, msg)
#define _PQ_ASSERT_PROTECTED(msg)

#endif

int
_pq_alloc(pq_queue_t *pq, int minprio, int maxprio)
{
	int ret = 0;
	int prioslots = maxprio - minprio + 1;

	if (pq == NULL)
		ret = -1;

	/* Create the priority queue with (maxprio - minprio + 1) slots: */
	else if	((pq->pq_lists =
	    (pq_list_t *) malloc(sizeof(pq_list_t) * prioslots)) == NULL)
		ret = -1;

	else {
		/* Remember the queue size: */
		pq->pq_size = prioslots;
		ret = _pq_init(pq);
	}
	return (ret);
}

int
_pq_init(pq_queue_t *pq)
{
	int i, ret = 0;

	if ((pq == NULL) || (pq->pq_lists == NULL))
		ret = -1;

	else {
		/* Initialize the queue for each priority slot: */
		for (i = 0; i < pq->pq_size; i++) {
			TAILQ_INIT(&pq->pq_lists[i].pl_head);
			pq->pq_lists[i].pl_prio = i;
			pq->pq_lists[i].pl_queued = 0;
		}

		/* Initialize the priority queue: */
		TAILQ_INIT(&pq->pq_queue);
		_PQ_CLEAR_ACTIVE();
	}
	return (ret);
}

void
_pq_remove(pq_queue_t *pq, pthread_t pthread)
{
	int prio = pthread->active_priority;

	/*
	 * Make some assertions when debugging is enabled:
	 */
	_PQ_ASSERT_INACTIVE("_pq_remove: pq_active");
	_PQ_SET_ACTIVE();
	_PQ_ASSERT_IN_PRIOQ(pthread, "_pq_remove: Not in priority queue");
	_PQ_ASSERT_PROTECTED("_pq_remove: prioq not protected!");

	/*
	 * Remove this thread from priority list.  Note that if
	 * the priority list becomes empty, it is not removed
	 * from the priority queue because another thread may be
	 * added to the priority list (resulting in a needless
	 * removal/insertion).  Priority lists are only removed
	 * from the priority queue when _pq_first is called.
	 */
	TAILQ_REMOVE(&pq->pq_lists[prio].pl_head, pthread, pqe);

	/* This thread is now longer in the priority queue. */
	pthread->flags &= ~PTHREAD_FLAGS_IN_PRIOQ;

	_PQ_CLEAR_ACTIVE();
}


void
_pq_insert_head(pq_queue_t *pq, pthread_t pthread)
{
	int prio;

	/*
	 * Don't insert suspended threads into the priority queue.
	 * The caller is responsible for setting the threads state.
	 */
	if ((pthread->flags & PTHREAD_FLAGS_SUSPENDED) != 0) {
		/* Make sure the threads state is suspended. */
		if (pthread->state != PS_SUSPENDED)
			PTHREAD_SET_STATE(pthread, PS_SUSPENDED);
	} else {
		/*
		 * Make some assertions when debugging is enabled:
		 */
		_PQ_ASSERT_INACTIVE("_pq_insert_head: pq_active");
		_PQ_SET_ACTIVE();
		_PQ_ASSERT_NOT_QUEUED(pthread,
		    "_pq_insert_head: Already in priority queue");
		_PQ_ASSERT_PROTECTED("_pq_insert_head: prioq not protected!");

		prio = pthread->active_priority;
		TAILQ_INSERT_HEAD(&pq->pq_lists[prio].pl_head, pthread, pqe);
		if (pq->pq_lists[prio].pl_queued == 0)
			/* Insert the list into the priority queue: */
			pq_insert_prio_list(pq, prio);

		/* Mark this thread as being in the priority queue. */
		pthread->flags |= PTHREAD_FLAGS_IN_PRIOQ;

		_PQ_CLEAR_ACTIVE();
	}
}


void
_pq_insert_tail(pq_queue_t *pq, pthread_t pthread)
{
	int prio;

	/*
	 * Don't insert suspended threads into the priority queue.
	 * The caller is responsible for setting the threads state.
	 */
	if ((pthread->flags & PTHREAD_FLAGS_SUSPENDED) != 0) {
		/* Make sure the threads state is suspended. */
		if (pthread->state != PS_SUSPENDED)
			PTHREAD_SET_STATE(pthread, PS_SUSPENDED);
	} else {
		/*
		 * Make some assertions when debugging is enabled:
		 */
		_PQ_ASSERT_INACTIVE("_pq_insert_tail: pq_active");
		_PQ_SET_ACTIVE();
		_PQ_ASSERT_NOT_QUEUED(pthread,
		    "_pq_insert_tail: Already in priority queue");
		_PQ_ASSERT_PROTECTED("_pq_insert_tail: prioq not protected!");

		prio = pthread->active_priority;
		TAILQ_INSERT_TAIL(&pq->pq_lists[prio].pl_head, pthread, pqe);
		if (pq->pq_lists[prio].pl_queued == 0)
			/* Insert the list into the priority queue: */
			pq_insert_prio_list(pq, prio);

		/* Mark this thread as being in the priority queue. */
		pthread->flags |= PTHREAD_FLAGS_IN_PRIOQ;

		_PQ_CLEAR_ACTIVE();
	}
}


pthread_t
_pq_first(pq_queue_t *pq)
{
	pq_list_t *pql;
	pthread_t pthread = NULL;

	/*
	 * Make some assertions when debugging is enabled:
	 */
	_PQ_ASSERT_INACTIVE("_pq_first: pq_active");
	_PQ_SET_ACTIVE();
	_PQ_ASSERT_PROTECTED("_pq_first: prioq not protected!");

	while (((pql = TAILQ_FIRST(&pq->pq_queue)) != NULL) &&
	    (pthread == NULL)) {
		if ((pthread = TAILQ_FIRST(&pql->pl_head)) == NULL) {
			/*
			 * The priority list is empty; remove the list
			 * from the queue.
			 */
			TAILQ_REMOVE(&pq->pq_queue, pql, pl_link);

			/* Mark the list as not being in the queue: */
			pql->pl_queued = 0;
		} else if ((pthread->flags & PTHREAD_FLAGS_SUSPENDED) != 0) {
			/*
			 * This thread is suspended; remove it from the
			 * list and ensure its state is suspended.
			 */
			TAILQ_REMOVE(&pql->pl_head, pthread, pqe);
			PTHREAD_SET_STATE(pthread, PS_SUSPENDED);

			/* This thread is now longer in the priority queue. */
			pthread->flags &= ~PTHREAD_FLAGS_IN_PRIOQ;
			pthread = NULL;
		}
	}

	_PQ_CLEAR_ACTIVE();
	return (pthread);
}


static void
pq_insert_prio_list(pq_queue_t *pq, int prio)
{
	pq_list_t *pql;

	/*
	 * Make some assertions when debugging is enabled:
	 */
	_PQ_ASSERT_ACTIVE("pq_insert_prio_list: pq_active");
	_PQ_ASSERT_PROTECTED("_pq_insert_prio_list: prioq not protected!");

	/*
	 * The priority queue is in descending priority order.  Start at
	 * the beginning of the queue and find the list before which the
	 * new list should be inserted.
	 */
	pql = TAILQ_FIRST(&pq->pq_queue);
	while ((pql != NULL) && (pql->pl_prio > prio))
		pql = TAILQ_NEXT(pql, pl_link);

	/* Insert the list: */
	if (pql == NULL)
		TAILQ_INSERT_TAIL(&pq->pq_queue, &pq->pq_lists[prio], pl_link);
	else
		TAILQ_INSERT_BEFORE(pql, &pq->pq_lists[prio], pl_link);

	/* Mark this list as being in the queue: */
	pq->pq_lists[prio].pl_queued = 1;
}

void
_waitq_insert(pthread_t pthread)
{
	pthread_t	tid;

	/*
	 * Make some assertions when debugging is enabled:
	 */
	_PQ_ASSERT_INACTIVE("_waitq_insert: pq_active");
	_PQ_SET_ACTIVE();
	_PQ_ASSERT_NOT_QUEUED(pthread, "_waitq_insert: Already in queue");

	if (pthread->wakeup_time.tv_sec == -1)
		TAILQ_INSERT_TAIL(&_waitingq, pthread, pqe);
	else {
		tid = TAILQ_FIRST(&_waitingq);
		while ((tid != NULL) && (tid->wakeup_time.tv_sec != -1) &&
		    ((tid->wakeup_time.tv_sec < pthread->wakeup_time.tv_sec) ||
		    ((tid->wakeup_time.tv_sec == pthread->wakeup_time.tv_sec) &&
		    (tid->wakeup_time.tv_nsec <= pthread->wakeup_time.tv_nsec))))
			tid = TAILQ_NEXT(tid, pqe);
		if (tid == NULL)
			TAILQ_INSERT_TAIL(&_waitingq, pthread, pqe);
		else
			TAILQ_INSERT_BEFORE(tid, pthread, pqe);
	}
	pthread->flags |= PTHREAD_FLAGS_IN_WAITQ;

	_PQ_CLEAR_ACTIVE();
}

void
_waitq_remove(pthread_t pthread)
{
	/*
	 * Make some assertions when debugging is enabled:
	 */
	_PQ_ASSERT_INACTIVE("_waitq_remove: pq_active");
	_PQ_SET_ACTIVE();
	_PQ_ASSERT_IN_WAITQ(pthread, "_waitq_remove: Not in queue");

	TAILQ_REMOVE(&_waitingq, pthread, pqe);
	pthread->flags &= ~PTHREAD_FLAGS_IN_WAITQ;

	_PQ_CLEAR_ACTIVE();
}

void
_waitq_setactive(void)
{
	_PQ_ASSERT_INACTIVE("_waitq_setactive: pq_active");
	_PQ_SET_ACTIVE();
} 

void
_waitq_clearactive(void)
{
	_PQ_ASSERT_ACTIVE("_waitq_clearactive: ! pq_active");
	_PQ_CLEAR_ACTIVE();
}
