/*-
 * Copyright (c) 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)proc.h	8.15 (Berkeley) 5/19/95
 * $FreeBSD$
 */

#ifndef _SYS_PROC_H_
#define	_SYS_PROC_H_

#include <sys/callout.h>		/* For struct callout. */
#include <sys/event.h>			/* For struct klist. */
#ifndef _KERNEL
#include <sys/filedesc.h>
#endif
#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/priority.h>
#include <sys/rtprio.h>			/* XXX. */
#include <sys/runq.h>
#include <sys/resource.h>
#include <sys/sigio.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#ifndef _KERNEL
#include <sys/time.h>			/* For structs itimerval, timeval. */
#else
#include <sys/pcpu.h>
#endif
#include <sys/ucontext.h>
#include <sys/ucred.h>
#include <machine/proc.h>		/* Machine-dependent proc substruct. */

/*
 * One structure allocated per session.
 *
 * List of locks
 * (m)		locked by s_mtx mtx
 * (e)		locked by proctree_lock sx
 * (c)		const until freeing
 */
struct session {
	int		s_count;	/* (m) Ref cnt; pgrps in session. */
	struct proc	*s_leader;	/* (m + e) Session leader. */
	struct vnode	*s_ttyvp;	/* (m) Vnode of controlling tty. */
	struct tty	*s_ttyp;	/* (m) Controlling tty. */
	pid_t		s_sid;		/* (c) Session ID. */
					/* (m) Setlogin() name: */
	char		s_login[roundup(MAXLOGNAME, sizeof(long))];
	struct mtx	s_mtx;		/* Mutex to protect members. */
};

/*
 * One structure allocated per process group.
 *
 * List of locks
 * (m)		locked by pg_mtx mtx
 * (e)		locked by proctree_lock sx
 * (c)		const until freeing
 */
struct pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* (e) Hash chain. */
	LIST_HEAD(, proc) pg_members;	/* (m + e) Pointer to pgrp members. */
	struct session	*pg_session;	/* (c) Pointer to session. */
	struct sigiolst	pg_sigiolst;	/* (m) List of sigio sources. */
	pid_t		pg_id;		/* (c) Process group id. */
	int		pg_jobc;	/* (m) Job control process count. */
	struct mtx	pg_mtx;		/* Mutex to protect members */
};

/*
 * pargs, used to hold a copy of the command line, if it had a sane length.
 */
struct pargs {
	u_int	ar_ref;		/* Reference count. */
	u_int	ar_length;	/* Length. */
	u_char	ar_args[1];	/* Arguments. */
};

/*-
 * Description of a process.
 *
 * This structure contains the information needed to manage a thread of
 * control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.  The process structure and the substructures
 * are always addressable except for those marked "(CPU)" below,
 * which might be addressable only on a processor on which the process
 * is running.
 *
 * Below is a key of locks used to protect each member of struct proc.  The
 * lock is indicated by a reference to a specific character in parens in the
 * associated comment.
 *      * - not yet protected
 *      a - only touched by curproc or parent during fork/wait
 *      b - created at fork, never changes
 *		(exception aiods switch vmspaces, but they are also
 *		marked 'P_SYSTEM' so hopefully it will be left alone)
 *      c - locked by proc mtx
 *      d - locked by allproc_lock lock
 *      e - locked by proctree_lock lock
 *      f - session mtx
 *      g - process group mtx
 *      h - callout_lock mtx
 *      i - by curproc or the master session mtx
 *      j - locked by proc slock
 *      k - only accessed by curthread
 *	k*- only accessed by curthread and from an interrupt
 *      l - the attaching proc or attaching proc parent
 *      m - Giant
 *      n - not locked, lazy
 *      o - ktrace lock
 *      p - select lock (sellock)
 *      q - td_contested lock
 *      r - p_peers lock
 *      t - thread lock
 *      x - created at fork, only changes during single threading in exec
 *      z - zombie threads lock
 *
 * If the locking key specifies two identifiers (for example, p_pptr) then
 * either lock is sufficient for read access, but both locks must be held
 * for write access.
 */
struct kaudit_record;
struct td_sched;
struct nlminfo;
struct kaioinfo;
struct p_sched;
struct proc;
struct sleepqueue;
struct thread;
struct trapframe;
struct turnstile;
struct mqueue_notifier;

/*
 * Here we define the two structures used for process information.
 *
 * The first is the thread. It might be thought of as a "Kernel
 * Schedulable Entity Context".
 * This structure contains all the information as to where a thread of
 * execution is now, or was when it was suspended, why it was suspended,
 * and anything else that will be needed to restart it when it is
 * rescheduled. It includes a scheduler specific substructure that is different
 * for each scheduler.
 *
 * M:N notes.
 * It is important to remember that when using M:N threading, 
 * a particular thread structure may only exist as long as
 * the system call or kernel entrance (e.g. by pagefault)
 * which it is currently executing. It should therefore NEVER be referenced
 * by pointers in long lived structures that live longer than a single
 * request. If several threads complete their work at the same time,
 * they will all rewind their stacks to the user boundary, report their
 * completion state, and all but one will be freed. That last one will
 * be kept to provide a kernel stack and pcb for the NEXT syscall or kernel
 * entrance (basically to save freeing and then re-allocating it).  The existing
 * thread keeps a cached spare thread available to allow it to quickly
 * get one when it needs a new one. There is also a system
 * cache of free threads. Threads have priority and partake in priority
 * inheritance schemes.
 *
 * The second is the proc (process) which owns all the resources of a process
 * other than CPU cycles, which are parceled out to the threads.
 */

/*
 * Kernel runnable context (thread).
 * This is what is put to sleep and reactivated.
 * Thread context.  Processes may have multiple threads.
 */
struct thread {
	struct mtx	*volatile td_lock; /* replaces sched lock */
	struct proc	*td_proc;	/* (*) Associated process. */
	TAILQ_ENTRY(thread) td_plist;	/* (*) All threads in this proc. */

	/* The two queues below should someday be merged. */
	TAILQ_ENTRY(thread) td_slpq;	/* (t) Sleep queue. */
	TAILQ_ENTRY(thread) td_lockq;	/* (t) Lock queue. */

	TAILQ_HEAD(, selinfo) td_selq;	/* (p) List of selinfos. */
	struct sleepqueue *td_sleepqueue; /* (k) Associated sleep queue. */
	struct turnstile *td_turnstile;	/* (k) Associated turnstile. */
	struct umtx_q   *td_umtxq;	/* (c?) Link for when we're blocked. */
	lwpid_t		td_tid;		/* (b) Thread ID. */
	sigqueue_t	td_sigqueue;	/* (c) Sigs arrived, not delivered. */
#define	td_siglist	td_sigqueue.sq_signals

/* Cleared during fork1() or thread_schedule_upcall(). */
#define	td_startzero td_flags
	int		td_flags;	/* (t) TDF_* flags. */
	int		td_inhibitors;	/* (t) Why can not run. */
	int		td_pflags;	/* (k) Private thread (TDP_*) flags. */
	int		td_dupfd;	/* (k) Ret value from fdopen. XXX */
	int		td_sqqueue;	/* (t) Sleepqueue queue blocked on. */
	void		*td_wchan;	/* (t) Sleep address. */
	const char	*td_wmesg;	/* (t) Reason for sleep. */
	u_char		td_lastcpu;	/* (t) Last cpu we were on. */
	u_char		td_oncpu;	/* (t) Which cpu we are on. */
	volatile u_char td_owepreempt;  /* (k*) Preempt on last critical_exit */
	short		td_locks;	/* (k) Count of non-spin locks. */
	u_char		td_tsqueue;	/* (t) Turnstile queue blocked on. */
	struct turnstile *td_blocked;	/* (t) Lock thread is blocked on. */
	const char	*td_lockname;	/* (t) Name of lock blocked on. */
	LIST_HEAD(, turnstile) td_contested;	/* (q) Contested locks. */
	struct lock_list_entry *td_sleeplocks; /* (k) Held sleep locks. */
	int		td_intr_nesting_level; /* (k) Interrupt recursion. */
	int		td_pinned;	/* (k) Temporary cpu pin count. */
	struct kse_thr_mailbox *td_mailbox; /* (*) Userland mailbox address. */
	struct ucred	*td_ucred;	/* (k) Reference to credentials. */
	struct thread	*td_standin;	/* (k + a) Use this for an upcall. */
	struct kse_upcall *td_upcall;	/* (k + t) Upcall structure. */
	u_int		td_estcpu;	/* (t) estimated cpu utilization */
	u_int		td_slptime;	/* (t) How long completely blocked. */
	struct rusage	td_ru;		/* (t) rusage information */
	uint64_t	td_runtime;	/* (t) How many cpu ticks we've run. */
	u_int 		td_pticks;	/* (t) Statclock hits for profiling */
	u_int		td_sticks;	/* (t) Statclock hits in system mode. */
	u_int		td_iticks;	/* (t) Statclock hits in intr mode. */
	u_int		td_uticks;	/* (t) Statclock hits in user mode. */
	u_int		td_uuticks;	/* (k) Statclock hits (usr), for UTS. */
	u_int		td_usticks;	/* (k) Statclock hits (sys), for UTS. */
	int		td_intrval;	/* (t) Return value of TDF_INTERRUPT. */
	sigset_t	td_oldsigmask;	/* (k) Saved mask from pre sigpause. */
	sigset_t	td_sigmask;	/* (c) Current signal mask. */
	volatile u_int	td_generation;	/* (k) For detection of preemption */
	stack_t		td_sigstk;	/* (k) Stack ptr and on-stack flag. */
	int		td_kflags;	/* (c) Flags for KSE threading. */
	int		td_xsig;	/* (c) Signal for ptrace */
	u_long		td_profil_addr;	/* (k) Temporary addr until AST. */
	u_int		td_profil_ticks; /* (k) Temporary ticks until AST. */
	char		td_name[MAXCOMLEN + 1];	/* (*) Thread name. */
#define	td_endzero td_base_pri

/* Copied during fork1() or thread_sched_upcall(). */
#define	td_startcopy td_endzero
	u_char		td_base_pri;	/* (t) Thread base kernel priority. */
	u_char		td_priority;	/* (t) Thread active priority. */
	u_char		td_pri_class;	/* (t) Scheduling class. */
	u_char		td_user_pri;	/* (t) User pri from estcpu and nice. */
	u_char		td_base_user_pri; /* (t) Base user pri */
#define	td_endcopy td_pcb

/*
 * Fields that must be manually set in fork1() or thread_sched_upcall()
 * or already have been set in the allocator, constructor, etc.
 */
	struct pcb	*td_pcb;	/* (k) Kernel VA of pcb and kstack. */
	enum {
		TDS_INACTIVE = 0x0,
		TDS_INHIBITED,
		TDS_CAN_RUN,
		TDS_RUNQ,
		TDS_RUNNING
	} td_state;			/* (t) thread state */
	register_t	td_retval[2];	/* (k) Syscall aux returns. */
	struct callout	td_slpcallout;	/* (h) Callout for sleep. */
	struct trapframe *td_frame;	/* (k) */
	struct vm_object *td_kstack_obj;/* (a) Kstack object. */
	vm_offset_t	td_kstack;	/* (a) Kernel VA of kstack. */
	int		td_kstack_pages; /* (a) Size of the kstack. */
	struct vm_object *td_altkstack_obj;/* (a) Alternate kstack object. */
	vm_offset_t	td_altkstack;	/* (a) Kernel VA of alternate kstack. */
	int		td_altkstack_pages; /* (a) Size of alternate kstack. */
	volatile u_int	td_critnest;	/* (k*) Critical section nest level. */
	struct mdthread td_md;		/* (k) Any machine-dependent fields. */
	struct td_sched	*td_sched;	/* (*) Scheduler-specific data. */
	struct kaudit_record	*td_ar;	/* (k) Active audit record, if any. */
	int		td_syscalls;	/* per-thread syscall count (used by NFS :)) */
};

struct mtx *thread_lock_block(struct thread *);
void thread_lock_unblock(struct thread *, struct mtx *);
void thread_lock_set(struct thread *, struct mtx *);
#define	THREAD_LOCK_ASSERT(td, type)					\
do {									\
	struct mtx *__m = (td)->td_lock;				\
	if (__m != &blocked_lock)					\
		mtx_assert(__m, (type));				\
} while (0)

/*
 * Flags kept in td_flags:
 * To change these you MUST have the scheduler lock.
 */
#define	TDF_BORROWING	0x00000001 /* Thread is borrowing pri from another. */
#define	TDF_INPANIC	0x00000002 /* Caused a panic, let it drive crashdump. */
#define	TDF_SINTR	0x00000008 /* Sleep is interruptible. */
#define	TDF_TIMEOUT	0x00000010 /* Timing out during sleep. */
#define	TDF_IDLETD	0x00000020 /* This is a per-CPU idle thread. */
#define	TDF_SELECT	0x00000040 /* Selecting; wakeup/waiting danger. */
#define	TDF_SLEEPABORT	0x00000080 /* sleepq_abort was called. */
#define	TDF_UNUSEDx100	0x00000100 /* --available-- */
#define	TDF_UBORROWING	0x00000200 /* Thread is borrowing user pri. */
#define	TDF_BOUNDARY	0x00000400 /* Thread suspended at user boundary */
#define	TDF_ASTPENDING	0x00000800 /* Thread has some asynchronous events. */
#define	TDF_TIMOFAIL	0x00001000 /* Timeout from sleep after we were awake. */
#define	TDF_INTERRUPT	0x00002000 /* Thread is marked as interrupted. */
#define	TDF_UPIBLOCKED	0x00004000 /* Thread blocked on user PI mutex. */
#define	TDF_UNUSED15	0x00008000 /* --available-- */
#define	TDF_NEEDRESCHED	0x00010000 /* Thread needs to yield. */
#define	TDF_NEEDSIGCHK	0x00020000 /* Thread may need signal delivery. */
#define	TDF_XSIG	0x00040000 /* Thread is exchanging signal under trace */
#define	TDF_UNUSED19	0x00080000 /* Thread is sleeping on a umtx. */
#define	TDF_THRWAKEUP	0x00100000 /* Libthr thread must not suspend itself. */
#define	TDF_DBSUSPEND	0x00200000 /* Thread is suspended by debugger */
#define	TDF_UNUSED22	0x00400000 /* --available-- */
#define	TDF_UNUSED23	0x00800000 /* --available-- */
#define	TDF_SCHED0	0x01000000 /* Reserved for scheduler private use */
#define	TDF_SCHED1	0x02000000 /* Reserved for scheduler private use */
#define	TDF_SCHED2	0x04000000 /* Reserved for scheduler private use */
#define	TDF_SCHED3	0x08000000 /* Reserved for scheduler private use */

/*
 * "Private" flags kept in td_pflags:
 * These are only accessed by curthread and thus need no locking.
 */
#define	TDP_OLDMASK	0x00000001 /* Need to restore mask after suspend. */
#define	TDP_INKTR	0x00000002 /* Thread is currently in KTR code. */
#define	TDP_INKTRACE	0x00000004 /* Thread is currently in KTRACE code. */
#define	TDP_UPCALLING	0x00000008 /* This thread is doing an upcall. */
#define	TDP_COWINPROGRESS 0x00000010 /* Snapshot copy-on-write in progress. */
#define	TDP_ALTSTACK	0x00000020 /* Have alternate signal stack. */
#define	TDP_DEADLKTREAT	0x00000040 /* Lock aquisition - deadlock treatment. */
#define	TDP_SA		0x00000080 /* A scheduler activation based thread. */
#define	TDP_NOSLEEPING	0x00000100 /* Thread is not allowed to sleep on a sq. */
#define	TDP_OWEUPC	0x00000200 /* Call addupc() at next AST. */
#define	TDP_ITHREAD	0x00000400 /* Thread is an interrupt thread. */
#define	TDP_CAN_UNBIND	0x00000800 /* Only temporarily bound. */
#define	TDP_SCHED1	0x00001000 /* Reserved for scheduler private use */
#define	TDP_SCHED2	0x00002000 /* Reserved for scheduler private use */
#define	TDP_SCHED3	0x00004000 /* Reserved for scheduler private use */
#define	TDP_SCHED4	0x00008000 /* Reserved for scheduler private use */
#define	TDP_GEOM	0x00010000 /* Settle GEOM before finishing syscall */
#define	TDP_SOFTDEP	0x00020000 /* Stuck processing softdep worklist */
#define	TDP_NORUNNINGBUF 0x00040000 /* Ignore runningbufspace check */
#define	TDP_WAKEUP	0x00080000 /* Don't sleep in umtx cond_wait */
#define	TDP_INBDFLUSH	0x00100000 /* Already in BO_BDFLUSH, do not recurse */

/*
 * Reasons that the current thread can not be run yet.
 * More than one may apply.
 */
#define	TDI_SUSPENDED	0x0001	/* On suspension queue. */
#define	TDI_SLEEPING	0x0002	/* Actually asleep! (tricky). */
#define	TDI_SWAPPED	0x0004	/* Stack not in mem.  Bad juju if run. */
#define	TDI_LOCK	0x0008	/* Stopped on a lock. */
#define	TDI_IWAIT	0x0010	/* Awaiting interrupt. */

/*
 * flags (in kflags) related to M:N threading.
 */
#define	TDK_KSEREL	0x0001	/* Blocked in msleep on p->p_completed. */
#define	TDK_KSERELSIG	0x0002	/* Blocked in msleep on p->p_siglist. */
#define	TDK_WAKEUP	0x0004	/* Thread has been woken by kse_wakeup. */

#define	TD_CAN_UNBIND(td)			\
    (((td)->td_pflags & TDP_CAN_UNBIND) &&	\
     ((td)->td_upcall != NULL))

#define	TD_IS_SLEEPING(td)	((td)->td_inhibitors & TDI_SLEEPING)
#define	TD_ON_SLEEPQ(td)	((td)->td_wchan != NULL)
#define	TD_IS_SUSPENDED(td)	((td)->td_inhibitors & TDI_SUSPENDED)
#define	TD_IS_SWAPPED(td)	((td)->td_inhibitors & TDI_SWAPPED)
#define	TD_ON_LOCK(td)		((td)->td_inhibitors & TDI_LOCK)
#define	TD_AWAITING_INTR(td)	((td)->td_inhibitors & TDI_IWAIT)
#define	TD_IS_RUNNING(td)	((td)->td_state == TDS_RUNNING)
#define	TD_ON_RUNQ(td)		((td)->td_state == TDS_RUNQ)
#define	TD_CAN_RUN(td)		((td)->td_state == TDS_CAN_RUN)
#define	TD_IS_INHIBITED(td)	((td)->td_state == TDS_INHIBITED)
#define	TD_ON_UPILOCK(td)	((td)->td_flags & TDF_UPIBLOCKED)
#if 0
#define TD_IS_IDLETHREAD(td)	((td) == pcpu(idlethread))
#else
#define TD_IS_IDLETHREAD(td)	((td)->td_flags & TDF_IDLETD)
#endif


#define	TD_SET_INHIB(td, inhib) do {			\
	(td)->td_state = TDS_INHIBITED;			\
	(td)->td_inhibitors |= (inhib);			\
} while (0)

#define	TD_CLR_INHIB(td, inhib) do {			\
	if (((td)->td_inhibitors & (inhib)) &&		\
	    (((td)->td_inhibitors &= ~(inhib)) == 0))	\
		(td)->td_state = TDS_CAN_RUN;		\
} while (0)

#define	TD_SET_SLEEPING(td)	TD_SET_INHIB((td), TDI_SLEEPING)
#define	TD_SET_SWAPPED(td)	TD_SET_INHIB((td), TDI_SWAPPED)
#define	TD_SET_LOCK(td)		TD_SET_INHIB((td), TDI_LOCK)
#define	TD_SET_SUSPENDED(td)	TD_SET_INHIB((td), TDI_SUSPENDED)
#define	TD_SET_IWAIT(td)	TD_SET_INHIB((td), TDI_IWAIT)
#define	TD_SET_EXITING(td)	TD_SET_INHIB((td), TDI_EXITING)

#define	TD_CLR_SLEEPING(td)	TD_CLR_INHIB((td), TDI_SLEEPING)
#define	TD_CLR_SWAPPED(td)	TD_CLR_INHIB((td), TDI_SWAPPED)
#define	TD_CLR_LOCK(td)		TD_CLR_INHIB((td), TDI_LOCK)
#define	TD_CLR_SUSPENDED(td)	TD_CLR_INHIB((td), TDI_SUSPENDED)
#define	TD_CLR_IWAIT(td)	TD_CLR_INHIB((td), TDI_IWAIT)

#define	TD_SET_RUNNING(td)	(td)->td_state = TDS_RUNNING
#define	TD_SET_RUNQ(td)		(td)->td_state = TDS_RUNQ
#define	TD_SET_CAN_RUN(td)	(td)->td_state = TDS_CAN_RUN

/*
 * An upcall is used when returning to userland.  If a thread does not have
 * an upcall on return to userland the thread exports its context and exits.
 */
struct kse_upcall {
	TAILQ_ENTRY(kse_upcall) ku_link;	/* List of upcalls in proc. */
	struct proc		*ku_proc;	/* Associated proc. */
	struct thread		*ku_owner;	/* Owning thread. */
	int			ku_flags;	/* KUF_* flags. */
	struct kse_mailbox	*ku_mailbox;	/* Userland mailbox address. */
	stack_t			ku_stack;	/* Userland upcall stack. */
	void			*ku_func;	/* Userland upcall function. */
	unsigned int		ku_mflags;	/* Cached upcall mbox flags. */
};

#define	KUF_DOUPCALL	0x00001		/* Do upcall now; don't wait. */
#define	KUF_EXITING	0x00002		/* Upcall structure is exiting. */

/*
 * XXX: Does this belong in resource.h or resourcevar.h instead?
 * Resource usage extension.  The times in rusage structs in the kernel are
 * never up to date.  The actual times are kept as runtimes and tick counts
 * (with control info in the "previous" times), and are converted when
 * userland asks for rusage info.  Backwards compatibility prevents putting
 * this directly in the user-visible rusage struct.
 *
 * Locking: (cj) means (j) for p_rux and (c) for p_crux.
 */
struct rusage_ext {
	u_int64_t	rux_runtime;    /* (cj) Real time. */
	u_int64_t	rux_uticks;     /* (cj) Statclock hits in user mode. */
	u_int64_t	rux_sticks;     /* (cj) Statclock hits in sys mode. */
	u_int64_t	rux_iticks;     /* (cj) Statclock hits in intr mode. */
	u_int64_t	rux_uu;         /* (c) Previous user time in usec. */
	u_int64_t	rux_su;         /* (c) Previous sys time in usec. */
	u_int64_t	rux_tu;         /* (c) Previous total time in usec. */
};

/*
 * The old fashionned process. May have multiple threads.
 *  Starts off with a single embedded THREAD.
 */
struct proc {
	LIST_ENTRY(proc) p_list;	/* (d) List of all processes. */
	TAILQ_HEAD(, thread) p_threads;	/* (j) all threads. */
	struct mtx	p_slock;	/* process spin lock */
	struct ucred	*p_ucred;	/* (c) Process owner's identity. */
	struct filedesc	*p_fd;		/* (b) Open files. */
	struct filedesc_to_leader *p_fdtol; /* (b) Tracking node */
					/* Accumulated stats for all threads? */
	struct pstats	*p_stats;	/* (b) Accounting/statistics (CPU). */
	struct plimit	*p_limit;	/* (c) Process limits. */
	struct callout	p_limco;	/* (c) Limit callout handle */
	struct sigacts	*p_sigacts;	/* (x) Signal actions, state (CPU). */
	TAILQ_HEAD(, kse_upcall) p_upcalls; /* (j) All upcalls in the proc. */

	/*
	 * The following don't make too much sense.
	 * See the td_ or ke_ versions of the same flags.
	 */
	int		p_flag;		/* (c) P_* flags. */
	int		p_sflag;	/* (j) PS_* flags. */
	enum {
		PRS_NEW = 0,		/* In creation */
		PRS_NORMAL,		/* threads can be run. */
		PRS_ZOMBIE
	} p_state;			/* (j/c) S* process status. */
	pid_t		p_pid;		/* (b) Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* (d) Hash chain. */
	LIST_ENTRY(proc) p_pglist;	/* (g + e) List of processes in pgrp. */
	struct proc	*p_pptr;	/* (c + e) Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* (e) List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* (e) Pointer to list of children. */
	struct mtx	p_mtx;		/* (n) Lock for this struct. */
	struct ksiginfo *p_ksi;	/* Locked by parent proc lock */
	sigqueue_t	p_sigqueue;	/* (c) Sigs not delivered to a td. */
#define p_siglist	p_sigqueue.sq_signals

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid
	pid_t		p_oppid;	/* (c + e) Save ppid in ptrace. XXX */
	struct vmspace	*p_vmspace;	/* (b) Address space. */
	u_int		p_swtime;	/* (j) Time swapped in or out. */
	struct itimerval p_realtimer;	/* (c) Alarm timer. */
	struct rusage_ext p_rux;	/* (cj) Internal resource usage. */
	struct rusage_ext p_crux;	/* (c) Internal child resource usage. */
	int		p_profthreads;	/* (c) Num threads in addupc_task. */
	int		p_maxthrwaits;	/* (c) Max threads num waiters */
	int		p_traceflag;	/* (o) Kernel trace points. */
	struct vnode	*p_tracevp;	/* (c + o) Trace to vnode. */
	struct ucred	*p_tracecred;	/* (o) Credentials to trace with. */
	struct vnode	*p_textvp;	/* (b) Vnode of executable. */
	char		p_lock;		/* (c) Proclock (prevent swap) count. */
	struct sigiolst	p_sigiolst;	/* (c) List of sigio sources. */
	int		p_sigparent;	/* (c) Signal to parent on exit. */
	int		p_sig;		/* (n) For core dump/debugger XXX. */
	u_long		p_code;		/* (n) For core dump/debugger XXX. */
	u_int		p_stops;	/* (c) Stop event bitmask. */
	u_int		p_stype;	/* (c) Stop event type. */
	char		p_step;		/* (c) Process is stopped. */
	u_char		p_pfsflags;	/* (c) Procfs flags. */
	struct nlminfo	*p_nlminfo;	/* (?) Only used by/for lockd. */
	struct kaioinfo	*p_aioinfo;	/* (c) ASYNC I/O info. */
	struct thread	*p_singlethread;/* (c + j) If single threading this is it */
	int		p_suspcount;	/* (j) Num threads in suspended mode. */
	struct thread	*p_xthread;	/* (c) Trap thread */
	int		p_boundary_count;/* (c) Num threads at user boundary */
	int		p_pendingcnt;	/* how many signals are pending */
	struct itimers	*p_itimers;	/* (c) POSIX interval timers. */
/* from ksegrp */
	int		p_numupcalls;	/* (j) Num upcalls. */
	int		p_upsleeps;	/* (c) Num threads in kse_release(). */
	struct kse_thr_mailbox *p_completed; /* (c) Completed thread mboxes. */
	int		p_nextupcall;	/* (n) Next upcall time. */
	int		p_upquantum;	/* (n) Quantum to schedule an upcall. */
/* End area that is zeroed on creation. */
#define	p_endzero	p_magic

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_endzero
	u_int		p_magic;	/* (b) Magic number. */
	char		p_comm[MAXCOMLEN + 1];	/* (b) Process name. */
	struct pgrp	*p_pgrp;	/* (c + e) Pointer to process group. */
	struct sysentvec *p_sysent;	/* (b) Syscall dispatch info. */
	struct pargs	*p_args;	/* (c) Process arguments. */
	rlim_t		p_cpulimit;	/* (c) Current CPU limit in seconds. */
	signed char	p_nice;		/* (c + j) Process "nice" value. */
/* End area that is copied on creation. */
#define	p_endcopy	p_xstat

	u_short		p_xstat;	/* (c) Exit status; also stop sig. */
	struct knlist	p_klist;	/* (c) Knotes attached to this proc. */
	int		p_numthreads;	/* (j) Number of threads. */
	struct mdproc	p_md;		/* Any machine-dependent fields. */
	struct callout	p_itcallout;	/* (h + c) Interval timer callout. */
	u_short		p_acflag;	/* (c) Accounting flags. */
	struct rusage	*p_ru;		/* (a) Exit information. */
	struct proc	*p_peers;	/* (r) */
	struct proc	*p_leader;	/* (b) */
	void		*p_emuldata;	/* (c) Emulator state data. */
	struct label	*p_label;	/* (*) Proc (not subject) MAC label. */
	struct p_sched	*p_sched;	/* (*) Scheduler-specific data. */
	STAILQ_HEAD(, ktr_request)	p_ktr;	/* (o) KTR event queue. */
	LIST_HEAD(, mqueue_notifier)	p_mqnotifier; /* (c) mqueue notifiers.*/
};

#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

#define	NOCPU	0xff		/* For when we aren't on a CPU. */

#define	PROC_SLOCK(p)	mtx_lock_spin(&(p)->p_slock)
#define	PROC_SUNLOCK(p)	mtx_unlock_spin(&(p)->p_slock)
#define	PROC_SLOCK_ASSERT(p, type)	mtx_assert(&(p)->p_slock, (type))

/* These flags are kept in p_flag. */
#define	P_ADVLOCK	0x00001	/* Process may hold a POSIX advisory lock. */
#define	P_CONTROLT	0x00002	/* Has a controlling terminal. */
#define	P_KTHREAD	0x00004	/* Kernel thread (*). */
#define	P_NOLOAD	0x00008	/* Ignore during load avg calculations. */
#define	P_PPWAIT	0x00010	/* Parent is waiting for child to exec/exit. */
#define	P_PROFIL	0x00020	/* Has started profiling. */
#define	P_STOPPROF	0x00040	/* Has thread requesting to stop profiling. */
#define	P_HADTHREADS	0x00080	/* Has had threads (no cleanup shortcuts) */
#define	P_SUGID		0x00100	/* Had set id privileges since last exec. */
#define	P_SYSTEM	0x00200	/* System proc: no sigs, stats or swapping. */
#define	P_SINGLE_EXIT	0x00400	/* Threads suspending should exit, not wait. */
#define	P_TRACED	0x00800	/* Debugged process being traced. */
#define	P_WAITED	0x01000	/* Someone is waiting for us. */
#define	P_WEXIT		0x02000	/* Working on exiting. */
#define	P_EXEC		0x04000	/* Process called exec. */
#define	P_SA		0x08000	/* Using scheduler activations. */
#define	P_CONTINUED	0x10000	/* Proc has continued from a stopped state. */
#define	P_STOPPED_SIG	0x20000	/* Stopped due to SIGSTOP/SIGTSTP. */
#define	P_STOPPED_TRACE	0x40000	/* Stopped because of tracing. */
#define	P_STOPPED_SINGLE 0x80000 /* Only 1 thread can continue (not to user). */
#define	P_PROTECTED	0x100000 /* Do not kill on memory overcommit. */
#define	P_SIGEVENT	0x200000 /* Process pending signals changed. */
#define	P_SINGLE_BOUNDARY 0x400000 /* Threads should suspend at user boundary. */
#define	P_HWPMC		0x800000 /* Process is using HWPMCs */

#define	P_JAILED	0x1000000 /* Process is in jail. */
#define	P_INEXEC	0x4000000 /* Process is in execve(). */
#define	P_STATCHILD	0x8000000 /* Child process stopped or exited. */

#define	P_STOPPED	(P_STOPPED_SIG|P_STOPPED_SINGLE|P_STOPPED_TRACE)
#define	P_SHOULDSTOP(p)	((p)->p_flag & P_STOPPED)

/* These flags are kept in p_sflag and are protected with proc slock. */
#define	PS_INMEM	0x00001	/* Loaded into memory. */
#define	PS_ALRMPEND	0x00020	/* Pending SIGVTALRM needs to be posted. */
#define	PS_PROFPEND	0x00040	/* Pending SIGPROF needs to be posted. */
#define	PS_SWAPINREQ	0x00100	/* Swapin request due to wakeup. */
#define	PS_SWAPPINGOUT	0x00200	/* Process is being swapped out. */
#define	PS_SWAPPINGIN	0x04000	/* Process is being swapped in. */
#define	PS_MACPEND	0x08000	/* AST-based MAC event pending. */

/*
 * These were process status values (p_stat), now they are only used in
 * legacy conversion code.
 */
#define	SIDL	1		/* Process being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Process debugging or suspension. */
#define	SZOMB	5		/* Awaiting collection by parent. */
#define	SWAIT	6		/* Waiting for interrupt. */
#define	SLOCK	7		/* Blocked on a lock. */

#define	P_MAGIC		0xbeefface

#ifdef _KERNEL

/* Flags for mi_switch(). */
#define	SW_VOL		0x0001		/* Voluntary switch. */
#define	SW_INVOL	0x0002		/* Involuntary switch. */
#define SW_PREEMPT	0x0004		/* The invol switch is a preemption */

/* How values for thread_single(). */
#define	SINGLE_NO_EXIT	0
#define	SINGLE_EXIT	1
#define	SINGLE_BOUNDARY	2

/* XXXKSE: Missing values for thread_suspend_check(). */

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PARGS);
MALLOC_DECLARE(M_PGRP);
MALLOC_DECLARE(M_SESSION);
MALLOC_DECLARE(M_SUBPROC);
MALLOC_DECLARE(M_ZOMBIE);
#endif

#define	FOREACH_PROC_IN_SYSTEM(p)					\
	LIST_FOREACH((p), &allproc, p_list)
#define	FOREACH_THREAD_IN_PROC(p, td)					\
	TAILQ_FOREACH((td), &(p)->p_threads, td_plist)
#define	FOREACH_UPCALL_IN_PROC(p, ku)					\
	TAILQ_FOREACH((ku), &(p)->p_upcalls, ku_link)

/* XXXKSE the following lines should probably only be used in 1:1 code: */
#define	FIRST_THREAD_IN_PROC(p)	TAILQ_FIRST(&(p)->p_threads)

/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 */
#define	PID_MAX		99999
#define	NO_PID		100000

#define	SESS_LEADER(p)	((p)->p_session->s_leader == (p))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s)	sessrele(s)


#define	STOPEVENT(p, e, v) do {						\
	if ((p)->p_stops & (e))	{					\
		PROC_LOCK(p);						\
		stopevent((p), (e), (v));				\
		PROC_UNLOCK(p);						\
	}								\
} while (0)
#define	_STOPEVENT(p, e, v) do {					\
	PROC_LOCK_ASSERT(p, MA_OWNED);					\
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, &p->p_mtx.lock_object, \
 	    "checking stopevent %d", (e));				\
	if ((p)->p_stops & (e))						\
		stopevent((p), (e), (v));				\
} while (0)

/* Lock and unlock a process. */
#define	PROC_LOCK(p)	mtx_lock(&(p)->p_mtx)
#define	PROC_TRYLOCK(p)	mtx_trylock(&(p)->p_mtx)
#define	PROC_UNLOCK(p)	mtx_unlock(&(p)->p_mtx)
#define	PROC_LOCKED(p)	mtx_owned(&(p)->p_mtx)
#define	PROC_LOCK_ASSERT(p, type)	mtx_assert(&(p)->p_mtx, (type))

/* Lock and unlock a process group. */
#define	PGRP_LOCK(pg)	mtx_lock(&(pg)->pg_mtx)
#define	PGRP_UNLOCK(pg)	mtx_unlock(&(pg)->pg_mtx)
#define	PGRP_LOCKED(pg)	mtx_owned(&(pg)->pg_mtx)
#define	PGRP_LOCK_ASSERT(pg, type)	mtx_assert(&(pg)->pg_mtx, (type))

#define	PGRP_LOCK_PGSIGNAL(pg) do {					\
	if ((pg) != NULL)						\
		PGRP_LOCK(pg);						\
} while (0)
#define	PGRP_UNLOCK_PGSIGNAL(pg) do {					\
	if ((pg) != NULL)						\
		PGRP_UNLOCK(pg);					\
} while (0)

/* Lock and unlock a session. */
#define	SESS_LOCK(s)	mtx_lock(&(s)->s_mtx)
#define	SESS_UNLOCK(s)	mtx_unlock(&(s)->s_mtx)
#define	SESS_LOCKED(s)	mtx_owned(&(s)->s_mtx)
#define	SESS_LOCK_ASSERT(s, type)	mtx_assert(&(s)->s_mtx, (type))

/* Hold process U-area in memory, normally for ptrace/procfs work. */
#define	PHOLD(p) do {							\
	PROC_LOCK(p);							\
	_PHOLD(p);							\
	PROC_UNLOCK(p);							\
} while (0)
#define	_PHOLD(p) do {							\
	PROC_LOCK_ASSERT((p), MA_OWNED);				\
	KASSERT(!((p)->p_flag & P_WEXIT) || (p) == curproc,		\
	    ("PHOLD of exiting process"));				\
	(p)->p_lock++;							\
	if (((p)->p_sflag & PS_INMEM) == 0)				\
		faultin((p));						\
} while (0)
#define PROC_ASSERT_HELD(p) do {					\
	KASSERT((p)->p_lock > 0, ("process not held"));			\
} while (0)

#define	PRELE(p) do {							\
	PROC_LOCK((p));							\
	_PRELE((p));							\
	PROC_UNLOCK((p));						\
} while (0)
#define	_PRELE(p) do {							\
	PROC_LOCK_ASSERT((p), MA_OWNED);				\
	(--(p)->p_lock);						\
	if (((p)->p_flag & P_WEXIT) && (p)->p_lock == 0)		\
		wakeup(&(p)->p_lock);					\
} while (0)
#define PROC_ASSERT_NOT_HELD(p) do {					\
	KASSERT((p)->p_lock == 0, ("process held"));			\
} while (0)

/* Check whether a thread is safe to be swapped out. */
#define	thread_safetoswapout(td) (TD_IS_SLEEPING(td) || TD_IS_SUSPENDED(td))

/* Control whether or not it is safe for curthread to sleep. */
#define	THREAD_NO_SLEEPING() do {					\
	KASSERT(!(curthread->td_pflags & TDP_NOSLEEPING),		\
	    ("nested no sleeping"));					\
	curthread->td_pflags |= TDP_NOSLEEPING;				\
} while (0)

#define	THREAD_SLEEPING_OK() do {					\
	KASSERT((curthread->td_pflags & TDP_NOSLEEPING),		\
	    ("nested sleeping ok"));					\
	curthread->td_pflags &= ~TDP_NOSLEEPING;			\
} while (0)

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

extern struct sx allproc_lock;
extern struct sx proctree_lock;
extern struct mtx ppeers_lock;
extern struct proc proc0;		/* Process slot for swapper. */
extern struct thread thread0;		/* Primary thread in proc0. */
extern struct vmspace vmspace0;		/* VM space for proc0. */
extern int hogticks;			/* Limit on kernel cpu hogs. */
extern int lastpid;
extern int nprocs, maxproc;		/* Current and max number of procs. */
extern int maxprocperuid;		/* Max procs per uid. */
extern u_long ps_arg_cache_limit;

LIST_HEAD(proclist, proc);
TAILQ_HEAD(procqueue, proc);
TAILQ_HEAD(threadqueue, thread);
extern struct proclist allproc;		/* List of all processes. */
extern struct proclist zombproc;	/* List of zombie processes. */
extern struct proc *initproc, *pageproc; /* Process slots for init, pager. */

extern struct uma_zone *proc_zone;

struct	proc *pfind(pid_t);		/* Find process by id. */
struct	pgrp *pgfind(pid_t);		/* Find process group by id. */
struct	proc *zpfind(pid_t);		/* Find zombie process by id. */

void	ast(struct trapframe *framep);
struct	thread *choosethread(void);
int	cr_cansignal(struct ucred *cred, struct proc *proc, int signum);
int	enterpgrp(struct proc *p, pid_t pgid, struct pgrp *pgrp,
	    struct session *sess);
int	enterthispgrp(struct proc *p, struct pgrp *pgrp);
void	faultin(struct proc *p);
void	fixjobc(struct proc *p, struct pgrp *pgrp, int entering);
int	fork1(struct thread *, int, int, struct proc **);
void	fork_exit(void (*)(void *, struct trapframe *), void *,
	    struct trapframe *);
void	fork_return(struct thread *, struct trapframe *);
int	inferior(struct proc *p);
void 	kick_proc0(void);
int	leavepgrp(struct proc *p);
int	maybe_preempt(struct thread *td);
void	mi_switch(int flags, struct thread *newtd);
int	p_candebug(struct thread *td, struct proc *p);
int	p_cansee(struct thread *td, struct proc *p);
int	p_cansched(struct thread *td, struct proc *p);
int	p_cansignal(struct thread *td, struct proc *p, int signum);
int	p_canwait(struct thread *td, struct proc *p);
struct	pargs *pargs_alloc(int len);
void	pargs_drop(struct pargs *pa);
void	pargs_free(struct pargs *pa);
void	pargs_hold(struct pargs *pa);
void	procinit(void);
void	proc_linkup(struct proc *p, struct thread *td);
void	proc_reparent(struct proc *child, struct proc *newparent);
struct	pstats *pstats_alloc(void);
void	pstats_fork(struct pstats *src, struct pstats *dst);
void	pstats_free(struct pstats *ps);
int	securelevel_ge(struct ucred *cr, int level);
int	securelevel_gt(struct ucred *cr, int level);
void	sessrele(struct session *);
void	setrunnable(struct thread *);
void	setsugid(struct proc *p);
int	sigonstack(size_t sp);
void	sleepinit(void);
void	stopevent(struct proc *, u_int, u_int);
void	threadinit(void);
void	cpu_idle(void);
extern	void (*cpu_idle_hook)(void);	/* Hook to machdep CPU idler. */
void	cpu_switch(struct thread *, struct thread *, struct mtx *);
void	cpu_throw(struct thread *, struct thread *) __dead2;
void	unsleep(struct thread *);
void	userret(struct thread *, struct trapframe *);

void	cpu_exit(struct thread *);
void	exit1(struct thread *, int) __dead2;
void	cpu_fork(struct thread *, struct proc *, struct thread *, int);
void	cpu_set_fork_handler(struct thread *, void (*)(void *), void *);

/* New in KSE. */
void	kse_unlink(struct thread *);
void	kse_GC(void);
void	kseinit(void);
void	cpu_set_upcall(struct thread *td, struct thread *td0);
void	cpu_set_upcall_kse(struct thread *, void (*)(void *), void *, stack_t *);
int	cpu_set_user_tls(struct thread *, void *tls_base);
void	cpu_thread_clean(struct thread *);
void	cpu_thread_exit(struct thread *);
void	cpu_thread_setup(struct thread *td);
void	cpu_thread_swapin(struct thread *);
void	cpu_thread_swapout(struct thread *);
struct	thread *thread_alloc(void);
void	thread_continued(struct proc *p);
void	thread_exit(void) __dead2;
int	thread_export_context(struct thread *td, int willexit);
void	thread_free(struct thread *td);
void	thread_link(struct thread *td, struct proc *p);
void	thread_reap(void);
struct thread *thread_schedule_upcall(struct thread *td, struct kse_upcall *ku);
void	thread_signal_add(struct thread *td, ksiginfo_t *);
int	thread_single(int how);
void	thread_single_end(void);
void	thread_stash(struct thread *td);
int	thread_statclock(int user);
void	thread_stopped(struct proc *p);
void	childproc_stopped(struct proc *child, int reason);
void	childproc_continued(struct proc *child);
void	childproc_exited(struct proc *child);
int	thread_suspend_check(int how);
void	thread_suspend_switch(struct thread *);
void	thread_suspend_one(struct thread *td);
struct thread *thread_switchout(struct thread *td, int flags,
	    struct thread *newtd);
void	thread_unlink(struct thread *td);
void	thread_unsuspend(struct proc *p);
void	thread_unsuspend_one(struct thread *td);
void	thread_unthread(struct thread *td);
int	thread_userret(struct thread *td, struct trapframe *frame);
void	thread_user_enter(struct thread *td);
void	thread_wait(struct proc *p);
struct thread	*thread_find(struct proc *p, lwpid_t tid);
void	thr_exit1(void);
struct kse_upcall *upcall_alloc(void);
void	upcall_free(struct kse_upcall *ku);
void	upcall_link(struct kse_upcall *ku, struct proc *p);
void	upcall_unlink(struct kse_upcall *ku);
void	upcall_remove(struct thread *td);
void	upcall_stash(struct kse_upcall *ke);

#endif	/* _KERNEL */

#endif	/* !_SYS_PROC_H_ */
