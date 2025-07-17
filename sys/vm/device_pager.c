/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <sys/user.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/uma.h>

static void dev_pager_init(void);
static vm_object_t dev_pager_alloc(void *, vm_ooffset_t, vm_prot_t,
    vm_ooffset_t, struct ucred *);
static void dev_pager_dealloc(vm_object_t);
static int dev_pager_getpages(vm_object_t, vm_page_t *, int, int *, int *);
static void dev_pager_putpages(vm_object_t, vm_page_t *, int, int, int *);
static boolean_t dev_pager_haspage(vm_object_t, vm_pindex_t, int *, int *);
static void dev_pager_free_page(vm_object_t object, vm_page_t m);
static int dev_pager_populate(vm_object_t object, vm_pindex_t pidx,
    int fault_type, vm_prot_t, vm_pindex_t *first, vm_pindex_t *last);

/* list of device pager objects */
static struct pagerlst dev_pager_object_list;
/* protect list manipulation */
static struct mtx dev_pager_mtx;

const struct pagerops devicepagerops = {
	.pgo_kvme_type = KVME_TYPE_DEVICE,
	.pgo_init =	dev_pager_init,
	.pgo_alloc =	dev_pager_alloc,
	.pgo_dealloc =	dev_pager_dealloc,
	.pgo_getpages =	dev_pager_getpages,
	.pgo_putpages =	dev_pager_putpages,
	.pgo_haspage =	dev_pager_haspage,
};

const struct pagerops mgtdevicepagerops = {
	.pgo_kvme_type = KVME_TYPE_MGTDEVICE,
	.pgo_alloc =	dev_pager_alloc,
	.pgo_dealloc =	dev_pager_dealloc,
	.pgo_getpages =	dev_pager_getpages,
	.pgo_putpages =	dev_pager_putpages,
	.pgo_haspage =	dev_pager_haspage,
	.pgo_populate =	dev_pager_populate,
};

static int old_dev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color);
static void old_dev_pager_dtor(void *handle);
static int old_dev_pager_fault(vm_object_t object, vm_ooffset_t offset,
    int prot, vm_page_t *mres);
static void old_dev_pager_path(void *handle, char *path, size_t len);

static const struct cdev_pager_ops old_dev_pager_ops = {
	.cdev_pg_ctor =	old_dev_pager_ctor,
	.cdev_pg_dtor =	old_dev_pager_dtor,
	.cdev_pg_fault = old_dev_pager_fault,
	.cdev_pg_path = old_dev_pager_path
};

static void
dev_pager_init(void)
{

	TAILQ_INIT(&dev_pager_object_list);
	mtx_init(&dev_pager_mtx, "dev_pager list", NULL, MTX_DEF);
}

vm_object_t
cdev_pager_lookup(void *handle)
{
	vm_object_t object;

again:
	mtx_lock(&dev_pager_mtx);
	object = vm_pager_object_lookup(&dev_pager_object_list, handle);
	if (object != NULL && object->un_pager.devp.handle == NULL) {
		msleep(&object->un_pager.devp.handle, &dev_pager_mtx,
		    PVM | PDROP, "cdplkp", 0);
		vm_object_deallocate(object);
		goto again;
	}
	mtx_unlock(&dev_pager_mtx);
	return (object);
}

vm_object_t
cdev_pager_allocate(void *handle, enum obj_type tp,
    const struct cdev_pager_ops *ops, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred)
{
	vm_object_t object;
	vm_pindex_t pindex;

	KASSERT(handle != NULL, ("device pager with NULL handle"));

	if (tp != OBJT_DEVICE && tp != OBJT_MGTDEVICE)
		return (NULL);
	KASSERT(tp == OBJT_MGTDEVICE || ops->cdev_pg_populate == NULL,
	    ("populate on unmanaged device pager"));

	/*
	 * Offset should be page aligned.
	 */
	if (foff & PAGE_MASK)
		return (NULL);

	/*
	 * Treat the mmap(2) file offset as an unsigned value for a
	 * device mapping.  This, in effect, allows a user to pass all
	 * possible off_t values as the mapping cookie to the driver.  At
	 * this point, we know that both foff and size are a multiple
	 * of the page size.  Do a check to avoid wrap.
	 */
	size = round_page(size);
	pindex = OFF_TO_IDX(foff) + OFF_TO_IDX(size);
	if (pindex > OBJ_MAX_SIZE || pindex < OFF_TO_IDX(foff) ||
	    pindex < OFF_TO_IDX(size))
		return (NULL);

again:
	mtx_lock(&dev_pager_mtx);

	/*
	 * Look up pager, creating as necessary.
	 */
	object = vm_pager_object_lookup(&dev_pager_object_list, handle);
	if (object == NULL) {
		vm_object_t object1;

		/*
		 * Allocate object and associate it with the pager.  Initialize
		 * the object's pg_color based upon the physical address of the
		 * device's memory.
		 */
		mtx_unlock(&dev_pager_mtx);
		object1 = vm_object_allocate(tp, pindex);
		mtx_lock(&dev_pager_mtx);
		object = vm_pager_object_lookup(&dev_pager_object_list, handle);
		if (object != NULL) {
			object1->type = OBJT_DEAD;
			vm_object_deallocate(object1);
			object1 = NULL;
			if (object->un_pager.devp.handle == NULL) {
				msleep(&object->un_pager.devp.handle,
				    &dev_pager_mtx, PVM | PDROP, "cdplkp", 0);
				vm_object_deallocate(object);
				goto again;
			}

			/*
			 * We raced with other thread while allocating object.
			 */
			if (pindex > object->size)
				object->size = pindex;
			KASSERT(object->type == tp,
			    ("Inconsistent device pager type %p %d",
			    object, tp));
			KASSERT(object->un_pager.devp.ops == ops,
			    ("Inconsistent devops %p %p", object, ops));
		} else {
			u_short color;

			object = object1;
			object1 = NULL;
			object->handle = handle;
			object->un_pager.devp.ops = ops;
			TAILQ_INIT(&object->un_pager.devp.devp_pglist);
			TAILQ_INSERT_TAIL(&dev_pager_object_list, object,
			    pager_object_list);
			mtx_unlock(&dev_pager_mtx);
			if (ops->cdev_pg_populate != NULL)
				vm_object_set_flag(object, OBJ_POPULATE);
			if (ops->cdev_pg_ctor(handle, size, prot, foff,
			    cred, &color) != 0) {
				mtx_lock(&dev_pager_mtx);
				TAILQ_REMOVE(&dev_pager_object_list, object,
				    pager_object_list);
				wakeup(&object->un_pager.devp.handle);
				mtx_unlock(&dev_pager_mtx);
				object->type = OBJT_DEAD;
				vm_object_deallocate(object);
				object = NULL;
				mtx_lock(&dev_pager_mtx);
			} else {
				mtx_lock(&dev_pager_mtx);
				object->flags |= OBJ_COLORED;
				object->pg_color = color;
				object->un_pager.devp.handle = handle;
				wakeup(&object->un_pager.devp.handle);
			}
		}
		MPASS(object1 == NULL);
	} else {
		if (object->un_pager.devp.handle == NULL) {
			msleep(&object->un_pager.devp.handle,
			    &dev_pager_mtx, PVM | PDROP, "cdplkp", 0);
			vm_object_deallocate(object);
			goto again;
		}
		if (pindex > object->size)
			object->size = pindex;
		KASSERT(object->type == tp,
		    ("Inconsistent device pager type %p %d", object, tp));
	}
	mtx_unlock(&dev_pager_mtx);
	return (object);
}

static vm_object_t
dev_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred)
{

	return (cdev_pager_allocate(handle, OBJT_DEVICE, &old_dev_pager_ops,
	    size, prot, foff, cred));
}

void
cdev_pager_get_path(vm_object_t object, char *path, size_t sz)
{
	if (object->un_pager.devp.ops->cdev_pg_path != NULL)
		object->un_pager.devp.ops->cdev_pg_path(
		    object->un_pager.devp.handle, path, sz);
}

void
cdev_pager_free_page(vm_object_t object, vm_page_t m)
{

	if (object->type == OBJT_MGTDEVICE) {
		struct pctrie_iter pages;

		vm_page_iter_init(&pages, object);
		vm_radix_iter_lookup(&pages, m->pindex);
		cdev_mgtdev_pager_free_page(&pages, m);
	} else if (object->type == OBJT_DEVICE)
		dev_pager_free_page(object, m);
	else
		KASSERT(false,
		    ("Invalid device type obj %p m %p", object, m));
}

void
cdev_mgtdev_pager_free_page(struct pctrie_iter *pages, vm_page_t m)
{
	pmap_remove_all(m);
	vm_page_iter_remove(pages, m);
}

void
cdev_mgtdev_pager_free_pages(vm_object_t object)
{
	struct pctrie_iter pages;
	vm_page_t m;

	vm_page_iter_init(&pages, object);
	VM_OBJECT_WLOCK(object);
retry:
	KASSERT(pctrie_iter_is_reset(&pages),
	    ("%s: pctrie_iter not reset for retry", __func__));
	VM_RADIX_FOREACH(m, &pages) {
		if (!vm_page_busy_acquire(m, VM_ALLOC_WAITFAIL)) {
			pctrie_iter_reset(&pages);
			goto retry;
		}
		cdev_mgtdev_pager_free_page(&pages, m);
	}
	VM_OBJECT_WUNLOCK(object);
}

static void
dev_pager_free_page(vm_object_t object, vm_page_t m)
{

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT((object->type == OBJT_DEVICE &&
	    (m->oflags & VPO_UNMANAGED) != 0),
	    ("Managed device or page obj %p m %p", object, m));
	TAILQ_REMOVE(&object->un_pager.devp.devp_pglist, m, plinks.q);
	vm_page_putfake(m);
}

static void
dev_pager_dealloc(vm_object_t object)
{
	vm_page_t m;

	VM_OBJECT_WUNLOCK(object);
	object->un_pager.devp.ops->cdev_pg_dtor(object->un_pager.devp.handle);

	mtx_lock(&dev_pager_mtx);
	TAILQ_REMOVE(&dev_pager_object_list, object, pager_object_list);
	mtx_unlock(&dev_pager_mtx);
	VM_OBJECT_WLOCK(object);

	if (object->type == OBJT_DEVICE) {
		/*
		 * Free up our fake pages.
		 */
		while ((m = TAILQ_FIRST(&object->un_pager.devp.devp_pglist))
		    != NULL) {
			if (vm_page_busy_acquire(m, VM_ALLOC_WAITFAIL) == 0)
				continue;

			dev_pager_free_page(object, m);
		}
	}
	object->handle = NULL;
	object->type = OBJT_DEAD;
}

static int
dev_pager_getpages(vm_object_t object, vm_page_t *ma, int count, int *rbehind,
    int *rahead)
{
	int error;

	/* Since our haspage reports zero after/before, the count is 1. */
	KASSERT(count == 1, ("%s: count %d", __func__, count));
	if (object->un_pager.devp.ops->cdev_pg_fault == NULL)
		return (VM_PAGER_FAIL);
	VM_OBJECT_WLOCK(object);
	error = object->un_pager.devp.ops->cdev_pg_fault(object,
	    IDX_TO_OFF(ma[0]->pindex), PROT_READ, &ma[0]);

	VM_OBJECT_ASSERT_WLOCKED(object);

	if (error == VM_PAGER_OK) {
		KASSERT((object->type == OBJT_DEVICE &&
		     (ma[0]->oflags & VPO_UNMANAGED) != 0) ||
		    (object->type == OBJT_MGTDEVICE &&
		     (ma[0]->oflags & VPO_UNMANAGED) == 0),
		    ("Wrong page type %p %p", ma[0], object));
		if (object->type == OBJT_DEVICE) {
			TAILQ_INSERT_TAIL(&object->un_pager.devp.devp_pglist,
			    ma[0], plinks.q);
		}
		if (rbehind)
			*rbehind = 0;
		if (rahead)
			*rahead = 0;
	}
	VM_OBJECT_WUNLOCK(object);

	return (error);
}

static int
dev_pager_populate(vm_object_t object, vm_pindex_t pidx, int fault_type,
    vm_prot_t max_prot, vm_pindex_t *first, vm_pindex_t *last)
{

	VM_OBJECT_ASSERT_WLOCKED(object);
	if (object->un_pager.devp.ops->cdev_pg_populate == NULL)
		return (VM_PAGER_FAIL);
	return (object->un_pager.devp.ops->cdev_pg_populate(object, pidx,
	    fault_type, max_prot, first, last));
}

static int
old_dev_pager_fault(vm_object_t object, vm_ooffset_t offset, int prot,
    vm_page_t *mres)
{
	vm_paddr_t paddr;
	vm_page_t m_paddr, page;
	struct cdev *dev;
	struct cdevsw *csw;
	struct file *fpop;
	struct thread *td;
	vm_memattr_t memattr, memattr1;
	int ref, ret;

	memattr = object->memattr;

	VM_OBJECT_WUNLOCK(object);

	dev = object->handle;
	csw = dev_refthread(dev, &ref);
	if (csw == NULL) {
		VM_OBJECT_WLOCK(object);
		return (VM_PAGER_FAIL);
	}
	td = curthread;
	fpop = td->td_fpop;
	td->td_fpop = NULL;
	ret = csw->d_mmap(dev, offset, &paddr, prot, &memattr);
	td->td_fpop = fpop;
	dev_relthread(dev, ref);
	if (ret != 0) {
		printf(
	    "WARNING: dev_pager_getpage: map function returns error %d", ret);
		VM_OBJECT_WLOCK(object);
		return (VM_PAGER_FAIL);
	}

	/* If "paddr" is a real page, perform a sanity check on "memattr". */
	if ((m_paddr = vm_phys_paddr_to_vm_page(paddr)) != NULL &&
	    (memattr1 = pmap_page_get_memattr(m_paddr)) != memattr) {
		/*
		 * For the /dev/mem d_mmap routine to return the
		 * correct memattr, pmap_page_get_memattr() needs to
		 * be called, which we do there.
		 */
		if ((csw->d_flags & D_MEM) == 0) {
			printf("WARNING: Device driver %s has set "
			    "\"memattr\" inconsistently (drv %u pmap %u).\n",
			    csw->d_name, memattr, memattr1);
		}
		memattr = memattr1;
	}
	if (((*mres)->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in result page is a fake page, update it with
		 * the new physical address.
		 */
		page = *mres;
		VM_OBJECT_WLOCK(object);
		vm_page_updatefake(page, paddr, memattr);
	} else {
		/*
		 * Replace the passed in reqpage page with our own fake page and
		 * free up the all of the original pages.
		 */
		page = vm_page_getfake(paddr, memattr);
		VM_OBJECT_WLOCK(object);
		vm_page_replace(page, object, (*mres)->pindex, *mres);
		*mres = page;
	}
	vm_page_valid(page);
	return (VM_PAGER_OK);
}

static void
dev_pager_putpages(vm_object_t object, vm_page_t *m, int count, int flags,
    int *rtvals)
{

	panic("dev_pager_putpage called");
}

static boolean_t
dev_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before,
    int *after)
{

	if (before != NULL)
		*before = 0;
	if (after != NULL)
		*after = 0;
	return (TRUE);
}

static int
old_dev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct cdev *dev;
	struct cdevsw *csw;
	vm_memattr_t dummy;
	vm_ooffset_t off;
	vm_paddr_t paddr;
	unsigned int npages;
	int ref;

	/*
	 * Make sure this device can be mapped.
	 */
	dev = handle;
	csw = dev_refthread(dev, &ref);
	if (csw == NULL)
		return (ENXIO);

	/*
	 * Check that the specified range of the device allows the desired
	 * protection.
	 *
	 * XXX assumes VM_PROT_* == PROT_*
	 */
	npages = OFF_TO_IDX(size);
	paddr = 0; /* Make paddr initialized for the case of size == 0. */
	for (off = foff; npages--; off += PAGE_SIZE) {
		if (csw->d_mmap(dev, off, &paddr, (int)prot, &dummy) != 0) {
			dev_relthread(dev, ref);
			return (EINVAL);
		}
	}

	dev_ref(dev);
	dev_relthread(dev, ref);
	*color = atop(paddr) - OFF_TO_IDX(off - PAGE_SIZE);
	return (0);
}

static void
old_dev_pager_dtor(void *handle)
{

	dev_rel(handle);
}

static void
old_dev_pager_path(void *handle, char *path, size_t len)
{
	struct cdev *cdev = handle;

	if (cdev != NULL)
		dev_copyname(cdev, path, len);
}
