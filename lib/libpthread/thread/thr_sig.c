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
 */
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static int	volatile yield_on_unlock_dead		= 0;
static int	volatile yield_on_unlock_thread	= 0;
static long	volatile thread_dead_lock		= 0;
static long	volatile thread_link_list_lock		= 0;

/* Lock the thread list: */
void
_lock_thread_list()
{
	/* Lock the thread list: */
	_spinlock(&thread_link_list_lock);
}

/* Lock the dead thread list: */
void
_lock_dead_thread_list()
{
	/* Lock the dead thread list: */
	_spinlock(&thread_dead_lock);
}

/* Lock the thread list: */
void
_unlock_thread_list()
{
	/* Unlock the thread list: */
	_atomic_unlock(&thread_link_list_lock);

	/*
	 * Check if a scheduler interrupt occurred while the thread
	 * list was locked:
	 */
	if (yield_on_unlock_thread) {
		/* Reset the interrupt flag: */
		yield_on_unlock_thread = 0;

		/* This thread has overstayed it's welcome: */
		sched_yield();
	}
}

/* Lock the dead thread list: */
void
_unlock_dead_thread_list()
{
	/* Unlock the dead thread list: */
	_atomic_unlock(&thread_dead_lock);

	/*
	 * Check if a scheduler interrupt occurred while the dead
	 * thread list was locked:
	 */
	if (yield_on_unlock_dead) {
		/* Reset the interrupt flag: */
		yield_on_unlock_dead = 0;

		/* This thread has overstayed it's welcome: */
		sched_yield();
	}
}

void
_thread_sig_handler(int sig, int code, struct sigcontext * scp)
{
	char            c;
	int             i;
	int		dispatch = 0;
	pthread_t       pthread;

	/*
	 * Check if the pthread kernel has unblocked signals (or is about to)
	 * and was on its way into a _select when the current
	 * signal interrupted it: 
	 */
	if (_thread_kern_in_select) {
		/* Cast the signal number to a character variable: */
		c = sig;

		/*
		 * Write the signal number to the kernel pipe so that it will
		 * be ready to read when this signal handler returns. This
		 * means that the _select call will complete
		 * immediately. 
		 */
		_thread_sys_write(_thread_kern_pipe[1], &c, 1);
	}

	/* Check if the signal requires a dump of thread information: */
	if (sig == SIGINFO)
		/* Dump thread information to file: */
		_thread_dump_info();

	/* Check if an interval timer signal: */
	else if (sig == SIGVTALRM) {
		/* Check if the scheduler interrupt has come at an
		 * unfortunate time which one of the threads is
		 * modifying the thread list:
		 */
		if (thread_link_list_lock)
			/*
			 * Set a flag so that the thread that has
			 * the lock yields when it unlocks the
			 * thread list:
			 */
			yield_on_unlock_thread = 1;

		/* Check if the scheduler interrupt has come at an
		 * unfortunate time which one of the threads is
		 * modifying the dead thread list:
		 */
		if (thread_dead_lock)
			/*
			 * Set a flag so that the thread that has
			 * the lock yields when it unlocks the
			 * dead thread list:
			 */
			yield_on_unlock_dead = 1;

		/*
		 * Check if the kernel has not been interrupted while
		 * executing scheduler code:
		 */
		else if (!_thread_kern_in_sched) {
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
		 * discarded when one of there signals occurs.
		 */
		if (sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU) {
			/*
			 * Enter a loop to discard pending SIGCONT
			 * signals:
			 */
			for (pthread = _thread_link_list;
			    pthread != NULL;
			    pthread = pthread->nxt)
				sigdelset(&pthread->sigpend,SIGCONT);
		}

		/* Check if the signal is not being ignored: */
		if (_thread_sigact[sig - 1].sa_handler != SIG_IGN)
			/*
			 * Enter a loop to process each thread in the linked
			 * list: 
			 */
			for (pthread = _thread_link_list; pthread != NULL;
			     pthread = pthread->nxt)
				_thread_signal(pthread,sig);

		/* Dispatch pending signals to the running thread: */
		_dispatch_signals();
	}

	/* Returns nothing. */
	return;
}

/* Perform thread specific actions in response to a signal: */
void
_thread_signal(pthread_t pthread, int sig)
{
	pthread_t saved;
	struct sigaction act;

	/*
	 * Flag the signal as pending. It will be dispatched later.
	 */
	sigaddset(&pthread->sigpend,sig);

	/* Check if system calls are not restarted: */
	if ((_thread_sigact[sig - 1].sa_flags & SA_RESTART) != 0) {
		/*
		 * System calls are flagged for restart.
		 *
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
		case PS_SUSPENDED:
			/* Nothing to do here. */
			break;

		/*
		 * System calls that are restarted when interrupted by
		 * a signal:
		 */
		case PS_FDR_WAIT:
		case PS_FDW_WAIT:
		case PS_SELECT_WAIT:
			break;

		/*
		 * States that are interrupted by the occurrence of a signal
		 * other than the scheduling alarm: 
		 */
		case PS_SLEEP_WAIT:
		case PS_SIGWAIT:
		case PS_WAIT_WAIT:
			/* Flag the operation as interrupted: */
			pthread->interrupted = 1;

			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
			break;
		}
	} else {
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
		case PS_SUSPENDED:
			/* Nothing to do here. */
			break;

		/*
		 * States that are interrupted by the occurrence of a signal
		 * other than the scheduling alarm: 
		 */
		case PS_FDR_WAIT:
		case PS_FDW_WAIT:
		case PS_SLEEP_WAIT:
		case PS_SIGWAIT:
		case PS_WAIT_WAIT:
		case PS_SELECT_WAIT:
			/* Flag the operation as interrupted: */
			pthread->interrupted = 1;

			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);

			/* Return the signal number: */
			pthread->signo = sig;
			break;
		}
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
