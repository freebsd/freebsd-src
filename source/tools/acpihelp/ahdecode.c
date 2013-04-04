/******************************************************************************
 *
 * Module Name: ahdecode - Operator/Opcode decoding for acpihelp utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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

#define ACPI_CREATE_PREDEFINED_TABLE
#define ACPI_CREATE_RESOURCE_TABLE

#include "acpihelp.h"
#include "acpredef.h"


/* Device IDs defined in the ACPI specification */

static const AH_DEVICE_ID  AhDeviceIds[] =
{
    {"PNP0A05",     "Generic Container Device"},
    {"PNP0A06",     "Generic Container Device"},
    {"PNP0C08",     "ACPI core hardware"},
    {"PNP0C09",     "Embedded Controller Device"},
    {"PNP0C0A",     "Control Method Battery"},
    {"PNP0C0B",     "Fan"},
    {"PNP0C0C",     "Power Button Device"},
    {"PNP0C0D",     "Lid Device"},
    {"PNP0C0E",     "Sleep Button Device"},
    {"PNP0C0F",     "PCI Interrupt Link Device"},
    {"PNP0C80",     "Memory Device"},

    {"ACPI0001",    "SMBus 1.0 Host Controller"},
    {"ACPI0002",    "Smart Battery Subsystem"},
    {"ACPI0003",    "Power Source Device"},
    {"ACPI0004",    "Module Device"},
    {"ACPI0005",    "SMBus 2.0 Host Controller"},
    {"ACPI0006",    "GPE Block Device"},
    {"ACPI0007",    "Processor Device"},
    {"ACPI0008",    "Ambient Light Sensor Device"},
    {"ACPI0009",    "I/O xAPIC Device"},
    {"ACPI000A",    "I/O APIC Device"},
    {"ACPI000B",    "I/O SAPIC Device"},
    {"ACPI000C",    "Processor Aggregator Device"},
    {"ACPI000D",    "Power Meter Device"},
    {"ACPI000E",    "Time/Alarm Device"},
    {"ACPI000F",    "User Presence Detection Device"},

    {NULL, NULL}
};

#define AH_DISPLAY_EXCEPTION(Status, Name) \
    printf ("%.4X: %s\n", Status, Name)

#define AH_DISPLAY_EXCEPTION_TEXT(Status, Exception) \
    printf ("%.4X: %-28s (%s)\n", Status, Exception->Name, Exception->Description)

#define BUFFER_LENGTH           128
#define LINE_BUFFER_LENGTH      512

static char         Gbl_Buffer[BUFFER_LENGTH];
static char         Gbl_LineBuffer[LINE_BUFFER_LENGTH];

/* Local prototypes */

static BOOLEAN
AhDisplayPredefinedName (
    char                    *Name,
    UINT32                  Length);

static void
AhDisplayPredefinedInfo (
    char                    *Name);

static void
AhDisplayResourceName (
    const ACPI_PREDEFINED_INFO  *ThisName);

static void
AhDisplayAmlOpcode (
    const AH_AML_OPCODE     *Op);

static void
AhDisplayAslOperator (
    const AH_ASL_OPERATOR   *Op);

static void
AhDisplayOperatorKeywords (
    const AH_ASL_OPERATOR   *Op);

static void
AhDisplayAslKeyword (
    const AH_ASL_KEYWORD    *Op);

static void
AhPrintOneField (
    UINT32                  Indent,
    UINT32                  CurrentPosition,
    UINT32                  MaxPosition,
    const char              *Field);


/*******************************************************************************
 *
 * FUNCTION:    AhFindPredefinedNames (entry point for predefined name search)
 *
 * PARAMETERS:  NamePrefix          - Name or prefix to find. Must start with
 *                                    an underscore. NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find and display all ACPI predefined names that match the
 *              input name or prefix. Includes the required number of arguments
 *              and the expected return type, if any.
 *
 ******************************************************************************/

void
AhFindPredefinedNames (
    char                    *NamePrefix)
{
    UINT32                  Length;
    BOOLEAN                 Found;
    char                    Name[9];


    if (!NamePrefix)
    {
        Found = AhDisplayPredefinedName (Name, 0);
        return;
    }

    /* Contruct a local name or name prefix */

    AhStrupr (NamePrefix);
    if (*NamePrefix == '_')
    {
        NamePrefix++;
    }

    Name[0] = '_';
    strncpy (&Name[1], NamePrefix, 7);

    Length = strlen (Name);
    if (Length > 4)
    {
        printf ("%.8s: Predefined name must be 4 characters maximum\n", Name);
        return;
    }

    Found = AhDisplayPredefinedName (Name, Length);
    if (!Found)
    {
        printf ("%s, no matching predefined names\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayPredefinedName
 *
 * PARAMETERS:  Name                - Name or name prefix
 *
 * RETURN:      TRUE if any names matched, FALSE otherwise
 *
 * DESCRIPTION: Display information about ACPI predefined names that match
 *              the input name or name prefix.
 *
 ******************************************************************************/

static BOOLEAN
AhDisplayPredefinedName (
    char                    *Name,
    UINT32                  Length)
{
    const AH_PREDEFINED_NAME    *Info;
    BOOLEAN                     Found = FALSE;
    BOOLEAN                     Matched;
    UINT32                      i;


    /* Find/display all names that match the input name prefix */

    for (Info = AslPredefinedInfo; Info->Name; Info++)
    {
        if (!Name)
        {
            Found = TRUE;
            printf ("%s: <%s>\n", Info->Name, Info->Description);
            printf ("%*s%s\n", 6, " ", Info->Action);

            AhDisplayPredefinedInfo (Info->Name);
            continue;
        }

        Matched = TRUE;
        for (i = 0; i < Length; i++)
        {
            if (Info->Name[i] != Name[i])
            {
                Matched = FALSE;
                break;
            }
        }

        if (Matched)
        {
            Found = TRUE;
            printf ("%s: <%s>\n", Info->Name, Info->Description);
            printf ("%*s%s\n", 6, " ", Info->Action);

            AhDisplayPredefinedInfo (Info->Name);
        }
    }

    return (Found);
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayPredefinedInfo
 *
 * PARAMETERS:  Name                - Exact 4-character ACPI name.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find the name in the main ACPICA predefined info table and
 *              display the # of arguments and the return value type.
 *
 *              Note: Resource Descriptor field names do not appear in this
 *              table -- thus, nothing will be displayed for them.
 *
 ******************************************************************************/

static void
AhDisplayPredefinedInfo (
    char                        *Name)
{
    const ACPI_PREDEFINED_INFO  *ThisName;


    /* NOTE: we check both tables always because there are some dupes */

    /* Check against the predefine methods first */

    ThisName = AcpiUtMatchPredefinedMethod (Name);
    if (ThisName)
    {
        AcpiUtDisplayPredefinedMethod (Gbl_Buffer, ThisName, TRUE);
    }

    /* Check against the predefined resource descriptor names */

    ThisName = AcpiUtMatchResourceName (Name);
    if (ThisName)
    {
        AhDisplayResourceName (ThisName);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayResourceName
 *
 * PARAMETERS:  ThisName            - Entry in the predefined method/name table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display information about a resource descriptor name.
 *
 ******************************************************************************/

static void
AhDisplayResourceName (
    const ACPI_PREDEFINED_INFO  *ThisName)
{
    UINT32                      NumTypes;


    NumTypes = AcpiUtGetResourceBitWidth (Gbl_Buffer,
        ThisName->Info.ArgumentList);

    printf ("      %4.4s resource descriptor field is %s bits wide%s\n",
        ThisName->Info.Name,
        Gbl_Buffer,
        (NumTypes > 1) ? " (depending on descriptor type)" : "");
}


/*******************************************************************************
 *
 * FUNCTION:    AhFindAmlOpcode (entry point for AML opcode name search)
 *
 * PARAMETERS:  Name                - Name or prefix for an AML opcode.
 *                                    NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find all AML opcodes that match the input Name or name
 *              prefix.
 *
 ******************************************************************************/

void
AhFindAmlOpcode (
    char                    *Name)
{
    const AH_AML_OPCODE     *Op;
    BOOLEAN                 Found = FALSE;


    AhStrupr (Name);

    /* Find/display all opcode names that match the input name prefix */

    for (Op = AmlOpcodeInfo; Op->OpcodeString; Op++)
    {
        if (!Op->OpcodeName) /* Unused opcodes */
        {
            continue;
        }

        if (!Name)
        {
            AhDisplayAmlOpcode (Op);
            Found = TRUE;
            continue;
        }

        /* Upper case the opcode name before substring compare */

        strcpy (Gbl_Buffer, Op->OpcodeName);
        AhStrupr (Gbl_Buffer);

        if (strstr (Gbl_Buffer, Name) == Gbl_Buffer)
        {
            AhDisplayAmlOpcode (Op);
            Found = TRUE;
        }
    }

    if (!Found)
    {
        printf ("%s, no matching AML operators\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDecodeAmlOpcode (entry point for AML opcode search)
 *
 * PARAMETERS:  OpcodeString        - String version of AML opcode
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display information about the input AML opcode
 *
 ******************************************************************************/

void
AhDecodeAmlOpcode (
    char                    *OpcodeString)
{
    const AH_AML_OPCODE     *Op;
    UINT32                  Opcode;
    UINT8                   Prefix;


    if (!OpcodeString)
    {
        AhFindAmlOpcode (NULL);
        return;
    }

    Opcode = ACPI_STRTOUL (OpcodeString, NULL, 16);
    if (Opcode > ACPI_UINT16_MAX)
    {
        printf ("Invalid opcode (more than 16 bits)\n");
        return;
    }

    /* Only valid opcode extension is 0x5B */

    Prefix = (Opcode & 0x0000FF00) >> 8;
    if (Prefix && (Prefix != 0x5B))
    {
        printf ("Invalid opcode (invalid extension prefix 0x%X)\n",
            Prefix);
        return;
    }

    /* Find/Display the opcode. May fall within an opcode range */

    for (Op = AmlOpcodeInfo; Op->OpcodeString; Op++)
    {
        if ((Opcode >= Op->OpcodeRangeStart) &&
            (Opcode <= Op->OpcodeRangeEnd))
        {
            AhDisplayAmlOpcode (Op);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayAmlOpcode
 *
 * PARAMETERS:  Op                  - An opcode info struct
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display the contents of an AML opcode information struct
 *
 ******************************************************************************/

static void
AhDisplayAmlOpcode (
    const AH_AML_OPCODE     *Op)
{

    if (!Op->OpcodeName)
    {
        printf ("%18s: Opcode=%-9s\n", "Reserved opcode", Op->OpcodeString);
        return;
    }

    /* Opcode name and value(s) */

    printf ("%18s: Opcode=%-9s Type (%s)",
        Op->OpcodeName, Op->OpcodeString, Op->Type);

    /* Optional fixed/static arguments */

    if (Op->FixedArguments)
    {
        printf (" FixedArgs (");
        AhPrintOneField (37, 36 + 7 + strlen (Op->Type) + 12,
            AH_MAX_AML_LINE_LENGTH, Op->FixedArguments);
        printf (")");
    }

    /* Optional variable-length argument list */

    if (Op->VariableArguments)
    {
        if (Op->FixedArguments)
        {
            printf ("\n%*s", 36, " ");
        }
        printf (" VariableArgs (");
        AhPrintOneField (37, 15, AH_MAX_AML_LINE_LENGTH, Op->VariableArguments);
        printf (")");
    }
    printf ("\n");

    /* Grammar specification */

    if (Op->Grammar)
    {
        AhPrintOneField (37, 0, AH_MAX_AML_LINE_LENGTH, Op->Grammar);
        printf ("\n");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhFindAslKeywords (entry point for ASL keyword search)
 *
 * PARAMETERS:  Name                - Name or prefix for an ASL keyword.
 *                                    NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find all ASL keywords that match the input Name or name
 *              prefix.
 *
 ******************************************************************************/

void
AhFindAslKeywords (
    char                    *Name)
{
    const AH_ASL_KEYWORD    *Keyword;
    BOOLEAN                 Found = FALSE;


    AhStrupr (Name);

    for (Keyword = AslKeywordInfo; Keyword->Name; Keyword++)
    {
        if (!Name)
        {
            AhDisplayAslKeyword (Keyword);
            Found = TRUE;
            continue;
        }

        /* Upper case the operator name before substring compare */

        strcpy (Gbl_Buffer, Keyword->Name);
        AhStrupr (Gbl_Buffer);

        if (strstr (Gbl_Buffer, Name) == Gbl_Buffer)
        {
            AhDisplayAslKeyword (Keyword);
            Found = TRUE;
        }
    }

    if (!Found)
    {
        printf ("%s, no matching ASL keywords\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayAslKeyword
 *
 * PARAMETERS:  Op                  - Pointer to ASL keyword with syntax info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format and display syntax info for an ASL keyword. Splits
 *              long lines appropriately for reading.
 *
 ******************************************************************************/

static void
AhDisplayAslKeyword (
    const AH_ASL_KEYWORD    *Op)
{

    /* ASL keyword name and description */

    printf ("%22s: %s\n", Op->Name, Op->Description);
    if (!Op->KeywordList)
    {
        return;
    }

    /* List of actual keywords */

    AhPrintOneField (24, 0, AH_MAX_ASL_LINE_LENGTH, Op->KeywordList);
    printf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AhFindAslOperators (entry point for ASL operator search)
 *
 * PARAMETERS:  Name                - Name or prefix for an ASL operator.
 *                                    NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find all ASL operators that match the input Name or name
 *              prefix.
 *
 ******************************************************************************/

void
AhFindAslOperators (
    char                    *Name)
{
    const AH_ASL_OPERATOR   *Operator;
    BOOLEAN                 Found = FALSE;


    AhStrupr (Name);

    /* Find/display all names that match the input name prefix */

    for (Operator = AslOperatorInfo; Operator->Name; Operator++)
    {
        if (!Name)
        {
            AhDisplayAslOperator (Operator);
            Found = TRUE;
            continue;
        }

        /* Upper case the operator name before substring compare */

        strcpy (Gbl_Buffer, Operator->Name);
        AhStrupr (Gbl_Buffer);

        if (strstr (Gbl_Buffer, Name) == Gbl_Buffer)
        {
            AhDisplayAslOperator (Operator);
            Found = TRUE;
        }
    }

    if (!Found)
    {
        printf ("%s, no matching ASL operators\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayAslOperator
 *
 * PARAMETERS:  Op                  - Pointer to ASL operator with syntax info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format and display syntax info for an ASL operator. Splits
 *              long lines appropriately for reading.
 *
 ******************************************************************************/

static void
AhDisplayAslOperator (
    const AH_ASL_OPERATOR   *Op)
{

    /* ASL operator name and description */

    printf ("%16s: %s\n", Op->Name, Op->Description);
    if (!Op->Syntax)
    {
        return;
    }

    /* Syntax for the operator */

    AhPrintOneField (18, 0, AH_MAX_ASL_LINE_LENGTH, Op->Syntax);
    printf ("\n");

    AhDisplayOperatorKeywords (Op);
    printf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayOperatorKeywords
 *
 * PARAMETERS:  Op                  - Pointer to ASL keyword with syntax info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display any/all keywords that are associated with the ASL
 *              operator.
 *
 ******************************************************************************/

static void
AhDisplayOperatorKeywords (
    const AH_ASL_OPERATOR   *Op)
{
    char                    *Token;
    char                    *Separators = "(){}, ";
    BOOLEAN                 FirstKeyword = TRUE;


    if (!Op || !Op->Syntax)
    {
        return;
    }

    /*
     * Find all parameters that have the word "keyword" within, and then
     * display the info about that keyword
     */
    strcpy (Gbl_LineBuffer, Op->Syntax);
    Token = strtok (Gbl_LineBuffer, Separators);
    while (Token)
    {
        if (strstr (Token, "Keyword"))
        {
            if (FirstKeyword)
            {
                printf ("\n");
                FirstKeyword = FALSE;
            }

            /* Found a keyword, display keyword information */

            AhFindAslKeywords (Token);
        }

        Token = strtok (NULL, Separators);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhPrintOneField
 *
 * PARAMETERS:  Indent              - Indent length for new line(s)
 *              CurrentPosition     - Position on current line
 *              MaxPosition         - Max allowed line length
 *              Field               - Data to output
 *
 * RETURN:      Line position after field is written
 *
 * DESCRIPTION: Split long lines appropriately for ease of reading.
 *
 ******************************************************************************/

static void
AhPrintOneField (
    UINT32                  Indent,
    UINT32                  CurrentPosition,
    UINT32                  MaxPosition,
    const char              *Field)
{
    UINT32                  Position;
    UINT32                  TokenLength;
    const char              *This;
    const char              *Next;
    const char              *Last;


    This = Field;
    Position = CurrentPosition;

    if (Position == 0)
    {
        printf ("%*s", (int) Indent, " ");
        Position = Indent;
    }

    Last = This + strlen (This);
    while ((Next = strpbrk (This, " ")))
    {
        TokenLength = Next - This;
        Position += TokenLength;

        /* Split long lines */

        if (Position > MaxPosition)
        {
            printf ("\n%*s", (int) Indent, " ");
            Position = TokenLength;
        }

        printf ("%.*s ", (int) TokenLength, This);
        This = Next + 1;
    }

    /* Handle last token on the input line */

    TokenLength = Last - This;
    if (TokenLength > 0)
    {
        Position += TokenLength;
        if (Position > MaxPosition)
        {
            printf ("\n%*s", (int) Indent, " ");
        }
        printf ("%s", This);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayDeviceIds
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all PNP* and ACPI* device IDs defined in the ACPI spec.
 *
 ******************************************************************************/

void
AhDisplayDeviceIds (
    void)
{
    const AH_DEVICE_ID      *DeviceId = AhDeviceIds;


    printf ("ACPI and PNP Device IDs defined in the ACPI specification:\n\n");
    while (DeviceId->Name)
    {
        printf ("%8s   %s\n", DeviceId->Name, DeviceId->Description);
        DeviceId++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDecodeException
 *
 * PARAMETERS:  HexString           - ACPI status string from command line, in
 *                                    hex. If null, display all exceptions.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode and display an ACPI_STATUS exception code.
 *
 ******************************************************************************/

void
AhDecodeException (
    char                    *HexString)
{
    const ACPI_EXCEPTION_INFO   *ExceptionInfo;
    UINT32                      Status;
    UINT32                      i;


    /*
     * A null input string means to decode and display all known
     * exception codes.
     */
    if (!HexString)
    {
        printf ("All defined ACPICA exception codes:\n\n");
        AH_DISPLAY_EXCEPTION (0, "AE_OK                        (No error occurred)");

        /* Display codes in each block of exception types */

        for (i = 1; (i & AE_CODE_MASK) <= AE_CODE_MAX; i += 0x1000)
        {
            Status = i;
            do
            {
                ExceptionInfo = AcpiUtValidateException ((ACPI_STATUS) Status);
                if (ExceptionInfo)
                {
                    AH_DISPLAY_EXCEPTION_TEXT (Status, ExceptionInfo);
                }
                Status++;

            } while (ExceptionInfo);
        }
        return;
    }

    /* Decode a single user-supplied exception code */

    Status = ACPI_STRTOUL (HexString, NULL, 16);
    if (!Status)
    {
        printf ("%s: Invalid hexadecimal exception code value\n", HexString);
        return;
    }

    if (Status > ACPI_UINT16_MAX)
    {
        AH_DISPLAY_EXCEPTION (Status, "Invalid exception code (more than 16 bits)");
        return;
    }

    ExceptionInfo = AcpiUtValidateException ((ACPI_STATUS) Status);
    if (!ExceptionInfo)
    {
        AH_DISPLAY_EXCEPTION (Status, "Unknown exception code");
        return;
    }

    AH_DISPLAY_EXCEPTION_TEXT (Status, ExceptionInfo);
}
