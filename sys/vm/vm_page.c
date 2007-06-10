/*-
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
 */

/*-
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
 *			GENERAL RULES ON VM_PAGE MANIPULATION
 *
 *	- a pageq mutex is required when adding or removing a page from a
 *	  page queue (vm_page_queue[]), regardless of other mutexes or the
 *	  busy state of a page.
 *
 *	- a hash chain mutex is required when associating or disassociating
 *	  a page from the VM PAGE CACHE hash table (vm_page_buckets),
 *	  regardless of other mutexes or the busy state of a page.
 *
 *	- either a hash chain mutex OR a busied page is required in order
 *	  to modify the page flags.  A hash chain mutex must be obtained in
 *	  order to busy a page.  A page's flags cannot be modified by a
 *	  hash chain mutex if the page is marked busy.
 *
 *	- The object memq mutex is held when inserting or removing
 *	  pages from an object (vm_page_insert() or vm_page_remove()).  This
 *	  is different from the object's main mutex.
 *
 *	Generally speaking, you have to be aware of side effects when running
 *	vm_page ops.  A vm_page_lookup() will return with the hash chain
 *	locked, whether it was able to lookup the page or not.  vm_page_free(),
 *	vm_page_cache(), vm_page_activate(), and a number of other routines
 *	will release the hash chain mutex for you.  Intermediate manipulation
 *	routines such as vm_page_flag_set() expect the hash chain to be held
 *	on entry and the hash chain will remain held on return.
 *
 *	pageq scanning can only occur with the pageq in question locked.
 *	We have a known bottleneck with the active queue, but the cache
 *	and free queues are actually arrays already. 
 */

/*
 *	Resident memory management module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/md_var.h>

/*
 *	Associated with page of user-allocatable memory is a
 *	page structure.
 */

struct mtx vm_page_queue_mtx;
struct mtx vm_page_queue_free_mtx;

vm_page_t vm_page_array = 0;
int vm_page_array_size = 0;
long first_page = 0;
int vm_page_zero_count = 0;

static int boot_pages = UMA_BOOT_PAGES;
TUNABLE_INT("vm.boot_pages", &boot_pages);
SYSCTL_INT(_vm, OID_AUTO, boot_pages, CTLFLAG_RD, &boot_pages, 0,
	"number of pages allocated for bootstrapping the VM system");

/*
 *	vm_set_page_size:
 *
 *	Sets the page size, perhaps based upon the memory
 *	size.  Must be called before any use of page-size
 *	dependent functions.
 */
void
vm_set_page_size(void)
{
	if (cnt.v_page_size == 0)
		cnt.v_page_size = PAGE_SIZE;
	if (((cnt.v_page_size - 1) & cnt.v_page_size) != 0)
		panic("vm_set_page_size: page size not a power of two");
}

/*
 *	vm_page_blacklist_lookup:
 *
 *	See if a physical address in this page has been listed
 *	in the blacklist tunable.  Entries in the tunable are
 *	separated by spaces or commas.  If an invalid integer is
 *	encountered then the rest of the string is skipped.
 */
static int
vm_page_blacklist_lookup(char *list, vm_paddr_t pa)
{
	vm_paddr_t bad;
	char *cp, *pos;

	for (pos = list; *pos != '\0'; pos = cp) {
		bad = strtoq(pos, &cp, 0);
		if (*cp != '\0') {
			if (*cp == ' ' || *cp == ',') {
				cp++;
				if (cp == pos)
					continue;
			} else
				break;
		}
		if (pa == trunc_page(bad))
			return (1);
	}
	return (0);
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
vm_offset_t
vm_page_startup(vm_offset_t vaddr)
{
	vm_offset_t mapped;
	vm_size_t npages;
	vm_paddr_t page_range;
	vm_paddr_t new_end;
	int i;
	vm_paddr_t pa;
	int nblocks;
	vm_paddr_t last_pa;
	char *list;

	/* the biggest memory array is the second group of pages */
	vm_paddr_t end;
	vm_paddr_t biggestsize;
	vm_paddr_t low_water, high_water;
	int biggestone;

	vm_paddr_t total;

	total = 0;
	biggestsize = 0;
	biggestone = 0;
	nblocks = 0;
	vaddr = round_page(vaddr);

	for (i = 0; phys_avail[i + 1]; i += 2) {
		phys_avail[i] = round_page(phys_avail[i]);
		phys_avail[i + 1] = trunc_page(phys_avail[i + 1]);
	}

	low_water = phys_avail[0];
	high_water = phys_avail[1];

	for (i = 0; phys_avail[i + 1]; i += 2) {
		vm_paddr_t size = phys_avail[i + 1] - phys_avail[i];

		if (size > biggestsize) {
			biggestone = i;
			biggestsize = size;
		}
		if (phys_avail[i] < low_water)
			low_water = phys_avail[i];
		if (phys_avail[i + 1] > high_water)
			high_water = phys_avail[i + 1];
		++nblocks;
		total += size;
	}

	end = phys_avail[biggestone+1];

	/*
	 * Initialize the locks.
	 */
	mtx_init(&vm_page_queue_mtx, "vm page queue mutex", NULL, MTX_DEF |
	    MTX_RECURSE);
	mtx_init(&vm_page_queue_free_mtx, "vm page queue free mutex", NULL,
	    MTX_DEF);

	/*
	 * Initialize the queue headers for the free queue, the active queue
	 * and the inactive queue.
	 */
	vm_pageq_init();

	/*
	 * Allocate memory for use when boot strapping the kernel memory
	 * allocator.
	 */
	new_end = end - (boot_pages * UMA_SLAB_SIZE);
	new_end = trunc_page(new_end);
	mapped = pmap_map(&vaddr, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);
	bzero((void *)mapped, end - new_end);
	uma_startup((void *)mapped, boot_pages);

#if defined(__amd64__) || defined(__i386__)
	/*
	 * Allocate a bitmap to indicate that a random physical page
	 * needs to be included in a minidump.
	 *
	 * The amd64 port needs this to indicate which direct map pages
	 * need to be dumped, via calls to dump_add_page()/dump_drop_page().
	 *
	 * However, i386 still needs this workspace internally within the
	 * minidump code.  In theory, they are not needed on i386, but are
	 * included should the sf_buf code decide to use them.
	 */
	page_range = phys_avail[(nblocks - 1) * 2 + 1] / PAGE_SIZE;
	vm_page_dump_size = round_page(roundup2(page_range, NBBY) / NBBY);
	new_end -= vm_page_dump_size;
	vm_page_dump = (void *)(uintptr_t)pmap_map(&vaddr, new_end,
	    new_end + vm_page_dump_size, VM_PROT_READ | VM_PROT_WRITE);
	bzero((void *)vm_page_dump, vm_page_dump_size);
#endif
	/*
	 * Compute the number of pages of memory that will be available for
	 * use (taking into account the overhead of a page structure per
	 * page).
	 */
	first_page = low_water / PAGE_SIZE;
#ifdef VM_PHYSSEG_SPARSE
	page_range = 0;
	for (i = 0; phys_avail[i + 1] != 0; i += 2)
		page_range += atop(phys_avail[i + 1] - phys_avail[i]);
#elif defined(VM_PHYSSEG_DENSE)
	page_range = high_water / PAGE_SIZE - first_page;
#else
#error "Either VM_PHYSSEG_DENSE or VM_PHYSSEG_SPARSE must be defined."
#endif
	npages = (total - (page_range * sizeof(struct vm_page)) -
	    (end - new_end)) / PAGE_SIZE;
	end = new_end;

	/*
	 * Reserve an unmapped guard page to trap access to vm_page_array[-1].
	 */
	vaddr += PAGE_SIZE;

	/*
	 * Initialize the mem entry structures now, and put them in the free
	 * queue.
	 */
	new_end = trunc_page(end - page_range * sizeof(struct vm_page));
	mapped = pmap_map(&vaddr, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);
	vm_page_array = (vm_page_t) mapped;
#ifdef __amd64__
	/*
	 * pmap_map on amd64 comes out of the direct-map, not kvm like i386,
	 * so the pages must be tracked for a crashdump to include this data.
	 * This includes the vm_page_array and the early UMA bootstrap pages.
	 */
	for (pa = new_end; pa < phys_avail[biggestone + 1]; pa += PAGE_SIZE)
		dump_add_page(pa);
#endif	
	phys_avail[biggestone + 1] = new_end;

	/*
	 * Clear all of the page structures
	 */
	bzero((caddr_t) vm_page_array, page_range * sizeof(struct vm_page));
	vm_page_array_size = page_range;

	/*
	 * This assertion tests the hypothesis that npages and total are
	 * redundant.  XXX
	 */
	page_range = 0;
	for (i = 0; phys_avail[i + 1] != 0; i += 2)
		page_range += atop(phys_avail[i + 1] - phys_avail[i]);
	KASSERT(page_range == npages,
	    ("vm_page_startup: inconsistent page counts"));

	/*
	 * Construct the free queue(s) in descending order (by physical
	 * address) so that the first 16MB of physical memory is allocated
	 * last rather than first.  On large-memory machines, this avoids
	 * the exhaustion of low physical memory before isa_dma_init has run.
	 */
	cnt.v_page_count = 0;
	cnt.v_free_count = 0;
	list = getenv("vm.blacklist");
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		pa = phys_avail[i];
		last_pa = phys_avail[i + 1];
		while (pa < last_pa) {
			if (list != NULL &&
			    vm_page_blacklist_lookup(list, pa))
				printf("Skipping page with pa 0x%jx\n",
				    (uintmax_t)pa);
			else
				vm_pageq_add_new_page(pa);
			pa += PAGE_SIZE;
		}
	}
	freeenv(list);
	return (vaddr);
}

void
vm_page_flag_set(vm_page_t m, unsigned short bits)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	m->flags |= bits;
} 

void
vm_page_flag_clear(vm_page_t m, unsigned short bits)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	m->flags &= ~bits;
}

void
vm_page_busy(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT((m->oflags & VPO_BUSY) == 0,
	    ("vm_page_busy: page already busy!!!"));
	m->oflags |= VPO_BUSY;
}

/*
 *      vm_page_flash:
 *
 *      wakeup anyone waiting for the page.
 */
void
vm_page_flash(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (m->oflags & VPO_WANTED) {
		m->oflags &= ~VPO_WANTED;
		wakeup(m);
	}
}

/*
 *      vm_page_wakeup:
 *
 *      clear the VPO_BUSY flag and wakeup anyone waiting for the
 *      page.
 *
 */
void
vm_page_wakeup(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT(m->oflags & VPO_BUSY, ("vm_page_wakeup: page not busy!!!"));
	m->oflags &= ~VPO_BUSY;
	vm_page_flash(m);
}

void
vm_page_io_start(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	m->busy++;
}

void
vm_page_io_finish(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	m->busy--;
	if (m->busy == 0)
		vm_page_flash(m);
}

/*
 * Keep page from being freed by the page daemon
 * much of the same effect as wiring, except much lower
 * overhead and should be used only for *very* temporary
 * holding ("wiring").
 */
void
vm_page_hold(vm_page_t mem)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
        mem->hold_count++;
}

void
vm_page_unhold(vm_page_t mem)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	--mem->hold_count;
	KASSERT(mem->hold_count >= 0, ("vm_page_unhold: hold count < 0!!!"));
	if (mem->hold_count == 0 && VM_PAGE_INQUEUE2(mem, PQ_HOLD))
		vm_page_free_toq(mem);
}

/*
 *	vm_page_free:
 *
 *	Free a page.
 */
void
vm_page_free(vm_page_t m)
{

	m->flags &= ~PG_ZERO;
	vm_page_free_toq(m);
}

/*
 *	vm_page_free_zero:
 *
 *	Free a page to the zerod-pages queue
 */
void
vm_page_free_zero(vm_page_t m)
{

	m->flags |= PG_ZERO;
	vm_page_free_toq(m);
}

/*
 *	vm_page_sleep:
 *
 *	Sleep and release the page queues lock.
 *
 *	The object containing the given page must be locked.
 */
void
vm_page_sleep(vm_page_t m, const char *msg)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (!mtx_owned(&vm_page_queue_mtx))
		vm_page_lock_queues();
	vm_page_flag_set(m, PG_REFERENCED);
	vm_page_unlock_queues();

	/*
	 * It's possible that while we sleep, the page will get
	 * unbusied and freed.  If we are holding the object
	 * lock, we will assume we hold a reference to the object
	 * such that even if m->object changes, we can re-lock
	 * it.
	 */
	m->oflags |= VPO_WANTED;
	msleep(m, VM_OBJECT_MTX(m->object), PVM, msg, 0);
}

/*
 *	vm_page_dirty:
 *
 *	make page all dirty
 */
void
vm_page_dirty(vm_page_t m)
{
	KASSERT(VM_PAGE_GETKNOWNQUEUE1(m) != PQ_CACHE,
	    ("vm_page_dirty: page in cache!"));
	KASSERT(VM_PAGE_GETKNOWNQUEUE1(m) != PQ_FREE,
	    ("vm_page_dirty: page is free!"));
	m->dirty = VM_PAGE_BITS_ALL;
}

/*
 *	vm_page_splay:
 *
 *	Implements Sleator and Tarjan's top-down splay algorithm.  Returns
 *	the vm_page containing the given pindex.  If, however, that
 *	pindex is not found in the vm_object, returns a vm_page that is
 *	adjacent to the pindex, coming before or after it.
 */
vm_page_t
vm_page_splay(vm_pindex_t pindex, vm_page_t root)
{
	struct vm_page dummy;
	vm_page_t lefttreemax, righttreemin, y;

	if (root == NULL)
		return (root);
	lefttreemax = righttreemin = &dummy;
	for (;; root = y) {
		if (pindex < root->pindex) {
			if ((y = root->left) == NULL)
				break;
			if (pindex < y->pindex) {
				/* Rotate right. */
				root->left = y->right;
				y->right = root;
				root = y;
				if ((y = root->left) == NULL)
					break;
			}
			/* Link into the new root's right tree. */
			righttreemin->left = root;
			righttreemin = root;
		} else if (pindex > root->pindex) {
			if ((y = root->right) == NULL)
				break;
			if (pindex > y->pindex) {
				/* Rotate left. */
				root->right = y->left;
				y->left = root;
				root = y;
				if ((y = root->right) == NULL)
					break;
			}
			/* Link into the new root's left tree. */
			lefttreemax->right = root;
			lefttreemax = root;
		} else
			break;
	}
	/* Assemble the new root. */
	lefttreemax->right = root->left;
	righttreemin->left = root->right;
	root->left = dummy.right;
	root->right = dummy.left;
	return (root);
}

/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object and object list.
 *
 *	The pagetables are not updated but will presumably fault the page
 *	in if necessary, or if a kernel page the caller will at some point
 *	enter the page into the kernel's pmap.  We are not allowed to block
 *	here so we *can't* do this anyway.
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
void
vm_page_insert(vm_page_t m, vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t root;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (m->object != NULL)
		panic("vm_page_insert: page already inserted");

	/*
	 * Record the object/offset pair in this page
	 */
	m->object = object;
	m->pindex = pindex;

	/*
	 * Now link into the object's ordered list of backed pages.
	 */
	root = object->root;
	if (root == NULL) {
		m->left = NULL;
		m->right = NULL;
		TAILQ_INSERT_TAIL(&object->memq, m, listq);
	} else {
		root = vm_page_splay(pindex, root);
		if (pindex < root->pindex) {
			m->left = root->left;
			m->right = root;
			root->left = NULL;
			TAILQ_INSERT_BEFORE(root, m, listq);
		} else if (pindex == root->pindex)
			panic("vm_page_insert: offset already allocated");
		else {
			m->right = root->right;
			m->left = root;
			root->right = NULL;
			TAILQ_INSERT_AFTER(&object->memq, root, m, listq);
		}
	}
	object->root = m;
	object->generation++;

	/*
	 * show that the object has one more resident page.
	 */
	object->resident_page_count++;
	/*
	 * Hold the vnode until the last page is released.
	 */
	if (object->resident_page_count == 1 && object->type == OBJT_VNODE)
		vhold((struct vnode *)object->handle);

	/*
	 * Since we are inserting a new and possibly dirty page,
	 * update the object's OBJ_MIGHTBEDIRTY flag.
	 */
	if (m->flags & PG_WRITEABLE)
		vm_object_set_writeable_dirty(object);
}

/*
 *	vm_page_remove:
 *				NOTE: used by device pager as well -wfj
 *
 *	Removes the given mem entry from the object/offset-page
 *	table and the object page list, but do not invalidate/terminate
 *	the backing store.
 *
 *	The object and page must be locked.
 *	The underlying pmap entry (if any) is NOT removed here.
 *	This routine may not block.
 */
void
vm_page_remove(vm_page_t m)
{
	vm_object_t object;
	vm_page_t root;

	if ((object = m->object) == NULL)
		return;
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (m->oflags & VPO_BUSY) {
		m->oflags &= ~VPO_BUSY;
		vm_page_flash(m);
	}
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);

	/*
	 * Now remove from the object's list of backed pages.
	 */
	if (m != object->root)
		vm_page_splay(m->pindex, object->root);
	if (m->left == NULL)
		root = m->right;
	else {
		root = vm_page_splay(m->pindex, m->left);
		root->right = m->right;
	}
	object->root = root;
	TAILQ_REMOVE(&object->memq, m, listq);

	/*
	 * And show that the object has one fewer resident page.
	 */
	object->resident_page_count--;
	object->generation++;
	/*
	 * The vnode may now be recycled.
	 */
	if (object->resident_page_count == 0 && object->type == OBJT_VNODE)
		vdrop((struct vnode *)object->handle);

	m->object = NULL;
}

/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 *	This routine may not block.
 *	This is a critical path routine
 */
vm_page_t
vm_page_lookup(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if ((m = object->root) != NULL && m->pindex != pindex) {
		m = vm_page_splay(pindex, m);
		if ((object->root = m)->pindex != pindex)
			m = NULL;
	}
	return (m);
}

/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	The object must be locked.
 *	This routine may not block.
 *
 *	Note: swap associated with the page must be invalidated by the move.  We
 *	      have to do this for several reasons:  (1) we aren't freeing the
 *	      page, (2) we are dirtying the page, (3) the VM system is probably
 *	      moving the page from object A to B, and will then later move
 *	      the backing store from A to B and we can't have a conflict.
 *
 *	Note: we *always* dirty the page.  It is necessary both for the
 *	      fact that we moved it, and because we may be invalidating
 *	      swap.  If the page is on the cache, we have to deactivate it
 *	      or vm_page_dirty() will panic.  Dirty pages are not allowed
 *	      on the cache.
 */
void
vm_page_rename(vm_page_t m, vm_object_t new_object, vm_pindex_t new_pindex)
{

	vm_page_remove(m);
	vm_page_insert(m, new_object, new_pindex);
	if (VM_PAGE_INQUEUE1(m, PQ_CACHE))
		vm_page_deactivate(m);
	vm_page_dirty(m);
}

/*
 *	vm_page_select_cache:
 *
 *	Move a page of the given color from the cache queue to the free
 *	queue.  As pages might be found, but are not applicable, they are
 *	deactivated.
 *
 *	This routine may not block.
 */
vm_page_t
vm_page_select_cache(int color)
{
	vm_object_t object;
	vm_page_t m;
	boolean_t was_trylocked;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	while ((m = vm_pageq_find(PQ_CACHE, color, FALSE)) != NULL) {
		KASSERT(m->dirty == 0, ("Found dirty cache page %p", m));
		KASSERT(!pmap_page_is_mapped(m),
		    ("Found mapped cache page %p", m));
		KASSERT((m->flags & PG_UNMANAGED) == 0,
		    ("Found unmanaged cache page %p", m));
		KASSERT(m->wire_count == 0, ("Found wired cache page %p", m));
		if (m->hold_count == 0 && (object = m->object,
		    (was_trylocked = VM_OBJECT_TRYLOCK(object)) ||
		    VM_OBJECT_LOCKED(object))) {
			KASSERT((m->oflags & VPO_BUSY) == 0 && m->busy == 0,
			    ("Found busy cache page %p", m));
			vm_page_free(m);
			if (was_trylocked)
				VM_OBJECT_UNLOCK(object);
			break;
		}
		vm_page_deactivate(m);
	}
	return (m);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a memory cell associated
 *	with this VM object/offset pair.
 *
 *	page_req classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *	VM_ALLOC_ZERO		zero page
 *
 *	This routine may not block.
 *
 *	Additional special handling is required when called from an
 *	interrupt (VM_ALLOC_INTERRUPT).  We are not allowed to mess with
 *	the page cache in this case.
 */
vm_page_t
vm_page_alloc(vm_object_t object, vm_pindex_t pindex, int req)
{
	vm_page_t m = NULL;
	int color, flags, page_req;

	page_req = req & VM_ALLOC_CLASS_MASK;
	KASSERT(curthread->td_intr_nesting_level == 0 ||
	    page_req == VM_ALLOC_INTERRUPT,
	    ("vm_page_alloc(NORMAL|SYSTEM) in interrupt context"));

	if ((req & VM_ALLOC_NOOBJ) == 0) {
		KASSERT(object != NULL,
		    ("vm_page_alloc: NULL object."));
		VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
		color = (pindex + object->pg_color) & PQ_COLORMASK;
	} else
		color = pindex & PQ_COLORMASK;

	/*
	 * The pager is allowed to eat deeper into the free page list.
	 */
	if ((curproc == pageproc) && (page_req != VM_ALLOC_INTERRUPT)) {
		page_req = VM_ALLOC_SYSTEM;
	};

loop:
	mtx_lock(&vm_page_queue_free_mtx);
	if (cnt.v_free_count > cnt.v_free_reserved ||
	    (page_req == VM_ALLOC_SYSTEM && 
	     cnt.v_cache_count == 0 && 
	     cnt.v_free_count > cnt.v_interrupt_free_min) ||
	    (page_req == VM_ALLOC_INTERRUPT && cnt.v_free_count > 0)) {
		/*
		 * Allocate from the free queue if the number of free pages
		 * exceeds the minimum for the request class.
		 */
		m = vm_pageq_find(PQ_FREE, color, (req & VM_ALLOC_ZERO) != 0);
	} else if (page_req != VM_ALLOC_INTERRUPT) {
		mtx_unlock(&vm_page_queue_free_mtx);
		/*
		 * Allocatable from cache (non-interrupt only).  On success,
		 * we must free the page and try again, thus ensuring that
		 * cnt.v_*_free_min counters are replenished.
		 */
		vm_page_lock_queues();
		if ((m = vm_page_select_cache(color)) == NULL) {
			KASSERT(cnt.v_cache_count == 0,
			    ("vm_page_alloc: cache queue is missing %d pages",
			    cnt.v_cache_count));
			vm_page_unlock_queues();
			atomic_add_int(&vm_pageout_deficit, 1);
			pagedaemon_wakeup();

			if (page_req != VM_ALLOC_SYSTEM) 
				return (NULL);

			mtx_lock(&vm_page_queue_free_mtx);
			if (cnt.v_free_count <= cnt.v_interrupt_free_min) {
				mtx_unlock(&vm_page_queue_free_mtx);
				return (NULL);
			}
			m = vm_pageq_find(PQ_FREE, color, (req & VM_ALLOC_ZERO) != 0);
		} else {
			vm_page_unlock_queues();
			goto loop;
		}
	} else {
		/*
		 * Not allocatable from cache from interrupt, give up.
		 */
		mtx_unlock(&vm_page_queue_free_mtx);
		atomic_add_int(&vm_pageout_deficit, 1);
		pagedaemon_wakeup();
		return (NULL);
	}

	/*
	 *  At this point we had better have found a good page.
	 */

	KASSERT(
	    m != NULL,
	    ("vm_page_alloc(): missing page on free queue")
	);

	/*
	 * Remove from free queue
	 */
	vm_pageq_remove_nowakeup(m);

	/*
	 * Initialize structure.  Only the PG_ZERO flag is inherited.
	 */
	flags = 0;
	if (m->flags & PG_ZERO) {
		vm_page_zero_count--;
		if (req & VM_ALLOC_ZERO)
			flags = PG_ZERO;
	}
	if (object != NULL && object->type == OBJT_PHYS)
		flags |= PG_UNMANAGED;
	m->flags = flags;
	if (req & (VM_ALLOC_NOBUSY | VM_ALLOC_NOOBJ))
		m->oflags = 0;
	else
		m->oflags = VPO_BUSY;
	if (req & VM_ALLOC_WIRED) {
		atomic_add_int(&cnt.v_wire_count, 1);
		m->wire_count = 1;
	} else
		m->wire_count = 0;
	m->hold_count = 0;
	m->act_count = 0;
	m->busy = 0;
	m->valid = 0;
	KASSERT(m->dirty == 0, ("vm_page_alloc: free/cache page %p was dirty", m));
	mtx_unlock(&vm_page_queue_free_mtx);

	if ((req & VM_ALLOC_NOOBJ) == 0)
		vm_page_insert(m, object, pindex);
	else
		m->pindex = pindex;

	/*
	 * Don't wakeup too often - wakeup the pageout daemon when
	 * we would be nearly out of memory.
	 */
	if (vm_paging_needed())
		pagedaemon_wakeup();

	return (m);
}

/*
 *	vm_wait:	(also see VM_WAIT macro)
 *
 *	Block until free pages are available for allocation
 *	- Called in various places before memory allocations.
 */
void
vm_wait(void)
{

	mtx_lock(&vm_page_queue_free_mtx);
	if (curproc == pageproc) {
		vm_pageout_pages_needed = 1;
		msleep(&vm_pageout_pages_needed, &vm_page_queue_free_mtx,
		    PDROP | PSWP, "VMWait", 0);
	} else {
		if (!vm_pages_needed) {
			vm_pages_needed = 1;
			wakeup(&vm_pages_needed);
		}
		msleep(&cnt.v_free_count, &vm_page_queue_free_mtx, PDROP | PVM,
		    "vmwait", 0);
	}
}

/*
 *	vm_waitpfault:	(also see VM_WAITPFAULT macro)
 *
 *	Block until free pages are available for allocation
 *	- Called only in vm_fault so that processes page faulting
 *	  can be easily tracked.
 *	- Sleeps at a lower priority than vm_wait() so that vm_wait()ing
 *	  processes will be able to grab memory first.  Do not change
 *	  this balance without careful testing first.
 */
void
vm_waitpfault(void)
{

	mtx_lock(&vm_page_queue_free_mtx);
	if (!vm_pages_needed) {
		vm_pages_needed = 1;
		wakeup(&vm_pages_needed);
	}
	msleep(&cnt.v_free_count, &vm_page_queue_free_mtx, PDROP | PUSER,
	    "pfault", 0);
}

/*
 *	vm_page_activate:
 *
 *	Put the specified page on the active list (if appropriate).
 *	Ensure that act_count is at least ACT_INIT but do not otherwise
 *	mess with it.
 *
 *	The page queues must be locked.
 *	This routine may not block.
 */
void
vm_page_activate(vm_page_t m)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (VM_PAGE_GETKNOWNQUEUE2(m) != PQ_ACTIVE) {
		if (VM_PAGE_INQUEUE1(m, PQ_CACHE))
			cnt.v_reactivated++;
		vm_pageq_remove(m);
		if (m->wire_count == 0 && (m->flags & PG_UNMANAGED) == 0) {
			if (m->act_count < ACT_INIT)
				m->act_count = ACT_INIT;
			vm_pageq_enqueue(PQ_ACTIVE, m);
		}
	} else {
		if (m->act_count < ACT_INIT)
			m->act_count = ACT_INIT;
	}
}

/*
 *	vm_page_free_wakeup:
 *
 *	Helper routine for vm_page_free_toq() and vm_page_cache().  This
 *	routine is called when a page has been added to the cache or free
 *	queues.
 *
 *	The page queues must be locked.
 *	This routine may not block.
 */
static inline void
vm_page_free_wakeup(void)
{

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	/*
	 * if pageout daemon needs pages, then tell it that there are
	 * some free.
	 */
	if (vm_pageout_pages_needed &&
	    cnt.v_cache_count + cnt.v_free_count >= cnt.v_pageout_free_min) {
		wakeup(&vm_pageout_pages_needed);
		vm_pageout_pages_needed = 0;
	}
	/*
	 * wakeup processes that are waiting on memory if we hit a
	 * high water mark. And wakeup scheduler process if we have
	 * lots of memory. this process will swapin processes.
	 */
	if (vm_pages_needed && !vm_page_count_min()) {
		vm_pages_needed = 0;
		wakeup(&cnt.v_free_count);
	}
}

/*
 *	vm_page_free_toq:
 *
 *	Returns the given page to the PQ_FREE list,
 *	disassociating it with any VM object.
 *
 *	Object and page must be locked prior to entry.
 *	This routine may not block.
 */

void
vm_page_free_toq(vm_page_t m)
{
	struct vpgqueues *pq;

	if (VM_PAGE_GETQUEUE(m) != PQ_NONE)
		mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	KASSERT(!pmap_page_is_mapped(m),
	    ("vm_page_free_toq: freeing mapped page %p", m));
	PCPU_INC(cnt.v_tfree);

	if (m->busy || VM_PAGE_INQUEUE1(m, PQ_FREE)) {
		printf(
		"vm_page_free: pindex(%lu), busy(%d), VPO_BUSY(%d), hold(%d)\n",
		    (u_long)m->pindex, m->busy, (m->oflags & VPO_BUSY) ? 1 : 0,
		    m->hold_count);
		if (VM_PAGE_INQUEUE1(m, PQ_FREE))
			panic("vm_page_free: freeing free page");
		else
			panic("vm_page_free: freeing busy page");
	}

	/*
	 * unqueue, then remove page.  Note that we cannot destroy
	 * the page here because we do not want to call the pager's
	 * callback routine until after we've put the page on the
	 * appropriate free queue.
	 */
	vm_pageq_remove_nowakeup(m);
	vm_page_remove(m);

	/*
	 * If fictitious remove object association and
	 * return, otherwise delay object association removal.
	 */
	if ((m->flags & PG_FICTITIOUS) != 0) {
		return;
	}

	m->valid = 0;
	vm_page_undirty(m);

	if (m->wire_count != 0) {
		if (m->wire_count > 1) {
			panic("vm_page_free: invalid wire count (%d), pindex: 0x%lx",
				m->wire_count, (long)m->pindex);
		}
		panic("vm_page_free: freeing wired page");
	}
	if (m->hold_count != 0) {
		m->flags &= ~PG_ZERO;
		vm_pageq_enqueue(PQ_HOLD, m);
		return;
	}
	VM_PAGE_SETQUEUE1(m, PQ_FREE);
	mtx_lock(&vm_page_queue_free_mtx);
	pq = &vm_page_queues[VM_PAGE_GETQUEUE(m)];
	pq->lcnt++;
	++(*pq->cnt);

	/*
	 * Put zero'd pages on the end ( where we look for zero'd pages
	 * first ) and non-zerod pages at the head.
	 */
	if (m->flags & PG_ZERO) {
		TAILQ_INSERT_TAIL(&pq->pl, m, pageq);
		++vm_page_zero_count;
	} else {
		TAILQ_INSERT_HEAD(&pq->pl, m, pageq);
		vm_page_zero_idle_wakeup();
	}
	vm_page_free_wakeup();
	mtx_unlock(&vm_page_queue_free_mtx);
}

/*
 *	vm_page_wire:
 *
 *	Mark this page as wired down by yet
 *	another map, removing it from paging queues
 *	as necessary.
 *
 *	The page queues must be locked.
 *	This routine may not block.
 */
void
vm_page_wire(vm_page_t m)
{

	/*
	 * Only bump the wire statistics if the page is not already wired,
	 * and only unqueue the page if it is on some queue (if it is unmanaged
	 * it is already off the queues).
	 */
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (m->flags & PG_FICTITIOUS)
		return;
	if (m->wire_count == 0) {
		if ((m->flags & PG_UNMANAGED) == 0)
			vm_pageq_remove(m);
		atomic_add_int(&cnt.v_wire_count, 1);
	}
	m->wire_count++;
	KASSERT(m->wire_count != 0, ("vm_page_wire: wire_count overflow m=%p", m));
}

/*
 *	vm_page_unwire:
 *
 *	Release one wiring of this page, potentially
 *	enabling it to be paged again.
 *
 *	Many pages placed on the inactive queue should actually go
 *	into the cache, but it is difficult to figure out which.  What
 *	we do instead, if the inactive target is well met, is to put
 *	clean pages at the head of the inactive queue instead of the tail.
 *	This will cause them to be moved to the cache more quickly and
 *	if not actively re-referenced, freed more quickly.  If we just
 *	stick these pages at the end of the inactive queue, heavy filesystem
 *	meta-data accesses can cause an unnecessary paging load on memory bound 
 *	processes.  This optimization causes one-time-use metadata to be
 *	reused more quickly.
 *
 *	BUT, if we are in a low-memory situation we have no choice but to
 *	put clean pages on the cache queue.
 *
 *	A number of routines use vm_page_unwire() to guarantee that the page
 *	will go into either the inactive or active queues, and will NEVER
 *	be placed in the cache - for example, just after dirtying a page.
 *	dirty pages in the cache are not allowed.
 *
 *	The page queues must be locked.
 *	This routine may not block.
 */
void
vm_page_unwire(vm_page_t m, int activate)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (m->flags & PG_FICTITIOUS)
		return;
	if (m->wire_count > 0) {
		m->wire_count--;
		if (m->wire_count == 0) {
			atomic_subtract_int(&cnt.v_wire_count, 1);
			if (m->flags & PG_UNMANAGED) {
				;
			} else if (activate)
				vm_pageq_enqueue(PQ_ACTIVE, m);
			else {
				vm_page_flag_clear(m, PG_WINATCFLS);
				vm_pageq_enqueue(PQ_INACTIVE, m);
			}
		}
	} else {
		panic("vm_page_unwire: invalid wire count: %d", m->wire_count);
	}
}


/*
 * Move the specified page to the inactive queue.  If the page has
 * any associated swap, the swap is deallocated.
 *
 * Normally athead is 0 resulting in LRU operation.  athead is set
 * to 1 if we want this page to be 'as if it were placed in the cache',
 * except without unmapping it from the process address space.
 *
 * This routine may not block.
 */
static inline void
_vm_page_deactivate(vm_page_t m, int athead)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);

	/*
	 * Ignore if already inactive.
	 */
	if (VM_PAGE_INQUEUE2(m, PQ_INACTIVE))
		return;
	if (m->wire_count == 0 && (m->flags & PG_UNMANAGED) == 0) {
		if (VM_PAGE_INQUEUE1(m, PQ_CACHE))
			cnt.v_reactivated++;
		vm_page_flag_clear(m, PG_WINATCFLS);
		vm_pageq_remove(m);
		if (athead)
			TAILQ_INSERT_HEAD(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
		else
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
		VM_PAGE_SETQUEUE2(m, PQ_INACTIVE);
		vm_page_queues[PQ_INACTIVE].lcnt++;
		cnt.v_inactive_count++;
	}
}

void
vm_page_deactivate(vm_page_t m)
{
    _vm_page_deactivate(m, 0);
}

/*
 * vm_page_try_to_cache:
 *
 * Returns 0 on failure, 1 on success
 */
int
vm_page_try_to_cache(vm_page_t m)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (m->dirty || m->hold_count || m->busy || m->wire_count ||
	    (m->oflags & VPO_BUSY) || (m->flags & PG_UNMANAGED)) {
		return (0);
	}
	pmap_remove_all(m);
	if (m->dirty)
		return (0);
	vm_page_cache(m);
	return (1);
}

/*
 * vm_page_try_to_free()
 *
 *	Attempt to free the page.  If we cannot free it, we do nothing.
 *	1 is returned on success, 0 on failure.
 */
int
vm_page_try_to_free(vm_page_t m)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (m->object != NULL)
		VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (m->dirty || m->hold_count || m->busy || m->wire_count ||
	    (m->oflags & VPO_BUSY) || (m->flags & PG_UNMANAGED)) {
		return (0);
	}
	pmap_remove_all(m);
	if (m->dirty)
		return (0);
	vm_page_free(m);
	return (1);
}

/*
 * vm_page_cache
 *
 * Put the specified page onto the page cache queue (if appropriate).
 *
 * This routine may not block.
 */
void
vm_page_cache(vm_page_t m)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((m->flags & PG_UNMANAGED) || (m->oflags & VPO_BUSY) || m->busy ||
	    m->hold_count || m->wire_count) {
		printf("vm_page_cache: attempting to cache busy page\n");
		return;
	}
	if (VM_PAGE_INQUEUE1(m, PQ_CACHE))
		return;

	/*
	 * Remove all pmaps and indicate that the page is not
	 * writeable or mapped.
	 */
	pmap_remove_all(m);
	if (m->dirty != 0) {
		panic("vm_page_cache: caching a dirty page, pindex: %ld",
			(long)m->pindex);
	}
	vm_pageq_remove_nowakeup(m);
	vm_pageq_enqueue(PQ_CACHE + m->pc, m);
	mtx_lock(&vm_page_queue_free_mtx);
	vm_page_free_wakeup();
	mtx_unlock(&vm_page_queue_free_mtx);
}

/*
 * vm_page_dontneed
 *
 *	Cache, deactivate, or do nothing as appropriate.  This routine
 *	is typically used by madvise() MADV_DONTNEED.
 *
 *	Generally speaking we want to move the page into the cache so
 *	it gets reused quickly.  However, this can result in a silly syndrome
 *	due to the page recycling too quickly.  Small objects will not be
 *	fully cached.  On the otherhand, if we move the page to the inactive
 *	queue we wind up with a problem whereby very large objects 
 *	unnecessarily blow away our inactive and cache queues.
 *
 *	The solution is to move the pages based on a fixed weighting.  We
 *	either leave them alone, deactivate them, or move them to the cache,
 *	where moving them to the cache has the highest weighting.
 *	By forcing some pages into other queues we eventually force the
 *	system to balance the queues, potentially recovering other unrelated
 *	space from active.  The idea is to not force this to happen too
 *	often.
 */
void
vm_page_dontneed(vm_page_t m)
{
	static int dnweight;
	int dnw;
	int head;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	dnw = ++dnweight;

	/*
	 * occassionally leave the page alone
	 */
	if ((dnw & 0x01F0) == 0 ||
	    VM_PAGE_INQUEUE2(m, PQ_INACTIVE) || 
	    VM_PAGE_INQUEUE1(m, PQ_CACHE)
	) {
		if (m->act_count >= ACT_INIT)
			--m->act_count;
		return;
	}

	if (m->dirty == 0 && pmap_is_modified(m))
		vm_page_dirty(m);

	if (m->dirty || (dnw & 0x0070) == 0) {
		/*
		 * Deactivate the page 3 times out of 32.
		 */
		head = 0;
	} else {
		/*
		 * Cache the page 28 times out of every 32.  Note that
		 * the page is deactivated instead of cached, but placed
		 * at the head of the queue instead of the tail.
		 */
		head = 1;
	}
	_vm_page_deactivate(m, head);
}

/*
 * Grab a page, waiting until we are waken up due to the page
 * changing state.  We keep on waiting, if the page continues
 * to be in the object.  If the page doesn't exist, first allocate it
 * and then conditionally zero it.
 *
 * This routine may block.
 */
vm_page_t
vm_page_grab(vm_object_t object, vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
retrylookup:
	if ((m = vm_page_lookup(object, pindex)) != NULL) {
		if (vm_page_sleep_if_busy(m, TRUE, "pgrbwt")) {
			if ((allocflags & VM_ALLOC_RETRY) == 0)
				return (NULL);
			goto retrylookup;
		} else {
			if ((allocflags & VM_ALLOC_WIRED) != 0) {
				vm_page_lock_queues();
				vm_page_wire(m);
				vm_page_unlock_queues();
			}
			if ((allocflags & VM_ALLOC_NOBUSY) == 0)
				vm_page_busy(m);
			return (m);
		}
	}
	m = vm_page_alloc(object, pindex, allocflags & ~VM_ALLOC_RETRY);
	if (m == NULL) {
		VM_OBJECT_UNLOCK(object);
		VM_WAIT;
		VM_OBJECT_LOCK(object);
		if ((allocflags & VM_ALLOC_RETRY) == 0)
			return (NULL);
		goto retrylookup;
	}
	if (allocflags & VM_ALLOC_ZERO && (m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);
	return (m);
}

/*
 * Mapping function for valid bits or for dirty bits in
 * a page.  May not block.
 *
 * Inputs are required to range within a page.
 */
inline int
vm_page_bits(int base, int size)
{
	int first_bit;
	int last_bit;

	KASSERT(
	    base + size <= PAGE_SIZE,
	    ("vm_page_bits: illegal base/size %d/%d", base, size)
	);

	if (size == 0)		/* handle degenerate case */
		return (0);

	first_bit = base >> DEV_BSHIFT;
	last_bit = (base + size - 1) >> DEV_BSHIFT;

	return ((2 << last_bit) - (1 << first_bit));
}

/*
 *	vm_page_set_validclean:
 *
 *	Sets portions of a page valid and clean.  The arguments are expected
 *	to be DEV_BSIZE aligned but if they aren't the bitmap is inclusive
 *	of any partial chunks touched by the range.  The invalid portion of
 *	such chunks will be zero'd.
 *
 *	This routine may not block.
 *
 *	(base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_validclean(vm_page_t m, int base, int size)
{
	int pagebits;
	int frag;
	int endoff;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (size == 0)	/* handle degenerate case */
		return;

	/*
	 * If the base is not DEV_BSIZE aligned and the valid
	 * bit is clear, we have to zero out a portion of the
	 * first block.
	 */
	if ((frag = base & ~(DEV_BSIZE - 1)) != base &&
	    (m->valid & (1 << (base >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, frag, base - frag);

	/*
	 * If the ending offset is not DEV_BSIZE aligned and the 
	 * valid bit is clear, we have to zero out a portion of
	 * the last block.
	 */
	endoff = base + size;
	if ((frag = endoff & ~(DEV_BSIZE - 1)) != endoff &&
	    (m->valid & (1 << (endoff >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, endoff,
		    DEV_BSIZE - (endoff & (DEV_BSIZE - 1)));

	/*
	 * Set valid, clear dirty bits.  If validating the entire
	 * page we can safely clear the pmap modify bit.  We also
	 * use this opportunity to clear the VPO_NOSYNC flag.  If a process
	 * takes a write fault on a MAP_NOSYNC memory area the flag will
	 * be set again.
	 *
	 * We set valid bits inclusive of any overlap, but we can only
	 * clear dirty bits for DEV_BSIZE chunks that are fully within
	 * the range.
	 */
	pagebits = vm_page_bits(base, size);
	m->valid |= pagebits;
#if 0	/* NOT YET */
	if ((frag = base & (DEV_BSIZE - 1)) != 0) {
		frag = DEV_BSIZE - frag;
		base += frag;
		size -= frag;
		if (size < 0)
			size = 0;
	}
	pagebits = vm_page_bits(base, size & (DEV_BSIZE - 1));
#endif
	m->dirty &= ~pagebits;
	if (base == 0 && size == PAGE_SIZE) {
		pmap_clear_modify(m);
		m->oflags &= ~VPO_NOSYNC;
	}
}

void
vm_page_clear_dirty(vm_page_t m, int base, int size)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	m->dirty &= ~vm_page_bits(base, size);
}

/*
 *	vm_page_set_invalid:
 *
 *	Invalidates DEV_BSIZE'd chunks within a page.  Both the
 *	valid and dirty bits for the effected areas are cleared.
 *
 *	May not block.
 */
void
vm_page_set_invalid(vm_page_t m, int base, int size)
{
	int bits;

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	bits = vm_page_bits(base, size);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (m->valid == VM_PAGE_BITS_ALL && bits != 0)
		pmap_remove_all(m);
	m->valid &= ~bits;
	m->dirty &= ~bits;
	m->object->generation++;
}

/*
 * vm_page_zero_invalid()
 *
 *	The kernel assumes that the invalid portions of a page contain 
 *	garbage, but such pages can be mapped into memory by user code.
 *	When this occurs, we must zero out the non-valid portions of the
 *	page so user code sees what it expects.
 *
 *	Pages are most often semi-valid when the end of a file is mapped 
 *	into memory and the file's size is not page aligned.
 */
void
vm_page_zero_invalid(vm_page_t m, boolean_t setvalid)
{
	int b;
	int i;

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	/*
	 * Scan the valid bits looking for invalid sections that
	 * must be zerod.  Invalid sub-DEV_BSIZE'd areas ( where the
	 * valid bit may be set ) have already been zerod by
	 * vm_page_set_validclean().
	 */
	for (b = i = 0; i <= PAGE_SIZE / DEV_BSIZE; ++i) {
		if (i == (PAGE_SIZE / DEV_BSIZE) || 
		    (m->valid & (1 << i))
		) {
			if (i > b) {
				pmap_zero_page_area(m, 
				    b << DEV_BSHIFT, (i - b) << DEV_BSHIFT);
			}
			b = i + 1;
		}
	}

	/*
	 * setvalid is TRUE when we can safely set the zero'd areas
	 * as being valid.  We can do this if there are no cache consistancy
	 * issues.  e.g. it is ok to do with UFS, but not ok to do with NFS.
	 */
	if (setvalid)
		m->valid = VM_PAGE_BITS_ALL;
}

/*
 *	vm_page_is_valid:
 *
 *	Is (partial) page valid?  Note that the case where size == 0
 *	will return FALSE in the degenerate case where the page is
 *	entirely invalid, and TRUE otherwise.
 *
 *	May not block.
 */
int
vm_page_is_valid(vm_page_t m, int base, int size)
{
	int bits = vm_page_bits(base, size);

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if (m->valid && ((m->valid & bits) == bits))
		return 1;
	else
		return 0;
}

/*
 * update dirty bits from pmap/mmu.  May not block.
 */
void
vm_page_test_dirty(vm_page_t m)
{
	if ((m->dirty != VM_PAGE_BITS_ALL) && pmap_is_modified(m)) {
		vm_page_dirty(m);
	}
}

int so_zerocp_fullpage = 0;

void
vm_page_cowfault(vm_page_t m)
{
	vm_page_t mnew;
	vm_object_t object;
	vm_pindex_t pindex;

	object = m->object;
	pindex = m->pindex;

 retry_alloc:
	pmap_remove_all(m);
	vm_page_remove(m);
	mnew = vm_page_alloc(object, pindex, VM_ALLOC_NORMAL | VM_ALLOC_NOBUSY);
	if (mnew == NULL) {
		vm_page_insert(m, object, pindex);
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(object);
		VM_WAIT;
		VM_OBJECT_LOCK(object);
		vm_page_lock_queues();
		goto retry_alloc;
	}

	if (m->cow == 0) {
		/* 
		 * check to see if we raced with an xmit complete when 
		 * waiting to allocate a page.  If so, put things back 
		 * the way they were 
		 */
		vm_page_free(mnew);
		vm_page_insert(m, object, pindex);
	} else { /* clear COW & copy page */
		if (!so_zerocp_fullpage)
			pmap_copy_page(m, mnew);
		mnew->valid = VM_PAGE_BITS_ALL;
		vm_page_dirty(mnew);
		mnew->wire_count = m->wire_count - m->cow;
		m->wire_count = m->cow;
	}
}

void 
vm_page_cowclear(vm_page_t m)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (m->cow) {
		m->cow--;
		/* 
		 * let vm_fault add back write permission  lazily
		 */
	} 
	/*
	 *  sf_buf_free() will free the page, so we needn't do it here
	 */ 
}

void
vm_page_cowsetup(vm_page_t m)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	m->cow++;
	pmap_remove_write(m);
}

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

DB_SHOW_COMMAND(page, vm_page_print_page_info)
{
	db_printf("cnt.v_free_count: %d\n", cnt.v_free_count);
	db_printf("cnt.v_cache_count: %d\n", cnt.v_cache_count);
	db_printf("cnt.v_inactive_count: %d\n", cnt.v_inactive_count);
	db_printf("cnt.v_active_count: %d\n", cnt.v_active_count);
	db_printf("cnt.v_wire_count: %d\n", cnt.v_wire_count);
	db_printf("cnt.v_free_reserved: %d\n", cnt.v_free_reserved);
	db_printf("cnt.v_free_min: %d\n", cnt.v_free_min);
	db_printf("cnt.v_free_target: %d\n", cnt.v_free_target);
	db_printf("cnt.v_cache_min: %d\n", cnt.v_cache_min);
	db_printf("cnt.v_inactive_target: %d\n", cnt.v_inactive_target);
}

DB_SHOW_COMMAND(pageq, vm_page_print_pageq_info)
{
	int i;
	db_printf("PQ_FREE:");
	for (i = 0; i < PQ_NUMCOLORS; i++) {
		db_printf(" %d", vm_page_queues[PQ_FREE + i].lcnt);
	}
	db_printf("\n");
		
	db_printf("PQ_CACHE:");
	for (i = 0; i < PQ_NUMCOLORS; i++) {
		db_printf(" %d", vm_page_queues[PQ_CACHE + i].lcnt);
	}
	db_printf("\n");

	db_printf("PQ_ACTIVE: %d, PQ_INACTIVE: %d\n",
		vm_page_queues[PQ_ACTIVE].lcnt,
		vm_page_queues[PQ_INACTIVE].lcnt);
}
#endif /* DDB */
