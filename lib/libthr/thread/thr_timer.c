/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <time.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "thr_private.h"

struct thread_node {
	struct pthread_attr		attr;
	TAILQ_ENTRY(thread_node)	link;
	pthread_t	thread;
	int		refcount;
	int		exit;
	jmp_buf		jbuf;
	struct timer 	*curtmr;
};

struct timer {
	union sigval	value;
	void		(*function)(union sigval *, int);
	int		timerid;
	long		flags;
	int		gen;
	struct thread_node *tn;
};

static struct timer		**timer_list;
static int			timer_gen;
static int			timer_max;
static umtx_t			timer_list_lock;
static TAILQ_HEAD(,thread_node)	timer_threads;
static umtx_t			timer_threads_lock;

static void	*service_loop(void *);
static int	register_timer(struct timer *);
static struct thread_node	*create_timer_thread(pthread_attr_t);
static void	release_timer_thread(struct thread_node *);

extern int __sys_timer_create(clockid_t, struct sigevent *, timer_t *);
extern int __sys_timer_delete(timer_t);

__weak_reference(__timer_create, timer_create);
__weak_reference(__timer_create, _timer_create);
__weak_reference(__timer_delete, timer_delete);
__weak_reference(__timer_delete, _timer_delete);

#define	SIGTIMER	SIGCANCEL	/* Reuse SIGCANCEL */

#define	WORKING		0x01
#define	WANTED		0x02

#define	TIMERS_LOCK(t)		THR_UMTX_LOCK((t), &timer_list_lock)
#define	TIMERS_UNLOCK(t)	THR_UMTX_UNLOCK((t), &timer_list_lock)

#define	THREADS_LOCK(t)		THR_UMTX_LOCK((t), &timer_threads_lock)
#define	THREADS_UNLOCK(t)	THR_UMTX_UNLOCK((t), &timer_threads_lock)

void
_thr_timer_init(void)
{
	_thr_umtx_init(&timer_list_lock);
	_thr_umtx_init(&timer_threads_lock);
	TAILQ_INIT(&timer_threads);
	timer_list = NULL;
	timer_max = 0;
}

/*
 * Purpose of the function is to implement POSIX timer's
 * SEGEV_THREAD notification mechanism.
 */
int
__timer_create(clockid_t clockid, struct sigevent *evp, timer_t *timerid)
{
	pthread_attr_t attr;
	struct sigevent ev;
	struct timer *tmr;
	int ret;

	/* Call syscall directly if it is not SIGEV_THREAD */
	if (evp == NULL || evp->sigev_notify != SIGEV_THREAD)
		return (__sys_timer_create(clockid, evp, timerid));

	/* Otherwise, do all magical things. */
	tmr = malloc(sizeof(*tmr));
	if (__predict_false(tmr == NULL)) {
		errno = EAGAIN;
		return (-1);
	}
	tmr->value = evp->sigev_value;
	/* XXX
	 * Here we pass second parameter an overrun count, this is
	 * not required by POSIX.
	 */
	tmr->function = (void (*)(union sigval *, int))
		evp->sigev_notify_function;
	tmr->flags = 0;
	tmr->timerid = -1;
	pthread_attr_init(&attr);
	if (evp->sigev_notify_attributes != NULL) {
		*attr = **(pthread_attr_t *)(evp->sigev_notify_attributes);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	}

	tmr->gen = atomic_fetchadd_int(&timer_gen, 1);
	tmr->tn = create_timer_thread(attr);
	if (tmr->tn == NULL) {
		free(tmr);
		errno = EAGAIN;
		return (-1);
	}

	/*
	 * Build a new sigevent, and tell kernel to deliver
	 * SIGTIMER signal to the new thread.
	 */
	ev.sigev_notify = SIGEV_THREAD_ID;
	ev.sigev_signo = SIGTIMER;
	ev.sigev_notify_thread_id = (lwpid_t)tmr->tn->thread->tid;
	ev.sigev_value.sigval_int = tmr->gen;
	ret = __sys_timer_create(clockid, &ev, &tmr->timerid);
	if (ret != 0 || register_timer(tmr) != 0) {
		ret = errno;
		release_timer_thread(tmr->tn);
		free(tmr);
		errno = ret;
		return (-1);
	}
	*timerid = tmr->timerid;
	return (0);
}

int
__timer_delete(timer_t timerid)
{
	struct pthread *curthread = _get_curthread();
	struct timer *tmr = NULL;
	long flags;

	TIMERS_LOCK(curthread);
	/*
	 * Check if this is a SIGEV_THREAD timer by looking up
	 * it in the registered list.
	 */
	if (timerid >= 0 && timerid < timer_max && 
	    (tmr = timer_list[timerid]) != NULL) {
		/* Take it from timer list */
		timer_list[timerid] = NULL;
		/* If the timer is servicing, allow it to complete. */
		while ((flags = tmr->flags) & WORKING) {
			tmr->flags |= WANTED;
			TIMERS_UNLOCK(curthread);
			_thr_umtx_wait(&tmr->flags, flags, NULL);
			TIMERS_LOCK(curthread);
		}
		TIMERS_UNLOCK(curthread);
		/*
		 * Drop reference count of servicing thread,
		 * may free the the thread.
		 */
		release_timer_thread(tmr->tn);
	} else
		TIMERS_UNLOCK(curthread);
	if (tmr != NULL)
		free(tmr);
	return (__sys_timer_delete(timerid));
}

static struct thread_node *
create_timer_thread(pthread_attr_t attr)
{
	struct pthread *curthread = _get_curthread();
	struct thread_node *tn;
	int ret;

	THREADS_LOCK(curthread);
	/* Search a thread matching the required pthread_attr. */
	TAILQ_FOREACH(tn, &timer_threads, link) {
		if (attr->stackaddr_attr == NULL) {
			if (attr->sched_policy == tn->attr.sched_policy &&
			    attr->sched_inherit == tn->attr.sched_inherit &&
			    attr->prio == tn->attr.prio &&
			    attr->stacksize_attr == tn->attr.stacksize_attr &&
			    attr->guardsize_attr == tn->attr.guardsize_attr &&
			    ((attr->flags & PTHREAD_SCOPE_SYSTEM) ==
			     (tn->attr.flags & PTHREAD_SCOPE_SYSTEM)))
				break;
		} else {
			/*
			 * Reuse the thread if it has same stack address,
			 * because two threads can not run on same stack.
			 */
			if (attr->stackaddr_attr == tn->attr.stackaddr_attr)
				break;
		}
	}
	if (tn != NULL) {
		tn->refcount++;
		THREADS_UNLOCK(curthread);
		return (tn);
	}
	tn = malloc(sizeof(*tn));
	tn->refcount = 1;
	tn->exit = 0;
	tn->attr = *attr;
	tn->curtmr = NULL;
	_thr_signal_block(curthread); /* SIGTIMER is also blocked. */
	TAILQ_INSERT_TAIL(&timer_threads, tn, link);
	ret = _pthread_create(&tn->thread, &attr, service_loop, tn);
	_thr_signal_unblock(curthread);
	if (ret != 0) {
		TAILQ_REMOVE(&timer_threads, tn, link);
		free(tn);
		tn = NULL;
	}
	THREADS_UNLOCK(curthread);
	return (tn);
}

static void
release_timer_thread(struct thread_node *tn)
{
	struct pthread *curthread = _get_curthread();
	struct pthread *th;

	THREADS_LOCK(curthread);
	if (--tn->refcount == 0) {
		/*
		 * If I am the last user, current implement kills the
		 * service thread, is this allowed by POSIX ? does
		 * this hurt performance ?
		 */ 
		tn->exit = 1;
		th = tn->thread;
		_thr_send_sig(th, SIGTIMER);
		pthread_join(th, NULL);
		TAILQ_REMOVE(&timer_threads, tn, link);
	}
	THREADS_UNLOCK(curthread);
}

/* Register a SIGEV_THREAD timer. */
static int
register_timer(struct timer *tmr)
{
	struct pthread *curthread = _get_curthread();
	struct timer **list;
	int count;

	while ((count = timer_max) <= tmr->timerid) {
		if (count < 32)
			count = 32;
		while (count <= tmr->timerid)
			count <<= 1;
		list = malloc(count * sizeof(void *));
		memset(list, 0, count * sizeof(void *));
		if (list == NULL)
			return (-1);
		TIMERS_LOCK(curthread);
		if (timer_max >= count) {
			TIMERS_UNLOCK(curthread);
			free(list);
			continue;
		}
		memcpy(timer_list, list, timer_max * sizeof(void *));
		timer_list = list;
		timer_max = count;
		THR_UMTX_UNLOCK(curthread, &timer_list_lock);
	}
	TIMERS_LOCK(curthread);
	timer_list[tmr->timerid] = tmr;
	TIMERS_UNLOCK(curthread);
	return (0);
}

static void
cleanup_thread(void *arg)
{
	struct pthread *curthread = _get_curthread();
	struct thread_node *tn = arg;

	if (tn->exit == 0) {
		/* broken usercode is killing us. */
		if (tn->curtmr) {
			TIMERS_LOCK(curthread);
			tn->curtmr->flags &= ~WORKING;
			if (tn->curtmr->flags & WANTED)
				_thr_umtx_wake(&tn->curtmr->flags, INT_MAX);
			TIMERS_UNLOCK(curthread);
		}
		atomic_clear_int(&curthread->cancelflags, THR_CANCEL_EXITING);
		longjmp(tn->jbuf, 1);
	}
}

static void *
service_loop(void *arg)
{
	struct pthread *curthread = _get_curthread();
	struct thread_node *tn = arg;
	struct timer *tmr;
	siginfo_t si;
	sigset_t set;

	/*
	 * service thread should not be killed by callback, if user
	 * tries to do so, the thread will be restarted.
	 */
	setjmp(tn->jbuf);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	sigemptyset(&set);
	sigaddset(&set, SIGTIMER);
	THR_CLEANUP_PUSH(curthread, cleanup_thread, tn);
	while (tn->exit == 0) {
		if (__predict_false(__sys_sigwaitinfo(&set, &si) == -1 ||
			si.si_code != SI_TIMER))
			continue;
		TIMERS_LOCK(curthread);
		if (si.si_timerid >= 0 && si.si_timerid < timer_max &&
		    (tmr = timer_list[si.si_timerid]) != NULL &&
		    si.si_value.sigval_int == tmr->gen) {
			tmr->flags |= WORKING;
			TIMERS_UNLOCK(curthread);
			tn->curtmr = tmr;
			tmr->function(&tmr->value, si.si_overrun);
			tn->curtmr = NULL;
			TIMERS_LOCK(curthread);
			tmr->flags &= ~WORKING;
			if (tmr->flags & WANTED)
				_thr_umtx_wake(&tmr->flags, INT_MAX);
		}
		TIMERS_UNLOCK(curthread);
	}
	THR_CLEANUP_POP(curthread, 1);
	return (0);
}
