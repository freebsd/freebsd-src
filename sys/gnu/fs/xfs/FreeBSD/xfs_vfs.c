/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_macros.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_clnt.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_imap.h"
#include "xfs_alloc.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_quota.h"

#include "xfs_mountops.h"

int
xvfs_mount(
	struct bhv_desc		*bdp,
	struct xfs_mount_args	*args,
	struct cred		*cr)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_mount)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_mount)(next, args, cr));
}

int
xvfs_parseargs(
	struct bhv_desc		*bdp,
	char			*s,
	struct xfs_mount_args	*args,
	int			f)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_parseargs)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_parseargs)(next, s, args, f));
}

int
xvfs_showargs(
	struct bhv_desc		*bdp,
	struct sbuf		*m)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_showargs)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_showargs)(next, m));
}

int
xvfs_unmount(
	struct bhv_desc		*bdp,
	int			fl,
	struct cred		*cr)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_unmount)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_unmount)(next, fl, cr));
}

int
xvfs_mntupdate(
	struct bhv_desc		*bdp,
	int			*fl,
	struct xfs_mount_args	*args)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_mntupdate)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_mntupdate)(next, fl, args));
}

int
xvfs_root(
	struct bhv_desc		*bdp,
	struct xfs_vnode	**vpp)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_root)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_root)(next, vpp));
}

int
xvfs_statvfs(
	struct bhv_desc		*bdp,
	struct statfs		*sp,
	struct xfs_vnode	*vp)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_statvfs)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_statvfs)(next, sp, vp));
}

int
xvfs_sync(
	struct bhv_desc		*bdp,
	int			fl,
	struct cred		*cr)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_sync)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_sync)(next, fl, cr));
}

int
xvfs_vget(
	struct bhv_desc		*bdp,
	struct xfs_vnode	**vpp,
	struct fid		*fidp)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_vget)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_vget)(next, vpp, fidp));
}

int
xvfs_dmapiops(
	struct bhv_desc		*bdp,
	caddr_t			addr)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_dmapiops)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_dmapiops)(next, addr));
}

int
xvfs_quotactl(
	struct bhv_desc		*bdp,
	int			cmd,
	int			id,
	caddr_t			addr)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_quotactl)
		next = BHV_NEXT(next);
	return ((*bhvtovfsops(next)->xvfs_quotactl)(next, cmd, id, addr));
}

struct inode *
xvfs_get_inode(
	struct bhv_desc		*bdp,
	xfs_ino_t		ino,
	int			fl)
{
	struct bhv_desc		*next = bdp;

	while (! (bhvtovfsops(next))->xvfs_get_inode)
		next = BHV_NEXTNULL(next);
	return ((*bhvtovfsops(next)->xvfs_get_inode)(next, ino, fl));
}

void
xvfs_init_vnode(
	struct bhv_desc		*bdp,
	struct xfs_vnode	*vp,
	struct bhv_desc		*bp,
	int			unlock)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_init_vnode)
		next = BHV_NEXT(next);
	((*bhvtovfsops(next)->xvfs_init_vnode)(next, vp, bp, unlock));
}

void
xvfs_force_shutdown(
	struct bhv_desc		*bdp,
	int			fl,
	char			*file,
	int			line)
{
	struct bhv_desc		*next = bdp;

	ASSERT(next);
	while (! (bhvtovfsops(next))->xvfs_force_shutdown)
		next = BHV_NEXT(next);
	((*bhvtovfsops(next)->xvfs_force_shutdown)(next, fl, file, line));
}

xfs_vfs_t *
vfs_allocate(struct mount *mp)
{
	struct xfs_vfs	 *vfsp;
	struct xfsmount  *xmp;

	xmp  = kmem_zalloc(sizeof(*xmp), KM_SLEEP);
	vfsp = XFSTOVFS(xmp);

	bhv_head_init(VFS_BHVHEAD(vfsp), "vfs");

	xmp->m_mp = mp;
	mp->mnt_data = (qaddr_t)xmp;
	vfsp->vfs_mp = mp;

	return vfsp;
}

void
vfs_deallocate(
	struct xfs_vfs		*vfsp)
{
	struct xfsmount *xmp;

	bhv_head_destroy(VFS_BHVHEAD(vfsp));

	xmp = VFSTOXFS(vfsp);
	kmem_free(xmp, sizeof(*xmp));
}

/*
 * Allocate and initialize a new XFS mount structure
 */
struct xfsmount *
xfsmount_allocate(struct mount *mp)
{
	xfs_vfs_t	*vfsp;

	vfsp = vfs_allocate(mp);

	ASSERT(vfsp);

	if (mp->mnt_flag & MNT_RDONLY)
		vfsp->vfs_flag |= VFS_RDONLY;

	bhv_insert_all_vfsops(vfsp);
	return (VFSTOXFS(vfsp));
}

void
xfsmount_deallocate(struct xfsmount *xmp)
{
	xfs_vfs_t	*vfsp;

	vfsp = XFSTOVFS(xmp);
	bhv_remove_all_vfsops(vfsp, 1);
	vfs_deallocate(vfsp);
}


void
vfs_insertops(
	struct xfs_vfs		*vfsp,
	struct bhv_vfsops	*vfsops)
{
	struct bhv_desc		*bdp;

	bdp = kmem_alloc(sizeof(struct bhv_desc), KM_SLEEP);
	bhv_desc_init(bdp, NULL, vfsp, vfsops);
	bhv_insert(&vfsp->vfs_bh, bdp);
}

void
vfs_insertbhv(
	struct xfs_vfs		*vfsp,
	struct bhv_desc		*bdp,
	struct xvfsops		*vfsops,
	void			*mount)
{
	bhv_desc_init(bdp, mount, vfsp, vfsops);
	bhv_insert_initial(&vfsp->vfs_bh, bdp);
}

void
bhv_remove_vfsops(
	struct xfs_vfs		*vfsp,
	int			pos)
{
	struct bhv_desc		*bhv;

	bhv = bhv_lookup_range(&vfsp->vfs_bh, pos, pos);
	if (bhv) {
		bhv_remove(&vfsp->vfs_bh, bhv);
		kmem_free(bhv, sizeof(*bhv));
	}
}

void
bhv_remove_all_vfsops(
	struct xfs_vfs		*vfsp,
	int			freebase)
{
	struct xfs_mount	*mp;

	bhv_remove_vfsops(vfsp, VFS_POSITION_QM);
	bhv_remove_vfsops(vfsp, VFS_POSITION_DM);
	bhv_remove_vfsops(vfsp, VFS_POSITION_IO);
	if (!freebase)
		return;
	mp = XFS_BHVTOM(bhv_lookup(VFS_BHVHEAD(vfsp), &xfs_vfsops));
	VFS_REMOVEBHV(vfsp, &mp->m_bhv);
	xfs_mount_free(mp, 0);
}

void
bhv_insert_all_vfsops(
	struct xfs_vfs		*vfsp)
{
	struct xfs_mount	*mp;

	mp = xfs_mount_init();
	vfs_insertbhv(vfsp, &mp->m_bhv, &xfs_vfsops, mp);
        vfs_insertdmapi(vfsp);
        vfs_insertquota(vfsp);
}
