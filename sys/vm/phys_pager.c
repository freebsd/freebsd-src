/*-
 * Copyright (c) 2000 Peter Wemm
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/vm/phys_pager.c,v 1.28.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker_set.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

/* list of phys pager objects */
static struct pagerlst phys_pager_object_list;
/* protect access to phys_pager_object_list */
static struct mtx phys_pager_mtx;

static void
phys_pager_init(void)
{

	TAILQ_INIT(&phys_pager_object_list);
	mtx_init(&phys_pager_mtx, "phys_pager list", NULL, MTX_DEF);
}

/*
 * MPSAFE
 */
static vm_object_t
phys_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
		 vm_ooffset_t foff)
{
	vm_object_t object, object1;
	vm_pindex_t pindex;

	/*
	 * Offset should be page aligned.
	 */
	if (foff & PAGE_MASK)
		return (NULL);

	pindex = OFF_TO_IDX(foff + PAGE_MASK + size);

	if (handle != NULL) {
		mtx_lock(&phys_pager_mtx);
		/*
		 * Look up pager, creating as necessary.
		 */
		object1 = NULL;
		object = vm_pager_object_lookup(&phys_pager_object_list, handle);
		if (object == NULL) {
			/*
			 * Allocate object and associate it with the pager.
			 */
			mtx_unlock(&phys_pager_mtx);
			object1 = vm_object_allocate(OBJT_PHYS, pindex);
			mtx_lock(&phys_pager_mtx);
			object = vm_pager_object_lookup(&phys_pager_object_list,
			    handle);
			if (object != NULL) {
				/*
				 * We raced with other thread while
				 * allocating object.
				 */
				if (pindex > object->size)
					object->size = pindex;
			} else {
				object = object1;
				object1 = NULL;
				object->handle = handle;
				TAILQ_INSERT_TAIL(&phys_pager_object_list, object,
				    pager_object_list);
			}
		} else {
			if (pindex > object->size)
				object->size = pindex;
		}
		mtx_unlock(&phys_pager_mtx);
		vm_object_deallocate(object1);
	} else {
		object = vm_object_allocate(OBJT_PHYS, pindex);
	}

	return (object);
}

/*
 * MPSAFE
 */
static void
phys_pager_dealloc(vm_object_t object)
{

	if (object->handle != NULL) {
		VM_OBJECT_UNLOCK(object);
		mtx_lock(&phys_pager_mtx);
		TAILQ_REMOVE(&phys_pager_object_list, object, pager_object_list);
		mtx_unlock(&phys_pager_mtx);
		VM_OBJECT_LOCK(object);
	}
}

/*
 * Fill as many pages as vm_fault has allocated for us.
 */
static int
phys_pager_getpages(vm_object_t object, vm_page_t *m, int count, int reqpage)
{
	int i;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	for (i = 0; i < count; i++) {
		if (m[i]->valid == 0) {
			if ((m[i]->flags & PG_ZERO) == 0)
				pmap_zero_page(m[i]);
			m[i]->valid = VM_PAGE_BITS_ALL;
		}
		KASSERT(m[i]->valid == VM_PAGE_BITS_ALL,
		    ("phys_pager_getpages: partially valid page %p", m[i]));
		m[i]->dirty = 0;
		/* The requested page must remain busy, the others not. */
		if (reqpage != i) {
			m[i]->oflags &= ~VPO_BUSY;
			m[i]->busy = 0;
		}
	}
	return (VM_PAGER_OK);
}

static void
phys_pager_putpages(vm_object_t object, vm_page_t *m, int count, boolean_t sync,
		    int *rtvals)
{

	panic("phys_pager_putpage called");
}

/*
 * Implement a pretty aggressive clustered getpages strategy.  Hint that
 * everything in an entire 4MB window should be prefaulted at once.
 *
 * XXX 4MB (1024 slots per page table page) is convenient for x86,
 * but may not be for other arches.
 */
#ifndef PHYSCLUSTER
#define PHYSCLUSTER 1024
#endif
static boolean_t
phys_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before,
		   int *after)
{
	vm_pindex_t base, end;

	base = pindex & (~(PHYSCLUSTER - 1));
	end = base + (PHYSCLUSTER - 1);
	if (before != NULL)
		*before = pindex - base;
	if (after != NULL)
		*after = end - pindex;
	return (TRUE);
}

struct pagerops physpagerops = {
	.pgo_init =	phys_pager_init,
	.pgo_alloc =	phys_pager_alloc,
	.pgo_dealloc = 	phys_pager_dealloc,
	.pgo_getpages =	phys_pager_getpages,
	.pgo_putpages =	phys_pager_putpages,
	.pgo_haspage =	phys_pager_haspage,
};
