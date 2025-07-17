/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1997 John Birrell <jb@cimlogic.com.au>.
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
 */

#include "namespace.h"
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_Tthr_kill, _pthread_kill);
__weak_reference(_Tthr_kill, pthread_kill);

int
_Tthr_kill(pthread_t pthread, int sig)
{
	struct pthread *curthread;
	int ret;

	/* Check for invalid signal numbers: */
	if (sig < 0 || sig > _SIG_MAXSIG)
		/* Invalid signal: */
		return (EINVAL);

	curthread = _get_curthread();

	/*
	 * Ensure the thread is in the list of active threads, and the
	 * signal is valid (signal 0 specifies error checking only) and
	 * not being ignored:
	 */
	if (curthread == pthread) {
		if (sig > 0)
			_thr_send_sig(pthread, sig);
		ret = 0;
	} else if ((ret = _thr_find_thread(curthread, pthread,
	    /*include dead*/0)) == 0) {
		if (sig > 0)
			_thr_send_sig(pthread, sig);
		THR_THREAD_UNLOCK(curthread, pthread);
	}

	/* Return the completion status: */
	return (ret);
}
