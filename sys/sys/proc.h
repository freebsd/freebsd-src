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

#include <machine/proc.h>		/* Machine-dependent proc substruct. */
#include <sys/callout.h>		/* For struct callout_handle. */
#include <sys/filedesc.h>
#include <sys/queue.h>
#include <sys/rtprio.h>			/* For struct rtprio. */
#include <sys/signal.h>
#ifndef _KERNEL
#include <sys/time.h>			/* For structs itimerval, timeval. */
#endif
#include <sys/ucred.h>
#include <sys/event.h>			/* For struct klist */

/*
 * One structure allocated per session.
 */
struct	session {
	int	s_count;		/* Ref cnt; pgrps in session. */
	struct	proc *s_leader;		/* Session leader. */
	struct	vnode *s_ttyvp;		/* Vnode of controlling terminal. */
	struct	tty *s_ttyp;		/* Controlling terminal. */
	pid_t	s_sid;			/* Session ID */
	char	s_login[roundup(MAXLOGNAME, sizeof(long))];	/* Setlogin() name. */
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
	int      ps_flag;
	struct	 sigacts *ps_sigacts;
	int	 ps_refcnt;
};

#define	PS_NOCLDWAIT	0x0001	/* No zombies if child dies */
#define	PS_NOCLDSTOP	0x0002	/* No SIGCHLD when children stop. */

/*
 * pasleep structure, used by asleep() syscall to hold requested priority
 * and timeout values for await().
 */
struct  pasleep {
	int	as_priority;	/* Async priority. */
	int	as_timo;	/* Async timeout. */
};

/*
 * pargs, used to hold a copy of the command line, if it had a sane
 * length
 */
struct	pargs {
	u_int	ar_ref;		/* Reference count */
	u_int	ar_length;	/* Length */
	u_char	ar_args[0];	/* Arguments */
};

/*
 * Description of a process.
 *
 * This structure contains the information needed to manage a thread of
 * control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.  The process structure and the substructures
 * are always addressable except for those marked "(PROC ONLY)" below,
 * which might be addressable only on a processor on which the process
 * is running.
 */

struct jail;

struct mtx;

struct ithd;

struct	proc {
	TAILQ_ENTRY(proc) p_procq;	/* run/sleep queue. */
	LIST_ENTRY(proc) p_list;	/* List of all processes. */

	/* substructures: */
	struct	pcred *p_cred;		/* Process owner's identity. */
	struct	filedesc *p_fd;		/* Ptr to open files structure. */
	struct	pstats *p_stats;	/* Accounting/statistics (PROC ONLY). */
	struct	plimit *p_limit;	/* Process limits. */
	struct	vm_object *p_upages_obj;/* Upages object */
	struct	procsig *p_procsig;
#define p_sigacts	p_procsig->ps_sigacts
#define p_sigignore	p_procsig->ps_sigignore
#define p_sigcatch	p_procsig->ps_sigcatch

#define	p_ucred		p_cred->pc_ucred
#define	p_rlimit	p_limit->pl_rlimit

	int	p_flag;			/* P_* flags. */
	char	p_stat;			/* S* process status. */
	char	p_pad1[3];

	pid_t	p_pid;			/* Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* Hash chain. */
	LIST_ENTRY(proc) p_pglist;	/* List of processes in pgrp. */
	struct	proc *p_pptr;	 	/* Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* Pointer to list of children. */

	struct callout_handle p_ithandle; /*
					      * Callout handle for scheduling
					      * p_realtimer.
					      */
/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid

	pid_t	p_oppid;	 /* Save parent pid during ptrace. XXX */
	int	p_dupfd;	 /* Sideways return value from fdopen. XXX */

	struct	vmspace *p_vmspace;	/* Address space. */

	/* scheduling */
	u_int	p_estcpu;	 /* Time averaged value of p_cpticks. */
	int	p_cpticks;	 /* Ticks of cpu time. */
	fixpt_t	p_pctcpu;	 /* %cpu for this process during p_swtime */
	void	*p_wchan;	 /* Sleep address. */
	const char *p_wmesg;	 /* Reason for sleep. */
	u_int	p_swtime;	 /* Time swapped in or out. */
	u_int	p_slptime;	 /* Time since last blocked. */

	struct	itimerval p_realtimer;	/* Alarm timer. */
	u_int64_t p_runtime;		/* Real time in microsec. */
	u_int64_t p_uu;			/* Previous user time in microsec. */
	u_int64_t p_su;			/* Previous system time in microsec. */
	u_int64_t p_iu;			/* Previous interrupt time in usec. */
	u_int64_t p_uticks;		/* Statclock hits in user mode. */
	u_int64_t p_sticks;		/* Statclock hits in system mode. */
	u_int64_t p_iticks;		/* Statclock hits processing intr. */

	int	p_traceflag;		/* Kernel trace points. */
	struct	vnode *p_tracep;	/* Trace to vnode. */

	sigset_t p_siglist;		/* Signals arrived but not delivered. */

	struct	vnode *p_textvp;	/* Vnode of executable. */

	char	p_lock;			/* Process lock (prevent swap) count. */
	u_char	p_oncpu;		/* Which cpu we are on */
	u_char	p_lastcpu;		/* Last cpu we were on */
	char	p_rqindex;		/* Run queue index */

	short	p_locks;		/* DEBUG: lockmgr count of held locks */
	short	p_simple_locks;		/* DEBUG: count of held simple locks */
	unsigned int	p_stops;	/* procfs event bitmask */
	unsigned int	p_stype;	/* procfs stop event type */
	char	p_step;			/* procfs stop *once* flag */
	unsigned char	p_pfsflags;	/* procfs flags */
	char	p_pad3[2];		/* padding for alignment */
	register_t p_retval[2];		/* syscall aux returns */
	struct	sigiolst p_sigiolst;	/* list of sigio sources */
	int	p_sigparent;		/* signal to parent on exit */
	sigset_t p_oldsigmask;		/* saved mask from before sigpause */
	int	p_sig;			/* for core dump/debugger XXX */
        u_long	p_code;	  	        /* for core dump/debugger XXX */
	struct	klist p_klist;		/* knotes attached to this process */
	LIST_HEAD(, mtx) p_heldmtx;	/* for debugging code */
	struct mtx *p_blocked;		/* Mutex process is blocked on */
	LIST_HEAD(, mtx) p_contested;	/* contested locks */

/* End area that is zeroed on creation. */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_sigmask

	sigset_t p_sigmask;	/* Current signal mask. */
	stack_t	p_sigstk;	/* sp & on stack state variable */

	int	p_magic;	/* Magic number. */
	u_char	p_priority;	/* Process priority. */
	u_char	p_usrpri;	/* User-priority based on p_cpu and p_nice. */
	u_char	p_nativepri;	/* Priority before propogation. */
	char	p_nice;		/* Process "nice" value. */
	char	p_comm[MAXCOMLEN+1];

	struct 	pgrp *p_pgrp;	/* Pointer to process group. */

	struct 	sysentvec *p_sysent; /* System call dispatch information. */

	struct	rtprio p_rtprio;	/* Realtime priority. */
	struct	prison *p_prison;
	struct	pargs *p_args;
/* End area that is copied on creation. */
#define	p_endcopy	p_addr
	struct	user *p_addr;	/* Kernel virtual addr of u-area (PROC ONLY). */
	struct	mdproc p_md;	/* Any machine-dependent fields. */

	u_short	p_xstat;	/* Exit status for wait; also stop signal. */
	u_short	p_acflag;	/* Accounting flags. */
	struct	rusage *p_ru;	/* Exit information. XXX */

	int	p_nthreads;	/* number of threads (only in leader) */
	void	*p_aioinfo;	/* ASYNC I/O info */
	int	p_wakeup;	/* thread id */
	struct proc *p_peers;	
	struct proc *p_leader;
	struct	pasleep p_asleep;	/* Used by asleep()/await(). */
	void	*p_emuldata;	/* process-specific emulator state data */
	struct ithd *p_ithd;	/* for interrupt threads only */
};

#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

/* Status values (p_stat) */
#define	SIDL	1		/* Process being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Process debugging or suspension. */
#define	SZOMB	5		/* Awaiting collection by parent. */
#define	SWAIT	6		/* Waiting for interrupt or CPU. */
#define	SMTX	7		/* Blocked on a mutex. */

/* These flags are kept in p_flags. */
#define	P_ADVLOCK	0x00001	/* Process may hold a POSIX advisory lock. */
#define	P_CONTROLT	0x00002	/* Has a controlling terminal. */
#define	P_INMEM		0x00004	/* Loaded into memory. */
#define	P_PPWAIT	0x00010	/* Parent is waiting for child to exec/exit. */
#define	P_PROFIL	0x00020	/* Has started profiling. */
#define	P_SELECT	0x00040	/* Selecting; wakeup/waiting danger. */
#define	P_SINTR		0x00080	/* Sleep is interruptible. */
#define	P_SUGID		0x00100	/* Had set id privileges since last exec. */
#define	P_SYSTEM	0x00200	/* System proc: no sigs, stats or swapping. */
#define	P_TIMEOUT	0x00400	/* Timing out during sleep. */
#define	P_TRACED	0x00800	/* Debugged process being traced. */
#define	P_WAITED	0x01000	/* Debugging process has waited for child. */
#define	P_WEXIT		0x02000	/* Working on exiting. */
#define	P_EXEC		0x04000	/* Process called exec. */

/* Should probably be changed into a hold count. */
/* was	P_NOSWAP	0x08000	was: Do not swap upages; p->p_hold */
/* was	P_PHYSIO	0x10000	was: Doing physical I/O; use p->p_hold */

/* Should be moved to machine-dependent areas. */
#define	P_OWEUPC	0x20000	/* Owe process an addupc() call at next ast. */

#define	P_SWAPPING	0x40000	/* Process is being swapped. */
#define	P_SWAPINREQ	0x80000	/* Swapin request due to wakeup */

/* Marked a kernel thread */
#define	P_BUFEXHAUST	0x100000 /* dirty buffers flush is in progress */
#define	P_KTHREADP	0x200000 /* Process is really a kernel thread */
#define	P_COWINPROGRESS	0x400000 /* Snapshot copy-on-write in progress */

#define	P_DEADLKTREAT   0x800000 /* lock aquisition - deadlock treatment */

#define	P_JAILED	0x1000000 /* Process is in jail */
#define	P_OLDMASK	0x2000000 /* need to restore mask before pause */
#define	P_ALTSTACK	0x4000000 /* have alternate signal stack */

#define	P_MAGIC		0xbeefface

#define	P_CAN_SEE	1
#define	P_CAN_KILL	2
#define	P_CAN_SCHED	3
#define	P_CAN_DEBUG	4

/*
 * MOVE TO ucred.h?
 *
 * Shareable process credentials (always resident).  This includes a reference
 * to the current user credentials as well as real and saved ids that may be
 * used to change ids.
 */
struct	pcred {
	struct	ucred *pc_ucred;	/* Current credentials. */
	uid_t	p_ruid;			/* Real user id. */
	uid_t	p_svuid;		/* Saved effective user id. */
	gid_t	p_rgid;			/* Real group id. */
	gid_t	p_svgid;		/* Saved effective group id. */
	int	p_refcnt;		/* Number of references. */
	struct	uidinfo *p_uidinfo;	/* Per uid resource consumption */
};

/*
 * Describe an interrupt thread.  There is one of these per irq.  BSD/OS makes
 * this a superset of struct proc, i.e. it_proc is the struct itself and not a
 * pointer.  We point in both directions, because it feels good that way.
 */
typedef struct ithd {
	struct proc	*it_proc;	/* interrupt process */

	LIST_HEAD(ihhead, intrhand) it_ihhead;
	LIST_HEAD(srchead, isrc) it_isrchead;

	/* Fields used by all interrupt threads */
	LIST_ENTRY(ithd) it_list;	/* All interrupt threads */
	int		it_need;	/* Needs service */
	int		irq;		/* irq */
	struct intrec	*it_ih;		/* head of handler queue */
	struct ithd	*it_interrupted; /* Who we interrupted */

	/* Fields used only for hard interrupt threads */
	int		it_stray;	/* Stray interrupts */

#ifdef APIC_IO
	/* Used by APIC interrupt sources */
	int		it_needeoi;	/* An EOI is needed */
	int		it_blocked;	/* at least 1 blocked apic src */
#endif

	/* stats */
#ifdef SMP_DEBUG
	int		it_busy;	/* failed attempts on runlock */
	int		it_lostneeded;	/* Number of it_need races lost */
	int		it_invprio;	/* Startup priority inversions */
#endif
#ifdef NEEDED
	/*
	 * These are in the BSD/OS i386 sources only, not in SPARC.
	 * I'm not yet sure we need them.
	 */
	LIST_HEAD(ihhead, intrhand) it_ihhead;
	LIST_HEAD(srchead, isrc) it_isrchead;

	/* Fields used by all interrupt threads */
	LIST_ENTRY(ithd) it_list;	/* All interrupt threads */

	/* Fields user only for soft interrupt threads */
	sifunc_t	it_service;	/* service routine */
	int		it_cnt;		/* number of schedule events */

#endif
} ithd;

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SESSION);
MALLOC_DECLARE(M_SUBPROC);
MALLOC_DECLARE(M_ZOMBIE);
MALLOC_DECLARE(M_PARGS);
#endif

/* flags for suser_xxx() */
#define PRISON_ROOT	1

/* Handy macro to determine of p1 can mangle p2 */

#define PRISON_CHECK(p1, p2) \
	((!(p1)->p_prison) || (p1)->p_prison == (p2)->p_prison)

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

/*
 * STOPEVENT is MP SAFE.
 */
extern void stopevent(struct proc*, unsigned int, unsigned int);
#define	STOPEVENT(p,e,v)				\
	do {						\
		if ((p)->p_stops & (e)) {		\
			mtx_enter(&Giant, MTX_DEF);	\
			stopevent(p,e,v);		\
			mtx_exit(&Giant, MTX_DEF);	\
		}					\
	} while (0)

/* hold process U-area in memory, normally for ptrace/procfs work */
#define PHOLD(p) {							\
	if ((p)->p_lock++ == 0 && ((p)->p_flag & P_INMEM) == 0)	\
		faultin(p);						\
}
#define PRELE(p)	(--(p)->p_lock)

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

#ifndef SET_CURPROC
#define SET_CURPROC(p)	(curproc = (p))
#endif

#ifndef curproc
extern struct proc *curproc;		/* Current running proc. */
extern struct proc *prevproc;		/* Previously running proc. */
extern struct proc *idleproc;		/* Current idle proc. */
extern u_int astpending;		/* software interrupt pending */
extern int switchticks;			/* `ticks' at last context switch. */
extern struct timeval switchtime;	/* Uptime at last context switch */
#endif
extern struct proc proc0;		/* Process slot for swapper. */
extern int hogticks;			/* Limit on kernel cpu hogs. */
extern int nprocs, maxproc;		/* Current and max number of procs. */
extern int maxprocperuid;		/* Max procs per uid. */
extern int sched_quantum;		/* Scheduling quantum in ticks */

LIST_HEAD(proclist, proc);
extern struct proclist allproc;		/* List of all processes. */
extern struct proclist zombproc;	/* List of zombie processes. */
extern struct proc *initproc, *pageproc, *updateproc; /* Process slots for init, pager. */

#define	NQS	32			/* 32 run queues. */
TAILQ_HEAD(rq, proc);
extern struct rq itqueues[];
extern struct rq rtqueues[];
extern struct rq queues[];
extern struct rq idqueues[];

/*
 * XXX macros for scheduler.  Shouldn't be here, but currently needed for
 * bounding the dubious p_estcpu inheritance in wait1().
 * INVERSE_ESTCPU_WEIGHT is only suitable for statclock() frequencies in
 * the range 100-256 Hz (approximately).
 */
#define	INVERSE_ESTCPU_WEIGHT	8	/* 1 / (priorities per estcpu level) */
#define	NICE_WEIGHT	1		/* priorities per nice level */
#define	PPQ		(128 / NQS)	/* priorities per queue */
#define	ESTCPULIM(e) \
    min((e), INVERSE_ESTCPU_WEIGHT * (NICE_WEIGHT * PRIO_TOTAL - PPQ) + \
	INVERSE_ESTCPU_WEIGHT - 1)

extern	u_long ps_arg_cache_limit;
extern	int ps_argsopen;
extern	int ps_showallprocs;

struct proc *pfind __P((pid_t));	/* Find process by id. */
struct pgrp *pgfind __P((pid_t));	/* Find process group by id. */

struct vm_zone;
extern struct vm_zone *proc_zone;

int	enterpgrp __P((struct proc *p, pid_t pgid, int mksess));
void	fixjobc __P((struct proc *p, struct pgrp *pgrp, int entering));
int	inferior __P((struct proc *p));
int	leavepgrp __P((struct proc *p));
void	mi_switch __P((void));
void	procinit __P((void));
int	p_can __P((const struct proc *p1, const struct proc *p2, int operation,
    int *privused));

void	resetpriority __P((struct proc *));
int	roundrobin_interval __P((void));
void	schedclock __P((struct proc *));
void	setrunnable __P((struct proc *));
void	setrunqueue __P((struct proc *));
void	sleepinit __P((void));
int	suser __P((const struct proc *));
int	suser_xxx __P((const struct ucred *cred, const struct proc *proc,
    int flag));
void	remrunqueue __P((struct proc *));
void	cpu_switch __P((void));
void	cpu_throw __P((void)) __dead2;
void	unsleep __P((struct proc *));

void	cpu_exit __P((struct proc *)) __dead2;
void	exit1 __P((struct proc *, int)) __dead2;
void	cpu_fork __P((struct proc *, struct proc *, int));
void	cpu_set_fork_handler __P((struct proc *, void (*)(void *), void *));
int	fork1 __P((struct proc *, int, struct proc **));
int	trace_req __P((struct proc *));
void	cpu_wait __P((struct proc *));
int	cpu_coredump __P((struct proc *, struct vnode *, struct ucred *));
void	setsugid __P((struct proc *p));
void	faultin __P((struct proc *p));

struct proc *	chooseproc __P((void));
u_int32_t	procrunnable __P((void));

#endif	/* _KERNEL */

#endif	/* !_SYS_PROC_H_ */
