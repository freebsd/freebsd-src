/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_bio.c	8.9 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfsnode.h>

/*
 * Just call nfs_writebp() with the force argument set to 1.
 *
 * NOTE: B_DONE may or may not be set in a_bp on call.
 */
static int
nfs_bwrite(struct buf *bp)
{

	return (nfs_writebp(bp, 1, curthread));
}

struct buf_ops buf_ops_nfs = {
	"buf_ops_nfs",
	nfs_bwrite
};

static struct buf *nfs_getcacheblk(struct vnode *vp, daddr_t bn, int size,
		    struct thread *td);

/*
 * Vnode op for VM getpages.
 */
int
nfs_getpages(struct vop_getpages_args *ap)
{
	int i, error, nextoff, size, toff, count, npages;
	struct uio uio;
	struct iovec iov;
	vm_offset_t kva;
	struct buf *bp;
	struct vnode *vp;
	struct thread *td;
	struct ucred *cred;
	struct nfsmount *nmp;
	vm_page_t *pages;

	GIANT_REQUIRED;

	vp = ap->a_vp;
	td = curthread;				/* XXX */
	cred = curthread->td_ucred;		/* XXX */
	nmp = VFSTONFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;

	if (vp->v_object == NULL) {
		printf("nfs_getpages: called with non-merged cache vnode??\n");
		return VM_PAGER_ERROR;
	}

	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {
		(void)nfs_fsinfo(nmp, vp, cred, td);
	}

	npages = btoc(count);

	/*
	 * If the requested page is partially valid, just return it and
	 * allow the pager to zero-out the blanks.  Partially valid pages
	 * can only occur at the file EOF.
	 */

	{
		vm_page_t m = pages[ap->a_reqpage];

		if (m->valid != 0) {
			/* handled by vm_fault now	  */
			/* vm_page_zero_invalid(m, TRUE); */
			for (i = 0; i < npages; ++i) {
				if (i != ap->a_reqpage)
					vm_page_free(pages[i]);
			}
			return(0);
		}
	}

	/*
	 * We use only the kva address for the buffer, but this is extremely
	 * convienient and fast.
	 */
	bp = getpbuf(&nfs_pbuf_freecnt);

	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);
	cnt.v_vnodein++;
	cnt.v_vnodepgsin += npages;

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;

	error = nfs_readrpc(vp, &uio, cred);
	pmap_qremove(kva, npages);

	relpbuf(bp, &nfs_pbuf_freecnt);

	if (error && (uio.uio_resid == count)) {
		printf("nfs_getpages: error %d\n", error);
		for (i = 0; i < npages; ++i) {
			if (i != ap->a_reqpage)
				vm_page_free(pages[i]);
		}
		return VM_PAGER_ERROR;
	}

	/*
	 * Calculate the number of bytes read and validate only that number
	 * of bytes.  Note that due to pending writes, size may be 0.  This
	 * does not mean that the remaining data is invalid!
	 */

	size = count - uio.uio_resid;

	for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
		vm_page_t m;
		nextoff = toff + PAGE_SIZE;
		m = pages[i];

		m->flags &= ~PG_ZERO;

		if (nextoff <= size) {
			/*
			 * Read operation filled an entire page
			 */
			m->valid = VM_PAGE_BITS_ALL;
			vm_page_undirty(m);
		} else if (size > toff) {
			/*
			 * Read operation filled a partial page.
			 */
			m->valid = 0;
			vm_page_set_validclean(m, 0, size - toff);
			/* handled by vm_fault now	  */
			/* vm_page_zero_invalid(m, TRUE); */
		} else {
			/*
			 * Read operation was short.  If no error occured
			 * we may have hit a zero-fill section.   We simply
			 * leave valid set to 0.
			 */
			;
		}
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
				if (m->flags & PG_WANTED)
					vm_page_activate(m);
				else
					vm_page_deactivate(m);
				vm_page_wakeup(m);
			} else {
				vm_page_free(m);
			}
		}
	}
	return 0;
}

/*
 * Vnode op for VM putpages.
 */
int
nfs_putpages(struct vop_putpages_args *ap)
{
	struct uio uio;
	struct iovec iov;
	vm_offset_t kva;
	struct buf *bp;
	int iomode, must_commit, i, error, npages, count;
	off_t offset;
	int *rtvals;
	struct vnode *vp;
	struct thread *td;
	struct ucred *cred;
	struct nfsmount *nmp;
	struct nfsnode *np;
	vm_page_t *pages;

	GIANT_REQUIRED;

	vp = ap->a_vp;
	np = VTONFS(vp);
	td = curthread;				/* XXX */
	cred = curthread->td_ucred;		/* XXX */
	nmp = VFSTONFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;
	rtvals = ap->a_rtvals;
	npages = btoc(count);
	offset = IDX_TO_OFF(pages[0]->pindex);

	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {
		(void)nfs_fsinfo(nmp, vp, cred, td);
	}

	for (i = 0; i < npages; i++)
		rtvals[i] = VM_PAGER_AGAIN;

	/*
	 * When putting pages, do not extend file past EOF.
	 */

	if (offset + count > np->n_size) {
		count = np->n_size - offset;
		if (count < 0)
			count = 0;
	}

	/*
	 * We use only the kva address for the buffer, but this is extremely
	 * convienient and fast.
	 */
	bp = getpbuf(&nfs_pbuf_freecnt);

	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);
	cnt.v_vnodeout++;
	cnt.v_vnodepgsout += count;

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;

	if ((ap->a_sync & VM_PAGER_PUT_SYNC) == 0)
	    iomode = NFSV3WRITE_UNSTABLE;
	else
	    iomode = NFSV3WRITE_FILESYNC;

	error = nfs_writerpc(vp, &uio, cred, &iomode, &must_commit);

	pmap_qremove(kva, npages);
	relpbuf(bp, &nfs_pbuf_freecnt);

	if (!error) {
		int nwritten = round_page(count - uio.uio_resid) / PAGE_SIZE;
		for (i = 0; i < nwritten; i++) {
			rtvals[i] = VM_PAGER_OK;
			vm_page_undirty(pages[i]);
		}
		if (must_commit) {
			nfs_clearcommit(vp->v_mount);
		}
	}
	return rtvals[0];
}

/*
 * Vnode op for read using bio
 */
int
nfs_bioread(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *cred)
{
	struct nfsnode *np = VTONFS(vp);
	int biosize, i;
	struct buf *bp = 0, *rabp;
	struct vattr vattr;
	struct thread *td;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	daddr_t lbn, rabn;
	int bcount;
	int seqcount;
	int nra, error = 0, n = 0, on = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("nfs_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)	/* XXX VDIR cookies can be negative */
		return (EINVAL);
	td = uio->uio_td;

	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0)
		(void)nfs_fsinfo(nmp, vp, cred, td);
	if (vp->v_type != VDIR &&
	    (uio->uio_offset + uio->uio_resid) > nmp->nm_maxfilesize)
		return (EFBIG);
	biosize = vp->v_mount->mnt_stat.f_iosize;
	seqcount = (int)((off_t)(ioflag >> 16) * biosize / BKVASIZE);
	/*
	 * For nfs, cache consistency can only be maintained approximately.
	 * Although RFC1094 does not specify the criteria, the following is
	 * believed to be compatible with the reference port.
	 * For nfs:
	 * If the file's modify time on the server has changed since the
	 * last read rpc or you have written to the file,
	 * you may have lost data cache consistency with the
	 * server, so flush all of the file's data out of the cache.
	 * Then force a getattr rpc to ensure that you have up to date
	 * attributes.
	 * NB: This implies that cache data can be read when up to
	 * NFS_ATTRTIMEO seconds out of date. If you find that you need current
	 * attributes this could be forced by setting n_attrstamp to 0 before
	 * the VOP_GETATTR() call.
	 */
	if (np->n_flag & NMODIFIED) {
		if (vp->v_type != VREG) {
			if (vp->v_type != VDIR)
				panic("nfs: bioread, not dir");
			nfs_invaldir(vp);
			error = nfs_vinvalbuf(vp, V_SAVE, cred, td, 1);
			if (error)
				return (error);
		}
		np->n_attrstamp = 0;
		error = VOP_GETATTR(vp, &vattr, cred, td);
		if (error)
			return (error);
		np->n_mtime = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, cred, td);
		if (error)
			return (error);
		if (np->n_mtime != vattr.va_mtime.tv_sec) {
			if (vp->v_type == VDIR)
				nfs_invaldir(vp);
			error = nfs_vinvalbuf(vp, V_SAVE, cred, td, 1);
			if (error)
				return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		}
	}
	do {
	    switch (vp->v_type) {
	    case VREG:
		nfsstats.biocache_reads++;
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize - 1);

		/*
		 * Start the read ahead(s), as required.
		 */
		if (nmp->nm_readahead > 0) {
		    for (nra = 0; nra < nmp->nm_readahead && nra < seqcount &&
			(off_t)(lbn + 1 + nra) * biosize < np->n_size; nra++) {
			rabn = lbn + 1 + nra;
			if (!incore(vp, rabn)) {
			    rabp = nfs_getcacheblk(vp, rabn, biosize, td);
			    if (!rabp)
				return (EINTR);
			    if ((rabp->b_flags & (B_CACHE|B_DELWRI)) == 0) {
				rabp->b_flags |= B_ASYNC;
				rabp->b_iocmd = BIO_READ;
				vfs_busy_pages(rabp, 0);
				if (nfs_asyncio(rabp, cred, td)) {
				    rabp->b_flags |= B_INVAL;
				    rabp->b_ioflags |= BIO_ERROR;
				    vfs_unbusy_pages(rabp);
				    brelse(rabp);
				    break;
				}
			    } else {
				brelse(rabp);
			    }
			}
		    }
		}

		/*
		 * Obtain the buffer cache block.  Figure out the buffer size
		 * when we are at EOF.  If we are modifying the size of the
		 * buffer based on an EOF condition we need to hold
		 * nfs_rslock() through obtaining the buffer to prevent
		 * a potential writer-appender from messing with n_size.
		 * Otherwise we may accidently truncate the buffer and
		 * lose dirty data.
		 *
		 * Note that bcount is *not* DEV_BSIZE aligned.
		 */

again:
		bcount = biosize;
		if ((off_t)lbn * biosize >= np->n_size) {
			bcount = 0;
		} else if ((off_t)(lbn + 1) * biosize > np->n_size) {
			bcount = np->n_size - (off_t)lbn * biosize;
		}
		if (bcount != biosize) {
			switch(nfs_rslock(np, td)) {
			case ENOLCK:
				goto again;
				/* not reached */
			case EINTR:
			case ERESTART:
				return(EINTR);
				/* not reached */
			default:
				break;
			}
		}

		bp = nfs_getcacheblk(vp, lbn, bcount, td);

		if (bcount != biosize)
			nfs_rsunlock(np, td);
		if (!bp)
			return (EINTR);

		/*
		 * If B_CACHE is not set, we must issue the read.  If this
		 * fails, we return an error.
		 */

		if ((bp->b_flags & B_CACHE) == 0) {
		    bp->b_iocmd = BIO_READ;
		    vfs_busy_pages(bp, 0);
		    error = nfs_doio(bp, cred, td);
		    if (error) {
			brelse(bp);
			return (error);
		    }
		}

		/*
		 * on is the offset into the current bp.  Figure out how many
		 * bytes we can copy out of the bp.  Note that bcount is
		 * NOT DEV_BSIZE aligned.
		 *
		 * Then figure out how many bytes we can copy into the uio.
		 */

		n = 0;
		if (on < bcount)
			n = min((unsigned)(bcount - on), uio->uio_resid);
		break;
	    case VLNK:
		nfsstats.biocache_readlinks++;
		bp = nfs_getcacheblk(vp, (daddr_t)0, NFS_MAXPATHLEN, td);
		if (!bp)
			return (EINTR);
		if ((bp->b_flags & B_CACHE) == 0) {
		    bp->b_iocmd = BIO_READ;
		    vfs_busy_pages(bp, 0);
		    error = nfs_doio(bp, cred, td);
		    if (error) {
			bp->b_ioflags |= BIO_ERROR;
			brelse(bp);
			return (error);
		    }
		}
		n = min(uio->uio_resid, NFS_MAXPATHLEN - bp->b_resid);
		on = 0;
		break;
	    case VDIR:
		nfsstats.biocache_readdirs++;
		if (np->n_direofoffset
		    && uio->uio_offset >= np->n_direofoffset) {
		    return (0);
		}
		lbn = (uoff_t)uio->uio_offset / NFS_DIRBLKSIZ;
		on = uio->uio_offset & (NFS_DIRBLKSIZ - 1);
		bp = nfs_getcacheblk(vp, lbn, NFS_DIRBLKSIZ, td);
		if (!bp)
		    return (EINTR);
		if ((bp->b_flags & B_CACHE) == 0) {
		    bp->b_iocmd = BIO_READ;
		    vfs_busy_pages(bp, 0);
		    error = nfs_doio(bp, cred, td);
		    if (error) {
			    brelse(bp);
		    }
		    while (error == NFSERR_BAD_COOKIE) {
			printf("got bad cookie vp %p bp %p\n", vp, bp);
			nfs_invaldir(vp);
			error = nfs_vinvalbuf(vp, 0, cred, td, 1);
			/*
			 * Yuck! The directory has been modified on the
			 * server. The only way to get the block is by
			 * reading from the beginning to get all the
			 * offset cookies.
			 *
			 * Leave the last bp intact unless there is an error.
			 * Loop back up to the while if the error is another
			 * NFSERR_BAD_COOKIE (double yuch!).
			 */
			for (i = 0; i <= lbn && !error; i++) {
			    if (np->n_direofoffset
				&& (i * NFS_DIRBLKSIZ) >= np->n_direofoffset)
				    return (0);
			    bp = nfs_getcacheblk(vp, i, NFS_DIRBLKSIZ, td);
			    if (!bp)
				return (EINTR);
			    if ((bp->b_flags & B_CACHE) == 0) {
				    bp->b_iocmd = BIO_READ;
				    vfs_busy_pages(bp, 0);
				    error = nfs_doio(bp, cred, td);
				    /*
				     * no error + B_INVAL == directory EOF,
				     * use the block.
				     */
				    if (error == 0 && (bp->b_flags & B_INVAL))
					    break;
			    }
			    /*
			     * An error will throw away the block and the
			     * for loop will break out.  If no error and this
			     * is not the block we want, we throw away the
			     * block and go for the next one via the for loop.
			     */
			    if (error || i < lbn)
				    brelse(bp);
			}
		    }
		    /*
		     * The above while is repeated if we hit another cookie
		     * error.  If we hit an error and it wasn't a cookie error,
		     * we give up.
		     */
		    if (error)
			    return (error);
		}

		/*
		 * If not eof and read aheads are enabled, start one.
		 * (You need the current block first, so that you have the
		 *  directory offset cookie of the next block.)
		 */
		if (nmp->nm_readahead > 0 &&
		    (bp->b_flags & B_INVAL) == 0 &&
		    (np->n_direofoffset == 0 ||
		    (lbn + 1) * NFS_DIRBLKSIZ < np->n_direofoffset) &&
		    !incore(vp, lbn + 1)) {
			rabp = nfs_getcacheblk(vp, lbn + 1, NFS_DIRBLKSIZ, td);
			if (rabp) {
			    if ((rabp->b_flags & (B_CACHE|B_DELWRI)) == 0) {
				rabp->b_flags |= B_ASYNC;
				rabp->b_iocmd = BIO_READ;
				vfs_busy_pages(rabp, 0);
				if (nfs_asyncio(rabp, cred, td)) {
				    rabp->b_flags |= B_INVAL;
				    rabp->b_ioflags |= BIO_ERROR;
				    vfs_unbusy_pages(rabp);
				    brelse(rabp);
				}
			    } else {
				brelse(rabp);
			    }
			}
		}
		/*
		 * Unlike VREG files, whos buffer size ( bp->b_bcount ) is
		 * chopped for the EOF condition, we cannot tell how large
		 * NFS directories are going to be until we hit EOF.  So
		 * an NFS directory buffer is *not* chopped to its EOF.  Now,
		 * it just so happens that b_resid will effectively chop it
		 * to EOF.  *BUT* this information is lost if the buffer goes
		 * away and is reconstituted into a B_CACHE state ( due to
		 * being VMIO ) later.  So we keep track of the directory eof
		 * in np->n_direofoffset and chop it off as an extra step
		 * right here.
		 */
		n = lmin(uio->uio_resid, NFS_DIRBLKSIZ - bp->b_resid - on);
		if (np->n_direofoffset && n > np->n_direofoffset - uio->uio_offset)
			n = np->n_direofoffset - uio->uio_offset;
		break;
	    default:
		printf(" nfs_bioread: type %x unexpected\n", vp->v_type);
		break;
	    };

	    if (n > 0) {
		    error = uiomove(bp->b_data + on, (int)n, uio);
	    }
	    switch (vp->v_type) {
	    case VREG:
		break;
	    case VLNK:
		n = 0;
		break;
	    case VDIR:
		break;
	    default:
		printf(" nfs_bioread: type %x unexpected\n", vp->v_type);
	    }
	    brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n > 0);
	return (error);
}

/*
 * Vnode op for write using bio
 */
int
nfs_write(struct vop_write_args *ap)
{
	int biosize;
	struct uio *uio = ap->a_uio;
	struct thread *td = uio->uio_td;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct ucred *cred = ap->a_cred;
	int ioflag = ap->a_ioflag;
	struct buf *bp;
	struct vattr vattr;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	daddr_t lbn;
	int bcount;
	int n, on, error = 0;
	int haverslock = 0;
	struct proc *p = td?td->td_proc:NULL;

	GIANT_REQUIRED;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("nfs_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_td != curthread)
		panic("nfs_write proc");
#endif
	if (vp->v_type != VREG)
		return (EIO);
	if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		return (np->n_error);
	}
	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0)
		(void)nfs_fsinfo(nmp, vp, cred, td);

	/*
	 * Synchronously flush pending buffers if we are in synchronous
	 * mode or if we are appending.
	 */
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			np->n_attrstamp = 0;
			error = nfs_vinvalbuf(vp, V_SAVE, cred, td, 1);
			if (error)
				return (error);
		}
	}

	/*
	 * If IO_APPEND then load uio_offset.  We restart here if we cannot
	 * get the append lock.
	 */
restart:
	if (ioflag & IO_APPEND) {
		np->n_attrstamp = 0;
		error = VOP_GETATTR(vp, &vattr, cred, td);
		if (error)
			return (error);
		uio->uio_offset = np->n_size;
	}

	if (uio->uio_offset < 0)
		return (EINVAL);
	if ((uio->uio_offset + uio->uio_resid) > nmp->nm_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	/*
	 * We need to obtain the rslock if we intend to modify np->n_size
	 * in order to guarentee the append point with multiple contending
	 * writers, to guarentee that no other appenders modify n_size
	 * while we are trying to obtain a truncated buffer (i.e. to avoid
	 * accidently truncating data written by another appender due to
	 * the race), and to ensure that the buffer is populated prior to
	 * our extending of the file.  We hold rslock through the entire
	 * operation.
	 *
	 * Note that we do not synchronize the case where someone truncates
	 * the file while we are appending to it because attempting to lock
	 * this case may deadlock other parts of the system unexpectedly.
	 */
	if ((ioflag & IO_APPEND) ||
	    uio->uio_offset + uio->uio_resid > np->n_size) {
		switch(nfs_rslock(np, td)) {
		case ENOLCK:
			goto restart;
			/* not reached */
		case EINTR:
		case ERESTART:
			return(EINTR);
			/* not reached */
		default:
			break;
		}
		haverslock = 1;
	}

	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	if (p && uio->uio_offset + uio->uio_resid >
	      p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		PROC_LOCK(p);
		psignal(p, SIGXFSZ);
		PROC_UNLOCK(p);
		if (haverslock)
			nfs_rsunlock(np, td);
		return (EFBIG);
	}

	biosize = vp->v_mount->mnt_stat.f_iosize;

	do {
		nfsstats.biocache_writes++;
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize-1);
		n = min((unsigned)(biosize - on), uio->uio_resid);
again:
		/*
		 * Handle direct append and file extension cases, calculate
		 * unaligned buffer size.
		 */

		if (uio->uio_offset == np->n_size && n) {
			/*
			 * Get the buffer (in its pre-append state to maintain
			 * B_CACHE if it was previously set).  Resize the
			 * nfsnode after we have locked the buffer to prevent
			 * readers from reading garbage.
			 */
			bcount = on;
			bp = nfs_getcacheblk(vp, lbn, bcount, td);

			if (bp != NULL) {
				long save;

				np->n_size = uio->uio_offset + n;
				np->n_flag |= NMODIFIED;
				vnode_pager_setsize(vp, np->n_size);

				save = bp->b_flags & B_CACHE;
				bcount += n;
				allocbuf(bp, bcount);
				bp->b_flags |= save;
				bp->b_magic = B_MAGIC_NFS;
				bp->b_op = &buf_ops_nfs;
			}
		} else {
			/*
			 * Obtain the locked cache block first, and then
			 * adjust the file's size as appropriate.
			 */
			bcount = on + n;
			if ((off_t)lbn * biosize + bcount < np->n_size) {
				if ((off_t)(lbn + 1) * biosize < np->n_size)
					bcount = biosize;
				else
					bcount = np->n_size - (off_t)lbn * biosize;
			}
			bp = nfs_getcacheblk(vp, lbn, bcount, td);
			if (uio->uio_offset + n > np->n_size) {
				np->n_size = uio->uio_offset + n;
				np->n_flag |= NMODIFIED;
				vnode_pager_setsize(vp, np->n_size);
			}
		}

		if (!bp) {
			error = EINTR;
			break;
		}

		/*
		 * Issue a READ if B_CACHE is not set.  In special-append
		 * mode, B_CACHE is based on the buffer prior to the write
		 * op and is typically set, avoiding the read.  If a read
		 * is required in special append mode, the server will
		 * probably send us a short-read since we extended the file
		 * on our end, resulting in b_resid == 0 and, thusly,
		 * B_CACHE getting set.
		 *
		 * We can also avoid issuing the read if the write covers
		 * the entire buffer.  We have to make sure the buffer state
		 * is reasonable in this case since we will not be initiating
		 * I/O.  See the comments in kern/vfs_bio.c's getblk() for
		 * more information.
		 *
		 * B_CACHE may also be set due to the buffer being cached
		 * normally.
		 */

		if (on == 0 && n == bcount) {
			bp->b_flags |= B_CACHE;
			bp->b_flags &= ~B_INVAL;
			bp->b_ioflags &= ~BIO_ERROR;
		}

		if ((bp->b_flags & B_CACHE) == 0) {
			bp->b_iocmd = BIO_READ;
			vfs_busy_pages(bp, 0);
			error = nfs_doio(bp, cred, td);
			if (error) {
				brelse(bp);
				break;
			}
		}
		if (!bp) {
			error = EINTR;
			break;
		}
		if (bp->b_wcred == NOCRED)
			bp->b_wcred = crhold(cred);
		np->n_flag |= NMODIFIED;

		/*
		 * If dirtyend exceeds file size, chop it down.  This should
		 * not normally occur but there is an append race where it
		 * might occur XXX, so we log it.
		 *
		 * If the chopping creates a reverse-indexed or degenerate
		 * situation with dirtyoff/end, we 0 both of them.
		 */

		if (bp->b_dirtyend > bcount) {
			printf("NFS append race @%lx:%d\n",
			    (long)bp->b_blkno * DEV_BSIZE,
			    bp->b_dirtyend - bcount);
			bp->b_dirtyend = bcount;
		}

		if (bp->b_dirtyoff >= bp->b_dirtyend)
			bp->b_dirtyoff = bp->b_dirtyend = 0;

		/*
		 * If the new write will leave a contiguous dirty
		 * area, just update the b_dirtyoff and b_dirtyend,
		 * otherwise force a write rpc of the old dirty area.
		 *
		 * While it is possible to merge discontiguous writes due to
		 * our having a B_CACHE buffer ( and thus valid read data
		 * for the hole), we don't because it could lead to
		 * significant cache coherency problems with multiple clients,
		 * especially if locking is implemented later on.
		 *
		 * as an optimization we could theoretically maintain
		 * a linked list of discontinuous areas, but we would still
		 * have to commit them separately so there isn't much
		 * advantage to it except perhaps a bit of asynchronization.
		 */

		if (bp->b_dirtyend > 0 &&
		    (on > bp->b_dirtyend || (on + n) < bp->b_dirtyoff)) {
			if (BUF_WRITE(bp) == EINTR)
				return (EINTR);
			goto again;
		}

		error = uiomove((char *)bp->b_data + on, n, uio);

		/*
		 * Since this block is being modified, it must be written
		 * again and not just committed.  Since write clustering does
		 * not work for the stage 1 data write, only the stage 2
		 * commit rpc, we have to clear B_CLUSTEROK as well.
		 */
		bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);

		if (error) {
			bp->b_ioflags |= BIO_ERROR;
			brelse(bp);
			break;
		}

		/*
		 * Only update dirtyoff/dirtyend if not a degenerate
		 * condition.
		 */
		if (n) {
			if (bp->b_dirtyend > 0) {
				bp->b_dirtyoff = min(on, bp->b_dirtyoff);
				bp->b_dirtyend = max((on + n), bp->b_dirtyend);
			} else {
				bp->b_dirtyoff = on;
				bp->b_dirtyend = on + n;
			}
			vfs_bio_set_validclean(bp, on, n);
		}
		/*
		 * If IO_NOWDRAIN then set B_NOWDRAIN (nfs-backed MD 
		 * filesystem)
		 */
		if (ioflag & IO_NOWDRAIN)
			bp->b_flags |= B_NOWDRAIN;

		/*
		 * If IO_SYNC do bwrite().
		 *
		 * IO_INVAL appears to be unused.  The idea appears to be
		 * to turn off caching in this case.  Very odd.  XXX
		 */
		if ((ioflag & IO_SYNC)) {
			if (ioflag & IO_INVAL)
				bp->b_flags |= B_NOCACHE;
			error = BUF_WRITE(bp);
			if (error)
				break;
		} else if ((n + on) == biosize) {
			bp->b_flags |= B_ASYNC;
			(void)nfs_writebp(bp, 0, 0);
		} else {
			bdwrite(bp);
		}
	} while (uio->uio_resid > 0 && n > 0);

	if (haverslock)
		nfs_rsunlock(np, td);

	return (error);
}

/*
 * Get an nfs cache block.
 *
 * Allocate a new one if the block isn't currently in the cache
 * and return the block marked busy. If the calling process is
 * interrupted by a signal for an interruptible mount point, return
 * NULL.
 *
 * The caller must carefully deal with the possible B_INVAL state of
 * the buffer.  nfs_doio() clears B_INVAL (and nfs_asyncio() clears it
 * indirectly), so synchronous reads can be issued without worrying about
 * the B_INVAL state.  We have to be a little more careful when dealing
 * with writes (see comments in nfs_write()) when extending a file past
 * its EOF.
 */
static struct buf *
nfs_getcacheblk(struct vnode *vp, daddr_t bn, int size, struct thread *td)
{
	struct buf *bp;
	struct mount *mp;
	struct nfsmount *nmp;

	mp = vp->v_mount;
	nmp = VFSTONFS(mp);

	if (nmp->nm_flag & NFSMNT_INT) {
		bp = getblk(vp, bn, size, PCATCH, 0);
		while (bp == (struct buf *)0) {
			if (nfs_sigintr(nmp, (struct nfsreq *)0, td->td_proc))
				return ((struct buf *)0);
			bp = getblk(vp, bn, size, 0, 2 * hz);
		}
	} else {
		bp = getblk(vp, bn, size, 0, 0);
	}

	if (vp->v_type == VREG) {
		int biosize;

		biosize = mp->mnt_stat.f_iosize;
		bp->b_blkno = bn * (biosize / DEV_BSIZE);
	}
	return (bp);
}

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
nfs_vinvalbuf(struct vnode *vp, int flags, struct ucred *cred,
    struct thread *td, int intrflg)
{
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, slpflag, slptimeo;

	if (vp->v_flag & VXLOCK) {
		return (0);
	}

	if ((nmp->nm_flag & NFSMNT_INT) == 0)
		intrflg = 0;
	if (intrflg) {
		slpflag = PCATCH;
		slptimeo = 2 * hz;
	} else {
		slpflag = 0;
		slptimeo = 0;
	}
	/*
	 * First wait for any other process doing a flush to complete.
	 */
	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = tsleep((caddr_t)&np->n_flag, PRIBIO + 2, "nfsvinval",
			slptimeo);
		if (error && intrflg &&
		    nfs_sigintr(nmp, (struct nfsreq *)0, td->td_proc))
			return (EINTR);
	}

	/*
	 * Now, flush as required.
	 */
	np->n_flag |= NFLUSHINPROG;
	error = vinvalbuf(vp, flags, cred, td, slpflag, 0);
	while (error) {
		if (intrflg &&
		    nfs_sigintr(nmp, (struct nfsreq *)0, td->td_proc)) {
			np->n_flag &= ~NFLUSHINPROG;
			if (np->n_flag & NFLUSHWANT) {
				np->n_flag &= ~NFLUSHWANT;
				wakeup((caddr_t)&np->n_flag);
			}
			return (EINTR);
		}
		error = vinvalbuf(vp, flags, cred, td, 0, slptimeo);
	}
	np->n_flag &= ~(NMODIFIED | NFLUSHINPROG);
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup((caddr_t)&np->n_flag);
	}
	return (0);
}

/*
 * Initiate asynchronous I/O. Return an error if no nfsiods are available.
 * This is mainly to avoid queueing async I/O requests when the nfsiods
 * are all hung on a dead server.
 *
 * Note: nfs_asyncio() does not clear (BIO_ERROR|B_INVAL) but when the bp
 * is eventually dequeued by the async daemon, nfs_doio() *will*.
 */
int
nfs_asyncio(struct buf *bp, struct ucred *cred, struct thread *td)
{
	struct nfsmount *nmp;
	int iod;
	int gotiod;
	int slpflag = 0;
	int slptimeo = 0;
	int error;

	nmp = VFSTONFS(bp->b_vp->v_mount);

	/*
	 * Commits are usually short and sweet so lets save some cpu and
	 * leave the async daemons for more important rpc's (such as reads
	 * and writes).
	 */
	if (bp->b_iocmd == BIO_WRITE && (bp->b_flags & B_NEEDCOMMIT) &&
	    (nmp->nm_bufqiods > nfs_numasync / 2)) {
		return(EIO);
	}

again:
	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = PCATCH;
	gotiod = FALSE;

	/*
	 * Find a free iod to process this request.
	 */
	for (iod = 0; iod < nfs_numasync; iod++)
		if (nfs_iodwant[iod]) {
			gotiod = TRUE;
			break;
		}

	/*
	 * Try to create one if none are free.
	 */
	if (!gotiod) {
		iod = nfs_nfsiodnew();
		if (iod != -1)
			gotiod = TRUE;
	}

	if (gotiod) {
		/*
		 * Found one, so wake it up and tell it which
		 * mount to process.
		 */
		NFS_DPF(ASYNCIO, ("nfs_asyncio: waking iod %d for mount %p\n",
		    iod, nmp));
		nfs_iodwant[iod] = (struct proc *)0;
		nfs_iodmount[iod] = nmp;
		nmp->nm_bufqiods++;
		wakeup((caddr_t)&nfs_iodwant[iod]);
	}

	/*
	 * If none are free, we may already have an iod working on this mount
	 * point.  If so, it will process our request.
	 */
	if (!gotiod) {
		if (nmp->nm_bufqiods > 0) {
			NFS_DPF(ASYNCIO,
				("nfs_asyncio: %d iods are already processing mount %p\n",
				 nmp->nm_bufqiods, nmp));
			gotiod = TRUE;
		}
	}

	/*
	 * If we have an iod which can process the request, then queue
	 * the buffer.
	 */
	if (gotiod) {
		/*
		 * Ensure that the queue never grows too large.  We still want
		 * to asynchronize so we block rather then return EIO.
		 */
		while (nmp->nm_bufqlen >= 2*nfs_numasync) {
			NFS_DPF(ASYNCIO,
				("nfs_asyncio: waiting for mount %p queue to drain\n", nmp));
			nmp->nm_bufqwant = TRUE;
			error = tsleep(&nmp->nm_bufq, slpflag | PRIBIO,
				       "nfsaio", slptimeo);
			if (error) {
				if (nfs_sigintr(nmp, NULL, td ? td->td_proc : NULL))
					return (EINTR);
				if (slpflag == PCATCH) {
					slpflag = 0;
					slptimeo = 2 * hz;
				}
			}
			/*
			 * We might have lost our iod while sleeping,
			 * so check and loop if nescessary.
			 */
			if (nmp->nm_bufqiods == 0) {
				NFS_DPF(ASYNCIO,
					("nfs_asyncio: no iods after mount %p queue was drained, looping\n", nmp));
				goto again;
			}
		}

		if (bp->b_iocmd == BIO_READ) {
			if (bp->b_rcred == NOCRED && cred != NOCRED)
				bp->b_rcred = crhold(cred);
		} else {
			bp->b_flags |= B_WRITEINPROG;
			if (bp->b_wcred == NOCRED && cred != NOCRED)
				bp->b_wcred = crhold(cred);
		}

		BUF_KERNPROC(bp);
		TAILQ_INSERT_TAIL(&nmp->nm_bufq, bp, b_freelist);
		nmp->nm_bufqlen++;
		return (0);
	}

	/*
	 * All the iods are busy on other mounts, so return EIO to
	 * force the caller to process the i/o synchronously.
	 */
	NFS_DPF(ASYNCIO, ("nfs_asyncio: no iods available, i/o is synchronous\n"));
	return (EIO);
}

/*
 * Do an I/O operation to/from a cache block. This may be called
 * synchronously or from an nfsiod.
 */
int
nfs_doio(struct buf *bp, struct ucred *cr, struct thread *td)
{
	struct uio *uiop;
	struct vnode *vp;
	struct nfsnode *np;
	struct nfsmount *nmp;
	int error = 0, iomode, must_commit = 0;
	struct uio uio;
	struct iovec io;
	struct proc *p = td ? td->td_proc : NULL;

	vp = bp->b_vp;
	np = VTONFS(vp);
	nmp = VFSTONFS(vp->v_mount);
	uiop = &uio;
	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_td = td;

	/*
	 * clear BIO_ERROR and B_INVAL state prior to initiating the I/O.  We
	 * do this here so we do not have to do it in all the code that
	 * calls us.
	 */
	bp->b_flags &= ~B_INVAL;
	bp->b_ioflags &= ~BIO_ERROR;

	KASSERT(!(bp->b_flags & B_DONE), ("nfs_doio: bp %p already marked done", bp));

	/*
	 * Historically, paging was done with physio, but no more.
	 */
	if (bp->b_flags & B_PHYS) {
	    /*
	     * ...though reading /dev/drum still gets us here.
	     */
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    /* mapping was done by vmapbuf() */
	    io.iov_base = bp->b_data;
	    uiop->uio_offset = ((off_t)bp->b_blkno) * DEV_BSIZE;
	    if (bp->b_iocmd == BIO_READ) {
		uiop->uio_rw = UIO_READ;
		nfsstats.read_physios++;
		error = nfs_readrpc(vp, uiop, cr);
	    } else {
		int com;

		iomode = NFSV3WRITE_DATASYNC;
		uiop->uio_rw = UIO_WRITE;
		nfsstats.write_physios++;
		error = nfs_writerpc(vp, uiop, cr, &iomode, &com);
	    }
	    if (error) {
		bp->b_ioflags |= BIO_ERROR;
		bp->b_error = error;
	    }
	} else if (bp->b_iocmd == BIO_READ) {
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    io.iov_base = bp->b_data;
	    uiop->uio_rw = UIO_READ;

	    switch (vp->v_type) {
	    case VREG:
		uiop->uio_offset = ((off_t)bp->b_blkno) * DEV_BSIZE;
		nfsstats.read_bios++;
		error = nfs_readrpc(vp, uiop, cr);

		if (!error) {
		    if (uiop->uio_resid) {
			/*
			 * If we had a short read with no error, we must have
			 * hit a file hole.  We should zero-fill the remainder.
			 * This can also occur if the server hits the file EOF.
			 *
			 * Holes used to be able to occur due to pending
			 * writes, but that is not possible any longer.
			 */
			int nread = bp->b_bcount - uiop->uio_resid;
			int left  = uiop->uio_resid;

			if (left > 0)
				bzero((char *)bp->b_data + nread, left);
			uiop->uio_resid = 0;
		    }
		}
		if (p && (vp->v_flag & VTEXT) &&
			(np->n_mtime != np->n_vattr.va_mtime.tv_sec)) {
			uprintf("Process killed due to text file modification\n");
			PROC_LOCK(p);
			psignal(p, SIGKILL);
			_PHOLD(p);
			PROC_UNLOCK(p);
		}
		break;
	    case VLNK:
		uiop->uio_offset = (off_t)0;
		nfsstats.readlink_bios++;
		error = nfs_readlinkrpc(vp, uiop, cr);
		break;
	    case VDIR:
		nfsstats.readdir_bios++;
		uiop->uio_offset = ((u_quad_t)bp->b_lblkno) * NFS_DIRBLKSIZ;
		if (nmp->nm_flag & NFSMNT_RDIRPLUS) {
			error = nfs_readdirplusrpc(vp, uiop, cr);
			if (error == NFSERR_NOTSUPP)
				nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
		}
		if ((nmp->nm_flag & NFSMNT_RDIRPLUS) == 0)
			error = nfs_readdirrpc(vp, uiop, cr);
		/*
		 * end-of-directory sets B_INVAL but does not generate an
		 * error.
		 */
		if (error == 0 && uiop->uio_resid == bp->b_bcount)
			bp->b_flags |= B_INVAL;
		break;
	    default:
		printf("nfs_doio:  type %x unexpected\n", vp->v_type);
		break;
	    };
	    if (error) {
		bp->b_ioflags |= BIO_ERROR;
		bp->b_error = error;
	    }
	} else {
	    /*
	     * If we only need to commit, try to commit
	     */
	    if (bp->b_flags & B_NEEDCOMMIT) {
		    int retv;
		    off_t off;

		    off = ((u_quad_t)bp->b_blkno) * DEV_BSIZE + bp->b_dirtyoff;
		    bp->b_flags |= B_WRITEINPROG;
		    retv = nfs_commit(
				bp->b_vp, off, bp->b_dirtyend-bp->b_dirtyoff,
				bp->b_wcred, td);
		    bp->b_flags &= ~B_WRITEINPROG;
		    if (retv == 0) {
			    bp->b_dirtyoff = bp->b_dirtyend = 0;
			    bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);
			    bp->b_resid = 0;
			    bufdone(bp);
			    return (0);
		    }
		    if (retv == NFSERR_STALEWRITEVERF) {
			    nfs_clearcommit(bp->b_vp->v_mount);
		    }
	    }

	    /*
	     * Setup for actual write
	     */

	    if ((off_t)bp->b_blkno * DEV_BSIZE + bp->b_dirtyend > np->n_size)
		bp->b_dirtyend = np->n_size - (off_t)bp->b_blkno * DEV_BSIZE;

	    if (bp->b_dirtyend > bp->b_dirtyoff) {
		io.iov_len = uiop->uio_resid = bp->b_dirtyend
		    - bp->b_dirtyoff;
		uiop->uio_offset = (off_t)bp->b_blkno * DEV_BSIZE
		    + bp->b_dirtyoff;
		io.iov_base = (char *)bp->b_data + bp->b_dirtyoff;
		uiop->uio_rw = UIO_WRITE;
		nfsstats.write_bios++;

		if ((bp->b_flags & (B_ASYNC | B_NEEDCOMMIT | B_NOCACHE | B_CLUSTER)) == B_ASYNC)
		    iomode = NFSV3WRITE_UNSTABLE;
		else
		    iomode = NFSV3WRITE_FILESYNC;

		bp->b_flags |= B_WRITEINPROG;
		error = nfs_writerpc(vp, uiop, cr, &iomode, &must_commit);

		/*
		 * When setting B_NEEDCOMMIT also set B_CLUSTEROK to try
		 * to cluster the buffers needing commit.  This will allow
		 * the system to submit a single commit rpc for the whole
		 * cluster.  We can do this even if the buffer is not 100%
		 * dirty (relative to the NFS blocksize), so we optimize the
		 * append-to-file-case.
		 *
		 * (when clearing B_NEEDCOMMIT, B_CLUSTEROK must also be
		 * cleared because write clustering only works for commit
		 * rpc's, not for the data portion of the write).
		 */

		if (!error && iomode == NFSV3WRITE_UNSTABLE) {
		    bp->b_flags |= B_NEEDCOMMIT;
		    if (bp->b_dirtyoff == 0
			&& bp->b_dirtyend == bp->b_bcount)
			bp->b_flags |= B_CLUSTEROK;
		} else {
		    bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);
		}
		bp->b_flags &= ~B_WRITEINPROG;

		/*
		 * For an interrupted write, the buffer is still valid
		 * and the write hasn't been pushed to the server yet,
		 * so we can't set BIO_ERROR and report the interruption
		 * by setting B_EINTR. For the B_ASYNC case, B_EINTR
		 * is not relevant, so the rpc attempt is essentially
		 * a noop.  For the case of a V3 write rpc not being
		 * committed to stable storage, the block is still
		 * dirty and requires either a commit rpc or another
		 * write rpc with iomode == NFSV3WRITE_FILESYNC before
		 * the block is reused. This is indicated by setting
		 * the B_DELWRI and B_NEEDCOMMIT flags.
		 *
		 * If the buffer is marked B_PAGING, it does not reside on
		 * the vp's paging queues so we cannot call bdirty().  The
		 * bp in this case is not an NFS cache block so we should
		 * be safe. XXX
		 */
    		if (error == EINTR
		    || (!error && (bp->b_flags & B_NEEDCOMMIT))) {
			int s;

			s = splbio();
			bp->b_flags &= ~(B_INVAL|B_NOCACHE);
			if ((bp->b_flags & B_PAGING) == 0) {
			    bdirty(bp);
			    bp->b_flags &= ~B_DONE;
			}
			if (error && (bp->b_flags & B_ASYNC) == 0)
			    bp->b_flags |= B_EINTR;
			splx(s);
	    	} else {
		    if (error) {
			bp->b_ioflags |= BIO_ERROR;
			bp->b_error = np->n_error = error;
			np->n_flag |= NWRITEERR;
		    }
		    bp->b_dirtyoff = bp->b_dirtyend = 0;
		}
	    } else {
		bp->b_resid = 0;
		bufdone(bp);
		return (0);
	    }
	}
	bp->b_resid = uiop->uio_resid;
	if (must_commit)
	    nfs_clearcommit(vp->v_mount);
	bufdone(bp);
	return (error);
}

/*
 * Used to aid in handling ftruncate() operations on the NFS client side.
 * Truncation creates a number of special problems for NFS.  We have to
 * throw away VM pages and buffer cache buffers that are beyond EOF, and
 * we have to properly handle VM pages or (potentially dirty) buffers
 * that straddle the truncation point.
 */

int
nfs_meta_setsize(struct vnode *vp, struct ucred *cred, struct thread *td, u_quad_t nsize)
{
	struct nfsnode *np = VTONFS(vp);
	u_quad_t tsize = np->n_size;
	int biosize = vp->v_mount->mnt_stat.f_iosize;
	int error = 0;

	np->n_size = nsize;

	if (np->n_size < tsize) {
		struct buf *bp;
		daddr_t lbn;
		int bufsize;

		/*
		 * vtruncbuf() doesn't get the buffer overlapping the 
		 * truncation point.  We may have a B_DELWRI and/or B_CACHE
		 * buffer that now needs to be truncated.
		 */
		error = vtruncbuf(vp, cred, td, nsize, biosize);
		lbn = nsize / biosize;
		bufsize = nsize & (biosize - 1);
		bp = nfs_getcacheblk(vp, lbn, bufsize, td);
		if (bp->b_dirtyoff > bp->b_bcount)
			bp->b_dirtyoff = bp->b_bcount;
		if (bp->b_dirtyend > bp->b_bcount)
			bp->b_dirtyend = bp->b_bcount;
		bp->b_flags |= B_RELBUF;  /* don't leave garbage around */
		brelse(bp);
	} else {
		vnode_pager_setsize(vp, nsize);
	}
	return(error);
}

