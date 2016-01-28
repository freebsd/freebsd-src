/*
 * Copyright (C) 2016 Cavium Inc.
 * All rights reserved.
 *
 * Developed by Semihalf.
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
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/cpuset.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "thunder_pcie_common.h"

#define	OFW_CELL_TO_UINT64(cell)	\
    (((uint64_t)(*(cell)) << 32) | (uint64_t)(*((cell) + 1)))

#define	SPACE_CODE_SHIFT	24
#define	SPACE_CODE_MASK		0x3
#define	SPACE_CODE_IO_SPACE	0x1
#define	PROPS_CELL_SIZE		1
#define	PCI_ADDR_CELL_SIZE	2

static int thunder_pcie_fdt_probe(device_t);
static int thunder_pcie_fdt_attach(device_t);

static device_method_t thunder_pcie_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		thunder_pcie_fdt_probe),
	DEVMETHOD(device_attach,	thunder_pcie_fdt_attach),
	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, thunder_pcie_fdt_driver, thunder_pcie_fdt_methods,
    sizeof(struct thunder_pcie_softc), thunder_pcie_driver);

static devclass_t thunder_pcie_fdt_devclass;

DRIVER_MODULE(thunder_pcib, simplebus, thunder_pcie_fdt_driver,
    thunder_pcie_fdt_devclass, 0, 0);
DRIVER_MODULE(thunder_pcib, ofwbus, thunder_pcie_fdt_driver,
    thunder_pcie_fdt_devclass, 0, 0);

static int thunder_pcie_fdt_ranges(device_t);

static int
thunder_pcie_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "cavium,thunder-pcie") ||
	    ofw_bus_is_compatible(dev, "cavium,pci-host-thunder-ecam")) {
		device_set_desc(dev, "Cavium Integrated PCI/PCI-E Controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
thunder_pcie_fdt_attach(device_t dev)
{

	/* Retrieve 'ranges' property from FDT */
	if (thunder_pcie_fdt_ranges(dev) != 0)
		return (ENXIO);

	return (thunder_pcie_attach(dev));
}

static int
thunder_pcie_fdt_ranges(device_t dev)
{
	struct thunder_pcie_softc *sc;
	phandle_t node;
	pcell_t pci_addr_cells, parent_addr_cells, size_cells;
	pcell_t attributes;
	pcell_t *ranges_buf, *cell_ptr;
	int cells_count, tuples_count;
	int tuple;
	int rv;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	/* Find address cells if present */
	if (OF_getencprop(node, "#address-cells", &pci_addr_cells,
	    sizeof(pci_addr_cells)) < sizeof(pci_addr_cells))
		pci_addr_cells = 2;

	/* Find size cells if present */
	if (OF_getencprop(node, "#size-cells", &size_cells,
	    sizeof(size_cells)) < sizeof(size_cells))
		size_cells = 1;

	/* Find parent address cells if present */
	if (OF_getencprop(OF_parent(node), "#address-cells",
	    &parent_addr_cells, sizeof(parent_addr_cells)) < sizeof(parent_addr_cells))
		parent_addr_cells = 2;

	/* Check if FDT format matches driver requirements */
	if ((parent_addr_cells != 2) || (pci_addr_cells != 3) ||
	    (size_cells != 2)) {
		device_printf(dev,
		    "Unexpected number of address or size cells in FDT "
		    " %d:%d:%d\n",
		    parent_addr_cells, pci_addr_cells, size_cells);
		return (ENXIO);
	}

	cells_count = OF_getencprop_alloc(node, "ranges",
	    sizeof(pcell_t), (void **)&ranges_buf);
	if (cells_count == -1) {
		device_printf(dev, "Error parsing FDT 'ranges' property\n");
		return (ENXIO);
	}

	tuples_count = cells_count /
	    (pci_addr_cells + parent_addr_cells + size_cells);
	if (tuples_count > RANGES_TUPLES_MAX) {
		device_printf(dev,
		    "Unexpected number of 'ranges' tuples in FDT\n");
		rv = ENXIO;
		goto out;
	}

	cell_ptr = ranges_buf;

	for (tuple = 0; tuple < tuples_count; tuple++) {
		/*
		 * TUPLE FORMAT:
		 *  attributes  - 32-bit attributes field
		 *  PCI address - bus address combined of two cells in
		 *                a following format:
		 *                <ADDR MSB> <ADDR LSB>
		 *  PA address  - physical address combined of two cells in
		 *                a following format:
		 *                <ADDR MSB> <ADDR LSB>
		 *  size        - range size combined of two cells in
		 *                a following format:
		 *                <ADDR MSB> <ADDR LSB>
		 */
		attributes = *cell_ptr;
		attributes = (attributes >> SPACE_CODE_SHIFT) & SPACE_CODE_MASK;
		if (attributes == SPACE_CODE_IO_SPACE) {
			/* Internal PCIe does not support IO space, ignore. */
			sc->ranges[tuple].phys_base = 0;
			sc->ranges[tuple].size = 0;
			cell_ptr +=
			    (pci_addr_cells + parent_addr_cells + size_cells);
			continue;
		}
		cell_ptr += PROPS_CELL_SIZE;
		sc->ranges[tuple].pci_base = OFW_CELL_TO_UINT64(cell_ptr);
		cell_ptr += PCI_ADDR_CELL_SIZE;
		sc->ranges[tuple].phys_base = OFW_CELL_TO_UINT64(cell_ptr);
		cell_ptr += parent_addr_cells;
		sc->ranges[tuple].size = OFW_CELL_TO_UINT64(cell_ptr);
		cell_ptr += size_cells;

		if (bootverbose) {
			device_printf(dev,
			    "\tPCI addr: 0x%jx, CPU addr: 0x%jx, Size: 0x%jx\n",
			    sc->ranges[tuple].pci_base,
			    sc->ranges[tuple].phys_base,
			    sc->ranges[tuple].size);
		}

	}
	for (; tuple < RANGES_TUPLES_MAX; tuple++) {
		/* zero-fill remaining tuples to mark empty elements in array */
		sc->ranges[tuple].phys_base = 0;
		sc->ranges[tuple].size = 0;
	}

	rv = 0;
out:
	free(ranges_buf, M_OFWPROP);
	return (rv);
}
