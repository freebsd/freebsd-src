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
#ifndef _KERNEL
#include <sys/time.h>			/* For structs itimerval, timeval. */
#endif
#include <sys/ucred.h>
#include <machine/proc.h>		/* Machine-dependent proc substruct. */

/*
 * One structure allocated per session.
 */
struct session {
	int		s_count;	/* Ref cnt; pgrps in session. */
	struct proc	*s_leader;	/* Session leader. */
	struct vnode	*s_ttyvp;	/* Vnode of controlling terminal. */
	struct tty	*s_ttyp;	/* Controlling terminal. */
	pid_t		s_sid;		/* Session ID. */
					/* Setlogin() name: */
	char		s_login[roundup(MAXLOGNAME, sizeof(long))];
};

/*
 * One structure allocated per process group.
 */
struct pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* Hash chain. */
	LIST_HEAD(, proc) pg_members;	/* Pointer to pgrp members. */
	struct session	*pg_session;	/* Pointer to session. */
	struct  sigiolst pg_sigiolst;	/* List of sigio sources. */
	pid_t		pg_id;		/* Pgrp id. */
	int		pg_jobc;	/* # procs qualifying pgrp for job control */
};

struct procsig {
	sigset_t ps_sigignore;	/* Signals being ignored. */
	sigset_t ps_sigcatch;	/* Signals being caught by user. */
	int	 ps_flag;
	struct	 sigacts *ps_sigacts;	/* Signal actions, state. */
	int	 ps_refcnt;
};

#define	PS_NOCLDWAIT	0x0001	/* No zombies if child dies */
#define	PS_NOCLDSTOP	0x0002	/* No SIGCHLD when children stop. */

/*
 * pargs, used to hold a copy of the command line, if it had a sane length.
 */
struct pargs {
	u_int	ar_ref;		/* Reference count. */
	u_int	ar_length;	/* Length. */
	u_char	ar_args[0];	/* Arguments. */
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
 *
 * If the locking key specifies two identifiers (for example, p_pptr) then
 * either lock is sufficient for read access, but both locks must be held
 * for write access.
 */
struct ithd;
struct nlminfo;
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
 * they will all rewind their stacks to the uer boundary, report their
 * completion state, and all but one will be freed. That last one will
 * be kept to provide a kernel stack and pcb for the NEXT syscall or kernel
 * entrance. (basically to save freeing and then re-allocating it) A process
 * might keep a cache of threads available to allow it to quickly
 * get one when it needs a new one. There would probably also be a system
 * cache of free threads.
 */
struct thread;

/* 
 * The second structure is the Kernel Schedulable Entity. (KSE)
 * As long as this is scheduled, it will continue to run any threads that
 * are assigned to it or the KSEGRP (see later) until either it runs out
 * of runnable threads or CPU.
 * It runs on one CPU and is assigned a quantum of time. When a thread is
 * blocked, The KSE continues to run and will search for another thread
 * in a runnable state amongst those it has. It May decide to return to user
 * mode with a new 'empty' thread if there are no runnable threads.
 * threads are associated with a KSE for cache reasons, but a sheduled KSE with
 * no runnable thread will try take a thread from a sibling KSE before
 * surrendering its quantum. In some schemes it gets it's quantum from the KSEG
 * and contributes to draining that quantum, along withthe other KSEs in
 * the group. (undecided)
 */
struct kse;

/*
 * The KSEGRP is allocated resources across a number of CPUs.
 * (Including a number of CPUxQUANTA. It parcels these QUANTA up among
 * Its KSEs, each of which should be running in a different CPU.
 * Priority and total available sheduled quanta are properties of a KSEGRP.
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
 */
struct thread {
	struct proc	*td_proc;	/* Associated process. */
	struct ksegrp	*td_ksegrp;	/* Associated KSEG. */
	struct kse	*td_last_kse;	/* Where it wants to be if possible. */
	struct kse	*td_kse;	/* Current KSE if running. */
	TAILQ_ENTRY(thread) td_plist;	/* All threads in this proc */
	TAILQ_ENTRY(thread) td_kglist;	/* All threads in this ksegrp */

	/* The two queues below should someday be merged */
	TAILQ_ENTRY(thread) td_slpq; 	/* (j) Sleep queue. XXXKSE */ 
	TAILQ_ENTRY(thread) td_blkq; 	/* (j) Mutex queue. XXXKSE */ 
	TAILQ_ENTRY(thread) td_runq; 	/* (j) Run queue(s). XXXKSE */ 

#define	td_startzero td_flags
	int		td_flags;	/* (j) TDF_* flags. */
	int		td_dupfd;	/* (k) Ret value from fdopen. XXX */
	void		*td_wchan;	/* (j) Sleep address. */
	const char	*td_wmesg;	/* (j) Reason for sleep. */
	u_char		td_lastcpu;	/* (j) Last cpu we were on. */
	short		td_locks;	/* (k) DEBUG: lockmgr count of locks */
	struct mtx	*td_blocked;	/* (j) Mutex process is blocked on. */
	struct ithd	*td_ithd;	/* (b) For interrupt threads only. */
	const char	*td_mtxname;	/* (j) Name of mutex blocked on. */
	LIST_HEAD(, mtx) td_contested;	/* (j) Contested locks. */
	struct lock_list_entry *td_sleeplocks; /* (k) Held sleep locks. */
	int		td_intr_nesting_level; /* (k) Interrupt recursion. */
#define	td_endzero td_md

#define	td_startcopy td_endzero
	/* XXXKSE p_md is in the "on your own" section in old struct proc */
	struct mdthread td_md;		/* (k) Any machine-dependent fields. */
	register_t 	td_retval[2];	/* (k) Syscall aux returns. */
#define	td_endcopy td_pcb

	struct ucred	*td_ucred;	/* (k) Reference to credentials. */
	struct pcb	*td_pcb;	/* (k) Kernel VA of pcb and kstack. */
	struct callout	td_slpcallout;	/* (h) Callout for sleep. */
	struct trapframe *td_frame;	/* (k) */
	struct vm_object *td_kstack_obj;/* (a) Kstack object. */
	vm_offset_t	td_kstack;	/* Kernel VA of kstack. */
};

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
	struct thread	*ke_thread;	/* Associated thread, if running. */
	TAILQ_ENTRY(kse) ke_kglist;	/* Queue of all KSEs in ke_ksegrp. */
	TAILQ_ENTRY(kse) ke_kgrlist;	/* Queue of all KSEs in this state. */
	TAILQ_ENTRY(kse) ke_procq;	/* (j) Run queue. */
	TAILQ_HEAD(, thread) ke_runq;	/* (td_runq) RUNNABLE bound to KSE. */

#define	ke_startzero ke_flags
	int		ke_flags;	/* (j) KEF_* flags. */
	/*u_int		ke_estcpu; */	/* (j) Time averaged val of cpticks. */
	int		ke_cpticks;	/* (j) Ticks of cpu time. */
	fixpt_t		ke_pctcpu;	/* (j) %cpu during p_swtime. */
	u_int64_t	ke_uu;		/* (j) Previous user time in usec. */
	u_int64_t	ke_su;		/* (j) Previous system time in usec. */
	u_int64_t	ke_iu;		/* (j) Previous intr time in usec. */
	u_int64_t	ke_uticks;	/* (j) Statclock hits in user mode. */
	u_int64_t	ke_sticks;	/* (j) Statclock hits in system mode. */
	u_int64_t	ke_iticks;	/* (j) Statclock hits in intr. */
	u_char		ke_oncpu;	/* (j) Which cpu we are on. */
	u_int		ke_slptime;	/* (j) Time since last idle. */
	char		ke_rqindex;	/* (j) Run queue index. */
#define	ke_endzero ke_priority

#define	ke_startcopy ke_endzero
	u_char		ke_priority;	/* (j) Process priority. */
	u_char		ke_usrpri;	/* (j) User pri from cpu & nice. */
#define	ke_endcopy ke_end

	int		ke_end;		/* dummy entry */
};

/*
 * Kernel-scheduled entity group (KSEG).  The scheduler considers each KSEG to
 * be an indivisible unit from a time-sharing perspective, though each KSEG may
 * contain multiple KSEs.
 */
struct ksegrp {
	struct proc	*kg_proc;	/* Process that contains this KSEG. */
	TAILQ_ENTRY(ksegrp) kg_ksegrp;	/* Queue of KSEGs in kg_proc. */
	TAILQ_HEAD(, kse) kg_kseq;	/* (ke_kglist) All KSEs. */
	TAILQ_HEAD(, kse) kg_rq;	/* (ke_kgrlist) Runnable KSEs. */
	TAILQ_HEAD(, kse) kg_iq;	/* (ke_kgrlist) Idle KSEs. */
	TAILQ_HEAD(, thread) kg_threads;/* (td_kglist) All threads. */
	TAILQ_HEAD(, thread) kg_runq;	/* (td_runq) Unbound RUNNABLE threads */
	TAILQ_HEAD(, thread) kg_slpq;	/* (td_runq) NONRUNNABLE threads. */

#define	kg_startzero kg_estcpu
 	u_int		kg_slptime;	/* (j) How long completely blocked. */
	u_int		kg_estcpu;	/* Sum of the same field in KSEs. */
#define	kg_endzero kg_pri

#define	kg_startcopy 	kg_endzero
	struct priority	kg_pri;		/* (j) Process priority. */
	char		kg_nice;	/* (j?/k?) Process "nice" value. */
	struct rtprio	kg_rtprio;	/* (j) Realtime priority. */
#define	kg_endcopy kg_runnable

	int		kg_runnable;	/* Num runnable threads on queue. */
	int		kg_runq_kses;	/* Num KSEs on runq. */
	int		kg_kses;	/* Num KSEs in group. */
};

/*
 * The old fashionned process. May have multiple threads, KSEGRPs
 * and KSEs. Starts off with a single embedded KSEGRP, KSE and THREAD.
 */
struct proc {
	LIST_ENTRY(proc) p_list;	/* (d) List of all processes. */
	TAILQ_HEAD(, ksegrp) p_ksegrps;	/* (kg_ksegrp) All KSEGs. */
	TAILQ_HEAD(, thread) p_threads;	/* (td_plist) Threads. (shortcut) */
	struct ucred	*p_ucred;	/* (c) Process owner's identity. */
	struct filedesc	*p_fd;		/* (b) Ptr to open files structure. */
					/* Accumulated stats for all KSEs? */
	struct pstats	*p_stats;	/* (b) Accounting/statistics (CPU). */
	struct plimit	*p_limit;	/* (m) Process limits. */
	struct vm_object *p_upages_obj; /* (a) Upages object. */
	struct procsig	*p_procsig;	/* (c) Signal actions, state (CPU). */
 
	struct ksegrp	p_ksegrp;
	struct kse	p_kse;
	struct thread	p_thread;

	/*
	 * The following don't make too much sense..
	 * See the td_ or ke_ versions of the same flags
	 */
	int		p_flag;		/* (c) P_* flags. */
	int		p_sflag;	/* (j) PS_* flags. */
	int		p_stat;		/* (j) S* process status. */

	pid_t		p_pid;		/* (b) Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* (d) Hash chain. */
	LIST_ENTRY(proc) p_pglist;	/* (c) List of processes in pgrp. */
	struct proc	*p_pptr;	/* (c + e) Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* (e) List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* (e) Pointer to list of children. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid
	pid_t		p_oppid; 	/* (c + e) Save ppid in ptrace. XXX */
	struct vmspace	*p_vmspace;	/* (b) Address space. */
	u_int		p_swtime;	/* (j) Time swapped in or out. */
	struct itimerval p_realtimer;	/* (h?/k?) Alarm timer. */
	u_int64_t	p_runtime;	/* (j) Real time in microsec. */
	int		p_traceflag;	/* (j?) Kernel trace points. */
	struct vnode	*p_tracep;	/* (j?) Trace to vnode. */
	sigset_t	p_siglist;	/* (c) Sigs arrived, not delivered. */
	struct vnode	*p_textvp;	/* (b) Vnode of executable. */
	struct mtx	p_mtx;		/* (k) Lock for this struct. */
	char		p_lock;		/* (c) Proclock (prevent swap) count. */
	struct klist	p_klist;	/* (c) Knotes attached to this proc. */
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
/* End area that is zeroed on creation. */
#define	p_startcopy	p_sigmask

/* The following fields are all copied upon creation in fork. */
#define	p_endzero	p_startcopy
	sigset_t	p_sigmask;	/* (c) Current signal mask. */
	stack_t		p_sigstk;	/* (c) Stack ptr and on-stack flag. */
	int		p_magic;	/* (b) Magic number. */
	char		p_comm[MAXCOMLEN + 1];	/* (b) Process name. */
	struct pgrp	*p_pgrp;	/* (e?/c?) Pointer to process group. */
	struct sysentvec *p_sysent;	/* (b) Syscall dispatch info. */
	struct pargs	*p_args;	/* (c) Process arguments. */
/* End area that is copied on creation. */
#define	p_endcopy	p_xstat

	u_short		p_xstat;	/* (c) Exit status; also stop sig. */
	struct mdproc	p_md;		/* (c) Any machine-dependent fields. */
	struct callout	p_itcallout;	/* (h) Interval timer callout. */
	struct user	*p_uarea;	/* (k) Kernel VA of u-area (CPU) */
	u_short		p_acflag;	/* (c) Accounting flags. */
	struct rusage	*p_ru;		/* (a) Exit information. XXX */
	struct proc	*p_peers;	/* (c) */
	struct proc	*p_leader;	/* (c) */
	void		*p_emuldata;	/* (c) Emulator state data. */
};

#define	p_rlimit	p_limit->pl_rlimit
#define	p_sigacts	p_procsig->ps_sigacts
#define	p_sigignore	p_procsig->ps_sigignore
#define	p_sigcatch	p_procsig->ps_sigcatch
#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

#define	NOCPU	0xff		/* For p_oncpu when we aren't on a CPU. */

/* Status values (p_stat). */
#define	SIDL	1		/* Process being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Process debugging or suspension. */
#define	SZOMB	5		/* Awaiting collection by parent. */
#define	SWAIT	6		/* Waiting for interrupt. */
#define	SMTX	7		/* Blocked on a mutex. */

/* These flags are kept in p_flag. */
#define	P_ADVLOCK	0x00001	/* Process may hold a POSIX advisory lock. */
#define	P_CONTROLT	0x00002	/* Has a controlling terminal. */
#define	P_KTHREAD	0x00004	/* Kernel thread. (*)*/
#define	P_NOLOAD	0x00008	/* Ignore during load avg calculations. */
#define	P_PPWAIT	0x00010	/* Parent is waiting for child to exec/exit. */
#define	P_SUGID		0x00100	/* Had set id privileges since last exec. */
#define	P_SYSTEM	0x00200	/* System proc: no sigs, stats or swapping. */
#define	P_TRACED	0x00800	/* Debugged process being traced. */
#define	P_WAITED	0x01000	/* Debugging process has waited for child. */
#define	P_WEXIT		0x02000	/* Working on exiting. */
#define	P_EXEC		0x04000	/* Process called exec. */
#define	P_KSES		0x08000	/* Process is using KSEs. */

/* Should be moved to machine-dependent areas. */
#define	P_BUFEXHAUST	0x100000 /* Dirty buffers flush is in progress. */
#define	P_COWINPROGRESS	0x400000 /* Snapshot copy-on-write in progress. */

#define	P_JAILED	0x1000000 /* Process is in jail. */
#define	P_OLDMASK	0x2000000 /* Need to restore mask after suspend. */
#define	P_ALTSTACK	0x4000000 /* Have alternate signal stack. */
#define	P_INEXEC	0x8000000 /* Process is in execve(). */

/* These flags are kept in p_sflag and are protected with sched_lock. */
#define	PS_INMEM	0x00001	/* Loaded into memory. */
#define	PS_PROFIL	0x00004	/* Has started profiling. */
#define	PS_ALRMPEND	0x00020	/* Pending SIGVTALRM needs to be posted. */
#define	PS_PROFPEND	0x00040	/* Pending SIGPROF needs to be posted. */
#define	PS_SWAPINREQ	0x00100	/* Swapin request due to wakeup. */
#define	PS_SWAPPING	0x00200	/* Process is being swapped. */

/* flags kept in td_flags */
#define	TDF_ONRUNQ	0x00001	/* This KE is on a run queue */
#define	TDF_SINTR	0x00008	/* Sleep is interruptible. */
#define	TDF_TIMEOUT	0x00010	/* Timing out during sleep. */
#define	TDF_SELECT	0x00040	/* Selecting; wakeup/waiting danger. */
#define	TDF_CVWAITQ	0x00080	/* Proces is on a cv_waitq (not slpq). */
#define	TDF_TIMOFAIL	0x01000	/* Timeout from sleep after we were awake. */
#define	TDF_DEADLKTREAT	0x800000 /* Lock aquisition - deadlock treatment. */

/* flags kept in ke_flags */
#define	KEF_ONRUNQ	0x00001	/* This KE is on a run queue */
#define	KEF_OWEUPC	0x00002	/* Owe process an addupc() call at next ast. */
#define	KEF_ASTPENDING	0x00400	/* KSE has a pending ast. */
#define	KEF_NEEDRESCHED	0x00800	/* Process needs to yield. */


#define	P_MAGIC		0xbeefface

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PARGS);
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

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(ke) do {						\
	mtx_assert(&sched_lock, MA_OWNED);				\
	(ke)->ke_flags |= KEF_ASTPENDING;				\
} while (0)

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
#define	PROC_UNLOCK_NOSWITCH(p)						\
	mtx_unlock_flags(&(p)->p_mtx, MTX_NOSWITCH)
#define	PROC_LOCKED(p)	mtx_owned(&(p)->p_mtx)
#define	PROC_LOCK_ASSERT(p, type)	mtx_assert(&(p)->p_mtx, (type))

/* Hold process U-area in memory, normally for ptrace/procfs work. */
#define	PHOLD(p) do {							\
	PROC_LOCK(p);							\
	_PHOLD(p);							\
	PROC_UNLOCK(p);							\
} while (0)
#define	_PHOLD(p) do {							\
	PROC_LOCK_ASSERT((p), MA_OWNED);				\
	if ((p)->p_lock++ == 0)						\
		faultin((p));						\
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

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

extern struct sx allproc_lock;
extern struct sx proctree_lock;
extern struct proc proc0;		/* Process slot for swapper. */
extern struct thread *thread0;		/* Primary thread in proc0 */
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

extern struct vm_zone *proc_zone;

extern int lastpid;

/*
 * XXX macros for scheduler.  Shouldn't be here, but currently needed for
 * bounding the dubious p_estcpu inheritance in wait1().
 * INVERSE_ESTCPU_WEIGHT is only suitable for statclock() frequencies in
 * the range 100-256 Hz (approximately).
 */
#define	ESTCPULIM(e) \
    min((e), INVERSE_ESTCPU_WEIGHT * (NICE_WEIGHT * (PRIO_MAX - PRIO_MIN) - \
	     RQ_PPQ) + INVERSE_ESTCPU_WEIGHT - 1)
#define	INVERSE_ESTCPU_WEIGHT	8	/* 1 / (priorities per estcpu level). */
#define	NICE_WEIGHT	1		/* Priorities per nice level. */

struct	proc *pfind __P((pid_t));	/* Find process by id. */
struct	pgrp *pgfind __P((pid_t));	/* Find process group by id. */
struct	proc *zpfind __P((pid_t));	/* Find zombie process by id. */

void	ast __P((struct trapframe *framep));
struct	thread *choosethread __P((void));
int	enterpgrp __P((struct proc *p, pid_t pgid, int mksess));
void	faultin __P((struct proc *p));
void	fixjobc __P((struct proc *p, struct pgrp *pgrp, int entering));
int	fork1 __P((struct thread *, int, struct proc **));
void	fork_exit __P((void (*)(void *, struct trapframe *), void *,
	    struct trapframe *));
void	fork_return __P((struct thread *, struct trapframe *));
int	inferior __P((struct proc *p));
int	leavepgrp __P((struct proc *p));
void	mi_switch __P((void));
int	p_candebug __P((struct proc *p1, struct proc *p2));
int	p_cansee __P((struct proc *p1, struct proc *p2));
int	p_cansched __P((struct proc *p1, struct proc *p2));
int	p_cansignal __P((struct proc *p1, struct proc *p2, int signum));
int	p_trespass __P((struct proc *p1, struct proc *p2));
void	procinit __P((void));
void	proc_linkup __P((struct proc *p));
void	proc_reparent __P((struct proc *child, struct proc *newparent));
int	procrunnable __P((void));
void	remrunqueue __P((struct thread *));
void	resetpriority __P((struct ksegrp *));
int	roundrobin_interval __P((void));
void	schedclock __P((struct thread *));
int	securelevel_ge __P((struct ucred *cr, int level));
int	securelevel_gt __P((struct ucred *cr, int level));
void	setrunnable __P((struct thread *));
void	setrunqueue __P((struct thread *));
void	setsugid __P((struct proc *p));
void	sleepinit __P((void));
void	stopevent __P((struct proc *, u_int, u_int));
void	cpu_idle __P((void));
void	cpu_switch __P((void));
void	cpu_throw __P((void)) __dead2;
void	unsleep __P((struct thread *));
void	updatepri __P((struct thread *));
void	userret __P((struct thread *, struct trapframe *, u_int));
void	maybe_resched __P((struct ksegrp *));

void	cpu_exit __P((struct thread *));
void	exit1 __P((struct thread *, int)) __dead2;
void	cpu_fork __P((struct thread *, struct proc *, int));
void	cpu_set_fork_handler __P((struct thread *, void (*)(void *), void *));
int	trace_req __P((struct proc *));
void	cpu_wait __P((struct proc *));
int	cpu_coredump __P((struct thread *, struct vnode *, struct ucred *));
#endif	/* _KERNEL */

#endif	/* !_SYS_PROC_H_ */
