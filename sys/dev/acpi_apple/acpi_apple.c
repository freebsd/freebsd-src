/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * acpi_apple(4) - FreeBSD driver for Apple platform ACPI devices.
 *
 * Handles multiple Apple ACPI devices with a single driver:
 *
 *   APP000B (GMUX) / APP9876 (PCH PMC) - platform instance:
 *     - Wake-on-LAN: walk ACPI namespace on shutdown, call _PSW(1) on
 *       any device with ACPI_FLAG_WAKE_ENABLED (dev.X.Y.wake=1).
 *     - Lid state: follow acpi_lid_event to blank and restore the
 *       backlight.
 *
 *   APP0002 (PNLF) - backlight instance:
 *     - Panel backlight control via GPU PCI BAR MMIO registers.
 *     - Supports Intel PCH/CPU, Nvidia NV40/NV50, AMD R600+ families.
 *     - Registers with backlight(9) framework.
 *
 * OSDW() gates on OSYS==0x2710 ("Darwin"), installed via
 * hw.acpi.apple_darwin_osi=1.  _PSW on GIGE only fires if the bge(4)
 * instance has ACPI_FLAG_WAKE_ENABLED (dev.bge.X.wake=1).
 */

#include <sys/cdefs.h>
#include "opt_acpi.h"
#include "acpi_apple.h"

#include "backlight_if.h"

#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("Apple")

/*
 * Attach to APP9876 (PCH PMC, T2 Macs), APP000B (GMUX, discrete GPU
 * Macs), or APP0002 (panel backlight, all Intel Macs with display).
 * APP0001 (SMC) is owned by asmc(4).
 *
 * BUS_PROBE_LOW_PRIORITY so a future apple_gmux driver can claim
 * APP000B at default priority.
 */
static char *acpi_apple_ids[] = { "APP9876", "APP000B", "APP0002", NULL };

static int
acpi_apple_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("apple"))
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, acpi_apple_ids, NULL);
	if (rv > 0)
		return (rv);

	if (acpi_MatchHid(acpi_get_handle(dev), "APP0002"))
		device_set_desc(dev, "Apple Panel Backlight");
	else
		device_set_desc(dev, "Apple platform controller");

	return (BUS_PROBE_LOW_PRIORITY);
}

static int
acpi_apple_attach(device_t dev)
{
	struct acpi_apple_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	if (acpi_MatchHid(sc->handle, "APP0002"))
		sc->mode = ACPI_APPLE_BACKLIGHT;
	else
		sc->mode = ACPI_APPLE_PLATFORM;

	switch (sc->mode) {
	case ACPI_APPLE_PLATFORM:
		return (acpi_apple_platform_attach(sc));
	case ACPI_APPLE_BACKLIGHT:
		return (acpi_apple_backlight_attach(sc));
	}
	return (ENXIO);
}

static int
acpi_apple_detach(device_t dev)
{
	struct acpi_apple_softc *sc;

	sc = device_get_softc(dev);
	switch (sc->mode) {
	case ACPI_APPLE_PLATFORM:
		acpi_apple_platform_detach(sc);
		break;
	case ACPI_APPLE_BACKLIGHT:
		acpi_apple_backlight_detach(sc);
		break;
	}
	return (0);
}

static device_method_t acpi_apple_methods[] = {
	DEVMETHOD(device_probe,			acpi_apple_probe),
	DEVMETHOD(device_attach,		acpi_apple_attach),
	DEVMETHOD(device_detach,		acpi_apple_detach),

	DEVMETHOD(backlight_update_status,	acpi_apple_bl_update_status),
	DEVMETHOD(backlight_get_status,		acpi_apple_bl_get_status),
	DEVMETHOD(backlight_get_info,		acpi_apple_bl_get_info),

	DEVMETHOD_END
};

static driver_t acpi_apple_driver = {
	"acpi_apple",
	acpi_apple_methods,
	sizeof(struct acpi_apple_softc),
};

DRIVER_MODULE(acpi_apple, acpi, acpi_apple_driver, 0, 0);
MODULE_DEPEND(acpi_apple, acpi, 1, 1, 1);
MODULE_DEPEND(acpi_apple, backlight, 1, 1, 1);
MODULE_VERSION(acpi_apple, 1);
