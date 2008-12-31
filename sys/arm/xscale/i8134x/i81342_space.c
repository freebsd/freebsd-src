/*	$NetBSD: i80321_space.c,v 1.6 2003/10/06 15:43:35 thorpej Exp $	*/

/*-
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * bus_space functions for i81342 I/O Processor.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/i8134x/i81342_space.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/pcb.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>

#include <arm/xscale/i8134x/i81342reg.h>
#include <arm/xscale/i8134x/i81342var.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(i81342);
bs_protos(i81342_io);
bs_protos(i81342_mem);
bs_protos(generic);
bs_protos(generic_armv4);

/*
 * Template bus_space -- copied, and the bits that are NULL are
 * filled in.
 */
const struct bus_space i81342_bs_tag_template = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	NULL,
	NULL,
	i81342_bs_subregion,

	/* allocation/deallocation */
	NULL,
	NULL,

	/* barrier */
	i81342_bs_barrier,

	/* read (single) */
	generic_bs_r_1,
	generic_armv4_bs_r_2,
	generic_bs_r_4,
	NULL,

	/* read multiple */
	generic_bs_rm_1,
	generic_armv4_bs_rm_2,
	generic_bs_rm_4,
	NULL,

	/* read region */
	generic_bs_rr_1,
	generic_armv4_bs_rr_2,
	generic_bs_rr_4,
	NULL,

	/* write (single) */
	generic_bs_w_1,
	generic_armv4_bs_w_2,
	generic_bs_w_4,
	NULL,

	/* write multiple */
	generic_bs_wm_1,
	generic_armv4_bs_wm_2,
	generic_bs_wm_4,
	NULL,

	/* write region */
	NULL,
	generic_armv4_bs_wr_2,
	generic_bs_wr_4,
	NULL,

	/* set multiple */
	NULL,
	NULL,
	NULL,
	NULL,

	/* set region */
	NULL,
	generic_armv4_bs_sr_2,
	generic_bs_sr_4,
	NULL,

	/* copy */
	NULL,
	generic_armv4_bs_c_2,
	NULL,
	NULL,

	/* read (single) stream */
	generic_bs_r_1,
	generic_armv4_bs_r_2,
	generic_bs_r_4,
	NULL,

	/* read multiple stream */
	generic_bs_rm_1,
	generic_armv4_bs_rm_2,
	generic_bs_rm_4,
	NULL,

	/* read region stream */
	generic_bs_rr_1,
	generic_armv4_bs_rr_2,
	generic_bs_rr_4,
	NULL,

	/* write (single) stream */
	generic_bs_w_1,
	generic_armv4_bs_w_2,
	generic_bs_w_4,
	NULL,

	/* write multiple stream */
	generic_bs_wm_1,
	generic_armv4_bs_wm_2,
	generic_bs_wm_4,
	NULL,

	/* write region stream */
	NULL,
	generic_armv4_bs_wr_2,
	generic_bs_wr_4,
	NULL,
};

void
i81342_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i81342_bs_tag_template;
	bs->bs_cookie = cookie;
}

void
i81342_io_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i81342_bs_tag_template;
	bs->bs_cookie = cookie;

	bs->bs_map = i81342_io_bs_map;
	bs->bs_unmap = i81342_io_bs_unmap;
	bs->bs_alloc = i81342_io_bs_alloc;
	bs->bs_free = i81342_io_bs_free;

}

void
i81342_mem_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i81342_bs_tag_template;
	bs->bs_cookie = cookie;

	bs->bs_map = i81342_mem_bs_map;
	bs->bs_unmap = i81342_mem_bs_unmap;
	bs->bs_alloc = i81342_mem_bs_alloc;
	bs->bs_free = i81342_mem_bs_free;

}

/* *** Routines shared by i81342, PCI IO, and PCI MEM. *** */

int
i81342_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}

void
i81342_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{

	/* Nothing to do. */
}

/* *** Routines for PCI IO. *** */

int
i81342_io_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{

	*bshp = bpa;
	return (0);
}

void
i81342_io_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
{

	/* Nothing to do. */
}

int
i81342_io_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i81342_io_bs_alloc(): not implemented");
}

void    
i81342_io_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i81342_io_bs_free(): not implemented");
}


/* *** Routines for PCI MEM. *** */
extern int badaddr_read(void *, int, void *);
static vm_offset_t allocable = 0xe1000000;
int
i81342_mem_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	struct i81342_pci_softc *sc = (struct i81342_pci_softc *)t;
	struct i81342_pci_map *tmp;
	vm_offset_t addr, endaddr;
	vm_paddr_t paddr;

	/* Lookup to see if we already have a mapping at this address. */
	tmp = sc->sc_pci_mappings;
	while (tmp) {
		if (tmp->paddr <= bpa && tmp->paddr + tmp->size >
		    bpa + size) {
			*bshp = bpa - tmp->paddr + tmp->vaddr;
			return (0);
		}
		tmp = tmp->next;
	}
	addr = allocable;
	endaddr = ((addr + size) &~ (0x1000000 - 1)) + 0x1000000;
	if (endaddr >= IOP34X_VADDR)
		panic("PCI virtual memory exhausted");
	allocable = endaddr;
	tmp = malloc(sizeof(*tmp), M_DEVBUF, M_WAITOK);
	tmp->next = NULL;
	paddr = bpa &~ (0x100000 - 1);
	tmp->paddr = paddr;
	tmp->vaddr = addr;
	tmp->size = 0;
	while (addr < endaddr) {
		pmap_kenter_supersection(addr, paddr + (sc->sc_is_atux ?
		    IOP34X_PCIX_OMBAR : IOP34X_PCIE_OMBAR), 0);
		addr += 0x1000000;
		paddr += 0x1000000;
		tmp->size += 0x1000000;
	}
	tmp->next = sc->sc_pci_mappings;
	sc->sc_pci_mappings = tmp;
	*bshp = bpa - tmp->paddr + tmp->vaddr;
	return (0);
}

void
i81342_mem_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
{
#if 0
	vm_offset_t va, endva;

	va = trunc_page((vm_offset_t)t);
	endva = va + round_page(size);

	/* Free the kernel virtual mapping. */
	kmem_free(kernel_map, va, endva - va);
#endif
}

int
i81342_mem_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i81342_mem_bs_alloc(): not implemented");
}

void    
i81342_mem_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i81342_mem_bs_free(): not implemented");
}
