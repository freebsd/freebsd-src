/*
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Find a thread in the linked list of active threads: */
int
_find_thread(pthread_t pthread)
{
	pthread_t pthread1;

	/* Check if the caller has specified an invalid thread: */
	if (pthread == NULL || pthread->magic != PTHREAD_MAGIC)
		/* Invalid thread: */
		return(EINVAL);

	/* Lock the thread list: */
	_lock_thread_list();

	/* Point to the first thread in the list: */
	pthread1 = _thread_link_list;

	/* Search for the thread to join to: */
	while (pthread1 != NULL && pthread1 != pthread) {
		/* Point to the next thread: */
		pthread1 = pthread1->nxt;
	}

	/* Unlock the thread list: */
	_unlock_thread_list();

	/* Return zero if the thread exists: */
	return ((pthread1 != NULL) ? 0:ESRCH);
}

/* Find a thread in the linked list of dead threads: */
int
_find_dead_thread(pthread_t pthread)
{
	pthread_t pthread1;

	/* Check if the caller has specified an invalid thread: */
	if (pthread == NULL || pthread->magic != PTHREAD_MAGIC)
		/* Invalid thread: */
		return(EINVAL);

	/*
	 * Lock the garbage collector mutex to ensure that the garbage
	 * collector is not using the dead thread list.
	 */
	if (pthread_mutex_lock(&_gc_mutex) != 0)
		PANIC("Cannot lock gc mutex");

	/* Point to the first thread in the list: */
	pthread1 = _thread_dead;

	/* Search for the thread to join to: */
	while (pthread1 != NULL && pthread1 != pthread) {
		/* Point to the next thread: */
		pthread1 = pthread1->nxt_dead;
	}

	/* Unlock the garbage collector mutex: */
	if (pthread_mutex_unlock(&_gc_mutex) != 0)
		PANIC("Cannot lock gc mutex");

	/* Return zero if the thread exists: */
	return ((pthread1 != NULL) ? 0:ESRCH);
}
#endif
