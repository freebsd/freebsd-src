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
 * 3. Neither the name of the author nor the names of any co-contributors
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

#include "namespace.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "un-namespace.h"
#include "thr_private.h"

#ifndef NELEMENTS
#define NELEMENTS(arr)	(sizeof(arr) / sizeof(arr[0]))
#endif

static void	dump_thread(int fd, pthread_t pthread, int long_version);
void		_pthread_set_name_np(pthread_t thread, char *name);

__weak_reference(_pthread_set_name_np, pthread_set_name_np);

struct s_thread_info {
	enum pthread_state state;
	const char	*name;
};

/* Static variables: */
static const struct s_thread_info thread_info[] = {
	{PS_RUNNING	, "Running"},
        {PS_LOCKWAIT    , "Waiting on an internal lock"},
	{PS_MUTEX_WAIT	, "Waiting on a mutex"},
	{PS_COND_WAIT	, "Waiting on a condition variable"},
	{PS_SLEEP_WAIT	, "Sleeping"},
	{PS_SIGSUSPEND	, "Suspended, waiting for a signal"},
	{PS_SIGWAIT	, "Waiting for a signal"},
	{PS_JOIN	, "Waiting to join"},
	{PS_SUSPENDED	, "Suspended"},
	{PS_DEAD	, "Dead"},
	{PS_DEADLOCK	, "Deadlocked"},
	{PS_STATE_MAX	, "Not a real state!"}
};

void
_thread_dump_info(void)
{
	char s[512], tempfile[128];
	pthread_t pthread;
	int fd, i;

	for (i = 0; i < 100000; i++) {
		snprintf(tempfile, sizeof(tempfile), "/tmp/pthread.dump.%u.%i",
			getpid(), i);
		/* Open the dump file for append and create it if necessary: */
		if ((fd = __sys_open(tempfile, O_RDWR | O_CREAT | O_EXCL,
			0666)) < 0) {
				/* Can't open the dump file. */
				if (errno == EEXIST)
					continue;
				/*
				 * We only need to continue in case of
				 * EEXIT error. Most other error
				 * codes means that we will fail all
				 * the times.
				 */
				return;
		} else {
			break;
		}
	}
	if (i==100000) {
		/* all 100000 possibilities are in use :( */
		return;
	} else {
		/* Dump the active threads. */
		strcpy(s, "\n\n========\nACTIVE THREADS\n\n");
		__sys_write(fd, s, strlen(s));

		/* Enter a loop to report each thread in the global list: */
		TAILQ_FOREACH(pthread, &_thread_list, tle) {
			if (pthread->state != PS_DEAD)
				dump_thread(fd, pthread, /*long_verson*/ 1);
		}

		/*
		 * Dump the ready threads.
		 * XXX - We can't easily do this because the run queues
		 *       are per-KSEG.
		 */
		strcpy(s, "\n\n========\nREADY THREADS - unimplemented\n\n");
		__sys_write(fd, s, strlen(s));


		/*
		 * Dump the waiting threads.
		 * XXX - We can't easily do this because the wait queues
		 *       are per-KSEG.
		 */
		strcpy(s, "\n\n========\nWAITING THREADS - unimplemented\n\n");
		__sys_write(fd, s, strlen(s));

		/* Close the dump file. */
		__sys_close(fd);
	}
}

static void
dump_thread(int fd, pthread_t pthread, int long_version)
{
	struct pthread *curthread = _get_curthread();
	char s[512];
	int i;

	/* Find the state: */
	for (i = 0; i < (int)NELEMENTS(thread_info) - 1; i++)
		if (thread_info[i].state == pthread->state)
			break;

	/* Output a record for the thread: */
	snprintf(s, sizeof(s),
	    "--------------------\n"
	    "Thread %p (%s), scope %s, prio %3d, blocked %s, state %s [%s:%d]\n",
	    pthread, (pthread->name == NULL) ? "" : pthread->name,
	    pthread->attr.flags & PTHREAD_SCOPE_SYSTEM ? "system" : "process",
	    pthread->active_priority, (pthread->blocked != 0) ? "yes" : "no",
	    thread_info[i].name, pthread->fname, pthread->lineno);
	__sys_write(fd, s, strlen(s));

	if (long_version != 0) {
		/* Check if this is the running thread: */
		if (pthread == curthread) {
			/* Output a record for the running thread: */
			strcpy(s, "This is the running thread\n");
			__sys_write(fd, s, strlen(s));
		}
		/* Check if this is the initial thread: */
		if (pthread == _thr_initial) {
			/* Output a record for the initial thread: */
			strcpy(s, "This is the initial thread\n");
			__sys_write(fd, s, strlen(s));
		}
	
		/* Process according to thread state: */
		switch (pthread->state) {
		case PS_SIGWAIT:
			snprintf(s, sizeof(s), "sigmask (hi) ");
			__sys_write(fd, s, strlen(s));
			for (i = _SIG_WORDS - 1; i >= 0; i--) {
				snprintf(s, sizeof(s), "%08x ",
				    pthread->sigmask.__bits[i]);
				__sys_write(fd, s, strlen(s));
			}
			snprintf(s, sizeof(s), "(lo)\n");
			__sys_write(fd, s, strlen(s));

			snprintf(s, sizeof(s), "waitset (hi) ");
			__sys_write(fd, s, strlen(s));
			for (i = _SIG_WORDS - 1; i >= 0; i--) {
				snprintf(s, sizeof(s), "%08x ",
				    pthread->data.sigwait->waitset->__bits[i]);
				__sys_write(fd, s, strlen(s));
			}
			snprintf(s, sizeof(s), "(lo)\n");
			__sys_write(fd, s, strlen(s));
			break;
		/*
		 * Trap other states that are not explicitly
		 * coded to dump information:
		 */
		default:
			snprintf(s, sizeof(s), "sigmask (hi) ");
			__sys_write(fd, s, strlen(s));
			for (i = _SIG_WORDS - 1; i >= 0; i--) {
				snprintf(s, sizeof(s), "%08x ",
				    pthread->sigmask.__bits[i]);
				__sys_write(fd, s, strlen(s));
			}
			snprintf(s, sizeof(s), "(lo)\n");
			__sys_write(fd, s, strlen(s));
			break;
		}
	}
}

/* Set the thread name for debug: */
void
_pthread_set_name_np(pthread_t thread, char *name)
{
	struct pthread *curthread = _get_curthread();
	char *new_name;
	char *prev_name;
	int ret;

	new_name = strdup(name);
	/* Add a reference to the target thread. */
	if (_thr_ref_add(curthread, thread, 0) != 0) {
		free(new_name);
		ret = ESRCH;
	}
	else {
		THR_THREAD_LOCK(curthread, thread);
		prev_name = thread->name;
		thread->name = new_name;
		THR_THREAD_UNLOCK(curthread, thread);
		_thr_ref_delete(curthread, thread);
		if (prev_name != NULL) {
			/* Free space for previous name. */
			free(prev_name);
		}
		ret = 0;
	}
#if 0
	/* XXX - Should return error code. */
	return (ret);
#endif
}
