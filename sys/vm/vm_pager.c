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
 *	from: @(#)vm_pager.c	7.4 (Berkeley) 5/7/91
 *	$Id: vm_pager.c,v 1.3 1993/10/16 16:20:50 rgrimes Exp $
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
 *	Paging space routine stubs.  Emulates a matchmaker-like interface
 *	for builtin pagers.
 */

#include "param.h"
#include "malloc.h"

#include "vm.h"
#include "vm_page.h"
#include "vm_kern.h"

#include "swappager.h"

#if NSWAPPAGER > 0
extern struct pagerops swappagerops;
#else
#define	swappagerops	NULL
#endif
#include "vnodepager.h"
#if NVNODEPAGER > 0
extern struct pagerops vnodepagerops;
#else
#define	vnodepagerops	NULL
#endif
#include "devpager.h"
#if NDEVPAGER > 0
extern struct pagerops devicepagerops;
#else
#define	devicepagerops	NULL
#endif

struct pagerops *pagertab[] = {
	&swappagerops,		/* PG_SWAP */
	&vnodepagerops,		/* PG_VNODE */
	&devicepagerops,	/* PG_DEV */
};
int npagers = sizeof (pagertab) / sizeof (pagertab[0]);

struct pagerops *dfltpagerops = NULL;	/* default pager */

/*
 * Kernel address space for mapping pages.
 * Used by pagers where KVAs are needed for IO.
 */
#define PAGER_MAP_SIZE	(256 * PAGE_SIZE)
vm_map_t pager_map;
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
	 * If pgops is a null pointer skip over it.
	 */
	for (pgops = pagertab; pgops < &pagertab[npagers]; pgops++)
		if (*pgops) (*(*pgops)->pgo_init)();
	if (dfltpagerops == NULL)
		panic("no default pager");
}

/*
 * Allocate an instance of a pager of the given type.
 */
vm_pager_t
vm_pager_allocate(type, handle, size, prot)
	int type;
	caddr_t handle;
	vm_size_t size;
	vm_prot_t prot;
{
	vm_pager_t pager;
	struct pagerops *ops;

	ops = (type == PG_DFLT) ? dfltpagerops : pagertab[type];
	return((*ops->pgo_alloc)(handle, size, prot));
}

void
vm_pager_deallocate(pager)
	vm_pager_t	pager;
{
	if (pager == NULL)
		panic("vm_pager_deallocate: null pager");

	VM_PAGER_DEALLOC(pager);
}

vm_pager_get(pager, m, sync)
	vm_pager_t	pager;
	vm_page_t	m;
	boolean_t	sync;
{
	extern boolean_t vm_page_zero_fill();

	if (pager == NULL)
		return(vm_page_zero_fill(m) ? VM_PAGER_OK : VM_PAGER_FAIL);
	return(VM_PAGER_GET(pager, m, sync));
}

vm_pager_put(pager, m, sync)
	vm_pager_t	pager;
	vm_page_t	m;
	boolean_t	sync;
{
	if (pager == NULL)
		panic("vm_pager_put: null pager");
	return(VM_PAGER_PUT(pager, m, sync));
}

boolean_t
vm_pager_has_page(pager, offset)
	vm_pager_t	pager;
	vm_offset_t	offset;
{
	if (pager == NULL)
		panic("vm_pager_has_page");
	return(VM_PAGER_HASPAGE(pager, offset));
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
		(*(*pgops)->pgo_putpage)(NULL, NULL, FALSE);
}

vm_offset_t
vm_pager_map_page(m)
	vm_page_t	m;
{
	vm_offset_t kva;

#ifdef DEBUG
	if (!m->busy || m->active)
		panic("vm_pager_map_page: page active or not busy");
	if (m->pagerowned)
		printf("vm_pager_map_page: page %x already in pager\n", m);
#endif
	kva = kmem_alloc_wait(pager_map, PAGE_SIZE);
#ifdef DEBUG
	m->pagerowned = 1;
#endif
	pmap_enter(vm_map_pmap(pager_map), kva, VM_PAGE_TO_PHYS(m),
		   VM_PROT_DEFAULT, TRUE);
	return(kva);
}

void
vm_pager_unmap_page(kva)
	vm_offset_t	kva;
{
#ifdef DEBUG
	vm_page_t m;

	m = PHYS_TO_VM_PAGE(pmap_extract(vm_map_pmap(pager_map), kva));
#endif
	kmem_free_wakeup(pager_map, kva, PAGE_SIZE);
#ifdef DEBUG
	if (m->pagerowned)
		m->pagerowned = 0;
	else
		printf("vm_pager_unmap_page: page %x(%x/%x) not owned\n",
		       m, kva, VM_PAGE_TO_PHYS(m));
#endif
}

vm_pager_t
vm_pager_lookup(list, handle)
	register queue_head_t *list;
	caddr_t handle;
{
	register vm_pager_t pager;

	pager = (vm_pager_t) queue_first(list);
	while (!queue_end(list, (queue_entry_t)pager)) {
		if (pager->pg_handle == handle)
			return(pager);
		pager = (vm_pager_t) queue_next(&pager->pg_list);
	}
	return(NULL);
}

/*
 * This routine gains a reference to the object.
 * Explicit deallocation is necessary.
 */
pager_cache(object, should_cache)
	vm_object_t	object;
	boolean_t	should_cache;
{
	if (object == NULL)
		return(KERN_INVALID_ARGUMENT);

	vm_object_cache_lock();
	vm_object_lock(object);
	object->can_persist = should_cache;
	vm_object_unlock(object);
	vm_object_cache_unlock();

	vm_object_deallocate(object);

	return(KERN_SUCCESS);
}
