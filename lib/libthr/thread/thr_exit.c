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
#include <sys/types.h>
#include <sys/signalvar.h>
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

void
_pthread_exit(void *status)
{
	_pthread_exit_mask(status, NULL);
}

void
_pthread_exit_mask(void *status, sigset_t *mask)
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
	curthread->cancel_enable = 0;
	curthread->cancel_async = 0;
	curthread->cancel_point = 0;
	if (mask != NULL)
		__sys_sigprocmask(SIG_SETMASK, mask, NULL);
	if (curthread->unblock_sigcancel) {
		sigset_t set;

		curthread->unblock_sigcancel = 0;
		SIGEMPTYSET(set);
		SIGADDSET(set, SIGCANCEL);
		__sys_sigprocmask(SIG_UNBLOCK, mask, NULL);
	}
	
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

	if (!_thr_isthreaded())
		exit(0);

	if (atomic_fetchadd_int(&_thread_active_threads, -1) == 1) {
		exit(0);
		/* Never reach! */
	}

	/* Tell malloc that the thread is exiting. */
	_malloc_thread_cleanup();

	THR_LOCK(curthread);
	curthread->state = PS_DEAD;
	if (curthread->flags & THR_FLAGS_NEED_SUSPEND) {
		curthread->cycle++;
		_thr_umtx_wake(&curthread->cycle, INT_MAX, 0);
	}
	/*
	 * Thread was created with initial refcount 1, we drop the
	 * reference count to allow it to be garbage collected.
	 */
	curthread->refcount--;
	_thr_try_gc(curthread, curthread); /* thread lock released */

	if (!curthread->force_exit && SHOULD_REPORT_EVENT(curthread, TD_DEATH))
		_thr_report_death(curthread);

#if defined(_PTHREADS_INVARIANTS)
	if (THR_IN_CRITICAL(curthread))
		PANIC("thread exits with resources held!");
#endif
	/*
	 * Kernel will do wakeup at the address, so joiner thread
	 * will be resumed if it is sleeping at the address.
	 */
	thr_exit(&curthread->tid);
	PANIC("thr_exit() returned");
	/* Never reach! */
}
