/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/pctrie.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>

/* list of phys pager objects */
static struct pagerlst phys_pager_object_list;
/* protect access to phys_pager_object_list */
static struct mtx phys_pager_mtx;

static int default_phys_pager_getpages(vm_object_t object, vm_page_t *m,
    int count, int *rbehind, int *rahead);
static int default_phys_pager_populate(vm_object_t object, vm_pindex_t pidx,
    int fault_type, vm_prot_t max_prot, vm_pindex_t *first, vm_pindex_t *last);
static boolean_t default_phys_pager_haspage(vm_object_t object,
    vm_pindex_t pindex, int *before, int *after);
const struct phys_pager_ops default_phys_pg_ops = {
	.phys_pg_getpages = default_phys_pager_getpages,
	.phys_pg_populate = default_phys_pager_populate,
	.phys_pg_haspage = default_phys_pager_haspage,
	.phys_pg_ctor = NULL,
	.phys_pg_dtor = NULL,
};

static void
phys_pager_init(void)
{

	TAILQ_INIT(&phys_pager_object_list);
	mtx_init(&phys_pager_mtx, "phys_pager list", NULL, MTX_DEF);
}

vm_object_t
phys_pager_allocate(void *handle, const struct phys_pager_ops *ops, void *data,
    vm_ooffset_t size, vm_prot_t prot, vm_ooffset_t foff, struct ucred *cred)
{
	vm_object_t object, object1;
	vm_pindex_t pindex;
	bool init;

	/*
	 * Offset should be page aligned.
	 */
	if (foff & PAGE_MASK)
		return (NULL);

	pindex = OFF_TO_IDX(foff + PAGE_MASK + size);
	init = true;

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
				init = false;
			} else {
				object = object1;
				object1 = NULL;
				object->handle = handle;
				object->un_pager.phys.ops = ops;
				object->un_pager.phys.data_ptr = data;
				if (ops->phys_pg_populate != NULL)
					vm_object_set_flag(object, OBJ_POPULATE);
				TAILQ_INSERT_TAIL(&phys_pager_object_list,
				    object, pager_object_list);
			}
		} else {
			if (pindex > object->size)
				object->size = pindex;
		}
		mtx_unlock(&phys_pager_mtx);
		vm_object_deallocate(object1);
	} else {
		object = vm_object_allocate(OBJT_PHYS, pindex);
		object->un_pager.phys.ops = ops;
		object->un_pager.phys.data_ptr = data;
		if (ops->phys_pg_populate != NULL)
			vm_object_set_flag(object, OBJ_POPULATE);
	}
	if (init && ops->phys_pg_ctor != NULL)
		ops->phys_pg_ctor(object, prot, foff, cred);

	return (object);
}

static vm_object_t
phys_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *ucred)
{
	return (phys_pager_allocate(handle, &default_phys_pg_ops, NULL,
	    size, prot, foff, ucred));
}

static void
phys_pager_dealloc(vm_object_t object)
{

	if (object->handle != NULL) {
		VM_OBJECT_WUNLOCK(object);
		mtx_lock(&phys_pager_mtx);
		TAILQ_REMOVE(&phys_pager_object_list, object, pager_object_list);
		mtx_unlock(&phys_pager_mtx);
		VM_OBJECT_WLOCK(object);
	}
	object->type = OBJT_DEAD;
	if (object->un_pager.phys.ops->phys_pg_dtor != NULL)
		object->un_pager.phys.ops->phys_pg_dtor(object);
	object->handle = NULL;
}

/*
 * Fill as many pages as vm_fault has allocated for us.
 */
static int
default_phys_pager_getpages(vm_object_t object, vm_page_t *m, int count,
    int *rbehind, int *rahead)
{
	int i;

	for (i = 0; i < count; i++) {
		if (vm_page_none_valid(m[i])) {
			if ((m[i]->flags & PG_ZERO) == 0)
				pmap_zero_page(m[i]);
			vm_page_valid(m[i]);
		}
		KASSERT(vm_page_all_valid(m[i]),
		    ("phys_pager_getpages: partially valid page %p", m[i]));
		KASSERT(m[i]->dirty == 0,
		    ("phys_pager_getpages: dirty page %p", m[i]));
	}
	if (rbehind)
		*rbehind = 0;
	if (rahead)
		*rahead = 0;
	return (VM_PAGER_OK);
}

static int
phys_pager_getpages(vm_object_t object, vm_page_t *m, int count, int *rbehind,
    int *rahead)
{
	return (object->un_pager.phys.ops->phys_pg_getpages(object, m,
	    count, rbehind, rahead));
}

/*
 * Implement a pretty aggressive clustered getpages strategy.  Hint that
 * everything in an entire 4MB window should be prefaulted at once.
 *
 * 4MB (1024 slots per page table page) is convenient for x86,
 * but may not be for other arches.
 */
#ifndef PHYSCLUSTER
#define PHYSCLUSTER 1024
#endif
static int phys_pager_cluster = PHYSCLUSTER;
SYSCTL_INT(_vm, OID_AUTO, phys_pager_cluster, CTLFLAG_RWTUN,
    &phys_pager_cluster, 0,
    "prefault window size for phys pager");

/*
 * Max hint to vm_page_alloc() about the further allocation needs
 * inside the phys_pager_populate() loop.  The number of bits used to
 * implement VM_ALLOC_COUNT() determines the hard limit on this value.
 * That limit is currently 65535.
 */
#define	PHYSALLOC	16

static int
default_phys_pager_populate(vm_object_t object, vm_pindex_t pidx,
    int fault_type __unused, vm_prot_t max_prot __unused, vm_pindex_t *first,
    vm_pindex_t *last)
{
	struct pctrie_iter pages;
	vm_page_t m;
	vm_pindex_t base, end, i;
	int ahead;

	VM_OBJECT_ASSERT_WLOCKED(object);
	base = rounddown(pidx, phys_pager_cluster);
	end = base + phys_pager_cluster - 1;
	if (end >= object->size)
		end = object->size - 1;
	if (*first > base)
		base = *first;
	if (end > *last)
		end = *last;
	*first = base;
	*last = end;
	vm_page_iter_init(&pages, object);

	for (i = base; i <= end; i++) {
		ahead = MIN(end - i, PHYSALLOC);
		m = vm_page_grab_iter(object, i,
		    VM_ALLOC_NORMAL | VM_ALLOC_COUNT(ahead), &pages);
		if (!vm_page_all_valid(m))
			vm_page_zero_invalid(m, TRUE);
		KASSERT(m->dirty == 0,
		    ("phys_pager_populate: dirty page %p", m));
	}
	return (VM_PAGER_OK);
}

static int
phys_pager_populate(vm_object_t object, vm_pindex_t pidx, int fault_type,
    vm_prot_t max_prot, vm_pindex_t *first, vm_pindex_t *last)
{
	return (object->un_pager.phys.ops->phys_pg_populate(object, pidx,
	    fault_type, max_prot, first, last));
}

static void
phys_pager_putpages(vm_object_t object, vm_page_t *m, int count, int flags,
    int *rtvals)
{

	panic("phys_pager_putpage called");
}

static boolean_t
default_phys_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before,
    int *after)
{
	vm_pindex_t base, end;

	base = rounddown(pindex, phys_pager_cluster);
	end = base + phys_pager_cluster - 1;
	if (before != NULL)
		*before = pindex - base;
	if (after != NULL)
		*after = end - pindex;
	return (TRUE);
}

static boolean_t
phys_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before,
    int *after)
{
	return (object->un_pager.phys.ops->phys_pg_haspage(object, pindex,
	    before, after));
}

const struct pagerops physpagerops = {
	.pgo_kvme_type = KVME_TYPE_PHYS,
	.pgo_init =	phys_pager_init,
	.pgo_alloc =	phys_pager_alloc,
	.pgo_dealloc = 	phys_pager_dealloc,
	.pgo_getpages =	phys_pager_getpages,
	.pgo_putpages =	phys_pager_putpages,
	.pgo_haspage =	phys_pager_haspage,
	.pgo_populate =	phys_pager_populate,
};
