/*-
 * Copyright (c) 1997, Duke University
 * All rights reserved.
 *
 * Author:
 *         Andrew Gallatin <gallatin@cs.duke.edu>  
 *            
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Duke University may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY DUKE UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DUKE UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITSOR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 */

/*
 * This is a set of routines for allocating large-sized mbuf payload
 * areas, and is primarily intended for use in receive side mbuf
 * allocation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/uio.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_pageout.h>
#include <sys/vmmeter.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <sys/proc.h>
#include <sys/jumbo.h>

/*
 * XXX this may be too high or too low.
 */
#define JUMBO_MAX_PAGES 3072

struct jumbo_kmap {
	vm_offset_t kva;
	SLIST_ENTRY(jumbo_kmap) entries;     /* Singly-linked List. */
};

static SLIST_HEAD(jumbo_kmap_head, jumbo_kmap) jumbo_kmap_free,
					       jumbo_kmap_inuse;

static struct mtx jumbo_mutex;
MTX_SYSINIT(jumbo_lock, &jumbo_mutex, "jumbo mutex", MTX_DEF);

static struct vm_object *jumbo_vm_object;
static unsigned long jumbo_vmuiomove_pgs_freed = 0;
#if 0
static int jumbo_vm_wakeup_wanted = 0;
#endif
vm_offset_t jumbo_basekva;

int
jumbo_vm_init(void)
{
	int i;
	struct jumbo_kmap *entry;

	mtx_lock(&jumbo_mutex);

	if (jumbo_vm_object != NULL) {
		mtx_unlock(&jumbo_mutex);
		return (1);
	}

	/* allocate our object */
	jumbo_vm_object = vm_object_allocate_wait(OBJT_DEFAULT, JUMBO_MAX_PAGES,
						  M_NOWAIT);

	if (jumbo_vm_object == NULL) {
		mtx_unlock(&jumbo_mutex);
		return (0);
	}

	SLIST_INIT(&jumbo_kmap_free);
	SLIST_INIT(&jumbo_kmap_inuse);

	/* grab some kernel virtual address space */
	jumbo_basekva = kmem_alloc_pageable(kernel_map,
		PAGE_SIZE * JUMBO_MAX_PAGES);
	if (jumbo_basekva == 0) {
		vm_object_deallocate(jumbo_vm_object);
		jumbo_vm_object = NULL;
		mtx_unlock(&jumbo_mutex);
		return 0;
	}
	for (i = 0; i < JUMBO_MAX_PAGES; i++) {
		entry = malloc(sizeof(struct jumbo_kmap), M_TEMP, M_NOWAIT);
		if (!entry && !i)  {
			mtx_unlock(&jumbo_mutex);
			panic("jumbo_vm_init: unable to allocated kvas");
		} else if (!entry) {
			printf("warning: jumbo_vm_init allocated only %d kva\n",
			       i);
			mtx_unlock(&jumbo_mutex);
			return 1;
		}
		entry->kva = jumbo_basekva + (vm_offset_t)i * PAGE_SIZE;
		SLIST_INSERT_HEAD(&jumbo_kmap_free, entry, entries);
	}
	mtx_unlock(&jumbo_mutex);
	return 1;
}

void
jumbo_freem(void *addr, void *args)
{
	vm_page_t frame;

	frame = PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)addr));

	/*
	 * Need giant for looking at the hold count below.  Convert this
	 * to the vm mutex once the VM code has been moved out from under
	 * giant.
	 */
	GIANT_REQUIRED;

	if (frame->hold_count == 0)
		jumbo_pg_free((vm_offset_t)addr);
	else
		printf("jumbo_freem: hold count for %p is %d!!??\n",
		       frame, frame->hold_count);
}

void 
jumbo_pg_steal(vm_page_t pg)
{
	vm_offset_t addr;
	struct jumbo_kmap *entry;

	addr = ptoa(pg->pindex) + jumbo_basekva;

	if (pg->object != jumbo_vm_object)
		panic("stealing a non jumbo_vm_object page");
	vm_page_remove(pg);

	mtx_lock(&jumbo_mutex);

	pmap_qremove(addr,1);
	entry = SLIST_FIRST(&jumbo_kmap_inuse);
	entry->kva = addr;
	SLIST_REMOVE_HEAD(&jumbo_kmap_inuse, entries);
	SLIST_INSERT_HEAD(&jumbo_kmap_free, entry, entries);

	mtx_unlock(&jumbo_mutex);

#if 0
	if (jumbo_vm_wakeup_wanted)
		wakeup(jumbo_vm_object);
#endif
}


vm_page_t 
jumbo_pg_alloc(void)
{
	vm_page_t pg;
	vm_pindex_t pindex;
	struct jumbo_kmap *entry;

 	pg = NULL;
	mtx_lock(&jumbo_mutex);

	entry = SLIST_FIRST(&jumbo_kmap_free);
	if (entry != NULL){
		pindex = atop(entry->kva - jumbo_basekva);
		VM_OBJECT_LOCK(jumbo_vm_object);
		pg = vm_page_alloc(jumbo_vm_object, pindex, VM_ALLOC_INTERRUPT);
		VM_OBJECT_UNLOCK(jumbo_vm_object);
		if (pg != NULL) {
			SLIST_REMOVE_HEAD(&jumbo_kmap_free, entries);
			SLIST_INSERT_HEAD(&jumbo_kmap_inuse, entry, entries);
			pmap_qenter(entry->kva, &pg, 1);
		}
	}
	mtx_unlock(&jumbo_mutex);
	return(pg);
}

void 
jumbo_pg_free(vm_offset_t addr)
{
	struct jumbo_kmap *entry;
	vm_paddr_t paddr;
	vm_page_t pg;

	paddr = pmap_kextract((vm_offset_t)addr);
	pg = PHYS_TO_VM_PAGE(paddr);

	if (pg->object != jumbo_vm_object) {
		jumbo_vmuiomove_pgs_freed++;
/*		if(vm_page_lookup(jumbo_vm_object, atop(addr - jumbo_basekva)))
			panic("vm_page_rename didn't");
		printf("freeing uiomoved pg:\t pindex = %d, padd = 0x%lx\n",
		       atop(addr - jumbo_basekva), paddr);
*/
	} else {
		vm_page_lock_queues();
		vm_page_busy(pg); /* vm_page_free wants pages to be busy*/
		vm_page_free(pg);
		vm_page_unlock_queues();
	}

	mtx_lock(&jumbo_mutex);

	pmap_qremove(addr,1);
	entry = SLIST_FIRST(&jumbo_kmap_inuse);
	entry->kva = addr;
	SLIST_REMOVE_HEAD(&jumbo_kmap_inuse, entries);
	SLIST_INSERT_HEAD(&jumbo_kmap_free, entry, entries);

	mtx_unlock(&jumbo_mutex);

#if 0
	if (jumbo_vm_wakeup_wanted)
		wakeup(jumbo_vm_object);
#endif
}
