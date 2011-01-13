/******************************************************************************
 *
 * Module Name: anmain - Main routine for the AcpiNames utility
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

#include "acpinames.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("anmain")


extern ACPI_TABLE_DESC  Tables[];

FILE                    *AcpiGbl_DebugFile;
static AE_TABLE_DESC    *AeTableListHead = NULL;


#define AE_SUPPORTED_OPTIONS    "?h"


/******************************************************************************
 *
 * FUNCTION:    usage
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a usage message
 *
 *****************************************************************************/

static void
usage (
    void)
{
    printf ("Usage: AcpiNames [options] AMLfile\n\n");

    printf ("Where:\n");
    printf ("   -?                  Display this message\n");
}


/******************************************************************************
 *
 * FUNCTION:    NsDumpEntireNamespace
 *
 * PARAMETERS:  AmlFilename         - Filename for DSDT or SSDT AML table
 *
 * RETURN:      Status (pass/fail)
 *
 * DESCRIPTION: Build an ACPI namespace for the input AML table, and dump the
 *              formatted namespace contents.
 *
 *****************************************************************************/

static int
NsDumpEntireNamespace (
    char                    *AmlFilename)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       *Table = NULL;
    UINT32                  TableCount = 0;
    AE_TABLE_DESC           *TableDesc;
    ACPI_HANDLE             Handle;


    /* Open the binary AML file and read the entire table */

    Status = AcpiDbReadTableFromFile (AmlFilename, &Table);
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not get input table %s, %s\n", AmlFilename,
            AcpiFormatException (Status));
        return (-1);
    }

    /* Table must be a DSDT. SSDTs are not currently supported */

    if (!ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_DSDT))
    {
        printf ("**** Input table signature is [%4.4s], must be [DSDT]\n",
            Table->Signature);
        return (-1);
    }

    /*
     * Allocate and link a table descriptor (allows for future expansion to
     * multiple input files)
     */
    TableDesc = AcpiOsAllocate (sizeof (AE_TABLE_DESC));
    TableDesc->Table = Table;
    TableDesc->Next = AeTableListHead;
    AeTableListHead = TableDesc;

    TableCount++;

    /*
     * Build a local XSDT with all tables. Normally, here is where the
     * RSDP search is performed to find the ACPI tables
     */
    Status = AeBuildLocalTables (TableCount, AeTableListHead);
    if (ACPI_FAILURE (Status))
    {
        return (-1);
    }

    /* Initialize table manager, get XSDT */

    Status = AcpiInitializeTables (Tables, ACPI_MAX_INIT_TABLES, TRUE);
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not initialize ACPI table manager, %s\n",
            AcpiFormatException (Status));
        return (-1);
    }

    /* Reallocate root table to dynamic memory */

    Status = AcpiReallocateRootTable ();
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not reallocate root table, %s\n",
            AcpiFormatException (Status));
        return (-1);
    }

    /* Load the ACPI namespace */

    Status = AcpiLoadTables ();
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not load ACPI tables, %s\n",
            AcpiFormatException (Status));
        return (-1);
    }

    /*
     * Enable ACPICA. These calls don't do much for this
     * utility, since we only dump the namespace. There is no
     * hardware or event manager code underneath.
     */
    Status = AcpiEnableSubsystem (
                ACPI_NO_ACPI_ENABLE |
                ACPI_NO_ADDRESS_SPACE_INIT |
                ACPI_NO_EVENT_INIT |
                ACPI_NO_HANDLER_INIT);
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not EnableSubsystem, %s\n",
            AcpiFormatException (Status));
        return (-1);
    }

    Status = AcpiInitializeObjects (
                ACPI_NO_ADDRESS_SPACE_INIT |
                ACPI_NO_DEVICE_INIT |
                ACPI_NO_EVENT_INIT);
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not InitializeObjects, %s\n",
            AcpiFormatException (Status));
        return (-1);
    }

    /*
     * Perform a namespace walk to dump the contents
     */
    AcpiOsPrintf ("\nACPI Namespace:\n");

    AcpiNsDumpObjects (ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY, ACPI_UINT32_MAX,
        ACPI_OWNER_ID_MAX, AcpiGbl_RootNode);


    /* Example: get a handle to the _GPE scope */

    Status = AcpiGetHandle (NULL, "\\_GPE", &Handle);
    AE_CHECK_OK (AcpiGetHandle, Status);

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * PARAMETERS:  argc, argv
 *
 * RETURN:      Status (pass/fail)
 *
 * DESCRIPTION: Main routine for NsDump utility
 *
 *****************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    **argv)
{
    ACPI_STATUS             Status;
    int                     j;


    printf (ACPI_COMMON_SIGNON ("ACPI Namespace Dump Utility"));

    if (argc < 2)
    {
        usage ();
        return (0);
    }

    /* Init globals and ACPICA */

    AcpiDbgLevel = ACPI_NORMAL_DEFAULT | ACPI_LV_TABLES;
    AcpiDbgLayer = 0xFFFFFFFF;

    Status = AcpiInitializeSubsystem ();
    AE_CHECK_OK (AcpiInitializeSubsystem, Status);

    /* Get the command line options */

    while ((j = AcpiGetopt (argc, argv, AE_SUPPORTED_OPTIONS)) != EOF) switch(j)
    {
    case '?':
    case 'h':
    default:
        usage();
        return (0);
    }

    /*
     * The next argument is the filename for the DSDT or SSDT.
     * Open the file, build namespace and dump it.
     */
    return (NsDumpEntireNamespace (argv[AcpiGbl_Optind]));
}

