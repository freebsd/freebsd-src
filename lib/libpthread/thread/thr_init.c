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

/* Allocate space for global thread variables here: */
#define GLOBAL_PTHREAD_PRIVATE

#include "namespace.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
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
#include <pthread_np.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "thr_private.h"

int	__pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int	__pthread_mutex_lock(pthread_mutex_t *);
int	__pthread_mutex_trylock(pthread_mutex_t *);
void	_thread_init_hack(void);

static void init_private(void);
static void init_main_thread(struct pthread *thread);

/*
 * All weak references used within libc should be in this table.
 * This is so that static libraries will work.
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
	&_thread_init_hack,
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

static int	init_once = 0;

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
	int fd;

	/* Check if this function has already been called: */
	if ((_thr_initial != NULL) && (curthread == NULL))
		/* Only initialize the threaded application once. */
		return;

	/*
	 * Make gcc quiescent about {,libgcc_}references not being
	 * referenced:
	 */
	if ((references[0] == NULL) || (libgcc_references[0] == NULL))
		PANIC("Failed loading mandatory references in _thread_init");

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
	_kse_init();

	/* Initialize the initial kse and kseg. */
#ifdef SYSTEM_SCOPE_ONLY
	_kse_initial = _kse_alloc(NULL, 1);
#else
	_kse_initial = _kse_alloc(NULL, 0);
#endif
	if (_kse_initial == NULL)
		PANIC("Can't allocate initial kse.");
	_kse_initial->k_kseg = _kseg_alloc(NULL);
	if (_kse_initial->k_kseg == NULL)
		PANIC("Can't allocate initial kseg.");
#ifdef SYSTEM_SCOPE_ONLY
	_kse_initial->k_kseg->kg_flags |= KGF_SINGLE_THREAD;
#endif
	_kse_initial->k_schedq = &_kse_initial->k_kseg->kg_schedq;

	TAILQ_INSERT_TAIL(&_kse_initial->k_kseg->kg_kseq, _kse_initial, k_kgqe);
	_kse_initial->k_kseg->kg_ksecount = 1;

	/* Set the initial thread. */
	if (curthread == NULL) {
		/* Create and initialize the initial thread. */
		curthread = _thr_alloc(NULL);
		if (curthread == NULL)
			PANIC("Can't allocate initial thread");
		_thr_initial = curthread;
		init_main_thread(curthread);
	} else {
		/*
		 * The initial thread is the current thread.  It is
		 * assumed that the current thread is already initialized
		 * because it is left over from a fork().
		 */
		_thr_initial = curthread;
	}
	_kse_initial->k_kseg->kg_threadcount = 0;
	_thr_initial->kse = _kse_initial;
	_thr_initial->kseg = _kse_initial->k_kseg;
	_thr_initial->active = 1;

	/*
	 * Add the thread to the thread list and to the KSEG's thread
         * queue.
	 */
	THR_LIST_ADD(_thr_initial);
	KSEG_THRQ_ADD(_kse_initial->k_kseg, _thr_initial);

	/* Setup the KSE/thread specific data for the current KSE/thread. */
	_thr_initial->kse->k_curthread = _thr_initial;
	_kcb_set(_thr_initial->kse->k_kcb);
	_tcb_set(_thr_initial->kse->k_kcb, _thr_initial->tcb);
	_thr_initial->kse->k_flags |= KF_INITIALIZED;

	_thr_rtld_init();
}

/*
 * This function and pthread_create() do a lot of the same things.
 * It'd be nice to consolidate the common stuff in one place.
 */
static void
init_main_thread(struct pthread *thread)
{
	int i;

	/* Setup the thread attributes. */
	thread->attr = _pthread_attr_default;
#ifdef SYSTEM_SCOPE_ONLY
	thread->attr.flags |= PTHREAD_SCOPE_SYSTEM;
#endif
	/*
	 * Set up the thread stack.
	 *
	 * Create a red zone below the main stack.  All other stacks
	 * are constrained to a maximum size by the parameters
	 * passed to mmap(), but this stack is only limited by
	 * resource limits, so this stack needs an explicitly mapped
	 * red zone to protect the thread stack that is just beyond.
	 */
	if (mmap((void *)_usrstack - THR_STACK_INITIAL -
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
	thread->attr.stackaddr_attr = (void *)_usrstack - THR_STACK_INITIAL;
	thread->attr.stacksize_attr = THR_STACK_INITIAL;
	thread->attr.guardsize_attr = _thr_guard_default;
	thread->attr.flags |= THR_STACK_USER;

	/*
	 * Write a magic value to the thread structure
	 * to help identify valid ones:
	 */
	thread->magic = THR_MAGIC;

	thread->slice_usec = -1;
	thread->cancelflags = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;
	thread->name = strdup("initial thread");

	/* Initialize the thread for signals: */
	SIGEMPTYSET(thread->sigmask);

	/*
	 * Set up the thread mailbox.  The threads saved context
	 * is also in the mailbox.
	 */
	thread->tcb->tcb_tmbx.tm_udata = thread;
	thread->tcb->tcb_tmbx.tm_context.uc_stack.ss_size =
	    thread->attr.stacksize_attr;
	thread->tcb->tcb_tmbx.tm_context.uc_stack.ss_sp =
	    thread->attr.stackaddr_attr;

	/* Default the priority of the initial thread: */
	thread->base_priority = THR_DEFAULT_PRIORITY;
	thread->active_priority = THR_DEFAULT_PRIORITY;
	thread->inherited_priority = 0;

	/* Initialize the mutex queue: */
	TAILQ_INIT(&thread->mutexq);

	/* Initialize thread locking. */
	if (_lock_init(&thread->lock, LCK_ADAPTIVE,
	    _thr_lock_wait, _thr_lock_wakeup) != 0)
		PANIC("Cannot initialize initial thread lock");
	for (i = 0; i < MAX_THR_LOCKLEVEL; i++) {
		_lockuser_init(&thread->lockusers[i], (void *)thread);
		_LCK_SET_PRIVATE2(&thread->lockusers[i], (void *)thread);
	}

	/* Initialize hooks in the thread structure: */
	thread->specific = NULL;
	thread->cleanup = NULL;
	thread->flags = 0;
	thread->continuation = NULL;

	thread->state = PS_RUNNING;
	thread->uniqueid = 0;
}

static void
init_private(void)
{
	struct clockinfo clockinfo;
	size_t len;
	int mib[2];

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

		/*
		 * Create a red zone below the main stack.  All other
		 * stacks are constrained to a maximum size by the
		 * parameters passed to mmap(), but this stack is only
		 * limited by resource limits, so this stack needs an
		 * explicitly mapped red zone to protect the thread stack
		 * that is just beyond.
		 */
		if (mmap((void *)_usrstack - THR_STACK_INITIAL -
		    _thr_guard_default, _thr_guard_default,
		    0, MAP_ANON, -1, 0) == MAP_FAILED)
			PANIC("Cannot allocate red zone for initial thread");

		/* Get the kernel clockrate: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_CLOCKRATE;
		len = sizeof (struct clockinfo);
		if (sysctl(mib, 2, &clockinfo, &len, NULL, 0) == 0)
			_clock_res_usec = clockinfo.tick;
		else
			_clock_res_usec = CLOCK_RES_USEC;

		_thr_page_size = getpagesize();
		_thr_guard_default = _thr_page_size;
		init_once = 1;	/* Don't do this again. */
	} else {
		/*
		 * Destroy the locks before creating them.  We don't
		 * know what state they are in so it is better to just
		 * recreate them.
		 */
		_lock_destroy(&_thread_signal_lock);
		_lock_destroy(&_mutex_static_lock);
		_lock_destroy(&_rwlock_static_lock);
		_lock_destroy(&_keytable_lock);
	}

	/* Initialize everything else. */
	TAILQ_INIT(&_thread_list);
	TAILQ_INIT(&_thread_gc_list);

	/*
	 * Initialize the lock for temporary installation of signal
	 * handlers (to support sigwait() semantics) and for the
	 * process signal mask and pending signal sets.
	 */
	if (_lock_init(&_thread_signal_lock, LCK_ADAPTIVE,
	    _kse_lock_wait, _kse_lock_wakeup) != 0)
		PANIC("Cannot initialize _thread_signal_lock");
	if (_lock_init(&_mutex_static_lock, LCK_ADAPTIVE,
	    _thr_lock_wait, _thr_lock_wakeup) != 0)
		PANIC("Cannot initialize mutex static init lock");
	if (_lock_init(&_rwlock_static_lock, LCK_ADAPTIVE,
	    _thr_lock_wait, _thr_lock_wakeup) != 0)
		PANIC("Cannot initialize rwlock static init lock");
	if (_lock_init(&_keytable_lock, LCK_ADAPTIVE,
	    _thr_lock_wait, _thr_lock_wakeup) != 0)
		PANIC("Cannot initialize thread specific keytable lock");
	_thr_spinlock_init();

	/* Clear pending signals and get the process signal mask. */
	SIGEMPTYSET(_thr_proc_sigpending);

	/*
	 * _thread_list_lock and _kse_count are initialized
	 * by _kse_init()
	 */
}
