/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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
 */

#include "namespace.h"
#include <sys/param.h>
#include <sys/auxv.h>
#include <sys/signalvar.h>
#include <sys/ioctl.h>
#include <sys/link_elf.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/ttycom.h>
#include <sys/mman.h>
#include <sys/rtprio.h>
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

char		*_usrstack;
struct pthread	*_thr_initial;
int		_libthr_debug;
int		_thread_event_mask;
struct pthread	*_thread_last_event;
pthreadlist	_thread_list = TAILQ_HEAD_INITIALIZER(_thread_list);
pthreadlist 	_thread_gc_list = TAILQ_HEAD_INITIALIZER(_thread_gc_list);
int		_thread_active_threads = 1;
atfork_head	_thr_atfork_list = TAILQ_HEAD_INITIALIZER(_thr_atfork_list);
struct urwlock	_thr_atfork_lock = DEFAULT_URWLOCK;

struct pthread_prio	_thr_priorities[3] = {
	{RTP_PRIO_MIN,  RTP_PRIO_MAX, 0}, /* FIFO */
	{0, 0, 63}, /* OTHER */
	{RTP_PRIO_MIN, RTP_PRIO_MAX, 0}  /* RR */
};

struct pthread_attr _pthread_attr_default = {
	.sched_policy = SCHED_OTHER,
	.sched_inherit = PTHREAD_INHERIT_SCHED,
	.prio = 0,
	.suspend = THR_CREATE_RUNNING,
	.flags = PTHREAD_SCOPE_SYSTEM,
	.stackaddr_attr = NULL,
	.stacksize_attr = THR_STACK_DEFAULT,
	.guardsize_attr = 0,
	.cpusetsize = 0,
	.cpuset = NULL
};

struct pthread_mutex_attr _pthread_mutexattr_default = {
	.m_type = PTHREAD_MUTEX_DEFAULT,
	.m_protocol = PTHREAD_PRIO_NONE,
	.m_ceiling = 0,
	.m_pshared = PTHREAD_PROCESS_PRIVATE,
	.m_robust = PTHREAD_MUTEX_STALLED,
};

struct pthread_mutex_attr _pthread_mutexattr_adaptive_default = {
	.m_type = PTHREAD_MUTEX_ADAPTIVE_NP,
	.m_protocol = PTHREAD_PRIO_NONE,
	.m_ceiling = 0,
	.m_pshared = PTHREAD_PROCESS_PRIVATE,
	.m_robust = PTHREAD_MUTEX_STALLED,
};

/* Default condition variable attributes: */
struct pthread_cond_attr _pthread_condattr_default = {
	.c_pshared = PTHREAD_PROCESS_PRIVATE,
	.c_clockid = CLOCK_REALTIME
};

int		_thr_is_smp = 0;
size_t		_thr_guard_default;
size_t		_thr_stack_default = THR_STACK_DEFAULT;
size_t		_thr_stack_initial = THR_STACK_INITIAL;
int		_thr_page_size;
int		_thr_spinloops;
int		_thr_yieldloops;
int		_thr_queuefifo = 4;
int		_gc_count;
struct umutex	_mutex_static_lock = DEFAULT_UMUTEX;
struct umutex	_cond_static_lock = DEFAULT_UMUTEX;
struct umutex	_rwlock_static_lock = DEFAULT_UMUTEX;
struct umutex	_keytable_lock = DEFAULT_UMUTEX;
struct urwlock	_thr_list_lock = DEFAULT_URWLOCK;
struct umutex	_thr_event_lock = DEFAULT_UMUTEX;
struct umutex	_suspend_all_lock = DEFAULT_UMUTEX;
struct pthread	*_single_thread;
int		_suspend_all_cycle;
int		_suspend_all_waiters;

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

STATIC_LIB_REQUIRE(_fork);
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
STATIC_LIB_REQUIRE(_raise);
STATIC_LIB_REQUIRE(_sem_destroy);
STATIC_LIB_REQUIRE(_sem_getvalue);
STATIC_LIB_REQUIRE(_sem_init);
STATIC_LIB_REQUIRE(_sem_post);
STATIC_LIB_REQUIRE(_sem_timedwait);
STATIC_LIB_REQUIRE(_sem_trywait);
STATIC_LIB_REQUIRE(_sem_wait);
STATIC_LIB_REQUIRE(_sigaction);
STATIC_LIB_REQUIRE(_sigprocmask);
STATIC_LIB_REQUIRE(_sigsuspend);
STATIC_LIB_REQUIRE(_sigtimedwait);
STATIC_LIB_REQUIRE(_sigwait);
STATIC_LIB_REQUIRE(_sigwaitinfo);
STATIC_LIB_REQUIRE(_spinlock);
STATIC_LIB_REQUIRE(_spinunlock);
STATIC_LIB_REQUIRE(_thread_init_hack);

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
	[PJT_ATFORK] = {DUAL_ENTRY(_thr_atfork)},
	[PJT_ATTR_DESTROY] = {DUAL_ENTRY(_thr_attr_destroy)},
	[PJT_ATTR_GETDETACHSTATE] = {DUAL_ENTRY(_thr_attr_getdetachstate)},
	[PJT_ATTR_GETGUARDSIZE] = {DUAL_ENTRY(_thr_attr_getguardsize)},
	[PJT_ATTR_GETINHERITSCHED] = {DUAL_ENTRY(_thr_attr_getinheritsched)},
	[PJT_ATTR_GETSCHEDPARAM] = {DUAL_ENTRY(_thr_attr_getschedparam)},
	[PJT_ATTR_GETSCHEDPOLICY] = {DUAL_ENTRY(_thr_attr_getschedpolicy)},
	[PJT_ATTR_GETSCOPE] = {DUAL_ENTRY(_thr_attr_getscope)},
	[PJT_ATTR_GETSTACKADDR] = {DUAL_ENTRY(_thr_attr_getstackaddr)},
	[PJT_ATTR_GETSTACKSIZE] = {DUAL_ENTRY(_thr_attr_getstacksize)},
	[PJT_ATTR_INIT] = {DUAL_ENTRY(_thr_attr_init)},
	[PJT_ATTR_SETDETACHSTATE] = {DUAL_ENTRY(_thr_attr_setdetachstate)},
	[PJT_ATTR_SETGUARDSIZE] = {DUAL_ENTRY(_thr_attr_setguardsize)},
	[PJT_ATTR_SETINHERITSCHED] = {DUAL_ENTRY(_thr_attr_setinheritsched)},
	[PJT_ATTR_SETSCHEDPARAM] = {DUAL_ENTRY(_thr_attr_setschedparam)},
	[PJT_ATTR_SETSCHEDPOLICY] = {DUAL_ENTRY(_thr_attr_setschedpolicy)},
	[PJT_ATTR_SETSCOPE] = {DUAL_ENTRY(_thr_attr_setscope)},
	[PJT_ATTR_SETSTACKADDR] = {DUAL_ENTRY(_thr_attr_setstackaddr)},
	[PJT_ATTR_SETSTACKSIZE] = {DUAL_ENTRY(_thr_attr_setstacksize)},
	[PJT_CANCEL] = {DUAL_ENTRY(_thr_cancel)},
	[PJT_CLEANUP_POP] = {DUAL_ENTRY(_thr_cleanup_pop)},
	[PJT_CLEANUP_PUSH] = {DUAL_ENTRY(_thr_cleanup_push)},
	[PJT_COND_BROADCAST] = {DUAL_ENTRY(_thr_cond_broadcast)},
	[PJT_COND_DESTROY] = {DUAL_ENTRY(_thr_cond_destroy)},
	[PJT_COND_INIT] = {DUAL_ENTRY(_thr_cond_init)},
	[PJT_COND_SIGNAL] = {DUAL_ENTRY(_thr_cond_signal)},
	[PJT_COND_TIMEDWAIT] = {DUAL_ENTRY(_thr_cond_timedwait)},
	[PJT_COND_WAIT] = {(pthread_func_t)__thr_cond_wait,
	    (pthread_func_t)_thr_cond_wait},
	[PJT_DETACH] = {DUAL_ENTRY(_thr_detach)},
	[PJT_EQUAL] = {DUAL_ENTRY(_thr_equal)},
	[PJT_EXIT] = {DUAL_ENTRY(_Tthr_exit)},
	[PJT_GETSPECIFIC] = {DUAL_ENTRY(_thr_getspecific)},
	[PJT_JOIN] = {DUAL_ENTRY(_thr_join)},
	[PJT_KEY_CREATE] = {DUAL_ENTRY(_thr_key_create)},
	[PJT_KEY_DELETE] = {DUAL_ENTRY(_thr_key_delete)},
	[PJT_KILL] = {DUAL_ENTRY(_Tthr_kill)},
	[PJT_MAIN_NP] = {DUAL_ENTRY(_thr_main_np)},
	[PJT_MUTEXATTR_DESTROY] = {DUAL_ENTRY(_thr_mutexattr_destroy)},
	[PJT_MUTEXATTR_INIT] = {DUAL_ENTRY(_thr_mutexattr_init)},
	[PJT_MUTEXATTR_SETTYPE] = {DUAL_ENTRY(_thr_mutexattr_settype)},
	[PJT_MUTEX_DESTROY] = {DUAL_ENTRY(_thr_mutex_destroy)},
	[PJT_MUTEX_INIT] = {DUAL_ENTRY(__Tthr_mutex_init)},
	[PJT_MUTEX_LOCK] = {DUAL_ENTRY(__Tthr_mutex_lock)},
	[PJT_MUTEX_TRYLOCK] = {DUAL_ENTRY(__Tthr_mutex_trylock)},
	[PJT_MUTEX_UNLOCK] = {DUAL_ENTRY(_thr_mutex_unlock)},
	[PJT_ONCE] = {DUAL_ENTRY(_thr_once)},
	[PJT_RWLOCK_DESTROY] = {DUAL_ENTRY(_thr_rwlock_destroy)},
	[PJT_RWLOCK_INIT] = {DUAL_ENTRY(_thr_rwlock_init)},
	[PJT_RWLOCK_RDLOCK] = {DUAL_ENTRY(_Tthr_rwlock_rdlock)},
	[PJT_RWLOCK_TRYRDLOCK] = {DUAL_ENTRY(_Tthr_rwlock_tryrdlock)},
	[PJT_RWLOCK_TRYWRLOCK] = {DUAL_ENTRY(_Tthr_rwlock_trywrlock)},
	[PJT_RWLOCK_UNLOCK] = {DUAL_ENTRY(_Tthr_rwlock_unlock)},
	[PJT_RWLOCK_WRLOCK] = {DUAL_ENTRY(_Tthr_rwlock_wrlock)},
	[PJT_SELF] = {DUAL_ENTRY(_Tthr_self)},
	[PJT_SETCANCELSTATE] = {DUAL_ENTRY(_thr_setcancelstate)},
	[PJT_SETCANCELTYPE] = {DUAL_ENTRY(_thr_setcanceltype)},
	[PJT_SETSPECIFIC] = {DUAL_ENTRY(_thr_setspecific)},
	[PJT_SIGMASK] = {DUAL_ENTRY(_thr_sigmask)},
	[PJT_TESTCANCEL] = {DUAL_ENTRY(_Tthr_testcancel)},
	[PJT_CLEANUP_POP_IMP] = {DUAL_ENTRY(__thr_cleanup_pop_imp)},
	[PJT_CLEANUP_PUSH_IMP] = {DUAL_ENTRY(__thr_cleanup_push_imp)},
	[PJT_CANCEL_ENTER] = {DUAL_ENTRY(_Tthr_cancel_enter)},
	[PJT_CANCEL_LEAVE] = {DUAL_ENTRY(_Tthr_cancel_leave)},
	[PJT_MUTEX_CONSISTENT] = {DUAL_ENTRY(_Tthr_mutex_consistent)},
	[PJT_MUTEXATTR_GETROBUST] = {DUAL_ENTRY(_thr_mutexattr_getrobust)},
	[PJT_MUTEXATTR_SETROBUST] = {DUAL_ENTRY(_thr_mutexattr_setrobust)},
	[PJT_GETTHREADID_NP] = {DUAL_ENTRY(_thr_getthreadid_np)},
	[PJT_ATTR_GET_NP] = {DUAL_ENTRY(_thr_attr_get_np)},
	[PJT_GETNAME_NP] = {DUAL_ENTRY(_thr_getname_np)},
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
	int first, dlopened;

	/* Check if this function has already been called: */
	if (_thr_initial != NULL && curthread == NULL)
		/* Only initialize the threaded application once. */
		return;

	/*
	 * Check the size of the jump table to make sure it is preset
	 * with the correct number of entries.
	 */
	if (sizeof(jmp_table) != sizeof(pthread_func_t) * PJT_MAX * 2)
		PANIC("Thread jump table not properly initialized");
	memcpy(__thr_jtable, jmp_table, sizeof(jmp_table));
	__thr_interpose_libc();

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
	} else {
		first = 0;
	}
		
	/*
	 * Add the thread to the thread list queue.
	 */
	THR_LIST_ADD(curthread);
	_thread_active_threads = 1;

	/* Setup the thread specific data */
	_tcb_set(curthread->tcb);

	if (first) {
		_thr_initial = curthread;
		dlopened = _rtld_is_dlopened(&_thread_autoinit_dummy_decl) != 0;
		_thr_signal_init(dlopened);
		if (_thread_event_mask & TD_CREATE)
			_thr_report_creation(curthread, curthread);
		/*
		 * Always use our rtld lock implementation.
		 * It is faster because it postpones signal handlers
		 * instead of calling sigprocmask(2).
		 */
		_thr_rtld_init();
	}
}

/*
 * This function and pthread_create() do a lot of the same things.
 * It'd be nice to consolidate the common stuff in one place.
 */
static void
init_main_thread(struct pthread *thread)
{
	struct sched_param sched_param;
	int i;

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
	if (mmap(_usrstack - _thr_stack_initial -
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
	thread->attr.stackaddr_attr = _usrstack - _thr_stack_initial;
	thread->attr.stacksize_attr = _thr_stack_initial;
	thread->attr.guardsize_attr = _thr_guard_default;
	thread->attr.flags |= THR_STACK_USER;

	/*
	 * Write a magic value to the thread structure
	 * to help identify valid ones:
	 */
	thread->magic = THR_MAGIC;

	thread->cancel_enable = 1;
	thread->cancel_async = 0;

	/* Initialize the mutex queues */
	for (i = 0; i < TMQ_NITEMS; i++)
		TAILQ_INIT(&thread->mq[i]);

	thread->state = PS_RUNNING;

	_thr_getscheduler(thread->tid, &thread->attr.sched_policy,
		 &sched_param);
	thread->attr.prio = sched_param.sched_priority;

#ifdef _PTHREAD_FORCED_UNWIND
	thread->unwind_stackend = _usrstack;
#endif

	/* Others cleared to zero by thr_alloc() */
}

bool
__thr_get_main_stack_base(char **base)
{
	size_t len;
	int mib[2];

	if (elf_aux_info(AT_USRSTACKBASE, base, sizeof(*base)) == 0)
		return (true);

	mib[0] = CTL_KERN;
	mib[1] = KERN_USRSTACK;
	len = sizeof(*base);
	if (sysctl(mib, nitems(mib), base, &len, NULL, 0) == 0)
		return (true);

	return (false);
}

bool
__thr_get_main_stack_lim(size_t *lim)
{
	struct rlimit rlim;

	if (elf_aux_info(AT_USRSTACKLIM, lim, sizeof(*lim)) == 0)
		return (true);

	if (getrlimit(RLIMIT_STACK, &rlim) == 0) {
		*lim = rlim.rlim_cur;
		return (true);
	}

	return (false);
}

static void
init_private(void)
{
	char *env, *env_bigstack, *env_splitstack;

	_thr_umutex_init(&_mutex_static_lock);
	_thr_umutex_init(&_cond_static_lock);
	_thr_umutex_init(&_rwlock_static_lock);
	_thr_umutex_init(&_keytable_lock);
	_thr_urwlock_init(&_thr_atfork_lock);
	_thr_umutex_init(&_thr_event_lock);
	_thr_umutex_init(&_suspend_all_lock);
	_thr_spinlock_init();
	_thr_list_init();
	_thr_wake_addr_init();
	_sleepq_init();
	_single_thread = NULL;
	_suspend_all_waiters = 0;

	/*
	 * Avoid reinitializing some things if they don't need to be,
	 * e.g. after a fork().
	 */
	if (init_once == 0) {
		__thr_pshared_init();
		__thr_malloc_init();

		/* Find the stack top */
		if (!__thr_get_main_stack_base(&_usrstack))
			PANIC("Cannot get kern.usrstack");
		env_bigstack = getenv("LIBPTHREAD_BIGSTACK_MAIN");
		env_splitstack = getenv("LIBPTHREAD_SPLITSTACK_MAIN");
		if (env_bigstack != NULL || env_splitstack == NULL) {
			if (!__thr_get_main_stack_lim(&_thr_stack_initial))
				PANIC("Cannot get stack rlimit");
		}
		_thr_is_smp = sysconf(_SC_NPROCESSORS_CONF);
		if (_thr_is_smp == -1)
			PANIC("Cannot get _SC_NPROCESSORS_CONF");
		_thr_is_smp = (_thr_is_smp > 1);
		_thr_page_size = getpagesize();
		_thr_guard_default = _thr_page_size;
		_pthread_attr_default.guardsize_attr = _thr_guard_default;
		_pthread_attr_default.stacksize_attr = _thr_stack_default;
		env = getenv("LIBPTHREAD_SPINLOOPS");
		if (env)
			_thr_spinloops = atoi(env);
		env = getenv("LIBPTHREAD_YIELDLOOPS");
		if (env)
			_thr_yieldloops = atoi(env);
		env = getenv("LIBPTHREAD_QUEUE_FIFO");
		if (env)
			_thr_queuefifo = atoi(env);
		TAILQ_INIT(&_thr_atfork_list);
		env = getenv("LIBPTHREAD_UMTX_MIN_TIMEOUT");
		if (env) {
			char *endptr;
			long mint;

			mint = strtol(env, &endptr, 0);
			if (*endptr == '\0' && mint >= 0) {
				_umtx_op(NULL, UMTX_OP_SET_MIN_TIMEOUT,
				    mint, NULL, NULL);
			}
		}
	}
	init_once = 1;
}
