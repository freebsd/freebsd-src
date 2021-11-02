/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Greg V <greg@unrelenting.technology>
 * Copyright (c) 2021 Ruslan Bukin <br@bsdpad.com>
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
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include "acpi_bus_if.h"
#include "pmu.h"

struct madt_ctx {
	struct pmu_softc *sc;
	int error;
	int i;
};

static void
madt_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_MADT_GENERIC_INTERRUPT *intr;
	struct intr_map_data_acpi *ad;
	struct intr_map_data *data;
	struct madt_ctx *ctx;
	struct pmu_softc *sc;
	struct pcpu *pcpu;
	int rid;
	int cpuid;
	int i;

	ctx = arg;
	sc = ctx->sc;
	rid = ctx->i;
	cpuid = -1;

	if (ctx->error)
		return;

	if (entry->Type != ACPI_MADT_TYPE_GENERIC_INTERRUPT)
		return;
	intr = (ACPI_MADT_GENERIC_INTERRUPT *)entry;

	for (i = 0; i < MAXCPU; i++) {
		pcpu = pcpu_find(i);
		if (pcpu != NULL && pcpu->pc_mpidr == intr->ArmMpidr) {
			cpuid = i;
			break;
		}
	}

	if (cpuid == -1) {
		/* pcpu not found. */
		device_printf(sc->dev, "MADT: could not find pcpu, "
		    "ArmMpidr %lx\n", intr->ArmMpidr);
		ctx->error = ENODEV;
		return;
	}

	if (bootverbose)
		device_printf(sc->dev, "MADT: cpu %d (mpidr %lu) irq %d "
		    "%s-triggered\n", cpuid, intr->ArmMpidr,
		    intr->PerformanceInterrupt,
		    (intr->Flags & ACPI_MADT_PERFORMANCE_IRQ_MODE) ?
		    "edge" : "level");

	bus_set_resource(sc->dev, SYS_RES_IRQ, ctx->i,
	    intr->PerformanceInterrupt, 1);

	sc->irq[ctx->i].res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ,
	    &rid, RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq[ctx->i].res == NULL) {
		device_printf(sc->dev, "Failed to allocate IRQ %d\n", ctx->i);
		ctx->error = ENXIO;
		return;
	}

	/*
	 * BUS_CONFIG_INTR does nothing on arm64, so we manually set trigger
	 * mode.
	 */
	data = rman_get_virtual(sc->irq[ctx->i].res);
	KASSERT(data->type == INTR_MAP_DATA_ACPI, ("Wrong data type"));
	ad = (struct intr_map_data_acpi *)data;
	ad->trig = (intr->Flags & ACPI_MADT_PERFORMANCE_IRQ_MODE) ?
		INTR_TRIGGER_EDGE : INTR_TRIGGER_LEVEL;
	ad->pol = INTR_POLARITY_HIGH;

	if (!intr_is_per_cpu(sc->irq[ctx->i].res))
		sc->irq[ctx->i].cpuid = cpuid;

	ctx->i++;
}

static void
pmu_acpi_identify(driver_t *driver, device_t parent)
{
	device_t dev;

	if (acpi_find_table(ACPI_SIG_MADT) == 0)
		return;

	dev = BUS_ADD_CHILD(parent, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST,
	    "pmu", -1);

	if (dev == NULL)
		device_printf(parent, "pmu: Unable to add pmu child\n");
}

static int
pmu_acpi_probe(device_t dev)
{

	device_set_desc(dev, "Performance Monitoring Unit");

	return (BUS_PROBE_NOWILDCARD);
}

static int
pmu_acpi_attach(device_t dev)
{
	struct pmu_softc *sc;
	struct madt_ctx ctx;
	ACPI_TABLE_MADT *madt;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	madt = acpi_map_table(acpi_find_table(ACPI_SIG_MADT), ACPI_SIG_MADT);
	if (madt == NULL) {
		device_printf(dev, "Unable to map the MADT table\n");
		return (ENXIO);
	}

	/* We have to initialize cpuid to -1. */
	for (i = 0; i < MAX_RLEN; i++)
		sc->irq[i].cpuid = -1;

	ctx.sc = sc;
	ctx.i = 0;
	ctx.error = 0;
	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_handler, &ctx);

	acpi_unmap_table(madt);

	if (ctx.error)
		return (ctx.error);

	return (pmu_attach(dev));
}

static device_method_t pmu_acpi_methods[] = {
	DEVMETHOD(device_identify,	pmu_acpi_identify),
	DEVMETHOD(device_probe,		pmu_acpi_probe),
	DEVMETHOD(device_attach,	pmu_acpi_attach),
	DEVMETHOD_END,
};

DEFINE_CLASS_0(pmu, pmu_acpi_driver, pmu_acpi_methods,
    sizeof(struct pmu_softc));

static devclass_t pmu_acpi_devclass;

DRIVER_MODULE(pmu, acpi, pmu_acpi_driver, pmu_acpi_devclass, 0, 0);
