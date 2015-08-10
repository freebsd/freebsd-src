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

#include "opt_ktrace.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

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
#include <vm/vm_reserv.h>

#define PFBAK 4
#define PFFOR 4

static int vm_fault_additional_pages(vm_page_t, int, int, vm_page_t *, int *);

#define	VM_FAULT_READ_BEHIND	8
#define	VM_FAULT_READ_DEFAULT	(1 + VM_FAULT_READ_AHEAD_INIT)
#define	VM_FAULT_READ_MAX	(1 + VM_FAULT_READ_AHEAD_MAX)
#define	VM_FAULT_NINCR		(VM_FAULT_READ_MAX / VM_FAULT_READ_BEHIND)
#define	VM_FAULT_SUM		(VM_FAULT_NINCR * (VM_FAULT_NINCR + 1) / 2)

#define	VM_FAULT_DONTNEED_MIN	1048576

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
};

static void vm_fault_dontneed(const struct faultstate *fs, vm_offset_t vaddr,
	    int ahead);
static void vm_fault_prefault(const struct faultstate *fs, vm_offset_t addra,
	    int faultcount, int reqpage);

static inline void
release_page(struct faultstate *fs)
{

	vm_page_xunbusy(fs->m);
	vm_page_lock(fs->m);
	vm_page_deactivate(fs->m);
	vm_page_unlock(fs->m);
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
	VM_OBJECT_WUNLOCK(fs->object);
	if (fs->object != fs->first_object) {
		VM_OBJECT_WLOCK(fs->first_object);
		vm_page_lock(fs->first_m);
		vm_page_free(fs->first_m);
		vm_page_unlock(fs->first_m);
		vm_object_pip_wakeup(fs->first_object);
		VM_OBJECT_WUNLOCK(fs->first_object);
		fs->first_m = NULL;
	}
	vm_object_deallocate(fs->first_object);
	unlock_map(fs);	
	if (fs->vp != NULL) { 
		vput(fs->vp);
		fs->vp = NULL;
	}
}

static void
vm_fault_dirty(vm_map_entry_t entry, vm_page_t m, vm_prot_t prot,
    vm_prot_t fault_type, int fault_flags, boolean_t set_wd)
{
	boolean_t need_dirty;

	if (((prot & VM_PROT_WRITE) == 0 &&
	    (fault_flags & VM_FAULT_DIRTY) == 0) ||
	    (m->oflags & VPO_UNMANAGED) != 0)
		return;

	VM_OBJECT_ASSERT_LOCKED(m->object);

	need_dirty = ((fault_type & VM_PROT_WRITE) != 0 &&
	    (fault_flags & VM_FAULT_WIRE) == 0) ||
	    (fault_flags & VM_FAULT_DIRTY) != 0;

	if (set_wd)
		vm_object_set_writeable_dirty(m->object);
	else
		/*
		 * If two callers of vm_fault_dirty() with set_wd ==
		 * FALSE, one for the map entry with MAP_ENTRY_NOSYNC
		 * flag set, other with flag clear, race, it is
		 * possible for the no-NOSYNC thread to see m->dirty
		 * != 0 and not clear VPO_NOSYNC.  Take vm_page lock
		 * around manipulation of VPO_NOSYNC and
		 * vm_page_dirty() call, to avoid the race and keep
		 * m->oflags consistent.
		 */
		vm_page_lock(m);

	/*
	 * If this is a NOSYNC mmap we do not want to set VPO_NOSYNC
	 * if the page is already dirty to prevent data written with
	 * the expectation of being synced from not being synced.
	 * Likewise if this entry does not request NOSYNC then make
	 * sure the page isn't marked NOSYNC.  Applications sharing
	 * data should use the same flags to avoid ping ponging.
	 */
	if ((entry->eflags & MAP_ENTRY_NOSYNC) != 0) {
		if (m->dirty == 0) {
			m->oflags |= VPO_NOSYNC;
		}
	} else {
		m->oflags &= ~VPO_NOSYNC;
	}

	/*
	 * If the fault is a write, we know that this page is being
	 * written NOW so dirty it explicitly to save on
	 * pmap_is_modified() calls later.
	 *
	 * Also tell the backing pager, if any, that it should remove
	 * any swap backing since the page is now dirty.
	 */
	if (need_dirty)
		vm_page_dirty(m);
	if (!set_wd)
		vm_page_unlock(m);
	if (need_dirty)
		vm_pager_page_unswapped(m);
}

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
 *	The map in question must be referenced, and remains so.
 *	Caller may hold no locks.
 */
int
vm_fault(vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
    int fault_flags)
{
	struct thread *td;
	int result;

	td = curthread;
	if ((td->td_pflags & TDP_NOFAULTING) != 0)
		return (KERN_PROTECTION_FAILURE);
#ifdef KTRACE
	if (map != kernel_map && KTRPOINT(td, KTR_FAULT))
		ktrfault(vaddr, fault_type);
#endif
	result = vm_fault_hold(map, trunc_page(vaddr), fault_type, fault_flags,
	    NULL);
#ifdef KTRACE
	if (map != kernel_map && KTRPOINT(td, KTR_FAULTEND))
		ktrfaultend(result);
#endif
	return (result);
}

int
vm_fault_hold(vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
    int fault_flags, vm_page_t *m_hold)
{
	vm_prot_t prot;
	int alloc_req, era, faultcount, nera, reqpage, result;
	boolean_t growstack, is_first_object_locked, wired;
	int map_generation;
	vm_object_t next_object;
	vm_page_t marray[VM_FAULT_READ_MAX];
	int hardfault;
	struct faultstate fs;
	struct vnode *vp;
	vm_page_t m;
	int ahead, behind, cluster_offset, error, locked;

	hardfault = 0;
	growstack = TRUE;
	PCPU_INC(cnt.v_vm_faults);
	fs.vp = NULL;
	faultcount = reqpage = 0;

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

	if (fs.entry->eflags & MAP_ENTRY_IN_TRANSITION &&
	    fs.entry->wiring_thread != curthread) {
		vm_map_unlock_read(fs.map);
		vm_map_lock(fs.map);
		if (vm_map_lookup_entry(fs.map, vaddr, &fs.entry) &&
		    (fs.entry->eflags & MAP_ENTRY_IN_TRANSITION)) {
			if (fs.vp != NULL) {
				vput(fs.vp);
				fs.vp = NULL;
			}
			fs.entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			vm_map_unlock_and_wait(fs.map, 0);
		} else
			vm_map_unlock(fs.map);
		goto RetryFault;
	}

	if (wired)
		fault_type = prot | (fault_type & VM_PROT_COPY);
	else
		KASSERT((fault_flags & VM_FAULT_WIRE) == 0,
		    ("!wired && VM_FAULT_WIRE"));

	if (fs.vp == NULL /* avoid locked vnode leak */ &&
	    (fault_flags & (VM_FAULT_WIRE | VM_FAULT_DIRTY)) == 0 &&
	    /* avoid calling vm_object_set_writeable_dirty() */
	    ((prot & VM_PROT_WRITE) == 0 ||
	    (fs.first_object->type != OBJT_VNODE &&
	    (fs.first_object->flags & OBJ_TMPFS_NODE) == 0) ||
	    (fs.first_object->flags & OBJ_MIGHTBEDIRTY) != 0)) {
		VM_OBJECT_RLOCK(fs.first_object);
		if ((prot & VM_PROT_WRITE) != 0 &&
		    (fs.first_object->type == OBJT_VNODE ||
		    (fs.first_object->flags & OBJ_TMPFS_NODE) != 0) &&
		    (fs.first_object->flags & OBJ_MIGHTBEDIRTY) == 0)
			goto fast_failed;
		m = vm_page_lookup(fs.first_object, fs.first_pindex);
		/* A busy page can be mapped for read|execute access. */
		if (m == NULL || ((prot & VM_PROT_WRITE) != 0 &&
		    vm_page_busied(m)) || m->valid != VM_PAGE_BITS_ALL)
			goto fast_failed;
		result = pmap_enter(fs.map->pmap, vaddr, m, prot,
		   fault_type | PMAP_ENTER_NOSLEEP | (wired ? PMAP_ENTER_WIRED :
		   0), 0);
		if (result != KERN_SUCCESS)
			goto fast_failed;
		if (m_hold != NULL) {
			*m_hold = m;
			vm_page_lock(m);
			vm_page_hold(m);
			vm_page_unlock(m);
		}
		vm_fault_dirty(fs.entry, m, prot, fault_type, fault_flags,
		    FALSE);
		VM_OBJECT_RUNLOCK(fs.first_object);
		if (!wired)
			vm_fault_prefault(&fs, vaddr, 0, 0);
		vm_map_lookup_done(fs.map, fs.entry);
		curthread->td_ru.ru_minflt++;
		return (KERN_SUCCESS);
fast_failed:
		if (!VM_OBJECT_TRYUPGRADE(fs.first_object)) {
			VM_OBJECT_RUNLOCK(fs.first_object);
			VM_OBJECT_WLOCK(fs.first_object);
		}
	} else {
		VM_OBJECT_WLOCK(fs.first_object);
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
	vm_object_reference_locked(fs.first_object);
	vm_object_pip_add(fs.first_object, 1);

	fs.lookup_still_valid = TRUE;

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
			 * Wait/Retry if the page is busy.  We have to do this
			 * if the page is either exclusive or shared busy
			 * because the vm_pager may be using read busy for
			 * pageouts (and even pageins if it is the vnode
			 * pager), and we could end up trying to pagein and
			 * pageout the same page simultaneously.
			 *
			 * We can theoretically allow the busy case on a read
			 * fault if the page is marked valid, but since such
			 * pages are typically already pmap'd, putting that
			 * special case in might be more effort then it is 
			 * worth.  We cannot under any circumstances mess
			 * around with a shared busied page except, perhaps,
			 * to pmap it.
			 */
			if (vm_page_busied(fs.m)) {
				/*
				 * Reference the page before unlocking and
				 * sleeping so that the page daemon is less
				 * likely to reclaim it. 
				 */
				vm_page_aflag_set(fs.m, PGA_REFERENCED);
				if (fs.object != fs.first_object) {
					if (!VM_OBJECT_TRYWLOCK(
					    fs.first_object)) {
						VM_OBJECT_WUNLOCK(fs.object);
						VM_OBJECT_WLOCK(fs.first_object);
						VM_OBJECT_WLOCK(fs.object);
					}
					vm_page_lock(fs.first_m);
					vm_page_free(fs.first_m);
					vm_page_unlock(fs.first_m);
					vm_object_pip_wakeup(fs.first_object);
					VM_OBJECT_WUNLOCK(fs.first_object);
					fs.first_m = NULL;
				}
				unlock_map(&fs);
				if (fs.m == vm_page_lookup(fs.object,
				    fs.pindex)) {
					vm_page_sleep_if_busy(fs.m, "vmpfw");
				}
				vm_object_pip_wakeup(fs.object);
				VM_OBJECT_WUNLOCK(fs.object);
				PCPU_INC(cnt.v_intrans);
				vm_object_deallocate(fs.first_object);
				goto RetryFault;
			}
			vm_page_lock(fs.m);
			vm_page_remque(fs.m);
			vm_page_unlock(fs.m);

			/*
			 * Mark page busy for other processes, and the 
			 * pagedaemon.  If it still isn't completely valid
			 * (readable), jump to readrest, else break-out ( we
			 * found the page ).
			 */
			vm_page_xbusy(fs.m);
			if (fs.m->valid != VM_PAGE_BITS_ALL)
				goto readrest;
			break;
		}

		/*
		 * Page is not resident.  If this is the search termination
		 * or the pager might contain the page, allocate a new page.
		 * Default objects are zero-fill, there is no real pager.
		 */
		if (fs.object->type != OBJT_DEFAULT ||
		    fs.object == fs.first_object) {
			if (fs.pindex >= fs.object->size) {
				unlock_and_deallocate(&fs);
				return (KERN_PROTECTION_FAILURE);
			}

			/*
			 * Allocate a new page for this object/offset pair.
			 *
			 * Unlocked read of the p_flag is harmless. At
			 * worst, the P_KILLED might be not observed
			 * there, and allocation can fail, causing
			 * restart and new reading of the p_flag.
			 */
			fs.m = NULL;
			if (!vm_page_count_severe() || P_KILLED(curproc)) {
#if VM_NRESERVLEVEL > 0
				vm_object_color(fs.object, atop(vaddr) -
				    fs.pindex);
#endif
				alloc_req = P_KILLED(curproc) ?
				    VM_ALLOC_SYSTEM : VM_ALLOC_NORMAL;
				if (fs.object->type != OBJT_VNODE &&
				    fs.object->backing_object == NULL)
					alloc_req |= VM_ALLOC_ZERO;
				fs.m = vm_page_alloc(fs.object, fs.pindex,
				    alloc_req);
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
		 * at the same time.  For default objects simply provide
		 * zero-filled pages.
		 */
		if (fs.object->type != OBJT_DEFAULT) {
			int rv;
			u_char behavior = vm_map_entry_behavior(fs.entry);

			era = fs.entry->read_ahead;
			if (behavior == MAP_ENTRY_BEHAV_RANDOM ||
			    P_KILLED(curproc)) {
				behind = 0;
				nera = 0;
				ahead = 0;
			} else if (behavior == MAP_ENTRY_BEHAV_SEQUENTIAL) {
				behind = 0;
				nera = VM_FAULT_READ_AHEAD_MAX;
				ahead = nera;
				if (fs.pindex == fs.entry->next_read)
					vm_fault_dontneed(&fs, vaddr, ahead);
			} else if (fs.pindex == fs.entry->next_read) {
				/*
				 * This is a sequential fault.  Arithmetically
				 * increase the requested number of pages in
				 * the read-ahead window.  The requested
				 * number of pages is "# of sequential faults
				 * x (read ahead min + 1) + read ahead min"
				 */
				behind = 0;
				nera = VM_FAULT_READ_AHEAD_MIN;
				if (era > 0) {
					nera += era + 1;
					if (nera > VM_FAULT_READ_AHEAD_MAX)
						nera = VM_FAULT_READ_AHEAD_MAX;
				}
				ahead = nera;
				if (era == VM_FAULT_READ_AHEAD_MAX)
					vm_fault_dontneed(&fs, vaddr, ahead);
			} else {
				/*
				 * This is a non-sequential fault.  Request a
				 * cluster of pages that is aligned to a
				 * VM_FAULT_READ_DEFAULT page offset boundary
				 * within the object.  Alignment to a page
				 * offset boundary is more likely to coincide
				 * with the underlying file system block than
				 * alignment to a virtual address boundary.
				 */
				cluster_offset = fs.pindex %
				    VM_FAULT_READ_DEFAULT;
				behind = ulmin(cluster_offset,
				    atop(vaddr - fs.entry->start));
				nera = 0;
				ahead = VM_FAULT_READ_DEFAULT - 1 -
				    cluster_offset;
			}
			ahead = ulmin(ahead, atop(fs.entry->end - vaddr) - 1);
			if (era != nera)
				fs.entry->read_ahead = nera;

			/*
			 * Call the pager to retrieve the data, if any, after
			 * releasing the lock on the map.  We hold a ref on
			 * fs.object and the pages are exclusive busied.
			 */
			unlock_map(&fs);

			if (fs.object->type == OBJT_VNODE) {
				vp = fs.object->handle;
				if (vp == fs.vp)
					goto vnode_locked;
				else if (fs.vp != NULL) {
					vput(fs.vp);
					fs.vp = NULL;
				}
				locked = VOP_ISLOCKED(vp);

				if (locked != LK_EXCLUSIVE)
					locked = LK_SHARED;
				/* Do not sleep for vnode lock while fs.m is busy */
				error = vget(vp, locked | LK_CANRECURSE |
				    LK_NOWAIT, curthread);
				if (error != 0) {
					vhold(vp);
					release_page(&fs);
					unlock_and_deallocate(&fs);
					error = vget(vp, locked | LK_RETRY |
					    LK_CANRECURSE, curthread);
					vdrop(vp);
					fs.vp = vp;
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
			 * fs.m plus the additional pages are exclusive busied.
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
				 *
				 * Pager could have changed the page.  Pager
				 * is responsible for disposition of old page
				 * if moved.
				 */
				fs.m = marray[reqpage];
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
				vm_page_lock(fs.m);
				vm_page_free(fs.m);
				vm_page_unlock(fs.m);
				fs.m = NULL;
				unlock_and_deallocate(&fs);
				return ((rv == VM_PAGER_ERROR) ? KERN_FAILURE : KERN_PROTECTION_FAILURE);
			}
			if (fs.object != fs.first_object) {
				vm_page_lock(fs.m);
				vm_page_free(fs.m);
				vm_page_unlock(fs.m);
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
				VM_OBJECT_WUNLOCK(fs.object);

				fs.object = fs.first_object;
				fs.pindex = fs.first_pindex;
				fs.m = fs.first_m;
				VM_OBJECT_WLOCK(fs.object);
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
			/* Don't try to prefault neighboring pages. */
			faultcount = 1;
			break;	/* break to PAGE HAS BEEN FOUND */
		} else {
			KASSERT(fs.object != next_object,
			    ("object loop %p", next_object));
			VM_OBJECT_WLOCK(next_object);
			vm_object_pip_add(next_object, 1);
			if (fs.object != fs.first_object)
				vm_object_pip_wakeup(fs.object);
			VM_OBJECT_WUNLOCK(fs.object);
			fs.object = next_object;
		}
	}

	vm_page_assert_xbusied(fs.m);

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
			    (is_first_object_locked = VM_OBJECT_TRYWLOCK(fs.first_object)) &&
				/*
				 * We don't chase down the shadow chain
				 */
			    fs.object == fs.first_object->backing_object) {
				/*
				 * get rid of the unnecessary page
				 */
				vm_page_lock(fs.first_m);
				vm_page_free(fs.first_m);
				vm_page_unlock(fs.first_m);
				/*
				 * grab the page and put it into the 
				 * process'es object.  The page is 
				 * automatically made dirty.
				 */
				if (vm_page_rename(fs.m, fs.first_object,
				    fs.first_pindex)) {
					unlock_and_deallocate(&fs);
					goto RetryFault;
				}
#if VM_NRESERVLEVEL > 0
				/*
				 * Rename the reservation.
				 */
				vm_reserv_rename(fs.m, fs.first_object,
				    fs.object, OFF_TO_IDX(
				    fs.first_object->backing_object_offset));
#endif
				vm_page_xbusy(fs.m);
				fs.first_m = fs.m;
				fs.m = NULL;
				PCPU_INC(cnt.v_cow_optim);
			} else {
				/*
				 * Oh, well, lets copy it.
				 */
				pmap_copy_page(fs.m, fs.first_m);
				fs.first_m->valid = VM_PAGE_BITS_ALL;
				if (wired && (fault_flags &
				    VM_FAULT_WIRE) == 0) {
					vm_page_lock(fs.first_m);
					vm_page_wire(fs.first_m);
					vm_page_unlock(fs.first_m);
					
					vm_page_lock(fs.m);
					vm_page_unwire(fs.m, PQ_INACTIVE);
					vm_page_unlock(fs.m);
				}
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
			VM_OBJECT_WUNLOCK(fs.object);
			/*
			 * Only use the new page below...
			 */
			fs.object = fs.first_object;
			fs.pindex = fs.first_pindex;
			fs.m = fs.first_m;
			if (!is_first_object_locked)
				VM_OBJECT_WLOCK(fs.object);
			PCPU_INC(cnt.v_cow_faults);
			curthread->td_cow++;
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
		fs.entry->next_read = fs.pindex + faultcount - reqpage;

	vm_fault_dirty(fs.entry, fs.m, prot, fault_type, fault_flags, TRUE);
	vm_page_assert_xbusied(fs.m);

	/*
	 * Page must be completely valid or it is not fit to
	 * map into user space.  vm_pager_get_pages() ensures this.
	 */
	KASSERT(fs.m->valid == VM_PAGE_BITS_ALL,
	    ("vm_fault: page %p partially invalid", fs.m));
	VM_OBJECT_WUNLOCK(fs.object);

	/*
	 * Put this page into the physical map.  We had to do the unlock above
	 * because pmap_enter() may sleep.  We don't put the page
	 * back on the active queue until later so that the pageout daemon
	 * won't find it (yet).
	 */
	pmap_enter(fs.map->pmap, vaddr, fs.m, prot,
	    fault_type | (wired ? PMAP_ENTER_WIRED : 0), 0);
	if (faultcount != 1 && (fault_flags & VM_FAULT_WIRE) == 0 &&
	    wired == 0)
		vm_fault_prefault(&fs, vaddr, faultcount, reqpage);
	VM_OBJECT_WLOCK(fs.object);
	vm_page_lock(fs.m);

	/*
	 * If the page is not wired down, then put it where the pageout daemon
	 * can find it.
	 */
	if ((fault_flags & VM_FAULT_WIRE) != 0) {
		KASSERT(wired, ("VM_FAULT_WIRE && !wired"));
		vm_page_wire(fs.m);
	} else
		vm_page_activate(fs.m);
	if (m_hold != NULL) {
		*m_hold = fs.m;
		vm_page_hold(fs.m);
	}
	vm_page_unlock(fs.m);
	vm_page_xunbusy(fs.m);

	/*
	 * Unlock everything, and return
	 */
	unlock_and_deallocate(&fs);
	if (hardfault) {
		PCPU_INC(cnt.v_io_faults);
		curthread->td_ru.ru_majflt++;
	} else 
		curthread->td_ru.ru_minflt++;

	return (KERN_SUCCESS);
}

/*
 * Speed up the reclamation of pages that precede the faulting pindex within
 * the first object of the shadow chain.  Essentially, perform the equivalent
 * to madvise(..., MADV_DONTNEED) on a large cluster of pages that precedes
 * the faulting pindex by the cluster size when the pages read by vm_fault()
 * cross a cluster-size boundary.  The cluster size is the greater of the
 * smallest superpage size and VM_FAULT_DONTNEED_MIN.
 *
 * When "fs->first_object" is a shadow object, the pages in the backing object
 * that precede the faulting pindex are deactivated by vm_fault().  So, this
 * function must only be concerned with pages in the first object.
 */
static void
vm_fault_dontneed(const struct faultstate *fs, vm_offset_t vaddr, int ahead)
{
	vm_map_entry_t entry;
	vm_object_t first_object, object;
	vm_offset_t end, start;
	vm_page_t m, m_next;
	vm_pindex_t pend, pstart;
	vm_size_t size;

	object = fs->object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	first_object = fs->first_object;
	if (first_object != object) {
		if (!VM_OBJECT_TRYWLOCK(first_object)) {
			VM_OBJECT_WUNLOCK(object);
			VM_OBJECT_WLOCK(first_object);
			VM_OBJECT_WLOCK(object);
		}
	}
	/* Neither fictitious nor unmanaged pages can be reclaimed. */
	if ((first_object->flags & (OBJ_FICTITIOUS | OBJ_UNMANAGED)) == 0) {
		size = VM_FAULT_DONTNEED_MIN;
		if (MAXPAGESIZES > 1 && size < pagesizes[1])
			size = pagesizes[1];
		end = rounddown2(vaddr, size);
		if (vaddr - end >= size - PAGE_SIZE - ptoa(ahead) &&
		    (entry = fs->entry)->start < end) {
			if (end - entry->start < size)
				start = entry->start;
			else
				start = end - size;
			pmap_advise(fs->map->pmap, start, end, MADV_DONTNEED);
			pstart = OFF_TO_IDX(entry->offset) + atop(start -
			    entry->start);
			m_next = vm_page_find_least(first_object, pstart);
			pend = OFF_TO_IDX(entry->offset) + atop(end -
			    entry->start);
			while ((m = m_next) != NULL && m->pindex < pend) {
				m_next = TAILQ_NEXT(m, listq);
				if (m->valid != VM_PAGE_BITS_ALL ||
				    vm_page_busied(m))
					continue;

				/*
				 * Don't clear PGA_REFERENCED, since it would
				 * likely represent a reference by a different
				 * process.
				 *
				 * Typically, at this point, prefetched pages
				 * are still in the inactive queue.  Only
				 * pages that triggered page faults are in the
				 * active queue.
				 */
				vm_page_lock(m);
				vm_page_deactivate(m);
				vm_page_unlock(m);
			}
		}
	}
	if (first_object != object)
		VM_OBJECT_WUNLOCK(first_object);
}

/*
 * vm_fault_prefault provides a quick way of clustering
 * pagefaults into a processes address space.  It is a "cousin"
 * of vm_map_pmap_enter, except it runs at page fault time instead
 * of mmap time.
 */
static void
vm_fault_prefault(const struct faultstate *fs, vm_offset_t addra,
    int faultcount, int reqpage)
{
	pmap_t pmap;
	vm_map_entry_t entry;
	vm_object_t backing_object, lobject;
	vm_offset_t addr, starta;
	vm_pindex_t pindex;
	vm_page_t m;
	int backward, forward, i;

	pmap = fs->map->pmap;
	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace))
		return;

	if (faultcount > 0) {
		backward = reqpage;
		forward = faultcount - reqpage - 1;
	} else {
		backward = PFBAK;
		forward = PFFOR;
	}
	entry = fs->entry;

	starta = addra - backward * PAGE_SIZE;
	if (starta < entry->start) {
		starta = entry->start;
	} else if (starta > addra) {
		starta = 0;
	}

	/*
	 * Generate the sequence of virtual addresses that are candidates for
	 * prefaulting in an outward spiral from the faulting virtual address,
	 * "addra".  Specifically, the sequence is "addra - PAGE_SIZE", "addra
	 * + PAGE_SIZE", "addra - 2 * PAGE_SIZE", "addra + 2 * PAGE_SIZE", ...
	 * If the candidate address doesn't have a backing physical page, then
	 * the loop immediately terminates.
	 */
	for (i = 0; i < 2 * imax(backward, forward); i++) {
		addr = addra + ((i >> 1) + 1) * ((i & 1) == 0 ? -PAGE_SIZE :
		    PAGE_SIZE);
		if (addr > addra + forward * PAGE_SIZE)
			addr = 0;

		if (addr < starta || addr >= entry->end)
			continue;

		if (!pmap_is_prefaultable(pmap, addr))
			continue;

		pindex = ((addr - entry->start) + entry->offset) >> PAGE_SHIFT;
		lobject = entry->object.vm_object;
		VM_OBJECT_RLOCK(lobject);
		while ((m = vm_page_lookup(lobject, pindex)) == NULL &&
		    lobject->type == OBJT_DEFAULT &&
		    (backing_object = lobject->backing_object) != NULL) {
			KASSERT((lobject->backing_object_offset & PAGE_MASK) ==
			    0, ("vm_fault_prefault: unaligned object offset"));
			pindex += lobject->backing_object_offset >> PAGE_SHIFT;
			VM_OBJECT_RLOCK(backing_object);
			VM_OBJECT_RUNLOCK(lobject);
			lobject = backing_object;
		}
		if (m == NULL) {
			VM_OBJECT_RUNLOCK(lobject);
			break;
		}
		if (m->valid == VM_PAGE_BITS_ALL &&
		    (m->flags & PG_FICTITIOUS) == 0)
			pmap_enter_quick(pmap, addr, m, entry->protection);
		VM_OBJECT_RUNLOCK(lobject);
	}
}

/*
 * Hold each of the physical pages that are mapped by the specified range of
 * virtual addresses, ["addr", "addr" + "len"), if those mappings are valid
 * and allow the specified types of access, "prot".  If all of the implied
 * pages are successfully held, then the number of held pages is returned
 * together with pointers to those pages in the array "ma".  However, if any
 * of the pages cannot be held, -1 is returned.
 */
int
vm_fault_quick_hold_pages(vm_map_t map, vm_offset_t addr, vm_size_t len,
    vm_prot_t prot, vm_page_t *ma, int max_count)
{
	vm_offset_t end, va;
	vm_page_t *mp;
	int count;
	boolean_t pmap_failed;

	if (len == 0)
		return (0);
	end = round_page(addr + len);
	addr = trunc_page(addr);

	/*
	 * Check for illegal addresses.
	 */
	if (addr < vm_map_min(map) || addr > end || end > vm_map_max(map))
		return (-1);

	if (atop(end - addr) > max_count)
		panic("vm_fault_quick_hold_pages: count > max_count");
	count = atop(end - addr);

	/*
	 * Most likely, the physical pages are resident in the pmap, so it is
	 * faster to try pmap_extract_and_hold() first.
	 */
	pmap_failed = FALSE;
	for (mp = ma, va = addr; va < end; mp++, va += PAGE_SIZE) {
		*mp = pmap_extract_and_hold(map->pmap, va, prot);
		if (*mp == NULL)
			pmap_failed = TRUE;
		else if ((prot & VM_PROT_WRITE) != 0 &&
		    (*mp)->dirty != VM_PAGE_BITS_ALL) {
			/*
			 * Explicitly dirty the physical page.  Otherwise, the
			 * caller's changes may go unnoticed because they are
			 * performed through an unmanaged mapping or by a DMA
			 * operation.
			 *
			 * The object lock is not held here.
			 * See vm_page_clear_dirty_mask().
			 */
			vm_page_dirty(*mp);
		}
	}
	if (pmap_failed) {
		/*
		 * One or more pages could not be held by the pmap.  Either no
		 * page was mapped at the specified virtual address or that
		 * mapping had insufficient permissions.  Attempt to fault in
		 * and hold these pages.
		 */
		for (mp = ma, va = addr; va < end; mp++, va += PAGE_SIZE)
			if (*mp == NULL && vm_fault_hold(map, va, prot,
			    VM_FAULT_NORMAL, mp) != KERN_SUCCESS)
				goto error;
	}
	return (count);
error:	
	for (mp = ma; mp < ma + count; mp++)
		if (*mp != NULL) {
			vm_page_lock(*mp);
			vm_page_unhold(*mp);
			vm_page_unlock(*mp);
		}
	return (-1);
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
	boolean_t upgrade;

#ifdef	lint
	src_map++;
#endif	/* lint */

	upgrade = src_entry == dst_entry;
	access = prot = dst_entry->protection;

	src_object = src_entry->object.vm_object;
	src_pindex = OFF_TO_IDX(src_entry->offset);

	if (upgrade && (dst_entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0) {
		dst_object = src_object;
		vm_object_reference(dst_object);
	} else {
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
	}

	VM_OBJECT_WLOCK(dst_object);
	KASSERT(upgrade || dst_entry->object.vm_object == NULL,
	    ("vm_fault_copy_entry: vm_object not NULL"));
	if (src_object != dst_object) {
		dst_entry->object.vm_object = dst_object;
		dst_entry->offset = 0;
		dst_object->charge = dst_entry->end - dst_entry->start;
	}
	if (fork_charge != NULL) {
		KASSERT(dst_entry->cred == NULL,
		    ("vm_fault_copy_entry: leaked swp charge"));
		dst_object->cred = curthread->td_ucred;
		crhold(dst_object->cred);
		*fork_charge += dst_object->charge;
	} else if (dst_object->cred == NULL) {
		KASSERT(dst_entry->cred != NULL, ("no cred for entry %p",
		    dst_entry));
		dst_object->cred = dst_entry->cred;
		dst_entry->cred = NULL;
	}

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
	 * Loop through all of the virtual pages within the entry's
	 * range, copying each page from the source object to the
	 * destination object.  Since the source is wired, those pages
	 * must exist.  In contrast, the destination is pageable.
	 * Since the destination object does share any backing storage
	 * with the source object, all of its pages must be dirtied,
	 * regardless of whether they can be written.
	 */
	for (vaddr = dst_entry->start, dst_pindex = 0;
	    vaddr < dst_entry->end;
	    vaddr += PAGE_SIZE, dst_pindex++) {
again:
		/*
		 * Find the page in the source object, and copy it in.
		 * Because the source is wired down, the page will be
		 * in memory.
		 */
		if (src_object != dst_object)
			VM_OBJECT_RLOCK(src_object);
		object = src_object;
		pindex = src_pindex + dst_pindex;
		while ((src_m = vm_page_lookup(object, pindex)) == NULL &&
		    (backing_object = object->backing_object) != NULL) {
			/*
			 * Unless the source mapping is read-only or
			 * it is presently being upgraded from
			 * read-only, the first object in the shadow
			 * chain should provide all of the pages.  In
			 * other words, this loop body should never be
			 * executed when the source mapping is already
			 * read/write.
			 */
			KASSERT((src_entry->protection & VM_PROT_WRITE) == 0 ||
			    upgrade,
			    ("vm_fault_copy_entry: main object missing page"));

			VM_OBJECT_RLOCK(backing_object);
			pindex += OFF_TO_IDX(object->backing_object_offset);
			if (object != dst_object)
				VM_OBJECT_RUNLOCK(object);
			object = backing_object;
		}
		KASSERT(src_m != NULL, ("vm_fault_copy_entry: page missing"));

		if (object != dst_object) {
			/*
			 * Allocate a page in the destination object.
			 */
			dst_m = vm_page_alloc(dst_object, (src_object ==
			    dst_object ? src_pindex : 0) + dst_pindex,
			    VM_ALLOC_NORMAL);
			if (dst_m == NULL) {
				VM_OBJECT_WUNLOCK(dst_object);
				VM_OBJECT_RUNLOCK(object);
				VM_WAIT;
				VM_OBJECT_WLOCK(dst_object);
				goto again;
			}
			pmap_copy_page(src_m, dst_m);
			VM_OBJECT_RUNLOCK(object);
			dst_m->valid = VM_PAGE_BITS_ALL;
			dst_m->dirty = VM_PAGE_BITS_ALL;
		} else {
			dst_m = src_m;
			if (vm_page_sleep_if_busy(dst_m, "fltupg"))
				goto again;
			vm_page_xbusy(dst_m);
			KASSERT(dst_m->valid == VM_PAGE_BITS_ALL,
			    ("invalid dst page %p", dst_m));
		}
		VM_OBJECT_WUNLOCK(dst_object);

		/*
		 * Enter it in the pmap. If a wired, copy-on-write
		 * mapping is being replaced by a write-enabled
		 * mapping, then wire that new mapping.
		 */
		pmap_enter(dst_map->pmap, vaddr, dst_m, prot,
		    access | (upgrade ? PMAP_ENTER_WIRED : 0), 0);

		/*
		 * Mark it no longer busy, and put it on the active list.
		 */
		VM_OBJECT_WLOCK(dst_object);
		
		if (upgrade) {
			if (src_m != dst_m) {
				vm_page_lock(src_m);
				vm_page_unwire(src_m, PQ_INACTIVE);
				vm_page_unlock(src_m);
				vm_page_lock(dst_m);
				vm_page_wire(dst_m);
				vm_page_unlock(dst_m);
			} else {
				KASSERT(dst_m->wire_count > 0,
				    ("dst_m %p is not wired", dst_m));
			}
		} else {
			vm_page_lock(dst_m);
			vm_page_activate(dst_m);
			vm_page_unlock(dst_m);
		}
		vm_page_xunbusy(dst_m);
	}
	VM_OBJECT_WUNLOCK(dst_object);
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

	VM_OBJECT_ASSERT_WLOCKED(m->object);

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

/*
 * Block entry into the machine-independent layer's page fault handler by
 * the calling thread.  Subsequent calls to vm_fault() by that thread will
 * return KERN_PROTECTION_FAILURE.  Enable machine-dependent handling of
 * spurious page faults. 
 */
int
vm_fault_disable_pagefaults(void)
{

	return (curthread_pflags_set(TDP_NOFAULTING | TDP_RESETSPUR));
}

void
vm_fault_enable_pagefaults(int save)
{

	curthread_pflags_restore(save);
}
