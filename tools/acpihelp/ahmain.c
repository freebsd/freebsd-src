/******************************************************************************
 *
 * Module Name: ahmain - Main module for the acpi help utility
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

#include "acpihelp.h"


/* Local prototypes */

static void
AhDisplayUsage (
    void);


/******************************************************************************
 *
 * FUNCTION:    AhDisplayUsage
 *
 * DESCRIPTION: Usage message
 *
 ******************************************************************************/

static void
AhDisplayUsage (
    void)
{

    printf ("\n");
    printf ("Usage: acpihelp <options> [NamePrefix | HexValue]\n\n");
    printf ("Where: -k [NamePrefix]     Find/Display ASL non-operator keyword(s)\n");
    printf ("       -m [NamePrefix]     Find/Display AML opcode name(s)\n");
    printf ("       -o [HexValue]       Decode hex AML opcode\n");
    printf ("       -p [NamePrefix]     Find/Display ASL predefined method name(s)\n");
    printf ("       -s [NamePrefix]     Find/Display ASL operator name(s)\n");
    printf ("\nNamePrefix/HexValue not specified means \"Display All\"\n");
    printf ("\nDefault search with NamePrefix and no options:\n");
    printf ("    Find ASL operator names - if NamePrefix does not start with underscore\n");
    printf ("    Find ASL predefined method names - if NamePrefix starts with underscore\n");
    printf ("\n");
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * DESCRIPTION: C main function for AcpiHelp utility.
 *
 ******************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    *argv[])
{
    char                    *Name;
    UINT32                  DecodeType;
    int                     j;


    printf (ACPI_COMMON_SIGNON ("ACPI Help Utility"));
    DecodeType = AH_DECODE_DEFAULT;

    if (argc < 2)
    {
        AhDisplayUsage ();
        return (0);
    }

    /* Command line options */

    while ((j = AcpiGetopt (argc, argv, "hkmops")) != EOF) switch (j)
    {
    case 'k':
        DecodeType = AH_DECODE_ASL_KEYWORD;
        break;

    case 'm':
        DecodeType = AH_DECODE_AML;
        break;

    case 'o':
        DecodeType = AH_DECODE_AML_OPCODE;
        break;

    case 'p':
        DecodeType = AH_DECODE_PREDEFINED_NAME;
        break;

    case 's':
        DecodeType = AH_DECODE_ASL;
        break;

    case 'h':
    default:
        AhDisplayUsage ();
        return (-1);
    }

    /* Missing (null) name means "display all" */

    Name = argv[AcpiGbl_Optind];

    switch (DecodeType)
    {
    case AH_DECODE_AML:
        AhFindAmlOpcode (Name);
        break;

    case AH_DECODE_AML_OPCODE:
        AhDecodeAmlOpcode (Name);
        break;

    case AH_DECODE_PREDEFINED_NAME:
        AhFindPredefinedNames (Name);
        break;

    case AH_DECODE_ASL:
        AhFindAslOperators (Name);
        break;

    case AH_DECODE_ASL_KEYWORD:
        AhFindAslKeywords (Name);
        break;

    default:
        if (!Name)
        {
            AhFindAslOperators (Name);
            break;
        }

        if (*Name == '_')
        {
            AhFindPredefinedNames (Name);
        }
        else
        {
            AhFindAslOperators (Name);
        }
        break;
    }

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AhStrupr (strupr)
 *
 * PARAMETERS:  SrcString           - The source string to convert
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert string to uppercase
 *
 * NOTE: This is not a POSIX function, so it appears here, not in utclib.c
 *
 ******************************************************************************/

void
AhStrupr (
    char                    *SrcString)
{
    char                    *String;


    if (!SrcString)
    {
        return;
    }

    /* Walk entire string, uppercasing the letters */

    for (String = SrcString; *String; String++)
    {
        *String = (char) toupper ((int) *String);
    }

    return;
}
