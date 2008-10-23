/* $Id: nfs_vnops.c,v 1.45 2003/11/05 14:59:02 rees Exp $ */

/*-
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

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
 *	@(#)nfs_vnops.c	8.16 (Berkeley) 5/27/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * vnode op calls for Sun NFS version 2 and 3
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/namei.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/lockmgr.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <fs/fifofs/fifo.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfs4client/nfs4.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfs_lock.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

/* NFSv4 */
#include <nfs4client/nfs4m_subs.h>
#include <nfs4client/nfs4_vn.h>

/* Defs */
#define	TRUE	1
#define	FALSE	0

/*
 * Ifdef for FreeBSD-current merged buffer cache. It is unfortunate that these
 * calls are not in getblk() and brelse() so that they would not be necessary
 * here.
 */
#ifndef B_VMIO
#define vfs_busy_pages(bp, f)
#endif

static int	nfs4_flush(struct vnode *, int, struct thread *,
		    int);
static int	nfs4_setattrrpc(struct vnode *, struct vattr *, struct ucred *);
static int      nfs4_closerpc(struct vnode *, struct ucred *, int);

static vop_lookup_t	nfs4_lookup;
static vop_create_t	nfs4_create;
static vop_mknod_t	nfs4_mknod;
static vop_open_t	nfs4_open;
static vop_close_t	nfs4_close;
static vop_access_t	nfs4_access;
static vop_getattr_t	nfs4_getattr;
static vop_setattr_t	nfs4_setattr;
static vop_read_t	nfs4_read;
static vop_fsync_t	nfs4_fsync;
static vop_remove_t	nfs4_remove;
static vop_link_t	nfs4_link;
static vop_rename_t	nfs4_rename;
static vop_mkdir_t	nfs4_mkdir;
static vop_rmdir_t	nfs4_rmdir;
static vop_symlink_t	nfs4_symlink;
static vop_readdir_t	nfs4_readdir;
static vop_strategy_t	nfs4_strategy;
static	int	nfs4_lookitup(struct vnode *, const char *, int,
		    struct ucred *, struct thread *, struct nfsnode **);
static	int	nfs4_sillyrename(struct vnode *, struct vnode *,
		    struct componentname *);
static vop_readlink_t	nfs4_readlink;
static vop_print_t	nfs4_print;
static vop_advlock_t	nfs4_advlock;
static vop_advlockasync_t nfs4_advlockasync;

/*
 * Global vfs data structures for nfs
 */
struct vop_vector nfs4_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_access =		nfs4_access,
	.vop_advlock =		nfs4_advlock,
	.vop_advlockasync =	nfs4_advlockasync,
	.vop_close =		nfs4_close,
	.vop_create =		nfs4_create,
	.vop_fsync =		nfs4_fsync,
	.vop_getattr =		nfs4_getattr,
	.vop_getpages =		nfs_getpages,
	.vop_putpages =		nfs_putpages,
	.vop_inactive =		nfs_inactive,
	.vop_lease =		VOP_NULL,
	.vop_link =		nfs4_link,
	.vop_lookup =		nfs4_lookup,
	.vop_mkdir =		nfs4_mkdir,
	.vop_mknod =		nfs4_mknod,
	.vop_open =		nfs4_open,
	.vop_print =		nfs4_print,
	.vop_read =		nfs4_read,
	.vop_readdir =		nfs4_readdir,
	.vop_readlink =		nfs4_readlink,
	.vop_reclaim =		nfs_reclaim,
	.vop_remove =		nfs4_remove,
	.vop_rename =		nfs4_rename,
	.vop_rmdir =		nfs4_rmdir,
	.vop_setattr =		nfs4_setattr,
	.vop_strategy =		nfs4_strategy,
	.vop_symlink =		nfs4_symlink,
	.vop_write =		nfs_write,
};

static int	nfs4_removerpc(struct vnode *dvp, const char *name, int namelen,
			      struct ucred *cred, struct thread *td);
static int	nfs4_renamerpc(struct vnode *fdvp, const char *fnameptr,
			      int fnamelen, struct vnode *tdvp,
			      const char *tnameptr, int tnamelen,
			      struct ucred *cred, struct thread *td);
static int	nfs4_renameit(struct vnode *sdvp, struct componentname *scnp,
			     struct sillyrename *sp);
static int      nfs4_openrpc(struct vnode *, struct vnode **,
                            struct componentname *, int, struct vattr *);
static int	nfs4_open_confirm(struct vnode *vp, struct nfs4_compound *cpp,
				 struct nfs4_oparg_open *openap,
				 struct nfs4_oparg_getfh *gfh,
				 struct ucred *cred, struct thread *td);
static int      nfs4_createrpc(struct vnode *, struct vnode **,
                              struct componentname *, nfstype,
                              struct vattr *, char *);

/*
 * Global variables
 */
struct nfs4_lowner nfs4_masterlowner;

#define	DIRHDSIZ	(sizeof (struct dirent) - (MAXNAMLEN + 1))

SYSCTL_DECL(_vfs_nfs4);

static int	nfs4_access_cache_timeout = NFS_MAXATTRTIMO;
SYSCTL_INT(_vfs_nfs4, OID_AUTO, access_cache_timeout, CTLFLAG_RW,
	   &nfs4_access_cache_timeout, 0, "NFS ACCESS cache timeout");

#if 0
static int	nfsv3_commit_on_close = 0;
SYSCTL_INT(_vfs_nfs4, OID_AUTO, nfsv3_commit_on_close, CTLFLAG_RW,
	   &nfsv3_commit_on_close, 0, "write+commit on close, else only write");

SYSCTL_INT(_vfs_nfs4, OID_AUTO, access_cache_hits, CTLFLAG_RD,
	   &nfsstats.accesscache_hits, 0, "NFS ACCESS cache hit count");

SYSCTL_INT(_vfs_nfs4, OID_AUTO, access_cache_misses, CTLFLAG_RD,
	   &nfsstats.accesscache_misses, 0, "NFS ACCESS cache miss count");
#endif

#define	NFSV3ACCESS_ALL (NFSV3ACCESS_READ | NFSV3ACCESS_MODIFY		\
			 | NFSV3ACCESS_EXTEND | NFSV3ACCESS_EXECUTE	\
			 | NFSV3ACCESS_DELETE | NFSV3ACCESS_LOOKUP)
static int
nfs4_v3_access_otw(struct vnode *vp, int wmode, struct thread *td,
    struct ucred *cred)
{
	const int v3 = 1;
	u_int32_t *tl;
	int error = 0, attrflag;

	return (0);

	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	caddr_t bpos, dpos;
	u_int32_t rmode;
	struct nfsnode *np = VTONFS(vp);

	nfsstats.rpccnt[NFSPROC_ACCESS]++;
	mreq = nfsm_reqhead(vp, NFSPROC_ACCESS, NFSX_FH(v3) + NFSX_UNSIGNED);
	mb = mreq;
	bpos = mtod(mb, caddr_t);
	nfsm_fhtom(vp, v3);
	tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(wmode);
	nfsm_request(vp, NFSPROC_ACCESS, td, cred);
	nfsm_postop_attr(vp, attrflag);
	if (!error) {
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		rmode = fxdr_unsigned(u_int32_t, *tl);
		np->n_mode = rmode;
		np->n_modeuid = cred->cr_uid;
		np->n_modestamp = time_second;
	}
	m_freem(mrep);
nfsmout:
	return error;
}

/*
 * nfs access vnode op.
 * For nfs version 2, just return ok. File accesses may fail later.
 * For nfs version 3, use the access rpc to check accessibility. If file modes
 * are changed on the server, accesses might still fail later.
 */
static int
nfs4_access(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	int error = 0;
	u_int32_t mode, wmode;
	int v3 = NFS_ISV3(vp);	/* v3 \in v4 */
	struct nfsnode *np = VTONFS(vp);
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfs4_compound cp;
	struct nfs4_oparg_access acc;
	struct thread *td = ap->a_td;
	struct ucred *cred = ap->a_cred;

	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
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
	 * For nfs v3, check to see if we have done this recently, and if
	 * so return our cached result instead of making an ACCESS call.
	 * If not, do an access rpc, otherwise you are stuck emulating
	 * ufs_access() locally using the vattr. This may not be correct,
	 * since the server may apply other access criteria such as
	 * client uid-->server uid mapping that we do not know about.
	 */
	/* XXX Disable this for now; needs fixing of _access_otw() */
	if (0 && v3) {
		if (ap->a_mode & VREAD)
			mode = NFSV3ACCESS_READ;
		else
			mode = 0;
		if (vp->v_type != VDIR) {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_EXECUTE;
		} else {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
				    NFSV3ACCESS_DELETE);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_LOOKUP;
		}
		/* XXX safety belt, only make blanket request if caching */
		if (nfs4_access_cache_timeout > 0) {
			wmode = NFSV3ACCESS_READ | NFSV3ACCESS_MODIFY |
			    NFSV3ACCESS_EXTEND | NFSV3ACCESS_EXECUTE |
			    NFSV3ACCESS_DELETE | NFSV3ACCESS_LOOKUP;
		} else {
			wmode = mode;
		}

		/*
		 * Does our cached result allow us to give a definite yes to
		 * this request?
		 */
		if (time_second < np->n_modestamp + nfs4_access_cache_timeout &&
		    ap->a_cred->cr_uid == np->n_modeuid &&
		    (np->n_mode & mode) == mode) {
			nfsstats.accesscache_hits++;
		} else {
			/*
			 * Either a no, or a don't know.  Go to the wire.
			 */
			nfsstats.accesscache_misses++;
		        error = nfs4_v3_access_otw(vp, wmode, ap->a_td,
			    ap->a_cred);
			if (error == 0) {
				if ((np->n_mode & mode) != mode)
					error = EACCES;
			}
		}
		return (error);
	}

	/* XXX use generic access code here? */
	mode = ap->a_mode & VREAD ? NFSV4ACCESS_READ : 0;
	if (vp->v_type == VDIR) {
		if (ap->a_mode & VWRITE)
			mode |= NFSV4ACCESS_MODIFY | NFSV4ACCESS_EXTEND | NFSV4ACCESS_DELETE;
		if (ap->a_mode & VEXEC)
			mode |= NFSV4ACCESS_LOOKUP;
	} else {
		if (ap->a_mode & VWRITE)
			mode |= NFSV4ACCESS_MODIFY | NFSV4ACCESS_EXTEND;
		if (ap->a_mode & VEXEC)
			mode |= NFSV4ACCESS_EXECUTE;
	}

	nfs_v4initcompound(&cp);
	acc.mode = mode;

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfsm_v4build_compound(&cp, "nfs4_access()");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_access(&cp, &acc);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_access(&cp, &acc);

	if ((acc.rmode & mode) != mode)
		error = EACCES;

 nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);

	return (error);
}

static int
nfs4_openrpc(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    int flags, struct vattr *vap)
{
	struct vnode *vp = *vpp;
	struct nfs4_oparg_getattr getattr;
	struct nfs4_oparg_getfh getfh;
	struct nfs4_oparg_open opena;
	struct nfs4_compound cp;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct ucred *cred = cnp->cn_cred;
	struct thread *td = cnp->cn_thread;
	struct nfs4_fctx xfc, *fcp;
	struct nfsnode *np;

	if (vp == NULL) {
		/* Create a new file */
		np = NULL;
		fcp = &xfc;
		bzero(fcp, sizeof(*fcp));
	} else {
		np = VTONFS(vp);
		fcp = flags & FWRITE ? &np->n_wfc : &np->n_rfc;
	}

	/*
	 * Since we are currently only one lockowner; we only open the
	 * file once each for reading and writing.
	 */
	if (fcp->refcnt++ != 0) {
		*vpp = vp;
		/*printf("not opening %s\n", np->n_name != NULL ? np->n_name : "");*/
		return (0);
	}

	fcp->lop = &nfs4_masterlowner;
	fcp->np = np;

	nfs_v4initcompound(&cp);
	cp.nmp = VFSTONFS(dvp->v_mount);

	opena.ctype = NCLNULL;
	opena.flags = flags;
	opena.vap = vap;
	opena.fcp = fcp;		/* For lockowner */
	opena.cnp = cnp;

	getattr.bm = &nfsv4_getattrbm;

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfsm_v4build_compound(&cp, "nfs4_openrpc()");
	nfsm_v4build_putfh(&cp, dvp);
	nfsm_v4build_open(&cp, &opena);
	nfsm_v4build_getattr(&cp, &getattr);
	nfsm_v4build_getfh(&cp, &getfh);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp != NULL ? vp : dvp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_open(&cp, &opena);
	nfsm_v4dissect_getattr(&cp, &getattr);
	nfsm_v4dissect_getfh(&cp, &getfh);

	error = nfs_v4postop(&cp, error);

	if (opena.rflags & NFSV4OPENRES_CONFIRM) {
		error = nfs4_open_confirm(vp ? vp : dvp, &cp, &opena, &getfh, cred, td);
		if (error != 0)
			goto nfsmout;
	}

	if (vp == NULL) {
		/* New file */
		error = nfs_nget(dvp->v_mount, &getfh.fh_val,
				 getfh.fh_len, &np, LK_EXCLUSIVE);
		if (error != 0)
			goto nfsmout;

		vp = NFSTOV(np);
		np->n_dvp = dvp;
		np->n_namelen = cnp->cn_namelen; /* XXX memory leaks on these; track! */
		if (np->n_name != NULL)
			free(np->n_name, M_NFSREQ);
		np->n_name = malloc(np->n_namelen + 1, M_NFSREQ, M_WAITOK);
		bcopy(cnp->cn_nameptr, np->n_name, np->n_namelen);
		np->n_name[np->n_namelen] = '\0';
		if (flags & FWRITE)
			np->n_wfc = *fcp;
		else
			np->n_rfc = *fcp;

		/*printf("opened new file %s\n", np->n_name);*/

		nfs4_vnop_loadattrcache(vp, &getattr.fa, NULL);
		*vpp = vp;
	} else {
		/*printf("openend \"old\" %s\n", np->n_name != NULL ? np->n_name : "");*/

		if (flags & O_TRUNC && np->n_size != 0) {
			struct vattr va;

			VATTR_NULL(&va);
			va.va_size = 0;
			error = nfs4_setattrrpc(vp, &va, cnp->cn_cred);
		}
		np->n_attrstamp = 0;
	}

 nfsmout:
	if (mrep != NULL)
		m_freem(mrep);

	return (error);
}

static int
nfs4_open_confirm(struct vnode *vp, struct nfs4_compound *cpp,
    struct nfs4_oparg_open *openap, struct nfs4_oparg_getfh *gfh,
    struct ucred *cred, struct thread *td)
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;

	nfs_v4initcompound(cpp);
	cpp->nmp = VFSTONFS(vp->v_mount);

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfsm_v4build_compound(cpp, "nfs4_open_confirm()");
	nfsm_v4build_putfh_nv(cpp, gfh);
	nfsm_v4build_open_confirm(cpp, openap);
	nfsm_v4build_finalize(cpp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(cpp);
	nfsm_v4dissect_putfh(cpp);
	nfsm_v4dissect_open_confirm(cpp, openap);

 nfsmout:
	error = nfs_v4postop(cpp, error);

	if (mrep != NULL)
		m_freem(mrep);

	return (error);
}


/*
 * nfs open vnode op
 * Check to see if the type is ok
 * and that deletion is not in progress.
 * For paged in text files, you will need to flush the page cache
 * if consistency is lost.
 */
/* ARGSUSED */
static int
nfs4_open(struct vop_open_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	enum vtype vtype = vp->v_type;
	int mode = ap->a_mode;
	struct componentname cn;

	if (vtype != VREG) {
		if (vtype != VDIR && vtype != VLNK) {
#ifdef DIAGNOSTIC
			printf("open eacces vtyp=%d\n", vp->v_type);
#endif
			return (EACCES);
		} else
			return (0);
	}

	if (np->n_flag & NCREATED) {
		np->n_flag &= ~NCREATED;
		return (0);
	}

	cn.cn_nameptr = np->n_name;
	cn.cn_namelen = np->n_namelen;
	cn.cn_cred = ap->a_cred;
	cn.cn_thread = ap->a_td;

	return (nfs4_openrpc(np->n_dvp, &vp, &cn, mode, NULL));
}

static int
nfs4_closerpc(struct vnode *vp, struct ucred *cred, int flags)
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct thread *td;
	struct nfs4_fctx *fcp;
	struct nfs4_compound cp;
	struct nfsnode *np = VTONFS(vp);

	td = curthread;
	fcp = flags & FWRITE ? &np->n_wfc : &np->n_rfc;

	nfs_v4initcompound(&cp);

	if (--fcp->refcnt != 0)
		return (0);

	/*printf("closing %s\n", np->n_name != NULL ? np->n_name : "");*/

	cp.fcp = fcp;

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfsm_v4build_compound(&cp, "nfs4_closerpc()");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_close(&cp, fcp);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_close(&cp, fcp);

 nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);

	return (error);
}

/*
 * nfs close vnode op
 * play it safe for now (see comments in v2/v3 nfs_close regarding dirty buffers)
 */
/* ARGSUSED */
static int
nfs4_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;

	if (vp->v_type != VREG)
		return (0);

	if (np->n_flag & NMODIFIED) {
		error = nfs_vinvalbuf(vp, V_SAVE, ap->a_td, 1);
		np->n_attrstamp = 0;
	}

	error = nfs4_closerpc(vp, ap->a_cred, ap->a_fflag);

	if (!error && np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		error = np->n_error;
	}
	return (error);
}

/*
 * nfs getattr call from vfs.
 */
static int
nfs4_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfs4_oparg_getattr ga;
	struct nfs4_compound cp;

	/*
	 * Update local times for special files.
	 */
	if (np->n_flag & (NACC | NUPD))
		np->n_flag |= NCHG;
	/*
	 * First look in the cache.
	 */
	if (nfs_getattrcache(vp, ap->a_vap) == 0)
		return (0);

	nfsstats.rpccnt[NFSPROC_GETATTR]++;

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, NFSX_FH(1));
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	ga.bm = &nfsv4_getattrbm;
	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_getattr()");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_getattr(&cp, &ga);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, curthread, ap->a_cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_getattr(&cp, &ga);

	nfs4_vnop_loadattrcache(vp, &ga.fa, ap->a_vap);

nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);
	return (error);
}

/*
 * nfs setattr call.
 */
static int
nfs4_setattr(struct vop_setattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr *vap = ap->a_vap;
	struct thread *td = curthread;
	int error = 0;
	u_quad_t tsize;

#ifndef nolint
	tsize = (u_quad_t)0;
#endif

	/*
	 * Setting of flags is not supported.
	 */
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			if (vap->va_mtime.tv_sec == VNOVAL &&
			    vap->va_atime.tv_sec == VNOVAL &&
			    vap->va_mode == (mode_t)VNOVAL &&
			    vap->va_uid == (uid_t)VNOVAL &&
			    vap->va_gid == (gid_t)VNOVAL)
				return (0);
 			vap->va_size = VNOVAL;
 			break;
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);

			/*
			 *  We run vnode_pager_setsize() early (why?),
			 * we must set np->n_size now to avoid vinvalbuf
			 * V_SAVE races that might setsize a lower
			 * value.
			 */

			tsize = np->n_size;
			error = nfs_meta_setsize(vp, ap->a_cred, td,
			    vap->va_size);

 			if (np->n_flag & NMODIFIED) {
 			    if (vap->va_size == 0)
 				error = nfs_vinvalbuf(vp, 0, td, 1);
 			    else
 				error = nfs_vinvalbuf(vp, V_SAVE, td, 1);
 			    if (error) {
				vnode_pager_setsize(vp, np->n_size);
 				return (error);
			    }
 			}
			/*
			 * np->n_size has already been set to vap->va_size
			 * in nfs_meta_setsize(). We must set it again since
			 * nfs_loadattrcache() could be called through
			 * nfs_meta_setsize() and could modify np->n_size.
			 */
 			np->n_vattr.va_size = np->n_size = vap->va_size;
  		};
  	} else if ((vap->va_mtime.tv_sec != VNOVAL ||
		vap->va_atime.tv_sec != VNOVAL) && (np->n_flag & NMODIFIED) &&
		vp->v_type == VREG &&
  		(error = nfs_vinvalbuf(vp, V_SAVE, td, 1)) == EINTR)
		return (error);

	if (vap->va_size != VNOVAL && np->n_wfc.refcnt == 0) {
		/* Have to open the file before we can truncate it */
		struct componentname cn;

		cn.cn_nameptr = np->n_name;
		cn.cn_namelen = np->n_namelen;
		cn.cn_cred = ap->a_cred;
		cn.cn_thread = td;
		error = nfs4_openrpc(np->n_dvp, &vp, &cn, FWRITE, NULL);
		if (error)
			return error;
		np->n_flag |= NTRUNCATE;
	}

	error = nfs4_setattrrpc(vp, vap, ap->a_cred);
	if (error && vap->va_size != VNOVAL) {
		np->n_size = np->n_vattr.va_size = tsize;
		vnode_pager_setsize(vp, np->n_size);
	}
	return (error);
}

/*
 * Do an nfs setattr rpc.
 */
static int
nfs4_setattrrpc(struct vnode *vp, struct vattr *vap, struct ucred *cred)
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct thread *td;
	struct nfs4_compound cp;
	struct nfs4_oparg_getattr ga;
	struct nfsnode *np = VTONFS(vp);
	struct nfs4_fctx *fcp;

	td = curthread;
	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	ga.bm = &nfsv4_getattrbm;
	fcp = (vap->va_size != VNOVAL) ? &np->n_wfc : NULL;
	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_setattrrpc");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_setattr(&cp, vap, fcp);
	nfsm_v4build_getattr(&cp, &ga);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_setattr(&cp);
	nfsm_v4dissect_getattr(&cp, &ga);

	nfs4_vnop_loadattrcache(vp, &ga.fa, NULL);

	/* TODO: do the settatr and close in a single compound rpc */
	if (np->n_flag & NTRUNCATE) {
		error = nfs4_closerpc(vp, cred, FWRITE);
		np->n_flag &= ~NTRUNCATE;
	}

nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);
	
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, unlock the directory nfsnode and do the rpc
 */
static int
nfs4_lookup(struct vop_lookup_args *ap)
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	int isdot, flags = cnp->cn_flags;
	struct vnode *newvp;
	struct nfsmount *nmp;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	long len;
	nfsfh_t *fhp;
	struct nfsnode *np;
	int error = 0, fhsize;
	struct thread *td = cnp->cn_thread;
	struct nfs4_compound cp;
	struct nfs4_oparg_getattr ga, dga;
	struct nfs4_oparg_lookup l;
	struct nfs4_oparg_getfh gfh;

	*vpp = NULLVP;
	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	nmp = VFSTONFS(dvp->v_mount);
	np = VTONFS(dvp);

	isdot = cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.';

	if ((error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, td)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	if ((error = cache_lookup(dvp, vpp, cnp)) && error != ENOENT) {
		struct vattr vattr;

		newvp = *vpp;
		if (!VOP_GETATTR(newvp, &vattr, cnp->cn_cred)
		 && vattr.va_ctime.tv_sec == VTONFS(newvp)->n_ctime) {
		     nfsstats.lookupcache_hits++;
		     if (cnp->cn_nameiop != LOOKUP &&
			 (flags & ISLASTCN))
			     cnp->cn_flags |= SAVENAME;
		     return (0);
		}
		cache_purge(newvp);
		if (newvp != dvp)
			vput(newvp);
		else
			vrele(newvp);
	}

	error = 0;
	newvp = NULLVP;
	nfsstats.lookupcache_misses++;
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;

	len = cnp->cn_namelen;
	mreq = nfsm_reqhead(NULL, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	ga.bm = &nfsv4_getattrbm;
	dga.bm = &nfsv4_getattrbm;
	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_lookup()");
	nfsm_v4build_putfh(&cp, dvp);
	nfsm_v4build_getattr(&cp, &dga);
	if (flags & ISDOTDOT)
		nfsm_v4build_lookupp(&cp);
	else if (!isdot) {
		l.name = cnp->cn_nameptr;
		l.namelen = len;
		nfsm_v4build_lookup(&cp, &l);
	}
	nfsm_v4build_getattr(&cp, &ga);
	nfsm_v4build_getfh(&cp, &gfh);
	nfsm_v4build_finalize(&cp);

	nfsm_request(dvp, NFSV4PROC_COMPOUND, cnp->cn_thread, cnp->cn_cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_getattr(&cp, &dga);
	if (flags & ISDOTDOT)
		nfsm_v4dissect_lookupp(&cp);
	else if (!isdot)
		nfsm_v4dissect_lookup(&cp);
	nfsm_v4dissect_getattr(&cp, &ga);
	nfsm_v4dissect_getfh(&cp, &gfh);

	nfs4_vnop_loadattrcache(dvp, &dga.fa, NULL);
	fhp = &gfh.fh_val;
	fhsize = gfh.fh_len;

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && (flags & ISLASTCN)) {
		if (NFS_CMPFH(np, fhp, fhsize))
			return (EISDIR);

		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np, LK_EXCLUSIVE);
		if (error)
			return (error);

		newvp = NFSTOV(np);

		nfs4_vnop_loadattrcache(newvp, &ga.fa, NULL);

		*vpp = newvp;
		cnp->cn_flags |= SAVENAME;
		return (0);
	}

	if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0);

		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np, LK_EXCLUSIVE);
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		if (error)
			return (error);
		newvp = NFSTOV(np);

		nfs4_vnop_loadattrcache(newvp, &ga.fa, NULL);
	} else if (NFS_CMPFH(np, fhp, fhsize)) {
		VREF(dvp);
		newvp = dvp;
	} else {
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np, LK_EXCLUSIVE);
		if (error)
			return (error);
		newvp = NFSTOV(np);

		/* Fill in np used by open. */
		np->n_dvp = dvp;
		np->n_namelen = cnp->cn_namelen;
		if (np->n_name != NULL)
			free(np->n_name, M_NFSREQ);
		np->n_name = malloc(np->n_namelen + 1, M_NFSREQ, M_WAITOK);
		bcopy(cnp->cn_nameptr, np->n_name, np->n_namelen);
		np->n_name[np->n_namelen] = '\0';

		nfs4_vnop_loadattrcache(newvp, &ga.fa, NULL);
	}

	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
		cnp->cn_flags |= SAVENAME;
	if ((cnp->cn_flags & MAKEENTRY) &&
	    (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN))) {
		np->n_ctime = np->n_vattr.va_ctime.tv_sec;
		cache_enter(dvp, newvp, cnp);
	}
	*vpp = newvp;
	m_freem(mrep);
nfsmout:
	error = nfs_v4postop(&cp, error);

	if (error) {
		if (newvp != NULLVP) {
			vrele(newvp);
			*vpp = NULLVP;
		}
		if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
		    (flags & ISLASTCN) && error == ENOENT) {
			if (dvp->v_mount->mnt_flag & MNT_RDONLY)
				error = EROFS;
			else
				error = EJUSTRETURN;
		}
		if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
			cnp->cn_flags |= SAVENAME;
	}

	return (error);
}

/*
 * nfs read call.
 * Just call nfs_bioread() to do the work.
 */
static int
nfs4_read(struct vop_read_args *ap)
{
	struct vnode *vp = ap->a_vp;

	switch (vp->v_type) {
	case VREG:
		return (nfs_bioread(vp, ap->a_uio, ap->a_ioflag, ap->a_cred));
	case VDIR:
		return (EISDIR);
	default:
		return (EOPNOTSUPP);
	}
}

/*
 * nfs readlink call
 */
static int
nfs4_readlink(struct vop_readlink_args *ap)
{
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VLNK)
		return (EINVAL);
	return (nfs_bioread(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Do a readlink rpc.
 * Called by nfs_doio() from below the buffer cache.
 */
int
nfs4_readlinkrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfs4_compound cp;

	nfsstats.rpccnt[NFSPROC_READLINK]++;

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_readlinkrpc()");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_readlink(&cp);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, uiop->uio_td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_readlink(&cp, uiop);

nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
int
nfs4_readrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfsmount *nmp;
	int error = 0, len, tsiz;
	struct nfs4_compound cp;
	struct nfs4_oparg_read read;
	struct nfsnode *np = VTONFS(vp);

	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > nmp->nm_maxfilesize)
		return (EFBIG);

	if (tsiz == 0)
		return (0);

	read.uiop = uiop;
	read.fcp = np->n_rfc.refcnt > 0 ? &np->n_rfc : &np->n_wfc;

	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;

		read.off = uiop->uio_offset;
		read.maxcnt = len;
		nfs_v4initcompound(&cp);

		mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
		mb = mreq;
		bpos = mtod(mb, caddr_t);

		nfsm_v4build_compound(&cp, "nfs4_readrpc()");
		nfsm_v4build_putfh(&cp, vp);
		nfsm_v4build_read(&cp, &read);
		nfsm_v4build_finalize(&cp);

		nfsm_request(vp, NFSV4PROC_COMPOUND, uiop->uio_td, cred);
		if (error != 0) {
			error = nfs_v4postop(&cp, error);
			goto nfsmout;
		}

		nfsm_v4dissect_compound(&cp);
		nfsm_v4dissect_putfh(&cp);
		nfsm_v4dissect_read(&cp, &read);

		if (read.eof || read.retlen == 0)
			tsiz = 0;
		else
			tsiz -= read.retlen;

		error = nfs_v4postop(&cp, error);

		m_freem(mrep);
		mrep = NULL;
	}
nfsmout:
	if (mrep != NULL)
		m_freem(mrep);
	
	return (error);
}

/*
 * nfs write call
 */
int
nfs4_writerpc(struct vnode *vp, struct uio *uiop, struct ucred *cred,
    int *iomode, int *must_commit)
{
	int32_t backup;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, len, tsiz, wccflag = 1, rlen;
	struct nfs4_compound cp;
	struct nfs4_oparg_write write;
	nfsv4stablehow commit, committed = NSHFILESYNC;
	caddr_t verf;
	struct nfsnode *np = VTONFS(vp);

#ifndef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfs: writerpc iovcnt > 1");
#endif
	*must_commit = 0;
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > nmp->nm_maxfilesize)
		return (EFBIG);

	if (tsiz == 0)
		return (0);

	write.stable = (nfsv4stablehow)*iomode;
	write.uiop = uiop;
	write.fcp = &np->n_wfc;

	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = (tsiz > nmp->nm_wsize) ? nmp->nm_wsize : tsiz;

		write.off = uiop->uio_offset;
		write.cnt = len;
		nfs_v4initcompound(&cp);

		mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
		mb = mreq;
		bpos = mtod(mb, caddr_t);

		nfsm_v4build_compound(&cp, "nfs4_writerpc()");
		nfsm_v4build_putfh(&cp, vp);
		nfsm_v4build_write(&cp, &write);
		nfsm_v4build_finalize(&cp);

		nfsm_request(vp, NFSV4PROC_COMPOUND, uiop->uio_td, cred);
		if (error != 0) {
			error = nfs_v4postop(&cp, error);
			goto nfsmout;
		}

		nfsm_v4dissect_compound(&cp);
		nfsm_v4dissect_putfh(&cp);
		nfsm_v4dissect_write(&cp, &write);

		rlen = write.retlen;
		if (rlen == 0) {
			error = NFSERR_IO;
			break;
		} else if (rlen < len) {
			backup = len - rlen;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base -  backup;
			uiop->uio_iov->iov_len += backup;
                        uiop->uio_offset -= backup;
                        uiop->uio_resid += backup;
                        len = rlen;
		}

		commit = write.committed;

		if (committed == NSHFILESYNC ||
		    (committed = NSHDATASYNC && commit == NSHUNSTABLE))
			committed = commit;

		verf = (caddr_t)write.wverf;

                if ((nmp->nm_flag & NFSSTA_HASWRITEVERF) == 0) {
                        bcopy(verf, nmp->nm_verf, NFSX_V4VERF);
                        nmp->nm_flag |= NFSMNT_HASWRITEVERF;
                } else if (bcmp(verf, nmp->nm_verf, NFSX_V4VERF)) {
                        *must_commit = 1;
                        bcopy(verf, nmp->nm_verf, NFSX_V4VERF);
                }

		/* XXX wccflag */
		if (wccflag)
			VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr.va_mtime;

		error = nfs_v4postop(&cp, error);

		m_freem(mrep);
		mrep = NULL;
		if (error)
			break;
		tsiz -= len;
	}
nfsmout:
	if (mrep != NULL)
		m_freem(mrep);
	*iomode = committed;
	if (error)
		uiop->uio_resid = tsiz;
	return (error);
}

/* ARGSUSED */
static int
nfs4_mknod(struct vop_mknod_args *ap)
{
	struct vattr *vap = ap->a_vap;
	struct vnode *newvp = NULL;
	int error;

	error = nfs4_createrpc(ap->a_dvp, &newvp,
	    ap->a_cnp, (nfstype)vap->va_type, vap, NULL);

	/* XXX - is this actually referenced here? */
	if (error == 0) {
		*ap->a_vpp = newvp;
		vrele(newvp);
	}

	return (error);
}

static int
nfs4_createrpc(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    nfstype ftype, struct vattr *vap, char *linktarget)
{
	struct nfsnode *dnp = VTONFS(dvp);
	struct nfsnode *np = NULL;
	struct vnode *newvp = NULL;
	struct nfs4_compound cp;
	struct nfs4_oparg_create c;
	struct nfs4_oparg_getattr ga;
	struct nfs4_oparg_getfh gfh;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	int error = 0;

	nfsstats.rpccnt[NFSPROC_CREATE]++;

	mreq = nfsm_reqhead(dvp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	bzero(&c, sizeof(c));
	bzero(&ga, sizeof(ga));

	c.type = ftype;
	c.vap = vap;
	c.linktext = linktarget;
	c.name = cnp->cn_nameptr;
	c.namelen = cnp->cn_namelen;

	ga.bm = &nfsv4_getattrbm;
	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_createrpc()");
	nfsm_v4build_putfh(&cp, dvp);
	nfsm_v4build_create(&cp, &c);
	nfsm_v4build_getattr(&cp, &ga);
	nfsm_v4build_getfh(&cp, &gfh);	
	nfsm_v4build_finalize(&cp); 

	nfsm_request(dvp, NFSV4PROC_COMPOUND, cnp->cn_thread, cnp->cn_cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_create(&cp, &c);
	nfsm_v4dissect_getattr(&cp, &ga);
	nfsm_v4dissect_getfh(&cp, &gfh);	
	
	error = nfs_nget(dvp->v_mount, &gfh.fh_val, gfh.fh_len, &np, LK_EXCLUSIVE);
	if (error != 0)
		goto nfsmout;

	newvp = NFSTOV(np);
	nfs4_vnop_loadattrcache(newvp, &ga.fa, NULL);

	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, newvp, cnp);

	dnp->n_flag |= NMODIFIED;
	dnp->n_attrstamp = 0;

 nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);

	/* XXX */
	/*free(cnp->cn_pnbuf, M_NAMEI);*/
	if (error != 0 && newvp != NULL)
		vput(newvp);
	else if (error == 0)
		*vpp = newvp;

	return (error);
}

static int
nfs4_renamerpc(struct vnode *fdvp, const char *fnameptr, int fnamelen,
    struct vnode *tdvp, const char *tnameptr, int tnamelen,
    struct ucred *cred, struct thread *td)
{

	struct nfsnode *fnp = VTONFS(fdvp), *tnp = VTONFS(tdvp);
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfs4_compound cp;
	struct nfs4_oparg_rename r;
	int error = 0;

	nfsstats.rpccnt[NFSPROC_RENAME]++;

	r.fname = fnameptr;
	r.fnamelen = fnamelen;
	r.tname = tnameptr;
	r.tnamelen = tnamelen;
	nfs_v4initcompound(&cp);

	mreq = nfsm_reqhead(fdvp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfsm_v4build_compound(&cp, "nfs4_renamerpc()");
	nfsm_v4build_putfh(&cp, fdvp);
	nfsm_v4build_savefh(&cp);
	nfsm_v4build_putfh(&cp, tdvp);
	nfsm_v4build_rename(&cp, &r);
	nfsm_v4build_finalize(&cp);

	nfsm_request(fdvp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_savefh(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_rename(&cp);

	/* XXX should this always be performed?  */
	fnp->n_flag |= NMODIFIED;
	tnp->n_flag |= NMODIFIED;
	fnp->n_attrstamp = tnp->n_attrstamp = 0;

 nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);

	return (error);
}

/*
 * nfs file create call
 */
static int
nfs4_create(struct vop_create_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct nfsnode *dnp = VTONFS(dvp);
	struct componentname *cnp = ap->a_cnp;
	struct vnode *newvp = NULL;
	int error = 0, fmode = (O_CREAT | FREAD | FWRITE);
	struct vattr vattr;

	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred)) != 0)
		return (error);

	if (vap->va_vaflags & VA_EXCLUSIVE)
		fmode |= O_EXCL;

	error = nfs4_openrpc(dvp, &newvp, cnp, fmode, vap);
	if (error != 0)
		goto out;

	VTONFS(newvp)->n_flag |= NCREATED;

	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, newvp, cnp);

	*ap->a_vpp = newvp;

	dnp->n_flag |= NMODIFIED;
	dnp->n_attrstamp = 0;	/* XXX; wccflag */

 out:
	return (error);
}

/*
 * nfs file remove call
 * To try and make nfs semantics closer to ufs semantics, a file that has
 * other processes using the vnode is renamed instead of removed and then
 * removed later on the last close.
 * - If v_usecount > 1
 *	  If a rename is not already in the works
 *	     call nfs4_sillyrename() to set it up
 *     else
 *	  do the remove rpc
 */
static int
nfs4_remove(struct vop_remove_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	struct vattr vattr;

#ifndef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("nfs4_remove: no name");
	if (vrefcnt(vp) < 1)
		panic("nfs4_remove: bad v_usecount");
#endif
	if (vp->v_type == VDIR)
		error = EPERM;
	else if (vrefcnt(vp) == 1 || (np->n_sillyrename &&
	    !VOP_GETATTR(vp, &vattr, cnp->cn_cred) && vattr.va_nlink > 1)) {
		/*
		 * Purge the name cache so that the chance of a lookup for
		 * the name succeeding while the remove is in progress is
		 * minimized. Without node locking it can still happen, such
		 * that an I/O op returns ESTALE, but since you get this if
		 * another host removes the file..
		 */
		cache_purge(vp);
		/*
		 * throw away biocache buffers, mainly to avoid
		 * unnecessary delayed writes later.
		 */
		error = nfs_vinvalbuf(vp, 0, cnp->cn_thread, 1);
		/* Do the rpc */
		if (error != EINTR)
			error = nfs4_removerpc(dvp, cnp->cn_nameptr,
				cnp->cn_namelen, cnp->cn_cred, cnp->cn_thread);
		/*
		 * Kludge City: If the first reply to the remove rpc is lost..
		 *   the reply to the retransmitted request will be ENOENT
		 *   since the file was in fact removed
		 *   Therefore, we cheat and return success.
		 */
		if (error == ENOENT)
			error = 0;
	} else if (!np->n_sillyrename)
		error = nfs4_sillyrename(dvp, vp, cnp);
	np->n_attrstamp = 0;
	return (error);
}

/*
 * nfs file remove rpc called from nfs_inactive
 */
int
nfs4_removeit(struct sillyrename *sp)
{
	/*
	 * Make sure that the directory vnode is still valid.
	 * XXX we should lock sp->s_dvp here.
	 */
	if (sp->s_dvp->v_type == VBAD)
		return (0);
	return (nfs4_removerpc(sp->s_dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		NULL));
}

/*
 * Nfs remove rpc, called from nfs4_remove() and nfs4_removeit().
 */
static int
nfs4_removerpc(struct vnode *dvp, const char *name, int namelen,
    struct ucred *cred, struct thread *td)
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfs4_compound cp;

	nfsstats.rpccnt[NFSPROC_REMOVE]++;

	mreq = nfsm_reqhead(dvp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_removerpc()");
	nfsm_v4build_putfh(&cp, dvp);
	nfsm_v4build_remove(&cp, name, namelen);
	nfsm_v4build_finalize(&cp);

	nfsm_request(dvp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_remove(&cp);

 nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);

	VTONFS(dvp)->n_flag |= NMODIFIED;
	VTONFS(dvp)->n_attrstamp = 0; /* XXX wccflag */

	return (error);
}

/*
 * nfs file rename call
 */
static int
nfs4_rename(struct vop_rename_args *ap)
{
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	int error;

 #ifndef DIAGNOSTIC
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("nfs4_rename: no name");
#endif
	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	if (fvp == tvp) {
		printf("nfs4_rename: fvp == tvp (can't happen)\n");
		error = 0;
		goto out;
	}
	if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
		goto out;

	/*
	 * We have to flush B_DELWRI data prior to renaming
	 * the file.  If we don't, the delayed-write buffers
	 * can be flushed out later after the file has gone stale
	 * under NFSV3.  NFSV2 does not have this problem because
	 * ( as far as I can tell ) it flushes dirty buffers more
	 * often.
	 */
	VOP_FSYNC(fvp, MNT_WAIT, fcnp->cn_thread);
	VOP_UNLOCK(fvp, 0);
	if (tvp)
	    VOP_FSYNC(tvp, MNT_WAIT, tcnp->cn_thread);

	/*
	 * If the tvp exists and is in use, sillyrename it before doing the
	 * rename of the new file over it.
	 * XXX Can't sillyrename a directory.
	 */
	if (tvp && vrefcnt(tvp) > 1 && !VTONFS(tvp)->n_sillyrename &&
		tvp->v_type != VDIR && !nfs4_sillyrename(tdvp, tvp, tcnp)) {
		vput(tvp);
		tvp = NULL;
	}

	error = nfs4_renamerpc(fdvp, fcnp->cn_nameptr, fcnp->cn_namelen,
		tdvp, tcnp->cn_nameptr, tcnp->cn_namelen, tcnp->cn_cred,
		tcnp->cn_thread);

	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}

out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs file rename rpc called from nfs4_remove() above
 */
static int
nfs4_renameit(struct vnode *sdvp, struct componentname *scnp,
    struct sillyrename *sp)
{
	return (nfs4_renamerpc(sdvp, scnp->cn_nameptr, scnp->cn_namelen, sdvp,
	    sp->s_name, sp->s_namlen, scnp->cn_cred, scnp->cn_thread));
}

/*
 * nfs hard link create call
 */
static int
nfs4_link(struct vop_link_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfs4_compound cp;
	struct nfs4_oparg_link l;

	if (vp->v_mount != tdvp->v_mount) {
		return (EXDEV);
	}

	/*
	 * Push all writes to the server, so that the attribute cache
	 * doesn't get "out of sync" with the server.
	 * XXX There should be a better way!
	 */
	VOP_FSYNC(vp, MNT_WAIT, cnp->cn_thread);

	nfsstats.rpccnt[NFSPROC_LINK]++;

	l.name = cnp->cn_nameptr;
	l.namelen = cnp->cn_namelen;
	nfs_v4initcompound(&cp);

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfsm_v4build_compound(&cp, "nfs4_link()");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_savefh(&cp);
	nfsm_v4build_putfh(&cp, tdvp);
	nfsm_v4build_link(&cp, &l);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, cnp->cn_thread, cnp->cn_cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_savefh(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_link(&cp);

	VTONFS(tdvp)->n_flag |= NMODIFIED;
	VTONFS(vp)->n_attrstamp = 0;
	VTONFS(tdvp)->n_attrstamp = 0;

nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);

	return (error);
}

/*
 * nfs symbolic link create call
 */
static int
nfs4_symlink(struct vop_symlink_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	int error = 0;
	struct vnode *newvp = NULL;

	nfsstats.rpccnt[NFSPROC_SYMLINK]++;

	error = nfs4_createrpc(ap->a_dvp, &newvp, ap->a_cnp, NFLNK,
	    ap->a_vap, ap->a_target);

	if (error != 0 && newvp != NULL)
		vput(newvp);
	else if (error == 0)
		 *ap->a_vpp = newvp;

	VTONFS(dvp)->n_flag |= NMODIFIED;
	VTONFS(dvp)->n_attrstamp = 0; /* XXX wccflags */

	return (error);
}

/*
 * nfs make dir call
 */
static int
nfs4_mkdir(struct vop_mkdir_args *ap)
{
	return (nfs4_createrpc(ap->a_dvp, ap->a_vpp, ap->a_cnp, NFDIR,
		    ap->a_vap, NULL));
}

/*
 * nfs remove directory call
 */
static int
nfs4_rmdir(struct vop_rmdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct nfsnode *dnp = VTONFS(dvp);
	struct componentname *cnp = ap->a_cnp;
	int error = 0;

	if (dvp == vp)
		return (EINVAL);

	error = (nfs4_removerpc(dvp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_cred,
			       NULL));
	if (error)
		return (error);

	dnp->n_flag |= NMODIFIED;
	dnp->n_attrstamp = 0;
	cache_purge(dvp);
	cache_purge(vp);

	return (error);
}

/*
 * nfs readdir call
 */
static int
nfs4_readdir(struct vop_readdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct uio *uio = ap->a_uio;
	int tresid, error;
	struct vattr vattr;

	if (vp->v_type != VDIR)
		return (EPERM);
	/*
	 * First, check for hit on the EOF offset cache
	 */
	if (np->n_direofoffset > 0 && uio->uio_offset >= np->n_direofoffset &&
	    (np->n_flag & NMODIFIED) == 0) {
		if (!VOP_GETATTR(vp, &vattr, ap->a_cred) &&
			!NFS_TIMESPEC_COMPARE(&np->n_mtime, &vattr.va_mtime)) {
			nfsstats.direofcache_hits++;
			return (0);
		}
	}

	/*
	 * Call nfs_bioread() to do the real work.
	 */
	tresid = uio->uio_resid;
	error = nfs_bioread(vp, uio, 0, ap->a_cred);

	if (!error && uio->uio_resid == tresid)
		nfsstats.direofcache_misses++;
	return (error);
}

static u_char fty_to_dty[] = {
	DT_UNKNOWN,		/* NFNON */
	DT_REG,			/* NFREG */
	DT_DIR,			/* NFDIR */
	DT_BLK,			/* NFBLK */
	DT_CHR,			/* NFCHR */
	DT_LNK,			/* NFLNK */
	DT_SOCK,		/* NFSOCK */
	DT_FIFO,		/* NFFIFO */
        DT_UNKNOWN,		/* NFATTRDIT */
        DT_UNKNOWN,		/* NFNAMEDATTR */
        DT_UNKNOWN,		/* NFBAD */
};

/*
 * Readdir rpc call.
 * Called from below the buffer cache by nfs_doio().
 */
int
nfs4_readdirrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	caddr_t p;
	uint64_t *cookiep;
	caddr_t bpos, dpos;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	uint64_t cookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp);
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	struct nfs4_compound cp;
	struct nfs4_oparg_readdir readdir;
	struct nfsv4_fattr fattr;
	u_int fty;

#ifndef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 || (uiop->uio_offset & (DIRBLKSIZ - 1)) ||
		(uiop->uio_resid & (DIRBLKSIZ - 1)))
		panic("nfs readdirrpc bad uio");
#endif

	/*
	 * If there is no cookie, assume directory was stale.
	 */
	cookiep = nfs4_getcookie(dnp, uiop->uio_offset, 0);
	if (cookiep)
		cookie = *cookiep;
	else
		return (NFSERR_BAD_COOKIE);

	/* Generate fake entries for "." and ".." */
	while (cookie < 2 && bigenough) {
		cookie++;
		len = 4 + DIRHDSIZ;

		if (len > uiop->uio_resid) {
			bigenough = 0;
			break;
		}
		dp = (struct dirent *)uiop->uio_iov->iov_base;

		dp->d_namlen = cookie;
		dp->d_reclen = len;
		dp->d_type = DT_DIR;
		if (cookie == 1)
			dp->d_fileno = dnp->n_vattr.va_fileid; /* XXX has problems with pynfs virtualhandles */
		else
			dp->d_fileno = dnp->n_dvp != NULL ?
			    VTONFS(dnp->n_dvp)->n_vattr.va_fileid : cookie;

		p = dp->d_name;
		*p++ = '.';
		if (cookie == 2)
			*p++ = '.';
		*p = '\0';

		blksiz += len;
		if (blksiz == DIRBLKSIZ)
			blksiz = 0;
		uiop->uio_offset += len;
		uiop->uio_resid -= len;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base + len;
		uiop->uio_iov->iov_len -= len;
	}

	if (cookie == 2)
		cookie = 0;

	/* This is sort of ugly, to prevent v4postop() from acting weird */
	bzero(&cp, sizeof(cp));

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of DIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	/*
	 * XXX this is sort of ugly for nfsv4; we don't maintain the
	 * strict abstraction, but do the decoding inline.  that's ok.
	 */
	while (more_dirs && bigenough) {
		nfsstats.rpccnt[NFSPROC_READDIR]++;

		mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
		mb = mreq;
		bpos = mtod(mb, caddr_t);

		readdir.cnt = nmp->nm_readdirsize;
		readdir.cookie = cookie;
		readdir.bm = &nfsv4_readdirbm;
		if (cookie == 0)
			bzero(&readdir.verf, sizeof(readdir.verf));
		else
			bcopy(&dnp->n_cookieverf, &readdir.verf,
			    sizeof(readdir.verf));

		nfs_v4initcompound(&cp);

		nfsm_v4build_compound(&cp, "nfs4_readdirrpc()");
		nfsm_v4build_putfh(&cp, vp);
		nfsm_v4build_readdir(&cp, &readdir);
		nfsm_v4build_finalize(&cp);

		nfsm_request(vp, NFSV4PROC_COMPOUND, uiop->uio_td, cred);
		if (error != 0)
			goto nfsmout;

		nfsm_v4dissect_compound(&cp);
		nfsm_v4dissect_putfh(&cp);

		/*
		 * XXX - Readdir gets handled inline like in
		 * NFSv{2,3}.  This is a nasty inconsistency and
		 * should be fixed.
		 */

		tl = nfsm_dissect(uint32_t *, 5 * NFSX_UNSIGNED);
		if (fxdr_unsigned(uint32_t, *tl++) != NFSV4OP_READDIR) {
			error = EBADRPC;
			goto nfsmout;
		}
		if (fxdr_unsigned(uint32_t, *tl++) != 0) {
			error = EBADRPC;
			goto nfsmout;
		}

		bcopy(tl, &dnp->n_cookieverf, NFSX_V4VERF);
		tl += 2;
		more_dirs = fxdr_unsigned(int, *tl++);

		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			tl = nfsm_dissect(uint32_t *, 3 * NFSX_UNSIGNED);
			cookie = fxdr_hyper(tl);
			tl += 2;
			/* XXX cookie sanity check */
			len = fxdr_unsigned(int, *tl++);
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				goto nfsmout;
			}
			tlen = nfsm_rndup(len);
			if (tlen == len)
				tlen += 4;	/* To ensure null termination */
			left = DIRBLKSIZ - blksiz;
			if ((tlen + DIRHDSIZ) > left) {
				dp->d_reclen += left;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_offset += left;
				uiop->uio_resid -= left;
				blksiz = 0;
			}
			if ((tlen + DIRHDSIZ) > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				dp = (struct dirent *)uiop->uio_iov->iov_base;

				dp->d_namlen = len;
				dp->d_reclen = tlen + DIRHDSIZ;

				blksiz += dp->d_reclen;
				if (blksiz == DIRBLKSIZ)
					blksiz = 0;
				uiop->uio_offset += DIRHDSIZ;
				uiop->uio_resid -= DIRHDSIZ;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;

				/* Copy name */
				nfsm_mtouio(uiop, len);
				p = uiop->uio_iov->iov_base;
				tlen -= len;
				*p = '\0';	/* null terminate */
				/* printf("nfs4_readdirrpc: name: \"%s\" cookie %d\n",
				   p - len, (int) cookie);*/
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_offset += tlen;
				uiop->uio_resid -= tlen;

				/* Copy attributes */
				nfsm_v4dissect_attrs(&fattr);

				dp->d_fileno = nfs_v4fileid4_to_fileid(
					fattr.fa4_valid & FA4V_FILEID &&
					    fattr.fa4_fileid ?
					    fattr.fa4_fileid : cookie);

				fty = (u_int)fattr.fa4_type;
				dp->d_type = fattr.fa4_valid & FA4V_TYPE &&
				    (fty < sizeof(fty_to_dty)) ?
				    fty_to_dty[fty] : DT_UNKNOWN;
			} else
				nfsm_adv(nfsm_rndup(len));

			tl = nfsm_dissect(uint32_t *, NFSX_UNSIGNED);
			more_dirs = fxdr_unsigned(int, *tl++);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);
		}

		error = nfs_v4postop(&cp, error);

		m_freem(mrep);
		mrep = NULL;
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = DIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		uiop->uio_iov->iov_base =
		    (char *)uiop->uio_iov->iov_base + left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_offset += left;
		uiop->uio_resid -= left;
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough)
		dnp->n_direofoffset = uiop->uio_offset;
	else {
		if (uiop->uio_resid > 0)
			printf("EEK! readdirrpc resid > 0\n");
		cookiep = nfs4_getcookie(dnp, uiop->uio_offset, 1);
		*cookiep = cookie;
	}
nfsmout:
	if (mrep != NULL)
		m_freem(mrep);
	return (error);
}

/*
 * Silly rename. To make the NFS filesystem that is stateless look a little
 * more like the "ufs" a remove of an active vnode is translated to a rename
 * to a funny looking filename that is removed by nfs_inactive on the
 * nfsnode. There is the potential for another process on a different client
 * to create the same funny name between the nfs_lookitup() fails and the
 * nfs_rename() completes, but...
 */
static int
nfs4_sillyrename(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	struct sillyrename *sp;
	struct nfsnode *np;
	int error;
	short pid;

	cache_purge(dvp);
	np = VTONFS(vp);
#ifndef DIAGNOSTIC
	if (vp->v_type == VDIR)
		panic("nfs: sillyrename dir");
#endif
	sp = malloc(sizeof (struct sillyrename), M_NFSREQ, M_WAITOK);
	sp->s_cred = crhold(cnp->cn_cred);
	sp->s_dvp = dvp;
	sp->s_removeit = nfs4_removeit;
	VREF(dvp);

	/* Fudge together a funny name */
	pid = cnp->cn_thread->td_proc->p_pid;
	sp->s_namlen = sprintf(sp->s_name, ".nfsA%04x4.4", pid);

	/* Try lookitups until we get one that isn't there */
	while (nfs4_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		cnp->cn_thread, NULL) == 0) {
		sp->s_name[4]++;
		if (sp->s_name[4] > 'z') {
			error = EINVAL;
			goto bad;
		}
	}
	error = nfs4_renameit(dvp, cnp, sp);
	if (error)
		goto bad;
	error = nfs4_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		cnp->cn_thread, &np);
	np->n_sillyrename = sp;
	return (0);
bad:
	vrele(sp->s_dvp);
	crfree(sp->s_cred);
	free((caddr_t)sp, M_NFSREQ);
	return (error);
}

/*
 * Look up a file name and optionally either update the file handle or
 * allocate an nfsnode, depending on the value of npp.
 * npp == NULL	--> just do the lookup
 * *npp == NULL --> allocate a new nfsnode and make sure attributes are
 *			handled too
 * *npp != NULL --> update the file handle in the vnode
 */
static int
nfs4_lookitup(struct vnode *dvp, const char *name, int len, struct ucred *cred,
    struct thread *td, struct nfsnode **npp)
{
	struct vnode *newvp = NULL;
	struct nfsnode *np, *dnp = VTONFS(dvp);
	caddr_t bpos, dpos;
	int error = 0, fhlen;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	nfsfh_t *nfhp;
	struct nfs4_compound cp;
	struct nfs4_oparg_lookup l;
	struct nfs4_oparg_getfh gfh;
	struct nfs4_oparg_getattr ga;

	nfsstats.rpccnt[NFSPROC_RENAME]++;

	mreq = nfsm_reqhead(dvp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	l.name = name;
	l.namelen = len;

	nfs_v4initcompound(&cp);

	ga.bm = &nfsv4_getattrbm;

	nfsm_v4build_compound(&cp, "nfs4_renamerpc()");
	nfsm_v4build_putfh(&cp, dvp);
	nfsm_v4build_lookup(&cp, &l);
	nfsm_v4build_getfh(&cp, &gfh);
	nfsm_v4build_getattr(&cp, &ga);

	nfsm_request(dvp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_lookup(&cp);
	nfsm_v4dissect_getfh(&cp, &gfh);
	nfsm_v4dissect_getattr(&cp, &ga);

	if (npp != NULL && error == 0) {
		nfhp = &gfh.fh_val;
		fhlen = gfh.fh_len;

		if (*npp != NULL) {
			np = *npp;
			if (np->n_fhsize > NFS_SMALLFH && fhlen <= NFS_SMALLFH) {
				free((caddr_t)np->n_fhp, M_NFSBIGFH);
				np->n_fhp = &np->n_fh;
			} else if (np->n_fhsize <= NFS_SMALLFH && fhlen>NFS_SMALLFH)
				np->n_fhp =(nfsfh_t *)malloc(fhlen, M_NFSBIGFH, M_WAITOK);
			bcopy((caddr_t)nfhp, (caddr_t)np->n_fhp, fhlen);
			np->n_fhsize = fhlen;
			newvp = NFSTOV(np);
		} else if (NFS_CMPFH(dnp, nfhp, fhlen)) {
			VREF(dvp);
			newvp = dvp;
		} else {
			error = nfs_nget(dvp->v_mount, nfhp, fhlen, &np, LK_EXCLUSIVE);
			if (error) {
				m_freem(mrep);
				return (error);
			}
			newvp = NFSTOV(np);
		}

		if (newvp != dvp) {
			np->n_dvp = dvp;
			np->n_namelen = len;
			if (np->n_name != NULL)
				free(np->n_name, M_NFSREQ);
			np->n_name = malloc(np->n_namelen + 1, M_NFSREQ, M_WAITOK);
			memcpy(np->n_name, name, len);
			np->n_name[len] = '\0';
		}
		nfs4_vnop_loadattrcache(newvp, &ga.fa, NULL);
	}

nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep != NULL)
		m_freem(mrep);
	if (npp && *npp == NULL) {
		if (error) {
			if (newvp) {
				if (newvp == dvp)
					vrele(newvp);
				else
					vput(newvp);
			}
		} else
			*npp = np;
	}


	return (error);
}

/*
 * Nfs Version 3 commit rpc
 */
int
nfs4_commit(struct vnode *vp, u_quad_t offset, int cnt, struct ucred *cred,
    struct thread *td)
{
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfs4_compound cp;
	struct nfs4_oparg_commit commit;

	if ((nmp->nm_state & NFSSTA_HASWRITEVERF) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_COMMIT]++;

	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, 0);
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	commit.start = offset;
	commit.len = cnt;

	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_commit()");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_commit(&cp, &commit);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_commit(&cp, &commit);
	
	/* XXX */
	/* nfsm_wcc_data(vp, wccflag);*/
	if (bcmp(nmp->nm_verf, commit.verf, NFSX_V4VERF)) {
		bcopy(commit.verf, nmp->nm_verf, NFSX_V4VERF);
		error = NFSERR_STALEWRITEVERF;
	}

nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep == NULL)
		m_freem(mrep);
	return (error);
}

/*
 * Strategy routine.
 * For async requests when nfsiod(s) are running, queue the request by
 * calling nfs_asyncio(), otherwise just all nfs_doio() to do the
 * request.
 */
static int
nfs4_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;
	struct ucred *cr;
	int error = 0;

	KASSERT(!(bp->b_flags & B_DONE),
	    ("nfs4_strategy: buffer %p unexpectedly marked B_DONE", bp));
	BUF_ASSERT_HELD(bp);

	if (bp->b_iocmd == BIO_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;

	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if ((bp->b_flags & B_ASYNC) == 0 ||
		nfs_asyncio(VFSTONFS(ap->a_vp->v_mount), bp, NOCRED, curthread))
		error = nfs_doio(ap->a_vp, bp, cr, curthread);
	return (error);
}

/*
 * fsync vnode op. Just call nfs4_flush() with commit == 1.
 */
/* ARGSUSED */
static int
nfs4_fsync(struct vop_fsync_args *ap)
{
	return (nfs4_flush(ap->a_vp, ap->a_waitfor, ap->a_td, 1));
}

/*
 * Flush all the blocks associated with a vnode.
 * 	Walk through the buffer pool and push any dirty pages
 *	associated with the vnode.
 */
static int
nfs4_flush(struct vnode *vp, int waitfor, struct thread *td,
    int commit)
{
	struct nfsnode *np = VTONFS(vp);
	struct bufobj *bo;
	struct buf *bp;
	int i;
	struct buf *nbp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, slptimeo = 0, slpflag = 0, retv, bvecpos;
	int passone = 1;
	u_quad_t off, endoff, toff;
	struct ucred* wcred = NULL;
	struct buf **bvec = NULL;
#ifndef NFS_COMMITBVECSIZ
#define NFS_COMMITBVECSIZ	20
#endif
	struct buf *bvec_on_stack[NFS_COMMITBVECSIZ];
	int bvecsize = 0, bveccount;
	bo = &vp->v_bufobj;

	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = PCATCH;
	if (!commit)
		passone = 0;
	/*
	 * A b_flags == (B_DELWRI | B_NEEDCOMMIT) block has been written to the
	 * server, but nas not been committed to stable storage on the server
	 * yet. On the first pass, the byte range is worked out and the commit
	 * rpc is done. On the second pass, nfs_writebp() is called to do the
	 * job.
	 */
again:
	off = (u_quad_t)-1;
	endoff = 0;
	bvecpos = 0;
	if (NFS_ISV3(vp) && commit) {
		if (bvec != NULL && bvec != bvec_on_stack)
			free(bvec, M_TEMP);
		/*
		 * Count up how many buffers waiting for a commit.
		 */
		bveccount = 0;
		BO_LOCK(bo);
		TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
			if (!BUF_ISLOCKED(bp) &&
			    (bp->b_flags & (B_DELWRI | B_NEEDCOMMIT))
				== (B_DELWRI | B_NEEDCOMMIT))
				bveccount++;
		}
		/*
		 * Allocate space to remember the list of bufs to commit.  It is
		 * important to use M_NOWAIT here to avoid a race with nfs4_write.
		 * If we can't get memory (for whatever reason), we will end up
		 * committing the buffers one-by-one in the loop below.
		 */
		if (bveccount > NFS_COMMITBVECSIZ) {
			/*
			 * Release the vnode interlock to avoid a lock
			 * order reversal.
			 */
			BO_UNLOCK(bo);
			bvec = (struct buf **)
				malloc(bveccount * sizeof(struct buf *),
				       M_TEMP, M_NOWAIT);
			BO_LOCK(bo);
			if (bvec == NULL) {
				bvec = bvec_on_stack;
				bvecsize = NFS_COMMITBVECSIZ;
			} else
				bvecsize = bveccount;
		} else {
			bvec = bvec_on_stack;
			bvecsize = NFS_COMMITBVECSIZ;
		}
		TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
			if (bvecpos >= bvecsize)
				break;
			if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL)) {
				nbp = TAILQ_NEXT(bp, b_bobufs);
				continue;
			}
			if ((bp->b_flags & (B_DELWRI | B_NEEDCOMMIT)) !=
			    (B_DELWRI | B_NEEDCOMMIT)) {
				BUF_UNLOCK(bp);
				nbp = TAILQ_NEXT(bp, b_bobufs);
				continue;
			}
			BO_UNLOCK(bo);
			bremfree(bp);
			/*
			 * Work out if all buffers are using the same cred
			 * so we can deal with them all with one commit.
			 *
			 * NOTE: we are not clearing B_DONE here, so we have
			 * to do it later on in this routine if we intend to
			 * initiate I/O on the bp.
			 *
			 * Note: to avoid loopback deadlocks, we do not
			 * assign b_runningbufspace.
			 */
			if (wcred == NULL)
				wcred = bp->b_wcred;
			else if (wcred != bp->b_wcred)
				wcred = NOCRED;
			vfs_busy_pages(bp, 1);

			BO_LOCK(bo);
			/*
			 * bp is protected by being locked, but nbp is not
			 * and vfs_busy_pages() may sleep.  We have to
			 * recalculate nbp.
			 */
			nbp = TAILQ_NEXT(bp, b_bobufs);

			/*
			 * A list of these buffers is kept so that the
			 * second loop knows which buffers have actually
			 * been committed. This is necessary, since there
			 * may be a race between the commit rpc and new
			 * uncommitted writes on the file.
			 */
			bvec[bvecpos++] = bp;
			toff = ((u_quad_t)bp->b_blkno) * DEV_BSIZE +
				bp->b_dirtyoff;
			if (toff < off)
				off = toff;
			toff += (u_quad_t)(bp->b_dirtyend - bp->b_dirtyoff);
			if (toff > endoff)
				endoff = toff;
		}
		BO_UNLOCK(bo);
	}
	if (bvecpos > 0) {
		/*
		 * Commit data on the server, as required.
		 * If all bufs are using the same wcred, then use that with
		 * one call for all of them, otherwise commit each one
		 * separately.
		 */
		if (wcred != NOCRED)
			retv = nfs4_commit(vp, off, (int)(endoff - off),
					  wcred, td);
		else {
			retv = 0;
			for (i = 0; i < bvecpos; i++) {
				off_t off, size;
				bp = bvec[i];
				off = ((u_quad_t)bp->b_blkno) * DEV_BSIZE +
					bp->b_dirtyoff;
				size = (u_quad_t)(bp->b_dirtyend
						  - bp->b_dirtyoff);
				retv = nfs4_commit(vp, off, (int)size,
						  bp->b_wcred, td);
				if (retv) break;
			}
		}

		if (retv == NFSERR_STALEWRITEVERF)
			nfs_clearcommit(vp->v_mount);

		/*
		 * Now, either mark the blocks I/O done or mark the
		 * blocks dirty, depending on whether the commit
		 * succeeded.
		 */
		for (i = 0; i < bvecpos; i++) {
			bp = bvec[i];
			bp->b_flags &= ~(B_NEEDCOMMIT | B_CLUSTEROK);
			if (retv) {
				/*
				 * Error, leave B_DELWRI intact
				 */
				vfs_unbusy_pages(bp);
				brelse(bp);
			} else {
				/*
				 * Success, remove B_DELWRI ( bundirty() ).
				 *
				 * b_dirtyoff/b_dirtyend seem to be NFS
				 * specific.  We should probably move that
				 * into bundirty(). XXX
				 */
				bufobj_wref(bo);
				bp->b_flags |= B_ASYNC;
				bundirty(bp);
				bp->b_flags &= ~B_DONE;
				bp->b_ioflags &= ~BIO_ERROR;
				bp->b_dirtyoff = bp->b_dirtyend = 0;
				bufdone(bp);
			}
		}
	}

	/*
	 * Start/do any write(s) that are required.
	 */
loop:
	BO_LOCK(bo);
	TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL)) {
			if (waitfor != MNT_WAIT || passone)
				continue;

			error = BUF_TIMELOCK(bp,
			    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK,
			    BO_MTX(bo), "nfsfsync", slpflag, slptimeo);
			if (error == 0)
				panic("nfs4_fsync: inconsistent lock");
			if (error == ENOLCK) {
				error = 0;
				goto loop;
			}
			if (nfs4_sigintr(nmp, NULL, td)) {
				error = EINTR;
				goto done;
			}
			if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			}
			goto loop;
		}
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("nfs4_fsync: not dirty");
		if ((passone || !commit) && (bp->b_flags & B_NEEDCOMMIT)) {
			BUF_UNLOCK(bp);
			continue;
		}
		BO_LOCK(bo);
		bremfree(bp);
		if (passone || !commit)
		    bp->b_flags |= B_ASYNC;
		else
		    bp->b_flags |= B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	if (passone) {
		passone = 0;
		BO_UNLOCK(bo);
		goto again;
	}
	if (waitfor == MNT_WAIT) {
		while (bo->bo_numoutput) {
			error = bufobj_wwait(bo, slpflag, slptimeo);
			if (error) {
			    BO_UNLOCK(bo);
			    if (nfs4_sigintr(nmp, NULL, td)) {
				error = EINTR;
				goto done;
			    }
			    if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			    }
			    BO_LOCK(bo);
			}
		}
		if (vp->v_bufobj.bo_dirty.bv_cnt > 0 && commit) {
			BO_UNLOCK(bo);
			goto loop;
		}
	}
	BO_UNLOCK(bo);
	if (np->n_flag & NWRITEERR) {
		error = np->n_error;
		np->n_flag &= ~NWRITEERR;
	}
done:
	if (bvec != NULL && bvec != bvec_on_stack)
		free(bvec, M_TEMP);
	return (error);
}

/*
 * NFS advisory byte-level locks.
 */
static int
nfs4_advlock(struct vop_advlock_args *ap)
{
	struct vnode *vp = ap->a_vp;
	u_quad_t size;
	int error;

	return (EPERM);

	error = vn_lock(vp, LK_SHARED);
	if (error)
		return (error);
	if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NOLOCKD) != 0) {
		size = VTONFS(vp)->n_size;
		VOP_UNLOCK(vp, 0);
		error = lf_advlock(ap, &(vp->v_lockf), size);
	} else
		error = nfs_dolock(ap);
	return (error);
}

/*
 * NFS advisory byte-level locks.
 */
static int
nfs4_advlockasync(struct vop_advlockasync_args *ap)
{
	struct vnode *vp = ap->a_vp;
	u_quad_t size;
	int error;

	return (EPERM);

	error = vn_lock(vp, LK_SHARED);
	if (error)
		return (error);
	if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NOLOCKD) != 0) {
		size = VTONFS(vp)->n_size;
		VOP_UNLOCK(vp, 0);
		error = lf_advlockasync(ap, &(vp->v_lockf), size);
	} else {
		VOP_UNLOCK(vp, 0);
		error = EOPNOTSUPP;
	}
	return (error);
}

/*
 * Print out the contents of an nfsnode.
 */
static int
nfs4_print(struct vop_print_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	printf("\tfileid %ld fsid 0x%x",
	   np->n_vattr.va_fileid, np->n_vattr.va_fsid);
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	printf("\n");
	return (0);
}

/*
 * This is the "real" nfs::bwrite(struct buf*).
 * We set B_CACHE if this is a VMIO buffer.
 */
int
nfs4_writebp(struct buf *bp, int force __unused, struct thread *td)
{
	int s;
	int oldflags = bp->b_flags;
#if 0
	int retv = 1;
	off_t off;
#endif

	BUF_ASSERT_HELD(bp);

	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return(0);
	}

	bp->b_flags |= B_CACHE;

	/*
	 * Undirty the bp.  We will redirty it later if the I/O fails.
	 */

	s = splbio();
	bundirty(bp);
	bp->b_flags &= ~B_DONE;
	bp->b_ioflags &= ~BIO_ERROR;
	bp->b_iocmd = BIO_WRITE;

	bufobj_wref(bp->b_bufobj);
	curthread->td_ru.ru_oublock++;
	splx(s);

	/*
	 * Note: to avoid loopback deadlocks, we do not
	 * assign b_runningbufspace.
	 */
	vfs_busy_pages(bp, 1);

	BUF_KERNPROC(bp);
	bp->b_iooffset = dbtob(bp->b_blkno);
	bstrategy(bp);

	if( (oldflags & B_ASYNC) == 0) {
		int rtval = bufwait(bp);

		if (oldflags & B_DELWRI) {
			s = splbio();
			reassignbuf(bp);
			splx(s);
		}

		brelse(bp);
		return (rtval);
	}

	return (0);
}

/*
 * Just call nfs_writebp() with the force argument set to 1.
 *
 * NOTE: B_DONE may or may not be set in a_bp on call.
 */
static int
nfs4_bwrite(struct buf *bp)
{

	return (nfs4_writebp(bp, 1, curthread));
}

struct buf_ops buf_ops_nfs4 = {
	.bop_name	=	"buf_ops_nfs4",
	.bop_write	=	nfs4_bwrite,
	.bop_strategy	=	bufstrategy,
	.bop_sync	=	bufsync,
	.bop_bdflush	=	bufbdflush,
};
