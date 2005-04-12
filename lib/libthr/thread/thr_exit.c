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
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "thr_private.h"

void	_pthread_exit(void *status);

__weak_reference(_pthread_exit, pthread_exit);

void
_thread_exit(char *fname, int lineno, char *msg)
{

	/* Write an error message to the standard error file descriptor: */
	_thread_printf(2,
	    "Fatal error '%s' at line %d in file %s (errno = %d)\n",
	    msg, lineno, fname, errno);

	abort();
}

/*
 * Only called when a thread is cancelled.  It may be more useful
 * to call it from pthread_exit() if other ways of asynchronous or
 * abnormal thread termination can be found.
 */
void
_thr_exit_cleanup(void)
{
	struct pthread	*curthread = _get_curthread();

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
	struct pthread *curthread = _get_curthread();

	/* Check if this thread is already in the process of exiting: */
	if ((curthread->cancelflags & THR_CANCEL_EXITING) != 0) {
		char msg[128];
		snprintf(msg, sizeof(msg), "Thread %p has called "
		    "pthread_exit() from a destructor. POSIX 1003.1 "
		    "1996 s16.2.5.2 does not allow this!", curthread);
		PANIC(msg);
	}

	/* Flag this thread as exiting. */
	atomic_set_int(&curthread->cancelflags, THR_CANCEL_EXITING);
	
	_thr_exit_cleanup();

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

	if (!_thr_isthreaded())
		exit(0);

	THREAD_LIST_LOCK(curthread);
	_thread_active_threads--;
	if (_thread_active_threads == 0) {
		THREAD_LIST_UNLOCK(curthread);
		exit(0);
		/* Never reach! */
	}
	if (curthread->tlflags & TLFLAGS_DETACHED)
		THR_GCLIST_ADD(curthread);
	curthread->state = PS_DEAD;
	THREAD_LIST_UNLOCK(curthread);
	if (curthread->joiner)
		_thr_umtx_wake(&curthread->state, INT_MAX);
	if (SHOULD_REPORT_EVENT(curthread, TD_DEATH))
		_thr_report_death(curthread);
	thr_exit(&curthread->tid);
	PANIC("thr_exit() returned");
	/* Never reach! */
}
