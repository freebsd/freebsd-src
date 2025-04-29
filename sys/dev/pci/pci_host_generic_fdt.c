/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2014,2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * the sponsorship of the FreeBSD Foundation.
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

/* Generic ECAM PCIe driver FDT attachment */

#include <sys/cdefs.h>
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#if defined(INTRNG)
#include <machine/intr.h>
#endif

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_host_generic.h>
#include <dev/pci/pci_host_generic_fdt.h>

#include <machine/intr.h>

#include "pcib_if.h"

#define	SPACE_CODE_SHIFT	24
#define	SPACE_CODE_MASK		0x3
#define	SPACE_CODE_IO_SPACE	0x1
#define	PROPS_CELL_SIZE		1
#define	PCI_ADDR_CELL_SIZE	2

struct pci_ofw_devinfo {
	STAILQ_ENTRY(pci_ofw_devinfo) pci_ofw_link;
	struct ofw_bus_devinfo  di_dinfo;
	uint8_t slot;
	uint8_t func;
	uint8_t bus;
};

/* Forward prototypes */

static int generic_pcie_fdt_probe(device_t dev);
static int parse_pci_mem_ranges(device_t, struct generic_pcie_core_softc *);
static int generic_pcie_ofw_bus_attach(device_t);
static const struct ofw_bus_devinfo *generic_pcie_ofw_get_devinfo(device_t,
    device_t);

static int
generic_pcie_fdt_probe(device_t dev)
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
pci_host_generic_setup_fdt(device_t dev)
{
	struct generic_pcie_fdt_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);

	STAILQ_INIT(&sc->pci_ofw_devlist);

	/* Retrieve 'ranges' property from FDT */
	if (bootverbose)
		device_printf(dev, "parsing FDT for ECAM%d:\n", sc->base.ecam);
	if (parse_pci_mem_ranges(dev, &sc->base))
		return (ENXIO);

	/* Attach OFW bus */
	if (generic_pcie_ofw_bus_attach(dev) != 0)
		return (ENXIO);

	node = ofw_bus_get_node(dev);
	if (sc->base.coherent == 0) {
		sc->base.coherent = OF_hasprop(node, "dma-coherent");
	}
	if (bootverbose)
		device_printf(dev, "Bus is%s cache-coherent\n",
		    sc->base.coherent ? "" : " not");

	/* TODO parse FDT bus ranges */
	sc->base.bus_start = 0;
	sc->base.bus_end = 0xFF;
	
	/*
	 * ofw_pcib uses device unit as PCI domain number.
	 * Do the same. Some boards have multiple RCs handled
	 * by different drivers, this ensures that there are
	 * no collisions.
	 */
	sc->base.ecam = device_get_unit(dev);

	error = pci_host_generic_core_attach(dev);
	if (error != 0)
		return (error);

	if (ofw_bus_is_compatible(dev, "marvell,armada8k-pcie-ecam") ||
	    ofw_bus_is_compatible(dev, "socionext,synquacer-pcie-ecam") ||
	    ofw_bus_is_compatible(dev, "snps,dw-pcie-ecam")) {
		device_set_desc(dev, "Synopsys DesignWare PCIe Controller");
		sc->base.quirks |= PCIE_ECAM_DESIGNWARE_QUIRK;
	}

	ofw_bus_setup_iinfo(node, &sc->pci_iinfo, sizeof(cell_t));

	return (0);
}

int
pci_host_generic_fdt_attach(device_t dev)
{
	int error;

	error = pci_host_generic_setup_fdt(dev);
	if (error != 0)
		return (error);

	device_add_child(dev, "pci", DEVICE_UNIT_ANY);
	bus_attach_children(dev);
	return (0);
}

static int
parse_pci_mem_ranges(device_t dev, struct generic_pcie_core_softc *sc)
{
	pcell_t pci_addr_cells, parent_addr_cells;
	pcell_t attributes, size_cells;
	cell_t *base_ranges;
	int nbase_ranges;
	phandle_t node;
	int i, j, k;

	node = ofw_bus_get_node(dev);

	OF_getencprop(node, "#address-cells", &pci_addr_cells,
					sizeof(pci_addr_cells));
	OF_getencprop(node, "#size-cells", &size_cells,
					sizeof(size_cells));
	OF_getencprop(OF_parent(node), "#address-cells", &parent_addr_cells,
					sizeof(parent_addr_cells));

	if (parent_addr_cells > 2 || pci_addr_cells != 3 || size_cells > 2) {
		device_printf(dev,
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
			sc->ranges[i].flags |= FLAG_TYPE_IO;
		} else {
			sc->ranges[i].flags |= FLAG_TYPE_MEM;
		}

		sc->ranges[i].rid = -1;
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

	free(base_ranges, M_DEVBUF);
	return (0);
}

static int
generic_pcie_fdt_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct generic_pcie_fdt_softc *sc;
	struct ofw_pci_register reg;
	uint32_t pintr, mintr[4];
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
generic_pcie_fdt_alloc_msi(device_t pci, device_t child, int count,
    int maxcount, int *irqs)
{
#if defined(INTRNG)
	phandle_t msi_parent;
	int err;

	err = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (err != 0)
		return (err);
	return (intr_alloc_msi(pci, child, msi_parent, count, maxcount,
	    irqs));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_fdt_release_msi(device_t pci, device_t child, int count, int *irqs)
{
#if defined(INTRNG)
	phandle_t msi_parent;
	int err;

	err = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (err != 0)
		return (err);
	return (intr_release_msi(pci, child, msi_parent, count, irqs));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_fdt_map_msi(device_t pci, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
#if defined(INTRNG)
	phandle_t msi_parent;
	int err;

	err = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (err != 0)
		return (err);
	return (intr_map_msi(pci, child, msi_parent, irq, addr, data));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_fdt_alloc_msix(device_t pci, device_t child, int *irq)
{
#if defined(INTRNG)
	phandle_t msi_parent;
	int err;

	err = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (err != 0)
		return (err);
	return (intr_alloc_msix(pci, child, msi_parent, irq));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_fdt_release_msix(device_t pci, device_t child, int irq)
{
#if defined(INTRNG)
	phandle_t msi_parent;
	int err;

	err = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (err != 0)
		return (err);
	return (intr_release_msix(pci, child, msi_parent, irq));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_get_iommu(device_t pci, device_t child, uintptr_t *id)
{
	struct pci_id_ofw_iommu *iommu;
	uint32_t iommu_rid;
	phandle_t iommu_xref;
	uint16_t pci_rid;
	phandle_t node;
	int err;

	node = ofw_bus_get_node(pci);
	pci_rid = pci_get_rid(child);

	iommu = (struct pci_id_ofw_iommu *)id;

	err = ofw_bus_iommu_map(node, pci_rid, &iommu_xref, &iommu_rid);
	if (err == 0) {
		iommu->id = iommu_rid;
		iommu->xref = iommu_xref;
	}

	return (err);
}

int
generic_pcie_get_id(device_t pci, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	phandle_t node;
	int err;
	uint32_t rid;
	uint16_t pci_rid;

	if (type == PCI_ID_OFW_IOMMU)
		return (generic_pcie_get_iommu(pci, child, id));

	if (type != PCI_ID_MSI)
		return (pcib_get_id(pci, child, type, id));

	node = ofw_bus_get_node(pci);
	pci_rid = pci_get_rid(child);

	err = ofw_bus_msimap(node, pci_rid, NULL, &rid);
	if (err != 0)
		return (err);
	*id = rid;

	return (0);
}

static const struct ofw_bus_devinfo *
generic_pcie_ofw_get_devinfo(device_t bus, device_t child)
{
	struct generic_pcie_fdt_softc *sc;
	struct pci_ofw_devinfo *di;
	uint8_t slot, func, busno;

	sc = device_get_softc(bus);
	slot = pci_get_slot(child);
	func = pci_get_function(child);
	busno = pci_get_bus(child);

	STAILQ_FOREACH(di, &sc->pci_ofw_devlist, pci_ofw_link)
		if (slot == di->slot && func == di->func && busno == di->bus)
			return (&di->di_dinfo);

	return (NULL);
}

/* Helper functions */

static int
generic_pcie_ofw_bus_attach(device_t dev)
{
	struct generic_pcie_fdt_softc *sc;
	struct pci_ofw_devinfo *di;
	phandle_t parent, node;
	pcell_t reg[5];
	ssize_t len;

	sc = device_get_softc(dev);
	parent = ofw_bus_get_node(dev);
	if (parent == 0)
		return (0);

	/* Iterate through all bus subordinates */
	for (node = OF_child(parent); node > 0; node = OF_peer(node)) {
		len = OF_getencprop(node, "reg", reg, sizeof(reg));
		if (len != 5 * sizeof(pcell_t))
			continue;

		/* Allocate and populate devinfo. */
		di = malloc(sizeof(*di), M_DEVBUF, M_WAITOK | M_ZERO);
		if (ofw_bus_gen_setup_devinfo(&di->di_dinfo, node) != 0) {
			free(di, M_DEVBUF);
			continue;
		}
		di->func = OFW_PCI_PHYS_HI_FUNCTION(reg[0]);
		di->slot = OFW_PCI_PHYS_HI_DEVICE(reg[0]);
		di->bus = OFW_PCI_PHYS_HI_BUS(reg[0]);
		STAILQ_INSERT_TAIL(&sc->pci_ofw_devlist, di, pci_ofw_link);
	}

	return (0);
}

static device_method_t generic_pcie_fdt_methods[] = {
	DEVMETHOD(device_probe,		generic_pcie_fdt_probe),
	DEVMETHOD(device_attach,	pci_host_generic_fdt_attach),

	/* pcib interface */
	DEVMETHOD(pcib_route_interrupt,	generic_pcie_fdt_route_interrupt),
	DEVMETHOD(pcib_alloc_msi,	generic_pcie_fdt_alloc_msi),
	DEVMETHOD(pcib_release_msi,	generic_pcie_fdt_release_msi),
	DEVMETHOD(pcib_alloc_msix,	generic_pcie_fdt_alloc_msix),
	DEVMETHOD(pcib_release_msix,	generic_pcie_fdt_release_msix),
	DEVMETHOD(pcib_map_msi,		generic_pcie_fdt_map_msi),
	DEVMETHOD(pcib_get_id,		generic_pcie_get_id),
	DEVMETHOD(pcib_request_feature,	pcib_request_feature_allow),

	DEVMETHOD(ofw_bus_get_devinfo,	generic_pcie_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, generic_pcie_fdt_driver, generic_pcie_fdt_methods,
    sizeof(struct generic_pcie_fdt_softc), generic_pcie_core_driver);

DRIVER_MODULE(pcib, simplebus, generic_pcie_fdt_driver, 0, 0);
DRIVER_MODULE(pcib, ofwbus, generic_pcie_fdt_driver, 0, 0);
