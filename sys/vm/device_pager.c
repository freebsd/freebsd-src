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
__FBSDID("$FreeBSD: src/sys/vm/device_pager.c,v 1.84.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

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
		vm_ooffset_t);
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

static vm_page_t dev_pager_getfake(vm_paddr_t);
static void dev_pager_putfake(vm_page_t);
static void dev_pager_updatefake(vm_page_t, vm_paddr_t);

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

/*
 * MPSAFE
 */
static vm_object_t
dev_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot, vm_ooffset_t foff)
{
	struct cdev *dev;
	vm_object_t object, object1;
	vm_pindex_t pindex;
	unsigned int npages;
	vm_paddr_t paddr;
	vm_offset_t off;
	struct cdevsw *csw;

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
	csw = dev_refthread(dev);
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
		if ((*csw->d_mmap)(dev, off, &paddr, (int)prot) != 0) {
			dev_relthread(dev);
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
		 * Allocate object and associate it with the pager.
		 */
		mtx_unlock(&dev_pager_mtx);
		object1 = vm_object_allocate(OBJT_DEVICE, pindex);
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
			TAILQ_INIT(&object->un_pager.devp.devp_pglist);
			TAILQ_INSERT_TAIL(&dev_pager_object_list, object,
			    pager_object_list);
		}
	} else {
		if (pindex > object->size)
			object->size = pindex;
	}
	mtx_unlock(&dev_pager_mtx);
	dev_relthread(dev);
	vm_object_deallocate(object1);
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
	while ((m = TAILQ_FIRST(&object->un_pager.devp.devp_pglist)) != 0) {
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
	vm_page_t page;
	struct cdev *dev;
	int i, ret;
	int prot;
	struct cdevsw *csw;
	struct thread *td;
	struct file *fpop;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	dev = object->handle;
	offset = m[reqpage]->pindex;
	VM_OBJECT_UNLOCK(object);
	csw = dev_refthread(dev);
	if (csw == NULL)
		panic("dev_pager_getpage: no cdevsw");
	prot = PROT_READ;	/* XXX should pass in? */

	td = curthread;
	fpop = td->td_fpop;
	td->td_fpop = NULL;
	ret = (*csw->d_mmap)(dev, (vm_offset_t)offset << PAGE_SHIFT, &paddr, prot);
	KASSERT(ret == 0, ("dev_pager_getpage: map function returns error"));
	td->td_fpop = fpop;
	dev_relthread(dev);

	if ((m[reqpage]->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in reqpage page is a fake page, update it with
		 * the new physical address.
		 */
		VM_OBJECT_LOCK(object);
		dev_pager_updatefake(m[reqpage], paddr);
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
		page = dev_pager_getfake(paddr);
		VM_OBJECT_LOCK(object);
		TAILQ_INSERT_TAIL(&object->un_pager.devp.devp_pglist, page, pageq);
		vm_page_lock_queues();
		for (i = 0; i < count; i++)
			vm_page_free(m[i]);
		vm_page_unlock_queues();
		vm_page_insert(page, object, offset);
		m[reqpage] = page;
	}

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
 * Instantiate a fictitious page.  Unlike physical memory pages, only
 * the machine-independent fields must be initialized.
 */
static vm_page_t
dev_pager_getfake(paddr)
	vm_paddr_t paddr;
{
	vm_page_t m;

	m = uma_zalloc(fakepg_zone, M_WAITOK);

	m->flags = PG_FICTITIOUS;
	m->oflags = VPO_BUSY;
	m->valid = VM_PAGE_BITS_ALL;
	m->dirty = 0;
	m->busy = 0;
	m->queue = PQ_NONE;
	m->object = NULL;

	m->wire_count = 1;
	m->hold_count = 0;
	m->phys_addr = paddr;

	return (m);
}

static void
dev_pager_putfake(m)
	vm_page_t m;
{
	if (!(m->flags & PG_FICTITIOUS))
		panic("dev_pager_putfake: bad page");
	uma_zfree(fakepg_zone, m);
}

static void
dev_pager_updatefake(m, paddr)
	vm_page_t m;
	vm_paddr_t paddr;
{
	if (!(m->flags & PG_FICTITIOUS))
		panic("dev_pager_updatefake: bad page");
	m->phys_addr = paddr;
	m->valid = VM_PAGE_BITS_ALL;
}
