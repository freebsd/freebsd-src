/*******************************************************************************
 *
 * Module Name: dbfileio - Debugger file I/O commands. These can't usually
 *              be used when running the debugger in Ring 0 (Kernel mode)
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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

#if (defined ACPI_DEBUGGER || defined ACPI_DISASSEMBLER)

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
       AcpiOsPrintf ("Debug output file %s closed\n", AcpiGbl_DbDebugFilename);
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
    ACPI_STRNCPY (AcpiGbl_DbDebugFilename, Name,
        sizeof (AcpiGbl_DbDebugFilename));
    AcpiGbl_DbOutputToFile = TRUE;

#endif
}
#endif


#ifdef ACPI_APPLICATION
#include <contrib/dev/acpica/include/acapps.h>

/*******************************************************************************
 *
 * FUNCTION:    AeLocalLoadTable
 *
 * PARAMETERS:  Table           - pointer to a buffer containing the entire
 *                                table to be loaded
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to load a table from the caller's
 *              buffer. The buffer must contain an entire ACPI Table including
 *              a valid header. The header fields will be verified, and if it
 *              is determined that the table is invalid, the call will fail.
 *
 ******************************************************************************/

static ACPI_STATUS
AeLocalLoadTable (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status = AE_OK;
/*    ACPI_TABLE_DESC         TableInfo; */


    ACPI_FUNCTION_TRACE (AeLocalLoadTable);
#if 0


    if (!Table)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    TableInfo.Pointer = Table;
    Status = AcpiTbRecognizeTable (&TableInfo, ACPI_TABLE_ALL);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the new table into the local data structures */

    Status = AcpiTbInitTableDescriptor (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_ALREADY_EXISTS)
        {
            /* Table already exists, no error */

            Status = AE_OK;
        }

        /* Free table allocated by AcpiTbGetTable */

        AcpiTbDeleteSingleTable (&TableInfo);
        return_ACPI_STATUS (Status);
    }

#if (!defined (ACPI_NO_METHOD_EXECUTION) && !defined (ACPI_CONSTANT_EVAL_ONLY))

    Status = AcpiNsLoadTable (TableInfo.InstalledDesc, AcpiGbl_RootNode);
    if (ACPI_FAILURE (Status))
    {
        /* Uninstall table and free the buffer */

        AcpiTbDeleteTablesByType (ACPI_TABLE_ID_DSDT);
        return_ACPI_STATUS (Status);
    }
#endif
#endif

    return_ACPI_STATUS (Status);
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetTableFromFile
 *
 * PARAMETERS:  Filename        - File where table is located
 *              ReturnTable     - Where a pointer to the table is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load an ACPI table from a file
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbGetTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **ReturnTable)
{
#ifdef ACPI_APPLICATION
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       *Table;
    BOOLEAN                 IsAmlTable = TRUE;


    Status = AcpiUtReadTableFromFile (Filename, &Table);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

#ifdef ACPI_DATA_TABLE_DISASSEMBLY
    IsAmlTable = AcpiUtIsAmlTable (Table);
#endif

    if (IsAmlTable)
    {
        /* Attempt to recognize and install the table */

        Status = AeLocalLoadTable (Table);
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

        AcpiTbPrintTableHeader (0, Table);

        fprintf (stderr,
            "Acpi table [%4.4s] successfully installed and loaded\n",
            Table->Signature);
    }

    AcpiGbl_AcpiHardwarePresent = FALSE;
    if (ReturnTable)
    {
        *ReturnTable = Table;
    }


#endif  /* ACPI_APPLICATION */
    return (AE_OK);
}

#endif  /* ACPI_DEBUGGER */
