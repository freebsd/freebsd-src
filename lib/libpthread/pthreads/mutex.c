/* ==== mutex.c ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Mutex functions.
 *
 *  1.00 93/07/19 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <errno.h>

/*
 * Basic mutex functionality

 * This is the basic lock order
 * queue
 * pthread
 * global
 *
 * semaphore functionality is defined in machdep.h
 */

/* ==========================================================================
 * pthread_mutex_init()
 *
 * In this implementation I don't need to allocate memory.
 * ENOMEM, EAGAIN should never be returned. Arch that have
 * weird constraints may need special coding.
 */
int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *mutex_attr)
{
	/* Only check if attr specifies some mutex type other than fast */
	if ((mutex_attr) && (mutex_attr->m_type != MUTEX_TYPE_FAST)) {
		if (mutex_attr->m_type >= MUTEX_TYPE_MAX) {
			return(EINVAL);
		}
		if (mutex->m_flags & MUTEX_FLAGS_INITED) {	
			return(EBUSY);
		}
		mutex->m_type = mutex_attr->m_type;
	} else {
		mutex->m_type = MUTEX_TYPE_FAST;
	}
	/* Set all other paramaters */
	pthread_queue_init(&mutex->m_queue);
	mutex->m_flags 	|= MUTEX_FLAGS_INITED;
	mutex->m_lock 	= SEMAPHORE_CLEAR;
	mutex->m_owner	= NULL;
	return(OK);
}

/* ==========================================================================
 * pthread_mutex_destroy()
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	/* Only check if mutex is of type other than fast */
	switch(mutex->m_type) {
	case MUTEX_TYPE_FAST:
		break;
	case MUTEX_TYPE_STATIC_FAST:
	default:
		return(EINVAL);
		break;
	}

	/* Cleanup mutex, others might want to use it. */
	pthread_queue_init(&mutex->m_queue);
	mutex->m_flags 	|= MUTEX_FLAGS_INITED;
	mutex->m_lock 	= SEMAPHORE_CLEAR;
	mutex->m_owner	= NULL;
	mutex->m_flags	= 0;
	return(OK);
}

/* ==========================================================================
 * pthread_mutex_trylock()
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	semaphore *lock;
	int rval;

	lock = &(mutex->m_lock);
	while (SEMAPHORE_TEST_AND_SET(lock)) {
		pthread_yield();
	}
	
	switch (mutex->m_type) {
	/*
	 * Fast mutexes do not check for any error conditions.
     */
	case MUTEX_TYPE_FAST: 
	case MUTEX_TYPE_STATIC_FAST:
		if (!mutex->m_owner) {
			mutex->m_owner = pthread_run;
			rval = OK;
		} else {
			rval = EBUSY;
		}
		break;
	default:
		rval = EINVAL;
		break;
	}
	SEMAPHORE_RESET(lock);
	return(rval);
}

/* ==========================================================================
 * pthread_mutex_lock()
 */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	semaphore *lock, *plock;
	int rval;

	lock = &(mutex->m_lock);
	while (SEMAPHORE_TEST_AND_SET(lock)) {
		pthread_yield();
	}
	
	switch (mutex->m_type) {
	/*
	 * Fast mutexes do not check for any error conditions.
     */
	case MUTEX_TYPE_FAST: 
	case MUTEX_TYPE_STATIC_FAST:
		if (mutex->m_owner) {
			plock = &(pthread_run->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				 pthread_yield();
			}
			pthread_queue_enq(&mutex->m_queue, pthread_run);
			SEMAPHORE_RESET(lock);

			/* Reschedule will unlock pthread_run */
			reschedule(PS_MUTEX_WAIT);
			return(OK);
		}
		mutex->m_owner = pthread_run;
		rval = OK;
		break;
	default:
		rval = EINVAL;
		break;
	}
	SEMAPHORE_RESET(lock);
	return(rval);
}

/* ==========================================================================
 * pthread_mutex_unlock()
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	struct pthread *pthread;
	semaphore *lock, *plock;
	int rval;

	lock = &(mutex->m_lock);
	while (SEMAPHORE_TEST_AND_SET(lock)) {
		pthread_yield();
	}

	switch (mutex->m_type) {
    /*
     * Fast mutexes do not check for any error conditions.
     */
    case MUTEX_TYPE_FAST:
    case MUTEX_TYPE_STATIC_FAST:
		if (pthread = pthread_queue_get(&mutex->m_queue)) {
			plock = &(pthread->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				 pthread_yield();
			}
			mutex->m_owner = pthread;

			/* Reset pthread state */
			pthread_queue_deq(&mutex->m_queue);
			pthread->state = PS_RUNNING;
			SEMAPHORE_RESET(plock);
		} else {
			mutex->m_owner = NULL;
		}
		rval = OK;
		break;
	default:
		rval = EINVAL;
		break;
	}
	SEMAPHORE_RESET(lock);
	return(rval);
}
