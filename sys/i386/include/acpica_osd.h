/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * Copyright (c) 2000 Munehiro Matsuda <haro@tk.kubota.co.jp>
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
 *	$Id: acpica_osd.h,v 1.3 2000/08/09 14:47:48 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/acpi_machdep.h>
#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include "pcib_if.h"

#ifdef ACPI_NO_OSDFUNC_INLINE
#define _ACPICA_INLINE_
#else
#define _ACPICA_INLINE_	static __inline
#endif

/*
 * ACPICA compatibility
 */

static UINT32	OsdInX(ACPI_IO_ADDRESS, int);
static void	OsdOutX(ACPI_IO_ADDRESS, UINT32, int);

/*
 * New-bus dependent code
 */

static device_t	get_pcib_device(void);

static __inline device_t
get_pcib_device()
{
	device_t	nexus, pcib;

	if ((nexus = device_find_child(root_bus, "nexus", 0)) == NULL) {
		return (NULL);
	}
	if ((pcib = device_find_child(nexus, "pcib", 0)) == NULL) {
		return (NULL);
	}

	return (pcib);
}

/*
 * Platform/Hardware independent I/O interfaces
 */

static __inline UINT32
OsdInX(ACPI_IO_ADDRESS InPort, int bytes)
{
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	UINT32			retval;

	bst = ACPI_BUS_SPACE_IO;
	bsh = InPort;
	switch (bytes) {
	case 1:
		retval = bus_space_read_1(bst, bsh, 0);
		break;
	case 2:
		retval = bus_space_read_2(bst, bsh, 0);
		break;
	case 4:
		retval = bus_space_read_4(bst, bsh, 0);
		break;
	default:
		printf("OsdInX: wrong length to read\n");
		retval = 0;
	}
	return (retval);
}

_ACPICA_INLINE_ UINT8
OsdIn8(ACPI_IO_ADDRESS InPort)
{
	return (OsdInX(InPort, 1) & 0xff);
}
 
_ACPICA_INLINE_ UINT16
OsdIn16(ACPI_IO_ADDRESS InPort)
{
	return (OsdInX(InPort, 2) & 0xffff);
}
 
_ACPICA_INLINE_ UINT32
OsdIn32(ACPI_IO_ADDRESS InPort)
{
	return (OsdInX(InPort, 4));
}

static __inline void
OsdOutX(ACPI_IO_ADDRESS OutPort, UINT32 Value, int bytes)
{
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;

	bst = ACPI_BUS_SPACE_IO;
	bsh = OutPort;
	switch (bytes) {
	case 1:
		bus_space_write_1(bst, bsh, 0, Value & 0xff);
		break;
	case 2:
		bus_space_write_2(bst, bsh, 0, Value & 0xffff);
		break;
	case 4:
		bus_space_write_4(bst, bsh, 0, Value);
		break;
	default:
		printf("OsdOutX: wrong length to write\n");
	}
}

_ACPICA_INLINE_ void
OsdOut8(ACPI_IO_ADDRESS OutPort, UINT8 Value)
{
	OsdOutX(OutPort, Value, 1);
}

_ACPICA_INLINE_ void
OsdOut16(ACPI_IO_ADDRESS OutPort, UINT16 Value)
{
	OsdOutX(OutPort, Value, 2);
}

_ACPICA_INLINE_ void
OsdOut32(ACPI_IO_ADDRESS OutPort, UINT32 Value)
{
	OsdOutX(OutPort, Value, 4);
}

_ACPICA_INLINE_ ACPI_STATUS
OsdMapMemory(void *PhysicalAddress, UINT32 Length, void **LogicalAddress)
{
	vm_offset_t	PhysicalEnd;
	vm_offset_t	PhysicalOffs;

	PhysicalEnd = (vm_offset_t)PhysicalAddress + Length;
	if (PhysicalEnd < 1024 * 1024) {
		/*
		 * The first 1Mb is mapped at KERNBASE.
		 */
		*LogicalAddress = (void *)(uintptr_t)
				  (KERNBASE + (vm_offset_t)PhysicalAddress);
	} else {
		PhysicalOffs = (vm_offset_t)PhysicalAddress -
			       trunc_page((vm_offset_t)PhysicalAddress);
		*LogicalAddress = (caddr_t)
		    pmap_mapdev((vm_offset_t)PhysicalAddress - PhysicalOffs,
				Length + PhysicalOffs) +
		    PhysicalOffs;
	}

	return (AE_OK);
}

_ACPICA_INLINE_ void
OsdUnMapMemory(void *LogicalAddress, UINT32 Length)
{

	if ((vm_offset_t)LogicalAddress + Length - KERNBASE >= 1024 * 1024) {
		pmap_unmapdev((vm_offset_t)LogicalAddress, Length);
	}
}

/*
 * Standard access to PCI configuration space
 */

static __inline ACPI_STATUS
OsdReadPciCfg(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT32 *Value, int bytes)
{
	static device_t	pcib = NULL;

	if (pcib == NULL && (pcib = get_pcib_device()) == NULL) {
		return (AE_ERROR);
	}

	*Value = PCIB_READ_CONFIG(pcib, Bus, (DeviceFunction >> 16) & 0xff,
	    DeviceFunction & 0xff, Register, bytes);

	return (AE_OK);
}

_ACPICA_INLINE_ ACPI_STATUS
OsdReadPciCfgByte(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT8 *Value)
{
	ACPI_STATUS	status;
	UINT32		tmp;

	status = OsdReadPciCfg(Bus, DeviceFunction, Register, &tmp, 1);
	*Value = tmp & 0xff;
	return (status);
}

_ACPICA_INLINE_ ACPI_STATUS
OsdReadPciCfgWord(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT16 *Value)
{
	ACPI_STATUS	status;
	UINT32		tmp;

	status = OsdReadPciCfg(Bus, DeviceFunction, Register, &tmp, 2);
	*Value = tmp & 0xffff;
	return (status);
}

_ACPICA_INLINE_ ACPI_STATUS
OsdReadPciCfgDword(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT32 *Value)
{
	ACPI_STATUS	status;
	UINT32		tmp;

	status = OsdReadPciCfg(Bus, DeviceFunction, Register, &tmp, 2);
	*Value = tmp;
	return (status);
}

static __inline ACPI_STATUS
OsdWritePciCfg(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT32 Value, int bytes)
{
	static device_t	pcib = NULL;

	if (pcib == NULL && (pcib = get_pcib_device()) == NULL) {
		return (AE_ERROR);
	}

	PCIB_WRITE_CONFIG(pcib, Bus, (DeviceFunction >> 16) & 0xff,
	    DeviceFunction & 0xff, Register, Value, bytes);

	return (AE_OK);
}

_ACPICA_INLINE_ ACPI_STATUS
OsdWritePciCfgByte(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT8 Value)
{
	return (OsdWritePciCfg(Bus, DeviceFunction, Register, (UINT32) Value, 1));
}

_ACPICA_INLINE_ ACPI_STATUS
OsdWritePciCfgWord(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT16 Value)
{
	return (OsdWritePciCfg(Bus, DeviceFunction, Register, (UINT32) Value, 2));
}

_ACPICA_INLINE_ ACPI_STATUS
OsdWritePciCfgDword(UINT32 Bus, UINT32 DeviceFunction, UINT32 Register, UINT32 Value)
{
	return (OsdWritePciCfg(Bus, DeviceFunction, Register, Value, 4));
}

_ACPICA_INLINE_ ACPI_STATUS
OsdSleep(UINT32 Seconds, UINT32 Milliseconds)
{
	int		error;
	ACPI_STATUS	status;

	error = acpi_sleep(Seconds * 1000 + Milliseconds);
	switch (error) {
	case 0:
		/* The running thread slept for the time specified */
		status = AE_OK;
		break;
	case 1:
		/* TBD!!!! */
		status = AE_BAD_PARAMETER;
		break;
	case 2:
	default:
		/* The running thread did not slept because of a host OS error */
		status = AE_ERROR;
		break;
	}

	return (status);
}

_ACPICA_INLINE_ ACPI_STATUS
OsdSleepUsec(UINT32 Microseconds)
{
	if (Microseconds == 0) {
		return (AE_BAD_PARAMETER);
	}

	DELAY(Microseconds);
	return (AE_OK);
}

