/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/vfs_cache.c,v 1.114.2.2.2.2 2008/12/09 17:33:56 kib Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/fnv_hash.h>

#include <vm/uma.h>

/*
 * This structure describes the elements in the cache of recent
 * names looked up by namei.
 */

struct	namecache {
	LIST_ENTRY(namecache) nc_hash;	/* hash chain */
	LIST_ENTRY(namecache) nc_src;	/* source vnode list */
	TAILQ_ENTRY(namecache) nc_dst;	/* destination vnode list */
	struct	vnode *nc_dvp;		/* vnode of parent of name */
	struct	vnode *nc_vp;		/* vnode the name refers to */
	u_char	nc_flag;		/* flag bits */
	u_char	nc_nlen;		/* length of name */
	char	nc_name[0];		/* segment name */
};

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
#define NCHHASH(hash) \
	(&nchashtbl[(hash) & nchash])
static LIST_HEAD(nchashhead, namecache) *nchashtbl;	/* Hash Table */
static TAILQ_HEAD(, namecache) ncneg;	/* Hash Table */
static u_long	nchash;			/* size of hash table */
SYSCTL_ULONG(_debug, OID_AUTO, nchash, CTLFLAG_RD, &nchash, 0, "");
static u_long	ncnegfactor = 16;	/* ratio of negative entries */
SYSCTL_ULONG(_debug, OID_AUTO, ncnegfactor, CTLFLAG_RW, &ncnegfactor, 0, "");
static u_long	numneg;			/* number of cache entries allocated */
SYSCTL_ULONG(_debug, OID_AUTO, numneg, CTLFLAG_RD, &numneg, 0, "");
static u_long	numcache;		/* number of cache entries allocated */
SYSCTL_ULONG(_debug, OID_AUTO, numcache, CTLFLAG_RD, &numcache, 0, "");
static u_long	numcachehv;		/* number of cache entries with vnodes held */
SYSCTL_ULONG(_debug, OID_AUTO, numcachehv, CTLFLAG_RD, &numcachehv, 0, "");
#if 0
static u_long	numcachepl;		/* number of cache purge for leaf entries */
SYSCTL_ULONG(_debug, OID_AUTO, numcachepl, CTLFLAG_RD, &numcachepl, 0, "");
#endif
struct	nchstats nchstats;		/* cache effectiveness statistics */

static struct mtx cache_lock;
MTX_SYSINIT(vfscache, &cache_lock, "Name Cache", MTX_DEF);

#define	CACHE_LOCK()	mtx_lock(&cache_lock)
#define	CACHE_UNLOCK()	mtx_unlock(&cache_lock)

/*
 * UMA zones for the VFS cache.
 *
 * The small cache is used for entries with short names, which are the
 * most common.  The large cache is used for entries which are too big to
 * fit in the small cache.
 */
static uma_zone_t cache_zone_small;
static uma_zone_t cache_zone_large;

#define	CACHE_PATH_CUTOFF	32
#define	CACHE_ZONE_SMALL	(sizeof(struct namecache) + CACHE_PATH_CUTOFF)
#define	CACHE_ZONE_LARGE	(sizeof(struct namecache) + NAME_MAX)

#define cache_alloc(len)	uma_zalloc(((len) <= CACHE_PATH_CUTOFF) ? \
	cache_zone_small : cache_zone_large, M_WAITOK)
#define cache_free(ncp)		do { \
	if (ncp != NULL) \
		uma_zfree(((ncp)->nc_nlen <= CACHE_PATH_CUTOFF) ? \
		    cache_zone_small : cache_zone_large, (ncp)); \
} while (0)

static int	doingcache = 1;		/* 1 => enable the cache */
SYSCTL_INT(_debug, OID_AUTO, vfscache, CTLFLAG_RW, &doingcache, 0, "");

/* Export size information to userland */
SYSCTL_INT(_debug_sizeof, OID_AUTO, namecache, CTLFLAG_RD, 0,
	sizeof(struct namecache), "");

/*
 * The new name cache statistics
 */
static SYSCTL_NODE(_vfs, OID_AUTO, cache, CTLFLAG_RW, 0, "Name cache statistics");
#define STATNODE(mode, name, var) \
	SYSCTL_ULONG(_vfs_cache, OID_AUTO, name, mode, var, 0, "");
STATNODE(CTLFLAG_RD, numneg, &numneg);
STATNODE(CTLFLAG_RD, numcache, &numcache);
static u_long numcalls; STATNODE(CTLFLAG_RD, numcalls, &numcalls);
static u_long dothits; STATNODE(CTLFLAG_RD, dothits, &dothits);
static u_long dotdothits; STATNODE(CTLFLAG_RD, dotdothits, &dotdothits);
static u_long numchecks; STATNODE(CTLFLAG_RD, numchecks, &numchecks);
static u_long nummiss; STATNODE(CTLFLAG_RD, nummiss, &nummiss);
static u_long nummisszap; STATNODE(CTLFLAG_RD, nummisszap, &nummisszap);
static u_long numposzaps; STATNODE(CTLFLAG_RD, numposzaps, &numposzaps);
static u_long numposhits; STATNODE(CTLFLAG_RD, numposhits, &numposhits);
static u_long numnegzaps; STATNODE(CTLFLAG_RD, numnegzaps, &numnegzaps);
static u_long numneghits; STATNODE(CTLFLAG_RD, numneghits, &numneghits);

SYSCTL_OPAQUE(_vfs_cache, OID_AUTO, nchstats, CTLFLAG_RD, &nchstats,
	sizeof(nchstats), "LU", "VFS cache effectiveness statistics");



static void cache_zap(struct namecache *ncp);
static int vn_fullpath1(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, u_int buflen);

static MALLOC_DEFINE(M_VFSCACHE, "vfscache", "VFS name cache entries");

/*
 * Flags in namecache.nc_flag
 */
#define NCF_WHITE	1

/*
 * Grab an atomic snapshot of the name cache hash chain lengths
 */
SYSCTL_NODE(_debug, OID_AUTO, hashstat, CTLFLAG_RW, NULL, "hash table stats");

static int
sysctl_debug_hashstat_rawnchash(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct nchashhead *ncpp;
	struct namecache *ncp;
	int n_nchash;
	int count;

	n_nchash = nchash + 1;	/* nchash is max index, not count */
	if (!req->oldptr)
		return SYSCTL_OUT(req, 0, n_nchash * sizeof(int));

	/* Scan hash tables for applicable entries */
	for (ncpp = nchashtbl; n_nchash > 0; n_nchash--, ncpp++) {
		count = 0;
		LIST_FOREACH(ncp, ncpp, nc_hash) {
			count++;
		}
		error = SYSCTL_OUT(req, &count, sizeof(count));
		if (error)
			return (error);
	}
	return (0);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, rawnchash, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_debug_hashstat_rawnchash, "S,int", "nchash chain lengths");

static int
sysctl_debug_hashstat_nchash(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct nchashhead *ncpp;
	struct namecache *ncp;
	int n_nchash;
	int count, maxlength, used, pct;

	if (!req->oldptr)
		return SYSCTL_OUT(req, 0, 4 * sizeof(int));

	n_nchash = nchash + 1;	/* nchash is max index, not count */
	used = 0;
	maxlength = 0;

	/* Scan hash tables for applicable entries */
	for (ncpp = nchashtbl; n_nchash > 0; n_nchash--, ncpp++) {
		count = 0;
		LIST_FOREACH(ncp, ncpp, nc_hash) {
			count++;
		}
		if (count)
			used++;
		if (maxlength < count)
			maxlength = count;
	}
	n_nchash = nchash + 1;
	pct = (used * 100 * 100) / n_nchash;
	error = SYSCTL_OUT(req, &n_nchash, sizeof(n_nchash));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &used, sizeof(used));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &maxlength, sizeof(maxlength));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &pct, sizeof(pct));
	if (error)
		return (error);
	return (0);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, nchash, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_debug_hashstat_nchash, "I", "nchash chain lengths");

/*
 * cache_zap():
 *
 *   Removes a namecache entry from cache, whether it contains an actual
 *   pointer to a vnode or if it is just a negative cache entry.
 */
static void
cache_zap(ncp)
	struct namecache *ncp;
{
	struct vnode *vp;

	mtx_assert(&cache_lock, MA_OWNED);
	CTR2(KTR_VFS, "cache_zap(%p) vp %p", ncp, ncp->nc_vp);
	vp = NULL;
	LIST_REMOVE(ncp, nc_hash);
	LIST_REMOVE(ncp, nc_src);
	if (LIST_EMPTY(&ncp->nc_dvp->v_cache_src)) {
		vp = ncp->nc_dvp;
		numcachehv--;
	}
	if (ncp->nc_vp) {
		TAILQ_REMOVE(&ncp->nc_vp->v_cache_dst, ncp, nc_dst);
		ncp->nc_vp->v_dd = NULL;
	} else {
		TAILQ_REMOVE(&ncneg, ncp, nc_dst);
		numneg--;
	}
	numcache--;
	cache_free(ncp);
	if (vp)
		vdrop(vp);
}

/*
 * Lookup an entry in the cache
 *
 * Lookup is called with dvp pointing to the directory to search,
 * cnp pointing to the name of the entry being sought. If the lookup
 * succeeds, the vnode is returned in *vpp, and a status of -1 is
 * returned. If the lookup determines that the name does not exist
 * (negative cacheing), a status of ENOENT is returned. If the lookup
 * fails, a status of zero is returned.
 *
 * vpp is locked and ref'd on return.  If we're looking up DOTDOT, dvp is
 * unlocked.  If we're looking up . an extra ref is taken, but the lock is
 * not recursively acquired.
 */

int
cache_lookup(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	struct namecache *ncp;
	struct thread *td;
	u_int32_t hash;
	int error, ltype;

	if (!doingcache) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}
	td = cnp->cn_thread;
retry:
	CACHE_LOCK();
	numcalls++;

	if (cnp->cn_nameptr[0] == '.') {
		if (cnp->cn_namelen == 1) {
			*vpp = dvp;
			CTR2(KTR_VFS, "cache_lookup(%p, %s) found via .",
			    dvp, cnp->cn_nameptr);
			dothits++;
			goto success;
		}
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			dotdothits++;
			if (dvp->v_dd == NULL ||
			    (cnp->cn_flags & MAKEENTRY) == 0) {
				CACHE_UNLOCK();
				return (0);
			}
			*vpp = dvp->v_dd;
			CTR3(KTR_VFS, "cache_lookup(%p, %s) found %p via ..",
			    dvp, cnp->cn_nameptr, *vpp);
			goto success;
		}
	}

	hash = fnv_32_buf(cnp->cn_nameptr, cnp->cn_namelen, FNV1_32_INIT);
	hash = fnv_32_buf(&dvp, sizeof(dvp), hash);
	LIST_FOREACH(ncp, (NCHHASH(hash)), nc_hash) {
		numchecks++;
		if (ncp->nc_dvp == dvp && ncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(ncp->nc_name, cnp->cn_nameptr, ncp->nc_nlen))
			break;
	}

	/* We failed to find an entry */
	if (ncp == 0) {
		if ((cnp->cn_flags & MAKEENTRY) == 0) {
			nummisszap++;
		} else {
			nummiss++;
		}
		nchstats.ncs_miss++;
		CACHE_UNLOCK();
		return (0);
	}

	/* We don't want to have an entry, so dump it */
	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		numposzaps++;
		nchstats.ncs_badhits++;
		cache_zap(ncp);
		CACHE_UNLOCK();
		return (0);
	}

	/* We found a "positive" match, return the vnode */
	if (ncp->nc_vp) {
		numposhits++;
		nchstats.ncs_goodhits++;
		*vpp = ncp->nc_vp;
		CTR4(KTR_VFS, "cache_lookup(%p, %s) found %p via ncp %p",
		    dvp, cnp->cn_nameptr, *vpp, ncp);
		goto success;
	}

	/* We found a negative match, and want to create it, so purge */
	if (cnp->cn_nameiop == CREATE) {
		numnegzaps++;
		nchstats.ncs_badhits++;
		cache_zap(ncp);
		CACHE_UNLOCK();
		return (0);
	}

	numneghits++;
	/*
	 * We found a "negative" match, so we shift it to the end of
	 * the "negative" cache entries queue to satisfy LRU.  Also,
	 * check to see if the entry is a whiteout; indicate this to
	 * the componentname, if so.
	 */
	TAILQ_REMOVE(&ncneg, ncp, nc_dst);
	TAILQ_INSERT_TAIL(&ncneg, ncp, nc_dst);
	nchstats.ncs_neghits++;
	if (ncp->nc_flag & NCF_WHITE)
		cnp->cn_flags |= ISWHITEOUT;
	CACHE_UNLOCK();
	return (ENOENT);

success:
	/*
	 * On success we return a locked and ref'd vnode as per the lookup
	 * protocol.
	 */
	if (dvp == *vpp) {   /* lookup on "." */
		VREF(*vpp);
		CACHE_UNLOCK();
		/*
		 * When we lookup "." we still can be asked to lock it
		 * differently...
		 */
		ltype = cnp->cn_lkflags & (LK_SHARED | LK_EXCLUSIVE);
		if (ltype == VOP_ISLOCKED(*vpp, td))
			return (-1);
		else if (ltype == LK_EXCLUSIVE)
			vn_lock(*vpp, LK_UPGRADE | LK_RETRY, td);
		return (-1);
	}
	ltype = 0;	/* silence gcc warning */
	if (cnp->cn_flags & ISDOTDOT) {
		ltype = VOP_ISLOCKED(dvp, td);
		VOP_UNLOCK(dvp, 0, td);
	}
	VI_LOCK(*vpp);
	CACHE_UNLOCK();
	error = vget(*vpp, cnp->cn_lkflags | LK_INTERLOCK, td);
	if (cnp->cn_flags & ISDOTDOT)
		vn_lock(dvp, ltype | LK_RETRY, td);
	if ((cnp->cn_flags & ISLASTCN) && (cnp->cn_lkflags & LK_EXCLUSIVE))
		ASSERT_VOP_ELOCKED(*vpp, "cache_lookup");
	if (error) {
		*vpp = NULL;
		goto retry;
	}
	return (-1);
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
	struct namecache *ncp, *n2;
	struct nchashhead *ncpp;
	u_int32_t hash;
	int hold;
	int zap;
	int len;

	CTR3(KTR_VFS, "cache_enter(%p, %p, %s)", dvp, vp, cnp->cn_nameptr);
	VNASSERT(vp == NULL || (vp->v_iflag & VI_DOOMED) == 0, vp,
	    ("cahe_enter: Adding a doomed vnode"));

	if (!doingcache)
		return;

	if (cnp->cn_nameptr[0] == '.') {
		if (cnp->cn_namelen == 1) {
			return;
		}
		/*
		 * For dotdot lookups only cache the v_dd pointer if the
		 * directory has a link back to its parent via v_cache_dst.
		 * Without this an unlinked directory would keep a soft
		 * reference to its parent which could not be NULLd at
		 * cache_purge() time.
		 */
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			CACHE_LOCK();
			if (!TAILQ_EMPTY(&dvp->v_cache_dst))
				dvp->v_dd = vp;
			CACHE_UNLOCK();
			return;
		}
	}

	hold = 0;
	zap = 0;

	/*
	 * Calculate the hash key and setup as much of the new
	 * namecache entry as possible before acquiring the lock.
	 */
	ncp = cache_alloc(cnp->cn_namelen);
	ncp->nc_vp = vp;
	ncp->nc_dvp = dvp;
	len = ncp->nc_nlen = cnp->cn_namelen;
	hash = fnv_32_buf(cnp->cn_nameptr, len, FNV1_32_INIT);
	bcopy(cnp->cn_nameptr, ncp->nc_name, len);
	hash = fnv_32_buf(&dvp, sizeof(dvp), hash);
	CACHE_LOCK();

	/*
	 * See if this vnode is already in the cache with this name.
	 * This can happen with concurrent lookups of the same path
	 * name.
	 */
	if (vp) {
		TAILQ_FOREACH(n2, &vp->v_cache_dst, nc_dst) {
			if (n2->nc_dvp == dvp &&
			    n2->nc_nlen == cnp->cn_namelen &&
			    !bcmp(n2->nc_name, cnp->cn_nameptr, n2->nc_nlen)) {
				CACHE_UNLOCK();
				cache_free(ncp);
				return;
			}
		}
	} else {
		TAILQ_FOREACH(n2, &ncneg, nc_dst) {
			if (n2->nc_nlen == cnp->cn_namelen &&
			    !bcmp(n2->nc_name, cnp->cn_nameptr, n2->nc_nlen)) {
				CACHE_UNLOCK();
				cache_free(ncp);
				return;
			}
		}
	}

	numcache++;
	if (!vp) {
		numneg++;
		ncp->nc_flag = cnp->cn_flags & ISWHITEOUT ? NCF_WHITE : 0;
	} else if (vp->v_type == VDIR) {
		vp->v_dd = dvp;
	} else {
		vp->v_dd = NULL;
	}

	/*
	 * Insert the new namecache entry into the appropriate chain
	 * within the cache entries table.
	 */
	ncpp = NCHHASH(hash);
	LIST_INSERT_HEAD(ncpp, ncp, nc_hash);
	if (LIST_EMPTY(&dvp->v_cache_src)) {
		hold = 1;
		numcachehv++;
	}
	LIST_INSERT_HEAD(&dvp->v_cache_src, ncp, nc_src);
	/*
	 * If the entry is "negative", we place it into the
	 * "negative" cache queue, otherwise, we place it into the
	 * destination vnode's cache entries queue.
	 */
	if (vp) {
		TAILQ_INSERT_HEAD(&vp->v_cache_dst, ncp, nc_dst);
	} else {
		TAILQ_INSERT_TAIL(&ncneg, ncp, nc_dst);
	}
	if (numneg * ncnegfactor > numcache) {
		ncp = TAILQ_FIRST(&ncneg);
		zap = 1;
	}
	if (hold)
		vhold(dvp);
	if (zap)
		cache_zap(ncp);
	CACHE_UNLOCK();
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */
static void
nchinit(void *dummy __unused)
{

	TAILQ_INIT(&ncneg);

	cache_zone_small = uma_zcreate("S VFS Cache", CACHE_ZONE_SMALL, NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	cache_zone_large = uma_zcreate("L VFS Cache", CACHE_ZONE_LARGE, NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_ZINIT);

	nchashtbl = hashinit(desiredvnodes * 2, M_VFSCACHE, &nchash);
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_SECOND, nchinit, NULL);


/*
 * Invalidate all entries to a particular vnode.
 */
void
cache_purge(vp)
	struct vnode *vp;
{

	CTR1(KTR_VFS, "cache_purge(%p)", vp);
	CACHE_LOCK();
	while (!LIST_EMPTY(&vp->v_cache_src))
		cache_zap(LIST_FIRST(&vp->v_cache_src));
	while (!TAILQ_EMPTY(&vp->v_cache_dst))
		cache_zap(TAILQ_FIRST(&vp->v_cache_dst));
	vp->v_dd = NULL;
	CACHE_UNLOCK();
}

/*
 * Flush all entries referencing a particular filesystem.
 */
void
cache_purgevfs(mp)
	struct mount *mp;
{
	struct nchashhead *ncpp;
	struct namecache *ncp, *nnp;

	/* Scan hash tables for applicable entries */
	CACHE_LOCK();
	for (ncpp = &nchashtbl[nchash]; ncpp >= nchashtbl; ncpp--) {
		LIST_FOREACH_SAFE(ncp, ncpp, nc_hash, nnp) {
			if (ncp->nc_dvp->v_mount == mp)
				cache_zap(ncp);
		}
	}
	CACHE_UNLOCK();
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
	struct vnode *dvp;
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	struct thread *td = cnp->cn_thread;

	*vpp = NULL;
	dvp = ap->a_dvp;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	error = VOP_ACCESS(dvp, VEXEC, cred, td);
	if (error)
		return (error);

	error = cache_lookup(dvp, vpp, cnp);
	if (error == 0)
		return (VOP_CACHEDLOOKUP(dvp, vpp, cnp));
	if (error == ENOENT)
		return (error);
	return (0);
}


#ifndef _SYS_SYSPROTO_H_
struct  __getcwd_args {
	u_char	*buf;
	u_int	buflen;
};
#endif

/*
 * XXX All of these sysctls would probably be more productive dead.
 */
static int disablecwd;
SYSCTL_INT(_debug, OID_AUTO, disablecwd, CTLFLAG_RW, &disablecwd, 0,
   "Disable the getcwd syscall");

/* Implementation of the getcwd syscall. */
int
__getcwd(td, uap)
	struct thread *td;
	struct __getcwd_args *uap;
{

	return (kern___getcwd(td, uap->buf, UIO_USERSPACE, uap->buflen));
}

int
kern___getcwd(struct thread *td, u_char *buf, enum uio_seg bufseg, u_int buflen)
{
	char *bp, *tmpbuf;
	struct filedesc *fdp;
	int error;

	if (disablecwd)
		return (ENODEV);
	if (buflen < 2)
		return (EINVAL);
	if (buflen > MAXPATHLEN)
		buflen = MAXPATHLEN;

	tmpbuf = malloc(buflen, M_TEMP, M_WAITOK);
	fdp = td->td_proc->p_fd;
	mtx_lock(&Giant);
	FILEDESC_SLOCK(fdp);
	error = vn_fullpath1(td, fdp->fd_cdir, fdp->fd_rdir, tmpbuf,
	    &bp, buflen);
	FILEDESC_SUNLOCK(fdp);
	mtx_unlock(&Giant);

	if (!error) {
		if (bufseg == UIO_SYSSPACE)
			bcopy(bp, buf, strlen(bp) + 1);
		else
			error = copyout(bp, buf, strlen(bp) + 1);
	}
	free(tmpbuf, M_TEMP);
	return (error);
}

/*
 * Thus begins the fullpath magic.
 */

#undef STATNODE
#define STATNODE(name)							\
	static u_int name;						\
	SYSCTL_UINT(_vfs_cache, OID_AUTO, name, CTLFLAG_RD, &name, 0, "")

static int disablefullpath;
SYSCTL_INT(_debug, OID_AUTO, disablefullpath, CTLFLAG_RW, &disablefullpath, 0,
	"Disable the vn_fullpath function");

/* These count for kern___getcwd(), too. */
STATNODE(numfullpathcalls);
STATNODE(numfullpathfail1);
STATNODE(numfullpathfail2);
STATNODE(numfullpathfail4);
STATNODE(numfullpathfound);

/*
 * Retrieve the full filesystem path that correspond to a vnode from the name
 * cache (if available)
 */
int
vn_fullpath(struct thread *td, struct vnode *vn, char **retbuf, char **freebuf)
{
	char *buf;
	struct filedesc *fdp;
	int error;

	if (disablefullpath)
		return (ENODEV);
	if (vn == NULL)
		return (EINVAL);

	buf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);
	error = vn_fullpath1(td, vn, fdp->fd_rdir, buf, retbuf, MAXPATHLEN);
	FILEDESC_SUNLOCK(fdp);

	if (!error)
		*freebuf = buf;
	else
		free(buf, M_TEMP);
	return (error);
}

/*
 * The magic behind kern___getcwd() and vn_fullpath().
 */
static int
vn_fullpath1(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, u_int buflen)
{
	char *bp;
	int error, i, slash_prefixed;
	struct namecache *ncp;

	bp = buf + buflen - 1;
	*bp = '\0';
	error = 0;
	slash_prefixed = 0;

	CACHE_LOCK();
	numfullpathcalls++;
	if (vp->v_type != VDIR) {
		ncp = TAILQ_FIRST(&vp->v_cache_dst);
		if (!ncp) {
			numfullpathfail2++;
			CACHE_UNLOCK();
			return (ENOENT);
		}
		for (i = ncp->nc_nlen - 1; i >= 0 && bp > buf; i--)
			*--bp = ncp->nc_name[i];
		if (bp == buf) {
			numfullpathfail4++;
			CACHE_UNLOCK();
			return (ENOMEM);
		}
		*--bp = '/';
		slash_prefixed = 1;
		vp = ncp->nc_dvp;
	}
	while (vp != rdir && vp != rootvnode) {
		if (vp->v_vflag & VV_ROOT) {
			if (vp->v_iflag & VI_DOOMED) {	/* forced unmount */
				error = EBADF;
				break;
			}
			vp = vp->v_mount->mnt_vnodecovered;
			continue;
		}
		if (vp->v_dd == NULL) {
			numfullpathfail1++;
			error = ENOTDIR;
			break;
		}
		ncp = TAILQ_FIRST(&vp->v_cache_dst);
		if (!ncp) {
			numfullpathfail2++;
			error = ENOENT;
			break;
		}
		MPASS(ncp->nc_dvp == vp->v_dd);
		for (i = ncp->nc_nlen - 1; i >= 0 && bp != buf; i--)
			*--bp = ncp->nc_name[i];
		if (bp == buf) {
			numfullpathfail4++;
			error = ENOMEM;
			break;
		}
		*--bp = '/';
		slash_prefixed = 1;
		vp = ncp->nc_dvp;
	}
	if (error) {
		CACHE_UNLOCK();
		return (error);
	}
	if (!slash_prefixed) {
		if (bp == buf) {
			numfullpathfail4++;
			CACHE_UNLOCK();
			return (ENOMEM);
		} else {
			*--bp = '/';
		}
	}
	numfullpathfound++;
	CACHE_UNLOCK();

	*retbuf = bp;
	return (0);
}
