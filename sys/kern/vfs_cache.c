/*
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Poul-Henning Kamp of the FreeBSD Project.
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
 *	@(#)vfs_cache.c	8.5 (Berkeley) 3/22/95
 * $Id: vfs_cache.c,v 1.30 1997/09/03 09:20:17 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>


/*
 * Name caching works as follows:
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where vp refers to the directory
 * containing name.
 *
 * If it is a "negative" entry, (i.e. for a name that is known NOT to
 * exist) the vnode pointer will be NULL.
 *
 * Upon reaching the last segment of a path, if the reference
 * is for DELETE, or NOCACHE is set (rewrite), and the
 * name is located in the cache, it will be dropped.
 */

/*
 * Structures associated with name cacheing.
 */
#define NCHHASH(dvp, cnp) \
	(&nchashtbl[((dvp)->v_id + (cnp)->cn_hash) & nchash])
static LIST_HEAD(nchashhead, namecache) *nchashtbl;	/* Hash Table */
static TAILQ_HEAD(, namecache) ncneg;	/* Hash Table */
static u_long	nchash;			/* size of hash table */
SYSCTL_INT(_debug, OID_AUTO, nchash, CTLFLAG_RD, &nchash, 0, "");
static u_long	ncnegfactor = 16;	/* ratio of negative entries */
SYSCTL_INT(_debug, OID_AUTO, ncnegfactor, CTLFLAG_RW, &ncnegfactor, 0, "");
static u_long	numneg;		/* number of cache entries allocated */
SYSCTL_INT(_debug, OID_AUTO, numneg, CTLFLAG_RD, &numneg, 0, "");
static u_long	numcache;		/* number of cache entries allocated */
SYSCTL_INT(_debug, OID_AUTO, numcache, CTLFLAG_RD, &numcache, 0, "");
struct	nchstats nchstats;		/* cache effectiveness statistics */

static int	doingcache = 1;		/* 1 => enable the cache */
SYSCTL_INT(_debug, OID_AUTO, vfscache, CTLFLAG_RW, &doingcache, 0, "");
SYSCTL_INT(_debug, OID_AUTO, vnsize, CTLFLAG_RD, 0, sizeof(struct vnode), "");
SYSCTL_INT(_debug, OID_AUTO, ncsize, CTLFLAG_RD, 0, sizeof(struct namecache), "");

static void cache_zap __P((struct namecache *ncp));

/*
 * Flags in namecache.nc_flag
 */
#define NCF_WHITE	1
/*
 * Delete an entry from its hash list and move it to the front
 * of the LRU list for immediate reuse.
 */
static void
cache_zap(ncp)
	struct namecache *ncp;
{
	LIST_REMOVE(ncp, nc_hash);
	LIST_REMOVE(ncp, nc_src);
	if (LIST_EMPTY(&ncp->nc_dvp->v_cache_src)) 
		vdrop(ncp->nc_dvp);
	if (ncp->nc_vp) {
		TAILQ_REMOVE(&ncp->nc_vp->v_cache_dst, ncp, nc_dst);
	} else {
		TAILQ_REMOVE(&ncneg, ncp, nc_dst);
		numneg--;
	}
	numcache--;
	free(ncp, M_CACHE);
}

/*
 * Lookup an entry in the cache
 *
 * We don't do this if the segment name is long, simply so the cache
 * can avoid holding long names (which would either waste space, or
 * add greatly to the complexity).
 *
 * Lookup is called with dvp pointing to the directory to search,
 * cnp pointing to the name of the entry being sought. If the lookup
 * succeeds, the vnode is returned in *vpp, and a status of -1 is
 * returned. If the lookup determines that the name does not exist
 * (negative cacheing), a status of ENOENT is returned. If the lookup
 * fails, a status of zero is returned.
 */

int
cache_lookup(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	register struct namecache *ncp, *nnp;
	register struct nchashhead *ncpp;

	if (!doingcache) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}

	if (cnp->cn_nameptr[0] == '.') {
		if (cnp->cn_namelen == 1) {
			*vpp = dvp;
			return (-1);
		}
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			if (dvp->v_dd->v_id != dvp->v_ddid ||
			    (cnp->cn_flags & MAKEENTRY) == 0) {
				dvp->v_ddid = 0;
				return (0);
			}
			*vpp = dvp->v_dd;
			return (-1);
		}
	}

	LIST_FOREACH(ncp, (NCHHASH(dvp, cnp)), nc_hash) {
		if (ncp->nc_dvp == dvp && ncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(ncp->nc_name, cnp->cn_nameptr, (u_int)ncp->nc_nlen))
			break;
	}

	/* We failed to find an entry */
	if (ncp == 0) {
		nchstats.ncs_miss++;
		return (0);
	}

	/* We don't want to have an entry, so dump it */
	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		nchstats.ncs_badhits++;
		cache_zap(ncp);
		return (0);
	}

	/* We found a "positive" match, return the vnode */
        if (ncp->nc_vp) {
		nchstats.ncs_goodhits++;
		*vpp = ncp->nc_vp;
		return (-1);
	}

	/* We found a negative match, and want to create it, so purge */
	if (cnp->cn_nameiop == CREATE) {
		nchstats.ncs_badhits++;
		cache_zap(ncp);
		return (0);
	}

	/*
	 * We found a "negative" match, ENOENT notifies client of this match.
	 * The nc_vpid field records whether this is a whiteout.
	 */
	TAILQ_REMOVE(&ncneg, ncp, nc_dst);
	TAILQ_INSERT_TAIL(&ncneg, ncp, nc_dst);
	nchstats.ncs_neghits++;
	if (ncp->nc_flag & NCF_WHITE)
		cnp->cn_flags |= ISWHITEOUT;
	return (ENOENT);
}

/*
 * Add an entry to the cache.
 */
void
cache_enter(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	register struct namecache *ncp;
	register struct nchashhead *ncpp;

	if (!doingcache)
		return;

	if (cnp->cn_nameptr[0] == '.') {
		if (cnp->cn_namelen == 1) {
			return;
		}
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			if (vp) {
				dvp->v_dd = vp;
				dvp->v_ddid = vp->v_id;
			} else {
				dvp->v_dd = dvp;
				dvp->v_ddid = 0;
			}
			return;
		}
	}
	 
	ncp = (struct namecache *)
		malloc(sizeof *ncp + cnp->cn_namelen, M_CACHE, M_WAITOK);
	bzero((char *)ncp, sizeof *ncp);
	numcache++;
	if (!vp) {
		numneg++;
		ncp->nc_flag = cnp->cn_flags & ISWHITEOUT ? NCF_WHITE : 0;
	} else if (vp->v_type == VDIR) {
		vp->v_dd = dvp;
		vp->v_ddid = dvp->v_id;
	}

	/*
	 * Fill in cache info, if vp is NULL this is a "negative" cache entry.
	 * For negative entries, we have to record whether it is a whiteout.
	 * the whiteout flag is stored in the nc_vpid field which is
	 * otherwise unused.
	 */
	ncp->nc_vp = vp;
	ncp->nc_dvp = dvp;
	ncp->nc_nlen = cnp->cn_namelen;
	bcopy(cnp->cn_nameptr, ncp->nc_name, (unsigned)ncp->nc_nlen);
	ncpp = NCHHASH(dvp, cnp);
	LIST_INSERT_HEAD(ncpp, ncp, nc_hash);
	if (LIST_EMPTY(&dvp->v_cache_src))
		vhold(dvp);
	LIST_INSERT_HEAD(&dvp->v_cache_src, ncp, nc_src);
	if (vp) {
		TAILQ_INSERT_HEAD(&vp->v_cache_dst, ncp, nc_dst);
	} else {
		TAILQ_INSERT_TAIL(&ncneg, ncp, nc_dst);
	}
	if (numneg*ncnegfactor > numcache) {
		ncp = TAILQ_FIRST(&ncneg);
		cache_zap(ncp);
	}
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */
void
nchinit()
{

	TAILQ_INIT(&ncneg);
	nchashtbl = hashinit(desiredvnodes*2, M_CACHE, &nchash);
}

/*
 * Invalidate all entries to particular vnode.
 *
 * We actually just increment the v_id, that will do it. The stale entries
 * will be purged by lookup as they get found. If the v_id wraps around, we
 * need to ditch the entire cache, to avoid confusion. No valid vnode will
 * ever have (v_id == 0).
 */
void
cache_purge(vp)
	struct vnode *vp;
{
	struct namecache *ncp;
	struct nchashhead *ncpp;
	static u_long nextid;

	while (!LIST_EMPTY(&vp->v_cache_src)) 
		cache_zap(LIST_FIRST(&vp->v_cache_src));
	while (!TAILQ_EMPTY(&vp->v_cache_dst)) 
		cache_zap(TAILQ_FIRST(&vp->v_cache_dst));

	nextid++;
	while (nextid == vp->v_id || !nextid)
		continue;
	vp->v_id = nextid;
	vp->v_dd = vp;
	vp->v_ddid = 0;
}

/*
 * Flush all entries referencing a particular filesystem.
 *
 * Since we need to check it anyway, we will flush all the invalid
 * entries at the same time.
 */
void
cache_purgevfs(mp)
	struct mount *mp;
{
	struct nchashhead *ncpp;
	struct namecache *ncp, *nnp;

	/* Scan hash tables for applicable entries */
	for (ncpp = &nchashtbl[nchash]; ncpp >= nchashtbl; ncpp--) {
		for (ncp = LIST_FIRST(ncpp); ncp != 0; ncp = nnp) {
			nnp = LIST_NEXT(ncp, nc_hash);
			if (ncp->nc_dvp->v_mount == mp) {
				cache_zap(ncp);
			}
		}
	}
}

/*
 * Perform canonical checks and cache lookup and pass on to filesystem
 * through the vop_cachedlookup only if needed.
 */

int
vfs_cache_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vdp;
	struct vnode *pdp;
	int lockparent;	
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	struct proc *p = cnp->cn_proc;
	u_long vpid;	/* capability number of vnode */

	*vpp = NULL;
	vdp = ap->a_dvp;
	lockparent = flags & LOCKPARENT;

	if (vdp->v_type != VDIR)
                return (ENOTDIR);

	if ((flags & ISLASTCN) && (vdp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	error = VOP_ACCESS(vdp, VEXEC, cred, cnp->cn_proc);

	if (error)
		return (error);

	error = cache_lookup(vdp, vpp, cnp);

	if (!error) 
		return (VCALL(vdp, VOFFSET(vop_cachedlookup), 
		    (struct vop_cachedlookup_args *)ap));

	if (error == ENOENT)
		return (error);

	pdp = vdp;
	vdp = *vpp;
	vpid = vdp->v_id;
	if (pdp == vdp) {   /* lookup on "." */
		VREF(vdp);
		error = 0;
	} else if (flags & ISDOTDOT) {
		VOP_UNLOCK(pdp, 0, p);
		error = vget(vdp, LK_EXCLUSIVE, p);
		if (!error && lockparent && (flags & ISLASTCN))
			error = vn_lock(pdp, LK_EXCLUSIVE, p);
	} else {
		error = vget(vdp, LK_EXCLUSIVE, p);
		if (!lockparent || error || !(flags & ISLASTCN))
			VOP_UNLOCK(pdp, 0, p);
	}
	/*
	 * Check that the capability number did not change
	 * while we were waiting for the lock.
	 */
	if (!error) {
		if (vpid == vdp->v_id)
			return (0);
		vput(vdp);
		if (lockparent && pdp != vdp && (flags & ISLASTCN))
			VOP_UNLOCK(pdp, 0, p);
	}
	error = vn_lock(pdp, LK_EXCLUSIVE, p);
	if (error)
		return (error);
	return (VCALL(vdp, VOFFSET(vop_cachedlookup), 
	    (struct vop_cachedlookup_args *)ap));
}
