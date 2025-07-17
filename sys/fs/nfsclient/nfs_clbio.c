/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/rwlock.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include <fs/nfs/nfsport.h>
#include <fs/nfsclient/nfsmount.h>
#include <fs/nfsclient/nfs.h>
#include <fs/nfsclient/nfsnode.h>
#include <fs/nfsclient/nfs_kdtrace.h>

extern int newnfs_directio_allow_mmap;
extern struct nfsstatsv1 nfsstatsv1;
extern struct mtx ncl_iod_mutex;
extern int ncl_numasync;
extern enum nfsiod_state ncl_iodwant[NFS_MAXASYNCDAEMON];
extern struct nfsmount *ncl_iodmount[NFS_MAXASYNCDAEMON];
extern int newnfs_directio_enable;
extern int nfs_keep_dirty_on_error;

uma_zone_t ncl_pbuf_zone;

static struct buf *nfs_getcacheblk(struct vnode *vp, daddr_t bn, int size,
    struct thread *td);
static int nfs_directio_write(struct vnode *vp, struct uio *uiop,
    struct ucred *cred, int ioflag);

/*
 * Vnode op for VM getpages.
 */
SYSCTL_DECL(_vfs_nfs);
static int use_buf_pager = 1;
SYSCTL_INT(_vfs_nfs, OID_AUTO, use_buf_pager, CTLFLAG_RWTUN,
    &use_buf_pager, 0,
    "Use buffer pager instead of direct readrpc call");

static daddr_t
ncl_gbp_getblkno(struct vnode *vp, vm_ooffset_t off)
{

	return (off / vp->v_bufobj.bo_bsize);
}

static int
ncl_gbp_getblksz(struct vnode *vp, daddr_t lbn, long *sz)
{
	struct nfsnode *np;
	u_quad_t nsize;
	int biosize, bcount;

	np = VTONFS(vp);
	NFSLOCKNODE(np);
	nsize = np->n_size;
	NFSUNLOCKNODE(np);

	biosize = vp->v_bufobj.bo_bsize;
	bcount = biosize;
	if ((off_t)lbn * biosize >= nsize)
		bcount = 0;
	else if ((off_t)(lbn + 1) * biosize > nsize)
		bcount = nsize - (off_t)lbn * biosize;
	*sz = bcount;
	return (0);
}

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
	td = curthread;
	cred = curthread->td_ucred;
	nmp = VFSTONFS(vp->v_mount);
	pages = ap->a_m;
	npages = ap->a_count;

	if ((object = vp->v_object) == NULL) {
		printf("ncl_getpages: called with non-merged cache vnode\n");
		return (VM_PAGER_ERROR);
	}

	if (newnfs_directio_enable && !newnfs_directio_allow_mmap) {
		NFSLOCKNODE(np);
		if ((np->n_flag & NNONCACHE) && (vp->v_type == VREG)) {
			NFSUNLOCKNODE(np);
			printf("ncl_getpages: called on non-cacheable vnode\n");
			return (VM_PAGER_ERROR);
		} else
			NFSUNLOCKNODE(np);
	}

	mtx_lock(&nmp->nm_mtx);
	if ((nmp->nm_flag & NFSMNT_NFSV3) != 0 &&
	    (nmp->nm_state & NFSSTA_GOTFSINFO) == 0) {
		mtx_unlock(&nmp->nm_mtx);
		/* We'll never get here for v4, because we always have fsinfo */
		(void)ncl_fsinfo(nmp, vp, cred, td);
	} else
		mtx_unlock(&nmp->nm_mtx);

	if (use_buf_pager)
		return (vfs_bio_getpages(vp, pages, npages, ap->a_rbehind,
		    ap->a_rahead, ncl_gbp_getblkno, ncl_gbp_getblksz));

	/*
	 * If the requested page is partially valid, just return it and
	 * allow the pager to zero-out the blanks.  Partially valid pages
	 * can only occur at the file EOF.
	 *
	 * XXXGL: is that true for NFS, where short read can occur???
	 */
	VM_OBJECT_WLOCK(object);
	if (!vm_page_none_valid(pages[npages - 1]) && --npages == 0)
		goto out;
	VM_OBJECT_WUNLOCK(object);

	/*
	 * We use only the kva address for the buffer, but this is extremely
	 * convenient and fast.
	 */
	bp = uma_zalloc(ncl_pbuf_zone, M_WAITOK);

	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);
	VM_CNT_INC(v_vnodein);
	VM_CNT_ADD(v_vnodepgsin, npages);

	count = npages << PAGE_SHIFT;
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

	uma_zfree(ncl_pbuf_zone, bp);

	if (error && (uio.uio_resid == count)) {
		printf("ncl_getpages: error %d\n", error);
		return (VM_PAGER_ERROR);
	}

	/*
	 * Calculate the number of bytes read and validate only that number
	 * of bytes.  Note that due to pending writes, size may be 0.  This
	 * does not mean that the remaining data is invalid!
	 */

	size = count - uio.uio_resid;
	VM_OBJECT_WLOCK(object);
	for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
		vm_page_t m;
		nextoff = toff + PAGE_SIZE;
		m = pages[i];

		if (nextoff <= size) {
			/*
			 * Read operation filled an entire page
			 */
			vm_page_valid(m);
			KASSERT(m->dirty == 0,
			    ("nfs_getpages: page %p is dirty", m));
		} else if (size > toff) {
			/*
			 * Read operation filled a partial page.
			 */
			vm_page_invalid(m);
			vm_page_set_valid_range(m, 0, size - toff);
			KASSERT(m->dirty == 0,
			    ("nfs_getpages: page %p is dirty", m));
		} else {
			/*
			 * Read operation was short.  If no error
			 * occurred we may have hit a zero-fill
			 * section.  We leave valid set to 0, and page
			 * is freed by vm_page_readahead_finish() if
			 * its index is not equal to requested, or
			 * page is zeroed and set valid by
			 * vm_pager_get_pages() for requested page.
			 */
			;
		}
	}
out:
	VM_OBJECT_WUNLOCK(object);
	if (ap->a_rbehind)
		*ap->a_rbehind = 0;
	if (ap->a_rahead)
		*ap->a_rahead = 0;
	return (VM_PAGER_OK);
}

/*
 * Vnode op for VM putpages.
 */
int
ncl_putpages(struct vop_putpages_args *ap)
{
	struct uio uio;
	struct iovec iov;
	int i, error, npages, count;
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
	/* Set the cred to n_writecred for the write rpcs. */
	if (np->n_writecred != NULL)
		cred = crhold(np->n_writecred);
	else
		cred = crhold(curthread->td_ucred);	/* XXX */
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

	NFSLOCKNODE(np);
	if (newnfs_directio_enable && !newnfs_directio_allow_mmap &&
	    (np->n_flag & NNONCACHE) && (vp->v_type == VREG)) {
		NFSUNLOCKNODE(np);
		printf("ncl_putpages: called on noncache-able vnode\n");
		NFSLOCKNODE(np);
	}
	/*
	 * When putting pages, do not extend file past EOF.
	 */
	if (offset + count > np->n_size) {
		count = np->n_size - offset;
		if (count < 0)
			count = 0;
	}
	NFSUNLOCKNODE(np);

	for (i = 0; i < npages; i++)
		rtvals[i] = VM_PAGER_ERROR;

	VM_CNT_INC(v_vnodeout);
	VM_CNT_ADD(v_vnodepgsout, count);

	iov.iov_base = unmapped_buf;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_resid = count;
	uio.uio_segflg = UIO_NOCOPY;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;

	error = VOP_WRITE(vp, &uio, vnode_pager_putpages_ioflags(ap->a_sync),
	    cred);
	crfree(cred);

	if (error == 0 || !nfs_keep_dirty_on_error) {
		vnode_pager_undirty_pages(pages, rtvals, count - uio.uio_resid,
		    np->n_size - offset, npages * PAGE_SIZE);
	}
	return (rtvals[0]);
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
	bool old_lock;

	/*
	 * Ensure the exclusive access to the node before checking
	 * whether the cache is consistent.
	 */
	old_lock = ncl_excl_start(vp);
	NFSLOCKNODE(np);
	if (np->n_flag & NMODIFIED) {
		NFSUNLOCKNODE(np);
		if (vp->v_type != VREG) {
			if (vp->v_type != VDIR)
				panic("nfs: bioread, not dir");
			ncl_invaldir(vp);
			error = ncl_vinvalbuf(vp, V_SAVE | V_ALLOWCLEAN, td, 1);
			if (error != 0)
				goto out;
		}
		np->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			goto out;
		NFSLOCKNODE(np);
		np->n_mtime = vattr.va_mtime;
		NFSUNLOCKNODE(np);
	} else {
		NFSUNLOCKNODE(np);
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			goto out;
		NFSLOCKNODE(np);
		if ((np->n_flag & NSIZECHANGED)
		    || (NFS_TIMESPEC_COMPARE(&np->n_mtime, &vattr.va_mtime))) {
			NFSUNLOCKNODE(np);
			if (vp->v_type == VDIR)
				ncl_invaldir(vp);
			error = ncl_vinvalbuf(vp, V_SAVE | V_ALLOWCLEAN, td, 1);
			if (error != 0)
				goto out;
			NFSLOCKNODE(np);
			np->n_mtime = vattr.va_mtime;
			np->n_flag &= ~NSIZECHANGED;
		}
		NFSUNLOCKNODE(np);
	}
out:
	ncl_excl_finish(vp, old_lock);
	return (error);
}

static bool
ncl_bioread_dora(struct vnode *vp)
{
	vm_object_t obj;

	obj = vp->v_object;
	if (obj == NULL)
		return (true);
	return (!vm_object_mightbedirty(vp->v_object) &&
	    vp->v_object->un_pager.vnp.writemappings == 0);
}

/*
 * Vnode op for read using bio
 */
int
ncl_bioread(struct vnode *vp, struct uio *uio, int ioflag, struct ucred *cred)
{
	struct nfsnode *np = VTONFS(vp);
	struct buf *bp, *rabp;
	struct thread *td;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	daddr_t lbn, rabn;
	int biosize, bcount, error, i, n, nra, on, save2, seqcount;
	off_t tmp_off;

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

	tmp_off = uio->uio_offset + uio->uio_resid;
	if (vp->v_type != VDIR &&
	    (tmp_off > nmp->nm_maxfilesize || tmp_off < uio->uio_offset)) {
		mtx_unlock(&nmp->nm_mtx);
		return (EFBIG);
	}
	mtx_unlock(&nmp->nm_mtx);

	if (newnfs_directio_enable && (ioflag & IO_DIRECT) && (vp->v_type == VREG))
		/* No caching/ no readaheads. Just read data into the user buffer */
		return ncl_readrpc(vp, uio, cred);

	n = 0;
	on = 0;
	biosize = vp->v_bufobj.bo_bsize;
	seqcount = (int)((off_t)(ioflag >> IO_SEQSHIFT) * biosize / BKVASIZE);

	error = nfs_bioread_check_cons(vp, td, cred);
	if (error)
		return error;

	save2 = curthread_pflags2_set(TDP2_SBPAGES);
	do {
	    u_quad_t nsize;

	    NFSLOCKNODE(np);
	    nsize = np->n_size;
	    NFSUNLOCKNODE(np);

	    switch (vp->v_type) {
	    case VREG:
		NFSINCRGLOBAL(nfsstatsv1.biocache_reads);
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset - (lbn * biosize);

		/*
		 * Start the read ahead(s), as required.  Do not do
		 * read-ahead if there are writeable mappings, since
		 * unlocked read by nfsiod could obliterate changes
		 * done by userspace.
		 */
		if (nmp->nm_readahead > 0 && ncl_bioread_dora(vp)) {
		    for (nra = 0; nra < nmp->nm_readahead && nra < seqcount &&
			(off_t)(lbn + 1 + nra) * biosize < nsize; nra++) {
			rabn = lbn + 1 + nra;
			if (incore(&vp->v_bufobj, rabn) == NULL) {
			    rabp = nfs_getcacheblk(vp, rabn, biosize, td);
			    if (!rabp) {
				error = newnfs_sigintr(nmp, td);
				if (error == 0)
					error = EINTR;
				goto out;
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
			if (error == 0)
				error = EINTR;
			goto out;
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
			goto out;
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
			n = MIN((unsigned)(bcount - on), uio->uio_resid);
		break;
	    case VLNK:
		NFSINCRGLOBAL(nfsstatsv1.biocache_readlinks);
		bp = nfs_getcacheblk(vp, (daddr_t)0, NFS_MAXPATHLEN, td);
		if (!bp) {
			error = newnfs_sigintr(nmp, td);
			if (error == 0)
				error = EINTR;
			goto out;
		}
		if ((bp->b_flags & B_CACHE) == 0) {
		    bp->b_iocmd = BIO_READ;
		    vfs_busy_pages(bp, 0);
		    error = ncl_doio(vp, bp, cred, td, 0);
		    if (error) {
			bp->b_ioflags |= BIO_ERROR;
			brelse(bp);
			goto out;
		    }
		}
		n = MIN(uio->uio_resid, NFS_MAXPATHLEN - bp->b_resid);
		on = 0;
		break;
	    case VDIR:
		NFSINCRGLOBAL(nfsstatsv1.biocache_readdirs);
		NFSLOCKNODE(np);
		if (np->n_direofoffset
		    && uio->uio_offset >= np->n_direofoffset) {
			NFSUNLOCKNODE(np);
			error = 0;
			goto out;
		}
		NFSUNLOCKNODE(np);
		lbn = (uoff_t)uio->uio_offset / NFS_DIRBLKSIZ;
		on = uio->uio_offset & (NFS_DIRBLKSIZ - 1);
		bp = nfs_getcacheblk(vp, lbn, NFS_DIRBLKSIZ, td);
		if (!bp) {
			error = newnfs_sigintr(nmp, td);
			if (error == 0)
				error = EINTR;
			goto out;
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
			    NFSLOCKNODE(np);
			    if (np->n_direofoffset
				&& (i * NFS_DIRBLKSIZ) >= np->n_direofoffset) {
				    NFSUNLOCKNODE(np);
				    error = 0;
				    goto out;
			    }
			    NFSUNLOCKNODE(np);
			    bp = nfs_getcacheblk(vp, i, NFS_DIRBLKSIZ, td);
			    if (!bp) {
				error = newnfs_sigintr(nmp, td);
				if (error == 0)
					error = EINTR;
				goto out;
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
			    goto out;
		}

		/*
		 * If not eof and read aheads are enabled, start one.
		 * (You need the current block first, so that you have the
		 *  directory offset cookie of the next block.)
		 */
		NFSLOCKNODE(np);
		if (nmp->nm_readahead > 0 && ncl_bioread_dora(vp) &&
		    (bp->b_flags & B_INVAL) == 0 &&
		    (np->n_direofoffset == 0 ||
		    (lbn + 1) * NFS_DIRBLKSIZ < np->n_direofoffset) &&
		    incore(&vp->v_bufobj, lbn + 1) == NULL) {
			NFSUNLOCKNODE(np);
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
			NFSLOCKNODE(np);
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
		NFSUNLOCKNODE(np);
		break;
	    default:
		printf(" ncl_bioread: type %x unexpected\n", vp->v_type);
		bp = NULL;
		break;
	    }

	    if (n > 0) {
		    error = vn_io_fault_uiomove(bp->b_data + on, (int)n, uio);
	    }
	    if (vp->v_type == VLNK)
		n = 0;
	    if (bp != NULL)
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n > 0);
out:
	curthread_pflags2_restore(save2);
	if ((curthread->td_pflags2 & TDP2_SBPAGES) == 0) {
		NFSLOCKNODE(np);
		ncl_pager_setsize(vp, NULL);
	}
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
nfs_directio_write(struct vnode *vp, struct uio *uiop, struct ucred *cred,
    int ioflag)
{
	struct uio uio;
	struct iovec iov;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct thread *td = uiop->uio_td;
	int error, iomode, must_commit, size, wsize;

	KASSERT((ioflag & IO_SYNC) != 0, ("nfs_directio_write: not sync"));
	mtx_lock(&nmp->nm_mtx);
	wsize = nmp->nm_wsize;
	mtx_unlock(&nmp->nm_mtx);
	while (uiop->uio_resid > 0) {
		size = MIN(uiop->uio_resid, wsize);
		size = MIN(uiop->uio_iov->iov_len, size);
		iov.iov_base = uiop->uio_iov->iov_base;
		iov.iov_len = size;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = uiop->uio_offset;
		uio.uio_resid = size;
		uio.uio_segflg = uiop->uio_segflg;
		uio.uio_rw = UIO_WRITE;
		uio.uio_td = td;
		iomode = NFSWRITE_FILESYNC;
		/*
		 * When doing direct I/O we do not care if the
		 * server's write verifier has changed, but we
		 * do not want to update the verifier if it has
		 * changed, since that hides the change from
		 * writes being done through the buffer cache.
		 * By passing must_commit in set to two, the code
		 * in nfsrpc_writerpc() will not update the
		 * verifier on the mount point.
		 */
		must_commit = 2;
		error = ncl_writerpc(vp, &uio, cred, &iomode,
		    &must_commit, 0, ioflag);
		KASSERT(must_commit == 2,
		    ("ncl_directio_write: Updated write verifier"));
		if (error != 0)
			return (error);
		if (iomode != NFSWRITE_FILESYNC)
			printf("nfs_directio_write: Broken server "
			    "did not reply FILE_SYNC\n");
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
	int bcount, noncontig_write, obcount;
	int bp_cached, n, on, error = 0, error1, save2, wouldcommit;
	size_t orig_resid, local_resid;
	off_t orig_size, tmp_off;
	struct timespec ts;

	KASSERT(uio->uio_rw == UIO_WRITE, ("ncl_write mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("ncl_write proc"));
	if (vp->v_type != VREG)
		return (EIO);
	NFSLOCKNODE(np);
	if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		NFSUNLOCKNODE(np);
		return (np->n_error);
	} else
		NFSUNLOCKNODE(np);
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
	if ((ioflag & IO_APPEND) || ((ioflag & IO_SYNC) && (np->n_flag &
	    NMODIFIED))) {
		/*
		 * For the case where IO_APPEND is being done using a
		 * direct output (to the NFS server) RPC and
		 * newnfs_directio_enable is 0, all buffer cache buffers,
		 * including ones not modified, must be invalidated.
		 * This ensures that stale data is not read out of the
		 * buffer cache.  The call also invalidates all mapped
		 * pages and, since the exclusive lock is held on the vnode,
		 * new pages cannot be faulted in.
		 *
		 * For the case where newnfs_directio_enable is set
		 * (which is not the default), it is not obvious that
		 * stale data should be left in the buffer cache, but
		 * the code has been this way for over a decade without
		 * complaints.  Note that, unlike doing IO_APPEND via
		 * a direct write RPC when newnfs_directio_enable is not set,
		 * when newnfs_directio_enable is set, reading is done via
		 * direct to NFS server RPCs as well.
		 */
		np->n_attrstamp = 0;
		KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
		error = ncl_vinvalbuf(vp, V_SAVE | ((ioflag &
		    IO_VMIO) != 0 ? V_VMIO : 0), td, 1);
		if (error != 0)
			return (error);
	}

	orig_resid = uio->uio_resid;
	NFSLOCKNODE(np);
	orig_size = np->n_size;
	NFSUNLOCKNODE(np);

	/*
	 * If IO_APPEND then load uio_offset.  We restart here if we cannot
	 * get the append lock.
	 */
	if (ioflag & IO_APPEND) {
		/*
		 * For NFSv4, the AppendWrite will Verify the size against
		 * the file's size on the server.  If not the same, the
		 * write will then be retried, using the file size returned
		 * by the AppendWrite.  However, for NFSv2 and NFSv3, the
		 * size must be acquired here via a Getattr RPC.
		 * The AppendWrite is not done for a pNFS mount.
		 */
		if (!NFSHASNFSV4(nmp) || NFSHASPNFS(nmp)) {
			np->n_attrstamp = 0;
			KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
			error = VOP_GETATTR(vp, &vattr, cred);
			if (error)
				return (error);
		}
		NFSLOCKNODE(np);
		uio->uio_offset = np->n_size;
		NFSUNLOCKNODE(np);
	}

	if (uio->uio_offset < 0)
		return (EINVAL);
	tmp_off = uio->uio_offset + uio->uio_resid;
	if (tmp_off > nmp->nm_maxfilesize || tmp_off < uio->uio_offset)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	/*
	 * Do IO_APPEND writing via a synchronous direct write.
	 * This can result in a significant performance improvement.
	 */
	if ((newnfs_directio_enable && (ioflag & IO_DIRECT)) ||
	    (ioflag & IO_APPEND)) {
		/*
		 * Direct writes to the server must be done NFSWRITE_FILESYNC,
		 * because the write data is not cached and, therefore, the
		 * write cannot be redone after a server reboot.
		 * Set IO_SYNC to make this happen.
		 */
		ioflag |= IO_SYNC;
		return (nfs_directio_write(vp, uio, cred, ioflag));
	}

	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	error = vn_rlimit_fsize(vp, uio, td);
	if (error != 0)
		return (error);

	save2 = curthread_pflags2_set(TDP2_SBPAGES);
	biosize = vp->v_bufobj.bo_bsize;
	/*
	 * Find all of this file's B_NEEDCOMMIT buffers.  If our writes
	 * would exceed the local maximum per-file write commit size when
	 * combined with those, we must decide whether to flush,
	 * go synchronous, or return error.  We don't bother checking
	 * IO_UNIT -- we just make all writes atomic anyway, as there's
	 * no point optimizing for something that really won't ever happen.
	 */
	wouldcommit = 0;
	if (!(ioflag & IO_SYNC)) {
		int nflag;

		NFSLOCKNODE(np);
		nflag = np->n_flag;
		NFSUNLOCKNODE(np);
		if (nflag & NMODIFIED) {
			BO_LOCK(&vp->v_bufobj);
			if (vp->v_bufobj.bo_dirty.bv_cnt != 0) {
				TAILQ_FOREACH(bp, &vp->v_bufobj.bo_dirty.bv_hd,
				    b_bobufs) {
					if (bp->b_flags & B_NEEDCOMMIT)
						wouldcommit += bp->b_bcount;
				}
			}
			BO_UNLOCK(&vp->v_bufobj);
		}
	}

	do {
		if (!(ioflag & IO_SYNC)) {
			wouldcommit += biosize;
			if (wouldcommit > nmp->nm_wcommitsize) {
				np->n_attrstamp = 0;
				KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
				error = ncl_vinvalbuf(vp, V_SAVE | ((ioflag &
				    IO_VMIO) != 0 ? V_VMIO : 0), td, 1);
				if (error != 0)
					goto out;
				wouldcommit = biosize;
			}
		}

		NFSINCRGLOBAL(nfsstatsv1.biocache_writes);
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset - (lbn * biosize);
		n = MIN((unsigned)(biosize - on), uio->uio_resid);
again:
		/*
		 * Handle direct append and file extension cases, calculate
		 * unaligned buffer size.
		 */
		NFSLOCKNODE(np);
		if ((np->n_flag & NHASBEENLOCKED) == 0 &&
		    (nmp->nm_flag & NFSMNT_NONCONTIGWR) != 0)
			noncontig_write = 1;
		else
			noncontig_write = 0;
		if ((uio->uio_offset == np->n_size ||
		    (noncontig_write != 0 &&
		    lbn == (np->n_size / biosize) &&
		    uio->uio_offset + n > np->n_size)) && n) {
			NFSUNLOCKNODE(np);
			/*
			 * Get the buffer (in its pre-append state to maintain
			 * B_CACHE if it was previously set).  Resize the
			 * nfsnode after we have locked the buffer to prevent
			 * readers from reading garbage.
			 */
			obcount = np->n_size - (lbn * biosize);
			bp = nfs_getcacheblk(vp, lbn, obcount, td);

			if (bp != NULL) {
				long save;

				NFSLOCKNODE(np);
				np->n_size = uio->uio_offset + n;
				np->n_flag |= NMODIFIED;
				np->n_flag &= ~NVNSETSZSKIP;
				vnode_pager_setsize(vp, np->n_size);
				NFSUNLOCKNODE(np);

				save = bp->b_flags & B_CACHE;
				bcount = on + n;
				allocbuf(bp, bcount);
				bp->b_flags |= save;
				if (noncontig_write != 0 && on > obcount)
					vfs_bio_bzero_buf(bp, obcount, on -
					    obcount);
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
			NFSUNLOCKNODE(np);
			bp = nfs_getcacheblk(vp, lbn, bcount, td);
			NFSLOCKNODE(np);
			if (uio->uio_offset + n > np->n_size) {
				np->n_size = uio->uio_offset + n;
				np->n_flag |= NMODIFIED;
				np->n_flag &= ~NVNSETSZSKIP;
				vnode_pager_setsize(vp, np->n_size);
			}
			NFSUNLOCKNODE(np);
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

		bp_cached = 1;
		if (on == 0 && n == bcount) {
			if ((bp->b_flags & B_CACHE) == 0)
				bp_cached = 0;
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
		NFSLOCKNODE(np);
		np->n_flag |= NMODIFIED;
		NFSUNLOCKNODE(np);

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
		 * If there has been a file lock applied to this file
		 * or vfs.nfs.old_noncontig_writing is set, do the following:
		 * While it is possible to merge discontiguous writes due to
		 * our having a B_CACHE buffer ( and thus valid read data
		 * for the hole), we don't because it could lead to
		 * significant cache coherency problems with multiple clients,
		 * especially if locking is implemented later on.
		 *
		 * If vfs.nfs.old_noncontig_writing is not set and there has
		 * not been file locking done on this file:
		 * Relax coherency a bit for the sake of performance and
		 * expand the current dirty region to contain the new
		 * write even if it means we mark some non-dirty data as
		 * dirty.
		 */

		if (noncontig_write == 0 && bp->b_dirtyend > 0 &&
		    (on > bp->b_dirtyend || (on + n) < bp->b_dirtyoff)) {
			if (bwrite(bp) == EINTR) {
				error = EINTR;
				break;
			}
			goto again;
		}

		local_resid = uio->uio_resid;
		error = vn_io_fault_uiomove((char *)bp->b_data + on, n, uio);

		if (error != 0 && !bp_cached) {
			/*
			 * This block has no other content then what
			 * possibly was written by the faulty uiomove.
			 * Release it, forgetting the data pages, to
			 * prevent the leak of uninitialized data to
			 * usermode.
			 */
			bp->b_ioflags |= BIO_ERROR;
			brelse(bp);
			uio->uio_offset -= local_resid - uio->uio_resid;
			uio->uio_resid = local_resid;
			break;
		}

		/*
		 * Since this block is being modified, it must be written
		 * again and not just committed.  Since write clustering does
		 * not work for the stage 1 data write, only the stage 2
		 * commit rpc, we have to clear B_CLUSTEROK as well.
		 */
		bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);

		/*
		 * Get the partial update on the progress made from
		 * uiomove, if an error occurred.
		 */
		if (error != 0)
			n = local_resid - uio->uio_resid;

		/*
		 * Only update dirtyoff/dirtyend if not a degenerate
		 * condition.
		 */
		if (n > 0) {
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
			error1 = bwrite(bp);
			if (error1 != 0) {
				if (error == 0)
					error = error1;
				break;
			}
		} else if ((n + on) == biosize || (ioflag & IO_ASYNC) != 0) {
			bp->b_flags |= B_ASYNC;
			(void) bwrite(bp);
		} else {
			bdwrite(bp);
		}

		if (error != 0)
			break;
	} while (uio->uio_resid > 0 && n > 0);

	if (error == 0) {
		nanouptime(&ts);
		NFSLOCKNODE(np);
		np->n_localmodtime = ts;
		NFSUNLOCKNODE(np);
	} else {
		if (ioflag & IO_UNIT) {
			VATTR_NULL(&vattr);
			vattr.va_size = orig_size;
			/* IO_SYNC is handled implicitely */
			(void)VOP_SETATTR(vp, &vattr, cred);
			uio->uio_offset -= orig_resid - uio->uio_resid;
			uio->uio_resid = orig_resid;
		}
	}

out:
	curthread_pflags2_restore(save2);
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
		bp = getblk(vp, bn, size, PCATCH, 0, 0);
		newnfs_restore_sigmask(td, &oldset);
		while (bp == NULL) {
			if (newnfs_sigintr(nmp, td))
				return (NULL);
			bp = getblk(vp, bn, size, 0, 2 * hz, 0);
		}
	} else {
		bp = getblk(vp, bn, size, 0, 0, 0);
	}

	if (vp->v_type == VREG)
		bp->b_blkno = bn * (vp->v_bufobj.bo_bsize / DEV_BSIZE);
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
	bool old_lock;
	struct timespec ts;

	ASSERT_VOP_LOCKED(vp, "ncl_vinvalbuf");

	if ((nmp->nm_flag & NFSMNT_INT) == 0)
		intrflg = 0;
	if (NFSCL_FORCEDISM(nmp->nm_mountp))
		intrflg = 1;
	if (intrflg) {
		slpflag = PCATCH;
		slptimeo = 2 * hz;
	} else {
		slpflag = 0;
		slptimeo = 0;
	}

	old_lock = ncl_excl_start(vp);
	if (old_lock)
		flags |= V_ALLOWCLEAN;

	/*
	 * Now, flush as required.
	 */
	if ((flags & (V_SAVE | V_VMIO)) == V_SAVE) {
		vnode_pager_clean_sync(vp);

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
	if (NFSHASPNFS(nmp)) {
		nfscl_layoutcommit(vp, td);
		nanouptime(&ts);
		/*
		 * Invalidate the attribute cache, since writes to a DS
		 * won't update the size attribute.
		 */
		NFSLOCKNODE(np);
		np->n_attrstamp = 0;
	} else {
		nanouptime(&ts);
		NFSLOCKNODE(np);
	}
	if ((np->n_flag & NMODIFIED) != 0) {
		np->n_localmodtime = ts;
		np->n_flag &= ~NMODIFIED;
	}
	NFSUNLOCKNODE(np);
out:
	ncl_excl_finish(vp, old_lock);
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
	 *
	 * Readdirplus RPCs do vget()s to acquire the vnodes for entries
	 * in the directory in order to update attributes. This can deadlock
	 * with another thread that is waiting for async I/O to be done by
	 * an nfsiod thread while holding a lock on one of these vnodes.
	 * To avoid this deadlock, don't allow the async nfsiod threads to
	 * perform Readdirplus RPCs.
	 */
	NFSLOCKIOD();
	if ((bp->b_iocmd == BIO_WRITE && (bp->b_flags & B_NEEDCOMMIT) &&
	     (nmp->nm_bufqiods > ncl_numasync / 2)) ||
	    (bp->b_vp->v_type == VDIR && (nmp->nm_flag & NFSMNT_RDIRPLUS))) {
		NFSUNLOCKIOD();
		return(EIO);
	}
again:
	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = PCATCH;
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
					NFSUNLOCKIOD();
					return (error2);
				}
				if (slpflag == PCATCH) {
					slpflag = 0;
					slptimeo = 2 * hz;
				}
			}
			/*
			 * We might have lost our iod while sleeping,
			 * so check and loop if necessary.
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
		KASSERT((bp->b_flags & B_DIRECT) == 0,
		    ("ncl_asyncio: B_DIRECT set"));
		NFSUNLOCKIOD();
		return (0);
	}

	NFSUNLOCKIOD();

	/*
	 * All the iods are busy on other mounts, so return EIO to
	 * force the caller to process the i/o synchronously.
	 */
	NFS_DPF(ASYNCIO, ("ncl_asyncio: no iods available, i/o is synchronous\n"));
	return (EIO);
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
		NFSINCRGLOBAL(nfsstatsv1.read_bios);
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
			ssize_t left = uiop->uio_resid;

			if (left > 0)
				bzero((char *)bp->b_data + nread, left);
			uiop->uio_resid = 0;
		    }
		}
		/* ASSERT_VOP_LOCKED(vp, "ncl_doio"); */
		if (p && vp->v_writecount <= -1) {
			NFSLOCKNODE(np);
			if (NFS_TIMESPEC_COMPARE(&np->n_mtime, &np->n_vattr.na_mtime)) {
				NFSUNLOCKNODE(np);
				PROC_LOCK(p);
				killproc(p, "text file modification");
				PROC_UNLOCK(p);
			} else
				NFSUNLOCKNODE(np);
		}
		break;
	    case VLNK:
		uiop->uio_offset = (off_t)0;
		NFSINCRGLOBAL(nfsstatsv1.readlink_bios);
		error = ncl_readlinkrpc(vp, uiop, cr);
		break;
	    case VDIR:
		NFSINCRGLOBAL(nfsstatsv1.readdir_bios);
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
		printf("ncl_doio:  type %x unexpected\n", vp->v_type);
		break;
	    }
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
		    if (NFSCL_FORCEDISM(vp->v_mount) || retv == 0) {
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
	    NFSLOCKNODE(np);
	    if ((off_t)bp->b_blkno * DEV_BSIZE + bp->b_dirtyend > np->n_size)
		bp->b_dirtyend = np->n_size - (off_t)bp->b_blkno * DEV_BSIZE;
	    NFSUNLOCKNODE(np);

	    if (bp->b_dirtyend > bp->b_dirtyoff) {
		io.iov_len = uiop->uio_resid = bp->b_dirtyend
		    - bp->b_dirtyoff;
		uiop->uio_offset = (off_t)bp->b_blkno * DEV_BSIZE
		    + bp->b_dirtyoff;
		io.iov_base = (char *)bp->b_data + bp->b_dirtyoff;
		uiop->uio_rw = UIO_WRITE;
		NFSINCRGLOBAL(nfsstatsv1.write_bios);

		if ((bp->b_flags & (B_ASYNC | B_NEEDCOMMIT | B_NOCACHE | B_CLUSTER)) == B_ASYNC)
		    iomode = NFSWRITE_UNSTABLE;
		else
		    iomode = NFSWRITE_FILESYNC;

		error = ncl_writerpc(vp, uiop, cr, &iomode, &must_commit,
		    called_from_strategy, 0);

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
			bp->b_flags &= ~(B_INVAL|B_NOCACHE);
			if ((bp->b_flags & B_PAGING) == 0) {
			    bdirty(bp);
			    bp->b_flags &= ~B_DONE;
			}
			if ((error == EINTR || error == ETIMEDOUT) &&
			    (bp->b_flags & B_ASYNC) == 0)
			    bp->b_flags |= B_EINTR;
		} else {
		    if (error) {
			bp->b_ioflags |= BIO_ERROR;
			bp->b_flags |= B_INVAL;
			bp->b_error = np->n_error = error;
			NFSLOCKNODE(np);
			np->n_flag |= NWRITEERR;
			np->n_attrstamp = 0;
			KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
			NFSUNLOCKNODE(np);
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
	if (must_commit == 1)
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
ncl_meta_setsize(struct vnode *vp, struct thread *td, u_quad_t nsize)
{
	struct nfsnode *np = VTONFS(vp);
	u_quad_t tsize;
	int biosize = vp->v_bufobj.bo_bsize;
	int error = 0;

	NFSLOCKNODE(np);
	tsize = np->n_size;
	np->n_size = nsize;
	NFSUNLOCKNODE(np);

	if (nsize < tsize) {
		struct buf *bp;
		daddr_t lbn;
		int bufsize;

		/*
		 * vtruncbuf() doesn't get the buffer overlapping the
		 * truncation point.  We may have a B_DELWRI and/or B_CACHE
		 * buffer that now needs to be truncated.
		 */
		error = vtruncbuf(vp, nsize, biosize);
		lbn = nsize / biosize;
		bufsize = nsize - (lbn * biosize);
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
