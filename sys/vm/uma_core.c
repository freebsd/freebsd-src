/*
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
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

#include "opt_param.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>
#include <sys/mbuf.h>

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

/*
 * This is the zone from which all zones are spawned.  The idea is that even 
 * the zone heads are allocated from the allocator, so we use the bss section
 * to bootstrap us.
 */
static struct uma_zone masterzone;
static uma_zone_t zones = &masterzone;

/* This is the zone from which all of uma_slab_t's are allocated. */
static uma_zone_t slabzone;

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

/* Linked list of all zones in the system */
static LIST_HEAD(,uma_zone) uma_zones = LIST_HEAD_INITIALIZER(&uma_zones); 

/* This mutex protects the zone list */
static struct mtx uma_mtx;

/* These are the pcpu cache locks */
static struct mtx uma_pcpu_mtx[MAXCPU];

/* Linked list of boot time pages */
static LIST_HEAD(,uma_slab) uma_boot_pages =
    LIST_HEAD_INITIALIZER(&uma_boot_pages);

/* Count of free boottime pages */
static int uma_boot_free = 0;

/* Is the VM done starting up? */
static int booted = 0;

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
	int align;
	u_int16_t flags;
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

uint8_t bucket_size[BUCKET_ZONES];

/* Prototypes.. */

static void *obj_alloc(uma_zone_t, int, u_int8_t *, int);
static void *page_alloc(uma_zone_t, int, u_int8_t *, int);
static void *startup_alloc(uma_zone_t, int, u_int8_t *, int);
static void page_free(void *, int, u_int8_t);
static uma_slab_t slab_zalloc(uma_zone_t, int);
static void cache_drain(uma_zone_t);
static void bucket_drain(uma_zone_t, uma_bucket_t);
static void zone_ctor(void *, int, void *);
static void zone_dtor(void *, int, void *);
static void zero_init(void *, int);
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
static void uma_zfree_internal(uma_zone_t, void *, void *, int);
static void bucket_enable(void);
static void bucket_init(void);
static uma_bucket_t bucket_alloc(int, int);
static void bucket_free(uma_bucket_t);
static void bucket_zone_drain(void);
static int uma_zalloc_bucket(uma_zone_t zone, int flags);
static uma_slab_t uma_zone_slab(uma_zone_t zone, int flags);
static void *uma_slab_alloc(uma_zone_t zone, uma_slab_t slab);
static void zone_drain(uma_zone_t);

void uma_print_zone(uma_zone_t);
void uma_print_stats(void);
static int sysctl_vm_zone(SYSCTL_HANDLER_ARGS);

SYSCTL_OID(_vm, OID_AUTO, zone, CTLTYPE_STRING|CTLFLAG_RD,
    NULL, 0, sysctl_vm_zone, "A", "Zone Info");
SYSINIT(uma_startup3, SI_SUB_VM_CONF, SI_ORDER_SECOND, uma_startup3, NULL);

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

static uma_bucket_t
bucket_alloc(int entries, int bflags)
{
	struct uma_bucket_zone *ubz;
	uma_bucket_t bucket;
	int idx;

	/*
	 * This is to stop us from allocating per cpu buckets while we're
	 * running out of UMA_BOOT_PAGES.  Otherwise, we would exhaust the
	 * boot pages.  This also prevents us from allocating buckets in
	 * low memory situations.
	 */

	if (bucketdisable)
		return (NULL);
	idx = howmany(entries, 1 << BUCKET_SHIFT);
	ubz = &bucket_zones[bucket_size[idx]];
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
	int idx;

	idx = howmany(bucket->ub_entries, 1 << BUCKET_SHIFT);
	ubz = &bucket_zones[bucket_size[idx]];
	uma_zfree_internal(ubz->ubz_zone, bucket, NULL, 0);
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
	uma_cache_t cache;
	u_int64_t alloc;
	int cpu;

	alloc = 0;

	/*
	 * Aggregate per cpu cache statistics back to the zone.
	 *
	 * I may rewrite this to set a flag in the per cpu cache instead of
	 * locking.  If the flag is not cleared on the next round I will have
	 * to lock and do it here instead so that the statistics don't get too
	 * far out of sync.
	 */
	if (!(zone->uz_flags & UMA_ZFLAG_INTERNAL)) {
		for (cpu = 0; cpu < mp_maxid; cpu++) {
			if (CPU_ABSENT(cpu))
				continue;
			CPU_LOCK(cpu); 
			cache = &zone->uz_cpu[cpu];
			/* Add them up, and reset */
			alloc += cache->uc_allocs;
			cache->uc_allocs = 0;
			CPU_UNLOCK(cpu);
		}
	}

	/* Now push these stats back into the zone.. */
	ZONE_LOCK(zone);
	zone->uz_allocs += alloc;

	/*
	 * Expand the zone hash table.
	 * 
	 * This is done if the number of slabs is larger than the hash size.
	 * What I'm trying to do here is completely reduce collisions.  This
	 * may be a little aggressive.  Should I allow for two collisions max?
	 */

	if (zone->uz_flags & UMA_ZONE_HASH &&
	    zone->uz_pages / zone->uz_ppera >= zone->uz_hash.uh_hashsize) {
		struct uma_hash newhash;
		struct uma_hash oldhash;
		int ret;

		/*
		 * This is so involved because allocating and freeing 
		 * while the zone lock is held will lead to deadlock.
		 * I have to do everything in stages and check for
		 * races.
		 */
		newhash = zone->uz_hash;
		ZONE_UNLOCK(zone);
		ret = hash_alloc(&newhash);
		ZONE_LOCK(zone);
		if (ret) {
			if (hash_expand(&zone->uz_hash, &newhash)) {
				oldhash = zone->uz_hash;
				zone->uz_hash = newhash;
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
 * 	Nothing
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
		    hash->uh_slab_hash, NULL, 0);
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
	if (zone->uz_flags & UMA_ZONE_MALLOC)
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
		uma_zfree_internal(zone, item, slab, 1);
	}
}

/*
 * Drains the per cpu caches for a zone.
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
	uma_bucket_t bucket;
	uma_cache_t cache;
	int cpu;

	/*
	 * We have to lock each cpu cache before locking the zone
	 */
	for (cpu = 0; cpu < mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		CPU_LOCK(cpu);
		cache = &zone->uz_cpu[cpu];
		bucket_drain(zone, cache->uc_allocbucket);
		bucket_drain(zone, cache->uc_freebucket);
		if (cache->uc_allocbucket != NULL)
			bucket_free(cache->uc_allocbucket);
		if (cache->uc_freebucket != NULL)
			bucket_free(cache->uc_freebucket);
		cache->uc_allocbucket = cache->uc_freebucket = NULL;
	}

	/*
	 * Drain the bucket queues and free the buckets, we just keep two per
	 * cpu (alloc/free).
	 */
	ZONE_LOCK(zone);
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
	for (cpu = 0; cpu < mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		CPU_UNLOCK(cpu);
	}
	ZONE_UNLOCK(zone);
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
	struct slabhead freeslabs = {};
	uma_slab_t slab;
	uma_slab_t n;
	u_int8_t flags;
	u_int8_t *mem;
	int i;

	/*
	 * We don't want to take pages from staticly allocated zones at this
	 * time
	 */
	if (zone->uz_flags & UMA_ZONE_NOFREE || zone->uz_freef == NULL)
		return;

	ZONE_LOCK(zone);

#ifdef UMA_DEBUG
	printf("%s free items: %u\n", zone->uz_name, zone->uz_free);
#endif
	if (zone->uz_free == 0)
		goto finished;

	slab = LIST_FIRST(&zone->uz_free_slab);
	while (slab) {
		n = LIST_NEXT(slab, us_link);

		/* We have no where to free these to */
		if (slab->us_flags & UMA_SLAB_BOOT) {
			slab = n;
			continue;
		}

		LIST_REMOVE(slab, us_link);
		zone->uz_pages -= zone->uz_ppera;
		zone->uz_free -= zone->uz_ipers;

		if (zone->uz_flags & UMA_ZONE_HASH)
			UMA_HASH_REMOVE(&zone->uz_hash, slab, slab->us_data);

		SLIST_INSERT_HEAD(&freeslabs, slab, us_hlink);

		slab = n;
	}
finished:
	ZONE_UNLOCK(zone);

	while ((slab = SLIST_FIRST(&freeslabs)) != NULL) {
		SLIST_REMOVE(&freeslabs, slab, uma_slab, us_hlink);
		if (zone->uz_fini)
			for (i = 0; i < zone->uz_ipers; i++)
				zone->uz_fini(
				    slab->us_data + (zone->uz_rsize * i),
				    zone->uz_size);
		flags = slab->us_flags;
		mem = slab->us_data;

		if (zone->uz_flags & UMA_ZONE_OFFPAGE)
			uma_zfree_internal(slabzone, slab, NULL, 0);
		if (zone->uz_flags & UMA_ZONE_MALLOC) {
			vm_object_t obj;

			if (flags & UMA_SLAB_KMEM)
				obj = kmem_object;
			else
				obj = NULL;
			for (i = 0; i < zone->uz_ppera; i++)
				vsetobj((vm_offset_t)mem + (i * PAGE_SIZE),
				    obj);
		}
#ifdef UMA_DEBUG
		printf("%s: Returning %d bytes.\n",
		    zone->uz_name, UMA_SLAB_SIZE * zone->uz_ppera);
#endif
		zone->uz_freef(mem, UMA_SLAB_SIZE * zone->uz_ppera, flags);
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
	uma_slab_t slab;	/* Starting slab */
	u_int8_t *mem;
	u_int8_t flags;
	int i;

	slab = NULL;

#ifdef UMA_DEBUG
	printf("slab_zalloc:  Allocating a new slab for %s\n", zone->uz_name);
#endif
	ZONE_UNLOCK(zone);

	if (zone->uz_flags & UMA_ZONE_OFFPAGE) {
		slab = uma_zalloc_internal(slabzone, NULL, wait);
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

	if ((zone->uz_flags & UMA_ZONE_MALLOC) == 0)
		wait |= M_ZERO;
	else
		wait &= ~M_ZERO;

	mem = zone->uz_allocf(zone, zone->uz_ppera * UMA_SLAB_SIZE,
	    &flags, wait);
	if (mem == NULL) {
		ZONE_LOCK(zone);
		return (NULL);
	}

	/* Point the slab into the allocated memory */
	if (!(zone->uz_flags & UMA_ZONE_OFFPAGE))
		slab = (uma_slab_t )(mem + zone->uz_pgoff);

	if (zone->uz_flags & UMA_ZONE_MALLOC)
		for (i = 0; i < zone->uz_ppera; i++)
			vsetslab((vm_offset_t)mem + (i * PAGE_SIZE), slab);

	slab->us_zone = zone;
	slab->us_data = mem;
	slab->us_freecount = zone->uz_ipers;
	slab->us_firstfree = 0;
	slab->us_flags = flags;
	for (i = 0; i < zone->uz_ipers; i++)
		slab->us_freelist[i] = i+1;

	if (zone->uz_init)
		for (i = 0; i < zone->uz_ipers; i++)
			zone->uz_init(slab->us_data + (zone->uz_rsize * i),
			    zone->uz_size);
	ZONE_LOCK(zone);

	if (zone->uz_flags & UMA_ZONE_HASH)
		UMA_HASH_INSERT(&zone->uz_hash, slab, mem);

	zone->uz_pages += zone->uz_ppera;
	zone->uz_free += zone->uz_ipers;

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
	zone->uz_allocf = uma_small_alloc;
#else
	zone->uz_allocf = page_alloc;
#endif
	return zone->uz_allocf(zone, bytes, pflag, wait);
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

	object = zone->uz_obj;
	retkva = 0;

	/* 
	 * This looks a little weird since we're getting one page at a time.
	 */
	VM_OBJECT_LOCK(object);
	p = TAILQ_LAST(&object->memq, pglist);
	pages = p != NULL ? p->pindex + 1 : 0;
	startpages = pages;
	zkva = zone->uz_kva + pages * PAGE_SIZE;
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
static void
zero_init(void *mem, int size)
{
	bzero(mem, size);
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
	int rsize;
	int memused;
	int ipers;

	rsize = zone->uz_size;

	if (rsize < UMA_SMALLEST_UNIT)
		rsize = UMA_SMALLEST_UNIT;

	if (rsize & zone->uz_align)
		rsize = (rsize & ~zone->uz_align) + (zone->uz_align + 1);

	zone->uz_rsize = rsize;

	rsize += 1;	/* Account for the byte of linkage */
	zone->uz_ipers = (UMA_SLAB_SIZE - sizeof(struct uma_slab)) / rsize;
	zone->uz_ppera = 1;

	KASSERT(zone->uz_ipers != 0, ("zone_small_init: ipers is 0, uh-oh!"));
	memused = zone->uz_ipers * zone->uz_rsize;

	/* Can we do any better? */
	if ((UMA_SLAB_SIZE - memused) >= UMA_MAX_WASTE) {
		/*
		 * We can't do this if we're internal or if we've been
		 * asked to not go to the VM for buckets.  If we do this we
		 * may end up going to the VM (kmem_map) for slabs which we
		 * do not want to do if we're UMA_ZFLAG_CACHEONLY as a
		 * result of UMA_ZONE_VM, which clearly forbids it.
		 */
		if ((zone->uz_flags & UMA_ZFLAG_INTERNAL) ||
		    (zone->uz_flags & UMA_ZFLAG_CACHEONLY))
			return;
		ipers = UMA_SLAB_SIZE / zone->uz_rsize;
		if (ipers > zone->uz_ipers) {
			zone->uz_flags |= UMA_ZONE_OFFPAGE;
			if ((zone->uz_flags & UMA_ZONE_MALLOC) == 0)
				zone->uz_flags |= UMA_ZONE_HASH;
			zone->uz_ipers = ipers;
		}
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
	int pages;

	KASSERT((zone->uz_flags & UMA_ZFLAG_CACHEONLY) == 0,
	    ("zone_large_init: Cannot large-init a UMA_ZFLAG_CACHEONLY zone"));

	pages = zone->uz_size / UMA_SLAB_SIZE;

	/* Account for remainder */
	if ((pages * UMA_SLAB_SIZE) < zone->uz_size)
		pages++;

	zone->uz_ppera = pages;
	zone->uz_ipers = 1;

	zone->uz_flags |= UMA_ZONE_OFFPAGE;
	if ((zone->uz_flags & UMA_ZONE_MALLOC) == 0)
		zone->uz_flags |= UMA_ZONE_HASH;

	zone->uz_rsize = zone->uz_size;
}

/* 
 * Zone header ctor.  This initializes all fields, locks, etc.  And inserts
 * the zone onto the global zone list.
 *
 * Arguments/Returns follow uma_ctor specifications
 *	udata  Actually uma_zcreat_args
 */

static void
zone_ctor(void *mem, int size, void *udata)
{
	struct uma_zctor_args *arg = udata;
	uma_zone_t zone = mem;
	int privlc;

	bzero(zone, size);
	zone->uz_name = arg->name;
	zone->uz_size = arg->size;
	zone->uz_ctor = arg->ctor;
	zone->uz_dtor = arg->dtor;
	zone->uz_init = arg->uminit;
	zone->uz_fini = arg->fini;
	zone->uz_align = arg->align;
	zone->uz_free = 0;
	zone->uz_pages = 0;
	zone->uz_flags = arg->flags;
	zone->uz_allocf = page_alloc;
	zone->uz_freef = page_free;

	if (arg->flags & UMA_ZONE_ZINIT)
		zone->uz_init = zero_init;

	if (arg->flags & UMA_ZONE_VM)
		zone->uz_flags |= UMA_ZFLAG_CACHEONLY;

	/*
	 * XXX:
	 * The +1 byte added to uz_size is to account for the byte of
	 * linkage that is added to the size in zone_small_init().  If
	 * we don't account for this here then we may end up in
	 * zone_small_init() with a calculated 'ipers' of 0.
	 */
	if ((zone->uz_size+1) > (UMA_SLAB_SIZE - sizeof(struct uma_slab)))
		zone_large_init(zone);
	else
		zone_small_init(zone);
	/*
	 * If we haven't booted yet we need allocations to go through the
	 * startup cache until the vm is ready.
	 */
	if (zone->uz_ppera == 1) {
#ifdef UMA_MD_SMALL_ALLOC
		zone->uz_allocf = uma_small_alloc;
		zone->uz_freef = uma_small_free;
#endif
		if (booted == 0)
			zone->uz_allocf = startup_alloc;
	}
	if (arg->flags & UMA_ZONE_MTXCLASS)
		privlc = 1;
	else
		privlc = 0;

	/*
	 * If we're putting the slab header in the actual page we need to
	 * figure out where in each page it goes.  This calculates a right 
	 * justified offset into the memory on an ALIGN_PTR boundary.
	 */
	if (!(zone->uz_flags & UMA_ZONE_OFFPAGE)) {
		int totsize;

		/* Size of the slab struct and free list */
		totsize = sizeof(struct uma_slab) + zone->uz_ipers;
		if (totsize & UMA_ALIGN_PTR)
			totsize = (totsize & ~UMA_ALIGN_PTR) +
			    (UMA_ALIGN_PTR + 1);
		zone->uz_pgoff = UMA_SLAB_SIZE - totsize;
		totsize = zone->uz_pgoff + sizeof(struct uma_slab)
		    + zone->uz_ipers;
		/* I don't think it's possible, but I'll make sure anyway */
		if (totsize > UMA_SLAB_SIZE) {
			printf("zone %s ipers %d rsize %d size %d\n",
			    zone->uz_name, zone->uz_ipers, zone->uz_rsize,
			    zone->uz_size);
			panic("UMA slab won't fit.\n");
		}
	}

	if (zone->uz_flags & UMA_ZONE_HASH)
		hash_alloc(&zone->uz_hash);

#ifdef UMA_DEBUG
	printf("%s(%p) size = %d ipers = %d ppera = %d pgoff = %d\n",
	    zone->uz_name, zone,
	    zone->uz_size, zone->uz_ipers,
	    zone->uz_ppera, zone->uz_pgoff);
#endif
	ZONE_LOCK_INIT(zone, privlc);

	mtx_lock(&uma_mtx);
	LIST_INSERT_HEAD(&uma_zones, zone, uz_link);
	mtx_unlock(&uma_mtx);

	/*
	 * Some internal zones don't have room allocated for the per cpu
	 * caches.  If we're internal, bail out here.
	 */
	if (zone->uz_flags & UMA_ZFLAG_INTERNAL)
		return;

	if (zone->uz_ipers <= BUCKET_MAX)
		zone->uz_count = zone->uz_ipers;
	else
		zone->uz_count = BUCKET_MAX;
}

/* 
 * Zone header dtor.  This frees all data, destroys locks, frees the hash table
 * and removes the zone from the global list.
 *
 * Arguments/Returns follow uma_dtor specifications
 *	udata  unused
 */

static void
zone_dtor(void *arg, int size, void *udata)
{
	uma_zone_t zone;

	zone = (uma_zone_t)arg;

	if (!(zone->uz_flags & UMA_ZFLAG_INTERNAL))
		cache_drain(zone);
	mtx_lock(&uma_mtx);
	LIST_REMOVE(zone, uz_link);
	zone_drain(zone);
	mtx_unlock(&uma_mtx);

	ZONE_LOCK(zone);
	if (zone->uz_free != 0) {
		printf("Zone %s was not empty (%d items). "
		    " Lost %d pages of memory.\n",
		    zone->uz_name, zone->uz_free, zone->uz_pages);
		uma_print_zone(zone);
	}

	ZONE_UNLOCK(zone);
	if (zone->uz_flags & UMA_ZONE_HASH)
		hash_free(&zone->uz_hash);

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
zone_foreach(void (*zfunc)(uma_zone_t))
{
	uma_zone_t zone;

	mtx_lock(&uma_mtx);
	LIST_FOREACH(zone, &uma_zones, uz_link)
		zfunc(zone);
	mtx_unlock(&uma_mtx);
}

/* Public functions */
/* See uma.h */
void
uma_startup(void *bootmem)
{
	struct uma_zctor_args args;
	uma_slab_t slab;
	int slabsize;
	int i;

#ifdef UMA_DEBUG
	printf("Creating uma zone headers zone.\n");
#endif
	mtx_init(&uma_mtx, "UMA lock", NULL, MTX_DEF);
	/* "manually" Create the initial zone */
	args.name = "UMA Zones";
	args.size = sizeof(struct uma_zone) +
	    (sizeof(struct uma_cache) * mp_maxid);
	args.ctor = zone_ctor;
	args.dtor = zone_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.align = 32 - 1;
	args.flags = UMA_ZFLAG_INTERNAL;
	/* The initial zone has no Per cpu queues so it's smaller */
	zone_ctor(zones, sizeof(struct uma_zone), &args);

	/* Initialize the pcpu cache lock set once and for all */
	for (i = 0; i < mp_maxid; i++)
		CPU_LOCK_INIT(i);
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
	printf("Creating slab zone.\n");
#endif

	/*
	 * This is the max number of free list items we'll have with
	 * offpage slabs.
	 */
	slabsize = UMA_SLAB_SIZE - sizeof(struct uma_slab);
	slabsize /= UMA_MAX_WASTE;
	slabsize++;			/* In case there it's rounded */
	slabsize += sizeof(struct uma_slab);

	/* Now make a zone for slab headers */
	slabzone = uma_zcreate("UMA Slabs",
				slabsize,
				NULL, NULL, NULL, NULL,
				UMA_ALIGN_PTR, UMA_ZFLAG_INTERNAL);

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
	callout_init(&uma_callout, 0);
	callout_reset(&uma_callout, UMA_TIMEOUT * hz, uma_timeout, NULL);
#ifdef UMA_DEBUG
	printf("UMA startup3 complete.\n");
#endif
}

/* See uma.h */
uma_zone_t  
uma_zcreate(char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
		uma_init uminit, uma_fini fini, int align, u_int16_t flags)
		     
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

	return (uma_zalloc_internal(zones, &args, M_WAITOK));
}

/* See uma.h */
void
uma_zdestroy(uma_zone_t zone)
{
	uma_zfree_internal(zones, zone, NULL, 0);
}

/* See uma.h */
void *
uma_zalloc_arg(uma_zone_t zone, void *udata, int flags)
{
	void *item;
	uma_cache_t cache;
	uma_bucket_t bucket;
	int cpu;

	/* This is the fast path allocation */
#ifdef UMA_DEBUG_ALLOC_1
	printf("Allocating one item from %s(%p)\n", zone->uz_name, zone);
#endif

#ifdef INVARIANTS
	/*
	 * To make sure that WAITOK or NOWAIT is set, but not more than
	 * one, and check against the API botches that are common.
	 * The uma code implies M_WAITOK if M_NOWAIT is not set, so
	 * we default to waiting if none of the flags is set.
	 */
	cpu = flags & (M_WAITOK | M_NOWAIT | M_DONTWAIT | M_TRYWAIT);
	if (cpu != M_NOWAIT && cpu != M_WAITOK) {
		static	struct timeval lasterr;
		static	int curerr, once;
		if (once == 0 && ppsratecheck(&lasterr, &curerr, 1)) {
			printf("Bad uma_zalloc flags: %x\n", cpu);
			backtrace();
			once++;
		}
	}
#endif
	if (!(flags & M_NOWAIT)) {
		KASSERT(curthread->td_intr_nesting_level == 0,
		   ("malloc(M_WAITOK) in interrupt context"));
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "malloc() of \"%s\"", zone->uz_name);
	}

zalloc_restart:
	cpu = PCPU_GET(cpuid);
	CPU_LOCK(cpu);
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
#ifdef INVARIANTS
			ZONE_LOCK(zone);
			uma_dbg_alloc(zone, NULL, item);
			ZONE_UNLOCK(zone);
#endif
			CPU_UNLOCK(cpu);
			if (zone->uz_ctor)
				zone->uz_ctor(item, zone->uz_size, udata);
			if (flags & M_ZERO)
				bzero(item, zone->uz_size);
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
	ZONE_LOCK(zone);
	/* Since we have locked the zone we may as well send back our stats */
	zone->uz_allocs += cache->uc_allocs;
	cache->uc_allocs = 0;

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
	/* We are no longer associated with this cpu!!! */
	CPU_UNLOCK(cpu);

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

	/* 
	 * This is to prevent us from recursively trying to allocate
	 * buckets.  The problem is that if an allocation forces us to
	 * grab a new bucket we will call page_alloc, which will go off
	 * and cause the vm to allocate vm_map_entries.  If we need new
	 * buckets there too we will recurse in kmem_alloc and bad 
	 * things happen.  So instead we return a NULL bucket, and make
	 * the code that allocates buckets smart enough to deal with it
	 */ 
	if (zone->uz_flags & UMA_ZFLAG_INTERNAL && zone->uz_recurse != 0)
		return (NULL);

	slab = NULL;

	for (;;) {
		/*
		 * Find a slab with some space.  Prefer slabs that are partially
		 * used over those that are totally full.  This helps to reduce
		 * fragmentation.
		 */
		if (zone->uz_free != 0) {
			if (!LIST_EMPTY(&zone->uz_part_slab)) {
				slab = LIST_FIRST(&zone->uz_part_slab);
			} else {
				slab = LIST_FIRST(&zone->uz_free_slab);
				LIST_REMOVE(slab, us_link);
				LIST_INSERT_HEAD(&zone->uz_part_slab, slab,
				us_link);
			}
			return (slab);
		}

		/*
		 * M_NOVM means don't ask at all!
		 */
		if (flags & M_NOVM)
			break;

		if (zone->uz_maxpages &&
		    zone->uz_pages >= zone->uz_maxpages) {
			zone->uz_flags |= UMA_ZFLAG_FULL;

			if (flags & M_NOWAIT)
				break;
			else 
				msleep(zone, &zone->uz_lock, PVM,
				    "zonelimit", 0);
			continue;
		}
		zone->uz_recurse++;
		slab = slab_zalloc(zone, flags);
		zone->uz_recurse--;
		/* 
		 * If we got a slab here it's safe to mark it partially used
		 * and return.  We assume that the caller is going to remove
		 * at least one item.
		 */
		if (slab) {
			LIST_INSERT_HEAD(&zone->uz_part_slab, slab, us_link);
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
	void *item;
	u_int8_t freei;
	
	freei = slab->us_firstfree;
	slab->us_firstfree = slab->us_freelist[freei];
	item = slab->us_data + (zone->uz_rsize * freei);

	slab->us_freecount--;
	zone->uz_free--;
#ifdef INVARIANTS
	uma_dbg_alloc(zone, slab, item);
#endif
	/* Move this slab to the full list */
	if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&zone->uz_full_slab, slab, us_link);
	}

	return (item);
}

static int
uma_zalloc_bucket(uma_zone_t zone, int flags)
{
	uma_bucket_t bucket;
	uma_slab_t slab;
	int max;

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
		if (zone->uz_flags & UMA_ZFLAG_CACHEONLY)
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
	while (bucket->ub_cnt < max &&
	    (slab = uma_zone_slab(zone, flags)) != NULL) {
		while (slab->us_freecount && bucket->ub_cnt < max) {
			bucket->ub_bucket[bucket->ub_cnt++] =
			    uma_slab_alloc(zone, slab);
		}
		/* Don't block on the next fill */
		flags |= M_NOWAIT;
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
	uma_slab_t slab;
	void *item;

	item = NULL;

#ifdef UMA_DEBUG_ALLOC
	printf("INTERNAL: Allocating one item from %s(%p)\n", zone->uz_name, zone);
#endif
	ZONE_LOCK(zone);

	slab = uma_zone_slab(zone, flags);
	if (slab == NULL) {
		ZONE_UNLOCK(zone);
		return (NULL);
	}

	item = uma_slab_alloc(zone, slab);

	ZONE_UNLOCK(zone);

	if (zone->uz_ctor != NULL)
		zone->uz_ctor(item, zone->uz_size, udata);
	if (flags & M_ZERO)
		bzero(item, zone->uz_size);

	return (item);
}

/* See uma.h */
void
uma_zfree_arg(uma_zone_t zone, void *item, void *udata)
{
	uma_cache_t cache;
	uma_bucket_t bucket;
	int bflags;
	int cpu;
	int skip;

	/* This is the fast path free */
	skip = 0;
#ifdef UMA_DEBUG_ALLOC_1
	printf("Freeing item %p to %s(%p)\n", item, zone->uz_name, zone);
#endif
	/*
	 * The race here is acceptable.  If we miss it we'll just have to wait
	 * a little longer for the limits to be reset.
	 */

	if (zone->uz_flags & UMA_ZFLAG_FULL)
		goto zfree_internal;

	if (zone->uz_dtor) {
		zone->uz_dtor(item, zone->uz_size, udata);
		skip = 1;
	}

zfree_restart:
	cpu = PCPU_GET(cpuid);
	CPU_LOCK(cpu);
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
#ifdef INVARIANTS
			ZONE_LOCK(zone);
			if (zone->uz_flags & UMA_ZONE_MALLOC)
				uma_dbg_free(zone, udata, item);
			else
				uma_dbg_free(zone, NULL, item);
			ZONE_UNLOCK(zone);
#endif
			CPU_UNLOCK(cpu);
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
	 */

	ZONE_LOCK(zone);

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
	/* We're done with this CPU now */
	CPU_UNLOCK(cpu);

	/* And the zone.. */
	ZONE_UNLOCK(zone);

#ifdef UMA_DEBUG_ALLOC
	printf("uma_zfree: Allocating new free bucket.\n");
#endif
	bflags = M_NOWAIT;

	if (zone->uz_flags & UMA_ZFLAG_CACHEONLY)
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

#ifdef INVARIANTS
	/*
	 * If we need to skip the dtor and the uma_dbg_free in
	 * uma_zfree_internal because we've already called the dtor
	 * above, but we ended up here, then we need to make sure
	 * that we take care of the uma_dbg_free immediately.
	 */
	if (skip) {
		ZONE_LOCK(zone);
		if (zone->uz_flags & UMA_ZONE_MALLOC)
			uma_dbg_free(zone, udata, item);
		else
			uma_dbg_free(zone, NULL, item);
		ZONE_UNLOCK(zone);
	}
#endif
	uma_zfree_internal(zone, item, udata, skip);

	return;

}

/*
 * Frees an item to an INTERNAL zone or allocates a free bucket
 *
 * Arguments:
 *	zone   The zone to free to
 *	item   The item we're freeing
 *	udata  User supplied data for the dtor
 *	skip   Skip the dtor, it was done in uma_zfree_arg
 */
static void
uma_zfree_internal(uma_zone_t zone, void *item, void *udata, int skip)
{
	uma_slab_t slab;
	u_int8_t *mem;
	u_int8_t freei;

	if (!skip && zone->uz_dtor)
		zone->uz_dtor(item, zone->uz_size, udata);

	ZONE_LOCK(zone);

	if (!(zone->uz_flags & UMA_ZONE_MALLOC)) {
		mem = (u_int8_t *)((unsigned long)item & (~UMA_SLAB_MASK));
		if (zone->uz_flags & UMA_ZONE_HASH)
			slab = hash_sfind(&zone->uz_hash, mem);
		else {
			mem += zone->uz_pgoff;
			slab = (uma_slab_t)mem;
		}
	} else {
		slab = (uma_slab_t)udata;
	}

	/* Do we need to remove from any lists? */
	if (slab->us_freecount+1 == zone->uz_ipers) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&zone->uz_free_slab, slab, us_link);
	} else if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&zone->uz_part_slab, slab, us_link);
	}

	/* Slab management stuff */	
	freei = ((unsigned long)item - (unsigned long)slab->us_data)
		/ zone->uz_rsize;

#ifdef INVARIANTS
	if (!skip)
		uma_dbg_free(zone, slab, item);
#endif

	slab->us_freelist[freei] = slab->us_firstfree;
	slab->us_firstfree = freei;
	slab->us_freecount++;

	/* Zone statistics */
	zone->uz_free++;

	if (zone->uz_flags & UMA_ZFLAG_FULL) {
		if (zone->uz_pages < zone->uz_maxpages)
			zone->uz_flags &= ~UMA_ZFLAG_FULL;

		/* We can handle one more allocation */
		wakeup_one(zone);
	}

	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_max(uma_zone_t zone, int nitems)
{
	ZONE_LOCK(zone);
	if (zone->uz_ppera > 1)
		zone->uz_maxpages = nitems * zone->uz_ppera;
	else 
		zone->uz_maxpages = nitems / zone->uz_ipers;

	if (zone->uz_maxpages * zone->uz_ipers < nitems)
		zone->uz_maxpages++;

	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_freef(uma_zone_t zone, uma_free freef)
{
	ZONE_LOCK(zone);
	zone->uz_freef = freef;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
void
uma_zone_set_allocf(uma_zone_t zone, uma_alloc allocf)
{
	ZONE_LOCK(zone);
	zone->uz_flags |= UMA_ZFLAG_PRIVALLOC;
	zone->uz_allocf = allocf;
	ZONE_UNLOCK(zone);
}

/* See uma.h */
int
uma_zone_set_obj(uma_zone_t zone, struct vm_object *obj, int count)
{
	int pages;
	vm_offset_t kva;

	pages = count / zone->uz_ipers;

	if (pages * zone->uz_ipers < count)
		pages++;

	kva = kmem_alloc_pageable(kernel_map, pages * UMA_SLAB_SIZE);

	if (kva == 0)
		return (0);
	if (obj == NULL) {
		obj = vm_object_allocate(OBJT_DEFAULT,
		    pages);
	} else {
		VM_OBJECT_LOCK_INIT(obj);
		_vm_object_allocate(OBJT_DEFAULT,
		    pages, obj);
	}
	ZONE_LOCK(zone);
	zone->uz_kva = kva;
	zone->uz_obj = obj;
	zone->uz_maxpages = pages;
	zone->uz_allocf = obj_alloc;
	zone->uz_flags |= UMA_ZONE_NOFREE | UMA_ZFLAG_PRIVALLOC;
	ZONE_UNLOCK(zone);
	return (1);
}

/* See uma.h */
void
uma_prealloc(uma_zone_t zone, int items)
{
	int slabs;
	uma_slab_t slab;

	ZONE_LOCK(zone);
	slabs = items / zone->uz_ipers;
	if (slabs * zone->uz_ipers < items)
		slabs++;
	while (slabs > 0) {
		slab = slab_zalloc(zone, M_WAITOK);
		LIST_INSERT_HEAD(&zone->uz_free_slab, slab, us_link);
		slabs--;
	}
	ZONE_UNLOCK(zone);
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
		uma_zfree_internal(slabzone, slab, NULL, 0);
	}


	return (mem);
}

void
uma_large_free(uma_slab_t slab)
{
	vsetobj((vm_offset_t)slab->us_data, kmem_object);
	/* 
	 * XXX: We get a lock order reversal if we don't have Giant:
	 * vm_map_remove (locks system map) -> vm_map_delete ->
	 *    vm_map_entry_unwire -> vm_fault_unwire -> mtx_lock(&Giant)
	 */
	if (!mtx_owned(&Giant)) {
		mtx_lock(&Giant);
		page_free(slab->us_data, slab->us_size, slab->us_flags);
		mtx_unlock(&Giant);
	} else
		page_free(slab->us_data, slab->us_size, slab->us_flags);
	uma_zfree_internal(slabzone, slab, NULL, 0);
}

void
uma_print_stats(void)
{
	zone_foreach(uma_print_zone);
}

static void
slab_print(uma_slab_t slab)
{
	printf("slab: zone %p, data %p, freecount %d, firstfree %d\n",
		slab->us_zone, slab->us_data, slab->us_freecount,
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
	uma_slab_t slab;
	int i;

	printf("%s(%p) size %d(%d) flags %d ipers %d ppera %d out %d free %d\n",
	    zone->uz_name, zone, zone->uz_size, zone->uz_rsize, zone->uz_flags,
	    zone->uz_ipers, zone->uz_ppera,
	    (zone->uz_ipers * zone->uz_pages) - zone->uz_free, zone->uz_free);
	printf("Part slabs:\n");
	LIST_FOREACH(slab, &zone->uz_part_slab, us_link)
		slab_print(slab);
	printf("Free slabs:\n");
	LIST_FOREACH(slab, &zone->uz_free_slab, us_link)
		slab_print(slab);
	printf("Full slabs:\n");
	LIST_FOREACH(slab, &zone->uz_full_slab, us_link)
		slab_print(slab);
	for (i = 0; i < mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		cache = &zone->uz_cpu[i];
		printf("CPU %d Cache:\n", i);
		cache_print(cache);
	}
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
	char *p;
	int cpu;
	int cachefree;
	uma_bucket_t bucket;
	uma_cache_t cache;

	cnt = 0;
	mtx_lock(&uma_mtx);
	LIST_FOREACH(z, &uma_zones, uz_link)
		cnt++;
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
	LIST_FOREACH(z, &uma_zones, uz_link) {
		if (cnt == 0)	/* list may have changed size */
			break;
		if (!(z->uz_flags & UMA_ZFLAG_INTERNAL)) {
			for (cpu = 0; cpu < mp_maxid; cpu++) {
				if (CPU_ABSENT(cpu))
					continue;
				CPU_LOCK(cpu);
			}
		}
		ZONE_LOCK(z);
		cachefree = 0;
		if (!(z->uz_flags & UMA_ZFLAG_INTERNAL)) {
			for (cpu = 0; cpu < mp_maxid; cpu++) {
				if (CPU_ABSENT(cpu))
					continue;
				cache = &z->uz_cpu[cpu];
				if (cache->uc_allocbucket != NULL)
					cachefree += cache->uc_allocbucket->ub_cnt;
				if (cache->uc_freebucket != NULL)
					cachefree += cache->uc_freebucket->ub_cnt;
				CPU_UNLOCK(cpu);
			}
		}
		LIST_FOREACH(bucket, &z->uz_full_bucket, ub_link) {
			cachefree += bucket->ub_cnt;
		}
		totalfree = z->uz_free + cachefree;
		len = snprintf(offset, linesize,
		    "%-12.12s  %6.6u, %8.8u, %6.6u, %6.6u, %8.8llu\n",
		    z->uz_name, z->uz_size,
		    z->uz_maxpages * z->uz_ipers,
		    (z->uz_ipers * (z->uz_pages / z->uz_ppera)) - totalfree,
		    totalfree,
		    (unsigned long long)z->uz_allocs);
		ZONE_UNLOCK(z);
		for (p = offset + 12; p > offset && *p == ' '; --p)
			/* nothing */ ;
		p[1] = ':';
		cnt--;
		offset += len;
	}
	mtx_unlock(&uma_mtx);
	*offset++ = '\0';
	error = SYSCTL_OUT(req, tmpbuf, offset - tmpbuf);
out:
	FREE(tmpbuf, M_TEMP);
	return (error);
}
