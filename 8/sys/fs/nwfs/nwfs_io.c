/*-
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>

#include <fs/nwfs/nwfs.h>
#include <fs/nwfs/nwfs_node.h>
#include <fs/nwfs/nwfs_subr.h>

static int nwfs_fastlookup = 1;

SYSCTL_DECL(_vfs_nwfs);
SYSCTL_INT(_vfs_nwfs, OID_AUTO, fastlookup, CTLFLAG_RW, &nwfs_fastlookup, 0, "");


extern int nwfs_pbuf_freecnt;

#define DE_SIZE	(sizeof(struct dirent))
#define	NWFS_RWCACHE

static int
nwfs_readvdir(struct vnode *vp, struct uio *uio, struct ucred *cred) {
	struct nwmount *nmp = VTONWFS(vp);
	int error, count, i;
	struct dirent dp;
	struct nwnode *np = VTONW(vp);
	struct nw_entry_info fattr;
	struct vnode *newvp;
	struct componentname cn;
	ncpfid fid;

	np = VTONW(vp);
	NCPVNDEBUG("dirname='%s'\n",np->n_name);
	if (uio->uio_resid < DE_SIZE || (uio->uio_offset < 0))
		return (EINVAL);
	error = 0;
	count = 0;
	i = uio->uio_offset / DE_SIZE; /* offset in directory */
	if (i == 0) {
		error = ncp_initsearch(vp, uio->uio_td, cred);
		if (error) {
			NCPVNDEBUG("cannot initialize search, error=%d",error);
			return( error );
		}
	}

	for (; uio->uio_resid >= DE_SIZE; i++) {
		bzero((char *) &dp, DE_SIZE);
		dp.d_reclen = DE_SIZE;
		switch (i) {
		    case 0:		/* `.' */
		    case 1:		/* `..' */
			dp.d_fileno = (i == 0) ? np->n_fid.f_id : np->n_parent.f_id;
			if (!dp.d_fileno) dp.d_fileno = NWFS_ROOT_INO;
			dp.d_namlen = i + 1;
			dp.d_name[0] = '.';
			dp.d_name[1] = '.';
			dp.d_name[i + 1] = '\0';
			dp.d_type = DT_DIR;
			break;
		    default:
			error = ncp_search_for_file_or_subdir(nmp, &np->n_seq, &fattr, uio->uio_td, cred);
			if (error && error < 0x80) break;
			dp.d_fileno = fattr.dirEntNum;
			dp.d_type = (fattr.attributes & aDIR) ? DT_DIR : DT_REG;
			dp.d_namlen = fattr.nameLen;
			bcopy(fattr.entryName, dp.d_name, dp.d_namlen);
			dp.d_name[dp.d_namlen] = '\0';
#if 0
			if (error && eofflag) {
			/*	*eofflag = 1;*/
				break;
			}
#endif
			break;
		}
		if (nwfs_fastlookup && !error && i > 1) {
			fid.f_id = fattr.dirEntNum;
			fid.f_parent = np->n_fid.f_id;
			error = nwfs_nget(vp->v_mount, fid, &fattr, vp, &newvp);
			if (!error) {
				VTONW(newvp)->n_ctime = VTONW(newvp)->n_vattr.va_ctime.tv_sec;
				cn.cn_nameptr = dp.d_name;
				cn.cn_namelen = dp.d_namlen;
				cache_enter(vp, newvp, &cn);
				vput(newvp);
			} else
				error = 0;
		}
		if (error >= 0x80) {
		    error = 0;
		    break;
		}
		if ((error = uiomove(&dp, DE_SIZE, uio)))
			break;
	}

	uio->uio_offset = i * DE_SIZE;
	return (error);
}

int
nwfs_readvnode(struct vnode *vp, struct uio *uiop, struct ucred *cred) {
	struct nwmount *nmp = VFSTONWFS(vp->v_mount);
	struct nwnode *np = VTONW(vp);
	struct thread *td;
	struct vattr vattr;
	int error, biosize;

	if (vp->v_type != VREG && vp->v_type != VDIR) {
		printf("%s: vn types other than VREG or VDIR are unsupported !\n",__func__);
		return EIO;
	}
	if (uiop->uio_resid == 0) return 0;
	if (uiop->uio_offset < 0) return EINVAL;
/*	if (uiop->uio_offset + uiop->uio_resid > nmp->nm_maxfilesize)
		return (EFBIG);*/
	td = uiop->uio_td;
	if (vp->v_type == VDIR) {
		error = nwfs_readvdir(vp, uiop, cred);
		return error;
	}
	biosize = NWFSTOCONN(nmp)->buffer_size;
	if (np->n_flag & NMODIFIED) {
		nwfs_attr_cacheremove(vp);
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error) return (error);
		np->n_mtime = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error) return (error);
		if (np->n_mtime != vattr.va_mtime.tv_sec) {
			error = nwfs_vinvalbuf(vp, td);
			if (error) return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		}
	}
	error = ncp_read(NWFSTOCONN(nmp), &np->n_fh, uiop, cred);
	return (error);
}

int
nwfs_writevnode(vp, uiop, cred, ioflag)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	int ioflag;
{
	struct nwmount *nmp = VTONWFS(vp);
	struct nwnode *np = VTONW(vp);
	struct thread *td;
/*	struct vattr vattr;*/
	int error = 0;

	if (vp->v_type != VREG) {
		printf("%s: vn types other than VREG unsupported !\n",__func__);
		return EIO;
	}
	NCPVNDEBUG("ofs=%d,resid=%d\n",(int)uiop->uio_offset, uiop->uio_resid);
	if (uiop->uio_offset < 0) return EINVAL;
/*	if (uiop->uio_offset + uiop->uio_resid > nmp->nm_maxfilesize)
		return (EFBIG);*/
	td = uiop->uio_td;
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			nwfs_attr_cacheremove(vp);
			error = nwfs_vinvalbuf(vp, td);
			if (error) return (error);
		}
		if (ioflag & IO_APPEND) {
		/* We can relay only on local information about file size,
		 * because until file is closed NetWare will not return
		 * the correct size. */
#ifdef notyet
			nwfs_attr_cacheremove(vp);
			error = VOP_GETATTR(vp, &vattr, cred);
			if (error) return (error);
#endif
			uiop->uio_offset = np->n_size;
		}
	}
	if (uiop->uio_resid == 0) return 0;

	if (vn_rlimit_fsize(vp, uiop, td))
		return (EFBIG);

	error = ncp_write(NWFSTOCONN(nmp), &np->n_fh, uiop, cred);
	NCPVNDEBUG("after: ofs=%d,resid=%d\n",(int)uiop->uio_offset, uiop->uio_resid);
	if (!error) {
		if (uiop->uio_offset > np->n_size) {
			np->n_vattr.va_size = np->n_size = uiop->uio_offset;
			vnode_pager_setsize(vp, np->n_size);
		}
	}
	return (error);
}

/*
 * Do an I/O operation to/from a cache block.
 */
int
nwfs_doio(vp, bp, cr, td)
	struct vnode *vp;
	struct buf *bp;
	struct ucred *cr;
	struct thread *td;
{
	struct uio *uiop;
	struct nwnode *np;
	struct nwmount *nmp;
	int error = 0;
	struct uio uio;
	struct iovec io;

	np = VTONW(vp);
	nmp = VFSTONWFS(vp->v_mount);
	uiop = &uio;
	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_td = td;
	if (bp->b_iocmd == BIO_READ) {
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    io.iov_base = bp->b_data;
	    uiop->uio_rw = UIO_READ;
	    switch (vp->v_type) {
	      case VREG:
		uiop->uio_offset = ((off_t)bp->b_blkno) * DEV_BSIZE;
		error = ncp_read(NWFSTOCONN(nmp), &np->n_fh, uiop, cr);
		if (error)
			break;
		if (uiop->uio_resid) {
			int left = uiop->uio_resid;
			int nread = bp->b_bcount - left;
			if (left > 0)
			    bzero((char *)bp->b_data + nread, left);
		}
		break;
/*	    case VDIR:
		nfsstats.readdir_bios++;
		uiop->uio_offset = ((u_quad_t)bp->b_lblkno) * NFS_DIRBLKSIZ;
		if (nmp->nm_flag & NFSMNT_RDIRPLUS) {
			error = nfs_readdirplusrpc(vp, uiop, cr);
			if (error == NFSERR_NOTSUPP)
				nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
		}
		if ((nmp->nm_flag & NFSMNT_RDIRPLUS) == 0)
			error = nfs_readdirrpc(vp, uiop, cr);
		if (error == 0 && uiop->uio_resid == bp->b_bcount)
			bp->b_flags |= B_INVAL;
		break;
*/
	    default:
		printf("nwfs_doio:  type %x unexpected\n",vp->v_type);
		break;
	    };
	    if (error) {
		bp->b_ioflags |= BIO_ERROR;
		bp->b_error = error;
	    }
	} else { /* write */
	    if (((bp->b_blkno * DEV_BSIZE) + bp->b_dirtyend) > np->n_size)
		bp->b_dirtyend = np->n_size - (bp->b_blkno * DEV_BSIZE);

	    if (bp->b_dirtyend > bp->b_dirtyoff) {
		io.iov_len = uiop->uio_resid = bp->b_dirtyend - bp->b_dirtyoff;
		uiop->uio_offset = ((off_t)bp->b_blkno) * DEV_BSIZE + bp->b_dirtyoff;
		io.iov_base = (char *)bp->b_data + bp->b_dirtyoff;
		uiop->uio_rw = UIO_WRITE;
		error = ncp_write(NWFSTOCONN(nmp), &np->n_fh, uiop, cr);

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
		 */
		if (error == EINTR
		    || (!error && (bp->b_flags & B_NEEDCOMMIT))) {
			int s;

			s = splbio();
			bp->b_flags &= ~(B_INVAL|B_NOCACHE);
			if ((bp->b_flags & B_ASYNC) == 0)
			    bp->b_flags |= B_EINTR;
			if ((bp->b_flags & B_PAGING) == 0) {
			    bdirty(bp);
			    bp->b_flags &= ~B_DONE;
			}
			if ((bp->b_flags & B_ASYNC) == 0)
			    bp->b_flags |= B_EINTR;
			splx(s);
		} else {
			if (error) {
				bp->b_ioflags |= BIO_ERROR;
				bp->b_error /*= np->n_error */= error;
/*				np->n_flag |= NWRITEERR;*/
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
	bufdone(bp);
	return (error);
}

/*
 * Vnode op for VM getpages.
 * Wish wish .... get rid from multiple IO routines
 */
int
nwfs_getpages(ap)
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_reqpage;
		vm_ooffset_t a_offset;
	} */ *ap;
{
#ifndef NWFS_RWCACHE
	return vop_stdgetpages(ap);(ap->a_vp, ap->a_m, ap->a_count,
#else
	int i, error, nextoff, size, toff, npages, count;
	struct uio uio;
	struct iovec iov;
	vm_offset_t kva;
	struct buf *bp;
	struct vnode *vp;
	struct thread *td;
	struct ucred *cred;
	struct nwmount *nmp;
	struct nwnode *np;
	vm_object_t object;
	vm_page_t *pages;

	vp = ap->a_vp;
	td = curthread;			/* XXX */
	cred = td->td_ucred;		/* XXX */
	np = VTONW(vp);
	nmp = VFSTONWFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;

	if ((object = vp->v_object) == NULL) {
		printf("nwfs_getpages: called with non-merged cache vnode??\n");
		return VM_PAGER_ERROR;
	}

	bp = getpbuf(&nwfs_pbuf_freecnt);
	npages = btoc(count);
	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;

	error = ncp_read(NWFSTOCONN(nmp), &np->n_fh, &uio,cred);
	pmap_qremove(kva, npages);

	relpbuf(bp, &nwfs_pbuf_freecnt);

	VM_OBJECT_LOCK(object);
	if (error && (uio.uio_resid == count)) {
		printf("nwfs_getpages: error %d\n",error);
		vm_page_lock_queues();
		for (i = 0; i < npages; i++) {
			if (ap->a_reqpage != i)
				vm_page_free(pages[i]);
		}
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(object);
		return VM_PAGER_ERROR;
	}

	size = count - uio.uio_resid;

	vm_page_lock_queues();
	for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
		vm_page_t m;
		nextoff = toff + PAGE_SIZE;
		m = pages[i];

		if (nextoff <= size) {
			m->valid = VM_PAGE_BITS_ALL;
			KASSERT(m->dirty == 0,
			    ("nwfs_getpages: page %p is dirty", m));
		} else {
			int nvalid = ((size + DEV_BSIZE - 1) - toff) & ~(DEV_BSIZE - 1);
			vm_page_set_valid(m, 0, nvalid);
			KASSERT((m->dirty & vm_page_bits(0, nvalid)) == 0,
			    ("nwfs_getpages: page %p is dirty", m));
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
	return 0;
#endif /* NWFS_RWCACHE */
}

/*
 * Vnode op for VM putpages.
 * possible bug: all IO done in sync mode
 * Note that vop_close always invalidate pages before close, so it's
 * not necessary to open vnode.
 */
int
nwfs_putpages(ap)
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_sync;
		int *a_rtvals;
		vm_ooffset_t a_offset;
	} */ *ap;
{
	int error;
	struct vnode *vp = ap->a_vp;
	struct thread *td;
	struct ucred *cred;

#ifndef NWFS_RWCACHE
	td = curthread;			/* XXX */
	cred = td->td_ucred;		/* XXX */
	VOP_OPEN(vp, FWRITE, cred, td, NULL);
	error = vop_stdputpages(ap);
	VOP_CLOSE(vp, FWRITE, cred, td);
	return error;
#else
	struct uio uio;
	struct iovec iov;
	vm_offset_t kva;
	struct buf *bp;
	int i, npages, count;
	int *rtvals;
	struct nwmount *nmp;
	struct nwnode *np;
	vm_page_t *pages;

	td = curthread;			/* XXX */
	cred = td->td_ucred;		/* XXX */
/*	VOP_OPEN(vp, FWRITE, cred, td, NULL);*/
	np = VTONW(vp);
	nmp = VFSTONWFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;
	rtvals = ap->a_rtvals;
	npages = btoc(count);

	for (i = 0; i < npages; i++) {
		rtvals[i] = VM_PAGER_ERROR;
	}

	bp = getpbuf(&nwfs_pbuf_freecnt);
	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;
	NCPVNDEBUG("ofs=%d,resid=%d\n",(int)uio.uio_offset, uio.uio_resid);

	error = ncp_write(NWFSTOCONN(nmp), &np->n_fh, &uio, cred);
/*	VOP_CLOSE(vp, FWRITE, cred, td);*/
	NCPVNDEBUG("paged write done: %d\n", error);

	pmap_qremove(kva, npages);
	relpbuf(bp, &nwfs_pbuf_freecnt);

	if (!error)
		vnode_pager_undirty_pages(pages, rtvals, count - uio.uio_resid);
	return rtvals[0];
#endif /* NWFS_RWCACHE */
}
/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
nwfs_vinvalbuf(vp, td)
	struct vnode *vp;
	struct thread *td;
{
	struct nwnode *np = VTONW(vp);
/*	struct nwmount *nmp = VTONWFS(vp);*/
	int error = 0;

	if (vp->v_iflag & VI_DOOMED)
		return (0);

	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = tsleep(&np->n_flag, PRIBIO + 2, "nwfsvinv", 2 * hz);
		error = ncp_chkintr(NWFSTOCONN(VTONWFS(vp)), td);
		if (error == EINTR)
			return EINTR;
	}
	np->n_flag |= NFLUSHINPROG;

	if (vp->v_bufobj.bo_object != NULL) {
		VM_OBJECT_LOCK(vp->v_bufobj.bo_object);
		vm_object_page_clean(vp->v_bufobj.bo_object, 0, 0, OBJPC_SYNC);
		VM_OBJECT_UNLOCK(vp->v_bufobj.bo_object);
	}

	error = vinvalbuf(vp, V_SAVE, PCATCH, 0);
	while (error) {
		if (error == ERESTART || error == EINTR) {
			np->n_flag &= ~NFLUSHINPROG;
			if (np->n_flag & NFLUSHWANT) {
				np->n_flag &= ~NFLUSHWANT;
				wakeup(&np->n_flag);
			}
			return EINTR;
		}
		error = vinvalbuf(vp, V_SAVE, PCATCH, 0);
	}
	np->n_flag &= ~(NMODIFIED | NFLUSHINPROG);
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup(&np->n_flag);
	}
	return (error);
}
