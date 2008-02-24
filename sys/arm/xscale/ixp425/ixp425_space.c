/*	$NetBSD: ixp425_space.c,v 1.6 2006/04/10 03:36:03 simonb Exp $ */

/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
 * All rights reserved.
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
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/ixp425/ixp425_space.c,v 1.1 2006/11/19 23:55:23 sam Exp $");

/*
 * bus_space I/O functions for ixp425
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/pcb.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

/* Proto types for all the bus_space structure functions */
bs_protos(ixp425);
bs_protos(generic);
bs_protos(generic_armv4);

struct bus_space ixp425_bs_tag = {
	/* cookie */
	.bs_cookie	= (void *) 0,

	/* mapping/unmapping */
	.bs_map		= ixp425_bs_map,
	.bs_unmap	= ixp425_bs_unmap,
	.bs_subregion	= ixp425_bs_subregion,

	/* allocation/deallocation */
	.bs_alloc	= ixp425_bs_alloc,
	.bs_free	= ixp425_bs_free,

	/* barrier */
	.bs_barrier	= ixp425_bs_barrier,

	/* read (single) */
	.bs_r_1		= generic_bs_r_1,
	.bs_r_2		= generic_armv4_bs_r_2,
	.bs_r_4		= generic_bs_r_4,
	.bs_r_8		= NULL,

	/* read multiple */
	.bs_rm_1	= generic_bs_rm_1,
	.bs_rm_2	= generic_armv4_bs_rm_2,
	.bs_rm_4	= generic_bs_rm_4,
	.bs_rm_8	= NULL,

	/* read region */
	.bs_rr_1	= generic_bs_rr_1,
	.bs_rr_2	= generic_armv4_bs_rr_2,
	.bs_rr_4	= generic_bs_rr_4,
	.bs_rr_8	= NULL,

	/* write (single) */
	.bs_w_1		= generic_bs_w_1,
	.bs_w_2		= generic_armv4_bs_w_2,
	.bs_w_4		= generic_bs_w_4,
	.bs_w_8		= NULL,

	/* write multiple */
	.bs_wm_1	= generic_bs_wm_1,
	.bs_wm_2	= generic_armv4_bs_wm_2,
	.bs_wm_4	= generic_bs_wm_4,
	.bs_wm_8	= NULL,

	/* write region */
	.bs_wr_1	= generic_bs_wr_1,
	.bs_wr_2	= generic_armv4_bs_wr_2,
	.bs_wr_4	= generic_bs_wr_4,
	.bs_wr_8	= NULL,

	/* set multiple */
	/* XXX not implemented */

	/* set region */
	.bs_sr_1	= NULL,
	.bs_sr_2	= generic_armv4_bs_sr_2,
	.bs_sr_4	= generic_bs_sr_4,
	.bs_sr_8	= NULL,

	/* copy */
	.bs_c_1		= NULL,
	.bs_c_2		= generic_armv4_bs_c_2,
	.bs_c_4		= NULL,
	.bs_c_8		= NULL,
};

int
ixp425_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
	      int cacheable, bus_space_handle_t *bshp)
{
	const struct pmap_devmap *pd;
	vm_paddr_t startpa, endpa, pa, offset;
	vm_offset_t va;
	pt_entry_t *pte;

	if ((pd = pmap_devmap_find_pa(bpa, size)) != NULL) {
		/* Device was statically mapped. */
		*bshp = pd->pd_va + (bpa - pd->pd_pa);
		return (0);
	}

	endpa = round_page(bpa + size);
	offset = bpa & PAGE_MASK;
	startpa = trunc_page(bpa);

	va = kmem_alloc(kernel_map, endpa - startpa);
	if (va == 0)
		return (ENOMEM);

	*bshp = va + offset;

	for (pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_kenter(va, pa);
		pte = vtopte(va);
		*pte &= ~L2_S_CACHE_MASK;
		PTE_SYNC(pte);
	}

	return (0);
}

void
ixp425_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
{
	vm_offset_t va, endva;

	if (pmap_devmap_find_va((vm_offset_t)t, size) != NULL) {
		/* Device was statically mapped; nothing to do. */
		return;
	}

	endva = round_page((vm_offset_t)t + size);
	va = trunc_page((vm_offset_t)t);

	while (va < endva) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	kmem_free(kernel_map, va, endva - va);
}

int
ixp425_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
	bus_size_t size, bus_size_t alignment, bus_size_t boundary, int cacheable,
	bus_addr_t *bpap, bus_space_handle_t *bshp)
{
	panic("ixp425_bs_alloc(): not implemented");
}

void    
ixp425_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	panic("ixp425_bs_free(): not implemented");
}

int
ixp425_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
	bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void
ixp425_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{
	/* Nothing to do. */
}	
