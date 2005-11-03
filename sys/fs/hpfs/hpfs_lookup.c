/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/hpfs/hpfs_lookup.c,v 1.2 2000/05/05 09:57:53 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>

#include <fs/hpfs/hpfs.h>
#include <fs/hpfs/hpfsmount.h>
#include <fs/hpfs/hpfs_subr.h>

int	hpfs_removedirent (struct hpfsmount *, lsn_t, char *, int, int *);

/*
 * This routine traverse the b+ tree representing directory
 * looking for file named 'name'. Returns buf struct and hpfsdirent
 * pointer. Calling routine is supposed to brelse buffer.
 * name is supposed in Unix encodeing.
 */
int
hpfs_genlookupbyname (
	struct hpfsnode *dhp,
	char *name,
	int namelen,
	struct buf **bpp,
	struct hpfsdirent **depp)
{
	struct hpfsmount *hpmp = dhp->h_hpmp;
	struct buf *bp;
	struct dirblk *dp;
	struct hpfsdirent *dep;
	lsn_t lsn;
	int error, res;

	dprintf(("hpfs_genlookupbyname(0x%x, %s (%d)): \n", 
		dhp->h_no, name, namelen));

	lsn = ((alleaf_t *)dhp->h_fn.fn_abd)->al_lsn;
dive:
	error = hpfs_breaddirblk (hpmp, lsn, &bp);
	if (error)
		return (error);

	dp = (struct dirblk *) bp->b_data;
	dep = D_DIRENT(dp);

	while(!(dep->de_flag & DE_END)) {
		dprintf(("no: 0x%x, size: %d, name: %2d:%.*s, flag: 0x%x\n",
			dep->de_fnode, dep->de_size, dep->de_namelen,
			dep->de_namelen, dep->de_name, dep->de_flag));

		res = hpfs_cmpfname(hpmp, name, namelen,
				dep->de_name, dep->de_namelen, dep->de_cpid);
		if (res == 0) {
			*bpp = bp;
			*depp = dep;
			return (0);
		} else if (res < 0)
			break;

		dep = (hpfsdirent_t *)(((caddr_t)dep) + dep->de_reclen);
	}

	if (dep->de_flag & DE_DOWN) {
		lsn = DE_DOWNLSN(dep);
		brelse(bp);
		goto dive;
	}

	brelse(bp);

	return (ENOENT);
}

int
hpfs_makefnode (
	struct vnode * dvp,
	struct vnode ** vpp,
	struct componentname *cnp,
	struct vattr *vap)
{
#ifdef HPFS_DEBUG
	register struct hpfsnode *dhp = VTOHP(dvp);
	dprintf(("hpfs_makefnode(0x%x, %s, %ld): \n",
		dhp->h_no, cnp->cn_nameptr, cnp->cn_namelen));
#endif

	return (EOPNOTSUPP);
}

int
hpfs_removedirent (
	struct hpfsmount *hpmp,
	lsn_t lsn,
	char *name,
	int namelen,
	int *retp)
{
#if 0
	struct buf *bp;
	dirblk_t *dbp;
	struct hpfsdirent *dep;
	int deoff;
	int error, ret;

	dprintf(("hpfs_removedirent(0x%x, %.*s, %d): \n",
		 lsn, namelen, name, namelen));

	error = hpfs_breaddirblk (hpmp, lsn, &bp);
	if (error)
		return (error);

	dbp = (dirblk_t *) bp->b_data;
	deoff = sizeof(dirblk_t);
	dep = DB_DIRENT(dbp);

	while(!(dep->de_flag & DE_END)) {
		dprintf(("no: 0x%x, size: %d, name: %2d:%.*s, flag: 0x%x\n",
			dep->de_fnode, dep->de_size, dep->de_namelen,
			dep->de_namelen, dep->de_name, dep->de_flag));

		res = hpfs_cmpfname(hpmp, name, namelen,
				dep->de_name, dep->de_namelen, dep->de_cpid);
		if (res == 0) {
			if (dep->de_flag & DE_DOWN) {
				/*XXXXXX*/
			} else {
				/* XXX we can copy less */
				bcopy (DE_NEXTDE(dep), dep, DB_BSIZE - deoff - dep->de_reclen);
				dbp->d_freeoff -= dep->de_reclen;
				*retp = 0;
			}
			bdwrite (bp);
			return (0);
		} else if (res < 0)
			break;

		deoff += dep->de_reclen;
		dep = DB_NEXTDE(dep);
	}

	if (dep->de_flag & DE_DOWN) {
		error = hpfs_removede (hpmp, DE_DOWNLSN(dep), name, namelen, &ret);
		if (error) {
			brelse (bp);
			return (error);
		}
		if (ret == 0) {
			if (deoff > sizeof (dirblk_t)) {
			} else if (deoff + dep->de_reclen < dbp->db_freeoff) {
			}
		}
	} else {
		error = ENOENT;
	}

	brelse (bp);
	return (error);
#endif
	return (EOPNOTSUPP);
}

int
hpfs_removefnode (
	struct vnode * dvp,
	struct vnode * vp,
	struct componentname *cnp)
{
#ifdef HPFS_DEBUG
	register struct hpfsnode *dhp = VTOHP(dvp);
	register struct hpfsnode *hp = VTOHP(vp);
	dprintf(("hpfs_removefnode(0x%x, 0x%x, %s, %ld): \n",
		dhp->h_no, hp->h_no, cnp->cn_nameptr, cnp->cn_namelen));
#endif


	return (EOPNOTSUPP);
}
