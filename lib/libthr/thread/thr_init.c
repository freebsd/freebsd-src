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
 * Early implementations of sigtimedwait interpreted the signal
 * set incorrectly.
 */
#define SIGTIMEDWAIT_SET_IS_INVERTED(osreldate) \
    ((500100 <= (osreldate) && (osreldate) <= 500113) || \
    (osreldate) == 501000 || (osreldate) == 501100)

extern void _thread_init_hack(void);

/*
 * All weak references used within libc should be in this table.
 * This will is so that static libraries will work.
 *
 * XXXTHR - Check this list.
 */
static void *references[] = {
	&_thread_init_hack,
	&_thread_init,
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
	&_thread_init_hack,
	&_thread_init,
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
	struct pthread	*pthread;
	int		fd;
	int             i;
	size_t		len;
	int		mib[2];
	sigset_t	set;
	int		osreldate;
	int		error;

	struct clockinfo clockinfo;
	struct sigaction act;

	/* Check if this function has already been called: */
	if (_thread_initial)
		/* Only initialise the threaded application once. */
		return;

	_pthread_page_size = getpagesize();
	_pthread_guard_default = getpagesize();
    	
	pthread_attr_default.guardsize_attr = _pthread_guard_default;


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

	/* Allocate memory for the thread structure of the initial thread: */
	if ((pthread = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/*
		 * Insufficient memory to initialise this application, so
		 * abort:
		 */
		PANIC("Cannot allocate memory for initial thread");
	}
	/* Zero the initial thread structure: */
	memset(pthread, 0, sizeof(struct pthread));

	_thread_initial = pthread;
	pthread->arch_id = _set_curthread(NULL, pthread, &error);

	/* Get our thread id. */
	thr_self(&pthread->thr_id);

	/* Give this thread default attributes: */
	memcpy((void *) &pthread->attr, &pthread_attr_default,
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
	pthread->stack = _usrstack - PTHREAD_STACK_INITIAL;

	/* Set the stack attributes. */
	pthread->attr.stackaddr_attr = pthread->stack;
	pthread->attr.stacksize_attr = PTHREAD_STACK_INITIAL;

	/*
	 * Write a magic value to the thread structure
	 * to help identify valid ones:
	 */
	pthread->magic = PTHREAD_MAGIC;

	/* Set the initial cancel state */
	pthread->cancelflags = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;

	/* Setup the context for initial thread. */
	getcontext(&pthread->ctx);
	pthread->ctx.uc_stack.ss_sp = pthread->stack;
	pthread->ctx.uc_stack.ss_size = PTHREAD_STACK_INITIAL;

	/* Default the priority of the initial thread: */
	pthread->base_priority = PTHREAD_DEFAULT_PRIORITY;
	pthread->active_priority = PTHREAD_DEFAULT_PRIORITY;
	pthread->inherited_priority = 0;

	/* Initialise the state of the initial thread: */
	pthread->state = PS_RUNNING;

	/* Set the name of the thread: */
	pthread->name = strdup("_thread_initial");

	/* Initialize joiner to NULL (no joiner): */
	pthread->joiner = NULL;

	/* Initialize the owned mutex queue and count: */
	TAILQ_INIT(&(pthread->mutexq));
	pthread->priority_mutex_count = 0;

	/* Initialise the rest of the fields: */
	pthread->specific = NULL;
	pthread->cleanup = NULL;
	pthread->flags = 0;
	pthread->error = 0;
	TAILQ_INIT(&_thread_list);
	TAILQ_INSERT_HEAD(&_thread_list, pthread, tle);

	/* Enter a loop to get the existing signal status: */
	for (i = 1; i < NSIG; i++) {
		/* Check for signals which cannot be trapped. */
		if (i == SIGKILL || i == SIGSTOP)
			continue;

		/* Get the signal handler details. */
		if (__sys_sigaction(i, NULL,
		    &_thread_sigact[i - 1]) != 0)
			PANIC("Cannot read signal handler info");
	}
	act.sa_sigaction = _thread_sig_wrapper;
	act.sa_flags = SA_SIGINFO;
	SIGFILLSET(act.sa_mask);

	if (__sys_sigaction(SIGTHR, &act, NULL))
		PANIC("Cannot set SIGTHR handler.\n");

	SIGEMPTYSET(set);
	SIGADDSET(set, SIGTHR);
	__sys_sigprocmask(SIG_BLOCK, &set, 0);

	/*
	 * Precompute the signal set used by _thread_suspend to wait
	 * for SIGTHR.
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_OSRELDATE;
	len = sizeof(osreldate);
	if (sysctl(mib, 2, &osreldate, &len, NULL, 0) == 0 &&
	    SIGTIMEDWAIT_SET_IS_INVERTED(osreldate)) {
		/* Kernel bug requires an inverted signal set. */
		SIGFILLSET(_thread_suspend_sigset);
		SIGDELSET(_thread_suspend_sigset, SIGTHR);
	} else {
		SIGEMPTYSET(_thread_suspend_sigset);
		SIGADDSET(_thread_suspend_sigset, SIGTHR);
	}
#ifdef _PTHREADS_INVARIANTS
	SIGADDSET(_thread_suspend_sigset, SIGALRM);
#endif

	/* Get the kernel clockrate: */
	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	len = sizeof (struct clockinfo);
	if (sysctl(mib, 2, &clockinfo, &len, NULL, 0) == 0)
		_clock_res_usec = clockinfo.tick > CLOCK_RES_USEC_MIN ?
		    clockinfo.tick : CLOCK_RES_USEC_MIN;

	/* Initialise the garbage collector mutex and condition variable. */
	if (_pthread_mutex_init(&dead_list_lock,NULL) != 0 ||
	    _pthread_cond_init(&_gc_cond,NULL) != 0)
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
