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
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Prototypes: */
static void	thread_sig_add(pthread_t pthread, int sig, int has_args);
static pthread_t thread_sig_find(int sig);
static void	thread_sig_handle_special(int sig);
static void	thread_sig_savecontext(pthread_t pthread, ucontext_t *ucp);
static void	thread_sigframe_add(pthread_t thread, int sig);
static void	thread_sigframe_leave(pthread_t thread, int frame);
static void	thread_sigframe_restore(pthread_t thread, struct pthread_signal_frame *psf);
static void	thread_sigframe_save(pthread_t thread, struct pthread_signal_frame *psf);

/* #define DEBUG_SIGNAL */
#ifdef DEBUG_SIGNAL
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

#if defined(_PTHREADS_INVARIANTS)
#define SIG_SET_ACTIVE()	_sig_in_handler = 1
#define SIG_SET_INACTIVE()	_sig_in_handler = 0
#else
#define SIG_SET_ACTIVE()
#define SIG_SET_INACTIVE()
#endif

void
_thread_sig_handler(int sig, siginfo_t *info, ucontext_t *ucp)
{
	pthread_t	pthread;
	int		current_frame;
	char		c;

	if (ucp == NULL)
		PANIC("Thread signal handler received null context");
	DBG_MSG("Got signal %d, current thread %p\n", sig, _thread_run);

	/* Check if an interval timer signal: */
	if (sig == _SCHED_SIGNAL) {
		/* Update the scheduling clock: */
		gettimeofday((struct timeval *)&_sched_tod, NULL);
		_sched_ticks++;

		if (_thread_kern_in_sched != 0) {
			/*
			 * The scheduler is already running; ignore this
			 * signal.
			 */
		}
		/*
		 * Check if the scheduler interrupt has come when
		 * the currently running thread has deferred thread
		 * signals.
		 */
		else if (_thread_run->sig_defer_count > 0)
			_thread_run->yield_on_sig_undefer = 1;
		else {
			/*
			 * Save the context of the currently running thread:
			 */
			thread_sig_savecontext(_thread_run, ucp);

			/*
			 * Schedule the next thread. This function is not
			 * expected to return because it will do a longjmp
			 * instead. 
			 */
			_thread_kern_sched(ucp);

			/*
			 * This point should not be reached, so abort the
			 * process: 
			 */
			PANIC("Returned to signal function from scheduler");
		}
	}
	/*
	 * Check if the kernel has been interrupted while the scheduler
	 * is accessing the scheduling queues or if there is a currently
	 * running thread that has deferred signals.
	 */
	else if ((_thread_kern_in_sched != 0) ||
	    (_thread_run->sig_defer_count > 0)) {
		/* Cast the signal number to a character variable: */
		c = sig;

		/*
		 * Write the signal number to the kernel pipe so that it will
		 * be ready to read when this signal handler returns.
		 */
		if (_queue_signals != 0) {
			_thread_sys_write(_thread_kern_pipe[1], &c, 1);
			DBG_MSG("Got signal %d, queueing to kernel pipe\n", sig);
		}
		if (_thread_sigq[sig - 1].blocked == 0) {
			DBG_MSG("Got signal %d, adding to _thread_sigq\n", sig);
			/*
			 * Do not block this signal; it will be blocked
			 * when the pending signals are run down.
			 */
			/* _thread_sigq[sig - 1].blocked = 1; */

			/*
			 * Queue the signal, saving siginfo and sigcontext
			 * (ucontext).
			 *
			 * XXX - Do we need to copy siginfo and ucp?
			 */
			_thread_sigq[sig - 1].signo = sig;
			if (info != NULL)
				memcpy(&_thread_sigq[sig - 1].siginfo, info,
				    sizeof(*info));
			memcpy(&_thread_sigq[sig - 1].uc, ucp, sizeof(*ucp));

			/* Indicate that there are queued signals: */
			_thread_sigq[sig - 1].pending = 1;
			_sigq_check_reqd = 1;
		}
		/* These signals need special handling: */
		else if (sig == SIGCHLD || sig == SIGTSTP ||
		    sig == SIGTTIN || sig == SIGTTOU) {
			_thread_sigq[sig - 1].pending = 1;
			_thread_sigq[sig - 1].signo = sig;
			_sigq_check_reqd = 1;
		}
		else
			DBG_MSG("Got signal %d, ignored.\n", sig);
	}
	/*
	 * The signal handlers should have been installed so that they
	 * cannot be interrupted by other signals.
	 */
	else if (_thread_sigq[sig - 1].blocked == 0) {
		/* The signal is not blocked; handle the signal: */
		current_frame = _thread_run->sigframe_count;

		/*
		 * Ignore subsequent occurrences of this signal
		 * until the current signal is handled:
		 */
		_thread_sigq[sig - 1].blocked = 1;

		/* This signal will be handled; clear the pending flag: */
		_thread_sigq[sig - 1].pending = 0;

		/*
		 * Save siginfo and sigcontext (ucontext).
		 *
		 * XXX - Do we need to copy siginfo and ucp?
		 */
		_thread_sigq[sig - 1].signo = sig;

		if (info != NULL)
			memcpy(&_thread_sigq[sig - 1].siginfo, info,
			    sizeof(*info));
		memcpy(&_thread_sigq[sig - 1].uc, ucp, sizeof(*ucp));
		SIG_SET_ACTIVE();

		/* Handle special signals: */
		thread_sig_handle_special(sig);

		if ((pthread = thread_sig_find(sig)) != NULL) {
			DBG_MSG("Got signal %d, adding frame to thread %p\n",
			    sig, pthread);
			/*
			 * A thread was found that can handle the signal.
			 * Save the context of the currently running thread
			 * so that we can switch to another thread without
			 * losing track of where the current thread left off.
			 * This also applies if the current thread is the
			 * thread to be signaled.
			 */
			thread_sig_savecontext(_thread_run, ucp);

			/* Setup the target thread to receive the signal: */
			thread_sig_add(pthread, sig, /*has_args*/ 1);

			/* Take a peek at the next ready to run thread: */
			pthread = PTHREAD_PRIOQ_FIRST();
			DBG_MSG("Finished adding frame, head of prio list %p\n",
			    pthread);
		}
		else
			DBG_MSG("No thread to handle signal %d\n", sig);
		SIG_SET_INACTIVE();

		/*
		 * Switch to a different context if the currently running
		 * thread takes a signal, or if another thread takes a
		 * signal and the currently running thread is not in a
		 * signal handler.
		 */
		if ((_thread_run->sigframe_count > current_frame) ||
		    ((pthread != NULL) &&
		    (pthread->active_priority > _thread_run->active_priority))) {
			/* Enter the kernel scheduler: */
			DBG_MSG("Entering scheduler from signal handler\n");
			_thread_kern_sched(ucp);
		}
	}
	else {
		SIG_SET_ACTIVE();
		thread_sig_handle_special(sig);
		SIG_SET_INACTIVE();
	}
}

static void
thread_sig_savecontext(pthread_t pthread, ucontext_t *ucp)
{
	struct pthread_signal_frame	*psf;

	psf = _thread_run->curframe;

	memcpy(&psf->ctx.uc, ucp, sizeof(*ucp));

	/* XXX - Save FP registers too? */
	FP_SAVE_UC(&psf->ctx.uc);

	/* Mark the context saved as a ucontext: */
	psf->ctxtype = CTX_UC;
}

/*
 * Find a thread that can handle the signal.
 */
pthread_t
thread_sig_find(int sig)
{
	int		handler_installed;
	pthread_t	pthread, pthread_next;
	pthread_t	suspended_thread, signaled_thread;

	DBG_MSG("Looking for thread to handle signal %d\n", sig);
	/* Check if the signal requires a dump of thread information: */
	if (sig == SIGINFO)
		/* Dump thread information to file: */
		_thread_dump_info();

	/* Check if an interval timer signal: */
	else if (sig == _SCHED_SIGNAL) {
		/*
		 * This shouldn't ever occur (should this panic?).
		 */
	} else {
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
		if ((_thread_run != &_thread_kern_thread) &&
		    !sigismember(&_thread_run->sigmask, sig))
			signaled_thread = _thread_run;
		else
			signaled_thread = NULL;
		if ((_thread_sigact[sig - 1].sa_handler == SIG_IGN) ||
		    (_thread_sigact[sig - 1].sa_handler == SIG_DFL))
			handler_installed = 0;
		else
			handler_installed = 1;

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
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				/*
				 * A signal handler is not invoked for threads
				 * in sigwait.  Clear the blocked and pending
				 * flags.
				 */ 
				_thread_sigq[sig - 1].blocked = 0;
				_thread_sigq[sig - 1].pending = 0;

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
			else if ((handler_installed != 0) &&
			    !sigismember(&pthread->sigmask, sig)) {
				if (pthread->state == PS_SIGSUSPEND) {
					if (suspended_thread == NULL)
						suspended_thread = pthread;
				} else if (signaled_thread == NULL)
					signaled_thread = pthread;
			}
		}

		/*
		 * Only perform wakeups and signal delivery if there is a
		 * custom handler installed:
		 */
		if (handler_installed == 0) {
			/*
			 * There is no handler installed.  Unblock the
			 * signal so that if a handler _is_ installed, any
			 * subsequent signals can be handled.
			 */
			_thread_sigq[sig - 1].blocked = 0;
		} else {
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
					if (!sigismember(&pthread->sigmask,
					    sig)) {
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
				sigaddset(&_process_sigpending, sig);
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
		}
	}

	/* Returns nothing. */
	return (NULL);
}

void
_thread_sig_check_pending(pthread_t pthread)
{
	sigset_t	sigset;
	int		i;

	/*
	 * Check if there are pending signals for the running
	 * thread or process that aren't blocked:
	 */
	sigset = pthread->sigpend;
	SIGSETOR(sigset, _process_sigpending);
	SIGSETNAND(sigset, pthread->sigmask);
	if (SIGNOTEMPTY(sigset)) {
		for (i = 1; i < NSIG; i++) {
			if (sigismember(&sigset, i) != 0) {
				if (sigismember(&pthread->sigpend, i) != 0)
					thread_sig_add(pthread, i,
					    /*has_args*/ 0);
				else {
					thread_sig_add(pthread, i,
					    /*has_args*/ 1);
					sigdelset(&_process_sigpending, i);
				}
			}
		}
	}
}

/*
 * This can only be called from the kernel scheduler.  It assumes that
 * all thread contexts are saved and that a signal frame can safely be
 * added to any user thread.
 */
void
_thread_sig_handle_pending(void)
{
	pthread_t	pthread;
	int		i, sig;

	PTHREAD_ASSERT(_thread_kern_in_sched != 0,
	    "_thread_sig_handle_pending called from outside kernel schedule");
	/*
	 * Check the array of pending signals:
	 */
	for (i = 0; i < NSIG; i++) {
		if (_thread_sigq[i].pending != 0) {
			/* This signal is no longer pending. */
			_thread_sigq[i].pending = 0;

			sig = _thread_sigq[i].signo;

			/* Some signals need special handling: */
			thread_sig_handle_special(sig);

			if (_thread_sigq[i].blocked == 0) {
				/*
				 * Block future signals until this one
				 * is handled:
				 */
				_thread_sigq[i].blocked = 1;

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

static void
thread_sig_handle_special(int sig)
{
	pthread_t	pthread, pthread_next;
	int		i;

	switch (sig) {
	case SIGCHLD:
		/*
		 * Go through the file list and set all files
		 * to non-blocking again in case the child
		 * set some of them to block. Sigh.
		 */
		for (i = 0; i < _thread_dtablesize; i++) {
			/* Check if this file is used: */
			if (_thread_fd_table[i] != NULL) {
				/*
				 * Set the file descriptor to non-blocking:
				 */
				_thread_sys_fcntl(i, F_SETFL,
				    _thread_fd_table[i]->flags | O_NONBLOCK);
			}
		}
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
thread_sig_add(pthread_t pthread, int sig, int has_args)
{
	int	restart, frame;
	int	block_signals = 0;
	int	suppress_handler = 0;

	restart = _thread_sigact[sig - 1].sa_flags & SA_RESTART;

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
	case PS_SIGTHREAD:
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
			if (restart == 0)
				pthread->interrupted = 1;
			PTHREAD_WAITQ_REMOVE(pthread);
			PTHREAD_SET_STATE(pthread, PS_RUNNING);
		}
		break;

	/*
	 * States which cannot be interrupted but still require the
	 * signal handler to run:
	 */
	case PS_COND_WAIT:
	case PS_JOIN:
	case PS_MUTEX_WAIT:
		/*
		 * Remove the thread from the wait queue.  It will
		 * be added back to the wait queue once all signal
		 * handlers have been invoked.
		 */
		PTHREAD_WAITQ_REMOVE(pthread);
		break;

	/*
	 * States which are interruptible but may need to be removed
	 * from queues before any signal handler is called.
	 *
	 * XXX - We may not need to handle this condition, but will
	 *       mark it as a potential problem.
	 */
	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_FILE_WAIT:
		if (restart == 0)
			pthread->interrupted = 1;
		/*
		 * Remove the thread from the wait queue.  Our
		 * signal handler hook will remove this thread
		 * from the fd or file queue before invoking
		 * the actual handler.
		 */
		PTHREAD_WAITQ_REMOVE(pthread);
		/*
		 * To ensure the thread is removed from the fd and file
		 * queues before any other signal interrupts it, set the
		 * signal mask to block all signals.  As soon as the thread
		 * is removed from the queue the signal mask will be
		 * restored.
		 */
		block_signals = 1;
		break;

	/*
	 * States which are interruptible:
	 */
	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
		if (restart == 0) {
			/*
			 * Flag the operation as interrupted and
			 * set the state to running:
			 */
			pthread->interrupted = 1;
			PTHREAD_SET_STATE(pthread, PS_RUNNING);
		}
		PTHREAD_WORKQ_REMOVE(pthread);
		PTHREAD_WAITQ_REMOVE(pthread);
		break;

	case PS_POLL_WAIT:
	case PS_SELECT_WAIT:
	case PS_SLEEP_WAIT:
		/*
		 * Unmasked signals always cause poll, select, and sleep
		 * to terminate early, regardless of SA_RESTART:
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

	if (suppress_handler == 0) {
		/*
		 * Save the current state of the thread and add a
		 * new signal frame.
		 */
		frame = pthread->sigframe_count;
		thread_sigframe_save(pthread, pthread->curframe);
		thread_sigframe_add(pthread, sig);
		pthread->sigframes[frame + 1]->sig_has_args = has_args;
		SIGSETOR(pthread->sigmask, _thread_sigact[sig - 1].sa_mask);
		if (block_signals != 0) {
			/* Save the signal mask and block all signals: */
			pthread->sigframes[frame + 1]->saved_state.psd_sigmask =
			    pthread->sigmask;
			sigfillset(&pthread->sigmask);
		}
		
		/* Make sure the thread is runnable: */
		if (pthread->state != PS_RUNNING)
			PTHREAD_SET_STATE(pthread, PS_RUNNING);
		/*
		 * The thread should be removed from all scheduling
		 * queues at this point.  Raise the priority and place
		 * the thread in the run queue.
		 */
		pthread->active_priority |= PTHREAD_SIGNAL_PRIORITY;
		if (pthread != _thread_run)
			PTHREAD_PRIOQ_INSERT_TAIL(pthread);
	}
}

/*
 * Send a signal to a specific thread (ala pthread_kill):
 */
void
_thread_sig_send(pthread_t pthread, int sig)
{
	/*
	 * Check that the signal is not being ignored:
	 */
	if (_thread_sigact[sig - 1].sa_handler != SIG_IGN) {
		if (pthread->state == PS_SIGWAIT &&
		    sigismember(pthread->data.sigwait, sig)) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		} else if (pthread == _thread_run) {
			/* Add the signal to the pending set: */
			sigaddset(&pthread->sigpend, sig);
			/*
			 * Deliver the signal to the process if a
			 * handler is not installed:
			 */
			if (_thread_sigact[sig - 1].sa_handler == SIG_DFL)
				kill(getpid(), sig);
			if (!sigismember(&pthread->sigmask, sig)) {
				/*
				 * Call the kernel scheduler which will safely
				 * install a signal frame for this thread:
				 */
				_thread_kern_sched_sig();
			}
		} else {
			if (pthread->state != PS_SIGWAIT &&
		 	   !sigismember(&pthread->sigmask, sig)) {
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
			else
				/* Increment the pending signal count. */
				sigaddset(&pthread->sigpend,sig);

			/*
			 * Deliver the signal to the process if a
			 * handler is not installed:
			 */
			if (_thread_sigact[sig - 1].sa_handler == SIG_DFL)
				kill(getpid(), sig);
		}
	}
}

/*
 * User thread signal handler wrapper.
 *
 *   thread - current running thread
 */
void
_thread_sig_wrapper(void)
{
	void (*sigfunc)(int, siginfo_t *, void *);
	struct pthread_signal_frame *psf;
	pthread_t thread;
	int	dead = 0;
	int	i, sig, has_args;
	int	frame, dst_frame;

	thread = _thread_run;

	/* Get the current frame and state: */
	frame = thread->sigframe_count;
	PTHREAD_ASSERT(frame > 0, "Invalid signal frame in signal handler");
	psf = thread->curframe;

	/* Check the threads previous state: */ 
	if (psf->saved_state.psd_state != PS_RUNNING) {
		/*
		 * Do a little cleanup handling for those threads in
		 * queues before calling the signal handler.  Signals
		 * for these threads are temporarily blocked until
		 * after cleanup handling.
		 */
		switch (psf->saved_state.psd_state) {
		case PS_FDLR_WAIT:
		case PS_FDLW_WAIT:
			_fd_lock_backout(thread);
			psf->saved_state.psd_state = PS_RUNNING;
			/* Reenable signals: */
			thread->sigmask = psf->saved_state.psd_sigmask;
			break;

		case PS_FILE_WAIT:
			_flockfile_backout(thread);
			psf->saved_state.psd_state = PS_RUNNING;
			/* Reenable signals: */
			thread->sigmask = psf->saved_state.psd_sigmask;
			break;

		default:
			break;
		}
	}

	/*
	 * Unless the thread exits or longjmps out of the signal handler,
	 * return to the previous frame:
	 */
	dst_frame = frame - 1;

	/*
	 * Check that a custom handler is installed and if the signal
	 * is not blocked:
	 */
	sigfunc = _thread_sigact[psf->signo - 1].sa_sigaction;
	if (((__sighandler_t *)sigfunc != SIG_DFL) &&
	    ((__sighandler_t *)sigfunc != SIG_IGN)) {
		/*
		 * The signal jump buffer is allocated off the stack.
		 * If the signal handler tries to [_][sig]longjmp() or
		 * setcontext(), our wrapped versions of these routines
		 * will copy the user supplied jump buffer or context
		 * to the destination signal frame, set the destination
		 * signal frame in psf->dst_frame, and _longjmp() back
		 * to here.
		 */
		jmp_buf	jb;

		/*
		 * Set up the context for abnormal returns out of signal
		 * handlers.
		 */
		psf->sig_jb = &jb;
		if (_setjmp(jb) == 0) {
			DBG_MSG("_thread_sig_wrapper: Entering frame %d, "
			    "stack 0x%lx\n", frame, GET_STACK_JB(jb));
			/*
			 * Invalidate the destination frame before calling
			 * the signal handler.
			 */
			psf->dst_frame = -1;

			/*
			 * Dispatch the signal via the custom signal
			 * handler:
			 */
			if (psf->sig_has_args == 0)
				(*(sigfunc))(psf->signo, NULL, NULL);
			else if ((_thread_sigact[psf->signo - 1].sa_flags &
			    SA_SIGINFO) != 0)
				(*(sigfunc))(psf->signo,
				    &_thread_sigq[psf->signo - 1].siginfo,
				    &_thread_sigq[psf->signo - 1].uc);
			else
				(*(sigfunc))(psf->signo,
				    (siginfo_t *)_thread_sigq[psf->signo - 1].siginfo.si_code,
				    &_thread_sigq[psf->signo - 1].uc);
		}
		else {
			/*
			 * The return from _setjmp() should only be non-zero
			 * when the signal handler wants to xxxlongjmp() or
			 * setcontext() to a different context, or if the
			 * thread has exited (via pthread_exit).
			 */
			/*
			 * Grab a copy of the destination frame before it
			 * gets clobbered after unwinding.
			 */
			dst_frame = psf->dst_frame;
			DBG_MSG("Abnormal exit from handler for signal %d, "
			    "frame %d\n", psf->signo, frame);

			/* Has the thread exited? */
			if ((dead = thread->flags & PTHREAD_EXITING) != 0)
				/* When exiting, unwind to frame 0. */
				dst_frame = 0;
			else if ((dst_frame < 0) || (dst_frame > frame))
				PANIC("Attempt to unwind to invalid "
				    "signal frame");

			/* Unwind to the target frame: */
			for (i = frame; i > dst_frame; i--) {
				DBG_MSG("Leaving frame %d, signal %d\n", i,
				    thread->sigframes[i]->signo);
				/* Leave the current signal frame: */
				thread_sigframe_leave(thread, i);

				/*
				 * Save whatever is needed out of the state
				 * data; as soon as the frame count is
				 * is decremented, another signal can arrive
				 * and corrupt this view of the state data.
				 */
				sig = thread->sigframes[i]->signo;
				has_args = thread->sigframes[i]->sig_has_args;

				/*
				 * We're done with this signal frame:
				 */
				thread->curframe = thread->sigframes[i - 1];
				thread->sigframe_count = i - 1;

				/*
				 * Only unblock the signal if it was a
				 * process signal as opposed to a signal
				 * generated by pthread_kill().
				 */
				if (has_args != 0)
					_thread_sigq[sig - 1].blocked = 0;
			}
		}
	}

	/*
	 * Call the kernel scheduler to schedule the next
	 * thread.
	 */
	if (dead == 0) {
		/* Restore the threads state: */
		thread_sigframe_restore(thread, thread->sigframes[dst_frame]);
		_thread_kern_sched_frame(dst_frame);
	}
	else {
		PTHREAD_ASSERT(dst_frame == 0,
		    "Invalid signal frame for dead thread");

		/* Perform any necessary cleanup before exiting. */
		thread_sigframe_leave(thread, 0);

		/* This should never return: */
		_thread_exit_finish();
		PANIC("Return from _thread_exit_finish in signal wrapper");
	}
}

static void
thread_sigframe_add(pthread_t thread, int sig)
{
	unsigned long	stackp = 0;

	/* Get the top of the threads stack: */
	switch (thread->curframe->ctxtype) {
	case CTX_JB:
	case CTX_JB_NOSIG:
		stackp = GET_STACK_JB(thread->curframe->ctx.jb);
		break;
	case CTX_SJB:
		stackp = GET_STACK_SJB(thread->curframe->ctx.sigjb);
		break;
	case CTX_UC:
		stackp = GET_STACK_UC(&thread->curframe->ctx.uc);
		break;
	default:
		PANIC("Invalid thread context type");
		break;
	}

	/*
	 * Leave a little space on the stack and round down to the
	 * nearest aligned word:
	 */
	stackp -= sizeof(double);
	stackp &= ~0x3UL;

	/* Allocate room on top of the stack for a new signal frame: */
	stackp -= sizeof(struct pthread_signal_frame);

	/* Set up the new frame: */
	thread->sigframe_count++;
	thread->sigframes[thread->sigframe_count] =
	   (struct pthread_signal_frame *) stackp;
	thread->curframe = thread->sigframes[thread->sigframe_count];
	thread->curframe->stackp = stackp;
	thread->curframe->ctxtype = CTX_JB_NOSIG;
	thread->curframe->longjmp_val = 1;
	thread->curframe->signo = sig;

	/*
	 * Set up the context:
	 */
	_setjmp(thread->curframe->ctx.jb);
	SET_STACK_JB(thread->curframe->ctx.jb, stackp);
	SET_RETURN_ADDR_JB(thread->curframe->ctx.jb, _thread_sig_wrapper);
}

/*
 * Locate the signal frame from the specified stack pointer.
 */
int
_thread_sigframe_find(pthread_t pthread, void *stackp)
{
	int	frame;

	/*
	 * Find the destination of the target frame based on the
	 * given stack pointer.
	 */
	for (frame = pthread->sigframe_count; frame >= 0; frame--) {
		if (stackp < (void *)pthread->sigframes[frame]->stackp)
			break;
	}
	return (frame);
}
 
void
thread_sigframe_leave(pthread_t thread, int frame)
{
	struct pthread_state_data	*psd;

	psd = &thread->sigframes[frame]->saved_state;

	/*
	 * Perform any necessary cleanup for this signal frame:
	 */
	switch (psd->psd_state) {
	case PS_DEAD:
	case PS_DEADLOCK:
	case PS_RUNNING:
	case PS_SIGTHREAD:
	case PS_STATE_MAX:
	case PS_SUSPENDED:
		break;

	/*
	 * Threads in the following states need to be removed
	 * from queues.
	 */
	case PS_COND_WAIT:
		_cond_wait_backout(thread);
		if ((psd->psd_flags & PTHREAD_FLAGS_IN_WAITQ) != 0)
			PTHREAD_WAITQ_REMOVE(thread);
		break;

	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
		_fd_lock_backout(thread);
		if ((psd->psd_flags & PTHREAD_FLAGS_IN_WAITQ) != 0)
			PTHREAD_WAITQ_REMOVE(thread);
		break;

	case PS_FILE_WAIT:
		_flockfile_backout(thread);
		if ((psd->psd_flags & PTHREAD_FLAGS_IN_WAITQ) != 0)
			PTHREAD_WAITQ_REMOVE(thread);
		break;

	case PS_JOIN:
		_join_backout(thread);
		if ((psd->psd_flags & PTHREAD_FLAGS_IN_WAITQ) != 0)
			PTHREAD_WAITQ_REMOVE(thread);
		break;

	case PS_MUTEX_WAIT:
		_mutex_lock_backout(thread);
		if ((psd->psd_flags & PTHREAD_FLAGS_IN_WAITQ) != 0)
			PTHREAD_WAITQ_REMOVE(thread);
		break;

	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
	case PS_POLL_WAIT:
	case PS_SELECT_WAIT:
	case PS_SIGSUSPEND:
	case PS_SIGWAIT:
	case PS_SLEEP_WAIT:
	case PS_SPINBLOCK:
	case PS_WAIT_WAIT:
		if ((psd->psd_flags & PTHREAD_FLAGS_IN_WAITQ) != 0) {
			PTHREAD_WAITQ_REMOVE(thread);
			if ((psd->psd_flags & PTHREAD_FLAGS_IN_WORKQ) != 0)
				PTHREAD_WORKQ_REMOVE(thread);
		}
		break;
	}
}

static void
thread_sigframe_restore(pthread_t thread, struct pthread_signal_frame *psf)
{
	thread->interrupted = psf->saved_state.psd_interrupted;
	thread->sigmask = psf->saved_state.psd_sigmask;
	thread->state = psf->saved_state.psd_state;
	thread->flags = psf->saved_state.psd_flags;
	thread->wakeup_time = psf->saved_state.psd_wakeup_time;
	thread->data = psf->saved_state.psd_wait_data;
}

static void
thread_sigframe_save(pthread_t thread, struct pthread_signal_frame *psf)
{
	psf->saved_state.psd_interrupted = thread->interrupted;
	psf->saved_state.psd_sigmask = thread->sigmask;
	psf->saved_state.psd_state = thread->state;
	psf->saved_state.psd_flags = thread->flags;
	thread->flags &= PTHREAD_FLAGS_PRIVATE | PTHREAD_FLAGS_TRACE |
	    PTHREAD_FLAGS_IN_CONDQ | PTHREAD_FLAGS_IN_MUTEXQ |
	    PTHREAD_FLAGS_IN_JOINQ;
	psf->saved_state.psd_wakeup_time = thread->wakeup_time;
	psf->saved_state.psd_wait_data = thread->data;
}

#endif
