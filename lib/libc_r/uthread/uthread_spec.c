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
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static	struct pthread_key key_table[PTHREAD_KEYS_MAX];

int
pthread_key_create(pthread_key_t * key, void (*destructor) (void *))
{
	for ((*key) = 0; (*key) < PTHREAD_KEYS_MAX; (*key)++) {
		/* Lock the key table entry: */
		_SPINLOCK(&key_table[*key].lock);

		if (key_table[(*key)].allocated == 0) {
			key_table[(*key)].allocated = 1;
			key_table[(*key)].destructor = destructor;

			/* Unlock the key table entry: */
			_SPINUNLOCK(&key_table[*key].lock);
			return (0);
		}

		/* Unlock the key table entry: */
		_SPINUNLOCK(&key_table[*key].lock);
	}
	return (EAGAIN);
}

int
pthread_key_delete(pthread_key_t key)
{
	int ret = 0;

	if (key < PTHREAD_KEYS_MAX) {
		/* Lock the key table entry: */
		_SPINLOCK(&key_table[key].lock);

		if (key_table[key].allocated)
			key_table[key].allocated = 0;
		else
			ret = EINVAL;

		/* Unlock the key table entry: */
		_SPINUNLOCK(&key_table[key].lock);
	} else
		ret = EINVAL;
	return (ret);
}

void 
_thread_cleanupspecific(void)
{
	void           *data = NULL;
	int             key;
	int             itr;
	void		(*destructor)( void *);

	for (itr = 0; itr < PTHREAD_DESTRUCTOR_ITERATIONS; itr++) {
		for (key = 0; key < PTHREAD_KEYS_MAX; key++) {
			if (_thread_run->specific_data_count) {
				/* Lock the key table entry: */
				_SPINLOCK(&key_table[key].lock);
				destructor = NULL;

				if (key_table[key].allocated) {
					if (_thread_run->specific_data[key]) {
						data = (void *) _thread_run->specific_data[key];
						_thread_run->specific_data[key] = NULL;
						_thread_run->specific_data_count--;
						destructor = key_table[key].destructor;
					}
				}

				/* Unlock the key table entry: */
				_SPINUNLOCK(&key_table[key].lock);

				/*
				 * If there is a destructore, call it
				 * with the key table entry unlocked:
				 */
				if (destructor)
					destructor(data);
			} else {
				free(_thread_run->specific_data);
				_thread_run->specific_data = NULL;
				return;
			}
		}
	}
	free(_thread_run->specific_data);
	_thread_run->specific_data = NULL;
}

static inline const void **
pthread_key_allocate_data(void)
{
	const void    **new_data;
	if ((new_data = (const void **) malloc(sizeof(void *) * PTHREAD_KEYS_MAX)) != NULL) {
		memset((void *) new_data, 0, sizeof(void *) * PTHREAD_KEYS_MAX);
	}
	return (new_data);
}

int 
pthread_setspecific(pthread_key_t key, const void *value)
{
	pthread_t       pthread;
	int             ret = 0;

	/* Point to the running thread: */
	pthread = _thread_run;

	if ((pthread->specific_data) ||
	    (pthread->specific_data = pthread_key_allocate_data())) {
		if (key < PTHREAD_KEYS_MAX) {
			if (key_table[key].allocated) {
				if (pthread->specific_data[key] == NULL) {
					if (value != NULL)
						pthread->specific_data_count++;
				} else {
					if (value == NULL)
						pthread->specific_data_count--;
				}
				pthread->specific_data[key] = value;
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
pthread_getspecific(pthread_key_t key)
{
	pthread_t       pthread;
	void		*data;

	/* Point to the running thread: */
	pthread = _thread_run;

	/* Check if there is specific data: */
	if (pthread->specific_data != NULL && key < PTHREAD_KEYS_MAX) {
		/* Check if this key has been used before: */
		if (key_table[key].allocated) {
			/* Return the value: */
			data = (void *) pthread->specific_data[key];
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
	return (data);
}
#endif
