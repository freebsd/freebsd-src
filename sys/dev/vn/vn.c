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
 *	$Id: vn.c,v 1.55 1998/02/20 13:27:36 bde Exp $
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
#include "vn.h"
#if NVN > 0

/* default is to have 8 VN's */
#if NVN < 8
#undef NVN
#define	NVN	8
#endif

#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/stat.h>
#include <sys/conf.h>
#ifdef SLICE
#include <sys/device.h>
#include <dev/slice/slice.h>
#endif	/* SLICE */

#include <miscfs/specfs/specdev.h>

#include <sys/vnioctl.h>

static	d_ioctl_t	vnioctl;
#ifndef	SLICE
static	d_open_t	vnopen;
static	d_close_t	vnclose;
static	d_dump_t	vndump;
static	d_psize_t	vnsize;
static	d_strategy_t	vnstrategy;

#define CDEV_MAJOR 43
#define BDEV_MAJOR 15
static struct cdevsw vn_cdevsw;
static struct bdevsw vn_bdevsw = 
	{ vnopen, vnclose, vnstrategy,                      vnioctl,	/*15*/
	  vndump, vnsize,  D_DISK | D_NOCLUSTERRW, "vn",    &vn_cdevsw, -1 };

#else /* SLICE */

static sl_h_IO_req_t	nvsIOreq;	/* IO req downward (to device) */
static sl_h_ioctl_t	nvsioctl;	/* ioctl req downward (to device) */
static sl_h_open_t	nvsopen;	/* downwards travelling open */
static sl_h_close_t	nvsclose;	/* downwards travelling close */

static struct slice_handler slicetype = {
	"vn",
	0,
	NULL,
	0,
	NULL,	/* constructor */
	&nvsIOreq,
	&nvsioctl,
	&nvsopen,
	&nvsclose,
	NULL,	/* revoke */
	NULL,	/* claim */
	NULL,	/* verify */
	NULL	/* upconfig */
};
#endif

#define	vnunit(dev)	dkunit(dev)

#define	getvnbuf()	\
	((struct buf *)malloc(sizeof(struct buf), M_DEVBUF, M_WAITOK))

#define putvnbuf(bp)	\
	free((caddr_t)(bp), M_DEVBUF)

struct vn_softc {
	int		 sc_flags;	/* flags */
	size_t		 sc_size;	/* size of vn */
#ifdef SLICE
	struct slice	*slice;
	struct slicelimits limit;
	int		 mynor;
	int		 unit;
#else
	struct diskslices *sc_slices;
#endif	/* SLICE */
	struct vnode	*sc_vp;		/* vnode */
	struct ucred	*sc_cred;	/* credentials */
	int		 sc_maxactive;	/* max # of active requests */
	struct buf	 sc_tab;	/* transfer queue */
	u_long		 sc_options;	/* options */
};

/* sc_flags */
#define VNF_INITED	0x01

static struct vn_softc *vn_softc[NVN];
static u_long	vn_options;

#define IFOPT(vn,opt) if (((vn)->sc_options|vn_options) & (opt))

static void	vniodone (struct buf *bp);
static int	vnsetcred (struct vn_softc *vn, struct ucred *cred);
static void	vnshutdown (int, void *);
static void	vnclear (struct vn_softc *vn);

#ifndef	SLICE
static	int
vnclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct vn_softc *vn = vn_softc[vnunit(dev)];

	IFOPT(vn, VN_LABELS)
		if (vn->sc_slices != NULL)
			dsclose(dev, mode, vn->sc_slices);
	return (0);
}

static	int
vnopen(dev_t dev, int flags, int mode, struct proc *p)
{
	int unit = vnunit(dev);
	struct vn_softc *vn;

	if (unit >= NVN) {
		if (vn_options & VN_FOLLOW)
			printf("vnopen(0x%lx, 0x%x, 0x%x, %p)\n",
				dev, flags, mode, p);
		return(ENOENT);
	}

	vn = vn_softc[unit];
	if (!vn) {
		vn = malloc(sizeof *vn, M_DEVBUF, M_WAITOK);
		if (!vn)
			return (ENOMEM);
		bzero(vn, sizeof *vn);
		vn_softc[unit] = vn;
	}

	IFOPT(vn, VN_FOLLOW)
		printf("vnopen(0x%lx, 0x%x, 0x%x, %p)\n", dev, flags, mode, p);

	IFOPT(vn, VN_LABELS) {
		if (vn->sc_flags & VNF_INITED) {
			struct disklabel label;

			/* Build label for whole disk. */
			bzero(&label, sizeof label);
			label.d_secsize = DEV_BSIZE;
			label.d_nsectors = 32;
			label.d_ntracks = 64;
			label.d_ncylinders = vn->sc_size / (32 * 64);
			label.d_secpercyl = 32 * 64;
			label.d_secperunit =
					label.d_partitions[RAW_PART].p_size =
					vn->sc_size;

			return (dsopen("vn", dev, mode, &vn->sc_slices, &label,
				       vnstrategy, (ds_setgeom_t *)NULL,
				       &vn_bdevsw, &vn_cdevsw));
		}
		if (dkslice(dev) != WHOLE_DISK_SLICE ||
		    dkpart(dev) != RAW_PART ||
		    mode != S_IFCHR)
			return (ENXIO);
	}
	return(0);
}

/*
 * this code does I/O calls through the appropriate VOP entry point...
 * unless a swap_pager I/O request is being done.  This strategy (-))
 * allows for coherency with mmap except in the case of paging.  This
 * is necessary, because the VOP calls use lots of memory (and actually
 * are not extremely efficient -- but we want to keep semantics correct),
 * and the pageout daemon gets really unhappy (and so does the rest of the
 * system) when it runs out of memory.
 */
static	void
vnstrategy(struct buf *bp)
{
	int unit = vnunit(bp->b_dev);
	register struct vn_softc *vn = vn_softc[unit];
	register daddr_t bn;
	int error;
	int isvplocked = 0;
	long sz;
	struct uio auio;
	struct iovec aiov;

	IFOPT(vn, VN_DEBUG)
		printf("vnstrategy(%p): unit %d\n", bp, unit);

	if ((vn->sc_flags & VNF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	IFOPT(vn, VN_LABELS) {
		bp->b_resid = bp->b_bcount;/* XXX best place to set this? */
		if (vn->sc_slices != NULL && dscheck(bp, vn->sc_slices) <= 0) {
			biodone(bp);
			return;
		}
		bn = bp->b_pblkno;
		bp->b_resid = bp->b_bcount;/* XXX best place to set this? */
	} else {
		bn = bp->b_blkno;
		sz = howmany(bp->b_bcount, DEV_BSIZE);
		bp->b_resid = bp->b_bcount;
		if (bn < 0 || bn + sz > vn->sc_size) {
			if (bn != vn->sc_size) {
				bp->b_error = EINVAL;
				bp->b_flags |= B_ERROR;
			}
			biodone(bp);
			return;
		}
	}

	if( (bp->b_flags & B_PAGING) == 0) {
		aiov.iov_base = bp->b_data;
		aiov.iov_len = bp->b_bcount;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = dbtob(bn);
		auio.uio_segflg = UIO_SYSSPACE;
		if( bp->b_flags & B_READ)
			auio.uio_rw = UIO_READ;
		else
			auio.uio_rw = UIO_WRITE;
		auio.uio_resid = bp->b_bcount;
		auio.uio_procp = curproc;
		if (!VOP_ISLOCKED(vn->sc_vp)) {
			isvplocked = 1;
			vn_lock(vn->sc_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
		}
		if( bp->b_flags & B_READ)
			error = VOP_READ(vn->sc_vp, &auio, 0, vn->sc_cred);
		else
			error = VOP_WRITE(vn->sc_vp, &auio, 0, vn->sc_cred);
		if (isvplocked) {
			VOP_UNLOCK(vn->sc_vp, 0, curproc);
			isvplocked = 0;
		}
		bp->b_resid = auio.uio_resid;

		if( error )
			bp->b_flags |= B_ERROR;
		biodone(bp);
	} else {
		long bsize, resid;
		off_t byten;
		int flags;
		caddr_t addr;
		struct buf *nbp;

		nbp = getvnbuf();
		byten = dbtob(bn);
		bsize = vn->sc_vp->v_mount->mnt_stat.f_iosize;
		addr = bp->b_data;
		flags = bp->b_flags | B_CALL;
		for (resid = bp->b_resid; resid; ) {
			struct vnode *vp;
			daddr_t nbn;
			int off, s, nra;

			nra = 0;
			if (!VOP_ISLOCKED(vn->sc_vp)) {
				isvplocked = 1;
				vn_lock(vn->sc_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
			}
			error = VOP_BMAP(vn->sc_vp, (daddr_t)(byten / bsize),
					 &vp, &nbn, &nra, NULL);
			if (isvplocked) {
				VOP_UNLOCK(vn->sc_vp, 0, curproc);
				isvplocked = 0;
			}
			if (error == 0 && nbn == -1)
				error = EIO;

			IFOPT(vn, VN_DONTCLUSTER)
				nra = 0;

			off = byten % bsize;
			if (off)
				sz = bsize - off;
			else
				sz = (1 + nra) * bsize;
			if (resid < sz)
				sz = resid;

			if (error) {
				bp->b_resid -= (resid - sz);
				bp->b_flags |= B_ERROR;
				biodone(bp);
				putvnbuf(nbp);
				return;
			}

			IFOPT(vn,VN_IO)
				printf(
			/* XXX no %qx in kernel.  Synthesize it. */
			"vnstrategy: vp %p/%p bn 0x%lx%08lx/0x%lx sz 0x%x\n",
				       vn->sc_vp, vp, (long)(byten >> 32),
				       (u_long)byten, nbn, sz);

			nbp->b_flags = flags;
			nbp->b_bcount = sz;
			nbp->b_bufsize = sz;
			nbp->b_error = 0;
			if (vp->v_type == VBLK || vp->v_type == VCHR)
				nbp->b_dev = vp->v_rdev;
			else
				nbp->b_dev = NODEV;
			nbp->b_data = addr;
			nbp->b_blkno = nbn + btodb(off);
			nbp->b_proc = bp->b_proc;
			nbp->b_iodone = vniodone;
			nbp->b_vp = vp;
			nbp->b_rcred = vn->sc_cred;	/* XXX crdup? */
			nbp->b_wcred = vn->sc_cred;	/* XXX crdup? */
			nbp->b_dirtyoff = bp->b_dirtyoff;
			nbp->b_dirtyend = bp->b_dirtyend;
			nbp->b_validoff = bp->b_validoff;
			nbp->b_validend = bp->b_validend;

			if ((nbp->b_flags & B_READ) == 0)
				nbp->b_vp->v_numoutput++;

			VOP_STRATEGY(nbp);

			s = splbio();
			while ((nbp->b_flags & B_DONE) == 0) {
				nbp->b_flags |= B_WANTED;
				tsleep(nbp, PRIBIO, "vnwait", 0);
			}
			splx(s);

			if( nbp->b_flags & B_ERROR) {
				bp->b_flags |= B_ERROR;
				bp->b_resid -= (resid - sz);
				biodone(bp);
				putvnbuf(nbp);
				return;
			}

			byten += sz;
			addr += sz;
			resid -= sz;
		}
		biodone(bp);
		putvnbuf(nbp);
	}
}

#else /* SLICE */
static void 
nvsIOreq(void *private ,struct buf *bp)
{
	struct vn_softc *vn = private;
	u_int32_t unit = vn->unit;
	register daddr_t bn;
	int error;
	int isvplocked = 0;
	long sz;
	struct uio auio;
	struct iovec aiov;

	IFOPT(vn, VN_DEBUG)
		printf("vnstrategy(%p): unit %d\n", bp, unit);

	if ((vn->sc_flags & VNF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	bn = bp->b_pblkno;
	bp->b_resid = bp->b_bcount;/* XXX best place to set this? */
	sz = howmany(bp->b_bcount, DEV_BSIZE);

	if( (bp->b_flags & B_PAGING) == 0) {
		aiov.iov_base = bp->b_data;
		aiov.iov_len = bp->b_bcount;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = dbtob(bn);
		auio.uio_segflg = UIO_SYSSPACE;
		if( bp->b_flags & B_READ)
			auio.uio_rw = UIO_READ;
		else
			auio.uio_rw = UIO_WRITE;
		auio.uio_resid = bp->b_bcount;
		auio.uio_procp = curproc;
		if (!VOP_ISLOCKED(vn->sc_vp)) {
			isvplocked = 1;
			vn_lock(vn->sc_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
		}
		if( bp->b_flags & B_READ)
			error = VOP_READ(vn->sc_vp, &auio, 0, vn->sc_cred);
		else
			error = VOP_WRITE(vn->sc_vp, &auio, 0, vn->sc_cred);
		if (isvplocked) {
			VOP_UNLOCK(vn->sc_vp, 0, curproc);
			isvplocked = 0;
		}
		bp->b_resid = auio.uio_resid;

		if( error )
			bp->b_flags |= B_ERROR;
		biodone(bp);
	} else {
		long bsize, resid;
		off_t byten;
		int flags;
		caddr_t addr;
		struct buf *nbp;

		nbp = getvnbuf();
		byten = dbtob(bn);
		/* This is probably the only time this is RIGHT */
		bsize = vn->sc_vp->v_mount->mnt_stat.f_iosize;
		addr = bp->b_data;
		flags = bp->b_flags | B_CALL;
		for (resid = bp->b_resid; resid; ) {
			struct vnode *vp;
			daddr_t nbn;
			int off, s, nra;

			nra = 0;
			if (!VOP_ISLOCKED(vn->sc_vp)) {
				isvplocked = 1;
				vn_lock(vn->sc_vp, LK_EXCLUSIVE | LK_RETRY, curproc);
			}
			error = VOP_BMAP(vn->sc_vp, (daddr_t)(byten / bsize),
					 &vp, &nbn, &nra, NULL);
			if (isvplocked) {
				VOP_UNLOCK(vn->sc_vp, 0, curproc);
				isvplocked = 0;
			}
			if (error == 0 && nbn == -1)
				error = EIO;

			IFOPT(vn, VN_DONTCLUSTER)
				nra = 0;

			off = byten % bsize;
			if (off)
				sz = bsize - off;
			else
				sz = (1 + nra) * bsize;
			if (resid < sz)
				sz = resid;

			if (error) {
				bp->b_resid -= (resid - sz);
				bp->b_flags |= B_ERROR;
				biodone(bp);
				putvnbuf(nbp);
				return;
			}

			IFOPT(vn,VN_IO)
				printf(
			/* XXX no %qx in kernel.  Synthesize it. */
			"vnstrategy: vp %p/%p bn 0x%lx%08lx/0x%lx sz 0x%x\n",
				       vn->sc_vp, vp, (long)(byten >> 32),
				       (u_long)byten, nbn, sz);

			nbp->b_flags = flags;
			nbp->b_bcount = sz;
			nbp->b_bufsize = sz;
			nbp->b_error = 0;
			if (vp->v_type == VBLK || vp->v_type == VCHR)
				nbp->b_dev = vp->v_rdev;
			else
				nbp->b_dev = NODEV;
			nbp->b_data = addr;
			nbp->b_blkno = nbn + btodb(off);
			nbp->b_proc = bp->b_proc;
			nbp->b_iodone = vniodone;
			nbp->b_vp = vp;
			nbp->b_rcred = vn->sc_cred;	/* XXX crdup? */
			nbp->b_wcred = vn->sc_cred;	/* XXX crdup? */
			nbp->b_dirtyoff = bp->b_dirtyoff;
			nbp->b_dirtyend = bp->b_dirtyend;
			nbp->b_validoff = bp->b_validoff;
			nbp->b_validend = bp->b_validend;

			if ((nbp->b_flags & B_READ) == 0)
				nbp->b_vp->v_numoutput++;

			VOP_STRATEGY(nbp);

			s = splbio();
			while ((nbp->b_flags & B_DONE) == 0) {
				nbp->b_flags |= B_WANTED;
				tsleep(nbp, PRIBIO, "vnwait", 0);
			}
			splx(s);

			if( nbp->b_flags & B_ERROR) {
				bp->b_flags |= B_ERROR;
				bp->b_resid -= (resid - sz);
				biodone(bp);
				putvnbuf(nbp);
				return;
			}

			byten += sz;
			addr += sz;
			resid -= sz;
		}
		biodone(bp);
		putvnbuf(nbp);
	}
}
#endif	/* SLICE */

void
vniodone( struct buf *bp) {
	bp->b_flags |= B_DONE;
	wakeup((caddr_t) bp);
}

/* ARGSUSED */
static	int
vnioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct vn_softc *vn = vn_softc[vnunit(dev)];
	struct vn_ioctl *vio;
	struct vattr vattr;
	struct nameidata nd;
	int error;
	u_long *f;
#ifdef	SLICE
	sh_p	tp;
#endif


	IFOPT(vn,VN_FOLLOW)
		printf("vnioctl(0x%lx, 0x%x, %p, 0x%x, %p): unit %d\n",
		       dev, cmd, data, flag, p, vnunit(dev));

	switch (cmd) {
	case VNIOCATTACH:
	case VNIOCDETACH:
	case VNIOCGSET:
	case VNIOCGCLEAR:
	case VNIOCUSET:
	case VNIOCUCLEAR:
		goto vn_specific;
	}
#ifndef SLICE

	IFOPT(vn,VN_LABELS) {
		if (vn->sc_slices != NULL) {
			error = dsioctl("vn", dev, cmd, data, flag,
					&vn->sc_slices, vnstrategy,
					(ds_setgeom_t *)NULL);
			if (error != ENOIOCTL)
				return (error);
		}
		if (dkslice(dev) != WHOLE_DISK_SLICE ||
		    dkpart(dev) != RAW_PART)
			return (ENOTTY);
	}

#endif
    vn_specific:

	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);

	vio = (struct vn_ioctl *)data;
	f = (u_long*)data;
	switch (cmd) {

	case VNIOCATTACH:
		if (vn->sc_flags & VNF_INITED)
			return(EBUSY);
		/*
		 * Always open for read and write.
		 * This is probably bogus, but it lets vn_open()
		 * weed out directories, sockets, etc. so we don't
		 * have to worry about them.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vn_file, p);
		error = vn_open(&nd, FREAD|FWRITE, 0);
		if (error)
			return(error);
		error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p);
		if (error) {
			VOP_UNLOCK(nd.ni_vp, 0, p);
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			return(error);
		}
		VOP_UNLOCK(nd.ni_vp, 0, p);
		vn->sc_vp = nd.ni_vp;
		vn->sc_size = btodb(vattr.va_size);	/* note truncation */
		error = vnsetcred(vn, p->p_ucred);
		if (error) {
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			return(error);
		}
		vio->vn_size = dbtob(vn->sc_size);
		vn->sc_flags |= VNF_INITED;
#ifdef	SLICE
/*
 * XXX The filesystem blocksize will say 1024
 * for a 8K filesystem. don't know yet how to deal with this,
 * so lie for now.. say 512.
 */
#if 0
		vn->limit.blksize = vn->sc_vp->v_mount->mnt_stat.f_bsize;
#else
		vn->limit.blksize = DEV_BSIZE;
#endif
		vn->slice->limits.blksize = vn->limit.blksize;
		vn->limit.slicesize = vattr.va_size;
		vn->slice->limits.slicesize = vattr.va_size;
		/* 
		 * We have a media to read/write.
		 * Try identify it.
		 */
		if ((tp = slice_probeall(vn->slice)) != NULL) {
			(*tp->constructor)(vn->slice);
		}
#else
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
#endif
		IFOPT(vn, VN_FOLLOW)
			printf("vnioctl: SET vp %p size %x\n",
			       vn->sc_vp, vn->sc_size);
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
		 * hardware devices?
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
		return (ENOTTY);
	}
	return(0);
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
	struct uio auio;
	struct iovec aiov;
	char *tmpbuf;
	int error;

	vn->sc_cred = crdup(cred);
	tmpbuf = malloc(DEV_BSIZE, M_TEMP, M_WAITOK);

	/* XXX: Horrible kludge to establish credentials for NFS */
	aiov.iov_base = tmpbuf;
	aiov.iov_len = min(DEV_BSIZE, dbtob(vn->sc_size));
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
	return (error);
}

void
vnshutdown(int howto, void *ignored)
{
	int i;

	for (i = 0; i < NVN; i++)
		if (vn_softc[i] && vn_softc[i]->sc_flags & VNF_INITED)
			vnclear(vn_softc[i]);
}

void
vnclear(struct vn_softc *vn)
{
	register struct vnode *vp = vn->sc_vp;
	struct proc *p = curproc;		/* XXX */

	IFOPT(vn, VN_FOLLOW)
		printf("vnclear(%p): vp=%p\n", vn, vp);
#ifdef	SLICE
	if (vn->slice->handler_up) {
		(*(vn->slice->handler_up->revoke)) (vn->slice->private_up);
	}
#else	/* SLICE */
	if (vn->sc_slices != NULL)
		dsgone(&vn->sc_slices);
#endif
	vn->sc_flags &= ~VNF_INITED;
	if (vp == (struct vnode *)0)
		panic("vnclear: null vp");
	(void) vn_close(vp, FREAD|FWRITE, vn->sc_cred, p);
	crfree(vn->sc_cred);
	vn->sc_vp = (struct vnode *)0;
	vn->sc_cred = (struct ucred *)0;
	vn->sc_size = 0;
}

#ifndef	SLICE
static	int
vnsize(dev_t dev)
{
	int unit = vnunit(dev);

	if (unit >= NVN || (!vn_softc[unit]) ||
	    (vn_softc[unit]->sc_flags & VNF_INITED) == 0)
		return(-1);
	return(vn_softc[unit]->sc_size);
}

static	int
vndump(dev_t dev)
{
	return (ENODEV);
}

static vn_devsw_installed = 0;
#endif	/* !SLICE */

static void 
vn_drvinit(void *unused)
{
#ifndef SLICE
	if( ! vn_devsw_installed ) {
		if (at_shutdown(&vnshutdown, NULL, SHUTDOWN_POST_SYNC)) {
			printf("vn: could not install shutdown hook\n");
			return;
		}
		bdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &vn_bdevsw);
		vn_devsw_installed = 1;
	}
#else /* SLICE */
	int mynor;
	int unit;
	struct vn_softc *vn;
	char namebuf[64];
	if (at_shutdown(&vnshutdown, NULL, SHUTDOWN_POST_SYNC)) {
		printf("vn: could not install shutdown hook\n");
		return;
	}
	for (unit = 0; unit < NVN; unit++) {
		vn = malloc(sizeof *vn, M_DEVBUF, M_NOWAIT);
		if (!vn)
			return;
		bzero(vn, sizeof *vn);
		vn_softc[unit] = vn;
		vn->unit = unit;
		sprintf(namebuf,"vn%d",vn->unit);
			vn->mynor = dkmakeminor(unit, WHOLE_DISK_SLICE,
				RAW_PART);
		vn->limit.blksize = DEV_BSIZE;
		vn->limit.slicesize = ((u_int64_t)vn->sc_size * DEV_BSIZE);
		sl_make_slice(&slicetype,
				vn,
				&vn->limit,
 				&vn->slice,
				NULL,
				namebuf);
		/* Allow full probing */
		vn->slice->probeinfo.typespecific = NULL;
		vn->slice->probeinfo.type = NULL;
	}
#define CDEV_MAJOR 20 /* not really needed */
#endif	/* SLICE */
}

SYSINIT(vndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,vn_drvinit,NULL)

#ifdef SLICE

static int
nvsopen(void *private, int flags, int mode, struct proc *p)
{
	struct vn_softc *vn;

	vn = private;

	IFOPT(vn, VN_FOLLOW)
		printf("vnopen(0x%lx, 0x%x, 0x%x, %p)\n", 
			makedev(0,vn->mynor) , flags, mode, p);
	return (0);
}

static void
nvsclose(void *private, int flags, int mode, struct proc *p)
{
	struct vn_softc *vn;

	vn = private;
	IFOPT(vn, VN_FOLLOW)
		printf("vnclose(0x%lx, 0x%x, 0x%x, %p)\n", 
			makedev(0,vn->mynor) , flags, mode, p);
	return;
}

static int
nvsioctl( void *private, int cmd, caddr_t addr, int flag, struct proc *p)
{
	struct vn_softc *vn;

	vn = private;

	return(vnioctl(makedev(0,vn->mynor), cmd, addr, flag, p));
}

#endif
#endif
