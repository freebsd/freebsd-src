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
 * $Id: nfs_node.c,v 1.17 1997/05/09 13:04:43 dfr Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>

LIST_HEAD(nfsnodehashhead, nfsnode) *nfsnodehashtbl;
u_long nfsnodehash;

#define TRUE	1
#define	FALSE	0

/*
 * Initialize hash links for nfsnodes
 * and build nfsnode free list.
 */
void
nfs_nhinit()
{

#ifndef lint
	if ((sizeof(struct nfsnode) - 1) & sizeof(struct nfsnode))
		printf("nfs_nhinit: bad size %d\n", sizeof(struct nfsnode));
#endif /* not lint */
	nfsnodehashtbl = hashinit(desiredvnodes, M_NFSNODE, &nfsnodehash);
}

/*
 * Compute an entry in the NFS hash table structure
 */
u_long
nfs_hash(fhp, fhsize)
	register nfsfh_t *fhp;
	int fhsize;
{
	register u_char *fhpp;
	register u_long fhsum;
	register int i;

	fhpp = &fhp->fh_bytes[0];
	fhsum = 0;
	for (i = 0; i < fhsize; i++)
		fhsum += *fhpp++;
	return (fhsum);
}

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
int nfs_node_hash_lock;

int
nfs_nget(mntp, fhp, fhsize, npp)
	struct mount *mntp;
	register nfsfh_t *fhp;
	int fhsize;
	struct nfsnode **npp;
{
	struct proc *p = curproc;	/* XXX */
	struct nfsnode *np;
	struct nfsnodehashhead *nhpp;
	register struct vnode *vp;
	struct vnode *nvp;
	int error;

	nhpp = NFSNOHASH(nfs_hash(fhp, fhsize));
loop:
	for (np = nhpp->lh_first; np != 0; np = np->n_hash.le_next) {
		if (mntp != NFSTOV(np)->v_mount || np->n_fhsize != fhsize ||
		    bcmp((caddr_t)fhp, (caddr_t)np->n_fhp, fhsize))
			continue;
		vp = NFSTOV(np);
		if (vget(vp, LK_EXCLUSIVE, p))
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
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(np, struct nfsnode *, sizeof *np, M_NFSNODE, M_WAITOK);
		
	error = getnewvnode(VT_NFS, mntp, nfsv2_vnodeop_p, &nvp);
	if (error) {
		if (nfs_node_hash_lock < 0)
			wakeup(&nfs_node_hash_lock);
		nfs_node_hash_lock = 0;
		*npp = 0;
		FREE(np, M_NFSNODE);
		return (error);
	}
	vp = nvp;
	bzero((caddr_t)np, sizeof *np);
	vp->v_data = np;
	np->n_vnode = vp;
	/*
	 * Insert the nfsnode in the hash queue for its new file handle
	 */
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	if (fhsize > NFS_SMALLFH) {
		MALLOC(np->n_fhp, nfsfh_t *, fhsize, M_NFSBIGFH, M_WAITOK);
	} else
		np->n_fhp = &np->n_fh;
	bcopy((caddr_t)fhp, (caddr_t)np->n_fhp, fhsize);
	np->n_fhsize = fhsize;
	*npp = np;

	if (nfs_node_hash_lock < 0)
		wakeup(&nfs_node_hash_lock);
	nfs_node_hash_lock = 0;

	/*
	 * Lock the new nfsnode.
	 */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);

	return (0);
}

int
nfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	register struct nfsnode *np;
	register struct sillyrename *sp;
	struct proc *p = curproc;	/* XXX */

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
		 * Remove the silly file that was rename'd earlier
		 */
		(void) nfs_vinvalbuf(ap->a_vp, 0, sp->s_cred, p, 1);
		nfs_removeit(sp);
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
		FREE((caddr_t)sp, M_NFSREQ);
	}
	np->n_flag &= (NMODIFIED | NFLUSHINPROG | NFLUSHWANT | NQNFSEVICTED |
		NQNFSNONCACHE | NQNFSWRITE);
	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
int
nfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	register struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	register struct nfsdmap *dp, *dp2;

	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", vp);

	LIST_REMOVE(np, n_hash);

	/*
	 * For nqnfs, take it off the timer queue as required.
	 */
	if ((nmp->nm_flag & NFSMNT_NQNFS) && np->n_timer.cqe_next != 0) {
		CIRCLEQ_REMOVE(&nmp->nm_timerhead, np, n_timer);
	}

	/*
	 * Free up any directory cookie structures and
	 * large file handle structures that might be associated with
	 * this nfs node.
	 */
	if (vp->v_type == VDIR) {
		dp = np->n_cookies.lh_first;
		while (dp) {
			dp2 = dp;
			dp = dp->ndm_list.le_next;
			FREE((caddr_t)dp2, M_NFSDIROFF);
		}
	}
	if (np->n_fhsize > NFS_SMALLFH) {
		FREE((caddr_t)np->n_fhp, M_NFSBIGFH);
	}

	cache_purge(vp);
	FREE(vp->v_data, M_NFSNODE);
	vp->v_data = (void *)0;
	return (0);
}

#if 0
/*
 * Lock an nfsnode
 */
int
nfs_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;

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
nfs_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap;
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
nfs_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	return VTONFS(ap->a_vp)->n_flag & NLOCKED ? 1 : 0;
}
#endif

/*
 * Nfs abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. Currently nothing to do.
 */
/* ARGSUSED */
int
nfs_abortop(ap)
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap;
{

	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ap->a_cnp->cn_pnbuf, M_NAMEI);
	return (0);
}
