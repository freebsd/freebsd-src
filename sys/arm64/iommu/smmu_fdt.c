/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/bitstring.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/taskqueue.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/iommu/iommu.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iommu/iommu.h>

#include <arm64/iommu/iommu.h>

#include "smmuvar.h"

static struct ofw_compat_data compat_data[] = {
	{ "arm,smmu-v3",			1 },
	{ NULL,					0 }
};

static int
smmu_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "ARM System MMU (SMMU) v3");

	return (BUS_PROBE_DEFAULT);
}

static int
smmu_fdt_attach(device_t dev)
{
	struct smmu_softc *sc;
	struct smmu_unit *unit;
	struct iommu_unit *iommu;
	phandle_t node;
	int err;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	rid = 0;
	sc->res[0] = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->res[0] == NULL) {
		device_printf(dev, "Can't allocate memory resource.\n");
		err = ENXIO;
		goto error;
	}

	/*
	 * Interrupt lines are "eventq", "priq", "cmdq-sync", "gerror".
	 */

	err = ofw_bus_find_string_index(node, "interrupt-names", "eventq",
	    &rid);
	if (err != 0) {
		device_printf(dev, "Can't get eventq IRQ.\n");
		err = ENXIO;
		goto error;
	}

	sc->res[1] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->res[1] == NULL) {
		device_printf(dev, "Can't allocate eventq IRQ resource.\n");
		err = ENXIO;
		goto error;
	}

	/*
	 * sc->res[2] is reserved for priq IRQ. It is optional and not used
	 * by the SMMU driver. This IRQ line may or may not be provided by
	 * hardware.
	 */

	err = ofw_bus_find_string_index(node, "interrupt-names", "cmdq-sync",
	    &rid);
	if (err != 0) {
		device_printf(dev, "Can't get cmdq-sync IRQ.\n");
		err = ENXIO;
		goto error;
	}

	sc->res[3] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->res[3] == NULL) {
		device_printf(dev, "Can't allocate cmdq-sync IRQ resource.\n");
		err = ENXIO;
		goto error;
	}

	err = ofw_bus_find_string_index(node, "interrupt-names", "gerror",
	    &rid);
	if (err != 0) {
		device_printf(dev, "Can't get gerror IRQ.\n");
		err = ENXIO;
		goto error;
	}

	sc->res[4] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->res[4] == NULL) {
		device_printf(dev, "Can't allocate gerror IRQ resource.\n");
		err = ENXIO;
		goto error;
	}

	err = smmu_attach(dev);
	if (err != 0)
		goto error;

	unit = &sc->unit;
	unit->dev = dev;

	iommu = &unit->iommu;
	iommu->dev = dev;

	LIST_INIT(&unit->domain_list);

	/* Use memory start address as an xref. */
	sc->xref = OF_xref_from_node(node);

	err = iommu_register(iommu);
	if (err) {
		device_printf(dev, "Failed to register SMMU.\n");
		return (ENXIO);
	}

	OF_device_register_xref(sc->xref, dev);

	return (0);

error:
	if (bootverbose) {
		device_printf(dev,
		    "Failed to attach. Error %d\n", err);
	}

	/* Failure so free resources. */
	smmu_detach(dev);

	return (err);
}

static device_method_t smmu_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		smmu_fdt_probe),
	DEVMETHOD(device_attach,	smmu_fdt_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(smmu, smmu_fdt_driver, smmu_fdt_methods,
    sizeof(struct smmu_softc), smmu_driver);

EARLY_DRIVER_MODULE(smmu, simplebus, smmu_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
