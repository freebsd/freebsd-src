/*-
 * Copyright (c) 2001 Mitsuru IWASAKI
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "acpi.h"
#include <machine/cpufunc.h>
#include <dev/acpica/acpica_support.h>

ACPI_MODULE_NAME("SUPPORT")

/*
 * Implement support code temporary here until officially merged into
 * Intel ACPI CA release.
 */

#undef _COMPONENT
#define _COMPONENT	ACPI_HARDWARE

/******************************************************************************
 *
 * FUNCTION:    AcpiEnterSleepStateS4Bios
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter a system sleep state S4BIOS
 *
 ******************************************************************************/
        
ACPI_STATUS
AcpiEnterSleepStateS4Bios (
    void)
{
    ACPI_OBJECT_LIST    ArgList;
    ACPI_OBJECT         Arg;
    UINT32              Value;


    ACPI_FUNCTION_TRACE ("AcpiEnterSleepStateS4Bios");

    /* run the _PTS and _GTS methods */

    ACPI_MEMSET(&ArgList, 0, sizeof(ArgList));
    ArgList.Count = 1;
    ArgList.Pointer = &Arg;

    ACPI_MEMSET(&Arg, 0, sizeof(Arg));
    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = ACPI_STATE_S4;

    AcpiEvaluateObject (NULL, "\\_PTS", &ArgList, NULL);
    AcpiEvaluateObject (NULL, "\\_GTS", &ArgList, NULL);

    /* clear wake status */

    AcpiSetRegister (ACPI_BITREG_WAKE_STATUS, 1, ACPI_MTX_LOCK);

    ACPI_DISABLE_IRQS ();

    AcpiHwDisableNonWakeupGpes();

    /* flush caches */

    ACPI_FLUSH_CPU_CACHE ();

    /* write the value to command port and wait until we enter sleep state */
    do
    {
        AcpiOsStall(1000000);
        AcpiOsWritePort (AcpiGbl_FADT->SmiCmd, AcpiGbl_FADT->S4BiosReq, 8);
        AcpiGetRegister (ACPI_BITREG_WAKE_STATUS, &Value, ACPI_MTX_LOCK);
    }
    while (!Value);

    AcpiHwEnableNonWakeupGpes();

    ACPI_ENABLE_IRQS ();

    return_ACPI_STATUS (AE_OK);
}

