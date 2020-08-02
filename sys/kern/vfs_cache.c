/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/counter.h>
#include <sys/filedesc.h>
#include <sys/fnv_hash.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/seqc.h>
#include <sys/sdt.h>
#include <sys/smr.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>
#include <ck_queue.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/capsicum.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <vm/uma.h>

SDT_PROVIDER_DECLARE(vfs);
SDT_PROBE_DEFINE3(vfs, namecache, enter, done, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, enter_negative, done, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE1(vfs, namecache, fullpath, entry, "struct vnode *");
SDT_PROBE_DEFINE3(vfs, namecache, fullpath, hit, "struct vnode *",
    "char *", "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, fullpath, miss, "struct vnode *");
SDT_PROBE_DEFINE3(vfs, namecache, fullpath, return, "int",
    "struct vnode *", "char *");
SDT_PROBE_DEFINE3(vfs, namecache, lookup, hit, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, lookup, hit__negative,
    "struct vnode *", "char *");
SDT_PROBE_DEFINE2(vfs, namecache, lookup, miss, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE1(vfs, namecache, purge, done, "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, purge_negative, done, "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, purgevfs, done, "struct mount *");
SDT_PROBE_DEFINE3(vfs, namecache, zap, done, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, zap_negative, done, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE2(vfs, namecache, shrink_negative, done, "struct vnode *",
    "char *");

SDT_PROBE_DEFINE3(vfs, fplookup, lookup, done, "struct nameidata", "int", "bool");
SDT_PROBE_DECLARE(vfs, namei, lookup, entry);
SDT_PROBE_DECLARE(vfs, namei, lookup, return);

/*
 * This structure describes the elements in the cache of recent
 * names looked up by namei.
 */
struct negstate {
	u_char neg_flag;
};
_Static_assert(sizeof(struct negstate) <= sizeof(struct vnode *),
    "the state must fit in a union with a pointer without growing it");

struct	namecache {
	CK_LIST_ENTRY(namecache) nc_hash;/* hash chain */
	LIST_ENTRY(namecache) nc_src;	/* source vnode list */
	TAILQ_ENTRY(namecache) nc_dst;	/* destination vnode list */
	struct	vnode *nc_dvp;		/* vnode of parent of name */
	union {
		struct	vnode *nu_vp;	/* vnode the name refers to */
		struct	negstate nu_neg;/* negative entry state */
	} n_un;
	u_char	nc_flag;		/* flag bits */
	u_char	nc_nlen;		/* length of name */
	char	nc_name[0];		/* segment name + nul */
};

/*
 * struct namecache_ts repeats struct namecache layout up to the
 * nc_nlen member.
 * struct namecache_ts is used in place of struct namecache when time(s) need
 * to be stored.  The nc_dotdottime field is used when a cache entry is mapping
 * both a non-dotdot directory name plus dotdot for the directory's
 * parent.
 */
struct	namecache_ts {
	struct	timespec nc_time;	/* timespec provided by fs */
	struct	timespec nc_dotdottime;	/* dotdot timespec provided by fs */
	int	nc_ticks;		/* ticks value when entry was added */
	struct namecache nc_nc;
};

#define	nc_vp		n_un.nu_vp
#define	nc_neg		n_un.nu_neg

/*
 * Flags in namecache.nc_flag
 */
#define NCF_WHITE	0x01
#define NCF_ISDOTDOT	0x02
#define	NCF_TS		0x04
#define	NCF_DTS		0x08
#define	NCF_DVDROP	0x10
#define	NCF_NEGATIVE	0x20
#define	NCF_INVALID	0x40

/*
 * Flags in negstate.neg_flag
 */
#define NEG_HOT		0x01

/*
 * Mark an entry as invalid.
 *
 * This is called before it starts getting deconstructed.
 */
static void
cache_ncp_invalidate(struct namecache *ncp)
{

	KASSERT((ncp->nc_flag & NCF_INVALID) == 0,
	    ("%s: entry %p already invalid", __func__, ncp));
	ncp->nc_flag |= NCF_INVALID;
	atomic_thread_fence_rel();
}

/*
 * Verify validity of an entry.
 *
 * All places which elide locks are supposed to call this after they are
 * done with reading from an entry.
 */
static bool
cache_ncp_invalid(struct namecache *ncp)
{

	atomic_thread_fence_acq();
	return ((ncp->nc_flag & NCF_INVALID) != 0);
}

/*
 * Name caching works as follows:
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (dvp, name) where dvp refers to the directory
 * containing name.
 *
 * If it is a "negative" entry, (i.e. for a name that is known NOT to
 * exist) the vnode pointer will be NULL.
 *
 * Upon reaching the last segment of a path, if the reference
 * is for DELETE, or NOCACHE is set (rewrite), and the
 * name is located in the cache, it will be dropped.
 *
 * These locks are used (in the order in which they can be taken):
 * NAME		TYPE	ROLE
 * vnodelock	mtx	vnode lists and v_cache_dd field protection
 * bucketlock	rwlock	for access to given set of hash buckets
 * neglist	mtx	negative entry LRU management
 *
 * Additionally, ncneg_shrink_lock mtx is used to have at most one thread
 * shrinking the LRU list.
 *
 * It is legal to take multiple vnodelock and bucketlock locks. The locking
 * order is lower address first. Both are recursive.
 *
 * "." lookups are lockless.
 *
 * ".." and vnode -> name lookups require vnodelock.
 *
 * name -> vnode lookup requires the relevant bucketlock to be held for reading.
 *
 * Insertions and removals of entries require involved vnodes and bucketlocks
 * to be write-locked to prevent other threads from seeing the entry.
 *
 * Some lookups result in removal of the found entry (e.g. getting rid of a
 * negative entry with the intent to create a positive one), which poses a
 * problem when multiple threads reach the state. Similarly, two different
 * threads can purge two different vnodes and try to remove the same name.
 *
 * If the already held vnode lock is lower than the second required lock, we
 * can just take the other lock. However, in the opposite case, this could
 * deadlock. As such, this is resolved by trylocking and if that fails unlocking
 * the first node, locking everything in order and revalidating the state.
 */

VFS_SMR_DECLARE;

/*
 * Structures associated with name caching.
 */
#define NCHHASH(hash) \
	(&nchashtbl[(hash) & nchash])
static __read_mostly CK_LIST_HEAD(nchashhead, namecache) *nchashtbl;/* Hash Table */
static u_long __read_mostly	nchash;			/* size of hash table */
SYSCTL_ULONG(_debug, OID_AUTO, nchash, CTLFLAG_RD, &nchash, 0,
    "Size of namecache hash table");
static u_long __read_mostly	ncnegfactor = 5; /* ratio of negative entries */
SYSCTL_ULONG(_vfs, OID_AUTO, ncnegfactor, CTLFLAG_RW, &ncnegfactor, 0,
    "Ratio of negative namecache entries");
static u_long __exclusive_cache_line	numneg;	/* number of negative entries allocated */
static u_long __exclusive_cache_line	numcache;/* number of cache entries allocated */
u_int ncsizefactor = 2;
SYSCTL_UINT(_vfs, OID_AUTO, ncsizefactor, CTLFLAG_RW, &ncsizefactor, 0,
    "Size factor for namecache");
static u_int __read_mostly	ncpurgeminvnodes;
SYSCTL_UINT(_vfs, OID_AUTO, ncpurgeminvnodes, CTLFLAG_RW, &ncpurgeminvnodes, 0,
    "Number of vnodes below which purgevfs ignores the request");
static u_int __read_mostly	ncsize; /* the size as computed on creation or resizing */

struct nchstats	nchstats;		/* cache effectiveness statistics */

static struct mtx __exclusive_cache_line	ncneg_shrink_lock;

struct neglist {
	struct mtx		nl_lock;
	TAILQ_HEAD(, namecache) nl_list;
} __aligned(CACHE_LINE_SIZE);

static struct neglist __read_mostly	*neglists;
static struct neglist ncneg_hot;
static u_long numhotneg;

#define	numneglists (ncneghash + 1)
static u_int __read_mostly	ncneghash;
static inline struct neglist *
NCP2NEGLIST(struct namecache *ncp)
{

	return (&neglists[(((uintptr_t)(ncp) >> 8) & ncneghash)]);
}

static inline struct negstate *
NCP2NEGSTATE(struct namecache *ncp)
{

	MPASS(ncp->nc_flag & NCF_NEGATIVE);
	return (&ncp->nc_neg);
}

#define	numbucketlocks (ncbuckethash + 1)
static u_int __read_mostly  ncbuckethash;
static struct rwlock_padalign __read_mostly  *bucketlocks;
#define	HASH2BUCKETLOCK(hash) \
	((struct rwlock *)(&bucketlocks[((hash) & ncbuckethash)]))

#define	numvnodelocks (ncvnodehash + 1)
static u_int __read_mostly  ncvnodehash;
static struct mtx __read_mostly *vnodelocks;
static inline struct mtx *
VP2VNODELOCK(struct vnode *vp)
{

	return (&vnodelocks[(((uintptr_t)(vp) >> 8) & ncvnodehash)]);
}

/*
 * UMA zones for the VFS cache.
 *
 * The small cache is used for entries with short names, which are the
 * most common.  The large cache is used for entries which are too big to
 * fit in the small cache.
 */
static uma_zone_t __read_mostly cache_zone_small;
static uma_zone_t __read_mostly cache_zone_small_ts;
static uma_zone_t __read_mostly cache_zone_large;
static uma_zone_t __read_mostly cache_zone_large_ts;

#define	CACHE_PATH_CUTOFF	35

static struct namecache *
cache_alloc(int len, int ts)
{
	struct namecache_ts *ncp_ts;
	struct namecache *ncp;

	if (__predict_false(ts)) {
		if (len <= CACHE_PATH_CUTOFF)
			ncp_ts = uma_zalloc_smr(cache_zone_small_ts, M_WAITOK);
		else
			ncp_ts = uma_zalloc_smr(cache_zone_large_ts, M_WAITOK);
		ncp = &ncp_ts->nc_nc;
	} else {
		if (len <= CACHE_PATH_CUTOFF)
			ncp = uma_zalloc_smr(cache_zone_small, M_WAITOK);
		else
			ncp = uma_zalloc_smr(cache_zone_large, M_WAITOK);
	}
	return (ncp);
}

static void
cache_free(struct namecache *ncp)
{
	struct namecache_ts *ncp_ts;

	if (ncp == NULL)
		return;
	if ((ncp->nc_flag & NCF_DVDROP) != 0)
		vdrop(ncp->nc_dvp);
	if (__predict_false(ncp->nc_flag & NCF_TS)) {
		ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
		if (ncp->nc_nlen <= CACHE_PATH_CUTOFF)
			uma_zfree_smr(cache_zone_small_ts, ncp_ts);
		else
			uma_zfree_smr(cache_zone_large_ts, ncp_ts);
	} else {
		if (ncp->nc_nlen <= CACHE_PATH_CUTOFF)
			uma_zfree_smr(cache_zone_small, ncp);
		else
			uma_zfree_smr(cache_zone_large, ncp);
	}
}

static void
cache_out_ts(struct namecache *ncp, struct timespec *tsp, int *ticksp)
{
	struct namecache_ts *ncp_ts;

	KASSERT((ncp->nc_flag & NCF_TS) != 0 ||
	    (tsp == NULL && ticksp == NULL),
	    ("No NCF_TS"));

	if (tsp == NULL && ticksp == NULL)
		return;

	ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
	if (tsp != NULL)
		*tsp = ncp_ts->nc_time;
	if (ticksp != NULL)
		*ticksp = ncp_ts->nc_ticks;
}

#ifdef DEBUG_CACHE
static int __read_mostly	doingcache = 1;	/* 1 => enable the cache */
SYSCTL_INT(_debug, OID_AUTO, vfscache, CTLFLAG_RW, &doingcache, 0,
    "VFS namecache enabled");
#endif

/* Export size information to userland */
SYSCTL_INT(_debug_sizeof, OID_AUTO, namecache, CTLFLAG_RD, SYSCTL_NULL_INT_PTR,
    sizeof(struct namecache), "sizeof(struct namecache)");

/*
 * The new name cache statistics
 */
static SYSCTL_NODE(_vfs, OID_AUTO, cache, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Name cache statistics");
#define STATNODE_ULONG(name, descr)					\
	SYSCTL_ULONG(_vfs_cache, OID_AUTO, name, CTLFLAG_RD, &name, 0, descr);
#define STATNODE_COUNTER(name, descr)					\
	static COUNTER_U64_DEFINE_EARLY(name);				\
	SYSCTL_COUNTER_U64(_vfs_cache, OID_AUTO, name, CTLFLAG_RD, &name, \
	    descr);
STATNODE_ULONG(numneg, "Number of negative cache entries");
STATNODE_ULONG(numcache, "Number of cache entries");
STATNODE_COUNTER(numcachehv, "Number of namecache entries with vnodes held");
STATNODE_COUNTER(numdrops, "Number of dropped entries due to reaching the limit");
STATNODE_COUNTER(dothits, "Number of '.' hits");
STATNODE_COUNTER(dotdothits, "Number of '..' hits");
STATNODE_COUNTER(nummiss, "Number of cache misses");
STATNODE_COUNTER(nummisszap, "Number of cache misses we do not want to cache");
STATNODE_COUNTER(numposzaps,
    "Number of cache hits (positive) we do not want to cache");
STATNODE_COUNTER(numposhits, "Number of cache hits (positive)");
STATNODE_COUNTER(numnegzaps,
    "Number of cache hits (negative) we do not want to cache");
STATNODE_COUNTER(numneghits, "Number of cache hits (negative)");
/* These count for vn_getcwd(), too. */
STATNODE_COUNTER(numfullpathcalls, "Number of fullpath search calls");
STATNODE_COUNTER(numfullpathfail1, "Number of fullpath search errors (ENOTDIR)");
STATNODE_COUNTER(numfullpathfail2,
    "Number of fullpath search errors (VOP_VPTOCNP failures)");
STATNODE_COUNTER(numfullpathfail4, "Number of fullpath search errors (ENOMEM)");
STATNODE_COUNTER(numfullpathfound, "Number of successful fullpath calls");
STATNODE_COUNTER(zap_and_exit_bucket_relock_success,
    "Number of successful removals after relocking");
static long zap_and_exit_bucket_fail; STATNODE_ULONG(zap_and_exit_bucket_fail,
    "Number of times zap_and_exit failed to lock");
static long zap_and_exit_bucket_fail2; STATNODE_ULONG(zap_and_exit_bucket_fail2,
    "Number of times zap_and_exit failed to lock");
static long cache_lock_vnodes_cel_3_failures;
STATNODE_ULONG(cache_lock_vnodes_cel_3_failures,
    "Number of times 3-way vnode locking failed");
STATNODE_ULONG(numhotneg, "Number of hot negative entries");
STATNODE_COUNTER(numneg_evicted,
    "Number of negative entries evicted when adding a new entry");
STATNODE_COUNTER(shrinking_skipped,
    "Number of times shrinking was already in progress");

static void cache_zap_locked(struct namecache *ncp);
static int vn_fullpath_hardlink(struct thread *td, struct nameidata *ndp, char **retbuf,
    char **freebuf, size_t *buflen);
static int vn_fullpath_any(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, size_t *buflen);
static int vn_fullpath_dir(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, size_t *len, bool slash_prefixed, size_t addend);

static MALLOC_DEFINE(M_VFSCACHE, "vfscache", "VFS name cache entries");

static int cache_yield;
SYSCTL_INT(_vfs_cache, OID_AUTO, yield, CTLFLAG_RD, &cache_yield, 0,
    "Number of times cache called yield");

static void __noinline
cache_maybe_yield(void)
{

	if (should_yield()) {
		cache_yield++;
		kern_yield(PRI_USER);
	}
}

static inline void
cache_assert_vlp_locked(struct mtx *vlp)
{

	if (vlp != NULL)
		mtx_assert(vlp, MA_OWNED);
}

static inline void
cache_assert_vnode_locked(struct vnode *vp)
{
	struct mtx *vlp;

	vlp = VP2VNODELOCK(vp);
	cache_assert_vlp_locked(vlp);
}

/*
 * TODO: With the value stored we can do better than computing the hash based
 * on the address and the choice of FNV should also be revisisted.
 */
static void
cache_prehash(struct vnode *vp)
{

	vp->v_nchash = fnv_32_buf(&vp, sizeof(vp), FNV1_32_INIT);
}

static uint32_t
cache_get_hash(char *name, u_char len, struct vnode *dvp)
{

	return (fnv_32_buf(name, len, dvp->v_nchash));
}

static inline struct rwlock *
NCP2BUCKETLOCK(struct namecache *ncp)
{
	uint32_t hash;

	hash = cache_get_hash(ncp->nc_name, ncp->nc_nlen, ncp->nc_dvp);
	return (HASH2BUCKETLOCK(hash));
}

#ifdef INVARIANTS
static void
cache_assert_bucket_locked(struct namecache *ncp, int mode)
{
	struct rwlock *blp;

	blp = NCP2BUCKETLOCK(ncp);
	rw_assert(blp, mode);
}
#else
#define cache_assert_bucket_locked(x, y) do { } while (0)
#endif

#define cache_sort_vnodes(x, y)	_cache_sort_vnodes((void **)(x), (void **)(y))
static void
_cache_sort_vnodes(void **p1, void **p2)
{
	void *tmp;

	MPASS(*p1 != NULL || *p2 != NULL);

	if (*p1 > *p2) {
		tmp = *p2;
		*p2 = *p1;
		*p1 = tmp;
	}
}

static void
cache_lock_all_buckets(void)
{
	u_int i;

	for (i = 0; i < numbucketlocks; i++)
		rw_wlock(&bucketlocks[i]);
}

static void
cache_unlock_all_buckets(void)
{
	u_int i;

	for (i = 0; i < numbucketlocks; i++)
		rw_wunlock(&bucketlocks[i]);
}

static void
cache_lock_all_vnodes(void)
{
	u_int i;

	for (i = 0; i < numvnodelocks; i++)
		mtx_lock(&vnodelocks[i]);
}

static void
cache_unlock_all_vnodes(void)
{
	u_int i;

	for (i = 0; i < numvnodelocks; i++)
		mtx_unlock(&vnodelocks[i]);
}

static int
cache_trylock_vnodes(struct mtx *vlp1, struct mtx *vlp2)
{

	cache_sort_vnodes(&vlp1, &vlp2);

	if (vlp1 != NULL) {
		if (!mtx_trylock(vlp1))
			return (EAGAIN);
	}
	if (!mtx_trylock(vlp2)) {
		if (vlp1 != NULL)
			mtx_unlock(vlp1);
		return (EAGAIN);
	}

	return (0);
}

static void
cache_lock_vnodes(struct mtx *vlp1, struct mtx *vlp2)
{

	MPASS(vlp1 != NULL || vlp2 != NULL);
	MPASS(vlp1 <= vlp2);

	if (vlp1 != NULL)
		mtx_lock(vlp1);
	if (vlp2 != NULL)
		mtx_lock(vlp2);
}

static void
cache_unlock_vnodes(struct mtx *vlp1, struct mtx *vlp2)
{

	MPASS(vlp1 != NULL || vlp2 != NULL);

	if (vlp1 != NULL)
		mtx_unlock(vlp1);
	if (vlp2 != NULL)
		mtx_unlock(vlp2);
}

static int
sysctl_nchstats(SYSCTL_HANDLER_ARGS)
{
	struct nchstats snap;

	if (req->oldptr == NULL)
		return (SYSCTL_OUT(req, 0, sizeof(snap)));

	snap = nchstats;
	snap.ncs_goodhits = counter_u64_fetch(numposhits);
	snap.ncs_neghits = counter_u64_fetch(numneghits);
	snap.ncs_badhits = counter_u64_fetch(numposzaps) +
	    counter_u64_fetch(numnegzaps);
	snap.ncs_miss = counter_u64_fetch(nummisszap) +
	    counter_u64_fetch(nummiss);

	return (SYSCTL_OUT(req, &snap, sizeof(snap)));
}
SYSCTL_PROC(_vfs_cache, OID_AUTO, nchstats, CTLTYPE_OPAQUE | CTLFLAG_RD |
    CTLFLAG_MPSAFE, 0, 0, sysctl_nchstats, "LU",
    "VFS cache effectiveness statistics");

#ifdef DIAGNOSTIC
/*
 * Grab an atomic snapshot of the name cache hash chain lengths
 */
static SYSCTL_NODE(_debug, OID_AUTO, hashstat,
    CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "hash table stats");

static int
sysctl_debug_hashstat_rawnchash(SYSCTL_HANDLER_ARGS)
{
	struct nchashhead *ncpp;
	struct namecache *ncp;
	int i, error, n_nchash, *cntbuf;

retry:
	n_nchash = nchash + 1;	/* nchash is max index, not count */
	if (req->oldptr == NULL)
		return SYSCTL_OUT(req, 0, n_nchash * sizeof(int));
	cntbuf = malloc(n_nchash * sizeof(int), M_TEMP, M_ZERO | M_WAITOK);
	cache_lock_all_buckets();
	if (n_nchash != nchash + 1) {
		cache_unlock_all_buckets();
		free(cntbuf, M_TEMP);
		goto retry;
	}
	/* Scan hash tables counting entries */
	for (ncpp = nchashtbl, i = 0; i < n_nchash; ncpp++, i++)
		CK_LIST_FOREACH(ncp, ncpp, nc_hash)
			cntbuf[i]++;
	cache_unlock_all_buckets();
	for (error = 0, i = 0; i < n_nchash; i++)
		if ((error = SYSCTL_OUT(req, &cntbuf[i], sizeof(int))) != 0)
			break;
	free(cntbuf, M_TEMP);
	return (error);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, rawnchash, CTLTYPE_INT|CTLFLAG_RD|
    CTLFLAG_MPSAFE, 0, 0, sysctl_debug_hashstat_rawnchash, "S,int",
    "nchash chain lengths");

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

	cache_lock_all_buckets();
	n_nchash = nchash + 1;	/* nchash is max index, not count */
	used = 0;
	maxlength = 0;

	/* Scan hash tables for applicable entries */
	for (ncpp = nchashtbl; n_nchash > 0; n_nchash--, ncpp++) {
		count = 0;
		CK_LIST_FOREACH(ncp, ncpp, nc_hash) {
			count++;
		}
		if (count)
			used++;
		if (maxlength < count)
			maxlength = count;
	}
	n_nchash = nchash + 1;
	cache_unlock_all_buckets();
	pct = (used * 100) / (n_nchash / 100);
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
SYSCTL_PROC(_debug_hashstat, OID_AUTO, nchash, CTLTYPE_INT|CTLFLAG_RD|
    CTLFLAG_MPSAFE, 0, 0, sysctl_debug_hashstat_nchash, "I",
    "nchash statistics (number of total/used buckets, maximum chain length, usage percentage)");
#endif

/*
 * Negative entries management
 *
 * A variation of LRU scheme is used. New entries are hashed into one of
 * numneglists cold lists. Entries get promoted to the hot list on first hit.
 *
 * The shrinker will demote hot list head and evict from the cold list in a
 * round-robin manner.
 */
static void
cache_negative_init(struct namecache *ncp)
{
	struct negstate *negstate;

	ncp->nc_flag |= NCF_NEGATIVE;
	negstate = NCP2NEGSTATE(ncp);
	negstate->neg_flag = 0;
}

static void
cache_negative_hit(struct namecache *ncp)
{
	struct neglist *neglist;
	struct negstate *negstate;

	negstate = NCP2NEGSTATE(ncp);
	if ((negstate->neg_flag & NEG_HOT) != 0)
		return;
	neglist = NCP2NEGLIST(ncp);
	mtx_lock(&ncneg_hot.nl_lock);
	mtx_lock(&neglist->nl_lock);
	if ((negstate->neg_flag & NEG_HOT) == 0) {
		numhotneg++;
		TAILQ_REMOVE(&neglist->nl_list, ncp, nc_dst);
		TAILQ_INSERT_TAIL(&ncneg_hot.nl_list, ncp, nc_dst);
		negstate->neg_flag |= NEG_HOT;
	}
	mtx_unlock(&neglist->nl_lock);
	mtx_unlock(&ncneg_hot.nl_lock);
}

static void
cache_negative_insert(struct namecache *ncp)
{
	struct neglist *neglist;

	MPASS(ncp->nc_flag & NCF_NEGATIVE);
	cache_assert_bucket_locked(ncp, RA_WLOCKED);
	neglist = NCP2NEGLIST(ncp);
	mtx_lock(&neglist->nl_lock);
	TAILQ_INSERT_TAIL(&neglist->nl_list, ncp, nc_dst);
	mtx_unlock(&neglist->nl_lock);
	atomic_add_rel_long(&numneg, 1);
}

static void
cache_negative_remove(struct namecache *ncp)
{
	struct neglist *neglist;
	struct negstate *negstate;
	bool hot_locked = false;
	bool list_locked = false;

	cache_assert_bucket_locked(ncp, RA_WLOCKED);
	neglist = NCP2NEGLIST(ncp);
	negstate = NCP2NEGSTATE(ncp);
	if ((negstate->neg_flag & NEG_HOT) != 0) {
		hot_locked = true;
		mtx_lock(&ncneg_hot.nl_lock);
		if ((negstate->neg_flag & NEG_HOT) == 0) {
			list_locked = true;
			mtx_lock(&neglist->nl_lock);
		}
	} else {
		list_locked = true;
		mtx_lock(&neglist->nl_lock);
		/*
		 * We may be racing against promotion in lockless lookup.
		 */
		if ((negstate->neg_flag & NEG_HOT) != 0) {
			mtx_unlock(&neglist->nl_lock);
			hot_locked = true;
			mtx_lock(&ncneg_hot.nl_lock);
			mtx_lock(&neglist->nl_lock);
		}
	}
	if ((negstate->neg_flag & NEG_HOT) != 0) {
		mtx_assert(&ncneg_hot.nl_lock, MA_OWNED);
		TAILQ_REMOVE(&ncneg_hot.nl_list, ncp, nc_dst);
		numhotneg--;
	} else {
		mtx_assert(&neglist->nl_lock, MA_OWNED);
		TAILQ_REMOVE(&neglist->nl_list, ncp, nc_dst);
	}
	if (list_locked)
		mtx_unlock(&neglist->nl_lock);
	if (hot_locked)
		mtx_unlock(&ncneg_hot.nl_lock);
	atomic_subtract_rel_long(&numneg, 1);
}

static void
cache_negative_shrink_select(struct namecache **ncpp,
    struct neglist **neglistpp)
{
	struct neglist *neglist;
	struct namecache *ncp;
	static u_int cycle;
	u_int i;

	*ncpp = ncp = NULL;

	for (i = 0; i < numneglists; i++) {
		neglist = &neglists[(cycle + i) % numneglists];
		if (TAILQ_FIRST(&neglist->nl_list) == NULL)
			continue;
		mtx_lock(&neglist->nl_lock);
		ncp = TAILQ_FIRST(&neglist->nl_list);
		if (ncp != NULL)
			break;
		mtx_unlock(&neglist->nl_lock);
	}

	*neglistpp = neglist;
	*ncpp = ncp;
	cycle++;
}

static void
cache_negative_zap_one(void)
{
	struct namecache *ncp, *ncp2;
	struct neglist *neglist;
	struct negstate *negstate;
	struct mtx *dvlp;
	struct rwlock *blp;

	if (mtx_owner(&ncneg_shrink_lock) != NULL ||
	    !mtx_trylock(&ncneg_shrink_lock)) {
		counter_u64_add(shrinking_skipped, 1);
		return;
	}

	mtx_lock(&ncneg_hot.nl_lock);
	ncp = TAILQ_FIRST(&ncneg_hot.nl_list);
	if (ncp != NULL) {
		neglist = NCP2NEGLIST(ncp);
		negstate = NCP2NEGSTATE(ncp);
		mtx_lock(&neglist->nl_lock);
		MPASS((negstate->neg_flag & NEG_HOT) != 0);
		TAILQ_REMOVE(&ncneg_hot.nl_list, ncp, nc_dst);
		TAILQ_INSERT_TAIL(&neglist->nl_list, ncp, nc_dst);
		negstate->neg_flag &= ~NEG_HOT;
		numhotneg--;
		mtx_unlock(&neglist->nl_lock);
	}
	mtx_unlock(&ncneg_hot.nl_lock);

	cache_negative_shrink_select(&ncp, &neglist);

	mtx_unlock(&ncneg_shrink_lock);
	if (ncp == NULL)
		return;

	MPASS(ncp->nc_flag & NCF_NEGATIVE);
	dvlp = VP2VNODELOCK(ncp->nc_dvp);
	blp = NCP2BUCKETLOCK(ncp);
	mtx_unlock(&neglist->nl_lock);
	mtx_lock(dvlp);
	rw_wlock(blp);
	/*
	 * Enter SMR to safely check the negative list.
	 * Even if the found pointer matches, the entry may now be reallocated
	 * and used by a different vnode.
	 */
	vfs_smr_enter();
	ncp2 = TAILQ_FIRST(&neglist->nl_list);
	if (ncp != ncp2 || dvlp != VP2VNODELOCK(ncp2->nc_dvp) ||
	    blp != NCP2BUCKETLOCK(ncp2)) {
		vfs_smr_exit();
		ncp = NULL;
	} else {
		vfs_smr_exit();
		SDT_PROBE2(vfs, namecache, shrink_negative, done, ncp->nc_dvp,
		    ncp->nc_name);
		cache_zap_locked(ncp);
		counter_u64_add(numneg_evicted, 1);
	}
	rw_wunlock(blp);
	mtx_unlock(dvlp);
	cache_free(ncp);
}

/*
 * cache_zap_locked():
 *
 *   Removes a namecache entry from cache, whether it contains an actual
 *   pointer to a vnode or if it is just a negative cache entry.
 */
static void
cache_zap_locked(struct namecache *ncp)
{

	if (!(ncp->nc_flag & NCF_NEGATIVE))
		cache_assert_vnode_locked(ncp->nc_vp);
	cache_assert_vnode_locked(ncp->nc_dvp);
	cache_assert_bucket_locked(ncp, RA_WLOCKED);

	CTR2(KTR_VFS, "cache_zap(%p) vp %p", ncp,
	    (ncp->nc_flag & NCF_NEGATIVE) ? NULL : ncp->nc_vp);

	cache_ncp_invalidate(ncp);

	CK_LIST_REMOVE(ncp, nc_hash);
	if (!(ncp->nc_flag & NCF_NEGATIVE)) {
		SDT_PROBE3(vfs, namecache, zap, done, ncp->nc_dvp,
		    ncp->nc_name, ncp->nc_vp);
		TAILQ_REMOVE(&ncp->nc_vp->v_cache_dst, ncp, nc_dst);
		if (ncp == ncp->nc_vp->v_cache_dd)
			ncp->nc_vp->v_cache_dd = NULL;
	} else {
		SDT_PROBE2(vfs, namecache, zap_negative, done, ncp->nc_dvp,
		    ncp->nc_name);
		cache_negative_remove(ncp);
	}
	if (ncp->nc_flag & NCF_ISDOTDOT) {
		if (ncp == ncp->nc_dvp->v_cache_dd)
			ncp->nc_dvp->v_cache_dd = NULL;
	} else {
		LIST_REMOVE(ncp, nc_src);
		if (LIST_EMPTY(&ncp->nc_dvp->v_cache_src)) {
			ncp->nc_flag |= NCF_DVDROP;
			counter_u64_add(numcachehv, -1);
		}
	}
	atomic_subtract_rel_long(&numcache, 1);
}

static void
cache_zap_negative_locked_vnode_kl(struct namecache *ncp, struct vnode *vp)
{
	struct rwlock *blp;

	MPASS(ncp->nc_dvp == vp);
	MPASS(ncp->nc_flag & NCF_NEGATIVE);
	cache_assert_vnode_locked(vp);

	blp = NCP2BUCKETLOCK(ncp);
	rw_wlock(blp);
	cache_zap_locked(ncp);
	rw_wunlock(blp);
}

static bool
cache_zap_locked_vnode_kl2(struct namecache *ncp, struct vnode *vp,
    struct mtx **vlpp)
{
	struct mtx *pvlp, *vlp1, *vlp2, *to_unlock;
	struct rwlock *blp;

	MPASS(vp == ncp->nc_dvp || vp == ncp->nc_vp);
	cache_assert_vnode_locked(vp);

	if (ncp->nc_flag & NCF_NEGATIVE) {
		if (*vlpp != NULL) {
			mtx_unlock(*vlpp);
			*vlpp = NULL;
		}
		cache_zap_negative_locked_vnode_kl(ncp, vp);
		return (true);
	}

	pvlp = VP2VNODELOCK(vp);
	blp = NCP2BUCKETLOCK(ncp);
	vlp1 = VP2VNODELOCK(ncp->nc_dvp);
	vlp2 = VP2VNODELOCK(ncp->nc_vp);

	if (*vlpp == vlp1 || *vlpp == vlp2) {
		to_unlock = *vlpp;
		*vlpp = NULL;
	} else {
		if (*vlpp != NULL) {
			mtx_unlock(*vlpp);
			*vlpp = NULL;
		}
		cache_sort_vnodes(&vlp1, &vlp2);
		if (vlp1 == pvlp) {
			mtx_lock(vlp2);
			to_unlock = vlp2;
		} else {
			if (!mtx_trylock(vlp1))
				goto out_relock;
			to_unlock = vlp1;
		}
	}
	rw_wlock(blp);
	cache_zap_locked(ncp);
	rw_wunlock(blp);
	if (to_unlock != NULL)
		mtx_unlock(to_unlock);
	return (true);

out_relock:
	mtx_unlock(vlp2);
	mtx_lock(vlp1);
	mtx_lock(vlp2);
	MPASS(*vlpp == NULL);
	*vlpp = vlp1;
	return (false);
}

static int __noinline
cache_zap_locked_vnode(struct namecache *ncp, struct vnode *vp)
{
	struct mtx *pvlp, *vlp1, *vlp2, *to_unlock;
	struct rwlock *blp;
	int error = 0;

	MPASS(vp == ncp->nc_dvp || vp == ncp->nc_vp);
	cache_assert_vnode_locked(vp);

	pvlp = VP2VNODELOCK(vp);
	if (ncp->nc_flag & NCF_NEGATIVE) {
		cache_zap_negative_locked_vnode_kl(ncp, vp);
		goto out;
	}

	blp = NCP2BUCKETLOCK(ncp);
	vlp1 = VP2VNODELOCK(ncp->nc_dvp);
	vlp2 = VP2VNODELOCK(ncp->nc_vp);
	cache_sort_vnodes(&vlp1, &vlp2);
	if (vlp1 == pvlp) {
		mtx_lock(vlp2);
		to_unlock = vlp2;
	} else {
		if (!mtx_trylock(vlp1)) {
			error = EAGAIN;
			goto out;
		}
		to_unlock = vlp1;
	}
	rw_wlock(blp);
	cache_zap_locked(ncp);
	rw_wunlock(blp);
	mtx_unlock(to_unlock);
out:
	mtx_unlock(pvlp);
	return (error);
}

/*
 * If trylocking failed we can get here. We know enough to take all needed locks
 * in the right order and re-lookup the entry.
 */
static int
cache_zap_unlocked_bucket(struct namecache *ncp, struct componentname *cnp,
    struct vnode *dvp, struct mtx *dvlp, struct mtx *vlp, uint32_t hash,
    struct rwlock *blp)
{
	struct namecache *rncp;

	cache_assert_bucket_locked(ncp, RA_UNLOCKED);

	cache_sort_vnodes(&dvlp, &vlp);
	cache_lock_vnodes(dvlp, vlp);
	rw_wlock(blp);
	CK_LIST_FOREACH(rncp, (NCHHASH(hash)), nc_hash) {
		if (rncp == ncp && rncp->nc_dvp == dvp &&
		    rncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(rncp->nc_name, cnp->cn_nameptr, rncp->nc_nlen))
			break;
	}
	if (rncp != NULL) {
		cache_zap_locked(rncp);
		rw_wunlock(blp);
		cache_unlock_vnodes(dvlp, vlp);
		counter_u64_add(zap_and_exit_bucket_relock_success, 1);
		return (0);
	}

	rw_wunlock(blp);
	cache_unlock_vnodes(dvlp, vlp);
	return (EAGAIN);
}

static int __noinline
cache_zap_wlocked_bucket(struct namecache *ncp, struct componentname *cnp,
    uint32_t hash, struct rwlock *blp)
{
	struct mtx *dvlp, *vlp;
	struct vnode *dvp;

	cache_assert_bucket_locked(ncp, RA_WLOCKED);

	dvlp = VP2VNODELOCK(ncp->nc_dvp);
	vlp = NULL;
	if (!(ncp->nc_flag & NCF_NEGATIVE))
		vlp = VP2VNODELOCK(ncp->nc_vp);
	if (cache_trylock_vnodes(dvlp, vlp) == 0) {
		cache_zap_locked(ncp);
		rw_wunlock(blp);
		cache_unlock_vnodes(dvlp, vlp);
		return (0);
	}

	dvp = ncp->nc_dvp;
	rw_wunlock(blp);
	return (cache_zap_unlocked_bucket(ncp, cnp, dvp, dvlp, vlp, hash, blp));
}

static int __noinline
cache_zap_rlocked_bucket(struct namecache *ncp, struct componentname *cnp,
    uint32_t hash, struct rwlock *blp)
{
	struct mtx *dvlp, *vlp;
	struct vnode *dvp;

	cache_assert_bucket_locked(ncp, RA_RLOCKED);

	dvlp = VP2VNODELOCK(ncp->nc_dvp);
	vlp = NULL;
	if (!(ncp->nc_flag & NCF_NEGATIVE))
		vlp = VP2VNODELOCK(ncp->nc_vp);
	if (cache_trylock_vnodes(dvlp, vlp) == 0) {
		rw_runlock(blp);
		rw_wlock(blp);
		cache_zap_locked(ncp);
		rw_wunlock(blp);
		cache_unlock_vnodes(dvlp, vlp);
		return (0);
	}

	dvp = ncp->nc_dvp;
	rw_runlock(blp);
	return (cache_zap_unlocked_bucket(ncp, cnp, dvp, dvlp, vlp, hash, blp));
}

static int
cache_zap_wlocked_bucket_kl(struct namecache *ncp, struct rwlock *blp,
    struct mtx **vlpp1, struct mtx **vlpp2)
{
	struct mtx *dvlp, *vlp;

	cache_assert_bucket_locked(ncp, RA_WLOCKED);

	dvlp = VP2VNODELOCK(ncp->nc_dvp);
	vlp = NULL;
	if (!(ncp->nc_flag & NCF_NEGATIVE))
		vlp = VP2VNODELOCK(ncp->nc_vp);
	cache_sort_vnodes(&dvlp, &vlp);

	if (*vlpp1 == dvlp && *vlpp2 == vlp) {
		cache_zap_locked(ncp);
		cache_unlock_vnodes(dvlp, vlp);
		*vlpp1 = NULL;
		*vlpp2 = NULL;
		return (0);
	}

	if (*vlpp1 != NULL)
		mtx_unlock(*vlpp1);
	if (*vlpp2 != NULL)
		mtx_unlock(*vlpp2);
	*vlpp1 = NULL;
	*vlpp2 = NULL;

	if (cache_trylock_vnodes(dvlp, vlp) == 0) {
		cache_zap_locked(ncp);
		cache_unlock_vnodes(dvlp, vlp);
		return (0);
	}

	rw_wunlock(blp);
	*vlpp1 = dvlp;
	*vlpp2 = vlp;
	if (*vlpp1 != NULL)
		mtx_lock(*vlpp1);
	mtx_lock(*vlpp2);
	rw_wlock(blp);
	return (EAGAIN);
}

static void
cache_lookup_unlock(struct rwlock *blp, struct mtx *vlp)
{

	if (blp != NULL) {
		rw_runlock(blp);
	} else {
		mtx_unlock(vlp);
	}
}

static int __noinline
cache_lookup_dot(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct timespec *tsp, int *ticksp)
{
	int ltype;

	*vpp = dvp;
	CTR2(KTR_VFS, "cache_lookup(%p, %s) found via .",
			dvp, cnp->cn_nameptr);
	counter_u64_add(dothits, 1);
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ".", *vpp);
	if (tsp != NULL)
		timespecclear(tsp);
	if (ticksp != NULL)
		*ticksp = ticks;
	vrefact(*vpp);
	/*
	 * When we lookup "." we still can be asked to lock it
	 * differently...
	 */
	ltype = cnp->cn_lkflags & LK_TYPE_MASK;
	if (ltype != VOP_ISLOCKED(*vpp)) {
		if (ltype == LK_EXCLUSIVE) {
			vn_lock(*vpp, LK_UPGRADE | LK_RETRY);
			if (VN_IS_DOOMED((*vpp))) {
				/* forced unmount */
				vrele(*vpp);
				*vpp = NULL;
				return (ENOENT);
			}
		} else
			vn_lock(*vpp, LK_DOWNGRADE | LK_RETRY);
	}
	return (-1);
}

static __noinline int
cache_lookup_nomakeentry(struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, struct timespec *tsp, int *ticksp)
{
	struct namecache *ncp;
	struct rwlock *blp;
	struct mtx *dvlp, *dvlp2;
	uint32_t hash;
	int error;

	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[0] == '.' && cnp->cn_nameptr[1] == '.') {
		counter_u64_add(dotdothits, 1);
		dvlp = VP2VNODELOCK(dvp);
		dvlp2 = NULL;
		mtx_lock(dvlp);
retry_dotdot:
		ncp = dvp->v_cache_dd;
		if (ncp == NULL) {
			SDT_PROBE3(vfs, namecache, lookup, miss, dvp,
			    "..", NULL);
			mtx_unlock(dvlp);
			if (dvlp2 != NULL)
				mtx_unlock(dvlp2);
			return (0);
		}
		if ((ncp->nc_flag & NCF_ISDOTDOT) != 0) {
			if (ncp->nc_dvp != dvp)
				panic("dvp %p v_cache_dd %p\n", dvp, ncp);
			if (!cache_zap_locked_vnode_kl2(ncp,
			    dvp, &dvlp2))
				goto retry_dotdot;
			MPASS(dvp->v_cache_dd == NULL);
			mtx_unlock(dvlp);
			if (dvlp2 != NULL)
				mtx_unlock(dvlp2);
			cache_free(ncp);
		} else {
			dvp->v_cache_dd = NULL;
			mtx_unlock(dvlp);
			if (dvlp2 != NULL)
				mtx_unlock(dvlp2);
		}
		return (0);
	}

	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);
	blp = HASH2BUCKETLOCK(hash);
retry:
	if (CK_LIST_EMPTY(NCHHASH(hash)))
		goto out_no_entry;

	rw_wlock(blp);

	CK_LIST_FOREACH(ncp, (NCHHASH(hash)), nc_hash) {
		if (ncp->nc_dvp == dvp && ncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(ncp->nc_name, cnp->cn_nameptr, ncp->nc_nlen))
			break;
	}

	/* We failed to find an entry */
	if (ncp == NULL) {
		rw_wunlock(blp);
		goto out_no_entry;
	}

	error = cache_zap_wlocked_bucket(ncp, cnp, hash, blp);
	if (__predict_false(error != 0)) {
		zap_and_exit_bucket_fail++;
		cache_maybe_yield();
		goto retry;
	}
	counter_u64_add(numposzaps, 1);
	cache_free(ncp);
	return (0);
out_no_entry:
	SDT_PROBE3(vfs, namecache, lookup, miss, dvp, cnp->cn_nameptr, NULL);
	counter_u64_add(nummisszap, 1);
	return (0);
}

/**
 * Lookup a name in the name cache
 *
 * # Arguments
 *
 * - dvp:	Parent directory in which to search.
 * - vpp:	Return argument.  Will contain desired vnode on cache hit.
 * - cnp:	Parameters of the name search.  The most interesting bits of
 *   		the cn_flags field have the following meanings:
 *   	- MAKEENTRY:	If clear, free an entry from the cache rather than look
 *   			it up.
 *   	- ISDOTDOT:	Must be set if and only if cn_nameptr == ".."
 * - tsp:	Return storage for cache timestamp.  On a successful (positive
 *   		or negative) lookup, tsp will be filled with any timespec that
 *   		was stored when this cache entry was created.  However, it will
 *   		be clear for "." entries.
 * - ticks:	Return storage for alternate cache timestamp.  On a successful
 *   		(positive or negative) lookup, it will contain the ticks value
 *   		that was current when the cache entry was created, unless cnp
 *   		was ".".
 *
 * # Returns
 *
 * - -1:	A positive cache hit.  vpp will contain the desired vnode.
 * - ENOENT:	A negative cache hit, or dvp was recycled out from under us due
 *		to a forced unmount.  vpp will not be modified.  If the entry
 *		is a whiteout, then the ISWHITEOUT flag will be set in
 *		cnp->cn_flags.
 * - 0:		A cache miss.  vpp will not be modified.
 *
 * # Locking
 *
 * On a cache hit, vpp will be returned locked and ref'd.  If we're looking up
 * .., dvp is unlocked.  If we're looking up . an extra ref is taken, but the
 * lock is not recursively acquired.
 */
int
cache_lookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct timespec *tsp, int *ticksp)
{
	struct namecache_ts *ncp_ts;
	struct namecache *ncp;
	struct negstate *negstate;
	struct rwlock *blp;
	struct mtx *dvlp;
	uint32_t hash;
	enum vgetstate vs;
	int error, ltype;
	bool try_smr, doing_smr, whiteout;

#ifdef DEBUG_CACHE
	if (__predict_false(!doingcache)) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}
#endif

	if (__predict_false(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.'))
		return (cache_lookup_dot(dvp, vpp, cnp, tsp, ticksp));

	if ((cnp->cn_flags & MAKEENTRY) == 0)
		return (cache_lookup_nomakeentry(dvp, vpp, cnp, tsp, ticksp));

	try_smr = true;
	if (cnp->cn_nameiop == CREATE)
		try_smr = false;
retry:
	doing_smr = false;
	blp = NULL;
	dvlp = NULL;
	error = 0;
	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[0] == '.' && cnp->cn_nameptr[1] == '.') {
		counter_u64_add(dotdothits, 1);
		dvlp = VP2VNODELOCK(dvp);
		mtx_lock(dvlp);
		ncp = dvp->v_cache_dd;
		if (ncp == NULL) {
			SDT_PROBE3(vfs, namecache, lookup, miss, dvp,
			    "..", NULL);
			mtx_unlock(dvlp);
			return (0);
		}
		if ((ncp->nc_flag & NCF_ISDOTDOT) != 0) {
			if (ncp->nc_flag & NCF_NEGATIVE)
				*vpp = NULL;
			else
				*vpp = ncp->nc_vp;
		} else
			*vpp = ncp->nc_dvp;
		/* Return failure if negative entry was found. */
		if (*vpp == NULL)
			goto negative_success;
		CTR3(KTR_VFS, "cache_lookup(%p, %s) found %p via ..",
		    dvp, cnp->cn_nameptr, *vpp);
		SDT_PROBE3(vfs, namecache, lookup, hit, dvp, "..",
		    *vpp);
		cache_out_ts(ncp, tsp, ticksp);
		if ((ncp->nc_flag & (NCF_ISDOTDOT | NCF_DTS)) ==
		    NCF_DTS && tsp != NULL) {
			ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
			*tsp = ncp_ts->nc_dotdottime;
		}
		goto success;
	}

	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);
retry_hashed:
	if (try_smr) {
		vfs_smr_enter();
		doing_smr = true;
		try_smr = false;
	} else {
		blp = HASH2BUCKETLOCK(hash);
		rw_rlock(blp);
	}

	CK_LIST_FOREACH(ncp, (NCHHASH(hash)), nc_hash) {
		if (ncp->nc_dvp == dvp && ncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(ncp->nc_name, cnp->cn_nameptr, ncp->nc_nlen))
			break;
	}

	/* We failed to find an entry */
	if (__predict_false(ncp == NULL)) {
		if (doing_smr)
			vfs_smr_exit();
		else
			rw_runlock(blp);
		SDT_PROBE3(vfs, namecache, lookup, miss, dvp, cnp->cn_nameptr,
		    NULL);
		counter_u64_add(nummiss, 1);
		return (0);
	}

	if (ncp->nc_flag & NCF_NEGATIVE)
		goto negative_success;

	/* We found a "positive" match, return the vnode */
	counter_u64_add(numposhits, 1);
	*vpp = ncp->nc_vp;
	CTR4(KTR_VFS, "cache_lookup(%p, %s) found %p via ncp %p",
	    dvp, cnp->cn_nameptr, *vpp, ncp);
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ncp->nc_name,
	    *vpp);
	cache_out_ts(ncp, tsp, ticksp);
success:
	/*
	 * On success we return a locked and ref'd vnode as per the lookup
	 * protocol.
	 */
	MPASS(dvp != *vpp);
	ltype = 0;	/* silence gcc warning */
	if (cnp->cn_flags & ISDOTDOT) {
		ltype = VOP_ISLOCKED(dvp);
		VOP_UNLOCK(dvp);
	}
	if (doing_smr) {
		if (cache_ncp_invalid(ncp)) {
			vfs_smr_exit();
			*vpp = NULL;
			goto retry;
		}
		vs = vget_prep_smr(*vpp);
		vfs_smr_exit();
		if (vs == VGET_NONE) {
			*vpp = NULL;
			goto retry;
		}
	} else {
		vs = vget_prep(*vpp);
		cache_lookup_unlock(blp, dvlp);
	}
	error = vget_finish(*vpp, cnp->cn_lkflags, vs);
	if (cnp->cn_flags & ISDOTDOT) {
		vn_lock(dvp, ltype | LK_RETRY);
		if (VN_IS_DOOMED(dvp)) {
			if (error == 0)
				vput(*vpp);
			*vpp = NULL;
			return (ENOENT);
		}
	}
	if (error) {
		*vpp = NULL;
		goto retry;
	}
	if ((cnp->cn_flags & ISLASTCN) &&
	    (cnp->cn_lkflags & LK_TYPE_MASK) == LK_EXCLUSIVE) {
		ASSERT_VOP_ELOCKED(*vpp, "cache_lookup");
	}
	return (-1);

negative_success:
	/* We found a negative match, and want to create it, so purge */
	if (cnp->cn_nameiop == CREATE) {
		MPASS(!doing_smr);
		counter_u64_add(numnegzaps, 1);
		goto zap_and_exit;
	}

	SDT_PROBE2(vfs, namecache, lookup, hit__negative, dvp, ncp->nc_name);
	cache_out_ts(ncp, tsp, ticksp);
	counter_u64_add(numneghits, 1);
	whiteout = (ncp->nc_flag & NCF_WHITE);

	if (doing_smr) {
		/*
		 * We need to take locks to promote an entry.
		 */
		negstate = NCP2NEGSTATE(ncp);
		if ((negstate->neg_flag & NEG_HOT) == 0 ||
		    cache_ncp_invalid(ncp)) {
			vfs_smr_exit();
			doing_smr = false;
			goto retry_hashed;
		}
		vfs_smr_exit();
	} else {
		cache_negative_hit(ncp);
		cache_lookup_unlock(blp, dvlp);
	}
	if (whiteout)
		cnp->cn_flags |= ISWHITEOUT;
	return (ENOENT);

zap_and_exit:
	MPASS(!doing_smr);
	if (blp != NULL)
		error = cache_zap_rlocked_bucket(ncp, cnp, hash, blp);
	else
		error = cache_zap_locked_vnode(ncp, dvp);
	if (__predict_false(error != 0)) {
		zap_and_exit_bucket_fail2++;
		cache_maybe_yield();
		goto retry;
	}
	cache_free(ncp);
	return (0);
}

struct celockstate {
	struct mtx *vlp[3];
	struct rwlock *blp[2];
};
CTASSERT((nitems(((struct celockstate *)0)->vlp) == 3));
CTASSERT((nitems(((struct celockstate *)0)->blp) == 2));

static inline void
cache_celockstate_init(struct celockstate *cel)
{

	bzero(cel, sizeof(*cel));
}

static void
cache_lock_vnodes_cel(struct celockstate *cel, struct vnode *vp,
    struct vnode *dvp)
{
	struct mtx *vlp1, *vlp2;

	MPASS(cel->vlp[0] == NULL);
	MPASS(cel->vlp[1] == NULL);
	MPASS(cel->vlp[2] == NULL);

	MPASS(vp != NULL || dvp != NULL);

	vlp1 = VP2VNODELOCK(vp);
	vlp2 = VP2VNODELOCK(dvp);
	cache_sort_vnodes(&vlp1, &vlp2);

	if (vlp1 != NULL) {
		mtx_lock(vlp1);
		cel->vlp[0] = vlp1;
	}
	mtx_lock(vlp2);
	cel->vlp[1] = vlp2;
}

static void
cache_unlock_vnodes_cel(struct celockstate *cel)
{

	MPASS(cel->vlp[0] != NULL || cel->vlp[1] != NULL);

	if (cel->vlp[0] != NULL)
		mtx_unlock(cel->vlp[0]);
	if (cel->vlp[1] != NULL)
		mtx_unlock(cel->vlp[1]);
	if (cel->vlp[2] != NULL)
		mtx_unlock(cel->vlp[2]);
}

static bool
cache_lock_vnodes_cel_3(struct celockstate *cel, struct vnode *vp)
{
	struct mtx *vlp;
	bool ret;

	cache_assert_vlp_locked(cel->vlp[0]);
	cache_assert_vlp_locked(cel->vlp[1]);
	MPASS(cel->vlp[2] == NULL);

	MPASS(vp != NULL);
	vlp = VP2VNODELOCK(vp);

	ret = true;
	if (vlp >= cel->vlp[1]) {
		mtx_lock(vlp);
	} else {
		if (mtx_trylock(vlp))
			goto out;
		cache_lock_vnodes_cel_3_failures++;
		cache_unlock_vnodes_cel(cel);
		if (vlp < cel->vlp[0]) {
			mtx_lock(vlp);
			mtx_lock(cel->vlp[0]);
			mtx_lock(cel->vlp[1]);
		} else {
			if (cel->vlp[0] != NULL)
				mtx_lock(cel->vlp[0]);
			mtx_lock(vlp);
			mtx_lock(cel->vlp[1]);
		}
		ret = false;
	}
out:
	cel->vlp[2] = vlp;
	return (ret);
}

static void
cache_lock_buckets_cel(struct celockstate *cel, struct rwlock *blp1,
    struct rwlock *blp2)
{

	MPASS(cel->blp[0] == NULL);
	MPASS(cel->blp[1] == NULL);

	cache_sort_vnodes(&blp1, &blp2);

	if (blp1 != NULL) {
		rw_wlock(blp1);
		cel->blp[0] = blp1;
	}
	rw_wlock(blp2);
	cel->blp[1] = blp2;
}

static void
cache_unlock_buckets_cel(struct celockstate *cel)
{

	if (cel->blp[0] != NULL)
		rw_wunlock(cel->blp[0]);
	rw_wunlock(cel->blp[1]);
}

/*
 * Lock part of the cache affected by the insertion.
 *
 * This means vnodelocks for dvp, vp and the relevant bucketlock.
 * However, insertion can result in removal of an old entry. In this
 * case we have an additional vnode and bucketlock pair to lock. If the
 * entry is negative, ncelock is locked instead of the vnode.
 *
 * That is, in the worst case we have to lock 3 vnodes and 2 bucketlocks, while
 * preserving the locking order (smaller address first).
 */
static void
cache_enter_lock(struct celockstate *cel, struct vnode *dvp, struct vnode *vp,
    uint32_t hash)
{
	struct namecache *ncp;
	struct rwlock *blps[2];

	blps[0] = HASH2BUCKETLOCK(hash);
	for (;;) {
		blps[1] = NULL;
		cache_lock_vnodes_cel(cel, dvp, vp);
		if (vp == NULL || vp->v_type != VDIR)
			break;
		ncp = vp->v_cache_dd;
		if (ncp == NULL)
			break;
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			break;
		MPASS(ncp->nc_dvp == vp);
		blps[1] = NCP2BUCKETLOCK(ncp);
		if (ncp->nc_flag & NCF_NEGATIVE)
			break;
		if (cache_lock_vnodes_cel_3(cel, ncp->nc_vp))
			break;
		/*
		 * All vnodes got re-locked. Re-validate the state and if
		 * nothing changed we are done. Otherwise restart.
		 */
		if (ncp == vp->v_cache_dd &&
		    (ncp->nc_flag & NCF_ISDOTDOT) != 0 &&
		    blps[1] == NCP2BUCKETLOCK(ncp) &&
		    VP2VNODELOCK(ncp->nc_vp) == cel->vlp[2])
			break;
		cache_unlock_vnodes_cel(cel);
		cel->vlp[0] = NULL;
		cel->vlp[1] = NULL;
		cel->vlp[2] = NULL;
	}
	cache_lock_buckets_cel(cel, blps[0], blps[1]);
}

static void
cache_enter_lock_dd(struct celockstate *cel, struct vnode *dvp, struct vnode *vp,
    uint32_t hash)
{
	struct namecache *ncp;
	struct rwlock *blps[2];

	blps[0] = HASH2BUCKETLOCK(hash);
	for (;;) {
		blps[1] = NULL;
		cache_lock_vnodes_cel(cel, dvp, vp);
		ncp = dvp->v_cache_dd;
		if (ncp == NULL)
			break;
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			break;
		MPASS(ncp->nc_dvp == dvp);
		blps[1] = NCP2BUCKETLOCK(ncp);
		if (ncp->nc_flag & NCF_NEGATIVE)
			break;
		if (cache_lock_vnodes_cel_3(cel, ncp->nc_vp))
			break;
		if (ncp == dvp->v_cache_dd &&
		    (ncp->nc_flag & NCF_ISDOTDOT) != 0 &&
		    blps[1] == NCP2BUCKETLOCK(ncp) &&
		    VP2VNODELOCK(ncp->nc_vp) == cel->vlp[2])
			break;
		cache_unlock_vnodes_cel(cel);
		cel->vlp[0] = NULL;
		cel->vlp[1] = NULL;
		cel->vlp[2] = NULL;
	}
	cache_lock_buckets_cel(cel, blps[0], blps[1]);
}

static void
cache_enter_unlock(struct celockstate *cel)
{

	cache_unlock_buckets_cel(cel);
	cache_unlock_vnodes_cel(cel);
}

static void __noinline
cache_enter_dotdot_prep(struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp)
{
	struct celockstate cel;
	struct namecache *ncp;
	uint32_t hash;
	int len;

	if (dvp->v_cache_dd == NULL)
		return;
	len = cnp->cn_namelen;
	cache_celockstate_init(&cel);
	hash = cache_get_hash(cnp->cn_nameptr, len, dvp);
	cache_enter_lock_dd(&cel, dvp, vp, hash);
	ncp = dvp->v_cache_dd;
	if (ncp != NULL && (ncp->nc_flag & NCF_ISDOTDOT)) {
		KASSERT(ncp->nc_dvp == dvp, ("wrong isdotdot parent"));
		cache_zap_locked(ncp);
	} else {
		ncp = NULL;
	}
	dvp->v_cache_dd = NULL;
	cache_enter_unlock(&cel);
	cache_free(ncp);
}

/*
 * Add an entry to the cache.
 */
void
cache_enter_time(struct vnode *dvp, struct vnode *vp, struct componentname *cnp,
    struct timespec *tsp, struct timespec *dtsp)
{
	struct celockstate cel;
	struct namecache *ncp, *n2, *ndd;
	struct namecache_ts *ncp_ts, *n2_ts;
	struct nchashhead *ncpp;
	uint32_t hash;
	int flag;
	int len;
	u_long lnumcache;

	CTR3(KTR_VFS, "cache_enter(%p, %p, %s)", dvp, vp, cnp->cn_nameptr);
	VNASSERT(vp == NULL || !VN_IS_DOOMED(vp), vp,
	    ("cache_enter: Adding a doomed vnode"));
	VNASSERT(dvp == NULL || !VN_IS_DOOMED(dvp), dvp,
	    ("cache_enter: Doomed vnode used as src"));

#ifdef DEBUG_CACHE
	if (__predict_false(!doingcache))
		return;
#endif

	flag = 0;
	if (__predict_false(cnp->cn_nameptr[0] == '.')) {
		if (cnp->cn_namelen == 1)
			return;
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			cache_enter_dotdot_prep(dvp, vp, cnp);
			flag = NCF_ISDOTDOT;
		}
	}

	/*
	 * Avoid blowout in namecache entries.
	 */
	lnumcache = atomic_fetchadd_long(&numcache, 1) + 1;
	if (__predict_false(lnumcache >= ncsize)) {
		atomic_add_long(&numcache, -1);
		counter_u64_add(numdrops, 1);
		return;
	}

	cache_celockstate_init(&cel);
	ndd = NULL;
	ncp_ts = NULL;

	/*
	 * Calculate the hash key and setup as much of the new
	 * namecache entry as possible before acquiring the lock.
	 */
	ncp = cache_alloc(cnp->cn_namelen, tsp != NULL);
	ncp->nc_flag = flag;
	ncp->nc_vp = vp;
	if (vp == NULL)
		cache_negative_init(ncp);
	ncp->nc_dvp = dvp;
	if (tsp != NULL) {
		ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
		ncp_ts->nc_time = *tsp;
		ncp_ts->nc_ticks = ticks;
		ncp_ts->nc_nc.nc_flag |= NCF_TS;
		if (dtsp != NULL) {
			ncp_ts->nc_dotdottime = *dtsp;
			ncp_ts->nc_nc.nc_flag |= NCF_DTS;
		}
	}
	len = ncp->nc_nlen = cnp->cn_namelen;
	hash = cache_get_hash(cnp->cn_nameptr, len, dvp);
	strlcpy(ncp->nc_name, cnp->cn_nameptr, len + 1);
	cache_enter_lock(&cel, dvp, vp, hash);

	/*
	 * See if this vnode or negative entry is already in the cache
	 * with this name.  This can happen with concurrent lookups of
	 * the same path name.
	 */
	ncpp = NCHHASH(hash);
	CK_LIST_FOREACH(n2, ncpp, nc_hash) {
		if (n2->nc_dvp == dvp &&
		    n2->nc_nlen == cnp->cn_namelen &&
		    !bcmp(n2->nc_name, cnp->cn_nameptr, n2->nc_nlen)) {
			if (tsp != NULL) {
				KASSERT((n2->nc_flag & NCF_TS) != 0,
				    ("no NCF_TS"));
				n2_ts = __containerof(n2, struct namecache_ts, nc_nc);
				n2_ts->nc_time = ncp_ts->nc_time;
				n2_ts->nc_ticks = ncp_ts->nc_ticks;
				if (dtsp != NULL) {
					n2_ts->nc_dotdottime = ncp_ts->nc_dotdottime;
					n2_ts->nc_nc.nc_flag |= NCF_DTS;
				}
			}
			goto out_unlock_free;
		}
	}

	if (flag == NCF_ISDOTDOT) {
		/*
		 * See if we are trying to add .. entry, but some other lookup
		 * has populated v_cache_dd pointer already.
		 */
		if (dvp->v_cache_dd != NULL)
			goto out_unlock_free;
		KASSERT(vp == NULL || vp->v_type == VDIR,
		    ("wrong vnode type %p", vp));
		dvp->v_cache_dd = ncp;
	}

	if (vp != NULL) {
		if (vp->v_type == VDIR) {
			if (flag != NCF_ISDOTDOT) {
				/*
				 * For this case, the cache entry maps both the
				 * directory name in it and the name ".." for the
				 * directory's parent.
				 */
				if ((ndd = vp->v_cache_dd) != NULL) {
					if ((ndd->nc_flag & NCF_ISDOTDOT) != 0)
						cache_zap_locked(ndd);
					else
						ndd = NULL;
				}
				vp->v_cache_dd = ncp;
			}
		} else {
			vp->v_cache_dd = NULL;
		}
	}

	if (flag != NCF_ISDOTDOT) {
		if (LIST_EMPTY(&dvp->v_cache_src)) {
			vhold(dvp);
			counter_u64_add(numcachehv, 1);
		}
		LIST_INSERT_HEAD(&dvp->v_cache_src, ncp, nc_src);
	}

	/*
	 * If the entry is "negative", we place it into the
	 * "negative" cache queue, otherwise, we place it into the
	 * destination vnode's cache entries queue.
	 */
	if (vp != NULL) {
		TAILQ_INSERT_HEAD(&vp->v_cache_dst, ncp, nc_dst);
		SDT_PROBE3(vfs, namecache, enter, done, dvp, ncp->nc_name,
		    vp);
	} else {
		if (cnp->cn_flags & ISWHITEOUT)
			ncp->nc_flag |= NCF_WHITE;
		cache_negative_insert(ncp);
		SDT_PROBE2(vfs, namecache, enter_negative, done, dvp,
		    ncp->nc_name);
	}

	atomic_thread_fence_rel();
	/*
	 * Insert the new namecache entry into the appropriate chain
	 * within the cache entries table.
	 */
	CK_LIST_INSERT_HEAD(ncpp, ncp, nc_hash);

	cache_enter_unlock(&cel);
	if (numneg * ncnegfactor > lnumcache)
		cache_negative_zap_one();
	cache_free(ndd);
	return;
out_unlock_free:
	cache_enter_unlock(&cel);
	cache_free(ncp);
	return;
}

static u_int
cache_roundup_2(u_int val)
{
	u_int res;

	for (res = 1; res <= val; res <<= 1)
		continue;

	return (res);
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */
static void
nchinit(void *dummy __unused)
{
	u_int i;

	cache_zone_small = uma_zcreate("S VFS Cache",
	    sizeof(struct namecache) + CACHE_PATH_CUTOFF + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGNOF(struct namecache),
	    UMA_ZONE_ZINIT);
	cache_zone_small_ts = uma_zcreate("STS VFS Cache",
	    sizeof(struct namecache_ts) + CACHE_PATH_CUTOFF + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGNOF(struct namecache_ts),
	    UMA_ZONE_ZINIT);
	cache_zone_large = uma_zcreate("L VFS Cache",
	    sizeof(struct namecache) + NAME_MAX + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGNOF(struct namecache),
	    UMA_ZONE_ZINIT);
	cache_zone_large_ts = uma_zcreate("LTS VFS Cache",
	    sizeof(struct namecache_ts) + NAME_MAX + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGNOF(struct namecache_ts),
	    UMA_ZONE_ZINIT);

	VFS_SMR_ZONE_SET(cache_zone_small);
	VFS_SMR_ZONE_SET(cache_zone_small_ts);
	VFS_SMR_ZONE_SET(cache_zone_large);
	VFS_SMR_ZONE_SET(cache_zone_large_ts);

	ncsize = desiredvnodes * ncsizefactor;
	nchashtbl = hashinit(desiredvnodes * 2, M_VFSCACHE, &nchash);
	ncbuckethash = cache_roundup_2(mp_ncpus * mp_ncpus) - 1;
	if (ncbuckethash < 7) /* arbitrarily chosen to avoid having one lock */
		ncbuckethash = 7;
	if (ncbuckethash > nchash)
		ncbuckethash = nchash;
	bucketlocks = malloc(sizeof(*bucketlocks) * numbucketlocks, M_VFSCACHE,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < numbucketlocks; i++)
		rw_init_flags(&bucketlocks[i], "ncbuc", RW_DUPOK | RW_RECURSE);
	ncvnodehash = ncbuckethash;
	vnodelocks = malloc(sizeof(*vnodelocks) * numvnodelocks, M_VFSCACHE,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < numvnodelocks; i++)
		mtx_init(&vnodelocks[i], "ncvn", NULL, MTX_DUPOK | MTX_RECURSE);
	ncpurgeminvnodes = numbucketlocks * 2;

	ncneghash = 3;
	neglists = malloc(sizeof(*neglists) * numneglists, M_VFSCACHE,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < numneglists; i++) {
		mtx_init(&neglists[i].nl_lock, "ncnegl", NULL, MTX_DEF);
		TAILQ_INIT(&neglists[i].nl_list);
	}
	mtx_init(&ncneg_hot.nl_lock, "ncneglh", NULL, MTX_DEF);
	TAILQ_INIT(&ncneg_hot.nl_list);

	mtx_init(&ncneg_shrink_lock, "ncnegs", NULL, MTX_DEF);
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_SECOND, nchinit, NULL);

void
cache_vnode_init(struct vnode *vp)
{

	LIST_INIT(&vp->v_cache_src);
	TAILQ_INIT(&vp->v_cache_dst);
	vp->v_cache_dd = NULL;
	cache_prehash(vp);
}

void
cache_changesize(u_long newmaxvnodes)
{
	struct nchashhead *new_nchashtbl, *old_nchashtbl;
	u_long new_nchash, old_nchash;
	struct namecache *ncp;
	uint32_t hash;
	u_long newncsize;
	int i;

	newncsize = newmaxvnodes * ncsizefactor;
	newmaxvnodes = cache_roundup_2(newmaxvnodes * 2);
	if (newmaxvnodes < numbucketlocks)
		newmaxvnodes = numbucketlocks;

	new_nchashtbl = hashinit(newmaxvnodes, M_VFSCACHE, &new_nchash);
	/* If same hash table size, nothing to do */
	if (nchash == new_nchash) {
		free(new_nchashtbl, M_VFSCACHE);
		return;
	}
	/*
	 * Move everything from the old hash table to the new table.
	 * None of the namecache entries in the table can be removed
	 * because to do so, they have to be removed from the hash table.
	 */
	cache_lock_all_vnodes();
	cache_lock_all_buckets();
	old_nchashtbl = nchashtbl;
	old_nchash = nchash;
	nchashtbl = new_nchashtbl;
	nchash = new_nchash;
	for (i = 0; i <= old_nchash; i++) {
		while ((ncp = CK_LIST_FIRST(&old_nchashtbl[i])) != NULL) {
			hash = cache_get_hash(ncp->nc_name, ncp->nc_nlen,
			    ncp->nc_dvp);
			CK_LIST_REMOVE(ncp, nc_hash);
			CK_LIST_INSERT_HEAD(NCHHASH(hash), ncp, nc_hash);
		}
	}
	ncsize = newncsize;
	cache_unlock_all_buckets();
	cache_unlock_all_vnodes();
	free(old_nchashtbl, M_VFSCACHE);
}

/*
 * Invalidate all entries from and to a particular vnode.
 */
void
cache_purge(struct vnode *vp)
{
	TAILQ_HEAD(, namecache) ncps;
	struct namecache *ncp, *nnp;
	struct mtx *vlp, *vlp2;

	CTR1(KTR_VFS, "cache_purge(%p)", vp);
	SDT_PROBE1(vfs, namecache, purge, done, vp);
	if (LIST_EMPTY(&vp->v_cache_src) && TAILQ_EMPTY(&vp->v_cache_dst) &&
	    vp->v_cache_dd == NULL)
		return;
	TAILQ_INIT(&ncps);
	vlp = VP2VNODELOCK(vp);
	vlp2 = NULL;
	mtx_lock(vlp);
retry:
	while (!LIST_EMPTY(&vp->v_cache_src)) {
		ncp = LIST_FIRST(&vp->v_cache_src);
		if (!cache_zap_locked_vnode_kl2(ncp, vp, &vlp2))
			goto retry;
		TAILQ_INSERT_TAIL(&ncps, ncp, nc_dst);
	}
	while (!TAILQ_EMPTY(&vp->v_cache_dst)) {
		ncp = TAILQ_FIRST(&vp->v_cache_dst);
		if (!cache_zap_locked_vnode_kl2(ncp, vp, &vlp2))
			goto retry;
		TAILQ_INSERT_TAIL(&ncps, ncp, nc_dst);
	}
	ncp = vp->v_cache_dd;
	if (ncp != NULL) {
		KASSERT(ncp->nc_flag & NCF_ISDOTDOT,
		   ("lost dotdot link"));
		if (!cache_zap_locked_vnode_kl2(ncp, vp, &vlp2))
			goto retry;
		TAILQ_INSERT_TAIL(&ncps, ncp, nc_dst);
	}
	KASSERT(vp->v_cache_dd == NULL, ("incomplete purge"));
	mtx_unlock(vlp);
	if (vlp2 != NULL)
		mtx_unlock(vlp2);
	TAILQ_FOREACH_SAFE(ncp, &ncps, nc_dst, nnp) {
		cache_free(ncp);
	}
}

/*
 * Invalidate all negative entries for a particular directory vnode.
 */
void
cache_purge_negative(struct vnode *vp)
{
	TAILQ_HEAD(, namecache) ncps;
	struct namecache *ncp, *nnp;
	struct mtx *vlp;

	CTR1(KTR_VFS, "cache_purge_negative(%p)", vp);
	SDT_PROBE1(vfs, namecache, purge_negative, done, vp);
	if (LIST_EMPTY(&vp->v_cache_src))
		return;
	TAILQ_INIT(&ncps);
	vlp = VP2VNODELOCK(vp);
	mtx_lock(vlp);
	LIST_FOREACH_SAFE(ncp, &vp->v_cache_src, nc_src, nnp) {
		if (!(ncp->nc_flag & NCF_NEGATIVE))
			continue;
		cache_zap_negative_locked_vnode_kl(ncp, vp);
		TAILQ_INSERT_TAIL(&ncps, ncp, nc_dst);
	}
	mtx_unlock(vlp);
	TAILQ_FOREACH_SAFE(ncp, &ncps, nc_dst, nnp) {
		cache_free(ncp);
	}
}

/*
 * Flush all entries referencing a particular filesystem.
 */
void
cache_purgevfs(struct mount *mp, bool force)
{
	TAILQ_HEAD(, namecache) ncps;
	struct mtx *vlp1, *vlp2;
	struct rwlock *blp;
	struct nchashhead *bucket;
	struct namecache *ncp, *nnp;
	u_long i, j, n_nchash;
	int error;

	/* Scan hash tables for applicable entries */
	SDT_PROBE1(vfs, namecache, purgevfs, done, mp);
	if (!force && mp->mnt_nvnodelistsize <= ncpurgeminvnodes)
		return;
	TAILQ_INIT(&ncps);
	n_nchash = nchash + 1;
	vlp1 = vlp2 = NULL;
	for (i = 0; i < numbucketlocks; i++) {
		blp = (struct rwlock *)&bucketlocks[i];
		rw_wlock(blp);
		for (j = i; j < n_nchash; j += numbucketlocks) {
retry:
			bucket = &nchashtbl[j];
			CK_LIST_FOREACH_SAFE(ncp, bucket, nc_hash, nnp) {
				cache_assert_bucket_locked(ncp, RA_WLOCKED);
				if (ncp->nc_dvp->v_mount != mp)
					continue;
				error = cache_zap_wlocked_bucket_kl(ncp, blp,
				    &vlp1, &vlp2);
				if (error != 0)
					goto retry;
				TAILQ_INSERT_HEAD(&ncps, ncp, nc_dst);
			}
		}
		rw_wunlock(blp);
		if (vlp1 == NULL && vlp2 == NULL)
			cache_maybe_yield();
	}
	if (vlp1 != NULL)
		mtx_unlock(vlp1);
	if (vlp2 != NULL)
		mtx_unlock(vlp2);

	TAILQ_FOREACH_SAFE(ncp, &ncps, nc_dst, nnp) {
		cache_free(ncp);
	}
}

/*
 * Perform canonical checks and cache lookup and pass on to filesystem
 * through the vop_cachedlookup only if needed.
 */

int
vfs_cache_lookup(struct vop_lookup_args *ap)
{
	struct vnode *dvp;
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	int flags = cnp->cn_flags;

	*vpp = NULL;
	dvp = ap->a_dvp;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	error = vn_dir_check_exec(dvp, cnp);
	if (error != 0)
		return (error);

	error = cache_lookup(dvp, vpp, cnp, NULL, NULL);
	if (error == 0)
		return (VOP_CACHEDLOOKUP(dvp, vpp, cnp));
	if (error == -1)
		return (0);
	return (error);
}

/* Implementation of the getcwd syscall. */
int
sys___getcwd(struct thread *td, struct __getcwd_args *uap)
{
	char *buf, *retbuf;
	size_t buflen;
	int error;

	buflen = uap->buflen;
	if (__predict_false(buflen < 2))
		return (EINVAL);
	if (buflen > MAXPATHLEN)
		buflen = MAXPATHLEN;

	buf = malloc(buflen, M_TEMP, M_WAITOK);
	error = vn_getcwd(td, buf, &retbuf, &buflen);
	if (error == 0)
		error = copyout(retbuf, uap->buf, buflen);
	free(buf, M_TEMP);
	return (error);
}

int
vn_getcwd(struct thread *td, char *buf, char **retbuf, size_t *buflen)
{
	struct pwd *pwd;
	int error;

	pwd = pwd_hold(td);
	error = vn_fullpath_any(td, pwd->pwd_cdir, pwd->pwd_rdir, buf, retbuf, buflen);
	pwd_drop(pwd);

#ifdef KTRACE
	if (KTRPOINT(curthread, KTR_NAMEI) && error == 0)
		ktrnamei(*retbuf);
#endif
	return (error);
}

static int
kern___realpathat(struct thread *td, int fd, const char *path, char *buf,
    size_t size, int flags, enum uio_seg pathseg)
{
	struct nameidata nd;
	char *retbuf, *freebuf;
	int error;

	if (flags != 0)
		return (EINVAL);
	NDINIT_ATRIGHTS(&nd, LOOKUP, FOLLOW | SAVENAME | WANTPARENT | AUDITVNODE1,
	    pathseg, path, fd, &cap_fstat_rights, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_fullpath_hardlink(td, &nd, &retbuf, &freebuf, &size);
	if (error == 0) {
		error = copyout(retbuf, buf, size);
		free(freebuf, M_TEMP);
	}
	NDFREE(&nd, 0);
	return (error);
}

int
sys___realpathat(struct thread *td, struct __realpathat_args *uap)
{

	return (kern___realpathat(td, uap->fd, uap->path, uap->buf, uap->size,
	    uap->flags, UIO_USERSPACE));
}

/*
 * Retrieve the full filesystem path that correspond to a vnode from the name
 * cache (if available)
 */
int
vn_fullpath(struct thread *td, struct vnode *vn, char **retbuf, char **freebuf)
{
	struct pwd *pwd;
	char *buf;
	size_t buflen;
	int error;

	if (__predict_false(vn == NULL))
		return (EINVAL);

	buflen = MAXPATHLEN;
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	pwd = pwd_hold(td);
	error = vn_fullpath_any(td, vn, pwd->pwd_rdir, buf, retbuf, &buflen);
	pwd_drop(pwd);

	if (!error)
		*freebuf = buf;
	else
		free(buf, M_TEMP);
	return (error);
}

/*
 * This function is similar to vn_fullpath, but it attempts to lookup the
 * pathname relative to the global root mount point.  This is required for the
 * auditing sub-system, as audited pathnames must be absolute, relative to the
 * global root mount point.
 */
int
vn_fullpath_global(struct thread *td, struct vnode *vn,
    char **retbuf, char **freebuf)
{
	char *buf;
	size_t buflen;
	int error;

	if (__predict_false(vn == NULL))
		return (EINVAL);
	buflen = MAXPATHLEN;
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	error = vn_fullpath_any(td, vn, rootvnode, buf, retbuf, &buflen);
	if (!error)
		*freebuf = buf;
	else
		free(buf, M_TEMP);
	return (error);
}

int
vn_vptocnp(struct vnode **vp, struct ucred *cred, char *buf, size_t *buflen)
{
	struct vnode *dvp;
	struct namecache *ncp;
	struct mtx *vlp;
	int error;

	vlp = VP2VNODELOCK(*vp);
	mtx_lock(vlp);
	TAILQ_FOREACH(ncp, &((*vp)->v_cache_dst), nc_dst) {
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			break;
	}
	if (ncp != NULL) {
		if (*buflen < ncp->nc_nlen) {
			mtx_unlock(vlp);
			vrele(*vp);
			counter_u64_add(numfullpathfail4, 1);
			error = ENOMEM;
			SDT_PROBE3(vfs, namecache, fullpath, return, error,
			    vp, NULL);
			return (error);
		}
		*buflen -= ncp->nc_nlen;
		memcpy(buf + *buflen, ncp->nc_name, ncp->nc_nlen);
		SDT_PROBE3(vfs, namecache, fullpath, hit, ncp->nc_dvp,
		    ncp->nc_name, vp);
		dvp = *vp;
		*vp = ncp->nc_dvp;
		vref(*vp);
		mtx_unlock(vlp);
		vrele(dvp);
		return (0);
	}
	SDT_PROBE1(vfs, namecache, fullpath, miss, vp);

	mtx_unlock(vlp);
	vn_lock(*vp, LK_SHARED | LK_RETRY);
	error = VOP_VPTOCNP(*vp, &dvp, cred, buf, buflen);
	vput(*vp);
	if (error) {
		counter_u64_add(numfullpathfail2, 1);
		SDT_PROBE3(vfs, namecache, fullpath, return,  error, vp, NULL);
		return (error);
	}

	*vp = dvp;
	if (VN_IS_DOOMED(dvp)) {
		/* forced unmount */
		vrele(dvp);
		error = ENOENT;
		SDT_PROBE3(vfs, namecache, fullpath, return, error, vp, NULL);
		return (error);
	}
	/*
	 * *vp has its use count incremented still.
	 */

	return (0);
}

/*
 * Resolve a directory to a pathname.
 *
 * The name of the directory can always be found in the namecache or fetched
 * from the filesystem. There is also guaranteed to be only one parent, meaning
 * we can just follow vnodes up until we find the root.
 *
 * The vnode must be referenced.
 */
static int
vn_fullpath_dir(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, size_t *len, bool slash_prefixed, size_t addend)
{
#ifdef KDTRACE_HOOKS
	struct vnode *startvp = vp;
#endif
	struct vnode *vp1;
	size_t buflen;
	int error;

	VNPASS(vp->v_type == VDIR || VN_IS_DOOMED(vp), vp);
	VNPASS(vp->v_usecount > 0, vp);

	buflen = *len;

	if (!slash_prefixed) {
		MPASS(*len >= 2);
		buflen--;
		buf[buflen] = '\0';
	}

	error = 0;

	SDT_PROBE1(vfs, namecache, fullpath, entry, vp);
	counter_u64_add(numfullpathcalls, 1);
	while (vp != rdir && vp != rootvnode) {
		/*
		 * The vp vnode must be already fully constructed,
		 * since it is either found in namecache or obtained
		 * from VOP_VPTOCNP().  We may test for VV_ROOT safely
		 * without obtaining the vnode lock.
		 */
		if ((vp->v_vflag & VV_ROOT) != 0) {
			vn_lock(vp, LK_RETRY | LK_SHARED);

			/*
			 * With the vnode locked, check for races with
			 * unmount, forced or not.  Note that we
			 * already verified that vp is not equal to
			 * the root vnode, which means that
			 * mnt_vnodecovered can be NULL only for the
			 * case of unmount.
			 */
			if (VN_IS_DOOMED(vp) ||
			    (vp1 = vp->v_mount->mnt_vnodecovered) == NULL ||
			    vp1->v_mountedhere != vp->v_mount) {
				vput(vp);
				error = ENOENT;
				SDT_PROBE3(vfs, namecache, fullpath, return,
				    error, vp, NULL);
				break;
			}

			vref(vp1);
			vput(vp);
			vp = vp1;
			continue;
		}
		if (vp->v_type != VDIR) {
			vrele(vp);
			counter_u64_add(numfullpathfail1, 1);
			error = ENOTDIR;
			SDT_PROBE3(vfs, namecache, fullpath, return,
			    error, vp, NULL);
			break;
		}
		error = vn_vptocnp(&vp, td->td_ucred, buf, &buflen);
		if (error)
			break;
		if (buflen == 0) {
			vrele(vp);
			error = ENOMEM;
			SDT_PROBE3(vfs, namecache, fullpath, return, error,
			    startvp, NULL);
			break;
		}
		buf[--buflen] = '/';
		slash_prefixed = true;
	}
	if (error)
		return (error);
	if (!slash_prefixed) {
		if (buflen == 0) {
			vrele(vp);
			counter_u64_add(numfullpathfail4, 1);
			SDT_PROBE3(vfs, namecache, fullpath, return, ENOMEM,
			    startvp, NULL);
			return (ENOMEM);
		}
		buf[--buflen] = '/';
	}
	counter_u64_add(numfullpathfound, 1);
	vrele(vp);

	*retbuf = buf + buflen;
	SDT_PROBE3(vfs, namecache, fullpath, return, 0, startvp, *retbuf);
	*len -= buflen;
	*len += addend;
	return (0);
}

/*
 * Resolve an arbitrary vnode to a pathname.
 *
 * Note 2 caveats:
 * - hardlinks are not tracked, thus if the vnode is not a directory this can
 *   resolve to a different path than the one used to find it
 * - namecache is not mandatory, meaning names are not guaranteed to be added
 *   (in which case resolving fails)
 */
static int
vn_fullpath_any(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, size_t *buflen)
{
	size_t orig_buflen;
	bool slash_prefixed;
	int error;

	if (*buflen < 2)
		return (EINVAL);

	orig_buflen = *buflen;

	vref(vp);
	slash_prefixed = false;
	if (vp->v_type != VDIR) {
		*buflen -= 1;
		buf[*buflen] = '\0';
		error = vn_vptocnp(&vp, td->td_ucred, buf, buflen);
		if (error)
			return (error);
		if (*buflen == 0) {
			vrele(vp);
			return (ENOMEM);
		}
		*buflen -= 1;
		buf[*buflen] = '/';
		slash_prefixed = true;
	}

	return (vn_fullpath_dir(td, vp, rdir, buf, retbuf, buflen, slash_prefixed,
	    orig_buflen - *buflen));
}

/*
 * Resolve an arbitrary vnode to a pathname (taking care of hardlinks).
 *
 * Since the namecache does not track handlings, the caller is expected to first
 * look up the target vnode with SAVENAME | WANTPARENT flags passed to namei.
 *
 * Then we have 2 cases:
 * - if the found vnode is a directory, the path can be constructed just by
 *   fullowing names up the chain
 * - otherwise we populate the buffer with the saved name and start resolving
 *   from the parent
 */
static int
vn_fullpath_hardlink(struct thread *td, struct nameidata *ndp, char **retbuf,
    char **freebuf, size_t *buflen)
{
	char *buf, *tmpbuf;
	struct pwd *pwd;
	struct componentname *cnp;
	struct vnode *vp;
	size_t addend;
	int error;
	bool slash_prefixed;

	if (*buflen < 2)
		return (EINVAL);
	if (*buflen > MAXPATHLEN)
		*buflen = MAXPATHLEN;

	slash_prefixed = false;

	buf = malloc(*buflen, M_TEMP, M_WAITOK);
	pwd = pwd_hold(td);

	addend = 0;
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR) {
		cnp = &ndp->ni_cnd;
		addend = cnp->cn_namelen + 2;
		if (*buflen < addend) {
			error = ENOMEM;
			goto out_bad;
		}
		*buflen -= addend;
		tmpbuf = buf + *buflen;
		tmpbuf[0] = '/';
		memcpy(&tmpbuf[1], cnp->cn_nameptr, cnp->cn_namelen);
		tmpbuf[addend - 1] = '\0';
		slash_prefixed = true;
		vp = ndp->ni_dvp;
	}

	vref(vp);
	error = vn_fullpath_dir(td, vp, pwd->pwd_rdir, buf, retbuf, buflen,
	    slash_prefixed, addend);
	if (error != 0)
		goto out_bad;

	pwd_drop(pwd);
	*freebuf = buf;

	return (0);
out_bad:
	pwd_drop(pwd);
	free(buf, M_TEMP);
	return (error);
}

struct vnode *
vn_dir_dd_ino(struct vnode *vp)
{
	struct namecache *ncp;
	struct vnode *ddvp;
	struct mtx *vlp;
	enum vgetstate vs;

	ASSERT_VOP_LOCKED(vp, "vn_dir_dd_ino");
	vlp = VP2VNODELOCK(vp);
	mtx_lock(vlp);
	TAILQ_FOREACH(ncp, &(vp->v_cache_dst), nc_dst) {
		if ((ncp->nc_flag & NCF_ISDOTDOT) != 0)
			continue;
		ddvp = ncp->nc_dvp;
		vs = vget_prep(ddvp);
		mtx_unlock(vlp);
		if (vget_finish(ddvp, LK_SHARED | LK_NOWAIT, vs))
			return (NULL);
		return (ddvp);
	}
	mtx_unlock(vlp);
	return (NULL);
}

int
vn_commname(struct vnode *vp, char *buf, u_int buflen)
{
	struct namecache *ncp;
	struct mtx *vlp;
	int l;

	vlp = VP2VNODELOCK(vp);
	mtx_lock(vlp);
	TAILQ_FOREACH(ncp, &vp->v_cache_dst, nc_dst)
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			break;
	if (ncp == NULL) {
		mtx_unlock(vlp);
		return (ENOENT);
	}
	l = min(ncp->nc_nlen, buflen - 1);
	memcpy(buf, ncp->nc_name, l);
	mtx_unlock(vlp);
	buf[l] = '\0';
	return (0);
}

/*
 * This function updates path string to vnode's full global path
 * and checks the size of the new path string against the pathlen argument.
 *
 * Requires a locked, referenced vnode.
 * Vnode is re-locked on success or ENODEV, otherwise unlocked.
 *
 * If vp is a directory, the call to vn_fullpath_global() always succeeds
 * because it falls back to the ".." lookup if the namecache lookup fails.
 */
int
vn_path_to_global_path(struct thread *td, struct vnode *vp, char *path,
    u_int pathlen)
{
	struct nameidata nd;
	struct vnode *vp1;
	char *rpath, *fbuf;
	int error;

	ASSERT_VOP_ELOCKED(vp, __func__);

	/* Construct global filesystem path from vp. */
	VOP_UNLOCK(vp);
	error = vn_fullpath_global(td, vp, &rpath, &fbuf);

	if (error != 0) {
		vrele(vp);
		return (error);
	}

	if (strlen(rpath) >= pathlen) {
		vrele(vp);
		error = ENAMETOOLONG;
		goto out;
	}

	/*
	 * Re-lookup the vnode by path to detect a possible rename.
	 * As a side effect, the vnode is relocked.
	 * If vnode was renamed, return ENOENT.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1,
	    UIO_SYSSPACE, path, td);
	error = namei(&nd);
	if (error != 0) {
		vrele(vp);
		goto out;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp1 = nd.ni_vp;
	vrele(vp);
	if (vp1 == vp)
		strcpy(path, rpath);
	else {
		vput(vp1);
		error = ENOENT;
	}

out:
	free(fbuf, M_TEMP);
	return (error);
}

#ifdef DDB
static void
db_print_vpath(struct vnode *vp)
{

	while (vp != NULL) {
		db_printf("%p: ", vp);
		if (vp == rootvnode) {
			db_printf("/");
			vp = NULL;
		} else {
			if (vp->v_vflag & VV_ROOT) {
				db_printf("<mount point>");
				vp = vp->v_mount->mnt_vnodecovered;
			} else {
				struct namecache *ncp;
				char *ncn;
				int i;

				ncp = TAILQ_FIRST(&vp->v_cache_dst);
				if (ncp != NULL) {
					ncn = ncp->nc_name;
					for (i = 0; i < ncp->nc_nlen; i++)
						db_printf("%c", *ncn++);
					vp = ncp->nc_dvp;
				} else {
					vp = NULL;
				}
			}
		}
		db_printf("\n");
	}

	return;
}

DB_SHOW_COMMAND(vpath, db_show_vpath)
{
	struct vnode *vp;

	if (!have_addr) {
		db_printf("usage: show vpath <struct vnode *>\n");
		return;
	}

	vp = (struct vnode *)addr;
	db_print_vpath(vp);
}

#endif

extern uma_zone_t namei_zone;

static bool __read_frequently cache_fast_lookup = true;
SYSCTL_BOOL(_vfs, OID_AUTO, cache_fast_lookup, CTLFLAG_RW,
    &cache_fast_lookup, 0, "");

#define CACHE_FPL_FAILED	-2020

static void
cache_fpl_cleanup_cnp(struct componentname *cnp)
{

	uma_zfree(namei_zone, cnp->cn_pnbuf);
#ifdef DIAGNOSTIC
	cnp->cn_pnbuf = NULL;
	cnp->cn_nameptr = NULL;
#endif
}

static void
cache_fpl_handle_root(struct nameidata *ndp, struct vnode **dpp)
{
	struct componentname *cnp;

	cnp = &ndp->ni_cnd;
	while (*(cnp->cn_nameptr) == '/') {
		cnp->cn_nameptr++;
		ndp->ni_pathlen--;
	}

	*dpp = ndp->ni_rootdir;
}

/*
 * Components of nameidata (or objects it can point to) which may
 * need restoring in case fast path lookup fails.
 */
struct nameidata_saved {
	long cn_namelen;
	char *cn_nameptr;
	size_t ni_pathlen;
	int cn_flags;
};

struct cache_fpl {
	struct nameidata *ndp;
	struct componentname *cnp;
	struct pwd *pwd;
	struct vnode *dvp;
	struct vnode *tvp;
	seqc_t dvp_seqc;
	seqc_t tvp_seqc;
	struct nameidata_saved snd;
	int line;
	enum cache_fpl_status status:8;
	bool in_smr;
};

static void
cache_fpl_checkpoint(struct cache_fpl *fpl, struct nameidata_saved *snd)
{

	snd->cn_flags = fpl->ndp->ni_cnd.cn_flags;
	snd->cn_namelen = fpl->ndp->ni_cnd.cn_namelen;
	snd->cn_nameptr = fpl->ndp->ni_cnd.cn_nameptr;
	snd->ni_pathlen = fpl->ndp->ni_pathlen;
}

static void
cache_fpl_restore(struct cache_fpl *fpl, struct nameidata_saved *snd)
{

	fpl->ndp->ni_cnd.cn_flags = snd->cn_flags;
	fpl->ndp->ni_cnd.cn_namelen = snd->cn_namelen;
	fpl->ndp->ni_cnd.cn_nameptr = snd->cn_nameptr;
	fpl->ndp->ni_pathlen = snd->ni_pathlen;
}

#ifdef INVARIANTS
#define cache_fpl_smr_assert_entered(fpl) ({			\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == true);				\
	VFS_SMR_ASSERT_ENTERED();				\
})
#define cache_fpl_smr_assert_not_entered(fpl) ({		\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == false);				\
	VFS_SMR_ASSERT_NOT_ENTERED();				\
})
#else
#define cache_fpl_smr_assert_entered(fpl) do { } while (0)
#define cache_fpl_smr_assert_not_entered(fpl) do { } while (0)
#endif

#define cache_fpl_smr_enter(fpl) ({				\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == false);				\
	vfs_smr_enter();					\
	_fpl->in_smr = true;					\
})

#define cache_fpl_smr_exit(fpl) ({				\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == true);				\
	vfs_smr_exit();						\
	_fpl->in_smr = false;					\
})

static int
cache_fpl_aborted_impl(struct cache_fpl *fpl, int line)
{

	if (fpl->status != CACHE_FPL_STATUS_UNSET) {
		KASSERT(fpl->status == CACHE_FPL_STATUS_PARTIAL,
		    ("%s: converting to abort from %d at %d, set at %d\n",
		    __func__, fpl->status, line, fpl->line));
	}
	fpl->status = CACHE_FPL_STATUS_ABORTED;
	fpl->line = line;
	return (CACHE_FPL_FAILED);
}

#define cache_fpl_aborted(x)	cache_fpl_aborted_impl((x), __LINE__)

static int
cache_fpl_partial_impl(struct cache_fpl *fpl, int line)
{

	KASSERT(fpl->status == CACHE_FPL_STATUS_UNSET,
	    ("%s: setting to partial at %d, but already set to %d at %d\n",
	    __func__, line, fpl->status, fpl->line));
	cache_fpl_smr_assert_entered(fpl);
	fpl->status = CACHE_FPL_STATUS_PARTIAL;
	fpl->line = line;
	return (CACHE_FPL_FAILED);
}

#define cache_fpl_partial(x)	cache_fpl_partial_impl((x), __LINE__)

static int
cache_fpl_handled_impl(struct cache_fpl *fpl, int error, int line)
{

	KASSERT(fpl->status == CACHE_FPL_STATUS_UNSET,
	    ("%s: setting to handled at %d, but already set to %d at %d\n",
	    __func__, line, fpl->status, fpl->line));
	cache_fpl_smr_assert_not_entered(fpl);
	MPASS(error != CACHE_FPL_FAILED);
	fpl->status = CACHE_FPL_STATUS_HANDLED;
	fpl->line = line;
	return (error);
}

#define cache_fpl_handled(x, e)	cache_fpl_handled_impl((x), (e), __LINE__)

#define CACHE_FPL_SUPPORTED_CN_FLAGS \
	(LOCKLEAF | LOCKPARENT | WANTPARENT | FOLLOW | LOCKSHARED | SAVENAME | \
	 ISOPEN | NOMACCHECK | AUDITVNODE1 | AUDITVNODE2)

static bool
cache_can_fplookup(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	struct thread *td;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	td = cnp->cn_thread;

	if (!cache_fast_lookup) {
		cache_fpl_aborted(fpl);
		return (false);
	}
#ifdef MAC
	if (mac_vnode_check_lookup_enabled()) {
		cache_fpl_aborted(fpl);
		return (false);
	}
#endif
	if ((cnp->cn_flags & ~CACHE_FPL_SUPPORTED_CN_FLAGS) != 0) {
		cache_fpl_aborted(fpl);
		return (false);
	}
	if (cnp->cn_nameiop != LOOKUP) {
		cache_fpl_aborted(fpl);
		return (false);
	}
	if (ndp->ni_dirfd != AT_FDCWD) {
		cache_fpl_aborted(fpl);
		return (false);
	}
	if (IN_CAPABILITY_MODE(td)) {
		cache_fpl_aborted(fpl);
		return (false);
	}
	if (AUDITING_TD(td)) {
		cache_fpl_aborted(fpl);
		return (false);
	}
	if (ndp->ni_startdir != NULL) {
		cache_fpl_aborted(fpl);
		return (false);
	}
	return (true);
}

static bool
cache_fplookup_vnode_supported(struct vnode *vp)
{

	return (vp->v_type != VLNK);
}

/*
 * Move a negative entry to the hot list.
 *
 * We have to take locks, but they may be contended and in the worst
 * case we may need to go off CPU. We don't want to spin within the
 * smr section and we can't block with it. Instead we are going to
 * look up the entry again.
 */
static int __noinline
cache_fplookup_negative_promote(struct cache_fpl *fpl, struct namecache *oncp,
    uint32_t hash)
{
	struct componentname *cnp;
	struct namecache *ncp;
	struct neglist *neglist;
	struct negstate *negstate;
	struct vnode *dvp;
	u_char nc_flag;

	cnp = fpl->cnp;
	dvp = fpl->dvp;

	if (!vhold_smr(dvp))
		return (cache_fpl_aborted(fpl));

	neglist = NCP2NEGLIST(oncp);
	cache_fpl_smr_exit(fpl);

	mtx_lock(&ncneg_hot.nl_lock);
	mtx_lock(&neglist->nl_lock);
	/*
	 * For hash iteration.
	 */
	cache_fpl_smr_enter(fpl);

	/*
	 * Avoid all surprises by only succeeding if we got the same entry and
	 * bailing completely otherwise.
	 *
	 * In particular at this point there can be a new ncp which matches the
	 * search but hashes to a different neglist.
	 */
	CK_LIST_FOREACH(ncp, (NCHHASH(hash)), nc_hash) {
		if (ncp == oncp)
			break;
	}

	/*
	 * No match to begin with.
	 */
	if (__predict_false(ncp == NULL)) {
		goto out_abort;
	}

	/*
	 * The newly found entry may be something different...
	 */
	if (!(ncp->nc_dvp == dvp && ncp->nc_nlen == cnp->cn_namelen &&
	    !bcmp(ncp->nc_name, cnp->cn_nameptr, ncp->nc_nlen))) {
		goto out_abort;
	}

	/*
	 * ... and not even negative.
	 */
	nc_flag = atomic_load_char(&ncp->nc_flag);
	if ((nc_flag & NCF_NEGATIVE) == 0) {
		goto out_abort;
	}

	if (__predict_false(cache_ncp_invalid(ncp))) {
		goto out_abort;
	}

	negstate = NCP2NEGSTATE(ncp);
	if ((negstate->neg_flag & NEG_HOT) == 0) {
		numhotneg++;
		TAILQ_REMOVE(&neglist->nl_list, ncp, nc_dst);
		TAILQ_INSERT_TAIL(&ncneg_hot.nl_list, ncp, nc_dst);
		negstate->neg_flag |= NEG_HOT;
	}

	SDT_PROBE2(vfs, namecache, lookup, hit__negative, dvp, ncp->nc_name);
	counter_u64_add(numneghits, 1);
	cache_fpl_smr_exit(fpl);
	mtx_unlock(&neglist->nl_lock);
	mtx_unlock(&ncneg_hot.nl_lock);
	vdrop(dvp);
	return (cache_fpl_handled(fpl, ENOENT));
out_abort:
	cache_fpl_smr_exit(fpl);
	mtx_unlock(&neglist->nl_lock);
	mtx_unlock(&ncneg_hot.nl_lock);
	vdrop(dvp);
	return (cache_fpl_aborted(fpl));
}

/*
 * The target vnode is not supported, prepare for the slow path to take over.
 */
static int
cache_fplookup_partial_setup(struct cache_fpl *fpl)
{
	struct componentname *cnp;
	enum vgetstate dvs;
	struct vnode *dvp;
	struct pwd *pwd;
	seqc_t dvp_seqc;

	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;

	dvs = vget_prep_smr(dvp);
	if (dvs == VGET_NONE) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_aborted(fpl));
	}

	cache_fpl_smr_exit(fpl);

	vget_finish_ref(dvp, dvs);
	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		vrele(dvp);
		return (cache_fpl_aborted(fpl));
	}

	pwd = pwd_hold(curthread);
	if (fpl->pwd != pwd) {
		vrele(dvp);
		pwd_drop(pwd);
		return (cache_fpl_aborted(fpl));
	}

	fpl->ndp->ni_startdir = dvp;
	return (0);
}

static int
cache_fplookup_final_child(struct cache_fpl *fpl, enum vgetstate tvs)
{
	struct componentname *cnp;
	struct vnode *tvp;
	seqc_t tvp_seqc;
	int error;

	cnp = fpl->cnp;
	tvp = fpl->tvp;
	tvp_seqc = fpl->tvp_seqc;

	if ((cnp->cn_flags & LOCKLEAF) != 0) {
		error = vget_finish(tvp, cnp->cn_lkflags, tvs);
		if (error != 0) {
			return (cache_fpl_aborted(fpl));
		}
	} else {
		vget_finish_ref(tvp, tvs);
	}

	if (!vn_seqc_consistent(tvp, tvp_seqc)) {
		if ((cnp->cn_flags & LOCKLEAF) != 0)
			vput(tvp);
		else
			vrele(tvp);
		return (cache_fpl_aborted(fpl));
	}

	return (cache_fpl_handled(fpl, 0));
}

static int __noinline
cache_fplookup_final_withparent(struct cache_fpl *fpl)
{
	enum vgetstate dvs, tvs;
	struct componentname *cnp;
	struct vnode *dvp, *tvp;
	seqc_t dvp_seqc, tvp_seqc;
	int error;

	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;
	tvp = fpl->tvp;
	tvp_seqc = fpl->tvp_seqc;

	MPASS((cnp->cn_flags & (LOCKPARENT|WANTPARENT)) != 0);

	/*
	 * This is less efficient than it can be for simplicity.
	 */
	dvs = vget_prep_smr(dvp);
	if (dvs == VGET_NONE) {
		return (cache_fpl_aborted(fpl));
	}
	tvs = vget_prep_smr(tvp);
	if (tvs == VGET_NONE) {
		cache_fpl_smr_exit(fpl);
		vget_abort(dvp, dvs);
		return (cache_fpl_aborted(fpl));
	}

	cache_fpl_smr_exit(fpl);

	if ((cnp->cn_flags & LOCKPARENT) != 0) {
		error = vget_finish(dvp, LK_EXCLUSIVE, dvs);
		if (error != 0) {
			vget_abort(tvp, tvs);
			return (cache_fpl_aborted(fpl));
		}
	} else {
		vget_finish_ref(dvp, dvs);
	}

	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		vget_abort(tvp, tvs);
		if ((cnp->cn_flags & LOCKPARENT) != 0)
			vput(dvp);
		else
			vrele(dvp);
		cache_fpl_aborted(fpl);
		return (error);
	}

	error = cache_fplookup_final_child(fpl, tvs);
	if (error != 0) {
		MPASS(fpl->status == CACHE_FPL_STATUS_ABORTED);
		if ((cnp->cn_flags & LOCKPARENT) != 0)
			vput(dvp);
		else
			vrele(dvp);
		return (error);
	}

	MPASS(fpl->status == CACHE_FPL_STATUS_HANDLED);
	return (0);
}

static int
cache_fplookup_final(struct cache_fpl *fpl)
{
	struct componentname *cnp;
	enum vgetstate tvs;
	struct vnode *dvp, *tvp;
	seqc_t dvp_seqc, tvp_seqc;

	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;
	tvp = fpl->tvp;
	tvp_seqc = fpl->tvp_seqc;

	VNPASS(cache_fplookup_vnode_supported(dvp), dvp);

	if ((cnp->cn_flags & (LOCKPARENT|WANTPARENT)) != 0)
		return (cache_fplookup_final_withparent(fpl));

	tvs = vget_prep_smr(tvp);
	if (tvs == VGET_NONE) {
		return (cache_fpl_partial(fpl));
	}

	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		cache_fpl_smr_exit(fpl);
		vget_abort(tvp, tvs);
		return (cache_fpl_aborted(fpl));
	}

	cache_fpl_smr_exit(fpl);
	return (cache_fplookup_final_child(fpl, tvs));
}

static int
cache_fplookup_next(struct cache_fpl *fpl)
{
	struct componentname *cnp;
	struct namecache *ncp;
	struct negstate *negstate;
	struct vnode *dvp, *tvp;
	u_char nc_flag;
	uint32_t hash;
	bool neg_hot;

	cnp = fpl->cnp;
	dvp = fpl->dvp;

	if (__predict_false(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.')) {
		fpl->tvp = dvp;
		fpl->tvp_seqc = vn_seqc_read_any(dvp);
		if (seqc_in_modify(fpl->tvp_seqc)) {
			return (cache_fpl_aborted(fpl));
		}
		return (0);
	}

	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);

	CK_LIST_FOREACH(ncp, (NCHHASH(hash)), nc_hash) {
		if (ncp->nc_dvp == dvp && ncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(ncp->nc_name, cnp->cn_nameptr, ncp->nc_nlen))
			break;
	}

	/*
	 * If there is no entry we have to punt to the slow path to perform
	 * actual lookup. Should there be nothing with this name a negative
	 * entry will be created.
	 */
	if (__predict_false(ncp == NULL)) {
		return (cache_fpl_partial(fpl));
	}

	tvp = atomic_load_ptr(&ncp->nc_vp);
	nc_flag = atomic_load_char(&ncp->nc_flag);
	if ((nc_flag & NCF_NEGATIVE) != 0) {
		negstate = NCP2NEGSTATE(ncp);
		neg_hot = ((negstate->neg_flag & NEG_HOT) != 0);
		if (__predict_false(cache_ncp_invalid(ncp))) {
			return (cache_fpl_partial(fpl));
		}
		if (__predict_false((nc_flag & NCF_WHITE) != 0)) {
			return (cache_fpl_partial(fpl));
		}
		if (!neg_hot) {
			return (cache_fplookup_negative_promote(fpl, ncp, hash));
		}
		SDT_PROBE2(vfs, namecache, lookup, hit__negative, dvp,
		    ncp->nc_name);
		counter_u64_add(numneghits, 1);
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled(fpl, ENOENT));
	}

	if (__predict_false(cache_ncp_invalid(ncp))) {
		return (cache_fpl_partial(fpl));
	}

	fpl->tvp = tvp;
	fpl->tvp_seqc = vn_seqc_read_any(tvp);
	if (seqc_in_modify(fpl->tvp_seqc)) {
		return (cache_fpl_partial(fpl));
	}

	if (!cache_fplookup_vnode_supported(tvp)) {
		return (cache_fpl_partial(fpl));
	}

	counter_u64_add(numposhits, 1);
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ncp->nc_name, tvp);
	return (0);
}

static bool
cache_fplookup_mp_supported(struct mount *mp)
{

	if (mp == NULL)
		return (false);
	if ((mp->mnt_kern_flag & MNTK_FPLOOKUP) == 0)
		return (false);
	if ((mp->mnt_flag & MNT_UNION) != 0)
		return (false);
	return (true);
}

/*
 * Walk up the mount stack (if any).
 *
 * Correctness is provided in the following ways:
 * - all vnodes are protected from freeing with SMR
 * - struct mount objects are type stable making them always safe to access
 * - stability of the particular mount is provided by busying it
 * - relationship between the vnode which is mounted on and the mount is
 *   verified with the vnode sequence counter after busying
 * - association between root vnode of the mount and the mount is protected
 *   by busy
 *
 * From that point on we can read the sequence counter of the root vnode
 * and get the next mount on the stack (if any) using the same protection.
 *
 * By the end of successful walk we are guaranteed the reached state was
 * indeed present at least at some point which matches the regular lookup.
 */
static int __noinline
cache_fplookup_climb_mount(struct cache_fpl *fpl)
{
	struct mount *mp, *prev_mp;
	struct vnode *vp;
	seqc_t vp_seqc;

	vp = fpl->tvp;
	vp_seqc = fpl->tvp_seqc;

	VNPASS(vp->v_type == VDIR || vp->v_type == VBAD, vp);
	mp = atomic_load_ptr(&vp->v_mountedhere);
	if (mp == NULL)
		return (0);

	prev_mp = NULL;
	for (;;) {
		if (!vfs_op_thread_enter(mp)) {
			if (prev_mp != NULL)
				vfs_op_thread_exit(prev_mp);
			return (cache_fpl_partial(fpl));
		}
		if (prev_mp != NULL)
			vfs_op_thread_exit(prev_mp);
		if (!vn_seqc_consistent(vp, vp_seqc)) {
			vfs_op_thread_exit(mp);
			return (cache_fpl_partial(fpl));
		}
		if (!cache_fplookup_mp_supported(mp)) {
			vfs_op_thread_exit(mp);
			return (cache_fpl_partial(fpl));
		}
		vp = atomic_load_ptr(&mp->mnt_rootvnode);
		if (vp == NULL || VN_IS_DOOMED(vp)) {
			vfs_op_thread_exit(mp);
			return (cache_fpl_partial(fpl));
		}
		vp_seqc = vn_seqc_read_any(vp);
		if (seqc_in_modify(vp_seqc)) {
			vfs_op_thread_exit(mp);
			return (cache_fpl_partial(fpl));
		}
		prev_mp = mp;
		mp = atomic_load_ptr(&vp->v_mountedhere);
		if (mp == NULL)
			break;
	}

	vfs_op_thread_exit(prev_mp);
	fpl->tvp = vp;
	fpl->tvp_seqc = vp_seqc;
	return (0);
}

static bool
cache_fplookup_need_climb_mount(struct cache_fpl *fpl)
{
	struct mount *mp;
	struct vnode *vp;

	vp = fpl->tvp;

	/*
	 * Hack: while this is a union, the pointer tends to be NULL so save on
	 * a branch.
	 */
	mp = atomic_load_ptr(&vp->v_mountedhere);
	if (mp == NULL)
		return (false);
	if (vp->v_type == VDIR)
		return (true);
	return (false);
}

/*
 * Parse the path.
 *
 * The code is mostly copy-pasted from regular lookup, see lookup().
 * The structure is maintained along with comments for easier maintenance.
 * Deduplicating the code will become feasible after fast path lookup
 * becomes more feature-complete.
 */
static int
cache_fplookup_parse(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	char *cp;
	char *prev_ni_next;             /* saved ndp->ni_next */
	size_t prev_ni_pathlen;         /* saved ndp->ni_pathlen */

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	/*
	 * Search a new directory.
	 *
	 * The last component of the filename is left accessible via
	 * cnp->cn_nameptr for callers that need the name. Callers needing
	 * the name set the SAVENAME flag. When done, they assume
	 * responsibility for freeing the pathname buffer.
	 */
	for (cp = cnp->cn_nameptr; *cp != 0 && *cp != '/'; cp++)
		continue;
	cnp->cn_namelen = cp - cnp->cn_nameptr;
	if (cnp->cn_namelen > NAME_MAX) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled(fpl, ENAMETOOLONG));
	}
	prev_ni_pathlen = ndp->ni_pathlen;
	ndp->ni_pathlen -= cnp->cn_namelen;
	KASSERT(ndp->ni_pathlen <= PATH_MAX,
	    ("%s: ni_pathlen underflow to %zd\n", __func__, ndp->ni_pathlen));
	prev_ni_next = ndp->ni_next;
	ndp->ni_next = cp;

	/*
	 * Replace multiple slashes by a single slash and trailing slashes
	 * by a null.  This must be done before VOP_LOOKUP() because some
	 * fs's don't know about trailing slashes.  Remember if there were
	 * trailing slashes to handle symlinks, existing non-directories
	 * and non-existing files that won't be directories specially later.
	 */
	while (*cp == '/' && (cp[1] == '/' || cp[1] == '\0')) {
		cp++;
		ndp->ni_pathlen--;
		if (*cp == '\0') {
			/*
			 * TODO
			 * Regular lookup performs the following:
			 * *ndp->ni_next = '\0';
			 * cnp->cn_flags |= TRAILINGSLASH;
			 *
			 * Which is problematic since it modifies data read
			 * from userspace. Then if fast path lookup was to
			 * abort we would have to either restore it or convey
			 * the flag. Since this is a corner case just ignore
			 * it for simplicity.
			 */
			return (cache_fpl_partial(fpl));
		}
	}
	ndp->ni_next = cp;

	cnp->cn_flags |= MAKEENTRY;

	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[1] == '.' && cnp->cn_nameptr[0] == '.')
		cnp->cn_flags |= ISDOTDOT;
	else
		cnp->cn_flags &= ~ISDOTDOT;
	if (*ndp->ni_next == 0)
		cnp->cn_flags |= ISLASTCN;
	else
		cnp->cn_flags &= ~ISLASTCN;

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. like "/." or ".".
	 *
	 * TODO
	 * Another corner case handled by the regular lookup
	 */
	if (__predict_false(cnp->cn_nameptr[0] == '\0')) {
		return (cache_fpl_partial(fpl));
	}
	return (0);
}

static void
cache_fplookup_parse_advance(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	cnp->cn_nameptr = ndp->ni_next;
	while (*cnp->cn_nameptr == '/') {
		cnp->cn_nameptr++;
		ndp->ni_pathlen--;
	}
}

static int
cache_fplookup_impl(struct vnode *dvp, struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	struct mount *mp;
	int error;

	error = CACHE_FPL_FAILED;
	ndp = fpl->ndp;
	ndp->ni_lcf = 0;
	cnp = fpl->cnp;
	cnp->cn_lkflags = LK_SHARED;
	if ((cnp->cn_flags & LOCKSHARED) == 0)
		cnp->cn_lkflags = LK_EXCLUSIVE;

	cache_fpl_checkpoint(fpl, &fpl->snd);

	fpl->dvp = dvp;
	fpl->dvp_seqc = vn_seqc_read_any(fpl->dvp);
	if (seqc_in_modify(fpl->dvp_seqc)) {
		cache_fpl_aborted(fpl);
		goto out;
	}
	mp = atomic_load_ptr(&fpl->dvp->v_mount);
	if (!cache_fplookup_mp_supported(mp)) {
		cache_fpl_aborted(fpl);
		goto out;
	}

	VNPASS(cache_fplookup_vnode_supported(fpl->dvp), fpl->dvp);

	for (;;) {
		error = cache_fplookup_parse(fpl);
		if (__predict_false(error != 0)) {
			break;
		}

		if (cnp->cn_flags & ISDOTDOT) {
			error = cache_fpl_partial(fpl);
			break;
		}

		VNPASS(cache_fplookup_vnode_supported(fpl->dvp), fpl->dvp);

		error = VOP_FPLOOKUP_VEXEC(fpl->dvp, cnp->cn_cred, cnp->cn_thread);
		if (__predict_false(error != 0)) {
			switch (error) {
			case EAGAIN:
			case EOPNOTSUPP: /* can happen when racing against vgone */
				cache_fpl_partial(fpl);
				break;
			default:
				/*
				 * See the API contract for VOP_FPLOOKUP_VEXEC.
				 */
				if (!vn_seqc_consistent(fpl->dvp, fpl->dvp_seqc)) {
					error = cache_fpl_aborted(fpl);
				} else {
					cache_fpl_smr_exit(fpl);
					cache_fpl_handled(fpl, error);
				}
				break;
			}
			break;
		}

		error = cache_fplookup_next(fpl);
		if (__predict_false(error != 0)) {
			break;
		}

		VNPASS(!seqc_in_modify(fpl->tvp_seqc), fpl->tvp);

		if (cache_fplookup_need_climb_mount(fpl)) {
			error = cache_fplookup_climb_mount(fpl);
			if (__predict_false(error != 0)) {
				break;
			}
		}

		VNPASS(!seqc_in_modify(fpl->tvp_seqc), fpl->tvp);

		if (cnp->cn_flags & ISLASTCN) {
			error = cache_fplookup_final(fpl);
			break;
		}

		if (!vn_seqc_consistent(fpl->dvp, fpl->dvp_seqc)) {
			error = cache_fpl_aborted(fpl);
			break;
		}

		fpl->dvp = fpl->tvp;
		fpl->dvp_seqc = fpl->tvp_seqc;

		cache_fplookup_parse_advance(fpl);
		cache_fpl_checkpoint(fpl, &fpl->snd);
	}
out:
	switch (fpl->status) {
	case CACHE_FPL_STATUS_UNSET:
		__assert_unreachable();
		break;
	case CACHE_FPL_STATUS_PARTIAL:
		cache_fpl_smr_assert_entered(fpl);
		return (cache_fplookup_partial_setup(fpl));
	case CACHE_FPL_STATUS_ABORTED:
		if (fpl->in_smr)
			cache_fpl_smr_exit(fpl);
		return (CACHE_FPL_FAILED);
	case CACHE_FPL_STATUS_HANDLED:
		cache_fpl_smr_assert_not_entered(fpl);
		if (__predict_false(error != 0)) {
			ndp->ni_dvp = NULL;
			ndp->ni_vp = NULL;
			cache_fpl_cleanup_cnp(cnp);
			return (error);
		}
		ndp->ni_dvp = fpl->dvp;
		ndp->ni_vp = fpl->tvp;
		if (cnp->cn_flags & SAVENAME)
			cnp->cn_flags |= HASBUF;
		else
			cache_fpl_cleanup_cnp(cnp);
		return (error);
	}
}

/*
 * Fast path lookup protected with SMR and sequence counters.
 *
 * Note: all VOP_FPLOOKUP_VEXEC routines have a comment referencing this one.
 *
 * Filesystems can opt in by setting the MNTK_FPLOOKUP flag and meeting criteria
 * outlined below.
 *
 * Traditional vnode lookup conceptually looks like this:
 *
 * vn_lock(current);
 * for (;;) {
 *	next = find();
 *	vn_lock(next);
 *	vn_unlock(current);
 *	current = next;
 *	if (last)
 *	    break;
 * }
 * return (current);
 *
 * Each jump to the next vnode is safe memory-wise and atomic with respect to
 * any modifications thanks to holding respective locks.
 *
 * The same guarantee can be provided with a combination of safe memory
 * reclamation and sequence counters instead. If all operations which affect
 * the relationship between the current vnode and the one we are looking for
 * also modify the counter, we can verify whether all the conditions held as
 * we made the jump. This includes things like permissions, mount points etc.
 * Counter modification is provided by enclosing relevant places in
 * vn_seqc_write_begin()/end() calls.
 *
 * Thus this translates to:
 *
 * vfs_smr_enter();
 * dvp_seqc = seqc_read_any(dvp);
 * if (seqc_in_modify(dvp_seqc)) // someone is altering the vnode
 *     abort();
 * for (;;) {
 * 	tvp = find();
 * 	tvp_seqc = seqc_read_any(tvp);
 * 	if (seqc_in_modify(tvp_seqc)) // someone is altering the target vnode
 * 	    abort();
 * 	if (!seqc_consistent(dvp, dvp_seqc) // someone is altering the vnode
 * 	    abort();
 * 	dvp = tvp; // we know nothing of importance has changed
 * 	dvp_seqc = tvp_seqc; // store the counter for the tvp iteration
 * 	if (last)
 * 	    break;
 * }
 * vget(); // secure the vnode
 * if (!seqc_consistent(tvp, tvp_seqc) // final check
 * 	    abort();
 * // at this point we know nothing has changed for any parent<->child pair
 * // as they were crossed during the lookup, meaning we matched the guarantee
 * // of the locked variant
 * return (tvp);
 *
 * The API contract for VOP_FPLOOKUP_VEXEC routines is as follows:
 * - they are called while within vfs_smr protection which they must never exit
 * - EAGAIN can be returned to denote checking could not be performed, it is
 *   always valid to return it
 * - if the sequence counter has not changed the result must be valid
 * - if the sequence counter has changed both false positives and false negatives
 *   are permitted (since the result will be rejected later)
 * - for simple cases of unix permission checks vaccess_vexec_smr can be used
 *
 * Caveats to watch out for:
 * - vnodes are passed unlocked and unreferenced with nothing stopping
 *   VOP_RECLAIM, in turn meaning that ->v_data can become NULL. It is advised
 *   to use atomic_load_ptr to fetch it.
 * - the aforementioned object can also get freed, meaning absent other means it
 *   should be protected with vfs_smr
 * - either safely checking permissions as they are modified or guaranteeing
 *   their stability is left to the routine
 */
int
cache_fplookup(struct nameidata *ndp, enum cache_fpl_status *status,
    struct pwd **pwdp)
{
	struct cache_fpl fpl;
	struct pwd *pwd;
	struct vnode *dvp;
	struct componentname *cnp;
	struct nameidata_saved orig;
	int error;

	*status = CACHE_FPL_STATUS_UNSET;
	bzero(&fpl, sizeof(fpl));
	fpl.status = CACHE_FPL_STATUS_UNSET;
	fpl.ndp = ndp;
	fpl.cnp = &ndp->ni_cnd;
	MPASS(curthread == fpl.cnp->cn_thread);

	if (!cache_can_fplookup(&fpl)) {
		SDT_PROBE3(vfs, fplookup, lookup, done, ndp, fpl.line, fpl.status);
		*status = fpl.status;
		return (EOPNOTSUPP);
	}

	cache_fpl_checkpoint(&fpl, &orig);

	cache_fpl_smr_enter(&fpl);
	pwd = pwd_get_smr();
	fpl.pwd = pwd;
	ndp->ni_rootdir = pwd->pwd_rdir;
	ndp->ni_topdir = pwd->pwd_jdir;

	cnp = fpl.cnp;
	cnp->cn_nameptr = cnp->cn_pnbuf;
	if (cnp->cn_pnbuf[0] == '/') {
		cache_fpl_handle_root(ndp, &dvp);
	} else {
		MPASS(ndp->ni_dirfd == AT_FDCWD);
		dvp = pwd->pwd_cdir;
	}

	SDT_PROBE4(vfs, namei, lookup, entry, dvp, cnp->cn_pnbuf, cnp->cn_flags, true);

	error = cache_fplookup_impl(dvp, &fpl);
	cache_fpl_smr_assert_not_entered(&fpl);
	SDT_PROBE3(vfs, fplookup, lookup, done, ndp, fpl.line, fpl.status);

	*status = fpl.status;
	switch (fpl.status) {
	case CACHE_FPL_STATUS_UNSET:
		__assert_unreachable();
		break;
	case CACHE_FPL_STATUS_HANDLED:
		SDT_PROBE3(vfs, namei, lookup, return, error,
		    (error == 0 ? ndp->ni_vp : NULL), true);
		break;
	case CACHE_FPL_STATUS_PARTIAL:
		*pwdp = fpl.pwd;
		cache_fpl_restore(&fpl, &fpl.snd);
		break;
	case CACHE_FPL_STATUS_ABORTED:
		cache_fpl_restore(&fpl, &orig);
		break;
	}
	return (error);
}
