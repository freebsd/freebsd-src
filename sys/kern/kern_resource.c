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
 *	@(#)kern_resource.c	8.5 (Berkeley) 1/21/94
 * $FreeBSD$
 */

#include "opt_compat.h"
#include "opt_rlimit.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sx.h>
#include <sys/time.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

static int donice __P((struct proc *curp, struct proc *chgp, int n));
/* dosetrlimit non-static:  Needed by SysVR4 emulator */
int dosetrlimit __P((struct proc *p, u_int which, struct rlimit *limp));

static MALLOC_DEFINE(M_UIDINFO, "uidinfo", "uidinfo structures");
#define	UIHASH(uid)	(&uihashtbl[(uid) & uihash])
static struct mtx uihashtbl_mtx;
static LIST_HEAD(uihashhead, uidinfo) *uihashtbl;
static u_long uihash;		/* size of hash table - 1 */

static struct uidinfo	*uilookup __P((uid_t uid));

/*
 * Resource controls and accounting.
 */

#ifndef _SYS_SYSPROTO_H_
struct getpriority_args {
	int	which;
	int	who;
};
#endif
int
getpriority(curp, uap)
	struct proc *curp;
	register struct getpriority_args *uap;
{
	register struct proc *p;
	register int low = PRIO_MAX + 1;

	switch (uap->which) {

	case PRIO_PROCESS:
		if (uap->who == 0)
			p = curp;
		else
			p = pfind(uap->who);
		if (p == 0)
			break;
		if (p_can(curp, p, P_CAN_SEE, NULL))
			break;
		low = p->p_nice;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;

		if (uap->who == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(uap->who)) == NULL)
			break;
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			if (!p_can(curp, p, P_CAN_SEE, NULL) && p->p_nice < low)
				low = p->p_nice;
		}
		break;
	}

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = curp->p_ucred->cr_uid;
		sx_slock(&allproc_lock);
		LIST_FOREACH(p, &allproc, p_list)
			if (!p_can(curp, p, P_CAN_SEE, NULL) &&
			    p->p_ucred->cr_uid == uap->who &&
			    p->p_nice < low)
				low = p->p_nice;
		sx_sunlock(&allproc_lock);
		break;

	default:
		return (EINVAL);
	}
	if (low == PRIO_MAX + 1)
		return (ESRCH);
	curp->p_retval[0] = low;
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setpriority_args {
	int	which;
	int	who;
	int	prio;
};
#endif
/* ARGSUSED */
int
setpriority(curp, uap)
	struct proc *curp;
	register struct setpriority_args *uap;
{
	register struct proc *p;
	int found = 0, error = 0;

	switch (uap->which) {

	case PRIO_PROCESS:
		if (uap->who == 0)
			p = curp;
		else
			p = pfind(uap->who);
		if (p == 0)
			break;
		if (p_can(curp, p, P_CAN_SEE, NULL))
			break;
		error = donice(curp, p, uap->prio);
		found++;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;

		if (uap->who == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(uap->who)) == NULL)
			break;
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			if (!p_can(curp, p, P_CAN_SEE, NULL)) {
				error = donice(curp, p, uap->prio);
				found++;
			}
		}
		break;
	}

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = curp->p_ucred->cr_uid;
		sx_slock(&allproc_lock);
		LIST_FOREACH(p, &allproc, p_list)
			if (p->p_ucred->cr_uid == uap->who &&
			    !p_can(curp, p, P_CAN_SEE, NULL)) {
				error = donice(curp, p, uap->prio);
				found++;
			}
		sx_sunlock(&allproc_lock);
		break;

	default:
		return (EINVAL);
	}
	if (found == 0)
		return (ESRCH);
	return (error);
}

static int
donice(curp, chgp, n)
	register struct proc *curp, *chgp;
	register int n;
{
	int	error;

	if ((error = p_can(curp, chgp, P_CAN_SCHED, NULL)))
		return (error);
	if (n > PRIO_MAX)
		n = PRIO_MAX;
	if (n < PRIO_MIN)
		n = PRIO_MIN;
	if (n < chgp->p_nice && suser(curp))
		return (EACCES);
	chgp->p_nice = n;
	(void)resetpriority(chgp);
	return (0);
}

/* rtprio system call */
#ifndef _SYS_SYSPROTO_H_
struct rtprio_args {
	int		function;
	pid_t		pid;
	struct rtprio	*rtp;
};
#endif

/*
 * Set realtime priority
 */

/* ARGSUSED */
int
rtprio(curp, uap)
	struct proc *curp;
	register struct rtprio_args *uap;
{
	register struct proc *p;
	struct rtprio rtp;
	int error;

	error = copyin(uap->rtp, &rtp, sizeof(struct rtprio));
	if (error)
		return (error);

	if (uap->pid == 0)
		p = curp;
	else
		p = pfind(uap->pid);

	if (p == 0)
		return (ESRCH);

	switch (uap->function) {
	case RTP_LOOKUP:
		if ((error = p_can(curp, p, P_CAN_SEE, NULL)))
			return (error);
		pri_to_rtp(&p->p_pri, &rtp);
		return (copyout(&rtp, uap->rtp, sizeof(struct rtprio)));
	case RTP_SET:
		if ((error = p_can(curp, p, P_CAN_SCHED, NULL)))
		        return (error);
		/* disallow setting rtprio in most cases if not superuser */
		if (suser(curp) != 0) {
			/* can't set someone else's */
			if (uap->pid)
				return (EPERM);
			/* can't set realtime priority */
/*
 * Realtime priority has to be restricted for reasons which should be
 * obvious. However, for idle priority, there is a potential for
 * system deadlock if an idleprio process gains a lock on a resource
 * that other processes need (and the idleprio process can't run
 * due to a CPU-bound normal process). Fix me! XXX
 */
#if 0
 			if (RTP_PRIO_IS_REALTIME(rtp.type))
#endif
			if (rtp.type != RTP_PRIO_NORMAL)
				return (EPERM);
		}
		if (rtp_to_pri(&rtp, &p->p_pri) == 0)
			return (0);
		return (EINVAL);
	default:
		return (EINVAL);
	}
}

int
rtp_to_pri(struct rtprio *rtp, struct priority *pri)
{

	if (rtp->prio > RTP_PRIO_MAX)
		return (-1);
	switch (RTP_PRIO_BASE(rtp->type)) {
	case RTP_PRIO_REALTIME:
		pri->pri_level = PRI_MIN_REALTIME + rtp->prio;
		break;
	case RTP_PRIO_NORMAL:
		pri->pri_level = PRI_MIN_TIMESHARE + rtp->prio;
		break;
	case RTP_PRIO_IDLE:
		pri->pri_level = PRI_MIN_IDLE + rtp->prio;
		break;
	default:
		return (-1);
	}
	pri->pri_class = rtp->type;
	pri->pri_native = pri->pri_level;
	pri->pri_user = pri->pri_level;
	return (0);
}

void
pri_to_rtp(struct priority *pri, struct rtprio *rtp)
{

	switch (PRI_BASE(pri->pri_class)) {
	case PRI_REALTIME:
		rtp->prio = pri->pri_level - PRI_MIN_REALTIME;
		break;
	case PRI_TIMESHARE:
		rtp->prio = pri->pri_level - PRI_MIN_TIMESHARE;
		break;
	case PRI_IDLE:
		rtp->prio = pri->pri_level - PRI_MIN_IDLE;
		break;
	default:
		break;
	}
	rtp->type = pri->pri_class;
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#ifndef _SYS_SYSPROTO_H_
struct osetrlimit_args {
	u_int	which;
	struct	orlimit *rlp;
};
#endif
/* ARGSUSED */
int
osetrlimit(p, uap)
	struct proc *p;
	register struct osetrlimit_args *uap;
{
	struct orlimit olim;
	struct rlimit lim;
	int error;

	if ((error =
	    copyin((caddr_t)uap->rlp, (caddr_t)&olim, sizeof(struct orlimit))))
		return (error);
	lim.rlim_cur = olim.rlim_cur;
	lim.rlim_max = olim.rlim_max;
	return (dosetrlimit(p, uap->which, &lim));
}

#ifndef _SYS_SYSPROTO_H_
struct ogetrlimit_args {
	u_int	which;
	struct	orlimit *rlp;
};
#endif
/* ARGSUSED */
int
ogetrlimit(p, uap)
	struct proc *p;
	register struct ogetrlimit_args *uap;
{
	struct orlimit olim;

	if (uap->which >= RLIM_NLIMITS)
		return (EINVAL);
	olim.rlim_cur = p->p_rlimit[uap->which].rlim_cur;
	if (olim.rlim_cur == -1)
		olim.rlim_cur = 0x7fffffff;
	olim.rlim_max = p->p_rlimit[uap->which].rlim_max;
	if (olim.rlim_max == -1)
		olim.rlim_max = 0x7fffffff;
	return (copyout((caddr_t)&olim, (caddr_t)uap->rlp, sizeof(olim)));
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

#ifndef _SYS_SYSPROTO_H_
struct __setrlimit_args {
	u_int	which;
	struct	rlimit *rlp;
};
#endif
/* ARGSUSED */
int
setrlimit(p, uap)
	struct proc *p;
	register struct __setrlimit_args *uap;
{
	struct rlimit alim;
	int error;

	if ((error =
	    copyin((caddr_t)uap->rlp, (caddr_t)&alim, sizeof (struct rlimit))))
		return (error);
	return (dosetrlimit(p, uap->which, &alim));
}

int
dosetrlimit(p, which, limp)
	struct proc *p;
	u_int which;
	struct rlimit *limp;
{
	register struct rlimit *alimp;
	int error;

	if (which >= RLIM_NLIMITS)
		return (EINVAL);
	alimp = &p->p_rlimit[which];

	/*
	 * Preserve historical bugs by treating negative limits as unsigned.
	 */
	if (limp->rlim_cur < 0)
		limp->rlim_cur = RLIM_INFINITY;
	if (limp->rlim_max < 0)
		limp->rlim_max = RLIM_INFINITY;

	if (limp->rlim_cur > alimp->rlim_max ||
	    limp->rlim_max > alimp->rlim_max)
		if ((error = suser_xxx(0, p, PRISON_ROOT)))
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

	case RLIMIT_CPU:
		if (limp->rlim_cur > RLIM_INFINITY / (rlim_t)1000000)
			p->p_limit->p_cpulimit = RLIM_INFINITY;
		else
			p->p_limit->p_cpulimit = 
			    (rlim_t)1000000 * limp->rlim_cur;
		break;
	case RLIMIT_DATA:
		if (limp->rlim_cur > MAXDSIZ)
			limp->rlim_cur = MAXDSIZ;
		if (limp->rlim_max > MAXDSIZ)
			limp->rlim_max = MAXDSIZ;
		break;

	case RLIMIT_STACK:
		if (limp->rlim_cur > MAXSSIZ)
			limp->rlim_cur = MAXSSIZ;
		if (limp->rlim_max > MAXSSIZ)
			limp->rlim_max = MAXSSIZ;
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
	*alimp = *limp;
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct __getrlimit_args {
	u_int	which;
	struct	rlimit *rlp;
};
#endif
/* ARGSUSED */
int
getrlimit(p, uap)
	struct proc *p;
	register struct __getrlimit_args *uap;
{

	if (uap->which >= RLIM_NLIMITS)
		return (EINVAL);
	return (copyout((caddr_t)&p->p_rlimit[uap->which], (caddr_t)uap->rlp,
	    sizeof (struct rlimit)));
}

/*
 * Transform the running time and tick information in proc p into user,
 * system, and interrupt time usage.
 */
void
calcru(p, up, sp, ip)
	struct proc *p;
	struct timeval *up;
	struct timeval *sp;
	struct timeval *ip;
{
	/* {user, system, interrupt, total} {ticks, usec}; previous tu: */
	u_int64_t ut, uu, st, su, it, iu, tt, tu, ptu;
	int s;
	struct timeval tv;

	mtx_assert(&sched_lock, MA_OWNED);
	/* XXX: why spl-protect ?  worst case is an off-by-one report */
	s = splstatclock();
	ut = p->p_uticks;
	st = p->p_sticks;
	it = p->p_iticks;
	splx(s);

	tt = ut + st + it;
	if (tt == 0) {
		st = 1;
		tt = 1;
	}

	tu = p->p_runtime;
	if (p == curproc) {
		/*
		 * Adjust for the current time slice.  This is actually fairly
		 * important since the error here is on the order of a time
		 * quantum, which is much greater than the sampling error.
		 */
		microuptime(&tv);
		if (timevalcmp(&tv, PCPU_PTR(switchtime), <))
			printf("microuptime() went backwards (%ld.%06ld -> %ld.%06ld)\n",
			    PCPU_GET(switchtime.tv_sec), PCPU_GET(switchtime.tv_usec),
			    tv.tv_sec, tv.tv_usec);
		else
			tu += (tv.tv_usec - PCPU_GET(switchtime.tv_usec)) +
			    (tv.tv_sec - PCPU_GET(switchtime.tv_sec)) *
			    (int64_t)1000000;
	}
	ptu = p->p_uu + p->p_su + p->p_iu;
	if (tu < ptu || (int64_t)tu < 0) {
		/* XXX no %qd in kernel.  Truncate. */
		printf("calcru: negative time of %ld usec for pid %d (%s)\n",
		       (long)tu, p->p_pid, p->p_comm);
		tu = ptu;
	}

	/* Subdivide tu. */
	uu = (tu * ut) / tt;
	su = (tu * st) / tt;
	iu = tu - uu - su;

	/* Enforce monotonicity. */
	if (uu < p->p_uu || su < p->p_su || iu < p->p_iu) {
		if (uu < p->p_uu)
			uu = p->p_uu;
		else if (uu + p->p_su + p->p_iu > tu)
			uu = tu - p->p_su - p->p_iu;
		if (st == 0)
			su = p->p_su;
		else {
			su = ((tu - uu) * st) / (st + it);
			if (su < p->p_su)
				su = p->p_su;
			else if (uu + su + p->p_iu > tu)
				su = tu - uu - p->p_iu;
		}
		KASSERT(uu + su + p->p_iu <= tu,
		    ("calcru: monotonisation botch 1"));
		iu = tu - uu - su;
		KASSERT(iu >= p->p_iu,
		    ("calcru: monotonisation botch 2"));
	}
	p->p_uu = uu;
	p->p_su = su;
	p->p_iu = iu;

	up->tv_sec = uu / 1000000;
	up->tv_usec = uu % 1000000;
	sp->tv_sec = su / 1000000;
	sp->tv_usec = su % 1000000;
	if (ip != NULL) {
		ip->tv_sec = iu / 1000000;
		ip->tv_usec = iu % 1000000;
	}
}

#ifndef _SYS_SYSPROTO_H_
struct getrusage_args {
	int	who;
	struct	rusage *rusage;
};
#endif
/* ARGSUSED */
int
getrusage(p, uap)
	register struct proc *p;
	register struct getrusage_args *uap;
{
	register struct rusage *rup;

	switch (uap->who) {

	case RUSAGE_SELF:
		rup = &p->p_stats->p_ru;
		mtx_lock_spin(&sched_lock);
		calcru(p, &rup->ru_utime, &rup->ru_stime, NULL);
		mtx_unlock_spin(&sched_lock);
		break;

	case RUSAGE_CHILDREN:
		rup = &p->p_stats->p_cru;
		break;

	default:
		return (EINVAL);
	}
	return (copyout((caddr_t)rup, (caddr_t)uap->rusage,
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
	bcopy(lim->pl_rlimit, copy->pl_rlimit, sizeof(struct plimit));
	copy->p_lflags = 0;
	copy->p_refcnt = 1;
	return (copy);
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
	mtx_init(&uihashtbl_mtx, "uidinfo hash", MTX_DEF);
}

/*
 * lookup a uidinfo struct for the parameter uid.
 * uihashtbl_mtx must be locked.
 */
static struct uidinfo *
uilookup(uid)
	uid_t uid;
{
	struct	uihashhead *uipp;
	struct	uidinfo *uip;

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
	struct	uidinfo *uip;

	mtx_lock(&uihashtbl_mtx);
	uip = uilookup(uid);
	if (uip == NULL) {
		struct  uidinfo *old_uip;

		mtx_unlock(&uihashtbl_mtx);
		uip = malloc(sizeof(*uip), M_UIDINFO, M_WAITOK | M_ZERO);
		mtx_lock(&uihashtbl_mtx);
		/*
		 * There's a chance someone created our uidinfo while we
		 * were in malloc and not holding the lock, so we have to
		 * make sure we don't insert a duplicate uidinfo
		 */
		if ((old_uip = uilookup(uid)) != NULL) {
			/* someone else beat us to it */
			free(uip, M_UIDINFO);
			uip = old_uip;
		} else {
			mtx_init(&uip->ui_mtx, "uidinfo struct", MTX_DEF);
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

	mtx_lock(&uip->ui_mtx);
	uip->ui_ref++;
	mtx_unlock(&uip->ui_mtx);
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
	mtx_lock(&uip->ui_mtx);

	if (--uip->ui_ref != 0) {
		mtx_unlock(&uip->ui_mtx);
		return;
	}

	/* Prepare for suboptimal case. */
	uip->ui_ref++;
	mtx_unlock(&uip->ui_mtx);
	mtx_lock(&uihashtbl_mtx);
	mtx_lock(&uip->ui_mtx);

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
			/* XXX no %qd in kernel.  Truncate. */
			printf("freeing uidinfo: uid = %d, sbsize = %ld\n",
			    uip->ui_uid, (long)uip->ui_sbsize);
		if (uip->ui_proccnt != 0)
			printf("freeing uidinfo: uid = %d, proccnt = %ld\n",
			    uip->ui_uid, uip->ui_proccnt);
		mtx_destroy(&uip->ui_mtx);
		FREE(uip, M_UIDINFO);
		return;
	}

	mtx_unlock(&uihashtbl_mtx);
	mtx_unlock(&uip->ui_mtx);
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

	mtx_lock(&uip->ui_mtx);
	/* don't allow them to exceed max, but allow subtraction */
	if (diff > 0 && uip->ui_proccnt + diff > max && max != 0) {
		mtx_unlock(&uip->ui_mtx);
		return (0);
	}
	uip->ui_proccnt += diff;
	if (uip->ui_proccnt < 0)
		printf("negative proccnt for uid = %d\n", uip->ui_uid);
	mtx_unlock(&uip->ui_mtx);
	return (1);
}

/*
 * Change the total socket buffer size a user has used.
 */
int
chgsbsize(uip, hiwat, to, max)
	struct	uidinfo	*uip;
	u_long *hiwat;
	u_long	to;
	rlim_t	max;
{
	rlim_t new;
	int s;

	s = splnet();
	mtx_lock(&uip->ui_mtx);
	new = uip->ui_sbsize + to - *hiwat;
	/* don't allow them to exceed max, but allow subtraction */
	if (to > *hiwat && new > max) {
		splx(s);
		mtx_unlock(&uip->ui_mtx);
		return (0);
	}
	uip->ui_sbsize = new;
	*hiwat = to;
	if (uip->ui_sbsize < 0)
		printf("negative sbsize for uid = %d\n", uip->ui_uid);
	splx(s);
	mtx_unlock(&uip->ui_mtx);
	return (1);
}
