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
 *	@(#)nfs_serv.c	8.3 (Berkeley) 1/12/94
 */

/*
 * nfs version 2 server calls to vnode ops
 * - these routines generally have 3 phases
 *   1 - break down and validate rpc request in mbuf list
 *   2 - do the vnode ops for the request
 *       (surprisingly ?? many are very similar to syscalls in vfs_syscalls.c)
 *   3 - build the rpc reply in an mbuf list
 *   nb:
 *	- do not mix the phases, since the nfsm_?? macros can return failures
 *	  on a bad rpc or similar and do not do any vrele() or vput()'s
 *
 *      - the nfsm_reply() macro generates an nfs rpc reply with the nfs
 *	error number iff error != 0 whereas
 *	returning an error from the server function implies a fatal error
 *	such as a badly constructed rpc request that should be dropped without
 *	a reply.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include <vm/vm.h>

#include <nfs/nfsv2.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nqnfs.h>

/* Defs */
#define	TRUE	1
#define	FALSE	0

/* Global vars */
extern u_long nfs_procids[NFS_NPROCS];
extern u_long nfs_xdrneg1;
extern u_long nfs_false, nfs_true;
nfstype nfs_type[9] = { NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, NFNON,
		      NFCHR, NFNON };

/*
 * nqnfs access service
 */
nqnfsrv_access(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, mode = 0;
	char *cp2;
	struct mbuf *mb, *mreq;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_long *, 3 * NFSX_UNSIGNED);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	if (*tl++ == nfs_true)
		mode |= VREAD;
	if (*tl++ == nfs_true)
		mode |= VWRITE;
	if (*tl == nfs_true)
		mode |= VEXEC;
	error = nfsrv_access(vp, mode, cred, rdonly, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(0);
	nfsm_srvdone;
}

/*
 * nfs getattr service
 */
nfsrv_getattr(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct nfsv2_fattr *fp;
	struct vattr va;
	register struct vattr *vap = &va;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	nqsrv_getl(vp, NQL_READ);
	error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfillattr;
	nfsm_srvdone;
}

/*
 * nfs setattr service
 */
nfsrv_setattr(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	struct vattr va;
	register struct vattr *vap = &va;
	register struct nfsv2_sattr *sp;
	register struct nfsv2_fattr *fp;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	u_quad_t frev, frev2;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_SATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	nqsrv_getl(vp, NQL_WRITE);
	VATTR_NULL(vap);
	/*
	 * Nah nah nah nah na nah
	 * There is a bug in the Sun client that puts 0xffff in the mode
	 * field of sattr when it should put in 0xffffffff. The u_short
	 * doesn't sign extend.
	 * --> check the low order 2 bytes for 0xffff
	 */
	if ((fxdr_unsigned(int, sp->sa_mode) & 0xffff) != 0xffff)
		vap->va_mode = nfstov_mode(sp->sa_mode);
	if (sp->sa_uid != nfs_xdrneg1)
		vap->va_uid = fxdr_unsigned(uid_t, sp->sa_uid);
	if (sp->sa_gid != nfs_xdrneg1)
		vap->va_gid = fxdr_unsigned(gid_t, sp->sa_gid);
	if (nfsd->nd_nqlflag == NQL_NOVAL) {
		if (sp->sa_nfssize != nfs_xdrneg1)
			vap->va_size = fxdr_unsigned(u_quad_t, sp->sa_nfssize);
		if (sp->sa_nfsatime.nfs_sec != nfs_xdrneg1) {
#ifdef notyet
			fxdr_nfstime(&sp->sa_nfsatime, &vap->va_atime);
#else
			vap->va_atime.ts_sec =
				fxdr_unsigned(long, sp->sa_nfsatime.nfs_sec);
			vap->va_atime.ts_nsec = 0;
#endif
		}
		if (sp->sa_nfsmtime.nfs_sec != nfs_xdrneg1)
			fxdr_nfstime(&sp->sa_nfsmtime, &vap->va_mtime);
	} else {
		fxdr_hyper(&sp->sa_nqsize, &vap->va_size);
		fxdr_nqtime(&sp->sa_nqatime, &vap->va_atime);
		fxdr_nqtime(&sp->sa_nqmtime, &vap->va_mtime);
		vap->va_flags = fxdr_unsigned(u_long, sp->sa_nqflags);
	}

	/*
	 * If the size is being changed write acces is required, otherwise
	 * just check for a read only file system.
	 */
	if (vap->va_size == ((u_quad_t)((quad_t) -1))) {
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			error = EROFS;
			goto out;
		}
	} else {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		} else if (error = nfsrv_access(vp, VWRITE, cred, rdonly,
			nfsd->nd_procp))
			goto out;
	}
	if (error = VOP_SETATTR(vp, vap, cred, nfsd->nd_procp)) {
		vput(vp);
		nfsm_reply(0);
	}
	error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp);
out:
	vput(vp);
	nfsm_reply(NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL) + 2*NFSX_UNSIGNED);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfillattr;
	if (nfsd->nd_nqlflag != NQL_NOVAL) {
		nfsm_build(tl, u_long *, 2*NFSX_UNSIGNED);
		txdr_hyper(&frev2, tl);
	}
	nfsm_srvdone;
}

/*
 * nfs lookup rpc
 */
nfsrv_lookup(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct nfsv2_fattr *fp;
	struct nameidata nd;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	register caddr_t cp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, cache, duration2, cache2, len;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vattr va, *vap = &va;
	u_quad_t frev, frev2;

	fhp = &nfh.fh_generic;
	duration2 = 0;
	if (nfsd->nd_nqlflag != NQL_NOVAL) {
		nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
		duration2 = fxdr_unsigned(int, *tl);
	}
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = LOOKUP;
	nd.ni_cnd.cn_flags = LOCKLEAF | SAVESTART;
	if (error = nfs_namei(&nd, fhp, len, nfsd->nd_slp, nam, &md, &dpos,
	    nfsd->nd_procp))
		nfsm_reply(0);
	nqsrv_getl(nd.ni_startdir, NQL_READ);
	vrele(nd.ni_startdir);
	FREE(nd.ni_cnd.cn_pnbuf, M_NAMEI);
	vp = nd.ni_vp;
	bzero((caddr_t)fhp, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	if (error = VFS_VPTOFH(vp, &fhp->fh_fid)) {
		vput(vp);
		nfsm_reply(0);
	}
	if (duration2)
		(void) nqsrv_getlease(vp, &duration2, NQL_READ, nfsd,
			nam, &cache2, &frev2, cred);
	error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(NFSX_FH + NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL) + 5*NFSX_UNSIGNED);
	if (nfsd->nd_nqlflag != NQL_NOVAL) {
		if (duration2) {
			nfsm_build(tl, u_long *, 5*NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NQL_READ);
			*tl++ = txdr_unsigned(cache2);
			*tl++ = txdr_unsigned(duration2);
			txdr_hyper(&frev2, tl);
		} else {
			nfsm_build(tl, u_long *, NFSX_UNSIGNED);
			*tl = 0;
		}
	}
	nfsm_srvfhtom(fhp);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfillattr;
	nfsm_srvdone;
}

/*
 * nfs readlink service
 */
nfsrv_readlink(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	struct iovec iv[(NFS_MAXPATHLEN+MLEN-1)/MLEN];
	register struct iovec *ivp = iv;
	register struct mbuf *mp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, i, tlen, len;
	char *cp2;
	struct mbuf *mb, *mb2, *mp2, *mp3, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	len = 0;
	i = 0;
	while (len < NFS_MAXPATHLEN) {
		MGET(mp, M_WAIT, MT_DATA);
		MCLGET(mp, M_WAIT);
		mp->m_len = NFSMSIZ(mp);
		if (len == 0)
			mp3 = mp2 = mp;
		else {
			mp2->m_next = mp;
			mp2 = mp;
		}
		if ((len+mp->m_len) > NFS_MAXPATHLEN) {
			mp->m_len = NFS_MAXPATHLEN-len;
			len = NFS_MAXPATHLEN;
		} else
			len += mp->m_len;
		ivp->iov_base = mtod(mp, caddr_t);
		ivp->iov_len = mp->m_len;
		i++;
		ivp++;
	}
	uiop->uio_iov = iv;
	uiop->uio_iovcnt = i;
	uiop->uio_offset = 0;
	uiop->uio_resid = len;
	uiop->uio_rw = UIO_READ;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_procp = (struct proc *)0;
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly)) {
		m_freem(mp3);
		nfsm_reply(0);
	}
	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto out;
	}
	nqsrv_getl(vp, NQL_READ);
	error = VOP_READLINK(vp, uiop, cred);
out:
	vput(vp);
	if (error)
		m_freem(mp3);
	nfsm_reply(NFSX_UNSIGNED);
	if (uiop->uio_resid > 0) {
		len -= uiop->uio_resid;
		tlen = nfsm_rndup(len);
		nfsm_adj(mp3, NFS_MAXPATHLEN-tlen, tlen-len);
	}
	nfsm_build(tl, u_long *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(len);
	mb->m_next = mp3;
	nfsm_srvdone;
}

/*
 * nfs read service
 */
nfsrv_read(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct iovec *iv;
	struct iovec *iv2;
	register struct mbuf *m;
	register struct nfsv2_fattr *fp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, i, cnt, len, left, siz, tlen;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct mbuf *m2;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	struct vattr va, *vap = &va;
	off_t off;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (nfsd->nd_nqlflag == NQL_NOVAL) {
		nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_long, *tl);
	} else {
		nfsm_dissect(tl, u_long *, 2 * NFSX_UNSIGNED);
		fxdr_hyper(tl, &off);
	}
	nfsm_srvstrsiz(cnt, NFS_MAXDATA);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	if (vp->v_type != VREG) {
		error = (vp->v_type == VDIR) ? EISDIR : EACCES;
		vput(vp);
		nfsm_reply(0);
	}
	nqsrv_getl(vp, NQL_READ);
	if ((error = nfsrv_access(vp, VREAD, cred, rdonly, nfsd->nd_procp)) &&
	    (error = nfsrv_access(vp, VEXEC, cred, rdonly, nfsd->nd_procp))) {
		vput(vp);
		nfsm_reply(0);
	}
	if (error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp)) {
		vput(vp);
		nfsm_reply(0);
	}
	if (off >= vap->va_size)
		cnt = 0;
	else if ((off + cnt) > vap->va_size)
		cnt = nfsm_rndup(vap->va_size - off);
	nfsm_reply(NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL)+NFSX_UNSIGNED+nfsm_rndup(cnt));
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_build(tl, u_long *, NFSX_UNSIGNED);
	len = left = cnt;
	if (cnt > 0) {
		/*
		 * Generate the mbuf list with the uio_iov ref. to it.
		 */
		i = 0;
		m = m2 = mb;
		MALLOC(iv, struct iovec *,
		       ((NFS_MAXDATA+MLEN-1)/MLEN) * sizeof (struct iovec),
		       M_TEMP, M_WAITOK);
		iv2 = iv;
		while (left > 0) {
			siz = min(M_TRAILINGSPACE(m), left);
			if (siz > 0) {
				m->m_len += siz;
				iv->iov_base = bpos;
				iv->iov_len = siz;
				iv++;
				i++;
				left -= siz;
			}
			if (left > 0) {
				MGET(m, M_WAIT, MT_DATA);
				MCLGET(m, M_WAIT);
				m->m_len = 0;
				m2->m_next = m;
				m2 = m;
				bpos = mtod(m, caddr_t);
			}
		}
		uiop->uio_iov = iv2;
		uiop->uio_iovcnt = i;
		uiop->uio_offset = off;
		uiop->uio_resid = cnt;
		uiop->uio_rw = UIO_READ;
		uiop->uio_segflg = UIO_SYSSPACE;
		error = VOP_READ(vp, uiop, IO_NODELOCKED, cred);
		off = uiop->uio_offset;
		FREE((caddr_t)iv2, M_TEMP);
		if (error || (error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp))) {
			m_freem(mreq);
			vput(vp);
			nfsm_reply(0);
		}
	} else
		uiop->uio_resid = 0;
	vput(vp);
	nfsm_srvfillattr;
	len -= uiop->uio_resid;
	tlen = nfsm_rndup(len);
	if (cnt != tlen || tlen != len)
		nfsm_adj(mb, cnt-tlen, tlen-len);
	*tl = txdr_unsigned(len);
	nfsm_srvdone;
}

/*
 * nfs write service
 */
nfsrv_write(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct iovec *ivp;
	register struct mbuf *mp;
	register struct nfsv2_fattr *fp;
	struct iovec iv[NFS_MAXIOVEC];
	struct vattr va;
	register struct vattr *vap = &va;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, siz, len, xfer;
	int ioflags = IO_SYNC | IO_NODELOCKED;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	off_t off;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_long *, 4 * NFSX_UNSIGNED);
	if (nfsd->nd_nqlflag == NQL_NOVAL) {
		off = (off_t)fxdr_unsigned(u_long, *++tl);
		tl += 2;
	} else {
		fxdr_hyper(tl, &off);
		tl += 2;
		if (fxdr_unsigned(u_long, *tl++))
			ioflags |= IO_APPEND;
	}
	len = fxdr_unsigned(long, *tl);
	if (len > NFS_MAXDATA || len <= 0) {
		error = EBADRPC;
		nfsm_reply(0);
	}
	if (dpos == (mtod(md, caddr_t)+md->m_len)) {
		mp = md->m_next;
		if (mp == NULL) {
			error = EBADRPC;
			nfsm_reply(0);
		}
	} else {
		mp = md;
		siz = dpos-mtod(mp, caddr_t);
		mp->m_len -= siz;
		NFSMADV(mp, siz);
	}
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	if (vp->v_type != VREG) {
		error = (vp->v_type == VDIR) ? EISDIR : EACCES;
		vput(vp);
		nfsm_reply(0);
	}
	nqsrv_getl(vp, NQL_WRITE);
	if (error = nfsrv_access(vp, VWRITE, cred, rdonly, nfsd->nd_procp)) {
		vput(vp);
		nfsm_reply(0);
	}
	uiop->uio_resid = 0;
	uiop->uio_rw = UIO_WRITE;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_procp = (struct proc *)0;
	/*
	 * Do up to NFS_MAXIOVEC mbufs of write each iteration of the
	 * loop until done.
	 */
	while (len > 0 && uiop->uio_resid == 0) {
		ivp = iv;
		siz = 0;
		uiop->uio_iov = ivp;
		uiop->uio_iovcnt = 0;
		uiop->uio_offset = off;
		while (len > 0 && uiop->uio_iovcnt < NFS_MAXIOVEC && mp != NULL) {
			ivp->iov_base = mtod(mp, caddr_t);
			if (len < mp->m_len)
				ivp->iov_len = xfer = len;
			else
				ivp->iov_len = xfer = mp->m_len;
#ifdef notdef
			/* Not Yet .. */
			if (M_HASCL(mp) && (((u_long)ivp->iov_base) & CLOFSET) == 0)
				ivp->iov_op = NULL;	/* what should it be ?? */
			else
				ivp->iov_op = NULL;
#endif
			uiop->uio_iovcnt++;
			ivp++;
			len -= xfer;
			siz += xfer;
			mp = mp->m_next;
		}
		if (len > 0 && mp == NULL) {
			error = EBADRPC;
			vput(vp);
			nfsm_reply(0);
		}
		uiop->uio_resid = siz;
		if (error = VOP_WRITE(vp, uiop, ioflags, cred)) {
			vput(vp);
			nfsm_reply(0);
		}
		off = uiop->uio_offset;
	}
	error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfillattr;
	if (nfsd->nd_nqlflag != NQL_NOVAL) {
		nfsm_build(tl, u_long *, 2*NFSX_UNSIGNED);
		txdr_hyper(&vap->va_filerev, tl);
	}
	nfsm_srvdone;
}

/*
 * nfs create service
 * now does a truncate to 0 length via. setattr if it already exists
 */
nfsrv_create(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct nfsv2_fattr *fp;
	struct vattr va;
	register struct vattr *vap = &va;
	register struct nfsv2_sattr *sp;
	register u_long *tl;
	struct nameidata nd;
	register caddr_t cp;
	register long t1;
	caddr_t bpos;
	int error = 0, rdev, cache, len, tsize;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	nd.ni_cnd.cn_nameiop = 0;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | SAVESTART;
	if (error = nfs_namei(&nd, fhp, len, nfsd->nd_slp, nam, &md, &dpos,
	    nfsd->nd_procp))
		nfsm_reply(0);
	VATTR_NULL(vap);
	nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_SATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	/*
	 * Iff doesn't exist, create it
	 * otherwise just truncate to 0 length
	 *   should I set the mode too ??
	 */
	if (nd.ni_vp == NULL) {
		vap->va_type = IFTOVT(fxdr_unsigned(u_long, sp->sa_mode));
		if (vap->va_type == VNON)
			vap->va_type = VREG;
		vap->va_mode = nfstov_mode(sp->sa_mode);
		if (nfsd->nd_nqlflag == NQL_NOVAL)
			rdev = fxdr_unsigned(long, sp->sa_nfssize);
		else
			rdev = fxdr_unsigned(long, sp->sa_nqrdev);
		if (vap->va_type == VREG || vap->va_type == VSOCK) {
			vrele(nd.ni_startdir);
			nqsrv_getl(nd.ni_dvp, NQL_WRITE);
			if (error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap))
				nfsm_reply(0);
			FREE(nd.ni_cnd.cn_pnbuf, M_NAMEI);
		} else if (vap->va_type == VCHR || vap->va_type == VBLK ||
			vap->va_type == VFIFO) {
			if (vap->va_type == VCHR && rdev == 0xffffffff)
				vap->va_type = VFIFO;
			if (vap->va_type == VFIFO) {
#ifndef FIFO
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				vput(nd.ni_dvp);
				error = ENXIO;
				goto out;
#endif /* FIFO */
			} else if (error = suser(cred, (u_short *)0)) {
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				vput(nd.ni_dvp);
				goto out;
			} else
				vap->va_rdev = (dev_t)rdev;
			nqsrv_getl(nd.ni_dvp, NQL_WRITE);
			if (error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap)) {
				vrele(nd.ni_startdir);
				nfsm_reply(0);
			}
			nd.ni_cnd.cn_nameiop = LOOKUP;
			nd.ni_cnd.cn_flags &= ~(LOCKPARENT | SAVESTART);
			nd.ni_cnd.cn_proc = nfsd->nd_procp;
			nd.ni_cnd.cn_cred = nfsd->nd_procp->p_ucred;
			if (error = lookup(&nd)) {
				free(nd.ni_cnd.cn_pnbuf, M_NAMEI);
				nfsm_reply(0);
			}
			FREE(nd.ni_cnd.cn_pnbuf, M_NAMEI);
			if (nd.ni_cnd.cn_flags & ISSYMLINK) {
				vrele(nd.ni_dvp);
				vput(nd.ni_vp);
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				error = EINVAL;
				nfsm_reply(0);
			}
		} else {
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
			error = ENXIO;
			goto out;
		}
		vp = nd.ni_vp;
	} else {
		vrele(nd.ni_startdir);
		free(nd.ni_cnd.cn_pnbuf, M_NAMEI);
		vp = nd.ni_vp;
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nfsd->nd_nqlflag == NQL_NOVAL) {
			tsize = fxdr_unsigned(long, sp->sa_nfssize);
			if (tsize != -1)
				vap->va_size = (u_quad_t)tsize;
			else
				vap->va_size = -1;
		} else
			fxdr_hyper(&sp->sa_nqsize, &vap->va_size);
		if (vap->va_size != -1) {
			if (error = nfsrv_access(vp, VWRITE, cred,
			    (nd.ni_cnd.cn_flags & RDONLY), nfsd->nd_procp)) {
				vput(vp);
				nfsm_reply(0);
			}
			nqsrv_getl(vp, NQL_WRITE);
			if (error = VOP_SETATTR(vp, vap, cred, nfsd->nd_procp)) {
				vput(vp);
				nfsm_reply(0);
			}
		}
	}
	bzero((caddr_t)fhp, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	if (error = VFS_VPTOFH(vp, &fhp->fh_fid)) {
		vput(vp);
		nfsm_reply(0);
	}
	error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(NFSX_FH+NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfhtom(fhp);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfillattr;
	return (error);
nfsmout:
	if (nd.ni_cnd.cn_nameiop || nd.ni_cnd.cn_flags)
		vrele(nd.ni_startdir);
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vput(nd.ni_vp);
	return (error);

out:
	vrele(nd.ni_startdir);
	free(nd.ni_cnd.cn_pnbuf, M_NAMEI);
	nfsm_reply(0);
}

/*
 * nfs remove service
 */
nfsrv_remove(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	struct nameidata nd;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, cache, len;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	if (error = nfs_namei(&nd, fhp, len, nfsd->nd_slp, nam, &md, &dpos,
	    nfsd->nd_procp))
		nfsm_reply(0);
	vp = nd.ni_vp;
	if (vp->v_type == VDIR &&
		(error = suser(cred, (u_short *)0)))
		goto out;
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT) {
		error = EBUSY;
		goto out;
	}
	if (vp->v_flag & VTEXT)
		(void) vnode_pager_uncache(vp);
out:
	if (!error) {
		nqsrv_getl(nd.ni_dvp, NQL_WRITE);
		nqsrv_getl(vp, NQL_WRITE);
		error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	nfsm_reply(0);
	nfsm_srvdone;
}

/*
 * nfs rename service
 */
nfsrv_rename(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, cache, len, len2;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct nameidata fromnd, tond;
	struct vnode *fvp, *tvp, *tdvp;
	nfsv2fh_t fnfh, tnfh;
	fhandle_t *ffhp, *tfhp;
	u_quad_t frev;
	uid_t saved_uid;

	ffhp = &fnfh.fh_generic;
	tfhp = &tnfh.fh_generic;
	fromnd.ni_cnd.cn_nameiop = 0;
	tond.ni_cnd.cn_nameiop = 0;
	nfsm_srvmtofh(ffhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	/*
	 * Remember our original uid so that we can reset cr_uid before
	 * the second nfs_namei() call, in case it is remapped.
	 */
	saved_uid = cred->cr_uid;
	fromnd.ni_cnd.cn_cred = cred;
	fromnd.ni_cnd.cn_nameiop = DELETE;
	fromnd.ni_cnd.cn_flags = WANTPARENT | SAVESTART;
	if (error = nfs_namei(&fromnd, ffhp, len, nfsd->nd_slp, nam, &md,
	    &dpos, nfsd->nd_procp))
		nfsm_reply(0);
	fvp = fromnd.ni_vp;
	nfsm_srvmtofh(tfhp);
	nfsm_strsiz(len2, NFS_MAXNAMLEN);
	cred->cr_uid = saved_uid;
	tond.ni_cnd.cn_cred = cred;
	tond.ni_cnd.cn_nameiop = RENAME;
	tond.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART;
	if (error = nfs_namei(&tond, tfhp, len2, nfsd->nd_slp, nam, &md,
	    &dpos, nfsd->nd_procp)) {
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = EISDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = ENOTDIR;
			goto out;
		}
		if (tvp->v_type == VDIR && tvp->v_mountedhere) {
			error = EXDEV;
			goto out;
		}
	}
	if (fvp->v_type == VDIR && fvp->v_mountedhere) {
		error = EBUSY;
		goto out;
	}
	if (fvp->v_mount != tdvp->v_mount) {
		error = EXDEV;
		goto out;
	}
	if (fvp == tdvp)
		error = EINVAL;
	/*
	 * If source is the same as the destination (that is the
	 * same vnode with the same name in the same directory),
	 * then there is nothing to do.
	 */
	if (fvp == tvp && fromnd.ni_dvp == tdvp &&
	    fromnd.ni_cnd.cn_namelen == tond.ni_cnd.cn_namelen &&
	    !bcmp(fromnd.ni_cnd.cn_nameptr, tond.ni_cnd.cn_nameptr,
	      fromnd.ni_cnd.cn_namelen))
		error = -1;
out:
	if (!error) {
		nqsrv_getl(fromnd.ni_dvp, NQL_WRITE);
		nqsrv_getl(tdvp, NQL_WRITE);
		if (tvp)
			nqsrv_getl(tvp, NQL_WRITE);
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
	} else {
		VOP_ABORTOP(tond.ni_dvp, &tond.ni_cnd);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	FREE(tond.ni_cnd.cn_pnbuf, M_NAMEI);
out1:
	vrele(fromnd.ni_startdir);
	FREE(fromnd.ni_cnd.cn_pnbuf, M_NAMEI);
	nfsm_reply(0);
	return (error);

nfsmout:
	if (tond.ni_cnd.cn_nameiop || tond.ni_cnd.cn_flags) {
		vrele(tond.ni_startdir);
		FREE(tond.ni_cnd.cn_pnbuf, M_NAMEI);
	}
	if (fromnd.ni_cnd.cn_nameiop || fromnd.ni_cnd.cn_flags) {
		vrele(fromnd.ni_startdir);
		FREE(fromnd.ni_cnd.cn_pnbuf, M_NAMEI);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	return (error);
}

/*
 * nfs link service
 */
nfsrv_link(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	struct nameidata nd;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, len;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *vp, *xp;
	nfsv2fh_t nfh, dnfh;
	fhandle_t *fhp, *dfhp;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	dfhp = &dnfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvmtofh(dfhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	if (error = nfsrv_fhtovp(fhp, FALSE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	if (vp->v_type == VDIR && (error = suser(cred, (u_short *)0)))
		goto out1;
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	if (error = nfs_namei(&nd, dfhp, len, nfsd->nd_slp, nam, &md, &dpos,
	    nfsd->nd_procp))
		goto out1;
	xp = nd.ni_vp;
	if (xp != NULL) {
		error = EEXIST;
		goto out;
	}
	xp = nd.ni_dvp;
	if (vp->v_mount != xp->v_mount)
		error = EXDEV;
out:
	if (!error) {
		nqsrv_getl(vp, NQL_WRITE);
		nqsrv_getl(xp, NQL_WRITE);
		error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
	}
out1:
	vrele(vp);
	nfsm_reply(0);
	nfsm_srvdone;
}

/*
 * nfs symbolic link service
 */
nfsrv_symlink(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	struct vattr va;
	struct nameidata nd;
	register struct vattr *vap = &va;
	register u_long *tl;
	register long t1;
	struct nfsv2_sattr *sp;
	caddr_t bpos;
	struct uio io;
	struct iovec iv;
	int error = 0, cache, len, len2;
	char *pathcp, *cp2;
	struct mbuf *mb, *mreq;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	pathcp = (char *)0;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	if (error = nfs_namei(&nd, fhp, len, nfsd->nd_slp, nam, &md, &dpos,
	    nfsd->nd_procp))
		goto out;
	nfsm_strsiz(len2, NFS_MAXPATHLEN);
	MALLOC(pathcp, caddr_t, len2 + 1, M_TEMP, M_WAITOK);
	iv.iov_base = pathcp;
	iv.iov_len = len2;
	io.uio_resid = len2;
	io.uio_offset = 0;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_procp = (struct proc *)0;
	nfsm_mtouio(&io, len2);
	nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_SATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	*(pathcp + len2) = '\0';
	if (nd.ni_vp) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		error = EEXIST;
		goto out;
	}
	VATTR_NULL(vap);
	vap->va_mode = fxdr_unsigned(u_short, sp->sa_mode);
	nqsrv_getl(nd.ni_dvp, NQL_WRITE);
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap, pathcp);
out:
	if (pathcp)
		FREE(pathcp, M_TEMP);
	nfsm_reply(0);
	return (error);
nfsmout:
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vrele(nd.ni_vp);
	if (pathcp)
		FREE(pathcp, M_TEMP);
	return (error);
}

/*
 * nfs mkdir service
 */
nfsrv_mkdir(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	struct vattr va;
	register struct vattr *vap = &va;
	register struct nfsv2_fattr *fp;
	struct nameidata nd;
	register caddr_t cp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, cache, len;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	if (error = nfs_namei(&nd, fhp, len, nfsd->nd_slp, nam, &md, &dpos,
	    nfsd->nd_procp))
		nfsm_reply(0);
	nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
	VATTR_NULL(vap);
	vap->va_type = VDIR;
	vap->va_mode = nfstov_mode(*tl++);
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		error = EEXIST;
		nfsm_reply(0);
	}
	nqsrv_getl(nd.ni_dvp, NQL_WRITE);
	if (error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap))
		nfsm_reply(0);
	vp = nd.ni_vp;
	bzero((caddr_t)fhp, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	if (error = VFS_VPTOFH(vp, &fhp->fh_fid)) {
		vput(vp);
		nfsm_reply(0);
	}
	error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(NFSX_FH+NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfhtom(fhp);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR(nfsd->nd_nqlflag != NQL_NOVAL));
	nfsm_srvfillattr;
	return (error);
nfsmout:
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vrele(nd.ni_vp);
	return (error);
}

/*
 * nfs rmdir service
 */
nfsrv_rmdir(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, cache, len;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct nameidata nd;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	if (error = nfs_namei(&nd, fhp, len, nfsd->nd_slp, nam, &md, &dpos,
	    nfsd->nd_procp))
		nfsm_reply(0);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (nd.ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	if (!error) {
		nqsrv_getl(nd.ni_dvp, NQL_WRITE);
		nqsrv_getl(vp, NQL_WRITE);
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	nfsm_reply(0);
	nfsm_srvdone;
}

/*
 * nfs readdir service
 * - mallocs what it thinks is enough to read
 *	count rounded up to a multiple of NFS_DIRBLKSIZ <= NFS_MAXREADDIR
 * - calls VOP_READDIR()
 * - loops around building the reply
 *	if the output generated exceeds count break out of loop
 *	The nfsm_clget macro is used here so that the reply will be packed
 *	tightly in mbuf clusters.
 * - it only knows that it has encountered eof when the VOP_READDIR()
 *	reads nothing
 * - as such one readdir rpc will return eof false although you are there
 *	and then the next will return eof
 * - it trims out records with d_fileno == 0
 *	this doesn't matter for Unix clients, but they might confuse clients
 *	for other os'.
 * NB: It is tempting to set eof to true if the VOP_READDIR() reads less
 *	than requested, but this may not apply to all filesystems. For
 *	example, client NFS does not { although it is never remote mounted
 *	anyhow }
 *     The alternate call nqnfsrv_readdirlook() does lookups as well.
 * PS: The NFS protocol spec. does not clarify what the "count" byte
 *	argument is a count of.. just name strings and file id's or the
 *	entire reply rpc or ...
 *	I tried just file name and id sizes and it confused the Sun client,
 *	so I am using the full rpc size now. The "paranoia.." comment refers
 *	to including the status longwords that are not a part of the dir.
 *	"entry" structures, but are in the rpc.
 */
struct flrep {
	u_long fl_cachable;
	u_long fl_duration;
	u_long fl_frev[2];
	nfsv2fh_t fl_nfh;
	u_long fl_fattr[NFSX_NQFATTR / sizeof (u_long)];
};

nfsrv_readdir(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register char *bp, *be;
	register struct mbuf *mp;
	register struct dirent *dp;
	register caddr_t cp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	struct mbuf *mb, *mb2, *mreq, *mp2;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io;
	struct iovec iv;
	int len, nlen, rem, xfer, tsiz, i, error = 0;
	int siz, cnt, fullsiz, eofflag, rdonly, cache;
	u_quad_t frev;
	u_long on, off, toff;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_long *, 2*NFSX_UNSIGNED);
	toff = fxdr_unsigned(u_long, *tl++);
	off = (toff & ~(NFS_DIRBLKSIZ-1));
	on = (toff & (NFS_DIRBLKSIZ-1));
	cnt = fxdr_unsigned(int, *tl);
	siz = ((cnt+NFS_DIRBLKSIZ-1) & ~(NFS_DIRBLKSIZ-1));
	if (cnt > NFS_MAXREADDIR)
		siz = NFS_MAXREADDIR;
	fullsiz = siz;
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	nqsrv_getl(vp, NQL_READ);
	if (error = nfsrv_access(vp, VEXEC, cred, rdonly, nfsd->nd_procp)) {
		vput(vp);
		nfsm_reply(0);
	}
	VOP_UNLOCK(vp);
	MALLOC(rbuf, caddr_t, siz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_procp = (struct proc *)0;
	error = VOP_READDIR(vp, &io, cred);
	off = (off_t)io.uio_offset;
	if (error) {
		vrele(vp);
		free((caddr_t)rbuf, M_TEMP);
		nfsm_reply(0);
	}
	if (io.uio_resid < fullsiz)
		eofflag = 0;
	else
		eofflag = 1;
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			nfsm_reply(2*NFSX_UNSIGNED);
			nfsm_build(tl, u_long *, 2*NFSX_UNSIGNED);
			*tl++ = nfs_false;
			*tl = nfs_true;
			FREE((caddr_t)rbuf, M_TEMP);
			return (0);
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf + on;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	while (cpos < cend && dp->d_fileno == 0) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	if (cpos >= cend) {
		toff = off;
		siz = fullsiz;
		on = 0;
		goto again;
	}

	cpos = rbuf + on;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	len = 3*NFSX_UNSIGNED;	/* paranoia, probably can be 0 */
	nfsm_reply(siz);
	mp = mp2 = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend) {
		if (dp->d_fileno != 0) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen)-nlen;
			len += (4*NFSX_UNSIGNED + nlen + rem);
			if (len > cnt) {
				eofflag = 0;
				break;
			}
			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_fileno);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(nlen);
			bp += NFSX_UNSIGNED;
	
			/* And loop around copying the name */
			xfer = nlen;
			cp = dp->d_name;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp+xfer) > be)
					tsiz = be-bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			/* And null pad to a long boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';
			nfsm_clget;
	
			/* Finish off the record */
			toff += dp->d_reclen;
			*tl = txdr_unsigned(toff);
			bp += NFSX_UNSIGNED;
		} else
			toff += dp->d_reclen;
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	vrele(vp);
	nfsm_clget;
	*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	nfsm_clget;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	if (mp != mb) {
		if (bp < be)
			mp->m_len = bp - mtod(mp, caddr_t);
	} else
		mp->m_len += bp - bpos;
	FREE(rbuf, M_TEMP);
	nfsm_srvdone;
}

nqnfsrv_readdirlook(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register char *bp, *be;
	register struct mbuf *mp;
	register struct dirent *dp;
	register caddr_t cp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	struct mbuf *mb, *mb2, *mreq, *mp2;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp, *nvp;
	struct flrep fl;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io;
	struct iovec iv;
	struct vattr va, *vap = &va;
	struct nfsv2_fattr *fp;
	int len, nlen, rem, xfer, tsiz, i, error = 0, duration2, cache2;
	int siz, cnt, fullsiz, eofflag, rdonly, cache;
	u_quad_t frev, frev2;
	u_long on, off, toff;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_long *, 3*NFSX_UNSIGNED);
	toff = fxdr_unsigned(u_long, *tl++);
	off = (toff & ~(NFS_DIRBLKSIZ-1));
	on = (toff & (NFS_DIRBLKSIZ-1));
	cnt = fxdr_unsigned(int, *tl++);
	duration2 = fxdr_unsigned(int, *tl);
	siz = ((cnt+NFS_DIRBLKSIZ-1) & ~(NFS_DIRBLKSIZ-1));
	if (cnt > NFS_MAXREADDIR)
		siz = NFS_MAXREADDIR;
	fullsiz = siz;
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	nqsrv_getl(vp, NQL_READ);
	if (error = nfsrv_access(vp, VEXEC, cred, rdonly, nfsd->nd_procp)) {
		vput(vp);
		nfsm_reply(0);
	}
	VOP_UNLOCK(vp);
	MALLOC(rbuf, caddr_t, siz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_procp = (struct proc *)0;
	error = VOP_READDIR(vp, &io, cred);
	off = (u_long)io.uio_offset;
	if (error) {
		vrele(vp);
		free((caddr_t)rbuf, M_TEMP);
		nfsm_reply(0);
	}
	if (io.uio_resid < fullsiz)
		eofflag = 0;
	else
		eofflag = 1;
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			nfsm_reply(2 * NFSX_UNSIGNED);
			nfsm_build(tl, u_long *, 2 * NFSX_UNSIGNED);
			*tl++ = nfs_false;
			*tl = nfs_true;
			FREE((caddr_t)rbuf, M_TEMP);
			return (0);
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf + on;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	while (cpos < cend && dp->d_fileno == 0) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	if (cpos >= cend) {
		toff = off;
		siz = fullsiz;
		on = 0;
		goto again;
	}

	cpos = rbuf + on;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	len = 3 * NFSX_UNSIGNED;	/* paranoia, probably can be 0 */
	nfsm_reply(siz);
	mp = mp2 = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend) {
		if (dp->d_fileno != 0) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen)-nlen;
	
			/*
			 * For readdir_and_lookup get the vnode using
			 * the file number.
			 */
			if (VFS_VGET(vp->v_mount, dp->d_fileno, &nvp))
				goto invalid;
			bzero((caddr_t)&fl.fl_nfh, sizeof (nfsv2fh_t));
			fl.fl_nfh.fh_generic.fh_fsid =
				nvp->v_mount->mnt_stat.f_fsid;
			if (VFS_VPTOFH(nvp, &fl.fl_nfh.fh_generic.fh_fid)) {
				vput(nvp);
				goto invalid;
			}
			if (duration2) {
				(void) nqsrv_getlease(nvp, &duration2, NQL_READ,
					nfsd, nam, &cache2, &frev2, cred);
				fl.fl_duration = txdr_unsigned(duration2);
				fl.fl_cachable = txdr_unsigned(cache2);
				txdr_hyper(&frev2, fl.fl_frev);
			} else
				fl.fl_duration = 0;
			if (VOP_GETATTR(nvp, vap, cred, nfsd->nd_procp)) {
				vput(nvp);
				goto invalid;
			}
			vput(nvp);
			fp = (struct nfsv2_fattr *)&fl.fl_fattr;
			nfsm_srvfillattr;
			len += (4*NFSX_UNSIGNED + nlen + rem + NFSX_FH
				+ NFSX_NQFATTR);
			if (len > cnt) {
				eofflag = 0;
				break;
			}
			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;

			/*
			 * For readdir_and_lookup copy the stuff out.
			 */
			xfer = sizeof (struct flrep);
			cp = (caddr_t)&fl;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp+xfer) > be)
					tsiz = be-bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_fileno);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(nlen);
			bp += NFSX_UNSIGNED;
	
			/* And loop around copying the name */
			xfer = nlen;
			cp = dp->d_name;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp+xfer) > be)
					tsiz = be-bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			/* And null pad to a long boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';
			nfsm_clget;
	
			/* Finish off the record */
			toff += dp->d_reclen;
			*tl = txdr_unsigned(toff);
			bp += NFSX_UNSIGNED;
		} else
invalid:
			toff += dp->d_reclen;
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	vrele(vp);
	nfsm_clget;
	*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	nfsm_clget;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	if (mp != mb) {
		if (bp < be)
			mp->m_len = bp - mtod(mp, caddr_t);
	} else
		mp->m_len += bp - bpos;
	FREE(rbuf, M_TEMP);
	nfsm_srvdone;
}

/*
 * nfs statfs service
 */
nfsrv_statfs(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct statfs *sf;
	register struct nfsv2_statfs *sfp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, isnq;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct statfs statfs;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	isnq = (nfsd->nd_nqlflag != NQL_NOVAL);
	nfsm_srvmtofh(fhp);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	sf = &statfs;
	error = VFS_STATFS(vp->v_mount, sf, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(NFSX_STATFS(isnq));
	nfsm_build(sfp, struct nfsv2_statfs *, NFSX_STATFS(isnq));
	sfp->sf_tsize = txdr_unsigned(NFS_MAXDGRAMDATA);
	sfp->sf_bsize = txdr_unsigned(sf->f_bsize);
	sfp->sf_blocks = txdr_unsigned(sf->f_blocks);
	sfp->sf_bfree = txdr_unsigned(sf->f_bfree);
	sfp->sf_bavail = txdr_unsigned(sf->f_bavail);
	if (isnq) {
		sfp->sf_files = txdr_unsigned(sf->f_files);
		sfp->sf_ffree = txdr_unsigned(sf->f_ffree);
	}
	nfsm_srvdone;
}

/*
 * Null operation, used by clients to ping server
 */
/* ARGSUSED */
nfsrv_null(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	caddr_t bpos;
	int error = VNOVAL, cache;
	struct mbuf *mb, *mreq;
	u_quad_t frev;

	nfsm_reply(0);
	return (error);
}

/*
 * No operation, used for obsolete procedures
 */
/* ARGSUSED */
nfsrv_noop(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	caddr_t bpos;
	int error, cache;
	struct mbuf *mb, *mreq;
	u_quad_t frev;

	if (nfsd->nd_repstat)
		error = nfsd->nd_repstat;
	else
		error = EPROCUNAVAIL;
	nfsm_reply(0);
	return (error);
}

/*
 * Perform access checking for vnodes obtained from file handles that would
 * refer to files already opened by a Unix client. You cannot just use
 * vn_writechk() and VOP_ACCESS() for two reasons.
 * 1 - You must check for exported rdonly as well as MNT_RDONLY for the write case
 * 2 - The owner is to be given access irrespective of mode bits so that
 *     processes that chmod after opening a file don't break. I don't like
 *     this because it opens a security hole, but since the nfs server opens
 *     a security hole the size of a barn door anyhow, what the heck.
 */
nfsrv_access(vp, flags, cred, rdonly, p)
	register struct vnode *vp;
	int flags;
	register struct ucred *cred;
	int rdonly;
	struct proc *p;
{
	struct vattr vattr;
	int error;
	if (flags & VWRITE) {
		/* Just vn_writechk() changed to check rdonly */
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket or a block or character
		 * device resident on the file system.
		 */
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			switch (vp->v_type) {
			case VREG: case VDIR: case VLNK:
				return (EROFS);
			}
		}
		/*
		 * If there's shared text associated with
		 * the inode, try to free it up once.  If
		 * we fail, we can't allow writing.
		 */
		if ((vp->v_flag & VTEXT) && !vnode_pager_uncache(vp))
			return (ETXTBSY);
	}
	if (error = VOP_GETATTR(vp, &vattr, cred, p))
		return (error);
	if ((error = VOP_ACCESS(vp, flags, cred, p)) &&
	    cred->cr_uid != vattr.va_uid)
		return (error);
	return (0);
}
