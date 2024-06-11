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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_host_generic.h>
#include <dev/pci/pci_host_generic_fdt.h>
#include <dev/pci/pcib_private.h>

#include "thunder_pcie_common.h"

#include "pcib_if.h"

#ifdef THUNDERX_PASS_1_1_ERRATA
static struct resource * thunder_pcie_fdt_alloc_resource(device_t, device_t,
    int, int *, rman_res_t, rman_res_t, rman_res_t, u_int);
static int thunder_pcie_fdt_release_resource(device_t, device_t,
    struct resource*);
#endif
static int thunder_pcie_fdt_attach(device_t);
static int thunder_pcie_fdt_probe(device_t);
static int thunder_pcie_fdt_get_id(device_t, device_t, enum pci_id_type,
    uintptr_t *);

static const struct ofw_bus_devinfo *thunder_pcie_ofw_get_devinfo(device_t,
    device_t);

/* OFW bus interface */
struct thunder_pcie_ofw_devinfo {
	struct ofw_bus_devinfo	di_dinfo;
	struct resource_list	di_rl;
};

static device_method_t thunder_pcie_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		thunder_pcie_fdt_probe),
	DEVMETHOD(device_attach,	thunder_pcie_fdt_attach),
#ifdef THUNDERX_PASS_1_1_ERRATA
	DEVMETHOD(bus_alloc_resource,	thunder_pcie_fdt_alloc_resource),
	DEVMETHOD(bus_release_resource, thunder_pcie_fdt_release_resource),
#endif

	/* pcib interface */
	DEVMETHOD(pcib_get_id,		thunder_pcie_fdt_get_id),

	/* ofw interface */
	DEVMETHOD(ofw_bus_get_devinfo,	thunder_pcie_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, thunder_pcie_fdt_driver, thunder_pcie_fdt_methods,
    sizeof(struct generic_pcie_fdt_softc), generic_pcie_fdt_driver);

DRIVER_MODULE(thunder_pcib, simplebus, thunder_pcie_fdt_driver, 0, 0);
DRIVER_MODULE(thunder_pcib, ofwbus, thunder_pcie_fdt_driver, 0, 0);

static const struct ofw_bus_devinfo *
thunder_pcie_ofw_get_devinfo(device_t bus __unused, device_t child)
{
	struct thunder_pcie_ofw_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_dinfo);
}

static void
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
thunder_pcie_ofw_bus_attach(device_t dev)
{
	struct thunder_pcie_ofw_devinfo *di;
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

static int
thunder_pcie_fdt_probe(device_t dev)
{

	/* Check if we're running on Cavium ThunderX */
	if (!CPU_MATCH(CPU_IMPL_MASK | CPU_PART_MASK,
	    CPU_IMPL_CAVIUM, CPU_PART_THUNDERX, 0, 0))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "pci-host-ecam-generic") ||
	    ofw_bus_is_compatible(dev, "cavium,thunder-pcie") ||
	    ofw_bus_is_compatible(dev, "cavium,pci-host-thunder-ecam")) {
		device_set_desc(dev, "Cavium Integrated PCI/PCI-E Controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
thunder_pcie_fdt_attach(device_t dev)
{
	struct generic_pcie_fdt_softc *sc;

	sc = device_get_softc(dev);
	thunder_pcie_identify_ecam(dev, &sc->base.ecam);
	sc->base.coherent = 1;

	/* Attach OFW bus */
	if (thunder_pcie_ofw_bus_attach(dev) != 0)
		return (ENXIO);

	return (pci_host_generic_fdt_attach(dev));
}

static int
thunder_pcie_fdt_get_id(device_t pci, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	phandle_t node;
	int bsf;

	if (type != PCI_ID_MSI)
		return (pcib_get_id(pci, child, type, id));

	node = ofw_bus_get_node(pci);
	if (OF_hasprop(node, "msi-map"))
		return (generic_pcie_get_id(pci, child, type, id));

	bsf = pci_get_rid(child);
	*id = (pci_get_domain(child) << PCI_RID_DOMAIN_SHIFT) | bsf;

	return (0);
}

#ifdef THUNDERX_PASS_1_1_ERRATA
struct resource *
thunder_pcie_fdt_alloc_resource(device_t dev, device_t child, int type,
    int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct generic_pcie_fdt_softc *sc;
	struct thunder_pcie_ofw_devinfo *di;
	struct resource_list_entry *rle;
	int i;

	/*
	 * For PCIe devices that do not have FDT nodes pass
	 * the request to the core driver.
	 */
	if ((int)ofw_bus_get_node(child) <= 0)
		return (thunder_pcie_alloc_resource(dev, child, type,
		    rid, start, end, count, flags));

	/* For other devices use OFW method */
	sc = device_get_softc(dev);

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
			if (start >= sc->base.ranges[i].phys_base &&
			    end < (sc->base.ranges[i].pci_base +
			    sc->base.ranges[i].size)) {
				start -= sc->base.ranges[i].phys_base;
				start += sc->base.ranges[i].pci_base;
				end -= sc->base.ranges[i].phys_base;
				end += sc->base.ranges[i].pci_base;
				break;
			}
		}

		if (i == MAX_RANGES_TUPLES) {
			device_printf(dev, "Could not map resource "
			    "%#jx-%#jx\n", start, end);
			return (NULL);
		}
	}

	return (bus_generic_alloc_resource(dev, child, type, rid, start,
	    end, count, flags));
}

static int
thunder_pcie_fdt_release_resource(device_t dev, device_t child,
    struct resource *res)
{

	if ((int)ofw_bus_get_node(child) <= 0)
		return (pci_host_generic_core_release_resource(dev, child,
		    res));

	return (bus_generic_release_resource(dev, child, res));
}
#endif
