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
#include "thr_private.h"

/* #define DEBUG_SIGNAL */
#ifdef DEBUG_SIGNAL
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

__weak_reference(_sigprocmask, sigprocmask);

int
_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t new;

	/*
	 * Make sure applications can't unblock our synchronization
	 * signal. We always want to take this with sigwait().
	 */
	if (set != NULL) {
		new = *set;
		switch (how) {
		case SIG_BLOCK:
		case SIG_SETMASK:
			SIGADDSET(new, SIGTHR);
			break;
		case SIG_UNBLOCK:
			SIGDELSET(new, SIGTHR);
			break;
		default:
			break;
		}
		set = &new;
	}

	return (__sys_sigprocmask(how, set, oset));
}

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

	if (_thread_initial == NULL)
		_thread_init();
	return (thr_kill(pthread->thr_id, sig));
}

/*
 * User thread signal handler wrapper.
 */
void
_thread_sig_wrapper(int sig, siginfo_t *info, ucontext_t *context)
{
	struct pthread_state_data psd;
	__siginfohandler_t *handler;

	GIANT_LOCK(curthread);
	/* Save the thread's previous state. */
	psd.psd_wait_data = curthread->data;
	psd.psd_state = curthread->state;
	psd.psd_flags = curthread->flags;

	/*
	 * Do a little cleanup handling for those threads in
	 * queues before calling the signal handler.  Signals
	 * for these threads are temporarily blocked until
	 * after cleanup handling.
	 */
	switch (psd.psd_state) {
	case PS_COND_WAIT:
		_cond_wait_backout(curthread);
		psd.psd_state = PS_RUNNING;
		break;

	case PS_MUTEX_WAIT:
		_mutex_lock_backout(curthread);
		psd.psd_state = PS_RUNNING;
		break;

	default:
		break;
	}

	if (_thread_sigact[sig -1].sa_handler != NULL) {
		GIANT_UNLOCK(curthread);
		handler = (__siginfohandler_t *)
			_thread_sigact[sig - 1].sa_handler;
		handler(sig, info, context);
		GIANT_LOCK(curthread);
	}

        /* Restore the signal frame. */
	curthread->data = psd.psd_wait_data;
	curthread->state = psd.psd_state;
	curthread->flags = psd.psd_flags &
	    (PTHREAD_FLAGS_PRIVATE | PTHREAD_FLAGS_TRACE);
	GIANT_UNLOCK(curthread);
}
