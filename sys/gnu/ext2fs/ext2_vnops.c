/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ext2_vnops.c	8.7 (Berkeley) 2/3/94
 */

#if !defined(__FreeBSD__)
#include "fifo.h"
#include "diagnostic.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#if !defined(__FreeBSD__)
#include <ufs/ufs/lockf.h>
#else
#include <sys/signalvar.h>
#endif
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ffs/ffs_extern.h>

#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_extern.h>

static int ext2_fsync __P((struct vop_fsync_args *));
static int ext2_read __P((struct vop_read_args *));
static int ext2_write __P((struct vop_write_args *));

/* Global vfs data structures for ufs. */
vop_t **ext2_vnodeop_p;
static struct vnodeopv_entry_desc ext2_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)vfs_cache_lookup },	/* lookup */
	{ &vop_cachedlookup_desc, (vop_t *)ext2_lookup },	/* lookup */
	{ &vop_create_desc, (vop_t *)ufs_create },	/* create */
	{ &vop_mknod_desc, (vop_t *)ufs_mknod },	/* mknod */
	{ &vop_open_desc, (vop_t *)ufs_open },		/* open */
	{ &vop_close_desc, (vop_t *)ufs_close },	/* close */
	{ &vop_access_desc, (vop_t *)ufs_access },	/* access */
	{ &vop_getattr_desc, (vop_t *)ufs_getattr },	/* getattr */
	{ &vop_setattr_desc, (vop_t *)ufs_setattr },	/* setattr */
	{ &vop_read_desc, (vop_t *)ext2_read },		/* read */
	{ &vop_write_desc, (vop_t *)ext2_write },	/* write */
	{ &vop_ioctl_desc, (vop_t *)ufs_ioctl },	/* ioctl */
	{ &vop_poll_desc, (vop_t *)ufs_poll },		/* poll */
	{ &vop_mmap_desc, (vop_t *)ufs_mmap },		/* mmap */
	{ &vop_fsync_desc, (vop_t *)ext2_fsync },	/* fsync */
	{ &vop_seek_desc, (vop_t *)ufs_seek },		/* seek */
	{ &vop_remove_desc, (vop_t *)ufs_remove },	/* remove */
	{ &vop_link_desc, (vop_t *)ufs_link },		/* link */
	{ &vop_rename_desc, (vop_t *)ufs_rename },	/* rename */
	{ &vop_mkdir_desc, (vop_t *)ufs_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)ufs_rmdir },	/* rmdir */
	{ &vop_symlink_desc, (vop_t *)ufs_symlink },	/* symlink */
	{ &vop_readdir_desc, (vop_t *)ext2_readdir },	/* readdir */
	{ &vop_readlink_desc, (vop_t *)ufs_readlink },	/* readlink */
	{ &vop_abortop_desc, (vop_t *)ufs_abortop },	/* abortop */
	{ &vop_inactive_desc, (vop_t *)ext2_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)ffs_reclaim },	/* reclaim */
	{ &vop_lock_desc, (vop_t *)ufs_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)ufs_unlock },	/* unlock */
	{ &vop_bmap_desc, (vop_t *)ufs_bmap },		/* bmap */
	{ &vop_strategy_desc, (vop_t *)ufs_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *)ufs_print },	/* print */
	{ &vop_islocked_desc, (vop_t *)ufs_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)ufs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, (vop_t *)ufs_advlock },	/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)ext2_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)ext2_valloc },	/* valloc */
	{ &vop_reallocblks_desc, (vop_t *)ext2_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, (vop_t *)ext2_vfree },	/* vfree */
	{ &vop_truncate_desc, (vop_t *)ext2_truncate },	/* truncate */
	{ &vop_update_desc, (vop_t *)ext2_update },	/* update */
	{ &vop_bwrite_desc, (vop_t *)vn_bwrite },	/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc ext2fs_vnodeop_opv_desc =
	{ &ext2_vnodeop_p, ext2_vnodeop_entries };

vop_t **ext2_specop_p;
static struct vnodeopv_entry_desc ext2_specop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)spec_lookup },	/* lookup */
/* XXX: vop_cachedlookup */
	{ &vop_create_desc, (vop_t *)spec_create },	/* create */
	{ &vop_mknod_desc, (vop_t *)spec_mknod },	/* mknod */
	{ &vop_open_desc, (vop_t *)spec_open },		/* open */
	{ &vop_close_desc, (vop_t *)ufsspec_close },	/* close */
	{ &vop_access_desc, (vop_t *)ufs_access },	/* access */
	{ &vop_getattr_desc, (vop_t *)ufs_getattr },	/* getattr */
	{ &vop_setattr_desc, (vop_t *)ufs_setattr },	/* setattr */
	{ &vop_read_desc, (vop_t *)ufsspec_read },	/* read */
	{ &vop_write_desc, (vop_t *)ufsspec_write },	/* write */
	{ &vop_ioctl_desc, (vop_t *)spec_ioctl },	/* ioctl */
	{ &vop_poll_desc, (vop_t *)spec_poll },		/* poll */
	{ &vop_mmap_desc, (vop_t *)spec_mmap },		/* mmap */
	{ &vop_fsync_desc, (vop_t *)ext2_fsync },	/* fsync */
	{ &vop_seek_desc, (vop_t *)spec_seek },		/* seek */
	{ &vop_remove_desc, (vop_t *)spec_remove },	/* remove */
	{ &vop_link_desc, (vop_t *)spec_link },		/* link */
	{ &vop_rename_desc, (vop_t *)spec_rename },	/* rename */
	{ &vop_mkdir_desc, (vop_t *)spec_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)spec_rmdir },	/* rmdir */
	{ &vop_symlink_desc, (vop_t *)spec_symlink },	/* symlink */
	{ &vop_readdir_desc, (vop_t *)spec_readdir },	/* readdir */
	{ &vop_readlink_desc, (vop_t *)spec_readlink },	/* readlink */
	{ &vop_abortop_desc, (vop_t *)spec_abortop },	/* abortop */
	{ &vop_inactive_desc, (vop_t *)ext2_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)ffs_reclaim },	/* reclaim */
	{ &vop_lock_desc, (vop_t *)ufs_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)ufs_unlock },	/* unlock */
	{ &vop_bmap_desc, (vop_t *)spec_bmap },		/* bmap */
	{ &vop_strategy_desc, (vop_t *)spec_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *)ufs_print },	/* print */
	{ &vop_islocked_desc, (vop_t *)ufs_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)spec_pathconf },	/* pathconf */
	{ &vop_advlock_desc, (vop_t *)spec_advlock },	/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)spec_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)spec_valloc },	/* valloc */
	{ &vop_reallocblks_desc, (vop_t *)spec_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, (vop_t *)ext2_vfree },	/* vfree */
	{ &vop_truncate_desc, (vop_t *)spec_truncate },	/* truncate */
	{ &vop_update_desc, (vop_t *)ext2_update },	/* update */
	{ &vop_bwrite_desc, (vop_t *)vn_bwrite },	/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc ext2fs_specop_opv_desc =
	{ &ext2_specop_p, ext2_specop_entries };

vop_t **ext2_fifoop_p;
static struct vnodeopv_entry_desc ext2_fifoop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)fifo_lookup },	/* lookup */
/* XXX: vop_cachedlookup */
	{ &vop_create_desc, (vop_t *)fifo_create },	/* create */
	{ &vop_mknod_desc, (vop_t *)fifo_mknod },	/* mknod */
	{ &vop_open_desc, (vop_t *)fifo_open },		/* open */
	{ &vop_close_desc, (vop_t *)ufsfifo_close },	/* close */
	{ &vop_access_desc, (vop_t *)ufs_access },	/* access */
	{ &vop_getattr_desc, (vop_t *)ufs_getattr },	/* getattr */
	{ &vop_setattr_desc, (vop_t *)ufs_setattr },	/* setattr */
	{ &vop_read_desc, (vop_t *)ufsfifo_read },	/* read */
	{ &vop_write_desc, (vop_t *)ufsfifo_write },	/* write */
	{ &vop_ioctl_desc, (vop_t *)fifo_ioctl },	/* ioctl */
	{ &vop_poll_desc, (vop_t *)fifo_poll },		/* poll */
	{ &vop_mmap_desc, (vop_t *)fifo_mmap },		/* mmap */
	{ &vop_fsync_desc, (vop_t *)ext2_fsync },	/* fsync */
	{ &vop_seek_desc, (vop_t *)fifo_seek },		/* seek */
	{ &vop_remove_desc, (vop_t *)fifo_remove },	/* remove */
	{ &vop_link_desc, (vop_t *)fifo_link },		/* link */
	{ &vop_rename_desc, (vop_t *)fifo_rename },	/* rename */
	{ &vop_mkdir_desc, (vop_t *)fifo_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)fifo_rmdir },	/* rmdir */
	{ &vop_symlink_desc, (vop_t *)fifo_symlink },	/* symlink */
	{ &vop_readdir_desc, (vop_t *)fifo_readdir },	/* readdir */
	{ &vop_readlink_desc, (vop_t *)fifo_readlink },	/* readlink */
	{ &vop_abortop_desc, (vop_t *)fifo_abortop },	/* abortop */
	{ &vop_inactive_desc, (vop_t *)ext2_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)ffs_reclaim },	/* reclaim */
	{ &vop_lock_desc, (vop_t *)ufs_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)ufs_unlock },	/* unlock */
	{ &vop_bmap_desc, (vop_t *)fifo_bmap },		/* bmap */
	{ &vop_strategy_desc, (vop_t *)fifo_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *)ufs_print },	/* print */
	{ &vop_islocked_desc, (vop_t *)ufs_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)fifo_pathconf },	/* pathconf */
	{ &vop_advlock_desc, (vop_t *)fifo_advlock },	/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)fifo_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)fifo_valloc },	/* valloc */
	{ &vop_reallocblks_desc, (vop_t *)fifo_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, (vop_t *)ext2_vfree },	/* vfree */
	{ &vop_truncate_desc, (vop_t *)fifo_truncate },	/* truncate */
	{ &vop_update_desc, (vop_t *)ext2_update },	/* update */
	{ &vop_bwrite_desc, (vop_t *)vn_bwrite },	/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc ext2fs_fifoop_opv_desc =
	{ &ext2_fifoop_p, ext2_fifoop_entries };

#if defined(__FreeBSD__)
	VNODEOP_SET(ext2fs_vnodeop_opv_desc);
	VNODEOP_SET(ext2fs_specop_opv_desc);
	VNODEOP_SET(ext2fs_fifoop_opv_desc);
#endif

/*
 * Enabling cluster read/write operations.
 */
static int	ext2_doclusterread = 1;
static int	ext2_doclusterwrite = 1;
SYSCTL_NODE(_vfs, MOUNT_EXT2FS, ext2fs, CTLFLAG_RW, 0, "EXT2FS filesystem");
SYSCTL_INT(_vfs_ext2fs, EXT2FS_CLUSTERREAD, doclusterread,
		   CTLFLAG_RW, &ext2_doclusterread, 0, "");
SYSCTL_INT(_vfs_ext2fs, EXT2FS_CLUSTERWRITE, doclusterwrite,
		   CTLFLAG_RW, &ext2_doclusterwrite, 0, "");

#include <gnu/ext2fs/ext2_readwrite.c>

/*
 * Synch an open file.
 */
/* ARGSUSED */
static int
ext2_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct buf *bp;
	struct timeval tv;
	struct buf *nbp;
	int s;

	/* 
	 * Clean memory object.
	 * XXX add this to all file systems.
	 * XXX why is all this fs specific?
	 */
#if !defined(__FreeBSD__)
	vn_pager_sync(vp, ap->a_waitfor);
#endif

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
	ext2_discard_prealloc(VTOI(vp));

loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("ext2_fsync: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		/*
		 * Wait for I/O associated with indirect blocks to complete,
		 * since there is no way to quickly wait for them below.
		 */
		if (bp->b_vp == vp || ap->a_waitfor == MNT_NOWAIT)
			(void) bawrite(bp);
		else
			(void) bwrite(bp);
		goto loop;
	}
	if (ap->a_waitfor == MNT_WAIT) {
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
#if !defined(__FreeBSD__)
			sleep((caddr_t)&vp->v_numoutput, PRIBIO + 1);
#else
			tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "extfsn", 0);
#endif
		}
#if DIAGNOSTIC
		if (vp->v_dirtyblkhd.lh_first) {
			vprint("ext2_fsync: dirty", vp);
			goto loop;
		}
#endif
	}
	splx(s);
	gettime(&tv);
	return (VOP_UPDATE(ap->a_vp, &tv, &tv, ap->a_waitfor == MNT_WAIT));
}
