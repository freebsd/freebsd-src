/*-
 * Copyright (c) 1986, 1989, 1991 The Regents of the University of California.
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
 *	from: @(#)proc.h	7.28 (Berkeley) 5/30/91
 *	$Id: proc.h,v 1.7 1994/03/15 02:48:51 wollman Exp $
 */

#ifndef _PROC_H_
#define	_PROC_H_

#include <machine/proc.h>		/* machine-dependent proc substruct */

/*
 * One structure allocated per session.
 */
struct	session {
	int	s_count;		/* ref cnt; pgrps in session */
	struct	proc *s_leader;		/* session leader */
	struct	vnode *s_ttyvp;		/* vnode of controlling terminal */
	struct	tty *s_ttyp;		/* controlling terminal */
	char	s_login[MAXLOGNAME];	/* setlogin() name */
};

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	struct	pgrp *pg_hforw;		/* forward link in hash bucket */
	struct	proc *pg_mem;		/* pointer to pgrp members */
	struct	session *pg_session;	/* pointer to session */
	pid_t	pg_id;			/* pgrp id */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

/*
 * Description of a process.
 * This structure contains the information needed to manage a thread
 * of control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.  The process structure and the substructures
 * are always addressible except for those marked "(PROC ONLY)" below,
 * which might be addressible only on a processor on which the process
 * is running.
 */
struct	proc {
	struct	proc *p_link;		/* doubly-linked run/sleep queue */
	struct	proc *p_rlink;
	struct	proc *p_nxt;		/* linked list of active procs */
	struct	proc **p_prev;		/*    and zombies */

	/* substructures: */
	struct	pcred *p_cred;		/* process owner's identity */
	struct	filedesc *p_fd;		/* ptr to open files structure */
	struct	pstats *p_stats;	/* accounting/statistics (PROC ONLY) */
	struct	plimit *p_limit;	/* process limits */
	struct	vmspace *p_vmspace;	/* address space */
	struct	sigacts *p_sigacts;	/* signal actions, state (PROC ONLY) */

#define	p_ucred		p_cred->pc_ucred
#define	p_rlimit	p_limit->pl_rlimit

	int	p_flag;
	char	p_stat;
/*	char	p_space; */

	pid_t	p_pid;		/* unique process id */
	struct	proc *p_hash;	/* hashed based on p_pid for kill+exit+... */
	struct	proc *p_pgrpnxt; /* pointer to next process in process group */
	struct	proc *p_pptr;	/* pointer to process structure of parent */
	struct	proc *p_osptr;	/* pointer to older sibling processes */

/* The following fields are all zeroed upon creation in fork */
#define	p_startzero	p_ysptr
	struct	proc *p_ysptr;	/* pointer to younger siblings */
	struct	proc *p_cptr;	/* pointer to youngest living child */

	/* scheduling */
	u_int	p_cpu;		/* cpu usage for scheduling */
	int	p_cpticks;	/* ticks of cpu time */
	fixpt_t	p_pctcpu;	/* %cpu for this process during p_time */
	caddr_t p_wchan;	/* event process is awaiting */
	u_int	p_time;		/* resident/nonresident time for swapping */
	u_int	p_slptime;	/* time since last block */

	struct	itimerval p_realtimer;	/* alarm timer */
	struct	timeval p_utime;	/* user time */
	struct	timeval p_stime;	/* system time */

	int	p_traceflag;	/* kernel trace points */
	struct	vnode *p_tracep;/* trace to vnode */

	int	p_sig;		/* signals pending to this process */

/* end area that is zeroed on creation */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork */
	sigset_t p_sigmask;	/* current signal mask */
#define	p_startcopy	p_sigmask
	sigset_t p_sigignore;	/* signals being ignored */
	sigset_t p_sigcatch;	/* signals being caught by user */

	u_char	p_pri;		/* priority, negative is high */
	u_char	p_usrpri;	/* user-priority based on p_cpu and p_nice */
	char	p_nice;		/* nice for cpu usage */
/*	char	p_space1; */

	struct 	pgrp *p_pgrp;	/* pointer to process group */
	char	p_comm[MAXCOMLEN+1];

/* end area that is copied on creation */
#define	p_endcopy p_wmesg
	const char *p_wmesg;	/* reason for sleep */
	int	p_thread;	/* id for this "thread" (Mach glue) XXX */
	struct	user *p_addr;	/* kernel virtual addr of u-area (PROC ONLY) */
	swblk_t	p_swaddr;	/* disk address of u area when swapped */
	int	*p_regs;	/* saved registers during syscall/trap */
	struct	mdproc p_md;	/* any machine-dependent fields */

	u_short	p_xstat;	/* Exit status for wait; also stop signal */
	u_short	p_dupfd;	/* sideways return value from fdopen XXX */
	u_short	p_acflag;	/* accounting flags */
/*	short	p_space2; */
	struct	rusage *p_ru;	/* exit information XXX */

	long	p_spare[4];	/* tmp spares to avoid shifting eproc */
};

#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

/* MOVE TO ucred.h? */
/*
 * Shareable process credentials (always resident).
 * This includes a reference to the current user credentials
 * as well as real and saved ids that may be used to change ids.
 */
struct	pcred {
	struct	ucred *pc_ucred;	/* current credentials */
	uid_t	p_ruid;			/* real user id */
	uid_t	p_svuid;		/* saved effective user id */
	gid_t	p_rgid;			/* real group id */
	gid_t	p_svgid;		/* saved effective group id */
	int	p_refcnt;		/* number of references */
};

/* stat codes */
#define	SSLEEP	1		/* awaiting an event */
#define	SWAIT	2		/* (abandoned state) */
#define	SRUN	3		/* running */
#define	SIDL	4		/* intermediate state in process creation */
#define	SZOMB	5		/* intermediate state in process termination */
#define	SSTOP	6		/* process being traced */

/* flag codes */
#define	SLOAD	0x00000001	/* in core */
#define	SSYS	0x00000002	/* swapper or pager process */
#define	SSINTR	0x00000004	/* sleep is interruptible */
#define	SCTTY	0x00000008	/* has a controlling terminal */
#define	SPPWAIT	0x00000010	/* parent is waiting for child to exec/exit */
#define SEXEC	0x00000020	/* process called exec */
#define	STIMO	0x00000040	/* timing out during sleep */
#define	SSEL	0x00000080	/* selecting; wakeup/waiting danger */
#define	SWEXIT	0x00000100	/* working on exiting */
#define	SNOCLDSTOP \
		0x00000200	/* no SIGCHLD when children stop */
/* the following three should probably be changed into a hold count */
#define	SLOCK	0x00000400	/* process being swapped out */
#define	SKEEP	0x00000800	/* another flag to prevent swap out */
#define	SPHYSIO	0x00001000	/* doing physical i/o */
#define	STRC	0x00004000	/* process is being traced */
#define	SWTED	0x00008000	/* another tracing flag */
#define	SADVLCK	0x00040000	/* process may hold a POSIX advisory lock */
/* the following should be moved to machine-dependent areas */
#define	SOWEUPC	0x00002000	/* owe process an addupc() call at next ast */
#ifdef HPUXCOMPAT
#define	SHPUX	0x00010000	/* HP-UX process (HPUXCOMPAT) */
#else
#define	SHPUX	0		/* not HP-UX process (HPUXCOMPAT) */
#endif
#define	SUGID	0x00020000	/* process has changed [ug]id since exec */

#define SPAGEDAEMON \
		0x00080000	/* process has been scanned by pageout daemon */

#ifdef KERNEL
/*
 * We use process IDs <= PID_MAX;
 * PID_MAX + 1 must also fit in a pid_t
 * (used to represent "no process group").
 */
#define	PID_MAX		30000
#define	NO_PID		30001
#define	PIDHASH(pid)	((pid) & pidhashmask)

#define SESS_LEADER(p)	((p)->p_session->s_leader == (p))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s)	{ \
		if (--(s)->s_count == 0) \
			FREE(s, M_SESSION); \
	}

extern	int pidhashmask;		/* in param.c */
extern	struct proc *pidhash[];		/* in param.c */
extern	struct pgrp *pgrphash[];	/* in param.c */
extern	struct proc *zombproc, *allproc; /* lists of procs in various states */
extern	struct proc proc0;		/* process slot for swapper */
extern	struct	proc *initproc, *pageproc; /* process slots for init, pager */
extern	struct proc *curproc;		/* current running proc */
extern	int nprocs, maxproc;		/* current and max number of procs */

#define	NQS	32		/* 32 run queues */
struct	prochd {
	struct	proc *ph_link;	/* linked list of running processes */
	struct	proc *ph_rlink;
};

extern struct prochd qs[NQS];
extern	int whichqs;		/* bit mask summarizing non-empty qs's */

extern struct 	pgrp *pgfind(pid_t);	/* find process group by id */
extern struct	proc *pfind(int);	/* find process by id */

void fixjobc(struct proc *, struct pgrp *, int);
void unsleep(struct proc *);
void setrun(struct proc *);
void setpri(struct proc *);

void	remrq __P((struct proc *));
void	setrq __P((struct proc *));
void	updatepri __P((struct proc *));
int	cpu_fork __P((struct proc *, struct proc *));
void	enterpgrp __P((struct proc *, int /*pid_t*/, int));

#endif	/* KERNEL */

#endif	/* !_PROC_H_ */
