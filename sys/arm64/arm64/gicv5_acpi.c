/*-
 * Copyright (c) 2026 Arm Ltd
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

#include "opt_acpi.h"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/cpuset.h>

#include <machine/intr.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <arm/arm/gic_common.h>
#include "gicv5var.h"

struct gic_v5_acpi_devinfo {
	struct gicv5_devinfo	di_base;
};

static device_identify_t gic_v5_acpi_identify;
static device_probe_t gic_v5_acpi_probe;
static device_attach_t gic_v5_acpi_attach;
static bus_get_resource_list_t gic_v5_acpi_get_resource_list;

static void gic_v5_acpi_bus_attach(device_t);

static device_method_t gic_v5_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,		gic_v5_acpi_identify),
	DEVMETHOD(device_probe,			gic_v5_acpi_probe),
	DEVMETHOD(device_attach,		gic_v5_acpi_attach),

	/* Bus interface */
	DEVMETHOD(bus_get_resource_list,	gic_v5_acpi_get_resource_list),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(gic, gic_v5_acpi_driver, gic_v5_acpi_methods,
    sizeof(struct gicv5_softc), gicv5_driver);

EARLY_DRIVER_MODULE(gic_v5, acpi, gic_v5_acpi_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

struct madt_table_data {
	device_t parent;
	device_t dev;
	ACPI_MADT_GICV5_IRS *irs;
	int count;
};

#define IRS_REGISTER_SIZE 0x10000
#define ITS_REGISTER_SIZE 0x10000
#define ITS_TRANSLATE_REGISTER_SIZE 0x10000
#define IWB_REGISTER_SIZE 0x10000

static void
madt_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	struct madt_table_data *madt_data;
	ACPI_MADT_GICV5_IRS *irs;

	madt_data = (struct madt_table_data *)arg;

	switch (entry->Type) {
	case ACPI_MADT_TYPE_GICV5_IRS:
		if (madt_data->irs) {
			if (bootverbose)
				device_printf(madt_data->parent,
				    "gic: Already have an IRS");
			break;
		}

		irs = (ACPI_MADT_GICV5_IRS *)entry;

		if (irs->Version != 5)
			break;

		madt_data->irs = irs;
		madt_data->count++;
		break;

	default:
		break;
	}
}

static void
rdist_map(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_MADT_GICV5_IRS *irs;

	struct madt_table_data *madt_data;

	madt_data = (struct madt_table_data *)arg;

	switch (entry->Type) {
	case ACPI_MADT_TYPE_GICV5_IRS:
		if (madt_data->irs) {
			if (bootverbose)
				device_printf(madt_data->parent,
				    "gic: Already have an IRS");
			break;
		}

		irs = (ACPI_MADT_GICV5_IRS *)entry;

		if (irs->Version != 5)
			break;

		madt_data->irs = irs;

		BUS_SET_RESOURCE(madt_data->parent, madt_data->dev,
		    SYS_RES_MEMORY, madt_data->count, irs->ConfigBaseAddress,
		    IRS_REGISTER_SIZE);
		madt_data->count++;
		break;

	default:
		break;
	}
}

static void
gic_v5_acpi_identify(driver_t *driver, device_t parent)
{
	struct madt_table_data madt_data;
	ACPI_TABLE_MADT *madt;
	vm_paddr_t physaddr;
	uintptr_t private;
	device_t dev;

	physaddr = acpi_find_table(ACPI_SIG_MADT);
	if (physaddr == 0)
		return;

	madt = acpi_map_table(physaddr, ACPI_SIG_MADT);
	if (madt == NULL) {
		device_printf(parent, "gic: Unable to map the MADT\n");
		return;
	}

	madt_data.parent = parent;
	madt_data.count = 0;
	madt_data.irs = NULL;
	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_handler, &madt_data);
	if (madt_data.count == 0) {
		device_printf(parent, "No GICv5 IRS\n");
		goto out;
	}

	dev = BUS_ADD_CHILD(parent, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE,
	    "gic", -1);
	if (dev == NULL) {
		device_printf(parent, "add gic child failed\n");
		goto out;
	}

	/* Add the MADT data */
	private = madt_data.irs->Version;
	acpi_set_private(dev, (void *)private);

	madt_data.dev = dev;
	madt_data.count = 0;
	madt_data.irs = NULL;
	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    rdist_map, &madt_data);

out:
	acpi_unmap_table(madt);
}

static int
gic_v5_acpi_probe(device_t dev)
{
	switch ((uintptr_t)acpi_get_private(dev)) {
	case 5:
		break;
	default:
		return (ENXIO);
	}

	device_set_desc(dev, "ARM Generic Interrupt Controller v5");
	return (BUS_PROBE_NOWILDCARD);
}

static void
madt_count_irs(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	struct gicv5_softc *sc = arg;

	if (entry->Type == ACPI_MADT_TYPE_GICV5_IRS)
		sc->gic_nirs++;
}

static int
gic_v5_acpi_count_nirs(struct gicv5_softc *sc, ACPI_TABLE_MADT *madt)
{
	sc->gic_nirs = 0;

	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_count_irs, sc);

	return (sc->gic_nirs > 0 ? 0 : ENXIO);
}

struct process_gicc_args {
	struct gicv5_softc *sc;
	ACPI_TABLE_MADT *madt;
	uint32_t irs_id;
	cpuset_t cpuset;
};

struct process_irs_args {
	device_t dev;
	struct gicv5_softc *sc;
	ACPI_TABLE_MADT *madt;
};

static void
madt_process_gicc(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	struct process_gicc_args *gicc_args;
	ACPI_MADT_GENERIC_INTERRUPT *gicc;

	gicc_args = (struct process_gicc_args *)arg;

	if (entry->Type == ACPI_MADT_TYPE_GENERIC_INTERRUPT) {
		gicc = (ACPI_MADT_GENERIC_INTERRUPT *)entry;

		if (gicc->IrsId == gicc_args->irs_id)
			CPU_SET(gicc->CpuInterfaceNumber, &gicc_args->cpuset);
	}
}

static void
madt_process_irs(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	struct process_gicc_args gicc_args;
	struct process_irs_args *irs_args;
	ACPI_MADT_GICV5_IRS *irs;
	struct gicv5_softc *sc;
	ACPI_TABLE_MADT *madt;
	int error;

	irs_args = (struct process_irs_args *)arg;
	sc = irs_args->sc;
	madt = irs_args->madt;

	if (entry->Type == ACPI_MADT_TYPE_GICV5_IRS) {
		irs = (ACPI_MADT_GICV5_IRS *)entry;

		gicc_args.sc = sc;
		gicc_args.madt = madt;
		gicc_args.irs_id = irs->IrsId;
		CPU_ZERO(&gicc_args.cpuset);

		/* Build gicc_args.cpuset from GICC tables */
		acpi_walk_subtables(madt + 1,
		    (char *)madt + madt->Header.Length,
		    madt_process_gicc, &gicc_args);

		sc->gic_nirs++;

		gicv5_irs_init(irs_args->dev, sc->gic_nirs - 1,
		    &gicc_args.cpuset);

		error = bus_set_resource(irs_args->dev, SYS_RES_MEMORY,
		    sc->gic_nirs - 1, irs->ConfigBaseAddress,
		    IRS_REGISTER_SIZE);
		if (error != 0)
			panic("%s: Unable to set memory resourec",
			    device_get_nameunit(irs_args->dev));
	}
}

static int
gic_v5_acpi_process_irs(device_t dev, struct gicv5_softc *sc,
    ACPI_TABLE_MADT *madt)
{
	struct process_irs_args args;

	args.dev = dev;
	args.sc = sc;
	args.madt = madt;

	sc->gic_nirs = 0;

	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_process_irs, &args);

	return (0);
}

static int
gic_v5_acpi_attach(device_t dev)
{
	struct gicv5_softc *sc;
	ACPI_TABLE_MADT *madt;
	vm_paddr_t physaddr;
	int err;

	sc = device_get_softc(dev);
	sc->gic_dev = dev;
	sc->gic_bus = GIC_BUS_ACPI;

	physaddr = acpi_find_table(ACPI_SIG_MADT);
	if (physaddr == 0)
		return (ENXIO);

	madt = acpi_map_table(physaddr, ACPI_SIG_MADT);
	if (madt == NULL) {
		device_printf(dev, "Unable to map the MADT\n");
		return (ENXIO);
	}

	err = gic_v5_acpi_count_nirs(sc, madt);
	if (err != 0)
		goto count_error;

	sc->gic_irs = mallocarray(sc->gic_nirs, sizeof(sc->gic_irs[0]),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	// for each IRS
	//   walk GICC entries to find matching IRS IDs
	err = gic_v5_acpi_process_irs(dev, sc, madt);

	gicv5_attach(dev);

	sc->gic_pic = intr_pic_register(dev, ACPI_INTR_XREF);
	if (sc->gic_pic == NULL)
		panic("%s: could not register PIC", device_get_nameunit(dev));

	intr_ipi_pic_register(dev, 0);

	err = intr_pic_claim_root(dev, ACPI_INTR_XREF, gicv5_intr, sc,
	    INTR_ROOT_IRQ);
	if (err != 0)
		panic("%s: Unable to claim PIC root", device_get_nameunit(dev));

	gic_v5_acpi_bus_attach(dev);

	bus_attach_children(dev);

	acpi_unmap_table(madt);

	return (0);

count_error:
	if (bootverbose) {
		device_printf(dev,
		    "Failed to attach. Error %d\n", err);
	}

	acpi_unmap_table(madt);

	return (err);
}

static void
gic_v5_add_children(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_MADT_GICv5_ITS *its;
	struct gic_v5_acpi_devinfo *di;
	struct gicv5_softc *sc;
	device_t dev;

	if (entry->Type == ACPI_MADT_TYPE_GICV5_ITS) {
		/* We have an ITS, add it as a child */
		its = (ACPI_MADT_GICv5_ITS *)entry;
		dev = arg;
		sc = device_get_softc(dev);

		di = malloc(sizeof(*di), M_DEVBUF, M_WAITOK | M_ZERO);

		resource_list_init(&di->di_base.di_rl);
		resource_list_add(&di->di_base.di_rl, SYS_RES_MEMORY, 0,
		    its->BaseAddress, its->BaseAddress + ITS_REGISTER_SIZE - 1,
		    ITS_REGISTER_SIZE);

		di->di_base.di_irs = sc->gic_irs[0];

		gicv5_add_child(dev, &di->di_base);

	}
}

static void
gic_v5_acpi_bus_attach(device_t dev)
{
	ACPI_TABLE_MADT *madt;
	vm_paddr_t physaddr;

	physaddr = acpi_find_table(ACPI_SIG_MADT);
	if (physaddr == 0) {
		device_printf(dev, "gic_v5_acpi_bus_attach: no madt\n");
		return;
	}

	madt = acpi_map_table(physaddr, ACPI_SIG_MADT);
	if (madt == NULL) {
		device_printf(dev, "Unable to map the MADT to add children\n");
		return;
	}

	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    gic_v5_add_children, dev);

	acpi_unmap_table(madt);

	bus_attach_children(dev);
}

static struct resource_list *
gic_v5_acpi_get_resource_list(device_t bus, device_t child)
{
	struct gic_v5_acpi_devinfo *di;

	di = device_get_ivars(child);
	KASSERT(di != NULL, ("%s: No devinfo", __func__));

	return (&di->di_base.di_rl);
}
