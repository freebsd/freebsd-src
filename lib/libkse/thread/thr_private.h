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
 * Include files.
 */
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/cdefs.h>
#include <sys/kse.h>
#include <sched.h>
#include <ucontext.h>
#include <unistd.h>
#include <pthread.h>
#include <pthread_np.h>

#include "ksd.h"
#include "lock.h"
#include "pthread_md.h"

/*
 * Evaluate the storage class specifier.
 */
#ifdef GLOBAL_PTHREAD_PRIVATE
#define SCLASS
#define SCLASS_PRESET(x...)	= x
#else
#define SCLASS			extern
#define SCLASS_PRESET(x...)
#endif

/*
 * Kernel fatal error handler macro.
 */
#define PANIC(string)   _thr_exit(__FILE__,__LINE__,string)


/* Output debug messages like this: */
#define stdout_debug(args...)	_thread_printf(STDOUT_FILENO, ##args)
#define stderr_debug(args...)	_thread_printf(STDOUT_FILENO, ##args)

#define	DBG_MUTEX	0x0001
#define	DBG_SIG		0x0002


#define THR_ASSERT(cond, msg) do {	\
	if (!(cond))			\
		PANIC(msg);		\
} while (0)


/*
 * State change macro without scheduling queue change:
 */
#define THR_SET_STATE(thrd, newstate) do {				\
	(thrd)->state = newstate;					\
	(thrd)->fname = __FILE__;					\
	(thrd)->lineno = __LINE__;					\
} while (0)


/*
 * Define the signals to be used for scheduling.
 */
#define _ITIMER_SCHED_TIMER	ITIMER_PROF
#define _SCHED_SIGNAL		SIGPROF

#define	TIMESPEC_ADD(dst, src, val)				\
	do { 							\
		(dst)->tv_sec = (src)->tv_sec + (val)->tv_sec;	\
		(dst)->tv_nsec = (src)->tv_nsec + (val)->tv_nsec; \
		if ((dst)->tv_nsec > 1000000000) {		\
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

/*
 * Priority queues.
 *
 * XXX It'd be nice if these were contained in uthread_priority_queue.[ch].
 */
typedef struct pq_list {
	TAILQ_HEAD(, pthread)	pl_head; /* list of threads at this priority */
	TAILQ_ENTRY(pq_list)	pl_link; /* link for queue of priority lists */
	int			pl_prio; /* the priority of this list */
	int			pl_queued; /* is this in the priority queue */
} pq_list_t;

typedef struct pq_queue {
	TAILQ_HEAD(, pq_list)	 pq_queue; /* queue of priority lists */
	pq_list_t		*pq_lists; /* array of all priority lists */
	int			 pq_size;  /* number of priority lists */
#define	PQF_ACTIVE	0x0001
	int			 pq_flags;
	int			 pq_threads;
} pq_queue_t;

/*
 * Each KSEG has a scheduling queue.  For now, threads that exist in their
 * own KSEG (system scope) will get a full priority queue.  In the future
 * this can be optimized for the single thread per KSEG case.
 */
struct sched_queue {
	pq_queue_t		sq_runq;
	TAILQ_HEAD(, pthread)	sq_waitq;	/* waiting in userland */
};

/* Used to maintain pending and active signals: */
struct sigstatus {
	siginfo_t	*info;		/* arg 2 to signal handler */
	int		pending;	/* Is this a pending signal? */
	int		blocked;	/*
					 * This signal has occured and hasn't
					 * yet been handled; ignore subsequent
					 * signals until the handler is done.
					 */
	int		signo;
};

typedef struct kse_thr_mailbox *kse_critical_t;

struct kse_group;

#define	MAX_KSE_LOCKLEVEL	3
struct kse {
	struct kse_mailbox	k_mbx;		/* kernel kse mailbox */
	/* -- location and order specific items for gdb -- */
	struct pthread		*k_curthread;	/* current thread */
	struct kse_group	*k_kseg;	/* parent KSEG */
	struct sched_queue	*k_schedq;	/* scheduling queue */
	/* -- end of location and order specific items -- */
	TAILQ_ENTRY(kse)	k_qe;		/* KSE list link entry */
	TAILQ_ENTRY(kse)	k_kgqe;		/* KSEG's KSE list entry */
	struct ksd		k_ksd;		/* KSE specific data */
	/*
	 * Items that are only modified by the kse, or that otherwise
	 * don't need to be locked when accessed
	 */
	struct lock		k_lock;
	struct lockuser		k_lockusers[MAX_KSE_LOCKLEVEL];
	int			k_locklevel;
	sigset_t		k_sigmask;
	struct sigstatus	k_sigq[NSIG];
	stack_t			k_stack;
	int			k_check_sigq;
	int			k_flags;
#define	KF_STARTED			0x0001	/* kernel kse created */
#define	KF_INITIALIZED			0x0002	/* initialized on 1st upcall */
	int			k_waiting;
	int			k_idle;		/* kse is idle */
	int			k_error;	/* syscall errno in critical */
	int			k_cpu;		/* CPU ID when bound */
	int			k_done;		/* this KSE is done */
};

/*
 * Each KSE group contains one or more KSEs in which threads can run.
 * At least for now, there is one scheduling queue per KSE group; KSEs
 * within the same KSE group compete for threads from the same scheduling
 * queue.  A scope system thread has one KSE in one KSE group; the group
 * does not use its scheduling queue.
 */
struct kse_group {
	TAILQ_HEAD(, kse)	kg_kseq;	/* list of KSEs in group */
	TAILQ_HEAD(, pthread)	kg_threadq;	/* list of threads in group */
	TAILQ_ENTRY(kse_group)  kg_qe;		/* link entry */
	struct sched_queue	kg_schedq;	/* scheduling queue */
	struct lock		kg_lock;
	int			kg_threadcount;	/* # of assigned threads */
	int			kg_ksecount;	/* # of assigned KSEs */
	int			kg_idle_kses;
	int			kg_flags;
#define	KGF_SINGLE_THREAD		0x0001	/* scope system kse group */
#define	KGF_SCHEDQ_INITED		0x0002	/* has an initialized schedq */
};

/*
 * Add/remove threads from a KSE's scheduling queue.
 * For now the scheduling queue is hung off the KSEG.
 */
#define	KSEG_THRQ_ADD(kseg, thr)			\
do {							\
	TAILQ_INSERT_TAIL(&(kseg)->kg_threadq, thr, kle);\
	(kseg)->kg_threadcount++;			\
} while (0)

#define	KSEG_THRQ_REMOVE(kseg, thr)			\
do {							\
	TAILQ_REMOVE(&(kseg)->kg_threadq, thr, kle);	\
	(kseg)->kg_threadcount--;			\
} while (0)


/*
 * Lock acquire and release for KSEs.
 */
#define	KSE_LOCK_ACQUIRE(kse, lck)					\
do {									\
	if ((kse)->k_locklevel >= MAX_KSE_LOCKLEVEL)			\
		PANIC("Exceeded maximum lock level");			\
	else {								\
		(kse)->k_locklevel++;					\
		_lock_acquire((lck),					\
		    &(kse)->k_lockusers[(kse)->k_locklevel - 1], 0);	\
	}								\
} while (0)

#define	KSE_LOCK_RELEASE(kse, lck)					\
do {									\
	if ((kse)->k_locklevel > 0) {					\
		_lock_release((lck),					\
		    &(kse)->k_lockusers[(kse)->k_locklevel - 1]);	\
		(kse)->k_locklevel--;					\
	}								\
} while (0)

/*
 * Lock our own KSEG.
 */
#define	KSE_LOCK(curkse)		\
	KSE_LOCK_ACQUIRE(curkse, &(curkse)->k_kseg->kg_lock)
#define	KSE_UNLOCK(curkse)		\
	KSE_LOCK_RELEASE(curkse, &(curkse)->k_kseg->kg_lock)

/*
 * Lock a potentially different KSEG.
 */
#define	KSE_SCHED_LOCK(curkse, kseg)	\
	KSE_LOCK_ACQUIRE(curkse, &(kseg)->kg_lock)
#define	KSE_SCHED_UNLOCK(curkse, kseg)	\
	KSE_LOCK_RELEASE(curkse, &(kseg)->kg_lock)

/*
 * Waiting queue manipulation macros (using pqe link):
 */
#define KSE_WAITQ_REMOVE(kse, thrd) \
do { \
	if (((thrd)->flags & THR_FLAGS_IN_WAITQ) != 0) { \
		TAILQ_REMOVE(&(kse)->k_schedq->sq_waitq, thrd, pqe); \
		(thrd)->flags &= ~THR_FLAGS_IN_WAITQ; \
	} \
} while (0)
#define KSE_WAITQ_INSERT(kse, thrd)	kse_waitq_insert(thrd)
#define	KSE_WAITQ_FIRST(kse)		TAILQ_FIRST(&(kse)->k_schedq->sq_waitq)

#define	KSE_SET_WAIT(kse) 	atomic_store_rel_int(&(kse)->k_waiting, 1)

#define	KSE_CLEAR_WAIT(kse) 	atomic_store_rel_int(&(kse)->k_waiting, 0)

#define	KSE_WAITING(kse)	(kse)->k_waiting != 0
#define	KSE_WAKEUP(kse)		kse_wakeup(&(kse)->k_mbx)

#define	KSE_SET_IDLE(kse)	((kse)->k_idle = 1)
#define	KSE_CLEAR_IDLE(kse)	((kse)->k_idle = 0)
#define	KSE_IS_IDLE(kse)	((kse)->k_idle != 0)

/*
 * TailQ initialization values.
 */
#define TAILQ_INITIALIZER	{ NULL, NULL }

/*
 * lock initialization values.
 */
#define	LCK_INITIALIZER		{ NULL, NULL, LCK_DEFAULT }

struct pthread_mutex {
	/*
	 * Lock for accesses to this structure.
	 */
	struct lock			m_lock;
	enum pthread_mutextype		m_type;
	int				m_protocol;
	TAILQ_HEAD(mutex_head, pthread)	m_queue;
	struct pthread			*m_owner;
	long				m_flags;
	int				m_count;
	int				m_refcount;

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
	int				m_prio;
	int				m_saved_prio;

	/*
	 * Link for list of all mutexes a thread currently owns.
	 */
	TAILQ_ENTRY(pthread_mutex)	m_qe;
};

/*
 * Flags for mutexes. 
 */
#define MUTEX_FLAGS_PRIVATE	0x01
#define MUTEX_FLAGS_INITED	0x02
#define MUTEX_FLAGS_BUSY	0x04

/*
 * Static mutex initialization values. 
 */
#define PTHREAD_MUTEX_STATIC_INITIALIZER				\
	{ LCK_INITIALIZER, PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE,	\
	TAILQ_INITIALIZER, NULL, MUTEX_FLAGS_PRIVATE, 0, 0, 0, 0,	\
	TAILQ_INITIALIZER }

struct pthread_mutex_attr {
	enum pthread_mutextype	m_type;
	int			m_protocol;
	int			m_ceiling;
	long			m_flags;
};

#define PTHREAD_MUTEXATTR_STATIC_INITIALIZER \
	{ PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE, 0, MUTEX_FLAGS_PRIVATE }

/* 
 * Condition variable definitions.
 */
enum pthread_cond_type {
	COND_TYPE_FAST,
	COND_TYPE_MAX
};

struct pthread_cond {
	/*
	 * Lock for accesses to this structure.
	 */
	struct lock			c_lock;
	enum pthread_cond_type		c_type;
	TAILQ_HEAD(cond_head, pthread)	c_queue;
	struct pthread_mutex		*c_mutex;
	long				c_flags;
	long				c_seqno;
};

struct pthread_cond_attr {
	enum pthread_cond_type	c_type;
	long			c_flags;
};

/*
 * Flags for condition variables.
 */
#define COND_FLAGS_PRIVATE	0x01
#define COND_FLAGS_INITED	0x02
#define COND_FLAGS_BUSY		0x04

/*
 * Static cond initialization values. 
 */
#define PTHREAD_COND_STATIC_INITIALIZER				\
	{ LCK_INITIALIZER, COND_TYPE_FAST, TAILQ_INITIALIZER,	\
	NULL, NULL, 0, 0 }

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
#define	THR_STACK_USER		0x100	/* 0xFF reserved for <pthread.h> */
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
#define THR_CREATE_RUNNING		0
#define THR_CREATE_SUSPENDED		1

/*
 * Miscellaneous definitions.
 */
#define THR_STACK_DEFAULT			65536

/*
 * Maximum size of initial thread's stack.  This perhaps deserves to be larger
 * than the stacks of other threads, since many applications are likely to run
 * almost entirely on this stack.
 */
#define THR_STACK_INITIAL			0x100000

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
#define THR_DEFAULT_PRIORITY			15
#define THR_MIN_PRIORITY			0
#define THR_MAX_PRIORITY			31	/* 0x1F */
#define THR_SIGNAL_PRIORITY			32	/* 0x20 */
#define THR_RT_PRIORITY				64	/* 0x40 */
#define THR_FIRST_PRIORITY			THR_MIN_PRIORITY
#define THR_LAST_PRIORITY	\
	(THR_MAX_PRIORITY + THR_SIGNAL_PRIORITY + THR_RT_PRIORITY)
#define THR_BASE_PRIORITY(prio)	((prio) & THR_MAX_PRIORITY)

/*
 * Clock resolution in microseconds.
 */
#define CLOCK_RES_USEC				10000

/*
 * Time slice period in microseconds.
 */
#define TIMESLICE_USEC				20000

/*
 * XXX - Define a thread-safe macro to get the current time of day
 *       which is updated at regular intervals by something.
 *
 * For now, we just make the system call to get the time.
 */
#define	KSE_GET_TOD(curkse, tsp) \
do {							\
	*tsp = (curkse)->k_mbx.km_timeofday;		\
	if ((tsp)->tv_sec == 0)				\
		clock_gettime(CLOCK_REALTIME, tsp);	\
} while (0)

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
	PS_LOCKWAIT,
	PS_MUTEX_WAIT,
	PS_COND_WAIT,
	PS_SLEEP_WAIT,
	PS_SIGSUSPEND,
	PS_SIGWAIT,
	PS_JOIN,
	PS_SUSPENDED,
	PS_DEAD,
	PS_DEADLOCK,
	PS_STATE_MAX
};


union pthread_wait_data {
	pthread_mutex_t	mutex;
	pthread_cond_t	cond;
	const sigset_t	*sigwait;	/* Waiting on a signal in sigwait */
	struct lock	*lock;
};

/*
 * Define a continuation routine that can be used to perform a
 * transfer of control:
 */
typedef void	(*thread_continuation_t) (void *);

/*
 * This stores a thread's state prior to running a signal handler.
 * It is used when a signal is delivered to a thread blocked in
 * userland.  If the signal handler returns normally, the thread's
 * state is restored from here.
 */
struct pthread_sigframe {
	int			psf_flags;
	int			psf_interrupted;
	int			psf_signo;
	enum pthread_state	psf_state;
	union pthread_wait_data psf_wait_data;
	struct timespec		psf_wakeup_time;
	sigset_t		psf_sigset;
	sigset_t		psf_sigmask;
	int			psf_seqno;
};

struct join_status {
	struct pthread	*thread;
	void		*ret;
	int		error;
};

struct pthread_specific_elem {
	const void	*data;
	int		seqno;
};


#define	MAX_THR_LOCKLEVEL	3
/*
 * Thread structure.
 */
struct pthread {
	/*
	 * Thread mailbox is first so it cal be aligned properly.
	 */
	struct kse_thr_mailbox	tmbx;
	void			*alloc_addr;	/* real address (unaligned) */

	/*
	 * Magic value to help recognize a valid thread structure
	 * from an invalid one:
	 */
#define	THR_MAGIC		((u_int32_t) 0xd09ba115)
	u_int32_t		magic;
	char			*name;
	u_int64_t		uniqueid; /* for gdb */

	/* Queue entry for list of all threads: */
	TAILQ_ENTRY(pthread)	tle;	/* link for all threads in process */
	TAILQ_ENTRY(pthread)	kle;	/* link for all threads in KSE/KSEG */

	/* Queue entry for GC lists: */
	TAILQ_ENTRY(pthread)	gcle;

	/*
	 * Lock for accesses to this thread structure.
	 */
	struct lock		lock;
	struct lockuser		lockusers[MAX_THR_LOCKLEVEL];
	int			locklevel;
	kse_critical_t		critical[MAX_KSE_LOCKLEVEL];
	struct kse		*kse;
	struct kse_group	*kseg;

	/*
	 * Thread start routine, argument, stack pointer and thread
	 * attributes.
	 */
	void			*(*start_routine)(void *);
	void			*arg;
	struct pthread_attr	attr;

	int			active;		/* thread running */
	int			blocked;	/* thread blocked in kernel */
	int			need_switchout;
	int			need_wakeup;

	/*
	 * Used for tracking delivery of signal handlers.
	 */
	struct pthread_sigframe	*curframe;
	siginfo_t		siginfo[NSIG];

 	/*
	 * Cancelability flags - the lower 2 bits are used by cancel
	 * definitions in pthread.h
	 */
#define THR_AT_CANCEL_POINT		0x0004
#define THR_CANCELLING			0x0008
#define THR_CANCEL_NEEDED		0x0010
	int			cancelflags;

	thread_continuation_t	continuation;

	/*
	 * The thread's base and pending signal masks.  The active
	 * signal mask is stored in the thread's context (in mailbox).
	 */
	sigset_t		sigmask;
	sigset_t		sigpend;
	int			sigmask_seqno;
	int			check_pending;
	int			have_signals;
	int			refcount;

	/* Thread state: */
	enum pthread_state	state;
	volatile int		lock_switch;

	/*
	 * Number of microseconds accumulated by this thread when
	 * time slicing is active.
	 */
	long			slice_usec;

	/*
	 * Time to wake up thread. This is used for sleeping threads and
	 * for any operation which may time out (such as select).
	 */
	struct timespec		wakeup_time;

	/* TRUE if operation has timed out. */
	int			timeout;

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
	struct join_status	join_status;

	/*
	 * The current thread can belong to only one scheduling queue at
	 * a time (ready or waiting queue).  It can also belong to:
	 *
	 *   o A queue of threads waiting for a mutex
	 *   o A queue of threads waiting for a condition variable
	 *
	 * It is possible for a thread to belong to more than one of the
	 * above queues if it is handling a signal.  A thread may only
	 * enter a mutex or condition variable queue when it is not
	 * being called from a signal handler.  If a thread is a member
	 * of one of these queues when a signal handler is invoked, it
	 * must be removed from the queue before invoking the handler
	 * and then added back to the queue after return from the handler.
	 *
	 * Use pqe for the scheduling queue link (both ready and waiting),
	 * sqe for synchronization (mutex, condition variable, and join)
	 * queue links, and qe for all other links.
	 */
	TAILQ_ENTRY(pthread)	pqe;	/* priority, wait queues link */
	TAILQ_ENTRY(pthread)	sqe;	/* synchronization queue link */

	/* Wait data. */
	union pthread_wait_data data;

	/*
	 * Set to TRUE if a blocking operation was
	 * interrupted by a signal:
	 */
	int			interrupted;

	/* Signal number when in state PS_SIGWAIT: */
	int			signo;

	/*
	 * Set to non-zero when this thread has entered a critical
	 * region.  We allow for recursive entries into critical regions.
	 */
	int			critical_count;

	/*
	 * Set to TRUE if this thread should yield after leaving a
	 * critical region to check for signals, messages, etc.
	 */
	int			critical_yield;

	int			sflags;
#define THR_FLAGS_IN_SYNCQ	0x0001

	/* Miscellaneous flags; only set with scheduling lock held. */
	int			flags;
#define THR_FLAGS_PRIVATE	0x0001
#define THR_FLAGS_IN_WAITQ	0x0002	/* in waiting queue using pqe link */
#define THR_FLAGS_IN_RUNQ	0x0004	/* in run queue using pqe link */
#define	THR_FLAGS_EXITING	0x0008	/* thread is exiting */
#define	THR_FLAGS_SUSPENDED	0x0010	/* thread is suspended */
#define	THR_FLAGS_GC_SAFE	0x0020	/* thread safe for cleaning */
#define	THR_FLAGS_IN_TDLIST	0x0040	/* thread in all thread list */
#define	THR_FLAGS_IN_GCLIST	0x0080	/* thread in gc list */
	/*
	 * Base priority is the user setable and retrievable priority
	 * of the thread.  It is only affected by explicit calls to
	 * set thread priority and upon thread creation via a thread
	 * attribute or default priority.
	 */
	char			base_priority;

	/*
	 * Inherited priority is the priority a thread inherits by
	 * taking a priority inheritence or protection mutex.  It
	 * is not affected by base priority changes.  Inherited
	 * priority defaults to and remains 0 until a mutex is taken
	 * that is being waited on by any other thread whose priority
	 * is non-zero.
	 */
	char			inherited_priority;

	/*
	 * Active priority is always the maximum of the threads base
	 * priority and inherited priority.  When there is a change
	 * in either the base or inherited priority, the active
	 * priority must be recalculated.
	 */
	char			active_priority;

	/* Number of priority ceiling or protection mutexes owned. */
	int			priority_mutex_count;

	/*
	 * Queue of currently owned mutexes.
	 */
	TAILQ_HEAD(, pthread_mutex)	mutexq;

	void				*ret;
	struct pthread_specific_elem	*specific;
	int				specific_data_count;

	/*
	 * Current locks bitmap for rtld.
	 */
	int	rtld_bits;

	/* Cleanup handlers Link List */
	struct pthread_cleanup *cleanup;
	char			*fname;	/* Ptr to source file name  */
	int			lineno;	/* Source line number.      */
};

/*
 * Critical regions can also be detected by looking at the threads
 * current lock level.  Ensure these macros increment and decrement
 * the lock levels such that locks can not be held with a lock level
 * of 0.
 */
#define	THR_IN_CRITICAL(thrd)					\
	(((thrd)->locklevel > 0) ||				\
	((thrd)->critical_count > 0))

#define	THR_YIELD_CHECK(thrd)					\
do {								\
	if (((thrd)->critical_yield != 0) &&			\
	    !(THR_IN_CRITICAL(thrd)))				\
		_thr_sched_switch(thrd);			\
	else if (((thrd)->check_pending != 0) &&		\
	    !(THR_IN_CRITICAL(thrd)))				\
		_thr_sig_check_pending(thrd);			\
} while (0)

#define	THR_LOCK_ACQUIRE(thrd, lck)				\
do {								\
	if ((thrd)->locklevel >= MAX_THR_LOCKLEVEL)		\
		PANIC("Exceeded maximum lock level");		\
	else {							\
		THR_DEACTIVATE_LAST_LOCK(thrd);			\
		(thrd)->locklevel++;				\
		_lock_acquire((lck),				\
		    &(thrd)->lockusers[(thrd)->locklevel - 1],	\
		    (thrd)->active_priority);			\
	}							\
} while (0)

#define	THR_LOCK_RELEASE(thrd, lck)				\
do {								\
	if ((thrd)->locklevel > 0) {				\
		_lock_release((lck),				\
		    &(thrd)->lockusers[(thrd)->locklevel - 1]);	\
		(thrd)->locklevel--;				\
		THR_ACTIVATE_LAST_LOCK(thrd);			\
		if ((thrd)->locklevel == 0)			\
			THR_YIELD_CHECK(thrd);			\
	}							\
} while (0)

#define THR_ACTIVATE_LAST_LOCK(thrd)					\
do {									\
	if ((thrd)->locklevel > 0)					\
		_lockuser_setactive(					\
		    &(thrd)->lockusers[(thrd)->locklevel - 1], 1);	\
} while (0)

#define	THR_DEACTIVATE_LAST_LOCK(thrd)					\
do {									\
	if ((thrd)->locklevel > 0)					\
		_lockuser_setactive(					\
		    &(thrd)->lockusers[(thrd)->locklevel - 1], 0);	\
} while (0)

/*
 * For now, threads will have their own lock separate from their
 * KSE scheduling lock.
 */
#define	THR_LOCK(thr)			THR_LOCK_ACQUIRE(thr, &(thr)->lock)
#define	THR_UNLOCK(thr)			THR_LOCK_RELEASE(thr, &(thr)->lock)
#define	THR_THREAD_LOCK(curthrd, thr)	THR_LOCK_ACQUIRE(curthrd, &(thr)->lock)
#define	THR_THREAD_UNLOCK(curthrd, thr)	THR_LOCK_RELEASE(curthrd, &(thr)->lock)

/*
 * Priority queue manipulation macros (using pqe link).  We use
 * the thread's kseg link instead of the kse link because a thread
 * does not (currently) have a statically assigned kse.
 */
#define THR_RUNQ_INSERT_HEAD(thrd)	\
	_pq_insert_head(&(thrd)->kseg->kg_schedq.sq_runq, thrd)
#define THR_RUNQ_INSERT_TAIL(thrd)	\
	_pq_insert_tail(&(thrd)->kseg->kg_schedq.sq_runq, thrd)
#define THR_RUNQ_REMOVE(thrd)		\
	_pq_remove(&(thrd)->kseg->kg_schedq.sq_runq, thrd)
#define THR_RUNQ_FIRST()		\
	_pq_first(&(thrd)->kseg->kg_schedq.sq_runq)

/*
 * Macros to insert/remove threads to the all thread list and
 * the gc list.
 */
#define	THR_LIST_ADD(thrd) do {					\
	if (((thrd)->flags & THR_FLAGS_IN_TDLIST) == 0) {	\
		TAILQ_INSERT_HEAD(&_thread_list, thrd, tle);	\
		(thrd)->flags |= THR_FLAGS_IN_TDLIST;		\
	}							\
} while (0)
#define	THR_LIST_REMOVE(thrd) do {				\
	if (((thrd)->flags & THR_FLAGS_IN_TDLIST) != 0) {	\
		TAILQ_REMOVE(&_thread_list, thrd, tle);		\
		(thrd)->flags &= ~THR_FLAGS_IN_TDLIST;		\
	}							\
} while (0)
#define	THR_GCLIST_ADD(thrd) do {				\
	if (((thrd)->flags & THR_FLAGS_IN_GCLIST) == 0) {	\
		TAILQ_INSERT_HEAD(&_thread_gc_list, thrd, gcle);\
		(thrd)->flags |= THR_FLAGS_IN_GCLIST;		\
		_gc_count++;					\
	}							\
} while (0)
#define	THR_GCLIST_REMOVE(thrd) do {				\
	if (((thrd)->flags & THR_FLAGS_IN_GCLIST) != 0) {	\
		TAILQ_REMOVE(&_thread_gc_list, thrd, gcle);	\
		(thrd)->flags &= ~THR_FLAGS_IN_GCLIST;		\
		_gc_count--;					\
	}							\
} while (0)

#define GC_NEEDED()	(atomic_load_acq_int(&_gc_count) >= 5)

/*
 * Locking the scheduling queue for another thread uses that thread's
 * KSEG lock.
 */
#define	THR_SCHED_LOCK(curthr, thr) do {		\
	(curthr)->critical[(curthr)->locklevel] = _kse_critical_enter(); \
	(curthr)->locklevel++;				\
	KSE_SCHED_LOCK((curthr)->kse, (thr)->kseg);	\
} while (0)

#define	THR_SCHED_UNLOCK(curthr, thr) do {		\
	KSE_SCHED_UNLOCK((curthr)->kse, (thr)->kseg);	\
	(curthr)->locklevel--;				\
	_kse_critical_leave((curthr)->critical[(curthr)->locklevel]); \
} while (0)

/* Take the scheduling lock with the intent to call the scheduler. */
#define	THR_LOCK_SWITCH(curthr) do {			\
	(void)_kse_critical_enter();			\
	KSE_SCHED_LOCK((curthr)->kse, (curthr)->kseg);	\
} while (0)

#define	THR_CRITICAL_ENTER(thr)		(thr)->critical_count++
#define	THR_CRITICAL_LEAVE(thr)	do {		\
	(thr)->critical_count--;		\
	if (((thr)->critical_yield != 0) &&	\
	    ((thr)->critical_count == 0)) {	\
		(thr)->critical_yield = 0;	\
		_thr_sched_switch(thr);		\
	}					\
} while (0)

#define	THR_IS_ACTIVE(thrd) \
	((thrd)->kse != NULL) && ((thrd)->kse->k_curthread == (thrd))

#define	THR_IN_SYNCQ(thrd)	(((thrd)->sflags & THR_FLAGS_IN_SYNCQ) != 0)

#define	THR_IS_SUSPENDED(thrd) \
	(((thrd)->state == PS_SUSPENDED) || \
	(((thrd)->flags & THR_FLAGS_SUSPENDED) != 0))
#define	THR_IS_EXITING(thrd)	(((thrd)->flags & THR_FLAGS_EXITING) != 0)
	
/*
 * Global variables for the pthread kernel.
 */

SCLASS void		*_usrstack	SCLASS_PRESET(NULL);
SCLASS struct kse	*_kse_initial	SCLASS_PRESET(NULL);
SCLASS struct pthread	*_thr_initial	SCLASS_PRESET(NULL);

/* List of all threads: */
SCLASS TAILQ_HEAD(, pthread)	_thread_list
    SCLASS_PRESET(TAILQ_HEAD_INITIALIZER(_thread_list));

/* List of threads needing GC: */
SCLASS TAILQ_HEAD(, pthread)	_thread_gc_list
    SCLASS_PRESET(TAILQ_HEAD_INITIALIZER(_thread_gc_list));

/* Default thread attributes: */
SCLASS struct pthread_attr _pthread_attr_default
    SCLASS_PRESET({
	SCHED_RR, 0, TIMESLICE_USEC, THR_DEFAULT_PRIORITY,
	THR_CREATE_RUNNING,	PTHREAD_CREATE_JOINABLE, NULL,
	NULL, NULL, THR_STACK_DEFAULT
    });

/* Default mutex attributes: */
SCLASS struct pthread_mutex_attr _pthread_mutexattr_default
    SCLASS_PRESET({PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE, 0, 0 });

/* Default condition variable attributes: */
SCLASS struct pthread_cond_attr _pthread_condattr_default
    SCLASS_PRESET({COND_TYPE_FAST, 0});

/* Clock resolution in usec.	*/
SCLASS int		_clock_res_usec		SCLASS_PRESET(CLOCK_RES_USEC);

/* Array of signal actions for this process: */
SCLASS struct sigaction	_thread_sigact[NSIG];

/*
 * Array of counts of dummy handlers for SIG_DFL signals.  This is used to
 * assure that there is always a dummy signal handler installed while there
 * is a thread sigwait()ing on the corresponding signal.
 */
SCLASS int		_thread_dfl_count[NSIG];

/*
 * Lock for above count of dummy handlers and for the process signal
 * mask and pending signal sets.
 */
SCLASS struct lock	_thread_signal_lock;

/* Pending signals and mask for this process: */
SCLASS sigset_t		_thr_proc_sigpending;
SCLASS sigset_t		_thr_proc_sigmask	SCLASS_PRESET({{0, 0, 0, 0}});
SCLASS siginfo_t	_thr_proc_siginfo[NSIG];

SCLASS pid_t		_thr_pid		SCLASS_PRESET(0);

/* Garbage collector lock. */
SCLASS struct lock	_gc_lock;
SCLASS int		_gc_check		SCLASS_PRESET(0);
SCLASS int		_gc_count		SCLASS_PRESET(0);

SCLASS struct lock	_mutex_static_lock;
SCLASS struct lock	_rwlock_static_lock;
SCLASS struct lock	_keytable_lock;
SCLASS struct lock	_thread_list_lock;
SCLASS int		_thr_guard_default;
SCLASS int		_thr_page_size;

SCLASS int		_thr_debug_flags	SCLASS_PRESET(0);

/* Undefine the storage class and preset specifiers: */
#undef  SCLASS
#undef	SCLASS_PRESET


/*
 * Function prototype definitions.
 */
__BEGIN_DECLS
int	_cond_reinit(pthread_cond_t *);
void	_cond_wait_backout(struct pthread *);
struct pthread *_get_curthread(void);
struct kse *_get_curkse(void);
void	_set_curkse(struct kse *);
struct kse *_kse_alloc(struct pthread *);
kse_critical_t _kse_critical_enter(void);
void	_kse_critical_leave(kse_critical_t);
int	_kse_in_critical(void);
void	_kse_free(struct pthread *, struct kse *);
void	_kse_init();
struct kse_group *_kseg_alloc(struct pthread *);
void	_kse_lock_wait(struct lock *, struct lockuser *lu);
void	_kse_lock_wakeup(struct lock *, struct lockuser *lu);
void	_kse_sig_check_pending(struct kse *);
void	_kse_single_thread(struct pthread *);
void	_kse_start(struct kse *);
int	_kse_setthreaded(int);
int	_kse_isthreaded(void);
void	_kseg_free(struct kse_group *);
int	_mutex_cv_lock(pthread_mutex_t *);
int	_mutex_cv_unlock(pthread_mutex_t *);
void	_mutex_lock_backout(struct pthread *);
void	_mutex_notify_priochange(struct pthread *, struct pthread *, int);
int	_mutex_reinit(struct pthread_mutex *);
void	_mutex_unlock_private(struct pthread *);
void	_libpthread_init(struct pthread *);
int	_pq_alloc(struct pq_queue *, int, int);
void	_pq_free(struct pq_queue *);
int	_pq_init(struct pq_queue *);
void	_pq_remove(struct pq_queue *pq, struct pthread *);
void	_pq_insert_head(struct pq_queue *pq, struct pthread *);
void	_pq_insert_tail(struct pq_queue *pq, struct pthread *);
struct pthread *_pq_first(struct pq_queue *pq);
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
int	_pthread_rwlock_init(pthread_rwlock_t *, const pthread_rwlockattr_t *);
int	_pthread_rwlock_destroy (pthread_rwlock_t *);
struct pthread *_pthread_self(void);
int	_pthread_setspecific(pthread_key_t, const void *);
struct pthread *_thr_alloc(struct pthread *);
int	_thread_enter_uts(struct kse_thr_mailbox *, struct kse_mailbox *);
int	_thread_switch(struct kse_thr_mailbox *, struct kse_thr_mailbox **);
void	_thr_exit(char *, int, char *);
void	_thr_exit_cleanup(void);
void	_thr_lock_wait(struct lock *lock, struct lockuser *lu);
void	_thr_lock_wakeup(struct lock *lock, struct lockuser *lu);
int	_thr_ref_add(struct pthread *, struct pthread *, int);
void	_thr_ref_delete(struct pthread *, struct pthread *);
int	_thr_schedule_add(struct pthread *, struct pthread *);
void	_thr_schedule_remove(struct pthread *, struct pthread *);
void	_thr_setrunnable(struct pthread *curthread, struct pthread *thread);
void	_thr_setrunnable_unlocked(struct pthread *thread);
void	_thr_sig_add(struct pthread *, int, siginfo_t *);
void	_thr_sig_dispatch(struct kse *, int, siginfo_t *);
int	_thr_stack_alloc(struct pthread_attr *);
void	_thr_stack_free(struct pthread_attr *);
void    _thr_exit_cleanup(void);
void	_thr_free(struct pthread *, struct pthread *);
void	_thr_gc(struct pthread *);
void    _thr_panic_exit(char *, int, char *);
void    _thread_cleanupspecific(void);
void    _thread_dump_info(void);
void	_thread_printf(int, const char *, ...);
void    _thr_sched_frame(struct pthread_sigframe *);
void	_thr_sched_switch(struct pthread *);
void	_thr_sched_switch_unlocked(struct pthread *);
void    _thr_set_timeout(const struct timespec *);
void	_thr_seterrno(struct pthread *, int);
void    _thr_sig_handler(int, siginfo_t *, ucontext_t *);
void    _thr_sig_check_pending(struct pthread *);
void	_thr_sig_rundown(struct pthread *, ucontext_t *,
	    struct pthread_sigframe *);
void	_thr_sig_send(struct pthread *pthread, int sig);
void	_thr_sig_wrapper(void);
void	_thr_sigframe_restore(struct pthread *thread, struct pthread_sigframe *psf);
void	_thr_spinlock_init(void);
void	_thr_enter_cancellation_point(struct pthread *);
void	_thr_leave_cancellation_point(struct pthread *);
int	_thr_setconcurrency(int new_level);
int	_thr_setmaxconcurrency(void);

/*
 * Aliases for _pthread functions. Should be called instead of
 * originals if PLT replocation is unwanted at runtme.
 */
int	_thr_cond_broadcast(pthread_cond_t *);
int	_thr_cond_signal(pthread_cond_t *);
int	_thr_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int	_thr_mutex_lock(pthread_mutex_t *);
int	_thr_mutex_unlock(pthread_mutex_t *);
int	_thr_rwlock_rdlock (pthread_rwlock_t *);
int	_thr_rwlock_wrlock (pthread_rwlock_t *);
int	_thr_rwlock_unlock (pthread_rwlock_t *);

/* XXX - Stuff that goes away when my sources get more up to date. */
/* #include <sys/kse.h> */
#ifdef SYS_KSE_H
int	__sys_kse_create(struct kse_mailbox *, int);
int	__sys_kse_thr_wakeup(struct kse_mailbox *);
int	__sys_kse_exit(struct kse_mailbox *);
int	__sys_kse_release(struct kse_mailbox *);
#endif

/* #include <sys/aio.h> */
#ifdef _SYS_AIO_H_
int	__sys_aio_suspend(const struct aiocb * const[], int, const struct timespec *);
#endif

/* #include <fcntl.h> */
#ifdef  _SYS_FCNTL_H_
int     __sys_fcntl(int, int, ...);
int     __sys_open(const char *, int, ...);
#endif

/* #include <sys/ioctl.h> */
#ifdef _SYS_IOCTL_H_
int	__sys_ioctl(int, unsigned long, ...);
#endif

/* #inclde <sched.h> */
#ifdef	_SCHED_H_
int	__sys_sched_yield(void);
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
#endif

/* #include <sys/socket.h> */
#ifdef _SYS_SOCKET_H_
int	__sys_sendfile(int, int, off_t, size_t, struct sf_hdtr *,
	    off_t *, int);
#endif

/* #include <sys/uio.h> */
#ifdef  _SYS_UIO_H_
ssize_t __sys_readv(int, const struct iovec *, int);
ssize_t __sys_writev(int, const struct iovec *, int);
#endif

/* #include <time.h> */
#ifdef	_TIME_H_
int	__sys_nanosleep(const struct timespec *, struct timespec *);
#endif

/* #include <unistd.h> */
#ifdef  _UNISTD_H_
int     __sys_close(int);
int     __sys_execve(const char *, char * const *, char * const *);
int	__sys_fork(void);
int	__sys_fsync(int);
pid_t	__sys_getpid(void);
int     __sys_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
ssize_t __sys_read(int, void *, size_t);
ssize_t __sys_write(int, const void *, size_t);
void	__sys_exit(int);
#endif

/* #include <poll.h> */
#ifdef _SYS_POLL_H_
int 	__sys_poll(struct pollfd *, unsigned, int);
#endif

/* #include <sys/mman.h> */
#ifdef _SYS_MMAN_H_
int	__sys_msync(void *, size_t, int);
#endif

#endif  /* !_THR_PRIVATE_H */
