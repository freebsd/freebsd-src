/*-
 * Copyright (c) 2000 Mitsaru Iwasaki
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
 * 6.2 : Memory Management
 */

#include "acpi.h"

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>

MALLOC_DEFINE(M_ACPICA, "acpica", "ACPI CA memory pool");

void *
AcpiOsAllocate(UINT32 Size)
{
    return(malloc(Size, M_ACPICA, M_NOWAIT));
}

void *
AcpiOsCallocate (UINT32 Size)
{
    void	*alloc;

    alloc = malloc(Size, M_ACPICA, M_NOWAIT);
    if (alloc != NULL)
	bzero(alloc, Size);
    return(alloc);
}

void
AcpiOsFree (void *Memory)
{
    free(Memory, M_ACPICA);
}

ACPI_STATUS
AcpiOsMapMemory (ACPI_PHYSICAL_ADDRESS PhysicalAddress, UINT32 Length, void **LogicalAddress)
{
    *LogicalAddress = pmap_mapdev((vm_offset_t)PhysicalAddress, Length);
    if (*LogicalAddress == NULL)
	return(AE_BAD_ADDRESS);
    return(AE_OK);
}

void
AcpiOsUnmapMemory (void *LogicalAddress, UINT32 Length)
{
    pmap_unmapdev((vm_offset_t)LogicalAddress, Length);
}

/*
 * There is no clean way to do this.  We make the charitable assumption
 * that callers will not pass garbage to us.
 */
BOOLEAN
AcpiOsReadable (void *Pointer, UINT32 Length)
{
    return(TRUE);
}

BOOLEAN
AcpiOsWritable (void *Pointer, UINT32 Length)
{
    return(TRUE);
}

static __inline
UINT32
AcpiOsMemInX (UINT32 Length, ACPI_PHYSICAL_ADDRESS InAddr)
{
    UINT32	Value;
    void	*LogicalAddress;

    if (AcpiOsMapMemory(InAddr, Length, &LogicalAddress) != AE_OK) {
	return(0);
    }

    switch (Length) {
    case 1:
	Value = (*(volatile u_int8_t *)LogicalAddress) & 0xff;
	break;
    case 2:
	Value = (*(volatile u_int16_t *)LogicalAddress) & 0xffff;
	break;
    case 4:
	Value = (*(volatile u_int32_t *)LogicalAddress);
	break;
    }

    AcpiOsUnmapMemory(LogicalAddress, Length);

    return(Value);
}

UINT8
AcpiOsMemIn8 (ACPI_PHYSICAL_ADDRESS InAddr)
{
    return((UINT8)AcpiOsMemInX(1, InAddr));
}
 
UINT16
AcpiOsMemIn16 (ACPI_PHYSICAL_ADDRESS InAddr)
{
    return((UINT16)AcpiOsMemInX(2, InAddr));
}
 
UINT32
AcpiOsMemIn32 (ACPI_PHYSICAL_ADDRESS InAddr)
{
    return((UINT32)AcpiOsMemInX(4, InAddr));
}
 
static __inline
void
AcpiOsMemOutX (UINT32 Length, ACPI_PHYSICAL_ADDRESS OutAddr, UINT32 Value)
{
    void	*LogicalAddress;

    if (AcpiOsMapMemory(OutAddr, Length, &LogicalAddress) != AE_OK) {
	return;
    }

    switch (Length) {
    case 1:
	(*(volatile u_int8_t *)LogicalAddress) = Value & 0xff;
	break;
    case 2:
	(*(volatile u_int16_t *)LogicalAddress) = Value & 0xffff;
	break;
    case 4:
	(*(volatile u_int32_t *)LogicalAddress) = Value;
	break;
    }

    AcpiOsUnmapMemory(LogicalAddress, Length);
}

void
AcpiOsMemOut8 (ACPI_PHYSICAL_ADDRESS OutAddr, UINT8 Value)
{
    AcpiOsMemOutX(1, OutAddr, (UINT32)Value);
}
 
void
AcpiOsMemOut16 (ACPI_PHYSICAL_ADDRESS OutAddr, UINT16 Value)
{
    AcpiOsMemOutX(2, OutAddr, (UINT32)Value);
}

void
AcpiOsMemOut32 (ACPI_PHYSICAL_ADDRESS OutAddr, UINT32 Value)
{
    AcpiOsMemOutX(4, OutAddr, (UINT32)Value);
}

