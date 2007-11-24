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
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/time.h>
#include <machine/reg.h>
#include <pthread.h>
#include "thr_private.h"
#include "libc_private.h"

static void free_thread(struct pthread *curthread, struct pthread *thread);
static int  create_stack(struct pthread_attr *pattr);
static void free_stack(struct pthread_attr *pattr);
static void thread_start(struct pthread *curthread,
		void *(*start_routine) (void *), void *arg);

__weak_reference(_pthread_create, pthread_create);

/*
 * Some notes on new thread creation and first time initializion
 * to enable multi-threading.
 *
 * There are basically two things that need to be done.
 *
 *   1) The internal library variables must be initialized.
 *   2) Upcalls need to be enabled to allow multiple threads
 *      to be run.
 *
 * The first may be done as a result of other pthread functions
 * being called.  When _thr_initial is null, _libpthread_init is
 * called to initialize the internal variables; this also creates
 * or sets the initial thread.  It'd be nice to automatically
 * have _libpthread_init called on program execution so we don't
 * have to have checks throughout the library.
 *
 * The second part is only triggered by the creation of the first
 * thread (other than the initial/main thread).  If the thread
 * being created is a scope system thread, then a new KSE/KSEG
 * pair needs to be allocated.  Also, if upcalls haven't been
 * enabled on the initial thread's KSE, they must be now that
 * there is more than one thread; this could be delayed until
 * the initial KSEG has more than one thread.
 */
int
_pthread_create(pthread_t * thread, const pthread_attr_t * attr,
	       void *(*start_routine) (void *), void *arg)
{
	struct pthread *curthread, *new_thread;
	struct kse *kse = NULL;
	struct kse_group *kseg = NULL;
	kse_critical_t crit;
	int ret = 0;

	if (_thr_initial == NULL)
		_libpthread_init(NULL);

	/*
	 * Turn on threaded mode, if failed, it is unnecessary to
	 * do further work.
	 */
	if (_kse_isthreaded() == 0 && _kse_setthreaded(1)) {
		return (EAGAIN);
	}
	curthread = _get_curthread();

	/*
	 * Allocate memory for the thread structure.
	 * Some functions use malloc, so don't put it
	 * in a critical region.
	 */
	if ((new_thread = _thr_alloc(curthread)) == NULL) {
		/* Insufficient memory to create a thread: */
		ret = EAGAIN;
	} else {
		/* Check if default thread attributes are required: */
		if (attr == NULL || *attr == NULL)
			/* Use the default thread attributes: */
			new_thread->attr = _pthread_attr_default;
		else {
			new_thread->attr = *(*attr);
			if ((*attr)->sched_inherit == PTHREAD_INHERIT_SCHED) {
				/* inherit scheduling contention scop */
				if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
					new_thread->attr.flags |= PTHREAD_SCOPE_SYSTEM;
				else
					new_thread->attr.flags &= ~PTHREAD_SCOPE_SYSTEM;
				/*
				 * scheduling policy and scheduling parameters will be
				 * inherited in following code.
				 */
			}
		}
		if (_thread_scope_system > 0)
			new_thread->attr.flags |= PTHREAD_SCOPE_SYSTEM;
		else if ((_thread_scope_system < 0)
		    && (thread != &_thr_sig_daemon))
			new_thread->attr.flags &= ~PTHREAD_SCOPE_SYSTEM;
		if (create_stack(&new_thread->attr) != 0) {
			/* Insufficient memory to create a stack: */
			ret = EAGAIN;
			_thr_free(curthread, new_thread);
		}
		else if (((new_thread->attr.flags & PTHREAD_SCOPE_SYSTEM) != 0) &&
		    (((kse = _kse_alloc(curthread, 1)) == NULL)
		    || ((kseg = _kseg_alloc(curthread)) == NULL))) {
			/* Insufficient memory to create a new KSE/KSEG: */
			ret = EAGAIN;
			if (kse != NULL) {
				kse->k_kcb->kcb_kmbx.km_flags |= KMF_DONE;
				_kse_free(curthread, kse);
			}
			free_stack(&new_thread->attr);
			_thr_free(curthread, new_thread);
		}
		else {
			if (kseg != NULL) {
				/* Add the KSE to the KSEG's list of KSEs. */
				TAILQ_INSERT_HEAD(&kseg->kg_kseq, kse, k_kgqe);
				kseg->kg_ksecount = 1;
				kse->k_kseg = kseg;
				kse->k_schedq = &kseg->kg_schedq;
			}
			/*
			 * Write a magic value to the thread structure
			 * to help identify valid ones:
			 */
			new_thread->magic = THR_MAGIC;

			new_thread->slice_usec = -1;
			new_thread->start_routine = start_routine;
			new_thread->arg = arg;
			new_thread->cancelflags = PTHREAD_CANCEL_ENABLE |
			    PTHREAD_CANCEL_DEFERRED;

			/* No thread is wanting to join to this one: */
			new_thread->joiner = NULL;

			/*
			 * Initialize the machine context.
			 * Enter a critical region to get consistent context.
			 */
			crit = _kse_critical_enter();
			THR_GETCONTEXT(&new_thread->tcb->tcb_tmbx.tm_context);
			/* Initialize the thread for signals: */
			new_thread->sigmask = curthread->sigmask;
			_kse_critical_leave(crit);

			new_thread->tcb->tcb_tmbx.tm_udata = new_thread;
			new_thread->tcb->tcb_tmbx.tm_context.uc_sigmask =
			    new_thread->sigmask;
			new_thread->tcb->tcb_tmbx.tm_context.uc_stack.ss_size =
			    new_thread->attr.stacksize_attr;
			new_thread->tcb->tcb_tmbx.tm_context.uc_stack.ss_sp =
			    new_thread->attr.stackaddr_attr;
			makecontext(&new_thread->tcb->tcb_tmbx.tm_context,
			    (void (*)(void))thread_start, 3, new_thread,
			    start_routine, arg);
			/*
			 * Check if this thread is to inherit the scheduling
			 * attributes from its parent:
			 */
			if (new_thread->attr.sched_inherit == PTHREAD_INHERIT_SCHED) {
				/*
				 * Copy the scheduling attributes.
				 * Lock the scheduling lock to get consistent
				 * scheduling parameters.
				 */
				THR_SCHED_LOCK(curthread, curthread);
				new_thread->base_priority =
				    curthread->base_priority &
				    ~THR_SIGNAL_PRIORITY;
				new_thread->attr.prio =
				    curthread->base_priority &
				    ~THR_SIGNAL_PRIORITY;
				new_thread->attr.sched_policy =
				    curthread->attr.sched_policy;
				THR_SCHED_UNLOCK(curthread, curthread);
			} else {
				/*
				 * Use just the thread priority, leaving the
				 * other scheduling attributes as their
				 * default values:
				 */
				new_thread->base_priority =
				    new_thread->attr.prio;
			}
			new_thread->active_priority = new_thread->base_priority;
			new_thread->inherited_priority = 0;

			/* Initialize the mutex queue: */
			TAILQ_INIT(&new_thread->mutexq);

			/* Initialise hooks in the thread structure: */
			new_thread->specific = NULL;
			new_thread->specific_data_count = 0;
			new_thread->cleanup = NULL;
			new_thread->flags = 0;
			new_thread->tlflags = 0;
			new_thread->sigbackout = NULL;
			new_thread->continuation = NULL;
			new_thread->wakeup_time.tv_sec = -1;
			new_thread->lock_switch = 0;
			sigemptyset(&new_thread->sigpend);
			new_thread->check_pending = 0;
			new_thread->locklevel = 0;
			new_thread->rdlock_count = 0;
			new_thread->sigstk.ss_sp = 0;
			new_thread->sigstk.ss_size = 0;
			new_thread->sigstk.ss_flags = SS_DISABLE;
			new_thread->oldsigmask = NULL;

			if (new_thread->attr.suspend == THR_CREATE_SUSPENDED) {
				new_thread->state = PS_SUSPENDED;
				new_thread->flags = THR_FLAGS_SUSPENDED;
			}
			else
				new_thread->state = PS_RUNNING;

			/*
			 * System scope threads have their own kse and
			 * kseg.  Process scope threads are all hung
			 * off the main process kseg.
			 */
			if ((new_thread->attr.flags & PTHREAD_SCOPE_SYSTEM) == 0) {
				new_thread->kseg = _kse_initial->k_kseg;
				new_thread->kse = _kse_initial;
			}
			else {
				kse->k_curthread = NULL;
				kse->k_kseg->kg_flags |= KGF_SINGLE_THREAD;
				new_thread->kse = kse;
				new_thread->kseg = kse->k_kseg;
				kse->k_kcb->kcb_kmbx.km_udata = kse;
				kse->k_kcb->kcb_kmbx.km_curthread = NULL;
			}

			/*
			 * Schedule the new thread starting a new KSEG/KSE
			 * pair if necessary.
			 */
			ret = _thr_schedule_add(curthread, new_thread);
			if (ret != 0)
				free_thread(curthread, new_thread);
			else {
				/* Return a pointer to the thread structure: */
				(*thread) = new_thread;
			}
		}
	}

	/* Return the status: */
	return (ret);
}

static void
free_thread(struct pthread *curthread, struct pthread *thread)
{
	free_stack(&thread->attr);
	if ((thread->attr.flags & PTHREAD_SCOPE_SYSTEM) != 0) {
		/* Free the KSE and KSEG. */
		_kseg_free(thread->kseg);
		_kse_free(curthread, thread->kse);
	}
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
free_stack(struct pthread_attr *pattr)
{
	struct kse *curkse;
	kse_critical_t crit;

	if ((pattr->flags & THR_STACK_USER) == 0) {
		crit = _kse_critical_enter();
		curkse = _get_curkse();
		KSE_LOCK_ACQUIRE(curkse, &_thread_list_lock);
		/* Stack routines don't use malloc/free. */
		_thr_stack_free(pattr);
		KSE_LOCK_RELEASE(curkse, &_thread_list_lock);
		_kse_critical_leave(crit);
	}
}

static void
thread_start(struct pthread *curthread, void *(*start_routine) (void *),
    void *arg)
{
	/* Run the current thread's start routine with argument: */
	pthread_exit(start_routine(arg));

	/* This point should never be reached. */
	PANIC("Thread has resumed after exit");
}
