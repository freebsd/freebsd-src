/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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

void
_thread_sig_handler(int sig, int code, struct sigcontext * scp)
{
	char            c;
	int             i;
	pthread_t       pthread;

	/*
	 * Check if the pthread kernel has unblocked signals (or is about to)
	 * and was on its way into a _thread_sys_select when the current
	 * signal interrupted it: 
	 */
	if (_thread_kern_in_select) {
		/* Cast the signal number to a character variable: */
		c = sig;

		/*
		 * Write the signal number to the kernel pipe so that it will
		 * be ready to read when this signal handler returns. This
		 * means that the _thread_sys_select call will complete
		 * immediately. 
		 */
		if (_thread_sys_write(_thread_kern_pipe[1], &c, 1) != 1) {
		}
	}
	/* Check if the signal requires a dump of thread information: */
	if (sig == SIGINFO) {
		/* Dump thread information to file: */
		_thread_dump_info();
	} else {
		/* Handle depending on signal type: */
		switch (sig) {
		/* Interval timer used for timeslicing: */
		case SIGVTALRM:
			/*
			 * Don't add the signal to any thread.  Just want to
			 * call the scheduler:
			 */
			break;

		/* Child termination: */
		case SIGCHLD:
			/*
			 * Enter a loop to process each thread in the linked
			 * list:
			 */
			for (pthread = _thread_link_list; pthread != NULL;
			     pthread = pthread->nxt) {
				/*
				 * Add the signal to the set of pending
				 * signals:
				 */
				pthread->sigpend[sig] += 1;
				if (pthread->state == PS_WAIT_WAIT) {
					/* Reset the error: */
					/* There should be another flag so that this is not required! ### */
					_thread_seterrno(pthread, 0);

					/* Change the state of the thread to run: */
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				}
			}

			/*
			 * Go through the file list and set all files
			 * to non-blocking again in case the child
			 * set some of them to block. Sigh.
			 */
			for (i = 0; i < _thread_dtablesize; i++) {
				/* Check if this file is used: */
				if (_thread_fd_table[i] != NULL) {
					/* Set the file descriptor to non-blocking: */
					_thread_sys_fcntl(i, F_SETFL, _thread_fd_table[i]->flags | O_NONBLOCK);
				}
			}
			break;

		/* Signals specific to the running thread: */
		case SIGBUS:
		case SIGEMT:
		case SIGFPE:
		case SIGILL:
		case SIGPIPE:
		case SIGSEGV:
		case SIGSYS:
			/* Add the signal to the set of pending signals: */
			_thread_run->sigpend[sig] += 1;
			break;

		/* Signals to send to all threads: */
		default:
			/*
			 * Enter a loop to process each thread in the linked
			 * list: 
			 */
			for (pthread = _thread_link_list; pthread != NULL;
			     pthread = pthread->nxt) {
				/*
				 * Add the signal to the set of pending
				 * signals: 
				 */
				pthread->sigpend[sig] += 1;
			}
			break;
		}

		/* Check if the kernel is not locked: */
		if (_thread_run != &_thread_kern_thread) {
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

	/* Returns nothing. */
	return;
}
#endif
