#define DEBUG 1
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>

#include <miscfs/specfs/specdev.h>

#include <sys/vnioctl.h>

#ifdef DEBUG
int dovncluster = 1;
int vndebug = 0x00;
#define VDB_FOLLOW	0x01
#define VDB_INIT	0x02
#define VDB_IO		0x04
#endif

#define	vnunit(x)	(minor(x) >> 3)

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
};

/* sc_flags */
#define VNF_INITED	0x01

struct vn_softc **vn_softc;
int numvnd;

void	vnattach __P((int num));
int	vnopen __P((dev_t dev, int flags, int mode, struct proc *p));
void	vnstrategy __P((struct buf *bp));
void	vnstart __P((struct vn_softc *vn));
void	vniodone __P((struct buf *bp));
int	vnread __P((dev_t dev, struct uio *uio, int flags, struct proc *p));
int	vnwrite __P((dev_t dev, struct uio *uio, int flags, struct proc *p));
int	vnioctl __P((dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p));
int	vnsetcred __P((struct vn_softc *vn, struct ucred *cred));
void	vnthrottle __P((struct vn_softc *vn, struct vnode *vp));
void	vnshutdown __P((void));
void	vnclear __P((struct vn_softc *vn));
size_t	vnsize __P((dev_t dev));
int	vndump __P((dev_t dev));

int vnclose() {return 0;}

int
vnopen(dev_t dev, int flags, int mode, struct proc *p)
{
	int unit = vnunit(dev),size;
	struct vn_softc **vscp, **old;

#ifdef DEBUG
	if (vndebug & VDB_FOLLOW)
		printf("vnopen(%x, %x, %x, %x)\n", dev, flags, mode, p);
#endif
	if (unit >= numvnd) {
		/*
		 * We need to get more space for our config.  If you say
		 * this is overkill, you're absolutely right.
		 */
		size = (unit+1) * (sizeof *vn_softc);
		vscp = (struct vn_softc **) malloc(size, M_DEVBUF, M_WAITOK);
		if (!vscp)
			return(ENOMEM);
	        bzero(vscp,size);
		if (numvnd)
			bcopy(vn_softc, vscp, numvnd * (sizeof *vn_softc));
		numvnd = unit + 1;
		old = vn_softc;
		vn_softc = vscp;
		if (old)
			free(old, M_DEVBUF);
	}
	if (!vn_softc[unit]) {
		vn_softc[unit] = (struct vn_softc *) 
			malloc (sizeof **vn_softc, M_DEVBUF, M_WAITOK);
		if (!vn_softc[unit])
			return(ENOMEM);
		bzero(vn_softc[unit], sizeof **vn_softc);
	}
	return(0);
}

/*
 * Break the request into bsize pieces and submit using VOP_BMAP/VOP_STRATEGY.
 * Note that this driver can only be used for swapping over NFS on the hp
 * since nfs_strategy on the vax cannot handle u-areas and page tables.
 */
void
vnstrategy(struct buf *bp)
{
	int unit = vnunit(bp->b_dev);
	register struct vn_softc *vn = vn_softc[unit];
	register struct buf *nbp;
	register int bn, bsize, resid;
	register caddr_t addr;
	int sz, flags, error;

#ifdef DEBUG
	if (vndebug & VDB_FOLLOW)
		printf("vnstrategy(%x): unit %d\n", bp, unit);
#endif
	if ((vn->sc_flags & VNF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
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
	bn = dbtob(bn);
	bsize = vn->sc_vp->v_mount->mnt_stat.f_iosize;
	addr = bp->b_data;
	flags = bp->b_flags | B_CALL;
	for (resid = bp->b_resid; resid; resid -= sz) {
		struct vnode *vp;
		daddr_t nbn;
		int off, s, nra;

		nra = 0;
		error = VOP_BMAP(vn->sc_vp, bn / bsize, &vp, &nbn, &nra);
		if (error == 0 && (long)nbn == -1)
			error = EIO;
#ifdef DEBUG
		if (!dovncluster)
			nra = 0;
#endif

		if (off = bn % bsize)
			sz = bsize - off;
		else
			sz = (1 + nra) * bsize;
		if (resid < sz)
			sz = resid;
#ifdef DEBUG
		if (vndebug & VDB_IO)
			printf("vnstrategy: vp %x/%x bn %x/%x sz %x\n",
			       vn->sc_vp, vp, bn, nbn, sz);
#endif

		nbp = getvnbuf();
		nbp->b_flags = flags;
		nbp->b_bcount = sz;
		nbp->b_bufsize = bp->b_bufsize;
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
		/*
		 * If there was an error or a hole in the file...punt.
		 * Note that we deal with this after the nbp allocation.
		 * This ensures that we properly clean up any operations
		 * that we have already fired off.
		 *
		 * XXX we could deal with holes here but it would be
		 * a hassle (in the write case).
		 */
		if (error) {
			nbp->b_error = error;
			nbp->b_flags |= B_ERROR;
			bp->b_resid -= (resid - sz);
			biodone(nbp);
			return;
		}
		/*
		 * Just sort by block number
		 */
		nbp->b_resid = nbp->b_blkno;
		s = splbio();
		disksort(&vn->sc_tab, nbp);
		if (vn->sc_tab.b_active < vn->sc_maxactive) {
			vn->sc_tab.b_active++;
			vnstart(vn);
		}
		splx(s);
		bn += sz;
		addr += sz;
	}
}

/*
 * Feed requests sequentially.
 * We do it this way to keep from flooding NFS servers if we are connected
 * to an NFS file.  This places the burden on the client rather than the
 * server.
 */
void
vnstart(struct vn_softc *vn)
{
	register struct buf *bp;

	/*
	 * Dequeue now since lower level strategy routine might
	 * queue using same links
	 */
	bp = vn->sc_tab.b_actf;
	vn->sc_tab.b_actf = bp->b_actf;
#ifdef DEBUG
	if (vndebug & VDB_IO)
		printf("vnstart(%d): bp %x vp %x blkno %x addr %x cnt %x\n",
		       0, bp, bp->b_vp, bp->b_blkno, bp->b_data,
		       bp->b_bcount);
#endif
	if ((bp->b_flags & B_READ) == 0)
		bp->b_vp->v_numoutput++;
	VOP_STRATEGY(bp);
}

void
vniodone(struct buf *bp)
{
	register struct buf *pbp = (struct buf *)bp->b_pfcent;	/* XXX */
	register struct vn_softc *vn = vn_softc[vnunit(pbp->b_dev)];
	int s;

	s = splbio();
#ifdef DEBUG
	if (vndebug & VDB_IO)
		printf("vniodone(%d): bp %x vp %x blkno %x addr %x cnt %x\n",
		       0, bp, bp->b_vp, bp->b_blkno, bp->b_data,
		       bp->b_bcount);
#endif
	if (bp->b_error) {
#ifdef DEBUG
		if (vndebug & VDB_IO)
			printf("vniodone: bp %x error %d\n", bp, bp->b_error);
#endif
		pbp->b_flags |= B_ERROR;
		pbp->b_error = biowait(bp);
	}
	pbp->b_resid -= bp->b_bcount;
	putvnbuf(bp);
	if (pbp->b_resid == 0) {
#ifdef DEBUG
		if (vndebug & VDB_IO)
			printf("vniodone: pbp %x iodone\n", pbp);
#endif
		biodone(pbp);
	}
	if (vn->sc_tab.b_actf)
		vnstart(vn);
	else
		vn->sc_tab.b_active--;
	splx(s);
}

int
vnread(dev_t dev, struct uio *uio, int flags, struct proc *p)
{

#ifdef DEBUG
	if (vndebug & VDB_FOLLOW)
		printf("vnread(%x, %x, %x, %x)\n", dev, uio, flags, p);
#endif
	return(physio(vnstrategy, NULL, dev, B_READ, minphys, uio));
}

int
vnwrite(dev_t dev, struct uio *uio, int flags, struct proc *p)
{

#ifdef DEBUG
	if (vndebug & VDB_FOLLOW)
		printf("vnwrite(%x, %x, %x, %x)\n", dev, uio, flags, p);
#endif
	return(physio(vnstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/* ARGSUSED */
int
vnioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int unit = vnunit(dev);
	register struct vn_softc *vn;
	struct vn_ioctl *vio;
	struct vattr vattr;
	struct nameidata nd;
	int error;

#ifdef DEBUG
	if (vndebug & VDB_FOLLOW)
		printf("vnioctl(%x, %x, %x, %x, %x): unit %d\n",
		       dev, cmd, data, flag, p, unit);
#endif
	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	if (unit >= numvnd)
		return (ENXIO);

	vn = vn_softc[unit];
	vio = (struct vn_ioctl *)data;
	switch (cmd) {

	case VNIOCSET:
		if (vn->sc_flags & VNF_INITED)
			return(EBUSY);
		/*
		 * Always open for read and write.
		 * This is probably bogus, but it lets vn_open()
		 * weed out directories, sockets, etc. so we don't
		 * have to worry about them.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vn_file, p);
		if (error = vn_open(&nd, FREAD|FWRITE, 0))
			return(error);
		if (error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p)) {
			VOP_UNLOCK(nd.ni_vp);
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			return(error);
		}
		VOP_UNLOCK(nd.ni_vp);
		vn->sc_vp = nd.ni_vp;
		vn->sc_size = btodb(vattr.va_size);	/* note truncation */
		if (error = vnsetcred(vn, p->p_ucred)) {
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			return(error);
		}
		vnthrottle(vn, vn->sc_vp);
		vio->vn_size = dbtob(vn->sc_size);
		vn->sc_flags |= VNF_INITED;
#ifdef DEBUG
		if (vndebug & VDB_INIT)
			printf("vnioctl: SET vp %x size %x\n",
			       vn->sc_vp, vn->sc_size);
#endif
		break;

	case VNIOCCLR:
		if ((vn->sc_flags & VNF_INITED) == 0)
			return(ENXIO);
		vnclear(vn);
#ifdef DEBUG
		if (vndebug & VDB_INIT)
			printf("vnioctl: CLRed\n");
#endif
		break;

	default:
		return(ENXIO);
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

/*
 * Set maxactive based on FS type
 */
void
vnthrottle(struct vn_softc *vn, struct vnode *vp)
{
#if 0
	extern int (**nfsv2_vnodeop_p)();

	if (vp->v_op == nfsv2_vnodeop_p)
		vn->sc_maxactive = 2;
	else
#endif
		vn->sc_maxactive = 8;

	if (vn->sc_maxactive < 1)
		vn->sc_maxactive = 1;
}

void
vnshutdown()
{
	int i;

	for (i = 0; i < numvnd; i++)
		if (vn_softc[i] && vn_softc[i]->sc_flags & VNF_INITED)
			vnclear(vn_softc[i]);
}
TEXT_SET(cleanup_set,vnshutdown);

void
vnclear(struct vn_softc *vn)
{
	register struct vnode *vp = vn->sc_vp;
	struct proc *p = curproc;		/* XXX */

#ifdef DEBUG
	if (vndebug & VDB_FOLLOW)
		printf("vnclear(%x): vp %x\n", vp);
#endif
	vn->sc_flags &= ~VNF_INITED;
	if (vp == (struct vnode *)0)
		panic("vnioctl: null vp");
	(void) vn_close(vp, FREAD|FWRITE, vn->sc_cred, p);
	crfree(vn->sc_cred);
	vn->sc_vp = (struct vnode *)0;
	vn->sc_cred = (struct ucred *)0;
	vn->sc_size = 0;
}

size_t
vnsize(dev_t dev)
{
	int unit = vnunit(dev);
	register struct vn_softc *vn = vn_softc[unit];

	if (unit >= numvnd || (vn->sc_flags & VNF_INITED) == 0)
		return(-1);
	return(vn->sc_size);
}

int
vndump(dev_t dev)
{
	return(ENXIO);
}
#endif
