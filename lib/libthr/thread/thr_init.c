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
int _pthread_stack_default;
int _pthread_stack_initial;

/*
 * Initialize the current thread.
 */
void
init_td_common(struct pthread *td, struct pthread_attr *attrp, int reinit)
{
	/*
	 * Some parts of a pthread are initialized only once.
	 */
	if (!reinit) {
		memset(td, 0, sizeof(struct pthread));
		td->cancelmode = M_DEFERRED;
		td->cancelstate = M_DEFERRED;
		td->cancellation = CS_NULL;
		memcpy(&td->attr, attrp, sizeof(struct pthread_attr));
		td->magic = PTHREAD_MAGIC;
		TAILQ_INIT(&td->mutexq);
		td->base_priority = PTHREAD_DEFAULT_PRIORITY;
		td->active_priority = PTHREAD_DEFAULT_PRIORITY;
		td->inherited_priority = PTHREAD_MIN_PRIORITY;
	} else {
		memset(&td->join_status, 0, sizeof(struct join_status));
	}
	td->joiner = NULL;
	td->error = 0;
	td->flags = 0;
}

/*
 * Initialize the active and dead threads list. Any threads in the active
 * list will be removed and the thread td * will be marked as the
 * initial thread and inserted in the list as the only thread. Any threads
 * in the dead threads list will also be removed.
 */
void
init_tdlist(struct pthread *td, int reinit)
{
	struct pthread *tdTemp, *tdTemp2;

	_thread_initial = td;
	td->name = strdup("_thread_initial");

	/*
	 * If this is not the first initialization, remove any entries
	 * that may be in the list and deallocate their memory. Also
	 * destroy any global pthread primitives (they will be recreated).
	 */
	if (reinit) {
		TAILQ_FOREACH_SAFE(tdTemp, &_thread_list, tle, tdTemp2) {
			if (tdTemp != NULL && tdTemp != td) {
				TAILQ_REMOVE(&_thread_list, tdTemp, tle);
				free(tdTemp);
			}
		}
		TAILQ_FOREACH_SAFE(tdTemp, &_dead_list, dle, tdTemp2) {
			if (tdTemp != NULL) {
				TAILQ_REMOVE(&_dead_list, tdTemp, dle);
				free(tdTemp);
			}
		}
		_pthread_mutex_destroy(&dead_list_lock);
	} else {
		TAILQ_INIT(&_thread_list);
		TAILQ_INIT(&_dead_list);

		/* Insert this thread as the first thread in the active list */
		TAILQ_INSERT_HEAD(&_thread_list, td, tle);
	}

	/*
	 * Initialize the active thread list lock and the
	 * dead threads list lock.
	 */
	memset(&thread_list_lock, 0, sizeof(spinlock_t));
	if (_pthread_mutex_init(&dead_list_lock,NULL) != 0)
		PANIC("Failed to initialize garbage collector primitives");
}

/*
 * Threaded process initialization
 */
void
_thread_init(void)
{
	struct pthread	*pthread;
	int		fd;
	size_t		len;
	int		mib[2];
	int		error;

	/* Check if this function has already been called: */
	if (_thread_initial)
		/* Only initialise the threaded application once. */
		return;

	_pthread_page_size = getpagesize();
	_pthread_guard_default = getpagesize();
	if (sizeof(void *) == 8) {
		_pthread_stack_default = PTHREAD_STACK64_DEFAULT;
		_pthread_stack_initial = PTHREAD_STACK64_INITIAL;
	}
	else {
		_pthread_stack_default = PTHREAD_STACK32_DEFAULT;
		_pthread_stack_initial = PTHREAD_STACK32_INITIAL;
	}

	pthread_attr_default.guardsize_attr = _pthread_guard_default;
	pthread_attr_default.stacksize_attr = _pthread_stack_default;

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

	init_tdlist(pthread, 0);
	init_td_common(pthread, &pthread_attr_default, 0);
	pthread->arch_id = _set_curthread(NULL, pthread, &error);

	/* Get our thread id. */
	thr_self(&pthread->thr_id);

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
	if (mmap(_usrstack - _pthread_stack_initial -
	    _pthread_guard_default, _pthread_guard_default, 0,
	    MAP_ANON, -1, 0) == MAP_FAILED)
		PANIC("Cannot allocate red zone for initial thread");

	/* Set the main thread stack pointer. */
	pthread->stack = _usrstack - _pthread_stack_initial;

	/* Set the stack attributes. */
	pthread->attr.stackaddr_attr = pthread->stack;
	pthread->attr.stacksize_attr = _pthread_stack_initial;

	/* Setup the context for initial thread. */
	getcontext(&pthread->ctx);
	pthread->ctx.uc_stack.ss_sp = pthread->stack;
	pthread->ctx.uc_stack.ss_size = _pthread_stack_initial;

	/* Initialize the atfork list and mutex */
	TAILQ_INIT(&_atfork_list);
	_pthread_mutex_init(&_atfork_mutex, NULL);
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
