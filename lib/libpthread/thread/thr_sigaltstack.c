/*-
 * Copyright (c) 2003 David Xu <davidxu@freebsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <signal.h>
#include "thr_private.h"

__weak_reference(_sigaltstack, sigaltstack);

int
_sigaltstack(stack_t *_ss, stack_t *_oss)
{
	struct pthread *curthread = _get_curthread();
	stack_t ss, oss;
	int oonstack, errsave, ret;
	kse_critical_t crit;

	if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM) {
		crit = _kse_critical_enter();
		ret = __sys_sigaltstack(_ss, _oss);
		errsave = errno;
		/* Get a copy */
		if (ret == 0 && _ss != NULL)
			curthread->sigstk = *_ss;
		_kse_critical_leave(crit);
		errno = errsave;
		return (ret);
	}

	if (_ss)
		ss = *_ss;
	if (_oss)
		oss = *_oss;

	/* Should get and set stack in atomic way */
	crit = _kse_critical_enter();
	oonstack = _thr_sigonstack(&ss);
	if (_oss != NULL) {
		oss = curthread->sigstk;
		oss.ss_flags = (curthread->sigstk.ss_flags & SS_DISABLE)
		    ? SS_DISABLE : ((oonstack) ? SS_ONSTACK : 0);
	}

	if (_ss != NULL) {
		if (oonstack) {
			_kse_critical_leave(crit);
			errno = EPERM;
			return (-1);
		}
		if ((ss.ss_flags & ~SS_DISABLE) != 0) {
			_kse_critical_leave(crit);
			errno = EINVAL;
			return (-1);
		}
		if (!(ss.ss_flags & SS_DISABLE)) {
			if (ss.ss_size < MINSIGSTKSZ) {
				_kse_critical_leave(crit);
				errno = ENOMEM;
				return (-1);
			}
			curthread->sigstk = ss;
		} else {
			curthread->sigstk.ss_flags |= SS_DISABLE;
		}
	}
	_kse_critical_leave(crit);
	if (_oss != NULL)
		*_oss = oss;
	return (0);
}

int
_thr_sigonstack(void *sp)
{
	struct pthread *curthread = _get_curthread();

	return ((curthread->sigstk.ss_flags & SS_DISABLE) == 0 ?
	    (((size_t)sp - (size_t)curthread->sigstk.ss_sp) < curthread->sigstk.ss_size)
	    : 0);
}

