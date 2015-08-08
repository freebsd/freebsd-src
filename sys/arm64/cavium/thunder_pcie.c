/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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

/* PCIe root complex driver for Cavium Thunder SOC */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/cpuset.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include "thunder_pcie_common.h"

#include "pcib_if.h"

/* Assembling ECAM Configuration Address */
#define	PCIE_BUS_SHIFT		20
#define	PCIE_SLOT_SHIFT		15
#define	PCIE_FUNC_SHIFT		12
#define	PCIE_BUS_MASK		0xFF
#define	PCIE_SLOT_MASK		0x1F
#define	PCIE_FUNC_MASK		0x07
#define	PCIE_REG_MASK		0xFFF

#define	PCIE_ADDR_OFFSET(bus, slot, func, reg)			\
    ((((bus) & PCIE_BUS_MASK) << PCIE_BUS_SHIFT)	|	\
    (((slot) & PCIE_SLOT_MASK) << PCIE_SLOT_SHIFT)	|	\
    (((func) & PCIE_FUNC_MASK) << PCIE_FUNC_SHIFT)	|	\
    ((reg) & PCIE_REG_MASK))

#define	THUNDER_ECAM0_CFG_BASE	0x848000000000UL
#define	THUNDER_ECAM1_CFG_BASE	0x849000000000UL
#define	THUNDER_ECAM2_CFG_BASE	0x84a000000000UL
#define	THUNDER_ECAM3_CFG_BASE	0x84b000000000UL
#define	THUNDER_ECAM4_CFG_BASE	0x948000000000UL
#define	THUNDER_ECAM5_CFG_BASE	0x949000000000UL
#define	THUNDER_ECAM6_CFG_BASE	0x94a000000000UL
#define	THUNDER_ECAM7_CFG_BASE	0x94b000000000UL

#define	OFW_CELL_TO_UINT64(cell)	\
    (((uint64_t)(*(cell)) << 32) | (uint64_t)(*((cell) + 1)))

#define	SPACE_CODE_SHIFT	24
#define	SPACE_CODE_MASK		0x3
#define	SPACE_CODE_IO_SPACE	0x1
#define	PROPS_CELL_SIZE		1
#define	PCI_ADDR_CELL_SIZE	2

struct thunder_pcie_softc {
	struct pcie_range	ranges[MAX_RANGES_TUPLES];
	struct rman		mem_rman;
	struct resource		*res;
	int			ecam;
	device_t		dev;
};

/* Forward prototypes */
static struct resource *thunder_pcie_alloc_resource(device_t,
    device_t, int, int *, u_long, u_long, u_long, u_int);
static int thunder_pcie_attach(device_t);
static int thunder_pcie_identify_pcib(device_t);
static int thunder_pcie_maxslots(device_t);
static int parse_pci_mem_ranges(struct thunder_pcie_softc *);
static int thunder_pcie_probe(device_t);
static uint32_t thunder_pcie_read_config(device_t, u_int, u_int, u_int, u_int,
    int);
static int thunder_pcie_read_ivar(device_t, device_t, int, uintptr_t *);
static int thunder_pcie_release_resource(device_t, device_t, int, int,
    struct resource *);
static void thunder_pcie_write_config(device_t, u_int, u_int,
    u_int, u_int, uint32_t, int);
static int thunder_pcie_write_ivar(device_t, device_t, int, uintptr_t);

static int
thunder_pcie_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "cavium,thunder-pcie")) {
		device_set_desc(dev, "Cavium Integrated PCI/PCI-E Controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
thunder_pcie_attach(device_t dev)
{
	int rid;
	struct thunder_pcie_softc *sc;
	int error;
	int tuple;
	uint64_t base, size;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Identify pcib domain */
	if (thunder_pcie_identify_pcib(dev))
		return (ENXIO);

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not map memory.\n");
		return (ENXIO);
	}

	sc->mem_rman.rm_type = RMAN_ARRAY;
	sc->mem_rman.rm_descr = "PCIe Memory";

	/* Retrieve 'ranges' property from FDT */
	if (bootverbose)
		device_printf(dev, "parsing FDT for ECAM%d:\n",
		    sc->ecam);
	if (parse_pci_mem_ranges(sc))
		return (ENXIO);

	/* Initialize rman and allocate memory regions */
	error = rman_init(&sc->mem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		base = sc->ranges[tuple].phys_base;
		size = sc->ranges[tuple].size;
		if ((base == 0) || (size == 0))
			continue; /* empty range element */

		error = rman_manage_region(&sc->mem_rman, base, base + size - 1);
		if (error) {
			device_printf(dev, "rman_manage_region() failed. error = %d\n", error);
			rman_fini(&sc->mem_rman);
			return (error);
		}
	}
	device_add_child(dev, "pci", -1);

	return (bus_generic_attach(dev));
}

static int
parse_pci_mem_ranges(struct thunder_pcie_softc *sc)
{
	phandle_t node;
	pcell_t pci_addr_cells, parent_addr_cells, size_cells;
	pcell_t attributes;
	pcell_t *ranges_buf, *cell_ptr;
	int cells_count, tuples_count;
	int tuple;
	int rv;

	node = ofw_bus_get_node(sc->dev);

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
		device_printf(sc->dev,
		    "Unexpected number of address or size cells in FDT "
		    " %d:%d:%d\n",
		    parent_addr_cells, pci_addr_cells, size_cells);
		return (ENXIO);
	}

	cells_count = OF_getencprop_alloc(node, "ranges",
	    sizeof(pcell_t), (void **)&ranges_buf);
	if (cells_count == -1) {
		device_printf(sc->dev, "Error parsing FDT 'ranges' property\n");
		return (ENXIO);
	}

	tuples_count = cells_count /
	    (pci_addr_cells + parent_addr_cells + size_cells);
	if ((tuples_count > MAX_RANGES_TUPLES) ||
	    (tuples_count < MIN_RANGES_TUPLES)) {
		device_printf(sc->dev,
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
			device_printf(sc->dev,
			    "\tPCI addr: 0x%jx, CPU addr: 0x%jx, Size: 0x%jx\n",
			    sc->ranges[tuple].pci_base,
			    sc->ranges[tuple].phys_base,
			    sc->ranges[tuple].size);
		}

	}
	for (; tuple < MAX_RANGES_TUPLES; tuple++) {
		/* zero-fill remaining tuples to mark empty elements in array */
		sc->ranges[tuple].phys_base = 0;
		sc->ranges[tuple].size = 0;
	}

	rv = 0;
out:
	free(ranges_buf, M_OFWPROP);
	return (rv);
}

static uint32_t
thunder_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	uint64_t offset;
	uint32_t data;
	struct thunder_pcie_softc *sc;
	bus_space_tag_t	t;
	bus_space_handle_t h;

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
		return (~0U);

	sc = device_get_softc(dev);

	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);
	t = rman_get_bustag(sc->res);
	h = rman_get_bushandle(sc->res);

	switch (bytes) {
	case 1:
		data = bus_space_read_1(t, h, offset);
		break;
	case 2:
		data = le16toh(bus_space_read_2(t, h, offset));
		break;
	case 4:
		data = le32toh(bus_space_read_4(t, h, offset));
		break;
	default:
		return (~0U);
	}

	return (data);
}

static void
thunder_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	uint64_t offset;
	struct thunder_pcie_softc *sc;
	bus_space_tag_t	t;
	bus_space_handle_t h;

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
		return ;

	sc = device_get_softc(dev);

	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);
	t = rman_get_bustag(sc->res);
	h = rman_get_bushandle(sc->res);

	switch (bytes) {
	case 1:
		bus_space_write_1(t, h, offset, val);
		break;
	case 2:
		bus_space_write_2(t, h, offset, htole16(val));
		break;
	case 4:
		bus_space_write_4(t, h, offset, htole32(val));
		break;
	default:
		return;
	}

}

static int
thunder_pcie_maxslots(device_t dev)
{

	/* max slots per bus acc. to standard */
	return (PCI_SLOTMAX);
}

static int
thunder_pcie_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	struct thunder_pcie_softc *sc;

	sc = device_get_softc(dev);

	if (index == PCIB_IVAR_BUS) {
		/* this pcib is always on bus 0 */
		*result = 0;
		return (0);
	}
	if (index == PCIB_IVAR_DOMAIN) {
		*result = sc->ecam;
		return (0);
	}

	return (ENOENT);
}

static int
thunder_pcie_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{

	return (ENOENT);
}

static int
thunder_pcie_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	if (type != SYS_RES_MEMORY)
		return (BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
		    type, rid, res));

	return (rman_release_resource(res));
}

static struct resource *
thunder_pcie_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct thunder_pcie_softc *sc = device_get_softc(dev);
	struct rman *rm = NULL;
	struct resource *res;

	switch (type) {
	case SYS_RES_IOPORT:
		goto fail;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->mem_rman;
		break;
	default:
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
		    type, rid, start, end, count, flags));
	};

	if ((start == 0UL) && (end == ~0UL)) {
		device_printf(dev,
		    "Cannot allocate resource with unspecified range\n");
		goto fail;
	}

	/* Convert input BUS address to required PHYS */
	if (range_addr_is_pci(sc->ranges, start, count) == 0)
		goto fail;
	start = range_addr_pci_to_phys(sc->ranges, start);
	end = start + count - 1;

	if (bootverbose) {
		device_printf(dev,
		    "rman_reserve_resource: start=%#lx, end=%#lx, count=%#lx\n",
		    start, end, count);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		goto fail;

	rman_set_rid(res, *rid);

	if ((flags & RF_ACTIVE) != 0)
		if (bus_activate_resource(child, type, *rid, res)) {
			rman_release_resource(res);
			goto fail;
		}

	return (res);

fail:
	if (bootverbose) {
		device_printf(dev, "%s FAIL: type=%d, rid=%d, "
		    "start=%016lx, end=%016lx, count=%016lx, flags=%x\n",
		    __func__, type, *rid, start, end, count, flags);
	}

	return (NULL);
}

static int
thunder_pcie_identify_pcib(device_t dev)
{
	struct thunder_pcie_softc *sc;
	u_long start;

	sc = device_get_softc(dev);
	start = bus_get_resource_start(dev, SYS_RES_MEMORY, 0);

	switch(start) {
	case THUNDER_ECAM0_CFG_BASE:
		sc->ecam = 0;
		break;
	case THUNDER_ECAM1_CFG_BASE:
		sc->ecam = 1;
		break;
	case THUNDER_ECAM2_CFG_BASE:
		sc->ecam = 2;
		break;
	case THUNDER_ECAM3_CFG_BASE:
		sc->ecam = 3;
		break;
	case THUNDER_ECAM4_CFG_BASE:
		sc->ecam = 4;
		break;
	case THUNDER_ECAM5_CFG_BASE:
		sc->ecam = 5;
		break;
	case THUNDER_ECAM6_CFG_BASE:
		sc->ecam = 6;
		break;
	case THUNDER_ECAM7_CFG_BASE:
		sc->ecam = 7;
		break;
	default:
		device_printf(dev,
		    "error: incorrect resource address=%#lx.\n", start);
		return (ENXIO);
	}
	return (0);
}

static device_method_t thunder_pcie_methods[] = {
	DEVMETHOD(device_probe,			thunder_pcie_probe),
	DEVMETHOD(device_attach,		thunder_pcie_attach),
	DEVMETHOD(pcib_maxslots,		thunder_pcie_maxslots),
	DEVMETHOD(pcib_read_config,		thunder_pcie_read_config),
	DEVMETHOD(pcib_write_config,		thunder_pcie_write_config),
	DEVMETHOD(bus_read_ivar,		thunder_pcie_read_ivar),
	DEVMETHOD(bus_write_ivar,		thunder_pcie_write_ivar),
	DEVMETHOD(bus_alloc_resource,		thunder_pcie_alloc_resource),
	DEVMETHOD(bus_release_resource,		thunder_pcie_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(pcib_map_msi,			thunder_common_map_msi),
	DEVMETHOD(pcib_alloc_msix,		thunder_common_alloc_msix),
	DEVMETHOD(pcib_release_msix,		thunder_common_release_msix),
	DEVMETHOD(pcib_alloc_msi,		thunder_common_alloc_msi),
	DEVMETHOD(pcib_release_msi,		thunder_common_release_msi),

	DEVMETHOD_END
};

static driver_t thunder_pcie_driver = {
	"pcib",
	thunder_pcie_methods,
	sizeof(struct thunder_pcie_softc),
};

static devclass_t thunder_pcie_devclass;

DRIVER_MODULE(thunder_pcib, simplebus, thunder_pcie_driver,
thunder_pcie_devclass, 0, 0);
DRIVER_MODULE(thunder_pcib, ofwbus, thunder_pcie_driver,
thunder_pcie_devclass, 0, 0);
