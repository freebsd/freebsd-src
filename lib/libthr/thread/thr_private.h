/*
 * Copyright (C) 2005 Daniel M. Eischen <deischen@freebsd.org>
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _THR_PRIVATE_H
#define _THR_PRIVATE_H

/*
 * Include files.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/atomic.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/thr.h>
#include <pthread.h>

#define	SYM_FB10(sym)			__CONCAT(sym, _fb10)
#define	SYM_FBP10(sym)			__CONCAT(sym, _fbp10)
#define	WEAK_REF(sym, alias)		__weak_reference(sym, alias)
#define	SYM_COMPAT(sym, impl, ver)	__sym_compat(sym, impl, ver)
#define	SYM_DEFAULT(sym, impl, ver)	__sym_default(sym, impl, ver)

#define	FB10_COMPAT(func, sym)				\
	WEAK_REF(func, SYM_FB10(sym));			\
	SYM_COMPAT(sym, SYM_FB10(sym), FBSD_1.0)

#define	FB10_COMPAT_PRIVATE(func, sym)			\
	WEAK_REF(func, SYM_FBP10(sym));			\
	SYM_DEFAULT(sym, SYM_FBP10(sym), FBSDprivate_1.0)

#ifndef __hidden
#define __hidden		__attribute__((visibility("hidden")))
#endif

#include "pthread_md.h"
#include "thr_umtx.h"
#include "thread_db.h"

typedef TAILQ_HEAD(pthreadlist, pthread) pthreadlist;
typedef TAILQ_HEAD(atfork_head, pthread_atfork) atfork_head;
TAILQ_HEAD(mutex_queue, pthread_mutex);

/* Signal to do cancellation */
#define	SIGCANCEL		32

/*
 * Kernel fatal error handler macro.
 */
#define PANIC(string)		_thread_exit(__FILE__,__LINE__,string)

/* Output debug messages like this: */
#define stdout_debug(args...)	_thread_printf(STDOUT_FILENO, ##args)
#define stderr_debug(args...)	_thread_printf(STDERR_FILENO, ##args)

#ifdef _PTHREADS_INVARIANTS
#define THR_ASSERT(cond, msg) do {	\
	if (__predict_false(!(cond)))	\
		PANIC(msg);		\
} while (0)
#else
#define THR_ASSERT(cond, msg)
#endif

#ifdef PIC
# define STATIC_LIB_REQUIRE(name)
#else
# define STATIC_LIB_REQUIRE(name) __asm (".globl " #name)
#endif

#define	TIMESPEC_ADD(dst, src, val)				\
	do { 							\
		(dst)->tv_sec = (src)->tv_sec + (val)->tv_sec;	\
		(dst)->tv_nsec = (src)->tv_nsec + (val)->tv_nsec; \
		if ((dst)->tv_nsec >= 1000000000) {		\
			(dst)->tv_sec++;			\
			(dst)->tv_nsec -= 1000000000;		\
		}						\
	} while (0)

#define	TIMESPEC_SUB(dst, src, val)				\
	do { 							\
		(dst)->tv_sec = (src)->tv_sec - (val)->tv_sec;	\
		(dst)->tv_nsec = (src)->tv_nsec - (val)->tv_nsec; \
		if ((dst)->tv_nsec < 0) {			\
			(dst)->tv_sec--;			\
			(dst)->tv_nsec += 1000000000;		\
		}						\
	} while (0)

struct pthread_mutex {
	/*
	 * Lock for accesses to this structure.
	 */
	struct umutex			m_lock;
	enum pthread_mutextype		m_type;
	struct pthread			*m_owner;
	int				m_count;
	int				m_refcount;
	int				m_spinloops;
	int				m_yieldloops;
	/*
	 * Link for all mutexes a thread currently owns.
	 */
	TAILQ_ENTRY(pthread_mutex)	m_qe;
};

struct pthread_mutex_attr {
	enum pthread_mutextype	m_type;
	int			m_protocol;
	int			m_ceiling;
};

#define PTHREAD_MUTEXATTR_STATIC_INITIALIZER \
	{ PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE, 0, MUTEX_FLAGS_PRIVATE }

struct pthread_cond {
	struct umutex	c_lock;
	struct ucond	c_kerncv;
	int		c_pshared;
	int		c_clockid;
};

struct pthread_cond_attr {
	int		c_pshared;
	int		c_clockid;
};

struct pthread_barrier {
	struct umutex		b_lock;
	struct ucond		b_cv;
	volatile int64_t	b_cycle;
	volatile int		b_count;
	volatile int		b_waiters;
};

struct pthread_barrierattr {
	int		pshared;
};

struct pthread_spinlock {
	struct umutex	s_lock;
};

/*
 * Flags for condition variables.
 */
#define COND_FLAGS_PRIVATE	0x01
#define COND_FLAGS_INITED	0x02
#define COND_FLAGS_BUSY		0x04

/*
 * Cleanup definitions.
 */
struct pthread_cleanup {
	struct pthread_cleanup	*prev;
	void			(*routine)(void *);
	void			*routine_arg;
	int			onheap;
};

#define	THR_CLEANUP_PUSH(td, func, arg) {		\
	struct pthread_cleanup __cup;			\
							\
	__cup.routine = func;				\
	__cup.routine_arg = arg;			\
	__cup.onheap = 0;				\
	__cup.prev = (td)->cleanup;			\
	(td)->cleanup = &__cup;

#define	THR_CLEANUP_POP(td, exec)			\
	(td)->cleanup = __cup.prev;			\
	if ((exec) != 0)				\
		__cup.routine(__cup.routine_arg);	\
}

struct pthread_atfork {
	TAILQ_ENTRY(pthread_atfork) qe;
	void (*prepare)(void);
	void (*parent)(void);
	void (*child)(void);
};

struct pthread_attr {
	int	sched_policy;
	int	sched_inherit;
	int	prio;
	int	suspend;
#define	THR_STACK_USER		0x100	/* 0xFF reserved for <pthread.h> */
	int	flags;
	void	*stackaddr_attr;
	size_t	stacksize_attr;
	size_t	guardsize_attr;
	cpuset_t	*cpuset;
	size_t	cpusetsize;
};

/*
 * Thread creation state attributes.
 */
#define THR_CREATE_RUNNING		0
#define THR_CREATE_SUSPENDED		1

/*
 * Miscellaneous definitions.
 */
#define THR_STACK_DEFAULT		(sizeof(void *) / 4 * 1024 * 1024)

/*
 * Maximum size of initial thread's stack.  This perhaps deserves to be larger
 * than the stacks of other threads, since many applications are likely to run
 * almost entirely on this stack.
 */
#define THR_STACK_INITIAL		(THR_STACK_DEFAULT * 2)

/*
 * Define priorities returned by kernel.
 */
#define THR_MIN_PRIORITY		(_thr_priorities[SCHED_OTHER-1].pri_min)
#define THR_MAX_PRIORITY		(_thr_priorities[SCHED_OTHER-1].pri_max)
#define THR_DEF_PRIORITY		(_thr_priorities[SCHED_OTHER-1].pri_default)

#define THR_MIN_RR_PRIORITY		(_thr_priorities[SCHED_RR-1].pri_min)
#define THR_MAX_RR_PRIORITY		(_thr_priorities[SCHED_RR-1].pri_max)
#define THR_DEF_RR_PRIORITY		(_thr_priorities[SCHED_RR-1].pri_default)

/* XXX The SCHED_FIFO should have same priority range as SCHED_RR */
#define THR_MIN_FIFO_PRIORITY		(_thr_priorities[SCHED_FIFO_1].pri_min)
#define THR_MAX_FIFO_PRIORITY		(_thr_priorities[SCHED_FIFO-1].pri_max)
#define THR_DEF_FIFO_PRIORITY		(_thr_priorities[SCHED_FIFO-1].pri_default)

struct pthread_prio {
	int	pri_min;
	int	pri_max;
	int	pri_default;
};

struct pthread_rwlockattr {
	int		pshared;
};

struct pthread_rwlock {
	struct urwlock 	lock;
	struct pthread	*owner;
};

/*
 * Thread states.
 */
enum pthread_state {
	PS_RUNNING,
	PS_DEAD
};

struct pthread_specific_elem {
	const void	*data;
	int		seqno;
};

struct pthread_key {
	volatile int	allocated;
	int		seqno;
	void            (*destructor)(void *);
};

/*
 * lwpid_t is 32bit but kernel thr API exports tid as long type
 * in very earily date.
 */
#define TID(thread)	((uint32_t) ((thread)->tid))

/*
 * Thread structure.
 */
struct pthread {
	/* Kernel thread id. */
	long			tid;
#define	TID_TERMINATED		1

	/*
	 * Lock for accesses to this thread structure.
	 */
	struct umutex		lock;

	/* Internal condition variable cycle number. */
	uint32_t		cycle;

	/* How many low level locks the thread held. */
	int			locklevel;

	/*
	 * Set to non-zero when this thread has entered a critical
	 * region.  We allow for recursive entries into critical regions.
	 */
	int			critical_count;

	/* Signal blocked counter. */
	int			sigblock;

	/* Queue entry for list of all threads. */
	TAILQ_ENTRY(pthread)	tle;	/* link for all threads in process */

	/* Queue entry for GC lists. */
	TAILQ_ENTRY(pthread)	gcle;

	/* Hash queue entry. */
	LIST_ENTRY(pthread)	hle;

	/* Threads reference count. */
	int			refcount;

	/*
	 * Thread start routine, argument, stack pointer and thread
	 * attributes.
	 */
	void			*(*start_routine)(void *);
	void			*arg;
	struct pthread_attr	attr;

#define	SHOULD_CANCEL(thr)					\
	((thr)->cancel_pending &&				\
	 ((thr)->cancel_point || (thr)->cancel_async) &&	\
	 (thr)->cancel_enable && (thr)->cancelling == 0)

	/* Cancellation is enabled */
	int			cancel_enable;

	/* Cancellation request is pending */
	int			cancel_pending;

	/* Thread is at cancellation point */
	int			cancel_point;

	/* Cancellation should be synchoronized */
	int			cancel_defer;

	/* Asynchronouse cancellation is enabled */
	int			cancel_async;

	/* Cancellation is in progress */
	int			cancelling;

	/* Thread temporary signal mask. */
	sigset_t		sigmask;

	/* Thread is in SIGCANCEL handler. */
	int			in_sigcancel_handler;

	/* New thread should unblock SIGCANCEL. */
	int			unblock_sigcancel;

	/* Force new thread to exit. */
	int			force_exit;

	/* Thread state: */
	enum pthread_state 	state;

	/*
	 * Error variable used instead of errno. The function __error()
	 * returns a pointer to this. 
	 */
	int			error;

	/*
	 * The joiner is the thread that is joining to this thread.  The
	 * join status keeps track of a join operation to another thread.
	 */
	struct pthread		*joiner;

	/* Miscellaneous flags; only set with scheduling lock held. */
	int			flags;
#define THR_FLAGS_PRIVATE	0x0001
#define	THR_FLAGS_NEED_SUSPEND	0x0002	/* thread should be suspended */
#define	THR_FLAGS_SUSPENDED	0x0004	/* thread is suspended */

	/* Thread list flags; only set with thread list lock held. */
	int			tlflags;
#define	TLFLAGS_GC_SAFE		0x0001	/* thread safe for cleaning */
#define	TLFLAGS_IN_TDLIST	0x0002	/* thread in all thread list */
#define	TLFLAGS_IN_GCLIST	0x0004	/* thread in gc list */
#define	TLFLAGS_DETACHED	0x0008	/* thread is detached */

	/* Queue of currently owned NORMAL or PRIO_INHERIT type mutexes. */
	struct mutex_queue	mutexq;

	/* Queue of all owned PRIO_PROTECT mutexes. */
	struct mutex_queue	pp_mutexq;

	void				*ret;
	struct pthread_specific_elem	*specific;
	int				specific_data_count;

	/* Number rwlocks rdlocks held. */
	int			rdlock_count;

	/*
	 * Current locks bitmap for rtld. */
	int			rtld_bits;

	/* Thread control block */
	struct tcb		*tcb;

	/* Cleanup handlers Link List */
	struct pthread_cleanup	*cleanup;

	/*
	 * Magic value to help recognize a valid thread structure
	 * from an invalid one:
	 */
#define	THR_MAGIC		((u_int32_t) 0xd09ba115)
	u_int32_t		magic;

	/* Enable event reporting */
	int			report_events;

	/* Event mask */
	int			event_mask;

	/* Event */
	td_event_msg_t		event_buf;
};

#define	THR_IN_CRITICAL(thrd)				\
	(((thrd)->locklevel > 0) ||			\
	((thrd)->critical_count > 0))

#define	THR_CRITICAL_ENTER(thrd)			\
	(thrd)->critical_count++

#define	THR_CRITICAL_LEAVE(thrd)			\
	do {						\
		(thrd)->critical_count--;		\
		_thr_ast(thrd);				\
	} while (0)

#define THR_UMUTEX_TRYLOCK(thrd, lck)			\
	_thr_umutex_trylock((lck), TID(thrd))

#define	THR_UMUTEX_LOCK(thrd, lck)			\
	_thr_umutex_lock((lck), TID(thrd))

#define	THR_UMUTEX_TIMEDLOCK(thrd, lck, timo)		\
	_thr_umutex_timedlock((lck), TID(thrd), (timo))

#define	THR_UMUTEX_UNLOCK(thrd, lck)			\
	_thr_umutex_unlock((lck), TID(thrd))

#define	THR_LOCK_ACQUIRE(thrd, lck)			\
do {							\
	(thrd)->locklevel++;				\
	_thr_umutex_lock(lck, TID(thrd));		\
} while (0)

#ifdef	_PTHREADS_INVARIANTS
#define	THR_ASSERT_LOCKLEVEL(thrd)			\
do {							\
	if (__predict_false((thrd)->locklevel <= 0))	\
		_thr_assert_lock_level();		\
} while (0)
#else
#define THR_ASSERT_LOCKLEVEL(thrd)
#endif

#define	THR_LOCK_RELEASE(thrd, lck)			\
do {							\
	THR_ASSERT_LOCKLEVEL(thrd);			\
	_thr_umutex_unlock((lck), TID(thrd));		\
	(thrd)->locklevel--;				\
	_thr_ast(thrd);					\
} while (0)

#define	THR_LOCK(curthrd)		THR_LOCK_ACQUIRE(curthrd, &(curthrd)->lock)
#define	THR_UNLOCK(curthrd)		THR_LOCK_RELEASE(curthrd, &(curthrd)->lock)
#define	THR_THREAD_LOCK(curthrd, thr)	THR_LOCK_ACQUIRE(curthrd, &(thr)->lock)
#define	THR_THREAD_UNLOCK(curthrd, thr)	THR_LOCK_RELEASE(curthrd, &(thr)->lock)

#define	THREAD_LIST_LOCK(curthrd)				\
do {								\
	THR_LOCK_ACQUIRE((curthrd), &_thr_list_lock);		\
} while (0)

#define	THREAD_LIST_UNLOCK(curthrd)				\
do {								\
	THR_LOCK_RELEASE((curthrd), &_thr_list_lock);		\
} while (0)

/*
 * Macros to insert/remove threads to the all thread list and
 * the gc list.
 */
#define	THR_LIST_ADD(thrd) do {					\
	if (((thrd)->tlflags & TLFLAGS_IN_TDLIST) == 0) {	\
		TAILQ_INSERT_HEAD(&_thread_list, thrd, tle);	\
		_thr_hash_add(thrd);				\
		(thrd)->tlflags |= TLFLAGS_IN_TDLIST;		\
	}							\
} while (0)
#define	THR_LIST_REMOVE(thrd) do {				\
	if (((thrd)->tlflags & TLFLAGS_IN_TDLIST) != 0) {	\
		TAILQ_REMOVE(&_thread_list, thrd, tle);		\
		_thr_hash_remove(thrd);				\
		(thrd)->tlflags &= ~TLFLAGS_IN_TDLIST;		\
	}							\
} while (0)
#define	THR_GCLIST_ADD(thrd) do {				\
	if (((thrd)->tlflags & TLFLAGS_IN_GCLIST) == 0) {	\
		TAILQ_INSERT_HEAD(&_thread_gc_list, thrd, gcle);\
		(thrd)->tlflags |= TLFLAGS_IN_GCLIST;		\
		_gc_count++;					\
	}							\
} while (0)
#define	THR_GCLIST_REMOVE(thrd) do {				\
	if (((thrd)->tlflags & TLFLAGS_IN_GCLIST) != 0) {	\
		TAILQ_REMOVE(&_thread_gc_list, thrd, gcle);	\
		(thrd)->tlflags &= ~TLFLAGS_IN_GCLIST;		\
		_gc_count--;					\
	}							\
} while (0)

#define GC_NEEDED()	(_gc_count >= 5)

#define SHOULD_REPORT_EVENT(curthr, e)			\
	(curthr->report_events && 			\
	 (((curthr)->event_mask | _thread_event_mask ) & e) != 0)

extern int __isthreaded;

/*
 * Global variables for the pthread kernel.
 */

extern char		*_usrstack __hidden;
extern struct pthread	*_thr_initial __hidden;

/* For debugger */
extern int		_libthr_debug;
extern int		_thread_event_mask;
extern struct pthread	*_thread_last_event;

/* List of all threads: */
extern pthreadlist	_thread_list;

/* List of threads needing GC: */
extern pthreadlist	_thread_gc_list __hidden;

extern int		_thread_active_threads;
extern atfork_head	_thr_atfork_list __hidden;
extern struct umutex	_thr_atfork_lock __hidden;

/* Default thread attributes: */
extern struct pthread_attr _pthread_attr_default __hidden;

/* Default mutex attributes: */
extern struct pthread_mutex_attr _pthread_mutexattr_default __hidden;

/* Default condition variable attributes: */
extern struct pthread_cond_attr _pthread_condattr_default __hidden;

extern struct pthread_prio _thr_priorities[] __hidden;

extern pid_t	_thr_pid __hidden;
extern int	_thr_is_smp __hidden;

extern size_t	_thr_guard_default __hidden;
extern size_t	_thr_stack_default __hidden;
extern size_t	_thr_stack_initial __hidden;
extern int	_thr_page_size __hidden;
extern int	_thr_spinloops __hidden;
extern int	_thr_yieldloops __hidden;

/* Garbage thread count. */
extern int	_gc_count __hidden;

extern struct umutex	_mutex_static_lock __hidden;
extern struct umutex	_cond_static_lock __hidden;
extern struct umutex	_rwlock_static_lock __hidden;
extern struct umutex	_keytable_lock __hidden;
extern struct umutex	_thr_list_lock __hidden;
extern struct umutex	_thr_event_lock __hidden;

/*
 * Function prototype definitions.
 */
__BEGIN_DECLS
int	_thr_setthreaded(int) __hidden;
int	_mutex_cv_lock(pthread_mutex_t *, int count) __hidden;
int	_mutex_cv_unlock(pthread_mutex_t *, int *count) __hidden;
int	_mutex_reinit(pthread_mutex_t *) __hidden;
void	_mutex_fork(struct pthread *curthread) __hidden;
void	_libpthread_init(struct pthread *) __hidden;
struct pthread *_thr_alloc(struct pthread *) __hidden;
void	_thread_exit(const char *, int, const char *) __hidden __dead2;
void	_thr_exit_cleanup(void) __hidden;
int	_thr_ref_add(struct pthread *, struct pthread *, int) __hidden;
void	_thr_ref_delete(struct pthread *, struct pthread *) __hidden;
void	_thr_ref_delete_unlocked(struct pthread *, struct pthread *) __hidden;
int	_thr_find_thread(struct pthread *, struct pthread *, int) __hidden;
void	_thr_rtld_init(void) __hidden;
void	_thr_rtld_fini(void) __hidden;
int	_thr_stack_alloc(struct pthread_attr *) __hidden;
void	_thr_stack_free(struct pthread_attr *) __hidden;
void	_thr_free(struct pthread *, struct pthread *) __hidden;
void	_thr_gc(struct pthread *) __hidden;
void    _thread_cleanupspecific(void) __hidden;
void    _thread_dump_info(void) __hidden;
void	_thread_printf(int, const char *, ...) __hidden;
void	_thr_spinlock_init(void) __hidden;
void	_thr_cancel_enter(struct pthread *) __hidden;
void	_thr_cancel_leave(struct pthread *) __hidden;
void	_thr_cancel_enter_defer(struct pthread *) __hidden;
void	_thr_cancel_leave_defer(struct pthread *, int) __hidden;
void	_thr_testcancel(struct pthread *) __hidden;
void	_thr_signal_block(struct pthread *) __hidden;
void	_thr_signal_unblock(struct pthread *) __hidden;
void	_thr_signal_init(void) __hidden;
void	_thr_signal_deinit(void) __hidden;
int	_thr_send_sig(struct pthread *, int sig) __hidden;
void	_thr_list_init(void) __hidden;
void	_thr_hash_add(struct pthread *) __hidden;
void	_thr_hash_remove(struct pthread *) __hidden;
struct pthread *_thr_hash_find(struct pthread *) __hidden;
void	_thr_link(struct pthread *, struct pthread *) __hidden;
void	_thr_unlink(struct pthread *, struct pthread *) __hidden;
void	_thr_suspend_check(struct pthread *) __hidden;
void	_thr_assert_lock_level(void) __hidden __dead2;
void	_thr_ast(struct pthread *) __hidden;
void	_thr_once_init(void) __hidden;
void	_thr_report_creation(struct pthread *curthread,
	    struct pthread *newthread) __hidden;
void	_thr_report_death(struct pthread *curthread) __hidden;
int	_thr_getscheduler(lwpid_t, int *, struct sched_param *) __hidden;
int	_thr_setscheduler(lwpid_t, int, const struct sched_param *) __hidden;
int	_rtp_to_schedparam(const struct rtprio *rtp, int *policy,
		struct sched_param *param) __hidden;
int	_schedparam_to_rtp(int policy, const struct sched_param *param,
		struct rtprio *rtp) __hidden;
void	_thread_bp_create(void);
void	_thread_bp_death(void);
int	_sched_yield(void);
void	_thr_sem_prefork(void);
void	_thr_sem_postfork(void);
void	_thr_sem_child_postfork(void);

void	_pthread_cleanup_push(void (*)(void *), void *);
void	_pthread_cleanup_pop(int);

/* #include <fcntl.h> */
#ifdef  _SYS_FCNTL_H_
int     __sys_fcntl(int, int, ...);
int     __sys_open(const char *, int, ...);
int     __sys_openat(int, const char *, int, ...);
#endif

/* #include <signal.h> */
#ifdef _SIGNAL_H_
int	__sys_kill(pid_t, int);
int     __sys_sigaction(int, const struct sigaction *, struct sigaction *);
int     __sys_sigpending(sigset_t *);
int     __sys_sigprocmask(int, const sigset_t *, sigset_t *);
int     __sys_sigsuspend(const sigset_t *);
int     __sys_sigreturn(ucontext_t *);
int     __sys_sigaltstack(const struct sigaltstack *, struct sigaltstack *);
int	__sys_sigwait(const sigset_t *, int *);
int	__sys_sigtimedwait(const sigset_t *, siginfo_t *,
		const struct timespec *);
int	__sys_sigwaitinfo(const sigset_t *set, siginfo_t *info);
#endif

/* #include <time.h> */
#ifdef	_TIME_H_
int	__sys_nanosleep(const struct timespec *, struct timespec *);
#endif

/* #include <unistd.h> */
#ifdef  _UNISTD_H_
int     __sys_close(int);
int	__sys_fork(void);
pid_t	__sys_getpid(void);
ssize_t __sys_read(int, void *, size_t);
ssize_t __sys_write(int, const void *, size_t);
void	__sys_exit(int);
#endif

int	_umtx_op_err(void *, int op, u_long, void *, void *) __hidden;

static inline int
_thr_isthreaded(void)
{
	return (__isthreaded != 0);
}

static inline int
_thr_is_inited(void)
{
	return (_thr_initial != NULL);
}

static inline void
_thr_check_init(void)
{
	if (_thr_initial == NULL)
		_libpthread_init(NULL);
}

__END_DECLS

#endif  /* !_THR_PRIVATE_H */
