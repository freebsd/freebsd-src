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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_sig.c,v 1.25.2.1 2000/03/22 01:19:16 jasone Exp $
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
static void	thread_sig_check_state(pthread_t pthread, int sig);
static void	thread_sig_finish_longjmp(void *arg);
static void	handle_state_change(pthread_t pthread);


/* Static variables: */
static spinlock_t	signal_lock = _SPINLOCK_INITIALIZER;
static unsigned int	pending_sigs[NSIG];
static unsigned int	handled_sigs[NSIG];
static int		volatile check_pending = 0;
static int		volatile check_waiting = 0;

/* Initialize signal handling facility: */
void
_thread_sig_init(void)
{
	int i;

	/* Clear pending and handled signal counts: */
	for (i = 1; i < NSIG; i++) {
		pending_sigs[i - 1] = 0;
		handled_sigs[i - 1] = 0;
	}

	/* Clear the lock: */
	signal_lock.access_lock = 0;

	/* Clear the process pending signals: */
	sigemptyset(&_process_sigpending);
}

void
_thread_sig_handler(int sig, int code, ucontext_t * scp)
{
	pthread_t pthread, pthread_next;
	int	i;
	char	c;

	/* Check if an interval timer signal: */
	if (sig == _SCHED_SIGNAL) {
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
			 * Schedule the next thread. This function is not
			 * expected to return because it will do a longjmp
			 * instead. 
			 */
			_thread_kern_sched(scp);

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
	else if ((_queue_signals != 0) || ((_thread_kern_in_sched == 0) &&
	    (_thread_run->sig_defer_count > 0))) {
		/* Cast the signal number to a character variable: */
		c = sig;

		/*
		 * Write the signal number to the kernel pipe so that it will
		 * be ready to read when this signal handler returns.
		 */
		_thread_sys_write(_thread_kern_pipe[1], &c, 1);

		/* Indicate that there are queued signals in the pipe. */
		_sigq_check_reqd = 1;
	} else {
		if (_atomic_lock(&signal_lock.access_lock)) {
			/* There is another signal handler running: */
			pending_sigs[sig - 1]++;
			check_pending = 1;
		}
		else {
			/* It's safe to handle the signal now. */
			pthread = _thread_sig_handle(sig, scp);

			/* Reset the pending and handled count back to 0: */
			pending_sigs[sig - 1] = 0;
			handled_sigs[sig - 1] = 0;

			if (pthread == NULL)
				signal_lock.access_lock = 0;
			else {
				sigaddset(&pthread->sigmask, sig);

				/*
				 * Make sure not to deliver the same signal to
				 * the thread twice.  sigpend is potentially
				 * modified by the call chain
				 * _thread_sig_handle() -->
				 * thread_sig_check_state(), which can happen
				 * just above.
				 */
				if (sigismember(&pthread->sigpend, sig))
					sigdelset(&pthread->sigpend, sig);

				signal_lock.access_lock = 0;
				_thread_sig_deliver(pthread, sig);
				sigdelset(&pthread->sigmask, sig);
			}
		}

		/* Enter a loop to process pending signals: */
		while ((check_pending != 0) &&
		    (_atomic_lock(&signal_lock.access_lock) == 0)) {
			check_pending = 0;
			for (i = 1; i < NSIG; i++) {
				if (pending_sigs[i - 1] > handled_sigs[i - 1]) {
					pending_sigs[i - 1] = handled_sigs[i - 1];
					pthread = _thread_sig_handle(i, scp);
					if (pthread != NULL) {
						sigaddset(&pthread->sigmask, i);
						/* Save the old state: */
						pthread->oldstate = pthread->state;
						signal_lock.access_lock = 0;
						_thread_sig_deliver(pthread, i);
						sigdelset(&pthread->sigmask, i);
						if (_atomic_lock(&signal_lock.access_lock)) {
							check_pending = 1;
							/*
							 * Have the lock holder take care
							 * of any state changes:
							 */
							if (pthread->state != pthread->oldstate)
								check_waiting = 1;
							return;
						}
						if (pthread->state != pthread->oldstate)
							handle_state_change(pthread);
					}
				}
			}
			while (check_waiting != 0) {
				check_waiting = 0;
				/*
				 * Enter a loop to wake up all threads waiting
				 * for a process to complete:
				 */
				for (pthread = TAILQ_FIRST(&_waitingq);
				    pthread != NULL; pthread = pthread_next) {
					pthread_next = TAILQ_NEXT(pthread, pqe);
					if (pthread->state == PS_RUNNING)
						handle_state_change(pthread);
				}
			}
			/* Release the lock: */
			signal_lock.access_lock = 0;
		}

		/*
		 * Check to see if the current thread performed a
		 * [sig|_]longjmp() out of a signal handler.
		 */
		if ((_thread_run->jmpflags & (JMPFLAGS_LONGJMP |
		    JMPFLAGS__LONGJMP)) != 0) {
			_thread_run->jmpflags = JMPFLAGS_NONE;
			__longjmp(_thread_run->nested_jmp.jmp,
			    _thread_run->longjmp_val);
		} else if ((_thread_run->jmpflags & JMPFLAGS_SIGLONGJMP) != 0) {
			_thread_run->jmpflags = JMPFLAGS_NONE;
			__siglongjmp(_thread_run->nested_jmp.sigjmp,
			    _thread_run->longjmp_val);
		}
	}
}

pthread_t
_thread_sig_handle(int sig, ucontext_t * scp)
{
	int		i, handler_installed;
	pthread_t	pthread, pthread_next;
	pthread_t	suspended_thread, signaled_thread;

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
		/* Check if a child has terminated: */
		if (sig == SIGCHLD) {
			/*
			 * Go through the file list and set all files
			 * to non-blocking again in case the child
			 * set some of them to block. Sigh.
			 */
			for (i = 0; i < _thread_dtablesize; i++) {
				/* Check if this file is used: */
				if (_thread_fd_table[i] != NULL) {
					/*
					 * Set the file descriptor to
					 * non-blocking:
					 */
					_thread_sys_fcntl(i, F_SETFL,
					    _thread_fd_table[i]->flags |
					    O_NONBLOCK);
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
		}

		/*
		 * POSIX says that pending SIGCONT signals are
		 * discarded when one of these signals occurs.
		 */
		if (sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU) {
			/*
			 * Enter a loop to discard pending SIGCONT
			 * signals:
			 */
			TAILQ_FOREACH(pthread, &_thread_list, tle) {
				sigdelset(&pthread->sigpend,SIGCONT);
			}
		}

		/*
		 * Enter a loop to look for threads that have the signal
		 * unmasked.  POSIX specifies that a thread in a sigwait
		 * will get the signal over any other threads.  Second
		 * preference will be threads in in a sigsuspend.  If
		 * none of the above, then the signal is delivered to the
		 * first thread we find.  Note that if a custom handler
		 * is not installed, the signal only affects threads in
		 * sigwait.
		 */
		suspended_thread = NULL;
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

				/* Return the signal number: */
				pthread->signo = sig;

				/*
				 * POSIX doesn't doesn't specify which thread
				 * will get the signal if there are multiple
				 * waiters, so we give it to the first thread
				 * we find.
				 *
				 * Do not attempt to deliver this signal
				 * to other threads.
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
		if (handler_installed != 0) {
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

				/*
				 * Perform any state changes due to signal
				 * arrival:
				 */
				thread_sig_check_state(pthread, sig);
				return (pthread);
			}
		}
	}

	/* Returns nothing. */
	return (NULL);
}

static void
thread_sig_finish_longjmp(void *arg)
{
	/*
	 * Check to see if the current thread performed a [_]longjmp() out of a
	 * signal handler.
	 */
	if ((_thread_run->jmpflags & (JMPFLAGS_LONGJMP | JMPFLAGS__LONGJMP))
	    != 0) {
		_thread_run->jmpflags = JMPFLAGS_NONE;
		_thread_run->continuation = NULL;
		__longjmp(_thread_run->nested_jmp.jmp,
		    _thread_run->longjmp_val);
	}
	/*
	 * Check to see if the current thread performed a siglongjmp
	 * out of a signal handler:
	 */
	else if ((_thread_run->jmpflags & JMPFLAGS_SIGLONGJMP) != 0) {
		_thread_run->jmpflags = JMPFLAGS_NONE;
		_thread_run->continuation = NULL;
		__siglongjmp(_thread_run->nested_jmp.sigjmp,
		    _thread_run->longjmp_val);
	}
}

static void
handle_state_change(pthread_t pthread)
{
	/*
	 * We should only need to handle threads whose state was
	 * changed to running:
	 */
	if (pthread->state == PS_RUNNING) {
		switch (pthread->oldstate) {
		/*
		 * States which do not change when a signal is trapped:
		 */
		case PS_DEAD:
		case PS_DEADLOCK:
		case PS_RUNNING:
		case PS_SIGTHREAD:
		case PS_STATE_MAX:
		case PS_SUSPENDED:
			break;

		/*
		 * States which need to return to critical sections
		 * before they can switch contexts:
		 */
		case PS_COND_WAIT:
		case PS_FDLR_WAIT:
		case PS_FDLW_WAIT:
		case PS_FILE_WAIT:
		case PS_JOIN:
		case PS_MUTEX_WAIT:
			/* Indicate that the thread was interrupted: */
			pthread->interrupted = 1;
			/*
			 * Defer the [sig|_]longjmp until leaving the critical
			 * region:
			 */
			pthread->jmpflags |= JMPFLAGS_DEFERRED;

			/* Set the continuation routine: */
			pthread->continuation = thread_sig_finish_longjmp;
			/* FALLTHROUGH */
		case PS_FDR_WAIT:
		case PS_FDW_WAIT:
		case PS_POLL_WAIT:
		case PS_SELECT_WAIT:
		case PS_SIGSUSPEND:
		case PS_SIGWAIT:
		case PS_SLEEP_WAIT:
		case PS_SPINBLOCK:
		case PS_WAIT_WAIT:
			if ((pthread->flags & PTHREAD_FLAGS_IN_WAITQ) != 0) {
				PTHREAD_WAITQ_REMOVE(pthread);
				if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
					PTHREAD_WORKQ_REMOVE(pthread);
			}
			break;
		}

		if ((pthread->flags & PTHREAD_FLAGS_IN_PRIOQ) == 0)
			PTHREAD_PRIOQ_INSERT_TAIL(pthread);
	}
}


/* Perform thread specific actions in response to a signal: */
static void
thread_sig_check_state(pthread_t pthread, int sig)
{
	/*
	 * Process according to thread state:
	 */
	switch (pthread->state) {
	/*
	 * States which do not change when a signal is trapped:
	 */
	case PS_COND_WAIT:
	case PS_DEAD:
	case PS_DEADLOCK:
	case PS_FILE_WAIT:
	case PS_JOIN:
	case PS_MUTEX_WAIT:
	case PS_RUNNING:
	case PS_STATE_MAX:
	case PS_SIGTHREAD:
	case PS_SPINBLOCK:
	case PS_SUSPENDED:
		/* Increment the pending signal count. */
		sigaddset(&pthread->sigpend,sig);
		break;

	case PS_SIGWAIT:
		/* Wake up the thread if the signal is blocked. */
		if (sigismember(pthread->data.sigwait, sig)) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		} else
			/* Increment the pending signal count. */
			sigaddset(&pthread->sigpend,sig);
		break;

	/*
	 * The wait state is a special case due to the handling of
	 * SIGCHLD signals.
	 */
	case PS_WAIT_WAIT:
		/*
		 * Check for signals other than the death of a child
		 * process:
		 */
		if (sig != SIGCHLD)
			/* Flag the operation as interrupted: */
			pthread->interrupted = 1;

		/* Change the state of the thread to run: */
		PTHREAD_NEW_STATE(pthread,PS_RUNNING);

		/* Return the signal number: */
		pthread->signo = sig;
		break;

	/*
	 * States that are interrupted by the occurrence of a signal
	 * other than the scheduling alarm: 
	 */
	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
	case PS_POLL_WAIT:
	case PS_SLEEP_WAIT:
	case PS_SELECT_WAIT:
		if ((_thread_sigact[sig - 1].sa_flags & SA_RESTART) == 0) {
			/* Flag the operation as interrupted: */
			pthread->interrupted = 1;

			if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
				PTHREAD_WORKQ_REMOVE(pthread);

			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		}
		break;

	case PS_SIGSUSPEND:
		/*
		 * Only wake up the thread if there is a handler installed
		 * for the signal.
		 */
		if (_thread_sigact[sig - 1].sa_handler != SIG_DFL) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		}
		break;
	}
}

/* Send a signal to a specific thread (ala pthread_kill): */
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
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		} else if (pthread->state != PS_SIGWAIT &&
		    !sigismember(&pthread->sigmask, sig)) {
			/* Perform any state changes due to signal arrival: */
			thread_sig_check_state(pthread, sig);
			/* Increment the pending signal count. */
			sigaddset(&pthread->sigpend,sig);
		} else {
			/* Increment the pending signal count. */
			sigaddset(&pthread->sigpend,sig);
		}
	}
}

/* Dispatch pending signals to the running thread: */
void
_dispatch_signals()
{
	sigset_t sigset;
	int i;

	/*
	 * Check if there are pending signals for the running
	 * thread or process that aren't blocked:
	 */
	sigset = _thread_run->sigpend;
	SIGSETOR(sigset, _process_sigpending);
	SIGSETNAND(sigset, _thread_run->sigmask);
	if (SIGNOTEMPTY(sigset)) {
		/*
		 * Enter a loop to calculate deliverable pending signals
		 * before actually delivering them.  The pending signals
		 * must be removed from the pending signal sets before
		 * calling the signal handler because the handler may
		 * call library routines that again check for and deliver
		 * pending signals.
		 */
		for (i = 1; i < NSIG; i++) {
			/*
			 * Check that a custom handler is installed
			 * and if the signal is not blocked:
			 */
			if (_thread_sigact[i - 1].sa_handler != SIG_DFL &&
			    _thread_sigact[i - 1].sa_handler != SIG_IGN &&
			    sigismember(&sigset, i)) {
				if (sigismember(&_thread_run->sigpend,i))
					/* Clear the thread pending signal: */
					sigdelset(&_thread_run->sigpend,i);
				else
					/* Clear the process pending signal: */
					sigdelset(&_process_sigpending,i);
			}
			else
				/* Remove the signal if it can't be handled: */
				sigdelset(&sigset, i);
		}

		/* Now deliver the signals: */
		for (i = 1; i < NSIG; i++) {
			if (sigismember(&sigset, i))
				/* Deliver the signal to the running thread: */
				_thread_sig_deliver(_thread_run, i);
		}
	}
}

/* Deliver a signal to a thread: */
void
_thread_sig_deliver(pthread_t pthread, int sig)
{
	sigset_t	mask;
	pthread_t	pthread_saved;
	jmp_buf		jb, *saved_sighandler_jmp_buf;

	/*
	 * Check that a custom handler is installed
	 * and if the signal is not blocked:
	 */
	if (_thread_sigact[sig - 1].sa_handler != SIG_DFL &&
	    _thread_sigact[sig - 1].sa_handler != SIG_IGN) {
		/* Save the current thread: */
		pthread_saved = _thread_run;

		/* Save the threads signal mask: */
		mask = pthread->sigmask;

		/*
		 * Add the current signal and signal handler
		 * mask to the thread's current signal mask:
		 */
		SIGSETOR(pthread->sigmask, _thread_sigact[sig - 1].sa_mask);
		sigaddset(&pthread->sigmask, sig);

		/* Current thread inside critical region? */
		if (_thread_run->sig_defer_count > 0)
			pthread->sig_defer_count++;

		/* Increment the number of nested signals being handled. */
		pthread->signal_nest_level++;

		/*
		 * The jump buffer is allocated off the stack and the current
		 * jump buffer is saved.  If the signal handler tries to
		 * [sig|_]longjmp(), our version of [sig|_]longjmp() will copy
		 * the user supplied jump buffer into
		 * _thread_run->nested_jmp.[sig]jmp and _longjmp() back to here.
		 */
		saved_sighandler_jmp_buf = pthread->sighandler_jmp_buf;
		pthread->sighandler_jmp_buf = &jb;

		_thread_run = pthread;

		if (_setjmp(jb) == 0) {
			/*
			 * Dispatch the signal via the custom signal
			 * handler:
			 */
			(*(_thread_sigact[sig - 1].sa_handler))(sig);
		}

		_thread_run = pthread_saved;

		pthread->sighandler_jmp_buf = saved_sighandler_jmp_buf;

		/* Decrement the signal nest level. */
		pthread->signal_nest_level--;

		/* Current thread inside critical region? */
		if (_thread_run->sig_defer_count > 0)
			pthread->sig_defer_count--;

		/* Restore the threads signal mask: */
		pthread->sigmask = mask;
	}
}
#endif
