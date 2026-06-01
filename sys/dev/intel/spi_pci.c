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
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>

#include <dev/intel/spi.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "spibus_if.h"

static struct intelspi_pci_device {
	uint32_t devid;
	enum intelspi_vers vers;
	const char *desc;
} intelspi_pci_devices[] = {
	{ 0x9c658086, SPI_LYNXPOINT, "Intel Lynx Point-LP SPI Controller-0" },
	{ 0x9c668086, SPI_LYNXPOINT, "Intel Lynx Point-LP SPI Controller-1" },
	{ 0x9ce58086, SPI_LYNXPOINT, "Intel Wildcat Point SPI Controller-0" },
	{ 0x9ce68086, SPI_LYNXPOINT, "Intel Wildcat Point SPI Controller-1" },
	{ 0x9d298086, SPI_SUNRISEPOINT, "Intel Sunrise Point-LP SPI Controller-0" },
	{ 0x9d2a8086, SPI_SUNRISEPOINT, "Intel Sunrise Point-LP SPI Controller-1" },
	{ 0xa1298086, SPI_SUNRISEPOINT, "Intel Sunrise Point-H SPI Controller-0" },
	{ 0xa12a8086, SPI_SUNRISEPOINT, "Intel Sunrise Point-H SPI Controller-1" },
	{ 0xa2a98086, SPI_SUNRISEPOINT, "Intel Kaby Lake-H SPI Controller-0" },
	{ 0xa2aa8086, SPI_SUNRISEPOINT, "Intel Kaby Lake-H SPI Controller-1" },
	{ 0xa3a98086, SPI_SUNRISEPOINT, "Intel Comet Lake-V SPI Controller-0" },
	{ 0xa3aa8086, SPI_SUNRISEPOINT, "Intel Comet Lake-V SPI Controller-1" },
};

static int
intelspi_pci_probe(device_t dev)
{
	struct intelspi_softc *sc = device_get_softc(dev);
	uint32_t devid = pci_get_devid(dev);
	int i;

	for (i = 0; i < nitems(intelspi_pci_devices); i++) {
		if (intelspi_pci_devices[i].devid == devid) {
			sc->sc_vers = intelspi_pci_devices[i].vers;
			/* The PCI device is listed in ACPI too.
			 * Not that we use the handle for anything... */
			sc->sc_handle = acpi_get_handle(dev);
			device_set_desc(dev, intelspi_pci_devices[i].desc);
			return (BUS_PROBE_DEFAULT);
		}
	}

	return (ENXIO);
}

static int
intelspi_pci_attach(device_t dev)
{
	struct intelspi_softc *sc = device_get_softc(dev);

	sc->sc_mem_rid = PCIR_BAR(0);
	sc->sc_irq_rid = 0;
	if (pci_alloc_msi(dev, &sc->sc_irq_rid)) {
		device_printf(dev, "Using MSI\n");
	}

	return (intelspi_attach(dev));
}

static int
intelspi_pci_detach(device_t dev)
{
	struct intelspi_softc *sc = device_get_softc(dev);
	int err;

	err = intelspi_detach(dev);
	if (err)
		return (err);

	if (sc->sc_irq_rid != 0)
		pci_release_msi(dev);

	return (0);
}

static device_method_t intelspi_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, intelspi_pci_probe),
	DEVMETHOD(device_attach, intelspi_pci_attach),
	DEVMETHOD(device_detach, intelspi_pci_detach),
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

static driver_t intelspi_pci_driver = {
	"spi",
	intelspi_pci_methods,
	sizeof(struct intelspi_softc),
};

DRIVER_MODULE(intelspi, pci, intelspi_pci_driver, 0, 0);
MODULE_DEPEND(intelspi, pci, 1, 1, 1);
MODULE_DEPEND(intelspi, spibus, 1, 1, 1);
MODULE_PNP_INFO("W32:vendor/device", pci, intelspi, intelspi_pci_devices,
    nitems(intelspi_pci_devices));
