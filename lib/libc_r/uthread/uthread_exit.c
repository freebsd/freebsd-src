/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "pthread_private.h"

#define FLAGS_IN_SCHEDQ	\
	(PTHREAD_FLAGS_IN_PRIOQ|PTHREAD_FLAGS_IN_WAITQ|PTHREAD_FLAGS_IN_WORKQ)

__weak_reference(_pthread_exit, pthread_exit);

void _exit(int status)
{
	int		flags;
	int             i;
	struct itimerval itimer;

	/* Disable the interval timer: */
	itimer.it_interval.tv_sec  = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_sec     = 0;
	itimer.it_value.tv_usec    = 0;
	setitimer(_ITIMER_SCHED_TIMER, &itimer, NULL);

	/* Close the pthread kernel pipe: */
	__sys_close(_thread_kern_pipe[0]);
	__sys_close(_thread_kern_pipe[1]);

	/*
	 * Enter a loop to set all file descriptors to blocking
	 * if they were not created as non-blocking:
	 */
	for (i = 0; i < _thread_dtablesize; i++) {
		/* Check if this file descriptor is in use: */
		if (_thread_fd_table[i] != NULL &&
			!(_thread_fd_table[i]->flags & O_NONBLOCK)) {
			/* Get the current flags: */
			flags = __sys_fcntl(i, F_GETFL, NULL);
			/* Clear the nonblocking file descriptor flag: */
			__sys_fcntl(i, F_SETFL, flags & ~O_NONBLOCK);
		}
	}

	/* Call the _exit syscall: */
	__sys_exit(status);
}

void
_thread_exit(char *fname, int lineno, char *string)
{
	char            s[256];

	/* Prepare an error message string: */
	snprintf(s, sizeof(s),
	    "Fatal error '%s' at line %d in file %s (errno = %d)\n",
	    string, lineno, fname, errno);

	/* Write the string to the standard error file descriptor: */
	__sys_write(2, s, strlen(s));

	/* Force this process to exit: */
	/* XXX - Do we want abort to be conditional on _PTHREADS_INVARIANTS? */
#if defined(_PTHREADS_INVARIANTS)
	abort();
#else
	__sys_exit(1);
#endif
}

/*
 * Only called when a thread is cancelled.  It may be more useful
 * to call it from pthread_exit() if other ways of asynchronous or
 * abnormal thread termination can be found.
 */
void
_thread_exit_cleanup(void)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * POSIX states that cancellation/termination of a thread should
	 * not release any visible resources (such as mutexes) and that
	 * it is the applications responsibility.  Resources that are
	 * internal to the threads library, including file and fd locks,
	 * are not visible to the application and need to be released.
	 */
	/* Unlock all owned fd locks: */
	_thread_fd_unlock_owned(curthread);

	/* Unlock all private mutexes: */
	_mutex_unlock_private(curthread);

	/*
	 * This still isn't quite correct because we don't account
	 * for held spinlocks (see libc/stdlib/malloc.c).
	 */
}

void
_pthread_exit(void *status)
{
	struct pthread	*curthread = _get_curthread();
	pthread_t pthread;

	/* Check if this thread is already in the process of exiting: */
	if ((curthread->flags & PTHREAD_EXITING) != 0) {
		char msg[128];
		snprintf(msg, sizeof(msg), "Thread %p has called pthread_exit() from a destructor. POSIX 1003.1 1996 s16.2.5.2 does not allow this!",curthread);
		PANIC(msg);
	}

	/* Flag this thread as exiting: */
	curthread->flags |= PTHREAD_EXITING;

	/* Save the return value: */
	curthread->ret = status;

	while (curthread->cleanup != NULL) {
		pthread_cleanup_pop(1);
	}
	if (curthread->attr.cleanup_attr != NULL) {
		curthread->attr.cleanup_attr(curthread->attr.arg_attr);
	}
	/* Check if there is thread specific data: */
	if (curthread->specific != NULL) {
		/* Run the thread-specific data destructors: */
		_thread_cleanupspecific();
	}

	/* Free thread-specific poll_data structure, if allocated: */
	if (curthread->poll_data.fds != NULL) {
		free(curthread->poll_data.fds);
		curthread->poll_data.fds = NULL;
	}

	/*
	 * Lock the garbage collector mutex to ensure that the garbage
	 * collector is not using the dead thread list.
	 */
	if (pthread_mutex_lock(&_gc_mutex) != 0)
		PANIC("Cannot lock gc mutex");

	/* Add this thread to the list of dead threads. */
	TAILQ_INSERT_HEAD(&_dead_list, curthread, dle);

	/*
	 * Signal the garbage collector thread that there is something
	 * to clean up.
	 */
	if (pthread_cond_signal(&_gc_cond) != 0)
		PANIC("Cannot signal gc cond");

	/*
	 * Avoid a race condition where a scheduling signal can occur
	 * causing the garbage collector thread to run.  If this happens,
	 * the current thread can be cleaned out from under us.
	 */
	_thread_kern_sig_defer();

	/* Unlock the garbage collector mutex: */
	if (pthread_mutex_unlock(&_gc_mutex) != 0)
		PANIC("Cannot unlock gc mutex");

	/* Check if there is a thread joining this one: */
	if (curthread->joiner != NULL) {
		pthread = curthread->joiner;
		curthread->joiner = NULL;

		switch (pthread->suspended) {
		case SUSP_JOIN:
			/*
			 * The joining thread is suspended.  Change the
			 * suspension state to make the thread runnable when it
			 * is resumed:
			 */
			pthread->suspended = SUSP_NO;
			break;
		case SUSP_NO:
			/* Make the joining thread runnable: */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
			break;
		default:
			PANIC("Unreachable code reached");
		}

		/* Set the return value for the joining thread: */
		pthread->join_status.ret = curthread->ret;
		pthread->join_status.error = 0;
		pthread->join_status.thread = NULL;

		/* Make this thread collectable by the garbage collector. */
		PTHREAD_ASSERT(((curthread->attr.flags & PTHREAD_DETACHED) ==
		    0), "Cannot join a detached thread");
		curthread->attr.flags |= PTHREAD_DETACHED;
	}

	/* Remove this thread from the thread list: */
	TAILQ_REMOVE(&_thread_list, curthread, tle);

	/* This thread will never be re-scheduled. */
	_thread_kern_sched_state(PS_DEAD, __FILE__, __LINE__);

	/* This point should not be reached. */
	PANIC("Dead thread has resumed");
}
