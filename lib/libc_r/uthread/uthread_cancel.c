/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 * $FreeBSD$
 */
#include <sys/errno.h>
#include <pthread.h>
#include "pthread_private.h"

static void	finish_cancellation(void *arg);

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
	} else if (pthread->state == PS_DEAD || pthread->state == PS_DEADLOCK) {
		ret = 0;
	} else {
		/* Protect the scheduling queues: */
		_thread_kern_sig_defer();

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

			case PS_SPINBLOCK:
			case PS_FDR_WAIT:
			case PS_FDW_WAIT:
			case PS_POLL_WAIT:
			case PS_SELECT_WAIT:
				/* Remove these threads from the work queue: */
				if ((pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
				    != 0)
					PTHREAD_WORKQ_REMOVE(pthread);
				/* Fall through: */
			case PS_SIGTHREAD:
			case PS_SLEEP_WAIT:
			case PS_WAIT_WAIT:
			case PS_SIGSUSPEND:
			case PS_SIGWAIT:
				/* Interrupt and resume: */
				pthread->interrupted = 1;
				pthread->cancelflags |= PTHREAD_CANCELLING;
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
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

			case PS_SUSPENDED:
				if (pthread->suspended == SUSP_NO ||
				    pthread->suspended == SUSP_YES ||
				    pthread->suspended == SUSP_JOIN ||
				    pthread->suspended == SUSP_NOWAIT) {
					/*
					 * This thread isn't in any scheduling
					 * queues; just change it's state:
					 */
					pthread->cancelflags |=
					    PTHREAD_CANCELLING;
					PTHREAD_SET_STATE(pthread, PS_RUNNING);
					break;
				}
				/* FALLTHROUGH */
			case PS_MUTEX_WAIT:
			case PS_COND_WAIT:
			case PS_FDLR_WAIT:
			case PS_FDLW_WAIT:
			case PS_FILE_WAIT:
				/*
				 * Threads in these states may be in queues.
				 * In order to preserve queue integrity, the
				 * cancelled thread must remove itself from the
				 * queue.  Mark the thread as interrupted and
				 * needing cancellation, and set the state to
				 * running.  When the thread resumes, it will
				 * remove itself from the queue and call the
				 * cancellation completion routine.
				 */
				pthread->interrupted = 1;
				pthread->cancelflags |= PTHREAD_CANCEL_NEEDED;
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				pthread->continuation = finish_cancellation;
				break;

			case PS_DEAD:
			case PS_DEADLOCK:
			case PS_STATE_MAX:
				/* Ignore - only here to silence -Wall: */
				break;
			}
		}

		/* Unprotect the scheduling queues: */
		_thread_kern_sig_undefer();

		ret = 0;
	}
	return (ret);
}

int
_pthread_setcancelstate(int state, int *oldstate)
{
	struct pthread	*curthread = _get_curthread();
	int ostate;
	int ret;

	ostate = curthread->cancelflags & PTHREAD_CANCEL_DISABLE;

	switch (state) {
	case PTHREAD_CANCEL_ENABLE:
		if (oldstate != NULL)
			*oldstate = ostate;
		curthread->cancelflags &= ~PTHREAD_CANCEL_DISABLE;
		if ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0)
			pthread_testcancel();
		ret = 0;
		break;
	case PTHREAD_CANCEL_DISABLE:
		if (oldstate != NULL)
			*oldstate = ostate;
		curthread->cancelflags |= PTHREAD_CANCEL_DISABLE;
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	return (ret);
}

int
_pthread_setcanceltype(int type, int *oldtype)
{
	struct pthread	*curthread = _get_curthread();
	int otype;
	int ret;

	otype = curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		if (oldtype != NULL)
			*oldtype = otype;
		curthread->cancelflags |= PTHREAD_CANCEL_ASYNCHRONOUS;
		pthread_testcancel();
		ret = 0;
		break;
	case PTHREAD_CANCEL_DEFERRED:
		if (oldtype != NULL)
			*oldtype = otype;
		curthread->cancelflags &= ~PTHREAD_CANCEL_ASYNCHRONOUS;
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	return (ret);
}

void
_pthread_testcancel(void)
{
	struct pthread	*curthread = _get_curthread();

	if (((curthread->cancelflags & PTHREAD_CANCEL_DISABLE) == 0) &&
	    ((curthread->cancelflags & PTHREAD_CANCELLING) != 0)) {
		/*
		 * It is possible for this thread to be swapped out
		 * while performing cancellation; do not allow it
		 * to be cancelled again.
		 */
		curthread->cancelflags &= ~PTHREAD_CANCELLING;
		_thread_exit_cleanup();
		pthread_exit(PTHREAD_CANCELED);
		PANIC("cancel");
	}
}

void
_thread_enter_cancellation_point(void)
{
	struct pthread	*curthread = _get_curthread();

	/* Look for a cancellation before we block: */
	pthread_testcancel();
	curthread->cancelflags |= PTHREAD_AT_CANCEL_POINT;
}

void
_thread_leave_cancellation_point(void)
{
	struct pthread	*curthread = _get_curthread();

	curthread->cancelflags &= ~PTHREAD_AT_CANCEL_POINT;
	/* Look for a cancellation after we unblock: */
	pthread_testcancel();
}

static void
finish_cancellation(void *arg)
{
	struct pthread	*curthread = _get_curthread();

	curthread->continuation = NULL;
	curthread->interrupted = 0;

	if ((curthread->cancelflags & PTHREAD_CANCEL_NEEDED) != 0) {
		curthread->cancelflags &= ~PTHREAD_CANCEL_NEEDED;
		_thread_exit_cleanup();
		pthread_exit(PTHREAD_CANCELED);
	}
}
