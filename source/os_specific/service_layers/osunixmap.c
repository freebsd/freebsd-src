/******************************************************************************
 *
 * Module Name: osunixmap - Unix OSL for file mappings
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "acpidump.h"
#include <unistd.h>
#include <sys/mman.h>
#ifdef _FreeBSD
#include <sys/param.h>
#endif

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("osunixmap")


#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined(_DragonFly) || defined(_FreeBSD) || defined(_QNX)
#define MMAP_FLAGS          MAP_SHARED
#else
#define MMAP_FLAGS          MAP_PRIVATE
#endif

#define SYSTEM_MEMORY       "/dev/mem"


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsGetPageSize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Page size of the platform.
 *
 * DESCRIPTION: Obtain page size of the platform.
 *
 ******************************************************************************/

static ACPI_SIZE
AcpiOsGetPageSize (
    void)
{

#ifdef PAGE_SIZE
    return PAGE_SIZE;
#else
    return sysconf (_SC_PAGESIZE);
#endif
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsMapMemory
 *
 * PARAMETERS:  Where               - Physical address of memory to be mapped
 *              Length              - How much memory to map
 *
 * RETURN:      Pointer to mapped memory. Null on error.
 *
 * DESCRIPTION: Map physical memory into local address space.
 *
 *****************************************************************************/

void *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   Where,
    ACPI_SIZE               Length)
{
    UINT8                   *MappedMemory;
    ACPI_PHYSICAL_ADDRESS   Offset;
    ACPI_SIZE               PageSize;
    int                     fd;


    fd = open (SYSTEM_MEMORY, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr, "Cannot open %s\n", SYSTEM_MEMORY);
        return (NULL);
    }

    /* Align the offset to use mmap */

    PageSize = AcpiOsGetPageSize ();
    Offset = Where % PageSize;

    /* Map the table header to get the length of the full table */

    MappedMemory = mmap (NULL, (Length + Offset), PROT_READ, MMAP_FLAGS,
        fd, (Where - Offset));
    if (MappedMemory == MAP_FAILED)
    {
        fprintf (stderr, "Cannot map %s\n", SYSTEM_MEMORY);
        close (fd);
        return (NULL);
    }

    close (fd);
    return (ACPI_CAST8 (MappedMemory + Offset));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsUnmapMemory
 *
 * PARAMETERS:  Where               - Logical address of memory to be unmapped
 *              Length              - How much memory to unmap
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete a previously created mapping. Where and Length must
 *              correspond to a previous mapping exactly.
 *
 *****************************************************************************/

void
AcpiOsUnmapMemory (
    void                    *Where,
    ACPI_SIZE               Length)
{
    ACPI_PHYSICAL_ADDRESS   Offset;
    ACPI_SIZE               PageSize;


    PageSize = AcpiOsGetPageSize ();
    Offset = ACPI_TO_INTEGER (Where) % PageSize;
    munmap ((UINT8 *) Where - Offset, (Length + Offset));
}
