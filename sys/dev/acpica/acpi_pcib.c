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
#include <sys/malloc.h>
#include <sys/kernel.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>

#include <dev/pci/pcivar.h>
#include "pcib_if.h"

/* Hooks for the ACPI CA debugging infrastructure. */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI")

ACPI_SERIAL_DECL(pcib, "ACPI PCI bus methods");

/*
 * For locking, we assume the caller is not concurrent since this is
 * triggered by newbus methods.
 */
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
     * Get the PCI interrupt routing table for this bus.  If we can't
     * get it, this is not an error but may reduce functionality.  There
     * are several valid bridges in the field that do not have a _PRT, so
     * only warn about missing tables if bootverbose is set.
     */
    prt->Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiGetIrqRoutingTable(acpi_get_handle(dev), prt);
    if (ACPI_FAILURE(status) && (bootverbose || status != AE_NOT_FOUND))
	device_printf(dev,
	    "could not get PCI interrupt routing table for %s - %s\n",
	    acpi_name(acpi_get_handle(dev)), AcpiFormatException(status));

    /*
     * Attach the PCI bus proper.
     */
    if ((child = device_add_child(dev, "pci", busno)) == NULL) {
	device_printf(device_get_parent(dev), "couldn't attach pci bus\n");
	return_VALUE(ENXIO);
    }

    /*
     * Now go scan the bus.
     */
    acpi_pci_link_config(dev, prt, busno);

    return_VALUE (bus_generic_attach(dev));
}

int
acpi_pcib_resume(device_t dev)
{
    acpi_pci_link_resume(dev);
    return (bus_generic_resume(dev));
}

/*
 * Route an interrupt for a child of the bridge.
 */
int
acpi_pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{
    struct acpi_prt_entry	*entry;
    int				i, interrupt;
    struct acpi_pci_link_entry	*link;
    ACPI_PCI_ROUTING_TABLE	*prt;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    interrupt = PCI_INVALID_IRQ;

    /* ACPI numbers pins 0-3, not 1-4 like the BIOS. */
    pin--;

    ACPI_SERIAL_BEGIN(pcib);

    /* Look up the PRT entry for this device. */
    entry = acpi_pci_find_prt(pcib, dev, pin);
    if (entry == NULL) {
	device_printf(pcib, "no PRT entry for %d.%d.INT%c\n", pci_get_bus(dev),
	    pci_get_slot(dev), 'A' + pin);
	goto out;
    }
    prt = &entry->prt;
    link = entry->pci_link;
    if (bootverbose) {
	device_printf(pcib, "matched entry for %d.%d.INT%c",
	    pci_get_bus(dev), pci_get_slot(dev), 'A' + pin);
	if (prt->Source[0] != '\0')
	    printf(" (src %s)", acpi_name(entry->prt_source));
	printf("\n");
    }

    /*
     * If source is empty/NULL, the source index is a global IRQ number
     * and it's hard-wired so we're done.
     */
    if (prt->Source == NULL || prt->Source[0] == '\0') {
	if (bootverbose)
	    device_printf(pcib, "slot %d INT%c hardwired to IRQ %d\n",
		pci_get_slot(dev), 'A' + pin, prt->SourceIndex);
	if (prt->SourceIndex)
	    interrupt = prt->SourceIndex;
	else
	    device_printf(pcib, "error: invalid hard-wired IRQ of 0\n");
	goto out;
    }

    /* XXX Support for multiple resources must be added to the link code. */
    if (prt->SourceIndex) {
	device_printf(pcib, "src index %d not yet supported\n",
	    prt->SourceIndex);
	goto out;
    }

    /* There has to be at least one interrupt available. */
    if (link->number_of_interrupts == 0) {
	device_printf(pcib, "device has no interrupts\n");
	goto out;
    }

    /* 
     * If the current interrupt has been routed, we're done.  This is the
     * case when the BIOS initializes it and we didn't disable it.
     */
    if (link->flags & ACPI_LINK_ROUTED) {
	interrupt = link->current_irq;
	if (bootverbose)
	    device_printf(pcib, "slot %d INT%c is already routed to irq %d\n",
		pci_get_slot(dev), 'A' + pin, interrupt);
	goto out;
    }

    if (bootverbose) {
	device_printf(pcib, "possible interrupts:");
	for (i = 0; i < link->number_of_interrupts; i++)
	    printf("%3d", link->interrupts[i]);
	printf("\n");
    }

    /*
     * Perform the link routing.  The link code will pick the best IRQ
     * for this pin and configure it.
     */
    interrupt = acpi_pci_link_route(dev, entry);

    if (bootverbose && PCI_INTERRUPT_VALID(interrupt))
	device_printf(pcib, "slot %d INT%c routed to irq %d via %s\n",
	    pci_get_slot(dev), 'A' + pin, interrupt,
	    acpi_name(entry->prt_source));

out:
    ACPI_SERIAL_END(pcib);

    return_VALUE (interrupt);
}
