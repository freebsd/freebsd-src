/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>.
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
 * Private thread definitions for the uthread kernel.
 *
 * $FreeBSD$
 */

#ifndef _THR_PRIVATE_H
#define _THR_PRIVATE_H

/*
 * Evaluate the storage class specifier.
 */
#ifdef GLOBAL_PTHREAD_PRIVATE
#define SCLASS
#else
#define SCLASS extern
#endif

/*
 * Include files.
 */
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <pthread_np.h>
#include <sched.h>
#include <signal.h>
#include <spinlock.h>
#include <stdio.h>
#include <ucontext.h>

#include <machine/atomic.h>
#include <sys/thr.h>
#include <sys/umtx.h>

/*
 * Kernel fatal error handler macro.
 */
#define PANIC(string)   _thread_exit(__FILE__,__LINE__,string)


/* Output debug messages like this: */
#define stdout_debug(args...)	_thread_printf(STDOUT_FILENO, args)
#define stderr_debug(args...)	_thread_printf(STDOUT_FILENO, args)

/*
 * Currently executing thread.
 */
#define	curthread	_get_curthread()

/*
 * State change macro without scheduling queue change:
 */
#define PTHREAD_SET_STATE(thrd, newstate) do {				\
	(thrd)->state = newstate;					\
	(thrd)->fname = __FILE__;					\
	(thrd)->lineno = __LINE__;					\
} while (0)

/*
 * State change macro with scheduling queue change - This must be
 * called with GIANT held.
 */
#if defined(_PTHREADS_INVARIANTS)
#include <assert.h>
#define PTHREAD_ASSERT(cond, msg) do {	\
	if (!(cond))			\
		PANIC(msg);		\
} while (0)
#define PTHREAD_ASSERT_NOT_IN_SYNCQ(thrd) \
	PTHREAD_ASSERT((((thrd)->flags & PTHREAD_FLAGS_IN_SYNCQ) == 0),	\
	    "Illegal call from signal handler");
#define PTHREAD_NEW_STATE(thrd, newstate) do {				\
	if ((thrd)->state != newstate) {				\
		if ((thrd)->state == PS_RUNNING) {			\
			PTHREAD_SET_STATE(thrd, newstate);		\
		} else if (newstate == PS_RUNNING) { 			\
			if (thr_kill(thrd->thr_id, SIGTHR))		\
				abort();				\
			PTHREAD_SET_STATE(thrd, newstate);		\
		}							\
	}								\
} while (0)
#else
#define PTHREAD_ASSERT(cond, msg)
#define PTHREAD_ASSERT_NOT_IN_SYNCQ(thrd)
#define PTHREAD_NEW_STATE(thrd, newstate) do {				\
	if (thr_kill(thrd->thr_id, SIGTHR))				\
		abort();						\
	PTHREAD_SET_STATE(thrd, newstate);				\
} while (0)
#if 0
#define PTHREAD_NEW_STATE(thrd, newstate) do {				\
	if ((thrd)->state != newstate) {				\
		if ((thrd)->state == PS_RUNNING) {			\
		} else if (newstate == PS_RUNNING) { 			\
			if (thr_kill(thrd->thr_id, SIGTHR))		\
				abort();				\
		}							\
	}								\
	PTHREAD_SET_STATE(thrd, newstate);				\
} while (0)
#endif
#endif


/*
 * TailQ initialization values.
 */
#define TAILQ_INITIALIZER	{ NULL, NULL }

#define	UMTX_INITIALIZER	{ NULL, NULL }

struct pthread_mutex_attr {
	enum pthread_mutextype	m_type;
	int			m_protocol;
	int			m_ceiling;
	long			m_flags;
};

/*
 * Static mutex initialization values. 
 */

#define PTHREAD_MUTEXATTR_STATIC_INITIALIZER \
	{ PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE, 0, MUTEX_FLAGS_PRIVATE }

#define PTHREAD_MUTEX_STATIC_INITIALIZER   \
	{ PTHREAD_MUTEXATTR_STATIC_INITIALIZER, UMTX_INITIALIZER, NULL,	\
	0, 0, TAILQ_INITIALIZER }

union pthread_mutex_data {
        void    *m_ptr;
        int     m_count;
};

struct pthread_mutex {
        enum pthread_mutextype          m_type;
        int                             m_protocol;
        TAILQ_HEAD(mutex_head, pthread) m_queue;
        struct pthread                  *m_owner;
        union pthread_mutex_data        m_data;
        long                            m_flags;
        int                             m_refcount;

        /*
         * Used for priority inheritence and protection.
         *
         *   m_prio       - For priority inheritence, the highest active
         *                  priority (threads locking the mutex inherit
         *                  this priority).  For priority protection, the
         *                  ceiling priority of this mutex.
         *   m_saved_prio - mutex owners inherited priority before
         *                  taking the mutex, restored when the owner
         *                  unlocks the mutex.
         */
        int                             m_prio;
        int                             m_saved_prio;

        /*
         * Link for list of all mutexes a thread currently owns.
         */
        TAILQ_ENTRY(pthread_mutex)      m_qe;

        /*
         * Lock for accesses to this structure.
         */
        spinlock_t                      lock;
};

/*
 * Flags for mutexes. 
 */
#define MUTEX_FLAGS_PRIVATE	0x01
#define MUTEX_FLAGS_INITED	0x02
#define MUTEX_FLAGS_BUSY	0x04

/* 
 * Condition variable definitions.
 */
enum pthread_cond_type {
	COND_TYPE_FAST,
	COND_TYPE_MAX
};

struct pthread_cond {
	enum pthread_cond_type		c_type;
	TAILQ_HEAD(cond_head, pthread)	c_queue;
	pthread_mutex_t			c_mutex;
	void				*c_data;
	long				c_flags;
	int				c_seqno;

	/*
	 * Lock for accesses to this structure.
	 */
	struct umtx			c_lock;
};

struct pthread_cond_attr {
	enum pthread_cond_type	c_type;
	long			c_flags;
};

/*
 * Flags for condition variables.
 */
#define COND_FLAGS_INITED	0x01

/*
 * Static cond initialization values. 
 */
#define PTHREAD_COND_STATIC_INITIALIZER    \
	{ COND_TYPE_FAST, TAILQ_INITIALIZER, NULL, NULL, \
	0, 0, UMTX_INITIALIZER }

/*
 * Semaphore definitions.
 */
struct sem {
#define	SEM_MAGIC	((u_int32_t) 0x09fa4012)
	u_int32_t	magic;
	pthread_mutex_t	lock;
	pthread_cond_t	gtzero;
	u_int32_t	count;
	u_int32_t	nwaiters;
};

/*
 * Cleanup definitions.
 */
struct pthread_cleanup {
	struct pthread_cleanup	*next;
	void			(*routine) ();
	void			*routine_arg;
};

struct pthread_attr {
	int	sched_policy;
	int	sched_inherit;
	int	sched_interval;
	int	prio;
	int	suspend;
	int	flags;
	void	*arg_attr;
	void	(*cleanup_attr) ();
	void	*stackaddr_attr;
	size_t	stacksize_attr;
	size_t	guardsize_attr;
};

/*
 * Thread creation state attributes.
 */
#define PTHREAD_CREATE_RUNNING			0
#define PTHREAD_CREATE_SUSPENDED		1

/*
 * Miscellaneous definitions.
 */
#define PTHREAD_STACK_DEFAULT			65536
/*
 * Size of default red zone at the end of each stack.  In actuality, this "red
 * zone" is merely an unmapped region, except in the case of the initial stack.
 * Since mmap() makes it possible to specify the maximum growth of a MAP_STACK
 * region, an unmapped gap between thread stacks achieves the same effect as
 * explicitly mapped red zones.
 * This is declared and initialized in uthread_init.c.
 */
extern int _pthread_guard_default;

extern int _pthread_page_size;

/*
 * Maximum size of initial thread's stack.  This perhaps deserves to be larger
 * than the stacks of other threads, since many applications are likely to run
 * almost entirely on this stack.
 */
#define PTHREAD_STACK_INITIAL			0x100000

/*
 * Define the different priority ranges.  All applications have thread
 * priorities constrained within 0-31.  The threads library raises the
 * priority when delivering signals in order to ensure that signal
 * delivery happens (from the POSIX spec) "as soon as possible".
 * In the future, the threads library will also be able to map specific
 * threads into real-time (cooperating) processes or kernel threads.
 * The RT and SIGNAL priorities will be used internally and added to
 * thread base priorities so that the scheduling queue can handle both
 * normal and RT priority threads with and without signal handling.
 *
 * The approach taken is that, within each class, signal delivery
 * always has priority over thread execution.
 */
#define PTHREAD_DEFAULT_PRIORITY		15
#define PTHREAD_MIN_PRIORITY			0
#define PTHREAD_MAX_PRIORITY			31	/* 0x1F */
#define PTHREAD_SIGNAL_PRIORITY			32	/* 0x20 */
#define PTHREAD_RT_PRIORITY			64	/* 0x40 */
#define PTHREAD_FIRST_PRIORITY			PTHREAD_MIN_PRIORITY
#define PTHREAD_LAST_PRIORITY	\
	(PTHREAD_MAX_PRIORITY + PTHREAD_SIGNAL_PRIORITY + PTHREAD_RT_PRIORITY)
#define PTHREAD_BASE_PRIORITY(prio)	((prio) & PTHREAD_MAX_PRIORITY)

/*
 * Clock resolution in microseconds.
 */
#define CLOCK_RES_USEC				10000
#define CLOCK_RES_USEC_MIN			1000

/*
 * Time slice period in microseconds.
 */
#define TIMESLICE_USEC				20000

/*
 * XXX Define a thread-safe macro to get the current time of day
 * which is updated at regular intervals by the scheduling signal
 * handler.
 */
#define	GET_CURRENT_TOD(tv)	gettimeofday(&(tv), NULL)


struct pthread_rwlockattr {
	int		pshared;
};

struct pthread_rwlock {
	pthread_mutex_t	lock;	/* monitor lock */
	int		state;	/* 0 = idle  >0 = # of readers  -1 = writer */
	pthread_cond_t	read_signal;
	pthread_cond_t	write_signal;
	int		blocked_writers;
};

/*
 * Thread states.
 */
enum pthread_state {
	PS_RUNNING,
	PS_MUTEX_WAIT,
	PS_COND_WAIT,
	PS_SLEEP_WAIT,	/* XXX We need to wrap syscalls to set this state */
	PS_WAIT_WAIT,
	PS_JOIN,
	PS_DEAD,
	PS_DEADLOCK,
	PS_STATE_MAX
};


/*
 * File descriptor locking definitions.
 */
#define FD_READ             0x1
#define FD_WRITE            0x2
#define FD_RDWR             (FD_READ | FD_WRITE)

union pthread_wait_data {
	pthread_mutex_t	mutex;
	pthread_cond_t	cond;
	spinlock_t	*spinlock;
	struct pthread	*thread;
};

struct join_status {
	struct pthread	*thread;
	void		*ret;
	int		error;
};

struct pthread_state_data {
	union pthread_wait_data psd_wait_data;
	enum pthread_state	psd_state;
	int			psd_flags;
};

struct pthread_specific_elem {
	const void	*data;
	int		seqno;
};

/*
 * Thread structure.
 */
struct pthread {
	/*
	 * Magic value to help recognize a valid thread structure
	 * from an invalid one:
	 */
#define	PTHREAD_MAGIC		((u_int32_t) 0xd09ba115)
	u_int32_t		magic;
	char			*name;
	u_int64_t		uniqueid; /* for gdb */
	thr_id_t		thr_id;

	/*
	 * Lock for accesses to this thread structure.
	 */
	spinlock_t		lock;

	/* Queue entry for list of all threads: */
	TAILQ_ENTRY(pthread)	tle;

	/* Queue entry for list of dead threads: */
	TAILQ_ENTRY(pthread)	dle;

	/*
	 * Thread start routine, argument, stack pointer and thread
	 * attributes.
	 */
	void			*(*start_routine)(void *);
	void			*arg;
	void			*stack;
	struct pthread_attr	attr;

	/*
	 * Machine context, including signal state.
	 */
	ucontext_t		ctx;

	/*
	 * Cancelability flags - the lower 2 bits are used by cancel
	 * definitions in pthread.h
	 */
#define PTHREAD_AT_CANCEL_POINT		0x0004
#define PTHREAD_CANCELLING		0x0008

	/*
	 * Protected by Giant.
	 */ 
	int	cancelflags;

	/* Thread state: */
	enum pthread_state	state;

	/*
	 * Error variable used instead of errno. The function __error()
	 * returns a pointer to this. 
	 */
	int	error;

	/*
	 * The joiner is the thread that is joining to this thread.  The
	 * join status keeps track of a join operation to another thread.
	 */
	struct pthread		*joiner;
	struct join_status	join_status;

	/*
	 * A thread can belong to:
	 *
	 *   o A queue of threads waiting for a mutex
	 *   o A queue of threads waiting for a condition variable
	 *
	 * A thread can also be joining a thread (the joiner field above).
	 *
	 * It must not be possible for a thread to belong to any of the
	 * above queues while it is handling a signal.  Signal handlers
	 * may longjmp back to previous stack frames circumventing normal
	 * control flow.  This could corrupt queue integrity if the thread
	 * retains membership in the queue.  Therefore, if a thread is a
	 * member of one of these queues when a signal handler is invoked,
	 * it must remove itself from the queue before calling the signal
	 * handler and reinsert itself after normal return of the handler.
	 *
	 * Use sqe for synchronization (mutex and condition variable) queue
	 * links.
	 */
	TAILQ_ENTRY(pthread)	sqe;	/* synchronization queue link */

	/* Wait data. */
	union pthread_wait_data data;

	/* Miscellaneous flags; only set with signals deferred. */
	int		flags;
#define PTHREAD_FLAGS_PRIVATE	0x0001
#define PTHREAD_EXITING		0x0002
#define PTHREAD_FLAGS_IN_CONDQ	0x0080	/* in condition queue using sqe link*/
#define PTHREAD_FLAGS_IN_MUTEXQ	0x0100	/* in mutex queue using sqe link */
#define	PTHREAD_FLAGS_SUSPENDED	0x0200	/* thread is suspended */
#define PTHREAD_FLAGS_TRACE	0x0400	/* for debugging purposes */
#define PTHREAD_FLAGS_IN_SYNCQ	\
    (PTHREAD_FLAGS_IN_CONDQ | PTHREAD_FLAGS_IN_MUTEXQ)

	/*
	 * Base priority is the user setable and retrievable priority
	 * of the thread.  It is only affected by explicit calls to
	 * set thread priority and upon thread creation via a thread
	 * attribute or default priority.
	 */
	char		base_priority;

	/*
	 * Inherited priority is the priority a thread inherits by
	 * taking a priority inheritence or protection mutex.  It
	 * is not affected by base priority changes.  Inherited
	 * priority defaults to and remains 0 until a mutex is taken
	 * that is being waited on by any other thread whose priority
	 * is non-zero.
	 */
	char		inherited_priority;

	/*
	 * Active priority is always the maximum of the threads base
	 * priority and inherited priority.  When there is a change
	 * in either the base or inherited priority, the active
	 * priority must be recalculated.
	 */
	char		active_priority;

	/* Number of priority ceiling or protection mutexes owned. */
	int		priority_mutex_count;

	/*
	 * Queue of currently owned mutexes.
	 */
	TAILQ_HEAD(, pthread_mutex)	mutexq;

	void				*ret;
	struct pthread_specific_elem	*specific;
	int				specific_data_count;

	/*
	 * Architecture specific id field used for _{get, set}_curthread()
	 * interface.
	 */
	void			*arch_id;

	/* Cleanup handlers Link List */
	struct pthread_cleanup *cleanup;
	char			*fname;	/* Ptr to source file name  */
	int			lineno;	/* Source line number.      */
};

/*
 * Global variables for the uthread kernel.
 */

SCLASS void *_usrstack
#ifdef GLOBAL_PTHREAD_PRIVATE
= (void *) USRSTACK;
#else
;
#endif

/* List of all threads: */
SCLASS TAILQ_HEAD(, pthread)	_thread_list
#ifdef GLOBAL_PTHREAD_PRIVATE
= TAILQ_HEAD_INITIALIZER(_thread_list);
#else
;
#endif

/* Dead threads: */
SCLASS TAILQ_HEAD(, pthread) _dead_list
#ifdef GLOBAL_PTHREAD_PRIVATE
= TAILQ_HEAD_INITIALIZER(_dead_list);
#else
;
#endif

/* Initial thread: */
SCLASS struct pthread *_thread_initial
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL;
#else
;
#endif

/* Default thread attributes: */
SCLASS struct pthread_attr pthread_attr_default
#ifdef GLOBAL_PTHREAD_PRIVATE
= { SCHED_RR, 0, TIMESLICE_USEC, PTHREAD_DEFAULT_PRIORITY,
	PTHREAD_CREATE_RUNNING, PTHREAD_CREATE_JOINABLE, NULL, NULL, NULL,
	PTHREAD_STACK_DEFAULT, -1 };
#else
;
#endif

/* Default mutex attributes: */
SCLASS struct pthread_mutex_attr pthread_mutexattr_default
#ifdef GLOBAL_PTHREAD_PRIVATE
= { PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE, 0, 0 };
#else
;
#endif

/* Default condition variable attributes: */
SCLASS struct pthread_cond_attr pthread_condattr_default
#ifdef GLOBAL_PTHREAD_PRIVATE
= { COND_TYPE_FAST, 0 };
#else
;
#endif

SCLASS int    _clock_res_usec		/* Clock resolution in usec.	*/
#ifdef GLOBAL_PTHREAD_PRIVATE
= CLOCK_RES_USEC;
#else
;
#endif

/* Giant lock. */
SCLASS struct umtx _giant_mutex
#ifdef GLOBAL_PTHREAD_PRIVATE
=  UMTX_INITIALIZER
#endif
;

SCLASS int _giant_count;

/* Garbage collector mutex and condition variable. */
SCLASS	pthread_mutex_t _gc_mutex
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL
#endif
;
SCLASS	pthread_cond_t  _gc_cond
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL
#endif
;

/*
 * Array of signal actions for this process.
 */
SCLASS struct  sigaction _thread_sigact[NSIG];

/* Tracks the number of threads blocked while waiting for a spinlock. */
SCLASS	volatile int	_spinblock_count
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0
#endif
;

/* 
 * And, should we climb the beanstalk,
 * We'll meet his brother, Giant.
 */
void GIANT_LOCK(pthread_t);
void GIANT_UNLOCK(pthread_t);

/* Undefine the storage class specifier: */
#undef  SCLASS

/*
 * Function prototype definitions.
 */
__BEGIN_DECLS
char    *__ttyname_basic(int);
char    *__ttyname_r_basic(int, char *, size_t);
char    *ttyname_r(int, char *, size_t);
void	_cond_wait_backout(pthread_t);
int     _find_thread(pthread_t);
pthread_t _get_curthread(void);
void	*_set_curthread(ucontext_t *, struct pthread *);
void	_retire_thread(void *arch_id);
void	*_thread_stack_alloc(size_t, size_t);
void	_thread_stack_free(void *, size_t, size_t);
int     _thread_create(pthread_t *,const pthread_attr_t *,void *(*start_routine)(void *),void *,pthread_t);
int	_mutex_cv_lock(pthread_mutex_t *);
int	_mutex_cv_unlock(pthread_mutex_t *);
void	_mutex_lock_backout(pthread_t);
void	_mutex_notify_priochange(pthread_t);
int	_mutex_reinit(pthread_mutex_t *);
void	_mutex_unlock_private(pthread_t);
int	_cond_reinit(pthread_cond_t *);
void	*_pthread_getspecific(pthread_key_t);
int	_pthread_key_create(pthread_key_t *, void (*) (void *));
int	_pthread_key_delete(pthread_key_t);
int	_pthread_mutex_destroy(pthread_mutex_t *);
int	_pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int	_pthread_mutex_lock(pthread_mutex_t *);
int	_pthread_mutex_trylock(pthread_mutex_t *);
int	_pthread_mutex_unlock(pthread_mutex_t *);
int	_pthread_mutexattr_init(pthread_mutexattr_t *);
int	_pthread_mutexattr_destroy(pthread_mutexattr_t *);
int	_pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int	_pthread_once(pthread_once_t *, void (*) (void));
pthread_t _pthread_self(void);
int	_pthread_setspecific(pthread_key_t, const void *);
void    _thread_exit(char *, int, char *);
void    _thread_exit_cleanup(void);
void    *_thread_cleanup(pthread_t);
void    _thread_cleanupspecific(void);
void    _thread_dump_info(void);
void    _thread_init(void);
void	_thread_sig_wrapper(int sig, siginfo_t *info, ucontext_t *context);
void	_thread_printf(int fd, const char *, ...);
void    _thread_start(void);
void	_thread_seterrno(pthread_t, int);
pthread_addr_t _thread_gc(pthread_addr_t);
void	_thread_enter_cancellation_point(void);
void	_thread_leave_cancellation_point(void);
void	_thread_cancellation_point(void);
int	_thread_suspend(pthread_t thread, struct timespec *abstime);
void	_thread_critical_enter(pthread_t);
void	_thread_critical_exit(pthread_t);

/* #include <sys/aio.h> */
#ifdef _SYS_AIO_H_
int	__sys_aio_suspend(const struct aiocb * const[], int, const struct timespec *);
#endif

/* #include <sys/event.h> */
#ifdef _SYS_EVENT_H_
int	__sys_kevent(int, const struct kevent *, int, struct kevent *,
	    int, const struct timespec *);
#endif

/* #include <sys/ioctl.h> */
#ifdef _SYS_IOCTL_H_
int	__sys_ioctl(int, unsigned long, ...);
#endif

/* #include <sys/mman.h> */
#ifdef _SYS_MMAN_H_
int	__sys_msync(void *, size_t, int);
#endif

/* #include <sys/mount.h> */
#ifdef _SYS_MOUNT_H_
int	__sys_fstatfs(int, struct statfs *);
#endif

/* #include <sys/socket.h> */
#ifdef _SYS_SOCKET_H_
int	__sys_accept(int, struct sockaddr *, socklen_t *);
int	__sys_bind(int, const struct sockaddr *, socklen_t);
int	__sys_connect(int, const struct sockaddr *, socklen_t);
int	__sys_getpeername(int, struct sockaddr *, socklen_t *);
int	__sys_getsockname(int, struct sockaddr *, socklen_t *);
int	__sys_getsockopt(int, int, int, void *, socklen_t *);
int	__sys_listen(int, int);
ssize_t	__sys_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t	__sys_recvmsg(int, struct msghdr *, int);
int	__sys_sendfile(int, int, off_t, size_t, struct sf_hdtr *, off_t *, int);
ssize_t	__sys_sendmsg(int, const struct msghdr *, int);
ssize_t	__sys_sendto(int, const void *,size_t, int, const struct sockaddr *, socklen_t);
int	__sys_setsockopt(int, int, int, const void *, socklen_t);
int	__sys_shutdown(int, int);
int	__sys_socket(int, int, int);
int	__sys_socketpair(int, int, int, int *);
#endif

/* #include <sys/stat.h> */
#ifdef _SYS_STAT_H_
int	__sys_fchflags(int, u_long);
int	__sys_fchmod(int, mode_t);
int	__sys_fstat(int, struct stat *);
#endif

/* #include <sys/uio.h> */
#ifdef _SYS_UIO_H_
ssize_t	__sys_readv(int, const struct iovec *, int);
ssize_t	__sys_writev(int, const struct iovec *, int);
#endif

/* #include <sys/wait.h> */
#ifdef WNOHANG
pid_t	__sys_wait4(pid_t, int *, int, struct rusage *);
#endif

/* #include <dirent.h> */
#ifdef _DIRENT_H_
int	__sys_getdirentries(int, char *, int, long *);
#endif

/* #include <fcntl.h> */
#ifdef _SYS_FCNTL_H_
int	__sys_fcntl(int, int, ...);
int	__sys_flock(int, int);
int	__sys_open(const char *, int, ...);
#endif

/* #include <poll.h> */
#ifdef _SYS_POLL_H_
int	__sys_poll(struct pollfd *, unsigned, int);
#endif

/* #include <signal.h> */
#ifdef _SIGNAL_H_
int	__sys_sigaction(int, const struct sigaction *, struct sigaction *);
int	__sys_sigaltstack(const struct sigaltstack *, struct sigaltstack *);
int	__sys_sigprocmask(int, const sigset_t *, sigset_t *);
int	__sys_sigreturn(ucontext_t *);
#endif

/* #include <unistd.h> */
#ifdef _UNISTD_H_
int	__sys_close(int);
int	__sys_dup(int);
int	__sys_dup2(int, int);
int	__sys_execve(const char *, char * const *, char * const *);
void	__sys_exit(int);
int	__sys_fchown(int, uid_t, gid_t);
pid_t	__sys_fork(void);
long	__sys_fpathconf(int, int);
int	__sys_fsync(int);
int	__sys_pipe(int *);
ssize_t	__sys_read(int, void *, size_t);
ssize_t	__sys_write(int, const void *, size_t);
#endif

__END_DECLS

#endif  /* !_PTHREAD_PRIVATE_H */
