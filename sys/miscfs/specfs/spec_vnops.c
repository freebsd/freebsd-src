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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#include <sys/tty.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>

static int	spec_advlock __P((struct vop_advlock_args *));  
static int	spec_badop __P((void));
static int	spec_bmap __P((struct vop_bmap_args *));
static int	spec_close __P((struct vop_close_args *));
static int	spec_freeblks __P((struct vop_freeblks_args *));
static int	spec_fsync __P((struct  vop_fsync_args *));
static int	spec_getpages __P((struct vop_getpages_args *));
static int	spec_inactive __P((struct  vop_inactive_args *));
static int	spec_ioctl __P((struct vop_ioctl_args *));
static int	spec_lookup __P((struct vop_lookup_args *));
static int	spec_open __P((struct vop_open_args *));
static int	spec_poll __P((struct vop_poll_args *));
static int	spec_print __P((struct vop_print_args *));
static int	spec_read __P((struct vop_read_args *));  
static int	spec_bufread __P((struct vop_read_args *));  
static int	spec_strategy __P((struct vop_strategy_args *));
static int	spec_write __P((struct vop_write_args *));
static int	spec_bufwrite __P((struct vop_write_args *));

vop_t **spec_vnodeop_p;
static struct vnodeopv_entry_desc spec_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) vop_ebadf },
	{ &vop_advlock_desc,		(vop_t *) spec_advlock },
	{ &vop_bmap_desc,		(vop_t *) spec_bmap },
	{ &vop_close_desc,		(vop_t *) spec_close },
	{ &vop_create_desc,		(vop_t *) spec_badop },
	{ &vop_freeblks_desc,		(vop_t *) spec_freeblks },
	{ &vop_fsync_desc,		(vop_t *) spec_fsync },
	{ &vop_getpages_desc,		(vop_t *) spec_getpages },
	{ &vop_inactive_desc,		(vop_t *) spec_inactive },
	{ &vop_ioctl_desc,		(vop_t *) spec_ioctl },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_link_desc,		(vop_t *) spec_badop },
	{ &vop_lookup_desc,		(vop_t *) spec_lookup },
	{ &vop_mkdir_desc,		(vop_t *) spec_badop },
	{ &vop_mknod_desc,		(vop_t *) spec_badop },
	{ &vop_open_desc,		(vop_t *) spec_open },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_poll_desc,		(vop_t *) spec_poll },
	{ &vop_print_desc,		(vop_t *) spec_print },
	{ &vop_read_desc,		(vop_t *) spec_read },
	{ &vop_readdir_desc,		(vop_t *) spec_badop },
	{ &vop_readlink_desc,		(vop_t *) spec_badop },
	{ &vop_reallocblks_desc,	(vop_t *) spec_badop },
	{ &vop_reclaim_desc,		(vop_t *) vop_null },
	{ &vop_remove_desc,		(vop_t *) spec_badop },
	{ &vop_rename_desc,		(vop_t *) spec_badop },
	{ &vop_rmdir_desc,		(vop_t *) spec_badop },
	{ &vop_setattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_strategy_desc,		(vop_t *) spec_strategy },
	{ &vop_symlink_desc,		(vop_t *) spec_badop },
	{ &vop_write_desc,		(vop_t *) spec_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc spec_vnodeop_opv_desc =
	{ &spec_vnodeop_p, spec_vnodeop_entries };

VNODEOP_SET(spec_vnodeop_opv_desc);

static int bdev_buffered = 0;
SYSCTL_INT(_vfs, OID_AUTO, bdev_buffered, CTLFLAG_RW, &bdev_buffered, 0, "");

int
spec_vnoperate(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap;
{
	return (VOCALL(spec_vnodeop_p, ap->a_desc->vdesc_offset, ap));
}

static void spec_getpages_iodone __P((struct buf *bp));

/*
 * Trivial lookup routine that always fails.
 */
static int
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
static int
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
	dev_t bdev, dev = vp->v_rdev;
	int error;
	struct cdevsw *dsw;
	const char *cp;

	/*
	 * Don't allow open if fs is mounted -nodev.
	 */
	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
		return (ENXIO);

	dsw = devsw(dev);
	if ( (dsw == NULL) || (dsw->d_open == NULL))
		return ENXIO;

	/* Make this field valid before any I/O in ->d_open */
	if (!dev->si_iosize_max)
		dev->si_iosize_max = DFLTPHYS;

	switch (vp->v_type) {
	case VCHR:
		if (ap->a_cred != FSCRED && (ap->a_mode & FWRITE)) {
			/*
			 * When running in very secure mode, do not allow
			 * opens for writing of any disk character devices.
			 */
			if (securelevel >= 2
			    && dsw->d_bmaj != -1
			    && (dsw->d_flags & D_TYPEMASK) == D_DISK)
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
		if ((dsw->d_flags & D_TYPEMASK) == D_TTY)
			vp->v_flag |= VISTTY;
		VOP_UNLOCK(vp, 0, p);
		error = (*dsw->d_open)(dev, ap->a_mode, S_IFCHR, p);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		break;
	case VBLK:
		/*
		 * When running in very secure mode, do not allow
		 * opens for writing of any disk block devices.
		 */
		if (securelevel >= 2 && ap->a_cred != FSCRED &&
		    (ap->a_mode & FWRITE) &&
		    (dsw->d_flags & D_TYPEMASK) == D_DISK)
			return (EPERM);

		/*
		 * Do not allow opens of block devices that are
		 * currently mounted.
		 */
		error = vfs_mountedon(vp);
		if (error)
			return (error);
		error = (*dsw->d_open)(dev, ap->a_mode, S_IFBLK, p);
		break;
	default:
		error = ENXIO;
		break;
	}

	if (error)
		return (error);

	if (dsw->d_flags & D_TTY) {
		if (dev->si_tty) {
			struct tty *tp;
			tp = dev->si_tty;
			if (!tp->t_stop) {
				printf("Warning:%s: no t_stop, using nottystop\n", devtoname(dev));
				tp->t_stop = nottystop;
			}
		}
	}

	if (vn_isdisk(vp)) {
		if (!dev->si_bsize_phys)
			dev->si_bsize_phys = DEV_BSIZE;
	}
	if ((dsw->d_flags & D_DISK) == 0) {
		cp = devtoname(dev);
		if (*cp == '#' && (dsw->d_flags & D_NAGGED) == 0) {
			printf("WARNING: driver %s should register devices with make_dev() (dev_t = \"%s\")\n",
			    dsw->d_name, cp);
			dsw->d_flags |= D_NAGGED;	
		}
	}
	return (error);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
static int
spec_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
 	struct proc *p = uio->uio_procp;
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("spec_read mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_read proc");
#endif
	if (uio->uio_resid == 0)
		return (0);

	if (vp->v_type == VCHR || (bdev_buffered == 0)) {
		VOP_UNLOCK(vp, 0, p);
		error = (*devsw(vp->v_rdev)->d_read)
			(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);
	} else {
		return (spec_bufread(ap));
	}
}


/* Vnode op for buffered read */
/* ARGSUSED */
static int
spec_bufread(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
 	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn, nextbn;
	long bsize, bscale;
	struct partinfo dpart;
	int n, on;
	d_ioctl_t *ioctl;
	int error = 0;
	int seqcount = ap->a_ioflag >> 16;
	dev_t dev;

	if (uio->uio_offset < 0)
		return (EINVAL);
	dev = vp->v_rdev;

	/*
	 * Calculate block size for block device.  The block size must
	 * be larger then the physical minimum.
	 */

	bsize = vp->v_rdev->si_bsize_best;
	if (bsize < vp->v_rdev->si_bsize_phys)
		bsize = vp->v_rdev->si_bsize_phys;
	if (bsize < BLKDEV_IOSIZE)
		bsize = BLKDEV_IOSIZE;

	if ((ioctl = devsw(dev)->d_ioctl) != NULL &&
	    (*ioctl)(dev, DIOCGPART, (caddr_t)&dpart, FREAD, p) == 0 &&
	    dpart.part->p_fstype == FS_BSDFFS &&
	    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
		bsize = dpart.part->p_frag * dpart.part->p_fsize;
	bscale = btodb(bsize);
	do {
		bn = btodb(uio->uio_offset) & ~(bscale - 1);
		on = uio->uio_offset % bsize;
		if (seqcount > 1) {
			nextbn = bn + bscale;
			error = breadn(vp, bn, (int)bsize, &nextbn,
				(int *)&bsize, 1, NOCRED, &bp);
		} else {
			error = bread(vp, bn, (int)bsize, NOCRED, &bp);
		}

		/*
		 * Figure out how much of the buffer is valid relative
		 * to our offset into the buffer, which may be negative
		 * if we are beyond the EOF.
		 *
		 * The valid size of the buffer is based on 
		 * bp->b_bcount (which may have been truncated by
		 * dscheck or the device) minus bp->b_resid, which
		 * may be indicative of an I/O error if non-zero.
		 */
		if (error == 0) {
			n = bp->b_bcount - on;
			if (n < 0) {
				error = EINVAL;
			} else {
				n = min(n, bp->b_bcount - bp->b_resid - on);
				if (n < 0)
					error = EIO;
			}
		}
		if (error) {
			brelse(bp);
			return (error);
		}
		n = min(n, uio->uio_resid);
		error = uiomove((char *)bp->b_data + on, n, uio);
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	return (error);
	/* NOTREACHED */
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
static int
spec_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("spec_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_write proc");
#endif

	if (vp->v_type == VCHR || (bdev_buffered == 0)) {
		VOP_UNLOCK(vp, 0, p);
		error = (*devsw(vp->v_rdev)->d_write)
			(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);
	} else {
		return (spec_bufwrite(ap));
	}
}


/* Vnode op for buffered write */
/* ARGSUSED */
static int
spec_bufwrite(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	int bsize, blkmask;
	struct partinfo dpart;
	register int n, on;
	int error = 0;

	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);

	/*
	 * Calculate block size for block device.  The block size must
	 * be larger then the physical minimum.
	 */
	bsize = vp->v_rdev->si_bsize_best;
	if (bsize < vp->v_rdev->si_bsize_phys)
		bsize = vp->v_rdev->si_bsize_phys;
	if (bsize < BLKDEV_IOSIZE)
		bsize = BLKDEV_IOSIZE;

	if ((*devsw(vp->v_rdev)->d_ioctl)(vp->v_rdev, DIOCGPART,
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

		/*
		 * Calculate potential request size, determine
		 * if we can avoid a read-before-write.
		 */
		n = min((unsigned)(bsize - on), uio->uio_resid);
		if (n == bsize)
			bp = getblk(vp, bn, bsize, 0, 0);
		else
			error = bread(vp, bn, bsize, NOCRED, &bp);

		/*
		 * n is the amount of effective space in the buffer
		 * that we wish to write relative to our offset into
		 * the buffer. We have to truncate it to the valid
		 * size of the buffer relative to our offset into
		 * the buffer (which may end up being negative if
		 * we are beyond the EOF).
		 *
		 * The valid size of the buffer is based on 
		 * bp->b_bcount (which may have been truncated by
		 * dscheck or the device) minus bp->b_resid, which
		 * may be indicative of an I/O error if non-zero.
		 *
		 * XXX In a newly created buffer, b_bcount == bsize
		 * and, being asynchronous, we have no idea of the
		 * EOF.
		 */
		if (error == 0) {
			n = min(n, bp->b_bcount - on);
			if (n < 0) {
				error = EINVAL;
			} else {
				n = min(n, bp->b_bcount - bp->b_resid - on);
				if (n < 0)
					error = EIO;
			}
		}
		if (error) {
			brelse(bp);
			return (error);
		}
		error = uiomove((char *)bp->b_data + on, n, uio);
		if (n + on == bsize)
			bawrite(bp);
		else
			bdwrite(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	return (error);
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
static int
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
		return ((*devsw(dev)->d_ioctl)(dev, ap->a_command, 
		    ap->a_data, ap->a_fflag, ap->a_p));
	case VBLK:
		return ((*devsw(dev)->d_ioctl)(dev, ap->a_command, 
		    ap->a_data, ap->a_fflag, ap->a_p));
	default:
		panic("spec_ioctl");
		/* NOTREACHED */
	}
}

/* ARGSUSED */
static int
spec_poll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register dev_t dev;

	switch (ap->a_vp->v_type) {

	case VCHR:
		dev = ap->a_vp->v_rdev;
		return (*devsw(dev)->d_poll)(dev, ap->a_events, ap->a_p);
	default:
		return (vop_defaultop((struct vop_generic_args *)ap));

	}
}
/*
 * Synch buffers associated with a block device
 */
/* ARGSUSED */
static int
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
	for (bp = TAILQ_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = TAILQ_NEXT(bp, b_vnbufs);
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("spec_fsync: not dirty");
		if ((vp->v_flag & VOBJBUF) && (bp->b_flags & B_CLUSTEROK)) {
			BUF_UNLOCK(bp);
			vfs_bio_awrite(bp);
			splx(s);
		} else {
			bremfree(bp);
			splx(s);
			bawrite(bp);
		}
		goto loop;
	}
	if (ap->a_waitfor == MNT_WAIT) {
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
			(void) tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "spfsyn", 0);
		}
#ifdef DIAGNOSTIC
		if (!TAILQ_EMPTY(&vp->v_dirtyblkhd)) {
			vprint("spec_fsync: dirty", vp);
			splx(s);
			goto loop;
		}
#endif
	}
	splx(s);
	return (0);
}

static int
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
static int
spec_strategy(ap)
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp;

	bp = ap->a_bp;
	if (((bp->b_flags & B_READ) == 0) &&
		(LIST_FIRST(&bp->b_dep)) != NULL && bioops.io_start)
		(*bioops.io_start)(bp);
	KASSERT(devsw(bp->b_dev) != NULL, 
	   ("No devsw on dev %s responsible for buffer %p\n", 
	   devtoname(bp->b_dev), bp));
	KASSERT(devsw(bp->b_dev)->d_strategy != NULL, 
	   ("No strategy on dev %s responsible for buffer %p\n", 
	   devtoname(bp->b_dev), bp));
	BUF_STRATEGY(bp, 0);
	return (0);
}

static int
spec_freeblks(ap)
	struct vop_freeblks_args /* {
		struct vnode *a_vp;
		daddr_t a_addr;
		daddr_t a_length;
	} */ *ap;
{
	struct cdevsw *bsw;
	struct buf *bp;

	bsw = devsw(ap->a_vp->v_rdev);
	if ((bsw->d_flags & D_CANFREE) == 0)
		return (0);
	bp = geteblk(ap->a_length);
	bp->b_flags |= B_FREEBUF;
	bp->b_dev = ap->a_vp->v_rdev;
	bp->b_blkno = ap->a_addr;
	bp->b_offset = dbtob(ap->a_addr);
	bp->b_bcount = ap->a_length;
	BUF_STRATEGY(bp, 0);
	return (0);
}

/*
 * Implement degenerate case where the block requested is the block
 * returned, and assume that the entire device is contiguous in regards
 * to the contiguous block range (runp and runb).
 */
static int
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
	struct vnode *vp = ap->a_vp;
	int runp = 0;
	int runb = 0;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (vp->v_type == VBLK && vp->v_mount != NULL)
		runp = runb = MAXBSIZE / vp->v_mount->mnt_stat.f_iosize;
	if (ap->a_runp != NULL)
		*ap->a_runp = runp;
	if (ap->a_runb != NULL)
		*ap->a_runb = runb;
	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
static int
spec_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	dev_t dev = vp->v_rdev;
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
		if (vcount(vp) == 2 && p && (vp->v_flag & VXLOCK) == 0 &&
		    vp == p->p_session->s_ttyvp) {
			vrele(vp);
			p->p_session->s_ttyvp = NULL;
		}
		mode = S_IFCHR;
		break;

	case VBLK:
		if (bdev_buffered) {
			/*
			 * On last close of a block device (that isn't mounted)
			 * we must invalidate any in core blocks, so that
			 * we can, for instance, change floppy disks.
			 */
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
			error = vinvalbuf(vp, V_SAVE, ap->a_cred, p, 0, 0);
			VOP_UNLOCK(vp, 0, p);
			if (error)
				return (error);
		}
		mode = S_IFBLK;
		break;

	default:
		panic("spec_close: not special");
	}
	/*
	 * We do not want to really close the device if it
	 * is still in use unless we are trying to close it
	 * forcibly. Since every use (buffer, vnode, swap, cmap)
	 * holds a reference to the vnode, and because we mark
	 * any other vnodes that alias this device, when the
	 * sum of the reference counts on all the aliased
	 * vnodes descends to one, we are on last close.
	 */
	if (vp->v_flag & VXLOCK) {
		/* Forced close */
	} else if (devsw(dev)->d_flags & D_TRACKCLOSE) {
		/* Keep device updated on status */
	} else if (vcount(vp) > 1) {
		return (0);
	}
	return (devsw(dev)->d_close(dev, ap->a_fflag, mode, p));
}

/*
 * Print out the contents of a special device vnode.
 */
static int
spec_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_NON, dev %s\n", devtoname(ap->a_vp->v_rdev));
	return (0);
}

/*
 * Special device advisory byte-level locks.
 */
/* ARGSUSED */
static int
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
 * Special device bad operation
 */
static int
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

static int
spec_getpages(ap)
	struct vop_getpages_args *ap;
{
	vm_offset_t kva;
	int error;
	int i, pcount, size, s;
	daddr_t blkno;
	struct buf *bp;
	vm_page_t m;
	vm_ooffset_t offset;
	int toff, nextoff, nread;
	struct vnode *vp = ap->a_vp;
	int blksiz;
	int gotreqpage;

	error = 0;
	pcount = round_page(ap->a_count) / PAGE_SIZE;

	/*
	 * Calculate the offset of the transfer and do sanity check.
	 * FreeBSD currently only supports an 8 TB range due to b_blkno
	 * being in DEV_BSIZE ( usually 512 ) byte chunks on call to
	 * VOP_STRATEGY.  XXX
	 */
	offset = IDX_TO_OFF(ap->a_m[0]->pindex) + ap->a_offset;

#define	DADDR_T_BIT	(sizeof(daddr_t)*8)
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
	 * Round up physical size for real devices.  We cannot round using
	 * v_mount's block size data because v_mount has nothing to do with
	 * the device.  i.e. it's usually '/dev'.  We need the physical block
	 * size for the device itself.
	 *
	 * We can't use v_specmountpoint because it only exists when the
	 * block device is mounted.  However, we can use v_rdev.
	 */

	if (vp->v_type == VBLK)
		blksiz = vp->v_rdev->si_bsize_phys;
	else
		blksiz = DEV_BSIZE;

	size = (ap->a_count + blksiz - 1) & ~(blksiz - 1);

	bp = getpbuf(NULL);
	kva = (vm_offset_t)bp->b_data;

	/*
	 * Map the pages to be read into the kva.
	 */
	pmap_qenter(kva, ap->a_m, pcount);

	/* Build a minimal buffer header. */
	bp->b_flags = B_READ | B_CALL;
	bp->b_iodone = spec_getpages_iodone;

	/* B_PHYS is not set, but it is nice to fill this in. */
	bp->b_rcred = bp->b_wcred = curproc->p_ucred;
	if (bp->b_rcred != NOCRED)
		crhold(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crhold(bp->b_wcred);
	bp->b_blkno = blkno;
	bp->b_lblkno = blkno;
	pbgetvp(ap->a_vp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;
	bp->b_resid = 0;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += pcount;

	/* Do the input. */
	VOP_STRATEGY(bp->b_vp, bp);

	s = splbio();

	/* We definitely need to be at splbio here. */
	while ((bp->b_flags & B_DONE) == 0)
		tsleep(bp, PVM, "spread", 0);

	splx(s);

	if ((bp->b_flags & B_ERROR) != 0) {
		if (bp->b_error)
			error = bp->b_error;
		else
			error = EIO;
	}

	nread = size - bp->b_resid;

	if (nread < ap->a_count) {
		bzero((caddr_t)kva + nread,
			ap->a_count - nread);
	}
	pmap_qremove(kva, pcount);


	gotreqpage = 0;
	for (i = 0, toff = 0; i < pcount; i++, toff = nextoff) {
		nextoff = toff + PAGE_SIZE;
		m = ap->a_m[i];

		m->flags &= ~PG_ZERO;

		if (nextoff <= nread) {
			m->valid = VM_PAGE_BITS_ALL;
			vm_page_undirty(m);
		} else if (toff < nread) {
			/*
			 * Since this is a VM request, we have to supply the
			 * unaligned offset to allow vm_page_set_validclean()
			 * to zero sub-DEV_BSIZE'd portions of the page.
			 */
			vm_page_set_validclean(m, 0, nread - toff);
		} else {
			m->valid = 0;
			vm_page_undirty(m);
		}

		if (i != ap->a_reqpage) {
			/*
			 * Just in case someone was asking for this page we
			 * now tell them that it is ok to use.
			 */
			if (!error || (m->valid == VM_PAGE_BITS_ALL)) {
				if (m->valid) {
					if (m->flags & PG_WANTED) {
						vm_page_activate(m);
					} else {
						vm_page_deactivate(m);
					}
					vm_page_wakeup(m);
				} else {
					vm_page_free(m);
				}
			} else {
				vm_page_free(m);
			}
		} else if (m->valid) {
			gotreqpage = 1;
			/*
			 * Since this is a VM request, we need to make the
			 * entire page presentable by zeroing invalid sections.
			 */
			if (m->valid != VM_PAGE_BITS_ALL)
			    vm_page_zero_invalid(m, FALSE);
		}
	}
	if (!gotreqpage) {
		m = ap->a_m[ap->a_reqpage];
#ifndef MAX_PERF
		printf(
	    "spec_getpages:(%s) I/O read failure: (error=%d) bp %p vp %p\n",
			devtoname(bp->b_dev), error, bp, bp->b_vp);
		printf(
	    "               size: %d, resid: %ld, a_count: %d, valid: 0x%x\n",
		    size, bp->b_resid, ap->a_count, m->valid);
		printf(
	    "               nread: %d, reqpage: %d, pindex: %lu, pcount: %d\n",
		    nread, ap->a_reqpage, (u_long)m->pindex, pcount);
#endif
		/*
		 * Free the buffer header back to the swap buffer pool.
		 */
		relpbuf(bp, NULL);
		return VM_PAGER_ERROR;
	}
	/*
	 * Free the buffer header back to the swap buffer pool.
	 */
	relpbuf(bp, NULL);
	return VM_PAGER_OK;
}
