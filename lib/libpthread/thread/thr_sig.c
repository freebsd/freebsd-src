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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static spinlock_t	signal_lock = _SPINLOCK_INITIALIZER;
unsigned int		pending_sigs[NSIG];
unsigned int		handled_sigs[NSIG];
int			volatile check_pending = 0;

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
}

void
_thread_sig_handler(int sig, int code, struct sigcontext * scp)
{
	char	c;
	int	i;

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
	}
	else {
		if (_atomic_lock(&signal_lock.access_lock)) {
			/* There is another signal handler running: */
			pending_sigs[sig - 1]++;
			check_pending = 1;
		}
		else {
			/* It's safe to handle the signal now. */
			_thread_sig_handle(sig, scp);

			/* Reset the pending and handled count back to 0: */
			pending_sigs[sig - 1] = 0;
			handled_sigs[sig - 1] = 0;

			signal_lock.access_lock = 0;
		}

		/* Enter a loop to process pending signals: */
		while ((check_pending != 0) &&
		    (_atomic_lock(&signal_lock.access_lock) == 0)) {
			check_pending = 0;
			for (i = 1; i < NSIG; i++) {
				if (pending_sigs[i - 1] > handled_sigs[i - 1])
					_thread_sig_handle(i, scp);
			}
			signal_lock.access_lock = 0;
		}
	}
}

void
_thread_sig_handle(int sig, struct sigcontext * scp)
{
	int		i;
	pthread_t	pthread, pthread_next;

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
		 * Enter a loop to process each thread in the waiting
		 * list that is sigwait-ing on a signal.  Since POSIX
		 * doesn't specify which thread will get the signal
		 * if there are multiple waiters, we'll give it to the
		 * first one we find.
		 */
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
				 * Do not attempt to deliver this signal
				 * to other threads.
				 */
				return;
			}
		}

		/* Check if the signal is not being ignored: */
		if (_thread_sigact[sig - 1].sa_handler != SIG_IGN)
			/*
			 * Enter a loop to process each thread in the linked
			 * list: 
			 */
			TAILQ_FOREACH(pthread, &_thread_list, tle) {
				pthread_t pthread_saved = _thread_run;

				/* Current thread inside critical region? */
				if (_thread_run->sig_defer_count > 0)
					pthread->sig_defer_count++;

				_thread_run = pthread;
				_thread_signal(pthread,sig);

				/*
				 * Dispatch pending signals to the
				 * running thread:
				 */
				_dispatch_signals();
				_thread_run = pthread_saved;

				/* Current thread inside critical region? */
				if (_thread_run->sig_defer_count > 0)
					pthread->sig_defer_count--;
			}
	}

	/* Returns nothing. */
	return;
}

/* Perform thread specific actions in response to a signal: */
void
_thread_signal(pthread_t pthread, int sig)
{
	/*
	 * Flag the signal as pending. It will be dispatched later.
	 */
	sigaddset(&pthread->sigpend,sig);

	/*
	 * Process according to thread state:
	 */
	switch (pthread->state) {
	/*
	 * States which do not change when a signal is trapped:
	 */
	case PS_COND_WAIT:
	case PS_DEAD:
	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_FILE_WAIT:
	case PS_JOIN:
	case PS_MUTEX_WAIT:
	case PS_RUNNING:
	case PS_STATE_MAX:
	case PS_SIGTHREAD:
	case PS_SIGWAIT:
	case PS_SUSPENDED:
		/* Nothing to do here. */
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
	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
	case PS_POLL_WAIT:
	case PS_SLEEP_WAIT:
	case PS_SELECT_WAIT:
		if (sig != SIGCHLD ||
		    _thread_sigact[sig - 1].sa_handler != SIG_DFL) {
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
		 * Only wake up the thread if the signal is unblocked
		 * and there is a handler installed for the signal.
		 */
		if (!sigismember(&pthread->sigmask, sig) &&
		    _thread_sigact[sig - 1].sa_handler != SIG_DFL) {
			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
		}
		break;
	}
}

/* Dispatch pending signals to the running thread: */
void
_dispatch_signals()
{
	int i;

	/*
	 * Check if there are pending signals for the running
	 * thread that aren't blocked:
	 */
	if ((_thread_run->sigpend & ~_thread_run->sigmask) != 0)
		/* Look for all possible pending signals: */
		for (i = 1; i < NSIG; i++)
			/*
			 * Check that a custom handler is installed
			 * and if the signal is not blocked:
			 */
			if (_thread_sigact[i - 1].sa_handler != SIG_DFL &&
			    _thread_sigact[i - 1].sa_handler != SIG_IGN &&
			    sigismember(&_thread_run->sigpend,i) &&
			    !sigismember(&_thread_run->sigmask,i)) {
				/* Clear the pending signal: */
				sigdelset(&_thread_run->sigpend,i);

				/*
				 * Dispatch the signal via the custom signal
				 * handler:
				 */
				(*(_thread_sigact[i - 1].sa_handler))(i);
			}
}
#endif
