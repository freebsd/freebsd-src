/*
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
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
 *	This product includes software developed by Daniel Eischen.
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
#include <sys/types.h>
#include <sys/rtprio.h>
#include <errno.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_pthread_getschedparam, pthread_getschedparam);

int
_pthread_getschedparam(pthread_t pthread, int *policy, 
	struct sched_param *param)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	if (policy == NULL || param == NULL)
		return (EINVAL);

	if (pthread == curthread) {
		/*
		 * Avoid searching the thread list when it is the current
		 * thread.
		 */
		THR_LOCK(curthread);
		*policy = curthread->attr.sched_policy;
		param->sched_priority = curthread->attr.prio;
		THR_UNLOCK(curthread);
		ret = 0;
	}
	/* Find the thread in the list of active threads. */
	else if ((ret = _thr_ref_add(curthread, pthread, /*include dead*/0))
	    == 0) {
		THR_THREAD_LOCK(curthread, pthread);
		*policy = pthread->attr.sched_policy;
		param->sched_priority = pthread->attr.prio;
		THR_THREAD_UNLOCK(curthread, pthread);
		_thr_ref_delete(curthread, pthread);
	}
	return (ret);
}
