/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 * $FreeBSD$
 */
#include <sys/errno.h>
#include <pthread.h>
#include <stdlib.h>
#include "thr_private.h"

/*
 * Static prototypes
 */
static void	testcancel(void);

__weak_reference(_pthread_cancel, pthread_cancel);
__weak_reference(_pthread_setcancelstate, pthread_setcancelstate);
__weak_reference(_pthread_setcanceltype, pthread_setcanceltype);
__weak_reference(_pthread_testcancel, pthread_testcancel);

int
_pthread_cancel(pthread_t pthread)
{
	int ret;
	pthread_t joined;

	/*
	 * When canceling a thread that has joined another thread, this
	 * routine breaks the normal lock order of locking first the
	 * joined and then the joiner. Therefore, it is necessary that
	 * if it can't obtain the second lock, that it release the first
	 * one and restart from the top.
	 */
retry:
	if ((ret = _find_thread(pthread)) != 0)
		/* The thread is not on the list of active threads */
		goto out;

	_thread_critical_enter(pthread);

	if (pthread->state == PS_DEAD || pthread->state == PS_DEADLOCK
	    || (pthread->flags & PTHREAD_EXITING) != 0) {
		/*
		 * The thread is in the process of (or has already) exited
		 * or is deadlocked.
		 */
		_thread_critical_exit(pthread);
		ret = 0;
		goto out;
	}

	/*
	 * The thread is on the active thread list and is not in the process
	 * of exiting.
	 */

	if (((pthread->cancelflags & PTHREAD_CANCEL_DISABLE) != 0) ||
	    (((pthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) == 0) &&
	    ((pthread->cancelflags & PTHREAD_AT_CANCEL_POINT) == 0)))
		/* Just mark it for cancellation: */
		pthread->cancelflags |= PTHREAD_CANCELLING;
	else {
		/*
		 * Check if we need to kick it back into the
		 * run queue:
		 */
		switch (pthread->state) {
		case PS_RUNNING:
			/* No need to resume: */
			pthread->cancelflags |= PTHREAD_CANCELLING;
			break;

		case PS_SLEEP_WAIT:
		case PS_WAIT_WAIT:
			pthread->cancelflags |= PTHREAD_CANCELLING;
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
			break;

		case PS_JOIN:
			/*
			 * Disconnect the thread from the joinee:
			 */
			if ((joined = pthread->join_status.thread) != NULL) {
				if (_spintrylock(&joined->lock) == EBUSY) {
					_thread_critical_exit(pthread);
					goto retry;
				}
				pthread->join_status.thread->joiner = NULL;
				_spinunlock(&joined->lock);
				joined = pthread->join_status.thread = NULL;
			}
			pthread->cancelflags |= PTHREAD_CANCELLING;
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
			break;

		case PS_MUTEX_WAIT:
		case PS_COND_WAIT:
			/*
			 * Threads in these states may be in queues.
			 * In order to preserve queue integrity, the
			 * cancelled thread must remove itself from the
			 * queue.  When the thread resumes, it will
			 * remove itself from the queue and call the
			 * cancellation routine.
			 */
			pthread->cancelflags |= PTHREAD_CANCELLING;
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
			break;

		case PS_DEAD:
		case PS_DEADLOCK:
		case PS_STATE_MAX:
			/* Ignore - only here to silence -Wall: */
			break;
		}
	}

	/* Unprotect the scheduling queues: */
	_thread_critical_exit(pthread);

	ret = 0;
out:
	return (ret);
}

int
_pthread_setcancelstate(int state, int *oldstate)
{
	int ostate, ret;

	ret = 0;

	_thread_critical_enter(curthread);

	ostate = curthread->cancelflags & PTHREAD_CANCEL_DISABLE;

	switch (state) {
	case PTHREAD_CANCEL_ENABLE:
		if (oldstate != NULL)
			*oldstate = ostate;
		curthread->cancelflags &= ~PTHREAD_CANCEL_DISABLE;
		if ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) == 0)
			break;
		testcancel();
		break;
	case PTHREAD_CANCEL_DISABLE:
		if (oldstate != NULL)
			*oldstate = ostate;
		curthread->cancelflags |= PTHREAD_CANCEL_DISABLE;
		break;
	default:
		ret = EINVAL;
	}

	_thread_critical_exit(curthread);
	return (ret);
}

int
_pthread_setcanceltype(int type, int *oldtype)
{
	int otype;

	_thread_critical_enter(curthread);
	otype = curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		if (oldtype != NULL)
			*oldtype = otype;
		curthread->cancelflags |= PTHREAD_CANCEL_ASYNCHRONOUS;
		testcancel();
		break;
	case PTHREAD_CANCEL_DEFERRED:
		if (oldtype != NULL)
			*oldtype = otype;
		curthread->cancelflags &= ~PTHREAD_CANCEL_ASYNCHRONOUS;
		break;
	default:
		return (EINVAL);
	}

	_thread_critical_exit(curthread);
	return (0);
}

void
_pthread_testcancel(void)
{
	_thread_critical_enter(curthread);
	testcancel();
	_thread_critical_exit(curthread);
}

static void
testcancel()
{
	/*
	 * This pthread should already be locked by the caller.
	 */

	if (((curthread->cancelflags & PTHREAD_CANCEL_DISABLE) == 0) &&
	    ((curthread->cancelflags & PTHREAD_CANCELLING) != 0) &&
	    ((curthread->flags & PTHREAD_EXITING) == 0)) {
		/*
		 * It is possible for this thread to be swapped out
		 * while performing cancellation; do not allow it
		 * to be cancelled again.
		 */
		curthread->cancelflags &= ~PTHREAD_CANCELLING;
		_thread_critical_exit(curthread);
		_thread_exit_cleanup();
		pthread_exit(PTHREAD_CANCELED);
		PANIC("cancel");
	}
}

void
_thread_enter_cancellation_point(void)
{
	_thread_critical_enter(curthread);
	testcancel();
	curthread->cancelflags |= PTHREAD_AT_CANCEL_POINT;
	_thread_critical_exit(curthread);
}

void
_thread_leave_cancellation_point(void)
{
	_thread_critical_enter(curthread);
	curthread->cancelflags &= ~PTHREAD_AT_CANCEL_POINT;
	testcancel();
	_thread_critical_exit(curthread);

}
