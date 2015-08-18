/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <arm64/arm64/gic.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

struct arm_gic_acpi_softc {
	struct arm_gic_softc gic_sc;
	struct resource_list res;
};

struct madt_table_data {
	device_t parent;
	ACPI_MADT_GENERIC_DISTRIBUTOR *dist;
	ACPI_MADT_GENERIC_INTERRUPT *intr;
};

static void
madt_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	struct madt_table_data *madt_data;

	madt_data = (struct madt_table_data *)arg;

	switch(entry->Type) {
	case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
		if (madt_data->intr != NULL) {
			if (bootverbose)
				device_printf(madt_data->parent,
				    "gic: Already have an interrupt table");
			break;
		}

		madt_data->intr = (ACPI_MADT_GENERIC_INTERRUPT *)entry;
		break;

	case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:
		if (madt_data->dist != NULL) {
			if (bootverbose)
				device_printf(madt_data->parent,
				    "gic: Already have a distributor table");
			break;
		}

		madt_data->dist = (ACPI_MADT_GENERIC_DISTRIBUTOR *)entry;
		break;

	default:
		break;
	}
}

static void
arm_gic_acpi_identify(driver_t *driver, device_t parent)
{
	struct madt_table_data madt_data;
	ACPI_TABLE_MADT *madt;
	vm_paddr_t physaddr;
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
	madt_data.dist = NULL;
	madt_data.intr = NULL;

	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_handler, &madt_data);
	if (madt_data.intr == NULL || madt_data.dist == NULL) {
		device_printf(parent,
		    "No gic interrupt or distributor table\n");
		goto out;
	}

	dev = BUS_ADD_CHILD(parent, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE,
	    "gic", -1);
	if (dev == NULL) {
		device_printf(parent, "add gic child failed\n");
		goto out;
	}

	/* Add the MADT data */
	BUS_SET_RESOURCE(parent, dev, SYS_RES_MEMORY, 0,
	    madt_data.dist->BaseAddress, PAGE_SIZE);
	BUS_SET_RESOURCE(parent, dev, SYS_RES_MEMORY, 1,
	    madt_data.intr->BaseAddress, PAGE_SIZE);

out:
	acpi_unmap_table(madt);
}

static int
arm_gic_acpi_probe(device_t dev)
{

	device_set_desc(dev, "ARM Generic Interrupt Controller");
	return (BUS_PROBE_NOWILDCARD);
}

static device_method_t arm_gic_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	arm_gic_acpi_identify),
	DEVMETHOD(device_probe,		arm_gic_acpi_probe),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gic, arm_gic_acpi_driver, arm_gic_acpi_methods,
    sizeof(struct arm_gic_acpi_softc), arm_gic_driver);

static devclass_t arm_gic_acpi_devclass;

EARLY_DRIVER_MODULE(gic, acpi, arm_gic_acpi_driver,
    arm_gic_acpi_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
