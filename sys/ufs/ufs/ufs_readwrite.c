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
 *	@(#)ufs_readwrite.c	8.11 (Berkeley) 5/8/95
 * $Id: ufs_readwrite.c,v 1.47 1998/03/30 09:56:31 phk Exp $
 */

#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct fs
#define	I_FS			i_fs
#define	READ			ffs_read
#define	READ_S			"ffs_read"
#define	WRITE			ffs_write
#define	WRITE_S			"ffs_write"

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <vm/vnode_pager.h>
#include <sys/poll.h>
#include <sys/sysctl.h>

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
	ufs_daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error;
	u_short mode;
	int seqcount;
	int ioflag;
	vm_object_t object;

	vp = ap->a_vp;
	seqcount = ap->a_ioflag >> 16;
	ip = VTOI(vp);
	mode = ip->i_mode;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;

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
	if ((u_int64_t)uio->uio_offset > fs->fs_maxfilesize)
		return (EFBIG);

	object = vp->v_object;

	bytesinfile = ip->i_size - uio->uio_offset;
	if (bytesinfile <= 0) {
		return 0;
	}

	if (object)
		vm_object_reference(object);
#if 1
	if ((ioflag & IO_VMIO) == 0 && (vfs_ioopt > 1) && object) {
		int nread, toread;
		toread = uio->uio_resid;
		if (toread > bytesinfile)
			toread = bytesinfile;
		if (toread >= PAGE_SIZE) {
			error = uioread(toread, uio, object, &nread);
			if ((uio->uio_resid == 0) || (error != 0)) {
				if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
					ip->i_flag |= IN_ACCESS;
				if (object)
					vm_object_vndeallocate(object);
				return error;
			}
		}
	}
#endif

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;
#if 1
		if ((ioflag & IO_VMIO) == 0 && (vfs_ioopt > 1) && object) {
			int nread, toread;
			toread = uio->uio_resid;
			if (toread > bytesinfile)
				toread = bytesinfile;
			if (toread >= PAGE_SIZE) {
				error = uioread(toread, uio, object, &nread);
				if ((uio->uio_resid == 0) || (error != 0)) {
					if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
						ip->i_flag |= IN_ACCESS;
					if (object)
						vm_object_vndeallocate(object);
					return error;
				}
				if (nread > 0) {
					continue;
				}
			}
		}
#endif

		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = BLKSIZE(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);

		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= ip->i_size)
			error = bread(vp, lbn, size, NOCRED, &bp);
		else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0)
			error = cluster_read(vp, ip->i_size, lbn,
				size, NOCRED, uio->uio_resid, seqcount, &bp);
		else if (lbn - 1 == vp->v_lastr) {
			int nextsize = BLKSIZE(fs, ip, nextlbn);
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else
			error = bread(vp, lbn, size, NOCRED, &bp);
		if (error) {
			brelse(bp);
			bp = NULL;
			break;
		}
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

		if (vfs_ioopt && object &&
				(bp->b_flags & B_VMIO) &&
				((blkoffset & PAGE_MASK) == 0) &&
				((xfersize & PAGE_MASK) == 0)) {
			error =
				uiomoveco((char *)bp->b_data + blkoffset,
					(int)xfersize, uio, object);
		} else {
			error =
				uiomove((char *)bp->b_data + blkoffset,
					(int)xfersize, uio);
		}

		if (error)
			break;

		if (ioflag & IO_VMIO) {
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			bqrelse(bp);
		}
	}

	if (bp != NULL) {
		if (ioflag & IO_VMIO) {
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			bqrelse(bp);
		}
	}

	if (object)
		vm_object_vndeallocate(object);
	if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
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
	ufs_daddr_t lbn;
	off_t osize;
	int blkoffset, error, extended, flags, ioflag, resid, size, xfersize;
	struct timeval tv;
	vm_object_t object;

	extended = 0;
	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);

	object = vp->v_object;
	if (object)
		vm_object_reference(object);

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", WRITE_S);
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_size;
		if ((ip->i_flags & APPEND) && uio->uio_offset != ip->i_size) {
			if (object)
				vm_object_vndeallocate(object);
			return (EPERM);
		}
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
	    (u_int64_t)uio->uio_offset + uio->uio_resid > fs->fs_maxfilesize) {
		if (object)
			vm_object_vndeallocate(object);
		return (EFBIG);
	}
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	p = uio->uio_procp;
	if (vp->v_type == VREG && p &&
	    uio->uio_offset + uio->uio_resid >
	    p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		if (object)
			vm_object_vndeallocate(object);
		return (EFBIG);
	}

	resid = uio->uio_resid;
	osize = ip->i_size;
	flags = ioflag & IO_SYNC ? B_SYNC : 0;

	if (object && (object->flags & OBJ_OPT)) {
		vm_freeze_copyopts(object,
			OFF_TO_IDX(uio->uio_offset),
			OFF_TO_IDX(uio->uio_offset + uio->uio_resid + PAGE_MASK));
	}

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;

		if (uio->uio_offset + xfersize > ip->i_size)
			vnode_pager_setsize(vp, uio->uio_offset + xfersize);

		if (fs->fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;
/* XXX is uio->uio_offset the right thing here? */
		error = VOP_BALLOC(vp, uio->uio_offset, xfersize,
		    ap->a_cred, flags, &bp);
		if (error != 0)
			break;

		if (uio->uio_offset + xfersize > ip->i_size) {
			ip->i_size = uio->uio_offset + xfersize;
			extended = 1;
		}

		size = BLKSIZE(fs, ip, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
		if (ioflag & IO_VMIO)
			bp->b_flags |= B_RELBUF;

		if (ioflag & IO_SYNC) {
			(void)bwrite(bp);
		} else if (xfersize + blkoffset == fs->fs_bsize) {
			if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERW) == 0) {
				bp->b_flags |= B_CLUSTEROK;
				cluster_write(bp, ip->i_size);
			} else {
				bawrite(bp);
			}
		} else {
			bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
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
			(void)UFS_TRUNCATE(vp, osize,
			    ioflag & IO_SYNC, ap->a_cred, uio->uio_procp);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC)) {
		getmicrotime(&tv);
		error = UFS_UPDATE(vp, &tv, &tv, 1);
	}
	if (!error)
		VN_POLLEVENT(vp, POLLWRITE | (extended ? POLLEXTEND : 0));

	if (object)
		vm_object_vndeallocate(object);

	return (error);
}


/*
 * get page routine
 */
int
ffs_getpages(ap)
	struct vop_getpages_args *ap;
{
	off_t foff, physoffset;
	int i, size, bsize;
	struct vnode *dp, *vp;
	vm_object_t obj;
	vm_pindex_t pindex, firstindex;
	vm_page_t m, mreq;
	int bbackwards, bforwards;
	int pbackwards, pforwards;
	int firstpage;
	int reqlblkno;
	daddr_t reqblkno;
	int poff;
	int pcount;
	int rtval;
	int pagesperblock;


	pcount = round_page(ap->a_count) / PAGE_SIZE;
	mreq = ap->a_m[ap->a_reqpage];
	firstindex = ap->a_m[0]->pindex;

	/*
	 * if ANY DEV_BSIZE blocks are valid on a large filesystem block
	 * then, the entire page is valid --
	 */
	if (mreq->valid) {
		mreq->valid = VM_PAGE_BITS_ALL;
		for (i = 0; i < pcount; i++) {
			if (i != ap->a_reqpage) {
				vm_page_free(ap->a_m[i]);
			}
		}
		return VM_PAGER_OK;
	}

	vp = ap->a_vp;
	obj = vp->v_object;
	bsize = vp->v_mount->mnt_stat.f_iosize;
	pindex = mreq->pindex;
	foff = IDX_TO_OFF(pindex) /* + ap->a_offset should be zero */;

	if (firstindex == 0)
		vp->v_lastr = 0;

	if ((obj->behavior != OBJ_RANDOM) &&
		((firstindex != 0) && (firstindex <= vp->v_lastr) &&
		 ((firstindex + pcount) > vp->v_lastr)) ||
		(obj->behavior == OBJ_SEQUENTIAL)) {
		struct uio auio;
		struct iovec aiov;
		int error;

		for (i = 0; i < pcount; i++) {
			m = ap->a_m[i];
			vm_page_activate(m);
			m->busy++;
			m->flags &= ~PG_BUSY;
		}

		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		aiov.iov_base = 0;
		aiov.iov_len = MAXBSIZE;
		auio.uio_resid = MAXBSIZE;
		auio.uio_offset = foff;
		auio.uio_segflg = UIO_NOCOPY;
		auio.uio_rw = UIO_READ;
		auio.uio_procp = curproc;
		error = VOP_READ(vp, &auio,
			IO_VMIO | ((MAXBSIZE / bsize) << 16), curproc->p_ucred);

		for (i = 0; i < pcount; i++) {
			m = ap->a_m[i];
			m->busy--;

			if ((m != mreq) && (m->wire_count == 0) && (m->hold_count == 0) &&
				(m->valid == 0) && (m->busy == 0) &&
				(m->flags & PG_BUSY) == 0) {
				m->flags |= PG_BUSY;
				vm_page_free(m);
			} else if (m == mreq) {
				while (m->flags & PG_BUSY) {
					vm_page_sleep(m, "ffspwt", NULL);
				}
				m->flags |= PG_BUSY;
				vp->v_lastr = m->pindex + 1;
			} else {
				if (m->wire_count == 0) {
					if (m->busy || (m->flags & PG_MAPPED) ||
						(m->flags & (PG_WANTED | PG_BUSY)) == PG_WANTED) {
						vm_page_activate(m);
					} else {
						vm_page_deactivate(m);
					}
				}
				vp->v_lastr = m->pindex + 1;
			}
		}

		if (mreq->valid == 0) 
			return VM_PAGER_ERROR;

		mreq->valid = VM_PAGE_BITS_ALL;
		return VM_PAGER_OK;
	}

	/*
	 * foff is the file offset of the required page
	 * reqlblkno is the logical block that contains the page
	 * poff is the index of the page into the logical block
	 */
	reqlblkno = foff / bsize;
	poff = (foff % bsize) / PAGE_SIZE;

	if ( VOP_BMAP( vp, reqlblkno, &dp, &reqblkno,
		&bforwards, &bbackwards) || (reqblkno == -1)) {
		for(i = 0; i < pcount; i++) {
			if (i != ap->a_reqpage)
				vm_page_free(ap->a_m[i]);
		}
		if (reqblkno == -1) {
			if ((mreq->flags & PG_ZERO) == 0)
				vm_page_zero_fill(mreq);
			mreq->dirty = 0;
			mreq->valid = VM_PAGE_BITS_ALL;
			return VM_PAGER_OK;
		} else {
			return VM_PAGER_ERROR;
		}
	}

	physoffset = (off_t)reqblkno * DEV_BSIZE + poff * PAGE_SIZE;
	pagesperblock = bsize / PAGE_SIZE;
	/*
	 * find the first page that is contiguous...
	 * note that pbackwards is the number of pages that are contiguous
	 * backwards.
	 */
	firstpage = 0;
	if (ap->a_count) {
		pbackwards = poff + bbackwards * pagesperblock;
		if (ap->a_reqpage > pbackwards) {
			firstpage = ap->a_reqpage - pbackwards;
			for(i=0;i<firstpage;i++)
				vm_page_free(ap->a_m[i]);
		}

	/*
	 * pforwards is the number of pages that are contiguous
	 * after the current page.
	 */
		pforwards = (pagesperblock - (poff + 1)) +
			bforwards * pagesperblock;
		if (pforwards < (pcount - (ap->a_reqpage + 1))) {
			for( i = ap->a_reqpage + pforwards + 1; i < pcount; i++)
				vm_page_free(ap->a_m[i]);
			pcount = ap->a_reqpage + pforwards + 1;
		}

	/*
	 * number of pages for I/O corrected for the non-contig pages at
	 * the beginning of the array.
	 */
		pcount -= firstpage;
	}

	/*
	 * calculate the size of the transfer
	 */

	size = pcount * PAGE_SIZE;
	vp->v_lastr = mreq->pindex + pcount;

	if ((IDX_TO_OFF(ap->a_m[firstpage]->pindex) + size) >
		obj->un_pager.vnp.vnp_size)
		size = obj->un_pager.vnp.vnp_size -
			IDX_TO_OFF(ap->a_m[firstpage]->pindex);

	physoffset -= foff;
	rtval = VOP_GETPAGES(dp, &ap->a_m[firstpage], size,
		(ap->a_reqpage - firstpage), physoffset);

	return (rtval);
}

/*
 * put page routine
 *
 * XXX By default, wimp out... note that a_offset is ignored (and always
 * XXX has been).
 */
int
ffs_putpages(ap)
	struct vop_putpages_args *ap;
{
	return vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_sync, ap->a_rtvals);
}
