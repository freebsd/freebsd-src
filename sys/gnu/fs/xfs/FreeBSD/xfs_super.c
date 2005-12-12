/*
 * Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_clnt.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_alloc.h"
#include "xfs_dmapi.h"
#include "xfs_quota.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_bit.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_itable.h"
#include "xfs_rw.h"
#include "xfs_acl.h"
#include "xfs_cap.h"
#include "xfs_mac.h"
#include "xfs_attr.h"
#include "xfs_buf_item.h"
#include "xfs_utils.h"
#include "xfs_version.h"

#include <geom/geom.h>
#include <geom/geom_vfs.h>

extern struct vop_vector xfs_fifoops;
extern struct buf_ops xfs_ops;

__uint64_t
xfs_max_file_offset(
	unsigned int		blockshift)
{

	return (OFF_MAX);
}

void
xfs_initialize_vnode(
	bhv_desc_t		*bdp,
	xfs_vnode_t		*vp,
	bhv_desc_t		*inode_bhv,
	int			unlock)
{
	xfs_inode_t		*ip = XFS_BHVTOI(inode_bhv);

	if (!inode_bhv->bd_vobj) {
		vp->v_vfsp = bhvtovfs(bdp);
		bhv_desc_init(inode_bhv, ip, vp, &xfs_vnodeops);
		bhv_insert(VN_BHV_HEAD(vp), inode_bhv);
	}

	/*
	 * XXX: Use VNON as an indication of freshly allocated vnode
	 * which need to be initialized and unlocked.
	 * This is _not_ like the same place in Linux version of
	 * routine.
	 */
	if (vp->v_type != VNON)
		return;

	vp->v_type = IFTOVT(ip->i_d.di_mode);
	vp->v_vnode->v_type = vp->v_type;

	if (vp->v_type == VFIFO)
		vp->v_vnode->v_op = &xfs_fifoops;

	ASSERT_VOP_LOCKED(vp->v_vnode, "xfs_initialize_vnode");

	/* For new inodes we need to set the ops vectors,
	 * and unlock the inode.
	 */
	if (unlock)
		VOP_UNLOCK(vp->v_vnode, 0, curthread);
}

struct vnode *
xfs_get_inode(
	bhv_desc_t	*bdp,
	xfs_ino_t	ino,
	int		flags)
{
	return NULL;
}

void
xfs_flush_inode(
        xfs_inode_t     *ip)
{
	printf("xfs_flush_inode NI\n");
}

void
xfs_flush_device(
        xfs_inode_t     *ip)
{
	printf("xfs_flush_device NI\n");
        xfs_log_force(ip->i_mount, (xfs_lsn_t)0, XFS_LOG_FORCE|XFS_LOG_SYNC);
}

/*ARGSUSED*/
int
xfs_blkdev_get(
	xfs_mount_t		*mp,
	const char		*name,
	struct vnode		**bdevp)
{
	struct nameidata	nd;
	struct nameidata	*ndp = &nd;
	int			error, ronly;
	struct thread		*td;
	struct vnode		*devvp;
	struct g_consumer	*cp;
	struct g_provider	*pp;
	mode_t			accessmode;

	td = curthread;

	NDINIT(ndp, LOOKUP, FOLLOW, UIO_SYSSPACE, name, td);
	if ((error = namei(ndp)) != 0)
		return (error);
	NDFREE(ndp, NDF_ONLY_PNBUF);
	devvp = ndp->ni_vp;

	if (!vn_isdisk(devvp, &error)) {
		vrele(devvp);
		return (error);
	}

	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);

	ronly = ((XFS_MTOVFS(mp)->vfs_flag & VFS_RDONLY) != 0);
	if (suser(td)) {
		accessmode = VREAD;
		if (!ronly)
			accessmode |= VWRITE;
		if ((error = VOP_ACCESS(devvp, accessmode, td->td_ucred, td))!= 0){
			vput(devvp);
			return (error);
		}
	}

	DROP_GIANT();
	g_topology_lock();

	/*
	 * XXX: Do not allow more than one consumer to open a device
	 *      associated with a particular GEOM provider.
	 *      This disables multiple read-only mounts of a device,
	 *      but it gets rid of panics in bmemfree() when you try to
	 *      mount the same device more than once.
	 *      During mounting, XFS does a bread() of the superblock, but does
	 *      not brelse() it.  A subsequent mount of the same device
	 *      will try to bread() the superblock, resulting in a panic in 
	 *      bremfree(), "buffer not on queue".
	 */
	pp = g_dev_getprovider(devvp->v_rdev);
 	if ((pp != NULL) && ((pp->acr | pp->acw | pp->ace ) != 0)) 
		error = EPERM;
	else 
		error = g_vfs_open(devvp, &cp, "xfs", ronly ? 0 : 1);

	g_topology_unlock();
	PICKUP_GIANT();

	VOP_UNLOCK(devvp, 0, td);
	if (error) {
		vput(devvp);
		return (error);
	}

	devvp->v_bufobj.bo_private = cp;
	devvp->v_bufobj.bo_ops = &xfs_ops;

	*bdevp = devvp;
	return (0);
}

void
xfs_blkdev_put(
	struct vnode	*devvp)
{
	struct g_consumer	*cp;

	if (devvp == NULL)
		return;

	vinvalbuf(devvp, V_SAVE, curthread, 0, 0);

	cp = devvp->v_bufobj.bo_private;
	DROP_GIANT();
	g_topology_lock();
	g_wither_geom_close(cp->geom, ENXIO);
	g_topology_unlock();
	PICKUP_GIANT();

        vrele(devvp);
}

void
xfs_flush_buftarg(
	xfs_buftarg_t		*btp)
{
	printf("xfs_flush_buftarg NI %p\n",btp);
}

void
xfs_free_buftarg(
	xfs_buftarg_t		*btp)
{
	xfs_flush_buftarg(btp);
	kmem_free(btp, sizeof(*btp));
}

int
xfs_readonly_buftarg(
	xfs_buftarg_t		*btp)
{
	struct g_consumer *cp;

	KASSERT(btp->specvp->v_bufobj.bo_ops == &xfs_ops,
	   ("Bogus xfs_buftarg_t pointer"));
	cp = btp->specvp->v_bufobj.bo_private;
	
	return (cp->acw == 0);
}

void
xfs_relse_buftarg(
	xfs_buftarg_t		*btp)
{
	printf("xfs_readonly_buftarg NI %p\n",btp);
}

unsigned int
xfs_getsize_buftarg(
	xfs_buftarg_t		*btp)
{
	struct g_consumer	*cp;
	cp = btp->specvp->v_bufobj.bo_private;
	return (cp->provider->sectorsize);
}

void
xfs_setsize_buftarg(
	xfs_buftarg_t		*btp,
	unsigned int		blocksize,
	unsigned int		sectorsize)
{
	printf("xfs_setsize_buftarg NI %p\n",btp);
}

xfs_buftarg_t *
xfs_alloc_buftarg(
	struct vnode	*bdev)
{
	xfs_buftarg_t		*btp;

	btp = kmem_zalloc(sizeof(*btp), KM_SLEEP);

	btp->dev    = bdev->v_rdev;
	btp->specvp = bdev;

	return btp;
}

int
init_xfs_fs( void )
{
	static char		message[] =
		XFS_VERSION_STRING " with " XFS_BUILD_OPTIONS " enabled\n";

	printf(message);

	vn_init();
	xfs_init();
	uuid_init();
	vfs_initdmapi();
	vfs_initquota();

	return 0;
}

void
exit_xfs_fs(void)
{
	xfs_cleanup();
	vfs_exitquota();
	vfs_exitdmapi();
	uuid_cleanup();
}

