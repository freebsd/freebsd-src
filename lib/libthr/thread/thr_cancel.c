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

/*
 * Posix requires this function to be async-cancel-safe, so it
 * may not aquire any type of resource or call any functions
 * that might do so.
 */
int
_pthread_cancel(pthread_t pthread)
{
	/* Don't continue if cancellation has already been set. */
	if (atomic_cmpset_int(&pthread->cancellation, (int)CS_NULL,
	    (int)CS_PENDING) != 1)
		return (0);

	/*
	 * Only wakeup threads that are in cancellation points or
	 * have set async cancel.
	 * XXX - access to pthread->flags is not safe. We should just
	 *	 unconditionally wake the thread and make sure that
	 *	 the the library correctly handles spurious wakeups.
	 */
	if ((pthread->cancellationpoint || pthread->cancelmode == M_ASYNC) &&
	    (pthread->flags & PTHREAD_FLAGS_NOT_RUNNING) != 0)
		PTHREAD_WAKE(pthread);
	return (0);
}

/*
 * Posix requires this function to be async-cancel-safe, so it
 * may not aquire any type of resource or call any functions
 * that might do so.
 */
int
_pthread_setcancelstate(int state, int *oldstate)
{
	int ostate;

	ostate = (curthread->cancelmode == M_OFF) ? PTHREAD_CANCEL_DISABLE :
	    PTHREAD_CANCEL_ENABLE;
	switch (state) {
	case PTHREAD_CANCEL_ENABLE:
		curthread->cancelmode = curthread->cancelstate;
		break;
	case PTHREAD_CANCEL_DISABLE:
		if (curthread->cancelmode != M_OFF) {
			curthread->cancelstate = curthread->cancelmode;
			curthread->cancelmode = M_OFF;
		}
		break;
	default:
		return (EINVAL);
	}
	if (oldstate != NULL)
		*oldstate = ostate;
	return (0);
}

/*
 * Posix requires this function to be async-cancel-safe, so it
 * may not aquire any type of resource or call any functions that
 * might do so.
 */
int
_pthread_setcanceltype(int type, int *oldtype)
{
	enum cancel_mode omode;

	omode = curthread->cancelstate;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		if (curthread->cancelmode != M_OFF)
			curthread->cancelmode = M_ASYNC;
		curthread->cancelstate = M_ASYNC;
		break;
	case PTHREAD_CANCEL_DEFERRED:
		if (curthread->cancelmode != M_OFF)
			curthread->cancelmode = M_DEFERRED;
		curthread->cancelstate = M_DEFERRED;
		break;
	default:
		return (EINVAL);
	}
	if (oldtype != NULL) {
		if (omode == M_DEFERRED)
			*oldtype = PTHREAD_CANCEL_DEFERRED;
		else if (omode == M_ASYNC)
			*oldtype = PTHREAD_CANCEL_ASYNCHRONOUS;
	}
	return (0);
}

void
_pthread_testcancel(void)
{
	testcancel();
}

static void
testcancel()
{
	if (curthread->cancelmode != M_OFF) {

		/* Cleanup a canceled thread only once. */
		if (atomic_cmpset_int(&curthread->cancellation,
		    (int)CS_PENDING, (int)CS_SET) == 1) {
			_thread_exit_cleanup();
			pthread_exit(PTHREAD_CANCELED);
			PANIC("cancel");
		}
	}
}

void
_thread_enter_cancellation_point(void)
{
	testcancel();
	curthread->cancellationpoint = 1;
}

void
_thread_leave_cancellation_point(void)
{
	curthread->cancellationpoint = 0;
	testcancel();
}
