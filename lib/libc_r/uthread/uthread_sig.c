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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <setjmp.h>
#include <string.h>
#include <pthread.h>
#include "pthread_private.h"

/* Prototypes: */
static void	thread_sig_add(struct pthread *pthread, int sig, int has_args);
static void	thread_sig_check_state(struct pthread *pthread, int sig);
static struct pthread *thread_sig_find(int sig);
static void	thread_sig_handle_special(int sig);
static void	thread_sigframe_add(struct pthread *thread, int sig,
		    int has_args);
static void	thread_sigframe_save(struct pthread *thread,
		    struct pthread_signal_frame *psf);
static void	thread_sig_invoke_handler(int sig, siginfo_t *info,
		    ucontext_t *ucp);

/*#define DEBUG_SIGNAL*/
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
	struct pthread	*curthread = _get_curthread();
	struct pthread	*pthread, *pthread_h;
	int		in_sched = _thread_kern_in_sched;
	char		c;

	if (ucp == NULL)
		PANIC("Thread signal handler received null context");
	DBG_MSG("Got signal %d, current thread %p\n", sig, curthread);

	/* Check if an interval timer signal: */
	if (sig == _SCHED_SIGNAL) {
		/* Update the scheduling clock: */
		gettimeofday((struct timeval *)&_sched_tod, NULL);
		_sched_ticks++;

		if (in_sched != 0) {
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
		else if (curthread->sig_defer_count > 0)
			curthread->yield_on_sig_undefer = 1;
		else {
			/* Schedule the next thread: */
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
	else if ((in_sched != 0) || (curthread->sig_defer_count > 0)) {
		/* Cast the signal number to a character variable: */
		c = sig;

		/*
		 * Write the signal number to the kernel pipe so that it will
		 * be ready to read when this signal handler returns.
		 */
		if (_queue_signals != 0) {
			__sys_write(_thread_kern_pipe[1], &c, 1);
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
		/*
		 * The signal is not blocked; handle the signal.
		 *
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

		pthread_h = NULL;
		if ((pthread = thread_sig_find(sig)) == NULL)
			DBG_MSG("No thread to handle signal %d\n", sig);
		else if (pthread == curthread) {
			/*
			 * Unblock the signal and restore the process signal
			 * mask in case we don't return from the handler:
			 */
			_thread_sigq[sig - 1].blocked = 0;
			__sys_sigprocmask(SIG_SETMASK, &_process_sigmask, NULL);

			/* Call the signal handler for the current thread: */
			thread_sig_invoke_handler(sig, info, ucp);

			/*
			 * Set the process signal mask in the context; it
			 * could have changed by the handler.
 			 */
			ucp->uc_sigmask = _process_sigmask;
 
			/* Resume the interrupted thread: */
			__sys_sigreturn(ucp);
		} else {
			DBG_MSG("Got signal %d, adding frame to thread %p\n",
			    sig, pthread);

			/* Setup the target thread to receive the signal: */
			thread_sig_add(pthread, sig, /*has_args*/ 1);

			/* Take a peek at the next ready to run thread: */
			pthread_h = PTHREAD_PRIOQ_FIRST();
			DBG_MSG("Finished adding frame, head of prio list %p\n",
			    pthread_h);
		}
		SIG_SET_INACTIVE();

		/*
		 * Switch to a different context if the currently running
		 * thread takes a signal, or if another thread takes a
		 * signal and the currently running thread is not in a
		 * signal handler.
		 */
		if ((pthread_h != NULL) &&
		    (pthread_h->active_priority > curthread->active_priority)) {
			/* Enter the kernel scheduler: */
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
thread_sig_invoke_handler(int sig, siginfo_t *info, ucontext_t *ucp)
 {
	struct pthread	*curthread = _get_curthread();
	void (*sigfunc)(int, siginfo_t *, void *);
	int		saved_seqno;
	sigset_t	saved_sigmask;

	/* Invoke the signal handler without going through the scheduler:
	 */
	DBG_MSG("Got signal %d, calling handler for current thread %p\n",
	    sig, curthread);

	/* Save the threads signal mask: */
	saved_sigmask = curthread->sigmask;
	saved_seqno = curthread->sigmask_seqno;
 
	/* Setup the threads signal mask: */
	SIGSETOR(curthread->sigmask, _thread_sigact[sig - 1].sa_mask);
	sigaddset(&curthread->sigmask, sig);
 
	/*
	 * Check that a custom handler is installed and if
	 * the signal is not blocked:
	 */
	sigfunc = _thread_sigact[sig - 1].sa_sigaction;
	if (((__sighandler_t *)sigfunc != SIG_DFL) &&
	    ((__sighandler_t *)sigfunc != SIG_IGN)) {
		if (((_thread_sigact[sig - 1].sa_flags & SA_SIGINFO) != 0) ||
		    (info == NULL))
			(*(sigfunc))(sig, info, ucp);
		else
			(*(sigfunc))(sig, (siginfo_t *)info->si_code, ucp);
	}
	/*
	 * Only restore the signal mask if it hasn't been changed by the
	 * application during invocation of the signal handler:
	 */
	if (curthread->sigmask_seqno == saved_seqno)
		curthread->sigmask = saved_sigmask;
}

/*
 * Find a thread that can handle the signal.
 */
struct pthread *
thread_sig_find(int sig)
{
	struct pthread	*curthread = _get_curthread();
	int		handler_installed;
	struct pthread	*pthread, *pthread_next;
	struct pthread	*suspended_thread, *signaled_thread;

	DBG_MSG("Looking for thread to handle signal %d\n", sig);
	/* Check if the signal requires a dump of thread information: */
	if (sig == SIGINFO) {
		/* Dump thread information to file: */
		_thread_dump_info();

		/* Unblock this signal to allow further dumps: */
		_thread_sigq[sig - 1].blocked = 0;
	}
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
		if ((curthread != &_thread_kern_thread) &&
		    !sigismember(&curthread->sigmask, sig))
			signaled_thread = curthread;
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
			    !sigismember(&pthread->sigmask, sig) &&
			    ((pthread->flags & PTHREAD_FLAGS_SUSPENDED) == 0)) {
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
_thread_sig_check_pending(struct pthread *pthread)
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
	struct pthread	*pthread;
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
	struct pthread	*pthread, *pthread_next;
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
				__sys_fcntl(i, F_SETFL,
				    _thread_fd_getflags(i) | O_NONBLOCK);
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
thread_sig_add(struct pthread *pthread, int sig, int has_args)
{
	int	restart;
	int	suppress_handler = 0;
	int	thread_is_active = 0;

	restart = _thread_sigact[sig - 1].sa_flags & SA_RESTART;

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
	case PS_SIGTHREAD:
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

/*
 * User thread signal handler wrapper.
 *
 *   thread - current running thread
 */
void
_thread_sig_wrapper(void)
{
	struct pthread_signal_frame *psf;
	struct pthread	*thread = _get_curthread();

	/* Get the current frame and state: */
	psf = thread->curframe;
	thread->curframe = NULL;
	PTHREAD_ASSERT(psf != NULL, "Invalid signal frame in signal handler");

	/*
	 * We're coming from the kernel scheduler; clear the in
	 * scheduler flag:
	 */
	_thread_kern_in_sched = 0;

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
			break;

		case PS_COND_WAIT:
			_cond_wait_backout(thread);
			psf->saved_state.psd_state = PS_RUNNING;
			break;

		case PS_MUTEX_WAIT:
			_mutex_lock_backout(thread);
			psf->saved_state.psd_state = PS_RUNNING;
			break;

		default:
			break;
		}
	}

	/* Unblock the signal in case we don't return from the handler: */
	_thread_sigq[psf->signo - 1].blocked = 0;

	/*
	 * Lower the priority before calling the handler in case
	 * it never returns (longjmps back):
	 */
	thread->active_priority &= ~PTHREAD_SIGNAL_PRIORITY;

	/*
	 * Reenable interruptions without checking for the need to
	 * context switch:
	 */
	thread->sig_defer_count = 0;

	/*
	 * Dispatch the signal via the custom signal handler:
	 */
	if (psf->sig_has_args == 0)
		thread_sig_invoke_handler(psf->signo, NULL, NULL);
	else
		thread_sig_invoke_handler(psf->signo, &psf->siginfo, &psf->uc);

	/*
	 * Call the kernel scheduler to safely restore the frame and
	 * schedule the next thread:
	 */
	_thread_kern_sched_frame(psf);
}

static void
thread_sigframe_add(struct pthread *thread, int sig, int has_args)
{
	struct pthread_signal_frame *psf = NULL;
	unsigned long	stackp;

 	/* Get the top of the threads stack: */
	stackp = GET_STACK_JB(thread->ctx.jb);

	/*
	 * Leave a little space on the stack and round down to the
	 * nearest aligned word:
	 */
	stackp -= sizeof(double);
	stackp &= ~0x3UL;

	/* Allocate room on top of the stack for a new signal frame: */
	stackp -= sizeof(struct pthread_signal_frame);

	psf = (struct pthread_signal_frame *) stackp;

	/* Save the current context in the signal frame: */
	thread_sigframe_save(thread, psf);

	/* Set handler specific information: */
	psf->sig_has_args = has_args;
	psf->signo = sig;
	if (has_args) {
		/* Copy the signal handler arguments to the signal frame: */
		memcpy(&psf->uc, &_thread_sigq[psf->signo - 1].uc,
		    sizeof(psf->uc));
		memcpy(&psf->siginfo, &_thread_sigq[psf->signo - 1].siginfo,
		    sizeof(psf->siginfo));
	}

	/* Setup the signal mask: */
	SIGSETOR(thread->sigmask, _thread_sigact[sig - 1].sa_mask);
	sigaddset(&thread->sigmask, sig);

	/* Set up the new frame: */
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
}

void
_thread_sigframe_restore(struct pthread *thread,
    struct pthread_signal_frame *psf)
{
	memcpy(&thread->ctx, &psf->ctx, sizeof(thread->ctx));
	/*
	 * Only restore the signal mask if it hasn't been changed
	 * by the application during invocation of the signal handler:
	 */
	if (thread->sigmask_seqno == psf->saved_state.psd_sigmask_seqno)
		thread->sigmask = psf->saved_state.psd_sigmask;
	thread->curframe = psf->saved_state.psd_curframe;
	thread->wakeup_time = psf->saved_state.psd_wakeup_time;
	thread->data = psf->saved_state.psd_wait_data;
	thread->state = psf->saved_state.psd_state;
	thread->flags = psf->saved_state.psd_flags;
	thread->interrupted = psf->saved_state.psd_interrupted;
	thread->signo = psf->saved_state.psd_signo;
	thread->sig_defer_count = psf->saved_state.psd_sig_defer_count;
}

static void
thread_sigframe_save(struct pthread *thread, struct pthread_signal_frame *psf)
{
	memcpy(&psf->ctx, &thread->ctx, sizeof(thread->ctx));
	psf->saved_state.psd_sigmask = thread->sigmask;
	psf->saved_state.psd_curframe = thread->curframe;
	psf->saved_state.psd_wakeup_time = thread->wakeup_time;
	psf->saved_state.psd_wait_data = thread->data;
	psf->saved_state.psd_state = thread->state;
	psf->saved_state.psd_flags = thread->flags &
	    (PTHREAD_FLAGS_PRIVATE | PTHREAD_FLAGS_TRACE);
	psf->saved_state.psd_interrupted = thread->interrupted;
	psf->saved_state.psd_sigmask_seqno = thread->sigmask_seqno;
	psf->saved_state.psd_signo = thread->signo;
	psf->saved_state.psd_sig_defer_count = thread->sig_defer_count;
}

