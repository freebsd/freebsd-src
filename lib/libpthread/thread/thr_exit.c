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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

void _exit(int status)
{
	int		flags;
	int             i;
	struct itimerval itimer;

	/* Disable the interval timer: */
	itimer.it_interval.tv_sec  = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_sec     = 0;
	itimer.it_value.tv_usec    = 0;
	setitimer(ITIMER_VIRTUAL, &itimer, NULL);

	/* Close the pthread kernel pipe: */
	_thread_sys_close(_thread_kern_pipe[0]);
	_thread_sys_close(_thread_kern_pipe[1]);

	/*
	 * Enter a loop to set all file descriptors to blocking
	 * if they were not created as non-blocking:
	 */
	for (i = 0; i < _thread_dtablesize; i++) {
		/* Check if this file descriptor is in use: */
		if (_thread_fd_table[i] != NULL &&
			!(_thread_fd_table[i]->flags & O_NONBLOCK)) {
			/* Get the current flags: */
			flags = _thread_sys_fcntl(i, F_GETFL, NULL);
			/* Clear the nonblocking file descriptor flag: */
			_thread_sys_fcntl(i, F_SETFL, flags & ~O_NONBLOCK);
		}
	}

	/* Call the _exit syscall: */
	_thread_sys__exit(status);
}

void
_thread_exit(char *fname, int lineno, char *string)
{
	char            s[256];

	/* Prepare an error message string: */
	strcpy(s, "Fatal error '");
	strcat(s, string);
	strcat(s, "' at line ? ");
	strcat(s, "in file ");
	strcat(s, fname);
	strcat(s, " (errno = ?");
	strcat(s, ")\n");

	/* Write the string to the standard error file descriptor: */
	_thread_sys_write(2, s, strlen(s));

	/* Force this process to exit: */
	_exit(1);
}

void
pthread_exit(void *status)
{
	int             sig;
	long            l;
	pthread_t       pthread;

	/* Save the return value: */
	_thread_run->ret = status;

	while (_thread_run->cleanup != NULL) {
		pthread_cleanup_pop(1);
	}

	if (_thread_run->attr.cleanup_attr != NULL) {
		_thread_run->attr.cleanup_attr(_thread_run->attr.arg_attr);
	}
	/* Check if there is thread specific data: */
	if (_thread_run->specific_data != NULL) {
		/* Run the thread-specific data destructors: */
		_thread_cleanupspecific();
	}
	/* Check if there are any threads joined to this one: */
	while ((pthread = _thread_queue_deq(&(_thread_run->join_queue))) != NULL) {
		/* Wake the joined thread and let it detach this thread: */
		PTHREAD_NEW_STATE(pthread,PS_RUNNING);
	}

	/* Lock the thread list: */
	_lock_thread_list();

	/* Check if the running thread is at the head of the linked list: */
	if (_thread_link_list == _thread_run) {
		/* There is no previous thread: */
		_thread_link_list = _thread_run->nxt;
	} else {
		/* Point to the first thread in the list: */
		pthread = _thread_link_list;

		/*
		 * Enter a loop to find the thread in the linked list before
		 * the running thread: 
		 */
		while (pthread != NULL && pthread->nxt != _thread_run) {
			/* Point to the next thread: */
			pthread = pthread->nxt;
		}

		/* Check that a previous thread was found: */
		if (pthread != NULL) {
			/*
			 * Point the previous thread to the one after the
			 * running thread: 
			 */
			pthread->nxt = _thread_run->nxt;
		}
	}

	/* Unlock the thread list: */
	_unlock_thread_list();

	/* Lock the dead thread list: */
	_lock_dead_thread_list();

	/*
	 * This thread will never run again. Add it to the list of dead
	 * threads: 
	 */
	_thread_run->nxt = _thread_dead;
	_thread_dead = _thread_run;

	/* Unlock the dead thread list: */
	_unlock_dead_thread_list();

	/*
	 * The running thread is no longer in the thread link list so it will
	 * now die: 
	 */
	_thread_kern_sched_state(PS_DEAD, __FILE__, __LINE__);

	/* This point should not be reached. */
	PANIC("Dead thread has resumed");
}
#endif
