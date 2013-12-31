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
#include <dev/pci/pcireg.h>

#include <machine/fdt.h>
#if defined(__arm__)
#include <machine/devmap.h>
#endif

#include "ofw_bus_if.h"
#include "pcib_if.h"

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

	/*
	 * Initialize the ranges so that we don't have to worry about
	 * having them all defined in the FDT. In particular, it is
	 * perfectly fine not to want I/O space on PCI busses.
	 */
	bzero(io_space, sizeof(*io_space));
	bzero(mem_space, sizeof(*mem_space));

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
	int err;

	debugf("Processing PCI node: %x\n", node);
	if ((err = fdt_pci_ranges_decode(node, io_space, mem_space)) != 0) {
		debugf("could not decode parent PCI node 'ranges'\n");
		return (err);
	}

	debugf("Post fixup dump:\n");
	fdt_pci_range_dump(io_space);
	fdt_pci_range_dump(mem_space);
	return (0);
}

#if defined(__arm__)
int
fdt_pci_devmap(phandle_t node, struct arm_devmap_entry *devmap, vm_offset_t io_va,
    vm_offset_t mem_va)
{
	struct fdt_pci_range io_space, mem_space;
	int error;

	if ((error = fdt_pci_ranges_decode(node, &io_space, &mem_space)) != 0)
		return (error);

	devmap->pd_va = (io_va ? io_va : io_space.base_parent);
	devmap->pd_pa = io_space.base_parent;
	devmap->pd_size = io_space.len;
	devmap->pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	devmap->pd_cache = PTE_NOCACHE;
	devmap++;

	devmap->pd_va = (mem_va ? mem_va : mem_space.base_parent);
	devmap->pd_pa = mem_space.base_parent;
	devmap->pd_size = mem_space.len;
	devmap->pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	devmap->pd_cache = PTE_NOCACHE;
	return (0);
}
#endif

