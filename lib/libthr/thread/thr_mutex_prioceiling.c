/*
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
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
 *	This product includes software developed by Daniel Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include "thr_private.h"

__weak_reference(_pthread_mutexattr_getprioceiling, pthread_mutexattr_getprioceiling);
__weak_reference(_pthread_mutexattr_setprioceiling, pthread_mutexattr_setprioceiling);
__weak_reference(_pthread_mutex_getprioceiling, pthread_mutex_getprioceiling);
__weak_reference(_pthread_mutex_setprioceiling, pthread_mutex_setprioceiling);

int
_pthread_mutexattr_getprioceiling(pthread_mutexattr_t *mattr, int *prioceiling)
{
	int ret = 0;

	if (*mattr == NULL)
		ret = EINVAL;
	else
		*prioceiling = (*mattr)->m_ceiling;

	return(ret);
}

int
_pthread_mutexattr_setprioceiling(pthread_mutexattr_t *mattr, int prioceiling)
{
	int ret = 0;

	if (*mattr == NULL)
		ret = EINVAL;
	else if (prioceiling <= PTHREAD_MAX_PRIORITY &&
	    prioceiling >= PTHREAD_MIN_PRIORITY)
		(*mattr)->m_ceiling = prioceiling;
	else
		ret = EINVAL;

	return (ret);
}

int
_pthread_mutex_getprioceiling(pthread_mutex_t *mutex,
			      int *prioceiling)
{
	if (*mutex == NULL)
		return (EINVAL);
	else
		*prioceiling = (*mutex)->m_prio;
	return (0);
}

int
_pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
			      int prioceiling, int *old_ceiling)
{
	int ret = 0;

	if (*mutex == NULL)
		return (EINVAL);
	else if (prioceiling > PTHREAD_MAX_PRIORITY ||
	    prioceiling < PTHREAD_MIN_PRIORITY)
		return (EINVAL);

	/*
	 * Because of the use of pthread_mutex_unlock(), the
	 * priority ceiling of a mutex cannot be changed
	 * while the mutex is held by another thread. It also,
	 * means that the the thread trying to change the
	 * priority ceiling must adhere to prio protection rules.
	 */
	if ((ret = pthread_mutex_lock(mutex)) == 0) {
		/* Return the old ceiling and set the new ceiling: */
		*old_ceiling = (*mutex)->m_prio;
		(*mutex)->m_prio = prioceiling;

		/* Unlock the mutex: */
		ret = pthread_mutex_unlock(mutex);
	}
	return(ret);
}
