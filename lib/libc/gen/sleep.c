/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)sleep.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#ifdef  _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"
#endif

#define ITIMERMAX 100000000

#ifndef _THREAD_SAFE
static void
sleephandler()
{
	return;
}
#endif	/* _THREAD_SAFE */

unsigned int
sleep(seconds)
	unsigned int seconds;
{
#ifdef _THREAD_SAFE
	unsigned int rest = 0;
	struct timespec time_to_sleep;
	struct timespec time_remaining;

	if (seconds != 0) {
	again:
		/*
		 * XXX
		 * Hack to work around itimerfix(9) gratuitously limiting
		 * the acceptable range for a struct timeval.tv_sec to
		 * <= ITIMERMAX.
		 */
		if (seconds > ITIMERMAX) {
			rest = seconds - ITIMERMAX;
			seconds = ITIMERMAX;
		}
		time_to_sleep.tv_sec = seconds;
		time_to_sleep.tv_nsec = 0;
		nanosleep(&time_to_sleep, &time_remaining);

		if (rest != 0 &&
		    time_remaining.tv_sec == 0 &&
		    time_remaining.tv_nsec == 0) {
			seconds = rest;
			rest = 0;
			goto again;
		}

		rest += time_remaining.tv_sec;
		if (time_remaining.tv_nsec > 0)
			rest++;      /* round up */
	}
	return (rest);
#else
	unsigned int rest = 0;
	struct timespec time_to_sleep;
	struct timespec time_remaining;
	struct sigaction act, oact;
	sigset_t mask, omask;
	int alarm_blocked;

	if (seconds != 0) {

		/* Block SIGALRM while fiddling with it */
		sigemptyset(&mask);
		sigaddset(&mask, SIGALRM);
		sigprocmask(SIG_BLOCK, &mask, &omask);

		/* Was SIGALRM blocked already? */
		alarm_blocked = sigismember(&omask, SIGALRM);

		if (!alarm_blocked) {
			/*
			 * Set up handler to interrupt signanosleep only if
			 * SIGALRM was unblocked. (Save some syscalls)
			 */
			memset(&act, 0, sizeof(act));
			act.sa_handler = sleephandler;
			sigaction(SIGALRM, &act, &oact);
		}

	again:
		/*
		 * XXX
		 * Hack to work around itimerfix(9) gratuitously limiting
		 * the acceptable range for a struct timeval.tv_sec to
		 * <= ITIMERMAX
		 */
		if (seconds > ITIMERMAX) {
			rest = seconds - ITIMERMAX;
			seconds = ITIMERMAX;
		}
		time_to_sleep.tv_sec = seconds;
		time_to_sleep.tv_nsec = 0;

  		/*
  		 * signanosleep() uses the given mask for the lifetime of
  		 * the syscall only - it resets on return.  Note that the
		 * old sleep() explicitly unblocks SIGALRM during the sleep,
		 * we don't do that now since we don't depend on SIGALRM
		 * to end the timeout.  If the process blocks SIGALRM, it
		 * gets what it asks for.
  		 */
		signanosleep(&time_to_sleep, &time_remaining, &omask);

		if (rest != 0 &&
		    time_remaining.tv_sec == 0 &&
		    time_remaining.tv_nsec == 0) {
			seconds = rest;
			rest = 0;
			goto again;
		}

		if (!alarm_blocked) {
			/* Unwind */
			sigaction(SIGALRM, &oact, (struct sigaction *)0);
			sigprocmask(SIG_SETMASK, &omask, (sigset_t *)0);
		}

		/* return how long is left */
		rest += time_remaining.tv_sec;
		if (time_remaining.tv_nsec > 0)
			rest++;      /* round up */
	}
	return (rest);
#endif	/* _THREAD_SAFE */
}
