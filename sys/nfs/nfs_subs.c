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
 *	From:	@(#)nfs_subs.c	7.41 (Berkeley) 5/15/91
 *	$Id: nfs_subs.c,v 1.6 1994/04/14 07:50:11 davidg Exp $
 */

/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */
#include "param.h"
#include "proc.h"
#include "filedesc.h"
#include "systm.h"
#include "kernel.h"
#include "mount.h"
#include "file.h"
#include "vnode.h"
#include "namei.h"
#include "mbuf.h"
#ifdef	PROTOTYPESDONE
#include "vm/vnode_pager.h"
#endif	/*PROTOTYPESDONE*/

#include "../ufs/quota.h"
#include "../ufs/inode.h"

#include "rpcv2.h"
#include "nfsv2.h"
#include "nfsnode.h"
#include "nfs.h"
#include "nfsiom.h"
#include "xdr_subs.h"
#include "nfsm_subs.h"
#include "nfscompress.h"

#define TRUE	1
#define	FALSE	0

struct nfsstats nfsstats;

/*
 * Data items converted to xdr at startup, since they are constant
 * This is kinda hokey, but may save a little time doing byte swaps
 */
u_long nfs_procids[NFS_NPROCS];
u_long nfs_xdrneg1;
u_long rpc_call, rpc_vers, rpc_reply, rpc_msgdenied,
	rpc_mismatch, rpc_auth_unix, rpc_msgaccepted;
u_long nfs_vers, nfs_prog, nfs_true, nfs_false;
/* And other global data */
static u_long *rpc_uidp = (u_long *)0;
static u_long nfs_xid = 1;
static char *rpc_unixauth;
extern long hostid;
enum vtype ntov_type[7] = { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VNON };
extern struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
extern struct nfsreq nfsreqh;

/* Function ret types */
static char *nfs_unixauth();

/*
 * Maximum number of groups passed through to NFS server.
 * According to RFC1057 it should be 16.
 * For release 3.X systems, the maximum value is 8.
 * For some other servers, the maximum value is 10.
 */
int numgrps = 8;

/*
 * Create the header for an rpc request packet
 * The function nfs_unixauth() creates a unix style authorization string
 * and returns a ptr to it.
 * The hsiz is the size of the rest of the nfs request header.
 * (just used to decide if a cluster is a good idea)
 * nb: Note that the prog, vers and procid args are already in xdr byte order
 */
struct mbuf *nfsm_reqh(prog, vers, procid, cred, hsiz, bpos, mb, retxid)
	u_long prog;
	u_long vers;
	u_long procid;
	struct ucred *cred;
	int hsiz;
	caddr_t *bpos;
	struct mbuf **mb;
	u_long *retxid;
{
	register struct mbuf *mreq, *m;
	register u_long *tl;
	struct mbuf *m1;
	char *ap;
	int asiz, siz;

	NFSMGETHDR(mreq);
	asiz = ((((cred->cr_ngroups - 1) > numgrps) ? numgrps :
		  (cred->cr_ngroups - 1)) << 2);
#ifdef FILLINHOST
	asiz += nfsm_rndup(hostnamelen)+(9*NFSX_UNSIGNED);
#else
	asiz += 9*NFSX_UNSIGNED;
#endif

	/* If we need a lot, alloc a cluster ?? */
	if ((asiz+hsiz+RPC_SIZ) > MHLEN)
		MCLGET(mreq, M_WAIT);
	mreq->m_len = NFSMSIZ(mreq);
	siz = mreq->m_len;
	m1 = mreq;
	/*
	 * Alloc enough mbufs
	 * We do it now to avoid all sleeps after the call to nfs_unixauth()
	 */
	while ((asiz+RPC_SIZ) > siz) {
		MGET(m, M_WAIT, MT_DATA);
		m1->m_next = m;
		m->m_len = MLEN;
		siz += MLEN;
		m1 = m;
	}
	tl = mtod(mreq, u_long *);
	*tl++ = *retxid = txdr_unsigned(++nfs_xid);
	*tl++ = rpc_call;
	*tl++ = rpc_vers;
	*tl++ = prog;
	*tl++ = vers;
	*tl++ = procid;

	/* Now we can call nfs_unixauth() and copy it in */
	ap = nfs_unixauth(cred);
	m = mreq;
	siz = m->m_len-RPC_SIZ;
	if (asiz <= siz) {
		bcopy(ap, (caddr_t)tl, asiz);
		m->m_len = asiz+RPC_SIZ;
	} else {
		bcopy(ap, (caddr_t)tl, siz);
		ap += siz;
		asiz -= siz;
		while (asiz > 0) {
			siz = (asiz > MLEN) ? MLEN : asiz;
			m = m->m_next;
			bcopy(ap, mtod(m, caddr_t), siz);
			m->m_len = siz;
			asiz -= siz;
			ap += siz;
		}
	}
	
	/* Finally, return values */
	*mb = m;
	*bpos = mtod(m, caddr_t)+m->m_len;
	return (mreq);
}

/*
 * copies mbuf chain to the uio scatter/gather list
 */
int
nfsm_mbuftouio(mrep, uiop, siz, dpos)
	struct mbuf **mrep;
	register struct uio *uiop;
	int siz;
	caddr_t *dpos;
{
	register char *mbufcp, *uiocp;
	register int xfer, left, len;
	register struct mbuf *mp;
	long uiosiz, rem;
	int error = 0;

	mp = *mrep;
	mbufcp = *dpos;
	len = mtod(mp, caddr_t)+mp->m_len-mbufcp;
	rem = nfsm_rndup(siz)-siz;
	while (siz > 0) {
		if (uiop->uio_iovcnt <= 0 || uiop->uio_iov == NULL)
			return (EFBIG);
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			while (len == 0) {
				mp = mp->m_next;
				if (mp == NULL)
					return (EBADRPC);
				mbufcp = mtod(mp, caddr_t);
				len = mp->m_len;
			}
			xfer = (left > len) ? len : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(mbufcp, uiocp, xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
				bcopy(mbufcp, uiocp, xfer);
			else
				copyout(mbufcp, uiocp, xfer);
			left -= xfer;
			len -= xfer;
			mbufcp += xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		if (uiop->uio_iov->iov_len <= siz) {
			uiop->uio_iovcnt--;
			uiop->uio_iov++;
		} else {
			uiop->uio_iov->iov_base += uiosiz;
			uiop->uio_iov->iov_len -= uiosiz;
		}
		siz -= uiosiz;
	}
	*dpos = mbufcp;
	*mrep = mp;
	if (rem > 0) {
		if (len < rem)
			error = nfs_adv(mrep, dpos, rem, len);
		else
			*dpos += rem;
	}
	return (error);
}

void nfsm_nullfree()
{
	/* Nothing to do here */
}


/*
 * copies a uio scatter/gather list to an mbuf chain...
 */
int
nfsm_uiotombuf(uiop, mq, siz, bpos)
	register struct uio *uiop;
	struct mbuf **mq;
	int siz;
	caddr_t *bpos;
{
	register char *uiocp;
	register struct mbuf *mp = 0, *mp2;
	register int xfer, left, len = 0;
	int uiosiz, clflg, rem;
	char *cp;

	if (siz > MLEN && uiop->uio_segflg != UIO_SYSSPACE)
		clflg = 1;
	else
		clflg = 0;
	rem = nfsm_rndup(siz)-siz;
	mp2 = *mq;
	while (siz > 0) {
		if (uiop->uio_iovcnt <= 0 || uiop->uio_iov == NULL)
			return (EINVAL);
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		if (uiop->uio_segflg == UIO_SYSSPACE) {
			MGET(mp, M_WAIT, MT_DATA);
			mp->m_flags |= M_EXT;
			mp->m_data = mp->m_ext.ext_buf = uiocp;
			mp->m_len = mp->m_ext.ext_size = uiosiz;
			mp->m_ext.ext_free = nfsm_nullfree;
			mp2->m_next = mp;
			mp2 = mp;
			uiop->uio_offset += uiosiz;
			uiop->uio_resid -= uiosiz;
		} else {
			while (left > 0) {
				MGET(mp, M_WAIT, MT_DATA);
				if (clflg)
					MCLGET(mp, M_WAIT);
				mp->m_len = NFSMSIZ(mp);
				mp2->m_next = mp;
				mp2 = mp;
				xfer = (left > mp->m_len) ? mp->m_len : left;
#ifdef notdef
				/* Not Yet.. */
				if (uiop->uio_iov->iov_op != NULL)
					(*(uiop->uio_iov->iov_op))
					(uiocp, mtod(mp, caddr_t), xfer);
				else
				if (uiop->uio_segflg == UIO_SYSSPACE)
					bcopy(uiocp, mtod(mp, caddr_t), xfer);
				else
#endif
				copyin(uiocp, mtod(mp, caddr_t), xfer);
				len = mp->m_len;
				mp->m_len = xfer;
				left -= xfer;
				uiocp += xfer;
				uiop->uio_offset += xfer;
				uiop->uio_resid -= xfer;
			}
		}
		if (uiop->uio_iov->iov_len <= siz) {
			uiop->uio_iovcnt--;
			uiop->uio_iov++;
		} else {
			uiop->uio_iov->iov_base += uiosiz;
			uiop->uio_iov->iov_len -= uiosiz;
		}
		siz -= uiosiz;
	}
	if (rem > 0) {
		if (rem > (len-mp->m_len)) {
			MGET(mp, M_WAIT, MT_DATA);
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
 * Help break down an mbuf chain by setting the first siz bytes contiguous
 * pointed to by returned val.
 * If Updateflg == True we can overwrite the first part of the mbuf data
 * This is used by the macros nfsm_disect and nfsm_disecton for tough
 * cases. (The macros use the vars. dpos and dpos2)
 */
int
nfsm_disct(mdp, dposp, siz, left, updateflg, cp2)
	struct mbuf **mdp;
	caddr_t *dposp;
	int siz;
	int left;
	int updateflg;
	caddr_t *cp2;
{
	register struct mbuf *mp, *mp2;
	register int siz2, xfer;
	register caddr_t tl;

	mp = *mdp;
	while (left == 0) {
		*mdp = mp = mp->m_next;
		if (mp == NULL)
			return (EBADRPC);
		left = mp->m_len;
		*dposp = mtod(mp, caddr_t);
	}
	if (left >= siz) {
		*cp2 = *dposp;
		*dposp += siz;
	} else if (mp->m_next == NULL) {
		return (EBADRPC);
	} else if (siz > MHLEN) {
		panic("nfs S too big");
	} else {
		/* Iff update, you can overwrite, else must alloc new mbuf */
		if (updateflg) {
			NFSMINOFF(mp);
		} else {
			MGET(mp2, M_WAIT, MT_DATA);
			mp2->m_next = mp->m_next;
			mp->m_next = mp2;
			mp->m_len -= left;
			mp = mp2;
		}
		*cp2 = tl = mtod(mp, caddr_t);
		bcopy(*dposp, tl, left);		/* Copy what was left */
		siz2 = siz-left;
		tl += left;
		mp2 = mp->m_next;
		/* Loop around copying up the siz2 bytes */
		while (siz2 > 0) {
			if (mp2 == NULL)
				return (EBADRPC);
			xfer = (siz2 > mp2->m_len) ? mp2->m_len : siz2;
			if (xfer > 0) {
				bcopy(mtod(mp2, caddr_t), tl, xfer);
				NFSMADV(mp2, xfer);
				mp2->m_len -= xfer;
				tl += xfer;
				siz2 -= xfer;
			}
			if (siz2 > 0)
				mp2 = mp2->m_next;
		}
		mp->m_len = siz;
		*mdp = mp2;
		*dposp = mtod(mp2, caddr_t);
	}
	return (0);
}

/*
 * Advance the position in the mbuf chain.
 */
int
nfs_adv(mdp, dposp, offs, left)
	struct mbuf **mdp;
	caddr_t *dposp;
	int offs;
	int left;
{
	register struct mbuf *m;
	register int s;

	m = *mdp;
	s = left;
	while (s < offs) {
		offs -= s;
		m = m->m_next;
		if (m == NULL)
			return (EBADRPC);
		s = m->m_len;
	}
	*mdp = m;
	*dposp = mtod(m, caddr_t)+offs;
	return (0);
}

/*
 * Copy a string into mbufs for the hard cases...
 */
int
nfsm_strtmbuf(mb, bpos, cp, siz)
	struct mbuf **mb;
	char **bpos;
	char *cp;
	long siz;
{
	register struct mbuf *m1 = 0, *m2;
	long left, xfer, len, tlen;
	u_long *tl;
	int putsize;

	putsize = 1;
	m2 = *mb;
	left = NFSMSIZ(m2)-m2->m_len;
	if (left > 0) {
		tl = ((u_long *)(*bpos));
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
	/* Loop arround adding mbufs */
	while (siz > 0) {
		MGET(m1, M_WAIT, MT_DATA);
		if (siz > MLEN)
			MCLGET(m1, M_WAIT);
		m1->m_len = NFSMSIZ(m1);
		m2->m_next = m1;
		m2 = m1;
		tl = mtod(m1, u_long *);
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
void
nfs_init()
{
	register int i;

	rpc_vers = txdr_unsigned(RPC_VER2);
	rpc_call = txdr_unsigned(RPC_CALL);
	rpc_reply = txdr_unsigned(RPC_REPLY);
	rpc_msgdenied = txdr_unsigned(RPC_MSGDENIED);
	rpc_msgaccepted = txdr_unsigned(RPC_MSGACCEPTED);
	rpc_mismatch = txdr_unsigned(RPC_MISMATCH);
	rpc_auth_unix = txdr_unsigned(RPCAUTH_UNIX);
	nfs_vers = txdr_unsigned(NFS_VER2);
	nfs_prog = txdr_unsigned(NFS_PROG);
	nfs_true = txdr_unsigned(TRUE);
	nfs_false = txdr_unsigned(FALSE);
	/* Loop thru nfs procids */
	for (i = 0; i < NFS_NPROCS; i++)
		nfs_procids[i] = txdr_unsigned(i);
	/* Ensure async daemons disabled */
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++)
		nfs_iodwant[i] = (struct proc *)0;
	nfs_xdrneg1 = txdr_unsigned(-1);
	nfs_nhinit();			/* Init the nfsnode table */
	nfsrv_initcache();		/* Init the server request cache */

	/*
	 * Initialize reply list and start timer
	 */
	nfsreqh.r_prev = nfsreqh.r_next = &nfsreqh;
	nfs_timer(0);
}

/*
 * Fill in the rest of the rpc_unixauth and return it
 */
static char *nfs_unixauth(cr)
	register struct ucred *cr;
{
	register u_long *tl;
	register int i;
	int ngr;

	/* Maybe someday there should be a cache of AUTH_SHORT's */
	if ((tl = rpc_uidp) == NULL) {
#ifdef FILLINHOST
		i = nfsm_rndup(hostnamelen)+(25*NFSX_UNSIGNED);
#else
		i = 25*NFSX_UNSIGNED;
#endif
		MALLOC(tl, u_long *, i, M_TEMP, M_WAITOK);
		bzero((caddr_t)tl, i);
		rpc_unixauth = (caddr_t)tl;
		*tl++ = txdr_unsigned(RPCAUTH_UNIX);
		tl++;	/* Fill in size later */
		*tl++ = hostid;
#ifdef FILLINHOST
		*tl++ = txdr_unsigned(hostnamelen);
		i = nfsm_rndup(hostnamelen);
		bcopy(hostname, (caddr_t)tl, hostnamelen);
		tl += (i>>2);
#else
		*tl++ = 0;
#endif
		rpc_uidp = tl;
	}
	*tl++ = txdr_unsigned(cr->cr_uid);
	*tl++ = txdr_unsigned(cr->cr_groups[0]);
	ngr = ((cr->cr_ngroups - 1) > numgrps) ? numgrps : (cr->cr_ngroups - 1);
	*tl++ = txdr_unsigned(ngr);
	for (i = 1; i <= ngr; i++)
		*tl++ = txdr_unsigned(cr->cr_groups[i]);
	/* And add the AUTH_NULL */
	*tl++ = 0;
	*tl = 0;
	i = (((caddr_t)tl)-rpc_unixauth)-12;
	tl = (u_long *)(rpc_unixauth+4);
	*tl = txdr_unsigned(i);
	return (rpc_unixauth);
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
nfs_loadattrcache(vpp, mdp, dposp, vaper)
	struct vnode **vpp;
	struct mbuf **mdp;
	caddr_t *dposp;
	struct vattr *vaper;
{
	register struct vnode *vp = *vpp;
	register struct vattr *vap;
	register struct nfsv2_fattr *fp;
	extern struct vnodeops spec_nfsv2nodeops;
	register struct nfsnode *np;
	register long t1;
	caddr_t dpos, cp2;
	int error = 0;
	struct mbuf *md;
	enum vtype type;
	u_short mode;
	long rdev;
	struct timeval mtime;
	struct vnode *nvp;

	md = *mdp;
	dpos = *dposp;
	t1 = (mtod(md, caddr_t)+md->m_len)-dpos;
	if (error = nfsm_disct(&md, &dpos, NFSX_FATTR, t1, TRUE, &cp2))
		return (error);
	fp = (struct nfsv2_fattr *)cp2;
	type = nfstov_type(fp->fa_type);
	mode = fxdr_unsigned(u_short, fp->fa_mode);
	if (type == VNON)
		type = IFTOVT(mode);
	rdev = fxdr_unsigned(long, fp->fa_rdev);
	fxdr_time(&fp->fa_mtime, &mtime);
	/*
	 * If v_type == VNON it is a new node, so fill in the v_type,
	 * n_mtime fields. Check to see if it represents a special 
	 * device, and if so, check for a possible alias. Once the
	 * correct vnode has been obtained, fill in the rest of the
	 * information.
	 */
	np = VTONFS(vp);
	if (vp->v_type == VNON) {
		if (type == VCHR && rdev == 0xffffffffUL)
			vp->v_type = type = VFIFO;
		else
			vp->v_type = type;
		if (vp->v_type == VFIFO) {
#ifdef FIFO
			extern struct vnodeops fifo_nfsv2nodeops;
			vp->v_op = &fifo_nfsv2nodeops;
#else
			return (EOPNOTSUPP);
#endif /* FIFO */
		}
		if (vp->v_type == VCHR || vp->v_type == VBLK) {
			vp->v_op = &spec_nfsv2nodeops;
			if (nvp = checkalias(vp, (dev_t)rdev, vp->v_mount)) {
				/*
				 * Reinitialize aliased node.
				 */
				np = VTONFS(nvp);
				np->n_vnode = nvp;
				np->n_flag = 0;
				nfs_lock(nvp);
				bcopy((caddr_t)&VTONFS(vp)->n_fh,
					(caddr_t)&np->n_fh, NFSX_FH);
				insque(np, nfs_hash(&np->n_fh));
				np->n_attrstamp = 0;
				np->n_sillyrename = (struct sillyrename *)0;
				/*
				 * Discard unneeded vnode and update actual one
				 */
				vput(vp);
				*vpp = nvp;
			}
		}
		np->n_mtime = mtime.tv_sec;
	}
	vap = &np->n_vattr;
	vap->va_type = type;
	vap->va_mode = (mode & 07777);
	vap->va_nlink = fxdr_unsigned(u_short, fp->fa_nlink);
	vap->va_uid = fxdr_unsigned(uid_t, fp->fa_uid);
	vap->va_gid = fxdr_unsigned(gid_t, fp->fa_gid);
	vap->va_size = fxdr_unsigned(u_long, fp->fa_size);
	if ((np->n_flag & NMODIFIED) == 0 || vap->va_size > np->n_size) {
		np->n_size = vap->va_size;
		vnode_pager_setsize(vp, np->n_size);
	}
	vap->va_size_rsv = 0;
	vap->va_blocksize = fxdr_unsigned(long, fp->fa_blocksize);
	vap->va_rdev = (dev_t)rdev;
	vap->va_bytes = fxdr_unsigned(long, fp->fa_blocks) * NFS_FABLKSIZE;
	vap->va_bytes_rsv = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_fileid = fxdr_unsigned(long, fp->fa_fileid);
	vap->va_atime.tv_sec = fxdr_unsigned(long, fp->fa_atime.tv_sec);
	vap->va_atime.tv_usec = 0;
	vap->va_flags = fxdr_unsigned(u_long, fp->fa_atime.tv_usec);
	vap->va_mtime = mtime;
	vap->va_ctime.tv_sec = fxdr_unsigned(long, fp->fa_ctime.tv_sec);
	vap->va_ctime.tv_usec = 0;
	vap->va_gen = fxdr_unsigned(u_long, fp->fa_ctime.tv_usec);
	np->n_attrstamp = time.tv_sec;
	*dposp = dpos;
	*mdp = md;
	if (vaper != NULL) {
		bcopy((caddr_t)vap, (caddr_t)vaper, sizeof(*vap));
		if ((np->n_flag & NMODIFIED) && (np->n_size > vap->va_size))
			vaper->va_size = np->n_size;
	}
	return (0);
}

/*
 * Check the time stamp
 * If the cache is valid, copy contents to *vap and return 0
 * otherwise return an error
 */
int
nfs_getattrcache(vp, vap)
	register struct vnode *vp;
	struct vattr *vap;
{
	register struct nfsnode *np;

	np = VTONFS(vp);
	if ((time.tv_sec-np->n_attrstamp) < NFS_ATTRTIMEO) {
		nfsstats.attrcache_hits++;
		bcopy((caddr_t)&np->n_vattr,(caddr_t)vap,sizeof(struct vattr));
		if ((np->n_flag & NMODIFIED) == 0) {
			np->n_size = vap->va_size;
			vnode_pager_setsize(vp, np->n_size);
		} else if (np->n_size > vap->va_size)
			vap->va_size = np->n_size;
		return (0);
	} else {
		nfsstats.attrcache_misses++;
		return (ENOENT);
	}
}

/*
 * Set up nameidata for a namei() call and do it
 */
int
nfs_namei(ndp, fhp, len, mdp, dposp, p)
	register struct nameidata *ndp;
	fhandle_t *fhp;
	int len;
	struct mbuf **mdp;
	caddr_t *dposp;
	struct proc *p;
{
	register int i, rem;
	register struct mbuf *md;
	register char *fromcp, *tocp;
	struct vnode *dp;
	int flag;
	int error;

	flag = ndp->ni_nameiop & OPMASK;
	MALLOC(ndp->ni_pnbuf, char *, len + 1, M_NAMEI, M_WAITOK);
	/*
	 * Copy the name from the mbuf list to ndp->ni_pnbuf
	 * and set the various ndp fields appropriately.
	 */
	fromcp = *dposp;
	tocp = ndp->ni_pnbuf;
	md = *mdp;
	rem = mtod(md, caddr_t) + md->m_len - fromcp;
	ndp->ni_hash = 0;
	for (i = 0; i < len; i++) {
		while (rem == 0) {
			md = md->m_next;
			if (md == NULL) {
				error = EBADRPC;
				goto out;
			}
			fromcp = mtod(md, caddr_t);
			rem = md->m_len;
		}
		if (*fromcp == '\0' || *fromcp == '/') {
			error = EINVAL;
			goto out;
		}
		if (*fromcp & 0200)
			if ((*fromcp&0377) == ('/'|0200) || flag != DELETE) {
				error = EINVAL;
				goto out;
			}
		ndp->ni_hash += (unsigned char)*fromcp;
		*tocp++ = *fromcp++;
		rem--;
	}
	*tocp = '\0';
	*mdp = md;
	*dposp = fromcp;
	len = nfsm_rndup(len)-len;
	if (len > 0) {
		if (rem >= len)
			*dposp += len;
		else if (error = nfs_adv(mdp, dposp, len, rem))
			goto out;
	}
	ndp->ni_pathlen = tocp - ndp->ni_pnbuf;
	ndp->ni_ptr = ndp->ni_pnbuf;
	/*
	 * Extract and set starting directory.
	 */
	if (error = nfsrv_fhtovp(fhp, FALSE, &dp, ndp->ni_cred))
		goto out;
	if (dp->v_type != VDIR) {
		vrele(dp);
		error = ENOTDIR;
		goto out;
	}
	ndp->ni_startdir = dp;
	ndp->ni_nameiop |= (NOCROSSMOUNT | REMOTE);
	/*
	 * And call lookup() to do the real work
	 */
	if (error = lookup(ndp, p))
		goto out;
	/*
	 * Check for encountering a symbolic link
	 */
	if (ndp->ni_more) {
		if ((ndp->ni_nameiop & LOCKPARENT) && ndp->ni_pathlen == 1)
			vput(ndp->ni_dvp);
		else
			vrele(ndp->ni_dvp);
		vput(ndp->ni_vp);
		ndp->ni_vp = NULL;
		error = EINVAL;
		goto out;
	}
	/*
	 * Check for saved name request
	 */
	if (ndp->ni_nameiop & (SAVENAME | SAVESTART)) {
		ndp->ni_nameiop |= HASBUF;
		return (0);
	}
out:
	FREE(ndp->ni_pnbuf, M_NAMEI);
	return (error);
}

/*
 * A fiddled version of m_adj() that ensures null fill to a long
 * boundary and only trims off the back end
 */
void
nfsm_adj(mp, len, nul)
	struct mbuf *mp;
	register int len;
	int nul;
{
	register struct mbuf *m;
	register int count, i;
	register char *cp;

	/*
	 * Trim from tail.  Scan the mbuf chain,
	 * calculating its length and finding the last mbuf.
	 * If the adjustment only affects this mbuf, then just
	 * adjust and return.  Otherwise, rescan and truncate
	 * after the remaining size.
	 */
	count = 0;
	m = mp;
	for (;;) {
		count += m->m_len;
		if (m->m_next == (struct mbuf *)0)
			break;
		m = m->m_next;
	}
	if (m->m_len > len) {
		m->m_len -= len;
		if (nul > 0) {
			cp = mtod(m, caddr_t)+m->m_len-nul;
			for (i = 0; i < nul; i++)
				*cp++ = '\0';
		}
		return;
	}
	count -= len;
	if (count < 0)
		count = 0;
	/*
	 * Correct length for chain is "count".
	 * Find the mbuf with last data, adjust its length,
	 * and toss data from remaining mbufs on chain.
	 */
	for (m = mp; m; m = m->m_next) {
		if (m->m_len >= count) {
			m->m_len = count;
			if (nul > 0) {
				cp = mtod(m, caddr_t)+m->m_len-nul;
				for (i = 0; i < nul; i++)
					*cp++ = '\0';
			}
			break;
		}
		count -= m->m_len;
	}
	while (m = m->m_next)
		m->m_len = 0;
}

/*
 * nfsrv_fhtovp() - convert a fh to a vnode ptr (optionally locked)
 * 	- look up fsid in mount list (if not found ret error)
 *	- check that it is exported
 *	- get vp by calling VFS_FHTOVP() macro
 *	- if not lockflag unlock it with VOP_UNLOCK()
 *	- if cred->cr_uid == 0 set it to m_exroot
 */
int
nfsrv_fhtovp(fhp, lockflag, vpp, cred)
	fhandle_t *fhp;
	int lockflag;
	struct vnode **vpp;
	struct ucred *cred;
{
	register struct mount *mp;

	if ((mp = getvfs(&fhp->fh_fsid)) == NULL)
		return (ESTALE);
	if ((mp->mnt_flag & MNT_EXPORTED) == 0)
		return (EACCES);
	if (VFS_FHTOVP(mp, &fhp->fh_fid, vpp))
		return (ESTALE);
	if (cred->cr_uid == 0)
		cred->cr_uid = mp->mnt_exroot;
	if (!lockflag)
		VOP_UNLOCK(*vpp);
	return (0);
}

/*
 * These two functions implement nfs rpc compression.
 * The algorithm is a trivial run length encoding of '\0' bytes. The high
 * order nibble of hex "e" is or'd with the number of zeroes - 2 in four
 * bits. (2 - 17 zeros) Any data byte with a high order nibble of hex "e"
 * is byte stuffed.
 * The compressed data is padded with 0x0 bytes to an even multiple of
 * 4 bytes in length to avoid any weird long pointer alignments.
 * If compression/uncompression is unsuccessful, the original mbuf list
 * is returned.
 * The first four bytes (the XID) are left uncompressed and the fifth
 * byte is set to 0x1 for request and 0x2 for reply.
 * An uncompressed RPC will always have the fifth byte == 0x0.
 */
struct mbuf *
nfs_compress(m0)
	struct mbuf *m0;
{
	register u_char ch, nextch;
	register int i, rlelast;
	register u_char *ip, *op;
	register int ileft, oleft, noteof;
	register struct mbuf *m, *om;
	struct mbuf **mp, *retm;
	int olen, clget;

	i = rlelast = 0;
	noteof = 1;
	m = m0;
	if (m->m_len < 12)
		return (m0);
	if (m->m_pkthdr.len >= MINCLSIZE)
		clget = 1;
	else
		clget = 0;
	ileft = m->m_len - 9;
	ip = mtod(m, u_char *);
	MGETHDR(om, M_WAIT, MT_DATA);
	if (clget)
		MCLGET(om, M_WAIT);
	retm = om;
	mp = &om->m_next;
	olen = om->m_len = 5;
	oleft = M_TRAILINGSPACE(om);
	op = mtod(om, u_char *);
	*((u_long *)op) = *((u_long *)ip);
	ip += 7;
	op += 4;
	*op++ = *ip++ + 1;
	nextch = *ip++;
	while (noteof) {
		ch = nextch;
		if (ileft == 0) {
			do {
				m = m->m_next;
			} while (m && m->m_len == 0);
			if (m) {
				ileft = m->m_len;
				ip = mtod(m, u_char *);
			} else {
				noteof = 0;
				nextch = 0x1;
				goto doit;
			}
		}
		nextch = *ip++;
		ileft--;
doit:
		if (ch == '\0') {
			if (++i == NFSC_MAX || nextch != '\0') {
				if (i < 2) {
					nfscput('\0');
				} else {
					if (rlelast == i) {
						nfscput('\0');
						i--;
					}
					if (NFSCRLE(i) == (nextch & 0xff)) {
						i--;
						if (i < 2) {
							nfscput('\0');
						} else {
							nfscput(NFSCRLE(i));
						}
						nfscput('\0');
						rlelast = 0;
					} else {
						nfscput(NFSCRLE(i));
						rlelast = i;
					}
				}
				i = 0;
			}
		} else {
			if ((ch & NFSCRL) == NFSCRL) {
				nfscput(ch);
			}
			nfscput(ch);
			i = rlelast = 0;
		}
	}
	if (olen < m0->m_pkthdr.len) {
		m_freem(m0);
		if (i = (olen & 0x3)) {
			i = 4 - i;
			while (i-- > 0) {
				nfscput('\0');
			}
		}
		retm->m_pkthdr.len = olen;
		retm->m_pkthdr.rcvif = (struct ifnet *)0;
		return (retm);
	} else {
		m_freem(retm);
		return (m0);
	}
}

struct mbuf *
nfs_uncompress(m0)
	struct mbuf *m0;
{
	register u_char cp, nextcp, *ip, *op;
	register struct mbuf *m, *om;
	struct mbuf *retm, **mp;
	int i, j, noteof, clget, ileft, oleft, olen;

	m = m0;
	i = 0;
	while (m && i < MINCLSIZE) {
		i += m->m_len;
		m = m->m_next;
	}
	if (i < 6)
		return (m0);
	if (i >= MINCLSIZE)
		clget = 1;
	else
		clget = 0;
	m = m0;
	MGET(om, M_WAIT, MT_DATA);
	if (clget)
		MCLGET(om, M_WAIT);
	olen = om->m_len = 8;
	oleft = M_TRAILINGSPACE(om);
	op = mtod(om, u_char *);
	retm = om;
	mp = &om->m_next;
	if (m->m_len >= 6) {
		ileft = m->m_len - 6;
		ip = mtod(m, u_char *);
		*((u_long *)op) = *((u_long *)ip);
		bzero(op + 4, 3);
		ip += 4;
		op += 7;
		if (*ip == '\0') {
			m_freem(om);
			return (m0);
		}
		*op++ = *ip++ - 1;
		cp = *ip++;
	} else {
		ileft = m->m_len;
		ip = mtod(m, u_char *);
		nfscget(*op++);
		nfscget(*op++);
		nfscget(*op++);
		nfscget(*op++);
		bzero(op, 3);
		op += 3;
		nfscget(*op);
		if (*op == '\0') {
			m_freem(om);
			return (m0);
		}
		(*op)--;
		op++;
		nfscget(cp);
	}
	noteof = 1;
	while (noteof) {
		if ((cp & NFSCRL) == NFSCRL) {
			nfscget(nextcp);
			if (cp == nextcp) {
				nfscput(cp);
				goto readit;
			} else {
				i = (cp & 0xf) + 2;
				for (j = 0; j < i; j++) {
					nfscput('\0');
				}
				cp = nextcp;
			}
		} else {
			nfscput(cp);
readit:
			nfscget(cp);
		}
	}
	m_freem(m0);
	if (i = (olen & 0x3))
		om->m_len -= i;
	return (retm);
}
