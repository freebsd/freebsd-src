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

#ifndef _PTHREAD_PRIVATE_H
#define _PTHREAD_PRIVATE_H

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
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/cdefs.h>
#include <sched.h>
#include <spinlock.h>
#include <pthread_np.h>

/*
 * Define machine dependent macros to get and set the stack pointer
 * from the supported contexts.  Also define a macro to set the return
 * address in a jmp_buf context.
 *
 * XXX - These need to be moved into architecture dependent support files.
 */
#if	defined(__i386__)
#define	GET_STACK_JB(jb)	((unsigned long)((jb)[0]._jb[2]))
#define	GET_STACK_SJB(sjb)	((unsigned long)((sjb)[0]._sjb[2]))
#define	GET_STACK_UC(ucp)	((unsigned long)((ucp)->uc_mcontext.mc_esp))
#define	SET_STACK_JB(jb, stk)	(jb)[0]._jb[2] = (int)(stk)
#define	SET_STACK_SJB(sjb, stk)	(sjb)[0]._sjb[2] = (int)(stk)
#define	SET_STACK_UC(ucp, stk)	(ucp)->uc_mcontext.mc_esp = (int)(stk)
#define	FP_SAVE_UC(ucp)		do {			\
	char	*fdata;					\
	fdata = (char *) (ucp)->uc_mcontext.mc_fpregs;	\
	__asm__("fnsave %0": :"m"(*fdata));		\
} while (0)
#define	FP_RESTORE_UC(ucp)	do {			\
	char	*fdata;					\
	fdata = (char *) (ucp)->uc_mcontext.mc_fpregs;	\
	__asm__("frstor %0": :"m"(*fdata));		\
} while (0)
#define SET_RETURN_ADDR_JB(jb, ra)	(jb)[0]._jb[0] = (int)(ra)
#elif	defined(__alpha__)
#include <machine/reg.h>
#define	GET_STACK_JB(jb)	((unsigned long)((jb)[0]._jb[R_SP + 4]))
#define	GET_STACK_SJB(sjb)	((unsigned long)((sjb)[0]._sjb[R_SP + 4]))
#define	GET_STACK_UC(ucp)	((ucp)->uc_mcontext.mc_regs[R_SP])
#define	SET_STACK_JB(jb, stk)	(jb)[0]._jb[R_SP + 4] = (long)(stk)
#define	SET_STACK_SJB(sjb, stk)	(sjb)[0]._sjb[R_SP + 4] = (long)(stk)
#define	SET_STACK_UC(ucp, stk)	(ucp)->uc_mcontext.mc_regs[R_SP] = (unsigned long)(stk)
#define	FP_SAVE_UC(ucp)
#define	FP_RESTORE_UC(ucp)
#define SET_RETURN_ADDR_JB(jb, ra) do {		\
	(jb)[0]._jb[2] = (long)(ra);		\
	(jb)[0]._jb[R_RA + 4] = 0;		\
	(jb)[0]._jb[R_T12 + 4] = (long)(ra);	\
} while (0)
#else
#error "Don't recognize this architecture!"
#endif

/*
 * Kernel fatal error handler macro.
 */
#define PANIC(string)   _thread_exit(__FILE__,__LINE__,string)


/* Output debug messages like this: */
#define stdout_debug(args...)	do {		\
	char buf[128];				\
	snprintf(buf, sizeof(buf), ##args);	\
	_thread_sys_write(1, buf, strlen(buf));	\
} while (0)
#define stderr_debug(args...)	do {		\
	char buf[128];				\
	snprintf(buf, sizeof(buf), ##args);	\
	_thread_sys_write(2, buf, strlen(buf));	\
} while (0)



/*
 * Priority queue manipulation macros (using pqe link):
 */
#define PTHREAD_PRIOQ_INSERT_HEAD(thrd)	_pq_insert_head(&_readyq,thrd)
#define PTHREAD_PRIOQ_INSERT_TAIL(thrd)	_pq_insert_tail(&_readyq,thrd)
#define PTHREAD_PRIOQ_REMOVE(thrd)	_pq_remove(&_readyq,thrd)
#define PTHREAD_PRIOQ_FIRST()		_pq_first(&_readyq)

/*
 * Waiting queue manipulation macros (using pqe link):
 */
#define PTHREAD_WAITQ_REMOVE(thrd)	_waitq_remove(thrd)
#define PTHREAD_WAITQ_INSERT(thrd)	_waitq_insert(thrd)

#if defined(_PTHREADS_INVARIANTS)
#define PTHREAD_WAITQ_CLEARACTIVE()	_waitq_clearactive()
#define PTHREAD_WAITQ_SETACTIVE()	_waitq_setactive()
#else
#define PTHREAD_WAITQ_CLEARACTIVE()
#define PTHREAD_WAITQ_SETACTIVE()
#endif

/*
 * Work queue manipulation macros (using qe link):
 */
#define PTHREAD_WORKQ_INSERT(thrd) do {					\
	TAILQ_INSERT_TAIL(&_workq,thrd,qe);				\
	(thrd)->flags |= PTHREAD_FLAGS_IN_WORKQ;			\
} while (0)
#define PTHREAD_WORKQ_REMOVE(thrd) do {					\
	TAILQ_REMOVE(&_workq,thrd,qe);					\
	(thrd)->flags &= ~PTHREAD_FLAGS_IN_WORKQ;			\
} while (0)


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
 * called with preemption deferred (see thread_kern_sched_[un]defer).
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
	if (_thread_kern_new_state != 0)				\
		PANIC("Recursive PTHREAD_NEW_STATE");			\
	_thread_kern_new_state = 1;					\
	if ((thrd)->state != newstate) {				\
		if ((thrd)->state == PS_RUNNING) {			\
			PTHREAD_PRIOQ_REMOVE(thrd);			\
			PTHREAD_WAITQ_INSERT(thrd);			\
		} else if (newstate == PS_RUNNING) { 			\
			PTHREAD_WAITQ_REMOVE(thrd);			\
			PTHREAD_PRIOQ_INSERT_TAIL(thrd);		\
		}							\
	}								\
	_thread_kern_new_state = 0;					\
	PTHREAD_SET_STATE(thrd, newstate);				\
} while (0)
#else
#define PTHREAD_ASSERT(cond, msg)
#define PTHREAD_ASSERT_NOT_IN_SYNCQ(thrd)
#define PTHREAD_NEW_STATE(thrd, newstate) do {				\
	if ((thrd)->state != newstate) {				\
		if ((thrd)->state == PS_RUNNING) {			\
			PTHREAD_PRIOQ_REMOVE(thrd);			\
			PTHREAD_WAITQ_INSERT(thrd);			\
		} else if (newstate == PS_RUNNING) { 			\
			PTHREAD_WAITQ_REMOVE(thrd);			\
			PTHREAD_PRIOQ_INSERT_TAIL(thrd);		\
		}							\
	}								\
	PTHREAD_SET_STATE(thrd, newstate);				\
} while (0)
#endif

/*
 * Define the signals to be used for scheduling.
 */
#if defined(_PTHREADS_COMPAT_SCHED)
#define _ITIMER_SCHED_TIMER	ITIMER_VIRTUAL
#define _SCHED_SIGNAL		SIGVTALRM
#else
#define _ITIMER_SCHED_TIMER	ITIMER_PROF
#define _SCHED_SIGNAL		SIGPROF
#endif

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
} pq_queue_t;


/*
 * TailQ initialization values.
 */
#define TAILQ_INITIALIZER	{ NULL, NULL }

/* 
 * Mutex definitions.
 */
union pthread_mutex_data {
	void	*m_ptr;
	int	m_count;
};

struct pthread_mutex {
	enum pthread_mutextype		m_type;
	int				m_protocol;
	TAILQ_HEAD(mutex_head, pthread)	m_queue;
	struct pthread			*m_owner;
	union pthread_mutex_data	m_data;
	long				m_flags;
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

	/*
	 * Lock for accesses to this structure.
	 */
	spinlock_t			lock;
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
#define PTHREAD_MUTEX_STATIC_INITIALIZER   \
	{ PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE, TAILQ_INITIALIZER, \
	NULL, { NULL }, MUTEX_FLAGS_PRIVATE, 0, 0, 0, TAILQ_INITIALIZER, \
	_SPINLOCK_INITIALIZER }

struct pthread_mutex_attr {
	enum pthread_mutextype	m_type;
	int			m_protocol;
	int			m_ceiling;
	long			m_flags;
};

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
	spinlock_t			lock;
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
#define PTHREAD_COND_STATIC_INITIALIZER    \
	{ COND_TYPE_FAST, TAILQ_INITIALIZER, NULL, NULL, \
	0, 0, _SPINLOCK_INITIALIZER }

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
};

/*
 * Thread creation state attributes.
 */
#define PTHREAD_CREATE_RUNNING			0
#define PTHREAD_CREATE_SUSPENDED		1

/*
 * Additional state for a thread suspended with pthread_suspend_np().
 */
enum pthread_susp {
	SUSP_NO,	/* Not suspended. */
	SUSP_YES,	/* Suspended. */
	SUSP_NOWAIT,	/* Suspended, was in a mutex or condition queue. */
	SUSP_MUTEX_WAIT,/* Suspended, still in a mutex queue. */
	SUSP_COND_WAIT	/* Suspended, still in a condition queue. */
};

/*
 * Miscellaneous definitions.
 */
#define PTHREAD_STACK_DEFAULT			65536
/*
 * Size of red zone at the end of each stack.  In actuality, this "red zone" is
 * merely an unmapped region, except in the case of the initial stack.  Since
 * mmap() makes it possible to specify the maximum growth of a MAP_STACK region,
 * an unmapped gap between thread stacks achieves the same effect as explicitly
 * mapped red zones.
 */
#define PTHREAD_STACK_GUARD			PAGE_SIZE

/*
 * Maximum size of initial thread's stack.  This perhaps deserves to be larger
 * than the stacks of other threads, since many applications are likely to run
 * almost entirely on this stack.
 */
#define PTHREAD_STACK_INITIAL			0x100000

/* Size of the scheduler stack: */
#define SCHED_STACK_SIZE			PAGE_SIZE

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

/*
 * Time slice period in microseconds.
 */
#define TIMESLICE_USEC				20000

/*
 * Define a thread-safe macro to get the current time of day
 * which is updated at regular intervals by the scheduling signal
 * handler.
 */
#define	GET_CURRENT_TOD(tv)				\
	do {						\
		tv.tv_sec = _sched_tod.tv_sec;		\
		tv.tv_usec = _sched_tod.tv_usec;	\
	} while (tv.tv_sec != _sched_tod.tv_sec)


struct pthread_key {
	spinlock_t	lock;
	volatile int	allocated;
	volatile int	count;
	void            (*destructor) ();
};

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
	PS_SIGTHREAD,
	PS_MUTEX_WAIT,
	PS_COND_WAIT,
	PS_FDLR_WAIT,
	PS_FDLW_WAIT,
	PS_FDR_WAIT,
	PS_FDW_WAIT,
	PS_FILE_WAIT,
	PS_POLL_WAIT,
	PS_SELECT_WAIT,
	PS_SLEEP_WAIT,
	PS_WAIT_WAIT,
	PS_SIGSUSPEND,
	PS_SIGWAIT,
	PS_SPINBLOCK,
	PS_JOIN,
	PS_SUSPENDED,
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

/*
 * File descriptor table structure.
 */
struct fd_table_entry {
	/*
	 * Lock for accesses to this file descriptor table
	 * entry. This is passed to _spinlock() to provide atomic
	 * access to this structure. It does *not* represent the
	 * state of the lock on the file descriptor.
	 */
	spinlock_t		lock;
	TAILQ_HEAD(, pthread)	r_queue;	/* Read queue.                        */
	TAILQ_HEAD(, pthread)	w_queue;	/* Write queue.                       */
	struct pthread		*r_owner;	/* Ptr to thread owning read lock.    */
	struct pthread		*w_owner;	/* Ptr to thread owning write lock.   */
	char			*r_fname;	/* Ptr to read lock source file name  */
	int			r_lineno;	/* Read lock source line number.      */
	char			*w_fname;	/* Ptr to write lock source file name */
	int			w_lineno;	/* Write lock source line number.     */
	int			r_lockcount;	/* Count for FILE read locks.         */
	int			w_lockcount;	/* Count for FILE write locks.        */
	int			flags;		/* Flags used in open.                */
};

struct pthread_poll_data {
	int	nfds;
	struct pollfd *fds;
};

union pthread_wait_data {
	pthread_mutex_t	mutex;
	pthread_cond_t	cond;
	const sigset_t	*sigwait;	/* Waiting on a signal in sigwait */
	struct {
		short	fd;		/* Used when thread waiting on fd */
		short	branch;		/* Line number, for debugging.    */
		char	*fname;		/* Source file name for debugging.*/
	} fd;
	FILE		*fp;
	struct pthread_poll_data *poll_data;
	spinlock_t	*spinlock;
	struct pthread	*thread;
};

/*
 * Define a continuation routine that can be used to perform a
 * transfer of control:
 */
typedef void	(*thread_continuation_t) (void *);

struct pthread_signal_frame;

struct pthread_state_data {
	struct pthread_signal_frame *psd_curframe;
	sigset_t		psd_sigmask;
	struct timespec		psd_wakeup_time;
	union pthread_wait_data psd_wait_data;
	enum pthread_state	psd_state;
	int			psd_flags;
	int			psd_interrupted;
	int			psd_longjmp_val;
	int			psd_sigmask_seqno;
	int			psd_signo;
	int			psd_sig_defer_count;
	/* XXX - What about thread->timeout and/or thread->error? */
};


/*
 * Normally thread contexts are stored as jmp_bufs via _setjmp()/_longjmp(),
 * but they may also be sigjmp_buf and ucontext_t.  When a thread is
 * interrupted by a signal, it's context is saved as a ucontext_t.  An
 * application is also free to use [_]longjmp()/[_]siglongjmp() to jump
 * between contexts within the same thread.  Future support will also
 * include setcontext()/getcontext().
 *
 * Define an enumerated type that can identify the 4 different context
 * types.
 */
typedef enum {
	CTX_JB_NOSIG,	/* context is jmp_buf without saved sigset */
	CTX_JB,		/* context is jmp_buf (with saved sigset) */
	CTX_SJB,	/* context is sigjmp_buf (with saved sigset) */
	CTX_UC		/* context is ucontext_t (with saved sigset) */
} thread_context_t;

/*
 * There are 2 basic contexts that a frame may contain at any
 * one time:
 *
 *   o ctx - The context that the thread should return to after normal
 *     completion of the signal handler.
 *   o sig_jb - The context just before the signal handler is invoked.
 *     Attempts at abnormal returns from user supplied signal handlers
 *     will return back to the signal context to perform any necessary
 *     cleanup.
 */
struct pthread_signal_frame {
	/*
	 * This stores the threads state before the signal.
	 */
	struct pthread_state_data saved_state;

	/*
	 * Threads return context; ctxtype identifies the type of context.
	 * For signal frame 0, these point to the context storage area
	 * within the pthread structure.  When handling signals (frame > 0),
	 * these point to a context storage area that is allocated off the
	 * threads stack.
	 */
	union {
		jmp_buf		jb;
		sigjmp_buf	sigjb;
		ucontext_t	uc;
	} ctx;
	thread_context_t	ctxtype;
	int			longjmp_val;
	int			signo;	/* signal, arg 1 to sighandler */
	int			sig_has_args;	/* use signal args if true */
	ucontext_t		uc;
	siginfo_t		siginfo;
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
	 * Threads return context; ctxtype identifies the type of context.
	 */
	union {
		jmp_buf		jb;
		sigjmp_buf	sigjb;
		ucontext_t	uc;
	} ctx;
	thread_context_t	ctxtype;
	int			longjmp_val;

	/*
	 * Used for tracking delivery of signal handlers.
	 */
	struct pthread_signal_frame	*curframe;

 	/*
	 * Cancelability flags - the lower 2 bits are used by cancel
	 * definitions in pthread.h
	 */
#define PTHREAD_AT_CANCEL_POINT		0x0004
#define PTHREAD_CANCELLING		0x0008
#define PTHREAD_CANCEL_NEEDED		0x0010
	int	cancelflags;

	enum pthread_susp	suspended;

	thread_continuation_t	continuation;

	/*
	 * Current signal mask and pending signals.
	 */
	sigset_t	sigmask;
	sigset_t	sigpend;
	int		sigmask_seqno;
	int		check_pending;

	/* Thread state: */
	enum pthread_state	state;

	/* Scheduling clock when this thread was last made active. */
	long	last_active;

	/* Scheduling clock when this thread was last made inactive. */
	long	last_inactive;

	/*
	 * Number of microseconds accumulated by this thread when
	 * time slicing is active.
	 */
	long	slice_usec;

	/*
	 * Time to wake up thread. This is used for sleeping threads and
	 * for any operation which may time out (such as select).
	 */
	struct timespec	wakeup_time;

	/* TRUE if operation has timed out. */
	int	timeout;

	/*
	 * Error variable used instead of errno. The function __error()
	 * returns a pointer to this. 
	 */
	int	error;

	/* Join queue head and link for waiting threads: */
	TAILQ_HEAD(join_head, pthread)	join_queue;

	/*
	 * The current thread can belong to only one scheduling queue at
	 * a time (ready or waiting queue).  It can also belong to:
	 *
	 *   o A queue of threads waiting for a mutex
	 *   o A queue of threads waiting for a condition variable
	 *   o A queue of threads waiting for another thread to terminate
	 *     (the join queue above)
	 *   o A queue of threads waiting for a file descriptor lock
	 *   o A queue of threads needing work done by the kernel thread
	 *     (waiting for a spinlock or file I/O)
	 *
	 * It is possible for a thread to belong to more than one of the
	 * above queues if it is handling a signal.  A thread may only
	 * enter a mutex, condition variable, or join queue when it is
	 * not being called from a signal handler.  If a thread is a
	 * member of one of these queues when a signal handler is invoked,
	 * it must remain in the queue.  For this reason, the links for
	 * these queues must not be (re)used for other queues.
	 *
	 * Use pqe for the scheduling queue link (both ready and waiting),
	 * sqe for synchronization (mutex, condition variable, and join)
	 * queue links, and qe for all other links.
	 */
	TAILQ_ENTRY(pthread)	pqe;	/* priority queue link */
	TAILQ_ENTRY(pthread)	sqe;	/* synchronization queue link */
	TAILQ_ENTRY(pthread)	qe;	/* all other queues link */

	/* Wait data. */
	union pthread_wait_data data;

	/*
	 * Allocated for converting select into poll.
	 */
	struct pthread_poll_data poll_data;

	/*
	 * Set to TRUE if a blocking operation was
	 * interrupted by a signal:
	 */
	int		interrupted;

	/* Signal number when in state PS_SIGWAIT: */
	int		signo;

	/*
	 * Set to non-zero when this thread has deferred signals.
	 * We allow for recursive deferral.
	 */
	int		sig_defer_count;

	/*
	 * Set to TRUE if this thread should yield after undeferring
	 * signals.
	 */
	int		yield_on_sig_undefer;

	/* Miscellaneous flags; only set with signals deferred. */
	int		flags;
#define PTHREAD_FLAGS_PRIVATE	0x0001
#define PTHREAD_EXITING		0x0002
#define PTHREAD_FLAGS_IN_WAITQ	0x0004	/* in waiting queue using pqe link */
#define PTHREAD_FLAGS_IN_PRIOQ	0x0008	/* in priority queue using pqe link */
#define PTHREAD_FLAGS_IN_WORKQ	0x0010	/* in work queue using qe link */
#define PTHREAD_FLAGS_IN_FILEQ	0x0020	/* in file lock queue using qe link */
#define PTHREAD_FLAGS_IN_FDQ	0x0040	/* in fd lock queue using qe link */
#define PTHREAD_FLAGS_IN_CONDQ	0x0080	/* in condition queue using sqe link*/
#define PTHREAD_FLAGS_IN_MUTEXQ	0x0100	/* in mutex queue using sqe link */
#define PTHREAD_FLAGS_IN_JOINQ	0x0200	/* in join queue using sqe link */
#define PTHREAD_FLAGS_TRACE	0x0400	/* for debugging purposes */
#define PTHREAD_FLAGS_IN_SYNCQ	\
    (PTHREAD_FLAGS_IN_CONDQ | PTHREAD_FLAGS_IN_MUTEXQ | PTHREAD_FLAGS_IN_JOINQ)

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

	void		*ret;
	const void	**specific_data;
	int		specific_data_count;

	/* Cleanup handlers Link List */
	struct pthread_cleanup *cleanup;
	char			*fname;	/* Ptr to source file name  */
	int			lineno;	/* Source line number.      */
};

/* Spare thread stack. */
struct stack {
	SLIST_ENTRY(stack)	qe; /* Queue entry for this stack. */
};

/*
 * Global variables for the uthread kernel.
 */

/* Kernel thread structure used when there are no running threads: */
SCLASS struct pthread   _thread_kern_thread;

/* Ptr to the thread structure for the running thread: */
SCLASS struct pthread   * volatile _thread_run
#ifdef GLOBAL_PTHREAD_PRIVATE
= &_thread_kern_thread;
#else
;
#endif

/* Ptr to the thread structure for the last user thread to run: */
SCLASS struct pthread   * volatile _last_user_thread
#ifdef GLOBAL_PTHREAD_PRIVATE
= &_thread_kern_thread;
#else
;
#endif

/*
 * Ptr to the thread running in single-threaded mode or NULL if
 * running multi-threaded (default POSIX behaviour).
 */
SCLASS struct pthread   * volatile _thread_single
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL;
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

/*
 * Array of kernel pipe file descriptors that are used to ensure that
 * no signals are missed in calls to _select.
 */
SCLASS int		_thread_kern_pipe[2]
#ifdef GLOBAL_PTHREAD_PRIVATE
= {
	-1,
	-1
};
#else
;
#endif
SCLASS int		volatile _queue_signals
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0;
#else
;
#endif
SCLASS int              _thread_kern_in_sched
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0;
#else
;
#endif

SCLASS int              _sig_in_handler
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0;
#else
;
#endif

/* Time of day at last scheduling timer signal: */
SCLASS struct timeval volatile	_sched_tod
#ifdef GLOBAL_PTHREAD_PRIVATE
= { 0, 0 };
#else
;
#endif

/*
 * Current scheduling timer ticks; used as resource usage.
 */
SCLASS unsigned int volatile	_sched_ticks
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0;
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
= { SCHED_RR, 0, TIMESLICE_USEC, PTHREAD_DEFAULT_PRIORITY, PTHREAD_CREATE_RUNNING,
	PTHREAD_CREATE_JOINABLE, NULL, NULL, NULL, PTHREAD_STACK_DEFAULT };
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

/*
 * Standard I/O file descriptors need special flag treatment since
 * setting one to non-blocking does all on *BSD. Sigh. This array
 * is used to store the initial flag settings.
 */
SCLASS int	_pthread_stdio_flags[3];

/* File table information: */
SCLASS struct fd_table_entry **_thread_fd_table
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL;
#else
;
#endif

/* Table for polling file descriptors: */
SCLASS struct pollfd *_thread_pfd_table
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL;
#else
;
#endif

SCLASS const int dtablecount
#ifdef GLOBAL_PTHREAD_PRIVATE
= 4096/sizeof(struct fd_table_entry);
#else
;
#endif
SCLASS int    _thread_dtablesize        /* Descriptor table size.           */
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0;
#else
;
#endif

SCLASS int    _clock_res_usec		/* Clock resolution in usec.	*/
#ifdef GLOBAL_PTHREAD_PRIVATE
= CLOCK_RES_USEC;
#else
;
#endif

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

/*
 * Array of counts of dummy handlers for SIG_DFL signals.  This is used to
 * assure that there is always a dummy signal handler installed while there is a
 * thread sigwait()ing on the corresponding signal.
 */
SCLASS int	_thread_dfl_count[NSIG];

/*
 * Pending signals and mask for this process:
 */
SCLASS sigset_t	_process_sigpending;
SCLASS sigset_t	_process_sigmask
#ifdef GLOBAL_PTHREAD_PRIVATE
= { {0, 0, 0, 0} }
#endif
;

/*
 * Scheduling queues:
 */
SCLASS pq_queue_t		_readyq;
SCLASS TAILQ_HEAD(, pthread)	_waitingq;

/*
 * Work queue:
 */
SCLASS TAILQ_HEAD(, pthread)	_workq;

/* Tracks the number of threads blocked while waiting for a spinlock. */
SCLASS	volatile int	_spinblock_count
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0
#endif
;

/* Used to maintain pending and active signals: */
struct sigstatus {
	int		pending;	/* Is this a pending signal? */
	int		blocked;	/*
					 * A handler is currently active for
					 * this signal; ignore subsequent
					 * signals until the handler is done.
					 */
	int		signo;		/* arg 1 to signal handler */
	siginfo_t	siginfo;	/* arg 2 to signal handler */
	ucontext_t	uc;		/* arg 3 to signal handler */
};

SCLASS struct sigstatus	_thread_sigq[NSIG];

/* Indicates that the signal queue needs to be checked. */
SCLASS	volatile int	_sigq_check_reqd
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0
#endif
;

/* The signal stack. */
SCLASS struct sigaltstack _thread_sigstack;

/* Thread switch hook. */
SCLASS pthread_switch_routine_t _sched_switch_hook
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL
#endif
;

/*
 * Spare stack queue.  Stacks of default size are cached in order to reduce
 * thread creation time.  Spare stacks are used in LIFO order to increase cache
 * locality.
 */
SCLASS SLIST_HEAD(, stack)	_stackq;

/*
 * Base address of next unallocated default-size {stack, red zone}.  Stacks are
 * allocated contiguously, starting below the bottom of the main stack.  When a
 * new stack is created, a red zone is created (actually, the red zone is simply
 * left unmapped) below the bottom of the stack, such that the stack will not be
 * able to grow all the way to the top of the next stack.  This isn't
 * fool-proof.  It is possible for a stack to grow by a large amount, such that
 * it grows into the next stack, and as long as the memory within the red zone
 * is never accessed, nothing will prevent one thread stack from trouncing all
 * over the next.
 */
SCLASS void *	_next_stack
#ifdef GLOBAL_PTHREAD_PRIVATE
/* main stack top   - main stack size       - stack size            - (red zone + main stack red zone) */
= (void *) USRSTACK - PTHREAD_STACK_INITIAL - PTHREAD_STACK_DEFAULT - (2 * PTHREAD_STACK_GUARD)
#endif
;

/*
 * Declare the kernel scheduler jump buffer and stack:
 */
SCLASS jmp_buf	_thread_kern_sched_jb;

SCLASS void *	_thread_kern_sched_stack
#ifdef GLOBAL_PTHREAD_PRIVATE
= NULL
#endif
;


/* Used for _PTHREADS_INVARIANTS checking. */
SCLASS int	_thread_kern_new_state
#ifdef GLOBAL_PTHREAD_PRIVATE
= 0
#endif
;

/* Undefine the storage class specifier: */
#undef  SCLASS

#ifdef	_LOCK_DEBUG
#define	_FD_LOCK(_fd,_type,_ts)		_thread_fd_lock_debug(_fd, _type, \
						_ts, __FILE__, __LINE__)
#define _FD_UNLOCK(_fd,_type)		_thread_fd_unlock_debug(_fd, _type, \
						__FILE__, __LINE__)
#else
#define	_FD_LOCK(_fd,_type,_ts)		_thread_fd_lock(_fd, _type, _ts)
#define _FD_UNLOCK(_fd,_type)		_thread_fd_unlock(_fd, _type)
#endif

/*
 * Function prototype definitions.
 */
__BEGIN_DECLS
char    *__ttyname_basic(int);
char    *__ttyname_r_basic(int, char *, size_t);
char    *ttyname_r(int, char *, size_t);
void	_cond_wait_backout(pthread_t);
void	_fd_lock_backout(pthread_t);
int     _find_dead_thread(pthread_t);
int     _find_thread(pthread_t);
void	_flockfile_backout(pthread_t);
void    _funlock_owned(pthread_t);
void	_join_backout(pthread_t);
int     _thread_create(pthread_t *,const pthread_attr_t *,void *(*start_routine)(void *),void *,pthread_t);
int     _thread_fd_lock(int, int, struct timespec *);
int     _thread_fd_lock_debug(int, int, struct timespec *,char *fname,int lineno);
int	_mutex_cv_lock(pthread_mutex_t *);
int	_mutex_cv_unlock(pthread_mutex_t *);
void	_mutex_lock_backout(pthread_t);
void	_mutex_notify_priochange(pthread_t);
int	_mutex_reinit(pthread_mutex_t *);
void	_mutex_unlock_private(pthread_t);
int	_cond_reinit(pthread_cond_t *);
int	_pq_alloc(struct pq_queue *, int, int);
int	_pq_init(struct pq_queue *);
void	_pq_remove(struct pq_queue *pq, struct pthread *);
void	_pq_insert_head(struct pq_queue *pq, struct pthread *);
void	_pq_insert_tail(struct pq_queue *pq, struct pthread *);
struct pthread *_pq_first(struct pq_queue *pq);
void	_waitq_insert(pthread_t pthread);
void	_waitq_remove(pthread_t pthread);
#if defined(_PTHREADS_INVARIANTS)
void	_waitq_setactive(void);
void	_waitq_clearactive(void);
#endif
void    _thread_exit(char *, int, char *);
void    _thread_exit_cleanup(void);
void    _thread_fd_unlock(int, int);
void    _thread_fd_unlock_debug(int, int, char *, int);
void    _thread_fd_unlock_owned(pthread_t);
void    *_thread_cleanup(pthread_t);
void    _thread_cleanupspecific(void);
void    _thread_dump_info(void);
void    _thread_init(void);
void    _thread_kern_sched(ucontext_t *);
void 	_thread_kern_scheduler(void);
void    _thread_kern_sched_frame(struct pthread_signal_frame *psf);
void    _thread_kern_sched_sig(void);
void    _thread_kern_sched_state(enum pthread_state, char *fname, int lineno);
void	_thread_kern_sched_state_unlock(enum pthread_state state,
	    spinlock_t *lock, char *fname, int lineno);
void    _thread_kern_set_timeout(const struct timespec *);
void    _thread_kern_sig_defer(void);
void    _thread_kern_sig_undefer(void);
void    _thread_sig_handler(int, siginfo_t *, ucontext_t *);
void    _thread_sig_check_pending(pthread_t pthread);
void    _thread_sig_handle_pending(void);
void	_thread_sig_send(pthread_t pthread, int sig);
void	_thread_sig_wrapper(void);
void	_thread_sigframe_restore(pthread_t thread, struct pthread_signal_frame *psf);
void    _thread_start(void);
void	_thread_seterrno(pthread_t, int);
int     _thread_fd_table_init(int fd);
pthread_addr_t _thread_gc(pthread_addr_t);
void	_thread_enter_cancellation_point(void);
void	_thread_leave_cancellation_point(void);
void	_thread_cancellation_point(void);

/* #include <signal.h> */
int     _thread_sys_sigaction(int, const struct sigaction *, struct sigaction *);
int     _thread_sys_sigpending(sigset_t *);
int     _thread_sys_sigprocmask(int, const sigset_t *, sigset_t *);
int     _thread_sys_sigsuspend(const sigset_t *);
int     _thread_sys_siginterrupt(int, int);
int     _thread_sys_sigpause(int);
int     _thread_sys_sigreturn(ucontext_t *);
int     _thread_sys_sigaltstack(const struct sigaltstack *, struct sigaltstack *);
int     _thread_sys_sigstack(const struct sigstack *, struct sigstack *);
int     _thread_sys_sigvec(int, struct sigvec *, struct sigvec *);
void    _thread_sys_psignal(unsigned int, const char *);
void    (*_thread_sys_signal(int, void (*)(int)))(int);

/* #include <sys/stat.h> */
#ifdef  _SYS_STAT_H_
int     _thread_sys_fchmod(int, mode_t);
int     _thread_sys_fstat(int, struct stat *);
int     _thread_sys_fchflags(int, u_long);
#endif

/* #include <sys/mount.h> */
#ifdef  _SYS_MOUNT_H_
int     _thread_sys_fstatfs(int, struct statfs *);
#endif
int     _thread_sys_pipe(int *);

/* #include <sys/socket.h> */
#ifdef  _SYS_SOCKET_H_
int     _thread_sys_accept(int, struct sockaddr *, int *);
int     _thread_sys_bind(int, const struct sockaddr *, int);
int     _thread_sys_connect(int, const struct sockaddr *, int);
int     _thread_sys_getpeername(int, struct sockaddr *, int *);
int     _thread_sys_getsockname(int, struct sockaddr *, int *);
int     _thread_sys_getsockopt(int, int, int, void *, int *);
int     _thread_sys_listen(int, int);
int     _thread_sys_setsockopt(int, int, int, const void *, int);
int     _thread_sys_shutdown(int, int);
int     _thread_sys_socket(int, int, int);
int     _thread_sys_socketpair(int, int, int, int *);
ssize_t _thread_sys_recv(int, void *, size_t, int);
ssize_t _thread_sys_recvfrom(int, void *, size_t, int, struct sockaddr *, int *);
ssize_t _thread_sys_recvmsg(int, struct msghdr *, int);
ssize_t _thread_sys_send(int, const void *, size_t, int);
ssize_t _thread_sys_sendmsg(int, const struct msghdr *, int);
ssize_t _thread_sys_sendto(int, const void *,size_t, int, const struct sockaddr *, int);
#endif

/* #include <stdio.h> */
#ifdef  _STDIO_H_
FILE    *_thread_sys_fdopen(int, const char *);
FILE    *_thread_sys_fopen(const char *, const char *);
FILE    *_thread_sys_freopen(const char *, const char *, FILE *);
FILE    *_thread_sys_popen(const char *, const char *);
FILE    *_thread_sys_tmpfile(void);
char    *_thread_sys_ctermid(char *);
char    *_thread_sys_cuserid(char *);
char    *_thread_sys_fgetln(FILE *, size_t *);
char    *_thread_sys_fgets(char *, int, FILE *);
char    *_thread_sys_gets(char *);
char    *_thread_sys_tempnam(const char *, const char *);
char    *_thread_sys_tmpnam(char *);
int     _thread_sys_fclose(FILE *);
int     _thread_sys_feof(FILE *);
int     _thread_sys_ferror(FILE *);
int     _thread_sys_fflush(FILE *);
int     _thread_sys_fgetc(FILE *);
int     _thread_sys_fgetpos(FILE *, fpos_t *);
int     _thread_sys_fileno(FILE *);
int     _thread_sys_fprintf(FILE *, const char *, ...);
int     _thread_sys_fpurge(FILE *);
int     _thread_sys_fputc(int, FILE *);
int     _thread_sys_fputs(const char *, FILE *);
int     _thread_sys_fscanf(FILE *, const char *, ...);
int     _thread_sys_fseek(FILE *, long, int);
int     _thread_sys_fsetpos(FILE *, const fpos_t *);
int     _thread_sys_getc(FILE *);
int     _thread_sys_getchar(void);
int     _thread_sys_getw(FILE *);
int     _thread_sys_pclose(FILE *);
int     _thread_sys_printf(const char *, ...);
int     _thread_sys_putc(int, FILE *);
int     _thread_sys_putchar(int);
int     _thread_sys_puts(const char *);
int     _thread_sys_putw(int, FILE *);
int     _thread_sys_remove(const char *);
int     _thread_sys_rename (const char *, const char *);
int     _thread_sys_scanf(const char *, ...);
int     _thread_sys_setlinebuf(FILE *);
int     _thread_sys_setvbuf(FILE *, char *, int, size_t);
int     _thread_sys_snprintf(char *, size_t, const char *, ...);
int     _thread_sys_sprintf(char *, const char *, ...);
int     _thread_sys_sscanf(const char *, const char *, ...);
int     _thread_sys_ungetc(int, FILE *);
int     _thread_sys_vfprintf(FILE *, const char *, _BSD_VA_LIST_);
int     _thread_sys_vprintf(const char *, _BSD_VA_LIST_);
int     _thread_sys_vscanf(const char *, _BSD_VA_LIST_);
int     _thread_sys_vsnprintf(char *, size_t, const char *, _BSD_VA_LIST_);
int     _thread_sys_vsprintf(char *, const char *, _BSD_VA_LIST_);
int     _thread_sys_vsscanf(const char *, const char *, _BSD_VA_LIST_);
long    _thread_sys_ftell(FILE *);
size_t  _thread_sys_fread(void *, size_t, size_t, FILE *);
size_t  _thread_sys_fwrite(const void *, size_t, size_t, FILE *);
void    _thread_sys_clearerr(FILE *);
void    _thread_sys_perror(const char *);
void    _thread_sys_rewind(FILE *);
void    _thread_sys_setbuf(FILE *, char *);
void    _thread_sys_setbuffer(FILE *, char *, int);
#endif

/* #include <unistd.h> */
#ifdef  _UNISTD_H_
char    *_thread_sys_ttyname(int);
int     _thread_sys_close(int);
int     _thread_sys_dup(int);
int     _thread_sys_dup2(int, int);
int     _thread_sys_exect(const char *, char * const *, char * const *);
int     _thread_sys_execve(const char *, char * const *, char * const *);
int     _thread_sys_fchdir(int);
int     _thread_sys_fchown(int, uid_t, gid_t);
int     _thread_sys_fsync(int);
int     _thread_sys_ftruncate(int, off_t);
int     _thread_sys_pause(void);
int     _thread_sys_pipe(int *);
int     _thread_sys_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
long    _thread_sys_fpathconf(int, int);
off_t   _thread_sys_lseek(int, off_t, int);
pid_t   _thread_sys_fork(void);
pid_t   _thread_sys_tcgetpgrp(int);
ssize_t _thread_sys_read(int, void *, size_t);
ssize_t _thread_sys_write(int, const void *, size_t);
void	_thread_sys__exit(int);
#endif

/* #include <fcntl.h> */
#ifdef  _SYS_FCNTL_H_
int     _thread_sys_creat(const char *, mode_t);
int     _thread_sys_fcntl(int, int, ...);
int     _thread_sys_flock(int, int);
int     _thread_sys_open(const char *, int, ...);
#endif

/* #include <sys/ioctl.h> */
#ifdef  _SYS_IOCTL_H_
int     _thread_sys_ioctl(int, unsigned long, ...);
#endif

/* #include <dirent.h> */
#ifdef  _DIRENT_H_
DIR     *___thread_sys_opendir2(const char *, int);
DIR     *_thread_sys_opendir(const char *);
int     _thread_sys_alphasort(const void *, const void *);
int     _thread_sys_scandir(const char *, struct dirent ***,
	int (*)(struct dirent *), int (*)(const void *, const void *));
int     _thread_sys_closedir(DIR *);
int     _thread_sys_getdirentries(int, char *, int, long *);
long    _thread_sys_telldir(const DIR *);
struct  dirent *_thread_sys_readdir(DIR *);
void    _thread_sys_rewinddir(DIR *);
void    _thread_sys_seekdir(DIR *, long);
#endif

/* #include <sys/uio.h> */
#ifdef  _SYS_UIO_H_
ssize_t _thread_sys_readv(int, const struct iovec *, int);
ssize_t _thread_sys_writev(int, const struct iovec *, int);
#endif

/* #include <sys/wait.h> */
#ifdef  WNOHANG
pid_t   _thread_sys_wait(int *);
pid_t   _thread_sys_waitpid(pid_t, int *, int);
pid_t   _thread_sys_wait3(int *, int, struct rusage *);
pid_t   _thread_sys_wait4(pid_t, int *, int, struct rusage *);
#endif

/* #include <poll.h> */
#ifdef _SYS_POLL_H_
int 	_thread_sys_poll(struct pollfd *, unsigned, int);
#endif

/* #include <sys/mman.h> */
#ifdef _SYS_MMAN_H_
int	_thread_sys_msync(void *, size_t, int);
#endif

/* #include <setjmp.h> */
#ifdef _SETJMP_H_
extern void	__siglongjmp(sigjmp_buf, int) __dead2;
extern void	__longjmp(jmp_buf, int) __dead2;
extern void	___longjmp(jmp_buf, int) __dead2;
#endif
__END_DECLS

#endif  /* !_PTHREAD_PRIVATE_H */
