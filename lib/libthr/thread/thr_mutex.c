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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <pthread.h>
#include "thr_private.h"

/*
 * Prototypes
 */
static spinlock_t static_init_lock = _SPINLOCK_INITIALIZER;

static struct pthread_mutex_attr	static_mutex_attr =
    PTHREAD_MUTEXATTR_STATIC_INITIALIZER;
static pthread_mutexattr_t		static_mattr = &static_mutex_attr;

/* Single underscore versions provided for libc internal usage: */
__weak_reference(__pthread_mutex_trylock, pthread_mutex_trylock);
__weak_reference(__pthread_mutex_lock, pthread_mutex_lock);

/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_init, pthread_mutex_init);
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_unlock, pthread_mutex_unlock);


/* Reinitialize a mutex to defaults. */
int
_mutex_reinit(pthread_mutex_t * mutex)
{
	int	ret = 0;

	if (mutex == NULL)
		return (EINVAL);
	if (*mutex == NULL)
		return (pthread_mutex_init(mutex, NULL));

	(*mutex)->m_attr.m_type = PTHREAD_MUTEX_DEFAULT;
	(*mutex)->m_attr.m_protocol = PTHREAD_PRIO_NONE;
	(*mutex)->m_attr.m_ceiling = 0;
	(*mutex)->m_attr.m_flags &= MUTEX_FLAGS_PRIVATE;
	(*mutex)->m_attr.m_flags |= MUTEX_FLAGS_INITED;
	bzero(&(*mutex)->m_mtx, sizeof(struct umtx));
	(*mutex)->m_owner = NULL;
	(*mutex)->m_count = 0;
	(*mutex)->m_refcount = 0;

	return (0);
}

int
_pthread_mutex_init(pthread_mutex_t * mutex,
		   const pthread_mutexattr_t * mutex_attr)
{
	enum pthread_mutextype	type;
	pthread_mutex_t	pmutex;

	if (mutex == NULL)
		return (EINVAL);

	/*
	 * Allocate mutex.
	 */
	pmutex = (pthread_mutex_t)calloc(1, sizeof(struct pthread_mutex));
	if (pmutex == NULL)
		return (ENOMEM);

	bzero(pmutex, sizeof(*pmutex));

	/* Set mutex attributes. */
	if (mutex_attr == NULL || *mutex_attr == NULL) {
		/* Default to a (error checking) POSIX mutex. */
		pmutex->m_attr.m_type = PTHREAD_MUTEX_ERRORCHECK;
		pmutex->m_attr.m_protocol = PTHREAD_PRIO_NONE;
		pmutex->m_attr.m_ceiling = 0;
		pmutex->m_attr.m_flags = 0;
	} else
		bcopy(*mutex_attr, &pmutex->m_attr, sizeof(mutex_attr));

	/*
	 * Sanity check mutex type.
	 */
	if ((pmutex->m_attr.m_type < PTHREAD_MUTEX_ERRORCHECK) ||
	    (pmutex->m_attr.m_type >= MUTEX_TYPE_MAX) ||
	    (pmutex->m_attr.m_protocol < PTHREAD_PRIO_NONE) ||
	    (pmutex->m_attr.m_protocol > PTHREAD_MUTEX_RECURSIVE))
		goto err;

	
	/*
	 * Initialize mutex.
	 */
	pmutex->m_attr.m_flags |= MUTEX_FLAGS_INITED;
	*mutex = pmutex;

	return (0);
err:
	free(pmutex);
	return (EINVAL);
}

int
_pthread_mutex_destroy(pthread_mutex_t * mutex)
{
	int	ret = 0;

	if (mutex == NULL || *mutex == NULL)
		return (EINVAL);

	/* Ensure that the mutex is unlocked. */
	if (((*mutex)->m_owner != NULL) ||
	    ((*mutex)->m_refcount != 0))
		return (EBUSY);


	/* Free it. */
	free(*mutex);
	*mutex = NULL;
	return (0);
}

static int
init_static(pthread_mutex_t *mutex)
{
	pthread_t curthread;
	int ret;

	curthread = _get_curthread();
	GIANT_LOCK(curthread);
	if (*mutex == NULL)
		ret = pthread_mutex_init(mutex, NULL);
	else
		ret = 0;
	GIANT_UNLOCK(curthread);
	return (ret);
}

static int
init_static_private(pthread_mutex_t *mutex)
{
	pthread_t curthread;
	int	ret;

	curthread = _get_curthread();
	GIANT_LOCK(curthread);
	if (*mutex == NULL)
		ret = pthread_mutex_init(mutex, &static_mattr);
	else
		ret = 0;
	GIANT_UNLOCK(curthread);
	return(ret);
}

static int
mutex_trylock_common(pthread_mutex_t *mutex)
{
	struct pthread	*curthread = _get_curthread();
	int error;

	PTHREAD_ASSERT((mutex != NULL) && (*mutex != NULL),
	    "Uninitialized mutex in pthread_mutex_trylock_common");
	
	/*
	 * Attempt to obtain the lock.
	 */
	if ((error = umtx_trylock(&(*mutex)->m_mtx, curthread->thr_id)) == 0) {
		(*mutex)->m_owner = curthread;
		TAILQ_INSERT_TAIL(&curthread->mutexq, *mutex, m_qe);

		return (0);
	}
	/* The lock was invalid. */
	if (error != EBUSY)
		abort();

	if ((*mutex)->m_owner == curthread) {
		if ((*mutex)->m_attr.m_type == PTHREAD_MUTEX_RECURSIVE) {
			(*mutex)->m_count++;
			return (0);
		} else
			return (EDEADLK);
	}

	return (error);
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	int	ret;

	if (mutex == NULL)
		return (EINVAL);

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if ((*mutex == NULL) && (ret = init_static(mutex)) != 0)
		return (ret);

	
	return (mutex_trylock_common(mutex));
}

int
_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	int	ret;

	if (mutex == NULL)
		return (EINVAL);

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking the mutex private (delete safe):
	 */
	if ((*mutex == NULL) && (ret = init_static_private(mutex)) != 0)
		return (ret);

	return (mutex_trylock_common(mutex));
}

static int
mutex_lock_common(pthread_mutex_t * mutex)
{
	struct pthread	*curthread = _get_curthread();
	int giant_count;
	int error;

	PTHREAD_ASSERT((mutex != NULL) && (*mutex != NULL),
	    "Uninitialized mutex in pthread_mutex_trylock_common");
	
	/*
	 * Obtain the lock.
	 */
	if ((error = umtx_trylock(&(*mutex)->m_mtx, curthread->thr_id)) == 0) {
		(*mutex)->m_owner = curthread;
		TAILQ_INSERT_TAIL(&curthread->mutexq, *mutex, m_qe);

		return (0);
	}
	/* The lock was invalid. */
	if (error != EBUSY)
		abort();

	if ((*mutex)->m_owner == curthread) {
		if ((*mutex)->m_attr.m_type == PTHREAD_MUTEX_RECURSIVE) {
			(*mutex)->m_count++;

			return (0);
		} else
			return (EDEADLK);
	}

	/*
	 * Lock Giant so we can save the recursion count and set our
	 * state.  Then we'll call into the kernel to block on this mutex.
	 */

	GIANT_LOCK(curthread);
	PTHREAD_SET_STATE(curthread, PS_MUTEX_WAIT);
	if (_giant_count != 1)
		abort();
	giant_count = _giant_count;

	/*
	 * This will unwind all references.
	 */
        _giant_count = 1;
	GIANT_UNLOCK(curthread);

	if ((error = umtx_lock(&(*mutex)->m_mtx, curthread->thr_id)) == 0) {
		(*mutex)->m_owner = curthread;
		TAILQ_INSERT_TAIL(&curthread->mutexq, *mutex, m_qe);
	} else
		_thread_printf(0, "umtx_lock(%d)\n", error);

	/*
 	 * Set our state and restore our recursion count.
	 */
	GIANT_LOCK(curthread);
	PTHREAD_SET_STATE(curthread, PS_RUNNING);

	giant_count = _giant_count;
	GIANT_UNLOCK(curthread);

	return (error);
}

int
__pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int	ret;

	if (_thread_initial == NULL)
		_thread_init();

	if (mutex == NULL)
		return (EINVAL);

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if ((*mutex == NULL) && ((ret = init_static(mutex)) != 0))
		return (ret);

	return (mutex_lock_common(mutex));
}

int
_pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	if (_thread_initial == NULL)
		_thread_init();

	if (mutex == NULL)
		return (EINVAL);

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	if ((*mutex == NULL) && ((ret = init_static_private(mutex)) != 0))
		return (ret);

	return (mutex_lock_common(mutex));
}


int
_mutex_cv_unlock(pthread_mutex_t * mutex)
{
	int ret;

	if ((ret = pthread_mutex_unlock(mutex)) == 0)
		(*mutex)->m_refcount++;

	return (ret);
}

int
_mutex_cv_lock(pthread_mutex_t * mutex)
{
	int	ret;


	if ((ret = pthread_mutex_lock(mutex)) == 0)
		(*mutex)->m_refcount--;

	return (ret);
}

int
_pthread_mutex_unlock(pthread_mutex_t * mutex)
{
	struct pthread	*curthread = _get_curthread();
	thr_id_t sav;
	int	ret = 0;

	if (mutex == NULL || *mutex == NULL)
		return (EINVAL);

	if ((*mutex)->m_owner != curthread)
		return (EPERM);

	if ((*mutex)->m_count != 0) {
		(*mutex)->m_count--;
		return (0);
	}

	TAILQ_REMOVE(&curthread->mutexq, *mutex, m_qe);
	(*mutex)->m_owner = NULL;

	sav = (*mutex)->m_mtx.u_owner;
	ret = umtx_unlock(&(*mutex)->m_mtx, curthread->thr_id);
	if (ret) {
		_thread_printf(0, "umtx_unlock(%d)", ret);
		_thread_printf(0, "%x : %x : %x\n", curthread, (*mutex)->m_mtx.u_owner, sav);
	}

	return (ret);
}

void
_mutex_unlock_private(pthread_t pthread)
{
	struct pthread_mutex	*m, *m_next;

	for (m = TAILQ_FIRST(&pthread->mutexq); m != NULL; m = m_next) {
		m_next = TAILQ_NEXT(m, m_qe);
		if ((m->m_attr.m_flags & MUTEX_FLAGS_PRIVATE) != 0)
			pthread_mutex_unlock(&m);
	}
}
