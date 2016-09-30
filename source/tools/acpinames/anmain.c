/******************************************************************************
 *
 * Module Name: anmain - Main routine for the AcpiNames utility
 *
 *****************************************************************************/

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

#include "acpinames.h"
#include "actables.h"
#include "errno.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("anmain")


/* Local prototypes */

static int
AnDumpEntireNamespace (
    ACPI_NEW_TABLE_DESC     *ListHead);


/*
 * Main routine for the ACPI user-space namespace utility.
 *
 * Portability note: The utility depends upon the host for command-line
 * wildcard support - it is not implemented locally. For example:
 *
 * Linux/Unix systems: Shell expands wildcards automatically.
 *
 * Windows: The setargv.obj module must be linked in to automatically
 * expand wildcards.
 */
BOOLEAN                     AcpiGbl_NsLoadOnly = FALSE;


#define AN_UTILITY_NAME             "ACPI Namespace Dump Utility"
#define AN_SUPPORTED_OPTIONS        "?hlvx:"


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

    ACPI_USAGE_HEADER ("AcpiNames [options] AMLfile");
    ACPI_OPTION ("-?",                  "Display this message");
    ACPI_OPTION ("-l",                  "Load namespace only, no display");
    ACPI_OPTION ("-v",                  "Display version information");
    ACPI_OPTION ("-x <DebugLevel>",     "Debug output level");
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
    ACPI_NEW_TABLE_DESC     *ListHead = NULL;
    ACPI_STATUS             Status;
    int                     j;


    ACPI_DEBUG_INITIALIZE (); /* For debug version only */

    /* Init debug globals and ACPICA */

    AcpiDbgLevel = ACPI_NORMAL_DEFAULT | ACPI_LV_TABLES;
    AcpiDbgLayer = 0xFFFFFFFF;

    Status = AcpiInitializeSubsystem ();
    ACPI_CHECK_OK (AcpiInitializeSubsystem, Status);
    if (ACPI_FAILURE (Status))
    {
        return (-1);
    }

    printf (ACPI_COMMON_SIGNON (AN_UTILITY_NAME));
    if (argc < 2)
    {
        usage ();
        return (0);
    }

    /* Get the command line options */

    while ((j = AcpiGetopt (argc, argv, AN_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch(j)
    {
    case 'l':

        AcpiGbl_NsLoadOnly = TRUE;
        break;

    case 'v': /* -v: (Version): signon already emitted, just exit */

        return (0);

    case 'x':

        AcpiDbgLevel = strtoul (AcpiGbl_Optarg, NULL, 0);
        printf ("Debug Level: 0x%8.8X\n", AcpiDbgLevel);
        break;

    case '?':
    case 'h':
    default:

        usage();
        return (0);
    }

    /* Get each of the ACPI table files on the command line */

    while (argv[AcpiGbl_Optind])
    {
        /* Get all ACPI AML tables in this file */

        Status = AcGetAllTablesFromFile (argv[AcpiGbl_Optind],
            ACPI_GET_ALL_TABLES, &ListHead);
        if (ACPI_FAILURE (Status))
        {
            return (-1);
        }

        AcpiGbl_Optind++;
    }

    printf ("\n");

    /*
     * The next argument is the filename for the DSDT or SSDT.
     * Open the file, build namespace and dump it.
     */
    return (AnDumpEntireNamespace (ListHead));
}


/******************************************************************************
 *
 * FUNCTION:    AnDumpEntireNamespace
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
AnDumpEntireNamespace (
    ACPI_NEW_TABLE_DESC     *ListHead)
{
    ACPI_STATUS             Status;
    ACPI_HANDLE             Handle;


    /*
     * Build a local XSDT with all tables. Normally, here is where the
     * RSDP search is performed to find the ACPI tables
     */
    Status = AnBuildLocalTables (ListHead);
    if (ACPI_FAILURE (Status))
    {
        return (-1);
    }

    /* Initialize table manager, get XSDT */

    Status = AcpiInitializeTables (NULL, ACPI_MAX_INIT_TABLES, TRUE);
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not initialize ACPI table manager, %s\n",
            AcpiFormatException (Status));
        return (-1);
    }

    /* Load the ACPI namespace */

    Status = AcpiTbLoadNamespace ();
    if (Status == AE_CTRL_TERMINATE)
    {
        /* At least one table load failed -- terminate with error */

        return (-1);
    }

    if (ACPI_FAILURE (Status))
    {
        printf ("**** While creating namespace, %s\n",
            AcpiFormatException (Status));
        return (-1);
    }

    if (AcpiGbl_NsLoadOnly)
    {
        printf ("**** Namespace successfully loaded\n");
        return (0);
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

    AcpiNsDumpObjects (ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY,
        ACPI_UINT32_MAX, ACPI_OWNER_ID_MAX, AcpiGbl_RootNode);


    /* Example: get a handle to the _GPE scope */

    Status = AcpiGetHandle (NULL, "\\_GPE", &Handle);
    ACPI_CHECK_OK (AcpiGetHandle, Status);

    return (0);
}
