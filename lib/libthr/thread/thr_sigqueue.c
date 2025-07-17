/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1997 John Birrell <jb@cimlogic.com.au>.
 * All rights reserved.
 *
 * Copyright 2024 The FreeBSD Foundation
 *
 * Portions of this software were developed by Konstantin Belousov
 * <kib@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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
 */

#include "namespace.h"
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_Tthr_sigqueue, _pthread_sigqueue);
__weak_reference(_Tthr_sigqueue, pthread_sigqueue);

int
_Tthr_sigqueue(pthread_t pthread, int sig, const union sigval value)
{
	struct pthread *curthread;
	int e, ret;

	if (sig < 0 || sig > _SIG_MAXSIG)
		return (EINVAL);

	curthread = _get_curthread();
	ret = 0;

	if (curthread == pthread) {
		if (sig > 0) {
			e = sigqueue(pthread->tid, sig | __SIGQUEUE_TID,
			    value);
			if (e == -1)
				ret = errno;
		}
	} else if ((ret = _thr_find_thread(curthread, pthread,
	    0)) == 0) {
		if (sig > 0) {
			e = sigqueue(pthread->tid, sig | __SIGQUEUE_TID,
			    value);
			if (e == -1)
				ret = errno;
		}
		THR_THREAD_UNLOCK(curthread, pthread);
	}

	return (ret);
}
