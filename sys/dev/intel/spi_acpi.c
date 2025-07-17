/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Val Packett <val@packett.cool>
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>

#include <dev/intel/spi.h>

#include "spibus_if.h"

static const struct intelspi_acpi_device {
	const char *hid;
	enum intelspi_vers vers;
	const char *desc;
} intelspi_acpi_devices[] = {
	{ "80860F0E", SPI_BAYTRAIL, "Intel Bay Trail SPI Controller" },
	{ "8086228E", SPI_BRASWELL, "Intel Braswell SPI Controller" },
};

static char *intelspi_ids[] = { "80860F0E", "8086228E", NULL };

static int
intelspi_acpi_probe(device_t dev)
{
	struct intelspi_softc *sc = device_get_softc(dev);
	char *hid;
	int i;

	if (acpi_disabled("spi"))
		return (ENXIO);

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, intelspi_ids, &hid) > 0)
		return (ENXIO);

	for (i = 0; i < nitems(intelspi_acpi_devices); i++) {
		if (strcmp(intelspi_acpi_devices[i].hid, hid) == 0) {
			sc->sc_vers = intelspi_acpi_devices[i].vers;
			sc->sc_handle = acpi_get_handle(dev);
			device_set_desc(dev, intelspi_acpi_devices[i].desc);
			return (BUS_PROBE_DEFAULT);
		}
	}

	return (ENXIO);
}

static int
intelspi_acpi_attach(device_t dev)
{
	struct intelspi_softc *sc = device_get_softc(dev);

	sc->sc_mem_rid = 0;
	sc->sc_irq_rid = 0;

	return (intelspi_attach(dev));
}

static device_method_t intelspi_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, intelspi_acpi_probe),
	DEVMETHOD(device_attach, intelspi_acpi_attach),
	DEVMETHOD(device_detach, intelspi_detach),
	DEVMETHOD(device_suspend, intelspi_suspend),
	DEVMETHOD(device_resume, intelspi_resume),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr, bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr, bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource, bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource, bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource, bus_generic_adjust_resource),

	/* SPI interface */
	DEVMETHOD(spibus_transfer, intelspi_transfer),

	DEVMETHOD_END
};

static driver_t intelspi_acpi_driver = {
	"spi",
	intelspi_acpi_methods,
	sizeof(struct intelspi_softc),
};

DRIVER_MODULE(intelspi, acpi, intelspi_acpi_driver, 0, 0);
MODULE_DEPEND(intelspi, acpi, 1, 1, 1);
MODULE_DEPEND(intelspi, spibus, 1, 1, 1);
ACPI_PNP_INFO(intelspi_ids);
