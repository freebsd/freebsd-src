/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)vm_map.c	8.3 (Berkeley) 1/12/94
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $Id: vm_map.c,v 1.84 1997/08/05 23:03:23 dyson Exp $
 */

/*
 *	Virtual memory mapping module.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_inherit.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <vm/default_pager.h>
#include <vm/vm_zone.h>

/*
 *	Virtual memory maps provide for the mapping, protection,
 *	and sharing of virtual memory objects.  In addition,
 *	this module provides for an efficient virtual copy of
 *	memory from one map to another.
 *
 *	Synchronization is required prior to most operations.
 *
 *	Maps consist of an ordered doubly-linked list of simple
 *	entries; a single hint is used to speed up lookups.
 *
 *	In order to properly represent the sharing of virtual
 *	memory regions among maps, the map structure is bi-level.
 *	Top-level ("address") maps refer to regions of sharable
 *	virtual memory.  These regions are implemented as
 *	("sharing") maps, which then refer to the actual virtual
 *	memory objects.  When two address maps "share" memory,
 *	their top-level maps both have references to the same
 *	sharing map.  When memory is virtual-copied from one
 *	address map to another, the references in the sharing
 *	maps are actually copied -- no copying occurs at the
 *	virtual memory object level.
 *
 *	Since portions of maps are specified by start/end addreses,
 *	which may not align with existing map entries, all
 *	routines merely "clip" entries to these start/end values.
 *	[That is, an entry is split into two, bordering at a
 *	start or end value.]  Note that these clippings may not
 *	always be necessary (as the two resulting entries are then
 *	not changed); however, the clipping is done for convenience.
 *	No attempt is currently made to "glue back together" two
 *	abutting entries.
 *
 *	As mentioned above, virtual copy operations are performed
 *	by copying VM object references from one sharing map to
 *	another, and then marking both regions as copy-on-write.
 *	It is important to note that only one writeable reference
 *	to a VM object region exists in any map -- this means that
 *	shadow object creation can be delayed until a write operation
 *	occurs.
 */

/*
 *	vm_map_startup:
 *
 *	Initialize the vm_map module.  Must be called before
 *	any other vm_map routines.
 *
 *	Map and entry structures are allocated from the general
 *	purpose memory pool with some exceptions:
 *
 *	- The kernel map and kmem submap are allocated statically.
 *	- Kernel map entries are allocated out of a static pool.
 *
 *	These restrictions are necessary since malloc() uses the
 *	maps and requires map entries.
 */

extern char kstack[];
extern int inmprotect;

static int kentry_count;
static struct vm_zone kmapentzone_store, mapentzone_store, mapzone_store;
static vm_zone_t mapentzone, kmapentzone, mapzone;
static struct vm_object kmapentobj, mapentobj, mapobj;
#define MAP_ENTRY_INIT	128
struct vm_map_entry map_entry_init[MAX_MAPENT];
struct vm_map_entry kmap_entry_init[MAX_KMAPENT];
struct vm_map map_init[MAX_KMAP];

static void _vm_map_clip_end __P((vm_map_t, vm_map_entry_t, vm_offset_t));
static void _vm_map_clip_start __P((vm_map_t, vm_map_entry_t, vm_offset_t));
static vm_map_entry_t vm_map_entry_create __P((vm_map_t));
static void vm_map_entry_delete __P((vm_map_t, vm_map_entry_t));
static void vm_map_entry_dispose __P((vm_map_t, vm_map_entry_t));
static void vm_map_entry_unwire __P((vm_map_t, vm_map_entry_t));
static void vm_map_copy_entry __P((vm_map_t, vm_map_t, vm_map_entry_t,
		vm_map_entry_t));

void
vm_map_startup()
{
	mapzone = &mapzone_store;
	zbootinit(mapzone, "MAP", sizeof (struct vm_map),
		map_init, MAX_KMAP);
	kmapentzone = &kmapentzone_store;
	zbootinit(kmapentzone, "KMAP ENTRY", sizeof (struct vm_map_entry),
		kmap_entry_init, MAX_KMAPENT);
	mapentzone = &mapentzone_store;
	zbootinit(mapentzone, "MAP ENTRY", sizeof (struct vm_map_entry),
		map_entry_init, MAX_MAPENT);
}

/*
 * Allocate a vmspace structure, including a vm_map and pmap,
 * and initialize those structures.  The refcnt is set to 1.
 * The remaining fields must be initialized by the caller.
 */
struct vmspace *
vmspace_alloc(min, max, pageable)
	vm_offset_t min, max;
	int pageable;
{
	register struct vmspace *vm;

	MALLOC(vm, struct vmspace *, sizeof(struct vmspace), M_VMMAP, M_WAITOK);
	bzero(vm, (caddr_t) &vm->vm_startcopy - (caddr_t) vm);
	vm_map_init(&vm->vm_map, min, max, pageable);
	pmap_pinit(&vm->vm_pmap);
	vm->vm_map.pmap = &vm->vm_pmap;		/* XXX */
	vm->vm_refcnt = 1;
	return (vm);
}

void
vm_init2(void) {
	zinitna(kmapentzone, &kmapentobj,
		NULL, 0, cnt.v_page_count / 4, ZONE_INTERRUPT, 4);
	zinitna(mapentzone, &mapentobj,
		NULL, 0, 0, 0, 4);
	zinitna(mapzone, &mapobj,
		NULL, 0, 0, 0, 1);
	pmap_init2();
}

void
vmspace_free(vm)
	register struct vmspace *vm;
{

	if (vm->vm_refcnt == 0)
		panic("vmspace_free: attempt to free already freed vmspace");

	if (--vm->vm_refcnt == 0) {

		/*
		 * Lock the map, to wait out all other references to it.
		 * Delete all of the mappings and pages they hold, then call
		 * the pmap module to reclaim anything left.
		 */
		vm_map_lock(&vm->vm_map);
		(void) vm_map_delete(&vm->vm_map, vm->vm_map.min_offset,
		    vm->vm_map.max_offset);
		vm_map_unlock(&vm->vm_map);

		while( vm->vm_map.ref_count != 1)
			tsleep(&vm->vm_map.ref_count, PVM, "vmsfre", 0);
		--vm->vm_map.ref_count;
		pmap_release(&vm->vm_pmap);
		FREE(vm, M_VMMAP);
	} else {
		wakeup(&vm->vm_map.ref_count);
	}
}

/*
 *	vm_map_create:
 *
 *	Creates and returns a new empty VM map with
 *	the given physical map structure, and having
 *	the given lower and upper address bounds.
 */
vm_map_t
vm_map_create(pmap, min, max, pageable)
	pmap_t pmap;
	vm_offset_t min, max;
	boolean_t pageable;
{
	register vm_map_t result;

	result = zalloc(mapzone);
	vm_map_init(result, min, max, pageable);
	result->pmap = pmap;
	return (result);
}

/*
 * Initialize an existing vm_map structure
 * such as that in the vmspace structure.
 * The pmap is set elsewhere.
 */
void
vm_map_init(map, min, max, pageable)
	register struct vm_map *map;
	vm_offset_t min, max;
	boolean_t pageable;
{
	map->header.next = map->header.prev = &map->header;
	map->nentries = 0;
	map->size = 0;
	map->ref_count = 1;
	map->is_main_map = TRUE;
	map->system_map = 0;
	map->min_offset = min;
	map->max_offset = max;
	map->entries_pageable = pageable;
	map->first_free = &map->header;
	map->hint = &map->header;
	map->timestamp = 0;
	lockinit(&map->lock, PVM, "thrd_sleep", 0, 0);
	simple_lock_init(&map->ref_lock);
}

/*
 *	vm_map_entry_dispose:	[ internal use only ]
 *
 *	Inverse of vm_map_entry_create.
 */
static void
vm_map_entry_dispose(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{
	zfree((map->system_map || !mapentzone) ? kmapentzone : mapentzone, entry);
}

/*
 *	vm_map_entry_create:	[ internal use only ]
 *
 *	Allocates a VM map entry for insertion.
 *	No entry fields are filled in.  This routine is
 */
static vm_map_entry_t
vm_map_entry_create(map)
	vm_map_t map;
{
	return zalloc((map->system_map || !mapentzone) ? kmapentzone : mapentzone);
}

/*
 *	vm_map_entry_{un,}link:
 *
 *	Insert/remove entries from maps.
 */
#define	vm_map_entry_link(map, after_where, entry) \
		{ \
		(map)->nentries++; \
		(entry)->prev = (after_where); \
		(entry)->next = (after_where)->next; \
		(entry)->prev->next = (entry); \
		(entry)->next->prev = (entry); \
		}
#define	vm_map_entry_unlink(map, entry) \
		{ \
		(map)->nentries--; \
		(entry)->next->prev = (entry)->prev; \
		(entry)->prev->next = (entry)->next; \
		}

/*
 *	vm_map_reference:
 *
 *	Creates another valid reference to the given map.
 *
 */
void
vm_map_reference(map)
	register vm_map_t map;
{
	if (map == NULL)
		return;

	map->ref_count++;
}

/*
 *	vm_map_deallocate:
 *
 *	Removes a reference from the specified map,
 *	destroying it if no references remain.
 *	The map should not be locked.
 */
void
vm_map_deallocate(map)
	register vm_map_t map;
{
	register int c;

	if (map == NULL)
		return;

	c = map->ref_count;

	if (c == 0)
		panic("vm_map_deallocate: deallocating already freed map");

	if (c != 1) {
		--map->ref_count;
		wakeup(&map->ref_count);
		return;
	}
	/*
	 * Lock the map, to wait out all other references to it.
	 */

	vm_map_lock_drain_interlock(map);
	(void) vm_map_delete(map, map->min_offset, map->max_offset);
	--map->ref_count;
	if( map->ref_count != 0) {
		vm_map_unlock(map);
		return;
	}

	pmap_destroy(map->pmap);

	vm_map_unlock(map);

	zfree(mapzone, map);
}

/*
 *	SAVE_HINT:
 *
 *	Saves the specified entry as the hint for
 *	future lookups.
 */
#define	SAVE_HINT(map,value) \
		(map)->hint = (value);

/*
 *	vm_map_lookup_entry:	[ internal use only ]
 *
 *	Finds the map entry containing (or
 *	immediately preceding) the specified address
 *	in the given map; the entry is returned
 *	in the "entry" parameter.  The boolean
 *	result indicates whether the address is
 *	actually contained in the map.
 */
boolean_t
vm_map_lookup_entry(map, address, entry)
	register vm_map_t map;
	register vm_offset_t address;
	vm_map_entry_t *entry;	/* OUT */
{
	register vm_map_entry_t cur;
	register vm_map_entry_t last;

	/*
	 * Start looking either from the head of the list, or from the hint.
	 */

	cur = map->hint;

	if (cur == &map->header)
		cur = cur->next;

	if (address >= cur->start) {
		/*
		 * Go from hint to end of list.
		 *
		 * But first, make a quick check to see if we are already looking
		 * at the entry we want (which is usually the case). Note also
		 * that we don't need to save the hint here... it is the same
		 * hint (unless we are at the header, in which case the hint
		 * didn't buy us anything anyway).
		 */
		last = &map->header;
		if ((cur != last) && (cur->end > address)) {
			*entry = cur;
			return (TRUE);
		}
	} else {
		/*
		 * Go from start to hint, *inclusively*
		 */
		last = cur->next;
		cur = map->header.next;
	}

	/*
	 * Search linearly
	 */

	while (cur != last) {
		if (cur->end > address) {
			if (address >= cur->start) {
				/*
				 * Save this lookup for future hints, and
				 * return
				 */

				*entry = cur;
				SAVE_HINT(map, cur);
				return (TRUE);
			}
			break;
		}
		cur = cur->next;
	}
	*entry = cur->prev;
	SAVE_HINT(map, *entry);
	return (FALSE);
}

/*
 *	vm_map_insert:
 *
 *	Inserts the given whole VM object into the target
 *	map at the specified address range.  The object's
 *	size should match that of the address range.
 *
 *	Requires that the map be locked, and leaves it so.
 */
int
vm_map_insert(map, object, offset, start, end, prot, max, cow)
	vm_map_t map;
	vm_object_t object;
	vm_ooffset_t offset;
	vm_offset_t start;
	vm_offset_t end;
	vm_prot_t prot, max;
	int cow;
{
	register vm_map_entry_t new_entry;
	register vm_map_entry_t prev_entry;
	vm_map_entry_t temp_entry;
	vm_object_t prev_object;
	u_char protoeflags;

	if ((object != NULL) && (cow & MAP_NOFAULT)) {
		panic("vm_map_insert: paradoxical MAP_NOFAULT request");
	}

	/*
	 * Check that the start and end points are not bogus.
	 */

	if ((start < map->min_offset) || (end > map->max_offset) ||
	    (start >= end))
		return (KERN_INVALID_ADDRESS);

	/*
	 * Find the entry prior to the proposed starting address; if it's part
	 * of an existing entry, this range is bogus.
	 */

	if (vm_map_lookup_entry(map, start, &temp_entry))
		return (KERN_NO_SPACE);

	prev_entry = temp_entry;

	/*
	 * Assert that the next entry doesn't overlap the end point.
	 */

	if ((prev_entry->next != &map->header) &&
	    (prev_entry->next->start < end))
		return (KERN_NO_SPACE);

	protoeflags = 0;
	if (cow & MAP_COPY_NEEDED)
		protoeflags |= MAP_ENTRY_NEEDS_COPY;

	if (cow & MAP_COPY_ON_WRITE)
		protoeflags |= MAP_ENTRY_COW;

	if (cow & MAP_NOFAULT)
		protoeflags |= MAP_ENTRY_NOFAULT;

	/*
	 * See if we can avoid creating a new entry by extending one of our
	 * neighbors.  Or at least extend the object.
	 */

	if ((object == NULL) &&
	    (prev_entry != &map->header) &&
	    (( prev_entry->eflags & (MAP_ENTRY_IS_A_MAP | MAP_ENTRY_IS_SUB_MAP)) == 0) &&
	    (prev_entry->end == start) &&
	    (prev_entry->wired_count == 0)) {
		

		if ((protoeflags == prev_entry->eflags) &&
		    ((cow & MAP_NOFAULT) ||
		     vm_object_coalesce(prev_entry->object.vm_object,
					OFF_TO_IDX(prev_entry->offset),
					(vm_size_t) (prev_entry->end - prev_entry->start),
					(vm_size_t) (end - prev_entry->end)))) {

			/*
			 * Coalesced the two objects.  Can we extend the
			 * previous map entry to include the new range?
			 */
			if ((prev_entry->inheritance == VM_INHERIT_DEFAULT) &&
			    (prev_entry->protection == prot) &&
			    (prev_entry->max_protection == max)) {

				map->size += (end - prev_entry->end);
				prev_entry->end = end;
				if ((cow & MAP_NOFAULT) == 0) {
					prev_object = prev_entry->object.vm_object;
					default_pager_convert_to_swapq(prev_object);
				}
				return (KERN_SUCCESS);
			}
			else {
				object = prev_entry->object.vm_object;
				offset = prev_entry->offset + (prev_entry->end -
							       prev_entry->start);

				vm_object_reference(object);
			}
		}
	}

	/*
	 * Create a new entry
	 */

	new_entry = vm_map_entry_create(map);
	new_entry->start = start;
	new_entry->end = end;

	new_entry->eflags = protoeflags;
	new_entry->object.vm_object = object;
	new_entry->offset = offset;

	if (map->is_main_map) {
		new_entry->inheritance = VM_INHERIT_DEFAULT;
		new_entry->protection = prot;
		new_entry->max_protection = max;
		new_entry->wired_count = 0;
	}
	/*
	 * Insert the new entry into the list
	 */

	vm_map_entry_link(map, prev_entry, new_entry);
	map->size += new_entry->end - new_entry->start;

	/*
	 * Update the free space hint
	 */
	if ((map->first_free == prev_entry) &&
		(prev_entry->end >= new_entry->start))
		map->first_free = new_entry;

	default_pager_convert_to_swapq(object);
	return (KERN_SUCCESS);
}

/*
 * Find sufficient space for `length' bytes in the given map, starting at
 * `start'.  The map must be locked.  Returns 0 on success, 1 on no space.
 */
int
vm_map_findspace(map, start, length, addr)
	register vm_map_t map;
	register vm_offset_t start;
	vm_size_t length;
	vm_offset_t *addr;
{
	register vm_map_entry_t entry, next;
	register vm_offset_t end;

	if (start < map->min_offset)
		start = map->min_offset;
	if (start > map->max_offset)
		return (1);

	/*
	 * Look for the first possible address; if there's already something
	 * at this address, we have to start after it.
	 */
	if (start == map->min_offset) {
		if ((entry = map->first_free) != &map->header)
			start = entry->end;
	} else {
		vm_map_entry_t tmp;

		if (vm_map_lookup_entry(map, start, &tmp))
			start = tmp->end;
		entry = tmp;
	}

	/*
	 * Look through the rest of the map, trying to fit a new region in the
	 * gap between existing regions, or after the very last region.
	 */
	for (;; start = (entry = next)->end) {
		/*
		 * Find the end of the proposed new region.  Be sure we didn't
		 * go beyond the end of the map, or wrap around the address;
		 * if so, we lose.  Otherwise, if this is the last entry, or
		 * if the proposed new region fits before the next entry, we
		 * win.
		 */
		end = start + length;
		if (end > map->max_offset || end < start)
			return (1);
		next = entry->next;
		if (next == &map->header || next->start >= end)
			break;
	}
	SAVE_HINT(map, entry);
	*addr = start;
	if (map == kernel_map && round_page(start + length) > kernel_vm_end)
		pmap_growkernel(round_page(start + length));
	return (0);
}

/*
 *	vm_map_find finds an unallocated region in the target address
 *	map with the given length.  The search is defined to be
 *	first-fit from the specified address; the region found is
 *	returned in the same parameter.
 *
 */
int
vm_map_find(map, object, offset, addr, length, find_space, prot, max, cow)
	vm_map_t map;
	vm_object_t object;
	vm_ooffset_t offset;
	vm_offset_t *addr;	/* IN/OUT */
	vm_size_t length;
	boolean_t find_space;
	vm_prot_t prot, max;
	int cow;
{
	register vm_offset_t start;
	int result, s = 0;

	start = *addr;

	if (map == kmem_map || map == mb_map)
		s = splvm();

	vm_map_lock(map);
	if (find_space) {
		if (vm_map_findspace(map, start, length, addr)) {
			vm_map_unlock(map);
			if (map == kmem_map || map == mb_map)
				splx(s);
			return (KERN_NO_SPACE);
		}
		start = *addr;
	}
	result = vm_map_insert(map, object, offset,
		start, start + length, prot, max, cow);
	vm_map_unlock(map);

	if (map == kmem_map || map == mb_map)
		splx(s);

	return (result);
}

/*
 *	vm_map_simplify_entry:
 *
 *	Simplify the given map entry by merging with either neighbor.
 */
void
vm_map_simplify_entry(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{
	vm_map_entry_t next, prev;
	vm_size_t prevsize, esize;

	if (entry->eflags & (MAP_ENTRY_IS_SUB_MAP|MAP_ENTRY_IS_A_MAP))
		return;

	prev = entry->prev;
	if (prev != &map->header) {
		prevsize = prev->end - prev->start;
		if ( (prev->end == entry->start) &&
		     (prev->object.vm_object == entry->object.vm_object) &&
		     (!prev->object.vm_object || (prev->object.vm_object->behavior == entry->object.vm_object->behavior)) &&
		     (!prev->object.vm_object ||
			(prev->offset + prevsize == entry->offset)) &&
		     (prev->eflags == entry->eflags) &&
		     (prev->protection == entry->protection) &&
		     (prev->max_protection == entry->max_protection) &&
		     (prev->inheritance == entry->inheritance) &&
		     (prev->wired_count == entry->wired_count)) {
			if (map->first_free == prev)
				map->first_free = entry;
			if (map->hint == prev)
				map->hint = entry;
			vm_map_entry_unlink(map, prev);
			entry->start = prev->start;
			entry->offset = prev->offset;
			if (prev->object.vm_object)
				vm_object_deallocate(prev->object.vm_object);
			vm_map_entry_dispose(map, prev);
		}
	}

	next = entry->next;
	if (next != &map->header) {
		esize = entry->end - entry->start;
		if ((entry->end == next->start) &&
		    (next->object.vm_object == entry->object.vm_object) &&
		    (!next->object.vm_object || (next->object.vm_object->behavior == entry->object.vm_object->behavior)) &&
		     (!entry->object.vm_object ||
			(entry->offset + esize == next->offset)) &&
		    (next->eflags == entry->eflags) &&
		    (next->protection == entry->protection) &&
		    (next->max_protection == entry->max_protection) &&
		    (next->inheritance == entry->inheritance) &&
		    (next->wired_count == entry->wired_count)) {
			if (map->first_free == next)
				map->first_free = entry;
			if (map->hint == next)
				map->hint = entry;
			vm_map_entry_unlink(map, next);
			entry->end = next->end;
			if (next->object.vm_object)
				vm_object_deallocate(next->object.vm_object);
			vm_map_entry_dispose(map, next);
	        }
	}
}
/*
 *	vm_map_clip_start:	[ internal use only ]
 *
 *	Asserts that the given entry begins at or after
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
#define vm_map_clip_start(map, entry, startaddr) \
{ \
	if (startaddr > entry->start) \
		_vm_map_clip_start(map, entry, startaddr); \
}

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
static void
_vm_map_clip_start(map, entry, start)
	register vm_map_t map;
	register vm_map_entry_t entry;
	register vm_offset_t start;
{
	register vm_map_entry_t new_entry;

	/*
	 * Split off the front portion -- note that we must insert the new
	 * entry BEFORE this one, so that this entry has the specified
	 * starting address.
	 */

	vm_map_simplify_entry(map, entry);

	/*
	 * If there is no object backing this entry, we might as well create
	 * one now.  If we defer it, an object can get created after the map
	 * is clipped, and individual objects will be created for the split-up
	 * map.  This is a bit of a hack, but is also about the best place to
	 * put this improvement.
	 */

	if (entry->object.vm_object == NULL) {
			vm_object_t object;

			object = vm_object_allocate(OBJT_DEFAULT,
					OFF_TO_IDX(entry->end - entry->start));
			entry->object.vm_object = object;
			entry->offset = 0;
	}

	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;

	new_entry->end = start;
	entry->offset += (start - entry->start);
	entry->start = start;

	vm_map_entry_link(map, entry->prev, new_entry);

	if (entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP))
		vm_map_reference(new_entry->object.share_map);
	else
		vm_object_reference(new_entry->object.vm_object);
}

/*
 *	vm_map_clip_end:	[ internal use only ]
 *
 *	Asserts that the given entry ends at or before
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */

#define vm_map_clip_end(map, entry, endaddr) \
{ \
	if (endaddr < entry->end) \
		_vm_map_clip_end(map, entry, endaddr); \
}

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
static void
_vm_map_clip_end(map, entry, end)
	register vm_map_t map;
	register vm_map_entry_t entry;
	register vm_offset_t end;
{
	register vm_map_entry_t new_entry;

	/*
	 * If there is no object backing this entry, we might as well create
	 * one now.  If we defer it, an object can get created after the map
	 * is clipped, and individual objects will be created for the split-up
	 * map.  This is a bit of a hack, but is also about the best place to
	 * put this improvement.
	 */

	if (entry->object.vm_object == NULL) {
			vm_object_t object;

			object = vm_object_allocate(OBJT_DEFAULT,
					OFF_TO_IDX(entry->end - entry->start));
			entry->object.vm_object = object;
			entry->offset = 0;
	}

	/*
	 * Create a new entry and insert it AFTER the specified entry
	 */

	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;

	new_entry->start = entry->end = end;
	new_entry->offset += (end - entry->start);

	vm_map_entry_link(map, entry, new_entry);

	if (entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP))
		vm_map_reference(new_entry->object.share_map);
	else
		vm_object_reference(new_entry->object.vm_object);
}

/*
 *	VM_MAP_RANGE_CHECK:	[ internal use only ]
 *
 *	Asserts that the starting and ending region
 *	addresses fall within the valid range of the map.
 */
#define	VM_MAP_RANGE_CHECK(map, start, end)		\
		{					\
		if (start < vm_map_min(map))		\
			start = vm_map_min(map);	\
		if (end > vm_map_max(map))		\
			end = vm_map_max(map);		\
		if (start > end)			\
			start = end;			\
		}

/*
 *	vm_map_submap:		[ kernel use only ]
 *
 *	Mark the given range as handled by a subordinate map.
 *
 *	This range must have been created with vm_map_find,
 *	and no other operations may have been performed on this
 *	range prior to calling vm_map_submap.
 *
 *	Only a limited number of operations can be performed
 *	within this rage after calling vm_map_submap:
 *		vm_fault
 *	[Don't try vm_map_copy!]
 *
 *	To remove a submapping, one must first remove the
 *	range from the superior map, and then destroy the
 *	submap (if desired).  [Better yet, don't try it.]
 */
int
vm_map_submap(map, start, end, submap)
	register vm_map_t map;
	register vm_offset_t start;
	register vm_offset_t end;
	vm_map_t submap;
{
	vm_map_entry_t entry;
	register int result = KERN_INVALID_ARGUMENT;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	} else
		entry = entry->next;

	vm_map_clip_end(map, entry, end);

	if ((entry->start == start) && (entry->end == end) &&
	    ((entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_COW)) == 0) &&
	    (entry->object.vm_object == NULL)) {
		entry->eflags |= MAP_ENTRY_IS_SUB_MAP;
		vm_map_reference(entry->object.sub_map = submap);
		result = KERN_SUCCESS;
	}
	vm_map_unlock(map);

	return (result);
}

/*
 *	vm_map_protect:
 *
 *	Sets the protection of the specified address
 *	region in the target map.  If "set_max" is
 *	specified, the maximum protection is to be set;
 *	otherwise, only the current protection is affected.
 */
int
vm_map_protect(map, start, end, new_prot, set_max)
	register vm_map_t map;
	register vm_offset_t start;
	register vm_offset_t end;
	register vm_prot_t new_prot;
	register boolean_t set_max;
{
	register vm_map_entry_t current;
	vm_map_entry_t entry;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	} else {
		entry = entry->next;
	}

	/*
	 * Make a first pass to check for protection violations.
	 */

	current = entry;
	while ((current != &map->header) && (current->start < end)) {
		if (current->eflags & MAP_ENTRY_IS_SUB_MAP) {
			vm_map_unlock(map);
			return (KERN_INVALID_ARGUMENT);
		}
		if ((new_prot & current->max_protection) != new_prot) {
			vm_map_unlock(map);
			return (KERN_PROTECTION_FAILURE);
		}
		current = current->next;
	}

	/*
	 * Go back and fix up protections. [Note that clipping is not
	 * necessary the second time.]
	 */

	current = entry;

	while ((current != &map->header) && (current->start < end)) {
		vm_prot_t old_prot;

		vm_map_clip_end(map, current, end);

		old_prot = current->protection;
		if (set_max)
			current->protection =
			    (current->max_protection = new_prot) &
			    old_prot;
		else
			current->protection = new_prot;

		/*
		 * Update physical map if necessary. Worry about copy-on-write
		 * here -- CHECK THIS XXX
		 */

		if (current->protection != old_prot) {
#define MASK(entry)	(((entry)->eflags & MAP_ENTRY_COW) ? ~VM_PROT_WRITE : \
							VM_PROT_ALL)
#define	max(a,b)	((a) > (b) ? (a) : (b))

			if (current->eflags & MAP_ENTRY_IS_A_MAP) {
				vm_map_entry_t share_entry;
				vm_offset_t share_end;

				vm_map_lock(current->object.share_map);
				(void) vm_map_lookup_entry(
				    current->object.share_map,
				    current->offset,
				    &share_entry);
				share_end = current->offset +
				    (current->end - current->start);
				while ((share_entry !=
					&current->object.share_map->header) &&
				    (share_entry->start < share_end)) {

					pmap_protect(map->pmap,
					    (max(share_entry->start,
						    current->offset) -
						current->offset +
						current->start),
					    min(share_entry->end,
						share_end) -
					    current->offset +
					    current->start,
					    current->protection &
					    MASK(share_entry));

					share_entry = share_entry->next;
				}
				vm_map_unlock(current->object.share_map);
			} else
				pmap_protect(map->pmap, current->start,
				    current->end,
				    current->protection & MASK(entry));
#undef	max
#undef	MASK
		}

		vm_map_simplify_entry(map, current);

		current = current->next;
	}

	vm_map_unlock(map);
	return (KERN_SUCCESS);
}

/*
 *	vm_map_madvise:
 *
 * 	This routine traverses a processes map handling the madvise
 *	system call.
 */
void
vm_map_madvise(map, pmap, start, end, advise)
	vm_map_t map;
	pmap_t pmap;
	vm_offset_t start, end;
	int advise;
{
	register vm_map_entry_t current;
	vm_map_entry_t entry;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	} else
		entry = entry->next;

	for(current = entry;
		(current != &map->header) && (current->start < end);
		current = current->next) {
		vm_size_t size = current->end - current->start;

		if (current->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP)) {
			continue;
		}

		/*
		 * Create an object if needed
		 */
		if (current->object.vm_object == NULL) {
			vm_object_t object;
			object = vm_object_allocate(OBJT_DEFAULT, OFF_TO_IDX(size));
			current->object.vm_object = object;
			current->offset = 0;
		}

		vm_map_clip_end(map, current, end);
		switch (advise) {
	case MADV_NORMAL:
			current->object.vm_object->behavior = OBJ_NORMAL;
			break;
	case MADV_SEQUENTIAL:
			current->object.vm_object->behavior = OBJ_SEQUENTIAL;
			break;
	case MADV_RANDOM:
			current->object.vm_object->behavior = OBJ_RANDOM;
			break;
	/*
	 * Right now, we could handle DONTNEED and WILLNEED with common code.
	 * They are mostly the same, except for the potential async reads (NYI).
	 */
	case MADV_FREE:
	case MADV_DONTNEED:
			{
				vm_pindex_t pindex;
				int count;
				size = current->end - current->start;
				pindex = OFF_TO_IDX(entry->offset);
				count = OFF_TO_IDX(size);
				/*
				 * MADV_DONTNEED removes the page from all
				 * pmaps, so pmap_remove is not necessary.
				 */
				vm_object_madvise(current->object.vm_object,
					pindex, count, advise);
			}
			break;

	case MADV_WILLNEED:
			{
				vm_pindex_t pindex;
				int count;
				size = current->end - current->start;
				pindex = OFF_TO_IDX(current->offset);
				count = OFF_TO_IDX(size);
				vm_object_madvise(current->object.vm_object,
					pindex, count, advise);
				pmap_object_init_pt(pmap, current->start,
					current->object.vm_object, pindex,
					(count << PAGE_SHIFT), 0);
			}
			break;

	default:
			break;
		}
	}

	vm_map_simplify_entry(map, entry);
	vm_map_unlock(map);
	return;
}	


/*
 *	vm_map_inherit:
 *
 *	Sets the inheritance of the specified address
 *	range in the target map.  Inheritance
 *	affects how the map will be shared with
 *	child maps at the time of vm_map_fork.
 */
int
vm_map_inherit(map, start, end, new_inheritance)
	register vm_map_t map;
	register vm_offset_t start;
	register vm_offset_t end;
	register vm_inherit_t new_inheritance;
{
	register vm_map_entry_t entry;
	vm_map_entry_t temp_entry;

	switch (new_inheritance) {
	case VM_INHERIT_NONE:
	case VM_INHERIT_COPY:
	case VM_INHERIT_SHARE:
		break;
	default:
		return (KERN_INVALID_ARGUMENT);
	}

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &temp_entry)) {
		entry = temp_entry;
		vm_map_clip_start(map, entry, start);
	} else
		entry = temp_entry->next;

	while ((entry != &map->header) && (entry->start < end)) {
		vm_map_clip_end(map, entry, end);

		entry->inheritance = new_inheritance;

		entry = entry->next;
	}

	vm_map_simplify_entry(map, temp_entry);
	vm_map_unlock(map);
	return (KERN_SUCCESS);
}

/*
 * Implement the semantics of mlock
 */
int
vm_map_user_pageable(map, start, end, new_pageable)
	register vm_map_t map;
	register vm_offset_t start;
	register vm_offset_t end;
	register boolean_t new_pageable;
{
	register vm_map_entry_t entry;
	vm_map_entry_t start_entry;
	register vm_offset_t failed = 0;
	int rv;

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &start_entry) == FALSE) {
		vm_map_unlock(map);
		return (KERN_INVALID_ADDRESS);
	}

	if (new_pageable) {

		entry = start_entry;
		vm_map_clip_start(map, entry, start);

		/*
		 * Now decrement the wiring count for each region. If a region
		 * becomes completely unwired, unwire its physical pages and
		 * mappings.
		 */
		vm_map_set_recursive(map);

		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			if (entry->eflags & MAP_ENTRY_USER_WIRED) {
				vm_map_clip_end(map, entry, end);
				entry->eflags &= ~MAP_ENTRY_USER_WIRED;
				entry->wired_count--;
				if (entry->wired_count == 0)
					vm_fault_unwire(map, entry->start, entry->end);
			}
			entry = entry->next;
		}
		vm_map_simplify_entry(map, start_entry);
		vm_map_clear_recursive(map);
	} else {

		/*
		 * Because of the possiblity of blocking, etc.  We restart
		 * through the process's map entries from beginning so that
		 * we don't end up depending on a map entry that could have
		 * changed.
		 */
	rescan:

		entry = start_entry;

		while ((entry != &map->header) && (entry->start < end)) {

			if (entry->eflags & MAP_ENTRY_USER_WIRED) {
				entry = entry->next;
				continue;
			}
			
			if (entry->wired_count != 0) {
				entry->wired_count++;
				entry->eflags |= MAP_ENTRY_USER_WIRED;
				entry = entry->next;
				continue;
			}

			/* Here on entry being newly wired */

			if ((entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP)) == 0) {
				int copyflag = entry->eflags & MAP_ENTRY_NEEDS_COPY;
				if (copyflag && ((entry->protection & VM_PROT_WRITE) != 0)) {

					vm_object_shadow(&entry->object.vm_object,
					    &entry->offset,
					    OFF_TO_IDX(entry->end
						- entry->start));
					entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;

				} else if (entry->object.vm_object == NULL) {

					entry->object.vm_object =
					    vm_object_allocate(OBJT_DEFAULT,
						OFF_TO_IDX(entry->end - entry->start));
					entry->offset = (vm_offset_t) 0;

				}
				default_pager_convert_to_swapq(entry->object.vm_object);
			}

			vm_map_clip_start(map, entry, start);
			vm_map_clip_end(map, entry, end);

			entry->wired_count++;
			entry->eflags |= MAP_ENTRY_USER_WIRED;

			/* First we need to allow map modifications */
			vm_map_set_recursive(map);
			lockmgr(&map->lock, LK_DOWNGRADE,(void *)0, curproc);

			rv = vm_fault_user_wire(map, entry->start, entry->end);
			if (rv) {

				entry->wired_count--;
				entry->eflags &= ~MAP_ENTRY_USER_WIRED;

				vm_map_clear_recursive(map);
				vm_map_unlock(map);
				
				(void) vm_map_user_pageable(map, start, entry->start, TRUE);
				return rv;
			}

			vm_map_clear_recursive(map);
			lockmgr(&map->lock, LK_UPGRADE, (void *)0, curproc);

			goto rescan;
		}
	}
	vm_map_unlock(map);
	return KERN_SUCCESS;
}

/*
 *	vm_map_pageable:
 *
 *	Sets the pageability of the specified address
 *	range in the target map.  Regions specified
 *	as not pageable require locked-down physical
 *	memory and physical page maps.
 *
 *	The map must not be locked, but a reference
 *	must remain to the map throughout the call.
 */
int
vm_map_pageable(map, start, end, new_pageable)
	register vm_map_t map;
	register vm_offset_t start;
	register vm_offset_t end;
	register boolean_t new_pageable;
{
	register vm_map_entry_t entry;
	vm_map_entry_t start_entry;
	register vm_offset_t failed = 0;
	int rv;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	/*
	 * Only one pageability change may take place at one time, since
	 * vm_fault assumes it will be called only once for each
	 * wiring/unwiring.  Therefore, we have to make sure we're actually
	 * changing the pageability for the entire region.  We do so before
	 * making any changes.
	 */

	if (vm_map_lookup_entry(map, start, &start_entry) == FALSE) {
		vm_map_unlock(map);
		return (KERN_INVALID_ADDRESS);
	}
	entry = start_entry;

	/*
	 * Actions are rather different for wiring and unwiring, so we have
	 * two separate cases.
	 */

	if (new_pageable) {

		vm_map_clip_start(map, entry, start);

		/*
		 * Unwiring.  First ensure that the range to be unwired is
		 * really wired down and that there are no holes.
		 */
		while ((entry != &map->header) && (entry->start < end)) {

			if (entry->wired_count == 0 ||
			    (entry->end < end &&
				(entry->next == &map->header ||
				    entry->next->start > entry->end))) {
				vm_map_unlock(map);
				return (KERN_INVALID_ARGUMENT);
			}
			entry = entry->next;
		}

		/*
		 * Now decrement the wiring count for each region. If a region
		 * becomes completely unwired, unwire its physical pages and
		 * mappings.
		 */
		vm_map_set_recursive(map);

		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			vm_map_clip_end(map, entry, end);

			entry->wired_count--;
			if (entry->wired_count == 0)
				vm_fault_unwire(map, entry->start, entry->end);

			entry = entry->next;
		}
		vm_map_simplify_entry(map, start_entry);
		vm_map_clear_recursive(map);
	} else {
		/*
		 * Wiring.  We must do this in two passes:
		 *
		 * 1.  Holding the write lock, we create any shadow or zero-fill
		 * objects that need to be created. Then we clip each map
		 * entry to the region to be wired and increment its wiring
		 * count.  We create objects before clipping the map entries
		 * to avoid object proliferation.
		 *
		 * 2.  We downgrade to a read lock, and call vm_fault_wire to
		 * fault in the pages for any newly wired area (wired_count is
		 * 1).
		 *
		 * Downgrading to a read lock for vm_fault_wire avoids a possible
		 * deadlock with another process that may have faulted on one
		 * of the pages to be wired (it would mark the page busy,
		 * blocking us, then in turn block on the map lock that we
		 * hold).  Because of problems in the recursive lock package,
		 * we cannot upgrade to a write lock in vm_map_lookup.  Thus,
		 * any actions that require the write lock must be done
		 * beforehand.  Because we keep the read lock on the map, the
		 * copy-on-write status of the entries we modify here cannot
		 * change.
		 */

		/*
		 * Pass 1.
		 */
		while ((entry != &map->header) && (entry->start < end)) {
			if (entry->wired_count == 0) {

				/*
				 * Perform actions of vm_map_lookup that need
				 * the write lock on the map: create a shadow
				 * object for a copy-on-write region, or an
				 * object for a zero-fill region.
				 *
				 * We don't have to do this for entries that
				 * point to sharing maps, because we won't
				 * hold the lock on the sharing map.
				 */
				if ((entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP)) == 0) {
					int copyflag = entry->eflags & MAP_ENTRY_NEEDS_COPY;
					if (copyflag &&
					    ((entry->protection & VM_PROT_WRITE) != 0)) {

						vm_object_shadow(&entry->object.vm_object,
						    &entry->offset,
						    OFF_TO_IDX(entry->end
							- entry->start));
						entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
					} else if (entry->object.vm_object == NULL) {
						entry->object.vm_object =
						    vm_object_allocate(OBJT_DEFAULT,
							OFF_TO_IDX(entry->end - entry->start));
						entry->offset = (vm_offset_t) 0;
					}
					default_pager_convert_to_swapq(entry->object.vm_object);
				}
			}
			vm_map_clip_start(map, entry, start);
			vm_map_clip_end(map, entry, end);
			entry->wired_count++;

			/*
			 * Check for holes
			 */
			if (entry->end < end &&
			    (entry->next == &map->header ||
				entry->next->start > entry->end)) {
				/*
				 * Found one.  Object creation actions do not
				 * need to be undone, but the wired counts
				 * need to be restored.
				 */
				while (entry != &map->header && entry->end > start) {
					entry->wired_count--;
					entry = entry->prev;
				}
				vm_map_unlock(map);
				return (KERN_INVALID_ARGUMENT);
			}
			entry = entry->next;
		}

		/*
		 * Pass 2.
		 */

		/*
		 * HACK HACK HACK HACK
		 *
		 * If we are wiring in the kernel map or a submap of it,
		 * unlock the map to avoid deadlocks.  We trust that the
		 * kernel is well-behaved, and therefore will not do
		 * anything destructive to this region of the map while
		 * we have it unlocked.  We cannot trust user processes
		 * to do the same.
		 *
		 * HACK HACK HACK HACK
		 */
		if (vm_map_pmap(map) == kernel_pmap) {
			vm_map_unlock(map);	/* trust me ... */
		} else {
			vm_map_set_recursive(map);
			lockmgr(&map->lock, LK_DOWNGRADE, (void*)0, curproc);
		}

		rv = 0;
		entry = start_entry;
		while (entry != &map->header && entry->start < end) {
			/*
			 * If vm_fault_wire fails for any page we need to undo
			 * what has been done.  We decrement the wiring count
			 * for those pages which have not yet been wired (now)
			 * and unwire those that have (later).
			 *
			 * XXX this violates the locking protocol on the map,
			 * needs to be fixed.
			 */
			if (rv)
				entry->wired_count--;
			else if (entry->wired_count == 1) {
				rv = vm_fault_wire(map, entry->start, entry->end);
				if (rv) {
					failed = entry->start;
					entry->wired_count--;
				}
			}
			entry = entry->next;
		}

		if (vm_map_pmap(map) == kernel_pmap) {
			vm_map_lock(map);
		} else {
			vm_map_clear_recursive(map);
		}
		if (rv) {
			vm_map_unlock(map);
			(void) vm_map_pageable(map, start, failed, TRUE);
			return (rv);
		}
		vm_map_simplify_entry(map, start_entry);
	}

	vm_map_unlock(map);

	return (KERN_SUCCESS);
}

/*
 * vm_map_clean
 *
 * Push any dirty cached pages in the address range to their pager.
 * If syncio is TRUE, dirty pages are written synchronously.
 * If invalidate is TRUE, any cached pages are freed as well.
 *
 * Returns an error if any part of the specified range is not mapped.
 */
int
vm_map_clean(map, start, end, syncio, invalidate)
	vm_map_t map;
	vm_offset_t start;
	vm_offset_t end;
	boolean_t syncio;
	boolean_t invalidate;
{
	register vm_map_entry_t current;
	vm_map_entry_t entry;
	vm_size_t size;
	vm_object_t object;
	vm_ooffset_t offset;

	vm_map_lock_read(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &entry)) {
		vm_map_unlock_read(map);
		return (KERN_INVALID_ADDRESS);
	}
	/*
	 * Make a first pass to check for holes.
	 */
	for (current = entry; current->start < end; current = current->next) {
		if (current->eflags & MAP_ENTRY_IS_SUB_MAP) {
			vm_map_unlock_read(map);
			return (KERN_INVALID_ARGUMENT);
		}
		if (end > current->end &&
		    (current->next == &map->header ||
			current->end != current->next->start)) {
			vm_map_unlock_read(map);
			return (KERN_INVALID_ADDRESS);
		}
	}

	/*
	 * Make a second pass, cleaning/uncaching pages from the indicated
	 * objects as we go.
	 */
	for (current = entry; current->start < end; current = current->next) {
		offset = current->offset + (start - current->start);
		size = (end <= current->end ? end : current->end) - start;
		if (current->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP)) {
			register vm_map_t smap;
			vm_map_entry_t tentry;
			vm_size_t tsize;

			smap = current->object.share_map;
			vm_map_lock_read(smap);
			(void) vm_map_lookup_entry(smap, offset, &tentry);
			tsize = tentry->end - offset;
			if (tsize < size)
				size = tsize;
			object = tentry->object.vm_object;
			offset = tentry->offset + (offset - tentry->start);
			vm_map_unlock_read(smap);
		} else {
			object = current->object.vm_object;
		}
		/*
		 * Note that there is absolutely no sense in writing out
		 * anonymous objects, so we track down the vnode object
		 * to write out.
		 * We invalidate (remove) all pages from the address space
		 * anyway, for semantic correctness.
		 */
		while (object->backing_object) {
			object = object->backing_object;
			offset += object->backing_object_offset;
			if (object->size < OFF_TO_IDX( offset + size))
				size = IDX_TO_OFF(object->size) - offset;
		}
		if (invalidate)
			pmap_remove(vm_map_pmap(map), current->start,
				current->start + size);
		if (object && (object->type == OBJT_VNODE)) {
			/*
			 * Flush pages if writing is allowed. XXX should we continue
			 * on an error?
			 *
			 * XXX Doing async I/O and then removing all the pages from
			 *     the object before it completes is probably a very bad
			 *     idea.
			 */
			if (current->protection & VM_PROT_WRITE) {
		   	    	vm_object_page_clean(object,
					OFF_TO_IDX(offset),
					OFF_TO_IDX(offset + size),
					(syncio||invalidate)?1:0, TRUE);
				if (invalidate)
					vm_object_page_remove(object,
						OFF_TO_IDX(offset),
						OFF_TO_IDX(offset + size),
						FALSE);
			}
		}
		start += size;
	}

	vm_map_unlock_read(map);
	return (KERN_SUCCESS);
}

/*
 *	vm_map_entry_unwire:	[ internal use only ]
 *
 *	Make the region specified by this entry pageable.
 *
 *	The map in question should be locked.
 *	[This is the reason for this routine's existence.]
 */
static void 
vm_map_entry_unwire(map, entry)
	vm_map_t map;
	register vm_map_entry_t entry;
{
	vm_fault_unwire(map, entry->start, entry->end);
	entry->wired_count = 0;
}

/*
 *	vm_map_entry_delete:	[ internal use only ]
 *
 *	Deallocate the given entry from the target map.
 */
static void
vm_map_entry_delete(map, entry)
	register vm_map_t map;
	register vm_map_entry_t entry;
{
	vm_map_entry_unlink(map, entry);
	map->size -= entry->end - entry->start;

	if (entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP)) {
		vm_map_deallocate(entry->object.share_map);
	} else {
		vm_object_deallocate(entry->object.vm_object);
	}

	vm_map_entry_dispose(map, entry);
}

/*
 *	vm_map_delete:	[ internal use only ]
 *
 *	Deallocates the given address range from the target
 *	map.
 *
 *	When called with a sharing map, removes pages from
 *	that region from all physical maps.
 */
int
vm_map_delete(map, start, end)
	register vm_map_t map;
	vm_offset_t start;
	register vm_offset_t end;
{
	register vm_map_entry_t entry;
	vm_map_entry_t first_entry;

	/*
	 * Find the start of the region, and clip it
	 */

	if (!vm_map_lookup_entry(map, start, &first_entry))
		entry = first_entry->next;
	else {
		entry = first_entry;
		vm_map_clip_start(map, entry, start);

		/*
		 * Fix the lookup hint now, rather than each time though the
		 * loop.
		 */

		SAVE_HINT(map, entry->prev);
	}

	/*
	 * Save the free space hint
	 */

	if (entry == &map->header) {
		map->first_free = &map->header;
	} else if (map->first_free->start >= start)
		map->first_free = entry->prev;

	/*
	 * Step through all entries in this region
	 */

	while ((entry != &map->header) && (entry->start < end)) {
		vm_map_entry_t next;
		vm_offset_t s, e;
		vm_object_t object;
		vm_ooffset_t offset;

		vm_map_clip_end(map, entry, end);

		next = entry->next;
		s = entry->start;
		e = entry->end;
		offset = entry->offset;

		/*
		 * Unwire before removing addresses from the pmap; otherwise,
		 * unwiring will put the entries back in the pmap.
		 */

		object = entry->object.vm_object;
		if (entry->wired_count != 0)
			vm_map_entry_unwire(map, entry);

		/*
		 * If this is a sharing map, we must remove *all* references
		 * to this data, since we can't find all of the physical maps
		 * which are sharing it.
		 */

		if (object == kernel_object || object == kmem_object) {
			vm_object_page_remove(object, OFF_TO_IDX(offset),
			    OFF_TO_IDX(offset + (e - s)), FALSE);
		} else if (!map->is_main_map) {
			vm_object_pmap_remove(object,
			    OFF_TO_IDX(offset),
			    OFF_TO_IDX(offset + (e - s)));
		} else {
			pmap_remove(map->pmap, s, e);
		}

		/*
		 * Delete the entry (which may delete the object) only after
		 * removing all pmap entries pointing to its pages.
		 * (Otherwise, its page frames may be reallocated, and any
		 * modify bits will be set in the wrong object!)
		 */

		vm_map_entry_delete(map, entry);
		entry = next;
	}
	return (KERN_SUCCESS);
}

/*
 *	vm_map_remove:
 *
 *	Remove the given address range from the target map.
 *	This is the exported form of vm_map_delete.
 */
int
vm_map_remove(map, start, end)
	register vm_map_t map;
	register vm_offset_t start;
	register vm_offset_t end;
{
	register int result, s = 0;

	if (map == kmem_map || map == mb_map)
		s = splvm();

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	result = vm_map_delete(map, start, end);
	vm_map_unlock(map);

	if (map == kmem_map || map == mb_map)
		splx(s);

	return (result);
}

/*
 *	vm_map_check_protection:
 *
 *	Assert that the target map allows the specified
 *	privilege on the entire address region given.
 *	The entire region must be allocated.
 */
boolean_t
vm_map_check_protection(map, start, end, protection)
	register vm_map_t map;
	register vm_offset_t start;
	register vm_offset_t end;
	register vm_prot_t protection;
{
	register vm_map_entry_t entry;
	vm_map_entry_t tmp_entry;

	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		return (FALSE);
	}
	entry = tmp_entry;

	while (start < end) {
		if (entry == &map->header) {
			return (FALSE);
		}
		/*
		 * No holes allowed!
		 */

		if (start < entry->start) {
			return (FALSE);
		}
		/*
		 * Check protection associated with entry.
		 */

		if ((entry->protection & protection) != protection) {
			return (FALSE);
		}
		/* go to next entry */

		start = entry->end;
		entry = entry->next;
	}
	return (TRUE);
}

/*
 *	vm_map_copy_entry:
 *
 *	Copies the contents of the source entry to the destination
 *	entry.  The entries *must* be aligned properly.
 */
static void
vm_map_copy_entry(src_map, dst_map, src_entry, dst_entry)
	vm_map_t src_map, dst_map;
	register vm_map_entry_t src_entry, dst_entry;
{
	if ((dst_entry->eflags|src_entry->eflags) &
		(MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP))
		return;

	if (src_entry->wired_count == 0) {

		/*
		 * If the source entry is marked needs_copy, it is already
		 * write-protected.
		 */
		if ((src_entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0) {

			boolean_t su;

			/*
			 * If the source entry has only one mapping, we can
			 * just protect the virtual address range.
			 */
			if (!(su = src_map->is_main_map)) {
				su = (src_map->ref_count == 1);
			}
			if (su) {
				pmap_protect(src_map->pmap,
				    src_entry->start,
				    src_entry->end,
				    src_entry->protection & ~VM_PROT_WRITE);
			} else {
				vm_object_pmap_copy(src_entry->object.vm_object,
				    OFF_TO_IDX(src_entry->offset),
				    OFF_TO_IDX(src_entry->offset + (src_entry->end
					- src_entry->start)));
			}
		}

		/*
		 * Make a copy of the object.
		 */
		if (src_entry->object.vm_object) {
			if ((src_entry->object.vm_object->handle == NULL) &&
				(src_entry->object.vm_object->type == OBJT_DEFAULT ||
				 src_entry->object.vm_object->type == OBJT_SWAP))
				vm_object_collapse(src_entry->object.vm_object);
			++src_entry->object.vm_object->ref_count;
			src_entry->eflags |= (MAP_ENTRY_COW|MAP_ENTRY_NEEDS_COPY);
			dst_entry->eflags |= (MAP_ENTRY_COW|MAP_ENTRY_NEEDS_COPY);
			dst_entry->object.vm_object =
				src_entry->object.vm_object;
			dst_entry->offset = src_entry->offset;
		} else {
			dst_entry->object.vm_object = NULL;
			dst_entry->offset = 0;
		}

		pmap_copy(dst_map->pmap, src_map->pmap, dst_entry->start,
		    dst_entry->end - dst_entry->start, src_entry->start);
	} else {
		/*
		 * Of course, wired down pages can't be set copy-on-write.
		 * Cause wired pages to be copied into the new map by
		 * simulating faults (the new pages are pageable)
		 */
		vm_fault_copy_entry(dst_map, src_map, dst_entry, src_entry);
	}
}

/*
 * vmspace_fork:
 * Create a new process vmspace structure and vm_map
 * based on those of an existing process.  The new map
 * is based on the old map, according to the inheritance
 * values on the regions in that map.
 *
 * The source map must not be locked.
 */
struct vmspace *
vmspace_fork(vm1)
	register struct vmspace *vm1;
{
	register struct vmspace *vm2;
	vm_map_t old_map = &vm1->vm_map;
	vm_map_t new_map;
	vm_map_entry_t old_entry;
	vm_map_entry_t new_entry;
	pmap_t new_pmap;
	vm_object_t object;

	vm_map_lock(old_map);

	vm2 = vmspace_alloc(old_map->min_offset, old_map->max_offset,
	    old_map->entries_pageable);
	bcopy(&vm1->vm_startcopy, &vm2->vm_startcopy,
	    (caddr_t) (vm1 + 1) - (caddr_t) &vm1->vm_startcopy);
	new_pmap = &vm2->vm_pmap;	/* XXX */
	new_map = &vm2->vm_map;	/* XXX */

	old_entry = old_map->header.next;

	while (old_entry != &old_map->header) {
		if (old_entry->eflags & MAP_ENTRY_IS_SUB_MAP)
			panic("vm_map_fork: encountered a submap");

		switch (old_entry->inheritance) {
		case VM_INHERIT_NONE:
			break;

		case VM_INHERIT_SHARE:
			/*
			 * Clone the entry, creating the shared object if necessary.
			 */
			object = old_entry->object.vm_object;
			if (object == NULL) {
				object = vm_object_allocate(OBJT_DEFAULT,
					OFF_TO_IDX(old_entry->end - old_entry->start));
				old_entry->object.vm_object = object;
				old_entry->offset = (vm_offset_t) 0;
			} else if (old_entry->eflags & MAP_ENTRY_NEEDS_COPY) {
				vm_object_shadow(&old_entry->object.vm_object,
					&old_entry->offset,
					OFF_TO_IDX(old_entry->end - old_entry->start));
				old_entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
				object = old_entry->object.vm_object;
			}

			/*
			 * Clone the entry, referencing the sharing map.
			 */
			new_entry = vm_map_entry_create(new_map);
			*new_entry = *old_entry;
			new_entry->wired_count = 0;
			++object->ref_count;

			/*
			 * Insert the entry into the new map -- we know we're
			 * inserting at the end of the new map.
			 */

			vm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);

			/*
			 * Update the physical map
			 */

			pmap_copy(new_map->pmap, old_map->pmap,
			    new_entry->start,
			    (old_entry->end - old_entry->start),
			    old_entry->start);
			break;

		case VM_INHERIT_COPY:
			/*
			 * Clone the entry and link into the map.
			 */
			new_entry = vm_map_entry_create(new_map);
			*new_entry = *old_entry;
			new_entry->wired_count = 0;
			new_entry->object.vm_object = NULL;
			new_entry->eflags &= ~MAP_ENTRY_IS_A_MAP;
			vm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);
			vm_map_copy_entry(old_map, new_map, old_entry,
			    new_entry);
			break;
		}
		old_entry = old_entry->next;
	}

	new_map->size = old_map->size;
	vm_map_unlock(old_map);

	return (vm2);
}

/*
 * Unshare the specified VM space for exec.  If other processes are
 * mapped to it, then create a new one.  The new vmspace is null.
 */

void
vmspace_exec(struct proc *p) {
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;
	vm_map_t map = &p->p_vmspace->vm_map;

	newvmspace = vmspace_alloc(map->min_offset, map->max_offset,
	    map->entries_pageable);
	bcopy(&oldvmspace->vm_startcopy, &newvmspace->vm_startcopy,
	    (caddr_t) (newvmspace + 1) - (caddr_t) &newvmspace->vm_startcopy);
	/*
	 * This code is written like this for prototype purposes.  The
	 * goal is to avoid running down the vmspace here, but let the
	 * other process's that are still using the vmspace to finally
	 * run it down.  Even though there is little or no chance of blocking
	 * here, it is a good idea to keep this form for future mods.
	 */
	vm_map_reference(&oldvmspace->vm_map);
	vmspace_free(oldvmspace);
	p->p_vmspace = newvmspace;
	if (p == curproc)
		pmap_activate(p);
	vm_map_deallocate(&oldvmspace->vm_map);
}

/*
 * Unshare the specified VM space for forcing COW.  This
 * is called by rfork, for the (RFMEM|RFPROC) == 0 case.
 */

void
vmspace_unshare(struct proc *p) {
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;

	if (oldvmspace->vm_refcnt == 1)
		return;
	newvmspace = vmspace_fork(oldvmspace);
	vm_map_reference(&oldvmspace->vm_map);
	vmspace_free(oldvmspace);
	p->p_vmspace = newvmspace;
	if (p == curproc)
		pmap_activate(p);
	vm_map_deallocate(&oldvmspace->vm_map);
}
	

/*
 *	vm_map_lookup:
 *
 *	Finds the VM object, offset, and
 *	protection for a given virtual address in the
 *	specified map, assuming a page fault of the
 *	type specified.
 *
 *	Leaves the map in question locked for read; return
 *	values are guaranteed until a vm_map_lookup_done
 *	call is performed.  Note that the map argument
 *	is in/out; the returned map must be used in
 *	the call to vm_map_lookup_done.
 *
 *	A handle (out_entry) is returned for use in
 *	vm_map_lookup_done, to make that fast.
 *
 *	If a lookup is requested with "write protection"
 *	specified, the map may be changed to perform virtual
 *	copying operations, although the data referenced will
 *	remain the same.
 */
int
vm_map_lookup(var_map, vaddr, fault_type, out_entry,
    object, pindex, out_prot, wired, single_use)
	vm_map_t *var_map;	/* IN/OUT */
	register vm_offset_t vaddr;
	register vm_prot_t fault_type;

	vm_map_entry_t *out_entry;	/* OUT */
	vm_object_t *object;	/* OUT */
	vm_pindex_t *pindex;	/* OUT */
	vm_prot_t *out_prot;	/* OUT */
	boolean_t *wired;	/* OUT */
	boolean_t *single_use;	/* OUT */
{
	vm_map_t share_map;
	vm_offset_t share_offset;
	register vm_map_entry_t entry;
	register vm_map_t map = *var_map;
	register vm_prot_t prot;
	register boolean_t su;

RetryLookup:;

	/*
	 * Lookup the faulting address.
	 */

	vm_map_lock_read(map);

#define	RETURN(why) \
		{ \
		vm_map_unlock_read(map); \
		return(why); \
		}

	/*
	 * If the map has an interesting hint, try it before calling full
	 * blown lookup routine.
	 */

	entry = map->hint;

	*out_entry = entry;

	if ((entry == &map->header) ||
	    (vaddr < entry->start) || (vaddr >= entry->end)) {
		vm_map_entry_t tmp_entry;

		/*
		 * Entry was either not a valid hint, or the vaddr was not
		 * contained in the entry, so do a full lookup.
		 */
		if (!vm_map_lookup_entry(map, vaddr, &tmp_entry))
			RETURN(KERN_INVALID_ADDRESS);

		entry = tmp_entry;
		*out_entry = entry;
	}
	
	/*
	 * Handle submaps.
	 */

	if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
		vm_map_t old_map = map;

		*var_map = map = entry->object.sub_map;
		vm_map_unlock_read(old_map);
		goto RetryLookup;
	}

	/*
	 * Check whether this task is allowed to have this page.
	 * Note the special case for MAP_ENTRY_COW
	 * pages with an override.  This is to implement a forced
	 * COW for debuggers.
	 */

	prot = entry->protection;
	if ((fault_type & VM_PROT_OVERRIDE_WRITE) == 0 ||
		(entry->eflags & MAP_ENTRY_COW) == 0 ||
		(entry->wired_count != 0)) {
		if ((fault_type & (prot)) != fault_type)
			RETURN(KERN_PROTECTION_FAILURE);
	}

	/*
	 * If this page is not pageable, we have to get it for all possible
	 * accesses.
	 */

	*wired = (entry->wired_count != 0);
	if (*wired)
		prot = fault_type = entry->protection;

	/*
	 * If we don't already have a VM object, track it down.
	 */

	su = (entry->eflags & MAP_ENTRY_IS_A_MAP) == 0;
	if (su) {
		share_map = map;
		share_offset = vaddr;
	} else {
		vm_map_entry_t share_entry;

		/*
		 * Compute the sharing map, and offset into it.
		 */

		share_map = entry->object.share_map;
		share_offset = (vaddr - entry->start) + entry->offset;

		/*
		 * Look for the backing store object and offset
		 */

		vm_map_lock_read(share_map);

		if (!vm_map_lookup_entry(share_map, share_offset,
			&share_entry)) {
			vm_map_unlock_read(share_map);
			RETURN(KERN_INVALID_ADDRESS);
		}
		entry = share_entry;
	}

	/*
	 * If the entry was copy-on-write, we either ...
	 */

	if (entry->eflags & MAP_ENTRY_NEEDS_COPY) {
		/*
		 * If we want to write the page, we may as well handle that
		 * now since we've got the sharing map locked.
		 *
		 * If we don't need to write the page, we just demote the
		 * permissions allowed.
		 */

		if (fault_type & VM_PROT_WRITE) {
			/*
			 * Make a new object, and place it in the object
			 * chain.  Note that no new references have appeared
			 * -- one just moved from the share map to the new
			 * object.
			 */

			if (lockmgr(&share_map->lock, LK_EXCLUPGRADE,
					(void *)0, curproc)) {

				if (share_map != map)
					vm_map_unlock_read(map);

				goto RetryLookup;
			}
			vm_object_shadow(
			    &entry->object.vm_object,
			    &entry->offset,
			    OFF_TO_IDX(entry->end - entry->start));

			entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;

			lockmgr(&share_map->lock, LK_DOWNGRADE,
				(void *)0, curproc);
		} else {
			/*
			 * We're attempting to read a copy-on-write page --
			 * don't allow writes.
			 */

			prot &= (~VM_PROT_WRITE);
		}
	}
	/*
	 * Create an object if necessary.
	 */
	if (entry->object.vm_object == NULL) {

		if (lockmgr(&share_map->lock, LK_EXCLUPGRADE,
				(void *)0, curproc)) {
			if (share_map != map)
				vm_map_unlock_read(map);
			goto RetryLookup;
		}
		entry->object.vm_object = vm_object_allocate(OBJT_DEFAULT,
		    OFF_TO_IDX(entry->end - entry->start));
		entry->offset = 0;
		lockmgr(&share_map->lock, LK_DOWNGRADE, (void *)0, curproc);
	}

	if (entry->object.vm_object != NULL)
		default_pager_convert_to_swapq(entry->object.vm_object);
	/*
	 * Return the object/offset from this entry.  If the entry was
	 * copy-on-write or empty, it has been fixed up.
	 */

	*pindex = OFF_TO_IDX((share_offset - entry->start) + entry->offset);
	*object = entry->object.vm_object;

	/*
	 * Return whether this is the only map sharing this data.
	 */

	if (!su) {
		su = (share_map->ref_count == 1);
	}
	*out_prot = prot;
	*single_use = su;

	return (KERN_SUCCESS);

#undef	RETURN
}

/*
 *	vm_map_lookup_done:
 *
 *	Releases locks acquired by a vm_map_lookup
 *	(according to the handle returned by that lookup).
 */

void
vm_map_lookup_done(map, entry)
	register vm_map_t map;
	vm_map_entry_t entry;
{
	/*
	 * If this entry references a map, unlock it first.
	 */

	if (entry->eflags & MAP_ENTRY_IS_A_MAP)
		vm_map_unlock_read(entry->object.share_map);

	/*
	 * Unlock the main-level map
	 */

	vm_map_unlock_read(map);
}

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

/*
 *	vm_map_print:	[ debug ]
 */
DB_SHOW_COMMAND(map, vm_map_print)
{
	/* XXX convert args. */
	register vm_map_t map = (vm_map_t)addr;
	boolean_t full = have_addr;

	register vm_map_entry_t entry;

	db_iprintf("%s map 0x%x: pmap=0x%x,ref=%d,nentries=%d,version=%d\n",
	    (map->is_main_map ? "Task" : "Share"),
	    (int) map, (int) (map->pmap), map->ref_count, map->nentries,
	    map->timestamp);

	if (!full && db_indent)
		return;

	db_indent += 2;
	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {
		db_iprintf("map entry 0x%x: start=0x%x, end=0x%x, ",
		    (int) entry, (int) entry->start, (int) entry->end);
		if (map->is_main_map) {
			static char *inheritance_name[4] =
			{"share", "copy", "none", "donate_copy"};

			db_printf("prot=%x/%x/%s, ",
			    entry->protection,
			    entry->max_protection,
			    inheritance_name[entry->inheritance]);
			if (entry->wired_count != 0)
				db_printf("wired, ");
		}
		if (entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP)) {
			db_printf("share=0x%x, offset=0x%x\n",
			    (int) entry->object.share_map,
			    (int) entry->offset);
			if ((entry->prev == &map->header) ||
			    ((entry->prev->eflags & MAP_ENTRY_IS_A_MAP) == 0) ||
			    (entry->prev->object.share_map !=
				entry->object.share_map)) {
				db_indent += 2;
				vm_map_print((int)entry->object.share_map,
					     full, 0, (char *)0);
				db_indent -= 2;
			}
		} else {
			db_printf("object=0x%x, offset=0x%x",
			    (int) entry->object.vm_object,
			    (int) entry->offset);
			if (entry->eflags & MAP_ENTRY_COW)
				db_printf(", copy (%s)",
				    (entry->eflags & MAP_ENTRY_NEEDS_COPY) ? "needed" : "done");
			db_printf("\n");

			if ((entry->prev == &map->header) ||
			    (entry->prev->eflags & MAP_ENTRY_IS_A_MAP) ||
			    (entry->prev->object.vm_object !=
				entry->object.vm_object)) {
				db_indent += 2;
				vm_object_print((int)entry->object.vm_object,
						full, 0, (char *)0);
				db_indent -= 2;
			}
		}
	}
	db_indent -= 2;
}
#endif /* DDB */
