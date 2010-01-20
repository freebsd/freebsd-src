/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)kern_resource.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/file.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/time.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>


static MALLOC_DEFINE(M_PLIMIT, "plimit", "plimit structures");
static MALLOC_DEFINE(M_UIDINFO, "uidinfo", "uidinfo structures");
#define	UIHASH(uid)	(&uihashtbl[(uid) & uihash])
static struct mtx uihashtbl_mtx;
static LIST_HEAD(uihashhead, uidinfo) *uihashtbl;
static u_long uihash;		/* size of hash table - 1 */

static void	calcru1(struct proc *p, struct rusage_ext *ruxp,
		    struct timeval *up, struct timeval *sp);
static int	donice(struct thread *td, struct proc *chgp, int n);
static struct uidinfo *uilookup(uid_t uid);

/*
 * Resource controls and accounting.
 */

#ifndef _SYS_SYSPROTO_H_
struct getpriority_args {
	int	which;
	int	who;
};
#endif
/*
 * MPSAFE
 */
int
getpriority(td, uap)
	struct thread *td;
	register struct getpriority_args *uap;
{
	struct proc *p;
	struct pgrp *pg;
	int error, low;

	error = 0;
	low = PRIO_MAX + 1;
	switch (uap->which) {

	case PRIO_PROCESS:
		if (uap->who == 0)
			low = td->td_proc->p_nice;
		else {
			p = pfind(uap->who);
			if (p == NULL)
				break;
			if (p_cansee(td, p) == 0)
				low = p->p_nice;
			PROC_UNLOCK(p);
		}
		break;

	case PRIO_PGRP:
		sx_slock(&proctree_lock);
		if (uap->who == 0) {
			pg = td->td_proc->p_pgrp;
			PGRP_LOCK(pg);
		} else {
			pg = pgfind(uap->who);
			if (pg == NULL) {
				sx_sunlock(&proctree_lock);
				break;
			}
		}
		sx_sunlock(&proctree_lock);
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (!p_cansee(td, p)) {
				if (p->p_nice < low)
					low = p->p_nice;
			}
			PROC_UNLOCK(p);
		}
		PGRP_UNLOCK(pg);
		break;

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = td->td_ucred->cr_uid;
		sx_slock(&allproc_lock);
		LIST_FOREACH(p, &allproc, p_list) {
			PROC_LOCK(p);
			if (!p_cansee(td, p) &&
			    p->p_ucred->cr_uid == uap->who) {
				if (p->p_nice < low)
					low = p->p_nice;
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
		break;

	default:
		error = EINVAL;
		break;
	}
	if (low == PRIO_MAX + 1 && error == 0)
		error = ESRCH;
	td->td_retval[0] = low;
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setpriority_args {
	int	which;
	int	who;
	int	prio;
};
#endif
/*
 * MPSAFE
 */
int
setpriority(td, uap)
	struct thread *td;
	struct setpriority_args *uap;
{
	struct proc *curp, *p;
	struct pgrp *pg;
	int found = 0, error = 0;

	curp = td->td_proc;
	switch (uap->which) {
	case PRIO_PROCESS:
		if (uap->who == 0) {
			PROC_LOCK(curp);
			error = donice(td, curp, uap->prio);
			PROC_UNLOCK(curp);
		} else {
			p = pfind(uap->who);
			if (p == 0)
				break;
			if (p_cansee(td, p) == 0)
				error = donice(td, p, uap->prio);
			PROC_UNLOCK(p);
		}
		found++;
		break;

	case PRIO_PGRP:
		sx_slock(&proctree_lock);
		if (uap->who == 0) {
			pg = curp->p_pgrp;
			PGRP_LOCK(pg);
		} else {
			pg = pgfind(uap->who);
			if (pg == NULL) {
				sx_sunlock(&proctree_lock);
				break;
			}
		}
		sx_sunlock(&proctree_lock);
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (!p_cansee(td, p)) {
				error = donice(td, p, uap->prio);
				found++;
			}
			PROC_UNLOCK(p);
		}
		PGRP_UNLOCK(pg);
		break;

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = td->td_ucred->cr_uid;
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			/* Do not bother to check PRS_NEW processes */
			if (p->p_state == PRS_NEW)
				continue;
			PROC_LOCK(p);
			if (p->p_ucred->cr_uid == uap->who &&
			    !p_cansee(td, p)) {
				error = donice(td, p, uap->prio);
				found++;
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
		break;

	default:
		error = EINVAL;
		break;
	}
	if (found == 0 && error == 0)
		error = ESRCH;
	return (error);
}

/*
 * Set "nice" for a (whole) process.
 */
static int
donice(struct thread *td, struct proc *p, int n)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((error = p_cansched(td, p)))
		return (error);
	if (n > PRIO_MAX)
		n = PRIO_MAX;
	if (n < PRIO_MIN)
		n = PRIO_MIN;
 	if (n < p->p_nice && suser(td) != 0)
		return (EACCES);
	mtx_lock_spin(&sched_lock);
	sched_nice(p, n);
	mtx_unlock_spin(&sched_lock);
	return (0);
}

/*
 * Set realtime priority.
 *
 * MPSAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct rtprio_args {
	int		function;
	pid_t		pid;
	struct rtprio	*rtp;
};
#endif

int
rtprio(td, uap)
	struct thread *td;		/* curthread */
	register struct rtprio_args *uap;
{
	struct proc *curp;
	struct proc *p;
	struct ksegrp *kg;
	struct rtprio rtp;
	int cierror, error;

	/* Perform copyin before acquiring locks if needed. */
	if (uap->function == RTP_SET)
		cierror = copyin(uap->rtp, &rtp, sizeof(struct rtprio));
	else
		cierror = 0;

	curp = td->td_proc;
	if (uap->pid == 0) {
		p = curp;
		PROC_LOCK(p);
	} else {
		p = pfind(uap->pid);
		if (p == NULL)
			return (ESRCH);
	}

	switch (uap->function) {
	case RTP_LOOKUP:
		if ((error = p_cansee(td, p)))
			break;
		mtx_lock_spin(&sched_lock);
		/*
		 * Return OUR priority if no pid specified,
		 * or if one is, report the highest priority
		 * in the process.  There isn't much more you can do as 
		 * there is only room to return a single priority.
		 * XXXKSE: maybe need a new interface to report 
		 * priorities of multiple system scope threads.
		 * Note: specifying our own pid is not the same
		 * as leaving it zero.
		 */
		if (uap->pid == 0) {
			pri_to_rtp(td->td_ksegrp, &rtp);
		} else {
			struct rtprio rtp2;

			rtp.type = RTP_PRIO_IDLE;
			rtp.prio = RTP_PRIO_MAX;
			FOREACH_KSEGRP_IN_PROC(p, kg) {
				pri_to_rtp(kg, &rtp2);
				if (rtp2.type <  rtp.type ||
				    (rtp2.type == rtp.type &&
				    rtp2.prio < rtp.prio)) {
					rtp.type = rtp2.type;
					rtp.prio = rtp2.prio;
				}
			}
		}
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
		return (copyout(&rtp, uap->rtp, sizeof(struct rtprio)));
	case RTP_SET:
		if ((error = p_cansched(td, p)) || (error = cierror))
			break;

		/* Disallow setting rtprio in most cases if not superuser. */
		if (suser(td) != 0) {
			/* can't set someone else's */
			if (uap->pid) {
				error = EPERM;
				break;
			}
			/* can't set realtime priority */
/*
 * Realtime priority has to be restricted for reasons which should be
 * obvious.  However, for idle priority, there is a potential for
 * system deadlock if an idleprio process gains a lock on a resource
 * that other processes need (and the idleprio process can't run
 * due to a CPU-bound normal process).  Fix me!  XXX
 */
#if 0
 			if (RTP_PRIO_IS_REALTIME(rtp.type)) {
#else
			if (rtp.type != RTP_PRIO_NORMAL) {
#endif
				error = EPERM;
				break;
			}
		}

		/*
		 * If we are setting our own priority, set just our
		 * KSEGRP but if we are doing another process,
		 * do all the groups on that process. If we
		 * specify our own pid we do the latter.
		 */
		mtx_lock_spin(&sched_lock);
		if (uap->pid == 0) {
			error = rtp_to_pri(&rtp, td->td_ksegrp);
		} else {
			FOREACH_KSEGRP_IN_PROC(p, kg) {
				if ((error = rtp_to_pri(&rtp, kg)) != 0) {
					break;
				}
			}
		}
		mtx_unlock_spin(&sched_lock);
		break;
	default:
		error = EINVAL;
		break;
	}
	PROC_UNLOCK(p);
	return (error);
}

int
rtp_to_pri(struct rtprio *rtp, struct ksegrp *kg)
{

	mtx_assert(&sched_lock, MA_OWNED);
	if (rtp->prio > RTP_PRIO_MAX)
		return (EINVAL);
	switch (RTP_PRIO_BASE(rtp->type)) {
	case RTP_PRIO_REALTIME:
		kg->kg_user_pri = PRI_MIN_REALTIME + rtp->prio;
		break;
	case RTP_PRIO_NORMAL:
		kg->kg_user_pri = PRI_MIN_TIMESHARE + rtp->prio;
		break;
	case RTP_PRIO_IDLE:
		kg->kg_user_pri = PRI_MIN_IDLE + rtp->prio;
		break;
	default:
		return (EINVAL);
	}
	sched_class(kg, rtp->type);
	if (curthread->td_ksegrp == kg) {
		sched_prio(curthread, kg->kg_user_pri); /* XXX dubious */
	}
	return (0);
}

void
pri_to_rtp(struct ksegrp *kg, struct rtprio *rtp)
{

	mtx_assert(&sched_lock, MA_OWNED);
	switch (PRI_BASE(kg->kg_pri_class)) {
	case PRI_REALTIME:
		rtp->prio = kg->kg_user_pri - PRI_MIN_REALTIME;
		break;
	case PRI_TIMESHARE:
		rtp->prio = kg->kg_user_pri - PRI_MIN_TIMESHARE;
		break;
	case PRI_IDLE:
		rtp->prio = kg->kg_user_pri - PRI_MIN_IDLE;
		break;
	default:
		break;
	}
	rtp->type = kg->kg_pri_class;
}

#if defined(COMPAT_43)
#ifndef _SYS_SYSPROTO_H_
struct osetrlimit_args {
	u_int	which;
	struct	orlimit *rlp;
};
#endif
/*
 * MPSAFE
 */
int
osetrlimit(td, uap)
	struct thread *td;
	register struct osetrlimit_args *uap;
{
	struct orlimit olim;
	struct rlimit lim;
	int error;

	if ((error = copyin(uap->rlp, &olim, sizeof(struct orlimit))))
		return (error);
	lim.rlim_cur = olim.rlim_cur;
	lim.rlim_max = olim.rlim_max;
	error = kern_setrlimit(td, uap->which, &lim);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ogetrlimit_args {
	u_int	which;
	struct	orlimit *rlp;
};
#endif
/*
 * MPSAFE
 */
int
ogetrlimit(td, uap)
	struct thread *td;
	register struct ogetrlimit_args *uap;
{
	struct orlimit olim;
	struct rlimit rl;
	struct proc *p;
	int error;

	if (uap->which >= RLIM_NLIMITS)
		return (EINVAL);
	p = td->td_proc;
	PROC_LOCK(p);
	lim_rlimit(p, uap->which, &rl);
	PROC_UNLOCK(p);

	/*
	 * XXX would be more correct to convert only RLIM_INFINITY to the
	 * old RLIM_INFINITY and fail with EOVERFLOW for other larger
	 * values.  Most 64->32 and 32->16 conversions, including not
	 * unimportant ones of uids are even more broken than what we
	 * do here (they blindly truncate).  We don't do this correctly
	 * here since we have little experience with EOVERFLOW yet.
	 * Elsewhere, getuid() can't fail...
	 */
	olim.rlim_cur = rl.rlim_cur > 0x7fffffff ? 0x7fffffff : rl.rlim_cur;
	olim.rlim_max = rl.rlim_max > 0x7fffffff ? 0x7fffffff : rl.rlim_max;
	error = copyout(&olim, uap->rlp, sizeof(olim));
	return (error);
}
#endif /* COMPAT_43 */

#ifndef _SYS_SYSPROTO_H_
struct __setrlimit_args {
	u_int	which;
	struct	rlimit *rlp;
};
#endif
/*
 * MPSAFE
 */
int
setrlimit(td, uap)
	struct thread *td;
	register struct __setrlimit_args *uap;
{
	struct rlimit alim;
	int error;

	if ((error = copyin(uap->rlp, &alim, sizeof(struct rlimit))))
		return (error);
	error = kern_setrlimit(td, uap->which, &alim);
	return (error);
}

int
kern_setrlimit(td, which, limp)
	struct thread *td;
	u_int which;
	struct rlimit *limp;
{
	struct plimit *newlim, *oldlim;
	struct proc *p;
	register struct rlimit *alimp;
	rlim_t oldssiz;
	int error;

	if (which >= RLIM_NLIMITS)
		return (EINVAL);

	/*
	 * Preserve historical bugs by treating negative limits as unsigned.
	 */
	if (limp->rlim_cur < 0)
		limp->rlim_cur = RLIM_INFINITY;
	if (limp->rlim_max < 0)
		limp->rlim_max = RLIM_INFINITY;

	oldssiz = 0;
	p = td->td_proc;
	newlim = lim_alloc();
	PROC_LOCK(p);
	oldlim = p->p_limit;
	alimp = &oldlim->pl_rlimit[which];
	if (limp->rlim_cur > alimp->rlim_max ||
	    limp->rlim_max > alimp->rlim_max)
		if ((error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL))) {
			PROC_UNLOCK(p);
			lim_free(newlim);
			return (error);
		}
	if (limp->rlim_cur > limp->rlim_max)
		limp->rlim_cur = limp->rlim_max;
	lim_copy(newlim, oldlim);
	alimp = &newlim->pl_rlimit[which];

	switch (which) {

	case RLIMIT_CPU:
		mtx_lock_spin(&sched_lock);
		p->p_cpulimit = limp->rlim_cur;
		mtx_unlock_spin(&sched_lock);
		break;
	case RLIMIT_DATA:
		if (limp->rlim_cur > maxdsiz)
			limp->rlim_cur = maxdsiz;
		if (limp->rlim_max > maxdsiz)
			limp->rlim_max = maxdsiz;
		break;

	case RLIMIT_STACK:
		if (limp->rlim_cur > maxssiz)
			limp->rlim_cur = maxssiz;
		if (limp->rlim_max > maxssiz)
			limp->rlim_max = maxssiz;
		oldssiz = alimp->rlim_cur;
		break;

	case RLIMIT_NOFILE:
		if (limp->rlim_cur > maxfilesperproc)
			limp->rlim_cur = maxfilesperproc;
		if (limp->rlim_max > maxfilesperproc)
			limp->rlim_max = maxfilesperproc;
		break;

	case RLIMIT_NPROC:
		if (limp->rlim_cur > maxprocperuid)
			limp->rlim_cur = maxprocperuid;
		if (limp->rlim_max > maxprocperuid)
			limp->rlim_max = maxprocperuid;
		if (limp->rlim_cur < 1)
			limp->rlim_cur = 1;
		if (limp->rlim_max < 1)
			limp->rlim_max = 1;
		break;
	}
	if (td->td_proc->p_sysent->sv_fixlimit != NULL)
		td->td_proc->p_sysent->sv_fixlimit(limp, which);
	*alimp = *limp;
	p->p_limit = newlim;
	PROC_UNLOCK(p);
	lim_free(oldlim);

	if (which == RLIMIT_STACK) {
		/*
		 * Stack is allocated to the max at exec time with only
		 * "rlim_cur" bytes accessible.  If stack limit is going
		 * up make more accessible, if going down make inaccessible.
		 */
		if (limp->rlim_cur != oldssiz) {
			vm_offset_t addr;
			vm_size_t size;
			vm_prot_t prot;

			if (limp->rlim_cur > oldssiz) {
				prot = p->p_sysent->sv_stackprot;
				size = limp->rlim_cur - oldssiz;
				addr = p->p_sysent->sv_usrstack -
				    limp->rlim_cur;
			} else {
				prot = VM_PROT_NONE;
				size = oldssiz - limp->rlim_cur;
				addr = p->p_sysent->sv_usrstack - oldssiz;
			}
			addr = trunc_page(addr);
			size = round_page(size);
			(void)vm_map_protect(&p->p_vmspace->vm_map,
			    addr, addr + size, prot, FALSE);
		}
	}

	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct __getrlimit_args {
	u_int	which;
	struct	rlimit *rlp;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getrlimit(td, uap)
	struct thread *td;
	register struct __getrlimit_args *uap;
{
	struct rlimit rlim;
	struct proc *p;
	int error;

	if (uap->which >= RLIM_NLIMITS)
		return (EINVAL);
	p = td->td_proc;
	PROC_LOCK(p);
	lim_rlimit(p, uap->which, &rlim);
	PROC_UNLOCK(p);
	error = copyout(&rlim, uap->rlp, sizeof(struct rlimit));
	return (error);
}

/*
 * Transform the running time and tick information in proc p into user,
 * system, and interrupt time usage.
 */
void
calcru(p, up, sp)
	struct proc *p;
	struct timeval *up;
	struct timeval *sp;
{
	struct bintime bt;
	struct rusage_ext rux;
	struct thread *td;
	int bt_valid;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_NOTOWNED);
	bt_valid = 0;
	mtx_lock_spin(&sched_lock);
	rux = p->p_rux;
	FOREACH_THREAD_IN_PROC(p, td) {
		if (TD_IS_RUNNING(td)) {
			/*
			 * Adjust for the current time slice.  This is
			 * actually fairly important since the error here is
			 * on the order of a time quantum which is much
			 * greater than the precision of binuptime().
			 */
			KASSERT(td->td_oncpu != NOCPU,
			    ("%s: running thread has no CPU pid: %d, tid %d",
			     __func__, p->p_pid, td->td_tid));
			if (!bt_valid) {
				binuptime(&bt);
				bt_valid = 1;
			}
			bintime_add(&rux.rux_runtime, &bt);
			bintime_sub(&rux.rux_runtime,
			    &pcpu_find(td->td_oncpu)->pc_switchtime);
		}
	}
	mtx_unlock_spin(&sched_lock);
	calcru1(p, &rux, up, sp);
	p->p_rux.rux_uu = rux.rux_uu;
	p->p_rux.rux_su = rux.rux_su;
	p->p_rux.rux_iu = rux.rux_iu;
}

void
calccru(p, up, sp)
	struct proc *p;
	struct timeval *up;
	struct timeval *sp;
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	calcru1(p, &p->p_crux, up, sp);
}

static void
calcru1(p, ruxp, up, sp)
	struct proc *p;
	struct rusage_ext *ruxp;
	struct timeval *up;
	struct timeval *sp;
{
	struct timeval tv;
	/* {user, system, interrupt, total} {ticks, usec}; previous tu: */
	u_int64_t ut, uu, st, su, it, iu, tt, tu, ptu;

	ut = ruxp->rux_uticks;
	st = ruxp->rux_sticks;
	it = ruxp->rux_iticks;
	tt = ut + st + it;
	if (tt == 0) {
		st = 1;
		tt = 1;
	}
	bintime2timeval(&ruxp->rux_runtime, &tv);
	tu = (u_int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
	ptu = ruxp->rux_uu + ruxp->rux_su + ruxp->rux_iu;
	if (tu + 3 > ptu) {
		/* Numeric slop for low counts */
	} else if (101 * tu > 100 * ptu) {
		/* 1% slop for large counts */
	} else {
		printf(
"calcru: runtime went backwards from %ju usec to %ju usec for pid %d (%s)\n",
		    (uintmax_t)ptu, (uintmax_t)tu, p->p_pid, p->p_comm);
		tu = ptu;
	}
	if ((int64_t)tu < 0) {
		printf("calcru: negative runtime of %jd usec for pid %d (%s)\n",
		    (intmax_t)tu, p->p_pid, p->p_comm);
		tu = ptu;
	}

	/* Subdivide tu. */
	uu = (tu * ut) / tt;
	su = (tu * st) / tt;
	iu = tu - uu - su;

	/* Enforce monotonicity. */
	if (uu < ruxp->rux_uu || su < ruxp->rux_su || iu < ruxp->rux_iu) {
		if (uu < ruxp->rux_uu)
			uu = ruxp->rux_uu;
		else if (uu + ruxp->rux_su + ruxp->rux_iu > tu)
			uu = tu - ruxp->rux_su - ruxp->rux_iu;
		if (st == 0)
			su = ruxp->rux_su;
		else {
			su = ((tu - uu) * st) / (st + it);
			if (su < ruxp->rux_su)
				su = ruxp->rux_su;
			else if (uu + su + ruxp->rux_iu > tu)
				su = tu - uu - ruxp->rux_iu;
		}
		KASSERT(uu + su + ruxp->rux_iu <= tu,
		    ("calcru: monotonisation botch 1"));
		iu = tu - uu - su;
		KASSERT(iu >= ruxp->rux_iu,
		    ("calcru: monotonisation botch 2"));
	}
	ruxp->rux_uu = uu;
	ruxp->rux_su = su;
	ruxp->rux_iu = iu;

	up->tv_sec = uu / 1000000;
	up->tv_usec = uu % 1000000;
	sp->tv_sec = su / 1000000;
	sp->tv_usec = su % 1000000;
}

#ifndef _SYS_SYSPROTO_H_
struct getrusage_args {
	int	who;
	struct	rusage *rusage;
};
#endif
/*
 * MPSAFE
 */
int
getrusage(td, uap)
	register struct thread *td;
	register struct getrusage_args *uap;
{
	struct rusage ru;
	int error;

	error = kern_getrusage(td, uap->who, &ru);
	if (error == 0)
		error = copyout(&ru, uap->rusage, sizeof(struct rusage));
	return (error);
}

int
kern_getrusage(td, who, rup)
	struct thread *td;
	int who;
	struct rusage *rup;
{
	struct proc *p;

	p = td->td_proc;
	PROC_LOCK(p);
	switch (who) {

	case RUSAGE_SELF:
		*rup = p->p_stats->p_ru;
		calcru(p, &rup->ru_utime, &rup->ru_stime);
		break;

	case RUSAGE_CHILDREN:
		*rup = p->p_stats->p_cru;
		calccru(p, &rup->ru_utime, &rup->ru_stime);
		break;

	default:
		PROC_UNLOCK(p);
		return (EINVAL);
	}
	PROC_UNLOCK(p);
	return (0);
}

void
ruadd(ru, rux, ru2, rux2)
	struct rusage *ru;
	struct rusage_ext *rux;
	struct rusage *ru2;
	struct rusage_ext *rux2;
{
	register long *ip, *ip2;
	register int i;

	bintime_add(&rux->rux_runtime, &rux2->rux_runtime);
	rux->rux_uticks += rux2->rux_uticks;
	rux->rux_sticks += rux2->rux_sticks;
	rux->rux_iticks += rux2->rux_iticks;
	rux->rux_uu += rux2->rux_uu;
	rux->rux_su += rux2->rux_su;
	rux->rux_iu += rux2->rux_iu;
	if (ru->ru_maxrss < ru2->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	ip = &ru->ru_first;
	ip2 = &ru2->ru_first;
	for (i = &ru->ru_last - &ru->ru_first; i >= 0; i--)
		*ip++ += *ip2++;
}

/*
 * Allocate a new resource limits structure and initialize its
 * reference count and mutex pointer.
 */
struct plimit *
lim_alloc()
{
	struct plimit *limp;

	limp = malloc(sizeof(struct plimit), M_PLIMIT, M_WAITOK);
	limp->pl_refcnt = 1;
	limp->pl_mtx = mtx_pool_alloc(mtxpool_sleep);
	return (limp);
}

struct plimit *
lim_hold(limp)
	struct plimit *limp;
{

	LIM_LOCK(limp);
	limp->pl_refcnt++;
	LIM_UNLOCK(limp);
	return (limp);
}

void
lim_free(limp)
	struct plimit *limp;
{

	LIM_LOCK(limp);
	KASSERT(limp->pl_refcnt > 0, ("plimit refcnt underflow"));
	if (--limp->pl_refcnt == 0) {
		LIM_UNLOCK(limp);
		free((void *)limp, M_PLIMIT);
		return;
	}
	LIM_UNLOCK(limp);
}

/*
 * Make a copy of the plimit structure.
 * We share these structures copy-on-write after fork.
 */
void
lim_copy(dst, src)
	struct plimit *dst, *src;
{

	KASSERT(dst->pl_refcnt == 1, ("lim_copy to shared limit"));
	bcopy(src->pl_rlimit, dst->pl_rlimit, sizeof(src->pl_rlimit));
}

/*
 * Return the hard limit for a particular system resource.  The
 * which parameter specifies the index into the rlimit array.
 */
rlim_t
lim_max(struct proc *p, int which)
{
	struct rlimit rl;

	lim_rlimit(p, which, &rl);
	return (rl.rlim_max);
}

/*
 * Return the current (soft) limit for a particular system resource.
 * The which parameter which specifies the index into the rlimit array
 */
rlim_t
lim_cur(struct proc *p, int which)
{
	struct rlimit rl;

	lim_rlimit(p, which, &rl);
	return (rl.rlim_cur);
}

/*
 * Return a copy of the entire rlimit structure for the system limit
 * specified by 'which' in the rlimit structure pointed to by 'rlp'.
 */
void
lim_rlimit(struct proc *p, int which, struct rlimit *rlp)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(which >= 0 && which < RLIM_NLIMITS,
	    ("request for invalid resource limit"));
	*rlp = p->p_limit->pl_rlimit[which];
	if (p->p_sysent->sv_fixlimit != NULL)
		p->p_sysent->sv_fixlimit(rlp, which);
}

/*
 * Find the uidinfo structure for a uid.  This structure is used to
 * track the total resource consumption (process count, socket buffer
 * size, etc.) for the uid and impose limits.
 */
void
uihashinit()
{

	uihashtbl = hashinit(maxproc / 16, M_UIDINFO, &uihash);
	mtx_init(&uihashtbl_mtx, "uidinfo hash", NULL, MTX_DEF);
}

/*
 * Look up a uidinfo struct for the parameter uid.
 * uihashtbl_mtx must be locked.
 */
static struct uidinfo *
uilookup(uid)
	uid_t uid;
{
	struct uihashhead *uipp;
	struct uidinfo *uip;

	mtx_assert(&uihashtbl_mtx, MA_OWNED);
	uipp = UIHASH(uid);
	LIST_FOREACH(uip, uipp, ui_hash)
		if (uip->ui_uid == uid)
			break;

	return (uip);
}

/*
 * Find or allocate a struct uidinfo for a particular uid.
 * Increase refcount on uidinfo struct returned.
 * uifree() should be called on a struct uidinfo when released.
 */
struct uidinfo *
uifind(uid)
	uid_t uid;
{
	struct uidinfo *old_uip, *uip;

	mtx_lock(&uihashtbl_mtx);
	uip = uilookup(uid);
	if (uip == NULL) {
		mtx_unlock(&uihashtbl_mtx);
		uip = malloc(sizeof(*uip), M_UIDINFO, M_WAITOK | M_ZERO);
		mtx_lock(&uihashtbl_mtx);
		/*
		 * There's a chance someone created our uidinfo while we
		 * were in malloc and not holding the lock, so we have to
		 * make sure we don't insert a duplicate uidinfo.
		 */
		if ((old_uip = uilookup(uid)) != NULL) {
			/* Someone else beat us to it. */
			free(uip, M_UIDINFO);
			uip = old_uip;
		} else {
			uip->ui_mtxp = mtx_pool_alloc(mtxpool_sleep);
			uip->ui_uid = uid;
			LIST_INSERT_HEAD(UIHASH(uid), uip, ui_hash);
		}
	}
	uihold(uip);
	mtx_unlock(&uihashtbl_mtx);
	return (uip);
}

/*
 * Place another refcount on a uidinfo struct.
 */
void
uihold(uip)
	struct uidinfo *uip;
{

	UIDINFO_LOCK(uip);
	uip->ui_ref++;
	UIDINFO_UNLOCK(uip);
}

/*-
 * Since uidinfo structs have a long lifetime, we use an
 * opportunistic refcounting scheme to avoid locking the lookup hash
 * for each release.
 *
 * If the refcount hits 0, we need to free the structure,
 * which means we need to lock the hash.
 * Optimal case:
 *   After locking the struct and lowering the refcount, if we find
 *   that we don't need to free, simply unlock and return.
 * Suboptimal case:
 *   If refcount lowering results in need to free, bump the count
 *   back up, loose the lock and aquire the locks in the proper
 *   order to try again.
 */
void
uifree(uip)
	struct uidinfo *uip;
{

	/* Prepare for optimal case. */
	UIDINFO_LOCK(uip);

	if (--uip->ui_ref != 0) {
		UIDINFO_UNLOCK(uip);
		return;
	}

	/* Prepare for suboptimal case. */
	uip->ui_ref++;
	UIDINFO_UNLOCK(uip);
	mtx_lock(&uihashtbl_mtx);
	UIDINFO_LOCK(uip);

	/*
	 * We must subtract one from the count again because we backed out
	 * our initial subtraction before dropping the lock.
	 * Since another thread may have added a reference after we dropped the
	 * initial lock we have to test for zero again.
	 */
	if (--uip->ui_ref == 0) {
		LIST_REMOVE(uip, ui_hash);
		mtx_unlock(&uihashtbl_mtx);
		if (uip->ui_sbsize != 0)
			printf("freeing uidinfo: uid = %d, sbsize = %jd\n",
			    uip->ui_uid, (intmax_t)uip->ui_sbsize);
		if (uip->ui_proccnt != 0)
			printf("freeing uidinfo: uid = %d, proccnt = %ld\n",
			    uip->ui_uid, uip->ui_proccnt);
		UIDINFO_UNLOCK(uip);
		FREE(uip, M_UIDINFO);
		return;
	}

	mtx_unlock(&uihashtbl_mtx);
	UIDINFO_UNLOCK(uip);
}

/*
 * Change the count associated with number of processes
 * a given user is using.  When 'max' is 0, don't enforce a limit
 */
int
chgproccnt(uip, diff, max)
	struct	uidinfo	*uip;
	int	diff;
	int	max;
{

	UIDINFO_LOCK(uip);
	/* Don't allow them to exceed max, but allow subtraction. */
	if (diff > 0 && uip->ui_proccnt + diff > max && max != 0) {
		UIDINFO_UNLOCK(uip);
		return (0);
	}
	uip->ui_proccnt += diff;
	if (uip->ui_proccnt < 0)
		printf("negative proccnt for uid = %d\n", uip->ui_uid);
	UIDINFO_UNLOCK(uip);
	return (1);
}

/*
 * Change the total socket buffer size a user has used.
 */
int
chgsbsize(uip, hiwat, to, max)
	struct	uidinfo	*uip;
	u_int  *hiwat;
	u_int	to;
	rlim_t	max;
{
	rlim_t new;

	UIDINFO_LOCK(uip);
	new = uip->ui_sbsize + to - *hiwat;
	/* Don't allow them to exceed max, but allow subtraction. */
	if (to > *hiwat && new > max) {
		UIDINFO_UNLOCK(uip);
		return (0);
	}
	uip->ui_sbsize = new;
	UIDINFO_UNLOCK(uip);
	*hiwat = to;
	if (new < 0)
		printf("negative sbsize for uid = %d\n", uip->ui_uid);
	return (1);
}
