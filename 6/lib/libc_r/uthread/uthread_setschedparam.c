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
#include "pthread_private.h"

__weak_reference(_pthread_setschedparam, pthread_setschedparam);

int
_pthread_setschedparam(pthread_t pthread, int policy, 
	const struct sched_param *param)
{
	int old_prio, in_readyq = 0, ret = 0;

	if ((param == NULL) || (policy < SCHED_FIFO) || (policy > SCHED_RR)) {
		/* Return an invalid argument error: */
		ret = EINVAL;
	} else if ((param->sched_priority < PTHREAD_MIN_PRIORITY) ||
	    (param->sched_priority > PTHREAD_MAX_PRIORITY)) {
		/* Return an unsupported value error. */
		ret = ENOTSUP;

	/* Find the thread in the list of active threads: */
	} else if ((ret = _find_thread(pthread)) == 0) {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		if (param->sched_priority !=
		    PTHREAD_BASE_PRIORITY(pthread->base_priority)) {
			/*
			 * Remove the thread from its current priority
			 * queue before any adjustments are made to its
			 * active priority:
			 */
			old_prio = pthread->active_priority;
			if ((pthread->flags & PTHREAD_FLAGS_IN_PRIOQ) != 0) {
				in_readyq = 1;
				PTHREAD_PRIOQ_REMOVE(pthread);
			}

			/* Set the thread base priority: */
			pthread->base_priority &=
			    (PTHREAD_SIGNAL_PRIORITY | PTHREAD_RT_PRIORITY);
			pthread->base_priority = param->sched_priority;

			/* Recalculate the active priority: */
			pthread->active_priority = MAX(pthread->base_priority,
			    pthread->inherited_priority);

			if (in_readyq) {
				if ((pthread->priority_mutex_count > 0) &&
				    (old_prio > pthread->active_priority)) {
					/*
					 * POSIX states that if the priority is
					 * being lowered, the thread must be
					 * inserted at the head of the queue for
					 * its priority if it owns any priority
					 * protection or inheritence mutexes.
					 */
					PTHREAD_PRIOQ_INSERT_HEAD(pthread);
				}
				else
					PTHREAD_PRIOQ_INSERT_TAIL(pthread);
			}

			/*
			 * Check for any mutex priority adjustments.  This
			 * includes checking for a priority mutex on which
			 * this thread is waiting.
			 */
			_mutex_notify_priochange(pthread);
		}

		/* Set the scheduling policy: */
		pthread->attr.sched_policy = policy;

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}
	return(ret);
}
