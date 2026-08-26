/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iommu/iommu.h>
#include <riscv/iommu/iommu_pmap.h>
#include <riscv/iommu/iommu.h>
#include <riscv/iommu/iommu_frontend.h>

static struct ofw_compat_data compat_data[] = {
	{ "riscv,iommu",	1 },
	{ NULL,			0 }
};

static struct resource_spec riscv_iommu_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },	/* CQ */
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },	/* FQ */
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },	/* PM */
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },	/* PQ */
	RESOURCE_SPEC_END
};

static int
riscv_iommu_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RISC-V IOMMU");

	return (BUS_PROBE_DEFAULT);
}

static int
riscv_iommu_fdt_attach(device_t dev)
{
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_unit *unit;
	struct iommu_unit *iommu;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	error = bus_alloc_resources(dev, riscv_iommu_spec, sc->res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		goto error;
	}

	error = riscv_iommu_attach(dev);
	if (error != 0) {
		device_printf(dev, "Failed to attach. Error %d\n", error);
		goto error;
	}

	unit = &sc->unit;
	unit->dev = dev;

	iommu = &unit->iommu;
	iommu->dev = dev;

	LIST_INIT(&unit->domain_list);

	sc->xref = OF_xref_from_node(node);

	error = iommu_register(iommu);
	if (error) {
		device_printf(dev, "Failed to register RISC-V IOMMU.\n");
		goto error;
	}

	return (0);

error:
	bus_release_resources(dev, riscv_iommu_spec, sc->res);

	return (error);
}

static device_method_t riscv_iommu_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		riscv_iommu_fdt_probe),
	DEVMETHOD(device_attach,	riscv_iommu_fdt_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(riscv_iommu, riscv_iommu_fdt_driver, riscv_iommu_fdt_methods,
    sizeof(struct riscv_iommu_softc), riscv_iommu_driver);
EARLY_DRIVER_MODULE(riscv_iommu, simplebus, riscv_iommu_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
