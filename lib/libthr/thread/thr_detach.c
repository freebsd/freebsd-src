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
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include "thr_private.h"

__weak_reference(_pthread_detach, pthread_detach);

int
_pthread_detach(pthread_t pthread)
{
	int error;

	if (pthread->magic != PTHREAD_MAGIC)
		return (EINVAL);

	PTHREAD_LOCK(pthread);

	if ((pthread->attr.flags & PTHREAD_DETACHED) != 0) {
		_thread_sigblock();
		DEAD_LIST_LOCK;
		error = pthread->isdead ? ESRCH : EINVAL;
		DEAD_LIST_UNLOCK;
		_thread_sigunblock();
		PTHREAD_UNLOCK(pthread);
		return (error);
	}

	pthread->attr.flags |= PTHREAD_DETACHED;

	/* Check if there is a joiner: */
	if (pthread->joiner != NULL) {
		struct pthread	*joiner = pthread->joiner;

		/* Set the return value for the woken thread: */
		joiner->join_status.error = ESRCH;
		joiner->join_status.ret = NULL;
		joiner->join_status.thread = NULL;

		/*
		 * Disconnect the joiner from the thread being detached:
		 */
		pthread->joiner = NULL;
		PTHREAD_WAKE(joiner);
	}

	PTHREAD_UNLOCK(pthread);

	return (0);
}
