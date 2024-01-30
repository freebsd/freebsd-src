/******************************************************************************
 *
 * Module Name: ahdecode - Miscellaneous decoding for acpihelp utility
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

#define ACPI_CREATE_PREDEFINED_TABLE
#define ACPI_CREATE_RESOURCE_TABLE

#include "acpihelp.h"
#include "acpredef.h"

BOOLEAN                  AslGbl_VerboseErrors = TRUE;

/* Local prototypes */

static BOOLEAN
AhDisplayPredefinedName (
    char                    *Name,
    UINT32                  Length);

static void
AhDisplayPredefinedInfo (
    char                    *Name);

static void
AhDoSpecialNames (
    char                    *Name);

static void
AhDisplayResourceName (
    const ACPI_PREDEFINED_INFO  *ThisName);


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

void
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
 * FUNCTION:    AhDisplayDirectives
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all iASL preprocessor directives.
 *
 ******************************************************************************/

void
AhDisplayDirectives (
    void)
{
    const AH_DIRECTIVE_INFO *Info;


    printf ("iASL Preprocessor Directives\n\n");

    for (Info = Gbl_PreprocessorDirectives; Info->Name; Info++)
    {
        printf ("  %-36s : %s\n", Info->Name, Info->Description);
    }
}


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
    char                    Name[ACPI_NAMESEG_SIZE + 1];


    if (!NamePrefix || (*NamePrefix == '*'))
    {
        (void) AhDisplayPredefinedName (NULL, 0);
        return;
    }

    Length = strlen (NamePrefix);
    if (Length > ACPI_NAMESEG_SIZE)
    {
        printf ("%.8s: Predefined name must be 4 characters maximum\n",
            NamePrefix);
        return;
    }

    /* Construct a local name or name prefix */

    AcpiUtStrupr (NamePrefix);
    if (*NamePrefix == '_')
    {
        NamePrefix++;
    }

    Name[0] = '_';
    AcpiUtSafeStrncpy (&Name[1], NamePrefix, 4);

    /* Check for special names such as _Exx, _ACx, etc. */

    AhDoSpecialNames (Name);

    /* Lookup and display the name(s) */

    Found = AhDisplayPredefinedName (Name, Length);
    if (!Found)
    {
        printf ("%s, no matching predefined names\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDoSpecialNames
 *
 * PARAMETERS:  Name          - Name or prefix to find
 *
 * RETURN:      None
 *
 * DESCRIPTION: Detect and handle the "special" names such as _Exx, _ACx, etc.
 *
 * Current support:
 *  _EJx
 *  _Exx
 *  _Lxx
 *  _Qxx
 *  _Wxx
 *  _ACx
 *  _ALx
 *  _T_x
 *
 ******************************************************************************/

static void
AhDoSpecialNames (
    char                    *Name)
{

    /*
     * Check for the special names that have one or more numeric
     * suffixes. For example, _Lxx can have 256 different flavors,
     * from _L00 to _LFF.
     */
    switch (Name[1])
    {
    case 'E':
        if (Name[2] == 'J')
        {
            if (isdigit ((int) Name[3]) || (Name[3] == 'X'))
            {
                /* _EJx */

                Name[3] = 'x';
                break;
            }
        }

        ACPI_FALLTHROUGH;

    case 'L':
    case 'Q':
    case 'W':
        if ((isxdigit ((int) Name[2]) && isxdigit ((int) Name[3]))
                ||
            ((Name[2] == 'X') && (Name[3] == 'X')))
        {
            /* _Exx, _Lxx, _Qxx, or _Wxx */

            Name[2] = 'x';
            Name[3] = 'x';
        }
        break;

    case 'A':
        if ((Name[2] == 'C') || (Name[2] == 'L'))
        {
            if (isdigit ((int) Name[3]) || (Name[3] == 'X'))
            {
                /* _ACx or _ALx */

                Name[3] = 'x';
            }
        }
        break;

    case 'T':
        if (Name[2] == '_')
        {
            /* _T_x (Reserved for iASL compiler */

            Name[3] = 'x';
        }
        break;

    default:
        break;
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
    UINT32                      i = 0;


    /* Find/display all names that match the input name prefix */

    for (Info = AslPredefinedInfo; Info->Name; Info++)
    {
        if (!Name)
        {
            Found = TRUE;
            printf ("%s: <%s>\n", Info->Name, Info->Description);
            printf ("%*s%s\n", 6, " ", Info->Action);

            AhDisplayPredefinedInfo (Info->Name);
            i++;
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

    if (!Name)
    {
        printf ("\nFound %d Predefined ACPI Names\n", i);
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

    /* Check against the predefined methods first */

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
 * FUNCTION:    AhDisplayDeviceIds
 *
 * PARAMETERS:  Name                - Device Hardware ID string.
 *                                    NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display PNP* and ACPI* device IDs.
 *
 ******************************************************************************/

void
AhDisplayDeviceIds (
    char                    *Name)
{
    const AH_DEVICE_ID      *Info;
    UINT32                  Length;
    BOOLEAN                 Matched;
    UINT32                  i;
    BOOLEAN                 Found = FALSE;


    /* Null input name indicates "display all" */

    if (!Name || (Name[0] == '*'))
    {
        printf ("ACPI and PNP Device/Hardware IDs:\n\n");
        for (Info = AslDeviceIds; Info->Name; Info++)
        {
            printf ("%8s   %s\n", Info->Name, Info->Description);
        }

        return;
    }

    Length = strlen (Name);
    if (Length > 8)
    {
        printf ("%.8s: Hardware ID must be 8 characters maximum\n", Name);
        return;
    }

    /* Find/display all names that match the input name prefix */

    AcpiUtStrupr (Name);
    for (Info = AslDeviceIds; Info->Name; Info++)
    {
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
            printf ("%8s   %s\n", Info->Name, Info->Description);
        }
    }

    if (!Found)
    {
        printf ("%s, Hardware ID not found\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayUuids
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all known UUIDs.
 *
 ******************************************************************************/

void
AhDisplayUuids (
    void)
{
    const AH_UUID           *Info;


    printf ("ACPI-related UUIDs/GUIDs:\n");

    /* Display entire table of known ACPI-related UUIDs/GUIDs */

    for (Info = Gbl_AcpiUuids; Info->Description; Info++)
    {
        if (!Info->String) /* Null UUID string means group description */
        {
            printf ("\n%36s\n", Info->Description);
        }
        else
        {
            printf ("%32s : %s\n", Info->Description, Info->String);
        }
    }

    /* Help info on how UUIDs/GUIDs strings are encoded */

    printf ("\n\nByte encoding of UUID/GUID strings"
        " into ACPI Buffer objects (use ToUUID from ASL):\n\n");

    printf ("%32s : %s\n", "Input UUID/GUID String format",
        "aabbccdd-eeff-gghh-iijj-kkllmmnnoopp");

    printf ("%32s : %s\n", "Expected output ACPI buffer",
        "dd,cc,bb,aa, ff,ee, hh,gg, ii,jj, kk,ll,mm,nn,oo,pp");
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all known ACPI tables
 *
 ******************************************************************************/

void
AhDisplayTables (
    void)
{
    const AH_TABLE          *Info;
    UINT32                  i = 0;


    printf ("Known/Supported ACPI tables:\n");

    for (Info = AcpiGbl_SupportedTables; Info->Signature; Info++)
    {
        printf ("%8u) %s : %s\n", i + 1, Info->Signature, Info->Description);
        i++;
    }

    printf ("\nTotal %u ACPI tables\n\n", i);
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
        AH_DISPLAY_EXCEPTION (0,
            "AE_OK                        (No error occurred)");

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

    Status = strtoul (HexString, NULL, 16);
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
