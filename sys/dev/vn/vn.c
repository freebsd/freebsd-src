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
 *	@(#)vn.c	8.6 (Berkeley) 4/1/94
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/stat.h>

#include <miscfs/specfs/specdev.h>

#include <sys/vnioctl.h>

#ifdef DEBUG
int dovncluster = 1;
int vndebug = 0x00;
#define VDB_FOLLOW	0x01
#define VDB_INIT	0x02
#define VDB_IO		0x04
#endif

#define	vnunit(dev)	dkunit(dev)

#define	getvnbuf()	\
	((struct buf *)malloc(sizeof(struct buf), M_DEVBUF, M_WAITOK))

#define putvnbuf(bp)	\
	free((caddr_t)(bp), M_DEVBUF)

struct vn_softc {
	int		 sc_flags;	/* flags */
	size_t		 sc_size;	/* size of vn */
	struct vnode	*sc_vp;		/* vnode */
	struct ucred	*sc_cred;	/* credentials */
	int		 sc_maxactive;	/* max # of active requests */
	struct buf	 sc_tab;	/* transfer queue */
	u_long		 sc_options;	/* options */
	struct diskslices *sc_slices;
};

/* sc_flags */
#define VNF_INITED	0x01

struct vn_softc *vn_softc[NVN];
u_long	vn_options;

#define IFOPT(vn,opt) if (((vn)->sc_options|vn_options) & (opt))

/*
 * XXX these decls should be static (without __P(())) or elsewhere.
 */
void	vniodone __P((struct buf *bp));
int	vnsetcred __P((struct vn_softc *vn, struct ucred *cred));
void	vnshutdown __P((void));
void	vnclear __P((struct vn_softc *vn));

int
vnclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct vn_softc *vn = vn_softc[vnunit(dev)];

	IFOPT(vn, VN_LABELS)
		if (vn->sc_slices != NULL)
			dsclose(dev, mode, vn->sc_slices);
	return (0);
}

int
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
				       vnstrategy, (ds_setgeom_t *)NULL));
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
void
vnstrategy(struct buf *bp)
{
	int unit = vnunit(bp->b_dev);
	register struct vn_softc *vn = vn_softc[unit];
	register daddr_t bn;
	int error;
	int sz;
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
		auio.uio_offset = bn*DEV_BSIZE;
		auio.uio_segflg = UIO_SYSSPACE;
		if( bp->b_flags & B_READ)
			auio.uio_rw = UIO_READ;
		else
			auio.uio_rw = UIO_WRITE;
		auio.uio_resid = bp->b_bcount;
		auio.uio_procp = curproc;
		if( bp->b_flags & B_READ)
			error = VOP_READ(vn->sc_vp, &auio, 0, vn->sc_cred);
		else
			error = VOP_WRITE(vn->sc_vp, &auio, 0, vn->sc_cred);

		bp->b_resid = auio.uio_resid;

		if( error )
			bp->b_flags |= B_ERROR;
		biodone(bp);
	} else {
		daddr_t bsize;
		int flags, resid;
		caddr_t addr;

		struct buf *nbp;

		nbp = getvnbuf();

		bn = dbtob(bn);
		bsize = vn->sc_vp->v_mount->mnt_stat.f_iosize;
		addr = bp->b_data;
		flags = bp->b_flags | B_CALL;
		for (resid = bp->b_resid; resid; ) {
			struct vnode *vp;
			daddr_t nbn;
			int off, s, nra;

			nra = 0;
			error = VOP_BMAP(vn->sc_vp, bn / bsize, &vp, &nbn, &nra, NULL);
			if (error == 0 && (long)nbn == -1)
				error = EIO;

			IFOPT(vn, VN_DONTCLUSTER)
				nra = 0;

			off = bn % bsize;
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
				printf("vnstrategy: vp %p/%p bn 0x%lx/0x%lx sz 0x%x\n",
				       vn->sc_vp, vp, bn, nbn, sz);

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
			nbp->b_pfcent = (int) bp;	/* XXX */
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

			bn += sz;
			addr += sz;
			resid -= sz;
		}
		biodone(bp);
		putvnbuf(nbp);
	}
}

void
vniodone( struct buf *bp) {
	bp->b_flags |= B_DONE;
	wakeup((caddr_t) bp);
}

/* ARGSUSED */
int
vnioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct vn_softc *vn = vn_softc[vnunit(dev)];
	struct vn_ioctl *vio;
	struct vattr vattr;
	struct nameidata nd;
	int error;
	u_long *f;


	IFOPT(vn,VN_FOLLOW)
		printf("vnioctl(0x%lx, 0x%lx, %p, 0x%x, %p): unit %d\n",
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

	IFOPT(vn,VN_LABELS) {
		if (vn->sc_slices != NULL) {
			error = dsioctl("vn", dev, cmd, data, flag,
					&vn->sc_slices, vnstrategy,
					(ds_setgeom_t *)NULL);
			if (error != -1)
				return (error);
		}
		if (dkslice(dev) != WHOLE_DISK_SLICE ||
		    dkpart(dev) != RAW_PART)
			return (ENOTTY);
	}

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
			VOP_UNLOCK(nd.ni_vp);
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			return(error);
		}
		VOP_UNLOCK(nd.ni_vp);
		vn->sc_vp = nd.ni_vp;
		vn->sc_size = btodb(vattr.va_size);	/* note truncation */
		error = vnsetcred(vn, p->p_ucred);
		if (error) {
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			return(error);
		}
		vio->vn_size = dbtob(vn->sc_size);
		vn->sc_flags |= VNF_INITED;
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
	error = VOP_READ(vn->sc_vp, &auio, 0, vn->sc_cred);

	free(tmpbuf, M_TEMP);
	return (error);
}

void
vnshutdown()
{
	int i;

	for (i = 0; i < NVN; i++)
		if (vn_softc[i] && vn_softc[i]->sc_flags & VNF_INITED)
			vnclear(vn_softc[i]);
}

TEXT_SET(cleanup_set, vnshutdown);

void
vnclear(struct vn_softc *vn)
{
	register struct vnode *vp = vn->sc_vp;
	struct proc *p = curproc;		/* XXX */

	IFOPT(vn, VN_FOLLOW)
		printf("vnclear(%p): vp=%p\n", vn, vp);
	vn->sc_flags &= ~VNF_INITED;
	if (vp == (struct vnode *)0)
		panic("vnclear: null vp");
	(void) vn_close(vp, FREAD|FWRITE, vn->sc_cred, p);
	crfree(vn->sc_cred);
	vn->sc_vp = (struct vnode *)0;
	vn->sc_cred = (struct ucred *)0;
	vn->sc_size = 0;
	IFOPT(vn, VN_LABELS)
		if (vn->sc_slices != NULL)
			dsgone(&vn->sc_slices);
}

int
vnsize(dev_t dev)
{
	int unit = vnunit(dev);

	if (unit >= NVN || (!vn_softc[unit]) ||
	    (vn_softc[unit]->sc_flags & VNF_INITED) == 0)
		return(-1);
	return(vn_softc[unit]->sc_size);
}

int
vndump(dev_t dev)
{
	return (ENODEV);
}
#endif
