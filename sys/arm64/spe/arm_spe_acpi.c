/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * Portions of this software were developed by Andrew Turner under sponsorship
 * from the FreeBSD Foundation.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <arm64/spe/arm_spe_dev.h>

static device_identify_t arm_spe_acpi_identify;
static device_probe_t arm_spe_acpi_probe;

static device_method_t arm_spe_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,      arm_spe_acpi_identify),
	DEVMETHOD(device_probe,         arm_spe_acpi_probe),

	DEVMETHOD_END,
};

DEFINE_CLASS_1(spe, arm_spe_acpi_driver, arm_spe_acpi_methods,
    sizeof(struct arm_spe_softc), arm_spe_driver);

DRIVER_MODULE(spe, acpi, arm_spe_acpi_driver, 0, 0);

struct madt_data {
	u_int irq;
	bool found;
	bool valid;
};

static void
madt_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_MADT_GENERIC_INTERRUPT *intr;
	struct madt_data *madt_data;
	u_int irq;

	madt_data = (struct madt_data *)arg;

	/* Exit early if we are have decided to not attach */
	if (!madt_data->valid)
		return;

	switch(entry->Type) {
	case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
		intr = (ACPI_MADT_GENERIC_INTERRUPT *)entry;
		irq = intr->SpeInterrupt;

		if (irq == 0) {
			madt_data->valid = false;
		} else if (!madt_data->found) {
			madt_data->found = true;
			madt_data->irq = irq;
		} else if (madt_data->irq != irq) {
			madt_data->valid = false;
		}
		break;

	default:
		break;
	}
}

static void
arm_spe_acpi_identify(driver_t *driver, device_t parent)
{
	struct madt_data madt_data;
	ACPI_TABLE_MADT *madt;
	device_t dev;
	vm_paddr_t physaddr;

	physaddr = acpi_find_table(ACPI_SIG_MADT);
	if (physaddr == 0)
		return;

	madt = acpi_map_table(physaddr, ACPI_SIG_MADT);
	if (madt == NULL) {
		device_printf(parent, "spe: Unable to map the MADT\n");
		return;
	}

	madt_data.irq = 0;
	madt_data.found = false;
	madt_data.valid = true;

	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_handler, &madt_data);

	if (!madt_data.found || !madt_data.valid)
		goto out;

	MPASS(madt_data.irq != 0);

	dev = BUS_ADD_CHILD(parent, 0, "spe", -1);
	if (dev == NULL) {
		device_printf(parent, "add spe child failed\n");
		goto out;
	}

	BUS_SET_RESOURCE(parent, dev, SYS_RES_IRQ, 0, madt_data.irq, 1);

out:
	acpi_unmap_table(madt);
}

static int
arm_spe_acpi_probe(device_t dev)
{
	device_set_desc(dev, "ARM Statistical Profiling Extension");
	return (BUS_PROBE_NOWILDCARD);
}
