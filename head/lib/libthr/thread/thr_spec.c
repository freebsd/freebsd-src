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
 * 3. Neither the name of the author nor the names of any co-contributors
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

#include "namespace.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

/* Static variables: */
struct pthread_key _thread_keytable[PTHREAD_KEYS_MAX];

__weak_reference(_pthread_key_create, pthread_key_create);
__weak_reference(_pthread_key_delete, pthread_key_delete);
__weak_reference(_pthread_getspecific, pthread_getspecific);
__weak_reference(_pthread_setspecific, pthread_setspecific);


int
_pthread_key_create(pthread_key_t *key, void (*destructor) (void *))
{
	struct pthread *curthread;
	int i;

	_thr_check_init();

	curthread = _get_curthread();

	/* Lock the key table: */
	THR_LOCK_ACQUIRE(curthread, &_keytable_lock);
	for (i = 0; i < PTHREAD_KEYS_MAX; i++) {

		if (_thread_keytable[i].allocated == 0) {
			_thread_keytable[i].allocated = 1;
			_thread_keytable[i].destructor = destructor;
			_thread_keytable[i].seqno++;

			/* Unlock the key table: */
			THR_LOCK_RELEASE(curthread, &_keytable_lock);
			*key = i;
			return (0);
		}

	}
	/* Unlock the key table: */
	THR_LOCK_RELEASE(curthread, &_keytable_lock);
	return (EAGAIN);
}

int
_pthread_key_delete(pthread_key_t key)
{
	struct pthread *curthread = _get_curthread();
	int ret = 0;

	if ((unsigned int)key < PTHREAD_KEYS_MAX) {
		/* Lock the key table: */
		THR_LOCK_ACQUIRE(curthread, &_keytable_lock);

		if (_thread_keytable[key].allocated)
			_thread_keytable[key].allocated = 0;
		else
			ret = EINVAL;

		/* Unlock the key table: */
		THR_LOCK_RELEASE(curthread, &_keytable_lock);
	} else
		ret = EINVAL;
	return (ret);
}

void 
_thread_cleanupspecific(void)
{
	struct pthread	*curthread = _get_curthread();
	void		(*destructor)( void *);
	const void	*data = NULL;
	int		key;
	int		i;

	if (curthread->specific == NULL)
		return;

	/* Lock the key table: */
	THR_LOCK_ACQUIRE(curthread, &_keytable_lock);
	for (i = 0; (i < PTHREAD_DESTRUCTOR_ITERATIONS) &&
	    (curthread->specific_data_count > 0); i++) {
		for (key = 0; (key < PTHREAD_KEYS_MAX) &&
		    (curthread->specific_data_count > 0); key++) {
			destructor = NULL;

			if (_thread_keytable[key].allocated &&
			    (curthread->specific[key].data != NULL)) {
				if (curthread->specific[key].seqno ==
				    _thread_keytable[key].seqno) {
					data = curthread->specific[key].data;
					destructor = _thread_keytable[key].destructor;
				}
				curthread->specific[key].data = NULL;
				curthread->specific_data_count--;
			}
			else if (curthread->specific[key].data != NULL) {
				/* 
				 * This can happen if the key is deleted via
				 * pthread_key_delete without first setting the value
				 * to NULL in all threads.  POSIX says that the
				 * destructor is not invoked in this case.
				 */
				curthread->specific[key].data = NULL;
				curthread->specific_data_count--;
			}

			/*
			 * If there is a destructor, call it
			 * with the key table entry unlocked:
			 */
			if (destructor != NULL) {
				/*
				 * Don't hold the lock while calling the
				 * destructor:
				 */
				THR_LOCK_RELEASE(curthread, &_keytable_lock);
				destructor(__DECONST(void *, data));
				THR_LOCK_ACQUIRE(curthread, &_keytable_lock);
			}
		}
	}
	THR_LOCK_RELEASE(curthread, &_keytable_lock);
	free(curthread->specific);
	curthread->specific = NULL;
	if (curthread->specific_data_count > 0)
		stderr_debug("Thread %p has exited with leftover "
		    "thread-specific data after %d destructor iterations\n",
		    curthread, PTHREAD_DESTRUCTOR_ITERATIONS);
}

static inline struct pthread_specific_elem *
pthread_key_allocate_data(void)
{
	struct pthread_specific_elem *new_data;

	new_data = (struct pthread_specific_elem *)
	    calloc(1, sizeof(struct pthread_specific_elem) * PTHREAD_KEYS_MAX);
	return (new_data);
}

int 
_pthread_setspecific(pthread_key_t key, const void *value)
{
	struct pthread	*pthread;
	int		ret = 0;

	/* Point to the running thread: */
	pthread = _get_curthread();

	if ((pthread->specific) ||
	    (pthread->specific = pthread_key_allocate_data())) {
		if ((unsigned int)key < PTHREAD_KEYS_MAX) {
			if (_thread_keytable[key].allocated) {
				if (pthread->specific[key].data == NULL) {
					if (value != NULL)
						pthread->specific_data_count++;
				} else if (value == NULL)
					pthread->specific_data_count--;
				pthread->specific[key].data = value;
				pthread->specific[key].seqno =
				    _thread_keytable[key].seqno;
				ret = 0;
			} else
				ret = EINVAL;
		} else
			ret = EINVAL;
	} else
		ret = ENOMEM;
	return (ret);
}

void *
_pthread_getspecific(pthread_key_t key)
{
	struct pthread	*pthread;
	const void	*data;

	/* Point to the running thread: */
	pthread = _get_curthread();

	/* Check if there is specific data: */
	if (pthread->specific != NULL && (unsigned int)key < PTHREAD_KEYS_MAX) {
		/* Check if this key has been used before: */
		if (_thread_keytable[key].allocated &&
		    (pthread->specific[key].seqno == _thread_keytable[key].seqno)) {
			/* Return the value: */
			data = pthread->specific[key].data;
		} else {
			/*
			 * This key has not been used before, so return NULL
			 * instead: 
			 */
			data = NULL;
		}
	} else
		/* No specific data has been created, so just return NULL: */
		data = NULL;
	return (__DECONST(void *, data));
}
