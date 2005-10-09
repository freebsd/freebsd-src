/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)ufs_vfsops.c	8.8 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_quota.h"
#include "opt_ufs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/vnode.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dirhash.h>
#endif

MALLOC_DEFINE(M_UFSMNT, "UFS mount", "UFS mount structure");

/*
 * Return the root of a filesystem.
 */
int
ufs_root(mp, flags, vpp, td)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
	struct thread *td;
{
	struct vnode *nvp;
	int error;

	error = VFS_VGET(mp, (ino_t)ROOTINO, flags, &nvp);
	if (error)
		return (error);
	*vpp = nvp;
	return (0);
}

/*
 * Do operations associated with quotas
 */
int
ufs_quotactl(mp, cmds, uid, arg, td)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct thread *td;
{
#ifndef QUOTA
	return (EOPNOTSUPP);
#else
	int cmd, type, error;

	if (uid == -1)
		uid = td->td_ucred->cr_ruid;
	cmd = cmds >> SUBCMDSHIFT;
	type = cmds & SUBCMDMASK;
	if ((u_int)type >= MAXQUOTAS)
		return (EINVAL);

	if (vfs_busy(mp, LK_NOWAIT, 0, td))
		return (0);

	switch (cmd) {
	case Q_QUOTAON:
		error = quotaon(td, mp, type, arg);
		break;

	case Q_QUOTAOFF:
		error = quotaoff(td, mp, type);
		break;

	case Q_SETQUOTA:
		error = setquota(td, mp, uid, type, arg);
		break;

	case Q_SETUSE:
		error = setuse(td, mp, uid, type, arg);
		break;

	case Q_GETQUOTA:
		error = getquota(td, mp, uid, type, arg);
		break;

	case Q_SYNC:
		error = qsync(mp);
		break;

	default:
		error = EINVAL;
		break;
	}
	vfs_unbusy(mp, td);
	return (error);
#endif
}

/*
 * Initial UFS filesystems, done only once.
 */
int
ufs_init(vfsp)
	struct vfsconf *vfsp;
{

#ifdef QUOTA
	dqinit();
#endif
#ifdef UFS_DIRHASH
	ufsdirhash_init();
#endif
	return (0);
}

/*
 * Uninitialise UFS filesystems, done before module unload.
 */
int
ufs_uninit(vfsp)
	struct vfsconf *vfsp;
{

#ifdef QUOTA
	dquninit();
#endif
#ifdef UFS_DIRHASH
	ufsdirhash_uninit();
#endif
	return (0);
}

/*
 * This is the generic part of fhtovp called after the underlying
 * filesystem has validated the file handle.
 *
 * Call the VFS_CHECKEXP beforehand to verify access.
 */
int
ufs_fhtovp(mp, ufhp, vpp)
	struct mount *mp;
	struct ufid *ufhp;
	struct vnode **vpp;
{
	struct inode *ip;
	struct vnode *nvp;
	int error;

	error = VFS_VGET(mp, ufhp->ufid_ino, LK_EXCLUSIVE, &nvp);
	if (error) {
		*vpp = NULLVP;
		return (error);
	}
	ip = VTOI(nvp);
	if (ip->i_mode == 0 || ip->i_gen != ufhp->ufid_gen ||
	    ip->i_effnlink <= 0) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	vnode_create_vobject(*vpp, DIP(ip, i_size), curthread);
	return (0);
}
