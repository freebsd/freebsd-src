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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static struct pthread_key key_table[PTHREAD_KEYS_MAX];

int
pthread_key_create(pthread_key_t * key, void (*destructor) (void *))
{
	for ((*key) = 0; (*key) < PTHREAD_KEYS_MAX; (*key)++) {
		if (key_table[(*key)].count == 0) {
			key_table[(*key)].count++;
			key_table[(*key)].destructor = destructor;
			return (0);
		}
	}
	return (EAGAIN);
}

int
pthread_key_delete(pthread_key_t key)
{
	int             ret;
	int             status;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	if (key < PTHREAD_KEYS_MAX) {
		switch (key_table[key].count) {
		case 1:
			key_table[key].destructor = NULL;
			key_table[key].count = 0;
		case 0:
			ret = 0;
			break;
		default:
			ret = EBUSY;
		}
	} else {
		ret = EINVAL;
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);
	return (ret);
}

void 
_thread_cleanupspecific(void)
{
	void           *data;
	int             key;
	int             itr;
	int             status;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	for (itr = 0; itr < PTHREAD_DESTRUCTOR_ITERATIONS; itr++) {
		for (key = 0; key < PTHREAD_KEYS_MAX; key++) {
			if (_thread_run->specific_data_count) {
				if (_thread_run->specific_data[key]) {
					data = (void *) _thread_run->specific_data[key];
					_thread_run->specific_data[key] = NULL;
					_thread_run->specific_data_count--;
					if (key_table[key].destructor) {
						key_table[key].destructor(data);
					}
					key_table[key].count--;
				}
			} else {
				free(_thread_run->specific_data);

				/* Unblock signals: */
				_thread_kern_sig_unblock(status);
				return;
			}
		}
	}
	free(_thread_run->specific_data);

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);
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
	int             status;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	/* Point to the running thread: */
	pthread = _thread_run;

	/*
	 * Enter a loop for signal handler threads to find the parent thread
	 * which has the specific data associated with it: 
	 */
	while (pthread->parent_thread != NULL) {
		/* Point to the parent thread: */
		pthread = pthread->parent_thread;
	}

	if ((pthread->specific_data) || (pthread->specific_data = pthread_key_allocate_data())) {
		if ((key < PTHREAD_KEYS_MAX) && (key_table)) {
			if (key_table[key].count) {
				if (pthread->specific_data[key] == NULL) {
					if (value != NULL) {
						pthread->specific_data_count++;
						key_table[key].count++;
					}
				} else {
					if (value == NULL) {
						pthread->specific_data_count--;
						key_table[key].count--;
					}
				}
				pthread->specific_data[key] = value;
				ret = 0;
			} else {
				ret = EINVAL;
			}
		} else {
			ret = EINVAL;
		}
	} else {
		ret = ENOMEM;
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);
	return (ret);
}

void *
pthread_getspecific(pthread_key_t key)
{
	pthread_t       pthread;
	int             status;
	void		*data;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	/* Point to the running thread: */
	pthread = _thread_run;

	/*
	 * Enter a loop for signal handler threads to find the parent thread
	 * which has the specific data associated with it: 
	 */
	while (pthread->parent_thread != NULL) {
		/* Point to the parent thread: */
		pthread = pthread->parent_thread;
	}

	/* Check for errors: */
	if (pthread == NULL) {
		/* Return an invalid argument error: */
		errno = EINVAL;
		data = NULL;
	}
	/* Check if there is specific data: */
	else if (pthread->specific_data != NULL && (key < PTHREAD_KEYS_MAX) && (key_table)) {
		/* Check if this key has been used before: */
		if (key_table[key].count) {
			/* Return the value: */
			data = (void *) pthread->specific_data[key];
		} else {
			/*
			 * This key has not been used before, so return NULL
			 * instead: 
			 */
			data = NULL;
		}
	} else {
		/* No specific data has been created, so just return NULL: */
		data = NULL;
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);
	return (data);
}
#endif
