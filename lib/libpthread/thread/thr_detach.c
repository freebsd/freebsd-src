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

int
pthread_detach(pthread_t * p_pthread)
{
	int             rval = 0;
	int             status;
	pthread_t       next_thread;
	pthread_t       pthread;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	/* Check for invalid calling parameters: */
	if (p_pthread == NULL || (pthread = *p_pthread) == NULL) {
		/* Return an invalid argument error: */
		errno = EINVAL;
		rval = -1;
	}
	/* Check if the thread has not been detached: */
	else if ((pthread->attr.flags & PTHREAD_DETACHED) == 0) {
		/* Flag the thread as detached: */
		pthread->attr.flags |= PTHREAD_DETACHED;

		/* Enter a loop to bring all threads off the join queue: */
		while ((next_thread = _thread_queue_deq(&pthread->join_queue)) != NULL) {
			/* Make the thread run: */
			PTHREAD_NEW_STATE(next_thread,PS_RUNNING);
		}

		/*
		 * NULL the thread pointer now that the thread has been
		 * detached: 
		 */
		*p_pthread = NULL;
	} else {
		/* Return an error: */
		errno = ESRCH;
		rval = -1;
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);

	/* Return the completion status: */
	return (rval);
}
#endif
