/*******************************************************************************
 *
 * Module Name: dbfileio - Debugger file I/O commands. These can't usually
 *              be used when running the debugger in Ring 0 (Kernel mode)
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/actables.h>
#include <stdio.h>
#ifdef ACPI_APPLICATION
#include <contrib/dev/acpica/include/acapps.h>
#endif

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbfileio")


#ifdef ACPI_DEBUGGER
/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCloseDebugFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: If open, close the current debug output file
 *
 ******************************************************************************/

void
AcpiDbCloseDebugFile (
    void)
{

#ifdef ACPI_APPLICATION

    if (AcpiGbl_DebugFile)
    {
       fclose (AcpiGbl_DebugFile);
       AcpiGbl_DebugFile = NULL;
       AcpiGbl_DbOutputToFile = FALSE;
       AcpiOsPrintf ("Debug output file %s closed\n",
            AcpiGbl_DbDebugFilename);
    }
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbOpenDebugFile
 *
 * PARAMETERS:  Name                - Filename to open
 *
 * RETURN:      None
 *
 * DESCRIPTION: Open a file where debug output will be directed.
 *
 ******************************************************************************/

void
AcpiDbOpenDebugFile (
    char                    *Name)
{

#ifdef ACPI_APPLICATION

    AcpiDbCloseDebugFile ();
    AcpiGbl_DebugFile = fopen (Name, "w+");
    if (!AcpiGbl_DebugFile)
    {
        AcpiOsPrintf ("Could not open debug file %s\n", Name);
        return;
    }

    AcpiOsPrintf ("Debug output file %s opened\n", Name);
    strncpy (AcpiGbl_DbDebugFilename, Name,
        sizeof (AcpiGbl_DbDebugFilename));
    AcpiGbl_DbOutputToFile = TRUE;

#endif
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbLoadTables
 *
 * PARAMETERS:  ListHead        - List of ACPI tables to load
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load ACPI tables from a previously constructed table list.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbLoadTables (
    ACPI_NEW_TABLE_DESC     *ListHead)
{
    ACPI_STATUS             Status;
    ACPI_NEW_TABLE_DESC     *TableListHead;
    ACPI_TABLE_HEADER       *Table;


    /* Load all ACPI tables in the list */

    TableListHead = ListHead;
    while (TableListHead)
    {
        Table = TableListHead->Table;

        Status = AcpiLoadTable (Table);
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_ALREADY_EXISTS)
            {
                AcpiOsPrintf ("Table %4.4s is already installed\n",
                    Table->Signature);
            }
            else
            {
                AcpiOsPrintf ("Could not install table, %s\n",
                    AcpiFormatException (Status));
            }

            return (Status);
        }

        fprintf (stderr,
            "Acpi table [%4.4s] successfully installed and loaded\n",
            Table->Signature);

        TableListHead = TableListHead->Next;
    }

    return (AE_OK);
}
