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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <contrib/dev/acpica/acpi.h>
#include <dev/acpica/acpivar.h>

#include <machine/pci_cfgreg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>
#include "pcib_if.h"

#include <dev/acpica/acpi_pcibvar.h>

/* Hooks for the ACPI CA debugging infrastructure. */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI_ACPI")

struct acpi_hpcib_softc {
    device_t		ap_dev;
    ACPI_HANDLE		ap_handle;
    int			ap_flags;

    int			ap_segment;	/* analagous to Alpha 'hose' */
    int			ap_bus;		/* bios-assigned bus number */

    ACPI_BUFFER		ap_prt;		/* interrupt routing table */
};

static int		acpi_pcib_acpi_probe(device_t bus);
static int		acpi_pcib_acpi_attach(device_t bus);
static int		acpi_pcib_acpi_resume(device_t bus);
static int		acpi_pcib_read_ivar(device_t dev, device_t child,
			    int which, uintptr_t *result);
static int		acpi_pcib_write_ivar(device_t dev, device_t child,
			    int which, uintptr_t value);
static uint32_t		acpi_pcib_read_config(device_t dev, int bus, int slot,
			    int func, int reg, int bytes);
static void		acpi_pcib_write_config(device_t dev, int bus, int slot,
			    int func, int reg, uint32_t data, int bytes);
static int		acpi_pcib_acpi_route_interrupt(device_t pcib,
			    device_t dev, int pin);
static struct resource *acpi_pcib_acpi_alloc_resource(device_t dev,
			    device_t child, int type, int *rid,
			    u_long start, u_long end, u_long count,
			    u_int flags);

static device_method_t acpi_pcib_acpi_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		acpi_pcib_acpi_probe),
    DEVMETHOD(device_attach,		acpi_pcib_acpi_attach),
    DEVMETHOD(device_shutdown,		bus_generic_shutdown),
    DEVMETHOD(device_suspend,		bus_generic_suspend),
    DEVMETHOD(device_resume,		acpi_pcib_acpi_resume),

    /* Bus interface */
    DEVMETHOD(bus_print_child,		bus_generic_print_child),
    DEVMETHOD(bus_read_ivar,		acpi_pcib_read_ivar),
    DEVMETHOD(bus_write_ivar,		acpi_pcib_write_ivar),
    DEVMETHOD(bus_alloc_resource,	acpi_pcib_acpi_alloc_resource),
    DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
    DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
    DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

    /* pcib interface */
    DEVMETHOD(pcib_maxslots,		pcib_maxslots),
    DEVMETHOD(pcib_read_config,		acpi_pcib_read_config),
    DEVMETHOD(pcib_write_config,	acpi_pcib_write_config),
    DEVMETHOD(pcib_route_interrupt,	acpi_pcib_acpi_route_interrupt),

    {0, 0}
};

static devclass_t pcib_devclass;

DEFINE_CLASS_0(pcib, acpi_pcib_acpi_driver, acpi_pcib_acpi_methods,
    sizeof(struct acpi_hpcib_softc));
DRIVER_MODULE(acpi_pcib, acpi, acpi_pcib_acpi_driver, pcib_devclass, 0, 0);
MODULE_DEPEND(acpi_pcib, acpi, 1, 1, 1);

static int
acpi_pcib_acpi_probe(device_t dev)
{
    static char *pcib_ids[] = { "PNP0A03", NULL };

    if (acpi_disabled("pcib") ||
	ACPI_ID_PROBE(device_get_parent(dev), dev, pcib_ids) == NULL)
	return (ENXIO);

    if (pci_cfgregopen() == 0)
	return (ENXIO);
    device_set_desc(dev, "ACPI Host-PCI bridge");
    return (0);
}

static int
acpi_pcib_acpi_attach(device_t dev)
{
    struct acpi_hpcib_softc	*sc;
    ACPI_STATUS			status;
    u_int addr, slot, func, busok;
    uint8_t busno;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = device_get_softc(dev);
    sc->ap_dev = dev;
    sc->ap_handle = acpi_get_handle(dev);

    /*
     * Get our base bus number by evaluating _BBN.
     * If this doesn't work, we assume we're bus number 0.
     *
     * XXX note that it may also not exist in the case where we are
     *     meant to use a private configuration space mechanism for this bus,
     *     so we should dig out our resources and check to see if we have
     *     anything like that.  How do we do this?
     * XXX If we have the requisite information, and if we don't think the
     *     default PCI configuration space handlers can deal with this bus,
     *     we should attach our own handler.
     * XXX invoke _REG on this for the PCI config space address space?
     * XXX It seems many BIOS's with multiple Host-PCI bridges do not set
     *     _BBN correctly.  They set _BBN to zero for all bridges.  Thus,
     *     if _BBN is zero and pcib0 already exists, we try to read our
     *     bus number from the configuration registers at address _ADR.
     */
    status = acpi_GetInteger(sc->ap_handle, "_BBN", &sc->ap_bus);
    if (ACPI_FAILURE(status)) {
	if (status != AE_NOT_FOUND) {
	    device_printf(dev, "could not evaluate _BBN - %s\n",
		AcpiFormatException(status));
	    return_VALUE (ENXIO);
	} else {
	    /* If it's not found, assume 0. */
	    sc->ap_bus = 0;
	}
    }

    /*
     * If the bus is zero and pcib0 already exists, read the bus number
     * via PCI config space.
     */
    busok = 1;
    if (sc->ap_bus == 0 && devclass_get_device(pcib_devclass, 0) != dev) {
	busok = 0;
	status = acpi_GetInteger(sc->ap_handle, "_ADR", &addr);
	if (ACPI_FAILURE(status)) {
	    if (status != AE_NOT_FOUND) {
		device_printf(dev, "could not evaluate _ADR - %s\n",
		    AcpiFormatException(status));
		return_VALUE (ENXIO);
	    } else
		device_printf(dev, "couldn't find _ADR\n");
	} else {
	    /* XXX: We assume bus 0. */
	    slot = ACPI_ADR_PCI_SLOT(addr);
	    func = ACPI_ADR_PCI_FUNC(addr);
	    if (bootverbose)
		device_printf(dev, "reading config registers from 0:%d:%d\n",
		    slot, func);
	    if (host_pcib_get_busno(pci_cfgregread, 0, slot, func, &busno) == 0)
		device_printf(dev, "couldn't read bus number from cfg space\n");
	    else {
		sc->ap_bus = busno;
		busok = 1;
	    }
	}
    }

    /*
     * If nothing else worked, hope that ACPI at least lays out the
     * host-PCI bridges in order and that as a result our unit number
     * is actually our bus number.  There are several reasons this
     * might not be true.
     */
    if (busok == 0) {
	sc->ap_bus = device_get_unit(dev);
	device_printf(dev, "trying bus number %d\n", sc->ap_bus);
    }

    /*
     * Get our segment number by evaluating _SEG
     * It's OK for this to not exist.
     */
    status = acpi_GetInteger(sc->ap_handle, "_SEG", &sc->ap_segment);
    if (ACPI_FAILURE(status)) {
	if (status != AE_NOT_FOUND) {
	    device_printf(dev, "could not evaluate _SEG - %s\n",
		AcpiFormatException(status));
	    return_VALUE (ENXIO);
	}
	/* If it's not found, assume 0. */
	sc->ap_segment = 0;
    }

    return (acpi_pcib_attach(dev, &sc->ap_prt, sc->ap_bus));
}

static int
acpi_pcib_acpi_resume(device_t dev)
{

    return (acpi_pcib_resume(dev));
}

/*
 * Support for standard PCI bridge ivars.
 */
static int
acpi_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
    struct acpi_hpcib_softc	*sc = device_get_softc(dev);

    switch (which) {
    case PCIB_IVAR_BUS:
	*result = sc->ap_bus;
	return (0);
    case ACPI_IVAR_HANDLE:
	*result = (uintptr_t)sc->ap_handle;
	return (0);
    case ACPI_IVAR_FLAGS:
	*result = (uintptr_t)sc->ap_flags;
	return (0);
    }
    return (ENOENT);
}

static int
acpi_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
    struct acpi_hpcib_softc	*sc = device_get_softc(dev);

    switch (which) {
    case PCIB_IVAR_BUS:
	sc->ap_bus = value;
	return (0);
    case ACPI_IVAR_HANDLE:
	sc->ap_handle = (ACPI_HANDLE)value;
	return (0);
    case ACPI_IVAR_FLAGS:
	sc->ap_flags = (int)value;
	return (0);
    }
    return (ENOENT);
}

static uint32_t
acpi_pcib_read_config(device_t dev, int bus, int slot, int func, int reg,
    int bytes)
{
    return (pci_cfgregread(bus, slot, func, reg, bytes));
}

static void
acpi_pcib_write_config(device_t dev, int bus, int slot, int func, int reg,
    uint32_t data, int bytes)
{
    pci_cfgregwrite(bus, slot, func, reg, data, bytes);
}

static int
acpi_pcib_acpi_route_interrupt(device_t pcib, device_t dev, int pin)
{
    struct acpi_hpcib_softc *sc = device_get_softc(pcib);

    return (acpi_pcib_route_interrupt(pcib, dev, pin, &sc->ap_prt));
}

static u_long acpi_host_mem_start = 0x80000000;
TUNABLE_ULONG("hw.acpi.host_mem_start", &acpi_host_mem_start);

struct resource *
acpi_pcib_acpi_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
    /*
     * If no memory preference is given, use upper 32MB slot most
     * bioses use for their memory window.  Typically other bridges
     * before us get in the way to assert their preferences on memory.
     * Hardcoding like this sucks, so a more MD/MI way needs to be
     * found to do it.  This is typically only used on older laptops
     * that don't have pci busses behind pci bridge, so assuming > 32MB
     * is liekly OK.
     */
    if (type == SYS_RES_MEMORY && start == 0UL && end == ~0UL)
	start = acpi_host_mem_start;
    if (type == SYS_RES_IOPORT && start == 0UL && end == ~0UL)
	start = 0x1000;
    return (bus_generic_alloc_resource(dev, child, type, rid, start, end,
	count, flags));
}
