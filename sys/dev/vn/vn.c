/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah Hdr: vn.c 1.13 94/04/02
 *
 *	from: @(#)vn.c	8.6 (Berkeley) 4/1/94
 * $FreeBSD$
 */

/*
 * Vnode disk driver.
 *
 * Block/character interface to a vnode.  Allows one to treat a file
 * as a disk (e.g. build a filesystem in it, mount it, etc.).
 *
 * NOTE 1: This uses the VOP_BMAP/VOP_STRATEGY interface to the vnode
 * instead of a simple VOP_RDWR.  We do this to avoid distorting the
 * local buffer cache.
 *
 * NOTE 2: There is a security issue involved with this driver.
 * Once mounted all access to the contents of the "mapped" file via
 * the special file is controlled by the permissions on the special
 * file, the protection of the mapped file is ignored (effectively,
 * by using root credentials in all transactions).
 *
 * NOTE 3: Doesn't interact with leases, should it?
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/vnioctl.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

static	d_ioctl_t	vnioctl;
static	d_open_t	vnopen;
static	d_close_t	vnclose;
static	d_psize_t	vnsize;
static	d_strategy_t	vnstrategy;

#define CDEV_MAJOR 43
#define BDEV_MAJOR 15

#define VN_BSIZE_BEST	8192

/*
 * cdevsw
 *	D_DISK		we want to look like a disk
 *	D_CANFREE	We support BIO_DELETE
 */

static struct cdevsw vn_cdevsw = {
	/* open */	vnopen,
	/* close */	vnclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	vnioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	vnstrategy,
	/* name */	"vn",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	vnsize,
	/* flags */	D_DISK|D_CANFREE,
	/* bmaj */	BDEV_MAJOR
};

#define	getvnbuf()	\
	((struct buf *)malloc(sizeof(struct buf), M_DEVBUF, M_WAITOK))

#define putvnbuf(bp)	\
	free((caddr_t)(bp), M_DEVBUF)

struct vn_softc {
	int		sc_unit;
	int		sc_flags;	/* flags 			*/
	int		sc_size;	/* size of vn, sc_secsize scale	*/
	int		sc_secsize;	/* sector size			*/
	struct diskslices *sc_slices;
	struct vnode	*sc_vp;		/* vnode if not NULL		*/
	vm_object_t	sc_object;	/* backing object if not NULL	*/
	struct ucred	*sc_cred;	/* credentials 			*/
	int		 sc_maxactive;	/* max # of active requests 	*/
	u_long		 sc_options;	/* options 			*/
	SLIST_ENTRY(vn_softc) sc_list;
};

static SLIST_HEAD(, vn_softc) vn_list;

/* sc_flags */
#define VNF_INITED	0x01
#define	VNF_READONLY	0x02

static u_long	vn_options = VN_LABELS;

#define IFOPT(vn,opt) if (((vn)->sc_options|vn_options) & (opt))
#define TESTOPT(vn,opt) (((vn)->sc_options|vn_options) & (opt))

static int	vnsetcred (struct vn_softc *vn, struct ucred *cred);
static void	vnclear (struct vn_softc *vn);
static int	vn_modevent (module_t, int, void *);
static int 	vniocattach_file (struct vn_softc *, struct vn_ioctl *, dev_t dev, int flag, struct proc *p);
static int 	vniocattach_swap (struct vn_softc *, struct vn_ioctl *, dev_t dev, int flag, struct proc *p);

static	int
vnclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct vn_softc *vn = dev->si_drv1;

	IFOPT(vn, VN_LABELS)
		if (vn->sc_slices != NULL)
			dsclose(dev, mode, vn->sc_slices);
	return (0);
}

static struct vn_softc *
vnfindvn(dev_t dev)
{
	int unit;
	struct vn_softc *vn;

	unit = dkunit(dev);
	vn = dev->si_drv1;
	if (!vn) {
		SLIST_FOREACH(vn, &vn_list, sc_list) {
			if (vn->sc_unit == unit) {
				dev->si_drv1 = vn;
				break;
			}
		}
	}
	if (!vn) {
		vn = malloc(sizeof *vn, M_DEVBUF, M_WAITOK);
		if (!vn)
			return (NULL);
		bzero(vn, sizeof *vn);
		vn->sc_unit = unit;
		dev->si_drv1 = vn;
		make_dev(&vn_cdevsw, 0, 
		    UID_ROOT, GID_OPERATOR, 0640, "vn%d", unit);
		SLIST_INSERT_HEAD(&vn_list, vn, sc_list);
	}
	return (vn);
}

static	int
vnopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct vn_softc *vn;

	/*
	 * Locate preexisting device
	 */

	if ((vn = dev->si_drv1) == NULL)
		vn = vnfindvn(dev);

	/*
	 * Update si_bsize fields for device.  This data will be overriden by
	 * the slice/parition code for vn accesses through partitions, and
	 * used directly if you open the 'whole disk' device.
	 *
	 * si_bsize_best must be reinitialized in case VN has been 
	 * reconfigured, plus make it at least VN_BSIZE_BEST for efficiency.
	 */
	dev->si_bsize_phys = vn->sc_secsize;
	dev->si_bsize_best = vn->sc_secsize;
	if (dev->si_bsize_best < VN_BSIZE_BEST)
		dev->si_bsize_best = VN_BSIZE_BEST;

	if ((flags & FWRITE) && (vn->sc_flags & VNF_READONLY))
		return (EACCES);

	IFOPT(vn, VN_FOLLOW)
		printf("vnopen(%s, 0x%x, 0x%x, %p)\n",
		    devtoname(dev), flags, mode, (void *)p);

	/*
	 * Initialize label
	 */

	IFOPT(vn, VN_LABELS) {
		if (vn->sc_flags & VNF_INITED) {
			struct disklabel label;

			/* Build label for whole disk. */
			bzero(&label, sizeof label);
			label.d_secsize = vn->sc_secsize;
			label.d_nsectors = 32;
			label.d_ntracks = 64 / (vn->sc_secsize / DEV_BSIZE);
			label.d_secpercyl = label.d_nsectors * label.d_ntracks;
			label.d_ncylinders = vn->sc_size / label.d_secpercyl;
			label.d_secperunit = vn->sc_size;
			label.d_partitions[RAW_PART].p_size = vn->sc_size;

			return (dsopen(dev, mode, 0, &vn->sc_slices, &label));
		}
		if (dkslice(dev) != WHOLE_DISK_SLICE ||
		    dkpart(dev) != RAW_PART ||
		    mode != S_IFCHR) {
			return (ENXIO);
		}
	}
	return(0);
}

/*
 *	vnstrategy:
 *
 *	Run strategy routine for VN device.  We use VOP_READ/VOP_WRITE calls
 *	for vnode-backed vn's, and the new vm_pager_strategy() call for
 *	vm_object-backed vn's.
 *
 *	NOTE: bp->b_blkno is DEV_BSIZE'd.  We must generate bp->b_pblkno for
 *	our uio or vn_pager_strategy() call that is vn->sc_secsize'd
 */

static	void
vnstrategy(struct bio *bp)
{
	int unit;
	struct vn_softc *vn;
	int error;

	unit = dkunit(bp->bio_dev);
	vn = bp->bio_dev->si_drv1;
	if (vn == NULL)
		vn = vnfindvn(bp->bio_dev);

	IFOPT(vn, VN_DEBUG)
		printf("vnstrategy(%p): unit %d\n", bp, unit);

	if ((vn->sc_flags & VNF_INITED) == 0) {
		bp->bio_error = ENXIO;
		bp->bio_flags |= BIO_ERROR;
		biodone(bp);
		return;
	}

	bp->bio_resid = bp->bio_bcount;

	IFOPT(vn, VN_LABELS) {
		if (vn->sc_slices != NULL && dscheck(bp, vn->sc_slices) <= 0) {
			bp->bio_error = EINVAL;
			bp->bio_flags |= BIO_ERROR;
			biodone(bp);
			return;
		}
	} else {
		int pbn;	/* in sc_secsize chunks */
		long sz;	/* in sc_secsize chunks */

		/*
		 * Check for required alignment.  Transfers must be a valid
		 * multiple of the sector size.
		 */
		if (bp->bio_bcount % vn->sc_secsize != 0 ||
		    bp->bio_blkno % (vn->sc_secsize / DEV_BSIZE) != 0) {
			bp->bio_error = EINVAL;
			bp->bio_flags |= BIO_ERROR;
			biodone(bp);
			return;
		}

		pbn = bp->bio_blkno / (vn->sc_secsize / DEV_BSIZE);
		sz = howmany(bp->bio_bcount, vn->sc_secsize);

		/*
		 * If out of bounds return an error.  If at the EOF point,
		 * simply read or write less.
		 */
		if (pbn < 0 || pbn >= vn->sc_size) {
			if (pbn != vn->sc_size) {
				bp->bio_error = EINVAL;
				/* XXX bp->b_flags |= B_INVAL; */
				bp->bio_flags |= BIO_ERROR;
			}
			biodone(bp);
			return;
		}

		/*
		 * If the request crosses EOF, truncate the request.
		 */
		if (pbn + sz > vn->sc_size) {
			bp->bio_bcount = (vn->sc_size - pbn) * vn->sc_secsize;
			bp->bio_resid = bp->bio_bcount;
		}
		bp->bio_pblkno = pbn;
	}

	if (vn->sc_vp && (bp->bio_cmd == BIO_DELETE)) {
		/*
		 * Not handled for vnode-backed element yet.
		 */
		biodone(bp);
	} else if (vn->sc_vp) {
		/*
		 * VNODE I/O
		 *
		 * If an error occurs, we set BIO_ERROR but we do not set 
		 * B_INVAL because (for a write anyway), the buffer is 
		 * still valid.
		 */
		struct uio auio;
		struct iovec aiov;
		struct mount *mp;

		bzero(&auio, sizeof(auio));

		aiov.iov_base = bp->bio_data;
		aiov.iov_len = bp->bio_bcount;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = (vm_ooffset_t)bp->bio_pblkno * vn->sc_secsize;
		auio.uio_segflg = UIO_SYSSPACE;
		if(bp->bio_cmd == BIO_READ)
			auio.uio_rw = UIO_READ;
		else
			auio.uio_rw = UIO_WRITE;
		auio.uio_resid = bp->bio_bcount;
		auio.uio_procp = curproc;
		if (VOP_ISLOCKED(vn->sc_vp, NULL))
			vprint("unexpected vn driver lock", vn->sc_vp);
		if (bp->bio_cmd == BIO_READ) {
			vn_lock(vn->sc_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
			error = VOP_READ(vn->sc_vp, &auio, 0, vn->sc_cred);
		} else {
			(void) vn_start_write(vn->sc_vp, &mp, V_WAIT);
			vn_lock(vn->sc_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
			error = VOP_WRITE(vn->sc_vp, &auio, 0, vn->sc_cred);
			vn_finished_write(mp);
		}
		VOP_UNLOCK(vn->sc_vp, 0, curproc);
		bp->bio_resid = auio.uio_resid;

		if (error) {
			bp->bio_error = error;
			bp->bio_flags |= BIO_ERROR;
		}
		biodone(bp);
	} else if (vn->sc_object) {
		/*
		 * OBJT_SWAP I/O
		 *
		 * ( handles read, write, freebuf )
		 *
		 * Note: if we pre-reserved swap, BIO_DELETE is disabled
		 */
#if 0
		KASSERT((bp->b_bufsize & (vn->sc_secsize - 1)) == 0,
		    ("vnstrategy: buffer %p too small for physio", bp));
#endif

		if ((bp->bio_cmd == BIO_DELETE) && TESTOPT(vn, VN_RESERVE)) {
			biodone(bp);
		} else {
			vm_pager_strategy(vn->sc_object, bp);
		}
	} else {
		bp->bio_flags |= BIO_ERROR;
		bp->bio_error = EINVAL;
		biodone(bp);
	}
}

/* ARGSUSED */
static	int
vnioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct vn_softc *vn;
	struct vn_ioctl *vio;
	int error;
	u_long *f;

	vn = dev->si_drv1;
	IFOPT(vn,VN_FOLLOW)
		printf("vnioctl(%s, 0x%lx, %p, 0x%x, %p): unit %d\n",
		    devtoname(dev), cmd, (void *)data, flag, (void *)p,
		    dkunit(dev));

	switch (cmd) {
	case VNIOCATTACH:
	case VNIOCDETACH:
	case VNIOCGSET:
	case VNIOCGCLEAR:
	case VNIOCUSET:
	case VNIOCUCLEAR:
		goto vn_specific;
	}

	IFOPT(vn,VN_LABELS) {
		if (vn->sc_slices != NULL) {
			error = dsioctl(dev, cmd, data, flag, &vn->sc_slices);
			if (error != ENOIOCTL)
				return (error);
		}
		if (dkslice(dev) != WHOLE_DISK_SLICE ||
		    dkpart(dev) != RAW_PART)
			return (ENOTTY);
	}

    vn_specific:

	error = suser(p);
	if (error)
		return (error);

	vio = (struct vn_ioctl *)data;
	f = (u_long*)data;
	switch (cmd) {

	case VNIOCATTACH:
		if (vn->sc_flags & VNF_INITED)
			return(EBUSY);

		if (vio->vn_file == NULL)
			error = vniocattach_swap(vn, vio, dev, flag, p);
		else
			error = vniocattach_file(vn, vio, dev, flag, p);
		break;

	case VNIOCDETACH:
		if ((vn->sc_flags & VNF_INITED) == 0)
			return(ENXIO);
		/*
		 * XXX handle i/o in progress.  Return EBUSY, or wait, or
		 * flush the i/o.
		 * XXX handle multiple opens of the device.  Return EBUSY,
		 * or revoke the fd's.
		 * How are these problems handled for removable and failing
		 * hardware devices? (Hint: They are not)
		 */
		vnclear(vn);
		IFOPT(vn, VN_FOLLOW)
			printf("vnioctl: CLRed\n");
		break;

	case VNIOCGSET:
		vn_options |= *f;
		*f = vn_options;
		break;

	case VNIOCGCLEAR:
		vn_options &= ~(*f);
		*f = vn_options;
		break;

	case VNIOCUSET:
		vn->sc_options |= *f;
		*f = vn->sc_options;
		break;

	case VNIOCUCLEAR:
		vn->sc_options &= ~(*f);
		*f = vn->sc_options;
		break;

	default:
		error = ENOTTY;
		break;
	}
	return(error);
}

/*
 *	vniocattach_file:
 *
 *	Attach a file to a VN partition.  Return the size in the vn_size
 *	field.
 */

static int
vniocattach_file(vn, vio, dev, flag, p)
	struct vn_softc *vn;
	struct vn_ioctl *vio;
	dev_t dev;
	int flag;
	struct proc *p;
{
	struct vattr vattr;
	struct nameidata nd;
	int error, flags;

	flags = FREAD|FWRITE;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vn_file, p);
	error = vn_open(&nd, &flags, 0);
	if (error) {
		if (error != EACCES && error != EPERM && error != EROFS)
			return (error);
		flags &= ~FWRITE;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vn_file, p);
		error = vn_open(&nd, &flags, 0);
		if (error)
			return (error);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_vp->v_type != VREG ||
	    (error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p))) {
		VOP_UNLOCK(nd.ni_vp, 0, p);
		(void) vn_close(nd.ni_vp, flags, p->p_ucred, p);
		return (error ? error : EINVAL);
	}
	VOP_UNLOCK(nd.ni_vp, 0, p);
	vn->sc_secsize = DEV_BSIZE;
	vn->sc_vp = nd.ni_vp;

	/*
	 * If the size is specified, override the file attributes.  Note that
	 * the vn_size argument is in PAGE_SIZE sized blocks.
	 */
	if (vio->vn_size)
		vn->sc_size = (quad_t)vio->vn_size * PAGE_SIZE / vn->sc_secsize;
	else
		vn->sc_size = vattr.va_size / vn->sc_secsize;
	error = vnsetcred(vn, p->p_ucred);
	if (error) {
		(void) vn_close(nd.ni_vp, flags, p->p_ucred, p);
		return(error);
	}
	vn->sc_flags |= VNF_INITED;
	if (flags == FREAD)
		vn->sc_flags |= VNF_READONLY;
	IFOPT(vn, VN_LABELS) {
		/*
		 * Reopen so that `ds' knows which devices are open.
		 * If this is the first VNIOCSET, then we've
		 * guaranteed that the device is the cdev and that
		 * no other slices or labels are open.  Otherwise,
		 * we rely on VNIOCCLR not being abused.
		 */
		error = vnopen(dev, flag, S_IFCHR, p);
		if (error)
			vnclear(vn);
	}
	IFOPT(vn, VN_FOLLOW)
		printf("vnioctl: SET vp %p size %x blks\n",
		       vn->sc_vp, vn->sc_size);
	return(0);
}

/*
 *	vniocattach_swap:
 *
 *	Attach swap backing store to a VN partition of the size specified
 *	in vn_size.
 */

static int
vniocattach_swap(vn, vio, dev, flag, p)
	struct vn_softc *vn;
	struct vn_ioctl *vio;
	dev_t dev;
	int flag;
	struct proc *p;
{
	int error;

	/*
	 * Range check.  Disallow negative sizes or any size less then the
	 * size of a page.  Then round to a page.
	 */

	if (vio->vn_size <= 0)
		return(EDOM);

	/*
	 * Allocate an OBJT_SWAP object.
	 *
	 * sc_secsize is PAGE_SIZE'd
	 *
	 * vio->vn_size is in PAGE_SIZE'd chunks.
	 * sc_size must be in PAGE_SIZE'd chunks.  
	 * Note the truncation.
	 */

	vn->sc_secsize = PAGE_SIZE;
	vn->sc_size = vio->vn_size;
	vn->sc_object = 
	 vm_pager_allocate(OBJT_SWAP, NULL, vn->sc_secsize * (vm_ooffset_t)vio->vn_size, VM_PROT_DEFAULT, 0);
	IFOPT(vn, VN_RESERVE) {
		if (swap_pager_reserve(vn->sc_object, 0, vn->sc_size) < 0) {
			vm_pager_deallocate(vn->sc_object);
			vn->sc_object = NULL;
			return(EDOM);
		}
	}
	vn->sc_flags |= VNF_INITED;

	error = vnsetcred(vn, p->p_ucred);
	if (error == 0) {
		IFOPT(vn, VN_LABELS) {
			/*
			 * Reopen so that `ds' knows which devices are open.
			 * If this is the first VNIOCSET, then we've
			 * guaranteed that the device is the cdev and that
			 * no other slices or labels are open.  Otherwise,
			 * we rely on VNIOCCLR not being abused.
			 */
			error = vnopen(dev, flag, S_IFCHR, p);
		}
	}
	if (error == 0) {
		IFOPT(vn, VN_FOLLOW) {
			printf("vnioctl: SET vp %p size %x\n",
			       vn->sc_vp, vn->sc_size);
		}
	}
	if (error)
		vnclear(vn);
	return(error);
}

/*
 * Duplicate the current processes' credentials.  Since we are called only
 * as the result of a SET ioctl and only root can do that, any future access
 * to this "disk" is essentially as root.  Note that credentials may change
 * if some other uid can write directly to the mapped file (NFS).
 */
int
vnsetcred(struct vn_softc *vn, struct ucred *cred)
{
	char *tmpbuf;
	int error = 0;

	/*
	 * Set credits in our softc
	 */

	if (vn->sc_cred)
		crfree(vn->sc_cred);
	vn->sc_cred = crdup(cred);

	/*
	 * Horrible kludge to establish credentials for NFS  XXX.
	 */

	if (vn->sc_vp) {
		struct uio auio;
		struct iovec aiov;

		tmpbuf = malloc(vn->sc_secsize, M_TEMP, M_WAITOK);
		bzero(&auio, sizeof(auio));

		aiov.iov_base = tmpbuf;
		aiov.iov_len = vn->sc_secsize;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_resid = aiov.iov_len;
		vn_lock(vn->sc_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
		error = VOP_READ(vn->sc_vp, &auio, 0, vn->sc_cred);
		VOP_UNLOCK(vn->sc_vp, 0, curproc);
		free(tmpbuf, M_TEMP);
	}
	return (error);
}

void
vnclear(struct vn_softc *vn)
{
	struct proc *p = curproc;		/* XXX */

	IFOPT(vn, VN_FOLLOW)
		printf("vnclear(%p): vp=%p\n", vn, vn->sc_vp);
	if (vn->sc_slices != NULL)
		dsgone(&vn->sc_slices);
	vn->sc_flags &= ~VNF_INITED;
	if (vn->sc_vp != NULL) {
		(void)vn_close(vn->sc_vp, vn->sc_flags & VNF_READONLY ?
		    FREAD : (FREAD|FWRITE), vn->sc_cred, p);
		vn->sc_vp = NULL;
	}
	vn->sc_flags &= ~VNF_READONLY;
	if (vn->sc_cred) {
		crfree(vn->sc_cred);
		vn->sc_cred = NULL;
	}
	if (vn->sc_object != NULL) {
		vm_pager_deallocate(vn->sc_object);
		vn->sc_object = NULL;
	}
	vn->sc_size = 0;
}

static	int
vnsize(dev_t dev)
{
	struct vn_softc *vn;

	vn = dev->si_drv1;
	if (!vn)
		return(-1);
	if ((vn->sc_flags & VNF_INITED) == 0)
		return(-1);

	return(vn->sc_size);
}

static int 
vn_modevent(module_t mod, int type, void *data)
{
	struct vn_softc *vn;

	switch (type) {
	case MOD_LOAD:
		cdevsw_add(&vn_cdevsw);
		break;

	case MOD_UNLOAD:
		/* fall through */
	case MOD_SHUTDOWN:
		for (;;) {
			vn = SLIST_FIRST(&vn_list);
			if (!vn)
				break;
			SLIST_REMOVE_HEAD(&vn_list, sc_list);
			if (vn->sc_flags & VNF_INITED)
				vnclear(vn);
			free(vn, M_DEVBUF);
		}
		break;
	default:
		break;
	}
	return 0;
}

DEV_MODULE(vn, vn_modevent, 0);
