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
/*
 * Rpc op calls, generally called from the vnode op calls or through the
 * buffer cache, for NFS v2, 3 and 4.
 * These do not normally make any changes to vnode arguments or use
 * structures that might change between the VFS variants. The returned
 * arguments are all at the end, after the NFSPROC_T *p one.
 */

#include "opt_inet6.h"

#include <fs/nfs/nfsport.h>
#include <fs/nfsclient/nfs.h>
#include <sys/extattr.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

SYSCTL_DECL(_vfs_nfs);

static int	nfsignore_eexist = 0;
SYSCTL_INT(_vfs_nfs, OID_AUTO, ignore_eexist, CTLFLAG_RW,
    &nfsignore_eexist, 0, "NFS ignore EEXIST replies for mkdir/symlink");

static int	nfscl_dssameconn = 0;
SYSCTL_INT(_vfs_nfs, OID_AUTO, dssameconn, CTLFLAG_RW,
    &nfscl_dssameconn, 0, "Use same TCP connection to multiple DSs");

static uint64_t nfs_maxcopyrange = SSIZE_MAX;
SYSCTL_U64(_vfs_nfs, OID_AUTO, maxcopyrange, CTLFLAG_RW,
    &nfs_maxcopyrange, 0, "Max size of a Copy so RPC times reasonable");

/*
 * Global variables
 */
extern struct nfsstatsv1 nfsstatsv1;
extern int nfs_numnfscbd;
extern struct timeval nfsboottime;
extern u_int32_t newnfs_false, newnfs_true;
extern nfstype nfsv34_type[9];
extern int nfsrv_useacl;
extern char nfsv4_callbackaddr[INET6_ADDRSTRLEN];
extern int nfscl_debuglevel;
extern int nfs_pnfsiothreads;
extern u_long sb_max_adj;
NFSCLSTATEMUTEX;
int nfstest_outofseq = 0;
int nfscl_assumeposixlocks = 1;
int nfscl_enablecallb = 0;
short nfsv4_cbport = NFSV4_CBPORT;
int nfstest_openallsetattr = 0;

#define	DIRHDSIZ	offsetof(struct dirent, d_name)

/*
 * nfscl_getsameserver() can return one of three values:
 * NFSDSP_USETHISSESSION - Use this session for the DS.
 * NFSDSP_SEQTHISSESSION - Use the nfsclds_sequence field of this dsp for new
 *     session.
 * NFSDSP_NOTFOUND - No matching server was found.
 */
enum nfsclds_state {
	NFSDSP_USETHISSESSION = 0,
	NFSDSP_SEQTHISSESSION = 1,
	NFSDSP_NOTFOUND = 2,
};

/*
 * Do a write RPC on a DS data file, using this structure for the arguments,
 * so that this function can be executed by a separate kernel process.
 */
struct nfsclwritedsdorpc {
	int			done;
	int			inprog;
	struct task		tsk;
	struct vnode		*vp;
	int			iomode;
	int			must_commit;
	nfsv4stateid_t		*stateidp;
	struct nfsclds		*dsp;
	uint64_t		off;
	int			len;
#ifdef notyet
	int			advise;
#endif
	struct nfsfh		*fhp;
	struct mbuf		*m;
	int			vers;
	int			minorvers;
	struct ucred		*cred;
	NFSPROC_T		*p;
	int			err;
};

static int nfsrpc_setattrrpc(vnode_t , struct vattr *, nfsv4stateid_t *,
    struct ucred *, NFSPROC_T *, struct nfsvattr *, int *);
static int nfsrpc_readrpc(vnode_t , struct uio *, struct ucred *,
    nfsv4stateid_t *, NFSPROC_T *, struct nfsvattr *, int *);
static int nfsrpc_writerpc(vnode_t , struct uio *, int *, int *,
    struct ucred *, nfsv4stateid_t *, NFSPROC_T *, struct nfsvattr *, int *,
    int);
static int nfsrpc_deallocaterpc(vnode_t, off_t, off_t, nfsv4stateid_t *,
    struct nfsvattr *, int *, struct ucred *, NFSPROC_T *);
static int nfsrpc_createv23(vnode_t , char *, int, struct vattr *,
    nfsquad_t, int, struct ucred *, NFSPROC_T *, struct nfsvattr *,
    struct nfsvattr *, struct nfsfh **, int *, int *);
static int nfsrpc_createv4(vnode_t , char *, int, struct vattr *,
    nfsquad_t, int, struct nfsclowner *, struct nfscldeleg **, struct ucred *,
    NFSPROC_T *, struct nfsvattr *, struct nfsvattr *, struct nfsfh **, int *,
    int *, int *);
static bool nfscl_invalidfname(bool, char *, int);
static int nfsrpc_locku(struct nfsrv_descript *, struct nfsmount *,
    struct nfscllockowner *, u_int64_t, u_int64_t,
    u_int32_t, struct ucred *, NFSPROC_T *, int);
static int nfsrpc_setaclrpc(vnode_t, struct ucred *, NFSPROC_T *,
    struct acl *, nfsv4stateid_t *);
static int nfsrpc_layouterror(struct nfsmount *, uint8_t *, int, uint64_t,
    uint64_t, nfsv4stateid_t *, struct ucred *, NFSPROC_T *, uint32_t,
    uint32_t, char *);
static int nfsrpc_getlayout(struct nfsmount *, vnode_t, struct nfsfh *, int,
    uint32_t, uint32_t *, nfsv4stateid_t *, uint64_t, struct nfscllayout **,
    struct ucred *, NFSPROC_T *);
static int nfsrpc_fillsa(struct nfsmount *, struct sockaddr_in *,
    struct sockaddr_in6 *, sa_family_t, int, int, struct nfsclds **,
    NFSPROC_T *);
static void nfscl_initsessionslots(struct nfsclsession *);
static int nfscl_doflayoutio(vnode_t, struct uio *, int *, int *, int *,
    nfsv4stateid_t *, int, struct nfscldevinfo *, struct nfscllayout *,
    struct nfsclflayout *, uint64_t, uint64_t, int, struct ucred *,
    NFSPROC_T *);
static int nfscl_dofflayoutio(vnode_t, struct uio *, int *, int *, int *,
    nfsv4stateid_t *, int, struct nfscldevinfo *, struct nfscllayout *,
    struct nfsclflayout *, uint64_t, uint64_t, int, int, struct mbuf *,
    struct nfsclwritedsdorpc *, struct ucred *, NFSPROC_T *);
static int nfsrpc_readds(vnode_t, struct uio *, nfsv4stateid_t *, int *,
    struct nfsclds *, uint64_t, int, struct nfsfh *, int, int, int,
    struct ucred *, NFSPROC_T *);
static int nfsrpc_writeds(vnode_t, struct uio *, int *, int *,
    nfsv4stateid_t *, struct nfsclds *, uint64_t, int,
    struct nfsfh *, int, int, int, int, struct ucred *, NFSPROC_T *);
static int nfsio_writedsmir(vnode_t, int *, int *, nfsv4stateid_t *,
    struct nfsclds *, uint64_t, int, struct nfsfh *, struct mbuf *, int, int,
    struct nfsclwritedsdorpc *, struct ucred *, NFSPROC_T *);
static int nfsrpc_writedsmir(vnode_t, int *, int *, nfsv4stateid_t *,
    struct nfsclds *, uint64_t, int, struct nfsfh *, struct mbuf *, int, int,
    struct ucred *, NFSPROC_T *);
static enum nfsclds_state nfscl_getsameserver(struct nfsmount *,
    struct nfsclds *, struct nfsclds **, uint32_t *);
static int nfsio_commitds(vnode_t, uint64_t, int, struct nfsclds *,
    struct nfsfh *, int, int, struct nfsclwritedsdorpc *, struct ucred *,
    NFSPROC_T *);
static int nfsrpc_commitds(vnode_t, uint64_t, int, struct nfsclds *,
    struct nfsfh *, int, int, struct ucred *, NFSPROC_T *);
#ifdef notyet
static int nfsio_adviseds(vnode_t, uint64_t, int, int, struct nfsclds *,
    struct nfsfh *, int, int, struct nfsclwritedsdorpc *, struct ucred *,
    NFSPROC_T *);
static int nfsrpc_adviseds(vnode_t, uint64_t, int, int, struct nfsclds *,
    struct nfsfh *, int, int, struct ucred *, NFSPROC_T *);
#endif
static int nfsrpc_allocaterpc(vnode_t, off_t, off_t, nfsv4stateid_t *,
    struct nfsvattr *, int *, struct ucred *, NFSPROC_T *);
static void nfsrv_setuplayoutget(struct nfsrv_descript *, int, uint64_t,
    uint64_t, uint64_t, nfsv4stateid_t *, int, int, int);
static int nfsrv_parseug(struct nfsrv_descript *, int, uid_t *, gid_t *,
    NFSPROC_T *);
static int nfsrv_parselayoutget(struct nfsmount *, struct nfsrv_descript *,
    nfsv4stateid_t *, int *, struct nfsclflayouthead *);
static int nfsrpc_getopenlayout(struct nfsmount *, vnode_t, u_int8_t *,
    int, uint8_t *, int, uint32_t, struct nfsclopen *, uint8_t *, int,
    struct nfscldeleg **, struct ucred *, NFSPROC_T *);
static int nfsrpc_getcreatelayout(vnode_t, char *, int, struct vattr *,
    nfsquad_t, int, struct nfsclowner *, struct nfscldeleg **,
    struct ucred *, NFSPROC_T *, struct nfsvattr *, struct nfsvattr *,
    struct nfsfh **, int *, int *, int *);
static int nfsrpc_openlayoutrpc(struct nfsmount *, vnode_t, u_int8_t *,
    int, uint8_t *, int, uint32_t, struct nfsclopen *, uint8_t *, int,
    struct nfscldeleg **, nfsv4stateid_t *, int, int, int, int *,
    struct nfsclflayouthead *, int *, struct ucred *, NFSPROC_T *);
static int nfsrpc_createlayout(vnode_t, char *, int, struct vattr *,
    nfsquad_t, int, struct nfsclowner *, struct nfscldeleg **,
    struct ucred *, NFSPROC_T *, struct nfsvattr *, struct nfsvattr *,
    struct nfsfh **, int *, int *, int *, nfsv4stateid_t *,
    int, int, int, int *, struct nfsclflayouthead *, int *);
static int nfsrpc_layoutget(struct nfsmount *, uint8_t *, int, int, uint64_t,
    uint64_t, uint64_t, int, int, nfsv4stateid_t *, int *,
    struct nfsclflayouthead *, struct ucred *, NFSPROC_T *);
static int nfsrpc_layoutgetres(struct nfsmount *, vnode_t, uint8_t *,
    int, nfsv4stateid_t *, int, uint32_t *, struct nfscllayout **,
    struct nfsclflayouthead *, int, int, int *, struct ucred *, NFSPROC_T *);
static int nfsrpc_copyrpc(vnode_t, off_t, vnode_t, off_t, size_t *,
    nfsv4stateid_t *, nfsv4stateid_t *, struct nfsvattr *, int *,
    struct nfsvattr *, int *, bool, int *, struct ucred *, NFSPROC_T *);
static int nfsrpc_seekrpc(vnode_t, off_t *, nfsv4stateid_t *, bool *,
    int, struct nfsvattr *, int *, struct ucred *);
static struct mbuf *nfsm_split(struct mbuf *, uint64_t);
static void nfscl_statfs(struct vnode *, struct ucred *, NFSPROC_T *);

int nfs_pnfsio(task_fn_t *, void *);

/*
 * nfs null call from vfs.
 */
int
nfsrpc_null(vnode_t vp, struct ucred *cred, NFSPROC_T *p)
{
	int error;
	struct nfsrv_descript nfsd, *nd = &nfsd;

	NFSCL_REQSTART(nd, NFSPROC_NULL, vp, NULL);
	error = nfscl_request(nd, vp, p, cred);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs access rpc op.
 * For nfs version 3 and 4, use the access rpc to check accessibility. If file
 * modes are changed on the server, accesses might still fail later.
 */
int
nfsrpc_access(vnode_t vp, int acmode, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp)
{
	int error;
	u_int32_t mode, rmode;

	if (acmode & VREAD)
		mode = NFSACCESS_READ;
	else
		mode = 0;
	if (vp->v_type == VDIR) {
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
	error = nfsrpc_accessrpc(vp, mode, cred, p, nap, attrflagp, &rmode);

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
int
nfsrpc_accessrpc(vnode_t vp, u_int32_t mode, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp, u_int32_t *rmodep)
{
	u_int32_t *tl;
	u_int32_t supported, rmode;
	int error;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;
	struct nfsmount *nmp;
	struct nfsnode *np;

	*attrflagp = 0;
	supported = mode;
	nmp = VFSTONFS(vp->v_mount);
	np = VTONFS(vp);
	if ((nmp->nm_privflag & NFSMNTP_FAKEROOTFH) != 0 &&
	    nmp->nm_fhsize == 0) {
		/* Attempt to get the actual root file handle. */
		error = nfsrpc_getdirpath(nmp, NFSMNT_DIRPATH(nmp), cred, p);
		if (error != 0)
			return (EACCES);
		if (np->n_fhp->nfh_len == NFSX_FHMAX + 1)
			nfscl_statfs(vp, cred, p);
	}
	NFSCL_REQSTART(nd, NFSPROC_ACCESS, vp, cred);
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
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV3) {
		error = nfscl_postop_attr(nd, nap, attrflagp);
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
			error = nfscl_postop_attr(nd, nap, attrflagp);

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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs open rpc
 */
int
nfsrpc_open(vnode_t vp, int amode, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsclopen *op;
	struct nfscldeleg *dp;
	struct nfsfh *nfhp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	u_int32_t mode, clidrev;
	int ret, newone, error, expireret = 0, retrycnt;

	/*
	 * For NFSv4, Open Ops are only done on Regular Files.
	 */
	if (vp->v_type != VREG)
		return (0);
	mode = 0;
	if (amode & FREAD)
		mode |= NFSV4OPEN_ACCESSREAD;
	if (amode & FWRITE)
		mode |= NFSV4OPEN_ACCESSWRITE;
	if (NFSHASNFSV4N(nmp)) {
		if (!NFSHASPNFS(nmp) && nfscl_enablecallb != 0 &&
		    nfs_numnfscbd > 0) {
			if ((mode & NFSV4OPEN_ACCESSWRITE) != 0)
				mode |= NFSV4OPEN_WANTWRITEDELEG;
			else
				mode |= NFSV4OPEN_WANTANYDELEG;
		} else
			mode |= NFSV4OPEN_WANTNODELEG;
	}
	nfhp = np->n_fhp;

	retrycnt = 0;
	do {
	    dp = NULL;
	    error = nfscl_open(vp, nfhp->nfh_fh, nfhp->nfh_len,
		(mode & NFSV4OPEN_ACCESSBOTH), 1, cred, p, NULL,
		&op, &newone, &ret, 1, true);
	    if (error) {
		return (error);
	    }
	    if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	    else
		clidrev = 0;
	    if (ret == NFSCLOPEN_DOOPEN) {
		if (np->n_v4 != NULL) {
			/*
			 * For the first attempt, try and get a layout, if
			 * pNFS is enabled for the mount.
			 */
			if (!NFSHASPNFS(nmp) || nfscl_enablecallb == 0 ||
			    nfs_numnfscbd == 0 ||
			    (np->n_flag & NNOLAYOUT) != 0 || retrycnt > 0)
				error = nfsrpc_openrpc(nmp, vp,
				    np->n_v4->n4_data,
				    np->n_v4->n4_fhlen, np->n_fhp->nfh_fh,
				    np->n_fhp->nfh_len, mode, op,
				    NFS4NODENAME(np->n_v4),
				    np->n_v4->n4_namelen,
				    &dp, 0, 0x0, cred, p, 0, 0);
			else
				error = nfsrpc_getopenlayout(nmp, vp,
				    np->n_v4->n4_data,
				    np->n_v4->n4_fhlen, np->n_fhp->nfh_fh,
				    np->n_fhp->nfh_len, mode, op,
				    NFS4NODENAME(np->n_v4),
				    np->n_v4->n4_namelen, &dp, cred, p);
			if (dp != NULL) {
				NFSLOCKNODE(np);
				np->n_flag &= ~NDELEGMOD;
				/*
				 * Invalidate the attribute cache, so that
				 * attributes that pre-date the issue of a
				 * delegation are not cached, since the
				 * cached attributes will remain valid while
				 * the delegation is held.
				 */
				NFSINVALATTRCACHE(np);
				NFSUNLOCKNODE(np);
				(void) nfscl_deleg(nmp->nm_mountp,
				    op->nfso_own->nfsow_clp,
				    nfhp->nfh_fh, nfhp->nfh_len, cred, p, dp);
			}
		} else if (NFSHASNFSV4N(nmp)) {
			/*
			 * For the first attempt, try and get a layout, if
			 * pNFS is enabled for the mount.
			 */
			if (!NFSHASPNFS(nmp) || nfscl_enablecallb == 0 ||
			    nfs_numnfscbd == 0 ||
			    (np->n_flag & NNOLAYOUT) != 0 || retrycnt > 0)
				error = nfsrpc_openrpc(nmp, vp, nfhp->nfh_fh,
				    nfhp->nfh_len, nfhp->nfh_fh, nfhp->nfh_len,
				    mode, op, NULL, 0, &dp, 0, 0x0, cred, p, 0,
				    0);
			else
				error = nfsrpc_getopenlayout(nmp, vp,
				    nfhp->nfh_fh, nfhp->nfh_len, nfhp->nfh_fh,
				    nfhp->nfh_len, mode, op, NULL, 0, &dp,
				    cred, p);
			if (dp != NULL) {
				NFSLOCKNODE(np);
				np->n_flag &= ~NDELEGMOD;
				/*
				 * Invalidate the attribute cache, so that
				 * attributes that pre-date the issue of a
				 * delegation are not cached, since the
				 * cached attributes will remain valid while
				 * the delegation is held.
				 */
				NFSINVALATTRCACHE(np);
				NFSUNLOCKNODE(np);
				(void) nfscl_deleg(nmp->nm_mountp,
				    op->nfso_own->nfsow_clp,
				    nfhp->nfh_fh, nfhp->nfh_len, cred, p, dp);
			}
		} else {
			error = EIO;
		}
		newnfs_copyincred(cred, &op->nfso_cred);
	    } else if (ret == NFSCLOPEN_SETCRED)
		/*
		 * This is a new local open on a delegation. It needs
		 * to have credentials so that an open can be done
		 * against the server during recovery.
		 */
		newnfs_copyincred(cred, &op->nfso_cred);

	    /*
	     * nfso_opencnt is the count of how many VOP_OPEN()s have
	     * been done on this Open successfully and a VOP_CLOSE()
	     * is expected for each of these.
	     * If error is non-zero, don't increment it, since the Open
	     * hasn't succeeded yet.
	     */
	    if (!error) {
		op->nfso_opencnt++;
		if (NFSHASNFSV4N(nmp) && NFSHASONEOPENOWN(nmp)) {
		    NFSLOCKNODE(np);
		    np->n_openstateid = op;
		    NFSUNLOCKNODE(np);
		}
	    }
	    nfscl_openrelease(nmp, op, error, newone);
	    if (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
		error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		error == NFSERR_BADSESSION) {
		(void) nfs_catnap(PZERO, error, "nfs_open");
	    } else if ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID)
		&& clidrev != 0) {
		expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		retrycnt++;
	    }
	} while (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    error == NFSERR_BADSESSION ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * the actual open rpc
 */
int
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
	struct nfsclsession *tsep;

	dp = *dpp;
	*dpp = NULL;
	nfscl_reqstart(nd, NFSPROC_OPEN, nmp, nfhp, fhlen, NULL, NULL, 0, 0,
	    cred);
	NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(op->nfso_own->nfsow_seqid);
	*tl++ = txdr_unsigned(mode & (NFSV4OPEN_ACCESSBOTH |
	    NFSV4OPEN_WANTDELEGMASK));
	*tl++ = txdr_unsigned((mode >> NFSLCK_SHIFT) & NFSV4OPEN_DENYBOTH);
	tsep = nfsmnt_mdssession(nmp);
	*tl++ = tsep->nfsess_clientid.lval[0];
	*tl = tsep->nfsess_clientid.lval[1];
	(void) nfsm_strtom(nd, op->nfso_own->nfsow_owner, NFSV4CL_LOCKNAMELEN);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OPEN_NOCREATE);
	if (reclaim) {
		*tl = txdr_unsigned(NFSV4OPEN_CLAIMPREVIOUS);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(delegtype);
	} else {
		if (dp != NULL) {
			if (NFSHASNFSV4N(nmp))
				*tl = txdr_unsigned(
				    NFSV4OPEN_CLAIMDELEGATECURFH);
			else
				*tl = txdr_unsigned(NFSV4OPEN_CLAIMDELEGATECUR);
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
			if (NFSHASNFSV4N(nmp))
				*tl++ = 0;
			else
				*tl++ = dp->nfsdl_stateid.seqid;
			*tl++ = dp->nfsdl_stateid.other[0];
			*tl++ = dp->nfsdl_stateid.other[1];
			*tl = dp->nfsdl_stateid.other[2];
			if (!NFSHASNFSV4N(nmp))
				(void)nfsm_strtom(nd, name, namelen);
		} else if (NFSHASNFSV4N(nmp)) {
			*tl = txdr_unsigned(NFSV4OPEN_CLAIMFH);
		} else {
			*tl = txdr_unsigned(NFSV4OPEN_CLAIMNULL);
			(void)nfsm_strtom(nd, name, namelen);
		}
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
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error)
		return (error);
	NFSCL_INCRSEQID(op->nfso_own->nfsow_seqid, nd);
	if (nd->nd_repstat == 0 || (nd->nd_repstat == NFSERR_DELAY &&
	    reclaim != 0 && (nd->nd_flag & ND_NOMOREDATA) == 0)) {
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
			ndp = malloc(
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
				}
			} else {
				ndp->nfsdl_flags = NFSCLDL_READ;
			}
			if (ret)
				ndp->nfsdl_flags |= NFSCLDL_RECALL;
			error = nfsrv_dissectace(nd, &ndp->nfsdl_ace, false,
			    &ret, &acesize, p);
			if (error)
				goto nfsmout;
		} else if (deleg == NFSV4OPEN_DELEGATENONEEXT &&
		    NFSHASNFSV4N(nmp)) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			deleg = fxdr_unsigned(uint32_t, *tl);
			if (deleg == NFSV4OPEN_CONTENTION ||
			    deleg == NFSV4OPEN_RESOURCE)
				NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		} else if (deleg != NFSV4OPEN_DELEGATENONE) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		/* If the 2nd element == NFS_OK, the Getattr succeeded. */
		if (*++tl == 0) {
			KASSERT(nd->nd_repstat == 0,
			    ("nfsrpc_openrpc: Getattr repstat"));
			error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
			    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
			    NULL, NULL, NULL, p, cred);
			if (error)
				goto nfsmout;
		}
		if (ndp != NULL) {
			if (reclaim != 0 && dp != NULL) {
				ndp->nfsdl_change = dp->nfsdl_change;
				ndp->nfsdl_modtime = dp->nfsdl_modtime;
				ndp->nfsdl_flags |= NFSCLDL_MODTIMESET;
			} else if (nd->nd_repstat == 0) {
				ndp->nfsdl_change = nfsva.na_filerev;
				ndp->nfsdl_modtime = nfsva.na_mtime;
				ndp->nfsdl_flags |= NFSCLDL_MODTIMESET;
			} else
				ndp->nfsdl_flags |= NFSCLDL_RECALL;
		}
		nd->nd_repstat = 0;
		if (!reclaim && (rflags & NFSV4OPEN_RESULTCONFIRM)) {
		    do {
			ret = nfsrpc_openconfirm(vp, newfhp, newfhlen, op,
			    cred, p);
			if (ret == NFSERR_DELAY)
			    (void) nfs_catnap(PZERO, ret, "nfs_open");
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
			    (void) nfs_catnap(PZERO, ret, "nfs_open2");
		    } while (ret == NFSERR_DELAY);
		    if (ret) {
			if (ndp != NULL) {
				free(ndp, M_NFSCLDELEG);
				ndp = NULL;
			}
			if (ret == NFSERR_STALECLIENTID ||
			    ret == NFSERR_STALEDONTRECOVER ||
			    ret == NFSERR_BADSESSION)
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
		free(ndp, M_NFSCLDELEG);
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * open downgrade rpc
 */
int
nfsrpc_opendowngrade(vnode_t vp, u_int32_t mode, struct nfsclopen *op,
    struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	NFSCL_REQSTART(nd, NFSPROC_OPENDOWNGRADE, vp, cred);
	NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID + 3 * NFSX_UNSIGNED);
	if (NFSHASNFSV4N(VFSTONFS(vp->v_mount)))
		*tl++ = 0;
	else
		*tl++ = op->nfso_stateid.seqid;
	*tl++ = op->nfso_stateid.other[0];
	*tl++ = op->nfso_stateid.other[1];
	*tl++ = op->nfso_stateid.other[2];
	*tl++ = txdr_unsigned(op->nfso_own->nfsow_seqid);
	*tl++ = txdr_unsigned(mode & NFSV4OPEN_ACCESSBOTH);
	*tl = txdr_unsigned((mode >> NFSLCK_SHIFT) & NFSV4OPEN_DENYBOTH);
	error = nfscl_request(nd, vp, p, cred);
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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * V4 Close operation.
 */
int
nfsrpc_close(vnode_t vp, int doclose, NFSPROC_T *p)
{
	struct nfsclclient *clp;
	int error;

	if (vp->v_type != VREG)
		return (0);
	if (doclose)
		error = nfscl_doclose(vp, &clp, p);
	else {
		error = nfscl_getclose(vp, &clp);
		if (error == 0)
			nfscl_clientrelease(clp);
	}
	return (error);
}

/*
 * Close the open.
 */
int
nfsrpc_doclose(struct nfsmount *nmp, struct nfsclopen *op, NFSPROC_T *p,
    bool loop_on_delayed, bool freeop)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfscllockowner *lp, *nlp;
	struct nfscllock *lop, *nlop;
	struct ucred *tcred;
	u_int64_t off = 0, len = 0;
	u_int32_t type = NFSV4LOCKT_READ;
	int error, do_unlock, trycnt;
	bool own_not_null;

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
						    (int)nd->nd_repstat,
						    "nfs_close");
				} while ((nd->nd_repstat == NFSERR_GRACE ||
				    nd->nd_repstat == NFSERR_DELAY) &&
				    error == 0 && trycnt++ < 5);
				if (op->nfso_posixlock)
					do_unlock = 0;
			}
			nfscl_freelock(lop, 0);
		}
		/*
		 * Do a ReleaseLockOwner.
		 * The lock owner name nfsl_owner may be used by other opens for
		 * other files but the lock_owner4 name that nfsrpc_rellockown()
		 * puts on the wire has the file handle for this file appended
		 * to it, so it can be done now.
		 */
		(void)nfsrpc_rellockown(nmp, lp, lp->nfsl_open->nfso_fh,
		    lp->nfsl_open->nfso_fhlen, tcred, p);
	}

	/*
	 * There could be other Opens for different files on the same
	 * OpenOwner, so locking is required.
	 */
	own_not_null = false;
	if (op->nfso_own != NULL) {
		own_not_null = true;
		NFSLOCKCLSTATE();
		nfscl_lockexcl(&op->nfso_own->nfsow_rwlock, NFSCLSTATEMUTEXPTR);
		NFSUNLOCKCLSTATE();
	}
	do {
		error = nfscl_tryclose(op, tcred, nmp, p, loop_on_delayed);
		if (error == NFSERR_GRACE)
			(void) nfs_catnap(PZERO, error, "nfs_close");
	} while (error == NFSERR_GRACE);
	if (own_not_null) {
		NFSLOCKCLSTATE();
		nfscl_lockunlock(&op->nfso_own->nfsow_rwlock);
	}

	LIST_FOREACH_SAFE(lp, &op->nfso_lock, nfsl_list, nlp)
		nfscl_freelockowner(lp, 0);
	if (freeop && error != NFSERR_DELAY)
		nfscl_freeopen(op, 0, true);
	if (own_not_null)
		NFSUNLOCKCLSTATE();
	NFSFREECRED(tcred);
	return (error);
}

/*
 * The actual Close RPC.
 */
int
nfsrpc_closerpc(struct nfsrv_descript *nd, struct nfsmount *nmp,
    struct nfsclopen *op, struct ucred *cred, NFSPROC_T *p,
    int syscred)
{
	u_int32_t *tl;
	int error;

	nfscl_reqstart(nd, NFSPROC_CLOSE, nmp, op->nfso_fh,
	    op->nfso_fhlen, NULL, NULL, 0, 0, cred);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED + NFSX_STATEID);
	if (NFSHASNFSV4N(nmp)) {
		*tl++ = 0;
		*tl++ = 0;
	} else {
		*tl++ = txdr_unsigned(op->nfso_own->nfsow_seqid);
		*tl++ = op->nfso_stateid.seqid;
	}
	*tl++ = op->nfso_stateid.other[0];
	*tl++ = op->nfso_stateid.other[1];
	*tl = op->nfso_stateid.other[2];
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error)
		return (error);
	if (!NFSHASNFSV4N(nmp))
		NFSCL_INCRSEQID(op->nfso_own->nfsow_seqid, nd);
	if (nd->nd_repstat == 0)
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
	error = nd->nd_repstat;
	if (!NFSHASNFSV4N(nmp) && error == NFSERR_STALESTATEID)
		nfscl_initiate_recovery(op->nfso_own->nfsow_clp);
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * V4 Open Confirm RPC.
 */
int
nfsrpc_openconfirm(vnode_t vp, u_int8_t *nfhp, int fhlen,
    struct nfsclopen *op, struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	int error;

	nmp = VFSTONFS(vp->v_mount);
	if (NFSHASNFSV4N(nmp))
		return (0);		/* No confirmation for NFSv4.1. */
	nfscl_reqstart(nd, NFSPROC_OPENCONFIRM, nmp, nfhp, fhlen, NULL, NULL,
	    0, 0, NULL);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED + NFSX_STATEID);
	*tl++ = op->nfso_stateid.seqid;
	*tl++ = op->nfso_stateid.other[0];
	*tl++ = op->nfso_stateid.other[1];
	*tl++ = op->nfso_stateid.other[2];
	*tl = txdr_unsigned(op->nfso_own->nfsow_seqid);
	error = nfscl_request(nd, vp, p, cred);
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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the setclientid and setclientid confirm RPCs. Called from nfs_statfs()
 * when a mount has just occurred and when the server replies NFSERR_EXPIRED.
 */
int
nfsrpc_setclient(struct nfsmount *nmp, struct nfsclclient *clp, int reclaim,
    bool *retokp, struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	u_int8_t *cp = NULL, *cp2, addr[INET6_ADDRSTRLEN + 9];
	u_short port;
	int error, isinet6 = 0, callblen;
	nfsquad_t confirm;
	static u_int32_t rev = 0;
	struct nfsclds *dsp, *odsp;
	struct in6_addr a6;
	struct nfsclsession *tsep;
	struct rpc_reconupcall recon;
	struct nfscl_reconarg *rcp;

	if (nfsboottime.tv_sec == 0)
		NFSSETBOOTTIME(nfsboottime);
	if (NFSHASNFSV4N(nmp)) {
		error = NFSERR_BADSESSION;
		odsp = dsp = NULL;
		if (retokp != NULL) {
			NFSLOCKMNT(nmp);
			odsp = TAILQ_FIRST(&nmp->nm_sess);
			NFSUNLOCKMNT(nmp);
		}
		if (odsp != NULL) {
			/*
			 * When a session already exists, first try a
			 * CreateSession with the extant ClientID.
			 */
			dsp = malloc(sizeof(struct nfsclds) +
			    odsp->nfsclds_servownlen + 1, M_NFSCLDS,
			    M_WAITOK | M_ZERO);
			dsp->nfsclds_expire = NFSD_MONOSEC + clp->nfsc_renew;
			dsp->nfsclds_servownlen = odsp->nfsclds_servownlen;
			dsp->nfsclds_sess.nfsess_clientid =
			    odsp->nfsclds_sess.nfsess_clientid;
			dsp->nfsclds_sess.nfsess_sequenceid =
			    odsp->nfsclds_sess.nfsess_sequenceid + 1;
			dsp->nfsclds_flags = odsp->nfsclds_flags;
			if (dsp->nfsclds_servownlen > 0)
				memcpy(dsp->nfsclds_serverown,
				    odsp->nfsclds_serverown,
				    dsp->nfsclds_servownlen + 1);
			mtx_init(&dsp->nfsclds_mtx, "nfsds", NULL, MTX_DEF);
			mtx_init(&dsp->nfsclds_sess.nfsess_mtx, "nfssession",
			    NULL, MTX_DEF);
			nfscl_initsessionslots(&dsp->nfsclds_sess);
			error = nfsrpc_createsession(nmp, &dsp->nfsclds_sess,
			    &nmp->nm_sockreq, NULL,
			    dsp->nfsclds_sess.nfsess_sequenceid, 1, cred, p);
			NFSCL_DEBUG(1, "create session for extant "
			    "ClientID=%d\n", error);
			if (error != 0) {
				nfscl_freenfsclds(dsp);
				dsp = NULL;
				/*
				 * If *retokp is true, return any error other
				 * than NFSERR_STALECLIENTID,
				 * NFSERR_BADSESSION or NFSERR_STALEDONTRECOVER
				 * so that nfscl_recover() will not loop.
				 */
				if (*retokp)
					return (NFSERR_IO);
			} else
				*retokp = true;
		} else if (retokp != NULL && *retokp)
			return (NFSERR_IO);
		if (error != 0) {
			/*
			 * Either there was no previous session or the
			 * CreateSession attempt failed, so...
			 * do an ExchangeID followed by the CreateSession.
			 */
			clp->nfsc_rev = rev++;
			error = nfsrpc_exchangeid(nmp, clp, &nmp->nm_sockreq, 0,
			    NFSV4EXCH_USEPNFSMDS | NFSV4EXCH_USENONPNFS, &dsp,
			    cred, p);
			NFSCL_DEBUG(1, "aft exch=%d\n", error);
			if (error == 0)
				error = nfsrpc_createsession(nmp,
				    &dsp->nfsclds_sess, &nmp->nm_sockreq, NULL,
				    dsp->nfsclds_sess.nfsess_sequenceid, 1,
				    cred, p);
			NFSCL_DEBUG(1, "aft createsess=%d\n", error);
		}
		if (error == 0) {
			/*
			 * If the session supports a backchannel, set up
			 * the BindConnectionToSession call in the krpc
			 * so that it is done on a reconnection.
			 */
			if (nfscl_enablecallb != 0 && nfs_numnfscbd > 0) {
				rcp = mem_alloc(sizeof(*rcp));
				rcp->minorvers = nmp->nm_minorvers;
				memcpy(rcp->sessionid,
				    dsp->nfsclds_sess.nfsess_sessionid,
				    NFSX_V4SESSIONID);
				recon.call = nfsrpc_bindconnsess;
				recon.arg = rcp;
				CLNT_CONTROL(nmp->nm_client, CLSET_RECONUPCALL,
				    &recon);
			}

			NFSLOCKMNT(nmp);
			/*
			 * The old sessions cannot be safely free'd
			 * here, since they may still be used by
			 * in-progress RPCs.
			 */
			tsep = NULL;
			if (TAILQ_FIRST(&nmp->nm_sess) != NULL) {
				/*
				 * Mark the old session defunct.  Needed
				 * when called from nfscl_hasexpired().
				 */
				tsep = NFSMNT_MDSSESSION(nmp);
				tsep->nfsess_defunct = 1;
			}
			TAILQ_INSERT_HEAD(&nmp->nm_sess, dsp,
			    nfsclds_list);
			/*
			 * Wake up RPCs waiting for a slot on the
			 * old session. These will then fail with
			 * NFSERR_BADSESSION and be retried with the
			 * new session by nfsv4_setsequence().
			 * Also wakeup() processes waiting for the
			 * new session.
			 */
			if (tsep != NULL)
				wakeup(&tsep->nfsess_slots);
			wakeup(&nmp->nm_sess);
			NFSUNLOCKMNT(nmp);
		} else if (dsp != NULL)
			nfscl_freenfsclds(dsp);
		if (error == 0 && reclaim == 0) {
			error = nfsrpc_reclaimcomplete(nmp, cred, p);
			NFSCL_DEBUG(1, "aft reclaimcomp=%d\n", error);
			if (error == NFSERR_COMPLETEALREADY ||
			    error == NFSERR_NOTSUPP)
				/* Ignore this error. */
				error = 0;
		}
		return (error);
	} else if (retokp != NULL && *retokp)
		return (NFSERR_IO);
	clp->nfsc_rev = rev++;

	/*
	 * Allocate a single session structure for NFSv4.0, because some of
	 * the fields are used by NFSv4.0 although it doesn't do a session.
	 */
	dsp = malloc(sizeof(struct nfsclds), M_NFSCLDS, M_WAITOK | M_ZERO);
	mtx_init(&dsp->nfsclds_mtx, "nfsds", NULL, MTX_DEF);
	mtx_init(&dsp->nfsclds_sess.nfsess_mtx, "nfssession", NULL, MTX_DEF);
	NFSLOCKMNT(nmp);
	TAILQ_INSERT_HEAD(&nmp->nm_sess, dsp, nfsclds_list);
	tsep = NFSMNT_MDSSESSION(nmp);
	NFSUNLOCKMNT(nmp);

	nfscl_reqstart(nd, NFSPROC_SETCLIENTID, nmp, NULL, 0, NULL, NULL, 0, 0,
	    NULL);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(nfsboottime.tv_sec);
	*tl = txdr_unsigned(clp->nfsc_rev);
	(void) nfsm_strtom(nd, clp->nfsc_id, clp->nfsc_idlen);

	/*
	 * set up the callback address
	 */
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFS_CALLBCKPROG);
	callblen = strlen(nfsv4_callbackaddr);
	if (callblen == 0)
		cp = nfscl_getmyip(nmp, &a6, &isinet6);
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
		NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
	    NFSM_DISSECT(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	    tsep->nfsess_clientid.lval[0] = *tl++;
	    tsep->nfsess_clientid.lval[1] = *tl++;
	    confirm.lval[0] = *tl++;
	    confirm.lval[1] = *tl;
	    m_freem(nd->nd_mrep);
	    nd->nd_mrep = NULL;

	    /*
	     * and confirm it.
	     */
	    nfscl_reqstart(nd, NFSPROC_SETCLIENTIDCFRM, nmp, NULL, 0, NULL,
		NULL, 0, 0, NULL);
	    NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	    *tl++ = tsep->nfsess_clientid.lval[0];
	    *tl++ = tsep->nfsess_clientid.lval[1];
	    *tl++ = confirm.lval[0];
	    *tl = confirm.lval[1];
	    nd->nd_flag |= ND_USEGSSNAME;
	    error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p,
		cred, NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	    if (error)
		return (error);
	    m_freem(nd->nd_mrep);
	    nd->nd_mrep = NULL;
	}
	error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs getattr call.
 */
int
nfsrpc_getattr(vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *nap)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;
	struct nfsnode *np;
	struct nfsmount *nmp;

	nmp = VFSTONFS(vp->v_mount);
	np = VTONFS(vp);
	if ((nmp->nm_privflag & NFSMNTP_FAKEROOTFH) != 0 &&
	    nmp->nm_fhsize == 0) {
		/* Attempt to get the actual root file handle. */
		error = nfsrpc_getdirpath(nmp, NFSMNT_DIRPATH(nmp), cred, p);
		if (error != 0)
			return (EACCES);
		if (np->n_fhp->nfh_len == NFSX_FHMAX + 1)
			nfscl_statfs(vp, cred, p);
	}
	NFSCL_REQSTART(nd, NFSPROC_GETATTR, vp, cred);
	if (nd->nd_flag & ND_NFSV4) {
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	if (!nd->nd_repstat)
		error = nfsm_loadattr(nd, nap);
	else
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs getattr call with non-vnode arguments.
 */
int
nfsrpc_getattrnovp(struct nfsmount *nmp, u_int8_t *fhp, int fhlen, int syscred,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, u_int64_t *xidp,
    uint32_t *leasep)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error, vers = NFS_VER2;
	nfsattrbit_t attrbits;

	nfscl_reqstart(nd, NFSPROC_GETATTR, nmp, fhp, fhlen, NULL, NULL, 0, 0,
	    cred);
	if (nd->nd_flag & ND_NFSV4) {
		vers = NFS_VER4;
		NFSGETATTR_ATTRBIT(&attrbits);
		NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_LEASETIME);
		(void) nfsrv_putattrbit(nd, &attrbits);
	} else if (nd->nd_flag & ND_NFSV3) {
		vers = NFS_VER3;
	}
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, vers, NULL, 1, xidp, NULL);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
		if ((nd->nd_flag & ND_NFSV4) != 0)
			error = nfsv4_loadattr(nd, NULL, nap, NULL, NULL, 0,
			    NULL, NULL, NULL, NULL, NULL, 0, NULL, leasep, NULL,
			    NULL, NULL);
		else
			error = nfsm_loadattr(nd, nap);
	} else
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do an nfs setattr operation.
 */
int
nfsrpc_setattr(vnode_t vp, struct vattr *vap, NFSACL_T *aclp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *rnap, int *attrflagp)
{
	int error, expireret = 0, openerr, retrycnt;
	u_int32_t clidrev = 0, mode;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
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
			    nfhp->nfh_len, mode, 0, cred, p, &stateid, &lckp);
			if (error && vp->v_type == VREG &&
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
					    mode, 0, cred, p, &stateid, &lckp);
			}
		}
		if (vap != NULL)
			error = nfsrpc_setattrrpc(vp, vap, &stateid, cred, p,
			    rnap, attrflagp);
		else
			error = nfsrpc_setaclrpc(vp, cred, p, aclp, &stateid);
		if (error == NFSERR_OPENMODE && mode == NFSV4OPEN_ACCESSREAD) {
			NFSLOCKMNT(nmp);
			nmp->nm_state |= NFSSTA_OPENMODE;
			NFSUNLOCKMNT(nmp);
		}
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (!openerr)
			(void) nfsrpc_close(vp, 0, p);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID || error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_setattr");
		} else if ((error == NFSERR_EXPIRED ||
		    ((!NFSHASINT(nmp) || !NFSHASNFSV4N(nmp)) &&
		    error == NFSERR_BADSTATEID)) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		} else if (error == NFSERR_BADSTATEID && NFSHASINT(nmp) &&
		    NFSHASNFSV4N(nmp)) {
			error = EIO;
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    error == NFSERR_BADSESSION ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4) ||
	    (error == NFSERR_OPENMODE && mode == NFSV4OPEN_ACCESSREAD &&
	     retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

static int
nfsrpc_setattrrpc(vnode_t vp, struct vattr *vap,
    nfsv4stateid_t *stateidp, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *rnap, int *attrflagp)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_SETATTR, vp, cred);
	if (nd->nd_flag & ND_NFSV4)
		nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
	vap->va_type = vp->v_type;
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
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4))
		error = nfscl_wcc_data(nd, vp, rnap, attrflagp, NULL, NULL);
	if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) == ND_NFSV4 && !error)
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
	if (!(nd->nd_flag & ND_NFSV3) && !nd->nd_repstat && !error)
		error = nfscl_postop_attr(nd, rnap, attrflagp);
	m_freem(nd->nd_mrep);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	return (error);
}

/*
 * nfs lookup rpc
 */
int
nfsrpc_lookup(vnode_t dvp, char *name, int len, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *dnap, struct nfsvattr *nap,
    struct nfsfh **nfhpp, int *attrflagp, int *dattrflagp, uint32_t openmode)
{
	uint32_t deleg, rflags, *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	struct nfsnode *np;
	struct nfsfh *nfhp;
	nfsattrbit_t attrbits;
	int error = 0, lookupp = 0, newone, ret, retop;
	uint8_t own[NFSV4CL_LOCKNAMELEN];
	struct nfsclopen *op;
	struct nfscldeleg *ndp;
	nfsv4stateid_t stateid;

	*attrflagp = 0;
	*dattrflagp = 0;
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	nmp = VFSTONFS(dvp->v_mount);
	if (len > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	if (NFSHASNFSV4(nmp) && len == 1 &&
		name[0] == '.') {
		/*
		 * Just return the current dir's fh.
		 */
		np = VTONFS(dvp);
		nfhp = malloc(sizeof (struct nfsfh) +
			np->n_fhp->nfh_len, M_NFSFH, M_WAITOK);
		nfhp->nfh_len = np->n_fhp->nfh_len;
		NFSBCOPY(np->n_fhp->nfh_fh, nfhp->nfh_fh, nfhp->nfh_len);
		*nfhpp = nfhp;
		return (0);
	}
	if (NFSHASNFSV4(nmp) && len == 2 &&
		name[0] == '.' && name[1] == '.') {
		lookupp = 1;
		openmode = 0;
		NFSCL_REQSTART(nd, NFSPROC_LOOKUPP, dvp, cred);
	} else if (openmode != 0) {
		NFSCL_REQSTART(nd, NFSPROC_LOOKUPOPEN, dvp, cred);
		nfsm_strtom(nd, name, len);
	} else {
		NFSCL_REQSTART(nd, NFSPROC_LOOKUP, dvp, cred);
		(void) nfsm_strtom(nd, name, len);
	}
	if (nd->nd_flag & ND_NFSV4) {
		NFSGETATTR_ATTRBIT(&attrbits);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(NFSV4OP_GETFH);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
		if (openmode != 0) {
			/* Test for a VREG file. */
			NFSZERO_ATTRBIT(&attrbits);
			NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_TYPE);
			NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_VERIFY);
			nfsrv_putattrbit(nd, &attrbits);
			NFSM_BUILD(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSX_UNSIGNED);
			*tl = vtonfsv34_type(VREG);

			/* Attempt the Open for VREG. */
			nfscl_filllockowner(NULL, own, F_POSIX);
			NFSM_BUILD(tl, uint32_t *, 6 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSV4OP_OPEN);
			*tl++ = 0;		/* seqid, ignored. */
			*tl++ = txdr_unsigned(openmode | NFSV4OPEN_WANTNODELEG);
			*tl++ = txdr_unsigned(NFSV4OPEN_DENYNONE);
			*tl++ = 0;		/* ClientID, ignored. */
			*tl = 0;
			nfsm_strtom(nd, own, NFSV4CL_LOCKNAMELEN);
			NFSM_BUILD(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSV4OPEN_NOCREATE);
			*tl = txdr_unsigned(NFSV4OPEN_CLAIMFH);
		}
	}
	error = nfscl_request(nd, dvp, p, cred);
	if (error)
		return (error);
	ndp = NULL;
	if (nd->nd_repstat) {
		/*
		 * When an NFSv4 Lookupp returns ENOENT, it means that
		 * the lookup is at the root of an fs, so return this dir.
		 */
		if (nd->nd_repstat == NFSERR_NOENT && lookupp) {
		    np = VTONFS(dvp);
		    nfhp = malloc(sizeof (struct nfsfh) +
			np->n_fhp->nfh_len, M_NFSFH, M_WAITOK);
		    nfhp->nfh_len = np->n_fhp->nfh_len;
		    NFSBCOPY(np->n_fhp->nfh_fh, nfhp->nfh_fh, nfhp->nfh_len);
		    *nfhpp = nfhp;
		    m_freem(nd->nd_mrep);
		    return (0);
		}
		if (nd->nd_flag & ND_NFSV3)
		    error = nfscl_postop_attr(nd, dnap, dattrflagp);
		else if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) ==
		    ND_NFSV4) {
			/* Load the directory attributes. */
			error = nfsm_loadattr(nd, dnap);
			if (error != 0)
				goto nfsmout;
			*dattrflagp = 1;
		}
		/* Check Lookup operation reply status. */
		if (openmode != 0 && (nd->nd_flag & ND_NOMOREDATA) == 0) {
			NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			if (*++tl != 0)
				goto nfsmout;
		}
		/* Look for GetFH reply. */
		if (openmode != 0 && (nd->nd_flag & ND_NOMOREDATA) == 0) {
			NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			if (*++tl != 0)
				goto nfsmout;
			error = nfsm_getfh(nd, nfhpp);
			if (error)
				goto nfsmout;
		}
		/* Look for Getattr reply. */
		if (openmode != 0 && (nd->nd_flag & ND_NOMOREDATA) == 0) {
			NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			if (*++tl != 0)
				goto nfsmout;
			error = nfsm_loadattr(nd, nap);
			if (error == 0) {
				/*
				 * We have now successfully completed the
				 * lookup, so set nd_repstat to 0.
				 */
				nd->nd_repstat = 0;
				*attrflagp = 1;
			}
		}
		goto nfsmout;
	}
	if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) == ND_NFSV4) {
		/* Load the directory attributes. */
		error = nfsm_loadattr(nd, dnap);
		if (error != 0)
			goto nfsmout;
		*dattrflagp = 1;
		/* Skip over the Lookup and GetFH operation status values. */
		NFSM_DISSECT(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	}
	error = nfsm_getfh(nd, nfhpp);
	if (error)
		goto nfsmout;

	error = nfscl_postop_attr(nd, nap, attrflagp);
	if (openmode != 0 && error == 0) {
		NFSM_DISSECT(tl, uint32_t *, NFSX_STATEID +
		    10 * NFSX_UNSIGNED);
		tl += 4;	/* Skip over Verify+Open status. */
		stateid.seqid = *tl++;
		stateid.other[0] = *tl++;
		stateid.other[1] = *tl++;
		stateid.other[2] = *tl;
		rflags = fxdr_unsigned(uint32_t, *(tl + 6));
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		if (error != 0)
			goto nfsmout;
		NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		deleg = fxdr_unsigned(uint32_t, *tl);
		if (deleg == NFSV4OPEN_DELEGATEREAD ||
		    deleg == NFSV4OPEN_DELEGATEWRITE) {
			/*
			 * Just need to fill in the fields used by
			 * nfscl_trydelegreturn().
			 * Mark the mount point as acquiring
			 * delegations, so NFSPROC_LOOKUPOPEN will
			 * no longer be done.
			 */
			NFSLOCKMNT(nmp);
			nmp->nm_privflag |= NFSMNTP_DELEGISSUED;
			NFSUNLOCKMNT(nmp);
			ndp = malloc(sizeof(struct nfscldeleg) +
			    (*nfhpp)->nfh_len, M_NFSCLDELEG, M_WAITOK);
			ndp->nfsdl_fhlen = (*nfhpp)->nfh_len;
			NFSBCOPY((*nfhpp)->nfh_fh, ndp->nfsdl_fh,
			    ndp->nfsdl_fhlen);
			newnfs_copyincred(cred, &ndp->nfsdl_cred);
			NFSM_DISSECT(tl, uint32_t *, NFSX_STATEID);
			ndp->nfsdl_stateid.seqid = *tl++;
			ndp->nfsdl_stateid.other[0] = *tl++;
			ndp->nfsdl_stateid.other[1] = *tl++;
			ndp->nfsdl_stateid.other[2] = *tl++;
		} else if (deleg == NFSV4OPEN_DELEGATENONEEXT &&
		    NFSHASNFSV4N(nmp)) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			deleg = fxdr_unsigned(uint32_t, *tl);
			if (deleg == NFSV4OPEN_CONTENTION ||
			    deleg == NFSV4OPEN_RESOURCE)
				NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		} else if (deleg != NFSV4OPEN_DELEGATENONE) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		ret = nfscl_open(dvp, (*nfhpp)->nfh_fh, (*nfhpp)->nfh_len,
		    openmode, 0, cred, p, NULL, &op, &newone, &retop, 1, true);
		if (ret != 0)
			goto nfsmout;
		if (newone != 0) {
			op->nfso_stateid.seqid = stateid.seqid;
			op->nfso_stateid.other[0] = stateid.other[0];
			op->nfso_stateid.other[1] = stateid.other[1];
			op->nfso_stateid.other[2] = stateid.other[2];
			op->nfso_mode = openmode;
		} else {
			op->nfso_stateid.seqid = stateid.seqid;
			if (retop == NFSCLOPEN_DOOPEN)
				op->nfso_mode |= openmode;
		}
		if ((rflags & NFSV4OPEN_LOCKTYPEPOSIX) != 0 ||
		    nfscl_assumeposixlocks)
			op->nfso_posixlock = 1;
		else
			op->nfso_posixlock = 0;
		nfscl_openrelease(nmp, op, 0, 0);
		if (ndp != NULL) {
			/*
			 * Since we do not have the vnode, we
			 * cannot invalidate cached attributes.
			 * Just return the delegation.
			 */
			nfscl_trydelegreturn(ndp, cred, nmp, p);
		}
	}
	if ((nd->nd_flag & ND_NFSV3) && !error)
		error = nfscl_postop_attr(nd, dnap, dattrflagp);
nfsmout:
	m_freem(nd->nd_mrep);
	if (!error && nd->nd_repstat)
		error = nd->nd_repstat;
	free(ndp, M_NFSCLDELEG);
	return (error);
}

/*
 * Do a readlink rpc.
 */
int
nfsrpc_readlink(vnode_t vp, struct uio *uiop, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsnode *np = VTONFS(vp);
	nfsattrbit_t attrbits;
	int error, len, cangetattr = 1;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_READLINK, vp, cred);
	if (nd->nd_flag & ND_NFSV4) {
		/*
		 * And do a Getattr op.
		 */
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSGETATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV3)
		error = nfscl_postop_attr(nd, nap, attrflagp);
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
			error = nfscl_postop_attr(nd, nap, attrflagp);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Read operation.
 */
int
nfsrpc_read(vnode_t vp, struct uio *uiop, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp)
{
	int error, expireret = 0, retrycnt;
	u_int32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
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
		newcred = NFSNEWCRED(cred);
	}
	retrycnt = 0;
	do {
		lckp = NULL;
		if (NFSHASNFSV4(nmp))
			(void)nfscl_getstateid(vp, nfhp->nfh_fh, nfhp->nfh_len,
			    NFSV4OPEN_ACCESSREAD, 0, newcred, p, &stateid,
			    &lckp);
		error = nfsrpc_readrpc(vp, uiop, newcred, &stateid, p, nap,
		    attrflagp);
		if (error == NFSERR_OPENMODE) {
			NFSLOCKMNT(nmp);
			nmp->nm_state |= NFSSTA_OPENMODE;
			NFSUNLOCKMNT(nmp);
		}
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID || error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_read");
		} else if ((error == NFSERR_EXPIRED ||
		    ((!NFSHASINT(nmp) || !NFSHASNFSV4N(nmp)) &&
		    error == NFSERR_BADSTATEID)) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		} else if (error == NFSERR_BADSTATEID && NFSHASINT(nmp) &&
		    NFSHASNFSV4N(nmp)) {
			error = EIO;
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    error == NFSERR_BADSESSION ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4) ||
	    (error == NFSERR_OPENMODE && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	if (NFSHASNFSV4(nmp))
		NFSFREECRED(newcred);
	return (error);
}

/*
 * The actual read RPC.
 */
static int
nfsrpc_readrpc(vnode_t vp, struct uio *uiop, struct ucred *cred,
    nfsv4stateid_t *stateidp, NFSPROC_T *p, struct nfsvattr *nap,
    int *attrflagp)
{
	u_int32_t *tl;
	int error = 0, len, retlen, tsiz, eof = 0;
	struct nfsrv_descript nfsd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsrv_descript *nd = &nfsd;
	int rsize;
	off_t tmp_off;

	*attrflagp = 0;
	tsiz = uiop->uio_resid;
	tmp_off = uiop->uio_offset + tsiz;
	NFSLOCKMNT(nmp);
	if (tmp_off > nmp->nm_maxfilesize || tmp_off < uiop->uio_offset) {
		NFSUNLOCKMNT(nmp);
		return (EFBIG);
	}
	rsize = nmp->nm_rsize;
	NFSUNLOCKMNT(nmp);
	nd->nd_mrep = NULL;
	while (tsiz > 0) {
		*attrflagp = 0;
		len = (tsiz > rsize) ? rsize : tsiz;
		NFSCL_REQSTART(nd, NFSPROC_READ, vp, cred);
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
		error = nfscl_request(nd, vp, p, cred);
		if (error)
			return (error);
		if (nd->nd_flag & ND_NFSV3) {
			error = nfscl_postop_attr(nd, nap, attrflagp);
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
		NFSM_STRSIZ(retlen, len);
		error = nfsm_mbufuio(nd, uiop, retlen);
		if (error)
			goto nfsmout;
		m_freem(nd->nd_mrep);
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
		m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs write operation
 * When called_from_strategy != 0, it should return EIO for an error that
 * indicates recovery is in progress, so that the buffer will be left
 * dirty and be written back to the server later. If it loops around,
 * the recovery thread could get stuck waiting for the buffer and recovery
 * will then deadlock.
 */
int
nfsrpc_write(vnode_t vp, struct uio *uiop, int *iomode, int *must_commit,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    int called_from_strategy, int ioflag)
{
	int error, expireret = 0, retrycnt, nostateid;
	u_int32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *np = VTONFS(vp);
	struct ucred *newcred;
	struct nfsfh *nfhp = NULL;
	nfsv4stateid_t stateid;
	void *lckp;

	KASSERT(*must_commit >= 0 && *must_commit <= 2,
	    ("nfsrpc_write: must_commit out of range=%d", *must_commit));
	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	newcred = cred;
	if (NFSHASNFSV4(nmp)) {
		newcred = NFSNEWCRED(cred);
		nfhp = np->n_fhp;
	}
	retrycnt = 0;
	do {
		lckp = NULL;
		nostateid = 0;
		if (NFSHASNFSV4(nmp)) {
			(void)nfscl_getstateid(vp, nfhp->nfh_fh, nfhp->nfh_len,
			    NFSV4OPEN_ACCESSWRITE, 0, newcred, p, &stateid,
			    &lckp);
			if (stateid.other[0] == 0 && stateid.other[1] == 0 &&
			    stateid.other[2] == 0) {
				nostateid = 1;
				NFSCL_DEBUG(1, "stateid0 in write\n");
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
			error = nfsrpc_writerpc(vp, uiop, iomode, must_commit,
			    newcred, &stateid, p, nap, attrflagp, ioflag);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID || error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_write");
		} else if ((error == NFSERR_EXPIRED ||
		    ((!NFSHASINT(nmp) || !NFSHASNFSV4N(nmp)) &&
		    error == NFSERR_BADSTATEID)) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		} else if (error == NFSERR_BADSTATEID && NFSHASINT(nmp) &&
		    NFSHASNFSV4N(nmp)) {
			error = EIO;
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_DELAY ||
	    ((error == NFSERR_STALESTATEID || error == NFSERR_BADSESSION ||
	      error == NFSERR_STALEDONTRECOVER) && called_from_strategy == 0) ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error != 0 && (retrycnt >= 4 ||
	    ((error == NFSERR_STALESTATEID || error == NFSERR_BADSESSION ||
	      error == NFSERR_STALEDONTRECOVER) && called_from_strategy != 0)))
		error = EIO;
	if (NFSHASNFSV4(nmp))
		NFSFREECRED(newcred);
	return (error);
}

/*
 * The actual write RPC.
 */
static int
nfsrpc_writerpc(vnode_t vp, struct uio *uiop, int *iomode,
    int *must_commit, struct ucred *cred, nfsv4stateid_t *stateidp,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp, int ioflag)
{
	u_int32_t *tl;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *np = VTONFS(vp);
	int error = 0, len, rlen, commit, committed = NFSWRITE_FILESYNC;
	int wccflag = 0;
	int32_t backup;
	struct nfsrv_descript *nd;
	nfsattrbit_t attrbits;
	uint64_t tmp_off;
	ssize_t tsiz, wsize;
	bool do_append;

	KASSERT(uiop->uio_iovcnt == 1, ("nfs: writerpc iovcnt > 1"));
	*attrflagp = 0;
	tsiz = uiop->uio_resid;
	tmp_off = uiop->uio_offset + tsiz;
	NFSLOCKMNT(nmp);
	if (tmp_off > nmp->nm_maxfilesize || tmp_off < uiop->uio_offset) {
		NFSUNLOCKMNT(nmp);
		return (EFBIG);
	}
	wsize = nmp->nm_wsize;
	do_append = false;
	if ((ioflag & IO_APPEND) != 0 && NFSHASNFSV4(nmp) && !NFSHASPNFS(nmp))
		do_append = true;
	NFSUNLOCKMNT(nmp);
	nd = malloc(sizeof(*nd), M_TEMP, M_WAITOK);
	nd->nd_mrep = NULL;	/* NFSv2 sometimes does a write with */
	nd->nd_repstat = 0;	/* uio_resid == 0, so the while is not done */
	while (tsiz > 0) {
		*attrflagp = 0;
		len = (tsiz > wsize) ? wsize : tsiz;
		if (do_append)
			NFSCL_REQSTART(nd, NFSPROC_APPENDWRITE, vp, cred);
		else
			NFSCL_REQSTART(nd, NFSPROC_WRITE, vp, cred);
		if (nd->nd_flag & ND_NFSV4) {
			if (do_append) {
				NFSZERO_ATTRBIT(&attrbits);
				NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_SIZE);
				nfsrv_putattrbit(nd, &attrbits);
				NFSM_BUILD(tl, uint32_t *, 2 * NFSX_UNSIGNED +
				    NFSX_HYPER);
				*tl++ = txdr_unsigned(NFSX_HYPER);
				txdr_hyper(uiop->uio_offset, tl); tl += 2;
				*tl = txdr_unsigned(NFSV4OP_WRITE);
			}
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
		error = nfsm_uiombuf(nd, uiop, len);
		if (error != 0) {
			m_freem(nd->nd_mreq);
			free(nd, M_TEMP);
			return (error);
		}
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
		 * As such, we get the rest of the attributes, but not
		 * Owner or Owner_group.
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
		error = nfscl_request(nd, vp, p, cred);
		if (error) {
			free(nd, M_TEMP);
			return (error);
		}
		if (nd->nd_repstat) {
			/*
			 * In case the rpc gets retried, roll
			 * the uio fields changed by nfsm_uiombuf()
			 * back.
			 */
			uiop->uio_offset -= len;
			uiop->uio_resid += len;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base - len;
			uiop->uio_iov->iov_len += len;
		}
		if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
			error = nfscl_wcc_data(nd, vp, nap, attrflagp,
			    &wccflag, &tmp_off);
			if (error)
				goto nfsmout;
		}
		if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) ==
		    (ND_NFSV4 | ND_NOMOREDATA) &&
		    nd->nd_repstat == NFSERR_NOTSAME && do_append) {
			/*
			 * Verify of the file's size failed, so redo the
			 * write using the file's size as returned in
			 * the wcc attributes.
			 */
			if (tmp_off + tsiz <= nmp->nm_maxfilesize) {
				do_append = false;
				uiop->uio_offset = tmp_off;
				m_freem(nd->nd_mrep);
				nd->nd_mrep = NULL;
				continue;
			} else
				nd->nd_repstat = EFBIG;
		}
		if (!nd->nd_repstat) {
			if (do_append) {
				/* Strip off the Write reply status. */
				do_append = false;
				NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			}
			if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
				NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED
					+ NFSX_VERF);
				rlen = fxdr_unsigned(int, *tl++);
				if (rlen == 0) {
					error = NFSERR_IO;
					goto nfsmout;
				} else if (rlen < len) {
					backup = len - rlen;
					uiop->uio_iov->iov_base =
					    (char *)uiop->uio_iov->iov_base -
					    backup;
					uiop->uio_iov->iov_len += backup;
					uiop->uio_offset -= backup;
					uiop->uio_resid += backup;
					len = rlen;
				}
				commit = fxdr_unsigned(int, *tl++);

				/*
				 * Return the lowest commitment level
				 * obtained by any of the RPCs.
				 */
				if (committed == NFSWRITE_FILESYNC)
					committed = commit;
				else if (committed == NFSWRITE_DATASYNC &&
					commit == NFSWRITE_UNSTABLE)
					committed = commit;
				NFSLOCKMNT(nmp);
				if (!NFSHASWRITEVERF(nmp)) {
					NFSBCOPY((caddr_t)tl,
					    (caddr_t)&nmp->nm_verf[0],
					    NFSX_VERF);
					NFSSETWRITEVERF(nmp);
	    			} else if (NFSBCMP(tl, nmp->nm_verf,
				    NFSX_VERF) && *must_commit != 2) {
					*must_commit = 1;
					NFSBCOPY(tl, nmp->nm_verf, NFSX_VERF);
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
		NFSWRITERPC_SETTIME(wccflag, np, nap, (nd->nd_flag & ND_NFSV4));
		m_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
		tsiz -= len;
	}
nfsmout:
	if (nd->nd_mrep != NULL)
		m_freem(nd->nd_mrep);
	*iomode = committed;
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	free(nd, M_TEMP);
	return (error);
}

/*
 * Do an nfs deallocate operation.
 */
int
nfsrpc_deallocate(vnode_t vp, off_t offs, off_t len, struct nfsvattr *nap,
    int *attrflagp, struct ucred *cred, NFSPROC_T *p)
{
	int error, expireret = 0, openerr, retrycnt;
	uint32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsfh *nfhp;
	nfsv4stateid_t stateid;
	void *lckp;

	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	retrycnt = 0;
	do {
		lckp = NULL;
		openerr = 1;
		nfhp = VTONFS(vp)->n_fhp;
		error = nfscl_getstateid(vp, nfhp->nfh_fh, nfhp->nfh_len,
		    NFSV4OPEN_ACCESSWRITE, 0, cred, p, &stateid, &lckp);
		if (error != 0) {
			/*
			 * No Open stateid, so try and open the file
			 * now.
			 */
			openerr = nfsrpc_open(vp, FWRITE, cred, p);
			if (openerr == 0)
				nfscl_getstateid(vp, nfhp->nfh_fh,
				    nfhp->nfh_len, NFSV4OPEN_ACCESSWRITE, 0,
				    cred, p, &stateid, &lckp);
		}
		error = nfsrpc_deallocaterpc(vp, offs, len, &stateid, nap,
		    attrflagp, cred, p);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (openerr == 0)
			nfsrpc_close(vp, 0, p);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID || error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_deallocate");
		} else if ((error == NFSERR_EXPIRED || (!NFSHASINT(nmp) &&
		    error == NFSERR_BADSTATEID)) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		} else if (error == NFSERR_BADSTATEID && NFSHASINT(nmp)) {
			error = EIO;
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    error == NFSERR_BADSESSION ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * The actual deallocate RPC.
 */
static int
nfsrpc_deallocaterpc(vnode_t vp, off_t offs, off_t len,
    nfsv4stateid_t *stateidp, struct nfsvattr *nap, int *attrflagp,
    struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsnode *np = VTONFS(vp);
	int error, wccflag;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_DEALLOCATE, vp, cred);
	nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
	NFSM_BUILD(tl, uint32_t *, 2 * NFSX_HYPER);
	txdr_hyper(offs, tl);
	tl += 2;
	txdr_hyper(len, tl);
	NFSWRITEGETATTR_ATTRBIT(&attrbits);
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred);
	if (error != 0)
		return (error);
	wccflag = 0;
	error = nfscl_wcc_data(nd, vp, nap, attrflagp, &wccflag, NULL);
	if (error != 0)
		goto nfsmout;
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		error = nfsm_loadattr(nd, nap);
		if (error != 0)
			goto nfsmout;
		*attrflagp = NFS_LATTR_NOSHRINK;
	}
	NFSWRITERPC_SETTIME(wccflag, np, nap, 1);
nfsmout:
	m_freem(nd->nd_mrep);
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
	return (error);
}

/*
 * nfs mknod rpc
 * For NFS v2 this is a kludge. Use a create rpc but with the IFMT bits of the
 * mode set to specify the file type and the size field for rdev.
 */
int
nfsrpc_mknod(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    u_int32_t rdev, __enum_uint8(vtype) vtyp, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *dnap, struct nfsvattr *nnap, struct nfsfh **nfhpp,
    int *attrflagp, int *dattrflagp)
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
	NFSCL_REQSTART(nd, NFSPROC_MKNOD, dvp, cred);
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
		nfscl_fillsattr(nd, vap, dvp, NFSSATTR_NEWFILE, 0);
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
	error = nfscl_request(nd, dvp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV4)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
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
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	if (!error && nd->nd_repstat)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs file create call
 * Mostly just call the approriate routine. (I separated out v4, so that
 * error recovery wouldn't be as difficult.)
 */
int
nfsrpc_create(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    nfsquad_t cverf, int fmode, struct ucred *cred, NFSPROC_T *p,
    struct nfsvattr *dnap, struct nfsvattr *nnap, struct nfsfh **nfhpp,
    int *attrflagp, int *dattrflagp)
{
	int error = 0, newone, expireret = 0, retrycnt, unlocked;
	struct nfsclowner *owp;
	struct nfscldeleg *dp;
	struct nfsmount *nmp = VFSTONFS(dvp->v_mount);
	u_int32_t clidrev;

	if (NFSHASNFSV4(nmp)) {
	    retrycnt = 0;
	    do {
		dp = NULL;
		error = nfscl_open(dvp, NULL, 0, (NFSV4OPEN_ACCESSWRITE |
		    NFSV4OPEN_ACCESSREAD), 0, cred, p, &owp, NULL, &newone,
		    NULL, 1, true);
		if (error)
			return (error);
		if (nmp->nm_clp != NULL)
			clidrev = nmp->nm_clp->nfsc_clientidrev;
		else
			clidrev = 0;
		if (!NFSHASPNFS(nmp) || nfscl_enablecallb == 0 ||
		    nfs_numnfscbd == 0 || retrycnt > 0)
			error = nfsrpc_createv4(dvp, name, namelen, vap, cverf,
			  fmode, owp, &dp, cred, p, dnap, nnap, nfhpp,
			  attrflagp, dattrflagp, &unlocked);
		else
			error = nfsrpc_getcreatelayout(dvp, name, namelen, vap,
			  cverf, fmode, owp, &dp, cred, p, dnap, nnap, nfhpp,
			  attrflagp, dattrflagp, &unlocked);
		/*
		 * There is no need to invalidate cached attributes here,
		 * since new post-delegation issue attributes are always
		 * returned by nfsrpc_createv4() and these will update the
		 * attribute cache.
		 */
		if (dp != NULL)
			(void) nfscl_deleg(nmp->nm_mountp, owp->nfsow_clp,
			    (*nfhpp)->nfh_fh, (*nfhpp)->nfh_len, cred, p, dp);
		nfscl_ownerrelease(nmp, owp, error, newone, unlocked);
		if (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_open");
		} else if ((error == NFSERR_EXPIRED ||
		    error == NFSERR_BADSTATEID) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
			retrycnt++;
		}
	    } while (error == NFSERR_GRACE || error == NFSERR_STALECLIENTID ||
		error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		error == NFSERR_BADSESSION ||
		((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
		 expireret == 0 && clidrev != 0 && retrycnt < 4));
	    if (error && retrycnt >= 4)
		    error = EIO;
	} else {
		error = nfsrpc_createv23(dvp, name, namelen, vap, cverf,
		    fmode, cred, p, dnap, nnap, nfhpp, attrflagp, dattrflagp);
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
    int *attrflagp, int *dattrflagp)
{
	u_int32_t *tl;
	int error = 0;
	struct nfsrv_descript nfsd, *nd = &nfsd;

	*nfhpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_CREATE, dvp, cred);
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
	error = nfscl_request(nd, dvp, p, cred);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
		error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (error)
			goto nfsmout;
	}
	if (nd->nd_flag & ND_NFSV3)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

static int
nfsrpc_createv4(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    nfsquad_t cverf, int fmode, struct nfsclowner *owp, struct nfscldeleg **dpp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp, int *unlockedp)
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
	struct nfsmount *nmp;
	struct nfsclsession *tsep;

	nmp = VFSTONFS(dvp->v_mount);
	np = VTONFS(dvp);
	*unlockedp = 0;
	*nfhpp = NULL;
	*dpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_CREATE, dvp, cred);
	/*
	 * For V4, this is actually an Open op.
	 */
	NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(owp->nfsow_seqid);
	if (NFSHASNFSV4N(nmp)) {
		if (!NFSHASPNFS(nmp) && nfscl_enablecallb != 0 &&
		    nfs_numnfscbd > 0)
			*tl++ = txdr_unsigned(NFSV4OPEN_ACCESSWRITE |
			    NFSV4OPEN_ACCESSREAD | NFSV4OPEN_WANTWRITEDELEG);
		else
			*tl++ = txdr_unsigned(NFSV4OPEN_ACCESSWRITE |
			    NFSV4OPEN_ACCESSREAD | NFSV4OPEN_WANTNODELEG);
	} else
		*tl++ = txdr_unsigned(NFSV4OPEN_ACCESSWRITE |
		    NFSV4OPEN_ACCESSREAD);
	*tl++ = txdr_unsigned(NFSV4OPEN_DENYNONE);
	tsep = nfsmnt_mdssession(nmp);
	*tl++ = tsep->nfsess_clientid.lval[0];
	*tl = tsep->nfsess_clientid.lval[1];
	(void) nfsm_strtom(nd, owp->nfsow_owner, NFSV4CL_LOCKNAMELEN);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OPEN_CREATE);
	if (fmode & O_EXCL) {
		if (NFSHASNFSV4N(nmp)) {
			if (NFSHASSESSPERSIST(nmp)) {
				/* Use GUARDED for persistent sessions. */
				*tl = txdr_unsigned(NFSCREATE_GUARDED);
				nfscl_fillsattr(nd, vap, dvp, NFSSATTR_NEWFILE,
				    0);
			} else {
				/* Otherwise, use EXCLUSIVE4_1. */
				*tl = txdr_unsigned(NFSCREATE_EXCLUSIVE41);
				NFSM_BUILD(tl, u_int32_t *, NFSX_VERF);
				*tl++ = cverf.lval[0];
				*tl = cverf.lval[1];
				nfscl_fillsattr(nd, vap, dvp, NFSSATTR_NEWFILE,
				    0);
			}
		} else {
			/* NFSv4.0 */
			*tl = txdr_unsigned(NFSCREATE_EXCLUSIVE);
			NFSM_BUILD(tl, u_int32_t *, NFSX_VERF);
			*tl++ = cverf.lval[0];
			*tl = cverf.lval[1];
		}
	} else {
		*tl = txdr_unsigned(NFSCREATE_UNCHECKED);
		nfscl_fillsattr(nd, vap, dvp, NFSSATTR_NEWFILE, 0);
	}
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OPEN_CLAIMNULL);
	(void) nfsm_strtom(nd, name, namelen);
	/* Get the new file's handle and attributes. */
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OP_GETFH);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	(void) nfsrv_putattrbit(nd, &attrbits);
	/* Get the directory's post-op attributes. */
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_PUTFH);
	(void)nfsm_fhtom(nmp, nd, np->n_fhp->nfh_fh, np->n_fhp->nfh_len, 0);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	(void) nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, dvp, p, cred);
	if (error)
		return (error);
	NFSCL_INCRSEQID(owp->nfsow_seqid, nd);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
		    6 * NFSX_UNSIGNED);
		stateid.seqid = *tl++;
		stateid.other[0] = *tl++;
		stateid.other[1] = *tl++;
		stateid.other[2] = *tl;
		rflags = fxdr_unsigned(u_int32_t, *(tl + 6));
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		if (error)
			goto nfsmout;
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		deleg = fxdr_unsigned(int, *tl);
		if (deleg == NFSV4OPEN_DELEGATEREAD ||
		    deleg == NFSV4OPEN_DELEGATEWRITE) {
			if (!(owp->nfsow_clp->nfsc_flags &
			      NFSCLFLAGS_FIRSTDELEG))
				owp->nfsow_clp->nfsc_flags |=
				  (NFSCLFLAGS_FIRSTDELEG | NFSCLFLAGS_GOTDELEG);
			dp = malloc(
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
				}
			} else {
				dp->nfsdl_flags = NFSCLDL_READ;
			}
			if (ret)
				dp->nfsdl_flags |= NFSCLDL_RECALL;
			error = nfsrv_dissectace(nd, &dp->nfsdl_ace, false,
			    &ret, &acesize, p);
			if (error)
				goto nfsmout;
		} else if (deleg == NFSV4OPEN_DELEGATENONEEXT &&
		    NFSHASNFSV4N(nmp)) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			deleg = fxdr_unsigned(uint32_t, *tl);
			if (deleg == NFSV4OPEN_CONTENTION ||
			    deleg == NFSV4OPEN_RESOURCE)
				NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		} else if (deleg != NFSV4OPEN_DELEGATENONE) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (error)
			goto nfsmout;
		/* Get rid of the PutFH and Getattr status values. */
		NFSM_DISSECT(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		/* Load the directory attributes. */
		error = nfsm_loadattr(nd, dnap);
		if (error)
			goto nfsmout;
		*dattrflagp = 1;
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
		    cred, p, NULL, &op, &newone, NULL, 0, false);
		if (error)
			goto nfsmout;
		op->nfso_stateid = stateid;
		newnfs_copyincred(cred, &op->nfso_cred);
		if ((rflags & NFSV4OPEN_RESULTCONFIRM)) {
		    do {
			ret = nfsrpc_openconfirm(dvp, nfhp->nfh_fh,
			    nfhp->nfh_len, op, cred, p);
			if (ret == NFSERR_DELAY)
			    (void) nfs_catnap(PZERO, ret, "nfs_create");
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
		    KASSERT(!NFSHASNFSV4N(nmp),
			("nfsrpc_createv4: result confirm"));
		    do {
			ret = nfsrpc_openrpc(VFSTONFS(dvp->v_mount), dvp,
			    np->n_fhp->nfh_fh, np->n_fhp->nfh_len,
			    nfhp->nfh_fh, nfhp->nfh_len,
			    (NFSV4OPEN_ACCESSWRITE | NFSV4OPEN_ACCESSREAD), op,
			    name, namelen, &dp, 0, 0x0, cred, p, 0, 1);
			if (ret == NFSERR_DELAY)
			    (void) nfs_catnap(PZERO, ret, "nfs_crt2");
		    } while (ret == NFSERR_DELAY);
		    if (ret) {
			if (dp != NULL) {
				free(dp, M_NFSCLDELEG);
				dp = NULL;
			}
			if (ret == NFSERR_STALECLIENTID ||
			    ret == NFSERR_STALEDONTRECOVER ||
			    ret == NFSERR_BADSESSION)
				error = ret;
		    }
		}
		nfscl_openrelease(nmp, op, error, newone);
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
		free(dp, M_NFSCLDELEG);
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Nfs remove rpc
 */
int
nfsrpc_remove(vnode_t dvp, char *name, int namelen, vnode_t vp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap, int *dattrflagp)
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
	nmp = VFSTONFS(dvp->v_mount);
tryagain:
	if (NFSHASNFSV4(nmp) && ret == 0) {
		ret = nfscl_removedeleg(vp, p, &dstateid);
		if (ret == 1) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGREMOVE, vp, cred);
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID +
			    NFSX_UNSIGNED);
			if (NFSHASNFSV4N(nmp))
				*tl++ = 0;
			else
				*tl++ = dstateid.seqid;
			*tl++ = dstateid.other[0];
			*tl++ = dstateid.other[1];
			*tl++ = dstateid.other[2];
			*tl = txdr_unsigned(NFSV4OP_PUTFH);
			np = VTONFS(dvp);
			(void)nfsm_fhtom(nmp, nd, np->n_fhp->nfh_fh,
			    np->n_fhp->nfh_len, 0);
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_REMOVE);
		}
	} else {
		ret = 0;
	}
	if (ret == 0)
		NFSCL_REQSTART(nd, NFSPROC_REMOVE, dvp, cred);
	(void) nfsm_strtom(nd, name, namelen);
	error = nfscl_request(nd, dvp, p, cred);
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
			m_freem(nd->nd_mrep);
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
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do an nfs rename rpc.
 */
int
nfsrpc_rename(vnode_t fdvp, vnode_t fvp, char *fnameptr, int fnamelen,
    vnode_t tdvp, vnode_t tvp, char *tnameptr, int tnamelen, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *fnap, struct nfsvattr *tnap,
    int *fattrflagp, int *tattrflagp)
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
	nmp = VFSTONFS(fdvp->v_mount);
	if (fnamelen > NFS_MAXNAMLEN || tnamelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
tryagain:
	if (NFSHASNFSV4(nmp) && ret == 0) {
		ret = nfscl_renamedeleg(fvp, &fdstateid, &gotfd, tvp,
		    &tdstateid, &gottd, p);
		if (gotfd && gottd) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGRENAME2, fvp, cred);
		} else if (gotfd) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGRENAME1, fvp, cred);
		} else if (gottd) {
			NFSCL_REQSTART(nd, NFSPROC_RETDELEGRENAME1, tvp, cred);
		}
		if (gotfd) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
			if (NFSHASNFSV4N(nmp))
				*tl++ = 0;
			else
				*tl++ = fdstateid.seqid;
			*tl++ = fdstateid.other[0];
			*tl++ = fdstateid.other[1];
			*tl = fdstateid.other[2];
			if (gottd) {
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(NFSV4OP_PUTFH);
				np = VTONFS(tvp);
				(void)nfsm_fhtom(nmp, nd, np->n_fhp->nfh_fh,
				    np->n_fhp->nfh_len, 0);
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(NFSV4OP_DELEGRETURN);
			}
		}
		if (gottd) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
			if (NFSHASNFSV4N(nmp))
				*tl++ = 0;
			else
				*tl++ = tdstateid.seqid;
			*tl++ = tdstateid.other[0];
			*tl++ = tdstateid.other[1];
			*tl = tdstateid.other[2];
		}
		if (ret > 0) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_PUTFH);
			np = VTONFS(fdvp);
			(void)nfsm_fhtom(nmp, nd, np->n_fhp->nfh_fh,
			    np->n_fhp->nfh_len, 0);
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFSV4OP_SAVEFH);
		}
	} else {
		ret = 0;
	}
	if (ret == 0)
		NFSCL_REQSTART(nd, NFSPROC_RENAME, fdvp, cred);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		NFSWCCATTR_ATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_PUTFH);
		(void)nfsm_fhtom(nmp, nd, VTONFS(tdvp)->n_fhp->nfh_fh,
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
		(void)nfsm_fhtom(nmp, nd, VTONFS(tdvp)->n_fhp->nfh_fh,
			VTONFS(tdvp)->n_fhp->nfh_len, 0);
	(void) nfsm_strtom(nd, tnameptr, tnamelen);
	error = nfscl_request(nd, fdvp, p, cred);
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
			m_freem(nd->nd_mrep);
			goto tryagain;
		}
		for (i = 0; i < (ret * 2); i++) {
			if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) ==
			    ND_NFSV4) {
			    NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			    if (*(tl + 1)) {
				if (i == 1 && ret > 1) {
				    /*
				     * If the Delegreturn failed, try again
				     * without it. The server will Recall, as
				     * required.
				     * If ret > 1, the second iteration of this
				     * loop is the second DelegReturn result.
				     */
				    m_freem(nd->nd_mrep);
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
		error = nfscl_wcc_data(nd, fdvp, fnap, fattrflagp, NULL, NULL);
		/* and the second wcc attribute reply. */
		if ((nd->nd_flag & (ND_NFSV4 | ND_NOMOREDATA)) == ND_NFSV4 &&
		    !error) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (*(tl + 1))
				nd->nd_flag |= ND_NOMOREDATA;
		}
		if (!error)
			error = nfscl_wcc_data(nd, tdvp, tnap, tattrflagp,
			    NULL, NULL);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs hard link create rpc
 */
int
nfsrpc_link(vnode_t dvp, vnode_t vp, char *name, int namelen,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nap, int *attrflagp, int *dattrflagp)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;
	int error = 0;

	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_LINK, vp, cred);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_PUTFH);
	}
	(void)nfsm_fhtom(VFSTONFS(dvp->v_mount), nd, VTONFS(dvp)->n_fhp->nfh_fh,
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
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV3) {
		error = nfscl_postop_attr(nd, nap, attrflagp);
		if (!error)
			error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp,
			    NULL, NULL);
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
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs symbolic link create rpc
 */
int
nfsrpc_symlink(vnode_t dvp, char *name, int namelen, const char *target,
    struct vattr *vap, struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	int slen, error = 0;

	*nfhpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	nmp = VFSTONFS(dvp->v_mount);
	slen = strlen(target);
	if (slen > NFS_MAXPATHLEN || namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_SYMLINK, dvp, cred);
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
	error = nfscl_request(nd, dvp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV4)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	if ((nd->nd_flag & ND_NFSV3) && !error) {
		if (!nd->nd_repstat)
			error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (!error)
			error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp,
			    NULL, NULL);
	}
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 * Only do this if vfs.nfs.ignore_eexist is set.
	 * Never do this for NFSv4.1 or later minor versions, since sessions
	 * should guarantee "exactly once" RPC semantics.
	 */
	if (error == EEXIST && nfsignore_eexist != 0 && (!NFSHASNFSV4(nmp) ||
	    nmp->nm_minorvers == 0))
		error = 0;
	return (error);
}

/*
 * nfs make dir rpc
 */
int
nfsrpc_mkdir(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;
	int error = 0;
	struct nfsfh *fhp;
	struct nfsmount *nmp;

	*nfhpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	nmp = VFSTONFS(dvp->v_mount);
	fhp = VTONFS(dvp)->n_fhp;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_MKDIR, dvp, cred);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFDIR);
	}
	(void) nfsm_strtom(nd, name, namelen);
	nfscl_fillsattr(nd, vap, dvp, NFSSATTR_SIZENEG1 | NFSSATTR_NEWFILE, 0);
	if (nd->nd_flag & ND_NFSV4) {
		NFSGETATTR_ATTRBIT(&attrbits);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(NFSV4OP_GETFH);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_PUTFH);
		(void)nfsm_fhtom(nmp, nd, fhp->nfh_fh, fhp->nfh_len, 0);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
	}
	error = nfscl_request(nd, dvp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & ND_NFSV4)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	if (!nd->nd_repstat && !error) {
		if (nd->nd_flag & ND_NFSV4) {
			NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		}
		if (!error)
			error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
		if (error == 0 && (nd->nd_flag & ND_NFSV4) != 0) {
			/* Get rid of the PutFH and Getattr status values. */
			NFSM_DISSECT(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			/* Load the directory attributes. */
			error = nfsm_loadattr(nd, dnap);
			if (error == 0)
				*dattrflagp = 1;
		}
	}
	if ((nd->nd_flag & ND_NFSV3) && !error)
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 * Only do this if vfs.nfs.ignore_eexist is set.
	 * Never do this for NFSv4.1 or later minor versions, since sessions
	 * should guarantee "exactly once" RPC semantics.
	 */
	if (error == EEXIST && nfsignore_eexist != 0 && (!NFSHASNFSV4(nmp) ||
	    nmp->nm_minorvers == 0))
		error = 0;
	return (error);
}

/*
 * nfs remove directory call
 */
int
nfsrpc_rmdir(vnode_t dvp, char *name, int namelen, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *dnap, int *dattrflagp)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error = 0;

	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_RMDIR, dvp, cred);
	(void) nfsm_strtom(nd, name, namelen);
	error = nfscl_request(nd, dvp, p, cred);
	if (error)
		return (error);
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4))
		error = nfscl_wcc_data(nd, dvp, dnap, dattrflagp, NULL, NULL);
	if (nd->nd_repstat && !error)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * Check to make sure the file name in a Readdir reply is valid.
 */
static bool
nfscl_invalidfname(bool is_v4, char *name, int len)
{
	int i;
	char *cp;

	if (is_v4 && ((len == 1 && name[0] == '.') ||
	    (len == 2 && name[0] == '.' && name[1] == '.'))) {
		printf("Readdir NFSv4 reply has dot or dotdot in it\n");
		return (true);
	}
	cp = name;
	for (i = 0; i < len; i++, cp++) {
		if (*cp == '/' || *cp == '\0') {
			printf("Readdir reply file name had imbedded / or nul"
			    " byte\n");
			return (true);
		}
	}
	return (false);
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
 *     of libc can use them and do whatever is necessary to make things work
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
int
nfsrpc_readdir(vnode_t vp, struct uio *uiop, nfsuint64 *cookiep,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    int *eofp)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	nfsquad_t cookie, ncookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp);
	struct nfsvattr nfsva;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int reqsize, tryformoredirs = 1, readsize, eof = 0, gotmnton = 0;
	u_int64_t dotfileid, dotdotfileid = 0, fakefileno = UINT64_MAX;
	char *cp;
	nfsattrbit_t attrbits, dattrbits;
	u_int32_t rderr, *tl2 = NULL;
	size_t tresid;
	bool validentry;

	KASSERT(uiop->uio_iovcnt == 1 &&
	    (uiop->uio_resid & (DIRBLKSIZ - 1)) == 0,
	    ("nfs readdirrpc bad uio"));
	KASSERT(uiop->uio_segflg == UIO_SYSSPACE,
	    ("nfsrpc_readdir: uio userspace"));
	ncookie.lval[0] = ncookie.lval[1] = 0;
	/*
	 * There is no point in reading a lot more than uio_resid, however
	 * adding one additional DIRBLKSIZ makes sense. Since uio_resid
	 * and nm_readdirsize are both exact multiples of DIRBLKSIZ, this
	 * will never make readsize > nm_readdirsize.
	 */
	readsize = nmp->nm_readdirsize;
	if (readsize > uiop->uio_resid)
		readsize = uiop->uio_resid + DIRBLKSIZ;

	*attrflagp = 0;
	if (eofp)
		*eofp = 0;
	tresid = uiop->uio_resid;
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
			NFSCL_REQSTART(nd, NFSPROC_LOOKUPP, vp, cred);
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSV4OP_GETFH);
			*tl = txdr_unsigned(NFSV4OP_GETATTR);
			(void) nfsrv_putattrbit(nd, &attrbits);
			error = nfscl_request(nd, vp, p, cred);
			if (error)
			    return (error);
			dotfileid = 0;	/* Fake out the compiler. */
			if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
			    error = nfsm_loadattr(nd, &nfsva);
			    if (error != 0)
				goto nfsmout;
			    dotfileid = nfsva.na_fileid;
			}
			if (nd->nd_repstat == 0) {
			    NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			    len = fxdr_unsigned(int, *(tl + 4));
			    if (len > 0 && len <= NFSX_V4FHMAX)
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
			    else
				error = EPERM;
			    if (!error) {
				NFSM_DISSECT(tl, u_int32_t *, 2*NFSX_UNSIGNED);
				nfsva.na_mntonfileno = UINT64_MAX;
				error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
				    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
				    NULL, NULL, NULL, p, cred);
				if (error) {
				    dotdotfileid = dotfileid;
				} else if (gotmnton) {
				    if (nfsva.na_mntonfileno != UINT64_MAX)
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
			m_freem(nd->nd_mrep);
			if (error)
			    return (error);
			nd->nd_mrep = NULL;
			dp = (struct dirent *)uiop->uio_iov->iov_base;
			dp->d_pad0 = dp->d_pad1 = 0;
			dp->d_off = 0;
			dp->d_type = DT_DIR;
			dp->d_fileno = dotfileid;
			dp->d_namlen = 1;
			*((uint64_t *)dp->d_name) = 0;	/* Zero pad it. */
			dp->d_name[0] = '.';
			dp->d_reclen = _GENERIC_DIRSIZ(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[8];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uiop->uio_resid -= dp->d_reclen;
			uiop->uio_offset += dp->d_reclen;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base + dp->d_reclen;
			uiop->uio_iov->iov_len -= dp->d_reclen;
			dp = (struct dirent *)uiop->uio_iov->iov_base;
			dp->d_pad0 = dp->d_pad1 = 0;
			dp->d_off = 0;
			dp->d_type = DT_DIR;
			dp->d_fileno = dotdotfileid;
			dp->d_namlen = 2;
			*((uint64_t *)dp->d_name) = 0;
			dp->d_name[0] = '.';
			dp->d_name[1] = '.';
			dp->d_reclen = _GENERIC_DIRSIZ(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[8];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uiop->uio_resid -= dp->d_reclen;
			uiop->uio_offset += dp->d_reclen;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base + dp->d_reclen;
			uiop->uio_iov->iov_len -= dp->d_reclen;
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
		NFSCL_REQSTART(nd, NFSPROC_READDIR, vp, cred);
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
		error = nfscl_request(nd, vp, p, cred);
		if (error)
			return (error);
		if (!(nd->nd_flag & ND_NFSV2)) {
			if (nd->nd_flag & ND_NFSV3)
				error = nfscl_postop_attr(nd, nap, attrflagp);
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

		/* loop through the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			validentry = true;
			if (nd->nd_flag & ND_NFSV4) {
				NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
				ncookie.lval[0] = *tl++;
				ncookie.lval[1] = *tl++;
				len = fxdr_unsigned(int, *tl);
			} else if (nd->nd_flag & ND_NFSV3) {
				NFSM_DISSECT(tl, u_int32_t *, 3*NFSX_UNSIGNED);
				nfsva.na_fileid = fxdr_hyper(tl);
				tl += 2;
				len = fxdr_unsigned(int, *tl);
			} else {
				NFSM_DISSECT(tl, u_int32_t *, 2*NFSX_UNSIGNED);
				nfsva.na_fileid = fxdr_unsigned(uint64_t,
				    *tl++);
				len = fxdr_unsigned(int, *tl);
			}
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				goto nfsmout;
			}
			tlen = roundup2(len, 8);
			if (tlen == len)
				tlen += 8;  /* To ensure null termination. */
			left = DIRBLKSIZ - blksiz;
			if (_GENERIC_DIRLEN(len) + NFSX_HYPER > left) {
				NFSBZERO(uiop->uio_iov->iov_base, left);
				dp->d_reclen += left;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				uiop->uio_offset += left;
				blksiz = 0;
			}
			if (_GENERIC_DIRLEN(len) + NFSX_HYPER >
			    uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				struct iovec saviov;
				off_t savoff;
				ssize_t savresid;
				int savblksiz;

				saviov.iov_base = uiop->uio_iov->iov_base;
				saviov.iov_len = uiop->uio_iov->iov_len;
				savoff = uiop->uio_offset;
				savresid = uiop->uio_resid;
				savblksiz = blksiz;

				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_pad0 = dp->d_pad1 = 0;
				dp->d_off = 0;
				dp->d_namlen = len;
				dp->d_reclen = _GENERIC_DIRLEN(len) +
				    NFSX_HYPER;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == DIRBLKSIZ)
					blksiz = 0;
				uiop->uio_resid -= DIRHDSIZ;
				uiop->uio_offset += DIRHDSIZ;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;
				cp = uiop->uio_iov->iov_base;
				error = nfsm_mbufuio(nd, uiop, len);
				if (error)
					goto nfsmout;
				/* Check for an invalid file name. */
				if (nfscl_invalidfname(
				    (nd->nd_flag & ND_NFSV4) != 0, cp, len)) {
					/* Skip over this entry. */
					uiop->uio_iov->iov_base =
					    saviov.iov_base;
					uiop->uio_iov->iov_len =
					    saviov.iov_len;
					uiop->uio_offset = savoff;
					uiop->uio_resid = savresid;
					blksiz = savblksiz;
					validentry = false;
				} else {
					cp = uiop->uio_iov->iov_base;
					tlen -= len;
					NFSBZERO(cp, tlen);
					cp += tlen; /* points to cookie store */
					tl2 = (u_int32_t *)cp;
					uiop->uio_iov->iov_base =
					    (char *)uiop->uio_iov->iov_base +
					    tlen + NFSX_HYPER;
					uiop->uio_iov->iov_len -= tlen +
					    NFSX_HYPER;
					uiop->uio_resid -= tlen + NFSX_HYPER;
					uiop->uio_offset += (tlen + NFSX_HYPER);
				}
			} else {
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
				if (error)
					goto nfsmout;
			}
			if (nd->nd_flag & ND_NFSV4) {
				rderr = 0;
				nfsva.na_mntonfileno = UINT64_MAX;
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
			if (bigenough && validentry) {
			    if (nd->nd_flag & ND_NFSV4) {
				if (rderr) {
				    dp->d_fileno = 0;
				} else {
				    if (gotmnton) {
					if (nfsva.na_mntonfileno != UINT64_MAX)
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
				error = nfscl_postop_attr(nd, nap, attrflagp);
				if (error)
					goto nfsmout;
			}
		}
		m_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = DIRBLKSIZ - blksiz;
		NFSBZERO(uiop->uio_iov->iov_base, left);
		dp->d_reclen += left;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base +
		    left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_resid -= left;
		uiop->uio_offset += left;
	}

	/*
	 * If returning no data, assume end of file.
	 * If not bigenough, return not end of file, since you aren't
	 *    returning all the data
	 * Otherwise, return the eof flag from the server.
	 */
	if (eofp) {
		if (tresid == ((size_t)(uiop->uio_resid)))
			*eofp = 1;
		else if (!bigenough)
			*eofp = 0;
		else
			*eofp = eof;
	}

	/*
	 * Add extra empty records to any remaining DIRBLKSIZ chunks.
	 */
	while (uiop->uio_resid > 0 && uiop->uio_resid != tresid) {
		dp = (struct dirent *)uiop->uio_iov->iov_base;
		NFSBZERO(dp, DIRBLKSIZ);
		dp->d_type = DT_UNKNOWN;
		tl = (u_int32_t *)&dp->d_name[4];
		*tl++ = cookie.lval[0];
		*tl = cookie.lval[1];
		dp->d_reclen = DIRBLKSIZ;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base +
		    DIRBLKSIZ;
		uiop->uio_iov->iov_len -= DIRBLKSIZ;
		uiop->uio_resid -= DIRBLKSIZ;
		uiop->uio_offset += DIRBLKSIZ;
	}

nfsmout:
	if (nd->nd_mrep != NULL)
		m_freem(nd->nd_mrep);
	return (error);
}

/*
 * NFS V3 readdir plus RPC. Used in place of nfsrpc_readdir().
 * (Also used for NFS V4 when mount flag set.)
 * (ditto above w.r.t. multiple of DIRBLKSIZ, etc.)
 */
int
nfsrpc_readdirplus(vnode_t vp, struct uio *uiop, nfsuint64 *cookiep,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp,
    int *eofp)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	vnode_t newvp = NULLVP;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp), *np;
	struct nfsvattr nfsva;
	struct nfsfh *nfhp;
	nfsquad_t cookie, ncookie;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int attrflag, tryformoredirs = 1, eof = 0, gotmnton = 0;
	int isdotdot = 0, unlocknewvp = 0;
	u_int64_t dotfileid, dotdotfileid = 0, fakefileno = UINT64_MAX;
	u_int64_t fileno = 0;
	char *cp;
	nfsattrbit_t attrbits, dattrbits;
	size_t tresid;
	u_int32_t *tl2 = NULL, rderr;
	struct timespec dctime, ts;
	bool attr_ok, validentry;

	KASSERT(uiop->uio_iovcnt == 1 &&
	    (uiop->uio_resid & (DIRBLKSIZ - 1)) == 0,
	    ("nfs readdirplusrpc bad uio"));
	KASSERT(uiop->uio_segflg == UIO_SYSSPACE,
	    ("nfsrpc_readdirplus: uio userspace"));
	ncookie.lval[0] = ncookie.lval[1] = 0;
	timespecclear(&dctime);
	*attrflagp = 0;
	if (eofp != NULL)
		*eofp = 0;
	ndp->ni_dvp = vp;
	nd->nd_mrep = NULL;
	cookie.lval[0] = cookiep->nfsuquad[0];
	cookie.lval[1] = cookiep->nfsuquad[1];
	tresid = uiop->uio_resid;

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
			NFSCL_REQSTART(nd, NFSPROC_LOOKUPP, vp, cred);
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSV4OP_GETFH);
			*tl = txdr_unsigned(NFSV4OP_GETATTR);
			(void) nfsrv_putattrbit(nd, &attrbits);
			error = nfscl_request(nd, vp, p, cred);
			if (error)
			    return (error);
			dotfileid = 0;	/* Fake out the compiler. */
			if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
			    error = nfsm_loadattr(nd, &nfsva);
			    if (error != 0)
				goto nfsmout;
			    dctime = nfsva.na_ctime;
			    dotfileid = nfsva.na_fileid;
			}
			if (nd->nd_repstat == 0) {
			    NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			    len = fxdr_unsigned(int, *(tl + 4));
			    if (len > 0 && len <= NFSX_V4FHMAX)
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
			    else
				error = EPERM;
			    if (!error) {
				NFSM_DISSECT(tl, u_int32_t *, 2*NFSX_UNSIGNED);
				nfsva.na_mntonfileno = UINT64_MAX;
				error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
				    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
				    NULL, NULL, NULL, p, cred);
				if (error) {
				    dotdotfileid = dotfileid;
				} else if (gotmnton) {
				    if (nfsva.na_mntonfileno != UINT64_MAX)
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
			m_freem(nd->nd_mrep);
			if (error)
			    return (error);
			nd->nd_mrep = NULL;
			dp = (struct dirent *)uiop->uio_iov->iov_base;
			dp->d_pad0 = dp->d_pad1 = 0;
			dp->d_off = 0;
			dp->d_type = DT_DIR;
			dp->d_fileno = dotfileid;
			dp->d_namlen = 1;
			*((uint64_t *)dp->d_name) = 0;	/* Zero pad it. */
			dp->d_name[0] = '.';
			dp->d_reclen = _GENERIC_DIRSIZ(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[8];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uiop->uio_resid -= dp->d_reclen;
			uiop->uio_offset += dp->d_reclen;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base + dp->d_reclen;
			uiop->uio_iov->iov_len -= dp->d_reclen;
			dp = (struct dirent *)uiop->uio_iov->iov_base;
			dp->d_pad0 = dp->d_pad1 = 0;
			dp->d_off = 0;
			dp->d_type = DT_DIR;
			dp->d_fileno = dotdotfileid;
			dp->d_namlen = 2;
			*((uint64_t *)dp->d_name) = 0;
			dp->d_name[0] = '.';
			dp->d_name[1] = '.';
			dp->d_reclen = _GENERIC_DIRSIZ(dp) + NFSX_HYPER;
			/*
			 * Just make these offset cookie 0.
			 */
			tl = (u_int32_t *)&dp->d_name[8];
			*tl++ = 0;
			*tl = 0;
			blksiz += dp->d_reclen;
			uiop->uio_resid -= dp->d_reclen;
			uiop->uio_offset += dp->d_reclen;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base + dp->d_reclen;
			uiop->uio_iov->iov_len -= dp->d_reclen;
		}
		NFSREADDIRPLUS_ATTRBIT(&attrbits);
		if (gotmnton)
			NFSSETBIT_ATTRBIT(&attrbits,
			    NFSATTRBIT_MOUNTEDONFILEID);
		if (!NFSISSET_ATTRBIT(&dnp->n_vattr.na_suppattr,
		    NFSATTRBIT_TIMECREATE))
			NFSCLRBIT_ATTRBIT(&attrbits, NFSATTRBIT_TIMECREATE);
	}

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		*attrflagp = 0;
		NFSCL_REQSTART(nd, NFSPROC_READDIRPLUS, vp, cred);
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
		nanouptime(&ts);
		error = nfscl_request(nd, vp, p, cred);
		if (error)
			return (error);
		if (nd->nd_flag & ND_NFSV3)
			error = nfscl_postop_attr(nd, nap, attrflagp);
		if (nd->nd_repstat || error) {
			if (!error)
				error = nd->nd_repstat;
			goto nfsmout;
		}
		if ((nd->nd_flag & ND_NFSV3) != 0 && *attrflagp != 0)
			dctime = nap->na_ctime;
		NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		NFSLOCKNODE(dnp);
		dnp->n_cookieverf.nfsuquad[0] = *tl++;
		dnp->n_cookieverf.nfsuquad[1] = *tl++;
		NFSUNLOCKNODE(dnp);
		more_dirs = fxdr_unsigned(int, *tl);
		if (!more_dirs)
			tryformoredirs = 0;

		/* loop through the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			validentry = true;
			NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			if (nd->nd_flag & ND_NFSV4) {
				ncookie.lval[0] = *tl++;
				ncookie.lval[1] = *tl++;
			} else {
				fileno = fxdr_hyper(tl);
				tl += 2;
			}
			len = fxdr_unsigned(int, *tl);
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				goto nfsmout;
			}
			tlen = roundup2(len, 8);
			if (tlen == len)
				tlen += 8;  /* To ensure null termination. */
			left = DIRBLKSIZ - blksiz;
			if (_GENERIC_DIRLEN(len) + NFSX_HYPER > left) {
				NFSBZERO(uiop->uio_iov->iov_base, left);
				dp->d_reclen += left;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				uiop->uio_offset += left;
				blksiz = 0;
			}
			if (_GENERIC_DIRLEN(len) + NFSX_HYPER >
			    uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				struct iovec saviov;
				off_t savoff;
				ssize_t savresid;
				int savblksiz;

				saviov.iov_base = uiop->uio_iov->iov_base;
				saviov.iov_len = uiop->uio_iov->iov_len;
				savoff = uiop->uio_offset;
				savresid = uiop->uio_resid;
				savblksiz = blksiz;

				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_pad0 = dp->d_pad1 = 0;
				dp->d_off = 0;
				dp->d_namlen = len;
				dp->d_reclen = _GENERIC_DIRLEN(len) +
				    NFSX_HYPER;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == DIRBLKSIZ)
					blksiz = 0;
				uiop->uio_resid -= DIRHDSIZ;
				uiop->uio_offset += DIRHDSIZ;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;
				cnp->cn_nameptr = uiop->uio_iov->iov_base;
				cnp->cn_namelen = len;
				NFSCNHASHZERO(cnp);
				cp = uiop->uio_iov->iov_base;
				error = nfsm_mbufuio(nd, uiop, len);
				if (error)
					goto nfsmout;
				/* Check for an invalid file name. */
				if (nfscl_invalidfname(
				    (nd->nd_flag & ND_NFSV4) != 0, cp, len)) {
					/* Skip over this entry. */
					uiop->uio_iov->iov_base =
					    saviov.iov_base;
					uiop->uio_iov->iov_len =
					    saviov.iov_len;
					uiop->uio_offset = savoff;
					uiop->uio_resid = savresid;
					blksiz = savblksiz;
					validentry = false;
				} else {
					cp = uiop->uio_iov->iov_base;
					tlen -= len;
					NFSBZERO(cp, tlen);
					cp += tlen; /* points to cookie store */
					tl2 = (u_int32_t *)cp;
					if (len == 2 &&
					    cnp->cn_nameptr[0] == '.' &&
					    cnp->cn_nameptr[1] == '.')
						isdotdot = 1;
					else
						isdotdot = 0;
					uiop->uio_iov->iov_base =
					    (char *)uiop->uio_iov->iov_base +
					    tlen + NFSX_HYPER;
					uiop->uio_iov->iov_len -= tlen +
					    NFSX_HYPER;
					uiop->uio_resid -= tlen + NFSX_HYPER;
					uiop->uio_offset += (tlen + NFSX_HYPER);
				}
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
					free(nfhp, M_NFSFH);
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

			if (bigenough && validentry) {
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
				attr_ok = true;
				if (NFSRV_CMPFH(nfhp->nfh_fh, nfhp->nfh_len,
				    dnp->n_fhp->nfh_fh, dnp->n_fhp->nfh_len)) {
				    VREF(vp);
				    newvp = vp;
				    unlocknewvp = 0;
				    free(nfhp, M_NFSFH);
				    np = dnp;
				} else if (isdotdot != 0) {
				    /*
				     * Skip doing a nfscl_nget() call for "..".
				     * There's a race between acquiring the nfs
				     * node here and lookups that look for the
				     * directory being read (in the parent).
				     * It would try to get a lock on ".." here,
				     * owning the lock on the directory being
				     * read. Lookup will hold the lock on ".."
				     * and try to acquire the lock on the
				     * directory being read.
				     * If the directory is unlocked/relocked,
				     * then there is a LOR with the buflock
				     * vp is relocked.
				     */
				    free(nfhp, M_NFSFH);
				} else {
				    error = nfscl_nget(vp->v_mount, vp,
				      nfhp, cnp, p, &np, LK_EXCLUSIVE);
				    if (!error) {
					newvp = NFSTOV(np);
					unlocknewvp = 1;
					/*
					 * If n_localmodtime >= time before RPC,
					 * then a file modification operation,
					 * such as VOP_SETATTR() of size, has
					 * occurred while the Lookup RPC and
					 * acquisition of the vnode happened. As
					 * such, the attributes might be stale,
					 * with possibly an incorrect size.
					 */
					NFSLOCKNODE(np);
					if (timespecisset(
					    &np->n_localmodtime) &&
					    timespeccmp(&np->n_localmodtime,
					    &ts, >=)) {
					    NFSCL_DEBUG(4, "nfsrpc_readdirplus:"
						" localmod stale attributes\n");
					    attr_ok = false;
					}
					NFSUNLOCKNODE(np);
				    }
				}
				nfhp = NULL;
				if (newvp != NULLVP) {
				    if (attr_ok)
					error = nfscl_loadattrcache(&newvp,
					    &nfsva, NULL, 0, 0);
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
				    if (cnp->cn_namelen <= NCHNAMLEN &&
					ndp->ni_dvp != ndp->ni_vp &&
					(newvp->v_type != VDIR ||
					 dctime.tv_sec != 0)) {
					cache_enter_time_flags(ndp->ni_dvp,
					    ndp->ni_vp, cnp,
					    &nfsva.na_ctime,
					    newvp->v_type != VDIR ? NULL :
					    &dctime, VFS_CACHE_DROPOLD);
				    }
				    if (unlocknewvp)
					vput(newvp);
				    else
					vrele(newvp);
				    newvp = NULLVP;
				}
			    }
			} else if (nfhp != NULL) {
			    free(nfhp, M_NFSFH);
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
				error = nfscl_postop_attr(nd, nap, attrflagp);
				if (error)
					goto nfsmout;
			}
		}
		m_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = DIRBLKSIZ - blksiz;
		NFSBZERO(uiop->uio_iov->iov_base, left);
		dp->d_reclen += left;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base +
		    left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_resid -= left;
		uiop->uio_offset += left;
	}

	/*
	 * If returning no data, assume end of file.
	 * If not bigenough, return not end of file, since you aren't
	 *    returning all the data
	 * Otherwise, return the eof flag from the server.
	 */
	if (eofp != NULL) {
		if (tresid == uiop->uio_resid)
			*eofp = 1;
		else if (!bigenough)
			*eofp = 0;
		else
			*eofp = eof;
	}

	/*
	 * Add extra empty records to any remaining DIRBLKSIZ chunks.
	 */
	while (uiop->uio_resid > 0 && uiop->uio_resid != tresid) {
		dp = (struct dirent *)uiop->uio_iov->iov_base;
		NFSBZERO(dp, DIRBLKSIZ);
		dp->d_type = DT_UNKNOWN;
		tl = (u_int32_t *)&dp->d_name[4];
		*tl++ = cookie.lval[0];
		*tl = cookie.lval[1];
		dp->d_reclen = DIRBLKSIZ;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base +
		    DIRBLKSIZ;
		uiop->uio_iov->iov_len -= DIRBLKSIZ;
		uiop->uio_resid -= DIRBLKSIZ;
		uiop->uio_offset += DIRBLKSIZ;
	}

nfsmout:
	if (nd->nd_mrep != NULL)
		m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Nfs commit rpc
 */
int
nfsrpc_commit(vnode_t vp, u_quad_t offset, int cnt, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t attrbits;
	int error;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_COMMIT, vp, cred);
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
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	error = nfscl_wcc_data(nd, vp, nap, attrflagp, NULL, NULL);
	if (!error && !nd->nd_repstat) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_VERF);
		NFSLOCKMNT(nmp);
		if (NFSBCMP(nmp->nm_verf, tl, NFSX_VERF)) {
			NFSBCOPY(tl, nmp->nm_verf, NFSX_VERF);
			nd->nd_repstat = NFSERR_STALEWRITEVERF;
		}
		NFSUNLOCKMNT(nmp);
		if (nd->nd_flag & ND_NFSV4)
			error = nfscl_postop_attr(nd, nap, attrflagp);
	}
nfsmout:
	if (!error && nd->nd_repstat)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * NFS byte range lock rpc.
 * (Mostly just calls one of the three lower level RPC routines.)
 */
int
nfsrpc_advlock(vnode_t vp, off_t size, int op, struct flock *fl,
    int reclaim, struct ucred *cred, NFSPROC_T *p, void *id, int flags)
{
	struct nfscllockowner *lp;
	struct nfsclclient *clp;
	struct nfsfh *nfhp;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
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
	}
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
		error = nfscl_getcl(vp->v_mount, cred, p, false, true, &clp);
		if (error)
			return (error);
		error = nfscl_lockt(vp, clp, off, len, fl, p, id, flags);
		if (!error) {
			clidrev = clp->nfsc_clientidrev;
			error = nfsrpc_lockt(nd, vp, clp, off, len, fl, cred,
			    p, id, flags);
		} else if (error == -1) {
			error = 0;
		}
		nfscl_clientrelease(clp);
	    } else if (op == F_UNLCK && fl->l_type == F_UNLCK) {
		/*
		 * We must loop around for all lockowner cases.
		 */
		callcnt = 0;
		error = nfscl_getcl(vp->v_mount, cred, p, false, true, &clp);
		if (error)
			return (error);
		do {
		    error = nfscl_relbytelock(vp, off, len, cred, p, callcnt,
			clp, id, flags, &lp, &dorpc);
		    /*
		     * If it returns a NULL lp, we're done.
		     */
		    if (lp == NULL) {
			if (callcnt == 0)
			    nfscl_clientrelease(clp);
			else
			    nfscl_releasealllocks(clp, vp, p, id, flags);
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
				(void) nfs_catnap(PZERO, (int)nd->nd_repstat,
				    "nfs_advlock");
			} while ((nd->nd_repstat == NFSERR_GRACE ||
			    nd->nd_repstat == NFSERR_DELAY) && error == 0);
		    }
		    callcnt++;
		} while (error == 0 && nd->nd_repstat == 0);
		nfscl_releasealllocks(clp, vp, p, id, flags);
	    } else if (op == F_SETLK) {
		error = nfscl_getbytelock(vp, off, len, fl->l_type, cred, p,
		    NULL, 0, id, flags, NULL, NULL, &lp, &newone, &donelocally);
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
		error == NFSERR_STALECLIENTID || error == NFSERR_DELAY ||
		error == NFSERR_BADSESSION) {
		(void) nfs_catnap(PZERO, error, "nfs_advlock");
	    } else if ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID)
		&& clidrev != 0) {
		expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		retrycnt++;
	    }
	} while (error == NFSERR_GRACE ||
	    error == NFSERR_STALECLIENTID || error == NFSERR_DELAY ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_STALESTATEID ||
	    error == NFSERR_BADSESSION ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * The lower level routine for the LockT case.
 */
int
nfsrpc_lockt(struct nfsrv_descript *nd, vnode_t vp,
    struct nfsclclient *clp, u_int64_t off, u_int64_t len, struct flock *fl,
    struct ucred *cred, NFSPROC_T *p, void *id, int flags)
{
	u_int32_t *tl;
	int error, type, size;
	uint8_t own[NFSV4CL_LOCKNAMELEN + NFSX_V4FHMAX];
	struct nfsnode *np;
	struct nfsmount *nmp;
	struct nfsclsession *tsep;

	nmp = VFSTONFS(vp->v_mount);
	NFSCL_REQSTART(nd, NFSPROC_LOCKT, vp, cred);
	NFSM_BUILD(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
	if (fl->l_type == F_RDLCK)
		*tl++ = txdr_unsigned(NFSV4LOCKT_READ);
	else
		*tl++ = txdr_unsigned(NFSV4LOCKT_WRITE);
	txdr_hyper(off, tl);
	tl += 2;
	txdr_hyper(len, tl);
	tl += 2;
	tsep = nfsmnt_mdssession(nmp);
	*tl++ = tsep->nfsess_clientid.lval[0];
	*tl = tsep->nfsess_clientid.lval[1];
	nfscl_filllockowner(id, own, flags);
	np = VTONFS(vp);
	NFSBCOPY(np->n_fhp->nfh_fh, &own[NFSV4CL_LOCKNAMELEN],
	    np->n_fhp->nfh_len);
	(void)nfsm_strtom(nd, own, NFSV4CL_LOCKNAMELEN + np->n_fhp->nfh_len);
	error = nfscl_request(nd, vp, p, cred);
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
	m_freem(nd->nd_mrep);
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
	    lp->nfsl_open->nfso_fhlen, NULL, NULL, 0, 0, cred);
	NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID + 6 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(type);
	*tl = txdr_unsigned(lp->nfsl_seqid);
	if (nfstest_outofseq &&
	    (arc4random() % nfstest_outofseq) == 0)
		*tl = txdr_unsigned(lp->nfsl_seqid + 1);
	tl++;
	if (NFSHASNFSV4N(nmp))
		*tl++ = 0;
	else
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
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * The actual Lock RPC.
 */
int
nfsrpc_lock(struct nfsrv_descript *nd, struct nfsmount *nmp, vnode_t vp,
    u_int8_t *nfhp, int fhlen, struct nfscllockowner *lp, int newone,
    int reclaim, u_int64_t off, u_int64_t len, short type, struct ucred *cred,
    NFSPROC_T *p, int syscred)
{
	u_int32_t *tl;
	int error, size;
	uint8_t own[NFSV4CL_LOCKNAMELEN + NFSX_V4FHMAX];
	struct nfsclsession *tsep;

	nfscl_reqstart(nd, NFSPROC_LOCK, nmp, nfhp, fhlen, NULL, NULL, 0, 0,
	    cred);
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
	    if (NFSHASNFSV4N(nmp))
		*tl++ = 0;
	    else
		*tl++ = lp->nfsl_open->nfso_stateid.seqid;
	    *tl++ = lp->nfsl_open->nfso_stateid.other[0];
	    *tl++ = lp->nfsl_open->nfso_stateid.other[1];
	    *tl++ = lp->nfsl_open->nfso_stateid.other[2];
	    *tl++ = txdr_unsigned(lp->nfsl_seqid);
	    tsep = nfsmnt_mdssession(nmp);
	    *tl++ = tsep->nfsess_clientid.lval[0];
	    *tl = tsep->nfsess_clientid.lval[1];
	    NFSBCOPY(lp->nfsl_owner, own, NFSV4CL_LOCKNAMELEN);
	    NFSBCOPY(nfhp, &own[NFSV4CL_LOCKNAMELEN], fhlen);
	    (void)nfsm_strtom(nd, own, NFSV4CL_LOCKNAMELEN + fhlen);
	} else {
	    *tl = newnfs_false;
	    NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID + NFSX_UNSIGNED);
	    if (NFSHASNFSV4N(nmp))
		*tl++ = 0;
	    else
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
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs statfs rpc
 * (always called with the vp for the mount point)
 */
int
nfsrpc_statfs(vnode_t vp, struct nfsstatfs *sbp, struct nfsfsinfo *fsp,
    uint32_t *leasep, struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap,
    int *attrflagp)
{
	u_int32_t *tl = NULL;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	nfsattrbit_t attrbits;
	int error;

	*attrflagp = 0;
	nmp = VFSTONFS(vp->v_mount);
	if (NFSHASNFSV4(nmp)) {
		/*
		 * For V4, you actually do a getattr.
		 */
		NFSCL_REQSTART(nd, NFSPROC_GETATTR, vp, cred);
		if (leasep != NULL)
			NFSROOTFS_GETATTRBIT(&attrbits);
		else
			NFSSTATFS_GETATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
		nd->nd_flag |= ND_USEGSSNAME;
		error = nfscl_request(nd, vp, p, cred);
		if (error)
			return (error);
		if (nd->nd_repstat == 0) {
			error = nfsv4_loadattr(nd, NULL, nap, NULL, NULL, 0,
			    NULL, NULL, sbp, fsp, NULL, 0, NULL, leasep, NULL,
			    p, cred);
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
		NFSCL_REQSTART(nd, NFSPROC_FSSTAT, vp, NULL);
		error = nfscl_request(nd, vp, p, cred);
		if (error)
			return (error);
		if (nd->nd_flag & ND_NFSV3) {
			error = nfscl_postop_attr(nd, nap, attrflagp);
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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs pathconf rpc
 */
int
nfsrpc_pathconf(vnode_t vp, struct nfsv3_pathconf *pc,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp;
	u_int32_t *tl;
	nfsattrbit_t attrbits;
	int error;
	struct nfsnode *np;

	*attrflagp = 0;
	nmp = VFSTONFS(vp->v_mount);
	if (NFSHASNFSV4(nmp)) {
		np = VTONFS(vp);
		if ((nmp->nm_privflag & NFSMNTP_FAKEROOTFH) != 0 &&
		    nmp->nm_fhsize == 0) {
			/* Attempt to get the actual root file handle. */
			error = nfsrpc_getdirpath(nmp, NFSMNT_DIRPATH(nmp),
			    cred, p);
			if (error != 0)
				return (EACCES);
			if (np->n_fhp->nfh_len == NFSX_FHMAX + 1)
				nfscl_statfs(vp, cred, p);
		}
		/*
		 * For V4, you actually do a getattr.
		 */
		NFSCL_REQSTART(nd, NFSPROC_GETATTR, vp, cred);
		NFSPATHCONF_GETATTRBIT(&attrbits);
		(void) nfsrv_putattrbit(nd, &attrbits);
		nd->nd_flag |= ND_USEGSSNAME;
		error = nfscl_request(nd, vp, p, cred);
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
		NFSCL_REQSTART(nd, NFSPROC_PATHCONF, vp, NULL);
		error = nfscl_request(nd, vp, p, cred);
		if (error)
			return (error);
		error = nfscl_postop_attr(nd, nap, attrflagp);
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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs version 3 fsinfo rpc call
 */
int
nfsrpc_fsinfo(vnode_t vp, struct nfsfsinfo *fsp, struct ucred *cred,
    NFSPROC_T *p, struct nfsvattr *nap, int *attrflagp)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_FSINFO, vp, NULL);
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	error = nfscl_postop_attr(nd, nap, attrflagp);
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
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Renew RPC.
 */
int
nfsrpc_renew(struct nfsclclient *clp, struct nfsclds *dsp, struct ucred *cred,
    NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfsmount *nmp;
	int error;
	struct nfssockreq *nrp;
	struct nfsclsession *tsep;

	nmp = clp->nfsc_nmp;
	if (nmp == NULL)
		return (0);
	if (dsp == NULL)
		nfscl_reqstart(nd, NFSPROC_RENEW, nmp, NULL, 0, NULL, NULL, 0,
		    0, cred);
	else
		nfscl_reqstart(nd, NFSPROC_RENEW, nmp, NULL, 0, NULL,
		    &dsp->nfsclds_sess, 0, 0, NULL);
	if (!NFSHASNFSV4N(nmp)) {
		/* NFSv4.1 just uses a Sequence Op and not a Renew. */
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		tsep = nfsmnt_mdssession(nmp);
		*tl++ = tsep->nfsess_clientid.lval[0];
		*tl = tsep->nfsess_clientid.lval[1];
	}
	nrp = NULL;
	if (dsp != NULL)
		nrp = dsp->nfsclds_sockp;
	if (nrp == NULL)
		/* If NULL, use the MDS socket. */
		nrp = &nmp->nm_sockreq;
	nd->nd_flag |= ND_USEGSSNAME;
	if (dsp == NULL)
		error = newnfs_request(nd, nmp, NULL, nrp, NULL, p, cred,
		    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	else {
		error = newnfs_request(nd, nmp, NULL, nrp, NULL, p, cred,
		    NFS_PROG, NFS_VER4, NULL, 1, NULL, &dsp->nfsclds_sess);
		if (error == ENXIO)
			nfscl_cancelreqs(dsp);
	}
	if (error)
		return (error);
	error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Releaselockowner RPC.
 */
int
nfsrpc_rellockown(struct nfsmount *nmp, struct nfscllockowner *lp,
    uint8_t *fh, int fhlen, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	u_int32_t *tl;
	int error;
	uint8_t own[NFSV4CL_LOCKNAMELEN + NFSX_V4FHMAX];
	struct nfsclsession *tsep;

	if (NFSHASNFSV4N(nmp)) {
		/* For NFSv4.1, do a FreeStateID. */
		nfscl_reqstart(nd, NFSPROC_FREESTATEID, nmp, NULL, 0, NULL,
		    NULL, 0, 0, cred);
		nfsm_stateidtom(nd, &lp->nfsl_stateid, NFSSTATEID_PUTSTATEID);
	} else {
		nfscl_reqstart(nd, NFSPROC_RELEASELCKOWN, nmp, NULL, 0, NULL,
		    NULL, 0, 0, NULL);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		tsep = nfsmnt_mdssession(nmp);
		*tl++ = tsep->nfsess_clientid.lval[0];
		*tl = tsep->nfsess_clientid.lval[1];
		NFSBCOPY(lp->nfsl_owner, own, NFSV4CL_LOCKNAMELEN);
		NFSBCOPY(fh, &own[NFSV4CL_LOCKNAMELEN], fhlen);
		(void)nfsm_strtom(nd, own, NFSV4CL_LOCKNAMELEN + fhlen);
	}
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error)
		return (error);
	error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Compound to get the mount pt FH.
 */
int
nfsrpc_getdirpath(struct nfsmount *nmp, u_char *dirpath, struct ucred *cred,
    NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	u_char *cp, *cp2, *fhp;
	int error, cnt, len, setnil;
	u_int32_t *opcntp;

	nfscl_reqstart(nd, NFSPROC_PUTROOTFH, nmp, NULL, 0, &opcntp, NULL, 0,
	    0, NULL);
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
	if (NFSHASNFSV4N(nmp))
		/* Has a Sequence Op done by nfscl_reqstart(). */
		*opcntp = txdr_unsigned(3 + cnt);
	else
		*opcntp = txdr_unsigned(2 + cnt);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETFH);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
		NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, u_int32_t *, (3 + 2 * cnt) * NFSX_UNSIGNED);
		tl += (2 + 2 * cnt);
		if ((len = fxdr_unsigned(int, *tl)) <= 0 ||
			len > NFSX_FHMAX) {
			nd->nd_repstat = NFSERR_BADXDR;
		} else {
			fhp = malloc(len + 1, M_TEMP, M_WAITOK);
			nd->nd_repstat = nfsrv_mtostr(nd, fhp, len);
			if (nd->nd_repstat == 0) {
				NFSLOCKMNT(nmp);
				if (nmp->nm_fhsize == 0) {
					NFSBCOPY(fhp, nmp->nm_fh, len);
					nmp->nm_fhsize = len;
				}
				NFSUNLOCKMNT(nmp);
			}
			free(fhp, M_TEMP);
		}
	}
	error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * This function performs the Delegreturn RPC.
 */
int
nfsrpc_delegreturn(struct nfscldeleg *dp, struct ucred *cred,
    struct nfsmount *nmp, NFSPROC_T *p, int syscred)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	int error;

	nfscl_reqstart(nd, NFSPROC_DELEGRETURN, nmp, dp->nfsdl_fh,
	    dp->nfsdl_fhlen, NULL, NULL, 0, 0, cred);
	NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
	if (NFSHASNFSV4N(nmp))
		*tl++ = 0;
	else
		*tl++ = dp->nfsdl_stateid.seqid;
	*tl++ = dp->nfsdl_stateid.other[0];
	*tl++ = dp->nfsdl_stateid.other[1];
	*tl = dp->nfsdl_stateid.other[2];
	if (syscred)
		nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error)
		return (error);
	error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs getacl call.
 */
int
nfsrpc_getacl(vnode_t vp, struct ucred *cred, NFSPROC_T *p, struct acl *aclp)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	if (nfsrv_useacl == 0 || !NFSHASNFSV4(nmp))
		return (EOPNOTSUPP);
	NFSCL_REQSTART(nd, NFSPROC_GETACL, vp, cred);
	NFSZERO_ATTRBIT(&attrbits);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_ACL);
	(void) nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	if (!nd->nd_repstat)
		error = nfsv4_loadattr(nd, vp, NULL, NULL, NULL, 0, NULL,
		    NULL, NULL, NULL, aclp, 0, NULL, NULL, NULL, p, cred);
	else
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * nfs setacl call.
 */
int
nfsrpc_setacl(vnode_t vp, struct ucred *cred, NFSPROC_T *p, struct acl *aclp)
{
	int error;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	if (nfsrv_useacl == 0 || !NFSHASNFSV4(nmp))
		return (EOPNOTSUPP);
	error = nfsrpc_setattr(vp, NULL, aclp, cred, p, NULL, NULL);
	return (error);
}

/*
 * nfs setacl call.
 */
static int
nfsrpc_setaclrpc(vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    struct acl *aclp, nfsv4stateid_t *stateidp)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;
	nfsattrbit_t attrbits;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	if (!NFSHASNFSV4(nmp))
		return (EOPNOTSUPP);
	NFSCL_REQSTART(nd, NFSPROC_SETACL, vp, cred);
	nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
	NFSZERO_ATTRBIT(&attrbits);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_ACL);
	(void) nfsv4_fillattr(nd, vp->v_mount, vp, aclp, NULL, NULL, 0,
	    &attrbits, NULL, NULL, 0, 0, 0, 0, (uint64_t)0, NULL);
	error = nfscl_request(nd, vp, p, cred);
	if (error)
		return (error);
	/* Don't care about the pre/postop attributes */
	m_freem(nd->nd_mrep);
	return (nd->nd_repstat);
}

/*
 * Do the NFSv4.1 Exchange ID.
 */
int
nfsrpc_exchangeid(struct nfsmount *nmp, struct nfsclclient *clp,
    struct nfssockreq *nrp, int minorvers, uint32_t exchflags,
    struct nfsclds **dspp, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl, v41flags;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfsclds *dsp;
	struct timespec verstime;
	int error, len;

	*dspp = NULL;
	if (minorvers == 0)
		minorvers = nmp->nm_minorvers;
	nfscl_reqstart(nd, NFSPROC_EXCHANGEID, nmp, NULL, 0, NULL, NULL,
	    NFS_VER4, minorvers, NULL);
	NFSM_BUILD(tl, uint32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(nfsboottime.tv_sec);	/* Client owner */
	*tl = txdr_unsigned(clp->nfsc_rev);
	(void) nfsm_strtom(nd, clp->nfsc_id, clp->nfsc_idlen);

	NFSM_BUILD(tl, uint32_t *, 3 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(exchflags);
	*tl++ = txdr_unsigned(NFSV4EXCH_SP4NONE);

	/* Set the implementation id4 */
	*tl = txdr_unsigned(1);
	(void) nfsm_strtom(nd, "freebsd.org", strlen("freebsd.org"));
	(void) nfsm_strtom(nd, version, strlen(version));
	NFSM_BUILD(tl, uint32_t *, NFSX_V4TIME);
	verstime.tv_sec = 1293840000;		/* Jan 1, 2011 */
	verstime.tv_nsec = 0;
	txdr_nfsv4time(&verstime, tl);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, nrp, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	NFSCL_DEBUG(1, "exchangeid err=%d reps=%d\n", error,
	    (int)nd->nd_repstat);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, 6 * NFSX_UNSIGNED + NFSX_HYPER);
		len = fxdr_unsigned(int, *(tl + 7));
		if (len < 0 || len > NFSV4_OPAQUELIMIT) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		dsp = malloc(sizeof(struct nfsclds) + len + 1, M_NFSCLDS,
		    M_WAITOK | M_ZERO);
		dsp->nfsclds_expire = NFSD_MONOSEC + clp->nfsc_renew;
		dsp->nfsclds_servownlen = len;
		dsp->nfsclds_sess.nfsess_clientid.lval[0] = *tl++;
		dsp->nfsclds_sess.nfsess_clientid.lval[1] = *tl++;
		dsp->nfsclds_sess.nfsess_sequenceid =
		    fxdr_unsigned(uint32_t, *tl++);
		v41flags = fxdr_unsigned(uint32_t, *tl);
		if ((v41flags & NFSV4EXCH_USEPNFSMDS) != 0 &&
		    NFSHASPNFSOPT(nmp)) {
			NFSCL_DEBUG(1, "set PNFS\n");
			NFSLOCKMNT(nmp);
			nmp->nm_state |= NFSSTA_PNFS;
			NFSUNLOCKMNT(nmp);
			dsp->nfsclds_flags |= NFSCLDS_MDS;
		}
		if ((v41flags & NFSV4EXCH_USEPNFSDS) != 0)
			dsp->nfsclds_flags |= NFSCLDS_DS;
		if (minorvers == NFSV42_MINORVERSION)
			dsp->nfsclds_flags |= NFSCLDS_MINORV2;
		if (len > 0)
			nd->nd_repstat = nfsrv_mtostr(nd,
			    dsp->nfsclds_serverown, len);
		if (nd->nd_repstat == 0) {
			mtx_init(&dsp->nfsclds_mtx, "nfsds", NULL, MTX_DEF);
			mtx_init(&dsp->nfsclds_sess.nfsess_mtx, "nfssession",
			    NULL, MTX_DEF);
			nfscl_initsessionslots(&dsp->nfsclds_sess);
			*dspp = dsp;
		} else
			free(dsp, M_NFSCLDS);
	}
	error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the NFSv4.1 Create Session.
 */
int
nfsrpc_createsession(struct nfsmount *nmp, struct nfsclsession *sep,
    struct nfssockreq *nrp, struct nfsclds *dsp, uint32_t sequenceid, int mds,
    struct ucred *cred, NFSPROC_T *p)
{
	uint32_t crflags, maxval, *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	int error, irdcnt, minorvers;

	/* Make sure nm_rsize, nm_wsize is set. */
	if (nmp->nm_rsize > NFS_MAXBSIZE || nmp->nm_rsize == 0)
		nmp->nm_rsize = NFS_MAXBSIZE;
	if (nmp->nm_wsize > NFS_MAXBSIZE || nmp->nm_wsize == 0)
		nmp->nm_wsize = NFS_MAXBSIZE;
	if (dsp == NULL)
		minorvers = nmp->nm_minorvers;
	else if ((dsp->nfsclds_flags & NFSCLDS_MINORV2) != 0)
		minorvers = NFSV42_MINORVERSION;
	else
		minorvers = NFSV41_MINORVERSION;
	nfscl_reqstart(nd, NFSPROC_CREATESESSION, nmp, NULL, 0, NULL, NULL,
	    NFS_VER4, minorvers, NULL);
	NFSM_BUILD(tl, uint32_t *, 4 * NFSX_UNSIGNED);
	*tl++ = sep->nfsess_clientid.lval[0];
	*tl++ = sep->nfsess_clientid.lval[1];
	*tl++ = txdr_unsigned(sequenceid);
	crflags = (NFSMNT_RDONLY(nmp->nm_mountp) ? 0 : NFSV4CRSESS_PERSIST);
	if (nfscl_enablecallb != 0 && nfs_numnfscbd > 0 && mds != 0)
		crflags |= NFSV4CRSESS_CONNBACKCHAN;
	*tl = txdr_unsigned(crflags);

	/* Fill in fore channel attributes. */
	NFSM_BUILD(tl, uint32_t *, 7 * NFSX_UNSIGNED);
	*tl++ = 0;				/* Header pad size */
	if ((nd->nd_flag & ND_NFSV42) != 0 && mds != 0 && sb_max_adj >=
	    nmp->nm_wsize && sb_max_adj >= nmp->nm_rsize) {
		/*
		 * NFSv4.2 Extended Attribute operations may want to do
		 * requests/replies that are larger than nm_rsize/nm_wsize.
		 */
		*tl++ = txdr_unsigned(sb_max_adj - NFS_MAXXDR);
		*tl++ = txdr_unsigned(sb_max_adj - NFS_MAXXDR);
	} else {
		*tl++ = txdr_unsigned(nmp->nm_wsize + NFS_MAXXDR);
		*tl++ = txdr_unsigned(nmp->nm_rsize + NFS_MAXXDR);
	}
	*tl++ = txdr_unsigned(4096);		/* Max response size cached */
	*tl++ = txdr_unsigned(20);		/* Max operations */
	*tl++ = txdr_unsigned(64);		/* Max slots */
	*tl = 0;				/* No rdma ird */

	/* Fill in back channel attributes. */
	NFSM_BUILD(tl, uint32_t *, 7 * NFSX_UNSIGNED);
	*tl++ = 0;				/* Header pad size */
	*tl++ = txdr_unsigned(10000);		/* Max request size */
	*tl++ = txdr_unsigned(10000);		/* Max response size */
	*tl++ = txdr_unsigned(4096);		/* Max response size cached */
	*tl++ = txdr_unsigned(4);		/* Max operations */
	*tl++ = txdr_unsigned(NFSV4_CBSLOTS);	/* Max slots */
	*tl = 0;				/* No rdma ird */

	NFSM_BUILD(tl, uint32_t *, 8 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFS_CALLBCKPROG);	/* Call back prog # */

	/* Allow AUTH_SYS callbacks as uid, gid == 0. */
	*tl++ = txdr_unsigned(1);		/* Auth_sys only */
	*tl++ = txdr_unsigned(AUTH_SYS);	/* AUTH_SYS type */
	*tl++ = txdr_unsigned(nfsboottime.tv_sec); /* time stamp */
	*tl++ = 0;				/* Null machine name */
	*tl++ = 0;				/* Uid == 0 */
	*tl++ = 0;				/* Gid == 0 */
	*tl = 0;				/* No additional gids */
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, nrp, NULL, p, cred, NFS_PROG,
	    NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, NFSX_V4SESSIONID +
		    2 * NFSX_UNSIGNED);
		bcopy(tl, sep->nfsess_sessionid, NFSX_V4SESSIONID);
		tl += NFSX_V4SESSIONID / NFSX_UNSIGNED;
		sep->nfsess_sequenceid = fxdr_unsigned(uint32_t, *tl++);
		crflags = fxdr_unsigned(uint32_t, *tl);
		if ((crflags & NFSV4CRSESS_PERSIST) != 0 && mds != 0) {
			NFSLOCKMNT(nmp);
			nmp->nm_state |= NFSSTA_SESSPERSIST;
			NFSUNLOCKMNT(nmp);
		}

		/* Get the fore channel slot count. */
		NFSM_DISSECT(tl, uint32_t *, 7 * NFSX_UNSIGNED);
		tl++;			/* Skip the header pad size. */

		/* Make sure nm_wsize is small enough. */
		maxval = fxdr_unsigned(uint32_t, *tl++);
		while (maxval < nmp->nm_wsize + NFS_MAXXDR) {
			if (nmp->nm_wsize > 8096)
				nmp->nm_wsize /= 2;
			else
				break;
		}
		sep->nfsess_maxreq = maxval;

		/* Make sure nm_rsize is small enough. */
		maxval = fxdr_unsigned(uint32_t, *tl++);
		while (maxval < nmp->nm_rsize + NFS_MAXXDR) {
			if (nmp->nm_rsize > 8096)
				nmp->nm_rsize /= 2;
			else
				break;
		}
		sep->nfsess_maxresp = maxval;

		sep->nfsess_maxcache = fxdr_unsigned(int, *tl++);
		tl++;
		sep->nfsess_foreslots = fxdr_unsigned(uint16_t, *tl++);
		NFSCL_DEBUG(4, "fore slots=%d\n", (int)sep->nfsess_foreslots);
		irdcnt = fxdr_unsigned(int, *tl);
		if (irdcnt < 0 || irdcnt > 1) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		if (irdcnt > 0)
			NFSM_DISSECT(tl, uint32_t *, irdcnt * NFSX_UNSIGNED);

		/* and the back channel slot count. */
		NFSM_DISSECT(tl, uint32_t *, 7 * NFSX_UNSIGNED);
		tl += 5;
		sep->nfsess_backslots = fxdr_unsigned(uint16_t, *tl);
		NFSCL_DEBUG(4, "back slots=%d\n", (int)sep->nfsess_backslots);
	}
	error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the NFSv4.1 Destroy Client.
 */
int
nfsrpc_destroyclient(struct nfsmount *nmp, struct nfsclclient *clp,
    struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	int error;
	struct nfsclsession *tsep;

	nfscl_reqstart(nd, NFSPROC_DESTROYCLIENT, nmp, NULL, 0, NULL, NULL, 0,
	    0, NULL);
	NFSM_BUILD(tl, uint32_t *, 2 * NFSX_UNSIGNED);
	tsep = nfsmnt_mdssession(nmp);
	*tl++ = tsep->nfsess_clientid.lval[0];
	*tl = tsep->nfsess_clientid.lval[1];
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the NFSv4.1 LayoutGet.
 */
static int
nfsrpc_layoutget(struct nfsmount *nmp, uint8_t *fhp, int fhlen, int iomode,
    uint64_t offset, uint64_t len, uint64_t minlen, int layouttype,
    int layoutlen, nfsv4stateid_t *stateidp, int *retonclosep,
    struct nfsclflayouthead *flhp, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	nfscl_reqstart(nd, NFSPROC_LAYOUTGET, nmp, fhp, fhlen, NULL, NULL, 0,
	    0, cred);
	nfsrv_setuplayoutget(nd, iomode, offset, len, minlen, stateidp,
	    layouttype, layoutlen, 0);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	NFSCL_DEBUG(4, "layget err=%d st=%d\n", error, nd->nd_repstat);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0)
		error = nfsrv_parselayoutget(nmp, nd, stateidp, retonclosep,
		    flhp);
	if (error == 0 && nd->nd_repstat != 0)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the NFSv4.1 Get Device Info.
 */
int
nfsrpc_getdeviceinfo(struct nfsmount *nmp, uint8_t *deviceid, int layouttype,
    uint32_t *notifybitsp, struct nfscldevinfo **ndip, struct ucred *cred,
    NFSPROC_T *p)
{
	uint32_t cnt, *tl, vers, minorvers;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct sockaddr_in sin, ssin;
	struct sockaddr_in6 sin6, ssin6;
	struct nfsclds *dsp = NULL, **dspp, **gotdspp;
	struct nfscldevinfo *ndi;
	int addrcnt = 0, bitcnt, error, gotminor, gotvers, i, isudp, j;
	int stripecnt;
	uint8_t stripeindex;
	sa_family_t af, safilled;

	ssin.sin_port = 0;		/* To shut up compiler. */
	ssin.sin_addr.s_addr = 0;	/* ditto */
	*ndip = NULL;
	ndi = NULL;
	gotdspp = NULL;
	nfscl_reqstart(nd, NFSPROC_GETDEVICEINFO, nmp, NULL, 0, NULL, NULL, 0,
	    0, cred);
	NFSM_BUILD(tl, uint32_t *, NFSX_V4DEVICEID + 3 * NFSX_UNSIGNED);
	NFSBCOPY(deviceid, tl, NFSX_V4DEVICEID);
	tl += (NFSX_V4DEVICEID / NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(layouttype);
	*tl++ = txdr_unsigned(100000);
	if (notifybitsp != NULL && *notifybitsp != 0) {
		*tl = txdr_unsigned(1);		/* One word of bits. */
		NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(*notifybitsp);
	} else
		*tl = txdr_unsigned(0);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		if (layouttype != fxdr_unsigned(int, *tl))
			printf("EEK! devinfo layout type not same!\n");
		if (layouttype == NFSLAYOUT_NFSV4_1_FILES) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			stripecnt = fxdr_unsigned(int, *tl);
			NFSCL_DEBUG(4, "stripecnt=%d\n", stripecnt);
			if (stripecnt < 1 || stripecnt > 4096) {
				printf("pNFS File layout devinfo stripecnt %d:"
				    " out of range\n", stripecnt);
				error = NFSERR_BADXDR;
				goto nfsmout;
			}
			NFSM_DISSECT(tl, uint32_t *, (stripecnt + 1) *
			    NFSX_UNSIGNED);
			addrcnt = fxdr_unsigned(int, *(tl + stripecnt));
			NFSCL_DEBUG(4, "addrcnt=%d\n", addrcnt);
			if (addrcnt < 1 || addrcnt > 128) {
				printf("NFS devinfo addrcnt %d: out of range\n",
				    addrcnt);
				error = NFSERR_BADXDR;
				goto nfsmout;
			}

			/*
			 * Now we know how many stripe indices and addresses, so
			 * we can allocate the structure the correct size.
			 */
			i = (stripecnt * sizeof(uint8_t)) /
			    sizeof(struct nfsclds *) + 1;
			NFSCL_DEBUG(4, "stripeindices=%d\n", i);
			ndi = malloc(sizeof(*ndi) + (addrcnt + i) *
			    sizeof(struct nfsclds *), M_NFSDEVINFO, M_WAITOK |
			    M_ZERO);
			NFSBCOPY(deviceid, ndi->nfsdi_deviceid,
			    NFSX_V4DEVICEID);
			ndi->nfsdi_refcnt = 0;
			ndi->nfsdi_flags = NFSDI_FILELAYOUT;
			ndi->nfsdi_stripecnt = stripecnt;
			ndi->nfsdi_addrcnt = addrcnt;
			/* Fill in the stripe indices. */
			for (i = 0; i < stripecnt; i++) {
				stripeindex = fxdr_unsigned(uint8_t, *tl++);
				NFSCL_DEBUG(4, "stripeind=%d\n", stripeindex);
				if (stripeindex >= addrcnt) {
					printf("pNFS File Layout devinfo"
					    " stripeindex %d: too big\n",
					    (int)stripeindex);
					error = NFSERR_BADXDR;
					goto nfsmout;
				}
				nfsfldi_setstripeindex(ndi, i, stripeindex);
			}
		} else if (layouttype == NFSLAYOUT_FLEXFILE) {
			/* For Flex File, we only get one address list. */
			ndi = malloc(sizeof(*ndi) + sizeof(struct nfsclds *),
			    M_NFSDEVINFO, M_WAITOK | M_ZERO);
			NFSBCOPY(deviceid, ndi->nfsdi_deviceid,
			    NFSX_V4DEVICEID);
			ndi->nfsdi_refcnt = 0;
			ndi->nfsdi_flags = NFSDI_FLEXFILE;
			addrcnt = ndi->nfsdi_addrcnt = 1;
		}

		/* Now, dissect the server address(es). */
		safilled = AF_UNSPEC;
		for (i = 0; i < addrcnt; i++) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			cnt = fxdr_unsigned(uint32_t, *tl);
			if (cnt == 0) {
				printf("NFS devinfo 0 len addrlist\n");
				error = NFSERR_BADXDR;
				goto nfsmout;
			}
			dspp = nfsfldi_addr(ndi, i);
			safilled = AF_UNSPEC;
			for (j = 0; j < cnt; j++) {
				error = nfsv4_getipaddr(nd, &sin, &sin6, &af,
				    &isudp);
				if (error != 0 && error != EPERM) {
					error = NFSERR_BADXDR;
					goto nfsmout;
				}
				if (error == 0 && isudp == 0) {
					/*
					 * The priority is:
					 * - Same address family.
					 * Save the address and dspp, so that
					 * the connection can be done after
					 * parsing is complete.
					 */
					if (safilled == AF_UNSPEC ||
					    (af == nmp->nm_nam->sa_family &&
					     safilled != nmp->nm_nam->sa_family)
					   ) {
						if (af == AF_INET)
							ssin = sin;
						else
							ssin6 = sin6;
						safilled = af;
						gotdspp = dspp;
					}
				}
			}
		}

		gotvers = NFS_VER4;	/* Default NFSv4.1 for File Layout. */
		gotminor = NFSV41_MINORVERSION;
		/* For Flex File, we will take one of the versions to use. */
		if (layouttype == NFSLAYOUT_FLEXFILE) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			j = fxdr_unsigned(int, *tl);
			if (j < 1 || j > NFSDEV_MAXVERS) {
				printf("pNFS: too many versions\n");
				error = NFSERR_BADXDR;
				goto nfsmout;
			}
			gotvers = 0;
			gotminor = 0;
			for (i = 0; i < j; i++) {
				NFSM_DISSECT(tl, uint32_t *, 5 * NFSX_UNSIGNED);
				vers = fxdr_unsigned(uint32_t, *tl++);
				minorvers = fxdr_unsigned(uint32_t, *tl++);
				if (vers == NFS_VER3)
					minorvers = 0;
				if ((vers == NFS_VER4 && ((minorvers ==
				    NFSV41_MINORVERSION && gotminor == 0) ||
				    minorvers == NFSV42_MINORVERSION)) ||
				    (vers == NFS_VER3 && gotvers == 0)) {
					gotvers = vers;
					gotminor = minorvers;
					/* We'll take this one. */
					ndi->nfsdi_versindex = i;
					ndi->nfsdi_vers = vers;
					ndi->nfsdi_minorvers = minorvers;
					ndi->nfsdi_rsize = fxdr_unsigned(
					    uint32_t, *tl++);
					ndi->nfsdi_wsize = fxdr_unsigned(
					    uint32_t, *tl++);
					if (*tl == newnfs_true)
						ndi->nfsdi_flags |=
						    NFSDI_TIGHTCOUPLED;
					else
						ndi->nfsdi_flags &=
						    ~NFSDI_TIGHTCOUPLED;
				}
			}
			if (gotvers == 0) {
				printf("pNFS: no NFSv3, NFSv4.1 or NFSv4.2\n");
				error = NFSERR_BADXDR;
				goto nfsmout;
			}
		}

		/* And the notify bits. */
		NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		bitcnt = fxdr_unsigned(int, *tl);
		if (bitcnt > 0) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			if (notifybitsp != NULL)
				*notifybitsp =
				    fxdr_unsigned(uint32_t, *tl);
		}
		if (safilled != AF_UNSPEC) {
			KASSERT(ndi != NULL, ("ndi is NULL"));
			*ndip = ndi;
		} else
			error = EPERM;
		if (error == 0) {
			/*
			 * Now we can do a TCP connection for the correct
			 * NFS version and IP address.
			 */
			error = nfsrpc_fillsa(nmp, &ssin, &ssin6, safilled,
			    gotvers, gotminor, &dsp, p);
		}
		if (error == 0) {
			KASSERT(gotdspp != NULL, ("gotdspp is NULL"));
			*gotdspp = dsp;
		}
	}
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
nfsmout:
	if (error != 0 && ndi != NULL)
		nfscl_freedevinfo(ndi);
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the NFSv4.1 LayoutCommit.
 */
int
nfsrpc_layoutcommit(struct nfsmount *nmp, uint8_t *fh, int fhlen, int reclaim,
    uint64_t off, uint64_t len, uint64_t lastbyte, nfsv4stateid_t *stateidp,
    int layouttype, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	nfscl_reqstart(nd, NFSPROC_LAYOUTCOMMIT, nmp, fh, fhlen, NULL, NULL,
	    0, 0, cred);
	NFSM_BUILD(tl, uint32_t *, 5 * NFSX_UNSIGNED + 3 * NFSX_HYPER +
	    NFSX_STATEID);
	txdr_hyper(off, tl);
	tl += 2;
	txdr_hyper(len, tl);
	tl += 2;
	if (reclaim != 0)
		*tl++ = newnfs_true;
	else
		*tl++ = newnfs_false;
	*tl++ = txdr_unsigned(stateidp->seqid);
	*tl++ = stateidp->other[0];
	*tl++ = stateidp->other[1];
	*tl++ = stateidp->other[2];
	*tl++ = newnfs_true;
	if (lastbyte < off)
		lastbyte = off;
	else if (lastbyte >= (off + len))
		lastbyte = off + len - 1;
	txdr_hyper(lastbyte, tl);
	tl += 2;
	*tl++ = newnfs_false;
	*tl++ = txdr_unsigned(layouttype);
	/* All supported layouts are 0 length. */
	*tl = txdr_unsigned(0);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the NFSv4.1 LayoutReturn.
 */
int
nfsrpc_layoutreturn(struct nfsmount *nmp, uint8_t *fh, int fhlen, int reclaim,
    int layouttype, uint32_t iomode, int layoutreturn, uint64_t offset,
    uint64_t len, nfsv4stateid_t *stateidp, struct ucred *cred, NFSPROC_T *p,
    uint32_t stat, uint32_t op, char *devid)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	uint64_t tu64;
	int error;

	nfscl_reqstart(nd, NFSPROC_LAYOUTRETURN, nmp, fh, fhlen, NULL, NULL,
	    0, 0, cred);
	NFSM_BUILD(tl, uint32_t *, 4 * NFSX_UNSIGNED);
	if (reclaim != 0)
		*tl++ = newnfs_true;
	else
		*tl++ = newnfs_false;
	*tl++ = txdr_unsigned(layouttype);
	*tl++ = txdr_unsigned(iomode);
	*tl = txdr_unsigned(layoutreturn);
	if (layoutreturn == NFSLAYOUTRETURN_FILE) {
		NFSM_BUILD(tl, uint32_t *, 2 * NFSX_HYPER + NFSX_STATEID +
		    NFSX_UNSIGNED);
		txdr_hyper(offset, tl);
		tl += 2;
		txdr_hyper(len, tl);
		tl += 2;
		NFSCL_DEBUG(4, "layoutret stseq=%d\n", (int)stateidp->seqid);
		*tl++ = txdr_unsigned(stateidp->seqid);
		*tl++ = stateidp->other[0];
		*tl++ = stateidp->other[1];
		*tl++ = stateidp->other[2];
		if (layouttype == NFSLAYOUT_NFSV4_1_FILES)
			*tl = txdr_unsigned(0);
		else if (layouttype == NFSLAYOUT_FLEXFILE) {
			if (stat != 0) {
				*tl = txdr_unsigned(2 * NFSX_HYPER +
				    NFSX_STATEID + NFSX_V4DEVICEID + 5 *
				    NFSX_UNSIGNED);
				NFSM_BUILD(tl, uint32_t *, 2 * NFSX_HYPER +
				    NFSX_STATEID + NFSX_V4DEVICEID + 5 *
				    NFSX_UNSIGNED);
				*tl++ = txdr_unsigned(1);	/* One error. */
				tu64 = 0;			/* Offset. */
				txdr_hyper(tu64, tl); tl += 2;
				tu64 = UINT64_MAX;		/* Length. */
				txdr_hyper(tu64, tl); tl += 2;
				NFSBCOPY(stateidp, tl, NFSX_STATEID);
				tl += (NFSX_STATEID / NFSX_UNSIGNED);
				*tl++ = txdr_unsigned(1);	/* One error. */
				NFSBCOPY(devid, tl, NFSX_V4DEVICEID);
				tl += (NFSX_V4DEVICEID / NFSX_UNSIGNED);
				*tl++ = txdr_unsigned(stat);
				*tl++ = txdr_unsigned(op);
			} else {
				*tl = txdr_unsigned(2 * NFSX_UNSIGNED);
				NFSM_BUILD(tl, uint32_t *, 2 * NFSX_UNSIGNED);
				/* No ioerrs. */
				*tl++ = 0;
			}
			*tl = 0;	/* No stats yet. */
		}
	}
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		if (*tl != 0) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_STATEID);
			stateidp->seqid = fxdr_unsigned(uint32_t, *tl++);
			stateidp->other[0] = *tl++;
			stateidp->other[1] = *tl++;
			stateidp->other[2] = *tl;
		}
	} else
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Do the NFSv4.2 LayoutError.
 */
static int
nfsrpc_layouterror(struct nfsmount *nmp, uint8_t *fh, int fhlen, uint64_t offset,
    uint64_t len, nfsv4stateid_t *stateidp, struct ucred *cred, NFSPROC_T *p,
    uint32_t stat, uint32_t op, char *devid)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	nfscl_reqstart(nd, NFSPROC_LAYOUTERROR, nmp, fh, fhlen, NULL, NULL,
	    0, 0, cred);
	NFSM_BUILD(tl, uint32_t *, 2 * NFSX_HYPER + NFSX_STATEID +
	    NFSX_V4DEVICEID + 3 * NFSX_UNSIGNED);
	txdr_hyper(offset, tl); tl += 2;
	txdr_hyper(len, tl); tl += 2;
	*tl++ = txdr_unsigned(stateidp->seqid);
	*tl++ = stateidp->other[0];
	*tl++ = stateidp->other[1];
	*tl++ = stateidp->other[2];
	*tl++ = txdr_unsigned(1);
	NFSBCOPY(devid, tl, NFSX_V4DEVICEID);
	tl += (NFSX_V4DEVICEID / NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(stat);
	*tl = txdr_unsigned(op);
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	if (nd->nd_repstat != 0)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Acquire a layout and devinfo, if possible. The caller must have acquired
 * a reference count on the nfsclclient structure before calling this.
 * Return the layout in lypp with a reference count on it, if successful.
 */
static int
nfsrpc_getlayout(struct nfsmount *nmp, vnode_t vp, struct nfsfh *nfhp,
    int iomode, uint32_t rw, uint32_t *notifybitsp, nfsv4stateid_t *stateidp,
    uint64_t off, struct nfscllayout **lypp, struct ucred *cred, NFSPROC_T *p)
{
	struct nfscllayout *lyp;
	struct nfsclflayout *flp;
	struct nfsclflayouthead flh;
	int error = 0, islocked, layoutlen, layouttype, recalled, retonclose;
	nfsv4stateid_t stateid;
	struct nfsclsession *tsep;

	*lypp = NULL;
	if (NFSHASFLEXFILE(nmp))
		layouttype = NFSLAYOUT_FLEXFILE;
	else
		layouttype = NFSLAYOUT_NFSV4_1_FILES;
	/*
	 * If lyp is returned non-NULL, there will be a refcnt (shared lock)
	 * on it, iff flp != NULL or a lock (exclusive lock) on it iff
	 * flp == NULL.
	 */
	lyp = nfscl_getlayout(nmp->nm_clp, nfhp->nfh_fh, nfhp->nfh_len,
	    off, rw, &flp, &recalled);
	islocked = 0;
	if (lyp == NULL || flp == NULL) {
		if (recalled != 0)
			return (EIO);
		LIST_INIT(&flh);
		tsep = nfsmnt_mdssession(nmp);
		layoutlen = tsep->nfsess_maxcache -
		    (NFSX_STATEID + 3 * NFSX_UNSIGNED);
		if (lyp == NULL) {
			stateid.seqid = 0;
			stateid.other[0] = stateidp->other[0];
			stateid.other[1] = stateidp->other[1];
			stateid.other[2] = stateidp->other[2];
			error = nfsrpc_layoutget(nmp, nfhp->nfh_fh,
			    nfhp->nfh_len, iomode, (uint64_t)0, UINT64_MAX,
			    (uint64_t)0, layouttype, layoutlen, &stateid,
			    &retonclose, &flh, cred, p);
		} else {
			islocked = 1;
			stateid.seqid = lyp->nfsly_stateid.seqid;
			stateid.other[0] = lyp->nfsly_stateid.other[0];
			stateid.other[1] = lyp->nfsly_stateid.other[1];
			stateid.other[2] = lyp->nfsly_stateid.other[2];
			error = nfsrpc_layoutget(nmp, nfhp->nfh_fh,
			    nfhp->nfh_len, iomode, off, UINT64_MAX,
			    (uint64_t)0, layouttype, layoutlen, &stateid,
			    &retonclose, &flh, cred, p);
		}
		error = nfsrpc_layoutgetres(nmp, vp, nfhp->nfh_fh,
		    nfhp->nfh_len, &stateid, retonclose, notifybitsp, &lyp,
		    &flh, layouttype, error, NULL, cred, p);
		if (error == 0)
			*lypp = lyp;
		else if (islocked != 0)
			nfscl_rellayout(lyp, 1);
	} else
		*lypp = lyp;
	return (error);
}

/*
 * Do a TCP connection plus exchange id and create session.
 * If successful, a "struct nfsclds" is linked into the list for the
 * mount point and a pointer to it is returned.
 */
static int
nfsrpc_fillsa(struct nfsmount *nmp, struct sockaddr_in *sin,
    struct sockaddr_in6 *sin6, sa_family_t af, int vers, int minorvers,
    struct nfsclds **dspp, NFSPROC_T *p)
{
	struct sockaddr_in *msad, *sad;
	struct sockaddr_in6 *msad6, *sad6;
	struct nfsclclient *clp;
	struct nfssockreq *nrp;
	struct nfsclds *dsp, *tdsp;
	int error, firsttry;
	enum nfsclds_state retv;
	uint32_t sequenceid = 0;

	KASSERT(nmp->nm_sockreq.nr_cred != NULL,
	    ("nfsrpc_fillsa: NULL nr_cred"));
	NFSLOCKCLSTATE();
	clp = nmp->nm_clp;
	NFSUNLOCKCLSTATE();
	if (clp == NULL)
		return (EPERM);
	if (af == AF_INET) {
		NFSLOCKMNT(nmp);
		/*
		 * Check to see if we already have a session for this
		 * address that is usable for a DS.
		 * Note that the MDS's address is in a different place
		 * than the sessions already acquired for DS's.
		 */
		msad = (struct sockaddr_in *)nmp->nm_sockreq.nr_nam;
		tdsp = TAILQ_FIRST(&nmp->nm_sess);
		while (tdsp != NULL) {
			if (msad != NULL && msad->sin_family == AF_INET &&
			    sin->sin_addr.s_addr == msad->sin_addr.s_addr &&
			    sin->sin_port == msad->sin_port &&
			    (tdsp->nfsclds_flags & NFSCLDS_DS) != 0 &&
			    tdsp->nfsclds_sess.nfsess_defunct == 0) {
				*dspp = tdsp;
				NFSUNLOCKMNT(nmp);
				NFSCL_DEBUG(4, "fnd same addr\n");
				return (0);
			}
			tdsp = TAILQ_NEXT(tdsp, nfsclds_list);
			if (tdsp != NULL && tdsp->nfsclds_sockp != NULL)
				msad = (struct sockaddr_in *)
				    tdsp->nfsclds_sockp->nr_nam;
			else
				msad = NULL;
		}
		NFSUNLOCKMNT(nmp);

		/* No IP address match, so look for new/trunked one. */
		sad = malloc(sizeof(*sad), M_SONAME, M_WAITOK | M_ZERO);
		sad->sin_len = sizeof(*sad);
		sad->sin_family = AF_INET;
		sad->sin_port = sin->sin_port;
		sad->sin_addr.s_addr = sin->sin_addr.s_addr;
		if (NFSHASPNFS(nmp) && NFSHASKERB(nmp)) {
			/* For pNFS, a separate server principal is needed. */
			nrp = malloc(sizeof(*nrp) + NI_MAXSERV + NI_MAXHOST,
			    M_NFSSOCKREQ, M_WAITOK | M_ZERO);
			/*
			 * Use the latter part of nr_srvprinc as a temporary
			 * buffer for the IP address.
			 */
			inet_ntoa_r(sad->sin_addr,
			    &nrp->nr_srvprinc[NI_MAXSERV]);
			NFSCL_DEBUG(1, "nfsrpc_fillsa: DS IP=%s\n",
			    &nrp->nr_srvprinc[NI_MAXSERV]);
			if (!rpc_gss_ip_to_srv_principal_call(
			    &nrp->nr_srvprinc[NI_MAXSERV], "nfs",
			    nrp->nr_srvprinc))
				nrp->nr_srvprinc[0] = '\0';
			NFSCL_DEBUG(1, "nfsrpc_fillsa: srv principal=%s\n",
			    nrp->nr_srvprinc);
		} else
			nrp = malloc(sizeof(*nrp), M_NFSSOCKREQ,
			    M_WAITOK | M_ZERO);
		nrp->nr_nam = (struct sockaddr *)sad;
	} else if (af == AF_INET6) {
		NFSLOCKMNT(nmp);
		/*
		 * Check to see if we already have a session for this
		 * address that is usable for a DS.
		 * Note that the MDS's address is in a different place
		 * than the sessions already acquired for DS's.
		 */
		msad6 = (struct sockaddr_in6 *)nmp->nm_sockreq.nr_nam;
		tdsp = TAILQ_FIRST(&nmp->nm_sess);
		while (tdsp != NULL) {
			if (msad6 != NULL && msad6->sin6_family == AF_INET6 &&
			    IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
			    &msad6->sin6_addr) &&
			    sin6->sin6_port == msad6->sin6_port &&
			    (tdsp->nfsclds_flags & NFSCLDS_DS) != 0 &&
			    tdsp->nfsclds_sess.nfsess_defunct == 0) {
				*dspp = tdsp;
				NFSUNLOCKMNT(nmp);
				return (0);
			}
			tdsp = TAILQ_NEXT(tdsp, nfsclds_list);
			if (tdsp != NULL && tdsp->nfsclds_sockp != NULL)
				msad6 = (struct sockaddr_in6 *)
				    tdsp->nfsclds_sockp->nr_nam;
			else
				msad6 = NULL;
		}
		NFSUNLOCKMNT(nmp);

		/* No IP address match, so look for new/trunked one. */
		sad6 = malloc(sizeof(*sad6), M_SONAME, M_WAITOK | M_ZERO);
		sad6->sin6_len = sizeof(*sad6);
		sad6->sin6_family = AF_INET6;
		sad6->sin6_port = sin6->sin6_port;
		NFSBCOPY(&sin6->sin6_addr, &sad6->sin6_addr,
		    sizeof(struct in6_addr));
		if (NFSHASPNFS(nmp) && NFSHASKERB(nmp)) {
			/* For pNFS, a separate server principal is needed. */
			nrp = malloc(sizeof(*nrp) + NI_MAXSERV + NI_MAXHOST,
			    M_NFSSOCKREQ, M_WAITOK | M_ZERO);
			/*
			 * Use the latter part of nr_srvprinc as a temporary
			 * buffer for the IP address.
			 */
			inet_ntop(AF_INET6, &sad6->sin6_addr,
			    &nrp->nr_srvprinc[NI_MAXSERV], NI_MAXHOST);
			NFSCL_DEBUG(1, "nfsrpc_fillsa: DS IP=%s\n",
			    &nrp->nr_srvprinc[NI_MAXSERV]);
			if (!rpc_gss_ip_to_srv_principal_call(
			    &nrp->nr_srvprinc[NI_MAXSERV], "nfs",
			    nrp->nr_srvprinc))
				nrp->nr_srvprinc[0] = '\0';
			NFSCL_DEBUG(1, "nfsrpc_fillsa: srv principal=%s\n",
			    nrp->nr_srvprinc);
		} else
			nrp = malloc(sizeof(*nrp), M_NFSSOCKREQ,
			    M_WAITOK | M_ZERO);
		nrp->nr_nam = (struct sockaddr *)sad6;
	} else
		return (EPERM);

	nrp->nr_sotype = SOCK_STREAM;
	mtx_init(&nrp->nr_mtx, "nfssock", NULL, MTX_DEF);
	nrp->nr_prog = NFS_PROG;
	nrp->nr_vers = vers;

	/*
	 * Use the credentials that were used for the mount, which are
	 * in nmp->nm_sockreq.nr_cred for newnfs_connect() etc.
	 * Ref. counting the credentials with crhold() is probably not
	 * necessary, since nm_sockreq.nr_cred won't be crfree()'d until
	 * unmount, but I did it anyhow.
	 */
	nrp->nr_cred = crhold(nmp->nm_sockreq.nr_cred);
	error = newnfs_connect(nmp, nrp, NULL, p, 0, false, &nrp->nr_client);
	NFSCL_DEBUG(3, "DS connect=%d\n", error);

	dsp = NULL;
	/* Now, do the exchangeid and create session. */
	if (error == 0) {
		if (vers == NFS_VER4) {
			firsttry = 0;
			do {
				error = nfsrpc_exchangeid(nmp, clp, nrp, 
				    minorvers, NFSV4EXCH_USEPNFSDS, &dsp,
				    nrp->nr_cred, p);
				NFSCL_DEBUG(3, "DS exchangeid=%d\n", error);
				if (error == NFSERR_MINORVERMISMATCH)
					minorvers = NFSV42_MINORVERSION;
			} while (error == NFSERR_MINORVERMISMATCH &&
			    firsttry++ == 0);
			if (error != 0)
				newnfs_disconnect(NULL, nrp);
		} else {
			dsp = malloc(sizeof(struct nfsclds), M_NFSCLDS,
			    M_WAITOK | M_ZERO);
			dsp->nfsclds_flags |= NFSCLDS_DS;
			dsp->nfsclds_expire = INT32_MAX; /* No renews needed. */
			mtx_init(&dsp->nfsclds_mtx, "nfsds", NULL, MTX_DEF);
			mtx_init(&dsp->nfsclds_sess.nfsess_mtx, "nfssession",
			    NULL, MTX_DEF);
		}
	}
	if (error == 0) {
		dsp->nfsclds_sockp = nrp;
		if (vers == NFS_VER4) {
			NFSLOCKMNT(nmp);
			retv = nfscl_getsameserver(nmp, dsp, &tdsp,
			    &sequenceid);
			NFSCL_DEBUG(3, "getsame ret=%d\n", retv);
			if (retv == NFSDSP_USETHISSESSION &&
			    nfscl_dssameconn != 0) {
				NFSLOCKDS(tdsp);
				tdsp->nfsclds_flags |= NFSCLDS_SAMECONN;
				NFSUNLOCKDS(tdsp);
				NFSUNLOCKMNT(nmp);
				/*
				 * If there is already a session for this
				 * server, use it.
				 */
				newnfs_disconnect(NULL, nrp);
				nfscl_freenfsclds(dsp);
				*dspp = tdsp;
				return (0);
			}
			if (retv == NFSDSP_NOTFOUND)
				sequenceid =
				    dsp->nfsclds_sess.nfsess_sequenceid;
			NFSUNLOCKMNT(nmp);
			error = nfsrpc_createsession(nmp, &dsp->nfsclds_sess,
			    nrp, dsp, sequenceid, 0, nrp->nr_cred, p);
			NFSCL_DEBUG(3, "DS createsess=%d\n", error);
		}
	} else {
		NFSFREECRED(nrp->nr_cred);
		NFSFREEMUTEX(&nrp->nr_mtx);
		free(nrp->nr_nam, M_SONAME);
		free(nrp, M_NFSSOCKREQ);
	}
	if (error == 0) {
		NFSCL_DEBUG(3, "add DS session\n");
		/*
		 * Put it at the end of the list. That way the list
		 * is ordered by when the entry was added. This matters
		 * since the one done first is the one that should be
		 * used for sequencid'ing any subsequent create sessions.
		 */
		NFSLOCKMNT(nmp);
		TAILQ_INSERT_TAIL(&nmp->nm_sess, dsp, nfsclds_list);
		NFSUNLOCKMNT(nmp);
		*dspp = dsp;
	} else if (dsp != NULL) {
		newnfs_disconnect(NULL, nrp);
		nfscl_freenfsclds(dsp);
	}
	return (error);
}

/*
 * Do the NFSv4.1 Reclaim Complete.
 */
int
nfsrpc_reclaimcomplete(struct nfsmount *nmp, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	int error;

	nfscl_reqstart(nd, NFSPROC_RECLAIMCOMPL, nmp, NULL, 0, NULL, NULL, 0,
	    0, cred);
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = newnfs_false;
	nd->nd_flag |= ND_USEGSSNAME;
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Initialize the slot tables for a session.
 */
static void
nfscl_initsessionslots(struct nfsclsession *sep)
{
	int i;

	for (i = 0; i < NFSV4_CBSLOTS; i++) {
		if (sep->nfsess_cbslots[i].nfssl_reply != NULL)
			m_freem(sep->nfsess_cbslots[i].nfssl_reply);
		NFSBZERO(&sep->nfsess_cbslots[i], sizeof(struct nfsslot));
	}
	for (i = 0; i < 64; i++)
		sep->nfsess_slotseq[i] = 0;
	sep->nfsess_slots = 0;
	sep->nfsess_badslots = 0;
}

/*
 * Called to try and do an I/O operation via an NFSv4.1 Data Server (DS).
 */
int
nfscl_doiods(vnode_t vp, struct uio *uiop, int *iomode, int *must_commit,
    uint32_t rwaccess, int docommit, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfscllayout *layp;
	struct nfscldevinfo *dip;
	struct nfsclflayout *rflp;
	struct mbuf *m, *m2;
	struct nfsclwritedsdorpc *drpc, *tdrpc;
	nfsv4stateid_t stateid;
	struct ucred *newcred;
	uint64_t lastbyte, len, off, oresid, xfer;
	int eof, error, firstmirror, i, iolaymode, mirrorcnt, recalled, timo;
	void *lckp;
	uint8_t *dev;
	void *iovbase = NULL;
	size_t iovlen = 0;
	off_t offs = 0;
	ssize_t resid = 0;
	uint32_t op;

	if (!NFSHASPNFS(nmp) || nfscl_enablecallb == 0 || nfs_numnfscbd == 0 ||
	    (np->n_flag & NNOLAYOUT) != 0)
		return (EIO);
	/* Now, get a reference cnt on the clientid for this mount. */
	if (nfscl_getref(nmp) == 0)
		return (EIO);

	/* Find an appropriate stateid. */
	newcred = NFSNEWCRED(cred);
	error = nfscl_getstateid(vp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len,
	    rwaccess, 1, newcred, p, &stateid, &lckp);
	if (error != 0) {
		NFSFREECRED(newcred);
		nfscl_relref(nmp);
		return (error);
	}
	/* Search for a layout for this file. */
	off = uiop->uio_offset;
	layp = nfscl_getlayout(nmp->nm_clp, np->n_fhp->nfh_fh,
	    np->n_fhp->nfh_len, off, rwaccess, &rflp, &recalled);
	if (layp == NULL || rflp == NULL) {
		if (recalled != 0) {
			NFSFREECRED(newcred);
			if (lckp != NULL)
				nfscl_lockderef(lckp);
			nfscl_relref(nmp);
			return (EIO);
		}
		if (layp != NULL) {
			nfscl_rellayout(layp, (rflp == NULL) ? 1 : 0);
			layp = NULL;
		}
		/* Try and get a Layout, if it is supported. */
		if (rwaccess == NFSV4OPEN_ACCESSWRITE ||
		    (np->n_flag & NWRITEOPENED) != 0)
			iolaymode = NFSLAYOUTIOMODE_RW;
		else
			iolaymode = NFSLAYOUTIOMODE_READ;
		error = nfsrpc_getlayout(nmp, vp, np->n_fhp, iolaymode,
		    rwaccess, NULL, &stateid, off, &layp, newcred, p);
		if (error != 0) {
			NFSLOCKNODE(np);
			np->n_flag |= NNOLAYOUT;
			NFSUNLOCKNODE(np);
			if (lckp != NULL)
				nfscl_lockderef(lckp);
			NFSFREECRED(newcred);
			if (layp != NULL)
				nfscl_rellayout(layp, 0);
			nfscl_relref(nmp);
			return (error);
		}
	}

	/*
	 * Loop around finding a layout that works for the first part of
	 * this I/O operation, and then call the function that actually
	 * does the RPC.
	 */
	eof = 0;
	len = (uint64_t)uiop->uio_resid;
	while (len > 0 && error == 0 && eof == 0) {
		off = uiop->uio_offset;
		error = nfscl_findlayoutforio(layp, off, rwaccess, &rflp);
		if (error == 0) {
			oresid = xfer = (uint64_t)uiop->uio_resid;
			if (xfer > (rflp->nfsfl_end - rflp->nfsfl_off))
				xfer = rflp->nfsfl_end - rflp->nfsfl_off;
			/*
			 * For Flex File layout with mirrored DSs, select one
			 * of them at random for reads. For writes and commits,
			 * do all mirrors.
			 */
			m = NULL;
			tdrpc = drpc = NULL;
			firstmirror = 0;
			mirrorcnt = 1;
			if ((layp->nfsly_flags & NFSLY_FLEXFILE) != 0 &&
			    (mirrorcnt = rflp->nfsfl_mirrorcnt) > 1) {
				if (rwaccess == NFSV4OPEN_ACCESSREAD) {
					firstmirror = arc4random() % mirrorcnt;
					mirrorcnt = firstmirror + 1;
				} else {
					if (docommit == 0) {
						/*
						 * Save values, so uiop can be
						 * rolled back upon a write
						 * error.
						 */
						offs = uiop->uio_offset;
						resid = uiop->uio_resid;
						iovbase =
						    uiop->uio_iov->iov_base;
						iovlen = uiop->uio_iov->iov_len;
						m = nfsm_uiombuflist(uiop, len,
						    0);
						if (m == NULL) {
							error = EFAULT;
							break;
						}
					}
					tdrpc = drpc = malloc(sizeof(*drpc) *
					    (mirrorcnt - 1), M_TEMP, M_WAITOK |
					    M_ZERO);
				}
			}
			for (i = firstmirror; i < mirrorcnt && error == 0; i++){
				m2 = NULL;
				if (m != NULL && i < mirrorcnt - 1)
					m2 = m_copym(m, 0, M_COPYALL, M_WAITOK);
				else {
					m2 = m;
					m = NULL;
				}
				if ((layp->nfsly_flags & NFSLY_FLEXFILE) != 0) {
					dev = rflp->nfsfl_ffm[i].dev;
					dip = nfscl_getdevinfo(nmp->nm_clp, dev,
					    rflp->nfsfl_ffm[i].devp);
				} else {
					dev = rflp->nfsfl_dev;
					dip = nfscl_getdevinfo(nmp->nm_clp, dev,
					    rflp->nfsfl_devp);
				}
				if (dip != NULL) {
					if ((rflp->nfsfl_flags & NFSFL_FLEXFILE)
					    != 0)
						error = nfscl_dofflayoutio(vp,
						    uiop, iomode, must_commit,
						    &eof, &stateid, rwaccess,
						    dip, layp, rflp, off, xfer,
						    i, docommit, m2, tdrpc,
						    newcred, p);
					else
						error = nfscl_doflayoutio(vp,
						    uiop, iomode, must_commit,
						    &eof, &stateid, rwaccess,
						    dip, layp, rflp, off, xfer,
						    docommit, newcred, p);
					nfscl_reldevinfo(dip);
				} else {
					if (m2 != NULL)
						m_freem(m2);
					error = EIO;
				}
				tdrpc++;
			}
			if (m != NULL)
				m_freem(m);
			tdrpc = drpc;
			timo = hz / 50;		/* Wait for 20msec. */
			if (timo < 1)
				timo = 1;
			for (i = firstmirror; i < mirrorcnt - 1 &&
			    tdrpc != NULL; i++, tdrpc++) {
				/*
				 * For the unused drpc entries, both inprog and
				 * err == 0, so this loop won't break.
				 */
				while (tdrpc->inprog != 0 && tdrpc->done == 0)
					tsleep(&tdrpc->tsk, PVFS, "clrpcio",
					    timo);
				if (error == 0 && tdrpc->err != 0)
					error = tdrpc->err;
				if (rwaccess != NFSV4OPEN_ACCESSREAD &&
				    docommit == 0 && *must_commit == 0 &&
				    tdrpc->must_commit == 1)
					*must_commit = 1;
			}
			free(drpc, M_TEMP);
			if (error == 0) {
				if (mirrorcnt > 1 && rwaccess ==
				    NFSV4OPEN_ACCESSWRITE && docommit == 0) {
					NFSLOCKCLSTATE();
					layp->nfsly_flags |= NFSLY_WRITTEN;
					NFSUNLOCKCLSTATE();
				}
				lastbyte = off + xfer - 1;
				NFSLOCKCLSTATE();
				if (lastbyte > layp->nfsly_lastbyte)
					layp->nfsly_lastbyte = lastbyte;
				NFSUNLOCKCLSTATE();
			} else if (error == NFSERR_OPENMODE &&
			    rwaccess == NFSV4OPEN_ACCESSREAD) {
				NFSLOCKMNT(nmp);
				nmp->nm_state |= NFSSTA_OPENMODE;
				NFSUNLOCKMNT(nmp);
			} else if ((error == NFSERR_NOSPC ||
			    error == NFSERR_IO || error == NFSERR_NXIO) &&
			    nmp->nm_minorvers == NFSV42_MINORVERSION) {
				if (docommit != 0)
					op = NFSV4OP_COMMIT;
				else if (rwaccess == NFSV4OPEN_ACCESSREAD)
					op = NFSV4OP_READ;
				else
					op = NFSV4OP_WRITE;
				nfsrpc_layouterror(nmp, np->n_fhp->nfh_fh,
				    np->n_fhp->nfh_len, off, xfer,
				    &layp->nfsly_stateid, newcred, p, error, op,
				    dip->nfsdi_deviceid);
				error = EIO;
			} else
				error = EIO;
			if (error == 0)
				len -= (oresid - (uint64_t)uiop->uio_resid);
			else if (mirrorcnt > 1 && rwaccess ==
			    NFSV4OPEN_ACCESSWRITE && docommit == 0) {
				/*
				 * In case the rpc gets retried, roll the
				 * uio fields changed by nfsm_uiombuflist()
				 * back.
				 */
				uiop->uio_offset = offs;
				uiop->uio_resid = resid;
				uiop->uio_iov->iov_base = iovbase;
				uiop->uio_iov->iov_len = iovlen;
			}
		}
	}
	if (lckp != NULL)
		nfscl_lockderef(lckp);
	NFSFREECRED(newcred);
	nfscl_rellayout(layp, 0);
	nfscl_relref(nmp);
	return (error);
}

/*
 * Find a file layout that will handle the first bytes of the requested
 * range and return the information from it needed to the I/O operation.
 */
int
nfscl_findlayoutforio(struct nfscllayout *lyp, uint64_t off, uint32_t rwaccess,
    struct nfsclflayout **retflpp)
{
	struct nfsclflayout *flp, *nflp, *rflp;
	uint32_t rw;

	rflp = NULL;
	rw = rwaccess;
	/* For reading, do the Read list first and then the Write list. */
	do {
		if (rw == NFSV4OPEN_ACCESSREAD)
			flp = LIST_FIRST(&lyp->nfsly_flayread);
		else
			flp = LIST_FIRST(&lyp->nfsly_flayrw);
		while (flp != NULL) {
			nflp = LIST_NEXT(flp, nfsfl_list);
			if (flp->nfsfl_off > off)
				break;
			if (flp->nfsfl_end > off &&
			    (rflp == NULL || rflp->nfsfl_end < flp->nfsfl_end))
				rflp = flp;
			flp = nflp;
		}
		if (rw == NFSV4OPEN_ACCESSREAD)
			rw = NFSV4OPEN_ACCESSWRITE;
		else
			rw = 0;
	} while (rw != 0);
	if (rflp != NULL) {
		/* This one covers the most bytes starting at off. */
		*retflpp = rflp;
		return (0);
	}
	return (EIO);
}

/*
 * Do I/O using an NFSv4.1 or NFSv4.2 file layout.
 */
static int
nfscl_doflayoutio(vnode_t vp, struct uio *uiop, int *iomode, int *must_commit,
    int *eofp, nfsv4stateid_t *stateidp, int rwflag, struct nfscldevinfo *dp,
    struct nfscllayout *lyp, struct nfsclflayout *flp, uint64_t off,
    uint64_t len, int docommit, struct ucred *cred, NFSPROC_T *p)
{
	uint64_t io_off, rel_off, stripe_unit_size, transfer, xfer;
	int commit_thru_mds, error, stripe_index, stripe_pos, minorvers;
	struct nfsnode *np;
	struct nfsfh *fhp;
	struct nfsclds **dspp;

	np = VTONFS(vp);
	rel_off = off - flp->nfsfl_patoff;
	stripe_unit_size = flp->nfsfl_util & NFSFLAYUTIL_STRIPE_MASK;
	stripe_pos = (rel_off / stripe_unit_size + flp->nfsfl_stripe1) %
	    dp->nfsdi_stripecnt;
	transfer = stripe_unit_size - (rel_off % stripe_unit_size);
	error = 0;

	/* Loop around, doing I/O for each stripe unit. */
	while (len > 0 && error == 0) {
		stripe_index = nfsfldi_stripeindex(dp, stripe_pos);
		dspp = nfsfldi_addr(dp, stripe_index);
		if (((*dspp)->nfsclds_flags & NFSCLDS_MINORV2) != 0)
			minorvers = NFSV42_MINORVERSION;
		else
			minorvers = NFSV41_MINORVERSION;
		if (len > transfer && docommit == 0)
			xfer = transfer;
		else
			xfer = len;
		if ((flp->nfsfl_util & NFSFLAYUTIL_DENSE) != 0) {
			/* Dense layout. */
			if (stripe_pos >= flp->nfsfl_fhcnt)
				return (EIO);
			fhp = flp->nfsfl_fh[stripe_pos];
			io_off = (rel_off / (stripe_unit_size *
			    dp->nfsdi_stripecnt)) * stripe_unit_size +
			    rel_off % stripe_unit_size;
		} else {
			/* Sparse layout. */
			if (flp->nfsfl_fhcnt > 1) {
				if (stripe_index >= flp->nfsfl_fhcnt)
					return (EIO);
				fhp = flp->nfsfl_fh[stripe_index];
			} else if (flp->nfsfl_fhcnt == 1)
				fhp = flp->nfsfl_fh[0];
			else
				fhp = np->n_fhp;
			io_off = off;
		}
		if ((flp->nfsfl_util & NFSFLAYUTIL_COMMIT_THRU_MDS) != 0) {
			commit_thru_mds = 1;
			if (docommit != 0)
				error = EIO;
		} else {
			commit_thru_mds = 0;
			NFSLOCKNODE(np);
			np->n_flag |= NDSCOMMIT;
			NFSUNLOCKNODE(np);
		}
		if (docommit != 0) {
			if (error == 0)
				error = nfsrpc_commitds(vp, io_off, xfer,
				    *dspp, fhp, NFS_VER4, minorvers, cred, p);
			if (error == 0) {
				/*
				 * Set both eof and uio_resid = 0 to end any
				 * loops.
				 */
				*eofp = 1;
				uiop->uio_resid = 0;
			} else {
				NFSLOCKNODE(np);
				np->n_flag &= ~NDSCOMMIT;
				NFSUNLOCKNODE(np);
			}
		} else if (rwflag == NFSV4OPEN_ACCESSREAD)
			error = nfsrpc_readds(vp, uiop, stateidp, eofp, *dspp,
			    io_off, xfer, fhp, 0, NFS_VER4, minorvers, cred, p);
		else {
			error = nfsrpc_writeds(vp, uiop, iomode, must_commit,
			    stateidp, *dspp, io_off, xfer, fhp, commit_thru_mds,
			    0, NFS_VER4, minorvers, cred, p);
			if (error == 0) {
				NFSLOCKCLSTATE();
				lyp->nfsly_flags |= NFSLY_WRITTEN;
				NFSUNLOCKCLSTATE();
			}
		}
		if (error == 0) {
			transfer = stripe_unit_size;
			stripe_pos = (stripe_pos + 1) % dp->nfsdi_stripecnt;
			len -= xfer;
			off += xfer;
		}
	}
	return (error);
}

/*
 * Do I/O using an NFSv4.1 flex file layout.
 */
static int
nfscl_dofflayoutio(vnode_t vp, struct uio *uiop, int *iomode, int *must_commit,
    int *eofp, nfsv4stateid_t *stateidp, int rwflag, struct nfscldevinfo *dp,
    struct nfscllayout *lyp, struct nfsclflayout *flp, uint64_t off,
    uint64_t len, int mirror, int docommit, struct mbuf *mp,
    struct nfsclwritedsdorpc *drpc, struct ucred *cred, NFSPROC_T *p)
{
	uint64_t xfer;
	int error;
	struct nfsnode *np;
	struct nfsfh *fhp;
	struct nfsclds **dspp;
	struct ucred *tcred;
	struct mbuf *m, *m2;
	uint32_t copylen;

	np = VTONFS(vp);
	error = 0;
	NFSCL_DEBUG(4, "nfscl_dofflayoutio: off=%ju len=%ju\n", (uintmax_t)off,
	    (uintmax_t)len);
	/* Loop around, doing I/O for each stripe unit. */
	while (len > 0 && error == 0) {
		dspp = nfsfldi_addr(dp, 0);
		fhp = flp->nfsfl_ffm[mirror].fh[dp->nfsdi_versindex];
		stateidp = &flp->nfsfl_ffm[mirror].st;
		NFSCL_DEBUG(4, "mirror=%d vind=%d fhlen=%d st.seqid=0x%x\n",
		    mirror, dp->nfsdi_versindex, fhp->nfh_len, stateidp->seqid);
		if ((dp->nfsdi_flags & NFSDI_TIGHTCOUPLED) == 0) {
			tcred = NFSNEWCRED(cred);
			tcred->cr_uid = flp->nfsfl_ffm[mirror].user;
			tcred->cr_groups[0] = flp->nfsfl_ffm[mirror].group;
			tcred->cr_ngroups = 1;
		} else
			tcred = cred;
		if (rwflag == NFSV4OPEN_ACCESSREAD)
			copylen = dp->nfsdi_rsize;
		else {
			copylen = dp->nfsdi_wsize;
			if (len > copylen && mp != NULL) {
				/*
				 * When a mirrored configuration needs to do
				 * multiple writes to each mirror, all writes
				 * except the last one must be a multiple of
				 * 4 bytes.  This is required so that the XDR
				 * does not need padding.
				 * If possible, clip the size to an exact
				 * multiple of the mbuf length, so that the
				 * split will be on an mbuf boundary.
				 */
				copylen &= 0xfffffffc;
				if (copylen > mp->m_len)
					copylen = copylen / mp->m_len *
					    mp->m_len;
			}
		}
		NFSLOCKNODE(np);
		np->n_flag |= NDSCOMMIT;
		NFSUNLOCKNODE(np);
		if (len > copylen && docommit == 0)
			xfer = copylen;
		else
			xfer = len;
		if (docommit != 0) {
			if (error == 0) {
				/*
				 * Do last mirrored DS commit with this thread.
				 */
				if (mirror < flp->nfsfl_mirrorcnt - 1)
					error = nfsio_commitds(vp, off, xfer,
					    *dspp, fhp, dp->nfsdi_vers,
					    dp->nfsdi_minorvers, drpc, tcred,
					    p);
				else
					error = nfsrpc_commitds(vp, off, xfer,
					    *dspp, fhp, dp->nfsdi_vers,
					    dp->nfsdi_minorvers, tcred, p);
				NFSCL_DEBUG(4, "commitds=%d\n", error);
				if (error != 0 && error != EACCES && error !=
				    ESTALE) {
					NFSCL_DEBUG(4,
					    "DS layreterr for commit\n");
					nfscl_dserr(NFSV4OP_COMMIT, error, dp,
					    lyp, *dspp);
				}
			}
			NFSCL_DEBUG(4, "aft nfsio_commitds=%d\n", error);
			if (error == 0) {
				/*
				 * Set both eof and uio_resid = 0 to end any
				 * loops.
				 */
				*eofp = 1;
				uiop->uio_resid = 0;
			} else {
				NFSLOCKNODE(np);
				np->n_flag &= ~NDSCOMMIT;
				NFSUNLOCKNODE(np);
			}
		} else if (rwflag == NFSV4OPEN_ACCESSREAD) {
			error = nfsrpc_readds(vp, uiop, stateidp, eofp, *dspp,
			    off, xfer, fhp, 1, dp->nfsdi_vers,
			    dp->nfsdi_minorvers, tcred, p);
			NFSCL_DEBUG(4, "readds=%d\n", error);
			if (error != 0 && error != EACCES && error != ESTALE) {
				NFSCL_DEBUG(4, "DS layreterr for read\n");
				nfscl_dserr(NFSV4OP_READ, error, dp, lyp,
				    *dspp);
			}
		} else {
			if (flp->nfsfl_mirrorcnt == 1) {
				error = nfsrpc_writeds(vp, uiop, iomode,
				    must_commit, stateidp, *dspp, off, xfer,
				    fhp, 0, 1, dp->nfsdi_vers,
				    dp->nfsdi_minorvers, tcred, p);
				if (error == 0) {
					NFSLOCKCLSTATE();
					lyp->nfsly_flags |= NFSLY_WRITTEN;
					NFSUNLOCKCLSTATE();
				}
			} else {
				m = mp;
				if (xfer < len) {
					/* The mbuf list must be split. */
					m2 = nfsm_split(mp, xfer);
					if (m2 != NULL)
						mp = m2;
					else {
						m_freem(mp);
						error = EIO;
					}
				}
				NFSCL_DEBUG(4, "mcopy len=%jd xfer=%jd\n",
				    (uintmax_t)len, (uintmax_t)xfer);
				/*
				 * Do last write to a mirrored DS with this
				 * thread.
				 */
				if (error == 0) {
					if (mirror < flp->nfsfl_mirrorcnt - 1)
						error = nfsio_writedsmir(vp,
						    iomode, must_commit,
						    stateidp, *dspp, off,
						    xfer, fhp, m,
						    dp->nfsdi_vers,
						    dp->nfsdi_minorvers, drpc,
						    tcred, p);
					else
						error = nfsrpc_writedsmir(vp,
						    iomode, must_commit,
						    stateidp, *dspp, off,
						    xfer, fhp, m,
						    dp->nfsdi_vers,
						    dp->nfsdi_minorvers, tcred,
						    p);
				}
				NFSCL_DEBUG(4, "nfsio_writedsmir=%d\n", error);
				if (error != 0 && error != EACCES && error !=
				    ESTALE) {
					NFSCL_DEBUG(4,
					    "DS layreterr for write\n");
					nfscl_dserr(NFSV4OP_WRITE, error, dp,
					    lyp, *dspp);
				}
			}
		}
		NFSCL_DEBUG(4, "aft read/writeds=%d\n", error);
		if (error == 0) {
			len -= xfer;
			off += xfer;
		}
		if ((dp->nfsdi_flags & NFSDI_TIGHTCOUPLED) == 0)
			NFSFREECRED(tcred);
	}
	NFSCL_DEBUG(4, "eo nfscl_dofflayoutio=%d\n", error);
	return (error);
}

/*
 * The actual read RPC done to a DS.
 */
static int
nfsrpc_readds(vnode_t vp, struct uio *uiop, nfsv4stateid_t *stateidp, int *eofp,
    struct nfsclds *dsp, uint64_t io_off, int len, struct nfsfh *fhp, int flex,
    int vers, int minorvers, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	int attrflag, error, retlen;
	struct nfsrv_descript nfsd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsrv_descript *nd = &nfsd;
	struct nfssockreq *nrp;
	struct nfsvattr na;

	nd->nd_mrep = NULL;
	if (vers == 0 || vers == NFS_VER4) {
		nfscl_reqstart(nd, NFSPROC_READDS, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		vers = NFS_VER4;
		NFSCL_DEBUG(4, "nfsrpc_readds: vers4 minvers=%d\n", minorvers);
		if (flex != 0)
			nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
		else
			nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSEQIDZERO);
	} else {
		nfscl_reqstart(nd, NFSPROC_READ, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		NFSDECRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_READ]);
		NFSINCRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_READDS]);
		NFSCL_DEBUG(4, "nfsrpc_readds: vers3\n");
	}
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED * 3);
	txdr_hyper(io_off, tl);
	*(tl + 2) = txdr_unsigned(len);
	nrp = dsp->nfsclds_sockp;
	NFSCL_DEBUG(4, "nfsrpc_readds: nrp=%p\n", nrp);
	if (nrp == NULL)
		/* If NULL, use the MDS socket. */
		nrp = &nmp->nm_sockreq;
	error = newnfs_request(nd, nmp, NULL, nrp, vp, p, cred,
	    NFS_PROG, vers, NULL, 1, NULL, &dsp->nfsclds_sess);
	NFSCL_DEBUG(4, "nfsrpc_readds: stat=%d err=%d\n", nd->nd_repstat,
	    error);
	if (error != 0)
		return (error);
	if (vers == NFS_VER3) {
		error = nfscl_postop_attr(nd, &na, &attrflag);
		NFSCL_DEBUG(4, "nfsrpc_readds: postop=%d\n", error);
		if (error != 0)
			goto nfsmout;
	}
	if (nd->nd_repstat != 0) {
		error = nd->nd_repstat;
		goto nfsmout;
	}
	if (vers == NFS_VER3) {
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		*eofp = fxdr_unsigned(int, *(tl + 1));
	} else {
		NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		*eofp = fxdr_unsigned(int, *tl);
	}
	NFSM_STRSIZ(retlen, len);
	NFSCL_DEBUG(4, "nfsrpc_readds: retlen=%d eof=%d\n", retlen, *eofp);
	error = nfsm_mbufuio(nd, uiop, retlen);
nfsmout:
	if (nd->nd_mrep != NULL)
		m_freem(nd->nd_mrep);
	return (error);
}

/*
 * The actual write RPC done to a DS.
 */
static int
nfsrpc_writeds(vnode_t vp, struct uio *uiop, int *iomode, int *must_commit,
    nfsv4stateid_t *stateidp, struct nfsclds *dsp, uint64_t io_off, int len,
    struct nfsfh *fhp, int commit_thru_mds, int flex, int vers, int minorvers,
    struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int attrflag, error, rlen, commit, committed = NFSWRITE_FILESYNC;
	int32_t backup;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfssockreq *nrp;
	struct nfsvattr na;

	KASSERT(uiop->uio_iovcnt == 1, ("nfs: writerpc iovcnt > 1"));
	nd->nd_mrep = NULL;
	if (vers == 0 || vers == NFS_VER4) {
		nfscl_reqstart(nd, NFSPROC_WRITEDS, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		NFSCL_DEBUG(4, "nfsrpc_writeds: vers4 minvers=%d\n", minorvers);
		vers = NFS_VER4;
		if (flex != 0)
			nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
		else
			nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSEQIDZERO);
		NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + 2 * NFSX_UNSIGNED);
	} else {
		nfscl_reqstart(nd, NFSPROC_WRITE, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		NFSDECRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_WRITE]);
		NFSINCRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_WRITEDS]);
		NFSCL_DEBUG(4, "nfsrpc_writeds: vers3\n");
		NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + 3 * NFSX_UNSIGNED);
	}
	txdr_hyper(io_off, tl);
	tl += 2;
	if (vers == NFS_VER3)
		*tl++ = txdr_unsigned(len);
	*tl++ = txdr_unsigned(*iomode);
	*tl = txdr_unsigned(len);
	error = nfsm_uiombuf(nd, uiop, len);
	if (error != 0) {
		m_freem(nd->nd_mreq);
		return (error);
	}
	nrp = dsp->nfsclds_sockp;
	if (nrp == NULL)
		/* If NULL, use the MDS socket. */
		nrp = &nmp->nm_sockreq;
	error = newnfs_request(nd, nmp, NULL, nrp, vp, p, cred,
	    NFS_PROG, vers, NULL, 1, NULL, &dsp->nfsclds_sess);
	NFSCL_DEBUG(4, "nfsrpc_writeds: err=%d stat=%d\n", error,
	    nd->nd_repstat);
	if (error != 0)
		return (error);
	if (nd->nd_repstat != 0) {
		/*
		 * In case the rpc gets retried, roll
		 * the uio fields changed by nfsm_uiombuf()
		 * back.
		 */
		uiop->uio_offset -= len;
		uiop->uio_resid += len;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base - len;
		uiop->uio_iov->iov_len += len;
		error = nd->nd_repstat;
	} else {
		if (vers == NFS_VER3) {
			error = nfscl_wcc_data(nd, vp, &na, &attrflag, NULL,
			    NULL);
			NFSCL_DEBUG(4, "nfsrpc_writeds: wcc_data=%d\n", error);
			if (error != 0)
				goto nfsmout;
		}
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED + NFSX_VERF);
		rlen = fxdr_unsigned(int, *tl++);
		NFSCL_DEBUG(4, "nfsrpc_writeds: len=%d rlen=%d\n", len, rlen);
		if (rlen == 0) {
			error = NFSERR_IO;
			goto nfsmout;
		} else if (rlen < len) {
			backup = len - rlen;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base - backup;
			uiop->uio_iov->iov_len += backup;
			uiop->uio_offset -= backup;
			uiop->uio_resid += backup;
			len = rlen;
		}
		commit = fxdr_unsigned(int, *tl++);

		/*
		 * Return the lowest commitment level
		 * obtained by any of the RPCs.
		 */
		if (committed == NFSWRITE_FILESYNC)
			committed = commit;
		else if (committed == NFSWRITE_DATASYNC &&
		    commit == NFSWRITE_UNSTABLE)
			committed = commit;
		if (commit_thru_mds != 0) {
			NFSLOCKMNT(nmp);
			if (!NFSHASWRITEVERF(nmp)) {
				NFSBCOPY(tl, nmp->nm_verf, NFSX_VERF);
				NFSSETWRITEVERF(nmp);
			} else if (NFSBCMP(tl, nmp->nm_verf, NFSX_VERF) &&
			    *must_commit != 2) {
				*must_commit = 1;
				NFSBCOPY(tl, nmp->nm_verf, NFSX_VERF);
			}
			NFSUNLOCKMNT(nmp);
		} else {
			NFSLOCKDS(dsp);
			if ((dsp->nfsclds_flags & NFSCLDS_HASWRITEVERF) == 0) {
				NFSBCOPY(tl, dsp->nfsclds_verf, NFSX_VERF);
				dsp->nfsclds_flags |= NFSCLDS_HASWRITEVERF;
			} else if (NFSBCMP(tl, dsp->nfsclds_verf, NFSX_VERF) &&
			    *must_commit != 2) {
				*must_commit = 1;
				NFSBCOPY(tl, dsp->nfsclds_verf, NFSX_VERF);
			}
			NFSUNLOCKDS(dsp);
		}
	}
nfsmout:
	if (nd->nd_mrep != NULL)
		m_freem(nd->nd_mrep);
	*iomode = committed;
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
	return (error);
}

/*
 * The actual write RPC done to a DS.
 * This variant is called from a separate kernel process for mirrors.
 * Any short write is considered an IO error.
 */
static int
nfsrpc_writedsmir(vnode_t vp, int *iomode, int *must_commit,
    nfsv4stateid_t *stateidp, struct nfsclds *dsp, uint64_t io_off, int len,
    struct nfsfh *fhp, struct mbuf *m, int vers, int minorvers,
    struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int attrflag, error, commit, committed = NFSWRITE_FILESYNC, rlen;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfssockreq *nrp;
	struct nfsvattr na;

	nd->nd_mrep = NULL;
	if (vers == 0 || vers == NFS_VER4) {
		nfscl_reqstart(nd, NFSPROC_WRITEDS, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		vers = NFS_VER4;
		NFSCL_DEBUG(4, "nfsrpc_writedsmir: vers4 minvers=%d\n",
		    minorvers);
		nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
		NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + 2 * NFSX_UNSIGNED);
	} else {
		nfscl_reqstart(nd, NFSPROC_WRITE, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		NFSDECRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_WRITE]);
		NFSINCRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_WRITEDS]);
		NFSCL_DEBUG(4, "nfsrpc_writedsmir: vers3\n");
		NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + 3 * NFSX_UNSIGNED);
	}
	txdr_hyper(io_off, tl);
	tl += 2;
	if (vers == NFS_VER3)
		*tl++ = txdr_unsigned(len);
	*tl++ = txdr_unsigned(*iomode);
	*tl = txdr_unsigned(len);
	if (len > 0) {
		/* Put data in mbuf chain. */
		nd->nd_mb->m_next = m;
	}
	nrp = dsp->nfsclds_sockp;
	if (nrp == NULL)
		/* If NULL, use the MDS socket. */
		nrp = &nmp->nm_sockreq;
	error = newnfs_request(nd, nmp, NULL, nrp, vp, p, cred,
	    NFS_PROG, vers, NULL, 1, NULL, &dsp->nfsclds_sess);
	NFSCL_DEBUG(4, "nfsrpc_writedsmir: err=%d stat=%d\n", error,
	    nd->nd_repstat);
	if (error != 0)
		return (error);
	if (nd->nd_repstat != 0)
		error = nd->nd_repstat;
	else {
		if (vers == NFS_VER3) {
			error = nfscl_wcc_data(nd, vp, &na, &attrflag, NULL,
			    NULL);
			NFSCL_DEBUG(4, "nfsrpc_writedsmir: wcc_data=%d\n",
			    error);
			if (error != 0)
				goto nfsmout;
		}
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED + NFSX_VERF);
		rlen = fxdr_unsigned(int, *tl++);
		NFSCL_DEBUG(4, "nfsrpc_writedsmir: len=%d rlen=%d\n", len,
		    rlen);
		if (rlen != len) {
			error = NFSERR_IO;
			NFSCL_DEBUG(4, "nfsrpc_writedsmir: len=%d rlen=%d\n",
			    len, rlen);
			goto nfsmout;
		}
		commit = fxdr_unsigned(int, *tl++);

		/*
		 * Return the lowest commitment level
		 * obtained by any of the RPCs.
		 */
		if (committed == NFSWRITE_FILESYNC)
			committed = commit;
		else if (committed == NFSWRITE_DATASYNC &&
		    commit == NFSWRITE_UNSTABLE)
			committed = commit;
		NFSLOCKDS(dsp);
		if ((dsp->nfsclds_flags & NFSCLDS_HASWRITEVERF) == 0) {
			NFSBCOPY(tl, dsp->nfsclds_verf, NFSX_VERF);
			dsp->nfsclds_flags |= NFSCLDS_HASWRITEVERF;
		} else if (NFSBCMP(tl, dsp->nfsclds_verf, NFSX_VERF) &&
		    *must_commit != 2) {
			*must_commit = 1;
			NFSBCOPY(tl, dsp->nfsclds_verf, NFSX_VERF);
		}
		NFSUNLOCKDS(dsp);
	}
nfsmout:
	if (nd->nd_mrep != NULL)
		m_freem(nd->nd_mrep);
	*iomode = committed;
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
	return (error);
}

/*
 * Start up the thread that will execute nfsrpc_writedsmir().
 */
static void
start_writedsmir(void *arg, int pending)
{
	struct nfsclwritedsdorpc *drpc;

	drpc = (struct nfsclwritedsdorpc *)arg;
	drpc->err = nfsrpc_writedsmir(drpc->vp, &drpc->iomode,
	    &drpc->must_commit, drpc->stateidp, drpc->dsp, drpc->off, drpc->len,
	    drpc->fhp, drpc->m, drpc->vers, drpc->minorvers, drpc->cred,
	    drpc->p);
	drpc->done = 1;
	crfree(drpc->cred);
	NFSCL_DEBUG(4, "start_writedsmir: err=%d\n", drpc->err);
}

/*
 * Set up the write DS mirror call for the pNFS I/O thread.
 */
static int
nfsio_writedsmir(vnode_t vp, int *iomode, int *must_commit,
    nfsv4stateid_t *stateidp, struct nfsclds *dsp, uint64_t off, int len,
    struct nfsfh *fhp, struct mbuf *m, int vers, int minorvers,
    struct nfsclwritedsdorpc *drpc, struct ucred *cred, NFSPROC_T *p)
{
	int error, ret;

	error = 0;
	drpc->done = 0;
	drpc->vp = vp;
	drpc->iomode = *iomode;
	drpc->must_commit = *must_commit;
	drpc->stateidp = stateidp;
	drpc->dsp = dsp;
	drpc->off = off;
	drpc->len = len;
	drpc->fhp = fhp;
	drpc->m = m;
	drpc->vers = vers;
	drpc->minorvers = minorvers;
	drpc->cred = crhold(cred);
	drpc->p = p;
	drpc->inprog = 0;
	ret = EIO;
	if (nfs_pnfsiothreads != 0) {
		ret = nfs_pnfsio(start_writedsmir, drpc);
		NFSCL_DEBUG(4, "nfsio_writedsmir: nfs_pnfsio=%d\n", ret);
	}
	if (ret != 0) {
		error = nfsrpc_writedsmir(vp, iomode, &drpc->must_commit,
		    stateidp, dsp, off, len, fhp, m, vers, minorvers, cred, p);
		crfree(drpc->cred);
	}
	NFSCL_DEBUG(4, "nfsio_writedsmir: error=%d\n", error);
	return (error);
}

/*
 * Free up the nfsclds structure.
 */
void
nfscl_freenfsclds(struct nfsclds *dsp)
{
	int i;

	if (dsp == NULL)
		return;
	if (dsp->nfsclds_sockp != NULL) {
		NFSFREECRED(dsp->nfsclds_sockp->nr_cred);
		NFSFREEMUTEX(&dsp->nfsclds_sockp->nr_mtx);
		free(dsp->nfsclds_sockp->nr_nam, M_SONAME);
		free(dsp->nfsclds_sockp, M_NFSSOCKREQ);
	}
	NFSFREEMUTEX(&dsp->nfsclds_mtx);
	NFSFREEMUTEX(&dsp->nfsclds_sess.nfsess_mtx);
	for (i = 0; i < NFSV4_CBSLOTS; i++) {
		if (dsp->nfsclds_sess.nfsess_cbslots[i].nfssl_reply != NULL)
			m_freem(
			    dsp->nfsclds_sess.nfsess_cbslots[i].nfssl_reply);
	}
	free(dsp, M_NFSCLDS);
}

static enum nfsclds_state
nfscl_getsameserver(struct nfsmount *nmp, struct nfsclds *newdsp,
    struct nfsclds **retdspp, uint32_t *sequencep)
{
	struct nfsclds *dsp;
	int fndseq;

	/*
	 * Search the list of nfsclds structures for one with the same
	 * server.
	 */
	fndseq = 0;
	TAILQ_FOREACH(dsp, &nmp->nm_sess, nfsclds_list) {
		if (dsp->nfsclds_servownlen == newdsp->nfsclds_servownlen &&
		    dsp->nfsclds_servownlen != 0 &&
		    !NFSBCMP(dsp->nfsclds_serverown, newdsp->nfsclds_serverown,
		    dsp->nfsclds_servownlen) &&
		    dsp->nfsclds_sess.nfsess_defunct == 0) {
			NFSCL_DEBUG(4, "fnd same fdsp=%p dsp=%p flg=0x%x\n",
			    TAILQ_FIRST(&nmp->nm_sess), dsp,
			    dsp->nfsclds_flags);
			if (fndseq == 0) {
				/* Get sequenceid# from first entry. */
				*sequencep =
				    dsp->nfsclds_sess.nfsess_sequenceid;
				fndseq = 1;
			}
			/* Server major id matches. */
			if ((dsp->nfsclds_flags & NFSCLDS_DS) != 0) {
				*retdspp = dsp;
				return (NFSDSP_USETHISSESSION);
			}
		}
	}
	if (fndseq != 0)
		return (NFSDSP_SEQTHISSESSION);
	return (NFSDSP_NOTFOUND);
}

/*
 * NFS commit rpc to a NFSv4.1 DS.
 */
static int
nfsrpc_commitds(vnode_t vp, uint64_t offset, int cnt, struct nfsclds *dsp,
    struct nfsfh *fhp, int vers, int minorvers, struct ucred *cred,
    NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfssockreq *nrp;
	struct nfsvattr na;
	int attrflag, error;

	nd->nd_mrep = NULL;
	if (vers == 0 || vers == NFS_VER4) {
		nfscl_reqstart(nd, NFSPROC_COMMITDS, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		vers = NFS_VER4;
	} else {
		nfscl_reqstart(nd, NFSPROC_COMMIT, nmp, fhp->nfh_fh,
		    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers,
		    NULL);
		NFSDECRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_COMMIT]);
		NFSINCRGLOBAL(nfsstatsv1.rpccnt[NFSPROC_COMMITDS]);
	}
	NFSCL_DEBUG(4, "nfsrpc_commitds: vers=%d minvers=%d\n", vers,
	    minorvers);
	NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);
	nrp = dsp->nfsclds_sockp;
	if (nrp == NULL)
		/* If NULL, use the MDS socket. */
		nrp = &nmp->nm_sockreq;
	error = newnfs_request(nd, nmp, NULL, nrp, vp, p, cred,
	    NFS_PROG, vers, NULL, 1, NULL, &dsp->nfsclds_sess);
	NFSCL_DEBUG(4, "nfsrpc_commitds: err=%d stat=%d\n", error,
	    nd->nd_repstat);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		if (vers == NFS_VER3) {
			error = nfscl_wcc_data(nd, vp, &na, &attrflag, NULL,
			    NULL);
			NFSCL_DEBUG(4, "nfsrpc_commitds: wccdata=%d\n", error);
			if (error != 0)
				goto nfsmout;
		}
		NFSM_DISSECT(tl, u_int32_t *, NFSX_VERF);
		NFSLOCKDS(dsp);
		if (NFSBCMP(tl, dsp->nfsclds_verf, NFSX_VERF)) {
			NFSBCOPY(tl, dsp->nfsclds_verf, NFSX_VERF);
			error = NFSERR_STALEWRITEVERF;
		}
		NFSUNLOCKDS(dsp);
	}
nfsmout:
	if (error == 0 && nd->nd_repstat != 0)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Start up the thread that will execute nfsrpc_commitds().
 */
static void
start_commitds(void *arg, int pending)
{
	struct nfsclwritedsdorpc *drpc;

	drpc = (struct nfsclwritedsdorpc *)arg;
	drpc->err = nfsrpc_commitds(drpc->vp, drpc->off, drpc->len,
	    drpc->dsp, drpc->fhp, drpc->vers, drpc->minorvers, drpc->cred,
	    drpc->p);
	drpc->done = 1;
	crfree(drpc->cred);
	NFSCL_DEBUG(4, "start_commitds: err=%d\n", drpc->err);
}

/*
 * Set up the commit DS mirror call for the pNFS I/O thread.
 */
static int
nfsio_commitds(vnode_t vp, uint64_t offset, int cnt, struct nfsclds *dsp,
    struct nfsfh *fhp, int vers, int minorvers,
    struct nfsclwritedsdorpc *drpc, struct ucred *cred, NFSPROC_T *p)
{
	int error, ret;

	error = 0;
	drpc->done = 0;
	drpc->vp = vp;
	drpc->off = offset;
	drpc->len = cnt;
	drpc->dsp = dsp;
	drpc->fhp = fhp;
	drpc->vers = vers;
	drpc->minorvers = minorvers;
	drpc->cred = crhold(cred);
	drpc->p = p;
	drpc->inprog = 0;
	ret = EIO;
	if (nfs_pnfsiothreads != 0) {
		ret = nfs_pnfsio(start_commitds, drpc);
		NFSCL_DEBUG(4, "nfsio_commitds: nfs_pnfsio=%d\n", ret);
	}
	if (ret != 0) {
		error = nfsrpc_commitds(vp, offset, cnt, dsp, fhp, vers,
		    minorvers, cred, p);
		crfree(drpc->cred);
	}
	NFSCL_DEBUG(4, "nfsio_commitds: error=%d\n", error);
	return (error);
}

/*
 * NFS Advise rpc
 */
int
nfsrpc_advise(vnode_t vp, off_t offset, uint64_t cnt, int advise,
    struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	nfsattrbit_t hints;
	int error;

	NFSZERO_ATTRBIT(&hints);
	if (advise == POSIX_FADV_WILLNEED)
		NFSSETBIT_ATTRBIT(&hints, NFSV4IOHINT_WILLNEED);
	else if (advise == POSIX_FADV_DONTNEED)
		NFSSETBIT_ATTRBIT(&hints, NFSV4IOHINT_DONTNEED);
	else
		return (0);
	NFSCL_REQSTART(nd, NFSPROC_IOADVISE, vp, cred);
	nfsm_stateidtom(nd, NULL, NFSSTATEID_PUTALLZERO);
	NFSM_BUILD(tl, uint32_t *, 2 * NFSX_HYPER);
	txdr_hyper(offset, tl);
	tl += 2;
	txdr_hyper(cnt, tl);
	nfsrv_putattrbit(nd, &hints);
	error = nfscl_request(nd, vp, p, cred);
	if (error != 0)
		return (error);
	if (nd->nd_repstat != 0)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

#ifdef notyet
/*
 * NFS advise rpc to a NFSv4.2 DS.
 */
static int
nfsrpc_adviseds(vnode_t vp, uint64_t offset, int cnt, int advise,
    struct nfsclds *dsp, struct nfsfh *fhp, int vers, int minorvers,
    struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfssockreq *nrp;
	nfsattrbit_t hints;
	int error;

	/* For NFS DSs prior to NFSv4.2, just return OK. */
	if (vers == NFS_VER3 || minorversion < NFSV42_MINORVERSION)
		return (0);
	NFSZERO_ATTRBIT(&hints);
	if (advise == POSIX_FADV_WILLNEED)
		NFSSETBIT_ATTRBIT(&hints, NFSV4IOHINT_WILLNEED);
	else if (advise == POSIX_FADV_DONTNEED)
		NFSSETBIT_ATTRBIT(&hints, NFSV4IOHINT_DONTNEED);
	else
		return (0);
	nd->nd_mrep = NULL;
	nfscl_reqstart(nd, NFSPROC_IOADVISEDS, nmp, fhp->nfh_fh,
	    fhp->nfh_len, NULL, &dsp->nfsclds_sess, vers, minorvers, NULL);
	vers = NFS_VER4;
	NFSCL_DEBUG(4, "nfsrpc_adviseds: vers=%d minvers=%d\n", vers,
	    minorvers);
	nfsm_stateidtom(nd, NULL, NFSSTATEID_PUTALLZERO);
	NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);
	nfsrv_putattrbit(nd, &hints);
	nrp = dsp->nfsclds_sockp;
	if (nrp == NULL)
		/* If NULL, use the MDS socket. */
		nrp = &nmp->nm_sockreq;
	error = newnfs_request(nd, nmp, NULL, nrp, vp, p, cred,
	    NFS_PROG, vers, NULL, 1, NULL, &dsp->nfsclds_sess);
	NFSCL_DEBUG(4, "nfsrpc_adviseds: err=%d stat=%d\n", error,
	    nd->nd_repstat);
	if (error != 0)
		return (error);
	if (nd->nd_repstat != 0)
		error = nd->nd_repstat;
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Start up the thread that will execute nfsrpc_commitds().
 */
static void
start_adviseds(void *arg, int pending)
{
	struct nfsclwritedsdorpc *drpc;

	drpc = (struct nfsclwritedsdorpc *)arg;
	drpc->err = nfsrpc_adviseds(drpc->vp, drpc->off, drpc->len,
	    drpc->advise, drpc->dsp, drpc->fhp, drpc->vers, drpc->minorvers,
	    drpc->cred, drpc->p);
	drpc->done = 1;
	crfree(drpc->cred);
	NFSCL_DEBUG(4, "start_adviseds: err=%d\n", drpc->err);
}

/*
 * Set up the advise DS mirror call for the pNFS I/O thread.
 */
static int
nfsio_adviseds(vnode_t vp, uint64_t offset, int cnt, int advise,
    struct nfsclds *dsp, struct nfsfh *fhp, int vers, int minorvers,
    struct nfsclwritedsdorpc *drpc, struct ucred *cred, NFSPROC_T *p)
{
	int error, ret;

	error = 0;
	drpc->done = 0;
	drpc->vp = vp;
	drpc->off = offset;
	drpc->len = cnt;
	drpc->advise = advise;
	drpc->dsp = dsp;
	drpc->fhp = fhp;
	drpc->vers = vers;
	drpc->minorvers = minorvers;
	drpc->cred = crhold(cred);
	drpc->p = p;
	drpc->inprog = 0;
	ret = EIO;
	if (nfs_pnfsiothreads != 0) {
		ret = nfs_pnfsio(start_adviseds, drpc);
		NFSCL_DEBUG(4, "nfsio_adviseds: nfs_pnfsio=%d\n", ret);
	}
	if (ret != 0) {
		error = nfsrpc_adviseds(vp, offset, cnt, advise, dsp, fhp, vers,
		    minorvers, cred, p);
		crfree(drpc->cred);
	}
	NFSCL_DEBUG(4, "nfsio_adviseds: error=%d\n", error);
	return (error);
}
#endif	/* notyet */

/*
 * Do the Allocate operation, retrying for recovery.
 */
int
nfsrpc_allocate(vnode_t vp, off_t off, off_t len, struct nfsvattr *nap,
    int *attrflagp, struct ucred *cred, NFSPROC_T *p)
{
	int error, expireret = 0, retrycnt, nostateid;
	uint32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsfh *nfhp = NULL;
	nfsv4stateid_t stateid;
	off_t tmp_off;
	void *lckp;

	if (len < 0)
		return (EINVAL);
	if (len == 0)
		return (0);
	tmp_off = off + len;
	NFSLOCKMNT(nmp);
	if (tmp_off > nmp->nm_maxfilesize || tmp_off < off) {
		NFSUNLOCKMNT(nmp);
		return (EFBIG);
	}
	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	NFSUNLOCKMNT(nmp);
	nfhp = VTONFS(vp)->n_fhp;
	retrycnt = 0;
	do {
		lckp = NULL;
		nostateid = 0;
		nfscl_getstateid(vp, nfhp->nfh_fh, nfhp->nfh_len,
		    NFSV4OPEN_ACCESSWRITE, 0, cred, p, &stateid, &lckp);
		if (stateid.other[0] == 0 && stateid.other[1] == 0 &&
		    stateid.other[2] == 0) {
			nostateid = 1;
			NFSCL_DEBUG(1, "stateid0 in allocate\n");
		}

		/*
		 * Not finding a stateid should probably never happen,
		 * but just return an error for this case.
		 */
		if (nostateid != 0)
			error = EIO;
		else
			error = nfsrpc_allocaterpc(vp, off, len, &stateid,
			    nap, attrflagp, cred, p);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID || error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_allocate");
		} else if ((error == NFSERR_EXPIRED || (!NFSHASINT(nmp) &&
		    error == NFSERR_BADSTATEID)) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev, p);
		} else if (error == NFSERR_BADSTATEID && NFSHASINT(nmp)) {
			error = EIO;
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_DELAY ||
	    error == NFSERR_STALESTATEID || error == NFSERR_BADSESSION ||
	    error == NFSERR_STALEDONTRECOVER ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error != 0 && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * The allocate RPC.
 */
static int
nfsrpc_allocaterpc(vnode_t vp, off_t off, off_t len, nfsv4stateid_t *stateidp,
    struct nfsvattr *nap, int *attrflagp, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	int error;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_ALLOCATE, vp, cred);
	nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
	NFSM_BUILD(tl, uint32_t *, 2 * NFSX_HYPER + NFSX_UNSIGNED);
	txdr_hyper(off, tl); tl += 2;
	txdr_hyper(len, tl); tl += 2;
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		error = nfsm_loadattr(nd, nap);
		if (error == 0)
			*attrflagp = NFS_LATTR_NOSHRINK;
	} else
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Set up the XDR arguments for the LayoutGet operation.
 */
static void
nfsrv_setuplayoutget(struct nfsrv_descript *nd, int iomode, uint64_t offset,
    uint64_t len, uint64_t minlen, nfsv4stateid_t *stateidp, int layouttype,
    int layoutlen, int usecurstateid)
{
	uint32_t *tl;

	NFSM_BUILD(tl, uint32_t *, 4 * NFSX_UNSIGNED + 3 * NFSX_HYPER +
	    NFSX_STATEID);
	*tl++ = newnfs_false;		/* Don't signal availability. */
	*tl++ = txdr_unsigned(layouttype);
	*tl++ = txdr_unsigned(iomode);
	txdr_hyper(offset, tl);
	tl += 2;
	txdr_hyper(len, tl);
	tl += 2;
	txdr_hyper(minlen, tl);
	tl += 2;
	if (usecurstateid != 0) {
		/* Special stateid for Current stateid. */
		*tl++ = txdr_unsigned(1);
		*tl++ = 0;
		*tl++ = 0;
		*tl++ = 0;
	} else {
		*tl++ = txdr_unsigned(stateidp->seqid);
		NFSCL_DEBUG(4, "layget seq=%d\n", (int)stateidp->seqid);
		*tl++ = stateidp->other[0];
		*tl++ = stateidp->other[1];
		*tl++ = stateidp->other[2];
	}
	*tl = txdr_unsigned(layoutlen);
}

/*
 * Parse the reply for a successful LayoutGet operation.
 */
static int
nfsrv_parselayoutget(struct nfsmount *nmp, struct nfsrv_descript *nd,
    nfsv4stateid_t *stateidp, int *retonclosep, struct nfsclflayouthead *flhp)
{
	uint32_t *tl;
	struct nfsclflayout *flp, *prevflp, *tflp;
	int cnt, error, fhcnt, gotiomode, i, iomode, j, k, l, laytype, nfhlen;
	int m, mirrorcnt;
	uint64_t retlen, off;
	struct nfsfh *nfhp;
	uint8_t *cp;
	uid_t user;
	gid_t grp;

	NFSCL_DEBUG(4, "in nfsrv_parselayoutget\n");
	error = 0;
	flp = NULL;
	gotiomode = -1;
	NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED + NFSX_STATEID);
	if (*tl++ != 0)
		*retonclosep = 1;
	else
		*retonclosep = 0;
	stateidp->seqid = fxdr_unsigned(uint32_t, *tl++);
	NFSCL_DEBUG(4, "retoncls=%d stseq=%d\n", *retonclosep,
	    (int)stateidp->seqid);
	stateidp->other[0] = *tl++;
	stateidp->other[1] = *tl++;
	stateidp->other[2] = *tl++;
	cnt = fxdr_unsigned(int, *tl);
	NFSCL_DEBUG(4, "layg cnt=%d\n", cnt);
	if (cnt <= 0 || cnt > 10000) {
		/* Don't accept more than 10000 layouts in reply. */
		error = NFSERR_BADXDR;
		goto nfsmout;
	}
	for (i = 0; i < cnt; i++) {
		/* Dissect to the layout type. */
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_HYPER +
		    3 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl); tl += 2;
		retlen = fxdr_hyper(tl); tl += 2;
		iomode = fxdr_unsigned(int, *tl++);
		laytype = fxdr_unsigned(int, *tl);
		NFSCL_DEBUG(4, "layt=%d off=%ju len=%ju iom=%d\n", laytype,
		    (uintmax_t)off, (uintmax_t)retlen, iomode);
		/* Ignore length of layout body for now. */
		if (laytype == NFSLAYOUT_NFSV4_1_FILES) {
			/* Parse the File layout up to fhcnt. */
			NFSM_DISSECT(tl, uint32_t *, 3 * NFSX_UNSIGNED +
			    NFSX_HYPER + NFSX_V4DEVICEID);
			fhcnt = fxdr_unsigned(int, *(tl + 4 +
			    NFSX_V4DEVICEID / NFSX_UNSIGNED));
			NFSCL_DEBUG(4, "fhcnt=%d\n", fhcnt);
			if (fhcnt < 0 || fhcnt > 100) {
				/* Don't accept more than 100 file handles. */
				error = NFSERR_BADXDR;
				goto nfsmout;
			}
			if (fhcnt > 0)
				flp = malloc(sizeof(*flp) + fhcnt *
				    sizeof(struct nfsfh *), M_NFSFLAYOUT,
				    M_WAITOK);
			else
				flp = malloc(sizeof(*flp), M_NFSFLAYOUT,
				    M_WAITOK);
			flp->nfsfl_flags = NFSFL_FILE;
			flp->nfsfl_fhcnt = 0;
			flp->nfsfl_devp = NULL;
			flp->nfsfl_off = off;
			if (flp->nfsfl_off + retlen < flp->nfsfl_off)
				flp->nfsfl_end = UINT64_MAX - flp->nfsfl_off;
			else
				flp->nfsfl_end = flp->nfsfl_off + retlen;
			flp->nfsfl_iomode = iomode;
			if (gotiomode == -1)
				gotiomode = flp->nfsfl_iomode;
			/* Ignore layout body length for now. */
			NFSBCOPY(tl, flp->nfsfl_dev, NFSX_V4DEVICEID);
			tl += (NFSX_V4DEVICEID / NFSX_UNSIGNED);
			flp->nfsfl_util = fxdr_unsigned(uint32_t, *tl++);
			NFSCL_DEBUG(4, "flutil=0x%x\n", flp->nfsfl_util);
			mtx_lock(&nmp->nm_mtx);
			if (nmp->nm_minorvers > 1 && (flp->nfsfl_util &
			    NFSFLAYUTIL_IOADVISE_THRU_MDS) != 0)
				nmp->nm_privflag |= NFSMNTP_IOADVISETHRUMDS;
			mtx_unlock(&nmp->nm_mtx);
			flp->nfsfl_stripe1 = fxdr_unsigned(uint32_t, *tl++);
			flp->nfsfl_patoff = fxdr_hyper(tl); tl += 2;
			NFSCL_DEBUG(4, "stripe1=%u poff=%ju\n",
			    flp->nfsfl_stripe1, (uintmax_t)flp->nfsfl_patoff);
			for (j = 0; j < fhcnt; j++) {
				NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
				nfhlen = fxdr_unsigned(int, *tl);
				if (nfhlen <= 0 || nfhlen > NFSX_V4FHMAX) {
					error = NFSERR_BADXDR;
					goto nfsmout;
				}
				nfhp = malloc(sizeof(*nfhp) + nfhlen - 1,
				    M_NFSFH, M_WAITOK);
				flp->nfsfl_fh[j] = nfhp;
				flp->nfsfl_fhcnt++;
				nfhp->nfh_len = nfhlen;
				NFSM_DISSECT(cp, uint8_t *, NFSM_RNDUP(nfhlen));
				NFSBCOPY(cp, nfhp->nfh_fh, nfhlen);
			}
		} else if (laytype == NFSLAYOUT_FLEXFILE) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED +
			    NFSX_HYPER);
			mirrorcnt = fxdr_unsigned(int, *(tl + 2));
			NFSCL_DEBUG(4, "mirrorcnt=%d\n", mirrorcnt);
			if (mirrorcnt < 1 || mirrorcnt > NFSDEV_MAXMIRRORS) {
				error = NFSERR_BADXDR;
				goto nfsmout;
			}
			flp = malloc(sizeof(*flp) + mirrorcnt *
			    sizeof(struct nfsffm), M_NFSFLAYOUT, M_WAITOK);
			flp->nfsfl_flags = NFSFL_FLEXFILE;
			flp->nfsfl_mirrorcnt = mirrorcnt;
			for (j = 0; j < mirrorcnt; j++)
				flp->nfsfl_ffm[j].devp = NULL;
			flp->nfsfl_off = off;
			if (flp->nfsfl_off + retlen < flp->nfsfl_off)
				flp->nfsfl_end = UINT64_MAX - flp->nfsfl_off;
			else
				flp->nfsfl_end = flp->nfsfl_off + retlen;
			flp->nfsfl_iomode = iomode;
			if (gotiomode == -1)
				gotiomode = flp->nfsfl_iomode;
			flp->nfsfl_stripeunit = fxdr_hyper(tl);
			NFSCL_DEBUG(4, "stripeunit=%ju\n",
			    (uintmax_t)flp->nfsfl_stripeunit);
			for (j = 0; j < mirrorcnt; j++) {
				NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
				k = fxdr_unsigned(int, *tl);
				if (k < 1 || k > 128) {
					error = NFSERR_BADXDR;
					goto nfsmout;
				}
				NFSCL_DEBUG(4, "servercnt=%d\n", k);
				for (l = 0; l < k; l++) {
					NFSM_DISSECT(tl, uint32_t *,
					    NFSX_V4DEVICEID + NFSX_STATEID +
					    2 * NFSX_UNSIGNED);
					if (l == 0) {
						/* Just use the first server. */
						NFSBCOPY(tl,
						    flp->nfsfl_ffm[j].dev,
						    NFSX_V4DEVICEID);
						tl += (NFSX_V4DEVICEID /
						    NFSX_UNSIGNED);
						tl++;
						flp->nfsfl_ffm[j].st.seqid =
						    *tl++;
						flp->nfsfl_ffm[j].st.other[0] =
						    *tl++;
						flp->nfsfl_ffm[j].st.other[1] =
						    *tl++;
						flp->nfsfl_ffm[j].st.other[2] =
						    *tl++;
						NFSCL_DEBUG(4, "st.seqid=%u "
						 "st.o0=0x%x st.o1=0x%x "
						 "st.o2=0x%x\n",
						 flp->nfsfl_ffm[j].st.seqid,
						 flp->nfsfl_ffm[j].st.other[0],
						 flp->nfsfl_ffm[j].st.other[1],
						 flp->nfsfl_ffm[j].st.other[2]);
					} else
						tl += ((NFSX_V4DEVICEID +
						    NFSX_STATEID +
						    NFSX_UNSIGNED) /
						    NFSX_UNSIGNED);
					fhcnt = fxdr_unsigned(int, *tl);
					NFSCL_DEBUG(4, "fhcnt=%d\n", fhcnt);
					if (fhcnt < 1 ||
					    fhcnt > NFSDEV_MAXVERS) {
						error = NFSERR_BADXDR;
						goto nfsmout;
					}
					for (m = 0; m < fhcnt; m++) {
						NFSM_DISSECT(tl, uint32_t *,
						    NFSX_UNSIGNED);
						nfhlen = fxdr_unsigned(int,
						    *tl);
						NFSCL_DEBUG(4, "nfhlen=%d\n",
						    nfhlen);
						if (nfhlen <= 0 || nfhlen >
						    NFSX_V4FHMAX) {
							error = NFSERR_BADXDR;
							goto nfsmout;
						}
						NFSM_DISSECT(cp, uint8_t *,
						    NFSM_RNDUP(nfhlen));
						if (l == 0) {
							flp->nfsfl_ffm[j].fhcnt 
							    = fhcnt;
							nfhp = malloc(
							    sizeof(*nfhp) +
							    nfhlen - 1, M_NFSFH,
							    M_WAITOK);
							flp->nfsfl_ffm[j].fh[m]
							    = nfhp;
							nfhp->nfh_len = nfhlen;
							NFSBCOPY(cp,
							    nfhp->nfh_fh,
							    nfhlen);
							NFSCL_DEBUG(4,
							    "got fh\n");
						}
					}
					/* Now, get the ffsd_user/ffds_group. */
					error = nfsrv_parseug(nd, 0, &user,
					    &grp, curthread);
					NFSCL_DEBUG(4, "after parseu=%d\n",
					    error);
					if (error == 0)
						error = nfsrv_parseug(nd, 1,
						    &user, &grp, curthread);
					NFSCL_DEBUG(4, "aft parseg=%d\n",
					    grp);
					if (error != 0)
						goto nfsmout;
					NFSCL_DEBUG(4, "user=%d group=%d\n",
					    user, grp);
					if (l == 0) {
						flp->nfsfl_ffm[j].user = user;
						flp->nfsfl_ffm[j].group = grp;
						NFSCL_DEBUG(4,
						    "usr=%d grp=%d\n", user,
						    grp);
					}
				}
			}
			NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			flp->nfsfl_fflags = fxdr_unsigned(uint32_t, *tl++);
#ifdef notnow
			/*
			 * At this time, there is no flag.
			 * NFSFLEXFLAG_IOADVISE_THRU_MDS might need to be
			 * added, or it may never exist?
			 */
			mtx_lock(&nmp->nm_mtx);
			if (nmp->nm_minorvers > 1 && (flp->nfsfl_fflags &
			    NFSFLEXFLAG_IOADVISE_THRU_MDS) != 0)
				nmp->nm_privflag |= NFSMNTP_IOADVISETHRUMDS;
			mtx_unlock(&nmp->nm_mtx);
#endif
			flp->nfsfl_statshint = fxdr_unsigned(uint32_t, *tl);
			NFSCL_DEBUG(4, "fflags=0x%x statshint=%d\n",
			    flp->nfsfl_fflags, flp->nfsfl_statshint);
		} else {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		if (flp->nfsfl_iomode == gotiomode) {
			/* Keep the list in increasing offset order. */
			tflp = LIST_FIRST(flhp);
			prevflp = NULL;
			while (tflp != NULL &&
			    tflp->nfsfl_off < flp->nfsfl_off) {
				prevflp = tflp;
				tflp = LIST_NEXT(tflp, nfsfl_list);
			}
			if (prevflp == NULL)
				LIST_INSERT_HEAD(flhp, flp, nfsfl_list);
			else
				LIST_INSERT_AFTER(prevflp, flp,
				    nfsfl_list);
			NFSCL_DEBUG(4, "flp inserted\n");
		} else {
			printf("nfscl_layoutget(): got wrong iomode\n");
			nfscl_freeflayout(flp);
		}
		flp = NULL;
	}
nfsmout:
	NFSCL_DEBUG(4, "eo nfsrv_parselayoutget=%d\n", error);
	if (error != 0 && flp != NULL)
		nfscl_freeflayout(flp);
	return (error);
}

/*
 * Parse a user/group digit string.
 */
static int
nfsrv_parseug(struct nfsrv_descript *nd, int dogrp, uid_t *uidp, gid_t *gidp,
    NFSPROC_T *p)
{
	uint32_t *tl;
	char *cp, *str, str0[NFSV4_SMALLSTR + 1];
	uint32_t len = 0;
	int error = 0;

	NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
	len = fxdr_unsigned(uint32_t, *tl);
	str = NULL;
	if (len > NFSV4_OPAQUELIMIT) {
		error = NFSERR_BADXDR;
		goto nfsmout;
	}
	NFSCL_DEBUG(4, "nfsrv_parseug: len=%d\n", len);
	if (len == 0) {
		if (dogrp != 0)
			*gidp = GID_NOGROUP;
		else
			*uidp = UID_NOBODY;
		return (0);
	}
	if (len > NFSV4_SMALLSTR)
		str = malloc(len + 1, M_TEMP, M_WAITOK);
	else
		str = str0;
	NFSM_DISSECT(cp, char *, NFSM_RNDUP(len));
	NFSBCOPY(cp, str, len);
	str[len] = '\0';
	NFSCL_DEBUG(4, "nfsrv_parseug: str=%s\n", str);
	if (dogrp != 0)
		error = nfsv4_strtogid(nd, str, len, gidp);
	else
		error = nfsv4_strtouid(nd, str, len, uidp);
nfsmout:
	if (len > NFSV4_SMALLSTR)
		free(str, M_TEMP);
	NFSCL_DEBUG(4, "eo nfsrv_parseug=%d\n", error);
	return (error);
}

/*
 * Similar to nfsrpc_getlayout(), except that it uses nfsrpc_openlayget(),
 * so that it does both an Open and a Layoutget.
 */
static int
nfsrpc_getopenlayout(struct nfsmount *nmp, vnode_t vp, u_int8_t *nfhp,
    int fhlen, uint8_t *newfhp, int newfhlen, uint32_t mode,
    struct nfsclopen *op, uint8_t *name, int namelen, struct nfscldeleg **dpp,
    struct ucred *cred, NFSPROC_T *p)
{
	struct nfscllayout *lyp;
	struct nfsclflayout *flp;
	struct nfsclflayouthead flh;
	int error, islocked, layoutlen, recalled, retonclose, usecurstateid;
	int layouttype, laystat;
	nfsv4stateid_t stateid;
	struct nfsclsession *tsep;

	error = 0;
	if (NFSHASFLEXFILE(nmp))
		layouttype = NFSLAYOUT_FLEXFILE;
	else
		layouttype = NFSLAYOUT_NFSV4_1_FILES;
	/*
	 * If lyp is returned non-NULL, there will be a refcnt (shared lock)
	 * on it, iff flp != NULL or a lock (exclusive lock) on it iff
	 * flp == NULL.
	 */
	lyp = nfscl_getlayout(nmp->nm_clp, newfhp, newfhlen, 0, mode, &flp,
	    &recalled);
	NFSCL_DEBUG(4, "nfsrpc_getopenlayout nfscl_getlayout lyp=%p\n", lyp);
	if (lyp == NULL)
		islocked = 0;
	else if (flp != NULL)
		islocked = 1;
	else
		islocked = 2;
	if ((lyp == NULL || flp == NULL) && recalled == 0) {
		LIST_INIT(&flh);
		tsep = nfsmnt_mdssession(nmp);
		layoutlen = tsep->nfsess_maxcache - (NFSX_STATEID +
		    3 * NFSX_UNSIGNED);
		if (lyp == NULL)
			usecurstateid = 1;
		else {
			usecurstateid = 0;
			stateid.seqid = lyp->nfsly_stateid.seqid;
			stateid.other[0] = lyp->nfsly_stateid.other[0];
			stateid.other[1] = lyp->nfsly_stateid.other[1];
			stateid.other[2] = lyp->nfsly_stateid.other[2];
		}
		error = nfsrpc_openlayoutrpc(nmp, vp, nfhp, fhlen,
		    newfhp, newfhlen, mode, op, name, namelen,
		    dpp, &stateid, usecurstateid, layouttype, layoutlen,
		    &retonclose, &flh, &laystat, cred, p);
		NFSCL_DEBUG(4, "aft nfsrpc_openlayoutrpc laystat=%d err=%d\n",
		    laystat, error);
		laystat = nfsrpc_layoutgetres(nmp, vp, newfhp, newfhlen,
		    &stateid, retonclose, NULL, &lyp, &flh, layouttype, laystat,
		    &islocked, cred, p);
	} else
		error = nfsrpc_openrpc(nmp, vp, nfhp, fhlen, newfhp, newfhlen,
		    mode, op, name, namelen, dpp, 0, 0, cred, p, 0, 0);
	if (islocked == 2)
		nfscl_rellayout(lyp, 1);
	else if (islocked == 1)
		nfscl_rellayout(lyp, 0);
	return (error);
}

/*
 * This function does an Open+LayoutGet for an NFSv4.1 mount with pNFS
 * enabled, only for the CLAIM_NULL case.  All other NFSv4 Opens are
 * handled by nfsrpc_openrpc().
 * For the case where op == NULL, dvp is the directory.  When op != NULL, it
 * can be NULL.
 */
static int
nfsrpc_openlayoutrpc(struct nfsmount *nmp, vnode_t vp, u_int8_t *nfhp,
    int fhlen, uint8_t *newfhp, int newfhlen, uint32_t mode,
    struct nfsclopen *op, uint8_t *name, int namelen, struct nfscldeleg **dpp,
    nfsv4stateid_t *stateidp, int usecurstateid, int layouttype,
    int layoutlen, int *retonclosep, struct nfsclflayouthead *flhp,
    int *laystatp, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfscldeleg *ndp = NULL;
	struct nfsvattr nfsva;
	struct nfsclsession *tsep;
	uint32_t rflags, deleg;
	nfsattrbit_t attrbits;
	int error, ret, acesize, limitby, iomode;

	*dpp = NULL;
	*laystatp = ENXIO;
	nfscl_reqstart(nd, NFSPROC_OPENLAYGET, nmp, nfhp, fhlen, NULL, NULL,
	    0, 0, cred);
	NFSM_BUILD(tl, uint32_t *, 5 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(op->nfso_own->nfsow_seqid);
	*tl++ = txdr_unsigned(mode & (NFSV4OPEN_ACCESSBOTH |
	    NFSV4OPEN_WANTDELEGMASK));
	*tl++ = txdr_unsigned((mode >> NFSLCK_SHIFT) & NFSV4OPEN_DENYBOTH);
	tsep = nfsmnt_mdssession(nmp);
	*tl++ = tsep->nfsess_clientid.lval[0];
	*tl = tsep->nfsess_clientid.lval[1];
	nfsm_strtom(nd, op->nfso_own->nfsow_owner, NFSV4CL_LOCKNAMELEN);
	NFSM_BUILD(tl, uint32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OPEN_NOCREATE);
	if (NFSHASNFSV4N(nmp)) {
		*tl = txdr_unsigned(NFSV4OPEN_CLAIMFH);
	} else {
		*tl = txdr_unsigned(NFSV4OPEN_CLAIMNULL);
		nfsm_strtom(nd, name, namelen);
	}
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSZERO_ATTRBIT(&attrbits);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_CHANGE);
	NFSSETBIT_ATTRBIT(&attrbits, NFSATTRBIT_TIMEMODIFY);
	nfsrv_putattrbit(nd, &attrbits);
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_LAYOUTGET);
	if ((mode & NFSV4OPEN_ACCESSWRITE) != 0)
		iomode = NFSLAYOUTIOMODE_RW;
	else
		iomode = NFSLAYOUTIOMODE_READ;
	nfsrv_setuplayoutget(nd, iomode, 0, UINT64_MAX, 0, stateidp,
	    layouttype, layoutlen, usecurstateid);
	error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, vp, p, cred,
	    NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
	if (error != 0)
		return (error);
	NFSCL_INCRSEQID(op->nfso_own->nfsow_seqid, nd);
	if (nd->nd_repstat != 0)
		*laystatp = nd->nd_repstat;
	if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
		/* ND_NOMOREDATA will be set if the Open operation failed. */
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
		    6 * NFSX_UNSIGNED);
		op->nfso_stateid.seqid = *tl++;
		op->nfso_stateid.other[0] = *tl++;
		op->nfso_stateid.other[1] = *tl++;
		op->nfso_stateid.other[2] = *tl;
		rflags = fxdr_unsigned(u_int32_t, *(tl + 6));
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		if (error != 0)
			goto nfsmout;
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		deleg = fxdr_unsigned(u_int32_t, *tl);
		if (deleg == NFSV4OPEN_DELEGATEREAD ||
		    deleg == NFSV4OPEN_DELEGATEWRITE) {
			if (!(op->nfso_own->nfsow_clp->nfsc_flags &
			      NFSCLFLAGS_FIRSTDELEG))
				op->nfso_own->nfsow_clp->nfsc_flags |=
				  (NFSCLFLAGS_FIRSTDELEG | NFSCLFLAGS_GOTDELEG);
			ndp = malloc(sizeof(struct nfscldeleg) + newfhlen,
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
			} else
				ndp->nfsdl_flags = NFSCLDL_READ;
			if (ret != 0)
				ndp->nfsdl_flags |= NFSCLDL_RECALL;
			error = nfsrv_dissectace(nd, &ndp->nfsdl_ace, false,
			    &ret, &acesize, p);
			if (error != 0)
				goto nfsmout;
		} else if (deleg == NFSV4OPEN_DELEGATENONEEXT &&
		    NFSHASNFSV4N(nmp)) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			deleg = fxdr_unsigned(uint32_t, *tl);
			if (deleg == NFSV4OPEN_CONTENTION ||
			    deleg == NFSV4OPEN_RESOURCE)
				NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		} else if (deleg != NFSV4OPEN_DELEGATENONE) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		if ((rflags & NFSV4OPEN_LOCKTYPEPOSIX) != 0 ||
		    nfscl_assumeposixlocks)
			op->nfso_posixlock = 1;
		else
			op->nfso_posixlock = 0;
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		/* If the 2nd element == NFS_OK, the Getattr succeeded. */
		if (*++tl == 0) {
			error = nfsv4_loadattr(nd, NULL, &nfsva, NULL,
			    NULL, 0, NULL, NULL, NULL, NULL, NULL, 0,
			    NULL, NULL, NULL, p, cred);
			if (error != 0)
				goto nfsmout;
			if (ndp != NULL) {
				ndp->nfsdl_change = nfsva.na_filerev;
				ndp->nfsdl_modtime = nfsva.na_mtime;
				ndp->nfsdl_flags |= NFSCLDL_MODTIMESET;
				*dpp = ndp;
				ndp = NULL;
			}
			/*
			 * At this point, the Open has succeeded, so set
			 * nd_repstat = NFS_OK.  If the Layoutget failed,
			 * this function just won't return a layout.
			 */
			if (nd->nd_repstat == 0) {
				NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
				*laystatp = fxdr_unsigned(int, *++tl);
				if (*laystatp == 0) {
					error = nfsrv_parselayoutget(nmp, nd,
					    stateidp, retonclosep, flhp);
					if (error != 0)
						*laystatp = error;
				}
			} else
				nd->nd_repstat = 0;	/* Return 0 for Open. */
		}
	}
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
nfsmout:
	free(ndp, M_NFSCLDELEG);
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Similar nfsrpc_createv4(), but also does the LayoutGet operation.
 * Used only for mounts with pNFS enabled.
 */
static int
nfsrpc_createlayout(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    nfsquad_t cverf, int fmode, struct nfsclowner *owp, struct nfscldeleg **dpp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp, int *unlockedp, nfsv4stateid_t *stateidp,
    int usecurstateid, int layouttype, int layoutlen, int *retonclosep,
    struct nfsclflayouthead *flhp, int *laystatp)
{
	uint32_t *tl;
	int error = 0, deleg, newone, ret, acesize, limitby;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct nfsclopen *op;
	struct nfscldeleg *dp = NULL;
	struct nfsnode *np;
	struct nfsfh *nfhp;
	struct nfsclsession *tsep;
	nfsattrbit_t attrbits;
	nfsv4stateid_t stateid;
	struct nfsmount *nmp;

	nmp = VFSTONFS(dvp->v_mount);
	np = VTONFS(dvp);
	*laystatp = ENXIO;
	*unlockedp = 0;
	*nfhpp = NULL;
	*dpp = NULL;
	*attrflagp = 0;
	*dattrflagp = 0;
	if (namelen > NFS_MAXNAMLEN)
		return (ENAMETOOLONG);
	NFSCL_REQSTART(nd, NFSPROC_CREATELAYGET, dvp, cred);
	/*
	 * For V4, this is actually an Open op.
	 */
	NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(owp->nfsow_seqid);
	if (NFSHASNFSV4N(nmp)) {
		if (!NFSHASPNFS(nmp) && nfscl_enablecallb != 0 &&
		    nfs_numnfscbd > 0)
			*tl++ = txdr_unsigned(NFSV4OPEN_ACCESSWRITE |
			    NFSV4OPEN_ACCESSREAD | NFSV4OPEN_WANTWRITEDELEG);
		else
			*tl++ = txdr_unsigned(NFSV4OPEN_ACCESSWRITE |
			    NFSV4OPEN_ACCESSREAD | NFSV4OPEN_WANTNODELEG);
	} else
		*tl++ = txdr_unsigned(NFSV4OPEN_ACCESSWRITE |
		    NFSV4OPEN_ACCESSREAD);
	*tl++ = txdr_unsigned(NFSV4OPEN_DENYNONE);
	tsep = nfsmnt_mdssession(nmp);
	*tl++ = tsep->nfsess_clientid.lval[0];
	*tl = tsep->nfsess_clientid.lval[1];
	nfsm_strtom(nd, owp->nfsow_owner, NFSV4CL_LOCKNAMELEN);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OPEN_CREATE);
	if ((fmode & O_EXCL) != 0) {
		if (NFSHASSESSPERSIST(nmp)) {
			/* Use GUARDED for persistent sessions. */
			*tl = txdr_unsigned(NFSCREATE_GUARDED);
			nfscl_fillsattr(nd, vap, dvp, NFSSATTR_NEWFILE, 0);
		} else {
			/* Otherwise, use EXCLUSIVE4_1. */
			*tl = txdr_unsigned(NFSCREATE_EXCLUSIVE41);
			NFSM_BUILD(tl, u_int32_t *, NFSX_VERF);
			*tl++ = cverf.lval[0];
			*tl = cverf.lval[1];
			nfscl_fillsattr(nd, vap, dvp, NFSSATTR_NEWFILE, 0);
		}
	} else {
		*tl = txdr_unsigned(NFSCREATE_UNCHECKED);
		nfscl_fillsattr(nd, vap, dvp, NFSSATTR_NEWFILE, 0);
	}
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OPEN_CLAIMNULL);
	nfsm_strtom(nd, name, namelen);
	/* Get the new file's handle and attributes, plus save the FH. */
	NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OP_SAVEFH);
	*tl++ = txdr_unsigned(NFSV4OP_GETFH);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);
	/* Get the directory's post-op attributes. */
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_PUTFH);
	(void)nfsm_fhtom(nmp, nd, np->n_fhp->nfh_fh, np->n_fhp->nfh_len, 0);
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	nfsrv_putattrbit(nd, &attrbits);
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(NFSV4OP_RESTOREFH);
	*tl = txdr_unsigned(NFSV4OP_LAYOUTGET);
	nfsrv_setuplayoutget(nd, NFSLAYOUTIOMODE_RW, 0, UINT64_MAX, 0, stateidp,
	    layouttype, layoutlen, usecurstateid);
	error = nfscl_request(nd, dvp, p, cred);
	if (error != 0)
		return (error);
	NFSCL_DEBUG(4, "nfsrpc_createlayout stat=%d err=%d\n", nd->nd_repstat,
	    error);
	if (nd->nd_repstat != 0)
		*laystatp = nd->nd_repstat;
	NFSCL_INCRSEQID(owp->nfsow_seqid, nd);
	if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
		NFSCL_DEBUG(4, "nfsrpc_createlayout open succeeded\n");
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
		    6 * NFSX_UNSIGNED);
		stateid.seqid = *tl++;
		stateid.other[0] = *tl++;
		stateid.other[1] = *tl++;
		stateid.other[2] = *tl;
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		if (error != 0)
			goto nfsmout;
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		deleg = fxdr_unsigned(int, *tl);
		if (deleg == NFSV4OPEN_DELEGATEREAD ||
		    deleg == NFSV4OPEN_DELEGATEWRITE) {
			if (!(owp->nfsow_clp->nfsc_flags &
			      NFSCLFLAGS_FIRSTDELEG))
				owp->nfsow_clp->nfsc_flags |=
				  (NFSCLFLAGS_FIRSTDELEG | NFSCLFLAGS_GOTDELEG);
			dp = malloc(sizeof(struct nfscldeleg) + NFSX_V4FHMAX,
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
			if (ret != 0)
				dp->nfsdl_flags |= NFSCLDL_RECALL;
			error = nfsrv_dissectace(nd, &dp->nfsdl_ace, false,
			    &ret, &acesize, p);
			if (error != 0)
				goto nfsmout;
		} else if (deleg != NFSV4OPEN_DELEGATENONE) {
			error = NFSERR_BADXDR;
			goto nfsmout;
		}

		/* Now, we should have the status for the SaveFH. */
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		if (*++tl == 0) {
			NFSCL_DEBUG(4, "nfsrpc_createlayout SaveFH ok\n");
			/*
			 * Now, process the GetFH and Getattr for the newly
			 * created file. nfscl_mtofh() will set
			 * ND_NOMOREDATA if these weren't successful.
			 */
			error = nfscl_mtofh(nd, nfhpp, nnap, attrflagp);
			NFSCL_DEBUG(4, "aft nfscl_mtofh err=%d\n", error);
			if (error != 0)
				goto nfsmout;
		} else
			nd->nd_flag |= ND_NOMOREDATA;
		/* Now we have the PutFH and Getattr for the directory. */
		if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
			NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			if (*++tl != 0)
				nd->nd_flag |= ND_NOMOREDATA;
			else {
				NFSM_DISSECT(tl, uint32_t *, 2 *
				    NFSX_UNSIGNED);
				if (*++tl != 0)
					nd->nd_flag |= ND_NOMOREDATA;
			}
		}
		if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
			/* Load the directory attributes. */
			error = nfsm_loadattr(nd, dnap);
			NFSCL_DEBUG(4, "aft nfsm_loadattr err=%d\n", error);
			if (error != 0)
				goto nfsmout;
			*dattrflagp = 1;
			if (dp != NULL && *attrflagp != 0) {
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
				NFSBCOPY(nfhp->nfh_fh, dp->nfsdl_fh,
				    nfhp->nfh_len);
			}
			/*
			 * Get an Open structure that will be
			 * attached to the OpenOwner, acquired already.
			 */
			error = nfscl_open(dvp, nfhp->nfh_fh, nfhp->nfh_len, 
			    (NFSV4OPEN_ACCESSWRITE | NFSV4OPEN_ACCESSREAD), 0,
			    cred, p, NULL, &op, &newone, NULL, 0, false);
			if (error != 0)
				goto nfsmout;
			op->nfso_stateid = stateid;
			newnfs_copyincred(cred, &op->nfso_cred);

			nfscl_openrelease(nmp, op, error, newone);
			*unlockedp = 1;

			/* Now, handle the RestoreFH and LayoutGet. */
			if (nd->nd_repstat == 0) {
				NFSM_DISSECT(tl, uint32_t *, 4 * NFSX_UNSIGNED);
				*laystatp = fxdr_unsigned(int, *(tl + 3));
				if (*laystatp == 0) {
					error = nfsrv_parselayoutget(nmp, nd,
					    stateidp, retonclosep, flhp);
					if (error != 0)
						*laystatp = error;
				}
				NFSCL_DEBUG(4, "aft nfsrv_parselayout err=%d\n",
				    error);
			} else
				nd->nd_repstat = 0;
		}
	}
	if (nd->nd_repstat != 0 && error == 0)
		error = nd->nd_repstat;
	if (error == NFSERR_STALECLIENTID)
		nfscl_initiate_recovery(owp->nfsow_clp);
nfsmout:
	NFSCL_DEBUG(4, "eo nfsrpc_createlayout err=%d\n", error);
	if (error == 0)
		*dpp = dp;
	else
		free(dp, M_NFSCLDELEG);
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Similar to nfsrpc_getopenlayout(), except that it used for the Create case.
 */
static int
nfsrpc_getcreatelayout(vnode_t dvp, char *name, int namelen, struct vattr *vap,
    nfsquad_t cverf, int fmode, struct nfsclowner *owp, struct nfscldeleg **dpp,
    struct ucred *cred, NFSPROC_T *p, struct nfsvattr *dnap,
    struct nfsvattr *nnap, struct nfsfh **nfhpp, int *attrflagp,
    int *dattrflagp, int *unlockedp)
{
	struct nfscllayout *lyp;
	struct nfsclflayouthead flh;
	struct nfsfh *nfhp;
	struct nfsclsession *tsep;
	struct nfsmount *nmp;
	nfsv4stateid_t stateid;
	int error, layoutlen, layouttype, retonclose, laystat;

	error = 0;
	nmp = VFSTONFS(dvp->v_mount);
	if (NFSHASFLEXFILE(nmp))
		layouttype = NFSLAYOUT_FLEXFILE;
	else
		layouttype = NFSLAYOUT_NFSV4_1_FILES;
	LIST_INIT(&flh);
	tsep = nfsmnt_mdssession(nmp);
	layoutlen = tsep->nfsess_maxcache - (NFSX_STATEID + 3 * NFSX_UNSIGNED);
	error = nfsrpc_createlayout(dvp, name, namelen, vap, cverf, fmode,
	    owp, dpp, cred, p, dnap, nnap, nfhpp, attrflagp, dattrflagp,
	    unlockedp, &stateid, 1, layouttype, layoutlen, &retonclose,
	    &flh, &laystat);
	NFSCL_DEBUG(4, "aft nfsrpc_createlayoutrpc laystat=%d err=%d\n",
	    laystat, error);
	lyp = NULL;
	if (laystat == 0) {
		nfhp = *nfhpp;
		laystat = nfsrpc_layoutgetres(nmp, dvp, nfhp->nfh_fh,
		    nfhp->nfh_len, &stateid, retonclose, NULL, &lyp, &flh,
		    layouttype, laystat, NULL, cred, p);
	} else
		laystat = nfsrpc_layoutgetres(nmp, dvp, NULL, 0, &stateid,
		    retonclose, NULL, &lyp, &flh, layouttype, laystat, NULL,
		    cred, p);
	if (laystat == 0)
		nfscl_rellayout(lyp, 0);
	return (error);
}

/*
 * Process the results of a layoutget() operation.
 */
static int
nfsrpc_layoutgetres(struct nfsmount *nmp, vnode_t vp, uint8_t *newfhp,
    int newfhlen, nfsv4stateid_t *stateidp, int retonclose, uint32_t *notifybit,
    struct nfscllayout **lypp, struct nfsclflayouthead *flhp, int layouttype,
    int laystat, int *islockedp, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsclflayout *tflp;
	struct nfscldevinfo *dip;
	uint8_t *dev;
	int i, mirrorcnt;

	if (laystat == NFSERR_UNKNLAYOUTTYPE) {
		NFSLOCKMNT(nmp);
		if (!NFSHASFLEXFILE(nmp)) {
			/* Switch to using Flex File Layout. */
			nmp->nm_state |= NFSSTA_FLEXFILE;
		} else if (layouttype == NFSLAYOUT_FLEXFILE) {
			/* Disable pNFS. */
			NFSCL_DEBUG(1, "disable PNFS\n");
			nmp->nm_state &= ~(NFSSTA_PNFS | NFSSTA_FLEXFILE);
		}
		NFSUNLOCKMNT(nmp);
	}
	if (laystat == 0) {
		NFSCL_DEBUG(4, "nfsrpc_layoutgetres at FOREACH\n");
		LIST_FOREACH(tflp, flhp, nfsfl_list) {
			if (layouttype == NFSLAYOUT_FLEXFILE)
				mirrorcnt = tflp->nfsfl_mirrorcnt;
			else
				mirrorcnt = 1;
			for (i = 0; i < mirrorcnt; i++) {
				laystat = nfscl_adddevinfo(nmp, NULL, i, tflp);
				NFSCL_DEBUG(4, "aft adddev=%d\n", laystat);
				if (laystat != 0) {
					if (layouttype == NFSLAYOUT_FLEXFILE)
						dev = tflp->nfsfl_ffm[i].dev;
					else
						dev = tflp->nfsfl_dev;
					laystat = nfsrpc_getdeviceinfo(nmp, dev,
					    layouttype, notifybit, &dip, cred,
					    p);
					NFSCL_DEBUG(4, "aft nfsrpc_gdi=%d\n",
					    laystat);
					if (laystat != 0)
						goto out;
					laystat = nfscl_adddevinfo(nmp, dip, i,
					    tflp);
					if (laystat != 0)
						printf("nfsrpc_layoutgetresout"
						    ": cannot add\n");
				}
			}
		}
	}
out:
	if (laystat == 0) {
		/*
		 * nfscl_layout() always returns with the nfsly_lock
		 * set to a refcnt (shared lock).
		 * Passing in dvp is sufficient, since it is only used to
		 * get the fsid for the file system.
		 */
		laystat = nfscl_layout(nmp, vp, newfhp, newfhlen, stateidp,
		    layouttype, retonclose, flhp, lypp, cred, p);
		NFSCL_DEBUG(4, "nfsrpc_layoutgetres: aft nfscl_layout=%d\n",
		    laystat);
		if (laystat == 0 && islockedp != NULL)
			*islockedp = 1;
	}
	return (laystat);
}

/*
 * nfs copy_file_range operation.
 */
int
nfsrpc_copy_file_range(vnode_t invp, off_t *inoffp, vnode_t outvp,
    off_t *outoffp, size_t *lenp, unsigned int flags, int *inattrflagp,
    struct nfsvattr *innap, int *outattrflagp, struct nfsvattr *outnap,
    struct ucred *cred, bool consecutive, bool *must_commitp)
{
	int commit, error, expireret = 0, retrycnt;
	u_int32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(invp->v_mount);
	struct nfsfh *innfhp = NULL, *outnfhp = NULL;
	nfsv4stateid_t instateid, outstateid;
	void *inlckp, *outlckp;

	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	innfhp = VTONFS(invp)->n_fhp;
	outnfhp = VTONFS(outvp)->n_fhp;
	retrycnt = 0;
	do {
		/* Get both stateids. */
		inlckp = NULL;
		nfscl_getstateid(invp, innfhp->nfh_fh, innfhp->nfh_len,
		    NFSV4OPEN_ACCESSREAD, 0, NULL, curthread, &instateid,
		    &inlckp);
		outlckp = NULL;
		nfscl_getstateid(outvp, outnfhp->nfh_fh, outnfhp->nfh_len,
		    NFSV4OPEN_ACCESSWRITE, 0, NULL, curthread, &outstateid,
		    &outlckp);

		error = nfsrpc_copyrpc(invp, *inoffp, outvp, *outoffp, lenp,
		    &instateid, &outstateid, innap, inattrflagp, outnap,
		    outattrflagp, consecutive, &commit, cred, curthread);
		if (error == 0) {
			if (commit != NFSWRITE_FILESYNC)
				*must_commitp = true;
			*inoffp += *lenp;
			*outoffp += *lenp;
		} else if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (inlckp != NULL)
			nfscl_lockderef(inlckp);
		if (outlckp != NULL)
			nfscl_lockderef(outlckp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID || error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_cfr");
		} else if ((error == NFSERR_EXPIRED || (!NFSHASINT(nmp) &&
		    error == NFSERR_BADSTATEID)) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev,
			    curthread);
		} else if (error == NFSERR_BADSTATEID && NFSHASINT(nmp)) {
			error = EIO;
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_DELAY ||
	    error == NFSERR_STALESTATEID || error == NFSERR_BADSESSION ||
	      error == NFSERR_STALEDONTRECOVER ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error != 0 && (retrycnt >= 4 ||
	    error == NFSERR_STALESTATEID || error == NFSERR_BADSESSION ||
	      error == NFSERR_STALEDONTRECOVER))
		error = EIO;
	return (error);
}

/*
 * The copy RPC.
 */
static int
nfsrpc_copyrpc(vnode_t invp, off_t inoff, vnode_t outvp, off_t outoff,
    size_t *lenp, nfsv4stateid_t *instateidp, nfsv4stateid_t *outstateidp,
    struct nfsvattr *innap, int *inattrflagp, struct nfsvattr *outnap,
    int *outattrflagp, bool consecutive, int *commitp, struct ucred *cred,
    NFSPROC_T *p)
{
	uint32_t *tl, *opcntp;
	int error;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfsmount *nmp;
	nfsattrbit_t attrbits;
	struct vattr va;
	uint64_t len;

	nmp = VFSTONFS(invp->v_mount);
	*inattrflagp = *outattrflagp = 0;
	*commitp = NFSWRITE_UNSTABLE;
	len = *lenp;
	*lenp = 0;
	if (len > nfs_maxcopyrange)
		len = nfs_maxcopyrange;
	nfscl_reqstart(nd, NFSPROC_COPY, nmp, VTONFS(invp)->n_fhp->nfh_fh,
	    VTONFS(invp)->n_fhp->nfh_len, &opcntp, NULL, 0, 0, cred);
	/*
	 * First do a Setattr of atime to the server's clock
	 * time.  The FreeBSD "collective" was of the opinion
	 * that setting atime was necessary for this syscall.
	 * Do the Setattr before the Copy, so that it can be
	 * handled well if the server replies NFSERR_DELAY to
	 * the Setattr operation.
	 */
	if ((nmp->nm_mountp->mnt_flag & MNT_NOATIME) == 0) {
		NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_SETATTR);
		nfsm_stateidtom(nd, instateidp, NFSSTATEID_PUTSTATEID);
		VATTR_NULL(&va);
		va.va_atime.tv_sec = va.va_atime.tv_nsec = 0;
		va.va_vaflags = VA_UTIMES_NULL;
		nfscl_fillsattr(nd, &va, invp, 0, 0);
		/* Bump opcnt from 7 to 8. */
		*opcntp = txdr_unsigned(8);
	}

	/* Now Getattr the invp attributes. */
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);

	/* Set outvp. */
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_PUTFH);
	(void)nfsm_fhtom(nmp, nd, VTONFS(outvp)->n_fhp->nfh_fh,
	    VTONFS(outvp)->n_fhp->nfh_len, 0);

	/* Do the Copy. */
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_COPY);
	nfsm_stateidtom(nd, instateidp, NFSSTATEID_PUTSTATEID);
	nfsm_stateidtom(nd, outstateidp, NFSSTATEID_PUTSTATEID);
	NFSM_BUILD(tl, uint32_t *, 3 * NFSX_HYPER + 4 * NFSX_UNSIGNED);
	txdr_hyper(inoff, tl); tl += 2;
	txdr_hyper(outoff, tl); tl += 2;
	txdr_hyper(len, tl); tl += 2;
	if (consecutive)
		*tl++ = newnfs_true;
	else
		*tl++ = newnfs_false;
	*tl++ = newnfs_true;
	*tl++ = 0;

	/* Get the outvp attributes. */
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSWRITEGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);

	error = nfscl_request(nd, invp, p, cred);
	if (error != 0)
		return (error);
	/* Skip over the Setattr reply. */
	if ((nd->nd_flag & ND_NOMOREDATA) == 0 &&
	    (nmp->nm_mountp->mnt_flag & MNT_NOATIME) == 0) {
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		if (*(tl + 1) == 0) {
			error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
			if (error != 0)
				goto nfsmout;
		} else
			nd->nd_flag |= ND_NOMOREDATA;
	}
	if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
		/* Get the input file's attributes. */
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		if (*(tl + 1) == 0) {
			error = nfsm_loadattr(nd, innap);
			if (error != 0)
				goto nfsmout;
			*inattrflagp = 1;
		} else
			nd->nd_flag |= ND_NOMOREDATA;
	}
	/* Skip over return stat for PutFH. */
	if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		if (*++tl != 0)
			nd->nd_flag |= ND_NOMOREDATA;
	}
	/* Skip over return stat for Copy. */
	if ((nd->nd_flag & ND_NOMOREDATA) == 0)
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		if (*tl != 0) {
			/* There should be no callback ids. */
			error = NFSERR_BADXDR;
			goto nfsmout;
		}
		NFSM_DISSECT(tl, uint32_t *, NFSX_HYPER + 3 * NFSX_UNSIGNED +
		    NFSX_VERF);
		len = fxdr_hyper(tl); tl += 2;
		*commitp = fxdr_unsigned(int, *tl++);
		NFSLOCKMNT(nmp);
		if (!NFSHASWRITEVERF(nmp)) {
			NFSBCOPY(tl, nmp->nm_verf, NFSX_VERF);
			NFSSETWRITEVERF(nmp);
	    	} else if (NFSBCMP(tl, nmp->nm_verf, NFSX_VERF)) {
			NFSBCOPY(tl, nmp->nm_verf, NFSX_VERF);
			nd->nd_repstat = NFSERR_STALEWRITEVERF;
		}
		NFSUNLOCKMNT(nmp);
		tl += (NFSX_VERF / NFSX_UNSIGNED);
		if (nd->nd_repstat == 0 && *++tl != newnfs_true)
			/* Must be a synchronous copy. */
			nd->nd_repstat = NFSERR_NOTSUPP;
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		error = nfsm_loadattr(nd, outnap);
		if (error == 0)
			*outattrflagp = NFS_LATTR_NOSHRINK;
		if (nd->nd_repstat == 0)
			*lenp = len;
	} else if (nd->nd_repstat == NFSERR_OFFLOADNOREQS) {
		/*
		 * For the case where consecutive is not supported, but
		 * synchronous is supported, we can try consecutive == false
		 * by returning this error.  Otherwise, return NFSERR_NOTSUPP,
		 * since Copy cannot be done.
		 */
		if ((nd->nd_flag & ND_NOMOREDATA) == 0) {
			NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
			if (!consecutive || *++tl == newnfs_false)
				nd->nd_repstat = NFSERR_NOTSUPP;
		} else
			nd->nd_repstat = NFSERR_BADXDR;
	}
	if (error == 0)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Seek operation.
 */
int
nfsrpc_seek(vnode_t vp, off_t *offp, bool *eofp, int content,
    struct ucred *cred, struct nfsvattr *nap, int *attrflagp)
{
	int error, expireret = 0, retrycnt;
	u_int32_t clidrev = 0;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *np = VTONFS(vp);
	struct nfsfh *nfhp = NULL;
	nfsv4stateid_t stateid;
	void *lckp;

	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	nfhp = np->n_fhp;
	retrycnt = 0;
	do {
		lckp = NULL;
		nfscl_getstateid(vp, nfhp->nfh_fh, nfhp->nfh_len,
		    NFSV4OPEN_ACCESSREAD, 0, cred, curthread, &stateid, &lckp);
		error = nfsrpc_seekrpc(vp, offp, &stateid, eofp, content,
		    nap, attrflagp, cred);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(nmp->nm_clp);
		if (lckp != NULL)
			nfscl_lockderef(lckp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID || error == NFSERR_BADSESSION) {
			(void) nfs_catnap(PZERO, error, "nfs_seek");
		} else if ((error == NFSERR_EXPIRED || (!NFSHASINT(nmp) &&
		    error == NFSERR_BADSTATEID)) && clidrev != 0) {
			expireret = nfscl_hasexpired(nmp->nm_clp, clidrev,
			    curthread);
		} else if (error == NFSERR_BADSTATEID && NFSHASINT(nmp)) {
			error = EIO;
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    error == NFSERR_BADSESSION ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4) ||
	    (error == NFSERR_OPENMODE && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * The seek RPC.
 */
static int
nfsrpc_seekrpc(vnode_t vp, off_t *offp, nfsv4stateid_t *stateidp, bool *eofp,
    int content, struct nfsvattr *nap, int *attrflagp, struct ucred *cred)
{
	uint32_t *tl;
	int error;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_SEEK, vp, cred);
	nfsm_stateidtom(nd, stateidp, NFSSTATEID_PUTSTATEID);
	NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + 2 * NFSX_UNSIGNED);
	txdr_hyper(*offp, tl); tl += 2;
	*tl++ = txdr_unsigned(content);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, curthread, cred);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, 3 * NFSX_UNSIGNED + NFSX_HYPER);
		if (*tl++ == newnfs_true)
			*eofp = true;
		else
			*eofp = false;
		*offp = fxdr_hyper(tl);
		/* Just skip over Getattr op status. */
		error = nfsm_loadattr(nd, nap);
		if (error == 0)
			*attrflagp = 1;
	}
	error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * The getextattr RPC.
 */
int
nfsrpc_getextattr(vnode_t vp, const char *name, struct uio *uiop, ssize_t *lenp,
    struct nfsvattr *nap, int *attrflagp, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	int error;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;
	uint32_t len, len2;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_GETEXTATTR, vp, cred);
	nfsm_strtom(nd, name, strlen(name));
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
		len = fxdr_unsigned(uint32_t, *tl);
		/* Sanity check lengths. */
		if (uiop != NULL && len > 0 && len <= IOSIZE_MAX &&
		    uiop->uio_resid <= UINT32_MAX) {
			len2 = uiop->uio_resid;
			if (len2 >= len)
				error = nfsm_mbufuio(nd, uiop, len);
			else {
				error = nfsm_mbufuio(nd, uiop, len2);
				if (error == 0) {
					/*
					 * nfsm_mbufuio() advances to a multiple
					 * of 4, so round up len2 as well.  Then
					 * we need to advance over the rest of
					 * the data, rounding up the remaining
					 * length.
					 */
					len2 = NFSM_RNDUP(len2);
					len2 = NFSM_RNDUP(len - len2);
					if (len2 > 0)
						error = nfsm_advance(nd, len2,
						    -1);
				}
			}
		} else if (uiop == NULL && len > 0) {
			/* Just wants the length and not the data. */
			error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
		} else if (len > 0)
			error = ENOATTR;
		if (error != 0)
			goto nfsmout;
		*lenp = len;
		/* Just skip over Getattr op status. */
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		error = nfsm_loadattr(nd, nap);
		if (error == 0)
			*attrflagp = 1;
	}
	if (error == 0)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * The setextattr RPC.
 */
int
nfsrpc_setextattr(vnode_t vp, const char *name, struct uio *uiop,
    struct nfsvattr *nap, int *attrflagp, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	int error;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_SETEXTATTR, vp, cred);
	if (uiop->uio_resid > nd->nd_maxreq) {
		/* nd_maxreq is set by NFSCL_REQSTART(). */
		m_freem(nd->nd_mreq);
		return (EINVAL);
	}
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4SXATTR_EITHER);
	nfsm_strtom(nd, name, strlen(name));
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(uiop->uio_resid);
	error = nfsm_uiombuf(nd, uiop, uiop->uio_resid);
	if (error != 0) {
		m_freem(nd->nd_mreq);
		return (error);
	}
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		/* Just skip over the reply and Getattr op status. */
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_HYPER + 3 *
		    NFSX_UNSIGNED);
		error = nfsm_loadattr(nd, nap);
		if (error == 0)
			*attrflagp = 1;
	}
	if (error == 0)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * The removeextattr RPC.
 */
int
nfsrpc_rmextattr(vnode_t vp, const char *name, struct nfsvattr *nap,
    int *attrflagp, struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	int error;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_RMEXTATTR, vp, cred);
	nfsm_strtom(nd, name, strlen(name));
	NFSM_BUILD(tl, uint32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred);
	if (error != 0)
		return (error);
	if (nd->nd_repstat == 0) {
		/* Just skip over the reply and Getattr op status. */
		NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_HYPER + 3 *
		    NFSX_UNSIGNED);
		error = nfsm_loadattr(nd, nap);
		if (error == 0)
			*attrflagp = 1;
	}
	if (error == 0)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * The listextattr RPC.
 */
int
nfsrpc_listextattr(vnode_t vp, uint64_t *cookiep, struct uio *uiop,
    size_t *lenp, bool *eofp, struct nfsvattr *nap, int *attrflagp,
    struct ucred *cred, NFSPROC_T *p)
{
	uint32_t *tl;
	int cnt, error, i, len;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	nfsattrbit_t attrbits;
	u_char c;

	*attrflagp = 0;
	NFSCL_REQSTART(nd, NFSPROC_LISTEXTATTR, vp, cred);
	NFSM_BUILD(tl, uint32_t *, NFSX_HYPER + 2 * NFSX_UNSIGNED);
	txdr_hyper(*cookiep, tl); tl += 2;
	*tl++ = txdr_unsigned(*lenp);
	*tl = txdr_unsigned(NFSV4OP_GETATTR);
	NFSGETATTR_ATTRBIT(&attrbits);
	nfsrv_putattrbit(nd, &attrbits);
	error = nfscl_request(nd, vp, p, cred);
	if (error != 0)
		return (error);
	*eofp = true;
	*lenp = 0;
	if (nd->nd_repstat == 0) {
		NFSM_DISSECT(tl, uint32_t *, NFSX_HYPER + NFSX_UNSIGNED);
		*cookiep = fxdr_hyper(tl); tl += 2;
		cnt = fxdr_unsigned(int, *tl);
		if (cnt < 0) {
			error = EBADRPC;
			goto nfsmout;
		}
		for (i = 0; i < cnt; i++) {
			NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
			len = fxdr_unsigned(int, *tl);
			if (len <= 0 || len > EXTATTR_MAXNAMELEN) {
				error = EBADRPC;
				goto nfsmout;
			}
			if (uiop == NULL)
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
			else if (uiop->uio_resid >= len + 1) {
				c = len;
				error = uiomove(&c, sizeof(c), uiop);
				if (error == 0)
					error = nfsm_mbufuio(nd, uiop, len);
			} else {
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
				*eofp = false;
			}
			if (error != 0)
				goto nfsmout;
			*lenp += (len + 1);
		}
		/* Get the eof and skip over the Getattr op status. */
		NFSM_DISSECT(tl, uint32_t *, 3 * NFSX_UNSIGNED);
		/*
		 * *eofp is set false above, because it wasn't able to copy
		 * all of the reply.
		 */
		if (*eofp && *tl == 0)
			*eofp = false;
		error = nfsm_loadattr(nd, nap);
		if (error == 0)
			*attrflagp = 1;
	}
	if (error == 0)
		error = nd->nd_repstat;
nfsmout:
	m_freem(nd->nd_mrep);
	return (error);
}

/*
 * Split an mbuf list.  For non-M_EXTPG mbufs, just use m_split().
 */
static struct mbuf *
nfsm_split(struct mbuf *mp, uint64_t xfer)
{
	struct mbuf *m, *m2;
	vm_page_t pg;
	int i, j, left, pgno, plen, trim;
	char *cp, *cp2;

	if ((mp->m_flags & M_EXTPG) == 0) {
		m = m_split(mp, xfer, M_WAITOK);
		return (m);
	}

	/* Find the correct mbuf to split at. */
	for (m = mp; m != NULL && xfer > m->m_len; m = m->m_next)
		xfer -= m->m_len;
	if (m == NULL)
		return (NULL);

	/* If xfer == m->m_len, we can just split the mbuf list. */
	if (xfer == m->m_len) {
		m2 = m->m_next;
		m->m_next = NULL;
		return (m2);
	}

	/* Find the page to split at. */
	pgno = 0;
	left = xfer;
	do {
		if (pgno == 0)
			plen = m_epg_pagelen(m, 0, m->m_epg_1st_off);
		else
			plen = m_epg_pagelen(m, pgno, 0);
		if (left <= plen)
			break;
		left -= plen;
		pgno++;
	} while (pgno < m->m_epg_npgs);
	if (pgno == m->m_epg_npgs)
		panic("nfsm_split: eroneous ext_pgs mbuf");

	m2 = mb_alloc_ext_pgs(M_WAITOK, mb_free_mext_pgs, 0);
	m2->m_epg_flags |= EPG_FLAG_ANON;

	/*
	 * If left < plen, allocate a new page for the new mbuf
	 * and copy the data after left in the page to this new
	 * page.
	 */
	if (left < plen) {
		pg = vm_page_alloc_noobj(VM_ALLOC_WAITOK | VM_ALLOC_NODUMP |
		    VM_ALLOC_WIRED);
		m2->m_epg_pa[0] = VM_PAGE_TO_PHYS(pg);
		m2->m_epg_npgs = 1;

		/* Copy the data after left to the new page. */
		trim = plen - left;
		cp = (char *)(void *)PHYS_TO_DMAP(m->m_epg_pa[pgno]);
		if (pgno == 0)
			cp += m->m_epg_1st_off;
		cp += left;
		cp2 = (char *)(void *)PHYS_TO_DMAP(m2->m_epg_pa[0]);
		if (pgno == m->m_epg_npgs - 1)
			m2->m_epg_last_len = trim;
		else {
			cp2 += PAGE_SIZE - trim;
			m2->m_epg_1st_off = PAGE_SIZE - trim;
			m2->m_epg_last_len = m->m_epg_last_len;
		}
		memcpy(cp2, cp, trim);
		m2->m_len = trim;
	} else {
		m2->m_len = 0;
		m2->m_epg_last_len = m->m_epg_last_len;
	}

	/* Move the pages beyond pgno to the new mbuf. */
	for (i = pgno + 1, j = m2->m_epg_npgs; i < m->m_epg_npgs; i++, j++) {
		m2->m_epg_pa[j] = m->m_epg_pa[i];
		/* Never moves page 0. */
		m2->m_len += m_epg_pagelen(m, i, 0);
	}
	m2->m_epg_npgs = j;
	m->m_epg_npgs = pgno + 1;
	m->m_epg_last_len = left;
	m->m_len = xfer;

	m2->m_next = m->m_next;
	m->m_next = NULL;
	return (m2);
}

/*
 * Do the NFSv4.1 Bind Connection to Session.
 * Called from the reconnect layer of the krpc (sys/rpc/clnt_rc.c).
 */
void
nfsrpc_bindconnsess(CLIENT *cl, void *arg, struct ucred *cr)
{
	struct nfscl_reconarg *rcp = (struct nfscl_reconarg *)arg;
	uint32_t res, *tl;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct rpc_callextra ext;
	struct timeval utimeout;
	enum clnt_stat stat;
	int error;

	nfscl_reqstart(nd, NFSPROC_BINDCONNTOSESS, NULL, NULL, 0, NULL, NULL,
	    NFS_VER4, rcp->minorvers, NULL);
	NFSM_BUILD(tl, uint32_t *, NFSX_V4SESSIONID + 2 * NFSX_UNSIGNED);
	memcpy(tl, rcp->sessionid, NFSX_V4SESSIONID);
	tl += NFSX_V4SESSIONID / NFSX_UNSIGNED;
	*tl++ = txdr_unsigned(NFSCDFC4_FORE_OR_BOTH);
	*tl = newnfs_false;

	memset(&ext, 0, sizeof(ext));
	utimeout.tv_sec = 30;
	utimeout.tv_usec = 0;
	ext.rc_auth = authunix_create(cr);
	nd->nd_mrep = NULL;
	stat = CLNT_CALL_MBUF(cl, &ext, NFSV4PROC_COMPOUND, nd->nd_mreq,
	    &nd->nd_mrep, utimeout);
	AUTH_DESTROY(ext.rc_auth);
	if (stat != RPC_SUCCESS) {
		printf("nfsrpc_bindconnsess: call failed stat=%d\n", stat);
		return;
	}
	if (nd->nd_mrep == NULL) {
		printf("nfsrpc_bindconnsess: no reply args\n");
		return;
	}
	error = 0;
	newnfs_realign(&nd->nd_mrep, M_WAITOK);
	nd->nd_md = nd->nd_mrep;
	nd->nd_dpos = mtod(nd->nd_md, char *);
	NFSM_DISSECT(tl, uint32_t *, 2 * NFSX_UNSIGNED);
	nd->nd_repstat = fxdr_unsigned(uint32_t, *tl++);
	if (nd->nd_repstat == NFSERR_OK) {
		res = fxdr_unsigned(uint32_t, *tl);
		if (res > 0 && (error = nfsm_advance(nd, NFSM_RNDUP(res),
		    -1)) != 0)
			goto nfsmout;
		NFSM_DISSECT(tl, uint32_t *, NFSX_V4SESSIONID +
		    4 * NFSX_UNSIGNED);
		tl += 3;
		if (!NFSBCMP(tl, rcp->sessionid, NFSX_V4SESSIONID)) {
			tl += NFSX_V4SESSIONID / NFSX_UNSIGNED;
			res = fxdr_unsigned(uint32_t, *tl);
			if (res != NFSCDFS4_BOTH)
				printf("nfsrpc_bindconnsess: did not "
				    "return FS4_BOTH\n");
		} else
			printf("nfsrpc_bindconnsess: not same "
			    "sessionid\n");
	} else if (nd->nd_repstat != NFSERR_BADSESSION)
		printf("nfsrpc_bindconnsess: returned %d\n", nd->nd_repstat);
nfsmout:
	if (error != 0)
		printf("nfsrpc_bindconnsess: reply bad xdr\n");
	m_freem(nd->nd_mrep);
}

/*
 * Do roughly what nfs_statfs() does for NFSv4, but when called with a shared
 * locked vnode.
 */
static void
nfscl_statfs(struct vnode *vp, struct ucred *cred, NFSPROC_T *td)
{
	struct nfsvattr nfsva;
	struct nfsfsinfo fs;
	struct nfsstatfs sb;
	struct mount *mp;
	struct nfsmount *nmp;
	uint32_t lease;
	int attrflag, error;

	mp = vp->v_mount;
	nmp = VFSTONFS(mp);
	error = nfsrpc_statfs(vp, &sb, &fs, &lease, cred, td, &nfsva,
	    &attrflag);
	if (attrflag != 0)
		(void) nfscl_loadattrcache(&vp, &nfsva, NULL, 0, 1);
	if (error == 0) {
		NFSLOCKCLSTATE();
		if (nmp->nm_clp != NULL)
			nmp->nm_clp->nfsc_renew = NFSCL_RENEW(lease);
		NFSUNLOCKCLSTATE();
		mtx_lock(&nmp->nm_mtx);
		nfscl_loadfsinfo(nmp, &fs);
		nfscl_loadsbinfo(nmp, &sb, &mp->mnt_stat);
		mp->mnt_stat.f_iosize = newnfs_iosize(nmp);
		mtx_unlock(&nmp->nm_mtx);
	}
}
