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
#include <dev/acpica/acpi_pcibvar.h>

#include <machine/pci_cfgreg.h>
#include <pci/pcivar.h>
#include <pci/pcib_private.h>
#include "pcib_if.h"

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI")

int
acpi_pcib_attach(device_t dev, ACPI_BUFFER *prt, int busno)
{
    device_t			child;
    ACPI_STATUS			status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * Don't attach if we're not really there.
     *
     * XXX: This isn't entirely correct since we may be a PCI bus
     * on a hot-plug docking station, etc.
     */
    if (!acpi_DeviceIsPresent(dev))
	return_VALUE(ENXIO);

    /*
     * Make sure that this bus hasn't already been found.
     */
    if (devclass_get_device(devclass_find("pci"), busno) != NULL) {
	device_printf(dev, "duplicate bus number %d - not probing bus\n",
	    busno);
	return_VALUE(0);
    }

    /*
     * Get the PCI interrupt routing table for this bus.
     */
    prt->Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiGetIrqRoutingTable(acpi_get_handle(dev), prt);
    if (ACPI_FAILURE(status))
	/* This is not an error, but it may reduce functionality. */
	device_printf(dev, "could not get PCI interrupt routing table - %s\n",
	    AcpiFormatException(status));

    /*
     * Attach the PCI bus proper.
     */
    if ((child = device_add_child(dev, "pci", busno)) == NULL) {
	device_printf(device_get_parent(dev), "couldn't attach pci bus");
	return_VALUE(ENXIO);
    }

    /*
     * Now go scan the bus.
     *
     * XXX It would be nice to defer this and count on the nexus getting it
     * after the first pass, but this does not seem to be reliable.
     */
    return_VALUE(bus_generic_attach(dev));
}

/*
 * Route an interrupt for a child of the bridge.
 *
 * XXX clean up error messages
 *
 * XXX this function is somewhat bulky
 */
int
acpi_pcib_route_interrupt(device_t pcib, device_t dev, int pin,
    ACPI_BUFFER *prtbuf)
{
    ACPI_PCI_ROUTING_TABLE	*prt;
    ACPI_HANDLE			lnkdev;
    ACPI_BUFFER			crsbuf, prsbuf;
    ACPI_RESOURCE		*crsres, *prsres, resbuf;
    ACPI_DEVICE_INFO		devinfo;
    ACPI_STATUS			status;
    u_int8_t			*prtp;
    int				interrupt;
    int				i;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    
    crsbuf.Pointer = NULL;
    prsbuf.Pointer = NULL;
    interrupt = 255;

    /* ACPI numbers pins 0-3, not 1-4 like the BIOS */
    pin--;

    prtp = prtbuf->Pointer;
    if (prtp == NULL)			/* didn't get routing table */
	goto out;

    /* scan the table looking for this device */
    for (;;) {
	prt = (ACPI_PCI_ROUTING_TABLE *)prtp;

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
	    if (bootverbose)
		device_printf(pcib, "matched entry for %d.%d.INT%c (source %s)\n",
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
	if (bootverbose)
	    device_printf(pcib, "device is hardwired to IRQ %d\n",
		prt->SourceIndex);
	interrupt = prt->SourceIndex;
	goto out;
    }
    
    /*
     * We have to find the source device (PCI interrupt link device)
     */
    if (ACPI_FAILURE(AcpiGetHandle(ACPI_ROOT_OBJECT, prt->Source, &lnkdev))) {
	device_printf(pcib, "couldn't find PCI interrupt link device %s\n",
	    prt->Source);
	goto out;
    }

    /*
     * Verify that this is a PCI link device, and that it's present.
     */
    if (ACPI_FAILURE(AcpiGetObjectInfo(lnkdev, &devinfo))) {
	device_printf(pcib, "couldn't validate PCI interrupt link device %s\n",
	    prt->Source);
	goto out;
    }
    if (!(devinfo.Valid & ACPI_VALID_HID) || strcmp("PNP0C0F", devinfo.HardwareId)) {
	device_printf(pcib, "PCI interrupt link device %s has wrong _HID (%s)\n",
		      prt->Source, devinfo.HardwareId);
	goto out;
    }
    if (devinfo.Valid & ACPI_VALID_STA && (devinfo.CurrentStatus & 0x9) != 0x9) {
	device_printf(pcib, "PCI interrupt link device %s not present\n",
		      prt->Source);
	goto out;
    }

    /*
     * Get the current and possible resources for the interrupt link device.
     */
    crsbuf.Length = ACPI_ALLOCATE_BUFFER;
    if (ACPI_FAILURE(status = AcpiGetCurrentResources(lnkdev, &crsbuf))) {
	device_printf(pcib, "couldn't get PCI interrupt link device _CRS data - %s\n",
		      AcpiFormatException(status));
	goto out;	/* this is fatal */
    }
    prsbuf.Length = ACPI_ALLOCATE_BUFFER;
    if (ACPI_FAILURE(status = AcpiGetPossibleResources(lnkdev, &prsbuf))) {
	device_printf(pcib, "couldn't get PCI interrupt link device _PRS data - %s\n",
		      AcpiFormatException(status));
	/* this is not fatal, since it may be hardwired */
    }
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "got %d bytes for %s._CRS\n", crsbuf.Length, acpi_name(lnkdev)));
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "got %d bytes for %s._PRS\n", prsbuf.Length, acpi_name(lnkdev)));

    /*
     * The interrupt may already be routed, so check _CRS first.  We don't check the
     * 'decoding' bit in the _STA result, since there's nothing in the spec that 
     * mandates it be set, however some BIOS' will set it if the decode is active.
     *
     * The Source Index points to the particular resource entry we're interested in.
     */
    if (ACPI_FAILURE(acpi_FindIndexedResource(&crsbuf, prt->SourceIndex, &crsres))) {
	device_printf(pcib, "_CRS buffer corrupt, cannot route interrupt\n");
	goto out;
    }

    /* type-check the resource we've got */
    if (crsres->Id != ACPI_RSTYPE_IRQ) {    /* XXX ACPI_RSTYPE_EXT_IRQ */
	device_printf(pcib, "_CRS resource entry has unsupported type %d\n",
	    crsres->Id);
	goto out;
    }

    /* if there's more than one interrupt, we are confused */
    if (crsres->Data.Irq.NumberOfInterrupts > 1) {
	device_printf(pcib, "device has too many interrupts (%d)\n",
	    crsres->Data.Irq.NumberOfInterrupts);
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
	device_printf(pcib, "device is routed to IRQ %d\n",
	    crsres->Data.Irq.Interrupts[0]);
	interrupt = crsres->Data.Irq.Interrupts[0];
	goto out;
    }
    
    /* 
     * There isn't an interrupt, so we have to look at _PRS to get one.
     * Get the set of allowed interrupts from the _PRS resource indexed by SourceIndex.
     */
    if (prsbuf.Pointer == NULL) {
	device_printf(pcib, "device has no routed interrupt and no _PRS on PCI interrupt link device\n");
	goto out;
    }
    if (ACPI_FAILURE(acpi_FindIndexedResource(&prsbuf, prt->SourceIndex, &prsres))) {
	device_printf(pcib, "_PRS buffer corrupt, cannot route interrupt\n");
	goto out;
    }

    /* type-check the resource we've got */
    if (prsres->Id != ACPI_RSTYPE_IRQ) {    /* XXX ACPI_RSTYPE_EXT_IRQ */
	device_printf(pcib, "_PRS resource entry has unsupported type %d\n",
	    prsres->Id);
	goto out;
    }

    /* there has to be at least one interrupt available */
    if (prsres->Data.Irq.NumberOfInterrupts < 1) {
	device_printf(pcib, "device has no interrupts\n");
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
     * Build a resource buffer and pass it to AcpiSetCurrentResources to route the
     * new interrupt.
     */
    device_printf(pcib, "possible interrupts:");
    for (i = 0; i < prsres->Data.Irq.NumberOfInterrupts; i++)
	printf("  %d", prsres->Data.Irq.Interrupts[i]);
    printf("\n");

    if (crsbuf.Pointer != NULL)			/* should never happen */
	AcpiOsFree(crsbuf.Pointer);
    crsbuf.Pointer = NULL;
    resbuf.Id = ACPI_RSTYPE_IRQ;
    resbuf.Length = ACPI_SIZEOF_RESOURCE(ACPI_RESOURCE_IRQ);
    resbuf.Data.Irq = prsres->Data.Irq;		/* structure copy other fields */
    resbuf.Data.Irq.NumberOfInterrupts = 1;
    resbuf.Data.Irq.Interrupts[0] = prsres->Data.Irq.Interrupts[0];	/* just take first... */
    if (ACPI_FAILURE(status = acpi_AppendBufferResource(&crsbuf, &resbuf))) {
	device_printf(pcib, "couldn't route interrupt %d via %s, interupt resource build failed - %s\n",
		      prsres->Data.Irq.Interrupts[0], acpi_name(lnkdev), AcpiFormatException(status));
	goto out;
    }
    if (ACPI_FAILURE(status = AcpiSetCurrentResources(lnkdev, &crsbuf))) {
	device_printf(pcib, "couldn't route interrupt %d via %s - %s\n",
		      prsres->Data.Irq.Interrupts[0], acpi_name(lnkdev), AcpiFormatException(status));
	goto out;
    }
    
    /* successful, return the interrupt we just routed */
    device_printf(pcib, "routed interrupt %d via %s\n", 
		  prsres->Data.Irq.Interrupts[0], acpi_name(lnkdev));
    interrupt = prsres->Data.Irq.Interrupts[0];

 out:
    if (crsbuf.Pointer != NULL)
	AcpiOsFree(crsbuf.Pointer);
    if (prsbuf.Pointer != NULL)
	AcpiOsFree(prsbuf.Pointer);

    /* XXX APIC_IO interrupt mapping? */
    return_VALUE(interrupt);
}

