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

/*
 * XXX This is all pretty dubious, since we really want the APIC and co.
 *     up and running long before attaching interrupts, etc.
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

#define APIC_MAGIC	0x43495041	/* "APIC" */

struct acpi_apic_softc {
    device_t		apic_dev;
    IO_APIC		*apic_ioapic;
};

static void		acpi_apic_identify(driver_t *driver, device_t bus);
static int		acpi_apic_probe(device_t dev);
static int		acpi_apic_attach(device_t dev);

static device_method_t acpi_apic_methods[] = {
    /* Device interface */
    DEVMETHOD(device_identify,	acpi_apic_identify),
    DEVMETHOD(device_probe,	acpi_apic_probe),
    DEVMETHOD(device_attach,	acpi_apic_attach),

    {0, 0}
};

static driver_t acpi_apic_driver = {
    "acpi_apic",
    acpi_apic_methods,
    sizeof(struct acpi_apic_softc),
};

devclass_t acpi_apic_devclass;
DRIVER_MODULE(acpi_apic, acpi, acpi_apic_driver, acpi_apic_devclass, 0, 0);

static void
acpi_apic_identify(driver_t *driver, device_t bus)
{
    ACPI_BUFFER		buf;
    ACPI_STATUS		status;
    APIC_HEADER		*hdr;
    APIC_TABLE		*tbl;
    device_t		child;
    int			len;
    void		*private;
    
    /*
     * Perform the tedious double-get to fetch the actual table.
     */
    buf.Length = 0;
    buf.Pointer = NULL;
    if ((status = AcpiGetTable(ACPI_TABLE_APIC, 1, &buf)) != AE_BUFFER_OVERFLOW) {
	if (status != AE_NOT_EXIST)
	    device_printf(bus, "error sizing APIC table - %s\n", acpi_strerror(status));
	return;
    }
    if ((buf.Pointer = AcpiOsAllocate(buf.Length)) == NULL)
	return;
    if ((status = AcpiGetTable(ACPI_TABLE_APIC, 1, &buf)) != AE_OK) {
	device_printf(bus, "error fetching APIC table - %s\n", acpi_strerror(status));
	return;
    }
    
    /*
     * Scan the tables, create child devices for each I/O APIC found
     */
    tbl = (APIC_TABLE *)buf.Pointer;
    len = tbl->header.Length - sizeof(APIC_TABLE);
    hdr = (APIC_HEADER *)((char *)buf.Pointer + sizeof(APIC_TABLE));
    while(len > 0) {
	if (hdr->Length > len) {
	    device_printf(bus, "APIC header corrupt (claims %d bytes where only %d left in structure)\n",
			  hdr->Length, len);
	    break;
	}
	switch (hdr->Type) {
	case APIC_IO:
	    if ((child = BUS_ADD_CHILD(bus, 0, "acpi_apic", -1)) == NULL) {
		device_printf(bus, "could not create I/O APIC device");
		break;
	    }
	    if ((private = AcpiOsAllocate(hdr->Length)) == NULL) {
		device_printf(bus, "could not allocate memory for APIC child");
		break;
	    }
	    bcopy(hdr, private, hdr->Length);
	    acpi_set_magic(child, APIC_MAGIC);
	    acpi_set_private(child, private);
	    device_set_desc(child, "I/O APIC");
	    break;
	}
	len -= hdr->Length;
	hdr = (APIC_HEADER *)((char *)hdr + hdr->Length);
    }

    AcpiOsFree(buf.Pointer);
}

static int
acpi_apic_probe(device_t dev)
{
    if (acpi_get_magic(dev) == APIC_MAGIC)
	return(0);
    return(ENXIO);
}

static int
acpi_apic_attach(device_t dev)
{
    struct acpi_apic_softc	*sc;

    sc = device_get_softc(dev);
    sc->apic_dev = dev;

    /*
     * Fetch our parameters.
     */
    sc->apic_ioapic = acpi_get_private(dev);
    device_printf(dev, "I/O APIC ID %d at 0x%08x vectors 0%x\n",
		  sc->apic_ioapic->IoApicId, sc->apic_ioapic->IoApicAddress, sc->apic_ioapic->Vector);
    return(0);
}
