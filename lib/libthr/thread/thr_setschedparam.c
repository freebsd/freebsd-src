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
#include <errno.h>
#include <sys/param.h>
#include <pthread.h>
#include <stdlib.h>
#include "thr_private.h"

__weak_reference(_pthread_getschedparam, pthread_getschedparam);
__weak_reference(_pthread_setschedparam, pthread_setschedparam);

int
_pthread_getschedparam(pthread_t pthread, int *policy, 
    struct sched_param *param)
{
	if (param == NULL || policy == NULL)
		return (EINVAL);
	if (_find_thread(pthread) == ESRCH)
		return (ESRCH);
	param->sched_priority = pthread->base_priority;
	*policy = pthread->attr.sched_policy;
	return(0);
}

int
_pthread_setschedparam(pthread_t pthread, int policy, 
	const struct sched_param *param)
{
	struct pthread_mutex *mtx;
	int old_prio;

	mtx = NULL;
	old_prio = 0;
	if ((param == NULL) || (policy < SCHED_FIFO) || (policy > SCHED_RR))
		return (EINVAL);
	if ((param->sched_priority < PTHREAD_MIN_PRIORITY) ||
	    (param->sched_priority > PTHREAD_MAX_PRIORITY))
		return (ENOTSUP);
	if (_find_thread(pthread) != 0)
		return (ESRCH);

	/*
	 * If the pthread is waiting on a mutex grab it now. Doing it now
	 * even though we do not need it immediately greatly simplifies the
	 * LOR avoidance code.
	 */
	do {
		PTHREAD_LOCK(pthread);
		if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0) {
			mtx = pthread->data.mutex;
			if (_spintrylock(&mtx->lock) == EBUSY)
				PTHREAD_UNLOCK(pthread);
			else
				break;
		} else {
			mtx = NULL;
			break;
		}
	} while (1);

	PTHREAD_ASSERT(pthread->active_priority >= pthread->inherited_priority,
	    "active priority cannot be less than inherited priority");
	old_prio = pthread->base_priority;
	pthread->base_priority = param->sched_priority;
	if (param->sched_priority <= pthread->active_priority) {
		/*
		 * Active priority is affected only if it was the
		 * base priority and the new base priority is lower.
		 */
		if (pthread->active_priority == old_prio &&
		    pthread->active_priority != pthread->inherited_priority) {
			pthread->active_priority = param->sched_priority;
			readjust_priorities(pthread, mtx);
		}

	} else {
		/*
		 * New base priority is greater than active priority. This
		 * only affects threads that are holding priority inheritance
		 * mutexes this thread is waiting on and its position in the
		 * queue.
		 */
		pthread->active_priority = param->sched_priority;
		readjust_priorities(pthread, mtx);

	}
	pthread->attr.sched_policy = policy;
	PTHREAD_UNLOCK(pthread);
	if (mtx != NULL)
		_SPINUNLOCK(&mtx->lock);
	return(0);
}
