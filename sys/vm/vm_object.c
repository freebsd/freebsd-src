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
 *	from: @(#)vm_object.c	8.5 (Berkeley) 3/22/94
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
 * $FreeBSD$
 */

/*
 *	Virtual memory object module.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>		/* for curproc, pageproc */
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <net/radix.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>
#include <vm/swap_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

static void	vm_object_qcollapse __P((vm_object_t object));

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

struct object_q vm_object_list;
static struct mtx vm_object_list_mtx;	/* lock for object list and count */
static long vm_object_count;		/* count of all objects */
vm_object_t kernel_object;
vm_object_t kmem_object;
static struct vm_object kernel_object_store;
static struct vm_object kmem_object_store;
extern int vm_pageout_page_count;

static long object_collapses;
static long object_bypasses;
static int next_index;
static vm_zone_t obj_zone;
static struct vm_zone obj_zone_store;
static int object_hash_rand;
#define VM_OBJECTS_INIT 256
static struct vm_object vm_objects_init[VM_OBJECTS_INIT];

void
_vm_object_allocate(type, size, object)
	objtype_t type;
	vm_size_t size;
	vm_object_t object;
{
	int incr;
	TAILQ_INIT(&object->memq);
	TAILQ_INIT(&object->shadow_head);

	object->type = type;
	object->size = size;
	object->ref_count = 1;
	object->flags = 0;
	if ((object->type == OBJT_DEFAULT) || (object->type == OBJT_SWAP))
		vm_object_set_flag(object, OBJ_ONEMAPPING);
	object->paging_in_progress = 0;
	object->resident_page_count = 0;
	object->shadow_count = 0;
	object->pg_color = next_index;
	if ( size > (PQ_L2_SIZE / 3 + PQ_PRIME1))
		incr = PQ_L2_SIZE / 3 + PQ_PRIME1;
	else
		incr = size;
	next_index = (next_index + incr) & PQ_L2_MASK;
	object->handle = NULL;
	object->backing_object = NULL;
	object->backing_object_offset = (vm_ooffset_t) 0;
	/*
	 * Try to generate a number that will spread objects out in the
	 * hash table.  We 'wipe' new objects across the hash in 128 page
	 * increments plus 1 more to offset it a little more by the time
	 * it wraps around.
	 */
	object->hash_rand = object_hash_rand - 129;

	object->generation++;

	TAILQ_INSERT_TAIL(&vm_object_list, object, object_list);
	vm_object_count++;
	object_hash_rand = object->hash_rand;
}

/*
 *	vm_object_init:
 *
 *	Initialize the VM objects module.
 */
void
vm_object_init()
{
	TAILQ_INIT(&vm_object_list);
	mtx_init(&vm_object_list_mtx, "vm object_list", MTX_DEF);
	vm_object_count = 0;
	
	kernel_object = &kernel_object_store;
	_vm_object_allocate(OBJT_DEFAULT, OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS),
	    kernel_object);

	kmem_object = &kmem_object_store;
	_vm_object_allocate(OBJT_DEFAULT, OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS),
	    kmem_object);

	obj_zone = &obj_zone_store;
	zbootinit(obj_zone, "VM OBJECT", sizeof (struct vm_object),
		vm_objects_init, VM_OBJECTS_INIT);
}

void
vm_object_init2() {
	zinitna(obj_zone, NULL, NULL, 0, 0, 0, 1);
}

/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */

vm_object_t
vm_object_allocate(type, size)
	objtype_t type;
	vm_size_t size;
{
	vm_object_t result;

	result = (vm_object_t) zalloc(obj_zone);

	_vm_object_allocate(type, size, result);

	return (result);
}


/*
 *	vm_object_reference:
 *
 *	Gets another reference to the given object.
 */
void
vm_object_reference(object)
	vm_object_t object;
{
	if (object == NULL)
		return;

	KASSERT(!(object->flags & OBJ_DEAD),
	    ("vm_object_reference: attempting to reference dead obj"));

	object->ref_count++;
	if (object->type == OBJT_VNODE) {
		while (vget((struct vnode *) object->handle, LK_RETRY|LK_NOOBJ, curproc)) {
			printf("vm_object_reference: delay in getting object\n");
		}
	}
}

void
vm_object_vndeallocate(object)
	vm_object_t object;
{
	struct vnode *vp = (struct vnode *) object->handle;

	KASSERT(object->type == OBJT_VNODE,
	    ("vm_object_vndeallocate: not a vnode object"));
	KASSERT(vp != NULL, ("vm_object_vndeallocate: missing vp"));
#ifdef INVARIANTS
	if (object->ref_count == 0) {
		vprint("vm_object_vndeallocate", vp);
		panic("vm_object_vndeallocate: bad object reference count");
	}
#endif

	object->ref_count--;
	if (object->ref_count == 0) {
		vp->v_flag &= ~VTEXT;
		vm_object_clear_flag(object, OBJ_OPT);
	}
	vrele(vp);
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
	vm_object_t object;
{
	vm_object_t temp;

	while (object != NULL) {

		if (object->type == OBJT_VNODE) {
			vm_object_vndeallocate(object);
			return;
		}

		KASSERT(object->ref_count != 0,
			("vm_object_deallocate: object deallocated too many times: %d", object->type));

		/*
		 * If the reference count goes to 0 we start calling
		 * vm_object_terminate() on the object chain.
		 * A ref count of 1 may be a special case depending on the
		 * shadow count being 0 or 1.
		 */
		object->ref_count--;
		if (object->ref_count > 1) {
			return;
		} else if (object->ref_count == 1) {
			if (object->shadow_count == 0) {
				vm_object_set_flag(object, OBJ_ONEMAPPING);
			} else if ((object->shadow_count == 1) &&
			    (object->handle == NULL) &&
			    (object->type == OBJT_DEFAULT ||
			     object->type == OBJT_SWAP)) {
				vm_object_t robject;

				robject = TAILQ_FIRST(&object->shadow_head);
				KASSERT(robject != NULL,
				    ("vm_object_deallocate: ref_count: %d, shadow_count: %d",
					 object->ref_count,
					 object->shadow_count));
				if ((robject->handle == NULL) &&
				    (robject->type == OBJT_DEFAULT ||
				     robject->type == OBJT_SWAP)) {

					robject->ref_count++;

					while (
						robject->paging_in_progress ||
						object->paging_in_progress
					) {
						vm_object_pip_sleep(robject, "objde1");
						vm_object_pip_sleep(object, "objde2");
					}

					if (robject->ref_count == 1) {
						robject->ref_count--;
						object = robject;
						goto doterm;
					}

					object = robject;
					vm_object_collapse(object);
					continue;
				}
			}

			return;

		}

doterm:

		temp = object->backing_object;
		if (temp) {
			TAILQ_REMOVE(&temp->shadow_head, object, shadow_list);
			temp->shadow_count--;
			if (temp->ref_count == 0)
				vm_object_clear_flag(temp, OBJ_OPT);
			temp->generation++;
			object->backing_object = NULL;
		}
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
 *	This routine may block.
 */
void
vm_object_terminate(object)
	vm_object_t object;
{
	vm_page_t p;
	int s;

	/*
	 * Make sure no one uses us.
	 */
	vm_object_set_flag(object, OBJ_DEAD);

	/*
	 * wait for the pageout daemon to be done with the object
	 */
	vm_object_pip_wait(object, "objtrm");

	KASSERT(!object->paging_in_progress,
		("vm_object_terminate: pageout in progress"));

	/*
	 * Clean and free the pages, as appropriate. All references to the
	 * object are gone, so we don't need to lock it.
	 */
	if (object->type == OBJT_VNODE) {
		struct vnode *vp;

		/*
		 * Freeze optimized copies.
		 */
		vm_freeze_copyopts(object, 0, object->size);

		/*
		 * Clean pages and flush buffers.
		 */
		vm_object_page_clean(object, 0, 0, OBJPC_SYNC);

		vp = (struct vnode *) object->handle;
		vinvalbuf(vp, V_SAVE, NOCRED, NULL, 0, 0);
	}

	KASSERT(object->ref_count == 0, 
		("vm_object_terminate: object with references, ref_count=%d",
		object->ref_count));

	/*
	 * Now free any remaining pages. For internal objects, this also
	 * removes them from paging queues. Don't free wired pages, just
	 * remove them from the object. 
	 */
	s = splvm();
	while ((p = TAILQ_FIRST(&object->memq)) != NULL) {
		KASSERT(!p->busy && (p->flags & PG_BUSY) == 0,
			("vm_object_terminate: freeing busy page %p "
			"p->busy = %d, p->flags %x\n", p, p->busy, p->flags));
		if (p->wire_count == 0) {
			vm_page_busy(p);
			vm_page_free(p);
			cnt.v_pfree++;
		} else {
			vm_page_busy(p);
			vm_page_remove(p);
		}
	}
	splx(s);

	/*
	 * Let the pager know object is dead.
	 */
	vm_pager_deallocate(object);

	/*
	 * Remove the object from the global object list.
	 */
	mtx_lock(&vm_object_list_mtx);
	TAILQ_REMOVE(&vm_object_list, object, object_list);
	mtx_unlock(&vm_object_list_mtx);

	wakeup(object);

	/*
	 * Free the space for the object.
	 */
	zfree(obj_zone, object);
}

/*
 *	vm_object_page_clean
 *
 *	Clean all dirty pages in the specified range of object.  Leaves page 
 * 	on whatever queue it is currently on.   If NOSYNC is set then do not
 *	write out pages with PG_NOSYNC set (originally comes from MAP_NOSYNC),
 *	leaving the object dirty.
 *
 *	Odd semantics: if start == end, we clean everything.
 *
 *	The object must be locked.
 */

void
vm_object_page_clean(object, start, end, flags)
	vm_object_t object;
	vm_pindex_t start;
	vm_pindex_t end;
	int flags;
{
	vm_page_t p, np, tp;
	vm_offset_t tstart, tend;
	vm_pindex_t pi;
	int s;
	struct vnode *vp;
	int runlen;
	int maxf;
	int chkb;
	int maxb;
	int i;
	int clearobjflags;
	int pagerflags;
	vm_page_t maf[vm_pageout_page_count];
	vm_page_t mab[vm_pageout_page_count];
	vm_page_t ma[vm_pageout_page_count];
	int curgeneration;

	if (object->type != OBJT_VNODE ||
		(object->flags & OBJ_MIGHTBEDIRTY) == 0)
		return;

	pagerflags = (flags & (OBJPC_SYNC | OBJPC_INVAL)) ? VM_PAGER_PUT_SYNC : 0;
	pagerflags |= (flags & OBJPC_INVAL) ? VM_PAGER_PUT_INVAL : 0;

	vp = object->handle;

	vm_object_set_flag(object, OBJ_CLEANING);

	tstart = start;
	if (end == 0) {
		tend = object->size;
	} else {
		tend = end;
	}

	/*
	 * Generally set CLEANCHK interlock and make the page read-only so
	 * we can then clear the object flags.
	 *
	 * However, if this is a nosync mmap then the object is likely to 
	 * stay dirty so do not mess with the page and do not clear the
	 * object flags.
	 */

	clearobjflags = 1;

	TAILQ_FOREACH(p, &object->memq, listq) {
		vm_page_flag_set(p, PG_CLEANCHK);
		if ((flags & OBJPC_NOSYNC) && (p->flags & PG_NOSYNC))
			clearobjflags = 0;
		else
			vm_page_protect(p, VM_PROT_READ);
	}

	if (clearobjflags && (tstart == 0) && (tend == object->size)) {
		vm_object_clear_flag(object, OBJ_WRITEABLE|OBJ_MIGHTBEDIRTY);
	}

rescan:
	curgeneration = object->generation;

	for(p = TAILQ_FIRST(&object->memq); p; p = np) {
		np = TAILQ_NEXT(p, listq);

		pi = p->pindex;
		if (((p->flags & PG_CLEANCHK) == 0) ||
			(pi < tstart) || (pi >= tend) ||
			(p->valid == 0) ||
			((p->queue - p->pc) == PQ_CACHE)) {
			vm_page_flag_clear(p, PG_CLEANCHK);
			continue;
		}

		vm_page_test_dirty(p);
		if ((p->dirty & p->valid) == 0) {
			vm_page_flag_clear(p, PG_CLEANCHK);
			continue;
		}

		/*
		 * If we have been asked to skip nosync pages and this is a
		 * nosync page, skip it.  Note that the object flags were
		 * not cleared in this case so we do not have to set them.
		 */
		if ((flags & OBJPC_NOSYNC) && (p->flags & PG_NOSYNC)) {
			vm_page_flag_clear(p, PG_CLEANCHK);
			continue;
		}

		s = splvm();
		while (vm_page_sleep_busy(p, TRUE, "vpcwai")) {
			if (object->generation != curgeneration) {
				splx(s);
				goto rescan;
			}
		}

		maxf = 0;
		for(i=1;i<vm_pageout_page_count;i++) {
			if ((tp = vm_page_lookup(object, pi + i)) != NULL) {
				if ((tp->flags & PG_BUSY) ||
					(tp->flags & PG_CLEANCHK) == 0 ||
					(tp->busy != 0))
					break;
				if((tp->queue - tp->pc) == PQ_CACHE) {
					vm_page_flag_clear(tp, PG_CLEANCHK);
					break;
				}
				vm_page_test_dirty(tp);
				if ((tp->dirty & tp->valid) == 0) {
					vm_page_flag_clear(tp, PG_CLEANCHK);
					break;
				}
				maf[ i - 1 ] = tp;
				maxf++;
				continue;
			}
			break;
		}

		maxb = 0;
		chkb = vm_pageout_page_count -  maxf;
		if (chkb) {
			for(i = 1; i < chkb;i++) {
				if ((tp = vm_page_lookup(object, pi - i)) != NULL) {
					if ((tp->flags & PG_BUSY) ||
						(tp->flags & PG_CLEANCHK) == 0 ||
						(tp->busy != 0))
						break;
					if((tp->queue - tp->pc) == PQ_CACHE) {
						vm_page_flag_clear(tp, PG_CLEANCHK);
						break;
					}
					vm_page_test_dirty(tp);
					if ((tp->dirty & tp->valid) == 0) {
						vm_page_flag_clear(tp, PG_CLEANCHK);
						break;
					}
					mab[ i - 1 ] = tp;
					maxb++;
					continue;
				}
				break;
			}
		}

		for(i=0;i<maxb;i++) {
			int index = (maxb - i) - 1;
			ma[index] = mab[i];
			vm_page_flag_clear(ma[index], PG_CLEANCHK);
		}
		vm_page_flag_clear(p, PG_CLEANCHK);
		ma[maxb] = p;
		for(i=0;i<maxf;i++) {
			int index = (maxb + i) + 1;
			ma[index] = maf[i];
			vm_page_flag_clear(ma[index], PG_CLEANCHK);
		}
		runlen = maxb + maxf + 1;

		splx(s);
		vm_pageout_flush(ma, runlen, pagerflags);
		for (i = 0; i<runlen; i++) {
			if (ma[i]->valid & ma[i]->dirty) {
				vm_page_protect(ma[i], VM_PROT_READ);
				vm_page_flag_set(ma[i], PG_CLEANCHK);
			}
		}
		if (object->generation != curgeneration)
			goto rescan;
	}

#if 0
	VOP_FSYNC(vp, NULL, (pagerflags & VM_PAGER_PUT_SYNC)?MNT_WAIT:0, curproc);
#endif

	vm_object_clear_flag(object, OBJ_CLEANING);
	return;
}

#ifdef not_used
/* XXX I cannot tell if this should be an exported symbol */
/*
 *	vm_object_deactivate_pages
 *
 *	Deactivate all pages in the specified object.  (Keep its pages
 *	in memory even though it is no longer referenced.)
 *
 *	The object must be locked.
 */
static void
vm_object_deactivate_pages(object)
	vm_object_t object;
{
	vm_page_t p, next;

	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = next) {
		next = TAILQ_NEXT(p, listq);
		vm_page_deactivate(p);
	}
}
#endif

/*
 * Same as vm_object_pmap_copy, except range checking really
 * works, and is meant for small sections of an object.
 *
 * This code protects resident pages by making them read-only
 * and is typically called on a fork or split when a page
 * is converted to copy-on-write.  
 *
 * NOTE: If the page is already at VM_PROT_NONE, calling
 * vm_page_protect will have no effect.
 */

void
vm_object_pmap_copy_1(object, start, end)
	vm_object_t object;
	vm_pindex_t start;
	vm_pindex_t end;
{
	vm_pindex_t idx;
	vm_page_t p;

	if (object == NULL || (object->flags & OBJ_WRITEABLE) == 0)
		return;

	for (idx = start; idx < end; idx++) {
		p = vm_page_lookup(object, idx);
		if (p == NULL)
			continue;
		vm_page_protect(p, VM_PROT_READ);
	}
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
	vm_object_t object;
	vm_pindex_t start;
	vm_pindex_t end;
{
	vm_page_t p;

	if (object == NULL)
		return;
	TAILQ_FOREACH(p, &object->memq, listq) {
		if (p->pindex >= start && p->pindex < end)
			vm_page_protect(p, VM_PROT_NONE);
	}
	if ((start == 0) && (object->size == end))
		vm_object_clear_flag(object, OBJ_WRITEABLE);
}

/*
 *	vm_object_madvise:
 *
 *	Implements the madvise function at the object/page level.
 *
 *	MADV_WILLNEED	(any object)
 *
 *	    Activate the specified pages if they are resident.
 *
 *	MADV_DONTNEED	(any object)
 *
 *	    Deactivate the specified pages if they are resident.
 *
 *	MADV_FREE	(OBJT_DEFAULT/OBJT_SWAP objects,
 *			 OBJ_ONEMAPPING only)
 *
 *	    Deactivate and clean the specified pages if they are
 *	    resident.  This permits the process to reuse the pages
 *	    without faulting or the kernel to reclaim the pages
 *	    without I/O.
 */
void
vm_object_madvise(object, pindex, count, advise)
	vm_object_t object;
	vm_pindex_t pindex;
	int count;
	int advise;
{
	vm_pindex_t end, tpindex;
	vm_object_t tobject;
	vm_page_t m;

	if (object == NULL)
		return;

	end = pindex + count;

	/*
	 * Locate and adjust resident pages
	 */

	for (; pindex < end; pindex += 1) {
relookup:
		tobject = object;
		tpindex = pindex;
shadowlookup:
		/*
		 * MADV_FREE only operates on OBJT_DEFAULT or OBJT_SWAP pages
		 * and those pages must be OBJ_ONEMAPPING.
		 */
		if (advise == MADV_FREE) {
			if ((tobject->type != OBJT_DEFAULT &&
			     tobject->type != OBJT_SWAP) ||
			    (tobject->flags & OBJ_ONEMAPPING) == 0) {
				continue;
			}
		}

		m = vm_page_lookup(tobject, tpindex);

		if (m == NULL) {
			/*
			 * There may be swap even if there is no backing page
			 */
			if (advise == MADV_FREE && tobject->type == OBJT_SWAP)
				swap_pager_freespace(tobject, tpindex, 1);

			/*
			 * next object
			 */
			tobject = tobject->backing_object;
			if (tobject == NULL)
				continue;
			tpindex += OFF_TO_IDX(tobject->backing_object_offset);
			goto shadowlookup;
		}

		/*
		 * If the page is busy or not in a normal active state,
		 * we skip it.  If the page is not managed there are no
		 * page queues to mess with.  Things can break if we mess
		 * with pages in any of the below states.
		 */
		if (
		    m->hold_count ||
		    m->wire_count ||
		    (m->flags & PG_UNMANAGED) ||
		    m->valid != VM_PAGE_BITS_ALL
		) {
			continue;
		}

 		if (vm_page_sleep_busy(m, TRUE, "madvpo"))
  			goto relookup;

		if (advise == MADV_WILLNEED) {
			vm_page_activate(m);
		} else if (advise == MADV_DONTNEED) {
			vm_page_dontneed(m);
		} else if (advise == MADV_FREE) {
			/*
			 * Mark the page clean.  This will allow the page
			 * to be freed up by the system.  However, such pages
			 * are often reused quickly by malloc()/free()
			 * so we do not do anything that would cause
			 * a page fault if we can help it.
			 *
			 * Specifically, we do not try to actually free
			 * the page now nor do we try to put it in the
			 * cache (which would cause a page fault on reuse).
			 *
			 * But we do make the page is freeable as we
			 * can without actually taking the step of unmapping
			 * it.
			 */
			pmap_clear_modify(m);
			m->dirty = 0;
			m->act_count = 0;
			vm_page_dontneed(m);
			if (tobject->type == OBJT_SWAP)
				swap_pager_freespace(tobject, tpindex, 1);
		}
	}	
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
	vm_object_t *object;	/* IN/OUT */
	vm_ooffset_t *offset;	/* IN/OUT */
	vm_size_t length;
{
	vm_object_t source;
	vm_object_t result;

	source = *object;

	/*
	 * Don't create the new object if the old object isn't shared.
	 */

	if (source != NULL &&
	    source->ref_count == 1 &&
	    source->handle == NULL &&
	    (source->type == OBJT_DEFAULT ||
	     source->type == OBJT_SWAP))
		return;

	/*
	 * Allocate a new object with the given length
	 */
	result = vm_object_allocate(OBJT_DEFAULT, length);
	KASSERT(result != NULL, ("vm_object_shadow: no object for shadowing"));

	/*
	 * The new object shadows the source object, adding a reference to it.
	 * Our caller changes his reference to point to the new object,
	 * removing a reference to the source object.  Net result: no change
	 * of reference count.
	 *
	 * Try to optimize the result object's page color when shadowing
	 * in order to maintain page coloring consistency in the combined 
	 * shadowed object.
	 */
	result->backing_object = source;
	if (source) {
		TAILQ_INSERT_TAIL(&source->shadow_head, result, shadow_list);
		source->shadow_count++;
		source->generation++;
		result->pg_color = (source->pg_color + OFF_TO_IDX(*offset)) & PQ_L2_MASK;
	}

	/*
	 * Store the offset into the source object, and fix up the offset into
	 * the new object.
	 */

	result->backing_object_offset = *offset;

	/*
	 * Return the new things
	 */

	*offset = 0;
	*object = result;
}

#define	OBSC_TEST_ALL_SHADOWED	0x0001
#define	OBSC_COLLAPSE_NOWAIT	0x0002
#define	OBSC_COLLAPSE_WAIT	0x0004

static __inline int
vm_object_backing_scan(vm_object_t object, int op)
{
	int s;
	int r = 1;
	vm_page_t p;
	vm_object_t backing_object;
	vm_pindex_t backing_offset_index;

	s = splvm();

	backing_object = object->backing_object;
	backing_offset_index = OFF_TO_IDX(object->backing_object_offset);

	/*
	 * Initial conditions
	 */

	if (op & OBSC_TEST_ALL_SHADOWED) {
		/*
		 * We do not want to have to test for the existence of
		 * swap pages in the backing object.  XXX but with the
		 * new swapper this would be pretty easy to do.
		 *
		 * XXX what about anonymous MAP_SHARED memory that hasn't
		 * been ZFOD faulted yet?  If we do not test for this, the
		 * shadow test may succeed! XXX
		 */
		if (backing_object->type != OBJT_DEFAULT) {
			splx(s);
			return(0);
		}
	}
	if (op & OBSC_COLLAPSE_WAIT) {
		vm_object_set_flag(backing_object, OBJ_DEAD);
	}

	/*
	 * Our scan
	 */

	p = TAILQ_FIRST(&backing_object->memq);
	while (p) {
		vm_page_t next = TAILQ_NEXT(p, listq);
		vm_pindex_t new_pindex = p->pindex - backing_offset_index;

		if (op & OBSC_TEST_ALL_SHADOWED) {
			vm_page_t pp;

			/*
			 * Ignore pages outside the parent object's range
			 * and outside the parent object's mapping of the 
			 * backing object.
			 *
			 * note that we do not busy the backing object's
			 * page.
			 */

			if (
			    p->pindex < backing_offset_index ||
			    new_pindex >= object->size
			) {
				p = next;
				continue;
			}

			/*
			 * See if the parent has the page or if the parent's
			 * object pager has the page.  If the parent has the
			 * page but the page is not valid, the parent's
			 * object pager must have the page.
			 *
			 * If this fails, the parent does not completely shadow
			 * the object and we might as well give up now.
			 */

			pp = vm_page_lookup(object, new_pindex);
			if (
			    (pp == NULL || pp->valid == 0) &&
			    !vm_pager_has_page(object, new_pindex, NULL, NULL)
			) {
				r = 0;
				break;
			}
		}

		/*
		 * Check for busy page
		 */

		if (op & (OBSC_COLLAPSE_WAIT | OBSC_COLLAPSE_NOWAIT)) {
			vm_page_t pp;

			if (op & OBSC_COLLAPSE_NOWAIT) {
				if (
				    (p->flags & PG_BUSY) ||
				    !p->valid || 
				    p->hold_count || 
				    p->wire_count ||
				    p->busy
				) {
					p = next;
					continue;
				}
			} else if (op & OBSC_COLLAPSE_WAIT) {
				if (vm_page_sleep_busy(p, TRUE, "vmocol")) {
					/*
					 * If we slept, anything could have
					 * happened.  Since the object is
					 * marked dead, the backing offset
					 * should not have changed so we
					 * just restart our scan.
					 */
					p = TAILQ_FIRST(&backing_object->memq);
					continue;
				}
			}

			/* 
			 * Busy the page
			 */
			vm_page_busy(p);

			KASSERT(
			    p->object == backing_object,
			    ("vm_object_qcollapse(): object mismatch")
			);

			/*
			 * Destroy any associated swap
			 */
			if (backing_object->type == OBJT_SWAP) {
				swap_pager_freespace(
				    backing_object, 
				    p->pindex,
				    1
				);
			}

			if (
			    p->pindex < backing_offset_index ||
			    new_pindex >= object->size
			) {
				/*
				 * Page is out of the parent object's range, we 
				 * can simply destroy it. 
				 */
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_free(p);
				p = next;
				continue;
			}

			pp = vm_page_lookup(object, new_pindex);
			if (
			    pp != NULL ||
			    vm_pager_has_page(object, new_pindex, NULL, NULL)
			) {
				/*
				 * page already exists in parent OR swap exists
				 * for this location in the parent.  Destroy 
				 * the original page from the backing object.
				 *
				 * Leave the parent's page alone
				 */
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_free(p);
				p = next;
				continue;
			}

			/*
			 * Page does not exist in parent, rename the
			 * page from the backing object to the main object. 
			 *
			 * If the page was mapped to a process, it can remain 
			 * mapped through the rename.
			 */
			if ((p->queue - p->pc) == PQ_CACHE)
				vm_page_deactivate(p);

			vm_page_rename(p, object, new_pindex);
			/* page automatically made dirty by rename */
		}
		p = next;
	}
	splx(s);
	return(r);
}


/*
 * this version of collapse allows the operation to occur earlier and
 * when paging_in_progress is true for an object...  This is not a complete
 * operation, but should plug 99.9% of the rest of the leaks.
 */
static void
vm_object_qcollapse(object)
	vm_object_t object;
{
	vm_object_t backing_object = object->backing_object;

	if (backing_object->ref_count != 1)
		return;

	backing_object->ref_count += 2;

	vm_object_backing_scan(object, OBSC_COLLAPSE_NOWAIT);

	backing_object->ref_count -= 2;
}

/*
 *	vm_object_collapse:
 *
 *	Collapse an object with the object backing it.
 *	Pages in the backing object are moved into the
 *	parent, and the backing object is deallocated.
 */
void
vm_object_collapse(object)
	vm_object_t object;
{
	while (TRUE) {
		vm_object_t backing_object;

		/*
		 * Verify that the conditions are right for collapse:
		 *
		 * The object exists and the backing object exists.
		 */
		if (object == NULL)
			break;

		if ((backing_object = object->backing_object) == NULL)
			break;

		/*
		 * we check the backing object first, because it is most likely
		 * not collapsable.
		 */
		if (backing_object->handle != NULL ||
		    (backing_object->type != OBJT_DEFAULT &&
		     backing_object->type != OBJT_SWAP) ||
		    (backing_object->flags & OBJ_DEAD) ||
		    object->handle != NULL ||
		    (object->type != OBJT_DEFAULT &&
		     object->type != OBJT_SWAP) ||
		    (object->flags & OBJ_DEAD)) {
			break;
		}

		if (
		    object->paging_in_progress != 0 ||
		    backing_object->paging_in_progress != 0
		) {
			vm_object_qcollapse(object);
			break;
		}

		/*
		 * We know that we can either collapse the backing object (if
		 * the parent is the only reference to it) or (perhaps) have
		 * the parent bypass the object if the parent happens to shadow
		 * all the resident pages in the entire backing object.
		 *
		 * This is ignoring pager-backed pages such as swap pages.
		 * vm_object_backing_scan fails the shadowing test in this
		 * case.
		 */

		if (backing_object->ref_count == 1) {
			/*
			 * If there is exactly one reference to the backing
			 * object, we can collapse it into the parent.  
			 */

			vm_object_backing_scan(object, OBSC_COLLAPSE_WAIT);

			/*
			 * Move the pager from backing_object to object.
			 */

			if (backing_object->type == OBJT_SWAP) {
				vm_object_pip_add(backing_object, 1);

				/*
				 * scrap the paging_offset junk and do a 
				 * discrete copy.  This also removes major 
				 * assumptions about how the swap-pager 
				 * works from where it doesn't belong.  The
				 * new swapper is able to optimize the
				 * destroy-source case.
				 */

				vm_object_pip_add(object, 1);
				swap_pager_copy(
				    backing_object,
				    object,
				    OFF_TO_IDX(object->backing_object_offset), TRUE);
				vm_object_pip_wakeup(object);

				vm_object_pip_wakeup(backing_object);
			}
			/*
			 * Object now shadows whatever backing_object did.
			 * Note that the reference to 
			 * backing_object->backing_object moves from within 
			 * backing_object to within object.
			 */

			TAILQ_REMOVE(
			    &object->backing_object->shadow_head, 
			    object,
			    shadow_list
			);
			object->backing_object->shadow_count--;
			object->backing_object->generation++;
			if (backing_object->backing_object) {
				TAILQ_REMOVE(
				    &backing_object->backing_object->shadow_head,
				    backing_object, 
				    shadow_list
				);
				backing_object->backing_object->shadow_count--;
				backing_object->backing_object->generation++;
			}
			object->backing_object = backing_object->backing_object;
			if (object->backing_object) {
				TAILQ_INSERT_TAIL(
				    &object->backing_object->shadow_head,
				    object, 
				    shadow_list
				);
				object->backing_object->shadow_count++;
				object->backing_object->generation++;
			}

			object->backing_object_offset +=
			    backing_object->backing_object_offset;

			/*
			 * Discard backing_object.
			 *
			 * Since the backing object has no pages, no pager left,
			 * and no object references within it, all that is
			 * necessary is to dispose of it.
			 */

			TAILQ_REMOVE(
			    &vm_object_list, 
			    backing_object,
			    object_list
			);
			vm_object_count--;

			zfree(obj_zone, backing_object);

			object_collapses++;
		} else {
			vm_object_t new_backing_object;

			/*
			 * If we do not entirely shadow the backing object,
			 * there is nothing we can do so we give up.
			 */

			if (vm_object_backing_scan(object, OBSC_TEST_ALL_SHADOWED) == 0) {
				break;
			}

			/*
			 * Make the parent shadow the next object in the
			 * chain.  Deallocating backing_object will not remove
			 * it, since its reference count is at least 2.
			 */

			TAILQ_REMOVE(
			    &backing_object->shadow_head,
			    object,
			    shadow_list
			);
			backing_object->shadow_count--;
			backing_object->generation++;

			new_backing_object = backing_object->backing_object;
			if ((object->backing_object = new_backing_object) != NULL) {
				vm_object_reference(new_backing_object);
				TAILQ_INSERT_TAIL(
				    &new_backing_object->shadow_head,
				    object,
				    shadow_list
				);
				new_backing_object->shadow_count++;
				new_backing_object->generation++;
				object->backing_object_offset +=
					backing_object->backing_object_offset;
			}

			/*
			 * Drop the reference count on backing_object. Since
			 * its ref_count was at least 2, it will not vanish;
			 * so we don't need to call vm_object_deallocate, but
			 * we do anyway.
			 */
			vm_object_deallocate(backing_object);
			object_bypasses++;
		}

		/*
		 * Try again with this object's new backing object.
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
vm_object_page_remove(object, start, end, clean_only)
	vm_object_t object;
	vm_pindex_t start;
	vm_pindex_t end;
	boolean_t clean_only;
{
	vm_page_t p, next;
	unsigned int size;
	int all;

	if (object == NULL ||
	    object->resident_page_count == 0)
		return;

	all = ((end == 0) && (start == 0));

	/*
	 * Since physically-backed objects do not use managed pages, we can't
	 * remove pages from the object (we must instead remove the page
	 * references, and then destroy the object).
	 */
	KASSERT(object->type != OBJT_PHYS, ("attempt to remove pages from a physical object"));

	vm_object_pip_add(object, 1);
again:
	size = end - start;
	if (all || size > object->resident_page_count / 4) {
		for (p = TAILQ_FIRST(&object->memq); p != NULL; p = next) {
			next = TAILQ_NEXT(p, listq);
			if (all || ((start <= p->pindex) && (p->pindex < end))) {
				if (p->wire_count != 0) {
					vm_page_protect(p, VM_PROT_NONE);
					if (!clean_only)
						p->valid = 0;
					continue;
				}

				/*
				 * The busy flags are only cleared at
				 * interrupt -- minimize the spl transitions
				 */

 				if (vm_page_sleep_busy(p, TRUE, "vmopar"))
 					goto again;

				if (clean_only && p->valid) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty)
						continue;
				}

				vm_page_busy(p);
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_free(p);
			}
		}
	} else {
		while (size > 0) {
			if ((p = vm_page_lookup(object, start)) != 0) {

				if (p->wire_count != 0) {
					vm_page_protect(p, VM_PROT_NONE);
					if (!clean_only)
						p->valid = 0;
					start += 1;
					size -= 1;
					continue;
				}

				/*
				 * The busy flags are only cleared at
				 * interrupt -- minimize the spl transitions
				 */
 				if (vm_page_sleep_busy(p, TRUE, "vmopar"))
					goto again;

				if (clean_only && p->valid) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty) {
						start += 1;
						size -= 1;
						continue;
					}
				}

				vm_page_busy(p);
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_free(p);
			}
			start += 1;
			size -= 1;
		}
	}
	vm_object_pip_wakeup(object);
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
vm_object_coalesce(prev_object, prev_pindex, prev_size, next_size)
	vm_object_t prev_object;
	vm_pindex_t prev_pindex;
	vm_size_t prev_size, next_size;
{
	vm_pindex_t next_pindex;

	if (prev_object == NULL) {
		return (TRUE);
	}

	if (prev_object->type != OBJT_DEFAULT &&
	    prev_object->type != OBJT_SWAP) {
		return (FALSE);
	}

	/*
	 * Try to collapse the object first
	 */
	vm_object_collapse(prev_object);

	/*
	 * Can't coalesce if: . more than one reference . paged out . shadows
	 * another object . has a copy elsewhere (any of which mean that the
	 * pages not mapped to prev_entry may be in use anyway)
	 */

	if (prev_object->backing_object != NULL) {
		return (FALSE);
	}

	prev_size >>= PAGE_SHIFT;
	next_size >>= PAGE_SHIFT;
	next_pindex = prev_pindex + prev_size;

	if ((prev_object->ref_count > 1) &&
	    (prev_object->size != next_pindex)) {
		return (FALSE);
	}

	/*
	 * Remove any pages that may still be in the object from a previous
	 * deallocation.
	 */
	if (next_pindex < prev_object->size) {
		vm_object_page_remove(prev_object,
				      next_pindex,
				      next_pindex + next_size, FALSE);
		if (prev_object->type == OBJT_SWAP)
			swap_pager_freespace(prev_object,
					     next_pindex, next_size);
	}

	/*
	 * Extend the object if necessary.
	 */
	if (next_pindex + next_size > prev_object->size)
		prev_object->size = next_pindex + next_size;

	return (TRUE);
}

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <sys/cons.h>

#include <ddb/ddb.h>

static int	_vm_object_in_map __P((vm_map_t map, vm_object_t object,
				       vm_map_entry_t entry));
static int	vm_object_in_map __P((vm_object_t object));

static int
_vm_object_in_map(map, object, entry)
	vm_map_t map;
	vm_object_t object;
	vm_map_entry_t entry;
{
	vm_map_t tmpm;
	vm_map_entry_t tmpe;
	vm_object_t obj;
	int entcount;

	if (map == 0)
		return 0;

	if (entry == 0) {
		tmpe = map->header.next;
		entcount = map->nentries;
		while (entcount-- && (tmpe != &map->header)) {
			if( _vm_object_in_map(map, object, tmpe)) {
				return 1;
			}
			tmpe = tmpe->next;
		}
	} else if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
		tmpm = entry->object.sub_map;
		tmpe = tmpm->header.next;
		entcount = tmpm->nentries;
		while (entcount-- && tmpe != &tmpm->header) {
			if( _vm_object_in_map(tmpm, object, tmpe)) {
				return 1;
			}
			tmpe = tmpe->next;
		}
	} else if ((obj = entry->object.vm_object) != NULL) {
		for(; obj; obj=obj->backing_object)
			if( obj == object) {
				return 1;
			}
	}
	return 0;
}

static int
vm_object_in_map( object)
	vm_object_t object;
{
	struct proc *p;

	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		if( !p->p_vmspace /* || (p->p_flag & (P_SYSTEM|P_WEXIT)) */)
			continue;
		if( _vm_object_in_map(&p->p_vmspace->vm_map, object, 0)) {
			sx_sunlock(&allproc_lock);
			return 1;
		}
	}
	sx_sunlock(&allproc_lock);
	if( _vm_object_in_map( kernel_map, object, 0))
		return 1;
	if( _vm_object_in_map( kmem_map, object, 0))
		return 1;
	if( _vm_object_in_map( pager_map, object, 0))
		return 1;
	if( _vm_object_in_map( buffer_map, object, 0))
		return 1;
	if( _vm_object_in_map( mb_map, object, 0))
		return 1;
	return 0;
}

DB_SHOW_COMMAND(vmochk, vm_object_check)
{
	vm_object_t object;

	/*
	 * make sure that internal objs are in a map somewhere
	 * and none have zero ref counts.
	 */
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		if (object->handle == NULL &&
		    (object->type == OBJT_DEFAULT || object->type == OBJT_SWAP)) {
			if (object->ref_count == 0) {
				db_printf("vmochk: internal obj has zero ref count: %ld\n",
					(long)object->size);
			}
			if (!vm_object_in_map(object)) {
				db_printf(
			"vmochk: internal obj is not in a map: "
			"ref: %d, size: %lu: 0x%lx, backing_object: %p\n",
				    object->ref_count, (u_long)object->size, 
				    (u_long)object->size,
				    (void *)object->backing_object);
			}
		}
	}
}

/*
 *	vm_object_print:	[ debug ]
 */
DB_SHOW_COMMAND(object, vm_object_print_static)
{
	/* XXX convert args. */
	vm_object_t object = (vm_object_t)addr;
	boolean_t full = have_addr;

	vm_page_t p;

	/* XXX count is an (unused) arg.  Avoid shadowing it. */
#define	count	was_count

	int count;

	if (object == NULL)
		return;

	db_iprintf(
	    "Object %p: type=%d, size=0x%lx, res=%d, ref=%d, flags=0x%x\n",
	    object, (int)object->type, (u_long)object->size,
	    object->resident_page_count, object->ref_count, object->flags);
	/*
	 * XXX no %qd in kernel.  Truncate object->backing_object_offset.
	 */
	db_iprintf(" sref=%d, backing_object(%d)=(%p)+0x%lx\n",
	    object->shadow_count, 
	    object->backing_object ? object->backing_object->ref_count : 0,
	    object->backing_object, (long)object->backing_object_offset);

	if (!full)
		return;

	db_indent += 2;
	count = 0;
	TAILQ_FOREACH(p, &object->memq, listq) {
		if (count == 0)
			db_iprintf("memory:=");
		else if (count == 6) {
			db_printf("\n");
			db_iprintf(" ...");
			count = 0;
		} else
			db_printf(",");
		count++;

		db_printf("(off=0x%lx,page=0x%lx)",
		    (u_long) p->pindex, (u_long) VM_PAGE_TO_PHYS(p));
	}
	if (count != 0)
		db_printf("\n");
	db_indent -= 2;
}

/* XXX. */
#undef count

/* XXX need this non-static entry for calling from vm_map_print. */
void
vm_object_print(addr, have_addr, count, modif)
        /* db_expr_t */ long addr;
	boolean_t have_addr;
	/* db_expr_t */ long count;
	char *modif;
{
	vm_object_print_static(addr, have_addr, count, modif);
}

DB_SHOW_COMMAND(vmopag, vm_object_print_pages)
{
	vm_object_t object;
	int nl = 0;
	int c;

	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		vm_pindex_t idx, fidx;
		vm_pindex_t osize;
		vm_offset_t pa = -1, padiff;
		int rcount;
		vm_page_t m;

		db_printf("new object: %p\n", (void *)object);
		if ( nl > 18) {
			c = cngetc();
			if (c != ' ')
				return;
			nl = 0;
		}
		nl++;
		rcount = 0;
		fidx = 0;
		osize = object->size;
		if (osize > 128)
			osize = 128;
		for(idx=0;idx<osize;idx++) {
			m = vm_page_lookup(object, idx);
			if (m == NULL) {
				if (rcount) {
					db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
						(long)fidx, rcount, (long)pa);
					if ( nl > 18) {
						c = cngetc();
						if (c != ' ')
							return;
						nl = 0;
					}
					nl++;
					rcount = 0;
				}
				continue;
			}

				
			if (rcount &&
				(VM_PAGE_TO_PHYS(m) == pa + rcount * PAGE_SIZE)) {
				++rcount;
				continue;
			}
			if (rcount) {
				padiff = pa + rcount * PAGE_SIZE - VM_PAGE_TO_PHYS(m);
				padiff >>= PAGE_SHIFT;
				padiff &= PQ_L2_MASK;
				if (padiff == 0) {
					pa = VM_PAGE_TO_PHYS(m) - rcount * PAGE_SIZE;
					++rcount;
					continue;
				}
				db_printf(" index(%ld)run(%d)pa(0x%lx)",
					(long)fidx, rcount, (long)pa);
				db_printf("pd(%ld)\n", (long)padiff);
				if ( nl > 18) {
					c = cngetc();
					if (c != ' ')
						return;
					nl = 0;
				}
				nl++;
			}
			fidx = idx;
			pa = VM_PAGE_TO_PHYS(m);
			rcount = 1;
		}
		if (rcount) {
			db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
				(long)fidx, rcount, (long)pa);
			if ( nl > 18) {
				c = cngetc();
				if (c != ' ')
					return;
				nl = 0;
			}
			nl++;
		}
	}
}
#endif /* DDB */
