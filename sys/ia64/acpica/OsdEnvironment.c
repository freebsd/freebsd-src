/*-
 * Copyright (c) 2000,2001 Michael Smith
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
 * 6.1 : Environmental support
 */
#include <sys/types.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>

#include "acpi.h"

extern u_int64_t ia64_efi_acpi_table;
extern u_int64_t ia64_efi_acpi20_table;

static u_long ia64_acpi_root;

SYSCTL_ULONG(_machdep, OID_AUTO, acpi_root, CTLFLAG_RD, &ia64_acpi_root, 0,
	     "The physical address of the RSDP");

ACPI_STATUS
AcpiOsInitialize(void)
{
	return(AE_OK);
}

ACPI_STATUS
AcpiOsTerminate(void)
{
	return(AE_OK);
}

ACPI_STATUS
AcpiOsGetRootPointer(UINT32 Flags, ACPI_POINTER *RsdpAddress)
{
	if (ia64_acpi_root == 0) {
		if (ia64_efi_acpi20_table) {
			/* XXX put under bootverbose. */
			printf("Using ACPI2.0 table at 0x%lx\n",
			       ia64_efi_acpi20_table);
			ia64_acpi_root = ia64_efi_acpi20_table;
		} else if (ia64_efi_acpi_table) {
			/* XXX put under bootverbose. */
			printf("Using ACPI1.x table at 0x%lx\n",
			       ia64_efi_acpi_table);
			ia64_acpi_root = ia64_efi_acpi_table;
		} else
			return (AE_NOT_FOUND);
	}

	RsdpAddress->PointerType = ACPI_PHYSICAL_POINTER;
	RsdpAddress->Pointer.Physical = ia64_acpi_root;
	return (AE_OK);
}
