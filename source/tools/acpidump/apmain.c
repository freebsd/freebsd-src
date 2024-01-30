/******************************************************************************
 *
 * Module Name: apmain - Main module for the acpidump utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#define _DECLARE_GLOBALS
#include "acpidump.h"


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
#define AP_SUPPORTED_OPTIONS        "?a:bc:f:hn:o:r:sv^xz"


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
    ACPI_OPTION ("-vd",                     "Display build date and time");
    ACPI_OPTION ("-z",                      "Verbose mode");

    ACPI_USAGE_TEXT ("\nTable Options:\n");

    ACPI_OPTION ("-a <Address>",            "Get table via a physical address");
    ACPI_OPTION ("-c <on|off>",             "Turning on/off customized table dumping");
    ACPI_OPTION ("-f <BinaryFile>",         "Get table via a binary file");
    ACPI_OPTION ("-n <Signature>",          "Get table via a name/signature");
    ACPI_OPTION ("-x",                      "Use RSDT instead of XSDT");

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
        fprintf (stderr, "Too many table options (max %d)\n", AP_MAX_ACTIONS);
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
            fprintf (stderr, "%s: Cannot handle this switch, please use on|off\n",
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

        Status = AcpiUtStrtoul64 (AcpiGbl_Optarg, &Gbl_RsdpBase);
        if (ACPI_FAILURE (Status))
        {
            fprintf (stderr, "%s: Could not convert to a physical address\n",
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

    case 'v': /* -v: (Version): signon already emitted, just exit */

        switch (AcpiGbl_Optarg[0])
        {
        case '^':  /* -v: (Version) */

            fprintf (stderr, ACPI_COMMON_SIGNON (AP_UTILITY_NAME));
            return (1);

        case 'd':

            fprintf (stderr, ACPI_COMMON_SIGNON (AP_UTILITY_NAME));
            printf (ACPI_COMMON_BUILD_TIME);
            return (1);

        default:

            printf ("Unknown option: -v%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'z':   /* Verbose mode */

        Gbl_VerboseMode = TRUE;
        fprintf (stderr, ACPI_COMMON_SIGNON (AP_UTILITY_NAME));
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

#if !defined(_GNU_EFI) && !defined(_EDK2_EFI)
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
    AcpiGbl_IntegerByteWidth = 8;

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

            fprintf (stderr, "Internal error, invalid action: 0x%X\n",
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
            fprintf (stderr, "Output file %s contains 0x%X (%u) bytes\n\n",
                Gbl_OutputFilename, FileSize, FileSize);
        }

        fclose (Gbl_OutputFile);
    }

    return (Status);
}
