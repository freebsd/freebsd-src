/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include <sys/cdefs.h>
#include "opt_platform.h"
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/xdma/xdma.h>
#include "xdma_if.h"

void
xdma_iommu_remove_entry(xdma_channel_t *xchan, vm_offset_t va)
{
	struct xdma_iommu *xio;

	xio = &xchan->xio;

	va &= ~(PAGE_SIZE - 1);
	pmap_remove(&xio->p, va, va + PAGE_SIZE);

	XDMA_IOMMU_REMOVE(xio->dev, xio, va);

	vmem_free(xio->vmem, va, PAGE_SIZE);
}

static void
xdma_iommu_enter(struct xdma_iommu *xio, vm_offset_t va,
    vm_paddr_t pa, vm_size_t size, vm_prot_t prot)
{
	vm_page_t m;
	pmap_t p;

	p = &xio->p;

	KASSERT((size & PAGE_MASK) == 0,
	    ("%s: device mapping not page-sized", __func__));

	for (; size > 0; size -= PAGE_SIZE) {
		m = PHYS_TO_VM_PAGE(pa);
		pmap_enter(p, va, m, prot, prot | PMAP_ENTER_WIRED, 0);

		XDMA_IOMMU_ENTER(xio->dev, xio, va, pa);

		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
}

void
xdma_iommu_add_entry(xdma_channel_t *xchan, vm_offset_t *va,
    vm_paddr_t pa, vm_size_t size, vm_prot_t prot)
{
	struct xdma_iommu *xio;
	vm_offset_t addr;

	size = roundup2(size, PAGE_SIZE);
	xio = &xchan->xio;

	if (vmem_alloc(xio->vmem, size,
	    M_FIRSTFIT | M_NOWAIT, &addr)) {
		panic("Could not allocate virtual address.\n");
	}

	addr |= pa & (PAGE_SIZE - 1);

	if (va)
		*va = addr;

	xdma_iommu_enter(xio, addr, pa, size, prot);
}

int
xdma_iommu_init(struct xdma_iommu *xio)
{
#ifdef FDT
	phandle_t mem_node, node;
	pcell_t mem_handle;
#endif

	pmap_pinit(&xio->p);

#ifdef FDT
	node = ofw_bus_get_node(xio->dev);
	if (!OF_hasprop(node, "va-region"))
		return (ENXIO);

	if (OF_getencprop(node, "va-region", (void *)&mem_handle,
	    sizeof(mem_handle)) <= 0)
		return (ENXIO);
#endif

	xio->vmem = vmem_create("xDMA vmem", 0, 0, PAGE_SIZE,
	    PAGE_SIZE, M_FIRSTFIT | M_WAITOK);
	if (xio->vmem == NULL)
		return (ENXIO);

#ifdef FDT
	mem_node = OF_node_from_xref(mem_handle);
	if (xdma_handle_mem_node(xio->vmem, mem_node) != 0) {
		vmem_destroy(xio->vmem);
		return (ENXIO);
	}
#endif

	XDMA_IOMMU_INIT(xio->dev, xio);

	return (0);
}

int
xdma_iommu_release(struct xdma_iommu *xio)
{

	pmap_release(&xio->p);

	vmem_destroy(xio->vmem);

	XDMA_IOMMU_RELEASE(xio->dev, xio);

	return (0);
}
