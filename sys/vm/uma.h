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
 * uma.h - External definitions for the Universal Memory Allocator
 *
 * Jeff Roberson <jroberson@chesapeake.net>
*/

#ifndef VM_UMA_H
#define VM_UMA_H

#include <sys/param.h>		/* For NULL */
#include <sys/malloc.h>		/* For M_* */

/* User visable parameters */
#define UMA_SMALLEST_UNIT       (PAGE_SIZE / 256) /* Smallest item allocated */

/* Types and type defs */

struct uma_zone; 
/* Opaque type used as a handle to the zone */
typedef struct uma_zone * uma_zone_t;

/* 
 * Item constructor
 *
 * Arguments:
 *	item  A pointer to the memory which has been allocated.
 *	arg   The arg field passed to uma_zalloc_arg
 *	size  The size of the allocated item
 * 
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	The constructor is called just before the memory is returned
 *	to the user. It may block if neccisary.
 */
typedef void (*uma_ctor)(void *mem, int size, void *arg);

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
 * 
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	The initializer is called when the memory is cached in the uma zone. 
 *	this should be the same state that the destructor leaves the object in.
 */
typedef void (*uma_init)(void *mem, int size);

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
 *	system for other uses.  It is the counter part to the init function.
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
 * For example, in the initializer you may want to initialize embeded locks,
 * NULL list pointers, set up initial states, magic numbers, etc.  This way if
 * the object is held in the allocator and re-used it won't be neccisary to
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
 *	name  The text name of the zone for debugging and stats, this memory
 *		should not be freed until the zone has been deallocated.
 *	size  The size of the object that is being created.
 *	ctor  The constructor that is called when the object is allocated
 *	dtor  The destructor that is called when the object is freed.
 *	init  An initializer that sets up the initial state of the memory.
 *	fini  A discard function that undoes initialization done by init.
 *		ctor/dtor/init/fini may all be null, see notes above.
 *	align A bitmask that corisponds to the requested alignment
 *		eg 4 would be 0x3
 *	flags A set of parameters that control the behavior of the zone
 *
 * Returns:
 *	A pointer to a structure which is intended to be opaque to users of
 *	the interface.  The value may be null if the wait flag is not set.
 */

uma_zone_t uma_zcreate(char *name, int size, uma_ctor ctor, uma_dtor dtor,
			uma_init uminit, uma_fini fini, int align,
			u_int16_t flags);

/* Definitions for uma_zcreate flags */
#define UMA_ZONE_PAGEABLE	0x0001	/* Return items not fully backed by
					   physical memory XXX Not yet */
#define UMA_ZONE_ZINIT		0x0002	/* Initialize with zeros */
#define UMA_ZONE_STATIC		0x0004	/* Staticly sized zone */
#define UMA_ZONE_OFFPAGE	0x0008	/* Force the slab structure allocation
					   off of the real memory */
#define UMA_ZONE_MALLOC		0x0010	/* For use by malloc(9) only! */
#define UMA_ZONE_NOFREE		0x0020	/* Do not free slabs of this type! */

/* Definitions for align */
#define UMA_ALIGN_PTR	(sizeof(void *) - 1)	/* Alignment fit for ptr */
#define UMA_ALIGN_LONG	(sizeof(long) - 1)	/* "" long */
#define UMA_ALIGN_INT	(sizeof(int) - 1)	/* "" int */
#define UMA_ALIGN_SHORT	(sizeof(short) - 1)	/* "" short */
#define UMA_ALIGN_CHAR	(sizeof(char) - 1)	/* "" char */
#define UMA_ALIGN_CACHE	(16 - 1)		/* Cache line size align */

/*
 * Destroys a uma zone
 *
 * Arguments:
 *	zone  The zone we want to destroy.
 *	wait  This flag indicates whether or not we should wait for all
 *		allocations to free, or return an errno on outstanding memory.
 *
 * Returns:
 *	0 on successful completion, or EWOULDBLOCK if there are outstanding
 *	allocations and the wait flag is M_NOWAIT
 */

int uma_zdestroy(uma_zone_t zone, int wait);

/*
 * Allocates an item out of a zone
 *
 * Arguments:
 *	zone  The zone we are allocating from
 *	arg   This data is passed to the ctor function
 *	wait  This flag indicates whether or not we are allowed to block while
 *		allocating memory for this zone should we run out.
 *
 * Returns:
 *	A non null pointer to an initialized element from the zone is
 *	garanteed if the wait flag is M_WAITOK, otherwise a null pointer may be
 *	returned if the zone is empty or the ctor failed.
 */

void *uma_zalloc_arg(uma_zone_t zone, void *arg, int wait);

/*
 * Allocates an item out of a zone without supplying an argument
 *
 * This is just a wrapper for uma_zalloc_arg for convenience.
 *
 */
static __inline void *uma_zalloc(uma_zone_t zone, int wait);

static __inline void *
uma_zalloc(uma_zone_t zone, int wait)
{
	return uma_zalloc_arg(zone, NULL, wait);
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
	return uma_zfree_arg(zone, item, NULL);
}

/*
 * XXX The rest of the prototypes in this header are h0h0 magic for the VM.
 * If you think you need to use it for a normal zone you're probably incorrect.
 */

/*
 * Backend page supplier routines
 *
 * Arguments:
 *	zone  The zone that is requesting pages
 *	size  The number of bytes being requested
 *	pflag Flags for these memory pages, see below.
 *	wait  Indicates our willingness to block.
 *
 * Returns:
 *	A pointer to the alloced memory or NULL on failure.
 */

typedef void *(*uma_alloc)(uma_zone_t zone, int size, u_int8_t *pflag, int wait);

/*
 * Backend page free routines
 *
 * Arguments:
 *	item  A pointer to the previously allocated pages
 *	size  The original size of the allocation
 *	pflag The flags for the slab.  See UMA_SLAB_* below
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
 *	UMA_SLAB_SIZE * UMA_BOOT_PAGES bytes. (see uma_int.h)
 *
 */

void uma_startup(void *bootmem);

/*
 * Finishes starting up the allocator.  This should
 * be called when kva is ready for normal allocs.
 *
 * Arguments:
 *	hash   An area of memory that will become the malloc hash
 *	elems  The number of elements in this array
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	uma_startup2 is called by kmeminit() to prepare the malloc
 *	hash bucket, and enable use of uma for malloc ops.
 */
 
void uma_startup2(void *hash, u_long elems);

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
 * Switches the backing object of a zone
 *
 * Arguments:
 *	zone  The zone to update
 *	obj   The obj to use for future allocations
 *	size  The size of the object to allocate
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
 * Replaces the standard page_alloc or obj_alloc functions for this zone
 *
 * Arguments:
 *	zone   The zone whos back end allocator is being changed.
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
 * These flags are setable in the allocf and visable in the freef.
 */
#define UMA_SLAB_BOOT	0x01		/* Slab alloced from boot pages */
#define UMA_SLAB_KMEM	0x02		/* Slab alloced from kmem_map */
#define UMA_SLAB_KMAP	0x04		/* Slab alloced from kernel_map */
#define UMA_SLAB_PRIV	0x08		/* Slab alloced from priv allocator */
#define UMA_SLAB_OFFP	0x10		/* Slab is managed seperately  */
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


#endif
