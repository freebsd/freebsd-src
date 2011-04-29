/*-
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

#include <fs/nfs/nfsport.h>
#include <fs/nfsclient/nfsmount.h>
#include <fs/nfsclient/nfs.h>
#include <fs/nfsclient/nfsnode.h>

extern int newnfs_directio_allow_mmap;
extern struct nfsstats newnfsstats;
extern struct mtx ncl_iod_mutex;
extern int ncl_numasync;
extern enum nfsiod_state ncl_iodwant[NFS_MAXASYNCDAEMON];
extern struct nfsmount *ncl_iodmount[NFS_MAXASYNCDAEMON];
extern int newnfs_directio_enable;

int ncl_pbuf_freecnt = -1;	/* start out unlimited */

static struct buf *nfs_getcacheblk(struct vnode *vp, daddr_t bn, int size,
    struct thread *td);
static int nfs_directio_write(struct vnode *vp, struct uio *uiop, 
    struct ucred *cred, int ioflag);

/*
 * Vnode op for VM getpages.
 */
int
ncl_getpages(struct vop_getpages_args *ap)
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
	vm_object_t object;
	vm_page_t *pages;
	struct nfsnode *np;

	vp = ap->a_vp;
	np = VTONFS(vp);
	td = curthread;				/* XXX */
	cred = curthread->td_ucred;		/* XXX */
	nmp = VFSTONFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;

	if ((object = vp->v_object) == NULL) {
		ncl_printf("nfs_getpages: called with non-merged cache vnode??\n");
		return (VM_PAGER_ERROR);
	}

	if (newnfs_directio_enable && !newnfs_directio_allow_mmap) {
		mtx_lock(&np->n_mtx);
		if ((np->n_flag & NNONCACHE) && (vp->v_type == VREG)) {
			mtx_unlock(&np->n_mtx);
			ncl_printf("nfs_getpages: called on non-cacheable vnode??\n");
			return (VM_PAGER_ERROR);
		} else
			mtx_unlock(&np->n_mtx);
	}

	mtx_lock(&nmp->nm_mtx);
	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {	
		mtx_unlock(&nmp->nm_mtx);
		/* We'll never get here for v4, because we always have fsinfo */
		(void)ncl_fsinfo(nmp, vp, cred, td);
	} else
		mtx_unlock(&nmp->nm_mtx);

	npages = btoc(count);

	/*
	 * If the requested page is partially valid, just return it and
	 * allow the pager to zero-out the blanks.  Partially valid pages
	 * can only occur at the file EOF.
	 */
	VM_OBJECT_LOCK(object);
	if (pages[ap->a_reqpage]->valid != 0) {
		vm_page_lock_queues();
		for (i = 0; i < npages; ++i) {
			if (i != ap->a_reqpage)
				vm_page_free(pages[i]);
		}
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(object);
		return (0);
	}
	VM_OBJECT_UNLOCK(object);

	/*
	 * We use only the kva address for the buffer, but this is extremely
	 * convienient and fast.
	 */
	bp = getpbuf(&ncl_pbuf_freecnt);

	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);
	PCPU_INC(cnt.v_vnodein);
	PCPU_ADD(cnt.v_vnodepgsin, npages);

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;

	error = ncl_readrpc(vp, &uio, cred);
	pmap_qremove(kva, npages);

	relpbuf(bp, &ncl_pbuf_freecnt);

	if (error && (uio.uio_resid == count)) {
		ncl_printf("nfs_getpages: error %d\n", error);
		VM_OBJECT_LOCK(object);
		vm_page_lock_queues();
		for (i = 0; i < npages; ++i) {
			if (i != ap->a_reqpage)
				vm_page_free(pages[i]);
		}
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(object);
		return (VM_PAGER_ERROR);
	}

	/*
	 * Calculate the number of bytes read and validate only that number
	 * of bytes.  Note that due to pending writes, size may be 0.  This
	 * does not mean that the remaining data is invalid!
	 */

	size = count - uio.uio_resid;
	VM_OBJECT_LOCK(object);
	vm_page_lock_queues();
	for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
		vm_page_t m;
		nextoff = toff + PAGE_SIZE;
		m = pages[i];

		if (nextoff <= size) {
			/*
			 * Read operation filled an entire page
			 */
			m->valid = VM_PAGE_BITS_ALL;
			KASSERT(m->dirty == 0,
			    ("nfs_getpages: page %p is dirty", m));
		} else if (size > toff) {
			/*
			 * Read operation filled a partial page.
			 */
			m->valid = 0;
			vm_page_set_valid(m, 0, size - toff);
			KASSERT(m->dirty == 0,
			    ("nfs_getpages: page %p is dirty", m));
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
				if (m->oflags & VPO_WANTED)
					vm_page_activate(m);
				else
					vm_page_deactivate(m);
				vm_page_wakeup(m);
			} else {
				vm_page_free(m);
			}
		}
	}
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(object);
	return (0);
}

/*
 * Vnode op for VM putpages.
 */
int
ncl_putpages(struct vop_putpages_args *ap)
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
	
	mtx_lock(&nmp->nm_mtx);
	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {
		mtx_unlock(&nmp->nm_mtx);
		(void)ncl_fsinfo(nmp, vp, cred, td);
	} else
		mtx_unlock(&nmp->nm_mtx);

	mtx_lock(&np->n_mtx);
	if (newnfs_directio_enable && !newnfs_directio_allow_mmap && 
	    (np->n_flag & NNONCACHE) && (vp->v_type == VREG)) {
		mtx_unlock(&np->n_mtx);		
		ncl_printf("ncl_putpages: called on noncache-able vnode??\n");
		mtx_lock(&np->n_mtx);
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
	mtx_unlock(&np->n_mtx);

	/*
	 * We use only the kva address for the buffer, but this is extremely
	 * convienient and fast.
	 */
	bp = getpbuf(&ncl_pbuf_freecnt);

	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);
	PCPU_INC(cnt.v_vnodeout);
	PCPU_ADD(cnt.v_vnodepgsout, count);

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
	    iomode = NFSWRITE_UNSTABLE;
	else
	    iomode = NFSWRITE_FILESYNC;

	error = ncl_writerpc(vp, &uio, cred, &iomode, &must_commit, 0);

	pmap_qremove(kva, npages);
	relpbuf(bp, &ncl_pbuf_freecnt);

	if (!error) {
		int nwritten = round_page(count - uio.uio_resid) / PAGE_SIZE;
		for (i = 0; i < nwritten; i++) {
			rtvals[i] = VM_PAGER_OK;
			vm_page_undirty(pages[i]);
		}
		if (must_commit) {
			ncl_clearcommit(vp->v_mount);
		}
	}
	return rtvals[0];
}

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
static inline int
nfs_bioread_check_cons(struct vnode *vp, struct thread *td, struct ucred *cred)
{
	int error = 0;
	struct vattr vattr;
	struct nfsnode *np = VTONFS(vp);
	int old_lock;
	
	/*
	 * Grab the exclusive lock before checking whether the cache is
	 * consistent.
	 * XXX - We can make this cheaper later (by acquiring cheaper locks).
	 * But for now, this suffices.
	 */
	old_lock = ncl_upgrade_vnlock(vp);
	if (vp->v_iflag & VI_DOOMED) {
		ncl_downgrade_vnlock(vp, old_lock);
		return (EBADF);
	}

	mtx_lock(&np->n_mtx);
	if (np->n_flag & NMODIFIED) {
		mtx_unlock(&np->n_mtx);
		if (vp->v_type != VREG) {
			if (vp->v_type != VDIR)
				panic("nfs: bioread, not dir");
			ncl_invaldir(vp);
			error = ncl_vinvalbuf(vp, V_SAVE, td, 1);
			if (error)
				goto out;
		}
		np->n_attrstamp = 0;
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			goto out;
		mtx_lock(&np->n_mtx);
		np->n_mtime = vattr.va_mtime;
		mtx_unlock(&np->n_mtx);
	} else {
		mtx_unlock(&np->n_mtx);
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			return (error);
		mtx_lock(&np->n_mtx);
		if ((np->n_flag & NSIZECHANGED)
		    || (NFS_TIMESPEC_COMPARE(&np->n_mtime, &vattr.va_mtime))) {
			mtx_unlock(&np->n_mtx);
			if (vp->v_type == VDIR)
				ncl_invaldir(vp);
			error = ncl_vinvalbuf(vp, V_SAVE, td, 1);
			if (error)
				goto out;
			mtx_lock(&np->n_mtx);
			np->n_mtime = vattr.va_mtime;
			np->n_flag &= ~NSIZECHANGED;
		}
		mtx_unlock(&np->n_mtx);
	}
out:	
	ncl_downgrade_vnlock(vp, old_lock);
	return error;
}

/*
 * Vnode op for read using bio
 */
int
ncl_bioread(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *cred)
{
	struct nfsnode *np = VTONFS(vp);
	int biosize, i;
	struct buf *bp, *rabp;
	struct thread *td;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	daddr_t lbn, rabn;
	int bcount;
	int seqcount;
	int nra, error = 0, n = 0, on = 0;

	KASSERT(uio->uio_rw == UIO_READ, ("ncl_read mode"));
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)	/* XXX VDIR cookies can be negative */
		return (EINVAL);
	td = uio->uio_td;

	mtx_lock(&nmp->nm_mtx);
	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {
		mtx_unlock(&nmp->nm_mtx);
		(void)ncl_fsinfo(nmp, vp, cred, td);
		mtx_lock(&nmp->nm_mtx);
	}
	if (nmp->nm_rsize == 0 || nmp->nm_readdirsize == 0)
		(void) newnfs_iosize(nmp);
	mtx_unlock(&nmp->nm_mtx);		

	if (vp->v_type != VDIR &&
	    (uio->uio_offset + uio->uio_resid) > nmp->nm_maxfilesize)
		return (EFBIG);

	if (newnfs_directio_enable && (ioflag & IO_DIRECT) && (vp->v_type == VREG))
		/* No caching/ no readaheads. Just read data into the user buffer */
		return ncl_readrpc(vp, uio, cred);

	biosize = vp->v_mount->mnt_stat.f_iosize;
	seqcount = (int)((off_t)(ioflag >> IO_SEQSHIFT) * biosize / BKVASIZE);
	
	error = nfs_bioread_check_cons(vp, td, cred);
	if (error)
		return error;

	do {
	    u_quad_t nsize;
			
	    mtx_lock(&np->n_mtx);
	    nsize = np->n_size;
	    mtx_unlock(&np->n_mtx);		    

	    switch (vp->v_type) {
	    case VREG:
		NFSINCRGLOBAL(newnfsstats.biocache_reads);
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize - 1);

		/*
		 * Start the read ahead(s), as required.
		 */
		if (nmp->nm_readahead > 0) {
		    for (nra = 0; nra < nmp->nm_readahead && nra < seqcount &&
			(off_t)(lbn + 1 + nra) * biosize < nsize; nra++) {
			rabn = lbn + 1 + nra;
			if (incore(&vp->v_bufobj, rabn) == NULL) {
			    rabp = nfs_getcacheblk(vp, rabn, biosize, td);
			    if (!rabp) {
				error = newnfs_sigintr(nmp, td);
				return (error ? error : EINTR);
			    }
			    if ((rabp->b_flags & (B_CACHE|B_DELWRI)) == 0) {
				rabp->b_flags |= B_ASYNC;
				rabp->b_iocmd = BIO_READ;
				vfs_busy_pages(rabp, 0);
				if (ncl_asyncio(nmp, rabp, cred, td)) {
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

		/* Note that bcount is *not* DEV_BSIZE aligned. */
		bcount = biosize;
		if ((off_t)lbn * biosize >= nsize) {
			bcount = 0;
		} else if ((off_t)(lbn + 1) * biosize > nsize) {
			bcount = nsize - (off_t)lbn * biosize;
		}
		bp = nfs_getcacheblk(vp, lbn, bcount, td);

		if (!bp) {
			error = newnfs_sigintr(nmp, td);
			return (error ? error : EINTR);
		}

		/*
		 * If B_CACHE is not set, we must issue the read.  If this
		 * fails, we return an error.
		 */

		if ((bp->b_flags & B_CACHE) == 0) {
		    bp->b_iocmd = BIO_READ;
		    vfs_busy_pages(bp, 0);
		    error = ncl_doio(vp, bp, cred, td, 0);
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
		NFSINCRGLOBAL(newnfsstats.biocache_readlinks);
		bp = nfs_getcacheblk(vp, (daddr_t)0, NFS_MAXPATHLEN, td);
		if (!bp) {
			error = newnfs_sigintr(nmp, td);
			return (error ? error : EINTR);
		}
		if ((bp->b_flags & B_CACHE) == 0) {
		    bp->b_iocmd = BIO_READ;
		    vfs_busy_pages(bp, 0);
		    error = ncl_doio(vp, bp, cred, td, 0);
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
		NFSINCRGLOBAL(newnfsstats.biocache_readdirs);
		if (np->n_direofoffset
		    && uio->uio_offset >= np->n_direofoffset) {
		    return (0);
		}
		lbn = (uoff_t)uio->uio_offset / NFS_DIRBLKSIZ;
		on = uio->uio_offset & (NFS_DIRBLKSIZ - 1);
		bp = nfs_getcacheblk(vp, lbn, NFS_DIRBLKSIZ, td);
		if (!bp) {
		    error = newnfs_sigintr(nmp, td);
		    return (error ? error : EINTR);
		}
		if ((bp->b_flags & B_CACHE) == 0) {
		    bp->b_iocmd = BIO_READ;
		    vfs_busy_pages(bp, 0);
		    error = ncl_doio(vp, bp, cred, td, 0);
		    if (error) {
			    brelse(bp);
		    }
		    while (error == NFSERR_BAD_COOKIE) {
			ncl_invaldir(vp);
			error = ncl_vinvalbuf(vp, 0, td, 1);
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
			    if (!bp) {
				error = newnfs_sigintr(nmp, td);
				return (error ? error : EINTR);
			    }
			    if ((bp->b_flags & B_CACHE) == 0) {
				    bp->b_iocmd = BIO_READ;
				    vfs_busy_pages(bp, 0);
				    error = ncl_doio(vp, bp, cred, td, 0);
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
		    incore(&vp->v_bufobj, lbn + 1) == NULL) {
			rabp = nfs_getcacheblk(vp, lbn + 1, NFS_DIRBLKSIZ, td);
			if (rabp) {
			    if ((rabp->b_flags & (B_CACHE|B_DELWRI)) == 0) {
				rabp->b_flags |= B_ASYNC;
				rabp->b_iocmd = BIO_READ;
				vfs_busy_pages(rabp, 0);
				if (ncl_asyncio(nmp, rabp, cred, td)) {
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
		ncl_printf(" ncl_bioread: type %x unexpected\n", vp->v_type);
		bp = NULL;
		break;
	    };

	    if (n > 0) {
		    error = uiomove(bp->b_data + on, (int)n, uio);
	    }
	    if (vp->v_type == VLNK)
		n = 0;
	    if (bp != NULL)
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n > 0);
	return (error);
}

/*
 * The NFS write path cannot handle iovecs with len > 1. So we need to 
 * break up iovecs accordingly (restricting them to wsize).
 * For the SYNC case, we can do this with 1 copy (user buffer -> mbuf). 
 * For the ASYNC case, 2 copies are needed. The first a copy from the 
 * user buffer to a staging buffer and then a second copy from the staging
 * buffer to mbufs. This can be optimized by copying from the user buffer
 * directly into mbufs and passing the chain down, but that requires a 
 * fair amount of re-working of the relevant codepaths (and can be done
 * later).
 */
static int
nfs_directio_write(vp, uiop, cred, ioflag)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	int ioflag;
{
	int error;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct thread *td = uiop->uio_td;
	int size;
	int wsize;
	
	mtx_lock(&nmp->nm_mtx);
	wsize = nmp->nm_wsize;
	mtx_unlock(&nmp->nm_mtx);
	if (ioflag & IO_SYNC) {
		int iomode, must_commit;
		struct uio uio;
		struct iovec iov;
do_sync:
		while (uiop->uio_resid > 0) {
			size = min(uiop->uio_resid, wsize);
			size = min(uiop->uio_iov->iov_len, size);
			iov.iov_base = uiop->uio_iov->iov_base;
			iov.iov_len = size;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_offset = uiop->uio_offset;
			uio.uio_resid = size;
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw = UIO_WRITE;
			uio.uio_td = td;
			iomode = NFSWRITE_FILESYNC;
			error = ncl_writerpc(vp, &uio, cred, &iomode,
			    &must_commit, 0);
			KASSERT((must_commit == 0), 
				("ncl_directio_write: Did not commit write"));
			if (error)
				return (error);
			uiop->uio_offset += size;
			uiop->uio_resid -= size;
			if (uiop->uio_iov->iov_len <= size) {
				uiop->uio_iovcnt--;
				uiop->uio_iov++;
			} else {
				uiop->uio_iov->iov_base = 
					(char *)uiop->uio_iov->iov_base + size;
				uiop->uio_iov->iov_len -= size;
			}
		}
	} else {
		struct uio *t_uio;
		struct iovec *t_iov;
		struct buf *bp;
		
		/*
		 * Break up the write into blocksize chunks and hand these
		 * over to nfsiod's for write back.
		 * Unfortunately, this incurs a copy of the data. Since 
		 * the user could modify the buffer before the write is 
		 * initiated.
		 * 
		 * The obvious optimization here is that one of the 2 copies
		 * in the async write path can be eliminated by copying the
		 * data here directly into mbufs and passing the mbuf chain
		 * down. But that will require a fair amount of re-working
		 * of the code and can be done if there's enough interest
		 * in NFS directio access.
		 */
		while (uiop->uio_resid > 0) {
			size = min(uiop->uio_resid, wsize);
			size = min(uiop->uio_iov->iov_len, size);
			bp = getpbuf(&ncl_pbuf_freecnt);
			t_uio = malloc(sizeof(struct uio), M_NFSDIRECTIO, M_WAITOK);
			t_iov = malloc(sizeof(struct iovec), M_NFSDIRECTIO, M_WAITOK);
			t_iov->iov_base = malloc(size, M_NFSDIRECTIO, M_WAITOK);
			t_iov->iov_len = size;
			t_uio->uio_iov = t_iov;
			t_uio->uio_iovcnt = 1;
			t_uio->uio_offset = uiop->uio_offset;
			t_uio->uio_resid = size;
			t_uio->uio_segflg = UIO_SYSSPACE;
			t_uio->uio_rw = UIO_WRITE;
			t_uio->uio_td = td;
			bcopy(uiop->uio_iov->iov_base, t_iov->iov_base, size);
			bp->b_flags |= B_DIRECT;
			bp->b_iocmd = BIO_WRITE;
			if (cred != NOCRED) {
				crhold(cred);
				bp->b_wcred = cred;
			} else 
				bp->b_wcred = NOCRED;			
			bp->b_caller1 = (void *)t_uio;
			bp->b_vp = vp;
			error = ncl_asyncio(nmp, bp, NOCRED, td);
			if (error) {
				free(t_iov->iov_base, M_NFSDIRECTIO);
				free(t_iov, M_NFSDIRECTIO);
				free(t_uio, M_NFSDIRECTIO);
				bp->b_vp = NULL;
				relpbuf(bp, &ncl_pbuf_freecnt);
				if (error == EINTR)
					return (error);
				goto do_sync;
			}
			uiop->uio_offset += size;
			uiop->uio_resid -= size;
			if (uiop->uio_iov->iov_len <= size) {
				uiop->uio_iovcnt--;
				uiop->uio_iov++;
			} else {
				uiop->uio_iov->iov_base = 
					(char *)uiop->uio_iov->iov_base + size;
				uiop->uio_iov->iov_len -= size;
			}
		}
	}
	return (0);
}

/*
 * Vnode op for write using bio
 */
int
ncl_write(struct vop_write_args *ap)
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
	struct proc *p = td?td->td_proc:NULL;

	KASSERT(uio->uio_rw == UIO_WRITE, ("ncl_write mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("ncl_write proc"));
	if (vp->v_type != VREG)
		return (EIO);
	mtx_lock(&np->n_mtx);
	if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		mtx_unlock(&np->n_mtx);
		return (np->n_error);
	} else
		mtx_unlock(&np->n_mtx);
	mtx_lock(&nmp->nm_mtx);
	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {
		mtx_unlock(&nmp->nm_mtx);
		(void)ncl_fsinfo(nmp, vp, cred, td);
		mtx_lock(&nmp->nm_mtx);
	}
	if (nmp->nm_wsize == 0)
		(void) newnfs_iosize(nmp);
	mtx_unlock(&nmp->nm_mtx);

	/*
	 * Synchronously flush pending buffers if we are in synchronous
	 * mode or if we are appending.
	 */
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		mtx_lock(&np->n_mtx);
		if (np->n_flag & NMODIFIED) {
			mtx_unlock(&np->n_mtx);
#ifdef notyet /* Needs matching nonblock semantics elsewhere, too. */
			/*
			 * Require non-blocking, synchronous writes to
			 * dirty files to inform the program it needs
			 * to fsync(2) explicitly.
			 */
			if (ioflag & IO_NDELAY)
				return (EAGAIN);
#endif
flush_and_restart:
			np->n_attrstamp = 0;
			error = ncl_vinvalbuf(vp, V_SAVE, td, 1);
			if (error)
				return (error);
		} else
			mtx_unlock(&np->n_mtx);
	}

	/*
	 * If IO_APPEND then load uio_offset.  We restart here if we cannot
	 * get the append lock.
	 */
	if (ioflag & IO_APPEND) {
		np->n_attrstamp = 0;
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			return (error);
		mtx_lock(&np->n_mtx);
		uio->uio_offset = np->n_size;
		mtx_unlock(&np->n_mtx);
	}

	if (uio->uio_offset < 0)
		return (EINVAL);
	if ((uio->uio_offset + uio->uio_resid) > nmp->nm_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	if (newnfs_directio_enable && (ioflag & IO_DIRECT) && vp->v_type == VREG)
		return nfs_directio_write(vp, uio, cred, ioflag);

	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	if (p != NULL) {
		PROC_LOCK(p);
		if (uio->uio_offset + uio->uio_resid >
		    lim_cur(p, RLIMIT_FSIZE)) {
			psignal(p, SIGXFSZ);
			PROC_UNLOCK(p);
			return (EFBIG);
		}
		PROC_UNLOCK(p);
	}

	biosize = vp->v_mount->mnt_stat.f_iosize;
	/*
	 * Find all of this file's B_NEEDCOMMIT buffers.  If our writes
	 * would exceed the local maximum per-file write commit size when
	 * combined with those, we must decide whether to flush,
	 * go synchronous, or return error.  We don't bother checking
	 * IO_UNIT -- we just make all writes atomic anyway, as there's
	 * no point optimizing for something that really won't ever happen.
	 */
	if (!(ioflag & IO_SYNC)) {
		int nflag;

		mtx_lock(&np->n_mtx);
		nflag = np->n_flag;
		mtx_unlock(&np->n_mtx);		
		int needrestart = 0;
		if (nmp->nm_wcommitsize < uio->uio_resid) {
			/*
			 * If this request could not possibly be completed
			 * without exceeding the maximum outstanding write
			 * commit size, see if we can convert it into a
			 * synchronous write operation.
			 */
			if (ioflag & IO_NDELAY)
				return (EAGAIN);
			ioflag |= IO_SYNC;
			if (nflag & NMODIFIED)
				needrestart = 1;
		} else if (nflag & NMODIFIED) {
			int wouldcommit = 0;
			BO_LOCK(&vp->v_bufobj);
			if (vp->v_bufobj.bo_dirty.bv_cnt != 0) {
				TAILQ_FOREACH(bp, &vp->v_bufobj.bo_dirty.bv_hd,
				    b_bobufs) {
					if (bp->b_flags & B_NEEDCOMMIT)
						wouldcommit += bp->b_bcount;
				}
			}
			BO_UNLOCK(&vp->v_bufobj);
			/*
			 * Since we're not operating synchronously and
			 * bypassing the buffer cache, we are in a commit
			 * and holding all of these buffers whether
			 * transmitted or not.  If not limited, this
			 * will lead to the buffer cache deadlocking,
			 * as no one else can flush our uncommitted buffers.
			 */
			wouldcommit += uio->uio_resid;
			/*
			 * If we would initially exceed the maximum
			 * outstanding write commit size, flush and restart.
			 */
			if (wouldcommit > nmp->nm_wcommitsize)
				needrestart = 1;
		}
		if (needrestart)
			goto flush_and_restart;
	}

	do {
		NFSINCRGLOBAL(newnfsstats.biocache_writes);
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize-1);
		n = min((unsigned)(biosize - on), uio->uio_resid);
again:
		/*
		 * Handle direct append and file extension cases, calculate
		 * unaligned buffer size.
		 */
		mtx_lock(&np->n_mtx);
		if (uio->uio_offset == np->n_size && n) {
			mtx_unlock(&np->n_mtx);
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

				mtx_lock(&np->n_mtx);
				np->n_size = uio->uio_offset + n;
				np->n_flag |= NMODIFIED;
				vnode_pager_setsize(vp, np->n_size);
				mtx_unlock(&np->n_mtx);

				save = bp->b_flags & B_CACHE;
				bcount += n;
				allocbuf(bp, bcount);
				bp->b_flags |= save;
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
			mtx_unlock(&np->n_mtx);
			bp = nfs_getcacheblk(vp, lbn, bcount, td);
			mtx_lock(&np->n_mtx);
			if (uio->uio_offset + n > np->n_size) {
				np->n_size = uio->uio_offset + n;
				np->n_flag |= NMODIFIED;
				vnode_pager_setsize(vp, np->n_size);
			}
			mtx_unlock(&np->n_mtx);
		}

		if (!bp) {
			error = newnfs_sigintr(nmp, td);
			if (!error)
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
			error = ncl_doio(vp, bp, cred, td, 0);
			if (error) {
				brelse(bp);
				break;
			}
		}
		if (bp->b_wcred == NOCRED)
			bp->b_wcred = crhold(cred);
		mtx_lock(&np->n_mtx);
		np->n_flag |= NMODIFIED;
		mtx_unlock(&np->n_mtx);

		/*
		 * If dirtyend exceeds file size, chop it down.  This should
		 * not normally occur but there is an append race where it
		 * might occur XXX, so we log it.
		 *
		 * If the chopping creates a reverse-indexed or degenerate
		 * situation with dirtyoff/end, we 0 both of them.
		 */

		if (bp->b_dirtyend > bcount) {
			ncl_printf("NFS append race @%lx:%d\n",
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
			if (bwrite(bp) == EINTR) {
				error = EINTR;
				break;
			}
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
			vfs_bio_set_valid(bp, on, n);
		}

		/*
		 * If IO_SYNC do bwrite().
		 *
		 * IO_INVAL appears to be unused.  The idea appears to be
		 * to turn off caching in this case.  Very odd.  XXX
		 */
		if ((ioflag & IO_SYNC)) {
			if (ioflag & IO_INVAL)
				bp->b_flags |= B_NOCACHE;
			error = bwrite(bp);
			if (error)
				break;
		} else if ((n + on) == biosize) {
			bp->b_flags |= B_ASYNC;
			(void) ncl_writebp(bp, 0, NULL);
		} else {
			bdwrite(bp);
		}
	} while (uio->uio_resid > 0 && n > 0);

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
 * the buffer.  ncl_doio() clears B_INVAL (and ncl_asyncio() clears it
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
 		sigset_t oldset;

 		newnfs_set_sigmask(td, &oldset);
		bp = getblk(vp, bn, size, NFS_PCATCH, 0, 0);
 		newnfs_restore_sigmask(td, &oldset);
		while (bp == NULL) {
			if (newnfs_sigintr(nmp, td))
				return (NULL);
			bp = getblk(vp, bn, size, 0, 2 * hz, 0);
		}
	} else {
		bp = getblk(vp, bn, size, 0, 0, 0);
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
ncl_vinvalbuf(struct vnode *vp, int flags, struct thread *td, int intrflg)
{
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, slpflag, slptimeo;
 	int old_lock = 0;

	ASSERT_VOP_LOCKED(vp, "ncl_vinvalbuf");

	if ((nmp->nm_flag & NFSMNT_INT) == 0)
		intrflg = 0;
	if ((nmp->nm_mountp->mnt_kern_flag & MNTK_UNMOUNTF))
		intrflg = 1;
	if (intrflg) {
		slpflag = NFS_PCATCH;
		slptimeo = 2 * hz;
	} else {
		slpflag = 0;
		slptimeo = 0;
	}

	old_lock = ncl_upgrade_vnlock(vp);
	if (vp->v_iflag & VI_DOOMED) {
		/*
		 * Since vgonel() uses the generic vinvalbuf() to flush
		 * dirty buffers and it does not call this function, it
		 * is safe to just return OK when VI_DOOMED is set.
		 */
		ncl_downgrade_vnlock(vp, old_lock);
		return (0);
	}

	/*
	 * Now, flush as required.
	 */
	if ((flags & V_SAVE) && (vp->v_bufobj.bo_object != NULL)) {
		VM_OBJECT_LOCK(vp->v_bufobj.bo_object);
		vm_object_page_clean(vp->v_bufobj.bo_object, 0, 0, OBJPC_SYNC);
		VM_OBJECT_UNLOCK(vp->v_bufobj.bo_object);
		/*
		 * If the page clean was interrupted, fail the invalidation.
		 * Not doing so, we run the risk of losing dirty pages in the 
		 * vinvalbuf() call below.
		 */
		if (intrflg && (error = newnfs_sigintr(nmp, td)))
			goto out;
	}

	error = vinvalbuf(vp, flags, slpflag, 0);
	while (error) {
		if (intrflg && (error = newnfs_sigintr(nmp, td)))
			goto out;
		error = vinvalbuf(vp, flags, 0, slptimeo);
	}
	mtx_lock(&np->n_mtx);
	if (np->n_directio_asyncwr == 0)
		np->n_flag &= ~NMODIFIED;
	mtx_unlock(&np->n_mtx);
out:
	ncl_downgrade_vnlock(vp, old_lock);
	return error;
}

/*
 * Initiate asynchronous I/O. Return an error if no nfsiods are available.
 * This is mainly to avoid queueing async I/O requests when the nfsiods
 * are all hung on a dead server.
 *
 * Note: ncl_asyncio() does not clear (BIO_ERROR|B_INVAL) but when the bp
 * is eventually dequeued by the async daemon, ncl_doio() *will*.
 */
int
ncl_asyncio(struct nfsmount *nmp, struct buf *bp, struct ucred *cred, struct thread *td)
{
	int iod;
	int gotiod;
	int slpflag = 0;
	int slptimeo = 0;
	int error, error2;

	/*
	 * Commits are usually short and sweet so lets save some cpu and
	 * leave the async daemons for more important rpc's (such as reads
	 * and writes).
	 */
	mtx_lock(&ncl_iod_mutex);
	if (bp->b_iocmd == BIO_WRITE && (bp->b_flags & B_NEEDCOMMIT) &&
	    (nmp->nm_bufqiods > ncl_numasync / 2)) {
		mtx_unlock(&ncl_iod_mutex);
		return(EIO);
	}
again:
	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = NFS_PCATCH;
	gotiod = FALSE;

	/*
	 * Find a free iod to process this request.
	 */
	for (iod = 0; iod < ncl_numasync; iod++)
		if (ncl_iodwant[iod] == NFSIOD_AVAILABLE) {
			gotiod = TRUE;
			break;
		}

	/*
	 * Try to create one if none are free.
	 */
	if (!gotiod)
		ncl_nfsiodnew();
	else {
		/*
		 * Found one, so wake it up and tell it which
		 * mount to process.
		 */
		NFS_DPF(ASYNCIO, ("ncl_asyncio: waking iod %d for mount %p\n",
		    iod, nmp));
		ncl_iodwant[iod] = NFSIOD_NOT_AVAILABLE;
		ncl_iodmount[iod] = nmp;
		nmp->nm_bufqiods++;
		wakeup(&ncl_iodwant[iod]);
	}

	/*
	 * If none are free, we may already have an iod working on this mount
	 * point.  If so, it will process our request.
	 */
	if (!gotiod) {
		if (nmp->nm_bufqiods > 0) {
			NFS_DPF(ASYNCIO,
				("ncl_asyncio: %d iods are already processing mount %p\n",
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
		while (nmp->nm_bufqlen >= 2*ncl_numasync) {
			NFS_DPF(ASYNCIO,
				("ncl_asyncio: waiting for mount %p queue to drain\n", nmp));
			nmp->nm_bufqwant = TRUE;
 			error = newnfs_msleep(td, &nmp->nm_bufq, 
			    &ncl_iod_mutex, slpflag | PRIBIO, "nfsaio",
  			   slptimeo);
			if (error) {
				error2 = newnfs_sigintr(nmp, td);
				if (error2) {
					mtx_unlock(&ncl_iod_mutex);					
					return (error2);
				}
				if (slpflag == NFS_PCATCH) {
					slpflag = 0;
					slptimeo = 2 * hz;
				}
			}
			/*
			 * We might have lost our iod while sleeping,
			 * so check and loop if nescessary.
			 */
			goto again;
		}

		/* We might have lost our nfsiod */
		if (nmp->nm_bufqiods == 0) {
			NFS_DPF(ASYNCIO,
				("ncl_asyncio: no iods after mount %p queue was drained, looping\n", nmp));
			goto again;
		}

		if (bp->b_iocmd == BIO_READ) {
			if (bp->b_rcred == NOCRED && cred != NOCRED)
				bp->b_rcred = crhold(cred);
		} else {
			if (bp->b_wcred == NOCRED && cred != NOCRED)
				bp->b_wcred = crhold(cred);
		}

		if (bp->b_flags & B_REMFREE)
			bremfreef(bp);
		BUF_KERNPROC(bp);
		TAILQ_INSERT_TAIL(&nmp->nm_bufq, bp, b_freelist);
		nmp->nm_bufqlen++;
		if ((bp->b_flags & B_DIRECT) && bp->b_iocmd == BIO_WRITE) {
			mtx_lock(&(VTONFS(bp->b_vp))->n_mtx);			
			VTONFS(bp->b_vp)->n_flag |= NMODIFIED;
			VTONFS(bp->b_vp)->n_directio_asyncwr++;
			mtx_unlock(&(VTONFS(bp->b_vp))->n_mtx);
		}
		mtx_unlock(&ncl_iod_mutex);
		return (0);
	}

	mtx_unlock(&ncl_iod_mutex);

	/*
	 * All the iods are busy on other mounts, so return EIO to
	 * force the caller to process the i/o synchronously.
	 */
	NFS_DPF(ASYNCIO, ("ncl_asyncio: no iods available, i/o is synchronous\n"));
	return (EIO);
}

void
ncl_doio_directwrite(struct buf *bp)
{
	int iomode, must_commit;
	struct uio *uiop = (struct uio *)bp->b_caller1;
	char *iov_base = uiop->uio_iov->iov_base;
	
	iomode = NFSWRITE_FILESYNC;
	uiop->uio_td = NULL; /* NULL since we're in nfsiod */
	ncl_writerpc(bp->b_vp, uiop, bp->b_wcred, &iomode, &must_commit, 0);
	KASSERT((must_commit == 0), ("ncl_doio_directwrite: Did not commit write"));
	free(iov_base, M_NFSDIRECTIO);
	free(uiop->uio_iov, M_NFSDIRECTIO);
	free(uiop, M_NFSDIRECTIO);
	if ((bp->b_flags & B_DIRECT) && bp->b_iocmd == BIO_WRITE) {
		struct nfsnode *np = VTONFS(bp->b_vp);
		mtx_lock(&np->n_mtx);
		np->n_directio_asyncwr--;
		if (np->n_directio_asyncwr == 0) {
			np->n_flag &= ~NMODIFIED;
			if ((np->n_flag & NFSYNCWAIT)) {
				np->n_flag &= ~NFSYNCWAIT;
				wakeup((caddr_t)&np->n_directio_asyncwr);
			}
		}
		mtx_unlock(&np->n_mtx);
	}
	bp->b_vp = NULL;
	relpbuf(bp, &ncl_pbuf_freecnt);
}

/*
 * Do an I/O operation to/from a cache block. This may be called
 * synchronously or from an nfsiod.
 */
int
ncl_doio(struct vnode *vp, struct buf *bp, struct ucred *cr, struct thread *td,
    int called_from_strategy)
{
	struct uio *uiop;
	struct nfsnode *np;
	struct nfsmount *nmp;
	int error = 0, iomode, must_commit = 0;
	struct uio uio;
	struct iovec io;
	struct proc *p = td ? td->td_proc : NULL;
	uint8_t	iocmd;
	
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

	KASSERT(!(bp->b_flags & B_DONE), ("ncl_doio: bp %p already marked done", bp));
	iocmd = bp->b_iocmd;
	if (iocmd == BIO_READ) {
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    io.iov_base = bp->b_data;
	    uiop->uio_rw = UIO_READ;

	    switch (vp->v_type) {
	    case VREG:
		uiop->uio_offset = ((off_t)bp->b_blkno) * DEV_BSIZE;
		NFSINCRGLOBAL(newnfsstats.read_bios);
		error = ncl_readrpc(vp, uiop, cr);

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
		/* ASSERT_VOP_LOCKED(vp, "ncl_doio"); */
		if (p && (vp->v_vflag & VV_TEXT)) {
			mtx_lock(&np->n_mtx);
			if (NFS_TIMESPEC_COMPARE(&np->n_mtime, &np->n_vattr.na_mtime)) {
				mtx_unlock(&np->n_mtx);
				PROC_LOCK(p);
				killproc(p, "text file modification");
				PROC_UNLOCK(p);
			} else
				mtx_unlock(&np->n_mtx);
		}
		break;
	    case VLNK:
		uiop->uio_offset = (off_t)0;
		NFSINCRGLOBAL(newnfsstats.readlink_bios);
		error = ncl_readlinkrpc(vp, uiop, cr);
		break;
	    case VDIR:
		NFSINCRGLOBAL(newnfsstats.readdir_bios);
		uiop->uio_offset = ((u_quad_t)bp->b_lblkno) * NFS_DIRBLKSIZ;
		if ((nmp->nm_flag & NFSMNT_RDIRPLUS) != 0) {
			error = ncl_readdirplusrpc(vp, uiop, cr, td);
			if (error == NFSERR_NOTSUPP)
				nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
		}
		if ((nmp->nm_flag & NFSMNT_RDIRPLUS) == 0)
			error = ncl_readdirrpc(vp, uiop, cr, td);
		/*
		 * end-of-directory sets B_INVAL but does not generate an
		 * error.
		 */
		if (error == 0 && uiop->uio_resid == bp->b_bcount)
			bp->b_flags |= B_INVAL;
		break;
	    default:
		ncl_printf("ncl_doio:  type %x unexpected\n", vp->v_type);
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
		    retv = ncl_commit(vp, off, bp->b_dirtyend-bp->b_dirtyoff,
			bp->b_wcred, td);
		    if (retv == 0) {
			    bp->b_dirtyoff = bp->b_dirtyend = 0;
			    bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);
			    bp->b_resid = 0;
			    bufdone(bp);
			    return (0);
		    }
		    if (retv == NFSERR_STALEWRITEVERF) {
			    ncl_clearcommit(vp->v_mount);
		    }
	    }

	    /*
	     * Setup for actual write
	     */
	    mtx_lock(&np->n_mtx);
	    if ((off_t)bp->b_blkno * DEV_BSIZE + bp->b_dirtyend > np->n_size)
		bp->b_dirtyend = np->n_size - (off_t)bp->b_blkno * DEV_BSIZE;
	    mtx_unlock(&np->n_mtx);

	    if (bp->b_dirtyend > bp->b_dirtyoff) {
		io.iov_len = uiop->uio_resid = bp->b_dirtyend
		    - bp->b_dirtyoff;
		uiop->uio_offset = (off_t)bp->b_blkno * DEV_BSIZE
		    + bp->b_dirtyoff;
		io.iov_base = (char *)bp->b_data + bp->b_dirtyoff;
		uiop->uio_rw = UIO_WRITE;
		NFSINCRGLOBAL(newnfsstats.write_bios);

		if ((bp->b_flags & (B_ASYNC | B_NEEDCOMMIT | B_NOCACHE | B_CLUSTER)) == B_ASYNC)
		    iomode = NFSWRITE_UNSTABLE;
		else
		    iomode = NFSWRITE_FILESYNC;

		error = ncl_writerpc(vp, uiop, cr, &iomode, &must_commit,
		    called_from_strategy);

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

		if (!error && iomode == NFSWRITE_UNSTABLE) {
		    bp->b_flags |= B_NEEDCOMMIT;
		    if (bp->b_dirtyoff == 0
			&& bp->b_dirtyend == bp->b_bcount)
			bp->b_flags |= B_CLUSTEROK;
		} else {
		    bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);
		}

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
		 * EIO is returned by ncl_writerpc() to indicate a recoverable
		 * write error and is handled as above, except that
		 * B_EINTR isn't set. One cause of this is a stale stateid
		 * error for the RPC that indicates recovery is required,
		 * when called with called_from_strategy != 0.
		 *
		 * If the buffer is marked B_PAGING, it does not reside on
		 * the vp's paging queues so we cannot call bdirty().  The
		 * bp in this case is not an NFS cache block so we should
		 * be safe. XXX
		 *
		 * The logic below breaks up errors into recoverable and 
		 * unrecoverable. For the former, we clear B_INVAL|B_NOCACHE
		 * and keep the buffer around for potential write retries.
		 * For the latter (eg ESTALE), we toss the buffer away (B_INVAL)
		 * and save the error in the nfsnode. This is less than ideal 
		 * but necessary. Keeping such buffers around could potentially
		 * cause buffer exhaustion eventually (they can never be written
		 * out, so will get constantly be re-dirtied). It also causes
		 * all sorts of vfs panics. For non-recoverable write errors, 
		 * also invalidate the attrcache, so we'll be forced to go over
		 * the wire for this object, returning an error to user on next
		 * call (most of the time).
		 */
    		if (error == EINTR || error == EIO || error == ETIMEDOUT
		    || (!error && (bp->b_flags & B_NEEDCOMMIT))) {
			int s;

			s = splbio();
			bp->b_flags &= ~(B_INVAL|B_NOCACHE);
			if ((bp->b_flags & B_PAGING) == 0) {
			    bdirty(bp);
			    bp->b_flags &= ~B_DONE;
			}
			if ((error == EINTR || error == ETIMEDOUT) &&
			    (bp->b_flags & B_ASYNC) == 0)
			    bp->b_flags |= B_EINTR;
			splx(s);
	    	} else {
		    if (error) {
			bp->b_ioflags |= BIO_ERROR;
			bp->b_flags |= B_INVAL;
			bp->b_error = np->n_error = error;
			mtx_lock(&np->n_mtx);
			np->n_flag |= NWRITEERR;
			np->n_attrstamp = 0;
			mtx_unlock(&np->n_mtx);
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
	    ncl_clearcommit(vp->v_mount);
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
ncl_meta_setsize(struct vnode *vp, struct ucred *cred, struct thread *td, u_quad_t nsize)
{
	struct nfsnode *np = VTONFS(vp);
	u_quad_t tsize;
	int biosize = vp->v_mount->mnt_stat.f_iosize;
	int error = 0;

	mtx_lock(&np->n_mtx);
	tsize = np->n_size;
	np->n_size = nsize;
	mtx_unlock(&np->n_mtx);

	if (nsize < tsize) {
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
 		if (!bp)
 			return EINTR;
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

