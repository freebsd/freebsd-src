/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "thr_private.h"

/*
 * Prototypes
 */
static pthread_t	cond_queue_deq(pthread_cond_t);
static void		cond_queue_remove(pthread_cond_t, pthread_t);
static void		cond_queue_enq(pthread_cond_t, pthread_t);
static int		cond_wait_common(pthread_cond_t *,
			    pthread_mutex_t *, const struct timespec *);

__weak_reference(_pthread_cond_init, pthread_cond_init);
__weak_reference(_pthread_cond_destroy, pthread_cond_destroy);
__weak_reference(_pthread_cond_wait, pthread_cond_wait);
__weak_reference(_pthread_cond_timedwait, pthread_cond_timedwait);
__weak_reference(_pthread_cond_signal, pthread_cond_signal);
__weak_reference(_pthread_cond_broadcast, pthread_cond_broadcast);

#define	COND_LOCK(c)						\
do {								\
	if (umtx_lock(&(c)->c_lock, curthread->thr_id))		\
		abort();					\
} while (0)

#define	COND_UNLOCK(c)						\
do {								\
	if (umtx_unlock(&(c)->c_lock, curthread->thr_id))	\
		abort();					\
} while (0)


/* Reinitialize a condition variable to defaults. */
int
_cond_reinit(pthread_cond_t *cond)
{
	if (cond == NULL)
		return (EINVAL);

	if (*cond == NULL)
		return (pthread_cond_init(cond, NULL));

	/*
	 * Initialize the condition variable structure:
	 */
	TAILQ_INIT(&(*cond)->c_queue);
	(*cond)->c_flags = COND_FLAGS_INITED;
	(*cond)->c_type = COND_TYPE_FAST;
	(*cond)->c_mutex = NULL;
	(*cond)->c_seqno = 0;
	bzero(&(*cond)->c_lock, sizeof((*cond)->c_lock));

	return (0);
}

int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
	enum pthread_cond_type type;
	pthread_cond_t	pcond;

	if (cond == NULL)
		return (EINVAL);

	/*
	 * Check if a pointer to a condition variable attribute
	 * structure was passed by the caller: 
	 */
	if (cond_attr != NULL && *cond_attr != NULL)
		type = (*cond_attr)->c_type;
	else
		/* Default to a fast condition variable: */
		type = COND_TYPE_FAST;

	/* Process according to condition variable type: */
	switch (type) {
	case COND_TYPE_FAST:
		break;
	default:
		return (EINVAL);
		break;
	}

	if ((pcond = (pthread_cond_t)
	    malloc(sizeof(struct pthread_cond))) == NULL)
		return (ENOMEM);
	/*
	 * Initialise the condition variable
	 * structure:
	 */
	TAILQ_INIT(&pcond->c_queue);
	pcond->c_flags |= COND_FLAGS_INITED;
	pcond->c_type = type;
	pcond->c_mutex = NULL;
	pcond->c_seqno = 0;
	bzero(&pcond->c_lock, sizeof(pcond->c_lock));

	*cond = pcond;

	return (0);
}

int
_pthread_cond_destroy(pthread_cond_t *cond)
{
	if (cond == NULL || *cond == NULL)
		return (EINVAL);

	COND_LOCK(*cond);

	/*
	 * Free the memory allocated for the condition
	 * variable structure:
	 */
	free(*cond);

	/*
	 * NULL the caller's pointer now that the condition
	 * variable has been destroyed:
	 */
	*cond = NULL;

	return (0);
}

int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	int rval;

	rval = cond_wait_common(cond, mutex, NULL);

	/* This should never happen. */
	if (rval == ETIMEDOUT)
		abort();

	return (rval);
}

int
_pthread_cond_timedwait(pthread_cond_t * cond, pthread_mutex_t * mutex,
		       const struct timespec * abstime)
{
	if (abstime == NULL || abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cond, mutex, abstime));
}

static int
cond_wait_common(pthread_cond_t * cond, pthread_mutex_t * mutex,
	         const struct timespec * abstime)
{
	int	rval = 0;
	int	done = 0;
	int	seqno;
	int	mtxrval;


	_thread_enter_cancellation_point();
	
	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	if (*cond == NULL && (rval = pthread_cond_init(cond, NULL)) != 0)
		return (rval);


	COND_LOCK(*cond);

	/*
	 * If the condvar was statically allocated, properly
	 * initialize the tail queue.
	 */
	if (((*cond)->c_flags & COND_FLAGS_INITED) == 0) {
		TAILQ_INIT(&(*cond)->c_queue);
		(*cond)->c_flags |= COND_FLAGS_INITED;
	}

	/* Process according to condition variable type. */

	switch ((*cond)->c_type) {
	/* Fast condition variable: */
	case COND_TYPE_FAST:
		if ((mutex == NULL) || (((*cond)->c_mutex != NULL) &&
		    ((*cond)->c_mutex != *mutex))) {
			COND_UNLOCK(*cond);
			rval = EINVAL;
			break;
		} 
		/* Remember the mutex */
		(*cond)->c_mutex = *mutex;

		if ((rval = _mutex_cv_unlock(mutex)) != 0) {
			if (rval == -1){
				printf("foo");
				fflush(stdout);
				abort();
			}

			COND_UNLOCK(*cond);
			break;
		}
		COND_UNLOCK(*cond);

		/*
		 * We need giant for the queue operations.  It also 
		 * protects seqno and the pthread flag fields.  This is
		 * dropped and reacquired in _thread_suspend().
		 */

		GIANT_LOCK(curthread);
		/*
		 * c_seqno is protected by giant.
		 */
		seqno = (*cond)->c_seqno;

		do {
			/*
			 * Queue the running thread on the condition
			 * variable.
			 */
			cond_queue_enq(*cond, curthread);

			if (curthread->cancelflags & PTHREAD_CANCELLING) {
				/*
				 * POSIX Says that we must relock the mutex
				 * even if we're being canceled.
				 */
				GIANT_UNLOCK(curthread);
				_mutex_cv_lock(mutex);
				pthread_testcancel();
				PANIC("Shouldn't have come back.");
			}

			PTHREAD_SET_STATE(curthread, PS_COND_WAIT);
			GIANT_UNLOCK(curthread);
			rval = _thread_suspend(curthread, (struct timespec *)abstime);
			if (rval == -1) {
				printf("foo");
				fflush(stdout);
				abort();
			}
			GIANT_LOCK(curthread);

			done = (seqno != (*cond)->c_seqno);

			cond_queue_remove(*cond, curthread);

		} while ((done == 0) && (rval == 0));
		/*
		 * If we timed out someone still may have signaled us
		 * before we got a chance to run again.  We check for
		 * this by looking to see if our state is RUNNING.
		 */
		if (rval == EAGAIN) {
			if (curthread->state != PS_RUNNING) {
				PTHREAD_SET_STATE(curthread, PS_RUNNING);
				rval = ETIMEDOUT;
			} else
				rval = 0;
		}
		GIANT_UNLOCK(curthread);

		mtxrval = _mutex_cv_lock(mutex);

		/*
		 * If the mutex failed return that error, otherwise we're
		 * returning ETIMEDOUT.
		 */
		if (mtxrval == -1) {
			printf("foo");
			fflush(stdout);
			abort();
		}
		if (mtxrval != 0)
			rval = mtxrval;

		break;

	/* Trap invalid condition variable types: */
	default:
		COND_UNLOCK(*cond);
		rval = EINVAL;
		break;
	}

	/*
	 * See if we have to cancel before we retry.  We could be
	 * canceled with the mutex held here!
	 */
	pthread_testcancel();

	_thread_leave_cancellation_point();

	return (rval);
}

int
_pthread_cond_signal(pthread_cond_t * cond)
{
	int             rval = 0;
	pthread_t       pthread;

	if (cond == NULL)
		return (EINVAL);
       /*
        * If the condition variable is statically initialized, perform dynamic
        * initialization.
        */
	if (*cond == NULL && (rval = pthread_cond_init(cond, NULL)) != 0)
		return (rval);


	COND_LOCK(*cond);

	/* Process according to condition variable type: */
	switch ((*cond)->c_type) {
	/* Fast condition variable: */
	case COND_TYPE_FAST:
		GIANT_LOCK(curthread);
		(*cond)->c_seqno++;

		if ((pthread = cond_queue_deq(*cond)) != NULL) {
			/*
			 * Wake up the signaled thread:
			 */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
		}

		GIANT_UNLOCK(curthread);
		break;

	/* Trap invalid condition variable types: */
	default:
		rval = EINVAL;
		break;
	}


	COND_UNLOCK(*cond);

	return (rval);
}

int
_pthread_cond_broadcast(pthread_cond_t * cond)
{
	int             rval = 0;
	pthread_t       pthread;

	if (cond == NULL)
		return (EINVAL);
       /*
        * If the condition variable is statically initialized, perform dynamic
        * initialization.
        */
	if (*cond == NULL && (rval = pthread_cond_init(cond, NULL)) != 0)
		return (rval);

	COND_LOCK(*cond);

	/* Process according to condition variable type: */
	switch ((*cond)->c_type) {
	/* Fast condition variable: */
	case COND_TYPE_FAST:
		GIANT_LOCK(curthread);
		(*cond)->c_seqno++;

		/*
		 * Enter a loop to bring all threads off the
		 * condition queue:
		 */
		while ((pthread = cond_queue_deq(*cond)) != NULL) {
			/*
			 * Wake up the signaled thread:
			 */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
		}
		GIANT_UNLOCK(curthread);

		/* There are no more waiting threads: */
		(*cond)->c_mutex = NULL;

		break;

	/* Trap invalid condition variable types: */
	default:
		rval = EINVAL;
		break;
	}

	COND_UNLOCK(*cond);


	return (rval);
}

void
_cond_wait_backout(pthread_t pthread)
{
	pthread_cond_t	cond;

	cond = pthread->data.cond;
	if (cond == NULL)
		return;

	COND_LOCK(cond);

	/* Process according to condition variable type: */
	switch (cond->c_type) {
	/* Fast condition variable: */
	case COND_TYPE_FAST:
		GIANT_LOCK(curthread);

		cond_queue_remove(cond, pthread);

		GIANT_UNLOCK(curthread);
		break;

	default:
		break;
	}

	COND_UNLOCK(cond);
}

/*
 * Dequeue a waiting thread from the head of a condition queue in
 * descending priority order.
 */
static pthread_t
cond_queue_deq(pthread_cond_t cond)
{
	pthread_t pthread;

	while ((pthread = TAILQ_FIRST(&cond->c_queue)) != NULL) {
		TAILQ_REMOVE(&cond->c_queue, pthread, sqe);
		cond_queue_remove(cond, pthread);
		if ((pthread->cancelflags & PTHREAD_CANCELLING) == 0 &&
		    pthread->state == PS_COND_WAIT)
			/*
			 * Only exit the loop when we find a thread
			 * that hasn't timed out or been canceled;
			 * those threads are already running and don't
			 * need their run state changed.
			 */
			break;
	}

	return(pthread);
}

/*
 * Remove a waiting thread from a condition queue in descending priority
 * order.
 */
static void
cond_queue_remove(pthread_cond_t cond, pthread_t pthread)
{
	/*
	 * Because pthread_cond_timedwait() can timeout as well
	 * as be signaled by another thread, it is necessary to
	 * guard against removing the thread from the queue if
	 * it isn't in the queue.
	 */
	if (pthread->flags & PTHREAD_FLAGS_IN_CONDQ) {
		TAILQ_REMOVE(&cond->c_queue, pthread, sqe);
		pthread->flags &= ~PTHREAD_FLAGS_IN_CONDQ;
	}
	/* Check for no more waiters. */
	if (TAILQ_FIRST(&cond->c_queue) == NULL)
		cond->c_mutex = NULL;
}

/*
 * Enqueue a waiting thread to a condition queue in descending priority
 * order.
 */
static void
cond_queue_enq(pthread_cond_t cond, pthread_t pthread)
{
	pthread_t tid = TAILQ_LAST(&cond->c_queue, cond_head);
	char *name;

	name = pthread->name ? pthread->name : "unknown";
	if ((pthread->flags & PTHREAD_FLAGS_IN_CONDQ) != 0)
		_thread_printf(2, "Thread (%s:%u) already on condq\n",
		    pthread->name, pthread->uniqueid);
	if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0)
		_thread_printf(2, "Thread (%s:%u) already on mutexq\n",
		    pthread->name, pthread->uniqueid);
	PTHREAD_ASSERT_NOT_IN_SYNCQ(pthread);

	/*
	 * For the common case of all threads having equal priority,
	 * we perform a quick check against the priority of the thread
	 * at the tail of the queue.
	 */
	if ((tid == NULL) || (pthread->active_priority <= tid->active_priority))
		TAILQ_INSERT_TAIL(&cond->c_queue, pthread, sqe);
	else {
		tid = TAILQ_FIRST(&cond->c_queue);
		while (pthread->active_priority <= tid->active_priority)
			tid = TAILQ_NEXT(tid, sqe);
		TAILQ_INSERT_BEFORE(tid, pthread, sqe);
	}
	pthread->flags |= PTHREAD_FLAGS_IN_CONDQ;
	pthread->data.cond = cond;
}
