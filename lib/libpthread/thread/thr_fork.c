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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

pid_t
fork(void)
{
	int             flags;
	int             status;
	pid_t           ret;
	pthread_t	pthread;
	pthread_t	pthread_next;

	/* Block signals to avoid being interrupted at a bad time: */
	_thread_kern_sig_block(&status);

	/* Fork a new process: */
	if ((ret = _thread_sys_fork()) <= 0) {
		/* Parent process or error. Nothing to do here. */
	} else {
		/* Close the pthread kernel pipe: */
		_thread_sys_close(_thread_kern_pipe[0]);
		_thread_sys_close(_thread_kern_pipe[1]);

		/* Reset signals pending for the running thread: */
		memset(_thread_run->sigpend, 0, sizeof(_thread_run->sigpend));

		/*
		 * Create a pipe that is written to by the signal handler to
		 * prevent signals being missed in calls to
		 * _thread_sys_select: 
		 */
		if (_thread_sys_pipe(_thread_kern_pipe) != 0) {
			/* Cannot create pipe, so abort: */
			PANIC("Cannot create pthread kernel pipe for forked process");
		}
		/* Get the flags for the read pipe: */
		else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL)) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Make the read pipe non-blocking: */
		else if (_thread_sys_fcntl(_thread_kern_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Get the flags for the write pipe: */
		else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL)) == -1) {
			/* Abort this application: */
			abort();
		}
		/* Make the write pipe non-blocking: */
		else if (_thread_sys_fcntl(_thread_kern_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
			/* Abort this application: */
			abort();
		} else {
			/* Point to the first thread in the list: */
			pthread = _thread_link_list;

			/*
			 * Enter a loop to remove all threads other than
			 * the running thread from the thread list:
			 */
			while (pthread != NULL) {
				pthread_next = pthread->nxt;
				if (pthread == _thread_run) {
					_thread_link_list = pthread;
					pthread->nxt = NULL;
				} else {
					if (pthread->attr.stackaddr_attr ==
					    NULL && pthread->stack != NULL) {
						/*
						 * Free the stack of the
						 * dead thread:
						 */
						free(pthread->stack);
					}
					if (pthread->specific_data != NULL)
						free(pthread->specific_data);
					free(pthread);
				}

				/* Point to the next thread: */
				pthread = pthread_next;
			}
		}
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(&status);

	/* Return the process ID: */
	return (ret);
}
#endif
