/*-
 * Copyright (c) 2014 Ed Maste <emaste@freebsd.org>
 * Copyright (c) 2025 Kayla Powell <kpowkitty@FreeBSD.org>
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

#include <machine/_inttypes.h>
#include <efi.h>
#include <Guid/Acpi.h>
#include <acpi.h>
#include "acpi_detect.h"

/* For ACPI rsdp discovery. */
EFI_GUID acpi = ACPI_TABLE_GUID;
EFI_GUID acpi20 = EFI_ACPI_TABLE_GUID;
ACPI_TABLE_RSDP *rsdp;

void
acpi_detect(void)
{
	char buf[24];
	int revision;

	feature_enable(FEATURE_EARLY_ACPI);
	if ((rsdp = efi_get_table(&acpi20)) == NULL)
		if ((rsdp = efi_get_table(&acpi)) == NULL)
			return;

	sprintf(buf, "0x%016"PRIxPTR, (uintptr_t)rsdp);
	setenv("acpi.rsdp", buf, 1);
	revision = rsdp->Revision;
	if (revision == 0)
		revision = 1;
	sprintf(buf, "%d", revision);
	setenv("acpi.revision", buf, 1);
	strncpy(buf, rsdp->OemId, sizeof(rsdp->OemId));
	buf[sizeof(rsdp->OemId)] = '\0';
	setenv("acpi.oem", buf, 1);
	sprintf(buf, "0x%016x", rsdp->RsdtPhysicalAddress);
	setenv("acpi.rsdt", buf, 1);
	if (revision >= 2) {
		/* XXX extended checksum? */
		sprintf(buf, "0x%016llx",
		    (unsigned long long)rsdp->XsdtPhysicalAddress);
		setenv("acpi.xsdt", buf, 1);
		sprintf(buf, "%d", rsdp->Length);
		setenv("acpi.xsdt_length", buf, 1);
	}
}
