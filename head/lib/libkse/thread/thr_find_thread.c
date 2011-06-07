/*
 * Copyright (c) 2003 Daniel Eischen <deischen@freebsd.org>
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>.
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
#include <errno.h>
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

/*
 * Find a thread in the linked list of active threads and add a reference
 * to it.  Threads with positive reference counts will not be deallocated
 * until all references are released.
 */
int
_thr_ref_add(struct pthread *curthread, struct pthread *thread,
    int include_dead)
{
	kse_critical_t crit;
	struct pthread *pthread;
	struct kse *curkse;

	if (thread == NULL)
		/* Invalid thread: */
		return (EINVAL);

	crit = _kse_critical_enter();
	curkse = _get_curkse();
	KSE_LOCK_ACQUIRE(curkse, &_thread_list_lock);
	pthread = _thr_hash_find(thread);
	if (pthread) {
		if ((include_dead == 0) &&
		    ((pthread->state == PS_DEAD) ||
		    ((pthread->state == PS_DEADLOCK) ||
		    ((pthread->flags & THR_FLAGS_EXITING) != 0))))
			pthread = NULL;
		else {
			pthread->refcount++;
			if (curthread != NULL)
				curthread->critical_count++;
		}
	}
	KSE_LOCK_RELEASE(curkse, &_thread_list_lock);
	_kse_critical_leave(crit);

	/* Return zero if the thread exists: */
	return ((pthread != NULL) ? 0 : ESRCH);
}

void
_thr_ref_delete(struct pthread *curthread, struct pthread *thread)
{
	kse_critical_t crit;
	struct kse *curkse;

	if (thread != NULL) {
		crit = _kse_critical_enter();
		curkse = _get_curkse();
		KSE_LOCK_ACQUIRE(curkse, &_thread_list_lock);
		thread->refcount--;
		if (curthread != NULL)
			curthread->critical_count--;
		if ((thread->refcount == 0) &&
		    (thread->tlflags & TLFLAGS_GC_SAFE) != 0)
			THR_GCLIST_ADD(thread);
		KSE_LOCK_RELEASE(curkse, &_thread_list_lock);
		_kse_critical_leave(crit);
	}
}
