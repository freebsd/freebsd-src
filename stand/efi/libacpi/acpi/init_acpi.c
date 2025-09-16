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
#include <init_acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acoutput.h>

#define _COMPONENT ACPI_LOADER
ACPI_MODULE_NAME("init_acpi");

/* Singleton protection. */
static int acpi_inited = 0;

/*
 * Initialize a smaller subset of the ACPI subsystem in the
 * loader. This subset is enough for users to evaluate
 * objects on the namespace, and includes event management.
 *
 * This function brings up the ACPICA core in four stages:
 * 1. Initialize the ACPICA subsystem.
 * 2. Initialize and validate ACPI root tables.
 * 3. Enable the subsystem (without full hardware mode, but events
 *     enabled).
 * 4. Load ACPI tables into the namespace.
 *
 * Returns ACPI_STATUS; failure indicates issue reading RSDT/XSDT.
 * Please refer to your motherboard manual to verify ACPI support.
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
		return_VALUE(status);
	}

	/* Initialize the ACPICA tables. */
	if (ACPI_FAILURE(status = AcpiInitializeTables(NULL, 2, TRUE))) {
		printf("ACPI: Table initialisation failed: %s\n",
		    AcpiFormatException(status));
		return_VALUE(status);
	}

	/* Enable the ACPI subsystem. */
	uint32_t flags = ACPI_REDUCED_HARDWARE | ACPI_NO_ACPI_ENABLE;
	if (ACPI_FAILURE(status = AcpiEnableSubsystem(flags))) {
		printf("ACPI: Enable subsystem failed: %s\n",
		    AcpiFormatException(status));
		return_VALUE(status);
	}

	/* Create the ACPI namespace from ACPI tables. */
	if (ACPI_FAILURE(status = AcpiLoadTables())) {
		printf("ACPI: Load tables failed: %s\n",
		    AcpiFormatException(status));
		return_VALUE(status);
	}

	return_VALUE(status);
}

/*
 * Initialize ACPI once for the loader.
 *
 * This is a singleton entry point. Safe to call multiple times;
 * subsequent calls will return success immediately.
 *
 * Returns 0 on success, ENXIO if ACPI could not be initialized.
 */
int
init_acpi(void)
{
	ACPI_STATUS status;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (acpi_inited) {
		return (0);
	}

	if (ACPI_FAILURE(status = acpi_Startup())) {
		printf("ACPI: Loader initialization failed with status %s\n",
		    AcpiFormatException(status));
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
	return (acpi_inited);
}
