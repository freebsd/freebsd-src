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
 *	@(#)vm_pager.c	8.7 (Berkeley) 7/7/94
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
 *	Paging space routine stubs.  Emulates a matchmaker-like interface
 *	for builtin pagers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#ifdef SWAPPAGER
extern struct pagerops swappagerops;
#endif

#ifdef VNODEPAGER
extern struct pagerops vnodepagerops;
#endif

#ifdef DEVPAGER
extern struct pagerops devicepagerops;
#endif

struct pagerops *pagertab[] = {
#ifdef SWAPPAGER
	&swappagerops,		/* PG_SWAP */
#else
	NULL,
#endif
#ifdef VNODEPAGER
	&vnodepagerops,		/* PG_VNODE */
#else
	NULL,
#endif
#ifdef DEVPAGER
	&devicepagerops,	/* PG_DEV */
#else
	NULL,
#endif
};
int npagers = sizeof (pagertab) / sizeof (pagertab[0]);

struct pagerops *dfltpagerops = NULL;	/* default pager */

/*
 * Kernel address space for mapping pages.
 * Used by pagers where KVAs are needed for IO.
 *
 * XXX needs to be large enough to support the number of pending async
 * cleaning requests (NPENDINGIO == 64) * the maximum swap cluster size
 * (MAXPHYS == 64k) if you want to get the most efficiency.
 */
#define PAGER_MAP_SIZE	(4 * 1024 * 1024)

vm_map_t pager_map;
boolean_t pager_map_wanted;
vm_offset_t pager_sva, pager_eva;

void
vm_pager_init()
{
	struct pagerops **pgops;

	/*
	 * Allocate a kernel submap for tracking get/put page mappings
	 */
	pager_map = kmem_suballoc(kernel_map, &pager_sva, &pager_eva,
				  PAGER_MAP_SIZE, FALSE);
	/*
	 * Initialize known pagers
	 */
	for (pgops = pagertab; pgops < &pagertab[npagers]; pgops++)
		if (pgops)
			(*(*pgops)->pgo_init)();
	if (dfltpagerops == NULL)
		panic("no default pager");
}

/*
 * Allocate an instance of a pager of the given type.
 * Size, protection and offset parameters are passed in for pagers that
 * need to perform page-level validation (e.g. the device pager).
 */
vm_pager_t
vm_pager_allocate(type, handle, size, prot, off)
	int type;
	caddr_t handle;
	vm_size_t size;
	vm_prot_t prot;
	vm_offset_t off;
{
	struct pagerops *ops;

	ops = (type == PG_DFLT) ? dfltpagerops : pagertab[type];
	if (ops)
		return ((*ops->pgo_alloc)(handle, size, prot, off));
	return (NULL);
}

void
vm_pager_deallocate(pager)
	vm_pager_t	pager;
{
	if (pager == NULL)
		panic("vm_pager_deallocate: null pager");

	(*pager->pg_ops->pgo_dealloc)(pager);
}

int
vm_pager_get_pages(pager, mlist, npages, sync)
	vm_pager_t	pager;
	vm_page_t	*mlist;
	int		npages;
	boolean_t	sync;
{
	int rv;

	if (pager == NULL) {
		rv = VM_PAGER_OK;
		while (npages--)
			if (!vm_page_zero_fill(*mlist)) {
				rv = VM_PAGER_FAIL;
				break;
			} else
				mlist++;
		return (rv);
	}
	return ((*pager->pg_ops->pgo_getpages)(pager, mlist, npages, sync));
}

int
vm_pager_put_pages(pager, mlist, npages, sync)
	vm_pager_t	pager;
	vm_page_t	*mlist;
	int		npages;
	boolean_t	sync;
{
	if (pager == NULL)
		panic("vm_pager_put_pages: null pager");
	return ((*pager->pg_ops->pgo_putpages)(pager, mlist, npages, sync));
}

/* XXX compatibility*/
int
vm_pager_get(pager, m, sync)
	vm_pager_t	pager;
	vm_page_t	m;
	boolean_t	sync;
{
	return vm_pager_get_pages(pager, &m, 1, sync);
}

/* XXX compatibility*/
int
vm_pager_put(pager, m, sync)
	vm_pager_t	pager;
	vm_page_t	m;
	boolean_t	sync;
{
	return vm_pager_put_pages(pager, &m, 1, sync);
}

boolean_t
vm_pager_has_page(pager, offset)
	vm_pager_t	pager;
	vm_offset_t	offset;
{
	if (pager == NULL)
		panic("vm_pager_has_page: null pager");
	return ((*pager->pg_ops->pgo_haspage)(pager, offset));
}

/*
 * Called by pageout daemon before going back to sleep.
 * Gives pagers a chance to clean up any completed async pageing operations.
 */
void
vm_pager_sync()
{
	struct pagerops **pgops;

	for (pgops = pagertab; pgops < &pagertab[npagers]; pgops++)
		if (pgops)
			(*(*pgops)->pgo_putpages)(NULL, NULL, 0, FALSE);
}

void
vm_pager_cluster(pager, offset, loff, hoff)
	vm_pager_t	pager;
	vm_offset_t	offset;
	vm_offset_t	*loff;
	vm_offset_t	*hoff;
{
	if (pager == NULL)
		panic("vm_pager_cluster: null pager");
	((*pager->pg_ops->pgo_cluster)(pager, offset, loff, hoff));
}

void
vm_pager_clusternull(pager, offset, loff, hoff)
	vm_pager_t	pager;
	vm_offset_t	offset;
	vm_offset_t	*loff;
	vm_offset_t	*hoff;
{
	panic("vm_pager_nullcluster called");
}

vm_offset_t
vm_pager_map_pages(mlist, npages, canwait)
	vm_page_t	*mlist;
	int		npages;
	boolean_t	canwait;
{
	vm_offset_t kva, va;
	vm_size_t size;
	vm_page_t m;

	/*
	 * Allocate space in the pager map, if none available return 0.
	 * This is basically an expansion of kmem_alloc_wait with optional
	 * blocking on no space.
	 */
	size = npages * PAGE_SIZE;
	vm_map_lock(pager_map);
	while (vm_map_findspace(pager_map, 0, size, &kva)) {
		if (!canwait) {
			vm_map_unlock(pager_map);
			return (0);
		}
		pager_map_wanted = TRUE;
		vm_map_unlock(pager_map);
		(void) tsleep(pager_map, PVM, "pager_map", 0);
		vm_map_lock(pager_map);
	}
	vm_map_insert(pager_map, NULL, 0, kva, kva + size);
	vm_map_unlock(pager_map);

	for (va = kva; npages--; va += PAGE_SIZE) {
		m = *mlist++;
#ifdef DEBUG
		if ((m->flags & PG_BUSY) == 0)
			panic("vm_pager_map_pages: page not busy");
		if (m->flags & PG_PAGEROWNED)
			panic("vm_pager_map_pages: page already in pager");
#endif
#ifdef DEBUG
		m->flags |= PG_PAGEROWNED;
#endif
		pmap_enter(vm_map_pmap(pager_map), va, VM_PAGE_TO_PHYS(m),
			   VM_PROT_DEFAULT, TRUE);
	}
	return (kva);
}

void
vm_pager_unmap_pages(kva, npages)
	vm_offset_t	kva;
	int		npages;
{
	vm_size_t size = npages * PAGE_SIZE;

#ifdef DEBUG
	vm_offset_t va;
	vm_page_t m;
	int np = npages;

	for (va = kva; np--; va += PAGE_SIZE) {
		m = vm_pager_atop(va);
		if (m->flags & PG_PAGEROWNED)
			m->flags &= ~PG_PAGEROWNED;
		else
			printf("vm_pager_unmap_pages: %x(%x/%x) not owned\n",
			       m, va, VM_PAGE_TO_PHYS(m));
	}
#endif
	pmap_remove(vm_map_pmap(pager_map), kva, kva + size);
	vm_map_lock(pager_map);
	(void) vm_map_delete(pager_map, kva, kva + size);
	if (pager_map_wanted)
		wakeup(pager_map);
	vm_map_unlock(pager_map);
}

vm_page_t
vm_pager_atop(kva)
	vm_offset_t	kva;
{
	vm_offset_t pa;

	pa = pmap_extract(vm_map_pmap(pager_map), kva);
	if (pa == 0)
		panic("vm_pager_atop");
	return (PHYS_TO_VM_PAGE(pa));
}

vm_pager_t
vm_pager_lookup(pglist, handle)
	register struct pagerlst *pglist;
	caddr_t handle;
{
	register vm_pager_t pager;

	for (pager = pglist->tqh_first; pager; pager = pager->pg_list.tqe_next)
		if (pager->pg_handle == handle)
			return (pager);
	return (NULL);
}

/*
 * This routine gains a reference to the object.
 * Explicit deallocation is necessary.
 */
int
pager_cache(object, should_cache)
	vm_object_t	object;
	boolean_t	should_cache;
{
	if (object == NULL)
		return (KERN_INVALID_ARGUMENT);

	vm_object_cache_lock();
	vm_object_lock(object);
	if (should_cache)
		object->flags |= OBJ_CANPERSIST;
	else
		object->flags &= ~OBJ_CANPERSIST;
	vm_object_unlock(object);
	vm_object_cache_unlock();

	vm_object_deallocate(object);

	return (KERN_SUCCESS);
}
