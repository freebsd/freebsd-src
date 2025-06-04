/*-
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

#include <stand.h>

#include <acpi.h>
#include <accommon.h>
#include <acoutput.h>

#include <init_acpi.h>

#define _COMPONENT ACPI_LOADER
ACPI_MODULE_NAME("init_acpi");

/* Holds the description of the acpi0 device. */
static char acpi_desc[ACPI_OEM_ID_SIZE + ACPI_OEM_TABLE_ID_SIZE + 2];

/* Holds the current ACPICA version. */
static char acpi_ca_version[12];

/* For ACPI rsdp discovery. */
EFI_GUID acpi = ACPI_TABLE_GUID;
EFI_GUID acpi20 = ACPI_20_TABLE_GUID;
ACPI_TABLE_RSDP *rsdp;

/* Singleton protection. */
static int acpi_inited = 0;

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

/*
 * Intialize the ACPI subsystem and tables.
 */
ACPI_STATUS
acpi_Startup(void)
{
	ACPI_STATUS status;
	
	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
	
	/* Initialize the ACPICA subsystem. */
	if (ACPI_FAILURE(status = AcpiInitializeSubsystem())) {
		printf("ACPI: Could not initialize Subsystem: %s\n",
		    AcpiFormatException(status));
		return_VALUE (status);
	}
	
	/* Initialize the ACPICA tables. */
	if (ACPI_FAILURE(status = AcpiInitializeTables(NULL, 2, TRUE))) {
		printf("ACPI: Table initialisation failed: %s\n",
		    AcpiFormatException(status));
		return_VALUE (status);
	}

	/* Enable the ACPI subsystem. */
	uint32_t flags = ACPI_REDUCED_HARDWARE | ACPI_NO_ACPI_ENABLE;
	if (ACPI_FAILURE(status = AcpiEnableSubsystem(flags))) {
		printf("ACPI: Enable subsystem failed: %s\n",
		    AcpiFormatException(status));
		return_VALUE (status);
	}

	/* Create the ACPI namespace from ACPI tables. */
	if (ACPI_FAILURE(status = AcpiLoadTables())) {
		printf("ACPI: Load tables failed: %s\n",
		    AcpiFormatException(status));
		return_VALUE (status);
	}

	return_VALUE (status);
}

/*
 * Detect ACPI and perform early initialisation.
 */
int
init_acpi(void)
{
	ACPI_STATUS status;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (acpi_inited) {
		return 0;
	}
	
	/* Initialize root tables. */
	if (ACPI_FAILURE(status = acpi_Startup())) {
		printf("ACPI: Startup failed with status %s\n", AcpiFormatException(status));
		printf("ACPI: Try disabling either ACPI or apic support.\n");
		return (ENXIO);
	}
	
	acpi_inited = 1;
	
	return (0);
}

/*
 * Confirm ACPI initialization.
 */
int
acpi_is_initialized(void) 
{
	return acpi_inited;
}
