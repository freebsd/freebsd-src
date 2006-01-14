/*-
 * Copyright (c) 1982, 1986, 1990, 1993, 1995
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
 *	@(#)ufs_quota.c	8.5 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

SYSCTL_DECL(_security_bsd);

static int unprivileged_get_quota = 0;
SYSCTL_INT(_security_bsd, OID_AUTO, unprivileged_get_quota, CTLFLAG_RW,
    &unprivileged_get_quota, 0,
    "Unprivileged processes may retrieve quotas for other uids and gids");

static MALLOC_DEFINE(M_DQUOT, "UFS quota", "UFS quota entries");

/*
 * Quota name to error message mapping.
 */
static char *quotatypes[] = INITQFNAMES;

static int chkdqchg(struct inode *, ufs2_daddr_t, struct ucred *, int);
static int chkiqchg(struct inode *, ino_t, struct ucred *, int);
static int dqget(struct vnode *,
		u_long, struct ufsmount *, int, struct dquot **);
static int dqsync(struct vnode *, struct dquot *);
static void dqflush(struct vnode *);

#ifdef DIAGNOSTIC
static void dqref(struct dquot *);
static void chkdquot(struct inode *);
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
	struct inode *ip;
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
	struct inode *ip;
	ufs2_daddr_t change;
	struct ucred *cred;
	int flags;
{
	struct dquot *dq;
	ufs2_daddr_t ncurblocks;
	int i, error;

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
				(void) tsleep(dq, PINOD+1, "chkdq1", 0);
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
	if ((flags & FORCE) == 0 && suser_cred(cred, 0)) {
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
			(void) tsleep(dq, PINOD+1, "chkdq2", 0);
		}
		/* Reset timer when crossing soft limit */
		if (dq->dq_curblocks + change >= dq->dq_bsoftlimit &&
		    dq->dq_curblocks < dq->dq_bsoftlimit)
			dq->dq_btime = time_second +
			    VFSTOUFS(ITOV(ip)->v_mount)->um_btime[i];
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
	ufs2_daddr_t change;
	struct ucred *cred;
	int type;
{
	struct dquot *dq = ip->i_dquot[type];
	ufs2_daddr_t ncurblocks = dq->dq_curblocks + change;

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
			dq->dq_btime = time_second +
			    VFSTOUFS(ITOV(ip)->v_mount)->um_btime[type];
			if (ip->i_uid == cred->cr_uid)
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "disk quota exceeded");
			return (0);
		}
		if (time_second > dq->dq_btime) {
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
	struct inode *ip;
	ino_t change;
	struct ucred *cred;
	int flags;
{
	struct dquot *dq;
	ino_t ncurinodes;
	int i, error;

#ifdef DIAGNOSTIC
	if ((flags & CHOWN) == 0)
		chkdquot(ip);
#endif
	if (change == 0)
		return (0);
	/* XXX: change is unsigned */
	if (change < 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			while (dq->dq_flags & DQ_LOCK) {
				dq->dq_flags |= DQ_WANT;
				(void) tsleep(dq, PINOD+1, "chkiq1", 0);
			}
			ncurinodes = dq->dq_curinodes + change;
			/* XXX: ncurinodes is unsigned */
			if (ncurinodes >= 0)
				dq->dq_curinodes = ncurinodes;
			else
				dq->dq_curinodes = 0;
			dq->dq_flags &= ~DQ_INODS;
			dq->dq_flags |= DQ_MOD;
		}
		return (0);
	}
	if ((flags & FORCE) == 0 && suser_cred(cred, 0)) {
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
			(void) tsleep(dq, PINOD+1, "chkiq2", 0);
		}
		/* Reset timer when crossing soft limit */
		if (dq->dq_curinodes + change >= dq->dq_isoftlimit &&
		    dq->dq_curinodes < dq->dq_isoftlimit)
			dq->dq_itime = time_second +
			    VFSTOUFS(ITOV(ip)->v_mount)->um_itime[i];
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
	ino_t change;
	struct ucred *cred;
	int type;
{
	struct dquot *dq = ip->i_dquot[type];
	ino_t ncurinodes = dq->dq_curinodes + change;

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
			dq->dq_itime = time_second +
			    VFSTOUFS(ITOV(ip)->v_mount)->um_itime[type];
			if (ip->i_uid == cred->cr_uid)
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "inode quota exceeded");
			return (0);
		}
		if (time_second > dq->dq_itime) {
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
	struct inode *ip;
{
	struct ufsmount *ump = VFSTOUFS(ITOV(ip)->v_mount);
	int i;

	for (i = 0; i < MAXQUOTAS; i++) {
		if (ump->um_quotas[i] == NULLVP ||
		    (ump->um_qflags[i] & (QTF_OPENING|QTF_CLOSING)))
			continue;
		if (ip->i_dquot[i] == NODQUOT) {
			vprint("chkdquot: missing dquot", ITOV(ip));
			panic("chkdquot: missing dquot");
		}
	}
}
#endif

/*
 * Code to process quotactl commands.
 */

/*
 * Q_QUOTAON - set up a quota file for a particular filesystem.
 */
int
quotaon(td, mp, type, fname)
	struct thread *td;
	struct mount *mp;
	int type;
	caddr_t fname;
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct vnode *vp, **vpp;
	struct vnode *mvp;
	struct dquot *dq;
	int error, flags;
	struct nameidata nd;

	error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
	if (error)
		return (error);

	vpp = &ump->um_quotas[type];
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, fname, td);
	flags = FREAD | FWRITE;
	error = vn_open(&nd, &flags, 0, -1);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	VOP_UNLOCK(vp, 0, td);
	if (vp->v_type != VREG) {
		(void) vn_close(vp, FREAD|FWRITE, td->td_ucred, td);
		return (EACCES);
	}
	if (*vpp != vp)
		quotaoff(td, mp, type);
	ump->um_qflags[type] |= QTF_OPENING;
	mp->mnt_flag |= MNT_QUOTA;
	ASSERT_VOP_LOCKED(vp, "quotaon");
	vp->v_vflag |= VV_SYSTEM;
	*vpp = vp;
	/*
	 * Save the credential of the process that turned on quotas.
	 * Set up the time limits for this quota.
	 */
	ump->um_cred[type] = crhold(td->td_ucred);
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
	MNT_ILOCK(mp);
again:
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		VI_LOCK(vp);
		MNT_IUNLOCK(mp);
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td)) {
			MNT_ILOCK(mp);
			MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
			goto again;
		}
		if (vp->v_type == VNON || vp->v_writecount == 0) {
			VOP_UNLOCK(vp, 0, td);
			vrele(vp);
			MNT_ILOCK(mp);
			continue;
		}
		error = getinoquota(VTOI(vp));
		VOP_UNLOCK(vp, 0, td);
		vrele(vp);
		MNT_ILOCK(mp);
		if (error) {
			MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
			break;
		}
	}
	MNT_IUNLOCK(mp);
	ump->um_qflags[type] &= ~QTF_OPENING;
	if (error)
		quotaoff(td, mp, type);
	return (error);
}

/*
 * Q_QUOTAOFF - turn off disk quotas for a filesystem.
 */
int
quotaoff(td, mp, type)
	struct thread *td;
	struct mount *mp;
	int type;
{
	struct vnode *vp;
	struct vnode *qvp, *mvp;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dquot *dq;
	struct inode *ip;
	int error;

	error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
	if (error)
		return (error);

	if ((qvp = ump->um_quotas[type]) == NULLVP)
		return (0);
	ump->um_qflags[type] |= QTF_CLOSING;
	/*
	 * Search vnodes associated with this mount point,
	 * deleting any references to quota file being closed.
	 */
	MNT_ILOCK(mp);
again:
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		VI_LOCK(vp);
		MNT_IUNLOCK(mp);
		if (vp->v_type == VNON) {
			VI_UNLOCK(vp);
			MNT_ILOCK(mp);
			continue;
		}
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td)) {
			MNT_ILOCK(mp);
			MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
			goto again;
		}
		ip = VTOI(vp);
		dq = ip->i_dquot[type];
		ip->i_dquot[type] = NODQUOT;
		dqrele(vp, dq);
		VOP_UNLOCK(vp, 0, td);
		vrele(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	dqflush(qvp);
	ASSERT_VOP_LOCKED(qvp, "quotaoff");
	qvp->v_vflag &= ~VV_SYSTEM;
	error = vn_close(qvp, FREAD|FWRITE, td->td_ucred, td);
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
getquota(td, mp, id, type, addr)
	struct thread *td;
	struct mount *mp;
	u_long id;
	int type;
	caddr_t addr;
{
	struct dquot *dq;
	int error;

	switch (type) {
	case USRQUOTA:
		if ((td->td_ucred->cr_uid != id) && !unprivileged_get_quota) {
			error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
			if (error)
				return (error);
		}
		break;  

	case GRPQUOTA:
		if (!groupmember(id, td->td_ucred) && !unprivileged_get_quota) {
			error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
			if (error)
				return (error);
		}
		break;

	default:
		return (EINVAL);
	}

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
setquota(td, mp, id, type, addr)
	struct thread *td;
	struct mount *mp;
	u_long id;
	int type;
	caddr_t addr;
{
	struct dquot *dq;
	struct dquot *ndq;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dqblk newlim;
	int error;

	error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
	if (error)
		return (error);

	error = copyin(addr, (caddr_t)&newlim, sizeof (struct dqblk));
	if (error)
		return (error);
	error = dqget(NULLVP, id, ump, type, &ndq);
	if (error)
		return (error);
	dq = ndq;
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep(dq, PINOD+1, "setqta", 0);
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
		newlim.dqb_btime = time_second + ump->um_btime[type];
	if (newlim.dqb_isoftlimit &&
	    dq->dq_curinodes >= newlim.dqb_isoftlimit &&
	    (dq->dq_isoftlimit == 0 || dq->dq_curinodes < dq->dq_isoftlimit))
		newlim.dqb_itime = time_second + ump->um_itime[type];
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
setuse(td, mp, id, type, addr)
	struct thread *td;
	struct mount *mp;
	u_long id;
	int type;
	caddr_t addr;
{
	struct dquot *dq;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dquot *ndq;
	struct dqblk usage;
	int error;

	error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
	if (error)
		return (error);

	error = copyin(addr, (caddr_t)&usage, sizeof (struct dqblk));
	if (error)
		return (error);
	error = dqget(NULLVP, id, ump, type, &ndq);
	if (error)
		return (error);
	dq = ndq;
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep(dq, PINOD+1, "setuse", 0);
	}
	/*
	 * Reset time limit if have a soft limit and were
	 * previously under it, but are now over it.
	 */
	if (dq->dq_bsoftlimit && dq->dq_curblocks < dq->dq_bsoftlimit &&
	    usage.dqb_curblocks >= dq->dq_bsoftlimit)
		dq->dq_btime = time_second + ump->um_btime[type];
	if (dq->dq_isoftlimit && dq->dq_curinodes < dq->dq_isoftlimit &&
	    usage.dqb_curinodes >= dq->dq_isoftlimit)
		dq->dq_itime = time_second + ump->um_itime[type];
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
	struct thread *td = curthread;		/* XXX */
	struct vnode *vp, *mvp;
	struct dquot *dq;
	int i, error;

	/*
	 * Check if the mount point has any quotas.
	 * If not, simply return.
	 */
	for (i = 0; i < MAXQUOTAS; i++)
		if (ump->um_quotas[i] != NULLVP)
			break;
	if (i == MAXQUOTAS)
		return (0);
	/*
	 * Search vnodes associated with this mount point,
	 * synchronizing any modified dquot structures.
	 */
	MNT_ILOCK(mp);
again:
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		VI_LOCK(vp);
		MNT_IUNLOCK(mp);
		if (vp->v_type == VNON) {
			VI_UNLOCK(vp);
			MNT_ILOCK(mp);
			continue;
		}
		error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, td);
		if (error) {
			MNT_ILOCK(mp);
			if (error == ENOENT) {
				MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
				goto again;
			}
			continue;
		}
		for (i = 0; i < MAXQUOTAS; i++) {
			dq = VTOI(vp)->i_dquot[i];
			if (dq != NODQUOT && (dq->dq_flags & DQ_MOD))
				dqsync(vp, dq);
		}
		vput(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	return (0);
}

/*
 * Code pertaining to management of the in-core dquot data structures.
 */
#define DQHASH(dqvp, id) \
	(&dqhashtbl[((((intptr_t)(dqvp)) >> 8) + id) & dqhash])
static LIST_HEAD(dqhash, dquot) *dqhashtbl;
static u_long dqhash;

/*
 * Dquot free list.
 */
#define	DQUOTINC	5	/* minimum free dquots desired */
static TAILQ_HEAD(dqfreelist, dquot) dqfreelist;
static long numdquot, desireddquot = DQUOTINC;

/*
 * Initialize the quota system.
 */
void
dqinit()
{

	dqhashtbl = hashinit(desiredvnodes, M_DQUOT, &dqhash);
	TAILQ_INIT(&dqfreelist);
}

/*
 * Shut down the quota system.
 */
void
dquninit()
{
	struct dquot *dq;

	hashdestroy(dqhashtbl, M_DQUOT, dqhash);
	while ((dq = TAILQ_FIRST(&dqfreelist)) != NULL) {
		TAILQ_REMOVE(&dqfreelist, dq, dq_freelist);
		free(dq, M_DQUOT);
	}
}

/*
 * Obtain a dquot structure for the specified identifier and quota file
 * reading the information from the file if necessary.
 */
static int
dqget(vp, id, ump, type, dqp)
	struct vnode *vp;
	u_long id;
	struct ufsmount *ump;
	int type;
	struct dquot **dqp;
{
	struct thread *td = curthread;		/* XXX */
	struct dquot *dq;
	struct dqhash *dqh;
	struct vnode *dqvp;
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
	dqh = DQHASH(dqvp, id);
	LIST_FOREACH(dq, dqh, dq_hash) {
		if (dq->dq_id != id ||
		    dq->dq_ump->um_quotas[dq->dq_type] != dqvp)
			continue;
		/*
		 * Cache hit with no references.  Take
		 * the structure off the free list.
		 */
		if (dq->dq_cnt == 0)
			TAILQ_REMOVE(&dqfreelist, dq, dq_freelist);
		DQREF(dq);
		*dqp = dq;
		return (0);
	}
	/*
	 * Not in cache, allocate a new one.
	 */
	if (TAILQ_FIRST(&dqfreelist) == NODQUOT &&
	    numdquot < MAXQUOTAS * desiredvnodes)
		desireddquot += DQUOTINC;
	if (numdquot < desireddquot) {
		dq = (struct dquot *)malloc(sizeof *dq, M_DQUOT,
		    M_WAITOK | M_ZERO);
		numdquot++;
	} else {
		if ((dq = TAILQ_FIRST(&dqfreelist)) == NULL) {
			tablefull("dquot");
			*dqp = NODQUOT;
			return (EUSERS);
		}
		if (dq->dq_cnt || (dq->dq_flags & DQ_MOD))
			panic("dqget: free dquot isn't");
		TAILQ_REMOVE(&dqfreelist, dq, dq_freelist);
		if (dq->dq_ump != NULL)
			LIST_REMOVE(dq, dq_hash);
	}
	/*
	 * Initialize the contents of the dquot structure.
	 */
	if (vp != dqvp)
		vn_lock(dqvp, LK_EXCLUSIVE | LK_RETRY, td);
	LIST_INSERT_HEAD(dqh, dq, dq_hash);
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
	auio.uio_td = (struct thread *)0;
	error = VOP_READ(dqvp, &auio, 0, ump->um_cred[type]);
	if (auio.uio_resid == sizeof(struct dqblk) && error == 0)
		bzero((caddr_t)&dq->dq_dqb, sizeof(struct dqblk));
	if (vp != dqvp)
		VOP_UNLOCK(dqvp, 0, td);
	if (dq->dq_flags & DQ_WANT)
		wakeup(dq);
	dq->dq_flags = 0;
	/*
	 * I/O error in reading quota file, release
	 * quota structure and reflect problem to caller.
	 */
	if (error) {
		LIST_REMOVE(dq, dq_hash);
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
			dq->dq_btime = time_second + ump->um_btime[type];
		if (dq->dq_itime == 0)
			dq->dq_itime = time_second + ump->um_itime[type];
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
	struct dquot *dq;
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
	TAILQ_INSERT_TAIL(&dqfreelist, dq, dq_freelist);
}

/*
 * Update the disk quota in the quota file.
 */
static int
dqsync(vp, dq)
	struct vnode *vp;
	struct dquot *dq;
{
	struct thread *td = curthread;		/* XXX */
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
	(void) vn_write_suspend_wait(dqvp, NULL, V_WAIT);
	if (vp != dqvp)
		vn_lock(dqvp, LK_EXCLUSIVE | LK_RETRY, td);
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep(dq, PINOD+2, "dqsync", 0);
		if ((dq->dq_flags & DQ_MOD) == 0) {
			if (vp != dqvp)
				VOP_UNLOCK(dqvp, 0, td);
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
	auio.uio_td = (struct thread *)0;
	error = VOP_WRITE(dqvp, &auio, 0, dq->dq_ump->um_cred[dq->dq_type]);
	if (auio.uio_resid && error == 0)
		error = EIO;
	if (dq->dq_flags & DQ_WANT)
		wakeup(dq);
	dq->dq_flags &= ~(DQ_MOD|DQ_LOCK|DQ_WANT);
	if (vp != dqvp)
		VOP_UNLOCK(dqvp, 0, td);
	return (error);
}

/*
 * Flush all entries from the cache for a particular vnode.
 */
static void
dqflush(vp)
	struct vnode *vp;
{
	struct dquot *dq, *nextdq;
	struct dqhash *dqh;

	/*
	 * Move all dquot's that used to refer to this quota
	 * file off their hash chains (they will eventually
	 * fall off the head of the free list and be re-used).
	 */
	for (dqh = &dqhashtbl[dqhash]; dqh >= dqhashtbl; dqh--) {
		for (dq = LIST_FIRST(dqh); dq; dq = nextdq) {
			nextdq = LIST_NEXT(dq, dq_hash);
			if (dq->dq_ump->um_quotas[dq->dq_type] != vp)
				continue;
			if (dq->dq_cnt)
				panic("dqflush: stray dquot");
			LIST_REMOVE(dq, dq_hash);
			dq->dq_ump = (struct ufsmount *)0;
		}
	}
}
