/*
 * Copyright (c) 2003 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2003 Jonathan Mini <mini@freebsd.org>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <setjmp.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "thr_private.h"

/* #define DEBUG_SIGNAL */
#ifdef DEBUG_SIGNAL
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

__weak_reference(_pthread_sigmask, pthread_sigmask);

int
_pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	int error;

	/*
	 * This always sets the mask on the current thread.
	 */	
	error = sigprocmask(how, set, oset);

	/*
	 * pthread_sigmask returns errno or success while sigprocmask returns
	 * -1 and sets errno.
	 */
	if (error == -1)
		error = errno;

	return (error);
}


__weak_reference(_pthread_kill, pthread_kill);

int
_pthread_kill(pthread_t pthread, int sig)
{
	int error;

	if (sig < 0 || sig > NSIG)
		return (EINVAL);
	if (_thread_initial == NULL)
		_thread_init();
	error = _find_thread(pthread);
	if (error != 0)
		return (error);

	/*
	 * A 0 signal means do error-checking but don't send signal.
	 */
	if (sig == 0)
		return (0);

	return (thr_kill(pthread->thr_id, sig));
}
