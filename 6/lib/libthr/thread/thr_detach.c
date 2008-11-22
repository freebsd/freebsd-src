/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * Copyright (C) 2003 Daniel M. Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <sys/types.h>
#include <errno.h>
#include <pthread.h>

#include "thr_private.h"

__weak_reference(_pthread_detach, pthread_detach);

int
_pthread_detach(pthread_t pthread)
{
	struct pthread *curthread = _get_curthread();
	int rval;

	if (pthread == NULL)
		return (EINVAL);

	THREAD_LIST_LOCK(curthread);
	if ((rval = _thr_find_thread(curthread, pthread,
			/*include dead*/1)) != 0) {
		THREAD_LIST_UNLOCK(curthread);
		return (rval);
	}

	/* Check if the thread is already detached or has a joiner. */
	if ((pthread->tlflags & TLFLAGS_DETACHED) != 0 ||
	    (pthread->joiner != NULL)) {
		THREAD_LIST_UNLOCK(curthread);
		return (EINVAL);
	}

	/* Flag the thread as detached. */
	pthread->tlflags |= TLFLAGS_DETACHED;
	if (pthread->state == PS_DEAD)
		THR_GCLIST_ADD(pthread);
	THREAD_LIST_UNLOCK(curthread);

	return (0);
}
