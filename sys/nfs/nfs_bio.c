/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
 *	From:	@(#)nfs_bio.c	7.19 (Berkeley) 4/16/91
 *	$Id: nfs_bio.c,v 1.6 1994/06/12 04:05:39 davidg Exp $
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "buf.h"
#include "uio.h"
#include "namei.h"
#include "vnode.h"
#include "trace.h"
#include "mount.h"
#include "resourcevar.h"
#ifdef	PROTOTYPESDONE
#include "vm/vnode_pager.h"
#endif	/*PROTOTYPESDONE*/

#include "nfsnode.h"
#include "nfsv2.h"
#include "nfs.h"
#include "nfsiom.h"
#include "nfsmount.h"

/* True and false, how exciting */
#define	TRUE	1
#define	FALSE	0

/*
 * Vnode op for read using bio
 * Any similarity to readip() is purely coincidental
 */
int
nfs_bioread(vp, uio, ioflag, cred)
	register struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	register struct nfsnode *np = VTONFS(vp);
	register int biosize;
	struct buf *bp;
	struct vattr vattr;
	daddr_t lbn, bn, rablock;
	int diff, error = 0;
	long n = 0, on = 0;

#ifdef lint
	ioflag = ioflag;
#endif /* lint */
#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("nfs_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0 && vp->v_type != VDIR)
		return (EINVAL);
	biosize = VFSTONFS(vp->v_mount)->nm_rsize;
	/*
	 * If the file's modify time on the server has changed since the
	 * last read rpc or you have written to the file,
	 * you may have lost data cache consistency with the
	 * server, so flush all of the file's data out of the cache.
	 * Then force a getattr rpc to ensure that you have up to date
	 * attributes.
	 * NB: This implies that cache data can be read when up to
	 * NFS_ATTRTIMEO seconds out of date. If you find that you need current
	 * attributes this could be forced by setting n_attrstamp to 0 before
	 * the nfs_dogetattr() call.
	 */
	if (vp->v_type != VLNK) {
		if (np->n_flag & NMODIFIED) {
			np->n_flag &= ~NMODIFIED;
			vinvalbuf(vp, TRUE);
			np->n_attrstamp = 0;
			np->n_direofoffset = 0;
			if (error = nfs_dogetattr(vp, &vattr, cred, 1,
			    uio->uio_procp))
				return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		} else {
			if (error = nfs_dogetattr(vp, &vattr, cred, 1,
			    uio->uio_procp))
				return (error);
			if (np->n_mtime != vattr.va_mtime.tv_sec) {
				np->n_direofoffset = 0;
				vinvalbuf(vp, TRUE);
				np->n_mtime = vattr.va_mtime.tv_sec;
			}
		}
	}
	do {
	    switch (vp->v_type) {
	    case VREG:
		nfsstats.biocache_reads++;
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize-1);
		n = MIN((unsigned)(biosize - on), uio->uio_resid);
		diff = np->n_size - uio->uio_offset;
		if (diff <= 0)
			return (error);
		if (diff < n)
			n = diff;
		bn = lbn*(biosize/DEV_BSIZE);
		rablock = (lbn+1)*(biosize/DEV_BSIZE);
		if (vp->v_lastr + 1 == lbn &&
		    np->n_size > (rablock * DEV_BSIZE))
			error = breada(vp, bn, biosize, rablock, biosize,
				cred, &bp);
		else
			error = bread(vp, bn, biosize, cred, &bp);
		vp->v_lastr = lbn;
		if (bp->b_resid) {
		   diff = (on >= (biosize-bp->b_resid)) ? 0 :
			(biosize-bp->b_resid-on);
		   n = MIN(n, diff);
		}
		break;
	    case VLNK:
		nfsstats.biocache_readlinks++;
		on = 0;
		error = bread(vp, (daddr_t)0, NFS_MAXPATHLEN, cred, &bp);
		n = MIN(uio->uio_resid, NFS_MAXPATHLEN - bp->b_resid);
		break;
	    case VDIR:
		nfsstats.biocache_readdirs++;
		on = 0;
		error = bread(vp, uio->uio_offset, NFS_DIRBLKSIZ, cred, &bp);
		n = MIN(uio->uio_resid, NFS_DIRBLKSIZ - bp->b_resid);
		break;
            default:
		;
	    };
	    if (error) {
		brelse(bp);
		return (error);
	    }
	    if (n > 0)
		error = uiomove(bp->b_un.b_addr + on, (int)n, uio);
	    switch (vp->v_type) {
	    case VREG:
		break;
	    case VLNK:
		n = 0;
		break;
	    case VDIR:
		uio->uio_offset = bp->b_blkno;
		break;
            default:
		;
	    };
	    brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	return (error);
}

/*
 * Vnode op for write using bio
 */
int
nfs_write(vp, uio, ioflag, cred)
	register struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct proc *p = uio->uio_procp;
	register int biosize;
	struct buf *bp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;
	daddr_t lbn, bn;
	int n, on, error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("nfs_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("nfs_write proc");
#endif
	if (vp->v_type != VREG)
		return (EIO);
	/* Should we try and do this ?? */
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			np->n_flag &= ~NMODIFIED;
			vinvalbuf(vp, TRUE);
		}
		if (ioflag & IO_APPEND) {
			np->n_attrstamp = 0;
			if (error = nfs_dogetattr(vp, &vattr, cred, 1, p))
				return (error);
			uio->uio_offset = np->n_size;
		}
		return (nfs_writerpc(vp, uio, cred));
	}
#ifdef notdef
	cnt = uio->uio_resid;
	osize = np->n_size;
#endif
	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_resid == 0)
		return (0);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	if (p && uio->uio_offset + uio->uio_resid >
	      p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}
	/*
	 * I use nm_rsize, not nm_wsize so that all buffer cache blocks
	 * will be the same size within a filesystem. nfs_writerpc will
	 * still use nm_wsize when sizing the rpc's.
	 */
	biosize = VFSTONFS(vp->v_mount)->nm_rsize;
	np->n_flag |= NMODIFIED;
	if ((ioflag & IO_PAGER) == 0)
		vnode_pager_uncache(vp);

	do {
		nfsstats.biocache_writes++;
		lbn = uio->uio_offset / biosize;
		on = uio->uio_offset & (biosize-1);
		n = MIN((unsigned)(biosize - on), uio->uio_resid);
		if (uio->uio_offset+n > np->n_size) {
			np->n_size = uio->uio_offset+n;
			vnode_pager_setsize(vp, np->n_size);
		}
		bn = lbn*(biosize/DEV_BSIZE);
again:
		bp = getblk(vp, bn, biosize);
		if (bp->b_wcred == NOCRED) {
			crhold(cred);
			bp->b_wcred = cred;
		}
		if (bp->b_dirtyend > 0) {
			/*
			 * If the new write will leave a contiguous dirty
			 * area, just update the b_dirtyoff and b_dirtyend,
			 * otherwise force a write rpc of the old dirty area.
			 */
			if (on <= bp->b_dirtyend && (on+n) >= bp->b_dirtyoff) {
				bp->b_dirtyoff = MIN(on, bp->b_dirtyoff);
				bp->b_dirtyend = MAX((on+n), bp->b_dirtyend);
			} else {
				bp->b_proc = p;
				if (error = bwrite(bp))
					return (error);
				goto again;
			}
		} else {
			bp->b_dirtyoff = on;
			bp->b_dirtyend = on+n;
		}
		if (error = uiomove(bp->b_un.b_addr + on, n, uio)) {
			brelse(bp);
			return (error);
		}
		bp->b_proc = (struct proc *)0;
		if ((n+on) == biosize) {
			bawrite(bp);
		} else {
			bdwrite(bp);
		}
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
#ifdef notdef
	/* Should we try and do this for nfs ?? */
	if (error && (ioflag & IO_UNIT)) {
		np->n_size = osize;
		uio->uio_offset -= cnt - uio->uio_resid;
		uio->uio_resid = cnt;
	}
#endif
	return (error);
}
