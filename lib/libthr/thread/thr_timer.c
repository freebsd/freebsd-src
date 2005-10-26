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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/signalvar.h>

#include "thr_private.h"

struct timer {
	int		signo;
	union sigval	value;
	void		(*function)(union sigval *);
	int		exit;
	int		timerid;
	umtx_t		lock;
};

extern int __sys_timer_create(clockid_t clockid, struct sigevent *evp,
	timer_t *timerid);

static void *timer_loop(void *arg);

__weak_reference(__timer_create, timer_create);
__weak_reference(__timer_create, _timer_create);

#define	SIGTIMER	SIGCANCEL	/* Reuse SIGCANCEL */

/*
 * Purpose of the function is to implement POSIX timer's
 * SEGEV_THREAD notification mechanism.
 */
int
__timer_create(clockid_t clockid, struct sigevent *evp, timer_t *timerid)
{
	struct pthread *curthread = _get_curthread();
	pthread_attr_t attr;
	struct sigevent ev;
	struct timer *tmr;
	pthread_t newtd;
	sigset_t set, oset;
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
	tmr->signo = SIGTIMER;
	tmr->value = evp->sigev_value;
	tmr->function = evp->sigev_notify_function;
	tmr->exit = 0;
	tmr->timerid = -1;
	_thr_umtx_init(&tmr->lock);
	pthread_attr_init(&attr);
	if (evp->sigev_notify_attributes != NULL) {
		*attr = **(pthread_attr_t *)(evp->sigev_notify_attributes);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	}
	/*
	 * Lock the mutex, so new thread can not continue until
	 * we have fully setup it.
	 */
	THR_UMTX_LOCK(curthread, &tmr->lock);

	/*
	 * Block signal the timer will fire for new thread, new thread
	 * will use sigwaitinfo, signal action should not be invoked.
	 * 
	 * Application user: if you want set signal mask for the
	 * background thread, please call sigprocmask for current before
	 * calling timer_create, this way, the signal mask will be inherited
	 * by new thread.
	 */
	SIGEMPTYSET(set);
	SIGADDSET(set, tmr->signo);
	__sys_sigprocmask(SIG_BLOCK, &set, &oset);
	ret = pthread_create(&newtd, &attr, timer_loop, tmr);
	__sys_sigprocmask(SIG_SETMASK, &oset, NULL);
	pthread_attr_destroy(&attr);
	if (__predict_false(ret != 0)) {
		THR_UMTX_UNLOCK(curthread, &tmr->lock);
		free(tmr);
		errno = ret;
		return (-1);
	}
	/*
	 * Build a new sigevent, and tell kernel to deliver
	 * SIGTIMER signal to the new thread.
	 */
	ev.sigev_notify = SIGEV_THREAD_ID;
	ev.sigev_signo = tmr->signo;
	ev.sigev_notify_thread_id = (int)newtd->tid;
	ev.sigev_value.sigval_ptr = tmr;
	ret = __sys_timer_create(clockid, &ev, timerid);
	if (ret != 0) {
		ret = errno;
		tmr->exit = 1;
		THR_UMTX_UNLOCK(curthread, &tmr->lock);
		pthread_join(newtd, NULL);
		errno = ret;
		return (-1);
	}
	tmr->timerid = *timerid;
	/*
	 * As specification says, the service thread should run in
	 * detached state, so you lose control of the thread!
	 */
	pthread_detach(newtd);
	THR_UMTX_UNLOCK(curthread, &tmr->lock);
	return (0);
}

/* Thread function to serve SEGEV_THREAD notifcation. */
static void *
timer_loop(void *arg)
{
	struct pthread *curthread = _get_curthread();
	struct timer *tmr = arg;
	siginfo_t si;
	sigset_t set;

	THR_UMTX_LOCK(curthread, &tmr->lock);
	THR_UMTX_UNLOCK(curthread, &tmr->lock);
	SIGEMPTYSET(set);
	SIGADDSET(set, tmr->signo);
	while (tmr->exit == 0) {
		if (__sys_sigwaitinfo(&set, &si) != -1) {
			if (si.si_code == SI_TIMER &&
			    si.si_timerid == tmr->timerid)
				tmr->function(&tmr->value);
		}
	}
	free(tmr);
	return (0);
}
