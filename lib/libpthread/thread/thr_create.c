/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"
#include "libc_private.h"

int
pthread_create(pthread_t * thread, const pthread_attr_t * attr,
	       void *(*start_routine) (void *), void *arg)
{
	int             i;
	int             ret = 0;
	int             status;
	pthread_t       new_thread;
	pthread_attr_t	pattr;
	void           *stack;

	/*
	 * Locking functions in libc are required when there are
	 * threads other than the initial thread.
	 */
	__isthreaded = 1;

	/* Allocate memory for the thread structure: */
	if ((new_thread = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/* Insufficient memory to create a thread: */
		ret = EAGAIN;
	} else {
		/* Check if default thread attributes are required: */
		if (attr == NULL || *attr == NULL) {
			/* Use the default thread attributes: */
			pattr = &pthread_attr_default;
		} else {
			pattr = *attr;
		}
		/* Check if a stack was specified in the thread attributes: */
		if ((stack = pattr->stackaddr_attr) != NULL) {
		}
		/* Allocate memory for the stack: */
		else if ((stack = (void *) malloc(pattr->stacksize_attr)) == NULL) {
			/* Insufficient memory to create a thread: */
			ret = EAGAIN;
			free(new_thread);
		}
		/* Check for errors: */
		if (ret != 0) {
		} else {
			/* Initialise the thread structure: */
			memset(new_thread, 0, sizeof(struct pthread));
			new_thread->slice_usec = -1;
			new_thread->sig_saved = 0;
			new_thread->stack = stack;
			new_thread->start_routine = start_routine;
			new_thread->arg = arg;

			/*
			 * Write a magic value to the thread structure
			 * to help identify valid ones:
			 */
			new_thread->magic = PTHREAD_MAGIC;

			if (pattr->suspend == PTHREAD_CREATE_SUSPENDED) {
				PTHREAD_NEW_STATE(new_thread,PS_SUSPENDED);
			} else {
				PTHREAD_NEW_STATE(new_thread,PS_RUNNING);
			}

			/* Initialise the thread for signals: */
			new_thread->sigmask = _thread_run->sigmask;

			/* Initialise the jump buffer: */
			setjmp(new_thread->saved_jmp_buf);

			/*
			 * Set up new stack frame so that it looks like it
			 * returned from a longjmp() to the beginning of
			 * _thread_start().
			 */
#if	defined(__FreeBSD__)
#if	defined(__alpha__)
			new_thread->saved_jmp_buf[0]._jb[2] = (long) _thread_start;
			new_thread->saved_jmp_buf[0]._jb[4 + R_RA] = 0;
			new_thread->saved_jmp_buf[0]._jb[4 + R_T12] = (long) _thread_start;
#else
			new_thread->saved_jmp_buf[0]._jb[0] = (long) _thread_start;
#endif
#elif	defined(__NetBSD__)
#if	defined(__alpha__)
			new_thread->saved_jmp_buf[2] = (long) _thread_start;
			new_thread->saved_jmp_buf[4 + R_RA] = 0;
			new_thread->saved_jmp_buf[4 + R_T12] = (long) _thread_start;
#else
			new_thread->saved_jmp_buf[0] = (long) _thread_start;
#endif
#else
#error	"Don't recognize this operating system!"
#endif

			/* The stack starts high and builds down: */
#if	defined(__FreeBSD__)
#if	defined(__alpha__)
			new_thread->saved_jmp_buf[0]._jb[4 + R_SP] = (long) new_thread->stack + pattr->stacksize_attr - sizeof(double);
#else
			new_thread->saved_jmp_buf[0]._jb[2] = (int) (new_thread->stack + pattr->stacksize_attr - sizeof(double));
#endif
#elif	defined(__NetBSD__)
#if	defined(__alpha__)
			new_thread->saved_jmp_buf[4 + R_SP] = (long) new_thread->stack + pattr->stacksize_attr - sizeof(double);
#else
			new_thread->saved_jmp_buf[2] = (long) new_thread->stack + pattr->stacksize_attr - sizeof(double);
#endif
#else
#error	"Don't recognize this operating system!"
#endif

			/* Copy the thread attributes: */
			memcpy(&new_thread->attr, pattr, sizeof(struct pthread_attr));

			/*
			 * Check if this thread is to inherit the scheduling
			 * attributes from its parent: 
			 */
			if (new_thread->attr.flags & PTHREAD_INHERIT_SCHED) {
				/* Copy the scheduling attributes: */
				new_thread->pthread_priority = _thread_run->pthread_priority;
				new_thread->attr.prio = _thread_run->pthread_priority;
				new_thread->attr.schedparam_policy = _thread_run->attr.schedparam_policy;
			} else {
				/*
				 * Use just the thread priority, leaving the
				 * other scheduling attributes as their
				 * default values: 
				 */
				new_thread->pthread_priority = new_thread->attr.prio;
			}

			/* Initialise the join queue for the new thread: */
			_thread_queue_init(&(new_thread->join_queue));

			/* Initialise hooks in the thread structure: */
			new_thread->specific_data = NULL;
			new_thread->cleanup = NULL;
			new_thread->queue = NULL;
			new_thread->qnxt = NULL;
			new_thread->flags = 0;

			/* Lock the thread list: */
			_lock_thread_list();

			/* Add the thread to the linked list of all threads: */
			new_thread->nxt = _thread_link_list;
			_thread_link_list = new_thread;

			/* Unlock the thread list: */
			_unlock_thread_list();

			/* Return a pointer to the thread structure: */
			(*thread) = new_thread;

			/* Schedule the new user thread: */
			_thread_kern_sched(NULL);
		}
	}

	/* Return the status: */
	return (ret);
}

void
_thread_start(void)
{
	/* Run the current thread's start routine with argument: */
	pthread_exit(_thread_run->start_routine(_thread_run->arg));

	/* This point should never be reached. */
	PANIC("Thread has resumed after exit");
}
#endif
