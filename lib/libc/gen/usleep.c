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
static char sccsid[] = "@(#)usleep.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#ifdef  _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"
#endif

#ifndef _THREAD_SAFE
static int alarm_termination;

static void
sleephandler()
{
	alarm_termination++;
}
#endif	/* _THREAD_SAFE */


void
usleep(useconds)
	unsigned int useconds;
{
#ifdef _THREAD_SAFE
	struct timespec time_to_sleep;
	struct timespec time_remaining;

	if (useconds) {
		time_to_sleep.tv_nsec = (useconds % 1000000) * 1000;
		time_to_sleep.tv_sec = useconds / 1000000;
		do {
			nanosleep(&time_to_sleep, &time_remaining);
			time_to_sleep = time_remaining;
		} while (time_to_sleep.tv_sec != 0 ||
			 time_to_sleep.tv_nsec != 0);
	}
#else
	struct timespec time_to_sleep;
	struct timespec time_remaining;
	struct sigaction act, oact;
	sigset_t mask, omask;

	if (useconds != 0) {
		time_to_sleep.tv_nsec = (useconds % 1000000) * 1000;
		time_to_sleep.tv_sec = useconds / 1000000;

		/*
		 * Set up handler to interrupt signanosleep and ensure
		 * SIGARLM is not blocked.  Block SIGALRM while fiddling
		 * with things.
		 */
		memset(&act, 0, sizeof(act));
		act.sa_handler = sleephandler;
		sigemptyset(&mask);
		sigaddset(&mask, SIGALRM);
		sigprocmask(SIG_BLOCK, &mask, &omask);
		sigaction(SIGALRM, &act, &oact);
		mask = omask;
		sigdelset(&mask, SIGALRM);
		alarm_termination = 0;

		/*
		 * signanosleep() uses the given mask for the lifetime of
		 * the syscall only - it resets on return.  Note that the
		 * old sleep explicitly unblocks SIGALRM during the sleep.
		 */

		do {
			signanosleep(&time_to_sleep, &time_remaining, &mask);
			time_to_sleep = time_remaining;
		} while (!alarm_termination &&
			 (time_to_sleep.tv_sec != 0 ||
			  time_to_sleep.tv_nsec != 0));

		/* Unwind */
		sigaction(SIGALRM, &oact, (struct sigaction *)0);
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *)0);
	}
#endif	/* _THREAD_SAFE */
}
