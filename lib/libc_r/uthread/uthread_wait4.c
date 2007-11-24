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
#include <sys/types.h>

#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(__wait4, wait4);

pid_t
_wait4(pid_t pid, int *istat, int options, struct rusage * rusage)
{
	struct pthread	*curthread = _get_curthread();
	pid_t	ret;

	_thread_kern_sig_defer();

	/* Perform a non-blocking wait4 syscall: */
	while ((ret = __sys_wait4(pid, istat, options | WNOHANG, rusage)) == 0 && (options & WNOHANG) == 0) {
		/* Reset the interrupted operation flag: */
		curthread->interrupted = 0;

		/* Schedule the next thread while this one waits: */
		_thread_kern_sched_state(PS_WAIT_WAIT, __FILE__, __LINE__);

		/* Check if this call was interrupted by a signal: */
		if (curthread->interrupted) {
			errno = EINTR;
			ret = -1;
			break;
		}
	}

	_thread_kern_sig_undefer();

	return (ret);
}

pid_t
__wait4(pid_t pid, int *istat, int options, struct rusage *rusage)
{
	pid_t ret;

	_thread_enter_cancellation_point();
	ret = _wait4(pid, istat, options, rusage);
	_thread_leave_cancellation_point();

	return ret;
}
