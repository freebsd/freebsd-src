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
 *	@(#)nfs_node.c	8.2 (Berkeley) 12/30/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/nqnfs.h>

struct nfsnode **nheadhashtbl;
u_long nheadhash;
#define	NFSNOHASH(fhsum)	((fhsum)&nheadhash)

#define TRUE	1
#define	FALSE	0

/*
 * Initialize hash links for nfsnodes
 * and build nfsnode free list.
 */
nfs_nhinit()
{

#ifndef lint
	if ((sizeof(struct nfsnode) - 1) & sizeof(struct nfsnode))
		printf("nfs_nhinit: bad size %d\n", sizeof(struct nfsnode));
#endif /* not lint */
	nheadhashtbl = hashinit(desiredvnodes, M_NFSNODE, &nheadhash);
}

/*
 * Compute an entry in the NFS hash table structure
 */
struct nfsnode **
nfs_hash(fhp)
	register nfsv2fh_t *fhp;
{
	register u_char *fhpp;
	register u_long fhsum;
	int i;

	fhpp = &fhp->fh_bytes[0];
	fhsum = 0;
	for (i = 0; i < NFSX_FH; i++)
		fhsum += *fhpp++;
	return (&nheadhashtbl[NFSNOHASH(fhsum)]);
}

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
nfs_nget(mntp, fhp, npp)
	struct mount *mntp;
	register nfsv2fh_t *fhp;
	struct nfsnode **npp;
{
	register struct nfsnode *np, *nq, **nhpp;
	register struct vnode *vp;
	extern int (**nfsv2_vnodeop_p)();
	struct vnode *nvp;
	int error;

	nhpp = nfs_hash(fhp);
loop:
	for (np = *nhpp; np; np = np->n_forw) {
		if (mntp != NFSTOV(np)->v_mount ||
		    bcmp((caddr_t)fhp, (caddr_t)&np->n_fh, NFSX_FH))
			continue;
		vp = NFSTOV(np);
		if (vget(vp, 1))
			goto loop;
		*npp = np;
		return(0);
	}
	if (error = getnewvnode(VT_NFS, mntp, nfsv2_vnodeop_p, &nvp)) {
		*npp = 0;
		return (error);
	}
	vp = nvp;
	MALLOC(np, struct nfsnode *, sizeof *np, M_NFSNODE, M_WAITOK);
	vp->v_data = np;
	np->n_vnode = vp;
	/*
	 * Insert the nfsnode in the hash queue for its new file handle
	 */
	np->n_flag = 0;
	if (nq = *nhpp)
		nq->n_back = &np->n_forw;
	np->n_forw = nq;
	np->n_back = nhpp;
	*nhpp = np;
	bcopy((caddr_t)fhp, (caddr_t)&np->n_fh, NFSX_FH);
	np->n_attrstamp = 0;
	np->n_direofoffset = 0;
	np->n_sillyrename = (struct sillyrename *)0;
	np->n_size = 0;
	np->n_mtime = 0;
	if (VFSTONFS(mntp)->nm_flag & NFSMNT_NQNFS) {
		np->n_brev = 0;
		np->n_lrev = 0;
		np->n_expiry = (time_t)0;
		np->n_tnext = (struct nfsnode *)0;
	}
	*npp = np;
	return (0);
}

nfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct nfsnode *np;
	register struct sillyrename *sp;
	struct proc *p = curproc;	/* XXX */
	extern int prtactive;

	np = VTONFS(ap->a_vp);
	if (prtactive && ap->a_vp->v_usecount != 0)
		vprint("nfs_inactive: pushing active", ap->a_vp);
	sp = np->n_sillyrename;
	np->n_sillyrename = (struct sillyrename *)0;
	if (sp) {
		/*
		 * Remove the silly file that was rename'd earlier
		 */
		(void) nfs_vinvalbuf(ap->a_vp, 0, sp->s_cred, p, 1);
		nfs_removeit(sp);
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
#ifdef SILLYSEPARATE
		free((caddr_t)sp, M_NFSREQ);
#endif
	}
	np->n_flag &= (NMODIFIED | NFLUSHINPROG | NFLUSHWANT | NQNFSEVICTED |
		NQNFSNONCACHE | NQNFSWRITE);
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
nfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	register struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	register struct nfsnode *nq;
	extern int prtactive;

	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", vp);
	/*
	 * Remove the nfsnode from its hash chain.
	 */
	if (nq = np->n_forw)
		nq->n_back = np->n_back;
	*np->n_back = nq;

	/*
	 * For nqnfs, take it off the timer queue as required.
	 */
	if ((nmp->nm_flag & NFSMNT_NQNFS) && np->n_tnext) {
		if (np->n_tnext == (struct nfsnode *)nmp)
			nmp->nm_tprev = np->n_tprev;
		else
			np->n_tnext->n_tprev = np->n_tprev;
		if (np->n_tprev == (struct nfsnode *)nmp)
			nmp->nm_tnext = np->n_tnext;
		else
			np->n_tprev->n_tnext = np->n_tnext;
	}
	cache_purge(vp);
	FREE(vp->v_data, M_NFSNODE);
	vp->v_data = (void *)0;
	return (0);
}

/*
 * Lock an nfsnode
 */
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
		sleep((caddr_t)vp, PINOD);
	}
	if (vp->v_tag == VT_NON)
		return (ENOENT);
	return (0);
}

/*
 * Unlock an nfsnode
 */
nfs_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return (0);
}

/*
 * Check for a locked nfsnode
 */
nfs_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return (0);
}

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
