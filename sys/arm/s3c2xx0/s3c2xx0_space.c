/*	$NetBSD: s3c2xx0_space.c,v 1.7 2005/11/24 13:08:32 yamt Exp $ */

/*
 * Copyright (c) 2002 Fujitsu Component Limited
 * Copyright (c) 2002 Genetec Corporation
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
 * 3. Neither the name of The Fujitsu Component Limited nor the name of
 *    Genetec corporation may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
 * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
 * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* derived from sa11x0_io.c */

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * bus_space functions for Samsung S3C2800/2400/2410.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(s3c2xx0);
bs_protos(generic);
bs_protos(generic_armv4);

struct bus_space s3c2xx0_bs_tag = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	s3c2xx0_bs_map,
	s3c2xx0_bs_unmap,
	s3c2xx0_bs_subregion,

	/* allocation/deallocation */
	s3c2xx0_bs_alloc,	/* not implemented */
	s3c2xx0_bs_free,	/* not implemented */

	/* barrier */
	s3c2xx0_bs_barrier,

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
	generic_bs_wr_1,
	generic_armv4_bs_wr_2,
	generic_bs_wr_4,
	NULL,

	/* set multiple */
	NULL,
	NULL,
	NULL,
	NULL,

	/* set region */
	generic_bs_sr_1,
	generic_armv4_bs_sr_2,
	NULL,
	NULL,

	/* copy */
	NULL,
	generic_armv4_bs_c_2,
	NULL,
	NULL,
};

int
s3c2xx0_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
	       int flag, bus_space_handle_t * bshp)
{
	u_long startpa, endpa, pa;
	vm_offset_t va;
	pt_entry_t *pte;
	const struct pmap_devmap *pd;

	if ((pd = pmap_devmap_find_pa(bpa, size)) != NULL) {
		/* Device was statically mapped. */
		*bshp = pd->pd_va + (bpa - pd->pd_pa);
		return 0;
	}

	startpa = trunc_page(bpa);
	endpa = round_page(bpa + size);

	va = kmem_alloc_nofault(kernel_map, endpa - startpa);
	if (!va)
		return (ENOMEM);

	*bshp = (bus_space_handle_t) (va + (bpa - startpa));

	for (pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_kenter(va, pa);
		pte = vtopte(va);
		if ((flag & BUS_SPACE_MAP_CACHEABLE) == 0)
			*pte &= ~L2_S_CACHE_MASK;
	}
	return (0);
}

void
s3c2xx0_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
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
s3c2xx0_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
		     bus_size_t size, bus_space_handle_t * nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}

void
s3c2xx0_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
		   bus_size_t len, int flags)
{

	/* Nothing to do. */
}

int
s3c2xx0_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
		 bus_size_t size, bus_size_t alignment, bus_size_t boundary,
    		 int flags, bus_addr_t * bpap, bus_space_handle_t * bshp)
{

	panic("s3c2xx0_io_bs_alloc(): not implemented\n");
}

void
s3c2xx0_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	panic("s3c2xx0_io_bs_free(): not implemented\n");
}
