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

/* Allocate space for global thread variables here: */
#define GLOBAL_PTHREAD_PRIVATE

#include "namespace.h"
#include <sys/param.h>
#include <sys/types.h>
#include <machine/reg.h>

#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#include "thr_private.h"

/*
 * All weak references used within libc should be in this table.
 * This will is so that static libraries will work.
 */
static void *references[] = {
	&_accept,
	&_bind,
	&_close,
	&_connect,
	&_dup,
	&_dup2,
	&_execve,
	&_fcntl,
	&_flock,
	&_flockfile,
	&_fstat,
	&_fstatfs,
	&_fsync,
	&_funlockfile,
	&_getdirentries,
	&_getlogin,
	&_getpeername,
	&_getsockname,
	&_getsockopt,
	&_ioctl,
	&_kevent,
	&_listen,
	&_nanosleep,
	&_open,
	&_pthread_getspecific,
	&_pthread_key_create,
	&_pthread_key_delete,
	&_pthread_mutex_destroy,
	&_pthread_mutex_init,
	&_pthread_mutex_lock,
	&_pthread_mutex_trylock,
	&_pthread_mutex_unlock,
	&_pthread_mutexattr_init,
	&_pthread_mutexattr_destroy,
	&_pthread_mutexattr_settype,
	&_pthread_once,
	&_pthread_setspecific,
	&_read,
	&_readv,
	&_recvfrom,
	&_recvmsg,
	&_select,
	&_sendmsg,
	&_sendto,
	&_setsockopt,
	&_sigaction,
	&_sigprocmask,
	&_sigsuspend,
	&_socket,
	&_socketpair,
	&_wait4,
	&_write,
	&_writev
};

/*
 * These are needed when linking statically.  All references within
 * libgcc (and in the future libc) to these routines are weak, but
 * if they are not (strongly) referenced by the application or other
 * libraries, then the actual functions will not be loaded.
 */
static void *libgcc_references[] = {
	&_pthread_once,
	&_pthread_key_create,
	&_pthread_key_delete,
	&_pthread_getspecific,
	&_pthread_setspecific,
	&_pthread_mutex_init,
	&_pthread_mutex_destroy,
	&_pthread_mutex_lock,
	&_pthread_mutex_trylock,
	&_pthread_mutex_unlock
};

int _pthread_guard_default;
int _pthread_page_size;

/*
 * Threaded process initialization
 */
void
_thread_init(void)
{
	int		fd;
	int             flags;
	int             i;
	size_t		len;
	int		mib[2];
	int		sched_stack_size;	/* Size of scheduler stack. */

	struct clockinfo clockinfo;

	_pthread_page_size = getpagesize();
	_pthread_guard_default = getpagesize();
	sched_stack_size = getpagesize();
    	
	pthread_attr_default.guardsize_attr = _pthread_guard_default;


	/* Check if this function has already been called: */
	if (_thread_initial)
		/* Only initialise the threaded application once. */
		return;

	/*
	 * Make gcc quiescent about {,libgcc_}references not being
	 * referenced:
	 */
	if ((references[0] == NULL) || (libgcc_references[0] == NULL))
		PANIC("Failed loading mandatory references in _thread_init");

	/*
	 * Check for the special case of this process running as
	 * or in place of init as pid = 1:
	 */
	if (getpid() == 1) {
		/*
		 * Setup a new session for this process which is
		 * assumed to be running as root.
		 */
		if (setsid() == -1)
			PANIC("Can't set session ID");
		if (revoke(_PATH_CONSOLE) != 0)
			PANIC("Can't revoke console");
		if ((fd = __sys_open(_PATH_CONSOLE, O_RDWR)) < 0)
			PANIC("Can't open console");
		if (setlogin("root") == -1)
			PANIC("Can't set login to root");
		if (__sys_ioctl(fd, TIOCSCTTY, (char *) NULL) == -1)
			PANIC("Can't set controlling terminal");
		if (__sys_dup2(fd, 0) == -1 ||
		    __sys_dup2(fd, 1) == -1 ||
		    __sys_dup2(fd, 2) == -1)
			PANIC("Can't dup2");
	}

	/* Allocate and initialize the ready queue: */
	if (_pq_alloc(&_readyq, PTHREAD_MIN_PRIORITY, PTHREAD_LAST_PRIORITY) !=
	    0) {
		/* Abort this application: */
		PANIC("Cannot allocate priority ready queue.");
	}
	/* Allocate memory for the thread structure of the initial thread: */
	else if ((_thread_initial = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/*
		 * Insufficient memory to initialise this application, so
		 * abort:
		 */
		PANIC("Cannot allocate memory for initial thread");
	}
	/* Allocate memory for the scheduler stack: */
	else if ((_thread_kern_sched_stack = malloc(sched_stack_size)) == NULL)
		PANIC("Failed to allocate stack for scheduler");
	else {
		/* Zero the global kernel thread structure: */
		memset(&_thread_kern_thread, 0, sizeof(struct pthread));
		_thread_kern_thread.flags = PTHREAD_FLAGS_PRIVATE;
		memset(_thread_initial, 0, sizeof(struct pthread));

		/* Initialize the waiting and work queues: */
		TAILQ_INIT(&_waitingq);
		TAILQ_INIT(&_workq);

		/* Initialize the scheduling switch hook routine: */
		_sched_switch_hook = NULL;

		/* Give this thread default attributes: */
		memcpy((void *) &_thread_initial->attr, &pthread_attr_default,
		    sizeof(struct pthread_attr));

		/* Find the stack top */
		mib[0] = CTL_KERN;
		mib[1] = KERN_USRSTACK;
		len = sizeof (_usrstack);
		if (sysctl(mib, 2, &_usrstack, &len, NULL, 0) == -1)
			_usrstack = (void *)USRSTACK;
		/*
		 * Create a red zone below the main stack.  All other stacks are
		 * constrained to a maximum size by the paramters passed to
		 * mmap(), but this stack is only limited by resource limits, so
		 * this stack needs an explicitly mapped red zone to protect the
		 * thread stack that is just beyond.
		 */
		if (mmap(_usrstack - PTHREAD_STACK_INITIAL -
		    _pthread_guard_default, _pthread_guard_default, 0,
		    MAP_ANON, -1, 0) == MAP_FAILED)
			PANIC("Cannot allocate red zone for initial thread");

		/* Set the main thread stack pointer. */
		_thread_initial->stack = _usrstack - PTHREAD_STACK_INITIAL;

		/* Set the stack attributes. */
		_thread_initial->attr.stackaddr_attr = _thread_initial->stack;
		_thread_initial->attr.stacksize_attr = PTHREAD_STACK_INITIAL;

		/* Setup the context for the scheduler. */
		_thread_kern_kse_mailbox.km_stack.ss_sp =
		    _thread_kern_sched_stack;
		_thread_kern_kse_mailbox.km_stack.ss_size = sched_stack_size;
		_thread_kern_kse_mailbox.km_func =
		    (void *)_thread_kern_scheduler;

		/*
		 * Write a magic value to the thread structure
		 * to help identify valid ones:
		 */
		_thread_initial->magic = PTHREAD_MAGIC;

		/* Set the initial cancel state */
		_thread_initial->cancelflags = PTHREAD_CANCEL_ENABLE |
		    PTHREAD_CANCEL_DEFERRED;

		/* Setup the context for initial thread. */
		getcontext(&_thread_initial->mailbox.tm_context);
		_thread_initial->mailbox.tm_context.uc_stack.ss_sp =
		    _thread_initial->stack;
		_thread_initial->mailbox.tm_context.uc_stack.ss_size =
		    PTHREAD_STACK_INITIAL;
		_thread_initial->mailbox.tm_udata = (void *)_thread_initial;

		/* Default the priority of the initial thread: */
		_thread_initial->base_priority = PTHREAD_DEFAULT_PRIORITY;
		_thread_initial->active_priority = PTHREAD_DEFAULT_PRIORITY;
		_thread_initial->inherited_priority = 0;

		/* Initialise the state of the initial thread: */
		_thread_initial->state = PS_RUNNING;

		/* Set the name of the thread: */
		_thread_initial->name = strdup("_thread_initial");

		/* Initialize joiner to NULL (no joiner): */
		_thread_initial->joiner = NULL;

		/* Initialize the owned mutex queue and count: */
		TAILQ_INIT(&(_thread_initial->mutexq));
		_thread_initial->priority_mutex_count = 0;

		/* Initialize the global scheduling time: */
		_sched_ticks = 0;
		gettimeofday((struct timeval *) &_sched_tod, NULL);

		/* Initialize last active: */
		_thread_initial->last_active = (long) _sched_ticks;

		/* Initialise the rest of the fields: */
		_thread_initial->sig_defer_count = 0;
		_thread_initial->specific = NULL;
		_thread_initial->cleanup = NULL;
		_thread_initial->flags = 0;
		_thread_initial->error = 0;
		TAILQ_INIT(&_thread_list);
		TAILQ_INSERT_HEAD(&_thread_list, _thread_initial, tle);
		_set_curthread(_thread_initial);

		/* Get the kernel clockrate: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_CLOCKRATE;
		len = sizeof (struct clockinfo);
		if (sysctl(mib, 2, &clockinfo, &len, NULL, 0) == 0)
			_clock_res_usec = clockinfo.tick > CLOCK_RES_USEC_MIN ?
			    clockinfo.tick : CLOCK_RES_USEC_MIN;

		/* Start KSE. */
		_thread_kern_kse_mailbox.km_curthread =
			&_thread_initial->mailbox;
		if (kse_create(&_thread_kern_kse_mailbox, 0) != 0)
			PANIC("kse_new failed");
	}

	/* Initialise the garbage collector mutex and condition variable. */
	if (_pthread_mutex_init(&_gc_mutex,NULL) != 0 ||
	    pthread_cond_init(&_gc_cond,NULL) != 0)
		PANIC("Failed to initialise garbage collector mutex or condvar");
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
