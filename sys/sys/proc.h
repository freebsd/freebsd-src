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
struct	session {
	int	s_count;		/* Ref cnt; pgrps in session. */
	struct	proc *s_leader;		/* Session leader. */
	struct	vnode *s_ttyvp;		/* Vnode of controlling terminal. */
	struct	tty *s_ttyp;		/* Controlling terminal. */
	pid_t	s_sid;			/* Session ID. */
					/* Setlogin() name: */
	char	s_login[roundup(MAXLOGNAME, sizeof(long))];
};

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* Hash chain. */
	LIST_HEAD(, proc) pg_members;	/* Pointer to pgrp members. */
	struct	session *pg_session;	/* Pointer to session. */
	struct  sigiolst pg_sigiolst;	/* List of sigio sources. */
	pid_t	pg_id;			/* Pgrp id. */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

struct	procsig {
	sigset_t ps_sigignore;	/* Signals being ignored. */
	sigset_t ps_sigcatch;	/* Signals being caught by user. */
	int	 ps_flag;
	struct	 sigacts *ps_sigacts;	/* Signal actions, state. */
	int	 ps_refcnt;
};

#define	PS_NOCLDWAIT	0x0001	/* No zombies if child dies */
#define	PS_NOCLDSTOP	0x0002	/* No SIGCHLD when children stop. */

/*
 * pasleep structure, used by asleep() syscall to hold requested priority
 * and timeout values for await().
 */
struct	pasleep {
	int	as_priority;	/* Async priority. */
	int	as_timo;	/* Async timeout. */
};

/*
 * pargs, used to hold a copy of the command line, if it had a sane length.
 */
struct	pargs {
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
 *      k - either by curproc or a lock which prevents the lock from
 *          going away, such as (d,e)
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

struct	proc {
	TAILQ_ENTRY(proc) p_procq;	/* (j) Run/mutex queue. */
	TAILQ_ENTRY(proc) p_slpq;	/* (j) Sleep queue. */
	LIST_ENTRY(proc) p_list;	/* (d) List of all processes. */

	/* substructures: */
	struct	ucred *p_ucred;		/* (c + k) Process owner's identity. */
	struct	filedesc *p_fd;		/* (b) Ptr to open files structure. */
	struct	pstats *p_stats;	/* (b) Accounting/statistics (CPU). */
	struct	plimit *p_limit;	/* (m) Process limits. */
	struct	vm_object *p_upages_obj;/* (a) Upages object. */
	struct	procsig *p_procsig;	/* (c) Signal actions, state (CPU). */
#define	p_sigacts	p_procsig->ps_sigacts
#define	p_sigignore	p_procsig->ps_sigignore
#define	p_sigcatch	p_procsig->ps_sigcatch

#define	p_rlimit	p_limit->pl_rlimit

	int	p_flag;			/* (c) P_* flags. */
	int	p_sflag;		/* (j) PS_* flags. */
	int	p_stat;			/* (j) S* process status. */

	pid_t	p_pid;			/* (b) Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* (d) Hash chain. */
	LIST_ENTRY(proc) p_pglist;	/* (c) List of processes in pgrp. */
	struct	proc *p_pptr;		/* (c + e) Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* (e) List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* (e) Pointer to list of children. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid

	pid_t	p_oppid;	 /* (c + e) Save parent pid during ptrace. XXX */
	int	p_dupfd;	 /* (c) Sideways ret value from fdopen. XXX */
	struct	vmspace *p_vmspace;	/* (b) Address space. */

	/* scheduling */
	u_int	p_estcpu;	 /* (j) Time averaged value of p_cpticks. */
	int	p_cpticks;	 /* (j) Ticks of cpu time. */
	fixpt_t	p_pctcpu;	 /* (j) %cpu during p_swtime. */
	struct	callout p_slpcallout;	/* (h) Callout for sleep. */
	void	*p_wchan;	 /* (j) Sleep address. */
	const char *p_wmesg;	 /* (j) Reason for sleep. */
	u_int	p_swtime;	 /* (j) Time swapped in or out. */
	u_int	p_slptime;	 /* (j) Time since last blocked. */

	struct	callout p_itcallout;	/* (h) Interval timer callout. */
	struct	itimerval p_realtimer;	/* (h?/k?) Alarm timer. */
	u_int64_t p_runtime;	/* (j) Real time in microsec. */
	u_int64_t p_uu;		/* (j) Previous user time in microsec. */
	u_int64_t p_su;		/* (j) Previous system time in microsec. */
	u_int64_t p_iu;		/* (j) Previous interrupt time in microsec. */
	u_int64_t p_uticks;	/* (j) Statclock hits in user mode. */
	u_int64_t p_sticks;	/* (j) Statclock hits in system mode. */
	u_int64_t p_iticks;	/* (j) Statclock hits processing intr. */

	int	p_traceflag;		/* (j?) Kernel trace points. */
	struct	vnode *p_tracep;	/* (j?) Trace to vnode. */

	sigset_t p_siglist;	/* (c) Signals arrived but not delivered. */

	struct	vnode *p_textvp;	/* (b) Vnode of executable. */

	struct	mtx p_mtx;		/* (k) Lock for this struct. */
	char	p_lock;		/* (c) Process lock (prevent swap) count. */
	u_char	p_oncpu;		/* (j) Which cpu we are on. */
	u_char	p_lastcpu;		/* (j) Last cpu we were on. */
	char	p_rqindex;		/* (j) Run queue index. */

	short	p_locks;	/* (*) DEBUG: lockmgr count of held locks */
	u_int	p_stops;		/* (c) Procfs event bitmask. */
	u_int	p_stype;		/* (c) Procfs stop event type. */
	char	p_step;			/* (c) Procfs stop *once* flag. */
	u_char	p_pfsflags;		/* (c) Procfs flags. */
	char	p_pad3[2];		/* Alignment. */
	register_t p_retval[2];		/* (k) Syscall aux returns. */
	struct	sigiolst p_sigiolst;	/* (c) List of sigio sources. */
	int	p_sigparent;		/* (c) Signal to parent on exit. */
	sigset_t p_oldsigmask;	/* (c) Saved mask from before sigpause. */
	int	p_sig;			/* (n) For core dump/debugger XXX. */
	u_long	p_code;			/* (n) For core dump/debugger XXX. */
	struct	klist p_klist;	/* (c) Knotes attached to this process. */
	struct	lock_list_entry *p_sleeplocks; /* (k) Held sleep locks. */
	struct	mtx *p_blocked;		/* (j) Mutex process is blocked on. */
	const char *p_mtxname;		/* (j) Name of mutex blocked on. */
	LIST_HEAD(, mtx) p_contested;	/* (j) Contested locks. */

	struct nlminfo	*p_nlminfo;	/* (?) only used by/for lockd */
	void	*p_aioinfo;	/* (c) ASYNC I/O info. */
	struct	ithd *p_ithd;	/* (b) For interrupt threads only. */
	int	p_intr_nesting_level;	/* (k) Interrupt recursion. */
	int	p_giant_optional;	/* (i) Giant Lock Sanity */

/* End area that is zeroed on creation. */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_sigmask

	sigset_t p_sigmask;	/* (c) Current signal mask. */
	stack_t	p_sigstk;	/* (c) Stack pointer and on-stack flag. */

	int	p_magic;	/* (b) Magic number. */
	struct	priority p_pri;	/* (j) Process priority. */
	char	p_nice;		/* (j?/k?) Process "nice" value. */
	char	p_comm[MAXCOMLEN + 1];	/* (b) Process name. */

	struct 	pgrp *p_pgrp;	/* (e?/c?) Pointer to process group. */
	struct 	sysentvec *p_sysent; /* (b) System call dispatch information. */
	struct	pargs *p_args;		/* (c + k) Process arguments. */

/* End area that is copied on creation. */
#define	p_endcopy	p_addr

	struct	user *p_addr;	/* (k) Kernel virtual addr of u-area (CPU). */
	struct	mdproc p_md;	/* (k) Any machine-dependent fields. */

	u_short	p_xstat;	/* (c) Exit status for wait; also stop sig. */
	u_short	p_acflag;	/* (c) Accounting flags. */
	struct	rusage *p_ru;	/* (a) Exit information. XXX */

	struct proc *p_peers;	/* (c) */
	struct proc *p_leader;	/* (c) */
	struct	pasleep p_asleep;	/* (k) Used by asleep()/await(). */
	void	*p_emuldata;	/* (c) Emulator state data. */
	struct trapframe *p_frame; /* (k) */
};

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
#define	P_KTHREAD	0x00004 /* Kernel thread. */
#define	P_NOLOAD	0x00008	/* Ignore during load avg calculations. */
#define	P_PPWAIT	0x00010	/* Parent is waiting for child to exec/exit. */
#define	P_SELECT	0x00040	/* Selecting; wakeup/waiting danger. */
#define	P_SUGID		0x00100	/* Had set id privileges since last exec. */
#define	P_SYSTEM	0x00200	/* System proc: no sigs, stats or swapping. */
#define	P_TRACED	0x00800	/* Debugged process being traced. */
#define	P_WAITED	0x01000	/* Debugging process has waited for child. */
#define	P_WEXIT		0x02000	/* Working on exiting. */
#define	P_EXEC		0x04000	/* Process called exec. */

/* Should be moved to machine-dependent areas. */

#define	P_BUFEXHAUST	0x100000 /* Dirty buffers flush is in progress. */
#define	P_COWINPROGRESS	0x400000 /* Snapshot copy-on-write in progress. */

#define	P_DEADLKTREAT	0x800000 /* Lock aquisition - deadlock treatment. */

#define	P_JAILED	0x1000000 /* Process is in jail. */
#define	P_OLDMASK	0x2000000 /* Need to restore mask after suspend. */
#define	P_ALTSTACK	0x4000000 /* Have alternate signal stack. */

/* These flags are kept in p_sflag and are protected with sched_lock. */
#define	PS_INMEM	0x00001	/* Loaded into memory. */
#define	PS_OWEUPC	0x00002	/* Owe process an addupc() call at next ast. */
#define	PS_PROFIL	0x00004	/* Has started profiling. */
#define	PS_SINTR	0x00008	/* Sleep is interruptible. */
#define	PS_TIMEOUT	0x00010	/* Timing out during sleep. */
#define	PS_ALRMPEND	0x00020 /* Pending SIGVTALRM needs to be posted. */
#define	PS_PROFPEND	0x00040 /* Pending SIGPROF needs to be posted. */
#define	PS_CVWAITQ	0x00080 /* Proces is on a cv_waitq (not slpq). */
#define	PS_SWAPINREQ	0x00100	/* Swapin request due to wakeup. */
#define	PS_SWAPPING	0x00200	/* Process is being swapped. */
#define	PS_ASTPENDING	0x00400	/* Process has a pending ast. */
#define	PS_NEEDRESCHED	0x00800	/* Process needs to yield. */

#define	P_MAGIC		0xbeefface

#define	P_CAN_SEE	1
#define	P_CAN_SCHED	3
#define	P_CAN_DEBUG	4

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PARGS);
MALLOC_DECLARE(M_SESSION);
MALLOC_DECLARE(M_SUBPROC);
MALLOC_DECLARE(M_ZOMBIE);
#endif

static __inline int
sigonstack(size_t sp)
{
	register struct proc *p = curproc;

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
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched(p) do {						\
	mtx_assert(&sched_lock, MA_OWNED);				\
	(p)->p_sflag |= PS_NEEDRESCHED;					\
} while (0)

#define	resched_wanted(p)	((p)->p_sflag & PS_NEEDRESCHED)

#define	clear_resched(p) do {						\
	mtx_assert(&sched_lock, MA_OWNED);				\
	(p)->p_sflag &= ~PS_NEEDRESCHED;				\
} while (0)

/*
 * Schedule an Asynchronous System Trap (AST) on return to user mode.
 */
#define	aston(p) do {							\
	mtx_assert(&sched_lock, MA_OWNED);				\
	(p)->p_sflag |= PS_ASTPENDING;					\
} while (0)

#define	astpending(p)	((p)->p_sflag & PS_ASTPENDING)

#define astoff(p) do {							\
	mtx_assert(&sched_lock, MA_OWNED);				\
	(p)->p_sflag &= ~PS_ASTPENDING;					\
} while (0)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	aston(p)

/* Handy macro to determine if p1 can mangle p2. */
#define	PRISON_CHECK(p1, p2) \
	((p1)->p_prison == NULL || (p1)->p_prison == (p2)->p_prison)

/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 */
#define	PID_MAX		99999
#define	NO_PID		100000

#define SESS_LEADER(p)	((p)->p_session->s_leader == (p))
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
#define PROC_LOCK(p)	mtx_lock(&(p)->p_mtx)
#define PROC_TRYLOCK(p)	mtx_trylock(&(p)->p_mtx)
#define PROC_UNLOCK(p)	mtx_unlock(&(p)->p_mtx)
#define	PROC_UNLOCK_NOSWITCH(p)						\
	mtx_unlock_flags(&(p)->p_mtx, MTX_NOSWITCH)
#define	PROC_LOCKED(p)	mtx_owned(&(p)->p_mtx)
#define	PROC_LOCK_ASSERT(p, type)	mtx_assert(&(p)->p_mtx, (type))

/* Hold process U-area in memory, normally for ptrace/procfs work. */
#define PHOLD(p) do {							\
	PROC_LOCK(p);							\
	_PHOLD(p);							\
	PROC_UNLOCK(p);							\
} while (0)
#define _PHOLD(p) do {							\
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
extern int hogticks;			/* Limit on kernel cpu hogs. */
extern int nprocs, maxproc;		/* Current and max number of procs. */
extern int maxprocperuid;		/* Max procs per uid. */
extern u_long ps_arg_cache_limit;
extern int ps_argsopen;
extern int ps_showallprocs;
extern int sched_quantum;		/* Scheduling quantum in ticks. */

LIST_HEAD(proclist, proc);
TAILQ_HEAD(procqueue, proc);
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
struct	proc *chooseproc __P((void));
int	enterpgrp __P((struct proc *p, pid_t pgid, int mksess));
void	faultin __P((struct proc *p));
void	fixjobc __P((struct proc *p, struct pgrp *pgrp, int entering));
int	fork1 __P((struct proc *, int, struct proc **));
void	fork_exit __P((void (*)(void *, struct trapframe *), void *,
	    struct trapframe *));
void	fork_return __P((struct proc *, struct trapframe *));
int	inferior __P((struct proc *p));
int	leavepgrp __P((struct proc *p));
void	mi_switch __P((void));
int	p_can __P((struct proc *p1, struct proc *p2, int operation,
	    int *privused));
int	p_cansignal __P((struct proc *p1, struct proc *p2, int signum));
int	p_trespass __P((struct proc *p1, struct proc *p2));
void	procinit __P((void));
void	proc_reparent __P((struct proc *child, struct proc *newparent));
int	procrunnable __P((void));
void	remrunqueue __P((struct proc *));
void	resetpriority __P((struct proc *));
int	roundrobin_interval __P((void));
void	schedclock __P((struct proc *));
void	setrunnable __P((struct proc *));
void	setrunqueue __P((struct proc *));
void	setsugid __P((struct proc *p));
void	sleepinit __P((void));
void	stopevent __P((struct proc *, u_int, u_int));
void	cpu_idle __P((void));
void	cpu_switch __P((void));
void	cpu_throw __P((void)) __dead2;
void	unsleep __P((struct proc *));
void	updatepri __P((struct proc *));
void	userret __P((struct proc *, struct trapframe *, u_quad_t));
void	maybe_resched __P((struct proc *));

void	cpu_exit __P((struct proc *)) __dead2;
void	exit1 __P((struct proc *, int)) __dead2;
void	cpu_fork __P((struct proc *, struct proc *, int));
void	cpu_set_fork_handler __P((struct proc *, void (*)(void *), void *));
int	trace_req __P((struct proc *));
void	cpu_wait __P((struct proc *));
int	cpu_coredump __P((struct proc *, struct vnode *, struct ucred *));
#endif	/* _KERNEL */

#endif	/* !_SYS_PROC_H_ */
