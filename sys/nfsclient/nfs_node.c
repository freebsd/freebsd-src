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

#include <vm/vm_zone.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfsmount.h>

static vm_zone_t nfsnode_zone;
static LIST_HEAD(nfsnodehashhead, nfsnode) *nfsnodehashtbl;
static u_long nfsnodehash;
static int nfs_node_hash_lock;

#define TRUE	1
#define	FALSE	0

SYSCTL_DECL(_debug_hashstat);

/*
 * Grab an atomic snapshot of the nfsnode hash chain lengths
 */
static int
sysctl_debug_hashstat_rawnfsnode(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct nfsnodehashhead *nnpp;
	struct nfsnode *nnp;
	int n_nfsnode;
	int count;

	n_nfsnode = nfsnodehash + 1;	/* nfsnodehash = max index, not count */
	if (!req->oldptr)
		return SYSCTL_OUT(req, 0, n_nfsnode * sizeof(int));

	/* Scan hash tables for applicable entries */
	for (nnpp = nfsnodehashtbl; n_nfsnode > 0; n_nfsnode--, nnpp++) {
		count = 0;
		LIST_FOREACH(nnp, nnpp, n_hash) {
			count++;
		}
		error = SYSCTL_OUT(req, (caddr_t)&count, sizeof(count));
		if (error)
			return (error);
	}
	return (0);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, rawnfsnode, CTLTYPE_INT|CTLFLAG_RD, 0, 0,
	    sysctl_debug_hashstat_rawnfsnode, "S,int", "nfsnode chain lengths");

static int
sysctl_debug_hashstat_nfsnode(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct nfsnodehashhead *nnpp;
	struct nfsnode *nnp;
	int n_nfsnode;
	int count, maxlength, used, pct;

	if (!req->oldptr)
		return SYSCTL_OUT(req, 0, 4 * sizeof(int));

	n_nfsnode = nfsnodehash + 1;	/* nfsnodehash = max index, not count */
	used = 0;
	maxlength = 0;

	/* Scan hash tables for applicable entries */
	for (nnpp = nfsnodehashtbl; n_nfsnode > 0; n_nfsnode--, nnpp++) {
		count = 0;
		LIST_FOREACH(nnp, nnpp, n_hash) {
			count++;
		}
		if (count)
			used++;
		if (maxlength < count)
			maxlength = count;
	}
	n_nfsnode = nfsnodehash + 1;
	pct = (used * 100 * 100) / n_nfsnode;
	error = SYSCTL_OUT(req, (caddr_t)&n_nfsnode, sizeof(n_nfsnode));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, (caddr_t)&used, sizeof(used));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, (caddr_t)&maxlength, sizeof(maxlength));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, (caddr_t)&pct, sizeof(pct));
	if (error)
		return (error);
	return (0);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, nfsnode, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_debug_hashstat_nfsnode, "I", "nfsnode chain lengths");

/*
 * Initialize hash links for nfsnodes
 * and build nfsnode free list.
 */
void
nfs_nhinit(void)
{

	nfsnode_zone = zinit("NFSNODE", sizeof(struct nfsnode), 0, 0, 1);
	nfsnodehashtbl = hashinit(desiredvnodes, M_NFSHASH, &nfsnodehash);
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
	struct nfsnode *np, *np2;
	struct nfsnodehashhead *nhpp;
	struct vnode *vp;
	struct vnode *nvp;
	int error;
	int rsflags;
	struct nfsmount *nmp;

	/*
	 * Calculate nfs mount point and figure out whether the rslock should
	 * be interruptable or not.
	 */
	nmp = VFSTONFS(mntp);
	if (nmp->nm_flag & NFSMNT_INT)
		rsflags = PCATCH;
	else
		rsflags = 0;

retry:
	nhpp = NFSNOHASH(fnv_32_buf(fhp->fh_bytes, fhsize, FNV1_32_INIT));
loop:
	LIST_FOREACH(np, nhpp, n_hash) {
		if (mntp != NFSTOV(np)->v_mount || np->n_fhsize != fhsize ||
		    bcmp((caddr_t)fhp, (caddr_t)np->n_fhp, fhsize))
			continue;
		vp = NFSTOV(np);
		/*
		 * np or vp may become invalid if vget() blocks, so loop 
		 */
		if (vget(vp, LK_EXCLUSIVE|LK_SLEEPFAIL, td))
			goto loop;
		*npp = np;
		return(0);
	}
	/*
	 * Obtain a lock to prevent a race condition if the getnewvnode()
	 * or MALLOC() below happens to block.
	 */
	if (nfs_node_hash_lock) {
		while (nfs_node_hash_lock) {
			nfs_node_hash_lock = -1;
			tsleep(&nfs_node_hash_lock, PVM, "nfsngt", 0);
		}
		goto loop;
	}
	nfs_node_hash_lock = 1;

	/*
	 * Allocate before getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if zalloc should block.
	 */
	np = zalloc(nfsnode_zone);

	error = getnewvnode(VT_NFS, mntp, nfsv2_vnodeop_p, &nvp);
	if (error) {
		if (nfs_node_hash_lock < 0)
			wakeup(&nfs_node_hash_lock);
		nfs_node_hash_lock = 0;
		*npp = 0;
		zfree(nfsnode_zone, np);
		return (error);
	}
	vp = nvp;
	bzero((caddr_t)np, sizeof *np);
	vp->v_data = np;
	np->n_vnode = vp;
	/*
	 * Insert the nfsnode in the hash queue for its new file handle
	 */
	LIST_FOREACH(np2, nhpp, n_hash) {
		if (mntp != NFSTOV(np2)->v_mount || np2->n_fhsize != fhsize ||
		    bcmp((caddr_t)fhp, (caddr_t)np2->n_fhp, fhsize))
			continue;
		vrele(vp);
		if (nfs_node_hash_lock < 0)
			wakeup(&nfs_node_hash_lock);
		nfs_node_hash_lock = 0;
		zfree(nfsnode_zone, np);
		goto retry;
	}
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	if (fhsize > NFS_SMALLFH) {
		MALLOC(np->n_fhp, nfsfh_t *, fhsize, M_NFSBIGFH, M_WAITOK);
	} else
		np->n_fhp = &np->n_fh;
	bcopy((caddr_t)fhp, (caddr_t)np->n_fhp, fhsize);
	np->n_fhsize = fhsize;
	lockinit(&np->n_rslock, PVFS | rsflags, "nfrslk", 0, LK_NOPAUSE);
	lockinit(&vp->v_lock, PVFS, "nfsnlk", 0, LK_NOPAUSE);
	*npp = np;

	if (nfs_node_hash_lock < 0)
		wakeup(&nfs_node_hash_lock);
	nfs_node_hash_lock = 0;

	/*
	 * Lock the new nfsnode.
	 */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	return (0);
}

int
nfs_inactive(struct vop_inactive_args *ap)
{
	struct nfsnode *np;
	struct sillyrename *sp;
	struct thread *td = curthread;	/* XXX */

	np = VTONFS(ap->a_vp);
	if (prtactive && ap->a_vp->v_usecount != 0)
		vprint("nfs_inactive: pushing active", ap->a_vp);
	if (ap->a_vp->v_type != VDIR) {
		sp = np->n_sillyrename;
		np->n_sillyrename = (struct sillyrename *)0;
	} else
		sp = (struct sillyrename *)0;
	if (sp) {
		/*
		 * We need a reference to keep the vnode from being
		 * recycled by getnewvnode while we do the I/O
		 * associated with discarding the buffers unless we
		 * are being forcibly unmounted in which case we already
		 * have our own reference.
		 */
		if (ap->a_vp->v_usecount > 0)
			(void) nfs_vinvalbuf(ap->a_vp, 0, sp->s_cred, td, 1);
		else if (vget(ap->a_vp, 0, td))
			panic("nfs_inactive: lost vnode");
		else {
			(void) nfs_vinvalbuf(ap->a_vp, 0, sp->s_cred, td, 1);
			vrele(ap->a_vp);
		}
		/*
		 * Remove the silly file that was rename'd earlier
		 */
		nfs_removeit(sp);
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
		FREE((caddr_t)sp, M_NFSREQ);
	}
	np->n_flag &= (NMODIFIED | NFLUSHINPROG | NFLUSHWANT);
	VOP_UNLOCK(ap->a_vp, 0, ap->a_td);
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

	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", vp);

	if (np->n_hash.le_prev != NULL)		/* XXX beware */
		LIST_REMOVE(np, n_hash);

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

	cache_purge(vp);
	zfree(nfsnode_zone, vp->v_data);
	vp->v_data = (void *)0;
	return (0);
}

#if 0
/*
 * Lock an nfsnode
 */
int
nfs_lock(struct vop_lock_args *ap)
{
	struct vnode *vp = ap->a_vp;

	/*
	 * Ugh, another place where interruptible mounts will get hung.
	 * If you make this sleep interruptible, then you have to fix all
	 * the VOP_LOCK() calls to expect interruptibility.
	 */
	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		(void) tsleep((caddr_t)vp, PINOD, "nfslck", 0);
	}
	if (vp->v_tag == VT_NON)
		return (ENOENT);

#if 0
	/*
	 * Only lock regular files.  If a server crashed while we were
	 * holding a directory lock, we could easily end up sleeping
	 * until the server rebooted while holding a lock on the root.
	 * Locks are only needed for protecting critical sections in
	 * VMIO at the moment.
	 * New vnodes will have type VNON but they should be locked
	 * since they may become VREG.  This is checked in loadattrcache
	 * and unwanted locks are released there.
	 */
	if (vp->v_type == VREG || vp->v_type == VNON) {
		while (np->n_flag & NLOCKED) {
			np->n_flag |= NWANTED;
			(void) tsleep((caddr_t) np, PINOD, "nfslck2", 0);
			/*
			 * If the vnode has transmuted into a VDIR while we
			 * were asleep, then skip the lock.
			 */
			if (vp->v_type != VREG && vp->v_type != VNON)
				return (0);
		}
		np->n_flag |= NLOCKED;
	}
#endif

	return (0);
}

/*
 * Unlock an nfsnode
 */
int
nfs_unlock(struct vop_unlock_args *ap)
{
#if 0
	struct vnode* vp = ap->a_vp;
        struct nfsnode* np = VTONFS(vp);

	if (vp->v_type == VREG || vp->v_type == VNON) {
		if (!(np->n_flag & NLOCKED))
			panic("nfs_unlock: nfsnode not locked");
		np->n_flag &= ~NLOCKED;
		if (np->n_flag & NWANTED) {
			np->n_flag &= ~NWANTED;
			wakeup((caddr_t) np);
		}
	}
#endif

	return (0);
}

/*
 * Check for a locked nfsnode
 */
int
nfs_islocked(struct vop_islocked_args *ap)
{

	return VTONFS(ap->a_vp)->n_flag & NLOCKED ? 1 : 0;
}
#endif

