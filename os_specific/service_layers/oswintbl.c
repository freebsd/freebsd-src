/******************************************************************************
 *
 * Module Name: oswintbl - Windows OSL for obtaining ACPI tables
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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


#include "acpi.h"

#ifdef WIN32
#pragma warning(disable:4115)   /* warning C4115: (caused by rpcasync.h) */
#include <windows.h>

#elif WIN64
#include <windowsx.h>
#endif

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("oswintbl")


static char             KeyBuffer[64];
static char             ErrorBuffer[64];


/* Little front-end to win FormatMessage */

char *
OsFormatException (
    LONG                Status)
{

    ErrorBuffer[0] = 0;
    FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, Status, 0,
        ErrorBuffer, 64, NULL);

    return (ErrorBuffer);
}


/******************************************************************************
 *
 * FUNCTION:    OsGetTable
 *
 * PARAMETERS:  Signature       - ACPI Signature for desired table. must be
 *                                  a null terminated string.
 *
 * RETURN:      Pointer to the table. NULL if failure.
 *
 * DESCRIPTION: Get an ACPI table from the Windows registry.
 *
 *****************************************************************************/

ACPI_TABLE_HEADER *
OsGetTable (
    char                *Signature)
{
    HKEY                Handle = NULL;
    ULONG               i;
    LONG                Status;
    ULONG               Type;
    ULONG               NameSize;
    ULONG               DataSize;
    HKEY                SubKey;
    ACPI_TABLE_HEADER   *ReturnTable;


    /* Get a handle to the table key */

    while (1)
    {
        ACPI_STRCPY (KeyBuffer, "HARDWARE\\ACPI\\");
        ACPI_STRCAT (KeyBuffer, Signature);

        Status = RegOpenKeyEx (HKEY_LOCAL_MACHINE, KeyBuffer,
                    0L, KEY_READ, &Handle);

        if (Status != ERROR_SUCCESS)
        {
            /*
             * Somewhere along the way, MS changed the registry entry for
             * the FADT from
             * HARDWARE/ACPI/FACP  to
             * HARDWARE/ACPI/FADT.
             *
             * This code allows for both.
             */
            if (ACPI_COMPARE_NAME (Signature, "FACP"))
            {
                Signature = "FADT";
            }
            else
            {
                AcpiOsPrintf (
                    "Could not find %s in registry at %s: %s (Status=0x%X)\n",
                    Signature, KeyBuffer, OsFormatException (Status), Status);
                return (NULL);
            }
        }
        else
        {
            break;
        }
    }

    /* Actual data for table is down a couple levels */

    for (i = 0; ;)
    {
        Status = RegEnumKey (Handle, i, KeyBuffer, sizeof (KeyBuffer));
        i += 1;
        if (Status == ERROR_NO_MORE_ITEMS)
        {
            break;
        }

        Status = RegOpenKey (Handle, KeyBuffer, &SubKey);
        if (Status != ERROR_SUCCESS)
        {
            AcpiOsPrintf ("Could not open %s entry: %s\n",
                Signature, OsFormatException (Status));
            return (NULL);
        }

        RegCloseKey (Handle);
        Handle = SubKey;
        i = 0;
    }

    /* Find the (binary) table entry */

    for (i = 0; ;)
    {
        NameSize = sizeof (KeyBuffer);
        Status = RegEnumValue (Handle, i, KeyBuffer, &NameSize,
                    NULL, &Type, NULL, 0);
        if (Status != ERROR_SUCCESS)
        {
            AcpiOsPrintf ("Could not get %s registry entry: %s\n",
                Signature, OsFormatException (Status));
            return (NULL);
        }

        if (Type == REG_BINARY)
        {
            break;
        }
        i += 1;
    }

    /* Get the size of the table */

    Status = RegQueryValueEx (Handle, KeyBuffer, NULL, NULL, NULL, &DataSize);
    if (Status != ERROR_SUCCESS)
    {
        AcpiOsPrintf ("Could not read the %s table size: %s\n",
            Signature, OsFormatException (Status));
        return (NULL);
    }

    /* Allocate a new buffer for the table */

    ReturnTable = AcpiOsAllocate (DataSize);
    if (!ReturnTable)
    {
        goto Cleanup;
    }

    /* Get the actual table from the registry */

    Status = RegQueryValueEx (Handle, KeyBuffer, NULL, NULL,
                (UCHAR *) ReturnTable, &DataSize);
    if (Status != ERROR_SUCCESS)
    {
        AcpiOsPrintf ("Could not read %s data: %s\n",
            Signature, OsFormatException (Status));
        AcpiOsFree (ReturnTable);
        return (NULL);
    }

Cleanup:
    RegCloseKey (Handle);
    return (ReturnTable);
}

