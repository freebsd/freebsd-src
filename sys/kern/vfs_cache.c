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
__FBSDID("$FreeBSD$");

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
SYSCTL_INT(_debug, OID_AUTO, vnsize, CTLFLAG_RD, 0, sizeof(struct vnode), "");
SYSCTL_INT(_debug, OID_AUTO, ncsize, CTLFLAG_RD, 0, sizeof(struct namecache), "");

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
	vp = NULL;
	LIST_REMOVE(ncp, nc_hash);
	LIST_REMOVE(ncp, nc_src);
	if (LIST_EMPTY(&ncp->nc_dvp->v_cache_src)) {
		vp = ncp->nc_dvp;
		numcachehv--;
	}
	if (ncp->nc_vp) {
		TAILQ_REMOVE(&ncp->nc_vp->v_cache_dst, ncp, nc_dst);
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
 * cache_leaf_test()
 *
 *      Test whether this (directory) vnode's namei cache entry contains
 *      subdirectories or not.  Used to determine whether the directory is
 *      a leaf in the namei cache or not.  Note: the directory may still
 *      contain files in the namei cache.
 *
 *      Returns 0 if the directory is a leaf, -1 if it isn't.
 */
int
cache_leaf_test(struct vnode *vp)
{
	struct namecache *ncpc;
	int leaf;

	leaf = 0;
	CACHE_LOCK();
	for (ncpc = LIST_FIRST(&vp->v_cache_src);
	     ncpc != NULL;
	     ncpc = LIST_NEXT(ncpc, nc_src)
	 ) {
		if (ncpc->nc_vp != NULL && ncpc->nc_vp->v_type == VDIR) {
			leaf = -1;
			break;
		}
	}
	CACHE_UNLOCK();
	return (leaf);
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
 */

int
cache_lookup(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	struct namecache *ncp;
	u_int32_t hash;

	if (!doingcache) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}

	CACHE_LOCK();
	numcalls++;

	if (cnp->cn_nameptr[0] == '.') {
		if (cnp->cn_namelen == 1) {
			*vpp = dvp;
			dothits++;
			CACHE_UNLOCK();
			return (-1);
		}
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			dotdothits++;
			if (dvp->v_dd->v_id != dvp->v_ddid ||
			    (cnp->cn_flags & MAKEENTRY) == 0) {
				dvp->v_ddid = 0;
				CACHE_UNLOCK();
				return (0);
			}
			*vpp = dvp->v_dd;
			CACHE_UNLOCK();
			return (-1);
		}
	}

	hash = fnv_32_buf(cnp->cn_nameptr, cnp->cn_namelen, FNV1_32_INIT);
	hash = fnv_32_buf(&dvp->v_id, sizeof(dvp->v_id), hash);
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
		CACHE_UNLOCK();
		return (-1);
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
	struct namecache *ncp;
	struct nchashhead *ncpp;
	u_int32_t hash;
	int hold;
	int zap;
	int len;

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

	hold = 0;
	zap = 0;
	ncp = cache_alloc(cnp->cn_namelen);
	CACHE_LOCK();
	numcache++;
	if (!vp) {
		numneg++;
		ncp->nc_flag = cnp->cn_flags & ISWHITEOUT ? NCF_WHITE : 0;
	} else if (vp->v_type == VDIR) {
		vp->v_dd = dvp;
		vp->v_ddid = dvp->v_id;
	}

	/*
	 * Set the rest of the namecache entry elements, calculate it's
	 * hash key and insert it into the appropriate chain within
	 * the cache entries table.
	 */
	ncp->nc_vp = vp;
	ncp->nc_dvp = dvp;
	len = ncp->nc_nlen = cnp->cn_namelen;
	hash = fnv_32_buf(cnp->cn_nameptr, len, FNV1_32_INIT);
	bcopy(cnp->cn_nameptr, ncp->nc_name, len);
	hash = fnv_32_buf(&dvp->v_id, sizeof(dvp->v_id), hash);
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
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_SECOND, nchinit, NULL)


/*
 * Invalidate all entries to a particular vnode.
 *
 * Remove all entries in the namecache relating to this vnode and
 * change the v_id.  We take the v_id from a global counter, since
 * it becomes a handy sequence number in crash-dumps that way.
 * No valid vnode will ever have (v_id == 0).
 *
 * XXX: Only time and the size of v_id prevents this from failing:
 * XXX: In theory we should hunt down all (struct vnode*, v_id)
 * XXX: soft references and nuke them, at least on the global
 * XXX: v_id wraparound.  The period of resistance can be extended
 * XXX: by incrementing each vnodes v_id individually instead of
 * XXX: using the global v_id.
 */
void
cache_purge(vp)
	struct vnode *vp;
{
	struct namecache *ncp;
	static u_long nextid;

	CACHE_LOCK();
	while (!LIST_EMPTY(&vp->v_cache_src)) {
		struct vnode *cvp;

		ncp = LIST_FIRST(&vp->v_cache_src);
		/*
		 * We must reset v_dd of any children so they don't
		 * continue to point to us.
		 */
		if ((cvp = ncp->nc_vp) && cvp->v_dd == vp) {
			cvp->v_dd = cvp;
			cvp->v_ddid = 0;
		}
		cache_zap(ncp);
	}
	while (!TAILQ_EMPTY(&vp->v_cache_dst))
		cache_zap(TAILQ_FIRST(&vp->v_cache_dst));
	do
		nextid++;
	while (nextid == vp->v_id || !nextid);
	vp->v_id = nextid;
	vp->v_dd = vp;
	vp->v_ddid = 0;
	CACHE_UNLOCK();
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
	struct nchashhead mplist;

	LIST_INIT(&mplist);
	ncp = NULL;

	/* Scan hash tables for applicable entries */
	CACHE_LOCK();
	for (ncpp = &nchashtbl[nchash]; ncpp >= nchashtbl; ncpp--) {
		for (ncp = LIST_FIRST(ncpp); ncp != 0; ncp = nnp) {
			nnp = LIST_NEXT(ncp, nc_hash);
			if (ncp->nc_dvp->v_mount == mp) {
				LIST_REMOVE(ncp, nc_hash);
				LIST_INSERT_HEAD(&mplist, ncp, nc_hash);
			}
		}
	}
	while (!LIST_EMPTY(&mplist))
		cache_zap(LIST_FIRST(&mplist));
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
	struct vnode *dvp, *vp;
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	struct thread *td = cnp->cn_thread;
	u_long vpid;	/* capability number of vnode */

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

	if (!error) {
		error = VOP_CACHEDLOOKUP(dvp, vpp, cnp);
#ifdef LOOKUP_SHARED
		if (!error && (flags & ISLASTCN) && (flags & LOCKSHARED))
			VOP_LOCK(*vpp, LK_DOWNGRADE, td);
#endif
		return (error);
	}
	if (error == ENOENT)
		return (error);

	vp = *vpp;
	vpid = vp->v_id;
	if (dvp == vp) {   /* lookup on "." */
		VREF(vp);
		error = 0;
	} else if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, td);
#ifdef LOOKUP_SHARED
		if ((flags & ISLASTCN) && (flags & LOCKSHARED))
			error = vget(vp, LK_SHARED, td);
		else
#endif
			error = vget(vp, LK_EXCLUSIVE, td);
		if (error)
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);
	} else {
#ifdef LOOKUP_SHARED
		if ((flags & ISLASTCN) && (flags & LOCKSHARED))
			error = vget(vp, LK_SHARED, td);
		else
#endif
			error = vget(vp, LK_EXCLUSIVE, td);
	}
	/*
	 * Check that the capability number did not change
	 * while we were waiting for the lock.
	 */
	if (!error) {
		if (vpid == vp->v_id)
			return (0);
		vput(vp);
		if (flags & ISDOTDOT)
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);
	}
	error = VOP_CACHEDLOOKUP(dvp, vpp, cnp);
#ifdef LOOKUP_SHARED
	if (!error && (flags & ISLASTCN) && (flags & LOCKSHARED))
		VOP_LOCK(*vpp, LK_DOWNGRADE, td);
#endif
	return (error);
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

/* Various statistics for the getcwd syscall */
static u_long numcwdcalls; STATNODE(CTLFLAG_RD, numcwdcalls, &numcwdcalls);
static u_long numcwdfail1; STATNODE(CTLFLAG_RD, numcwdfail1, &numcwdfail1);
static u_long numcwdfail2; STATNODE(CTLFLAG_RD, numcwdfail2, &numcwdfail2);
static u_long numcwdfail3; STATNODE(CTLFLAG_RD, numcwdfail3, &numcwdfail3);
static u_long numcwdfail4; STATNODE(CTLFLAG_RD, numcwdfail4, &numcwdfail4);
static u_long numcwdfound; STATNODE(CTLFLAG_RD, numcwdfound, &numcwdfound);

/* Implementation of the getcwd syscall */
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
	int error, i, slash_prefixed;
	struct filedesc *fdp;
	struct namecache *ncp;
	struct vnode *vp;

	numcwdcalls++;
	if (disablecwd)
		return (ENODEV);
	if (buflen < 2)
		return (EINVAL);
	if (buflen > MAXPATHLEN)
		buflen = MAXPATHLEN;
	mtx_lock(&Giant);
	error = 0;
	tmpbuf = bp = malloc(buflen, M_TEMP, M_WAITOK);
	bp += buflen - 1;
	*bp = '\0';
	fdp = td->td_proc->p_fd;
	slash_prefixed = 0;
	FILEDESC_LOCK(fdp);
	for (vp = fdp->fd_cdir; vp != fdp->fd_rdir && vp != rootvnode;) {
		if (vp->v_vflag & VV_ROOT) {
			if (vp->v_mount == NULL) {	/* forced unmount */
				error = EBADF;
				goto out;
			}
			vp = vp->v_mount->mnt_vnodecovered;
			continue;
		}
		if (vp->v_dd->v_id != vp->v_ddid) {
			numcwdfail1++;
			error = ENOTDIR;
			goto out;
		}
		CACHE_LOCK();
		ncp = TAILQ_FIRST(&vp->v_cache_dst);
		if (!ncp) {
			numcwdfail2++;
			CACHE_UNLOCK();
			error = ENOENT;
			goto out;
		}
		if (ncp->nc_dvp != vp->v_dd) {
			numcwdfail3++;
			CACHE_UNLOCK();
			error = EBADF;
			goto out;
		}
		for (i = ncp->nc_nlen - 1; i >= 0; i--) {
			if (bp == tmpbuf) {
				numcwdfail4++;
				CACHE_UNLOCK();
				error = ENOMEM;
				goto out;
			}
			*--bp = ncp->nc_name[i];
		}
		if (bp == tmpbuf) {
			numcwdfail4++;
			CACHE_UNLOCK();
			error = ENOMEM;
			goto out;
		}
		*--bp = '/';
		slash_prefixed = 1;
		vp = vp->v_dd;
		CACHE_UNLOCK();
	}
	if (!slash_prefixed) {
		if (bp == tmpbuf) {
			numcwdfail4++;
			error = ENOMEM;
			goto out;
		}
		*--bp = '/';
	}
	FILEDESC_UNLOCK(fdp);
	mtx_unlock(&Giant);
	numcwdfound++;
	if (bufseg == UIO_SYSSPACE)
		bcopy(bp, buf, strlen(bp) + 1);
	else
		error = copyout(bp, buf, strlen(bp) + 1);
	free(tmpbuf, M_TEMP);
	return (error);
out:
	FILEDESC_UNLOCK(fdp);
	mtx_unlock(&Giant);
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

STATNODE(numfullpathcalls);
STATNODE(numfullpathfail1);
STATNODE(numfullpathfail2);
STATNODE(numfullpathfail3);
STATNODE(numfullpathfail4);
STATNODE(numfullpathfound);

/*
 * Retrieve the full filesystem path that correspond to a vnode from the name
 * cache (if available)
 */
int
vn_fullpath(struct thread *td, struct vnode *vn, char **retbuf, char **freebuf)
{
	char *bp, *buf;
	int i, slash_prefixed;
	struct filedesc *fdp;
	struct namecache *ncp;
	struct vnode *vp;

	numfullpathcalls++;
	if (disablefullpath)
		return (ENODEV);
	if (vn == NULL)
		return (EINVAL);
	buf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	bp = buf + MAXPATHLEN - 1;
	*bp = '\0';
	fdp = td->td_proc->p_fd;
	slash_prefixed = 0;
	ASSERT_VOP_LOCKED(vn, "vn_fullpath");
	FILEDESC_LOCK(fdp);
	for (vp = vn; vp != fdp->fd_rdir && vp != rootvnode;) {
		if (vp->v_vflag & VV_ROOT) {
			if (vp->v_mount == NULL) {	/* forced unmount */
				FILEDESC_UNLOCK(fdp);
				free(buf, M_TEMP);
				return (EBADF);
			}
			vp = vp->v_mount->mnt_vnodecovered;
			continue;
		}
		if (vp != vn && vp->v_dd->v_id != vp->v_ddid) {
			FILEDESC_UNLOCK(fdp);
			free(buf, M_TEMP);
			numfullpathfail1++;
			return (ENOTDIR);
		}
		CACHE_LOCK();
		ncp = TAILQ_FIRST(&vp->v_cache_dst);
		if (!ncp) {
			numfullpathfail2++;
			CACHE_UNLOCK();
			FILEDESC_UNLOCK(fdp);
			free(buf, M_TEMP);
			return (ENOENT);
		}
		if (vp != vn && ncp->nc_dvp != vp->v_dd) {
			numfullpathfail3++;
			CACHE_UNLOCK();
			FILEDESC_UNLOCK(fdp);
			free(buf, M_TEMP);
			return (EBADF);
		}
		for (i = ncp->nc_nlen - 1; i >= 0; i--) {
			if (bp == buf) {
				numfullpathfail4++;
				CACHE_UNLOCK();
				FILEDESC_UNLOCK(fdp);
				free(buf, M_TEMP);
				return (ENOMEM);
			}
			*--bp = ncp->nc_name[i];
		}
		if (bp == buf) {
			numfullpathfail4++;
			CACHE_UNLOCK();
			FILEDESC_UNLOCK(fdp);
			free(buf, M_TEMP);
			return (ENOMEM);
		}
		*--bp = '/';
		slash_prefixed = 1;
		vp = ncp->nc_dvp;
		CACHE_UNLOCK();
	}
	if (!slash_prefixed) {
		if (bp == buf) {
			numfullpathfail4++;
			FILEDESC_UNLOCK(fdp);
			free(buf, M_TEMP);
			return (ENOMEM);
		}
		*--bp = '/';
	}
	FILEDESC_UNLOCK(fdp);
	numfullpathfound++;
	*retbuf = bp;
	*freebuf = buf;
	return (0);
}
