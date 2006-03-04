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

#include <sys/types.h>
#include <machine/atomic.h>

#include "namespace.h"
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <ucontext.h>
#include <sys/thr.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "un-namespace.h"

#include "sigev_thread.h"

/* Lowest number of worker threads should be kept. */
#define SIGEV_WORKER_LOW		0

/* Highest number of worker threads can be created. */
#define	SIGEV_WORKER_HIGH		20

/* How long an idle worker thread should stay. */
#define SIGEV_WORKER_IDLE		10

struct sigev_worker {
	LIST_ENTRY(sigev_worker)	sw_link;
	pthread_cond_t		sw_cv;
	struct sigev_node	*sw_sn;
	int			sw_flags;
	int			*sw_readyptr;
};

#define	SWF_READYQ		1

LIST_HEAD(sigev_list_head, sigev_node);
#define HASH_QUEUES		17
#define	HASH(t, id)		((((id) << 3) + (t)) % HASH_QUEUES)

static struct sigev_list_head	sigev_hash[HASH_QUEUES];
static struct sigev_list_head	sigev_all;
static TAILQ_HEAD(, sigev_node)	sigev_actq;
static TAILQ_HEAD(, sigev_thread_node)	sigev_threads;
static int			sigev_generation;
static pthread_mutex_t		*sigev_list_mtx;
static pthread_once_t		sigev_once = PTHREAD_ONCE_INIT;
static pthread_once_t		sigev_once_default = PTHREAD_ONCE_INIT;
static pthread_mutex_t		*sigev_threads_mtx;
static pthread_cond_t		*sigev_threads_cv;
static pthread_cond_t		*sigev_actq_cv;
static struct sigev_thread_node	*sigev_default_thread;
static struct sigev_thread_attr	sigev_default_sna;
static pthread_attr_t		sigev_default_attr;
static int			atfork_registered;
static LIST_HEAD(,sigev_worker)	sigev_worker_ready;
static int			sigev_worker_count;
static int			sigev_worker_start;
static int			sigev_worker_high;
static int			sigev_worker_low;
static pthread_cond_t		*sigev_worker_init_cv;

static void __sigev_fork_prepare(void);
static void __sigev_fork_parent(void);
static void __sigev_fork_child(void);
static struct sigev_thread_node *sigev_thread_create(pthread_attr_t *,
	struct sigev_thread_node *, int);
static void *sigev_service_loop(void *);
static void *sigev_worker_routine(void *);
static void sigev_put(struct sigev_node *);
static void worker_cleanup(void *arg);

#pragma weak pthread_create
#pragma weak pthread_attr_getschedpolicy
#pragma weak pthread_attr_getinheritsched
#pragma weak pthread_attr_getschedparam
#pragma weak pthread_attr_getscope
#pragma weak pthread_attr_getstacksize
#pragma weak pthread_attr_getstackaddr
#pragma weak pthread_attr_getguardsize
#pragma weak pthread_attr_init
#pragma weak pthread_attr_setscope
#pragma weak pthread_attr_setdetachstate
#pragma weak pthread_atfork
#pragma weak _pthread_once
#pragma weak pthread_cleanup_push
#pragma weak pthread_cleanup_pop
#pragma weak pthread_setcancelstate

static __inline void
attr2sna(pthread_attr_t *attr, struct sigev_thread_attr *sna)
{
	struct sched_param sched_param;

	pthread_attr_getschedpolicy(attr, &sna->sna_policy);
	pthread_attr_getinheritsched(attr, &sna->sna_inherit);
	pthread_attr_getschedparam(attr, &sched_param);
	sna->sna_prio = sched_param.sched_priority;
	pthread_attr_getstacksize(attr, &sna->sna_stacksize);
	pthread_attr_getstackaddr(attr, &sna->sna_stackaddr);
	pthread_attr_getguardsize(attr, &sna->sna_guardsize);
}

static __inline int
sna_eq(const struct sigev_thread_attr *a, const struct sigev_thread_attr *b)
{
	return memcmp(a, b, sizeof(*a)) == 0;
}

static __inline int
have_threads(void)
{
	return (&pthread_create != NULL);
}

void
__sigev_thread_init(void)
{
	static int inited = 0;
	int i;

	sigev_list_mtx = malloc(sizeof(pthread_mutex_t));
	_pthread_mutex_init(sigev_list_mtx, NULL);
	sigev_threads_mtx = malloc(sizeof(pthread_mutex_t));
	_pthread_mutex_init(sigev_threads_mtx, NULL);
	sigev_threads_cv = malloc(sizeof(pthread_cond_t));
	_pthread_cond_init(sigev_threads_cv, NULL);
	sigev_actq_cv = malloc(sizeof(pthread_cond_t));
	_pthread_cond_init(sigev_actq_cv, NULL);
	for (i = 0; i < HASH_QUEUES; ++i)
		LIST_INIT(&sigev_hash[i]);
	LIST_INIT(&sigev_all);
	TAILQ_INIT(&sigev_threads);
	TAILQ_INIT(&sigev_actq);
	sigev_default_thread = NULL;
	sigev_worker_count = 0;
	sigev_worker_start = 0;
	LIST_INIT(&sigev_worker_ready);
	sigev_worker_high = SIGEV_WORKER_HIGH;
	sigev_worker_low = SIGEV_WORKER_LOW;
	sigev_worker_init_cv = malloc(sizeof(pthread_cond_t));
	_pthread_cond_init(sigev_worker_init_cv, NULL);
	if (atfork_registered == 0) {
		pthread_atfork(
			__sigev_fork_prepare,
			__sigev_fork_parent,
			__sigev_fork_child);
		atfork_registered = 1;
	}
	if (!inited) {
		pthread_attr_init(&sigev_default_attr);
		attr2sna(&sigev_default_attr, &sigev_default_sna);
		pthread_attr_setscope(&sigev_default_attr,
			PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setdetachstate(&sigev_default_attr,
			PTHREAD_CREATE_DETACHED);
		inited = 1;
	}
	sigev_default_thread = sigev_thread_create(NULL, NULL, 0);
}

int
__sigev_check_init(void)
{
	if (!have_threads())
		return (-1);

	_pthread_once(&sigev_once, __sigev_thread_init);
	return (sigev_default_thread != NULL) ? 0 : -1;
}

static void
__sigev_fork_prepare(void)
{
}

static void
__sigev_fork_parent(void)
{
}

static void
__sigev_fork_child(void)
{
	/*
	 * This is a hack, the thread libraries really should
	 * check if the handlers were already registered in
	 * pthread_atfork().
	 */
	atfork_registered = 1;
	memcpy(&sigev_once, &sigev_once_default, sizeof(sigev_once));
	__sigev_thread_init();
}

int
__sigev_list_lock(void)
{
	return _pthread_mutex_lock(sigev_list_mtx);
}

int
__sigev_list_unlock(void)
{
	return _pthread_mutex_unlock(sigev_list_mtx);
}

int
__sigev_thread_list_lock(void)
{
	return _pthread_mutex_lock(sigev_threads_mtx);
}

int
__sigev_thread_list_unlock(void)
{
	return _pthread_mutex_unlock(sigev_threads_mtx);
}

struct sigev_node *
__sigev_alloc(int type, const struct sigevent *evp, struct sigev_node *prev,
	int usethreadpool)
{
	struct sigev_node *sn;

	sn = calloc(1, sizeof(*sn));
	if (sn != NULL) {
		sn->sn_value = evp->sigev_value;
		sn->sn_func = evp->sigev_notify_function;
		sn->sn_gen = atomic_fetchadd_int(&sigev_generation, 1);
		sn->sn_type = type;
		if (usethreadpool)
			sn->sn_flags |= SNF_THREADPOOL;
		sn->sn_tn = sigev_thread_create(evp->sigev_notify_attributes,
			prev ? prev->sn_tn : NULL, usethreadpool);
		if (sn->sn_tn == NULL) {
			free(sn);
			sn = NULL;
		}
	}
	return (sn);
}

void
__sigev_get_sigevent(struct sigev_node *sn, struct sigevent *newevp,
	sigev_id_t id)
{
	/*
	 * Build a new sigevent, and tell kernel to deliver SIGEV_SIGSERVICE
	 * signal to the new thread.
	 */
	newevp->sigev_notify = SIGEV_THREAD_ID;
	newevp->sigev_signo  = SIGEV_SIGSERVICE;
	newevp->sigev_notify_thread_id = (lwpid_t)sn->sn_tn->tn_lwpid;
	newevp->sigev_value.sival_ptr = (void *)id;
}

void
__sigev_free(struct sigev_node *sn)
{
	free(sn);
}

struct sigev_node *
__sigev_find(int type, sigev_id_t id)
{
	struct sigev_node *sn;
	int chain = HASH(type, id);

	LIST_FOREACH(sn, &sigev_hash[chain], sn_link) {
		if (sn->sn_type == type && sn->sn_id == id)
			break;
	}
	return (sn);
}

int
__sigev_register(struct sigev_node *sn)
{
	int chain = HASH(sn->sn_type, sn->sn_id);

	LIST_INSERT_HEAD(&sigev_hash[chain], sn, sn_link);
	LIST_INSERT_HEAD(&sigev_all, sn, sn_allist);
	return (0);
}

int
__sigev_delete(int type, sigev_id_t id)
{
	struct sigev_node *sn;

	sn = __sigev_find(type, id);
	if (sn != NULL)
		return (__sigev_delete_node(sn));
	return (0);
}

int
__sigev_delete_node(struct sigev_node *sn)
{
	LIST_REMOVE(sn, sn_link);
	LIST_REMOVE(sn, sn_allist);

	__sigev_thread_list_lock();
	if (--sn->sn_tn->tn_refcount == 0)
		if (!(sn->sn_flags & SNF_THREADPOOL))
			pthread_kill(sn->sn_tn->tn_thread, SIGEV_SIGSERVICE);
	__sigev_thread_list_unlock();
	if (sn->sn_flags & SNF_WORKING)
		sn->sn_flags |= SNF_REMOVED;
	else {
		if (sn->sn_flags & SNF_ACTQ) {
			TAILQ_REMOVE(&sigev_actq, sn, sn_actq);
		}
		__sigev_free(sn);
	}
	return (0);
}

static
sigev_id_t
sigev_get_id(siginfo_t *si)
{
	switch(si->si_code) {
	case SI_TIMER:
		return (si->si_timerid);
	case SI_MESGQ:
		return (si->si_mqd);
	case SI_ASYNCIO:
		return (sigev_id_t)si->si_value.sival_ptr;
	}
	return (-1);
}

static struct sigev_thread_node *
sigev_thread_create(pthread_attr_t *pattr, struct sigev_thread_node *prev,
	int usepool)
{
	struct sigev_thread_node *tn;
	struct sigev_thread_attr sna;
	sigset_t set;
	int ret;

	if (pattr == NULL)
		pattr = &sigev_default_attr;
	else {
		pthread_attr_setscope(pattr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setdetachstate(pattr, PTHREAD_CREATE_DETACHED);
	}
 
	attr2sna(pattr, &sna);

	__sigev_thread_list_lock();

	if (prev != NULL && sna_eq(&prev->tn_sna, &sna)) {
		prev->tn_refcount++;
		__sigev_thread_list_unlock();
		return (prev);
	}

	if (sna_eq(&sna, &sigev_default_sna) && usepool &&
	    sigev_default_thread != NULL) {
		sigev_default_thread->tn_refcount++;
		__sigev_thread_list_unlock();
		return (sigev_default_thread);
	}

	tn = NULL;
	/* Search a thread matching the required stack address */
	if (sna.sna_stackaddr != NULL) {
		TAILQ_FOREACH(tn, &sigev_threads, tn_link) {
			if (sna.sna_stackaddr == tn->tn_sna.sna_stackaddr)
				break;
		}
	}

	if (tn != NULL) {
		tn->tn_refcount++;
		__sigev_thread_list_unlock();
		return (tn);
	}
	tn = malloc(sizeof(*tn));
	tn->tn_sna = sna;
	tn->tn_cur = NULL;
	tn->tn_lwpid = -1;
	tn->tn_refcount = 1;
	TAILQ_INSERT_TAIL(&sigev_threads, tn, tn_link);
	sigemptyset(&set);
	sigaddset(&set, SIGEV_SIGSERVICE);
	_sigprocmask(SIG_BLOCK, &set, NULL);
	ret = pthread_create(&tn->tn_thread, pattr, sigev_service_loop, tn);
	_sigprocmask(SIG_UNBLOCK, &set, NULL);
	if (ret != 0) {
		TAILQ_REMOVE(&sigev_threads, tn, tn_link);
		__sigev_thread_list_unlock();
		free(tn);
		tn = NULL;
	} else {
		/* wait the thread to get its lwpid */
		while (tn->tn_lwpid == -1)
			_pthread_cond_wait(sigev_threads_cv, sigev_threads_mtx);
		__sigev_thread_list_unlock();
	}
	return (tn);
}

static void
after_dispatch(struct sigev_thread_node *tn)
{
	struct sigev_node *sn;

	if ((sn = tn->tn_cur) != NULL) {
		__sigev_list_lock();
		sn->sn_flags &= ~SNF_WORKING;
		if (sn->sn_flags & SNF_REMOVED)
			__sigev_free(sn);
		else if (sn->sn_flags & SNF_ONESHOT)
			__sigev_delete_node(sn);
		tn->tn_cur = NULL;
		__sigev_list_unlock();
	}
	tn->tn_cur = NULL;
}

/*
 * This function is called if user callback calls
 * pthread_exit() or pthread_cancel() for the thread.
 */
static void
thread_cleanup(void *arg)
{
	struct sigev_thread_node *tn = arg;

	fprintf(stderr, "Dangerous Robinson, calling pthread_exit() from "
			"SIGEV_THREAD is undefined.");
	after_dispatch(tn);
	/* longjmp(tn->tn_jbuf, 1); */
	abort();
}

/*
 * Main notification dispatch function, the function either
 * run user callback by itself or hand off the notifications
 * to worker threads depend on flags.
 */
static void *
sigev_service_loop(void *arg)
{
	siginfo_t si;
	sigset_t set;
	struct sigev_thread_node *tn;
	struct sigev_node *sn;
	sigev_id_t id;
	int ret;

	tn = arg;
	thr_self(&tn->tn_lwpid);
	__sigev_thread_list_lock();
	_pthread_cond_broadcast(sigev_threads_cv);
	__sigev_thread_list_unlock();

	/*
	 * Service thread should not be killed by callback, if user
	 * attempts to do so, the thread will be restarted.
	 */
	setjmp(tn->tn_jbuf);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	sigemptyset(&set);
	sigaddset(&set, SIGEV_SIGSERVICE);
	pthread_cleanup_push(thread_cleanup, tn);
	for (;;) {
		ret = sigwaitinfo(&set, &si);
		__sigev_thread_list_lock();
		if (tn->tn_refcount == 0) {
			TAILQ_REMOVE(&sigev_threads, tn, tn_link);
			__sigev_thread_list_unlock();
			free(tn);
			break;
		}
		__sigev_thread_list_unlock();
		if (ret == -1)
			continue;
		id = sigev_get_id(&si);
		__sigev_list_lock();
		sn = __sigev_find(si.si_code, id);
		if (sn != NULL) {
			sn->sn_info = si;
			if (!(sn->sn_flags & SNF_THREADPOOL)) {
				tn->tn_cur = sn;
				sn->sn_flags |= SNF_WORKING;
				__sigev_list_unlock();
				sn->sn_dispatch(sn);
				after_dispatch(tn);
			} else {
				assert(!(sn->sn_flags & SNF_ACTQ));
				sigev_put(sn);
			}
		} else {
			tn->tn_cur = NULL;
			__sigev_list_unlock();
		}
	}
	pthread_cleanup_pop(0);
	return (0);
}

/*
 * Hand off notifications to worker threads.
 *
 * prerequist: sigev list locked.
 */
static void 
sigev_put(struct sigev_node *sn)
{
	struct sigev_worker *worker;
	pthread_t td;
	int ret, ready;

	TAILQ_INSERT_TAIL(&sigev_actq, sn, sn_actq);
	sn->sn_flags |= SNF_ACTQ;
	/*
	 * check if we should add more worker threads unless quota is hit.
	 */
	if (LIST_EMPTY(&sigev_worker_ready) &&
	    sigev_worker_count + sigev_worker_start < sigev_worker_high) {
		sigev_worker_start++;
		__sigev_list_unlock();
		worker = malloc(sizeof(*worker));
		_pthread_cond_init(&worker->sw_cv, NULL);
		worker->sw_flags = 0;
		worker->sw_sn = 0;
		worker->sw_readyptr = &ready;
		ready = 0;
		ret = pthread_create(&td, &sigev_default_attr,
			sigev_worker_routine, worker);
		if (ret) {
			warnc(ret, "%s:%s can not create worker thread",
				__FILE__, __func__);
			__sigev_list_lock();
			sigev_worker_start--;
			__sigev_list_unlock();
		} else {
			__sigev_list_lock();
			while (ready == 0) {
				_pthread_cond_wait(sigev_worker_init_cv,
					sigev_list_mtx);	
			}
			__sigev_list_unlock();
		}
	} else {
		worker = LIST_FIRST(&sigev_worker_ready);
		if (worker) {
			LIST_REMOVE(worker, sw_link);
			worker->sw_flags &= ~SWF_READYQ;
			_pthread_cond_broadcast(&worker->sw_cv);
			__sigev_list_unlock();
		}
	}
}

/*
 * Background thread to dispatch notification to user code.
 * These threads are not bound to any realtime objects.
 */
static void *
sigev_worker_routine(void *arg)
{
	struct sigev_worker *worker;
	struct sigev_node *sn;
	struct timespec ts;
	int ret;

	worker = arg;
	__sigev_list_lock();
	sigev_worker_count++;
	sigev_worker_start--;
	(*(worker->sw_readyptr))++;
	LIST_INSERT_HEAD(&sigev_worker_ready, worker, sw_link);
	worker->sw_flags |= SWF_READYQ;
	_pthread_cond_broadcast(sigev_worker_init_cv);

	for (;;) {
		if (worker->sw_flags & SWF_READYQ) {
			LIST_REMOVE(worker, sw_link);
			worker->sw_flags &= ~SWF_READYQ;
		}

		sn = TAILQ_FIRST(&sigev_actq);
		if (sn != NULL) {
			TAILQ_REMOVE(&sigev_actq, sn, sn_actq);
			sn->sn_flags &= ~SNF_ACTQ;
			sn->sn_flags |= SNF_WORKING;
			__sigev_list_unlock();

			worker->sw_sn = sn;
			pthread_cleanup_push(worker_cleanup, worker);
			sn->sn_dispatch(sn);
			pthread_cleanup_pop(0);
			worker->sw_sn = NULL;

			__sigev_list_lock();
			sn->sn_flags &= ~SNF_WORKING;
			if (sn->sn_flags & SNF_REMOVED)
				__sigev_free(sn);
			else if (sn->sn_flags & SNF_ONESHOT)
				__sigev_delete_node(sn);
		} else {
			LIST_INSERT_HEAD(&sigev_worker_ready, worker, sw_link);
			worker->sw_flags |= SWF_READYQ;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += SIGEV_WORKER_IDLE;
			ret = _pthread_cond_timedwait(&worker->sw_cv,
				sigev_list_mtx, &ts);
			if (ret == ETIMEDOUT) {
				/*
				 * If we were timeouted and there is nothing
				 * to do, exit the thread.
				 */
				if (TAILQ_EMPTY(&sigev_actq) &&
				    (worker->sw_flags & SWF_READYQ) &&
				    sigev_worker_count > sigev_worker_low)
					goto out;
			}
		}
	}
out:
	if (worker->sw_flags & SWF_READYQ) {
		LIST_REMOVE(worker, sw_link);
		worker->sw_flags &= ~SWF_READYQ;
	}
	sigev_worker_count--;
	__sigev_list_unlock();
	_pthread_cond_destroy(&worker->sw_cv);
	free(worker);
	return (0);
}

static void
worker_cleanup(void *arg)
{
	struct sigev_worker *worker = arg;
	struct sigev_node *sn;

	sn = worker->sw_sn;
	__sigev_list_lock();
	sn->sn_flags &= ~SNF_WORKING;
	if (sn->sn_flags & SNF_REMOVED)
		__sigev_free(sn);
	else if (sn->sn_flags & SNF_ONESHOT)
		__sigev_delete_node(sn);
	sigev_worker_count--;
	__sigev_list_unlock();
	_pthread_cond_destroy(&worker->sw_cv);
	free(worker);
}
