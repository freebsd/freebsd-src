/******************************************************************************
 *
 * Module Name: ahaml - AML opcode decoding for acpihelp utility
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

#include "acpihelp.h"


/* Local prototypes */

static void
AhDisplayAmlOpcode (
    const AH_AML_OPCODE     *Op);

static void
AhDisplayAmlType (
    const AH_AML_TYPE       *Op);


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


    AcpiUtStrupr (Name);

    /* Find/display all opcode names that match the input name prefix */

    for (Op = Gbl_AmlOpcodeInfo; Op->OpcodeString; Op++)
    {
        if (!Op->OpcodeName) /* Unused opcodes */
        {
            continue;
        }

        if (!Name || (Name[0] == '*'))
        {
            AhDisplayAmlOpcode (Op);
            Found = TRUE;
            continue;
        }

        /* Upper case the opcode name before substring compare */

        strcpy (Gbl_Buffer, Op->OpcodeName);
        AcpiUtStrupr (Gbl_Buffer);

        if (strstr (Gbl_Buffer, Name) == Gbl_Buffer)
        {
            AhDisplayAmlOpcode (Op);
            Found = TRUE;
        }
    }

    if (!Found && Name)
    {
        printf ("%s, no matching AML opcodes\n", Name);
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

    Opcode = strtoul (OpcodeString, NULL, 16);
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

    for (Op = Gbl_AmlOpcodeInfo; Op->OpcodeString; Op++)
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
 * FUNCTION:    AhFindAmlTypes (entry point for AML grammar keyword search)
 *
 * PARAMETERS:  Name                - Name or prefix for an AML grammar element.
 *                                    NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find all AML grammar keywords that match the input Name or name
 *              prefix.
 *
 ******************************************************************************/

void
AhFindAmlTypes (
    char                    *Name)
{
    const AH_AML_TYPE       *Keyword;
    BOOLEAN                 Found = FALSE;


    AcpiUtStrupr (Name);

    for (Keyword = Gbl_AmlTypesInfo; Keyword->Name; Keyword++)
    {
        if (!Name)
        {
            printf ("    %s\n", Keyword->Name);
            Found = TRUE;
            continue;
        }

        if (*Name == '*')
        {
            AhDisplayAmlType (Keyword);
            Found = TRUE;
            continue;
        }

        /* Upper case the operator name before substring compare */

        strcpy (Gbl_Buffer, Keyword->Name);
        AcpiUtStrupr (Gbl_Buffer);

        if (strstr (Gbl_Buffer, Name) == Gbl_Buffer)
        {
            AhDisplayAmlType (Keyword);
            Found = TRUE;
        }
    }

    if (!Found)
    {
        printf ("%s, no matching AML grammar type\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayAmlType
 *
 * PARAMETERS:  Op                  - Pointer to AML grammar info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format and display info for an AML grammar element.
 *
 ******************************************************************************/

static void
AhDisplayAmlType (
    const AH_AML_TYPE       *Op)
{
    char                    *Description;


    Description = Op->Description;
    printf ("%4s", " ");    /* Primary indent */

    /* Emit the entire description string */

    while (*Description)
    {
        /* Description can be multiple lines, must indent each */

        while (*Description != '\n')
        {
            printf ("%c", *Description);
            Description++;
        }

        printf ("\n");
        Description++;

        /* Do indent */

        if (*Description)
        {
            printf ("%8s", " ");    /* Secondary indent */

            /* Index extra for a comment */

            if ((Description[0] == '/') &&
                (Description[1] == '/'))
            {
                printf ("%4s", " ");
            }
        }
    }

    printf ("\n");
}
