/*
 * Copyright (c) 1989, 1993, 1995
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
 *	@(#)spec_vnops.c	8.14 (Berkeley) 5/21/95
 * $Id: spec_vnops.c,v 1.38 1997/03/24 11:24:44 bde Exp $
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>

#include <miscfs/specfs/specdev.h>

static int	spec_ebadf __P((void));
static int	spec_getattr __P((struct  vop_getattr_args *));

struct vnode *speclisth[SPECHSZ];
vop_t **spec_vnodeop_p;
static struct vnodeopv_entry_desc spec_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)spec_lookup },	/* lookup */
	{ &vop_create_desc, (vop_t *)spec_create },	/* create */
	{ &vop_mknod_desc, (vop_t *)spec_mknod },	/* mknod */
	{ &vop_open_desc, (vop_t *)spec_open },		/* open */
	{ &vop_close_desc, (vop_t *)spec_close },	/* close */
	{ &vop_access_desc, (vop_t *)spec_access },	/* access */
	{ &vop_getattr_desc, (vop_t *)spec_getattr },	/* getattr */
	{ &vop_setattr_desc, (vop_t *)spec_setattr },	/* setattr */
	{ &vop_read_desc, (vop_t *)spec_read },		/* read */
	{ &vop_write_desc, (vop_t *)spec_write },	/* write */
	{ &vop_lease_desc, (vop_t *)spec_lease_check },	/* lease */
	{ &vop_ioctl_desc, (vop_t *)spec_ioctl },	/* ioctl */
	{ &vop_select_desc, (vop_t *)spec_select },	/* select */
	{ &vop_revoke_desc, (vop_t *)spec_revoke },	/* revoke */
	{ &vop_mmap_desc, (vop_t *)spec_mmap },		/* mmap */
	{ &vop_fsync_desc, (vop_t *)spec_fsync },	/* fsync */
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
	{ &vop_inactive_desc, (vop_t *)spec_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)spec_reclaim },	/* reclaim */
	{ &vop_lock_desc, (vop_t *)spec_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)spec_unlock },	/* unlock */
	{ &vop_bmap_desc, (vop_t *)spec_bmap },		/* bmap */
	{ &vop_strategy_desc, (vop_t *)spec_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *)spec_print },	/* print */
	{ &vop_islocked_desc, (vop_t *)spec_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)spec_pathconf },	/* pathconf */
	{ &vop_advlock_desc, (vop_t *)spec_advlock },	/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)spec_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)spec_valloc },	/* valloc */
	{ &vop_vfree_desc, (vop_t *)spec_vfree },	/* vfree */
	{ &vop_truncate_desc, (vop_t *)spec_truncate },	/* truncate */
	{ &vop_update_desc, (vop_t *)spec_update },	/* update */
	{ &vop_bwrite_desc, (vop_t *)vn_bwrite },	/* bwrite */
	{ &vop_getpages_desc, (vop_t *)spec_getpages},	/* getpages */
	{ NULL, NULL }
};
static struct vnodeopv_desc spec_vnodeop_opv_desc =
	{ &spec_vnodeop_p, spec_vnodeop_entries };

VNODEOP_SET(spec_vnodeop_opv_desc);

static void spec_getpages_iodone __P((struct buf *bp));

/*
 * Trivial lookup routine that always fails.
 */
int
spec_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * Open a special file.
 */
/* ARGSUSED */
int
spec_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct proc *p = ap->a_p;
	struct vnode *bvp, *vp = ap->a_vp;
	dev_t bdev, dev = (dev_t)vp->v_rdev;
	int maj = major(dev);
	int error;

	/*
	 * Don't allow open if fs is mounted -nodev.
	 */
	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
		return (ENXIO);

	switch (vp->v_type) {

	case VCHR:
		if ((u_int)maj >= nchrdev)
			return (ENXIO);
		if ( (cdevsw[maj] == NULL) || (cdevsw[maj]->d_open == NULL))
			return ENXIO;
		if (ap->a_cred != FSCRED && (ap->a_mode & FWRITE)) {
			/*
			 * When running in very secure mode, do not allow
			 * opens for writing of any disk character devices.
			 */
			if (securelevel >= 2
			    && cdevsw[maj]->d_bdev
			    && cdevsw[maj]->d_bdev->d_flags == D_DISK)
				return (EPERM);
			/*
			 * When running in secure mode, do not allow opens
			 * for writing of /dev/mem, /dev/kmem, or character
			 * devices whose corresponding block devices are
			 * currently mounted.
			 */
			if (securelevel >= 1) {
				if ((bdev = chrtoblk(dev)) != NODEV &&
				    vfinddev(bdev, VBLK, &bvp) &&
				    bvp->v_usecount > 0 &&
				    (error = vfs_mountedon(bvp)))
					return (error);
				if (iskmemdev(dev))
					return (EPERM);
			}
		}
#if 0
		/*
		 * Lite2 stuff.  We will almost certainly do this
		 * differently with devfs.  The only use of this flag
		 * is in dead_read to make ttys return EOF instead of
		 * EIO when they are dead.  Pre-lite2 FreeBSD returns
		 * EOF for all character devices.
		 */
		if (cdevsw[maj]->d_type == D_TTY)
			vp->v_flag |= VISTTY;
#endif
		VOP_UNLOCK(vp, 0, p);
		error = (*cdevsw[maj]->d_open)(dev, ap->a_mode, S_IFCHR, p);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if ((u_int)maj >= nblkdev)
			return (ENXIO);
		if ( (bdevsw[maj] == NULL) || (bdevsw[maj]->d_open == NULL))
			return ENXIO;
		/*
		 * When running in very secure mode, do not allow
		 * opens for writing of any disk block devices.
		 */
		if (securelevel >= 2 && ap->a_cred != FSCRED &&
		    (ap->a_mode & FWRITE) && bdevsw[maj]->d_flags == D_DISK)
			return (EPERM);
		/*
		 * Do not allow opens of block devices that are
		 * currently mounted.
		 */
		error = vfs_mountedon(vp);
		if (error)
			return (error);
		return ((*bdevsw[maj]->d_open)(dev, ap->a_mode, S_IFBLK, p));
	}
	return (0);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
int
spec_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
 	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn, nextbn;
	long bsize, bscale;
	struct partinfo dpart;
	int n, on, majordev;
	d_ioctl_t *ioctl;
	int error = 0;
	dev_t dev;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("spec_read mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_read proc");
#endif
	if (uio->uio_resid == 0)
		return (0);

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0, p);
		error = (*cdevsw[major(vp->v_rdev)]->d_read)
			(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		dev = vp->v_rdev;
		if ((majordev = major(dev)) < nblkdev &&
		    (ioctl = bdevsw[majordev]->d_ioctl) != NULL &&
		    (*ioctl)(dev, DIOCGPART, (caddr_t)&dpart, FREAD, p) == 0 &&
		    dpart.part->p_fstype == FS_BSDFFS &&
		    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
			bsize = dpart.part->p_frag * dpart.part->p_fsize;
		bscale = btodb(bsize);
		do {
			bn = btodb(uio->uio_offset) & ~(bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (vp->v_lastr + bscale == bn) {
				nextbn = bn + bscale;
				error = breadn(vp, bn, (int)bsize, &nextbn,
					(int *)&bsize, 1, NOCRED, &bp);
			} else
				error = bread(vp, bn, (int)bsize, NOCRED, &bp);
			vp->v_lastr = bn;
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove((char *)bp->b_data + on, n, uio);
			brelse(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_read type");
	}
	/* NOTREACHED */
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
int
spec_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	int bsize, blkmask;
	struct partinfo dpart;
	register int n, on;
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("spec_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_write proc");
#endif

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0, p);
		error = (*cdevsw[major(vp->v_rdev)]->d_write)
			(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if (uio->uio_resid == 0)
			return (0);
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if ((*bdevsw[major(vp->v_rdev)]->d_ioctl)(vp->v_rdev, DIOCGPART,
		    (caddr_t)&dpart, FREAD, p) == 0) {
			if (dpart.part->p_fstype == FS_BSDFFS &&
			    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
				bsize = dpart.part->p_frag *
				    dpart.part->p_fsize;
		}
		blkmask = btodb(bsize) - 1;
		do {
			bn = btodb(uio->uio_offset) & ~blkmask;
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (n == bsize)
				bp = getblk(vp, bn, bsize, 0, 0);
			else
				error = bread(vp, bn, bsize, NOCRED, &bp);
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove((char *)bp->b_data + on, n, uio);
			if (n + on == bsize) {
				/* bawrite(bp); */
				cluster_write(bp, 0);
			} else
				bdwrite(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_write type");
	}
	/* NOTREACHED */
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
int
spec_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	dev_t dev = ap->a_vp->v_rdev;

	switch (ap->a_vp->v_type) {

	case VCHR:
		return ((*cdevsw[major(dev)]->d_ioctl)(dev, ap->a_command, ap->a_data,
		    ap->a_fflag, ap->a_p));

	case VBLK:
		if (ap->a_command == 0 && (int)ap->a_data == B_TAPE)
			if (bdevsw[major(dev)]->d_flags == D_TAPE)
				return (0);
			else
				return (1);
		return ((*bdevsw[major(dev)]->d_ioctl)(dev, ap->a_command, ap->a_data,
		   ap->a_fflag, ap->a_p));

	default:
		panic("spec_ioctl");
		/* NOTREACHED */
	}
}

/* ARGSUSED */
int
spec_select(ap)
	struct vop_select_args /* {
		struct vnode *a_vp;
		int  a_which;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register dev_t dev;

	switch (ap->a_vp->v_type) {

	default:
		return (1);		/* XXX */

	case VCHR:
		dev = ap->a_vp->v_rdev;
		return (*cdevsw[major(dev)]->d_select)(dev, ap->a_which, ap->a_p);
	}
}
/*
 * Synch buffers associated with a block device
 */
/* ARGSUSED */
int
spec_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int  a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct buf *bp;
	struct buf *nbp;
	int s;

	if (vp->v_type == VCHR)
		return (0);
	/*
	 * Flush all dirty buffers associated with a block device.
	 */
loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("spec_fsync: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		bawrite(bp);
		goto loop;
	}
	if (ap->a_waitfor == MNT_WAIT) {
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
			(void) tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "spfsyn", 0);
		}
#ifdef DIAGNOSTIC
		if (vp->v_dirtyblkhd.lh_first) {
			vprint("spec_fsync: dirty", vp);
			splx(s);
			goto loop;
		}
#endif
	}
	splx(s);
	return (0);
}

int
spec_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{

	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
	return (0);
}

/*
 * Just call the device strategy routine
 */
int
spec_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{

	(*bdevsw[major(ap->a_bp->b_dev)]->d_strategy)(ap->a_bp);
	return (0);
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
spec_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
int
spec_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	dev_t dev = vp->v_rdev;
	d_close_t *devclose;
	int mode, error;

	switch (vp->v_type) {

	case VCHR:
		/*
		 * Hack: a tty device that is a controlling terminal
		 * has a reference from the session structure.
		 * We cannot easily tell that a character device is
		 * a controlling terminal, unless it is the closing
		 * process' controlling terminal.  In that case,
		 * if the reference count is 2 (this last descriptor
		 * plus the session), release the reference from the session.
		 */
		if (vcount(vp) == 2 && ap->a_p &&
		    vp == ap->a_p->p_session->s_ttyvp) {
			vrele(vp);
			ap->a_p->p_session->s_ttyvp = NULL;
		}
		/*
		 * If the vnode is locked, then we are in the midst
		 * of forcably closing the device, otherwise we only
		 * close on last reference.
		 */
		if (vcount(vp) > 1 && (vp->v_flag & VXLOCK) == 0)
			return (0);
		devclose = cdevsw[major(dev)]->d_close;
		mode = S_IFCHR;
		break;

	case VBLK:
		/*
		 * On last close of a block device (that isn't mounted)
		 * we must invalidate any in core blocks, so that
		 * we can, for instance, change floppy disks.
		 */
		error = vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 0, 0);
		if (error)
			return (error);
		/*
		 * We do not want to really close the device if it
		 * is still in use unless we are trying to close it
		 * forcibly. Since every use (buffer, vnode, swap, cmap)
		 * holds a reference to the vnode, and because we mark
		 * any other vnodes that alias this device, when the
		 * sum of the reference counts on all the aliased
		 * vnodes descends to one, we are on last close.
		 */
		if ((vcount(vp) > (vp->v_object?2:1)) &&
			(vp->v_flag & VXLOCK) == 0)
			return (0);

		if (vp->v_object) {
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
			vnode_pager_uncache(vp, p);
			VOP_UNLOCK(vp, 0, p);
		}

		devclose = bdevsw[major(dev)]->d_close;
		mode = S_IFBLK;
		break;

	default:
		panic("spec_close: not special");
	}

	return ((*devclose)(dev, ap->a_fflag, mode, ap->a_p));
}

/*
 * Print out the contents of a special device vnode.
 */
int
spec_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_NON, dev %d, %d\n", major(ap->a_vp->v_rdev),
		minor(ap->a_vp->v_rdev));
	return (0);
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
spec_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Special device advisory byte-level locks.
 */
/* ARGSUSED */
int
spec_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{

	return (ap->a_flags & F_FLOCK ? EOPNOTSUPP : EINVAL);
}

/*
 * Special device failed operation
 */
static int
spec_ebadf()
{

	return (EBADF);
}

/*
 * Special device bad operation
 */
int
spec_badop()
{

	panic("spec_badop called");
	/* NOTREACHED */
}

static void
spec_getpages_iodone(bp)
	struct buf *bp;
{

	bp->b_flags |= B_DONE;
	wakeup(bp);
}

int
spec_getpages(ap)
	struct vop_getpages_args *ap;
{
	vm_offset_t kva;
	int error;
	int i, pcount, size, s;
	daddr_t blkno;
	struct buf *bp;
	vm_ooffset_t offset;
	struct vnode *vp = ap->a_vp;
	int blksiz;

	error = 0;
	pcount = round_page(ap->a_count) / PAGE_SIZE;

	/*
	 * Calculate the offset of the transfer.
	 */
	offset = IDX_TO_OFF(ap->a_m[0]->pindex) + ap->a_offset;

	/* XXX sanity check before we go into details. */
	/* XXX limits should be defined elsewhere. */
#define	DADDR_T_BIT	32
#define	OFFSET_MAX	((1LL << (DADDR_T_BIT + DEV_BSHIFT)) - 1)
	if (offset < 0 || offset > OFFSET_MAX) {
		/* XXX still no %q in kernel. */
		printf("spec_getpages: preposterous offset 0x%x%08x\n",
		       (u_int)((u_quad_t)offset >> 32),
		       (u_int)(offset & 0xffffffff));
		return (VM_PAGER_ERROR);
	}

	blkno = btodb(offset);

	/*
	 * Round up physical size for real devices, use the
	 * fundamental blocksize of the fs if possible.
	 */
	if (vp && vp->v_mount)
		blksiz = vp->v_mount->mnt_stat.f_bsize;
	else
		blksiz = DEV_BSIZE;
	size = (ap->a_count + blksiz - 1) & ~(blksiz - 1);

	bp = getpbuf();
	kva = (vm_offset_t)bp->b_data;

	/*
	 * Map the pages to be read into the kva.
	 */
	pmap_qenter(kva, ap->a_m, pcount);

	/* Build a minimal buffer header. */
	bp->b_flags = B_BUSY | B_READ | B_CALL;
	bp->b_iodone = spec_getpages_iodone;

	/* B_PHYS is not set, but it is nice to fill this in. */
	bp->b_proc = curproc;
	bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
	if (bp->b_rcred != NOCRED)
		crhold(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crhold(bp->b_wcred);
	bp->b_blkno = blkno;
	bp->b_lblkno = blkno;
	pbgetvp(ap->a_vp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += pcount;

	/* Do the input. */
	VOP_STRATEGY(bp);
	if (bp->b_flags & B_ASYNC)
		return (VM_PAGER_PEND);

	s = splbio();

	/* We definitely need to be at splbio here. */
	while ((bp->b_flags & B_DONE) == 0)
		tsleep(bp, PVM, "vnread", 0);

	splx(s);

	if ((bp->b_flags & B_ERROR) != 0)
		error = EIO;

	if (!error && ap->a_count != pcount * PAGE_SIZE)
		bzero((caddr_t)kva + ap->a_count,
		      PAGE_SIZE * pcount - ap->a_count);
	pmap_qremove(kva, pcount);

	/*
	 * Free the buffer header back to the swap buffer pool.
	 */
	relpbuf(bp);

	for (i = 0; i < pcount; i++) {
		ap->a_m[i]->dirty = 0;
		ap->a_m[i]->valid = VM_PAGE_BITS_ALL;
		ap->a_m[i]->flags &= ~PG_ZERO;
		if (i != ap->a_reqpage) {
			/*
			 * Whether or not to leave the page activated is up in
			 * the air, but we should put the page on a page queue
			 * somewhere (it already is in the object).  Result:
			 * It appears that emperical results show that
			 * deactivating pages is best.
			 */

			/*
			 * Just in case someone was asking for this page we
			 * now tell them that it is ok to use.
			 */
			if (!error) {
				vm_page_deactivate(ap->a_m[i]);
				PAGE_WAKEUP(ap->a_m[i]);
			} else
				vnode_pager_freepage(ap->a_m[i]);
		}
	}
	if (error)
		printf("spec_getpages: I/O read error\n");
	return (error ? VM_PAGER_ERROR : VM_PAGER_OK);
}

/* ARGSUSED */
static int
spec_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct vattr *vap = ap->a_vap;
	struct partinfo dpart;

	bzero(vap, sizeof (*vap));

	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;

	if ((*bdevsw[major(vp->v_rdev)]->d_ioctl)(vp->v_rdev, DIOCGPART,
	    (caddr_t)&dpart, FREAD, ap->a_p) == 0) {
		vap->va_bytes = dbtob(dpart.disklab->d_partitions
				      [minor(vp->v_rdev)].p_size);
		vap->va_size = vap->va_bytes;
	}
	return (0);
}
