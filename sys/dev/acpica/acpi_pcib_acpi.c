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
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

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

    ACPI_BUFFER		ap_prt;		/* interrupt routing table */
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
     * Get the PCI interrupt routing table.
     */
    if ((status = acpi_GetIntoBuffer(sc->ap_handle, AcpiGetIrqRoutingTable, &sc->ap_prt)) != AE_OK) {
	device_printf(dev, "could not get PCI interrupt routing table - %s\n", acpi_strerror(status));
	/* this is not an error, but it may reduce functionality */
    }

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

    /*
     * XXX cross-reference our children to attached devices on the child bus
     *     via _ADR, so we can provide power management.
     */
    /* XXX implement */

    return_VALUE(result);
}

/*
 * ACPI doesn't tell us how many slots there are, so use the standard
 * maximum.
 */
static int
acpi_pcib_maxslots(device_t dev)
{
    return(PCI_SLOTMAX);
}

/*
 * Support for standard PCI bridge ivars.
 */
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

/*
 * Route an interrupt for a child of the bridge.
 *
 * XXX clean up error messages
 *
 * XXX this function is somewhat bulky
 */
static int
acpi_pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{
    struct acpi_pcib_softc	*sc;
    PCI_ROUTING_TABLE		*prt;
    ACPI_HANDLE			lnkdev;
    ACPI_BUFFER			crsbuf, prsbuf;
    ACPI_RESOURCE		*crsres, *prsres;
    ACPI_DEVICE_INFO		devinfo;
    ACPI_STATUS			status;
    u_int8_t			*prtp;
    device_t			*devlist;
    int				devcount;
    int				bus;
    int				interrupt;
    int				i;
    
    crsbuf.Pointer = NULL;
    prsbuf.Pointer = NULL;
    devlist = NULL;
    interrupt = 255;

    /* ACPI numbers pins 0-3, not 1-4 like the BIOS */
    pin--;

    /* find the bridge softc */
    if (devclass_get_devices(acpi_pcib_devclass, &devlist, &devcount))
	goto out;
    BUS_READ_IVAR(pcib, pcib, PCIB_IVAR_BUS, (uintptr_t *)&bus);
    sc = NULL;
    for (i = 0; i < devcount; i++) {
	sc = device_get_softc(*(devlist + i));
	if (sc->ap_bus == bus)
	    break;
	sc = NULL;
    }
    if (sc == NULL)			/* not one of ours */
	goto out;
    prtp = sc->ap_prt.Pointer;
    if (prtp == NULL)			/* didn't get routing table */
	goto out;

    /* scan the table looking for this device */
    for (;;) {
	prt = (PCI_ROUTING_TABLE *)prtp;

	if (prt->Length == 0)		/* end of table */
	    goto out;

	/*
	 * Compare the slot number (high word of Address) and pin number
	 * (note that ACPI uses 0 for INTA) to check for a match.
	 *
	 * Note that the low word of the Address field (function number)
	 * is required by the specification to be 0xffff.  We don't risk
	 * checking it here.
	 */
	if ((((prt->Address & 0xffff0000) >> 16) == pci_get_slot(dev)) &&
	    (prt->Pin == pin)) {
	    device_printf(sc->ap_dev, "matched entry for %d.%d.INT%c (source %s)\n",
			  pci_get_bus(dev), pci_get_slot(dev), 'A' + pin, prt->Source);
	    break;
	}
	
	/* skip to next entry */
	prtp += prt->Length;
    }

    /*
     * If source is empty/NULL, the source index is the global IRQ number.
     */
    if ((prt->Source == NULL) || (prt->Source[0] == '\0')) {
	device_printf(sc->ap_dev, "device is hardwired to IRQ %d\n", prt->SourceIndex);
	interrupt = prt->SourceIndex;
	goto out;
    }
    
    /*
     * We have to find the source device (PCI interrupt link device)
     */
    if (ACPI_FAILURE(AcpiGetHandle(ACPI_ROOT_OBJECT, prt->Source, &lnkdev))) {
	device_printf(sc->ap_dev, "couldn't find PCI interrupt link device %s\n", prt->Source);
	goto out;
    }

    /*
     * Verify that this is a PCI link device, and that it's present.
     */
    if (ACPI_FAILURE(AcpiGetObjectInfo(lnkdev, &devinfo))) {
	device_printf(sc->ap_dev, "couldn't validate PCI interrupt link device %s\n", prt->Source);
	goto out;
    }
    if (!(devinfo.Valid & ACPI_VALID_HID) || strcmp("PNP0C0F", devinfo.HardwareId)) {
	device_printf(sc->ap_dev, "PCI interrupt link device %s has wrong _HID (%s)\n",
		      prt->Source, devinfo.HardwareId);
	goto out;
    }
    /* should be 'present' and 'functioning' */
    if ((devinfo.CurrentStatus & 0x09) != 0x09) {
	device_printf(sc->ap_dev, "PCI interrupt link device %s unavailable (CurrentStatus 0x%x)\n",
		      prt->Source, devinfo.CurrentStatus);
	goto out;
    }

    /*
     * Get the current and possible resources for the interrupt link device.
     */
    if (ACPI_FAILURE(status = acpi_GetIntoBuffer(lnkdev, AcpiGetCurrentResources, &crsbuf))) {
	device_printf(sc->ap_dev, "couldn't get PCI interrupt link device _CRS data - %s\n",
		      acpi_strerror(status));
	goto out;	/* this is fatal */
    }
    if ((status = acpi_GetIntoBuffer(lnkdev, AcpiGetPossibleResources, &prsbuf)) != AE_OK) {
	device_printf(sc->ap_dev, "couldn't get PCI interrupt link device _PRS data - %s\n",
		      acpi_strerror(status));
	/* this is not fatal, since it may be hardwired */
    }
    DEBUG_PRINT(TRACE_RESOURCES, ("got %d bytes for %s._CRS\n", crsbuf.Length, acpi_name(lnkdev)));
    DEBUG_PRINT(TRACE_RESOURCES, ("got %d bytes for %s._PRS\n", prsbuf.Length, acpi_name(lnkdev)));

    /*
     * The interrupt may already be routed, so check _CRS first.  We don't check the
     * 'decoding' bit in the _STA result, since there's nothing in the spec that 
     * mandates it be set, however some BIOS' will set it if the decode is active.
     *
     * The Source Index points to the particular resource entry we're interested in.
     */
    if (ACPI_FAILURE(acpi_FindIndexedResource((ACPI_RESOURCE *)crsbuf.Pointer, prt->SourceIndex, &crsres))) {
	device_printf(sc->ap_dev, "_CRS buffer corrupt, cannot route interrupt\n");
	goto out;
    }

    /* type-check the resource we've got */
    if (crsres->Id != ACPI_RSTYPE_IRQ) {    /* XXX ACPI_RSTYPE_EXT_IRQ */
	device_printf(sc->ap_dev, "_CRS resource entry has unsupported type %d\n", crsres->Id);
	goto out;
    }

    /* if there's more than one interrupt, we are confused */
    if (crsres->Data.Irq.NumberOfInterrupts > 1) {
	device_printf(sc->ap_dev, "device has too many interrupts (%d)\n", crsres->Data.Irq.NumberOfInterrupts);
	goto out;
    }

    /* 
     * If there's only one interrupt, and it's not zero, then we're already routed.
     *
     * Note that we could also check the 'decoding' bit in _STA, but can't depend on
     * it since it's not part of the spec.
     *
     * XXX check ASL examples to see if this is an acceptable set of tests
     */
    if ((crsres->Data.Irq.NumberOfInterrupts == 1) && (crsres->Data.Irq.Interrupts[0] != 0)) {
	device_printf(sc->ap_dev, "device is routed to IRQ %d\n", crsres->Data.Irq.Interrupts[0]);
	interrupt = crsres->Data.Irq.Interrupts[0];
	goto out;
    }
    
    /* 
     * There isn't an interrupt, so we have to look at _PRS to get one.
     * Get the set of allowed interrupts from the _PRS resource indexed by SourceIndex.
     */
    if (prsbuf.Pointer == NULL) {
	device_printf(sc->ap_dev, "device has no routed interrupt and no _PRS on PCI interrupt link device\n");
	goto out;
    }
    if (ACPI_FAILURE(acpi_FindIndexedResource((ACPI_RESOURCE *)prsbuf.Pointer, prt->SourceIndex, &prsres))) {
	device_printf(sc->ap_dev, "_PRS buffer corrupt, cannot route interrupt\n");
	goto out;
    }

    /* type-check the resource we've got */
    if (prsres->Id != ACPI_RSTYPE_IRQ) {    /* XXX ACPI_RSTYPE_EXT_IRQ */
	device_printf(sc->ap_dev, "_PRS resource entry has unsupported type %d\n", prsres->Id);
	goto out;
    }

    /* there has to be at least one interrupt available */
    if (prsres->Data.Irq.NumberOfInterrupts < 1) {
	device_printf(sc->ap_dev, "device has no interrupts\n");
	goto out;
    }

    /*
     * Pick an interrupt to use.  Note that a more scientific approach than just
     * taking the first one available would be desirable.
     *
     * The PCI BIOS $PIR table offers "preferred PCI interrupts", but ACPI doesn't
     * seem to offer a similar mechanism, so picking a "good" interrupt here is a
     * difficult task.
     *
     * Populate our copy of _CRS and pass it back to _SRS to set the interrupt.
     */
    device_printf(sc->ap_dev, "possible interrupts:");
    for (i = 0; i < prsres->Data.Irq.NumberOfInterrupts; i++)
	printf("  %d", prsres->Data.Irq.Interrupts[i]);
    printf("\n");
    crsres->Data.Irq.Interrupts[0] = prsres->Data.Irq.Interrupts[0];
    crsres->Data.Irq.NumberOfInterrupts = 1;
    if (ACPI_FAILURE(status = AcpiSetCurrentResources(lnkdev, &crsbuf))) {
	device_printf(sc->ap_dev, "couldn't route interrupt %d via %s - %s\n",
		      prsres->Data.Irq.Interrupts[0], acpi_name(lnkdev), acpi_strerror(status));
	goto out;
    }
    
    /* successful, return the interrupt we just routed */
    device_printf(sc->ap_dev, "routed interrupt %d via %s\n", 
		  prsres->Data.Irq.Interrupts[0], acpi_name(lnkdev));
    interrupt = prsres->Data.Irq.Interrupts[0];

 out:
    if (devlist != NULL)
	free(devlist, M_TEMP);
    if (crsbuf.Pointer != NULL)
	AcpiOsFree(crsbuf.Pointer);
    if (prsbuf.Pointer != NULL)
	AcpiOsFree(prsbuf.Pointer);

    /* XXX APIC_IO interrupt mapping? */
    return(interrupt);
}

