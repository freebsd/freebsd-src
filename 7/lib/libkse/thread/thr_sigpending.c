/*
 * Copyright (c) 1999 Daniel Eischen <eischen@vigrid.com>.
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
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

int	_sigpending(sigset_t *set);

LT10_COMPAT_PRIVATE(_sigpending);
LT10_COMPAT_DEFAULT(sigpending);

__weak_reference(_sigpending, sigpending);

int
_sigpending(sigset_t *set)
{
	struct pthread *curthread = _get_curthread();
	kse_critical_t crit;
	sigset_t sigset;
	int ret = 0;

	/* Check for a null signal set pointer: */
	if (set == NULL) {
		/* Return an invalid argument: */
		ret = EINVAL;
	}
	else {
		if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
			return (__sys_sigpending(set));

		crit = _kse_critical_enter();
		KSE_SCHED_LOCK(curthread->kse, curthread->kseg);
		sigset = curthread->sigpend;
		KSE_SCHED_UNLOCK(curthread->kse, curthread->kseg);
		KSE_LOCK_ACQUIRE(curthread->kse, &_thread_signal_lock);
		SIGSETOR(sigset, _thr_proc_sigpending);
		KSE_LOCK_RELEASE(curthread->kse, &_thread_signal_lock);
		_kse_critical_leave(crit);
		*set = sigset;
	}
	/* Return the completion status: */
	return (ret);
}
