/******************************************************************************
 *
 * Module Name: asluuid-- compiler UUID support
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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


extern UINT8                AcpiGbl_MapToUuidOffset[UUID_BUFFER_LENGTH];


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


    if (!InString || (strlen (InString) != UUID_STRING_LENGTH))
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
        else
        {
            /* All other positions must contain hex digits */

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
        OutString[AcpiGbl_MapToUuidOffset[i]] =
            AcpiUtHexToAsciiChar (UuidBuffer[i], 4);

        OutString[AcpiGbl_MapToUuidOffset[i] + 1] =
            AcpiUtHexToAsciiChar (UuidBuffer[i], 0);
    }

    /* Insert required hyphens (dashes) */

    OutString[UUID_HYPHEN1_OFFSET] =
    OutString[UUID_HYPHEN2_OFFSET] =
    OutString[UUID_HYPHEN3_OFFSET] =
    OutString[UUID_HYPHEN4_OFFSET] = '-';

    OutString[UUID_STRING_LENGTH] = 0; /* Null terminate */
    return (AE_OK);
}
