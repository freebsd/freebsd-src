/******************************************************************************
 *
 * Module Name: asluuid-- compiler UUID support
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


#include "aslcompiler.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asluuid")


/*
 * UUID support functions.
 *
 * This table is used to convert an input UUID ascii string to a 16 byte
 * buffer and the reverse. The table maps a UUID buffer index 0-15 to
 * the index within the 36-byte UUID string where the associated 2-byte
 * hex value can be found.
 *
 * 36-byte UUID strings are of the form:
 *     aabbccdd-eeff-gghh-iijj-kkllmmnnoopp
 * Where aa-pp are one byte hex numbers, made up of two hex digits
 *
 * Note: This table is basically the inverse of the string-to-offset table
 * found in the ACPI spec in the description of the ToUUID macro.
 */
static UINT8    Gbl_MapToUuidOffset[16] =
{
    6,4,2,0,11,9,16,14,19,21,24,26,28,30,32,34
};

#define UUID_BUFFER_LENGTH          16
#define UUID_STRING_LENGTH          36

/* Positions for required hyphens (dashes) in UUID strings */

#define UUID_HYPHEN1_OFFSET         8
#define UUID_HYPHEN2_OFFSET         13
#define UUID_HYPHEN3_OFFSET         18
#define UUID_HYPHEN4_OFFSET         23


/*******************************************************************************
 *
 * FUNCTION:    AuValiduateUuid
 *
 * PARAMETERS:  InString            - 36-byte formatted UUID string
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check all 36 characters for correct format
 *
 ******************************************************************************/

ACPI_STATUS
AuValidateUuid (
    char                    *InString)
{
    UINT32                  i;


    if (!InString || (ACPI_STRLEN (InString) != UUID_STRING_LENGTH))
    {
        return (AE_BAD_PARAMETER);
    }

    /* Check all 36 characters for correct format */

    for (i = 0; i < UUID_STRING_LENGTH; i++)
    {
        /* Must have 4 hyphens (dashes) in these positions: */

        if ((i == UUID_HYPHEN1_OFFSET) ||
            (i == UUID_HYPHEN2_OFFSET) ||
            (i == UUID_HYPHEN3_OFFSET) ||
            (i == UUID_HYPHEN4_OFFSET))
        {
            if (InString[i] != '-')
            {
                return (AE_BAD_PARAMETER);
            }
        }

        /* All other positions must contain hex digits */

        else
        {
            if (!isxdigit ((int) InString[i]))
            {
                return (AE_BAD_PARAMETER);
            }
        }
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AuConvertStringToUuid
 *
 * PARAMETERS:  InString            - 36-byte formatted UUID string
 *              UuidBuffer          - 16-byte UUID buffer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert 36-byte formatted UUID string to 16-byte UUID buffer
 *
 ******************************************************************************/

ACPI_STATUS
AuConvertStringToUuid (
    char                    *InString,
    char                    *UuidBuffer)
{
    UINT32                  i;


    if (!InString || !UuidBuffer)
    {
        return (AE_BAD_PARAMETER);
    }

    for (i = 0; i < UUID_BUFFER_LENGTH; i++)
    {
        UuidBuffer[i]  = (char) (UtHexCharToValue (InString[Gbl_MapToUuidOffset[i]]) << 4);
        UuidBuffer[i] |= (char)  UtHexCharToValue (InString[Gbl_MapToUuidOffset[i] + 1]);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AuConvertUuidToString
 *
 * PARAMETERS:  UuidBuffer          - 16-byte UUID buffer
 *              OutString           - 36-byte formatted UUID string
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert 16-byte UUID buffer to 36-byte formatted UUID string
 *              OutString must be 37 bytes to include null terminator.
 *
 ******************************************************************************/

ACPI_STATUS
AuConvertUuidToString (
    char                    *UuidBuffer,
    char                    *OutString)
{
    UINT32                  i;


    if (!UuidBuffer || !OutString)
    {
        return (AE_BAD_PARAMETER);
    }

    for (i = 0; i < UUID_BUFFER_LENGTH; i++)
    {
        OutString[Gbl_MapToUuidOffset[i]] =     (UINT8) AslHexLookup[(UuidBuffer[i] >> 4) & 0xF];
        OutString[Gbl_MapToUuidOffset[i] + 1] = (UINT8) AslHexLookup[UuidBuffer[i] & 0xF];
    }

    /* Insert required hyphens (dashes) */

    OutString[UUID_HYPHEN1_OFFSET] =
    OutString[UUID_HYPHEN2_OFFSET] =
    OutString[UUID_HYPHEN3_OFFSET] =
    OutString[UUID_HYPHEN4_OFFSET] = '-';

    OutString[UUID_STRING_LENGTH] = 0; /* Null terminate */
    return (AE_OK);
}
