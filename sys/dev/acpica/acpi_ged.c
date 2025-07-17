/*-
 * Copyright (c) 2022 Takanori Watanabe
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
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT ACPI_BUS
ACPI_MODULE_NAME("GED")

static MALLOC_DEFINE(M_ACPIGED, "acpiged", "ACPI Generic event data");

struct acpi_ged_event {
	device_t dev;
	struct resource *r;
	int rid;
	void *cookie;
	ACPI_HANDLE ah;
	ACPI_OBJECT_LIST args;
	ACPI_OBJECT arg1;
};

struct acpi_ged_softc {
	int numevts;
	struct acpi_ged_event *evts;
};

static int acpi_ged_probe(device_t dev);
static int acpi_ged_attach(device_t dev);
static int acpi_ged_detach(device_t dev);

static char *ged_ids[] = { "ACPI0013", NULL };

static device_method_t acpi_ged_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, acpi_ged_probe),
	DEVMETHOD(device_attach, acpi_ged_attach),
	DEVMETHOD(device_detach, acpi_ged_detach),
	DEVMETHOD_END
};

static driver_t acpi_ged_driver = {
	"acpi_ged",
	acpi_ged_methods,
	sizeof(struct acpi_ged_softc),
};

DRIVER_MODULE(acpi_ged, acpi, acpi_ged_driver, 0, 0);
MODULE_DEPEND(acpi_ged, acpi, 1, 1, 1);

static int acpi_ged_defer;
SYSCTL_INT(_debug_acpi, OID_AUTO, ged_defer, CTLFLAG_RWTUN,
    &acpi_ged_defer, 0,
    "Handle ACPI GED via a task, rather than in the ISR");

static void
acpi_ged_evt(void *arg)
{
	struct acpi_ged_event *evt = arg;

	AcpiEvaluateObject(evt->ah, NULL, &evt->args, NULL);
}

static void
acpi_ged_intr(void *arg)
{
	struct acpi_ged_event *evt = arg;

	if (acpi_ged_defer)
		AcpiOsExecute(OSL_GPE_HANDLER, acpi_ged_evt, arg);
	else
		AcpiEvaluateObject(evt->ah, NULL, &evt->args, NULL);
}
static int
acpi_ged_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("ged"))
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, ged_ids, NULL);
	if (rv > 0)
		return (ENXIO);

	device_set_desc(dev, "Generic Event Device");
	return (rv);
}

/*this should be in acpi_resource.*/
static int
acpi_get_trigger(ACPI_RESOURCE *res)
{
	int trig;

	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_IRQ:
		KASSERT(res->Data.Irq.InterruptCount == 1,
			("%s: multiple interrupts", __func__));
		trig = res->Data.Irq.Triggering;
		break;
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		KASSERT(res->Data.ExtendedIrq.InterruptCount == 1,
			("%s: multiple interrupts", __func__));
		trig = res->Data.ExtendedIrq.Triggering;
		break;
	default:
		panic("%s: bad resource type %u", __func__, res->Type);
	}

	return (trig == ACPI_EDGE_SENSITIVE)
		? INTR_TRIGGER_EDGE : INTR_TRIGGER_LEVEL;
}

static int
acpi_ged_attach(device_t dev)
{
	struct acpi_ged_softc *sc = device_get_softc(dev);
	struct resource_list *rl;
	struct resource_list_entry *rle;
	ACPI_RESOURCE ares;
	ACPI_HANDLE evt_method;
	int i;
	int rawirq, trig;
	char name[] = "_Xnn";

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	if (ACPI_FAILURE(AcpiGetHandle(acpi_get_handle(dev), "_EVT",
				       &evt_method))) {
		device_printf(dev, "_EVT not found\n");
		evt_method = NULL;
	}

	rl = BUS_GET_RESOURCE_LIST(device_get_parent(dev), dev);
	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type == SYS_RES_IRQ) {
			sc->numevts++;
		}
	}
	sc->evts = mallocarray(sc->numevts, sizeof(*sc->evts), M_ACPIGED,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < sc->numevts; i++) {
		sc->evts[i].dev = dev;
		sc->evts[i].rid = i;
		sc->evts[i].r = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->evts[i].rid,  RF_ACTIVE | RF_SHAREABLE);
		if (sc->evts[i].r == NULL) {
			device_printf(dev, "Cannot alloc %dth irq\n", i);
			continue;
		}
#ifdef INTRNG
		{
			struct intr_map_data_acpi *ima;
			ima = rman_get_virtual(sc->evts[i].r);
			if (ima == NULL) {
				device_printf(dev, "map not found"
					      " non-intrng?\n");
				rawirq = rman_get_start(sc->evts[i].r);
				trig = INTR_TRIGGER_LEVEL;
				if (ACPI_SUCCESS(acpi_lookup_irq_resource
					(dev, sc->evts[i].rid,
					 sc->evts[i].r, &ares))) {
					trig = acpi_get_trigger(&ares);
				}
			} else if (ima->hdr.type == INTR_MAP_DATA_ACPI) {
				device_printf(dev, "Raw IRQ %d\n", ima->irq);
				rawirq = ima->irq;
				trig = ima->trig;
			} else {
				device_printf(dev, "Not supported intr"
					      " type%d\n", ima->hdr.type);
				continue;
			}
		}
#else
		rawirq = rman_get_start(sc->evts[i].r);
		trig = INTR_TRIGGER_LEVEL;
		if (ACPI_SUCCESS(acpi_lookup_irq_resource
				(dev, sc->evts[i].rid,
				 sc->evts[i].r, &ares))) {
			trig = acpi_get_trigger(&ares);
		}
#endif
		if (rawirq < 0x100) {
			sprintf(name, "_%c%02X",
				((trig == INTR_TRIGGER_EDGE) ? 'E' : 'L'),
				rawirq);
			if (ACPI_SUCCESS(AcpiGetHandle
					(acpi_get_handle(dev),
					 name, &sc->evts[i].ah))) {
				sc->evts[i].args.Count = 0; /* ensure */
			} else {
				sc->evts[i].ah = NULL; /* ensure */
			}
		}

		if (sc->evts[i].ah == NULL) {
			if (evt_method != NULL) {
				sc->evts[i].ah = evt_method;
				sc->evts[i].arg1.Type = ACPI_TYPE_INTEGER;
				sc->evts[i].arg1.Integer.Value = rawirq;
				sc->evts[i].args.Count = 1;
				sc->evts[i].args.Pointer = &sc->evts[i].arg1;
			} else{
				device_printf
					(dev,
					 "Cannot find handler method %d\n",
					 i);
				continue;
			}
		}

		if (bus_setup_intr(dev, sc->evts[i].r,
			INTR_TYPE_MISC | INTR_MPSAFE | INTR_SLEEPABLE |
			INTR_EXCL, NULL,  acpi_ged_intr, &sc->evts[i],
			&sc->evts[i].cookie) != 0) {
			device_printf(dev, "Failed to setup intr %d\n", i);
		}
	}

	return_VALUE(0);
}

static int
acpi_ged_detach(device_t dev)
{
	struct acpi_ged_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->numevts; i++) {
		if (sc->evts[i].cookie) {
			bus_teardown_intr(dev, sc->evts[i].r,
			    sc->evts[i].cookie);
		}
		if (sc->evts[i].r) {
			bus_release_resource(dev, SYS_RES_IRQ, sc->evts[i].rid,
			    sc->evts[i].r);
		}
	}
	free(sc->evts, M_ACPIGED);

	return (0);
}
