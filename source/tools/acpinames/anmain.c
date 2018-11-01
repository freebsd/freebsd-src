/******************************************************************************
 *
 * Module Name: anmain - Main routine for the AcpiNames utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2018, Intel Corp.
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
#define AN_SUPPORTED_OPTIONS        "?hlv^x:"


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
    ACPI_OPTION ("-vd",                 "Display build date and time");
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

    /* Set flags so that the interpreter is not used */

    AcpiGbl_ExecuteTablesAsMethods = FALSE;

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

    case 'v':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':  /* -v: (Version): signon already emitted, just exit */

            exit (0);

        case 'd':

            printf (ACPI_COMMON_BUILD_TIME);
            return (0);

        default:

            printf ("Unknown option: -v%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

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

    /* Build the namespace from the tables */

    Status = AcpiLoadTables ();
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
     * NOTE:
     * We don't need to do any further ACPICA initialization, since we don't
     * have any hardware, nor is the interpreter configured.
     *
     * Namely, we don't need these calls:
     *  AcpiEnableSubsystem
     *  AcpiInitializeObjects
     */


    /*
     * Perform a namespace walk to dump the contents
     */
    AcpiOsPrintf ("\nACPI Namespace:\n");

    AcpiNsDumpObjects (ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY,
        ACPI_UINT32_MAX, ACPI_OWNER_ID_MAX, AcpiGbl_RootNode);


    /* Example: get a handle to the _GPE scope */

    Status = AcpiGetHandle (NULL, "\\_GPE", &Handle);
    ACPI_CHECK_OK (AcpiGetHandle, Status);

    AcDeleteTableList (ListHead);
    return (0);
}
