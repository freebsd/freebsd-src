/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/fdt/fdt_common.h>

#include <machine/fdt.h>

#include "ofw_bus_if.h"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

#define FDT_RANGES_CELLS	((3 + 3 + 2) * 2)

static void
fdt_pci_range_dump(struct fdt_pci_range *range)
{
#ifdef DEBUG
	printf("\n");
	printf("  base_pci = 0x%08lx\n", range->base_pci);
	printf("  base_par = 0x%08lx\n", range->base_parent);
	printf("  len      = 0x%08lx\n", range->len);
#endif
}

int
fdt_pci_ranges_decode(phandle_t node, struct fdt_pci_range *io_space,
    struct fdt_pci_range *mem_space)
{
	pcell_t ranges[FDT_RANGES_CELLS];
	struct fdt_pci_range *pci_space;
	pcell_t addr_cells, size_cells, par_addr_cells;
	pcell_t *rangesptr;
	pcell_t cell0, cell1, cell2;
	int tuple_size, tuples, i, rv, offset_cells, len;

	/*
	 * Retrieve 'ranges' property.
	 */
	if ((fdt_addrsize_cells(node, &addr_cells, &size_cells)) != 0)
		return (EINVAL);
	if (addr_cells != 3 || size_cells != 2)
		return (ERANGE);

	par_addr_cells = fdt_parent_addr_cells(node);
	if (par_addr_cells > 3)
		return (ERANGE);

	len = OF_getproplen(node, "ranges");
	if (len > sizeof(ranges))
		return (ENOMEM);

	if (OF_getprop(node, "ranges", ranges, sizeof(ranges)) <= 0)
		return (EINVAL);

	tuple_size = sizeof(pcell_t) * (addr_cells + par_addr_cells +
	    size_cells);
	tuples = len / tuple_size;

	rangesptr = &ranges[0];
	offset_cells = 0;
	for (i = 0; i < tuples; i++) {
		cell0 = fdt_data_get((void *)rangesptr, 1);
		rangesptr++;
		cell1 = fdt_data_get((void *)rangesptr, 1);
		rangesptr++;
		cell2 = fdt_data_get((void *)rangesptr, 1);
		rangesptr++;

		if (cell0 & 0x02000000) {
			pci_space = mem_space;
		} else if (cell0 & 0x01000000) {
			pci_space = io_space;
		} else {
			rv = ERANGE;
			goto out;
		}

		if (par_addr_cells == 3) {
			/*
			 * This is a PCI subnode 'ranges'. Skip cell0 and
			 * cell1 of this entry and only use cell2.
			 */
			offset_cells = 2;
			rangesptr += offset_cells;
		}

		if (fdt_data_verify((void *)rangesptr, par_addr_cells -
		    offset_cells)) {
			rv = ERANGE;
			goto out;
		}
		pci_space->base_parent = fdt_data_get((void *)rangesptr,
		    par_addr_cells - offset_cells);
		rangesptr += par_addr_cells - offset_cells;

		if (fdt_data_verify((void *)rangesptr, size_cells)) {
			rv = ERANGE;
			goto out;
		}
		pci_space->len = fdt_data_get((void *)rangesptr, size_cells);
		rangesptr += size_cells;

		pci_space->base_pci = cell2;
	}
	rv = 0;
out:
	return (rv);
}

int
fdt_pci_ranges(phandle_t node, struct fdt_pci_range *io_space,
    struct fdt_pci_range *mem_space)
{
	struct fdt_pci_range par_io_space, par_mem_space;
	u_long base;
	int err;

	debugf("Processing parent PCI node: %x\n", node);
	if ((err = fdt_pci_ranges_decode(OF_parent(node), &par_io_space,
	    &par_mem_space)) != 0) {
		debugf("could not decode parent PCI node 'ranges'\n");
		return (err);
	}

	debugf("Processing PCI sub node: %x\n", node);
	if ((err = fdt_pci_ranges_decode(node, io_space, mem_space)) != 0) {
		debugf("could not decode PCI subnode 'ranges'\n");
		return (err);
	}

	base = io_space->base_parent & 0x000fffff;
	base += par_io_space.base_parent;
	io_space->base_parent = base;

	base = mem_space->base_parent & 0x000fffff;
	base += par_mem_space.base_parent;
	mem_space->base_parent = base;

	debugf("Post fixup dump:\n");
	fdt_pci_range_dump(io_space);
	fdt_pci_range_dump(mem_space);

	return (0);
}

static int
fdt_addr_cells(phandle_t node, int *addr_cells)
{
	pcell_t cell;
	int cell_size;

	cell_size = sizeof(cell);
	if (OF_getprop(node, "#address-cells", &cell, cell_size) < cell_size)
		return (EINVAL);
	*addr_cells = fdt32_to_cpu((int)cell);

	if (*addr_cells > 3)
		return (ERANGE);
	return (0);
}

static int
fdt_interrupt_cells(phandle_t node)
{
	pcell_t intr_cells;

	if (OF_getprop(node, "#interrupt-cells", &intr_cells,
	    sizeof(intr_cells)) <= 0) {
		debugf("no intr-cells defined, defaulting to 1\n");
		intr_cells = 1;
	}
	intr_cells = fdt32_to_cpu(intr_cells);

	return ((int)intr_cells);
}

int
fdt_pci_intr_info(phandle_t node, struct fdt_pci_intr *intr_info)
{
	void *map, *mask;
	phandle_t pci_par;
	int intr_cells, addr_cells;
	int len;

	addr_cells = fdt_parent_addr_cells(node);

	pci_par = OF_parent(node);
	intr_cells = fdt_interrupt_cells(pci_par);

	/*
	 * Retrieve the interrupt map and mask properties.
	 */
	len = OF_getprop_alloc(pci_par, "interrupt-map-mask", 1, &mask);
	if (len / sizeof(pcell_t) != (addr_cells + intr_cells)) {
		debugf("bad mask len = %d\n", len);
		goto err;
	}

	len = OF_getprop_alloc(pci_par, "interrupt-map", 1, &map);
	if (len <= 0) {
		debugf("bad map len = %d\n", len);
		goto err;
	}

	intr_info->map_len = len;
	intr_info->map = map;
	intr_info->mask = mask;
	intr_info->addr_cells = addr_cells;
	intr_info->intr_cells = intr_cells;
	return (0);

err:
	free(mask, M_OFWPROP);
	return (ENXIO);
}

int
fdt_pci_route_intr(int bus, int slot, int func, int pin,
    struct fdt_pci_intr *intr_info, int *interrupt)
{
	pcell_t child_spec[4], masked[4];
	ihandle_t iph;
	pcell_t intr_par;
	pcell_t *map_ptr;
	uint32_t addr;
	int i, j, map_len;
	int par_intr_cells, par_addr_cells, child_spec_cells, row_cells;
	int par_idx, spec_idx, err, trig, pol;

	child_spec_cells = intr_info->addr_cells + intr_info->intr_cells;
	if (child_spec_cells > sizeof(child_spec) / sizeof(pcell_t))
		return (ENOMEM);

	addr = (bus << 16) | (slot << 11) | (func << 8);
	child_spec[0] = addr;
	child_spec[1] = 0;
	child_spec[2] = 0;
	child_spec[3] = pin;

	map_len = intr_info->map_len;
	map_ptr = intr_info->map;

	par_idx = child_spec_cells;
	i = 0;
	while (i < map_len) {
		iph = fdt32_to_cpu(map_ptr[par_idx]);
		intr_par = OF_instance_to_package(iph);

		err = fdt_addr_cells(intr_par, &par_addr_cells);
		if (err != 0) {
			debugf("could not retrieve intr parent #addr-cells\n");
			return (err);
		}
		par_intr_cells = fdt_interrupt_cells(intr_par);

		row_cells = child_spec_cells + 1 + par_addr_cells +
		    par_intr_cells;

		/*
		 * Apply mask and look up the entry in interrupt map.
		 */
		for (j = 0; j < child_spec_cells; j++) {
			masked[j] = child_spec[j] &
			    fdt32_to_cpu(intr_info->mask[j]);

			if (masked[j] != fdt32_to_cpu(map_ptr[j]))
				goto next;
		}

		/*
		 * Decode interrupt of the parent intr controller.
		 */
		spec_idx = child_spec_cells + 1 + par_addr_cells;
		err = fdt_intr_decode(intr_par, &map_ptr[spec_idx],
		    interrupt, &trig, &pol);
		if (err != 0) {
			debugf("could not decode interrupt\n");
			return (err);
		}
		debugf("decoded intr = %d, trig = %d, pol = %d\n", *interrupt,
		    trig, pol);

		/* XXX we should probably call powerpc_config() here... */

		return (0);

next:
		map_ptr += row_cells;
		i += (row_cells * sizeof(pcell_t));
	}

	return (ENXIO);
}

#if defined(__arm__)
int
fdt_pci_devmap(phandle_t node, struct pmap_devmap *devmap, vm_offset_t io_va,
    vm_offset_t mem_va)
{
	struct fdt_pci_range io_space, mem_space;
	int error;

	if ((error = fdt_pci_ranges_decode(node, &io_space, &mem_space)) != 0)
		return (error);

	devmap->pd_va = io_va;
	devmap->pd_pa = io_space.base_parent;
	devmap->pd_size = io_space.len;
	devmap->pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	devmap->pd_cache = PTE_NOCACHE;
	devmap++;

	devmap->pd_va = mem_va;
	devmap->pd_pa = mem_space.base_parent;
	devmap->pd_size = mem_space.len;
	devmap->pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	devmap->pd_cache = PTE_NOCACHE;
	return (0);
}
#endif
