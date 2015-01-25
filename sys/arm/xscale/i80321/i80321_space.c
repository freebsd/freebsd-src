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
 * bus_space functions for i80321 I/O Processor.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include <arm/xscale/i80321/i80321reg.h>
#include <arm/xscale/i80321/i80321var.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(i80321);
bs_protos(i80321_io);
bs_protos(i80321_mem);

void
i80321_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = *arm_base_bs_tag;
	bs->bs_privdata = cookie;
}

void
i80321_io_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = *arm_base_bs_tag;
	bs->bs_privdata = cookie;

	bs->bs_map = i80321_io_bs_map;
	bs->bs_unmap = i80321_io_bs_unmap;
	bs->bs_alloc = i80321_io_bs_alloc;
	bs->bs_free = i80321_io_bs_free;

}

void
i80321_mem_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = *arm_base_bs_tag;
	bs->bs_privdata = cookie;

	bs->bs_map = i80321_mem_bs_map;
	bs->bs_unmap = i80321_mem_bs_unmap;
	bs->bs_alloc = i80321_mem_bs_alloc;
	bs->bs_free = i80321_mem_bs_free;

}

/* *** Routines shared by i80321, PCI IO, and PCI MEM. *** */

int
i80321_bs_subregion(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}

void
i80321_bs_barrier(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{

	/* Nothing to do. */
}

/* *** Routines for PCI IO. *** */

extern struct i80321_softc *i80321_softc;
int
i80321_io_bs_map(bus_space_tag_t tag, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	struct i80321_softc *sc = i80321_softc;
	vm_offset_t winvaddr;
	uint32_t busbase;

	if (bpa >= sc->sc_ioout_xlate &&
	    bpa < (sc->sc_ioout_xlate + VERDE_OUT_XLATE_IO_WIN_SIZE)) {
		busbase = sc->sc_ioout_xlate;
		winvaddr = sc->sc_iow_vaddr;
	} else
		return (EINVAL);

	if ((bpa + size) >= (busbase + VERDE_OUT_XLATE_IO_WIN_SIZE))
		return (EINVAL);

	/*
	 * Found the window -- PCI I/O space is mapped at a fixed
	 * virtual address by board-specific code.  Translate the
	 * bus address to the virtual address.
	 */
	*bshp = winvaddr + (bpa - busbase);

	return (0);
}

void
i80321_io_bs_unmap(bus_space_tag_t tag, bus_space_handle_t h, bus_size_t size)
{

	/* Nothing to do. */
}

int
i80321_io_bs_alloc(bus_space_tag_t tag, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i80321_io_bs_alloc(): not implemented");
}

void
i80321_io_bs_free(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i80321_io_bs_free(): not implemented");
}


/* *** Routines for PCI MEM. *** */
extern int badaddr_read(void *, int, void *);
int
i80321_mem_bs_map(bus_space_tag_t tag, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{

	*bshp = (vm_offset_t)pmap_mapdev(bpa, size);
	return (0);
}

void
i80321_mem_bs_unmap(bus_space_tag_t tag, bus_space_handle_t h, bus_size_t size)
{

	pmap_unmapdev((vm_offset_t)h, size);
}

int
i80321_mem_bs_alloc(bus_space_tag_t tag, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i80321_mem_bs_alloc(): not implemented");
}

void
i80321_mem_bs_free(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i80321_mem_bs_free(): not implemented");
}
