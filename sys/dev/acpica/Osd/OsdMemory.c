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
AcpiOsMapMemory (void *PhysicalAddress, UINT32 Length, void **LogicalAddress)
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

