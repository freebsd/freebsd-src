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
 *	From:	@(#)nfs_node.c	7.34 (Berkeley) 5/15/91
 *	$Id: nfs_node.c,v 1.4 1994/01/31 23:40:48 martin Exp $
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "mount.h"
#include "namei.h"
#include "vnode.h"
#include "kernel.h"
#include "malloc.h"

#include "nfsv2.h"
#include "nfs.h"
#include "nfsnode.h"
#include "nfsmount.h"

/* The request list head */
extern struct nfsreq nfsreqh;

#define	NFSNOHSZ	512
#if	((NFSNOHSZ&(NFSNOHSZ-1)) == 0)
#define	NFSNOHASH(fhsum)	((fhsum)&(NFSNOHSZ-1))
#else
#define	NFSNOHASH(fhsum)	(((unsigned)(fhsum))%NFSNOHSZ)
#endif

union nhead {
	union  nhead *nh_head[2];
	struct nfsnode *nh_chain[2];
} nhead[NFSNOHSZ];

#define TRUE	1
#define	FALSE	0

/*
 * Initialize hash links for nfsnodes
 * and build nfsnode free list.
 */
void
nfs_nhinit()
{
	register int i;
	register union  nhead *nh = nhead;

#ifndef lint
	if (VN_MAXPRIVATE < sizeof(struct nfsnode))
		panic("nfs_nhinit: too small");
#endif /* not lint */
	for (i = NFSNOHSZ; --i >= 0; nh++) {
		nh->nh_head[0] = nh;
		nh->nh_head[1] = nh;
	}
}

/*
 * Compute an entry in the NFS hash table structure
 */
union nhead *
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
	return (&nhead[NFSNOHASH(fhsum)]);
}

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
int
nfs_nget(mntp, fhp, npp)
	struct mount *mntp;
	register nfsv2fh_t *fhp;
	struct nfsnode **npp;
{
	register struct nfsnode *np;
	register struct vnode *vp;
	extern struct vnodeops nfsv2_vnodeops;
	struct vnode *nvp;
	union nhead *nh;
	int error;

	nh = nfs_hash(fhp);
loop:
	for (np = nh->nh_chain[0]; np != (struct nfsnode *)nh; np = np->n_forw) {
		if (mntp != NFSTOV(np)->v_mount ||
		    bcmp((caddr_t)fhp, (caddr_t)&np->n_fh, NFSX_FH))
			continue;
		if ((np->n_flag & NLOCKED) != 0) {
			np->n_flag |= NWANT;
			(void) tsleep((caddr_t)np, PINOD, "nfsnode", 0);
			goto loop;
		}
		vp = NFSTOV(np);
		if (vget(vp))
			goto loop;
		*npp = np;
		return(0);
	}
	if (error = getnewvnode(VT_NFS, mntp, &nfsv2_vnodeops, &nvp)) {
		*npp = 0;
		return (error);
	}
	vp = nvp;
	np = VTONFS(vp);
	np->n_vnode = vp;
	/*
	 * Insert the nfsnode in the hash queue for its new file handle
	 */
	np->n_flag = 0;
	insque(np, nh);
	nfs_lock(vp);
	bcopy((caddr_t)fhp, (caddr_t)&np->n_fh, NFSX_FH);
	np->n_attrstamp = 0;
	np->n_direofoffset = 0;
	np->n_sillyrename = (struct sillyrename *)0;
	np->n_size = 0;
	np->n_mtime = 0;
	np->n_lockf = 0;
	*npp = np;
	return (0);
}

int
nfs_inactive(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	register struct nfsnode *np;
	register struct sillyrename *sp;
	struct nfsnode *dnp;
	extern int prtactive;

	np = VTONFS(vp);
	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_inactive: pushing active", vp);
	nfs_lock(vp);
	sp = np->n_sillyrename;
	np->n_sillyrename = (struct sillyrename *)0;
	if (sp) {
		/*
		 * Remove the silly file that was rename'd earlier
		 */
		if (!nfs_nget(vp->v_mount, &sp->s_fh, &dnp)) {
			sp->s_dvp = NFSTOV(dnp);
			nfs_removeit(sp, p);
			nfs_nput(sp->s_dvp);
		}
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
		free((caddr_t)sp, M_NFSREQ);
	}
	nfs_unlock(vp);
	np->n_flag &= NMODIFIED;
#ifdef notdef
	/*
	 * Scan the request list for any requests left hanging about
	 */
	s = splnet();
	rep = nfsreqh.r_next;
	while (rep && rep != &nfsreqh) {
		if (rep->r_vp == vp) {
			rep->r_prev->r_next = rep2 = rep->r_next;
			rep->r_next->r_prev = rep->r_prev;
			m_freem(rep->r_mreq);
			if (rep->r_mrep != NULL)
				m_freem(rep->r_mrep);
			free((caddr_t)rep, M_NFSREQ);
			rep = rep2;
		} else
			rep = rep->r_next;
	}
	splx(s);
#endif
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
int
nfs_reclaim(vp)
	register struct vnode *vp;
{
	register struct nfsnode *np = VTONFS(vp);
	extern int prtactive;

	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", vp);
	/*
	 * Remove the nfsnode from its hash chain.
	 */
	remque(np);
	np->n_forw = np;
	np->n_back = np;
	cache_purge(vp);
	np->n_flag = 0;
	np->n_direofoffset = 0;
	return (0);
}

/*
 * In theory, NFS does not need locking, but we make provision
 * for doing it just in case it is needed.
 */
int donfslocking = 0;

/*
 * Lock an nfsnode
 */
int
nfs_lock(vp)
	struct vnode *vp;
{
	register struct nfsnode *np = VTONFS(vp);

	if (!donfslocking)
		return 0;
	while (np->n_flag & NLOCKED) {
		np->n_flag |= NWANT;
		if (np->n_lockholder == curproc->p_pid)
			panic("locking against myself");
		np->n_lockwaiter = curproc->p_pid;
		(void) tsleep((caddr_t)np, PINOD, "nfslock", 0);
	}
	np->n_lockwaiter = 0;
	np->n_lockholder = curproc->p_pid;
	np->n_flag |= NLOCKED;
	return(0);
}

/*
 * Unlock an nfsnode
 */
int
nfs_unlock(vp)
	struct vnode *vp;
{
	register struct nfsnode *np = VTONFS(vp);

	np->n_lockholder = 0;
	np->n_flag &= ~NLOCKED;
	if (np->n_flag & NWANT) {
		np->n_flag &= ~NWANT;
		wakeup((caddr_t)np);
	}
	return(0);
}

/*
 * Check for a locked nfsnode
 */
int
nfs_islocked(vp)
	struct vnode *vp;
{

	if (VTONFS(vp)->n_flag & NLOCKED)
		return (1);
	return (0);
}

/*
 * Unlock and vrele()
 * since I can't decide if dirs. should be locked, I will check for
 * the lock and be flexible
 */
void
nfs_nput(vp)
	struct vnode *vp;
{
	register struct nfsnode *np = VTONFS(vp);

	if (np->n_flag & NLOCKED)
		nfs_unlock(vp);
	vrele(vp);
}

/*
 * Nfs abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. Currently nothing to do.
 */
/* ARGSUSED */
int
nfs_abortop(ndp)
	struct nameidata *ndp;
{

	if ((ndp->ni_nameiop & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ndp->ni_pnbuf, M_NAMEI);
	return (0);
}
