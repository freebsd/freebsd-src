/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 *
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
 *	from: @(#)vm_fault.c	8.4 (Berkeley) 1/12/94
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

#include <sys/mount.h>	/* XXX Temporary for VFS_LOCK_GIANT() */

#define PFBAK 4
#define PFFOR 4
#define PAGEORDER_SIZE (PFBAK+PFFOR)

static int prefault_pageorder[] = {
	-1 * PAGE_SIZE, 1 * PAGE_SIZE,
	-2 * PAGE_SIZE, 2 * PAGE_SIZE,
	-3 * PAGE_SIZE, 3 * PAGE_SIZE,
	-4 * PAGE_SIZE, 4 * PAGE_SIZE
};

static int vm_fault_additional_pages(vm_page_t, int, int, vm_page_t *, int *);
static void vm_fault_prefault(pmap_t, vm_offset_t, vm_map_entry_t);

#define VM_FAULT_READ_AHEAD 8
#define VM_FAULT_READ_BEHIND 7
#define VM_FAULT_READ (VM_FAULT_READ_AHEAD+VM_FAULT_READ_BEHIND+1)

struct faultstate {
	vm_page_t m;
	vm_object_t object;
	vm_pindex_t pindex;
	vm_page_t first_m;
	vm_object_t	first_object;
	vm_pindex_t first_pindex;
	vm_map_t map;
	vm_map_entry_t entry;
	int lookup_still_valid;
	struct vnode *vp;
	int vfslocked;
};

static inline void
release_page(struct faultstate *fs)
{

	vm_page_wakeup(fs->m);
	vm_page_lock_queues();
	vm_page_deactivate(fs->m);
	vm_page_unlock_queues();
	fs->m = NULL;
}

static inline void
unlock_map(struct faultstate *fs)
{

	if (fs->lookup_still_valid) {
		vm_map_lookup_done(fs->map, fs->entry);
		fs->lookup_still_valid = FALSE;
	}
}

static void
unlock_and_deallocate(struct faultstate *fs)
{

	vm_object_pip_wakeup(fs->object);
	VM_OBJECT_UNLOCK(fs->object);
	if (fs->object != fs->first_object) {
		VM_OBJECT_LOCK(fs->first_object);
		vm_page_lock_queues();
		vm_page_free(fs->first_m);
		vm_page_unlock_queues();
		vm_object_pip_wakeup(fs->first_object);
		VM_OBJECT_UNLOCK(fs->first_object);
		fs->first_m = NULL;
	}
	vm_object_deallocate(fs->first_object);
	unlock_map(fs);	
	if (fs->vp != NULL) { 
		vput(fs->vp);
		fs->vp = NULL;
	}
	VFS_UNLOCK_GIANT(fs->vfslocked);
	fs->vfslocked = 0;
}

/*
 * TRYPAGER - used by vm_fault to calculate whether the pager for the
 *	      current object *might* contain the page.
 *
 *	      default objects are zero-fill, there is no real pager.
 */
#define TRYPAGER	(fs.object->type != OBJT_DEFAULT && \
			((fault_flags & VM_FAULT_CHANGE_WIRING) == 0 || wired))

/*
 *	vm_fault:
 *
 *	Handle a page fault occurring at the given address,
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
vm_fault(vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
	 int fault_flags)
{
	vm_prot_t prot;
	int is_first_object_locked, result;
	boolean_t are_queues_locked, growstack, wired;
	int map_generation;
	vm_object_t next_object;
	vm_page_t marray[VM_FAULT_READ];
	int hardfault;
	int faultcount, ahead, behind;
	struct faultstate fs;
	struct vnode *vp;
	int locked, error;

	hardfault = 0;
	growstack = TRUE;
	PCPU_INC(cnt.v_vm_faults);
	fs.vp = NULL;
	fs.vfslocked = 0;
	faultcount = behind = 0;

RetryFault:;

	/*
	 * Find the backing store object and offset into it to begin the
	 * search.
	 */
	fs.map = map;
	result = vm_map_lookup(&fs.map, vaddr, fault_type, &fs.entry,
	    &fs.first_object, &fs.first_pindex, &prot, &wired);
	if (result != KERN_SUCCESS) {
		if (growstack && result == KERN_INVALID_ADDRESS &&
		    map != kernel_map) {
			result = vm_map_growstack(curproc, vaddr);
			if (result != KERN_SUCCESS)
				return (KERN_FAILURE);
			growstack = FALSE;
			goto RetryFault;
		}
		return (result);
	}

	map_generation = fs.map->timestamp;

	if (fs.entry->eflags & MAP_ENTRY_NOFAULT) {
		panic("vm_fault: fault on nofault entry, addr: %lx",
		    (u_long)vaddr);
	}

	/*
	 * Make a reference to this object to prevent its disposal while we
	 * are messing with it.  Once we have the reference, the map is free
	 * to be diddled.  Since objects reference their shadows (and copies),
	 * they will stay around as well.
	 *
	 * Bump the paging-in-progress count to prevent size changes (e.g. 
	 * truncation operations) during I/O.  This must be done after
	 * obtaining the vnode lock in order to avoid possible deadlocks.
	 */
	VM_OBJECT_LOCK(fs.first_object);
	vm_object_reference_locked(fs.first_object);
	vm_object_pip_add(fs.first_object, 1);

	fs.lookup_still_valid = TRUE;

	if (wired)
		fault_type = prot;

	fs.first_m = NULL;

	/*
	 * Search for the page at object/offset.
	 */
	fs.object = fs.first_object;
	fs.pindex = fs.first_pindex;
	while (TRUE) {
		/*
		 * If the object is dead, we stop here
		 */
		if (fs.object->flags & OBJ_DEAD) {
			unlock_and_deallocate(&fs);
			return (KERN_PROTECTION_FAILURE);
		}

		/*
		 * See if page is resident
		 */
		fs.m = vm_page_lookup(fs.object, fs.pindex);
		if (fs.m != NULL) {
			/* 
			 * check for page-based copy on write.
			 * We check fs.object == fs.first_object so
			 * as to ensure the legacy COW mechanism is
			 * used when the page in question is part of
			 * a shadow object.  Otherwise, vm_page_cowfault()
			 * removes the page from the backing object, 
			 * which is not what we want.
			 */
			vm_page_lock_queues();
			if ((fs.m->cow) && 
			    (fault_type & VM_PROT_WRITE) &&
			    (fs.object == fs.first_object)) {
				vm_page_cowfault(fs.m);
				vm_page_unlock_queues();
				unlock_and_deallocate(&fs);
				goto RetryFault;
			}

			/*
			 * Wait/Retry if the page is busy.  We have to do this
			 * if the page is busy via either VPO_BUSY or 
			 * vm_page_t->busy because the vm_pager may be using
			 * vm_page_t->busy for pageouts ( and even pageins if
			 * it is the vnode pager ), and we could end up trying
			 * to pagein and pageout the same page simultaneously.
			 *
			 * We can theoretically allow the busy case on a read
			 * fault if the page is marked valid, but since such
			 * pages are typically already pmap'd, putting that
			 * special case in might be more effort then it is 
			 * worth.  We cannot under any circumstances mess
			 * around with a vm_page_t->busy page except, perhaps,
			 * to pmap it.
			 */
			if ((fs.m->oflags & VPO_BUSY) || fs.m->busy) {
				vm_page_unlock_queues();
				VM_OBJECT_UNLOCK(fs.object);
				if (fs.object != fs.first_object) {
					VM_OBJECT_LOCK(fs.first_object);
					vm_page_lock_queues();
					vm_page_free(fs.first_m);
					vm_page_unlock_queues();
					vm_object_pip_wakeup(fs.first_object);
					VM_OBJECT_UNLOCK(fs.first_object);
					fs.first_m = NULL;
				}
				unlock_map(&fs);
				VM_OBJECT_LOCK(fs.object);
				if (fs.m == vm_page_lookup(fs.object,
				    fs.pindex)) {
					vm_page_sleep_if_busy(fs.m, TRUE,
					    "vmpfw");
				}
				vm_object_pip_wakeup(fs.object);
				VM_OBJECT_UNLOCK(fs.object);
				PCPU_INC(cnt.v_intrans);
				vm_object_deallocate(fs.first_object);
				goto RetryFault;
			}
			vm_pageq_remove(fs.m);
			vm_page_unlock_queues();

			/*
			 * Mark page busy for other processes, and the 
			 * pagedaemon.  If it still isn't completely valid
			 * (readable), jump to readrest, else break-out ( we
			 * found the page ).
			 */
			vm_page_busy(fs.m);
			if (fs.m->valid != VM_PAGE_BITS_ALL &&
				fs.m->object != kernel_object && fs.m->object != kmem_object) {
				goto readrest;
			}

			break;
		}

		/*
		 * Page is not resident, If this is the search termination
		 * or the pager might contain the page, allocate a new page.
		 */
		if (TRYPAGER || fs.object == fs.first_object) {
			if (fs.pindex >= fs.object->size) {
				unlock_and_deallocate(&fs);
				return (KERN_PROTECTION_FAILURE);
			}

			/*
			 * Allocate a new page for this object/offset pair.
			 */
			fs.m = NULL;
			if (!vm_page_count_severe()) {
#if VM_NRESERVLEVEL > 0
				if ((fs.object->flags & OBJ_COLORED) == 0) {
					fs.object->flags |= OBJ_COLORED;
					fs.object->pg_color = atop(vaddr) -
					    fs.pindex;
				}
#endif
				fs.m = vm_page_alloc(fs.object, fs.pindex,
				    (fs.object->type == OBJT_VNODE ||
				     fs.object->backing_object != NULL) ?
				    VM_ALLOC_NORMAL : VM_ALLOC_ZERO);
			}
			if (fs.m == NULL) {
				unlock_and_deallocate(&fs);
				VM_WAITPFAULT;
				goto RetryFault;
			} else if (fs.m->valid == VM_PAGE_BITS_ALL)
				break;
		}

readrest:
		/*
		 * We have found a valid page or we have allocated a new page.
		 * The page thus may not be valid or may not be entirely 
		 * valid.
		 *
		 * Attempt to fault-in the page if there is a chance that the
		 * pager has it, and potentially fault in additional pages
		 * at the same time.
		 */
		if (TRYPAGER) {
			int rv;
			int reqpage = 0;
			u_char behavior = vm_map_entry_behavior(fs.entry);

			if (behavior == MAP_ENTRY_BEHAV_RANDOM) {
				ahead = 0;
				behind = 0;
			} else {
				behind = (vaddr - fs.entry->start) >> PAGE_SHIFT;
				if (behind > VM_FAULT_READ_BEHIND)
					behind = VM_FAULT_READ_BEHIND;

				ahead = ((fs.entry->end - vaddr) >> PAGE_SHIFT) - 1;
				if (ahead > VM_FAULT_READ_AHEAD)
					ahead = VM_FAULT_READ_AHEAD;
			}
			is_first_object_locked = FALSE;
			if ((behavior == MAP_ENTRY_BEHAV_SEQUENTIAL ||
			     (behavior != MAP_ENTRY_BEHAV_RANDOM &&
			      fs.pindex >= fs.entry->lastr &&
			      fs.pindex < fs.entry->lastr + VM_FAULT_READ)) &&
			    (fs.first_object == fs.object ||
			     (is_first_object_locked = VM_OBJECT_TRYLOCK(fs.first_object))) &&
			    fs.first_object->type != OBJT_DEVICE &&
			    fs.first_object->type != OBJT_PHYS &&
			    fs.first_object->type != OBJT_SG) {
				vm_pindex_t firstpindex, tmppindex;

				if (fs.first_pindex < 2 * VM_FAULT_READ)
					firstpindex = 0;
				else
					firstpindex = fs.first_pindex - 2 * VM_FAULT_READ;

				are_queues_locked = FALSE;
				/*
				 * note: partially valid pages cannot be 
				 * included in the lookahead - NFS piecemeal
				 * writes will barf on it badly.
				 */
				for (tmppindex = fs.first_pindex - 1;
					tmppindex >= firstpindex;
					--tmppindex) {
					vm_page_t mt;

					mt = vm_page_lookup(fs.first_object, tmppindex);
					if (mt == NULL || (mt->valid != VM_PAGE_BITS_ALL))
						break;
					if (mt->busy ||
					    (mt->oflags & VPO_BUSY))
						continue;
					if (!are_queues_locked) {
						are_queues_locked = TRUE;
						vm_page_lock_queues();
					}
					if (mt->hold_count ||
						mt->wire_count) 
						continue;
					pmap_remove_all(mt);
					if (mt->dirty) {
						vm_page_deactivate(mt);
					} else {
						vm_page_cache(mt);
					}
				}
				if (are_queues_locked)
					vm_page_unlock_queues();
				ahead += behind;
				behind = 0;
			}
			if (is_first_object_locked)
				VM_OBJECT_UNLOCK(fs.first_object);

			/*
			 * Call the pager to retrieve the data, if any, after
			 * releasing the lock on the map.  We hold a ref on
			 * fs.object and the pages are VPO_BUSY'd.
			 */
			unlock_map(&fs);

vnode_lock:
			if (fs.object->type == OBJT_VNODE) {
				vp = fs.object->handle;
				if (vp == fs.vp)
					goto vnode_locked;
				else if (fs.vp != NULL) {
					vput(fs.vp);
					fs.vp = NULL;
				}
				locked = VOP_ISLOCKED(vp);

				if (VFS_NEEDSGIANT(vp->v_mount) && !fs.vfslocked) {
					fs.vfslocked = 1;
					if (!mtx_trylock(&Giant)) {
						VM_OBJECT_UNLOCK(fs.object);
						mtx_lock(&Giant);
						VM_OBJECT_LOCK(fs.object);
						goto vnode_lock;
					}
				}
				if (locked != LK_EXCLUSIVE)
					locked = LK_SHARED;
				/* Do not sleep for vnode lock while fs.m is busy */
				error = vget(vp, locked | LK_CANRECURSE |
				    LK_NOWAIT, curthread);
				if (error != 0) {
					int vfslocked;

					vfslocked = fs.vfslocked;
					fs.vfslocked = 0; /* Keep Giant */
					vhold(vp);
					release_page(&fs);
					unlock_and_deallocate(&fs);
					error = vget(vp, locked | LK_RETRY |
					    LK_CANRECURSE, curthread);
					vdrop(vp);
					fs.vp = vp;
					fs.vfslocked = vfslocked;
					KASSERT(error == 0,
					    ("vm_fault: vget failed"));
					goto RetryFault;
				}
				fs.vp = vp;
			}
vnode_locked:
			KASSERT(fs.vp == NULL || !fs.map->system_map,
			    ("vm_fault: vnode-backed object mapped by system map"));

			/*
			 * now we find out if any other pages should be paged
			 * in at this time this routine checks to see if the
			 * pages surrounding this fault reside in the same
			 * object as the page for this fault.  If they do,
			 * then they are faulted in also into the object.  The
			 * array "marray" returned contains an array of
			 * vm_page_t structs where one of them is the
			 * vm_page_t passed to the routine.  The reqpage
			 * return value is the index into the marray for the
			 * vm_page_t passed to the routine.
			 *
			 * fs.m plus the additional pages are VPO_BUSY'd.
			 */
			faultcount = vm_fault_additional_pages(
			    fs.m, behind, ahead, marray, &reqpage);

			rv = faultcount ?
			    vm_pager_get_pages(fs.object, marray, faultcount,
				reqpage) : VM_PAGER_FAIL;

			if (rv == VM_PAGER_OK) {
				/*
				 * Found the page. Leave it busy while we play
				 * with it.
				 */

				/*
				 * Relookup in case pager changed page. Pager
				 * is responsible for disposition of old page
				 * if moved.
				 */
				fs.m = vm_page_lookup(fs.object, fs.pindex);
				if (!fs.m) {
					unlock_and_deallocate(&fs);
					goto RetryFault;
				}

				hardfault++;
				break; /* break to PAGE HAS BEEN FOUND */
			}
			/*
			 * Remove the bogus page (which does not exist at this
			 * object/offset); before doing so, we must get back
			 * our object lock to preserve our invariant.
			 *
			 * Also wake up any other process that may want to bring
			 * in this page.
			 *
			 * If this is the top-level object, we must leave the
			 * busy page to prevent another process from rushing
			 * past us, and inserting the page in that object at
			 * the same time that we are.
			 */
			if (rv == VM_PAGER_ERROR)
				printf("vm_fault: pager read error, pid %d (%s)\n",
				    curproc->p_pid, curproc->p_comm);
			/*
			 * Data outside the range of the pager or an I/O error
			 */
			/*
			 * XXX - the check for kernel_map is a kludge to work
			 * around having the machine panic on a kernel space
			 * fault w/ I/O error.
			 */
			if (((fs.map != kernel_map) && (rv == VM_PAGER_ERROR)) ||
				(rv == VM_PAGER_BAD)) {
				vm_page_lock_queues();
				vm_page_free(fs.m);
				vm_page_unlock_queues();
				fs.m = NULL;
				unlock_and_deallocate(&fs);
				return ((rv == VM_PAGER_ERROR) ? KERN_FAILURE : KERN_PROTECTION_FAILURE);
			}
			if (fs.object != fs.first_object) {
				vm_page_lock_queues();
				vm_page_free(fs.m);
				vm_page_unlock_queues();
				fs.m = NULL;
				/*
				 * XXX - we cannot just fall out at this
				 * point, m has been freed and is invalid!
				 */
			}
		}

		/*
		 * We get here if the object has default pager (or unwiring) 
		 * or the pager doesn't have the page.
		 */
		if (fs.object == fs.first_object)
			fs.first_m = fs.m;

		/*
		 * Move on to the next object.  Lock the next object before
		 * unlocking the current one.
		 */
		fs.pindex += OFF_TO_IDX(fs.object->backing_object_offset);
		next_object = fs.object->backing_object;
		if (next_object == NULL) {
			/*
			 * If there's no object left, fill the page in the top
			 * object with zeros.
			 */
			if (fs.object != fs.first_object) {
				vm_object_pip_wakeup(fs.object);
				VM_OBJECT_UNLOCK(fs.object);

				fs.object = fs.first_object;
				fs.pindex = fs.first_pindex;
				fs.m = fs.first_m;
				VM_OBJECT_LOCK(fs.object);
			}
			fs.first_m = NULL;

			/*
			 * Zero the page if necessary and mark it valid.
			 */
			if ((fs.m->flags & PG_ZERO) == 0) {
				pmap_zero_page(fs.m);
			} else {
				PCPU_INC(cnt.v_ozfod);
			}
			PCPU_INC(cnt.v_zfod);
			fs.m->valid = VM_PAGE_BITS_ALL;
			break;	/* break to PAGE HAS BEEN FOUND */
		} else {
			KASSERT(fs.object != next_object,
			    ("object loop %p", next_object));
			VM_OBJECT_LOCK(next_object);
			vm_object_pip_add(next_object, 1);
			if (fs.object != fs.first_object)
				vm_object_pip_wakeup(fs.object);
			VM_OBJECT_UNLOCK(fs.object);
			fs.object = next_object;
		}
	}

	KASSERT((fs.m->oflags & VPO_BUSY) != 0,
	    ("vm_fault: not busy after main loop"));

	/*
	 * PAGE HAS BEEN FOUND. [Loop invariant still holds -- the object lock
	 * is held.]
	 */

	/*
	 * If the page is being written, but isn't already owned by the
	 * top-level object, we have to copy it into a new page owned by the
	 * top-level object.
	 */
	if (fs.object != fs.first_object) {
		/*
		 * We only really need to copy if we want to write it.
		 */
		if ((fault_type & (VM_PROT_COPY | VM_PROT_WRITE)) != 0) {
			/*
			 * This allows pages to be virtually copied from a 
			 * backing_object into the first_object, where the 
			 * backing object has no other refs to it, and cannot
			 * gain any more refs.  Instead of a bcopy, we just 
			 * move the page from the backing object to the 
			 * first object.  Note that we must mark the page 
			 * dirty in the first object so that it will go out 
			 * to swap when needed.
			 */
			is_first_object_locked = FALSE;
			if (
				/*
				 * Only one shadow object
				 */
				(fs.object->shadow_count == 1) &&
				/*
				 * No COW refs, except us
				 */
				(fs.object->ref_count == 1) &&
				/*
				 * No one else can look this object up
				 */
				(fs.object->handle == NULL) &&
				/*
				 * No other ways to look the object up
				 */
				((fs.object->type == OBJT_DEFAULT) ||
				 (fs.object->type == OBJT_SWAP)) &&
			    (is_first_object_locked = VM_OBJECT_TRYLOCK(fs.first_object)) &&
				/*
				 * We don't chase down the shadow chain
				 */
			    fs.object == fs.first_object->backing_object) {
				vm_page_lock_queues();
				/*
				 * get rid of the unnecessary page
				 */
				vm_page_free(fs.first_m);
				/*
				 * grab the page and put it into the 
				 * process'es object.  The page is 
				 * automatically made dirty.
				 */
				vm_page_rename(fs.m, fs.first_object, fs.first_pindex);
				vm_page_unlock_queues();
				vm_page_busy(fs.m);
				fs.first_m = fs.m;
				fs.m = NULL;
				PCPU_INC(cnt.v_cow_optim);
			} else {
				/*
				 * Oh, well, lets copy it.
				 */
				pmap_copy_page(fs.m, fs.first_m);
				fs.first_m->valid = VM_PAGE_BITS_ALL;
			}
			if (fs.m) {
				/*
				 * We no longer need the old page or object.
				 */
				release_page(&fs);
			}
			/*
			 * fs.object != fs.first_object due to above 
			 * conditional
			 */
			vm_object_pip_wakeup(fs.object);
			VM_OBJECT_UNLOCK(fs.object);
			/*
			 * Only use the new page below...
			 */
			fs.object = fs.first_object;
			fs.pindex = fs.first_pindex;
			fs.m = fs.first_m;
			if (!is_first_object_locked)
				VM_OBJECT_LOCK(fs.object);
			PCPU_INC(cnt.v_cow_faults);
		} else {
			prot &= ~VM_PROT_WRITE;
		}
	}

	/*
	 * We must verify that the maps have not changed since our last
	 * lookup.
	 */
	if (!fs.lookup_still_valid) {
		vm_object_t retry_object;
		vm_pindex_t retry_pindex;
		vm_prot_t retry_prot;

		if (!vm_map_trylock_read(fs.map)) {
			release_page(&fs);
			unlock_and_deallocate(&fs);
			goto RetryFault;
		}
		fs.lookup_still_valid = TRUE;
		if (fs.map->timestamp != map_generation) {
			result = vm_map_lookup_locked(&fs.map, vaddr, fault_type,
			    &fs.entry, &retry_object, &retry_pindex, &retry_prot, &wired);

			/*
			 * If we don't need the page any longer, put it on the inactive
			 * list (the easiest thing to do here).  If no one needs it,
			 * pageout will grab it eventually.
			 */
			if (result != KERN_SUCCESS) {
				release_page(&fs);
				unlock_and_deallocate(&fs);

				/*
				 * If retry of map lookup would have blocked then
				 * retry fault from start.
				 */
				if (result == KERN_FAILURE)
					goto RetryFault;
				return (result);
			}
			if ((retry_object != fs.first_object) ||
			    (retry_pindex != fs.first_pindex)) {
				release_page(&fs);
				unlock_and_deallocate(&fs);
				goto RetryFault;
			}

			/*
			 * Check whether the protection has changed or the object has
			 * been copied while we left the map unlocked. Changing from
			 * read to write permission is OK - we leave the page
			 * write-protected, and catch the write fault. Changing from
			 * write to read permission means that we can't mark the page
			 * write-enabled after all.
			 */
			prot &= retry_prot;
		}
	}
	/*
	 * If the page was filled by a pager, update the map entry's
	 * last read offset.  Since the pager does not return the
	 * actual set of pages that it read, this update is based on
	 * the requested set.  Typically, the requested and actual
	 * sets are the same.
	 *
	 * XXX The following assignment modifies the map
	 * without holding a write lock on it.
	 */
	if (hardfault)
		fs.entry->lastr = fs.pindex + faultcount - behind;

	if (prot & VM_PROT_WRITE) {
		vm_object_set_writeable_dirty(fs.object);

		/*
		 * If the fault is a write, we know that this page is being
		 * written NOW so dirty it explicitly to save on 
		 * pmap_is_modified() calls later.
		 *
		 * If this is a NOSYNC mmap we do not want to set VPO_NOSYNC
		 * if the page is already dirty to prevent data written with
		 * the expectation of being synced from not being synced.
		 * Likewise if this entry does not request NOSYNC then make
		 * sure the page isn't marked NOSYNC.  Applications sharing
		 * data should use the same flags to avoid ping ponging.
		 *
		 * Also tell the backing pager, if any, that it should remove
		 * any swap backing since the page is now dirty.
		 */
		if (fs.entry->eflags & MAP_ENTRY_NOSYNC) {
			if (fs.m->dirty == 0)
				fs.m->oflags |= VPO_NOSYNC;
		} else {
			fs.m->oflags &= ~VPO_NOSYNC;
		}
		if (fault_flags & VM_FAULT_DIRTY) {
			vm_page_dirty(fs.m);
			vm_pager_page_unswapped(fs.m);
		}
	}

	/*
	 * Page had better still be busy
	 */
	KASSERT(fs.m->oflags & VPO_BUSY,
		("vm_fault: page %p not busy!", fs.m));
	/*
	 * Page must be completely valid or it is not fit to
	 * map into user space.  vm_pager_get_pages() ensures this.
	 */
	KASSERT(fs.m->valid == VM_PAGE_BITS_ALL,
	    ("vm_fault: page %p partially invalid", fs.m));
	VM_OBJECT_UNLOCK(fs.object);

	/*
	 * Put this page into the physical map.  We had to do the unlock above
	 * because pmap_enter() may sleep.  We don't put the page
	 * back on the active queue until later so that the pageout daemon
	 * won't find it (yet).
	 */
	pmap_enter(fs.map->pmap, vaddr, fault_type, fs.m, prot, wired);
	if ((fault_flags & VM_FAULT_CHANGE_WIRING) == 0 && wired == 0)
		vm_fault_prefault(fs.map->pmap, vaddr, fs.entry);
	VM_OBJECT_LOCK(fs.object);
	vm_page_lock_queues();
	vm_page_flag_set(fs.m, PG_REFERENCED);

	/*
	 * If the page is not wired down, then put it where the pageout daemon
	 * can find it.
	 */
	if (fault_flags & VM_FAULT_CHANGE_WIRING) {
		if (wired)
			vm_page_wire(fs.m);
		else
			vm_page_unwire(fs.m, 1);
	} else {
		vm_page_activate(fs.m);
	}
	vm_page_unlock_queues();
	vm_page_wakeup(fs.m);

	/*
	 * Unlock everything, and return
	 */
	unlock_and_deallocate(&fs);
	if (hardfault)
		curthread->td_ru.ru_majflt++;
	else
		curthread->td_ru.ru_minflt++;

	return (KERN_SUCCESS);
}

/*
 * vm_fault_prefault provides a quick way of clustering
 * pagefaults into a processes address space.  It is a "cousin"
 * of vm_map_pmap_enter, except it runs at page fault time instead
 * of mmap time.
 */
static void
vm_fault_prefault(pmap_t pmap, vm_offset_t addra, vm_map_entry_t entry)
{
	int i;
	vm_offset_t addr, starta;
	vm_pindex_t pindex;
	vm_page_t m;
	vm_object_t object;

	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace))
		return;

	object = entry->object.vm_object;

	starta = addra - PFBAK * PAGE_SIZE;
	if (starta < entry->start) {
		starta = entry->start;
	} else if (starta > addra) {
		starta = 0;
	}

	for (i = 0; i < PAGEORDER_SIZE; i++) {
		vm_object_t backing_object, lobject;

		addr = addra + prefault_pageorder[i];
		if (addr > addra + (PFFOR * PAGE_SIZE))
			addr = 0;

		if (addr < starta || addr >= entry->end)
			continue;

		if (!pmap_is_prefaultable(pmap, addr))
			continue;

		pindex = ((addr - entry->start) + entry->offset) >> PAGE_SHIFT;
		lobject = object;
		VM_OBJECT_LOCK(lobject);
		while ((m = vm_page_lookup(lobject, pindex)) == NULL &&
		    lobject->type == OBJT_DEFAULT &&
		    (backing_object = lobject->backing_object) != NULL) {
			KASSERT((lobject->backing_object_offset & PAGE_MASK) ==
			    0, ("vm_fault_prefault: unaligned object offset"));
			pindex += lobject->backing_object_offset >> PAGE_SHIFT;
			VM_OBJECT_LOCK(backing_object);
			VM_OBJECT_UNLOCK(lobject);
			lobject = backing_object;
		}
		/*
		 * give-up when a page is not in memory
		 */
		if (m == NULL) {
			VM_OBJECT_UNLOCK(lobject);
			break;
		}
		if (m->valid == VM_PAGE_BITS_ALL &&
		    (m->flags & PG_FICTITIOUS) == 0) {
			vm_page_lock_queues();
			pmap_enter_quick(pmap, addr, m, entry->protection);
			vm_page_unlock_queues();
		}
		VM_OBJECT_UNLOCK(lobject);
	}
}

/*
 *	vm_fault_quick:
 *
 *	Ensure that the requested virtual address, which may be in userland,
 *	is valid.  Fault-in the page if necessary.  Return -1 on failure.
 */
int
vm_fault_quick(caddr_t v, int prot)
{
	int r;

	if (prot & VM_PROT_WRITE)
		r = subyte(v, fubyte(v));
	else
		r = fubyte(v);
	return(r);
}

/*
 *	vm_fault_wire:
 *
 *	Wire down a range of virtual addresses in a map.
 */
int
vm_fault_wire(vm_map_t map, vm_offset_t start, vm_offset_t end,
    boolean_t fictitious)
{
	vm_offset_t va;
	int rv;

	/*
	 * We simulate a fault to get the page and enter it in the physical
	 * map.  For user wiring, we only ask for read access on currently
	 * read-only sections.
	 */
	for (va = start; va < end; va += PAGE_SIZE) {
		rv = vm_fault(map, va, VM_PROT_NONE, VM_FAULT_CHANGE_WIRING);
		if (rv) {
			if (va != start)
				vm_fault_unwire(map, start, va, fictitious);
			return (rv);
		}
	}
	return (KERN_SUCCESS);
}

/*
 *	vm_fault_unwire:
 *
 *	Unwire a range of virtual addresses in a map.
 */
void
vm_fault_unwire(vm_map_t map, vm_offset_t start, vm_offset_t end,
    boolean_t fictitious)
{
	vm_paddr_t pa;
	vm_offset_t va;
	pmap_t pmap;

	pmap = vm_map_pmap(map);

	/*
	 * Since the pages are wired down, we must be able to get their
	 * mappings from the physical map system.
	 */
	for (va = start; va < end; va += PAGE_SIZE) {
		pa = pmap_extract(pmap, va);
		if (pa != 0) {
			pmap_change_wiring(pmap, va, FALSE);
			if (!fictitious) {
				vm_page_lock_queues();
				vm_page_unwire(PHYS_TO_VM_PAGE(pa), 1);
				vm_page_unlock_queues();
			}
		}
	}
}

/*
 *	Routine:
 *		vm_fault_copy_entry
 *	Function:
 *		Create new shadow object backing dst_entry with private copy of
 *		all underlying pages. When src_entry is equal to dst_entry,
 *		function implements COW for wired-down map entry. Otherwise,
 *		it forks wired entry into dst_map.
 *
 *	In/out conditions:
 *		The source and destination maps must be locked for write.
 *		The source map entry must be wired down (or be a sharing map
 *		entry corresponding to a main map entry that is wired down).
 */
void
vm_fault_copy_entry(vm_map_t dst_map, vm_map_t src_map,
    vm_map_entry_t dst_entry, vm_map_entry_t src_entry,
    vm_ooffset_t *fork_charge)
{
	vm_object_t backing_object, dst_object, object, src_object;
	vm_pindex_t dst_pindex, pindex, src_pindex;
	vm_prot_t access, prot;
	vm_offset_t vaddr;
	vm_page_t dst_m;
	vm_page_t src_m;
	boolean_t src_readonly, upgrade;

#ifdef	lint
	src_map++;
#endif	/* lint */

	upgrade = src_entry == dst_entry;

	src_object = src_entry->object.vm_object;
	src_pindex = OFF_TO_IDX(src_entry->offset);
	src_readonly = (src_entry->protection & VM_PROT_WRITE) == 0;

	/*
	 * Create the top-level object for the destination entry. (Doesn't
	 * actually shadow anything - we copy the pages directly.)
	 */
	dst_object = vm_object_allocate(OBJT_DEFAULT,
	    OFF_TO_IDX(dst_entry->end - dst_entry->start));
#if VM_NRESERVLEVEL > 0
	dst_object->flags |= OBJ_COLORED;
	dst_object->pg_color = atop(dst_entry->start);
#endif

	VM_OBJECT_LOCK(dst_object);
	KASSERT(upgrade || dst_entry->object.vm_object == NULL,
	    ("vm_fault_copy_entry: vm_object not NULL"));
	dst_entry->object.vm_object = dst_object;
	dst_entry->offset = 0;
	dst_object->charge = dst_entry->end - dst_entry->start;
	if (fork_charge != NULL) {
		KASSERT(dst_entry->uip == NULL,
		    ("vm_fault_copy_entry: leaked swp charge"));
		dst_object->uip = curthread->td_ucred->cr_ruidinfo;
		uihold(dst_object->uip);
		*fork_charge += dst_object->charge;
	} else {
		dst_object->uip = dst_entry->uip;
		dst_entry->uip = NULL;
	}
	access = prot = dst_entry->protection;
	/*
	 * If not an upgrade, then enter the mappings in the pmap as
	 * read and/or execute accesses.  Otherwise, enter them as
	 * write accesses.
	 *
	 * A writeable large page mapping is only created if all of
	 * the constituent small page mappings are modified. Marking
	 * PTEs as modified on inception allows promotion to happen
	 * without taking potentially large number of soft faults.
	 */
	if (!upgrade)
		access &= ~VM_PROT_WRITE;

	/*
	 * Loop through all of the pages in the entry's range, copying each
	 * one from the source object (it should be there) to the destination
	 * object.
	 */
	for (vaddr = dst_entry->start, dst_pindex = 0;
	    vaddr < dst_entry->end;
	    vaddr += PAGE_SIZE, dst_pindex++) {

		/*
		 * Allocate a page in the destination object.
		 */
		do {
			dst_m = vm_page_alloc(dst_object, dst_pindex,
			    VM_ALLOC_NORMAL);
			if (dst_m == NULL) {
				VM_OBJECT_UNLOCK(dst_object);
				VM_WAIT;
				VM_OBJECT_LOCK(dst_object);
			}
		} while (dst_m == NULL);

		/*
		 * Find the page in the source object, and copy it in.
		 * (Because the source is wired down, the page will be in
		 * memory.)
		 */
		VM_OBJECT_LOCK(src_object);
		object = src_object;
		pindex = src_pindex + dst_pindex;
		while ((src_m = vm_page_lookup(object, pindex)) == NULL &&
		    src_readonly &&
		    (backing_object = object->backing_object) != NULL) {
			/*
			 * Allow fallback to backing objects if we are reading.
			 */
			VM_OBJECT_LOCK(backing_object);
			pindex += OFF_TO_IDX(object->backing_object_offset);
			VM_OBJECT_UNLOCK(object);
			object = backing_object;
		}
		if (src_m == NULL)
			panic("vm_fault_copy_wired: page missing");
		pmap_copy_page(src_m, dst_m);
		VM_OBJECT_UNLOCK(object);
		dst_m->valid = VM_PAGE_BITS_ALL;
		VM_OBJECT_UNLOCK(dst_object);

		/*
		 * Enter it in the pmap. If a wired, copy-on-write
		 * mapping is being replaced by a write-enabled
		 * mapping, then wire that new mapping.
		 */
		pmap_enter(dst_map->pmap, vaddr, access, dst_m, prot, upgrade);

		/*
		 * Mark it no longer busy, and put it on the active list.
		 */
		VM_OBJECT_LOCK(dst_object);
		vm_page_lock_queues();
		if (upgrade) {
			vm_page_unwire(src_m, 0);
			vm_page_wire(dst_m);
		} else
			vm_page_activate(dst_m);
		vm_page_unlock_queues();
		vm_page_wakeup(dst_m);
	}
	VM_OBJECT_UNLOCK(dst_object);
	if (upgrade) {
		dst_entry->eflags &= ~(MAP_ENTRY_COW | MAP_ENTRY_NEEDS_COPY);
		vm_object_deallocate(src_object);
	}
}


/*
 * This routine checks around the requested page for other pages that
 * might be able to be faulted in.  This routine brackets the viable
 * pages for the pages to be paged in.
 *
 * Inputs:
 *	m, rbehind, rahead
 *
 * Outputs:
 *  marray (array of vm_page_t), reqpage (index of requested page)
 *
 * Return value:
 *  number of pages in marray
 */
static int
vm_fault_additional_pages(m, rbehind, rahead, marray, reqpage)
	vm_page_t m;
	int rbehind;
	int rahead;
	vm_page_t *marray;
	int *reqpage;
{
	int i,j;
	vm_object_t object;
	vm_pindex_t pindex, startpindex, endpindex, tpindex;
	vm_page_t rtm;
	int cbehind, cahead;

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);

	object = m->object;
	pindex = m->pindex;
	cbehind = cahead = 0;

	/*
	 * if the requested page is not available, then give up now
	 */
	if (!vm_pager_has_page(object, pindex, &cbehind, &cahead)) {
		return 0;
	}

	if ((cbehind == 0) && (cahead == 0)) {
		*reqpage = 0;
		marray[0] = m;
		return 1;
	}

	if (rahead > cahead) {
		rahead = cahead;
	}

	if (rbehind > cbehind) {
		rbehind = cbehind;
	}

	/*
	 * scan backward for the read behind pages -- in memory 
	 */
	if (pindex > 0) {
		if (rbehind > pindex) {
			rbehind = pindex;
			startpindex = 0;
		} else {
			startpindex = pindex - rbehind;
		}

		if ((rtm = TAILQ_PREV(m, pglist, listq)) != NULL &&
		    rtm->pindex >= startpindex)
			startpindex = rtm->pindex + 1;

		/* tpindex is unsigned; beware of numeric underflow. */
		for (i = 0, tpindex = pindex - 1; tpindex >= startpindex &&
		    tpindex < pindex; i++, tpindex--) {

			rtm = vm_page_alloc(object, tpindex, VM_ALLOC_NORMAL |
			    VM_ALLOC_IFNOTCACHED);
			if (rtm == NULL) {
				/*
				 * Shift the allocated pages to the
				 * beginning of the array.
				 */
				for (j = 0; j < i; j++) {
					marray[j] = marray[j + tpindex + 1 -
					    startpindex];
				}
				break;
			}

			marray[tpindex - startpindex] = rtm;
		}
	} else {
		startpindex = 0;
		i = 0;
	}

	marray[i] = m;
	/* page offset of the required page */
	*reqpage = i;

	tpindex = pindex + 1;
	i++;

	/*
	 * scan forward for the read ahead pages
	 */
	endpindex = tpindex + rahead;
	if ((rtm = TAILQ_NEXT(m, listq)) != NULL && rtm->pindex < endpindex)
		endpindex = rtm->pindex;
	if (endpindex > object->size)
		endpindex = object->size;

	for (; tpindex < endpindex; i++, tpindex++) {

		rtm = vm_page_alloc(object, tpindex, VM_ALLOC_NORMAL |
		    VM_ALLOC_IFNOTCACHED);
		if (rtm == NULL) {
			break;
		}

		marray[i] = rtm;
	}

	/* return number of pages */
	return i;
}
