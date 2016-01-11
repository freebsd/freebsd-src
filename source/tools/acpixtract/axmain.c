/******************************************************************************
 *
 * Module Name: axmain - main module for acpixtract utility
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

#define DEFINE_ACPIXTRACT_GLOBALS
#include "acpixtract.h"


/* Local prototypes */

static void
DisplayUsage (
    void);


/******************************************************************************
 *
 * FUNCTION:    DisplayUsage
 *
 * DESCRIPTION: Usage message
 *
 ******************************************************************************/

static void
DisplayUsage (
    void)
{

    ACPI_USAGE_HEADER ("acpixtract [option] <InputFile>");

    ACPI_OPTION ("-a",                  "Extract all tables, not just DSDT/SSDT");
    ACPI_OPTION ("-l",                  "List table summaries, do not extract");
    ACPI_OPTION ("-m",                  "Extract multiple DSDT/SSDTs to a single file");
    ACPI_OPTION ("-s <signature>",      "Extract all tables with <signature>");
    ACPI_OPTION ("-v",                  "Display version information");

    ACPI_USAGE_TEXT ("\nExtract binary ACPI tables from text acpidump output\n");
    ACPI_USAGE_TEXT ("Default invocation extracts the DSDT and all SSDTs\n");
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * DESCRIPTION: C main function
 *
 ******************************************************************************/

int
main (
    int                     argc,
    char                    *argv[])
{
    char                    *Filename;
    int                     AxAction;
    int                     Status;
    int                     j;


    Gbl_TableCount = 0;
    Gbl_TableListHead = NULL;
    AxAction = AX_EXTRACT_AML_TABLES; /* Default: DSDT & SSDTs */

    ACPI_DEBUG_INITIALIZE (); /* For debug version only */
    AcpiOsInitialize ();
    printf (ACPI_COMMON_SIGNON (AX_UTILITY_NAME));

    if (argc < 2)
    {
        DisplayUsage ();
        return (0);
    }

    /* Command line options */

    while ((j = AcpiGetopt (argc, argv, AX_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch (j)
    {
    case 'a':

        AxAction = AX_EXTRACT_ALL;          /* Extract all tables found */
        break;

    case 'l':

        AxAction = AX_LIST_ALL;             /* List tables only, do not extract */
        break;

    case 'm':

        AxAction = AX_EXTRACT_MULTI_TABLE;  /* Make single file for all DSDT/SSDTs */
        break;

    case 's':

        AxAction = AX_EXTRACT_SIGNATURE;    /* Extract only tables with this sig */
        break;

    case 'v': /* -v: (Version): signon already emitted, just exit */

        return (0);

    case 'h':
    default:

        DisplayUsage ();
        return (0);
    }

    /* Input filename is always required */

    Filename = argv[AcpiGbl_Optind];
    if (!Filename)
    {
        printf ("Missing required input filename\n");
        return (-1);
    }

    /* Perform requested action */

    switch (AxAction)
    {
    case AX_EXTRACT_ALL:

        Status = AxExtractTables (Filename, NULL, AX_OPTIONAL_TABLES);
        break;

    case AX_EXTRACT_MULTI_TABLE:

        Status = AxExtractToMultiAmlFile (Filename);
        break;

    case AX_LIST_ALL:

        Status = AxListTables (Filename);
        break;

    case AX_EXTRACT_SIGNATURE:

        Status = AxExtractTables (Filename, AcpiGbl_Optarg, AX_REQUIRED_TABLE);
        break;

    default:
        /*
         * Default output is the DSDT and all SSDTs. One DSDT is required,
         * any SSDTs are optional.
         */
        Status = AxExtractTables (Filename, "DSDT", AX_REQUIRED_TABLE);
        if (Status)
        {
            return (Status);
        }

        Status = AxExtractTables (Filename, "SSDT", AX_OPTIONAL_TABLES);
        break;
    }

    return (Status);
}
