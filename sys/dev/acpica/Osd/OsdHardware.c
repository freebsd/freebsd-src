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
 * 6.7 : Hardware Abstraction
 */

#include "acpi.h"

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/pci_cfgreg.h>

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

#define ACPI_BUS_SPACE_IO	I386_BUS_SPACE_IO
#define ACPI_BUS_HANDLE		0

UINT8
AcpiOsIn8(ACPI_IO_ADDRESS InPort)
{
    return(bus_space_read_1(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, InPort));
}

UINT16
AcpiOsIn16(ACPI_IO_ADDRESS InPort)
{
    return(bus_space_read_2(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, InPort));
}

UINT32
AcpiOsIn32(ACPI_IO_ADDRESS InPort)
{
    return(bus_space_read_4(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, InPort));
}

void
AcpiOsOut8(ACPI_IO_ADDRESS OutPort, UINT8 Value)
{
    bus_space_write_1(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, OutPort, Value);
}

void
AcpiOsOut16(ACPI_IO_ADDRESS OutPort, UINT16 Value)
{
    bus_space_write_2(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, OutPort, Value);
}

void
AcpiOsOut32(ACPI_IO_ADDRESS OutPort, UINT32 Value)
{
    bus_space_write_4(ACPI_BUS_SPACE_IO, ACPI_BUS_HANDLE, OutPort, Value);
}

ACPI_STATUS
AcpiOsReadPciCfgByte (UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT8 *Value)
{
    u_int32_t	result;

    if (!pci_cfgregopen())
	return(AE_NOT_EXIST);
    result = pci_cfgregread(Bus, DeviceFunction >> 16, DeviceFunction & 0xff, Register, 1);
    *Value = (UINT8)result;
    return(AE_OK);
}

ACPI_STATUS
AcpiOsReadPciCfgWord (UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT16 *Value)
{
    u_int32_t	result;

    if (!pci_cfgregopen())
	return(AE_NOT_EXIST);
    result = pci_cfgregread(Bus, DeviceFunction >> 16, DeviceFunction & 0xff, Register, 2);
    *Value = (UINT16)result;
    return(AE_OK);
}

ACPI_STATUS
AcpiOsReadPciCfgDword (UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT32 *Value)
{
    if (!pci_cfgregopen())
	return(AE_NOT_EXIST);
    *Value = pci_cfgregread(Bus, DeviceFunction >> 16, DeviceFunction & 0xff, Register, 4);
    return(AE_OK);
}

ACPI_STATUS
AcpiOsWritePciCfgByte (UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT8 Value)
{
    if (!pci_cfgregopen())
	return(AE_NOT_EXIST);
    pci_cfgregwrite(Bus, DeviceFunction >> 16, DeviceFunction & 0xff, Register, (u_int32_t)Value, 1);
    return(AE_OK);
}

ACPI_STATUS
AcpiOsWritePciCfgWord (UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT16 Value)
{
    if (!pci_cfgregopen())
	return(AE_NOT_EXIST);
    pci_cfgregwrite(Bus, DeviceFunction >> 16, DeviceFunction & 0xff, Register, (u_int32_t)Value, 2);
    return(AE_OK);
}

ACPI_STATUS
AcpiOsWritePciCfgDword (UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT32 Value)
{
    if (!pci_cfgregopen())
	return(AE_NOT_EXIST);
    pci_cfgregwrite(Bus, DeviceFunction >> 16, DeviceFunction & 0xff, Register, (u_int32_t)Value, 4);
    return(AE_OK);
}
