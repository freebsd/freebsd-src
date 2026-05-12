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

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iommu/iommu.h>
#include <riscv/iommu/iommu_pmap.h>
#include <riscv/iommu/iommu.h>
#include <riscv/iommu/iommu_frontend.h>

#define	PCI_DEVICE_ID_REDHAT_RISCV_IOMMU	0x0014
#define	PCI_VENDOR_ID_REDHAT			0x1b36
#define PCI_DEVICE_ID_RIVOS_RISCV_IOMMU_GA	0x0008
#define	PCI_VENDOR_ID_RIVOS			0x1efd

static int
iommu_pci_probe(device_t dev)
{
	uint16_t vendor_id, device_id;

	vendor_id = pci_get_vendor(dev);
	device_id = pci_get_device(dev);

	if (vendor_id == PCI_VENDOR_ID_REDHAT &&
	    device_id == PCI_DEVICE_ID_REDHAT_RISCV_IOMMU) {
		device_set_desc(dev, "RedHat IOMMU");
		return (BUS_PROBE_DEFAULT);
	}

	if (vendor_id == PCI_VENDOR_ID_RIVOS &&
	    device_id == PCI_DEVICE_ID_RIVOS_RISCV_IOMMU_GA) {
		device_set_desc(dev, "Rivos IOMMU");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
iommu_pci_attach(device_t dev)
{
	struct riscv_iommu_unit *unit;
	struct riscv_iommu_softc *sc;
	struct iommu_unit *iommu;
	phandle_t node;
	int count;
	int error;
	int rid;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	pci_enable_busmaster(dev);

	rid = PCIR_BAR(0);
	sc->res[0] = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->res[0] == NULL) {
		device_printf(dev, "couldn't map memory\n");
		goto error;
	}

	count = 4;
	if (pci_alloc_msix(dev, &count) == 0) {
		for (i = 0; i < 4; i++) {
			rid = i;
			sc->res[1 + i] = bus_alloc_resource_any(dev,
			    SYS_RES_IRQ, &rid, RF_ACTIVE);
			if (sc->res[i + 1] == NULL) {
				device_printf(dev, "Can't allocate IRQ "
				    " resource.\n");
				goto error;
			}
		}
	} else
		device_printf(dev, "Can't allocate MSI-X interrupts."
		    " Ignoring.\n");

	error = riscv_iommu_attach(dev);
	if (error)
		goto error;

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
	if (sc->res[0])
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0),
		    sc->res[0]);

	for (i = 0; i < 4; i++)
		if (sc->res[i + 1])
			bus_release_resource(dev, SYS_RES_IRQ, i,
			    sc->res[i + 1]);

	return (error);
}

static device_method_t riscv_iommu_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		iommu_pci_probe),
	DEVMETHOD(device_attach,	iommu_pci_attach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(riscv_iommu, riscv_iommu_pci_driver,
    riscv_iommu_pci_methods, sizeof(struct riscv_iommu_softc),
    riscv_iommu_driver);
DRIVER_MODULE(riscv_iommu, pci, riscv_iommu_pci_driver, NULL, NULL);
