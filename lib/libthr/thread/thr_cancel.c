/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 * $FreeBSD$
 */
#include <sys/errno.h>
#include <pthread.h>
#include "thr_private.h"

__weak_reference(_pthread_cancel, pthread_cancel);
__weak_reference(_pthread_setcancelstate, pthread_setcancelstate);
__weak_reference(_pthread_setcanceltype, pthread_setcanceltype);
__weak_reference(_pthread_testcancel, pthread_testcancel);

int
_pthread_cancel(pthread_t pthread)
{
	int ret;

	if ((ret = _find_thread(pthread)) != 0) {
		/* NOTHING */
	} else if (pthread->state == PS_DEAD || pthread->state == PS_DEADLOCK
	    || (pthread->flags & PTHREAD_EXITING) != 0) {
		ret = 0;
	} else {
		GIANT_LOCK(curthread);

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
				if (pthread->join_status.thread != NULL) {
					pthread->join_status.thread->joiner
					    = NULL;
					pthread->join_status.thread = NULL;
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
		GIANT_UNLOCK(curthread);

		ret = 0;
	}
	return (ret);
}

int
_pthread_setcancelstate(int state, int *oldstate)
{
	int ostate;

	GIANT_LOCK(curthread);
	ostate = curthread->cancelflags & PTHREAD_CANCEL_DISABLE;

	switch (state) {
	case PTHREAD_CANCEL_ENABLE:
		if (oldstate != NULL)
			*oldstate = ostate;
		curthread->cancelflags &= ~PTHREAD_CANCEL_DISABLE;
		if ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) == 0) 
			break;
		GIANT_UNLOCK(curthread);
		pthread_testcancel();
		break;
	case PTHREAD_CANCEL_DISABLE:
		if (oldstate != NULL)
			*oldstate = ostate;
		curthread->cancelflags |= PTHREAD_CANCEL_DISABLE;
		GIANT_UNLOCK(curthread);
		break;
	default:
		GIANT_UNLOCK(curthread);
		return (EINVAL);
	}

	return (0);
}

int
_pthread_setcanceltype(int type, int *oldtype)
{
	int otype;

	GIANT_LOCK(curthread);
	otype = curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		if (oldtype != NULL)
			*oldtype = otype;
		curthread->cancelflags |= PTHREAD_CANCEL_ASYNCHRONOUS;
		GIANT_UNLOCK(curthread);
		pthread_testcancel();
		break;
	case PTHREAD_CANCEL_DEFERRED:
		if (oldtype != NULL)
			*oldtype = otype;
		curthread->cancelflags &= ~PTHREAD_CANCEL_ASYNCHRONOUS;
		GIANT_UNLOCK(curthread);
		break;
	default:
		GIANT_UNLOCK(curthread);
		return (EINVAL);
	}

	return (0);
}

/*
 * XXXTHR Make an internal locked version.
 */
void
_pthread_testcancel(void)
{
	GIANT_LOCK(curthread);
	if (((curthread->cancelflags & PTHREAD_CANCEL_DISABLE) == 0) &&
	    ((curthread->cancelflags & PTHREAD_CANCELLING) != 0) &&
	    ((curthread->flags & PTHREAD_EXITING) == 0)) {
		/*
		 * It is possible for this thread to be swapped out
		 * while performing cancellation; do not allow it
		 * to be cancelled again.
		 */
		curthread->cancelflags &= ~PTHREAD_CANCELLING;
		GIANT_UNLOCK(curthread);
		_thread_exit_cleanup();
		pthread_exit(PTHREAD_CANCELED);
		PANIC("cancel");
	}
	GIANT_UNLOCK(curthread);
}

void
_thread_enter_cancellation_point(void)
{
	pthread_testcancel();

	GIANT_LOCK(curthread);
	curthread->cancelflags |= PTHREAD_AT_CANCEL_POINT;
	GIANT_UNLOCK(curthread);
}

void
_thread_leave_cancellation_point(void)
{
	GIANT_LOCK(curthread);
	curthread->cancelflags &= ~PTHREAD_AT_CANCEL_POINT;
	GIANT_UNLOCK(curthread);

	pthread_testcancel();
}
