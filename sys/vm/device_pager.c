/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)device_pager.c	7.2 (Berkeley) 4/20/91
 *	$Id: device_pager.c,v 1.4 1993/10/16 16:20:10 rgrimes Exp $
 */

/*
 * Page to/from special files.
 */

#include "devpager.h"
#if NDEVPAGER > 0

#include "param.h"
#include "conf.h"
#include "mman.h"
#include "malloc.h"

#include "vm.h"
#include "vm_page.h"
#include "vm_kern.h"
#include "device_pager.h"
#include "vnode.h"
#include "specdev.h"

queue_head_t	dev_pager_list;	/* list of managed devices */

#ifdef DEBUG
int	dpagerdebug = 0;
#define	DDB_FOLLOW	0x01
#define DDB_INIT	0x02
#define DDB_ALLOC	0x04
#define DDB_FAIL	0x08
#endif

void
dev_pager_init()
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_init()\n");
#endif
	queue_init(&dev_pager_list);
}

vm_pager_t
dev_pager_alloc(handle, size, prot)
	caddr_t handle;
	vm_size_t size;
	vm_prot_t prot;
{
	dev_t dev;
	vm_pager_t pager;
	int (*mapfunc)(), nprot;
	register vm_object_t object;
	register vm_page_t page;
	register dev_pager_t devp;
	register int npages, off;
	extern int nullop(), enodev();


#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_alloc(%x, %x, %x)\n", handle, size, prot);
#endif
	/*
	 * Pageout to device, should never happen.
	 */
	if (handle == NULL)
		panic("dev_pager_alloc called");

	/*
	 * Look it up, creating as necessary
	 */
	pager = vm_pager_lookup(&dev_pager_list, handle);
	if (pager == NULL) {
		/*
		 * Validation.  Make sure this device can be mapped
		 * and that range to map is acceptible to device.
		 */
		dev = ((struct vnode *) handle)->v_rdev;
		mapfunc = cdevsw[major(dev)].d_mmap;
		if (!mapfunc || mapfunc == enodev || mapfunc == nullop)
			return(NULL);
		nprot = 0;
		if (prot & VM_PROT_READ)
			nprot |= PROT_READ;
		if (prot & VM_PROT_WRITE)
			nprot |= PROT_WRITE;
		if (prot & VM_PROT_EXECUTE)
			nprot |= PROT_EXEC;
		npages = atop(round_page(size));
		for (off = 0; npages--; off += PAGE_SIZE)
			if ((*mapfunc)(dev, off, nprot) == -1)
				return(NULL);
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
		devp->devp_dev = dev;
		devp->devp_npages = atop(round_page(size));
		pager->pg_handle = handle;
		pager->pg_ops = &devicepagerops;
		pager->pg_type = PG_DEVICE;
		pager->pg_data = (caddr_t)devp;
		/*
		 * Allocate object and vm_page structures to describe memory
		 */
		npages = devp->devp_npages;
		object = devp->devp_object = vm_object_allocate(ptoa(npages));
		vm_object_enter(object, pager);
		vm_object_setpager(object, pager, (vm_offset_t)0, FALSE);
		devp->devp_pages = (vm_page_t)
			kmem_alloc(kernel_map, npages*sizeof(struct vm_page));
		off = 0;
		for (page = devp->devp_pages;
		     page < &devp->devp_pages[npages]; page++) {
			vm_object_lock(object);
			vm_page_init(page, object, off);
			page->phys_addr =
				pmap_phys_address((*mapfunc)(dev, off, nprot));
			page->wire_count = 1;
			page->fictitious = TRUE;
			PAGE_WAKEUP(page);
			vm_object_unlock(object);
			off += PAGE_SIZE;
		}
		/*
		 * Finally, put it on the managed list so other can find it.
		 */
		queue_enter(&dev_pager_list, pager, vm_pager_t, pg_list);
#ifdef DEBUG
		if (dpagerdebug & DDB_ALLOC)
			printf("dev_pager_alloc: pages %d@%x\n",
			       devp->devp_npages, devp->devp_pages);
#endif
	} else {
		/*
		 * vm_object_lookup() gains a reference and also
		 * removes the object from the cache.
		 */
		devp = (dev_pager_t)pager->pg_data;
		if (vm_object_lookup(pager) != devp->devp_object)
			panic("dev_pager_setup: bad object");
	}
#ifdef DEBUG
	if (dpagerdebug & DDB_ALLOC) {
		printf("dev_pager_alloc: pager %x devp %x object %x\n",
		       pager, devp, object);
		vm_object_print(object, FALSE);
	}
#endif
	return(pager);

}

void
dev_pager_dealloc(pager)
	vm_pager_t pager;
{
	dev_pager_t devp = (dev_pager_t)pager->pg_data;
	register vm_object_t object;

#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_dealloc(%x)\n", pager);
#endif
	queue_remove(&dev_pager_list, pager, vm_pager_t, pg_list);
	object = devp->devp_object;
#ifdef DEBUG
	if (dpagerdebug & DDB_ALLOC)
		printf("dev_pager_dealloc: devp %x object %x pages %d@%x\n",
		       devp, object, devp->devp_npages, devp->devp_pages);
#endif
	while (!queue_empty(&object->memq))
		vm_page_remove((vm_page_t)queue_first(&object->memq));
	kmem_free(kernel_map, devp->devp_pages,
		  devp->devp_npages * sizeof(struct vm_page));
	free((caddr_t)devp, M_VMPGDATA);
	free((caddr_t)pager, M_VMPAGER);
	pager->pg_data = 0;
}

dev_pager_getpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_getpage(%x, %x)\n", pager, m);
#endif
	return(VM_PAGER_BAD);
}

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
		return;
	panic("dev_pager_putpage called");
}

boolean_t
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
#endif
