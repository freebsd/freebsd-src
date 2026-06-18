/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * Copyright (c) 2025 Arm Ltd
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <arm/arm/gic_common.h>
#include "gicv5var.h"

#include "pci_if.h"

struct gicv5_fdt_devinfo {
	struct gicv5_devinfo	di_base;
	struct ofw_bus_devinfo	di_dinfo;
};

static device_probe_t gicv5_fdt_probe;
static device_probe_t gicv5_fdt_attach;

static ofw_bus_get_devinfo_t gicv5_fdt_get_devinfo;

static device_method_t gicv5_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gicv5_fdt_probe),
	DEVMETHOD(device_attach,	gicv5_fdt_attach),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	gicv5_fdt_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(gic, gicv5_fdt_driver, gicv5_fdt_methods,
    sizeof(struct gicv5_softc), gicv5_driver);

EARLY_DRIVER_MODULE(gicv5, simplebus, gicv5_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

static int
gicv5_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "arm,gic-v5"))
		return (ENXIO);

	device_set_desc(dev, "ARM Generic Interrupt Controller v5");
	return (BUS_PROBE_DEFAULT);
}

static void
gicv5_fdt_add_child(device_t dev, phandle_t node, struct gicv5_irs *irs,
    pcell_t addr_cells, pcell_t size_cells)
{
	struct gicv5_fdt_devinfo *di;

	di = malloc(sizeof(*di), M_DEVBUF, M_WAITOK | M_ZERO);
	di->di_base.di_irs = irs;
	if (ofw_bus_gen_setup_devinfo(&di->di_dinfo, node)) {
		if (bootverbose) {
			device_printf(dev,
			    "Could not set up devinfo\n");
		}
		free(di, M_DEVBUF);
		return;
	}

	/* Initialize and populate resource list. */
	resource_list_init(&di->di_base.di_rl);
	ofw_bus_reg_to_rl(dev, node, addr_cells, size_cells,
	    &di->di_base.di_rl);

	if (!gicv5_add_child(dev, &di->di_base)) {
		if (bootverbose) {
			device_printf(dev,
			    "Could not add child: %s\n",
			    di->di_dinfo.obd_name);
		}
		resource_list_free(&di->di_base.di_rl);
		ofw_bus_gen_destroy_devinfo(&di->di_dinfo);
		free(di, M_DEVBUF);
	}
}

static void
gicv5_fdt_add_irs_children(device_t dev, struct gicv5_irs *irs,
    phandle_t node, pcell_t addr_cells, pcell_t size_cells)
{
	phandle_t child;

	for (child = OF_child(node); child != 0; child = OF_peer(child))
		gicv5_fdt_add_child(dev, child, irs, addr_cells, size_cells);
}

static void
gicv5_fdt_add_children(device_t dev)
{
	char compat[16];
	struct gicv5_softc *sc;
	phandle_t node;
	pcell_t addr_cells, size_cells;
	phandle_t child;
	u_int irs_idx;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if (OF_getencprop(node, "#address-cells", &addr_cells,
	    sizeof(addr_cells)) == -1) {
		device_printf(dev, "Unable to read #address-cells\n");
		return;
	}
	if (OF_getencprop(node, "#size-cells", &size_cells,
	    sizeof(size_cells)) == -1) {
		device_printf(dev, "Unable to read #size-cells\n");
		return;
	}

	irs_idx = 0;
	/* Find children nodes to attach devices to */
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "compatible", compat, sizeof(compat)) < 0)
			continue;

		/* Don't connect to the IRS, but connect to its children */
		if (strcmp(compat, "arm,gic-v5-irs") == 0) {
			MPASS(irs_idx < sc->gic_nirs);
			gicv5_fdt_add_irs_children(dev, sc->gic_irs[irs_idx],
			    child, addr_cells, size_cells);
			irs_idx++;
			continue;
		}

		gicv5_fdt_add_child(dev, child, NULL, addr_cells, size_cells);
	}
}

static int
gicv5_fdt_attach(device_t dev)
{
	char compat[16];
	struct gicv5_softc *sc;
	ssize_t rv;
	phandle_t child, node;
	bus_addr_t paddr;
	bus_size_t size;
	intptr_t xref;
	int error, i;

	sc = device_get_softc(dev);
	sc->gic_bus = GIC_BUS_FDT;

	node = ofw_bus_get_node(dev);
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "compatible", compat, sizeof(compat)) < 0)
			continue;

		if (strcmp(compat, "arm,gic-v5-irs") != 0)
			continue;

		sc->gic_nirs++;
	}
	if (sc->gic_nirs == 0)
		panic("%s: Invalid configuration, no IRS defined",
		    device_get_nameunit(dev));
	sc->gic_irs = mallocarray(sc->gic_nirs, sizeof(sc->gic_irs[0]),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	i = 0;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		cpuset_t cpuset;
		pcell_t *cpus;

		if (OF_getprop(child, "compatible", compat, sizeof(compat)) < 0)
			continue;

		if (strcmp(compat, "arm,gic-v5-irs") != 0)
			continue;

		/* TODO: What to do with a standalone IRS? */
		rv = OF_getencprop_alloc(child, "cpus", (void **)&cpus);
		if (rv < 0)
			continue;

		CPU_ZERO(&cpuset);
		for (u_int c = 0; c < rv / sizeof(pcell_t); c++) {
			struct pcpu *pcpu;
			device_t cpu;
			uintptr_t v;

			cpu = OF_device_from_xref(cpus[c]);
			if (cpu == NULL) {
				device_printf(dev,
				    "Unable to find device for CPU %u\n", c);
				continue;
			}

			/*
			 * We can't use cpu_get_pcpu here as it expect the
			 * passed in device to be a direct child of the cpu
			 * device.
			 */
			if (BUS_READ_IVAR(cpu, dev, CPU_IVAR_PCPU, &v) != 0 ||
			    v == 0) {
				/*
				 * This can happen when not all CPUs in the
				 * dtb are enabled, e.g. in the Arm FVP.
				 */
				continue;
			}
			pcpu = (struct pcpu *)v;

			CPU_SET(pcpu->pc_cpuid, &cpuset);
		}

		OF_prop_free(cpus);

		gicv5_irs_init(dev, i, &cpuset);
		error = ofw_reg_to_paddr(child, 0, &paddr, &size, NULL);
		if (error != 0)
			panic("%s: Invalid configuration, no physical address "
			    "or size found for child irs %u",
			    device_get_nameunit(dev), i);

		error = bus_set_resource(dev, SYS_RES_MEMORY, i, paddr, size);
		if (error != 0)
			panic("%s: Unable to set memory resource",
			    device_get_nameunit(dev));
		i++;
	}

	gicv5_attach(dev);

	xref = OF_xref_from_node(node);
	sc->gic_pic = intr_pic_register(dev, xref);
	if (sc->gic_pic == NULL)
		panic("%s: Unable to register PIC", device_get_nameunit(dev));

	/* Register xref */
	OF_device_register_xref(xref, dev);
	intr_ipi_pic_register(dev, 0);

	error = intr_pic_claim_root(dev, xref, gicv5_intr, sc, INTR_ROOT_IRQ);
	if (error != 0)
		panic("%s: Unable to claim PIC root", device_get_nameunit(dev));

	gicv5_fdt_add_children(dev);
	bus_attach_children(dev);

	return (0);
}

static const struct ofw_bus_devinfo *
gicv5_fdt_get_devinfo(device_t dev __unused, device_t child)
{
	struct gicv5_fdt_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_dinfo);
}
