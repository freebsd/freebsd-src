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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Rpc op calls, generally called from the vnode op calls or through the
 * buffer cache, for NFS v2, 3 and 4.
 * These do not normally make any changes to vnode arguments or use
 * structures that might change between the VFS variants. The returned
 * arguments are all at the end, after the NFSPROC_T *p one.
 */

#ifndef APPLEKEXT
#include <fs/nfs/nfsport.h>

/*
 * Global variables
 */
extern int nfs_numnfscbd;
extern struct timeval nfsboottime;
extern u_int32_t newnfs_false, newnfs_true;
extern nfstype nfsv34_type[9];
extern int nfsrv_useacl;
extern char nfsv4_callbackaddr[INET6_ADDRSTRLEN];
NFSCLSTATEMUTEX;
int nfstest_outofseq = 0;
int nfscl_assumeposixlocks = 1;
int nfscl_enablecallb = 0;
short nfsv4_cbport = NFSV4_CBPORT;
int nfstest_openallsetattr = 0;
#endif	/* !APPLEKEXT */

#define	DIRHDSIZ	(sizeof (struct dirent) - (MAXNAMLEN + 1))

static int nfsrpc_setattrrpc(vnode_t , struct vattr *, nfsv4stateid_t *,
    struct ucred *, NFSPROC_T *, struct nfsvattr *, int *, void *);
static int nfsrpc_readrpc(vnode_t , struct uio *, struct ucred *,
    nfsv4stateid_t *, NFSPROC_T *, struct nfsvattr *, int *, void *);
static int nfsrpc_writerpc(vnode_t , struct uio *, int *, u_char *,
    struct ucred *, nfsv4stateid_t *, NFSPROC_T *, struct nfsvattr *, int *,
    void *);
static int nfsrpc_createv23(vnode_t , char *, int, struct vattr *,
    nfsquad_t, int, struct ucred *, NFSPROC_T *, struct nfsvattr *,
    struct nfsvattr *, struct nfsfh **, int *, int *, void *);
static int nfsrpc_createv4(vnode_t , char *, int, struct vattr *,
    nfsquad_t, int, struct nfsclowner *, struct nfscldeleg **, struct ucred *,
    NFSPROC_T *, struct nfsvattr *, struct nfsvattr *, struct nfsfh **, int *,
    int *, void *, int *);
static int nfsrpc_locku(struct nfsrv_descript *, struct nfsmount *,
    struct nfscllockowner *, u_int64_t, u_int64_t,
    u_int32_t, struct ucred *, NFSPROC_T *, int);
static int nfsrpc_setaclrpc(vnode_t, struct ucred *, NFSPROC_T *,
    struct acl *, nfsv4stateid_t *, void *);

/*
 * nfs null call from vfs.
 */
APPLESTATIC int
nfsrpc_null(vnode_t vp, struct ucred *cred, NFSPROC_T *p)
{
	int error;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	
	NFSCL_REQSTART(nd, NFSPROC_NULL, vp);
	error = nfscl_request(nd, vp, p, cred, NULL);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs access rpc op.
 * For nfs version 3 and 4, use the access rpc to check accessibility. If file
 * modes are changed on the server, accesses might still fail later.
 */
APPLESTATIC int
nfsrpc_access(vnode_t vp, int acmode, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp)
{
	int error;
	u_int32_t mode, rmode;

	if (acmode & VREAD)
		mode = NFSACCESS_READ;
	else
		mode = 0;
	if (vnode_vtype(vp) == VDIR) {
		if (acmode & VWRITE)
			mode |= (NFSACCESS_MODIFY | NFSACCESS_EXTEND |
				 NFSACCESS_DELETE);
		if (acmode & VEXEC)
			mode |= NFSACCESS_LOOKUP;
	} else {
		if (acmode & VWRITE)
			mode |= (NFSACCESS_MODIFY | NFSACCESS_EXTEND);
		if (acmode & VEXEC)
			mode |= NFSACCESS_EXECUTE;
	}

	/*
	 * Now, just call nfsrpc_accessrpc() to do the actual RPC.
	 */
	error = nfsrpc_accessrpc(vp, mode, cred, p, nap, attrflagp, &rmode,
	    NULL);

	/*
	 * The NFS V3 spec does not clarify whether or not
	 * the returned access bits can be a superset of
	 * the ones requested, so...
	 */
	if (!error && (rmode & mode) != mode)
		error = EACCES;
	return (error);
}

/*
 * The actual rpc, separated out for Darwin.
 */
APPLESTATIC int
nfsrpc_accessrpc(vnode_t vp, u_int32_t mode, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp, u_int32_t *rmodep,
    void *stuff)
{
	u_int32_t *tl;
	u_int32_t supported, rmode;
	int error;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	supported = mode;
	NFSCL_REQSTART(nd, NFSPROC_ACCESS, vp);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(mode);
	if (nd->nd_flag & ND_NFSV4) {
		/*
		 * And do a Getattr op.
		 */
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV3) {
		error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
		if (error)
			goto nfsmout;
	}
	if (!nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV4) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			supported = fxdr_unsigned(u_int32_t, *tl++);
		} else {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		}
		rmode = fxdr_unsigned(u_int32_t, *tl);
		if (nd->nd_flag & ND_NFSV4)
			error = nfscl_postop_attr(nd, nap, attrflagp, stuff);

		/*
		 * It's not obvious what should be done about
		 * unsupported access modes. For now, be paranoid
		 * and clear the unsupported ones.
		 */
		rmode &= supported;
		*rmodep = rmode;
	} else
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs open rpc
 */
APPLESTATIC int
nfsrpc_open(vnode_t vp, int amode, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsclopen *op;
	struct nfscldeleg *dp;
	struct nfsfh *nfhp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	u_int32_t mode, clidrev;
	int ret, newone, error, expireret = 0, retrycnt;

	/*
	 * For NFSv4, Open Ops are only done on Regular Files.
	 */
	if (vnode_vtype(vp) != VREG)
		return (0);
	mode = 0;
	if (amode & FREAD)
		mode |= NFSV4OPEN_ACCESSREAD;
	if (amode & FWRITE)
		mode |= NFSV4OPEN_ACCESSWRITE;
	nfhp = np->n_fhp;

	retrycnt = 0;
#ifdef notdef
{ char name[100]; int namel;
namel = (np->n_v4->n4_namelen < 100) ? np->n_v4->n4_namelen : 99;
bcopy(NFS4NODENAME(np->n_v4), name, namel);
name[namel] = '\0';
printf("rpcopen p=0x%x name=%s",p->p_pid,name);
if (nfhp->nfh_len > 0) printf(" fh=0x%x\n",nfhp->nfh_fh[12]);
else printf(" fhl=0\n");
}
#endif
	do {
	    dp = NULL;
	    error = nfscl_open(vp, nfhp->nfh_fh, nfhp->nfh_len, mode, 1,
		cred, p, NULL, &op, &newone, &ret, 1);
	    if (error) {
		return (error);
	    }
	    if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	    else
		clidrev = 0;
	    if (ret == NFSCLOPEN_DOOPEN) {
		if (np->n_v4 != NULL) {
			error = nfsrpc_openrpc(nmp, vp, np->n_v4->n4_data,
			   np->n_v4->n4_fhlen, np->n_fhp->nfh_fh,
			   np->n_fhp->nfh_len, mode, op,
			   NFS4NODENAME(np->n_v4), np->n_v4->n4_namelen, &dp,
			   0, 0x0, cred, p, 0, 0);
			if (dp != NULL) {
#ifdef APPLE
				OSBitAndAtomic((int32_t)~NDELEGMOD, (UInt32 *)&np->n_flag);
#else
				NFSLOCKNODE(np);
				np->n_flag &= ~NDELEGMOD;
				NFSUNLOCKNODE(np);
#endif
				(void) nfscl_deleg(nmp->nm_mountp,
				    op->nfso_own->nfsow_clp,
				    nfhp->nfh_fh, nfhp->nfh_len, cred, p, &dp);
			}
		} else {
			error = EIO;
		}
		newnfs_copyincred(cred, &op->nfso_cred);
	    }

	    /*
	     * nfso_opencnt is the count of how many VOP_OPEN()s have
	     * been done on this Open successfully and a VOP_CLOSE()
	     * is expected for each of these.
	     * If error is non-zero, don't increment it, since the Open
	     * hasn't succeeded yet.
	     */
	    if (!error)
		op->nfso_opencnt++;
	    nfscl_openrelease(op, error, newone);
	    if (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
		error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY) {
		(void) nfs_catnap(PZERO, "nfs_open");
	    } else if ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID)
		&& clidrev != 0) {
		expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		retrycnt++;
	    }
	} while (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * the actual open rpc
 */
APPLESTATIC int
nfsrpc_openrpc(struct nfsmount *nmp, vnode_t vp, u_int8_t *nfhp, int fhlen,
    u_int8_t *newfhp, int newfhlen, u_int32_t mode, struct nfsclopen *op,
    u_int8_t *name, int namelen, struct nfscldeleg **dpp,
    int reclaim, u_int32_t delegtype, struct ucred *cred, NFSPROC_T *p,
    int syscred, int recursed)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfscldeleg *dp, *ndp = NULL;
	struct nfsvattr nfsva;
	u_int32_t rflags, deleg;
	nfsattrbit_t attrbits;
	int error, ret, acesize, limitby;

	dp = *dpp;
	*dpp = NULL;
	nfscl_reqstart(nd, NFSPROC_OPEN, nmp, nfhp, fhlen, NULL);
	NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(op->nfso_own->nfsow_seqid);
	*tl++ = txdr_unsigned(mode & NFSV4OPEN_ACCESSBOTH);
	*tl++ = txdr_unsigned((mode >> NFSLCK_SHIFT) & NFSV4OPEN_DENYBOTH);
	*tl++ = op->nfso_own->nfsow_clp->nfsc_clientid.lval[0];
	*tl = op->nfso_own->nfsow_clp->nfsc_clientid.lval[1];
	(void) nfsm_strtom(nd, op->nfso_own->nfsow_owner, NFSV4CL_LOCKNAMELEN);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OPEN_NOCREATE);
	if (reclaim) {
		*tl = txdr_unsigned(NFSV4OPEN_CLAIMPREVIOUS);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(delegtype);
	} else {
		if (dp != NULL) {
			*tl = txdr_unsigned(NFSV4OPEN_CLAIMDELEGATECUR);
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
			*tl++ = dp->nfsdl_stateid.seqid;
			*tl++ = dp->nfsdl_stateid.other[0];
			*tl++ = dp->nfsdl_stateid.other[1];
			*tl = dp->nfsdl_stateid.other[2];
		} else {
			*tl = txdr_unsigned(NFSV4OPEN_CLAIMNULL);
		}
		(void) nfsm_strtom(nd, name, namelen);
	}
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSZERO_ATTRBIT(&attrbits);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_CHANGE);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_TIMEMODIFY);
	(void) nfsrv_putattrbit(nd, &attrbits);
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, vp, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	NFSCL_INCRSEQID(op->nfso_own->nfsow_seqid, nd);
	if (!nd->nd_repstat) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
		    6 * NFSX_UNSIGNED);
		op->nfso_stateid.seqid = *tl++;
		op->nfso_stateid.other[0] = *tl++;
		op->nfso_stateid.other[1] = *tl++;
		op->nfso_stateid.other[2] = *tl;
		rflags = fxdr_unsigned(u_int32_t, *(tl + 6));
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		if (error)
			goto nfsmout;
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		deleg = fxdr_unsigned(u_int32_t, *tl);
		if (deleg == NFSV4OPEN_DELEGATEREAD ||
		    deleg == NFSV4OPEN_DELEGATEWRITE) {
			if (!(op->nfso_own->nfsow_clp->nfsc_flags &
			      NFSCLFLAGS_FIRSTDELEG))
				op->nfso_own->nfsow_clp->nfsc_flags |=
				  (NFSCLFLAGS_FIRSTDELEG | NFSCLFLAGS_GOTDELEG);
			MALLOC(ndp, struct nfscldeleg *,
			    sizeof (struct nfscldeleg) + newfhlen,
			    M_NFSCLDELEG, M_WAITOK);
			LIST_INIT(&ndp->nfsdl_owner);
			LIST_INIT(&ndp->nfsdl_lock);
			ndp->nfsdl_clp = op->nfso_own->nfsow_clp;
			ndp->nfsdl_fhlen = newfhlen;
			NFSBCOPY(newfhp, ndp->nfsdl_fh, newfhlen);
			newnfs_copyincred(cred, &ndp->nfsdl_cred);
			nfscl_lockinit(&ndp->nfsdl_rwlock);
			NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
			    NFSX_UNSIGNED);
			ndp->nfsdl_stateid.seqid = *tl++;
			ndp->nfsdl_stateid.other[0] = *tl++;
			ndp->nfsdl_stateid.other[1] = *tl++;
			ndp->nfsdl_stateid.other[2] = *tl++;
			ret = fxdr_unsigned(int, *tl);
			if (deleg == NFSV4OPEN_DELEGATEWRITE) {
				ndp->nfsdl_flags = NFSCLDL_WRITE;
				/*
				 * Indicates how much the file can grow.
				 */
				NFSM_DISSECT(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
				limitby = fxdr_unsigned(int, *tl++);
				switch (limitby) {
				case NFSV4OPEN_LIMITSIZE:
					ndp->nfsdl_sizelimit = fxdr_hyper(tl);
					break;
				case NFSV4OPEN_LIMITBLOCKS:
					ndp->nfsdl_sizelimit =
					    fxdr_unsigned(u_int64_t, *tl++);
					ndp->nfsdl_sizelimit *=
					    fxdr_unsigned(u_int64_t, *tl);
					break;
				default:
					error = NFSERR_BADXDR;
					goto nfsmout;
				};
			} else {
				ndp->nfsdl_flags = NFSCLDL_READ;
			}
			if (ret)
				ndp->nfsdl_flags |= NFSCLDL_RECALL;
			error = nfsrv_dissectace(nd, &ndp->nfsdl_ace, &ret,
			    &acesize, p);
			if (error)
				goto nfsmout;
		} else if (deleg != NFSV4OPEN_DELEGATENONE) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
		    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
		    NULL, NULL, NULL, p, cred);
		if (error)
			goto nfsmout;
		if (ndp != NULL) {
			ndp->nfsdl_change = nfsva.na_filerev;
			ndp->nfsdl_modtime = nfsva.na_mtime;
			ndp->nfsdl_flags |= NFSCLDL_MODTIMESET;
		}
		if (!reclaim && (rflags & NFSV4OPEN_RESULTCONFIRM)) {
		    do {
			ret = nfsrpc_openconfirm(vp, newfhp, newfhlen, op,
			    cred, p);
			if (ret == NFSERR_DELAY)
			    (void) nfs_catnap(PZERO, "nfs_open");
		    } while (ret == NFSERR_DELAY);
		    error = ret;
		}
		if ((rflags & NFSV4OPEN_LOCKTYPEPOSIX) ||
		    nfscl_assumeposixlocks)
		    op->nfso_posixlock = 1;
		else
		    op->nfso_posixlock = 0;

		/*
		 * If the server is handing out delegations, but we didn't
		 * get one because an OpenConfirm was required, try the
		 * Open again, to get a delegation. This is a harmless no-op,
		 * from a server's point of view.
		 */
		if (!reclaim && (rflags & NFSV4OPEN_RESULTCONFIRM) &&
		    (op->nfso_own->nfsow_clp->nfsc_flags & NFSCLFLAGS_GOTDELEG)
		    && !error && dp == NULL && ndp == NULL && !recursed) {
		    do {
			ret = nfsrpc_openrpc(nmp, vp, nfhp, fhlen, newfhp,
			    newfhlen, mode, op, name, namelen, &ndp, 0, 0x0,
			    cred, p, syscred, 1);
			if (ret == NFSERR_DELAY)
			    (void) nfs_catnap(PZERO, "nfs_open2");
		    } while (ret == NFSERR_DELAY);
		    if (ret) {
			if (ndp != NULL)
				FREE((caddr_t)ndp, M_NFSCLDELEG);
			if (ret == NFSERR_STALECLIENTID ||
			    ret == NFSERR_STALEDONTRECOVER)
				error = ret;
		    }
		}
	}
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
	if (error == NFSERR_STALECLIENTID)
		nfscl_initiate_recovery(op->nfso_own->nfsow_clp);
nfsmout:
	if (!error)
		*dpp = ndp;
	else if (ndp != NULL)
		FREE((caddr_t)ndp, M_NFSCLDELEG);
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * open downgrade rpc
 */
APPLESTATIC int
nfsrpc_opendowngrade(vnode_t vp, u_int32_t mode, struct nfsclopen *op,
    struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	NFSCL_REQSTART(nd, NFSPROC_OPENDOWNGRADE, vp);
	NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID + 3 * NFSX_UNSIGNED);
	*tl++ = op->nfso_stateid.seqid;
	*tl++ = op->nfso_stateid.other[0];
	*tl++ = op->nfso_stateid.other[1];
	*tl++ = op->nfso_stateid.other[2];
	*tl++ = txdr_unsigned(op->nfso_own->nfsow_seqid);
	*tl++ = txdr_unsigned(mode & NFSV4OPEN_ACCESSBOTH);
	*tl = txdr_unsigned((mode >> NFSLCK_SHIFT) & NFSV4OPEN_DENYBOTH);
	error = nfscl_request(nd, vp, p, cred, NULL);
	if (error)
		return (error);
	NFSCL_INCRSEQID(op->nfso_own->nfsow_seqid, nd);
	if (!nd->nd_repstat) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
		op->nfso_stateid.seqid = *tl++;
		op->nfso_stateid.other[0] = *tl++;
		op->nfso_stateid.other[1] = *tl++;
		op->nfso_stateid.other[2] = *tl;
	}
	if (nd->nd_repstat && error == 0)
		error = nd->nd_repstat;
	if (error == NFSERR_STALESTATEID)
		nfscl_initiate_recovery(op->nfso_own->nfsow_clp);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * V4 Close operation.
 */
APPLESTATIC int
nfsrpc_close(vnode_t vp, int doclose, NFSPROC_T *p)
{
	struct nfsclclient *clp;
	int error;

	if (vnode_vtype(vp) != VREG)
		return (0);
	if (doclose)
		error = nfscl_doclose(vp, &clp, p);
	else
		error = nfscl_getclose(vp, &clp);
	if (error)
		return (error);

	nfscl_clientrelease(clp);
	return (0);
}

/*
 * Close the open.
 */
APPLESTATIC void
nfsrpc_doclose(struct nfsmount *nmp, struct nfsclopen *op, NFSPROC_T *p)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfscllockowner *lp;
	struct nfscllock *lop, *nlop;
	struct ucred *tcred;
	u_int64_t off = 0, len = 0;
	u_int32_t type = NFSV4LOCKT_READ;
	int error, do_unlock, trycnt;

	tcred = newnfs_getcred();
	newnfs_copycred(&op->nfso_cred, tcred);
	/*
	 * (Theoretically this could be done in the same
	 *  compound as the close, but having multiple
	 *  sequenced Ops in the same compound might be
	 *  too scary for some servers.)
	 */
	if (op->nfso_posixlock) {
		off = 0;
		len = NFS64BITSSET;
		type = NFSV4LOCKT_READ;
	}

	/*
	 * Since this function is only called from VOP_INACTIVE(), no
	 * other thread will be manipulating this Open. As such, the
	 * lock lists are not being changed by other threads, so it should
	 * be safe to do this without locking.
	 */
	LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
		do_unlock = 1;
		LIST_FOREACH_SAFE(lop, &lp->nfsl_lock, nfslo_list, nlop) {
			if (op->nfso_posixlock == 0) {
				off = lop->nfslo_first;
				len = lop->nfslo_end - lop->nfslo_first;
				if (lop->nfslo_type == F_WRLCK)
					type = NFSV4LOCKT_WRITE;
				else
					type = NFSV4LOCKT_READ;
			}
			if (do_unlock) {
				trycnt = 0;
				do {
					error = nfsrpc_locku(nd, nmp, lp, off,
					    len, type, tcred, p, 0);
					if ((nd->nd_repstat == NFSERR_GRACE ||
					    nd->nd_repstat == NFSERR_DELAY) &&
					    error == 0)
						(void) nfs_catnap(PZERO,
						    "nfs_close");
				} while ((nd->nd_repstat == NFSERR_GRACE ||
				    nd->nd_repstat == NFSERR_DELAY) &&
				    error == 0 && trycnt++ < 5);
				if (op->nfso_posixlock)
					do_unlock = 0;
			}
			nfscl_freelock(lop, 0);
		}
	}

	/*
	 * There could be other Opens for different files on the same
	 * OpenOwner, so locking is required.
	 */
	NFSLOCKCLSTATE();
	nfscl_lockexcl(&op->nfso_own->nfsow_rwlock, NFSCLSTATEMUTEXPTR);
	NFSUNLOCKCLSTATE();
	do {
		error = nfscl_tryclose(op, tcred, nmp, p);
		if (error == NFSERR_GRACE)
			(void) nfs_catnap(PZERO, "nfs_close");
	} while (error == NFSERR_GRACE);
	NFSLOCKCLSTATE();
	nfscl_lockunlock(&op->nfso_own->nfsow_rwlock);

	/*
	 * Move the lockowner to nfsc_defunctlockowner,
	 * so the Renew thread will do the ReleaseLockOwner
	 * Op on it later. There might still be other
	 * opens using the same lockowner name.
	 */
	lp = LIST_FIRST(&op->nfso_lock);
	if (lp != NULL) {
		while (LIST_NEXT(lp, nfsl_list) != NULL)
			lp = LIST_NEXT(lp, nfsl_list);
		LIST_PREPEND(&nmp->nm_clp->nfsc_defunctlockowner,
		    &op->nfso_lock, lp, nfsl_list);
		LIST_INIT(&op->nfso_lock);
	}
	nfscl_freeopen(op, 0);
	NFSUNLOCKCLSTATE();
	NFSFREECRED(tcred);
}

/*
 * The actual Close RPC.
 */
APPLESTATIC int
nfsrpc_closerpc(struct nfsrv_descript *nd, struct nfsmount *nmp,
    struct nfsclopen *op, struct ucred *cred, NFSPROC_T *p,
    int syscred)
{
	u_int32_t *tl;
	int error;

	nfscl_reqstart(nd, NFSPROC_CLOSE, nmp, op->nfso_fh,
	    op->nfso_fhlen, NULL);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED + NFSX_STATEID);
	*tl++ = txdr_unsigned(op->nfso_own->nfsow_seqid);
	*tl++ = op->nfso_stateid.seqid;
	*tl++ = op->nfso_stateid.other[0];
	*tl++ = op->nfso_stateid.other[1];
	*tl = op->nfso_stateid.other[2];
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	NFSCL_INCRSEQID(op->nfso_own->nfsow_seqid, nd);
	if (nd->nd_repstat == 0)
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
	error = nd->nd_repstat;
	if (error == NFSERR_STALESTATEID)
		nfscl_initiate_recovery(op->nfso_own->nfsow_clp);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * V4 Open Confirm RPC.
 */
APPLESTATIC int
nfsrpc_openconfirm(vnode_t vp, u_int8_t *nfhp, int fhlen,
    struct nfsclopen *op, struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	nfscl_reqstart(nd, NFSPROC_OPENCONFIRM, VFSTONFS(vnode_mount(vp)),
	    nfhp, fhlen, NULL);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED + NFSX_STATEID);
	*tl++ = op->nfso_stateid.seqid;
	*tl++ = op->nfso_stateid.other[0];
	*tl++ = op->nfso_stateid.other[1];
	*tl++ = op->nfso_stateid.other[2];
	*tl = txdr_unsigned(op->nfso_own->nfsow_seqid);
	error = nfscl_request(nd, vp, p, cred, NULL);
	if (error)
		return (error);
	NFSCL_INCRSEQID(op->nfso_own->nfsow_seqid, nd);
	if (!nd->nd_repstat) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
		op->nfso_stateid.seqid = *tl++;
		op->nfso_stateid.other[0] = *tl++;
		op->nfso_stateid.other[1] = *tl++;
		op->nfso_stateid.other[2] = *tl;
	}
	error = nd->nd_repstat;
	if (error == NFSERR_STALESTATEID)
		nfscl_initiate_recovery(op->nfso_own->nfsow_clp);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the setclientid and setclientid confirm RPCs. Called from nfs_statfs()
 * when a mount has just occurred and when the server replies NFSERR_EXPIRED.
 */
APPLESTATIC int
nfsrpc_setclient(struct nfsmount *nmp, struct nfsclclient *clp,
    struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;
	u_int8_t *cp = NULL, *cp2, addr[INET6_ADDRSTRLEN + 9];
	u_short port;
	int error, isinet6 = 0, callblen;
	nfsquad_t confirm;
	u_int32_t lease;
	static u_int32_t rev = 0;

	if (nfsboottime.tv_sec == 0)
		NFSSETBOOTTIME(nfsboottime);
	nfscl_reqstart(nd, NFSPROC_SETCLIENTID, nmp, NULL, 0, NULL);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(nfsboottime.tv_sec);
	*tl = txdr_unsigned(rev++);
	(void) nfsm_strtom(nd, clp->nfsc_id, clp->nfsc_idlen);

	/*
	 * set up the callback address
	 */
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFS_CALLBCKPROG);
	callblen = strlen(nfsv4_callbackaddr);
	if (callblen == 0)
		cp = nfscl_getmyip(nmp, &isinet6);
	if (nfscl_enablecallb && nfs_numnfscbd > 0 &&
	    (callblen > 0 || cp != NULL)) {
		port = htons(nfsv4_cbport);
		cp2 = (u_int8_t *)&port;
#ifdef INET6
		if ((callblen > 0 &&
		     strchr(nfsv4_callbackaddr, ':')) || isinet6) {
			char ip6buf[INET6_ADDRSTRLEN], *ip6add;

			(void) nfsm_strtom(nd, "tcp6", 4);
			if (callblen == 0) {
				ip6_sprintf(ip6buf, (struct in6_addr *)cp);
				ip6add = ip6buf;
			} else {
				ip6add = nfsv4_callbackaddr;
			}
			snprintf(addr, INET6_ADDRSTRLEN + 9, "%s.%d.%d",
			    ip6add, cp2[0], cp2[1]);
		} else
#endif
		{
			(void) nfsm_strtom(nd, "tcp", 3);
			if (callblen == 0)
				snprintf(addr, INET6_ADDRSTRLEN + 9,
				    "%d.%d.%d.%d.%d.%d", cp[0], cp[1],
				    cp[2], cp[3], cp2[0], cp2[1]);
			else
				snprintf(addr, INET6_ADDRSTRLEN + 9,
				    "%s.%d.%d", nfsv4_callbackaddr,
				    cp2[0], cp2[1]);
		}
		(void) nfsm_strtom(nd, addr, strlen(addr));
	} else {
		(void) nfsm_strtom(nd, "tcp", 3);
		(void) nfsm_strtom(nd, "0.0.0.0.0.0", 11);
	}
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(clp->nfsc_cbident);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
		NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
	    NFSM_DISSECT(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	    clp->nfsc_clientid.lval[0] = *tl++;
	    clp->nfsc_clientid.lval[1] = *tl++;
	    confirm.lval[0] = *tl++;
	    confirm.lval[1] = *tl;
	    mbuf_freem(nd->nd_mrep);
	    nd->nd_mrep = NULL;

	    /*
	     * and confirm it.
	     */
	    nfscl_reqstart(nd, NFSPROC_SETCLIENTIDCFRM, nmp, NULL, 0, NULL);
	    NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	    *tl++ = clp->nfsc_clientid.lval[0];
	    *tl++ = clp->nfsc_clientid.lval[1];
	    *tl++ = confirm.lval[0];
	    *tl = confirm.lval[1];
	    nd->nd_flag |= ND_USEGSSNAME;
	    error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p,
		cred, NFS_PROG, NFS_VER4, NULL, 1, NULL);
	    if (error)
		return (error);
	    mbuf_freem(nd->nd_mrep);
	    nd->nd_mrep = NULL;
	    if (nd->nd_repstat == 0) {
		nfscl_reqstart(nd, NFSPROC_GETATTR, nmp, nmp->nm_fh,
		    nmp->nm_fhsize, NULL);
		NFSZERO_ATTRBIT(&attrbits);
		NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_LEASETIME);
		(void) nfsrv_putattrbit(nd, &attrbits);
		nd->nd_flag |= ND_USEGSSNAME;
		error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p,
		    cred, NFS_PROG, NFS_VER4, NULL, 1, NULL);
		if (error)
		    return (error);
		if (nd->nd_repstat == 0) {
		    error = nfsv4_loadattr(nd, NULL, NULL, NULL, NULL, 0, NULL,
			NULL, NULL, NULL, NULL, 0, NULL, &lease, NULL, p, cred);
		    if (error)
			goto nfsmout;
		    clp->nfsc_renew = NFSCL_RENEW(lease);
		    clp->nfsc_expire = NFSD_MONOSEC + clp->nfsc_renew;
		    clp->nfsc_clientidrev++;
		    if (clp->nfsc_clientidrev == 0)
			clp->nfsc_clientidrev++;
		}
	    }
	}
	error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs getattr call.
 */
APPLESTATIC int
nfsrpc_getattr(vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *nap, void *stuff)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;
	
	NFSCL_REQSTART(nd, NFSPROC_GETATTR, vp);
	if (nd->nd_flag & ND_NFSV4) {
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	if (!nd->nd_repstat)
		error = nfsm_loadattr(nd, nap);
	else
		error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs getattr call with non-vnode arguemnts.
 */
APPLESTATIC int
nfsrpc_getattrnovp(struct nfsmount *nmp, u_int8_t *fhp, int fhlen, int syscred,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, u_int64_t *xidp)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error, vers = NFS_VER2;
	nfsattrbit_t attrbits;
	
	nfscl_reqstart(nd, NFSPROC_GETATTR, nmp, fhp, fhlen, NULL);
	if (nd->nd_flag & ND_NFSV4) {
		vers = NFS_VER4;
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	} else if (nd->nd_flag & ND_NFSV3) {
		vers = NFS_VER3;
	}
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, vers, NULL, 1, xidp);
	if (error)
		return (error);
	if (!nd->nd_repstat)
		error = nfsm_loadattr(nd, nap);
	else
		error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do an nfs setattr operation.
 */
APPLESTATIC int
nfsrpc_setattr(vnode_t vp, struct vattr *vap, NFSACL_T *aclp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *rnap, int *attrflagp,
    void *stuff)
{
	int error, expireret = 0, openerr, retrycnt;
	u_int32_t clidrev = 0, mode;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	struct nfsfh *nfhp;
	nfsv4stateid_t stateid;
	void *lckp;

	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	if (vap != NULL && NFSATTRISSET(u_quad_t, vap, va_size))
		mode = NFSV4OPEN_ACCESSWRITE;
	else
		mode = NFSV4OPEN_ACCESSREAD;
	retrycnt = 0;
	do {
		lckp = NULL;
		openerr = 1;
		if (NFSHASNFSV4(nmp)) {
			nfhp = VTONFS(vp)->n_fhp;
			error = nfscl_getstateid(vp, nfhp->nfh_fh,
			    nfhp->nfh_len, mode, cred, p, &stateid, &lckp);
			if (error && vnode_vtype(vp) == VREG &&
			    (mode == NFSV4OPEN_ACCESSWRITE ||
			     nfstest_openallsetattr)) {
				/*
				 * No Open stateid, so try and open the file
				 * now.
				 */
				if (mode == NFSV4OPEN_ACCESSWRITE)
					openerr = nfsrpc_open(vp, FWRITE, cred,
					    p);
				else
					openerr = nfsrpc_open(vp, FREAD, cred,
					    p);
				if (!openerr)
					(void) nfscl_getstateid(vp,
					    nfhp->nfh_fh, nfhp->nfh_len,
					    mode, cred, p, &stateid, &lckp);
			}
		}
		if (vap != NULL)
			error = nfsrpc_setattrrpc(vp, vap, &stateid, cred, p,
			    rnap, attrflagp, stuff);
		else
			error = nfsrpc_setaclrpc(vp, cred, p, aclp, &stateid,
			    stuff);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (!openerr)
			(void) nfsrpc_close(vp, 0, p);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID) {
			(void) nfs_catnap(PZERO, "nfs_setattr");
		} else if ((error == NFSERR_EXPIRED ||
		    error == NFSERR_BADSTATEID) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

static int
nfsrpc_setattrrpc(vnode_t vp, struct vattr *vap,
    nfsv4stateid_t *stateidp, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *rnap, int *attrflagp, void *stuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_SETATTR, vp);
	if (nd->nd_flag & ND_NFSV4)
		nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
	vap->va_type = vnode_vtype(vp);
	nfscl_fillsattr(nd, vap, vp, NFSSATTR_FULL, 0);
	if (nd->nd_flag & ND_NFSV3) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = newnfs_false;
	} else if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4))
		error = nfscl_wcc_data(nd, vp, rnap, attrflagp, NULL, stuff);
	if ((nd->nd_flag & ND_NFSV4) && !error)
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
	if (!(nd->nd_flag & ND_NFSV3) && !nd->nd_repstat && !error)
		error = nfscl_postop_attr(nd, rnap, attrflagp, stuff);
	mbuf_freem(nd->nd_mrep);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	return (error);
}

/*
 * nfs lookup rpc
 */
APPLESTATIC int
nfsrpc_lookup(vnode_t dvp, char *name, int len, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *dnap, struct nfsvattr *nap,
    struct nfsfh **nfhpp, int *attrflagp, int *dattrflagp, void *stuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	struct nfsnode *np;
	struct nfsfh *nfhp;
	nfsattrbit_t attrbits;
	int error = 0, lookupp = 0;

	*attrflagp = 0;
	*dattrflagp = 0;
	if (vnode_vtype(dvp) != VDIR)
		return (ENOTDIR);
	nmp = VFSTONFS(vnode_mount(dvp));
	if (len > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	if (NFSHASNFSV4(nmp) && len == 1 &&
		name[0] == '.') {
		/*
		 * Just return the current dir's fh.
		 */
		np = VTONFS(dvp);
		MALLOC(nfhp, struct nfsfh *, sizeof (struct nfsfh) +
			np->n_fhp->nfh_len, M_NFSFH, M_WAITOK);
		nfhp->nfh_len = np->n_fhp->nfh_len;
		NFSBCOPY(np->n_fhp->nfh_fh, nfhp->nfh_fh, nfhp->nfh_len);
		*nfhpp = nfhp;
		return (0);
	}
	if (NFSHASNFSV4(nmp) && len == 2 &&
		name[0] == '.' && name[1] == '.') {
		lookupp = 1;
		NFSCL_REQSTART(nd, NFSPROC_LOOKUPP, dvp);
	} else {
		NFSCL_REQSTART(nd, NFSPROC_LOOKUP, dvp);
		(void) nfsm_strtom(nd, name, len);
	}
	if (nd->nd_flag & ND_NFSV4) {
		NFSGETATTR_ATTRBIT(&attrbits);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(NFSV4OP_GETFH);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, dvp, p, cred, stuff);
	if (error)
		return (error);
	if (nd->nd_repstat) {
		/*
		 * When an NFSv4 Lookupp returns ENOENT, it means that
		 * the lookup is at the root of an fs, so return this dir.
		 */
		if (nd->nd_repstat == NFSERR_NOENT && lookupp) {
		    np = VTONFS(dvp);
		    MALLOC(nfhp, struct nfsfh *, sizeof (struct nfsfh) +
			np->n_fhp->nfh_len, M_NFSFH, M_WAITOK);
		    nfhp->nfh_len = np->n_fhp->nfh_len;
		    NFSBCOPY(np->n_fhp->nfh_fh, nfhp->nfh_fh, nfhp->nfh_len);
		    *nfhpp = nfhp;
		    mbuf_freem(nd->nd_mrep);
		    return (0);
		}
		if (nd->nd_flag & ND_NFSV3)
		    error = nfscl_postop_attr(nd, dnap, dattrflagp, stuff);
		goto nfsmout;
	}
	if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) == ND_NFSV4) {
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		if (*(tl + 1)) {
			nd->nd_flag |= ND_NOMOREDATA;
			goto nfsmout;
		}
	}
	error = nfsm_getfh(nd, nfhpp);
	if (error)
		goto nfsmout;

	error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
	if ((nd->nd_flag & ND_NFSV3) && !error)
		error = nfscl_postop_attr(nd, dnap, dattrflagp, stuff);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	if (!error && nd->nd_repstat)
		error = nd->nd_repstat;
	return (error);
}

/*
 * Do a readlink rpc.
 */
APPLESTATIC int
nfsrpc_readlink(vnode_t vp, struct uio *uiop, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp, void *stuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsnode *np = VTONFS(vp);
	nfsattrbit_t attrbits;
	int error, len, cangetattr = 1;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_READLINK, vp);
	if (nd->nd_flag & ND_NFSV4) {
		/*
		 * And do a Getattr op.
		 */
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV3)
		error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
	if (!nd->nd_repstat && !error) {
		NFSM_STRSIZ(len, NFS_MAXPATHLEN);
		/*
		 * This seems weird to me, but must have been added to
		 * FreeBSD for some reason. The only thing I can think of
		 * is that there was/is some server that replies with
		 * more link data than it should?
		 */
		if (len == NFS_MAXPATHLEN) {
			NFSLOCKNODE(np);
			if (np->n_size > 0 && np->n_size < NFS_MAXPATHLEN) {
				len = np->n_size;
				cangetattr = 0;
			}
			NFSUNLOCKNODE(np);
		}
		error = nfsm_mbufuio(nd, uiop, len);
		if ((nd->nd_flag & ND_NFSV4) && !error && cangetattr)
			error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * Read operation.
 */
APPLESTATIC int
nfsrpc_read(vnode_t vp, struct uio *uiop, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp, void *stuff)
{
	int error, expireret = 0, retrycnt;
	u_int32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	struct nfsnode *np = VTONFS(vp);
	struct ucred *newcred;
	struct nfsfh *nfhp = NULL;
	nfsv4stateid_t stateid;
	void *lckp;

	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	newcred = cred;
	if (NFSHASNFSV4(nmp)) {
		nfhp = np->n_fhp;
		if (p == NULL)
			newcred = NFSNEWCRED(cred);
	}
	retrycnt = 0;
	do {
		lckp = NULL;
		if (NFSHASNFSV4(nmp))
			(void)nfscl_getstateid(vp, nfhp->nfh_fh, nfhp->nfh_len,
			    NFSV4OPEN_ACCESSREAD, newcred, p, &stateid, &lckp);
		error = nfsrpc_readrpc(vp, uiop, newcred, &stateid, p, nap,
		    attrflagp, stuff);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID) {
			(void) nfs_catnap(PZERO, "nfs_read");
		} else if ((error == NFSERR_EXPIRED ||
		    error == NFSERR_BADSTATEID) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	if (NFSHASNFSV4(nmp) && p == NULL)
		NFSFREECRED(newcred);
	return (error);
}

/*
 * The actual read RPC.
 */
static int
nfsrpc_readrpc(vnode_t vp, struct uio *uiop, struct ucred *cred,
    nfsv4stateid_t *stateidp, NFSPROC_T *p, struct nfsvattr *nap,
    int *attrflagp, void *stuff)
{
	u_int32_t *tl;
	int error = 0, len, retlen, tsiz, eof = 0;
	struct nfsrv_descript nfsd;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	struct nfsrv_descript *nd = &nfsd;

	*attrflagp = 0;
	tsiz = uio_uio_resid(uiop);
	if (uiop->uio_offset + tsiz > 0xffffffff &&
	    !NFSHASNFSV3OR4(nmp))
		return (EFBIG);
	nd->nd_mrep = NULL;
	while (tsiz > 0) {
		*attrflagp = 0;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		NFSCL_REQSTART(nd, NFSPROC_READ, vp);
		if (nd->nd_flag & ND_NFSV4)
			nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED * 3);
		if (nd->nd_flag & ND_NFSV2) {
			*tl++ = txdr_unsigned(uiop->uio_offset);
			*tl++ = txdr_unsigned(len);
			*tl = 0;
		} else {
			txdr_hyper(uiop->uio_offset, tl);
			*(tl + 2) = txdr_unsigned(len);
		}
		/*
		 * Since I can't do a Getattr for NFSv4 for Write, there
		 * doesn't seem any point in doing one here, either.
		 * (See the comment in nfsrpc_writerpc() for more info.)
		 */
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		if (nd->nd_flag & ND_NFSV3) {
			error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
		} else if (!nd->nd_repstat && (nd->nd_flag & ND_NFSV2)) {
			error = nfsm_loadattr(nd, nap);
			if (!error)
				*attrflagp = 1;
		}
		if (nd->nd_repstat || error) {
			if (!error)
				error = nd->nd_repstat;
			goto nfsmout;
		}
		if (nd->nd_flag & ND_NFSV3) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *(tl + 1));
		} else if (nd->nd_flag & ND_NFSV4) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *tl);
		}
		NFSM_STRSIZ(retlen, nmp->nm_rsize);
		error = nfsm_mbufuio(nd, uiop, retlen);
		if (error)
			goto nfsmout;
		mbuf_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
		tsiz -= retlen;
		if (!(nd->nd_flag & ND_NFSV2)) {
			if (eof || retlen == 0)
				tsiz = 0;
		} else if (retlen < len)
			tsiz = 0;
	}
	return (0);
nfsmout:
	if (nd->nd_mrep != NULL)
		mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs write operation
 */
APPLESTATIC int
nfsrpc_write(vnode_t vp, struct uio *uiop, int *iomode, u_char *verfp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    void *stuff)
{
	int error, expireret = 0, retrycnt, nostateid;
	u_int32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	struct nfsnode *np = VTONFS(vp);
	struct ucred *newcred;
	struct nfsfh *nfhp = NULL;
	nfsv4stateid_t stateid;
	void *lckp;

	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	newcred = cred;
	if (NFSHASNFSV4(nmp)) {
		if (p == NULL)
			newcred = NFSNEWCRED(cred);
		nfhp = np->n_fhp;
	}
	retrycnt = 0;
	do {
		lckp = NULL;
		nostateid = 0;
		if (NFSHASNFSV4(nmp)) {
			(void)nfscl_getstateid(vp, nfhp->nfh_fh, nfhp->nfh_len,
			    NFSV4OPEN_ACCESSWRITE, newcred, p, &stateid, &lckp);
			if (stateid.other[0] == 0 && stateid.other[1] == 0 &&
			    stateid.other[2] == 0) {
				nostateid = 1;
				printf("stateid0 in write\n");
			}
		}

		/*
		 * If there is no stateid for NFSv4, it means this is an
		 * extraneous write after close. Basically a poorly
		 * implemented buffer cache. Just don't do the write.
		 */
		if (nostateid)
			error = 0;
		else
			error = nfsrpc_writerpc(vp, uiop, iomode, verfp,
			    newcred, &stateid, p, nap, attrflagp, stuff);
if (error == NFSERR_BADSTATEID) {
printf("st=0x%x 0x%x 0x%x\n",stateid.other[0],stateid.other[1],stateid.other[2]);
nfscl_dumpstate(nmp, 1, 1, 0, 0);
}
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID) {
			(void) nfs_catnap(PZERO, "nfs_write");
		} else if ((error == NFSERR_EXPIRED ||
		    error == NFSERR_BADSTATEID) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	if (NFSHASNFSV4(nmp) && p == NULL)
		NFSFREECRED(newcred);
	return (error);
}

/*
 * The actual write RPC.
 */
static int
nfsrpc_writerpc(vnode_t vp, struct uio *uiop, int *iomode,
    u_char *verfp, struct ucred *cred, nfsv4stateid_t *stateidp,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp, void *stuff)
{
	u_int32_t *tl;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	struct nfsnode *np = VTONFS(vp);
	int error = 0, len, tsiz, rlen, commit, committed = NFSWRITE_FILESYNC;
	int wccflag = 0, wsize;
	int32_t backup;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfs: writerpc iovcnt > 1");
#endif
	*attrflagp = 0;
	tsiz = uio_uio_resid(uiop);
	NFSLOCKMNT(nmp);
	if (uiop->uio_offset + tsiz > 0xffffffff &&
	    !NFSHASNFSV3OR4(nmp)) {
		NFSUNLOCKMNT(nmp);
		return (EFBIG);
	}
	wsize = nmp->nm_wsize;
	NFSUNLOCKMNT(nmp);
	nd->nd_mrep = NULL;	/* NFSv2 sometimes does a write with */
	nd->nd_repstat = 0;	/* uio_resid == 0, so the while is not done */
	while (tsiz > 0) {
		nmp = VFSTONFS(vnode_mount(vp));
		if (nmp == NULL) {
			error = ENXIO;
			goto nfsmout;
		}
		*attrflagp = 0;
		len = (tsiz > wsize) ? wsize : tsiz;
		NFSCL_REQSTART(nd, NFSPROC_WRITE, vp);
		if (nd->nd_flag & ND_NFSV4) {
			nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
			NFSM_BUILD(tl, u_int32_t *, NFSX_HYPER+2*NFSX_UNSIGNED);
			txdr_hyper(uiop->uio_offset, tl);
			tl += 2;
			*tl++ = txdr_unsigned(*iomode);
			*tl = txdr_unsigned(len);
		} else if (nd->nd_flag & ND_NFSV3) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_HYPER+3*NFSX_UNSIGNED);
			txdr_hyper(uiop->uio_offset, tl);
			tl += 2;
			*tl++ = txdr_unsigned(len);
			*tl++ = txdr_unsigned(*iomode);
			*tl = txdr_unsigned(len);
		} else {
			u_int32_t x;

			NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			/*
			 * Not sure why someone changed this, since the
			 * RFC clearly states that "beginoffset" and
			 * "totalcount" are ignored, but it wouldn't
			 * surprise me if there's a busted server out there.
			 */
			/* Set both "begin" and "current" to non-garbage. */
			x = txdr_unsigned((u_int32_t)uiop->uio_offset);
			*tl++ = x;      /* "begin offset" */
			*tl++ = x;      /* "current offset" */
			x = txdr_unsigned(len);
			*tl++ = x;      /* total to this offset */
			*tl = x;        /* size of this write */

		}
		nfsm_uiombuf(nd, uiop, len);
		/*
		 * Although it is tempting to do a normal Getattr Op in the
		 * NFSv4 compound, the result can be a nearly hung client
		 * system if the Getattr asks for Owner and/or OwnerGroup.
		 * It occurs when the client can't map either the Owner or
		 * Owner_group name in the Getattr reply to a uid/gid. When
		 * there is a cache miss, the kernel does an upcall to the
		 * nfsuserd. Then, it can try and read the local /etc/passwd
		 * or /etc/group file. It can then block in getnewbuf(),
		 * waiting for dirty writes to be pushed to the NFS server.
		 * The only reason this doesn't result in a complete
		 * deadlock, is that the upcall times out and allows
		 * the write to complete. However, progress is so slow
		 * that it might just as well be deadlocked.
		 * So, we just get the attributes that change with each
		 * write Op.
		 * nb: nfscl_loadattrcache() needs to be told that these
		 *     partial attributes from a write rpc are being
		 *     passed in, via a argument flag.
		 */
		if (nd->nd_flag & ND_NFSV4) {
			NFSWRITEGETATTR_ATTRBIT(&attrbits);
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_GETATTR);
			(void) nfsrv_putattrbit(nd, &attrbits);
		}
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		if (nd->nd_repstat) {
			/*
			 * In case the rpc gets retried, roll
			 * the uio fileds changed by nfsm_uiombuf()
			 * back.
			 */
			uiop->uio_offset -= len;
			uio_uio_resid_add(uiop, len);
			uio_iov_base_add(uiop, -len);
			uio_iov_len_add(uiop, len);
		}
		if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
			error = nfscl_wcc_data(nd, vp, nap, attrflagp,
			    &wccflag, stuff);
			if (error)
				goto nfsmout;
		}
		if (!nd->nd_repstat) {
			if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
				NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED
					+ NFSX_VERF);
				rlen = fxdr_unsigned(int, *tl++);
				if (rlen == 0) {
					error = NFSERR_IO;
					goto nfsmout;
				} else if (rlen < len) {
					backup = len - rlen;
					uio_iov_base_add(uiop, -(backup));
					uio_iov_len_add(uiop, backup);
					uiop->uio_offset -= backup;
					uio_uio_resid_add(uiop, backup);
					len = rlen;
				}
				commit = fxdr_unsigned(int, *tl++);

				/*
				 * Return the lowest committment level
				 * obtained by any of the RPCs.
				 */
				if (committed == NFSWRITE_FILESYNC)
					committed = commit;
				else if (committed == NFSWRITE_DATASYNC &&
					commit == NFSWRITE_UNSTABLE)
					committed = commit;
				if (verfp != NULL)
					NFSBCOPY((caddr_t)tl, verfp, NFSX_VERF);
				NFSLOCKMNT(nmp);
				if (!NFSHASWRITEVERF(nmp)) {
					NFSBCOPY((caddr_t)tl,
					    (caddr_t)&nmp->nm_verf[0],
					    NFSX_VERF);
					NFSSETWRITEVERF(nmp);
				}
				NFSUNLOCKMNT(nmp);
			}
			if (nd->nd_flag & ND_NFSV4)
				NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (nd->nd_flag & (ND_NFSV2 | ND_NFSV4)) {
				error = nfsm_loadattr(nd, nap);
				if (!error)
					*attrflagp = NFS_LATTR_NOSHRINK;
			}
		} else {
			error = nd->nd_repstat;
		}
		if (error)
			goto nfsmout;
		NFSWRITERPC_SETTIME(wccflag, np, (nd->nd_flag & ND_NFSV4));
		mbuf_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
		tsiz -= len;
	}
nfsmout:
	if (nd->nd_mrep != NULL)
		mbuf_freem(nd->nd_mrep);
	*iomode = committed;
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	return (error);
}

/*
 * nfs mknod rpc
 * For NFS v2 this is a kludge. Use a create rpc but with the IFMT bits of the
 * mode set to specify the file type and the size field for rdev.
 */
APPLESTATIC int
nfsrpc_mknod(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    u_int32_t rdev, enum vtype vtyp, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *dnap, struct nfsvattr *nnap, struct nfsfh **nfhpp,
    int *attrflagp, int *dattrflagp, void *dstuff)
{
	u_int32_t *tl;
	int error = 0;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;

	*nfhpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_MKNOD, dvp);
	if (nd->nd_flag & ND_NFSV4) {
		if (vtyp == VBLK || vtyp == VCHR) {
			NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			*tl++ = vtonfsv34_type(vtyp);
			*tl++ = txdr_unsigned(NFSMAJOR(rdev));
			*tl = txdr_unsigned(NFSMINOR(rdev));
		} else {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = vtonfsv34_type(vtyp);
		}
	}
	(void) nfsm_strtom(nd, name, namelen);
	if (nd->nd_flag & ND_NFSV3) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = vtonfsv34_type(vtyp);
	}
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4))
		nfscl_fillsattr(nd, vap, dvp, 0, 0);
	if ((nd->nd_flag & ND_NFSV3) &&
	    (vtyp == VCHR || vtyp == VBLK)) {
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(NFSMAJOR(rdev));
		*tl = txdr_unsigned(NFSMINOR(rdev));
	}
	if (nd->nd_flag & ND_NFSV4) {
		NFSGETATTR_ATTRBIT(&attrbits);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(NFSV4OP_GETFH);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	if (nd->nd_flag & ND_NFSV2)
		nfscl_fillsattr(nd, vap, dvp, NFSSATTR_SIZERDEV, rdev);
	error = nfscl_request(nd, dvp, p, cred, dstuff);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV4)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if (!nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV4) {
			NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
			if (error)
				goto nfsmout;
		}
		error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (error)
			goto nfsmout;
	}
	if (nd->nd_flag & ND_NFSV3)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if (!error && nd->nd_repstat)
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs file create call
 * Mostly just call the approriate routine. (I separated out v4, so that
 * error recovery wouldn't be as difficult.)
 */
APPLESTATIC int
nfsrpc_create(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    nfsquad_t cverf, int fmode, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *dnap, struct nfsvattr *nnap, struct nfsfh **nfhpp,
    int *attrflagp, int *dattrflagp, void *dstuff)
{
	int error = 0, newone, expireret = 0, retrycnt, unlocked;
	struct nfsclowner *owp;
	struct nfscldeleg *dp;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(dvp));
	u_int32_t clidrev;

	if (NFSHASNFSV4(nmp)) {
	    retrycnt = 0;
	    do {
		dp = NULL;
		error = nfscl_open(dvp, NULL, 0, (NFSV4OPEN_ACCESSWRITE |
		    NFSV4OPEN_ACCESSREAD), 0, cred, p, &owp, NULL, &newone,
		    NULL, 1);
		if (error)
			return (error);
		if (nmp->nm_clp != NULL)
			clidrev = nmp->nm_clp->nfsc_clientidrev;
		else
			clidrev = 0;
		error = nfsrpc_createv4(dvp, name, namelen, vap, cverf, fmode,
		  owp, &dp, cred, p, dnap, nnap, nfhpp, attrflagp, dattrflagp,
		  dstuff, &unlocked);
		if (dp != NULL)
			(void) nfscl_deleg(nmp->nm_mountp, owp->nfsow_clp,
			    (*nfhpp)->nfh_fh, (*nfhpp)->nfh_len, cred, p, &dp);
		nfscl_ownerrelease(owp, error, newone, unlocked);
		if (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY) {
			(void) nfs_catnap(PZERO, "nfs_open");
		} else if ((error == NFSERR_EXPIRED ||
		    error == NFSERR_BADSTATEID) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
			retrycnt++;
		}
	    } while (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
		error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
		 expireret == 0 && clidrev != 0 && retrycnt < 4));
	    if (error && retrycnt >= 4)
		    error = EIO;
	} else {
		error = nfsrpc_createv23(dvp, name, namelen, vap, cverf,
		    fmode, cred, p, dnap, nnap, nfhpp, attrflagp, dattrflagp,
		    dstuff);
	}
	return (error);
}

/*
 * The create rpc for v2 and 3.
 */
static int
nfsrpc_createv23(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    nfsquad_t cverf, int fmode, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *dnap, struct nfsvattr *nnap, struct nfsfh **nfhpp,
    int *attrflagp, int *dattrflagp, void *dstuff)
{
	u_int32_t *tl;
	int error = 0;
	struct nfsrv_descript nfsd, *nd = &nfsd;

	*nfhpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_CREATE, dvp);
	(void) nfsm_strtom(nd, name, namelen);
	if (nd->nd_flag & ND_NFSV3) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		if (fmode & O_EXCL) {
			*tl = txdr_unsigned(NFSCREATE_EXCLUSIVE);
			NFSM_BUILD(tl, u_int32_t *, NFSX_VERF);
			*tl++ = cverf.lval[0];
			*tl = cverf.lval[1];
		} else {
			*tl = txdr_unsigned(NFSCREATE_UNCHECKED);
			nfscl_fillsattr(nd, vap, dvp, 0, 0);
		}
	} else {
		nfscl_fillsattr(nd, vap, dvp, NFSSATTR_SIZE0, 0);
	}
	error = nfscl_request(nd, dvp, p, cred, dstuff);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
		error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (error)
			goto nfsmout;
	}
	if (nd->nd_flag & ND_NFSV3)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

static int
nfsrpc_createv4(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    nfsquad_t cverf, int fmode, struct nfsclowner *owp, struct nfscldeleg **dpp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp, void *dstuff, int *unlockedp)
{
	u_int32_t *tl;
	int error = 0, deleg, newone, ret, acesize, limitby;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsclopen *op;
	struct nfscldeleg *dp = NULL;
	struct nfsnode *np;
	struct nfsfh *nfhp;
	nfsattrbit_t attrbits;
	nfsv4stateid_t stateid;
	u_int32_t rflags;

	*unlockedp = 0;
	*nfhpp = NULL;
	*dpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_CREATE, dvp);
	/*
	 * For V4, this is actually an Open op.
	 */
	NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(owp->nfsow_seqid);
	*tl++ = txdr_unsigned(NFSV4OPEN_ACCESSWRITE |
	    NFSV4OPEN_ACCESSREAD);
	*tl++ = txdr_unsigned(NFSV4OPEN_DENYNONE);
	*tl++ = owp->nfsow_clp->nfsc_clientid.lval[0];
	*tl = owp->nfsow_clp->nfsc_clientid.lval[1];
	(void) nfsm_strtom(nd, owp->nfsow_owner, NFSV4CL_LOCKNAMELEN);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OPEN_CREATE);
	if (fmode & O_EXCL) {
		*tl = txdr_unsigned(NFSCREATE_EXCLUSIVE);
		NFSM_BUILD(tl, u_int32_t *, NFSX_VERF);
		*tl++ = cverf.lval[0];
		*tl = cverf.lval[1];
	} else {
		*tl = txdr_unsigned(NFSCREATE_UNCHECKED);
		nfscl_fillsattr(nd, vap, dvp, 0, 0);
	}
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OPEN_CLAIMNULL);
	(void) nfsm_strtom(nd, name, namelen);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OP_GETFH);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	(void) nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, dvp, p, cred, dstuff);
	if (error)
		return (error);
	error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if (error)
		goto nfsmout;
	NFSCL_INCRSEQID(owp->nfsow_seqid, nd);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
		    6 * NFSX_UNSIGNED);
		stateid.seqid = *tl++;
		stateid.other[0] = *tl++;
		stateid.other[1] = *tl++;
		stateid.other[2] = *tl;
		rflags = fxdr_unsigned(u_int32_t, *(tl + 6));
		(void) nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		deleg = fxdr_unsigned(int, *tl);
		if (deleg == NFSV4OPEN_DELEGATEREAD ||
		    deleg == NFSV4OPEN_DELEGATEWRITE) {
			if (!(owp->nfsow_clp->nfsc_flags &
			      NFSCLFLAGS_FIRSTDELEG))
				owp->nfsow_clp->nfsc_flags |=
				  (NFSCLFLAGS_FIRSTDELEG | NFSCLFLAGS_GOTDELEG);
			MALLOC(dp, struct nfscldeleg *,
			    sizeof (struct nfscldeleg) + NFSX_V4FHMAX,
			    M_NFSCLDELEG, M_WAITOK);
			LIST_INIT(&dp->nfsdl_owner);
			LIST_INIT(&dp->nfsdl_lock);
			dp->nfsdl_clp = owp->nfsow_clp;
			newnfs_copyincred(cred, &dp->nfsdl_cred);
			nfscl_lockinit(&dp->nfsdl_rwlock);
			NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
			    NFSX_UNSIGNED);
			dp->nfsdl_stateid.seqid = *tl++;
			dp->nfsdl_stateid.other[0] = *tl++;
			dp->nfsdl_stateid.other[1] = *tl++;
			dp->nfsdl_stateid.other[2] = *tl++;
			ret = fxdr_unsigned(int, *tl);
			if (deleg == NFSV4OPEN_DELEGATEWRITE) {
				dp->nfsdl_flags = NFSCLDL_WRITE;
				/*
				 * Indicates how much the file can grow.
				 */
				NFSM_DISSECT(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
				limitby = fxdr_unsigned(int, *tl++);
				switch (limitby) {
				case NFSV4OPEN_LIMITSIZE:
					dp->nfsdl_sizelimit = fxdr_hyper(tl);
					break;
				case NFSV4OPEN_LIMITBLOCKS:
					dp->nfsdl_sizelimit =
					    fxdr_unsigned(u_int64_t, *tl++);
					dp->nfsdl_sizelimit *=
					    fxdr_unsigned(u_int64_t, *tl);
					break;
				default:
					error = NFSERR_BADXDR;
					goto nfsmout;
				};
			} else {
				dp->nfsdl_flags = NFSCLDL_READ;
			}
			if (ret)
				dp->nfsdl_flags |= NFSCLDL_RECALL;
			error = nfsrv_dissectace(nd, &dp->nfsdl_ace, &ret,
			    &acesize, p);
			if (error)
				goto nfsmout;
		} else if (deleg != NFSV4OPEN_DELEGATENONE) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (error)
			goto nfsmout;
		if (dp != NULL && *attrflagp) {
			dp->nfsdl_change = nnap->na_filerev;
			dp->nfsdl_modtime = nnap->na_mtime;
			dp->nfsdl_flags |= NFSCLDL_MODTIMESET;
		}
		/*
		 * We can now complete the Open state.
		 */
		nfhp = *nfhpp;
		if (dp != NULL) {
			dp->nfsdl_fhlen = nfhp->nfh_len;
			NFSBCOPY(nfhp->nfh_fh, dp->nfsdl_fh, nfhp->nfh_len);
		}
		/*
		 * Get an Open structure that will be
		 * attached to the OpenOwner, acquired already.
		 */
		error = nfscl_open(dvp, nfhp->nfh_fh, nfhp->nfh_len, 
		    (NFSV4OPEN_ACCESSWRITE | NFSV4OPEN_ACCESSREAD), 0,
		    cred, p, NULL, &op, &newone, NULL, 0);
		if (error)
			goto nfsmout;
		op->nfso_stateid = stateid;
		newnfs_copyincred(cred, &op->nfso_cred);
		if ((rflags & NFSV4OPEN_RESULTCONFIRM)) {
		    do {
			ret = nfsrpc_openconfirm(dvp, nfhp->nfh_fh,
			    nfhp->nfh_len, op, cred, p);
			if (ret == NFSERR_DELAY)
			    (void) nfs_catnap(PZERO, "nfs_create");
		    } while (ret == NFSERR_DELAY);
		    error = ret;
		}

		/*
		 * If the server is handing out delegations, but we didn't
		 * get one because an OpenConfirm was required, try the
		 * Open again, to get a delegation. This is a harmless no-op,
		 * from a server's point of view.
		 */
		if ((rflags & NFSV4OPEN_RESULTCONFIRM) &&
		    (owp->nfsow_clp->nfsc_flags & NFSCLFLAGS_GOTDELEG) &&
		    !error && dp == NULL) {
		    np = VTONFS(dvp);
		    do {
			ret = nfsrpc_openrpc(VFSTONFS(vnode_mount(dvp)), dvp,
			    np->n_fhp->nfh_fh, np->n_fhp->nfh_len,
			    nfhp->nfh_fh, nfhp->nfh_len,
			    (NFSV4OPEN_ACCESSWRITE | NFSV4OPEN_ACCESSREAD), op,
			    name, namelen, &dp, 0, 0x0, cred, p, 0, 1);
			if (ret == NFSERR_DELAY)
			    (void) nfs_catnap(PZERO, "nfs_crt2");
		    } while (ret == NFSERR_DELAY);
		    if (ret) {
			if (dp != NULL)
				FREE((caddr_t)dp, M_NFSCLDELEG);
			if (ret == NFSERR_STALECLIENTID ||
			    ret == NFSERR_STALEDONTRECOVER)
				error = ret;
		    }
		}
		nfscl_openrelease(op, error, newone);
		*unlockedp = 1;
	}
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
	if (error == NFSERR_STALECLIENTID)
		nfscl_initiate_recovery(owp->nfsow_clp);
nfsmout:
	if (!error)
		*dpp = dp;
	else if (dp != NULL)
		FREE((caddr_t)dp, M_NFSCLDELEG);
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * Nfs remove rpc
 */
APPLESTATIC int
nfsrpc_remove(vnode_t dvp, char *name, int namelen, vnode_t vp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap, int *dattrflagp,
    void *dstuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsnode *np;
	struct nfsmount *nmp;
	nfsv4stateid_t dstateid;
	int error, ret = 0, i;

	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	nmp = VFSTONFS(vnode_mount(dvp));
tryagain:
	if (NFSHASNFSV4(nmp) && ret == 0) {
		ret = nfscl_removedeleg(vp, p, &dstateid);
		if (ret == 1) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGREMOVE, vp);
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID +
			    NFSX_UNSIGNED);
			*tl++ = dstateid.seqid;
			*tl++ = dstateid.other[0];
			*tl++ = dstateid.other[1];
			*tl++ = dstateid.other[2];
			*tl = txdr_unsigned(NFSV4OP_PUTFH);
			np = VTONFS(dvp);
			(void) nfsm_fhtom(nd, np->n_fhp->nfh_fh,
			    np->n_fhp->nfh_len, 0);
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_REMOVE);
		}
	} else {
		ret = 0;
	}
	if (ret == 0)
		NFSCL_REQSTART(nd, NFSPROC_REMOVE, dvp);
	(void) nfsm_strtom(nd, name, namelen);
	error = nfscl_request(nd, dvp, p, cred, dstuff);
	if (error)
		return (error);
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
		/* For NFSv4, parse out any Delereturn replies. */
		if (ret > 0 && nd->nd_repstat != 0 &&
		    (nd->nd_flag & ND_NOMOREDATA)) {
			/*
			 * If the Delegreturn failed, try again without
			 * it. The server will Recall, as required.
			 */
			mbuf_freem(nd->nd_mrep);
			goto tryagain;
		}
		for (i = 0; i < (ret * 2); i++) {
			if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) ==
			    ND_NFSV4) {
			    NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			    if (*(tl + 1))
				nd->nd_flag |= ND_NOMOREDATA;
			}
		}
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do an nfs rename rpc.
 */
APPLESTATIC int
nfsrpc_rename(vnode_t fdvp, vnode_t fvp, char *fnameptr, int fnamelen,
    vnode_t tdvp, vnode_t tvp, char *tnameptr, int tnamelen, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *fnap, struct nfsvattr *tnap,
    int *fattrflagp, int *tattrflagp, void *fstuff, void *tstuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	struct nfsnode *np;
	nfsattrbit_t attrbits;
	nfsv4stateid_t fdstateid, tdstateid;
	int error = 0, ret = 0, gottd = 0, gotfd = 0, i;
	
	*fattrflagp = 0;
	*tattrflagp = 0;
	nmp = VFSTONFS(vnode_mount(fdvp));
	if (fnamelen > NFS_MAXNAMLEN || tnamelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
tryagain:
	if (NFSHASNFSV4(nmp) && ret == 0) {
		ret = nfscl_renamedeleg(fvp, &fdstateid, &gotfd, tvp,
		    &tdstateid, &gottd, p);
		if (gotfd && gottd) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGRENAME2, fvp);
		} else if (gotfd) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGRENAME1, fvp);
		} else if (gottd) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGRENAME1, tvp);
		}
		if (gotfd) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
			*tl++ = fdstateid.seqid;
			*tl++ = fdstateid.other[0];
			*tl++ = fdstateid.other[1];
			*tl = fdstateid.other[2];
			if (gottd) {
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(NFSV4OP_PUTFH);
				np = VTONFS(tvp);
				(void) nfsm_fhtom(nd, np->n_fhp->nfh_fh,
				    np->n_fhp->nfh_len, 0);
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(NFSV4OP_DELEGRETURN);
			}
		}
		if (gottd) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
			*tl++ = tdstateid.seqid;
			*tl++ = tdstateid.other[0];
			*tl++ = tdstateid.other[1];
			*tl = tdstateid.other[2];
		}
		if (ret > 0) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_PUTFH);
			np = VTONFS(fdvp);
			(void) nfsm_fhtom(nd, np->n_fhp->nfh_fh,
			    np->n_fhp->nfh_len, 0);
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_SAVEFH);
		}
	} else {
		ret = 0;
	}
	if (ret == 0)
		NFSCL_REQSTART(nd, NFSPROC_RENAME, fdvp);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSWCCATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_PUTFH);
		(void) nfsm_fhtom(nd, VTONFS(tdvp)->n_fhp->nfh_fh,
		    VTONFS(tdvp)->n_fhp->nfh_len, 0);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
		nd->nd_flag |= ND_V4WCCATTR;
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_RENAME);
	}
	(void) nfsm_strtom(nd, fnameptr, fnamelen);
	if (!(nd->nd_flag & ND_NFSV4))
		(void) nfsm_fhtom(nd, VTONFS(tdvp)->n_fhp->nfh_fh,
			VTONFS(tdvp)->n_fhp->nfh_len, 0);
	(void) nfsm_strtom(nd, tnameptr, tnamelen);
	error = nfscl_request(nd, fdvp, p, cred, fstuff);
	if (error)
		return (error);
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
		/* For NFSv4, parse out any Delereturn replies. */
		if (ret > 0 && nd->nd_repstat != 0 &&
		    (nd->nd_flag & ND_NOMOREDATA)) {
			/*
			 * If the Delegreturn failed, try again without
			 * it. The server will Recall, as required.
			 */
			mbuf_freem(nd->nd_mrep);
			goto tryagain;
		}
		for (i = 0; i < (ret * 2); i++) {
			if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) ==
			    ND_NFSV4) {
			    NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			    if (*(tl + 1)) {
				if (i == 0 && ret > 1) {
				    /*
				     * If the Delegreturn failed, try again
				     * without it. The server will Recall, as
				     * required.
				     * If ret > 1, the first iteration of this
				     * loop is the second DelegReturn result.
				     */
				    mbuf_freem(nd->nd_mrep);
				    goto tryagain;
				} else {
				    nd->nd_flag |= ND_NOMOREDATA;
				}
			    }
			}
		}
		/* Now, the first wcc attribute reply. */
		if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) == ND_NFSV4) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (*(tl + 1))
				nd->nd_flag |= ND_NOMOREDATA;
		}
		error = nfscl_wcc_data(nd, fdvp, fnap, fattrflagp, NULL,
		    fstuff);
		/* and the second wcc attribute reply. */
		if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) == ND_NFSV4 &&
		    !error) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (*(tl + 1))
				nd->nd_flag |= ND_NOMOREDATA;
		}
		if (!error)
			error = nfscl_wcc_data(nd, tdvp, tnap, tattrflagp,
			    NULL, tstuff);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs hard link create rpc
 */
APPLESTATIC int
nfsrpc_link(vnode_t dvp, vnode_t vp, char *name, int namelen,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nap, int *attrflagp, int *dattrflagp, void *dstuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;
	int error = 0;

	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_LINK, vp);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_PUTFH);
	}
	(void) nfsm_fhtom(nd, VTONFS(dvp)->n_fhp->nfh_fh,
		VTONFS(dvp)->n_fhp->nfh_len, 0);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSWCCATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
		nd->nd_flag |= ND_V4WCCATTR;
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_LINK);
	}
	(void) nfsm_strtom(nd, name, namelen);
	error = nfscl_request(nd, vp, p, cred, dstuff);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV3) {
		error = nfscl_postop_attr(nd, nap, attrflagp, dstuff);
		if (!error)
			error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp,
			    NULL, dstuff);
	} else if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) == ND_NFSV4) {
		/*
		 * First, parse out the PutFH and Getattr result.
		 */
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		if (!(*(tl + 1)))
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		if (*(tl + 1))
			nd->nd_flag |= ND_NOMOREDATA;
		/*
		 * Get the pre-op attributes.
		 */
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs symbolic link create rpc
 */
APPLESTATIC int
nfsrpc_symlink(vnode_t dvp, char *name, int namelen, char *target,
    struct vattr *vap, struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp, void *dstuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	int slen, error = 0;

	*nfhpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	nmp = VFSTONFS(vnode_mount(dvp));
	slen = strlen(target);
	if (slen > NFS_MAXPATHLEN || namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_SYMLINK, dvp);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFLNK);
		(void) nfsm_strtom(nd, target, slen);
	}
	(void) nfsm_strtom(nd, name, namelen);
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4))
		nfscl_fillsattr(nd, vap, dvp, 0, 0);
	if (!(nd->nd_flag & ND_NFSV4))
		(void) nfsm_strtom(nd, target, slen);
	if (nd->nd_flag & ND_NFSV2)
		nfscl_fillsattr(nd, vap, dvp, NFSSATTR_SIZENEG1, 0);
	error = nfscl_request(nd, dvp, p, cred, dstuff);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV4)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if ((nd->nd_flag & ND_NFSV3) && !error) {
		if (!nd->nd_repstat)
			error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (!error)
			error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp,
			    NULL, dstuff);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs make dir rpc
 */
APPLESTATIC int
nfsrpc_mkdir(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp, void *dstuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;
	int error = 0;

	*nfhpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_MKDIR, dvp);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFDIR);
	}
	(void) nfsm_strtom(nd, name, namelen);
	nfscl_fillsattr(nd, vap, dvp, NFSSATTR_SIZENEG1, 0);
	if (nd->nd_flag & ND_NFSV4) {
		NFSGETATTR_ATTRBIT(&attrbits);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(NFSV4OP_GETFH);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, dvp, p, cred, dstuff);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV4)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if (!nd->nd_repstat && !error) {
		if (nd->nd_flag & ND_NFSV4) {
			NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		}
		if (!error)
			error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
	}
	if ((nd->nd_flag & ND_NFSV3) && !error)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	/*
	 * Kludge: Map EEXIST => 0 assuming that you have a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs remove directory call
 */
APPLESTATIC int
nfsrpc_rmdir(vnode_t dvp, char *name, int namelen, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *dnap, int *dattrflagp, void *dstuff)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error = 0;

	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_RMDIR, dvp);
	(void) nfsm_strtom(nd, name, namelen);
	error = nfscl_request(nd, dvp, p, cred, dstuff);
	if (error)
		return (error);
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4))
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, dstuff);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * Readdir rpc.
 * Always returns with either uio_resid unchanged, if you are at the
 * end of the directory, or uio_resid == 0, with all DIRBLKSIZ chunks
 * filled in.
 * I felt this would allow caching of directory blocks more easily
 * than returning a pertially filled block.
 * Directory offset cookies:
 * Oh my, what to do with them...
 * I can think of three ways to deal with them:
 * 1 - have the layer above these RPCs maintain a map between logical
 *     directory byte offsets and the NFS directory offset cookies
 * 2 - pass the opaque directory offset cookies up into userland
 *     and let the libc functions deal with them, via the system call
 * 3 - return them to userland in the "struct dirent", so future versions
 *     of libc can use them and do whatever is necessary to amke things work
 *     above these rpc calls, in the meantime
 * For now, I do #3 by "hiding" the directory offset cookies after the
 * d_name field in struct dirent. This is space inside d_reclen that
 * will be ignored by anything that doesn't know about them.
 * The directory offset cookies are filled in as the last 8 bytes of
 * each directory entry, after d_name. Someday, the userland libc
 * functions may be able to use these. In the meantime, it satisfies
 * OpenBSD's requirements for cookies being returned.
 * If expects the directory offset cookie for the read to be in uio_offset
 * and returns the one for the next entry after this directory block in
 * there, as well.
 */
APPLESTATIC int
nfsrpc_readdir(vnode_t vp, struct uio *uiop, nfsuint64 *cookiep,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    int *eofp, void *stuff)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	nfsquad_t cookie, ncookie;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	struct nfsnode *dnp = VTONFS(vp);
	struct nfsvattr nfsva;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int reqsize, tryformoredirs = 1, readsize, eof = 0, gotmnton = 0;
	long dotfileid, dotdotfileid = 0;
	u_int32_t fakefileno = 0xffffffff, rderr;
	char *cp;
	nfsattrbit_t attrbits, dattrbits;
	u_int32_t *tl2 = NULL;
	size_t tresid;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 || (uio_uio_resid(uiop) & (DIRBLKSIZ - 1)))
		panic("nfs readdirrpc bad uio");
#endif

	/*
	 * There is no point in reading a lot more than uio_resid, however
	 * adding one additional DIRBLKSIZ makes sense. Since uio_resid
	 * and nm_readdirsize are both exact multiples of DIRBLKSIZ, this
	 * will never make readsize > nm_readdirsize.
	 */
	readsize = nmp->nm_readdirsize;
	if (readsize > uio_uio_resid(uiop))
		readsize = uio_uio_resid(uiop) + DIRBLKSIZ;

	*attrflagp = 0;
	if (eofp)
		*eofp = 0;
	tresid = uio_uio_resid(uiop);
	cookie.lval[0] = cookiep->nfsuquad[0];
	cookie.lval[1] = cookiep->nfsuquad[1];
	nd->nd_mrep = NULL;

	/*
	 * For NFSv4, first create the "." and ".." entries.
	 */
	if (NFSHASNFSV4(nmp)) {
		reqsize = 6 * NFSX_UNSIGNED;
		NFSGETATTR_ATTRBIT(&dattrbits);
		NFSZERO_ATTRBIT(&attrbits);
		NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_FILEID);
		NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_TYPE);
		if (NFSISSET_ATTRBIT(&dnp->n_vattr.na_suppattr,
		    NFSATTRBIT_MOUNTEDONFILEID)) {
			NFSSETBIT_ATTRBIT(&attrbits,
			    NFSATTRBIT_MOUNTEDONFILEID);
			gotmnton = 1;
		} else {
			/*
			 * Must fake it. Use the fileno, except when the
			 * fsid is != to that of the directory. For that
			 * case, generate a fake fileno that is not the same.
			 */
			NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_FSID);
			gotmnton = 0;
		}

		/*
		 * Joy, oh joy. For V4 we get to hand craft '.' and '..'.
		 */
		if (uiop->uio_offset == 0) {
#if defined(__FreeBSD_version) && __FreeBSD_version >= 800000
			error = VOP_GETATTR(vp, &nfsva.na_vattr, cred);
#else
			error = VOP_GETATTR(vp, &nfsva.na_vattr, cred, p);
#endif
			if (error)
			    return (error);
			dotfileid = nfsva.na_fileid;
			NFSCL_REQSTART(nd, NFSPROC_LOOKUPP, vp);
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSV4OP_GETFH);
			*tl = txdr_unsigned(NFSV4OP_GETATTR);
			(void) nfsrv_putattrbit(nd, &attrbits);
			error = nfscl_request(nd, vp, p, cred, stuff);
			if (error)
			    return (error);
			if (nd->nd_repstat == 0) {
			    NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
			    len = fxdr_unsigned(int, *(tl + 2));
			    if (len > 0 && len <= NFSX_V4FHMAX)
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
			    else
				error = EPERM;
			    if (!error) {
				NFSM_DISSECT(tl, u_int32_t *, 2*NFSX_UNSIGNED);
				nfsva.na_mntonfileno = 0xffffffff;
				error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
				    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
				    NULL, NULL, NULL, p, cred);
				if (error) {
				    dotdotfileid = dotfileid;
				} else if (gotmnton) {
				    if (nfsva.na_mntonfileno != 0xffffffff)
					dotdotfileid = nfsva.na_mntonfileno;
				    else
					dotdotfileid = nfsva.na_fileid;
				} else if (nfsva.na_filesid[0] ==
				    dnp->n_vattr.na_filesid[0] &&
				    nfsva.na_filesid[1] ==
				    dnp->n_vattr.na_filesid[1]) {
				    dotdotfileid = nfsva.na_fileid;
				} else {
				    do {
					fakefileno--;
				    } while (fakefileno ==
					nfsva.na_fileid);
				    dotdotfileid = fakefileno;
				}
			    }
			} else if (nd->nd_repstat == NFSERR_NOENT) {
			    /*
			     * Lookupp returns NFSERR_NOENT when we are
			     * at the root, so just use the current dir.
			     */
			    nd->nd_repstat = 0;
			    dotdotfileid = dotfileid;
			} else {
			    error = nd->nd_repstat;
			}
			mbuf_freem(nd->nd_mrep);
			if (error)
			    return (error);
			nd->nd_mrep = NULL;
			dp = (struct dirent *) CAST_DOWN(caddr_t, uio_iov_base(uiop));
			dp->d_type = DT_DIR;
			dp->d_fileno = dotfileid;
			dp->d_namlen = 1;
			dp->d_name[0] = '.';
			dp->d_name[1] = '\0';
			dp->d_reclen = DIRENT_SIZE(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[4];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uio_uio_resid_add(uiop, -(dp->d_reclen));
			uiop->uio_offset += dp->d_reclen;
			uio_iov_base_add(uiop, dp->d_reclen);
			uio_iov_len_add(uiop, -(dp->d_reclen));
			dp = (struct dirent *) CAST_DOWN(caddr_t, uio_iov_base(uiop));
			dp->d_type = DT_DIR;
			dp->d_fileno = dotdotfileid;
			dp->d_namlen = 2;
			dp->d_name[0] = '.';
			dp->d_name[1] = '.';
			dp->d_name[2] = '\0';
			dp->d_reclen = DIRENT_SIZE(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[4];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uio_uio_resid_add(uiop, -(dp->d_reclen));
			uiop->uio_offset += dp->d_reclen;
			uio_iov_base_add(uiop, dp->d_reclen);
			uio_iov_len_add(uiop, -(dp->d_reclen));
		}
		NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_RDATTRERROR);
	} else {
		reqsize = 5 * NFSX_UNSIGNED;
	}


	/*
	 * Loop around doing readdir rpc's of size readsize.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		*attrflagp = 0;
		NFSCL_REQSTART(nd, NFSPROC_READDIR, vp);
		if (nd->nd_flag & ND_NFSV2) {
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = cookie.lval[1];
			*tl = txdr_unsigned(readsize);
		} else {
			NFSM_BUILD(tl, u_int32_t *, reqsize);
			*tl++ = cookie.lval[0];
			*tl++ = cookie.lval[1];
			if (cookie.qval == 0) {
				*tl++ = 0;
				*tl++ = 0;
			} else {
				NFSLOCKNODE(dnp);
				*tl++ = dnp->n_cookieverf.nfsuquad[0];
				*tl++ = dnp->n_cookieverf.nfsuquad[1];
				NFSUNLOCKNODE(dnp);
			}
			if (nd->nd_flag & ND_NFSV4) {
				*tl++ = txdr_unsigned(readsize);
				*tl = txdr_unsigned(readsize);
				(void) nfsrv_putattrbit(nd, &attrbits);
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(NFSV4OP_GETATTR);
				(void) nfsrv_putattrbit(nd, &dattrbits);
			} else {
				*tl = txdr_unsigned(readsize);
			}
		}
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		if (!(nd->nd_flag & ND_NFSV2)) {
			if (nd->nd_flag & ND_NFSV3)
				error = nfscl_postop_attr(nd, nap, attrflagp,
				    stuff);
			if (!nd->nd_repstat && !error) {
				NFSM_DISSECT(tl, u_int32_t *, NFSX_HYPER);
				NFSLOCKNODE(dnp);
				dnp->n_cookieverf.nfsuquad[0] = *tl++;
				dnp->n_cookieverf.nfsuquad[1] = *tl;
				NFSUNLOCKNODE(dnp);
			}
		}
		if (nd->nd_repstat || error) {
			if (!error)
				error = nd->nd_repstat;
			goto nfsmout;
		}
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);
		if (!more_dirs)
			tryformoredirs = 0;
	
		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			if (nd->nd_flag & ND_NFSV4) {
				NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
				ncookie.lval[0] = *tl++;
				ncookie.lval[1] = *tl++;
				len = fxdr_unsigned(int, *tl);
			} else if (nd->nd_flag & ND_NFSV3) {
				NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
				nfsva.na_fileid =
				    fxdr_unsigned(long, *++tl);
				len = fxdr_unsigned(int, *++tl);
			} else {
				NFSM_DISSECT(tl, u_int32_t *, 2*NFSX_UNSIGNED);
				nfsva.na_fileid =
				    fxdr_unsigned(long, *tl++);
				len = fxdr_unsigned(int, *tl);
			}
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				goto nfsmout;
			}
			tlen = NFSM_RNDUP(len);
			if (tlen == len)
				tlen += 4;  /* To ensure null termination */
			left = DIRBLKSIZ - blksiz;
			if ((int)(tlen + DIRHDSIZ + NFSX_HYPER) > left) {
				dp->d_reclen += left;
				uio_iov_base_add(uiop, left);
				uio_iov_len_add(uiop, -(left));
				uio_uio_resid_add(uiop, -(left));
				uiop->uio_offset += left;
				blksiz = 0;
			}
			if ((int)(tlen + DIRHDSIZ + NFSX_HYPER) > uio_uio_resid(uiop))
				bigenough = 0;
			if (bigenough) {
				dp = (struct dirent *) CAST_DOWN(caddr_t, uio_iov_base(uiop));
				dp->d_namlen = len;
				dp->d_reclen = tlen + DIRHDSIZ + NFSX_HYPER;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == DIRBLKSIZ)
					blksiz = 0;
				uio_uio_resid_add(uiop, -(DIRHDSIZ));
				uiop->uio_offset += DIRHDSIZ;
				uio_iov_base_add(uiop, DIRHDSIZ);
				uio_iov_len_add(uiop, -(DIRHDSIZ));
				error = nfsm_mbufuio(nd, uiop, len);
				if (error)
					goto nfsmout;
				cp = CAST_DOWN(caddr_t, uio_iov_base(uiop));
				tlen -= len;
				*cp = '\0';	/* null terminate */
				cp += tlen;	/* points to cookie storage */
				tl2 = (u_int32_t *)cp;
				uio_iov_base_add(uiop, (tlen + NFSX_HYPER));
				uio_iov_len_add(uiop, -(tlen + NFSX_HYPER));
				uio_uio_resid_add(uiop, -(tlen + NFSX_HYPER));
				uiop->uio_offset += (tlen + NFSX_HYPER);
			} else {
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
				if (error)
					goto nfsmout;
			}
			if (nd->nd_flag & ND_NFSV4) {
				rderr = 0;
				nfsva.na_mntonfileno = 0xffffffff;
				error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
				    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
				    NULL, NULL, &rderr, p, cred);
				if (error)
					goto nfsmout;
				NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			} else if (nd->nd_flag & ND_NFSV3) {
				NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
				ncookie.lval[0] = *tl++;
				ncookie.lval[1] = *tl++;
			} else {
				NFSM_DISSECT(tl, u_int32_t *, 2*NFSX_UNSIGNED);
				ncookie.lval[0] = 0;
				ncookie.lval[1] = *tl++;
			}
			if (bigenough) {
			    if (nd->nd_flag & ND_NFSV4) {
				if (rderr) {
				    dp->d_fileno = 0;
				} else {
				    if (gotmnton) {
					if (nfsva.na_mntonfileno != 0xffffffff)
					    dp->d_fileno = nfsva.na_mntonfileno;
					else
					    dp->d_fileno = nfsva.na_fileid;
				    } else if (nfsva.na_filesid[0] ==
					dnp->n_vattr.na_filesid[0] &&
					nfsva.na_filesid[1] ==
					dnp->n_vattr.na_filesid[1]) {
					dp->d_fileno = nfsva.na_fileid;
				    } else {
					do {
					    fakefileno--;
					} while (fakefileno ==
					    nfsva.na_fileid);
					dp->d_fileno = fakefileno;
				    }
				    dp->d_type = vtonfs_dtype(nfsva.na_type);
				}
			    } else {
				dp->d_fileno = nfsva.na_fileid;
			    }
			    *tl2++ = cookiep->nfsuquad[0] = cookie.lval[0] =
				ncookie.lval[0];
			    *tl2 = cookiep->nfsuquad[1] = cookie.lval[1] =
				ncookie.lval[1];
			}
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *tl);
			if (tryformoredirs)
				more_dirs = !eof;
			if (nd->nd_flag & ND_NFSV4) {
				error = nfscl_postop_attr(nd, nap, attrflagp,
				    stuff);
				if (error)
					goto nfsmout;
			}
		}
		mbuf_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = DIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		uio_iov_base_add(uiop, left);
		uio_iov_len_add(uiop, -(left));
		uio_uio_resid_add(uiop, -(left));
		uiop->uio_offset += left;
	}

	/*
	 * If returning no data, assume end of file.
	 * If not bigenough, return not end of file, since you aren't
	 *    returning all the data
	 * Otherwise, return the eof flag from the server.
	 */
	if (eofp) {
		if (tresid == ((size_t)(uio_uio_resid(uiop))))
			*eofp = 1;
		else if (!bigenough)
			*eofp = 0;
		else
			*eofp = eof;
	}

	/*
	 * Add extra empty records to any remaining DIRBLKSIZ chunks.
	 */
	while (uio_uio_resid(uiop) > 0 && ((size_t)(uio_uio_resid(uiop))) != tresid) {
		dp = (struct dirent *) CAST_DOWN(caddr_t, uio_iov_base(uiop));
		dp->d_type = DT_UNKNOWN;
		dp->d_fileno = 0;
		dp->d_namlen = 0;
		dp->d_name[0] = '\0';
		tl = (u_int32_t *)&dp->d_name[4];
		*tl++ = cookie.lval[0];
		*tl = cookie.lval[1];
		dp->d_reclen = DIRBLKSIZ;
		uio_iov_base_add(uiop, DIRBLKSIZ);
		uio_iov_len_add(uiop, -(DIRBLKSIZ));
		uio_uio_resid_add(uiop, -(DIRBLKSIZ));
		uiop->uio_offset += DIRBLKSIZ;
	}

nfsmout:
	if (nd->nd_mrep != NULL)
		mbuf_freem(nd->nd_mrep);
	return (error);
}

#ifndef APPLE
/*
 * NFS V3 readdir plus RPC. Used in place of nfsrpc_readdir().
 * (Also used for NFS V4 when mount flag set.)
 * (ditto above w.r.t. multiple of DIRBLKSIZ, etc.)
 */
APPLESTATIC int
nfsrpc_readdirplus(vnode_t vp, struct uio *uiop, nfsuint64 *cookiep,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    int *eofp, void *stuff)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	vnode_t newvp = NULLVP;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	struct nfsnode *dnp = VTONFS(vp), *np;
	struct nfsvattr nfsva;
	struct nfsfh *nfhp;
	nfsquad_t cookie, ncookie;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int attrflag, tryformoredirs = 1, eof = 0, gotmnton = 0;
	int unlocknewvp = 0;
	long dotfileid, dotdotfileid = 0, fileno = 0;
	char *cp;
	nfsattrbit_t attrbits, dattrbits;
	size_t tresid;
	u_int32_t *tl2 = NULL, fakefileno = 0xffffffff, rderr;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 || (uio_uio_resid(uiop) & (DIRBLKSIZ - 1)))
		panic("nfs readdirplusrpc bad uio");
#endif
	*attrflagp = 0;
	if (eofp != NULL)
		*eofp = 0;
	ndp->ni_dvp = vp;
	nd->nd_mrep = NULL;
	cookie.lval[0] = cookiep->nfsuquad[0];
	cookie.lval[1] = cookiep->nfsuquad[1];
	tresid = uio_uio_resid(uiop);

	/*
	 * For NFSv4, first create the "." and ".." entries.
	 */
	if (NFSHASNFSV4(nmp)) {
		NFSGETATTR_ATTRBIT(&dattrbits);
		NFSZERO_ATTRBIT(&attrbits);
		NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_FILEID);
		if (NFSISSET_ATTRBIT(&dnp->n_vattr.na_suppattr,
		    NFSATTRBIT_MOUNTEDONFILEID)) {
			NFSSETBIT_ATTRBIT(&attrbits,
			    NFSATTRBIT_MOUNTEDONFILEID);
			gotmnton = 1;
		} else {
			/*
			 * Must fake it. Use the fileno, except when the
			 * fsid is != to that of the directory. For that
			 * case, generate a fake fileno that is not the same.
			 */
			NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_FSID);
			gotmnton = 0;
		}

		/*
		 * Joy, oh joy. For V4 we get to hand craft '.' and '..'.
		 */
		if (uiop->uio_offset == 0) {
#if defined(__FreeBSD_version) && __FreeBSD_version >= 800000
			error = VOP_GETATTR(vp, &nfsva.na_vattr, cred);
#else
			error = VOP_GETATTR(vp, &nfsva.na_vattr, cred, p);
#endif
			if (error)
			    return (error);
			dotfileid = nfsva.na_fileid;
			NFSCL_REQSTART(nd, NFSPROC_LOOKUPP, vp);
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSV4OP_GETFH);
			*tl = txdr_unsigned(NFSV4OP_GETATTR);
			(void) nfsrv_putattrbit(nd, &attrbits);
			error = nfscl_request(nd, vp, p, cred, stuff);
			if (error)
			    return (error);
			if (nd->nd_repstat == 0) {
			    NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
			    len = fxdr_unsigned(int, *(tl + 2));
			    if (len > 0 && len <= NFSX_V4FHMAX)
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
			    else
				error = EPERM;
			    if (!error) {
				NFSM_DISSECT(tl, u_int32_t *, 2*NFSX_UNSIGNED);
				nfsva.na_mntonfileno = 0xffffffff;
				error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
				    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
				    NULL, NULL, NULL, p, cred);
				if (error) {
				    dotdotfileid = dotfileid;
				} else if (gotmnton) {
				    if (nfsva.na_mntonfileno != 0xffffffff)
					dotdotfileid = nfsva.na_mntonfileno;
				    else
					dotdotfileid = nfsva.na_fileid;
				} else if (nfsva.na_filesid[0] ==
				    dnp->n_vattr.na_filesid[0] &&
				    nfsva.na_filesid[1] ==
				    dnp->n_vattr.na_filesid[1]) {
				    dotdotfileid = nfsva.na_fileid;
				} else {
				    do {
					fakefileno--;
				    } while (fakefileno ==
					nfsva.na_fileid);
				    dotdotfileid = fakefileno;
				}
			    }
			} else if (nd->nd_repstat == NFSERR_NOENT) {
			    /*
			     * Lookupp returns NFSERR_NOENT when we are
			     * at the root, so just use the current dir.
			     */
			    nd->nd_repstat = 0;
			    dotdotfileid = dotfileid;
			} else {
			    error = nd->nd_repstat;
			}
			mbuf_freem(nd->nd_mrep);
			if (error)
			    return (error);
			nd->nd_mrep = NULL;
			dp = (struct dirent *)uio_iov_base(uiop);
			dp->d_type = DT_DIR;
			dp->d_fileno = dotfileid;
			dp->d_namlen = 1;
			dp->d_name[0] = '.';
			dp->d_name[1] = '\0';
			dp->d_reclen = DIRENT_SIZE(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[4];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uio_uio_resid_add(uiop, -(dp->d_reclen));
			uiop->uio_offset += dp->d_reclen;
			uio_iov_base_add(uiop, dp->d_reclen);
			uio_iov_len_add(uiop, -(dp->d_reclen));
			dp = (struct dirent *)uio_iov_base(uiop);
			dp->d_type = DT_DIR;
			dp->d_fileno = dotdotfileid;
			dp->d_namlen = 2;
			dp->d_name[0] = '.';
			dp->d_name[1] = '.';
			dp->d_name[2] = '\0';
			dp->d_reclen = DIRENT_SIZE(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[4];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uio_uio_resid_add(uiop, -(dp->d_reclen));
			uiop->uio_offset += dp->d_reclen;
			uio_iov_base_add(uiop, dp->d_reclen);
			uio_iov_len_add(uiop, -(dp->d_reclen));
		}
		NFSREADDIRPLUS_ATTRBIT(&attrbits);
		if (gotmnton)
			NFSSETBIT_ATTRBIT(&attrbits,
			    NFSATTRBIT_MOUNTEDONFILEID);
	}

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		*attrflagp = 0;
		NFSCL_REQSTART(nd, NFSPROC_READDIRPLUS, vp);
 		NFSM_BUILD(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
		*tl++ = cookie.lval[0];
		*tl++ = cookie.lval[1];
		if (cookie.qval == 0) {
			*tl++ = 0;
			*tl++ = 0;
		} else {
			NFSLOCKNODE(dnp);
			*tl++ = dnp->n_cookieverf.nfsuquad[0];
			*tl++ = dnp->n_cookieverf.nfsuquad[1];
			NFSUNLOCKNODE(dnp);
		}
		*tl++ = txdr_unsigned(nmp->nm_readdirsize);
		*tl = txdr_unsigned(nmp->nm_readdirsize);
		if (nd->nd_flag & ND_NFSV4) {
			(void) nfsrv_putattrbit(nd, &attrbits);
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_GETATTR);
			(void) nfsrv_putattrbit(nd, &dattrbits);
		}
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		if (nd->nd_flag & ND_NFSV3)
			error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
		if (nd->nd_repstat || error) {
			if (!error)
				error = nd->nd_repstat;
			goto nfsmout;
		}
		NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		NFSLOCKNODE(dnp);
		dnp->n_cookieverf.nfsuquad[0] = *tl++;
		dnp->n_cookieverf.nfsuquad[1] = *tl++;
		NFSUNLOCKNODE(dnp);
		more_dirs = fxdr_unsigned(int, *tl);
		if (!more_dirs)
			tryformoredirs = 0;
	
		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			if (nd->nd_flag & ND_NFSV4) {
				ncookie.lval[0] = *tl++;
				ncookie.lval[1] = *tl++;
			} else {
				fileno = fxdr_unsigned(long, *++tl);
				tl++;
			}
			len = fxdr_unsigned(int, *tl);
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				goto nfsmout;
			}
			tlen = NFSM_RNDUP(len);
			if (tlen == len)
				tlen += 4;  /* To ensure null termination */
			left = DIRBLKSIZ - blksiz;
			if ((tlen + DIRHDSIZ + NFSX_HYPER) > left) {
				dp->d_reclen += left;
				uio_iov_base_add(uiop, left);
				uio_iov_len_add(uiop, -(left));
				uio_uio_resid_add(uiop, -(left));
				uiop->uio_offset += left;
				blksiz = 0;
			}
			if ((tlen + DIRHDSIZ + NFSX_HYPER) > uio_uio_resid(uiop))
				bigenough = 0;
			if (bigenough) {
				dp = (struct dirent *)uio_iov_base(uiop);
				dp->d_namlen = len;
				dp->d_reclen = tlen + DIRHDSIZ + NFSX_HYPER;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == DIRBLKSIZ)
					blksiz = 0;
				uio_uio_resid_add(uiop, -(DIRHDSIZ));
				uiop->uio_offset += DIRHDSIZ;
				uio_iov_base_add(uiop, DIRHDSIZ);
				uio_iov_len_add(uiop, -(DIRHDSIZ));
				cnp->cn_nameptr = uio_iov_base(uiop);
				cnp->cn_namelen = len;
				NFSCNHASHZERO(cnp);
				error = nfsm_mbufuio(nd, uiop, len);
				if (error)
					goto nfsmout;
				cp = uio_iov_base(uiop);
				tlen -= len;
				*cp = '\0';
				cp += tlen;	/* points to cookie storage */
				tl2 = (u_int32_t *)cp;
				uio_iov_base_add(uiop, (tlen + NFSX_HYPER));
				uio_iov_len_add(uiop, -(tlen + NFSX_HYPER));
				uio_uio_resid_add(uiop, -(tlen + NFSX_HYPER));
				uiop->uio_offset += (tlen + NFSX_HYPER);
			} else {
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
				if (error)
					goto nfsmout;
			}
			nfhp = NULL;
			if (nd->nd_flag & ND_NFSV3) {
				NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
				ncookie.lval[0] = *tl++;
				ncookie.lval[1] = *tl++;
				attrflag = fxdr_unsigned(int, *tl);
				if (attrflag) {
				  error = nfsm_loadattr(nd, &nfsva);
				  if (error)
					goto nfsmout;
				}
				NFSM_DISSECT(tl,u_int32_t *,NFSX_UNSIGNED);
				if (*tl) {
					error = nfsm_getfh(nd, &nfhp);
					if (error)
					    goto nfsmout;
				}
				if (!attrflag && nfhp != NULL) {
					FREE((caddr_t)nfhp, M_NFSFH);
					nfhp = NULL;
				}
			} else {
				rderr = 0;
				nfsva.na_mntonfileno = 0xffffffff;
				error = nfsv4_loadattr(nd, NULL, &nfsva, &nfhp,
				    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
				    NULL, NULL, &rderr, p, cred);
				if (error)
					goto nfsmout;
			}

			if (bigenough) {
			    if (nd->nd_flag & ND_NFSV4) {
				if (rderr) {
				    dp->d_fileno = 0;
				} else if (gotmnton) {
				    if (nfsva.na_mntonfileno != 0xffffffff)
					dp->d_fileno = nfsva.na_mntonfileno;
				    else
					dp->d_fileno = nfsva.na_fileid;
				} else if (nfsva.na_filesid[0] ==
				    dnp->n_vattr.na_filesid[0] &&
				    nfsva.na_filesid[1] ==
				    dnp->n_vattr.na_filesid[1]) {
				    dp->d_fileno = nfsva.na_fileid;
				} else {
				    do {
					fakefileno--;
				    } while (fakefileno ==
					nfsva.na_fileid);
				    dp->d_fileno = fakefileno;
				}
			    } else {
				dp->d_fileno = fileno;
			    }
			    *tl2++ = cookiep->nfsuquad[0] = cookie.lval[0] =
				ncookie.lval[0];
			    *tl2 = cookiep->nfsuquad[1] = cookie.lval[1] =
				ncookie.lval[1];

			    if (nfhp != NULL) {
				if (NFSRV_CMPFH(nfhp->nfh_fh, nfhp->nfh_len,
				    dnp->n_fhp->nfh_fh, dnp->n_fhp->nfh_len)) {
				    VREF(vp);
				    newvp = vp;
				    unlocknewvp = 0;
				    FREE((caddr_t)nfhp, M_NFSFH);
				    np = dnp;
				} else {
				    error = nfscl_nget(vnode_mount(vp), vp,
				      nfhp, cnp, p, &np, NULL);
				    if (!error) {
					newvp = NFSTOV(np);
					unlocknewvp = 1;
				    }
				}
				nfhp = NULL;
				if (newvp != NULLVP) {
				    error = nfscl_loadattrcache(&newvp,
					&nfsva, NULL, NULL, 0, 0);
				    if (error) {
					if (unlocknewvp)
					    vput(newvp);
					else
					    vrele(newvp);
					goto nfsmout;
				    }
				    dp->d_type =
					vtonfs_dtype(np->n_vattr.na_type);
				    ndp->ni_vp = newvp;
				    NFSCNHASH(cnp, HASHINIT);
				    if (cnp->cn_namelen <= NCHNAMLEN) {
					np->n_ctime =
					  np->n_vattr.na_ctime.tv_sec;
					cache_enter(ndp->ni_dvp,ndp->ni_vp,cnp);
				    }
				    if (unlocknewvp)
					vput(newvp);
				    else
					vrele(newvp);
				    newvp = NULLVP;
				}
			    }
			} else if (nfhp != NULL) {
			    FREE((caddr_t)nfhp, M_NFSFH);
			}
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *tl);
			if (tryformoredirs)
				more_dirs = !eof;
			if (nd->nd_flag & ND_NFSV4) {
				error = nfscl_postop_attr(nd, nap, attrflagp,
				    stuff);
				if (error)
					goto nfsmout;
			}
		}
		mbuf_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = DIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		uio_iov_base_add(uiop, left);
		uio_iov_len_add(uiop, -(left));
		uio_uio_resid_add(uiop, -(left));
		uiop->uio_offset += left;
	}

	/*
	 * If returning no data, assume end of file.
	 * If not bigenough, return not end of file, since you aren't
	 *    returning all the data
	 * Otherwise, return the eof flag from the server.
	 */
	if (eofp != NULL) {
		if (tresid == uio_uio_resid(uiop))
			*eofp = 1;
		else if (!bigenough)
			*eofp = 0;
		else
			*eofp = eof;
	}

	/*
	 * Add extra empty records to any remaining DIRBLKSIZ chunks.
	 */
	while (uio_uio_resid(uiop) > 0 && uio_uio_resid(uiop) != tresid) {
		dp = (struct dirent *)uio_iov_base(uiop);
		dp->d_type = DT_UNKNOWN;
		dp->d_fileno = 0;
		dp->d_namlen = 0;
		dp->d_name[0] = '\0';
		tl = (u_int32_t *)&dp->d_name[4];
		*tl++ = cookie.lval[0];
		*tl = cookie.lval[1];
		dp->d_reclen = DIRBLKSIZ;
		uio_iov_base_add(uiop, DIRBLKSIZ);
		uio_iov_len_add(uiop, -(DIRBLKSIZ));
		uio_uio_resid_add(uiop, -(DIRBLKSIZ));
		uiop->uio_offset += DIRBLKSIZ;
	}

nfsmout:
	if (nd->nd_mrep != NULL)
		mbuf_freem(nd->nd_mrep);
	return (error);
}
#endif	/* !APPLE */

/*
 * Nfs commit rpc
 */
APPLESTATIC int
nfsrpc_commit(vnode_t vp, u_quad_t offset, int cnt, struct ucred *cred,
    NFSPROC_T *p, u_char *verfp, struct nfsvattr *nap, int *attrflagp,
    void *stuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;
	int error;
	
	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_COMMIT, vp);
	NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);
	if (nd->nd_flag & ND_NFSV4) {
		/*
		 * And do a Getattr op.
		 */
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	error = nfscl_wcc_data(nd, vp, nap, attrflagp, NULL, stuff);
	if (!error && !nd->nd_repstat) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_VERF);
		NFSBCOPY((caddr_t)tl, verfp, NFSX_VERF);
		if (nd->nd_flag & ND_NFSV4)
			error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
	}
nfsmout:
	if (!error && nd->nd_repstat)
		error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * NFS byte range lock rpc.
 * (Mostly just calls one of the three lower level RPC routines.)
 */
APPLESTATIC int
nfsrpc_advlock(vnode_t vp, off_t size, int op, struct flock *fl,
    int reclaim, struct ucred *cred, NFSPROC_T *p)
{
	struct nfscllockowner *lp;
	struct nfsclclient *clp;
	struct nfsfh *nfhp;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	u_int64_t off, len;
	off_t start, end;
	u_int32_t clidrev = 0;
	int error = 0, newone = 0, expireret = 0, retrycnt, donelocally;
	int callcnt, dorpc;

	/*
	 * Convert the flock structure into a start and end and do POSIX
	 * bounds checking.
	 */
	switch (fl->l_whence) {
	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		start = fl->l_start;
		off = fl->l_start;
		break;
	case SEEK_END:
		start = size + fl->l_start;
		off = size + fl->l_start;
		break;
	default:
		return (EINVAL);
	};
	if (start < 0)
		return (EINVAL);
	if (fl->l_len != 0) {
		end = start + fl->l_len - 1;
		if (end < start)
			return (EINVAL);
	}

	len = fl->l_len;
	if (len == 0)
		len = NFS64BITSSET;
	retrycnt = 0;
	do {
	    nd->nd_repstat = 0;
	    if (op == F_GETLK) {
		error = nfscl_getcl(vp, cred, p, &clp);
		if (error)
			return (error);
		error = nfscl_lockt(vp, clp, off, len, fl, p);
		if (!error) {
			clidrev = clp->nfsc_clientidrev;
			error = nfsrpc_lockt(nd, vp, clp, off, len, fl, cred,
			    p);
		} else if (error == -1) {
			error = 0;
		}
		nfscl_clientrelease(clp);
	    } else if (op == F_UNLCK && fl->l_type == F_UNLCK) {
		/*
		 * We must loop around for all lockowner cases.
		 */
		callcnt = 0;
		error = nfscl_getcl(vp, cred, p, &clp);
		if (error)
			return (error);
		do {
		    error = nfscl_relbytelock(vp, off, len, cred, p, callcnt,
			clp, &lp, &dorpc);
		    /*
		     * If it returns a NULL lp, we're done.
		     */
		    if (lp == NULL) {
			if (callcnt == 0)
			    nfscl_clientrelease(clp);
			else
			    nfscl_releasealllocks(clp, vp, p);
			return (error);
		    }
		    if (nmp->nm_clp != NULL)
			clidrev = nmp->nm_clp->nfsc_clientidrev;
		    else
			clidrev = 0;
		    /*
		     * If the server doesn't support Posix lock semantics,
		     * only allow locks on the entire file, since it won't
		     * handle overlapping byte ranges.
		     * There might still be a problem when a lock
		     * upgrade/downgrade (read<->write) occurs, since the
		     * server "might" expect an unlock first?
		     */
		    if (dorpc && (lp->nfsl_open->nfso_posixlock ||
			(off == 0 && len == NFS64BITSSET))) {
			/*
			 * Since the lock records will go away, we must
			 * wait for grace and delay here.
			 */
			do {
			    error = nfsrpc_locku(nd, nmp, lp, off, len,
				NFSV4LOCKT_READ, cred, p, 0);
			    if ((nd->nd_repstat == NFSERR_GRACE ||
				 nd->nd_repstat == NFSERR_DELAY) &&
				error == 0)
				(void) nfs_catnap(PZERO, "nfs_advlock");
			} while ((nd->nd_repstat == NFSERR_GRACE ||
			    nd->nd_repstat == NFSERR_DELAY) && error == 0);
		    }
		    callcnt++;
		} while (error == 0 && nd->nd_repstat == 0);
		nfscl_releasealllocks(clp, vp, p);
	    } else if (op == F_SETLK) {
		error = nfscl_getbytelock(vp, off, len, fl->l_type, cred, p,
		    NULL, 0, NULL, NULL, &lp, &newone, &donelocally);
		if (error || donelocally) {
			return (error);
		}
		if (nmp->nm_clp != NULL)
			clidrev = nmp->nm_clp->nfsc_clientidrev;
		else
			clidrev = 0;
		nfhp = VTONFS(vp)->n_fhp;
		if (!lp->nfsl_open->nfso_posixlock &&
		    (off != 0 || len != NFS64BITSSET)) {
			error = EINVAL;
		} else {
			error = nfsrpc_lock(nd, nmp, vp, nfhp->nfh_fh,
			    nfhp->nfh_len, lp, newone, reclaim, off,
			    len, fl->l_type, cred, p, 0);
		}
		if (!error)
			error = nd->nd_repstat;
		nfscl_lockrelease(lp, error, newone);
	    } else {
		error = EINVAL;
	    }
	    if (!error)
	        error = nd->nd_repstat;
	    if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		error == NFSERR_STALEDONTRECOVER ||
		error == NFSERR_STALECLIENTID || error == NFSERR_DELAY) {
		(void) nfs_catnap(PZERO, "nfs_advlock");
	    } else if ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID)
		&& clidrev != 0) {
		expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		retrycnt++;
	    }
	} while (error == NFSERR_GRACE ||
	    error == NFSERR_STALECLIENTID || error == NFSERR_DELAY ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_STALESTATEID ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * The lower level routine for the LockT case.
 */
APPLESTATIC int
nfsrpc_lockt(struct nfsrv_descript *nd, vnode_t vp,
    struct nfsclclient *clp, u_int64_t off, u_int64_t len, struct flock *fl,
    struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	int error, type, size;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];

	NFSCL_REQSTART(nd, NFSPROC_LOCKT, vp);
	NFSM_BUILD(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
	if (fl->l_type == F_RDLCK)
		*tl++ = txdr_unsigned(NFSV4LOCKT_READ);
	else
		*tl++ = txdr_unsigned(NFSV4LOCKT_WRITE);
	txdr_hyper(off, tl);
	tl += 2;
	txdr_hyper(len, tl);
	tl += 2;
	*tl++ = clp->nfsc_clientid.lval[0];
	*tl = clp->nfsc_clientid.lval[1];
	nfscl_filllockowner(p, own);
	(void) nfsm_strtom(nd, own, NFSV4CL_LOCKNAMELEN);
	error = nfscl_request(nd, vp, p, cred, NULL);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
		fl->l_type = F_UNLCK;
	} else if (nd->nd_repstat == NFSERR_DENIED) {
		nd->nd_repstat = 0;
		fl->l_whence = SEEK_SET;
		NFSM_DISSECT(tl, u_int32_t *, 8 * NFSX_UNSIGNED);
		fl->l_start = fxdr_hyper(tl);
		tl += 2;
		len = fxdr_hyper(tl);
		tl += 2;
		if (len == NFS64BITSSET)
			fl->l_len = 0;
		else
			fl->l_len = len;
		type = fxdr_unsigned(int, *tl++);
		if (type == NFSV4LOCKT_WRITE)
			fl->l_type = F_WRLCK;
		else
			fl->l_type = F_RDLCK;
		/*
		 * XXX For now, I have no idea what to do with the
		 * conflicting lock_owner, so I'll just set the pid == 0
		 * and skip over the lock_owner.
		 */
		fl->l_pid = (pid_t)0;
		tl += 2;
		size = fxdr_unsigned(int, *tl);
		if (size < 0 || size > NFSV4_OPAQUELIMIT)
			error = EBADRPC;
		if (!error)
			error = nfsm_advance(nd, NFSM_RNDUP(size), -1);
	} else if (nd->nd_repstat == NFSERR_STALECLIENTID)
		nfscl_initiate_recovery(clp);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * Lower level function that performs the LockU RPC.
 */
static int
nfsrpc_locku(struct nfsrv_descript *nd, struct nfsmount *nmp,
    struct nfscllockowner *lp, u_int64_t off, u_int64_t len,
    u_int32_t type, struct ucred *cred, NFSPROC_T *p, int syscred)
{
	u_int32_t *tl;
	int error;

	nfscl_reqstart(nd, NFSPROC_LOCKU, nmp, lp->nfsl_open->nfso_fh,
	    lp->nfsl_open->nfso_fhlen, NULL);
	NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID + 6 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(type);
	*tl = txdr_unsigned(lp->nfsl_seqid);
	if (nfstest_outofseq &&
	    (arc4random() % nfstest_outofseq) == 0)
		*tl = txdr_unsigned(lp->nfsl_seqid + 1);
	tl++;
	*tl++ = lp->nfsl_stateid.seqid;
	*tl++ = lp->nfsl_stateid.other[0];
	*tl++ = lp->nfsl_stateid.other[1];
	*tl++ = lp->nfsl_stateid.other[2];
	txdr_hyper(off, tl);
	tl += 2;
	txdr_hyper(len, tl);
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL);
	NFSCL_INCRSEQID(lp->nfsl_seqid, nd);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
		lp->nfsl_stateid.seqid = *tl++;
		lp->nfsl_stateid.other[0] = *tl++;
		lp->nfsl_stateid.other[1] = *tl++;
		lp->nfsl_stateid.other[2] = *tl;
	} else if (nd->nd_repstat == NFSERR_STALESTATEID)
		nfscl_initiate_recovery(lp->nfsl_open->nfso_own->nfsow_clp);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * The actual Lock RPC.
 */
APPLESTATIC int
nfsrpc_lock(struct nfsrv_descript *nd, struct nfsmount *nmp, vnode_t vp,
    u_int8_t *nfhp, int fhlen, struct nfscllockowner *lp, int newone,
    int reclaim, u_int64_t off, u_int64_t len, short type, struct ucred *cred,
    NFSPROC_T *p, int syscred)
{
	u_int32_t *tl;
	int error, size;

	nfscl_reqstart(nd, NFSPROC_LOCK, nmp, nfhp, fhlen, NULL);
	NFSM_BUILD(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
	if (type == F_RDLCK)
		*tl++ = txdr_unsigned(NFSV4LOCKT_READ);
	else
		*tl++ = txdr_unsigned(NFSV4LOCKT_WRITE);
	*tl++ = txdr_unsigned(reclaim);
	txdr_hyper(off, tl);
	tl += 2;
	txdr_hyper(len, tl);
	tl += 2;
	if (newone) {
	    *tl = newnfs_true;
	    NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID +
		2 * NFSX_UNSIGNED + NFSX_HYPER);
	    *tl++ = txdr_unsigned(lp->nfsl_open->nfso_own->nfsow_seqid);
	    *tl++ = lp->nfsl_open->nfso_stateid.seqid;
	    *tl++ = lp->nfsl_open->nfso_stateid.other[0];
	    *tl++ = lp->nfsl_open->nfso_stateid.other[1];
	    *tl++ = lp->nfsl_open->nfso_stateid.other[2];
	    *tl++ = txdr_unsigned(lp->nfsl_seqid);
	    *tl++ = lp->nfsl_open->nfso_own->nfsow_clp->nfsc_clientid.lval[0];
	    *tl = lp->nfsl_open->nfso_own->nfsow_clp->nfsc_clientid.lval[1];
	    (void) nfsm_strtom(nd, lp->nfsl_owner, NFSV4CL_LOCKNAMELEN);
	} else {
	    *tl = newnfs_false;
	    NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID + NFSX_UNSIGNED);
	    *tl++ = lp->nfsl_stateid.seqid;
	    *tl++ = lp->nfsl_stateid.other[0];
	    *tl++ = lp->nfsl_stateid.other[1];
	    *tl++ = lp->nfsl_stateid.other[2];
	    *tl = txdr_unsigned(lp->nfsl_seqid);
	    if (nfstest_outofseq &&
		(arc4random() % nfstest_outofseq) == 0)
		    *tl = txdr_unsigned(lp->nfsl_seqid + 1);
	}
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, vp, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	if (newone)
	    NFSCL_INCRSEQID(lp->nfsl_open->nfso_own->nfsow_seqid, nd);
	NFSCL_INCRSEQID(lp->nfsl_seqid, nd);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
		lp->nfsl_stateid.seqid = *tl++;
		lp->nfsl_stateid.other[0] = *tl++;
		lp->nfsl_stateid.other[1] = *tl++;
		lp->nfsl_stateid.other[2] = *tl;
	} else if (nd->nd_repstat == NFSERR_DENIED) {
		NFSM_DISSECT(tl, u_int32_t *, 8 * NFSX_UNSIGNED);
		size = fxdr_unsigned(int, *(tl + 7));
		if (size < 0 || size > NFSV4_OPAQUELIMIT)
			error = EBADRPC;
		if (!error)
			error = nfsm_advance(nd, NFSM_RNDUP(size), -1);
	} else if (nd->nd_repstat == NFSERR_STALESTATEID)
		nfscl_initiate_recovery(lp->nfsl_open->nfso_own->nfsow_clp);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs statfs rpc
 * (always called with the vp for the mount point)
 */
APPLESTATIC int
nfsrpc_statfs(vnode_t vp, struct nfsstatfs *sbp, struct nfsfsinfo *fsp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    void *stuff)
{
	u_int32_t *tl = NULL;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	nfsattrbit_t attrbits;
	int error;

	*attrflagp = 0;
	nmp = VFSTONFS(vnode_mount(vp));
	if (NFSHASNFSV4(nmp)) {
		/*
		 * For V4, you actually do a getattr.
		 */
		NFSCL_REQSTART(nd, NFSPROC_GETATTR, vp);
		NFSSTATFS_GETATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
		nd->nd_flag |= ND_USEGSSNAME;
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		if (nd->nd_repstat == 0) {
			error = nfsv4_loadattr(nd, NULL, nap, NULL, NULL, 0,
			    NULL, NULL, sbp, fsp, NULL, 0, NULL, NULL, NULL, p,
			    cred);
			if (!error) {
				nmp->nm_fsid[0] = nap->na_filesid[0];
				nmp->nm_fsid[1] = nap->na_filesid[1];
				NFSSETHASSETFSID(nmp);
				*attrflagp = 1;
			}
		} else {
			error = nd->nd_repstat;
		}
		if (error)
			goto nfsmout;
	} else {
		NFSCL_REQSTART(nd, NFSPROC_FSSTAT, vp);
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		if (nd->nd_flag & ND_NFSV3) {
			error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
			if (error)
				goto nfsmout;
		}
		if (nd->nd_repstat) {
			error = nd->nd_repstat;
			goto nfsmout;
		}
		NFSM_DISSECT(tl, u_int32_t *,
		    NFSX_STATFS(nd->nd_flag & ND_NFSV3));
	}
	if (NFSHASNFSV3(nmp)) {
		sbp->sf_tbytes = fxdr_hyper(tl); tl += 2;
		sbp->sf_fbytes = fxdr_hyper(tl); tl += 2;
		sbp->sf_abytes = fxdr_hyper(tl); tl += 2;
		sbp->sf_tfiles = fxdr_hyper(tl); tl += 2;
		sbp->sf_ffiles = fxdr_hyper(tl); tl += 2;
		sbp->sf_afiles = fxdr_hyper(tl); tl += 2;
		sbp->sf_invarsec = fxdr_unsigned(u_int32_t, *tl);
	} else if (NFSHASNFSV4(nmp) == 0) {
		sbp->sf_tsize = fxdr_unsigned(u_int32_t, *tl++);
		sbp->sf_bsize = fxdr_unsigned(u_int32_t, *tl++);
		sbp->sf_blocks = fxdr_unsigned(u_int32_t, *tl++);
		sbp->sf_bfree = fxdr_unsigned(u_int32_t, *tl++);
		sbp->sf_bavail = fxdr_unsigned(u_int32_t, *tl);
	}
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs pathconf rpc
 */
APPLESTATIC int
nfsrpc_pathconf(vnode_t vp, struct nfsv3_pathconf *pc,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    void *stuff)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	u_int32_t *tl;
	nfsattrbit_t attrbits;
	int error;

	*attrflagp = 0;
	nmp = VFSTONFS(vnode_mount(vp));
	if (NFSHASNFSV4(nmp)) {
		/*
		 * For V4, you actually do a getattr.
		 */
		NFSCL_REQSTART(nd, NFSPROC_GETATTR, vp);
		NFSPATHCONF_GETATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
		nd->nd_flag |= ND_USEGSSNAME;
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		if (nd->nd_repstat == 0) {
			error = nfsv4_loadattr(nd, NULL, nap, NULL, NULL, 0,
			    pc, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, p,
			    cred);
			if (!error)
				*attrflagp = 1;
		} else {
			error = nd->nd_repstat;
		}
	} else {
		NFSCL_REQSTART(nd, NFSPROC_PATHCONF, vp);
		error = nfscl_request(nd, vp, p, cred, stuff);
		if (error)
			return (error);
		error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
		if (nd->nd_repstat && !error)
			error = nd->nd_repstat;
		if (!error) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_V3PATHCONF);
			pc->pc_linkmax = fxdr_unsigned(u_int32_t, *tl++);
			pc->pc_namemax = fxdr_unsigned(u_int32_t, *tl++);
			pc->pc_notrunc = fxdr_unsigned(u_int32_t, *tl++);
			pc->pc_chownrestricted =
			    fxdr_unsigned(u_int32_t, *tl++);
			pc->pc_caseinsensitive =
			    fxdr_unsigned(u_int32_t, *tl++);
			pc->pc_casepreserving = fxdr_unsigned(u_int32_t, *tl);
		}
	}
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs version 3 fsinfo rpc call
 */
APPLESTATIC int
nfsrpc_fsinfo(vnode_t vp, struct nfsfsinfo *fsp, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp, void *stuff)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_FSINFO, vp);
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	error = nfscl_postop_attr(nd, nap, attrflagp, stuff);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	if (!error) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_V3FSINFO);
		fsp->fs_rtmax = fxdr_unsigned(u_int32_t, *tl++);
		fsp->fs_rtpref = fxdr_unsigned(u_int32_t, *tl++);
		fsp->fs_rtmult = fxdr_unsigned(u_int32_t, *tl++);
		fsp->fs_wtmax = fxdr_unsigned(u_int32_t, *tl++);
		fsp->fs_wtpref = fxdr_unsigned(u_int32_t, *tl++);
		fsp->fs_wtmult = fxdr_unsigned(u_int32_t, *tl++);
		fsp->fs_dtpref = fxdr_unsigned(u_int32_t, *tl++);
		fsp->fs_maxfilesize = fxdr_hyper(tl);
		tl += 2;
		fxdr_nfsv3time(tl, &fsp->fs_timedelta);
		tl += 2;
		fsp->fs_properties = fxdr_unsigned(u_int32_t, *tl);
	}
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Renew RPC.
 */
APPLESTATIC int
nfsrpc_renew(struct nfsclclient *clp, struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfsmount *nmp;
	int error;

	nmp = clp->nfsc_nmp;
	if (nmp == NULL)
		return (0);
	nfscl_reqstart(nd, NFSPROC_RENEW, nmp, NULL, 0, NULL);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = clp->nfsc_clientid.lval[0];
	*tl = clp->nfsc_clientid.lval[1];
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
		NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Releaselockowner RPC.
 */
APPLESTATIC int
nfsrpc_rellockown(struct nfsmount *nmp, struct nfscllockowner *lp,
    struct ucred *cred, NFSPROC_T *p)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	u_int32_t *tl;
	int error;

	nfscl_reqstart(nd, NFSPROC_RELEASELCKOWN, nmp, NULL, 0, NULL);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = nmp->nm_clp->nfsc_clientid.lval[0];
	*tl = nmp->nm_clp->nfsc_clientid.lval[1];
	(void) nfsm_strtom(nd, lp->nfsl_owner, NFSV4CL_LOCKNAMELEN);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Compound to get the mount pt FH.
 */
APPLESTATIC int
nfsrpc_getdirpath(struct nfsmount *nmp, u_char *dirpath, struct ucred *cred,
    NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	u_char *cp, *cp2;
	int error, cnt, len, setnil;
	u_int32_t *opcntp;

	nfscl_reqstart(nd, NFSPROC_PUTROOTFH, nmp, NULL, 0, &opcntp);
	cp = dirpath;
	cnt = 0;
	do {
		setnil = 0;
		while (*cp == '/')
			cp++;
		cp2 = cp;
		while (*cp2 != '\0' && *cp2 != '/')
			cp2++;
		if (*cp2 == '/') {
			setnil = 1;
			*cp2 = '\0';
		}
		if (cp2 != cp) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_LOOKUP);
			nfsm_strtom(nd, cp, strlen(cp));
			cnt++;
		}
		if (setnil)
			*cp2++ = '/';
		cp = cp2;
	} while (*cp != '\0');
	*opcntp = txdr_unsigned(2 + cnt);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETFH);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
		NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, u_int32_t *, (3 + 2 * cnt) * NFSX_UNSIGNED);
		tl += (2 + 2 * cnt);
		if ((len = fxdr_unsigned(int, *tl)) <= 0 ||
			len > NFSX_FHMAX) {
			nd->nd_repstat = NFSERR_BADXDR;
		} else {
			nd->nd_repstat = nfsrv_mtostr(nd, nmp->nm_fh, len);
			if (nd->nd_repstat == 0)
				nmp->nm_fhsize = len;
		}
	}
	error = nd->nd_repstat;
nfsmout:
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Delegreturn RPC.
 */
APPLESTATIC int
nfsrpc_delegreturn(struct nfscldeleg *dp, struct ucred *cred,
    struct nfsmount *nmp, NFSPROC_T *p, int syscred)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	int error;

	nfscl_reqstart(nd, NFSPROC_DELEGRETURN, nmp, dp->nfsdl_fh,
	    dp->nfsdl_fhlen, NULL);
	NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
	*tl++ = dp->nfsdl_stateid.seqid;
	*tl++ = dp->nfsdl_stateid.other[0];
	*tl++ = dp->nfsdl_stateid.other[1];
	*tl = dp->nfsdl_stateid.other[2];
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL);
	if (error)
		return (error);
	error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs getacl call.
 */
APPLESTATIC int
nfsrpc_getacl(vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    struct acl *aclp, void *stuff)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	
	if (nfsrv_useacl == 0 || !NFSHASNFSV4(nmp))
		return (EOPNOTSUPP);
	NFSCL_REQSTART(nd, NFSPROC_GETACL, vp);
	NFSZERO_ATTRBIT(&attrbits);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_ACL);
	(void) nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	if (!nd->nd_repstat)
		error = nfsv4_loadattr(nd, vp, NULL, NULL, NULL, 0, NULL,
		    NULL, NULL, NULL, aclp, 0, NULL, NULL, NULL, p, cred);
	else
		error = nd->nd_repstat;
	mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs setacl call.
 */
APPLESTATIC int
nfsrpc_setacl(vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    struct acl *aclp, void *stuff)
{
	int error;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	
	if (nfsrv_useacl == 0 || !NFSHASNFSV4(nmp))
		return (EOPNOTSUPP);
	error = nfsrpc_setattr(vp, NULL, aclp, cred, p, NULL, NULL, stuff);
	return (error);
}

/*
 * nfs setacl call.
 */
static int
nfsrpc_setaclrpc(vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    struct acl *aclp, nfsv4stateid_t *stateidp, void *stuff)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	
	if (!NFSHASNFSV4(nmp))
		return (EOPNOTSUPP);
	NFSCL_REQSTART(nd, NFSPROC_SETACL, vp);
	nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
	NFSZERO_ATTRBIT(&attrbits);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_ACL);
	(void) nfsv4_fillattr(nd, vp, aclp, NULL, NULL, 0, &attrbits,
	    NULL, NULL, 0, 0);
	error = nfscl_request(nd, vp, p, cred, stuff);
	if (error)
		return (error);
	/* Don't care about the pre/postop attributes */
	mbuf_freem(nd->nd_mrep);
	return (nd->nd_repstat);
}
