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
 */

/*
 * Page to/from special files.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/device_pager.h>

#include "vnode.h"
#include "specdev.h"

queue_head_t	dev_pager_list;		/* list of managed devices */
queue_head_t	dev_pager_fakelist;	/* list of available vm_page_t's */

#ifdef DEBUG
int	dpagerdebug = 0;
#define	DDB_FOLLOW	0x01
#define DDB_INIT	0x02
#define DDB_ALLOC	0x04
#define DDB_FAIL	0x08
#endif

static vm_pager_t	 dev_pager_alloc
			    __P((caddr_t, vm_size_t, vm_prot_t, vm_offset_t));
static void		 dev_pager_dealloc __P((vm_pager_t));
static int		 dev_pager_getpage
			    __P((vm_pager_t, vm_page_t, boolean_t));
static boolean_t	 dev_pager_haspage __P((vm_pager_t, vm_offset_t));
static void		 dev_pager_init __P((void));
static int		 dev_pager_putpage
			    __P((vm_pager_t, vm_page_t, boolean_t));
static vm_page_t	 dev_pager_getfake __P((vm_offset_t));
static void		 dev_pager_putfake __P((vm_page_t));

struct pagerops devicepagerops = {
	dev_pager_init,
	dev_pager_alloc,
	dev_pager_dealloc,
	dev_pager_getpage,
	0,
	dev_pager_putpage,
	dev_pager_haspage
};

static void
dev_pager_init()
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_init()\n");
#endif
	queue_init(&dev_pager_list);
	queue_init(&dev_pager_fakelist);
}

static vm_pager_t
dev_pager_alloc(handle, size, prot, foff)
	caddr_t handle;
	vm_size_t size;
	vm_prot_t prot;
	vm_offset_t foff;
{
	dev_t dev;
	vm_pager_t pager;
	int (*mapfunc)();
	vm_object_t object;
	dev_pager_t devp;
	unsigned int npages, off;

#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_alloc(%x, %x, %x, %x)\n",
		       handle, size, prot, foff);
#endif
#ifdef DIAGNOSTIC
	/*
	 * Pageout to device, should never happen.
	 */
	if (handle == NULL)
		panic("dev_pager_alloc called");
#endif

	/*
	 * Make sure this device can be mapped.
	 */
	dev = (dev_t)(u_long)handle;
	mapfunc = cdevsw[major(dev)].d_mmap;
	if (mapfunc == NULL || mapfunc == enodev || mapfunc == nullop)
		return(NULL);

	/*
	 * Offset should be page aligned.
	 */
	if (foff & (PAGE_SIZE-1))
		return(NULL);

	/*
	 * Check that the specified range of the device allows the
	 * desired protection.
	 *
	 * XXX assumes VM_PROT_* == PROT_*
	 */
	npages = atop(round_page(size));
	for (off = foff; npages--; off += PAGE_SIZE)
		if ((*mapfunc)(dev, off, (int)prot) == -1)
			return(NULL);

	/*
	 * Look up pager, creating as necessary.
	 */
top:
	pager = vm_pager_lookup(&dev_pager_list, handle);
	if (pager == NULL) {
		/*
		 * Allocate and initialize pager structs
		 */
		pager = (vm_pager_t)malloc(sizeof *pager, M_VMPAGER, M_WAITOK);
		if (pager == NULL)
			return(NULL);
		devp = (dev_pager_t)malloc(sizeof *devp, M_VMPGDATA, M_WAITOK);
		if (devp == NULL) {
			free((caddr_t)pager, M_VMPAGER);
			return(NULL);
		}
		pager->pg_handle = handle;
		pager->pg_ops = &devicepagerops;
		pager->pg_type = PG_DEVICE;
		pager->pg_data = (caddr_t)devp;
		queue_init(&devp->devp_pglist);
		/*
		 * Allocate object and associate it with the pager.
		 */
		object = devp->devp_object = vm_object_allocate(0);
		vm_object_enter(object, pager);
		vm_object_setpager(object, pager, (vm_offset_t)foff, FALSE);
		/*
		 * Finally, put it on the managed list so other can find it.
		 * First we re-lookup in case someone else beat us to this
		 * point (due to blocking in the various mallocs).  If so,
		 * we free everything and start over.
		 */
		if (vm_pager_lookup(&dev_pager_list, handle)) {
			free((caddr_t)devp, M_VMPGDATA);
			free((caddr_t)pager, M_VMPAGER);
			goto top;
		}
		queue_enter(&dev_pager_list, pager, vm_pager_t, pg_list);
#ifdef DEBUG
		if (dpagerdebug & DDB_ALLOC) {
			printf("dev_pager_alloc: pager %x devp %x object %x\n",
			       pager, devp, object);
			vm_object_print(object, FALSE);
		}
#endif
	} else {
		/*
		 * vm_object_lookup() gains a reference and also
		 * removes the object from the cache.
		 */
		object = vm_object_lookup(pager);
#ifdef DIAGNOSTIC
		devp = (dev_pager_t)pager->pg_data;
		if (object != devp->devp_object)
			panic("dev_pager_setup: bad object");
#endif
	}
	return(pager);
}

static void
dev_pager_dealloc(pager)
	vm_pager_t pager;
{
	dev_pager_t devp;
	vm_object_t object;
	vm_page_t m;

#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_dealloc(%x)\n", pager);
#endif
	queue_remove(&dev_pager_list, pager, vm_pager_t, pg_list);
	/*
	 * Get the object.
	 * Note: cannot use vm_object_lookup since object has already
	 * been removed from the hash chain.
	 */
	devp = (dev_pager_t)pager->pg_data;
	object = devp->devp_object;
#ifdef DEBUG
	if (dpagerdebug & DDB_ALLOC)
		printf("dev_pager_dealloc: devp %x object %x\n", devp, object);
#endif
	/*
	 * Free up our fake pages.
	 */
	while (!queue_empty(&devp->devp_pglist)) {
		queue_remove_first(&devp->devp_pglist, m, vm_page_t, pageq);
		dev_pager_putfake(m);
	}
	free((caddr_t)devp, M_VMPGDATA);
	free((caddr_t)pager, M_VMPAGER);
}

static int
dev_pager_getpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
	register vm_object_t object;
	vm_offset_t offset, paddr;
	vm_page_t page;
	dev_t dev;
	int s;
	int (*mapfunc)(), prot;

#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_getpage(%x, %x)\n", pager, m);
#endif

	object = m->object;
	dev = (dev_t)(u_long)pager->pg_handle;
	offset = m->offset + object->paging_offset;
	prot = PROT_READ;	/* XXX should pass in? */
	mapfunc = cdevsw[major(dev)].d_mmap;

	if (mapfunc == NULL || mapfunc == enodev || mapfunc == nullop)
		panic("dev_pager_getpage: no map function");

	paddr = pmap_phys_address((*mapfunc)((dev_t)dev, (int)offset, prot));
#ifdef DIAGNOSTIC
	if (paddr == -1)
		panic("dev_pager_getpage: map function returns error");
#endif
	/*
	 * Replace the passed in page with our own fake page and free
	 * up the original.
	 */
	page = dev_pager_getfake(paddr);
	queue_enter(&((dev_pager_t)pager->pg_data)->devp_pglist,
		    page, vm_page_t, pageq);
	vm_object_lock(object);
	vm_page_lock_queues();
	vm_page_free(m);
	vm_page_unlock_queues();
	s = splhigh();
	vm_page_insert(page, object, offset);
	splx(s);
	PAGE_WAKEUP(m);
	if (offset + PAGE_SIZE > object->size)
		object->size = offset + PAGE_SIZE;	/* XXX anal */
	vm_object_unlock(object);

	return(VM_PAGER_OK);
}

static int
dev_pager_putpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_putpage(%x, %x)\n", pager, m);
#endif
	if (pager == NULL)
		return 0;
	panic("dev_pager_putpage called");
}

static boolean_t
dev_pager_haspage(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_haspage(%x, %x)\n", pager, offset);
#endif
	return(TRUE);
}

static vm_page_t
dev_pager_getfake(paddr)
	vm_offset_t paddr;
{
	vm_page_t m;
	int i;

	if (queue_empty(&dev_pager_fakelist)) {
		m = (vm_page_t)malloc(PAGE_SIZE, M_VMPGDATA, M_WAITOK);
		for (i = PAGE_SIZE / sizeof(*m); i > 0; i--) {
			queue_enter(&dev_pager_fakelist, m, vm_page_t, pageq);
			m++;
		}
	}
	queue_remove_first(&dev_pager_fakelist, m, vm_page_t, pageq);

	m->flags = PG_BUSY | PG_CLEAN | PG_FAKE | PG_FICTITIOUS;

	m->wire_count = 1;
	m->phys_addr = paddr;

	return(m);
}

static void
dev_pager_putfake(m)
	vm_page_t m;
{
#ifdef DIAGNOSTIC
	if (!(m->flags & PG_FICTITIOUS))
		panic("dev_pager_putfake: bad page");
#endif
	queue_enter(&dev_pager_fakelist, m, vm_page_t, pageq);
}
