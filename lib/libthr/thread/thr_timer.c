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

#include <sys/cdefs.h>
#include <sys/types.h>

#include <mqueue.h>
#include <time.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "thr_private.h"

struct thread_node {
	struct pthread_attr	tn_attr;
	TAILQ_ENTRY(thread_node)tn_link;
	pthread_t		tn_thread;
	int			tn_refcount;
	int			tn_exit;
	jmp_buf			tn_jbuf;
	struct rtobj_node *	tn_curobj;
};

struct rtobj_node {
	LIST_ENTRY(rtobj_node)	rt_link;
	int			rt_type;
	union sigval		rt_value;
	void *			rt_func;
	umtx_t			rt_flags;
	union {
		int	_timerid;
		int	_mqd;
	} _rt_id;
	int			rt_gen;
	struct thread_node *	rt_tn;
};

#define	rt_timerid		_rt_id._timerid
#define	rt_mqd			_rt_id._mqd

LIST_HEAD(rtobj_hash_head, rtobj_node);
#define HASH_QUEUES		17
#define	HASH(t, id)		((((id) << 3) + (t)) % HASH_QUEUES)
static struct rtobj_hash_head	rtobj_hash[HASH_QUEUES];
static int			generation;
static umtx_t			hash_lock;
static TAILQ_HEAD(,thread_node)	threads;
static umtx_t			threads_lock;

static struct thread_node *thread_create(pthread_attr_t *);
static void	thread_release(struct thread_node *);
static struct rtobj_node *rtobj_find(int, int);
static int	rtobj_register(struct rtobj_node *);
static int	rtobj_delete(int, int);
static int	rtobj_delete_obj(struct rtobj_node *);
static void *	service_loop(void *);
static void	timer_dispatch(struct rtobj_node *p, int);

extern int __sys_timer_create(clockid_t, struct sigevent *, timer_t *);
extern int __sys_timer_delete(timer_t);
extern int __sys_mq_notify(mqd_t mqdes, const struct sigevent *);
extern int __mq_close(mqd_t mqd);

__weak_reference(__timer_create, timer_create);
__weak_reference(__timer_create, _timer_create);
__weak_reference(__timer_delete, timer_delete);
__weak_reference(__timer_delete, _timer_delete);
__weak_reference(__mq_notify, mq_notify);
__weak_reference(__mq_notify, _mq_notify);
__weak_reference(___mq_close, mq_close);
__weak_reference(___mq_close, _mq_close);

#define	SIGSERVICE		(SIGCANCEL+1)

#define	RT_WORKING		0x01
#define	RT_WANTED		0x02

#define	HASH_LOCK(t)		THR_LOCK_ACQUIRE((t), &hash_lock)
#define	HASH_UNLOCK(t)		THR_LOCK_RELEASE((t), &hash_lock)

#define	THREADS_LOCK(t)		THR_LOCK_ACQUIRE((t), &threads_lock)
#define	THREADS_UNLOCK(t)	THR_LOCK_RELEASE((t), &threads_lock)

void
_thr_timer_init(void)
{
	int i;

	_thr_umtx_init(&hash_lock);
	_thr_umtx_init(&threads_lock);
	for (i = 0; i < HASH_QUEUES; ++i)
		LIST_INIT(&rtobj_hash[i]);
	TAILQ_INIT(&threads);
}

static struct rtobj_node *
rtobj_alloc(int type, const struct sigevent *evp)
{
	struct rtobj_node *obj;

	obj = calloc(1, sizeof(*obj));
	if (obj != NULL) {
		obj->rt_value = evp->sigev_value;
		obj->rt_func = evp->sigev_notify_function;
		obj->rt_gen = atomic_fetchadd_int(&generation, 1);
		obj->rt_type = type;
	}

	return (obj);
}

static __inline void
rtobj_free(struct rtobj_node *obj)
{
	free(obj);
}

static struct rtobj_node *
rtobj_find(int type, int id)
{
	struct rtobj_node *obj;
	int chain = HASH(type, id);

	LIST_FOREACH(obj, &rtobj_hash[chain], rt_link) {
		if (obj->rt_type == type && obj->rt_mqd == id)
			break;
	}
	return (obj);
}

static int
rtobj_register(struct rtobj_node *obj)
{
	int chain = HASH(obj->rt_type, obj->rt_mqd);

	LIST_INSERT_HEAD(&rtobj_hash[chain], obj, rt_link);
	return (0);
}

static int
rtobj_delete(int type, int id)
{
	struct rtobj_node *obj;

	obj = rtobj_find(type, id);
	if (obj != NULL)
		return (rtobj_delete_obj(obj));
	return (0);
}

static int
rtobj_delete_obj(struct rtobj_node *obj)
{
	struct pthread *curthread = _get_curthread();
	umtx_t flags;

	LIST_REMOVE(obj, rt_link);
	/* If the timer is servicing, allow it to complete. */
	while ((flags = obj->rt_flags) & RT_WORKING) {
		obj->rt_flags |= RT_WANTED;
		HASH_UNLOCK(curthread);
		_thr_umtx_wait(&obj->rt_flags, flags, NULL);
		HASH_LOCK(curthread);
	}
	HASH_UNLOCK(curthread);
	/*
	 * Drop reference count of servicing thread,
	 * may free the thread.
	 */
	thread_release(obj->rt_tn);
	rtobj_free(obj);

	HASH_LOCK(curthread);
	return (0);
}

/*
 * Purpose of the function is to implement POSIX timer's
 * SEGEV_THREAD notification mechanism.
 */
int
__timer_create(clockid_t clockid, struct sigevent *evp, timer_t *timerid)
{
	struct sigevent ev;
	struct rtobj_node *obj;
	struct pthread *curthread;
	int ret, err;

	/* Call syscall directly if it is not SIGEV_THREAD */
	if (evp == NULL || evp->sigev_notify != SIGEV_THREAD)
		return (__sys_timer_create(clockid, evp, timerid));

	/* Otherwise, do all magical things. */
	obj = rtobj_alloc(SI_TIMER, evp);
	if (obj == NULL) {
		errno = EAGAIN;
		return (-1);
	}
	obj->rt_tn = thread_create(evp->sigev_notify_attributes);
	if (obj->rt_tn == NULL) {
		rtobj_free(obj);
		errno = EAGAIN;
		return (-1);
	}

	/*
	 * Build a new sigevent, and tell kernel to deliver SIGSERVICE
	 * signal to the new thread.
	 */
	ev.sigev_notify = SIGEV_THREAD_ID;
	ev.sigev_signo = SIGSERVICE;
	ev.sigev_notify_thread_id = (lwpid_t)obj->rt_tn->tn_thread->tid;
	ev.sigev_value.sival_int = obj->rt_gen;
	ret = __sys_timer_create(clockid, &ev, &obj->rt_timerid);
	if (ret != 0) {
		err = errno;
		thread_release(obj->rt_tn);
		rtobj_free(obj);
		errno = err;
		return (-1);
	}
	curthread = _get_curthread();
	HASH_LOCK(curthread);
	rtobj_register(obj);
	HASH_UNLOCK(curthread);
	*timerid = obj->rt_timerid;
	return (0);
}

int
__timer_delete(timer_t timerid)
{
	struct pthread *curthread = _get_curthread();

	HASH_LOCK(curthread);
	rtobj_delete(SI_TIMER, timerid);
	HASH_UNLOCK(curthread);
	return (__sys_timer_delete(timerid));
}

typedef void (*timer_func)(union sigval val, int timerid, int overrun);

static void
timer_dispatch(struct rtobj_node *obj, int overrun)
{
	timer_func f = obj->rt_func;

	f(obj->rt_value, obj->rt_timerid, overrun);
}

int
___mq_close(mqd_t mqd)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	HASH_LOCK(curthread);
	rtobj_delete(SI_MESGQ, mqd);
	ret = __mq_close(mqd);
	HASH_UNLOCK(curthread);
	return (ret);
}

int
__mq_notify(mqd_t mqd, const struct sigevent *evp)
{
	struct sigevent ev;
	struct rtobj_node *obj;
	struct pthread *curthread;
	int ret, err;

	curthread = _get_curthread();

	HASH_LOCK(curthread);
	rtobj_delete(SI_MESGQ, mqd);
	if (evp == NULL || evp->sigev_notify != SIGEV_THREAD) {
		ret = __sys_mq_notify(mqd, evp);
		HASH_UNLOCK(curthread);
		return (ret);
	}
	HASH_UNLOCK(curthread);

	obj = rtobj_alloc(SI_MESGQ, evp);
	if (obj == NULL) {
		errno = EAGAIN;
		return (-1);
	}
	obj->rt_tn = thread_create(evp->sigev_notify_attributes);
	if (obj->rt_tn == NULL) {
		rtobj_free(obj);
		errno = EAGAIN;
		return (-1);
	}
	obj->rt_mqd = mqd;
	/*
	 * Build a new sigevent, and tell kernel to deliver SIGSERVICE
	 * signal to the new thread.
	 */
	ev.sigev_notify = SIGEV_THREAD_ID;
	ev.sigev_signo = SIGSERVICE;
	ev.sigev_notify_thread_id = (lwpid_t)obj->rt_tn->tn_thread->tid;
	ev.sigev_value.sival_int = obj->rt_gen;
	HASH_LOCK(curthread);
	ret = __sys_mq_notify(mqd, &ev);
	if (ret != 0) {
		err = errno;
		HASH_UNLOCK(curthread);
		thread_release(obj->rt_tn);
		rtobj_free(obj);
		errno = err;
		return (-1);
	}
	rtobj_register(obj);
	HASH_UNLOCK(curthread);
	return (0);
}

typedef void (*mq_func)(union sigval val, int mqd);

static void
mq_dispatch(struct rtobj_node *obj)
{
	mq_func f = obj->rt_func;

	f(obj->rt_value, obj->rt_mqd);
}

static struct thread_node *
thread_create(pthread_attr_t *pattr)
{
	pthread_attr_t attr;
	struct pthread *curthread;
	struct thread_node *tn;
	int ret;

	curthread = _get_curthread();
	pthread_attr_init(&attr);
	if (pattr != NULL) {
		*attr = **(pthread_attr_t *)pattr;
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	}

	THREADS_LOCK(curthread);
	/* Search a thread matching the required pthread_attr. */
	TAILQ_FOREACH(tn, &threads, tn_link) {
		if (attr->stackaddr_attr == NULL) {
			if (attr->sched_policy == tn->tn_attr.sched_policy &&
			    attr->sched_inherit == tn->tn_attr.sched_inherit &&
			    attr->prio == tn->tn_attr.prio &&
			    attr->stacksize_attr == tn->tn_attr.stacksize_attr &&
			    attr->guardsize_attr == tn->tn_attr.guardsize_attr &&
			    ((attr->flags & PTHREAD_SCOPE_SYSTEM) ==
			     (tn->tn_attr.flags & PTHREAD_SCOPE_SYSTEM)))
				break;
		} else {
			/*
			 * Reuse the thread if it has same stack address,
			 * because two threads can not run on same stack.
			 */
			if (attr->stackaddr_attr == tn->tn_attr.stackaddr_attr)
				break;
		}
	}
	if (tn != NULL) {
		tn->tn_refcount++;
		THREADS_UNLOCK(curthread);
		pthread_attr_destroy(&attr);
		return (tn);
	}
	tn = malloc(sizeof(*tn));
	tn->tn_refcount = 1;
	tn->tn_exit = 0;
	tn->tn_attr = *attr;
	tn->tn_curobj = NULL;
	_thr_signal_block(curthread); /* SIGSERVICE is also blocked. */
	TAILQ_INSERT_TAIL(&threads, tn, tn_link);
	ret = _pthread_create(&tn->tn_thread, &attr, service_loop, tn);
	_thr_signal_unblock(curthread);
	if (ret != 0) {
		TAILQ_REMOVE(&threads, tn, tn_link);
		free(tn);
		tn = NULL;
	}
	THREADS_UNLOCK(curthread);
	pthread_attr_destroy(&attr);
	return (tn);
}

static void
thread_release(struct thread_node *tn)
{
	struct pthread *curthread = _get_curthread();

	THREADS_LOCK(curthread);
	tn->tn_refcount--;
#if 0
	if (tn->tn_refcount == 0) {
		struct pthread *th;
		tn->tn_exit = 1;
		th = tn->tn_thread;
		_thr_send_sig(th, SIGSERVICE);
		TAILQ_REMOVE(&threads, tn, tn_link);
	} else
#endif
		tn = NULL;
	THREADS_UNLOCK(curthread);
	if (tn != NULL)
		free(tn);
}

/*
 * This function is called if user callback calls
 * pthread_exit() or pthread_cancel() for the thread.
 */
static void
thread_cleanup(void *arg)
{
	struct pthread *curthread = _get_curthread();
	struct thread_node *tn = arg;

	if (tn->tn_exit == 0) {
		/* broken user code is killing us. */
		if (tn->tn_curobj != NULL) {
			HASH_LOCK(curthread);
			tn->tn_curobj->rt_flags &= ~RT_WORKING;
			if (tn->tn_curobj->rt_flags & RT_WANTED)
				_thr_umtx_wake(&tn->tn_curobj->rt_flags, INT_MAX);
			HASH_UNLOCK(curthread);
		}
		atomic_clear_int(&curthread->cancelflags, THR_CANCEL_EXITING);
		longjmp(tn->tn_jbuf, 1);
	}
}

static void *
service_loop(void *arg)
{
	siginfo_t si;
	sigset_t set;
	struct thread_node *tn;
	struct pthread *curthread;
	struct rtobj_node *obj;

	tn = arg;
	curthread = _get_curthread();
	/*
	 * Service thread should not be killed by callback, if user
	 * tries to do so, the thread will be restarted.
	 */
	setjmp(tn->tn_jbuf);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	sigemptyset(&set);
	sigaddset(&set, SIGCANCEL);
	__sys_sigprocmask(SIG_UNBLOCK, &set, NULL);
	sigdelset(&set, SIGCANCEL);
	sigaddset(&set, SIGSERVICE);
	THR_CLEANUP_PUSH(curthread, thread_cleanup, tn);
	while (tn->tn_exit == 0) {
		if (__predict_false(__sys_sigwaitinfo(&set, &si) == -1 ||
			(si.si_code != SI_TIMER && si.si_code != SI_MESGQ)))
			continue;
		HASH_LOCK(curthread);
		obj = rtobj_find(si.si_code, si.si_timerid);
		if (obj && (obj->rt_gen == si.si_value.sival_int)) {
			obj->rt_flags |= RT_WORKING;
			HASH_UNLOCK(curthread);
			tn->tn_curobj = obj;
			if (si.si_code == SI_TIMER)
				timer_dispatch(obj, si.si_overrun);
			else if (si.si_code == SI_MESGQ)
				mq_dispatch(obj);
			tn->tn_curobj = NULL;
			HASH_LOCK(curthread);
			obj->rt_flags &= ~RT_WORKING;
			if (obj->rt_flags & RT_WANTED)
				_thr_umtx_wake(&obj->rt_flags, INT_MAX);
			else if (si.si_code == SI_MESGQ) {
				/*
				 * mq_notify is oneshot event, should remove
				 * atomatically by the thread.
				 */
				rtobj_delete_obj(obj);
			}
		}
		HASH_UNLOCK(curthread);
	}
	THR_CLEANUP_POP(curthread, 0);
	return (0);
}
