/*
 * Copyright (c) 1991, 1993, 1995
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
 *	@(#)ufs_inode.c	8.9 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_quota.h"
#include "opt_ufs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dirhash.h>
#endif

/*
 * Last reference to an inode.  If necessary, write or delete it.
 */
int
ufs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct thread *td = ap->a_td;
	mode_t mode;
	int error = 0;

	VI_LOCK(vp);
	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_inactive: pushing active", vp);
	VI_UNLOCK(vp);

	/*
	 * Ignore inodes related to stale file handles.
	 */
	if (ip->i_mode == 0)
		goto out;
	if (ip->i_effnlink == 0 && DOINGSOFTDEP(vp))
		softdep_releasefile(ip);
	if (ip->i_nlink <= 0) {
		(void) vn_write_suspend_wait(vp, NULL, V_WAIT);
#ifdef QUOTA
		if (!getinoquota(ip))
			(void)chkiq(ip, -1, NOCRED, FORCE);
#endif
#ifdef UFS_EXTATTR
		ufs_extattr_vnode_inactive(vp, td);
#endif
		error = UFS_TRUNCATE(vp, (off_t)0, IO_EXT | IO_NORMAL,
		    NOCRED, td);
		/*
		 * Setting the mode to zero needs to wait for the inode
		 * to be written just as does a change to the link count.
		 * So, rather than creating a new entry point to do the
		 * same thing, we just use softdep_change_linkcnt().
		 */
		DIP(ip, i_rdev) = 0;
		mode = ip->i_mode;
		ip->i_mode = 0;
		DIP(ip, i_mode) = 0;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (DOINGSOFTDEP(vp))
			softdep_change_linkcnt(ip);
		UFS_VFREE(vp, ip->i_number, mode);
	}
	if (ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) {
		if ((ip->i_flag & (IN_CHANGE | IN_UPDATE | IN_MODIFIED)) == 0 &&
		    vn_write_suspend_wait(vp, NULL, V_NOWAIT)) {
			ip->i_flag &= ~IN_ACCESS;
		} else {
			(void) vn_write_suspend_wait(vp, NULL, V_WAIT);
			UFS_UPDATE(vp, 0);
		}
	}
out:
	VOP_UNLOCK(vp, 0, td);
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (ip->i_mode == 0)
		vrecycle(vp, NULL, td);
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ufs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ufsmount *ump = ip->i_ump;
#ifdef QUOTA
	int i;
#endif

	VI_LOCK(vp);
	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_reclaim: pushing active", vp);
	VI_UNLOCK(vp);
	if (ip->i_flag & IN_LAZYMOD) {
		ip->i_flag |= IN_MODIFIED;
		UFS_UPDATE(vp, 0);
	}
	/*
	 * Remove the inode from its hash chain.
	 */
	ufs_ihashrem(ip);
	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
#ifdef QUOTA
	for (i = 0; i < MAXQUOTAS; i++) {
		if (ip->i_dquot[i] != NODQUOT) {
			dqrele(vp, ip->i_dquot[i]);
			ip->i_dquot[i] = NODQUOT;
		}
	}
#endif
#ifdef UFS_DIRHASH
	if (ip->i_dirhash != NULL)
		ufsdirhash_free(ip);
#endif
	UFS_IFREE(ump, ip);
	vp->v_data = 0;
	return (0);
}
