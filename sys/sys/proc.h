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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
#include <sys/filedesc.h>
#include <sys/queue.h>
#include <sys/priority.h>
#include <sys/rtprio.h>			/* XXX */
#include <sys/runq.h>
#include <sys/signal.h>
#include <sys/_label.h>
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
	struct	proc	*s_leader;	/* (m + e) Session leader. */
	struct	vnode	*s_ttyvp;	/* (m) Vnode of controlling tty. */
	struct	tty	*s_ttyp;	/* (m) Controlling tty. */
	pid_t		s_sid;		/* (c) Session ID. */
					/* (m) Setlogin() name: */
	char		s_login[roundup(MAXLOGNAME, sizeof(long))];
	struct	mtx	s_mtx;		/* Mutex to protect members */
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
	pid_t		pg_id;		/* (c) Pgrp id. */
	int		pg_jobc;	/* (m) job cntl proc count */
	struct	mtx	pg_mtx;		/*  Mutex to protect members */
};

struct procsig {
	sigset_t ps_sigignore;		/* Signals being ignored. */
	sigset_t ps_sigcatch;		/* Signals being caught by user. */
	int	 ps_flag;
	struct	 sigacts *ps_sigacts;	/* Signal actions, state. */
	int	 ps_refcnt;
};

#define	PS_NOCLDWAIT	0x0001	/* No zombies if child dies */
#define	PS_NOCLDSTOP	0x0002	/* No SIGCHLD when children stop. */
#define	PS_CLDSIGIGN	0x0004	/* The SIGCHLD handler is SIG_IGN. */

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
 *      	(exception aiods switch vmspaces, but they are also
 *      	marked 'P_SYSTEM' so hopefully it will be left alone)
 *      c - locked by proc mtx
 *      d - locked by allproc_lock lock
 *      e - locked by proctree_lock lock
 *      f - session mtx
 *      g - process group mtx
 *      h - callout_lock mtx
 *      i - by curproc or the master session mtx
 *      j - locked by sched_lock mtx
 *      k - only accessed by curthread
 *      l - the attaching proc or attaching proc parent
 *      m - Giant
 *      n - not locked, lazy
 *      o - ktrace lock
 *      p - select lock (sellock)
 *      r - p_peers lock
 *
 * If the locking key specifies two identifiers (for example, p_pptr) then
 * either lock is sufficient for read access, but both locks must be held
 * for write access.
 */
struct ithd;
struct ke_sched;
struct kg_sched;
struct nlminfo;
struct p_sched;
struct td_sched;
struct trapframe;

/*
 * Here we define the four structures used for process information.
 *
 * The first is the thread. It might be though of as a "Kernel
 * Schedulable Entity Context".
 * This structure contains all the information as to where a thread of 
 * execution is now, or was when it was suspended, why it was suspended,
 * and anything else that will be needed to restart it when it is
 * rescheduled. Always associated with a KSE when running, but can be
 * reassigned to an equivalent KSE  when being restarted for
 * load balancing. Each of these is associated with a kernel stack
 * and a pcb.
 * 
 * It is important to remember that a particular thread structure only
 * exists as long as the system call or kernel entrance (e.g. by pagefault)
 * which it is currently executing. It should threfore NEVER be referenced
 * by pointers in long lived structures that live longer than a single
 * request. If several threads complete their work at the same time,
 * they will all rewind their stacks to the user boundary, report their
 * completion state, and all but one will be freed. That last one will
 * be kept to provide a kernel stack and pcb for the NEXT syscall or kernel
 * entrance. (basically to save freeing and then re-allocating it) The KSE
 * keeps a cached thread available to allow it to quickly
 * get one when it needs a new one. There is also a system
 * cache of free threads. Threads have priority and partake in priority
 * inherritance schemes.
 */
struct thread;

/* 
 * The second structure is the Kernel Schedulable Entity. (KSE)
 * It represents the ability to take a slot in the scheduler queue.
 * As long as this is scheduled, it could continue to run any threads that
 * are assigned to the KSEGRP (see later) until either it runs out
 * of runnable threads of high enough priority, or CPU.
 * It runs on one CPU and is assigned a quantum of time. When a thread is
 * blocked, The KSE continues to run and will search for another thread
 * in a runnable state amongst those it has. It May decide to return to user
 * mode with a new 'empty' thread if there are no runnable threads.
 * Threads are temporarily associated with a KSE for scheduling reasons.
 */
struct kse;

/*
 * The KSEGRP is allocated resources across a number of CPUs.
 * (Including a number of CPUxQUANTA. It parcels these QUANTA up among
 * Its KSEs, each of which should be running in a different CPU.
 * BASE priority and total available quanta are properties of a KSEGRP.
 * Multiple KSEGRPs in a single process compete against each other
 * for total quanta in the same way that a forked child competes against
 * it's parent process.
 */
struct ksegrp;

/*
 * A process is the owner of all system resources allocated to a task
 * except CPU quanta.
 * All KSEGs under one process see, and have the same access to, these
 * resources (e.g. files, memory, sockets, permissions kqueues).
 * A process may compete for CPU cycles on the same basis as a
 * forked process cluster by spawning several KSEGRPs. 
 */
struct proc;

/***************
 * In pictures:
 With a single run queue used by all processors:

 RUNQ: --->KSE---KSE--...               SLEEPQ:[]---THREAD---THREAD---THREAD
	   |   /                               []---THREAD
	   KSEG---THREAD--THREAD--THREAD       []
	                                       []---THREAD---THREAD

  (processors run THREADs from the KSEG until they are exhausted or
  the KSEG exhausts its quantum) 

With PER-CPU run queues:
KSEs on the separate run queues directly
They would be given priorities calculated from the KSEG.

 *
 *****************/

/*
 * Kernel runnable context (thread).
 * This is what is put to sleep and reactivated.
 * The first KSE available in the correct group will run this thread.
 * If several are available, use the one on the same CPU as last time.
 * When waing to be run, threads are hung off the KSEGRP in priority order.
 * with N runnable and queued KSEs in the KSEGRP, the first N threads
 * are linked to them. Other threads are not yet assigned.
 */
struct thread {
	struct proc	*td_proc;	/* Associated process. */
	struct ksegrp	*td_ksegrp;	/* Associated KSEG. */
	TAILQ_ENTRY(thread) td_plist;	/* All threads in this proc */
	TAILQ_ENTRY(thread) td_kglist;	/* All threads in this ksegrp */

	/* The two queues below should someday be merged */
	TAILQ_ENTRY(thread) td_slpq; 	/* (j) Sleep queue. XXXKSE */ 
	TAILQ_ENTRY(thread) td_lockq; 	/* (j) Lock queue. XXXKSE */ 
	TAILQ_ENTRY(thread) td_runq; 	/* (j) Run queue(s). XXXKSE */ 

	TAILQ_HEAD(, selinfo) td_selq;	/* (p) List of selinfos. */

/* Cleared during fork1() or thread_sched_upcall() */
#define	td_startzero td_flags
	int		td_flags;	/* (j) TDF_* flags. */
	int		td_inhibitors;	/* (j) Why can not run */
	struct kse	*td_last_kse;	/* (j) Previous value of td_kse */
	struct kse	*td_kse;	/* (j) Current KSE if running. */
	int		td_dupfd;	/* (k) Ret value from fdopen. XXX */
	void		*td_wchan;	/* (j) Sleep address. */
	const char	*td_wmesg;	/* (j) Reason for sleep. */
	u_char		td_lastcpu;	/* (j) Last cpu we were on. */
	u_char		td_inktr;	/* (k) Currently handling a KTR. */
	u_char		td_inktrace;	/* (k) Currently handling a KTRACE. */
	short		td_locks;	/* (k) DEBUG: lockmgr count of locks */
	struct mtx	*td_blocked;	/* (j) Mutex process is blocked on. */
	struct ithd	*td_ithd;	/* (b) For interrupt threads only. */
	const char	*td_lockname;	/* (j) Name of lock blocked on. */
	LIST_HEAD(, mtx) td_contested;	/* (j) Contested locks. */
	struct lock_list_entry *td_sleeplocks; /* (k) Held sleep locks. */
	int		td_intr_nesting_level; /* (k) Interrupt recursion. */
	struct kse_thr_mailbox *td_mailbox; /* the userland mailbox address */
	struct ucred	*td_ucred;	/* (k) Reference to credentials. */
	void		(*td_switchin)(void); /* (k) Switchin special func. */
	struct thread	*td_standin;	/* (?) use this for an upcall */
	u_int		td_usticks;	/* Statclock hits in kernel, for UTS */
	u_int		td_critnest;	/* (k) Critical section nest level. */
#define	td_endzero td_base_pri

/* Copied during fork1() or thread_sched_upcall() */
#define	td_startcopy td_endzero
	u_char		td_base_pri;	/* (j) Thread base kernel priority. */
	u_char		td_priority;	/* (j) Thread active priority. */
#define	td_endcopy td_pcb

/*
 * fields that must be manually set in fork1() or thread_sched_upcall()
 * or already have been set in the allocator, contstructor, etc..
 */
	struct pcb	*td_pcb;	/* (k) Kernel VA of pcb and kstack. */
	enum {
		TDS_INACTIVE = 0x20,
		TDS_INHIBITED,
		TDS_CAN_RUN,
		TDS_RUNQ,
		TDS_RUNNING
	} td_state;
	register_t 	td_retval[2];	/* (k) Syscall aux returns. */
	struct callout	td_slpcallout;	/* (h) Callout for sleep. */
	struct trapframe *td_frame;	/* (k) */
	struct vm_object *td_kstack_obj;/* (a) Kstack object. */
	vm_offset_t	td_kstack;	/* Kernel VA of kstack. */
	int		td_kstack_pages; /* Size of the kstack */
	struct vm_object *td_altkstack_obj;/* (a) Alternate kstack object. */
	vm_offset_t	td_altkstack;	/* Kernel VA of alternate kstack. */
	int		td_altkstack_pages; /* Size of the alternate kstack */
	struct mdthread td_md;		/* (k) Any machine-dependent fields. */
	struct td_sched	*td_sched;	/* Scheduler specific data */
};
/* flags kept in td_flags */ 
#define	TDF_UNBOUND	0x000001 /* May give away the kse, uses the kg runq. */
#define	TDF_INPANIC	0x000002 /* Caused a panic, let it drive crashdump. */
#define	TDF_SINTR	0x000008 /* Sleep is interruptible. */
#define	TDF_TIMEOUT	0x000010 /* Timing out during sleep. */
#define	TDF_SELECT	0x000040 /* Selecting; wakeup/waiting danger. */
#define	TDF_CVWAITQ	0x000080 /* Thread is on a cv_waitq (not slpq). */
#define	TDF_UPCALLING	0x000100 /* This thread is doing an upcall. */
#define	TDF_ONSLEEPQ	0x000200 /* On the sleep queue. */
#define	TDF_INMSLEEP	0x000400 /* Don't recurse in msleep(). */
#define	TDF_TIMOFAIL	0x001000 /* Timeout from sleep after we were awake. */
#define	TDF_INTERRUPT	0x002000 /* Thread is marked as interrupted. */
#define	TDF_DEADLKTREAT	0x800000 /* Lock aquisition - deadlock treatment. */

#define	TDI_SUSPENDED	0x01	/* On suspension queue. */
#define	TDI_SLEEPING	0x02	/* Actually asleep! (tricky). */
#define	TDI_SWAPPED	0x04	/* Stack not in mem.. bad juju if run. */
#define	TDI_LOCK	0x08	/* Stopped on a lock. */
#define	TDI_IWAIT	0x10	/* Awaiting interrupt. */
#define	TDI_LOAN	0x20	/* bound thread's KSE is lent */

#define	TD_IS_SLEEPING(td)	((td)->td_inhibitors & TDI_SLEEPING)
#define	TD_ON_SLEEPQ(td)	((td)->td_wchan != NULL)
#define	TD_IS_SUSPENDED(td)	((td)->td_inhibitors & TDI_SUSPENDED)
#define	TD_IS_SWAPPED(td)	((td)->td_inhibitors & TDI_SWAPPED)
#define	TD_ON_LOCK(td)		((td)->td_inhibitors & TDI_LOCK)
#define	TD_LENT(td)		((td)->td_inhibitors & TDI_LOAN)
#define	TD_AWAITING_INTR(td)	((td)->td_inhibitors & TDI_IWAIT)
#define	TD_IS_RUNNING(td)	((td)->td_state == TDS_RUNNING)
#define	TD_ON_RUNQ(td)		((td)->td_state == TDS_RUNQ)
#define	TD_CAN_RUN(td)		((td)->td_state == TDS_CAN_RUN)
#define	TD_IS_INHIBITED(td)	((td)->td_state == TDS_INHIBITED)

#define	TD_SET_INHIB(td, inhib) do {			\
	(td)->td_state = TDS_INHIBITED;			\
	(td)->td_inhibitors |= inhib;			\
} while (0)

#define	TD_CLR_INHIB(td, inhib) do {			\
	if (((td)->td_inhibitors & inhib) &&		\
	    (((td)->td_inhibitors &= ~inhib) == 0))	\
		(td)->td_state = TDS_CAN_RUN;		\
} while (0)

#define	TD_SET_SLEEPING(td)	TD_SET_INHIB((td), TDI_SLEEPING)
#define	TD_SET_SWAPPED(td)	TD_SET_INHIB((td), TDI_SWAPPED)
#define	TD_SET_LOCK(td)		TD_SET_INHIB((td), TDI_LOCK)
#define	TD_SET_SUSPENDED(td)	TD_SET_INHIB((td), TDI_SUSPENDED)
#define	TD_SET_IWAIT(td)	TD_SET_INHIB((td), TDI_IWAIT)
#define	TD_SET_LOAN(td)		TD_SET_INHIB((td), TDI_LOAN)

#define	TD_CLR_SLEEPING(td)	TD_CLR_INHIB((td), TDI_SLEEPING)
#define	TD_CLR_SWAPPED(td)	TD_CLR_INHIB((td), TDI_SWAPPED)
#define	TD_CLR_LOCK(td)		TD_CLR_INHIB((td), TDI_LOCK)
#define	TD_CLR_SUSPENDED(td)	TD_CLR_INHIB((td), TDI_SUSPENDED)
#define	TD_CLR_IWAIT(td)	TD_CLR_INHIB((td), TDI_IWAIT)
#define	TD_CLR_LOAN(td)		TD_CLR_INHIB((td), TDI_LOAN)

#define	TD_SET_RUNNING(td)	do {(td)->td_state = TDS_RUNNING; } while (0)
#define	TD_SET_RUNQ(td)		do {(td)->td_state = TDS_RUNQ; } while (0)
#define	TD_SET_CAN_RUN(td)	do {(td)->td_state = TDS_CAN_RUN; } while (0)
#define	TD_SET_ON_SLEEPQ(td)	do {(td)->td_flags |= TDF_ONSLEEPQ; } while (0)
#define	TD_CLR_ON_SLEEPQ(td)	do {			\
		(td)->td_flags &= ~TDF_ONSLEEPQ;	\
		(td)->td_wchan = NULL;			\
} while (0)


/*
 * Traps for young players:
 * The main thread variable that controls whether a thread acts as a threaded
 * or unthreaded thread is the td_bound counter (0 == unbound).
 * UPCALLS run with the UNBOUND flags clear, after they are first scheduled.
 * i.e. they bind themselves to whatever thread thay are first scheduled with.
 * You may see BOUND threads in KSE processes but you should never see
 * UNBOUND threads in non KSE processes.
 */

/*
 * The schedulable entity that can be given a context to run.
 * A process may have several of these. Probably one per processor
 * but posibly a few more. In this universe they are grouped
 * with a KSEG that contains the priority and niceness
 * for the group.
 */
struct kse {
	struct proc	*ke_proc;	/* Associated process. */
	struct ksegrp	*ke_ksegrp;	/* Associated KSEG. */
	TAILQ_ENTRY(kse) ke_kglist;	/* Queue of all KSEs in ke_ksegrp. */
	TAILQ_ENTRY(kse) ke_kgrlist;	/* Queue of all KSEs in this state. */
	TAILQ_ENTRY(kse) ke_procq;	/* (j) Run queue. */

#define	ke_startzero ke_flags
	int		ke_flags;	/* (j) KEF_* flags. */
	struct thread	*ke_thread;	/* Active associated thread. */
	struct thread	*ke_bound;	/* Thread bound to this KSE (*) */
	int		ke_cpticks;	/* (j) Ticks of cpu time. */
	fixpt_t		ke_pctcpu;	/* (j) %cpu during p_swtime. */
	u_int64_t	ke_uu;		/* (j) Previous user time in usec. */
	u_int64_t	ke_su;		/* (j) Previous system time in usec. */
	u_int64_t	ke_iu;		/* (j) Previous intr time in usec. */
	u_int64_t	ke_uticks;	/* (j) Statclock hits in user mode. */
	u_int64_t	ke_sticks;	/* (j) Statclock hits in system mode. */
	u_int64_t	ke_iticks;	/* (j) Statclock hits in intr. */
	u_int		ke_uuticks;	/* Statclock hits in user, for UTS */
	u_int		ke_usticks;	/* Statclock hits in kernel, for UTS */
	u_char		ke_oncpu;	/* (j) Which cpu we are on. */
	char		ke_rqindex;	/* (j) Run queue index. */
	enum {
		KES_IDLE = 0x10,
		KES_ONRUNQ,
		KES_UNQUEUED,		/* in transit */
		KES_THREAD		/* slaved to thread state */
	} ke_state;			/* (j) S* process status. */
	struct kse_mailbox *ke_mailbox;	/* the userland mailbox address */
	stack_t		ke_stack;
	void		*ke_upcall;
	struct thread	*ke_tdspare;	/* spare thread for upcalls */
#define	ke_endzero ke_dummy
	u_char		ke_dummy;
	struct ke_sched	*ke_sched;	/* Scheduler specific data */
};

/* flags kept in ke_flags */
#define	KEF_OWEUPC	0x00002	/* Owe process an addupc() call at next ast. */
#define	KEF_IDLEKSE	0x00004	/* A 'Per CPU idle process'.. has one thread */
#define	KEF_LOANED	0x00008	/* On loan from the bound thread to another */
#define	KEF_USER	0x00200	/* Process is not officially in the kernel */
#define	KEF_ASTPENDING	0x00400	/* KSE has a pending ast. */
#define	KEF_NEEDRESCHED	0x00800	/* Process needs to yield. */
#define	KEF_ONLOANQ	0x01000 /* KSE is on loan queue. */
#define	KEF_DIDRUN	0x02000	/* KSE actually ran. */
#define	KEF_EXIT	0x04000	/* KSE is being killed. */

/*
 * (*) A bound KSE with a bound thread in a KSE process may be lent to
 * Other threads, as long as those threads do not leave the kernel. 
 * The other threads must be either exiting, or be unbound with a valid
 * mailbox so that they can save their state there rather than going
 * to user space. While this happens the real bound thread is still linked
 * to the kse via the ke_bound field, and the KSE has its "KEF_LOANED
 * flag set.
 */

/*
 * Kernel-scheduled entity group (KSEG).  The scheduler considers each KSEG to
 * be an indivisible unit from a time-sharing perspective, though each KSEG may
 * contain multiple KSEs.
 */
struct ksegrp {
	struct proc	*kg_proc;	/* Process that contains this KSEG. */
	TAILQ_ENTRY(ksegrp) kg_ksegrp;	/* Queue of KSEGs in kg_proc. */
	TAILQ_HEAD(, kse) kg_kseq;	/* (ke_kglist) All KSEs. */
	TAILQ_HEAD(, kse) kg_iq;	/* (ke_kgrlist) Idle KSEs. */
	TAILQ_HEAD(, kse) kg_lq;	/* (ke_kgrlist) Loan KSEs. */
	TAILQ_HEAD(, thread) kg_threads;/* (td_kglist) All threads. */
	TAILQ_HEAD(, thread) kg_runq;	/* (td_runq) waiting RUNNABLE threads */
	TAILQ_HEAD(, thread) kg_slpq;	/* (td_runq) NONRUNNABLE threads. */

#define	kg_startzero kg_estcpu
	u_int		kg_estcpu;	/* Sum of the same field in KSEs. */
 	u_int		kg_slptime;	/* (j) How long completely blocked. */
	struct thread 	*kg_last_assigned; /* Last thread assigned to a KSE */
	int		kg_runnable;	/* Num runnable threads on queue. */
	int		kg_runq_kses;	/* Num KSEs on runq. */
	int		kg_loan_kses;	/* Num KSEs on loan queue. */
	struct kse_thr_mailbox *kg_completed; /* (c) completed thread mboxes */
#define	kg_endzero kg_pri_class

#define	kg_startcopy 	kg_endzero
	u_char		kg_pri_class;	/* (j) Scheduling class. */
	u_char		kg_user_pri;	/* (j) User pri from estcpu and nice. */
	char		kg_nice;	/* (j?/k?) Process "nice" value. */
#define	kg_endcopy kg_numthreads
	int		kg_numthreads;	/* Num threads in total */
	int		kg_idle_kses;	/* num KSEs idle */
	int		kg_kses;	/* Num KSEs in group. */
	struct kg_sched	*kg_sched;	/* Scheduler specific data */
};

/*
 * The old fashionned process. May have multiple threads, KSEGRPs
 * and KSEs. Starts off with a single embedded KSEGRP, KSE and THREAD.
 */
struct proc {
	LIST_ENTRY(proc) p_list;	/* (d) List of all processes. */
	TAILQ_HEAD(, ksegrp) p_ksegrps;	/* (kg_ksegrp) All KSEGs. */
	TAILQ_HEAD(, thread) p_threads;	/* (td_plist) Threads. (shortcut) */
	TAILQ_HEAD(, thread) p_suspended; /* (td_runq) suspended threads */
	struct ucred	*p_ucred;	/* (c) Process owner's identity. */
	struct filedesc	*p_fd;		/* (b) Ptr to open files structure. */
					/* Accumulated stats for all KSEs? */
	struct pstats	*p_stats;	/* (b) Accounting/statistics (CPU). */
	struct plimit	*p_limit;	/* (m) Process limits. */
	struct vm_object *p_upages_obj; /* (a) Upages object. */
	struct procsig	*p_procsig;	/* (c) Signal actions, state (CPU). */

	/*struct ksegrp	p_ksegrp;
	struct kse	p_kse; */

	/*
	 * The following don't make too much sense..
	 * See the td_ or ke_ versions of the same flags
	 */
	int		p_flag;		/* (c) P_* flags. */
	int		p_sflag;	/* (j) PS_* flags. */
	enum {
		PRS_NEW = 0,		/* In creation */
		PRS_NORMAL,		/* KSEs can be run */
		PRS_ZOMBIE
	} p_state;			/* (j) S* process status. */
	pid_t		p_pid;		/* (b) Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* (d) Hash chain. */
	LIST_ENTRY(proc) p_pglist;	/* (g + e) List of processes in pgrp. */
	struct proc	*p_pptr;	/* (c + e) Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* (e) List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* (e) Pointer to list of children. */
	struct mtx	p_mtx;		/* (k) Lock for this struct. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid
	pid_t		p_oppid; 	/* (c + e) Save ppid in ptrace. XXX */
	struct vmspace	*p_vmspace;	/* (b) Address space. */
	u_int		p_swtime;	/* (j) Time swapped in or out. */
	struct itimerval p_realtimer;	/* (h?/k?) Alarm timer. */
	struct bintime	p_runtime;	/* (j) Real time. */
	int		p_traceflag;	/* (o) Kernel trace points. */
	struct vnode	*p_tracep;	/* (c + o) Trace to vnode. */
	sigset_t	p_siglist;	/* (c) Sigs arrived, not delivered. */
	struct vnode	*p_textvp;	/* (b) Vnode of executable. */
	char		p_lock;		/* (c) Proclock (prevent swap) count. */
	struct klist p_klist;		/* (c) Knotes attached to this proc. */
	struct sigiolst	p_sigiolst;	/* (c) List of sigio sources. */
	int		p_sigparent;	/* (c) Signal to parent on exit. */
	sigset_t	p_oldsigmask;	/* (c) Saved mask from pre sigpause. */
	int		p_sig;		/* (n) For core dump/debugger XXX. */
	u_long		p_code;		/* (n) For core dump/debugger XXX. */
	u_int		p_stops;	/* (c) Stop event bitmask. */
	u_int		p_stype;	/* (c) Stop event type. */
	char		p_step;		/* (c) Process is stopped. */
	u_char		p_pfsflags;	/* (c) Procfs flags. */
	struct nlminfo	*p_nlminfo;	/* (?) Only used by/for lockd. */
	void		*p_aioinfo;	/* (c) ASYNC I/O info. */
	struct thread	*p_singlethread;/* (j) If single threading this is it */
	int		p_suspcount;	/* (j) # threads in suspended mode */
	int		p_userthreads;	/* (j) # threads in userland */
/* End area that is zeroed on creation. */
#define	p_endzero	p_sigmask

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_endzero
	sigset_t	p_sigmask;	/* (c) Current signal mask. */
	stack_t		p_sigstk;	/* (c) Stack ptr and on-stack flag. */
	u_int		p_magic;	/* (b) Magic number. */
	char		p_comm[MAXCOMLEN + 1];	/* (b) Process name. */
	struct pgrp	*p_pgrp;	/* (c + e) Pointer to process group. */
	struct sysentvec *p_sysent;	/* (b) Syscall dispatch info. */
	struct pargs	*p_args;	/* (c) Process arguments. */
	rlim_t		p_cpulimit;	/* (j) Current CPU limit in seconds. */
/* End area that is copied on creation. */
#define	p_endcopy	p_xstat

	u_short		p_xstat;	/* (c) Exit status; also stop sig. */
	int		p_numthreads;	/* (?) number of threads */
	int		p_numksegrps;	/* (?) number of ksegrps */
	struct mdproc	p_md;		/* (c) Any machine-dependent fields. */
	struct callout	p_itcallout;	/* (h) Interval timer callout. */
	struct user	*p_uarea;	/* (k) Kernel VA of u-area (CPU) */
	u_short		p_acflag;	/* (c) Accounting flags. */
	struct rusage	*p_ru;		/* (a) Exit information. XXX */
	struct proc	*p_peers;	/* (r) */
	struct proc	*p_leader;	/* (b) */
	void		*p_emuldata;	/* (c) Emulator state data. */
	struct label	p_label;	/* process (not subject) MAC label */
	struct p_sched	*p_sched;	/* Scheduler specific data */
};

#define	p_rlimit	p_limit->pl_rlimit
#define	p_sigacts	p_procsig->ps_sigacts
#define	p_sigignore	p_procsig->ps_sigignore
#define	p_sigcatch	p_procsig->ps_sigcatch
#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

#define	NOCPU	0xff		/* For when we aren't on a CPU. (SMP) */

/* Status values (p_stat). */

/* These flags are kept in p_flag. */
#define	P_ADVLOCK	0x00001	/* Process may hold a POSIX advisory lock. */
#define	P_CONTROLT	0x00002	/* Has a controlling terminal. */
#define	P_KTHREAD	0x00004	/* Kernel thread. (*)*/
#define	P_NOLOAD	0x00008	/* Ignore during load avg calculations. */
#define	P_PPWAIT	0x00010	/* Parent is waiting for child to exec/exit. */
#define	P_SUGID		0x00100	/* Had set id privileges since last exec. */
#define	P_SYSTEM	0x00200	/* System proc: no sigs, stats or swapping. */
#define	P_WAITED	0x01000	/* Someone is waiting for us */
#define	P_WEXIT		0x02000	/* Working on exiting. */
#define	P_EXEC		0x04000	/* Process called exec. */
#define	P_KSES		0x08000	/* Process is using KSEs. */
#define	P_CONTINUED	0x10000	/* Proc has continued from a stopped state. */

/* flags that control how threads may be suspended for some reason */
#define	P_STOPPED_SIG		0x20000	/* Stopped due to SIGSTOP/SIGTSTP */
#define	P_STOPPED_TRACE		0x40000	/* Stopped because of tracing */
#define	P_STOPPED_SINGLE	0x80000	/* Only one thread can continue */
					/* (not to user) */
#define	P_SINGLE_EXIT		0x00400	/* Threads suspending should exit, */
					/* not wait */
#define	P_TRACED		0x00800	/* Debugged process being traced. */
#define	P_STOPPED		(P_STOPPED_SIG|P_STOPPED_SINGLE|P_STOPPED_TRACE)
#define	P_SHOULDSTOP(p)		((p)->p_flag & P_STOPPED)

/* Should be moved to machine-dependent areas. */
#define	P_UNUSED100000	0x100000
#define	P_COWINPROGRESS	0x400000 /* Snapshot copy-on-write in progress. */

#define	P_JAILED	0x1000000 /* Process is in jail. */
#define	P_OLDMASK	0x2000000 /* Need to restore mask after suspend. */
#define	P_ALTSTACK	0x4000000 /* Have alternate signal stack. */
#define	P_INEXEC	0x8000000 /* Process is in execve(). */

/* These flags are kept in p_sflag and are protected with sched_lock. */
#define	PS_INMEM	0x00001	/* Loaded into memory. */
#define	PS_XCPU		0x00002 /* Exceeded CPU limit. */
#define	PS_PROFIL	0x00004	/* Has started profiling. */
#define	PS_ALRMPEND	0x00020	/* Pending SIGVTALRM needs to be posted. */
#define	PS_PROFPEND	0x00040	/* Pending SIGPROF needs to be posted. */
#define	PS_SWAPINREQ	0x00100	/* Swapin request due to wakeup. */
#define	PS_SWAPPING	0x00200	/* Process is being swapped. */
#define	PS_NEEDSIGCHK	0x02000	/* Process may need signal delivery. */
#define	PS_SWAPPINGIN	0x04000	/* Swapin in progress. */
#define	PS_MACPEND	0x08000	/* Ast()-based MAC event pending. */

/* used only in legacy conversion code */
#define	SIDL	1		/* Process being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Process debugging or suspension. */
#define	SZOMB	5		/* Awaiting collection by parent. */
#define	SWAIT	6		/* Waiting for interrupt. */
#define	SLOCK	7		/* Blocked on a lock. */

#define	P_MAGIC		0xbeefface

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PARGS);
MALLOC_DECLARE(M_PGRP);
MALLOC_DECLARE(M_SESSION);
MALLOC_DECLARE(M_SUBPROC);
MALLOC_DECLARE(M_ZOMBIE);
#endif

#define	FOREACH_PROC_IN_SYSTEM(p)					\
	LIST_FOREACH((p), &allproc, p_list)
#define	FOREACH_KSEGRP_IN_PROC(p, kg)					\
	TAILQ_FOREACH((kg), &(p)->p_ksegrps, kg_ksegrp)
#define	FOREACH_THREAD_IN_GROUP(kg, td)					\
	TAILQ_FOREACH((td), &(kg)->kg_threads, td_kglist)
#define	FOREACH_KSE_IN_GROUP(kg, ke)					\
	TAILQ_FOREACH((ke), &(kg)->kg_kseq, ke_kglist)
#define	FOREACH_THREAD_IN_PROC(p, td)					\
	TAILQ_FOREACH((td), &(p)->p_threads, td_plist)

/* XXXKSE the lines below should probably only be used in 1:1 code */
#define	FIRST_THREAD_IN_PROC(p) TAILQ_FIRST(&p->p_threads)
#define	FIRST_KSEGRP_IN_PROC(p) TAILQ_FIRST(&p->p_ksegrps)
#define	FIRST_KSE_IN_KSEGRP(kg) TAILQ_FIRST(&kg->kg_kseq)
#define	FIRST_KSE_IN_PROC(p) FIRST_KSE_IN_KSEGRP(FIRST_KSEGRP_IN_PROC(p))

static __inline int
sigonstack(size_t sp)
{
	register struct thread *td = curthread;
	struct proc *p = td->td_proc;

	return ((p->p_flag & P_ALTSTACK) ?
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	    ((p->p_sigstk.ss_size == 0) ? (p->p_sigstk.ss_flags & SS_ONSTACK) :
		((sp - (size_t)p->p_sigstk.ss_sp) < p->p_sigstk.ss_size))
#else
	    ((sp - (size_t)p->p_sigstk.ss_sp) < p->p_sigstk.ss_size)
#endif
	    : 0);
}

/* Handy macro to determine if p1 can mangle p2. */
#define	PRISON_CHECK(p1, p2) \
	((p1)->p_prison == NULL || (p1)->p_prison == (p2)->p_prison)

/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 */
#define	PID_MAX		99999
#define	NO_PID		100000

#define	SESS_LEADER(p)	((p)->p_session->s_leader == (p))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s) {							\
	if (--(s)->s_count == 0)					\
		FREE(s, M_SESSION);					\
}

#define	STOPEVENT(p, e, v) do {						\
	PROC_LOCK(p);							\
	_STOPEVENT((p), (e), (v));					\
	PROC_UNLOCK(p);							\
} while (0)
#define	_STOPEVENT(p, e, v) do {					\
	PROC_LOCK_ASSERT(p, MA_OWNED);					\
	if ((p)->p_stops & (e)) {					\
		stopevent((p), (e), (v));				\
	}								\
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

#define	PGRP_LOCK_PGSIGNAL(pg)						\
	do {								\
		if ((pg) != NULL)					\
			PGRP_LOCK(pg);					\
	} while (0);

#define	PGRP_UNLOCK_PGSIGNAL(pg)					\
	do {								\
		if ((pg) != NULL)					\
			PGRP_UNLOCK(pg);				\
	} while (0);

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
	if ((p)->p_lock++ == 0) {					\
		mtx_lock_spin(&sched_lock);				\
		faultin((p));						\
		mtx_unlock_spin(&sched_lock);				\
	}								\
} while (0)

#define	PRELE(p) do {							\
	PROC_LOCK((p));							\
	_PRELE((p));							\
	PROC_UNLOCK((p));						\
} while (0)
#define	_PRELE(p) do {							\
	PROC_LOCK_ASSERT((p), MA_OWNED);				\
	(--(p)->p_lock);						\
} while (0)

/* Check whether a thread is safe to be swapped out. */
#define	thread_safetoswapout(td) (TD_IS_SLEEPING(td) || TD_IS_SUSPENDED(td))

/* Lock and unlock process arguments. */
#define	PARGS_LOCK(p)		mtx_lock(&pargs_ref_lock)
#define	PARGS_UNLOCK(p)		mtx_unlock(&pargs_ref_lock)

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

extern struct sx allproc_lock;
extern struct sx proctree_lock;
extern struct mtx pargs_ref_lock;
extern struct mtx ppeers_lock;
extern struct proc proc0;		/* Process slot for swapper. */
extern struct thread thread0;		/* Primary thread in proc0 */
extern struct ksegrp ksegrp0;		/* Primary ksegrp in proc0 */
extern struct kse kse0;			/* Primary kse in proc0 */
extern int hogticks;			/* Limit on kernel cpu hogs. */
extern int nprocs, maxproc;		/* Current and max number of procs. */
extern int maxprocperuid;		/* Max procs per uid. */
extern u_long ps_arg_cache_limit;
extern int ps_argsopen;
extern int ps_showallprocs;
extern int sched_quantum;		/* Scheduling quantum in ticks. */

LIST_HEAD(proclist, proc);
TAILQ_HEAD(procqueue, proc);
TAILQ_HEAD(threadqueue, thread);
extern struct proclist allproc;		/* List of all processes. */
extern struct proclist zombproc;	/* List of zombie processes. */
extern struct proc *initproc, *pageproc; /* Process slots for init, pager. */
extern struct proc *updateproc;		/* Process slot for syncer (sic). */

extern struct uma_zone *proc_zone;

extern int lastpid;

struct	proc *pfind(pid_t);	/* Find process by id. */
struct	pgrp *pgfind(pid_t);	/* Find process group by id. */
struct	proc *zpfind(pid_t);	/* Find zombie process by id. */

void	adjustrunqueue(struct thread *, int newpri);
void	ast(struct trapframe *framep);
struct	thread *choosethread(void);
int	cr_cansignal(struct ucred *cred, struct proc *proc, int signum);
int	enterpgrp(struct proc *p, pid_t pgid, struct pgrp *pgrp, struct session *sess);
int	enterthispgrp(struct proc *p, struct pgrp *pgrp);
void	faultin(struct proc *p);
void	fixjobc(struct proc *p, struct pgrp *pgrp, int entering);
int	fork1(struct thread *, int, int, struct proc **);
void	fork_exit(void (*)(void *, struct trapframe *), void *,
	    struct trapframe *);
void	fork_return(struct thread *, struct trapframe *);
int	inferior(struct proc *p);
int	leavepgrp(struct proc *p);
void	mi_switch(void);
int	p_candebug(struct thread *td, struct proc *p);
int	p_cansee(struct thread *td, struct proc *p);
int	p_cansched(struct thread *td, struct proc *p);
int	p_cansignal(struct thread *td, struct proc *p, int signum);
struct	pargs *pargs_alloc(int len);
void	pargs_drop(struct pargs *pa);
void	pargs_free(struct pargs *pa);
void	pargs_hold(struct pargs *pa);
void	procinit(void);
void	threadinit(void);
void	proc_linkup(struct proc *p, struct ksegrp *kg,
	    struct kse *ke, struct thread *td);
void	proc_reparent(struct proc *child, struct proc *newparent);
int	securelevel_ge(struct ucred *cr, int level);
int	securelevel_gt(struct ucred *cr, int level);
void	setrunnable(struct thread *);
void	setrunqueue(struct thread *);
void	setsugid(struct proc *p);
void	sleepinit(void);
void	stopevent(struct proc *, u_int, u_int);
void	cpu_idle(void);
void	cpu_switch(void);
void	cpu_throw(void) __dead2;
void	unsleep(struct thread *);
void	userret(struct thread *, struct trapframe *, u_int);

void	cpu_exit(struct thread *);
void	cpu_sched_exit(struct thread *);
void	exit1(struct thread *, int) __dead2;
void	cpu_fork(struct thread *, struct proc *, struct thread *, int);
void	cpu_set_fork_handler(struct thread *, void (*)(void *), void *);
void	cpu_wait(struct proc *);

/* New in KSE. */
struct	ksegrp *ksegrp_alloc(void);
void	ksegrp_free(struct ksegrp *kg);
void	ksegrp_stash(struct ksegrp *kg);
struct	kse *kse_alloc(void);
void	kse_free(struct kse *ke);
void	kse_stash(struct kse *ke);
void	cpu_set_upcall(struct thread *td, void *pcb);
void	cpu_set_upcall_kse(struct thread *td, struct kse *ke);
void	cpu_thread_exit(struct thread *);
void	cpu_thread_setup(struct thread *td);
void	kse_reassign(struct kse *ke);
void	kse_link(struct kse *ke, struct ksegrp *kg);
void	kse_unlink(struct kse *ke);
void	ksegrp_link(struct ksegrp *kg, struct proc *p);
void	ksegrp_unlink(struct ksegrp *kg);
void	make_kse_runnable(struct kse *ke);
struct thread *signal_upcall(struct proc *p, int sig);
struct	thread *thread_alloc(void);
void	thread_exit(void) __dead2;
int	thread_export_context(struct thread *td);
void	thread_free(struct thread *td);
void	thread_getcontext(struct thread *td, ucontext_t *uc);
void	thread_link(struct thread *td, struct ksegrp *kg);
void	thread_reap(void);
struct thread *thread_schedule_upcall(struct thread *td, struct kse *ke);
int	thread_setcontext(struct thread *td, ucontext_t *uc);
int	thread_single(int how);
#define	SINGLE_NO_EXIT 0			/* values for 'how' */
#define	SINGLE_EXIT 1
void	thread_single_end(void);
void	thread_stash(struct thread *td);
int	thread_suspend_check(int how);
void	thread_suspend_one(struct thread *td);
void	thread_unsuspend(struct proc *p);
void	thread_unsuspend_one(struct thread *td);
int	thread_userret(struct thread *td, struct trapframe *frame);
void	thread_user_enter(struct proc *p, struct thread *td);
int	thread_add_ticks_intr(int user, uint ticks);

void	thread_sanity_check(struct thread *td, char *);
#endif	/* _KERNEL */

#endif	/* !_SYS_PROC_H_ */
