/*
 * Copyright (c) 2003 Daniel M. Eischen <deischen@gdeb.com>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <sys/signalvar.h>

#include "thr_private.h"

static void free_thread(struct pthread *curthread, struct pthread *thread);
static int  create_stack(struct pthread_attr *pattr);
static void free_stack(struct pthread *curthread, struct pthread_attr *pattr);
static void thread_start(struct pthread *curthread);

__weak_reference(_pthread_create, pthread_create);

int
_pthread_create(pthread_t * thread, const pthread_attr_t * attr,
	       void *(*start_routine) (void *), void *arg)
{
	ucontext_t uc;
	sigset_t sigmask, oldsigmask;
	struct pthread *curthread, *new_thread;
	int ret = 0;

	_thr_check_init();

	/*
	 * Tell libc and others now they need lock to protect their data.
	 */
	if (_thr_isthreaded() == 0 && _thr_setthreaded(1))
		return (EAGAIN);

	curthread = _get_curthread();
	if ((new_thread = _thr_alloc(curthread)) == NULL)
		return (EAGAIN);

	if (attr == NULL || *attr == NULL)
		/* Use the default thread attributes: */
		new_thread->attr = _pthread_attr_default;
	else
		new_thread->attr = *(*attr);
	if (new_thread->attr.sched_inherit == PTHREAD_INHERIT_SCHED) {
		/* inherit scheduling contention scope */
		if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
			new_thread->attr.flags |= PTHREAD_SCOPE_SYSTEM;
		else
			new_thread->attr.flags &= ~PTHREAD_SCOPE_SYSTEM;
		/*
		 * scheduling policy and scheduling parameters will be
		 * inherited in following code.
		 */
	}

	if (_thr_scope_system > 0)
		new_thread->attr.flags |= PTHREAD_SCOPE_SYSTEM;
	else if (_thr_scope_system < 0)
		new_thread->attr.flags &= ~PTHREAD_SCOPE_SYSTEM;

	if (create_stack(&new_thread->attr) != 0) {
		/* Insufficient memory to create a stack: */
		new_thread->terminated = 1;
		_thr_free(curthread, new_thread);
		return (EAGAIN);
	}
	/*
	 * Write a magic value to the thread structure
	 * to help identify valid ones:
	 */
	new_thread->magic = THR_MAGIC;
	new_thread->start_routine = start_routine;
	new_thread->arg = arg;
	new_thread->cancelflags = PTHREAD_CANCEL_ENABLE |
	    PTHREAD_CANCEL_DEFERRED;
	getcontext(&uc);
	SIGFILLSET(uc.uc_sigmask);
	uc.uc_stack.ss_sp = new_thread->attr.stackaddr_attr;
	uc.uc_stack.ss_size = new_thread->attr.stacksize_attr;
	makecontext(&uc, (void (*)(void))thread_start, 1, new_thread);

	/*
	 * Check if this thread is to inherit the scheduling
	 * attributes from its parent:
	 */
	if (new_thread->attr.sched_inherit == PTHREAD_INHERIT_SCHED) {
		/*
		 * Copy the scheduling attributes. Lock the scheduling
		 * lock to get consistent scheduling parameters.
		 */
		THR_LOCK(curthread);
		new_thread->base_priority = curthread->base_priority;
		new_thread->attr.prio = curthread->base_priority;
		new_thread->attr.sched_policy = curthread->attr.sched_policy;
		THR_UNLOCK(curthread);
	} else {
		/*
		 * Use just the thread priority, leaving the
		 * other scheduling attributes as their
		 * default values:
		 */
		new_thread->base_priority = new_thread->attr.prio;
	}
	new_thread->active_priority = new_thread->base_priority;

	/* Initialize the mutex queue: */
	TAILQ_INIT(&new_thread->mutexq);
	TAILQ_INIT(&new_thread->pri_mutexq);

	/* Initialise hooks in the thread structure: */
	if (new_thread->attr.suspend == THR_CREATE_SUSPENDED)
		new_thread->flags = THR_FLAGS_SUSPENDED;
	new_thread->state = PS_RUNNING;
	/*
	 * Thread created by thr_create() inherits currrent thread
	 * sigmask, however, before new thread setup itself correctly,
	 * it can not handle signal, so we should masks all signals here.
	 */
	SIGFILLSET(sigmask);
	__sys_sigprocmask(SIG_SETMASK, &sigmask, &oldsigmask);
	new_thread->sigmask = oldsigmask;
	/* Add the new thread. */
	_thr_link(curthread, new_thread);
	/* Return thread pointer eariler so that new thread can use it. */
	(*thread) = new_thread;
	/* Schedule the new thread. */
	ret = thr_create(&uc, &new_thread->tid, 0);
	__sys_sigprocmask(SIG_SETMASK, &oldsigmask, NULL);
	if (ret != 0) {
		_thr_unlink(curthread, new_thread);
		free_thread(curthread, new_thread);
		(*thread) = 0;
		ret = EAGAIN;
	}
	return (ret);
}

static void
free_thread(struct pthread *curthread, struct pthread *thread)
{
	free_stack(curthread, &thread->attr);
	curthread->terminated = 1;
	_thr_free(curthread, thread);
}

static int
create_stack(struct pthread_attr *pattr)
{
	int ret;

	/* Check if a stack was specified in the thread attributes: */
	if ((pattr->stackaddr_attr) != NULL) {
		pattr->guardsize_attr = 0;
		pattr->flags |= THR_STACK_USER;
		ret = 0;
	}
	else
		ret = _thr_stack_alloc(pattr);
	return (ret);
}

static void
free_stack(struct pthread *curthread, struct pthread_attr *pattr)
{
	if ((pattr->flags & THR_STACK_USER) == 0) {
		THREAD_LIST_LOCK(curthread);
		/* Stack routines don't use malloc/free. */
		_thr_stack_free(pattr);
		THREAD_LIST_UNLOCK(curthread);
	}
}

static void
thread_start(struct pthread *curthread)
{
	_tcb_set(curthread->tcb);

	/* Thread was created with all signals blocked, unblock them. */
	__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);

	if (curthread->flags & THR_FLAGS_NEED_SUSPEND)
		_thr_suspend_check(curthread);

	/* Run the current thread's start routine with argument: */
	pthread_exit(curthread->start_routine(curthread->arg));

	/* This point should never be reached. */
	PANIC("Thread has resumed after exit");
}
