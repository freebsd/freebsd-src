/*-
 * Copyright (c) 2002, 2003, 2004, 2005 Jeffrey Roberson <jeff@FreeBSD.org>
 * Copyright (c) 2004, 2005 Bosko Milekic <bmilekic@FreeBSD.org>
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
 * uma.h - External definitions for the Universal Memory Allocator
 *
*/

#ifndef VM_UMA_H
#define VM_UMA_H

#include <sys/param.h>		/* For NULL */
#include <sys/malloc.h>		/* For M_* */

/* User visible parameters */
#define UMA_SMALLEST_UNIT       (PAGE_SIZE / 256) /* Smallest item allocated */

/* Types and type defs */

struct uma_zone;
/* Opaque type used as a handle to the zone */
typedef struct uma_zone * uma_zone_t;

void zone_drain(uma_zone_t);

/* 
 * Item constructor
 *
 * Arguments:
 *	item  A pointer to the memory which has been allocated.
 *	arg   The arg field passed to uma_zalloc_arg
 *	size  The size of the allocated item
 *	flags See zalloc flags
 * 
 * Returns:
 *	0      on success
 *      errno  on failure
 *
 * Discussion:
 *	The constructor is called just before the memory is returned
 *	to the user. It may block if necessary.
 */
typedef int (*uma_ctor)(void *mem, int size, void *arg, int flags);

/*
 * Item destructor
 *
 * Arguments:
 *	item  A pointer to the memory which has been allocated.
 *	size  The size of the item being destructed.
 *	arg   Argument passed through uma_zfree_arg
 * 
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	The destructor may perform operations that differ from those performed
 *	by the initializer, but it must leave the object in the same state.
 *	This IS type stable storage.  This is called after EVERY zfree call.
 */
typedef void (*uma_dtor)(void *mem, int size, void *arg);

/* 
 * Item initializer
 *
 * Arguments:
 *	item  A pointer to the memory which has been allocated.
 *	size  The size of the item being initialized.
 *	flags See zalloc flags
 * 
 * Returns:
 *	0      on success
 *      errno  on failure
 *
 * Discussion:
 *	The initializer is called when the memory is cached in the uma zone. 
 *	The initializer and the destructor should leave the object in the same
 *	state.
 */
typedef int (*uma_init)(void *mem, int size, int flags);

/*
 * Item discard function
 *
 * Arguments:
 * 	item  A pointer to memory which has been 'freed' but has not left the 
 *	      zone's cache.
 *	size  The size of the item being discarded.
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	This routine is called when memory leaves a zone and is returned to the
 *	system for other uses.  It is the counter-part to the init function.
 */
typedef void (*uma_fini)(void *mem, int size);

/*
 * What's the difference between initializing and constructing?
 *
 * The item is initialized when it is cached, and this is the state that the 
 * object should be in when returned to the allocator. The purpose of this is
 * to remove some code which would otherwise be called on each allocation by
 * utilizing a known, stable state.  This differs from the constructor which
 * will be called on EVERY allocation.
 *
 * For example, in the initializer you may want to initialize embedded locks,
 * NULL list pointers, set up initial states, magic numbers, etc.  This way if
 * the object is held in the allocator and re-used it won't be necessary to
 * re-initialize it.
 *
 * The constructor may be used to lock a data structure, link it on to lists,
 * bump reference counts or total counts of outstanding structures, etc.
 *
 */


/* Function proto types */

/*
 * Create a new uma zone
 *
 * Arguments:
 *	name  The text name of the zone for debugging and stats. This memory
 *		should not be freed until the zone has been deallocated.
 *	size  The size of the object that is being created.
 *	ctor  The constructor that is called when the object is allocated.
 *	dtor  The destructor that is called when the object is freed.
 *	init  An initializer that sets up the initial state of the memory.
 *	fini  A discard function that undoes initialization done by init.
 *		ctor/dtor/init/fini may all be null, see notes above.
 *	align A bitmask that corresponds to the requested alignment
 *		eg 4 would be 0x3
 *	flags A set of parameters that control the behavior of the zone.
 *
 * Returns:
 *	A pointer to a structure which is intended to be opaque to users of
 *	the interface.  The value may be null if the wait flag is not set.
 */
uma_zone_t uma_zcreate(char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
			uma_init uminit, uma_fini fini, int align,
			u_int32_t flags);

/*
 * Create a secondary uma zone
 *
 * Arguments:
 *	name  The text name of the zone for debugging and stats. This memory
 *		should not be freed until the zone has been deallocated.
 *	ctor  The constructor that is called when the object is allocated.
 *	dtor  The destructor that is called when the object is freed.
 *	zinit  An initializer that sets up the initial state of the memory
 *		as the object passes from the Keg's slab to the Zone's cache.
 *	zfini  A discard function that undoes initialization done by init
 *		as the object passes from the Zone's cache to the Keg's slab.
 *
 *		ctor/dtor/zinit/zfini may all be null, see notes above.
 *		Note that the zinit and zfini specified here are NOT
 *		exactly the same as the init/fini specified to uma_zcreate()
 *		when creating a master zone.  These zinit/zfini are called
 *		on the TRANSITION from keg to zone (and vice-versa). Once
 *		these are set, the primary zone may alter its init/fini
 *		(which are called when the object passes from VM to keg)
 *		using uma_zone_set_init/fini()) as well as its own
 *		zinit/zfini (unset by default for master zone) with
 *		uma_zone_set_zinit/zfini() (note subtle 'z' prefix).
 *
 *	master  A reference to this zone's Master Zone (Primary Zone),
 *		which contains the backing Keg for the Secondary Zone
 *		being added.
 *
 * Returns:
 *	A pointer to a structure which is intended to be opaque to users of
 *	the interface.  The value may be null if the wait flag is not set.
 */
uma_zone_t uma_zsecond_create(char *name, uma_ctor ctor, uma_dtor dtor,
		    uma_init zinit, uma_fini zfini, uma_zone_t master);

/*
 * Add a second master to a secondary zone.  This provides multiple data
 * backends for objects with the same size.  Both masters must have
 * compatible allocation flags.  Presently, UMA_ZONE_MALLOC type zones are
 * the only supported.
 *
 * Returns:
 * 	Error on failure, 0 on success.
 */
int uma_zsecond_add(uma_zone_t zone, uma_zone_t master);

/*
 * Definitions for uma_zcreate flags
 *
 * These flags share space with UMA_ZFLAGs in uma_int.h.  Be careful not to
 * overlap when adding new features.  0xf0000000 is in use by uma_int.h.
 */
#define UMA_ZONE_PAGEABLE	0x0001	/* Return items not fully backed by
					   physical memory XXX Not yet */
#define UMA_ZONE_ZINIT		0x0002	/* Initialize with zeros */
#define UMA_ZONE_STATIC		0x0004	/* Statically sized zone */
#define UMA_ZONE_OFFPAGE	0x0008	/* Force the slab structure allocation
					   off of the real memory */
#define UMA_ZONE_MALLOC		0x0010	/* For use by malloc(9) only! */
#define UMA_ZONE_NOFREE		0x0020	/* Do not free slabs of this type! */
#define UMA_ZONE_MTXCLASS	0x0040	/* Create a new lock class */
#define	UMA_ZONE_VM		0x0080	/*
					 * Used for internal vm datastructures
					 * only.
					 */
#define	UMA_ZONE_HASH		0x0100	/*
					 * Use a hash table instead of caching
					 * information in the vm_page.
					 */
#define	UMA_ZONE_SECONDARY	0x0200	/* Zone is a Secondary Zone */
#define	UMA_ZONE_REFCNT		0x0400	/* Allocate refcnts in slabs */
#define	UMA_ZONE_MAXBUCKET	0x0800	/* Use largest buckets */
#define	UMA_ZONE_CACHESPREAD	0x1000	/*
					 * Spread memory start locations across
					 * all possible cache lines.  May
					 * require many virtually contiguous
					 * backend pages and can fail early.
					 */
#define	UMA_ZONE_VTOSLAB	0x2000	/* Zone uses vtoslab for lookup. */

/*
 * These flags are shared between the keg and zone.  In zones wishing to add
 * new kegs these flags must be compatible.  Some are determined based on
 * physical parameters of the request and may not be provided by the consumer.
 */
#define	UMA_ZONE_INHERIT						\
    (UMA_ZONE_OFFPAGE | UMA_ZONE_MALLOC | UMA_ZONE_NOFREE |		\
    UMA_ZONE_HASH | UMA_ZONE_REFCNT | UMA_ZONE_VTOSLAB)

/* Definitions for align */
#define UMA_ALIGN_PTR	(sizeof(void *) - 1)	/* Alignment fit for ptr */
#define UMA_ALIGN_LONG	(sizeof(long) - 1)	/* "" long */
#define UMA_ALIGN_INT	(sizeof(int) - 1)	/* "" int */
#define UMA_ALIGN_SHORT	(sizeof(short) - 1)	/* "" short */
#define UMA_ALIGN_CHAR	(sizeof(char) - 1)	/* "" char */
#define UMA_ALIGN_CACHE	(0 - 1)			/* Cache line size align */

/*
 * Destroys an empty uma zone.  If the zone is not empty uma complains loudly.
 *
 * Arguments:
 *	zone  The zone we want to destroy.
 *
 */
void uma_zdestroy(uma_zone_t zone);

/*
 * Allocates an item out of a zone
 *
 * Arguments:
 *	zone  The zone we are allocating from
 *	arg   This data is passed to the ctor function
 *	flags See sys/malloc.h for available flags.
 *
 * Returns:
 *	A non-null pointer to an initialized element from the zone is
 *	guaranteed if the wait flag is M_WAITOK.  Otherwise a null pointer
 *	may be returned if the zone is empty or the ctor failed.
 */

void *uma_zalloc_arg(uma_zone_t zone, void *arg, int flags);

/*
 * Allocates an item out of a zone without supplying an argument
 *
 * This is just a wrapper for uma_zalloc_arg for convenience.
 *
 */
static __inline void *uma_zalloc(uma_zone_t zone, int flags);

static __inline void *
uma_zalloc(uma_zone_t zone, int flags)
{
	return uma_zalloc_arg(zone, NULL, flags);
}

/*
 * Frees an item back into the specified zone.
 *
 * Arguments:
 *	zone  The zone the item was originally allocated out of.
 *	item  The memory to be freed.
 *	arg   Argument passed to the destructor
 *
 * Returns:
 *	Nothing.
 */

void uma_zfree_arg(uma_zone_t zone, void *item, void *arg);

/*
 * Frees an item back to a zone without supplying an argument
 *
 * This is just a wrapper for uma_zfree_arg for convenience.
 *
 */
static __inline void uma_zfree(uma_zone_t zone, void *item);

static __inline void
uma_zfree(uma_zone_t zone, void *item)
{
	uma_zfree_arg(zone, item, NULL);
}

/*
 * XXX The rest of the prototypes in this header are h0h0 magic for the VM.
 * If you think you need to use it for a normal zone you're probably incorrect.
 */

/*
 * Backend page supplier routines
 *
 * Arguments:
 *	zone  The zone that is requesting pages.
 *	size  The number of bytes being requested.
 *	pflag Flags for these memory pages, see below.
 *	wait  Indicates our willingness to block.
 *
 * Returns:
 *	A pointer to the allocated memory or NULL on failure.
 */

typedef void *(*uma_alloc)(uma_zone_t zone, int size, u_int8_t *pflag, int wait);

/*
 * Backend page free routines
 *
 * Arguments:
 *	item  A pointer to the previously allocated pages.
 *	size  The original size of the allocation.
 *	pflag The flags for the slab.  See UMA_SLAB_* below.
 *
 * Returns:
 *	None
 */
typedef void (*uma_free)(void *item, int size, u_int8_t pflag);



/*
 * Sets up the uma allocator. (Called by vm_mem_init)
 *
 * Arguments:
 *	bootmem  A pointer to memory used to bootstrap the system.
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	This memory is used for zones which allocate things before the
 *	backend page supplier can give us pages.  It should be
 *	UMA_SLAB_SIZE * boot_pages bytes. (see uma_int.h)
 *
 */

void uma_startup(void *bootmem, int boot_pages);

/*
 * Finishes starting up the allocator.  This should
 * be called when kva is ready for normal allocs.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	uma_startup2 is called by kmeminit() to enable us of uma for malloc.
 */
 
void uma_startup2(void);

/*
 * Reclaims unused memory for all zones
 *
 * Arguments:
 *	None
 * Returns:
 *	None
 *
 * This should only be called by the page out daemon.
 */

void uma_reclaim(void);

/*
 * Sets the alignment mask to be used for all zones requesting cache
 * alignment.  Should be called by MD boot code prior to starting VM/UMA.
 *
 * Arguments:
 *	align The alignment mask
 *
 * Returns:
 *	Nothing
 */
void uma_set_align(int align);

/*
 * Switches the backing object of a zone
 *
 * Arguments:
 *	zone  The zone to update.
 *	obj   The VM object to use for future allocations.
 *	size  The size of the object to allocate.
 *
 * Returns:
 *	0  if kva space can not be allocated
 *	1  if successful
 *
 * Discussion:
 *	A NULL object can be used and uma will allocate one for you.  Setting
 *	the size will limit the amount of memory allocated to this zone.
 *
 */
struct vm_object;
int uma_zone_set_obj(uma_zone_t zone, struct vm_object *obj, int size);

/*
 * Sets a high limit on the number of items allowed in a zone
 *
 * Arguments:
 *	zone  The zone to limit
 *	nitems  The requested upper limit on the number of items allowed
 *
 * Returns:
 *	int  The effective value of nitems after rounding up based on page size
 */
int uma_zone_set_max(uma_zone_t zone, int nitems);

/*
 * Obtains the effective limit on the number of items in a zone
 *
 * Arguments:
 *	zone  The zone to obtain the effective limit from
 *
 * Return:
 *	0  No limit
 *	int  The effective limit of the zone
 */
int uma_zone_get_max(uma_zone_t zone);

/*
 * Obtains the approximate current number of items allocated from a zone
 *
 * Arguments:
 *	zone  The zone to obtain the current allocation count from
 *
 * Return:
 *	int  The approximate current number of items allocated from the zone
 */
int uma_zone_get_cur(uma_zone_t zone);

/*
 * The following two routines (uma_zone_set_init/fini)
 * are used to set the backend init/fini pair which acts on an
 * object as it becomes allocated and is placed in a slab within
 * the specified zone's backing keg.  These should probably not
 * be changed once allocations have already begun, but only be set
 * immediately upon zone creation.
 */
void uma_zone_set_init(uma_zone_t zone, uma_init uminit);
void uma_zone_set_fini(uma_zone_t zone, uma_fini fini);

/*
 * The following two routines (uma_zone_set_zinit/zfini) are
 * used to set the zinit/zfini pair which acts on an object as
 * it passes from the backing Keg's slab cache to the
 * specified Zone's bucket cache.  These should probably not
 * be changed once allocations have already begun, but only be set
 * immediately upon zone creation.
 */
void uma_zone_set_zinit(uma_zone_t zone, uma_init zinit);
void uma_zone_set_zfini(uma_zone_t zone, uma_fini zfini);

/*
 * Replaces the standard page_alloc or obj_alloc functions for this zone
 *
 * Arguments:
 *	zone   The zone whose backend allocator is being changed.
 *	allocf A pointer to the allocation function
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	This could be used to implement pageable allocation, or perhaps
 *	even DMA allocators if used in conjunction with the OFFPAGE
 *	zone flag.
 */

void uma_zone_set_allocf(uma_zone_t zone, uma_alloc allocf);

/*
 * Used for freeing memory provided by the allocf above
 *
 * Arguments:
 *	zone  The zone that intends to use this free routine.
 *	freef The page freeing routine.
 *
 * Returns:
 *	Nothing
 */

void uma_zone_set_freef(uma_zone_t zone, uma_free freef);

/*
 * These flags are setable in the allocf and visible in the freef.
 */
#define UMA_SLAB_BOOT	0x01		/* Slab alloced from boot pages */
#define UMA_SLAB_KMEM	0x02		/* Slab alloced from kmem_map */
#define UMA_SLAB_KERNEL	0x04		/* Slab alloced from kernel_map */
#define UMA_SLAB_PRIV	0x08		/* Slab alloced from priv allocator */
#define UMA_SLAB_OFFP	0x10		/* Slab is managed separately  */
#define UMA_SLAB_MALLOC	0x20		/* Slab is a large malloc slab */
/* 0x40 and 0x80 are available */

/*
 * Used to pre-fill a zone with some number of items
 *
 * Arguments:
 *	zone    The zone to fill
 *	itemcnt The number of items to reserve
 *
 * Returns:
 *	Nothing
 *
 * NOTE: This is blocking and should only be done at startup
 */
void uma_prealloc(uma_zone_t zone, int itemcnt);

/*
 * Used to lookup the reference counter allocated for an item
 * from a UMA_ZONE_REFCNT zone.  For UMA_ZONE_REFCNT zones,
 * reference counters are allocated for items and stored in
 * the underlying slab header.
 *
 * Arguments:
 * 	zone  The UMA_ZONE_REFCNT zone to which the item belongs.
 *	item  The address of the item for which we want a refcnt.
 *
 * Returns:
 * 	A pointer to a u_int32_t reference counter.
 */
u_int32_t *uma_find_refcnt(uma_zone_t zone, void *item);

/*
 * Used to determine if a fixed-size zone is exhausted.
 *
 * Arguments:
 *	zone    The zone to check
 *
 * Returns:
 * 	Non-zero if zone is exhausted.
 */
int uma_zone_exhausted(uma_zone_t zone);
int uma_zone_exhausted_nolock(uma_zone_t zone);

/*
 * Exported statistics structures to be used by user space monitoring tools.
 * Statistics stream consists of a uma_stream_header, followed by a series of
 * alternative uma_type_header and uma_type_stat structures.
 */
#define	UMA_STREAM_VERSION	0x00000001
struct uma_stream_header {
	u_int32_t	ush_version;	/* Stream format version. */
	u_int32_t	ush_maxcpus;	/* Value of MAXCPU for stream. */
	u_int32_t	ush_count;	/* Number of records. */
	u_int32_t	_ush_pad;	/* Pad/reserved field. */
};

#define	UTH_MAX_NAME	32
#define	UTH_ZONE_SECONDARY	0x00000001
struct uma_type_header {
	/*
	 * Static per-zone data, some extracted from the supporting keg.
	 */
	char		uth_name[UTH_MAX_NAME];
	u_int32_t	uth_align;	/* Keg: alignment. */
	u_int32_t	uth_size;	/* Keg: requested size of item. */
	u_int32_t	uth_rsize;	/* Keg: real size of item. */
	u_int32_t	uth_maxpages;	/* Keg: maximum number of pages. */
	u_int32_t	uth_limit;	/* Keg: max items to allocate. */

	/*
	 * Current dynamic zone/keg-derived statistics.
	 */
	u_int32_t	uth_pages;	/* Keg: pages allocated. */
	u_int32_t	uth_keg_free;	/* Keg: items free. */
	u_int32_t	uth_zone_free;	/* Zone: items free. */
	u_int32_t	uth_bucketsize;	/* Zone: desired bucket size. */
	u_int32_t	uth_zone_flags;	/* Zone: flags. */
	u_int64_t	uth_allocs;	/* Zone: number of allocations. */
	u_int64_t	uth_frees;	/* Zone: number of frees. */
	u_int64_t	uth_fails;	/* Zone: number of alloc failures. */
	u_int64_t	uth_sleeps;	/* Zone: number of alloc sleeps. */
	u_int64_t	_uth_reserved1[2];	/* Reserved. */
};

struct uma_percpu_stat {
	u_int64_t	ups_allocs;	/* Cache: number of allocations. */
	u_int64_t	ups_frees;	/* Cache: number of frees. */
	u_int64_t	ups_cache_free;	/* Cache: free items in cache. */
	u_int64_t	_ups_reserved[5];	/* Reserved. */
};

#endif
