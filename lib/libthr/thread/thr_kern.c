/*
 * Copyright (c) 2003 Jeffrey Roberson <jeff@freebsd.org>
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
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <sys/time.h>
#include <sys/timespec.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "thr_private.h"

/* XXX Why can't I get this from time.h? :-( */
#define timespecsub(vvp, uvp)                                           \
        do {                                                            \
                (vvp)->tv_sec -= (uvp)->tv_sec;                         \
                (vvp)->tv_nsec -= (uvp)->tv_nsec;                       \
                if ((vvp)->tv_nsec < 0) {                               \
                        (vvp)->tv_sec--;                                \
                        (vvp)->tv_nsec += 1000000000;                   \
                }                                                       \
        } while (0)

static sigset_t restore;

void
GIANT_LOCK(pthread_t pthread)
{
	sigset_t set;
	sigset_t sav;
	int error;

	/*
	 * Block all signals.
	 */
	SIGFILLSET(set);

	/*
	 * We can not use the global 'restore' set until after we have
	 * acquired the giant lock.
	 */
#if 0
	error = __sys_sigprocmask(SIG_SETMASK, &set, &sav);
	if (error) {
		_thread_printf(0, "GIANT_LOCK: sig err %d\n", errno);
		abort();
	}
#endif

	error = umtx_lock(&_giant_mutex, pthread->thr_id);
	if (error) {
		_thread_printf(0, "GIANT_LOCK: %d\n", errno);
		abort();
	}
	
	restore = sav;
}

void
GIANT_UNLOCK(pthread_t pthread)
{
	sigset_t set;
	int error;

	/*
	 * restore is protected by giant.  We could restore our signal state
	 * incorrectly if someone else set restore between unlocking giant
	 * and restoring the signal mask.  To avoid this we cache a copy prior
	 * to the unlock.
	 */
	set = restore;

	error = umtx_unlock(&_giant_mutex, pthread->thr_id);
	if (error) {
		_thread_printf(0, "GIANT_UNLOCK: %d\n", errno);
		abort();
	}

#if 0
	/*
	 * Restore signals.
	 */
	error = __sys_sigprocmask(SIG_SETMASK, &set, NULL);
	if (error) {
		_thread_printf(0, "GIANT_UNLOCK: sig err %d\n", errno);
		abort();
	}
#endif
}

int
_thread_suspend(pthread_t pthread, struct timespec *abstime)
{
	struct timespec remaining;
	struct timespec *ts;
	siginfo_t info;
	sigset_t set;
	int error;

	/*
	 * Catch SIGTHR.
	 */
	SIGFILLSET(set);
	SIGDELSET(set, SIGTHR);

	/*
	 * Compute the remainder of the run time.
	 */
	if (abstime) {
		struct timespec now;
		struct timeval tv;

		GET_CURRENT_TOD(tv);
		TIMEVAL_TO_TIMESPEC(&tv, &now);

		remaining = *abstime;
		timespecsub(&remaining, &now);
		ts = &remaining;
	} else
		ts = NULL;

	error = sigtimedwait(&set, &info, ts);
	if (error == -1)
		error = errno;

	return (error);
}
