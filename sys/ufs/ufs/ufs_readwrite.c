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
 * $FreeBSD$
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
#include <sys/event.h>
#include <sys/vmmeter.h>

#define VN_KNOTE(vp, b) \
	KNOTE((struct klist *)&vp->v_pollinfo.vpi_selinfo.si_note, (b))

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
	int error, orig_resid;
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

	orig_resid = uio->uio_resid;
	if (orig_resid <= 0)
		return (0);

	object = vp->v_object;

	bytesinfile = ip->i_size - uio->uio_offset;
	if (bytesinfile <= 0) {
		if ((vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
			ip->i_flag |= IN_ACCESS;
		return 0;
	}

	if (object)
		vm_object_reference(object);

#ifdef ENABLE_VFS_IOOPT
	/*
	 * If IO optimisation is turned on,
	 * and we are NOT a VM based IO request, 
	 * (i.e. not headed for the buffer cache)
	 * but there IS a vm object associated with it.
	 */
	if ((ioflag & IO_VMIO) == 0 && (vfs_ioopt > 1) && object) {
		int nread, toread;

		toread = uio->uio_resid;
		if (toread > bytesinfile)
			toread = bytesinfile;
		if (toread >= PAGE_SIZE) {
			/*
			 * Then if it's at least a page in size, try 
			 * get the data from the object using vm tricks
			 */
			error = uioread(toread, uio, object, &nread);
			if ((uio->uio_resid == 0) || (error != 0)) {
				/*
				 * If we finished or there was an error
				 * then finish up (the reference previously
				 * obtained on object must be released).
				 */
				if ((error == 0 ||
				    uio->uio_resid != orig_resid) &&
				    (vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
					ip->i_flag |= IN_ACCESS;

				if (object)
					vm_object_vndeallocate(object);
				return error;
			}
		}
	}
#endif

	/*
	 * Ok so we couldn't do it all in one vm trick...
	 * so cycle around trying smaller bites..
	 */
	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;
#ifdef ENABLE_VFS_IOOPT
		if ((ioflag & IO_VMIO) == 0 && (vfs_ioopt > 1) && object) {
			/*
			 * Obviously we didn't finish above, but we
			 * didn't get an error either. Try the same trick again.
			 * but this time we are looping.
			 */
			int nread, toread;
			toread = uio->uio_resid;
			if (toread > bytesinfile)
				toread = bytesinfile;

			/*
			 * Once again, if there isn't enough for a
			 * whole page, don't try optimising.
			 */
			if (toread >= PAGE_SIZE) {
				error = uioread(toread, uio, object, &nread);
				if ((uio->uio_resid == 0) || (error != 0)) {
					/*
					 * If we finished or there was an 
					 * error then finish up (the reference
					 * previously obtained on object must 
					 * be released).
					 */
					if ((error == 0 ||
					    uio->uio_resid != orig_resid) &&
					    (vp->v_mount->mnt_flag &
					    MNT_NOATIME) == 0)
						ip->i_flag |= IN_ACCESS;
					if (object)
						vm_object_vndeallocate(object);
					return error;
				}
				/*
				 * To get here we didnt't finish or err.
				 * If we did get some data,
				 * loop to try another bite.
				 */
				if (nread > 0) {
					continue;
				}
			}
		}
#endif

		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;

		/*
		 * size of buffer.  The buffer representing the
		 * end of the file is rounded up to the size of
		 * the block type ( fragment or full block, 
		 * depending ).
		 */
		size = BLKSIZE(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);
		
		/*
		 * The amount we want to transfer in this iteration is
		 * one FS block less the amount of the data before
		 * our startpoint (duh!)
		 */
		xfersize = fs->fs_bsize - blkoffset;

		/*
		 * But if we actually want less than the block,
		 * or the file doesn't have a whole block more of data,
		 * then use the lesser number.
		 */
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= ip->i_size)
			/*
			 * Don't do readahead if this is the end of the file.
			 */
			error = bread(vp, lbn, size, NOCRED, &bp);
		else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0)
			/* 
			 * Otherwise if we are allowed to cluster,
			 * grab as much as we can.
			 *
			 * XXX  This may not be a win if we are not
			 * doing sequential access.
			 */
			error = cluster_read(vp, ip->i_size, lbn,
				size, NOCRED, uio->uio_resid, seqcount, &bp);
		else if (seqcount > 1) {
			/*
			 * If we are NOT allowed to cluster, then
			 * if we appear to be acting sequentially,
			 * fire off a request for a readahead
			 * as well as a read. Note that the 4th and 5th
			 * arguments point to arrays of the size specified in
			 * the 6th argument.
			 */
			int nextsize = BLKSIZE(fs, ip, nextlbn);
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else
			/*
			 * Failing all of the above, just read what the 
			 * user asked for. Interestingly, the same as
			 * the first option above.
			 */
			error = bread(vp, lbn, size, NOCRED, &bp);
		if (error) {
			brelse(bp);
			bp = NULL;
			break;
		}

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

#ifdef ENABLE_VFS_IOOPT
		if (vfs_ioopt && object &&
		    (bp->b_flags & B_VMIO) &&
		    ((blkoffset & PAGE_MASK) == 0) &&
		    ((xfersize & PAGE_MASK) == 0)) {
			/*
			 * If VFS IO  optimisation is turned on,
			 * and it's an exact page multiple
			 * And a normal VM based op,
			 * then use uiomiveco()
			 */
			error =
				uiomoveco((char *)bp->b_data + blkoffset,
					(int)xfersize, uio, object);
		} else 
#endif
		{
			/*
			 * otherwise use the general form
			 */
			error =
				uiomove((char *)bp->b_data + blkoffset,
					(int)xfersize, uio);
		}

		if (error)
			break;

		if ((ioflag & IO_VMIO) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
			/*
			 * If there are no dependencies, and
			 * it's VMIO, then we don't need the buf,
			 * mark it available for freeing. The VM has the data.
			 */
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			/*
			 * Otherwise let whoever
			 * made the request take care of
			 * freeing it. We just queue
			 * it onto another list.
			 */
			bqrelse(bp);
		}
	}

	/* 
	 * This can only happen in the case of an error
	 * because the loop above resets bp to NULL on each iteration
	 * and on normal completion has not set a new value into it.
	 * so it must have come from a 'break' statement
	 */
	if (bp != NULL) {
		if ((ioflag & IO_VMIO) &&
		   (LIST_FIRST(&bp->b_dep) == NULL)) {
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			bqrelse(bp);
		}
	}

	if (object)
		vm_object_vndeallocate(object);
	if ((error == 0 || uio->uio_resid != orig_resid) &&
	    (vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
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
	int seqcount;
	int blkoffset, error, extended, flags, ioflag, resid, size, xfersize;
	vm_object_t object;

	extended = 0;
	seqcount = ap->a_ioflag >> 16;
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
		panic("%s: dir write", WRITE_S);
		break;
	default:
		panic("%s: type %p %d (%d,%d)", WRITE_S, vp, (int)vp->v_type,
			(int)uio->uio_offset,
			(int)uio->uio_resid
		);
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
		PROC_LOCK(p);
		psignal(p, SIGXFSZ);
		PROC_UNLOCK(p);
		if (object)
			vm_object_vndeallocate(object);
		return (EFBIG);
	}

	resid = uio->uio_resid;
	osize = ip->i_size;
	flags = 0;
	if ((ioflag & IO_SYNC) && !DOINGASYNC(vp))
		flags = B_SYNC;

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

                /*      
                 * Avoid a data-consistency race between write() and mmap()
                 * by ensuring that newly allocated blocks are zerod.  The
                 * race can occur even in the case where the write covers
                 * the entire block.
                 */
		flags |= B_CLRBUF;
#if 0
		if (fs->fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;
#endif
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
		if ((ioflag & IO_VMIO) &&
		   (LIST_FIRST(&bp->b_dep) == NULL))
			bp->b_flags |= B_RELBUF;

		if (ioflag & IO_SYNC) {
			(void)bwrite(bp);
		} else if (vm_page_count_severe() ||
			    buf_dirty_count_severe() ||
			    (ioflag & IO_ASYNC)) {
			bp->b_flags |= B_CLUSTEROK;
			bawrite(bp);
		} else if (xfersize + blkoffset == fs->fs_bsize) {
			if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERW) == 0) {
				bp->b_flags |= B_CLUSTEROK;
				cluster_write(bp, ip->i_size, seqcount);
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
	if (resid > uio->uio_resid && ap->a_cred && 
	    suser_xxx(ap->a_cred, NULL, PRISON_ROOT))
		ip->i_mode &= ~(ISUID | ISGID);
	if (resid > uio->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)UFS_TRUNCATE(vp, osize,
			    ioflag & IO_SYNC, ap->a_cred, uio->uio_procp);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC))
		error = UFS_UPDATE(vp, 1);

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
	vm_page_t mreq;
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
	 * if ANY DEV_BSIZE blocks are valid on a large filesystem block,
	 * then the entire page is valid.  Since the page may be mapped,
	 * user programs might reference data beyond the actual end of file
	 * occuring within the page.  We have to zero that data.
	 */
	if (mreq->valid) {
		if (mreq->valid != VM_PAGE_BITS_ALL)
			vm_page_zero_invalid(mreq, TRUE);
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

	if (bsize < PAGE_SIZE)
		return vnode_pager_generic_getpages(ap->a_vp, ap->a_m,
						    ap->a_count,
						    ap->a_reqpage);

	/*
	 * foff is the file offset of the required page
	 * reqlblkno is the logical block that contains the page
	 * poff is the index of the page into the logical block
	 */
	reqlblkno = foff / bsize;
	poff = (foff % bsize) / PAGE_SIZE;

	dp = VTOI(vp)->i_devvp;
	if (ufs_bmaparray(vp, reqlblkno, &reqblkno, &bforwards, &bbackwards)
	    || (reqblkno == -1)) {
		for(i = 0; i < pcount; i++) {
			if (i != ap->a_reqpage)
				vm_page_free(ap->a_m[i]);
		}
		if (reqblkno == -1) {
			if ((mreq->flags & PG_ZERO) == 0)
				vm_page_zero_fill(mreq);
			vm_page_undirty(mreq);
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
