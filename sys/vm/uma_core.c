/*-
 * Copyright (c) 2002, 2003, 2004, 2005 Jeffrey Roberson <jeff@FreeBSD.org>
 * Copyright (c) 2004, 2005 Bosko Milekic <bmilekic@FreeBSD.org>
 * Copyright (c) 2004-2005 Robert N. M. Watson
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
 * effecient.  A primary design goal is to return unused memory to the rest of
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

/* I should really use ktr.. */
/*
#define UMA_DEBUG 1
#define UMA_DEBUG_ALLOC 1
#define UMA_DEBUG_ALLOC_1 1
*/

#include "opt_ddb.h"
#include "opt_param.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>

#include <machine/vmparam.h>

#include <ddb/ddb.h>

/*
 * This is the zone and keg from which all zones are spawned.  The idea is that
 * even the zone & keg heads are allocated from the allocator, so we use the
 * bss section to bootstrap us.
 */
static struct uma_keg masterkeg;
static struct uma_zone masterzone_k;
static struct uma_zone masterzone_z;
static uma_zone_t kegs = &masterzone_k;
static uma_zone_t zones = &masterzone_z;

/* This is the zone from which all of uma_slab_t's are allocated. */
static uma_zone_t slabzone;
static uma_zone_t slabrefzone;	/* With refcounters (for UMA_ZONE_REFCNT) */

/*
 * The initial hash tables come out of this zone so they can be allocated
 * prior to malloc coming up.
 */
static uma_zone_t hashzone;

static MALLOC_DEFINE(M_UMAHASH, "UMAHash", "UMA Hash Buckets");

/*
 * Are we allowed to allocate buckets?
 */
static int bucketdisable = 1;

/* Linked list of all kegs in the system */
static LIST_HEAD(,uma_keg) uma_kegs = LIST_HEAD_INITIALIZER(&uma_kegs);

/* This mutex protects the keg list */
static struct mtx uma_mtx;

/* Linked list of boot time pages */
static LIST_HEAD(,uma_slab) uma_boot_pages =
    LIST_HEAD_INITIALIZER(&uma_boot_pages);

/* Count of free boottime pages */
static int uma_boot_free = 0;

/* Is the VM done starting up? */
static int booted = 0;

/* Maximum number of allowed items-per-slab if the slab header is OFFPAGE */
static u_int uma_max_ipers;
static u_int uma_max_ipers_ref;

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
	char *name;
	size_t size;
	uma_ctor ctor;
	uma_dtor dtor;
	uma_init uminit;
	uma_fini fini;
	uma_keg_t keg;
	int align;
	u_int32_t flags;
};

struct uma_kctor_args {
	uma_zone_t zone;
	size_t size;
	uma_init uminit;
	uma_fini fini;
	int align;
	u_int32_t flags;
};

struct uma_bucket_zone {
	uma_zone_t	ubz_zone;
	char		*ubz_name;
	int		ubz_entries;
};

#define	BUCKET_MAX	128

struct uma_bucket_zone bucket_zones[] = {
	{ NULL, "16 Bucket", 16 },
	{ NULL, "32 Bucket", 32 },
	{ NULL, "64 Bucket", 64 },
	{ NULL, "128 Bucket", 128 },
	{ NULL, NULL, 0}
};

#define	BUCKET_SHIFT	4
#define	BUCKET_ZONES	((BUCKET_MAX >> BUCKET_SHIFT) + 1)

/*
 * bucket_size[] maps requested bucket sizes to zones that allocate a bucket
 * of approximately the right size.
 */
static uint8_t bucket_size[BUCKET_ZONES];

/*
 * Flags and enumerations to be passed to internal functions.
 */
enum zfreeskip { SKIP_NONE, SKIP_DTOR, SKIP_FINI };

#define	ZFREE_STATFAIL	0x00000001	/* Update zone failure statistic. */
#define	ZFREE_STATFREE	0x00000002	/* Update zone free statistic. */

/* Prototypes.. */

static void *obj_alloc(uma_zone_t, int, u_int8_t *, int);
static void *page_alloc(uma_zone_t, int, u_int8_t *, int);
static void *startup_alloc(uma_zone_t, int, u_int8_t *, int);
static void page_free(void *, int, u_int8_t);
static uma_slab_t slab_zalloc(uma_zone_t, int);
static void cache_drain(uma_zone_t);
static void bucket_drain(uma_zone_t, uma_bucket_t);
static void bucket_cache_drain(uma_zone_t zone);
static int keg_ctor(void *, int, void *, int);
static void keg_dtor(void *, int, void *);
static int zone_ctor(void *, int, void *, int);
static void zone_dtor(void *, int, void *);
static int zero_init(void *, int, int);
static void zone_small_init(uma_zone_t zone);
static void zone_large_init(uma_zone_t zone);
static void zone_foreach(void (*zfunc)(uma_zone_t));
static void zone_timeout(uma_zone_t zone);
static int hash_alloc(struct uma_hash *);
static int hash_expand(struct uma_hash *, struct uma_hash *);
static void hash_free(struct uma_hash *hash);
static void uma_timeout(void *);
static void uma_startup3(void);
static void *uma_zalloc_internal(uma_zone_t, void *, int);
static void uma_zfree_internal(uma_zone_t, void *, void *, enum zfreeskip,
    int);
static void bucket_enable(void);
static void bucket_init(void);
static uma_bucket_t bucket_alloc(int, int);
static void bucket_free(uma_bucket_t);
static void bucket_zone_drain(void);
static int uma_zalloc_bucket(uma_zone_t zone, int flags);
static uma_slab_t uma_zone_slab(uma_zone_t zone, int flags);
static void *uma_slab_alloc(uma_zone_t zone, uma_slab_t slab);
static void zone_drain(uma_zone_t);
static uma_zone_t uma_kcreate(uma_zone_t zone, size_t size, uma_init uminit,
    uma_fini fini, int align, u_int32_t flags);

void uma_print_zone(uma_zone_t);
void uma_print_stats(void);
static int sysctl_vm_zone(SYSCTL_HANDLER_ARGS);
static int sysctl_vm_zone_count(SYSCTL_HANDLER_ARGS);
static int sysctl_vm_zone_stats(SYSCTL_HANDLER_ARGS);

#ifdef WITNESS
static int nosleepwithlocks = 1;
SYSCTL_INT(_debug, OID_AUTO, nosleepwithlocks, CTLFLAG_RW, &nosleepwithlocks,
    0, "Convert M_WAITOK to M_NOWAIT to avoid lock-held-across-sleep paths");
#else
static int nosleepwithlocks = 0;
SYSCTL_INT(_debug, OID_AUTO, nosleepwithlocks, CTLFLAG_RW, &nosleepwithlocks,
    0, "Convert M_WAITOK to M_NOWAIT to avoid lock-held-across-sleep paths");
#endif
SYSCTL_OID(_vm, OID_AUTO, zone, CTLTYPE_STRING|CTLFLAG_RD,
    NULL, 0, sysctl_vm_zone, "A", "Zone Info");
SYSINIT(uma_startup3, SI_SUB_VM_CONF, SI_ORDER_SECOND, uma_startup3, NULL);

SYSCTL_PROC(_vm, OID_AUTO, zone_count, CTLFLAG_RD|CTLTYPE_INT,
    0, 0, sysctl_vm_zone_count, "I", "Number of UMA zones");

SYSCTL_PROC(_vm, OID_AUTO, zone_stats, CTLFLAG_RD|CTLTYPE_STRUCT,
    0, 0, sysctl_vm_zone_stats, "s,struct uma_type_header", "Zone Stats");

/*
 * This routine checks to see whether or not it's safe to enable buckets.
 */

static void
bucket_enable(void)
{
	if (cnt.v_free_count < cnt.v_free_min)
		bucketdisable = 1;
	else
		bucketdisable = 0;
}

/*
 * Initialize bucket_zones, the array of zones of buckets of various sizes.
 *
 * For each zone, calculate the memory required for each bucket, consisting
 * of the header and an array of pointers.  Initialize bucket_size[] to point
 * the range of appropriate bucket sizes at the zone.
 */
static void
bucket_init(void)
{
	struct uma_bucket_zone *ubz;
	int i;
	int j;

	for (i = 0, j = 0; bucket_zones[j].ubz_entries != 0; j++) {
		int size;

		ubz = &bucket_zones[j];
		size = roundup(sizeof(struct uma_bucket), sizeof(void *));
		size += sizeof(void *) * ubz->ubz_entries;
		ubz->ubz_zone = uma_zcreate(ubz->ubz_name, size,
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);
		for (; i <= ubz->ubz_entries; i += (1 << BUCKET_SHIFT))
			bucket_size[i >> BUCKET_SHIFT] = j;
	}
}

/*
 * Given a desired number of entries for a bucket, return the zone from which
 * to allocate the bucket.
 */
static struct uma_bucket_zone *
bucket_zone_lookup(int entries)
{
	int idx;

	idx = howmany(entries, 1 << BUCKET_SHIFT);
	return (&bucket_zones[bucket_size[idx]]);
}

static uma_bucket_t
bucket_alloc(int entries, int bflags)
{
	struct uma_bucket_zone *ubz;
	uma_bucket_t bucket;

	/*
	 * This is to stop us from allocating per cpu buckets while we're
	 * running out of UMA_BOOT_PAGES.  Otherwise, we would exhaust the
	 * boot pages.  This also prevents us from allocating buckets in
	 * low memory situations.
	 */
	if (bucketdisable)
		return (NULL);

	ubz = bucket_zone_lookup(entries);
	bucket = uma_zalloc_internal(ubz->ubz_zone, NULL, bflags);
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
bucket_free(uma_bucket_t bucket)
{
	struct uma_bucket_zone *ubz;

	ubz = bucket_zone_lookup(bucket->ub_entries);
	uma_zfree_internal(ubz->ubz_zone, bucket, NULL, SKIP_NONE,
	    ZFREE_STATFREE);
}

static void
bucket_zone_drain(void)
{
	struct uma_bucket_zone *ubz;

	for (ubz = &bucket_zones[0]; ubz->ubz_entries != 0; ubz++)
		zone_drain(ubz->ubz_zone);
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
	zone_foreach(zone_timeout);

	/* Reschedule this event */
	callout_reset(&uma_callout, UMA_TIMEOUT * hz, uma_timeout, NULL);
}

/*
 * Routine to perform timeout driven calculations.  This expands the
 * hashes and does per cpu statistics aggregation.
 *
 *  Arguments:
 *	zone  The zone to operate on
 *
 *  Returns:
 *	Nothing
 */
static void
zone_timeout(uma_zone_t zone)
{
	uma_keg_t keg;
	u_int64_t alloc;

	keg = zone->uz_keg;
	alloc = 0;

	/*
	 * Expand the zone hash table.
	 *
	 * This is done if the number of slabs is larger than the hash size.
	 * What I'm trying to do here is completely reduce collisions.  This
	 * may be a little aggressive.  Should I allow for two collisions max?
	 */
	ZONE_LOCK(zone);
	if (keg->uk_flags & UMA_ZONE_HASH &&
	    keg->uk_pages / keg->uk_ppera >= keg->uk_hash.uh_hashsize) {
		struct uma_hash newhash;
		struct uma_hash oldhash;
		int ret;

		/*
		 * This is so involved because allocating and freeing
		 * while the zone lock is held will lead to deadlock.
		 * I have to do everything in stages and check for
		 * races.
		 */
		newhash = keg->uk_hash;
		ZONE_UNLOCK(zone);
		ret = hash_alloc(&newhash);
		ZONE_LOCK(zone);
		if (ret) {
			if (hash_expand(&keg->uk_hash, &newhash)) {
				oldhash = keg->uk_hash;
				keg->uk_hash = newhash;
			} else
				oldhash = newhash;

			ZONE_UNLOCK(zone);
			hash_free(&oldhash);
			ZONE_LOCK(zone);
		}
	}
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
 *	1 on sucess and 0 on failure.
 */
static int
hash_alloc(struct uma_hash *hash)
{
	int oldsize;
	int alloc;

	oldsize = hash->uh_hashsize;

	/* We're just going to go to a power of two greater */
	if (oldsize)  {
		hash->uh_hashsize = oldsize * 2;
		alloc = sizeof(hash->uh_slab_hash[0]) * hash->uh_hashsize;
		hash->uh_slab_hash = (struct slabhead *)malloc(alloc,
		    M_UMAHASH, M_NOWAIT);
	} else {
		alloc = sizeof(hash->uh_slab_hash[0]) * UMA_HASH_SIZE_INIT;
		hash->uh_slab_hash = uma_zalloc_internal(hashzone, NULL,
		    M_WAITOK);
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
	uma_slab_t slab;
	int hval;
	int i;

	if (!newhash->uh_slab_hash)
		return (0);

	if (oldhash->uh_hashsize >= newhash->uh_hashsize)
		return (0);

	/*
	 * I need to investigate hash algorithms for resizing without a
	 * full rehash.
	 */

	for (i = 0; i < oldhash->uh_hashsize; i++)
		while (!SLIST_EMPTY(&oldhash->uh_slab_hash[i])) {
			slab = SLIST_FIRST(&oldhash->uh_slab_hash[i]);
			SLIST_REMOVE_HEAD(&oldhash->uh_slab_hash[i], us_hlink);
			hval = UMA_HASH(newhash, slab->us_data);
			SLIST_INSERT_HEAD(&newhash->uh_slab_hash[hval],
			    slab, us_hlink);
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
		uma_zfree_internal(hashzone,
		    hash->uh_slab_hash, NULL, SKIP_NONE, ZFREE_STATFREE);
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
	uma_slab_t slab;
	int mzone;
	void *item;

	if (bucket == NULL)
		return;

	slab = NULL;
	mzone = 0;

	/* We have to lookup the slab again for malloc.. */
	if (zone->uz_keg->uk_flags & UMA_ZONE_MALLOC)
		mzone = 1;

	while (bucket->ub_cnt > 0)  {
		bucket->ub_cnt--;
		item = bucket->ub_bucket[bucket->ub_cnt];
#ifdef INVARIANTS
		bucket->ub_bucket[bucket->ub_cnt] = NULL;
		KASSERT(item != NULL,
		    ("bucket_drain: botched ptr, item is NULL"));
#endif
		/*
		 * This is extremely inefficient.  The slab pointer was passed
		 * to uma_zfree_arg, but we lost it because the buckets don't
		 * hold them.  This will go away when free() gets a size passed
		 * to it.
		 */
		if (mzone)
			slab = vtoslab((vm_offset_t)item & (~UMA_SLAB_MASK));
		uma_zfree_internal(zone, item, slab, SKIP_DTOR, 0);
	}
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
	 * XXX: We lock the zone before passing into bucket_cache_drain() as
	 * it is used elsewhere.  Should the tear-down path be made special
	 * there in some form?
	 */
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		cache = &zone->uz_cpu[cpu];
		bucket_drain(zone, cache->uc_allocbucket);
		bucket_drain(zone, cache->uc_freebucket);
		if (cache->uc_allocbucket != NULL)
			bucket_free(cache->uc_allocbucket);
		if (cache->uc_freebucket != NULL)
			bucket_free(cache->uc_freebucket);
		cache->uc_allocbucket = cache->uc_freebucket = NULL;
	}
	ZONE_LOCK(zone);
	bucket_cache_drain(zone);
	ZONE_UNLOCK(zone);
}

/*
 * Drain the cached buckets from a zone.  Expects a locked zone on entry.
 */
static void
bucket_cache_drain(uma_zone_t zone)
{
	uma_bucket_t bucket;

	/*
	 * Drain the bucket queues and free the buckets, we just keep two per
	 * cpu (alloc/free).
	 */
	while ((bucket = LIST_FIRST(&zone->uz_full_bucket)) != NULL) {
		LIST_REMOVE(bucket, ub_link);
		ZONE_UNLOCK(zone);
		bucket_drain(zone, bucket);
		bucket_free(bucket);
		ZONE_LOCK(zone);
	}

	/* Now we do the free queue.. */
	while ((bucket = LIST_FIRST(&zone->uz_free_bucket)) != NULL) {
		LIST_REMOVE(bucket, ub_link);
		bucket_free(bucket);
	}
}

/*
 * Frees pages from a zone back to the system.  This is done on demand from
 * the pageout daemon.
 *
 * Arguments:
 *	zone  The zone to free pages from
 *	 all  Should we drain all items?
 *
 * Returns:
 *	Nothing.
 */
static void
zone_drain(uma_zone_t zone)
{
	struct slabhead freeslabs = { 0 };
	uma_keg_t keg;
	uma_slab_t slab;
	uma_slab_t n;
	u_int8_t flags;
	u_int8_t *mem;
	int i;

	keg = zone->uz_keg;

	/*
	 * We don't want to take pages from statically allocated zones at this
	 * time
	 */
	if (keg->uk_flags & UMA_ZONE_NOFREE || keg->uk_freef == NULL)
		return;

	ZONE_LOCK(zone);

#ifdef UMA_DEBUG
	printf("%s free items: %u\n", zone->uz_name, keg->uk_free);
#endif
	bucket_cache_drain(zone);
	if (keg->uk_free == 0)
		goto finished;

	slab = LIST_FIRST(&keg->uk_free_slab);
	while (slab) {
		n = LIST_NEXT(slab, us_link);

		/* We have no where to free these to */
		if (slab->us_flags & UMA_SLAB_BOOT) {
			slab = n;
			continue;
		}

		LIST_REMOVE(slab, us_link);
		keg->uk_pages -= keg->uk_ppera;
		keg->uk_free -= keg->uk_ipers;

		if (keg->uk_flags & UMA_ZONE_HASH)
			UMA_HASH_REMOVE(&keg->uk_hash, slab, slab->us_data);

		SLIST_INSERT_HEAD(&freeslabs, slab, us_hlink);

		slab = n;
	}
finished:
	ZONE_UNLOCK(zone);

	while ((slab = SLIST_FIRST(&freeslabs)) != NULL) {
		SLIST_REMOVE(&freeslabs, slab, uma_slab, us_hlink);
		if (keg->uk_fini)
			for (i = 0; i < keg->uk_ipers; i++)
				keg->uk_fini(
				    slab->us_data + (keg->uk_rsize * i),
				    keg->uk_size);
		flags = slab->us_flags;
		mem = slab->us_data;

		if ((keg->uk_flags & UMA_ZONE_MALLOC) ||
		    (keg->uk_flags & UMA_ZONE_REFCNT)) {
			vm_object_t obj;

			if (flags & UMA_SLAB_KMEM)
				obj = kmem_object;
			else
				obj = NULL;
			for (i = 0; i < keg->uk_ppera; i++)
				vsetobj((vm_offset_t)mem + (i * PAGE_SIZE),
				    obj);
		}
		if (keg->uk_flags & UMA_ZONE_OFFPAGE)
			uma_zfree_internal(keg->uk_slabzone, slab, NULL,
			    SKIP_NONE, ZFREE_STATFREE);
#ifdef UMA_DEBUG
		printf("%s: Returning %d bytes.\n",
		    zone->uz_name, UMA_SLAB_SIZE * keg->uk_ppera);
#endif
		keg->uk_freef(mem, UMA_SLAB_SIZE * keg->uk_ppera, flags);
	}
}

/*
 * Allocate a new slab for a zone.  This does not insert the slab onto a list.
 *
 * Arguments:
 *	zone  The zone to allocate slabs for
 *	wait  Shall we wait?
 *
 * Returns:
 *	The slab that was allocated or NULL if there is no memory and the
 *	caller specified M_NOWAIT.
 */
static uma_slab_t
slab_zalloc(uma_zone_t zone, int wait)
{
	uma_slabrefcnt_t slabref;
	uma_slab_t slab;
	uma_keg_t keg;
	u_int8_t *mem;
	u_int8_t flags;
	int i;

	slab = NULL;
	keg = zone->uz_keg;

#ifdef UMA_DEBUG
	printf("slab_zalloc:  Allocating a new slab for %s\n", zone->uz_name);
#endif
	ZONE_UNLOCK(zone);

	if (keg->uk_flags & UMA_ZONE_OFFPAGE) {
		slab = uma_zalloc_internal(keg->uk_slabzone, NULL, wait);
		if (slab == NULL) {
			ZONE_LOCK(zone);
			return NULL;
		}
	}

	/*
	 * This reproduces the old vm_zone behavior of zero filling pages the
	 * first time they are added to a zone.
	 *
	 * Malloced items are zeroed in uma_zalloc.
	 */

	if ((keg->uk_flags & UMA_ZONE_MALLOC) == 0)
		wait |= M_ZERO;
	else
		wait &= ~M_ZERO;

	mem = keg->uk_allocf(zone, keg->uk_ppera * UMA_SLAB_SIZE,
	    &flags, wait);
	if (mem == NULL) {
		if (keg->uk_flags & UMA_ZONE_OFFPAGE)
			uma_zfree_internal(keg->uk_slabzone, slab, NULL,
			    SKIP_NONE, ZFREE_STATFREE);
		ZONE_LOCK(zone);
		return (NULL);
	}

	/* Point the slab into the allocated memory */
	if (!(keg->uk_flags & UMA_ZONE_OFFPAGE))
		slab = (uma_slab_t )(mem + keg->uk_pgoff);

	if ((keg->uk_flags & UMA_ZONE_MALLOC) ||
	    (keg->uk_flags & UMA_ZONE_REFCNT))
		for (i = 0; i < keg->uk_ppera; i++)
			vsetslab((vm_offset_t)mem + (i * PAGE_SIZE), slab);

	slab->us_keg = keg;
	slab->us_data = mem;
	slab->us_freecount = keg->uk_ipers;
	slab->us_firstfree = 0;
	slab->us_flags = flags;

	if (keg->uk_flags & UMA_ZONE_REFCNT) {
		slabref = (uma_slabrefcnt_t)slab;
		for (i = 0; i < keg->uk_ipers; i++) {
			slabref->us_freelist[i].us_refcnt = 0;
			slabref->us_freelist[i].us_item = i+1;
		}
	} else {
		for (i = 0; i < keg->uk_ipers; i++)
			slab->us_freelist[i].us_item = i+1;
	}

	if (keg->uk_init != NULL) {
		for (i = 0; i < keg->uk_ipers; i++)
			if (keg->uk_init(slab->us_data + (keg->uk_rsize * i),
			    keg->uk_size, wait) != 0)
				break;
		if (i != keg->uk_ipers) {
			if (keg->uk_fini != NULL) {
				for (i--; i > -1; i--)
					keg->uk_fini(slab->us_data +
					    (keg->uk_rsize * i),
					    keg->uk_size);
			}
			if ((keg->uk_flags & UMA_ZONE_MALLOC) ||
			    (keg->uk_flags & UMA_ZONE_REFCNT)) {
				vm_object_t obj;

				if (flags & UMA_SLAB_KMEM)
					obj = kmem_object;
				else
					obj = NULL;
				for (i = 0; i < keg->uk_ppera; i++)
					vsetobj((vm_offset_t)mem +
					    (i * PAGE_SIZE), obj);
			}
			if (keg->uk_flags & UMA_ZONE_OFFPAGE)
				uma_zfree_internal(keg->uk_slabzone, slab,
				    NULL, SKIP_NONE, ZFREE_STATFREE);
			keg->uk_freef(mem, UMA_SLAB_SIZE * keg->uk_ppera,
			    flags);
			ZONE_LOCK(zone);
			return (NULL);
		}
	}
	ZONE_LOCK(zone);

	if (keg->uk_flags & UMA_ZONE_HASH)
		UMA_HASH_INSERT(&keg->uk_hash, slab, mem);

	keg->uk_pages += keg->uk_ppera;
	keg->uk_free += keg->uk_ipers;

	return (slab);
}

/*
 * This function is intended to be used early on in place of page_alloc() so
 * that we may use the boot time page cache to satisfy allocations before
 * the VM is ready.
 */
static void *
startup_alloc(uma_zone_t zone, int bytes, u_int8_t *pflag, int wait)
{
	uma_keg_t keg;

	keg = zone->uz_keg;

	/*
	 * Check our small startup cache to see if it has pages remaining.
	 */
	mtx_lock(&uma_mtx);
	if (uma_boot_free != 0) {
		uma_slab_t tmps;

		tmps = LIST_FIRST(&uma_boot_pages);
		LIST_REMOVE(tmps, us_link);
		uma_boot_free--;
		mtx_unlock(&uma_mtx);
		*pflag = tmps->us_flags;
		return (tmps->us_data);
	}
	mtx_unlock(&uma_mtx);
	if (booted == 0)
		panic("UMA: Increase UMA_BOOT_PAGES");
	/*
	 * Now that we've booted reset these users to their real allocator.
	 */
#ifdef UMA_MD_SMALL_ALLOC
	keg->uk_allocf = uma_small_alloc;
#else
	keg->uk_allocf = page_alloc;
#endif
	return keg->uk_allocf(zone, bytes, pflag, wait);
}

/*
 * Allocates a number of pages from the system
 *
 * Arguments:
 *	zone  Unused
 *	bytes  The number of bytes requested
 *	wait  Shall we wait?
 *
 * Returns:
 *	A pointer to the alloced memory or possibly
 *	NULL if M_NOWAIT is set.
 */
static void *
page_alloc(uma_zone_t zone, int bytes, u_int8_t *pflag, int wait)
{
	void *p;	/* Returned page */

	*pflag = UMA_SLAB_KMEM;
	p = (void *) kmem_malloc(kmem_map, bytes, wait);

	return (p);
}

/*
 * Allocates a number of pages from within an object
 *
 * Arguments:
 *	zone   Unused
 *	bytes  The number of bytes requested
 *	wait   Shall we wait?
 *
 * Returns:
 *	A pointer to the alloced memory or possibly
 *	NULL if M_NOWAIT is set.
 */
static void *
obj_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	vm_object_t object;
	vm_offset_t retkva, zkva;
	vm_page_t p;
	int pages, startpages;

	object = zone->uz_keg->uk_obj;
	retkva = 0;

	/*
	 * This looks a little weird since we're getting one page at a time.
	 */
	VM_OBJECT_LOCK(object);
	p = TAILQ_LAST(&object->memq, pglist);
	pages = p != NULL ? p->pindex + 1 : 0;
	startpages = pages;
	zkva = zone->uz_keg->uk_kva + pages * PAGE_SIZE;
	for (; bytes > 0; bytes -= PAGE_SIZE) {
		p = vm_page_alloc(object, pages,
		    VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED);
		if (p == NULL) {
			if (pages != startpages)
				pmap_qremove(retkva, pages - startpages);
			while (pages != startpages) {
				pages--;
				p = TAILQ_LAST(&object->memq, pglist);
				vm_page_lock_queues();
				vm_page_unwire(p, 0);
				vm_page_free(p);
				vm_page_unlock_queues();
			}
			retkva = 0;
			goto done;
		}
		pmap_qenter(zkva, &p, 1);
		if (retkva == 0)
			retkva = zkva;
		zkva += PAGE_SIZE;
		pages += 1;
	}
done:
	VM_OBJECT_UNLOCK(object);
	*flags = UMA_SLAB_PRIV;

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
page_free(void *mem, int size, u_int8_t flags)
{
	vm_map_t map;

	if (flags & UMA_SLAB_KMEM)
		map = kmem_map;
	else
		panic("UMA: page_free used with invalid flags %d\n", flags);

	kmem_free(map, (vm_offset_t)mem, size);
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

/*
 * Finish creating a small uma zone.  This calculates ipers, and the zone size.
 *
 * Arguments
 *	zone  The zone we should initialize
 *
 * Returns
 *	Nothing
 */
static void
zone_small_init(uma_zone_t zone)
{
	uma_keg_t keg;
	u_int rsize;
	u_int memused;
	u_int wastedspace;
	u_int shsize;

	keg = zone->uz_keg;
	KASSERT(keg != NULL, ("Keg is null in zone_small_init"));
	rsize = keg->uk_size;

	if (rsize < UMA_SMALLEST_UNIT)
		rsize = UMA_SMALLEST_UNIT;
	if (rsize & keg->uk_align)
		rsize = (rsize & ~keg->uk_align) + (keg->uk_align + 1);

	keg->uk_rsize = rsize;
	keg->uk_ppera = 1;

	if (keg->uk_flags & UMA_ZONE_REFCNT) {
		rsize += UMA_FRITMREF_SZ;	/* linkage & refcnt */
		shsize = sizeof(struct uma_slab_refcnt);
	} else {
		rsize += UMA_FRITM_SZ;	/* Account for linkage */
		shsize = sizeof(struct uma_slab);
	}

	keg->uk_ipers = (UMA_SLAB_SIZE - shsize) / rsize;
	KASSERT(keg->uk_ipers != 0, ("zone_small_init: ipers is 0"));
	memused = keg->uk_ipers * rsize + shsize;
	wastedspace = UMA_SLAB_SIZE - memused;

	/*
	 * We can't do OFFPAGE if we're internal or if we've been
	 * asked to not go to the VM for buckets.  If we do this we
	 * may end up going to the VM (kmem_map) for slabs which we
	 * do not want to do if we're UMA_ZFLAG_CACHEONLY as a
	 * result of UMA_ZONE_VM, which clearly forbids it.
	 */
	if ((keg->uk_flags & UMA_ZFLAG_INTERNAL) ||
	    (keg->uk_flags & UMA_ZFLAG_CACHEONLY))
		return;

	if ((wastedspace >= UMA_MAX_WASTE) &&
	    (keg->uk_ipers < (UMA_SLAB_SIZE / keg->uk_rsize))) {
		keg->uk_ipers = UMA_SLAB_SIZE / keg->uk_rsize;
		KASSERT(keg->uk_ipers <= 255,
		    ("zone_small_init: keg->uk_ipers too high!"));
#ifdef UMA_DEBUG
		printf("UMA decided we need offpage slab headers for "
		    "zone: %s, calculated wastedspace = %d, "
		    "maximum wasted space allowed = %d, "
		    "calculated ipers = %d, "
		    "new wasted space = %d\n", zone->uz_name, wastedspace,
		    UMA_MAX_WASTE, keg->uk_ipers,
		    UMA_SLAB_SIZE - keg->uk_ipers * keg->uk_rsize);
#endif
		keg->uk_flags |= UMA_ZONE_OFFPAGE;
		if ((keg->uk_flags & UMA_ZONE_MALLOC) == 0)
			keg->uk_flags |= UMA_ZONE_HASH;
	}
}

/*
 * Finish creating a large (> UMA_SLAB_SIZE) uma zone.  Just give in and do
 * OFFPAGE for now.  When I can allow for more dynamic slab sizes this will be
 * more complicated.
 *
 * Arguments
 *	zone  The zone we should initialize
 *
 * Returns
 *	Nothing
 */
static void
zone_large_init(uma_zone_t zone)
{
	uma_keg_t keg;
	int pages;

	keg = zone->uz_keg;

	KASSERT(keg != NULL, ("Keg is null in zone_large_init"));
	KASSERT((keg->uk_flags & UMA_ZFLAG_CACHEONLY) == 0,
	    ("zone_large_init: Cannot large-init a UMA_ZFLAG_CACHEONLY zone"));

	pages = keg->uk_size / UMA_SLAB_SIZE;

	/* Account for remainder */
	if ((pages * UMA_SLAB_SIZE) < keg->uk_size)
		pages++;

	keg->uk_ppera = pages;
	keg->uk_ipers = 1;

	keg->uk_flags |= UMA_ZONE_OFFPAGE;
	if ((keg->uk_flags & UMA_ZONE_MALLOC) == 0)
		keg->uk_flags |= UMA_ZONE_HASH;

	keg->uk_rsize = keg->uk_size;
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
	keg->uk_pages = 0;
	keg->uk_flags = arg->flags;
	keg->uk_allocf = page_alloc;
	keg->uk_freef = page_free;
	keg->uk_recurse = 0;
	keg->uk_slabzone = NULL;

	/*
	 * The master zone is passed to us at keg-creation time.
	 */
	zone = arg->zone;
	zone->uz_keg = keg;

	if (arg->flags & UMA_ZONE_VM)
		keg->uk_flags |= UMA_ZFLAG_CACHEONLY;

	if (arg->flags & UMA_ZONE_ZINIT)
		keg->uk_init = zero_init;

	/*
	 * The +UMA_FRITM_SZ added to uk_size is to account for the
	 * linkage that is added to the size in zone_small_init().  If
	 * we don't account for this here then we may end up in
	 * zone_small_init() with a calculated 'ipers' of 0.
	 */
	if (keg->uk_flags & UMA_ZONE_REFCNT) {
		if ((keg->uk_size+UMA_FRITMREF_SZ) >
		    (UMA_SLAB_SIZE - sizeof(struct uma_slab_refcnt)))
			zone_large_init(zone);
		else
			zone_small_init(zone);
	} else {
		if ((keg->uk_size+UMA_FRITM_SZ) >
		    (UMA_SLAB_SIZE - sizeof(struct uma_slab)))
			zone_large_init(zone);
		else
			zone_small_init(zone);
	}

	if (keg->uk_flags & UMA_ZONE_OFFPAGE) {
		if (keg->uk_flags & UMA_ZONE_REFCNT)
			keg->uk_slabzone = slabrefzone;
		else
			keg->uk_slabzone = slabzone;
	}

	/*
	 * If we haven't booted yet we need allocations to go through the
	 * startup cache until the vm is ready.
	 */
	if (keg->uk_ppera == 1) {
#ifdef UMA_MD_SMALL_ALLOC
		keg->uk_allocf = uma_small_alloc;
		keg->uk_freef = uma_small_free;
#endif
		if (booted == 0)
			keg->uk_allocf = startup_alloc;
	}

	/*
	 * Initialize keg's lock (shared among zones) through
	 * Master zone
	 */
	zone->uz_lock = &keg->uk_lock;
	if (arg->flags & UMA_ZONE_MTXCLASS)
		ZONE_LOCK_INIT(zone, 1);
	else
		ZONE_LOCK_INIT(zone, 0);

	/*
	 * If we're putting the slab header in the actual page we need to
	 * figure out where in each page it goes.  This calculates a right
	 * justified offset into the memory on an ALIGN_PTR boundary.
	 */
	if (!(keg->uk_flags & UMA_ZONE_OFFPAGE)) {
		u_int totsize;

		/* Size of the slab struct and free list */
		if (keg->uk_flags & UMA_ZONE_REFCNT)
			totsize = sizeof(struct uma_slab_refcnt) +
			    keg->uk_ipers * UMA_FRITMREF_SZ;
		else
			totsize = sizeof(struct uma_slab) +
			    keg->uk_ipers * UMA_FRITM_SZ;

		if (totsize & UMA_ALIGN_PTR)
			totsize = (totsize & ~UMA_ALIGN_PTR) +
			    (UMA_ALIGN_PTR + 1);
		keg->uk_pgoff = UMA_SLAB_SIZE - totsize;

		if (keg->uk_flags & UMA_ZONE_REFCNT)
			totsize = keg->uk_pgoff + sizeof(struct uma_slab_refcnt)
			    + keg->uk_ipers * UMA_FRITMREF_SZ;
		else
			totsize = keg->uk_pgoff + sizeof(struct uma_slab)
			    + keg->uk_ipers * UMA_FRITM_SZ;

		/*
		 * The only way the following is possible is if with our
		 * UMA_ALIGN_PTR adjustments we are now bigger than
		 * UMA_SLAB_SIZE.  I haven't checked whether this is
		 * mathematically possible for all cases, so we make
		 * sure here anyway.
		 */
		if (totsize > UMA_SLAB_SIZE) {
			printf("zone %s ipers %d rsize %d size %d\n",
			    zone->uz_name, keg->uk_ipers, keg->uk_rsize,
			    keg->uk_size);
			panic("UMA slab won't fit.\n");
		}
	}

	if (keg->uk_flags & UMA_ZONE_HASH)
		hash_alloc(&keg->uk_hash);

#ifdef UMA_DEBUG
	printf("%s(%p) size = %d ipers = %d ppera = %d pgoff = %d\n",
	    zone->uz_name, zone,
	    keg->uk_size, keg->uk_ipers,
	    keg->uk_ppera, keg->uk_pgoff);
#endif

	LIST_INSERT_HEAD(&keg->uk_zones, zone, uz_link);

	mtx_lock(&uma_mtx);
	LIST_INSERT_HEAD(&uma_kegs, keg, uk_link);
	mtx_unlock(&uma_mtx);
	return (0);
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
	struct uma_zctor_args *arg = udata;
	uma_zone_t zone = mem;
	uma_zone_t z;
	uma_keg_t keg;

	bzero(zone, size);
	zone->uz_name = arg->name;
	zone->uz_ctor = arg->ctor;
	zone->uz_dtor = arg->dtor;
	zone->uz_init = NULL;
	zone->uz_fini = NULL;
	zone->uz_allocs = 0;
	zone->uz_frees = 0;
	zone->uz_fails = 0;
	zone->uz_fills = zone->uz_count = 0;

	if (arg->flags & UMA_ZONE_SECONDARY) {
		KASSERT(arg->keg != NULL, ("Secondary zone on zero'd keg"));
		keg = arg->keg;
		zone->uz_keg = keg;
		zone->uz_init = arg->uminit;
		zone->uz_fini = arg->fini;
		zone->uz_lock = &keg->uk_lock;
		mtx_lock(&uma_mtx);
		ZONE_LOCK(zone);
		keg->uk_flags |= UMA_ZONE_SECONDARY;
		LIST_FOREACH(z, &keg->uk_zones, uz_link) {
			if (LIST_NEXT(z, uz_link) == NULL) {
				LIST_INSERT_AFTER(z, zone, uz_link);
				break;
			}
		}
		ZONE_UNLOCK(zone);
		mtx_unlock(&uma_mtx);
	} else if (arg->keg == NULL) {
		if (uma_kcreate(zone, arg->size, arg->uminit, arg->fini,
		    arg->align, arg->flags) == NULL)
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
	keg = zone->uz_keg;
	zone->uz_lock = &keg->uk_lock;

	/*
	 * Some internal zones don't have room allocated for the per cpu
	 * caches.  If we're internal, bail out here.
	 */
	if (keg->uk_flags & UMA_ZFLAG_INTERNAL) {
		KASSERT((keg->uk_flags & UMA_ZONE_SECONDARY) == 0,
		    ("Secondary zone requested UMA_ZFLAG_INTERNAL"));
		return (0);
	}

	if (keg->uk_flags & UMA_ZONE_MAXBUCKET)
		zone->uz_count = BUCKET_MAX;
	else if (keg->uk_ipers <= BUCKET_MAX)
		zone->uz_count = keg->uk_ipers;
	else
		zone->uz_count = BUCKET_MAX;
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
	mtx_lock(&keg->uk_lock);
	if (keg->uk_free != 0) {
		printf("Freed UMA keg was not empty (%d items). "
		    " Lost %d pages of memory.\n",
		    keg->uk_free, keg->uk_pages);
	}
	mtx_unlock(&keg->uk_lock);

	if (keg->uk_flags & UMA_ZONE_HASH)
		hash_free(&keg->uk_hash);

	mtx_destroy(&keg->uk_lock);
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
	keg = zone->uz_keg;

	if (!(keg->uk_flags & UMA_ZFLAG_INTERNAL))
		cache_drain(zone);

	mtx_lock(&uma_mtx);
	zone_drain(zone);
	if (keg->uk_flags & UMA_ZONE_SECONDARY) {
		LIST_REMOVE(zone, uz_link);
		/*
		 * XXX there are some races here where
		 * the zone can be drained but zone lock
		 * released and then refilled before we
		 * remove it... we dont care for now
		 */
		ZONE_LOCK(zone);
		if (LIST_EMPTY(&keg->uk_zones))
			keg->uk_flags &= ~UMA_ZONE_SECONDARY;
		ZONE_UNLOCK(zone);
		mtx_unlock(&uma_mtx);
	} else {
		LIST_REMOVE(keg, uk_link);
		LIST_REMOVE(zone, uz_link);
		mtx_unlock(&uma_mtx);
		uma_zfree_internal(kegs, keg, NULL, SKIP_NONE,
		    ZFREE_STATFREE);
	}
	zone->uz_keg = NULL;
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
zone_foreach(void (*zfunc)(uma_zone_t))
{
	uma_keg_t keg;
	uma_zone_t zone;

	mtx_lock(&uma_mtx);
	LIST_FOREACH(keg, &uma_kegs, uk_link) {
		LIST_FOREACH(zone, &keg->uk_zones, uz_link)
			zfunc(zone);
	}
	mtx_unlock(&uma_mtx);
}

/* Public functions */
/* See uma.h */
void
uma_startup(void *bootmem)
{
	struct uma_zctor_args args;
	uma_slab_t slab;
	u_int slabsize;
	u_int objsize, totsize, wsize;
	int i;

#ifdef UMA_DEBUG
	printf("Creating uma keg headers zone and keg.\n");
#endif
	/*
	 * The general UMA lock is a recursion-allowed lock because
	 * there is a code path where, while we're still configured
	 * to use startup_alloc() for backend page allocations, we
	 * may end up in uma_reclaim() which calls zone_foreach(zone_drain),
	 * which grabs uma_mtx, only to later call into startup_alloc()
	 * because while freeing we needed to allocate a bucket.  Since
	 * startup_alloc() also takes uma_mtx, we need to be able to
	 * recurse on it.
	 */
	mtx_init(&uma_mtx, "UMA lock", NULL, MTX_DEF | MTX_RECURSE);

	/*
	 * Figure out the maximum number of items-per-slab we'll have if
	 * we're using the OFFPAGE slab header to track free items, given
	 * all possible object sizes and the maximum desired wastage
	 * (UMA_MAX_WASTE).
	 *
	 * We iterate until we find an object size for
	 * which the calculated wastage in zone_small_init() will be
	 * enough to warrant OFFPAGE.  Since wastedspace versus objsize
	 * is an overall increasing see-saw function, we find the smallest
	 * objsize such that the wastage is always acceptable for objects
	 * with that objsize or smaller.  Since a smaller objsize always
	 * generates a larger possible uma_max_ipers, we use this computed
	 * objsize to calculate the largest ipers possible.  Since the
	 * ipers calculated for OFFPAGE slab headers is always larger than
	 * the ipers initially calculated in zone_small_init(), we use
	 * the former's equation (UMA_SLAB_SIZE / keg->uk_rsize) to
	 * obtain the maximum ipers possible for offpage slab headers.
	 *
	 * It should be noted that ipers versus objsize is an inversly
	 * proportional function which drops off rather quickly so as
	 * long as our UMA_MAX_WASTE is such that the objsize we calculate
	 * falls into the portion of the inverse relation AFTER the steep
	 * falloff, then uma_max_ipers shouldn't be too high (~10 on i386).
	 *
	 * Note that we have 8-bits (1 byte) to use as a freelist index
	 * inside the actual slab header itself and this is enough to
	 * accomodate us.  In the worst case, a UMA_SMALLEST_UNIT sized
	 * object with offpage slab header would have ipers =
	 * UMA_SLAB_SIZE / UMA_SMALLEST_UNIT (currently = 256), which is
	 * 1 greater than what our byte-integer freelist index can
	 * accomodate, but we know that this situation never occurs as
	 * for UMA_SMALLEST_UNIT-sized objects, we will never calculate
	 * that we need to go to offpage slab headers.  Or, if we do,
	 * then we trap that condition below and panic in the INVARIANTS case.
	 */
	wsize = UMA_SLAB_SIZE - sizeof(struct uma_slab) - UMA_MAX_WASTE;
	totsize = wsize;
	objsize = UMA_SMALLEST_UNIT;
	while (totsize >= wsize) {
		totsize = (UMA_SLAB_SIZE - sizeof(struct uma_slab)) /
		    (objsize + UMA_FRITM_SZ);
		totsize *= (UMA_FRITM_SZ + objsize);
		objsize++;
	}
	if (objsize > UMA_SMALLEST_UNIT)
		objsize--;
	uma_max_ipers = UMA_SLAB_SIZE / objsize;

	wsize = UMA_SLAB_SIZE - sizeof(struct uma_slab_refcnt) - UMA_MAX_WASTE;
	totsize = wsize;
	objsize = UMA_SMALLEST_UNIT;
	while (totsize >= wsize) {
		totsize = (UMA_SLAB_SIZE - sizeof(struct uma_slab_refcnt)) /
		    (objsize + UMA_FRITMREF_SZ);
		totsize *= (UMA_FRITMREF_SZ + objsize);
		objsize++;
	}
	if (objsize > UMA_SMALLEST_UNIT)
		objsize--;
	uma_max_ipers_ref = UMA_SLAB_SIZE / objsize;

	KASSERT((uma_max_ipers_ref <= 255) && (uma_max_ipers <= 255),
	    ("uma_startup: calculated uma_max_ipers values too large!"));

#ifdef UMA_DEBUG
	printf("Calculated uma_max_ipers (for OFFPAGE) is %d\n", uma_max_ipers);
	printf("Calculated uma_max_ipers_slab (for OFFPAGE) is %d\n",
	    uma_max_ipers_ref);
#endif

	/* "manually" create the initial zone */
	args.name = "UMA Kegs";
	args.size = sizeof(struct uma_keg);
	args.ctor = keg_ctor;
	args.dtor = keg_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.keg = &masterkeg;
	args.align = 32 - 1;
	args.flags = UMA_ZFLAG_INTERNAL;
	/* The initial zone has no Per cpu queues so it's smaller */
	zone_ctor(kegs, sizeof(struct uma_zone), &args, M_WAITOK);

#ifdef UMA_DEBUG
	printf("Filling boot free list.\n");
#endif
	for (i = 0; i < UMA_BOOT_PAGES; i++) {
		slab = (uma_slab_t)((u_int8_t *)bootmem + (i * UMA_SLAB_SIZE));
		slab->us_data = (u_int8_t *)slab;
		slab->us_flags = UMA_SLAB_BOOT;
		LIST_INSERT_HEAD(&uma_boot_pages, slab, us_link);
		uma_boot_free++;
	}

#ifdef UMA_DEBUG
	printf("Creating uma zone headers zone and keg.\n");
#endif
	args.name = "UMA Zones";
	args.size = sizeof(struct uma_zone) +
	    (sizeof(struct uma_cache) * (mp_maxid + 1));
	args.ctor = zone_ctor;
	args.dtor = zone_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.keg = NULL;
	args.align = 32 - 1;
	args.flags = UMA_ZFLAG_INTERNAL;
	/* The initial zone has no Per cpu queues so it's smaller */
	zone_ctor(zones, sizeof(struct uma_zone), &args, M_WAITOK);

#ifdef UMA_DEBUG
	printf("Initializing pcpu cache locks.\n");
#endif
#ifdef UMA_DEBUG
	printf("Creating slab and hash zones.\n");
#endif

	/*
	 * This is the max number of free list items we'll have with
	 * offpage slabs.
	 */
	slabsize = uma_max_ipers * UMA_FRITM_SZ;
	slabsize += sizeof(struct uma_slab);

	/* Now make a zone for slab headers */
	slabzone = uma_zcreate("UMA Slabs",
				slabsize,
				NULL, NULL, NULL, NULL,
				UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);

	/*
	 * We also create a zone for the bigger slabs with reference
	 * counts in them, to accomodate UMA_ZONE_REFCNT zones.
	 */
	slabsize = uma_max_ipers_ref * UMA_FRITMREF_SZ;
	slabsize += sizeof(struct uma_slab_refcnt);
	slabrefzone = uma_zcreate("UMA RCntSlabs",
				  slabsize,
				  NULL, NULL, NULL, NULL,
				  UMA_ALIGN_PTR,
				  UMA_ZFLAG_INTERNAL);

	hashzone = uma_zcreate("UMA Hash",
	    sizeof(struct slabhead *) * UMA_HASH_SIZE_INIT,
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);

	bucket_init();

#ifdef UMA_MD_SMALL_ALLOC
	booted = 1;
#endif

#ifdef UMA_DEBUG
	printf("UMA startup complete.\n");
#endif
}

/* see uma.h */
void
uma_startup2(void)
{
	booted = 1;
	bucket_enable();
#ifdef UMA_DEBUG
	printf("UMA startup2 complete.\n");
#endif
}

/*
 * Initialize our callout handle
 *
 */

static void
uma_startup3(void)
{
#ifdef UMA_DEBUG
	printf("Starting callout.\n");
#endif
	callout_init(&uma_callout, CALLOUT_MPSAFE);
	callout_reset(&uma_callout, UMA_TIMEOUT * hz, uma_timeout, NULL);
#ifdef UMA_DEBUG
	printf("UMA startup3 complete.\n");
#endif
}

static uma_zone_t
uma_kcreate(uma_zone_t zone, size_t size, uma_init uminit, uma_fini fini,
		int align, u_int32_t flags)
{
	struct uma_kctor_args args;

	args.size = size;
	args.uminit = uminit;
	args.fini = fini;
	args.align = align;
	args.flags = flags;
	args.zone = zone;
	return (uma_zalloc_internal(kegs, &args, M_WAITOK));
}

/* See uma.h */
uma_zone_t
uma_zcreate(char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
		uma_init uminit, uma_fini fini, int align, u_int32_t flags)

{
	struct uma_zctor_args args;

	/* This stuff is essential for the zone ctor */
	args.name = name;
	args.size = size;
	args.ctor = ctor;
	args.dtor = dtor;
	args.uminit = uminit;
	args.fini = fini;
	args.align = align;
	args.flags = flags;
	args.keg = NULL;

	return (uma_zalloc_internal(zones, &args, M_WAITOK));
}

/* See uma.h */
uma_zone_t
uma_zsecond_create(char *name, uma_ctor ctor, uma_dtor dtor,
		    uma_init zinit, uma_fini zfini, uma_zone_t master)
{
	struct uma_zctor_args args;

	args.name = name;
	args.size = master->uz_keg->uk_size;
	args.ctor = ctor;
	args.dtor = dtor;
	args.uminit = zinit;
	args.fini = zfini;
	args.align = master->uz_keg->uk_align;
	args.flags = master->uz_keg->uk_flags | UMA_ZONE_SECONDARY;
	args.keg = master->uz_keg;

	return (uma_zalloc_internal(zones, &args, M_WAITOK));
}

/* See uma.h */
void
uma_zdestroy(uma_zone_t zone)
{

	uma_zfree_internal(zones, zone, NULL, SKIP_NONE, ZFREE_STATFREE);
}

/* See uma.h */
void *
uma_zalloc_arg(uma_zone_t zone, void *udata, int flags)
{
	void *item;
	uma_cache_t cache;
	uma_bucket_t bucket;
	int cpu;
	int badness;

	/* This is the fast path allocation */
#ifdef UMA_DEBUG_ALLOC_1
	printf("Allocating one item from %s(%p)\n", zone->uz_name, zone);
#endif
	CTR3(KTR_UMA, "uma_zalloc_arg thread %x zone %s flags %d", curthread,
	    zone->uz_name, flags);

	if (!(flags & M_NOWAIT)) {
		KASSERT(curthread->td_intr_nesting_level == 0,
		   ("malloc(M_WAITOK) in interrupt context"));
		if (nosleepwithlocks) {
#ifdef WITNESS
			badness = WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
			    NULL,
			    "malloc(M_WAITOK) of \"%s\", forcing M_NOWAIT",
			    zone->uz_name);
#else
			badness = 1;
#endif
		} else {
			badness = 0;
#ifdef WITNESS
			WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
			    "malloc(M_WAITOK) of \"%s\"", zone->uz_name);
#endif
		}
		if (badness) {
			flags &= ~M_WAITOK;
			flags |= M_NOWAIT;
		}
	}

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
zalloc_restart:
	critical_enter();
	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];

zalloc_start:
	bucket = cache->uc_allocbucket;

	if (bucket) {
		if (bucket->ub_cnt > 0) {
			bucket->ub_cnt--;
			item = bucket->ub_bucket[bucket->ub_cnt];
#ifdef INVARIANTS
			bucket->ub_bucket[bucket->ub_cnt] = NULL;
#endif
			KASSERT(item != NULL,
			    ("uma_zalloc: Bucket pointer mangled."));
			cache->uc_allocs++;
			critical_exit();
#ifdef INVARIANTS
			ZONE_LOCK(zone);
			uma_dbg_alloc(zone, NULL, item);
			ZONE_UNLOCK(zone);
#endif
			if (zone->uz_ctor != NULL) {
				if (zone->uz_ctor(item, zone->uz_keg->uk_size,
				    udata, flags) != 0) {
					uma_zfree_internal(zone, item, udata,
					    SKIP_DTOR, ZFREE_STATFAIL |
					    ZFREE_STATFREE);
					return (NULL);
				}
			}
			if (flags & M_ZERO)
				bzero(item, zone->uz_keg->uk_size);
			return (item);
		} else if (cache->uc_freebucket) {
			/*
			 * We have run out of items in our allocbucket.
			 * See if we can switch with our free bucket.
			 */
			if (cache->uc_freebucket->ub_cnt > 0) {
#ifdef UMA_DEBUG_ALLOC
				printf("uma_zalloc: Swapping empty with"
				    " alloc.\n");
#endif
				bucket = cache->uc_freebucket;
				cache->uc_freebucket = cache->uc_allocbucket;
				cache->uc_allocbucket = bucket;

				goto zalloc_start;
			}
		}
	}
	/*
	 * Attempt to retrieve the item from the per-CPU cache has failed, so
	 * we must go back to the zone.  This requires the zone lock, so we
	 * must drop the critical section, then re-acquire it when we go back
	 * to the cache.  Since the critical section is released, we may be
	 * preempted or migrate.  As such, make sure not to maintain any
	 * thread-local state specific to the cache from prior to releasing
	 * the critical section.
	 */
	critical_exit();
	ZONE_LOCK(zone);
	critical_enter();
	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];
	bucket = cache->uc_allocbucket;
	if (bucket != NULL) {
		if (bucket->ub_cnt > 0) {
			ZONE_UNLOCK(zone);
			goto zalloc_start;
		}
		bucket = cache->uc_freebucket;
		if (bucket != NULL && bucket->ub_cnt > 0) {
			ZONE_UNLOCK(zone);
			goto zalloc_start;
		}
	}

	/* Since we have locked the zone we may as well send back our stats */
	zone->uz_allocs += cache->uc_allocs;
	cache->uc_allocs = 0;
	zone->uz_frees += cache->uc_frees;
	cache->uc_frees = 0;

	/* Our old one is now a free bucket */
	if (cache->uc_allocbucket) {
		KASSERT(cache->uc_allocbucket->ub_cnt == 0,
		    ("uma_zalloc_arg: Freeing a non free bucket."));
		LIST_INSERT_HEAD(&zone->uz_free_bucket,
		    cache->uc_allocbucket, ub_link);
		cache->uc_allocbucket = NULL;
	}

	/* Check the free list for a new alloc bucket */
	if ((bucket = LIST_FIRST(&zone->uz_full_bucket)) != NULL) {
		KASSERT(bucket->ub_cnt != 0,
		    ("uma_zalloc_arg: Returning an empty bucket."));

		LIST_REMOVE(bucket, ub_link);
		cache->uc_allocbucket = bucket;
		ZONE_UNLOCK(zone);
		goto zalloc_start;
	}
	/* We are no longer associated with this CPU. */
	critical_exit();

	/* Bump up our uz_count so we get here less */
	if (zone->uz_count < BUCKET_MAX)
		zone->uz_count++;

	/*
	 * Now lets just fill a bucket and put it on the free list.  If that
	 * works we'll restart the allocation from the begining.
	 */
	if (uma_zalloc_bucket(zone, flags)) {
		ZONE_UNLOCK(zone);
		goto zalloc_restart;
	}
	ZONE_UNLOCK(zone);
	/*
	 * We may not be able to get a bucket so return an actual item.
	 */
#ifdef UMA_DEBUG
	printf("uma_zalloc_arg: Bucketzone returned NULL\n");
#endif

	return (uma_zalloc_internal(zone, udata, flags));
}

static uma_slab_t
uma_zone_slab(uma_zone_t zone, int flags)
{
	uma_slab_t slab;
	uma_keg_t keg;

	keg = zone->uz_keg;

	/*
	 * This is to prevent us from recursively trying to allocate
	 * buckets.  The problem is that if an allocation forces us to
	 * grab a new bucket we will call page_alloc, which will go off
	 * and cause the vm to allocate vm_map_entries.  If we need new
	 * buckets there too we will recurse in kmem_alloc and bad
	 * things happen.  So instead we return a NULL bucket, and make
	 * the code that allocates buckets smart enough to deal with it
	 *
	 * XXX: While we want this protection for the bucket zones so that
	 * recursion from the VM is handled (and the calling code that
	 * allocates buckets knows how to deal with it), we do not want
	 * to prevent allocation from the slab header zones (slabzone
	 * and slabrefzone) if uk_recurse is not zero for them.  The
	 * reason is that it could lead to NULL being returned for
	 * slab header allocations even in the M_WAITOK case, and the
	 * caller can't handle that. 
	 */
	if (keg->uk_flags & UMA_ZFLAG_INTERNAL && keg->uk_recurse != 0)
		if ((zone != slabzone) && (zone != slabrefzone))
			return (NULL);

	slab = NULL;

	for (;;) {
		/*
		 * Find a slab with some space.  Prefer slabs that are partially
		 * used over those that are totally full.  This helps to reduce
		 * fragmentation.
		 */
		if (keg->uk_free != 0) {
			if (!LIST_EMPTY(&keg->uk_part_slab)) {
				slab = LIST_FIRST(&keg->uk_part_slab);
			} else {
				slab = LIST_FIRST(&keg->uk_free_slab);
				LIST_REMOVE(slab, us_link);
				LIST_INSERT_HEAD(&keg->uk_part_slab, slab,
				    us_link);
			}
			return (slab);
		}

		/*
		 * M_NOVM means don't ask at all!
		 */
		if (flags & M_NOVM)
			break;

		if (keg->uk_maxpages &&
		    keg->uk_pages >= keg->uk_maxpages) {
			keg->uk_flags |= UMA_ZFLAG_FULL;

			if (flags & M_NOWAIT)
				break;
			else
				msleep(keg, &keg->uk_lock, PVM,
				    "zonelimit", 0);
			continue;
		}
		keg->uk_recurse++;
		slab = slab_zalloc(zone, flags);
		keg->uk_recurse--;

		/*
		 * If we got a slab here it's safe to mark it partially used
		 * and return.  We assume that the caller is going to remove
		 * at least one item.
		 */
		if (slab) {
			LIST_INSERT_HEAD(&keg->uk_part_slab, slab, us_link);
			return (slab);
		}
		/*
		 * We might not have been able to get a slab but another cpu
		 * could have while we were unlocked.  Check again before we
		 * fail.
		 */
		if (flags & M_NOWAIT)
			flags |= M_NOVM;
	}
	return (slab);
}

static void *
uma_slab_alloc(uma_zone_t zone, uma_slab_t slab)
{
	uma_keg_t keg;
	uma_slabrefcnt_t slabref;
	void *item;
	u_int8_t freei;

	keg = zone->uz_keg;

	freei = slab->us_firstfree;
	if (keg->uk_flags & UMA_ZONE_REFCNT) {
		slabref = (uma_slabrefcnt_t)slab;
		slab->us_firstfree = slabref->us_freelist[freei].us_item;
	} else {
		slab->us_firstfree = slab->us_freelist[freei].us_item;
	}
	item = slab->us_data + (keg->uk_rsize * freei);

	slab->us_freecount--;
	keg->uk_free--;
#ifdef INVARIANTS
	uma_dbg_alloc(zone, slab, item);
#endif
	/* Move this slab to the full list */
	if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&keg->uk_full_slab, slab, us_link);
	}

	return (item);
}

static int
uma_zalloc_bucket(uma_zone_t zone, int flags)
{
	uma_bucket_t bucket;
	uma_slab_t slab;
	int16_t saved;
	int max, origflags = flags;

	/*
	 * Try this zone's free list first so we don't allocate extra buckets.
	 */
	if ((bucket = LIST_FIRST(&zone->uz_free_bucket)) != NULL) {
		KASSERT(bucket->ub_cnt == 0,
		    ("uma_zalloc_bucket: Bucket on free list is not empty."));
		LIST_REMOVE(bucket, ub_link);
	} else {
		int bflags;

		bflags = (flags & ~M_ZERO);
		if (zone->uz_keg->uk_flags & UMA_ZFLAG_CACHEONLY)
			bflags |= M_NOVM;

		ZONE_UNLOCK(zone);
		bucket = bucket_alloc(zone->uz_count, bflags);
		ZONE_LOCK(zone);
	}

	if (bucket == NULL)
		return (0);

#ifdef SMP
	/*
	 * This code is here to limit the number of simultaneous bucket fills
	 * for any given zone to the number of per cpu caches in this zone. This
	 * is done so that we don't allocate more memory than we really need.
	 */
	if (zone->uz_fills >= mp_ncpus)
		goto done;

#endif
	zone->uz_fills++;

	max = MIN(bucket->ub_entries, zone->uz_count);
	/* Try to keep the buckets totally full */
	saved = bucket->ub_cnt;
	while (bucket->ub_cnt < max &&
	    (slab = uma_zone_slab(zone, flags)) != NULL) {
		while (slab->us_freecount && bucket->ub_cnt < max) {
			bucket->ub_bucket[bucket->ub_cnt++] =
			    uma_slab_alloc(zone, slab);
		}

		/* Don't block on the next fill */
		flags |= M_NOWAIT;
	}

	/*
	 * We unlock here because we need to call the zone's init.
	 * It should be safe to unlock because the slab dealt with
	 * above is already on the appropriate list within the keg
	 * and the bucket we filled is not yet on any list, so we
	 * own it.
	 */
	if (zone->uz_init != NULL) {
		int i;

		ZONE_UNLOCK(zone);
		for (i = saved; i < bucket->ub_cnt; i++)
			if (zone->uz_init(bucket->ub_bucket[i],
			    zone->uz_keg->uk_size, origflags) != 0)
				break;
		/*
		 * If we couldn't initialize the whole bucket, put the
		 * rest back onto the freelist.
		 */
		if (i != bucket->ub_cnt) {
			int j;

			for (j = i; j < bucket->ub_cnt; j++) {
				uma_zfree_internal(zone, bucket->ub_bucket[j],
				    NULL, SKIP_FINI, 0);
#ifdef INVARIANTS
				bucket->ub_bucket[j] = NULL;
#endif
			}
			bucket->ub_cnt = i;
		}
		ZONE_LOCK(zone);
	}

	zone->uz_fills--;
	if (bucket->ub_cnt != 0) {
		LIST_INSERT_HEAD(&zone->uz_full_bucket,
		    bucket, ub_link);
		return (1);
	}
#ifdef SMP
done:
#endif
	bucket_free(bucket);

	return (0);
}
/*
 * Allocates an item for an internal zone
 *
 * Arguments
 *	zone   The zone to alloc for.
 *	udata  The data to be passed to the constructor.
 *	flags  M_WAITOK, M_NOWAIT, M_ZERO.
 *
 * Returns
 *	NULL if there is no memory and M_NOWAIT is set
 *	An item if successful
 */

static void *
uma_zalloc_internal(uma_zone_t zone, void *udata, int flags)
{
	uma_keg_t keg;
	uma_slab_t slab;
	void *item;

	item = NULL;
	keg = zone->uz_keg;

#ifdef UMA_DEBUG_ALLOC
	printf("INTERNAL: Allocating one item from %s(%p)\n", zone->uz_name, zone);
#endif
	ZONE_LOCK(zone);

	slab = uma_zone_slab(zone, flags);
	if (slab == NULL) {
		zone->uz_fails++;
		ZONE_UNLOCK(zone);
		return (NULL);
	}

	item = uma_slab_alloc(zone, slab);

	zone->uz_allocs++;

	ZONE_UNLOCK(zone);

	/*
	 * We have to call both the zone's init (not the keg's init)
	 * and the zone's ctor.  This is because the item is going from
	 * a keg slab directly to the user, and the user is expecting it
	 * to be both zone-init'd as well as zone-ctor'd.
	 */
	if (zone->uz_init != NULL) {
		if (zone->uz_init(item, keg->uk_size, flags) != 0) {
			uma_zfree_internal(zone, item, udata, SKIP_FINI,
			    ZFREE_STATFAIL | ZFREE_STATFREE);
			return (NULL);
		}
	}
	if (zone->uz_ctor != NULL) {
		if (zone->uz_ctor(item, keg->uk_size, udata, flags) != 0) {
			uma_zfree_internal(zone, item, udata, SKIP_DTOR,
			    ZFREE_STATFAIL | ZFREE_STATFREE);
			return (NULL);
		}
	}
	if (flags & M_ZERO)
		bzero(item, keg->uk_size);

	return (item);
}

/* See uma.h */
void
uma_zfree_arg(uma_zone_t zone, void *item, void *udata)
{
	uma_keg_t keg;
	uma_cache_t cache;
	uma_bucket_t bucket;
	int bflags;
	int cpu;

	keg = zone->uz_keg;

#ifdef UMA_DEBUG_ALLOC_1
	printf("Freeing item %p to %s(%p)\n", item, zone->uz_name, zone);
#endif
	CTR2(KTR_UMA, "uma_zfree_arg thread %x zone %s", curthread,
	    zone->uz_name);

	if (zone->uz_dtor)
		zone->uz_dtor(item, keg->uk_size, udata);
#ifdef INVARIANTS
	ZONE_LOCK(zone);
	if (keg->uk_flags & UMA_ZONE_MALLOC)
		uma_dbg_free(zone, udata, item);
	else
		uma_dbg_free(zone, NULL, item);
	ZONE_UNLOCK(zone);
#endif
	/*
	 * The race here is acceptable.  If we miss it we'll just have to wait
	 * a little longer for the limits to be reset.
	 */
	if (keg->uk_flags & UMA_ZFLAG_FULL)
		goto zfree_internal;

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
zfree_restart:
	critical_enter();
	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];

zfree_start:
	bucket = cache->uc_freebucket;

	if (bucket) {
		/*
		 * Do we have room in our bucket? It is OK for this uz count
		 * check to be slightly out of sync.
		 */

		if (bucket->ub_cnt < bucket->ub_entries) {
			KASSERT(bucket->ub_bucket[bucket->ub_cnt] == NULL,
			    ("uma_zfree: Freeing to non free bucket index."));
			bucket->ub_bucket[bucket->ub_cnt] = item;
			bucket->ub_cnt++;
			cache->uc_frees++;
			critical_exit();
			return;
		} else if (cache->uc_allocbucket) {
#ifdef UMA_DEBUG_ALLOC
			printf("uma_zfree: Swapping buckets.\n");
#endif
			/*
			 * We have run out of space in our freebucket.
			 * See if we can switch with our alloc bucket.
			 */
			if (cache->uc_allocbucket->ub_cnt <
			    cache->uc_freebucket->ub_cnt) {
				bucket = cache->uc_freebucket;
				cache->uc_freebucket = cache->uc_allocbucket;
				cache->uc_allocbucket = bucket;
				goto zfree_start;
			}
		}
	}
	/*
	 * We can get here for two reasons:
	 *
	 * 1) The buckets are NULL
	 * 2) The alloc and free buckets are both somewhat full.
	 *
	 * We must go back the zone, which requires acquiring the zone lock,
	 * which in turn means we must release and re-acquire the critical
	 * section.  Since the critical section is released, we may be
	 * preempted or migrate.  As such, make sure not to maintain any
	 * thread-local state specific to the cache from prior to releasing
	 * the critical section.
	 */
	critical_exit();
	ZONE_LOCK(zone);
	critical_enter();
	cpu = curcpu;
	cache = &zone->uz_cpu[cpu];
	if (cache->uc_freebucket != NULL) {
		if (cache->uc_freebucket->ub_cnt <
		    cache->uc_freebucket->ub_entries) {
			ZONE_UNLOCK(zone);
			goto zfree_start;
		}
		if (cache->uc_allocbucket != NULL &&
		    (cache->uc_allocbucket->ub_cnt <
		    cache->uc_freebucket->ub_cnt)) {
			ZONE_UNLOCK(zone);
			goto zfree_start;
		}
	}

	/* Since we have locked the zone we may as well send back our stats */
	zone->uz_allocs += cache->uc_allocs;
	cache->uc_allocs = 0;
	zone->uz_frees += cache->uc_frees;
	cache->uc_frees = 0;

	bucket = cache->uc_freebucket;
	cache->uc_freebucket = NULL;

	/* Can we throw this on the zone full list? */
	if (bucket != NULL) {
#ifdef UMA_DEBUG_ALLOC
		printf("uma_zfree: Putting old bucket on the free list.\n");
#endif
		/* ub_cnt is pointing to the last free item */
		KASSERT(bucket->ub_cnt != 0,
		    ("uma_zfree: Attempting to insert an empty bucket onto the full list.\n"));
		LIST_INSERT_HEAD(&zone->uz_full_bucket,
		    bucket, ub_link);
	}
	if ((bucket = LIST_FIRST(&zone->uz_free_bucket)) != NULL) {
		LIST_REMOVE(bucket, ub_link);
		ZONE_UNLOCK(zone);
		cache->uc_freebucket = bucket;
		goto zfree_start;
	}
	/* We are no longer associated with this CPU. */
	critical_exit();

	/* And the zone.. */
	ZONE_UNLOCK(zone);

#ifdef UMA_DEBUG_ALLOC
	printf("uma_zfree: Allocating new free bucket.\n");
#endif
	bflags = M_NOWAIT;

	if (keg->uk_flags & UMA_ZFLAG_CACHEONLY)
		bflags |= M_NOVM;
	bucket = bucket_alloc(zone->uz_count, bflags);
	if (bucket) {
		ZONE_LOCK(zone);
		LIST_INSERT_HEAD(&zone->uz_free_bucket,
		    bucket, ub_link);
		ZONE_UNLOCK(zone);
		goto zfree_restart;
	}

	/*
	 * If nothing else caught this, we'll just do an internal free.
	 */
zfree_internal:
	uma_zfree_internal(zone, item, udata, SKIP_DTOR, ZFREE_STATFAIL |
	    ZFREE_STATFREE);

	return;
}

/*
 * Frees an item to an INTERNAL zone or allocates a free bucket
 *
 * Arguments:
 *	zone   The zone to free to
 *	item   The item we're freeing
 *	udata  User supplied data for the dtor
 *	skip   Skip dtors and finis
 */
static void
uma_zfree_internal(uma_zone_t zone, void *item, void *udata,
    enum zfreeskip skip, int flags)
{
	uma_slab_t slab;
	uma_slabrefcnt_t slabref;
	uma_keg_t keg;
	u_int8_t *mem;
	u_int8_t freei;

	keg = zone->uz_keg;

	if (skip < SKIP_DTOR && zone->uz_dtor)
		zone->uz_dtor(item, keg->uk_size, udata);
	if (skip < SKIP_FINI && zone->uz_fini)
		zone->uz_fini(item, keg->uk_size);

	ZONE_LOCK(zone);

	if (flags & ZFREE_STATFAIL)
		zone->uz_fails++;
	if (flags & ZFREE_STATFREE)
		zone->uz_frees++;

	if (!(keg->uk_flags & UMA_ZONE_MALLOC)) {
		mem = (u_int8_t *)((unsigned long)item & (~UMA_SLAB_MASK));
		if (keg->uk_flags & UMA_ZONE_HASH)
			slab = hash_sfind(&keg->uk_hash, mem);
		else {
			mem += keg->uk_pgoff;
			slab = (uma_slab_t)mem;
		}
	} else {
		slab = (uma_slab_t)udata;
	}

	/* Do we need to remove from any lists? */
	if (slab->us_freecount+1 == keg->uk_ipers) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&keg->uk_free_slab, slab, us_link);
	} else if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&keg->uk_part_slab, slab, us_link);
	}

	/* Slab management stuff */
	freei = ((unsigned long)item - (unsigned long)slab->us_data)
		/ keg->uk_rsize;

#ifdef INVARIANTS
	if (!skip)
		uma_dbg_free(zone, slab, item);
#endif

	if (keg->uk_flags & UMA_ZONE_REFCNT) {
		slabref = (uma_slabrefcnt_t)slab;
		slabref->us_freelist[freei].us_item = slab->us_firstfree;
	} else {
		slab->us_freelist[freei].us_item = slab->us_firstfree;
	}
	slab->us_firstfree = freei;
	slab->us_freecount++;

	/* Zone statistics */
	keg->uk_free++;

	if (keg->uk_flags & UMA_ZFLAG_FULL) {
		if (keg->uk_pages < keg->uk_maxpages)
			keg->uk_flags &= ~UMA_ZFLAG_FULL;

		/* We can handle one more allocation */
		wakeup_one(keg);
	}

	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_max(uma_zone_t zone, int nitems)
{
	uma_keg_t keg;

	keg = zone->uz_keg;
	ZONE_LOCK(zone);
	if (keg->uk_ppera > 1)
		keg->uk_maxpages = nitems * keg->uk_ppera;
	else
		keg->uk_maxpages = nitems / keg->uk_ipers;

	if (keg->uk_maxpages * keg->uk_ipers < nitems)
		keg->uk_maxpages++;

	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_init(uma_zone_t zone, uma_init uminit)
{
	ZONE_LOCK(zone);
	KASSERT(zone->uz_keg->uk_pages == 0,
	    ("uma_zone_set_init on non-empty keg"));
	zone->uz_keg->uk_init = uminit;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_fini(uma_zone_t zone, uma_fini fini)
{
	ZONE_LOCK(zone);
	KASSERT(zone->uz_keg->uk_pages == 0,
	    ("uma_zone_set_fini on non-empty keg"));
	zone->uz_keg->uk_fini = fini;
	ZONE_UNLOCK(zone);
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
	ZONE_LOCK(zone);
	zone->uz_keg->uk_freef = freef;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
/* XXX uk_allocf is not actually used with the zone locked */
void
uma_zone_set_allocf(uma_zone_t zone, uma_alloc allocf)
{
	ZONE_LOCK(zone);
	zone->uz_keg->uk_flags |= UMA_ZFLAG_PRIVALLOC;
	zone->uz_keg->uk_allocf = allocf;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
int
uma_zone_set_obj(uma_zone_t zone, struct vm_object *obj, int count)
{
	uma_keg_t keg;
	vm_offset_t kva;
	int pages;

	keg = zone->uz_keg;
	pages = count / keg->uk_ipers;

	if (pages * keg->uk_ipers < count)
		pages++;

	kva = kmem_alloc_nofault(kernel_map, pages * UMA_SLAB_SIZE);

	if (kva == 0)
		return (0);
	if (obj == NULL) {
		obj = vm_object_allocate(OBJT_DEFAULT,
		    pages);
	} else {
		VM_OBJECT_LOCK_INIT(obj, "uma object");
		_vm_object_allocate(OBJT_DEFAULT,
		    pages, obj);
	}
	ZONE_LOCK(zone);
	keg->uk_kva = kva;
	keg->uk_obj = obj;
	keg->uk_maxpages = pages;
	keg->uk_allocf = obj_alloc;
	keg->uk_flags |= UMA_ZONE_NOFREE | UMA_ZFLAG_PRIVALLOC;
	ZONE_UNLOCK(zone);
	return (1);
}

/* See uma.h */
void
uma_prealloc(uma_zone_t zone, int items)
{
	int slabs;
	uma_slab_t slab;
	uma_keg_t keg;

	keg = zone->uz_keg;
	ZONE_LOCK(zone);
	slabs = items / keg->uk_ipers;
	if (slabs * keg->uk_ipers < items)
		slabs++;
	while (slabs > 0) {
		slab = slab_zalloc(zone, M_WAITOK);
		LIST_INSERT_HEAD(&keg->uk_free_slab, slab, us_link);
		slabs--;
	}
	ZONE_UNLOCK(zone);
}

/* See uma.h */
u_int32_t *
uma_find_refcnt(uma_zone_t zone, void *item)
{
	uma_slabrefcnt_t slabref;
	uma_keg_t keg;
	u_int32_t *refcnt;
	int idx;

	keg = zone->uz_keg;
	slabref = (uma_slabrefcnt_t)vtoslab((vm_offset_t)item &
	    (~UMA_SLAB_MASK));
	KASSERT(slabref != NULL && slabref->us_keg->uk_flags & UMA_ZONE_REFCNT,
	    ("uma_find_refcnt(): zone possibly not UMA_ZONE_REFCNT"));
	idx = ((unsigned long)item - (unsigned long)slabref->us_data)
	    / keg->uk_rsize;
	refcnt = &slabref->us_freelist[idx].us_refcnt;
	return refcnt;
}

/* See uma.h */
void
uma_reclaim(void)
{
#ifdef UMA_DEBUG
	printf("UMA: vm asked us to release pages!\n");
#endif
	bucket_enable();
	zone_foreach(zone_drain);
	/*
	 * Some slabs may have been freed but this zone will be visited early
	 * we visit again so that we can free pages that are empty once other
	 * zones are drained.  We have to do the same for buckets.
	 */
	zone_drain(slabzone);
	zone_drain(slabrefzone);
	bucket_zone_drain();
}

void *
uma_large_malloc(int size, int wait)
{
	void *mem;
	uma_slab_t slab;
	u_int8_t flags;

	slab = uma_zalloc_internal(slabzone, NULL, wait);
	if (slab == NULL)
		return (NULL);
	mem = page_alloc(NULL, size, &flags, wait);
	if (mem) {
		vsetslab((vm_offset_t)mem, slab);
		slab->us_data = mem;
		slab->us_flags = flags | UMA_SLAB_MALLOC;
		slab->us_size = size;
	} else {
		uma_zfree_internal(slabzone, slab, NULL, SKIP_NONE,
		    ZFREE_STATFAIL | ZFREE_STATFREE);
	}

	return (mem);
}

void
uma_large_free(uma_slab_t slab)
{
	vsetobj((vm_offset_t)slab->us_data, kmem_object);
	page_free(slab->us_data, slab->us_size, slab->us_flags);
	uma_zfree_internal(slabzone, slab, NULL, SKIP_NONE, ZFREE_STATFREE);
}

void
uma_print_stats(void)
{
	zone_foreach(uma_print_zone);
}

static void
slab_print(uma_slab_t slab)
{
	printf("slab: keg %p, data %p, freecount %d, firstfree %d\n",
		slab->us_keg, slab->us_data, slab->us_freecount,
		slab->us_firstfree);
}

static void
cache_print(uma_cache_t cache)
{
	printf("alloc: %p(%d), free: %p(%d)\n",
		cache->uc_allocbucket,
		cache->uc_allocbucket?cache->uc_allocbucket->ub_cnt:0,
		cache->uc_freebucket,
		cache->uc_freebucket?cache->uc_freebucket->ub_cnt:0);
}

void
uma_print_zone(uma_zone_t zone)
{
	uma_cache_t cache;
	uma_keg_t keg;
	uma_slab_t slab;
	int i;

	keg = zone->uz_keg;
	printf("%s(%p) size %d(%d) flags %d ipers %d ppera %d out %d free %d\n",
	    zone->uz_name, zone, keg->uk_size, keg->uk_rsize, keg->uk_flags,
	    keg->uk_ipers, keg->uk_ppera,
	    (keg->uk_ipers * keg->uk_pages) - keg->uk_free, keg->uk_free);
	printf("Part slabs:\n");
	LIST_FOREACH(slab, &keg->uk_part_slab, us_link)
		slab_print(slab);
	printf("Free slabs:\n");
	LIST_FOREACH(slab, &keg->uk_free_slab, us_link)
		slab_print(slab);
	printf("Full slabs:\n");
	LIST_FOREACH(slab, &keg->uk_full_slab, us_link)
		slab_print(slab);
	for (i = 0; i <= mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		cache = &zone->uz_cpu[i];
		printf("CPU %d Cache:\n", i);
		cache_print(cache);
	}
}

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
uma_zone_sumstat(uma_zone_t z, int *cachefreep, u_int64_t *allocsp,
    u_int64_t *freesp)
{
	uma_cache_t cache;
	u_int64_t allocs, frees;
	int cachefree, cpu;

	allocs = frees = 0;
	cachefree = 0;
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		cache = &z->uz_cpu[cpu];
		if (cache->uc_allocbucket != NULL)
			cachefree += cache->uc_allocbucket->ub_cnt;
		if (cache->uc_freebucket != NULL)
			cachefree += cache->uc_freebucket->ub_cnt;
		allocs += cache->uc_allocs;
		frees += cache->uc_frees;
	}
	allocs += z->uz_allocs;
	frees += z->uz_frees;
	if (cachefreep != NULL)
		*cachefreep = cachefree;
	if (allocsp != NULL)
		*allocsp = allocs;
	if (freesp != NULL)
		*freesp = frees;
}

/*
 * Sysctl handler for vm.zone
 *
 * stolen from vm_zone.c
 */
static int
sysctl_vm_zone(SYSCTL_HANDLER_ARGS)
{
	int error, len, cnt;
	const int linesize = 128;	/* conservative */
	int totalfree;
	char *tmpbuf, *offset;
	uma_zone_t z;
	uma_keg_t zk;
	char *p;
	int cachefree;
	uma_bucket_t bucket;
	u_int64_t allocs, frees;

	cnt = 0;
	mtx_lock(&uma_mtx);
	LIST_FOREACH(zk, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &zk->uk_zones, uz_link)
			cnt++;
	}
	mtx_unlock(&uma_mtx);
	MALLOC(tmpbuf, char *, (cnt == 0 ? 1 : cnt) * linesize,
			M_TEMP, M_WAITOK);
	len = snprintf(tmpbuf, linesize,
	    "\nITEM            SIZE     LIMIT     USED    FREE  REQUESTS\n\n");
	if (cnt == 0)
		tmpbuf[len - 1] = '\0';
	error = SYSCTL_OUT(req, tmpbuf, cnt == 0 ? len-1 : len);
	if (error || cnt == 0)
		goto out;
	offset = tmpbuf;
	mtx_lock(&uma_mtx);
	LIST_FOREACH(zk, &uma_kegs, uk_link) {
	  LIST_FOREACH(z, &zk->uk_zones, uz_link) {
		if (cnt == 0)	/* list may have changed size */
			break;
		ZONE_LOCK(z);
		cachefree = 0;
		if (!(zk->uk_flags & UMA_ZFLAG_INTERNAL)) {
			uma_zone_sumstat(z, &cachefree, &allocs, &frees);
		} else {
			allocs = z->uz_allocs;
			frees = z->uz_frees;
		}

		LIST_FOREACH(bucket, &z->uz_full_bucket, ub_link) {
			cachefree += bucket->ub_cnt;
		}
		totalfree = zk->uk_free + cachefree;
		len = snprintf(offset, linesize,
		    "%-12.12s  %6.6u, %8.8u, %6.6u, %6.6u, %8.8llu\n",
		    z->uz_name, zk->uk_size,
		    zk->uk_maxpages * zk->uk_ipers,
		    (zk->uk_ipers * (zk->uk_pages / zk->uk_ppera)) - totalfree,
		    totalfree,
		    (unsigned long long)allocs);
		ZONE_UNLOCK(z);
		for (p = offset + 12; p > offset && *p == ' '; --p)
			/* nothing */ ;
		p[1] = ':';
		cnt--;
		offset += len;
	  }
	}
	mtx_unlock(&uma_mtx);
	*offset++ = '\0';
	error = SYSCTL_OUT(req, tmpbuf, offset - tmpbuf);
out:
	FREE(tmpbuf, M_TEMP);
	return (error);
}

static int
sysctl_vm_zone_count(SYSCTL_HANDLER_ARGS)
{
	uma_keg_t kz;
	uma_zone_t z;
	int count;

	count = 0;
	mtx_lock(&uma_mtx);
	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link)
			count++;
	}
	mtx_unlock(&uma_mtx);
	return (sysctl_handle_int(oidp, &count, 0, req));
}

static int
sysctl_vm_zone_stats(SYSCTL_HANDLER_ARGS)
{
	struct uma_stream_header ush;
	struct uma_type_header uth;
	struct uma_percpu_stat ups;
	uma_bucket_t bucket;
	struct sbuf sbuf;
	uma_cache_t cache;
	uma_keg_t kz;
	uma_zone_t z;
	char *buffer;
	int buflen, count, error, i;

	mtx_lock(&uma_mtx);
restart:
	mtx_assert(&uma_mtx, MA_OWNED);
	count = 0;
	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link)
			count++;
	}
	mtx_unlock(&uma_mtx);

	buflen = sizeof(ush) + count * (sizeof(uth) + sizeof(ups) *
	    (mp_maxid + 1)) + 1;
	buffer = malloc(buflen, M_TEMP, M_WAITOK | M_ZERO);

	mtx_lock(&uma_mtx);
	i = 0;
	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link)
			i++;
	}
	if (i > count) {
		free(buffer, M_TEMP);
		goto restart;
	}
	count =  i;

	sbuf_new(&sbuf, buffer, buflen, SBUF_FIXEDLEN);

	/*
	 * Insert stream header.
	 */
	bzero(&ush, sizeof(ush));
	ush.ush_version = UMA_STREAM_VERSION;
	ush.ush_maxcpus = (mp_maxid + 1);
	ush.ush_count = count;
	if (sbuf_bcat(&sbuf, &ush, sizeof(ush)) < 0) {
		mtx_unlock(&uma_mtx);
		error = ENOMEM;
		goto out;
	}

	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link) {
			bzero(&uth, sizeof(uth));
			ZONE_LOCK(z);
			strlcpy(uth.uth_name, z->uz_name, UTH_MAX_NAME);
			uth.uth_align = kz->uk_align;
			uth.uth_pages = kz->uk_pages;
			uth.uth_keg_free = kz->uk_free;
			uth.uth_size = kz->uk_size;
			uth.uth_rsize = kz->uk_rsize;
			uth.uth_maxpages = kz->uk_maxpages;
			if (kz->uk_ppera > 1)
				uth.uth_limit = kz->uk_maxpages /
				    kz->uk_ppera;
			else
				uth.uth_limit = kz->uk_maxpages *
				    kz->uk_ipers;

			/*
			 * A zone is secondary is it is not the first entry
			 * on the keg's zone list.
			 */
			if ((kz->uk_flags & UMA_ZONE_SECONDARY) &&
			    (LIST_FIRST(&kz->uk_zones) != z))
				uth.uth_zone_flags = UTH_ZONE_SECONDARY;

			LIST_FOREACH(bucket, &z->uz_full_bucket, ub_link)
				uth.uth_zone_free += bucket->ub_cnt;
			uth.uth_allocs = z->uz_allocs;
			uth.uth_frees = z->uz_frees;
			uth.uth_fails = z->uz_fails;
			if (sbuf_bcat(&sbuf, &uth, sizeof(uth)) < 0) {
				ZONE_UNLOCK(z);
				mtx_unlock(&uma_mtx);
				error = ENOMEM;
				goto out;
			}
			/*
			 * While it is not normally safe to access the cache
			 * bucket pointers while not on the CPU that owns the
			 * cache, we only allow the pointers to be exchanged
			 * without the zone lock held, not invalidated, so
			 * accept the possible race associated with bucket
			 * exchange during monitoring.
			 */
			for (i = 0; i < (mp_maxid + 1); i++) {
				bzero(&ups, sizeof(ups));
				if (kz->uk_flags & UMA_ZFLAG_INTERNAL)
					goto skip;
				cache = &z->uz_cpu[i];
				if (cache->uc_allocbucket != NULL)
					ups.ups_cache_free +=
					    cache->uc_allocbucket->ub_cnt;
				if (cache->uc_freebucket != NULL)
					ups.ups_cache_free +=
					    cache->uc_freebucket->ub_cnt;
				ups.ups_allocs = cache->uc_allocs;
				ups.ups_frees = cache->uc_frees;
skip:
				if (sbuf_bcat(&sbuf, &ups, sizeof(ups)) < 0) {
					ZONE_UNLOCK(z);
					mtx_unlock(&uma_mtx);
					error = ENOMEM;
					goto out;
				}
			}
			ZONE_UNLOCK(z);
		}
	}
	mtx_unlock(&uma_mtx);
	sbuf_finish(&sbuf);
	error = SYSCTL_OUT(req, sbuf_data(&sbuf), sbuf_len(&sbuf));
out:
	free(buffer, M_TEMP);
	return (error);
}

#ifdef DDB
DB_SHOW_COMMAND(uma, db_show_uma)
{
	u_int64_t allocs, frees;
	uma_bucket_t bucket;
	uma_keg_t kz;
	uma_zone_t z;
	int cachefree;

	db_printf("%18s %12s %12s %12s %8s\n", "Zone", "Allocs", "Frees",
	    "Used", "Cache");
	LIST_FOREACH(kz, &uma_kegs, uk_link) {
		LIST_FOREACH(z, &kz->uk_zones, uz_link) {
			if (kz->uk_flags & UMA_ZFLAG_INTERNAL) {
				allocs = z->uz_allocs;
				frees = z->uz_frees;
				cachefree = 0;
			} else
				uma_zone_sumstat(z, &cachefree, &allocs,
				    &frees);
			if (!((kz->uk_flags & UMA_ZONE_SECONDARY) &&
			    (LIST_FIRST(&kz->uk_zones) != z)))
				cachefree += kz->uk_free;
			LIST_FOREACH(bucket, &z->uz_full_bucket, ub_link)
				cachefree += bucket->ub_cnt;
			db_printf("%18s %12llu %12llu %12llu %8d\n", z->uz_name,
			    allocs, frees, allocs - frees, cachefree);
		}
	}
}
#endif
