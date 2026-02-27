/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "namespace.h"
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"
#include "libc_private.h"
#include "spinlock.h"

#pragma weak system
int
system(const char *command)
{
	return (((int (*)(const char *))
	    __libc_interposing[INTERPOS_system])(command));
}

int
__libc_system(const char *command)
{
	static spinlock_t lock = _SPINLOCK_INITIALIZER;
	static volatile unsigned long concurrent;
	static struct sigaction ointact, oquitact;
	struct sigaction ign;
	sigset_t sigblock, osigblock;
	int pstat = -1, serrno = 0;
	pid_t pid;

	if (command == NULL)			/* just checking... */
		return (eaccess(_PATH_BSHELL, X_OK) == 0);

	/*
	 * If we are the first concurrent instance, ignore SIGINT and
	 * SIGQUIT.  Block SIGCHLD regardless of concurrency, since on
	 * FreeBSD, sigprocmask() is equivalent to pthread_sigmask().
	 */
	if (__isthreaded)
		_SPINLOCK(&lock);
	if (concurrent++ == 0) {
		memset(&ign, 0, sizeof(ign));
		ign.sa_handler = SIG_IGN;
		sigemptyset(&ign.sa_mask);
		(void)__libc_sigaction(SIGINT, &ign, &ointact);
		(void)__libc_sigaction(SIGQUIT, &ign, &oquitact);
	}
	sigemptyset(&sigblock);
	sigaddset(&sigblock, SIGCHLD);
	(void)__libc_sigprocmask(SIG_BLOCK, &sigblock, &osigblock);
	if (__isthreaded)
		_SPINUNLOCK(&lock);

	/*
	 * Fork the child process.
	 */
	if ((pid = fork()) < 0) {		/* error */
		serrno = errno;
	} else if (pid == 0) {			/* child */
		/*
		 * Restore original signal dispositions.
		 */
		(void)__libc_sigaction(SIGINT, &ointact, NULL);
		(void)__libc_sigaction(SIGQUIT,  &oquitact, NULL);
		(void)__sys_sigprocmask(SIG_SETMASK, &osigblock, NULL);
		/*
		 * Exec the command.
		 */
		execl(_PATH_BSHELL, "sh", "-c", command, NULL);
		_exit(127);
	} else {				/* parent */
		/*
		 * Wait for the child to terminate.
		 */
		while (_wait4(pid, &pstat, 0, NULL) < 0) {
			if (errno != EINTR) {
				serrno = errno;
				break;
			}
		}
	}

	/*
	 * If we are the last concurrent instance, restore original signal
	 * dispositions.  Unblock SIGCHLD, unless it was already blocked.
	 */
	if (__isthreaded)
		_SPINLOCK(&lock);
	if (--concurrent == 0) {
		(void)__libc_sigaction(SIGINT, &ointact, NULL);
		(void)__libc_sigaction(SIGQUIT,  &oquitact, NULL);
	}
	if (!sigismember(&osigblock, SIGCHLD))
		(void)__libc_sigprocmask(SIG_UNBLOCK, &sigblock, NULL);
	if (__isthreaded)
		_SPINUNLOCK(&lock);
	if (serrno != 0)
		errno = serrno;
	return (pstat);
}

__weak_reference(__libc_system, __system);
__weak_reference(__libc_system, _system);
