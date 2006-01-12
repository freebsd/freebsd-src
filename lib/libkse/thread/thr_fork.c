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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <spinlock.h>
#include <sys/signalvar.h>

#include "libc_private.h"
#include "thr_private.h"

__weak_reference(_fork, fork);

pid_t
_fork(void)
{
	sigset_t sigset, oldset;
	struct pthread *curthread;
	struct pthread_atfork *af;
	pid_t ret;
	int errsave;

	curthread = _get_curthread();

	if (!_kse_isthreaded()) {
		SIGFILLSET(sigset);
		__sys_sigprocmask(SIG_SETMASK, &sigset, &oldset);
		ret = __sys_fork();
		if (ret == 0)
			/* Child */
			__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask,
			    NULL);
		else
			__sys_sigprocmask(SIG_SETMASK, &oldset, NULL);
		return (ret);
	}

	/*
	 * Masks all signals until we reach a safe point in
	 * _kse_single_thread, and the signal masks will be
	 * restored in that function, for M:N thread, all 
	 * signals were already masked in kernel atomically,
	 * we only need to do this for bound thread.
	 */
	if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM) {
		SIGFILLSET(sigset);
		__sys_sigprocmask(SIG_SETMASK, &sigset, &oldset);
	}

	_pthread_mutex_lock(&_thr_atfork_mutex);

	/* Run down atfork prepare handlers. */
	TAILQ_FOREACH_REVERSE(af, &_thr_atfork_list, atfork_head, qe) {
		if (af->prepare != NULL)
			af->prepare();
	}

	/* Fork a new process: */
	if (_kse_isthreaded() != 0) {
		_malloc_prefork();
	}
	if ((ret = __sys_fork()) == 0) {
		/* Child process */
		errsave = errno; 

		/* Kernel signal mask is restored in _kse_single_thread */
		_kse_single_thread(curthread);

		/* Run down atfork child handlers. */
		TAILQ_FOREACH(af, &_thr_atfork_list, qe) {
			if (af->child != NULL)
				af->child();
		}
		_thr_mutex_reinit(&_thr_atfork_mutex);
	} else {
		if (_kse_isthreaded() != 0) {
			_malloc_postfork();
		}
		errsave = errno; 
		if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM) {
			__sys_sigprocmask(SIG_SETMASK, &oldset, NULL);
		}
		/* Run down atfork parent handlers. */
		TAILQ_FOREACH(af, &_thr_atfork_list, qe) {
			if (af->parent != NULL)
				af->parent();
		}
		_pthread_mutex_unlock(&_thr_atfork_mutex);
	}
	errno = errsave;

	/* Return the process ID: */
	return (ret);
}
