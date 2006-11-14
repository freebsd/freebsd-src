/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * Copyright (C) 2003 Daniel M. Eischen <deischen@freebsd.org>
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

#include <sys/types.h>
#include <sys/signalvar.h>
#include <pthread.h>

#include "thr_private.h"

/*#define DEBUG_THREAD_KERN */
#ifdef DEBUG_THREAD_KERN
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

/*
 * This is called when the first thread (other than the initial
 * thread) is created.
 */
int
_thr_setthreaded(int threaded)
{
	if (((threaded == 0) ^ (__isthreaded == 0)) == 0)
		return (0);

	__isthreaded = threaded;
	if (threaded != 0) {
		_thr_rtld_init();
	} else {
		_thr_rtld_fini();
	}
	return (0);
}

void
_thr_signal_block(struct pthread *curthread)
{
	sigset_t set;
	
	if (curthread->sigblock > 0) {
		curthread->sigblock++;
		return;
	}
	SIGFILLSET(set);
	SIGDELSET(set, SIGBUS);
	SIGDELSET(set, SIGILL);
	SIGDELSET(set, SIGFPE);
	SIGDELSET(set, SIGSEGV);
	SIGDELSET(set, SIGTRAP);
	__sys_sigprocmask(SIG_BLOCK, &set, &curthread->sigmask);
	curthread->sigblock++;
}

void
_thr_signal_unblock(struct pthread *curthread)
{
	if (--curthread->sigblock == 0)
		__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);
}

int
_thr_send_sig(struct pthread *thread, int sig)
{
	return thr_kill(thread->tid, sig);
}

void
_thr_assert_lock_level()
{
	PANIC("locklevel <= 0");
}
