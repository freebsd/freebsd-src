/*-
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
 *	@(#)device_pager.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

static void dev_pager_init(void);
static vm_object_t dev_pager_alloc(void *, vm_ooffset_t, vm_prot_t,
    vm_ooffset_t, struct ucred *);
static void dev_pager_dealloc(vm_object_t);
static int dev_pager_getpages(vm_object_t, vm_page_t *, int, int);
static void dev_pager_putpages(vm_object_t, vm_page_t *, int, 
		boolean_t, int *);
static boolean_t dev_pager_haspage(vm_object_t, vm_pindex_t, int *,
		int *);

/* list of device pager objects */
static struct pagerlst dev_pager_object_list;
/* protect list manipulation */
static struct mtx dev_pager_mtx;


static uma_zone_t fakepg_zone;

static vm_page_t dev_pager_getfake(vm_paddr_t, vm_memattr_t);
static void dev_pager_putfake(vm_page_t);
static void dev_pager_updatefake(vm_page_t, vm_paddr_t, vm_memattr_t);

struct pagerops devicepagerops = {
	.pgo_init =	dev_pager_init,
	.pgo_alloc =	dev_pager_alloc,
	.pgo_dealloc =	dev_pager_dealloc,
	.pgo_getpages =	dev_pager_getpages,
	.pgo_putpages =	dev_pager_putpages,
	.pgo_haspage =	dev_pager_haspage,
};

static void
dev_pager_init()
{
	TAILQ_INIT(&dev_pager_object_list);
	mtx_init(&dev_pager_mtx, "dev_pager list", NULL, MTX_DEF);
	fakepg_zone = uma_zcreate("DP fakepg", sizeof(struct vm_page),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
	    UMA_ZONE_NOFREE|UMA_ZONE_VM); 
}

static __inline int
dev_mmap(struct cdevsw *csw, struct cdev *dev, vm_offset_t offset,
    vm_paddr_t *paddr, int nprot, vm_memattr_t *memattr)
{

	if (csw->d_flags & D_MMAP2)
		return (csw->d_mmap2(dev, offset, paddr, nprot, memattr));
	else
		return (csw->d_mmap(dev, offset, paddr, nprot));
}

/*
 * MPSAFE
 */
static vm_object_t
dev_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred)
{
	struct cdev *dev;
	vm_object_t object, object1;
	vm_pindex_t pindex;
	unsigned int npages;
	vm_paddr_t paddr;
	vm_offset_t off;
	vm_memattr_t dummy;
	struct cdevsw *csw;
	int ref;

	/*
	 * Offset should be page aligned.
	 */
	if (foff & PAGE_MASK)
		return (NULL);

	size = round_page(size);
	pindex = OFF_TO_IDX(foff + size);

	/*
	 * Make sure this device can be mapped.
	 */
	dev = handle;
	csw = dev_refthread(dev, &ref);
	if (csw == NULL)
		return (NULL);

	/*
	 * Check that the specified range of the device allows the desired
	 * protection.
	 *
	 * XXX assumes VM_PROT_* == PROT_*
	 */
	npages = OFF_TO_IDX(size);
	for (off = foff; npages--; off += PAGE_SIZE)
		if (dev_mmap(csw, dev, off, &paddr, (int)prot, &dummy) != 0) {
			dev_relthread(dev, ref);
			return (NULL);
		}

	mtx_lock(&dev_pager_mtx);

	/*
	 * Look up pager, creating as necessary.
	 */
	object1 = NULL;
	object = vm_pager_object_lookup(&dev_pager_object_list, handle);
	if (object == NULL) {
		/*
		 * Allocate object and associate it with the pager.  Initialize
		 * the object's pg_color based upon the physical address of the
		 * device's memory.
		 */
		mtx_unlock(&dev_pager_mtx);
		object1 = vm_object_allocate(OBJT_DEVICE, pindex);
		object1->flags |= OBJ_COLORED;
		object1->pg_color = atop(paddr) - OFF_TO_IDX(off - PAGE_SIZE);
		TAILQ_INIT(&object1->un_pager.devp.devp_pglist);
		mtx_lock(&dev_pager_mtx);
		object = vm_pager_object_lookup(&dev_pager_object_list, handle);
		if (object != NULL) {
			/*
			 * We raced with other thread while allocating object.
			 */
			if (pindex > object->size)
				object->size = pindex;
		} else {
			object = object1;
			object1 = NULL;
			object->handle = handle;
			TAILQ_INSERT_TAIL(&dev_pager_object_list, object,
			    pager_object_list);
		}
	} else {
		if (pindex > object->size)
			object->size = pindex;
	}
	mtx_unlock(&dev_pager_mtx);
	dev_relthread(dev, ref);
	if (object1 != NULL) {
		object1->handle = object1;
		mtx_lock(&dev_pager_mtx);
		TAILQ_INSERT_TAIL(&dev_pager_object_list, object1,
		    pager_object_list);
		mtx_unlock(&dev_pager_mtx);
		vm_object_deallocate(object1);
	}
	return (object);
}

static void
dev_pager_dealloc(object)
	vm_object_t object;
{
	vm_page_t m;

	VM_OBJECT_UNLOCK(object);
	mtx_lock(&dev_pager_mtx);
	TAILQ_REMOVE(&dev_pager_object_list, object, pager_object_list);
	mtx_unlock(&dev_pager_mtx);
	VM_OBJECT_LOCK(object);
	/*
	 * Free up our fake pages.
	 */
	while ((m = TAILQ_FIRST(&object->un_pager.devp.devp_pglist)) != NULL) {
		TAILQ_REMOVE(&object->un_pager.devp.devp_pglist, m, pageq);
		dev_pager_putfake(m);
	}
}

static int
dev_pager_getpages(object, m, count, reqpage)
	vm_object_t object;
	vm_page_t *m;
	int count;
	int reqpage;
{
	vm_pindex_t offset;
	vm_paddr_t paddr;
	vm_page_t m_paddr, page;
	vm_memattr_t memattr;
	struct cdev *dev;
	int i, ref, ret;
	struct cdevsw *csw;
	struct thread *td;
	struct file *fpop;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	dev = object->handle;
	page = m[reqpage];
	offset = page->pindex;
	memattr = object->memattr;
	VM_OBJECT_UNLOCK(object);
	csw = dev_refthread(dev, &ref);
	if (csw == NULL)
		panic("dev_pager_getpage: no cdevsw");
	td = curthread;
	fpop = td->td_fpop;
	td->td_fpop = NULL;
	ret = dev_mmap(csw, dev, (vm_offset_t)offset << PAGE_SHIFT, &paddr,
	    PROT_READ, &memattr);
	KASSERT(ret == 0, ("dev_pager_getpage: map function returns error"));
	td->td_fpop = fpop;
	dev_relthread(dev, ref);
	/* If "paddr" is a real page, perform a sanity check on "memattr". */
	if ((m_paddr = vm_phys_paddr_to_vm_page(paddr)) != NULL &&
	    pmap_page_get_memattr(m_paddr) != memattr) {
		memattr = pmap_page_get_memattr(m_paddr);
		printf(
	    "WARNING: A device driver has set \"memattr\" inconsistently.\n");
	}
	if ((page->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in reqpage page is a fake page, update it with
		 * the new physical address.
		 */
		VM_OBJECT_LOCK(object);
		dev_pager_updatefake(page, paddr, memattr);
		if (count > 1) {
			vm_page_lock_queues();
			for (i = 0; i < count; i++) {
				if (i != reqpage)
					vm_page_free(m[i]);
			}
			vm_page_unlock_queues();
		}
	} else {
		/*
		 * Replace the passed in reqpage page with our own fake page and
		 * free up the all of the original pages.
		 */
		page = dev_pager_getfake(paddr, memattr);
		VM_OBJECT_LOCK(object);
		TAILQ_INSERT_TAIL(&object->un_pager.devp.devp_pglist, page, pageq);
		vm_page_lock_queues();
		for (i = 0; i < count; i++)
			vm_page_free(m[i]);
		vm_page_unlock_queues();
		vm_page_insert(page, object, offset);
		m[reqpage] = page;
	}
	page->valid = VM_PAGE_BITS_ALL;
	return (VM_PAGER_OK);
}

static void
dev_pager_putpages(object, m, count, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{
	panic("dev_pager_putpage called");
}

static boolean_t
dev_pager_haspage(object, pindex, before, after)
	vm_object_t object;
	vm_pindex_t pindex;
	int *before;
	int *after;
{
	if (before != NULL)
		*before = 0;
	if (after != NULL)
		*after = 0;
	return (TRUE);
}

/*
 * Create a fictitious page with the specified physical address and memory
 * attribute.  The memory attribute is the only the machine-dependent aspect
 * of a fictitious page that must be initialized.
 */
static vm_page_t
dev_pager_getfake(vm_paddr_t paddr, vm_memattr_t memattr)
{
	vm_page_t m;

	m = uma_zalloc(fakepg_zone, M_WAITOK | M_ZERO);
	m->phys_addr = paddr;
	/* Fictitious pages don't use "segind". */
	m->flags = PG_FICTITIOUS;
	/* Fictitious pages don't use "order" or "pool". */
	m->oflags = VPO_BUSY;
	m->wire_count = 1;
	pmap_page_set_memattr(m, memattr);
	return (m);
}

/*
 * Release a fictitious page.
 */
static void
dev_pager_putfake(vm_page_t m)
{

	if (!(m->flags & PG_FICTITIOUS))
		panic("dev_pager_putfake: bad page");
	uma_zfree(fakepg_zone, m);
}

/*
 * Update the given fictitious page to the specified physical address and
 * memory attribute.
 */
static void
dev_pager_updatefake(vm_page_t m, vm_paddr_t paddr, vm_memattr_t memattr)
{

	if (!(m->flags & PG_FICTITIOUS))
		panic("dev_pager_updatefake: bad page");
	m->phys_addr = paddr;
	pmap_page_set_memattr(m, memattr);
}
