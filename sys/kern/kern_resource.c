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
 *	@(#)kern_resource.c	8.8 (Berkeley) 2/14/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>

int	donice __P((struct proc *curp, struct proc *chgp, int n));
int	dosetrlimit __P((struct proc *p, u_int which, struct rlimit *limp));

/*
 * Resource controls and accounting.
 */

int
getpriority(curp, uap, retval)
	struct proc *curp;
	register struct getpriority_args /* {
		syscallarg(int) which;
		syscallarg(int) who;
	} */ *uap;
	register_t *retval;
{
	register struct proc *p;
	register int low = PRIO_MAX + 1;

	switch (SCARG(uap, which)) {

	case PRIO_PROCESS:
		if (SCARG(uap, who) == 0)
			p = curp;
		else
			p = pfind(SCARG(uap, who));
		if (p == 0)
			break;
		low = p->p_nice;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;

		if (SCARG(uap, who) == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(SCARG(uap, who))) == NULL)
			break;
		for (p = pg->pg_members.lh_first; p != 0;
		     p = p->p_pglist.le_next) {
			if (p->p_nice < low)
				low = p->p_nice;
		}
		break;
	}

	case PRIO_USER:
		if (SCARG(uap, who) == 0)
			SCARG(uap, who) = curp->p_ucred->cr_uid;
		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next)
			if (p->p_ucred->cr_uid == SCARG(uap, who) &&
			    p->p_nice < low)
				low = p->p_nice;
		break;

	default:
		return (EINVAL);
	}
	if (low == PRIO_MAX + 1)
		return (ESRCH);
	*retval = low;
	return (0);
}

/* ARGSUSED */
int
setpriority(curp, uap, retval)
	struct proc *curp;
	register struct setpriority_args /* {
		syscallarg(int) which;
		syscallarg(int) who;
		syscallarg(int) prio;
	} */ *uap;
	register_t *retval;
{
	register struct proc *p;
	int found = 0, error = 0;

	switch (SCARG(uap, which)) {

	case PRIO_PROCESS:
		if (SCARG(uap, who) == 0)
			p = curp;
		else
			p = pfind(SCARG(uap, who));
		if (p == 0)
			break;
		error = donice(curp, p, SCARG(uap, prio));
		found++;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;
		 
		if (SCARG(uap, who) == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(SCARG(uap, who))) == NULL)
			break;
		for (p = pg->pg_members.lh_first; p != 0;
		    p = p->p_pglist.le_next) {
			error = donice(curp, p, SCARG(uap, prio));
			found++;
		}
		break;
	}

	case PRIO_USER:
		if (SCARG(uap, who) == 0)
			SCARG(uap, who) = curp->p_ucred->cr_uid;
		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next)
			if (p->p_ucred->cr_uid == SCARG(uap, who)) {
				error = donice(curp, p, SCARG(uap, prio));
				found++;
			}
		break;

	default:
		return (EINVAL);
	}
	if (found == 0)
		return (ESRCH);
	return (error);
}

int
donice(curp, chgp, n)
	register struct proc *curp, *chgp;
	register int n;
{
	register struct pcred *pcred = curp->p_cred;

	if (pcred->pc_ucred->cr_uid && pcred->p_ruid &&
	    pcred->pc_ucred->cr_uid != chgp->p_ucred->cr_uid &&
	    pcred->p_ruid != chgp->p_ucred->cr_uid)
		return (EPERM);
	if (n > PRIO_MAX)
		n = PRIO_MAX;
	if (n < PRIO_MIN)
		n = PRIO_MIN;
	if (n < chgp->p_nice && suser(pcred->pc_ucred, &curp->p_acflag))
		return (EACCES);
	chgp->p_nice = n;
	(void)resetpriority(chgp);
	return (0);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
/* ARGSUSED */
int
compat_43_setrlimit(p, uap, retval)
	struct proc *p;
	struct compat_43_setrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct ogetrlimit *) rlp;
	} */ *uap;
	register_t *retval;
{
	struct orlimit olim;
	struct rlimit lim;
	int error;

	if (error = copyin((caddr_t)SCARG(uap, rlp), (caddr_t)&olim,
	    sizeof (struct orlimit)))
		return (error);
	lim.rlim_cur = olim.rlim_cur;
	lim.rlim_max = olim.rlim_max;
	return (dosetrlimit(p, SCARG(uap, which), &lim));
}

/* ARGSUSED */
int
compat_43_getrlimit(p, uap, retval)
	struct proc *p;
	register struct compat_43_getrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct ogetrlimit *) rlp;
	} */ *uap;
	register_t *retval;
{
	struct orlimit olim;

	if (SCARG(uap, which) >= RLIM_NLIMITS)
		return (EINVAL);
	olim.rlim_cur = p->p_rlimit[SCARG(uap, which)].rlim_cur;
	if (olim.rlim_cur == -1)
		olim.rlim_cur = 0x7fffffff;
	olim.rlim_max = p->p_rlimit[SCARG(uap, which)].rlim_max;
	if (olim.rlim_max == -1)
		olim.rlim_max = 0x7fffffff;
	return (copyout((caddr_t)&olim, (caddr_t)SCARG(uap, rlp),
	    sizeof(olim)));
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

/* ARGSUSED */
int
setrlimit(p, uap, retval)
	struct proc *p;
	register struct setrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct rlimit *) rlp;
	} */ *uap;
	register_t *retval;
{
	struct rlimit alim;
	int error;

	if (error = copyin((caddr_t)SCARG(uap, rlp), (caddr_t)&alim,
	    sizeof (struct rlimit)))
		return (error);
	return (dosetrlimit(p, SCARG(uap, which), &alim));
}

int
dosetrlimit(p, which, limp)
	struct proc *p;
	u_int which;
	struct rlimit *limp;
{
	register struct rlimit *alimp;
	extern unsigned maxdmap;
	int error;

	if (which >= RLIM_NLIMITS)
		return (EINVAL);
	alimp = &p->p_rlimit[which];
	if (limp->rlim_cur > alimp->rlim_max || 
	    limp->rlim_max > alimp->rlim_max)
		if (error = suser(p->p_ucred, &p->p_acflag))
			return (error);
	if (limp->rlim_cur > limp->rlim_max)
		limp->rlim_cur = limp->rlim_max;
	if (p->p_limit->p_refcnt > 1 &&
	    (p->p_limit->p_lflags & PL_SHAREMOD) == 0) {
		p->p_limit->p_refcnt--;
		p->p_limit = limcopy(p->p_limit);
		alimp = &p->p_rlimit[which];
	}

	switch (which) {

	case RLIMIT_DATA:
		if (limp->rlim_cur > maxdmap)
			limp->rlim_cur = maxdmap;
		if (limp->rlim_max > maxdmap)
			limp->rlim_max = maxdmap;
		break;

	case RLIMIT_STACK:
		if (limp->rlim_cur > maxdmap)
			limp->rlim_cur = maxdmap;
		if (limp->rlim_max > maxdmap)
			limp->rlim_max = maxdmap;
		/*
		 * Stack is allocated to the max at exec time with only
		 * "rlim_cur" bytes accessible.  If stack limit is going
		 * up make more accessible, if going down make inaccessible.
		 */
		if (limp->rlim_cur != alimp->rlim_cur) {
			vm_offset_t addr;
			vm_size_t size;
			vm_prot_t prot;

			if (limp->rlim_cur > alimp->rlim_cur) {
				prot = VM_PROT_ALL;
				size = limp->rlim_cur - alimp->rlim_cur;
				addr = USRSTACK - limp->rlim_cur;
			} else {
				prot = VM_PROT_NONE;
				size = alimp->rlim_cur - limp->rlim_cur;
				addr = USRSTACK - alimp->rlim_cur;
			}
			addr = trunc_page(addr);
			size = round_page(size);
			(void) vm_map_protect(&p->p_vmspace->vm_map,
					      addr, addr+size, prot, FALSE);
		}
		break;

	case RLIMIT_NOFILE:
		if (limp->rlim_cur > maxfiles)
			limp->rlim_cur = maxfiles;
		if (limp->rlim_max > maxfiles)
			limp->rlim_max = maxfiles;
		break;

	case RLIMIT_NPROC:
		if (limp->rlim_cur > maxproc)
			limp->rlim_cur = maxproc;
		if (limp->rlim_max > maxproc)
			limp->rlim_max = maxproc;
		break;
	}
	*alimp = *limp;
	return (0);
}

/* ARGSUSED */
int
getrlimit(p, uap, retval)
	struct proc *p;
	register struct getrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct rlimit *) rlp;
	} */ *uap;
	register_t *retval;
{

	if (SCARG(uap, which) >= RLIM_NLIMITS)
		return (EINVAL);
	return (copyout((caddr_t)&p->p_rlimit[SCARG(uap, which)],
	    (caddr_t)SCARG(uap, rlp), sizeof (struct rlimit)));
}

/*
 * Transform the running time and tick information in proc p into user,
 * system, and interrupt time usage.
 */
void
calcru(p, up, sp, ip)
	register struct proc *p;
	register struct timeval *up;
	register struct timeval *sp;
	register struct timeval *ip;
{
	register u_quad_t u, st, ut, it, tot;
	register u_long sec, usec;
	register int s;
	struct timeval tv;

	s = splstatclock();
	st = p->p_sticks;
	ut = p->p_uticks;
	it = p->p_iticks;
	splx(s);

	tot = st + ut + it;
	if (tot == 0) {
		up->tv_sec = up->tv_usec = 0;
		sp->tv_sec = sp->tv_usec = 0;
		if (ip != NULL)
			ip->tv_sec = ip->tv_usec = 0;
		return;
	}

	sec = p->p_rtime.tv_sec;
	usec = p->p_rtime.tv_usec;
	if (p == curproc) {
		/*
		 * Adjust for the current time slice.  This is actually fairly
		 * important since the error here is on the order of a time
		 * quantum, which is much greater than the sampling error.
		 */
		microtime(&tv);
		sec += tv.tv_sec - runtime.tv_sec;
		usec += tv.tv_usec - runtime.tv_usec;
	}
	u = sec * 1000000 + usec;
	st = (u * st) / tot;
	sp->tv_sec = st / 1000000;
	sp->tv_usec = st % 1000000;
	ut = (u * ut) / tot;
	up->tv_sec = ut / 1000000;
	up->tv_usec = ut % 1000000;
	if (ip != NULL) {
		it = (u * it) / tot;
		ip->tv_sec = it / 1000000;
		ip->tv_usec = it % 1000000;
	}
}

/* ARGSUSED */
int
getrusage(p, uap, retval)
	register struct proc *p;
	register struct getrusage_args /* {
		syscallarg(int) who;
		syscallarg(struct rusage *) rusage;
	} */ *uap;
	register_t *retval;
{
	register struct rusage *rup;

	switch (SCARG(uap, who)) {

	case RUSAGE_SELF:
		rup = &p->p_stats->p_ru;
		calcru(p, &rup->ru_utime, &rup->ru_stime, NULL);
		break;

	case RUSAGE_CHILDREN:
		rup = &p->p_stats->p_cru;
		break;

	default:
		return (EINVAL);
	}
	return (copyout((caddr_t)rup, (caddr_t)SCARG(uap, rusage),
	    sizeof (struct rusage)));
}

void
ruadd(ru, ru2)
	register struct rusage *ru, *ru2;
{
	register long *ip, *ip2;
	register int i;

	timevaladd(&ru->ru_utime, &ru2->ru_utime);
	timevaladd(&ru->ru_stime, &ru2->ru_stime);
	if (ru->ru_maxrss < ru2->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	ip = &ru->ru_first; ip2 = &ru2->ru_first;
	for (i = &ru->ru_last - &ru->ru_first; i >= 0; i--)
		*ip++ += *ip2++;
}

/*
 * Make a copy of the plimit structure.
 * We share these structures copy-on-write after fork,
 * and copy when a limit is changed.
 */
struct plimit *
limcopy(lim)
	struct plimit *lim;
{
	register struct plimit *copy;

	MALLOC(copy, struct plimit *, sizeof(struct plimit),
	    M_SUBPROC, M_WAITOK);
	bcopy(lim->pl_rlimit, copy->pl_rlimit,
	    sizeof(struct rlimit) * RLIM_NLIMITS);
	copy->p_lflags = 0;
	copy->p_refcnt = 1;
	return (copy);
}
