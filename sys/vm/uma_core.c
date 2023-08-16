/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include "opt_ddb.h"
#include "opt_param.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/asan.h>
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
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/smr.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_domainset.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_dumpset.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>

#include <ddb/ddb.h>

#ifdef DEBUG_MEMGUARD
#include <vm/memguard.h>
#endif

#include <machine/md_var.h>

#ifdef INVARIANTS
#define	UMA_ALWAYS_CTORDTOR	1
#else
#define	UMA_ALWAYS_CTORDTOR	0
#endif

/*
 * This is the zone and keg from which all zones are spawned.
 */
static uma_zone_t kegs;
static uma_zone_t zones;

/*
 * On INVARIANTS builds, the slab contains a second bitset of the same size,
 * "dbg_bits", which is laid out immediately after us_free.
 */
#ifdef INVARIANTS
#define	SLAB_BITSETS	2
#else
#define	SLAB_BITSETS	1
#endif

/*
 * These are the two zones from which all offpage uma_slab_ts are allocated.
 *
 * One zone is for slab headers that can represent a larger number of items,
 * making the slabs themselves more efficient, and the other zone is for
 * headers that are smaller and represent fewer items, making the headers more
 * efficient.
 */
#define	SLABZONE_SIZE(setsize)					\
    (sizeof(struct uma_hash_slab) + BITSET_SIZE(setsize) * SLAB_BITSETS)
#define	SLABZONE0_SETSIZE	(PAGE_SIZE / 16)
#define	SLABZONE1_SETSIZE	SLAB_MAX_SETSIZE
#define	SLABZONE0_SIZE	SLABZONE_SIZE(SLABZONE0_SETSIZE)
#define	SLABZONE1_SIZE	SLABZONE_SIZE(SLABZONE1_SETSIZE)
static uma_zone_t slabzones[2];

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

/*
 * Mutex for global lists: uma_kegs, uma_cachezones, and the per-keg list of
 * zones.
 */
static struct rwlock_padalign __exclusive_cache_line uma_rwlock;

static struct sx uma_reclaim_lock;

/*
 * First available virual address for boot time allocations.
 */
static vm_offset_t bootstart;
static vm_offset_t bootmem;

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
static enum {
	BOOT_COLD,
	BOOT_KVA,
	BOOT_PCPU,
	BOOT_RUNNING,
	BOOT_SHUTDOWN,
} booted = BOOT_COLD;

/*
 * This is the handle used to schedule events that need to happen
 * outside of the allocation fast path.
 */
static struct timeout_task uma_timeout_task;
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
	const char	*ubz_name;
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

struct uma_bucket_zone bucket_zones[] = {
	/* Literal bucket sizes. */
	{ NULL, "2 Bucket", 2, 4096 },
	{ NULL, "4 Bucket", 4, 3072 },
	{ NULL, "8 Bucket", 8, 2048 },
	{ NULL, "16 Bucket", 16, 1024 },
	/* Rounded down power of 2 sizes for efficiency. */
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

void	uma_startup1(vm_offset_t);
void	uma_startup2(void);

static void *noobj_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void *page_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void *pcpu_page_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void *startup_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void *contig_alloc(uma_zone_t, vm_size_t, int, uint8_t *, int);
static void page_free(void *, vm_size_t, uint8_t);
static void pcpu_page_free(void *, vm_size_t, uint8_t);
static uma_slab_t keg_alloc_slab(uma_keg_t, uma_zone_t, int, int, int);
static void cache_drain(uma_zone_t);
static void bucket_drain(uma_zone_t, uma_bucket_t);
static void bucket_cache_reclaim(uma_zone_t zone, bool, int);
static bool bucket_cache_reclaim_domain(uma_zone_t, bool, bool, int);
static int keg_ctor(void *, int, void *, int);
static void keg_dtor(void *, int, void *);
static void keg_drain(uma_keg_t keg, int domain);
static int zone_ctor(void *, int, void *, int);
static void zone_dtor(void *, int, void *);
static inline void item_dtor(uma_zone_t zone, void *item, int size,
    void *udata, enum zfreeskip skip);
static int zero_init(void *, int, int);
static void zone_free_bucket(uma_zone_t zone, uma_bucket_t bucket, void *udata,
    int itemdomain, bool ws);
static void zone_foreach(void (*zfunc)(uma_zone_t, void *), void *);
static void zone_foreach_unlocked(void (*zfunc)(uma_zone_t, void *), void *);
static void zone_timeout(uma_zone_t zone, void *);
static int hash_alloc(struct uma_hash *, u_int);
static int hash_expand(struct uma_hash *, struct uma_hash *);
static void hash_free(struct uma_hash *hash);
static void uma_timeout(void *, int);
static void uma_shutdown(void);
static void *zone_alloc_item(uma_zone_t, void *, int, int);
static void zone_free_item(uma_zone_t, void *, void *, enum zfreeskip);
static int zone_alloc_limit(uma_zone_t zone, int count, int flags);
static void zone_free_limit(uma_zone_t zone, int count);
static void bucket_enable(void);
static void bucket_init(void);
static uma_bucket_t bucket_alloc(uma_zone_t zone, void *, int);
static void bucket_free(uma_zone_t zone, uma_bucket_t, void *);
static void bucket_zone_drain(int domain);
static uma_bucket_t zone_alloc_bucket(uma_zone_t, void *, int, int);
static void *slab_alloc_item(uma_keg_t keg, uma_slab_t slab);
static void slab_free_item(uma_zone_t zone, uma_slab_t slab, void *item);
static size_t slab_sizeof(int nitems);
static uma_keg_t uma_kcreate(uma_zone_t zone, size_t size, uma_init uminit,
    uma_fini fini, int align, uint32_t flags);
static int zone_import(void *, void **, int, int, int);
static void zone_release(void *, void **, int);
static bool cache_alloc(uma_zone_t, uma_cache_t, void *, int);
static bool cache_free(uma_zone_t, uma_cache_t, void *, int);

static int sysctl_vm_zone_count(SYSCTL_HANDLER_ARGS);
static int sysctl_vm_zone_stats(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_zone_allocs(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_zone_frees(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_zone_flags(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_slab_efficiency(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_uma_zone_items(SYSCTL_HANDLER_ARGS);

static uint64_t uma_zone_get_allocs(uma_zone_t zone);

static SYSCTL_NODE(_vm, OID_AUTO, debug, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Memory allocation debugging");

#ifdef INVARIANTS
static uint64_t uma_keg_get_allocs(uma_keg_t zone);
static inline struct noslabbits *slab_dbg_bits(uma_slab_t slab, uma_keg_t keg);

static bool uma_dbg_kskip(uma_keg_t keg, void *mem);
static bool uma_dbg_zskip(uma_zone_t zone, void *mem);
static void uma_dbg_free(uma_zone_t zone, uma_slab_t slab, void *item);
static void uma_dbg_alloc(uma_zone_t zone, uma_slab_t slab, void *item);

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

SYSCTL_NODE(_vm, OID_AUTO, uma, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Universal Memory Allocator");

SYSCTL_PROC(_vm, OID_AUTO, zone_count, CTLFLAG_RD|CTLFLAG_MPSAFE|CTLTYPE_INT,
    0, 0, sysctl_vm_zone_count, "I", "Number of UMA zones");

SYSCTL_PROC(_vm, OID_AUTO, zone_stats, CTLFLAG_RD|CTLFLAG_MPSAFE|CTLTYPE_STRUCT,
    0, 0, sysctl_vm_zone_stats, "s,struct uma_type_header", "Zone Stats");

static int zone_warnings = 1;
SYSCTL_INT(_vm, OID_AUTO, zone_warnings, CTLFLAG_RWTUN, &zone_warnings, 0,
    "Warn when UMA zones becomes full");

static int multipage_slabs = 1;
TUNABLE_INT("vm.debug.uma_multipage_slabs", &multipage_slabs);
SYSCTL_INT(_vm_debug, OID_AUTO, uma_multipage_slabs,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &multipage_slabs, 0,
    "UMA may choose larger slab sizes for better efficiency");

/*
 * Select the slab zone for an offpage slab with the given maximum item count.
 */
static inline uma_zone_t
slabzone(int ipers)
{

	return (slabzones[ipers > SLABZONE0_SETSIZE]);
}

/*
 * This routine checks to see whether or not it's safe to enable buckets.
 */
static void
bucket_enable(void)
{

	KASSERT(booted >= BOOT_KVA, ("Bucket enable before init"));
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
		    UMA_ZONE_MTXCLASS | UMA_ZFLAG_BUCKET |
		    UMA_ZONE_FIRSTTOUCH);
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
	 * Don't allocate buckets early in boot.
	 */
	if (__predict_false(booted < BOOT_KVA))
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
	if (((uintptr_t)udata & UMA_ZONE_VM) != 0)
		flags |= M_NOVM;
	ubz = bucket_zone_lookup(atomic_load_16(&zone->uz_bucket_size));
	if (ubz->ubz_zone == zone && (ubz + 1)->ubz_entries != 0)
		ubz++;
	bucket = uma_zalloc_arg(ubz->ubz_zone, udata, flags);
	if (bucket) {
#ifdef INVARIANTS
		bzero(bucket->ub_bucket, sizeof(void *) * ubz->ubz_entries);
#endif
		bucket->ub_cnt = 0;
		bucket->ub_entries = min(ubz->ubz_entries,
		    zone->uz_bucket_size_max);
		bucket->ub_seq = SMR_SEQ_INVALID;
		CTR3(KTR_UMA, "bucket_alloc: zone %s(%p) allocated bucket %p",
		    zone->uz_name, zone, bucket);
	}

	return (bucket);
}

static void
bucket_free(uma_zone_t zone, uma_bucket_t bucket, void *udata)
{
	struct uma_bucket_zone *ubz;

	if (bucket->ub_cnt != 0)
		bucket_drain(zone, bucket);

	KASSERT(bucket->ub_cnt == 0,
	    ("bucket_free: Freeing a non free bucket."));
	KASSERT(bucket->ub_seq == SMR_SEQ_INVALID,
	    ("bucket_free: Freeing an SMR bucket."));
	if ((zone->uz_flags & UMA_ZFLAG_BUCKET) == 0)
		udata = (void *)(uintptr_t)zone->uz_flags;
	ubz = bucket_zone_lookup(bucket->ub_entries);
	uma_zfree_arg(ubz->ubz_zone, bucket, udata);
}

static void
bucket_zone_drain(int domain)
{
	struct uma_bucket_zone *ubz;

	for (ubz = &bucket_zones[0]; ubz->ubz_entries != 0; ubz++)
		uma_zone_reclaim_domain(ubz->ubz_zone, UMA_RECLAIM_DRAIN,
		    domain);
}

#ifdef KASAN
_Static_assert(UMA_SMALLEST_UNIT % KASAN_SHADOW_SCALE == 0,
    "Base UMA allocation size not a multiple of the KASAN scale factor");

static void
kasan_mark_item_valid(uma_zone_t zone, void *item)
{
	void *pcpu_item;
	size_t sz, rsz;
	int i;

	if ((zone->uz_flags & UMA_ZONE_NOKASAN) != 0)
		return;

	sz = zone->uz_size;
	rsz = roundup2(sz, KASAN_SHADOW_SCALE);
	if ((zone->uz_flags & UMA_ZONE_PCPU) == 0) {
		kasan_mark(item, sz, rsz, KASAN_GENERIC_REDZONE);
	} else {
		pcpu_item = zpcpu_base_to_offset(item);
		for (i = 0; i <= mp_maxid; i++)
			kasan_mark(zpcpu_get_cpu(pcpu_item, i), sz, rsz,
			    KASAN_GENERIC_REDZONE);
	}
}

static void
kasan_mark_item_invalid(uma_zone_t zone, void *item)
{
	void *pcpu_item;
	size_t sz;
	int i;

	if ((zone->uz_flags & UMA_ZONE_NOKASAN) != 0)
		return;

	sz = roundup2(zone->uz_size, KASAN_SHADOW_SCALE);
	if ((zone->uz_flags & UMA_ZONE_PCPU) == 0) {
		kasan_mark(item, 0, sz, KASAN_UMA_FREED);
	} else {
		pcpu_item = zpcpu_base_to_offset(item);
		for (i = 0; i <= mp_maxid; i++)
			kasan_mark(zpcpu_get_cpu(pcpu_item, i), 0, sz,
			    KASAN_UMA_FREED);
	}
}

static void
kasan_mark_slab_valid(uma_keg_t keg, void *mem)
{
	size_t sz;

	if ((keg->uk_flags & UMA_ZONE_NOKASAN) == 0) {
		sz = keg->uk_ppera * PAGE_SIZE;
		kasan_mark(mem, sz, sz, 0);
	}
}

static void
kasan_mark_slab_invalid(uma_keg_t keg, void *mem)
{
	size_t sz;

	if ((keg->uk_flags & UMA_ZONE_NOKASAN) == 0) {
		if ((keg->uk_flags & UMA_ZFLAG_OFFPAGE) != 0)
			sz = keg->uk_ppera * PAGE_SIZE;
		else
			sz = keg->uk_pgoff;
		kasan_mark(mem, 0, sz, KASAN_UMA_FREED);
	}
}
#else /* !KASAN */
static void
kasan_mark_item_valid(uma_zone_t zone __unused, void *item __unused)
{
}

static void
kasan_mark_item_invalid(uma_zone_t zone __unused, void *item __unused)
{
}

static void
kasan_mark_slab_valid(uma_keg_t keg __unused, void *mem __unused)
{
}

static void
kasan_mark_slab_invalid(uma_keg_t keg __unused, void *mem __unused)
{
}
#endif /* KASAN */

#ifdef KMSAN
static inline void
kmsan_mark_item_uninitialized(uma_zone_t zone, void *item)
{
	void *pcpu_item;
	size_t sz;
	int i;

	if ((zone->uz_flags &
	    (UMA_ZFLAG_CACHE | UMA_ZONE_SECONDARY | UMA_ZONE_MALLOC)) != 0) {
		/*
		 * Cache zones should not be instrumented by default, as UMA
		 * does not have enough information to do so correctly.
		 * Consumers can mark items themselves if it makes sense to do
		 * so.
		 *
		 * Items from secondary zones are initialized by the parent
		 * zone and thus cannot safely be marked by UMA.
		 *
		 * malloc zones are handled directly by malloc(9) and friends,
		 * since they can provide more precise origin tracking.
		 */
		return;
	}
	if (zone->uz_keg->uk_init != NULL) {
		/*
		 * By definition, initialized items cannot be marked.  The
		 * best we can do is mark items from these zones after they
		 * are freed to the keg.
		 */
		return;
	}

	sz = zone->uz_size;
	if ((zone->uz_flags & UMA_ZONE_PCPU) == 0) {
		kmsan_orig(item, sz, KMSAN_TYPE_UMA, KMSAN_RET_ADDR);
		kmsan_mark(item, sz, KMSAN_STATE_UNINIT);
	} else {
		pcpu_item = zpcpu_base_to_offset(item);
		for (i = 0; i <= mp_maxid; i++) {
			kmsan_orig(zpcpu_get_cpu(pcpu_item, i), sz,
			    KMSAN_TYPE_UMA, KMSAN_RET_ADDR);
			kmsan_mark(zpcpu_get_cpu(pcpu_item, i), sz,
			    KMSAN_STATE_INITED);
		}
	}
}
#else /* !KMSAN */
static inline void
kmsan_mark_item_uninitialized(uma_zone_t zone __unused, void *item __unused)
{
}
#endif /* KMSAN */

/*
 * Acquire the domain lock and record contention.
 */
static uma_zone_domain_t
zone_domain_lock(uma_zone_t zone, int domain)
{
	uma_zone_domain_t zdom;
	bool lockfail;

	zdom = ZDOM_GET(zone, domain);
	lockfail = false;
	if (ZDOM_OWNED(zdom))
		lockfail = true;
	ZDOM_LOCK(zdom);
	/* This is unsynchronized.  The counter does not need to be precise. */
	if (lockfail && zone->uz_bucket_size < zone->uz_bucket_size_max)
		zone->uz_bucket_size++;
	return (zdom);
}

/*
 * Search for the domain with the least cached items and return it if it
 * is out of balance with the preferred domain.
 */
static __noinline int
zone_domain_lowest(uma_zone_t zone, int pref)
{
	long least, nitems, prefitems;
	int domain;
	int i;

	prefitems = least = LONG_MAX;
	domain = 0;
	for (i = 0; i < vm_ndomains; i++) {
		nitems = ZDOM_GET(zone, i)->uzd_nitems;
		if (nitems < least) {
			domain = i;
			least = nitems;
		}
		if (domain == pref)
			prefitems = nitems;
	}
	if (prefitems < least * 2)
		return (pref);

	return (domain);
}

/*
 * Search for the domain with the most cached items and return it or the
 * preferred domain if it has enough to proceed.
 */
static __noinline int
zone_domain_highest(uma_zone_t zone, int pref)
{
	long most, nitems;
	int domain;
	int i;

	if (ZDOM_GET(zone, pref)->uzd_nitems > BUCKET_MAX)
		return (pref);

	most = 0;
	domain = 0;
	for (i = 0; i < vm_ndomains; i++) {
		nitems = ZDOM_GET(zone, i)->uzd_nitems;
		if (nitems > most) {
			domain = i;
			most = nitems;
		}
	}

	return (domain);
}

/*
 * Set the maximum imax value.
 */
static void
zone_domain_imax_set(uma_zone_domain_t zdom, int nitems)
{
	long old;

	old = zdom->uzd_imax;
	do {
		if (old >= nitems)
			return;
	} while (atomic_fcmpset_long(&zdom->uzd_imax, &old, nitems) == 0);

	/*
	 * We are at new maximum, so do the last WSS update for the old
	 * bimin and prepare to measure next allocation batch.
	 */
	if (zdom->uzd_wss < old - zdom->uzd_bimin)
		zdom->uzd_wss = old - zdom->uzd_bimin;
	zdom->uzd_bimin = nitems;
}

/*
 * Attempt to satisfy an allocation by retrieving a full bucket from one of the
 * zone's caches.  If a bucket is found the zone is not locked on return.
 */
static uma_bucket_t
zone_fetch_bucket(uma_zone_t zone, uma_zone_domain_t zdom, bool reclaim)
{
	uma_bucket_t bucket;
	long cnt;
	int i;
	bool dtor = false;

	ZDOM_LOCK_ASSERT(zdom);

	if ((bucket = STAILQ_FIRST(&zdom->uzd_buckets)) == NULL)
		return (NULL);

	/* SMR Buckets can not be re-used until readers expire. */
	if ((zone->uz_flags & UMA_ZONE_SMR) != 0 &&
	    bucket->ub_seq != SMR_SEQ_INVALID) {
		if (!smr_poll(zone->uz_smr, bucket->ub_seq, false))
			return (NULL);
		bucket->ub_seq = SMR_SEQ_INVALID;
		dtor = (zone->uz_dtor != NULL) || UMA_ALWAYS_CTORDTOR;
		if (STAILQ_NEXT(bucket, ub_link) != NULL)
			zdom->uzd_seq = STAILQ_NEXT(bucket, ub_link)->ub_seq;
	}
	STAILQ_REMOVE_HEAD(&zdom->uzd_buckets, ub_link);

	KASSERT(zdom->uzd_nitems >= bucket->ub_cnt,
	    ("%s: item count underflow (%ld, %d)",
	    __func__, zdom->uzd_nitems, bucket->ub_cnt));
	KASSERT(bucket->ub_cnt > 0,
	    ("%s: empty bucket in bucket cache", __func__));
	zdom->uzd_nitems -= bucket->ub_cnt;

	if (reclaim) {
		/*
		 * Shift the bounds of the current WSS interval to avoid
		 * perturbing the estimates.
		 */
		cnt = lmin(zdom->uzd_bimin, bucket->ub_cnt);
		atomic_subtract_long(&zdom->uzd_imax, cnt);
		zdom->uzd_bimin -= cnt;
		zdom->uzd_imin -= lmin(zdom->uzd_imin, bucket->ub_cnt);
		if (zdom->uzd_limin >= bucket->ub_cnt) {
			zdom->uzd_limin -= bucket->ub_cnt;
		} else {
			zdom->uzd_limin = 0;
			zdom->uzd_timin = 0;
		}
	} else if (zdom->uzd_bimin > zdom->uzd_nitems) {
		zdom->uzd_bimin = zdom->uzd_nitems;
		if (zdom->uzd_imin > zdom->uzd_nitems)
			zdom->uzd_imin = zdom->uzd_nitems;
	}

	ZDOM_UNLOCK(zdom);
	if (dtor)
		for (i = 0; i < bucket->ub_cnt; i++)
			item_dtor(zone, bucket->ub_bucket[i], zone->uz_size,
			    NULL, SKIP_NONE);

	return (bucket);
}

/*
 * Insert a full bucket into the specified cache.  The "ws" parameter indicates
 * whether the bucket's contents should be counted as part of the zone's working
 * set.  The bucket may be freed if it exceeds the bucket limit.
 */
static void
zone_put_bucket(uma_zone_t zone, int domain, uma_bucket_t bucket, void *udata,
    const bool ws)
{
	uma_zone_domain_t zdom;

	/* We don't cache empty buckets.  This can happen after a reclaim. */
	if (bucket->ub_cnt == 0)
		goto out;
	zdom = zone_domain_lock(zone, domain);

	/*
	 * Conditionally set the maximum number of items.
	 */
	zdom->uzd_nitems += bucket->ub_cnt;
	if (__predict_true(zdom->uzd_nitems < zone->uz_bucket_max)) {
		if (ws) {
			zone_domain_imax_set(zdom, zdom->uzd_nitems);
		} else {
			/*
			 * Shift the bounds of the current WSS interval to
			 * avoid perturbing the estimates.
			 */
			atomic_add_long(&zdom->uzd_imax, bucket->ub_cnt);
			zdom->uzd_imin += bucket->ub_cnt;
			zdom->uzd_bimin += bucket->ub_cnt;
			zdom->uzd_limin += bucket->ub_cnt;
		}
		if (STAILQ_EMPTY(&zdom->uzd_buckets))
			zdom->uzd_seq = bucket->ub_seq;

		/*
		 * Try to promote reuse of recently used items.  For items
		 * protected by SMR, try to defer reuse to minimize polling.
		 */
		if (bucket->ub_seq == SMR_SEQ_INVALID)
			STAILQ_INSERT_HEAD(&zdom->uzd_buckets, bucket, ub_link);
		else
			STAILQ_INSERT_TAIL(&zdom->uzd_buckets, bucket, ub_link);
		ZDOM_UNLOCK(zdom);
		return;
	}
	zdom->uzd_nitems -= bucket->ub_cnt;
	ZDOM_UNLOCK(zdom);
out:
	bucket_free(zone, bucket, udata);
}

/* Pops an item out of a per-cpu cache bucket. */
static inline void *
cache_bucket_pop(uma_cache_t cache, uma_cache_bucket_t bucket)
{
	void *item;

	CRITICAL_ASSERT(curthread);

	bucket->ucb_cnt--;
	item = bucket->ucb_bucket->ub_bucket[bucket->ucb_cnt];
#ifdef INVARIANTS
	bucket->ucb_bucket->ub_bucket[bucket->ucb_cnt] = NULL;
	KASSERT(item != NULL, ("uma_zalloc: Bucket pointer mangled."));
#endif
	cache->uc_allocs++;

	return (item);
}

/* Pushes an item into a per-cpu cache bucket. */
static inline void
cache_bucket_push(uma_cache_t cache, uma_cache_bucket_t bucket, void *item)
{

	CRITICAL_ASSERT(curthread);
	KASSERT(bucket->ucb_bucket->ub_bucket[bucket->ucb_cnt] == NULL,
	    ("uma_zfree: Freeing to non free bucket index."));

	bucket->ucb_bucket->ub_bucket[bucket->ucb_cnt] = item;
	bucket->ucb_cnt++;
	cache->uc_frees++;
}

/*
 * Unload a UMA bucket from a per-cpu cache.
 */
static inline uma_bucket_t
cache_bucket_unload(uma_cache_bucket_t bucket)
{
	uma_bucket_t b;

	b = bucket->ucb_bucket;
	if (b != NULL) {
		MPASS(b->ub_entries == bucket->ucb_entries);
		b->ub_cnt = bucket->ucb_cnt;
		bucket->ucb_bucket = NULL;
		bucket->ucb_entries = bucket->ucb_cnt = 0;
	}

	return (b);
}

static inline uma_bucket_t
cache_bucket_unload_alloc(uma_cache_t cache)
{

	return (cache_bucket_unload(&cache->uc_allocbucket));
}

static inline uma_bucket_t
cache_bucket_unload_free(uma_cache_t cache)
{

	return (cache_bucket_unload(&cache->uc_freebucket));
}

static inline uma_bucket_t
cache_bucket_unload_cross(uma_cache_t cache)
{

	return (cache_bucket_unload(&cache->uc_crossbucket));
}

/*
 * Load a bucket into a per-cpu cache bucket.
 */
static inline void
cache_bucket_load(uma_cache_bucket_t bucket, uma_bucket_t b)
{

	CRITICAL_ASSERT(curthread);
	MPASS(bucket->ucb_bucket == NULL);
	MPASS(b->ub_seq == SMR_SEQ_INVALID);

	bucket->ucb_bucket = b;
	bucket->ucb_cnt = b->ub_cnt;
	bucket->ucb_entries = b->ub_entries;
}

static inline void
cache_bucket_load_alloc(uma_cache_t cache, uma_bucket_t b)
{

	cache_bucket_load(&cache->uc_allocbucket, b);
}

static inline void
cache_bucket_load_free(uma_cache_t cache, uma_bucket_t b)
{

	cache_bucket_load(&cache->uc_freebucket, b);
}

#ifdef NUMA
static inline void 
cache_bucket_load_cross(uma_cache_t cache, uma_bucket_t b)
{

	cache_bucket_load(&cache->uc_crossbucket, b);
}
#endif

/*
 * Copy and preserve ucb_spare.
 */
static inline void
cache_bucket_copy(uma_cache_bucket_t b1, uma_cache_bucket_t b2)
{

	b1->ucb_bucket = b2->ucb_bucket;
	b1->ucb_entries = b2->ucb_entries;
	b1->ucb_cnt = b2->ucb_cnt;
}

/*
 * Swap two cache buckets.
 */
static inline void
cache_bucket_swap(uma_cache_bucket_t b1, uma_cache_bucket_t b2)
{
	struct uma_cache_bucket b3;

	CRITICAL_ASSERT(curthread);

	cache_bucket_copy(&b3, b1);
	cache_bucket_copy(b1, b2);
	cache_bucket_copy(b2, &b3);
}

/*
 * Attempt to fetch a bucket from a zone on behalf of the current cpu cache.
 */
static uma_bucket_t
cache_fetch_bucket(uma_zone_t zone, uma_cache_t cache, int domain)
{
	uma_zone_domain_t zdom;
	uma_bucket_t bucket;
	smr_seq_t seq;

	/*
	 * Avoid the lock if possible.
	 */
	zdom = ZDOM_GET(zone, domain);
	if (zdom->uzd_nitems == 0)
		return (NULL);

	if ((cache_uz_flags(cache) & UMA_ZONE_SMR) != 0 &&
	    (seq = atomic_load_32(&zdom->uzd_seq)) != SMR_SEQ_INVALID &&
	    !smr_poll(zone->uz_smr, seq, false))
		return (NULL);

	/*
	 * Check the zone's cache of buckets.
	 */
	zdom = zone_domain_lock(zone, domain);
	if ((bucket = zone_fetch_bucket(zone, zdom, false)) != NULL)
		return (bucket);
	ZDOM_UNLOCK(zdom);

	return (NULL);
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
uma_timeout(void *context __unused, int pending __unused)
{
	bucket_enable();
	zone_foreach(zone_timeout, NULL);

	/* Reschedule this event */
	taskqueue_enqueue_timeout(taskqueue_thread, &uma_timeout_task,
	    UMA_TIMEOUT * hz);
}

/*
 * Update the working set size estimates for the zone's bucket cache.
 * The constants chosen here are somewhat arbitrary.
 */
static void
zone_domain_update_wss(uma_zone_domain_t zdom)
{
	long m;

	ZDOM_LOCK_ASSERT(zdom);
	MPASS(zdom->uzd_imax >= zdom->uzd_nitems);
	MPASS(zdom->uzd_nitems >= zdom->uzd_bimin);
	MPASS(zdom->uzd_bimin >= zdom->uzd_imin);

	/*
	 * Estimate WSS as modified moving average of biggest allocation
	 * batches for each period over few minutes (UMA_TIMEOUT of 20s).
	 */
	zdom->uzd_wss = lmax(zdom->uzd_wss * 3 / 4,
	    zdom->uzd_imax - zdom->uzd_bimin);

	/*
	 * Estimate longtime minimum item count as a combination of recent
	 * minimum item count, adjusted by WSS for safety, and the modified
	 * moving average over the last several hours (UMA_TIMEOUT of 20s).
	 * timin measures time since limin tried to go negative, that means
	 * we were dangerously close to or got out of cache.
	 */
	m = zdom->uzd_imin - zdom->uzd_wss;
	if (m >= 0) {
		if (zdom->uzd_limin >= m)
			zdom->uzd_limin = m;
		else
			zdom->uzd_limin = (m + zdom->uzd_limin * 255) / 256;
		zdom->uzd_timin++;
	} else {
		zdom->uzd_limin = 0;
		zdom->uzd_timin = 0;
	}

	/* To reduce period edge effects on WSS keep half of the imax. */
	atomic_subtract_long(&zdom->uzd_imax,
	    (zdom->uzd_imax - zdom->uzd_nitems + 1) / 2);
	zdom->uzd_imin = zdom->uzd_bimin = zdom->uzd_nitems;
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
	u_int slabs, pages;

	if ((zone->uz_flags & UMA_ZFLAG_HASH) == 0)
		goto trim;

	keg = zone->uz_keg;

	/*
	 * Hash zones are non-numa by definition so the first domain
	 * is the only one present.
	 */
	KEG_LOCK(keg, 0);
	pages = keg->uk_domain[0].ud_pages;

	/*
	 * Expand the keg hash table.
	 *
	 * This is done if the number of slabs is larger than the hash size.
	 * What I'm trying to do here is completely reduce collisions.  This
	 * may be a little aggressive.  Should I allow for two collisions max?
	 */
	if ((slabs = pages / keg->uk_ppera) > keg->uk_hash.uh_hashsize) {
		struct uma_hash newhash;
		struct uma_hash oldhash;
		int ret;

		/*
		 * This is so involved because allocating and freeing
		 * while the keg lock is held will lead to deadlock.
		 * I have to do everything in stages and check for
		 * races.
		 */
		KEG_UNLOCK(keg, 0);
		ret = hash_alloc(&newhash, 1 << fls(slabs));
		KEG_LOCK(keg, 0);
		if (ret) {
			if (hash_expand(&keg->uk_hash, &newhash)) {
				oldhash = keg->uk_hash;
				keg->uk_hash = newhash;
			} else
				oldhash = newhash;

			KEG_UNLOCK(keg, 0);
			hash_free(&oldhash);
			goto trim;
		}
	}
	KEG_UNLOCK(keg, 0);

trim:
	/* Trim caches not used for a long time. */
	if ((zone->uz_flags & UMA_ZONE_UNMANAGED) == 0) {
		for (int i = 0; i < vm_ndomains; i++) {
			if (bucket_cache_reclaim_domain(zone, false, false, i) &&
			    (zone->uz_flags & UMA_ZFLAG_CACHE) == 0)
				keg_drain(zone->uz_keg, i);
		}
	}
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
 *	bucket The free/alloc bucket with items.
 *
 * Returns:
 *	Nothing
 */
static void
bucket_drain(uma_zone_t zone, uma_bucket_t bucket)
{
	int i;

	if (bucket->ub_cnt == 0)
		return;

	if ((zone->uz_flags & UMA_ZONE_SMR) != 0 &&
	    bucket->ub_seq != SMR_SEQ_INVALID) {
		smr_wait(zone->uz_smr, bucket->ub_seq);
		bucket->ub_seq = SMR_SEQ_INVALID;
		for (i = 0; i < bucket->ub_cnt; i++)
			item_dtor(zone, bucket->ub_bucket[i],
			    zone->uz_size, NULL, SKIP_NONE);
	}
	if (zone->uz_fini)
		for (i = 0; i < bucket->ub_cnt; i++) {
			kasan_mark_item_valid(zone, bucket->ub_bucket[i]);
			zone->uz_fini(bucket->ub_bucket[i], zone->uz_size);
			kasan_mark_item_invalid(zone, bucket->ub_bucket[i]);
		}
	zone->uz_release(zone->uz_arg, bucket->ub_bucket, bucket->ub_cnt);
	if (zone->uz_max_items > 0)
		zone_free_limit(zone, bucket->ub_cnt);
#ifdef INVARIANTS
	bzero(bucket->ub_bucket, sizeof(void *) * bucket->ub_cnt);
#endif
	bucket->ub_cnt = 0;
}

/*
 * Drains the per cpu caches for a zone.
 *
 * NOTE: This may only be called while the zone is being torn down, and not
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
	uma_bucket_t bucket;
	smr_seq_t seq;
	int cpu;

	/*
	 * XXX: It is safe to not lock the per-CPU caches, because we're
	 * tearing down the zone anyway.  I.e., there will be no further use
	 * of the caches at this point.
	 *
	 * XXX: It would good to be able to assert that the zone is being
	 * torn down to prevent improper use of cache_drain().
	 */
	seq = SMR_SEQ_INVALID;
	if ((zone->uz_flags & UMA_ZONE_SMR) != 0)
		seq = smr_advance(zone->uz_smr);
	CPU_FOREACH(cpu) {
		cache = &zone->uz_cpu[cpu];
		bucket = cache_bucket_unload_alloc(cache);
		if (bucket != NULL)
			bucket_free(zone, bucket, NULL);
		bucket = cache_bucket_unload_free(cache);
		if (bucket != NULL) {
			bucket->ub_seq = seq;
			bucket_free(zone, bucket, NULL);
		}
		bucket = cache_bucket_unload_cross(cache);
		if (bucket != NULL) {
			bucket->ub_seq = seq;
			bucket_free(zone, bucket, NULL);
		}
	}
	bucket_cache_reclaim(zone, true, UMA_ANYDOMAIN);
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
	critical_enter();
	cache = &zone->uz_cpu[curcpu];
	domain = PCPU_GET(domain);
	b1 = cache_bucket_unload_alloc(cache);

	/*
	 * Don't flush SMR zone buckets.  This leaves the zone without a
	 * bucket and forces every free to synchronize().
	 */
	if ((zone->uz_flags & UMA_ZONE_SMR) == 0) {
		b2 = cache_bucket_unload_free(cache);
		b3 = cache_bucket_unload_cross(cache);
	}
	critical_exit();

	if (b1 != NULL)
		zone_free_bucket(zone, b1, NULL, domain, false);
	if (b2 != NULL)
		zone_free_bucket(zone, b2, NULL, domain, false);
	if (b3 != NULL) {
		/* Adjust the domain so it goes to zone_free_cross. */
		domain = (domain + 1) % vm_ndomains;
		zone_free_bucket(zone, b3, NULL, domain, false);
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
	 * Polite bucket sizes shrinking was not enough, shrink aggressively.
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
static bool
bucket_cache_reclaim_domain(uma_zone_t zone, bool drain, bool trim, int domain)
{
	uma_zone_domain_t zdom;
	uma_bucket_t bucket;
	long target;
	bool done = false;

	/*
	 * The cross bucket is partially filled and not part of
	 * the item count.  Reclaim it individually here.
	 */
	zdom = ZDOM_GET(zone, domain);
	if ((zone->uz_flags & UMA_ZONE_SMR) == 0 || drain) {
		ZONE_CROSS_LOCK(zone);
		bucket = zdom->uzd_cross;
		zdom->uzd_cross = NULL;
		ZONE_CROSS_UNLOCK(zone);
		if (bucket != NULL)
			bucket_free(zone, bucket, NULL);
	}

	/*
	 * If we were asked to drain the zone, we are done only once
	 * this bucket cache is empty.  If trim, we reclaim items in
	 * excess of the zone's estimated working set size.  Multiple
	 * consecutive calls will shrink the WSS and so reclaim more.
	 * If neither drain nor trim, then voluntarily reclaim 1/4
	 * (to reduce first spike) of items not used for a long time.
	 */
	ZDOM_LOCK(zdom);
	zone_domain_update_wss(zdom);
	if (drain)
		target = 0;
	else if (trim)
		target = zdom->uzd_wss;
	else if (zdom->uzd_timin > 900 / UMA_TIMEOUT)
		target = zdom->uzd_nitems - zdom->uzd_limin / 4;
	else {
		ZDOM_UNLOCK(zdom);
		return (done);
	}
	while ((bucket = STAILQ_FIRST(&zdom->uzd_buckets)) != NULL &&
	    zdom->uzd_nitems >= target + bucket->ub_cnt) {
		bucket = zone_fetch_bucket(zone, zdom, true);
		if (bucket == NULL)
			break;
		bucket_free(zone, bucket, NULL);
		done = true;
		ZDOM_LOCK(zdom);
	}
	ZDOM_UNLOCK(zdom);
	return (done);
}

static void
bucket_cache_reclaim(uma_zone_t zone, bool drain, int domain)
{
	int i;

	/*
	 * Shrink the zone bucket size to ensure that the per-CPU caches
	 * don't grow too large.
	 */
	if (zone->uz_bucket_size > zone->uz_bucket_size_min)
		zone->uz_bucket_size--;

	if (domain != UMA_ANYDOMAIN &&
	    (zone->uz_flags & UMA_ZONE_ROUNDROBIN) == 0) {
		bucket_cache_reclaim_domain(zone, drain, true, domain);
	} else {
		for (i = 0; i < vm_ndomains; i++)
			bucket_cache_reclaim_domain(zone, drain, true, i);
	}
}

static void
keg_free_slab(uma_keg_t keg, uma_slab_t slab, int start)
{
	uint8_t *mem;
	size_t size;
	int i;
	uint8_t flags;

	CTR4(KTR_UMA, "keg_free_slab keg %s(%p) slab %p, returning %d bytes",
	    keg->uk_name, keg, slab, PAGE_SIZE * keg->uk_ppera);

	mem = slab_data(slab, keg);
	size = PAGE_SIZE * keg->uk_ppera;

	kasan_mark_slab_valid(keg, mem);
	if (keg->uk_fini != NULL) {
		for (i = start - 1; i > -1; i--)
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
	flags = slab->us_flags;
	if (keg->uk_flags & UMA_ZFLAG_OFFPAGE) {
		zone_free_item(slabzone(keg->uk_ipers), slab_tohashslab(slab),
		    NULL, SKIP_NONE);
	}
	keg->uk_freef(mem, size, flags);
	uma_total_dec(size);
}

static void
keg_drain_domain(uma_keg_t keg, int domain)
{
	struct slabhead freeslabs;
	uma_domain_t dom;
	uma_slab_t slab, tmp;
	uint32_t i, stofree, stokeep, partial;

	dom = &keg->uk_domain[domain];
	LIST_INIT(&freeslabs);

	CTR4(KTR_UMA, "keg_drain %s(%p) domain %d free items: %u",
	    keg->uk_name, keg, domain, dom->ud_free_items);

	KEG_LOCK(keg, domain);

	/*
	 * Are the free items in partially allocated slabs sufficient to meet
	 * the reserve? If not, compute the number of fully free slabs that must
	 * be kept.
	 */
	partial = dom->ud_free_items - dom->ud_free_slabs * keg->uk_ipers;
	if (partial < keg->uk_reserve) {
		stokeep = min(dom->ud_free_slabs,
		    howmany(keg->uk_reserve - partial, keg->uk_ipers));
	} else {
		stokeep = 0;
	}
	stofree = dom->ud_free_slabs - stokeep;

	/*
	 * Partition the free slabs into two sets: those that must be kept in
	 * order to maintain the reserve, and those that may be released back to
	 * the system.  Since one set may be much larger than the other,
	 * populate the smaller of the two sets and swap them if necessary.
	 */
	for (i = min(stofree, stokeep); i > 0; i--) {
		slab = LIST_FIRST(&dom->ud_free_slab);
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&freeslabs, slab, us_link);
	}
	if (stofree > stokeep)
		LIST_SWAP(&freeslabs, &dom->ud_free_slab, uma_slab, us_link);

	if ((keg->uk_flags & UMA_ZFLAG_HASH) != 0) {
		LIST_FOREACH(slab, &freeslabs, us_link)
			UMA_HASH_REMOVE(&keg->uk_hash, slab);
	}
	dom->ud_free_items -= stofree * keg->uk_ipers;
	dom->ud_free_slabs -= stofree;
	dom->ud_pages -= stofree * keg->uk_ppera;
	KEG_UNLOCK(keg, domain);

	LIST_FOREACH_SAFE(slab, &freeslabs, us_link, tmp)
		keg_free_slab(keg, slab, keg->uk_ipers);
}

/*
 * Frees pages from a keg back to the system.  This is done on demand from
 * the pageout daemon.
 *
 * Returns nothing.
 */
static void
keg_drain(uma_keg_t keg, int domain)
{
	int i;

	if ((keg->uk_flags & UMA_ZONE_NOFREE) != 0)
		return;
	if (domain != UMA_ANYDOMAIN) {
		keg_drain_domain(keg, domain);
	} else {
		for (i = 0; i < vm_ndomains; i++)
			keg_drain_domain(keg, i);
	}
}

static void
zone_reclaim(uma_zone_t zone, int domain, int waitok, bool drain)
{
	/*
	 * Count active reclaim operations in order to interlock with
	 * zone_dtor(), which removes the zone from global lists before
	 * attempting to reclaim items itself.
	 *
	 * The zone may be destroyed while sleeping, so only zone_dtor() should
	 * specify M_WAITOK.
	 */
	ZONE_LOCK(zone);
	if (waitok == M_WAITOK) {
		while (zone->uz_reclaimers > 0)
			msleep(zone, ZONE_LOCKPTR(zone), PVM, "zonedrain", 1);
	}
	zone->uz_reclaimers++;
	ZONE_UNLOCK(zone);
	bucket_cache_reclaim(zone, drain, domain);

	if ((zone->uz_flags & UMA_ZFLAG_CACHE) == 0)
		keg_drain(zone->uz_keg, domain);
	ZONE_LOCK(zone);
	zone->uz_reclaimers--;
	if (zone->uz_reclaimers == 0)
		wakeup(zone);
	ZONE_UNLOCK(zone);
}

/*
 * Allocate a new slab for a keg and inserts it into the partial slab list.
 * The keg should be unlocked on entry.  If the allocation succeeds it will
 * be locked on return.
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
	uma_domain_t dom;
	uma_slab_t slab;
	unsigned long size;
	uint8_t *mem;
	uint8_t sflags;
	int i;

	TSENTER();

	KASSERT(domain >= 0 && domain < vm_ndomains,
	    ("keg_alloc_slab: domain %d out of range", domain));

	slab = NULL;
	mem = NULL;
	if (keg->uk_flags & UMA_ZFLAG_OFFPAGE) {
		uma_hash_slab_t hslab;
		hslab = zone_alloc_item(slabzone(keg->uk_ipers), NULL,
		    domain, aflags);
		if (hslab == NULL)
			goto fail;
		slab = &hslab->uhs_slab;
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
	mem = keg->uk_allocf(zone, size, domain, &sflags, aflags);
	if (mem == NULL) {
		if (keg->uk_flags & UMA_ZFLAG_OFFPAGE)
			zone_free_item(slabzone(keg->uk_ipers),
			    slab_tohashslab(slab), NULL, SKIP_NONE);
		goto fail;
	}
	uma_total_inc(size);

	/* For HASH zones all pages go to the same uma_domain. */
	if ((keg->uk_flags & UMA_ZFLAG_HASH) != 0)
		domain = 0;

	kmsan_mark(mem, size,
	    (aflags & M_ZERO) != 0 ? KMSAN_STATE_INITED : KMSAN_STATE_UNINIT);

	/* Point the slab into the allocated memory */
	if (!(keg->uk_flags & UMA_ZFLAG_OFFPAGE))
		slab = (uma_slab_t)(mem + keg->uk_pgoff);
	else
		slab_tohashslab(slab)->uhs_data = mem;

	if (keg->uk_flags & UMA_ZFLAG_VTOSLAB)
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
			goto fail;
		}
	}
	kasan_mark_slab_invalid(keg, mem);
	KEG_LOCK(keg, domain);

	CTR3(KTR_UMA, "keg_alloc_slab: allocated slab %p for %s(%p)",
	    slab, keg->uk_name, keg);

	if (keg->uk_flags & UMA_ZFLAG_HASH)
		UMA_HASH_INSERT(&keg->uk_hash, slab, mem);

	/*
	 * If we got a slab here it's safe to mark it partially used
	 * and return.  We assume that the caller is going to remove
	 * at least one item.
	 */
	dom = &keg->uk_domain[domain];
	LIST_INSERT_HEAD(&dom->ud_part_slab, slab, us_link);
	dom->ud_pages += keg->uk_ppera;
	dom->ud_free_items += keg->uk_ipers;

	TSEXIT();
	return (slab);

fail:
	return (NULL);
}

/*
 * This function is intended to be used early on in place of page_alloc().  It
 * performs contiguous physical memory allocations and uses a bump allocator for
 * KVA, so is usable before the kernel map is initialized.
 */
static void *
startup_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *pflag,
    int wait)
{
	vm_paddr_t pa;
	vm_page_t m;
	int i, pages;

	pages = howmany(bytes, PAGE_SIZE);
	KASSERT(pages > 0, ("%s can't reserve 0 pages", __func__));

	*pflag = UMA_SLAB_BOOT;
	m = vm_page_alloc_noobj_contig_domain(domain, malloc2vm_flags(wait) |
	    VM_ALLOC_WIRED, pages, (vm_paddr_t)0, ~(vm_paddr_t)0, 1, 0,
	    VM_MEMATTR_DEFAULT);
	if (m == NULL)
		return (NULL);

	pa = VM_PAGE_TO_PHYS(m);
	for (i = 0; i < pages; i++, pa += PAGE_SIZE) {
#if defined(__aarch64__) || defined(__amd64__) || \
    defined(__riscv) || defined(__powerpc64__)
		if ((wait & M_NODUMP) == 0)
			dump_add_page(pa);
#endif
	}

	/* Allocate KVA and indirectly advance bootmem. */
	return ((void *)pmap_map(&bootmem, m->phys_addr,
	    m->phys_addr + (pages * PAGE_SIZE), VM_PROT_READ | VM_PROT_WRITE));
}

static void
startup_free(void *mem, vm_size_t bytes)
{
	vm_offset_t va;
	vm_page_t m;

	va = (vm_offset_t)mem;
	m = PHYS_TO_VM_PAGE(pmap_kextract(va));

	/*
	 * startup_alloc() returns direct-mapped slabs on some platforms.  Avoid
	 * unmapping ranges of the direct map.
	 */
	if (va >= bootstart && va + bytes <= bootmem)
		pmap_remove(kernel_pmap, va, va + bytes);
	for (; bytes != 0; bytes -= PAGE_SIZE, m++) {
#if defined(__aarch64__) || defined(__amd64__) || \
    defined(__riscv) || defined(__powerpc64__)
		dump_drop_page(VM_PAGE_TO_PHYS(m));
#endif
		vm_page_unwire_noq(m);
		vm_page_free(m);
	}
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
	p = kmem_malloc_domainset(DOMAINSET_FIXED(domain), bytes, wait);

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
	flags = VM_ALLOC_SYSTEM | VM_ALLOC_WIRED | malloc2vm_flags(wait);
	*pflag = UMA_SLAB_KERNEL;
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu)) {
			p = vm_page_alloc_noobj(flags);
		} else {
#ifndef NUMA
			p = vm_page_alloc_noobj(flags);
#else
			pc = pcpu_find(cpu);
			if (__predict_false(VM_DOMAIN_EMPTY(pc->pc_domain)))
				p = NULL;
			else
				p = vm_page_alloc_noobj_domain(pc->pc_domain,
				    flags);
			if (__predict_false(p == NULL))
				p = vm_page_alloc_noobj(flags);
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
 * Allocates a number of pages not belonging to a VM object
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
	int req;

	TAILQ_INIT(&alloctail);
	keg = zone->uz_keg;
	req = VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED;
	if ((wait & M_WAITOK) != 0)
		req |= VM_ALLOC_WAITOK;

	npages = howmany(bytes, PAGE_SIZE);
	while (npages > 0) {
		p = vm_page_alloc_noobj_domain(domain, req);
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
 * Allocate physically contiguous pages.
 */
static void *
contig_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *pflag,
    int wait)
{

	*pflag = UMA_SLAB_KERNEL;
	return ((void *)kmem_alloc_contig_domainset(DOMAINSET_FIXED(domain),
	    bytes, wait, 0, ~(vm_paddr_t)0, 1, 0, VM_MEMATTR_DEFAULT));
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

	if ((flags & UMA_SLAB_BOOT) != 0) {
		startup_free(mem, size);
		return;
	}

	KASSERT((flags & UMA_SLAB_KERNEL) != 0,
	    ("UMA: page_free used with invalid flags %x", flags));

	kmem_free(mem, size);
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

	if ((flags & UMA_SLAB_BOOT) != 0) {
		startup_free(mem, size);
		return;
	}

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
static struct noslabbits *
slab_dbg_bits(uma_slab_t slab, uma_keg_t keg)
{

	return ((void *)((char *)&slab->us_free + BITSET_SIZE(keg->uk_ipers)));
}
#endif

/*
 * Actual size of embedded struct slab (!OFFPAGE).
 */
static size_t
slab_sizeof(int nitems)
{
	size_t s;

	s = sizeof(struct uma_slab) + BITSET_SIZE(nitems) * SLAB_BITSETS;
	return (roundup(s, UMA_ALIGN_PTR + 1));
}

#define	UMA_FIXPT_SHIFT	31
#define	UMA_FRAC_FIXPT(n, d)						\
	((uint32_t)(((uint64_t)(n) << UMA_FIXPT_SHIFT) / (d)))
#define	UMA_FIXPT_PCT(f)						\
	((u_int)(((uint64_t)100 * (f)) >> UMA_FIXPT_SHIFT))
#define	UMA_PCT_FIXPT(pct)	UMA_FRAC_FIXPT((pct), 100)
#define	UMA_MIN_EFF	UMA_PCT_FIXPT(100 - UMA_MAX_WASTE)

/*
 * Compute the number of items that will fit in a slab.  If hdr is true, the
 * item count may be limited to provide space in the slab for an inline slab
 * header.  Otherwise, all slab space will be provided for item storage.
 */
static u_int
slab_ipers_hdr(u_int size, u_int rsize, u_int slabsize, bool hdr)
{
	u_int ipers;
	u_int padpi;

	/* The padding between items is not needed after the last item. */
	padpi = rsize - size;

	if (hdr) {
		/*
		 * Start with the maximum item count and remove items until
		 * the slab header first alongside the allocatable memory.
		 */
		for (ipers = MIN(SLAB_MAX_SETSIZE,
		    (slabsize + padpi - slab_sizeof(1)) / rsize);
		    ipers > 0 &&
		    ipers * rsize - padpi + slab_sizeof(ipers) > slabsize;
		    ipers--)
			continue;
	} else {
		ipers = MIN((slabsize + padpi) / rsize, SLAB_MAX_SETSIZE);
	}

	return (ipers);
}

struct keg_layout_result {
	u_int format;
	u_int slabsize;
	u_int ipers;
	u_int eff;
};

static void
keg_layout_one(uma_keg_t keg, u_int rsize, u_int slabsize, u_int fmt,
    struct keg_layout_result *kl)
{
	u_int total;

	kl->format = fmt;
	kl->slabsize = slabsize;

	/* Handle INTERNAL as inline with an extra page. */
	if ((fmt & UMA_ZFLAG_INTERNAL) != 0) {
		kl->format &= ~UMA_ZFLAG_INTERNAL;
		kl->slabsize += PAGE_SIZE;
	}

	kl->ipers = slab_ipers_hdr(keg->uk_size, rsize, kl->slabsize,
	    (fmt & UMA_ZFLAG_OFFPAGE) == 0);

	/* Account for memory used by an offpage slab header. */
	total = kl->slabsize;
	if ((fmt & UMA_ZFLAG_OFFPAGE) != 0)
		total += slabzone(kl->ipers)->uz_keg->uk_rsize;

	kl->eff = UMA_FRAC_FIXPT(kl->ipers * rsize, total);
}

/*
 * Determine the format of a uma keg.  This determines where the slab header
 * will be placed (inline or offpage) and calculates ipers, rsize, and ppera.
 *
 * Arguments
 *	keg  The zone we should initialize
 *
 * Returns
 *	Nothing
 */
static void
keg_layout(uma_keg_t keg)
{
	struct keg_layout_result kl = {}, kl_tmp;
	u_int fmts[2];
	u_int alignsize;
	u_int nfmt;
	u_int pages;
	u_int rsize;
	u_int slabsize;
	u_int i, j;

	KASSERT((keg->uk_flags & UMA_ZONE_PCPU) == 0 ||
	    (keg->uk_size <= UMA_PCPU_ALLOC_SIZE &&
	     (keg->uk_flags & UMA_ZONE_CACHESPREAD) == 0),
	    ("%s: cannot configure for PCPU: keg=%s, size=%u, flags=0x%b",
	     __func__, keg->uk_name, keg->uk_size, keg->uk_flags,
	     PRINT_UMA_ZFLAGS));
	KASSERT((keg->uk_flags & (UMA_ZFLAG_INTERNAL | UMA_ZONE_VM)) == 0 ||
	    (keg->uk_flags & (UMA_ZONE_NOTOUCH | UMA_ZONE_PCPU)) == 0,
	    ("%s: incompatible flags 0x%b", __func__, keg->uk_flags,
	     PRINT_UMA_ZFLAGS));

	alignsize = keg->uk_align + 1;
#ifdef KASAN
	/*
	 * ASAN requires that each allocation be aligned to the shadow map
	 * scale factor.
	 */
	if (alignsize < KASAN_SHADOW_SCALE)
		alignsize = KASAN_SHADOW_SCALE;
#endif

	/*
	 * Calculate the size of each allocation (rsize) according to
	 * alignment.  If the requested size is smaller than we have
	 * allocation bits for we round it up.
	 */
	rsize = MAX(keg->uk_size, UMA_SMALLEST_UNIT);
	rsize = roundup2(rsize, alignsize);

	if ((keg->uk_flags & UMA_ZONE_CACHESPREAD) != 0) {
		/*
		 * We want one item to start on every align boundary in a page.
		 * To do this we will span pages.  We will also extend the item
		 * by the size of align if it is an even multiple of align.
		 * Otherwise, it would fall on the same boundary every time.
		 */
		if ((rsize & alignsize) == 0)
			rsize += alignsize;
		slabsize = rsize * (PAGE_SIZE / alignsize);
		slabsize = MIN(slabsize, rsize * SLAB_MAX_SETSIZE);
		slabsize = MIN(slabsize, UMA_CACHESPREAD_MAX_SIZE);
		slabsize = round_page(slabsize);
	} else {
		/*
		 * Start with a slab size of as many pages as it takes to
		 * represent a single item.  We will try to fit as many
		 * additional items into the slab as possible.
		 */
		slabsize = round_page(keg->uk_size);
	}

	/* Build a list of all of the available formats for this keg. */
	nfmt = 0;

	/* Evaluate an inline slab layout. */
	if ((keg->uk_flags & (UMA_ZONE_NOTOUCH | UMA_ZONE_PCPU)) == 0)
		fmts[nfmt++] = 0;

	/* TODO: vm_page-embedded slab. */

	/*
	 * We can't do OFFPAGE if we're internal or if we've been
	 * asked to not go to the VM for buckets.  If we do this we
	 * may end up going to the VM for slabs which we do not want
	 * to do if we're UMA_ZONE_VM, which clearly forbids it.
	 * In those cases, evaluate a pseudo-format called INTERNAL
	 * which has an inline slab header and one extra page to
	 * guarantee that it fits.
	 *
	 * Otherwise, see if using an OFFPAGE slab will improve our
	 * efficiency.
	 */
	if ((keg->uk_flags & (UMA_ZFLAG_INTERNAL | UMA_ZONE_VM)) != 0)
		fmts[nfmt++] = UMA_ZFLAG_INTERNAL;
	else
		fmts[nfmt++] = UMA_ZFLAG_OFFPAGE;

	/*
	 * Choose a slab size and format which satisfy the minimum efficiency.
	 * Prefer the smallest slab size that meets the constraints.
	 *
	 * Start with a minimum slab size, to accommodate CACHESPREAD.  Then,
	 * for small items (up to PAGE_SIZE), the iteration increment is one
	 * page; and for large items, the increment is one item.
	 */
	i = (slabsize + rsize - keg->uk_size) / MAX(PAGE_SIZE, rsize);
	KASSERT(i >= 1, ("keg %s(%p) flags=0x%b slabsize=%u, rsize=%u, i=%u",
	    keg->uk_name, keg, keg->uk_flags, PRINT_UMA_ZFLAGS, slabsize,
	    rsize, i));
	for ( ; ; i++) {
		slabsize = (rsize <= PAGE_SIZE) ? ptoa(i) :
		    round_page(rsize * (i - 1) + keg->uk_size);

		for (j = 0; j < nfmt; j++) {
			/* Only if we have no viable format yet. */
			if ((fmts[j] & UMA_ZFLAG_INTERNAL) != 0 &&
			    kl.ipers > 0)
				continue;

			keg_layout_one(keg, rsize, slabsize, fmts[j], &kl_tmp);
			if (kl_tmp.eff <= kl.eff)
				continue;

			kl = kl_tmp;

			CTR6(KTR_UMA, "keg %s layout: format %#x "
			    "(ipers %u * rsize %u) / slabsize %#x = %u%% eff",
			    keg->uk_name, kl.format, kl.ipers, rsize,
			    kl.slabsize, UMA_FIXPT_PCT(kl.eff));

			/* Stop when we reach the minimum efficiency. */
			if (kl.eff >= UMA_MIN_EFF)
				break;
		}

		if (kl.eff >= UMA_MIN_EFF || !multipage_slabs ||
		    slabsize >= SLAB_MAX_SETSIZE * rsize ||
		    (keg->uk_flags & (UMA_ZONE_PCPU | UMA_ZONE_CONTIG)) != 0)
			break;
	}

	pages = atop(kl.slabsize);
	if ((keg->uk_flags & UMA_ZONE_PCPU) != 0)
		pages *= mp_maxid + 1;

	keg->uk_rsize = rsize;
	keg->uk_ipers = kl.ipers;
	keg->uk_ppera = pages;
	keg->uk_flags |= kl.format;

	/*
	 * How do we find the slab header if it is offpage or if not all item
	 * start addresses are in the same page?  We could solve the latter
	 * case with vaddr alignment, but we don't.
	 */
	if ((keg->uk_flags & UMA_ZFLAG_OFFPAGE) != 0 ||
	    (keg->uk_ipers - 1) * rsize >= PAGE_SIZE) {
		if ((keg->uk_flags & UMA_ZONE_NOTPAGE) != 0)
			keg->uk_flags |= UMA_ZFLAG_HASH;
		else
			keg->uk_flags |= UMA_ZFLAG_VTOSLAB;
	}

	CTR6(KTR_UMA, "%s: keg=%s, flags=%#x, rsize=%u, ipers=%u, ppera=%u",
	    __func__, keg->uk_name, keg->uk_flags, rsize, keg->uk_ipers,
	    pages);
	KASSERT(keg->uk_ipers > 0 && keg->uk_ipers <= SLAB_MAX_SETSIZE,
	    ("%s: keg=%s, flags=0x%b, rsize=%u, ipers=%u, ppera=%u", __func__,
	     keg->uk_name, keg->uk_flags, PRINT_UMA_ZFLAGS, rsize,
	     keg->uk_ipers, pages));
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
	int i;

	bzero(keg, size);
	keg->uk_size = arg->size;
	keg->uk_init = arg->uminit;
	keg->uk_fini = arg->fini;
	keg->uk_align = arg->align;
	keg->uk_reserve = 0;
	keg->uk_flags = arg->flags;

	/*
	 * We use a global round-robin policy by default.  Zones with
	 * UMA_ZONE_FIRSTTOUCH set will use first-touch instead, in which
	 * case the iterator is never run.
	 */
	keg->uk_dr.dr_policy = DOMAINSET_RR();
	keg->uk_dr.dr_iter = 0;

	/*
	 * The primary zone is passed to us at keg-creation time.
	 */
	zone = arg->zone;
	keg->uk_name = zone->uz_name;

	if (arg->flags & UMA_ZONE_ZINIT)
		keg->uk_init = zero_init;

	if (arg->flags & UMA_ZONE_MALLOC)
		keg->uk_flags |= UMA_ZFLAG_VTOSLAB;

#ifndef SMP
	keg->uk_flags &= ~UMA_ZONE_PCPU;
#endif

	keg_layout(keg);

	/*
	 * Use a first-touch NUMA policy for kegs that pmap_extract() will
	 * work on.  Use round-robin for everything else.
	 *
	 * Zones may override the default by specifying either.
	 */
#ifdef NUMA
	if ((keg->uk_flags &
	    (UMA_ZONE_ROUNDROBIN | UMA_ZFLAG_CACHE | UMA_ZONE_NOTPAGE)) == 0)
		keg->uk_flags |= UMA_ZONE_FIRSTTOUCH;
	else if ((keg->uk_flags & UMA_ZONE_FIRSTTOUCH) == 0)
		keg->uk_flags |= UMA_ZONE_ROUNDROBIN;
#endif

	/*
	 * If we haven't booted yet we need allocations to go through the
	 * startup cache until the vm is ready.
	 */
#ifdef UMA_MD_SMALL_ALLOC
	if (keg->uk_ppera == 1)
		keg->uk_allocf = uma_small_alloc;
	else
#endif
	if (booted < BOOT_KVA)
		keg->uk_allocf = startup_alloc;
	else if (keg->uk_flags & UMA_ZONE_PCPU)
		keg->uk_allocf = pcpu_page_alloc;
	else if ((keg->uk_flags & UMA_ZONE_CONTIG) != 0 && keg->uk_ppera > 1)
		keg->uk_allocf = contig_alloc;
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
	 * Initialize keg's locks.
	 */
	for (i = 0; i < vm_ndomains; i++)
		KEG_LOCK_INIT(keg, i, (arg->flags & UMA_ZONE_MTXCLASS));

	/*
	 * If we're putting the slab header in the actual page we need to
	 * figure out where in each page it goes.  See slab_sizeof
	 * definition.
	 */
	if (!(keg->uk_flags & UMA_ZFLAG_OFFPAGE)) {
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

	if (keg->uk_flags & UMA_ZFLAG_HASH)
		hash_alloc(&keg->uk_hash, 0);

	CTR3(KTR_UMA, "keg_ctor %p zone %s(%p)", keg, zone->uz_name, zone);

	LIST_INSERT_HEAD(&keg->uk_zones, zone, uz_link);

	rw_wlock(&uma_rwlock);
	LIST_INSERT_HEAD(&uma_kegs, keg, uk_link);
	rw_wunlock(&uma_rwlock);
	return (0);
}

static void
zone_kva_available(uma_zone_t zone, void *unused)
{
	uma_keg_t keg;

	if ((zone->uz_flags & UMA_ZFLAG_CACHE) != 0)
		return;
	KEG_GET(zone, keg);

	if (keg->uk_allocf == startup_alloc) {
		/* Switch to the real allocator. */
		if (keg->uk_flags & UMA_ZONE_PCPU)
			keg->uk_allocf = pcpu_page_alloc;
		else if ((keg->uk_flags & UMA_ZONE_CONTIG) != 0 &&
		    keg->uk_ppera > 1)
			keg->uk_allocf = contig_alloc;
		else
			keg->uk_allocf = page_alloc;
	}
}

static void
zone_alloc_counters(uma_zone_t zone, void *unused)
{

	zone->uz_allocs = counter_u64_alloc(M_WAITOK);
	zone->uz_frees = counter_u64_alloc(M_WAITOK);
	zone->uz_fails = counter_u64_alloc(M_WAITOK);
	zone->uz_xdomain = counter_u64_alloc(M_WAITOK);
}

static void
zone_alloc_sysctl(uma_zone_t zone, void *unused)
{
	uma_zone_domain_t zdom;
	uma_domain_t dom;
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
	    OID_AUTO, zone->uz_ctlname, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
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
	if ((zone->uz_flags & UMA_ZFLAG_HASH) == 0)
		domains = vm_ndomains;
	else
		domains = 1;
	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid), OID_AUTO,
	    "keg", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
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
		    "reserve", CTLFLAG_RD, &keg->uk_reserve, 0,
		    "number of reserved items");
		SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "efficiency", CTLFLAG_RD | CTLTYPE_INT | CTLFLAG_MPSAFE,
		    keg, 0, sysctl_handle_uma_slab_efficiency, "I",
		    "Slab utilization (100 - internal fragmentation %)");
		domainoid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(oid),
		    OID_AUTO, "domain", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
		for (i = 0; i < domains; i++) {
			dom = &keg->uk_domain[i];
			oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(domainoid),
			    OID_AUTO, VM_DOMAIN(i)->vmd_name,
			    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
			SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
			    "pages", CTLFLAG_RD, &dom->ud_pages, 0,
			    "Total pages currently allocated from VM");
			SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
			    "free_items", CTLFLAG_RD, &dom->ud_free_items, 0,
			    "Items free in the slab layer");
			SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
			    "free_slabs", CTLFLAG_RD, &dom->ud_free_slabs, 0,
			    "Unused slabs");
		}
	} else
		SYSCTL_ADD_CONST_STRING(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "name", CTLFLAG_RD, nokeg, "Keg name");

	/*
	 * Information about zone limits.
	 */
	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid), OID_AUTO,
	    "limit", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "items", CTLFLAG_RD | CTLTYPE_U64 | CTLFLAG_MPSAFE,
	    zone, 0, sysctl_handle_uma_zone_items, "QU",
	    "Current number of allocated items if limit is set");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "max_items", CTLFLAG_RD, &zone->uz_max_items, 0,
	    "Maximum number of allocated and cached items");
	SYSCTL_ADD_U32(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "sleepers", CTLFLAG_RD, &zone->uz_sleepers, 0,
	    "Number of threads sleeping at limit");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "sleeps", CTLFLAG_RD, &zone->uz_sleeps, 0,
	    "Total zone limit sleeps");
	SYSCTL_ADD_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "bucket_max", CTLFLAG_RD, &zone->uz_bucket_max, 0,
	    "Maximum number of items in each domain's bucket cache");

	/*
	 * Per-domain zone information.
	 */
	domainoid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid),
	    OID_AUTO, "domain", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	for (i = 0; i < domains; i++) {
		zdom = ZDOM_GET(zone, i);
		oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(domainoid),
		    OID_AUTO, VM_DOMAIN(i)->vmd_name,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
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
		    "bimin", CTLFLAG_RD, &zdom->uzd_bimin,
		    "Minimum item count in this batch");
		SYSCTL_ADD_LONG(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "wss", CTLFLAG_RD, &zdom->uzd_wss,
		    "Working set size");
		SYSCTL_ADD_LONG(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "limin", CTLFLAG_RD, &zdom->uzd_limin,
		    "Long time minimum item count");
		SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "timin", CTLFLAG_RD, &zdom->uzd_timin, 0,
		    "Time since zero long time minimum item count");
	}

	/*
	 * General statistics.
	 */
	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(zone->uz_oid), OID_AUTO,
	    "stats", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
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
	SYSCTL_ADD_COUNTER_U64(NULL, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "xdomain", CTLFLAG_RD, &zone->uz_xdomain,
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

static void
zone_update_caches(uma_zone_t zone)
{
	int i;

	for (i = 0; i <= mp_maxid; i++) {
		cache_set_uz_size(&zone->uz_cpu[i], zone->uz_size);
		cache_set_uz_flags(&zone->uz_cpu[i], zone->uz_flags);
	}
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
	uma_zone_domain_t zdom;
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
	zone->uz_bucket_size = 0;
	zone->uz_bucket_size_min = 0;
	zone->uz_bucket_size_max = BUCKET_MAX;
	zone->uz_flags = (arg->flags & UMA_ZONE_SMR);
	zone->uz_warning = NULL;
	/* The domain structures follow the cpu structures. */
	zone->uz_bucket_max = ULONG_MAX;
	timevalclear(&zone->uz_ratecheck);

	/* Count the number of duplicate names. */
	cnt.name = arg->name;
	cnt.count = 0;
	zone_foreach(zone_count, &cnt);
	zone->uz_namecnt = cnt.count;
	ZONE_CROSS_LOCK_INIT(zone);

	for (i = 0; i < vm_ndomains; i++) {
		zdom = ZDOM_GET(zone, i);
		ZDOM_LOCK_INIT(zone, zdom, (arg->flags & UMA_ZONE_MTXCLASS));
		STAILQ_INIT(&zdom->uzd_buckets);
	}

#if defined(INVARIANTS) && !defined(KASAN) && !defined(KMSAN)
	if (arg->uminit == trash_init && arg->fini == trash_fini)
		zone->uz_flags |= UMA_ZFLAG_TRASH | UMA_ZFLAG_CTORDTOR;
#elif defined(KASAN)
	if ((arg->flags & (UMA_ZONE_NOFREE | UMA_ZFLAG_CACHE)) != 0)
		arg->flags |= UMA_ZONE_NOKASAN;
#endif

	/*
	 * This is a pure cache zone, no kegs.
	 */
	if (arg->import) {
		KASSERT((arg->flags & UMA_ZFLAG_CACHE) != 0,
		    ("zone_ctor: Import specified for non-cache zone."));
		zone->uz_flags = arg->flags;
		zone->uz_size = arg->size;
		zone->uz_import = arg->import;
		zone->uz_release = arg->release;
		zone->uz_arg = arg->arg;
#ifdef NUMA
		/*
		 * Cache zones are round-robin unless a policy is
		 * specified because they may have incompatible
		 * constraints.
		 */
		if ((zone->uz_flags & UMA_ZONE_FIRSTTOUCH) == 0)
			zone->uz_flags |= UMA_ZONE_ROUNDROBIN;
#endif
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
		karg.flags = (arg->flags & ~UMA_ZONE_SMR);
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
	if (booted >= BOOT_PCPU) {
		zone_alloc_counters(zone, NULL);
		if (booted >= BOOT_RUNNING)
			zone_alloc_sysctl(zone, NULL);
	} else {
		zone->uz_allocs = EARLY_COUNTER;
		zone->uz_frees = EARLY_COUNTER;
		zone->uz_fails = EARLY_COUNTER;
	}

	/* Caller requests a private SMR context. */
	if ((zone->uz_flags & UMA_ZONE_SMR) != 0)
		zone->uz_smr = smr_create(zone->uz_name, 0, 0);

	KASSERT((arg->flags & (UMA_ZONE_MAXBUCKET | UMA_ZONE_NOBUCKET)) !=
	    (UMA_ZONE_MAXBUCKET | UMA_ZONE_NOBUCKET),
	    ("Invalid zone flag combination"));
	if (arg->flags & UMA_ZFLAG_INTERNAL)
		zone->uz_bucket_size_max = zone->uz_bucket_size = 0;
	if ((arg->flags & UMA_ZONE_MAXBUCKET) != 0)
		zone->uz_bucket_size = BUCKET_MAX;
	else if ((arg->flags & UMA_ZONE_NOBUCKET) != 0)
		zone->uz_bucket_size = 0;
	else
		zone->uz_bucket_size = bucket_select(zone->uz_size);
	zone->uz_bucket_size_min = zone->uz_bucket_size;
	if (zone->uz_dtor != NULL || zone->uz_ctor != NULL)
		zone->uz_flags |= UMA_ZFLAG_CTORDTOR;
	zone_update_caches(zone);

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
	uint32_t free, pages;
	int i;

	keg = (uma_keg_t)arg;
	free = pages = 0;
	for (i = 0; i < vm_ndomains; i++) {
		free += keg->uk_domain[i].ud_free_items;
		pages += keg->uk_domain[i].ud_pages;
		KEG_LOCK_FINI(keg, i);
	}
	if (pages != 0)
		printf("Freed UMA keg (%s) was not empty (%u items). "
		    " Lost %u pages of memory.\n",
		    keg->uk_name ? keg->uk_name : "",
		    pages / keg->uk_ppera * keg->uk_ipers - free, pages);

	hash_free(&keg->uk_hash);
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
	int i;

	zone = (uma_zone_t)arg;

	sysctl_remove_oid(zone->uz_oid, 1, 1);

	if (!(zone->uz_flags & UMA_ZFLAG_INTERNAL))
		cache_drain(zone);

	rw_wlock(&uma_rwlock);
	LIST_REMOVE(zone, uz_link);
	rw_wunlock(&uma_rwlock);
	if ((zone->uz_flags & (UMA_ZONE_SECONDARY | UMA_ZFLAG_CACHE)) == 0) {
		keg = zone->uz_keg;
		keg->uk_reserve = 0;
	}
	zone_reclaim(zone, UMA_ANYDOMAIN, M_WAITOK, true);

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
	counter_u64_free(zone->uz_xdomain);
	free(zone->uz_ctlname, M_UMA);
	for (i = 0; i < vm_ndomains; i++)
		ZDOM_LOCK_FINI(ZDOM_GET(zone, i));
	ZONE_CROSS_LOCK_FINI(zone);
}

static void
zone_foreach_unlocked(void (*zfunc)(uma_zone_t, void *arg), void *arg)
{
	uma_keg_t keg;
	uma_zone_t zone;

	LIST_FOREACH(keg, &uma_kegs, uk_link) {
		LIST_FOREACH(zone, &keg->uk_zones, uz_link)
			zfunc(zone, arg);
	}
	LIST_FOREACH(zone, &uma_cachezones, uz_link)
		zfunc(zone, arg);
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

	rw_rlock(&uma_rwlock);
	zone_foreach_unlocked(zfunc, arg);
	rw_runlock(&uma_rwlock);
}

/*
 * Initialize the kernel memory allocator.  This is done after pages can be
 * allocated but before general KVA is available.
 */
void
uma_startup1(vm_offset_t virtual_avail)
{
	struct uma_zctor_args args;
	size_t ksize, zsize, size;
	uma_keg_t primarykeg;
	uintptr_t m;
	int domain;
	uint8_t pflag;

	bootstart = bootmem = virtual_avail;

	rw_init(&uma_rwlock, "UMA lock");
	sx_init(&uma_reclaim_lock, "umareclaim");

	ksize = sizeof(struct uma_keg) +
	    (sizeof(struct uma_domain) * vm_ndomains);
	ksize = roundup(ksize, UMA_SUPER_ALIGN);
	zsize = sizeof(struct uma_zone) +
	    (sizeof(struct uma_cache) * (mp_maxid + 1)) +
	    (sizeof(struct uma_zone_domain) * vm_ndomains);
	zsize = roundup(zsize, UMA_SUPER_ALIGN);

	/* Allocate the zone of zones, zone of kegs, and zone of zones keg. */
	size = (zsize * 2) + ksize;
	for (domain = 0; domain < vm_ndomains; domain++) {
		m = (uintptr_t)startup_alloc(NULL, size, domain, &pflag,
		    M_NOWAIT | M_ZERO);
		if (m != 0)
			break;
	}
	zones = (uma_zone_t)m;
	m += zsize;
	kegs = (uma_zone_t)m;
	m += zsize;
	primarykeg = (uma_keg_t)m;

	/* "manually" create the initial zone */
	memset(&args, 0, sizeof(args));
	args.name = "UMA Kegs";
	args.size = ksize;
	args.ctor = keg_ctor;
	args.dtor = keg_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.keg = primarykeg;
	args.align = UMA_SUPER_ALIGN - 1;
	args.flags = UMA_ZFLAG_INTERNAL;
	zone_ctor(kegs, zsize, &args, M_WAITOK);

	args.name = "UMA Zones";
	args.size = zsize;
	args.ctor = zone_ctor;
	args.dtor = zone_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.keg = NULL;
	args.align = UMA_SUPER_ALIGN - 1;
	args.flags = UMA_ZFLAG_INTERNAL;
	zone_ctor(zones, zsize, &args, M_WAITOK);

	/* Now make zones for slab headers */
	slabzones[0] = uma_zcreate("UMA Slabs 0", SLABZONE0_SIZE,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);
	slabzones[1] = uma_zcreate("UMA Slabs 1", SLABZONE1_SIZE,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);

	hashzone = uma_zcreate("UMA Hash",
	    sizeof(struct slabhead *) * UMA_HASH_SIZE_INIT,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);

	bucket_init();
	smr_init();
}

#ifndef UMA_MD_SMALL_ALLOC
extern void vm_radix_reserve_kva(void);
#endif

/*
 * Advertise the availability of normal kva allocations and switch to
 * the default back-end allocator.  Marks the KVA we consumed on startup
 * as used in the map.
 */
void
uma_startup2(void)
{

	if (bootstart != bootmem) {
		vm_map_lock(kernel_map);
		(void)vm_map_insert(kernel_map, NULL, 0, bootstart, bootmem,
		    VM_PROT_RW, VM_PROT_RW, MAP_NOFAULT);
		vm_map_unlock(kernel_map);
	}

#ifndef UMA_MD_SMALL_ALLOC
	/* Set up radix zone to use noobj_alloc. */
	vm_radix_reserve_kva();
#endif

	booted = BOOT_KVA;
	zone_foreach_unlocked(zone_kva_available, NULL);
	bucket_enable();
}

/*
 * Allocate counters as early as possible so that boot-time allocations are
 * accounted more precisely.
 */
static void
uma_startup_pcpu(void *arg __unused)
{

	zone_foreach_unlocked(zone_alloc_counters, NULL);
	booted = BOOT_PCPU;
}
SYSINIT(uma_startup_pcpu, SI_SUB_COUNTER, SI_ORDER_ANY, uma_startup_pcpu, NULL);

/*
 * Finish our initialization steps.
 */
static void
uma_startup3(void *arg __unused)
{

#ifdef INVARIANTS
	TUNABLE_INT_FETCH("vm.debug.divisor", &dbg_divisor);
	uma_dbg_cnt = counter_u64_alloc(M_WAITOK);
	uma_skip_cnt = counter_u64_alloc(M_WAITOK);
#endif
	zone_foreach_unlocked(zone_alloc_sysctl, NULL);
	booted = BOOT_RUNNING;

	EVENTHANDLER_REGISTER(shutdown_post_sync, uma_shutdown, NULL,
	    EVENTHANDLER_PRI_FIRST);
}
SYSINIT(uma_startup3, SI_SUB_VM_CONF, SI_ORDER_SECOND, uma_startup3, NULL);

static void
uma_startup4(void *arg __unused)
{
	TIMEOUT_TASK_INIT(taskqueue_thread, &uma_timeout_task, 0, uma_timeout,
	    NULL);
	taskqueue_enqueue_timeout(taskqueue_thread, &uma_timeout_task,
	    UMA_TIMEOUT * hz);
}
SYSINIT(uma_startup4, SI_SUB_TASKQ, SI_ORDER_ANY, uma_startup4, NULL);

static void
uma_shutdown(void)
{

	booted = BOOT_SHUTDOWN;
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

	KASSERT(powerof2(align + 1), ("invalid zone alignment %d for \"%s\"",
	    align, name));

	/* This stuff is essential for the zone ctor */
	memset(&args, 0, sizeof(args));
	args.name = name;
	args.size = size;
	args.ctor = ctor;
	args.dtor = dtor;
	args.uminit = uminit;
	args.fini = fini;
#if defined(INVARIANTS) && !defined(KASAN) && !defined(KMSAN)
	/*
	 * Inject procedures which check for memory use after free if we are
	 * allowed to scramble the memory while it is not allocated.  This
	 * requires that: UMA is actually able to access the memory, no init
	 * or fini procedures, no dependency on the initial value of the
	 * memory, and no (legitimate) use of the memory after free.  Note,
	 * the ctor and dtor do not need to be empty.
	 */
	if ((!(flags & (UMA_ZONE_ZINIT | UMA_ZONE_NOTOUCH |
	    UMA_ZONE_NOFREE))) && uminit == NULL && fini == NULL) {
		args.uminit = trash_init;
		args.fini = trash_fini;
	}
#endif
	args.align = align;
	args.flags = flags;
	args.keg = NULL;

	sx_xlock(&uma_reclaim_lock);
	res = zone_alloc_item(zones, &args, UMA_ANYDOMAIN, M_WAITOK);
	sx_xunlock(&uma_reclaim_lock);

	return (res);
}

/* See uma.h */
uma_zone_t
uma_zsecond_create(const char *name, uma_ctor ctor, uma_dtor dtor,
    uma_init zinit, uma_fini zfini, uma_zone_t primary)
{
	struct uma_zctor_args args;
	uma_keg_t keg;
	uma_zone_t res;

	keg = primary->uz_keg;
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

	sx_xlock(&uma_reclaim_lock);
	res = zone_alloc_item(zones, &args, UMA_ANYDOMAIN, M_WAITOK);
	sx_xunlock(&uma_reclaim_lock);

	return (res);
}

/* See uma.h */
uma_zone_t
uma_zcache_create(const char *name, int size, uma_ctor ctor, uma_dtor dtor,
    uma_init zinit, uma_fini zfini, uma_import zimport, uma_release zrelease,
    void *arg, int flags)
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

	/*
	 * Large slabs are expensive to reclaim, so don't bother doing
	 * unnecessary work if we're shutting down.
	 */
	if (booted == BOOT_SHUTDOWN &&
	    zone->uz_fini == NULL && zone->uz_release == zone_release)
		return;
	sx_xlock(&uma_reclaim_lock);
	zone_free_item(zones, zone, NULL, SKIP_NONE);
	sx_xunlock(&uma_reclaim_lock);
}

void
uma_zwait(uma_zone_t zone)
{

	if ((zone->uz_flags & UMA_ZONE_SMR) != 0)
		uma_zfree_smr(zone, uma_zalloc_smr(zone, M_WAITOK));
	else if ((zone->uz_flags & UMA_ZONE_PCPU) != 0)
		uma_zfree_pcpu(zone, uma_zalloc_pcpu(zone, M_WAITOK));
	else
		uma_zfree(zone, uma_zalloc(zone, M_WAITOK));
}

void *
uma_zalloc_pcpu_arg(uma_zone_t zone, void *udata, int flags)
{
	void *item, *pcpu_item;
#ifdef SMP
	int i;

	MPASS(zone->uz_flags & UMA_ZONE_PCPU);
#endif
	item = uma_zalloc_arg(zone, udata, flags & ~M_ZERO);
	if (item == NULL)
		return (NULL);
	pcpu_item = zpcpu_base_to_offset(item);
	if (flags & M_ZERO) {
#ifdef SMP
		for (i = 0; i <= mp_maxid; i++)
			bzero(zpcpu_get_cpu(pcpu_item, i), zone->uz_size);
#else
		bzero(item, zone->uz_size);
#endif
	}
	return (pcpu_item);
}

/*
 * A stub while both regular and pcpu cases are identical.
 */
void
uma_zfree_pcpu_arg(uma_zone_t zone, void *pcpu_item, void *udata)
{
	void *item;

#ifdef SMP
	MPASS(zone->uz_flags & UMA_ZONE_PCPU);
#endif

        /* uma_zfree_pcu_*(..., NULL) does nothing, to match free(9). */
        if (pcpu_item == NULL)
                return;

	item = zpcpu_offset_to_base(pcpu_item);
	uma_zfree_arg(zone, item, udata);
}

static inline void *
item_ctor(uma_zone_t zone, int uz_flags, int size, void *udata, int flags,
    void *item)
{
#ifdef INVARIANTS
	bool skipdbg;
#endif

	kasan_mark_item_valid(zone, item);
	kmsan_mark_item_uninitialized(zone, item);

#ifdef INVARIANTS
	skipdbg = uma_dbg_zskip(zone, item);
	if (!skipdbg && (uz_flags & UMA_ZFLAG_TRASH) != 0 &&
	    zone->uz_ctor != trash_ctor)
		trash_ctor(item, size, udata, flags);
#endif

	/* Check flags before loading ctor pointer. */
	if (__predict_false((uz_flags & UMA_ZFLAG_CTORDTOR) != 0) &&
	    __predict_false(zone->uz_ctor != NULL) &&
	    zone->uz_ctor(item, size, udata, flags) != 0) {
		counter_u64_add(zone->uz_fails, 1);
		zone_free_item(zone, item, udata, SKIP_DTOR | SKIP_CNT);
		return (NULL);
	}
#ifdef INVARIANTS
	if (!skipdbg)
		uma_dbg_alloc(zone, NULL, item);
#endif
	if (__predict_false(flags & M_ZERO))
		return (memset(item, 0, size));

	return (item);
}

static inline void
item_dtor(uma_zone_t zone, void *item, int size, void *udata,
    enum zfreeskip skip)
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
	if (__predict_true(skip < SKIP_DTOR)) {
		if (zone->uz_dtor != NULL)
			zone->uz_dtor(item, size, udata);
#ifdef INVARIANTS
		if (!skipdbg && (zone->uz_flags & UMA_ZFLAG_TRASH) != 0 &&
		    zone->uz_dtor != trash_dtor)
			trash_dtor(item, size, udata);
#endif
	}
	kasan_mark_item_invalid(zone, item);
}

#ifdef NUMA
static int
item_domain(void *item)
{
	int domain;

	domain = vm_phys_domain(vtophys(item));
	KASSERT(domain >= 0 && domain < vm_ndomains,
	    ("%s: unknown domain for item %p", __func__, item));
	return (domain);
}
#endif

#if defined(INVARIANTS) || defined(DEBUG_MEMGUARD) || defined(WITNESS)
#if defined(INVARIANTS) && (defined(DDB) || defined(STACK))
#include <sys/stack.h>
#endif
#define	UMA_ZALLOC_DEBUG
static int
uma_zalloc_debug(uma_zone_t zone, void **itemp, void *udata, int flags)
{
	int error;

	error = 0;
#ifdef WITNESS
	if (flags & M_WAITOK) {
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "uma_zalloc_debug: zone \"%s\"", zone->uz_name);
	}
#endif

#ifdef INVARIANTS
	KASSERT((flags & M_EXEC) == 0,
	    ("uma_zalloc_debug: called with M_EXEC"));
	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("uma_zalloc_debug: called within spinlock or critical section"));
	KASSERT((zone->uz_flags & UMA_ZONE_PCPU) == 0 || (flags & M_ZERO) == 0,
	    ("uma_zalloc_debug: allocating from a pcpu zone with M_ZERO"));

	_Static_assert(M_NOWAIT != 0 && M_WAITOK != 0,
	    "M_NOWAIT and M_WAITOK must be non-zero for this assertion:");
#if 0
	/*
	 * Give the #elif clause time to find problems, then remove it
	 * and enable this.  (Remove <sys/stack.h> above, too.)
	 */
	KASSERT((flags & (M_NOWAIT|M_WAITOK)) == M_NOWAIT ||
	    (flags & (M_NOWAIT|M_WAITOK)) == M_WAITOK,
	    ("uma_zalloc_debug: must pass one of M_NOWAIT or M_WAITOK"));
#elif defined(DDB) || defined(STACK)
	if (__predict_false((flags & (M_NOWAIT|M_WAITOK)) != M_NOWAIT &&
	    (flags & (M_NOWAIT|M_WAITOK)) != M_WAITOK)) {
		static int stack_count;
		struct stack st;

		if (stack_count < 10) {
			++stack_count;
			printf("uma_zalloc* called with bad WAIT flags:\n");
			stack_save(&st);
			stack_print(&st);
		}
	}
#endif
#endif

#ifdef DEBUG_MEMGUARD
	if ((zone->uz_flags & (UMA_ZONE_SMR | UMA_ZFLAG_CACHE)) == 0 &&
	    memguard_cmp_zone(zone)) {
		void *item;
		item = memguard_alloc(zone->uz_size, flags);
		if (item != NULL) {
			error = EJUSTRETURN;
			if (zone->uz_init != NULL &&
			    zone->uz_init(item, zone->uz_size, flags) != 0) {
				*itemp = NULL;
				return (error);
			}
			if (zone->uz_ctor != NULL &&
			    zone->uz_ctor(item, zone->uz_size, udata,
			    flags) != 0) {
				counter_u64_add(zone->uz_fails, 1);
				if (zone->uz_fini != NULL)
					zone->uz_fini(item, zone->uz_size);
				*itemp = NULL;
				return (error);
			}
			*itemp = item;
			return (error);
		}
		/* This is unfortunate but should not be fatal. */
	}
#endif
	return (error);
}

static int
uma_zfree_debug(uma_zone_t zone, void *item, void *udata)
{
	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("uma_zfree_debug: called with spinlock or critical section held"));

#ifdef DEBUG_MEMGUARD
	if ((zone->uz_flags & (UMA_ZONE_SMR | UMA_ZFLAG_CACHE)) == 0 &&
	    is_memguard_addr(item)) {
		if (zone->uz_dtor != NULL)
			zone->uz_dtor(item, zone->uz_size, udata);
		if (zone->uz_fini != NULL)
			zone->uz_fini(item, zone->uz_size);
		memguard_free(item);
		return (EJUSTRETURN);
	}
#endif
	return (0);
}
#endif

static inline void *
cache_alloc_item(uma_zone_t zone, uma_cache_t cache, uma_cache_bucket_t bucket,
    void *udata, int flags)
{
	void *item;
	int size, uz_flags;

	item = cache_bucket_pop(cache, bucket);
	size = cache_uz_size(cache);
	uz_flags = cache_uz_flags(cache);
	critical_exit();
	return (item_ctor(zone, uz_flags, size, udata, flags, item));
}

static __noinline void *
cache_alloc_retry(uma_zone_t zone, uma_cache_t cache, void *udata, int flags)
{
	uma_cache_bucket_t bucket;
	int domain;

	while (cache_alloc(zone, cache, udata, flags)) {
		cache = &zone->uz_cpu[curcpu];
		bucket = &cache->uc_allocbucket;
		if (__predict_false(bucket->ucb_cnt == 0))
			continue;
		return (cache_alloc_item(zone, cache, bucket, udata, flags));
	}
	critical_exit();

	/*
	 * We can not get a bucket so try to return a single item.
	 */
	if (zone->uz_flags & UMA_ZONE_FIRSTTOUCH)
		domain = PCPU_GET(domain);
	else
		domain = UMA_ANYDOMAIN;
	return (zone_alloc_item(zone, udata, domain, flags));
}

/* See uma.h */
void *
uma_zalloc_smr(uma_zone_t zone, int flags)
{
	uma_cache_bucket_t bucket;
	uma_cache_t cache;

	CTR3(KTR_UMA, "uma_zalloc_smr zone %s(%p) flags %d", zone->uz_name,
	    zone, flags);

#ifdef UMA_ZALLOC_DEBUG
	void *item;

	KASSERT((zone->uz_flags & UMA_ZONE_SMR) != 0,
	    ("uma_zalloc_arg: called with non-SMR zone."));
	if (uma_zalloc_debug(zone, &item, NULL, flags) == EJUSTRETURN)
		return (item);
#endif

	critical_enter();
	cache = &zone->uz_cpu[curcpu];
	bucket = &cache->uc_allocbucket;
	if (__predict_false(bucket->ucb_cnt == 0))
		return (cache_alloc_retry(zone, cache, NULL, flags));
	return (cache_alloc_item(zone, cache, bucket, NULL, flags));
}

/* See uma.h */
void *
uma_zalloc_arg(uma_zone_t zone, void *udata, int flags)
{
	uma_cache_bucket_t bucket;
	uma_cache_t cache;

	/* Enable entropy collection for RANDOM_ENABLE_UMA kernel option */
	random_harvest_fast_uma(&zone, sizeof(zone), RANDOM_UMA);

	/* This is the fast path allocation */
	CTR3(KTR_UMA, "uma_zalloc_arg zone %s(%p) flags %d", zone->uz_name,
	    zone, flags);

#ifdef UMA_ZALLOC_DEBUG
	void *item;

	KASSERT((zone->uz_flags & UMA_ZONE_SMR) == 0,
	    ("uma_zalloc_arg: called with SMR zone."));
	if (uma_zalloc_debug(zone, &item, udata, flags) == EJUSTRETURN)
		return (item);
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
	cache = &zone->uz_cpu[curcpu];
	bucket = &cache->uc_allocbucket;
	if (__predict_false(bucket->ucb_cnt == 0))
		return (cache_alloc_retry(zone, cache, udata, flags));
	return (cache_alloc_item(zone, cache, bucket, udata, flags));
}

/*
 * Replenish an alloc bucket and possibly restore an old one.  Called in
 * a critical section.  Returns in a critical section.
 *
 * A false return value indicates an allocation failure.
 * A true return value indicates success and the caller should retry.
 */
static __noinline bool
cache_alloc(uma_zone_t zone, uma_cache_t cache, void *udata, int flags)
{
	uma_bucket_t bucket;
	int curdomain, domain;
	bool new;

	CRITICAL_ASSERT(curthread);

	/*
	 * If we have run out of items in our alloc bucket see
	 * if we can switch with the free bucket.
	 *
	 * SMR Zones can't re-use the free bucket until the sequence has
	 * expired.
	 */
	if ((cache_uz_flags(cache) & UMA_ZONE_SMR) == 0 &&
	    cache->uc_freebucket.ucb_cnt != 0) {
		cache_bucket_swap(&cache->uc_freebucket,
		    &cache->uc_allocbucket);
		return (true);
	}

	/*
	 * Discard any empty allocation bucket while we hold no locks.
	 */
	bucket = cache_bucket_unload_alloc(cache);
	critical_exit();

	if (bucket != NULL) {
		KASSERT(bucket->ub_cnt == 0,
		    ("cache_alloc: Entered with non-empty alloc bucket."));
		bucket_free(zone, bucket, udata);
	}

	/*
	 * Attempt to retrieve the item from the per-CPU cache has failed, so
	 * we must go back to the zone.  This requires the zdom lock, so we
	 * must drop the critical section, then re-acquire it when we go back
	 * to the cache.  Since the critical section is released, we may be
	 * preempted or migrate.  As such, make sure not to maintain any
	 * thread-local state specific to the cache from prior to releasing
	 * the critical section.
	 */
	domain = PCPU_GET(domain);
	if ((cache_uz_flags(cache) & UMA_ZONE_ROUNDROBIN) != 0 ||
	    VM_DOMAIN_EMPTY(domain))
		domain = zone_domain_highest(zone, domain);
	bucket = cache_fetch_bucket(zone, cache, domain);
	if (bucket == NULL && zone->uz_bucket_size != 0 && !bucketdisable) {
		bucket = zone_alloc_bucket(zone, udata, domain, flags);
		new = true;
	} else {
		new = false;
	}

	CTR3(KTR_UMA, "uma_zalloc: zone %s(%p) bucket zone returned %p",
	    zone->uz_name, zone, bucket);
	if (bucket == NULL) {
		critical_enter();
		return (false);
	}

	/*
	 * See if we lost the race or were migrated.  Cache the
	 * initialized bucket to make this less likely or claim
	 * the memory directly.
	 */
	critical_enter();
	cache = &zone->uz_cpu[curcpu];
	if (cache->uc_allocbucket.ucb_bucket == NULL &&
	    ((cache_uz_flags(cache) & UMA_ZONE_FIRSTTOUCH) == 0 ||
	    (curdomain = PCPU_GET(domain)) == domain ||
	    VM_DOMAIN_EMPTY(curdomain))) {
		if (new)
			atomic_add_long(&ZDOM_GET(zone, domain)->uzd_imax,
			    bucket->ub_cnt);
		cache_bucket_load_alloc(cache, bucket);
		return (true);
	}

	/*
	 * We lost the race, release this bucket and start over.
	 */
	critical_exit();
	zone_put_bucket(zone, domain, bucket, udata, !new);
	critical_enter();

	return (true);
}

void *
uma_zalloc_domain(uma_zone_t zone, void *udata, int domain, int flags)
{
#ifdef NUMA
	uma_bucket_t bucket;
	uma_zone_domain_t zdom;
	void *item;
#endif

	/* Enable entropy collection for RANDOM_ENABLE_UMA kernel option */
	random_harvest_fast_uma(&zone, sizeof(zone), RANDOM_UMA);

	/* This is the fast path allocation */
	CTR4(KTR_UMA, "uma_zalloc_domain zone %s(%p) domain %d flags %d",
	    zone->uz_name, zone, domain, flags);

	KASSERT((zone->uz_flags & UMA_ZONE_SMR) == 0,
	    ("uma_zalloc_domain: called with SMR zone."));
#ifdef NUMA
	KASSERT((zone->uz_flags & UMA_ZONE_FIRSTTOUCH) != 0,
	    ("uma_zalloc_domain: called with non-FIRSTTOUCH zone."));

	if (vm_ndomains == 1)
		return (uma_zalloc_arg(zone, udata, flags));

#ifdef UMA_ZALLOC_DEBUG
	if (uma_zalloc_debug(zone, &item, udata, flags) == EJUSTRETURN)
		return (item);
#endif

	/*
	 * Try to allocate from the bucket cache before falling back to the keg.
	 * We could try harder and attempt to allocate from per-CPU caches or
	 * the per-domain cross-domain buckets, but the complexity is probably
	 * not worth it.  It is more important that frees of previous
	 * cross-domain allocations do not blow up the cache.
	 */
	zdom = zone_domain_lock(zone, domain);
	if ((bucket = zone_fetch_bucket(zone, zdom, false)) != NULL) {
		item = bucket->ub_bucket[bucket->ub_cnt - 1];
#ifdef INVARIANTS
		bucket->ub_bucket[bucket->ub_cnt - 1] = NULL;
#endif
		bucket->ub_cnt--;
		zone_put_bucket(zone, domain, bucket, udata, true);
		item = item_ctor(zone, zone->uz_flags, zone->uz_size, udata,
		    flags, item);
		if (item != NULL) {
			KASSERT(item_domain(item) == domain,
			    ("%s: bucket cache item %p from wrong domain",
			    __func__, item));
			counter_u64_add(zone->uz_allocs, 1);
		}
		return (item);
	}
	ZDOM_UNLOCK(zdom);
	return (zone_alloc_item(zone, udata, domain, flags));
#else
	return (uma_zalloc_arg(zone, udata, flags));
#endif
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
	KEG_LOCK_ASSERT(keg, domain);

	slab = NULL;
	start = domain;
	do {
		dom = &keg->uk_domain[domain];
		if ((slab = LIST_FIRST(&dom->ud_part_slab)) != NULL)
			return (slab);
		if ((slab = LIST_FIRST(&dom->ud_free_slab)) != NULL) {
			LIST_REMOVE(slab, us_link);
			dom->ud_free_slabs--;
			LIST_INSERT_HEAD(&dom->ud_part_slab, slab, us_link);
			return (slab);
		}
		if (rr)
			domain = (domain + 1) % vm_ndomains;
	} while (domain != start);

	return (NULL);
}

/*
 * Fetch an existing slab from a free or partial list.  Returns with the
 * keg domain lock held if a slab was found or unlocked if not.
 */
static uma_slab_t
keg_fetch_free_slab(uma_keg_t keg, int domain, bool rr, int flags)
{
	uma_slab_t slab;
	uint32_t reserve;

	/* HASH has a single free list. */
	if ((keg->uk_flags & UMA_ZFLAG_HASH) != 0)
		domain = 0;

	KEG_LOCK(keg, domain);
	reserve = (flags & M_USE_RESERVE) != 0 ? 0 : keg->uk_reserve;
	if (keg->uk_domain[domain].ud_free_items <= reserve ||
	    (slab = keg_first_slab(keg, domain, rr)) == NULL) {
		KEG_UNLOCK(keg, domain);
		return (NULL);
	}
	return (slab);
}

static uma_slab_t
keg_fetch_slab(uma_keg_t keg, uma_zone_t zone, int rdomain, const int flags)
{
	struct vm_domainset_iter di;
	uma_slab_t slab;
	int aflags, domain;
	bool rr;

	KASSERT((flags & (M_WAITOK | M_NOVM)) != (M_WAITOK | M_NOVM),
	    ("%s: invalid flags %#x", __func__, flags));

restart:
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
		 * M_NOVM is used to break the recursion that can otherwise
		 * occur if low-level memory management routines use UMA.
		 */
		if ((flags & M_NOVM) == 0) {
			slab = keg_alloc_slab(keg, zone, domain, flags, aflags);
			if (slab != NULL)
				return (slab);
		}

		if (!rr) {
			if ((flags & M_USE_RESERVE) != 0) {
				/*
				 * Drain reserves from other domains before
				 * giving up or sleeping.  It may be useful to
				 * support per-domain reserves eventually.
				 */
				rdomain = UMA_ANYDOMAIN;
				goto restart;
			}
			if ((flags & M_WAITOK) == 0)
				break;
			vm_wait_domain(domain);
		} else if (vm_domainset_iter_policy(&di, &domain) != 0) {
			if ((flags & M_WAITOK) != 0) {
				vm_wait_doms(&keg->uk_dr.dr_policy->ds_mask, 0);
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
	if ((slab = keg_fetch_free_slab(keg, domain, rr, flags)) != NULL)
		return (slab);

	return (NULL);
}

static void *
slab_alloc_item(uma_keg_t keg, uma_slab_t slab)
{
	uma_domain_t dom;
	void *item;
	int freei;

	KEG_LOCK_ASSERT(keg, slab->us_domain);

	dom = &keg->uk_domain[slab->us_domain];
	freei = BIT_FFS(keg->uk_ipers, &slab->us_free) - 1;
	BIT_CLR(keg->uk_ipers, freei, &slab->us_free);
	item = slab_item(slab, keg, freei);
	slab->us_freecount--;
	dom->ud_free_items--;

	/*
	 * Move this slab to the full list.  It must be on the partial list, so
	 * we do not need to update the free slab count.  In particular,
	 * keg_fetch_slab() always returns slabs on the partial list.
	 */
	if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&dom->ud_full_slab, slab, us_link);
	}

	return (item);
}

static int
zone_import(void *arg, void **bucket, int max, int domain, int flags)
{
	uma_domain_t dom;
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
	/* Try to keep the buckets totally full */
	for (i = 0; i < max; ) {
		if ((slab = keg_fetch_slab(keg, zone, domain, flags)) == NULL)
			break;
#ifdef NUMA
		stripe = howmany(max, vm_ndomains);
#endif
		dom = &keg->uk_domain[slab->us_domain];
		do {
			bucket[i++] = slab_alloc_item(keg, slab);
			if (keg->uk_reserve > 0 &&
			    dom->ud_free_items <= keg->uk_reserve) {
				/*
				 * Avoid depleting the reserve after a
				 * successful item allocation, even if
				 * M_USE_RESERVE is specified.
				 */
				KEG_UNLOCK(keg, slab->us_domain);
				goto out;
			}
#ifdef NUMA
			/*
			 * If the zone is striped we pick a new slab for every
			 * N allocations.  Eliminating this conditional will
			 * instead pick a new domain for each bucket rather
			 * than stripe within each bucket.  The current option
			 * produces more fragmentation and requires more cpu
			 * time but yields better distribution.
			 */
			if ((zone->uz_flags & UMA_ZONE_ROUNDROBIN) != 0 &&
			    vm_ndomains > 1 && --stripe == 0)
				break;
#endif
		} while (slab->us_freecount != 0 && i < max);
		KEG_UNLOCK(keg, slab->us_domain);

		/* Don't block if we allocated any successfully. */
		flags &= ~M_WAITOK;
		flags |= M_NOWAIT;
	}
out:
	return i;
}

static int
zone_alloc_limit_hard(uma_zone_t zone, int count, int flags)
{
	uint64_t old, new, total, max;

	/*
	 * The hard case.  We're going to sleep because there were existing
	 * sleepers or because we ran out of items.  This routine enforces
	 * fairness by keeping fifo order.
	 *
	 * First release our ill gotten gains and make some noise.
	 */
	for (;;) {
		zone_free_limit(zone, count);
		zone_log_warning(zone);
		zone_maxaction(zone);
		if (flags & M_NOWAIT)
			return (0);

		/*
		 * We need to allocate an item or set ourself as a sleeper
		 * while the sleepq lock is held to avoid wakeup races.  This
		 * is essentially a home rolled semaphore.
		 */
		sleepq_lock(&zone->uz_max_items);
		old = zone->uz_items;
		do {
			MPASS(UZ_ITEMS_SLEEPERS(old) < UZ_ITEMS_SLEEPERS_MAX);
			/* Cache the max since we will evaluate twice. */
			max = zone->uz_max_items;
			if (UZ_ITEMS_SLEEPERS(old) != 0 ||
			    UZ_ITEMS_COUNT(old) >= max)
				new = old + UZ_ITEMS_SLEEPER;
			else
				new = old + MIN(count, max - old);
		} while (atomic_fcmpset_64(&zone->uz_items, &old, new) == 0);

		/* We may have successfully allocated under the sleepq lock. */
		if (UZ_ITEMS_SLEEPERS(new) == 0) {
			sleepq_release(&zone->uz_max_items);
			return (new - old);
		}

		/*
		 * This is in a different cacheline from uz_items so that we
		 * don't constantly invalidate the fastpath cacheline when we
		 * adjust item counts.  This could be limited to toggling on
		 * transitions.
		 */
		atomic_add_32(&zone->uz_sleepers, 1);
		atomic_add_64(&zone->uz_sleeps, 1);

		/*
		 * We have added ourselves as a sleeper.  The sleepq lock
		 * protects us from wakeup races.  Sleep now and then retry.
		 */
		sleepq_add(&zone->uz_max_items, NULL, "zonelimit", 0, 0);
		sleepq_wait(&zone->uz_max_items, PVM);

		/*
		 * After wakeup, remove ourselves as a sleeper and try
		 * again.  We no longer have the sleepq lock for protection.
		 *
		 * Subract ourselves as a sleeper while attempting to add
		 * our count.
		 */
		atomic_subtract_32(&zone->uz_sleepers, 1);
		old = atomic_fetchadd_64(&zone->uz_items,
		    -(UZ_ITEMS_SLEEPER - count));
		/* We're no longer a sleeper. */
		old -= UZ_ITEMS_SLEEPER;

		/*
		 * If we're still at the limit, restart.  Notably do not
		 * block on other sleepers.  Cache the max value to protect
		 * against changes via sysctl.
		 */
		total = UZ_ITEMS_COUNT(old);
		max = zone->uz_max_items;
		if (total >= max)
			continue;
		/* Truncate if necessary, otherwise wake other sleepers. */
		if (total + count > max) {
			zone_free_limit(zone, total + count - max);
			count = max - total;
		} else if (total + count < max && UZ_ITEMS_SLEEPERS(old) != 0)
			wakeup_one(&zone->uz_max_items);

		return (count);
	}
}

/*
 * Allocate 'count' items from our max_items limit.  Returns the number
 * available.  If M_NOWAIT is not specified it will sleep until at least
 * one item can be allocated.
 */
static int
zone_alloc_limit(uma_zone_t zone, int count, int flags)
{
	uint64_t old;
	uint64_t max;

	max = zone->uz_max_items;
	MPASS(max > 0);

	/*
	 * We expect normal allocations to succeed with a simple
	 * fetchadd.
	 */
	old = atomic_fetchadd_64(&zone->uz_items, count);
	if (__predict_true(old + count <= max))
		return (count);

	/*
	 * If we had some items and no sleepers just return the
	 * truncated value.  We have to release the excess space
	 * though because that may wake sleepers who weren't woken
	 * because we were temporarily over the limit.
	 */
	if (old < max) {
		zone_free_limit(zone, (old + count) - max);
		return (max - old);
	}
	return (zone_alloc_limit_hard(zone, count, flags));
}

/*
 * Free a number of items back to the limit.
 */
static void
zone_free_limit(uma_zone_t zone, int count)
{
	uint64_t old;

	MPASS(count > 0);

	/*
	 * In the common case we either have no sleepers or
	 * are still over the limit and can just return.
	 */
	old = atomic_fetchadd_64(&zone->uz_items, -count);
	if (__predict_true(UZ_ITEMS_SLEEPERS(old) == 0 ||
	   UZ_ITEMS_COUNT(old) - count >= zone->uz_max_items))
		return;

	/*
	 * Moderate the rate of wakeups.  Sleepers will continue
	 * to generate wakeups if necessary.
	 */
	wakeup_one(&zone->uz_max_items);
}

static uma_bucket_t
zone_alloc_bucket(uma_zone_t zone, void *udata, int domain, int flags)
{
	uma_bucket_t bucket;
	int error, maxbucket, cnt;

	CTR3(KTR_UMA, "zone_alloc_bucket zone %s(%p) domain %d", zone->uz_name,
	    zone, domain);

	/* Avoid allocs targeting empty domains. */
	if (domain != UMA_ANYDOMAIN && VM_DOMAIN_EMPTY(domain))
		domain = UMA_ANYDOMAIN;
	else if ((zone->uz_flags & UMA_ZONE_ROUNDROBIN) != 0)
		domain = UMA_ANYDOMAIN;

	if (zone->uz_max_items > 0)
		maxbucket = zone_alloc_limit(zone, zone->uz_bucket_size,
		    M_NOWAIT);
	else
		maxbucket = zone->uz_bucket_size;
	if (maxbucket == 0)
		return (NULL);

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

		for (i = 0; i < bucket->ub_cnt; i++) {
			kasan_mark_item_valid(zone, bucket->ub_bucket[i]);
			error = zone->uz_init(bucket->ub_bucket[i],
			    zone->uz_size, flags);
			kasan_mark_item_invalid(zone, bucket->ub_bucket[i]);
			if (error != 0)
				break;
		}

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
	if (zone->uz_max_items > 0 && cnt < maxbucket)
		zone_free_limit(zone, maxbucket - cnt);

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
	void *item;

	if (zone->uz_max_items > 0 && zone_alloc_limit(zone, 1, flags) == 0) {
		counter_u64_add(zone->uz_fails, 1);
		return (NULL);
	}

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
		int error;

		kasan_mark_item_valid(zone, item);
		error = zone->uz_init(item, zone->uz_size, flags);
		kasan_mark_item_invalid(zone, item);
		if (error != 0) {
			zone_free_item(zone, item, udata, SKIP_FINI | SKIP_CNT);
			goto fail_cnt;
		}
	}
	item = item_ctor(zone, zone->uz_flags, zone->uz_size, udata, flags,
	    item);
	if (item == NULL)
		goto fail;

	counter_u64_add(zone->uz_allocs, 1);
	CTR3(KTR_UMA, "zone_alloc_item item %p from %s(%p)", item,
	    zone->uz_name, zone);

	return (item);

fail_cnt:
	counter_u64_add(zone->uz_fails, 1);
fail:
	if (zone->uz_max_items > 0)
		zone_free_limit(zone, 1);
	CTR2(KTR_UMA, "zone_alloc_item failed from %s(%p)",
	    zone->uz_name, zone);

	return (NULL);
}

/* See uma.h */
void
uma_zfree_smr(uma_zone_t zone, void *item)
{
	uma_cache_t cache;
	uma_cache_bucket_t bucket;
	int itemdomain;
#ifdef NUMA
	int uz_flags;
#endif

	CTR3(KTR_UMA, "uma_zfree_smr zone %s(%p) item %p",
	    zone->uz_name, zone, item);

#ifdef UMA_ZALLOC_DEBUG
	KASSERT((zone->uz_flags & UMA_ZONE_SMR) != 0,
	    ("uma_zfree_smr: called with non-SMR zone."));
	KASSERT(item != NULL, ("uma_zfree_smr: Called with NULL pointer."));
	SMR_ASSERT_NOT_ENTERED(zone->uz_smr);
	if (uma_zfree_debug(zone, item, NULL) == EJUSTRETURN)
		return;
#endif
	cache = &zone->uz_cpu[curcpu];
	itemdomain = 0;
#ifdef NUMA
	uz_flags = cache_uz_flags(cache);
	if ((uz_flags & UMA_ZONE_FIRSTTOUCH) != 0)
		itemdomain = item_domain(item);
#endif
	critical_enter();
	do {
		cache = &zone->uz_cpu[curcpu];
		/* SMR Zones must free to the free bucket. */
		bucket = &cache->uc_freebucket;
#ifdef NUMA
		if ((uz_flags & UMA_ZONE_FIRSTTOUCH) != 0 &&
		    PCPU_GET(domain) != itemdomain) {
			bucket = &cache->uc_crossbucket;
		}
#endif
		if (__predict_true(bucket->ucb_cnt < bucket->ucb_entries)) {
			cache_bucket_push(cache, bucket, item);
			critical_exit();
			return;
		}
	} while (cache_free(zone, cache, NULL, itemdomain));
	critical_exit();

	/*
	 * If nothing else caught this, we'll just do an internal free.
	 */
	zone_free_item(zone, item, NULL, SKIP_NONE);
}

/* See uma.h */
void
uma_zfree_arg(uma_zone_t zone, void *item, void *udata)
{
	uma_cache_t cache;
	uma_cache_bucket_t bucket;
	int itemdomain, uz_flags;

	/* Enable entropy collection for RANDOM_ENABLE_UMA kernel option */
	random_harvest_fast_uma(&zone, sizeof(zone), RANDOM_UMA);

	CTR3(KTR_UMA, "uma_zfree_arg zone %s(%p) item %p",
	    zone->uz_name, zone, item);

#ifdef UMA_ZALLOC_DEBUG
	KASSERT((zone->uz_flags & UMA_ZONE_SMR) == 0,
	    ("uma_zfree_arg: called with SMR zone."));
	if (uma_zfree_debug(zone, item, udata) == EJUSTRETURN)
		return;
#endif
        /* uma_zfree(..., NULL) does nothing, to match free(9). */
        if (item == NULL)
                return;

	/*
	 * We are accessing the per-cpu cache without a critical section to
	 * fetch size and flags.  This is acceptable, if we are preempted we
	 * will simply read another cpu's line.
	 */
	cache = &zone->uz_cpu[curcpu];
	uz_flags = cache_uz_flags(cache);
	if (UMA_ALWAYS_CTORDTOR ||
	    __predict_false((uz_flags & UMA_ZFLAG_CTORDTOR) != 0))
		item_dtor(zone, item, cache_uz_size(cache), udata, SKIP_NONE);

	/*
	 * The race here is acceptable.  If we miss it we'll just have to wait
	 * a little longer for the limits to be reset.
	 */
	if (__predict_false(uz_flags & UMA_ZFLAG_LIMIT)) {
		if (atomic_load_32(&zone->uz_sleepers) > 0)
			goto zfree_item;
	}

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
	itemdomain = 0;
#ifdef NUMA
	if ((uz_flags & UMA_ZONE_FIRSTTOUCH) != 0)
		itemdomain = item_domain(item);
#endif
	critical_enter();
	do {
		cache = &zone->uz_cpu[curcpu];
		/*
		 * Try to free into the allocbucket first to give LIFO
		 * ordering for cache-hot datastructures.  Spill over
		 * into the freebucket if necessary.  Alloc will swap
		 * them if one runs dry.
		 */
		bucket = &cache->uc_allocbucket;
#ifdef NUMA
		if ((uz_flags & UMA_ZONE_FIRSTTOUCH) != 0 &&
		    PCPU_GET(domain) != itemdomain) {
			bucket = &cache->uc_crossbucket;
		} else
#endif
		if (bucket->ucb_cnt == bucket->ucb_entries &&
		   cache->uc_freebucket.ucb_cnt <
		   cache->uc_freebucket.ucb_entries)
			cache_bucket_swap(&cache->uc_freebucket,
			    &cache->uc_allocbucket);
		if (__predict_true(bucket->ucb_cnt < bucket->ucb_entries)) {
			cache_bucket_push(cache, bucket, item);
			critical_exit();
			return;
		}
	} while (cache_free(zone, cache, udata, itemdomain));
	critical_exit();

	/*
	 * If nothing else caught this, we'll just do an internal free.
	 */
zfree_item:
	zone_free_item(zone, item, udata, SKIP_DTOR);
}

#ifdef NUMA
/*
 * sort crossdomain free buckets to domain correct buckets and cache
 * them.
 */
static void
zone_free_cross(uma_zone_t zone, uma_bucket_t bucket, void *udata)
{
	struct uma_bucketlist emptybuckets, fullbuckets;
	uma_zone_domain_t zdom;
	uma_bucket_t b;
	smr_seq_t seq;
	void *item;
	int domain;

	CTR3(KTR_UMA,
	    "uma_zfree: zone %s(%p) draining cross bucket %p",
	    zone->uz_name, zone, bucket);

	/*
	 * It is possible for buckets to arrive here out of order so we fetch
	 * the current smr seq rather than accepting the bucket's.
	 */
	seq = SMR_SEQ_INVALID;
	if ((zone->uz_flags & UMA_ZONE_SMR) != 0)
		seq = smr_advance(zone->uz_smr);

	/*
	 * To avoid having ndomain * ndomain buckets for sorting we have a
	 * lock on the current crossfree bucket.  A full matrix with
	 * per-domain locking could be used if necessary.
	 */
	STAILQ_INIT(&emptybuckets);
	STAILQ_INIT(&fullbuckets);
	ZONE_CROSS_LOCK(zone);
	for (; bucket->ub_cnt > 0; bucket->ub_cnt--) {
		item = bucket->ub_bucket[bucket->ub_cnt - 1];
		domain = item_domain(item);
		zdom = ZDOM_GET(zone, domain);
		if (zdom->uzd_cross == NULL) {
			if ((b = STAILQ_FIRST(&emptybuckets)) != NULL) {
				STAILQ_REMOVE_HEAD(&emptybuckets, ub_link);
				zdom->uzd_cross = b;
			} else {
				/*
				 * Avoid allocating a bucket with the cross lock
				 * held, since allocation can trigger a
				 * cross-domain free and bucket zones may
				 * allocate from each other.
				 */
				ZONE_CROSS_UNLOCK(zone);
				b = bucket_alloc(zone, udata, M_NOWAIT);
				if (b == NULL)
					goto out;
				ZONE_CROSS_LOCK(zone);
				if (zdom->uzd_cross != NULL) {
					STAILQ_INSERT_HEAD(&emptybuckets, b,
					    ub_link);
				} else {
					zdom->uzd_cross = b;
				}
			}
		}
		b = zdom->uzd_cross;
		b->ub_bucket[b->ub_cnt++] = item;
		b->ub_seq = seq;
		if (b->ub_cnt == b->ub_entries) {
			STAILQ_INSERT_HEAD(&fullbuckets, b, ub_link);
			if ((b = STAILQ_FIRST(&emptybuckets)) != NULL)
				STAILQ_REMOVE_HEAD(&emptybuckets, ub_link);
			zdom->uzd_cross = b;
		}
	}
	ZONE_CROSS_UNLOCK(zone);
out:
	if (bucket->ub_cnt == 0)
		bucket->ub_seq = SMR_SEQ_INVALID;
	bucket_free(zone, bucket, udata);

	while ((b = STAILQ_FIRST(&emptybuckets)) != NULL) {
		STAILQ_REMOVE_HEAD(&emptybuckets, ub_link);
		bucket_free(zone, b, udata);
	}
	while ((b = STAILQ_FIRST(&fullbuckets)) != NULL) {
		STAILQ_REMOVE_HEAD(&fullbuckets, ub_link);
		domain = item_domain(b->ub_bucket[0]);
		zone_put_bucket(zone, domain, b, udata, true);
	}
}
#endif

static void
zone_free_bucket(uma_zone_t zone, uma_bucket_t bucket, void *udata,
    int itemdomain, bool ws)
{

#ifdef NUMA
	/*
	 * Buckets coming from the wrong domain will be entirely for the
	 * only other domain on two domain systems.  In this case we can
	 * simply cache them.  Otherwise we need to sort them back to
	 * correct domains.
	 */
	if ((zone->uz_flags & UMA_ZONE_FIRSTTOUCH) != 0 &&
	    vm_ndomains > 2 && PCPU_GET(domain) != itemdomain) {
		zone_free_cross(zone, bucket, udata);
		return;
	}
#endif

	/*
	 * Attempt to save the bucket in the zone's domain bucket cache.
	 */
	CTR3(KTR_UMA,
	    "uma_zfree: zone %s(%p) putting bucket %p on free list",
	    zone->uz_name, zone, bucket);
	/* ub_cnt is pointing to the last free item */
	if ((zone->uz_flags & UMA_ZONE_ROUNDROBIN) != 0)
		itemdomain = zone_domain_lowest(zone, itemdomain);
	zone_put_bucket(zone, itemdomain, bucket, udata, ws);
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
cache_free(uma_zone_t zone, uma_cache_t cache, void *udata, int itemdomain)
{
	uma_cache_bucket_t cbucket;
	uma_bucket_t newbucket, bucket;

	CRITICAL_ASSERT(curthread);

	if (zone->uz_bucket_size == 0)
		return false;

	cache = &zone->uz_cpu[curcpu];
	newbucket = NULL;

	/*
	 * FIRSTTOUCH domains need to free to the correct zdom.  When
	 * enabled this is the zdom of the item.   The bucket is the
	 * cross bucket if the current domain and itemdomain do not match.
	 */
	cbucket = &cache->uc_freebucket;
#ifdef NUMA
	if ((cache_uz_flags(cache) & UMA_ZONE_FIRSTTOUCH) != 0) {
		if (PCPU_GET(domain) != itemdomain) {
			cbucket = &cache->uc_crossbucket;
			if (cbucket->ucb_cnt != 0)
				counter_u64_add(zone->uz_xdomain,
				    cbucket->ucb_cnt);
		}
	}
#endif
	bucket = cache_bucket_unload(cbucket);
	KASSERT(bucket == NULL || bucket->ub_cnt == bucket->ub_entries,
	    ("cache_free: Entered with non-full free bucket."));

	/* We are no longer associated with this CPU. */
	critical_exit();

	/*
	 * Don't let SMR zones operate without a free bucket.  Force
	 * a synchronize and re-use this one.  We will only degrade
	 * to a synchronize every bucket_size items rather than every
	 * item if we fail to allocate a bucket.
	 */
	if ((zone->uz_flags & UMA_ZONE_SMR) != 0) {
		if (bucket != NULL)
			bucket->ub_seq = smr_advance(zone->uz_smr);
		newbucket = bucket_alloc(zone, udata, M_NOWAIT);
		if (newbucket == NULL && bucket != NULL) {
			bucket_drain(zone, bucket);
			newbucket = bucket;
			bucket = NULL;
		}
	} else if (!bucketdisable)
		newbucket = bucket_alloc(zone, udata, M_NOWAIT);

	if (bucket != NULL)
		zone_free_bucket(zone, bucket, udata, itemdomain, true);

	critical_enter();
	if ((bucket = newbucket) == NULL)
		return (false);
	cache = &zone->uz_cpu[curcpu];
#ifdef NUMA
	/*
	 * Check to see if we should be populating the cross bucket.  If it
	 * is already populated we will fall through and attempt to populate
	 * the free bucket.
	 */
	if ((cache_uz_flags(cache) & UMA_ZONE_FIRSTTOUCH) != 0) {
		if (PCPU_GET(domain) != itemdomain &&
		    cache->uc_crossbucket.ucb_bucket == NULL) {
			cache_bucket_load_cross(cache, bucket);
			return (true);
		}
	}
#endif
	/*
	 * We may have lost the race to fill the bucket or switched CPUs.
	 */
	if (cache->uc_freebucket.ucb_bucket != NULL) {
		critical_exit();
		bucket_free(zone, bucket, udata);
		critical_enter();
	} else
		cache_bucket_load_free(cache, bucket);

	return (true);
}

static void
slab_free_item(uma_zone_t zone, uma_slab_t slab, void *item)
{
	uma_keg_t keg;
	uma_domain_t dom;
	int freei;

	keg = zone->uz_keg;
	KEG_LOCK_ASSERT(keg, slab->us_domain);

	/* Do we need to remove from any lists? */
	dom = &keg->uk_domain[slab->us_domain];
	if (slab->us_freecount + 1 == keg->uk_ipers) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&dom->ud_free_slab, slab, us_link);
		dom->ud_free_slabs++;
	} else if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&dom->ud_part_slab, slab, us_link);
	}

	/* Slab management. */
	freei = slab_item_index(slab, keg, item);
	BIT_SET(keg->uk_ipers, freei, &slab->us_free);
	slab->us_freecount++;

	/* Keg statistics. */
	dom->ud_free_items++;
}

static void
zone_release(void *arg, void **bucket, int cnt)
{
	struct mtx *lock;
	uma_zone_t zone;
	uma_slab_t slab;
	uma_keg_t keg;
	uint8_t *mem;
	void *item;
	int i;

	zone = arg;
	keg = zone->uz_keg;
	lock = NULL;
	if (__predict_false((zone->uz_flags & UMA_ZFLAG_HASH) != 0))
		lock = KEG_LOCK(keg, 0);
	for (i = 0; i < cnt; i++) {
		item = bucket[i];
		if (__predict_true((zone->uz_flags & UMA_ZFLAG_VTOSLAB) != 0)) {
			slab = vtoslab((vm_offset_t)item);
		} else {
			mem = (uint8_t *)((uintptr_t)item & (~UMA_SLAB_MASK));
			if ((zone->uz_flags & UMA_ZFLAG_HASH) != 0)
				slab = hash_sfind(&keg->uk_hash, mem);
			else
				slab = (uma_slab_t)(mem + keg->uk_pgoff);
		}
		if (lock != KEG_LOCKPTR(keg, slab->us_domain)) {
			if (lock != NULL)
				mtx_unlock(lock);
			lock = KEG_LOCK(keg, slab->us_domain);
		}
		slab_free_item(zone, slab, item);
	}
	if (lock != NULL)
		mtx_unlock(lock);
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
static __noinline void
zone_free_item(uma_zone_t zone, void *item, void *udata, enum zfreeskip skip)
{

	/*
	 * If a free is sent directly to an SMR zone we have to
	 * synchronize immediately because the item can instantly
	 * be reallocated. This should only happen in degenerate
	 * cases when no memory is available for per-cpu caches.
	 */
	if ((zone->uz_flags & UMA_ZONE_SMR) != 0 && skip == SKIP_NONE)
		smr_synchronize(zone->uz_smr);

	item_dtor(zone, item, zone->uz_size, udata, skip);

	if (skip < SKIP_FINI && zone->uz_fini) {
		kasan_mark_item_valid(zone, item);
		zone->uz_fini(item, zone->uz_size);
		kasan_mark_item_invalid(zone, item);
	}

	zone->uz_release(zone->uz_arg, &item, 1);

	if (skip & SKIP_CNT)
		return;

	counter_u64_add(zone->uz_frees, 1);

	if (zone->uz_max_items > 0)
		zone_free_limit(zone, 1);
}

/* See uma.h */
int
uma_zone_set_max(uma_zone_t zone, int nitems)
{

	/*
	 * If the limit is small, we may need to constrain the maximum per-CPU
	 * cache size, or disable caching entirely.
	 */
	uma_zone_set_maxcache(zone, nitems);

	/*
	 * XXX This can misbehave if the zone has any allocations with
	 * no limit and a limit is imposed.  There is currently no
	 * way to clear a limit.
	 */
	ZONE_LOCK(zone);
	if (zone->uz_max_items == 0)
		ZONE_ASSERT_COLD(zone);
	zone->uz_max_items = nitems;
	zone->uz_flags |= UMA_ZFLAG_LIMIT;
	zone_update_caches(zone);
	/* We may need to wake waiters. */
	wakeup(&zone->uz_max_items);
	ZONE_UNLOCK(zone);

	return (nitems);
}

/* See uma.h */
void
uma_zone_set_maxcache(uma_zone_t zone, int nitems)
{
	int bpcpu, bpdom, bsize, nb;

	ZONE_LOCK(zone);

	/*
	 * Compute a lower bound on the number of items that may be cached in
	 * the zone.  Each CPU gets at least two buckets, and for cross-domain
	 * frees we use an additional bucket per CPU and per domain.  Select the
	 * largest bucket size that does not exceed half of the requested limit,
	 * with the left over space given to the full bucket cache.
	 */
	bpdom = 0;
	bpcpu = 2;
#ifdef NUMA
	if ((zone->uz_flags & UMA_ZONE_FIRSTTOUCH) != 0 && vm_ndomains > 1) {
		bpcpu++;
		bpdom++;
	}
#endif
	nb = bpcpu * mp_ncpus + bpdom * vm_ndomains;
	bsize = nitems / nb / 2;
	if (bsize > BUCKET_MAX)
		bsize = BUCKET_MAX;
	else if (bsize == 0 && nitems / nb > 0)
		bsize = 1;
	zone->uz_bucket_size_max = zone->uz_bucket_size = bsize;
	if (zone->uz_bucket_size_min > zone->uz_bucket_size_max)
		zone->uz_bucket_size_min = zone->uz_bucket_size_max;
	zone->uz_bucket_max = nitems - nb * bsize;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
int
uma_zone_get_max(uma_zone_t zone)
{
	int nitems;

	nitems = atomic_load_64(&zone->uz_max_items);

	return (nitems);
}

/* See uma.h */
void
uma_zone_set_warning(uma_zone_t zone, const char *warning)
{

	ZONE_ASSERT_COLD(zone);
	zone->uz_warning = warning;
}

/* See uma.h */
void
uma_zone_set_maxaction(uma_zone_t zone, uma_maxaction_t maxaction)
{

	ZONE_ASSERT_COLD(zone);
	TASK_INIT(&zone->uz_maxaction, 0, (task_fn_t *)maxaction, zone);
}

/* See uma.h */
int
uma_zone_get_cur(uma_zone_t zone)
{
	int64_t nitems;
	u_int i;

	nitems = 0;
	if (zone->uz_allocs != EARLY_COUNTER && zone->uz_frees != EARLY_COUNTER)
		nitems = counter_u64_fetch(zone->uz_allocs) -
		    counter_u64_fetch(zone->uz_frees);
	CPU_FOREACH(i)
		nitems += atomic_load_64(&zone->uz_cpu[i].uc_allocs) -
		    atomic_load_64(&zone->uz_cpu[i].uc_frees);

	return (nitems < 0 ? 0 : nitems);
}

static uint64_t
uma_zone_get_allocs(uma_zone_t zone)
{
	uint64_t nitems;
	u_int i;

	nitems = 0;
	if (zone->uz_allocs != EARLY_COUNTER)
		nitems = counter_u64_fetch(zone->uz_allocs);
	CPU_FOREACH(i)
		nitems += atomic_load_64(&zone->uz_cpu[i].uc_allocs);

	return (nitems);
}

static uint64_t
uma_zone_get_frees(uma_zone_t zone)
{
	uint64_t nitems;
	u_int i;

	nitems = 0;
	if (zone->uz_frees != EARLY_COUNTER)
		nitems = counter_u64_fetch(zone->uz_frees);
	CPU_FOREACH(i)
		nitems += atomic_load_64(&zone->uz_cpu[i].uc_frees);

	return (nitems);
}

#ifdef INVARIANTS
/* Used only for KEG_ASSERT_COLD(). */
static uint64_t
uma_keg_get_allocs(uma_keg_t keg)
{
	uma_zone_t z;
	uint64_t nitems;

	nitems = 0;
	LIST_FOREACH(z, &keg->uk_zones, uz_link)
		nitems += uma_zone_get_allocs(z);

	return (nitems);
}
#endif

/* See uma.h */
void
uma_zone_set_init(uma_zone_t zone, uma_init uminit)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_ASSERT_COLD(keg);
	keg->uk_init = uminit;
}

/* See uma.h */
void
uma_zone_set_fini(uma_zone_t zone, uma_fini fini)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_ASSERT_COLD(keg);
	keg->uk_fini = fini;
}

/* See uma.h */
void
uma_zone_set_zinit(uma_zone_t zone, uma_init zinit)
{

	ZONE_ASSERT_COLD(zone);
	zone->uz_init = zinit;
}

/* See uma.h */
void
uma_zone_set_zfini(uma_zone_t zone, uma_fini zfini)
{

	ZONE_ASSERT_COLD(zone);
	zone->uz_fini = zfini;
}

/* See uma.h */
void
uma_zone_set_freef(uma_zone_t zone, uma_free freef)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_ASSERT_COLD(keg);
	keg->uk_freef = freef;
}

/* See uma.h */
void
uma_zone_set_allocf(uma_zone_t zone, uma_alloc allocf)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_ASSERT_COLD(keg);
	keg->uk_allocf = allocf;
}

/* See uma.h */
void
uma_zone_set_smr(uma_zone_t zone, smr_t smr)
{

	ZONE_ASSERT_COLD(zone);

	KASSERT(smr != NULL, ("Got NULL smr"));
	KASSERT((zone->uz_flags & UMA_ZONE_SMR) == 0,
	    ("zone %p (%s) already uses SMR", zone, zone->uz_name));
	zone->uz_flags |= UMA_ZONE_SMR;
	zone->uz_smr = smr;
	zone_update_caches(zone);
}

smr_t
uma_zone_get_smr(uma_zone_t zone)
{

	return (zone->uz_smr);
}

/* See uma.h */
void
uma_zone_reserve(uma_zone_t zone, int items)
{
	uma_keg_t keg;

	KEG_GET(zone, keg);
	KEG_ASSERT_COLD(keg);
	keg->uk_reserve = items;
}

/* See uma.h */
int
uma_zone_reserve_kva(uma_zone_t zone, int count)
{
	uma_keg_t keg;
	vm_offset_t kva;
	u_int pages;

	KEG_GET(zone, keg);
	KEG_ASSERT_COLD(keg);
	ZONE_ASSERT_COLD(zone);

	pages = howmany(count, keg->uk_ipers) * keg->uk_ppera;

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

	MPASS(keg->uk_kva == 0);
	keg->uk_kva = kva;
	keg->uk_offset = 0;
	zone->uz_max_items = pages * keg->uk_ipers;
#ifdef UMA_MD_SMALL_ALLOC
	keg->uk_allocf = (keg->uk_ppera > 1) ? noobj_alloc : uma_small_alloc;
#else
	keg->uk_allocf = noobj_alloc;
#endif
	keg->uk_flags |= UMA_ZFLAG_LIMIT | UMA_ZONE_NOFREE;
	zone->uz_flags |= UMA_ZFLAG_LIMIT | UMA_ZONE_NOFREE;
	zone_update_caches(zone);

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
	slabs = howmany(items, keg->uk_ipers);
	while (slabs-- > 0) {
		aflags = M_NOWAIT;
		vm_domainset_iter_policy_ref_init(&di, &keg->uk_dr, &domain,
		    &aflags);
		for (;;) {
			slab = keg_alloc_slab(keg, zone, domain, M_WAITOK,
			    aflags);
			if (slab != NULL) {
				dom = &keg->uk_domain[slab->us_domain];
				/*
				 * keg_alloc_slab() always returns a slab on the
				 * partial list.
				 */
				LIST_REMOVE(slab, us_link);
				LIST_INSERT_HEAD(&dom->ud_free_slab, slab,
				    us_link);
				dom->ud_free_slabs++;
				KEG_UNLOCK(keg, slab->us_domain);
				break;
			}
			if (vm_domainset_iter_policy(&di, &domain) != 0)
				vm_wait_doms(&keg->uk_dr.dr_policy->ds_mask, 0);
		}
	}
}

/*
 * Returns a snapshot of memory consumption in bytes.
 */
size_t
uma_zone_memory(uma_zone_t zone)
{
	size_t sz;
	int i;

	sz = 0;
	if (zone->uz_flags & UMA_ZFLAG_CACHE) {
		for (i = 0; i < vm_ndomains; i++)
			sz += ZDOM_GET(zone, i)->uzd_nitems;
		return (sz * zone->uz_size);
	}
	for (i = 0; i < vm_ndomains; i++)
		sz += zone->uz_keg->uk_domain[i].ud_pages;

	return (sz * PAGE_SIZE);
}

struct uma_reclaim_args {
	int	domain;
	int	req;
};

static void
uma_reclaim_domain_cb(uma_zone_t zone, void *arg)
{
	struct uma_reclaim_args *args;

	args = arg;
	if ((zone->uz_flags & UMA_ZONE_UNMANAGED) == 0)
		uma_zone_reclaim_domain(zone, args->req, args->domain);
}

/* See uma.h */
void
uma_reclaim(int req)
{
	uma_reclaim_domain(req, UMA_ANYDOMAIN);
}

void
uma_reclaim_domain(int req, int domain)
{
	struct uma_reclaim_args args;

	bucket_enable();

	args.domain = domain;
	args.req = req;

	sx_slock(&uma_reclaim_lock);
	switch (req) {
	case UMA_RECLAIM_TRIM:
	case UMA_RECLAIM_DRAIN:
		zone_foreach(uma_reclaim_domain_cb, &args);
		break;
	case UMA_RECLAIM_DRAIN_CPU:
		zone_foreach(uma_reclaim_domain_cb, &args);
		pcpu_cache_drain_safe(NULL);
		zone_foreach(uma_reclaim_domain_cb, &args);
		break;
	default:
		panic("unhandled reclamation request %d", req);
	}

	/*
	 * Some slabs may have been freed but this zone will be visited early
	 * we visit again so that we can free pages that are empty once other
	 * zones are drained.  We have to do the same for buckets.
	 */
	uma_zone_reclaim_domain(slabzones[0], UMA_RECLAIM_DRAIN, domain);
	uma_zone_reclaim_domain(slabzones[1], UMA_RECLAIM_DRAIN, domain);
	bucket_zone_drain(domain);
	sx_sunlock(&uma_reclaim_lock);
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
	uma_zone_reclaim_domain(zone, req, UMA_ANYDOMAIN);
}

void
uma_zone_reclaim_domain(uma_zone_t zone, int req, int domain)
{
	switch (req) {
	case UMA_RECLAIM_TRIM:
		zone_reclaim(zone, domain, M_NOWAIT, false);
		break;
	case UMA_RECLAIM_DRAIN:
		zone_reclaim(zone, domain, M_NOWAIT, true);
		break;
	case UMA_RECLAIM_DRAIN_CPU:
		pcpu_cache_drain_safe(zone);
		zone_reclaim(zone, domain, M_NOWAIT, true);
		break;
	default:
		panic("unhandled reclamation request %d", req);
	}
}

/* See uma.h */
int
uma_zone_exhausted(uma_zone_t zone)
{

	return (atomic_load_32(&zone->uz_sleepers) > 0);
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
		cachefree += cache->uc_allocbucket.ucb_cnt;
		cachefree += cache->uc_freebucket.ucb_cnt;
		xdomain += cache->uc_crossbucket.ucb_cnt;
		cachefree += cache->uc_crossbucket.ucb_cnt;
		allocs += cache->uc_allocs;
		frees += cache->uc_frees;
	}
	allocs += counter_u64_fetch(z->uz_allocs);
	frees += counter_u64_fetch(z->uz_frees);
	xdomain += counter_u64_fetch(z->uz_xdomain);
	sleeps += z->uz_sleeps;
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
	uma_cache_t cache;
	int i;

	for (i = 0; i < vm_ndomains; i++) {
		zdom = ZDOM_GET(z, i);
		uth->uth_zone_free += zdom->uzd_nitems;
	}
	uth->uth_allocs = counter_u64_fetch(z->uz_allocs);
	uth->uth_frees = counter_u64_fetch(z->uz_frees);
	uth->uth_fails = counter_u64_fetch(z->uz_fails);
	uth->uth_xdomain = counter_u64_fetch(z->uz_xdomain);
	uth->uth_sleeps = z->uz_sleeps;

	for (i = 0; i < mp_maxid + 1; i++) {
		bzero(&ups[i], sizeof(*ups));
		if (internal || CPU_ABSENT(i))
			continue;
		cache = &z->uz_cpu[i];
		ups[i].ups_cache_free += cache->uc_allocbucket.ucb_cnt;
		ups[i].ups_cache_free += cache->uc_freebucket.ucb_cnt;
		ups[i].ups_cache_free += cache->uc_crossbucket.ucb_cnt;
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
	uint64_t items;
	uint32_t kfree, pages;
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
		kfree = pages = 0;
		for (i = 0; i < vm_ndomains; i++) {
			kfree += kz->uk_domain[i].ud_free_items;
			pages += kz->uk_domain[i].ud_pages;
		}
		LIST_FOREACH(z, &kz->uk_zones, uz_link) {
			bzero(&uth, sizeof(uth));
			strlcpy(uth.uth_name, z->uz_name, UTH_MAX_NAME);
			uth.uth_align = kz->uk_align;
			uth.uth_size = kz->uk_size;
			uth.uth_rsize = kz->uk_rsize;
			if (z->uz_max_items > 0) {
				items = UZ_ITEMS_COUNT(z->uz_items);
				uth.uth_pages = (items / kz->uk_ipers) *
					kz->uk_ppera;
			} else
				uth.uth_pages = pages;
			uth.uth_maxpages = (z->uz_max_items / kz->uk_ipers) *
			    kz->uk_ppera;
			uth.uth_limit = z->uz_max_items;
			uth.uth_keg_free = kfree;

			/*
			 * A zone is secondary is it is not the first entry
			 * on the keg's zone list.
			 */
			if ((z->uz_flags & UMA_ZONE_SECONDARY) &&
			    (LIST_FIRST(&kz->uk_zones) != z))
				uth.uth_zone_flags = UTH_ZONE_SECONDARY;
			uma_vm_zone_stats(&uth, z, &sbuf, ups,
			    kz->uk_flags & UMA_ZFLAG_INTERNAL);
			(void)sbuf_bcat(&sbuf, &uth, sizeof(uth));
			for (i = 0; i < mp_maxid + 1; i++)
				(void)sbuf_bcat(&sbuf, &ups[i], sizeof(ups[i]));
		}
	}
	LIST_FOREACH(z, &uma_cachezones, uz_link) {
		bzero(&uth, sizeof(uth));
		strlcpy(uth.uth_name, z->uz_name, UTH_MAX_NAME);
		uth.uth_size = z->uz_size;
		uma_vm_zone_stats(&uth, z, &sbuf, ups, false);
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
	if ((keg->uk_flags & UMA_ZFLAG_OFFPAGE) != 0)
		total += slabzone(keg->uk_ipers)->uz_keg->uk_rsize;
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

static int
sysctl_handle_uma_zone_items(SYSCTL_HANDLER_ARGS)
{
	uma_zone_t zone = arg1;
	uint64_t cur;

	cur = UZ_ITEMS_COUNT(atomic_load_64(&zone->uz_items));
	return (sysctl_handle_64(oidp, &cur, 0, req));
}

#ifdef INVARIANTS
static uma_slab_t
uma_dbg_getslab(uma_zone_t zone, void *item)
{
	uma_slab_t slab;
	uma_keg_t keg;
	uint8_t *mem;

	/*
	 * It is safe to return the slab here even though the
	 * zone is unlocked because the item's allocation state
	 * essentially holds a reference.
	 */
	mem = (uint8_t *)((uintptr_t)item & (~UMA_SLAB_MASK));
	if ((zone->uz_flags & UMA_ZFLAG_CACHE) != 0)
		return (NULL);
	if (zone->uz_flags & UMA_ZFLAG_VTOSLAB)
		return (vtoslab((vm_offset_t)mem));
	keg = zone->uz_keg;
	if ((keg->uk_flags & UMA_ZFLAG_HASH) == 0)
		return ((uma_slab_t)(mem + keg->uk_pgoff));
	KEG_LOCK(keg, 0);
	slab = hash_sfind(&keg->uk_hash, mem);
	KEG_UNLOCK(keg, 0);

	return (slab);
}

static bool
uma_dbg_zskip(uma_zone_t zone, void *mem)
{

	if ((zone->uz_flags & UMA_ZFLAG_CACHE) != 0)
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
			panic("uma: item %p did not belong to zone %s",
			    item, zone->uz_name);
	}
	keg = zone->uz_keg;
	freei = slab_item_index(slab, keg, item);

	if (BIT_TEST_SET_ATOMIC(keg->uk_ipers, freei,
	    slab_dbg_bits(slab, keg)))
		panic("Duplicate alloc of %p from zone %p(%s) slab %p(%d)",
		    item, zone, zone->uz_name, slab, freei);
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
			panic("uma: Freed item %p did not belong to zone %s",
			    item, zone->uz_name);
	}
	keg = zone->uz_keg;
	freei = slab_item_index(slab, keg, item);

	if (freei >= keg->uk_ipers)
		panic("Invalid free of %p from zone %p(%s) slab %p(%d)",
		    item, zone, zone->uz_name, slab, freei);

	if (slab_item(slab, keg, freei) != item)
		panic("Unaligned free of %p from zone %p(%s) slab %p(%d)",
		    item, zone, zone->uz_name, slab, freei);

	if (!BIT_TEST_CLR_ATOMIC(keg->uk_ipers, freei,
	    slab_dbg_bits(slab, keg)))
		panic("Duplicate free of %p from zone %p(%s) slab %p(%d)",
		    item, zone, zone->uz_name, slab, freei);
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
	for (i = 0; i < vm_ndomains; i++) {
		*cachefree += ZDOM_GET(z, i)->uzd_nitems;
		if (!((z->uz_flags & UMA_ZONE_SECONDARY) &&
		    (LIST_FIRST(&kz->uk_zones) != z)))
			*cachefree += kz->uk_domain[i].ud_free_items;
	}
	*used = *allocs - frees;
	return (((int64_t)*used + *cachefree) * kz->uk_size);
}

DB_SHOW_COMMAND_FLAGS(uma, db_show_uma, DB_CMD_MEMSAFE)
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

DB_SHOW_COMMAND_FLAGS(umacache, db_show_umacache, DB_CMD_MEMSAFE)
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
			cachefree += ZDOM_GET(z, i)->uzd_nitems;
		db_printf("%18s %8ju %8jd %8ld %12ju %8u\n",
		    z->uz_name, (uintmax_t)z->uz_size,
		    (intmax_t)(allocs - frees), cachefree,
		    (uintmax_t)allocs, z->uz_bucket_size);
		if (db_pager_quit)
			return;
	}
}
#endif	/* DDB */
