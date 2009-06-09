/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
#include <sys/types.h>
#include <sys/signalvar.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include "un-namespace.h"
#include "thr_private.h"

int	__sigsuspend(const sigset_t * set);

LT10_COMPAT_PRIVATE(__sigsuspend);
LT10_COMPAT_PRIVATE(_sigsuspend);
LT10_COMPAT_DEFAULT(sigsuspend);

__weak_reference(__sigsuspend, sigsuspend);

int
_sigsuspend(const sigset_t *set)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t	oldmask, newmask, tempset;
	int             ret = -1;

	if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
		return (__sys_sigsuspend(set));

	/* Check if a new signal set was provided by the caller: */
	if (set != NULL) {
		newmask = *set;
		SIG_CANTMASK(newmask);
		THR_LOCK_SWITCH(curthread);

		/* Save current sigmask: */
		oldmask = curthread->sigmask;
		curthread->oldsigmask = &oldmask;

		/* Change the caller's mask: */
		curthread->sigmask = newmask;
		tempset = curthread->sigpend;
		SIGSETNAND(tempset, newmask);
		if (SIGISEMPTY(tempset)) {
			THR_SET_STATE(curthread, PS_SIGSUSPEND);
			/* Wait for a signal: */
			_thr_sched_switch_unlocked(curthread);
		} else {
			curthread->check_pending = 1;
			THR_UNLOCK_SWITCH(curthread);
			/* check pending signal I can handle: */
			_thr_sig_check_pending(curthread);
		}
		if ((curthread->cancelflags & THR_CANCELLING) != 0)
			curthread->oldsigmask = NULL;
		else {
			THR_ASSERT(curthread->oldsigmask == NULL,
		 	          "oldsigmask is not cleared");
		}

		/* Always return an interrupted error: */
		errno = EINTR;
	} else {
		/* Return an invalid argument error: */
		errno = EINVAL;
	}

	/* Return the completion status: */
	return (ret);
}

int
__sigsuspend(const sigset_t * set)
{
	struct pthread *curthread = _get_curthread();
	int		ret;

	_thr_cancel_enter(curthread);
	ret = _sigsuspend(set);
	_thr_cancel_leave(curthread, 1);

	return (ret);
}
