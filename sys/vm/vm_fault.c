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
 *	@(#)vm_fault.c	8.4 (Berkeley) 1/12/94
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
 *	Page fault handling module.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

/*
 *	vm_fault:
 *
 *	Handle a page fault occuring at the given address,
 *	requiring the given permissions, in the map specified.
 *	If successful, the page is inserted into the
 *	associated physical map.
 *
 *	NOTE: the given address should be truncated to the
 *	proper page address.
 *
 *	KERN_SUCCESS is returned if the page fault is handled; otherwise,
 *	a standard error specifying why the fault is fatal is returned.
 *
 *
 *	The map in question must be referenced, and remains so.
 *	Caller may hold no locks.
 */
int
vm_fault(map, vaddr, fault_type, change_wiring)
	vm_map_t	map;
	vm_offset_t	vaddr;
	vm_prot_t	fault_type;
	boolean_t	change_wiring;
{
	vm_object_t		first_object;
	vm_offset_t		first_offset;
	vm_map_entry_t		entry;
	register vm_object_t	object;
	register vm_offset_t	offset;
	register vm_page_t	m;
	vm_page_t		first_m;
	vm_prot_t		prot;
	int			result;
	boolean_t		wired;
	boolean_t		su;
	boolean_t		lookup_still_valid;
	boolean_t		page_exists;
	vm_page_t		old_m;
	vm_object_t		next_object;

	cnt.v_faults++;		/* needs lock XXX */
/*
 *	Recovery actions
 */
#define	FREE_PAGE(m)	{				\
	PAGE_WAKEUP(m);					\
	vm_page_lock_queues();				\
	vm_page_free(m);				\
	vm_page_unlock_queues();			\
}

#define	RELEASE_PAGE(m)	{				\
	PAGE_WAKEUP(m);					\
	vm_page_lock_queues();				\
	vm_page_activate(m);				\
	vm_page_unlock_queues();			\
}

#define	UNLOCK_MAP	{				\
	if (lookup_still_valid) {			\
		vm_map_lookup_done(map, entry);		\
		lookup_still_valid = FALSE;		\
	}						\
}

#define	UNLOCK_THINGS	{				\
	object->paging_in_progress--;			\
	vm_object_unlock(object);			\
	if (object != first_object) {			\
		vm_object_lock(first_object);		\
		FREE_PAGE(first_m);			\
		first_object->paging_in_progress--;	\
		vm_object_unlock(first_object);		\
	}						\
	UNLOCK_MAP;					\
}

#define	UNLOCK_AND_DEALLOCATE	{			\
	UNLOCK_THINGS;					\
	vm_object_deallocate(first_object);		\
}

    RetryFault: ;

	/*
	 *	Find the backing store object and offset into
	 *	it to begin the search.
	 */

	if ((result = vm_map_lookup(&map, vaddr, fault_type, &entry,
			&first_object, &first_offset,
			&prot, &wired, &su)) != KERN_SUCCESS) {
		return(result);
	}
	lookup_still_valid = TRUE;

	if (wired)
		fault_type = prot;

	first_m = NULL;

   	/*
	 *	Make a reference to this object to
	 *	prevent its disposal while we are messing with
	 *	it.  Once we have the reference, the map is free
	 *	to be diddled.  Since objects reference their
	 *	shadows (and copies), they will stay around as well.
	 */

	vm_object_lock(first_object);

	first_object->ref_count++;
	first_object->paging_in_progress++;

	/*
	 *	INVARIANTS (through entire routine):
	 *
	 *	1)	At all times, we must either have the object
	 *		lock or a busy page in some object to prevent
	 *		some other thread from trying to bring in
	 *		the same page.
	 *
	 *		Note that we cannot hold any locks during the
	 *		pager access or when waiting for memory, so
	 *		we use a busy page then.
	 *
	 *		Note also that we aren't as concerned about
	 *		more than one thead attempting to pager_data_unlock
	 *		the same page at once, so we don't hold the page
	 *		as busy then, but do record the highest unlock
	 *		value so far.  [Unlock requests may also be delivered
	 *		out of order.]
	 *
	 *	2)	Once we have a busy page, we must remove it from
	 *		the pageout queues, so that the pageout daemon
	 *		will not grab it away.
	 *
	 *	3)	To prevent another thread from racing us down the
	 *		shadow chain and entering a new page in the top
	 *		object before we do, we must keep a busy page in
	 *		the top object while following the shadow chain.
	 *
	 *	4)	We must increment paging_in_progress on any object
	 *		for which we have a busy page, to prevent
	 *		vm_object_collapse from removing the busy page
	 *		without our noticing.
	 */

	/*
	 *	Search for the page at object/offset.
	 */

	object = first_object;
	offset = first_offset;

	/*
	 *	See whether this page is resident
	 */

	while (TRUE) {
		m = vm_page_lookup(object, offset);
		if (m != NULL) {
			/*
			 *	If the page is being brought in,
			 *	wait for it and then retry.
			 */
			if (m->flags & PG_BUSY) {
#ifdef DOTHREADS
				int	wait_result;

				PAGE_ASSERT_WAIT(m, !change_wiring);
				UNLOCK_THINGS;
				thread_block();
				wait_result = current_thread()->wait_result;
				vm_object_deallocate(first_object);
				if (wait_result != THREAD_AWAKENED)
					return(KERN_SUCCESS);
				goto RetryFault;
#else
				PAGE_ASSERT_WAIT(m, !change_wiring);
				UNLOCK_THINGS;
				cnt.v_intrans++;
				thread_block();
				vm_object_deallocate(first_object);
				goto RetryFault;
#endif
			}

			/*
			 *	Remove the page from the pageout daemon's
			 *	reach while we play with it.
			 */

			vm_page_lock_queues();
			if (m->flags & PG_INACTIVE) {
				TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
				m->flags &= ~PG_INACTIVE;
				cnt.v_inactive_count--;
				cnt.v_reactivated++;
			} 

			if (m->flags & PG_ACTIVE) {
				TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
				m->flags &= ~PG_ACTIVE;
				cnt.v_active_count--;
			}
			vm_page_unlock_queues();

			/*
			 *	Mark page busy for other threads.
			 */
			m->flags |= PG_BUSY;
			break;
		}

		if (((object->pager != NULL) &&
				(!change_wiring || wired))
		    || (object == first_object)) {

			/*
			 *	Allocate a new page for this object/offset
			 *	pair.
			 */

			m = vm_page_alloc(object, offset);

			if (m == NULL) {
				UNLOCK_AND_DEALLOCATE;
				VM_WAIT;
				goto RetryFault;
			}
		}

		if (object->pager != NULL && (!change_wiring || wired)) {
			int rv;

			/*
			 *	Now that we have a busy page, we can
			 *	release the object lock.
			 */
			vm_object_unlock(object);

			/*
			 *	Call the pager to retrieve the data, if any,
			 *	after releasing the lock on the map.
			 */
			UNLOCK_MAP;
			cnt.v_pageins++;
			rv = vm_pager_get(object->pager, m, TRUE);

			/*
			 *	Reaquire the object lock to preserve our
			 *	invariant.
			 */
			vm_object_lock(object);

			/*
			 *	Found the page.
			 *	Leave it busy while we play with it.
			 */
			if (rv == VM_PAGER_OK) {
				/*
				 *	Relookup in case pager changed page.
				 *	Pager is responsible for disposition
				 *	of old page if moved.
				 */
				m = vm_page_lookup(object, offset);

				cnt.v_pgpgin++;
				m->flags &= ~PG_FAKE;
				m->flags |= PG_CLEAN;
				pmap_clear_modify(VM_PAGE_TO_PHYS(m));
				break;
			}

			/*
			 * IO error or page outside the range of the pager:
			 * cleanup and return an error.
			 */
			if (rv == VM_PAGER_ERROR || rv == VM_PAGER_BAD) {
				FREE_PAGE(m);
				UNLOCK_AND_DEALLOCATE;
				return(KERN_PROTECTION_FAILURE); /* XXX */
			}
			/*
			 * rv == VM_PAGER_FAIL:
			 *
			 * Page does not exist at this object/offset.
			 * Free the bogus page (waking up anyone waiting
			 * for it) and continue on to the next object.
			 *
			 * If this is the top-level object, we must
			 * leave the busy page to prevent another
			 * thread from rushing past us, and inserting
			 * the page in that object at the same time
			 * that we are.
			 */
			if (object != first_object) {
				FREE_PAGE(m);
				/* note that `m' is not used after this */
			}
		}

		/*
		 * We get here if the object has no pager (or unwiring)
		 * or the pager doesn't have the page.
		 */
		if (object == first_object)
			first_m = m;

		/*
		 *	Move on to the next object.  Lock the next
		 *	object before unlocking the current one.
		 */

		offset += object->shadow_offset;
		next_object = object->shadow;
		if (next_object == NULL) {
			/*
			 *	If there's no object left, fill the page
			 *	in the top object with zeros.
			 */
			if (object != first_object) {
				object->paging_in_progress--;
				vm_object_unlock(object);

				object = first_object;
				offset = first_offset;
				m = first_m;
				vm_object_lock(object);
			}
			first_m = NULL;

			vm_page_zero_fill(m);
			cnt.v_zfod++;
			m->flags &= ~PG_FAKE;
			break;
		}
		else {
			vm_object_lock(next_object);
			if (object != first_object)
				object->paging_in_progress--;
			vm_object_unlock(object);
			object = next_object;
			object->paging_in_progress++;
		}
	}

	if ((m->flags & (PG_ACTIVE | PG_INACTIVE | PG_BUSY)) != PG_BUSY)
		panic("vm_fault: active, inactive or !busy after main loop");

	/*
	 *	PAGE HAS BEEN FOUND.
	 *	[Loop invariant still holds -- the object lock
	 *	is held.]
	 */

	old_m = m;	/* save page that would be copied */

	/*
	 *	If the page is being written, but isn't
	 *	already owned by the top-level object,
	 *	we have to copy it into a new page owned
	 *	by the top-level object.
	 */

	if (object != first_object) {
	    	/*
		 *	We only really need to copy if we
		 *	want to write it.
		 */

	    	if (fault_type & VM_PROT_WRITE) {

			/*
			 *	If we try to collapse first_object at this
			 *	point, we may deadlock when we try to get
			 *	the lock on an intermediate object (since we
			 *	have the bottom object locked).  We can't
			 *	unlock the bottom object, because the page
			 *	we found may move (by collapse) if we do.
			 *
			 *	Instead, we first copy the page.  Then, when
			 *	we have no more use for the bottom object,
			 *	we unlock it and try to collapse.
			 *
			 *	Note that we copy the page even if we didn't
			 *	need to... that's the breaks.
			 */

		    	/*
			 *	We already have an empty page in
			 *	first_object - use it.
			 */

			vm_page_copy(m, first_m);
			first_m->flags &= ~PG_FAKE;

			/*
			 *	If another map is truly sharing this
			 *	page with us, we have to flush all
			 *	uses of the original page, since we
			 *	can't distinguish those which want the
			 *	original from those which need the
			 *	new copy.
			 *
			 *	XXX If we know that only one map has
			 *	access to this page, then we could
			 *	avoid the pmap_page_protect() call.
			 */

			vm_page_lock_queues();
			vm_page_activate(m);
			vm_page_deactivate(m);
			pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);
			vm_page_unlock_queues();

			/*
			 *	We no longer need the old page or object.
			 */
			PAGE_WAKEUP(m);
			object->paging_in_progress--;
			vm_object_unlock(object);

			/*
			 *	Only use the new page below...
			 */

			cnt.v_cow_faults++;
			m = first_m;
			object = first_object;
			offset = first_offset;

			/*
			 *	Now that we've gotten the copy out of the
			 *	way, let's try to collapse the top object.
			 */
			vm_object_lock(object);
			/*
			 *	But we have to play ugly games with
			 *	paging_in_progress to do that...
			 */
			object->paging_in_progress--;
			vm_object_collapse(object);
			object->paging_in_progress++;
		}
		else {
		    	prot &= ~VM_PROT_WRITE;
			m->flags |= PG_COPYONWRITE;
		}
	}

	if (m->flags & (PG_ACTIVE|PG_INACTIVE))
		panic("vm_fault: active or inactive before copy object handling");

	/*
	 *	If the page is being written, but hasn't been
	 *	copied to the copy-object, we have to copy it there.
	 */
    RetryCopy:
	if (first_object->copy != NULL) {
		vm_object_t copy_object = first_object->copy;
		vm_offset_t copy_offset;
		vm_page_t copy_m;

		/*
		 *	We only need to copy if we want to write it.
		 */
		if ((fault_type & VM_PROT_WRITE) == 0) {
			prot &= ~VM_PROT_WRITE;
			m->flags |= PG_COPYONWRITE;
		}
		else {
			/*
			 *	Try to get the lock on the copy_object.
			 */
			if (!vm_object_lock_try(copy_object)) {
				vm_object_unlock(object);
				/* should spin a bit here... */
				vm_object_lock(object);
				goto RetryCopy;
			}

			/*
			 *	Make another reference to the copy-object,
			 *	to keep it from disappearing during the
			 *	copy.
			 */
			copy_object->ref_count++;

			/*
			 *	Does the page exist in the copy?
			 */
			copy_offset = first_offset
				- copy_object->shadow_offset;
			copy_m = vm_page_lookup(copy_object, copy_offset);
			if (page_exists = (copy_m != NULL)) {
				if (copy_m->flags & PG_BUSY) {
#ifdef DOTHREADS
					int	wait_result;

					/*
					 *	If the page is being brought
					 *	in, wait for it and then retry.
					 */
					PAGE_ASSERT_WAIT(copy_m, !change_wiring);
					RELEASE_PAGE(m);
					copy_object->ref_count--;
					vm_object_unlock(copy_object);
					UNLOCK_THINGS;
					thread_block();
					wait_result = current_thread()->wait_result;
					vm_object_deallocate(first_object);
					if (wait_result != THREAD_AWAKENED)
						return(KERN_SUCCESS);
					goto RetryFault;
#else
					/*
					 *	If the page is being brought
					 *	in, wait for it and then retry.
					 */
					PAGE_ASSERT_WAIT(copy_m, !change_wiring);
					RELEASE_PAGE(m);
					copy_object->ref_count--;
					vm_object_unlock(copy_object);
					UNLOCK_THINGS;
					thread_block();
					vm_object_deallocate(first_object);
					goto RetryFault;
#endif
				}
			}

			/*
			 *	If the page is not in memory (in the object)
			 *	and the object has a pager, we have to check
			 *	if the pager has the data in secondary
			 *	storage.
			 */
			if (!page_exists) {

				/*
				 *	If we don't allocate a (blank) page
				 *	here... another thread could try
				 *	to page it in, allocate a page, and
				 *	then block on the busy page in its
				 *	shadow (first_object).  Then we'd
				 *	trip over the busy page after we
				 *	found that the copy_object's pager
				 *	doesn't have the page...
				 */
				copy_m = vm_page_alloc(copy_object,
								copy_offset);
				if (copy_m == NULL) {
					/*
					 *	Wait for a page, then retry.
					 */
					RELEASE_PAGE(m);
					copy_object->ref_count--;
					vm_object_unlock(copy_object);
					UNLOCK_AND_DEALLOCATE;
					VM_WAIT;
					goto RetryFault;
				}

			 	if (copy_object->pager != NULL) {
					vm_object_unlock(object);
					vm_object_unlock(copy_object);
					UNLOCK_MAP;

					page_exists = vm_pager_has_page(
							copy_object->pager,
							(copy_offset + copy_object->paging_offset));

					vm_object_lock(copy_object);

					/*
					 * Since the map is unlocked, someone
					 * else could have copied this object
					 * and put a different copy_object
					 * between the two.  Or, the last
					 * reference to the copy-object (other
					 * than the one we have) may have
					 * disappeared - if that has happened,
					 * we don't need to make the copy.
					 */
					if (copy_object->shadow != object ||
					    copy_object->ref_count == 1) {
						/*
						 *	Gaah... start over!
						 */
						FREE_PAGE(copy_m);
						vm_object_unlock(copy_object);
						vm_object_deallocate(copy_object);
							/* may block */
						vm_object_lock(object);
						goto RetryCopy;
					}
					vm_object_lock(object);

					if (page_exists) {
						/*
						 *	We didn't need the page
						 */
						FREE_PAGE(copy_m);
					}
				}
			}
			if (!page_exists) {
				/*
				 *	Must copy page into copy-object.
				 */
				vm_page_copy(m, copy_m);
				copy_m->flags &= ~PG_FAKE;

				/*
				 * Things to remember:
				 * 1. The copied page must be marked 'dirty'
				 *    so it will be paged out to the copy
				 *    object.
				 * 2. If the old page was in use by any users
				 *    of the copy-object, it must be removed
				 *    from all pmaps.  (We can't know which
				 *    pmaps use it.)
				 */
				vm_page_lock_queues();
				pmap_page_protect(VM_PAGE_TO_PHYS(old_m),
						  VM_PROT_NONE);
				copy_m->flags &= ~PG_CLEAN;
				vm_page_activate(copy_m);	/* XXX */
				vm_page_unlock_queues();

				PAGE_WAKEUP(copy_m);
			}
			/*
			 *	The reference count on copy_object must be
			 *	at least 2: one for our extra reference,
			 *	and at least one from the outside world
			 *	(we checked that when we last locked
			 *	copy_object).
			 */
			copy_object->ref_count--;
			vm_object_unlock(copy_object);
			m->flags &= ~PG_COPYONWRITE;
		}
	}

	if (m->flags & (PG_ACTIVE | PG_INACTIVE))
		panic("vm_fault: active or inactive before retrying lookup");

	/*
	 *	We must verify that the maps have not changed
	 *	since our last lookup.
	 */

	if (!lookup_still_valid) {
		vm_object_t	retry_object;
		vm_offset_t	retry_offset;
		vm_prot_t	retry_prot;

		/*
		 *	Since map entries may be pageable, make sure we can
		 *	take a page fault on them.
		 */
		vm_object_unlock(object);

		/*
		 *	To avoid trying to write_lock the map while another
		 *	thread has it read_locked (in vm_map_pageable), we
		 *	do not try for write permission.  If the page is
		 *	still writable, we will get write permission.  If it
		 *	is not, or has been marked needs_copy, we enter the
		 *	mapping without write permission, and will merely
		 *	take another fault.
		 */
		result = vm_map_lookup(&map, vaddr,
				fault_type & ~VM_PROT_WRITE, &entry,
				&retry_object, &retry_offset, &retry_prot,
				&wired, &su);

		vm_object_lock(object);

		/*
		 *	If we don't need the page any longer, put it on the
		 *	active list (the easiest thing to do here).  If no
		 *	one needs it, pageout will grab it eventually.
		 */

		if (result != KERN_SUCCESS) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			return(result);
		}

		lookup_still_valid = TRUE;

		if ((retry_object != first_object) ||
				(retry_offset != first_offset)) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			goto RetryFault;
		}

		/*
		 *	Check whether the protection has changed or the object
		 *	has been copied while we left the map unlocked.
		 *	Changing from read to write permission is OK - we leave
		 *	the page write-protected, and catch the write fault.
		 *	Changing from write to read permission means that we
		 *	can't mark the page write-enabled after all.
		 */
		prot &= retry_prot;
		if (m->flags & PG_COPYONWRITE)
			prot &= ~VM_PROT_WRITE;
	}

	/*
	 * (the various bits we're fiddling with here are locked by
	 * the object's lock)
	 */

	/* XXX This distorts the meaning of the copy_on_write bit */

	if (prot & VM_PROT_WRITE)
		m->flags &= ~PG_COPYONWRITE;

	/*
	 *	It's critically important that a wired-down page be faulted
	 *	only once in each map for which it is wired.
	 */

	if (m->flags & (PG_ACTIVE | PG_INACTIVE))
		panic("vm_fault: active or inactive before pmap_enter");

	vm_object_unlock(object);

	/*
	 *	Put this page into the physical map.
	 *	We had to do the unlock above because pmap_enter
	 *	may cause other faults.   We don't put the
	 *	page back on the active queue until later so
	 *	that the page-out daemon won't find us (yet).
	 */

	pmap_enter(map->pmap, vaddr, VM_PAGE_TO_PHYS(m), prot, wired);

	/*
	 *	If the page is not wired down, then put it where the
	 *	pageout daemon can find it.
	 */
	vm_object_lock(object);
	vm_page_lock_queues();
	if (change_wiring) {
		if (wired)
			vm_page_wire(m);
		else
			vm_page_unwire(m);
	}
	else
		vm_page_activate(m);
	vm_page_unlock_queues();

	/*
	 *	Unlock everything, and return
	 */

	PAGE_WAKEUP(m);
	UNLOCK_AND_DEALLOCATE;

	return(KERN_SUCCESS);

}

/*
 *	vm_fault_wire:
 *
 *	Wire down a range of virtual addresses in a map.
 */
int
vm_fault_wire(map, start, end)
	vm_map_t	map;
	vm_offset_t	start, end;
{
	register vm_offset_t	va;
	register pmap_t		pmap;
	int			rv;

	pmap = vm_map_pmap(map);

	/*
	 *	Inform the physical mapping system that the
	 *	range of addresses may not fault, so that
	 *	page tables and such can be locked down as well.
	 */

	pmap_pageable(pmap, start, end, FALSE);

	/*
	 *	We simulate a fault to get the page and enter it
	 *	in the physical map.
	 */

	for (va = start; va < end; va += PAGE_SIZE) {
		rv = vm_fault(map, va, VM_PROT_NONE, TRUE);
		if (rv) {
			if (va != start)
				vm_fault_unwire(map, start, va);
			return(rv);
		}
	}
	return(KERN_SUCCESS);
}


/*
 *	vm_fault_unwire:
 *
 *	Unwire a range of virtual addresses in a map.
 */
void vm_fault_unwire(map, start, end)
	vm_map_t	map;
	vm_offset_t	start, end;
{

	register vm_offset_t	va, pa;
	register pmap_t		pmap;

	pmap = vm_map_pmap(map);

	/*
	 *	Since the pages are wired down, we must be able to
	 *	get their mappings from the physical map system.
	 */

	vm_page_lock_queues();

	for (va = start; va < end; va += PAGE_SIZE) {
		pa = pmap_extract(pmap, va);
		if (pa == (vm_offset_t) 0) {
			panic("unwire: page not in pmap");
		}
		pmap_change_wiring(pmap, va, FALSE);
		vm_page_unwire(PHYS_TO_VM_PAGE(pa));
	}
	vm_page_unlock_queues();

	/*
	 *	Inform the physical mapping system that the range
	 *	of addresses may fault, so that page tables and
	 *	such may be unwired themselves.
	 */

	pmap_pageable(pmap, start, end, TRUE);

}

/*
 *	Routine:
 *		vm_fault_copy_entry
 *	Function:
 *		Copy all of the pages from a wired-down map entry to another.
 *
 *	In/out conditions:
 *		The source and destination maps must be locked for write.
 *		The source map entry must be wired down (or be a sharing map
 *		entry corresponding to a main map entry that is wired down).
 */

void vm_fault_copy_entry(dst_map, src_map, dst_entry, src_entry)
	vm_map_t	dst_map;
	vm_map_t	src_map;
	vm_map_entry_t	dst_entry;
	vm_map_entry_t	src_entry;
{

	vm_object_t	dst_object;
	vm_object_t	src_object;
	vm_offset_t	dst_offset;
	vm_offset_t	src_offset;
	vm_prot_t	prot;
	vm_offset_t	vaddr;
	vm_page_t	dst_m;
	vm_page_t	src_m;

#ifdef	lint
	src_map++;
#endif

	src_object = src_entry->object.vm_object;
	src_offset = src_entry->offset;

	/*
	 *	Create the top-level object for the destination entry.
	 *	(Doesn't actually shadow anything - we copy the pages
	 *	directly.)
	 */
	dst_object = vm_object_allocate(
			(vm_size_t) (dst_entry->end - dst_entry->start));

	dst_entry->object.vm_object = dst_object;
	dst_entry->offset = 0;

	prot  = dst_entry->max_protection;

	/*
	 *	Loop through all of the pages in the entry's range, copying
	 *	each one from the source object (it should be there) to the
	 *	destination object.
	 */
	for (vaddr = dst_entry->start, dst_offset = 0;
	     vaddr < dst_entry->end;
	     vaddr += PAGE_SIZE, dst_offset += PAGE_SIZE) {

		/*
		 *	Allocate a page in the destination object
		 */
		vm_object_lock(dst_object);
		do {
			dst_m = vm_page_alloc(dst_object, dst_offset);
			if (dst_m == NULL) {
				vm_object_unlock(dst_object);
				VM_WAIT;
				vm_object_lock(dst_object);
			}
		} while (dst_m == NULL);

		/*
		 *	Find the page in the source object, and copy it in.
		 *	(Because the source is wired down, the page will be
		 *	in memory.)
		 */
		vm_object_lock(src_object);
		src_m = vm_page_lookup(src_object, dst_offset + src_offset);
		if (src_m == NULL)
			panic("vm_fault_copy_wired: page missing");

		vm_page_copy(src_m, dst_m);

		/*
		 *	Enter it in the pmap...
		 */
		vm_object_unlock(src_object);
		vm_object_unlock(dst_object);

		pmap_enter(dst_map->pmap, vaddr, VM_PAGE_TO_PHYS(dst_m),
				prot, FALSE);

		/*
		 *	Mark it no longer busy, and put it on the active list.
		 */
		vm_object_lock(dst_object);
		vm_page_lock_queues();
		vm_page_activate(dst_m);
		vm_page_unlock_queues();
		PAGE_WAKEUP(dst_m);
		vm_object_unlock(dst_object);
	}

}
