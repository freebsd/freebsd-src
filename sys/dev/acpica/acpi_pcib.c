/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *	$FreeBSD$
 */
#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

#include <machine/pci_cfgreg.h>
#include <pci/pcivar.h>
#include "pcib_if.h"

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BUS
MODULE_NAME("PCI")

struct acpi_pcib_softc {
    device_t		ap_dev;
    ACPI_HANDLE		ap_handle;

    int			ap_segment;	/* analagous to Alpha 'hose' */
    int			ap_bus;		/* bios-assigned bus number */
};

static int		acpi_pcib_probe(device_t bus);
static int		acpi_pcib_attach(device_t bus);
static int		acpi_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result);
static int		acpi_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value);
static int		acpi_pcib_maxslots(device_t dev);
static u_int32_t	acpi_pcib_read_config(device_t dev, int bus, int slot, int func, int reg, int bytes);
static void		acpi_pcib_write_config(device_t dev, int bus, int slot, int func, int reg, 
					       u_int32_t data, int bytes);
static int		acpi_pcib_route_interrupt(device_t pcib, device_t dev, int pin);

static device_method_t acpi_pcib_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		acpi_pcib_probe),
    DEVMETHOD(device_attach,		acpi_pcib_attach),
    DEVMETHOD(device_shutdown,		bus_generic_shutdown),
    DEVMETHOD(device_suspend,		bus_generic_suspend),
    DEVMETHOD(device_resume,		bus_generic_resume),

    /* Bus interface */
    DEVMETHOD(bus_print_child,		bus_generic_print_child),
    DEVMETHOD(bus_read_ivar,		acpi_pcib_read_ivar),
    DEVMETHOD(bus_write_ivar,		acpi_pcib_write_ivar),
    DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
    DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
    DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource, 	bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
    DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

    /* pcib interface */
    DEVMETHOD(pcib_maxslots,		acpi_pcib_maxslots),
    DEVMETHOD(pcib_read_config,		acpi_pcib_read_config),
    DEVMETHOD(pcib_write_config,	acpi_pcib_write_config),
    DEVMETHOD(pcib_route_interrupt,	acpi_pcib_route_interrupt),

    {0, 0}
};

static driver_t acpi_pcib_driver = {
    "acpi_pcib",
    acpi_pcib_methods,
    sizeof(struct acpi_pcib_softc),
};

devclass_t acpi_pcib_devclass;
DRIVER_MODULE(acpi_pcib, acpi, acpi_pcib_driver, acpi_pcib_devclass, 0, 0);

static int
acpi_pcib_probe(device_t dev)
{

    if ((acpi_get_type(dev) == ACPI_TYPE_DEVICE) &&
	!acpi_disabled("pci") &&
	acpi_MatchHid(dev, "PNP0A03")) {

	/*
	 * Set device description 
	 */
	device_set_desc(dev, "Host-PCI bridge");
	return(0);
    }
    return(ENXIO);
}

static int
acpi_pcib_attach(device_t dev)
{
    struct acpi_pcib_softc	*sc;
    device_t			child;
    ACPI_STATUS			status;
    int				result;

    FUNCTION_TRACE(__func__);

    sc = device_get_softc(dev);
    sc->ap_dev = dev;
    sc->ap_handle = acpi_get_handle(dev);

    /*
     * Don't attach if we're not really there.
     *
     * XXX this isn't entirely correct, since we may be a PCI bus
     * on a hot-plug docking station, etc.
     */
    if (!acpi_DeviceIsPresent(dev))
	return_VALUE(ENXIO);

    /*
     * Get our segment number by evaluating _SEG
     * It's OK for this to not exist.
     */
    if ((status = acpi_EvaluateInteger(sc->ap_handle, "_SEG", &sc->ap_segment)) != AE_OK) {
	if (status != AE_NOT_FOUND) {
	    device_printf(dev, "could not evaluate _SEG - %s\n", acpi_strerror(status));
	    return_VALUE(ENXIO);
	}
	/* if it's not found, assume 0 */
	sc->ap_segment = 0;
    }

    /*
     * Get our base bus number by evaluating _BBN
     * If this doesn't exist, we assume we're bus number 0.
     *
     * XXX note that it may also not exist in the case where we are 
     *     meant to use a private configuration space mechanism for this bus,
     *     so we should dig out our resources and check to see if we have
     *     anything like that.  How do we do this?
     * XXX If we have the requisite information, and if we don't think the
     *     default PCI configuration space handlers can deal with this bus,
     *     we should attach our own handler.
     * XXX invoke _REG on this for the PCI config space address space?
     */
    if ((status = acpi_EvaluateInteger(sc->ap_handle, "_BBN", &sc->ap_bus)) != AE_OK) {
	if (status != AE_NOT_FOUND) {
	    device_printf(dev, "could not evaluate _BBN - %s\n", acpi_strerror(status));
	    return_VALUE(ENXIO);
	}
	/* if it's not found, assume 0 */
	sc->ap_bus = 0;
    }

    /*
     * Make sure that this bus hasn't already been found.  If it has, return silently
     * (should we complain here?).
     */
    if (devclass_get_device(devclass_find("pci"), sc->ap_bus) != NULL)
	return_VALUE(0);

    /*
     * Attach the PCI bus proper.
     */
    if ((child = device_add_child(dev, "pci", sc->ap_bus)) == NULL) {
	device_printf(device_get_parent(dev), "couldn't attach pci bus");
	return_VALUE(ENXIO);
    }

    /*
     * Now go scan the bus.
     *
     * XXX It would be nice to defer this and count on the nexus getting it
     * after the first pass, but this does not seem to be reliable.
     */
    result = bus_generic_attach(dev);
    return_VALUE(result);
}

static int
acpi_pcib_maxslots(device_t dev)
{
    return(31);
}

static int
acpi_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
    struct acpi_pcib_softc	*sc = device_get_softc(dev);

    switch (which) {
    case  PCIB_IVAR_BUS:
	*result = sc->ap_bus;
	return(0);
    }
    return(ENOENT);
}

static int
acpi_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
    struct acpi_pcib_softc	*sc = device_get_softc(dev);

    switch (which) {
    case  PCIB_IVAR_BUS:
	sc->ap_bus = value;
	return(0);
    }
    return(ENOENT);
}

static u_int32_t
acpi_pcib_read_config(device_t dev, int bus, int slot, int func, int reg, int bytes)
{
    return(pci_cfgregread(bus, slot, func, reg, bytes));
}

static void
acpi_pcib_write_config(device_t dev, int bus, int slot, int func, int reg, u_int32_t data, int bytes)
{
    pci_cfgregwrite(bus, slot, func, reg, data, bytes);
}

static int
acpi_pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{
    /* XXX this is not the right way to do this! */
    pci_cfgregopen();
    return(pci_cfgintr(pci_get_bus(dev), pci_get_slot(dev), pin));
}
