/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include "opt_acpi.h"

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/bitstring.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/tree.h>
#include <sys/taskqueue.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/iommu/iommu.h>

#include <arm64/iommu/iommu.h>

#include "smmuvar.h"

#define	MEMORY_RESOURCE_SIZE	0x20000
#define	MAX_SMMU		8

struct smmu_acpi_devinfo {
	struct resource_list	di_rl;
};

struct iort_table_data {
	device_t parent;
	device_t dev;
	ACPI_IORT_SMMU_V3 *smmu[MAX_SMMU];
	int count;
};

static void
iort_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	struct iort_table_data *iort_data;
	ACPI_IORT_NODE *node;
	int i;

	iort_data = (struct iort_table_data *)arg;
	i = iort_data->count;

	switch(entry->Type) {
	case ACPI_IORT_NODE_SMMU_V3:
		if (i == MAX_SMMU) {
			printf("SMMUv3 found, but no space available.\n");
			break;
		}

		if (iort_data->smmu[i] != NULL) {
			if (bootverbose)
				device_printf(iort_data->parent,
				    "smmu: Already have an SMMU table");
			break;
		}
		node = (ACPI_IORT_NODE *)entry;
		iort_data->smmu[i] = (ACPI_IORT_SMMU_V3 *)node->NodeData;
		iort_data->count++;
		break;
	default:
		break;
	}
}

static void
smmu_acpi_identify(driver_t *driver, device_t parent)
{
	struct iort_table_data iort_data;
	ACPI_TABLE_IORT *iort;
	vm_paddr_t iort_pa;
	uintptr_t priv;
	device_t dev;
	int i;

	iort_pa = acpi_find_table(ACPI_SIG_IORT);
	if (iort_pa == 0)
		return;

	iort = acpi_map_table(iort_pa, ACPI_SIG_IORT);
	if (iort == NULL) {
		device_printf(parent, "smmu: Unable to map the IORT\n");
		return;
	}

	iort_data.parent = parent;
	for (i = 0; i < MAX_SMMU; i++)
		iort_data.smmu[i] = NULL;
	iort_data.count = 0;

	acpi_walk_subtables(iort + 1, (char *)iort + iort->Header.Length,
	    iort_handler, &iort_data);
	if (iort_data.count == 0) {
		device_printf(parent, "No SMMU found.\n");
		goto out;
	}

	for (i = 0; i < iort_data.count; i++) {
		dev = BUS_ADD_CHILD(parent,
		    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE, "smmu", -1);
		if (dev == NULL) {
			device_printf(parent, "add smmu child failed\n");
			goto out;
		}

		/* Add the IORT data */
		BUS_SET_RESOURCE(parent, dev, SYS_RES_IRQ, 0,
		    iort_data.smmu[i]->EventGsiv, 1);
		BUS_SET_RESOURCE(parent, dev, SYS_RES_IRQ, 1,
		    iort_data.smmu[i]->PriGsiv, 1);
		BUS_SET_RESOURCE(parent, dev, SYS_RES_IRQ, 2,
		    iort_data.smmu[i]->SyncGsiv, 1);
		BUS_SET_RESOURCE(parent, dev, SYS_RES_IRQ, 3,
		    iort_data.smmu[i]->GerrGsiv, 1);
		BUS_SET_RESOURCE(parent, dev, SYS_RES_MEMORY, 0,
		    iort_data.smmu[i]->BaseAddress, MEMORY_RESOURCE_SIZE);

		priv = iort_data.smmu[i]->Flags;
		priv <<= 32;
		priv |= iort_data.smmu[i]->Model;

		acpi_set_private(dev, (void *)priv);
	}

	iort_data.dev = dev;

out:
	acpi_unmap_table(iort);
}

static int
smmu_acpi_probe(device_t dev)
{

	switch((uintptr_t)acpi_get_private(dev) & 0xffffffff) {
	case ACPI_IORT_SMMU_V3_GENERIC:
		/* Generic SMMUv3 */
		break;
	default:
		return (ENXIO);
	}

	device_set_desc(dev, SMMU_DEVSTR);

	return (BUS_PROBE_NOWILDCARD);
}

static int
smmu_acpi_attach(device_t dev)
{
	struct smmu_softc *sc;
	struct smmu_unit *unit;
	struct iommu_unit *iommu;
	uintptr_t priv;
	int err;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	priv = (uintptr_t)acpi_get_private(dev);
	if ((priv >> 32) & ACPI_IORT_SMMU_V3_COHACC_OVERRIDE)
		sc->features |= SMMU_FEATURE_COHERENCY;

	if (bootverbose)
		device_printf(sc->dev, "%s: features %x\n",
		    __func__, sc->features);

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

	rid = 0;
	sc->res[1] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->res[1] == NULL) {
		device_printf(dev, "Can't allocate eventq IRQ resource.\n");
		err = ENXIO;
		goto error;
	}

	rid = 2;
	sc->res[3] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->res[3] == NULL) {
		device_printf(dev, "Can't allocate cmdq-sync IRQ resource.\n");
		err = ENXIO;
		goto error;
	}

	rid = 3;
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
	sc->xref = bus_get_resource_start(dev, SYS_RES_MEMORY, 0);

	err = iommu_register(iommu);
	if (err) {
		device_printf(dev, "Failed to register SMMU.\n");
		return (ENXIO);
	}

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

static device_method_t smmu_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,		smmu_acpi_identify),
	DEVMETHOD(device_probe,			smmu_acpi_probe),
	DEVMETHOD(device_attach,		smmu_acpi_attach),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(smmu, smmu_acpi_driver, smmu_acpi_methods,
    sizeof(struct smmu_softc), smmu_driver);

EARLY_DRIVER_MODULE(smmu, acpi, smmu_acpi_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
