/*
 * Copyright (c) 2002, Jeffrey Roberson <jroberson@chesapeake.net>
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
 *
 * $FreeBSD$
 *
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
 *	- Improve INVARIANTS (0xdeadc0de write out)
 *	- Investigate cache size adjustments
 */

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
#include <sys/smp.h>
#include <sys/vmmeter.h>

#include <machine/types.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

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

/*
 * Zone that buckets come from.
 */
static uma_zone_t bucketzone;

/*
 * Are we allowed to allocate buckets?
 */
static int bucketdisable = 1;

/* Linked list of all zones in the system */
static LIST_HEAD(,uma_zone) uma_zones = LIST_HEAD_INITIALIZER(&uma_zones); 

/* This mutex protects the zone list */
static struct mtx uma_mtx;

/* Linked list of boot time pages */
static LIST_HEAD(,uma_slab) uma_boot_pages =
    LIST_HEAD_INITIALIZER(&uma_boot_pages);

/* Count of free boottime pages */
static int uma_boot_free = 0;

/* Is the VM done starting up? */
static int booted = 0;

/* This is the handle used to schedule our working set calculator */
static struct callout uma_callout;

/* This is mp_maxid + 1, for use while looping over each cpu */
static int maxcpu;

/*
 * This structure is passed as the zone ctor arg so that I don't have to create
 * a special allocation function just for zones.
 */
struct uma_zctor_args {
	char *name;
	int size;
	uma_ctor ctor;
	uma_dtor dtor;
	uma_init uminit;
	uma_fini fini;
	int align;
	u_int16_t flags;
};

/*
 * This is the malloc hash table which is used to find the zone that a
 * malloc allocation came from.  It is not currently resizeable.  The
 * memory for the actual hash bucket is allocated in kmeminit.
 */
struct uma_hash mhash;
struct uma_hash *mallochash = &mhash;

/* Prototypes.. */

static void *obj_alloc(uma_zone_t, int, u_int8_t *, int);
static void *page_alloc(uma_zone_t, int, u_int8_t *, int);
static void page_free(void *, int, u_int8_t);
static uma_slab_t slab_zalloc(uma_zone_t, int);
static void cache_drain(uma_zone_t);
static void bucket_drain(uma_zone_t, uma_bucket_t);
static void zone_drain(uma_zone_t);
static void zone_ctor(void *, int, void *);
static void zone_dtor(void *, int, void *);
static void zero_init(void *, int);
static void zone_small_init(uma_zone_t zone);
static void zone_large_init(uma_zone_t zone);
static void zone_foreach(void (*zfunc)(uma_zone_t));
static void zone_timeout(uma_zone_t zone);
static struct slabhead *hash_alloc(int *);
static void hash_expand(struct uma_hash *, struct slabhead *, int);
static void hash_free(struct slabhead *hash, int hashsize);
static void uma_timeout(void *);
static void uma_startup3(void);
static void *uma_zalloc_internal(uma_zone_t, void *, int, uma_bucket_t);
static void uma_zfree_internal(uma_zone_t, void *, void *, int);
static void bucket_enable(void);
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


/*
 * Routine called by timeout which is used to fire off some time interval
 * based calculations.  (working set, stats, etc.)
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
	callout_reset(&uma_callout, UMA_WORKING_TIME * hz, uma_timeout, NULL);
}

/*
 * Routine to perform timeout driven calculations.  This does the working set
 * as well as hash expanding, and per cpu statistics aggregation.
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
	int free;
	int cpu;

	alloc = 0;
	free = 0;

	/*
	 * Aggregate per cpu cache statistics back to the zone.
	 *
	 * I may rewrite this to set a flag in the per cpu cache instead of
	 * locking.  If the flag is not cleared on the next round I will have
	 * to lock and do it here instead so that the statistics don't get too
	 * far out of sync.
	 */
	if (!(zone->uz_flags & UMA_ZFLAG_INTERNAL)) {
		for (cpu = 0; cpu < maxcpu; cpu++) {
			if (CPU_ABSENT(cpu))
				continue;
			CPU_LOCK(zone, cpu); 
			cache = &zone->uz_cpu[cpu];
			/* Add them up, and reset */
			alloc += cache->uc_allocs;
			cache->uc_allocs = 0;
			if (cache->uc_allocbucket)
				free += cache->uc_allocbucket->ub_ptr + 1;
			if (cache->uc_freebucket)
				free += cache->uc_freebucket->ub_ptr + 1;
			CPU_UNLOCK(zone, cpu);
		}
	}

	/* Now push these stats back into the zone.. */
	ZONE_LOCK(zone);
	zone->uz_allocs += alloc;

	/*
	 * cachefree is an instantanious snapshot of what is in the per cpu
	 * caches, not an accurate counter
	 */
	zone->uz_cachefree = free;

	/*
	 * Expand the zone hash table.
	 * 
	 * This is done if the number of slabs is larger than the hash size.
	 * What I'm trying to do here is completely reduce collisions.  This
	 * may be a little aggressive.  Should I allow for two collisions max?
	 */

	if ((zone->uz_flags & UMA_ZFLAG_OFFPAGE) &&
	    !(zone->uz_flags & UMA_ZFLAG_MALLOC)) {
		if (zone->uz_pages / zone->uz_ppera
		    >= zone->uz_hash.uh_hashsize) {
			struct slabhead *newhash;
			int newsize;

			newsize = zone->uz_hash.uh_hashsize;
			ZONE_UNLOCK(zone);
			newhash = hash_alloc(&newsize);
			ZONE_LOCK(zone);
			hash_expand(&zone->uz_hash, newhash, newsize);
		}
	}

	/*
	 * Here we compute the working set size as the total number of items 
	 * left outstanding since the last time interval.  This is slightly
	 * suboptimal. What we really want is the highest number of outstanding
	 * items during the last time quantum.  This should be close enough.
	 *
	 * The working set size is used to throttle the zone_drain function.
	 * We don't want to return memory that we may need again immediately.
	 */
	alloc = zone->uz_allocs - zone->uz_oallocs;
	zone->uz_oallocs = zone->uz_allocs;
	zone->uz_wssize = alloc;

	ZONE_UNLOCK(zone);
}

/*
 * Allocate and zero fill the next sized hash table from the appropriate
 * backing store.
 *
 * Arguments:
 *	oldsize  On input it's the size we're currently at and on output
 *		 it is the expanded size.
 *
 * Returns:
 *	slabhead The new hash bucket or NULL if the allocation failed.
 */
struct slabhead *
hash_alloc(int *oldsize)
{
	struct slabhead *newhash;
	int newsize;
	int alloc;

	/* We're just going to go to a power of two greater */
	if (*oldsize)  {
		newsize = (*oldsize) * 2;
		alloc = sizeof(newhash[0]) * newsize;
		/* XXX Shouldn't be abusing DEVBUF here */
		newhash = (struct slabhead *)malloc(alloc, M_DEVBUF, M_NOWAIT);
	} else {
		alloc = sizeof(newhash[0]) * UMA_HASH_SIZE_INIT;
		newhash = uma_zalloc_internal(hashzone, NULL, M_WAITOK, NULL);
		newsize = UMA_HASH_SIZE_INIT;
	}
	if (newhash)
		bzero(newhash, alloc);

	*oldsize = newsize;

	return (newhash);
}

/*
 * Expands the hash table for OFFPAGE zones.  This is done from zone_timeout
 * to reduce collisions.  This must not be done in the regular allocation path,
 * otherwise, we can recurse on the vm while allocating pages.
 *
 * Arguments:
 *	hash  The hash you want to expand by a factor of two.
 *
 * Returns:
 * 	Nothing
 *
 * Discussion:
 */
static void
hash_expand(struct uma_hash *hash, struct slabhead *newhash, int newsize)
{
	struct slabhead *oldhash;
	uma_slab_t slab;
	int oldsize;
	int hval;
	int i;

	if (!newhash)
		return;

	oldsize = hash->uh_hashsize;
	oldhash = hash->uh_slab_hash;

	if (oldsize >= newsize) {
		hash_free(newhash, newsize);
		return;
	}

	hash->uh_hashmask = newsize - 1;

	/*
	 * I need to investigate hash algorithms for resizing without a
	 * full rehash.
	 */

	for (i = 0; i < oldsize; i++)
		while (!SLIST_EMPTY(&hash->uh_slab_hash[i])) {
			slab = SLIST_FIRST(&hash->uh_slab_hash[i]);
			SLIST_REMOVE_HEAD(&hash->uh_slab_hash[i], us_hlink);
			hval = UMA_HASH(hash, slab->us_data);
			SLIST_INSERT_HEAD(&newhash[hval], slab, us_hlink);
		}

	if (oldhash) 
		hash_free(oldhash, oldsize);

	hash->uh_slab_hash = newhash;
	hash->uh_hashsize = newsize;

	return;
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
hash_free(struct slabhead *slab_hash, int hashsize)
{
	if (hashsize == UMA_HASH_SIZE_INIT)
		uma_zfree_internal(hashzone,
		    slab_hash, NULL, 0);
	else
		free(slab_hash, M_DEVBUF);
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
	if (zone->uz_flags & UMA_ZFLAG_MALLOC)
		mzone = 1;

	while (bucket->ub_ptr > -1)  {
		item = bucket->ub_bucket[bucket->ub_ptr];
#ifdef INVARIANTS
		bucket->ub_bucket[bucket->ub_ptr] = NULL;
		KASSERT(item != NULL,
		    ("bucket_drain: botched ptr, item is NULL"));
#endif
		bucket->ub_ptr--;
		/* 
		 * This is extremely inefficient.  The slab pointer was passed
		 * to uma_zfree_arg, but we lost it because the buckets don't
		 * hold them.  This will go away when free() gets a size passed
		 * to it.
		 */
		if (mzone)
			slab = hash_sfind(mallochash,
			    (u_int8_t *)((unsigned long)item &
			   (~UMA_SLAB_MASK)));
		uma_zfree_internal(zone, item, slab, 1);
	}
}

/*
 * Drains the per cpu caches for a zone.
 *
 * Arguments:
 *	zone  The zone to drain, must be unlocked.
 *
 * Returns:
 *	Nothing
 *
 * This function returns with the zone locked so that the per cpu queues can
 * not be filled until zone_drain is finished.
 *
 */
static void
cache_drain(uma_zone_t zone)
{
	uma_bucket_t bucket;
	uma_cache_t cache;
	int cpu;

	/*
	 * Flush out the per cpu queues.
	 *
	 * XXX This causes unnecessary thrashing due to immediately having
	 * empty per cpu queues.  I need to improve this.
	 */

	/*
	 * We have to lock each cpu cache before locking the zone
	 */
	ZONE_UNLOCK(zone);

	for (cpu = 0; cpu < maxcpu; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		CPU_LOCK(zone, cpu);
		cache = &zone->uz_cpu[cpu];
		bucket_drain(zone, cache->uc_allocbucket);
		bucket_drain(zone, cache->uc_freebucket);
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
		uma_zfree_internal(bucketzone, bucket, NULL, 0);
		ZONE_LOCK(zone);
	}

	/* Now we do the free queue.. */
	while ((bucket = LIST_FIRST(&zone->uz_free_bucket)) != NULL) {
		LIST_REMOVE(bucket, ub_link);
		uma_zfree_internal(bucketzone, bucket, NULL, 0);
	}

	/* We unlock here, but they will all block until the zone is unlocked */
	for (cpu = 0; cpu < maxcpu; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		CPU_UNLOCK(zone, cpu);
	}

	zone->uz_cachefree = 0;
}

/*
 * Frees pages from a zone back to the system.  This is done on demand from
 * the pageout daemon.
 *
 * Arguments:
 *	zone  The zone to free pages from
 *	all   Should we drain all items?
 *
 * Returns:
 *	Nothing.
 */
static void
zone_drain(uma_zone_t zone)
{
	uma_slab_t slab;
	uma_slab_t n;
	u_int64_t extra;
	u_int8_t flags;
	u_int8_t *mem;
	int i;

	/*
	 * We don't want to take pages from staticly allocated zones at this
	 * time
	 */
	if (zone->uz_flags & UMA_ZFLAG_NOFREE || zone->uz_freef == NULL)
		return;

	ZONE_LOCK(zone);

	if (!(zone->uz_flags & UMA_ZFLAG_INTERNAL))
		cache_drain(zone);

	if (zone->uz_free < zone->uz_wssize)
		goto finished;
#ifdef UMA_DEBUG
	printf("%s working set size: %llu free items: %u\n",
	    zone->uz_name, (unsigned long long)zone->uz_wssize, zone->uz_free);
#endif
	extra = zone->uz_free - zone->uz_wssize;
	extra /= zone->uz_ipers;

	/* extra is now the number of extra slabs that we can free */

	if (extra == 0)
		goto finished;

	slab = LIST_FIRST(&zone->uz_free_slab);
	while (slab && extra) {
		n = LIST_NEXT(slab, us_link);

		/* We have no where to free these to */
		if (slab->us_flags & UMA_SLAB_BOOT) {
			slab = n;
			continue;
		}

		LIST_REMOVE(slab, us_link);
		zone->uz_pages -= zone->uz_ppera;
		zone->uz_free -= zone->uz_ipers;
		if (zone->uz_fini)
			for (i = 0; i < zone->uz_ipers; i++)
				zone->uz_fini(
				    slab->us_data + (zone->uz_rsize * i),
				    zone->uz_size);
		flags = slab->us_flags;
		mem = slab->us_data;
		if (zone->uz_flags & UMA_ZFLAG_OFFPAGE) {
			if (zone->uz_flags & UMA_ZFLAG_MALLOC) {
				UMA_HASH_REMOVE(mallochash,
				    slab, slab->us_data); 
			} else {
				UMA_HASH_REMOVE(&zone->uz_hash,
				    slab, slab->us_data);
			}
			uma_zfree_internal(slabzone, slab, NULL, 0);
		} else if (zone->uz_flags & UMA_ZFLAG_MALLOC)
			UMA_HASH_REMOVE(mallochash, slab, slab->us_data);
#ifdef UMA_DEBUG
		printf("%s: Returning %d bytes.\n",
		    zone->uz_name, UMA_SLAB_SIZE * zone->uz_ppera);
#endif
		zone->uz_freef(mem, UMA_SLAB_SIZE * zone->uz_ppera, flags);

		slab = n;
		extra--;
	}

finished:
	ZONE_UNLOCK(zone);
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
 *	
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

	if (zone->uz_flags & UMA_ZFLAG_OFFPAGE) {
		slab = uma_zalloc_internal(slabzone, NULL, wait, NULL);
		if (slab == NULL) {
			ZONE_LOCK(zone);
			return NULL;
		}
	}

	if (booted || (zone->uz_flags & UMA_ZFLAG_PRIVALLOC)) {
		mtx_lock(&Giant);
		mem = zone->uz_allocf(zone, 
		    zone->uz_ppera * UMA_SLAB_SIZE, &flags, wait);
		mtx_unlock(&Giant);
		if (mem == NULL) {
			ZONE_LOCK(zone);
			return (NULL);
		}
	} else {
		uma_slab_t tmps;

		if (zone->uz_ppera > 1)
			panic("UMA: Attemping to allocate multiple pages before vm has started.\n");
		if (zone->uz_flags & UMA_ZFLAG_MALLOC)
			panic("Mallocing before uma_startup2 has been called.\n");
		if (uma_boot_free == 0)
			panic("UMA: Ran out of pre init pages, increase UMA_BOOT_PAGES\n");
		tmps = LIST_FIRST(&uma_boot_pages);
		LIST_REMOVE(tmps, us_link);
		uma_boot_free--;
		mem = tmps->us_data;
	}

	ZONE_LOCK(zone);

	/* Alloc slab structure for offpage, otherwise adjust it's position */
	if (!(zone->uz_flags & UMA_ZFLAG_OFFPAGE)) {
		slab = (uma_slab_t )(mem + zone->uz_pgoff);
	} else  {
		if (!(zone->uz_flags & UMA_ZFLAG_MALLOC))
			UMA_HASH_INSERT(&zone->uz_hash, slab, mem);
	}
	if (zone->uz_flags & UMA_ZFLAG_MALLOC) {
#ifdef UMA_DEBUG
		printf("Inserting %p into malloc hash from slab %p\n",
		    mem, slab);
#endif
		/* XXX Yikes! No lock on the malloc hash! */
		UMA_HASH_INSERT(mallochash, slab, mem);
	}

	slab->us_zone = zone;
	slab->us_data = mem;

	/*
	 * This is intended to spread data out across cache lines.
	 *
	 * This code doesn't seem to work properly on x86, and on alpha
	 * it makes absolutely no performance difference. I'm sure it could
	 * use some tuning, but sun makes outrageous claims about it's
	 * performance.
	 */
#if 0
	if (zone->uz_cachemax) {
		slab->us_data += zone->uz_cacheoff;
		zone->uz_cacheoff += UMA_CACHE_INC;
		if (zone->uz_cacheoff > zone->uz_cachemax)
			zone->uz_cacheoff = 0;
	}
#endif
	
	slab->us_freecount = zone->uz_ipers;
	slab->us_firstfree = 0;
	slab->us_flags = flags;
	for (i = 0; i < zone->uz_ipers; i++)
		slab->us_freelist[i] = i+1;

	if (zone->uz_init)
		for (i = 0; i < zone->uz_ipers; i++)
			zone->uz_init(slab->us_data + (zone->uz_rsize * i),
			    zone->uz_size);

	zone->uz_pages += zone->uz_ppera;
	zone->uz_free += zone->uz_ipers;

	return (slab);
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

	/*
	 * XXX The original zone allocator did this, but I don't think it's
	 * necessary in current.
	 */

	if (lockstatus(&kernel_map->lock, NULL)) {
		*pflag = UMA_SLAB_KMEM;
		p = (void *) kmem_malloc(kmem_map, bytes, wait);
	} else {
		*pflag = UMA_SLAB_KMAP;
		p = (void *) kmem_alloc(kernel_map, bytes);
	}
  
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
	vm_offset_t zkva;
	vm_offset_t retkva;
	vm_page_t p;
	int pages;

	retkva = NULL;
	pages = zone->uz_pages;

	/* 
	 * This looks a little weird since we're getting one page at a time
	 */
	while (bytes > 0) {
		p = vm_page_alloc(zone->uz_obj, pages,
		    VM_ALLOC_INTERRUPT);
		if (p == NULL)
			return (NULL);

		zkva = zone->uz_kva + pages * PAGE_SIZE;
		if (retkva == NULL)
			retkva = zkva;
		pmap_qenter(zkva, &p, 1);
		bytes -= PAGE_SIZE;
		pages += 1;
	}

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
 *
 */
static void
page_free(void *mem, int size, u_int8_t flags)
{
	vm_map_t map;
	if (flags & UMA_SLAB_KMEM)
		map = kmem_map;
	else if (flags & UMA_SLAB_KMAP)
		map = kernel_map;
	else
		panic("UMA: page_free used with invalid flags %d\n", flags);

	kmem_free(map, (vm_offset_t)mem, size);
}

/*
 * Zero fill initializer
 *
 * Arguments/Returns follow uma_init specifications
 *
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

	memused = zone->uz_ipers * zone->uz_rsize;

	/* Can we do any better? */
	if ((UMA_SLAB_SIZE - memused) >= UMA_MAX_WASTE) {
		if (zone->uz_flags & UMA_ZFLAG_INTERNAL) 
			return;
		ipers = UMA_SLAB_SIZE / zone->uz_rsize;
		if (ipers > zone->uz_ipers) {
			zone->uz_flags |= UMA_ZFLAG_OFFPAGE;
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

	pages = zone->uz_size / UMA_SLAB_SIZE;

	/* Account for remainder */
	if ((pages * UMA_SLAB_SIZE) < zone->uz_size)
		pages++;

	zone->uz_ppera = pages;
	zone->uz_ipers = 1;

	zone->uz_flags |= UMA_ZFLAG_OFFPAGE;
	zone->uz_rsize = zone->uz_size;
}

/* 
 * Zone header ctor.  This initializes all fields, locks, etc.  And inserts
 * the zone onto the global zone list.
 *
 * Arguments/Returns follow uma_ctor specifications
 *	udata  Actually uma_zcreat_args
 *
 */

static void
zone_ctor(void *mem, int size, void *udata)
{
	struct uma_zctor_args *arg = udata;
	uma_zone_t zone = mem;
	int cplen;
	int cpu;

	bzero(zone, size);
	zone->uz_name = arg->name;
	zone->uz_size = arg->size;
	zone->uz_ctor = arg->ctor;
	zone->uz_dtor = arg->dtor;
	zone->uz_init = arg->uminit;
	zone->uz_align = arg->align;
	zone->uz_free = 0;
	zone->uz_pages = 0;
	zone->uz_flags = 0;
	zone->uz_allocf = page_alloc;
	zone->uz_freef = page_free;

	if (arg->flags & UMA_ZONE_ZINIT)
		zone->uz_init = zero_init;

	if (arg->flags & UMA_ZONE_INTERNAL)
		zone->uz_flags |= UMA_ZFLAG_INTERNAL;

	if (arg->flags & UMA_ZONE_MALLOC)
		zone->uz_flags |= UMA_ZFLAG_MALLOC;

	if (arg->flags & UMA_ZONE_NOFREE)
		zone->uz_flags |= UMA_ZFLAG_NOFREE;

	if (zone->uz_size > UMA_SLAB_SIZE)
		zone_large_init(zone);
	else
		zone_small_init(zone);

	/* We do this so that the per cpu lock name is unique for each zone */
	memcpy(zone->uz_lname, "PCPU ", 5);
	cplen = min(strlen(zone->uz_name) + 1, LOCKNAME_LEN - 6);
	memcpy(zone->uz_lname+5, zone->uz_name, cplen);
	zone->uz_lname[LOCKNAME_LEN - 1] = '\0';

	/*
	 * If we're putting the slab header in the actual page we need to
	 * figure out where in each page it goes.  This calculates a right 
	 * justified offset into the memory on a ALIGN_PTR boundary.
	 */
	if (!(zone->uz_flags & UMA_ZFLAG_OFFPAGE)) {
		int totsize;
		int waste;

		/* Size of the slab struct and free list */
		totsize = sizeof(struct uma_slab) + zone->uz_ipers;
		if (totsize & UMA_ALIGN_PTR)
			totsize = (totsize & ~UMA_ALIGN_PTR) +
			    (UMA_ALIGN_PTR + 1);
		zone->uz_pgoff = UMA_SLAB_SIZE - totsize;

		waste = zone->uz_pgoff;
		waste -= (zone->uz_ipers * zone->uz_rsize);

		/*
		 * This calculates how much space we have for cache line size
		 * optimizations.  It works by offseting each slab slightly.
		 * Currently it breaks on x86, and so it is disabled.
		 */

		if (zone->uz_align < UMA_CACHE_INC && waste > UMA_CACHE_INC) {
			zone->uz_cachemax = waste - UMA_CACHE_INC;
			zone->uz_cacheoff = 0;
		} 

		totsize = zone->uz_pgoff + sizeof(struct uma_slab)
		    + zone->uz_ipers;
		/* I don't think it's possible, but I'll make sure anyway */
		if (totsize > UMA_SLAB_SIZE) {
			printf("zone %s ipers %d rsize %d size %d\n",
			    zone->uz_name, zone->uz_ipers, zone->uz_rsize,
			    zone->uz_size);
			panic("UMA slab won't fit.\n");
		}
	} else {
		struct slabhead *newhash;
		int hashsize;

		hashsize = 0;
		newhash = hash_alloc(&hashsize);
		hash_expand(&zone->uz_hash, newhash, hashsize);	
		zone->uz_pgoff = 0;
	}

#ifdef UMA_DEBUG
	printf("%s(%p) size = %d ipers = %d ppera = %d pgoff = %d\n",
	    zone->uz_name, zone,
	    zone->uz_size, zone->uz_ipers,
	    zone->uz_ppera, zone->uz_pgoff);
#endif
	ZONE_LOCK_INIT(zone);

	mtx_lock(&uma_mtx);
	LIST_INSERT_HEAD(&uma_zones, zone, uz_link);
	mtx_unlock(&uma_mtx);

	/*
	 * Some internal zones don't have room allocated for the per cpu
	 * caches.  If we're internal, bail out here.
	 */

	if (zone->uz_flags & UMA_ZFLAG_INTERNAL)
		return;

	if (zone->uz_ipers < UMA_BUCKET_SIZE)
		zone->uz_count = zone->uz_ipers - 1;
	else
		zone->uz_count = UMA_BUCKET_SIZE - 1;

	for (cpu = 0; cpu < maxcpu; cpu++)
		CPU_LOCK_INIT(zone, cpu);
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
	int cpu;

	zone = (uma_zone_t)arg;

	mtx_lock(&uma_mtx);
	LIST_REMOVE(zone, uz_link);
	mtx_unlock(&uma_mtx);

	ZONE_LOCK(zone);
	zone->uz_wssize = 0;
	ZONE_UNLOCK(zone);

	zone_drain(zone);
	ZONE_LOCK(zone);
	if (zone->uz_free != 0)
		printf("Zone %s was not empty.  Lost %d pages of memory.\n",
		    zone->uz_name, zone->uz_pages);

	if ((zone->uz_flags & UMA_ZFLAG_INTERNAL) != 0)
		for (cpu = 0; cpu < maxcpu; cpu++)
			CPU_LOCK_FINI(zone, cpu);

	if ((zone->uz_flags & UMA_ZFLAG_OFFPAGE) != 0)
		hash_free(zone->uz_hash.uh_slab_hash,
		    zone->uz_hash.uh_hashsize);

	ZONE_UNLOCK(zone);
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
	LIST_FOREACH(zone, &uma_zones, uz_link) {
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
	int slabsize;
	int i;

#ifdef UMA_DEBUG
	printf("Creating uma zone headers zone.\n");
#endif
#ifdef SMP
	maxcpu = mp_maxid + 1;
#else
	maxcpu = 1;
#endif
#ifdef UMA_DEBUG 
	printf("Max cpu = %d, mp_maxid = %d\n", maxcpu, mp_maxid);
	Debugger("stop");
#endif
	mtx_init(&uma_mtx, "UMA lock", NULL, MTX_DEF);
	/* "manually" Create the initial zone */
	args.name = "UMA Zones";
	args.size = sizeof(struct uma_zone) +
	    (sizeof(struct uma_cache) * (maxcpu - 1));
	args.ctor = zone_ctor;
	args.dtor = zone_dtor;
	args.uminit = zero_init;
	args.fini = NULL;
	args.align = 32 - 1;
	args.flags = UMA_ZONE_INTERNAL;
	/* The initial zone has no Per cpu queues so it's smaller */
	zone_ctor(zones, sizeof(struct uma_zone), &args);

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
				UMA_ALIGN_PTR, UMA_ZONE_INTERNAL);

	hashzone = uma_zcreate("UMA Hash",
	    sizeof(struct slabhead *) * UMA_HASH_SIZE_INIT,
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_INTERNAL);

	bucketzone = uma_zcreate("UMA Buckets", sizeof(struct uma_bucket),
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_INTERNAL);


#ifdef UMA_DEBUG
	printf("UMA startup complete.\n");
#endif
}

/* see uma.h */
void
uma_startup2(void *hashmem, u_long elems)
{
	bzero(hashmem, elems * sizeof(void *));
	mallochash->uh_slab_hash = hashmem;
	mallochash->uh_hashsize = elems;
	mallochash->uh_hashmask = elems - 1;
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
	callout_reset(&uma_callout, UMA_WORKING_TIME * hz, uma_timeout, NULL);
#ifdef UMA_DEBUG
	printf("UMA startup3 complete.\n");
#endif
}

/* See uma.h */
uma_zone_t  
uma_zcreate(char *name, int size, uma_ctor ctor, uma_dtor dtor, uma_init uminit,
		     uma_fini fini, int align, u_int16_t flags)
		     
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

	return (uma_zalloc_internal(zones, &args, M_WAITOK, NULL));
}

/* See uma.h */
void
uma_zdestroy(uma_zone_t zone)
{
	uma_zfree_internal(zones, zone, NULL, 0);
}

/* See uma.h */
void *
uma_zalloc_arg(uma_zone_t zone, void *udata, int wait)
{
	void *item;
	uma_cache_t cache;
	uma_bucket_t bucket;
	int cpu;

	/* This is the fast path allocation */
#ifdef UMA_DEBUG_ALLOC_1
	printf("Allocating one item from %s(%p)\n", zone->uz_name, zone);
#endif

zalloc_restart:
	cpu = PCPU_GET(cpuid);
	CPU_LOCK(zone, cpu);
	cache = &zone->uz_cpu[cpu];

zalloc_start:
	bucket = cache->uc_allocbucket;

	if (bucket) {
		if (bucket->ub_ptr > -1) {
			item = bucket->ub_bucket[bucket->ub_ptr];
#ifdef INVARIANTS
			bucket->ub_bucket[bucket->ub_ptr] = NULL;
#endif
			bucket->ub_ptr--;
			KASSERT(item != NULL,
			    ("uma_zalloc: Bucket pointer mangled."));
			cache->uc_allocs++;
			CPU_UNLOCK(zone, cpu);
			if (zone->uz_ctor)
				zone->uz_ctor(item, zone->uz_size, udata);
			return (item);
		} else if (cache->uc_freebucket) {
			/*
			 * We have run out of items in our allocbucket.
			 * See if we can switch with our free bucket.
			 */
			if (cache->uc_freebucket->ub_ptr > -1) {
				uma_bucket_t swap;

#ifdef UMA_DEBUG_ALLOC
				printf("uma_zalloc: Swapping empty with alloc.\n");
#endif
				swap = cache->uc_freebucket;
				cache->uc_freebucket = cache->uc_allocbucket;
				cache->uc_allocbucket = swap;

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
		KASSERT(cache->uc_allocbucket->ub_ptr == -1,
		    ("uma_zalloc_arg: Freeing a non free bucket."));
		LIST_INSERT_HEAD(&zone->uz_free_bucket,
		    cache->uc_allocbucket, ub_link);
		cache->uc_allocbucket = NULL;
	}

	/* Check the free list for a new alloc bucket */
	if ((bucket = LIST_FIRST(&zone->uz_full_bucket)) != NULL) {
		KASSERT(bucket->ub_ptr != -1,
		    ("uma_zalloc_arg: Returning an empty bucket."));

		LIST_REMOVE(bucket, ub_link);
		cache->uc_allocbucket = bucket;
		ZONE_UNLOCK(zone);
		goto zalloc_start;
	} 
	/* Bump up our uz_count so we get here less */
	if (zone->uz_count < UMA_BUCKET_SIZE - 1)
		zone->uz_count++;

	/* We are no longer associated with this cpu!!! */
	CPU_UNLOCK(zone, cpu);

	/*
	 * Now lets just fill a bucket and put it on the free list.  If that
	 * works we'll restart the allocation from the begining.
	 *
	 * Try this zone's free list first so we don't allocate extra buckets.
	 */

	if ((bucket = LIST_FIRST(&zone->uz_free_bucket)) != NULL)
		LIST_REMOVE(bucket, ub_link);

	/* Now we no longer need the zone lock. */
	ZONE_UNLOCK(zone);

	if (bucket == NULL)
		bucket = uma_zalloc_internal(bucketzone,
		    NULL, wait, NULL);

	if (bucket != NULL) {
#ifdef INVARIANTS
		bzero(bucket, bucketzone->uz_size);
#endif
		bucket->ub_ptr = -1;

		if (uma_zalloc_internal(zone, udata, wait, bucket))
			goto zalloc_restart;
		else
			uma_zfree_internal(bucketzone, bucket, NULL, 0);
	} 
	/*
	 * We may not get a bucket if we recurse, so 
	 * return an actual item.
	 */
#ifdef UMA_DEBUG
	printf("uma_zalloc_arg: Bucketzone returned NULL\n");
#endif

	return (uma_zalloc_internal(zone, udata, wait, NULL));
}

/*
 * Allocates an item for an internal zone OR fills a bucket
 *
 * Arguments
 *	zone   The zone to alloc for.
 *	udata  The data to be passed to the constructor.
 *	wait   M_WAITOK or M_NOWAIT.
 *	bucket The bucket to fill or NULL
 *
 * Returns
 *	NULL if there is no memory and M_NOWAIT is set
 *	An item if called on an interal zone
 *	Non NULL if called to fill a bucket and it was successful.
 *
 * Discussion:
 *	This was much cleaner before it had to do per cpu caches.  It is
 *	complicated now because it has to handle the simple internal case, and
 *	the more involved bucket filling and allocation.
 */

static void *
uma_zalloc_internal(uma_zone_t zone, void *udata, int wait, uma_bucket_t bucket)
{
	uma_slab_t slab;
	u_int8_t freei;
	void *item;

	item = NULL;

	/*
	 * This is to stop us from allocating per cpu buckets while we're
	 * running out of UMA_BOOT_PAGES.  Otherwise, we would exhaust the
	 * boot pages.
	 */

	if (bucketdisable && zone == bucketzone)
		return (NULL);

#ifdef UMA_DEBUG_ALLOC
	printf("INTERNAL: Allocating one item from %s(%p)\n", zone->uz_name, zone);
#endif
	ZONE_LOCK(zone);

	/*
	 * This code is here to limit the number of simultaneous bucket fills
	 * for any given zone to the number of per cpu caches in this zone. This
	 * is done so that we don't allocate more memory than we really need.
	 */

	if (bucket) {
#ifdef SMP
		if (zone->uz_fills >= mp_ncpus) {
#else
		if (zone->uz_fills > 1) {
#endif
			ZONE_UNLOCK(zone);
			return (NULL);
		}

		zone->uz_fills++;
	}

new_slab:

	/* Find a slab with some space */
	if (zone->uz_free) {
		if (!LIST_EMPTY(&zone->uz_part_slab)) {
			slab = LIST_FIRST(&zone->uz_part_slab);
		} else {
			slab = LIST_FIRST(&zone->uz_free_slab);
			LIST_REMOVE(slab, us_link);
			LIST_INSERT_HEAD(&zone->uz_part_slab, slab, us_link);
		}
	} else {
		/* 
		 * This is to prevent us from recursively trying to allocate
		 * buckets.  The problem is that if an allocation forces us to
		 * grab a new bucket we will call page_alloc, which will go off
		 * and cause the vm to allocate vm_map_entries.  If we need new
		 * buckets there too we will recurse in kmem_alloc and bad 
		 * things happen.  So instead we return a NULL bucket, and make
		 * the code that allocates buckets smart enough to deal with it			 */ 
		if (zone == bucketzone && zone->uz_recurse != 0) {
			ZONE_UNLOCK(zone);
			return (NULL);
		}
		while (zone->uz_maxpages &&
		    zone->uz_pages >= zone->uz_maxpages) {
			zone->uz_flags |= UMA_ZFLAG_FULL;

			if (wait & M_WAITOK)
				msleep(zone, &zone->uz_lock, PVM, "zonelimit", 0);
			else 
				goto alloc_fail;

			goto new_slab;
		}

		zone->uz_recurse++;
		slab = slab_zalloc(zone, wait);
		zone->uz_recurse--;
		/* 
		 * We might not have been able to get a slab but another cpu
		 * could have while we were unlocked.  If we did get a slab put
		 * it on the partially used slab list.  If not check the free
		 * count and restart or fail accordingly.
		 */
		if (slab)
			LIST_INSERT_HEAD(&zone->uz_part_slab, slab, us_link);
		else if (zone->uz_free == 0)
			goto alloc_fail;
		else 
			goto new_slab;
	}
	/*
	 * If this is our first time though put this guy on the list.
	 */
	if (bucket != NULL && bucket->ub_ptr == -1)
		LIST_INSERT_HEAD(&zone->uz_full_bucket,
		    bucket, ub_link);


	while (slab->us_freecount) {
		freei = slab->us_firstfree;
		slab->us_firstfree = slab->us_freelist[freei];
#ifdef INVARIANTS
		slab->us_freelist[freei] = 255;
#endif
		slab->us_freecount--;
		zone->uz_free--;
		item = slab->us_data + (zone->uz_rsize * freei);

		if (bucket == NULL) {
			zone->uz_allocs++;
			break;
		}
		bucket->ub_bucket[++bucket->ub_ptr] = item;

		/* Don't overfill the bucket! */
		if (bucket->ub_ptr == zone->uz_count) 
			break;
	}

	/* Move this slab to the full list */
	if (slab->us_freecount == 0) {
		LIST_REMOVE(slab, us_link);
		LIST_INSERT_HEAD(&zone->uz_full_slab, slab, us_link);
	}

	if (bucket != NULL) {
		/* Try to keep the buckets totally full, but don't block */
		if (bucket->ub_ptr < zone->uz_count) {
			wait = M_NOWAIT;
			goto new_slab;
		} else
			zone->uz_fills--;
	}

	ZONE_UNLOCK(zone);

	/* Only construct at this time if we're not filling a bucket */
	if (bucket == NULL && zone->uz_ctor != NULL) 
		zone->uz_ctor(item, zone->uz_size, udata);

	return (item);

alloc_fail:
	if (bucket != NULL)
		zone->uz_fills--;
	ZONE_UNLOCK(zone);

	if (bucket != NULL && bucket->ub_ptr != -1)
		return (bucket);

	return (NULL);
}

/* See uma.h */
void
uma_zfree_arg(uma_zone_t zone, void *item, void *udata)
{
	uma_cache_t cache;
	uma_bucket_t bucket;
	int cpu;

	/* This is the fast path free */
#ifdef UMA_DEBUG_ALLOC_1
	printf("Freeing item %p to %s(%p)\n", item, zone->uz_name, zone);
#endif
	/*
	 * The race here is acceptable.  If we miss it we'll just have to wait
	 * a little longer for the limits to be reset.
	 */

	if (zone->uz_flags & UMA_ZFLAG_FULL)
		goto zfree_internal;

zfree_restart:
	cpu = PCPU_GET(cpuid);
	CPU_LOCK(zone, cpu);
	cache = &zone->uz_cpu[cpu];

zfree_start:
	bucket = cache->uc_freebucket;

	if (bucket) {
		/*
		 * Do we have room in our bucket? It is OK for this uz count
		 * check to be slightly out of sync.
		 */

		if (bucket->ub_ptr < zone->uz_count) {
			bucket->ub_ptr++;
			KASSERT(bucket->ub_bucket[bucket->ub_ptr] == NULL,
			    ("uma_zfree: Freeing to non free bucket index."));
			bucket->ub_bucket[bucket->ub_ptr] = item;
			if (zone->uz_dtor)
				zone->uz_dtor(item, zone->uz_size, udata);
			CPU_UNLOCK(zone, cpu);
			return;
		} else if (cache->uc_allocbucket) {
#ifdef UMA_DEBUG_ALLOC
			printf("uma_zfree: Swapping buckets.\n");
#endif
			/*
			 * We have run out of space in our freebucket.
			 * See if we can switch with our alloc bucket.
			 */
			if (cache->uc_allocbucket->ub_ptr < 
			    cache->uc_freebucket->ub_ptr) {
				uma_bucket_t swap;

				swap = cache->uc_freebucket;
				cache->uc_freebucket = cache->uc_allocbucket;
				cache->uc_allocbucket = swap;

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
	 */

	ZONE_LOCK(zone);

	bucket = cache->uc_freebucket;
	cache->uc_freebucket = NULL;

	/* Can we throw this on the zone full list? */
	if (bucket != NULL) {
#ifdef UMA_DEBUG_ALLOC
		printf("uma_zfree: Putting old bucket on the free list.\n");
#endif
		/* ub_ptr is pointing to the last free item */
		KASSERT(bucket->ub_ptr != -1,
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
	CPU_UNLOCK(zone, cpu);

	/* And the zone.. */
	ZONE_UNLOCK(zone);

#ifdef UMA_DEBUG_ALLOC
	printf("uma_zfree: Allocating new free bucket.\n");
#endif
	bucket = uma_zalloc_internal(bucketzone,
	    NULL, M_NOWAIT, NULL);
	if (bucket) {
#ifdef INVARIANTS
		bzero(bucket, bucketzone->uz_size);
#endif
		bucket->ub_ptr = -1;
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

	uma_zfree_internal(zone, item, udata, 0);

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

	ZONE_LOCK(zone);

	if (!(zone->uz_flags & UMA_ZFLAG_MALLOC)) {
		mem = (u_int8_t *)((unsigned long)item & (~UMA_SLAB_MASK));
		if (zone->uz_flags & UMA_ZFLAG_OFFPAGE)
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
	if (((freei * zone->uz_rsize) + slab->us_data) != item)
		panic("zone: %s(%p) slab %p freed address %p unaligned.\n", 
		    zone->uz_name, zone, slab, item);
	if (freei >= zone->uz_ipers)
		panic("zone: %s(%p) slab %p freelist %i out of range 0-%d\n",
		    zone->uz_name, zone, slab, freei, zone->uz_ipers-1);

	if (slab->us_freelist[freei] != 255) {
		printf("Slab at %p, freei %d = %d.\n",
		    slab, freei, slab->us_freelist[freei]);
		panic("Duplicate free of item %p from zone %p(%s)\n",
		    item, zone, zone->uz_name);
	}
#endif
	slab->us_freelist[freei] = slab->us_firstfree;
	slab->us_firstfree = freei;
	slab->us_freecount++;

	/* Zone statistics */
	zone->uz_free++;

	if (!skip && zone->uz_dtor)
		zone->uz_dtor(item, zone->uz_size, udata);

	if (zone->uz_flags & UMA_ZFLAG_FULL) {
		if (zone->uz_pages < zone->uz_maxpages)
			zone->uz_flags &= ~UMA_ZFLAG_FULL;

		/* We can handle one more allocation */
		wakeup_one(&zone);
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

	mtx_lock(&Giant);

	pages = count / zone->uz_ipers;

	if (pages * zone->uz_ipers < count)
		pages++;

	kva = kmem_alloc_pageable(kernel_map, pages * UMA_SLAB_SIZE);

	if (kva == 0) {
		mtx_unlock(&Giant);
		return (0);
	}


	if (obj == NULL)
		obj = vm_object_allocate(OBJT_DEFAULT,
		    zone->uz_maxpages);
	else 
		_vm_object_allocate(OBJT_DEFAULT,
		    zone->uz_maxpages, obj);

	ZONE_LOCK(zone);
	zone->uz_kva = kva;
	zone->uz_obj = obj;
	zone->uz_maxpages = pages;

	zone->uz_allocf = obj_alloc;
	zone->uz_flags |= UMA_ZFLAG_NOFREE | UMA_ZFLAG_PRIVALLOC;

	ZONE_UNLOCK(zone);
	mtx_unlock(&Giant);

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
	/*
	 * You might think that the delay below would improve performance since
	 * the allocator will give away memory that it may ask for immediately.
	 * Really, it makes things worse, since cpu cycles are so much cheaper
	 * than disk activity.
	 */
#if 0
	static struct timeval tv = {0};
	struct timeval now;
	getmicrouptime(&now);
	if (now.tv_sec > tv.tv_sec + 30)
		tv = now;
	else
		return;
#endif
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
	zone_drain(bucketzone);
}

void *
uma_large_malloc(int size, int wait)
{
	void *mem;
	uma_slab_t slab;
	u_int8_t flags;

	slab = uma_zalloc_internal(slabzone, NULL, wait, NULL);
	if (slab == NULL)
		return (NULL);

	mem = page_alloc(NULL, size, &flags, wait);
	if (mem) {
		slab->us_data = mem;
		slab->us_flags = flags | UMA_SLAB_MALLOC;
		slab->us_size = size;
		UMA_HASH_INSERT(mallochash, slab, mem);
	} else {
		uma_zfree_internal(slabzone, slab, NULL, 0);
	}


	return (mem);
}

void
uma_large_free(uma_slab_t slab)
{
	UMA_HASH_REMOVE(mallochash, slab, slab->us_data);
	page_free(slab->us_data, slab->us_size, slab->us_flags);
	uma_zfree_internal(slabzone, slab, NULL, 0);
}

void
uma_print_stats(void)
{
	zone_foreach(uma_print_zone);
}

void
uma_print_zone(uma_zone_t zone)
{
	printf("%s(%p) size %d(%d) flags %d ipers %d ppera %d out %d free %d\n",
	    zone->uz_name, zone, zone->uz_size, zone->uz_rsize, zone->uz_flags,
	    zone->uz_ipers, zone->uz_ppera,
	    (zone->uz_ipers * zone->uz_pages) - zone->uz_free, zone->uz_free);
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
		ZONE_LOCK(z);
		totalfree = z->uz_free + z->uz_cachefree;
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
