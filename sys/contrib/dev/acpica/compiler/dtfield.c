/******************************************************************************
 *
 * Module Name: dtfield.c - Code generation for individual source fields
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code.  No other license or right
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
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
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
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
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
 *****************************************************************************/

#define __DTFIELD_C__

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtfield")


/* Local prototypes */

static void
DtCompileString (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength);

static char *
DtNormalizeBuffer (
    char                    *Buffer,
    UINT32                  *Count);


/******************************************************************************
 *
 * FUNCTION:    DtCompileOneField
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              Field               - Field to be compiled
 *              ByteLength          - Byte length of the field
 *              Type                - Field type
 *
 * RETURN:      None
 *
 * DESCRIPTION: Compile a field value to binary
 *
 *****************************************************************************/

void
DtCompileOneField (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength,
    UINT8                   Type,
    UINT8                   Flags)
{

    switch (Type)
    {
    case DT_FIELD_TYPE_INTEGER:
        DtCompileInteger (Buffer, Field, ByteLength, Flags);
        break;

    case DT_FIELD_TYPE_STRING:
        DtCompileString (Buffer, Field, ByteLength);
        break;

    case DT_FIELD_TYPE_BUFFER:
        DtCompileBuffer (Buffer, Field->Value, Field, ByteLength);
        break;

    default:
        DtFatal (ASL_MSG_COMPILER_INTERNAL, Field, "Invalid field type");
        break;
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileString
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              Field               - String to be copied to buffer
 *              ByteLength          - Maximum length of string
 *
 * RETURN:      None
 *
 * DESCRIPTION: Copy string to the buffer
 *
 *****************************************************************************/

static void
DtCompileString (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength)
{
    UINT32                  Length;


    Length = ACPI_STRLEN (Field->Value);

    /* Check if the string is too long for the field */

    if (Length > ByteLength)
    {
        sprintf (MsgBuffer, "Maximum %u characters", ByteLength);
        DtError (ASL_ERROR, ASL_MSG_STRING_LENGTH, Field, MsgBuffer);
        Length = ByteLength;
    }

    ACPI_MEMCPY (Buffer, Field->Value, Length);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileInteger
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              Field               - Field obj with Integer to be compiled
 *              ByteLength          - Byte length of the integer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Compile an integer
 *
 *****************************************************************************/

void
DtCompileInteger (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength,
    UINT8                   Flags)
{
    UINT64                  Value = 0;
    UINT64                  MaxValue;
    UINT8                   *Hex;
    char                    *Message = NULL;
    ACPI_STATUS             Status;
    int                     i;


    /* Byte length must be in range 1-8 */

    if ((ByteLength > 8) || (ByteLength == 0))
    {
        DtFatal (ASL_MSG_COMPILER_INTERNAL, Field,
            "Invalid internal Byte length");
        return;
    }

    /* Convert string to an actual integer */

    Status = DtStrtoul64 (Field->Value, &Value);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_LIMIT)
        {
            Message = "Constant larger than 64 bits";
        }
        else if (Status == AE_BAD_CHARACTER)
        {
            Message = "Invalid character in constant";
        }

        DtError (ASL_ERROR, ASL_MSG_INVALID_HEX_INTEGER, Field, Message);
        goto Exit;
    }

    /* Ensure that reserved fields are set to zero */
    /* TBD: should we set to zero, or just make this an ERROR? */
    /* TBD: Probably better to use a flag */

    if (!ACPI_STRCMP (Field->Name, "Reserved") &&
        (Value != 0))
    {
        DtError (ASL_WARNING, ASL_MSG_RESERVED_VALUE, Field,
            "Setting to zero");
        Value = 0;
    }

    /* Check if the value must be non-zero */

    if ((Value == 0) && (Flags & DT_NON_ZERO))
    {
        DtError (ASL_ERROR, ASL_MSG_ZERO_VALUE, Field, NULL);
    }

    /*
     * Generate the maximum value for the data type (ByteLength)
     * Note: construct chosen for maximum portability
     */
    MaxValue = ((UINT64) (-1)) >> (64 - (ByteLength * 8));

    /* Validate that the input value is within range of the target */

    if (Value > MaxValue)
    {
        sprintf (MsgBuffer, "Maximum %u bytes", ByteLength);
        DtError (ASL_ERROR, ASL_MSG_INTEGER_SIZE, Field, MsgBuffer);
    }

    /*
     * TBD: hard code for ASF! Capabilites field.
     *
     * This field is actually a buffer, not a 56-bit integer --
     * so, the ordering is reversed. Something should be fixed
     * so we don't need this code.
     */
    if (ByteLength == 7)
    {
        Hex = ACPI_CAST_PTR (UINT8, &Value);
        for (i = 6; i >= 0; i--)
        {
            Buffer[i] = *Hex;
            Hex++;
        }
        return;
    }

Exit:
    ACPI_MEMCPY (Buffer, &Value, ByteLength);
    return;
}


/******************************************************************************
 *
 * FUNCTION:    DtNormalizeBuffer
 *
 * PARAMETERS:  Buffer              - Input buffer
 *              Count               - Output the count of hex number in
 *                                    the Buffer
 *
 * RETURN:      The normalized buffer, freed by caller
 *
 * DESCRIPTION: [1A,2B,3C,4D] or 1A, 2B, 3C, 4D will be normalized
 *              to 1A 2B 3C 4D
 *
 *****************************************************************************/

static char *
DtNormalizeBuffer (
    char                    *Buffer,
    UINT32                  *Count)
{
    char                    *NewBuffer;
    char                    *TmpBuffer;
    UINT32                  BufferCount = 0;
    BOOLEAN                 Separator = TRUE;
    char                    c;


    NewBuffer = UtLocalCalloc (ACPI_STRLEN (Buffer) + 1);
    TmpBuffer = NewBuffer;

    while ((c = *Buffer++))
    {
        switch (c)
        {
        /* Valid separators */

        case '[':
        case ']':
        case ' ':
        case ',':
            Separator = TRUE;
            break;

        default:
            if (Separator)
            {
                /* Insert blank as the standard separator */

                if (NewBuffer[0])
                {
                    *TmpBuffer++ = ' ';
                    BufferCount++;
                }

                Separator = FALSE;
            }

            *TmpBuffer++ = c;
            break;
        }
    }

    *Count = BufferCount + 1;
    return (NewBuffer);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileBuffer
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              StringValue         - Integer list to be compiled
 *              Field               - Current field object
 *              ByteLength          - Byte length of the integer list
 *
 * RETURN:      Count of remaining data in the input list
 *
 * DESCRIPTION: Compile and pack an integer list, for example
 *              "AA 1F 20 3B" ==> Buffer[] = {0xAA,0x1F,0x20,0x3B}
 *
 *****************************************************************************/

UINT32
DtCompileBuffer (
    UINT8                   *Buffer,
    char                    *StringValue,
    DT_FIELD                *Field,
    UINT32                  ByteLength)
{
    ACPI_STATUS             Status;
    char                    Hex[3];
    UINT64                  Value;
    UINT32                  i;
    UINT32                  Count;


    /* Allow several different types of value separators */

    StringValue = DtNormalizeBuffer (StringValue, &Count);

    Hex[2] = 0;
    for (i = 0; i < Count; i++)
    {
        /* Each element of StringValue is three chars */

        Hex[0] = StringValue[(3 * i)];
        Hex[1] = StringValue[(3 * i) + 1];

        /* Convert one hex byte */

        Value = 0;
        Status = DtStrtoul64 (Hex, &Value);
        if (ACPI_FAILURE (Status))
        {
            DtError (ASL_ERROR, ASL_MSG_BUFFER_ELEMENT, Field, MsgBuffer);
            return (ByteLength - Count);
        }

        Buffer[i] = (UINT8) Value;
    }

    ACPI_FREE (StringValue);
    return (ByteLength - Count);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileFlag
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              Field               - Field to be compiled
 *              Info                - Flag info
 *
 * RETURN:
 *
 * DESCRIPTION: Compile a flag
 *
 *****************************************************************************/

void
DtCompileFlag (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    ACPI_DMTABLE_INFO       *Info)
{
    UINT64                  Value = 0;
    UINT32                  BitLength = 1;
    UINT8                   BitPosition = 0;
    ACPI_STATUS             Status;


    Status = DtStrtoul64 (Field->Value, &Value);
    if (ACPI_FAILURE (Status))
    {
        DtError (ASL_ERROR, ASL_MSG_INVALID_HEX_INTEGER, Field, NULL);
    }

    switch (Info->Opcode)
    {
    case ACPI_DMT_FLAG0:
    case ACPI_DMT_FLAG1:
    case ACPI_DMT_FLAG2:
    case ACPI_DMT_FLAG3:
    case ACPI_DMT_FLAG4:
    case ACPI_DMT_FLAG5:
    case ACPI_DMT_FLAG6:
    case ACPI_DMT_FLAG7:

        BitPosition = Info->Opcode;
        BitLength = 1;
        break;

    case ACPI_DMT_FLAGS0:

        BitPosition = 0;
        BitLength = 2;
        break;


    case ACPI_DMT_FLAGS2:

        BitPosition = 2;
        BitLength = 2;
        break;

    default:

        DtFatal (ASL_MSG_COMPILER_INTERNAL, Field, "Invalid flag opcode");
        break;
    }

    /* Check range of the input flag value */

    if (Value >= ((UINT64) 1 << BitLength))
    {
        sprintf (MsgBuffer, "Maximum %u bit", BitLength);
        DtError (ASL_ERROR, ASL_MSG_FLAG_VALUE, Field, MsgBuffer);
        Value = 0;
    }

    *Buffer |= (UINT8) (Value << BitPosition);
}
