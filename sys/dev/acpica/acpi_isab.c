/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

/*
 * ISA Bridge driver for Generic ISA Bus Devices.  See section 10.7 of the
 * ACPI 2.0a specification for details on this device.
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>
#include <isa/isavar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("ISA_ACPI")

struct acpi_isab_softc {
	device_t	ap_dev;
	ACPI_HANDLE	ap_handle;
};


static int	acpi_isab_probe(device_t bus);
static int	acpi_isab_attach(device_t bus);
static int	acpi_isab_read_ivar(device_t dev, device_t child, int which,
		    uintptr_t *result);

static device_method_t acpi_isab_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_isab_probe),
	DEVMETHOD(device_attach,	acpi_isab_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	acpi_isab_read_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{0, 0}
};

static driver_t acpi_isab_driver = {
	"isab",
	acpi_isab_methods,
	sizeof(struct acpi_isab_softc),
};

DRIVER_MODULE(acpi_isab, acpi, acpi_isab_driver, isab_devclass, 0, 0);

static int
acpi_isab_probe(device_t dev)
{

	if ((acpi_get_type(dev) == ACPI_TYPE_DEVICE) &&
	    !acpi_disabled("isa") &&
	    devclass_get_device(isab_devclass, 0) == NULL &&
	    (acpi_MatchHid(dev, "PNP0A05") || acpi_MatchHid(dev, "PNP0A06"))) {
		device_set_desc(dev, "ACPI Generic ISA bridge");
		return(0);
	}
	return(ENXIO);
}

static int
acpi_isab_attach(device_t dev)
{
	struct acpi_isab_softc *sc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	sc = device_get_softc(dev);
	sc->ap_dev = dev;
	sc->ap_handle = acpi_get_handle(dev);

	return (isab_attach(dev));
}

static int
acpi_isab_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct acpi_isab_softc *sc = device_get_softc(dev);

	switch (which) {
	case  ACPI_IVAR_HANDLE:
		*result = (uintptr_t)sc->ap_handle;
		return(0);
	}
	return(ENOENT);
}
