/*
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
 * $Id: uthread_spinlock.c,v 1.3 1998/06/06 07:27:06 jb Exp $
 *
 */

#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "pthread_private.h"

extern char *__progname;

/*
 * Lock a location for the running thread. Yield to allow other
 * threads to run if this thread is blocked because the lock is
 * not available. Note that this function does not sleep. It
 * assumes that the lock will be available very soon.
 */
void
_spinlock(spinlock_t *lck)
{
	/*
	 * Try to grab the lock and loop if another thread grabs
	 * it before we do.
	 */
	while(_atomic_lock(&lck->access_lock)) {
		/* Give up the time slice: */
		sched_yield();

		/* Check if already locked by the running thread: */
		if (lck->lock_owner == (long) _thread_run)
			return;
	}

	/* The running thread now owns the lock: */
	lck->lock_owner = (long) _thread_run;
}

/*
 * Lock a location for the running thread. Yield to allow other
 * threads to run if this thread is blocked because the lock is
 * not available. Note that this function does not sleep. It
 * assumes that the lock will be available very soon.
 *
 * This function checks if the running thread has already locked the
 * location, warns if this occurs and creates a thread dump before
 * returning.
 */
void
_spinlock_debug(spinlock_t *lck, char *fname, int lineno)
{
	/*
	 * Try to grab the lock and loop if another thread grabs
	 * it before we do.
	 */
	while(_atomic_lock(&lck->access_lock)) {
		/* Give up the time slice: */
		sched_yield();

		/* Check if already locked by the running thread: */
		if (lck->lock_owner == (long) _thread_run) {
			char str[256];
			snprintf(str, sizeof(str), "%s - Warning: Thread %p attempted to lock %p from %s (%d) which it had already locked in %s (%d)\n", __progname, _thread_run, lck, fname, lineno, lck->fname, lck->lineno);
			_thread_sys_write(2,str,strlen(str));

			/* Create a thread dump to help debug this problem: */
			_thread_dump_info();
			return;
		}
	}

	/* The running thread now owns the lock: */
	lck->lock_owner = (long) _thread_run;
	lck->fname = fname;
	lck->lineno = lineno;
}
