/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Based on sys/vm/device_pager.c.
 */

static const char vpsid[] =
    "$Id: vps_pager.c 153 2013-06-03 16:18:17Z klaus $";

/*
 * This pager makes it possible to provide a linear snapshot of
 * a vps instance's userspace memory.
 * It only maps pages that really exist, untouched virtual memory
 * regions don't get included.
 *
 * Zero copying is done, instead fake pages are used that are
 * allocated on demand and the backing real pages are wired down.
 * On putting of those fake pages the backing real pages are
 * unwired, so that they can be paged out any time necessary.
 *
 * Per snapshot exactly one vm object is provided to the vpsctl
 * process. It includes the system pages and the user pages.
 *
 * Since all activity of a vps instance is suspended during
 * snapshot there can't be any changes in vm maps or missing
 * objects (tm).
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/sx.h>
#include <sys/malloc.h>

#include <vps/vps.h>
#include <vps/vps2.h>
#include <vps/vps_int.h>
#include <vps/vps_snapst.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

int vps_pager_put_object(vm_object_t object, long desired);

static void vps_pager_init(void);
static vm_object_t vps_pager_alloc(void *, vm_ooffset_t, vm_prot_t,
	vm_ooffset_t, struct ucred *);
static void vps_pager_dealloc(vm_object_t);
static int vps_pager_getpages(vm_object_t, vm_page_t *, int, int);
static void vps_pager_putpages(vm_object_t, vm_page_t *, int,
	boolean_t, int *);
static boolean_t vps_pager_haspage(vm_object_t, vm_pindex_t, int *,
	int *);
void vps_pager_lowmem(void *);

struct pagerops vps_pager_ops = {
	.pgo_init =     vps_pager_init,
	.pgo_alloc =    vps_pager_alloc,
	.pgo_dealloc =  vps_pager_dealloc,
	.pgo_getpages = vps_pager_getpages,
	.pgo_putpages = vps_pager_putpages,
	.pgo_haspage =  vps_pager_haspage,
};

/* list of vps pager objects */
static struct pagerlst vps_pager_object_list;

/* protect list manipulation */
static struct mtx vps_pager_mtx;

static int lowmem_registered;

static void
vps_pager_init(void)
{

	TAILQ_INIT(&vps_pager_object_list);
	mtx_init(&vps_pager_mtx, "vps_pager list", NULL, MTX_DEF);
}

static vm_object_t
vps_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
	vm_ooffset_t foff, struct ucred *cred)
{
	vm_object_t object;

	if (lowmem_registered == 0) {
		lowmem_registered = 1;
		EVENTHANDLER_REGISTER(vm_lowmem, vps_pager_lowmem, NULL,
			EVENTHANDLER_PRI_ANY);
	}

	object = vm_object_allocate(OBJT_VPS, size);

	object->handle = handle;
	TAILQ_INIT(&object->un_pager.devp.devp_pglist);

	DBGCORE("%s: handle=%p size=%zu\n", __func__, handle, (size_t)size);

	mtx_lock(&vps_pager_mtx);
	TAILQ_INSERT_TAIL(&vps_pager_object_list, object,
	    pager_object_list);
	mtx_unlock(&vps_pager_mtx);

	return (object);
}

static void
vps_pager_dealloc(vm_object_t object)
{
	vm_object_t object2;
	vm_page_t *marr;
	vm_page_t m1;
	int *rtvals;
	int found;
	int count;
	int i;

	count = object->resident_page_count;
	VM_OBJECT_WUNLOCK(object);

	mtx_lock(&vps_pager_mtx);
	found = 0;
	TAILQ_FOREACH(object2, &vps_pager_object_list, pager_object_list)
		if (object2 == object)
			found = 1;
	if (found == 0) {
		/* Already removed.
		   XXX find out why dealloc is called twice ! */
		mtx_unlock(&vps_pager_mtx);
		VM_OBJECT_WLOCK(object);
		return;
	}
	TAILQ_REMOVE(&vps_pager_object_list, object, pager_object_list);
	mtx_unlock(&vps_pager_mtx);

	marr = malloc(sizeof(vm_page_t) * count, M_TEMP, M_WAITOK);
	rtvals = malloc(sizeof(int) * count, M_TEMP, M_WAITOK);

	VM_OBJECT_WLOCK(object);

	i = 0;
	TAILQ_FOREACH(m1, &object->memq, listq) {
		marr[i] = m1;
		i++;
	}

	vps_pager_putpages(object, marr, count, 0, rtvals);

	VM_OBJECT_WUNLOCK(object);

	free(marr, M_TEMP);
	free(rtvals, M_TEMP);

	VM_OBJECT_WLOCK(object);

	DBGCORE("%s: vps_pager_dealloc: object=%p, freed %d pages\n",
		__func__, object, count);
}

static vm_map_entry_t
vps_vm_object_in_map(vm_map_t map, vm_object_t object)
{
	vm_map_entry_t entry;
	vm_object_t obj;
	int entcount;

	/* XXX submaps */

	entry = map->header.next;
	entcount = map->nentries;
	while (entcount-- && (entry != &map->header)) {
		if ((obj = entry->object.vm_object) != NULL) {
			for (; obj; obj = obj->backing_object) {
				if (obj == object) {
					return (entry);
				}
			}
		}
	}

	return (NULL);
}

/*
 * lowmem event
 */
void
vps_pager_lowmem(void *arg)
{
	vm_object_t object, object2;
	vm_page_t *marr;
	vm_page_t m1;
	vm_map_entry_t entry;
	struct proc *p;
	vm_map_t map;
	int *rtvals;
	int maxcount = 0x1000;
	int i, j;


	/*
	 * Determine which pages we can release:
	 * The lower pindexes are less likely to be accessed soon again.
	 */

	if ((marr = malloc(sizeof(vm_page_t) * maxcount,
	    M_TEMP, M_NOWAIT)) == NULL)
		return;
	if ((rtvals = malloc(sizeof(int) * maxcount,
	    M_TEMP, M_NOWAIT)) == NULL) {
		free(marr, M_TEMP);
		return;
	}

	mtx_lock(&vps_pager_mtx);
	TAILQ_FOREACH_SAFE(object, &vps_pager_object_list,
	    pager_object_list, object2) {
		mtx_unlock(&vps_pager_mtx);
		VM_OBJECT_WLOCK(object);

		if ((object->flags & OBJ_DEAD) == 0 &&
			object->resident_page_count > 0) {


			/* Release the first half of resident pages
			   in object. */
			/* memq is sorted */

			i = 0;
			TAILQ_FOREACH(m1, &object->memq, listq) {
				marr[i] = m1;
				if (i >= maxcount)
					break;
				if (i >= object->resident_page_count / 2)
					break;
				i++;
			}

			VM_OBJECT_WUNLOCK(object);

			/*
			 * We have to manually release all pmap mappings
			 * since our pages are fictious.
			 * Worst of all, we have to walk through all
			 * processes vmspace maps to find out which map
			 * refers to our object.
			 */
			/* XXX per VPS */
			sx_slock(&V_allproc_lock);
			FOREACH_PROC_IN_SYSTEM(p) {
				PROC_LOCK(p);
				map = &p->p_vmspace->vm_map;
				PROC_UNLOCK(p);
				if (map == NULL)
					continue;
				if ((entry = vps_vm_object_in_map(map,
				    object)) == NULL)
					continue;

				for (j = 0; j < i; j++) {
					pmap_remove(map->pmap,
					    entry->start +
					    (marr[j]->pindex << PAGE_SHIFT),
					    entry->start +
					    ((marr[j]->pindex+1) <<
					    PAGE_SHIFT));
				}
			}
			sx_sunlock(&V_allproc_lock);

			VM_OBJECT_WLOCK(object);

			vps_pager_putpages(object, marr, i, 0, rtvals);

			DBGCORE("%s: object=%p freed %d pages\n",
				__func__, object, i);

		}

		VM_OBJECT_WUNLOCK(object);
		mtx_lock(&vps_pager_mtx);
	}
	mtx_unlock(&vps_pager_mtx);

	free(marr, M_TEMP);
	free(rtvals, M_TEMP);

}

/*
 * Called from pageout daemon whenever memory should be reclaimed.
 *
 * We put our fake pages in the requested object,
 * which in turn unwires the backing pages.
 * They can be paged out then any time.
 */

int
vps_pager_put_object(vm_object_t object, long desired)
{
	vm_page_t *marr;
	vm_page_t m1;
	int *rtvals;
	int count;
	int i;

	DBGCORE("%s: ENT object=%p desired=%ld nresident=%d\n",
		__func__, object, desired, object->resident_page_count);

	count = object->resident_page_count;
	VM_OBJECT_WUNLOCK(object);

	marr = malloc(sizeof(vm_page_t) * count, M_TEMP, M_WAITOK);
	rtvals = malloc(sizeof(int) * count, M_TEMP, M_WAITOK);

	VM_OBJECT_WLOCK(object);

	i = 0;
	TAILQ_FOREACH(m1, &object->memq, listq) {
		marr[i] = m1;
		i++;
	}

	vps_pager_putpages(object, marr, count, 0, rtvals);

	VM_OBJECT_WUNLOCK(object);

	free(marr, M_TEMP);
	free(rtvals, M_TEMP);

	VM_OBJECT_WLOCK(object);

	DBGCORE("%s: RET object=%p nresident=%d\n",
		__func__, object, object->resident_page_count);

	return (count);
}

static int
vps_pager_getpages(vm_object_t object, vm_page_t *m, int count, int reqpage)
{
	struct vps_snapst_ctx *ctx;
	struct vps_page_ref *pref;
	vm_memattr_t memattr;
	vm_pindex_t pidx2;
	vm_pindex_t pidx;
	vm_paddr_t paddr;
	vm_object_t obj2;
	vm_page_t m1;
	vm_page_t m2;
	int origin;
	int rv;
	int i;
	char need_wakeup = 0;

	VM_OBJECT_ASSERT_WLOCKED(object);

	KASSERT(object->handle != NULL,
		("%s: handle == NULL, object=%p\n", __func__, object));

	ctx = (struct vps_snapst_ctx *)object->handle;
	pidx = m[reqpage]->pindex;
	m1 = m[reqpage];
	memattr = object->memattr;

	VM_OBJECT_WUNLOCK(object);

	if (pidx < ctx->nsyspages) {
		/* system pages */

		obj2 = ctx->vmobj;
		pidx2 = pidx;
		origin = 0;

	} else if (pidx >= ctx->nsyspages &&
	    pidx < (ctx->nsyspages + ctx->nuserpages)) {
		/* user pages */

		pref = &ctx->page_ref[pidx - ctx->nsyspages];
		obj2 = pref->obj;
		pidx2 = pref->pidx;
		origin = pref->origin;

	} else {
		/* invalid */
		VM_OBJECT_WLOCK(object);
		DBGCORE("%s: requested invalid pindex %zu, object=%p\n",
			__func__, (size_t)pidx, object);
		return (VM_PAGER_FAIL);
	}

	VM_OBJECT_WLOCK(obj2);
	if (obj2->type != OBJT_SWAP) {
		m2 = vm_page_lookup(obj2, pidx2);
		/* XXX also here there is no page sometimes
		KASSERT(m2 != NULL, ("%s: obj2=%p type=%x pidx2=0x%lx "
		    "no page\n", __func__, obj2, obj2->type, pidx2));
		*/

		if (m2)
			ctx->pager_npages_res++;

	} else if (obj2->type == OBJT_SWAP) {
		m2 = vm_page_lookup(obj2, pidx2);
		if (m2 == NULL) {
			m2 = vm_page_grab(obj2, pidx2, VM_ALLOC_NORMAL |
			    VM_ALLOC_RETRY);
			pmap_zero_page(m2);
			rv = vm_pager_get_pages(obj2, &m2, 1, 0);
			/*
			XXX
			XXX
			XXX
			if (rv != VM_PAGER_OK)
			*/
			if (rv == VM_PAGER_ERROR)
				panic("%s: vm_pager_get_pages: rv = %d\n"
				    "obj1=%p pidx1=%zu obj2=%p pidx=%zu\n",
				    __func__, rv, object, (size_t)pidx,
				    obj2, (size_t)pidx2);
			/* XXX
			KASSERT(m2->valid == VM_PAGE_BITS_ALL,
			    ("%s: m2=%p valid=0x%x\n",
			    __func__, m2, m2->valid));
			*/
			need_wakeup = 1;

			if (m2)
				ctx->pager_npages_swap++;

			/* Note that this page is not physically mapped
			   now in user space. */
		} else {
			ctx->pager_npages_res++;
		}
	} else {
		panic("%s: obj2=%p type=%x\n", __func__, obj2, obj2->type);
	}

	/* relookup */
	if ((m2 = vm_page_lookup(obj2, pidx2))) {
		vm_page_lock(m2);
		vm_page_wire(m2);
		paddr = m2->phys_addr;
		vm_page_unlock(m2);
		/*
		KASSERT(m2->valid == VM_PAGE_BITS_ALL,
		    ("%s: m2=%p valid=0x%x\n",
		    __func__, m2, m2->valid));
		*/
	} else {
		/* XXX
		if (vm_pager_has_page(obj2, pidx2, NULL, NULL) == FALSE)
			panic("%s: page vanished ! obj2=%p pidx=0x%lx\n",
			    __func__, obj2, pidx2);
		else
			panic("%s: pager has page but doesn't come in ! "
			    "obj2=%p pidx=0x%lx\n",
			    __func__, obj2, pidx2);
		*/
		/* We still don't have a page, give up. */
		ctx->pager_npages_miss++;
		VM_OBJECT_WUNLOCK(obj2);
		VM_OBJECT_WLOCK(object);
		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				vm_page_lock(m[i]);
				vm_page_free(m[i]);
				vm_page_unlock(m[i]);
			}
		}
		/* XXX */
		DBGCORE("%s: lost page: obj=%p pidx=%zu origin=%d\n",
			__func__, obj2, (size_t)pidx2, origin);
		return (VM_PAGER_FAIL);
	}

	if (need_wakeup)
		vm_page_wakeup(m2);

	VM_OBJECT_WUNLOCK(obj2);

	/* Now we have a page. */

	/* 0 <-- always replace page; makes cleanup easier. */
	if (0 && (m1->flags & PG_FICTITIOUS) != 0) {
		VM_OBJECT_WLOCK(object);
		vm_page_updatefake(m1, paddr, object->memattr);
		if (count > 1) {
			for (i = 0; i < count; i++) {
				if (i != reqpage) {
					vm_page_lock(m[i]);
					vm_page_free(m[i]);
					vm_page_unlock(m[i]);
				}
			}
		}

	} else {
		/*
		 * Replace the passed in reqpage page with our own
		 * fake page and free up all of the original pages.
		 */
		m1 = vm_page_getfake(paddr, object->memattr);
		VM_OBJECT_WLOCK(object);
		TAILQ_INSERT_TAIL(&object->un_pager.devp.devp_pglist,
		    m1, pageq);
		for (i = 0; i < count; i++) {
			vm_page_lock(m[i]);
			vm_page_free(m[i]);
			vm_page_unlock(m[i]);
		}
		vm_page_insert(m1, object, pidx);
		vm_page_lock(m1);

		/* Since page is VPO_UNMANAGED, it is not put on
		   any queues. */
		vm_page_unwire(m1, 1);

		KASSERT(m1->flags & PG_FICTITIOUS,
			("%s: m1=%p not PG_FICTITIOUS\n", __func__, m1));

		vm_page_unlock(m1);
		m[reqpage] = m1;

	}

	m1->valid = VM_PAGE_BITS_ALL;

	if (pidx + 1 == (ctx->nsyspages + ctx->nuserpages))
		DBGCORE("%s: pages: resident: %d swap: %d missing: %d\n",
			__func__,
			ctx->pager_npages_res,
			ctx->pager_npages_swap,
			ctx->pager_npages_miss);

	return (VM_PAGER_OK);
}

static void
vps_pager_putpages(vm_object_t object, vm_page_t *m, int count,
	boolean_t sync, int *rtvals)
{
	struct vps_snapst_ctx *ctx;
	struct vps_page_ref *pref;
	vm_pindex_t pidx2;
	vm_pindex_t pidx;
	vm_object_t obj2;
	vm_page_t m1;
	vm_page_t m2;
	int i;

	DBGCORE("%s: object=%p m=%p count=%d sync=%d rtvals=%p\n",
		__func__, object, m, count, sync, rtvals);

	KASSERT(object->handle != NULL,
		("%s: handle == NULL, object=%p\n", __func__, object));

	ctx = (struct vps_snapst_ctx *)object->handle;

	for (i = 0; i < count; i++) {
		m1 = m[i];
		KASSERT(m1 != NULL,
		    ("%s: m1 == NULL, i=%d\n", __func__, i));
		pidx = m1->pindex;

		vm_page_lock(m1);
		if (m1->hold_count != 0) {
			/* Not possible now. */
			vm_page_unlock(m1);
			rtvals[i] = VM_PAGER_FAIL;
			continue;
		}
		TAILQ_REMOVE(&object->un_pager.devp.devp_pglist, m1, pageq);
		vm_page_remove(m1);
		vm_page_unlock(m1);
		vm_page_putfake(m1);

		if (pidx < ctx->nsyspages) {
			/* system pages */

			obj2 = ctx->vmobj;
			pidx2 = pidx;

		} else if (pidx >= ctx->nsyspages &&
		    pidx < (ctx->nsyspages + ctx->nuserpages)) {
			/* user pages */

			pref = &ctx->page_ref[pidx - ctx->nsyspages];
			obj2 = pref->obj;
			pidx2 = pref->pidx;

		} else {
			DBGCORE("%s: invalid pidx %zu, object=%p\n",
				__func__, (size_t)pidx, object);
			rtvals[i] = VM_PAGER_FAIL;
			continue;
		}

		VM_OBJECT_WLOCK(obj2);
		m2 = vm_page_lookup(obj2, pidx2);
		VM_OBJECT_WUNLOCK(obj2);
		KASSERT(m2 != NULL, ("%s: m2 == NULL\n", __func__));
		vm_page_lock(m2);
		vm_page_unwire(m2, 0);
		vm_page_unlock(m2);

		rtvals[i] = VM_PAGER_OK;
	}
}

static boolean_t
vps_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before,
    int *after)
{
	struct vps_snapst_ctx *ctx;
	struct vps_page_ref *ref;

	KASSERT(object->handle != NULL,
		("%s: handle == NULL, object=%p\n", __func__, object));

	ctx = (struct vps_snapst_ctx *)object->handle;
	ref = ctx->page_ref;

	KASSERT(object == ctx->vps_vmobject,
		("%s: object != ctx->vps_vmobject; object=%p pindex=%zu\n",
		__func__, object, (size_t)pindex));

	if (pindex >= (ctx->nsyspages + ctx->nuserpages))
		return (FALSE);

	if (before)
		*before = pindex;
	if (after)
		*after = pindex < (ctx->nsyspages + ctx->nuserpages) ?
			(ctx->nsyspages + ctx->nuserpages) - pindex - 1 :
			0;

	return (TRUE);
}

/* EOF */
