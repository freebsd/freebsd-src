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
 * 3. Neither the name of the author nor the names of any co-contributors
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

#include "namespace.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "thr_private.h"

void	_pthread_exit(void *status);

__weak_reference(_pthread_exit, pthread_exit);

void
_thread_exit(const char *fname, int lineno, const char *msg)
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
}

void
_pthread_exit(void *status)
{
	struct pthread *curthread = _get_curthread();

	/* Check if this thread is already in the process of exiting: */
	if (curthread->cancelling) {
		char msg[128];
		snprintf(msg, sizeof(msg), "Thread %p has called "
		    "pthread_exit() from a destructor. POSIX 1003.1 "
		    "1996 s16.2.5.2 does not allow this!", curthread);
		PANIC(msg);
	}

	/* Flag this thread as exiting. */
	curthread->cancelling = 1;
	
	_thr_exit_cleanup();

	/* Save the return value: */
	curthread->ret = status;
	while (curthread->cleanup != NULL) {
		_pthread_cleanup_pop(1);
	}

	/* Check if there is thread specific data: */
	if (curthread->specific != NULL) {
		/* Run the thread-specific data destructors: */
		_thread_cleanupspecific();
	}

	/* Tell malloc that the thread is exiting. */
	_malloc_thread_cleanup();

	if (!_thr_isthreaded())
		exit(0);

	THREAD_LIST_LOCK(curthread);
	_thread_active_threads--;
	if (_thread_active_threads == 0) {
		THREAD_LIST_UNLOCK(curthread);
		exit(0);
		/* Never reach! */
	}
	THR_LOCK(curthread);
	curthread->state = PS_DEAD;
	if (curthread->flags & THR_FLAGS_NEED_SUSPEND) {
		curthread->cycle++;
		_thr_umtx_wake(&curthread->cycle, INT_MAX, 0);
	}
	THR_UNLOCK(curthread);
	/*
	 * Thread was created with initial refcount 1, we drop the
	 * reference count to allow it to be garbage collected.
	 */
	curthread->refcount--;
	if (curthread->tlflags & TLFLAGS_DETACHED)
		THR_GCLIST_ADD(curthread);
	THREAD_LIST_UNLOCK(curthread);
	if (!curthread->force_exit && SHOULD_REPORT_EVENT(curthread, TD_DEATH))
		_thr_report_death(curthread);

	/*
	 * Kernel will do wakeup at the address, so joiner thread
	 * will be resumed if it is sleeping at the address.
	 */
	thr_exit(&curthread->tid);
	PANIC("thr_exit() returned");
	/* Never reach! */
}
