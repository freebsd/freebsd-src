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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: uthread_gc.c,v 1.2 1998/09/30 19:17:51 dt Exp $
 *
 * Garbage collector thread. Frees memory allocated for dead threads.
 *
 */
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include "pthread_private.h"

pthread_addr_t
_thread_gc(pthread_addr_t arg)
{
	int		f_debug;
	int		f_done = 0;
	int		ret;
	sigset_t	mask;
	pthread_t	pthread;
	pthread_t	pthread_cln;
	pthread_t	pthread_nxt;
	pthread_t	pthread_prv;
	struct timespec	abstime;
	void		*p_stack;

	/* Block all signals */
	sigfillset (&mask);
	sigprocmask (SIG_BLOCK, &mask, NULL);

	/* Mark this thread as a library thread (not a user thread). */
	_thread_run->flags |= PTHREAD_FLAGS_PRIVATE;

	/* Set a debug flag based on an environment variable. */
	f_debug = (getenv("LIBC_R_DEBUG") != NULL);

	/* Set the name of this thread. */
	pthread_set_name_np(_thread_run,"GC");

	while (!f_done) {
		/* Check if debugging this application. */
		if (f_debug)
			/* Dump thread info to file. */
			_thread_dump_info();

		/* Lock the thread list: */
		_lock_thread_list();

		/* Check if this is the last running thread: */
		if (_thread_link_list == _thread_run &&
		    _thread_link_list->nxt == NULL)
			/*
			 * This is the last thread, so it can exit
			 * now.
			 */
			f_done = 1;

		/* Unlock the thread list: */
		_unlock_thread_list();

		/*
		 * Lock the garbage collector mutex which ensures that
		 * this thread sees another thread exit:
		 */
		if (pthread_mutex_lock(&_gc_mutex) != 0)
			PANIC("Cannot lock gc mutex");

		/* No stack of thread structure to free yet: */
		p_stack = NULL;
		pthread_cln = NULL;

		/* Point to the first dead thread (if there are any): */
		pthread = _thread_dead;

		/* There is no previous dead thread: */
		pthread_prv = NULL;

		/*
		 * Enter a loop to search for the first dead thread that
		 * has memory to free.
		 */
		while (p_stack == NULL && pthread_cln == NULL &&
		    pthread != NULL) {
			/* Save a pointer to the next thread: */
			pthread_nxt = pthread->nxt_dead;

			/* Check if the initial thread: */
			if (pthread == _thread_initial)
				/* Don't destroy the initial thread. */
				pthread_prv = pthread;

			/*
			 * Check if this thread has detached:
			 */
			else if ((pthread->attr.flags &
			    PTHREAD_DETACHED) != 0) {
				/*
				 * Check if there is no previous dead
				 * thread:
				 */
				if (pthread_prv == NULL)
					/*
					 * The dead thread is at the head
					 * of the list: 
					 */
					_thread_dead = pthread_nxt;
				else
					/*
					 * The dead thread is not at the
					 * head of the list: 
					 */
					pthread_prv->nxt_dead =
					    pthread->nxt_dead;

				/*
				 * Check if the stack was not specified by
				 * the caller to pthread_create and has not
				 * been destroyed yet: 
				 */
				if (pthread->attr.stackaddr_attr == NULL &&
				    pthread->stack != NULL) {
					/*
					 * Point to the stack that must
					 * be freed outside the locks:
					 */
					p_stack = pthread->stack;
				}

				/*
				 * Point to the thread structure that must
				 * be freed outside the locks:
				 */
				pthread_cln = pthread;
			} else {
				/*
				 * This thread has not detached, so do
				 * not destroy it: 
				 */
				pthread_prv = pthread;

				/*
				 * Check if the stack was not specified by
				 * the caller to pthread_create and has not
				 * been destroyed yet: 
				 */
				if (pthread->attr.stackaddr_attr == NULL &&
				    pthread->stack != NULL) {
					/*
					 * Point to the stack that must
					 * be freed outside the locks:
					 */
					p_stack = pthread->stack;

					/*
					 * NULL the stack pointer now
					 * that the memory has been freed: 
					 */
					pthread->stack = NULL;
				}
			}

			/* Point to the next thread: */
			pthread = pthread_nxt;
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
			/* Lock the thread list: */
			_lock_thread_list();

			/*
			 * Check if the thread is at the head of the
			 * linked list.
			 */
			if (_thread_link_list == pthread_cln)
				/* There is no previous thread: */
				_thread_link_list = pthread_cln->nxt;
			else {
				/* Point to the first thread in the list: */
				pthread = _thread_link_list;

				/*
				 * Enter a loop to find the thread in the
				 * linked list before the thread that is
				 * about to be freed.
				 */
				while (pthread != NULL &&
				    pthread->nxt != pthread_cln)
					/* Point to the next thread: */
					pthread = pthread->nxt;

				/* Check that a previous thread was found: */
				if (pthread != NULL) {
					/*
					 * Point the previous thread to
					 * the one after the thread being
					 * freed: 
					 */
					pthread->nxt = pthread_cln->nxt;
				}
			}

			/* Unlock the thread list: */
			_unlock_thread_list();

			/*
			 * Free the memory allocated for the thread
			 * structure.
			 */
			free(pthread_cln);
		}
		
	}
	return (NULL);
}
