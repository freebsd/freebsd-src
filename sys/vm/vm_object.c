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
 *	from: @(#)vm_object.c	7.4 (Berkeley) 5/7/91
 *	$Id: vm_object.c,v 1.25 1994/04/14 07:50:21 davidg Exp $
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
 */

/*
 *	Virtual memory object module.
 */

#include "ddb.h"
#include "param.h"
#include "malloc.h"
#include "systm.h"

#include "vm.h"
#include "vm_page.h"
#include "proc.h"


static void _vm_object_allocate(vm_size_t, vm_object_t);
void vm_object_deactivate_pages(vm_object_t);
static void vm_object_cache_trim(void);
static void vm_object_remove(vm_pager_t);

/*
 *	Virtual memory objects maintain the actual data
 *	associated with allocated virtual memory.  A given
 *	page of memory exists within exactly one object.
 *
 *	An object is only deallocated when all "references"
 *	are given up.  Only one "reference" to a given
 *	region of an object should be writeable.
 *
 *	Associated with each object is a list of all resident
 *	memory pages belonging to that object; this list is
 *	maintained by the "vm_page" module, and locked by the object's
 *	lock.
 *
 *	Each object also records a "pager" routine which is
 *	used to retrieve (and store) pages to the proper backing
 *	storage.  In addition, objects may be backed by other
 *	objects from which they were virtual-copied.
 *
 *	The only items within the object structure which are
 *	modified after time of creation are:
 *		reference count		locked by object's lock
 *		pager routine		locked by object's lock
 *
 */


queue_head_t	vm_object_cached_list;	/* list of objects persisting */
int		vm_object_cached;	/* size of cached list */
simple_lock_data_t	vm_cache_lock;	/* lock for object cache */

queue_head_t	vm_object_list;		/* list of allocated objects */
long		vm_object_count;	/* count of all objects */
simple_lock_data_t	vm_object_list_lock;
					/* lock for object list and count */

vm_object_t	kernel_object;		/* the single kernel object */
vm_object_t	kmem_object;		/* the kernel malloc object */
struct vm_object	kernel_object_store;
struct vm_object	kmem_object_store;

extern int vm_cache_max;
#define	VM_OBJECT_HASH_COUNT	127	

queue_head_t	vm_object_hashtable[VM_OBJECT_HASH_COUNT];

long	object_collapses = 0;
long	object_bypasses  = 0;

/*
 * internal version of vm_object_allocate
 */
static inline void
_vm_object_allocate(size, object)
	vm_size_t		size;
	register vm_object_t	object;
{
	queue_init(&object->memq);
	vm_object_lock_init(object);
	object->ref_count = 1;
	object->resident_page_count = 0;
	object->size = size;
	object->can_persist = FALSE;
	object->paging_in_progress = 0;
	object->copy = NULL;

	/*
	 *	Object starts out read-write, with no pager.
	 */

	object->pager = NULL;
	object->internal = TRUE;	/* vm_allocate_with_pager will reset */
	object->paging_offset = 0;
	object->shadow = NULL;
	object->shadow_offset = (vm_offset_t) 0;

	simple_lock(&vm_object_list_lock);
	queue_enter(&vm_object_list, object, vm_object_t, object_list);
	vm_object_count++;
	simple_unlock(&vm_object_list_lock);
}

/*
 *	vm_object_init:
 *
 *	Initialize the VM objects module.
 */
void
vm_object_init()
{
	register int	i;

	queue_init(&vm_object_cached_list);
	queue_init(&vm_object_list);
	vm_object_count = 0;
	simple_lock_init(&vm_cache_lock);
	simple_lock_init(&vm_object_list_lock);

	for (i = 0; i < VM_OBJECT_HASH_COUNT; i++)
		queue_init(&vm_object_hashtable[i]);

	kernel_object = &kernel_object_store;
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
			kernel_object);

	kmem_object = &kmem_object_store;
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
			kmem_object);
}

/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */

vm_object_t
vm_object_allocate(size)
	vm_size_t	size;
{
	register vm_object_t	result;
	int s;

	result = (vm_object_t)
		malloc((u_long)sizeof *result, M_VMOBJ, M_WAITOK);
		

	_vm_object_allocate(size, result);

	return(result);
}


/*
 *	vm_object_reference:
 *
 *	Gets another reference to the given object.
 */
inline void
vm_object_reference(object)
	register vm_object_t	object;
{
	if (object == NULL)
		return;

	vm_object_lock(object);
	object->ref_count++;
	vm_object_unlock(object);
}

/*
 *	vm_object_deallocate:
 *
 *	Release a reference to the specified object,
 *	gained either through a vm_object_allocate
 *	or a vm_object_reference call.  When all references
 *	are gone, storage associated with this object
 *	may be relinquished.
 *
 *	No object may be locked.
 */
void
vm_object_deallocate(object)
	vm_object_t	object;
{
	vm_object_t	temp;

	while (object != NULL) {

		/*
		 *	The cache holds a reference (uncounted) to
		 *	the object; we must lock it before removing
		 *	the object.
		 */

		vm_object_cache_lock();

		/*
		 *	Lose the reference
		 */
		vm_object_lock(object);
		if (--(object->ref_count) != 0) {

			vm_object_unlock(object);
			/*
			 *	If there are still references, then
			 *	we are done.
			 */
			vm_object_cache_unlock();
			return;
		}

		/*
		 *	See if this object can persist.  If so, enter
		 *	it in the cache, then deactivate all of its
		 *	pages.
		 */

		if (object->can_persist) {

			queue_enter(&vm_object_cached_list, object,
				vm_object_t, cached_list);
			vm_object_cached++;
			vm_object_cache_unlock();

			/* vm_object_deactivate_pages(object); */
			vm_object_unlock(object);

			vm_object_cache_trim();
			return;
		}

		/*
		 *	Make sure no one can look us up now.
		 */
		vm_object_remove(object->pager);
		vm_object_cache_unlock();
	
		temp = object->shadow;
		vm_object_terminate(object);
			/* unlocks and deallocates object */
		object = temp;
	}
}

/*
 *	vm_object_terminate actually destroys the specified object, freeing
 *	up all previously used resources.
 *
 *	The object must be locked.
 */
void
vm_object_terminate(object)
	register vm_object_t	object;
{
	register vm_page_t	p;
	vm_object_t		shadow_object;
	int s;

	/*
	 *	Detach the object from its shadow if we are the shadow's
	 *	copy.
	 */
	if ((shadow_object = object->shadow) != NULL) {
		vm_object_lock(shadow_object);
		if (shadow_object->copy == object)
			shadow_object->copy = NULL;
/*
		else if (shadow_object->copy != NULL)
			panic("vm_object_terminate: copy/shadow inconsistency");
*/
		vm_object_unlock(shadow_object);
	}

	/*
	 * optim: get rid of any pages that we can right now
	 *        so the pageout daemon can't get any more to page
	 *        out at rundown.
	 */
#if 0
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		vm_page_t next = (vm_page_t) queue_next(&p->listq);
		VM_PAGE_CHECK(p);
		vm_page_lock_queues();

		if (p->flags & PG_BUSY) {
			p = next;
			vm_page_unlock_queues();
			continue;
		}
		if (!object->internal) {
			if ((p->flags & PG_CLEAN) == 0) {
				p = next;
				vm_page_unlock_queues();
				continue;
			}

			if (pmap_is_modified(VM_PAGE_TO_PHYS(p))) {
				p->flags &= ~PG_CLEAN;
				p = next;
				vm_page_unlock_queues();
				continue;
			}
		}

		vm_page_free(p);
		vm_page_unlock_queues();
		p = next;
	}
#endif

	/*
	 *	Wait until the pageout daemon is through
	 *	with the object.
	 */

	while (object->paging_in_progress != 0) {
		vm_object_sleep(object, object, FALSE);
		vm_object_lock(object);
	}

	/*
	 *	While the paging system is locked,
	 *	pull the object's pages off the active
	 *	and inactive queues.  This keeps the
	 *	pageout daemon from playing with them
	 *	during vm_pager_deallocate.
	 *
	 *	We can't free the pages yet, because the
	 *	object's pager may have to write them out
	 *	before deallocating the paging space.
	 */

	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		VM_PAGE_CHECK(p);

		vm_page_lock_queues();
		s = splimp();
		if (p->flags & PG_ACTIVE) {
			queue_remove(&vm_page_queue_active, p, vm_page_t,
						pageq);
			p->flags &= ~PG_ACTIVE;
			vm_page_active_count--;
		}

		if (p->flags & PG_INACTIVE) {
			queue_remove(&vm_page_queue_inactive, p, vm_page_t,
						pageq);
			p->flags &= ~PG_INACTIVE;
			vm_page_inactive_count--;
		}
		splx(s);
		vm_page_unlock_queues();
		p = (vm_page_t) queue_next(&p->listq);
	}
				
	vm_object_unlock(object);

	if (object->paging_in_progress != 0)
		panic("vm_object_deallocate: pageout in progress");

	/*
	 *	Clean and free the pages, as appropriate.
	 *	All references to the object are gone,
	 *	so we don't need to lock it.
	 */

	if (!object->internal) {
		vm_object_lock(object);
		vm_object_page_clean(object, 0, 0);
		vm_object_unlock(object);
	}

	while (!queue_empty(&object->memq)) {
		p = (vm_page_t) queue_first(&object->memq);

		VM_PAGE_CHECK(p);

		vm_page_lock_queues();
		vm_page_free(p);
		vm_page_unlock_queues();
	}

	/*
	 *	Let the pager know object is dead.
	 */

	if (object->pager != NULL) {
		vm_pager_deallocate(object->pager);
	}


	simple_lock(&vm_object_list_lock);
	queue_remove(&vm_object_list, object, vm_object_t, object_list);
	vm_object_count--;
	simple_unlock(&vm_object_list_lock);

	/*
	 *	Free the space for the object.
	 */

	free((caddr_t)object, M_VMOBJ);
}

/*
 *	vm_object_page_clean
 *
 *	Clean all dirty pages in the specified range of object.
 *	Leaves page on whatever queue it is currently on.
 *
 *	Odd semantics: if start == end, we clean everything.
 *
 *	The object must be locked.
 */
void
vm_object_page_clean(object, start, end)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register vm_page_t	p;
	int s;
	int size;

	if (object->pager == NULL)
		return;

	if (start != end) {
		start = trunc_page(start);
		end = round_page(end);
	}
	size = end - start;

again:
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p) && ((start == end) || (size != 0) ) ) {
		if (start == end || (p->offset >= start && p->offset < end)) {
			if (p->flags & PG_BUSY)
				goto next;

			size -= PAGE_SIZE;

			if ((p->flags & PG_CLEAN)
				 && pmap_is_modified(VM_PAGE_TO_PHYS(p)))
				p->flags &= ~PG_CLEAN;

			if (p->flags & PG_ACTIVE)
				vm_page_deactivate(p);

			if ((p->flags & PG_CLEAN) == 0) {
				vm_pageout_clean(p,1);
				goto again;
			}
		}
next:
		p = (vm_page_t) queue_next(&p->listq);
	}
	wakeup((caddr_t)object);
}

/*
 *	vm_object_deactivate_pages
 *
 *	Deactivate all pages in the specified object.  (Keep its pages
 *	in memory even though it is no longer referenced.)
 *
 *	The object must be locked.
 */
void
vm_object_deactivate_pages(object)
	register vm_object_t	object;
{
	register vm_page_t	p, next;

	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		next = (vm_page_t) queue_next(&p->listq);
		vm_page_lock_queues();
		if ((p->flags & (PG_INACTIVE|PG_BUSY)) == 0 &&
			(p->wire_count == 0 && p->hold_count == 0))
			vm_page_deactivate(p);	/* optimisation from mach 3.0 -
						 * andrew@werple.apana.org.au,
						 * Feb '93
						 */
		vm_page_unlock_queues();
		p = next;
	}
}

/*
 *	Trim the object cache to size.
 */
void
vm_object_cache_trim()
{
	register vm_object_t	object;

	vm_object_cache_lock();
	while (vm_object_cached > vm_cache_max) {
		object = (vm_object_t) queue_first(&vm_object_cached_list);
		vm_object_cache_unlock();

		if (object != vm_object_lookup(object->pager))
			panic("vm_object_deactivate: I'm sooo confused.");

		pager_cache(object, FALSE);

		vm_object_cache_lock();
	}
	vm_object_cache_unlock();
}

/*
 *	vm_object_shutdown()
 *
 *	Shut down the object system.  Unfortunately, while we
 *	may be trying to do this, init is happily waiting for
 *	processes to exit, and therefore will be causing some objects
 *	to be deallocated.  To handle this, we gain a fake reference
 *	to all objects we release paging areas for.  This will prevent
 *	a duplicate deallocation.  This routine is probably full of
 *	race conditions!
 */

#if 0
void
vm_object_shutdown()
{
	register vm_object_t	object;

	/*
	 *	Clean up the object cache *before* we screw up the reference
	 *	counts on all of the objects.
	 */

	vm_object_cache_clear();


	/*
	 *	First we gain a reference to each object so that
	 *	no one else will deallocate them.
	 */

	simple_lock(&vm_object_list_lock);
	object = (vm_object_t) queue_first(&vm_object_list);
	while (!queue_end(&vm_object_list, (queue_entry_t) object)) {
		vm_object_reference(object);
		object = (vm_object_t) queue_next(&object->object_list);
	}
	simple_unlock(&vm_object_list_lock);

	/*
	 *	Now we deallocate all the paging areas.  We don't need
	 *	to lock anything because we've reduced to a single
	 *	processor while shutting down.	This also assumes that
	 *	no new objects are being created.
	 */

	object = (vm_object_t) queue_first(&vm_object_list);
	while (!queue_end(&vm_object_list, (queue_entry_t) object)) {
		if (object->pager != NULL)
			vm_pager_deallocate(object->pager);
		object = (vm_object_t) queue_next(&object->object_list);
		printf(".");
	}
	printf("done.\n");
}
#endif
/*
 *	vm_object_pmap_copy:
 *
 *	Makes all physical pages in the specified
 *	object range copy-on-write.  No writeable
 *	references to these pages should remain.
 *
 *	The object must *not* be locked.
 */
void
vm_object_pmap_copy(object, start, end)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register vm_page_t	p;
	vm_offset_t amount;

	if (object == NULL)
		return;

	start = trunc_page(start);
	end = round_page(end);

	amount = ((end - start) + PAGE_SIZE - 1) / PAGE_SIZE;

	vm_object_lock(object);
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		if ((start <= p->offset) && (p->offset < end)) {
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_READ);
			p->flags |= PG_COPY_ON_WRITE;
			amount -= 1;
			if (amount <= 0)
				break;
		}
		p = (vm_page_t) queue_next(&p->listq);
	}
	vm_object_unlock(object);
}

/*
 *	vm_object_pmap_remove:
 *
 *	Removes all physical pages in the specified
 *	object range from all physical maps.
 *
 *	The object must *not* be locked.
 */
void
vm_object_pmap_remove(object, start, end)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register vm_page_t	p;
	vm_offset_t size;

	if (object == NULL)
		return;

	vm_object_lock(object);
again:
	size = ((end - start) + PAGE_SIZE - 1) / PAGE_SIZE;
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		if ((start <= p->offset) && (p->offset < end)) {
			if (p->flags & PG_BUSY) {
				p->flags |= PG_WANTED;
				tsleep((caddr_t) p, PVM, "vmopmr", 0);
				goto again;
			}
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
			if ((p->flags & PG_CLEAN) == 0)
				p->flags |= PG_LAUNDRY;
			if (--size <= 0) break;
		}
		p = (vm_page_t) queue_next(&p->listq);
	}
	vm_object_unlock(object);
}

/*
 *	vm_object_copy:
 *
 *	Create a new object which is a copy of an existing
 *	object, and mark all of the pages in the existing
 *	object 'copy-on-write'.  The new object has one reference.
 *	Returns the new object.
 *
 *	May defer the copy until later if the object is not backed
 *	up by a non-default pager.
 */
void
vm_object_copy(src_object, src_offset, size, dst_object, dst_offset, src_needs_copy)
	register vm_object_t	src_object;
	vm_offset_t		src_offset;
	vm_size_t		size;
	vm_object_t		*dst_object;	/* OUT */
	vm_offset_t		*dst_offset;	/* OUT */
	boolean_t		*src_needs_copy;	/* OUT */
{
	register vm_object_t	new_copy;
	register vm_object_t	old_copy;
	vm_offset_t		new_start, new_end;
	vm_offset_t		src_offset_end;
	vm_offset_t		tmpsize;

	register vm_page_t	p;

	if (src_object == NULL) {
		/*
		 *	Nothing to copy
		 */
		*dst_object = NULL;
		*dst_offset = 0;
		*src_needs_copy = FALSE;
		return;
	}

	/*
	 *	If the object's pager is null_pager or the
	 *	default pager, we don't have to make a copy
	 *	of it.  Instead, we set the needs copy flag and
	 *	make a shadow later.
	 *	DYSON: check for swap(default) pager too....
	 */

	vm_object_lock(src_object);

	/*
	 *	Try to collapse the object before copying it.
	 */

	vm_object_collapse(src_object);

	if (src_object->pager == NULL ||
	    src_object->pager->pg_type == PG_SWAP ||  
	    src_object->internal) {

		/*
		 *	Make another reference to the object
		 */
		src_object->ref_count++;

		/*
		 *	Mark all of the pages copy-on-write.
		 */
		tmpsize = size;
		src_offset_end = src_offset + size;
		for (p = (vm_page_t) queue_first(&src_object->memq);
		     !queue_end(&src_object->memq, (queue_entry_t)p) && tmpsize > 0;
		     p = (vm_page_t) queue_next(&p->listq)) {
			if (src_offset <= p->offset &&
			    p->offset < src_offset_end) {
				p->flags |= PG_COPY_ON_WRITE;
				tmpsize -= PAGE_SIZE;
			    }
		}
		vm_object_unlock(src_object);

		*dst_object = src_object;
		*dst_offset = src_offset;
		
		/*
		 *	Must make a shadow when write is desired
		 */
		*src_needs_copy = TRUE;
		return;
	}

	/*
	 *	If the object has a pager, the pager wants to
	 *	see all of the changes.  We need a copy-object
	 *	for the changed pages.
	 *
	 *	If there is a copy-object, and it is empty,
	 *	no changes have been made to the object since the
	 *	copy-object was made.  We can use the same copy-
	 *	object.
	 */

    Retry1:
	old_copy = src_object->copy;
	if (old_copy != NULL) {
		/*
		 *	Try to get the locks (out of order)
		 */
		if (!vm_object_lock_try(old_copy)) {
			vm_object_unlock(src_object);

			/* should spin a bit here... */
			vm_object_lock(src_object);
			goto Retry1;
		}

		if (old_copy->resident_page_count == 0 &&
		    old_copy->pager == NULL) {
			/*
			 *	Return another reference to
			 *	the existing copy-object.
			 */
			old_copy->ref_count++;
			vm_object_unlock(old_copy);
			vm_object_unlock(src_object);
			*dst_object = old_copy;
			*dst_offset = src_offset;
			*src_needs_copy = FALSE;
			return;
		}
		vm_object_unlock(old_copy);
	}
	vm_object_unlock(src_object);

	/*
	 *	If the object has a pager, the pager wants
	 *	to see all of the changes.  We must make
	 *	a copy-object and put the changed pages there.
	 *
	 *	The copy-object is always made large enough to
	 *	completely shadow the original object, since
	 *	it may have several users who want to shadow
	 *	the original object at different points.
	 */

	new_copy = vm_object_allocate(src_object->size);

    Retry2:
	vm_object_lock(src_object);
	/*
	 *	Copy object may have changed while we were unlocked
	 */
	old_copy = src_object->copy;
	if (old_copy != NULL) {
		/*
		 *	Try to get the locks (out of order)
		 */
		if (!vm_object_lock_try(old_copy)) {
			vm_object_unlock(src_object);
			goto Retry2;
		}

		/*
		 *	Consistency check
		 */
		if (old_copy->shadow != src_object ||
		    old_copy->shadow_offset != (vm_offset_t) 0)
			panic("vm_object_copy: copy/shadow inconsistency");

		/*
		 *	Make the old copy-object shadow the new one.
		 *	It will receive no more pages from the original
		 *	object.
		 */

		src_object->ref_count--;	/* remove ref. from old_copy */
		old_copy->shadow = new_copy;
		new_copy->ref_count++;		/* locking not needed - we
						   have the only pointer */
		vm_object_unlock(old_copy);	/* done with old_copy */
	}

	new_start = (vm_offset_t) 0;	/* always shadow original at 0 */
	new_end   = (vm_offset_t) new_copy->size; /* for the whole object */

	/*
	 *	Point the new copy at the existing object.
	 */

	new_copy->shadow = src_object;
	new_copy->shadow_offset = new_start;
	src_object->ref_count++;
	src_object->copy = new_copy;

	/*
	 *	Mark all the affected pages of the existing object
	 *	copy-on-write.
	 */
	tmpsize = size;
	p = (vm_page_t) queue_first(&src_object->memq);
	while (!queue_end(&src_object->memq, (queue_entry_t) p) && tmpsize > 0) {
		if ((new_start <= p->offset) && (p->offset < new_end)) {
			p->flags |= PG_COPY_ON_WRITE;
			tmpsize -= PAGE_SIZE;
		}
		p = (vm_page_t) queue_next(&p->listq);
	}

	vm_object_unlock(src_object);

	*dst_object = new_copy;
	*dst_offset = src_offset - new_start;
	*src_needs_copy = FALSE;
}

/*
 *	vm_object_shadow:
 *
 *	Create a new object which is backed by the
 *	specified existing object range.  The source
 *	object reference is deallocated.
 *
 *	The new object and offset into that object
 *	are returned in the source parameters.
 */

void
vm_object_shadow(object, offset, length)
	vm_object_t	*object;	/* IN/OUT */
	vm_offset_t	*offset;	/* IN/OUT */
	vm_size_t	length;
{
	register vm_object_t	source;
	register vm_object_t	result;

	source = *object;

	/*
	 *	Allocate a new object with the given length
	 */

	if ((result = vm_object_allocate(length)) == NULL)
		panic("vm_object_shadow: no object for shadowing");

	/*
	 *	The new object shadows the source object, adding
	 *	a reference to it.  Our caller changes his reference
	 *	to point to the new object, removing a reference to
	 *	the source object.  Net result: no change of reference
	 *	count.
	 */
	result->shadow = source;
	
	/*
	 *	Store the offset into the source object,
	 *	and fix up the offset into the new object.
	 */

	result->shadow_offset = *offset;

	/*
	 *	Return the new things
	 */

	*offset = 0;
	*object = result;
}

/*
 *	Set the specified object's pager to the specified pager.
 */

void
vm_object_setpager(object, pager, paging_offset,
			read_only)
	vm_object_t	object;
	vm_pager_t	pager;
	vm_offset_t	paging_offset;
	boolean_t	read_only;
{
#ifdef	lint
	read_only++;	/* No longer used */
#endif	lint

	vm_object_lock(object);			/* XXX ? */
	if (object->pager && object->pager != pager) {
		panic("!!!pager already allocated!!!\n");
	}
	object->pager = pager;
	object->paging_offset = paging_offset;
	vm_object_unlock(object);			/* XXX ? */
}

/*
 *	vm_object_hash hashes the pager/id pair.
 */

#define vm_object_hash(pager) \
	((((unsigned)pager) >> 5)%VM_OBJECT_HASH_COUNT)

/*
 *	vm_object_lookup looks in the object cache for an object with the
 *	specified pager and paging id.
 */

vm_object_t
vm_object_lookup(pager)
	vm_pager_t	pager;
{
	register queue_t		bucket;
	register vm_object_hash_entry_t	entry;
	vm_object_t			object;

	bucket = &vm_object_hashtable[vm_object_hash(pager)];

	vm_object_cache_lock();

	entry = (vm_object_hash_entry_t) queue_first(bucket);
	while (!queue_end(bucket, (queue_entry_t) entry)) {
		object = entry->object;
		if (object->pager == pager) {
			vm_object_lock(object);
			if (object->ref_count == 0) {
				queue_remove(&vm_object_cached_list, object,
						vm_object_t, cached_list);
				vm_object_cached--;
			}
			object->ref_count++;
			vm_object_unlock(object);
			vm_object_cache_unlock();
			return(object);
		}
		entry = (vm_object_hash_entry_t) queue_next(&entry->hash_links);
	}

	vm_object_cache_unlock();
	return(NULL);
}

/*
 *	vm_object_enter enters the specified object/pager/id into
 *	the hash table.
 */

void
vm_object_enter(object, pager)
	vm_object_t	object;
	vm_pager_t	pager;
{
	register queue_t		bucket;
	register vm_object_hash_entry_t	entry;

	/*
	 *	We don't cache null objects, and we can't cache
	 *	objects with the null pager.
	 */

	if (object == NULL)
		return;
	if (pager == NULL)
		return;

	bucket = &vm_object_hashtable[vm_object_hash(pager)];
	entry = (vm_object_hash_entry_t)
		malloc((u_long)sizeof *entry, M_VMOBJHASH, M_WAITOK);
	entry->object = object;
	object->can_persist = TRUE;

	vm_object_cache_lock();
	queue_enter(bucket, entry, vm_object_hash_entry_t, hash_links);
	vm_object_cache_unlock();
}

/*
 *	vm_object_remove:
 *
 *	Remove the pager from the hash table.
 *	Note:  This assumes that the object cache
 *	is locked.  XXX this should be fixed
 *	by reorganizing vm_object_deallocate.
 */
void
vm_object_remove(pager)
	register vm_pager_t	pager;
{
	register queue_t		bucket;
	register vm_object_hash_entry_t	entry;
	register vm_object_t		object;

	bucket = &vm_object_hashtable[vm_object_hash(pager)];

	entry = (vm_object_hash_entry_t) queue_first(bucket);
	while (!queue_end(bucket, (queue_entry_t) entry)) {
		object = entry->object;
		if (object->pager == pager) {
			queue_remove(bucket, entry, vm_object_hash_entry_t,
					hash_links);
			free((caddr_t)entry, M_VMOBJHASH);
			break;
		}
		entry = (vm_object_hash_entry_t) queue_next(&entry->hash_links);
	}
}

/*
 *	vm_object_cache_clear removes all objects from the cache.
 *
 */
void
vm_object_cache_clear()
{
	register vm_object_t	object;

	/*
	 *	Remove each object in the cache by scanning down the
	 *	list of cached objects.
	 */
	vm_object_cache_lock();
	while (!queue_empty(&vm_object_cached_list)) {
		object = (vm_object_t) queue_first(&vm_object_cached_list);
		vm_object_cache_unlock();

		/* 
		 * Note: it is important that we use vm_object_lookup
		 * to gain a reference, and not vm_object_reference, because
		 * the logic for removing an object from the cache lies in 
		 * lookup.
		 */
		if (object != vm_object_lookup(object->pager))
			panic("vm_object_cache_clear: I'm sooo confused.");
		pager_cache(object, FALSE);

		vm_object_cache_lock();
	}
	vm_object_cache_unlock();
}

boolean_t	vm_object_collapse_allowed = TRUE;
/*
 *	vm_object_collapse:
 *
 *	Collapse an object with the object backing it.
 *	Pages in the backing object are moved into the
 *	parent, and the backing object is deallocated.
 *
 *	Requires that the object be locked and the page
 *	queues be unlocked.
 *
 *	This routine has significant changes by John S. Dyson
 *	to fix some swap memory leaks.  18 Dec 93
 *
 */
void
vm_object_collapse(object)
	register vm_object_t	object;

{
	register vm_object_t	backing_object;
	register vm_offset_t	backing_offset;
	register vm_size_t	size;
	register vm_offset_t	new_offset;
	register vm_page_t	p, pp;

	if (!vm_object_collapse_allowed)
		return;

	while (TRUE) {
		/*
		 *	Verify that the conditions are right for collapse:
		 *
		 *	The object exists and no pages in it are currently
		 *	being paged out.
		 */
		if (object == NULL ||
		    object->paging_in_progress != 0)
			return;

		/*
		 *		There is a backing object, and
		 */
	
		if ((backing_object = object->shadow) == NULL)
			return;
	
		vm_object_lock(backing_object);
		/*
		 *	...
		 *		The backing object is not read_only,
		 *		and no pages in the backing object are
		 *		currently being paged out.
		 *		The backing object is internal.
		 */
	
		if (!backing_object->internal ||
		    backing_object->paging_in_progress != 0 ) {
			vm_object_unlock(backing_object);
			return;
		}
	
		/*
		 *	The backing object can't be a copy-object:
		 *	the shadow_offset for the copy-object must stay
		 *	as 0.  Furthermore (for the 'we have all the
		 *	pages' case), if we bypass backing_object and
		 *	just shadow the next object in the chain, old
		 *	pages from that object would then have to be copied
		 *	BOTH into the (former) backing_object and into the
		 *	parent object.
		 */
		if (backing_object->shadow != NULL &&
		    backing_object->shadow->copy == backing_object) {
			vm_object_unlock(backing_object);
			return;
		}

		/*
		 * we can deal only with the swap pager
		 */
		if ((object->pager && 
		    	object->pager->pg_type != PG_SWAP) ||
		    (backing_object->pager && 
		    	backing_object->pager->pg_type != PG_SWAP)) {
			vm_object_unlock(backing_object);
			return;
		}
			

		/*
		 *	We know that we can either collapse the backing
		 *	object (if the parent is the only reference to
		 *	it) or (perhaps) remove the parent's reference
		 *	to it.
		 */

		backing_offset = object->shadow_offset;
		size = object->size;

		/*
		 *	If there is exactly one reference to the backing
		 *	object, we can collapse it into the parent.
		 */
	
		if (backing_object->ref_count == 1) {

			/*
			 *	We can collapse the backing object.
			 *
			 *	Move all in-memory pages from backing_object
			 *	to the parent.  Pages that have been paged out
			 *	will be overwritten by any of the parent's
			 *	pages that shadow them.
			 */

			while (!queue_empty(&backing_object->memq)) {

				p = (vm_page_t)
					queue_first(&backing_object->memq);

				new_offset = (p->offset - backing_offset);

				/*
				 *	If the parent has a page here, or if
				 *	this page falls outside the parent,
				 *	dispose of it.
				 *
				 *	Otherwise, move it as planned.
				 */

				if (p->offset < backing_offset ||
				    new_offset >= size) {
					vm_page_lock_queues();
					vm_page_free(p);
					vm_page_unlock_queues();
				} else {
				    pp = vm_page_lookup(object, new_offset);
				    if (pp != NULL || (object->pager && vm_pager_has_page(object->pager,
						object->paging_offset + new_offset))) {
					vm_page_lock_queues();
					vm_page_free(p);
					vm_page_unlock_queues();
				    } else {
					vm_page_rename(p, object, new_offset);
				    }
				}
			}

			/*
			 *	Move the pager from backing_object to object.
			 */

			if (backing_object->pager) {
				backing_object->paging_in_progress++;
				if (object->pager) {
					vm_pager_t bopager;
					object->paging_in_progress++;
					/*
					 * copy shadow object pages into ours
					 * and destroy unneeded pages in shadow object.
					 */
					bopager = backing_object->pager;
					backing_object->pager = NULL;
					vm_object_remove(backing_object->pager);
					swap_pager_copy(
						bopager, backing_object->paging_offset,
						object->pager, object->paging_offset,
						object->shadow_offset);
					object->paging_in_progress--;
					if (object->paging_in_progress == 0)
						wakeup((caddr_t)object);
				} else {
					object->paging_in_progress++;
					/*
					 * grab the shadow objects pager
					 */
					object->pager = backing_object->pager;
					object->paging_offset = backing_object->paging_offset + backing_offset;
					vm_object_remove(backing_object->pager);
					backing_object->pager = NULL;
					/*
					 * free unnecessary blocks
					 */
					swap_pager_freespace(object->pager, 0, object->paging_offset);
					object->paging_in_progress--;
					if (object->paging_in_progress == 0)
						wakeup((caddr_t)object);
				}
				backing_object->paging_in_progress--;
				if (backing_object->paging_in_progress == 0)
					wakeup((caddr_t)backing_object);
			}


			/*
			 *	Object now shadows whatever backing_object did.
			 *	Note that the reference to backing_object->shadow
			 *	moves from within backing_object to within object.
			 */

			object->shadow = backing_object->shadow;
			object->shadow_offset += backing_object->shadow_offset;
			if (object->shadow != NULL &&
			    object->shadow->copy != NULL) {
				panic("vm_object_collapse: we collapsed a copy-object!");
			}
			/*
			 *	Discard backing_object.
			 *
			 *	Since the backing object has no pages, no
			 *	pager left, and no object references within it,
			 *	all that is necessary is to dispose of it.
			 */

			vm_object_unlock(backing_object);

			simple_lock(&vm_object_list_lock);
			queue_remove(&vm_object_list, backing_object,
						vm_object_t, object_list);
			vm_object_count--;
			simple_unlock(&vm_object_list_lock);

			free((caddr_t)backing_object, M_VMOBJ);

			object_collapses++;
		}
		else {
			/*
			 *	If all of the pages in the backing object are
			 *	shadowed by the parent object, the parent
			 *	object no longer has to shadow the backing
			 *	object; it can shadow the next one in the
			 *	chain.
			 *
			 *	The backing object must not be paged out - we'd
			 *	have to check all of the paged-out pages, as
			 *	well.
			 */

			if (backing_object->pager != NULL) {
				vm_object_unlock(backing_object);
				return;
			}

			/*
			 *	Should have a check for a 'small' number
			 *	of pages here.
			 */

			p = (vm_page_t) queue_first(&backing_object->memq);
			while (!queue_end(&backing_object->memq,
					  (queue_entry_t) p)) {

				new_offset = (p->offset - backing_offset);

				/*
				 *	If the parent has a page here, or if
				 *	this page falls outside the parent,
				 *	keep going.
				 *
				 *	Otherwise, the backing_object must be
				 *	left in the chain.
				 */

				if (p->offset >= backing_offset &&
				    new_offset <= size &&
				    ((pp = vm_page_lookup(object, new_offset)) == NULL || (pp->flags & PG_FAKE)) &&
					(!object->pager || !vm_pager_has_page(object->pager, object->paging_offset+new_offset))) {
					/*
					 *	Page still needed.
					 *	Can't go any further.
					 */
					vm_object_unlock(backing_object);
					return;
				}
				p = (vm_page_t) queue_next(&p->listq);
			}

			/*
			 *	Make the parent shadow the next object
			 *	in the chain.  Deallocating backing_object
			 *	will not remove it, since its reference
			 *	count is at least 2.
			 */

			vm_object_reference(object->shadow = backing_object->shadow);
			object->shadow_offset += backing_object->shadow_offset;

#if 1
			/* Mach 3.0 code */
			/* andrew@werple.apana.org.au, 12 Feb 1993 */

			/*
			 *      Backing object might have had a copy pointer
			 *      to us.  If it did, clear it.
			 */
			 if (backing_object->copy == object)
				backing_object->copy = NULL;
#endif

			/*	Drop the reference count on backing_object.
			 *	Since its ref_count was at least 2, it
			 *	will not vanish; so we don't need to call
			 *	vm_object_deallocate.
			 */
			if (backing_object->ref_count == 1)
				printf("should have called obj deallocate\n");
			backing_object->ref_count--;
			vm_object_unlock(backing_object);

			object_bypasses ++;

		}

		/*
		 *	Try again with this object's new backing object.
		 */
	}
}

/*
 *	vm_object_page_remove: [internal]
 *
 *	Removes all physical pages in the specified
 *	object range from the object's list of pages.
 *
 *	The object must be locked.
 */
void
vm_object_page_remove(object, start, end)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register vm_page_t	p, next;
	vm_offset_t size;
	int cnt;
	int s;

	if (object == NULL)
		return;

	start = trunc_page(start);
	end = round_page(end);
again:
	size = end-start;
	if (size > 4*PAGE_SIZE || size >= object->size/4) {
		p = (vm_page_t) queue_first(&object->memq);
		while (!queue_end(&object->memq, (queue_entry_t) p) && size > 0) {
			next = (vm_page_t) queue_next(&p->listq);
			if ((start <= p->offset) && (p->offset < end)) {
				if (p->flags & PG_BUSY) {
					p->flags |= PG_WANTED;
					tsleep((caddr_t) p, PVM, "vmopar", 0);
					goto again;
				}
				pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
				vm_page_lock_queues();
				vm_page_free(p);
				vm_page_unlock_queues();
				size -= PAGE_SIZE;
			}
			p = next;
		}
	} else {
		while (size > 0) {
			while (p = vm_page_lookup(object, start)) {
				if (p->flags & PG_BUSY) {
					p->flags |= PG_WANTED;
					tsleep((caddr_t) p, PVM, "vmopar", 0);
					goto again;
				}
				pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
				vm_page_lock_queues();
				vm_page_free(p);
				vm_page_unlock_queues();
			}
			start += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	}
}

/*
 *	Routine:	vm_object_coalesce
 *	Function:	Coalesces two objects backing up adjoining
 *			regions of memory into a single object.
 *
 *	returns TRUE if objects were combined.
 *
 *	NOTE:	Only works at the moment if the second object is NULL -
 *		if it's not, which object do we lock first?
 *
 *	Parameters:
 *		prev_object	First object to coalesce
 *		prev_offset	Offset into prev_object
 *		next_object	Second object into coalesce
 *		next_offset	Offset into next_object
 *
 *		prev_size	Size of reference to prev_object
 *		next_size	Size of reference to next_object
 *
 *	Conditions:
 *	The object must *not* be locked.
 */
boolean_t
vm_object_coalesce(prev_object, next_object, prev_offset, next_offset, prev_size, next_size)
	register vm_object_t	prev_object;
	vm_object_t	next_object;
	vm_offset_t	prev_offset, next_offset;
	vm_size_t	prev_size, next_size;
{
	vm_size_t	newsize;

#ifdef	lint
	next_offset++;
#endif	lint

	if (next_object != NULL) {
		return(FALSE);
	}

	if (prev_object == NULL) {
		return(TRUE);
	}

	vm_object_lock(prev_object);

	/*
	 *	Try to collapse the object first
	 */
	vm_object_collapse(prev_object);

	/*
	 *	Can't coalesce if:
	 *	. more than one reference
	 *	. paged out
	 *	. shadows another object
	 *	. has a copy elsewhere
	 *	(any of which mean that the pages not mapped to
	 *	prev_entry may be in use anyway)
	 */
	if (prev_object->ref_count > 1 ||
		prev_object->pager != NULL ||
		prev_object->shadow != NULL ||
		prev_object->copy != NULL) {
		vm_object_unlock(prev_object);
		return(FALSE);
	}

	/*
	 *	Remove any pages that may still be in the object from
	 *	a previous deallocation.
	 */
	vm_object_page_remove(prev_object,
			prev_offset + prev_size,
			prev_offset + prev_size + next_size);

	/*
	 *	Extend the object if necessary.
	 */
	newsize = prev_offset + prev_size + next_size;
	if (newsize > prev_object->size)
		prev_object->size = newsize;

	vm_object_unlock(prev_object);
	return(TRUE);
}

/*
 * returns page after looking up in shadow chain
 */
 
vm_page_t
vm_object_page_lookup(object, offset)
	vm_object_t object;
	vm_offset_t offset;
{
	vm_page_t m;
	if (!(m=vm_page_lookup(object, offset))) {
		if (!object->shadow)
			return 0;
		else
			return vm_object_page_lookup(object->shadow, offset + object->shadow_offset);
	}
	return m;
}

#if defined(DEBUG) || (NDDB > 0)
/*
 *	vm_object_print:	[ debug ]
 */
void
vm_object_print(object, full)
	vm_object_t	object;
	boolean_t	full;
{
	register vm_page_t	p;
	extern indent;

	register int count;

	if (object == NULL)
		return;

	iprintf("Object 0x%x: size=0x%x, res=%d, ref=%d, ",
		(int) object, (int) object->size,
		object->resident_page_count, object->ref_count);
	printf("pager=0x%x+0x%x, shadow=(0x%x)+0x%x\n",
	       (int) object->pager, (int) object->paging_offset,
	       (int) object->shadow, (int) object->shadow_offset);
	printf("cache: next=0x%x, prev=0x%x\n",
	       object->cached_list.next, object->cached_list.prev);

	if (!full)
		return;

	indent += 2;
	count = 0;
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		if (count == 0)
			iprintf("memory:=");
		else if (count == 6) {
			printf("\n");
			iprintf(" ...");
			count = 0;
		} else
			printf(",");
		count++;

		printf("(off=0x%x,page=0x%x)", p->offset, VM_PAGE_TO_PHYS(p));
		p = (vm_page_t) queue_next(&p->listq);
	}
	if (count != 0)
		printf("\n");
	indent -= 2;
}
#endif /* defined(DEBUG) || (NDDB > 0) */
