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
 * $FreeBSD$
 *
 */

/*
 * POSIX stdio FILE locking functions. These assume that the locking
 * is only required at FILE structure level, not at file descriptor
 * level too.
 *
 */

#include "namespace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "un-namespace.h"

/*
 * Weak symbols for externally visible functions in this file:
 */
#pragma weak	flockfile=_flockfile
#pragma weak	_flockfile_debug=_flockfile_debug_stub
#pragma weak	ftrylockfile=_ftrylockfile
#pragma weak	funlockfile=_funlockfile

static int	init_lock(FILE *);

/*
 * The FILE lock structure. The FILE *fp is locked when the mutex
 * is locked.
 */
struct	__file_lock {
	pthread_mutex_t	fl_mutex;
	pthread_t	fl_owner;	/* current owner */
	int		fl_count;	/* recursive lock count */
};

/*
 * We need to retain binary compatibility for a while.  So pretend
 * that _lock is part of FILE * even though it is dereferenced off
 * _extra now.  When we stop encoding the size of FILE into binaries
 * this can be changed in stdio.h.  This will reduce the amount of
 * code that has to change in the future (just remove this comment
 * and #define).
 */
#define _lock _extra->_mtlock

/*
 * Allocate and initialize a file lock.
 */
static int
init_lock(FILE *fp)
{
	static pthread_mutex_t init_lock_mutex = PTHREAD_MUTEX_INITIALIZER;
	struct __file_lock *p;
	int	ret;

	if ((p = malloc(sizeof(struct __file_lock))) == NULL)
		ret = -1;
	else {
		p->fl_mutex = PTHREAD_MUTEX_INITIALIZER;
		p->fl_owner = NULL;
		p->fl_count = 0;
		if (pthread_mutex_lock(&init_lock_mutex) != 0) {
			free(p);
			return (-1);
		}
		if (fp->_lock != NULL) {	/* lost the race */
			free(p);
			return (0);
		}
		fp->_lock = p;
		pthread_mutex_unlock(&init_lock_mutex);
		ret = 0;
	}
	return (ret);
}

void
_flockfile(FILE *fp)
{
	pthread_t curthread = _pthread_self();

	/*
	 * Check if this is a real file with a valid lock, creating
	 * the lock if needed:
	 */
	if ((fp->_file >= 0) &&
	    ((fp->_lock != NULL) || (init_lock(fp) == 0))) {
		if (fp->_lock->fl_owner == curthread)
			fp->_lock->fl_count++;
		else {
			/*
			 * Make sure this mutex is treated as a private
			 * internal mutex:
			 */
			_pthread_mutex_lock(&fp->_lock->fl_mutex);
			fp->_lock->fl_owner = curthread;
			fp->_lock->fl_count = 1;
		}
	}
}

/*
 * This can be overriden by the threads library if it is linked in.
 */
void
_flockfile_debug_stub(FILE *fp, char *fname, int lineno)
{
	_flockfile(fp);
}

int
_ftrylockfile(FILE *fp)
{
	pthread_t curthread = _pthread_self();
	int	ret = 0;

	/*
	 * Check if this is a real file with a valid lock, creating
	 * the lock if needed:
	 */
	if (((fp->_lock != NULL) || (init_lock(fp) == 0))) {
		if (fp->_lock->fl_owner == curthread)
			fp->_lock->fl_count++;
		/*
		 * Make sure this mutex is treated as a private
		 * internal mutex:
		 */
		else if (_pthread_mutex_trylock(&fp->_lock->fl_mutex) == 0) {
			fp->_lock->fl_owner = curthread;
			fp->_lock->fl_count = 1;
		}
		else
			ret = -1;
	}
	else
		ret = -1;
	return (ret);
}

void 
_funlockfile(FILE *fp)
{
	pthread_t	curthread = _pthread_self();

	/*
	 * Check if this is a real file with a valid lock owned
	 * by the current thread:
	 */
	if ((fp->_lock != NULL) &&
	    (fp->_lock->fl_owner == curthread)) {
		/*
		 * Check if this thread has locked the FILE
		 * more than once:
		 */
		if (fp->_lock->fl_count > 1)
			/*
			 * Decrement the count of the number of
			 * times the running thread has locked this
			 * file:
			 */
			fp->_lock->fl_count--;
		else {
			/*
			 * The running thread will release the
			 * lock now:
			 */
			fp->_lock->fl_count = 0;
			fp->_lock->fl_owner = NULL;
			_pthread_mutex_unlock(&fp->_lock->fl_mutex);
		}
	}
}
