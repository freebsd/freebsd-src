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
 *	@(#)nfs_serv.c  8.8 (Berkeley) 7/31/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * nfs version 2 and 3 server calls to vnode ops
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
 *	For Version 3, nfsm_reply() does not return for the error case, since
 *	most version 3 rpcs return more than the status for error cases.
 *
 * Other notes:
 *	Warning: always pay careful attention to resource cleanup on return
 *	and note that nfsm_*() macros can terminate a procedure on certain
 *	errors.
 *
 *	lookup() and namei()
 *	may return garbage in various structural fields/return elements
 *	if an error is returned, and may garbage up nd.ni_dvp even if no
 *	error is returned and you did not request LOCKPARENT or WANTPARENT.
 *
 *	We use the ni_cnd.cn_flags 'HASBUF' flag to track whether the name
 *	buffer has been freed or not.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/buf.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>

#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfsserver/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfsserver/nfsm_subs.h>

#ifdef NFSRV_DEBUG
#define nfsdbprintf(info)	printf info
#else
#define nfsdbprintf(info)
#endif

#define MAX_COMMIT_COUNT	(1024 * 1024)

#define NUM_HEURISTIC		64
#define NHUSE_INIT		64
#define NHUSE_INC		16
#define NHUSE_MAX		2048

static struct nfsheur {
	struct vnode *nh_vp;	/* vp to match (unreferenced pointer) */
	off_t nh_nextr;		/* next offset for sequential detection */
	int nh_use;		/* use count for selection */
	int nh_seqcount;	/* heuristic */
} nfsheur[NUM_HEURISTIC];

/* Global vars */

int nfsrvw_procrastinate = NFS_GATHERDELAY * 1000;
int nfsrvw_procrastinate_v3 = 0;

static struct timeval	nfsver = { 0 };

SYSCTL_NODE(_vfs, OID_AUTO, nfsrv, CTLFLAG_RW, 0, "NFS server");

static int nfs_async;
static int nfs_commit_blks;
static int nfs_commit_miss;
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, async, CTLFLAG_RW, &nfs_async, 0, "");
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, commit_blks, CTLFLAG_RW, &nfs_commit_blks, 0, "");
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, commit_miss, CTLFLAG_RW, &nfs_commit_miss, 0, "");

struct nfsrvstats nfsrvstats;
SYSCTL_STRUCT(_vfs_nfsrv, NFS_NFSRVSTATS, nfsrvstats, CTLFLAG_RD,
	&nfsrvstats, nfsrvstats, "S,nfsrvstats");

static int	nfsrv_access(struct vnode *, int, struct ucred *, int,
		    struct thread *, int);
static void	nfsrvw_coalesce(struct nfsrv_descript *,
		    struct nfsrv_descript *);

/*
 * Clear nameidata fields that are tested in nsfmout cleanup code prior
 * to using first nfsm macro (that might jump to the cleanup code).
 */

static __inline void
ndclear(struct nameidata *nd)
{

	nd->ni_cnd.cn_flags = 0;
	nd->ni_vp = NULL;
	nd->ni_dvp = NULL;
	nd->ni_startdir = NULL;
}

/*
 * nfs v3 access service
 */
int
nfsrv3_access(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly, getret;
	struct mbuf *mb, *mreq;
	struct vattr vattr, *vap = &vattr;
	u_long testmode, nfsmode;
	int v3 = (nfsd->nd_flag & ND_NFSV3);

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (!v3)
		panic("nfsrv3_access: v3 proc called on a v2 connection");
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(1, (struct vattr *)0);
		error = 0;
		goto nfsmout;
	}
	nfsmode = fxdr_unsigned(u_int32_t, *tl);
	if ((nfsmode & NFSV3ACCESS_READ) &&
		nfsrv_access(vp, VREAD, cred, rdonly, td, 0))
		nfsmode &= ~NFSV3ACCESS_READ;
	if (vp->v_type == VDIR)
		testmode = (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
			NFSV3ACCESS_DELETE);
	else
		testmode = (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
	if ((nfsmode & testmode) &&
		nfsrv_access(vp, VWRITE, cred, rdonly, td, 0))
		nfsmode &= ~testmode;
	if (vp->v_type == VDIR)
		testmode = NFSV3ACCESS_LOOKUP;
	else
		testmode = NFSV3ACCESS_EXECUTE;
	if ((nfsmode & testmode) &&
		nfsrv_access(vp, VEXEC, cred, rdonly, td, 0))
		nfsmode &= ~testmode;
	getret = VOP_GETATTR(vp, vap, cred, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_POSTOPATTR(1) + NFSX_UNSIGNED);
	nfsm_srvpostop_attr(getret, vap);
	tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(nfsmode);
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs getattr service
 */
int
nfsrv_getattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va;
	struct vattr *vap = &va;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly;
	struct mbuf *mb, *mreq;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(0);
		error = 0;
		goto nfsmout;
	}
	error = VOP_GETATTR(vp, vap, cred, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	if (error) {
		error = 0;
		goto nfsmout;
	}
	fp = nfsm_build(struct nfs_fattr *,
	    NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	nfsm_srvfillattr(vap, fp);
	/* fall through */

nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs setattr service
 */
int
nfsrv_setattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, preat;
	struct vattr *vap = &va;
	struct nfsv2_sattr *sp;
	struct nfs_fattr *fp;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly, preat_ret = 1, postat_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3), gcheck = 0;
	struct mbuf *mb, *mreq;
	struct timespec guard;
	struct mount *mp = NULL;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto out;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto out;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	VATTR_NULL(vap);
	if (v3) {
		nfsm_srvsattr(vap);
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		gcheck = fxdr_unsigned(int, *tl);
		if (gcheck) {
			tl = nfsm_dissect(u_int32_t *, 2 * NFSX_UNSIGNED);
			fxdr_nfsv3time(tl, &guard);
		}
	} else {
		sp = nfsm_dissect(struct nfsv2_sattr *, NFSX_V2SATTR);
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
			vap->va_size = fxdr_unsigned(u_quad_t, sp->sa_size);
		if (sp->sa_atime.nfsv2_sec != nfs_xdrneg1) {
#ifdef notyet
			fxdr_nfsv2time(&sp->sa_atime, &vap->va_atime);
#else
			vap->va_atime.tv_sec =
				fxdr_unsigned(int32_t, sp->sa_atime.nfsv2_sec);
			vap->va_atime.tv_nsec = 0;
#endif
		}
		if (sp->sa_mtime.nfsv2_sec != nfs_xdrneg1)
			fxdr_nfsv2time(&sp->sa_mtime, &vap->va_mtime);

	}

	/*
	 * Now that we have all the fields, lets do it.
	 */
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		if (v3)
			nfsm_srvwcc_data(preat_ret, &preat, postat_ret, vap);
		error = 0;
		goto nfsmout;
	}

	/*
	 * vp now an active resource, pay careful attention to cleanup
	 */
	if (v3) {
		error = preat_ret = VOP_GETATTR(vp, &preat, cred, td);
		if (!error && gcheck &&
			(preat.va_ctime.tv_sec != guard.tv_sec ||
			 preat.va_ctime.tv_nsec != guard.tv_nsec))
			error = NFSERR_NOT_SYNC;
		if (error) {
			vput(vp);
			vp = NULL;
			nfsm_reply(NFSX_WCCDATA(v3));
			if (v3)
				nfsm_srvwcc_data(preat_ret, &preat, postat_ret, vap);
			error = 0;
			goto nfsmout;
		}
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
		} else if ((error = nfsrv_access(vp, VWRITE, cred, rdonly,
			td, 0)) != 0)
			goto out;
	}
	error = VOP_SETATTR(vp, vap, cred, td);
	postat_ret = VOP_GETATTR(vp, vap, cred, td);
	if (!error)
		error = postat_ret;
out:
	if (vp != NULL)
		vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_WCCORFATTR(v3));
	if (v3) {
		nfsm_srvwcc_data(preat_ret, &preat, postat_ret, vap);
		error = 0;
		goto nfsmout;
	} else {
		/* v2 non-error case (see nfsm_reply). */
		fp = nfsm_build(struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
	/* fall through */

nfsmout:
	if (vp)
		vput(vp);
	vn_finished_write(mp);
	return(error);
}

/*
 * nfs lookup rpc
 */
int
nfsrv_lookup(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct nameidata nd, ind, *ndp = &nd;
	struct vnode *vp, *dirp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, len, dirattr_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3), pubflag;
	struct mbuf *mb, *mreq;
	struct vattr va, dirattr, *vap = &va;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	pubflag = nfs_ispublicfh(fhp);

	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = LOOKUP;
	nd.ni_cnd.cn_flags = LOCKLEAF | SAVESTART;
	error = nfs_namei(&nd, fhp, len, slp, nam, &md, &dpos,
		&dirp, td, pubflag);

	/*
	 * namei failure, only dirp to cleanup.  Clear out garbarge from
	 * structure in case macros jump to nfsmout.
	 */

	if (error) {
		if (dirp) {
			if (v3)
				dirattr_ret = VOP_GETATTR(dirp, &dirattr, cred,
					td);
			vrele(dirp);
			dirp = NULL;
		}
		nfsm_reply(NFSX_POSTOPATTR(v3));
		if (v3)
			nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		error = 0;
		goto nfsmout;
	}

	/*
	 * Locate index file for public filehandle
	 *
	 * error is 0 on entry and 0 on exit from this block.
	 */

	if (pubflag) {
		if (nd.ni_vp->v_type == VDIR && nfs_pub.np_index != NULL) {
			/*
			 * Setup call to lookup() to see if we can find
			 * the index file. Arguably, this doesn't belong
			 * in a kernel.. Ugh.  If an error occurs, do not
			 * try to install an index file and then clear the
			 * error.
			 *
			 * When we replace nd with ind and redirect ndp,
			 * maintenance of ni_startdir and ni_vp shift to
			 * ind and we have to clean them up in the old nd.
			 * However, the cnd resource continues to be maintained
			 * via the original nd.  Confused?  You aren't alone!
			 */
			ind = nd;
			VOP_UNLOCK(nd.ni_vp, 0, td);
			ind.ni_pathlen = strlen(nfs_pub.np_index);
			ind.ni_cnd.cn_nameptr = ind.ni_cnd.cn_pnbuf =
			    nfs_pub.np_index;
			ind.ni_startdir = nd.ni_vp;
			VREF(ind.ni_startdir);

			error = lookup(&ind);
			ind.ni_dvp = NULL;

			if (error == 0) {
				/*
				 * Found an index file. Get rid of
				 * the old references.  transfer nd.ni_vp'
				 */
				if (dirp)
					vrele(dirp);
				dirp = nd.ni_vp;
				nd.ni_vp = NULL;
				vrele(nd.ni_startdir);
				nd.ni_startdir = NULL;
				ndp = &ind;
			}
			error = 0;
		}
		/*
		 * If the public filehandle was used, check that this lookup
		 * didn't result in a filehandle outside the publicly exported
		 * filesystem.  We clear the poor vp here to avoid lockups due
		 * to NFS I/O.
		 */

		if (ndp->ni_vp->v_mount != nfs_pub.np_mount) {
			vput(nd.ni_vp);
			nd.ni_vp = NULL;
			error = EPERM;
		}
	}

	if (dirp) {
		if (v3)
			dirattr_ret = VOP_GETATTR(dirp, &dirattr, cred,
				td);
		vrele(dirp);
		dirp = NULL;
	}

	/*
	 * Resources at this point:
	 *	ndp->ni_vp	may not be NULL
	 *
	 */

	if (error) {
		nfsm_reply(NFSX_POSTOPATTR(v3));
		if (v3)
			nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		error = 0;
		goto nfsmout;
	}

	/*
	 * Clear out some resources prior to potentially blocking.  This
	 * is not as critical as ni_dvp resources in other routines, but
	 * it helps.
	 */
	vrele(ndp->ni_startdir);
	ndp->ni_startdir = NULL;
	NDFREE(&nd, NDF_ONLY_PNBUF);

	/*
	 * Get underlying attribute, then release remaining resources ( for
	 * the same potential blocking reason ) and reply.
	 */
	vp = ndp->ni_vp;
	bzero((caddr_t)fhp, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fhp->fh_fid);
	if (!error)
		error = VOP_GETATTR(vp, vap, cred, td);

	vput(vp);
	ndp->ni_vp = NULL;
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_POSTOPORFATTR(v3) + NFSX_POSTOPATTR(v3));
	if (error) {
		if (v3)
			nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		error = 0;
		goto nfsmout;
	}
	nfsm_srvfhtom(fhp, v3);
	if (v3) {
		nfsm_srvpostop_attr(0, vap);
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
	} else {
		fp = nfsm_build(struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}

nfsmout:
	if (dirp)
		vrele(dirp);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (ndp->ni_startdir)
		vrele(ndp->ni_startdir);
	if (ndp->ni_vp)
		vput(ndp->ni_vp);
	return (error);
}

/*
 * nfs readlink service
 */
int
nfsrv_readlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct iovec iv[(NFS_MAXPATHLEN+MLEN-1)/MLEN];
	struct iovec *ivp = iv;
	struct mbuf *mp;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly, i, tlen, len, getret;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mp3, *nmp, *mreq;
	struct vnode *vp = NULL;
	struct vattr attr;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	mp = (struct mbuf *)0;
#endif
	mp3 = NULL;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	len = 0;
	i = 0;
	while (len < NFS_MAXPATHLEN) {
		MGET(nmp, M_TRYWAIT, MT_DATA);
		MCLGET(nmp, M_TRYWAIT);
		nmp->m_len = NFSMSIZ(nmp);
		if (len == 0)
			mp3 = mp = nmp;
		else {
			mp->m_next = nmp;
			mp = nmp;
		}
		if ((len + mp->m_len) > NFS_MAXPATHLEN) {
			mp->m_len = NFS_MAXPATHLEN - len;
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
	uiop->uio_td = (struct thread *)0;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		if (v3)
			nfsm_srvpostop_attr(1, (struct vattr *)0);
		error = 0;
		goto nfsmout;
	}
	if (vp->v_type != VLNK) {
		if (v3)
			error = EINVAL;
		else
			error = ENXIO;
		goto out;
	}
	error = VOP_READLINK(vp, uiop, cred);
out:
	getret = VOP_GETATTR(vp, &attr, cred, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_UNSIGNED);
	if (v3) {
		nfsm_srvpostop_attr(getret, &attr);
		if (error) {
			error = 0;
			goto nfsmout;
		}
	}
	if (uiop->uio_resid > 0) {
		len -= uiop->uio_resid;
		tlen = nfsm_rndup(len);
		nfsm_adj(mp3, NFS_MAXPATHLEN-tlen, tlen-len);
	}
	tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(len);
	mb->m_next = mp3;
	mp3 = NULL;
nfsmout:
	if (mp3)
		m_freem(mp3);
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs read service
 */
int
nfsrv_read(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct iovec *iv;
	struct iovec *iv2;
	struct mbuf *m;
	struct nfs_fattr *fp;
	u_int32_t *tl;
	int i;
	caddr_t bpos;
	int error = 0, rdonly, cnt, len, left, siz, tlen, getret;
	int v3 = (nfsd->nd_flag & ND_NFSV3), reqlen;
	struct mbuf *mb, *mreq;
	struct mbuf *m2;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	struct vattr va, *vap = &va;
	struct nfsheur *nh;
	off_t off;
	int ioflag = 0;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (v3) {
		tl = nfsm_dissect(u_int32_t *, 2 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
	} else {
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *tl);
	}
	nfsm_srvstrsiz(reqlen, NFS_SRVMAXDATA(nfsd));

	/*
	 * Reference vp.  If an error occurs, vp will be invalid, but we
	 * have to NULL it just in case.  The macros might goto nfsmout
	 * as well.
	 */

	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		vp = NULL;
		nfsm_reply(2 * NFSX_UNSIGNED);
		if (v3)
			nfsm_srvpostop_attr(1, (struct vattr *)0);
		error = 0;
		goto nfsmout;
	}

	if (vp->v_type != VREG) {
		if (v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
	}
	if (!error) {
		if ((error = nfsrv_access(vp, VREAD, cred, rdonly, td, 1)) != 0)
			error = nfsrv_access(vp, VEXEC, cred, rdonly, td, 1);
	}
	getret = VOP_GETATTR(vp, vap, cred, td);
	if (!error)
		error = getret;
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_POSTOPATTR(v3));
		if (v3)
			nfsm_srvpostop_attr(getret, vap);
		error = 0;
		goto nfsmout;
	}

	/*
	 * Calculate byte count to read
	 */

	if (off >= vap->va_size)
		cnt = 0;
	else if ((off + reqlen) > vap->va_size)
		cnt = vap->va_size - off;
	else
		cnt = reqlen;

	/*
	 * Calculate seqcount for heuristic
	 */

	{
		int hi;
		int try = 4;

		/*
		 * Locate best candidate
		 */

		hi = ((int)(vm_offset_t)vp / sizeof(struct vnode)) & (NUM_HEURISTIC - 1);
		nh = &nfsheur[hi];

		while (try--) {
			if (nfsheur[hi].nh_vp == vp) {
				nh = &nfsheur[hi];
				break;
			}
			if (nfsheur[hi].nh_use > 0)
				--nfsheur[hi].nh_use;
			hi = (hi + 1) & (NUM_HEURISTIC - 1);
			if (nfsheur[hi].nh_use < nh->nh_use)
				nh = &nfsheur[hi];
		}

		if (nh->nh_vp != vp) {
			nh->nh_vp = vp;
			nh->nh_nextr = off;
			nh->nh_use = NHUSE_INIT;
			if (off == 0)
				nh->nh_seqcount = 4;
			else
				nh->nh_seqcount = 1;
		}

		/*
		 * Calculate heuristic
		 */

		if ((off == 0 && nh->nh_seqcount > 0) || off == nh->nh_nextr) {
			if (++nh->nh_seqcount > 127)
				nh->nh_seqcount = 127;
		} else if (nh->nh_seqcount > 1) {
			nh->nh_seqcount = 1;
		} else {
			nh->nh_seqcount = 0;
		}
		nh->nh_use += NHUSE_INC;
		if (nh->nh_use > NHUSE_MAX)
			nh->nh_use = NHUSE_MAX;
		ioflag |= nh->nh_seqcount << 16;
        }

	nfsm_reply(NFSX_POSTOPORFATTR(v3) + 3 * NFSX_UNSIGNED+nfsm_rndup(cnt));
	if (v3) {
		tl = nfsm_build(u_int32_t *, NFSX_V3FATTR + 4 * NFSX_UNSIGNED);
		*tl++ = nfs_true;
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V3FATTR / sizeof (u_int32_t));
	} else {
		tl = nfsm_build(u_int32_t *, NFSX_V2FATTR + NFSX_UNSIGNED);
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V2FATTR / sizeof (u_int32_t));
	}
	len = left = nfsm_rndup(cnt);
	if (cnt > 0) {
		/*
		 * Generate the mbuf list with the uio_iov ref. to it.
		 */
		i = 0;
		m = m2 = mb;
		while (left > 0) {
			siz = min(M_TRAILINGSPACE(m), left);
			if (siz > 0) {
				left -= siz;
				i++;
			}
			if (left > 0) {
				MGET(m, M_TRYWAIT, MT_DATA);
				MCLGET(m, M_TRYWAIT);
				m->m_len = 0;
				m2->m_next = m;
				m2 = m;
			}
		}
		MALLOC(iv, struct iovec *, i * sizeof (struct iovec),
		       M_TEMP, M_WAITOK);
		uiop->uio_iov = iv2 = iv;
		m = mb;
		left = len;
		i = 0;
		while (left > 0) {
			if (m == NULL)
				panic("nfsrv_read iov");
			siz = min(M_TRAILINGSPACE(m), left);
			if (siz > 0) {
				iv->iov_base = mtod(m, caddr_t) + m->m_len;
				iv->iov_len = siz;
				m->m_len += siz;
				left -= siz;
				iv++;
				i++;
			}
			m = m->m_next;
		}
		uiop->uio_iovcnt = i;
		uiop->uio_offset = off;
		uiop->uio_resid = len;
		uiop->uio_rw = UIO_READ;
		uiop->uio_segflg = UIO_SYSSPACE;
		error = VOP_READ(vp, uiop, IO_NODELOCKED | ioflag, cred);
		off = uiop->uio_offset;
		nh->nh_nextr = off;
		FREE((caddr_t)iv2, M_TEMP);
		if (error || (getret = VOP_GETATTR(vp, vap, cred, td))) {
			if (!error)
				error = getret;
			m_freem(mreq);
			vput(vp);
			vp = NULL;
			nfsm_reply(NFSX_POSTOPATTR(v3));
			if (v3)
				nfsm_srvpostop_attr(getret, vap);
			error = 0;
			goto nfsmout;
		}
	} else {
		uiop->uio_resid = 0;
	}
	vput(vp);
	vp = NULL;
	nfsm_srvfillattr(vap, fp);
	tlen = len - uiop->uio_resid;
	cnt = cnt < tlen ? cnt : tlen;
	tlen = nfsm_rndup(cnt);
	if (len != tlen || tlen != cnt)
		nfsm_adj(mb, len - tlen, tlen - cnt);
	if (v3) {
		*tl++ = txdr_unsigned(cnt);
		if (len < reqlen)
			*tl++ = nfs_true;
		else
			*tl++ = nfs_false;
	}
	*tl = txdr_unsigned(cnt);
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs write service
 */
int
nfsrv_write(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct iovec *ivp;
	int i, cnt;
	struct mbuf *mp;
	struct nfs_fattr *fp;
	struct iovec *iv;
	struct vattr va, forat;
	struct vattr *vap = &va;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly, len, forat_ret = 1;
	int ioflags, aftat_ret = 1, retlen = 0, zeroing, adjust;
	int stable = NFSV3WRITE_FILESYNC;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	off_t off;
	struct mount *mntp = NULL;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (mrep == NULL) {
		*mrq = NULL;
		error = 0;
		goto nfsmout;
	}
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mntp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto ereply;
	}
	if ((error = VFS_FHTOVP(mntp, &fhp->fh_fid, &vp)) != 0) {
		mntp = NULL;
		goto ereply;
	}
	(void) vn_start_write(vp, &mntp, V_WAIT);
	vput(vp);
	vp = NULL;
	if (v3) {
		tl = nfsm_dissect(u_int32_t *, 5 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
		tl += 3;
		stable = fxdr_unsigned(int, *tl++);
	} else {
		tl = nfsm_dissect(u_int32_t *, 4 * NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
		if (nfs_async)
	    		stable = NFSV3WRITE_UNSTABLE;
	}
	retlen = len = fxdr_unsigned(int32_t, *tl);
	cnt = i = 0;

	/*
	 * For NFS Version 2, it is not obvious what a write of zero length
	 * should do, but I might as well be consistent with Version 3,
	 * which is to return ok so long as there are no permission problems.
	 */
	if (len > 0) {
	    zeroing = 1;
	    mp = mrep;
	    while (mp) {
		if (mp == md) {
			zeroing = 0;
			adjust = dpos - mtod(mp, caddr_t);
			mp->m_len -= adjust;
			if (mp->m_len > 0 && adjust > 0)
				mp->m_data += adjust;
		}
		if (zeroing)
			mp->m_len = 0;
		else if (mp->m_len > 0) {
			i += mp->m_len;
			if (i > len) {
				mp->m_len -= (i - len);
				zeroing	= 1;
			}
			if (mp->m_len > 0)
				cnt++;
		}
		mp = mp->m_next;
	    }
	}
	if (len > NFS_MAXDATA || len < 0 || i < len) {
		error = EIO;
		nfsm_reply(2 * NFSX_UNSIGNED);
		if (v3)
			nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		error = 0;
		goto nfsmout;
	}
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		vp = NULL;
		nfsm_reply(2 * NFSX_UNSIGNED);
		if (v3)
			nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		error = 0;
		goto nfsmout;
	}
	if (v3)
		forat_ret = VOP_GETATTR(vp, &forat, cred, td);
	if (vp->v_type != VREG) {
		if (v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
	}
	if (!error)
		error = nfsrv_access(vp, VWRITE, cred, rdonly, td, 1);
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_WCCDATA(v3));
		if (v3)
			nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		error = 0;
		goto nfsmout;
	}

	if (len > 0) {
	    MALLOC(ivp, struct iovec *, cnt * sizeof (struct iovec), M_TEMP,
		M_WAITOK);
	    uiop->uio_iov = iv = ivp;
	    uiop->uio_iovcnt = cnt;
	    mp = mrep;
	    while (mp) {
		if (mp->m_len > 0) {
			ivp->iov_base = mtod(mp, caddr_t);
			ivp->iov_len = mp->m_len;
			ivp++;
		}
		mp = mp->m_next;
	    }

	    /*
	     * XXX
	     * The IO_METASYNC flag indicates that all metadata (and not just
	     * enough to ensure data integrity) mus be written to stable storage
	     * synchronously.
	     * (IO_METASYNC is not yet implemented in 4.4BSD-Lite.)
	     */
	    if (stable == NFSV3WRITE_UNSTABLE)
		ioflags = IO_NODELOCKED;
	    else if (stable == NFSV3WRITE_DATASYNC)
		ioflags = (IO_SYNC | IO_NODELOCKED);
	    else
		ioflags = (IO_METASYNC | IO_SYNC | IO_NODELOCKED);
	    uiop->uio_resid = len;
	    uiop->uio_rw = UIO_WRITE;
	    uiop->uio_segflg = UIO_SYSSPACE;
	    uiop->uio_td = (struct thread *)0;
	    uiop->uio_offset = off;
	    error = VOP_WRITE(vp, uiop, ioflags, cred);
	    nfsrvstats.srvvop_writes++;
	    FREE((caddr_t)iv, M_TEMP);
	}
	aftat_ret = VOP_GETATTR(vp, vap, cred, td);
	vput(vp);
	vp = NULL;
	if (!error)
		error = aftat_ret;
ereply:
	nfsm_reply(NFSX_PREOPATTR(v3) + NFSX_POSTOPORFATTR(v3) +
		2 * NFSX_UNSIGNED + NFSX_WRITEVERF(v3));
	if (v3) {
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		if (error) {
			error = 0;
			goto nfsmout;
		}
		tl = nfsm_build(u_int32_t *, 4 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(retlen);
		/*
		 * If nfs_async is set, then pretend the write was FILESYNC.
		 */
		if (stable == NFSV3WRITE_UNSTABLE && !nfs_async)
			*tl++ = txdr_unsigned(stable);
		else
			*tl++ = txdr_unsigned(NFSV3WRITE_FILESYNC);
		/*
		 * Actually, there is no need to txdr these fields,
		 * but it may make the values more human readable,
		 * for debugging purposes.
		 */
		if (nfsver.tv_sec == 0)
			nfsver = boottime;
		*tl++ = txdr_unsigned(nfsver.tv_sec);
		*tl = txdr_unsigned(nfsver.tv_usec);
	} else {
		/* v2, non-error case (see nfsm_reply). */
		fp = nfsm_build(struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
nfsmout:
	if (vp)
		vput(vp);
	vn_finished_write(mntp);
	return(error);
}

/*
 * NFS write service with write gathering support. Called when
 * nfsrvw_procrastinate > 0.
 * See: Chet Juszczak, "Improving the Write Performance of an NFS Server",
 * in Proc. of the Winter 1994 Usenix Conference, pg. 247-259, San Franscisco,
 * Jan. 1994.
 */
int
nfsrv_writegather(struct nfsrv_descript **ndp, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct iovec *ivp;
	struct mbuf *mp;
	struct nfsrv_descript *wp, *nfsd, *owp, *swp;
	struct nfs_fattr *fp;
	int i;
	struct iovec *iov;
	struct nfsrvw_delayhash *wpp;
	struct ucred *cred;
	struct vattr va, forat;
	u_int32_t *tl;
	caddr_t bpos, dpos;
	int error = 0, rdonly, len, forat_ret = 1;
	int ioflags, aftat_ret = 1, s, adjust, v3, zeroing;
	struct mbuf *mb, *mreq, *mrep, *md;
	struct vnode *vp = NULL;
	struct uio io, *uiop = &io;
	u_quad_t cur_usec;
	struct mount *mntp = NULL;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	i = 0;
	len = 0;
#endif
	*mrq = NULL;
	if (*ndp) {
	    nfsd = *ndp;
	    *ndp = NULL;
	    mrep = nfsd->nd_mrep;
	    md = nfsd->nd_md;
	    dpos = nfsd->nd_dpos;
	    cred = &nfsd->nd_cr;
	    v3 = (nfsd->nd_flag & ND_NFSV3);
	    LIST_INIT(&nfsd->nd_coalesce);
	    nfsd->nd_mreq = NULL;
	    nfsd->nd_stable = NFSV3WRITE_FILESYNC;
	    cur_usec = nfs_curusec();
	    nfsd->nd_time = cur_usec +
		(v3 ? nfsrvw_procrastinate_v3 : nfsrvw_procrastinate);

	    /*
	     * Now, get the write header..
	     */
	    nfsm_srvmtofh(&nfsd->nd_fh);
	    if (v3) {
		tl = nfsm_dissect(u_int32_t *, 5 * NFSX_UNSIGNED);
		nfsd->nd_off = fxdr_hyper(tl);
		tl += 3;
		nfsd->nd_stable = fxdr_unsigned(int, *tl++);
	    } else {
		tl = nfsm_dissect(u_int32_t *, 4 * NFSX_UNSIGNED);
		nfsd->nd_off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
		if (nfs_async)
			nfsd->nd_stable = NFSV3WRITE_UNSTABLE;
	    }
	    len = fxdr_unsigned(int32_t, *tl);
	    nfsd->nd_len = len;
	    nfsd->nd_eoff = nfsd->nd_off + len;

	    /*
	     * Trim the header out of the mbuf list and trim off any trailing
	     * junk so that the mbuf list has only the write data.
	     */
	    zeroing = 1;
	    i = 0;
	    mp = mrep;
	    while (mp) {
		if (mp == md) {
		    zeroing = 0;
		    adjust = dpos - mtod(mp, caddr_t);
		    mp->m_len -= adjust;
		    if (mp->m_len > 0 && adjust > 0)
			mp->m_data += adjust;
		}
		if (zeroing)
		    mp->m_len = 0;
		else {
		    i += mp->m_len;
		    if (i > len) {
			mp->m_len -= (i - len);
			zeroing = 1;
		    }
		}
		mp = mp->m_next;
	    }
	    if (len > NFS_MAXDATA || len < 0  || i < len) {
nfsmout:
		m_freem(mrep);
		error = EIO;
		nfsm_writereply(2 * NFSX_UNSIGNED);
		if (v3)
		    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
		nfsd->nd_mreq = mreq;
		nfsd->nd_mrep = NULL;
		nfsd->nd_time = 0;
	    }

	    /*
	     * Add this entry to the hash and time queues.
	     */
	    s = splsoftclock();
	    owp = NULL;
	    wp = LIST_FIRST(&slp->ns_tq);
	    while (wp && wp->nd_time < nfsd->nd_time) {
		owp = wp;
		wp = LIST_NEXT(wp, nd_tq);
	    }
	    NFS_DPF(WG, ("Q%03x", nfsd->nd_retxid & 0xfff));
	    if (owp) {
		LIST_INSERT_AFTER(owp, nfsd, nd_tq);
	    } else {
		LIST_INSERT_HEAD(&slp->ns_tq, nfsd, nd_tq);
	    }
	    if (nfsd->nd_mrep) {
		wpp = NWDELAYHASH(slp, nfsd->nd_fh.fh_fid.fid_data);
		owp = NULL;
		wp = LIST_FIRST(wpp);
		while (wp &&
		    bcmp((caddr_t)&nfsd->nd_fh,(caddr_t)&wp->nd_fh, NFSX_V3FH)){
		    owp = wp;
		    wp = LIST_NEXT(wp, nd_hash);
		}
		while (wp && wp->nd_off < nfsd->nd_off &&
		    !bcmp((caddr_t)&nfsd->nd_fh,(caddr_t)&wp->nd_fh, NFSX_V3FH)) {
		    owp = wp;
		    wp = LIST_NEXT(wp, nd_hash);
		}
		if (owp) {
		    LIST_INSERT_AFTER(owp, nfsd, nd_hash);

		    /*
		     * Search the hash list for overlapping entries and
		     * coalesce.
		     */
		    for(; nfsd && NFSW_CONTIG(owp, nfsd); nfsd = wp) {
			wp = LIST_NEXT(nfsd, nd_hash);
			if (NFSW_SAMECRED(owp, nfsd))
			    nfsrvw_coalesce(owp, nfsd);
		    }
		} else {
		    LIST_INSERT_HEAD(wpp, nfsd, nd_hash);
		}
	    }
	    splx(s);
	}

	/*
	 * Now, do VOP_WRITE()s for any one(s) that need to be done now
	 * and generate the associated reply mbuf list(s).
	 */
loop1:
	cur_usec = nfs_curusec();
	s = splsoftclock();
	for (nfsd = LIST_FIRST(&slp->ns_tq); nfsd; nfsd = owp) {
		owp = LIST_NEXT(nfsd, nd_tq);
		if (nfsd->nd_time > cur_usec)
		    break;
		if (nfsd->nd_mreq)
		    continue;
		NFS_DPF(WG, ("P%03x", nfsd->nd_retxid & 0xfff));
		LIST_REMOVE(nfsd, nd_tq);
		LIST_REMOVE(nfsd, nd_hash);
		splx(s);
		mrep = nfsd->nd_mrep;
		nfsd->nd_mrep = NULL;
		cred = &nfsd->nd_cr;
		v3 = (nfsd->nd_flag & ND_NFSV3);
		forat_ret = aftat_ret = 1;
		error = nfsrv_fhtovp(&nfsd->nd_fh, 1, &vp, cred, slp,
		    nfsd->nd_nam, &rdonly, TRUE);
		if (!error) {
		    if (v3)
			forat_ret = VOP_GETATTR(vp, &forat, cred, td);
		    if (vp->v_type != VREG) {
			if (v3)
			    error = EINVAL;
			else
			    error = (vp->v_type == VDIR) ? EISDIR : EACCES;
		    }
		} else {
		    vp = NULL;
		}
		if (!error)
		    error = nfsrv_access(vp, VWRITE, cred, rdonly, td, 1);
		if (nfsd->nd_stable == NFSV3WRITE_UNSTABLE)
		    ioflags = IO_NODELOCKED;
		else if (nfsd->nd_stable == NFSV3WRITE_DATASYNC)
		    ioflags = (IO_SYNC | IO_NODELOCKED);
		else
		    ioflags = (IO_METASYNC | IO_SYNC | IO_NODELOCKED);
		uiop->uio_rw = UIO_WRITE;
		uiop->uio_segflg = UIO_SYSSPACE;
		uiop->uio_td = (struct thread *)0;
		uiop->uio_offset = nfsd->nd_off;
		uiop->uio_resid = nfsd->nd_eoff - nfsd->nd_off;
		if (uiop->uio_resid > 0) {
		    mp = mrep;
		    i = 0;
		    while (mp) {
			if (mp->m_len > 0)
			    i++;
			mp = mp->m_next;
		    }
		    uiop->uio_iovcnt = i;
		    MALLOC(iov, struct iovec *, i * sizeof (struct iovec),
			M_TEMP, M_WAITOK);
		    uiop->uio_iov = ivp = iov;
		    mp = mrep;
		    while (mp) {
			if (mp->m_len > 0) {
			    ivp->iov_base = mtod(mp, caddr_t);
			    ivp->iov_len = mp->m_len;
			    ivp++;
			}
			mp = mp->m_next;
		    }
		    if (!error) {
			if (vn_start_write(vp, &mntp, V_NOWAIT) != 0) {
			    VOP_UNLOCK(vp, 0, td);
			    error = vn_start_write(NULL, &mntp, V_WAIT);
			    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
			}
		    }
		    if (!error) {
			error = VOP_WRITE(vp, uiop, ioflags, cred);
			nfsrvstats.srvvop_writes++;
			vn_finished_write(mntp);
		    }
		    FREE((caddr_t)iov, M_TEMP);
		}
		m_freem(mrep);
		if (vp) {
		    aftat_ret = VOP_GETATTR(vp, &va, cred, td);
		    vput(vp);
		    vp = NULL;
		}

		/*
		 * Loop around generating replies for all write rpcs that have
		 * now been completed.
		 */
		swp = nfsd;
		do {
		    NFS_DPF(WG, ("R%03x", nfsd->nd_retxid & 0xfff));
		    if (error) {
			nfsm_writereply(NFSX_WCCDATA(v3));
			if (v3) {
			    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
			}
		    } else {
			nfsm_writereply(NFSX_PREOPATTR(v3) +
			    NFSX_POSTOPORFATTR(v3) + 2 * NFSX_UNSIGNED +
			    NFSX_WRITEVERF(v3));
			if (v3) {
			    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
			    tl = nfsm_build(u_int32_t *, 4 * NFSX_UNSIGNED);
			    *tl++ = txdr_unsigned(nfsd->nd_len);
			    *tl++ = txdr_unsigned(swp->nd_stable);
			    /*
			     * Actually, there is no need to txdr these fields,
			     * but it may make the values more human readable,
			     * for debugging purposes.
			     */
			    if (nfsver.tv_sec == 0)
				    nfsver = boottime;
			    *tl++ = txdr_unsigned(nfsver.tv_sec);
			    *tl = txdr_unsigned(nfsver.tv_usec);
			} else {
			    fp = nfsm_build(struct nfs_fattr *, NFSX_V2FATTR);
			    nfsm_srvfillattr(&va, fp);
			}
		    }
		    nfsd->nd_mreq = mreq;
		    if (nfsd->nd_mrep)
			panic("nfsrv_write: nd_mrep not free");

		    /*
		     * Done. Put it at the head of the timer queue so that
		     * the final phase can return the reply.
		     */
		    s = splsoftclock();
		    if (nfsd != swp) {
			nfsd->nd_time = 0;
			LIST_INSERT_HEAD(&slp->ns_tq, nfsd, nd_tq);
		    }
		    nfsd = LIST_FIRST(&swp->nd_coalesce);
		    if (nfsd) {
			LIST_REMOVE(nfsd, nd_tq);
		    }
		    splx(s);
		} while (nfsd);
		s = splsoftclock();
		swp->nd_time = 0;
		LIST_INSERT_HEAD(&slp->ns_tq, swp, nd_tq);
		splx(s);
		goto loop1;
	}
	splx(s);

	/*
	 * Search for a reply to return.
	 */
	s = splsoftclock();
	LIST_FOREACH(nfsd, &slp->ns_tq, nd_tq)
		if (nfsd->nd_mreq) {
		    NFS_DPF(WG, ("X%03x", nfsd->nd_retxid & 0xfff));
		    LIST_REMOVE(nfsd, nd_tq);
		    *mrq = nfsd->nd_mreq;
		    *ndp = nfsd;
		    break;
		}
	splx(s);
	return (0);
}

/*
 * Coalesce the write request nfsd into owp. To do this we must:
 * - remove nfsd from the queues
 * - merge nfsd->nd_mrep into owp->nd_mrep
 * - update the nd_eoff and nd_stable for owp
 * - put nfsd on owp's nd_coalesce list
 * NB: Must be called at splsoftclock().
 */
static void
nfsrvw_coalesce(struct nfsrv_descript *owp, struct nfsrv_descript *nfsd)
{
        int overlap;
        struct mbuf *mp;
	struct nfsrv_descript *p;

	NFS_DPF(WG, ("C%03x-%03x",
		     nfsd->nd_retxid & 0xfff, owp->nd_retxid & 0xfff));
        LIST_REMOVE(nfsd, nd_hash);
        LIST_REMOVE(nfsd, nd_tq);
        if (owp->nd_eoff < nfsd->nd_eoff) {
            overlap = owp->nd_eoff - nfsd->nd_off;
            if (overlap < 0)
                panic("nfsrv_coalesce: bad off");
            if (overlap > 0)
                m_adj(nfsd->nd_mrep, overlap);
            mp = owp->nd_mrep;
            while (mp->m_next)
                mp = mp->m_next;
            mp->m_next = nfsd->nd_mrep;
            owp->nd_eoff = nfsd->nd_eoff;
        } else
            m_freem(nfsd->nd_mrep);
        nfsd->nd_mrep = NULL;
        if (nfsd->nd_stable == NFSV3WRITE_FILESYNC)
            owp->nd_stable = NFSV3WRITE_FILESYNC;
        else if (nfsd->nd_stable == NFSV3WRITE_DATASYNC &&
            owp->nd_stable == NFSV3WRITE_UNSTABLE)
            owp->nd_stable = NFSV3WRITE_DATASYNC;
        LIST_INSERT_HEAD(&owp->nd_coalesce, nfsd, nd_tq);

	/*
	 * If nfsd had anything else coalesced into it, transfer them
	 * to owp, otherwise their replies will never get sent.
	 */
	for (p = LIST_FIRST(&nfsd->nd_coalesce); p;
	     p = LIST_FIRST(&nfsd->nd_coalesce)) {
	    LIST_REMOVE(p, nd_tq);
	    LIST_INSERT_HEAD(&owp->nd_coalesce, p, nd_tq);
	}
}

/*
 * nfs create service
 * now does a truncate to 0 length via. setattr if it already exists
 */
int
nfsrv_create(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va, dirfor, diraft;
	struct vattr *vap = &va;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	struct nameidata nd;
	caddr_t bpos;
	int error = 0, rdev, len, tsize, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3), how, exclusive_flag = 0;
	caddr_t cp;
	struct mbuf *mb, *mreq;
	struct vnode *dirp = (struct vnode *)0;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t tempsize;
	u_char cverf[NFSX_V3CREATEVERF];
	struct mount *mp = NULL;
	struct vnode *vp;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	rdev = 0;
#endif
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto ereply;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto ereply;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	nfsm_srvnamesiz(len);

	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | SAVESTART;

	/*
	 * Call namei and do initial cleanup to get a few things
	 * out of the way.  If we get an initial error we cleanup
	 * and return here to avoid special-casing the invalid nd
	 * structure through the rest of the case.  dirp may be
	 * set even if an error occurs, but the nd structure will not
	 * be valid at all if an error occurs so we have to invalidate it
	 * prior to calling nfsm_reply ( which might goto nfsmout ).
	 */
	error = nfs_namei(&nd, fhp, len, slp, nam, &md, &dpos,
		&dirp, td, FALSE);
	if (dirp) {
		if (v3) {
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				td);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(v3));
		if (v3)
			nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}

	/*
	 * No error.  Continue.  State:
	 *
	 *	startdir	is valid ( we release this immediately )
	 *	dirp 		may be valid
	 *	nd.ni_vp	may be valid
	 *	nd.ni_dvp	is valid
	 *
	 * The error state is set through the code and we may also do some
	 * opportunistic releasing of vnodes to avoid holding locks through
	 * NFS I/O.  The cleanup at the end is a catch-all
	 */

	VATTR_NULL(vap);
	if (v3) {
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		how = fxdr_unsigned(int, *tl);
		switch (how) {
		case NFSV3CREATE_GUARDED:
			if (nd.ni_vp) {
				error = EEXIST;
				break;
			}
			/* fall through */
		case NFSV3CREATE_UNCHECKED:
			nfsm_srvsattr(vap);
			break;
		case NFSV3CREATE_EXCLUSIVE:
			cp = nfsm_dissect(caddr_t, NFSX_V3CREATEVERF);
			bcopy(cp, cverf, NFSX_V3CREATEVERF);
			exclusive_flag = 1;
			if (nd.ni_vp == NULL)
				vap->va_mode = 0;
			break;
		};
		vap->va_type = VREG;
	} else {
		sp = nfsm_dissect(struct nfsv2_sattr *, NFSX_V2SATTR);
		vap->va_type = IFTOVT(fxdr_unsigned(u_int32_t, sp->sa_mode));
		if (vap->va_type == VNON)
			vap->va_type = VREG;
		vap->va_mode = nfstov_mode(sp->sa_mode);
		switch (vap->va_type) {
		case VREG:
			tsize = fxdr_unsigned(int32_t, sp->sa_size);
			if (tsize != -1)
				vap->va_size = (u_quad_t)tsize;
			break;
		case VCHR:
		case VBLK:
		case VFIFO:
			rdev = fxdr_unsigned(long, sp->sa_size);
			break;
		default:
			break;
		};
	}

	/*
	 * Iff doesn't exist, create it
	 * otherwise just truncate to 0 length
	 *   should I set the mode too ?
	 *
	 * The only possible error we can have at this point is EEXIST.
	 * nd.ni_vp will also be non-NULL in that case.
	 */
	if (nd.ni_vp == NULL) {
		if (vap->va_type == VREG || vap->va_type == VSOCK) {
			error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap);
			if (error)
				NDFREE(&nd, NDF_ONLY_PNBUF);
			else {
			    	nfsrv_object_create(nd.ni_vp);
				if (exclusive_flag) {
					exclusive_flag = 0;
					VATTR_NULL(vap);
					bcopy(cverf, (caddr_t)&vap->va_atime,
						NFSX_V3CREATEVERF);
					error = VOP_SETATTR(nd.ni_vp, vap, cred,
						td);
				}
			}
		} else if (
			vap->va_type == VCHR ||
			vap->va_type == VBLK ||
			vap->va_type == VFIFO
		) {
			/*
			 * Handle SysV FIFO node special cases.  All other
			 * devices require super user to access.
			 */
			if (vap->va_type == VCHR && rdev == 0xffffffff)
				vap->va_type = VFIFO;
                        if (vap->va_type != VFIFO &&
                            (error = suser_xxx(cred, 0, 0))) {
				goto nfsmreply0;
                        }
			vap->va_rdev = rdev;
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap);
			if (error) {
				NDFREE(&nd, NDF_ONLY_PNBUF);
				goto nfsmreply0;
			}
			vput(nd.ni_vp);
			nd.ni_vp = NULL;

			/*
			 * release dvp prior to lookup
			 */
			vput(nd.ni_dvp);
			nd.ni_dvp = NULL;

			/*
			 * Setup for lookup.
			 *
			 * Even though LOCKPARENT was cleared, ni_dvp may
			 * be garbage.
			 */
			nd.ni_cnd.cn_nameiop = LOOKUP;
			nd.ni_cnd.cn_flags &= ~(LOCKPARENT);
			nd.ni_cnd.cn_thread = td;
			nd.ni_cnd.cn_cred = cred;

			error = lookup(&nd);
			nd.ni_dvp = NULL;

			if (error != 0) {
				nfsm_reply(0);
				/* fall through on certain errors */
			}
			nfsrv_object_create(nd.ni_vp);
			if (nd.ni_cnd.cn_flags & ISSYMLINK) {
				error = EINVAL;
				goto nfsmreply0;
			}
		} else {
			error = ENXIO;
		}
	} else {
		if (vap->va_size != -1) {
			error = nfsrv_access(nd.ni_vp, VWRITE, cred,
			    (nd.ni_cnd.cn_flags & RDONLY), td, 0);
			if (!error) {
				tempsize = vap->va_size;
				VATTR_NULL(vap);
				vap->va_size = tempsize;
				error = VOP_SETATTR(nd.ni_vp, vap, cred,
					 td);
			}
		}
	}

	if (!error) {
		bzero((caddr_t)fhp, sizeof(nfh));
		fhp->fh_fsid = nd.ni_vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(nd.ni_vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(nd.ni_vp, vap, cred, td);
	}
	if (v3) {
		if (exclusive_flag && !error &&
			bcmp(cverf, (caddr_t)&vap->va_atime, NFSX_V3CREATEVERF))
			error = EEXIST;
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, td);
		vrele(dirp);
		dirp = NULL;
	}
ereply:
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_FATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(0, vap);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	} else {
		/* v2 non-error case (see nfsm_reply). */
		nfsm_srvfhtom(fhp, v3);
		fp = nfsm_build(struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
	goto nfsmout;

nfsmreply0:
	nfsm_reply(0);
nfsmout:
	if (nd.ni_startdir) {
		vrele(nd.ni_startdir);
		nd.ni_startdir = NULL;
	}
	if (dirp)
		vrele(dirp);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_dvp) {
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
	}
	if (nd.ni_vp)
		vput(nd.ni_vp);
	vn_finished_write(mp);
	return (error);
}

/*
 * nfs v3 mknod service
 */
int
nfsrv_mknod(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct vattr *vap = &va;
	u_int32_t *tl;
	struct nameidata nd;
	caddr_t bpos;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	u_int32_t major, minor;
	enum vtype vtyp;
	struct mbuf *mb, *mreq;
	struct vnode *vp, *dirp = (struct vnode *)0;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct mount *mp = NULL;
	int v3 = (nfsd->nd_flag & ND_NFSV3);

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (!v3)
		panic("nfsrv_mknod: v3 proc called on a v2 connection");
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto ereply;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto ereply;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	nfsm_srvnamesiz(len);

	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | SAVESTART;

	/*
	 * Handle nfs_namei() call.  If an error occurs, the nd structure
	 * is not valid.  However, nfsm_*() routines may still jump to
	 * nfsmout.
	 */

	error = nfs_namei(&nd, fhp, len, slp, nam, &md, &dpos,
		&dirp, td, FALSE);
	if (dirp)
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred, td);
	if (error) {
		nfsm_reply(NFSX_WCCDATA(1));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}
	tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
	vtyp = nfsv3tov_type(*tl);
	if (vtyp != VCHR && vtyp != VBLK && vtyp != VSOCK && vtyp != VFIFO) {
		error = NFSERR_BADTYPE;
		goto out;
	}
	VATTR_NULL(vap);
	nfsm_srvsattr(vap);
	if (vtyp == VCHR || vtyp == VBLK) {
		tl = nfsm_dissect(u_int32_t *, 2 * NFSX_UNSIGNED);
		major = fxdr_unsigned(u_int32_t, *tl++);
		minor = fxdr_unsigned(u_int32_t, *tl);
		vap->va_rdev = makeudev(major, minor);
	}

	/*
	 * Iff doesn't exist, create it.
	 */
	if (nd.ni_vp) {
		error = EEXIST;
		goto out;
	}
	vap->va_type = vtyp;
	if (vtyp == VSOCK) {
		vrele(nd.ni_startdir);
		nd.ni_startdir = NULL;
		error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap);
		if (error)
			NDFREE(&nd, NDF_ONLY_PNBUF);
	} else {
		if (vtyp != VFIFO && (error = suser_xxx(cred, 0, 0)))
			goto out;
		error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap);
		if (error) {
			NDFREE(&nd, NDF_ONLY_PNBUF);
			goto out;
		}
		vput(nd.ni_vp);
		nd.ni_vp = NULL;

		/*
		 * Release dvp prior to lookup
		 */
		vput(nd.ni_dvp);
		nd.ni_dvp = NULL;

		nd.ni_cnd.cn_nameiop = LOOKUP;
		nd.ni_cnd.cn_flags &= ~(LOCKPARENT);
		nd.ni_cnd.cn_thread = td;
		nd.ni_cnd.cn_cred = td->td_proc->p_ucred;

		error = lookup(&nd);
		nd.ni_dvp = NULL;

		if (error)
			goto out;
		if (nd.ni_cnd.cn_flags & ISSYMLINK)
			error = EINVAL;
	}

	/*
	 * send response, cleanup, return.
	 */
out:
	if (nd.ni_startdir) {
		vrele(nd.ni_startdir);
		nd.ni_startdir = NULL;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_dvp) {
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		nd.ni_dvp = NULL;
	}
	vp = nd.ni_vp;
	if (!error) {
		bzero((caddr_t)fhp, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(vp, vap, cred, td);
		vput(vp);
		vp = NULL;
		nd.ni_vp = NULL;
	}
	diraft_ret = VOP_GETATTR(dirp, &diraft, cred, td);
	if (dirp) {
		vrele(dirp);
		dirp = NULL;
	}
ereply:
	nfsm_reply(NFSX_SRVFH(1) + NFSX_POSTOPATTR(1) + NFSX_WCCDATA(1));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(0, vap);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	}
	vn_finished_write(mp);
	return (0);
nfsmout:
	if (dirp)
		vrele(dirp);
	if (nd.ni_startdir)
		vrele(nd.ni_startdir);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_dvp) {
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
	}
	if (nd.ni_vp)
		vput(nd.ni_vp);
	vn_finished_write(mp);
	return (error);
}

/*
 * nfs remove service
 */
int
nfsrv_remove(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nameidata nd;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct vnode *dirp;
	struct vattr dirfor, diraft;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct mount *mp = NULL;
	struct vnode *vp;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto ereply;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto ereply;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	nfsm_srvnamesiz(len);

	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, fhp, len, slp, nam, &md, &dpos,
		&dirp, td, FALSE);
	if (dirp) {
		if (v3) {
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				td);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error == 0) {
		if (nd.ni_vp->v_type == VDIR) {
			error = EPERM;		/* POSIX */
			goto out;
		}
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 */
		if (nd.ni_vp->v_flag & VROOT) {
			error = EBUSY;
			goto out;
		}
out:
		if (!error) {
			error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
			NDFREE(&nd, NDF_ONLY_PNBUF);
		}
	}
	if (dirp && v3) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, td);
		vrele(dirp);
		dirp = NULL;
	}
ereply:
	nfsm_reply(NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	}
nfsmout:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_dvp) {
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
	}
	if (nd.ni_vp)
		vput(nd.ni_vp);
	vn_finished_write(mp);
	return(error);
}

/*
 * nfs rename service
 */
int
nfsrv_rename(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, len, len2, fdirfor_ret = 1, fdiraft_ret = 1;
	int tdirfor_ret = 1, tdiraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct nameidata fromnd, tond;
	struct vnode *fvp, *tvp, *tdvp, *fdirp = (struct vnode *)0;
	struct vnode *tdirp = (struct vnode *)0;
	struct vattr fdirfor, fdiraft, tdirfor, tdiraft;
	nfsfh_t fnfh, tnfh;
	fhandle_t *ffhp, *tfhp;
	uid_t saved_uid;
	struct mount *mp = NULL;
	struct vnode *vp;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	fvp = (struct vnode *)0;
#endif
	ffhp = &fnfh.fh_generic;
	tfhp = &tnfh.fh_generic;

	/*
	 * Clear fields incase goto nfsmout occurs from macro.
	 */

	ndclear(&fromnd);
	ndclear(&tond);

	nfsm_srvmtofh(ffhp);
	if ((mp = vfs_getvfs(&ffhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto out1;
	}
	if ((error = VFS_FHTOVP(mp, &ffhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto out1;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	nfsm_srvnamesiz(len);
	/*
	 * Remember our original uid so that we can reset cr_uid before
	 * the second nfs_namei() call, in case it is remapped.
	 */
	saved_uid = cred->cr_uid;
	fromnd.ni_cnd.cn_cred = cred;
	fromnd.ni_cnd.cn_nameiop = DELETE;
	fromnd.ni_cnd.cn_flags = WANTPARENT | SAVESTART;
	error = nfs_namei(&fromnd, ffhp, len, slp, nam, &md,
		&dpos, &fdirp, td, FALSE);
	if (fdirp) {
		if (v3) {
			fdirfor_ret = VOP_GETATTR(fdirp, &fdirfor, cred,
				td);
		} else {
			vrele(fdirp);
			fdirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(2 * NFSX_WCCDATA(v3));
		if (v3) {
			nfsm_srvwcc_data(fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
			nfsm_srvwcc_data(tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
		}
		error = 0;
		goto nfsmout;
	}
	fvp = fromnd.ni_vp;
	nfsm_srvmtofh(tfhp);
	nfsm_strsiz(len2, NFS_MAXNAMLEN);
	cred->cr_uid = saved_uid;
	tond.ni_cnd.cn_cred = cred;
	tond.ni_cnd.cn_nameiop = RENAME;
	tond.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART;
	error = nfs_namei(&tond, tfhp, len2, slp, nam, &md,
		&dpos, &tdirp, td, FALSE);
	if (tdirp) {
		if (v3) {
			tdirfor_ret = VOP_GETATTR(tdirp, &tdirfor, cred,
				td);
		} else {
			vrele(tdirp);
			tdirp = NULL;
		}
	}
	if (error)
		goto out1;

	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			if (v3)
				error = EEXIST;
			else
				error = EISDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			if (v3)
				error = EEXIST;
			else
				error = ENOTDIR;
			goto out;
		}
		if (tvp->v_type == VDIR && tvp->v_mountedhere) {
			if (v3)
				error = EXDEV;
			else
				error = ENOTEMPTY;
			goto out;
		}
	}
	if (fvp->v_type == VDIR && fvp->v_mountedhere) {
		if (v3)
			error = EXDEV;
		else
			error = ENOTEMPTY;
		goto out;
	}
	if (fvp->v_mount != tdvp->v_mount) {
		if (v3)
			error = EXDEV;
		else
			error = ENOTEMPTY;
		goto out;
	}
	if (fvp == tdvp) {
		if (v3)
			error = EINVAL;
		else
			error = ENOTEMPTY;
	}
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
		/*
		 * The VOP_RENAME function releases all vnode references &
		 * locks prior to returning so we need to clear the pointers
		 * to bypass cleanup code later on.
		 */
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
		fromnd.ni_dvp = NULL;
		fromnd.ni_vp = NULL;
		tond.ni_dvp = NULL;
		tond.ni_vp = NULL;
		if (error) {
			fromnd.ni_cnd.cn_flags &= ~HASBUF;
			tond.ni_cnd.cn_flags &= ~HASBUF;
		}
	} else {
		if (error == -1)
			error = 0;
	}
	/* fall through */

out1:
	if (fdirp)
		fdiraft_ret = VOP_GETATTR(fdirp, &fdiraft, cred, td);
	if (tdirp)
		tdiraft_ret = VOP_GETATTR(tdirp, &tdiraft, cred, td);
	nfsm_reply(2 * NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
		nfsm_srvwcc_data(tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
	}
	error = 0;
	/* fall through */

nfsmout:
	/*
	 * Clear out tond related fields
	 */
	if (tdirp)
		vrele(tdirp);
	if (tond.ni_startdir)
		vrele(tond.ni_startdir);
	NDFREE(&tond, NDF_ONLY_PNBUF);
	if (tond.ni_dvp) {
		if (tond.ni_dvp == tond.ni_vp)
			vrele(tond.ni_dvp);
		else
			vput(tond.ni_dvp);
	}
	if (tond.ni_vp)
		vput(tond.ni_vp);

	/*
	 * Clear out fromnd related fields
	 */
	if (fdirp)
		vrele(fdirp);
	if (fromnd.ni_startdir)
		vrele(fromnd.ni_startdir);
	NDFREE(&fromnd, NDF_ONLY_PNBUF);
	if (fromnd.ni_dvp)
		vrele(fromnd.ni_dvp);
	if (fromnd.ni_vp)
		vrele(fromnd.ni_vp);

	vn_finished_write(mp);
	return (error);
}

/*
 * nfs link service
 */
int
nfsrv_link(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nameidata nd;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly, len, dirfor_ret = 1, diraft_ret = 1;
	int getret = 1, v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct vnode *vp = NULL, *xp, *dirp = (struct vnode *)0;
	struct vattr dirfor, diraft, at;
	nfsfh_t nfh, dnfh;
	fhandle_t *fhp, *dfhp;
	struct mount *mp = NULL;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	dfhp = &dnfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto ereply;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto ereply;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	nfsm_srvmtofh(dfhp);
	nfsm_srvnamesiz(len);

	error = nfsrv_fhtovp(fhp, FALSE, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
		if (v3) {
			nfsm_srvpostop_attr(getret, &at);
			nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		}
		vp = NULL;
		error = 0;
		goto nfsmout;
	}
	if (vp->v_type == VDIR) {
		error = EPERM;		/* POSIX */
		goto out1;
	}
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	error = nfs_namei(&nd, dfhp, len, slp, nam, &md, &dpos,
		&dirp, td, FALSE);
	if (dirp) {
		if (v3) {
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				td);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error)
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
		error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
		NDFREE(&nd, NDF_ONLY_PNBUF);
	}
	/* fall through */

out1:
	if (v3)
		getret = VOP_GETATTR(vp, &at, cred, td);
	if (dirp)
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, td);
ereply:
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvpostop_attr(getret, &at);
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	}
	/* fall through */

nfsmout:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (dirp)
		vrele(dirp);
	if (vp)
		vrele(vp);
	if (nd.ni_dvp) {
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
	}
	if (nd.ni_vp)
		vrele(nd.ni_vp);
	vn_finished_write(mp);
	return(error);
}

/*
 * nfs symbolic link service
 */
int
nfsrv_symlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct nameidata nd;
	struct vattr *vap = &va;
	u_int32_t *tl;
	struct nfsv2_sattr *sp;
	char *bpos, *pathcp = (char *)0;
	struct uio io;
	struct iovec iv;
	int error = 0, len, len2, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct vnode *dirp = (struct vnode *)0;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct mount *mp = NULL;
	struct vnode *vp;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto out;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto out;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | SAVESTART;
	error = nfs_namei(&nd, fhp, len, slp, nam, &md, &dpos,
		&dirp, td, FALSE);
	if (dirp) {
		if (v3) {
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				td);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error)
		goto out;

	VATTR_NULL(vap);
	if (v3)
		nfsm_srvsattr(vap);
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
	io.uio_td = (struct thread *)0;
	nfsm_mtouio(&io, len2);
	if (!v3) {
		sp = nfsm_dissect(struct nfsv2_sattr *, NFSX_V2SATTR);
		vap->va_mode = fxdr_unsigned(u_int16_t, sp->sa_mode);
	}
	*(pathcp + len2) = '\0';
	if (nd.ni_vp) {
		error = EEXIST;
		goto out;
	}

	/*
	 * issue symlink op.  SAVESTART is set so the underlying path component
	 * is only freed by the VOP if an error occurs.
	 */
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap, pathcp);
	if (error)
		NDFREE(&nd, NDF_ONLY_PNBUF);
	else {
		vput(nd.ni_vp);
		nd.ni_vp = NULL;
	}
	/*
	 * releases directory prior to potential lookup op.
	 */
	vput(nd.ni_dvp);
	nd.ni_dvp = NULL;

	if (error == 0) {
	    if (v3) {
		/*
		 * Issue lookup.  Leave SAVESTART set so we can easily free
		 * the name buffer later on.
		 *
		 * since LOCKPARENT is not set, ni_dvp will be garbage on
		 * return whether an error occurs or not.
		 */
		nd.ni_cnd.cn_nameiop = LOOKUP;
		nd.ni_cnd.cn_flags &= ~(LOCKPARENT | FOLLOW);
		nd.ni_cnd.cn_flags |= (NOFOLLOW | LOCKLEAF);
		nd.ni_cnd.cn_thread = td;
		nd.ni_cnd.cn_cred = cred;

		error = lookup(&nd);
		nd.ni_dvp = NULL;

		if (error == 0) {
			bzero((caddr_t)fhp, sizeof(nfh));
			fhp->fh_fsid = nd.ni_vp->v_mount->mnt_stat.f_fsid;
			error = VFS_VPTOFH(nd.ni_vp, &fhp->fh_fid);
			if (!error)
				error = VOP_GETATTR(nd.ni_vp, vap, cred,
					td);
			vput(nd.ni_vp);
			nd.ni_vp = NULL;
		}
	    }
	}
out:
	/*
	 * These releases aren't strictly required, does even doing them
	 * make any sense? XXX can nfsm_reply() block?
	 */
	if (pathcp) {
		FREE(pathcp, M_TEMP);
		pathcp = NULL;
	}
	if (dirp) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, td);
		vrele(dirp);
		dirp = NULL;
	}
	if (nd.ni_startdir) {
		vrele(nd.ni_startdir);
		nd.ni_startdir = NULL;
	}
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(0, vap);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	}
	error = 0;
	/* fall through */

nfsmout:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_dvp) {
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
	}
	if (nd.ni_vp)
		vrele(nd.ni_vp);
	if (nd.ni_startdir)
		vrele(nd.ni_startdir);
	if (dirp)
		vrele(dirp);
	if (pathcp)
		FREE(pathcp, M_TEMP);

	vn_finished_write(mp);
	return (error);
}

/*
 * nfs mkdir service
 */
int
nfsrv_mkdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct vattr *vap = &va;
	struct nfs_fattr *fp;
	struct nameidata nd;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct vnode *dirp = NULL;
	int vpexcl = 0;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct mount *mp = NULL;
	struct vnode *vp;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto out;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto out;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;

	error = nfs_namei(&nd, fhp, len, slp, nam, &md, &dpos,
		&dirp, td, FALSE);
	if (dirp) {
		if (v3) {
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				td);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(v3));
		if (v3)
			nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}
	VATTR_NULL(vap);
	if (v3) {
		nfsm_srvsattr(vap);
	} else {
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		vap->va_mode = nfstov_mode(*tl++);
	}

	/*
	 * At this point nd.ni_dvp is referenced and exclusively locked and
	 * nd.ni_vp, if it exists, is referenced but not locked.
	 */

	vap->va_type = VDIR;
	if (nd.ni_vp != NULL) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		error = EEXIST;
		goto out;
	}

	/*
	 * Issue mkdir op.  Since SAVESTART is not set, the pathname
	 * component is freed by the VOP call.  This will fill-in
	 * nd.ni_vp, reference, and exclusively lock it.
	 */
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, vap);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vpexcl = 1;

	vput(nd.ni_dvp);
	nd.ni_dvp = NULL;

	if (!error) {
		bzero((caddr_t)fhp, sizeof(nfh));
		fhp->fh_fsid = nd.ni_vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(nd.ni_vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(nd.ni_vp, vap, cred, td);
	}
out:
	if (dirp)
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, td);
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(0, vap);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	} else {
		/* v2, non-error case (see nfsm_reply) */
		nfsm_srvfhtom(fhp, v3);
		fp = nfsm_build(struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
	error = 0;
	/* fall through */

nfsmout:
	if (dirp)
		vrele(dirp);
	if (nd.ni_dvp) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_dvp == nd.ni_vp && vpexcl)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
	}
	if (nd.ni_vp) {
		if (vpexcl)
			vput(nd.ni_vp);
		else
			vrele(nd.ni_vp);
	}
	vn_finished_write(mp);
	return (error);
}

/*
 * nfs rmdir service
 */
int
nfsrv_rmdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct vnode *vp, *dirp = (struct vnode *)0;
	struct vattr dirfor, diraft;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct nameidata nd;
	struct mount *mp = NULL;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	ndclear(&nd);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto out;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto out;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, fhp, len, slp, nam, &md, &dpos,
		&dirp, td, FALSE);
	if (dirp) {
		if (v3) {
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				td);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(v3));
		if (v3)
			nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}
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
	/*
	 * Issue or abort op.  Since SAVESTART is not set, path name
	 * component is freed by the VOP after either.
	 */
	if (!error)
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	if (dirp)
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, td);
	nfsm_reply(NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	}
	/* fall through */

nfsmout:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (dirp)
		vrele(dirp);
	if (nd.ni_dvp) {
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
	}
	if (nd.ni_vp)
		vput(nd.ni_vp);

	vn_finished_write(mp);
	return(error);
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
 *     The alternate call nfsrv_readdirplus() does lookups as well.
 * PS: The NFS protocol spec. does not clarify what the "count" byte
 *	argument is a count of.. just name strings and file id's or the
 *	entire reply rpc or ...
 *	I tried just file name and id sizes and it confused the Sun client,
 *	so I am using the full rpc size now. The "paranoia.." comment refers
 *	to including the status longwords that are not a part of the dir.
 *	"entry" structures, but are in the rpc.
 */
struct flrep {
	nfsuint64	fl_off;
	u_int32_t	fl_postopok;
	u_int32_t	fl_fattr[NFSX_V3FATTR / sizeof (u_int32_t)];
	u_int32_t	fl_fhok;
	u_int32_t	fl_fhsize;
	u_int32_t	fl_nfh[NFSX_V3FH / sizeof (u_int32_t)];
};

int
nfsrv_readdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	char *bp, *be;
	struct mbuf *mp;
	struct dirent *dp;
	caddr_t cp;
	u_int32_t *tl;
	caddr_t bpos;
	struct mbuf *mb, *mreq;
	char *cpos, *cend, *rbuf;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io;
	struct iovec iv;
	int len, nlen, rem, xfer, tsiz, i, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly, ncookies;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	u_quad_t off, toff, verf;
	u_long *cookies = NULL, *cookiep; /* needs to be int64_t or off_t */

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (v3) {
		tl = nfsm_dissect(u_int32_t *, 5 * NFSX_UNSIGNED);
		toff = fxdr_hyper(tl);
		tl += 2;
		verf = fxdr_hyper(tl);
		tl += 2;
	} else {
		tl = nfsm_dissect(u_int32_t *, 2 * NFSX_UNSIGNED);
		toff = fxdr_unsigned(u_quad_t, *tl++);
		verf = 0;	/* shut up gcc */
	}
	off = toff;
	cnt = fxdr_unsigned(int, *tl);
	siz = ((cnt + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (siz > xfer)
		siz = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (!error && vp->v_type != VDIR) {
		error = ENOTDIR;
		vput(vp);
		vp = NULL;
	}
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		if (v3)
			nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}

	/*
	 * Obtain lock on vnode for this section of the code
	 */
	if (v3) {
		error = getret = VOP_GETATTR(vp, &at, cred, td);
#if 0
		/*
		 * XXX This check may be too strict for Solaris 2.5 clients.
		 */
		if (!error && toff && verf && verf != at.va_filerev)
			error = NFSERR_BAD_COOKIE;
#endif
	}
	if (!error)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, td, 0);
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_POSTOPATTR(v3));
		if (v3)
			nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	VOP_UNLOCK(vp, 0, td);

	/*
	 * end section.  Allocate rbuf and continue
	 */
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
	io.uio_td = (struct thread *)0;
	eofflag = 0;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (cookies) {
		free((caddr_t)cookies, M_TEMP);
		cookies = NULL;
	}
	error = VOP_READDIR(vp, &io, cred, &eofflag, &ncookies, &cookies);
	off = (off_t)io.uio_offset;
	if (!cookies && !error)
		error = NFSERR_PERM;
	if (v3) {
		getret = VOP_GETATTR(vp, &at, cred, td);
		if (!error)
			error = getret;
	}
	VOP_UNLOCK(vp, 0, td);
	if (error) {
		vrele(vp);
		vp = NULL;
		free((caddr_t)rbuf, M_TEMP);
		if (cookies)
			free((caddr_t)cookies, M_TEMP);
		nfsm_reply(NFSX_POSTOPATTR(v3));
		if (v3)
			nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			vp = NULL;
			nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_COOKIEVERF(v3) +
				2 * NFSX_UNSIGNED);
			if (v3) {
				nfsm_srvpostop_attr(getret, &at);
				tl = nfsm_build(u_int32_t *, 4 * NFSX_UNSIGNED);
				txdr_hyper(at.va_filerev, tl);
				tl += 2;
			} else
				tl = nfsm_build(u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = nfs_false;
			*tl = nfs_true;
			FREE((caddr_t)rbuf, M_TEMP);
			FREE((caddr_t)cookies, M_TEMP);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;
	/*
	 * For some reason FreeBSD's ufs_readdir() chooses to back the
	 * directory offset up to a block boundary, so it is necessary to
	 * skip over the records that precede the requested offset. This
	 * requires the assumption that file offset cookies monotonically
	 * increase.
	 */
	while (cpos < cend && ncookies > 0 &&
		(dp->d_fileno == 0 || dp->d_type == DT_WHT ||
		 ((u_quad_t)(*cookiep)) <= toff)) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		toff = off;
		siz = fullsiz;
		goto again;
	}

	len = 3 * NFSX_UNSIGNED;	/* paranoia, probably can be 0 */
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_COOKIEVERF(v3) + siz);
	if (v3) {
		nfsm_srvpostop_attr(getret, &at);
		tl = nfsm_build(u_int32_t *, 2 * NFSX_UNSIGNED);
		txdr_hyper(at.va_filerev, tl);
	}
	mp = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend && ncookies > 0) {
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen) - nlen;
			len += (4 * NFSX_UNSIGNED + nlen + rem);
			if (v3)
				len += 2 * NFSX_UNSIGNED;
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
			if (v3) {
				nfsm_clget;
				*tl = 0;
				bp += NFSX_UNSIGNED;
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
			/* And null pad to a int32_t boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';
			nfsm_clget;

			/* Finish off the record */
			if (v3) {
				*tl = 0;
				bp += NFSX_UNSIGNED;
				nfsm_clget;
			}
			*tl = txdr_unsigned(*cookiep);
			bp += NFSX_UNSIGNED;
		}
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	vrele(vp);
	vp = NULL;
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
	FREE((caddr_t)rbuf, M_TEMP);
	FREE((caddr_t)cookies, M_TEMP);

nfsmout:
	if (vp)
		vrele(vp);
	return(error);
}

int
nfsrv_readdirplus(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	char *bp, *be;
	struct mbuf *mp;
	struct dirent *dp;
	caddr_t cp;
	u_int32_t *tl;
	caddr_t bpos;
	struct mbuf *mb, *mreq;
	char *cpos, *cend, *rbuf;
	struct vnode *vp = NULL, *nvp;
	struct flrep fl;
	nfsfh_t nfh;
	fhandle_t *fhp, *nfhp = (fhandle_t *)fl.fl_nfh;
	struct uio io;
	struct iovec iv;
	struct vattr va, at, *vap = &va;
	struct nfs_fattr *fp;
	int len, nlen, rem, xfer, tsiz, i, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly, dirlen, ncookies;
	u_quad_t off, toff, verf;
	u_long *cookies = NULL, *cookiep; /* needs to be int64_t or off_t */
	int v3 = (nfsd->nd_flag & ND_NFSV3);

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (!v3)
		panic("nfsrv_readdirplus: v3 proc called on a v2 connection");
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	tl = nfsm_dissect(u_int32_t *, 6 * NFSX_UNSIGNED);
	toff = fxdr_hyper(tl);
	tl += 2;
	verf = fxdr_hyper(tl);
	tl += 2;
	siz = fxdr_unsigned(int, *tl++);
	cnt = fxdr_unsigned(int, *tl);
	off = toff;
	siz = ((siz + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (siz > xfer)
		siz = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (!error && vp->v_type != VDIR) {
		error = ENOTDIR;
		vput(vp);
		vp = NULL;
	}
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	error = getret = VOP_GETATTR(vp, &at, cred, td);
#if 0
	/*
	 * XXX This check may be too strict for Solaris 2.5 clients.
	 */
	if (!error && toff && verf && verf != at.va_filerev)
		error = NFSERR_BAD_COOKIE;
#endif
	if (!error)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, td, 0);
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	VOP_UNLOCK(vp, 0, td);
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
	io.uio_td = (struct thread *)0;
	eofflag = 0;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (cookies) {
		free((caddr_t)cookies, M_TEMP);
		cookies = NULL;
	}
	error = VOP_READDIR(vp, &io, cred, &eofflag, &ncookies, &cookies);
	off = (u_quad_t)io.uio_offset;
	getret = VOP_GETATTR(vp, &at, cred, td);
	VOP_UNLOCK(vp, 0, td);
	if (!cookies && !error)
		error = NFSERR_PERM;
	if (!error)
		error = getret;
	if (error) {
		vrele(vp);
		vp = NULL;
		if (cookies)
			free((caddr_t)cookies, M_TEMP);
		free((caddr_t)rbuf, M_TEMP);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			vp = NULL;
			nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF +
				2 * NFSX_UNSIGNED);
			nfsm_srvpostop_attr(getret, &at);
			tl = nfsm_build(u_int32_t *, 4 * NFSX_UNSIGNED);
			txdr_hyper(at.va_filerev, tl);
			tl += 2;
			*tl++ = nfs_false;
			*tl = nfs_true;
			FREE((caddr_t)cookies, M_TEMP);
			FREE((caddr_t)rbuf, M_TEMP);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;
	/*
	 * For some reason FreeBSD's ufs_readdir() chooses to back the
	 * directory offset up to a block boundary, so it is necessary to
	 * skip over the records that precede the requested offset. This
	 * requires the assumption that file offset cookies monotonically
	 * increase.
	 */
	while (cpos < cend && ncookies > 0 &&
		(dp->d_fileno == 0 || dp->d_type == DT_WHT ||
		 ((u_quad_t)(*cookiep)) <= toff)) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		toff = off;
		siz = fullsiz;
		goto again;
	}

	/*
	 * Probe one of the directory entries to see if the filesystem
	 * supports VGET.
	 */
	if (VFS_VGET(vp->v_mount, dp->d_fileno, &nvp) == EOPNOTSUPP) {
		error = NFSERR_NOTSUPP;
		vrele(vp);
		vp = NULL;
		free((caddr_t)cookies, M_TEMP);
		free((caddr_t)rbuf, M_TEMP);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	vput(nvp);
	nvp = NULL;

	dirlen = len = NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF +
	    2 * NFSX_UNSIGNED;
	nfsm_reply(cnt);
	nfsm_srvpostop_attr(getret, &at);
	tl = nfsm_build(u_int32_t *, 2 * NFSX_UNSIGNED);
	txdr_hyper(at.va_filerev, tl);
	mp = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend && ncookies > 0) {
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen)-nlen;

			/*
			 * For readdir_and_lookup get the vnode using
			 * the file number.
			 */
			if (VFS_VGET(vp->v_mount, dp->d_fileno, &nvp))
				goto invalid;
			bzero((caddr_t)nfhp, NFSX_V3FH);
			nfhp->fh_fsid =
				nvp->v_mount->mnt_stat.f_fsid;
			if (VFS_VPTOFH(nvp, &nfhp->fh_fid)) {
				vput(nvp);
				nvp = NULL;
				goto invalid;
			}
			if (VOP_GETATTR(nvp, vap, cred, td)) {
				vput(nvp);
				nvp = NULL;
				goto invalid;
			}
			vput(nvp);
			nvp = NULL;

			/*
			 * If either the dircount or maxcount will be
			 * exceeded, get out now. Both of these lengths
			 * are calculated conservatively, including all
			 * XDR overheads.
			 */
			len += (8 * NFSX_UNSIGNED + nlen + rem + NFSX_V3FH +
				NFSX_V3POSTOPATTR);
			dirlen += (6 * NFSX_UNSIGNED + nlen + rem);
			if (len > cnt || dirlen > fullsiz) {
				eofflag = 0;
				break;
			}

			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			fp = (struct nfs_fattr *)&fl.fl_fattr;
			nfsm_srvfillattr(vap, fp);
			fl.fl_fhsize = txdr_unsigned(NFSX_V3FH);
			fl.fl_fhok = nfs_true;
			fl.fl_postopok = nfs_true;
			fl.fl_off.nfsuquad[0] = 0;
			fl.fl_off.nfsuquad[1] = txdr_unsigned(*cookiep);

			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = 0;
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
				if ((bp + xfer) > be)
					tsiz = be - bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			/* And null pad to a int32_t boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';

			/*
			 * Now copy the flrep structure out.
			 */
			xfer = sizeof (struct flrep);
			cp = (caddr_t)&fl;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp + xfer) > be)
					tsiz = be - bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
		}
invalid:
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	vrele(vp);
	vp = NULL;
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
	FREE((caddr_t)cookies, M_TEMP);
	FREE((caddr_t)rbuf, M_TEMP);
nfsmout:
	if (vp)
		vrele(vp);
	return(error);
}

/*
 * nfs commit service
 */
int
nfsrv_commit(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr bfor, aft;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly, for_ret = 1, aft_ret = 1, cnt;
	struct mbuf *mb, *mreq;
	u_quad_t off;
	struct mount *mp = NULL;
	int v3 = (nfsd->nd_flag & ND_NFSV3);

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (!v3)
		panic("nfsrv_commit: v3 proc called on a v2 connection");
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if ((mp = vfs_getvfs(&fhp->fh_fsid)) == NULL) {
		error = ESTALE;
		goto ereply;
	}
	if ((error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp)) != 0) {
		mp = NULL;
		goto ereply;
	}
	(void) vn_start_write(vp, &mp, V_WAIT);
	vput(vp);
	vp = NULL;
	tl = nfsm_dissect(u_int32_t *, 3 * NFSX_UNSIGNED);

	/*
	 * XXX At this time VOP_FSYNC() does not accept offset and byte
	 * count parameters, so these arguments are useless (someday maybe).
	 */
	off = fxdr_hyper(tl);
	tl += 2;
	cnt = fxdr_unsigned(int, *tl);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(for_ret, &bfor, aft_ret, &aft);
		error = 0;
		goto nfsmout;
	}
	for_ret = VOP_GETATTR(vp, &bfor, cred, td);

	if (cnt > MAX_COMMIT_COUNT) {
		/*
		 * Give up and do the whole thing
		 */
		if (vp->v_object &&
		   (vp->v_object->flags & OBJ_MIGHTBEDIRTY)) {
			vm_object_page_clean(vp->v_object, 0, 0, OBJPC_SYNC);
		}
		error = VOP_FSYNC(vp, cred, MNT_WAIT, td);
	} else {
		/*
		 * Locate and synchronously write any buffers that fall
		 * into the requested range.  Note:  we are assuming that
		 * f_iosize is a power of 2.
		 */
		int iosize = vp->v_mount->mnt_stat.f_iosize;
		int iomask = iosize - 1;
		int s;
		daddr_t lblkno;

		/*
		 * Align to iosize boundry, super-align to page boundry.
		 */
		if (off & iomask) {
			cnt += off & iomask;
			off &= ~(u_quad_t)iomask;
		}
		if (off & PAGE_MASK) {
			cnt += off & PAGE_MASK;
			off &= ~(u_quad_t)PAGE_MASK;
		}
		lblkno = off / iosize;

		if (vp->v_object &&
		   (vp->v_object->flags & OBJ_MIGHTBEDIRTY)) {
			vm_object_page_clean(vp->v_object, off / PAGE_SIZE, (cnt + PAGE_MASK) / PAGE_SIZE, OBJPC_SYNC);
		}

		s = splbio();
		while (cnt > 0) {
			struct buf *bp;

			/*
			 * If we have a buffer and it is marked B_DELWRI we
			 * have to lock and write it.  Otherwise the prior
			 * write is assumed to have already been committed.
			 */
			if ((bp = gbincore(vp, lblkno)) != NULL && (bp->b_flags & B_DELWRI)) {
				if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT)) {
					BUF_LOCK(bp, LK_EXCLUSIVE | LK_SLEEPFAIL);
					continue; /* retry */
				}
				bremfree(bp);
				bp->b_flags &= ~B_ASYNC;
				BUF_WRITE(bp);
				++nfs_commit_miss;
			}
			++nfs_commit_blks;
			if (cnt < iosize)
				break;
			cnt -= iosize;
			++lblkno;
		}
		splx(s);
	}

	aft_ret = VOP_GETATTR(vp, &aft, cred, td);
	vput(vp);
	vp = NULL;
ereply:
	nfsm_reply(NFSX_V3WCCDATA + NFSX_V3WRITEVERF);
	nfsm_srvwcc_data(for_ret, &bfor, aft_ret, &aft);
	if (!error) {
		tl = nfsm_build(u_int32_t *, NFSX_V3WRITEVERF);
		if (nfsver.tv_sec == 0)
			nfsver = boottime;
		*tl++ = txdr_unsigned(nfsver.tv_sec);
		*tl = txdr_unsigned(nfsver.tv_usec);
	} else {
		error = 0;
	}
nfsmout:
	if (vp)
		vput(vp);
	vn_finished_write(mp);
	return(error);
}

/*
 * nfs statfs service
 */
int
nfsrv_statfs(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct statfs *sf;
	struct nfs_statfs *sfp;
	u_int32_t *tl;
	caddr_t bpos;
	int error = 0, rdonly, getret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct statfs statfs;
	u_quad_t tval;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		if (v3)
			nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	sf = &statfs;
	error = VFS_STATFS(vp->v_mount, sf, td);
	getret = VOP_GETATTR(vp, &at, cred, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_STATFS(v3));
	if (v3)
		nfsm_srvpostop_attr(getret, &at);
	if (error) {
		error = 0;
		goto nfsmout;
	}
	sfp = nfsm_build(struct nfs_statfs *, NFSX_STATFS(v3));
	if (v3) {
		tval = (u_quad_t)sf->f_blocks;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_tbytes);
		tval = (u_quad_t)sf->f_bfree;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_fbytes);
		tval = (u_quad_t)sf->f_bavail;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_abytes);
		sfp->sf_tfiles.nfsuquad[0] = 0;
		sfp->sf_tfiles.nfsuquad[1] = txdr_unsigned(sf->f_files);
		sfp->sf_ffiles.nfsuquad[0] = 0;
		sfp->sf_ffiles.nfsuquad[1] = txdr_unsigned(sf->f_ffree);
		sfp->sf_afiles.nfsuquad[0] = 0;
		sfp->sf_afiles.nfsuquad[1] = txdr_unsigned(sf->f_ffree);
		sfp->sf_invarsec = 0;
	} else {
		sfp->sf_tsize = txdr_unsigned(NFS_MAXDGRAMDATA);
		sfp->sf_bsize = txdr_unsigned(sf->f_bsize);
		sfp->sf_blocks = txdr_unsigned(sf->f_blocks);
		sfp->sf_bfree = txdr_unsigned(sf->f_bfree);
		sfp->sf_bavail = txdr_unsigned(sf->f_bavail);
	}
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs fsinfo service
 */
int
nfsrv_fsinfo(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	struct nfsv3_fsinfo *sip;
	caddr_t bpos;
	int error = 0, rdonly, getret = 1, pref;
	struct mbuf *mb, *mreq;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t maxfsize;
	struct statfs sb;
	int v3 = (nfsd->nd_flag & ND_NFSV3);

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (!v3)
		panic("nfsrv_fsinfo: v3 proc called on a v2 connection");
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}

	/* XXX Try to make a guess on the max file size. */
	VFS_STATFS(vp->v_mount, &sb, td);
	maxfsize = (u_quad_t)0x80000000 * sb.f_bsize - 1;

	getret = VOP_GETATTR(vp, &at, cred, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3FSINFO);
	nfsm_srvpostop_attr(getret, &at);
	sip = nfsm_build(struct nfsv3_fsinfo *, NFSX_V3FSINFO);

	/*
	 * XXX
	 * There should be file system VFS OP(s) to get this information.
	 * For now, assume ufs.
	 */
	if (slp->ns_so->so_type == SOCK_DGRAM)
		pref = NFS_MAXDGRAMDATA;
	else
		pref = NFS_MAXDATA;
	sip->fs_rtmax = txdr_unsigned(NFS_MAXDATA);
	sip->fs_rtpref = txdr_unsigned(pref);
	sip->fs_rtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_wtmax = txdr_unsigned(NFS_MAXDATA);
	sip->fs_wtpref = txdr_unsigned(pref);
	sip->fs_wtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_dtpref = txdr_unsigned(pref);
	txdr_hyper(maxfsize, &sip->fs_maxfilesize);
	sip->fs_timedelta.nfsv3_sec = 0;
	sip->fs_timedelta.nfsv3_nsec = txdr_unsigned(1);
	sip->fs_properties = txdr_unsigned(NFSV3FSINFO_LINK |
		NFSV3FSINFO_SYMLINK | NFSV3FSINFO_HOMOGENEOUS |
		NFSV3FSINFO_CANSETTIME);
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs pathconf service
 */
int
nfsrv_pathconf(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	struct nfsv3_pathconf *pc;
	caddr_t bpos;
	int error = 0, rdonly, getret = 1;
	register_t linkmax, namemax, chownres, notrunc;
	struct mbuf *mb, *mreq;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	int v3 = (nfsd->nd_flag & ND_NFSV3);

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (!v3)
		panic("nfsrv_pathconf: v3 proc called on a v2 connection");
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly, TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	error = VOP_PATHCONF(vp, _PC_LINK_MAX, &linkmax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NAME_MAX, &namemax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_CHOWN_RESTRICTED, &chownres);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NO_TRUNC, &notrunc);
	getret = VOP_GETATTR(vp, &at, cred, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3PATHCONF);
	nfsm_srvpostop_attr(getret, &at);
	if (error) {
		error = 0;
		goto nfsmout;
	}
	pc = nfsm_build(struct nfsv3_pathconf *, NFSX_V3PATHCONF);

	pc->pc_linkmax = txdr_unsigned(linkmax);
	pc->pc_namemax = txdr_unsigned(namemax);
	pc->pc_notrunc = txdr_unsigned(notrunc);
	pc->pc_chownrestricted = txdr_unsigned(chownres);

	/*
	 * These should probably be supported by VOP_PATHCONF(), but
	 * until msdosfs is exportable (why would you want to?), the
	 * Unix defaults should be ok.
	 */
	pc->pc_caseinsensitive = nfs_false;
	pc->pc_casepreserving = nfs_true;
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * Null operation, used by clients to ping server
 */
/* ARGSUSED */
int
nfsrv_null(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep;
	caddr_t bpos;
	int error = NFSERR_RETVOID;
	struct mbuf *mb, *mreq;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nfsm_reply(0);
nfsmout:
	return (error);
}

/*
 * No operation, used for obsolete procedures
 */
/* ARGSUSED */
int
nfsrv_noop(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep;
	caddr_t bpos;
	int error;
	struct mbuf *mb, *mreq;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (nfsd->nd_repstat)
		error = nfsd->nd_repstat;
	else
		error = EPROCUNAVAIL;
	nfsm_reply(0);
	error = 0;
nfsmout:
	return (error);
}

/*
 * Perform access checking for vnodes obtained from file handles that would
 * refer to files already opened by a Unix client. You cannot just use
 * vn_writechk() and VOP_ACCESS() for two reasons.
 * 1 - You must check for exported rdonly as well as MNT_RDONLY for the write case
 * 2 - The owner is to be given access irrespective of mode bits for some
 *     operations, so that processes that chmod after opening a file don't
 *     break. I don't like this because it opens a security hole, but since
 *     the nfs server opens a security hole the size of a barn door anyhow,
 *     what the heck.
 *
 * The exception to rule 2 is EPERM. If a file is IMMUTABLE, VOP_ACCESS()
 * will return EPERM instead of EACCESS. EPERM is always an error.
 */
static int
nfsrv_access(struct vnode *vp, int flags, struct ucred *cred, int rdonly,
    struct thread *td, int override)
{
	struct vattr vattr;
	int error;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (flags & VWRITE) {
		/* Just vn_writechk() changed to check rdonly */
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket or a block or character
		 * device resident on the file system.
		 */
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			switch (vp->v_type) {
			case VREG:
			case VDIR:
			case VLNK:
				return (EROFS);
			default:
				break;
			}
		}
		/*
		 * If there's shared text associated with
		 * the inode, we can't allow writing.
		 */
		if (vp->v_flag & VTEXT)
			return (ETXTBSY);
	}
	error = VOP_GETATTR(vp, &vattr, cred, td);
	if (error)
		return (error);
	error = VOP_ACCESS(vp, flags, cred, td);
	/*
	 * Allow certain operations for the owner (reads and writes
	 * on files that are already open).
	 */
	if (override && error == EACCES && cred->cr_uid == vattr.va_uid)
		error = 0;
	return error;
}
