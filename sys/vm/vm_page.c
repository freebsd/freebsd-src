/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vm_page.c	7.4 (Berkeley) 5/7/91
 *	$Id: vm_page.c,v 1.2 1993/10/16 16:20:44 rgrimes Exp $
 */

/*
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
 */

/*
 *	Resident memory management module.
 */

#include "param.h"

#include "vm.h"
#include "vm_map.h"
#include "vm_page.h"
#include "vm_pageout.h"

/*
 *	Associated with page of user-allocatable memory is a
 *	page structure.
 */

queue_head_t	*vm_page_buckets;		/* Array of buckets */
int		vm_page_bucket_count = 0;	/* How big is array? */
int		vm_page_hash_mask;		/* Mask for hash function */
simple_lock_data_t	bucket_lock;		/* lock for all buckets XXX */

vm_size_t	page_size  = 4096;
vm_size_t	page_mask  = 4095;
int		page_shift = 12;

queue_head_t	vm_page_queue_free;
queue_head_t	vm_page_queue_active;
queue_head_t	vm_page_queue_inactive;
simple_lock_data_t	vm_page_queue_lock;
simple_lock_data_t	vm_page_queue_free_lock;

vm_page_t	vm_page_array;
long		first_page;
long		last_page;
vm_offset_t	first_phys_addr;
vm_offset_t	last_phys_addr;

int	vm_page_free_count;
int	vm_page_active_count;
int	vm_page_inactive_count;
int	vm_page_wire_count;
int	vm_page_laundry_count;

int	vm_page_free_target = 0;
int	vm_page_free_min = 0;
int	vm_page_inactive_target = 0;
int	vm_page_free_reserved = 0;

/*
 *	vm_set_page_size:
 *
 *	Sets the page size, perhaps based upon the memory
 *	size.  Must be called before any use of page-size
 *	dependent functions.
 *
 *	Sets page_shift and page_mask from page_size.
 */
void vm_set_page_size()
{
	page_mask = page_size - 1;

	if ((page_mask & page_size) != 0)
		panic("vm_set_page_size: page size not a power of two");

	for (page_shift = 0; ; page_shift++)
		if ((1 << page_shift) == page_size)
			break;
}


/*
 *	vm_page_startup:
 *
 *	Initializes the resident memory module.
 *
 *	Allocates memory for the page cells, and
 *	for the object/offset-to-page hash table headers.
 *	Each page cell is initialized and placed on the free list.
 */
vm_offset_t vm_page_startup(start, end, vaddr)
	register vm_offset_t	start;
	vm_offset_t	end;
	register vm_offset_t	vaddr;
{
	register vm_offset_t	mapped;
	register vm_page_t	m;
	register queue_t	bucket;
	vm_size_t		npages;
	register vm_offset_t	new_start;
	int			i;
	vm_offset_t		pa;

	extern	vm_offset_t	kentry_data;
	extern	vm_size_t	kentry_data_size;


	/*
	 *	Initialize the locks
	 */

	simple_lock_init(&vm_page_queue_free_lock);
	simple_lock_init(&vm_page_queue_lock);

	/*
	 *	Initialize the queue headers for the free queue,
	 *	the active queue and the inactive queue.
	 */

	queue_init(&vm_page_queue_free);
	queue_init(&vm_page_queue_active);
	queue_init(&vm_page_queue_inactive);

	/*
	 *	Allocate (and initialize) the hash table buckets.
	 *
	 *	The number of buckets MUST BE a power of 2, and
	 *	the actual value is the next power of 2 greater
	 *	than the number of physical pages in the system.
	 *
	 *	Note:
	 *		This computation can be tweaked if desired.
	 */

	vm_page_buckets = (queue_t) vaddr;
	bucket = vm_page_buckets;
	if (vm_page_bucket_count == 0) {
		vm_page_bucket_count = 1;
		while (vm_page_bucket_count < atop(end - start))
			vm_page_bucket_count <<= 1;
	}

	vm_page_hash_mask = vm_page_bucket_count - 1;

	/*
	 *	Validate these addresses.
	 */

	new_start = round_page(((queue_t)start) + vm_page_bucket_count);
	mapped = vaddr;
	vaddr = pmap_map(mapped, start, new_start,
			VM_PROT_READ|VM_PROT_WRITE);
	start = new_start;
	bzero((caddr_t) mapped, vaddr - mapped);
	mapped = vaddr;

	for (i = vm_page_bucket_count; i--;) {
		queue_init(bucket);
		bucket++;
	}

	simple_lock_init(&bucket_lock);

	/*
	 *	round (or truncate) the addresses to our page size.
	 */

	end = trunc_page(end);

	/*
	 *	Pre-allocate maps and map entries that cannot be dynamically
	 *	allocated via malloc().  The maps include the kernel_map and
	 *	kmem_map which must be initialized before malloc() will
	 *	work (obviously).  Also could include pager maps which would
	 *	be allocated before kmeminit.
	 *
	 *	Allow some kernel map entries... this should be plenty
	 *	since people shouldn't be cluttering up the kernel
	 *	map (they should use their own maps).
	 */

	kentry_data_size = MAX_KMAP * sizeof(struct vm_map) +
			   MAX_KMAPENT * sizeof(struct vm_map_entry);
	kentry_data_size = round_page(kentry_data_size);
	kentry_data = (vm_offset_t) vaddr;
	vaddr += kentry_data_size;

	/*
	 *	Validate these zone addresses.
	 */

	new_start = start + (vaddr - mapped);
	pmap_map(mapped, start, new_start, VM_PROT_READ|VM_PROT_WRITE);
	bzero((caddr_t) mapped, (vaddr - mapped));
	mapped = vaddr;
	start = new_start;

	/*
 	 *	Compute the number of pages of memory that will be
	 *	available for use (taking into account the overhead
	 *	of a page structure per page).
	 */

	vm_page_free_count = npages =
		(end - start + sizeof(struct vm_page))/(PAGE_SIZE + sizeof(struct vm_page));

	/*
	 *	Initialize the mem entry structures now, and
	 *	put them in the free queue.
	 */

	m = vm_page_array = (vm_page_t) vaddr;
	first_page = start;
	first_page += npages*sizeof(struct vm_page);
	first_page = atop(round_page(first_page));
	last_page  = first_page + npages - 1;

	first_phys_addr = ptoa(first_page);
	last_phys_addr  = ptoa(last_page) + page_mask;

	/*
	 *	Validate these addresses.
	 */

	new_start = start + (round_page(m + npages) - mapped);
	mapped = pmap_map(mapped, start, new_start,
			VM_PROT_READ|VM_PROT_WRITE);
	start = new_start;

	/*
	 *	Clear all of the page structures
	 */
	bzero((caddr_t)m, npages * sizeof(*m));

	pa = first_phys_addr;
	while (npages--) {
		m->copy_on_write = FALSE;
		m->wanted = FALSE;
		m->inactive = FALSE;
		m->active = FALSE;
		m->busy = FALSE;
		m->object = NULL;
		m->phys_addr = pa;
		queue_enter(&vm_page_queue_free, m, vm_page_t, pageq);
		m++;
		pa += PAGE_SIZE;
	}

	/*
	 *	Initialize vm_pages_needed lock here - don't wait for pageout
	 *	daemon	XXX
	 */
	simple_lock_init(&vm_pages_needed_lock);

	return(mapped);
}

/*
 *	vm_page_hash:
 *
 *	Distributes the object/offset key pair among hash buckets.
 *
 *	NOTE:  This macro depends on vm_page_bucket_count being a power of 2.
 */
#define vm_page_hash(object, offset) \
	(((unsigned)object+(unsigned)atop(offset))&vm_page_hash_mask)

/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object/object-page
 *	table and object list.
 *
 *	The object and page must be locked.
 */

void vm_page_insert(mem, object, offset)
	register vm_page_t	mem;
	register vm_object_t	object;
	register vm_offset_t	offset;
{
	register queue_t	bucket;
	int			spl;

	VM_PAGE_CHECK(mem);

	if (mem->tabled)
		panic("vm_page_insert: already inserted");

	/*
	 *	Record the object/offset pair in this page
	 */

	mem->object = object;
	mem->offset = offset;

	/*
	 *	Insert it into the object_object/offset hash table
	 */

	bucket = &vm_page_buckets[vm_page_hash(object, offset)];
	spl = splimp();
	simple_lock(&bucket_lock);
	queue_enter(bucket, mem, vm_page_t, hashq);
	simple_unlock(&bucket_lock);
	(void) splx(spl);

	/*
	 *	Now link into the object's list of backed pages.
	 */

	queue_enter(&object->memq, mem, vm_page_t, listq);
	mem->tabled = TRUE;

	/*
	 *	And show that the object has one more resident
	 *	page.
	 */

	object->resident_page_count++;
}

/*
 *	vm_page_remove:		[ internal use only ]
 *
 *	Removes the given mem entry from the object/offset-page
 *	table and the object page list.
 *
 *	The object and page must be locked.
 */

void vm_page_remove(mem)
	register vm_page_t	mem;
{
	register queue_t	bucket;
	int			spl;

	VM_PAGE_CHECK(mem);

	if (!mem->tabled)
		return;

	/*
	 *	Remove from the object_object/offset hash table
	 */

	bucket = &vm_page_buckets[vm_page_hash(mem->object, mem->offset)];
	spl = splimp();
	simple_lock(&bucket_lock);
	queue_remove(bucket, mem, vm_page_t, hashq);
	simple_unlock(&bucket_lock);
	(void) splx(spl);

	/*
	 *	Now remove from the object's list of backed pages.
	 */

	queue_remove(&mem->object->memq, mem, vm_page_t, listq);

	/*
	 *	And show that the object has one fewer resident
	 *	page.
	 */

	mem->object->resident_page_count--;

	mem->tabled = FALSE;
}

/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.  No side effects.
 */

vm_page_t vm_page_lookup(object, offset)
	register vm_object_t	object;
	register vm_offset_t	offset;
{
	register vm_page_t	mem;
	register queue_t	bucket;
	int			spl;

	/*
	 *	Search the hash table for this object/offset pair
	 */

	bucket = &vm_page_buckets[vm_page_hash(object, offset)];

	spl = splimp();
	simple_lock(&bucket_lock);
	mem = (vm_page_t) queue_first(bucket);
	while (!queue_end(bucket, (queue_entry_t) mem)) {
		VM_PAGE_CHECK(mem);
		if ((mem->object == object) && (mem->offset == offset)) {
			simple_unlock(&bucket_lock);
			splx(spl);
			return(mem);
		}
		mem = (vm_page_t) queue_next(&mem->hashq);
	}

	simple_unlock(&bucket_lock);
	splx(spl);
	return(NULL);
}

/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	The object must be locked.
 */
void vm_page_rename(mem, new_object, new_offset)
	register vm_page_t	mem;
	register vm_object_t	new_object;
	vm_offset_t		new_offset;
{
	if (mem->object == new_object)
		return;

	vm_page_lock_queues();	/* keep page from moving out from
				   under pageout daemon */
    	vm_page_remove(mem);
	vm_page_insert(mem, new_object, new_offset);
	vm_page_unlock_queues();
}

void		vm_page_init(mem, object, offset)
	vm_page_t	mem;
	vm_object_t	object;
	vm_offset_t	offset;
{
#ifdef DEBUG
#define	vm_page_init(mem, object, offset)  {\
		(mem)->busy = TRUE; \
		(mem)->tabled = FALSE; \
		vm_page_insert((mem), (object), (offset)); \
		(mem)->absent = FALSE; \
		(mem)->fictitious = FALSE; \
		(mem)->page_lock = VM_PROT_NONE; \
		(mem)->unlock_request = VM_PROT_NONE; \
		(mem)->laundry = FALSE; \
		(mem)->active = FALSE; \
		(mem)->inactive = FALSE; \
		(mem)->wire_count = 0; \
		(mem)->clean = TRUE; \
		(mem)->copy_on_write = FALSE; \
		(mem)->fake = TRUE; \
		(mem)->pagerowned = FALSE; \
		(mem)->ptpage = FALSE; \
	}
#else
#define	vm_page_init(mem, object, offset)  {\
		(mem)->busy = TRUE; \
		(mem)->tabled = FALSE; \
		vm_page_insert((mem), (object), (offset)); \
		(mem)->absent = FALSE; \
		(mem)->fictitious = FALSE; \
		(mem)->page_lock = VM_PROT_NONE; \
		(mem)->unlock_request = VM_PROT_NONE; \
		(mem)->laundry = FALSE; \
		(mem)->active = FALSE; \
		(mem)->inactive = FALSE; \
		(mem)->wire_count = 0; \
		(mem)->clean = TRUE; \
		(mem)->copy_on_write = FALSE; \
		(mem)->fake = TRUE; \
	}
#endif

	vm_page_init(mem, object, offset);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a memory cell associated
 *	with this VM object/offset pair.
 *
 *	Object must be locked.
 */
vm_page_t vm_page_alloc(object, offset)
	vm_object_t	object;
	vm_offset_t	offset;
{
	register vm_page_t	mem;
	int		spl;

	spl = splimp();				/* XXX */
	simple_lock(&vm_page_queue_free_lock);
	if (	object != kernel_object &&
		object != kmem_object	&&
		vm_page_free_count <= vm_page_free_reserved) {

		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
		return(NULL);
	}
	if (queue_empty(&vm_page_queue_free)) {
		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
		return(NULL);
	}

	queue_remove_first(&vm_page_queue_free, mem, vm_page_t, pageq);

	vm_page_free_count--;
	simple_unlock(&vm_page_queue_free_lock);
	splx(spl);

	vm_page_init(mem, object, offset);

	/*
	 *	Decide if we should poke the pageout daemon.
	 *	We do this if the free count is less than the low
	 *	water mark, or if the free count is less than the high
	 *	water mark (but above the low water mark) and the inactive
	 *	count is less than its target.
	 *
	 *	We don't have the counts locked ... if they change a little,
	 *	it doesn't really matter.
	 */

	if ((vm_page_free_count < vm_page_free_min) ||
			((vm_page_free_count < vm_page_free_target) &&
			(vm_page_inactive_count < vm_page_inactive_target)))
		thread_wakeup(&vm_pages_needed);
	return(mem);
}

/*
 *	vm_page_free:
 *
 *	Returns the given page to the free list,
 *	disassociating it with any VM object.
 *
 *	Object and page must be locked prior to entry.
 */
void vm_page_free(mem)
	register vm_page_t	mem;
{
	vm_page_remove(mem);
	if (mem->active) {
		queue_remove(&vm_page_queue_active, mem, vm_page_t, pageq);
		mem->active = FALSE;
		vm_page_active_count--;
	}

	if (mem->inactive) {
		queue_remove(&vm_page_queue_inactive, mem, vm_page_t, pageq);
		mem->inactive = FALSE;
		vm_page_inactive_count--;
	}

	if (!mem->fictitious) {
		int	spl;

		spl = splimp();
		simple_lock(&vm_page_queue_free_lock);
		queue_enter(&vm_page_queue_free, mem, vm_page_t, pageq);

		vm_page_free_count++;
		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
	}
}

/*
 *	vm_page_wire:
 *
 *	Mark this page as wired down by yet
 *	another map, removing it from paging queues
 *	as necessary.
 *
 *	The page queues must be locked.
 */
void vm_page_wire(mem)
	register vm_page_t	mem;
{
	VM_PAGE_CHECK(mem);

	if (mem->wire_count == 0) {
		if (mem->active) {
			queue_remove(&vm_page_queue_active, mem, vm_page_t,
						pageq);
			vm_page_active_count--;
			mem->active = FALSE;
		}
		if (mem->inactive) {
			queue_remove(&vm_page_queue_inactive, mem, vm_page_t,
						pageq);
			vm_page_inactive_count--;
			mem->inactive = FALSE;
		}
		vm_page_wire_count++;
	}
	mem->wire_count++;
}

/*
 *	vm_page_unwire:
 *
 *	Release one wiring of this page, potentially
 *	enabling it to be paged again.
 *
 *	The page queues must be locked.
 */
void vm_page_unwire(mem)
	register vm_page_t	mem;
{
	VM_PAGE_CHECK(mem);

	mem->wire_count--;
	if (mem->wire_count == 0) {
		queue_enter(&vm_page_queue_active, mem, vm_page_t, pageq);
		vm_page_active_count++;
		mem->active = TRUE;
		vm_page_wire_count--;
	}
}

/*
 *	vm_page_deactivate:
 *
 *	Returns the given page to the inactive list,
 *	indicating that no physical maps have access
 *	to this page.  [Used by the physical mapping system.]
 *
 *	The page queues must be locked.
 */
void vm_page_deactivate(m)
	register vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	/*
	 *	Only move active pages -- ignore locked or already
	 *	inactive ones.
	 *
	 *	XXX: sometimes we get pages which aren't wired down
	 *	or on any queue - we need to put them on the inactive
	 *	queue also, otherwise we lose track of them.
	 *	Paul Mackerras (paulus@cs.anu.edu.au) 9-Jan-93.
	 */

	if (!m->inactive && m->wire_count == 0) {
		pmap_clear_reference(VM_PAGE_TO_PHYS(m));
		if (m->active) {
			queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
			m->active = FALSE;
			vm_page_active_count--;
		}
		queue_enter(&vm_page_queue_inactive, m, vm_page_t, pageq);
		m->inactive = TRUE;
		vm_page_inactive_count++;
		if (pmap_is_modified(VM_PAGE_TO_PHYS(m)))
			m->clean = FALSE;
		m->laundry = !m->clean;
	}
}

/*
 *	vm_page_activate:
 *
 *	Put the specified page on the active list (if appropriate).
 *
 *	The page queues must be locked.
 */

void vm_page_activate(m)
	register vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	if (m->inactive) {
		queue_remove(&vm_page_queue_inactive, m, vm_page_t,
						pageq);
		vm_page_inactive_count--;
		m->inactive = FALSE;
	}
	if (m->wire_count == 0) {
		if (m->active)
			panic("vm_page_activate: already active");

		queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
		m->active = TRUE;
		vm_page_active_count++;
	}
}

/*
 *	vm_page_zero_fill:
 *
 *	Zero-fill the specified page.
 *	Written as a standard pagein routine, to
 *	be used by the zero-fill object.
 */

boolean_t vm_page_zero_fill(m)
	vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	pmap_zero_page(VM_PAGE_TO_PHYS(m));
	return(TRUE);
}

/*
 *	vm_page_copy:
 *
 *	Copy one page to another
 */

void vm_page_copy(src_m, dest_m)
	vm_page_t	src_m;
	vm_page_t	dest_m;
{
	VM_PAGE_CHECK(src_m);
	VM_PAGE_CHECK(dest_m);

	pmap_copy_page(VM_PAGE_TO_PHYS(src_m), VM_PAGE_TO_PHYS(dest_m));
}
