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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */
#ifndef APPLEKEXT
#include <fs/nfs/nfsport.h>

extern struct nfsstatsv1 nfsstatsv1;
extern struct nfsv4_opflag nfsv4_opflag[NFSV41_NOPS];
extern int ncl_mbuf_mlen;
extern enum vtype newnv2tov_type[8];
extern enum vtype nv34tov_type[8];
extern int	nfs_bigreply[NFSV41_NPROCS];
NFSCLSTATEMUTEX;
#endif	/* !APPLEKEXT */

static nfsuint64 nfs_nullcookie = {{ 0, 0 }};
static struct {
	int	op;
	int	opcnt;
	const u_char *tag;
	int	taglen;
} nfsv4_opmap[NFSV41_NPROCS] = {
	{ 0, 1, "Null", 4 },
	{ NFSV4OP_GETATTR, 1, "Getattr", 7, },
	{ NFSV4OP_SETATTR, 2, "Setattr", 7, },
	{ NFSV4OP_LOOKUP, 3, "Lookup", 6, },
	{ NFSV4OP_ACCESS, 2, "Access", 6, },
	{ NFSV4OP_READLINK, 2, "Readlink", 8, },
	{ NFSV4OP_READ, 1, "Read", 4, },
	{ NFSV4OP_WRITE, 2, "Write", 5, },
	{ NFSV4OP_OPEN, 5, "Open", 4, },
	{ NFSV4OP_CREATE, 5, "Create", 6, },
	{ NFSV4OP_CREATE, 1, "Create", 6, },
	{ NFSV4OP_CREATE, 3, "Create", 6, },
	{ NFSV4OP_REMOVE, 1, "Remove", 6, },
	{ NFSV4OP_REMOVE, 1, "Remove", 6, },
	{ NFSV4OP_SAVEFH, 5, "Rename", 6, },
	{ NFSV4OP_SAVEFH, 4, "Link", 4, },
	{ NFSV4OP_READDIR, 2, "Readdir", 7, },
	{ NFSV4OP_READDIR, 2, "Readdir", 7, },
	{ NFSV4OP_GETATTR, 1, "Getattr", 7, },
	{ NFSV4OP_GETATTR, 1, "Getattr", 7, },
	{ NFSV4OP_GETATTR, 1, "Getattr", 7, },
	{ NFSV4OP_COMMIT, 2, "Commit", 6, },
	{ NFSV4OP_LOOKUPP, 3, "Lookupp", 7, },
	{ NFSV4OP_SETCLIENTID, 1, "SetClientID", 11, },
	{ NFSV4OP_SETCLIENTIDCFRM, 1, "SetClientIDConfirm", 18, },
	{ NFSV4OP_LOCK, 1, "Lock", 4, },
	{ NFSV4OP_LOCKU, 1, "LockU", 5, },
	{ NFSV4OP_OPEN, 2, "Open", 4, },
	{ NFSV4OP_CLOSE, 1, "Close", 5, },
	{ NFSV4OP_OPENCONFIRM, 1, "Openconfirm", 11, },
	{ NFSV4OP_LOCKT, 1, "LockT", 5, },
	{ NFSV4OP_OPENDOWNGRADE, 1, "Opendowngrade", 13, },
	{ NFSV4OP_RENEW, 1, "Renew", 5, },
	{ NFSV4OP_PUTROOTFH, 1, "Dirpath", 7, },
	{ NFSV4OP_RELEASELCKOWN, 1, "Rellckown", 9, },
	{ NFSV4OP_DELEGRETURN, 1, "Delegret", 8, },
	{ NFSV4OP_DELEGRETURN, 3, "DelegRemove", 11, },
	{ NFSV4OP_DELEGRETURN, 7, "DelegRename1", 12, },
	{ NFSV4OP_DELEGRETURN, 9, "DelegRename2", 12, },
	{ NFSV4OP_GETATTR, 1, "Getacl", 6, },
	{ NFSV4OP_SETATTR, 1, "Setacl", 6, },
	{ NFSV4OP_EXCHANGEID, 1, "ExchangeID", 10, },
	{ NFSV4OP_CREATESESSION, 1, "CreateSession", 13, },
	{ NFSV4OP_DESTROYSESSION, 1, "DestroySession", 14, },
	{ NFSV4OP_DESTROYCLIENTID, 1, "DestroyClient", 13, },
	{ NFSV4OP_FREESTATEID, 1, "FreeStateID", 11, },
	{ NFSV4OP_LAYOUTGET, 1, "LayoutGet", 9, },
	{ NFSV4OP_GETDEVINFO, 1, "GetDeviceInfo", 13, },
	{ NFSV4OP_LAYOUTCOMMIT, 1, "LayoutCommit", 12, },
	{ NFSV4OP_LAYOUTRETURN, 1, "LayoutReturn", 12, },
	{ NFSV4OP_RECLAIMCOMPL, 1, "ReclaimComplete", 15, },
	{ NFSV4OP_WRITE, 1, "WriteDS", 7, },
	{ NFSV4OP_READ, 1, "ReadDS", 6, },
	{ NFSV4OP_COMMIT, 1, "CommitDS", 8, },
	{ NFSV4OP_OPEN, 3, "OpenLayoutGet", 13, },
	{ NFSV4OP_OPEN, 8, "CreateLayGet", 12, },
};

/*
 * NFS RPCS that have large request message size.
 */
static int nfs_bigrequest[NFSV41_NPROCS] = {
	0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0
};

/*
 * Start building a request. Mostly just put the first file handle in
 * place.
 */
APPLESTATIC void
nfscl_reqstart(struct nfsrv_descript *nd, int procnum, struct nfsmount *nmp,
    u_int8_t *nfhp, int fhlen, u_int32_t **opcntpp, struct nfsclsession *sep,
    int vers, int minorvers)
{
	struct mbuf *mb;
	u_int32_t *tl;
	int opcnt;
	nfsattrbit_t attrbits;

	/*
	 * First, fill in some of the fields of nd.
	 */
	nd->nd_slotseq = NULL;
	if (vers == NFS_VER4) {
		nd->nd_flag = ND_NFSV4 | ND_NFSCL;
		if (minorvers == NFSV41_MINORVERSION)
			nd->nd_flag |= ND_NFSV41;
	} else if (vers == NFS_VER3)
		nd->nd_flag = ND_NFSV3 | ND_NFSCL;
	else {
		if (NFSHASNFSV4(nmp)) {
			nd->nd_flag = ND_NFSV4 | ND_NFSCL;
			if (NFSHASNFSV4N(nmp))
				nd->nd_flag |= ND_NFSV41;
		} else if (NFSHASNFSV3(nmp))
			nd->nd_flag = ND_NFSV3 | ND_NFSCL;
		else
			nd->nd_flag = ND_NFSV2 | ND_NFSCL;
	}
	nd->nd_procnum = procnum;
	nd->nd_repstat = 0;

	/*
	 * Get the first mbuf for the request.
	 */
	if (nfs_bigrequest[procnum])
		NFSMCLGET(mb, M_WAITOK);
	else
		NFSMGET(mb);
	mbuf_setlen(mb, 0);
	nd->nd_mreq = nd->nd_mb = mb;
	nd->nd_bpos = NFSMTOD(mb, caddr_t);
	
	/*
	 * And fill the first file handle into the request.
	 */
	if (nd->nd_flag & ND_NFSV4) {
		opcnt = nfsv4_opmap[procnum].opcnt +
		    nfsv4_opflag[nfsv4_opmap[procnum].op].needscfh;
		if ((nd->nd_flag & ND_NFSV41) != 0) {
			opcnt += nfsv4_opflag[nfsv4_opmap[procnum].op].needsseq;
			if (procnum == NFSPROC_RENEW)
				/*
				 * For the special case of Renew, just do a
				 * Sequence Op.
				 */
				opcnt = 1;
			else if (procnum == NFSPROC_WRITEDS ||
			    procnum == NFSPROC_COMMITDS)
				/*
				 * For the special case of a Writeor Commit to
				 * a DS, the opcnt == 3, for Sequence, PutFH,
				 * Write/Commit.
				 */
				opcnt = 3;
		}
		/*
		 * What should the tag really be?
		 */
		(void) nfsm_strtom(nd, nfsv4_opmap[procnum].tag,
			nfsv4_opmap[procnum].taglen);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		if ((nd->nd_flag & ND_NFSV41) != 0)
			*tl++ = txdr_unsigned(NFSV41_MINORVERSION);
		else
			*tl++ = txdr_unsigned(NFSV4_MINORVERSION);
		if (opcntpp != NULL)
			*opcntpp = tl;
		*tl = txdr_unsigned(opcnt);
		if ((nd->nd_flag & ND_NFSV41) != 0 &&
		    nfsv4_opflag[nfsv4_opmap[procnum].op].needsseq > 0) {
			if (nfsv4_opflag[nfsv4_opmap[procnum].op].loopbadsess >
			    0)
				nd->nd_flag |= ND_LOOPBADSESS;
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_SEQUENCE);
			if (sep == NULL) {
				sep = nfsmnt_mdssession(nmp);
				nfsv4_setsequence(nmp, nd, sep,
				    nfs_bigreply[procnum]);
			} else
				nfsv4_setsequence(nmp, nd, sep,
				    nfs_bigreply[procnum]);
		}
		if (nfsv4_opflag[nfsv4_opmap[procnum].op].needscfh > 0) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_PUTFH);
			(void) nfsm_fhtom(nd, nfhp, fhlen, 0);
			if (nfsv4_opflag[nfsv4_opmap[procnum].op].needscfh
			    == 2 && procnum != NFSPROC_WRITEDS &&
			    procnum != NFSPROC_COMMITDS) {
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(NFSV4OP_GETATTR);
				/*
				 * For Lookup Ops, we want all the directory
				 * attributes, so we can load the name cache.
				 */
				if (procnum == NFSPROC_LOOKUP ||
				    procnum == NFSPROC_LOOKUPP)
					NFSGETATTR_ATTRBIT(&attrbits);
				else {
					NFSWCCATTR_ATTRBIT(&attrbits);
					nd->nd_flag |= ND_V4WCCATTR;
				}
				(void) nfsrv_putattrbit(nd, &attrbits);
			}
		}
		if (procnum != NFSPROC_RENEW ||
		    (nd->nd_flag & ND_NFSV41) == 0) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(nfsv4_opmap[procnum].op);
		}
	} else {
		(void) nfsm_fhtom(nd, nfhp, fhlen, 0);
	}
	if (procnum < NFSV41_NPROCS)
		NFSINCRGLOBAL(nfsstatsv1.rpccnt[procnum]);
}

/*
 * copies a uio scatter/gather list to an mbuf chain.
 * NOTE: can ony handle iovcnt == 1
 */
APPLESTATIC void
nfsm_uiombuf(struct nfsrv_descript *nd, struct uio *uiop, int siz)
{
	char *uiocp;
	struct mbuf *mp, *mp2;
	int xfer, left, mlen;
	int uiosiz, clflg, rem;
	char *cp, *tcp;

	KASSERT(uiop->uio_iovcnt == 1, ("nfsm_uiotombuf: iovcnt != 1"));

	if (siz > ncl_mbuf_mlen)	/* or should it >= MCLBYTES ?? */
		clflg = 1;
	else
		clflg = 0;
	rem = NFSM_RNDUP(siz) - siz;
	mp = mp2 = nd->nd_mb;
	while (siz > 0) {
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			mlen = M_TRAILINGSPACE(mp);
			if (mlen == 0) {
				if (clflg)
					NFSMCLGET(mp, M_WAITOK);
				else
					NFSMGET(mp);
				mbuf_setlen(mp, 0);
				mbuf_setnext(mp2, mp);
				mp2 = mp;
				mlen = M_TRAILINGSPACE(mp);
			}
			xfer = (left > mlen) ? mlen : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(uiocp, NFSMTOD(mp, caddr_t) + mbuf_len(mp),
				    xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
			    NFSBCOPY(uiocp, NFSMTOD(mp, caddr_t) + mbuf_len(mp),
				xfer);
			else
			    copyin(CAST_USER_ADDR_T(uiocp), NFSMTOD(mp, caddr_t)
				+ mbuf_len(mp), xfer);
			mbuf_setlen(mp, mbuf_len(mp) + xfer);
			left -= xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		tcp = (char *)uiop->uio_iov->iov_base;
		tcp += uiosiz;
		uiop->uio_iov->iov_base = (void *)tcp;
		uiop->uio_iov->iov_len -= uiosiz;
		siz -= uiosiz;
	}
	if (rem > 0) {
		if (rem > M_TRAILINGSPACE(mp)) {
			NFSMGET(mp);
			mbuf_setlen(mp, 0);
			mbuf_setnext(mp2, mp);
		}
		cp = NFSMTOD(mp, caddr_t) + mbuf_len(mp);
		for (left = 0; left < rem; left++)
			*cp++ = '\0';
		mbuf_setlen(mp, mbuf_len(mp) + rem);
		nd->nd_bpos = cp;
	} else
		nd->nd_bpos = NFSMTOD(mp, caddr_t) + mbuf_len(mp);
	nd->nd_mb = mp;
}

/*
 * copies a uio scatter/gather list to an mbuf chain.
 * This version returns the mbuf list and does not use "nd".
 * NOTE: can ony handle iovcnt == 1
 */
struct mbuf *
nfsm_uiombuflist(struct uio *uiop, int siz, struct mbuf **mbp, char **cpp)
{
	char *uiocp;
	struct mbuf *mp, *mp2, *firstmp;
	int xfer, left, mlen;
	int uiosiz, clflg;
	char *tcp;

	KASSERT(uiop->uio_iovcnt == 1, ("nfsm_uiotombuf: iovcnt != 1"));

	if (siz > ncl_mbuf_mlen)	/* or should it >= MCLBYTES ?? */
		clflg = 1;
	else
		clflg = 0;
	if (clflg != 0)
		NFSMCLGET(mp, M_WAITOK);
	else
		NFSMGET(mp);
	mbuf_setlen(mp, 0);
	firstmp = mp2 = mp;
	while (siz > 0) {
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			mlen = M_TRAILINGSPACE(mp);
			if (mlen == 0) {
				if (clflg)
					NFSMCLGET(mp, M_WAITOK);
				else
					NFSMGET(mp);
				mbuf_setlen(mp, 0);
				mbuf_setnext(mp2, mp);
				mp2 = mp;
				mlen = M_TRAILINGSPACE(mp);
			}
			xfer = (left > mlen) ? mlen : left;
			if (uiop->uio_segflg == UIO_SYSSPACE)
				NFSBCOPY(uiocp, NFSMTOD(mp, caddr_t) +
				    mbuf_len(mp), xfer);
			else
				copyin(uiocp, NFSMTOD(mp, caddr_t) +
				    mbuf_len(mp), xfer);
			mbuf_setlen(mp, mbuf_len(mp) + xfer);
			left -= xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		tcp = (char *)uiop->uio_iov->iov_base;
		tcp += uiosiz;
		uiop->uio_iov->iov_base = (void *)tcp;
		uiop->uio_iov->iov_len -= uiosiz;
		siz -= uiosiz;
	}
	if (cpp != NULL)
		*cpp = NFSMTOD(mp, caddr_t) + mbuf_len(mp);
	if (mbp != NULL)
		*mbp = mp;
	return (firstmp);
}

/*
 * Load vnode attributes from the xdr file attributes.
 * Returns EBADRPC if they can't be parsed, 0 otherwise.
 */
APPLESTATIC int
nfsm_loadattr(struct nfsrv_descript *nd, struct nfsvattr *nap)
{
	struct nfs_fattr *fp;
	int error = 0;

	if (nd->nd_flag & ND_NFSV4) {
		error = nfsv4_loadattr(nd, NULL, nap, NULL, NULL, 0, NULL,
		    NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL);
	} else if (nd->nd_flag & ND_NFSV3) {
		NFSM_DISSECT(fp, struct nfs_fattr *, NFSX_V3FATTR);
		nap->na_type = nfsv34tov_type(fp->fa_type);
		nap->na_mode = fxdr_unsigned(u_short, fp->fa_mode);
		nap->na_rdev = NFSMAKEDEV(
		    fxdr_unsigned(int, fp->fa3_rdev.specdata1),
		    fxdr_unsigned(int, fp->fa3_rdev.specdata2));
		nap->na_nlink = fxdr_unsigned(uint32_t, fp->fa_nlink);
		nap->na_uid = fxdr_unsigned(uid_t, fp->fa_uid);
		nap->na_gid = fxdr_unsigned(gid_t, fp->fa_gid);
		nap->na_size = fxdr_hyper(&fp->fa3_size);
		nap->na_blocksize = NFS_FABLKSIZE;
		nap->na_bytes = fxdr_hyper(&fp->fa3_used);
		nap->na_fileid = fxdr_hyper(&fp->fa3_fileid);
		fxdr_nfsv3time(&fp->fa3_atime, &nap->na_atime);
		fxdr_nfsv3time(&fp->fa3_ctime, &nap->na_ctime);
		fxdr_nfsv3time(&fp->fa3_mtime, &nap->na_mtime);
		nap->na_flags = 0;
		nap->na_filerev = 0;
	} else {
		NFSM_DISSECT(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nap->na_type = nfsv2tov_type(fp->fa_type);
		nap->na_mode = fxdr_unsigned(u_short, fp->fa_mode);
		if (nap->na_type == VNON || nap->na_type == VREG)
			nap->na_type = IFTOVT(nap->na_mode);
		nap->na_rdev = fxdr_unsigned(dev_t, fp->fa2_rdev);

		/*
		 * Really ugly NFSv2 kludge.
		 */
		if (nap->na_type == VCHR && nap->na_rdev == ((dev_t)-1))
			nap->na_type = VFIFO;
		nap->na_nlink = fxdr_unsigned(u_short, fp->fa_nlink);
		nap->na_uid = fxdr_unsigned(uid_t, fp->fa_uid);
		nap->na_gid = fxdr_unsigned(gid_t, fp->fa_gid);
		nap->na_size = fxdr_unsigned(u_int32_t, fp->fa2_size);
		nap->na_blocksize = fxdr_unsigned(int32_t, fp->fa2_blocksize);
		nap->na_bytes =
		    (u_quad_t)fxdr_unsigned(int32_t, fp->fa2_blocks) *
		    NFS_FABLKSIZE;
		nap->na_fileid = fxdr_unsigned(uint64_t, fp->fa2_fileid);
		fxdr_nfsv2time(&fp->fa2_atime, &nap->na_atime);
		fxdr_nfsv2time(&fp->fa2_mtime, &nap->na_mtime);
		nap->na_flags = 0;
		nap->na_ctime.tv_sec = fxdr_unsigned(u_int32_t,
		    fp->fa2_ctime.nfsv2_sec);
		nap->na_ctime.tv_nsec = 0;
		nap->na_gen = fxdr_unsigned(u_int32_t,fp->fa2_ctime.nfsv2_usec);
		nap->na_filerev = 0;
	}
nfsmout:
	return (error);
}

/*
 * This function finds the directory cookie that corresponds to the
 * logical byte offset given.
 */
APPLESTATIC nfsuint64 *
nfscl_getcookie(struct nfsnode *np, off_t off, int add)
{
	struct nfsdmap *dp, *dp2;
	int pos;

	pos = off / NFS_DIRBLKSIZ;
	if (pos == 0) {
		KASSERT(!add, ("nfs getcookie add at 0"));
		return (&nfs_nullcookie);
	}
	pos--;
	dp = LIST_FIRST(&np->n_cookies);
	if (!dp) {
		if (add) {
			dp = malloc(sizeof (struct nfsdmap),
				M_NFSDIROFF, M_WAITOK);
			dp->ndm_eocookie = 0;
			LIST_INSERT_HEAD(&np->n_cookies, dp, ndm_list);
		} else
			return (NULL);
	}
	while (pos >= NFSNUMCOOKIES) {
		pos -= NFSNUMCOOKIES;
		if (LIST_NEXT(dp, ndm_list) != NULL) {
			if (!add && dp->ndm_eocookie < NFSNUMCOOKIES &&
				pos >= dp->ndm_eocookie)
				return (NULL);
			dp = LIST_NEXT(dp, ndm_list);
		} else if (add) {
			dp2 = malloc(sizeof (struct nfsdmap),
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
 * Gets a file handle out of an nfs reply sent to the client and returns
 * the file handle and the file's attributes.
 * For V4, it assumes that Getfh and Getattr Op's results are here.
 */
APPLESTATIC int
nfscl_mtofh(struct nfsrv_descript *nd, struct nfsfh **nfhpp,
    struct nfsvattr *nap, int *attrflagp)
{
	u_int32_t *tl;
	int error = 0, flag = 1;

	*nfhpp = NULL;
	*attrflagp = 0;
	/*
	 * First get the file handle and vnode.
	 */
	if (nd->nd_flag & ND_NFSV3) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		flag = fxdr_unsigned(int, *tl);
	} else if (nd->nd_flag & ND_NFSV4) {
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		/* If the GetFH failed, clear flag. */
		if (*++tl != 0) {
			nd->nd_flag |= ND_NOMOREDATA;
			flag = 0;
			error = ENXIO;	/* Return ENXIO so *nfhpp isn't used. */
		}
	}
	if (flag) {
		error = nfsm_getfh(nd, nfhpp);
		if (error)
			return (error);
	}

	/*
	 * Now, get the attributes.
	 */
	if (flag != 0 && (nd->nd_flag & ND_NFSV4) != 0) {
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		if (*++tl != 0) {
			nd->nd_flag |= ND_NOMOREDATA;
			flag = 0;
		}
	} else if (nd->nd_flag & ND_NFSV3) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		if (flag) {
			flag = fxdr_unsigned(int, *tl);
		} else if (fxdr_unsigned(int, *tl)) {
			error = nfsm_advance(nd, NFSX_V3FATTR, -1);
			if (error)
				return (error);
		}
	}
	if (flag) {
		error = nfsm_loadattr(nd, nap);
		if (!error)
			*attrflagp = 1;
	}
nfsmout:
	return (error);
}

/*
 * Put a state Id in the mbuf list.
 */
APPLESTATIC void
nfsm_stateidtom(struct nfsrv_descript *nd, nfsv4stateid_t *stateidp, int flag)
{
	nfsv4stateid_t *st;

	NFSM_BUILD(st, nfsv4stateid_t *, NFSX_STATEID);
	if (flag == NFSSTATEID_PUTALLZERO) {
		st->seqid = 0;
		st->other[0] = 0;
		st->other[1] = 0;
		st->other[2] = 0;
	} else if (flag == NFSSTATEID_PUTALLONE) {
		st->seqid = 0xffffffff;
		st->other[0] = 0xffffffff;
		st->other[1] = 0xffffffff;
		st->other[2] = 0xffffffff;
	} else if (flag == NFSSTATEID_PUTSEQIDZERO) {
		st->seqid = 0;
		st->other[0] = stateidp->other[0];
		st->other[1] = stateidp->other[1];
		st->other[2] = stateidp->other[2];
	} else {
		st->seqid = stateidp->seqid;
		st->other[0] = stateidp->other[0];
		st->other[1] = stateidp->other[1];
		st->other[2] = stateidp->other[2];
	}
}

/*
 * Initialize the owner/delegation sleep lock.
 */
APPLESTATIC void
nfscl_lockinit(struct nfsv4lock *lckp)
{

	lckp->nfslock_usecnt = 0;
	lckp->nfslock_lock = 0;
}

/*
 * Get an exclusive lock. (Not needed for OpenBSD4, since there is only one
 * thread for each posix process in the kernel.)
 */
APPLESTATIC void
nfscl_lockexcl(struct nfsv4lock *lckp, void *mutex)
{
	int igotlock;

	do {
		igotlock = nfsv4_lock(lckp, 1, NULL, mutex, NULL);
	} while (!igotlock);
}

/*
 * Release an exclusive lock.
 */
APPLESTATIC void
nfscl_lockunlock(struct nfsv4lock *lckp)
{

	nfsv4_unlock(lckp, 0);
}

/*
 * Called to derefernce a lock on a stateid (delegation or open owner).
 */
APPLESTATIC void
nfscl_lockderef(struct nfsv4lock *lckp)
{

	NFSLOCKCLSTATE();
	lckp->nfslock_usecnt--;
	if (lckp->nfslock_usecnt == 0 && (lckp->nfslock_lock & NFSV4LOCK_WANTED)) {
		lckp->nfslock_lock &= ~NFSV4LOCK_WANTED;
		wakeup((caddr_t)lckp);
	}
	NFSUNLOCKCLSTATE();
}

