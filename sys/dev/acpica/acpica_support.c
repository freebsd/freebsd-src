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
 *
 * $FreeBSD$
 */

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

    AcpiHwBitRegisterWrite (ACPI_BITREG_WAKE_STATUS, 1, ACPI_MTX_LOCK);

    acpi_disable_irqs ();

    AcpiHwDisableNonWakeupGpes();

    /* flush caches */

#ifdef __i386__
    wbinvd();
#endif

    /* write the value to command port and wait until we enter sleep state */
    do
    {
        AcpiOsStall(1000000);
        AcpiOsWritePort (AcpiGbl_FADT->SmiCmd, AcpiGbl_FADT->S4BiosReq, 8);
    }
    while (!AcpiHwBitRegisterRead (ACPI_BITREG_WAKE_STATUS, ACPI_MTX_LOCK));

    AcpiHwEnableNonWakeupGpes();

    acpi_enable_irqs ();

    return_ACPI_STATUS (AE_OK);
}


#undef _COMPONENT
#define _COMPONENT	ACPI_TABLES

/*******************************************************************************
 *
 * FUNCTION:    AcpiSetDsdtTablePtr
 *
 * PARAMETERS:  TablePtr        - pointer to a buffer containing the entire
 *                                DSDT table to override
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Set DSDT table ptr for DSDT overriding.  This function should
 *              be called perior than AcpiLoadTables(). 
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetDsdtTablePtr(
    ACPI_TABLE_HEADER   *TablePtr)
{
    ACPI_FUNCTION_TRACE ("AcpiSetDsdtTablePtr");

    if (!TablePtr)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (AcpiGbl_AcpiTables[ACPI_TABLE_DSDT].LoadedIntoNamespace)
    {
        return_ACPI_STATUS (AE_ALREADY_EXISTS);
    }

    AcpiGbl_DSDT = TablePtr;

    return_ACPI_STATUS (AE_OK);
}

