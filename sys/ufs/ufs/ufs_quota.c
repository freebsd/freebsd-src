/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
 *	@(#)ufs_quota.c	8.2 (Berkeley) 12/30/93
 * $Id: ufs_quota.c,v 1.7 1996/01/05 18:31:56 wollman Exp $
 */

#include "opt_quota.h"		/* not really necessary... */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

/*
 * Quota name to error message mapping.
 */
static char *quotatypes[] = INITQFNAMES;

static int chkdqchg __P((struct inode *, long, struct ucred *, int));
static int chkiqchg __P((struct inode *, long, struct ucred *, int));
static int dqget __P((struct vnode *,
		u_long, struct ufsmount *, int, struct dquot **));
static int dqsync __P((struct vnode *, struct dquot *));
static void dqflush __P((struct vnode *));

#ifdef DIAGNOSTIC
static void dqref __P((struct dquot *));
static void chkdquot __P((struct inode *));
#endif

/*
 * Set up the quotas for an inode.
 *
 * This routine completely defines the semantics of quotas.
 * If other criterion want to be used to establish quotas, the
 * MAXQUOTAS value in quotas.h should be increased, and the
 * additional dquots set up here.
 */
int
getinoquota(ip)
	register struct inode *ip;
{
	struct ufsmount *ump;
	struct vnode *vp = ITOV(ip);
	int error;

	ump = VFSTOUFS(vp->v_mount);
	/*
	 * Set up the user quota based on file uid.
	 * EINVAL means that quotas are not enabled.
	 */
	if (ip->i_dquot[USRQUOTA] == NODQUOT &&
	    (error =
		dqget(vp, ip->i_uid, ump, USRQUOTA, &ip->i_dquot[USRQUOTA])) &&
	    error != EINVAL)
		return (error);
	/*
	 * Set up the group quota based on file gid.
	 * EINVAL means that quotas are not enabled.
	 */
	if (ip->i_dquot[GRPQUOTA] == NODQUOT &&
	    (error =
		dqget(vp, ip->i_gid, ump, GRPQUOTA, &ip->i_dquot[GRPQUOTA])) &&
	    error != EINVAL)
		return (error);
	return (0);
}

/*
 * Update disk usage, and take corrective action.
 */
int
chkdq(ip, change, cred, flags)
	register struct inode *ip;
	long change;
	struct ucred *cred;
	int flags;
{
	register struct dquot *dq;
	register int i;
	int ncurblocks, error;

#ifdef DIAGNOSTIC
	if ((flags & CHOWN) == 0)
		chkdquot(ip);
#endif
	if (change == 0)
		return (0);
	if (change < 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			while (dq->dq_flags & DQ_LOCK) {
				dq->dq_flags |= DQ_WANT;
				(void) tsleep((caddr_t)dq, PINOD+1, "chkdq1", 0);
			}
			ncurblocks = dq->dq_curblocks + change;
			if (ncurblocks >= 0)
				dq->dq_curblocks = ncurblocks;
			else
				dq->dq_curblocks = 0;
			dq->dq_flags &= ~DQ_BLKS;
			dq->dq_flags |= DQ_MOD;
		}
		return (0);
	}
	if ((flags & FORCE) == 0 && cred->cr_uid != 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			error = chkdqchg(ip, change, cred, i);
			if (error)
				return (error);
		}
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		while (dq->dq_flags & DQ_LOCK) {
			dq->dq_flags |= DQ_WANT;
			(void) tsleep((caddr_t)dq, PINOD+1, "chkdq2", 0);
		}
		dq->dq_curblocks += change;
		dq->dq_flags |= DQ_MOD;
	}
	return (0);
}

/*
 * Check for a valid change to a users allocation.
 * Issue an error message if appropriate.
 */
static int
chkdqchg(ip, change, cred, type)
	struct inode *ip;
	long change;
	struct ucred *cred;
	int type;
{
	register struct dquot *dq = ip->i_dquot[type];
	long ncurblocks = dq->dq_curblocks + change;

	/*
	 * If user would exceed their hard limit, disallow space allocation.
	 */
	if (ncurblocks >= dq->dq_bhardlimit && dq->dq_bhardlimit) {
		if ((dq->dq_flags & DQ_BLKS) == 0 &&
		    ip->i_uid == cred->cr_uid) {
			uprintf("\n%s: write failed, %s disk limit reached\n",
			    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
			    quotatypes[type]);
			dq->dq_flags |= DQ_BLKS;
		}
		return (EDQUOT);
	}
	/*
	 * If user is over their soft limit for too long, disallow space
	 * allocation. Reset time limit as they cross their soft limit.
	 */
	if (ncurblocks >= dq->dq_bsoftlimit && dq->dq_bsoftlimit) {
		if (dq->dq_curblocks < dq->dq_bsoftlimit) {
			dq->dq_btime = time.tv_sec +
			    VFSTOUFS(ITOV(ip)->v_mount)->um_btime[type];
			if (ip->i_uid == cred->cr_uid)
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "disk quota exceeded");
			return (0);
		}
		if (time.tv_sec > dq->dq_btime) {
			if ((dq->dq_flags & DQ_BLKS) == 0 &&
			    ip->i_uid == cred->cr_uid) {
				uprintf("\n%s: write failed, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type],
				    "disk quota exceeded for too long");
				dq->dq_flags |= DQ_BLKS;
			}
			return (EDQUOT);
		}
	}
	return (0);
}

/*
 * Check the inode limit, applying corrective action.
 */
int
chkiq(ip, change, cred, flags)
	register struct inode *ip;
	long change;
	struct ucred *cred;
	int flags;
{
	register struct dquot *dq;
	register int i;
	int ncurinodes, error;

#ifdef DIAGNOSTIC
	if ((flags & CHOWN) == 0)
		chkdquot(ip);
#endif
	if (change == 0)
		return (0);
	if (change < 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			while (dq->dq_flags & DQ_LOCK) {
				dq->dq_flags |= DQ_WANT;
				(void) tsleep((caddr_t)dq, PINOD+1, "chkiq1", 0);
			}
			ncurinodes = dq->dq_curinodes + change;
			if (ncurinodes >= 0)
				dq->dq_curinodes = ncurinodes;
			else
				dq->dq_curinodes = 0;
			dq->dq_flags &= ~DQ_INODS;
			dq->dq_flags |= DQ_MOD;
		}
		return (0);
	}
	if ((flags & FORCE) == 0 && cred->cr_uid != 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			error = chkiqchg(ip, change, cred, i);
			if (error)
				return (error);
		}
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		while (dq->dq_flags & DQ_LOCK) {
			dq->dq_flags |= DQ_WANT;
			(void) tsleep((caddr_t)dq, PINOD+1, "chkiq2", 0);
		}
		dq->dq_curinodes += change;
		dq->dq_flags |= DQ_MOD;
	}
	return (0);
}

/*
 * Check for a valid change to a users allocation.
 * Issue an error message if appropriate.
 */
static int
chkiqchg(ip, change, cred, type)
	struct inode *ip;
	long change;
	struct ucred *cred;
	int type;
{
	register struct dquot *dq = ip->i_dquot[type];
	long ncurinodes = dq->dq_curinodes + change;

	/*
	 * If user would exceed their hard limit, disallow inode allocation.
	 */
	if (ncurinodes >= dq->dq_ihardlimit && dq->dq_ihardlimit) {
		if ((dq->dq_flags & DQ_INODS) == 0 &&
		    ip->i_uid == cred->cr_uid) {
			uprintf("\n%s: write failed, %s inode limit reached\n",
			    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
			    quotatypes[type]);
			dq->dq_flags |= DQ_INODS;
		}
		return (EDQUOT);
	}
	/*
	 * If user is over their soft limit for too long, disallow inode
	 * allocation. Reset time limit as they cross their soft limit.
	 */
	if (ncurinodes >= dq->dq_isoftlimit && dq->dq_isoftlimit) {
		if (dq->dq_curinodes < dq->dq_isoftlimit) {
			dq->dq_itime = time.tv_sec +
			    VFSTOUFS(ITOV(ip)->v_mount)->um_itime[type];
			if (ip->i_uid == cred->cr_uid)
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "inode quota exceeded");
			return (0);
		}
		if (time.tv_sec > dq->dq_itime) {
			if ((dq->dq_flags & DQ_INODS) == 0 &&
			    ip->i_uid == cred->cr_uid) {
				uprintf("\n%s: write failed, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type],
				    "inode quota exceeded for too long");
				dq->dq_flags |= DQ_INODS;
			}
			return (EDQUOT);
		}
	}
	return (0);
}

#ifdef DIAGNOSTIC
/*
 * On filesystems with quotas enabled, it is an error for a file to change
 * size and not to have a dquot structure associated with it.
 */
static void
chkdquot(ip)
	register struct inode *ip;
{
	struct ufsmount *ump = VFSTOUFS(ITOV(ip)->v_mount);
	register int i;

	for (i = 0; i < MAXQUOTAS; i++) {
		if (ump->um_quotas[i] == NULLVP ||
		    (ump->um_qflags[i] & (QTF_OPENING|QTF_CLOSING)))
			continue;
		if (ip->i_dquot[i] == NODQUOT) {
			vprint("chkdquot: missing dquot", ITOV(ip));
			panic("missing dquot");
		}
	}
}
#endif

/*
 * Code to process quotactl commands.
 */

/*
 * Q_QUOTAON - set up a quota file for a particular file system.
 */
int
quotaon(p, mp, type, fname)
	struct proc *p;
	struct mount *mp;
	register int type;
	caddr_t fname;
{
	register struct ufsmount *ump = VFSTOUFS(mp);
	register struct vnode *vp, **vpp;
	struct vnode *nextvp;
	struct dquot *dq;
	int error;
	struct nameidata nd;

	vpp = &ump->um_quotas[type];
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, fname, p);
	error = vn_open(&nd, FREAD|FWRITE, 0);
	if (error)
		return (error);
	vp = nd.ni_vp;
	VOP_UNLOCK(vp);
	if (vp->v_type != VREG) {
		(void) vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (EACCES);
	}
	if (vfs_busy(mp)) {
		(void) vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (EBUSY);
	}
	if (*vpp != vp)
		quotaoff(p, mp, type);
	ump->um_qflags[type] |= QTF_OPENING;
	mp->mnt_flag |= MNT_QUOTA;
	vp->v_flag |= VSYSTEM;
	*vpp = vp;
	/*
	 * Save the credential of the process that turned on quotas.
	 * Set up the time limits for this quota.
	 */
	crhold(p->p_ucred);
	ump->um_cred[type] = p->p_ucred;
	ump->um_btime[type] = MAX_DQ_TIME;
	ump->um_itime[type] = MAX_IQ_TIME;
	if (dqget(NULLVP, 0, ump, type, &dq) == 0) {
		if (dq->dq_btime > 0)
			ump->um_btime[type] = dq->dq_btime;
		if (dq->dq_itime > 0)
			ump->um_itime[type] = dq->dq_itime;
		dqrele(NULLVP, dq);
	}
	/*
	 * Search vnodes associated with this mount point,
	 * adding references to quota file being opened.
	 * NB: only need to add dquot's for inodes being modified.
	 */
again:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nextvp) {
		nextvp = vp->v_mntvnodes.le_next;
		if (vp->v_writecount == 0)
			continue;
		if (vget(vp, 1))
			goto again;
		error = getinoquota(VTOI(vp));
		if (error) {
			vput(vp);
			break;
		}
		vput(vp);
		if (vp->v_mntvnodes.le_next != nextvp || vp->v_mount != mp)
			goto again;
	}
	ump->um_qflags[type] &= ~QTF_OPENING;
	if (error)
		quotaoff(p, mp, type);
	vfs_unbusy(mp);
	return (error);
}

/*
 * Q_QUOTAOFF - turn off disk quotas for a filesystem.
 */
int
quotaoff(p, mp, type)
	struct proc *p;
	struct mount *mp;
	register int type;
{
	register struct vnode *vp;
	struct vnode *qvp, *nextvp;
	struct ufsmount *ump = VFSTOUFS(mp);
	register struct dquot *dq;
	register struct inode *ip;
	int error;

	if ((mp->mnt_flag & MNT_MPBUSY) == 0)
		panic("quotaoff: not busy");
	if ((qvp = ump->um_quotas[type]) == NULLVP)
		return (0);
	ump->um_qflags[type] |= QTF_CLOSING;
	/*
	 * Search vnodes associated with this mount point,
	 * deleting any references to quota file being closed.
	 */
again:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nextvp) {
		nextvp = vp->v_mntvnodes.le_next;
		if (vget(vp, 1))
			goto again;
		ip = VTOI(vp);
		dq = ip->i_dquot[type];
		ip->i_dquot[type] = NODQUOT;
		dqrele(vp, dq);
		vput(vp);
		if (vp->v_mntvnodes.le_next != nextvp || vp->v_mount != mp)
			goto again;
	}
	dqflush(qvp);
	qvp->v_flag &= ~VSYSTEM;
	error = vn_close(qvp, FREAD|FWRITE, p->p_ucred, p);
	ump->um_quotas[type] = NULLVP;
	crfree(ump->um_cred[type]);
	ump->um_cred[type] = NOCRED;
	ump->um_qflags[type] &= ~QTF_CLOSING;
	for (type = 0; type < MAXQUOTAS; type++)
		if (ump->um_quotas[type] != NULLVP)
			break;
	if (type == MAXQUOTAS)
		mp->mnt_flag &= ~MNT_QUOTA;
	return (error);
}

/*
 * Q_GETQUOTA - return current values in a dqblk structure.
 */
int
getquota(mp, id, type, addr)
	struct mount *mp;
	u_long id;
	int type;
	caddr_t addr;
{
	struct dquot *dq;
	int error;

	error = dqget(NULLVP, id, VFSTOUFS(mp), type, &dq);
	if (error)
		return (error);
	error = copyout((caddr_t)&dq->dq_dqb, addr, sizeof (struct dqblk));
	dqrele(NULLVP, dq);
	return (error);
}

/*
 * Q_SETQUOTA - assign an entire dqblk structure.
 */
int
setquota(mp, id, type, addr)
	struct mount *mp;
	u_long id;
	int type;
	caddr_t addr;
{
	register struct dquot *dq;
	struct dquot *ndq;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dqblk newlim;
	int error;

	error = copyin(addr, (caddr_t)&newlim, sizeof (struct dqblk));
	if (error)
		return (error);
	error = dqget(NULLVP, id, ump, type, &ndq);
	if (error)
		return (error);
	dq = ndq;
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep((caddr_t)dq, PINOD+1, "setqta", 0);
	}
	/*
	 * Copy all but the current values.
	 * Reset time limit if previously had no soft limit or were
	 * under it, but now have a soft limit and are over it.
	 */
	newlim.dqb_curblocks = dq->dq_curblocks;
	newlim.dqb_curinodes = dq->dq_curinodes;
	if (dq->dq_id != 0) {
		newlim.dqb_btime = dq->dq_btime;
		newlim.dqb_itime = dq->dq_itime;
	}
	if (newlim.dqb_bsoftlimit &&
	    dq->dq_curblocks >= newlim.dqb_bsoftlimit &&
	    (dq->dq_bsoftlimit == 0 || dq->dq_curblocks < dq->dq_bsoftlimit))
		newlim.dqb_btime = time.tv_sec + ump->um_btime[type];
	if (newlim.dqb_isoftlimit &&
	    dq->dq_curinodes >= newlim.dqb_isoftlimit &&
	    (dq->dq_isoftlimit == 0 || dq->dq_curinodes < dq->dq_isoftlimit))
		newlim.dqb_itime = time.tv_sec + ump->um_itime[type];
	dq->dq_dqb = newlim;
	if (dq->dq_curblocks < dq->dq_bsoftlimit)
		dq->dq_flags &= ~DQ_BLKS;
	if (dq->dq_curinodes < dq->dq_isoftlimit)
		dq->dq_flags &= ~DQ_INODS;
	if (dq->dq_isoftlimit == 0 && dq->dq_bsoftlimit == 0 &&
	    dq->dq_ihardlimit == 0 && dq->dq_bhardlimit == 0)
		dq->dq_flags |= DQ_FAKE;
	else
		dq->dq_flags &= ~DQ_FAKE;
	dq->dq_flags |= DQ_MOD;
	dqrele(NULLVP, dq);
	return (0);
}

/*
 * Q_SETUSE - set current inode and block usage.
 */
int
setuse(mp, id, type, addr)
	struct mount *mp;
	u_long id;
	int type;
	caddr_t addr;
{
	register struct dquot *dq;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dquot *ndq;
	struct dqblk usage;
	int error;

	error = copyin(addr, (caddr_t)&usage, sizeof (struct dqblk));
	if (error)
		return (error);
	error = dqget(NULLVP, id, ump, type, &ndq);
	if (error)
		return (error);
	dq = ndq;
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep((caddr_t)dq, PINOD+1, "setuse", 0);
	}
	/*
	 * Reset time limit if have a soft limit and were
	 * previously under it, but are now over it.
	 */
	if (dq->dq_bsoftlimit && dq->dq_curblocks < dq->dq_bsoftlimit &&
	    usage.dqb_curblocks >= dq->dq_bsoftlimit)
		dq->dq_btime = time.tv_sec + ump->um_btime[type];
	if (dq->dq_isoftlimit && dq->dq_curinodes < dq->dq_isoftlimit &&
	    usage.dqb_curinodes >= dq->dq_isoftlimit)
		dq->dq_itime = time.tv_sec + ump->um_itime[type];
	dq->dq_curblocks = usage.dqb_curblocks;
	dq->dq_curinodes = usage.dqb_curinodes;
	if (dq->dq_curblocks < dq->dq_bsoftlimit)
		dq->dq_flags &= ~DQ_BLKS;
	if (dq->dq_curinodes < dq->dq_isoftlimit)
		dq->dq_flags &= ~DQ_INODS;
	dq->dq_flags |= DQ_MOD;
	dqrele(NULLVP, dq);
	return (0);
}

/*
 * Q_SYNC - sync quota files to disk.
 */
int
qsync(mp)
	struct mount *mp;
{
	struct ufsmount *ump = VFSTOUFS(mp);
	register struct vnode *vp, *nextvp;
	register struct dquot *dq;
	register int i;

	/*
	 * Check if the mount point has any quotas.
	 * If not, simply return.
	 */
	if ((mp->mnt_flag & MNT_MPBUSY) == 0)
		panic("qsync: not busy");
	for (i = 0; i < MAXQUOTAS; i++)
		if (ump->um_quotas[i] != NULLVP)
			break;
	if (i == MAXQUOTAS)
		return (0);
	/*
	 * Search vnodes associated with this mount point,
	 * synchronizing any modified dquot structures.
	 */
again:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nextvp) {
		nextvp = vp->v_mntvnodes.le_next;
		if (VOP_ISLOCKED(vp))
			continue;
		if (vget(vp, 1))
			goto again;
		for (i = 0; i < MAXQUOTAS; i++) {
			dq = VTOI(vp)->i_dquot[i];
			if (dq != NODQUOT && (dq->dq_flags & DQ_MOD))
				dqsync(vp, dq);
		}
		vput(vp);
		if (vp->v_mntvnodes.le_next != nextvp || vp->v_mount != mp)
			goto again;
	}
	return (0);
}

/*
 * Code pertaining to management of the in-core dquot data structures.
 */
static struct dquot **dqhashtbl;
static u_long dqhash;

/*
 * Dquot free list.
 */
#define	DQUOTINC	5	/* minimum free dquots desired */
static struct dquot *dqfreel, **dqback = &dqfreel;
static long numdquot, desireddquot = DQUOTINC;

/*
 * Initialize the quota system.
 */
void
dqinit()
{

	dqhashtbl = hashinit(desiredvnodes, M_DQUOT, &dqhash);
}

/*
 * Obtain a dquot structure for the specified identifier and quota file
 * reading the information from the file if necessary.
 */
static int
dqget(vp, id, ump, type, dqp)
	struct vnode *vp;
	u_long id;
	register struct ufsmount *ump;
	register int type;
	struct dquot **dqp;
{
	register struct dquot *dq, *dp, **dpp;
	register struct vnode *dqvp;
	struct iovec aiov;
	struct uio auio;
	int error;

	dqvp = ump->um_quotas[type];
	if (dqvp == NULLVP || (ump->um_qflags[type] & QTF_CLOSING)) {
		*dqp = NODQUOT;
		return (EINVAL);
	}
	/*
	 * Check the cache first.
	 */
	dpp = &dqhashtbl[((((int)(dqvp)) >> 8) + id) & dqhash];
	for (dq = *dpp; dq; dq = dq->dq_forw) {
		if (dq->dq_id != id ||
		    dq->dq_ump->um_quotas[dq->dq_type] != dqvp)
			continue;
		/*
		 * Cache hit with no references.  Take
		 * the structure off the free list.
		 */
		if (dq->dq_cnt == 0) {
			if ((dp = dq->dq_freef) != NODQUOT)
				dp->dq_freeb = dq->dq_freeb;
			else
				dqback = dq->dq_freeb;
			*dq->dq_freeb = dp;
		}
		DQREF(dq);
		*dqp = dq;
		return (0);
	}
	/*
	 * Not in cache, allocate a new one.
	 */
	if (dqfreel == NODQUOT && numdquot < MAXQUOTAS * desiredvnodes)
		desireddquot += DQUOTINC;
	if (numdquot < desireddquot) {
		dq = (struct dquot *)malloc(sizeof *dq, M_DQUOT, M_WAITOK);
		bzero((char *)dq, sizeof *dq);
		numdquot++;
	} else {
		if ((dq = dqfreel) == NULL) {
			tablefull("dquot");
			*dqp = NODQUOT;
			return (EUSERS);
		}
		if (dq->dq_cnt || (dq->dq_flags & DQ_MOD))
			panic("free dquot isn't");
		if ((dp = dq->dq_freef) != NODQUOT)
			dp->dq_freeb = &dqfreel;
		else
			dqback = &dqfreel;
		dqfreel = dp;
		dq->dq_freef = NULL;
		dq->dq_freeb = NULL;
		dp = dq->dq_forw;
		if (dp)
			dp->dq_back = dq->dq_back;
		*dq->dq_back = dp;
	}
	/*
	 * Initialize the contents of the dquot structure.
	 */
	if (vp != dqvp)
		VOP_LOCK(dqvp);
	dp = *dpp;
	if (dp)
		dp->dq_back = &dq->dq_forw;
	dq->dq_forw = dp;
	dq->dq_back = dpp;
	*dpp = dq;
	DQREF(dq);
	dq->dq_flags = DQ_LOCK;
	dq->dq_id = id;
	dq->dq_ump = ump;
	dq->dq_type = type;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (caddr_t)&dq->dq_dqb;
	aiov.iov_len = sizeof (struct dqblk);
	auio.uio_resid = sizeof (struct dqblk);
	auio.uio_offset = (off_t)(id * sizeof (struct dqblk));
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = (struct proc *)0;
	error = VOP_READ(dqvp, &auio, 0, ump->um_cred[type]);
	if (auio.uio_resid == sizeof(struct dqblk) && error == 0)
		bzero((caddr_t)&dq->dq_dqb, sizeof(struct dqblk));
	if (vp != dqvp)
		VOP_UNLOCK(dqvp);
	if (dq->dq_flags & DQ_WANT)
		wakeup((caddr_t)dq);
	dq->dq_flags = 0;
	/*
	 * I/O error in reading quota file, release
	 * quota structure and reflect problem to caller.
	 */
	if (error) {
		dp = dq->dq_forw;
		if (dp)
			dp->dq_back = dq->dq_back;
		*dq->dq_back = dp;
		dq->dq_forw = NULL;
		dq->dq_back = NULL;
		dqrele(vp, dq);
		*dqp = NODQUOT;
		return (error);
	}
	/*
	 * Check for no limit to enforce.
	 * Initialize time values if necessary.
	 */
	if (dq->dq_isoftlimit == 0 && dq->dq_bsoftlimit == 0 &&
	    dq->dq_ihardlimit == 0 && dq->dq_bhardlimit == 0)
		dq->dq_flags |= DQ_FAKE;
	if (dq->dq_id != 0) {
		if (dq->dq_btime == 0)
			dq->dq_btime = time.tv_sec + ump->um_btime[type];
		if (dq->dq_itime == 0)
			dq->dq_itime = time.tv_sec + ump->um_itime[type];
	}
	*dqp = dq;
	return (0);
}

#ifdef DIAGNOSTIC
/*
 * Obtain a reference to a dquot.
 */
static void
dqref(dq)
	struct dquot *dq;
{

	dq->dq_cnt++;
}
#endif

/*
 * Release a reference to a dquot.
 */
void
dqrele(vp, dq)
	struct vnode *vp;
	register struct dquot *dq;
{

	if (dq == NODQUOT)
		return;
	if (dq->dq_cnt > 1) {
		dq->dq_cnt--;
		return;
	}
	if (dq->dq_flags & DQ_MOD)
		(void) dqsync(vp, dq);
	if (--dq->dq_cnt > 0)
		return;
	if (dqfreel != NODQUOT) {
		*dqback = dq;
		dq->dq_freeb = dqback;
	} else {
		dqfreel = dq;
		dq->dq_freeb = &dqfreel;
	}
	dq->dq_freef = NODQUOT;
	dqback = &dq->dq_freef;
}

/*
 * Update the disk quota in the quota file.
 */
static int
dqsync(vp, dq)
	struct vnode *vp;
	register struct dquot *dq;
{
	struct vnode *dqvp;
	struct iovec aiov;
	struct uio auio;
	int error;

	if (dq == NODQUOT)
		panic("dqsync: dquot");
	if ((dq->dq_flags & DQ_MOD) == 0)
		return (0);
	if ((dqvp = dq->dq_ump->um_quotas[dq->dq_type]) == NULLVP)
		panic("dqsync: file");
	if (vp != dqvp)
		VOP_LOCK(dqvp);
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep((caddr_t)dq, PINOD+2, "dqsync", 0);
		if ((dq->dq_flags & DQ_MOD) == 0) {
			if (vp != dqvp)
				VOP_UNLOCK(dqvp);
			return (0);
		}
	}
	dq->dq_flags |= DQ_LOCK;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (caddr_t)&dq->dq_dqb;
	aiov.iov_len = sizeof (struct dqblk);
	auio.uio_resid = sizeof (struct dqblk);
	auio.uio_offset = (off_t)(dq->dq_id * sizeof (struct dqblk));
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_procp = (struct proc *)0;
	error = VOP_WRITE(dqvp, &auio, 0, dq->dq_ump->um_cred[dq->dq_type]);
	if (auio.uio_resid && error == 0)
		error = EIO;
	if (dq->dq_flags & DQ_WANT)
		wakeup((caddr_t)dq);
	dq->dq_flags &= ~(DQ_MOD|DQ_LOCK|DQ_WANT);
	if (vp != dqvp)
		VOP_UNLOCK(dqvp);
	return (error);
}

/*
 * Flush all entries from the cache for a particular vnode.
 */
static void
dqflush(vp)
	register struct vnode *vp;
{
	register struct dquot *dq, *dp, **dpp, *nextdq;

	/*
	 * Move all dquot's that used to refer to this quota
	 * file off their hash chains (they will eventually
	 * fall off the head of the free list and be re-used).
	 */
	for (dpp = &dqhashtbl[dqhash]; dpp >= dqhashtbl; dpp--) {
		for (dq = *dpp; dq; dq = nextdq) {
			nextdq = dq->dq_forw;
			if (dq->dq_ump->um_quotas[dq->dq_type] != vp)
				continue;
			if (dq->dq_cnt)
				panic("dqflush: stray dquot");
			dp = dq->dq_forw;
			if (dp)
				dp->dq_back = dq->dq_back;
			*dq->dq_back = dp;
			dq->dq_forw = NULL;
			dq->dq_back = NULL;
			dq->dq_ump = (struct ufsmount *)0;
		}
	}
}
