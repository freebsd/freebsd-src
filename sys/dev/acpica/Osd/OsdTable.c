/*-
 * Copyright (c) 2002 Mitsaru Iwasaki
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
 * ACPI Table interfaces
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/linker.h>

#include "acpi.h"
#include <contrib/dev/acpica/actables.h>

#undef _COMPONENT
#define _COMPONENT      ACPI_TABLES

static char acpi_osname[128];
TUNABLE_STR("hw.acpi.osname", acpi_osname, sizeof(acpi_osname));

static struct {
    ACPI_TABLE_HEADER_DEF
    uint32_t no_op;
} __packed fake_ssdt;

ACPI_STATUS
AcpiOsPredefinedOverride (
    const ACPI_PREDEFINED_NAMES *InitVal,
    ACPI_STRING                 *NewVal)
{
    if (InitVal == NULL || NewVal == NULL)
	return (AE_BAD_PARAMETER);

    *NewVal = NULL;
    if (strncmp(InitVal->Name, "_OS_", 4) == 0 && strlen(acpi_osname) > 0) {
	printf("ACPI: Overriding _OS definition with \"%s\"\n", acpi_osname);
	*NewVal = acpi_osname;
    }

    return (AE_OK);
}

ACPI_STATUS
AcpiOsTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable)
{
    caddr_t acpi_dsdt, p;

    if (ExistingTable == NULL || NewTable == NULL)
	return (AE_BAD_PARAMETER);

    /* If we're not overriding the DSDT, just return. */
    *NewTable = NULL;
    if ((acpi_dsdt = preload_search_by_type("acpi_dsdt")) == NULL)
	return (AE_OK);
    if ((p = preload_search_info(acpi_dsdt, MODINFO_ADDR)) == NULL)
	return (AE_OK);

    /*
     * Override the DSDT with the user's custom version.  Override the
     * contents of any SSDTs with a simple no-op table since the user's
     * DSDT is expected to contain their contents as well.
     */
    if (strncmp(ExistingTable->Signature, "DSDT", 4) == 0) {
	printf("ACPI: overriding DSDT/SSDT with custom table\n");
	*NewTable = *(void **)p;
    } else if (strncmp(ExistingTable->Signature, "SSDT", 4) == 0) {
	if (fake_ssdt.Length == 0) {
	    sprintf(fake_ssdt.Signature, "%.4s", "SSDT");
	    fake_ssdt.Length = htole32(sizeof(fake_ssdt));
	    fake_ssdt.Revision = 2;
	    fake_ssdt.Checksum = 0;
	    sprintf(fake_ssdt.OemId, "%.6s", "FBSD  ");
	    sprintf(fake_ssdt.OemTableId, "%.8s", "NullSSDT");
	    fake_ssdt.OemRevision = htole32(1);
	    sprintf(fake_ssdt.AslCompilerId, "%.4s", "FBSD");
	    fake_ssdt.AslCompilerRevision = htole32(1);
	    fake_ssdt.no_op = htole32(0x005c0310); /* Scope(\) */
	    fake_ssdt.Checksum -= AcpiTbChecksum(&fake_ssdt, sizeof(fake_ssdt));
	}
	*NewTable = (void *)&fake_ssdt;
    }

    return (AE_OK);
}
