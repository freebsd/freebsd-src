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

int
_thread_create(pthread_t * thread, const pthread_attr_t * attr,
	       void *(*start_routine) (void *), void *arg, pthread_t parent)
{
	int             i;
	int             ret = 0;
	int             status;
	pthread_t       new_thread;
	pthread_attr_t	pattr;
	void           *stack;

	/* Block signals: */
	_thread_kern_sig_block(&status);

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
			if (pattr->suspend == PTHREAD_CREATE_SUSPENDED) {
				new_thread->state = PS_SUSPENDED;
			} else {
				new_thread->state = PS_RUNNING;
			}

			/* Initialise the thread for signals: */
			new_thread->sigmask = _thread_run->sigmask;

			/*
			 * Enter a loop to initialise the signal handler
			 * array: 
			 */
			for (i = 1; i < NSIG; i++) {
				/* Default the signal handler: */
				sigfillset(&new_thread->act[i - 1].sa_mask);
				new_thread->act[i - 1].sa_handler = _thread_run->act[i - 1].sa_handler;
				new_thread->act[i - 1].sa_flags = _thread_run->act[i - 1].sa_flags;
			}

			/* Initialise the jump buffer: */
			_thread_sys_setjmp(new_thread->saved_jmp_buf);

			/*
			 * Set up new stack frame so that it looks like it
			 * returned from a longjmp() to the beginning of
			 * _thread_start(). Check if this is a user thread: 
			 */
			if (parent == NULL) {
				/* Use the user start function: */
#if	defined(__FreeBSD__)
				new_thread->saved_jmp_buf[0]._jb[0] = (long) _thread_start;
#elif	defined(__NetBSD__)
#if	defined(__alpha)
				new_thread->saved_jmp_buf[2] = (long) _thread_start;
				new_thread->saved_jmp_buf[4 + R_RA] = 0;
				new_thread->saved_jmp_buf[4 + R_T12] = (long) _thread_start;
#else
				new_thread->saved_jmp_buf[0] = (long) _thread_start;
#endif
#else
#error	"Don't recognize this operating system!"
#endif
			} else {
				/*
				 * Use the (funny) signal handler start
				 * function: 
				 */
#if	defined(__FreeBSD__)
				new_thread->saved_jmp_buf[0]._jb[0] = (int) _thread_start_sig_handler;
#elif	defined(__NetBSD__)
#if	defined(__alpha)
				new_thread->saved_jmp_buf[2] = (long) _thread_start_sig_handler;
				new_thread->saved_jmp_buf[4 + R_RA] = 0;
				new_thread->saved_jmp_buf[4 + R_T12] = (long) _thread_start_sig_handler;
#else
				new_thread->saved_jmp_buf[0] = (long) _thread_start_sig_handler;
#endif
#else
#error	"Don't recognize this operating system!"
#endif
			}

			/* The stack starts high and builds down: */
#if	defined(__FreeBSD__)
			new_thread->saved_jmp_buf[0]._jb[2] = (int) (new_thread->stack + pattr->stacksize_attr - sizeof(double));
#elif	defined(__NetBSD__)
#if	defined(__alpha)
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
			new_thread->parent_thread = parent;
			new_thread->flags = 0;

			/* Add the thread to the linked list of all threads: */
			new_thread->nxt = _thread_link_list;
			_thread_link_list = new_thread;

			/* Return a pointer to the thread structure: */
			(*thread) = new_thread;

			/* Check if a parent thread was specified: */
			if (parent != NULL) {
				/*
				 * A parent thread was specified, so this is
				 * a signal handler thread which must now
				 * wait for the signal handler to complete: 
				 */
				parent->state = PS_SIGTHREAD;
			} else {
				/* Schedule the new user thread: */
				_thread_kern_sched(NULL);
			}
		}
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);

	/* Return the status: */
	return (ret);
}

int
pthread_create(pthread_t * thread, const pthread_attr_t * attr,
	       void *(*start_routine) (void *), void *arg)
{
	int             ret = 0;

	/*
	 * Call the low level thread creation function which allows a parent
	 * thread to be specified: 
	 */
	ret = _thread_create(thread, attr, start_routine, arg, NULL);

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

void
_thread_start_sig_handler(void)
{
	int             sig;
	long            arg;
	void            (*sig_routine) (int);

	/*
	 * Cast the argument from 'void *' to a variable that is NO SMALLER
	 * than a pointer (otherwise gcc under NetBSD/Alpha will complain): 
	 */
	arg = (long) _thread_run->arg;

	/* Cast the argument as a signal number: */
	sig = (int) arg;

	/* Cast a pointer to the signal handler function: */
	sig_routine = (void (*) (int)) _thread_run->start_routine;

	/* Call the signal handler function: */
	(*sig_routine) (sig);

	/* Exit the signal handler thread: */
	pthread_exit(&arg);

	/* This point should never be reached. */
	PANIC("Signal handler thread has resumed after exit");
}
#endif
