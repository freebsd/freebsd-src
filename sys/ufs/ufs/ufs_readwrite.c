/*-
 * Copyright (c) 1993
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
 *	@(#)ufs_readwrite.c	8.7 (Berkeley) 1/21/94
 * $Id: ufs_readwrite.c,v 1.10 1995/08/25 19:40:32 bde Exp $
 */

#ifdef LFS_READWRITE
#define	BLKSIZE(a, b, c)	blksize(a)
#define	FS			struct lfs
#define	I_FS			i_lfs
#define	READ			lfs_read
#define	READ_S			"lfs_read"
#define	WRITE			lfs_write
#define	WRITE_S			"lfs_write"
#define	fs_bsize		lfs_bsize
#define	fs_maxfilesize		lfs_maxfilesize
#else
#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct fs
#define	I_FS			i_fs
#define	READ			ffs_read
#define	READ_S			"ffs_read"
#define	WRITE			ffs_write
#define	WRITE_S			"ffs_write"
#include <vm/vm.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#endif

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
int
READ(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp;
	register struct inode *ip;
	register struct uio *uio;
	register FS *fs;
	struct buf *bp;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error;
	u_short mode;

	vp = ap->a_vp;
	ip = VTOI(vp);
	mode = ip->i_mode;
	uio = ap->a_uio;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", READ_S);

	if (vp->v_type == VLNK) {
		if ((int)ip->i_size < vp->v_mount->mnt_maxsymlinklen)
			panic("%s: short symlink", READ_S);
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", READ_S, vp->v_type);
#endif
	fs = ip->I_FS;
	if ((u_quad_t)uio->uio_offset > fs->fs_maxfilesize)
		return (EFBIG);

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = BLKSIZE(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

#ifdef LFS_READWRITE
		(void)lfs_check(vp, lbn);
		error = cluster_read(vp, ip->i_size, lbn, size, NOCRED, &bp);
#else
		if (lblktosize(fs, nextlbn) > ip->i_size)
			error = bread(vp, lbn, size, NOCRED, &bp);
		else if (doclusterread)
			error = cluster_read(vp,
			    ip->i_size, lbn, size, NOCRED, &bp);
		else if (lbn - 1 == vp->v_lastr) {
			int nextsize = BLKSIZE(fs, ip, nextlbn);
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else
			error = bread(vp, lbn, size, NOCRED, &bp);
#endif
		if (error)
			break;
		vp->v_lastr = lbn;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= bp->b_resid;
		if (size < xfersize) {
			if (size == 0)
				break;
			xfersize = size;
		}
		if (uio->uio_segflg != UIO_NOCOPY)
			ip->i_flag |= IN_RECURSE;
		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
		if (uio->uio_segflg != UIO_NOCOPY)
			ip->i_flag &= ~IN_RECURSE;
		if (error)
			break;

		brelse(bp);
	}
	if (bp != NULL)
		brelse(bp);
	ip->i_flag |= IN_ACCESS;
	return (error);
}

/*
 * Vnode op for writing.
 */
int
WRITE(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp;
	register struct uio *uio;
	register struct inode *ip;
	register FS *fs;
	struct buf *bp;
	struct proc *p;
	daddr_t lbn;
	off_t osize;
	int blkoffset, error, flags, ioflag, resid, size, xfersize;
	struct timeval tv;

	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", WRITE_S);
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_size;
		if ((ip->i_flags & APPEND) && uio->uio_offset != ip->i_size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("%s: nonsync dir write", WRITE_S);
		break;
	default:
		panic("%s: type", WRITE_S);
	}

	fs = ip->I_FS;
	if (uio->uio_offset < 0 ||
	    (u_quad_t)uio->uio_offset + uio->uio_resid > fs->fs_maxfilesize)
		return (EFBIG);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	p = uio->uio_procp;
	if (vp->v_type == VREG && p &&
	    uio->uio_offset + uio->uio_resid >
	    p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}

	resid = uio->uio_resid;
	osize = ip->i_size;
	flags = ioflag & IO_SYNC ? B_SYNC : 0;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;

		if (uio->uio_offset + xfersize > ip->i_size)
			vnode_pager_setsize(vp, (u_long)uio->uio_offset + xfersize);

#ifdef LFS_READWRITE
		(void)lfs_check(vp, lbn);
		error = lfs_balloc(vp, xfersize, lbn, &bp);
#else
		if (fs->fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;

		error = ffs_balloc(ip,
		    lbn, blkoffset + xfersize, ap->a_cred, &bp, flags);
#endif
		if (error)
			break;

		if (uio->uio_offset + xfersize > ip->i_size) {
			ip->i_size = uio->uio_offset + xfersize;
		}

		size = BLKSIZE(fs, ip, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		if (uio->uio_segflg != UIO_NOCOPY)
			ip->i_flag |= IN_RECURSE;
		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
		if (uio->uio_segflg != UIO_NOCOPY)
			ip->i_flag &= ~IN_RECURSE;
#ifdef LFS_READWRITE
		(void)VOP_BWRITE(bp);
#else
		if (ioflag & IO_VMIO)
			bp->b_flags |= B_RELBUF;

		if (ioflag & IO_SYNC) {
			(void)bwrite(bp);
		} else if (xfersize + blkoffset == fs->fs_bsize) {
			if (doclusterwrite) {
				bp->b_flags |= B_CLUSTEROK;
				cluster_write(bp, ip->i_size);
			} else {
				bawrite(bp);
			}
		} else {
			bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
#endif
		if (error || xfersize == 0)
			break;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if (resid > uio->uio_resid && ap->a_cred && ap->a_cred->cr_uid != 0)
		ip->i_mode &= ~(ISUID | ISGID);
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)VOP_TRUNCATE(vp, osize,
			    ioflag & IO_SYNC, ap->a_cred, uio->uio_procp);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC)) {
		tv = time;
		error = VOP_UPDATE(vp, &tv, &tv, 1);
	}
	return (error);
}

#ifndef LFS_READWRITE

static void ffs_getpages_iodone(struct buf *bp) {
	bp->b_flags |= B_DONE;
	wakeup(bp);
}

/*
 * get page routine
 */
int
ffs_getpages(ap)
	struct vop_getpages_args *ap;
{
	vm_offset_t kva, foff;
	int i, size, bsize;
	struct vnode *dp;
	struct buf *bp;
	int s;
	int error = 0;
	int contigbackwards, contigforwards;
	int pcontigbackwards, pcontigforwards;
	int firstcontigpage;
	int reqlblkno, reqblkno;
	int poff;

	/*
	 * if ANY DEV_BSIZE blocks are valid on a large filesystem block
	 * then, the entire page is valid --
	 */
	if (ap->a_m[ap->a_reqpage]->valid) {
		ap->a_m[ap->a_reqpage]->valid = VM_PAGE_BITS_ALL;
		for (i = 0; i < ap->a_count; i++) {
			if (i != ap->a_reqpage)
				vnode_pager_freepage(ap->a_m[i]);
		}
		return VM_PAGER_OK;
	}

	bsize = ap->a_vp->v_mount->mnt_stat.f_iosize;
	foff = ap->a_m[ap->a_reqpage]->offset;
	reqlblkno = foff / bsize;
	poff = (foff - reqlblkno * bsize) / PAGE_SIZE;

	if ( VOP_BMAP( ap->a_vp, reqlblkno, &dp, &reqblkno, &contigforwards,
		&contigbackwards) || (reqblkno == -1)) {
		for(i = 0; i < ap->a_count; i++) {
			if (i != ap->a_reqpage)
				vnode_pager_freepage(ap->a_m[i]);
		}
		if (reqblkno == -1) {
			if ((ap->a_m[ap->a_reqpage]->flags & PG_ZERO) == 0)
				vm_page_zero_fill(ap->a_m[ap->a_reqpage]);
			return VM_PAGER_OK;
		} else {
			return VM_PAGER_ERROR;
		}
	}

	reqblkno += (poff * PAGE_SIZE) / DEV_BSIZE;

	firstcontigpage = 0;
	pcontigbackwards = 0;
	if (ap->a_reqpage > 0) {
		pcontigbackwards = poff + ((contigbackwards * bsize) / PAGE_SIZE);
		if (pcontigbackwards < ap->a_reqpage) {
			firstcontigpage = ap->a_reqpage - pcontigbackwards;
			for(i = 0; i < firstcontigpage; i++)
				vnode_pager_freepage(ap->a_m[i]);
		}
	}

	pcontigforwards = ((bsize / PAGE_SIZE) - (poff + 1)) +
		(contigforwards * bsize) / PAGE_SIZE;
	if (pcontigforwards < (ap->a_count - (ap->a_reqpage + 1))) {
		for( i = ap->a_reqpage + pcontigforwards + 1; i < ap->a_count; i++)
			vnode_pager_freepage(ap->a_m[i]);
		ap->a_count = ap->a_reqpage + pcontigforwards + 1;
	}

	if (firstcontigpage != 0) {
		for (i = firstcontigpage; i < ap->a_count; i++) {
			ap->a_m[i - firstcontigpage] = ap->a_m[i];
		}
		ap->a_count -= firstcontigpage;
		ap->a_reqpage -= firstcontigpage;
	}

	/*
	 * calculate the size of the transfer
	 */
	foff = ap->a_m[0]->offset;
	reqblkno -= (ap->a_m[ap->a_reqpage]->offset - foff) / DEV_BSIZE;
	size = ap->a_count * PAGE_SIZE;
	if ((foff + size) >
		((vm_object_t) ap->a_vp->v_object)->un_pager.vnp.vnp_size)
		size = ((vm_object_t) ap->a_vp->v_object)->un_pager.vnp.vnp_size - foff;

	/*
	 * round up physical size for real devices
	 */
	if (dp->v_type == VBLK || dp->v_type == VCHR)
		size = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);

	bp = getpbuf();
	kva = (vm_offset_t) bp->b_data;

	/*
	 * and map the pages to be read into the kva
	 */
	pmap_qenter(kva, ap->a_m, ap->a_count);

	/* build a minimal buffer header */
	bp->b_flags = B_BUSY | B_READ | B_CALL;
	bp->b_iodone = ffs_getpages_iodone;
	/* B_PHYS is not set, but it is nice to fill this in */
	bp->b_proc = curproc;
	bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
	if (bp->b_rcred != NOCRED)
		crhold(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crhold(bp->b_wcred);
	bp->b_blkno = reqblkno;
	pbgetvp(dp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += ap->a_count;

	/* do the input */
	VOP_STRATEGY(bp);

	s = splbio();
	/* we definitely need to be at splbio here */

	while ((bp->b_flags & B_DONE) == 0) {
		tsleep(bp, PVM, "vnread", 0);
	}
	splx(s);
	if ((bp->b_flags & B_ERROR) != 0)
		error = EIO;

	if (!error) {
		if (size != ap->a_count * PAGE_SIZE)
			bzero((caddr_t) kva + size, PAGE_SIZE * ap->a_count - size);
	}
	pmap_qremove(kva, ap->a_count);

	/*
	 * free the buffer header back to the swap buffer pool
	 */
	relpbuf(bp);

	for (i = 0; i < ap->a_count; i++) {
		pmap_clear_modify(VM_PAGE_TO_PHYS(ap->a_m[i]));
		ap->a_m[i]->dirty = 0;
		ap->a_m[i]->valid = VM_PAGE_BITS_ALL;
		if (i != ap->a_reqpage) {

			/*
			 * whether or not to leave the page activated is up in
			 * the air, but we should put the page on a page queue
			 * somewhere. (it already is in the object). Result:
			 * It appears that emperical results show that
			 * deactivating pages is best.
			 */

			/*
			 * just in case someone was asking for this page we
			 * now tell them that it is ok to use
			 */
			if (!error) {
				vm_page_deactivate(ap->a_m[i]);
				PAGE_WAKEUP(ap->a_m[i]);
			} else {
				vnode_pager_freepage(ap->a_m[i]);
			}
		}
	}
	if (error) {
		printf("ffs_getpages: I/O read error\n");
	}
	return (error ? VM_PAGER_ERROR : VM_PAGER_OK);
}
#endif
