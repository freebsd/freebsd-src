/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/dnlc.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/kstat.h>
#include <sys/atomic.h>
#include <sys/taskq.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/eventhandler.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>

#define	TRACE_0(...)	do { } while (0)
#define	TRACE_2(...)	do { } while (0)
#define	TRACE_4(...)	do { } while (0)

SYSCTL_DECL(_vfs_zfs);
SYSCTL_NODE(_vfs_zfs, OID_AUTO, dnlc, CTLFLAG_RW, 0, "ZFS namecache");

/*
 * Directory name lookup cache.
 * Based on code originally done by Robert Elz at Melbourne.
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  Each hash chain is ordered by LRU
 * Cache is indexed by hash value obtained from (vp, name)
 * where the vp refers to the directory containing the name.
 */

/*
 * Tunable nc_hashavelen is the average length desired for this chain, from
 * which the size of the nc_hash table is derived at create time.
 */
#define	NC_HASHAVELEN_DEFAULT	4
int nc_hashavelen = NC_HASHAVELEN_DEFAULT;

/*
 * NC_MOVETOFRONT is the move-to-front threshold: if the hash lookup
 * depth exceeds this value, we move the looked-up entry to the front of
 * its hash chain.  The idea is to make sure that the most frequently
 * accessed entries are found most quickly (by keeping them near the
 * front of their hash chains).
 */
#define	NC_MOVETOFRONT	2

/*
 *
 * DNLC_MAX_RELE is used to size an array on the stack when releasing
 * vnodes. This array is used rather than calling VN_RELE() inline because
 * all dnlc locks must be dropped by that time in order to avoid a
 * possible deadlock. This deadlock occurs when the dnlc holds the last
 * reference to the vnode and so the VOP_INACTIVE vector is called which
 * can in turn call back into the dnlc. A global array was used but had
 * many problems:
 *	1) Actually doesn't have an upper bound on the array size as
 *	   entries can be added after starting the purge.
 *	2) The locking scheme causes a hang.
 *	3) Caused serialisation on the global lock.
 *	4) The array was often unnecessarily huge.
 *
 * Note the current value 8 allows up to 4 cache entries (to be purged
 * from each hash chain), before having to cycle around and retry.
 * This ought to be ample given that nc_hashavelen is typically very small.
 */
#define	DNLC_MAX_RELE	8 /* must be even */

/*
 * Hash table of name cache entries for fast lookup, dynamically
 * allocated at startup.
 */
nc_hash_t *nc_hash;

/*
 * Rotors. Used to select entries on a round-robin basis.
 */
static nc_hash_t *dnlc_purge_fs1_rotor;
static nc_hash_t *dnlc_free_rotor;

/*
 * # of dnlc entries (uninitialized)
 *
 * the initial value was chosen as being
 * a random string of bits, probably not
 * normally chosen by a systems administrator
 */
int ncsize = -1;
TUNABLE_INT("vfs.zfs.dnlc.ncsize", &ncsize);
SYSCTL_INT(_vfs_zfs_dnlc, OID_AUTO, ncsize, CTLFLAG_RDTUN, &ncsize, 0,
    "Number of DNLC entries");
volatile uint32_t dnlc_nentries = 0;	/* current num of name cache entries */
SYSCTL_UINT(_vfs_zfs_dnlc, OID_AUTO, nentries, CTLFLAG_RD,
    __DEVOLATILE(u_int *, &dnlc_nentries), 0, "Number of DNLC entries in use");
static int nc_hashsz;			/* size of hash table */
static int nc_hashmask;			/* size of hash table minus 1 */

/*
 * The dnlc_reduce_cache() taskq queue is activated when there are
 * ncsize name cache entries and if no parameter is provided, it reduces
 * the size down to dnlc_nentries_low_water, which is by default one
 * hundreth less (or 99%) of ncsize.
 *
 * If a parameter is provided to dnlc_reduce_cache(), then we reduce
 * the size down based on ncsize_onepercent - where ncsize_onepercent
 * is 1% of ncsize; however, we never let dnlc_reduce_cache() reduce
 * the size below 3% of ncsize (ncsize_min_percent).
 */
#define	DNLC_LOW_WATER_DIVISOR_DEFAULT 100
uint_t dnlc_low_water_divisor = DNLC_LOW_WATER_DIVISOR_DEFAULT;
uint_t dnlc_nentries_low_water;
int dnlc_reduce_idle = 1; /* no locking needed */
uint_t ncsize_onepercent;
uint_t ncsize_min_percent;

/*
 * If dnlc_nentries hits dnlc_max_nentries (twice ncsize)
 * then this means the dnlc_reduce_cache() taskq is failing to
 * keep up. In this case we refuse to add new entries to the dnlc
 * until the taskq catches up.
 */
uint_t dnlc_max_nentries; /* twice ncsize */
SYSCTL_UINT(_vfs_zfs_dnlc, OID_AUTO, max_nentries, CTLFLAG_RD,
    &dnlc_max_nentries, 0, "Maximum number of DNLC entries");
uint64_t dnlc_max_nentries_cnt = 0; /* statistic on times we failed */

/*
 * Tunable to define when we should just remove items from
 * the end of the chain.
 */
#define	DNLC_LONG_CHAIN 8
uint_t dnlc_long_chain = DNLC_LONG_CHAIN;

/*
 * ncstats has been deprecated, due to the integer size of the counters
 * which can easily overflow in the dnlc.
 * It is maintained (at some expense) for compatability.
 * The preferred interface is the kstat accessible nc_stats below.
 */
struct ncstats ncstats;

struct nc_stats ncs = {
	{ "hits",			KSTAT_DATA_UINT64 },
	{ "misses",			KSTAT_DATA_UINT64 },
	{ "negative_cache_hits",	KSTAT_DATA_UINT64 },
	{ "enters",			KSTAT_DATA_UINT64 },
	{ "double_enters",		KSTAT_DATA_UINT64 },
	{ "purge_total_entries",	KSTAT_DATA_UINT64 },
	{ "purge_all",			KSTAT_DATA_UINT64 },
	{ "purge_vp",			KSTAT_DATA_UINT64 },
	{ "purge_vfs",			KSTAT_DATA_UINT64 },
	{ "purge_fs1",			KSTAT_DATA_UINT64 },
	{ "pick_free",			KSTAT_DATA_UINT64 },
	{ "pick_heuristic",		KSTAT_DATA_UINT64 },
	{ "pick_last",			KSTAT_DATA_UINT64 },
};

static int doingcache = 1;
TUNABLE_INT("vfs.zfs.dnlc.enable", &doingcache);
SYSCTL_INT(_vfs_zfs_dnlc, OID_AUTO, enable, CTLFLAG_RDTUN, &doingcache, 0,
    "Enable/disable name cache");

vnode_t negative_cache_vnode;

/*
 * Insert entry at the front of the queue
 */
#define	nc_inshash(ncp, hp) \
{ \
	(ncp)->hash_next = (hp)->hash_next; \
	(ncp)->hash_prev = (ncache_t *)(hp); \
	(hp)->hash_next->hash_prev = (ncp); \
	(hp)->hash_next = (ncp); \
}

/*
 * Remove entry from hash queue
 */
#define	nc_rmhash(ncp) \
{ \
	(ncp)->hash_prev->hash_next = (ncp)->hash_next; \
	(ncp)->hash_next->hash_prev = (ncp)->hash_prev; \
	(ncp)->hash_prev = NULL; \
	(ncp)->hash_next = NULL; \
}

/*
 * Free an entry.
 */
#define	dnlc_free(ncp) \
{ \
	kmem_free((ncp), sizeof (ncache_t) + (ncp)->namlen); \
	atomic_add_32(&dnlc_nentries, -1); \
}


/*
 * Cached directory info.
 * ======================
 */

/*
 * Cached directory free space hash function.
 * Needs the free space handle and the dcp to get the hash table size
 * Returns the hash index.
 */
#define	DDFHASH(handle, dcp) ((handle >> 2) & (dcp)->dc_fhash_mask)

/*
 * Cached directory name entry hash function.
 * Uses the name and returns in the input arguments the hash and the name
 * length.
 */
#define	DNLC_DIR_HASH(name, hash, namelen)			\
	{							\
		char Xc, *Xcp;					\
		hash = *name;					\
		for (Xcp = (name + 1); (Xc = *Xcp) != 0; Xcp++)	\
			hash = (hash << 4) + hash + Xc;		\
		ASSERT((Xcp - (name)) <= ((1 << NBBY) - 1));	\
		namelen = Xcp - (name);				\
	}

/* special dircache_t pointer to indicate error should be returned */
/*
 * The anchor directory cache pointer can contain 3 types of values,
 * 1) NULL: No directory cache
 * 2) DC_RET_LOW_MEM (-1): There was a directory cache that found to be
 *    too big or a memory shortage occurred. This value remains in the
 *    pointer until a dnlc_dir_start() which returns the a DNOMEM error.
 *    This is kludgy but efficient and only visible in this source file.
 * 3) A valid cache pointer.
 */
#define	DC_RET_LOW_MEM (dircache_t *)1
#define	VALID_DIR_CACHE(dcp) ((dircache_t *)(dcp) > DC_RET_LOW_MEM)

/* Prototypes */
static ncache_t *dnlc_get(uchar_t namlen);
static ncache_t *dnlc_search(vnode_t *dp, char *name, uchar_t namlen, int hash);
static void do_dnlc_reduce_cache(void *);
static void dnlc_lowvnodes(void *arg __unused, int nvnodes);

static kstat_t *dnlc_ksp = NULL;
static eventhandler_tag dnlc_event_lowvnodes = NULL;

/*
 * Initialize the directory cache.
 */
static void
dnlc_init(void *arg __unused)
{
	nc_hash_t *hp;
	int i;

	/*
	 * Set up the size of the dnlc (ncsize) and its low water mark.
	 */
	if (ncsize == -1) {
		/* calculate a reasonable size for the low water */
		dnlc_nentries_low_water = (desiredvnodes * 49) / 100;
		ncsize = dnlc_nentries_low_water +
		    (dnlc_nentries_low_water / dnlc_low_water_divisor);
	} else {
		/* don't change the user specified ncsize */
		dnlc_nentries_low_water =
		    ncsize - (ncsize / dnlc_low_water_divisor);
	}
	if (ncsize <= 0) {
		doingcache = 0;
		ncsize = 0;
		cmn_err(CE_NOTE, "name cache (dnlc) disabled");
		return;
	}
	dnlc_max_nentries = ncsize * 2;
	ncsize_onepercent = ncsize / 100;
	ncsize_min_percent = ncsize_onepercent * 3;

	/*
	 * Initialise the hash table.
	 * Compute hash size rounding to the next power of two.
	 */
	nc_hashsz = ncsize / nc_hashavelen;
	nc_hashsz = 1 << highbit(nc_hashsz);
	nc_hashmask = nc_hashsz - 1;
	nc_hash = kmem_zalloc(nc_hashsz * sizeof (*nc_hash), KM_SLEEP);
	for (i = 0; i < nc_hashsz; i++) {
		hp = (nc_hash_t *)&nc_hash[i];
		mutex_init(&hp->hash_lock, NULL, MUTEX_DEFAULT, NULL);
		hp->hash_next = (ncache_t *)hp;
		hp->hash_prev = (ncache_t *)hp;
	}

	/*
	 * Initialize rotors
	 */
	dnlc_free_rotor = dnlc_purge_fs1_rotor = &nc_hash[0];

	/*
	 * Initialise the reference count of the negative cache vnode to 1
	 * so that it never goes away (VOP_INACTIVE isn't called on it).
	 */
	negative_cache_vnode.v_count = 1;
	negative_cache_vnode.v_holdcnt = 1;
	mtx_init(&negative_cache_vnode.v_interlock, "vnode interlock", NULL,
	    MTX_DEF);

	/*
	 * Initialise kstats - both the old compatability raw kind and
	 * the more extensive named stats.
	 */
	dnlc_ksp = kstat_create("zfs", 0, "dnlcstats", "misc", KSTAT_TYPE_NAMED,
	    sizeof (ncs) / sizeof (kstat_named_t), KSTAT_FLAG_VIRTUAL);
	if (dnlc_ksp) {
		dnlc_ksp->ks_data = (void *) &ncs;
		kstat_install(dnlc_ksp);
	}

	dnlc_event_lowvnodes = EVENTHANDLER_REGISTER(vfs_lowvnodes,
	    dnlc_lowvnodes, NULL, EVENTHANDLER_PRI_FIRST);
}

static void
dnlc_fini(void *arg __unused)
{
	nc_hash_t *hp;
	int i;

	if (dnlc_event_lowvnodes != NULL)
		EVENTHANDLER_DEREGISTER(vfs_lowvnodes, dnlc_event_lowvnodes);

	if (dnlc_ksp != NULL) {
		kstat_delete(dnlc_ksp);
		dnlc_ksp = NULL;
	}

	mtx_destroy(&negative_cache_vnode.v_interlock);

	for (i = 0; i < nc_hashsz; i++) {
		hp = (nc_hash_t *)&nc_hash[i];
		mutex_destroy(&hp->hash_lock);
	}
	kmem_free(nc_hash, nc_hashsz * sizeof (*nc_hash));
}

/*
 * Add a name to the directory cache.
 *
 * This function is basically identical with
 * dnlc_enter().  The difference is that when the
 * desired dnlc entry is found, the vnode in the
 * ncache is compared with the vnode passed in.
 *
 * If they are not equal then the ncache is
 * updated with the passed in vnode.  Otherwise
 * it just frees up the newly allocated dnlc entry.
 */
void
dnlc_update(vnode_t *dp, char *name, vnode_t *vp)
{
	ncache_t *ncp;
	ncache_t *tcp;
	vnode_t *tvp;
	nc_hash_t *hp;
	int hash;
	uchar_t namlen;

	TRACE_0(TR_FAC_NFS, TR_DNLC_ENTER_START, "dnlc_update_start:");

	if (!doingcache) {
		TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
		    "dnlc_update_end:(%S) %d", "not caching", 0);
		return;
	}

	/*
	 * Get a new dnlc entry and initialize it now.
	 * If we fail to get a new entry, call dnlc_remove() to purge
	 * any existing dnlc entry including negative cache (DNLC_NO_VNODE)
	 * entry.
	 * Failure to clear an existing entry could result in false dnlc
	 * lookup (negative/stale entry).
	 */
	DNLCHASH(name, dp, hash, namlen);
	if ((ncp = dnlc_get(namlen)) == NULL) {
		dnlc_remove(dp, name);
		return;
	}
	ncp->dp = dp;
	VN_HOLD(dp);
	ncp->vp = vp;
	VN_HOLD(vp);
	bcopy(name, ncp->name, namlen + 1); /* name and null */
	ncp->hash = hash;
	hp = &nc_hash[hash & nc_hashmask];

	mutex_enter(&hp->hash_lock);
	if ((tcp = dnlc_search(dp, name, namlen, hash)) != NULL) {
		if (tcp->vp != vp) {
			tvp = tcp->vp;
			tcp->vp = vp;
			mutex_exit(&hp->hash_lock);
			VN_RELE(tvp);
			ncstats.enters++;
			ncs.ncs_enters.value.ui64++;
			TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
			    "dnlc_update_end:(%S) %d", "done", ncstats.enters);
		} else {
			mutex_exit(&hp->hash_lock);
			VN_RELE(vp);
			ncstats.dbl_enters++;
			ncs.ncs_dbl_enters.value.ui64++;
			TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
			    "dnlc_update_end:(%S) %d",
			    "dbl enter", ncstats.dbl_enters);
		}
		VN_RELE(dp);
		dnlc_free(ncp);		/* crfree done here */
		return;
	}
	/*
	 * insert the new entry, since it is not in dnlc yet
	 */
	nc_inshash(ncp, hp);
	mutex_exit(&hp->hash_lock);
	ncstats.enters++;
	ncs.ncs_enters.value.ui64++;
	TRACE_2(TR_FAC_NFS, TR_DNLC_ENTER_END,
	    "dnlc_update_end:(%S) %d", "done", ncstats.enters);
}

/*
 * Look up a name in the directory name cache.
 *
 * Return a doubly-held vnode if found: one hold so that it may
 * remain in the cache for other users, the other hold so that
 * the cache is not re-cycled and the identity of the vnode is
 * lost before the caller can use the vnode.
 */
vnode_t *
dnlc_lookup(vnode_t *dp, char *name)
{
	ncache_t *ncp;
	nc_hash_t *hp;
	vnode_t *vp;
	int hash, depth;
	uchar_t namlen;

	TRACE_2(TR_FAC_NFS, TR_DNLC_LOOKUP_START,
	    "dnlc_lookup_start:dp %x name %s", dp, name);

	if (!doingcache) {
		TRACE_4(TR_FAC_NFS, TR_DNLC_LOOKUP_END,
		    "dnlc_lookup_end:%S %d vp %x name %s",
		    "not_caching", 0, NULL, name);
		return (NULL);
	}

	DNLCHASH(name, dp, hash, namlen);
	depth = 1;
	hp = &nc_hash[hash & nc_hashmask];
	mutex_enter(&hp->hash_lock);

	for (ncp = hp->hash_next; ncp != (ncache_t *)hp;
	    ncp = ncp->hash_next) {
		if (ncp->hash == hash &&	/* fast signature check */
		    ncp->dp == dp &&
		    ncp->namlen == namlen &&
		    bcmp(ncp->name, name, namlen) == 0) {
			/*
			 * Move this entry to the head of its hash chain
			 * if it's not already close.
			 */
			if (depth > NC_MOVETOFRONT) {
				ncache_t *next = ncp->hash_next;
				ncache_t *prev = ncp->hash_prev;

				prev->hash_next = next;
				next->hash_prev = prev;
				ncp->hash_next = next = hp->hash_next;
				ncp->hash_prev = (ncache_t *)hp;
				next->hash_prev = ncp;
				hp->hash_next = ncp;

				ncstats.move_to_front++;
			}

			/*
			 * Put a hold on the vnode now so its identity
			 * can't change before the caller has a chance to
			 * put a hold on it.
			 */
			vp = ncp->vp;
			VN_HOLD(vp);
			mutex_exit(&hp->hash_lock);
			ncstats.hits++;
			ncs.ncs_hits.value.ui64++;
			if (vp == DNLC_NO_VNODE) {
				ncs.ncs_neg_hits.value.ui64++;
			}
			TRACE_4(TR_FAC_NFS, TR_DNLC_LOOKUP_END,
				"dnlc_lookup_end:%S %d vp %x name %s",
				"hit", ncstats.hits, vp, name);
			return (vp);
		}
		depth++;
	}

	mutex_exit(&hp->hash_lock);
	ncstats.misses++;
	ncs.ncs_misses.value.ui64++;
	TRACE_4(TR_FAC_NFS, TR_DNLC_LOOKUP_END,
		"dnlc_lookup_end:%S %d vp %x name %s", "miss", ncstats.misses,
	    NULL, name);
	return (NULL);
}

/*
 * Remove an entry in the directory name cache.
 */
void
dnlc_remove(vnode_t *dp, char *name)
{
	ncache_t *ncp;
	nc_hash_t *hp;
	uchar_t namlen;
	int hash;

	if (!doingcache)
		return;
	DNLCHASH(name, dp, hash, namlen);
	hp = &nc_hash[hash & nc_hashmask];

	mutex_enter(&hp->hash_lock);
	if (ncp = dnlc_search(dp, name, namlen, hash)) {
		/*
		 * Free up the entry
		 */
		nc_rmhash(ncp);
		mutex_exit(&hp->hash_lock);
		VN_RELE(ncp->vp);
		VN_RELE(ncp->dp);
		dnlc_free(ncp);
		return;
	}
	mutex_exit(&hp->hash_lock);
}

/*
 * Purge cache entries referencing a vfsp.  Caller supplies a count
 * of entries to purge; up to that many will be freed.  A count of
 * zero indicates that all such entries should be purged.  Returns
 * the number of entries that were purged.
 */
int
dnlc_purge_vfsp(vfs_t *vfsp, int count)
{
	nc_hash_t *nch;
	ncache_t *ncp;
	int n = 0;
	int index;
	int i;
	vnode_t *nc_rele[DNLC_MAX_RELE];

	if (!doingcache)
		return (0);

	ncstats.purges++;
	ncs.ncs_purge_vfs.value.ui64++;

	for (nch = nc_hash; nch < &nc_hash[nc_hashsz]; nch++) {
		index = 0;
		mutex_enter(&nch->hash_lock);
		ncp = nch->hash_next;
		while (ncp != (ncache_t *)nch) {
			ncache_t *np;

			np = ncp->hash_next;
			ASSERT(ncp->dp != NULL);
			ASSERT(ncp->vp != NULL);
			if ((ncp->dp->v_vfsp == vfsp) ||
			    (ncp->vp->v_vfsp == vfsp)) {
				n++;
				nc_rele[index++] = ncp->vp;
				nc_rele[index++] = ncp->dp;
				nc_rmhash(ncp);
				dnlc_free(ncp);
				ncs.ncs_purge_total.value.ui64++;
				if (index == DNLC_MAX_RELE) {
					ncp = np;
					break;
				}
				if (count != 0 && n >= count) {
					break;
				}
			}
			ncp = np;
		}
		mutex_exit(&nch->hash_lock);
		/* Release holds on all the vnodes now that we have no locks */
		for (i = 0; i < index; i++) {
			VN_RELE(nc_rele[i]);
		}
		if (count != 0 && n >= count) {
			return (n);
		}
		if (ncp != (ncache_t *)nch) {
			nch--; /* Do current hash chain again */
		}
	}
	return (n);
}

/*
 * Utility routine to search for a cache entry. Return the
 * ncache entry if found, NULL otherwise.
 */
static ncache_t *
dnlc_search(vnode_t *dp, char *name, uchar_t namlen, int hash)
{
	nc_hash_t *hp;
	ncache_t *ncp;

	hp = &nc_hash[hash & nc_hashmask];

	for (ncp = hp->hash_next; ncp != (ncache_t *)hp; ncp = ncp->hash_next) {
		if (ncp->hash == hash &&
		    ncp->dp == dp &&
		    ncp->namlen == namlen &&
		    bcmp(ncp->name, name, namlen) == 0)
			return (ncp);
	}
	return (NULL);
}

#if ((1 << NBBY) - 1) < (MAXNAMELEN - 1)
#error ncache_t name length representation is too small
#endif

void
dnlc_reduce_cache(void *reduce_percent)
{
	if (dnlc_reduce_idle && (dnlc_nentries >= ncsize || reduce_percent)) {
		dnlc_reduce_idle = 0;
		if ((taskq_dispatch(system_taskq, do_dnlc_reduce_cache,
		    reduce_percent, TQ_NOSLEEP)) == 0)
			dnlc_reduce_idle = 1;
	}
}

/*
 * Get a new name cache entry.
 * If the dnlc_reduce_cache() taskq isn't keeping up with demand, or memory
 * is short then just return NULL. If we're over ncsize then kick off a
 * thread to free some in use entries down to dnlc_nentries_low_water.
 * Caller must initialise all fields except namlen.
 * Component names are defined to be less than MAXNAMELEN
 * which includes a null.
 */
static ncache_t *
dnlc_get(uchar_t namlen)
{
	ncache_t *ncp;

	if (dnlc_nentries > dnlc_max_nentries) {
		dnlc_max_nentries_cnt++; /* keep a statistic */
		return (NULL);
	}
	ncp = kmem_alloc(sizeof (ncache_t) + namlen, KM_NOSLEEP);
	if (ncp == NULL) {
		return (NULL);
	}
	ncp->namlen = namlen;
	atomic_add_32(&dnlc_nentries, 1);
	dnlc_reduce_cache(NULL);
	return (ncp);
}

/*
 * Taskq routine to free up name cache entries to reduce the
 * cache size to the low water mark if "reduce_percent" is not provided.
 * If "reduce_percent" is provided, reduce cache size by
 * (ncsize_onepercent * reduce_percent).
 */
/*ARGSUSED*/
static void
do_dnlc_reduce_cache(void *reduce_percent)
{
	nc_hash_t *hp = dnlc_free_rotor, *start_hp = hp;
	vnode_t *vp;
	ncache_t *ncp;
	int cnt;
	uint_t low_water = dnlc_nentries_low_water;

	if (reduce_percent) {
		uint_t reduce_cnt;

		/*
		 * Never try to reduce the current number
		 * of cache entries below 3% of ncsize.
		 */
		if (dnlc_nentries <= ncsize_min_percent) {
			dnlc_reduce_idle = 1;
			return;
		}
		reduce_cnt = ncsize_onepercent *
		    (uint_t)(uintptr_t)reduce_percent;

		if (reduce_cnt > dnlc_nentries ||
		    dnlc_nentries - reduce_cnt < ncsize_min_percent)
			low_water = ncsize_min_percent;
		else
			low_water = dnlc_nentries - reduce_cnt;
	}

	do {
		/*
		 * Find the first non empty hash queue without locking.
		 * Only look at each hash queue once to avoid an infinite loop.
		 */
		do {
			if (++hp == &nc_hash[nc_hashsz])
				hp = nc_hash;
		} while (hp->hash_next == (ncache_t *)hp && hp != start_hp);

		/* return if all hash queues are empty. */
		if (hp->hash_next == (ncache_t *)hp) {
			dnlc_reduce_idle = 1;
			return;
		}

		mutex_enter(&hp->hash_lock);
		for (cnt = 0, ncp = hp->hash_prev; ncp != (ncache_t *)hp;
		    ncp = ncp->hash_prev, cnt++) {
			vp = ncp->vp;
			/*
			 * A name cache entry with a reference count
			 * of one is only referenced by the dnlc.
			 * Also negative cache entries are purged first.
			 */
			if (!vn_has_cached_data(vp) &&
			    ((vp->v_count == 1) || (vp == DNLC_NO_VNODE))) {
				ncs.ncs_pick_heur.value.ui64++;
				goto found;
			}
			/*
			 * Remove from the end of the chain if the
			 * chain is too long
			 */
			if (cnt > dnlc_long_chain) {
				ncp = hp->hash_prev;
				ncs.ncs_pick_last.value.ui64++;
				vp = ncp->vp;
				goto found;
			}
		}
		/* check for race and continue */
		if (hp->hash_next == (ncache_t *)hp) {
			mutex_exit(&hp->hash_lock);
			continue;
		}

		ncp = hp->hash_prev; /* pick the last one in the hash queue */
		ncs.ncs_pick_last.value.ui64++;
		vp = ncp->vp;
found:
		/*
		 * Remove from hash chain.
		 */
		nc_rmhash(ncp);
		mutex_exit(&hp->hash_lock);
		VN_RELE(vp);
		VN_RELE(ncp->dp);
		dnlc_free(ncp);
	} while (dnlc_nentries > low_water);

	dnlc_free_rotor = hp;
	dnlc_reduce_idle = 1;
}

static void
dnlc_lowvnodes(void *arg __unused, int nvnodes)
{

	nvnodes /= ncsize_onepercent;
	/* Free no less than 5%. */
	nvnodes = nvnodes < 5 * ncsize_onepercent ? 5 * ncsize_onepercent : nvnodes;
	dnlc_reduce_cache((void *)(intptr_t)nvnodes);
}

SYSINIT(dnlc, SI_SUB_VFS, SI_ORDER_ANY, dnlc_init, NULL);
SYSUNINIT(dnlc, SI_SUB_VFS, SI_ORDER_ANY, dnlc_fini, NULL);
