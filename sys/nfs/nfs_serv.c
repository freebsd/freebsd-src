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
 *	From:	@(#)nfs_serv.c	7.40 (Berkeley) 5/15/91
 *	$Id: nfs_serv.c,v 1.6 1994/06/11 23:33:53 karl Exp $
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

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "file.h"
#include "namei.h"
#include "vnode.h"
#include "mount.h"
#include "mbuf.h"

#include "../ufs/quota.h"
#include "../ufs/inode.h"
#include "../ufs/dir.h"

#include "nfsv2.h"
#include "nfs.h"
#include "xdr_subs.h"
#include "nfsm_subs.h"
#include "nfsnode.h"

/* Defs */
#define	TRUE	1
#define	FALSE	0

/* Global vars */
extern u_long nfs_procids[NFS_NPROCS];
extern u_long nfs_xdrneg1;
extern u_long nfs_false, nfs_true;
nfstype nfs_type[9]={ NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, NFNON,
		      NFCHR, NFNON };

/*
 * nfs getattr service
 */
int
nfsrv_getattr(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
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
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred))
		nfsm_reply(0);
	error = VOP_GETATTR(vp, vap, cred, p);
	vput(vp);
	nfsm_reply(NFSX_FATTR);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR);
	nfsm_srvfillattr;
	nfsm_srvdone;
}

/*
 * nfs setattr service
 */
int
nfsrv_setattr(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
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
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_disect(sp, struct nfsv2_sattr *, NFSX_SATTR);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred))
		nfsm_reply(0);
	if (error = nfsrv_access(vp, VWRITE, cred, p))
		goto out;
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
	if (sp->sa_size != nfs_xdrneg1)
		vap->va_size = fxdr_unsigned(u_long, sp->sa_size);
	/*
	 * The usec field of sa_atime is overloaded with the va_flags field
	 * for 4.4BSD clients. Hopefully other clients always set both the
	 * sec and usec fields to -1 when not setting the atime.
	 */
	if (sp->sa_atime.tv_sec != nfs_xdrneg1) {
		vap->va_atime.tv_sec = fxdr_unsigned(long, sp->sa_atime.tv_sec);
		vap->va_atime.tv_usec = 0;
	}
	if (sp->sa_atime.tv_usec != nfs_xdrneg1)
		vap->va_flags = fxdr_unsigned(u_long, sp->sa_atime.tv_usec);
	if (sp->sa_mtime.tv_sec != nfs_xdrneg1)
		fxdr_time(&sp->sa_mtime, &vap->va_mtime);
	if (error = VOP_SETATTR(vp, vap, cred, p)) {
		vput(vp);
		nfsm_reply(0);
	}
	error = VOP_GETATTR(vp, vap, cred, p);
out:
	vput(vp);
	nfsm_reply(NFSX_FATTR);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR);
	nfsm_srvfillattr;
	nfsm_srvdone;
}

/*
 * nfs lookup rpc
 */
int
nfsrv_lookup(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
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
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	long len;
	struct vattr va, *vap = &va;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cred = cred;
	nd.ni_nameiop = LOOKUP | LOCKLEAF;
	if (error = nfs_namei(&nd, fhp, len, &md, &dpos, p))
		nfsm_reply(0);
	vp = nd.ni_vp;
	bzero((caddr_t)fhp, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	if (error = VFS_VPTOFH(vp, &fhp->fh_fid)) {
		vput(vp);
		nfsm_reply(0);
	}
	error = VOP_GETATTR(vp, vap, cred, p);
	vput(vp);
	nfsm_reply(NFSX_FH+NFSX_FATTR);
	nfsm_srvfhtom(fhp);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR);
	nfsm_srvfillattr;
	nfsm_srvdone;
}

/*
 * nfs readlink service
 */
int
nfsrv_readlink(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	struct iovec iv[(NFS_MAXPATHLEN+MLEN-1)/MLEN];
	register struct iovec *ivp = iv;
	register struct mbuf *mp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mp2 = 0, *mp3 = 0, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	int i, tlen, len;

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
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred)) {
		m_freem(mp3);
		nfsm_reply(0);
	}
	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto out;
	}
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
int
nfsrv_read(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	register struct iovec *iv;
	struct iovec *iv2;
	register struct mbuf *m;
	register struct nfsv2_fattr *fp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct mbuf *m2 = 0, *m3;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	struct vattr va, *vap = &va;
	int i, cnt, len, left, siz, tlen;
	off_t off;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_disect(tl, u_long *, NFSX_UNSIGNED);
	off = fxdr_unsigned(off_t, *tl);
	nfsm_srvstrsiz(cnt, NFS_MAXDATA);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred))
		nfsm_reply(0);
	if ((error = nfsrv_access(vp, VREAD, cred, p)) &&
		(error = nfsrv_access(vp, VEXEC, cred, p))) {
		vput(vp);
		nfsm_reply(0);
	}
	len = left = cnt;
	/*
	 * Generate the mbuf list with the uio_iov ref. to it.
	 */
	i = 0;
	m3 = (struct mbuf *)0;
#ifdef lint
	m2 = (struct mbuf *)0;
#endif /* lint */
	MALLOC(iv, struct iovec *,
	       ((NFS_MAXDATA+MLEN-1)/MLEN) * sizeof (struct iovec), M_TEMP,
	       M_WAITOK);
	iv2 = iv;
	while (left > 0) {
		MGET(m, M_WAIT, MT_DATA);
		if (left > MINCLSIZE)
			MCLGET(m, M_WAIT);
		m->m_len = 0;
		siz = min(M_TRAILINGSPACE(m), left);
		m->m_len = siz;
		iv->iov_base = mtod(m, caddr_t);
		iv->iov_len = siz;
		iv++;
		i++;
		left -= siz;
		if (m3) {
			m2->m_next = m;
			m2 = m;
		} else
			m3 = m2 = m;
	}
	uiop->uio_iov = iv2;
	uiop->uio_iovcnt = i;
	uiop->uio_offset = off;
	uiop->uio_resid = cnt;
	uiop->uio_rw = UIO_READ;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_procp = (struct proc *)0;
	error = VOP_READ(vp, uiop, IO_NODELOCKED, cred);
	off = uiop->uio_offset;
	FREE((caddr_t)iv2, M_TEMP);
	if (error) {
		m_freem(m3);
		vput(vp);
		nfsm_reply(0);
	}
	if (error = VOP_GETATTR(vp, vap, cred, p))
		m_freem(m3);
	vput(vp);
	nfsm_reply(NFSX_FATTR+NFSX_UNSIGNED);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR);
	nfsm_srvfillattr;
	len -= uiop->uio_resid;
	if (len > 0) {
		tlen = nfsm_rndup(len);
		if (cnt != tlen || tlen != len)
			nfsm_adj(m3, cnt-tlen, tlen-len);
	} else {
		m_freem(m3);
		m3 = (struct mbuf *)0;
	}
	nfsm_build(tl, u_long *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(len);
	mb->m_next = m3;
	nfsm_srvdone;
}

/*
 * nfs write service
 */
int
nfsrv_write(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
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
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	off_t off;
	long siz, len, xfer;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_disect(tl, u_long *, 4*NFSX_UNSIGNED);
	off = fxdr_unsigned(off_t, *++tl);
	tl += 2;
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
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred))
		nfsm_reply(0);
	if (error = nfsrv_access(vp, VWRITE, cred, p)) {
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
		if (error = VOP_WRITE(vp, uiop, IO_SYNC | IO_NODELOCKED,
			cred)) {
			vput(vp);
			nfsm_reply(0);
		}
		off = uiop->uio_offset;
	}
	error = VOP_GETATTR(vp, vap, cred, p);
	vput(vp);
	nfsm_reply(NFSX_FATTR);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR);
	nfsm_srvfillattr;
	nfsm_srvdone;
}

/*
 * nfs create service
 * if it already exists, just set length		* 28 Aug 92*
 * do NOT truncate unconditionally !
 */
int
nfsrv_create(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	register struct nfsv2_fattr *fp;
	struct vattr va;
	register struct vattr *vap = &va;
	struct nameidata nd;
	register caddr_t cp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	long rdev;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	long len;

	nd.ni_nameiop = 0;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cred = cred;
	nd.ni_nameiop = CREATE | LOCKPARENT | LOCKLEAF | SAVESTART;
	if (error = nfs_namei(&nd, fhp, len, &md, &dpos, p))
		nfsm_reply(0);
	VATTR_NULL(vap);
	nfsm_disect(tl, u_long *, NFSX_SATTR);
	/*
	 * If it doesn't exist, create it		* 28 Aug 92*
	 * otherwise just set length from attributes
	 *   should I set the mode too ??
	 */
	if (nd.ni_vp == NULL) {
		vap->va_type = IFTOVT(fxdr_unsigned(u_long, *tl));
		if (vap->va_type == VNON)
			vap->va_type = VREG;
		vap->va_mode = nfstov_mode(*tl);
		rdev = fxdr_unsigned(long, *(tl+3));
		if (vap->va_type == VREG || vap->va_type == VSOCK) {
			vrele(nd.ni_startdir);
			if (error = VOP_CREATE(&nd, vap, p))
				nfsm_reply(0);
			FREE(nd.ni_pnbuf, M_NAMEI);
		} else if (vap->va_type == VCHR || vap->va_type == VBLK ||
			vap->va_type == VFIFO) {
			if (vap->va_type == VCHR && rdev == 0xffffffffUL)
				vap->va_type = VFIFO;
			if (vap->va_type == VFIFO) {
#ifndef FIFO
				VOP_ABORTOP(&nd);
				vput(nd.ni_dvp);
				error = ENXIO;
				goto out;
#endif /* FIFO */
			} else if (error = suser(cred, (u_short *)0)) {
				VOP_ABORTOP(&nd);
				vput(nd.ni_dvp);
				goto out;
			} else
				vap->va_rdev = (dev_t)rdev;
			if (error = VOP_MKNOD(&nd, vap, cred, p)) {
				vrele(nd.ni_startdir);
				nfsm_reply(0);
			}
			nd.ni_nameiop &= ~(OPMASK | LOCKPARENT | SAVESTART);
			nd.ni_nameiop |= LOOKUP;
			if (error = lookup(&nd, p)) {
				free(nd.ni_pnbuf, M_NAMEI);
				nfsm_reply(0);
			}
			FREE(nd.ni_pnbuf, M_NAMEI);
			if (nd.ni_more) {
				vrele(nd.ni_dvp);
				vput(nd.ni_vp);
				VOP_ABORTOP(&nd);
				error = EINVAL;
				nfsm_reply(0);
			}
		} else {
			VOP_ABORTOP(&nd);
			vput(nd.ni_dvp);
			error = ENXIO;
			goto out;
		}
		vp = nd.ni_vp;
	} else {
		vrele(nd.ni_startdir);
		free(nd.ni_pnbuf, M_NAMEI);
		vp = nd.ni_vp;
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		VOP_ABORTOP(&nd);
		vap->va_size = fxdr_unsigned(long, *(tl+3));	/* 28 Aug 92*/
		if (vap->va_size != -1) {
			if (error = nfsrv_access(vp, VWRITE, cred, p)) {
				vput(vp);
				nfsm_reply(0);
			}
			if (error = VOP_SETATTR(vp, vap, cred, p)) {
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
	error = VOP_GETATTR(vp, vap, cred, p);
	vput(vp);
	nfsm_reply(NFSX_FH+NFSX_FATTR);
	nfsm_srvfhtom(fhp);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR);
	nfsm_srvfillattr;
	return (error);
nfsmout:
	if (nd.ni_nameiop)
		vrele(nd.ni_startdir);
	VOP_ABORTOP(&nd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vput(nd.ni_vp);
	return (error);

out:
	vrele(nd.ni_startdir);
	free(nd.ni_pnbuf, M_NAMEI);
	nfsm_reply(0);
	return 0;
}

/*
 * nfs remove service
 */
int
nfsrv_remove(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	struct nameidata nd;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	long len;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cred = cred;
	nd.ni_nameiop = DELETE | LOCKPARENT | LOCKLEAF;
	if (error = nfs_namei(&nd, fhp, len, &md, &dpos, p))
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
		error = VOP_REMOVE(&nd, p);
	} else {
		VOP_ABORTOP(&nd);
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
int
nfsrv_rename(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct nameidata fromnd, tond;
	struct vnode *fvp = 0, *tvp, *tdvp;
	nfsv2fh_t fnfh, tnfh;
	fhandle_t *ffhp, *tfhp;
	long len, len2;
	int rootflg = 0;

	ffhp = &fnfh.fh_generic;
	tfhp = &tnfh.fh_generic;
	fromnd.ni_nameiop = 0;
	tond.ni_nameiop = 0;
	nfsm_srvmtofh(ffhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	/*
	 * Remember if we are root so that we can reset cr_uid before
	 * the second nfs_namei() call
	 */
	if (cred->cr_uid == 0)
		rootflg++;
	fromnd.ni_cred = cred;
	fromnd.ni_nameiop = DELETE | WANTPARENT | SAVESTART;
	if (error = nfs_namei(&fromnd, ffhp, len, &md, &dpos, p))
		nfsm_reply(0);
	fvp = fromnd.ni_vp;
	nfsm_srvmtofh(tfhp);
	nfsm_strsiz(len2, NFS_MAXNAMLEN);
	if (rootflg)
		cred->cr_uid = 0;
	tond.ni_cred = cred;
	tond.ni_nameiop = RENAME | LOCKPARENT | LOCKLEAF | NOCACHE
		| SAVESTART;
	if (error = nfs_namei(&tond, tfhp, len2, &md, &dpos, p)) {
		VOP_ABORTOP(&fromnd);
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
	    fromnd.ni_namelen == tond.ni_namelen &&
	    !bcmp(fromnd.ni_ptr, tond.ni_ptr, fromnd.ni_namelen))
		error = -1;
out:
	if (!error) {
		error = VOP_RENAME(&fromnd, &tond, p);
	} else {
		VOP_ABORTOP(&tond);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(&fromnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	FREE(tond.ni_pnbuf, M_NAMEI);
out1:
	vrele(fromnd.ni_startdir);
	FREE(fromnd.ni_pnbuf, M_NAMEI);
	nfsm_reply(0);
	return (error);

nfsmout:
	if (tond.ni_nameiop) {
		vrele(tond.ni_startdir);
		FREE(tond.ni_pnbuf, M_NAMEI);
	}
	if (fromnd.ni_nameiop) {
		vrele(fromnd.ni_startdir);
		FREE(fromnd.ni_pnbuf, M_NAMEI);
		VOP_ABORTOP(&fromnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	return (error);
}

/*
 * nfs link service
 */
int
nfsrv_link(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	struct nameidata nd;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *vp, *xp;
	nfsv2fh_t nfh, dnfh;
	fhandle_t *fhp, *dfhp;
	long len;

	fhp = &nfh.fh_generic;
	dfhp = &dnfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvmtofh(dfhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	if (error = nfsrv_fhtovp(fhp, FALSE, &vp, cred))
		nfsm_reply(0);
	if (vp->v_type == VDIR && (error = suser(cred, NULL)))
		goto out1;
	nd.ni_cred = cred;
	nd.ni_nameiop = CREATE | LOCKPARENT;
	if (error = nfs_namei(&nd, dfhp, len, &md, &dpos, p))
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
		error = VOP_LINK(vp, &nd, p);
	} else {
		VOP_ABORTOP(&nd);
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
int
nfsrv_symlink(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
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
	int error = 0;
	char *pathcp, *cp2;
	struct mbuf *mb, *mreq;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	long len, len2;

	pathcp = (char *)0;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cred = cred;
	nd.ni_nameiop = CREATE | LOCKPARENT;
	if (error = nfs_namei(&nd, fhp, len, &md, &dpos, p))
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
	nfsm_disect(sp, struct nfsv2_sattr *, NFSX_SATTR);
	*(pathcp + len2) = '\0';
	if (nd.ni_vp) {
		VOP_ABORTOP(&nd);
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
	error = VOP_SYMLINK(&nd, vap, pathcp, p);
out:
	if (pathcp)
		FREE(pathcp, M_TEMP);
	nfsm_reply(0);
	return (error);
nfsmout:
	VOP_ABORTOP(&nd);
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
int
nfsrv_mkdir(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	struct vattr va;
	register struct vattr *vap = &va;
	register struct nfsv2_fattr *fp;
	struct nameidata nd;
	register caddr_t cp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	long len;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cred = cred;
	nd.ni_nameiop = CREATE | LOCKPARENT;
	if (error = nfs_namei(&nd, fhp, len, &md, &dpos, p))
		nfsm_reply(0);
	nfsm_disect(tl, u_long *, NFSX_UNSIGNED);
	VATTR_NULL(vap);
	vap->va_type = VDIR;
	vap->va_mode = nfstov_mode(*tl++);
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(&nd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		error = EEXIST;
		nfsm_reply(0);
	}
	if (error = VOP_MKDIR(&nd, vap, p))
		nfsm_reply(0);
	vp = nd.ni_vp;
	bzero((caddr_t)fhp, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	if (error = VFS_VPTOFH(vp, &fhp->fh_fid)) {
		vput(vp);
		nfsm_reply(0);
	}
	error = VOP_GETATTR(vp, vap, cred, p);
	vput(vp);
	nfsm_reply(NFSX_FH+NFSX_FATTR);
	nfsm_srvfhtom(fhp);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_FATTR);
	nfsm_srvfillattr;
	return (error);
nfsmout:
	VOP_ABORTOP(&nd);
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
int
nfsrv_rmdir(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf *mrep, *md, **mrq;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	long len;
	struct nameidata nd;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvstrsiz(len, NFS_MAXNAMLEN);
	nd.ni_cred = cred;
	nd.ni_nameiop = DELETE | LOCKPARENT | LOCKLEAF;
	if (error = nfs_namei(&nd, fhp, len, &md, &dpos, p))
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
		error = VOP_RMDIR(&nd, p);
	} else {
		VOP_ABORTOP(&nd);
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
 * - it trims out records with d_ino == 0
 *	this doesn't matter for Unix clients, but they might confuse clients
 *	for other os'.
 * NB: It is tempting to set eof to true if the VOP_READDIR() reads less
 *	than requested, but this may not apply to all filesystems. For
 *	example, client NFS does not { although it is never remote mounted
 *	anyhow }
 * PS: The NFS protocol spec. does not clarify what the "count" byte
 *	argument is a count of.. just name strings and file id's or the
 *	entire reply rpc or ...
 *	I tried just file name and id sizes and it confused the Sun client,
 *	so I am using the full rpc size now. The "paranoia.." comment refers
 *	to including the status longwords that are not a part of the dir.
 *	"entry" structures, but are in the rpc.
 */
int
nfsrv_readdir(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	register char *bp, *be;
	register struct mbuf *mp = 0;
	register struct direct *dp;
	register caddr_t cp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	char *cpos, *cend;
	int len, nlen, rem, xfer, tsiz, i;
	struct vnode *vp;
	struct mbuf *mp2 = 0, *mp3;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct uio io;
	struct iovec iv;
	int siz, cnt, fullsiz, eofflag;
	u_long on;
	char *rbuf;
	off_t off, toff;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_disect(tl, u_long *, 2*NFSX_UNSIGNED);
	toff = fxdr_unsigned(off_t, *tl++);
	off = (toff & ~(NFS_DIRBLKSIZ-1));
	on = (toff & (NFS_DIRBLKSIZ-1));
	cnt = fxdr_unsigned(int, *tl);
	siz = ((cnt+NFS_DIRBLKSIZ-1) & ~(NFS_DIRBLKSIZ-1));
	if (cnt > NFS_MAXREADDIR)
		siz = NFS_MAXREADDIR;
	fullsiz = siz;
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred))
		nfsm_reply(0);
	if (error = nfsrv_access(vp, VEXEC, cred, p)) {
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
	io.uio_offset = off;
	io.uio_resid = fullsiz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_procp = (struct proc *)0;
	error = VOP_READDIR(vp, &io, cred, &eofflag);
	off = io.uio_offset;
	if (error) {
		vrele(vp);
		free((caddr_t)rbuf, M_TEMP);
		nfsm_reply(0);
	}
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
	dp = (struct direct *)cpos;
	while (cpos < cend && dp->d_ino == 0) {
		cpos += dp->d_reclen;
		dp = (struct direct *)cpos;
	}
	if (cpos >= cend) {
		toff = off;
		siz = fullsiz;
		on = 0;
		goto again;
	}

	cpos = rbuf + on;
	cend = rbuf + siz;
	dp = (struct direct *)cpos;
	vrele(vp);
	len = 3*NFSX_UNSIGNED;	/* paranoia, probably can be 0 */
	bp = be = (caddr_t)0;
	mp3 = (struct mbuf *)0;
	nfsm_reply(siz);

	/* Loop through the records and build reply */
	while (cpos < cend) {
		if (dp->d_ino != 0) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen)-nlen;
	
			/*
			 * As noted above, the NFS spec. is not clear about what
			 * should be included in "count" as totalled up here in
			 * "len".
			 */
			len += (4*NFSX_UNSIGNED+nlen+rem);
			if (len > cnt) {
				eofflag = 0;
				break;
			}
	
			/* Build the directory record xdr from the direct entry */
			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_ino);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(nlen);
			bp += NFSX_UNSIGNED;
	
			/* And loop arround copying the name */
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
		dp = (struct direct *)cpos;
	}
	nfsm_clget;
	*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	nfsm_clget;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	if (bp < be)
		mp->m_len = bp-mtod(mp, caddr_t);
	mb->m_next = mp3;
	FREE(rbuf, M_TEMP);
	nfsm_srvdone;
}

/*
 * nfs statfs service
 */
int
nfsrv_statfs(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	register struct statfs *sf;
	register struct nfsv2_statfs *sfp;
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct statfs statfs;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (error = nfsrv_fhtovp(fhp, TRUE, &vp, cred))
		nfsm_reply(0);
	sf = &statfs;
	error = VFS_STATFS(vp->v_mount, sf, p);
	vput(vp);
	nfsm_reply(NFSX_STATFS);
	nfsm_build(sfp, struct nfsv2_statfs *, NFSX_STATFS);
	sfp->sf_tsize = txdr_unsigned(NFS_MAXDGRAMDATA);
	sfp->sf_bsize = txdr_unsigned(sf->f_fsize);
	sfp->sf_blocks = txdr_unsigned(sf->f_blocks);
	sfp->sf_bfree = txdr_unsigned(sf->f_bfree);
	sfp->sf_bavail = txdr_unsigned(sf->f_bavail);
	nfsm_srvdone;
}

/*
 * Null operation, used by clients to ping server
 */
/* ARGSUSED */
int
nfsrv_null(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	caddr_t bpos;
	int error = 0;
	struct mbuf *mb, *mreq;

	error = VNOVAL;
	nfsm_reply(0);
	return (error);
}

/*
 * No operation, used for obsolete procedures
 */
/* ARGSUSED */
int
nfsrv_noop(mrep, md, dpos, cred, xid, mrq, repstat, p)
	struct mbuf **mrq;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	u_long xid;
	int *repstat;
	struct proc *p;
{
	caddr_t bpos;
	int error;					/* 08 Sep 92*/
	struct mbuf *mb, *mreq;

	if (*repstat)					/* 08 Sep 92*/
		error = *repstat;
	else
		error = EPROCUNAVAIL;
	nfsm_reply(0);
	return (error);
}

/*
 * Perform access checking for vnodes obtained from file handles that would
 * refer to files already opened by a Unix client. You cannot just use
 * vn_writechk() and VOP_ACCESS() for two reasons.
 * 1 - You must check for MNT_EXRDONLY as well as MNT_RDONLY for the write case
 * 2 - The owner is to be given access irrespective of mode bits so that
 *     processes that chmod after opening a file don't break. I don't like
 *     this because it opens a security hole, but since the nfs server opens
 *     a security hole the size of a barn door anyhow, what the heck.
 */
int
nfsrv_access(vp, flags, cred, p)
	register struct vnode *vp;
	int flags;
	register struct ucred *cred;
	struct proc *p;
{
	struct vattr vattr;
	int error;
	if (flags & VWRITE) {
		/* Just vn_writechk() changed to check MNT_EXRDONLY */
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket or a block or character
		 * device resident on the file system.
		 */
		if (vp->v_mount->mnt_flag & (MNT_RDONLY | MNT_EXRDONLY)) {
			switch (vp->v_type) {
			case VREG: case VDIR: case VLNK:
				return (EROFS);
			      default:
				;
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
