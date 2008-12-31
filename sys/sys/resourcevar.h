/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)resourcevar.h	8.4 (Berkeley) 1/9/95
 * $FreeBSD: src/sys/sys/resourcevar.h,v 1.52.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_SYS_RESOURCEVAR_H_
#define	_SYS_RESOURCEVAR_H_

#include <sys/resource.h>
#include <sys/queue.h>
#ifdef _KERNEL
#include <sys/_lock.h>
#include <sys/_mutex.h>
#endif

/*
 * Kernel per-process accounting / statistics
 * (not necessarily resident except when running).
 *
 * Locking key:
 *      b - created at fork, never changes
 *      c - locked by proc mtx
 *      j - locked by proc slock
 *      k - only accessed by curthread
 */
struct pstats {
#define	pstat_startzero	p_cru
	struct	rusage p_cru;		/* Stats for reaped children. */
	struct	itimerval p_timer[3];	/* (j) Virtual-time timers. */
#define	pstat_endzero	pstat_startcopy

#define	pstat_startcopy	p_prof
	struct uprof {			/* Profile arguments. */
		caddr_t	pr_base;	/* (c + j) Buffer base. */
		u_long	pr_size;	/* (c + j) Buffer size. */
		u_long	pr_off;		/* (c + j) PC offset. */
		u_long	pr_scale;	/* (c + j) PC scaling. */
	} p_prof;
#define	pstat_endcopy	p_start
	struct	timeval p_start;	/* (b) Starting time. */
};

#ifdef _KERNEL

/*
 * Kernel shareable process resource limits.  Because this structure
 * is moderately large but changes infrequently, it is normally
 * shared copy-on-write after forks.
 */
struct plimit {
	struct	rlimit pl_rlimit[RLIM_NLIMITS];
	int	pl_refcnt;		/* number of references */
};

/*-
 * Per uid resource consumption
 *
 * Locking guide:
 * (a) Constant from inception
 * (b) Locked by ui_mtxp
 * (c) Locked by global uihashtbl_mtx
 */
struct uidinfo {
	LIST_ENTRY(uidinfo) ui_hash;	/* (c) hash chain of uidinfos */
	rlim_t	ui_sbsize;		/* (b) socket buffer space consumed */
	long	ui_proccnt;		/* (b) number of processes */
	uid_t	ui_uid;			/* (a) uid */
	u_int	ui_ref;			/* (b) reference count */
	struct mtx *ui_mtxp;		/* protect all counts/limits */
};

#define	UIDINFO_LOCK(ui)	mtx_lock((ui)->ui_mtxp)
#define	UIDINFO_UNLOCK(ui)	mtx_unlock((ui)->ui_mtxp)

struct proc;
struct rusage_ext;
struct thread;

void	 addupc_intr(struct thread *td, uintfptr_t pc, u_int ticks);
void	 addupc_task(struct thread *td, uintfptr_t pc, u_int ticks);
void	 calccru(struct proc *p, struct timeval *up, struct timeval *sp);
void	 calcru(struct proc *p, struct timeval *up, struct timeval *sp);
int	 chgproccnt(struct uidinfo *uip, int diff, int maxval);
int	 chgsbsize(struct uidinfo *uip, u_int *hiwat, u_int to,
	    rlim_t maxval);
int	 fuswintr(void *base);
struct plimit
	*lim_alloc(void);
void	 lim_copy(struct plimit *dst, struct plimit *src);
rlim_t	 lim_cur(struct proc *p, int which);
void	 lim_fork(struct proc *p1, struct proc *p2);
void	 lim_free(struct plimit *limp);
struct plimit
	*lim_hold(struct plimit *limp);
rlim_t	 lim_max(struct proc *p, int which);
void	 lim_rlimit(struct proc *p, int which, struct rlimit *rlp);
void	 ruadd(struct rusage *ru, struct rusage_ext *rux, struct rusage *ru2,
	    struct rusage_ext *rux2);
void	 rucollect(struct rusage *ru, struct rusage *ru2);
void	 rufetch(struct proc *p, struct rusage *ru);
void	 rufetchcalc(struct proc *p, struct rusage *ru, struct timeval *up,
	    struct timeval *sp);
void	 ruxagg(struct rusage_ext *rux, struct thread *td);
int	 suswintr(void *base, int word);
struct uidinfo
	*uifind(uid_t uid);
void	 uifree(struct uidinfo *uip);
void	 uihashinit(void);
void	 uihold(struct uidinfo *uip);

#endif /* _KERNEL */
#endif /* !_SYS_RESOURCEVAR_H_ */
