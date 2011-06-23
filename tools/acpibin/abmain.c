
/******************************************************************************
 *
 * Module Name: abmain - Main module for the acpi binary utility
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


#define _DECLARE_GLOBALS
#include "acpibin.h"
#include "acapps.h"

/* Local prototypes */

static void
AbDisplayUsage (
    UINT8                   OptionCount);


/******************************************************************************
 *
 * FUNCTION:    AbDisplayUsage
 *
 * DESCRIPTION: Usage message
 *
 ******************************************************************************/

static void
AbDisplayUsage (
    UINT8                   OptionCount)
{

    if (OptionCount)
    {
        printf ("Option requires %u arguments\n\n", OptionCount);
    }

    printf ("Usage: acpibin [options]\n\n");
    printf ("Options:\n\n");
    printf (" -c <File1> <File2>           Compare two AML files\n");
    printf (" -d <InFile> <OutFile>        Dump AML binary to text file\n");
    printf (" -e <Sig> <InFile> <OutFile>  Extract binary AML table from AcpiDmp file\n\n");

    printf (" -h <File>                    Display table header for binary AML file\n");
    printf (" -s <File>                    Update checksum for binary AML file\n");
    printf (" -t                           Terse mode\n");

    printf ("\n");
    return;
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * DESCRIPTION: C main function
 *
 ******************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    *argv[])
{
    int                     j;
    int                     Status = AE_OK;


    AcpiGbl_DebugFile = NULL;
    AcpiGbl_DbOutputFlags = DB_CONSOLE_OUTPUT ;

    AcpiOsInitialize ();
    printf (ACPI_COMMON_SIGNON ("ACPI Binary AML File Utility"));

    if (argc < 2)
    {
        AbDisplayUsage (0);
        return 0;
    }

    /* Command line options */

    while ((j = AcpiGetopt (argc, argv, "c:d:e:h:s:t")) != EOF) switch(j)
    {
    case 'c':   /* Compare Files */

        if (argc < 4)
        {
            AbDisplayUsage (2);
            return -1;
        }

        Status = AbCompareAmlFiles (AcpiGbl_Optarg, argv[AcpiGbl_Optind]);
        break;

    case 'd':   /* Dump AML file */

        if (argc < 4)
        {
            AbDisplayUsage (2);
            return -1;
        }

        Status = AbDumpAmlFile (AcpiGbl_Optarg, argv[AcpiGbl_Optind]);
        break;

    case 'e':   /* Extract AML text file */

        if (argc < 5)
        {
            AbDisplayUsage (3);
            return -1;
        }

        Status = AbExtractAmlFile (AcpiGbl_Optarg, argv[AcpiGbl_Optind],
                    argv[AcpiGbl_Optind+1]);
        break;

    case 'h':   /* Display ACPI table header */

        if (argc < 3)
        {
            AbDisplayUsage (1);
            return -1;
        }

        AbDisplayHeader (AcpiGbl_Optarg);
        return (0);

    case 's':   /* Compute/update checksum */

        if (argc < 3)
        {
            AbDisplayUsage (1);
            return -1;
        }

        AbComputeChecksum (AcpiGbl_Optarg);
        return (0);

    case 't':   /* Enable terse mode */

        Gbl_TerseMode = TRUE;
        break;

    default:
        AbDisplayUsage (0);
        return -1;
    }

    return Status;
}
