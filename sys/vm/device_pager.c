/*
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
 *	@(#)device_pager.c	8.1 (Berkeley) 6/11/93
 * $Id: device_pager.c,v 1.15 1995/12/03 18:59:55 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/device_pager.h>

struct pagerlst dev_pager_object_list;	/* list of device pager objects */
TAILQ_HEAD(, vm_page) dev_pager_fakelist;	/* list of available vm_page_t's */

static vm_page_t dev_pager_getfake __P((vm_offset_t));
static void dev_pager_putfake __P((vm_page_t));

static int dev_pager_alloc_lock, dev_pager_alloc_lock_want;

struct pagerops devicepagerops = {
	dev_pager_init,
	dev_pager_alloc,
	dev_pager_dealloc,
	dev_pager_getpages,
	dev_pager_putpages,
	dev_pager_haspage,
	NULL
};

void
dev_pager_init()
{
	TAILQ_INIT(&dev_pager_object_list);
	TAILQ_INIT(&dev_pager_fakelist);
}

vm_object_t
dev_pager_alloc(handle, size, prot, foff)
	void *handle;
	vm_size_t size;
	vm_prot_t prot;
	vm_offset_t foff;
{
	dev_t dev;
	d_mmap_t *mapfunc;
	vm_object_t object;
	unsigned int npages, off;

	/*
	 * Make sure this device can be mapped.
	 */
	dev = (dev_t) (u_long) handle;
	mapfunc = cdevsw[major(dev)].d_mmap;
	if (mapfunc == NULL || mapfunc == (d_mmap_t *)nullop) {
		printf("obsolete map function %p\n", (void *)mapfunc);
		return (NULL);
	}

	/*
	 * Offset should be page aligned.
	 */
	if (foff & (PAGE_SIZE - 1))
		return (NULL);

	/*
	 * Check that the specified range of the device allows the desired
	 * protection.
	 *
	 * XXX assumes VM_PROT_* == PROT_*
	 */
	npages = atop(round_page(size));
	for (off = foff; npages--; off += PAGE_SIZE)
		if ((*mapfunc) (dev, off, (int) prot) == -1)
			return (NULL);

	/*
	 * Lock to prevent object creation race contion.
	 */
	while (dev_pager_alloc_lock) {
		dev_pager_alloc_lock_want++;
		tsleep(&dev_pager_alloc_lock, PVM, "dvpall", 0);
		dev_pager_alloc_lock_want--;
	}
	dev_pager_alloc_lock = 1;

	/*
	 * Look up pager, creating as necessary.
	 */
	object = vm_pager_object_lookup(&dev_pager_object_list, handle);
	if (object == NULL) {
		/*
		 * Allocate object and associate it with the pager.
		 */
		object = vm_object_allocate(OBJT_DEVICE, foff + size);
		object->handle = handle;
		TAILQ_INIT(&object->un_pager.devp.devp_pglist);
		TAILQ_INSERT_TAIL(&dev_pager_object_list, object, pager_object_list);
	} else {
		/*
		 * Gain a reference to the object.
		 */
		vm_object_reference(object);
		if (foff + size > object->size)
			object->size = foff + size;
	}

	dev_pager_alloc_lock = 0;
	if (dev_pager_alloc_lock_want)
		wakeup(&dev_pager_alloc_lock);

	return (object);
}

void
dev_pager_dealloc(object)
	vm_object_t object;
{
	vm_page_t m;

	TAILQ_REMOVE(&dev_pager_object_list, object, pager_object_list);
	/*
	 * Free up our fake pages.
	 */
	while ((m = object->un_pager.devp.devp_pglist.tqh_first) != 0) {
		TAILQ_REMOVE(&object->un_pager.devp.devp_pglist, m, pageq);
		dev_pager_putfake(m);
	}
}

int
dev_pager_getpages(object, m, count, reqpage)
	vm_object_t object;
	vm_page_t *m;
	int count;
	int reqpage;
{
	vm_offset_t offset, paddr;
	vm_page_t page;
	dev_t dev;
	int i, s;
	d_mmap_t *mapfunc;
	int prot;

	dev = (dev_t) (u_long) object->handle;
	offset = m[reqpage]->offset + object->paging_offset;
	prot = PROT_READ;	/* XXX should pass in? */
	mapfunc = cdevsw[major(dev)].d_mmap;

	if (mapfunc == NULL || mapfunc == (d_mmap_t *)nullop)
		panic("dev_pager_getpage: no map function");

	paddr = pmap_phys_address((*mapfunc) ((dev_t) dev, (int) offset, prot));
#ifdef DIAGNOSTIC
	if (paddr == -1)
		panic("dev_pager_getpage: map function returns error");
#endif
	/*
	 * Replace the passed in reqpage page with our own fake page and free up the
	 * all of the original pages.
	 */
	page = dev_pager_getfake(paddr);
	TAILQ_INSERT_TAIL(&object->un_pager.devp.devp_pglist, page, pageq);
	for (i = 0; i < count; i++) {
		PAGE_WAKEUP(m[i]);
		vm_page_free(m[i]);
	}
	s = splhigh();
	vm_page_insert(page, object, offset);
	splx(s);

	return (VM_PAGER_OK);
}

int
dev_pager_putpages(object, m, count, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{
	panic("dev_pager_putpage called");
}

boolean_t
dev_pager_haspage(object, offset, before, after)
	vm_object_t object;
	vm_offset_t offset;
	int *before;
	int *after;
{
	if (before != NULL)
		*before = 0;
	if (after != NULL)
		*after = 0;
	return (TRUE);
}

static vm_page_t
dev_pager_getfake(paddr)
	vm_offset_t paddr;
{
	vm_page_t m;
	int i;

	if (dev_pager_fakelist.tqh_first == NULL) {
		m = (vm_page_t) malloc(PAGE_SIZE * 2, M_VMPGDATA, M_WAITOK);
		for (i = (PAGE_SIZE * 2) / sizeof(*m); i > 0; i--) {
			TAILQ_INSERT_TAIL(&dev_pager_fakelist, m, pageq);
			m++;
		}
	}
	m = dev_pager_fakelist.tqh_first;
	TAILQ_REMOVE(&dev_pager_fakelist, m, pageq);

	m->flags = PG_BUSY | PG_FICTITIOUS;
	m->valid = VM_PAGE_BITS_ALL;
	m->dirty = 0;
	m->busy = 0;
	m->bmapped = 0;

	m->wire_count = 1;
	m->phys_addr = paddr;

	return (m);
}

static void
dev_pager_putfake(m)
	vm_page_t m;
{
	if (!(m->flags & PG_FICTITIOUS))
		panic("dev_pager_putfake: bad page");
	TAILQ_INSERT_TAIL(&dev_pager_fakelist, m, pageq);
}
