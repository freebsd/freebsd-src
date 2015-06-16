/******************************************************************************
 *
 * Module Name: apmain - Main module for the acpidump utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#define _DECLARE_GLOBALS
#include "acpidump.h"
#include "acapps.h"


/*
 * acpidump - A portable utility for obtaining system ACPI tables and dumping
 * them in an ASCII hex format suitable for binary extraction via acpixtract.
 *
 * Obtaining the system ACPI tables is an OS-specific operation.
 *
 * This utility can be ported to any host operating system by providing a
 * module containing system-specific versions of these interfaces:
 *
 *      AcpiOsGetTableByAddress
 *      AcpiOsGetTableByIndex
 *      AcpiOsGetTableByName
 *
 * See the ACPICA Reference Guide for the exact definitions of these
 * interfaces. Also, see these ACPICA source code modules for example
 * implementations:
 *
 *      source/os_specific/service_layers/oswintbl.c
 *      source/os_specific/service_layers/oslinuxtbl.c
 */


/* Local prototypes */

static void
ApDisplayUsage (
    void);

static int
ApDoOptions (
    int                     argc,
    char                    **argv);

static int
ApInsertAction (
    char                    *Argument,
    UINT32                  ToBeDone);


/* Table for deferred actions from command line options */

AP_DUMP_ACTION              ActionTable [AP_MAX_ACTIONS];
UINT32                      CurrentAction = 0;


#define AP_UTILITY_NAME             "ACPI Binary Table Dump Utility"
#define AP_SUPPORTED_OPTIONS        "?a:bc:f:hn:o:r:svxz"


/******************************************************************************
 *
 * FUNCTION:    ApDisplayUsage
 *
 * DESCRIPTION: Usage message for the AcpiDump utility
 *
 ******************************************************************************/

static void
ApDisplayUsage (
    void)
{

    ACPI_USAGE_HEADER ("acpidump [options]");

    ACPI_OPTION ("-b",                      "Dump tables to binary files");
    ACPI_OPTION ("-h -?",                   "This help message");
    ACPI_OPTION ("-o <File>",               "Redirect output to file");
    ACPI_OPTION ("-r <Address>",            "Dump tables from specified RSDP");
    ACPI_OPTION ("-s",                      "Print table summaries only");
    ACPI_OPTION ("-v",                      "Display version information");
    ACPI_OPTION ("-z",                      "Verbose mode");

    ACPI_USAGE_TEXT ("\nTable Options:\n");

    ACPI_OPTION ("-a <Address>",            "Get table via a physical address");
    ACPI_OPTION ("-c <on|off>",             "Turning on/off customized table dumping");
    ACPI_OPTION ("-f <BinaryFile>",         "Get table via a binary file");
    ACPI_OPTION ("-n <Signature>",          "Get table via a name/signature");
    ACPI_OPTION ("-x",                      "Do not use but dump XSDT");
    ACPI_OPTION ("-x -x",                   "Do not use or dump XSDT");

    ACPI_USAGE_TEXT (
        "\n"
        "Invocation without parameters dumps all available tables\n"
        "Multiple mixed instances of -a, -f, and -n are supported\n\n");
}


/******************************************************************************
 *
 * FUNCTION:    ApInsertAction
 *
 * PARAMETERS:  Argument            - Pointer to the argument for this action
 *              ToBeDone            - What to do to process this action
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Add an action item to the action table
 *
 ******************************************************************************/

static int
ApInsertAction (
    char                    *Argument,
    UINT32                  ToBeDone)
{

    /* Insert action and check for table overflow */

    ActionTable [CurrentAction].Argument = Argument;
    ActionTable [CurrentAction].ToBeDone = ToBeDone;

    CurrentAction++;
    if (CurrentAction > AP_MAX_ACTIONS)
    {
        AcpiLogError ("Too many table options (max %u)\n", AP_MAX_ACTIONS);
        return (-1);
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApDoOptions
 *
 * PARAMETERS:  argc/argv           - Standard argc/argv
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Command line option processing. The main actions for getting
 *              and dumping tables are deferred via the action table.
 *
 *****************************************************************************/

static int
ApDoOptions (
    int                     argc,
    char                    **argv)
{
    int                     j;
    ACPI_STATUS             Status;


    /* Command line options */

    while ((j = AcpiGetopt (argc, argv, AP_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch (j)
    {
    /*
     * Global options
     */
    case 'b':   /* Dump all input tables to binary files */

        Gbl_BinaryMode = TRUE;
        continue;

    case 'c':   /* Dump customized tables */

        if (!strcmp (AcpiGbl_Optarg, "on"))
        {
            Gbl_DumpCustomizedTables = TRUE;
        }
        else if (!strcmp (AcpiGbl_Optarg, "off"))
        {
            Gbl_DumpCustomizedTables = FALSE;
        }
        else
        {
            AcpiLogError ("%s: Cannot handle this switch, please use on|off\n",
                AcpiGbl_Optarg);
            return (-1);
        }
        continue;

    case 'h':
    case '?':

        ApDisplayUsage ();
        return (1);

    case 'o':   /* Redirect output to a single file */

        if (ApOpenOutputFile (AcpiGbl_Optarg))
        {
            return (-1);
        }
        continue;

    case 'r':   /* Dump tables from specified RSDP */

        Status = AcpiUtStrtoul64 (AcpiGbl_Optarg, 0, &Gbl_RsdpBase);
        if (ACPI_FAILURE (Status))
        {
            AcpiLogError ("%s: Could not convert to a physical address\n",
                AcpiGbl_Optarg);
            return (-1);
        }
        continue;

    case 's':   /* Print table summaries only */

        Gbl_SummaryMode = TRUE;
        continue;

    case 'x':   /* Do not use XSDT */

        if (!AcpiGbl_DoNotUseXsdt)
        {
            AcpiGbl_DoNotUseXsdt = TRUE;
        }
        else
        {
            Gbl_DoNotDumpXsdt = TRUE;
        }
        continue;

    case 'v':   /* Revision/version */

        AcpiOsPrintf (ACPI_COMMON_SIGNON (AP_UTILITY_NAME));
        return (1);

    case 'z':   /* Verbose mode */

        Gbl_VerboseMode = TRUE;
        AcpiLogError (ACPI_COMMON_SIGNON (AP_UTILITY_NAME));
        continue;

    /*
     * Table options
     */
    case 'a':   /* Get table by physical address */

        if (ApInsertAction (AcpiGbl_Optarg, AP_DUMP_TABLE_BY_ADDRESS))
        {
            return (-1);
        }
        break;

    case 'f':   /* Get table from a file */

        if (ApInsertAction (AcpiGbl_Optarg, AP_DUMP_TABLE_BY_FILE))
        {
            return (-1);
        }
        break;

    case 'n':   /* Get table by input name (signature) */

        if (ApInsertAction (AcpiGbl_Optarg, AP_DUMP_TABLE_BY_NAME))
        {
            return (-1);
        }
        break;

    default:

        ApDisplayUsage ();
        return (-1);
    }

    /* If there are no actions, this means "get/dump all tables" */

    if (CurrentAction == 0)
    {
        if (ApInsertAction (NULL, AP_DUMP_ALL_TABLES))
        {
            return (-1);
        }
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * PARAMETERS:  argc/argv           - Standard argc/argv
 *
 * RETURN:      Status
 *
 * DESCRIPTION: C main function for acpidump utility
 *
 ******************************************************************************/

#ifndef _GNU_EFI
int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    *argv[])
#else
int ACPI_SYSTEM_XFACE
acpi_main (
    int                     argc,
    char                    *argv[])
#endif
{
    int                     Status = 0;
    AP_DUMP_ACTION          *Action;
    UINT32                  FileSize;
    UINT32                  i;


    ACPI_DEBUG_INITIALIZE (); /* For debug version only */
    AcpiOsInitialize ();
    Gbl_OutputFile = ACPI_FILE_OUT;

    /* Process command line options */

    Status = ApDoOptions (argc, argv);
    if (Status > 0)
    {
        return (0);
    }
    if (Status < 0)
    {
        return (Status);
    }

    /* Get/dump ACPI table(s) as requested */

    for (i = 0; i < CurrentAction; i++)
    {
        Action = &ActionTable[i];
        switch (Action->ToBeDone)
        {
        case AP_DUMP_ALL_TABLES:

            Status = ApDumpAllTables ();
            break;

        case AP_DUMP_TABLE_BY_ADDRESS:

            Status = ApDumpTableByAddress (Action->Argument);
            break;

        case AP_DUMP_TABLE_BY_NAME:

            Status = ApDumpTableByName (Action->Argument);
            break;

        case AP_DUMP_TABLE_BY_FILE:

            Status = ApDumpTableFromFile (Action->Argument);
            break;

        default:

            AcpiLogError ("Internal error, invalid action: 0x%X\n",
                Action->ToBeDone);
            return (-1);
        }

        if (Status)
        {
            return (Status);
        }
    }

    if (Gbl_OutputFilename)
    {
        if (Gbl_VerboseMode)
        {
            /* Summary for the output file */

            FileSize = CmGetFileSize (Gbl_OutputFile);
            AcpiLogError ("Output file %s contains 0x%X (%u) bytes\n\n",
                Gbl_OutputFilename, FileSize, FileSize);
        }

        AcpiOsCloseFile (Gbl_OutputFile);
    }

    return (Status);
}
