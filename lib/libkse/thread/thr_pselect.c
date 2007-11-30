/*-
 * Copyright (c) 2002 Daniel M. Eischen <deischen@freebsd.org>
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

extern int	__pselect(int count, fd_set *rfds, fd_set *wfds, fd_set *efds, 
		    const struct timespec *timo, const sigset_t *mask);

int 		_pselect(int count, fd_set *rfds, fd_set *wfds, fd_set *efds, 
		    const struct timespec *timo, const sigset_t *mask);

LT10_COMPAT_PRIVATE(_pselect);
LT10_COMPAT_DEFAULT(pselect);

__weak_reference(_pselect, pselect);

int 
_pselect(int count, fd_set *rfds, fd_set *wfds, fd_set *efds, 
	const struct timespec *timo, const sigset_t *mask)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	_thr_cancel_enter(curthread);
	ret = __pselect(count, rfds, wfds, efds, timo, mask);
	_thr_cancel_leave(curthread, 1);

	return (ret);
}
