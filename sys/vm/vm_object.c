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
 * $Id: vm_object.c,v 1.117 1998/03/08 06:25:59 dyson Exp $
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

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

static void	vm_object_qcollapse __P((vm_object_t object));
static void vm_object_dispose __P((vm_object_t));

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
static struct simplelock vm_object_list_lock;
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
#define VM_OBJECTS_INIT 256
static struct vm_object vm_objects_init[VM_OBJECTS_INIT];

void
_vm_object_allocate(type, size, object)
	objtype_t type;
	vm_size_t size;
	register vm_object_t object;
{
	int incr;
	TAILQ_INIT(&object->memq);
	TAILQ_INIT(&object->shadow_head);

	object->type = type;
	object->size = size;
	object->ref_count = 1;
	object->flags = 0;
	object->behavior = OBJ_NORMAL;
	object->paging_in_progress = 0;
	object->resident_page_count = 0;
	object->cache_count = 0;
	object->wire_count = 0;
	object->shadow_count = 0;
	object->pg_color = next_index;
	if ( size > (PQ_L2_SIZE / 3 + PQ_PRIME1))
		incr = PQ_L2_SIZE / 3 + PQ_PRIME1;
	else
		incr = size;
	next_index = (next_index + incr) & PQ_L2_MASK;
	object->handle = NULL;
	object->paging_offset = (vm_ooffset_t) 0;
	object->backing_object = NULL;
	object->backing_object_offset = (vm_ooffset_t) 0;
	object->page_hint = NULL;

	object->last_read = 0;
	object->generation++;

	TAILQ_INSERT_TAIL(&vm_object_list, object, object_list);
	vm_object_count++;
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
	simple_lock_init(&vm_object_list_lock);
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
	register vm_object_t result;
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
	register vm_object_t object;
{
	if (object == NULL)
		return;

#if defined(DIAGNOSTIC)
	if (object->flags & OBJ_DEAD)
		panic("vm_object_reference: attempting to reference dead obj");
#endif

	object->ref_count++;
	if (object->type == OBJT_VNODE) {
		while (vget((struct vnode *) object->handle, LK_RETRY|LK_NOOBJ, curproc)) {
#if !defined(MAX_PERF)
			printf("vm_object_reference: delay in getting object\n");
#endif
		}
	}
}

void
vm_object_vndeallocate(object)
	vm_object_t object;
{
	struct vnode *vp = (struct vnode *) object->handle;
#if defined(DIAGNOSTIC)
	if (object->type != OBJT_VNODE)
		panic("vm_object_vndeallocate: not a vnode object");
	if (vp == NULL)
		panic("vm_object_vndeallocate: missing vp");
	if (object->ref_count == 0) {
		vprint("vm_object_vndeallocate", vp);
		panic("vm_object_vndeallocate: bad object reference count");
	}
#endif

	object->ref_count--;
	if (object->ref_count == 0) {
		vp->v_flag &= ~VTEXT;
		object->flags &= ~OBJ_OPT;
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
	int s;
	vm_object_t temp;

	while (object != NULL) {

		if (object->type == OBJT_VNODE) {
			vm_object_vndeallocate(object);
			return;
		}

		if (object->ref_count == 0) {
			panic("vm_object_deallocate: object deallocated too many times: %d", object->type);
		} else if (object->ref_count > 2) {
			object->ref_count--;
			return;
		}

		/*
		 * Here on ref_count of one or two, which are special cases for
		 * objects.
		 */
		if ((object->ref_count == 2) && (object->shadow_count == 1)) {

			object->ref_count--;
			if ((object->handle == NULL) &&
			    (object->type == OBJT_DEFAULT ||
			     object->type == OBJT_SWAP)) {
				vm_object_t robject;

				robject = TAILQ_FIRST(&object->shadow_head);
#if defined(DIAGNOSTIC)
				if (robject == NULL)
					panic("vm_object_deallocate: ref_count: %d,"
						  " shadow_count: %d",
						  object->ref_count, object->shadow_count);
#endif
				if ((robject->handle == NULL) &&
				    (robject->type == OBJT_DEFAULT ||
				     robject->type == OBJT_SWAP)) {

					robject->ref_count++;

			retry:
					if (robject->paging_in_progress ||
							object->paging_in_progress) {
						vm_object_pip_sleep(robject, "objde1");
						if (robject->paging_in_progress &&
							robject->type == OBJT_SWAP) {
							swap_pager_sync();
							goto retry;
						}

						vm_object_pip_sleep(object, "objde2");
						if (object->paging_in_progress &&
							object->type == OBJT_SWAP) {
							swap_pager_sync();
						}
						goto retry;
					}

					if( robject->ref_count == 1) {
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

		} else {
			object->ref_count--;
			if (object->ref_count != 0)
				return;
		}

doterm:

		temp = object->backing_object;
		if (temp) {
			TAILQ_REMOVE(&temp->shadow_head, object, shadow_list);
			temp->shadow_count--;
			if (temp->ref_count == 0)
				temp->flags &= ~OBJ_OPT;
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
 */
void
vm_object_terminate(object)
	register vm_object_t object;
{
	register vm_page_t p;
	int s;

	/*
	 * Make sure no one uses us.
	 */
	object->flags |= OBJ_DEAD;

	/*
	 * wait for the pageout daemon to be done with the object
	 */
	vm_object_pip_wait(object, "objtrm");

#if defined(DIAGNOSTIC)
	if (object->paging_in_progress != 0)
		panic("vm_object_terminate: pageout in progress");
#endif

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

	} else if (object->type != OBJT_DEAD) {

		/*
		 * Now free the pages. For internal objects, this also removes them
		 * from paging queues.
		 */
		while ((p = TAILQ_FIRST(&object->memq)) != NULL) {
#if !defined(MAX_PERF)
			if (p->busy || (p->flags & PG_BUSY))
				printf("vm_object_terminate: freeing busy page\n");
#endif
			p->flags |= PG_BUSY;
			vm_page_free(p);
			cnt.v_pfree++;
		}

	}

	if (object->type != OBJT_DEAD) {
		/*
		 * Let the pager know object is dead.
		 */
		vm_pager_deallocate(object);
	}

	if (object->ref_count == 0) {
		if ((object->type != OBJT_DEAD) || (object->resident_page_count == 0))
			vm_object_dispose(object);
	}
}

/*
 * vm_object_dispose
 *
 * Dispose the object.
 */
static void
vm_object_dispose(object)
	vm_object_t object;
{
		simple_lock(&vm_object_list_lock);
		TAILQ_REMOVE(&vm_object_list, object, object_list);
		vm_object_count--;
		simple_unlock(&vm_object_list_lock);
		/*
   		* Free the space for the object.
   		*/
		zfree(obj_zone, object);
		wakeup(object);
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
vm_object_page_clean(object, start, end, flags)
	vm_object_t object;
	vm_pindex_t start;
	vm_pindex_t end;
	int flags;
{
	register vm_page_t p, np, tp;
	register vm_offset_t tstart, tend;
	vm_pindex_t pi;
	int s;
	struct vnode *vp;
	int runlen;
	int maxf;
	int chkb;
	int maxb;
	int i;
	int pagerflags;
	vm_page_t maf[vm_pageout_page_count];
	vm_page_t mab[vm_pageout_page_count];
	vm_page_t ma[vm_pageout_page_count];
	int curgeneration;
	struct proc *pproc = curproc;	/* XXX */

	if (object->type != OBJT_VNODE ||
		(object->flags & OBJ_MIGHTBEDIRTY) == 0)
		return;

	pagerflags = (flags & (OBJPC_SYNC | OBJPC_INVAL)) ? VM_PAGER_PUT_SYNC : 0;
	pagerflags |= (flags & OBJPC_INVAL) ? VM_PAGER_PUT_INVAL : 0;

	vp = object->handle;

	object->flags |= OBJ_CLEANING;

	tstart = start;
	if (end == 0) {
		tend = object->size;
	} else {
		tend = end;
	}

	for(p = TAILQ_FIRST(&object->memq); p; p = TAILQ_NEXT(p, listq)) {
		p->flags |= PG_CLEANCHK;
		vm_page_protect(p, VM_PROT_READ);
	}

	if ((tstart == 0) && (tend == object->size)) {
		object->flags &= ~(OBJ_WRITEABLE|OBJ_MIGHTBEDIRTY);
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
			p->flags &= ~PG_CLEANCHK;
			continue;
		}

		vm_page_test_dirty(p);
		if ((p->dirty & p->valid) == 0) {
			p->flags &= ~PG_CLEANCHK;
			continue;
		}

		s = splvm();
		while ((p->flags & PG_BUSY) || p->busy) {
			p->flags |= PG_WANTED | PG_REFERENCED;
			tsleep(p, PVM, "vpcwai", 0);
			if (object->generation != curgeneration) {
				splx(s);
				goto rescan;
			}
		}

		maxf = 0;
		for(i=1;i<vm_pageout_page_count;i++) {
			if (tp = vm_page_lookup(object, pi + i)) {
				if ((tp->flags & PG_BUSY) ||
					(tp->flags & PG_CLEANCHK) == 0 ||
					(tp->busy != 0))
					break;
				if((tp->queue - tp->pc) == PQ_CACHE) {
					tp->flags &= ~PG_CLEANCHK;
					break;
				}
				vm_page_test_dirty(tp);
				if ((tp->dirty & tp->valid) == 0) {
					tp->flags &= ~PG_CLEANCHK;
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
				if (tp = vm_page_lookup(object, pi - i)) {
					if ((tp->flags & PG_BUSY) ||
						(tp->flags & PG_CLEANCHK) == 0 ||
						(tp->busy != 0))
						break;
					if((tp->queue - tp->pc) == PQ_CACHE) {
						tp->flags &= ~PG_CLEANCHK;
						break;
					}
					vm_page_test_dirty(tp);
					if ((tp->dirty & tp->valid) == 0) {
						tp->flags &= ~PG_CLEANCHK;
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
			ma[index]->flags &= ~PG_CLEANCHK;
		}
		p->flags &= ~PG_CLEANCHK;
		ma[maxb] = p;
		for(i=0;i<maxf;i++) {
			int index = (maxb + i) + 1;
			ma[index] = maf[i];
			ma[index]->flags &= ~PG_CLEANCHK;
		}
		runlen = maxb + maxf + 1;
		splx(s);
		vm_pageout_flush(ma, runlen, pagerflags);
		if (object->generation != curgeneration)
			goto rescan;
	}

	VOP_FSYNC(vp, NULL, (pagerflags & VM_PAGER_PUT_SYNC)?1:0, curproc);

	object->flags &= ~OBJ_CLEANING;
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
	register vm_object_t object;
{
	register vm_page_t p, next;

	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = next) {
		next = TAILQ_NEXT(p, listq);
		vm_page_deactivate(p);
	}
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
	register vm_object_t object;
	register vm_pindex_t start;
	register vm_pindex_t end;
{
	register vm_page_t p;

	if (object == NULL || (object->flags & OBJ_WRITEABLE) == 0)
		return;

	for (p = TAILQ_FIRST(&object->memq);
		p != NULL;
		p = TAILQ_NEXT(p, listq)) {
		vm_page_protect(p, VM_PROT_READ);
	}

	object->flags &= ~OBJ_WRITEABLE;
}

/*
 * Same as vm_object_pmap_copy_1, except range checking really
 * works, and is meant for small sections of an object.
 */
void
vm_object_pmap_copy_1(object, start, end)
	register vm_object_t object;
	register vm_pindex_t start;
	register vm_pindex_t end;
{
	vm_pindex_t idx;
	register vm_page_t p;

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
	register vm_object_t object;
	register vm_pindex_t start;
	register vm_pindex_t end;
{
	register vm_page_t p;
	if (object == NULL)
		return;
	for (p = TAILQ_FIRST(&object->memq);
		p != NULL;
		p = TAILQ_NEXT(p, listq)) {
		if (p->pindex >= start && p->pindex < end)
			vm_page_protect(p, VM_PROT_NONE);
	}
	if ((start == 0) && (object->size == end))
		object->flags &= ~OBJ_WRITEABLE;
}

/*
 *	vm_object_madvise:
 *
 *	Implements the madvise function at the object/page level.
 */
void
vm_object_madvise(object, pindex, count, advise)
	vm_object_t object;
	vm_pindex_t pindex;
	int count;
	int advise;
{
	int s;
	vm_pindex_t end, tpindex;
	vm_object_t tobject;
	vm_page_t m;

	if (object == NULL)
		return;

	end = pindex + count;

	for (; pindex < end; pindex += 1) {

relookup:
		tobject = object;
		tpindex = pindex;
shadowlookup:
		m = vm_page_lookup(tobject, tpindex);
		if (m == NULL) {
			if (tobject->type != OBJT_DEFAULT) {
				continue;
			}
				
			tobject = tobject->backing_object;
			if ((tobject == NULL) || (tobject->ref_count != 1)) {
				continue;
			}
			tpindex += OFF_TO_IDX(tobject->backing_object_offset);
			goto shadowlookup;
		}

		/*
		 * If the page is busy or not in a normal active state,
		 * we skip it.  Things can break if we mess with pages
		 * in any of the below states.
		 */
		if (m->hold_count || m->wire_count ||
			m->valid != VM_PAGE_BITS_ALL) {
			continue;
		}

 		if (vm_page_sleep(m, "madvpo", &m->busy))
  			goto relookup;

		if (advise == MADV_WILLNEED) {
			vm_page_activate(m);
		} else if (advise == MADV_DONTNEED) {
			vm_page_deactivate(m);
		} else if (advise == MADV_FREE) {
			pmap_clear_modify(VM_PAGE_TO_PHYS(m));
			m->dirty = 0;
			/*
			 * Force a demand zero if attempt to read from swap.
			 * We currently don't handle vnode files correctly,
			 * and will reread stale contents unnecessarily.
			 */
			if (object->type == OBJT_SWAP)
				swap_pager_dmzspace(tobject, m->pindex, 1);
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
	register vm_object_t source;
	register vm_object_t result;

	source = *object;

	/*
	 * Allocate a new object with the given length
	 */

	if ((result = vm_object_allocate(OBJT_DEFAULT, length)) == NULL)
		panic("vm_object_shadow: no object for shadowing");

	/*
	 * The new object shadows the source object, adding a reference to it.
	 * Our caller changes his reference to point to the new object,
	 * removing a reference to the source object.  Net result: no change
	 * of reference count.
	 */
	result->backing_object = source;
	if (source) {
		TAILQ_INSERT_TAIL(&source->shadow_head, result, shadow_list);
		source->shadow_count++;
		source->generation++;
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


/*
 * this version of collapse allows the operation to occur earlier and
 * when paging_in_progress is true for an object...  This is not a complete
 * operation, but should plug 99.9% of the rest of the leaks.
 */
static void
vm_object_qcollapse(object)
	register vm_object_t object;
{
	register vm_object_t backing_object;
	register vm_pindex_t backing_offset_index, paging_offset_index;
	vm_pindex_t backing_object_paging_offset_index;
	vm_pindex_t new_pindex;
	register vm_page_t p, pp;
	register vm_size_t size;

	backing_object = object->backing_object;
	if (backing_object->ref_count != 1)
		return;

	backing_object->ref_count += 2;

	backing_offset_index = OFF_TO_IDX(object->backing_object_offset);
	backing_object_paging_offset_index = OFF_TO_IDX(backing_object->paging_offset);
	paging_offset_index = OFF_TO_IDX(object->paging_offset);
	size = object->size;
	p = TAILQ_FIRST(&backing_object->memq);
	while (p) {
		vm_page_t next;

		next = TAILQ_NEXT(p, listq);
		if ((p->flags & (PG_BUSY | PG_FICTITIOUS)) ||
		    !p->valid || p->hold_count || p->wire_count || p->busy) {
			p = next;
			continue;
		}
		p->flags |= PG_BUSY;

		new_pindex = p->pindex - backing_offset_index;
		if (p->pindex < backing_offset_index ||
		    new_pindex >= size) {
			if (backing_object->type == OBJT_SWAP)
				swap_pager_freespace(backing_object,
				    backing_object_paging_offset_index+p->pindex,
				    1);
			vm_page_protect(p, VM_PROT_NONE);
			vm_page_free(p);
		} else {
			pp = vm_page_lookup(object, new_pindex);
			if (pp != NULL ||
				(object->type == OBJT_SWAP && vm_pager_has_page(object,
				    paging_offset_index + new_pindex, NULL, NULL))) {
				if (backing_object->type == OBJT_SWAP)
					swap_pager_freespace(backing_object,
					    backing_object_paging_offset_index + p->pindex, 1);
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_free(p);
			} else {
				if (backing_object->type == OBJT_SWAP)
					swap_pager_freespace(backing_object,
					    backing_object_paging_offset_index + p->pindex, 1);

				if ((p->queue - p->pc) == PQ_CACHE)
					vm_page_deactivate(p);
				else
					vm_page_protect(p, VM_PROT_NONE);

				vm_page_rename(p, object, new_pindex);
				p->dirty = VM_PAGE_BITS_ALL;
			}
		}
		p = next;
	}
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
	vm_object_t backing_object;
	vm_ooffset_t backing_offset;
	vm_size_t size;
	vm_pindex_t new_pindex, backing_offset_index;
	vm_page_t p, pp;

	while (TRUE) {
		/*
		 * Verify that the conditions are right for collapse:
		 *
		 * The object exists and no pages in it are currently being paged
		 * out.
		 */
		if (object == NULL)
			return;

		/*
		 * Make sure there is a backing object.
		 */
		if ((backing_object = object->backing_object) == NULL)
			return;

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
			return;
		}

		if (object->paging_in_progress != 0 ||
		    backing_object->paging_in_progress != 0) {
			vm_object_qcollapse(object);
			return;
		}

		/*
		 * We know that we can either collapse the backing object (if
		 * the parent is the only reference to it) or (perhaps) remove
		 * the parent's reference to it.
		 */

		backing_offset = object->backing_object_offset;
		backing_offset_index = OFF_TO_IDX(backing_offset);
		size = object->size;

		/*
		 * If there is exactly one reference to the backing object, we
		 * can collapse it into the parent.
		 */

		if (backing_object->ref_count == 1) {

			backing_object->flags |= OBJ_DEAD;
			/*
			 * We can collapse the backing object.
			 *
			 * Move all in-memory pages from backing_object to the
			 * parent.  Pages that have been paged out will be
			 * overwritten by any of the parent's pages that
			 * shadow them.
			 */

			while ((p = TAILQ_FIRST(&backing_object->memq)) != 0) {

				new_pindex = p->pindex - backing_offset_index;
				p->flags |= PG_BUSY;

				/*
				 * If the parent has a page here, or if this
				 * page falls outside the parent, dispose of
				 * it.
				 *
				 * Otherwise, move it as planned.
				 */

				if (p->pindex < backing_offset_index ||
				    new_pindex >= size) {
					vm_page_protect(p, VM_PROT_NONE);
					vm_page_free(p);
				} else {
					pp = vm_page_lookup(object, new_pindex);
					if (pp != NULL || (object->type == OBJT_SWAP && vm_pager_has_page(object,
					    OFF_TO_IDX(object->paging_offset) + new_pindex, NULL, NULL))) {
						vm_page_protect(p, VM_PROT_NONE);
						vm_page_free(p);
					} else {
						if ((p->queue - p->pc) == PQ_CACHE)
							vm_page_deactivate(p);
						else
							vm_page_protect(p, VM_PROT_NONE);
						vm_page_rename(p, object, new_pindex);
						p->dirty = VM_PAGE_BITS_ALL;
					}
				}
			}

			/*
			 * Move the pager from backing_object to object.
			 */

			if (backing_object->type == OBJT_SWAP) {
				backing_object->paging_in_progress++;
				if (object->type == OBJT_SWAP) {
					object->paging_in_progress++;
					/*
					 * copy shadow object pages into ours
					 * and destroy unneeded pages in
					 * shadow object.
					 */
					swap_pager_copy(
					    backing_object,
					    OFF_TO_IDX(backing_object->paging_offset),
					    object,
					    OFF_TO_IDX(object->paging_offset),
					    OFF_TO_IDX(object->backing_object_offset));
					vm_object_pip_wakeup(object);
				} else {
					object->paging_in_progress++;
					/*
					 * move the shadow backing_object's pager data to
					 * "object" and convert "object" type to OBJT_SWAP.
					 */
					object->type = OBJT_SWAP;
					object->un_pager.swp.swp_nblocks =
					    backing_object->un_pager.swp.swp_nblocks;
					object->un_pager.swp.swp_allocsize =
					    backing_object->un_pager.swp.swp_allocsize;
					object->un_pager.swp.swp_blocks =
					    backing_object->un_pager.swp.swp_blocks;
					object->un_pager.swp.swp_poip =		/* XXX */
					    backing_object->un_pager.swp.swp_poip;
					object->paging_offset = backing_object->paging_offset + backing_offset;
					TAILQ_INSERT_TAIL(&swap_pager_un_object_list, object, pager_object_list);

					/*
					 * Convert backing object from OBJT_SWAP to
					 * OBJT_DEFAULT. XXX - only the TAILQ_REMOVE is
					 * actually necessary.
					 */
					backing_object->type = OBJT_DEFAULT;
					TAILQ_REMOVE(&swap_pager_un_object_list, backing_object, pager_object_list);
					/*
					 * free unnecessary blocks
					 */
					swap_pager_freespace(object, 0,
						OFF_TO_IDX(object->paging_offset));
					vm_object_pip_wakeup(object);
				}

				vm_object_pip_wakeup(backing_object);
			}
			/*
			 * Object now shadows whatever backing_object did.
			 * Note that the reference to backing_object->backing_object
			 * moves from within backing_object to within object.
			 */

			TAILQ_REMOVE(&object->backing_object->shadow_head, object,
			    shadow_list);
			object->backing_object->shadow_count--;
			object->backing_object->generation++;
			if (backing_object->backing_object) {
				TAILQ_REMOVE(&backing_object->backing_object->shadow_head,
				    backing_object, shadow_list);
				backing_object->backing_object->shadow_count--;
				backing_object->backing_object->generation++;
			}
			object->backing_object = backing_object->backing_object;
			if (object->backing_object) {
				TAILQ_INSERT_TAIL(&object->backing_object->shadow_head,
				    object, shadow_list);
				object->backing_object->shadow_count++;
				object->backing_object->generation++;
			}

			object->backing_object_offset += backing_object->backing_object_offset;
			/*
			 * Discard backing_object.
			 *
			 * Since the backing object has no pages, no pager left,
			 * and no object references within it, all that is
			 * necessary is to dispose of it.
			 */

			TAILQ_REMOVE(&vm_object_list, backing_object,
			    object_list);
			vm_object_count--;

			zfree(obj_zone, backing_object);

			object_collapses++;
		} else {
			vm_object_t new_backing_object;
			/*
			 * If all of the pages in the backing object are
			 * shadowed by the parent object, the parent object no
			 * longer has to shadow the backing object; it can
			 * shadow the next one in the chain.
			 *
			 * The backing object must not be paged out - we'd have
			 * to check all of the paged-out pages, as well.
			 */

			if (backing_object->type != OBJT_DEFAULT) {
				return;
			}
			/*
			 * Should have a check for a 'small' number of pages
			 * here.
			 */

			for (p = TAILQ_FIRST(&backing_object->memq); p;
					p = TAILQ_NEXT(p, listq)) {

				new_pindex = p->pindex - backing_offset_index;
				p->flags |= PG_BUSY;

				/*
				 * If the parent has a page here, or if this
				 * page falls outside the parent, keep going.
				 *
				 * Otherwise, the backing_object must be left in
				 * the chain.
				 */

				if (p->pindex >= backing_offset_index &&
					new_pindex <= size) {

					pp = vm_page_lookup(object, new_pindex);

					if ((pp == NULL) || (pp->flags & PG_BUSY) || pp->busy) {
						PAGE_WAKEUP(p);
						return;
					}

					pp->flags |= PG_BUSY;
					if ((pp->valid == 0) &&
				   	    !vm_pager_has_page(object, OFF_TO_IDX(object->paging_offset) + new_pindex, NULL, NULL)) {
						/*
						 * Page still needed. Can't go any
						 * further.
						 */
						PAGE_WAKEUP(pp);
						PAGE_WAKEUP(p);
						return;
					}
					PAGE_WAKEUP(pp);
				}
				PAGE_WAKEUP(p);
			}

			/*
			 * Make the parent shadow the next object in the
			 * chain.  Deallocating backing_object will not remove
			 * it, since its reference count is at least 2.
			 */

			TAILQ_REMOVE(&backing_object->shadow_head,
			    object, shadow_list);
			backing_object->shadow_count--;
			backing_object->generation++;

			new_backing_object = backing_object->backing_object;
			if (object->backing_object = new_backing_object) {
				vm_object_reference(new_backing_object);
				TAILQ_INSERT_TAIL(&new_backing_object->shadow_head,
				    object, shadow_list);
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
	register vm_object_t object;
	register vm_pindex_t start;
	register vm_pindex_t end;
	boolean_t clean_only;
{
	register vm_page_t p, next;
	unsigned int size;
	int s, all;

	if (object == NULL)
		return;

	all = ((end == 0) && (start == 0));

	object->paging_in_progress++;
again:
	size = end - start;
	if (all || size > 4 || size >= object->size / 4) {
		for (p = TAILQ_FIRST(&object->memq); p != NULL; p = next) {
			next = TAILQ_NEXT(p, listq);
			if (all || ((start <= p->pindex) && (p->pindex < end))) {
				if (p->wire_count != 0) {
					vm_page_protect(p, VM_PROT_NONE);
					p->valid = 0;
					continue;
				}

				/*
				 * The busy flags are only cleared at
				 * interrupt -- minimize the spl transitions
				 */

 				if (vm_page_sleep(p, "vmopar", &p->busy))
 					goto again;

				if (clean_only && p->valid) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty)
						continue;
				}

				p->flags |= PG_BUSY;
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_free(p);
			}
		}
	} else {
		while (size > 0) {
			if ((p = vm_page_lookup(object, start)) != 0) {

				if (p->wire_count != 0) {
					p->valid = 0;
					vm_page_protect(p, VM_PROT_NONE);
					start += 1;
					size -= 1;
					continue;
				}

				/*
				 * The busy flags are only cleared at
				 * interrupt -- minimize the spl transitions
				 */
 				if (vm_page_sleep(p, "vmopar", &p->busy))
					goto again;

				if (clean_only && p->valid) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty) {
						start += 1;
						size -= 1;
						continue;
					}
				}

				p->flags |= PG_BUSY;
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
	register vm_object_t prev_object;
	vm_pindex_t prev_pindex;
	vm_size_t prev_size, next_size;
{
	vm_size_t newsize;

	if (prev_object == NULL) {
		return (TRUE);
	}

	if (prev_object->type != OBJT_DEFAULT) {
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

	if ((prev_object->ref_count > 1) &&
	    (prev_object->size != prev_pindex + prev_size)) {
		return (FALSE);
	}

	/*
	 * Remove any pages that may still be in the object from a previous
	 * deallocation.
	 */

	vm_object_page_remove(prev_object,
	    prev_pindex + prev_size,
	    prev_pindex + prev_size + next_size, FALSE);

	/*
	 * Extend the object if necessary.
	 */
	newsize = prev_pindex + prev_size + next_size;
	if (newsize > prev_object->size)
		prev_object->size = newsize;

	return (TRUE);
}

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <machine/cons.h>

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
	} else if (entry->eflags & (MAP_ENTRY_IS_A_MAP|MAP_ENTRY_IS_SUB_MAP)) {
		tmpm = entry->object.share_map;
		tmpe = tmpm->header.next;
		entcount = tmpm->nentries;
		while (entcount-- && tmpe != &tmpm->header) {
			if( _vm_object_in_map(tmpm, object, tmpe)) {
				return 1;
			}
			tmpe = tmpe->next;
		}
	} else if (obj = entry->object.vm_object) {
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
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if( !p->p_vmspace /* || (p->p_flag & (P_SYSTEM|P_WEXIT)) */)
			continue;
		if( _vm_object_in_map(&p->p_vmspace->vm_map, object, 0))
			return 1;
	}
	if( _vm_object_in_map( kernel_map, object, 0))
		return 1;
	if( _vm_object_in_map( kmem_map, object, 0))
		return 1;
	if( _vm_object_in_map( pager_map, object, 0))
		return 1;
	if( _vm_object_in_map( buffer_map, object, 0))
		return 1;
	if( _vm_object_in_map( io_map, object, 0))
		return 1;
	if( _vm_object_in_map( phys_map, object, 0))
		return 1;
	if( _vm_object_in_map( mb_map, object, 0))
		return 1;
	if( _vm_object_in_map( u_map, object, 0))
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
	for (object = TAILQ_FIRST(&vm_object_list);
			object != NULL;
			object = TAILQ_NEXT(object, object_list)) {
		if (object->handle == NULL &&
		    (object->type == OBJT_DEFAULT || object->type == OBJT_SWAP)) {
			if (object->ref_count == 0) {
				db_printf("vmochk: internal obj has zero ref count: %d\n",
					object->size);
			}
			if (!vm_object_in_map(object)) {
				db_printf("vmochk: internal obj is not in a map: "
		"ref: %d, size: %d: 0x%x, backing_object: 0x%x\n",
				    object->ref_count, object->size, 
				    object->size, object->backing_object);
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

	register vm_page_t p;

	/* XXX count is an (unused) arg.  Avoid shadowing it. */
#define	count	was_count

	register int count;

	if (object == NULL)
		return;

	db_iprintf("Object 0x%x: type=%d, size=0x%x, res=%d, ref=%d, flags=0x%x\n",
	    (int) object, (int) object->type, (int) object->size,
	    object->resident_page_count,
		object->ref_count,
		object->flags);
	db_iprintf(" sref=%d, offset=0x%x, backing_object(%d)=(0x%x)+0x%x\n",
		object->shadow_count,
	    (int) object->paging_offset,
		(((int)object->backing_object)?object->backing_object->ref_count:0),
	    (int) object->backing_object,
		(int) object->backing_object_offset);

	if (!full)
		return;

	db_indent += 2;
	count = 0;
	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = TAILQ_NEXT(p, listq)) {
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
	db_expr_t addr;
	boolean_t have_addr;
	db_expr_t count;
	char *modif;
{
	vm_object_print_static(addr, have_addr, count, modif);
}

DB_SHOW_COMMAND(vmopag, vm_object_print_pages)
{
	vm_object_t object;
	int nl = 0;
	int c;
	for (object = TAILQ_FIRST(&vm_object_list);
			object != NULL;
			object = TAILQ_NEXT(object, object_list)) {
		vm_pindex_t idx, fidx;
		vm_pindex_t osize;
		vm_offset_t pa = -1, padiff;
		int rcount;
		vm_page_t m;

		db_printf("new object: 0x%x\n", object);
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
					db_printf(" index(%d)run(%d)pa(0x%x)\n",
						fidx, rcount, pa);
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
				db_printf(" index(%d)run(%d)pa(0x%x)", fidx, rcount, pa);
				db_printf("pd(%d)\n", padiff);
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
			db_printf(" index(%d)run(%d)pa(0x%x)\n", fidx, rcount, pa);
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
