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
#include "thr_private.h"

__weak_reference(_pthread_exit, pthread_exit);

static void	deadlist_free_threads();

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
	/*
	 * POSIX states that cancellation/termination of a thread should
	 * not release any visible resources (such as mutexes) and that
	 * it is the applications responsibility.  Resources that are
	 * internal to the threads library, including file and fd locks,
	 * are not visible to the application and need to be released.
	 */
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
	struct pthread *pthread;
	int exitNow = 0;

	/*
	 * This thread will no longer handle any signals.
	 */
	_thread_sigblock();

	/* Check if this thread is already in the process of exiting: */
	if (curthread->exiting) {
		char msg[128];
		snprintf(msg, sizeof(msg), "Thread %p has called pthread_exit() from a destructor. POSIX 1003.1 1996 s16.2.5.2 does not allow this!",curthread);
		PANIC(msg);
	}

	/* Flag this thread as exiting: */
	curthread->exiting = 1;

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

	/*
	 * Remove read-write lock list. It is allocated as-needed.
	 * Therefore, it must be checked for validity before freeing.
	 */
	if (curthread->rwlockList != NULL)
		free(curthread->rwlockList);

	/* Lock the dead list first to maintain correct lock order */
	DEAD_LIST_LOCK;
	THREAD_LIST_LOCK;

	/* Check if there is a thread joining this one: */
	if (curthread->joiner != NULL) {
		pthread = curthread->joiner;
		curthread->joiner = NULL;

		/* Set the return value for the joining thread: */
		pthread->join_status.ret = curthread->ret;
		pthread->join_status.error = 0;
		pthread->join_status.thread = NULL;

		/* Make the joining thread runnable: */
		PTHREAD_WAKE(pthread);

		curthread->attr.flags |= PTHREAD_DETACHED;
	}

	/*
	 * Free any memory allocated for dead threads.
	 * Add this thread to the list of dead threads, and
	 * also remove it from the active threads list.
	 */
	deadlist_free_threads();
	TAILQ_INSERT_HEAD(&_dead_list, curthread, dle);
	TAILQ_REMOVE(&_thread_list, curthread, tle);
	
	/* If we're the last thread, call it quits */
	if (TAILQ_EMPTY(&_thread_list))
		exitNow = 1;

	THREAD_LIST_UNLOCK;
	DEAD_LIST_UNLOCK;

	if (exitNow)
		exit(0);

	/*
	 * This function will not return unless we are the last
	 * thread, which we can't be because we've already checked
	 * for that.
	 */
	thr_exit((long *)&curthread->isdead);

	/* This point should not be reached. */
	PANIC("Dead thread has resumed");
}

/*
 * Note: this function must be called with the dead thread list
 *	 locked.
 */
static void
deadlist_free_threads()
{
	struct pthread *ptd, *ptdTemp;

	TAILQ_FOREACH_SAFE(ptd, &_dead_list, dle, ptdTemp) {
		/* Don't destroy the initial thread or non-detached threads. */
		if (ptd == _thread_initial ||
		    (ptd->attr.flags & PTHREAD_DETACHED) == 0)
			continue;
		TAILQ_REMOVE(&_dead_list, ptd, dle);
		deadlist_free_onethread(ptd);
	}
}

void
deadlist_free_onethread(struct pthread *ptd)
{

	if (ptd->attr.stackaddr_attr == NULL && ptd->stack != NULL) {
		STACK_LOCK;
		_thread_stack_free(ptd->stack, ptd->attr.stacksize_attr,
		    ptd->attr.guardsize_attr);
		STACK_UNLOCK;
	}
	_retire_thread(ptd->arch_id);
	free(ptd);
}
