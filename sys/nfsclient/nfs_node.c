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
 *	@(#)nfs_node.c	8.6 (Berkeley) 5/22/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fnv_hash.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/uma.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfsmount.h>

static uma_zone_t nfsnode_zone;

#define TRUE	1
#define	FALSE	0

void
nfs_nhinit(void)
{

	nfsnode_zone = uma_zcreate("NFSNODE", sizeof(struct nfsnode), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}

void
nfs_nhuninit(void)
{
	uma_zdestroy(nfsnode_zone);
}

struct nfs_vncmp {
	int	fhsize;
	void	*fh;
};

static int
nfs_vncmpf(struct vnode *vp, void *arg)
{
	struct nfs_vncmp *a;
	struct nfsnode *np;

	a = arg;
	np = VTONFS(vp);
	return (bcmp(a->fh, np->n_fhp, a->fhsize));
}

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
int
nfs_nget(struct mount *mntp, nfsfh_t *fhp, int fhsize, struct nfsnode **npp)
{
	struct thread *td = curthread;	/* XXX */
	struct nfsnode *np;
	struct vnode *vp;
	struct vnode *nvp;
	int error;
	u_int hash;
	int rsflags;
	struct nfsmount *nmp;
	struct nfs_vncmp ncmp;

	/*
	 * Calculate nfs mount point and figure out whether the rslock should
	 * be interruptible or not.
	 */
	nmp = VFSTONFS(mntp);
	if (nmp->nm_flag & NFSMNT_INT)
		rsflags = PCATCH;
	else
		rsflags = 0;

	*npp = NULL;

	hash = fnv_32_buf(fhp->fh_bytes, fhsize, FNV1_32_INIT);
	ncmp.fhsize = fhsize;
	ncmp.fh = fhp;

	error = vfs_hash_get(mntp, hash, LK_EXCLUSIVE,
	    td, &nvp, nfs_vncmpf, &ncmp);
	if (error)
		return (error);
	if (nvp != NULL) {
		*npp = VTONFS(nvp);
		return (0);
	}

	/*
	 * Allocate before getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if zalloc should block.
	 */
	np = uma_zalloc(nfsnode_zone, M_WAITOK | M_ZERO);

	if (nmp->nm_flag & NFSMNT_NFSV4)
		error = getnewvnode("nfs4", mntp, &nfs4_vnodeops, &nvp);
	else
		error = getnewvnode("nfs", mntp, &nfs_vnodeops, &nvp);
	if (error) {
		uma_zfree(nfsnode_zone, np);
		return (error);
	}
	vp = nvp;
	if (nmp->nm_flag & NFSMNT_NFSV4)
		vp->v_bufobj.bo_ops = &buf_ops_nfs4;
	else
		vp->v_bufobj.bo_ops = &buf_ops_nfs;
	vp->v_data = np;
	np->n_vnode = vp;
	error = vfs_hash_insert(vp, hash, LK_EXCLUSIVE,
	    td, &nvp, nfs_vncmpf, &ncmp);
	if (error)
		return (error);
	if (nvp != NULL) {
		*npp = VTONFS(nvp);
		/* XXX I wonder of nfs_reclaim will survive the unused vnode */
		return (0);
	}
	if (fhsize > NFS_SMALLFH) {
		MALLOC(np->n_fhp, nfsfh_t *, fhsize, M_NFSBIGFH, M_WAITOK);
	} else
		np->n_fhp = &np->n_fh;
	bcopy((caddr_t)fhp, (caddr_t)np->n_fhp, fhsize);
	np->n_fhsize = fhsize;
	lockinit(&np->n_rslock, PVFS | rsflags, "nfrslk", 0, 0);
	*npp = np;

	return (0);
}

int
nfs_inactive(struct vop_inactive_args *ap)
{
	struct nfsnode *np;
	struct sillyrename *sp;
	struct thread *td = curthread;	/* XXX */

	np = VTONFS(ap->a_vp);
	if (prtactive && vrefcnt(ap->a_vp) != 0)
		vprint("nfs_inactive: pushing active", ap->a_vp);
	if (ap->a_vp->v_type != VDIR) {
		sp = np->n_sillyrename;
		np->n_sillyrename = NULL;
	} else
		sp = NULL;
	if (sp) {
		(void)nfs_vinvalbuf(ap->a_vp, 0, td, 1);
		/*
		 * Remove the silly file that was rename'd earlier
		 */
		(sp->s_removeit)(sp);
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
		FREE((caddr_t)sp, M_NFSREQ);
	}
	np->n_flag &= NMODIFIED;
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
int
nfs_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsdmap *dp, *dp2;

	if (prtactive && vrefcnt(vp) != 0)
		vprint("nfs_reclaim: pushing active", vp);

	vfs_hash_remove(vp);

	/*
	 * Free up any directory cookie structures and
	 * large file handle structures that might be associated with
	 * this nfs node.
	 */
	if (vp->v_type == VDIR) {
		dp = LIST_FIRST(&np->n_cookies);
		while (dp) {
			dp2 = dp;
			dp = LIST_NEXT(dp, ndm_list);
			FREE((caddr_t)dp2, M_NFSDIROFF);
		}
	}
	if (np->n_fhsize > NFS_SMALLFH) {
		FREE((caddr_t)np->n_fhp, M_NFSBIGFH);
	}

	lockdestroy(&np->n_rslock);
	uma_zfree(nfsnode_zone, vp->v_data);
	vp->v_data = NULL;
	vnode_destroy_vobject(vp);
	return (0);
}
