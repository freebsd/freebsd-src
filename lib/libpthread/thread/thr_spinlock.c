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
 *
 */

#include <sys/types.h>
#include <machine/atomic.h>

#include <libc_private.h>
#include "spinlock.h"
#include "thr_private.h"

/*
 * These are for compatability only.  Spinlocks of this type
 * are deprecated.
 */

void
_spinunlock(spinlock_t *lck)
{
	kse_critical_t crit;

	crit = (kse_critical_t)lck->fname;
	atomic_store_rel_long(&lck->access_lock, 0);
	if (crit != NULL)
		_kse_critical_leave(crit);
}


/*
 * Lock a location for the running thread. Yield to allow other
 * threads to run if this thread is blocked because the lock is
 * not available. Note that this function does not sleep. It
 * assumes that the lock will be available very soon.
 */
void
_spinlock(spinlock_t *lck)
{
	kse_critical_t crit;

	/*
	 * Try to grab the lock and loop if another thread grabs
	 * it before we do.
	 */
	if (_kse_isthreaded())
		crit = _kse_critical_enter();
	else
		crit = NULL;
	while(_atomic_lock(&lck->access_lock)) {
		while (lck->access_lock)
			;
	}
	lck->fname = (char *)crit;
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
	_spinlock(lck);
}
