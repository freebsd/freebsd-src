/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

/* Prototypes: */
static void	thread_sig_handle_special(int sig);

static void	thread_sig_add(struct pthread *pthread, int sig, int has_args);
static void	thread_sig_check_state(struct pthread *pthread, int sig);
static struct pthread *thread_sig_find(int sig);
static void	thread_sigframe_add(struct pthread *thread, int sig,
		    int has_args);
static void	thread_sigframe_save(struct pthread *thread,
		    struct pthread_state_data *psd);
static void	thread_sigframe_restore(struct pthread *thread,
		    struct pthread_state_data *psd);

/* #define DEBUG_SIGNAL */
#ifdef DEBUG_SIGNAL
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

/*
 * Dispatch a signal to a thread, if appropriate.
 */
void
_thread_sig_dispatch(int sig)
{
	struct pthread	*pthread;

	DBG_MSG(">>> _thread_sig_dispatch(%d)\n", sig);

	thread_sig_handle_special(sig);
	if (sigismember(&_thread_sigmask, sig))
		/* Don't deliver the signal if it's masked. */
		return;
	/* Mask the signal until it's handled.  */
	sigaddset(&_thread_sigmask, sig);
	/* This signal will be handled; clear the pending flag. */
	sigdelset(&_thread_sigpending, sig);

	/*
	 * Deliver the signal to a thread.
	 */
	if ((pthread = thread_sig_find(sig)) == NULL) {
		DBG_MSG("No thread to handle signal %d\n", sig);
		return;
	}
	DBG_MSG("Got signal %d, selecting thread %p\n", sig, pthread);
	thread_sig_add(pthread, sig, /*has_args*/ 1);
}

/*
 * Find a thread that can handle the signal.
 */
static struct pthread *
thread_sig_find(int sig)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread	*pthread, *pthread_next;
	struct pthread	*suspended_thread, *signaled_thread;

	DBG_MSG("Looking for thread to handle signal %d\n", sig);
	/* Check if the signal requires a dump of thread information: */
	if (sig == SIGINFO) {
		/* Dump thread information to file: */
		_thread_dump_info();

		/* Unblock this signal to allow further dumps: */
		sigdelset(&_thread_sigmask, sig);
	}

	/*
	 * Enter a loop to look for threads that have the signal
	 * unmasked.  POSIX specifies that a thread in a sigwait
	 * will get the signal over any other threads.  Second
	 * preference will be threads in in a sigsuspend.  Third
	 * preference will be the current thread.  If none of the
	 * above, then the signal is delivered to the first thread
	 * that is found.  Note that if a custom handler is not
	 * installed, the signal only affects threads in sigwait.
	 */
	suspended_thread = NULL;
	if ((curthread != &_thread_kern_thread) &&
	    !sigismember(&curthread->mailbox.tm_context.uc_sigmask, sig))
		signaled_thread = curthread;
	else
		signaled_thread = NULL;

	for (pthread = TAILQ_FIRST(&_waitingq);
	    pthread != NULL; pthread = pthread_next) {
		/*
		 * Grab the next thread before possibly destroying
		 * the link entry.
		 */
		pthread_next = TAILQ_NEXT(pthread, pqe);

		if ((pthread->state == PS_SIGWAIT) &&
		    sigismember(pthread->data.sigwait, sig)) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);

			/*
			 * A signal handler is not invoked for threads
			 * in sigwait.  Clear the blocked and pending
			 * flags.
			 */
			sigdelset(&_thread_sigmask, sig);
			sigdelset(&_thread_sigpending, sig);

			/* Return the signal number: */
			pthread->signo = sig;

			/*
			 * POSIX doesn't doesn't specify which thread
			 * will get the signal if there are multiple
			 * waiters, so we give it to the first thread
			 * we find.
			 *
			 * Do not attempt to deliver this signal
			 * to other threads and do not add the signal
			 * to the process pending set.
			 */
			return (NULL);
		}
		if (!sigismember(
			&pthread->mailbox.tm_context.uc_sigmask, sig) &&
		    ((pthread->flags & PTHREAD_FLAGS_SUSPENDED) == 0)) {
			if (pthread->state == PS_SIGSUSPEND) {
				if (suspended_thread == NULL)
					suspended_thread = pthread;
			} else if (signaled_thread == NULL)
				signaled_thread = pthread;
		}
	}

	/*
	 * If we didn't find a thread in the waiting queue,
	 * check the all threads queue:
	 */
	if (suspended_thread == NULL &&
	    signaled_thread == NULL) {
		/*
		 * Enter a loop to look for other threads
		 * capable of receiving the signal:
		 */
		TAILQ_FOREACH(pthread, &_thread_list, tle) {
			if (!sigismember(
			    &pthread->mailbox.tm_context.uc_sigmask, sig)) {
				signaled_thread = pthread;
				break;
			}
		}
	}

	if (suspended_thread == NULL &&
	    signaled_thread == NULL)
		/*
		 * Add it to the set of signals pending
		 * on the process:
		 */
		sigaddset(&_thread_sigpending, sig);
	else {
		/*
		 * We only deliver the signal to one thread;
		 * give preference to the suspended thread:
		 */
		if (suspended_thread != NULL)
			pthread = suspended_thread;
		else
			pthread = signaled_thread;
		return (pthread);
	}

	/* Returns nothing. */
	return (NULL);
}

#if __XXX_NOT_YET__
void
_thread_sig_check_pending(struct pthread *pthread)
{
	sigset_t	sigset;
	int		i;

	/*
	 * Check if there are pending signals for the running
	 * thread or process that aren't blocked:
	 */
	sigset = pthread->sigpend;
	SIGSETOR(sigset, _thread_sigpending);
	SIGSETNAND(sigset, pthread->sigmask);
	SIGSETNAND(sigset, _thread_sigmask);
	if (SIGNOTEMPTY(sigset)) {
		for (i = 1; i < NSIG; i++) {
			if (sigismember(&sigset, i) != 0) {
				if (sigismember(&pthread->sigpend, i) != 0)
					thread_sig_add(pthread, i,
					    /*has_args*/ 0);
				else {
					thread_sig_add(pthread, i,
					    /*has_args*/ 1);
					sigdelset(&_thread_sigpending, i);
				}
			}
		}
	}
}
#endif

#if __XXX_NOT_YET__
/*
 * This can only be called from the kernel scheduler.  It assumes that
 * all thread contexts are saved and that a signal frame can safely be
 * added to any user thread.
 */
void
_thread_sig_handle_pending(void)
{
	struct pthread	*pthread;
	int		sig;

	/*
	 * Check the array of pending signals:
	 */
	for (sig = 1; sig <= NSIG; sig++) {
		if (sigismember(&_thread_sigpending, sig)) {
			/* This signal is no longer pending. */
			sigdelset(&_thread_sigpending, sig);
			/* Some signals need special handling. */
			thread_sig_handle_special(sig);
			/* Deliver the signal. */
			if (sigismember(&_thread_sigmask, sig)) {
				sigaddset(&_thread_sigmask, sig);
				if ((pthread = thread_sig_find(sig)) != NULL) {
					/*
					 * Setup the target thread to receive
					 * the signal:
					 */
					thread_sig_add(pthread, sig,
					    /*has_args*/ 1);
				}
			}
		}
	}
}
#endif

/*
 * Do special processing to the thread states before we deliver
 * a signal to the application.
 */
static void
thread_sig_handle_special(int sig)
{
	struct pthread	*pthread, *pthread_next;
	int		i;

	switch (sig) {
	case SIGCHLD:
		/*
		 * Enter a loop to wake up all threads waiting
		 * for a process to complete:
		 */
		for (pthread = TAILQ_FIRST(&_waitingq);
		    pthread != NULL; pthread = pthread_next) {
			/*
			 * Grab the next thread before possibly
			 * destroying the link entry:
			 */
			pthread_next = TAILQ_NEXT(pthread, pqe);

			/*
			 * If this thread is waiting for a child
			 * process to complete, wake it up:
			 */
			if (pthread->state == PS_WAIT_WAIT) {
				/* Make the thread runnable: */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);

				/* Return the signal number: */
				pthread->signo = sig;
			}
		}
		break;

	/*
	 * POSIX says that pending SIGCONT signals are
	 * discarded when one of these signals occurs.
	 */
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		/*
		 * Enter a loop to discard pending SIGCONT
		 * signals:
		 */
		TAILQ_FOREACH(pthread, &_thread_list, tle) {
			sigdelset(&pthread->sigpend, SIGCONT);
		}
		break;

	default:
		break;
	}
}

/*
 * Perform thread specific actions in response to a signal.
 * This function is only called if there is a handler installed
 * for the signal, and if the target thread has the signal
 * unmasked.
 */
static void
thread_sig_add(struct pthread *pthread, int sig, int has_args)
{
	int	suppress_handler = 0;
	int	thread_is_active = 0;

	/* Make sure this signal isn't still in the pending set: */
	sigdelset(&pthread->sigpend, sig);

	/*
	 * Process according to thread state:
	 */
	switch (pthread->state) {
	/*
	 * States which do not change when a signal is trapped:
	 */
	case PS_DEAD:
	case PS_DEADLOCK:
	case PS_STATE_MAX:
		/*
		 * You can't call a signal handler for threads in these
		 * states.
		 */
		suppress_handler = 1;
		break;

	/*
	 * States which do not need any cleanup handling when signals
	 * occur:
	 */
	case PS_RUNNING:
		/*
		 * Remove the thread from the queue before changing its
		 * priority:
		 */
		if ((pthread->flags & PTHREAD_FLAGS_IN_PRIOQ) != 0)
			PTHREAD_PRIOQ_REMOVE(pthread);
		else
			/*
			 * This thread is running; avoid placing it in
			 * the run queue:
			 */
			thread_is_active = 1;
		break;

	case PS_SUSPENDED:
		break;

	case PS_SPINBLOCK:
		/* Remove the thread from the workq and waitq: */
		PTHREAD_WORKQ_REMOVE(pthread);
		PTHREAD_WAITQ_REMOVE(pthread);
		/* Make the thread runnable: */
		PTHREAD_SET_STATE(pthread, PS_RUNNING);
		break;

	case PS_SIGWAIT:
		/* The signal handler is not called for threads in SIGWAIT. */
		suppress_handler = 1;
		/* Wake up the thread if the signal is blocked. */
		if (sigismember(pthread->data.sigwait, sig)) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		} else
			/* Increment the pending signal count. */
			sigaddset(&pthread->sigpend, sig);
		break;

	/*
	 * The wait state is a special case due to the handling of
	 * SIGCHLD signals.
	 */
	case PS_WAIT_WAIT:
		if (sig == SIGCHLD) {
			/* Change the state of the thread to run: */
			PTHREAD_WAITQ_REMOVE(pthread);
			PTHREAD_SET_STATE(pthread, PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		}
		else {
			/*
			 * Mark the thread as interrupted only if the
			 * restart flag is not set on the signal action:
			 */
			PTHREAD_WAITQ_REMOVE(pthread);
			PTHREAD_SET_STATE(pthread, PS_RUNNING);
		}
		break;

	/*
	 * States which cannot be interrupted but still require the
	 * signal handler to run:
	 */
	case PS_COND_WAIT:
	case PS_MUTEX_WAIT:
		/*
		 * Remove the thread from the wait queue.  It will
		 * be added back to the wait queue once all signal
		 * handlers have been invoked.
		 */
		PTHREAD_WAITQ_REMOVE(pthread);
		break;

	case PS_JOIN:
		/*
		 * Remove the thread from the wait queue.  It will
		 * be added back to the wait queue once all signal
		 * handlers have been invoked.
		 */
		PTHREAD_WAITQ_REMOVE(pthread);
		/* Make the thread runnable: */
		PTHREAD_SET_STATE(pthread, PS_RUNNING);
		break;

	case PS_SLEEP_WAIT:
		/*
		 * Unmasked signals always cause sleep to terminate early,
		 * regardless of SA_RESTART:
		 */
		pthread->interrupted = 1;
		/* Remove threads in poll and select from the workq: */
		if ((pthread->flags & PTHREAD_FLAGS_IN_WORKQ) != 0)
			PTHREAD_WORKQ_REMOVE(pthread);
		PTHREAD_WAITQ_REMOVE(pthread);
		PTHREAD_SET_STATE(pthread, PS_RUNNING);
		break;

	case PS_SIGSUSPEND:
		PTHREAD_WAITQ_REMOVE(pthread);
		PTHREAD_SET_STATE(pthread, PS_RUNNING);
		break;
	}

	DBG_MSG(">>> suppress_handler = %d, thread_is_active = %d\n",
	    suppress_handler, thread_is_active);

	if (suppress_handler == 0) {
		/* Setup a signal frame and save the current threads state: */
		thread_sigframe_add(pthread, sig, has_args);

		/*
		 * Signals are deferred until just before the threads
		 * signal handler is invoked:
		 */
		pthread->sig_defer_count = 1;

		/* Make sure the thread is runnable: */
		if (pthread->state != PS_RUNNING)
			PTHREAD_SET_STATE(pthread, PS_RUNNING);
		/*
		 * The thread should be removed from all scheduling
		 * queues at this point.  Raise the priority and place
		 * the thread in the run queue.  It is also possible
		 * for a signal to be sent to a suspended thread,
		 * mostly via pthread_kill().  If a thread is suspended,
		 * don't insert it into the priority queue; just set
		 * its state to suspended and it will run the signal
		 * handler when it is resumed.
		 */
		pthread->active_priority |= PTHREAD_SIGNAL_PRIORITY;
		if ((pthread->flags & PTHREAD_FLAGS_SUSPENDED) != 0)
			PTHREAD_SET_STATE(pthread, PS_SUSPENDED);
		else if (thread_is_active == 0)
			PTHREAD_PRIOQ_INSERT_TAIL(pthread);
	}
}

#if __XXX_NOT_YET__
static void
thread_sig_check_state(struct pthread *pthread, int sig)
{
	/*
	 * Process according to thread state:
	 */
	switch (pthread->state) {
	/*
	 * States which do not change when a signal is trapped:
	 */
	case PS_DEAD:
	case PS_DEADLOCK:
	case PS_STATE_MAX:
	case PS_RUNNING:
	case PS_SUSPENDED:
	case PS_SPINBLOCK:
	case PS_COND_WAIT:
	case PS_JOIN:
	case PS_MUTEX_WAIT:
		break;

	case PS_SIGWAIT:
		/* Wake up the thread if the signal is blocked. */
		if (sigismember(pthread->data.sigwait, sig)) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		} else
			/* Increment the pending signal count. */
			sigaddset(&pthread->sigpend, sig);
		break;

	/*
	 * The wait state is a special case due to the handling of
	 * SIGCHLD signals.
	 */
	case PS_WAIT_WAIT:
		if (sig == SIGCHLD) {
			/*
			 * Remove the thread from the wait queue and
			 * make it runnable:
			 */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		}
		break;

	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_SIGSUSPEND:
	case PS_SLEEP_WAIT:
		/*
		 * Remove the thread from the wait queue and make it
		 * runnable:
		 */
		PTHREAD_NEW_STATE(pthread, PS_RUNNING);

		/* Flag the operation as interrupted: */
		pthread->interrupted = 1;
		break;

	/*
	 * These states are additionally in the work queue:
	 */
	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
	case PS_FILE_WAIT:
	case PS_POLL_WAIT:
	case PS_SELECT_WAIT:
		/*
		 * Remove the thread from the wait and work queues, and
		 * make it runnable:
		 */
		PTHREAD_WORKQ_REMOVE(pthread);
		PTHREAD_NEW_STATE(pthread, PS_RUNNING);

		/* Flag the operation as interrupted: */
		pthread->interrupted = 1;
		break;
	}
}
#endif

#if __XXX_NOT_YET__
/*
 * Send a signal to a specific thread (ala pthread_kill):
 */
void
_thread_sig_send(struct pthread *pthread, int sig)
{
	struct pthread	*curthread = _get_curthread();

	/* Check for signals whose actions are SIG_DFL: */
	if (_thread_sigact[sig - 1].sa_handler == SIG_DFL) {
		/*
		 * Check to see if a temporary signal handler is
		 * installed for sigwaiters:
		 */
		if (_thread_dfl_count[sig] == 0)
			/*
			 * Deliver the signal to the process if a handler
			 * is not installed:
			 */
			kill(getpid(), sig);
		/*
		 * Assuming we're still running after the above kill(),
		 * make any necessary state changes to the thread:
		 */
		thread_sig_check_state(pthread, sig);
	}
	/*
	 * Check that the signal is not being ignored:
	 */
	else if (_thread_sigact[sig - 1].sa_handler != SIG_IGN) {
		if (pthread->state == PS_SIGWAIT &&
		    sigismember(pthread->data.sigwait, sig)) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
	
			/* Return the signal number: */
			pthread->signo = sig;
		} else if (sigismember(&pthread->sigmask, sig))
			/* Add the signal to the pending set: */
			sigaddset(&pthread->sigpend, sig);
		else if (pthread == curthread)
			/* Call the signal handler for the current thread: */
			thread_sig_invoke_handler(sig, NULL, NULL);
		else {
			/* Protect the scheduling queues: */
			_thread_kern_sig_defer();
			/*
			 * Perform any state changes due to signal
			 * arrival:
			 */
			thread_sig_add(pthread, sig, /* has args */ 0);
			/* Unprotect the scheduling queues: */
			_thread_kern_sig_undefer();
		}
	}
}
#endif

/*
 * User thread signal handler wrapper.
 *
 *   thread - current running thread
 */
void
_thread_sig_wrapper(int sig, siginfo_t *info, ucontext_t *context)
{
	struct pthread_state_data psd;
	struct pthread	*thread = _get_curthread();
	__siginfohandler_t *handler;

	/* Save the thread's previous state. */
	thread_sigframe_save(thread, &psd);

	/* Check the threads previous state: */
	if (psd.psd_state != PS_RUNNING) {
		/*
		 * Do a little cleanup handling for those threads in
		 * queues before calling the signal handler.  Signals
		 * for these threads are temporarily blocked until
		 * after cleanup handling.
		 */
		switch (psd.psd_state) {
		case PS_COND_WAIT:
			_cond_wait_backout(thread);
			psd.psd_state = PS_RUNNING;
			break;

		case PS_MUTEX_WAIT:
			_mutex_lock_backout(thread);
			psd.psd_state = PS_RUNNING;
			break;

		default:
			break;
		}
	}

	/* Unblock the signal in case we don't return from the handler. */
	/*
 	 * XXX - This is totally bogus. We need to lock the signal mask
         * somehow.
	 */
	sigdelset(&_thread_sigmask, sig);

	/*
	 * Lower the priority before calling the handler in case
	 * it never returns (longjmps back):
	 */
	thread->active_priority &= ~PTHREAD_SIGNAL_PRIORITY;

	/*
	 * Reenable interruptions without checking for the need to
	 * context switch.
	 */
	thread->sig_defer_count = 0;

	if (_thread_sigact[sig -1].sa_handler != NULL) {
		handler = (__siginfohandler_t *)
			_thread_sigact[sig - 1].sa_handler;
		handler(sig, info, context);
	}

        /* Restore the signal frame. */
        thread_sigframe_restore(thread, &psd);

        /* The signal mask was restored; check for any pending signals. */
        /* XXX - thread->check_pending = 1; */
}

static void
thread_sigframe_add(struct pthread *thread, int sig, int has_args)
{
	struct pthread_signal_frame *psf = NULL;
	unsigned long	stackp;

	/* Add a signal frame to the stack, pointing to our signal wrapper. */
	signalcontext(&thread->mailbox.tm_context, sig,
	    (__sighandler_t *)_thread_sig_wrapper);

	/* Setup the new signal mask. */
	SIGSETOR(thread->mailbox.tm_context.uc_sigmask,
	    _thread_sigact[sig - 1].sa_mask);
	sigaddset(&thread->mailbox.tm_context.uc_sigmask, sig);
#if 0
	/* Set up the new frame. */
	thread->curframe = psf;
	thread->flags &= PTHREAD_FLAGS_PRIVATE | PTHREAD_FLAGS_TRACE |
	    PTHREAD_FLAGS_IN_SYNCQ;

	/*
	 * Set up the context:
	 */
	stackp -= sizeof(double);
	_setjmp(thread->ctx.jb);
	SET_STACK_JB(thread->ctx.jb, stackp);
	SET_RETURN_ADDR_JB(thread->ctx.jb, _thread_sig_wrapper);
#endif
}

static void
thread_sigframe_restore(struct pthread *thread, struct pthread_state_data *psd)
{
	thread->wakeup_time = psd->psd_wakeup_time;
	thread->data = psd->psd_wait_data;
	thread->state = psd->psd_state;
	thread->flags = psd->psd_flags;
	thread->interrupted = psd->psd_interrupted;
	thread->sig_defer_count = psd->psd_sig_defer_count;
}

static void
thread_sigframe_save(struct pthread *thread, struct pthread_state_data *psd)
{
	psd->psd_wakeup_time = thread->wakeup_time;
	psd->psd_wait_data = thread->data;
	psd->psd_state = thread->state;
	psd->psd_flags = thread->flags &
	    (PTHREAD_FLAGS_PRIVATE | PTHREAD_FLAGS_TRACE);
	psd->psd_interrupted = thread->interrupted;
	psd->psd_sig_defer_count = thread->sig_defer_count;
}

void
_thread_sig_handler(int sig, siginfo_t *info, ucontext_t *context)
{

	/* Nothing. */
}
