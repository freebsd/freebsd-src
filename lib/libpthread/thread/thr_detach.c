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
#include <machine/atomic.h>
#include <errno.h>
#include <pthread.h>
#include "thr_private.h"

__weak_reference(_pthread_detach, pthread_detach);

int
_pthread_detach(pthread_t pthread)
{
	struct pthread *curthread = _get_curthread();
	struct kse_mailbox *kmbx = NULL;
	struct pthread *joiner;
	int rval = 0;

	/* Check for invalid calling parameters: */
	if (pthread == NULL || pthread->magic != THR_MAGIC)
		/* Return an invalid argument error: */
		rval = EINVAL;

	else if ((rval = _thr_ref_add(curthread, pthread,
	    /*include dead*/1)) != 0) {
		/* Return an error: */
	}

	/* Check if the thread is already detached: */
	else if ((pthread->attr.flags & PTHREAD_DETACHED) != 0) {
		/* Return an error: */
		_thr_ref_delete(curthread, pthread);
		rval = EINVAL;
	} else {
		/* Lock the detached thread: */
		THR_SCHED_LOCK(curthread, pthread);

		/* Flag the thread as detached: */
		pthread->attr.flags |= PTHREAD_DETACHED;

		/* Retrieve any joining thread and remove it: */
		joiner = pthread->joiner;
		if ((joiner != NULL) && (joiner->kseg == pthread->kseg)) {
			/*
			 * We already own the scheduler lock for the joiner.
			 * Take advantage of that and make the joiner runnable.
			 */
			if (joiner->join_status.thread == pthread) {
				/*
				 * Set the return value for the woken thread:
				 */
				joiner->join_status.error = ESRCH;
				joiner->join_status.ret = NULL;
				joiner->join_status.thread = NULL;

				kmbx = _thr_setrunnable_unlocked(joiner);
			}
			joiner = NULL;
		}
		THR_SCHED_UNLOCK(curthread, pthread);
		/* See if there is a thread waiting in pthread_join(): */
		if ((joiner != NULL) &&
		    (_thr_ref_add(curthread, joiner, 0) == 0)) {
			/* Lock the joiner before fiddling with it. */
			THR_SCHED_LOCK(curthread, joiner);
			if (joiner->join_status.thread == pthread) {
				/*
				 * Set the return value for the woken thread:
				 */
				joiner->join_status.error = ESRCH;
				joiner->join_status.ret = NULL;
				joiner->join_status.thread = NULL;

				kmbx = _thr_setrunnable_unlocked(joiner);
			}
			THR_SCHED_UNLOCK(curthread, joiner);
			_thr_ref_delete(curthread, joiner);
		}
		_thr_ref_delete(curthread, pthread);
		if (kmbx != NULL)
			kse_wakeup(kmbx);
	}

	/* Return the completion status: */
	return (rval);
}
