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
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

struct s_thread_info {
	enum pthread_state state;
	char           *name;
};

/* Static variables: */
static const struct s_thread_info thread_info[] = {
	{PS_RUNNING	, "Running"},
	{PS_SIGTHREAD	, "Waiting on signal thread"},
	{PS_MUTEX_WAIT	, "Waiting on a mutex"},
	{PS_COND_WAIT	, "Waiting on a condition variable"},
	{PS_FDLR_WAIT	, "Waiting for a file read lock"},
	{PS_FDLW_WAIT	, "Waiting for a file write lock"},
	{PS_FDR_WAIT	, "Waiting for read"},
	{PS_FDW_WAIT	, "Waiting for write"},
	{PS_FILE_WAIT	, "Waiting for FILE lock"},
	{PS_SELECT_WAIT	, "Waiting on select"},
	{PS_SLEEP_WAIT	, "Sleeping"},
	{PS_WAIT_WAIT	, "Waiting process"},
	{PS_SIGWAIT	, "Waiting for a signal"},
	{PS_JOIN	, "Waiting to join"},
	{PS_SUSPENDED	, "Suspended"},
	{PS_DEAD	, "Dead"},
	{PS_STATE_MAX	, "Not a real state!"}
};

void
_thread_dump_info(void)
{
	char            s[128];
	int             fd;
	int             i;
	int             j;
	pthread_t       pthread;

	/* Open the dump file for append and create it if necessary: */
	if ((fd = _thread_sys_open("/tmp/uthread.dump", O_RDWR | O_CREAT | O_APPEND, 0666)) < 0) {
		/* Can't open the dump file. */
	} else {
		/* Output a header for active threads: */
		strcpy(s, "\n\n=============\nACTIVE THREADS\n\n");
		_thread_sys_write(fd, s, strlen(s));

		/* Enter a loop to report each thread in the global list: */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Find the state: */
			for (j = 0; j < (sizeof(thread_info) / sizeof(struct s_thread_info)) - 1; j++)
				if (thread_info[j].state == pthread->state)
					break;
			/* Output a record for the current thread: */
			sprintf(s, "--------------------\nThread %p (%s) prio %3d state %s [%s:%d]\n",
				pthread, (pthread->name == NULL) ? "":pthread->name, pthread->pthread_priority, thread_info[j].name,pthread->fname,pthread->lineno);
			_thread_sys_write(fd, s, strlen(s));

			/* Check if this is the running thread: */
			if (pthread == _thread_run) {
				/* Output a record for the running thread: */
				strcpy(s, "This is the running thread\n");
				_thread_sys_write(fd, s, strlen(s));
			}
			/* Check if this is the initial thread: */
			if (pthread == _thread_initial) {
				/* Output a record for the initial thread: */
				strcpy(s, "This is the initial thread\n");
				_thread_sys_write(fd, s, strlen(s));
			}
			/* Process according to thread state: */
			switch (pthread->state) {
			/* File descriptor read lock wait: */
			case PS_FDLR_WAIT:
			case PS_FDLW_WAIT:
			case PS_FDR_WAIT:
			case PS_FDW_WAIT:
				/* Write the lock details: */
				sprintf(s, "fd %d[%s:%d]", pthread->data.fd.fd, pthread->data.fd.fname, pthread->data.fd.branch);
				_thread_sys_write(fd, s, strlen(s));
				sprintf(s, "owner %pr/%pw\n", _thread_fd_table[pthread->data.fd.fd]->r_owner, _thread_fd_table[pthread->data.fd.fd]->w_owner);
				_thread_sys_write(fd, s, strlen(s));
				break;
			case PS_SIGWAIT:
				sprintf(s, "sigmask 0x%08lx\n", pthread->sigmask);
				_thread_sys_write(fd, s, strlen(s));
				break;

			/*
			 * Trap other states that are not explicitly
			 * coded to dump information: 
			 */
			default:
				/* Nothing to do here. */
				break;
			}
		}

		/* Check if there are no dead threads: */
		if (_thread_dead == NULL) {
			/* Output a record: */
			strcpy(s, "\n\nTHERE ARE NO DEAD THREADS\n");
			_thread_sys_write(fd, s, strlen(s));
		} else {
			/* Output a header for dead threads: */
			strcpy(s, "\n\nDEAD THREADS\n\n");
			_thread_sys_write(fd, s, strlen(s));

			/*
			 * Enter a loop to report each thread in the global
			 * dead thread list: 
			 */
			for (pthread = _thread_dead; pthread != NULL; pthread = pthread->nxt) {
				/* Output a record for the current thread: */
				sprintf(s, "Thread %p prio %3d [%s:%d]\n", pthread, pthread->pthread_priority,pthread->fname,pthread->lineno);
				_thread_sys_write(fd, s, strlen(s));
			}
		}

		/* Output a header for file descriptors: */
		strcpy(s, "\n\n=============\nFILE DESCRIPTOR TABLE\n\n");
		_thread_sys_write(fd, s, strlen(s));

		/* Enter a loop to report file descriptor lock usage: */
		for (i = 0; i < _thread_dtablesize; i++) {
			/*
			 * Check if memory is allocated for this file
			 * descriptor: 
			 */
			if (_thread_fd_table[i] != NULL) {
				/* Report the file descriptor lock status: */
				sprintf(s, "fd[%3d] read owner %p count %d [%s:%d]\n        write owner %p count %d [%s:%d]\n",
					i,
					_thread_fd_table[i]->r_owner,
					_thread_fd_table[i]->r_lockcount,
					_thread_fd_table[i]->r_fname,
					_thread_fd_table[i]->r_lineno,
					_thread_fd_table[i]->w_owner,
					_thread_fd_table[i]->w_lockcount,
					_thread_fd_table[i]->w_fname,
					_thread_fd_table[i]->w_lineno);
				_thread_sys_write(fd, s, strlen(s));
			}
		}

		/* Close the dump file: */
		_thread_sys_close(fd);
	}
	return;
}

/* Set the thread name for debug: */
void
pthread_set_name_np(pthread_t thread, char *name)
{
	/* Check if the caller has specified a valid thread: */
	if (thread != NULL && thread->magic == PTHREAD_MAGIC)
		thread->name = strdup(name);
	return;
}
#endif
