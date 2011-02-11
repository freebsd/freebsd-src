/******************************************************************************
 *
 * Module Name: dtfield.c - Code generation for individual source fields
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

#define __DTFIELD_C__

#include "aslcompiler.h"
#include "dtcompiler.h"

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtfield")


/* Local prototypes */

static void
DtCompileString (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength);

static void
DtCompileUnicode (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength);

static ACPI_STATUS
DtCompileUuid (
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
    ACPI_STATUS             Status;

    switch (Type)
    {
    case DT_FIELD_TYPE_INTEGER:
        DtCompileInteger (Buffer, Field, ByteLength, Flags);
        break;

    case DT_FIELD_TYPE_STRING:
        DtCompileString (Buffer, Field, ByteLength);
        break;

    case DT_FIELD_TYPE_UUID:
        Status = DtCompileUuid (Buffer, Field, ByteLength);
        if (ACPI_SUCCESS (Status))
        {
            break;
        }

        /* Fall through. */

    case DT_FIELD_TYPE_BUFFER:
        DtCompileBuffer (Buffer, Field->Value, Field, ByteLength);
        break;

    case DT_FIELD_TYPE_UNICODE:
        DtCompileUnicode (Buffer, Field, ByteLength);
        break;

    case DT_FIELD_TYPE_DEVICE_PATH:
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
 * FUNCTION:    DtCompileUnicode
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              Field               - String to be copied to buffer
 *              ByteLength          - Maximum length of string
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert ASCII string to Unicode string
 *
 * Note:  The Unicode string is 16 bits per character, no leading signature,
 *        with a 16-bit terminating NULL.
 *
 *****************************************************************************/

static void
DtCompileUnicode (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength)
{
    UINT32                  Count;
    UINT32                  i;
    char                    *AsciiString;
    UINT16                  *UnicodeString;


    AsciiString = Field->Value;
    UnicodeString = (UINT16 *) Buffer;
    Count = ACPI_STRLEN (AsciiString) + 1;

    /* Convert to Unicode string (including null terminator) */

    for (i = 0; i < Count; i++)
    {
        UnicodeString[i] = (UINT16) AsciiString[i];
    }
}


/*******************************************************************************
 *
 * FUNCTION:    DtCompileUuid
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              Field               - String to be copied to buffer
 *              ByteLength          - Maximum length of string
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert UUID string to 16-byte buffer
 *
 ******************************************************************************/

static ACPI_STATUS
DtCompileUuid (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength)
{
    char                    *InString;
    ACPI_STATUS             Status;


    InString = Field->Value;

    Status = AuValidateUuid (InString);
    if (ACPI_FAILURE (Status))
    {
        sprintf (MsgBuffer, "%s", Field->Value);
        DtNameError (ASL_ERROR, ASL_MSG_INVALID_UUID, Field, MsgBuffer);
    }
    else
    {
        Status = AuConvertStringToUuid (InString, (char *) Buffer);
    }

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileInteger
 *
 * PARAMETERS:  Buffer              - Output buffer
 *              Field               - Field obj with Integer to be compiled
 *              ByteLength          - Byte length of the integer
 *              Flags               - Additional compile info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Compile an integer. Supports integer expressions with C-style
 *              operators.
 *
 *****************************************************************************/

void
DtCompileInteger (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength,
    UINT8                   Flags)
{
    UINT64                  Value;
    UINT64                  MaxValue;


    /* Output buffer byte length must be in range 1-8 */

    if ((ByteLength > 8) || (ByteLength == 0))
    {
        DtFatal (ASL_MSG_COMPILER_INTERNAL, Field,
            "Invalid internal Byte length");
        return;
    }

    /* Resolve integer expression to a single integer value */

    Value = DtResolveIntegerExpression (Field);

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
        sprintf (MsgBuffer, "%8.8X%8.8X", ACPI_FORMAT_UINT64 (Value));
        DtError (ASL_ERROR, ASL_MSG_INTEGER_SIZE, Field, MsgBuffer);
    }

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
