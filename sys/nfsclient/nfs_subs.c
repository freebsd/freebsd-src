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
 *	@(#)nfs_subs.c  8.8 (Berkeley) 5/22/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsmount.h>

#include <netinet/in.h>

/*
 * Data items converted to xdr at startup, since they are constant
 * This is kinda hokey, but may save a little time doing byte swaps
 */
u_int32_t	nfs_xdrneg1;
u_int32_t	rpc_call, rpc_vers, rpc_reply, rpc_msgdenied, rpc_autherr,
		    rpc_mismatch, rpc_auth_unix, rpc_msgaccepted;
u_int32_t	nfs_true, nfs_false;

/* And other global data */
static u_int32_t nfs_xid = 0;
static enum vtype nv2tov_type[8]= {
	VNON, VREG, VDIR, VBLK, VCHR, VLNK, VNON,  VNON
};

int		nfs_ticks;
int		nfs_pbuf_freecnt = -1;	/* start out unlimited */

struct nfs_reqq	nfs_reqq;
struct nfs_bufq	nfs_bufq;

static int	nfs_prev_nfsclnt_sy_narg;
static sy_call_t *nfs_prev_nfsclnt_sy_call;

/*
 * and the reverse mapping from generic to Version 2 procedure numbers
 */
int nfsv2_procid[NFS_NPROCS] = {
	NFSV2PROC_NULL,
	NFSV2PROC_GETATTR,
	NFSV2PROC_SETATTR,
	NFSV2PROC_LOOKUP,
	NFSV2PROC_NOOP,
	NFSV2PROC_READLINK,
	NFSV2PROC_READ,
	NFSV2PROC_WRITE,
	NFSV2PROC_CREATE,
	NFSV2PROC_MKDIR,
	NFSV2PROC_SYMLINK,
	NFSV2PROC_CREATE,
	NFSV2PROC_REMOVE,
	NFSV2PROC_RMDIR,
	NFSV2PROC_RENAME,
	NFSV2PROC_LINK,
	NFSV2PROC_READDIR,
	NFSV2PROC_NOOP,
	NFSV2PROC_STATFS,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
};

LIST_HEAD(nfsnodehashhead, nfsnode);

/*
 * Create the header for an rpc request packet
 * The hsiz is the size of the rest of the nfs request header.
 * (just used to decide if a cluster is a good idea)
 */
struct mbuf *
nfsm_reqhead(struct vnode *vp, u_long procid, int hsiz)
{
	struct mbuf *mb;

	MGET(mb, M_TRYWAIT, MT_DATA);
	if (hsiz >= MINCLSIZE)
		MCLGET(mb, M_TRYWAIT);
	mb->m_len = 0;
	return (mb);
}

/*
 * Build the RPC header and fill in the authorization info.
 * The authorization string argument is only used when the credentials
 * come from outside of the kernel.
 * Returns the head of the mbuf list.
 */
struct mbuf *
nfsm_rpchead(struct ucred *cr, int nmflag, int procid, int auth_type,
    int auth_len, struct mbuf *mrest, int mrest_len, struct mbuf **mbp,
    u_int32_t *xidp)
{
	struct mbuf *mb;
	u_int32_t *tl;
	caddr_t bpos;
	int i;
	struct mbuf *mreq;
	int grpsiz, authsiz;

	authsiz = nfsm_rndup(auth_len);
	MGETHDR(mb, M_TRYWAIT, MT_DATA);
	if ((authsiz + 10 * NFSX_UNSIGNED) >= MINCLSIZE) {
		MCLGET(mb, M_TRYWAIT);
	} else if ((authsiz + 10 * NFSX_UNSIGNED) < MHLEN) {
		MH_ALIGN(mb, authsiz + 10 * NFSX_UNSIGNED);
	} else {
		MH_ALIGN(mb, 8 * NFSX_UNSIGNED);
	}
	mb->m_len = 0;
	mreq = mb;
	bpos = mtod(mb, caddr_t);

	/*
	 * First the RPC header.
	 */
	tl = nfsm_build(u_int32_t *, 8 * NFSX_UNSIGNED);

	/* Get a pretty random xid to start with */
	if (!nfs_xid)
		nfs_xid = random();
	/*
	 * Skip zero xid if it should ever happen.
	 */
	if (++nfs_xid == 0)
		nfs_xid++;

	*tl++ = *xidp = txdr_unsigned(nfs_xid);
	*tl++ = rpc_call;
	*tl++ = rpc_vers;
	*tl++ = txdr_unsigned(NFS_PROG);
	if (nmflag & NFSMNT_NFSV3) {
		*tl++ = txdr_unsigned(NFS_VER3);
		*tl++ = txdr_unsigned(procid);
	} else {
		*tl++ = txdr_unsigned(NFS_VER2);
		*tl++ = txdr_unsigned(nfsv2_procid[procid]);
	}

	/*
	 * And then the authorization cred.
	 */
	*tl++ = txdr_unsigned(auth_type);
	*tl = txdr_unsigned(authsiz);
	switch (auth_type) {
	case RPCAUTH_UNIX:
		tl = nfsm_build(u_int32_t *, auth_len);
		*tl++ = 0;		/* stamp ?? */
		*tl++ = 0;		/* NULL hostname */
		*tl++ = txdr_unsigned(cr->cr_uid);
		*tl++ = txdr_unsigned(cr->cr_groups[0]);
		grpsiz = (auth_len >> 2) - 5;
		*tl++ = txdr_unsigned(grpsiz);
		for (i = 1; i <= grpsiz; i++)
			*tl++ = txdr_unsigned(cr->cr_groups[i]);
		break;
	}

	/*
	 * And the verifier...
	 */
	tl = nfsm_build(u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(RPCAUTH_NULL);
	*tl = 0;
	mb->m_next = mrest;
	mreq->m_pkthdr.len = authsiz + 10 * NFSX_UNSIGNED + mrest_len;
	mreq->m_pkthdr.rcvif = NULL;
	*mbp = mb;
	return (mreq);
}

/*
 * copies a uio scatter/gather list to an mbuf chain.
 * NOTE: can ony handle iovcnt == 1
 */
int
nfsm_uiotombuf(struct uio *uiop, struct mbuf **mq, int siz, caddr_t *bpos)
{
	char *uiocp;
	struct mbuf *mp, *mp2;
	int xfer, left, mlen;
	int uiosiz, clflg, rem;
	char *cp;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfsm_uiotombuf: iovcnt != 1");
#endif

	if (siz > MLEN)		/* or should it >= MCLBYTES ?? */
		clflg = 1;
	else
		clflg = 0;
	rem = nfsm_rndup(siz)-siz;
	mp = mp2 = *mq;
	while (siz > 0) {
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			mlen = M_TRAILINGSPACE(mp);
			if (mlen == 0) {
				MGET(mp, M_TRYWAIT, MT_DATA);
				if (clflg)
					MCLGET(mp, M_TRYWAIT);
				mp->m_len = 0;
				mp2->m_next = mp;
				mp2 = mp;
				mlen = M_TRAILINGSPACE(mp);
			}
			xfer = (left > mlen) ? mlen : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
				bcopy(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			else
				copyin(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			mp->m_len += xfer;
			left -= xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		uiop->uio_iov->iov_base =
		    (char *)uiop->uio_iov->iov_base + uiosiz;
		uiop->uio_iov->iov_len -= uiosiz;
		siz -= uiosiz;
	}
	if (rem > 0) {
		if (rem > M_TRAILINGSPACE(mp)) {
			MGET(mp, M_TRYWAIT, MT_DATA);
			mp->m_len = 0;
			mp2->m_next = mp;
		}
		cp = mtod(mp, caddr_t)+mp->m_len;
		for (left = 0; left < rem; left++)
			*cp++ = '\0';
		mp->m_len += rem;
		*bpos = cp;
	} else
		*bpos = mtod(mp, caddr_t)+mp->m_len;
	*mq = mp;
	return (0);
}

/*
 * Copy a string into mbufs for the hard cases...
 */
int
nfsm_strtmbuf(struct mbuf **mb, char **bpos, const char *cp, long siz)
{
	struct mbuf *m1 = NULL, *m2;
	long left, xfer, len, tlen;
	u_int32_t *tl;
	int putsize;

	putsize = 1;
	m2 = *mb;
	left = M_TRAILINGSPACE(m2);
	if (left > 0) {
		tl = ((u_int32_t *)(*bpos));
		*tl++ = txdr_unsigned(siz);
		putsize = 0;
		left -= NFSX_UNSIGNED;
		m2->m_len += NFSX_UNSIGNED;
		if (left > 0) {
			bcopy(cp, (caddr_t) tl, left);
			siz -= left;
			cp += left;
			m2->m_len += left;
			left = 0;
		}
	}
	/* Loop around adding mbufs */
	while (siz > 0) {
		MGET(m1, M_TRYWAIT, MT_DATA);
		if (siz > MLEN)
			MCLGET(m1, M_TRYWAIT);
		m1->m_len = NFSMSIZ(m1);
		m2->m_next = m1;
		m2 = m1;
		tl = mtod(m1, u_int32_t *);
		tlen = 0;
		if (putsize) {
			*tl++ = txdr_unsigned(siz);
			m1->m_len -= NFSX_UNSIGNED;
			tlen = NFSX_UNSIGNED;
			putsize = 0;
		}
		if (siz < m1->m_len) {
			len = nfsm_rndup(siz);
			xfer = siz;
			if (xfer < len)
				*(tl+(xfer>>2)) = 0;
		} else {
			xfer = len = m1->m_len;
		}
		bcopy(cp, (caddr_t) tl, xfer);
		m1->m_len = len+tlen;
		siz -= xfer;
		cp += xfer;
	}
	*mb = m1;
	*bpos = mtod(m1, caddr_t)+m1->m_len;
	return (0);
}

/*
 * Called once to initialize data structures...
 */
int
nfs_init(struct vfsconf *vfsp)
{
	int i;

	nfsmount_zone = uma_zcreate("NFSMOUNT", sizeof(struct nfsmount),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	rpc_vers = txdr_unsigned(RPC_VER2);
	rpc_call = txdr_unsigned(RPC_CALL);
	rpc_reply = txdr_unsigned(RPC_REPLY);
	rpc_msgdenied = txdr_unsigned(RPC_MSGDENIED);
	rpc_msgaccepted = txdr_unsigned(RPC_MSGACCEPTED);
	rpc_mismatch = txdr_unsigned(RPC_MISMATCH);
	rpc_autherr = txdr_unsigned(RPC_AUTHERR);
	rpc_auth_unix = txdr_unsigned(RPCAUTH_UNIX);
	nfs_true = txdr_unsigned(TRUE);
	nfs_false = txdr_unsigned(FALSE);
	nfs_xdrneg1 = txdr_unsigned(-1);
	nfs_ticks = (hz * NFS_TICKINTVL + 500) / 1000;
	if (nfs_ticks < 1)
		nfs_ticks = 1;
	/* Ensure async daemons disabled */
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++) {
		nfs_iodwant[i] = NULL;
		nfs_iodmount[i] = NULL;
	}
	nfs_nhinit();			/* Init the nfsnode table */

	/*
	 * Initialize reply list and start timer
	 */
	TAILQ_INIT(&nfs_reqq);

	nfs_timer(0);

	nfs_prev_nfsclnt_sy_narg = sysent[SYS_nfsclnt].sy_narg;
	sysent[SYS_nfsclnt].sy_narg = 2;
	nfs_prev_nfsclnt_sy_call = sysent[SYS_nfsclnt].sy_call;
	sysent[SYS_nfsclnt].sy_call = (sy_call_t *)nfsclnt;

	nfs_pbuf_freecnt = nswbuf / 2 + 1;

	return (0);
}

int
nfs_uninit(struct vfsconf *vfsp)
{

	untimeout(nfs_timer, (void *)NULL, nfs_timer_handle);
	sysent[SYS_nfsclnt].sy_narg = nfs_prev_nfsclnt_sy_narg;
	sysent[SYS_nfsclnt].sy_call = nfs_prev_nfsclnt_sy_call;
	return (0);
}

/*
 * Attribute cache routines.
 * nfs_loadattrcache() - loads or updates the cache contents from attributes
 *	that are on the mbuf list
 * nfs_getattrcache() - returns valid attributes if found in cache, returns
 *	error otherwise
 */

/*
 * Load the attribute cache (that lives in the nfsnode entry) with
 * the values on the mbuf list and
 * Iff vap not NULL
 *    copy the attributes to *vaper
 */
int
nfs_loadattrcache(struct vnode **vpp, struct mbuf **mdp, caddr_t *dposp,
    struct vattr *vaper, int dontshrink)
{
	struct vnode *vp = *vpp;
	struct vattr *vap;
	struct nfs_fattr *fp;
	struct nfsnode *np;
	int32_t t1;
	caddr_t cp2;
	int rdev;
	struct mbuf *md;
	enum vtype vtyp;
	u_short vmode;
	struct timespec mtime;
	int v3 = NFS_ISV3(vp);

	md = *mdp;
	t1 = (mtod(md, caddr_t) + md->m_len) - *dposp;
	cp2 = nfsm_disct(mdp, dposp, NFSX_FATTR(v3), t1);
	if (cp2 == NULL)
		return EBADRPC;
	fp = (struct nfs_fattr *)cp2;
	if (v3) {
		vtyp = nfsv3tov_type(fp->fa_type);
		vmode = fxdr_unsigned(u_short, fp->fa_mode);
		rdev = makeudev(fxdr_unsigned(int, fp->fa3_rdev.specdata1),
			fxdr_unsigned(int, fp->fa3_rdev.specdata2));
		fxdr_nfsv3time(&fp->fa3_mtime, &mtime);
	} else {
		vtyp = nfsv2tov_type(fp->fa_type);
		vmode = fxdr_unsigned(u_short, fp->fa_mode);
		/*
		 * XXX
		 *
		 * The duplicate information returned in fa_type and fa_mode
		 * is an ambiguity in the NFS version 2 protocol.
		 *
		 * VREG should be taken literally as a regular file.  If a
		 * server intents to return some type information differently
		 * in the upper bits of the mode field (e.g. for sockets, or
		 * FIFOs), NFSv2 mandates fa_type to be VNON.  Anyway, we
		 * leave the examination of the mode bits even in the VREG
		 * case to avoid breakage for bogus servers, but we make sure
		 * that there are actually type bits set in the upper part of
		 * fa_mode (and failing that, trust the va_type field).
		 *
		 * NFSv3 cleared the issue, and requires fa_mode to not
		 * contain any type information (while also introduing sockets
		 * and FIFOs for fa_type).
		 */
		if (vtyp == VNON || (vtyp == VREG && (vmode & S_IFMT) != 0))
			vtyp = IFTOVT(vmode);
		rdev = fxdr_unsigned(int32_t, fp->fa2_rdev);
		fxdr_nfsv2time(&fp->fa2_mtime, &mtime);

		/*
		 * Really ugly NFSv2 kludge.
		 */
		if (vtyp == VCHR && rdev == 0xffffffff)
			vtyp = VFIFO;
	}

	/*
	 * If v_type == VNON it is a new node, so fill in the v_type,
	 * n_mtime fields. Check to see if it represents a special
	 * device, and if so, check for a possible alias. Once the
	 * correct vnode has been obtained, fill in the rest of the
	 * information.
	 */
	np = VTONFS(vp);
	if (vp->v_type != vtyp) {
		vp->v_type = vtyp;
		if (vp->v_type == VFIFO) {
			vp->v_op = fifo_nfsnodeop_p;
		}
		if (vp->v_type == VCHR || vp->v_type == VBLK) {
			vp->v_op = spec_nfsnodeop_p;
			vp = addaliasu(vp, rdev);
			np->n_vnode = vp;
		}
		np->n_mtime = mtime.tv_sec;
	}
	vap = &np->n_vattr;
	vap->va_type = vtyp;
	vap->va_mode = (vmode & 07777);
	vap->va_rdev = rdev;
	vap->va_mtime = mtime;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	if (v3) {
		vap->va_nlink = fxdr_unsigned(u_short, fp->fa_nlink);
		vap->va_uid = fxdr_unsigned(uid_t, fp->fa_uid);
		vap->va_gid = fxdr_unsigned(gid_t, fp->fa_gid);
		vap->va_size = fxdr_hyper(&fp->fa3_size);
		vap->va_blocksize = NFS_FABLKSIZE;
		vap->va_bytes = fxdr_hyper(&fp->fa3_used);
		vap->va_fileid = fxdr_unsigned(int32_t,
		    fp->fa3_fileid.nfsuquad[1]);
		fxdr_nfsv3time(&fp->fa3_atime, &vap->va_atime);
		fxdr_nfsv3time(&fp->fa3_ctime, &vap->va_ctime);
		vap->va_flags = 0;
		vap->va_filerev = 0;
	} else {
		vap->va_nlink = fxdr_unsigned(u_short, fp->fa_nlink);
		vap->va_uid = fxdr_unsigned(uid_t, fp->fa_uid);
		vap->va_gid = fxdr_unsigned(gid_t, fp->fa_gid);
		vap->va_size = fxdr_unsigned(u_int32_t, fp->fa2_size);
		vap->va_blocksize = fxdr_unsigned(int32_t, fp->fa2_blocksize);
		vap->va_bytes = (u_quad_t)fxdr_unsigned(int32_t, fp->fa2_blocks)
		    * NFS_FABLKSIZE;
		vap->va_fileid = fxdr_unsigned(int32_t, fp->fa2_fileid);
		fxdr_nfsv2time(&fp->fa2_atime, &vap->va_atime);
		vap->va_flags = 0;
		vap->va_ctime.tv_sec = fxdr_unsigned(u_int32_t,
		    fp->fa2_ctime.nfsv2_sec);
		vap->va_ctime.tv_nsec = 0;
		vap->va_gen = fxdr_unsigned(u_int32_t, fp->fa2_ctime.nfsv2_usec);
		vap->va_filerev = 0;
	}
	np->n_attrstamp = time_second;
	if (vap->va_size != np->n_size) {
		if (vap->va_type == VREG) {
			if (dontshrink && vap->va_size < np->n_size) {
				/*
				 * We've been told not to shrink the file;
				 * zero np->n_attrstamp to indicate that
				 * the attributes are stale.
				 */
				vap->va_size = np->n_size;
				np->n_attrstamp = 0;
			} else if (np->n_flag & NMODIFIED) {
				if (vap->va_size < np->n_size)
					vap->va_size = np->n_size;
				else
					np->n_size = vap->va_size;
			} else {
				np->n_size = vap->va_size;
			}
			vnode_pager_setsize(vp, np->n_size);
		} else {
			np->n_size = vap->va_size;
		}
	}
	if (vaper != NULL) {
		bcopy((caddr_t)vap, (caddr_t)vaper, sizeof(*vap));
		if (np->n_flag & NCHG) {
			if (np->n_flag & NACC)
				vaper->va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vaper->va_mtime = np->n_mtim;
		}
	}
	return (0);
}

#ifdef NFS_ACDEBUG
#include <sys/sysctl.h>
SYSCTL_DECL(_vfs_nfs);
static int nfs_acdebug;
SYSCTL_INT(_vfs_nfs, OID_AUTO, acdebug, CTLFLAG_RW, &nfs_acdebug, 0, "");
#endif

/*
 * Check the time stamp
 * If the cache is valid, copy contents to *vap and return 0
 * otherwise return an error
 */
int
nfs_getattrcache(struct vnode *vp, struct vattr *vaper)
{
	struct nfsnode *np;
	struct vattr *vap;
	struct nfsmount *nmp;
	int timeo;

	np = VTONFS(vp);
	vap = &np->n_vattr;
	nmp = VFSTONFS(vp->v_mount);
	/* XXX n_mtime doesn't seem to be updated on a miss-and-reload */
	timeo = (time_second - np->n_mtime) / 10;

#ifdef NFS_ACDEBUG
	if (nfs_acdebug>1)
		printf("nfs_getattrcache: initial timeo = %d\n", timeo);
#endif

	if (vap->va_type == VDIR) {
		if ((np->n_flag & NMODIFIED) || timeo < nmp->nm_acdirmin)
			timeo = nmp->nm_acdirmin;
		else if (timeo > nmp->nm_acdirmax)
			timeo = nmp->nm_acdirmax;
	} else {
		if ((np->n_flag & NMODIFIED) || timeo < nmp->nm_acregmin)
			timeo = nmp->nm_acregmin;
		else if (timeo > nmp->nm_acregmax)
			timeo = nmp->nm_acregmax;
	}

#ifdef NFS_ACDEBUG
	if (nfs_acdebug > 2)
		printf("acregmin %d; acregmax %d; acdirmin %d; acdirmax %d\n",
			nmp->nm_acregmin, nmp->nm_acregmax,
			nmp->nm_acdirmin, nmp->nm_acdirmax);

	if (nfs_acdebug)
		printf("nfs_getattrcache: age = %d; final timeo = %d\n",
			(time_second - np->n_attrstamp), timeo);
#endif

	if ((time_second - np->n_attrstamp) >= timeo) {
		nfsstats.attrcache_misses++;
		return (ENOENT);
	}
	nfsstats.attrcache_hits++;
	if (vap->va_size != np->n_size) {
		if (vap->va_type == VREG) {
			if (np->n_flag & NMODIFIED) {
				if (vap->va_size < np->n_size)
					vap->va_size = np->n_size;
				else
					np->n_size = vap->va_size;
			} else {
				np->n_size = vap->va_size;
			}
			vnode_pager_setsize(vp, np->n_size);
		} else {
			np->n_size = vap->va_size;
		}
	}
	bcopy((caddr_t)vap, (caddr_t)vaper, sizeof(struct vattr));
	if (np->n_flag & NCHG) {
		if (np->n_flag & NACC)
			vaper->va_atime = np->n_atim;
		if (np->n_flag & NUPD)
			vaper->va_mtime = np->n_mtim;
	}
	return (0);
}

static nfsuint64 nfs_nullcookie = { { 0, 0 } };
/*
 * This function finds the directory cookie that corresponds to the
 * logical byte offset given.
 */
nfsuint64 *
nfs_getcookie(struct nfsnode *np, off_t off, int add)
{
	struct nfsdmap *dp, *dp2;
	int pos;

	pos = (uoff_t)off / NFS_DIRBLKSIZ;
	if (pos == 0 || off < 0) {
#ifdef DIAGNOSTIC
		if (add)
			panic("nfs getcookie add at <= 0");
#endif
		return (&nfs_nullcookie);
	}
	pos--;
	dp = LIST_FIRST(&np->n_cookies);
	if (!dp) {
		if (add) {
			MALLOC(dp, struct nfsdmap *, sizeof (struct nfsdmap),
				M_NFSDIROFF, M_WAITOK);
			dp->ndm_eocookie = 0;
			LIST_INSERT_HEAD(&np->n_cookies, dp, ndm_list);
		} else
			return (NULL);
	}
	while (pos >= NFSNUMCOOKIES) {
		pos -= NFSNUMCOOKIES;
		if (LIST_NEXT(dp, ndm_list)) {
			if (!add && dp->ndm_eocookie < NFSNUMCOOKIES &&
				pos >= dp->ndm_eocookie)
				return (NULL);
			dp = LIST_NEXT(dp, ndm_list);
		} else if (add) {
			MALLOC(dp2, struct nfsdmap *, sizeof (struct nfsdmap),
				M_NFSDIROFF, M_WAITOK);
			dp2->ndm_eocookie = 0;
			LIST_INSERT_AFTER(dp, dp2, ndm_list);
			dp = dp2;
		} else
			return (NULL);
	}
	if (pos >= dp->ndm_eocookie) {
		if (add)
			dp->ndm_eocookie = pos + 1;
		else
			return (NULL);
	}
	return (&dp->ndm_cookies[pos]);
}

/*
 * Invalidate cached directory information, except for the actual directory
 * blocks (which are invalidated separately).
 * Done mainly to avoid the use of stale offset cookies.
 */
void
nfs_invaldir(struct vnode *vp)
{
	struct nfsnode *np = VTONFS(vp);

#ifdef DIAGNOSTIC
	if (vp->v_type != VDIR)
		panic("nfs: invaldir not dir");
#endif
	if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NFSV4) != 0) {
		nfs4_invaldir(vp);
		return;
	}
	np->n_direofoffset = 0;
	np->n_cookieverf.nfsuquad[0] = 0;
	np->n_cookieverf.nfsuquad[1] = 0;
	if (LIST_FIRST(&np->n_cookies))
		LIST_FIRST(&np->n_cookies)->ndm_eocookie = 0;
}

/*
 * The write verifier has changed (probably due to a server reboot), so all
 * B_NEEDCOMMIT blocks will have to be written again. Since they are on the
 * dirty block list as B_DELWRI, all this takes is clearing the B_NEEDCOMMIT
 * and B_CLUSTEROK flags.  Once done the new write verifier can be set for the
 * mount point.
 *
 * B_CLUSTEROK must be cleared along with B_NEEDCOMMIT because stage 1 data
 * writes are not clusterable.
 */
void
nfs_clearcommit(struct mount *mp)
{
	struct vnode *vp, *nvp;
	struct buf *bp, *nbp;
	int s;

	GIANT_REQUIRED;

	s = splbio();
	MNT_ILOCK(mp);
loop:
	for (vp = TAILQ_FIRST(&mp->mnt_nvnodelist); vp; vp = nvp) {
		if (vp->v_mount != mp)	/* Paranoia */
			goto loop;
		nvp = TAILQ_NEXT(vp, v_nmntvnodes);
		VI_LOCK(vp);
		if (vp->v_iflag & VI_XLOCK) {
			VI_UNLOCK(vp);
			continue;
		}
		MNT_IUNLOCK(mp);
		for (bp = TAILQ_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
			nbp = TAILQ_NEXT(bp, b_vnbufs);
			if (BUF_REFCNT(bp) == 0 &&
			    (bp->b_flags & (B_DELWRI | B_NEEDCOMMIT))
				== (B_DELWRI | B_NEEDCOMMIT))
				bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);
		}
		VI_UNLOCK(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	splx(s);
}

/*
 * Helper functions for former macros.  Some of these should be
 * moved to their callers.
 */

int
nfsm_mtofh_xx(struct vnode *d, struct vnode **v, int v3, int *f,
    struct mbuf **md, caddr_t *dpos)
{
	struct nfsnode *ttnp;
	struct vnode *ttvp;
	nfsfh_t *ttfhp;
	u_int32_t *tl;
	int ttfhsize;
	int t1;

	if (v3) {
		tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		*f = fxdr_unsigned(int, *tl);
	} else
		*f = 1;
	if (*f) {
		t1 = nfsm_getfh_xx(&ttfhp, &ttfhsize, (v3), md, dpos);
		if (t1 != 0)
			return t1;
		t1 = nfs_nget(d->v_mount, ttfhp, ttfhsize, &ttnp);
		if (t1 != 0)
			return t1;
		*v = NFSTOV(ttnp);
	}
	if (v3) {
		tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		if (*f)
			*f = fxdr_unsigned(int, *tl);
		else if (fxdr_unsigned(int, *tl))
			nfsm_adv_xx(NFSX_V3FATTR, md, dpos);
	}
	if (*f) {
		ttvp = *v;
		t1 = nfs_loadattrcache(&ttvp, md, dpos, NULL, 0);
		if (t1)
			return t1;
		*v = ttvp;
	}
	return 0;
}

int
nfsm_getfh_xx(nfsfh_t **f, int *s, int v3, struct mbuf **md, caddr_t *dpos)
{
	u_int32_t *tl;

	if (v3) {
		tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		*s = fxdr_unsigned(int, *tl);
		if (*s <= 0 || *s > NFSX_V3FHMAX)
			return EBADRPC;
	} else
		*s = NFSX_V2FH;
	*f = nfsm_dissect_xx(nfsm_rndup(*s), md, dpos);
	if (*f == NULL)
		return EBADRPC;
	else
		return 0;
}


int
nfsm_loadattr_xx(struct vnode **v, struct vattr *va, struct mbuf **md,
    caddr_t *dpos)
{
	int t1;

	struct vnode *ttvp = *v;
	t1 = nfs_loadattrcache(&ttvp, md, dpos, va, 0);
	if (t1 != 0)
		return t1;
	*v = ttvp;
	return 0;
}

int
nfsm_postop_attr_xx(struct vnode **v, int *f, struct mbuf **md,
    caddr_t *dpos)
{
	u_int32_t *tl;
	int t1;

	struct vnode *ttvp = *v;
	tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	*f = fxdr_unsigned(int, *tl);
	if (*f != 0) {
		t1 = nfs_loadattrcache(&ttvp, md, dpos, NULL, 1);
		if (t1 != 0) {
			*f = 0;
			return t1;
		}
		*v = ttvp;
	}
	return 0;
}

int
nfsm_wcc_data_xx(struct vnode **v, int *f, struct mbuf **md, caddr_t *dpos)
{
	u_int32_t *tl;
	int ttattrf, ttretf = 0;
	int t1;

	tl = nfsm_dissect_xx(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	if (*tl == nfs_true) {
		tl = nfsm_dissect_xx(6 * NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		if (*f)
			ttretf = (VTONFS(*v)->n_mtime ==
			    fxdr_unsigned(u_int32_t, *(tl + 2)));
	}
	t1 = nfsm_postop_attr_xx(v, &ttattrf, md, dpos);
	if (t1)
		return t1;
	if (*f)
		*f = ttretf;
	else
		*f = ttattrf;
	return 0;
}

int
nfsm_strtom_xx(const char *a, int s, int m, struct mbuf **mb, caddr_t *bpos)
{
	u_int32_t *tl;
	int t1;

	if (s > m)
		return ENAMETOOLONG;
	t1 = nfsm_rndup(s) + NFSX_UNSIGNED;
	if (t1 <= M_TRAILINGSPACE(*mb)) {
		tl = nfsm_build_xx(t1, mb, bpos);
		*tl++ = txdr_unsigned(s);
		*(tl + ((t1 >> 2) - 2)) = 0;
		bcopy(a, tl, s);
	} else {
		t1 = nfsm_strtmbuf(mb, bpos, a, s);
		if (t1 != 0)
			return t1;
	}
	return 0;
}

int
nfsm_fhtom_xx(struct vnode *v, int v3, struct mbuf **mb, caddr_t *bpos)
{
	u_int32_t *tl;
	int t1;
	caddr_t cp;

	if (v3) {
		t1 = nfsm_rndup(VTONFS(v)->n_fhsize) + NFSX_UNSIGNED;
		if (t1 < M_TRAILINGSPACE(*mb)) {
			tl = nfsm_build_xx(t1, mb, bpos);
			*tl++ = txdr_unsigned(VTONFS(v)->n_fhsize);
			*(tl + ((t1 >> 2) - 2)) = 0;
			bcopy(VTONFS(v)->n_fhp, tl, VTONFS(v)->n_fhsize);
		} else {
			t1 = nfsm_strtmbuf(mb, bpos,
			    (const char *)VTONFS(v)->n_fhp,
			    VTONFS(v)->n_fhsize);
			if (t1 != 0)
				return t1;
		}
	} else {
		cp = nfsm_build_xx(NFSX_V2FH, mb, bpos);
		bcopy(VTONFS(v)->n_fhp, cp, NFSX_V2FH);
	}
	return 0;
}

void
nfsm_v3attrbuild_xx(struct vattr *va, int full, struct mbuf **mb,
    caddr_t *bpos)
{
	u_int32_t *tl;

	if (va->va_mode != (mode_t)VNOVAL) {
		tl = nfsm_build_xx(2 * NFSX_UNSIGNED, mb, bpos);
		*tl++ = nfs_true;
		*tl = txdr_unsigned(va->va_mode);
	} else {
		tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
		*tl = nfs_false;
	}
	if (full && va->va_uid != (uid_t)VNOVAL) {
		tl = nfsm_build_xx(2 * NFSX_UNSIGNED, mb, bpos);
		*tl++ = nfs_true;
		*tl = txdr_unsigned(va->va_uid);
	} else {
		tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
		*tl = nfs_false;
	}
	if (full && va->va_gid != (gid_t)VNOVAL) {
		tl = nfsm_build_xx(2 * NFSX_UNSIGNED, mb, bpos);
		*tl++ = nfs_true;
		*tl = txdr_unsigned(va->va_gid);
	} else {
		tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
		*tl = nfs_false;
	}
	if (full && va->va_size != VNOVAL) {
		tl = nfsm_build_xx(3 * NFSX_UNSIGNED, mb, bpos);
		*tl++ = nfs_true;
		txdr_hyper(va->va_size, tl);
	} else {
		tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
		*tl = nfs_false;
	}
	if (va->va_atime.tv_sec != VNOVAL) {
		if (va->va_atime.tv_sec != time_second) {
			tl = nfsm_build_xx(3 * NFSX_UNSIGNED, mb, bpos);
			*tl++ = txdr_unsigned(NFSV3SATTRTIME_TOCLIENT);
			txdr_nfsv3time(&va->va_atime, tl);
		} else {
			tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
			*tl = txdr_unsigned(NFSV3SATTRTIME_TOSERVER);
		}
	} else {
		tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
		*tl = txdr_unsigned(NFSV3SATTRTIME_DONTCHANGE);
	}
	if (va->va_mtime.tv_sec != VNOVAL) {
		if (va->va_mtime.tv_sec != time_second) {
			tl = nfsm_build_xx(3 * NFSX_UNSIGNED, mb, bpos);
			*tl++ = txdr_unsigned(NFSV3SATTRTIME_TOCLIENT);
			txdr_nfsv3time(&va->va_mtime, tl);
		} else {
			tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
			*tl = txdr_unsigned(NFSV3SATTRTIME_TOSERVER);
		}
	} else {
		tl = nfsm_build_xx(NFSX_UNSIGNED, mb, bpos);
		*tl = txdr_unsigned(NFSV3SATTRTIME_DONTCHANGE);
	}
}
