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

#include <linux/blkdev.h>
#include <linux/init.h>

STATIC struct quotactl_ops linvfs_qops;
STATIC struct super_operations linvfs_sops;
STATIC kmem_cache_t * linvfs_inode_cachep;

STATIC struct xfs_mount_args *
xfs_args_allocate(
	struct super_block	*sb)
{
	struct xfs_mount_args	*args;

	args = kmem_zalloc(sizeof(struct xfs_mount_args), KM_SLEEP);
	args->logbufs = args->logbufsize = -1;
	strncpy(args->fsname, bdevname(sb->s_dev), MAXNAMELEN);

	/* Copy the already-parsed mount(2) flags we're interested in */
	if (sb->s_flags & MS_NOATIME)
		args->flags |= XFSMNT_NOATIME;

	/* Default to 32 bit inodes on Linux all the time */
	args->flags |= XFSMNT_32BITINODES;

	return args;
}

__uint64_t
xfs_max_file_offset(
	unsigned int		blockshift)
{
	unsigned int		pagefactor = 1;
	unsigned int		bitshift = BITS_PER_LONG - 1;

	/* Figure out maximum filesize, on Linux this can depend on
	 * the filesystem blocksize (on 32 bit platforms).
	 * __block_prepare_write does this in an [unsigned] long...
	 *      page->index << (PAGE_CACHE_SHIFT - bbits)
	 * So, for page sized blocks (4K on 32 bit platforms),
	 * this wraps at around 8Tb (hence MAX_LFS_FILESIZE which is
	 *      (((u64)PAGE_CACHE_SIZE << (BITS_PER_LONG-1))-1)
	 * but for smaller blocksizes it is less (bbits = log2 bsize).
	 * Note1: get_block_t takes a long (implicit cast from above)
	 * Note2: The Large Block Device (LBD and HAVE_SECTOR_T) patch
	 * can optionally convert the [unsigned] long from above into
	 * an [unsigned] long long.
	 */

#if BITS_PER_LONG == 32
	pagefactor = PAGE_CACHE_SIZE >> (PAGE_CACHE_SHIFT - blockshift);
#endif

	return (((__uint64_t)pagefactor) << bitshift) - 1;
}

STATIC __inline__ void
xfs_set_inodeops(
	struct inode		*inode)
{
	vnode_t			*vp = LINVFS_GET_VP(inode);

	if (vp->v_type == VNON) {
		remove_inode_hash(inode);
		make_bad_inode(inode);
	} else if (S_ISREG(inode->i_mode)) {
		inode->i_op = &linvfs_file_inode_operations;
		inode->i_fop = &linvfs_file_operations;
		inode->i_mapping->a_ops = &linvfs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &linvfs_dir_inode_operations;
		inode->i_fop = &linvfs_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &linvfs_symlink_inode_operations;
		if (inode->i_blocks)
			inode->i_mapping->a_ops = &linvfs_aops;
	} else {
		inode->i_op = &linvfs_file_inode_operations;
		init_special_inode(inode, inode->i_mode,
					kdev_t_to_nr(inode->i_rdev));
	}
}

STATIC __inline__ void
xfs_revalidate_inode(
	xfs_mount_t		*mp,
	vnode_t			*vp,
	xfs_inode_t		*ip)
{
	struct inode		*inode = LINVFS_GET_IP(vp);

	inode->i_mode	= (ip->i_d.di_mode & MODEMASK) | VTTOIF(vp->v_type);
	inode->i_nlink	= ip->i_d.di_nlink;
	inode->i_uid	= ip->i_d.di_uid;
	inode->i_gid	= ip->i_d.di_gid;
	if (((1 << vp->v_type) & ((1<<VBLK) | (1<<VCHR))) == 0) {
		inode->i_rdev = NODEV;
	} else {
		xfs_dev_t dev = ip->i_df.if_u2.if_rdev;
		inode->i_rdev = XFS_DEV_TO_KDEVT(dev);
	}
	inode->i_blksize = PAGE_CACHE_SIZE;
	inode->i_generation = ip->i_d.di_gen;
	i_size_write(inode, ip->i_d.di_size);
	inode->i_blocks =
		XFS_FSB_TO_BB(mp, ip->i_d.di_nblocks + ip->i_delayed_blks);
	inode->i_atime	= ip->i_d.di_atime.t_sec;
	inode->i_mtime	= ip->i_d.di_mtime.t_sec;
	inode->i_ctime	= ip->i_d.di_ctime.t_sec;
	if (ip->i_d.di_flags & XFS_DIFLAG_IMMUTABLE)
		inode->i_flags |= S_IMMUTABLE;
	else
		inode->i_flags &= ~S_IMMUTABLE;
	if (ip->i_d.di_flags & XFS_DIFLAG_APPEND)
		inode->i_flags |= S_APPEND;
	else
		inode->i_flags &= ~S_APPEND;
	if (ip->i_d.di_flags & XFS_DIFLAG_SYNC)
		inode->i_flags |= S_SYNC;
	else
		inode->i_flags &= ~S_SYNC;
	if (ip->i_d.di_flags & XFS_DIFLAG_NOATIME)
		inode->i_flags |= S_NOATIME;
	else
		inode->i_flags &= ~S_NOATIME;

	vp->v_flag &= ~VMODIFIED;
}

void
xfs_initialize_vnode(
	bhv_desc_t		*bdp,
	vnode_t			*vp,
	bhv_desc_t		*inode_bhv,
	int			unlock)
{
	xfs_inode_t		*ip = XFS_BHVTOI(inode_bhv);
	struct inode		*inode = LINVFS_GET_IP(vp);

	if (!inode_bhv->bd_vobj) {
		vp->v_vfsp = bhvtovfs(bdp);
		bhv_desc_init(inode_bhv, ip, vp, &xfs_vnodeops);
		bhv_insert(VN_BHV_HEAD(vp), inode_bhv);
	}

	vp->v_type = IFTOVT(ip->i_d.di_mode);

	/* Have we been called during the new inode create process,
	 * in which case we are too early to fill in the Linux inode.
	 */
	if (vp->v_type == VNON)
		return;

	xfs_revalidate_inode(XFS_BHVTOM(bdp), vp, ip);

	/* For new inodes we need to set the ops vectors,
	 * and unlock the inode.
	 */
	if (unlock && (inode->i_state & I_NEW)) {
		xfs_set_inodeops(inode);
		unlock_new_inode(inode);
	}
}

struct inode *
xfs_get_inode(
	bhv_desc_t	*bdp,
	xfs_ino_t	ino,
	int		flags)
{
	struct vfs	*vfsp = bhvtovfs(bdp);

	return iget_locked(vfsp->vfs_super, ino);
}

void
xfs_flush_inode(
	xfs_inode_t	*ip)
{
	struct inode	*inode = LINVFS_GET_IP(XFS_ITOV(ip));

	filemap_fdatawrite(inode->i_mapping);
}

void
xfs_flush_device(
	xfs_inode_t	*ip)
{
	struct inode	*inode = LINVFS_GET_IP(XFS_ITOV(ip));

	fsync_no_super(inode->i_dev);
	xfs_log_force(ip->i_mount, (xfs_lsn_t)0, XFS_LOG_FORCE|XFS_LOG_SYNC);
}

struct dentry *
d_alloc_anon(struct inode *inode)
{
	struct dentry *dentry;

	spin_lock(&dcache_lock);
	list_for_each_entry(dentry, &inode->i_dentry, d_alias) {
		if (!(dentry->d_flags & DCACHE_NFSD_DISCONNECTED))
			goto found;
	}
	spin_unlock(&dcache_lock);

	dentry = d_alloc_root(inode);
	if (likely(dentry != NULL))
		dentry->d_flags |= DCACHE_NFSD_DISCONNECTED;
	return dentry;
 found:
	dget_locked(dentry);
	dentry->d_vfs_flags |= DCACHE_REFERENCED;
	spin_unlock(&dcache_lock);
	iput(inode);
	return dentry;
}

/*ARGSUSED*/
int
xfs_blkdev_get(
	xfs_mount_t		*mp,
	const char		*name,
	struct block_device	**bdevp)
{
	struct nameidata	nd;
	int			error;

	error = path_lookup(name, LOOKUP_POSITIVE|LOOKUP_FOLLOW, &nd);
	if (error) {
		printk("XFS: Invalid device [%s], error=%d\n", name, error);
		return -error;
	}

	/* I think we actually want bd_acquire here..  --hch */
	*bdevp = bdget(kdev_t_to_nr(nd.dentry->d_inode->i_rdev));
	if (*bdevp)
		error = blkdev_get(*bdevp, FMODE_READ|FMODE_WRITE, 0, BDEV_FS);
	else
		error = -ENOMEM;

	path_release(&nd);
	return -error;
}

void
xfs_blkdev_put(
	struct block_device	*bdev)
{
	if (bdev)
		blkdev_put(bdev, BDEV_FS);
}

void
xfs_flush_buftarg(
	xfs_buftarg_t		*btp)
{
	pagebuf_delwri_flush(btp, PBDF_WAIT, NULL);
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
	return is_read_only(btp->pbr_kdev);
}

void
xfs_relse_buftarg(
	xfs_buftarg_t		*btp)
{
	destroy_buffers(btp->pbr_kdev);
	truncate_inode_pages(btp->pbr_mapping, 0LL);
}

unsigned int
xfs_getsize_buftarg(
	xfs_buftarg_t		*btp)
{
	return block_size(btp->pbr_kdev);
}

void
xfs_setsize_buftarg(
	xfs_buftarg_t		*btp,
	unsigned int		blocksize,
	unsigned int		sectorsize)
{
	btp->pbr_bsize = blocksize;
	btp->pbr_sshift = ffs(sectorsize) - 1;
	btp->pbr_smask = sectorsize - 1;

	if (set_blocksize(btp->pbr_kdev, sectorsize)) {
		printk(KERN_WARNING
			"XFS: Cannot set_blocksize to %u on device 0x%x\n",
			sectorsize, kdev_t_to_nr(btp->pbr_kdev));
	}
}

xfs_buftarg_t *
xfs_alloc_buftarg(
	struct block_device	*bdev)
{
	xfs_buftarg_t		*btp;

	btp = kmem_zalloc(sizeof(*btp), KM_SLEEP);

	btp->pbr_dev =  bdev->bd_dev;
	btp->pbr_kdev = to_kdev_t(btp->pbr_dev);
	btp->pbr_bdev = bdev;
	btp->pbr_mapping = bdev->bd_inode->i_mapping;
	xfs_setsize_buftarg(btp, PAGE_CACHE_SIZE,
			    get_hardsect_size(btp->pbr_kdev));

	switch (MAJOR(btp->pbr_dev)) {
	case MD_MAJOR:
	case EVMS_MAJOR:
		btp->pbr_flags = PBR_ALIGNED_ONLY;
		break;
	case LVM_BLK_MAJOR:
		btp->pbr_flags = PBR_SECTOR_ONLY;
		break;
	}

	return btp;
}

STATIC struct inode *
linvfs_alloc_inode(
	struct super_block	*sb)
{
	vnode_t			*vp;

	vp = (vnode_t *)kmem_cache_alloc(linvfs_inode_cachep, 
                kmem_flags_convert(KM_SLEEP));
	if (!vp)
		return NULL;
	return LINVFS_GET_IP(vp);
}

STATIC void
linvfs_destroy_inode(
	struct inode		*inode)
{
	kmem_cache_free(linvfs_inode_cachep, LINVFS_GET_VP(inode));
}

#define VNODE_SIZE	\
	(sizeof(vnode_t) - sizeof(struct inode) + offsetof(struct inode, u))

STATIC void
init_once(
	void			*data,
	kmem_cache_t		*cachep,
	unsigned long		flags)
{
	vnode_t			*vp = (vnode_t *)data;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR) {
		struct inode *inode = LINVFS_GET_IP(vp);
		memset(vp, 0, VNODE_SIZE);
		__inode_init_once(inode);
	}
}

STATIC int
init_inodecache( void )
{
	linvfs_inode_cachep = kmem_cache_create("linvfs_icache",
				VNODE_SIZE, 0, SLAB_HWCACHE_ALIGN,
				init_once, NULL);

	if (linvfs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

STATIC void
destroy_inodecache( void )
{
	if (kmem_cache_destroy(linvfs_inode_cachep))
		printk(KERN_WARNING "%s: cache still in use!\n", __FUNCTION__);
}

/*
 * Attempt to flush the inode, this will actually fail
 * if the inode is pinned, but we dirty the inode again
 * at the point when it is unpinned after a log write,
 * since this is when the inode itself becomes flushable. 
 */
STATIC void
linvfs_write_inode(
	struct inode		*inode,
	int			sync)
{
	vnode_t			*vp = LINVFS_GET_VP(inode);
	int			error, flags = FLUSH_INODE;

	if (vp) {
		vn_trace_entry(vp, __FUNCTION__, (inst_t *)__return_address);
		if (sync)
			flags |= FLUSH_SYNC;
		VOP_IFLUSH(vp, flags, error);
	}
}

STATIC void
linvfs_clear_inode(
	struct inode		*inode)
{
	vnode_t			*vp = LINVFS_GET_VP(inode);

	if (vp) {
		vn_rele(vp);
		vn_trace_entry(vp, __FUNCTION__, (inst_t *)__return_address);
		/*
		 * Do all our cleanup, and remove this vnode.
		 */
		vn_remove(vp);
	}
}


#define SYNCD_FLAGS	(SYNC_FSDATA|SYNC_BDFLUSH|SYNC_ATTR|SYNC_REFCACHE)

STATIC int
syncd(void *arg)
{
	vfs_t			*vfsp = (vfs_t *) arg;
	int			error;

	daemonize();
	reparent_to_init();
	sigmask_lock();
	sigfillset(&current->blocked);
	__recalc_sigpending(current);
	sigmask_unlock();

	sprintf(current->comm, "xfssyncd");

	vfsp->vfs_sync_task = current;
	wmb();
	wake_up(&vfsp->vfs_wait_sync_task);

	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(xfs_syncd_interval);
		if (vfsp->vfs_flag & VFS_UMOUNT)
			break;
		if (vfsp->vfs_flag & VFS_RDONLY)
			continue;
		VFS_SYNC(vfsp, SYNCD_FLAGS, NULL, error);
	}

	vfsp->vfs_sync_task = NULL;
	wmb();
	wake_up(&vfsp->vfs_wait_sync_task);

	return 0;
}

STATIC int
linvfs_start_syncd(vfs_t *vfsp)
{
	int pid;

	pid = kernel_thread(syncd, (void *) vfsp,
			CLONE_VM | CLONE_FS | CLONE_FILES);
	if (pid < 0)
		return pid;
	wait_event(vfsp->vfs_wait_sync_task, vfsp->vfs_sync_task);
	return 0;
}

STATIC void
linvfs_stop_syncd(vfs_t *vfsp)
{
	vfsp->vfs_flag |= VFS_UMOUNT;
	wmb();

	wake_up_process(vfsp->vfs_sync_task);
	wait_event(vfsp->vfs_wait_sync_task, !vfsp->vfs_sync_task);
}

STATIC void
linvfs_put_super(
	struct super_block	*sb)
{
	vfs_t			*vfsp = LINVFS_GET_VFS(sb);
	int			error;

	linvfs_stop_syncd(vfsp);
	VFS_SYNC(vfsp, SYNC_ATTR|SYNC_DELWRI, NULL, error);
	if (!error)
		VFS_UNMOUNT(vfsp, 0, NULL, error);
	if (error) {
		printk("XFS unmount got error %d\n", error);
		printk("%s: vfsp/0x%p left dangling!\n", __FUNCTION__, vfsp);
		return;
	}

	vfs_deallocate(vfsp);
}

STATIC void
linvfs_write_super(
	struct super_block	*sb)
{
	vfs_t			*vfsp = LINVFS_GET_VFS(sb);
	int			error;

	if (sb->s_flags & MS_RDONLY) {
		sb->s_dirt = 0; /* paranoia */
		return;
	}
	/* Push the log and superblock a little */
	VFS_SYNC(vfsp, SYNC_FSDATA, NULL, error);
	sb->s_dirt = 0;
}

STATIC int
linvfs_sync_super(
	struct super_block	*sb)
{
	vfs_t		*vfsp = LINVFS_GET_VFS(sb);
	int		error;

	VFS_SYNC(vfsp, SYNC_FSDATA|SYNC_WAIT, NULL, error);
	return -error;
}

STATIC int
linvfs_statfs(
	struct super_block	*sb,
	struct statfs		*statp)
{
	vfs_t			*vfsp = LINVFS_GET_VFS(sb);
	int			error;

	VFS_STATVFS(vfsp, statp, NULL, error);
	return -error;
}

STATIC int
linvfs_remount(
	struct super_block	*sb,
	int			*flags,
	char			*options)
{
	vfs_t			*vfsp = LINVFS_GET_VFS(sb);
	struct xfs_mount_args	*args = xfs_args_allocate(sb);
	int			error;

	VFS_PARSEARGS(vfsp, options, args, 1, error);
	if (!error)
		VFS_MNTUPDATE(vfsp, flags, args, error);
	kmem_free(args, sizeof(*args));
	return -error;
}

STATIC void
linvfs_freeze_fs(
	struct super_block	*sb)
{
	vfs_t			*vfsp = LINVFS_GET_VFS(sb);
	vnode_t			*vp;
	int			error;

	if (sb->s_flags & MS_RDONLY)
		return;
	VFS_ROOT(vfsp, &vp, error);
	VOP_IOCTL(vp, LINVFS_GET_IP(vp), NULL, 0, XFS_IOC_FREEZE, 0, error);
	VN_RELE(vp);
}

STATIC void
linvfs_unfreeze_fs(
	struct super_block	*sb)
{
	vfs_t			*vfsp = LINVFS_GET_VFS(sb);
	vnode_t			*vp;
	int			error;

	VFS_ROOT(vfsp, &vp, error);
	VOP_IOCTL(vp, LINVFS_GET_IP(vp), NULL, 0, XFS_IOC_THAW, 0, error);
	VN_RELE(vp);
}

STATIC int
linvfs_dentry_to_fh(
	struct dentry		*dentry,
	__u32			*data,
	int			*lenp,
	int			need_parent)
{
	struct inode		*inode = dentry->d_inode ;
	vnode_t			*vp = LINVFS_GET_VP(inode);
	int			maxlen = *lenp;
	xfs_fid2_t		fid;
	int			error;

	if (maxlen < 3)
		return 255 ;

	VOP_FID2(vp, (struct fid *)&fid, error);
	data[0] = (__u32)fid.fid_ino;	/* 32 bits of inode is OK */
	data[1] = fid.fid_gen;

	*lenp = 2 ;
	if (maxlen < 4 || ! need_parent)
		return 2 ;

	inode = dentry->d_parent->d_inode ;
	vp = LINVFS_GET_VP(inode);

	VOP_FID2(vp, (struct fid *)&fid, error);
	data[2] = (__u32)fid.fid_ino;	/* 32 bits of inode is OK */
	*lenp = 3 ;
	if (maxlen < 4)
		return 3 ;
	data[3] = fid.fid_gen;
	*lenp = 4 ;
	return 4 ;
}

STATIC struct dentry *
linvfs_fh_to_dentry(
	struct super_block	*sb,
	__u32			*data,
	int			len,
	int			fhtype,
	int			parent)
{
	vnode_t			*vp;
	struct inode		*inode = NULL;
	struct dentry		*result;
	xfs_fid2_t		xfid;
	vfs_t			*vfsp = LINVFS_GET_VFS(sb);
	int			error;

	xfid.fid_len = sizeof(xfs_fid2_t) - sizeof(xfid.fid_len);
	xfid.fid_pad = 0;

	if (!parent) {
		xfid.fid_gen = data[1];
		xfid.fid_ino = (__u64)data[0];
	} else {
		if (fhtype == 4)
			xfid.fid_gen = data[3];
		else
			xfid.fid_gen = 0;
		xfid.fid_ino = (__u64)data[2];
	}

	VFS_VGET(vfsp, &vp, (fid_t *)&xfid, error);
	if (error || vp == NULL)
		return ERR_PTR(-ESTALE) ;

	inode = LINVFS_GET_IP(vp);

	result = d_alloc_anon(inode);
	if (unlikely(result == NULL)) {
		iput(inode);
		return ERR_PTR(-ENOMEM);
	}
	return result;
}

STATIC int
linvfs_show_options(
	struct seq_file		*m,
	struct vfsmount		*mnt)
{
	struct vfs		*vfsp = LINVFS_GET_VFS(mnt->mnt_sb);
	int			error;

	VFS_SHOWARGS(vfsp, m, error);
	return error;
}

STATIC int
linvfs_getxstate(
	struct super_block	*sb,
	struct fs_quota_stat	*fqs)
{
	struct vfs		*vfsp = LINVFS_GET_VFS(sb);
	int			error;

	VFS_QUOTACTL(vfsp, Q_XGETQSTAT, 0, (caddr_t)fqs, error);
	return -error;
}

STATIC int
linvfs_setxstate(
	struct super_block	*sb,
	unsigned int		flags,
	int			op)
{
	struct vfs		*vfsp = LINVFS_GET_VFS(sb);
	int			error;

	VFS_QUOTACTL(vfsp, op, 0, (caddr_t)&flags, error);
	return -error;
}

STATIC int
linvfs_getxquota(
	struct super_block	*sb,
	int			type,
	qid_t			id,
	struct fs_disk_quota	*fdq)
{
	struct vfs		*vfsp = LINVFS_GET_VFS(sb);
	int			error, getmode;

	getmode = (type == GRPQUOTA) ? Q_XGETGQUOTA : Q_XGETQUOTA;
	VFS_QUOTACTL(vfsp, getmode, id, (caddr_t)fdq, error);
	return -error;
}

STATIC int
linvfs_setxquota(
	struct super_block	*sb,
	int			type,
	qid_t			id,
	struct fs_disk_quota	*fdq)
{
	struct vfs		*vfsp = LINVFS_GET_VFS(sb);
	int			error, setmode;

	setmode = (type == GRPQUOTA) ? Q_XSETGQLIM : Q_XSETQLIM;
	VFS_QUOTACTL(vfsp, setmode, id, (caddr_t)fdq, error);
	return -error;
}

STATIC struct super_block *
linvfs_read_super(
	struct super_block	*sb,
	void			*data,
	int			silent)
{
	vnode_t			*rootvp;
	struct vfs		*vfsp = vfs_allocate();
	struct xfs_mount_args	*args = xfs_args_allocate(sb);
	struct statfs		statvfs;
	int			error;

	vfsp->vfs_super = sb;
	LINVFS_SET_VFS(sb, vfsp);
	if (sb->s_flags & MS_RDONLY)
		vfsp->vfs_flag |= VFS_RDONLY;
	bhv_insert_all_vfsops(vfsp);

	VFS_PARSEARGS(vfsp, (char *)data, args, 0, error);
	if (error) {
		bhv_remove_all_vfsops(vfsp, 1);
		goto fail_vfsop;
	}

	sb_min_blocksize(sb, BBSIZE);
	sb->s_qcop = &linvfs_qops;
	sb->s_op = &linvfs_sops;

	VFS_MOUNT(vfsp, args, NULL, error);
	if (error) {
		bhv_remove_all_vfsops(vfsp, 1);
		goto fail_vfsop;
	}

	VFS_STATVFS(vfsp, &statvfs, NULL, error);
	if (error)
		goto fail_unmount;

	sb->s_dirt = 1;
	sb->s_magic = statvfs.f_type;
	sb->s_blocksize = statvfs.f_bsize;
	sb->s_blocksize_bits = ffs(statvfs.f_bsize) - 1;
	sb->s_maxbytes = xfs_max_file_offset(sb->s_blocksize_bits);
	set_posix_acl_flag(sb);

	VFS_ROOT(vfsp, &rootvp, error);
	if (error)
		goto fail_unmount;

	sb->s_root = d_alloc_root(LINVFS_GET_IP(rootvp));
	if (!sb->s_root)
		goto fail_vnrele;
	if (is_bad_inode(sb->s_root->d_inode))
		goto fail_vnrele;
	if (linvfs_start_syncd(vfsp))
		goto fail_vnrele;
	vn_trace_exit(rootvp, __FUNCTION__, (inst_t *)__return_address);

	kmem_free(args, sizeof(*args));
	return sb;

fail_vnrele:
	if (sb->s_root) {
		dput(sb->s_root);
		sb->s_root = NULL;
	} else {
		VN_RELE(rootvp);
	}

fail_unmount:
	VFS_UNMOUNT(vfsp, 0, NULL, error);

fail_vfsop:
	vfs_deallocate(vfsp);
	kmem_free(args, sizeof(*args));
	return NULL;
}


STATIC struct super_operations linvfs_sops = {
	.alloc_inode		= linvfs_alloc_inode,
	.destroy_inode		= linvfs_destroy_inode,
	.write_inode		= linvfs_write_inode,
	.clear_inode		= linvfs_clear_inode,
	.put_super		= linvfs_put_super,
	.write_super		= linvfs_write_super,
	.sync_fs		= linvfs_sync_super,
	.write_super_lockfs	= linvfs_freeze_fs,
	.unlockfs		= linvfs_unfreeze_fs,
	.statfs			= linvfs_statfs,
	.remount_fs		= linvfs_remount,
	.fh_to_dentry		= linvfs_fh_to_dentry,
	.dentry_to_fh		= linvfs_dentry_to_fh,
	.show_options		= linvfs_show_options,
};

STATIC struct quotactl_ops linvfs_qops = {
	.get_xstate		= linvfs_getxstate,
	.set_xstate		= linvfs_setxstate,
	.get_xquota		= linvfs_getxquota,
	.set_xquota		= linvfs_setxquota,
};

STATIC struct file_system_type xfs_fs_type = {
	.owner			= THIS_MODULE,
	.name			= "xfs",
	.read_super		= linvfs_read_super,
	.fs_flags		= FS_REQUIRES_DEV,
};


STATIC int __init
init_xfs_fs( void )
{
	int			error;
	struct sysinfo		si;
	static char		message[] __initdata = KERN_INFO \
		XFS_VERSION_STRING " with " XFS_BUILD_OPTIONS " enabled\n";

	printk(message);

	si_meminfo(&si);
	xfs_physmem = si.totalram;

	ktrace_init(64);

	error = init_inodecache();
	if (error < 0)
		goto undo_inodecache;

	error = pagebuf_init();
	if (error < 0)
		goto undo_pagebuf;

	vn_init();
	xfs_init();
	uuid_init();
	vfs_initdmapi();
	vfs_initquota();

	error = register_filesystem(&xfs_fs_type);
	if (error)
		goto undo_register;
	return 0;

undo_register:
	pagebuf_terminate();

undo_pagebuf:
	destroy_inodecache();

undo_inodecache:
	return error;
}

STATIC void __exit
exit_xfs_fs( void )
{
	unregister_filesystem(&xfs_fs_type);
	xfs_cleanup();
	vfs_exitquota();
	vfs_exitdmapi();
	pagebuf_terminate();
	destroy_inodecache();
	ktrace_uninit();
}

module_init(init_xfs_fs);
module_exit(exit_xfs_fs);

MODULE_AUTHOR("Silicon Graphics, Inc.");
MODULE_DESCRIPTION(XFS_VERSION_STRING " with " XFS_BUILD_OPTIONS " enabled");
MODULE_LICENSE("GPL");
