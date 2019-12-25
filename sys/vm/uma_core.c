/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2002-2019 Jeffrey Roberson <jeff@FreeBSD.org>
 * Copyright (c) 2004, 2005 Bosko Milekic <bmilekic@FreeBSD.org>
 * Copyright (c) 2004-2006 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * uma_core.c  Implementation of the Universal Memory allocator
 *
 * This allocator is intended to replace the multitude of similar object caches
 * in the standard FreeBSD kernel.  The intent is to be flexible as well as
 * efficient.  A primary design goal is to return unused memory to the rest of
 * the system.  This will make the system as a whole more flexible due to the
 * ability to move memory to subsystems which most need it instead of leaving
 * pools of reserved memory unused.
 *
 * The basic ideas stem from similar slab/zone based allocators whose algorithms
 * are well known.
 *
 */

/*
 * TODO:
 *	- Improve memory usage for large allocations
 *	- Investigate cache size adjustments
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_param.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitset.h>
#include <sys/domainset.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/taskqueue.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_domainset.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>

#include <ddb/ddb.h>

#ifdef DEBUG_MEMGUARD
#include <vm/memguard.h>
#endif

/*
 * This is the zone and keg from which all zones are spawned.
 */
static uma_zone_t kegs;
static uma_zone_t zones;

/* This is the zone from which all offpage uma_slab_ts are allocated. */
static uma_zone_t slabzone;

/*
 * The initial hash tables come out of this zone so they can be allocated
 * prior to malloc coming up.
 */
static uma_zone_t hashzone;

/* The boot-time adjusted value for cache line alignment. */
int uma_align_cache = 64 - 1;

static MALLOC_DEFINE(M_UMAHASH, "UMAHash", "UMA Hash Buckets");
static MALLOC_DEFINE(M_UMA, "UMA", "UMA Misc");

/*
 * Are we allowed to allocate buckets?
 */
static int bucketdisable = 1;

/* Linked list of all kegs in the system */
static LIST_HEAD(,uma_keg) uma_kegs = LIST_HEAD_INITIALIZER(uma_kegs);

/* Linked list of all cache-only zones in the system */
static LIST_HEAD(,uma_zone) uma_cachezones =
    LIST_HEAD_INITIALIZER(uma_cachezones);

/* This RW lock protects the keg list */
static struct rwlock_padalign __exclusive_cache_line uma_rwlock;

/*
 * Pointer and counter to pool of pages, that is preallocated at
 * startup to bootstrap UMA.
 */
static char *bootmem;
static int boot_pages;

static struct sx uma_reclaim_lock;

/*
 * kmem soft limit, initialized by uma_set_limit().  Ensure that early
 * allocations don't trigger a wakeup of the reclaim thread.
 */
unsigned long uma_kmem_limit = LONG_MAX;
SYSCTL_ULONG(_vm, OID_AUTO, uma_kmem_limit, CTLFLAG_RD, &uma_kmem_limit, 0,
    "UMA kernel memory soft limit");
unsigned long uma_kmem_total;
SYSCTL_ULONG(_vm, OID_AUTO, uma_kmem_total, CTLFLAG_RD, &uma_kmem_total, 0,
    "UMA kernel memory usage");

/* Is the VM done starting up? */
static enum { BOOT_COLD = 0, BOOT_STRAPPED, BOOT_PAGEALLOC, BOOT_BUCKETS,
    BOOT_RUNNING } booted = BOOT_COLD;

/*
 * This is the handle used to schedule events that need to happen
 * outside of the allocation fast path.
 */
static struct callout uma_callout;
#define	UMA_TIMEOUT	20		/* Seconds for callout interval. */

/*
 * This structure is passed as the zone ctor arg so that I don't have to create
 * a special allocation function just for zones.
 */
struct uma_zctor_args {
	const char *name;
	size_t size;
	uma_ctor ctor;
	uma_dtor dtor;
	uma_init uminit;
	uma_fini fini;
	uma_import import;
	uma_release release;
	void *arg;
	uma_keg_t keg;
	int align;
	uint32_t flags;
};

struct uma_kctor_args {
	uma_zone_t zone;
	size_t size;
	uma_init uminit;
	uma_fini fini;
	int align;
	uint32_t flags;
};

struct uma_bucket_zone {
	uma_zone_t	ubz_zone;
	char		*ubz_name;
	int		ubz_entries;	/* Number of items it can hold. */
	int		ubz_maxsize;	/* Maximum allocation size per-item. */
};

/*
 * Compute the actual number of bucket entries to pack them in power
 * of two sizes for more efficient space utilization.
 */
#define	BUCKET_SIZE(n)						\
    (((sizeof(void *) * (n)) - sizeof(struct uma_bucket)) / sizeof(void *))

#define	BUCKET_MAX	BUCKET_SIZE(256)
#define	BUCKET_MIN	BUCKET_SIZE(4)

struct uma_bucket_zone bucket_zones[] = {
	{ NULL, "4 Bucket", BUCKET_SIZE(4), 4096 },
	{ NULL, "6 Bucket", BUCKET_SIZE(6), 3072 },
	{ NULL, "8 Bucket", BUCKET_SIZE(8), 2048 },
	{ NULL, "12 Bucket", BUCKET_SIZE(12), 1536 },
	{ NULL, "16 Bucket", BUCKET_SIZE(16), 1024 },
	{ NULL, "32 Bucket", BUCKET_SIZE(32), 512 },
	{ NULL, "64 Bucket", BUCKET_SIZE(64), 256 },
	{ NULL, "128 Bucket", BUCKET_SIZE(128), 128 },
	{ NULL, "256 Bucket", BUCKET_SIZE(256), 64 },
	{ NULL, NULL, 0}
};

/*
 * Flags and enumerations to be passed to internal functions.
 */
enum zfreeskip {
	SKIP_NONE =	0,
	SKIP_CNT =	0x00000001,
	SKIP_DTOR =	0x00010000,
	SKIP_FINI =	0x00020000,
};

/* Prototypes.. */

int	uma_startup_count(int);
void	uma_startup(void *, int);
void	uma_startup1(void);
void	uma_startup2(void);

static void *noobj_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void *page_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void *pcpu_page_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void *startup_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void page_free(void *, vm_size_t, uint8_t);
static void pcpu_page_free(void *, vm_size_t, uint8_t);
static uma_slab_t keg_alloc_slab(uma_keg_t, uma_zone_t, int, int, int);
static void cache_drain(uma_zone_t);
static void bucket_drain(uma_zone_t, uma_bucket_t);
static void bucket_cache_reclaim(uma_zone_t zone, bool);
static int keg_ctor(void *, int, void *, int);
static void keg_dtor(void *, int, void *);
static int zone_ctor(void *, int, void *, int);
static void zone_dtor(void *, int, void *);
static int zero_init(void *, int, int);
static void keg_small_init(uma_keg_t keg);
static void keg_large_init(uma_keg_t keg);
static void zone_foreach(void (*zfunc)(uma_zone_t, void *), void *);
static void zone_timeout(uma_zone_t zone, void *);
static int hash_alloc(struct uma_hash *, u_int);
static int hash_expand(struct uma_hash *, struct uma_hash *);
static void hash_free(struct uma_hash *hash);
static void uma_timeout(void *);
static void uma_startup3(void);
static void *zone_alloc_item(uma_zone_t, void *, int, int);
static void *zone_alloc_item_locked(uma_zone_t, void *, int, int);
static void zone_free_item(uma_zone_t, void *, void *, enum zfreeskip);
static void bucket_enable(void);
static void bucket_init(void);
static uma_bucket_t bucket_alloc(uma_zone_t zone, void *, int);
static void bucket_free(uma_zone_t zone, uma_bucket_t, void *);
static void bucket_zone_drain(void);
static uma_bucket_t zone_alloc_bucket(uma_zone_t, void *, int, int);
static void *slab_alloc_item(uma_keg_t keg, uma_slab_t slab);
static void slab_free_item(uma_zone_t zone, uma_slab_t slab, void *item);
static uma_keg_t uma_kcreate(uma_zone_t zone, size_t size, uma_init uminit,
    uma_fini fini, int align, uint32_t flags);
static int zone_import(void *, void **, int, int, int);
static void zone_release(void *, void **, int);
static void uma_zero_item(void *, uma_zone_t);
static bool cache_alloc(uma_zone_t, uma_cache_t, void *, int);
static bool cache_free(uma_zone_t, uma_cache_t, void *, void *, int);

static int sysctl_vm_zone_count(SYSCTL_HANDLER_ARGS);
static int sysctl_vm_zone_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_zone_allocs(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_zone_frees(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_zone_flags(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_slab_efficiency(SYSCTL_HANDLER_ARGS);

#ifdef INVARIANTS
static inline struct noslabbits *slab_dbg_bits(uma_slab_t slab, uma_keg_t keg);

static bool uma_dbg_kskip(uma_keg_t keg, void *mem);
static bool uma_dbg_zskip(uma_zone_t zone, void *mem);
static void uma_dbg_free(uma_zone_t zone, uma_slab_t slab, void *item);
static void uma_dbg_alloc(uma_zone_t zone, uma_slab_t slab, void *item);

static SYSCTL_NODE(_vm, OID_AUTO, debug, CTLFLAG_RD, 0,
    "Memory allocation debugging");

static u_int dbg_divisor = 1;
SYSCTL_UINT(_vm_debug, OID_AUTO, divisor,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &dbg_divisor, 0,
    "Debug & thrash every this item in memory allocator");

static counter_u64_t uma_dbg_cnt = EARLY_COUNTER;
static counter_u64_t uma_skip_cnt = EARLY_COUNTER;
SYSCTL_COUNTER_U64(_vm_debug, OID_AUTO, trashed, CTLFLAG_RD,
    &uma_dbg_cnt, "memory items debugged");
SYSCTL_COUNTER_U64(_vm_debug, OID_AUTO, skipped, CTLFLAG_RD,
    &uma_skip_cnt, "memory items skipped, not debugged");
#endif

SYSINIT(uma_startup3, SI_SUB_VM_CONF, SI_ORDER_SECOND, uma_startup3, NULL);

SYSCTL_NODE(_vm, OID_AUTO, uma, CTLFLAG_RW, 0, "Universal Memory Allocator");

SYSCTL_PROC(_vm, OID_AUTO, zone_count, CTLFLAG_RD|CTLTYPE_INT,
    0, 0, sysctl_vm_zone_count, "I", "Number of UMA zones");

SYSCTL_PROC(_vm, OID_AUTO, zone_stats, CTLFLAG_RD|CTLTYPE_STRUCT,
    0, 0, sysctl_vm_zone_stats, "s,struct uma_type_header", "Zone Stats");

static int zone_warnings = 1;
SYSCTL_INT(_vm, OID_AUTO, zone_warnings, CTLFLAG_RWTUN, &zone_warnings, 0,
    "Warn when UMA zones becomes full");

/*
 * This routine checks to see whether or not it's safe to enable buckets.
 */
static void
bucket_enable(void)
{

	KASSERT(booted >= BOOT_BUCKETS, ("Bucket enable before init"));
	bucketdisable = vm_page_count_min();
}

/*
 * Initialize bucket_zones, the array of zones of buckets of various sizes.
 *
 * For each zone, calculate the memory required for each bucket, consisting
 * of the header and an array of pointers.
 */
static void
bucket_init(void)
{
	struct uma_bucket_zone *ubz;
	int size;

	for (ubz = &bucket_zones[0]; ubz->ubz_entries != 0; ubz++) {
		size = roundup(sizeof(struct uma_bucket), sizeof(void *));
		size += sizeof(void *) * ubz->ubz_entries;
		ubz->ubz_zone = uma_zcreate(ubz->ubz_name, size,
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
		    UMA_ZONE_MTXCLASS | UMA_ZFLAG_BUCKET | UMA_ZONE_NUMA);
	}
}

/*
 * Given a desired number of entries for a bucket, return the zone from which
 * to allocate the bucket.
 */
static struct uma_bucket_zone *
bucket_zone_lookup(int entries)
{
	struct uma_bucket_zone *ubz;

	for (ubz = &bucket_zones[0]; ubz->ubz_entries != 0; ubz++)
		if (ubz->ubz_entries >= entries)
			return (ubz);
	ubz--;
	return (ubz);
}

static struct uma_bucket_zone *
bucket_zone_max(uma_zone_t zone, int nitems)
{
	struct uma_bucket_zone *ubz;
	int bpcpu;

	bpcpu = 2;
#ifdef UMA_XDOMAIN
	if ((zone->uz_flags & UMA_ZONE_NUMA) != 0)
		/* Count the cross-domain bucket. */
		bpcpu++;
#endif

	for (ubz = &bucket_zones[0]; ubz->ubz_entries != 0; ubz++)
		if (ubz->ubz_entries * bpcpu * mp_ncpus > nitems)
			break;
	if (ubz == &bucket_zones[0])
		ubz = NULL;
	else
		ubz--;
	return (ubz);
}

static int
bucket_select(int size)
{
	struct uma_bucket_zone *ubz;

	ubz = &bucket_zones[0];
	if (size > ubz->ubz_maxsize)
		return MAX((ubz->ubz_maxsize * ubz->ubz_entries) / size, 1);

	for (; ubz->ubz_entries != 0; ubz++)
		if (ubz->ubz_maxsize < size)
			break;
	ubz--;
	return (ubz->ubz_entries);
}

static uma_bucket_t
bucket_alloc(uma_zone_t zone, void *udata, int flags)
{
	struct uma_bucket_zone *ubz;
	uma_bucket_t bucket;

	/*
	 * This is to stop us from allocating per cpu buckets while we're
	 * running out of vm.boot_pages.  Otherwise, we would exhaust the
	 * boot pages.  This also prevents us from allocating buckets in
	 * low memory situations.
	 */
	if (bucketdisable)
		return (NULL);
	/*
	 * To limit bucket recursion we store the original zone flags
	 * in a cookie passed via zalloc_arg/zfree_arg.  This allows the
	 * NOVM flag to persist even through deep recursions.  We also
	 * store ZFLAG_BUCKET once we have recursed attempting to allocate
	 * a bucket for a bucket zone so we do not allow infinite bucket
	 * recursion.  This cookie will even persist to frees of unused
	 * buckets via the allocation path or bucket allocations in the
	 * free path.
	 */
	if ((zone->uz_flags & UMA_ZFLAG_BUCKET) == 0)
		udata = (void *)(uintptr_t)zone->uz_flags;
	else {
		if ((uintptr_t)udata & UMA_ZFLAG_BUCKET)
			return (NULL);
		udata = (void *)((uintptr_t)udata | UMA_ZFLAG_BUCKET);
	}
	if ((uintptr_t)udata & UMA_ZFLAG_CACHEONLY)
		flags |= M_NOVM;
	ubz = bucket_zone_lookup(zone->uz_bucket_size);
	if (ubz->ubz_zone == zone && (ubz + 1)->ubz_entries != 0)
		ubz++;
	bucket = uma_zalloc_arg(ubz->ubz_zone, udata, flags);
	if (bucket) {
#ifdef INVARIANTS
		bzero(bucket->ub_bucket, sizeof(void *) * ubz->ubz_entries);
#endif
		bucket->ub_cnt = 0;
		bucket->ub_entries = ubz->ubz_entries;
	}

	return (bucket);
}

static void
bucket_free(uma_zone_t zone, uma_bucket_t bucket, void *udata)
{
	struct uma_bucket_zone *ubz;

	KASSERT(bucket->ub_cnt == 0,
	    ("bucket_free: Freeing a non free bucket."));
	if ((zone->uz_flags & UMA_ZFLAG_BUCKET) == 0)
		udata = (void *)(uintptr_t)zone->uz_flags;
	ubz = bucket_zone_lookup(bucket->ub_entries);
	uma_zfree_arg(ubz->ubz_zone, bucket, udata);
}

static void
bucket_zone_drain(void)
{
	struct uma_bucket_zone *ubz;

	for (ubz = &bucket_zones[0]; ubz->ubz_entries != 0; ubz++)
		uma_zone_reclaim(ubz->ubz_zone, UMA_RECLAIM_DRAIN);
}

/*
 * Attempt to satisfy an allocation by retrieving a full bucket from one of the
 * zone's caches.
 */
static uma_bucket_t
zone_fetch_bucket(uma_zone_t zone, uma_zone_domain_t zdom)
{
	uma_bucket_t bucket;

	ZONE_LOCK_ASSERT(zone);

	if ((bucket = TAILQ_FIRST(&zdom->uzd_buckets)) != NULL) {
		MPASS(zdom->uzd_nitems >= bucket->ub_cnt);
		TAILQ_REMOVE(&zdom->uzd_buckets, bucket, ub_link);
		zdom->uzd_nitems -= bucket->ub_cnt;
		if (zdom->uzd_imin > zdom->uzd_nitems)
			zdom->uzd_imin = zdom->uzd_nitems;
		zone->uz_bkt_count -= bucket->ub_cnt;
	}
	return (bucket);
}

/*
 * Insert a full bucket into the specified cache.  The "ws" parameter indicates
 * whether the bucket's contents should be counted as part of the zone's working
 * set.
 */
static void
zone_put_bucket(uma_zone_t zone, uma_zone_domain_t zdom, uma_bucket_t bucket,
    const bool ws)
{

	ZONE_LOCK_ASSERT(zone);
	KASSERT(!ws || zone->uz_bkt_count < zone->uz_bkt_max,
	    ("%s: zone %p overflow", __func__, zone));

	if (ws)
		TAILQ_INSERT_HEAD(&zdom->uzd_buckets, bucket, ub_link);
	else
		TAILQ_INSERT_TAIL(&zdom->uzd_buckets, bucket, ub_link);
	zdom->uzd_nitems += bucket->ub_cnt;
	if (ws && zdom->uzd_imax < zdom->uzd_nitems)
		zdom->uzd_imax = zdom->uzd_nitems;
	zone->uz_bkt_count += bucket->ub_cnt;
}

static void
zone_log_warning(uma_zone_t zone)
{
	static const struct timeval warninterval = { 300, 0 };

	if (!zone_warnings || zone->uz_warning == NULL)
		return;

	if (ratecheck(&zone->uz_ratecheck, &warninterval))
		printf("[zone: %s] %s\n", zone->uz_name, zone->uz_warning);
}

static inline void
zone_maxaction(uma_zone_t zone)
{

	if (zone->uz_maxaction.ta_func != NULL)
		taskqueue_enqueue(taskqueue_thread, &zone->uz_maxaction);
}

/*
 * Routine called by timeout which is used to fire off some time interval
 * based calculations.  (stats, hash size, etc.)
 *
 * Arguments:
 *	arg   Unused
 *
 * Returns:
 *	Nothing
 */
static void
uma_timeout(void *unused)
{
	bucket_enable();
	zone_foreach(zone_timeout, NULL);

	/* Reschedule this event */
	callout_reset(&uma_callout, UMA_TIMEOUT * hz, uma_timeout, NULL);
}

/*
 * Update the working set size estimate for the zone's bucket cache.
 * The constants chosen here are somewhat arbitrary.  With an update period of
 * 20s (UMA_TIMEOUT), this estimate is dominated by zone activity over the
 * last 100s.
 */
static void
zone_domain_update_wss(uma_zone_domain_t zdom)
{
	long wss;

	MPASS(zdom->uzd_imax >= zdom->uzd_imin);
	wss = zdom->uzd_imax - zdom->uzd_imin;
	zdom->uzd_imax = zdom->uzd_imin = zdom->uzd_nitems;
	zdom->uzd_wss = (4 * wss + zdom->uzd_wss) / 5;
}

/*
 * Routine to perform timeout driven calculations.  This expands the
 * hashes and does per cpu statistics aggregation.
 *
 *  Returns nothing.
 */
static void
zone_timeout(uma_zone_t zone, void *unused)
{
	uma_keg_t keg;
	u_int slabs;

	if ((zone->uz_flags & UMA_ZONE_HASH) == 0)
		goto update_wss;

	keg = zone->uz_keg;
	KEG_LOCK(keg);
	/*
	 * Expand the keg hash table.
	 *
	 * This is done if the number of slabs is larger than the hash size.
	 * What I'm trying to do here is completely reduce collisions.  This
	 * may be a little aggressive.  Should I allow for two collisions max?
	 */
	if (keg->uk_flags & UMA_ZONE_HASH &&
	    (slabs = keg->uk_pages / keg->uk_ppera) >
	     keg->uk_hash.uh_hashsize) {
		struct uma_hash newhash;
		struct uma_hash oldhash;
		int ret;

		/*
		 * This is so involved because allocating and freeing
		 * while the keg lock is held will lead to deadlock.
		 * I have to do everything in stages and check for
		 * races.
		 */
		KEG_UNLOCK(keg);
		ret = hash_alloc(&newhash, 1 << fls(slabs));
		KEG_LOCK(keg);
		if (ret) {
			if (hash_expand(&keg->uk_hash, &newhash)) {
				oldhash = keg->uk_hash;
				keg->uk_hash = newhash;
			} else
				oldhash = newhash;

			KEG_UNLOCK(keg);
			hash_free(&oldhash);
			return;
		}
	}
	KEG_UNLOCK(keg);

update_wss:
	ZONE_LOCK(zone);
	for (int i = 0; i < vm_ndomains; i++)
		zone_domain_update_wss(&zone->uz_domain[i]);
	ZONE_UNLOCK(zone);
}

/*
 * Allocate and zero fill the next sized hash table from the appropriate
 * backing store.
 *
 * Arguments:
 *	hash  A new hash structure with the old hash size in uh_hashsize
 *
 * Returns:
 *	1 on success and 0 on failure.
 */
static int
hash_alloc(struct uma_hash *hash, u_int size)
{
	size_t alloc;

	KASSERT(powerof2(size), ("hash size must be power of 2"));
	if (size > UMA_HASH_SIZE_INIT)  {
		hash->uh_hashsize = size;
		alloc = sizeof(hash->uh_slab_hash[0]) * hash->uh_hashsize;
		hash->uh_slab_hash = malloc(alloc, M_UMAHASH, M_NOWAIT);
	} else {
		alloc = sizeof(hash->uh_slab_hash[0]) * UMA_HASH_SIZE_INIT;
		hash->uh_slab_hash = zone_alloc_item(hashzone, NULL,
		    UMA_ANYDOMAIN, M_WAITOK);
		hash->uh_hashsize = UMA_HASH_SIZE_INIT;
	}
	if (hash->uh_slab_hash) {
		bzero(hash->uh_slab_hash, alloc);
		hash->uh_hashmask = hash->uh_hashsize - 1;
		return (1);
	}

	return (0);
}

/*
 * Expands the hash table for HASH zones.  This is done from zone_timeout
 * to reduce collisions.  This must not be done in the regular allocation
 * path, otherwise, we can recurse on the vm while allocating pages.
 *
 * Arguments:
 *	oldhash  The hash you want to expand
 *	newhash  The hash structure for the new table
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 */
static int
hash_expand(struct uma_hash *oldhash, struct uma_hash *newhash)
{
	uma_hash_slab_t slab;
	u_int hval;
	u_int idx;

	if (!newhash->uh_slab_hash)
		return (0);

	if (oldhash->uh_hashsize >= newhash->uh_hashsize)
		return (0);

	/*
	 * I need to investigate hash algorithms for resizing without a
	 * full rehash.
	 */

	for (idx = 0; idx < oldhash->uh_hashsize; idx++)
		while (!LIST_EMPTY(&oldhash->uh_slab_hash[idx])) {
			slab = LIST_FIRST(&oldhash->uh_slab_hash[idx]);
			LIST_REMOVE(slab, uhs_hlink);
			hval = UMA_HASH(newhash, slab->uhs_data);
			LIST_INSERT_HEAD(&newhash->uh_slab_hash[hval],
			    slab, uhs_hlink);
		}

	return (1);
}

/*
 * Free the hash bucket to the appropriate backing store.
 *
 * Arguments:
 *	slab_hash  The hash bucket we're freeing
 *	hashsize   The number of entries in that hash bucket
 *
 * Returns:
 *	Nothing
 */
static void
hash_free(struct uma_hash *hash)
{
	if (hash->uh_slab_hash == NULL)
		return;
	if (hash->uh_hashsize == UMA_HASH_SIZE_INIT)
		zone_free_item(hashzone, hash->uh_slab_hash, NULL, SKIP_NONE);
	else
		free(hash->uh_slab_hash, M_UMAHASH);
}

/*
 * Frees all outstanding items in a bucket
 *
 * Arguments:
 *	zone   The zone to free to, must be unlocked.
 *	bucket The free/alloc bucket with items, cpu queue must be locked.
 *
 * Returns:
 *	Nothing
 */

static void
bucket_drain(uma_zone_t zone, uma_bucket_t bucket)
{
	int i;

	if (bucket == NULL)
		return;

	if (zone->uz_fini)
		for (i = 0; i < bucket->ub_cnt; i++) 
			zone->uz_fini(bucket->ub_bucket[i], zone->uz_size);
	zone->uz_release(zone->uz_arg, bucket->ub_bucket, bucket->ub_cnt);
	if (zone->uz_max_items > 0) {
		ZONE_LOCK(zone);
		zone->uz_items -= bucket->ub_cnt;
		if (zone->uz_sleepers && zone->uz_items < zone->uz_max_items)
			wakeup_one(zone);
		ZONE_UNLOCK(zone);
	}
	bucket->ub_cnt = 0;
}

/*
 * Drains the per cpu caches for a zone.
 *
 * NOTE: This may only be called while the zone is being turn down, and not
 * during normal operation.  This is necessary in order that we do not have
 * to migrate CPUs to drain the per-CPU caches.
 *
 * Arguments:
 *	zone     The zone to drain, must be unlocked.
 *
 * Returns:
 *	Nothing
 */
static void
cache_drain(uma_zone_t zone)
{
	uma_cache_t cache;
	int cpu;

	/*
	 * XXX: It is safe to not lock the per-CPU caches, because we're
	 * tearing down the zone anyway.  I.e., there will be no further use
	 * of the caches at this point.
	 *
	 * XXX: It would good to be able to assert that the zone is being
	 * torn down to prevent improper use of cache_drain().
	 *
	 * XXX: We lock the zone before passing into bucket_cache_reclaim() as
	 * it is used elsewhere.  Should the tear-down path be made special
	 * there in some form?
	 */
	CPU_FOREACH(cpu) {
		cache = &zone->uz_cpu[cpu];
		bucket_drain(zone, cache->uc_allocbucket);
		if (cache->uc_allocbucket != NULL)
			bucket_free(zone, cache->uc_allocbucket, NULL);
		cache->uc_allocbucket = NULL;
		bucket_drain(zone, cache->uc_freebucket);
		if (cache->uc_freebucket != NULL)
			bucket_free(zone, cache->uc_freebucket, NULL);
		cache->uc_freebucket = NULL;
		bucket_drain(zone, cache->uc_crossbucket);
		if (cache->uc_crossbucket != NULL)
			bucket_free(zone, cache->uc_crossbucket, NULL);
		cache->uc_crossbucket = NULL;
	}
	ZONE_LOCK(zone);
	bucket_cache_reclaim(zone, true);
	ZONE_UNLOCK(zone);
}

static void
cache_shrink(uma_zone_t zone, void *unused)
{

	if (zone->uz_flags & UMA_ZFLAG_INTERNAL)
		return;

	ZONE_LOCK(zone);
	zone->uz_bucket_size =
	    (zone->uz_bucket_size_min + zone->uz_bucket_size) / 2;
	ZONE_UNLOCK(zone);
}

static void
cache_drain_safe_cpu(uma_zone_t zone, void *unused)
{
	uma_cache_t cache;
	uma_bucket_t b1, b2, b3;
	int domain;

	if (zone->uz_flags & UMA_ZFLAG_INTERNAL)
		return;

	b1 = b2 = b3 = NULL;
	ZONE_LOCK(zone);
	critical_enter();
	if (zone->uz_flags & UMA_ZONE_NUMA)
		domain = PCPU_GET(domain);
	else
		domain = 0;
	cache = &zone->uz_cpu[curcpu];
	if (cache->uc_allocbucket) {
		if (cache->uc_allocbucket->ub_cnt != 0)
			zone_put_bucket(zone, &zone->uz_domain[domain],
			    cache->uc_allocbucket, false);
		else
			b1 = cache->uc_allocbucket;
		cache->uc_allocbucket = NULL;
	}
	if (cache->uc_freebucket) {
		if (cache->uc_freebucket->ub_cnt != 0)
			zone_put_bucket(zone, &zone->uz_domain[domain],
			    cache->uc_freebucket, false);
		else
			b2 = cache->uc_freebucket;
		cache->uc_freebucket = NULL;
	}
	b3 = cache->uc_crossbucket;
	cache->uc_crossbucket = NULL;
	critical_exit();
	ZONE_UNLOCK(zone);
	if (b1)
		bucket_free(zone, b1, NULL);
	if (b2)
		bucket_free(zone, b2, NULL);
	if (b3) {
		bucket_drain(zone, b3);
		bucket_free(zone, b3, NULL);
	}
}

/*
 * Safely drain per-CPU caches of a zone(s) to alloc bucket.
 * This is an expensive call because it needs to bind to all CPUs
 * one by one and enter a critical section on each of them in order
 * to safely access their cache buckets.
 * Zone lock must not be held on call this function.
 */
static void
pcpu_cache_drain_safe(uma_zone_t zone)
{
	int cpu;

	/*
	 * Polite bucket sizes shrinking was not enouth, shrink aggressively.
	 */
	if (zone)
		cache_shrink(zone, NULL);
	else
		zone_foreach(cache_shrink, NULL);

	CPU_FOREACH(cpu) {
		thread_lock(curthread);
		sched_bind(curthread, cpu);
		thread_unlock(curthread);

		if (zone)
			cache_drain_safe_cpu(zone, NULL);
		else
			zone_foreach(cache_drain_safe_cpu, NULL);
	}
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);
}

/*
 * Reclaim cached buckets from a zone.  All buckets are reclaimed if the caller
 * requested a drain, otherwise the per-domain caches are trimmed to either
 * estimated working set size.
 */
static void
bucket_cache_reclaim(uma_zone_t zone, bool drain)
{
	uma_zone_domain_t zdom;
	uma_bucket_t bucket;
	long target, tofree;
	int i;

	for (i = 0; i < vm_ndomains; i++) {
		zdom = &zone->uz_domain[i];

		/*
		 * If we were asked to drain the zone, we are done only once
		 * this bucket cache is empty.  Otherwise, we reclaim items in
		 * excess of the zone's estimated working set size.  If the
		 * difference nitems - imin is larger than the WSS estimate,
		 * then the estimate will grow at the end of this interval and
		 * we ignore the historical average.
		 */
		target = drain ? 0 : lmax(zdom->uzd_wss, zdom->uzd_nitems -
		    zdom->uzd_imin);
		while (zdom->uzd_nitems > target) {
			bucket = TAILQ_LAST(&zdom->uzd_buckets, uma_bucketlist);
			if (bucket == NULL)
				break;
			tofree = bucket->ub_cnt;
			TAILQ_REMOVE(&zdom->uzd_buckets, bucket, ub_link);
			zdom->uzd_nitems -= tofree;

			/*
			 * Shift the bounds of the current WSS interval to avoid
			 * perturbing the estimate.
			 */
			zdom->uzd_imax -= lmin(zdom->uzd_imax, tofree);
			zdom->uzd_imin -= lmin(zdom->uzd_imin, tofree);

			ZONE_UNLOCK(zone);
			bucket_drain(zone, bucket);
			bucket_free(zone, bucket, NULL);
			ZONE_LOCK(zone);
		}
	}

	/*
	 * Shrink the zone bucket size to ensure that the per-CPU caches
	 * don't grow too large.
	 */
	if (zone->uz_bucket_size > zone->uz_bucket_size_min)
		zone->uz_bucket_size--;
}

static void
keg_free_slab(uma_keg_t keg, uma_slab_t slab, int start)
{
	uint8_t *mem;
	int i;
	uint8_t flags;

	CTR4(KTR_UMA, "keg_free_slab keg %s(%p) slab %p, returning %d bytes",
	    keg->uk_name, keg, slab, PAGE_SIZE * keg->uk_ppera);

	mem = slab_data(slab, keg);
	flags = slab->us_flags;
	i = start;
	if (keg->uk_fini != NULL) {
		for (i--; i > -1; i--)
#ifdef INVARIANTS
		/*
		 * trash_fini implies that dtor was trash_dtor. trash_fini
		 * would check that memory hasn't been modified since free,
		 * which executed trash_dtor.
		 * That's why we need to run uma_dbg_kskip() check here,
		 * albeit we don't make skip check for other init/fini
		 * invocations.
		 */
		if (!uma_dbg_kskip(keg, slab_item(slab, keg, i)) ||
		    keg->uk_fini != trash_fini)
#endif
			keg->uk_fini(slab_item(slab, keg, i), keg->uk_size);
	}
	if (keg->uk_flags & UMA_ZONE_OFFPAGE)
		zone_free_item(keg->uk_slabzone, slab, NULL, SKIP_NONE);
	keg->uk_freef(mem, PAGE_SIZE * keg->uk_ppera, flags);
	uma_total_dec(PAGE_SIZE * keg->uk_ppera);
}

/*
 * Frees pages from a keg back to the system.  This is done on demand from
 * the pageout daemon.
 *
 * Returns nothing.
 */
static void
keg_drain(uma_keg_t keg)
{
	struct slabhead freeslabs = { 0 };
	uma_domain_t dom;
	uma_slab_t slab, tmp;
	int i;

	/*
	 * We don't want to take pages from statically allocated kegs at this
	 * time
	 */
	if (keg->uk_flags & UMA_ZONE_NOFREE || keg->uk_freef == NULL)
		return;

	CTR3(KTR_UMA, "keg_drain %s(%p) free items: %u",
	    keg->uk_name, keg, keg->uk_free);
	KEG_LOCK(keg);
	if (keg->uk_free == 0)
		goto finished;

	for (i = 0; i < vm_ndomains; i++) {
		dom = &keg->uk_domain[i];
		LIST_FOREACH_SAFE(slab, &dom->ud_free_slab, us_link, tmp) {
			/* We have nowhere to free these to. */
			if (slab->us_flags & UMA_SLAB_BOOT)
				continue;

			LIST_REMOVE(slab, us_link);
			keg->uk_pages -= keg->uk_ppera;
			keg->uk_free -= keg->uk_ipers;

			if (keg->uk_flags & UMA_ZONE_HASH)
				UMA_HASH_REMOVE(&keg->uk_hash, slab);

			LIST_INSERT_HEAD(&freeslabs, slab, us_link);
		}
	}

finished:
	KEG_UNLOCK(keg);

	while ((slab = LIST_FIRST(&freeslabs)) != NULL) {
		LIST_REMOVE(slab, us_link);
		keg_free_slab(keg, slab, keg->uk_ipers);
	}
}

static void
zone_reclaim(uma_zone_t zone, int waitok, bool drain)
{

	/*
	 * Set draining to interlock with zone_dtor() so we can release our
	 * locks as we go.  Only dtor() should do a WAITOK call since it
	 * is the only call that knows the structure will still be available
	 * when it wakes up.
	 */
	ZONE_LOCK(zone);
	while (zone->uz_flags & UMA_ZFLAG_RECLAIMING) {
		if (waitok == M_NOWAIT)
			goto out;
		msleep(zone, zone->uz_lockptr, PVM, "zonedrain", 1);
	}
	zone->uz_flags |= UMA_ZFLAG_RECLAIMING;
	bucket_cache_reclaim(zone, drain);
	ZONE_UNLOCK(zone);

	/*
	 * The DRAINING flag protects us from being freed while
	 * we're running.  Normally the uma_rwlock would protect us but we
	 * must be able to release and acquire the right lock for each keg.
	 */
	if ((zone->uz_flags & UMA_ZFLAG_CACHE) == 0)
		keg_drain(zone->uz_keg);
	ZONE_LOCK(zone);
	zone->uz_flags &= ~UMA_ZFLAG_RECLAIMING;
	wakeup(zone);
out:
	ZONE_UNLOCK(zone);
}

static void
zone_drain(uma_zone_t zone, void *unused)
{

	zone_reclaim(zone, M_NOWAIT, true);
}

static void
zone_trim(uma_zone_t zone, void *unused)
{

	zone_reclaim(zone, M_NOWAIT, false);
}

/*
 * Allocate a new slab for a keg.  This does not insert the slab onto a list.
 * If the allocation was successful, the keg lock will be held upon return,
 * otherwise the keg will be left unlocked.
 *
 * Arguments:
 *	flags   Wait flags for the item initialization routine
 *	aflags  Wait flags for the slab allocation
 *
 * Returns:
 *	The slab that was allocated or NULL if there is no memory and the
 *	caller specified M_NOWAIT.
 */
static uma_slab_t
keg_alloc_slab(uma_keg_t keg, uma_zone_t zone, int domain, int flags,
    int aflags)
{
	uma_alloc allocf;
	uma_slab_t slab;
	unsigned long size;
	uint8_t *mem;
	uint8_t sflags;
	int i;

	KASSERT(domain >= 0 && domain < vm_ndomains,
	    ("keg_alloc_slab: domain %d out of range", domain));
	KEG_LOCK_ASSERT(keg);
	MPASS(zone->uz_lockptr == &keg->uk_lock);

	allocf = keg->uk_allocf;
	KEG_UNLOCK(keg);

	slab = NULL;
	mem = NULL;
	if (keg->uk_flags & UMA_ZONE_OFFPAGE) {
		slab = zone_alloc_item(keg->uk_slabzone, NULL, domain, aflags);
		if (slab == NULL)
			goto out;
	}

	/*
	 * This reproduces the old vm_zone behavior of zero filling pages the
	 * first time they are added to a zone.
	 *
	 * Malloced items are zeroed in uma_zalloc.
	 */

	if ((keg->uk_flags & UMA_ZONE_MALLOC) == 0)
		aflags |= M_ZERO;
	else
		aflags &= ~M_ZERO;

	if (keg->uk_flags & UMA_ZONE_NODUMP)
		aflags |= M_NODUMP;

	/* zone is passed for legacy reasons. */
	size = keg->uk_ppera * PAGE_SIZE;
	mem = allocf(zone, size, domain, &sflags, aflags);
	if (mem == NULL) {
		if (keg->uk_flags & UMA_ZONE_OFFPAGE)
			zone_free_item(keg->uk_slabzone, slab, NULL, SKIP_NONE);
		slab = NULL;
		goto out;
	}
	uma_total_inc(size);

	/* Point the slab into the allocated memory */
	if (!(keg->uk_flags & UMA_ZONE_OFFPAGE))
		slab = (uma_slab_t )(mem + keg->uk_pgoff);
	else
		((uma_hash_slab_t)slab)->uhs_data = mem;

	if (keg->uk_flags & UMA_ZONE_VTOSLAB)
		for (i = 0; i < keg->uk_ppera; i++)
			vsetzoneslab((vm_offset_t)mem + (i * PAGE_SIZE),
			    zone, slab);

	slab->us_freecount = keg->uk_ipers;
	slab->us_flags = sflags;
	slab->us_domain = domain;
	BIT_FILL(keg->uk_ipers, &slab->us_free);
#ifdef INVARIANTS
	BIT_ZERO(keg->uk_ipers, slab_dbg_bits(slab, keg));
#endif

	if (keg->uk_init != NULL) {
		for (i = 0; i < keg->uk_ipers; i++)
			if (keg->uk_init(slab_item(slab, keg, i),
			    keg->uk_size, flags) != 0)
				break;
		if (i != keg->uk_ipers) {
			keg_free_slab(keg, slab, i);
			slab = NULL;
			goto out;
		}
	}
	KEG_LOCK(keg);

	CTR3(KTR_UMA, "keg_alloc_slab: allocated slab %p for %s(%p)",
	    slab, keg->uk_name, keg);

	if (keg->uk_flags & UMA_ZONE_HASH)
		UMA_HASH_INSERT(&keg->uk_hash, slab, mem);

	keg->uk_pages += keg->uk_ppera;
	keg->uk_free += keg->uk_ipers;

out:
	return (slab);
}

/*
 * This function is intended to be used early on in place of page_alloc() so
 * that we may use the boot time page cache to satisfy allocations before
 * the VM is ready.
 */
static void *
startup_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *pflag,
    int wait)
{
	uma_keg_t keg;
	void *mem;
	int pages;

	keg = zone->uz_keg;
	/*
	 * If we are in BOOT_BUCKETS or higher, than switch to real
	 * allocator.  Zones with page sized slabs switch at BOOT_PAGEALLOC.
	 */
	switch (booted) {
		case BOOT_COLD:
		case BOOT_STRAPPED:
			break;
		case BOOT_PAGEALLOC:
			if (keg->uk_ppera > 1)
				break;
		case BOOT_BUCKETS:
		case BOOT_RUNNING:
#ifdef UMA_MD_SMALL_ALLOC
			keg->uk_allocf = (keg->uk_ppera > 1) ?
			    page_alloc : uma_small_alloc;
#else
			keg->uk_allocf = page_alloc;
#endif
			return keg->uk_allocf(zone, bytes, domain, pflag, wait);
	}

	/*
	 * Check our small startup cache to see if it has pages remaining.
	 */
	pages = howmany(bytes, PAGE_SIZE);
	KASSERT(pages > 0, ("%s can't reserve 0 pages", __func__));
	if (pages > boot_pages)
		panic("UMA zone \"%s\": Increase vm.boot_pages", zone->uz_name);
#ifdef DIAGNOSTIC
	printf("%s from \"%s\", %d boot pages left\n", __func__, zone->uz_name,
	    boot_pages);
#endif
	mem = bootmem;
	boot_pages -= pages;
	bootmem += pages * PAGE_SIZE;
	*pflag = UMA_SLAB_BOOT;

	return (mem);
}

/*
 * Allocates a number of pages from the system
 *
 * Arguments:
 *	bytes  The number of bytes requested
 *	wait  Shall we wait?
 *
 * Returns:
 *	A pointer to the alloced memory or possibly
 *	NULL if M_NOWAIT is set.
 */
static void *
page_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *pflag,
    int wait)
{
	void *p;	/* Returned page */

	*pflag = UMA_SLAB_KERNEL;
	p = (void *)kmem_malloc_domainset(DOMAINSET_FIXED(domain), bytes, wait);

	return (p);
}

static void *
pcpu_page_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *pflag,
    int wait)
{
	struct pglist alloctail;
	vm_offset_t addr, zkva;
	int cpu, flags;
	vm_page_t p, p_next;
#ifdef NUMA
	struct pcpu *pc;
#endif

	MPASS(bytes == (mp_maxid + 1) * PAGE_SIZE);

	TAILQ_INIT(&alloctail);
	flags = VM_ALLOC_SYSTEM | VM_ALLOC_WIRED | VM_ALLOC_NOOBJ |
	    malloc2vm_flags(wait);
	*pflag = UMA_SLAB_KERNEL;
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu)) {
			p = vm_page_alloc(NULL, 0, flags);
		} else {
#ifndef NUMA
			p = vm_page_alloc(NULL, 0, flags);
#else
			pc = pcpu_find(cpu);
			p = vm_page_alloc_domain(NULL, 0, pc->pc_domain, flags);
			if (__predict_false(p == NULL))
				p = vm_page_alloc(NULL, 0, flags);
#endif
		}
		if (__predict_false(p == NULL))
			goto fail;
		TAILQ_INSERT_TAIL(&alloctail, p, listq);
	}
	if ((addr = kva_alloc(bytes)) == 0)
		goto fail;
	zkva = addr;
	TAILQ_FOREACH(p, &alloctail, listq) {
		pmap_qenter(zkva, &p, 1);
		zkva += PAGE_SIZE;
	}
	return ((void*)addr);
fail:
	TAILQ_FOREACH_SAFE(p, &alloctail, listq, p_next) {
		vm_page_unwire_noq(p);
		vm_page_free(p);
	}
	return (NULL);
}

/*
 * Allocates a number of pages from within an object
 *
 * Arguments:
 *	bytes  The number of bytes requested
 *	wait   Shall we wait?
 *
 * Returns:
 *	A pointer to the alloced memory or possibly
 *	NULL if M_NOWAIT is set.
 */
static void *
noobj_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *flags,
    int wait)
{
	TAILQ_HEAD(, vm_page) alloctail;
	u_long npages;
	vm_offset_t retkva, zkva;
	vm_page_t p, p_next;
	uma_keg_t keg;

	TAILQ_INIT(&alloctail);
	keg = zone->uz_keg;

	npages = howmany(bytes, PAGE_SIZE);
	while (npages > 0) {
		p = vm_page_alloc_domain(NULL, 0, domain, VM_ALLOC_INTERRUPT |
		    VM_ALLOC_WIRED | VM_ALLOC_NOOBJ |
		    ((wait & M_WAITOK) != 0 ? VM_ALLOC_WAITOK :
		    VM_ALLOC_NOWAIT));
		if (p != NULL) {
			/*
			 * Since the page does not belong to an object, its
			 * listq is unused.
			 */
			TAILQ_INSERT_TAIL(&alloctail, p, listq);
			npages--;
			continue;
		}
		/*
		 * Page allocation failed, free intermediate pages and
		 * exit.
		 */
		TAILQ_FOREACH_SAFE(p, &alloctail, listq, p_next) {
			vm_page_unwire_noq(p);
			vm_page_free(p); 
		}
		return (NULL);
	}
	*flags = UMA_SLAB_PRIV;
	zkva = keg->uk_kva +
	    atomic_fetchadd_long(&keg->uk_offset, round_page(bytes));
	retkva = zkva;
	TAILQ_FOREACH(p, &alloctail, listq) {
		pmap_qenter(zkva, &p, 1);
		zkva += PAGE_SIZE;
	}

	return ((void *)retkva);
}

/*
 * Frees a number of pages to the system
 *
 * Arguments:
 *	mem   A pointer to the memory to be freed
 *	size  The size of the memory being freed
 *	flags The original p->us_flags field
 *
 * Returns:
 *	Nothing
 */
static void
page_free(void *mem, vm_size_t size, uint8_t flags)
{

	if ((flags & UMA_SLAB_KERNEL) == 0)
		panic("UMA: page_free used with invalid flags %x", flags);

	kmem_free((vm_offset_t)mem, size);
}

/*
 * Frees pcpu zone allocations
 *
 * Arguments:
 *	mem   A pointer to the memory to be freed
 *	size  The size of the memory being freed
 *	flags The original p->us_flags field
 *
 * Returns:
 *	Nothing
 */
static void
pcpu_page_free(void *mem, vm_size_t size, uint8_t flags)
{
	vm_offset_t sva, curva;
	vm_paddr_t paddr;
	vm_page_t m;

	MPASS(size == (mp_maxid+1)*PAGE_SIZE);
	sva = (vm_offset_t)mem;
	for (curva = sva; curva < sva + size; curva += PAGE_SIZE) {
		paddr = pmap_kextract(curva);
		m = PHYS_TO_VM_PAGE(paddr);
		vm_page_unwire_noq(m);
		vm_page_free(m);
	}
	pmap_qremove(sva, size >> PAGE_SHIFT);
	kva_free(sva, size);
}


/*
 * Zero fill initializer
 *
 * Arguments/Returns follow uma_init specifications
 */
static int
zero_init(void *mem, int size, int flags)
{
	bzero(mem, size);
	return (0);
}

#ifdef INVARIANTS
struct noslabbits *
slab_dbg_bits(uma_slab_t slab, uma_keg_t keg)
{

	return ((void *)((char *)&slab->us_free + BITSET_SIZE(keg->uk_ipers)));
}
#endif

/*
 * Actual size of embedded struct slab (!OFFPAGE).
 */
size_t
slab_sizeof(int nitems)
{
	size_t s;

	s = sizeof(struct uma_slab) + BITSET_SIZE(nitems) * SLAB_BITSETS;
	return (roundup(s, UMA_ALIGN_PTR + 1));
}

/*
 * Size of memory for embedded slabs (!OFFPAGE).
 */
size_t
slab_space(int nitems)
{
	return (UMA_SLAB_SIZE - slab_sizeof(nitems));
}

/*
 * Compute the number of items that will fit in an embedded (!OFFPAGE) slab
 * with a given size and alignment.
 */
int
slab_ipers(size_t size, int align)
{
	int rsize;
	int nitems;

        /*
         * Compute the ideal number of items that will fit in a page and
         * then compute the actual number based on a bitset nitems wide.
         */
	rsize = roundup(size, align + 1);
        nitems = UMA_SLAB_SIZE / rsize;
	return (slab_space(nitems) / rsize);
}

/*
 * Finish creating a small uma keg.  This calculates ipers, and the keg size.
 *
 * Arguments
 *	keg  The zone we should initialize
 *
 * Returns
 *	Nothing
 */
static void
keg_small_init(uma_keg_t keg)
{
	u_int rsize;
	u_int memused;
	u_int wastedspace;
	u_int shsize;
	u_int slabsize;

	if (keg->uk_flags & UMA_ZONE_PCPU) {
		u_int ncpus = (mp_maxid + 1) ? (mp_maxid + 1) : MAXCPU;

		slabsize = UMA_PCPU_ALLOC_SIZE;
		keg->uk_ppera = ncpus;
	} else {
		slabsize = UMA_SLAB_SIZE;
		keg->uk_ppera = 1;
	}

	/*
	 * Calculate the size of each allocation (rsize) according to
	 * alignment.  If the requested size is smaller than we have
	 * allocation bits for we round it up.
	 */
	rsize = keg->uk_size;
	if (rsize < slabsize / SLAB_MAX_SETSIZE)
		rsize = slabsize / SLAB_MAX_SETSIZE;
	if (rsize & keg->uk_align)
		rsize = roundup(rsize, keg->uk_align + 1);
	keg->uk_rsize = rsize;

	KASSERT((keg->uk_flags & UMA_ZONE_PCPU) == 0 ||
	    keg->uk_rsize < UMA_PCPU_ALLOC_SIZE,
	    ("%s: size %u too large", __func__, keg->uk_rsize));

	/*
	 * Use a pessimistic bit count for shsize.  It may be possible to
	 * squeeze one more item in for very particular sizes if we were
	 * to loop and reduce the bitsize if there is waste.
	 */
	if (keg->uk_flags & UMA_ZONE_OFFPAGE)
		shsize = 0;
	else 
		shsize = slab_sizeof(slabsize / rsize);

	if (rsize <= slabsize - shsize)
		keg->uk_ipers = (slabsize - shsize) / rsize;
	else {
		/* Handle special case when we have 1 item per slab, so
		 * alignment requirement can be relaxed. */
		KASSERT(keg->uk_size <= slabsize - shsize,
		    ("%s: size %u greater than slab", __func__, keg->uk_size));
		keg->uk_ipers = 1;
	}
	KASSERT(keg->uk_ipers > 0 && keg->uk_ipers <= SLAB_MAX_SETSIZE,
	    ("%s: keg->uk_ipers %u", __func__, keg->uk_ipers));

	memused = keg->uk_ipers * rsize + shsize;
	wastedspace = slabsize - memused;

	/*
	 * We can't do OFFPAGE if we're internal or if we've been
	 * asked to not go to the VM for buckets.  If we do this we
	 * may end up going to the VM  for slabs which we do not
	 * want to do if we're UMA_ZFLAG_CACHEONLY as a result
	 * of UMA_ZONE_VM, which clearly forbids it.
	 */
	if ((keg->uk_flags & UMA_ZFLAG_INTERNAL) ||
	    (keg->uk_flags & UMA_ZFLAG_CACHEONLY))
		return;

	/*
	 * See if using an OFFPAGE slab will limit our waste.  Only do
	 * this if it permits more items per-slab.
	 *
	 * XXX We could try growing slabsize to limit max waste as well.
	 * Historically this was not done because the VM could not
	 * efficiently handle contiguous allocations.
	 */
	if ((wastedspace >= slabsize / UMA_MAX_WASTE) &&
	    (keg->uk_ipers < (slabsize / keg->uk_rsize))) {
		keg->uk_ipers = slabsize / keg->uk_rsize;
		KASSERT(keg->uk_ipers > 0 && keg->uk_ipers <= SLAB_MAX_SETSIZE,
		    ("%s: keg->uk_ipers %u", __func__, keg->uk_ipers));
		CTR6(KTR_UMA, "UMA decided we need offpage slab headers for "
		    "keg: %s(%p), calculated wastedspace = %d, "
		    "maximum wasted space allowed = %d, "
		    "calculated ipers = %d, "
		    "new wasted space = %d\n", keg->uk_name, keg, wastedspace,
		    slabsize / UMA_MAX_WASTE, keg->uk_ipers,
		    slabsize - keg->uk_ipers * keg->uk_rsize);
		/*
		 * If we had access to memory to embed a slab header we
		 * also have a page structure to use vtoslab() instead of
		 * hash to find slabs.  If the zone was explicitly created
		 * OFFPAGE we can't necessarily touch the memory.
		 */
		if ((keg->uk_flags & UMA_ZONE_OFFPAGE) == 0)
			keg->uk_flags |= UMA_ZONE_OFFPAGE | UMA_ZONE_VTOSLAB;
	}

	if ((keg->uk_flags & UMA_ZONE_OFFPAGE) &&
	    (keg->uk_flags & UMA_ZONE_VTOSLAB) == 0)
		keg->uk_flags |= UMA_ZONE_HASH;
}

/*
 * Finish creating a large (> UMA_SLAB_SIZE) uma kegs.  Just give in and do
 * OFFPAGE for now.  When I can allow for more dynamic slab sizes this will be
 * more complicated.
 *
 * Arguments
 *	keg  The keg we should initialize
 *
 * Returns
 *	Nothing
 */
static void
keg_large_init(uma_keg_t keg)
{

	KASSERT(keg != NULL, ("Keg is null in keg_large_init"));
	KASSERT((keg->uk_flags & UMA_ZONE_PCPU) == 0,
	    ("%s: Cannot large-init a UMA_ZONE_PCPU keg", __func__));

	keg->uk_ppera = howmany(keg->uk_size, PAGE_SIZE);
	keg->uk_ipers = 1;
	keg->uk_rsize = keg->uk_size;

	/* Check whether we have enough space to not do OFFPAGE. */
	if ((keg->uk_flags & UMA_ZONE_OFFPAGE) == 0 &&
	    PAGE_SIZE * keg->uk_ppera - keg->uk_rsize <
	    slab_sizeof(SLAB_MIN_SETSIZE)) {
		/*
		 * We can't do OFFPAGE if we're internal, in which case
		 * we need an extra page per allocation to contain the
		 * slab header.
		 */
		if ((keg->uk_flags & UMA_ZFLAG_INTERNAL) == 0)
			keg->uk_flags |= UMA_ZONE_OFFPAGE | UMA_ZONE_VTOSLAB;
		else
			keg->uk_ppera++;
	}

	if ((keg->uk_flags & UMA_ZONE_OFFPAGE) &&
	    (keg->uk_flags & UMA_ZONE_VTOSLAB) == 0)
		keg->uk_flags |= UMA_ZONE_HASH;
}

static void
keg_cachespread_init(uma_keg_t keg)
{
	int alignsize;
	int trailer;
	int pages;
	int rsize;

	KASSERT((keg->uk_flags & UMA_ZONE_PCPU) == 0,
	    ("%s: Cannot cachespread-init a UMA_ZONE_PCPU keg", __func__));

	alignsize = keg->uk_align + 1;
	rsize = keg->uk_size;
	/*
	 * We want one item to start on every align boundary in a page.  To
	 * do this we will span pages.  We will also extend the item by the
	 * size of align if it is an even multiple of align.  Otherwise, it
	 * would fall on the same boundary every time.
	 */
	if (rsize & keg->uk_align)
		rsize = (rsize & ~keg->uk_align) + alignsize;
	if ((rsize & alignsize) == 0)
		rsize += alignsize;
	trailer = rsize - keg->uk_size;
	pages = (rsize * (PAGE_SIZE / alignsize)) / PAGE_SIZE;
	pages = MIN(pages, (128 * 1024) / PAGE_SIZE);
	keg->uk_rsize = rsize;
	keg->uk_ppera = pages;
	keg->uk_ipers = ((pages * PAGE_SIZE) + trailer) / rsize;
	keg->uk_flags |= UMA_ZONE_OFFPAGE | UMA_ZONE_VTOSLAB;
	KASSERT(keg->uk_ipers <= SLAB_MAX_SETSIZE,
	    ("%s: keg->uk_ipers too high(%d) increase max_ipers", __func__,
	    keg->uk_ipers));
}

/*
 * Keg header ctor.  This initializes all fields, locks, etc.  And inserts
 * the keg onto the global keg list.
 *
 * Arguments/Returns follow uma_ctor specifications
 *	udata  Actually uma_kctor_args
 */
static int
keg_ctor(void *mem, int size, void *udata, int flags)
{
	struct uma_kctor_args *arg = udata;
	uma_keg_t keg = mem;
	uma_zone_t zone;

	bzero(keg, size);
	keg->uk_size = arg->size;
	keg->uk_init = arg->uminit;
	keg->uk_fini = arg->fini;
	keg->uk_align = arg->align;
	keg->uk_free = 0;
	keg->uk_reserve = 0;
	keg->uk_pages = 0;
	keg->uk_flags = arg->flags;
	keg->uk_slabzone = NULL;

	/*
	 * We use a global round-robin policy by default.  Zones with
	 * UMA_ZONE_NUMA set will use first-touch instead, in which case the
	 * iterator is never run.
	 */
	keg->uk_dr.dr_policy = DOMAINSET_RR();
	keg->uk_dr.dr_iter = 0;

	/*
	 * The master zone is passed to us at keg-creation time.
	 */
	zone = arg->zone;
	keg->uk_name = zone->uz_name;

	if (arg->flags & UMA_ZONE_VM)
		keg->uk_flags |= UMA_ZFLAG_CACHEONLY;

	if (arg->flags & UMA_ZONE_ZINIT)
		keg->uk_init = zero_init;

	if (arg->flags & UMA_ZONE_MALLOC)
		keg->uk_flags |= UMA_ZONE_VTOSLAB;

	if (arg->flags & UMA_ZONE_PCPU)
#ifdef SMP
		keg->uk_flags |= UMA_ZONE_OFFPAGE;
#else
		keg->uk_flags &= ~UMA_ZONE_PCPU;
#endif

	if (keg->uk_flags & UMA_ZONE_CACHESPREAD) {
		keg_cachespread_init(keg);
	} else {
		if (keg->uk_size > slab_space(SLAB_MIN_SETSIZE))
			keg_large_init(keg);
		else
			keg_small_init(keg);
	}

	if (keg->uk_flags & UMA_ZONE_OFFPAGE)
		keg->uk_slabzone = slabzone;

	/*
	 * If we haven't booted yet we need allocations to go through the
	 * startup cache until the vm is ready.
	 */
	if (booted < BOOT_PAGEALLOC)
		keg->uk_allocf = startup_alloc;
#ifdef UMA_MD_SMALL_ALLOC
	else if (keg->uk_ppera == 1)
		keg->uk_allocf = uma_small_alloc;
#endif
	else if (keg->uk_flags & UMA_ZONE_PCPU)
		keg->uk_allocf = pcpu_page_alloc;
	else
		keg->uk_allocf = page_alloc;
#ifdef UMA_MD_SMALL_ALLOC
	if (keg->uk_ppera == 1)
		keg->uk_freef = uma_small_free;
	else
#endif
	if (keg->uk_flags & UMA_ZONE_PCPU)
		keg->uk_freef = pcpu_page_free;
	else
		keg->uk_freef = page_free;

	/*
	 * Initialize keg's lock
	 */
	KEG_LOCK_INIT(keg, (arg->flags & UMA_ZONE_MTXCLASS));

	/*
	 * If we're putting the slab header in the actual page we need to
	 * figure out where in each page it goes.  See slab_sizeof
	 * definition.
	 */
	if (!(keg->uk_flags & UMA_ZONE_OFFPAGE)) {
		size_t shsize;

		shsize = slab_sizeof(keg->uk_ipers);
		keg->uk_pgoff = (PAGE_SIZE * keg->uk_ppera) - shsize;
		/*
		 * The only way the following is possible is if with our
		 * UMA_ALIGN_PTR adjustments we are now bigger than
		 * UMA_SLAB_SIZE.  I haven't checked whether this is
		 * mathematically possible for all cases, so we make
		 * sure here anyway.
		 */
		KASSERT(keg->uk_pgoff + shsize <= PAGE_SIZE * keg->uk_ppera,
		    ("zone %s ipers %d rsize %d size %d slab won't fit",
		    zone->uz_name, keg->uk_ipers, keg->uk_rsize, keg->uk_size));
	}

	if (keg->uk_flags & UMA_ZONE_HASH)
		hash_alloc(&keg->uk_hash, 0);

	CTR5(KTR_UMA, "keg_ctor %p zone %s(%p) out %d free %d\n",
	    keg, zone->uz_name, zone,
	    (keg->uk_pages / keg->uk_ppera) * keg->uk_ipers - keg->uk_free,
	    keg->uk_free);

	LIST_INSERT_HEAD(&keg->uk_zones, zone, uz_link);

	rw_wlock(&uma_rwlock);
	LIST_INSERT_HEAD(&uma_kegs, keg, uk_link);
	rw_wunlock(&uma_rwlock);
	return (0);
}

static void
zone_alloc_counters(uma_zone_t zone, void *unused)
{

	zone->uz_allocs = counter_u64_alloc(M_WAITOK);
	zone->uz_frees = counter_u64_alloc(M_WAITOK);
	zone->uz_fails = counter_u64_alloc(M_WAITOK);
}

static void
zone_alloc_sysctl(uma_zone_t zone, void *unused)
{
	uma_zone_domain_t zdom;
	uma_keg_t keg;
	struct sysctl_oid *oid, *domainoid;
	int domains, i, cnt;
	static const char *nokeg = "cache zone";
	char *c;

	/*
	 * Make a sysctl safe copy of the zone name by removing
	 * any special characters and handling dups by appending
	 * an index.
	 */
	if (zone->uz_namecnt != 0) {
		/* Count the number of decimal digits and '_' separator. */
		for (i = 1, cnt = zone->uz_namecnt; cnt != 0; i++)
			cnt /= 10;
		zone->uz_ctlname = malloc(strlen(zone->uz_name) + i + 1,
		    M_UMA, M_WAITOK);
		sprintf(zone->uz_ctlname, "%s_%d", zone->uz_name,
		    zone->uz_namecnt);
	} else
		zone->uz_ctlname = strdup(zone->uz_name, M_UMA);
	for (c = zone->uz_ctlname; *c != '\0'; c++)
		if (strchr("./\\ -", *c) != NULL)
			*c = '_';

	/*
	 * Basic parameters at the root.
	 */
	zone->uz_oid = SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(_vm_uma),
	    OID_AUTO, zone->uz_ctlname, CTLFLAG_RD, NULL, "");
	oid = zone->uz_oid;
	SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "size", CTLFLAG_RD, &zone->uz_size, 0, "Allocation size");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "flags", CTLFLAG_RD | CTLTYPE_STRING | CTLFLAG_MPSAFE,
	    zone, 0, sysctl_handle_uma_zone_flags, "A",
	    "Allocator configuration flags");
	SYSCTL_ADD_U16(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "bucket_size", CTLFLAG_RD, &zone->uz_bucket_size, 0,
	    "Desired per-cpu cache size");
	SYSCTL_ADD_U16(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "bucket_size_max", CTLFLAG_RD, &zone->uz_bucket_size_max, 0,
	    "Maximum allowed per-cpu cache size");

	/*
	 * keg if present.
	 */
	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid), OID_AUTO,
	    "keg", CTLFLAG_RD, NULL, "");
	keg = zone->uz_keg;
	if ((zone->uz_flags & UMA_ZFLAG_CACHE) == 0) {
		SYSCTL_ADD_CONST_STRING(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "name", CTLFLAG_RD, keg->uk_name, "Keg name");
		SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "rsize", CTLFLAG_RD, &keg->uk_rsize, 0,
		    "Real object size with alignment");
		SYSCTL_ADD_U16(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "ppera", CTLFLAG_RD, &keg->uk_ppera, 0,
		    "pages per-slab allocation");
		SYSCTL_ADD_U16(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "ipers", CTLFLAG_RD, &keg->uk_ipers, 0,
		    "items available per-slab");
		SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "align", CTLFLAG_RD, &keg->uk_align, 0,
		    "item alignment mask");
		SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "pages", CTLFLAG_RD, &keg->uk_pages, 0,
		    "Total pages currently allocated from VM");
		SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "free", CTLFLAG_RD, &keg->uk_free, 0,
		    "items free in the slab layer");
		SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "efficiency", CTLFLAG_RD | CTLTYPE_INT | CTLFLAG_MPSAFE,
		    keg, 0, sysctl_handle_uma_slab_efficiency, "I",
		    "Slab utilization (100 - internal fragmentation %)");
	} else
		SYSCTL_ADD_CONST_STRING(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "name", CTLFLAG_RD, nokeg, "Keg name");

	/*
	 * Information about zone limits.
	 */
	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid), OID_AUTO,
	    "limit", CTLFLAG_RD, NULL, "");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "items", CTLFLAG_RD, &zone->uz_items, 0,
	    "current number of cached items");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "max_items", CTLFLAG_RD, &zone->uz_max_items, 0,
	    "Maximum number of cached items");
	SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "sleepers", CTLFLAG_RD, &zone->uz_sleepers, 0,
	    "Number of threads sleeping at limit");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "sleeps", CTLFLAG_RD, &zone->uz_sleeps, 0,
	    "Total zone limit sleeps");

	/*
	 * Per-domain information.
	 */
	if ((zone->uz_flags & UMA_ZONE_NUMA) != 0)
		domains = vm_ndomains;
	else
		domains = 1;
	domainoid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid),
	    OID_AUTO, "domain", CTLFLAG_RD, NULL, "");
	for (i = 0; i < domains; i++) {
		zdom = &zone->uz_domain[i];
		oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(domainoid),
		    OID_AUTO, VM_DOMAIN(i)->vmd_name, CTLFLAG_RD, NULL, "");
		SYSCTL_ADD_LONG(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "nitems", CTLFLAG_RD, &zdom->uzd_nitems,
		    "number of items in this domain");
		SYSCTL_ADD_LONG(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "imax", CTLFLAG_RD, &zdom->uzd_imax,
		    "maximum item count in this period");
		SYSCTL_ADD_LONG(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "imin", CTLFLAG_RD, &zdom->uzd_imin,
		    "minimum item count in this period");
		SYSCTL_ADD_LONG(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "wss", CTLFLAG_RD, &zdom->uzd_wss,
		    "Working set size");
	}

	/*
	 * General statistics.
	 */
	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid), OID_AUTO,
	    "stats", CTLFLAG_RD, NULL, "");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "current", CTLFLAG_RD | CTLTYPE_INT | CTLFLAG_MPSAFE,
	    zone, 1, sysctl_handle_uma_zone_cur, "I",
	    "Current number of allocated items");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "allocs", CTLFLAG_RD | CTLTYPE_U64 | CTLFLAG_MPSAFE,
	    zone, 0, sysctl_handle_uma_zone_allocs, "QU",
	    "Total allocation calls");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "frees", CTLFLAG_RD | CTLTYPE_U64 | CTLFLAG_MPSAFE,
	    zone, 0, sysctl_handle_uma_zone_frees, "QU",
	    "Total free calls");
	SYSCTL_ADD_COUNTER_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "fails", CTLFLAG_RD, &zone->uz_fails,
	    "Number of allocation failures");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "xdomain", CTLFLAG_RD, &zone->uz_xdomain, 0,
	    "Free calls from the wrong domain");
}

struct uma_zone_count {
	const char	*name;
	int		count;
};

static void
zone_count(uma_zone_t zone, void *arg)
{
	struct uma_zone_count *cnt;

	cnt = arg;
	/*
	 * Some zones are rapidly created with identical names and
	 * destroyed out of order.  This can lead to gaps in the count.
	 * Use one greater than the maximum observed for this name.
	 */
	if (strcmp(zone->uz_name, cnt->name) == 0)
		cnt->count = MAX(cnt->count,
		    zone->uz_namecnt + 1);
}

/*
 * Zone header ctor.  This initializes all fields, locks, etc.
 *
 * Arguments/Returns follow uma_ctor specifications
 *	udata  Actually uma_zctor_args
 */
static int
zone_ctor(void *mem, int size, void *udata, int flags)
{
	struct uma_zone_count cnt;
	struct uma_zctor_args *arg = udata;
	uma_zone_t zone = mem;
	uma_zone_t z;
	uma_keg_t keg;
	int i;

	bzero(zone, size);
	zone->uz_name = arg->name;
	zone->uz_ctor = arg->ctor;
	zone->uz_dtor = arg->dtor;
	zone->uz_init = NULL;
	zone->uz_fini = NULL;
	zone->uz_sleeps = 0;
	zone->uz_xdomain = 0;
	zone->uz_bucket_size = 0;
	zone->uz_bucket_size_min = 0;
	zone->uz_bucket_size_max = BUCKET_MAX;
	zone->uz_flags = 0;
	zone->uz_warning = NULL;
	/* The domain structures follow the cpu structures. */
	zone->uz_domain = (struct uma_zone_domain *)&zone->uz_cpu[mp_ncpus];
	zone->uz_bkt_max = ULONG_MAX;
	timevalclear(&zone->uz_ratecheck);

	/* Count the number of duplicate names. */
	cnt.name = arg->name;
	cnt.count = 0;
	zone_foreach(zone_count, &cnt);
	zone->uz_namecnt = cnt.count;

	for (i = 0; i < vm_ndomains; i++)
		TAILQ_INIT(&zone->uz_domain[i].uzd_buckets);

#ifdef INVARIANTS
	if (arg->uminit == trash_init && arg->fini == trash_fini)
		zone->uz_flags |= UMA_ZFLAG_TRASH;
#endif

	/*
	 * This is a pure cache zone, no kegs.
	 */
	if (arg->import) {
		if (arg->flags & UMA_ZONE_VM)
			arg->flags |= UMA_ZFLAG_CACHEONLY;
		zone->uz_flags = arg->flags;
		zone->uz_size = arg->size;
		zone->uz_import = arg->import;
		zone->uz_release = arg->release;
		zone->uz_arg = arg->arg;
		zone->uz_lockptr = &zone->uz_lock;
		ZONE_LOCK_INIT(zone, (arg->flags & UMA_ZONE_MTXCLASS));
		rw_wlock(&uma_rwlock);
		LIST_INSERT_HEAD(&uma_cachezones, zone, uz_link);
		rw_wunlock(&uma_rwlock);
		goto out;
	}

	/*
	 * Use the regular zone/keg/slab allocator.
	 */
	zone->uz_import = zone_import;
	zone->uz_release = zone_release;
	zone->uz_arg = zone; 
	keg = arg->keg;

	if (arg->flags & UMA_ZONE_SECONDARY) {
		KASSERT((zone->uz_flags & UMA_ZONE_SECONDARY) == 0,
		    ("Secondary zone requested UMA_ZFLAG_INTERNAL"));
		KASSERT(arg->keg != NULL, ("Secondary zone on zero'd keg"));
		zone->uz_init = arg->uminit;
		zone->uz_fini = arg->fini;
		zone->uz_lockptr = &keg->uk_lock;
		zone->uz_flags |= UMA_ZONE_SECONDARY;
		rw_wlock(&uma_rwlock);
		ZONE_LOCK(zone);
		LIST_FOREACH(z, &keg->uk_zones, uz_link) {
			if (LIST_NEXT(z, uz_link) == NULL) {
				LIST_INSERT_AFTER(z, zone, uz_link);
				break;
			}
		}
		ZONE_UNLOCK(zone);
		rw_wunlock(&uma_rwlock);
	} else if (keg == NULL) {
		if ((keg = uma_kcreate(zone, arg->size, arg->uminit, arg->fini,
		    arg->align, arg->flags)) == NULL)
			return (ENOMEM);
	} else {
		struct uma_kctor_args karg;
		int error;

		/* We should only be here from uma_startup() */
		karg.size = arg->size;
		karg.uminit = arg->uminit;
		karg.fini = arg->fini;
		karg.align = arg->align;
		karg.flags = arg->flags;
		karg.zone = zone;
		error = keg_ctor(arg->keg, sizeof(struct uma_keg), &karg,
		    flags);
		if (error)
			return (error);
	}

	/* Inherit properties from the keg. */
	zone->uz_keg = keg;
	zone->uz_size = keg->uk_size;
	zone->uz_flags |= (keg->uk_flags &
	    (UMA_ZONE_INHERIT | UMA_ZFLAG_INHERIT));

out:
	if (__predict_true(booted == BOOT_RUNNING)) {
		zone_alloc_counters(zone, NULL);
		zone_alloc_sysctl(zone, NULL);
	} else {
		zone->uz_allocs = EARLY_COUNTER;
		zone->uz_frees = EARLY_COUNTER;
		zone->uz_fails = EARLY_COUNTER;
	}

	KASSERT((arg->flags & (UMA_ZONE_MAXBUCKET | UMA_ZONE_NOBUCKET)) !=
	    (UMA_ZONE_MAXBUCKET | UMA_ZONE_NOBUCKET),
	    ("Invalid zone flag combination"));
	if (arg->flags & UMA_ZFLAG_INTERNAL)
		zone->uz_bucket_size_max = zone->uz_bucket_size = 0;
	if ((arg->flags & UMA_ZONE_MAXBUCKET) != 0)
		zone->uz_bucket_size = BUCKET_MAX;
	else if ((arg->flags & UMA_ZONE_MINBUCKET) != 0)
		zone->uz_bucket_size_max = zone->uz_bucket_size = BUCKET_MIN;
	else if ((arg->flags & UMA_ZONE_NOBUCKET) != 0)
		zone->uz_bucket_size = 0;
	else
		zone->uz_bucket_size = bucket_select(zone->uz_size);
	zone->uz_bucket_size_min = zone->uz_bucket_size;

	return (0);
}

/*
 * Keg header dtor.  This frees all data, destroys locks, frees the hash
 * table and removes the keg from the global list.
 *
 * Arguments/Returns follow uma_dtor specifications
 *	udata  unused
 */
static void
keg_dtor(void *arg, int size, void *udata)
{
	uma_keg_t keg;

	keg = (uma_keg_t)arg;
	KEG_LOCK(keg);
	if (keg->uk_free != 0) {
		printf("Freed UMA keg (%s) was not empty (%d items). "
		    " Lost %d pages of memory.\n",
		    keg->uk_name ? keg->uk_name : "",
		    keg->uk_free, keg->uk_pages);
	}
	KEG_UNLOCK(keg);

	hash_free(&keg->uk_hash);

	KEG_LOCK_FINI(keg);
}

/*
 * Zone header dtor.
 *
 * Arguments/Returns follow uma_dtor specifications
 *	udata  unused
 */
static void
zone_dtor(void *arg, int size, void *udata)
{
	uma_zone_t zone;
	uma_keg_t keg;

	zone = (uma_zone_t)arg;

	sysctl_remove_oid(zone->uz_oid, 1, 1);

	if (!(zone->uz_flags & UMA_ZFLAG_INTERNAL))
		cache_drain(zone);

	rw_wlock(&uma_rwlock);
	LIST_REMOVE(zone, uz_link);
	rw_wunlock(&uma_rwlock);
	/*
	 * XXX there are some races here where
	 * the zone can be drained but zone lock
	 * released and then refilled before we
	 * remove it... we dont care for now
	 */
	zone_reclaim(zone, M_WAITOK, true);
	/*
	 * We only destroy kegs from non secondary/non cache zones.
	 */
	if ((zone->uz_flags & (UMA_ZONE_SECONDARY | UMA_ZFLAG_CACHE)) == 0) {
		keg = zone->uz_keg;
		rw_wlock(&uma_rwlock);
		LIST_REMOVE(keg, uk_link);
		rw_wunlock(&uma_rwlock);
		zone_free_item(kegs, keg, NULL, SKIP_NONE);
	}
	counter_u64_free(zone->uz_allocs);
	counter_u64_free(zone->uz_frees);
	counter_u64_free(zone->uz_fails);
	free(zone->uz_ctlname, M_UMA);
	if (zone->uz_lockptr == &zone->uz_lock)
		ZONE_LOCK_FINI(zone);
}

/*
 * Traverses every zone in the system and calls a callback
 *
 * Arguments:
 *	zfunc  A pointer to a function which accepts a zone
 *		as an argument.
 *
 * Returns:
 *	Nothing
 */
static void
zone_foreach(void (*zfunc)(uma_zone_t, void *arg), void *arg)
{
	uma_keg_t keg;
	uma_zone_t zone;

	/*
	 * Before BOOT_RUNNING we are guaranteed to be single
	 * threaded, so locking isn't needed. Startup functions
	 * are allowed to use M_WAITOK.
	 */
	if (__predict_true(booted == BOOT_RUNNING))
		rw_rlock(&uma_rwlock);
	LIST_FOREACH(keg, &uma_kegs, uk_link) {
		LIST_FOREACH(zone, &keg->uk_zones, uz_link)
			zfunc(zone, arg);
	}
	LIST_FOREACH(zone, &uma_cachezones, uz_link)
		zfunc(zone, arg);
	if (__predict_true(booted == BOOT_RUNNING))
		rw_runlock(&uma_rwlock);
}

/*
 * Count how many pages do we need to bootstrap.  VM supplies
 * its need in early zones in the argument, we add up our zones,
 * which consist of the UMA Slabs, UMA Hash and 9 Bucket zones.  The
 * zone of zones and zone of kegs are accounted separately.
 */
#define	UMA_BOOT_ZONES	11
/* Zone of zones and zone of kegs have arbitrary alignment. */
#define	UMA_BOOT_ALIGN	32
static int zsize, ksize;
int
uma_startup_count(int vm_zones)
{
	int zones, pages;
	size_t space, size;

	ksize = sizeof(struct uma_keg) +
	    (sizeof(struct uma_domain) * vm_ndomains);
	zsize = sizeof(struct uma_zone) +
	    (sizeof(struct uma_cache) * (mp_maxid + 1)) +
	    (sizeof(struct uma_zone_domain) * vm_ndomains);

	/*
	 * Memory for the zone of kegs and its keg,
	 * and for zone of zones.
	 */
	pages = howmany(roundup(zsize, CACHE_LINE_SIZE) * 2 +
	    roundup(ksize, CACHE_LINE_SIZE), PAGE_SIZE);

#ifdef	UMA_MD_SMALL_ALLOC
	zones = UMA_BOOT_ZONES;
#else
	zones = UMA_BOOT_ZONES + vm_zones;
	vm_zones = 0;
#endif
	size = slab_sizeof(SLAB_MAX_SETSIZE);
	space = slab_space(SLAB_MAX_SETSIZE);

	/* Memory for the rest of startup zones, UMA and VM, ... */
	if (zsize > space) {
		/* See keg_large_init(). */
		u_int ppera;

		ppera = howmany(roundup2(zsize, UMA_BOOT_ALIGN), PAGE_SIZE);
		if (PAGE_SIZE * ppera - roundup2(zsize, UMA_BOOT_ALIGN) < size)
			ppera++;
		pages += (zones + vm_zones) * ppera;
	} else if (roundup2(zsize, UMA_BOOT_ALIGN) > space)
		/* See keg_small_init() special case for uk_ppera = 1. */
		pages += zones;
	else
		pages += howmany(zones,
		    space / roundup2(zsize, UMA_BOOT_ALIGN));

	/* ... and their kegs. Note that zone of zones allocates a keg! */
	pages += howmany(zones + 1,
	    space / roundup2(ksize, UMA_BOOT_ALIGN));

	return (pages);
}

void
uma_startup(void *mem, int npages)
{
	struct uma_zctor_args args;
	uma_keg_t masterkeg;
	uintptr_t m;

#ifdef DIAGNOSTIC
	printf("Entering %s with %d boot pages configured\n", __func__, npages);
#endif

	rw_init(&uma_rwlock, "UMA lock");

	/* Use bootpages memory for the zone of zones and zone of kegs. */
	m = (uintptr_t)mem;
	zones = (uma_zone_t)m;
	m += roundup(zsize, CACHE_LINE_SIZE);
	kegs = (uma_zone_t)m;
	m += roundup(zsize, CACHE_LINE_SIZE);
	masterkeg = (uma_keg_t)m;
	m += roundup(ksize, CACHE_LINE_SIZE);
	m = roundup(m, PAGE_SIZE);
	npages -= (m - (uintptr_t)mem) / PAGE_SIZE;
	mem = (void *)m;

	/* "manually" create the initial zone */
	memset(&args, 0, sizeof(args));
	args.name = "UMA Kegs";
	args.size = ksize;
	args.ctor = keg_ctor;
	args.dtor = keg_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.keg = masterkeg;
	args.align = UMA_BOOT_ALIGN - 1;
	args.flags = UMA_ZFLAG_INTERNAL;
	zone_ctor(kegs, zsize, &args, M_WAITOK);

	bootmem = mem;
	boot_pages = npages;

	args.name = "UMA Zones";
	args.size = zsize;
	args.ctor = zone_ctor;
	args.dtor = zone_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.keg = NULL;
	args.align = UMA_BOOT_ALIGN - 1;
	args.flags = UMA_ZFLAG_INTERNAL;
	zone_ctor(zones, zsize, &args, M_WAITOK);

	/* Now make a zone for slab headers */
	slabzone = uma_zcreate("UMA Slabs", sizeof(struct uma_hash_slab),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);

	hashzone = uma_zcreate("UMA Hash",
	    sizeof(struct slabhead *) * UMA_HASH_SIZE_INIT,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);

	booted = BOOT_STRAPPED;
}

void
uma_startup1(void)
{

#ifdef DIAGNOSTIC
	printf("Entering %s with %d boot pages left\n", __func__, boot_pages);
#endif
	booted = BOOT_PAGEALLOC;
}

void
uma_startup2(void)
{

#ifdef DIAGNOSTIC
	printf("Entering %s with %d boot pages left\n", __func__, boot_pages);
#endif
	sx_init(&uma_reclaim_lock, "umareclaim");
	bucket_init();
	booted = BOOT_BUCKETS;
	bucket_enable();
}

/*
 * Initialize our callout handle
 *
 */
static void
uma_startup3(void)
{

#ifdef INVARIANTS
	TUNABLE_INT_FETCH("vm.debug.divisor", &dbg_divisor);
	uma_dbg_cnt = counter_u64_alloc(M_WAITOK);
	uma_skip_cnt = counter_u64_alloc(M_WAITOK);
#endif
	zone_foreach(zone_alloc_counters, NULL);
	zone_foreach(zone_alloc_sysctl, NULL);
	callout_init(&uma_callout, 1);
	callout_reset(&uma_callout, UMA_TIMEOUT * hz, uma_timeout, NULL);
	booted = BOOT_RUNNING;
}

static uma_keg_t
uma_kcreate(uma_zone_t zone, size_t size, uma_init uminit, uma_fini fini,
		int align, uint32_t flags)
{
	struct uma_kctor_args args;

	args.size = size;
	args.uminit = uminit;
	args.fini = fini;
	args.align = (align == UMA_ALIGN_CACHE) ? uma_align_cache : align;
	args.flags = flags;
	args.zone = zone;
	return (zone_alloc_item(kegs, &args, UMA_ANYDOMAIN, M_WAITOK));
}

/* Public functions */
/* See uma.h */
void
uma_set_align(int align)
{

	if (align != UMA_ALIGN_CACHE)
		uma_align_cache = align;
}

/* See uma.h */
uma_zone_t
uma_zcreate(const char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
		uma_init uminit, uma_fini fini, int align, uint32_t flags)

{
	struct uma_zctor_args args;
	uma_zone_t res;
	bool locked;

	KASSERT(powerof2(align + 1), ("invalid zone alignment %d for \"%s\"",
	    align, name));

	/* Sets all zones to a first-touch domain policy. */
#ifdef UMA_FIRSTTOUCH
	flags |= UMA_ZONE_NUMA;
#endif

	/* This stuff is essential for the zone ctor */
	memset(&args, 0, sizeof(args));
	args.name = name;
	args.size = size;
	args.ctor = ctor;
	args.dtor = dtor;
	args.uminit = uminit;
	args.fini = fini;
#ifdef  INVARIANTS
	/*
	 * Inject procedures which check for memory use after free if we are
	 * allowed to scramble the memory while it is not allocated.  This
	 * requires that: UMA is actually able to access the memory, no init
	 * or fini procedures, no dependency on the initial value of the
	 * memory, and no (legitimate) use of the memory after free.  Note,
	 * the ctor and dtor do not need to be empty.
	 *
	 * XXX UMA_ZONE_OFFPAGE.
	 */
	if ((!(flags & (UMA_ZONE_ZINIT | UMA_ZONE_NOFREE))) &&
	    uminit == NULL && fini == NULL) {
		args.uminit = trash_init;
		args.fini = trash_fini;
	}
#endif
	args.align = align;
	args.flags = flags;
	args.keg = NULL;

	if (booted < BOOT_BUCKETS) {
		locked = false;
	} else {
		sx_slock(&uma_reclaim_lock);
		locked = true;
	}
	res = zone_alloc_item(zones, &args, UMA_ANYDOMAIN, M_WAITOK);
	if (locked)
		sx_sunlock(&uma_reclaim_lock);
	return (res);
}

/* See uma.h */
uma_zone_t
uma_zsecond_create(char *name, uma_ctor ctor, uma_dtor dtor,
		    uma_init zinit, uma_fini zfini, uma_zone_t master)
{
	struct uma_zctor_args args;
	uma_keg_t keg;
	uma_zone_t res;
	bool locked;

	keg = master->uz_keg;
	memset(&args, 0, sizeof(args));
	args.name = name;
	args.size = keg->uk_size;
	args.ctor = ctor;
	args.dtor = dtor;
	args.uminit = zinit;
	args.fini = zfini;
	args.align = keg->uk_align;
	args.flags = keg->uk_flags | UMA_ZONE_SECONDARY;
	args.keg = keg;

	if (booted < BOOT_BUCKETS) {
		locked = false;
	} else {
		sx_slock(&uma_reclaim_lock);
		locked = true;
	}
	/* XXX Attaches only one keg of potentially many. */
	res = zone_alloc_item(zones, &args, UMA_ANYDOMAIN, M_WAITOK);
	if (locked)
		sx_sunlock(&uma_reclaim_lock);
	return (res);
}

/* See uma.h */
uma_zone_t
uma_zcache_create(char *name, int size, uma_ctor ctor, uma_dtor dtor,
		    uma_init zinit, uma_fini zfini, uma_import zimport,
		    uma_release zrelease, void *arg, int flags)
{
	struct uma_zctor_args args;

	memset(&args, 0, sizeof(args));
	args.name = name;
	args.size = size;
	args.ctor = ctor;
	args.dtor = dtor;
	args.uminit = zinit;
	args.fini = zfini;
	args.import = zimport;
	args.release = zrelease;
	args.arg = arg;
	args.align = 0;
	args.flags = flags | UMA_ZFLAG_CACHE;

	return (zone_alloc_item(zones, &args, UMA_ANYDOMAIN, M_WAITOK));
}

/* See uma.h */
void
uma_zdestroy(uma_zone_t zone)
{

	sx_slock(&uma_reclaim_lock);
	zone_free_item(zones, zone, NULL, SKIP_NONE);
	sx_sunlock(&uma_reclaim_lock);
}

void
uma_zwait(uma_zone_t zone)
{
	void *item;

	item = uma_zalloc_arg(zone, NULL, M_WAITOK);
	uma_zfree(zone, item);
}

void *
uma_zalloc_pcpu_arg(uma_zone_t zone, void *udata, int flags)
{
	void *item;
#ifdef SMP
	int i;

	MPASS(zone->uz_flags & UMA_ZONE_PCPU);
#endif
	item = uma_zalloc_arg(zone, udata, flags & ~M_ZERO);
	if (item != NULL && (flags & M_ZERO)) {
#ifdef SMP
		for (i = 0; i <= mp_maxid; i++)
			bzero(zpcpu_get_cpu(item, i), zone->uz_size);
#else
		bzero(item, zone->uz_size);
#endif
	}
	return (item);
}

/*
 * A stub while both regular and pcpu cases are identical.
 */
void
uma_zfree_pcpu_arg(uma_zone_t zone, void *item, void *udata)
{

#ifdef SMP
	MPASS(zone->uz_flags & UMA_ZONE_PCPU);
#endif
	uma_zfree_arg(zone, item, udata);
}

static inline void *
bucket_pop(uma_zone_t zone, uma_cache_t cache, uma_bucket_t bucket)
{
	void *item;

	bucket->ub_cnt--;
	item = bucket->ub_bucket[bucket->ub_cnt];
#ifdef INVARIANTS
	bucket->ub_bucket[bucket->ub_cnt] = NULL;
	KASSERT(item != NULL, ("uma_zalloc: Bucket pointer mangled."));
#endif
	cache->uc_allocs++;

	return (item);
}

static inline void
bucket_push(uma_zone_t zone, uma_cache_t cache, uma_bucket_t bucket,
    void *item)
{
	KASSERT(bucket->ub_bucket[bucket->ub_cnt] == NULL,
	    ("uma_zfree: Freeing to non free bucket index."));
	bucket->ub_bucket[bucket->ub_cnt] = item;
	bucket->ub_cnt++;
	cache->uc_frees++;
}

static void *
item_ctor(uma_zone_t zone, void *udata, int flags, void *item)
{
#ifdef INVARIANTS
	bool skipdbg;

	skipdbg = uma_dbg_zskip(zone, item);
	if (!skipdbg && (zone->uz_flags & UMA_ZFLAG_TRASH) != 0 &&
	    zone->uz_ctor != trash_ctor)
		trash_ctor(item, zone->uz_size, udata, flags);
#endif
	if (__predict_false(zone->uz_ctor != NULL) &&
	    zone->uz_ctor(item, zone->uz_size, udata, flags) != 0) {
		counter_u64_add(zone->uz_fails, 1);
		zone_free_item(zone, item, udata, SKIP_DTOR | SKIP_CNT);
		return (NULL);
	}
#ifdef INVARIANTS
	if (!skipdbg)
		uma_dbg_alloc(zone, NULL, item);
#endif
	if (flags & M_ZERO)
		uma_zero_item(item, zone);

	return (item);
}

static inline void
item_dtor(uma_zone_t zone, void *item, void *udata, enum zfreeskip skip)
{
#ifdef INVARIANTS
	bool skipdbg;

	skipdbg = uma_dbg_zskip(zone, item);
	if (skip == SKIP_NONE && !skipdbg) {
		if ((zone->uz_flags & UMA_ZONE_MALLOC) != 0)
			uma_dbg_free(zone, udata, item);
		else
			uma_dbg_free(zone, NULL, item);
	}
#endif
	if (skip < SKIP_DTOR) {
		if (zone->uz_dtor != NULL)
			zone->uz_dtor(item, zone->uz_size, udata);
#ifdef INVARIANTS
		if (!skipdbg && (zone->uz_flags & UMA_ZFLAG_TRASH) != 0 &&
		    zone->uz_dtor != trash_dtor)
			trash_dtor(item, zone->uz_size, udata);
#endif
	}
}

/* See uma.h */
void *
uma_zalloc_arg(uma_zone_t zone, void *udata, int flags)
{
	uma_bucket_t bucket;
	uma_cache_t cache;
	void *item;
	int cpu, domain;

	/* Enable entropy collection for RANDOM_ENABLE_UMA kernel option */
	random_harvest_fast_uma(&zone, sizeof(zone), RANDOM_UMA);

	/* This is the fast path allocation */
	CTR4(KTR_UMA, "uma_zalloc_arg thread %x zone %s(%p) flags %d",
	    curthread, zone->uz_name, zone, flags);

	if (flags & M_WAITOK) {
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "uma_zalloc_arg: zone \"%s\"", zone->uz_name);
	}
	KASSERT((flags & M_EXEC) == 0, ("uma_zalloc_arg: called with M_EXEC"));
	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("uma_zalloc_arg: called with spinlock or critical section held"));
	if (zone->uz_flags & UMA_ZONE_PCPU)
		KASSERT((flags & M_ZERO) == 0, ("allocating from a pcpu zone "
		    "with M_ZERO passed"));

#ifdef DEBUG_MEMGUARD
	if (memguard_cmp_zone(zone)) {
		item = memguard_alloc(zone->uz_size, flags);
		if (item != NULL) {
			if (zone->uz_init != NULL &&
			    zone->uz_init(item, zone->uz_size, flags) != 0)
				return (NULL);
			if (zone->uz_ctor != NULL &&
			    zone->uz_ctor(item, zone->uz_size, udata,
			    flags) != 0) {
				counter_u64_add(zone->uz_fails, 1);
			    	zone->uz_fini(item, zone->uz_size);
				return (NULL);
			}
			return (item);
		}
		/* This is unfortunate but should not be fatal. */
	}
#endif
	/*
	 * If possible, allocate from the per-CPU cache.  There are two
	 * requirements for safe access to the per-CPU cache: (1) the thread
	 * accessing the cache must not be preempted or yield during access,
	 * and (2) the thread must not migrate CPUs without switching which
	 * cache it accesses.  We rely on a critical section to prevent
	 * preemption and migration.  We release the critical section in
	 * order to acquire the zone mutex if we are unable to allocate from
	 * the current cache; when we re-acquire the critical section, we
	 * must detect and handle migration if it has occurred.
	 */
	critical_enter();
	do {
		cpu = curcpu;
		cache = &zone->uz_cpu[cpu];
		bucket = cache->uc_allocbucket;
		if (__predict_true(bucket != NULL && bucket->ub_cnt != 0)) {
			item = bucket_pop(zone, cache, bucket);
			critical_exit();
			return (item_ctor(zone, udata, flags, item));
		}
	} while (cache_alloc(zone, cache, udata, flags));
	critical_exit();

	/*
	 * We can not get a bucket so try to return a single item.
	 */
	if (zone->uz_flags & UMA_ZONE_NUMA)
		domain = PCPU_GET(domain);
	else
		domain = UMA_ANYDOMAIN;
	return (zone_alloc_item_locked(zone, udata, domain, flags));
}

/*
 * Replenish an alloc bucket and possibly restore an old one.  Called in
 * a critical section.  Returns in a critical section.
 *
 * A false return value indicates failure and returns with the zone lock
 * held.  A true return value indicates success and the caller should retry.
 */
static __noinline bool
cache_alloc(uma_zone_t zone, uma_cache_t cache, void *udata, int flags)
{
	uma_zone_domain_t zdom;
	uma_bucket_t bucket;
	int cpu, domain;
	bool lockfail;

	CRITICAL_ASSERT(curthread);

	/*
	 * If we have run out of items in our alloc bucket see
	 * if we can switch with the free bucket.
	 */
	bucket = cache->uc_freebucket;
	if (bucket != NULL && bucket->ub_cnt != 0) {
		cache->uc_freebucket = cache->uc_allocbucket;
		cache->uc_allocbucket = bucket;
		return (true);
	}

	/*
	 * Discard any empty allocation bucket while we hold no locks.
	 */
	bucket = cache->uc_allocbucket;
	cache->uc_allocbucket = NULL;
	critical_exit();
	if (bucket != NULL)
		bucket_free(zone, bucket, udata);

	/*
	 * Attempt to retrieve the item from the per-CPU cache has failed, so
	 * we must go back to the zone.  This requires the zone lock, so we
	 * must drop the critical section, then re-acquire it when we go back
	 * to the cache.  Since the critical section is released, we may be
	 * preempted or migrate.  As such, make sure not to maintain any
	 * thread-local state specific to the cache from prior to releasing
	 * the critical section.
	 */
	lockfail = 0;
	if (ZONE_TRYLOCK(zone) == 0) {
		/* Record contention to size the buckets. */
		ZONE_LOCK(zone);
		lockfail = 1;
	}

	critical_enter();
	/* Short-circuit for zones without buckets and low memory. */
	if (zone->uz_bucket_size == 0 || bucketdisable)
		return (false);

	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];

	/* See if we lost the race to fill the cache. */
	if (cache->uc_allocbucket != NULL) {
		ZONE_UNLOCK(zone);
		return (true);
	}

	/*
	 * Check the zone's cache of buckets.
	 */
	if (zone->uz_flags & UMA_ZONE_NUMA) {
		domain = PCPU_GET(domain);
		zdom = &zone->uz_domain[domain];
	} else {
		domain = UMA_ANYDOMAIN;
		zdom = &zone->uz_domain[0];
	}

	if ((bucket = zone_fetch_bucket(zone, zdom)) != NULL) {
		ZONE_UNLOCK(zone);
		KASSERT(bucket->ub_cnt != 0,
		    ("uma_zalloc_arg: Returning an empty bucket."));
		cache->uc_allocbucket = bucket;
		return (true);
	}
	/* We are no longer associated with this CPU. */
	critical_exit();

	/*
	 * We bump the uz count when the cache size is insufficient to
	 * handle the working set.
	 */
	if (lockfail && zone->uz_bucket_size < zone->uz_bucket_size_max)
		zone->uz_bucket_size++;

	/*
	 * Fill a bucket and attempt to use it as the alloc bucket.
	 */
	bucket = zone_alloc_bucket(zone, udata, domain, flags);
	CTR3(KTR_UMA, "uma_zalloc: zone %s(%p) bucket zone returned %p",
	    zone->uz_name, zone, bucket);
	critical_enter();
	if (bucket == NULL)
		return (false);

	/*
	 * See if we lost the race or were migrated.  Cache the
	 * initialized bucket to make this less likely or claim
	 * the memory directly.
	 */
	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];
	if (cache->uc_allocbucket == NULL &&
	    ((zone->uz_flags & UMA_ZONE_NUMA) == 0 ||
	    domain == PCPU_GET(domain))) {
		cache->uc_allocbucket = bucket;
		zdom->uzd_imax += bucket->ub_cnt;
	} else if (zone->uz_bkt_count >= zone->uz_bkt_max) {
		critical_exit();
		ZONE_UNLOCK(zone);
		bucket_drain(zone, bucket);
		bucket_free(zone, bucket, udata);
		critical_enter();
		return (true);
	} else
		zone_put_bucket(zone, zdom, bucket, false);
	ZONE_UNLOCK(zone);
	return (true);
}

void *
uma_zalloc_domain(uma_zone_t zone, void *udata, int domain, int flags)
{

	/* Enable entropy collection for RANDOM_ENABLE_UMA kernel option */
	random_harvest_fast_uma(&zone, sizeof(zone), RANDOM_UMA);

	/* This is the fast path allocation */
	CTR5(KTR_UMA,
	    "uma_zalloc_domain thread %x zone %s(%p) domain %d flags %d",
	    curthread, zone->uz_name, zone, domain, flags);

	if (flags & M_WAITOK) {
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "uma_zalloc_domain: zone \"%s\"", zone->uz_name);
	}
	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("uma_zalloc_domain: called with spinlock or critical section held"));

	return (zone_alloc_item(zone, udata, domain, flags));
}

/*
 * Find a slab with some space.  Prefer slabs that are partially used over those
 * that are totally full.  This helps to reduce fragmentation.
 *
 * If 'rr' is 1, search all domains starting from 'domain'.  Otherwise check
 * only 'domain'.
 */
static uma_slab_t
keg_first_slab(uma_keg_t keg, int domain, bool rr)
{
	uma_domain_t dom;
	uma_slab_t slab;
	int start;

	KASSERT(domain >= 0 && domain < vm_ndomains,
	    ("keg_first_slab: domain %d out of range", domain));
	KEG_LOCK_ASSERT(keg);

	slab = NULL;
	start = domain;
	do {
		dom = &keg->uk_domain[domain];
		if (!LIST_EMPTY(&dom->ud_part_slab))
			return (LIST_FIRST(&dom->ud_part_slab));
		if (!LIST_EMPTY(&dom->ud_free_slab)) {
			slab = LIST_FIRST(&dom->ud_free_slab);
			LIST_REMOVE(slab, us_link);
			LIST_INSERT_HEAD(&dom->ud_part_slab, slab, us_link);
			return (slab);
		}
		if (rr)
			domain = (domain + 1) % vm_ndomains;
	} while (domain != start);

	return (NULL);
}

static uma_slab_t
keg_fetch_free_slab(uma_keg_t keg, int domain, bool rr, int flags)
{
	uint32_t reserve;

	KEG_LOCK_ASSERT(keg);

	reserve = (flags & M_USE_RESERVE) != 0 ? 0 : keg->uk_reserve;
	if (keg->uk_free <= reserve)
		return (NULL);
	return (keg_first_slab(keg, domain, rr));
}

static uma_slab_t
keg_fetch_slab(uma_keg_t keg, uma_zone_t zone, int rdomain, const int flags)
{
	struct vm_domainset_iter di;
	uma_domain_t dom;
	uma_slab_t slab;
	int aflags, domain;
	bool rr;

restart:
	KEG_LOCK_ASSERT(keg);

	/*
	 * Use the keg's policy if upper layers haven't already specified a
	 * domain (as happens with first-touch zones).
	 *
	 * To avoid races we run the iterator with the keg lock held, but that
	 * means that we cannot allow the vm_domainset layer to sleep.  Thus,
	 * clear M_WAITOK and handle low memory conditions locally.
	 */
	rr = rdomain == UMA_ANYDOMAIN;
	if (rr) {
		aflags = (flags & ~M_WAITOK) | M_NOWAIT;
		vm_domainset_iter_policy_ref_init(&di, &keg->uk_dr, &domain,
		    &aflags);
	} else {
		aflags = flags;
		domain = rdomain;
	}

	for (;;) {
		slab = keg_fetch_free_slab(keg, domain, rr, flags);
		if (slab != NULL)
			return (slab);

		/*
		 * M_NOVM means don't ask at all!
		 */
		if (flags & M_NOVM)
			break;

		KASSERT(zone->uz_max_items == 0 ||
		    zone->uz_items <= zone->uz_max_items,
		    ("%s: zone %p overflow", __func__, zone));

		slab = keg_alloc_slab(keg, zone, domain, flags, aflags);
		/*
		 * If we got a slab here it's safe to mark it partially used
		 * and return.  We assume that the caller is going to remove
		 * at least one item.
		 */
		if (slab) {
			dom = &keg->uk_domain[slab->us_domain];
			LIST_INSERT_HEAD(&dom->ud_part_slab, slab, us_link);
			return (slab);
		}
		KEG_LOCK(keg);
		if (!rr && (flags & M_WAITOK) == 0)
			break;
		if (rr && vm_domainset_iter_policy(&di, &domain) != 0) {
			if ((flags & M_WAITOK) != 0) {
				KEG_UNLOCK(keg);
				vm_wait_doms(&keg->uk_dr.dr_policy->ds_mask);
				KEG_LOCK(keg);
				goto restart;
			}
			break;
		}
	}

	/*
	 * We might not have been able to get a slab but another cpu
	 * could have while we were unlocked.  Check again before we
	 * fail.
	 */
	if ((slab = keg_fetch_free_slab(keg, domain, rr, flags)) != NULL) {
		return (slab);
	}
	return (NULL);
}

static void *
slab_alloc_item(uma_keg_t keg, uma_slab_t slab)
{
	uma_domain_t dom;
	void *item;
	uint8_t freei;

	KEG_LOCK_ASSERT(keg);

	freei = BIT_FFS(keg->uk_ipers, &slab->us_free) - 1;
	BIT_CLR(keg->uk_ipers, freei, &slab->us_free);
	item = slab_item(slab, keg, freei);
	slab->us_freecount--;
	keg->uk_free--;

	/* Move this slab to the full list */
	if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		dom = &keg->uk_domain[slab->us_domain];
		LIST_INSERT_HEAD(&dom->ud_full_slab, slab, us_link);
	}

	return (item);
}

static int
zone_import(void *arg, void **bucket, int max, int domain, int flags)
{
	uma_zone_t zone;
	uma_slab_t slab;
	uma_keg_t keg;
#ifdef NUMA
	int stripe;
#endif
	int i;

	zone = arg;
	slab = NULL;
	keg = zone->uz_keg;
	KEG_LOCK(keg);
	/* Try to keep the buckets totally full */
	for (i = 0; i < max; ) {
		if ((slab = keg_fetch_slab(keg, zone, domain, flags)) == NULL)
			break;
#ifdef NUMA
		stripe = howmany(max, vm_ndomains);
#endif
		while (slab->us_freecount && i < max) { 
			bucket[i++] = slab_alloc_item(keg, slab);
			if (keg->uk_free <= keg->uk_reserve)
				break;
#ifdef NUMA
			/*
			 * If the zone is striped we pick a new slab for every
			 * N allocations.  Eliminating this conditional will
			 * instead pick a new domain for each bucket rather
			 * than stripe within each bucket.  The current option
			 * produces more fragmentation and requires more cpu
			 * time but yields better distribution.
			 */
			if ((zone->uz_flags & UMA_ZONE_NUMA) == 0 &&
			    vm_ndomains > 1 && --stripe == 0)
				break;
#endif
		}
		/* Don't block if we allocated any successfully. */
		flags &= ~M_WAITOK;
		flags |= M_NOWAIT;
	}
	KEG_UNLOCK(keg);

	return i;
}

static uma_bucket_t
zone_alloc_bucket(uma_zone_t zone, void *udata, int domain, int flags)
{
	uma_bucket_t bucket;
	int maxbucket, cnt;

	CTR1(KTR_UMA, "zone_alloc:_bucket domain %d)", domain);

	/* Avoid allocs targeting empty domains. */
	if (domain != UMA_ANYDOMAIN && VM_DOMAIN_EMPTY(domain))
		domain = UMA_ANYDOMAIN;

	if (zone->uz_max_items > 0) {
		if (zone->uz_items >= zone->uz_max_items)
			return (false);
		maxbucket = MIN(zone->uz_bucket_size,
		    zone->uz_max_items - zone->uz_items);
		zone->uz_items += maxbucket;
	} else
		maxbucket = zone->uz_bucket_size;
	ZONE_UNLOCK(zone);

	/* Don't wait for buckets, preserve caller's NOVM setting. */
	bucket = bucket_alloc(zone, udata, M_NOWAIT | (flags & M_NOVM));
	if (bucket == NULL) {
		cnt = 0;
		goto out;
	}

	bucket->ub_cnt = zone->uz_import(zone->uz_arg, bucket->ub_bucket,
	    MIN(maxbucket, bucket->ub_entries), domain, flags);

	/*
	 * Initialize the memory if necessary.
	 */
	if (bucket->ub_cnt != 0 && zone->uz_init != NULL) {
		int i;

		for (i = 0; i < bucket->ub_cnt; i++)
			if (zone->uz_init(bucket->ub_bucket[i], zone->uz_size,
			    flags) != 0)
				break;
		/*
		 * If we couldn't initialize the whole bucket, put the
		 * rest back onto the freelist.
		 */
		if (i != bucket->ub_cnt) {
			zone->uz_release(zone->uz_arg, &bucket->ub_bucket[i],
			    bucket->ub_cnt - i);
#ifdef INVARIANTS
			bzero(&bucket->ub_bucket[i],
			    sizeof(void *) * (bucket->ub_cnt - i));
#endif
			bucket->ub_cnt = i;
		}
	}

	cnt = bucket->ub_cnt;
	if (bucket->ub_cnt == 0) {
		bucket_free(zone, bucket, udata);
		counter_u64_add(zone->uz_fails, 1);
		bucket = NULL;
	}
out:
	ZONE_LOCK(zone);
	if (zone->uz_max_items > 0 && cnt < maxbucket) {
		MPASS(zone->uz_items >= maxbucket - cnt);
		zone->uz_items -= maxbucket - cnt;
		if (zone->uz_sleepers > 0 &&
		    (cnt == 0 ? zone->uz_items + 1 : zone->uz_items) <
		    zone->uz_max_items)
			wakeup_one(zone);
	}

	return (bucket);
}

/*
 * Allocates a single item from a zone.
 *
 * Arguments
 *	zone   The zone to alloc for.
 *	udata  The data to be passed to the constructor.
 *	domain The domain to allocate from or UMA_ANYDOMAIN.
 *	flags  M_WAITOK, M_NOWAIT, M_ZERO.
 *
 * Returns
 *	NULL if there is no memory and M_NOWAIT is set
 *	An item if successful
 */

static void *
zone_alloc_item(uma_zone_t zone, void *udata, int domain, int flags)
{

	ZONE_LOCK(zone);
	return (zone_alloc_item_locked(zone, udata, domain, flags));
}

/*
 * Returns with zone unlocked.
 */
static void *
zone_alloc_item_locked(uma_zone_t zone, void *udata, int domain, int flags)
{
	void *item;

	ZONE_LOCK_ASSERT(zone);

	if (zone->uz_max_items > 0) {
		if (zone->uz_items >= zone->uz_max_items) {
			zone_log_warning(zone);
			zone_maxaction(zone);
			if (flags & M_NOWAIT) {
				ZONE_UNLOCK(zone);
				return (NULL);
			}
			zone->uz_sleeps++;
			zone->uz_sleepers++;
			while (zone->uz_items >= zone->uz_max_items)
				mtx_sleep(zone, zone->uz_lockptr, PVM,
				    "zonelimit", 0);
			zone->uz_sleepers--;
			if (zone->uz_sleepers > 0 &&
			    zone->uz_items + 1 < zone->uz_max_items)
				wakeup_one(zone);
		}
		zone->uz_items++;
	}
	ZONE_UNLOCK(zone);

	/* Avoid allocs targeting empty domains. */
	if (domain != UMA_ANYDOMAIN && VM_DOMAIN_EMPTY(domain))
		domain = UMA_ANYDOMAIN;

	if (zone->uz_import(zone->uz_arg, &item, 1, domain, flags) != 1)
		goto fail_cnt;

	/*
	 * We have to call both the zone's init (not the keg's init)
	 * and the zone's ctor.  This is because the item is going from
	 * a keg slab directly to the user, and the user is expecting it
	 * to be both zone-init'd as well as zone-ctor'd.
	 */
	if (zone->uz_init != NULL) {
		if (zone->uz_init(item, zone->uz_size, flags) != 0) {
			zone_free_item(zone, item, udata, SKIP_FINI | SKIP_CNT);
			goto fail_cnt;
		}
	}
	item = item_ctor(zone, udata, flags, item);
	if (item == NULL)
		goto fail;

	counter_u64_add(zone->uz_allocs, 1);
	CTR3(KTR_UMA, "zone_alloc_item item %p from %s(%p)", item,
	    zone->uz_name, zone);

	return (item);

fail_cnt:
	counter_u64_add(zone->uz_fails, 1);
fail:
	if (zone->uz_max_items > 0) {
		ZONE_LOCK(zone);
		/* XXX Decrement without wakeup */
		zone->uz_items--;
		ZONE_UNLOCK(zone);
	}
	CTR2(KTR_UMA, "zone_alloc_item failed from %s(%p)",
	    zone->uz_name, zone);
	return (NULL);
}

/* See uma.h */
void
uma_zfree_arg(uma_zone_t zone, void *item, void *udata)
{
	uma_cache_t cache;
	uma_bucket_t bucket;
	int cpu, domain, itemdomain;

	/* Enable entropy collection for RANDOM_ENABLE_UMA kernel option */
	random_harvest_fast_uma(&zone, sizeof(zone), RANDOM_UMA);

	CTR2(KTR_UMA, "uma_zfree_arg thread %x zone %s", curthread,
	    zone->uz_name);

	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("uma_zfree_arg: called with spinlock or critical section held"));

        /* uma_zfree(..., NULL) does nothing, to match free(9). */
        if (item == NULL)
                return;
#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(item)) {
		if (zone->uz_dtor != NULL)
			zone->uz_dtor(item, zone->uz_size, udata);
		if (zone->uz_fini != NULL)
			zone->uz_fini(item, zone->uz_size);
		memguard_free(item);
		return;
	}
#endif
	item_dtor(zone, item, udata, SKIP_NONE);

	/*
	 * The race here is acceptable.  If we miss it we'll just have to wait
	 * a little longer for the limits to be reset.
	 */
	if (zone->uz_sleepers > 0)
		goto zfree_item;

	/*
	 * If possible, free to the per-CPU cache.  There are two
	 * requirements for safe access to the per-CPU cache: (1) the thread
	 * accessing the cache must not be preempted or yield during access,
	 * and (2) the thread must not migrate CPUs without switching which
	 * cache it accesses.  We rely on a critical section to prevent
	 * preemption and migration.  We release the critical section in
	 * order to acquire the zone mutex if we are unable to free to the
	 * current cache; when we re-acquire the critical section, we must
	 * detect and handle migration if it has occurred.
	 */
	domain = itemdomain = 0;
	critical_enter();
	do {
		cpu = curcpu;
		cache = &zone->uz_cpu[cpu];
		bucket = cache->uc_allocbucket;
#ifdef UMA_XDOMAIN
		if ((zone->uz_flags & UMA_ZONE_NUMA) != 0) {
			itemdomain = _vm_phys_domain(pmap_kextract((vm_offset_t)item));
			domain = PCPU_GET(domain);
		}
		if ((zone->uz_flags & UMA_ZONE_NUMA) != 0 &&
		    domain != itemdomain) {
			bucket = cache->uc_crossbucket;
		} else
#endif

		/*
		 * Try to free into the allocbucket first to give LIFO ordering
		 * for cache-hot datastructures.  Spill over into the freebucket
		 * if necessary.  Alloc will swap them if one runs dry.
		 */
		if (bucket == NULL || bucket->ub_cnt >= bucket->ub_entries)
			bucket = cache->uc_freebucket;
		if (__predict_true(bucket != NULL &&
		    bucket->ub_cnt < bucket->ub_entries)) {
			bucket_push(zone, cache, bucket, item);
			critical_exit();
			return;
		}
	} while (cache_free(zone, cache, udata, item, itemdomain));
	critical_exit();

	/*
	 * If nothing else caught this, we'll just do an internal free.
	 */
zfree_item:
	zone_free_item(zone, item, udata, SKIP_DTOR);
}

static void
zone_free_bucket(uma_zone_t zone, uma_bucket_t bucket, void *udata,
    int domain, int itemdomain)
{
	uma_zone_domain_t zdom;

#ifdef UMA_XDOMAIN
	/*
	 * Buckets coming from the wrong domain will be entirely for the
	 * only other domain on two domain systems.  In this case we can
	 * simply cache them.  Otherwise we need to sort them back to
	 * correct domains by freeing the contents to the slab layer.
	 */
	if (domain != itemdomain && vm_ndomains > 2) {
		CTR3(KTR_UMA,
		    "uma_zfree: zone %s(%p) draining cross bucket %p",
		    zone->uz_name, zone, bucket);
		bucket_drain(zone, bucket);
		bucket_free(zone, bucket, udata);
		return;
	}
#endif
	/*
	 * Attempt to save the bucket in the zone's domain bucket cache.
	 *
	 * We bump the uz count when the cache size is insufficient to
	 * handle the working set.
	 */
	if (ZONE_TRYLOCK(zone) == 0) {
		/* Record contention to size the buckets. */
		ZONE_LOCK(zone);
		if (zone->uz_bucket_size < zone->uz_bucket_size_max)
			zone->uz_bucket_size++;
	}

	CTR3(KTR_UMA,
	    "uma_zfree: zone %s(%p) putting bucket %p on free list",
	    zone->uz_name, zone, bucket);
	/* ub_cnt is pointing to the last free item */
	KASSERT(bucket->ub_cnt == bucket->ub_entries,
	    ("uma_zfree: Attempting to insert partial  bucket onto the full list.\n"));
	if (zone->uz_bkt_count >= zone->uz_bkt_max) {
		ZONE_UNLOCK(zone);
		bucket_drain(zone, bucket);
		bucket_free(zone, bucket, udata);
	} else {
		zdom = &zone->uz_domain[itemdomain];
		zone_put_bucket(zone, zdom, bucket, true);
		ZONE_UNLOCK(zone);
	}
}

/*
 * Populate a free or cross bucket for the current cpu cache.  Free any
 * existing full bucket either to the zone cache or back to the slab layer.
 *
 * Enters and returns in a critical section.  false return indicates that
 * we can not satisfy this free in the cache layer.  true indicates that
 * the caller should retry.
 */
static __noinline bool
cache_free(uma_zone_t zone, uma_cache_t cache, void *udata, void *item,
    int itemdomain)
{
	uma_bucket_t bucket;
	int cpu, domain;

	CRITICAL_ASSERT(curthread);

	if (zone->uz_bucket_size == 0 || bucketdisable)
		return false;

	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];

	/*
	 * NUMA domains need to free to the correct zdom.  When XDOMAIN
	 * is enabled this is the zdom of the item and the bucket may be
	 * the cross bucket if they do not match.
	 */
	if ((zone->uz_flags & UMA_ZONE_NUMA) != 0)
#ifdef UMA_XDOMAIN
		domain = PCPU_GET(domain);
#else
		itemdomain = domain = PCPU_GET(domain);
#endif
	else
		itemdomain = domain = 0;
#ifdef UMA_XDOMAIN
	if (domain != itemdomain) {
		bucket = cache->uc_crossbucket;
		cache->uc_crossbucket = NULL;
		if (bucket != NULL)
			atomic_add_64(&zone->uz_xdomain, bucket->ub_cnt);
	} else
#endif
	{
		bucket = cache->uc_freebucket;
		cache->uc_freebucket = NULL;
	}


	/* We are no longer associated with this CPU. */
	critical_exit();

	if (bucket != NULL)
		zone_free_bucket(zone, bucket, udata, domain, itemdomain);

	bucket = bucket_alloc(zone, udata, M_NOWAIT);
	CTR3(KTR_UMA, "uma_zfree: zone %s(%p) allocated bucket %p",
	    zone->uz_name, zone, bucket);
	critical_enter();
	if (bucket == NULL)
		return (false);
	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];
#ifdef UMA_XDOMAIN
	/*
	 * Check to see if we should be populating the cross bucket.  If it
	 * is already populated we will fall through and attempt to populate
	 * the free bucket.
	 */
	if ((zone->uz_flags & UMA_ZONE_NUMA) != 0) {
		domain = PCPU_GET(domain);
		if (domain != itemdomain && cache->uc_crossbucket == NULL) {
			cache->uc_crossbucket = bucket;
			return (true);
		}
	}
#endif
	/*
	 * We may have lost the race to fill the bucket or switched CPUs.
	 */
	if (cache->uc_freebucket != NULL) {
		critical_exit();
		bucket_free(zone, bucket, udata);
		critical_enter();
	} else
		cache->uc_freebucket = bucket;

	return (true);
}

void
uma_zfree_domain(uma_zone_t zone, void *item, void *udata)
{

	/* Enable entropy collection for RANDOM_ENABLE_UMA kernel option */
	random_harvest_fast_uma(&zone, sizeof(zone), RANDOM_UMA);

	CTR2(KTR_UMA, "uma_zfree_domain thread %x zone %s", curthread,
	    zone->uz_name);

	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("uma_zfree_domain: called with spinlock or critical section held"));

        /* uma_zfree(..., NULL) does nothing, to match free(9). */
        if (item == NULL)
                return;
	zone_free_item(zone, item, udata, SKIP_NONE);
}

static void
slab_free_item(uma_zone_t zone, uma_slab_t slab, void *item)
{
	uma_keg_t keg;
	uma_domain_t dom;
	uint8_t freei;

	keg = zone->uz_keg;
	MPASS(zone->uz_lockptr == &keg->uk_lock);
	KEG_LOCK_ASSERT(keg);

	dom = &keg->uk_domain[slab->us_domain];

	/* Do we need to remove from any lists? */
	if (slab->us_freecount+1 == keg->uk_ipers) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&dom->ud_free_slab, slab, us_link);
	} else if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&dom->ud_part_slab, slab, us_link);
	}

	/* Slab management. */
	freei = slab_item_index(slab, keg, item);
	BIT_SET(keg->uk_ipers, freei, &slab->us_free);
	slab->us_freecount++;

	/* Keg statistics. */
	keg->uk_free++;
}

static void
zone_release(void *arg, void **bucket, int cnt)
{
	uma_zone_t zone;
	void *item;
	uma_slab_t slab;
	uma_keg_t keg;
	uint8_t *mem;
	int i;

	zone = arg;
	keg = zone->uz_keg;
	KEG_LOCK(keg);
	for (i = 0; i < cnt; i++) {
		item = bucket[i];
		if (!(zone->uz_flags & UMA_ZONE_VTOSLAB)) {
			mem = (uint8_t *)((uintptr_t)item & (~UMA_SLAB_MASK));
			if (zone->uz_flags & UMA_ZONE_HASH) {
				slab = hash_sfind(&keg->uk_hash, mem);
			} else {
				mem += keg->uk_pgoff;
				slab = (uma_slab_t)mem;
			}
		} else
			slab = vtoslab((vm_offset_t)item);
		slab_free_item(zone, slab, item);
	}
	KEG_UNLOCK(keg);
}

/*
 * Frees a single item to any zone.
 *
 * Arguments:
 *	zone   The zone to free to
 *	item   The item we're freeing
 *	udata  User supplied data for the dtor
 *	skip   Skip dtors and finis
 */
static void
zone_free_item(uma_zone_t zone, void *item, void *udata, enum zfreeskip skip)
{

	item_dtor(zone, item, udata, skip);

	if (skip < SKIP_FINI && zone->uz_fini)
		zone->uz_fini(item, zone->uz_size);

	zone->uz_release(zone->uz_arg, &item, 1);

	if (skip & SKIP_CNT)
		return;

	counter_u64_add(zone->uz_frees, 1);

	if (zone->uz_max_items > 0) {
		ZONE_LOCK(zone);
		zone->uz_items--;
		if (zone->uz_sleepers > 0 &&
		    zone->uz_items < zone->uz_max_items)
			wakeup_one(zone);
		ZONE_UNLOCK(zone);
	}
}

/* See uma.h */
int
uma_zone_set_max(uma_zone_t zone, int nitems)
{
	struct uma_bucket_zone *ubz;
	int count;

	ZONE_LOCK(zone);
	ubz = bucket_zone_max(zone, nitems);
	count = ubz != NULL ? ubz->ubz_entries : 0;
	zone->uz_bucket_size_max = zone->uz_bucket_size = count;
	if (zone->uz_bucket_size_min > zone->uz_bucket_size_max)
		zone->uz_bucket_size_min = zone->uz_bucket_size_max;
	zone->uz_max_items = nitems;
	ZONE_UNLOCK(zone);

	return (nitems);
}

/* See uma.h */
void
uma_zone_set_maxcache(uma_zone_t zone, int nitems)
{
	struct uma_bucket_zone *ubz;
	int bpcpu;

	ZONE_LOCK(zone);
	ubz = bucket_zone_max(zone, nitems);
	if (ubz != NULL) {
		bpcpu = 2;
#ifdef UMA_XDOMAIN
		if ((zone->uz_flags & UMA_ZONE_NUMA) != 0)
			/* Count the cross-domain bucket. */
			bpcpu++;
#endif
		nitems -= ubz->ubz_entries * bpcpu * mp_ncpus;
		zone->uz_bucket_size_max = ubz->ubz_entries;
	} else {
		zone->uz_bucket_size_max = zone->uz_bucket_size = 0;
	}
	if (zone->uz_bucket_size_min > zone->uz_bucket_size_max)
		zone->uz_bucket_size_min = zone->uz_bucket_size_max;
	zone->uz_bkt_max = nitems;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
int
uma_zone_get_max(uma_zone_t zone)
{
	int nitems;

	ZONE_LOCK(zone);
	nitems = zone->uz_max_items;
	ZONE_UNLOCK(zone);

	return (nitems);
}

/* See uma.h */
void
uma_zone_set_warning(uma_zone_t zone, const char *warning)
{

	ZONE_LOCK(zone);
	zone->uz_warning = warning;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_maxaction(uma_zone_t zone, uma_maxaction_t maxaction)
{

	ZONE_LOCK(zone);
	TASK_INIT(&zone->uz_maxaction, 0, (task_fn_t *)maxaction, zone);
	ZONE_UNLOCK(zone);
}

/* See uma.h */
int
uma_zone_get_cur(uma_zone_t zone)
{
	int64_t nitems;
	u_int i;

	ZONE_LOCK(zone);
	nitems = counter_u64_fetch(zone->uz_allocs) -
	    counter_u64_fetch(zone->uz_frees);
	if ((zone->uz_flags & UMA_ZFLAG_INTERNAL) == 0) {
		CPU_FOREACH(i) {
			/*
			 * See the comment in uma_vm_zone_stats() regarding
			 * the safety of accessing the per-cpu caches. With
			 * the zone lock held, it is safe, but can potentially
			 * result in stale data.
			 */
			nitems += zone->uz_cpu[i].uc_allocs -
			    zone->uz_cpu[i].uc_frees;
		}
	}
	ZONE_UNLOCK(zone);

	return (nitems < 0 ? 0 : nitems);
}

static uint64_t
uma_zone_get_allocs(uma_zone_t zone)
{
	uint64_t nitems;
	u_int i;

	ZONE_LOCK(zone);
	nitems = counter_u64_fetch(zone->uz_allocs);
	if ((zone->uz_flags & UMA_ZFLAG_INTERNAL) == 0) {
		CPU_FOREACH(i) {
			/*
			 * See the comment in uma_vm_zone_stats() regarding
			 * the safety of accessing the per-cpu caches. With
			 * the zone lock held, it is safe, but can potentially
			 * result in stale data.
			 */
			nitems += zone->uz_cpu[i].uc_allocs;
		}
	}
	ZONE_UNLOCK(zone);

	return (nitems);
}

static uint64_t
uma_zone_get_frees(uma_zone_t zone)
{
	uint64_t nitems;
	u_int i;

	ZONE_LOCK(zone);
	nitems = counter_u64_fetch(zone->uz_frees);
	if ((zone->uz_flags & UMA_ZFLAG_INTERNAL) == 0) {
		CPU_FOREACH(i) {
			/*
			 * See the comment in uma_vm_zone_stats() regarding
			 * the safety of accessing the per-cpu caches. With
			 * the zone lock held, it is safe, but can potentially
			 * result in stale data.
			 */
			nitems += zone->uz_cpu[i].uc_frees;
		}
	}
	ZONE_UNLOCK(zone);

	return (nitems);
}

/* See uma.h */
void
uma_zone_set_init(uma_zone_t zone, uma_init uminit)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_LOCK(keg);
	KASSERT(keg->uk_pages == 0,
	    ("uma_zone_set_init on non-empty keg"));
	keg->uk_init = uminit;
	KEG_UNLOCK(keg);
}

/* See uma.h */
void
uma_zone_set_fini(uma_zone_t zone, uma_fini fini)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_LOCK(keg);
	KASSERT(keg->uk_pages == 0,
	    ("uma_zone_set_fini on non-empty keg"));
	keg->uk_fini = fini;
	KEG_UNLOCK(keg);
}

/* See uma.h */
void
uma_zone_set_zinit(uma_zone_t zone, uma_init zinit)
{

	ZONE_LOCK(zone);
	KASSERT(zone->uz_keg->uk_pages == 0,
	    ("uma_zone_set_zinit on non-empty keg"));
	zone->uz_init = zinit;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_zfini(uma_zone_t zone, uma_fini zfini)
{

	ZONE_LOCK(zone);
	KASSERT(zone->uz_keg->uk_pages == 0,
	    ("uma_zone_set_zfini on non-empty keg"));
	zone->uz_fini = zfini;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
/* XXX uk_freef is not actually used with the zone locked */
void
uma_zone_set_freef(uma_zone_t zone, uma_free freef)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KASSERT(keg != NULL, ("uma_zone_set_freef: Invalid zone type"));
	KEG_LOCK(keg);
	keg->uk_freef = freef;
	KEG_UNLOCK(keg);
}

/* See uma.h */
/* XXX uk_allocf is not actually used with the zone locked */
void
uma_zone_set_allocf(uma_zone_t zone, uma_alloc allocf)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_LOCK(keg);
	keg->uk_allocf = allocf;
	KEG_UNLOCK(keg);
}

/* See uma.h */
void
uma_zone_reserve(uma_zone_t zone, int items)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_LOCK(keg);
	keg->uk_reserve = items;
	KEG_UNLOCK(keg);
}

/* See uma.h */
int
uma_zone_reserve_kva(uma_zone_t zone, int count)
{
	uma_keg_t keg;
	vm_offset_t kva;
	u_int pages;

	KEG_GET(zone, keg);

	pages = count / keg->uk_ipers;
	if (pages * keg->uk_ipers < count)
		pages++;
	pages *= keg->uk_ppera;

#ifdef UMA_MD_SMALL_ALLOC
	if (keg->uk_ppera > 1) {
#else
	if (1) {
#endif
		kva = kva_alloc((vm_size_t)pages * PAGE_SIZE);
		if (kva == 0)
			return (0);
	} else
		kva = 0;

	ZONE_LOCK(zone);
	MPASS(keg->uk_kva == 0);
	keg->uk_kva = kva;
	keg->uk_offset = 0;
	zone->uz_max_items = pages * keg->uk_ipers;
#ifdef UMA_MD_SMALL_ALLOC
	keg->uk_allocf = (keg->uk_ppera > 1) ? noobj_alloc : uma_small_alloc;
#else
	keg->uk_allocf = noobj_alloc;
#endif
	keg->uk_flags |= UMA_ZONE_NOFREE;
	ZONE_UNLOCK(zone);

	return (1);
}

/* See uma.h */
void
uma_prealloc(uma_zone_t zone, int items)
{
	struct vm_domainset_iter di;
	uma_domain_t dom;
	uma_slab_t slab;
	uma_keg_t keg;
	int aflags, domain, slabs;

	KEG_GET(zone, keg);
	KEG_LOCK(keg);
	slabs = items / keg->uk_ipers;
	if (slabs * keg->uk_ipers < items)
		slabs++;
	while (slabs-- > 0) {
		aflags = M_NOWAIT;
		vm_domainset_iter_policy_ref_init(&di, &keg->uk_dr, &domain,
		    &aflags);
		for (;;) {
			slab = keg_alloc_slab(keg, zone, domain, M_WAITOK,
			    aflags);
			if (slab != NULL) {
				dom = &keg->uk_domain[slab->us_domain];
				LIST_INSERT_HEAD(&dom->ud_free_slab, slab,
				    us_link);
				break;
			}
			KEG_LOCK(keg);
			if (vm_domainset_iter_policy(&di, &domain) != 0) {
				KEG_UNLOCK(keg);
				vm_wait_doms(&keg->uk_dr.dr_policy->ds_mask);
				KEG_LOCK(keg);
			}
		}
	}
	KEG_UNLOCK(keg);
}

/* See uma.h */
void
uma_reclaim(int req)
{

	CTR0(KTR_UMA, "UMA: vm asked us to release pages!");
	sx_xlock(&uma_reclaim_lock);
	bucket_enable();

	switch (req) {
	case UMA_RECLAIM_TRIM:
		zone_foreach(zone_trim, NULL);
		break;
	case UMA_RECLAIM_DRAIN:
	case UMA_RECLAIM_DRAIN_CPU:
		zone_foreach(zone_drain, NULL);
		if (req == UMA_RECLAIM_DRAIN_CPU) {
			pcpu_cache_drain_safe(NULL);
			zone_foreach(zone_drain, NULL);
		}
		break;
	default:
		panic("unhandled reclamation request %d", req);
	}

	/*
	 * Some slabs may have been freed but this zone will be visited early
	 * we visit again so that we can free pages that are empty once other
	 * zones are drained.  We have to do the same for buckets.
	 */
	zone_drain(slabzone, NULL);
	bucket_zone_drain();
	sx_xunlock(&uma_reclaim_lock);
}

static volatile int uma_reclaim_needed;

void
uma_reclaim_wakeup(void)
{

	if (atomic_fetchadd_int(&uma_reclaim_needed, 1) == 0)
		wakeup(uma_reclaim);
}

void
uma_reclaim_worker(void *arg __unused)
{

	for (;;) {
		sx_xlock(&uma_reclaim_lock);
		while (atomic_load_int(&uma_reclaim_needed) == 0)
			sx_sleep(uma_reclaim, &uma_reclaim_lock, PVM, "umarcl",
			    hz);
		sx_xunlock(&uma_reclaim_lock);
		EVENTHANDLER_INVOKE(vm_lowmem, VM_LOW_KMEM);
		uma_reclaim(UMA_RECLAIM_DRAIN_CPU);
		atomic_store_int(&uma_reclaim_needed, 0);
		/* Don't fire more than once per-second. */
		pause("umarclslp", hz);
	}
}

/* See uma.h */
void
uma_zone_reclaim(uma_zone_t zone, int req)
{

	switch (req) {
	case UMA_RECLAIM_TRIM:
		zone_trim(zone, NULL);
		break;
	case UMA_RECLAIM_DRAIN:
		zone_drain(zone, NULL);
		break;
	case UMA_RECLAIM_DRAIN_CPU:
		pcpu_cache_drain_safe(zone);
		zone_drain(zone, NULL);
		break;
	default:
		panic("unhandled reclamation request %d", req);
	}
}

/* See uma.h */
int
uma_zone_exhausted(uma_zone_t zone)
{
	int full;

	ZONE_LOCK(zone);
	full = zone->uz_sleepers > 0;
	ZONE_UNLOCK(zone);
	return (full);	
}

int
uma_zone_exhausted_nolock(uma_zone_t zone)
{
	return (zone->uz_sleepers > 0);
}

static void
uma_zero_item(void *item, uma_zone_t zone)
{

	bzero(item, zone->uz_size);
}

unsigned long
uma_limit(void)
{

	return (uma_kmem_limit);
}

void
uma_set_limit(unsigned long limit)
{

	uma_kmem_limit = limit;
}

unsigned long
uma_size(void)
{

	return (atomic_load_long(&uma_kmem_total));
}

long
uma_avail(void)
{

	return (uma_kmem_limit - uma_size());
}

#ifdef DDB
/*
 * Generate statistics across both the zone and its per-cpu cache's.  Return
 * desired statistics if the pointer is non-NULL for that statistic.
 *
 * Note: does not update the zone statistics, as it can't safely clear the
 * per-CPU cache statistic.
 *
 * XXXRW: Following the uc_allocbucket and uc_freebucket pointers here isn't
 * safe from off-CPU; we should modify the caches to track this information
 * directly so that we don't have to.
 */
static void
uma_zone_sumstat(uma_zone_t z, long *cachefreep, uint64_t *allocsp,
    uint64_t *freesp, uint64_t *sleepsp, uint64_t *xdomainp)
{
	uma_cache_t cache;
	uint64_t allocs, frees, sleeps, xdomain;
	int cachefree, cpu;

	allocs = frees = sleeps = xdomain = 0;
	cachefree = 0;
	CPU_FOREACH(cpu) {
		cache = &z->uz_cpu[cpu];
		if (cache->uc_allocbucket != NULL)
			cachefree += cache->uc_allocbucket->ub_cnt;
		if (cache->uc_freebucket != NULL)
			cachefree += cache->uc_freebucket->ub_cnt;
		if (cache->uc_crossbucket != NULL) {
			xdomain += cache->uc_crossbucket->ub_cnt;
			cachefree += cache->uc_crossbucket->ub_cnt;
		}
		allocs += cache->uc_allocs;
		frees += cache->uc_frees;
	}
	allocs += counter_u64_fetch(z->uz_allocs);
	frees += counter_u64_fetch(z->uz_frees);
	sleeps += z->uz_sleeps;
	xdomain += z->uz_xdomain;
	if (cachefreep != NULL)
		*cachefreep = cachefree;
	if (allocsp != NULL)
		*allocsp = allocs;
	if (freesp != NULL)
		*freesp = frees;
	if (sleepsp != NULL)
		*sleepsp = sleeps;
	if (xdomainp != NULL)
		*xdomainp = xdomain;
}
#endif /* DDB */

static int
sysctl_vm_zone_count(SYSCTL_HANDLER_ARGS)
{
	uma_keg_t kz;
	uma_zone_t z;
	int count;

	count = 0;
	rw_rlock(&uma_rwlock);
	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link)
			count++;
	}
	LIST_FOREACH(z, &uma_cachezones, uz_link)
		count++;

	rw_runlock(&uma_rwlock);
	return (sysctl_handle_int(oidp, &count, 0, req));
}

static void
uma_vm_zone_stats(struct uma_type_header *uth, uma_zone_t z, struct sbuf *sbuf,
    struct uma_percpu_stat *ups, bool internal)
{
	uma_zone_domain_t zdom;
	uma_bucket_t bucket;
	uma_cache_t cache;
	int i;


	for (i = 0; i < vm_ndomains; i++) {
		zdom = &z->uz_domain[i];
		uth->uth_zone_free += zdom->uzd_nitems;
	}
	uth->uth_allocs = counter_u64_fetch(z->uz_allocs);
	uth->uth_frees = counter_u64_fetch(z->uz_frees);
	uth->uth_fails = counter_u64_fetch(z->uz_fails);
	uth->uth_sleeps = z->uz_sleeps;
	uth->uth_xdomain = z->uz_xdomain;

	/*
	 * While it is not normally safe to access the cache bucket pointers
	 * while not on the CPU that owns the cache, we only allow the pointers
	 * to be exchanged without the zone lock held, not invalidated, so
	 * accept the possible race associated with bucket exchange during
	 * monitoring.  Use atomic_load_ptr() to ensure that the bucket pointers
	 * are loaded only once.
	 */
	for (i = 0; i < mp_maxid + 1; i++) {
		bzero(&ups[i], sizeof(*ups));
		if (internal || CPU_ABSENT(i))
			continue;
		cache = &z->uz_cpu[i];
		bucket = (uma_bucket_t)atomic_load_ptr(&cache->uc_allocbucket);
		if (bucket != NULL)
			ups[i].ups_cache_free += bucket->ub_cnt;
		bucket = (uma_bucket_t)atomic_load_ptr(&cache->uc_freebucket);
		if (bucket != NULL)
			ups[i].ups_cache_free += bucket->ub_cnt;
		bucket = (uma_bucket_t)atomic_load_ptr(&cache->uc_crossbucket);
		if (bucket != NULL)
			ups[i].ups_cache_free += bucket->ub_cnt;
		ups[i].ups_allocs = cache->uc_allocs;
		ups[i].ups_frees = cache->uc_frees;
	}
}

static int
sysctl_vm_zone_stats(SYSCTL_HANDLER_ARGS)
{
	struct uma_stream_header ush;
	struct uma_type_header uth;
	struct uma_percpu_stat *ups;
	struct sbuf sbuf;
	uma_keg_t kz;
	uma_zone_t z;
	int count, error, i;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	sbuf_clear_flags(&sbuf, SBUF_INCLUDENUL);
	ups = malloc((mp_maxid + 1) * sizeof(*ups), M_TEMP, M_WAITOK);

	count = 0;
	rw_rlock(&uma_rwlock);
	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link)
			count++;
	}

	LIST_FOREACH(z, &uma_cachezones, uz_link)
		count++;

	/*
	 * Insert stream header.
	 */
	bzero(&ush, sizeof(ush));
	ush.ush_version = UMA_STREAM_VERSION;
	ush.ush_maxcpus = (mp_maxid + 1);
	ush.ush_count = count;
	(void)sbuf_bcat(&sbuf, &ush, sizeof(ush));

	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link) {
			bzero(&uth, sizeof(uth));
			ZONE_LOCK(z);
			strlcpy(uth.uth_name, z->uz_name, UTH_MAX_NAME);
			uth.uth_align = kz->uk_align;
			uth.uth_size = kz->uk_size;
			uth.uth_rsize = kz->uk_rsize;
			if (z->uz_max_items > 0)
				uth.uth_pages = (z->uz_items / kz->uk_ipers) *
					kz->uk_ppera;
			else
				uth.uth_pages = kz->uk_pages;
			uth.uth_maxpages = (z->uz_max_items / kz->uk_ipers) *
			    kz->uk_ppera;
			uth.uth_limit = z->uz_max_items;
			uth.uth_keg_free = z->uz_keg->uk_free;

			/*
			 * A zone is secondary is it is not the first entry
			 * on the keg's zone list.
			 */
			if ((z->uz_flags & UMA_ZONE_SECONDARY) &&
			    (LIST_FIRST(&kz->uk_zones) != z))
				uth.uth_zone_flags = UTH_ZONE_SECONDARY;
			uma_vm_zone_stats(&uth, z, &sbuf, ups,
			    kz->uk_flags & UMA_ZFLAG_INTERNAL);
			ZONE_UNLOCK(z);
			(void)sbuf_bcat(&sbuf, &uth, sizeof(uth));
			for (i = 0; i < mp_maxid + 1; i++)
				(void)sbuf_bcat(&sbuf, &ups[i], sizeof(ups[i]));
		}
	}
	LIST_FOREACH(z, &uma_cachezones, uz_link) {
		bzero(&uth, sizeof(uth));
		ZONE_LOCK(z);
		strlcpy(uth.uth_name, z->uz_name, UTH_MAX_NAME);
		uth.uth_size = z->uz_size;
		uma_vm_zone_stats(&uth, z, &sbuf, ups, false);
		ZONE_UNLOCK(z);
		(void)sbuf_bcat(&sbuf, &uth, sizeof(uth));
		for (i = 0; i < mp_maxid + 1; i++)
			(void)sbuf_bcat(&sbuf, &ups[i], sizeof(ups[i]));
	}

	rw_runlock(&uma_rwlock);
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	free(ups, M_TEMP);
	return (error);
}

int
sysctl_handle_uma_zone_max(SYSCTL_HANDLER_ARGS)
{
	uma_zone_t zone = *(uma_zone_t *)arg1;
	int error, max;

	max = uma_zone_get_max(zone);
	error = sysctl_handle_int(oidp, &max, 0, req);
	if (error || !req->newptr)
		return (error);

	uma_zone_set_max(zone, max);

	return (0);
}

int
sysctl_handle_uma_zone_cur(SYSCTL_HANDLER_ARGS)
{
	uma_zone_t zone;
	int cur;

	/*
	 * Some callers want to add sysctls for global zones that
	 * may not yet exist so they pass a pointer to a pointer.
	 */
	if (arg2 == 0)
		zone = *(uma_zone_t *)arg1;
	else
		zone = arg1;
	cur = uma_zone_get_cur(zone);
	return (sysctl_handle_int(oidp, &cur, 0, req));
}

static int
sysctl_handle_uma_zone_allocs(SYSCTL_HANDLER_ARGS)
{
	uma_zone_t zone = arg1;
	uint64_t cur;

	cur = uma_zone_get_allocs(zone);
	return (sysctl_handle_64(oidp, &cur, 0, req));
}

static int
sysctl_handle_uma_zone_frees(SYSCTL_HANDLER_ARGS)
{
	uma_zone_t zone = arg1;
	uint64_t cur;

	cur = uma_zone_get_frees(zone);
	return (sysctl_handle_64(oidp, &cur, 0, req));
}

static int
sysctl_handle_uma_zone_flags(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	uma_zone_t zone = arg1;
	int error;

	sbuf_new_for_sysctl(&sbuf, NULL, 0, req);
	if (zone->uz_flags != 0)
		sbuf_printf(&sbuf, "0x%b", zone->uz_flags, PRINT_UMA_ZFLAGS);
	else
		sbuf_printf(&sbuf, "0");
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);

	return (error);
}

static int
sysctl_handle_uma_slab_efficiency(SYSCTL_HANDLER_ARGS)
{
	uma_keg_t keg = arg1;
	int avail, effpct, total;

	total = keg->uk_ppera * PAGE_SIZE;
	if ((keg->uk_flags & UMA_ZONE_OFFPAGE) != 0)
		total += slab_sizeof(SLAB_MAX_SETSIZE);
	/*
	 * We consider the client's requested size and alignment here, not the
	 * real size determination uk_rsize, because we also adjust the real
	 * size for internal implementation reasons (max bitset size).
	 */
	avail = keg->uk_ipers * roundup2(keg->uk_size, keg->uk_align + 1);
	if ((keg->uk_flags & UMA_ZONE_PCPU) != 0)
		avail *= mp_maxid + 1;
	effpct = 100 * avail / total;
	return (sysctl_handle_int(oidp, &effpct, 0, req));
}

#ifdef INVARIANTS
static uma_slab_t
uma_dbg_getslab(uma_zone_t zone, void *item)
{
	uma_slab_t slab;
	uma_keg_t keg;
	uint8_t *mem;

	mem = (uint8_t *)((uintptr_t)item & (~UMA_SLAB_MASK));
	if (zone->uz_flags & UMA_ZONE_VTOSLAB) {
		slab = vtoslab((vm_offset_t)mem);
	} else {
		/*
		 * It is safe to return the slab here even though the
		 * zone is unlocked because the item's allocation state
		 * essentially holds a reference.
		 */
		if (zone->uz_lockptr == &zone->uz_lock)
			return (NULL);
		ZONE_LOCK(zone);
		keg = zone->uz_keg;
		if (keg->uk_flags & UMA_ZONE_HASH)
			slab = hash_sfind(&keg->uk_hash, mem);
		else
			slab = (uma_slab_t)(mem + keg->uk_pgoff);
		ZONE_UNLOCK(zone);
	}

	return (slab);
}

static bool
uma_dbg_zskip(uma_zone_t zone, void *mem)
{

	if (zone->uz_lockptr == &zone->uz_lock)
		return (true);

	return (uma_dbg_kskip(zone->uz_keg, mem));
}

static bool
uma_dbg_kskip(uma_keg_t keg, void *mem)
{
	uintptr_t idx;

	if (dbg_divisor == 0)
		return (true);

	if (dbg_divisor == 1)
		return (false);

	idx = (uintptr_t)mem >> PAGE_SHIFT;
	if (keg->uk_ipers > 1) {
		idx *= keg->uk_ipers;
		idx += ((uintptr_t)mem & PAGE_MASK) / keg->uk_rsize;
	}

	if ((idx / dbg_divisor) * dbg_divisor != idx) {
		counter_u64_add(uma_skip_cnt, 1);
		return (true);
	}
	counter_u64_add(uma_dbg_cnt, 1);

	return (false);
}

/*
 * Set up the slab's freei data such that uma_dbg_free can function.
 *
 */
static void
uma_dbg_alloc(uma_zone_t zone, uma_slab_t slab, void *item)
{
	uma_keg_t keg;
	int freei;

	if (slab == NULL) {
		slab = uma_dbg_getslab(zone, item);
		if (slab == NULL) 
			panic("uma: item %p did not belong to zone %s\n",
			    item, zone->uz_name);
	}
	keg = zone->uz_keg;
	freei = slab_item_index(slab, keg, item);

	if (BIT_ISSET(keg->uk_ipers, freei, slab_dbg_bits(slab, keg)))
		panic("Duplicate alloc of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);
	BIT_SET_ATOMIC(keg->uk_ipers, freei, slab_dbg_bits(slab, keg));
}

/*
 * Verifies freed addresses.  Checks for alignment, valid slab membership
 * and duplicate frees.
 *
 */
static void
uma_dbg_free(uma_zone_t zone, uma_slab_t slab, void *item)
{
	uma_keg_t keg;
	int freei;

	if (slab == NULL) {
		slab = uma_dbg_getslab(zone, item);
		if (slab == NULL) 
			panic("uma: Freed item %p did not belong to zone %s\n",
			    item, zone->uz_name);
	}
	keg = zone->uz_keg;
	freei = slab_item_index(slab, keg, item);

	if (freei >= keg->uk_ipers)
		panic("Invalid free of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);

	if (slab_item(slab, keg, freei) != item)
		panic("Unaligned free of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);

	if (!BIT_ISSET(keg->uk_ipers, freei, slab_dbg_bits(slab, keg)))
		panic("Duplicate free of %p from zone %p(%s) slab %p(%d)\n",
		    item, zone, zone->uz_name, slab, freei);

	BIT_CLR_ATOMIC(keg->uk_ipers, freei, slab_dbg_bits(slab, keg));
}
#endif /* INVARIANTS */

#ifdef DDB
static int64_t
get_uma_stats(uma_keg_t kz, uma_zone_t z, uint64_t *allocs, uint64_t *used,
    uint64_t *sleeps, long *cachefree, uint64_t *xdomain)
{
	uint64_t frees;
	int i;

	if (kz->uk_flags & UMA_ZFLAG_INTERNAL) {
		*allocs = counter_u64_fetch(z->uz_allocs);
		frees = counter_u64_fetch(z->uz_frees);
		*sleeps = z->uz_sleeps;
		*cachefree = 0;
		*xdomain = 0;
	} else
		uma_zone_sumstat(z, cachefree, allocs, &frees, sleeps,
		    xdomain);
	if (!((z->uz_flags & UMA_ZONE_SECONDARY) &&
	    (LIST_FIRST(&kz->uk_zones) != z)))
		*cachefree += kz->uk_free;
	for (i = 0; i < vm_ndomains; i++)
		*cachefree += z->uz_domain[i].uzd_nitems;
	*used = *allocs - frees;
	return (((int64_t)*used + *cachefree) * kz->uk_size);
}

DB_SHOW_COMMAND(uma, db_show_uma)
{
	const char *fmt_hdr, *fmt_entry;
	uma_keg_t kz;
	uma_zone_t z;
	uint64_t allocs, used, sleeps, xdomain;
	long cachefree;
	/* variables for sorting */
	uma_keg_t cur_keg;
	uma_zone_t cur_zone, last_zone;
	int64_t cur_size, last_size, size;
	int ties;

	/* /i option produces machine-parseable CSV output */
	if (modif[0] == 'i') {
		fmt_hdr = "%s,%s,%s,%s,%s,%s,%s,%s,%s\n";
		fmt_entry = "\"%s\",%ju,%jd,%ld,%ju,%ju,%u,%jd,%ju\n";
	} else {
		fmt_hdr = "%18s %6s %7s %7s %11s %7s %7s %10s %8s\n";
		fmt_entry = "%18s %6ju %7jd %7ld %11ju %7ju %7u %10jd %8ju\n";
	}

	db_printf(fmt_hdr, "Zone", "Size", "Used", "Free", "Requests",
	    "Sleeps", "Bucket", "Total Mem", "XFree");

	/* Sort the zones with largest size first. */
	last_zone = NULL;
	last_size = INT64_MAX;
	for (;;) {
		cur_zone = NULL;
		cur_size = -1;
		ties = 0;
		LIST_FOREACH(kz, &uma_kegs, uk_link) {
			LIST_FOREACH(z, &kz->uk_zones, uz_link) {
				/*
				 * In the case of size ties, print out zones
				 * in the order they are encountered.  That is,
				 * when we encounter the most recently output
				 * zone, we have already printed all preceding
				 * ties, and we must print all following ties.
				 */
				if (z == last_zone) {
					ties = 1;
					continue;
				}
				size = get_uma_stats(kz, z, &allocs, &used,
				    &sleeps, &cachefree, &xdomain);
				if (size > cur_size && size < last_size + ties)
				{
					cur_size = size;
					cur_zone = z;
					cur_keg = kz;
				}
			}
		}
		if (cur_zone == NULL)
			break;

		size = get_uma_stats(cur_keg, cur_zone, &allocs, &used,
		    &sleeps, &cachefree, &xdomain);
		db_printf(fmt_entry, cur_zone->uz_name,
		    (uintmax_t)cur_keg->uk_size, (intmax_t)used, cachefree,
		    (uintmax_t)allocs, (uintmax_t)sleeps,
		    (unsigned)cur_zone->uz_bucket_size, (intmax_t)size,
		    xdomain);

		if (db_pager_quit)
			return;
		last_zone = cur_zone;
		last_size = cur_size;
	}
}

DB_SHOW_COMMAND(umacache, db_show_umacache)
{
	uma_zone_t z;
	uint64_t allocs, frees;
	long cachefree;
	int i;

	db_printf("%18s %8s %8s %8s %12s %8s\n", "Zone", "Size", "Used", "Free",
	    "Requests", "Bucket");
	LIST_FOREACH(z, &uma_cachezones, uz_link) {
		uma_zone_sumstat(z, &cachefree, &allocs, &frees, NULL, NULL);
		for (i = 0; i < vm_ndomains; i++)
			cachefree += z->uz_domain[i].uzd_nitems;
		db_printf("%18s %8ju %8jd %8ld %12ju %8u\n",
		    z->uz_name, (uintmax_t)z->uz_size,
		    (intmax_t)(allocs - frees), cachefree,
		    (uintmax_t)allocs, z->uz_bucket_size);
		if (db_pager_quit)
			return;
	}
}
#endif	/* DDB */
