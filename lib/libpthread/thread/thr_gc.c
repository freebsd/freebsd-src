/*
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>
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
 * Garbage collector thread. Frees memory allocated for dead threads.
 *
 */
#include <sys/param.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include "thr_private.h"

pthread_addr_t
_thread_gc(pthread_addr_t arg)
{
	struct pthread	*curthread = _get_curthread();
	int		f_debug;
	int		f_done = 0;
	int		ret;
	sigset_t	mask;
	pthread_t	pthread;
	pthread_t	pthread_cln;
	struct timespec	abstime;
	void		*p_stack;

	/* Block all signals */
	sigfillset(&mask);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

	/* Mark this thread as a library thread (not a user thread). */
	curthread->flags |= PTHREAD_FLAGS_PRIVATE;

	/* Set a debug flag based on an environment variable. */
	f_debug = (getenv("LIBC_R_DEBUG") != NULL);

	/* Set the name of this thread. */
	pthread_set_name_np(curthread,"GC");

	while (!f_done) {
		/* Check if debugging this application. */
		if (f_debug)
			/* Dump thread info to file. */
			_thread_dump_info();

		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/* Check if this is the last running thread: */
		if (TAILQ_FIRST(&_thread_list) == curthread &&
		    TAILQ_NEXT(curthread, tle) == NULL)
			/*
			 * This is the last thread, so it can exit
			 * now.
			 */
			f_done = 1;

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();

		/* No stack of thread structure to free yet: */
		p_stack = NULL;
		pthread_cln = NULL;

		/*
		 * Lock the garbage collector mutex which ensures that
		 * this thread sees another thread exit:
		 */
		if (pthread_mutex_lock(&_gc_mutex) != 0)
			PANIC("Cannot lock gc mutex");

		/*
		 * Enter a loop to search for the first dead thread that
		 * has memory to free.
		 */
		for (pthread = TAILQ_FIRST(&_dead_list);
		     p_stack == NULL && pthread_cln == NULL && pthread != NULL;
		     pthread = TAILQ_NEXT(pthread, dle)) {
			/* Check if the initial thread: */
			if (pthread == _thread_initial) {
				/* Don't destroy the initial thread. */
			}
			/*
			 * Check if this thread has detached:
			 */
			else if ((pthread->attr.flags &
			    PTHREAD_DETACHED) != 0) {
				/* Remove this thread from the dead list: */
				TAILQ_REMOVE(&_dead_list, pthread, dle);

				/*
				 * Check if the stack was not specified by
				 * the caller to pthread_create() and has not
				 * been destroyed yet: 
				 */
				if (pthread->attr.stackaddr_attr == NULL &&
				    pthread->stack != NULL) {
					_thread_stack_free(pthread->stack,
					    pthread->attr.stacksize_attr,
					    pthread->attr.guardsize_attr);
				}

				/*
				 * Point to the thread structure that must
				 * be freed outside the locks:
				 */
				pthread_cln = pthread;

			} else {
				/*
				 * This thread has not detached, so do
				 * not destroy it.
				 *
				 * Check if the stack was not specified by
				 * the caller to pthread_create() and has not
				 * been destroyed yet: 
				 */
				if (pthread->attr.stackaddr_attr == NULL &&
				    pthread->stack != NULL) {
					_thread_stack_free(pthread->stack,
					    pthread->attr.stacksize_attr,
					    pthread->attr.guardsize_attr);

					/*
					 * NULL the stack pointer now that the
					 * memory has been freed:
					 */
					pthread->stack = NULL;
				}
			}
		}

		/*
		 * Check if this is not the last thread and there is no
		 * memory to free this time around.
		 */
		if (!f_done && p_stack == NULL && pthread_cln == NULL) {
			/* Get the current time. */
			if (clock_gettime(CLOCK_REALTIME,&abstime) != 0)
				PANIC("gc cannot get time");

			/*
			 * Do a backup poll in 10 seconds if no threads
			 * die before then.
			 */
			abstime.tv_sec += 10;

			/*
			 * Wait for a signal from a dying thread or a
			 * timeout (for a backup poll).
			 */
			if ((ret = pthread_cond_timedwait(&_gc_cond,
			    &_gc_mutex, &abstime)) != 0 && ret != ETIMEDOUT)
				PANIC("gc cannot wait for a signal");
		}

		/* Unlock the garbage collector mutex: */
		if (pthread_mutex_unlock(&_gc_mutex) != 0)
			PANIC("Cannot unlock gc mutex");

		/*
		 * If there is memory to free, do it now. The call to
		 * free() might block, so this must be done outside the
		 * locks.
		 */
		if (p_stack != NULL)
			free(p_stack);
		if (pthread_cln != NULL) {
			if (pthread_cln->name != NULL) {
				/* Free the thread name string. */
				free(pthread_cln->name);
			}
			/*
			 * Free the memory allocated for the thread
			 * structure.
			 */
			free(pthread_cln);
		}
	}
	return (NULL);
}
