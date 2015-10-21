/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2014 The FreeBSD Foundation
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

/* Generic ECAM PCIe driver */

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
#include <sys/rwlock.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <vm/vm_page.h>

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

#define	MAX_RANGES_TUPLES	5
#define	MIN_RANGES_TUPLES	2

#define	PCI_IO_WINDOW_OFFSET	0x1000
#define	PCI_IRQ_START		32
#define	PCI_IRQ_END		(PCI_IRQ_START + 4)

#define	SPACE_CODE_SHIFT	24
#define	SPACE_CODE_MASK		0x3
#define	SPACE_CODE_IO_SPACE	0x1
#define	PROPS_CELL_SIZE		1
#define	PCI_ADDR_CELL_SIZE	2

struct pcie_range {
	uint64_t	pci_base;
	uint64_t	phys_base;
	uint64_t	size;
	uint64_t	flags;
#define	FLAG_IO		(1 << 0)
#define	FLAG_MEM	(1 << 1)
};

struct generic_pcie_softc {
	struct pcie_range	ranges[MAX_RANGES_TUPLES];
	int			nranges;
	struct rman		mem_rman;
	struct rman		io_rman;
	struct rman		irq_rman;
	struct resource		*res;
	struct resource		*res1;
	int			ecam;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	device_t		dev;
	bus_space_handle_t	ioh;
};

/* Forward prototypes */

static int generic_pcie_probe(device_t dev);
static int generic_pcie_attach(device_t dev);
static int parse_pci_mem_ranges(struct generic_pcie_softc *sc);
static uint32_t generic_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes);
static void generic_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes);
static int generic_pcie_maxslots(device_t dev);
static int generic_pcie_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result);
static int generic_pcie_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value);
static struct resource *generic_pcie_alloc_resource(device_t dev,
    device_t child, int type, int *rid, u_long start, u_long end,
    u_long count, u_int flags);
static int generic_pcie_release_resource(device_t dev, device_t child,
    int type, int rid, struct resource *res);

static int
generic_pcie_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "pci-host-ecam-generic")) {
		device_set_desc(dev, "Generic PCI host controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
generic_pcie_attach(device_t dev)
{
	struct generic_pcie_softc *sc;
	uint64_t phys_base;
	uint64_t pci_base;
	uint64_t size;
	int error;
	int tuple;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not map memory.\n");
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	sc->mem_rman.rm_type = RMAN_ARRAY;
	sc->mem_rman.rm_descr = "PCIe Memory";
	sc->io_rman.rm_type = RMAN_ARRAY;
	sc->io_rman.rm_descr = "PCIe IO window";

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

	error = rman_init(&sc->io_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		phys_base = sc->ranges[tuple].phys_base;
		pci_base = sc->ranges[tuple].pci_base;
		size = sc->ranges[tuple].size;
		if (phys_base == 0 || size == 0)
			continue; /* empty range element */
		if (sc->ranges[tuple].flags & FLAG_MEM) {
			error = rman_manage_region(&sc->mem_rman,
						phys_base,
						phys_base + size);
		} else if (sc->ranges[tuple].flags & FLAG_IO) {
			error = rman_manage_region(&sc->io_rman,
					pci_base + PCI_IO_WINDOW_OFFSET,
					pci_base + PCI_IO_WINDOW_OFFSET + size);
		} else
			continue;
		if (error) {
			device_printf(dev, "rman_manage_region() failed."
						"error = %d\n", error);
			rman_fini(&sc->mem_rman);
			return (error);
		}
	}

	/* TODO: get IRQ numbers from FDT */
	sc->irq_rman.rm_type = RMAN_ARRAY;
	sc->irq_rman.rm_descr = "Generic PCIe IRQs";
	if (rman_init(&sc->irq_rman) != 0 ||
	    rman_manage_region(&sc->irq_rman, PCI_IRQ_START,
		PCI_IRQ_END) != 0) {
		panic("Generic PCI: failed to set up IRQ rman");
	}

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static int
parse_pci_mem_ranges(struct generic_pcie_softc *sc)
{
	pcell_t pci_addr_cells, parent_addr_cells;
	pcell_t attributes, size_cells;
	cell_t *base_ranges;
	int nbase_ranges;
	phandle_t node;
	int i, j, k;
	int tuple;

	node = ofw_bus_get_node(sc->dev);

	OF_getencprop(node, "#address-cells", &pci_addr_cells,
					sizeof(pci_addr_cells));
	OF_getencprop(node, "#size-cells", &size_cells,
					sizeof(size_cells));
	OF_getencprop(OF_parent(node), "#address-cells", &parent_addr_cells,
					sizeof(parent_addr_cells));

	if (parent_addr_cells != 2 || pci_addr_cells != 3 || size_cells != 2) {
		device_printf(sc->dev,
		    "Unexpected number of address or size cells in FDT\n");
		return (ENXIO);
	}

	nbase_ranges = OF_getproplen(node, "ranges");
	sc->nranges = nbase_ranges / sizeof(cell_t) /
	    (parent_addr_cells + pci_addr_cells + size_cells);
	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < sc->nranges; i++) {
		attributes = (base_ranges[j++] >> SPACE_CODE_SHIFT) & \
							SPACE_CODE_MASK;
		if (attributes == SPACE_CODE_IO_SPACE) {
			sc->ranges[i].flags |= FLAG_IO;
		} else {
			sc->ranges[i].flags |= FLAG_MEM;
		}

		sc->ranges[i].pci_base = 0;
		for (k = 0; k < (pci_addr_cells - 1); k++) {
			sc->ranges[i].pci_base <<= 32;
			sc->ranges[i].pci_base |= base_ranges[j++];
		}
		sc->ranges[i].phys_base = 0;
		for (k = 0; k < parent_addr_cells; k++) {
			sc->ranges[i].phys_base <<= 32;
			sc->ranges[i].phys_base |= base_ranges[j++];
		}
		sc->ranges[i].size = 0;
		for (k = 0; k < size_cells; k++) {
			sc->ranges[i].size <<= 32;
			sc->ranges[i].size |= base_ranges[j++];
		}
	}

	for (; i < MAX_RANGES_TUPLES; i++) {
		/* zero-fill remaining tuples to mark empty elements in array */
		sc->ranges[i].pci_base = 0;
		sc->ranges[i].phys_base = 0;
		sc->ranges[i].size = 0;
	}

	if (bootverbose) {
		for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
			device_printf(sc->dev,
			    "\tPCI addr: 0x%jx, CPU addr: 0x%jx, Size: 0x%jx\n",
			    sc->ranges[tuple].pci_base,
			    sc->ranges[tuple].phys_base,
			    sc->ranges[tuple].size);
		}
	}

	free(base_ranges, M_DEVBUF);
	return (0);
}

static uint32_t
generic_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct generic_pcie_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t	t;
	uint64_t offset;
	uint32_t data;

	if (bus > 255 || slot > 31 || func > 7 || reg > 4095)
		return (~0U);

	sc = device_get_softc(dev);

	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);
	t = sc->bst;
	h = sc->bsh;

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

	if (reg == PCIR_INTLINE) {
		data += PCI_IRQ_START;
	}

	return (data);
}

static void
generic_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct generic_pcie_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t t;
	uint64_t offset;

	if (bus > 255 || slot > 31 || func > 7 || reg > 4095)
		return;

	sc = device_get_softc(dev);

	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);

	t = sc->bst;
	h = sc->bsh;

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
generic_pcie_maxslots(device_t dev)
{

	return (31); /* max slots per bus acc. to standard */
}

static int
generic_pcie_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	struct generic_pcie_softc *sc;
	int secondary_bus;

	sc = device_get_softc(dev);

	if (index == PCIB_IVAR_BUS) {
		/* this pcib adds only pci bus 0 as child */
		secondary_bus = 0;
		*result = secondary_bus;
		return (0);

	}

	if (index == PCIB_IVAR_DOMAIN) {
		*result = sc->ecam;
		return (0);
	}

	device_printf(dev, "ERROR: Unknown index.\n");
	return (ENOENT);
}

static int
generic_pcie_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{

	return (ENOENT);
}

static struct rman *
generic_pcie_rman(struct generic_pcie_softc *sc, int type)
{

	switch (type) {
	case SYS_RES_IOPORT:
		return (&sc->io_rman);
	case SYS_RES_MEMORY:
		return (&sc->mem_rman);
	case SYS_RES_IRQ:
		return (&sc->irq_rman);
	default:
		break;
	}

	return (NULL);
}

static int
generic_pcie_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *res)
{
	struct generic_pcie_softc *sc;
	struct rman *rm;

	sc = device_get_softc(dev);

	rm = generic_pcie_rman(sc, type);
	if (rm != NULL) {
		KASSERT(rman_is_region_manager(res, rm), ("rman mismatch"));
		rman_release_resource(res);
	}

	return (bus_generic_release_resource(dev, child, type, rid, res));
}

static struct resource *
generic_pcie_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct generic_pcie_softc *sc;
	struct resource *res;
	struct rman *rm;

	sc = device_get_softc(dev);

	rm = generic_pcie_rman(sc, type);
	if (rm == NULL)
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
		    type, rid, start, end, count, flags));

	if (bootverbose) {
		device_printf(dev,
		    "rman_reserve_resource: start=%#lx, end=%#lx, count=%#lx\n",
		    start, end, count);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		goto fail;

	rman_set_rid(res, *rid);

	if (flags & RF_ACTIVE)
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
generic_pcie_adjust_resource(device_t dev, device_t child, int type,
    struct resource *res, u_long start, u_long end)
{
	struct generic_pcie_softc *sc;
	struct rman *rm;

	sc = device_get_softc(dev);

	rm = generic_pcie_rman(sc, type);
	if (rm != NULL)
		return (rman_adjust_resource(res, start, end));
	return (bus_generic_adjust_resource(dev, child, type, res, start, end));
}

static int
generic_pcie_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct generic_pcie_softc *sc;
	uint64_t phys_base;
	uint64_t pci_base;
	uint64_t size;
	int found;
	int res;
	int i;

	sc = device_get_softc(dev);

	if ((res = rman_activate_resource(r)) != 0)
		return (res);

	switch(type) {
	case SYS_RES_IOPORT:
		found = 0;
		for (i = 0; i < MAX_RANGES_TUPLES; i++) {
			pci_base = sc->ranges[i].pci_base;
			phys_base = sc->ranges[i].phys_base;
			size = sc->ranges[i].size;

			if ((rid > pci_base) && (rid < (pci_base + size))) {
				found = 1;
				break;
			}
		}
		if (found) {
			rman_set_start(r, rman_get_start(r) + phys_base);
			BUS_ACTIVATE_RESOURCE(device_get_parent(dev), child,
						type, rid, r);
		} else {
			device_printf(dev, "Failed to activate IOPORT resource\n");
			res = 0;
		}
		break;
	case SYS_RES_MEMORY:
		BUS_ACTIVATE_RESOURCE(device_get_parent(dev), child, type, rid, r);
		break;
	default:
		break;
	}

	return (res);
}

static int
generic_pcie_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct generic_pcie_softc *sc;
	vm_offset_t vaddr;
	int res;

	sc = device_get_softc(dev);

	if ((res = rman_deactivate_resource(r)) != 0)
		return (res);

	switch(type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		vaddr = (vm_offset_t)rman_get_virtual(r);
		pmap_unmapdev(vaddr, rman_get_size(r));
		break;
	default:
		break;
	}

	return (res);
}

static device_method_t generic_pcie_methods[] = {
	DEVMETHOD(device_probe,			generic_pcie_probe),
	DEVMETHOD(device_attach,		generic_pcie_attach),
	DEVMETHOD(bus_read_ivar,		generic_pcie_read_ivar),
	DEVMETHOD(bus_write_ivar,		generic_pcie_write_ivar),
	DEVMETHOD(bus_alloc_resource,		generic_pcie_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		generic_pcie_adjust_resource),
	DEVMETHOD(bus_release_resource,		generic_pcie_release_resource),
	DEVMETHOD(bus_activate_resource,	generic_pcie_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	generic_pcie_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(pcib_maxslots,		generic_pcie_maxslots),
	DEVMETHOD(pcib_read_config,		generic_pcie_read_config),
	DEVMETHOD(pcib_write_config,		generic_pcie_write_config),
	DEVMETHOD_END
};

static driver_t generic_pcie_driver = {
	"pcib",
	generic_pcie_methods,
	sizeof(struct generic_pcie_softc),
};

static devclass_t generic_pcie_devclass;

DRIVER_MODULE(pcib, simplebus, generic_pcie_driver,
generic_pcie_devclass, 0, 0);
DRIVER_MODULE(pcib, ofwbus, generic_pcie_driver,
generic_pcie_devclass, 0, 0);
