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

/* Allocate space for global thread variables here: */
#define GLOBAL_PTHREAD_PRIVATE

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

#ifdef GCC_2_8_MADE_THREAD_AWARE
typedef void *** (*dynamic_handler_allocator)();
extern void __set_dynamic_handler_allocator(dynamic_handler_allocator);

static pthread_key_t except_head_key;

typedef struct {
  void **__dynamic_handler_chain;
  void *top_elt[2];
} except_struct;

static void ***dynamic_allocator_handler_fn()
{
	except_struct *dh = (except_struct *)pthread_getspecific(except_head_key);

	if(dh == NULL) {
		dh = (except_struct *)malloc( sizeof(except_struct) );
		memset(dh, '\0', sizeof(except_struct));
		dh->__dynamic_handler_chain= dh->top_elt;
		pthread_setspecific(except_head_key, (void *)dh);
	}
	return &dh->__dynamic_handler_chain;
}
#endif /* GCC_2_8_MADE_THREAD_AWARE */

/*
 * Threaded process initialization
 */
void
_thread_init(void)
{
	int             flags;
	int             i;
	struct sigaction act;

	/* Check if this function has already been called: */
	if (_thread_initial)
		/* Only initialise the threaded application once. */
		return;

	/* Get the standard I/O flags before messing with them : */
	for (i = 0; i < 3; i++)
		if ((_pthread_stdio_flags[i] =
		    _thread_sys_fcntl(i,F_GETFL, NULL)) == -1)
			PANIC("Cannot get stdio flags");

	/*
	 * Create a pipe that is written to by the signal handler to prevent
	 * signals being missed in calls to _select: 
	 */
	if (_thread_sys_pipe(_thread_kern_pipe) != 0) {
		/* Cannot create pipe, so abort: */
		PANIC("Cannot create kernel pipe");
	}
	/* Get the flags for the read pipe: */
	else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel read pipe flags");
	}
	/* Make the read pipe non-blocking: */
	else if (_thread_sys_fcntl(_thread_kern_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot make kernel read pipe non-blocking");
	}
	/* Get the flags for the write pipe: */
	else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Make the write pipe non-blocking: */
	else if (_thread_sys_fcntl(_thread_kern_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Allocate memory for the thread structure of the initial thread: */
	else if ((_thread_initial = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/*
		 * Insufficient memory to initialise this application, so
		 * abort: 
		 */
		PANIC("Cannot allocate memory for initial thread");
	} else {
		/* Zero the global kernel thread structure: */
		memset(&_thread_kern_thread, 0, sizeof(struct pthread));
		memset(_thread_initial, 0, sizeof(struct pthread));

		/* Default the priority of the initial thread: */
		_thread_initial->pthread_priority = PTHREAD_DEFAULT_PRIORITY;

		/* Initialise the state of the initial thread: */
		_thread_initial->state = PS_RUNNING;

		/* Initialise the queue: */
		_thread_queue_init(&(_thread_initial->join_queue));

		/* Initialise the rest of the fields: */
		_thread_initial->specific_data = NULL;
		_thread_initial->cleanup = NULL;
		_thread_initial->queue = NULL;
		_thread_initial->qnxt = NULL;
		_thread_initial->nxt = NULL;
		_thread_initial->flags = 0;
		_thread_initial->error = 0;
		_thread_link_list = _thread_initial;
		_thread_run = _thread_initial;

		/* Initialise the global signal action structure: */
		sigfillset(&act.sa_mask);
		act.sa_handler = (void (*) ()) _thread_sig_handler;
		act.sa_flags = SA_RESTART;

		/* Enter a loop to get the existing signal status: */
		for (i = 1; i < NSIG; i++) {
			/* Check for signals which cannot be trapped: */
			if (i == SIGKILL || i == SIGSTOP) {
			}

			/* Get the signal handler details: */
			else if (_thread_sys_sigaction(i, NULL,
			    &_thread_sigact[i - 1]) != 0) {
				/*
				 * Abort this process if signal
				 * initialisation fails: 
				 */
				PANIC("Cannot read signal handler info");
			}
		}

		/*
		 * Install the signal handler for the most important
		 * signals that the user-thread kernel needs. Actually
		 * SIGINFO isn't really needed, but it is nice to have.
		 */
		if (_thread_sys_sigaction(SIGVTALRM, &act, NULL) != 0 ||
		    _thread_sys_sigaction(SIGINFO  , &act, NULL) != 0 ||
		    _thread_sys_sigaction(SIGCHLD  , &act, NULL) != 0) {
			/*
			 * Abort this process if signal initialisation fails: 
			 */
			PANIC("Cannot initialise signal handler");
		}

		/* Get the table size: */
		if ((_thread_dtablesize = getdtablesize()) < 0) {
			/*
			 * Cannot get the system defined table size, so abort
			 * this process. 
			 */
			PANIC("Cannot get dtablesize");
		}
		/* Allocate memory for the file descriptor table: */
		if ((_thread_fd_table = (struct fd_table_entry **) malloc(sizeof(struct fd_table_entry *) * _thread_dtablesize)) == NULL) {
			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process. 
			 */
			PANIC("Cannot allocate memory for file descriptor table");
		} else {
			/*
			 * Enter a loop to initialise the file descriptor
			 * table: 
			 */
			for (i = 0; i < _thread_dtablesize; i++) {
				/* Initialise the file descriptor table: */
				_thread_fd_table[i] = NULL;
			}
		}
	}

#ifdef GCC_2_8_MADE_THREAD_AWARE
	/* Create the thread-specific data for the exception linked list. */
	if(pthread_key_create(&except_head_key, NULL) != 0)
        	PANIC("Failed to create thread specific execption head");

	/* Setup the gcc exception handler per thread. */
	__set_dynamic_handler_allocator( dynamic_allocator_handler_fn );
#endif /* GCC_2_8_MADE_THREAD_AWARE */

	return;
}

/*
 * Special start up code for NetBSD/Alpha 
 */
#if	defined(__NetBSD__) && defined(__alpha__)
int 
main(int argc, char *argv[], char *env);

int
_thread_main(int argc, char *argv[], char *env)
{
	_thread_init();
	return (main(argc, argv, env));
}
#endif
#else
/*
 * A stub for non-threaded programs.
 */
void
_thread_init(void)
{
}
#endif
