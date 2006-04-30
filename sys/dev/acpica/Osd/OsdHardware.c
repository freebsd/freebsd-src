/*-
 * Copyright (c) 2000, 2001 Michael Smith
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

/*
 * 6.7 : Hardware Abstraction
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <contrib/dev/acpica/acpi.h>

#include <machine/bus.h>
#include <machine/pci_cfgreg.h>
#include <dev/pci/pcireg.h>

/*
 * ACPICA's rather gung-ho approach to hardware resource ownership is a little
 * troublesome insofar as there is no easy way for us to know in advance 
 * exactly which I/O resources it's going to want to use.
 * 
 * In order to deal with this, we ignore resource ownership entirely, and simply
 * use the native I/O space accessor functionality.  This is Evil, but it works.
 *
 * XXX use an intermediate #define for the tag/handle
 */

#ifdef __i386__
#define ACPI_BUS_SPACE_IO	I386_BUS_SPACE_IO
#define ACPI_BUS_HANDLE		0
#endif
#ifdef __ia64__
#define ACPI_BUS_SPACE_IO	IA64_BUS_SPACE_IO
#define ACPI_BUS_HANDLE		0
#endif
#ifdef __amd64__
#define ACPI_BUS_SPACE_IO	AMD64_BUS_SPACE_IO
#define ACPI_BUS_HANDLE		0
#endif

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS InPort, UINT32 *Value, UINT32 Width)
{
    switch (Width) {
    case 8:
        *(u_int8_t *)Value = bus_space_read_1(ACPI_BUS_SPACE_IO,
	    ACPI_BUS_HANDLE, InPort);
        break;
    case 16:
        *(u_int16_t *)Value = bus_space_read_2(ACPI_BUS_SPACE_IO,
	    ACPI_BUS_HANDLE, InPort);
        break;
    case 32:
        *(u_int32_t *)Value = bus_space_read_4(ACPI_BUS_SPACE_IO,
	    ACPI_BUS_HANDLE, InPort);
        break;
    default:
        /* debug trap goes here */
	break;
    }

    return (AE_OK);
}

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS OutPort, UINT32	Value, UINT32 Width)
{
    switch (Width) {
    case 8:
        bus_space_write_1(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, OutPort, Value);
        break;
    case 16:
        bus_space_write_2(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, OutPort, Value);
        break;
    case 32:
        bus_space_write_4(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, OutPort, Value);
        break;
    default:
        /* debug trap goes here */
	break;
    }

    return (AE_OK);
}

ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, void *Value,
    UINT32 Width)
{
    u_int32_t	byte_width = Width / 8;
    u_int32_t	val;

    if (!pci_cfgregopen())
        return (AE_NOT_EXIST);

    val = pci_cfgregread(PciId->Bus, PciId->Device, PciId->Function, Register,
	byte_width);
    switch (Width) {
    case 8:
	*(u_int8_t *)Value = val & 0xff;
	break;
    case 16:
	*(u_int16_t *)Value = val & 0xffff;
	break;
    case 32:
	*(u_int32_t *)Value = val;
	break;
    default:
	/* debug trap goes here */
	break;
    }
    
    return (AE_OK);
}


ACPI_STATUS
AcpiOsWritePciConfiguration (ACPI_PCI_ID *PciId, UINT32 Register,
    ACPI_INTEGER Value, UINT32 Width)
{
    u_int32_t	byte_width = Width / 8;

    if (!pci_cfgregopen())
    	return (AE_NOT_EXIST);

    pci_cfgregwrite(PciId->Bus, PciId->Device, PciId->Function, Register,
	Value, byte_width);

    return (AE_OK);
}

/* XXX should use acpivar.h but too many include dependencies */
extern ACPI_STATUS acpi_GetInteger(ACPI_HANDLE handle, char *path, int
    *number);

/*
 * Depth-first recursive case for finding the bus, given the slot/function.
 */
static int
acpi_bus_number(ACPI_HANDLE root, ACPI_HANDLE curr, ACPI_PCI_ID *PciId)
{
    ACPI_HANDLE parent;
    ACPI_STATUS status;
    ACPI_OBJECT_TYPE type;
    UINT32 adr;
    int bus, slot, func, class, subclass, header;

    /* Try to get the _BBN object of the root, otherwise assume it is 0. */
    bus = 0;
    if (root == curr) {
	status = acpi_GetInteger(root, "_BBN", &bus);
	if (ACPI_FAILURE(status) && bootverbose)
	    printf("acpi_bus_number: root bus has no _BBN, assuming 0\n");
	return (bus);
    }
    status = AcpiGetParent(curr, &parent);
    if (ACPI_FAILURE(status))
	return (bus);
    
    /* First, recurse up the tree until we find the host bus. */
    bus = acpi_bus_number(root, parent, PciId);

    /* Validate parent bus device type. */
    if (ACPI_FAILURE(AcpiGetType(parent, &type)) || type != ACPI_TYPE_DEVICE) {
	printf("acpi_bus_number: not a device, type %d\n", type);
	return (bus);
    }

    /* Get the parent's slot and function. */
    status = acpi_GetInteger(parent, "_ADR", &adr);
    if (ACPI_FAILURE(status)) {
	printf("acpi_bus_number: can't get _ADR\n");
	return (bus);
    }
    slot = ACPI_HIWORD(adr);
    func = ACPI_LOWORD(adr);

    /* Is this a PCI-PCI or Cardbus-PCI bridge? */
    class = pci_cfgregread(bus, slot, func, PCIR_CLASS, 1);
    if (class != PCIC_BRIDGE)
	return (bus);
    subclass = pci_cfgregread(bus, slot, func, PCIR_SUBCLASS, 1);

    /* Find the header type, masking off the multifunction bit. */
    header = pci_cfgregread(bus, slot, func, PCIR_HDRTYPE, 1) & PCIM_HDRTYPE;
    if (header == PCIM_HDRTYPE_BRIDGE && subclass == PCIS_BRIDGE_PCI)
	bus = pci_cfgregread(bus, slot, func, PCIR_SECBUS_1, 1);
    if (header == PCIM_HDRTYPE_CARDBUS && subclass == PCIS_BRIDGE_CARDBUS)
	bus = pci_cfgregread(bus, slot, func, PCIR_SECBUS_2, 1);
    return (bus);
}

/*
 * Find the bus number for a device
 *
 * rhandle: handle for the root bus
 * chandle: handle for the device
 * PciId: pointer to device slot and function, we fill out bus
 */
void
AcpiOsDerivePciId(ACPI_HANDLE rhandle, ACPI_HANDLE chandle, ACPI_PCI_ID **PciId)
{
    ACPI_HANDLE parent;
    ACPI_STATUS status;
    int bus;

    if (pci_cfgregopen() == 0)
	panic("AcpiOsDerivePciId unable to initialize pci bus");

    /* Try to read _BBN for bus number if we're at the root */
    bus = 0;
    if (rhandle == chandle) {
	status = acpi_GetInteger(rhandle, "_BBN", &bus);
	if (ACPI_FAILURE(status) && bootverbose)
	    printf("AcpiOsDerivePciId: root bus has no _BBN, assuming 0\n");
    }

    /*
     * Get the parent handle and call the recursive case.  It is not
     * clear why we seem to be getting a chandle that points to a child
     * of the desired slot/function but passing in the parent handle
     * here works.
     */
    if (ACPI_SUCCESS(AcpiGetParent(chandle, &parent)))
	bus = acpi_bus_number(rhandle, parent, *PciId);
    (*PciId)->Bus = bus;
    if (bootverbose) {
	printf("AcpiOsDerivePciId: bus %d dev %d func %d\n",
	    (*PciId)->Bus, (*PciId)->Device, (*PciId)->Function);
    }
}
