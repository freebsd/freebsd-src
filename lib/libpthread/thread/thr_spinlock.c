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

#define	MAX_SPINLOCKS	5

struct spinlock_extra {
	spinlock_t	*owner;
	pthread_mutex_t	lock;
};

static void	init_spinlock(spinlock_t *lck);

static pthread_mutex_t		spinlock_static_lock;
static struct spinlock_extra	extra[MAX_SPINLOCKS];
static int			spinlock_count = 0;
static int			initialized = 0;

/*
 * These are for compatability only.  Spinlocks of this type
 * are deprecated.
 */

void
_spinunlock(spinlock_t *lck)
{
	struct spinlock_extra *extra;

	extra = (struct spinlock_extra *)lck->fname;
	pthread_mutex_unlock(&extra->lock);
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
	struct spinlock_extra *extra;

	if (!__isthreaded)
		PANIC("Spinlock called when not threaded.");
	if (!initialized)
		PANIC("Spinlocks not initialized.");
	/*
	 * Try to grab the lock and loop if another thread grabs
	 * it before we do.
	 */
	if (lck->fname == NULL)
		init_spinlock(lck);
	extra = (struct spinlock_extra *)lck->fname;
	pthread_mutex_lock(&extra->lock);
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

static void
init_spinlock(spinlock_t *lck)
{
	pthread_mutex_lock(&spinlock_static_lock);
	if ((lck->fname == NULL) && (spinlock_count < MAX_SPINLOCKS)) {
		lck->fname = (char *)&extra[spinlock_count];
		extra[spinlock_count].owner = lck;
		spinlock_count++;
	}
	pthread_mutex_unlock(&spinlock_static_lock);
	if (lck->fname == NULL)
		PANIC("Exceeded max spinlocks");
}

void
_thr_spinlock_init(void)
{
	int i;

	if (initialized != 0) {
		_thr_mutex_reinit(&spinlock_static_lock);
		for (i = 0; i < spinlock_count; i++)
			_thr_mutex_reinit(&extra[i].lock);
	} else {
		if (pthread_mutex_init(&spinlock_static_lock, NULL))
			PANIC("Cannot initialize spinlock_static_lock");
		for (i = 0; i < MAX_SPINLOCKS; i++) {
			if (pthread_mutex_init(&extra[i].lock, NULL))
				PANIC("Cannot initialize spinlock extra");
		}
		initialized = 1;
	}
}
