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

#include "opt_platform.h"

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
#include <dev/ofw/ofw_pci.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_host_generic.h>

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

#define	PCI_IO_WINDOW_OFFSET	0x1000

#define	SPACE_CODE_SHIFT	24
#define	SPACE_CODE_MASK		0x3
#define	SPACE_CODE_IO_SPACE	0x1
#define	PROPS_CELL_SIZE		1
#define	PCI_ADDR_CELL_SIZE	2

/* OFW bus interface */
struct generic_pcie_ofw_devinfo {
	struct ofw_bus_devinfo	di_dinfo;
	struct resource_list	di_rl;
};

/* Forward prototypes */

static int generic_pcie_probe(device_t dev);
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
static struct resource *generic_pcie_alloc_resource_ofw(device_t, device_t,
    int, int *, rman_res_t, rman_res_t, rman_res_t, u_int);
static struct resource *generic_pcie_alloc_resource_pcie(device_t dev,
    device_t child, int type, int *rid, rman_res_t start, rman_res_t end,
    rman_res_t count, u_int flags);
static int generic_pcie_release_resource(device_t dev, device_t child,
    int type, int rid, struct resource *res);
static int generic_pcie_release_resource_ofw(device_t, device_t, int, int,
    struct resource *);
static int generic_pcie_release_resource_pcie(device_t, device_t, int, int,
    struct resource *);
static int generic_pcie_ofw_bus_attach(device_t);
static const struct ofw_bus_devinfo *generic_pcie_ofw_get_devinfo(device_t,
    device_t);

static __inline void
get_addr_size_cells(phandle_t node, pcell_t *addr_cells, pcell_t *size_cells)
{

	*addr_cells = 2;
	/* Find address cells if present */
	OF_getencprop(node, "#address-cells", addr_cells, sizeof(*addr_cells));

	*size_cells = 2;
	/* Find size cells if present */
	OF_getencprop(node, "#size-cells", size_cells, sizeof(*size_cells));
}

static int
generic_pcie_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "pci-host-ecam-generic")) {
		device_set_desc(dev, "Generic PCI host controller");
		return (BUS_PROBE_GENERIC);
	}
	if (ofw_bus_is_compatible(dev, "arm,gem5_pcie")) {
		device_set_desc(dev, "GEM5 PCIe host controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

int
pci_host_generic_attach(device_t dev)
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

	/* Retrieve 'ranges' property from FDT */
	if (bootverbose)
		device_printf(dev, "parsing FDT for ECAM%d:\n",
		    sc->ecam);
	if (parse_pci_mem_ranges(sc))
		return (ENXIO);

	/* Attach OFW bus */
	if (generic_pcie_ofw_bus_attach(dev) != 0)
		return (ENXIO);

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
			   phys_base, phys_base + size - 1);
		} else if (sc->ranges[tuple].flags & FLAG_IO) {
			error = rman_manage_region(&sc->io_rman,
			   pci_base + PCI_IO_WINDOW_OFFSET,
			   pci_base + PCI_IO_WINDOW_OFFSET + size - 1);
		} else
			continue;
		if (error) {
			device_printf(dev, "rman_manage_region() failed."
						"error = %d\n", error);
			rman_fini(&sc->mem_rman);
			return (error);
		}
	}

	ofw_bus_setup_iinfo(ofw_bus_get_node(dev), &sc->pci_iinfo,
	    sizeof(cell_t));

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

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
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

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
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
generic_pcie_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct generic_pcie_softc *sc;
	struct ofw_pci_register reg;
	uint32_t pintr, mintr[2];
	phandle_t iparent;
	int intrcells;

	sc = device_get_softc(bus);
	pintr = pin;

	bzero(&reg, sizeof(reg));
	reg.phys_hi = (pci_get_bus(dev) << OFW_PCI_PHYS_HI_BUSSHIFT) |
	    (pci_get_slot(dev) << OFW_PCI_PHYS_HI_DEVICESHIFT) |
	    (pci_get_function(dev) << OFW_PCI_PHYS_HI_FUNCTIONSHIFT);

	intrcells = ofw_bus_lookup_imap(ofw_bus_get_node(dev),
	    &sc->pci_iinfo, &reg, sizeof(reg), &pintr, sizeof(pintr),
	    mintr, sizeof(mintr), &iparent);
	if (intrcells) {
		pintr = ofw_bus_map_intr(dev, iparent, intrcells, mintr);
		return (pintr);
	}

	device_printf(bus, "could not route pin %d for device %d.%d\n",
	    pin, pci_get_slot(dev), pci_get_function(dev));
	return (PCI_INVALID_IRQ);
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

	if (bootverbose)
		device_printf(dev, "ERROR: Unknown index %d.\n", index);
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
	default:
		break;
	}

	return (NULL);
}

static int
generic_pcie_release_resource_pcie(device_t dev, device_t child, int type,
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

static int
generic_pcie_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *res)
{

	/* For PCIe devices that do not have FDT nodes, use PCIB method */
	if ((int)ofw_bus_get_node(child) <= 0) {
		return (generic_pcie_release_resource_pcie(dev,
		    child, type, rid, res));
	}

	/* For other devices use OFW method */
	return (generic_pcie_release_resource_ofw(dev,
	    child, type, rid, res));
}

struct resource *
pci_host_generic_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{

	/* For PCIe devices that do not have FDT nodes, use PCIB method */
	if ((int)ofw_bus_get_node(child) <= 0)
		return (generic_pcie_alloc_resource_pcie(dev, child, type, rid,
		    start, end, count, flags));

	/* For other devices use OFW method */
	return (generic_pcie_alloc_resource_ofw(dev, child, type, rid,
	    start, end, count, flags));
}

static struct resource *
generic_pcie_alloc_resource_pcie(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
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
		    "rman_reserve_resource: start=%#jx, end=%#jx, count=%#jx\n",
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
	device_printf(dev, "%s FAIL: type=%d, rid=%d, "
	    "start=%016jx, end=%016jx, count=%016jx, flags=%x\n",
	    __func__, type, *rid, start, end, count, flags);

	return (NULL);
}

static int
generic_pcie_adjust_resource(device_t dev, device_t child, int type,
    struct resource *res, rman_res_t start, rman_res_t end)
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

static int
generic_pcie_alloc_msi(device_t pci, device_t child, int count, int maxcount,
    int *irqs)
{

#if defined(__aarch64__)
	return (arm_alloc_msi(pci, child, count, maxcount, irqs));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_release_msi(device_t pci, device_t child, int count, int *irqs)
{

#if defined(__aarch64__)
	return (arm_release_msi(pci, child, count, irqs));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_map_msi(device_t pci, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{

#if defined(__aarch64__)
	return (arm_map_msi(pci, child, irq, addr, data));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_alloc_msix(device_t pci, device_t child, int *irq)
{

#if defined(__aarch64__)
	return (arm_alloc_msix(pci, child, irq));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_release_msix(device_t pci, device_t child, int irq)
{

#if defined(__aarch64__)
	return (arm_release_msix(pci, child, irq));
#else
	return (ENXIO);
#endif
}

static device_method_t generic_pcie_methods[] = {
	DEVMETHOD(device_probe,			generic_pcie_probe),
	DEVMETHOD(device_attach,		pci_host_generic_attach),
	DEVMETHOD(bus_read_ivar,		generic_pcie_read_ivar),
	DEVMETHOD(bus_write_ivar,		generic_pcie_write_ivar),
	DEVMETHOD(bus_alloc_resource,		pci_host_generic_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		generic_pcie_adjust_resource),
	DEVMETHOD(bus_release_resource,		generic_pcie_release_resource),
	DEVMETHOD(bus_activate_resource,	generic_pcie_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	generic_pcie_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		generic_pcie_maxslots),
	DEVMETHOD(pcib_route_interrupt,		generic_pcie_route_interrupt),
	DEVMETHOD(pcib_read_config,		generic_pcie_read_config),
	DEVMETHOD(pcib_write_config,		generic_pcie_write_config),
	DEVMETHOD(pcib_alloc_msi,		generic_pcie_alloc_msi),
	DEVMETHOD(pcib_release_msi,		generic_pcie_release_msi),
	DEVMETHOD(pcib_alloc_msix,		generic_pcie_alloc_msix),
	DEVMETHOD(pcib_release_msix,		generic_pcie_release_msix),
	DEVMETHOD(pcib_map_msi,			generic_pcie_map_msi),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,		generic_pcie_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,		ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,		ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,		ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,		ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,		ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static const struct ofw_bus_devinfo *
generic_pcie_ofw_get_devinfo(device_t bus __unused, device_t child)
{
	struct generic_pcie_ofw_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_dinfo);
}

static struct resource *
generic_pcie_alloc_resource_ofw(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct generic_pcie_softc *sc;
	struct generic_pcie_ofw_devinfo *di;
	struct resource_list_entry *rle;
	int i;

	sc = device_get_softc(bus);

	if (RMAN_IS_DEFAULT_RANGE(start, end)) {
		if ((di = device_get_ivars(child)) == NULL)
			return (NULL);
		if (type == SYS_RES_IOPORT)
		    type = SYS_RES_MEMORY;

		/* Find defaults for this rid */
		rle = resource_list_find(&di->di_rl, type, *rid);
		if (rle == NULL)
			return (NULL);

		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	if (type == SYS_RES_MEMORY) {
		/* Remap through ranges property */
		for (i = 0; i < MAX_RANGES_TUPLES; i++) {
			if (start >= sc->ranges[i].phys_base && end <
			    sc->ranges[i].pci_base + sc->ranges[i].size) {
				start -= sc->ranges[i].phys_base;
				start += sc->ranges[i].pci_base;
				end -= sc->ranges[i].phys_base;
				end += sc->ranges[i].pci_base;
				break;
			}
		}

		if (i == MAX_RANGES_TUPLES) {
			device_printf(bus, "Could not map resource "
			    "%#jx-%#jx\n", start, end);
			return (NULL);
		}
	}

	return (bus_generic_alloc_resource(bus, child, type, rid, start, end,
	    count, flags));
}

static int
generic_pcie_release_resource_ofw(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	return (bus_generic_release_resource(bus, child, type, rid, res));
}

/* Helper functions */

static int
generic_pcie_ofw_bus_attach(device_t dev)
{
	struct generic_pcie_ofw_devinfo *di;
	device_t child;
	phandle_t parent, node;
	pcell_t addr_cells, size_cells;

	parent = ofw_bus_get_node(dev);
	if (parent > 0) {
		get_addr_size_cells(parent, &addr_cells, &size_cells);
		/* Iterate through all bus subordinates */
		for (node = OF_child(parent); node > 0; node = OF_peer(node)) {

			/* Allocate and populate devinfo. */
			di = malloc(sizeof(*di), M_DEVBUF, M_WAITOK | M_ZERO);
			if (ofw_bus_gen_setup_devinfo(&di->di_dinfo, node) != 0) {
				free(di, M_DEVBUF);
				continue;
			}

			/* Initialize and populate resource list. */
			resource_list_init(&di->di_rl);
			ofw_bus_reg_to_rl(dev, node, addr_cells, size_cells,
			    &di->di_rl);
			ofw_bus_intr_to_rl(dev, node, &di->di_rl, NULL);

			/* Add newbus device for this FDT node */
			child = device_add_child(dev, NULL, -1);
			if (child == NULL) {
				resource_list_free(&di->di_rl);
				ofw_bus_gen_destroy_devinfo(&di->di_dinfo);
				free(di, M_DEVBUF);
				continue;
			}

			device_set_ivars(child, di);
		}
	}

	return (0);
}

DEFINE_CLASS_0(pcib, generic_pcie_driver,
    generic_pcie_methods, sizeof(struct generic_pcie_softc));

devclass_t generic_pcie_devclass;

DRIVER_MODULE(pcib, simplebus, generic_pcie_driver,
    generic_pcie_devclass, 0, 0);
DRIVER_MODULE(pcib, ofwbus, generic_pcie_driver,
    generic_pcie_devclass, 0, 0);

