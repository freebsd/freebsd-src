/******************************************************************************
 *
 * Module Name: ahmain - Main module for the acpi help utility
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

#include "acpihelp.h"


/* Local prototypes */

static void
AhDisplayUsage (
    void);

#define AH_UTILITY_NAME             "ACPI Help Utility"
#define AH_SUPPORTED_OPTIONS        "adehikmopstuv"


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

    ACPI_USAGE_HEADER ("acpihelp <options> [Name/Prefix | HexValue]");
    ACPI_OPTION ("-h",                      "Display help");
    ACPI_OPTION ("-v",                      "Display version information");

    ACPI_USAGE_TEXT ("\nAML (ACPI Machine Language) Names and Encodings:\n");
    ACPI_OPTION ("-a [Name/Prefix]",        "Find/Display both ASL operator and AML opcode name(s)");
    ACPI_OPTION ("-m [Name/Prefix]",        "Find/Display AML opcode name(s)");

    ACPI_USAGE_TEXT ("\nACPI Values:\n");
    ACPI_OPTION ("-e [HexValue]",           "Decode ACPICA exception code");
    ACPI_OPTION ("-o [HexValue]",           "Decode hex AML opcode");

    ACPI_USAGE_TEXT ("\nASL (ACPI Source Language) Names and Symbols:\n");
    ACPI_OPTION ("-k [Name/Prefix]",        "Find/Display ASL non-operator keyword(s)");
    ACPI_OPTION ("-p [Name/Prefix]",        "Find/Display ASL predefined method name(s)");
    ACPI_OPTION ("-s [Name/Prefix]",        "Find/Display ASL operator name(s)");

    ACPI_USAGE_TEXT ("\nOther ACPI Names:\n");
    ACPI_OPTION ("-i [Name/Prefix]",        "Find/Display ACPI/PNP Hardware ID(s)");
    ACPI_OPTION ("-d",                      "Display iASL Preprocessor directives");
    ACPI_OPTION ("-t",                      "Display supported ACPI tables");
    ACPI_OPTION ("-u",                      "Display ACPI-related UUIDs");

    ACPI_USAGE_TEXT ("\nName/Prefix or HexValue not specified means \"Display All\"\n");
    ACPI_USAGE_TEXT ("\nDefault search with valid Name/Prefix and no options:\n");
    ACPI_USAGE_TEXT ("    Find ASL/AML operator names - if NamePrefix does not start with underscore\n");
    ACPI_USAGE_TEXT ("    Find ASL predefined method names - if NamePrefix starts with underscore\n");
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


    AcpiOsInitialize ();
    ACPI_DEBUG_INITIALIZE (); /* For debug version only */
    printf (ACPI_COMMON_SIGNON (AH_UTILITY_NAME));
    DecodeType = AH_DECODE_DEFAULT;

    if (argc < 2)
    {
        AhDisplayUsage ();
        return (0);
    }

    /* Command line options */

    while ((j = AcpiGetopt (argc, argv, AH_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch (j)
    {
    case 'a':

        DecodeType = AH_DECODE_ASL_AML;
        break;

    case 'd':

        DecodeType = AH_DISPLAY_DIRECTIVES;
        break;

    case 'e':

        DecodeType = AH_DECODE_EXCEPTION;
        break;

    case 'i':

        DecodeType = AH_DISPLAY_DEVICE_IDS;
        break;

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

    case 't':

        DecodeType = AH_DISPLAY_TABLES;
        break;

    case 'u':

        DecodeType = AH_DISPLAY_UUIDS;
        break;

    case 'v': /* -v: (Version): signon already emitted, just exit */

        return (0);

    case 'h':
    default:

        AhDisplayUsage ();
        return (-1);
    }

    /* Missing (null) name means "display all" */

    Name = argv[AcpiGbl_Optind];

    switch (DecodeType)
    {
    case AH_DECODE_ASL_AML:

        AhFindAslAndAmlOperators (Name);
        break;

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

    case AH_DISPLAY_DEVICE_IDS:

        AhDisplayDeviceIds (Name);
        break;

    case AH_DECODE_EXCEPTION:

        AhDecodeException (Name);
        break;

    case AH_DISPLAY_UUIDS:

        AhDisplayUuids ();
        break;

    case AH_DISPLAY_TABLES:

        AhDisplayTables ();
        break;

    case AH_DISPLAY_DIRECTIVES:

        AhDisplayDirectives ();
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
            AhFindAslAndAmlOperators (Name);
        }
        break;
    }

    return (0);
}
