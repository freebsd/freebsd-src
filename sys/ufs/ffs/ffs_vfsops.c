/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#include "opt_quota.h"
#include "opt_ufs.h"
#include "opt_ffs.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/gsb_crc32.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <security/mac/mac_framework.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/gjournal.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <vm/vm.h>
#include <vm/uma.h>
#include <vm/vm_page.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include <ddb/ddb.h>

static uma_zone_t uma_inode, uma_ufs1, uma_ufs2;
VFS_SMR_DECLARE;

static int	ffs_mountfs(struct vnode *, struct mount *, struct thread *);
static void	ffs_ifree(struct ufsmount *ump, struct inode *ip);
static int	ffs_sync_lazy(struct mount *mp);
static int	ffs_use_bread(void *devfd, off_t loc, void **bufp, int size);
static int	ffs_use_bwrite(void *devfd, off_t loc, void *buf, int size);

static vfs_init_t ffs_init;
static vfs_uninit_t ffs_uninit;
static vfs_extattrctl_t ffs_extattrctl;
static vfs_cmount_t ffs_cmount;
static vfs_unmount_t ffs_unmount;
static vfs_mount_t ffs_mount;
static vfs_statfs_t ffs_statfs;
static vfs_fhtovp_t ffs_fhtovp;
static vfs_sync_t ffs_sync;

static struct vfsops ufs_vfsops = {
	.vfs_extattrctl =	ffs_extattrctl,
	.vfs_fhtovp =		ffs_fhtovp,
	.vfs_init =		ffs_init,
	.vfs_mount =		ffs_mount,
	.vfs_cmount =		ffs_cmount,
	.vfs_quotactl =		ufs_quotactl,
	.vfs_root =		vfs_cache_root,
	.vfs_cachedroot =	ufs_root,
	.vfs_statfs =		ffs_statfs,
	.vfs_sync =		ffs_sync,
	.vfs_uninit =		ffs_uninit,
	.vfs_unmount =		ffs_unmount,
	.vfs_vget =		ffs_vget,
	.vfs_susp_clean =	process_deferred_inactive,
};

VFS_SET(ufs_vfsops, ufs, VFCF_FILEREVINC);
MODULE_VERSION(ufs, 1);

static b_strategy_t ffs_geom_strategy;
static b_write_t ffs_bufwrite;

static struct buf_ops ffs_ops = {
	.bop_name =	"FFS",
	.bop_write =	ffs_bufwrite,
	.bop_strategy =	ffs_geom_strategy,
	.bop_sync =	bufsync,
#ifdef NO_FFS_SNAPSHOT
	.bop_bdflush =	bufbdflush,
#else
	.bop_bdflush =	ffs_bdflush,
#endif
};

/*
 * Note that userquota and groupquota options are not currently used
 * by UFS/FFS code and generally mount(8) does not pass those options
 * from userland, but they can be passed by loader(8) via
 * vfs.root.mountfrom.options.
 */
static const char *ffs_opts[] = { "acls", "async", "noatime", "noclusterr",
    "noclusterw", "noexec", "export", "force", "from", "groupquota",
    "multilabel", "nfsv4acls", "snapshot", "nosuid", "suiddir",
    "nosymfollow", "sync", "union", "userquota", "untrusted", NULL };

static int ffs_enxio_enable = 1;
SYSCTL_DECL(_vfs_ffs);
SYSCTL_INT(_vfs_ffs, OID_AUTO, enxio_enable, CTLFLAG_RWTUN,
    &ffs_enxio_enable, 0,
    "enable mapping of other disk I/O errors to ENXIO");

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
static int
ffs_blkatoff(struct vnode *vp, off_t offset, char **res, struct buf **bpp)
{
	struct inode *ip;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn;
	int bsize, error;

	ip = VTOI(vp);
	fs = ITOFS(ip);
	lbn = lblkno(fs, offset);
	bsize = blksize(fs, ip, lbn);

	*bpp = NULL;
	error = bread(vp, lbn, bsize, NOCRED, &bp);
	if (error) {
		return (error);
	}
	if (res)
		*res = (char *)bp->b_data + blkoff(fs, offset);
	*bpp = bp;
	return (0);
}

/*
 * Load up the contents of an inode and copy the appropriate pieces
 * to the incore copy.
 */
static int
ffs_load_inode(struct buf *bp, struct inode *ip, struct fs *fs, ino_t ino)
{
	struct ufs1_dinode *dip1;
	struct ufs2_dinode *dip2;
	int error;

	if (I_IS_UFS1(ip)) {
		dip1 = ip->i_din1;
		*dip1 =
		    *((struct ufs1_dinode *)bp->b_data + ino_to_fsbo(fs, ino));
		ip->i_mode = dip1->di_mode;
		ip->i_nlink = dip1->di_nlink;
		ip->i_effnlink = dip1->di_nlink;
		ip->i_size = dip1->di_size;
		ip->i_flags = dip1->di_flags;
		ip->i_gen = dip1->di_gen;
		ip->i_uid = dip1->di_uid;
		ip->i_gid = dip1->di_gid;
		if (ffs_oldfscompat_inode_read(fs, ip->i_dp, time_second) &&
		    fs->fs_ronly == 0)
			UFS_INODE_SET_FLAG(ip, IN_MODIFIED);
		return (0);
	}
	dip2 = ((struct ufs2_dinode *)bp->b_data + ino_to_fsbo(fs, ino));
	if ((error = ffs_verify_dinode_ckhash(fs, dip2)) != 0 &&
	    !ffs_fsfail_cleanup(ITOUMP(ip), error)) {
		printf("%s: inode %jd: check-hash failed\n", fs->fs_fsmnt,
		    (intmax_t)ino);
		return (error);
	}
	*ip->i_din2 = *dip2;
	dip2 = ip->i_din2;
	ip->i_mode = dip2->di_mode;
	ip->i_nlink = dip2->di_nlink;
	ip->i_effnlink = dip2->di_nlink;
	ip->i_size = dip2->di_size;
	ip->i_flags = dip2->di_flags;
	ip->i_gen = dip2->di_gen;
	ip->i_uid = dip2->di_uid;
	ip->i_gid = dip2->di_gid;
	if (ffs_oldfscompat_inode_read(fs, ip->i_dp, time_second) &&
	    fs->fs_ronly == 0)
		UFS_INODE_SET_FLAG(ip, IN_MODIFIED);
	return (0);
}

/*
 * Verify that a filesystem block number is a valid data block.
 * This routine is only called on untrusted filesystems.
 */
static int
ffs_check_blkno(struct mount *mp, ino_t inum, ufs2_daddr_t daddr, int blksize)
{
	struct fs *fs;
	struct ufsmount *ump;
	ufs2_daddr_t end_daddr;
	int cg, havemtx;

	KASSERT((mp->mnt_flag & MNT_UNTRUSTED) != 0,
	    ("ffs_check_blkno called on a trusted file system"));
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	cg = dtog(fs, daddr);
	end_daddr = daddr + numfrags(fs, blksize);
	/*
	 * Verify that the block number is a valid data block. Also check
	 * that it does not point to an inode block or a superblock. Accept
	 * blocks that are unalloacted (0) or part of snapshot metadata
	 * (BLK_NOCOPY or BLK_SNAP).
	 *
	 * Thus, the block must be in a valid range for the filesystem and
	 * either in the space before a backup superblock (except the first
	 * cylinder group where that space is used by the bootstrap code) or
	 * after the inode blocks and before the end of the cylinder group.
	 */
	if ((uint64_t)daddr <= BLK_SNAP ||
	    ((uint64_t)end_daddr <= fs->fs_size &&
	    ((cg > 0 && end_daddr <= cgsblock(fs, cg)) ||
	    (daddr >= cgdmin(fs, cg) &&
	    end_daddr <= cgbase(fs, cg) + fs->fs_fpg))))
		return (0);
	if ((havemtx = mtx_owned(UFS_MTX(ump))) == 0)
		UFS_LOCK(ump);
	if (ppsratecheck(&ump->um_last_integritymsg,
	    &ump->um_secs_integritymsg, 1)) {
		UFS_UNLOCK(ump);
		uprintf("\n%s: inode %jd, out-of-range indirect block "
		    "number %jd\n", mp->mnt_stat.f_mntonname, inum, daddr);
		if (havemtx)
			UFS_LOCK(ump);
	} else if (!havemtx)
		UFS_UNLOCK(ump);
	return (EINTEGRITY);
}

/*
 * On first ENXIO error, initiate an asynchronous forcible unmount.
 * Used to unmount filesystems whose underlying media has gone away.
 *
 * Return true if a cleanup is in progress.
 */
int
ffs_fsfail_cleanup(struct ufsmount *ump, int error)
{
	int retval;

	UFS_LOCK(ump);
	retval = ffs_fsfail_cleanup_locked(ump, error);
	UFS_UNLOCK(ump);
	return (retval);
}

int
ffs_fsfail_cleanup_locked(struct ufsmount *ump, int error)
{
	mtx_assert(UFS_MTX(ump), MA_OWNED);
	if (error == ENXIO && (ump->um_flags & UM_FSFAIL_CLEANUP) == 0) {
		ump->um_flags |= UM_FSFAIL_CLEANUP;
		if (ump->um_mountp == rootvnode->v_mount)
			panic("UFS: root fs would be forcibly unmounted");

		/*
		 * Queue an async forced unmount.
		 */
		vfs_ref(ump->um_mountp);
		dounmount(ump->um_mountp,
		    MNT_FORCE | MNT_RECURSE | MNT_DEFERRED, curthread);
		printf("UFS: forcibly unmounting %s from %s\n",
		    ump->um_mountp->mnt_stat.f_mntfromname,
		    ump->um_mountp->mnt_stat.f_mntonname);
	}
	return ((ump->um_flags & UM_FSFAIL_CLEANUP) != 0);
}

/*
 * Wrapper used during ENXIO cleanup to allocate empty buffers when
 * the kernel is unable to read the real one. They are needed so that
 * the soft updates code can use them to unwind its dependencies.
 */
int
ffs_breadz(struct ufsmount *ump, struct vnode *vp, daddr_t lblkno,
    daddr_t dblkno, int size, daddr_t *rablkno, int *rabsize, int cnt,
    struct ucred *cred, int flags, void (*ckhashfunc)(struct buf *),
    struct buf **bpp)
{
	int error;

	flags |= GB_CVTENXIO;
	error = breadn_flags(vp, lblkno, dblkno, size, rablkno, rabsize, cnt,
	    cred, flags, ckhashfunc, bpp);
	if (error != 0 && ffs_fsfail_cleanup(ump, error)) {
		error = getblkx(vp, lblkno, dblkno, size, 0, 0, flags, bpp);
		KASSERT(error == 0, ("getblkx failed"));
		vfs_bio_bzero_buf(*bpp, 0, size);
	}
	return (error);
}

static int
ffs_mount(struct mount *mp)
{
	struct vnode *devvp, *odevvp;
	struct thread *td;
	struct ufsmount *ump = NULL;
	struct fs *fs;
	int error, flags;
	int error1 __diagused;
	uint64_t mntorflags, saved_mnt_flag;
	accmode_t accmode;
	struct nameidata ndp;
	char *fspec;
	bool mounted_softdep;

	td = curthread;
	if (vfs_filteropt(mp->mnt_optnew, ffs_opts))
		return (EINVAL);
	if (uma_inode == NULL) {
		uma_inode = uma_zcreate("FFS inode",
		    sizeof(struct inode), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
		uma_ufs1 = uma_zcreate("FFS1 dinode",
		    sizeof(struct ufs1_dinode), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
		uma_ufs2 = uma_zcreate("FFS2 dinode",
		    sizeof(struct ufs2_dinode), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
		VFS_SMR_ZONE_SET(uma_inode);
	}

	vfs_deleteopt(mp->mnt_optnew, "groupquota");
	vfs_deleteopt(mp->mnt_optnew, "userquota");

	fspec = vfs_getopts(mp->mnt_optnew, "from", &error);
	if (error)
		return (error);

	mntorflags = 0;
	if (vfs_getopt(mp->mnt_optnew, "untrusted", NULL, NULL) == 0)
		mntorflags |= MNT_UNTRUSTED;

	if (vfs_getopt(mp->mnt_optnew, "acls", NULL, NULL) == 0)
		mntorflags |= MNT_ACLS;

	if (vfs_getopt(mp->mnt_optnew, "snapshot", NULL, NULL) == 0) {
		mntorflags |= MNT_SNAPSHOT;
		/*
		 * Once we have set the MNT_SNAPSHOT flag, do not
		 * persist "snapshot" in the options list.
		 */
		vfs_deleteopt(mp->mnt_optnew, "snapshot");
		vfs_deleteopt(mp->mnt_opt, "snapshot");
	}

	if (vfs_getopt(mp->mnt_optnew, "nfsv4acls", NULL, NULL) == 0) {
		if (mntorflags & MNT_ACLS) {
			vfs_mount_error(mp,
			    "\"acls\" and \"nfsv4acls\" options "
			    "are mutually exclusive");
			return (EINVAL);
		}
		mntorflags |= MNT_NFS4ACLS;
	}

	MNT_ILOCK(mp);
	mp->mnt_kern_flag &= ~MNTK_FPLOOKUP;
	mp->mnt_flag |= mntorflags;
	MNT_IUNLOCK(mp);

	/*
	 * If this is a snapshot request, take the snapshot.
	 */
	if (mp->mnt_flag & MNT_SNAPSHOT) {
		if ((mp->mnt_flag & MNT_UPDATE) == 0)
			return (EINVAL);
		return (ffs_snapshot(mp, fspec));
	}

	/*
	 * Must not call namei() while owning busy ref.
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		vfs_unbusy(mp);

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible disk device.
	 */
	NDINIT(&ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, fspec);
	error = namei(&ndp);
	if ((mp->mnt_flag & MNT_UPDATE) != 0) {
		/*
		 * Unmount does not start if MNT_UPDATE is set.  Mount
		 * update busies mp before setting MNT_UPDATE.  We
		 * must be able to retain our busy ref successfully,
		 * without sleep.
		 */
		error1 = vfs_busy(mp, MBF_NOWAIT);
		MPASS(error1 == 0);
	}
	if (error != 0)
		return (error);
	NDFREE_PNBUF(&ndp);
	if (!vn_isdisk_error(ndp.ni_vp, &error)) {
		vput(ndp.ni_vp);
		return (error);
	}

	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	accmode = VREAD;
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		accmode |= VWRITE;
	error = VOP_ACCESS(ndp.ni_vp, accmode, td->td_ucred, td);
	if (error)
		error = priv_check(td, PRIV_VFS_MOUNT_PERM);
	if (error) {
		vput(ndp.ni_vp);
		return (error);
	}

	/*
	 * New mount
	 *
	 * We need the name for the mount point (also used for
	 * "last mounted on") copied in. If an error occurs,
	 * the mount point is discarded by the upper level code.
	 * Note that vfs_mount_alloc() populates f_mntonname for us.
	 */
	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		if ((error = ffs_mountfs(ndp.ni_vp, mp, td)) != 0) {
			vrele(ndp.ni_vp);
			return (error);
		}
	} else {
		/*
		 * When updating, check whether changing from read-only to
		 * read/write; if there is no device name, that's all we do.
		 */
		ump = VFSTOUFS(mp);
		fs = ump->um_fs;
		odevvp = ump->um_odevvp;
		devvp = ump->um_devvp;

		/*
		 * If it's not the same vnode, or at least the same device
		 * then it's not correct.
		 */
		if (ndp.ni_vp->v_rdev != ump->um_odevvp->v_rdev)
			error = EINVAL; /* needs translation */
		vput(ndp.ni_vp);
		if (error)
			return (error);
		if (fs->fs_ronly == 0 &&
		    vfs_flagopt(mp->mnt_optnew, "ro", NULL, 0)) {
			/*
			 * Flush any dirty data and suspend filesystem.
			 */
			if ((error = vn_start_write(NULL, &mp, V_WAIT)) != 0)
				return (error);
			error = vfs_write_suspend_umnt(mp);
			if (error != 0)
				return (error);

			fs->fs_ronly = 1;
			if (MOUNTEDSOFTDEP(mp)) {
				MNT_ILOCK(mp);
				mp->mnt_flag &= ~MNT_SOFTDEP;
				MNT_IUNLOCK(mp);
				mounted_softdep = true;
			} else
				mounted_softdep = false;

			/*
			 * Check for and optionally get rid of files open
			 * for writing.
			 */
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			if (mounted_softdep) {
				error = softdep_flushfiles(mp, flags, td);
			} else {
				error = ffs_flushfiles(mp, flags, td);
			}
			if (error) {
				fs->fs_ronly = 0;
				if (mounted_softdep) {
					MNT_ILOCK(mp);
					mp->mnt_flag |= MNT_SOFTDEP;
					MNT_IUNLOCK(mp);
				}
				vfs_write_resume(mp, 0);
				return (error);
			}

			if (fs->fs_pendingblocks != 0 ||
			    fs->fs_pendinginodes != 0) {
				printf("WARNING: %s Update error: blocks %jd "
				    "files %d\n", fs->fs_fsmnt, 
				    (intmax_t)fs->fs_pendingblocks,
				    fs->fs_pendinginodes);
				fs->fs_pendingblocks = 0;
				fs->fs_pendinginodes = 0;
			}
			if ((fs->fs_flags & (FS_UNCLEAN | FS_NEEDSFSCK)) == 0)
				fs->fs_clean = 1;
			if ((error = ffs_sbupdate(ump, MNT_WAIT, 0)) != 0) {
				fs->fs_ronly = 0;
				fs->fs_clean = 0;
				if (mounted_softdep) {
					MNT_ILOCK(mp);
					mp->mnt_flag |= MNT_SOFTDEP;
					MNT_IUNLOCK(mp);
				}
				vfs_write_resume(mp, 0);
				return (error);
			}
			if (mounted_softdep)
				softdep_unmount(mp);
			g_topology_lock();
			/*
			 * Drop our write and exclusive access.
			 */
			g_access(ump->um_cp, 0, -1, -1);
			g_topology_unlock();
			MNT_ILOCK(mp);
			mp->mnt_flag |= MNT_RDONLY;
			MNT_IUNLOCK(mp);
			/*
			 * Allow the writers to note that filesystem
			 * is ro now.
			 */
			vfs_write_resume(mp, 0);
		}
		if ((mp->mnt_flag & MNT_RELOAD) &&
		    (error = ffs_reload(mp, 0)) != 0) {
			return (error);
		} else {
			/* ffs_reload replaces the superblock structure */
			fs = ump->um_fs;
		}
		if (fs->fs_ronly &&
		    !vfs_flagopt(mp->mnt_optnew, "ro", NULL, 0)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			vn_lock(odevvp, LK_EXCLUSIVE | LK_RETRY);
			error = VOP_ACCESS(odevvp, VREAD | VWRITE,
			    td->td_ucred, td);
			if (error)
				error = priv_check(td, PRIV_VFS_MOUNT_PERM);
			VOP_UNLOCK(odevvp);
			if (error) {
				return (error);
			}
			fs->fs_flags &= ~FS_UNCLEAN;
			if (fs->fs_clean == 0) {
				fs->fs_flags |= FS_UNCLEAN;
				if ((mp->mnt_flag & MNT_FORCE) ||
				    ((fs->fs_flags &
				     (FS_SUJ | FS_NEEDSFSCK)) == 0 &&
				     (fs->fs_flags & FS_DOSOFTDEP))) {
					printf("WARNING: %s was not properly "
					   "dismounted\n",
					   mp->mnt_stat.f_mntonname);
				} else {
					vfs_mount_error(mp,
					   "R/W mount of %s denied. %s.%s",
					   mp->mnt_stat.f_mntonname,
					   "Filesystem is not clean - run fsck",
					   (fs->fs_flags & FS_SUJ) == 0 ? "" :
					   " Forced mount will invalidate"
					   " journal contents");
					return (EPERM);
				}
			}
			g_topology_lock();
			/*
			 * Request exclusive write access.
			 */
			error = g_access(ump->um_cp, 0, 1, 1);
			g_topology_unlock();
			if (error)
				return (error);
			if ((error = vn_start_write(NULL, &mp, V_WAIT)) != 0)
				return (error);
			error = vfs_write_suspend_umnt(mp);
			if (error != 0)
				return (error);
			fs->fs_ronly = 0;
			MNT_ILOCK(mp);
			saved_mnt_flag = MNT_RDONLY;
			if (MOUNTEDSOFTDEP(mp) && (mp->mnt_flag &
			    MNT_ASYNC) != 0)
				saved_mnt_flag |= MNT_ASYNC;
			mp->mnt_flag &= ~saved_mnt_flag;
			MNT_IUNLOCK(mp);
			fs->fs_mtime = time_second;
			/* check to see if we need to start softdep */
			if ((fs->fs_flags & FS_DOSOFTDEP) &&
			    (error = softdep_mount(devvp, mp, fs, td->td_ucred))){
				fs->fs_ronly = 1;
				MNT_ILOCK(mp);
				mp->mnt_flag |= saved_mnt_flag;
				MNT_IUNLOCK(mp);
				vfs_write_resume(mp, 0);
				return (error);
			}
			fs->fs_clean = 0;
			if ((error = ffs_sbupdate(ump, MNT_WAIT, 0)) != 0) {
				fs->fs_ronly = 1;
				if ((fs->fs_flags & FS_DOSOFTDEP) != 0)
					softdep_unmount(mp);
				MNT_ILOCK(mp);
				mp->mnt_flag |= saved_mnt_flag;
				MNT_IUNLOCK(mp);
				vfs_write_resume(mp, 0);
				return (error);
			}
			if (fs->fs_snapinum[0] != 0)
				ffs_snapshot_mount(mp);
			vfs_write_resume(mp, 0);
		}
		/*
		 * Soft updates is incompatible with "async",
		 * so if we are doing softupdates stop the user
		 * from setting the async flag in an update.
		 * Softdep_mount() clears it in an initial mount
		 * or ro->rw remount.
		 */
		if (MOUNTEDSOFTDEP(mp)) {
			/* XXX: Reset too late ? */
			MNT_ILOCK(mp);
			mp->mnt_flag &= ~MNT_ASYNC;
			MNT_IUNLOCK(mp);
		}
		/*
		 * Keep MNT_ACLS flag if it is stored in superblock.
		 */
		if ((fs->fs_flags & FS_ACLS) != 0) {
			/* XXX: Set too late ? */
			MNT_ILOCK(mp);
			mp->mnt_flag |= MNT_ACLS;
			MNT_IUNLOCK(mp);
		}

		if ((fs->fs_flags & FS_NFS4ACLS) != 0) {
			/* XXX: Set too late ? */
			MNT_ILOCK(mp);
			mp->mnt_flag |= MNT_NFS4ACLS;
			MNT_IUNLOCK(mp);
		}

	}

	MNT_ILOCK(mp);
	/*
	 * This is racy versus lookup, see ufs_fplookup_vexec for details.
	 */
	if ((mp->mnt_kern_flag & MNTK_FPLOOKUP) != 0)
		panic("MNTK_FPLOOKUP set on mount %p when it should not be", mp);
	if ((mp->mnt_flag & (MNT_ACLS | MNT_NFS4ACLS | MNT_UNION)) == 0)
		mp->mnt_kern_flag |= MNTK_FPLOOKUP;
	MNT_IUNLOCK(mp);

	vfs_mountedfrom(mp, fspec);
	return (0);
}

/*
 * Compatibility with old mount system call.
 */

static int
ffs_cmount(struct mntarg *ma, void *data, uint64_t flags)
{
	struct ufs_args args;
	int error;

	if (data == NULL)
		return (EINVAL);
	error = copyin(data, &args, sizeof args);
	if (error)
		return (error);

	ma = mount_argsu(ma, "from", args.fspec, MAXPATHLEN);
	ma = mount_arg(ma, "export", &args.export, sizeof(args.export));
	error = kernel_mount(ma, flags);

	return (error);
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). If the 'force' flag
 * is 0, the filesystem must be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) If requested, clear MNTK_SUSPEND2 and MNTK_SUSPENDED flags
 *	   to allow secondary writers.
 *	4) invalidate all cached file data.
 *	5) re-read inode data for all active vnodes.
 */
int
ffs_reload(struct mount *mp, int flags)
{
	struct vnode *vp, *mvp, *devvp;
	struct inode *ip;
	struct buf *bp;
	struct fs *fs, *newfs;
	struct ufsmount *ump;
	int error;

	ump = VFSTOUFS(mp);

	MNT_ILOCK(mp);
	if ((mp->mnt_flag & MNT_RDONLY) == 0 && (flags & FFSR_FORCE) == 0) {
		MNT_IUNLOCK(mp);
		return (EINVAL);
	}
	MNT_IUNLOCK(mp);

	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOUFS(mp)->um_devvp;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	if (vinvalbuf(devvp, 0, 0, 0) != 0)
		panic("ffs_reload: dirty1");
	VOP_UNLOCK(devvp);

	/*
	 * Step 2: re-read superblock from disk.
	 */
	if ((error = ffs_sbget(devvp, &newfs, UFS_STDSB, 0, M_UFSMNT,
	    ffs_use_bread)) != 0)
		return (error);
	/*
	 * Replace our superblock with the new superblock. Preserve
	 * our read-only status.
	 */
	fs = VFSTOUFS(mp)->um_fs;
	newfs->fs_ronly = fs->fs_ronly;
	free(fs->fs_csp, M_UFSMNT);
	free(fs->fs_si, M_UFSMNT);
	free(fs, M_UFSMNT);
	fs = VFSTOUFS(mp)->um_fs = newfs;
	ump->um_bsize = fs->fs_bsize;
	ump->um_maxsymlinklen = fs->fs_maxsymlinklen;
	UFS_LOCK(ump);
	if (fs->fs_pendingblocks != 0 || fs->fs_pendinginodes != 0) {
		printf("WARNING: %s: reload pending error: blocks %jd "
		    "files %d\n", mp->mnt_stat.f_mntonname,
		    (intmax_t)fs->fs_pendingblocks, fs->fs_pendinginodes);
		fs->fs_pendingblocks = 0;
		fs->fs_pendinginodes = 0;
	}
	UFS_UNLOCK(ump);
	/*
	 * Step 3: If requested, clear MNTK_SUSPEND2 and MNTK_SUSPENDED flags
	 * to allow secondary writers.
	 */
	if ((flags & FFSR_UNSUSPEND) != 0) {
		MNT_ILOCK(mp);
		mp->mnt_kern_flag &= ~(MNTK_SUSPENDED | MNTK_SUSPEND2);
		wakeup(&mp->mnt_flag);
		MNT_IUNLOCK(mp);
	}

loop:
	MNT_VNODE_FOREACH_ALL(vp, mp, mvp) {
		/*
		 * Skip syncer vnode.
		 */
		if (vp->v_type == VNON) {
			VI_UNLOCK(vp);
			continue;
		}
		/*
		 * Step 4: invalidate all cached file data.
		 */
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK)) {
			MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
			goto loop;
		}
		if (vinvalbuf(vp, 0, 0, 0))
			panic("ffs_reload: dirty2");
		/*
		 * Step 5: re-read inode data for all active vnodes.
		 */
		ip = VTOI(vp);
		error =
		    bread(devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		    (int)fs->fs_bsize, NOCRED, &bp);
		if (error) {
			vput(vp);
			MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
			return (error);
		}
		if ((error = ffs_load_inode(bp, ip, fs, ip->i_number)) != 0) {
			brelse(bp);
			vput(vp);
			MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
			return (error);
		}
		ip->i_effnlink = ip->i_nlink;
		brelse(bp);
		vput(vp);
	}
	return (0);
}

/*
 * Common code for mount and mountroot
 */
static int
ffs_mountfs(struct vnode *odevvp, struct mount *mp, struct thread *td)
{
	struct ufsmount *ump;
	struct fs *fs;
	struct cdev *dev;
	int error, i, len, ronly;
	struct ucred *cred;
	struct g_consumer *cp;
	struct mount *nmp;
	struct vnode *devvp;
	int candelete, canspeedup;

	fs = NULL;
	ump = NULL;
	cred = td ? td->td_ucred : NOCRED;
	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

	devvp = mntfs_allocvp(mp, odevvp);
	KASSERT(devvp->v_type == VCHR, ("reclaimed devvp"));
	dev = devvp->v_rdev;
	KASSERT(dev->si_snapdata == NULL, ("non-NULL snapshot data"));
	if (atomic_cmpset_acq_ptr((uintptr_t *)&dev->si_mountpt, 0,
	    (uintptr_t)mp) == 0) {
		mntfs_freevp(devvp);
		return (EBUSY);
	}
	g_topology_lock();
	error = g_vfs_open(devvp, &cp, "ffs", ronly ? 0 : 1);
	g_topology_unlock();
	if (error != 0) {
		atomic_store_rel_ptr((uintptr_t *)&dev->si_mountpt, 0);
		mntfs_freevp(devvp);
		return (error);
	}
	dev_ref(dev);
	devvp->v_bufobj.bo_ops = &ffs_ops;
	BO_LOCK(&odevvp->v_bufobj);
	odevvp->v_bufobj.bo_flag |= BO_NOBUFS;
	BO_UNLOCK(&odevvp->v_bufobj);
	VOP_UNLOCK(devvp);
	if (dev->si_iosize_max != 0)
		mp->mnt_iosize_max = dev->si_iosize_max;
	if (mp->mnt_iosize_max > maxphys)
		mp->mnt_iosize_max = maxphys;
	if ((SBLOCKSIZE % cp->provider->sectorsize) != 0) {
		error = EINVAL;
		vfs_mount_error(mp,
		    "Invalid sectorsize %d for superblock size %d",
		    cp->provider->sectorsize, SBLOCKSIZE);
		goto out;
	}
	/* fetch the superblock and summary information */
	if ((mp->mnt_flag & (MNT_ROOTFS | MNT_FORCE)) != 0)
		error = ffs_sbsearch(devvp, &fs, 0, M_UFSMNT, ffs_use_bread);
	else
		error = ffs_sbget(devvp, &fs, UFS_STDSB, 0, M_UFSMNT,
		    ffs_use_bread);
	if (error != 0)
		goto out;
	fs->fs_flags &= ~FS_UNCLEAN;
	if (fs->fs_clean == 0) {
		fs->fs_flags |= FS_UNCLEAN;
		if (ronly || (mp->mnt_flag & MNT_FORCE) ||
		    ((fs->fs_flags & (FS_SUJ | FS_NEEDSFSCK)) == 0 &&
		     (fs->fs_flags & FS_DOSOFTDEP))) {
			printf("WARNING: %s was not properly dismounted\n",
			    mp->mnt_stat.f_mntonname);
		} else {
			vfs_mount_error(mp, "R/W mount on %s denied. "
			    "Filesystem is not clean - run fsck.%s",
			    mp->mnt_stat.f_mntonname,
			    (fs->fs_flags & FS_SUJ) == 0 ? "" :
			    " Forced mount will invalidate journal contents");
			error = EPERM;
			goto out;
		}
		if ((fs->fs_pendingblocks != 0 || fs->fs_pendinginodes != 0) &&
		    (mp->mnt_flag & MNT_FORCE)) {
			printf("WARNING: %s: lost blocks %jd files %d\n",
			    mp->mnt_stat.f_mntonname,
			    (intmax_t)fs->fs_pendingblocks,
			    fs->fs_pendinginodes);
			fs->fs_pendingblocks = 0;
			fs->fs_pendinginodes = 0;
		}
	}
	if (fs->fs_pendingblocks != 0 || fs->fs_pendinginodes != 0) {
		printf("WARNING: %s: mount pending error: blocks %jd "
		    "files %d\n", mp->mnt_stat.f_mntonname,
		    (intmax_t)fs->fs_pendingblocks, fs->fs_pendinginodes);
		fs->fs_pendingblocks = 0;
		fs->fs_pendinginodes = 0;
	}
	if ((fs->fs_flags & FS_GJOURNAL) != 0) {
#ifdef UFS_GJOURNAL
		/*
		 * Get journal provider name.
		 */
		len = 1024;
		mp->mnt_gjprovider = malloc((uint64_t)len, M_UFSMNT, M_WAITOK);
		if (g_io_getattr("GJOURNAL::provider", cp, &len,
		    mp->mnt_gjprovider) == 0) {
			mp->mnt_gjprovider = realloc(mp->mnt_gjprovider, len,
			    M_UFSMNT, M_WAITOK);
			MNT_ILOCK(mp);
			mp->mnt_flag |= MNT_GJOURNAL;
			MNT_IUNLOCK(mp);
		} else {
			if ((mp->mnt_flag & MNT_RDONLY) == 0)
				printf("WARNING: %s: GJOURNAL flag on fs "
				    "but no gjournal provider below\n",
				    mp->mnt_stat.f_mntonname);
			free(mp->mnt_gjprovider, M_UFSMNT);
			mp->mnt_gjprovider = NULL;
		}
#else
		printf("WARNING: %s: GJOURNAL flag on fs but no "
		    "UFS_GJOURNAL support\n", mp->mnt_stat.f_mntonname);
#endif
	} else {
		mp->mnt_gjprovider = NULL;
	}
	ump = malloc(sizeof *ump, M_UFSMNT, M_WAITOK | M_ZERO);
	ump->um_cp = cp;
	ump->um_bo = &devvp->v_bufobj;
	ump->um_fs = fs;
	if (fs->fs_magic == FS_UFS1_MAGIC) {
		ump->um_fstype = UFS1;
		ump->um_balloc = ffs_balloc_ufs1;
	} else {
		ump->um_fstype = UFS2;
		ump->um_balloc = ffs_balloc_ufs2;
	}
	ump->um_blkatoff = ffs_blkatoff;
	ump->um_truncate = ffs_truncate;
	ump->um_update = ffs_update;
	ump->um_valloc = ffs_valloc;
	ump->um_vfree = ffs_vfree;
	ump->um_ifree = ffs_ifree;
	ump->um_rdonly = ffs_rdonly;
	ump->um_snapgone = ffs_snapgone;
	if ((mp->mnt_flag & MNT_UNTRUSTED) != 0)
		ump->um_check_blkno = ffs_check_blkno;
	else
		ump->um_check_blkno = NULL;
	mtx_init(UFS_MTX(ump), "FFS", "FFS Lock", MTX_DEF);
	sx_init(&ump->um_checkpath_lock, "uchpth");
	fs->fs_ronly = ronly;
	fs->fs_active = NULL;
	mp->mnt_data = ump;
	mp->mnt_stat.f_fsid.val[0] = fs->fs_id[0];
	mp->mnt_stat.f_fsid.val[1] = fs->fs_id[1];
	nmp = NULL;
	if (fs->fs_id[0] == 0 || fs->fs_id[1] == 0 ||
	    (nmp = vfs_getvfs(&mp->mnt_stat.f_fsid))) {
		if (nmp)
			vfs_rel(nmp);
		vfs_getnewfsid(mp);
	}
	ump->um_bsize = fs->fs_bsize;
	ump->um_maxsymlinklen = fs->fs_maxsymlinklen;
	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	MNT_IUNLOCK(mp);
	if ((fs->fs_flags & FS_MULTILABEL) != 0) {
#ifdef MAC
		MNT_ILOCK(mp);
		mp->mnt_flag |= MNT_MULTILABEL;
		MNT_IUNLOCK(mp);
#else
		printf("WARNING: %s: multilabel flag on fs but "
		    "no MAC support\n", mp->mnt_stat.f_mntonname);
#endif
	}
	if ((fs->fs_flags & FS_ACLS) != 0) {
#ifdef UFS_ACL
		MNT_ILOCK(mp);

		if (mp->mnt_flag & MNT_NFS4ACLS)
			printf("WARNING: %s: ACLs flag on fs conflicts with "
			    "\"nfsv4acls\" mount option; option ignored\n",
			    mp->mnt_stat.f_mntonname);
		mp->mnt_flag &= ~MNT_NFS4ACLS;
		mp->mnt_flag |= MNT_ACLS;

		MNT_IUNLOCK(mp);
#else
		printf("WARNING: %s: ACLs flag on fs but no ACLs support\n",
		    mp->mnt_stat.f_mntonname);
#endif
	}
	if ((fs->fs_flags & FS_NFS4ACLS) != 0) {
#ifdef UFS_ACL
		MNT_ILOCK(mp);

		if (mp->mnt_flag & MNT_ACLS)
			printf("WARNING: %s: NFSv4 ACLs flag on fs conflicts "
			    "with \"acls\" mount option; option ignored\n",
			    mp->mnt_stat.f_mntonname);
		mp->mnt_flag &= ~MNT_ACLS;
		mp->mnt_flag |= MNT_NFS4ACLS;

		MNT_IUNLOCK(mp);
#else
		printf("WARNING: %s: NFSv4 ACLs flag on fs but no "
		    "ACLs support\n", mp->mnt_stat.f_mntonname);
#endif
	}
	if ((fs->fs_flags & FS_TRIM) != 0) {
		len = sizeof(int);
		if (g_io_getattr("GEOM::candelete", cp, &len,
		    &candelete) == 0) {
			if (candelete)
				ump->um_flags |= UM_CANDELETE;
			else
				printf("WARNING: %s: TRIM flag on fs but disk "
				    "does not support TRIM\n",
				    mp->mnt_stat.f_mntonname);
		} else {
			printf("WARNING: %s: TRIM flag on fs but disk does "
			    "not confirm that it supports TRIM\n",
			    mp->mnt_stat.f_mntonname);
		}
		if (((ump->um_flags) & UM_CANDELETE) != 0) {
			ump->um_trim_tq = taskqueue_create("trim", M_WAITOK,
			    taskqueue_thread_enqueue, &ump->um_trim_tq);
			taskqueue_start_threads(&ump->um_trim_tq, 1, PVFS,
			    "%s trim", mp->mnt_stat.f_mntonname);
			ump->um_trimhash = hashinit(MAXTRIMIO, M_TRIM,
			    &ump->um_trimlisthashsize);
		}
	}

	len = sizeof(int);
	if (g_io_getattr("GEOM::canspeedup", cp, &len, &canspeedup) == 0) {
		if (canspeedup)
			ump->um_flags |= UM_CANSPEEDUP;
	}

	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_odevvp = odevvp;
	ump->um_nindir = fs->fs_nindir;
	ump->um_bptrtodb = fs->fs_fsbtodb;
	ump->um_seqinc = fs->fs_frag;
	for (i = 0; i < MAXQUOTAS; i++)
		ump->um_quotas[i] = NULLVP;
#ifdef UFS_EXTATTR
	ufs_extattr_uepm_init(&ump->um_extattr);
#endif
	/*
	 * Set FS local "last mounted on" information (NULL pad)
	 */
	bzero(fs->fs_fsmnt, MAXMNTLEN);
	strlcpy(fs->fs_fsmnt, mp->mnt_stat.f_mntonname, MAXMNTLEN);
	mp->mnt_stat.f_iosize = fs->fs_bsize;

	if (mp->mnt_flag & MNT_ROOTFS) {
		/*
		 * Root mount; update timestamp in mount structure.
		 * this will be used by the common root mount code
		 * to update the system clock.
		 */
		mp->mnt_time = fs->fs_time;
	}

	if (ronly == 0) {
		fs->fs_mtime = time_second;
		if ((fs->fs_flags & FS_DOSOFTDEP) &&
		    (error = softdep_mount(devvp, mp, fs, cred)) != 0) {
			ffs_flushfiles(mp, FORCECLOSE, td);
			goto out;
		}
		if (fs->fs_snapinum[0] != 0)
			ffs_snapshot_mount(mp);
		fs->fs_fmod = 1;
		fs->fs_clean = 0;
		(void) ffs_sbupdate(ump, MNT_WAIT, 0);
	}
	/*
	 * Initialize filesystem state information in mount struct.
	 */
	MNT_ILOCK(mp);
	mp->mnt_kern_flag |= MNTK_LOOKUP_SHARED | MNTK_EXTENDED_SHARED |
	    MNTK_NO_IOPF | MNTK_UNMAPPED_BUFS | MNTK_USES_BCACHE;
	MNT_IUNLOCK(mp);
#ifdef UFS_EXTATTR
#ifdef UFS_EXTATTR_AUTOSTART
	/*
	 *
	 * Auto-starting does the following:
	 *	- check for /.attribute in the fs, and extattr_start if so
	 *	- for each file in .attribute, enable that file with
	 * 	  an attribute of the same name.
	 * Not clear how to report errors -- probably eat them.
	 * This would all happen while the filesystem was busy/not
	 * available, so would effectively be "atomic".
	 */
	(void) ufs_extattr_autostart(mp, td);
#endif /* !UFS_EXTATTR_AUTOSTART */
#endif /* !UFS_EXTATTR */
	return (0);
out:
	if (fs != NULL) {
		free(fs->fs_csp, M_UFSMNT);
		free(fs->fs_si, M_UFSMNT);
		free(fs, M_UFSMNT);
	}
	if (cp != NULL) {
		g_topology_lock();
		g_vfs_close(cp);
		g_topology_unlock();
	}
	if (ump != NULL) {
		mtx_destroy(UFS_MTX(ump));
		sx_destroy(&ump->um_checkpath_lock);
		if (mp->mnt_gjprovider != NULL) {
			free(mp->mnt_gjprovider, M_UFSMNT);
			mp->mnt_gjprovider = NULL;
		}
		MPASS(ump->um_softdep == NULL);
		free(ump, M_UFSMNT);
		mp->mnt_data = NULL;
	}
	BO_LOCK(&odevvp->v_bufobj);
	odevvp->v_bufobj.bo_flag &= ~BO_NOBUFS;
	BO_UNLOCK(&odevvp->v_bufobj);
	atomic_store_rel_ptr((uintptr_t *)&dev->si_mountpt, 0);
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	mntfs_freevp(devvp);
	dev_rel(dev);
	return (error);
}

/*
 * A read function for use by filesystem-layer routines.
 */
static int
ffs_use_bread(void *devfd, off_t loc, void **bufp, int size)
{
	struct buf *bp;
	int error;

	KASSERT(*bufp == NULL, ("ffs_use_bread: non-NULL *bufp %p\n", *bufp));
	*bufp = malloc(size, M_UFSMNT, M_WAITOK);
	if ((error = bread((struct vnode *)devfd, btodb(loc), size, NOCRED,
	    &bp)) != 0)
		return (error);
	bcopy(bp->b_data, *bufp, size);
	bp->b_flags |= B_INVAL | B_NOCACHE;
	brelse(bp);
	return (0);
}

/*
 * unmount system call
 */
static int
ffs_unmount(struct mount *mp, int mntflags)
{
	struct thread *td;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs;
	int error, flags, susp;
#ifdef UFS_EXTATTR
	int e_restart;
#endif

	flags = 0;
	td = curthread;
	fs = ump->um_fs;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	susp = fs->fs_ronly == 0;
#ifdef UFS_EXTATTR
	if ((error = ufs_extattr_stop(mp, td))) {
		if (error != EOPNOTSUPP)
			printf("WARNING: unmount %s: ufs_extattr_stop "
			    "returned errno %d\n", mp->mnt_stat.f_mntonname,
			    error);
		e_restart = 0;
	} else {
		ufs_extattr_uepm_destroy(&ump->um_extattr);
		e_restart = 1;
	}
#endif
	if (susp) {
		error = vfs_write_suspend_umnt(mp);
		if (error != 0)
			goto fail1;
	}
	if (MOUNTEDSOFTDEP(mp))
		error = softdep_flushfiles(mp, flags, td);
	else
		error = ffs_flushfiles(mp, flags, td);
	if (error != 0 && !ffs_fsfail_cleanup(ump, error))
		goto fail;

	UFS_LOCK(ump);
	if (fs->fs_pendingblocks != 0 || fs->fs_pendinginodes != 0) {
		printf("WARNING: unmount %s: pending error: blocks %jd "
		    "files %d\n", fs->fs_fsmnt, (intmax_t)fs->fs_pendingblocks,
		    fs->fs_pendinginodes);
		fs->fs_pendingblocks = 0;
		fs->fs_pendinginodes = 0;
	}
	UFS_UNLOCK(ump);
	if (MOUNTEDSOFTDEP(mp))
		softdep_unmount(mp);
	MPASS(ump->um_softdep == NULL);
	if (fs->fs_ronly == 0) {
		fs->fs_clean = fs->fs_flags & (FS_UNCLEAN|FS_NEEDSFSCK) ? 0 : 1;
		error = ffs_sbupdate(ump, MNT_WAIT, 0);
		if (ffs_fsfail_cleanup(ump, error))
			error = 0;
		if (error != 0 && !ffs_fsfail_cleanup(ump, error)) {
			fs->fs_clean = 0;
			goto fail;
		}
	}
	if (susp)
		vfs_write_resume(mp, VR_START_WRITE);
	if (ump->um_trim_tq != NULL) {
		MPASS(ump->um_trim_inflight == 0);
		taskqueue_free(ump->um_trim_tq);
		free (ump->um_trimhash, M_TRIM);
	}
	vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
	g_topology_lock();
	g_vfs_close(ump->um_cp);
	g_topology_unlock();
	BO_LOCK(&ump->um_odevvp->v_bufobj);
	ump->um_odevvp->v_bufobj.bo_flag &= ~BO_NOBUFS;
	BO_UNLOCK(&ump->um_odevvp->v_bufobj);
	atomic_store_rel_ptr((uintptr_t *)&ump->um_dev->si_mountpt, 0);
	mntfs_freevp(ump->um_devvp);
	vrele(ump->um_odevvp);
	dev_rel(ump->um_dev);
	mtx_destroy(UFS_MTX(ump));
	sx_destroy(&ump->um_checkpath_lock);
	if (mp->mnt_gjprovider != NULL) {
		free(mp->mnt_gjprovider, M_UFSMNT);
		mp->mnt_gjprovider = NULL;
	}
	free(fs->fs_csp, M_UFSMNT);
	free(fs->fs_si, M_UFSMNT);
	free(fs, M_UFSMNT);
	free(ump, M_UFSMNT);
	mp->mnt_data = NULL;
	if (td->td_su == mp) {
		td->td_su = NULL;
		vfs_rel(mp);
	}
	return (error);

fail:
	if (susp)
		vfs_write_resume(mp, VR_START_WRITE);
fail1:
#ifdef UFS_EXTATTR
	if (e_restart) {
		ufs_extattr_uepm_init(&ump->um_extattr);
#ifdef UFS_EXTATTR_AUTOSTART
		(void) ufs_extattr_autostart(mp, td);
#endif
	}
#endif

	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
int
ffs_flushfiles(struct mount *mp, int flags, struct thread *td)
{
	struct ufsmount *ump;
	int qerror, error;

	ump = VFSTOUFS(mp);
	qerror = 0;
#ifdef QUOTA
	if (mp->mnt_flag & MNT_QUOTA) {
		int i;
		error = vflush(mp, 0, SKIPSYSTEM|flags, td);
		if (error)
			return (error);
		for (i = 0; i < MAXQUOTAS; i++) {
			error = quotaoff(td, mp, i);
			if (error != 0) {
				if ((flags & EARLYFLUSH) == 0)
					return (error);
				else
					qerror = error;
			}
		}

		/*
		 * Here we fall through to vflush again to ensure that
		 * we have gotten rid of all the system vnodes, unless
		 * quotas must not be closed.
		 */
	}
#endif
	/* devvp is not locked there */
	if (ump->um_devvp->v_vflag & VV_COPYONWRITE) {
		if ((error = vflush(mp, 0, SKIPSYSTEM | flags, td)) != 0)
			return (error);
		ffs_snapshot_unmount(mp);
		flags |= FORCECLOSE;
		/*
		 * Here we fall through to vflush again to ensure
		 * that we have gotten rid of all the system vnodes.
		 */
	}

	/*
	 * Do not close system files if quotas were not closed, to be
	 * able to sync the remaining dquots.  The freeblks softupdate
	 * workitems might hold a reference on a dquot, preventing
	 * quotaoff() from completing.  Next round of
	 * softdep_flushworklist() iteration should process the
	 * blockers, allowing the next run of quotaoff() to finally
	 * flush held dquots.
	 *
	 * Otherwise, flush all the files.
	 */
	if (qerror == 0 && (error = vflush(mp, 0, flags, td)) != 0)
		return (error);

	/*
	 * If this is a forcible unmount and there were any files that
	 * were unlinked but still open, then vflush() will have
	 * truncated and freed those files, which might have started
	 * some trim work.  Wait here for any trims to complete
	 * and process the blkfrees which follow the trims.
	 * This may create more dirty devvp buffers and softdep deps.
	 */
	if (ump->um_trim_tq != NULL) {
		while (ump->um_trim_inflight != 0)
			pause("ufsutr", hz);
		taskqueue_drain_all(ump->um_trim_tq);
	}

	/*
	 * Flush filesystem metadata.
	 */
	vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_FSYNC(ump->um_devvp, MNT_WAIT, td);
	VOP_UNLOCK(ump->um_devvp);
	return (error);
}

/*
 * Get filesystem statistics.
 */
static int
ffs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct ufsmount *ump;
	struct fs *fs;

	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	if (fs->fs_magic != FS_UFS1_MAGIC && fs->fs_magic != FS_UFS2_MAGIC)
		panic("ffs_statfs");
	sbp->f_version = STATFS_VERSION;
	sbp->f_bsize = fs->fs_fsize;
	sbp->f_iosize = fs->fs_bsize;
	sbp->f_blocks = fs->fs_dsize;
	UFS_LOCK(ump);
	sbp->f_bfree = fs->fs_cstotal.cs_nbfree * fs->fs_frag +
	    fs->fs_cstotal.cs_nffree + dbtofsb(fs, fs->fs_pendingblocks);
	sbp->f_bavail = freespace(fs, fs->fs_minfree) +
	    dbtofsb(fs, fs->fs_pendingblocks);
	sbp->f_files =  fs->fs_ncg * fs->fs_ipg - UFS_ROOTINO;
	sbp->f_ffree = fs->fs_cstotal.cs_nifree + fs->fs_pendinginodes;
	UFS_UNLOCK(ump);
	sbp->f_namemax = UFS_MAXNAMLEN;
	return (0);
}

static bool
sync_doupdate(struct inode *ip)
{

	return ((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_MODIFIED |
	    IN_UPDATE)) != 0);
}

static int
ffs_sync_lazy_filter(struct vnode *vp, void *arg __unused)
{
	struct inode *ip;

	/*
	 * Flags are safe to access because ->v_data invalidation
	 * is held off by listmtx.
	 */
	if (vp->v_type == VNON)
		return (false);
	ip = VTOI(vp);
	if (!sync_doupdate(ip) && (vp->v_iflag & VI_OWEINACT) == 0)
		return (false);
	return (true);
}

/*
 * For a lazy sync, we only care about access times, quotas and the
 * superblock.  Other filesystem changes are already converted to
 * cylinder group blocks or inode blocks updates and are written to
 * disk by syncer.
 */
static int
ffs_sync_lazy(struct mount *mp)
{
	struct vnode *mvp, *vp;
	struct inode *ip;
	int allerror, error;

	allerror = 0;
	if ((mp->mnt_flag & MNT_NOATIME) != 0) {
#ifdef QUOTA
		qsync(mp);
#endif
		goto sbupdate;
	}
	MNT_VNODE_FOREACH_LAZY(vp, mp, mvp, ffs_sync_lazy_filter, NULL) {
		if (vp->v_type == VNON) {
			VI_UNLOCK(vp);
			continue;
		}
		ip = VTOI(vp);

		/*
		 * The IN_ACCESS flag is converted to IN_MODIFIED by
		 * ufs_close() and ufs_getattr() by the calls to
		 * ufs_itimes_locked(), without subsequent UFS_UPDATE().
		 * Test also all the other timestamp flags too, to pick up
		 * any other cases that could be missed.
		 */
		if (!sync_doupdate(ip) && (vp->v_iflag & VI_OWEINACT) == 0) {
			VI_UNLOCK(vp);
			continue;
		}
		if ((error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK)) != 0)
			continue;
#ifdef QUOTA
		qsyncvp(vp);
#endif
		if (sync_doupdate(ip))
			error = ffs_update(vp, 0);
		if (error != 0)
			allerror = error;
		vput(vp);
	}
sbupdate:
	if (VFSTOUFS(mp)->um_fs->fs_fmod != 0 &&
	    (error = ffs_sbupdate(VFSTOUFS(mp), MNT_LAZY, 0)) != 0)
		allerror = error;
	return (allerror);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked busy using
 * vfs_busy().
 */
static int
ffs_sync(struct mount *mp, int waitfor)
{
	struct vnode *mvp, *vp, *devvp;
	struct thread *td;
	struct inode *ip;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs;
	int error, count, lockreq, allerror = 0;
	int suspend;
	int suspended;
	int secondary_writes;
	int secondary_accwrites;
	int softdep_deps;
	int softdep_accdeps;
	struct bufobj *bo;

	suspend = 0;
	suspended = 0;
	td = curthread;
	fs = ump->um_fs;
	if (fs->fs_fmod != 0 && fs->fs_ronly != 0)
		panic("%s: ffs_sync: modification on read-only filesystem",
		    fs->fs_fsmnt);
	if (waitfor == MNT_LAZY) {
		if (!rebooting)
			return (ffs_sync_lazy(mp));
		waitfor = MNT_NOWAIT;
	}

	/*
	 * Write back each (modified) inode.
	 */
	lockreq = LK_EXCLUSIVE | LK_NOWAIT;
	if (waitfor == MNT_SUSPEND) {
		suspend = 1;
		waitfor = MNT_WAIT;
	}
	if (waitfor == MNT_WAIT)
		lockreq = LK_EXCLUSIVE;
	lockreq |= LK_INTERLOCK;
loop:
	/* Grab snapshot of secondary write counts */
	MNT_ILOCK(mp);
	secondary_writes = mp->mnt_secondary_writes;
	secondary_accwrites = mp->mnt_secondary_accwrites;
	MNT_IUNLOCK(mp);

	/* Grab snapshot of softdep dependency counts */
	softdep_get_depcounts(mp, &softdep_deps, &softdep_accdeps);

	MNT_VNODE_FOREACH_ALL(vp, mp, mvp) {
		/*
		 * Depend on the vnode interlock to keep things stable enough
		 * for a quick test.  Since there might be hundreds of
		 * thousands of vnodes, we cannot afford even a subroutine
		 * call unless there's a good chance that we have work to do.
		 */
		if (vp->v_type == VNON) {
			VI_UNLOCK(vp);
			continue;
		}
		ip = VTOI(vp);
		if ((ip->i_flag &
		    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
		    vp->v_bufobj.bo_dirty.bv_cnt == 0) {
			VI_UNLOCK(vp);
			continue;
		}
		if ((error = vget(vp, lockreq)) != 0) {
			if (error == ENOENT) {
				MNT_VNODE_FOREACH_ALL_ABORT(mp, mvp);
				goto loop;
			}
			continue;
		}
#ifdef QUOTA
		qsyncvp(vp);
#endif
		for (;;) {
			error = ffs_syncvnode(vp, waitfor, 0);
			if (error == ERELOOKUP)
				continue;
			if (error != 0)
				allerror = error;
			break;
		}
		vput(vp);
	}
	/*
	 * Force stale filesystem control information to be flushed.
	 */
	if (waitfor == MNT_WAIT || rebooting) {
		if ((error = softdep_flushworklist(ump->um_mountp, &count, td)))
			allerror = error;
		if (ffs_fsfail_cleanup(ump, allerror))
			allerror = 0;
		/* Flushed work items may create new vnodes to clean */
		if (allerror == 0 && count)
			goto loop;
	}

	devvp = ump->um_devvp;
	bo = &devvp->v_bufobj;
	BO_LOCK(bo);
	if (bo->bo_numoutput > 0 || bo->bo_dirty.bv_cnt > 0) {
		BO_UNLOCK(bo);
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_FSYNC(devvp, waitfor, td);
		VOP_UNLOCK(devvp);
		if (MOUNTEDSOFTDEP(mp) && (error == 0 || error == EAGAIN))
			error = ffs_sbupdate(ump, waitfor, 0);
		if (error != 0)
			allerror = error;
		if (ffs_fsfail_cleanup(ump, allerror))
			allerror = 0;
		if (allerror == 0 && waitfor == MNT_WAIT)
			goto loop;
	} else if (suspend != 0) {
		if (softdep_check_suspend(mp,
					  devvp,
					  softdep_deps,
					  softdep_accdeps,
					  secondary_writes,
					  secondary_accwrites) != 0) {
			MNT_IUNLOCK(mp);
			goto loop;	/* More work needed */
		}
		mtx_assert(MNT_MTX(mp), MA_OWNED);
		mp->mnt_kern_flag |= MNTK_SUSPEND2 | MNTK_SUSPENDED;
		MNT_IUNLOCK(mp);
		suspended = 1;
	} else
		BO_UNLOCK(bo);
	/*
	 * Write back modified superblock.
	 */
	if (fs->fs_fmod != 0 &&
	    (error = ffs_sbupdate(ump, waitfor, suspended)) != 0)
		allerror = error;
	if (ffs_fsfail_cleanup(ump, allerror))
		allerror = 0;
	return (allerror);
}

int
ffs_vget(struct mount *mp, ino_t ino, int flags, struct vnode **vpp)
{
	return (ffs_vgetf(mp, ino, flags, vpp, 0));
}

int
ffs_vgetf(struct mount *mp,
	ino_t ino,
	int flags,
	struct vnode **vpp,
	int ffs_flags)
{
	struct fs *fs;
	struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	daddr_t dbn;
	int error;

	MPASS((ffs_flags & (FFSV_REPLACE | FFSV_REPLACE_DOOMED)) == 0 ||
	    (flags & LK_EXCLUSIVE) != 0);

	error = vfs_hash_get(mp, ino, flags, curthread, vpp, NULL, NULL);
	if (error != 0)
		return (error);
	if (*vpp != NULL) {
		if ((ffs_flags & FFSV_REPLACE) == 0 ||
		    ((ffs_flags & FFSV_REPLACE_DOOMED) == 0 ||
		    !VN_IS_DOOMED(*vpp)))
			return (0);
		vgone(*vpp);
		vput(*vpp);
	}

	/*
	 * We must promote to an exclusive lock for vnode creation.  This
	 * can happen if lookup is passed LOCKSHARED.
	 */
	if ((flags & LK_TYPE_MASK) == LK_SHARED) {
		flags &= ~LK_TYPE_MASK;
		flags |= LK_EXCLUSIVE;
	}

	/*
	 * We do not lock vnode creation as it is believed to be too
	 * expensive for such rare case as simultaneous creation of vnode
	 * for same ino by different processes. We just allow them to race
	 * and check later to decide who wins. Let the race begin!
	 */

	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	ip = uma_zalloc_smr(uma_inode, M_WAITOK | M_ZERO);

	/* Allocate a new vnode/inode. */
	error = getnewvnode("ufs", mp, fs->fs_magic == FS_UFS1_MAGIC ?
	    &ffs_vnodeops1 : &ffs_vnodeops2, &vp);
	if (error) {
		*vpp = NULL;
		uma_zfree_smr(uma_inode, ip);
		return (error);
	}
	/*
	 * FFS supports recursive locking.
	 */
	lockmgr(vp->v_vnlock, LK_EXCLUSIVE | LK_NOWITNESS, NULL);
	VN_LOCK_AREC(vp);
	vp->v_data = ip;
	vp->v_bufobj.bo_bsize = fs->fs_bsize;
	ip->i_vnode = vp;
	ip->i_ump = ump;
	ip->i_number = ino;
	ip->i_ea_refs = 0;
	ip->i_nextclustercg = -1;
	ip->i_flag = fs->fs_magic == FS_UFS1_MAGIC ? 0 : IN_UFS2;
	ip->i_mode = 0; /* ensure error cases below throw away vnode */
	cluster_init_vn(&ip->i_clusterw);
#ifdef DIAGNOSTIC
	ufs_init_trackers(ip);
#endif
#ifdef QUOTA
	{
		int i;
		for (i = 0; i < MAXQUOTAS; i++)
			ip->i_dquot[i] = NODQUOT;
	}
#endif

	if (ffs_flags & FFSV_FORCEINSMQ)
		vp->v_vflag |= VV_FORCEINSMQ;
	error = insmntque(vp, mp);
	if (error != 0) {
		uma_zfree_smr(uma_inode, ip);
		*vpp = NULL;
		return (error);
	}
	vp->v_vflag &= ~VV_FORCEINSMQ;
	error = vfs_hash_insert(vp, ino, flags, curthread, vpp, NULL, NULL);
	if (error != 0)
		return (error);
	if (*vpp != NULL) {
		/*
		 * Calls from ffs_valloc() (i.e. FFSV_REPLACE set)
		 * operate on empty inode, which must not be found by
		 * other threads until fully filled.  Vnode for empty
		 * inode must be not re-inserted on the hash by other
		 * thread, after removal by us at the beginning.
		 */
		MPASS((ffs_flags & FFSV_REPLACE) == 0);
		return (0);
	}
	if (I_IS_UFS1(ip))
		ip->i_din1 = uma_zalloc(uma_ufs1, M_WAITOK);
	else
		ip->i_din2 = uma_zalloc(uma_ufs2, M_WAITOK);

	if ((ffs_flags & FFSV_NEWINODE) != 0) {
		/* New inode, just zero out its contents. */
		if (I_IS_UFS1(ip))
			memset(ip->i_din1, 0, sizeof(struct ufs1_dinode));
		else
			memset(ip->i_din2, 0, sizeof(struct ufs2_dinode));
	} else {
		/* Read the disk contents for the inode, copy into the inode. */
		dbn = fsbtodb(fs, ino_to_fsba(fs, ino));
		error = ffs_breadz(ump, ump->um_devvp, dbn, dbn,
		    (int)fs->fs_bsize, NULL, NULL, 0, NOCRED, 0, NULL, &bp);
		if (error != 0) {
			/*
			 * The inode does not contain anything useful, so it
			 * would be misleading to leave it on its hash chain.
			 * With mode still zero, it will be unlinked and
			 * returned to the free list by vput().
			 */
			vgone(vp);
			vput(vp);
			*vpp = NULL;
			return (error);
		}
		if ((error = ffs_load_inode(bp, ip, fs, ino)) != 0) {
			bqrelse(bp);
			vgone(vp);
			vput(vp);
			*vpp = NULL;
			return (error);
		}
		bqrelse(bp);
	}
	if (DOINGSOFTDEP(vp) && (!fs->fs_ronly ||
	    (ffs_flags & FFSV_FORCEINODEDEP) != 0))
		softdep_load_inodeblock(ip);
	else
		ip->i_effnlink = ip->i_nlink;

	/*
	 * Initialize the vnode from the inode, check for aliases.
	 * Note that the underlying vnode may have changed.
	 */
	error = ufs_vinit(mp, I_IS_UFS1(ip) ? &ffs_fifoops1 : &ffs_fifoops2,
	    &vp);
	if (error) {
		vgone(vp);
		vput(vp);
		*vpp = NULL;
		return (error);
	}

	/*
	 * Finish inode initialization.
	 */
	if (vp->v_type != VFIFO) {
		/* FFS supports shared locking for all files except fifos. */
		VN_LOCK_ASHARE(vp);
	}

	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_gen == 0) {
		while (ip->i_gen == 0)
			ip->i_gen = arc4random();
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			UFS_INODE_SET_FLAG(ip, IN_MODIFIED);
			DIP_SET(ip, i_gen, ip->i_gen);
		}
	}
#ifdef MAC
	if ((mp->mnt_flag & MNT_MULTILABEL) && ip->i_mode) {
		/*
		 * If this vnode is already allocated, and we're running
		 * multi-label, attempt to perform a label association
		 * from the extended attributes on the inode.
		 */
		error = mac_vnode_associate_extattr(mp, vp);
		if (error) {
			/* ufs_inactive will release ip->i_devvp ref. */
			vgone(vp);
			vput(vp);
			*vpp = NULL;
			return (error);
		}
	}
#endif

	vn_set_state(vp, VSTATE_CONSTRUCTED);
	*vpp = vp;
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - for UFS2 check that the inode number is initialized
 * - call ffs_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
static int
ffs_fhtovp(struct mount *mp, struct fid *fhp, int flags, struct vnode **vpp)
{
	struct ufid *ufhp;

	ufhp = (struct ufid *)fhp;
	return (ffs_inotovp(mp, ufhp->ufid_ino, ufhp->ufid_gen, flags,
	    vpp, 0));
}

/*
 * Return a vnode from a mounted filesystem for inode with specified
 * generation number. Return ESTALE if the inode with given generation
 * number no longer exists on that filesystem.
 */
int
ffs_inotovp(struct mount *mp,
	ino_t ino,
	uint64_t gen,
	int lflags,
	struct vnode **vpp,
	int ffs_flags)
{
	struct ufsmount *ump;
	struct vnode *nvp;
	struct inode *ip;
	struct fs *fs;
	struct cg *cgp;
	struct buf *bp;
	uint64_t cg;

	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	*vpp = NULL;

	if (ino < UFS_ROOTINO || ino >= fs->fs_ncg * fs->fs_ipg)
		return (ESTALE);

	/*
	 * Need to check if inode is initialized because UFS2 does lazy
	 * initialization and nfs_fhtovp can offer arbitrary inode numbers.
	 */
	if (fs->fs_magic == FS_UFS2_MAGIC) {
		cg = ino_to_cg(fs, ino);
		if (ffs_getcg(fs, ump->um_devvp, cg, 0, &bp, &cgp) != 0)
			return (ESTALE);
		if (ino >= cg * fs->fs_ipg + cgp->cg_initediblk) {
			brelse(bp);
			return (ESTALE);
		}
		brelse(bp);
	}

	if (ffs_vgetf(mp, ino, lflags, &nvp, ffs_flags) != 0)
		return (ESTALE);

	ip = VTOI(nvp);
	if (ip->i_mode == 0 || ip->i_gen != gen || ip->i_effnlink <= 0) {
		if (ip->i_mode == 0)
			vgone(nvp);
		vput(nvp);
		return (ESTALE);
	}

	vnode_create_vobject(nvp, DIP(ip, i_size), curthread);
	*vpp = nvp;
	return (0);
}

/*
 * Initialize the filesystem.
 */
static int
ffs_init(struct vfsconf *vfsp)
{

	ffs_susp_initialize();
	softdep_initialize();
	return (ufs_init(vfsp));
}

/*
 * Undo the work of ffs_init().
 */
static int
ffs_uninit(struct vfsconf *vfsp)
{
	int ret;

	ret = ufs_uninit(vfsp);
	softdep_uninitialize();
	ffs_susp_uninitialize();
	taskqueue_drain_all(taskqueue_thread);
	return (ret);
}

/*
 * Structure used to pass information from ffs_sbupdate to its
 * helper routine ffs_use_bwrite.
 */
struct devfd {
	struct ufsmount	*ump;
	struct buf	*sbbp;
	int		 waitfor;
	int		 suspended;
	int		 error;
};

/*
 * Write a superblock and associated information back to disk.
 */
int
ffs_sbupdate(struct ufsmount *ump, int waitfor, int suspended)
{
	struct fs *fs;
	struct buf *sbbp;
	struct devfd devfd;

	fs = ump->um_fs;
	if (fs->fs_ronly == 1 &&
	    (ump->um_mountp->mnt_flag & (MNT_RDONLY | MNT_UPDATE)) !=
	    (MNT_RDONLY | MNT_UPDATE))
		panic("ffs_sbupdate: write read-only filesystem");
	/*
	 * We use the superblock's buf to serialize calls to ffs_sbupdate().
	 * Copy superblock to this buffer and have it written out.
	 */
	sbbp = getblk(ump->um_devvp, btodb(fs->fs_sblockloc),
	    (int)fs->fs_sbsize, 0, 0, 0);
	UFS_LOCK(ump);
	fs->fs_fmod = 0;
	bcopy((caddr_t)fs, sbbp->b_data, (uint64_t)fs->fs_sbsize);
	UFS_UNLOCK(ump);
	fs = (struct fs *)sbbp->b_data;
	/*
	 * Initialize info needed for write function.
	 */
	devfd.ump = ump;
	devfd.sbbp = sbbp;
	devfd.waitfor = waitfor;
	devfd.suspended = suspended;
	devfd.error = 0;
	return (ffs_sbput(&devfd, fs, fs->fs_sblockloc, ffs_use_bwrite));
}

/*
 * Write function for use by filesystem-layer routines.
 */
static int
ffs_use_bwrite(void *devfd, off_t loc, void *buf, int size)
{
	struct devfd *devfdp;
	struct ufsmount *ump;
	struct buf *bp;
	struct fs *fs;
	int error;

	devfdp = devfd;
	ump = devfdp->ump;
	bp = devfdp->sbbp;
	fs = (struct fs *)bp->b_data;
	/*
	 * Writing the superblock summary information.
	 */
	if (loc != fs->fs_sblockloc) {
		bp = getblk(ump->um_devvp, btodb(loc), size, 0, 0, 0);
		bcopy(buf, bp->b_data, (uint64_t)size);
		if (devfdp->suspended)
			bp->b_flags |= B_VALIDSUSPWRT;
		if (devfdp->waitfor != MNT_WAIT)
			bawrite(bp);
		else if ((error = bwrite(bp)) != 0)
			devfdp->error = error;
		return (0);
	}
	/*
	 * Writing the superblock itself. We need to do special checks for it.
	 * A negative error code is returned to indicate that a copy of the
	 * superblock has been made and that the copy is discarded when the
	 * I/O is done. So the the caller should not attempt to restore the
	 * fs_si field after the write is done. The caller will convert the
	 * error code back to its usual positive value when returning it.
	 */
	if (ffs_fsfail_cleanup(ump, devfdp->error))
		devfdp->error = 0;
	if (devfdp->error != 0) {
		brelse(bp);
		return (-devfdp->error - 1);
	}
	if (MOUNTEDSOFTDEP(ump->um_mountp))
		softdep_setup_sbupdate(ump, fs, bp);
	if (devfdp->suspended)
		bp->b_flags |= B_VALIDSUSPWRT;
	if (devfdp->waitfor != MNT_WAIT)
		bawrite(bp);
	else if ((error = bwrite(bp)) != 0)
		devfdp->error = error;
	return (-devfdp->error - 1);
}

static int
ffs_extattrctl(struct mount *mp, int cmd, struct vnode *filename_vp,
	int attrnamespace, const char *attrname)
{

#ifdef UFS_EXTATTR
	return (ufs_extattrctl(mp, cmd, filename_vp, attrnamespace,
	    attrname));
#else
	return (vfs_stdextattrctl(mp, cmd, filename_vp, attrnamespace,
	    attrname));
#endif
}

static void
ffs_ifree(struct ufsmount *ump, struct inode *ip)
{

	if (ump->um_fstype == UFS1 && ip->i_din1 != NULL)
		uma_zfree(uma_ufs1, ip->i_din1);
	else if (ip->i_din2 != NULL)
		uma_zfree(uma_ufs2, ip->i_din2);
	uma_zfree_smr(uma_inode, ip);
}

static int dobkgrdwrite = 1;
SYSCTL_INT(_debug, OID_AUTO, dobkgrdwrite, CTLFLAG_RW, &dobkgrdwrite, 0,
    "Do background writes (honoring the BV_BKGRDWRITE flag)?");

/*
 * Complete a background write started from bwrite.
 */
static void
ffs_backgroundwritedone(struct buf *bp)
{
	struct bufobj *bufobj;
	struct buf *origbp;

#ifdef SOFTUPDATES
	if (!LIST_EMPTY(&bp->b_dep) && (bp->b_ioflags & BIO_ERROR) != 0)
		softdep_handle_error(bp);
#endif

	/*
	 * Find the original buffer that we are writing.
	 */
	bufobj = bp->b_bufobj;
	BO_LOCK(bufobj);
	if ((origbp = gbincore(bp->b_bufobj, bp->b_lblkno)) == NULL)
		panic("backgroundwritedone: lost buffer");

	/*
	 * We should mark the cylinder group buffer origbp as
	 * dirty, to not lose the failed write.
	 */
	if ((bp->b_ioflags & BIO_ERROR) != 0)
		origbp->b_vflags |= BV_BKGRDERR;
	BO_UNLOCK(bufobj);
	/*
	 * Process dependencies then return any unfinished ones.
	 */
	if (!LIST_EMPTY(&bp->b_dep) && (bp->b_ioflags & BIO_ERROR) == 0)
		buf_complete(bp);
#ifdef SOFTUPDATES
	if (!LIST_EMPTY(&bp->b_dep))
		softdep_move_dependencies(bp, origbp);
#endif
	/*
	 * This buffer is marked B_NOCACHE so when it is released
	 * by biodone it will be tossed.  Clear B_IOSTARTED in case of error.
	 */
	bp->b_flags |= B_NOCACHE;
	bp->b_flags &= ~(B_CACHE | B_IOSTARTED);
	pbrelvp(bp);

	/*
	 * Prevent brelse() from trying to keep and re-dirtying bp on
	 * errors. It causes b_bufobj dereference in
	 * bdirty()/reassignbuf(), and b_bufobj was cleared in
	 * pbrelvp() above.
	 */
	if ((bp->b_ioflags & BIO_ERROR) != 0)
		bp->b_flags |= B_INVAL;
	bufdone(bp);
	BO_LOCK(bufobj);
	/*
	 * Clear the BV_BKGRDINPROG flag in the original buffer
	 * and awaken it if it is waiting for the write to complete.
	 * If BV_BKGRDINPROG is not set in the original buffer it must
	 * have been released and re-instantiated - which is not legal.
	 */
	KASSERT((origbp->b_vflags & BV_BKGRDINPROG),
	    ("backgroundwritedone: lost buffer2"));
	origbp->b_vflags &= ~BV_BKGRDINPROG;
	if (origbp->b_vflags & BV_BKGRDWAIT) {
		origbp->b_vflags &= ~BV_BKGRDWAIT;
		wakeup(&origbp->b_xflags);
	}
	BO_UNLOCK(bufobj);
}

/*
 * Write, release buffer on completion.  (Done by iodone
 * if async).  Do not bother writing anything if the buffer
 * is invalid.
 *
 * Note that we set B_CACHE here, indicating that buffer is
 * fully valid and thus cacheable.  This is true even of NFS
 * now so we set it generally.  This could be set either here
 * or in biodone() since the I/O is synchronous.  We put it
 * here.
 */
static int
ffs_bufwrite(struct buf *bp)
{
	struct buf *newbp;
	struct cg *cgp;

	CTR3(KTR_BUF, "bufwrite(%p) vp %p flags %X", bp, bp->b_vp, bp->b_flags);
	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return (0);
	}

	if (!BUF_ISLOCKED(bp))
		panic("bufwrite: buffer is not busy???");
	/*
	 * If a background write is already in progress, delay
	 * writing this block if it is asynchronous. Otherwise
	 * wait for the background write to complete.
	 */
	BO_LOCK(bp->b_bufobj);
	if (bp->b_vflags & BV_BKGRDINPROG) {
		if (bp->b_flags & B_ASYNC) {
			BO_UNLOCK(bp->b_bufobj);
			bdwrite(bp);
			return (0);
		}
		bp->b_vflags |= BV_BKGRDWAIT;
		msleep(&bp->b_xflags, BO_LOCKPTR(bp->b_bufobj), PRIBIO,
		    "bwrbg", 0);
		if (bp->b_vflags & BV_BKGRDINPROG)
			panic("bufwrite: still writing");
	}
	bp->b_vflags &= ~BV_BKGRDERR;
	BO_UNLOCK(bp->b_bufobj);

	/*
	 * If this buffer is marked for background writing and we
	 * do not have to wait for it, make a copy and write the
	 * copy so as to leave this buffer ready for further use.
	 *
	 * This optimization eats a lot of memory.  If we have a page
	 * or buffer shortfall we can't do it.
	 */
	if (dobkgrdwrite && (bp->b_xflags & BX_BKGRDWRITE) &&
	    (bp->b_flags & B_ASYNC) &&
	    !vm_page_count_severe() &&
	    !buf_dirty_count_severe()) {
		KASSERT(bp->b_iodone == NULL,
		    ("bufwrite: needs chained iodone (%p)", bp->b_iodone));

		/* get a new block */
		newbp = geteblk(bp->b_bufsize, GB_NOWAIT_BD);
		if (newbp == NULL)
			goto normal_write;

		KASSERT(buf_mapped(bp), ("Unmapped cg"));
		memcpy(newbp->b_data, bp->b_data, bp->b_bufsize);
		BO_LOCK(bp->b_bufobj);
		bp->b_vflags |= BV_BKGRDINPROG;
		BO_UNLOCK(bp->b_bufobj);
		newbp->b_xflags |=
		    (bp->b_xflags & BX_FSPRIV) | BX_BKGRDMARKER;
		newbp->b_lblkno = bp->b_lblkno;
		newbp->b_blkno = bp->b_blkno;
		newbp->b_offset = bp->b_offset;
		newbp->b_iodone = ffs_backgroundwritedone;
		newbp->b_flags |= B_ASYNC;
		newbp->b_flags &= ~B_INVAL;
		pbgetvp(bp->b_vp, newbp);

#ifdef SOFTUPDATES
		/*
		 * Move over the dependencies.  If there are rollbacks,
		 * leave the parent buffer dirtied as it will need to
		 * be written again.
		 */
		if (LIST_EMPTY(&bp->b_dep) ||
		    softdep_move_dependencies(bp, newbp) == 0)
			bundirty(bp);
#else
		bundirty(bp);
#endif

		/*
		 * Initiate write on the copy, release the original.  The
		 * BKGRDINPROG flag prevents it from going away until 
		 * the background write completes. We have to recalculate
		 * its check hash in case the buffer gets freed and then
		 * reconstituted from the buffer cache during a later read.
		 */
		if ((bp->b_xflags & BX_CYLGRP) != 0) {
			cgp = (struct cg *)bp->b_data;
			cgp->cg_ckhash = 0;
			cgp->cg_ckhash =
			    calculate_crc32c(~0L, bp->b_data, bp->b_bcount);
		}
		bqrelse(bp);
		bp = newbp;
	} else
		/* Mark the buffer clean */
		bundirty(bp);

	/* Let the normal bufwrite do the rest for us */
normal_write:
	/*
	 * If we are writing a cylinder group, update its time.
	 */
	if ((bp->b_xflags & BX_CYLGRP) != 0) {
		cgp = (struct cg *)bp->b_data;
		cgp->cg_old_time = cgp->cg_time = time_second;
	}
	return (bufwrite(bp));
}

static void
ffs_geom_strategy(struct bufobj *bo, struct buf *bp)
{
	struct vnode *vp;
	struct buf *tbp;
	int error, nocopy;

	/*
	 * This is the bufobj strategy for the private VCHR vnodes
	 * used by FFS to access the underlying storage device.
	 * We override the default bufobj strategy and thus bypass
	 * VOP_STRATEGY() for these vnodes.
	 */
	vp = bo2vnode(bo);
	KASSERT(bp->b_vp == NULL || bp->b_vp->v_type != VCHR ||
	    bp->b_vp->v_rdev == NULL ||
	    bp->b_vp->v_rdev->si_mountpt == NULL ||
	    VFSTOUFS(bp->b_vp->v_rdev->si_mountpt) == NULL ||
	    vp == VFSTOUFS(bp->b_vp->v_rdev->si_mountpt)->um_devvp,
	    ("ffs_geom_strategy() with wrong vp"));
	if (bp->b_iocmd == BIO_WRITE) {
		if ((bp->b_flags & B_VALIDSUSPWRT) == 0 &&
		    bp->b_vp != NULL && bp->b_vp->v_mount != NULL &&
		    (bp->b_vp->v_mount->mnt_kern_flag & MNTK_SUSPENDED) != 0)
			panic("ffs_geom_strategy: bad I/O");
		nocopy = bp->b_flags & B_NOCOPY;
		bp->b_flags &= ~(B_VALIDSUSPWRT | B_NOCOPY);
		if ((vp->v_vflag & VV_COPYONWRITE) && nocopy == 0 &&
		    vp->v_rdev->si_snapdata != NULL) {
			if ((bp->b_flags & B_CLUSTER) != 0) {
				runningbufwakeup(bp);
				TAILQ_FOREACH(tbp, &bp->b_cluster.cluster_head,
					      b_cluster.cluster_entry) {
					error = ffs_copyonwrite(vp, tbp);
					if (error != 0 &&
					    error != EOPNOTSUPP) {
						bp->b_error = error;
						bp->b_ioflags |= BIO_ERROR;
						bp->b_flags &= ~B_BARRIER;
						bufdone(bp);
						return;
					}
				}
				(void)runningbufclaim(bp, bp->b_bufsize);
			} else {
				error = ffs_copyonwrite(vp, bp);
				if (error != 0 && error != EOPNOTSUPP) {
					bp->b_error = error;
					bp->b_ioflags |= BIO_ERROR;
					bp->b_flags &= ~B_BARRIER;
					bufdone(bp);
					return;
				}
			}
		}
#ifdef SOFTUPDATES
		if ((bp->b_flags & B_CLUSTER) != 0) {
			TAILQ_FOREACH(tbp, &bp->b_cluster.cluster_head,
				      b_cluster.cluster_entry) {
				if (!LIST_EMPTY(&tbp->b_dep))
					buf_start(tbp);
			}
		} else {
			if (!LIST_EMPTY(&bp->b_dep))
				buf_start(bp);
		}

#endif
		/*
		 * Check for metadata that needs check-hashes and update them.
		 */
		switch (bp->b_xflags & BX_FSPRIV) {
		case BX_CYLGRP:
			((struct cg *)bp->b_data)->cg_ckhash = 0;
			((struct cg *)bp->b_data)->cg_ckhash =
			    calculate_crc32c(~0L, bp->b_data, bp->b_bcount);
			break;

		case BX_SUPERBLOCK:
		case BX_INODE:
		case BX_INDIR:
		case BX_DIR:
			printf("Check-hash write is unimplemented!!!\n");
			break;

		case 0:
			break;

		default:
			printf("multiple buffer types 0x%b\n",
			    (bp->b_xflags & BX_FSPRIV), PRINT_UFS_BUF_XFLAGS);
			break;
		}
	}
	if (bp->b_iocmd != BIO_READ && ffs_enxio_enable)
		bp->b_xflags |= BX_CVTENXIO;
	g_vfs_strategy(bo, bp);
}

int
ffs_own_mount(const struct mount *mp)
{

	if (mp->mnt_op == &ufs_vfsops)
		return (1);
	return (0);
}

#ifdef	DDB
#ifdef SOFTUPDATES

/* defined in ffs_softdep.c */
extern void db_print_ffs(struct ufsmount *ump);

DB_SHOW_COMMAND(ffs, db_show_ffs)
{
	struct mount *mp;
	struct ufsmount *ump;

	if (have_addr) {
		ump = VFSTOUFS((struct mount *)addr);
		db_print_ffs(ump);
		return;
	}

	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (!strcmp(mp->mnt_stat.f_fstypename, ufs_vfsconf.vfc_name))
			db_print_ffs(VFSTOUFS(mp));
	}
}

#endif	/* SOFTUPDATES */
#endif	/* DDB */
