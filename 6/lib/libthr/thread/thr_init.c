/*
 * Copyright (c) 2003 Daniel M. Eischen <deischen@freebsd.org>
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

#include "namespace.h"
#include <sys/types.h>
#include <sys/signalvar.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/ttycom.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "thr_private.h"

void		*_usrstack;
struct pthread	*_thr_initial;
int		_thr_scope_system;
int		_libthr_debug;
int		_thread_event_mask;
struct pthread	*_thread_last_event;
pthreadlist	_thread_list = TAILQ_HEAD_INITIALIZER(_thread_list);
pthreadlist 	_thread_gc_list = TAILQ_HEAD_INITIALIZER(_thread_gc_list);
int		_thread_active_threads = 1;
atfork_head	_thr_atfork_list = TAILQ_HEAD_INITIALIZER(_thr_atfork_list);
umtx_t		_thr_atfork_lock;

struct pthread_attr _pthread_attr_default = {
	.sched_policy = SCHED_RR,
	.sched_inherit = 0,
	.sched_interval = TIMESLICE_USEC,
	.prio = THR_DEFAULT_PRIORITY,
	.suspend = THR_CREATE_RUNNING,
	.flags = PTHREAD_SCOPE_SYSTEM,
	.arg_attr = NULL,
	.cleanup_attr = NULL,
	.stackaddr_attr = NULL,
	.stacksize_attr = THR_STACK_DEFAULT,
	.guardsize_attr = 0
};

struct pthread_mutex_attr _pthread_mutexattr_default = {
	.m_type = PTHREAD_MUTEX_DEFAULT,
	.m_protocol = PTHREAD_PRIO_NONE,
	.m_ceiling = 0,
	.m_flags = 0
};

/* Default condition variable attributes: */
struct pthread_cond_attr _pthread_condattr_default = {
	.c_pshared = PTHREAD_PROCESS_PRIVATE,
	.c_clockid = CLOCK_REALTIME
};

pid_t		_thr_pid;
int		_thr_guard_default;
int		_thr_stack_default = THR_STACK_DEFAULT;
int		_thr_stack_initial = THR_STACK_INITIAL;
int		_thr_page_size;
int		_gc_count;
umtx_t		_mutex_static_lock;
umtx_t		_cond_static_lock;
umtx_t		_rwlock_static_lock;
umtx_t		_keytable_lock;
umtx_t		_thr_list_lock;
umtx_t		_thr_event_lock;

int	__pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int	__pthread_mutex_lock(pthread_mutex_t *);
int	__pthread_mutex_trylock(pthread_mutex_t *);
void	_thread_init_hack(void) __attribute__ ((constructor));

static void init_private(void);
static void init_main_thread(struct pthread *thread);

/*
 * All weak references used within libc should be in this table.
 * This is so that static libraries will work.
 */
STATIC_LIB_REQUIRE(_accept);
STATIC_LIB_REQUIRE(_bind);
STATIC_LIB_REQUIRE(_close);
STATIC_LIB_REQUIRE(_connect);
STATIC_LIB_REQUIRE(_dup);
STATIC_LIB_REQUIRE(_dup2);
STATIC_LIB_REQUIRE(_execve);
STATIC_LIB_REQUIRE(_fcntl);
STATIC_LIB_REQUIRE(_flock);
STATIC_LIB_REQUIRE(_flockfile);
STATIC_LIB_REQUIRE(_fstat);
STATIC_LIB_REQUIRE(_fstatfs);
STATIC_LIB_REQUIRE(_fsync);
STATIC_LIB_REQUIRE(_getdirentries);
STATIC_LIB_REQUIRE(_getlogin);
STATIC_LIB_REQUIRE(_getpeername);
STATIC_LIB_REQUIRE(_getsockname);
STATIC_LIB_REQUIRE(_getsockopt);
STATIC_LIB_REQUIRE(_ioctl);
STATIC_LIB_REQUIRE(_kevent);
STATIC_LIB_REQUIRE(_listen);
STATIC_LIB_REQUIRE(_nanosleep);
STATIC_LIB_REQUIRE(_open);
STATIC_LIB_REQUIRE(_pthread_getspecific);
STATIC_LIB_REQUIRE(_pthread_key_create);
STATIC_LIB_REQUIRE(_pthread_key_delete);
STATIC_LIB_REQUIRE(_pthread_mutex_destroy);
STATIC_LIB_REQUIRE(_pthread_mutex_init);
STATIC_LIB_REQUIRE(_pthread_mutex_lock);
STATIC_LIB_REQUIRE(_pthread_mutex_trylock);
STATIC_LIB_REQUIRE(_pthread_mutex_unlock);
STATIC_LIB_REQUIRE(_pthread_mutexattr_init);
STATIC_LIB_REQUIRE(_pthread_mutexattr_destroy);
STATIC_LIB_REQUIRE(_pthread_mutexattr_settype);
STATIC_LIB_REQUIRE(_pthread_once);
STATIC_LIB_REQUIRE(_pthread_setspecific);
STATIC_LIB_REQUIRE(_read);
STATIC_LIB_REQUIRE(_readv);
STATIC_LIB_REQUIRE(_recvfrom);
STATIC_LIB_REQUIRE(_recvmsg);
STATIC_LIB_REQUIRE(_select);
STATIC_LIB_REQUIRE(_sendmsg);
STATIC_LIB_REQUIRE(_sendto);
STATIC_LIB_REQUIRE(_setsockopt);
STATIC_LIB_REQUIRE(_sigaction);
STATIC_LIB_REQUIRE(_sigprocmask);
STATIC_LIB_REQUIRE(_sigsuspend);
STATIC_LIB_REQUIRE(_socket);
STATIC_LIB_REQUIRE(_socketpair);
STATIC_LIB_REQUIRE(_thread_init_hack);
STATIC_LIB_REQUIRE(_wait4);
STATIC_LIB_REQUIRE(_write);
STATIC_LIB_REQUIRE(_writev);

/*
 * These are needed when linking statically.  All references within
 * libgcc (and in the future libc) to these routines are weak, but
 * if they are not (strongly) referenced by the application or other
 * libraries, then the actual functions will not be loaded.
 */
STATIC_LIB_REQUIRE(_pthread_once);
STATIC_LIB_REQUIRE(_pthread_key_create);
STATIC_LIB_REQUIRE(_pthread_key_delete);
STATIC_LIB_REQUIRE(_pthread_getspecific);
STATIC_LIB_REQUIRE(_pthread_setspecific);
STATIC_LIB_REQUIRE(_pthread_mutex_init);
STATIC_LIB_REQUIRE(_pthread_mutex_destroy);
STATIC_LIB_REQUIRE(_pthread_mutex_lock);
STATIC_LIB_REQUIRE(_pthread_mutex_trylock);
STATIC_LIB_REQUIRE(_pthread_mutex_unlock);
STATIC_LIB_REQUIRE(_pthread_create);

/* Pull in all symbols required by libthread_db */
STATIC_LIB_REQUIRE(_thread_state_running);

#define	DUAL_ENTRY(entry)	\
	(pthread_func_t)entry, (pthread_func_t)entry

static pthread_func_t jmp_table[][2] = {
	{DUAL_ENTRY(_pthread_cond_broadcast)},	/* PJT_COND_BROADCAST */
	{DUAL_ENTRY(_pthread_cond_destroy)},	/* PJT_COND_DESTROY */
	{DUAL_ENTRY(_pthread_cond_init)},	/* PJT_COND_INIT */
	{DUAL_ENTRY(_pthread_cond_signal)},	/* PJT_COND_SIGNAL */
	{(pthread_func_t)__pthread_cond_wait,
	 (pthread_func_t)_pthread_cond_wait},	/* PJT_COND_WAIT */
	{DUAL_ENTRY(_pthread_getspecific)},	/* PJT_GETSPECIFIC */
	{DUAL_ENTRY(_pthread_key_create)},	/* PJT_KEY_CREATE */
	{DUAL_ENTRY(_pthread_key_delete)},	/* PJT_KEY_DELETE*/
	{DUAL_ENTRY(_pthread_main_np)},		/* PJT_MAIN_NP */
	{DUAL_ENTRY(_pthread_mutex_destroy)},	/* PJT_MUTEX_DESTROY */
	{DUAL_ENTRY(_pthread_mutex_init)},	/* PJT_MUTEX_INIT */
	{(pthread_func_t)__pthread_mutex_lock,
	 (pthread_func_t)_pthread_mutex_lock},	/* PJT_MUTEX_LOCK */
	{(pthread_func_t)__pthread_mutex_trylock,
	 (pthread_func_t)_pthread_mutex_trylock},/* PJT_MUTEX_TRYLOCK */
	{DUAL_ENTRY(_pthread_mutex_unlock)},	/* PJT_MUTEX_UNLOCK */
	{DUAL_ENTRY(_pthread_mutexattr_destroy)}, /* PJT_MUTEXATTR_DESTROY */
	{DUAL_ENTRY(_pthread_mutexattr_init)},	/* PJT_MUTEXATTR_INIT */
	{DUAL_ENTRY(_pthread_mutexattr_settype)}, /* PJT_MUTEXATTR_SETTYPE */
	{DUAL_ENTRY(_pthread_once)},		/* PJT_ONCE */
	{DUAL_ENTRY(_pthread_rwlock_destroy)},	/* PJT_RWLOCK_DESTROY */
	{DUAL_ENTRY(_pthread_rwlock_init)},	/* PJT_RWLOCK_INIT */
	{DUAL_ENTRY(_pthread_rwlock_rdlock)},	/* PJT_RWLOCK_RDLOCK */
	{DUAL_ENTRY(_pthread_rwlock_tryrdlock)},/* PJT_RWLOCK_TRYRDLOCK */
	{DUAL_ENTRY(_pthread_rwlock_trywrlock)},/* PJT_RWLOCK_TRYWRLOCK */
	{DUAL_ENTRY(_pthread_rwlock_unlock)},	/* PJT_RWLOCK_UNLOCK */
	{DUAL_ENTRY(_pthread_rwlock_wrlock)},	/* PJT_RWLOCK_WRLOCK */
	{DUAL_ENTRY(_pthread_self)},		/* PJT_SELF */
	{DUAL_ENTRY(_pthread_setspecific)},	/* PJT_SETSPECIFIC */
	{DUAL_ENTRY(_pthread_sigmask)}		/* PJT_SIGMASK */
};

static int init_once = 0;

/*
 * For the shared version of the threads library, the above is sufficient.
 * But for the archive version of the library, we need a little bit more.
 * Namely, we must arrange for this particular module to be pulled in from
 * the archive library at link time.  To accomplish that, we define and
 * initialize a variable, "_thread_autoinit_dummy_decl".  This variable is
 * referenced (as an extern) from libc/stdlib/exit.c. This will always
 * create a need for this module, ensuring that it is present in the
 * executable.
 */
extern int _thread_autoinit_dummy_decl;
int _thread_autoinit_dummy_decl = 0;

void
_thread_init_hack(void)
{

	_libpthread_init(NULL);
}


/*
 * Threaded process initialization.
 *
 * This is only called under two conditions:
 *
 *   1) Some thread routines have detected that the library hasn't yet
 *      been initialized (_thr_initial == NULL && curthread == NULL), or
 *
 *   2) An explicit call to reinitialize after a fork (indicated
 *      by curthread != NULL)
 */
void
_libpthread_init(struct pthread *curthread)
{
	int fd, first = 0;
	sigset_t sigset, oldset;

	/* Check if this function has already been called: */
	if ((_thr_initial != NULL) && (curthread == NULL))
		/* Only initialize the threaded application once. */
		return;

	/*
	 * Check the size of the jump table to make sure it is preset
	 * with the correct number of entries.
	 */
	if (sizeof(jmp_table) != (sizeof(pthread_func_t) * PJT_MAX * 2))
		PANIC("Thread jump table not properly initialized");
	memcpy(__thr_jtable, jmp_table, sizeof(jmp_table));

	/*
	 * Check for the special case of this process running as
	 * or in place of init as pid = 1:
	 */
	if ((_thr_pid = getpid()) == 1) {
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
	}

	/* Initialize pthread private data. */
	init_private();

	/* Set the initial thread. */
	if (curthread == NULL) {
		first = 1;
		/* Create and initialize the initial thread. */
		curthread = _thr_alloc(NULL);
		if (curthread == NULL)
			PANIC("Can't allocate initial thread");
		init_main_thread(curthread);
	}
	/*
	 * Add the thread to the thread list queue.
	 */
	THR_LIST_ADD(curthread);
	_thread_active_threads = 1;

	/* Setup the thread specific data */
	_tcb_set(curthread->tcb);

	if (first) {
		SIGFILLSET(sigset);
		SIGDELSET(sigset, SIGTRAP);
		__sys_sigprocmask(SIG_SETMASK, &sigset, &oldset);
		_thr_signal_init();
		_thr_initial = curthread;
		SIGDELSET(oldset, SIGCANCEL);
		__sys_sigprocmask(SIG_SETMASK, &oldset, NULL);
		if (_thread_event_mask & TD_CREATE)
			_thr_report_creation(curthread, curthread);
	}
}

/*
 * This function and pthread_create() do a lot of the same things.
 * It'd be nice to consolidate the common stuff in one place.
 */
static void
init_main_thread(struct pthread *thread)
{
	/* Setup the thread attributes. */
	thr_self(&thread->tid);
	thread->attr = _pthread_attr_default;
	/*
	 * Set up the thread stack.
	 *
	 * Create a red zone below the main stack.  All other stacks
	 * are constrained to a maximum size by the parameters
	 * passed to mmap(), but this stack is only limited by
	 * resource limits, so this stack needs an explicitly mapped
	 * red zone to protect the thread stack that is just beyond.
	 */
	if (mmap((void *)_usrstack - _thr_stack_initial -
	    _thr_guard_default, _thr_guard_default, 0, MAP_ANON,
	    -1, 0) == MAP_FAILED)
		PANIC("Cannot allocate red zone for initial thread");

	/*
	 * Mark the stack as an application supplied stack so that it
	 * isn't deallocated.
	 *
	 * XXX - I'm not sure it would hurt anything to deallocate
	 *       the main thread stack because deallocation doesn't
	 *       actually free() it; it just puts it in the free
	 *       stack queue for later reuse.
	 */
	thread->attr.stackaddr_attr = (void *)_usrstack - _thr_stack_initial;
	thread->attr.stacksize_attr = _thr_stack_initial;
	thread->attr.guardsize_attr = _thr_guard_default;
	thread->attr.flags |= THR_STACK_USER;

	/*
	 * Write a magic value to the thread structure
	 * to help identify valid ones:
	 */
	thread->magic = THR_MAGIC;

	thread->cancelflags = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;
	thread->name = strdup("initial thread");

	/* Default the priority of the initial thread: */
	thread->base_priority = THR_DEFAULT_PRIORITY;
	thread->active_priority = THR_DEFAULT_PRIORITY;
	thread->inherited_priority = 0;

	/* Initialize the mutex queue: */
	TAILQ_INIT(&thread->mutexq);
	TAILQ_INIT(&thread->pri_mutexq);

	thread->state = PS_RUNNING;

	/* Others cleared to zero by thr_alloc() */
}

static void
init_private(void)
{
	size_t len;
	int mib[2];

	_thr_umtx_init(&_mutex_static_lock);
	_thr_umtx_init(&_cond_static_lock);
	_thr_umtx_init(&_rwlock_static_lock);
	_thr_umtx_init(&_keytable_lock);
	_thr_umtx_init(&_thr_atfork_lock);
	_thr_umtx_init(&_thr_event_lock);
	_thr_spinlock_init();
	_thr_list_init();

	/*
	 * Avoid reinitializing some things if they don't need to be,
	 * e.g. after a fork().
	 */
	if (init_once == 0) {
		/* Find the stack top */
		mib[0] = CTL_KERN;
		mib[1] = KERN_USRSTACK;
		len = sizeof (_usrstack);
		if (sysctl(mib, 2, &_usrstack, &len, NULL, 0) == -1)
			PANIC("Cannot get kern.usrstack from sysctl");
		_thr_page_size = getpagesize();
		_thr_guard_default = _thr_page_size;
		_pthread_attr_default.guardsize_attr = _thr_guard_default;
		_pthread_attr_default.stacksize_attr = _thr_stack_default;

		TAILQ_INIT(&_thr_atfork_list);
#ifdef SYSTEM_SCOPE_ONLY
		_thr_scope_system = 1;
#else
		if (getenv("LIBPTHREAD_SYSTEM_SCOPE") != NULL)
			_thr_scope_system = 1;
		else if (getenv("LIBPTHREAD_PROCESS_SCOPE") != NULL)
			_thr_scope_system = -1;
#endif
	}
	init_once = 1;
}
