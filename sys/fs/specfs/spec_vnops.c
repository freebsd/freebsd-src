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
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

static int	spec_advlock(struct vop_advlock_args *);
static int	spec_close(struct vop_close_args *);
static int	spec_freeblks(struct vop_freeblks_args *);
static int	spec_fsync(struct  vop_fsync_args *);
static int	spec_getpages(struct vop_getpages_args *);
static int	spec_ioctl(struct vop_ioctl_args *);
static int	spec_kqfilter(struct vop_kqfilter_args *);
static int	spec_open(struct vop_open_args *);
static int	spec_poll(struct vop_poll_args *);
static int	spec_print(struct vop_print_args *);
static int	spec_read(struct vop_read_args *);
static int	spec_specstrategy(struct vop_specstrategy_args *);
static int	spec_write(struct vop_write_args *);

vop_t **spec_vnodeop_p;
static struct vnodeopv_entry_desc spec_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) vop_ebadf },
	{ &vop_advlock_desc,		(vop_t *) spec_advlock },
	{ &vop_bmap_desc,		(vop_t *) vop_panic },
	{ &vop_close_desc,		(vop_t *) spec_close },
	{ &vop_create_desc,		(vop_t *) vop_panic },
	{ &vop_freeblks_desc,		(vop_t *) spec_freeblks },
	{ &vop_fsync_desc,		(vop_t *) spec_fsync },
	{ &vop_getpages_desc,		(vop_t *) spec_getpages },
	{ &vop_getwritemount_desc, 	(vop_t *) vop_stdgetwritemount },
	{ &vop_ioctl_desc,		(vop_t *) spec_ioctl },
	{ &vop_kqfilter_desc,		(vop_t *) spec_kqfilter },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_link_desc,		(vop_t *) vop_panic },
	{ &vop_mkdir_desc,		(vop_t *) vop_panic },
	{ &vop_mknod_desc,		(vop_t *) vop_panic },
	{ &vop_open_desc,		(vop_t *) spec_open },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_poll_desc,		(vop_t *) spec_poll },
	{ &vop_print_desc,		(vop_t *) spec_print },
	{ &vop_read_desc,		(vop_t *) spec_read },
	{ &vop_readdir_desc,		(vop_t *) vop_panic },
	{ &vop_readlink_desc,		(vop_t *) vop_panic },
	{ &vop_reallocblks_desc,	(vop_t *) vop_panic },
	{ &vop_reclaim_desc,		(vop_t *) vop_null },
	{ &vop_remove_desc,		(vop_t *) vop_panic },
	{ &vop_rename_desc,		(vop_t *) vop_panic },
	{ &vop_rmdir_desc,		(vop_t *) vop_panic },
	{ &vop_setattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_specstrategy_desc,	(vop_t *) spec_specstrategy },
	{ &vop_strategy_desc,		(vop_t *) vop_panic },
	{ &vop_symlink_desc,		(vop_t *) vop_panic },
	{ &vop_write_desc,		(vop_t *) spec_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc spec_vnodeop_opv_desc =
	{ &spec_vnodeop_p, spec_vnodeop_entries };

VNODEOP_SET(spec_vnodeop_opv_desc);

int
spec_vnoperate(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap;
{
	return (VOCALL(spec_vnodeop_p, ap->a_desc->vdesc_offset, ap));
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
		struct thread *a_td;
	} */ *ap;
{
	struct thread *td = ap->a_td;
	struct vnode *vp = ap->a_vp;
	dev_t dev = vp->v_rdev;
	int error;
	struct cdevsw *dsw;
	const char *cp;

	if (vp->v_type == VBLK)
		return (ENXIO);

	/* Don't allow open if fs is mounted -nodev. */
	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
		return (ENXIO);

	dsw = devsw(dev);
	if (dsw == NULL || dsw->d_open == NULL)
		return (ENXIO);

	/* Make this field valid before any I/O in d_open. */
	if (dev->si_iosize_max == 0)
		dev->si_iosize_max = DFLTPHYS;

	/*
	 * XXX: Disks get special billing here, but it is mostly wrong.
	 * XXX: Disk partitions can overlap and the real checks should
	 * XXX: take this into account, and consequently they need to
	 * XXX: live in the disk slice code.  Some checks do.
	 */
	if (vn_isdisk(vp, NULL) && ap->a_cred != FSCRED &&
	    (ap->a_mode & FWRITE)) {
		/*
		 * Never allow opens for write if the disk is mounted R/W.
		 */
		if (vp->v_rdev->si_mountpoint != NULL &&
		    !(vp->v_rdev->si_mountpoint->mnt_flag & MNT_RDONLY))
			return (EBUSY);

		/*
		 * When running in secure mode, do not allow opens
		 * for writing if the disk is mounted.
		 */
		error = securelevel_ge(td->td_ucred, 1);
		if (error && vfs_mountedon(vp))
			return (error);

		/*
		 * When running in very secure mode, do not allow
		 * opens for writing of any disks.
		 */
		error = securelevel_ge(td->td_ucred, 2);
		if (error)
			return (error);
	}

	/* XXX: Special casing of ttys for deadfs.  Probably redundant. */
	if (dsw->d_flags & D_TTY)
		vp->v_vflag |= VV_ISTTY;

	VOP_UNLOCK(vp, 0, td);
	if(dsw->d_flags & D_NOGIANT) {
		DROP_GIANT();
		error = dsw->d_open(dev, ap->a_mode, S_IFCHR, td);
		PICKUP_GIANT();
	} else
		error = dsw->d_open(dev, ap->a_mode, S_IFCHR, td);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

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

	if (vn_isdisk(vp, NULL)) {
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
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp;
	struct thread *td;
	struct uio *uio;
	dev_t dev;
	int error, resid;
	struct cdevsw *dsw;

	vp = ap->a_vp;
	dev = vp->v_rdev;
	uio = ap->a_uio;
	td = uio->uio_td;
	resid = uio->uio_resid;

	if (resid == 0)
		return (0);

	dsw = devsw(dev);
	VOP_UNLOCK(vp, 0, td);
	if (dsw->d_flags & D_NOGIANT) {
		DROP_GIANT();
		error = dsw->d_read(dev, uio, ap->a_ioflag);
		PICKUP_GIANT();
	} else
		error = dsw->d_read(dev, uio, ap->a_ioflag);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (uio->uio_resid != resid || (error == 0 && resid != 0))
		vfs_timestamp(&dev->si_atime);
	return (error);
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
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp;
	struct thread *td;
	struct uio *uio;
	dev_t dev;
	int error, resid;
	struct cdevsw *dsw;

	vp = ap->a_vp;
	dev = vp->v_rdev;
	dsw = devsw(dev);
	uio = ap->a_uio;
	td = uio->uio_td;
	resid = uio->uio_resid;

	VOP_UNLOCK(vp, 0, td);
	if (dsw->d_flags & D_NOGIANT) {
		DROP_GIANT();
		error = dsw->d_write(dev, uio, ap->a_ioflag);
		PICKUP_GIANT();
	} else
		error = dsw->d_write(dev, uio, ap->a_ioflag);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (uio->uio_resid != resid || (error == 0 && resid != 0)) {
		vfs_timestamp(&dev->si_ctime);
		dev->si_mtime = dev->si_ctime;
	}
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
		u_long  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	dev_t dev;
	int error;
	struct cdevsw *dsw;

	dev = ap->a_vp->v_rdev;
	dsw = devsw(dev);
	if (dsw->d_flags & D_NOGIANT) {
		DROP_GIANT();
		error = dsw->d_ioctl(dev, ap->a_command,
		    ap->a_data, ap->a_fflag, ap->a_td);
		PICKUP_GIANT();
	} else 
		error = dsw->d_ioctl(dev, ap->a_command,
		    ap->a_data, ap->a_fflag, ap->a_td);
	if (error == ENOIOCTL)
		error = ENOTTY;
	return (error);
}

/* ARGSUSED */
static int
spec_poll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	dev_t dev;
	struct cdevsw *dsw;
	int error;

	dev = ap->a_vp->v_rdev;
	dsw = devsw(dev);
	if (dsw->d_flags & D_NOGIANT) {
		DROP_GIANT();
		error = dsw->d_poll(dev, ap->a_events, ap->a_td);
		PICKUP_GIANT();
	} else
		error = dsw->d_poll(dev, ap->a_events, ap->a_td);
	return(error);
}

/* ARGSUSED */
static int
spec_kqfilter(ap)
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap;
{
	dev_t dev;
	struct cdevsw *dsw;
	int error;

	dev = ap->a_vp->v_rdev;
	dsw = devsw(dev);
	if (dsw->d_flags & D_NOGIANT) {
		DROP_GIANT();
		error = dsw->d_kqfilter(dev, ap->a_kn);
		PICKUP_GIANT();
	} else
		error = dsw->d_kqfilter(dev, ap->a_kn);
	return (error);
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
		struct thread *a_td;
	} */ *ap;
{
	if (!vn_isdisk(ap->a_vp, NULL))
		return (0);

	return (vop_stdfsync(ap));
}

/*
 * Mutex to use when delaying niced I/O bound processes in spec_strategy().
 */
static struct mtx strategy_mtx;
static void
strategy_init(void)
{

	mtx_init(&strategy_mtx, "strategy", NULL, MTX_DEF);
}
SYSINIT(strategy, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, strategy_init, NULL)

static int doslowdown = 0;
SYSCTL_INT(_debug, OID_AUTO, doslowdown, CTLFLAG_RW, &doslowdown, 0, "");

/*
 * Just call the device strategy routine
 */
static int
spec_xstrategy(struct vnode *vp, struct buf *bp)
{
	struct mount *mp;
	int error;
	struct cdevsw *dsw;
	struct thread *td = curthread;
	
	KASSERT(bp->b_iocmd == BIO_READ ||
		bp->b_iocmd == BIO_WRITE ||
		bp->b_iocmd == BIO_DELETE, 
		("Wrong b_iocmd buf=%p cmd=%d", bp, bp->b_iocmd));

	/*
	 * Slow down disk requests for niced processes.
	 */
	if (doslowdown && td && td->td_ksegrp->kg_nice > 0) {
		mtx_lock(&strategy_mtx);
		msleep(&strategy_mtx, &strategy_mtx,
		    PPAUSE | PCATCH | PDROP, "ioslow",
		    td->td_ksegrp->kg_nice);
	}
	if (bp->b_iocmd == BIO_WRITE) {
		if ((bp->b_flags & B_VALIDSUSPWRT) == 0 &&
		    bp->b_vp != NULL && bp->b_vp->v_mount != NULL &&
		    (bp->b_vp->v_mount->mnt_kern_flag & MNTK_SUSPENDED) != 0)
			panic("spec_strategy: bad I/O");
		bp->b_flags &= ~B_VALIDSUSPWRT;
		if (LIST_FIRST(&bp->b_dep) != NULL)
			buf_start(bp);
		mp_fixme("This should require the vnode lock.");
		if ((vp->v_vflag & VV_COPYONWRITE) &&
		    vp->v_rdev->si_copyonwrite &&
		    (error = (*vp->v_rdev->si_copyonwrite)(vp, bp)) != 0 &&
		    error != EOPNOTSUPP) {
			bp->b_io.bio_error = error;
			bp->b_io.bio_flags |= BIO_ERROR;
			biodone(&bp->b_io);
			return (0);
		}
	}
	/*
	 * Collect statistics on synchronous and asynchronous read
	 * and write counts for disks that have associated filesystems.
	 */
	if (vn_isdisk(vp, NULL) && (mp = vp->v_rdev->si_mountpoint) != NULL) {
		if (bp->b_iocmd == BIO_WRITE) {
			if (bp->b_lock.lk_lockholder == LK_KERNPROC)
				mp->mnt_stat.f_asyncwrites++;
			else
				mp->mnt_stat.f_syncwrites++;
		} else {
			if (bp->b_lock.lk_lockholder == LK_KERNPROC)
				mp->mnt_stat.f_asyncreads++;
			else
				mp->mnt_stat.f_syncreads++;
		}
	}
	if (devsw(bp->b_dev) == NULL) {
		bp->b_io.bio_error = ENXIO;
		bp->b_io.bio_flags |= BIO_ERROR;
		biodone(&bp->b_io);
		return (0);
	}
	dsw = devsw(bp->b_dev);
	KASSERT(dsw->d_strategy != NULL,
	   ("No strategy on dev %s responsible for buffer %p\n",
	   devtoname(bp->b_dev), bp));
	
	if ((dsw->d_flags & D_NOGIANT) && !(bp->b_flags & B_KEEPGIANT)) {
		DROP_GIANT();
		DEV_STRATEGY(bp);
		PICKUP_GIANT();
	} else
		DEV_STRATEGY(bp);
		
	return (0);
}

static int
spec_specstrategy(ap)
	struct vop_specstrategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap;
{

	KASSERT(ap->a_vp->v_rdev == ap->a_bp->b_dev,
	    ("%s, dev %s != %s", __func__,
	    devtoname(ap->a_vp->v_rdev),
	    devtoname(ap->a_bp->b_dev)));
	return spec_xstrategy(ap->a_vp, ap->a_bp);
}

static int
spec_freeblks(ap)
	struct vop_freeblks_args /* {
		struct vnode *a_vp;
		daddr_t a_addr;
		daddr_t a_length;
	} */ *ap;
{
	struct buf *bp;

	/*
	 * XXX: This assumes that strategy does the deed right away.
	 * XXX: this may not be TRTTD.
	 */
	if ((ap->a_vp->v_rdev->si_flags & SI_CANDELETE) == 0)
		return (0);
	bp = geteblk(ap->a_length);
	bp->b_iocmd = BIO_DELETE;
	bp->b_dev = ap->a_vp->v_rdev;
	bp->b_blkno = ap->a_addr;
	bp->b_offset = dbtob(ap->a_addr);
	bp->b_bcount = ap->a_length;
	BUF_KERNPROC(bp);
	DEV_STRATEGY(bp);
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
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp, *oldvp;
	struct thread *td = ap->a_td;
	dev_t dev = vp->v_rdev;
	struct cdevsw *dsw;
	int error;

	/*
	 * Hack: a tty device that is a controlling terminal
	 * has a reference from the session structure.
	 * We cannot easily tell that a character device is
	 * a controlling terminal, unless it is the closing
	 * process' controlling terminal.  In that case,
	 * if the reference count is 2 (this last descriptor
	 * plus the session), release the reference from the session.
	 */

	/*
	 * This needs to be rewritten to take the vp interlock into
	 * consideration.
	 */

	dsw = devsw(dev);
	oldvp = NULL;
	sx_xlock(&proctree_lock);
	if (td && vp == td->td_proc->p_session->s_ttyvp) {
		SESS_LOCK(td->td_proc->p_session);
		VI_LOCK(vp);
		if (vcount(vp) == 2 && (vp->v_iflag & VI_XLOCK) == 0) {
			td->td_proc->p_session->s_ttyvp = NULL;
			oldvp = vp;
		}
		VI_UNLOCK(vp);
		SESS_UNLOCK(td->td_proc->p_session);
	}
	sx_xunlock(&proctree_lock);
	if (oldvp != NULL)
		vrele(oldvp);
	/*
	 * We do not want to really close the device if it
	 * is still in use unless we are trying to close it
	 * forcibly. Since every use (buffer, vnode, swap, cmap)
	 * holds a reference to the vnode, and because we mark
	 * any other vnodes that alias this device, when the
	 * sum of the reference counts on all the aliased
	 * vnodes descends to one, we are on last close.
	 */
	VI_LOCK(vp);
	if (vp->v_iflag & VI_XLOCK) {
		/* Forced close. */
	} else if (dsw->d_flags & D_TRACKCLOSE) {
		/* Keep device updated on status. */
	} else if (vcount(vp) > 1) {
		VI_UNLOCK(vp);
		return (0);
	}
	VI_UNLOCK(vp);
	if (dsw->d_flags & D_NOGIANT) {
		DROP_GIANT();
		error = dsw->d_close(dev, ap->a_fflag, S_IFCHR, td);
		PICKUP_GIANT();
	} else
		error = dsw->d_close(dev, ap->a_fflag, S_IFCHR, td);
	return (error);
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

	printf("\tdev %s\n", devtoname(ap->a_vp->v_rdev));
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

	GIANT_REQUIRED;

	error = 0;
	pcount = round_page(ap->a_count) / PAGE_SIZE;

	/*
	 * Calculate the offset of the transfer and do a sanity check.
	 * FreeBSD currently only supports an 8 TB range due to b_blkno
	 * being in DEV_BSIZE ( usually 512 ) byte chunks on call to
	 * VOP_STRATEGY.  XXX
	 */
	offset = IDX_TO_OFF(ap->a_m[0]->pindex) + ap->a_offset;
	blkno = btodb(offset);

	/*
	 * Round up physical size for real devices.  We cannot round using
	 * v_mount's block size data because v_mount has nothing to do with
	 * the device.  i.e. it's usually '/dev'.  We need the physical block
	 * size for the device itself.
	 *
	 * We can't use v_rdev->si_mountpoint because it only exists when the
	 * block device is mounted.  However, we can use v_rdev.
	 */

	if (vn_isdisk(vp, NULL))
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
	bp->b_iocmd = BIO_READ;
	bp->b_iodone = bdone;

	/* B_PHYS is not set, but it is nice to fill this in. */
	KASSERT(bp->b_rcred == NOCRED, ("leaking read ucred"));
	KASSERT(bp->b_wcred == NOCRED, ("leaking write ucred"));
	bp->b_rcred = crhold(curthread->td_ucred);
	bp->b_wcred = crhold(curthread->td_ucred);
	bp->b_blkno = blkno;
	bp->b_lblkno = blkno;
	pbgetvp(ap->a_vp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;
	bp->b_resid = 0;
	bp->b_runningbufspace = bp->b_bufsize;
	runningbufspace += bp->b_runningbufspace;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += pcount;

	/* Do the input. */
	spec_xstrategy(bp->b_vp, bp);

	s = splbio();
	bwait(bp, PVM, "spread");
	splx(s);

	if ((bp->b_ioflags & BIO_ERROR) != 0) {
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
	VM_OBJECT_LOCK(vp->v_object);
	vm_page_lock_queues();
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
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(vp->v_object);
	if (!gotreqpage) {
		m = ap->a_m[ap->a_reqpage];
		printf(
	    "spec_getpages:(%s) I/O read failure: (error=%d) bp %p vp %p\n",
			devtoname(bp->b_dev), error, bp, bp->b_vp);
		printf(
	    "               size: %d, resid: %ld, a_count: %d, valid: 0x%x\n",
		    size, bp->b_resid, ap->a_count, m->valid);
		printf(
	    "               nread: %d, reqpage: %d, pindex: %lu, pcount: %d\n",
		    nread, ap->a_reqpage, (u_long)m->pindex, pcount);
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
