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

#include <sys/kernel.h>
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

/*
 * Some BIOS vendors use AML to read/write directly to IO space.  This
 * can cause a problem if such accesses interfere with the OS's access to
 * the same ports.  Windows XP and newer systems block accesses to certain
 * IO ports.  We print a message or block accesses based on a tunable.
 */
static int illegal_bios_ports[] = {
	0x000, 0x00f,	/* DMA controller 1 */
	0x020, 0x021,	/* PIC */
	0x040, 0x043,	/* Timer 1 */
	0x048, 0x04b,	/* Timer 2 failsafe */
	0x070, 0x071,	/* CMOS and RTC */
	0x074, 0x076,	/* Extended CMOS */
	0x081, 0x083,	/* DMA1 page registers */
	0x087, 0x087,	/* DMA1 ch0 low page */
	0x089, 0x08b,	/* DMA2 ch2 (0x89), ch3 low page (0x8a, 0x8b) */
	0x08f, 0x091,	/* DMA2 low page refresh (0x8f) */
			/* Arb ctrl port, card select feedback (0x90, 0x91) */
	0x093, 0x094,	/* System board setup */
	0x096, 0x097,	/* POS channel select */
	0x0a0, 0x0a1,	/* PIC (cascaded) */
	0x0c0, 0x0df,	/* ISA DMA */
	0x4d0, 0x4d1,	/* PIC ELCR (edge/level control) */
	0xcf8, 0xcff,	/* PCI config space. Microsoft adds 0xd00 also but
			   that seems incorrect. */
	-1, -1
};

/* Block accesses to bad IO port addresses or just print a warning. */
static int block_bad_io;
TUNABLE_INT("debug.acpi.block_bad_io", &block_bad_io);

/*
 * Look up bad ports in our table.  Returns 0 if ok, 1 if marked bad but
 * access is still allowed, or -1 to deny access.
 */
static int
acpi_os_check_port(UINT32 addr, UINT32 width)
{
	int error, *port;

	error = 0;
	for (port = illegal_bios_ports; *port != -1; port += 2) {
		if ((addr >= port[0] && addr <= port[1]) ||
		    (addr < port[0] && addr + (width / 8) > port[0])) {
			if (block_bad_io)
			    error = -1;
			else
			    error = 1;
			break;
		}
	}

	return (error);
}

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS InPort, UINT32 *Value, UINT32 Width)
{
    int error;

    error = acpi_os_check_port(InPort, Width);
    if (error != 0) {
	if (bootverbose)
		printf("acpi: bad read from port 0x%03x (%d)\n",
			(int)InPort, Width);
	if (error == -1)
	    return (AE_BAD_PARAMETER);
    }

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
    int error;

    error = acpi_os_check_port(OutPort, Width);
    if (error != 0) {
	if (bootverbose)
		printf("acpi: bad write to port 0x%03x (%d), val %#x\n",
			(int)OutPort, Width, Value);
	if (error == -1)
	    return (AE_BAD_PARAMETER);
    }

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
