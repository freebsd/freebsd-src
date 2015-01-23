/******************************************************************************
 *
 * Module Name: cfsize - Common get file size function
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acapps.h>
#include <stdio.h>

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("cmfsize")


/*******************************************************************************
 *
 * FUNCTION:    CmGetFileSize
 *
 * PARAMETERS:  File                    - Open file descriptor
 *
 * RETURN:      File Size. On error, -1 (ACPI_UINT32_MAX)
 *
 * DESCRIPTION: Get the size of a file. Uses seek-to-EOF. File must be open.
 *              Does not disturb the current file pointer.
 *
 ******************************************************************************/

UINT32
CmGetFileSize (
    ACPI_FILE               File)
{
    long                    FileSize;
    long                    CurrentOffset;
    ACPI_STATUS             Status;


    /* Save the current file pointer, seek to EOF to obtain file size */

    CurrentOffset = AcpiOsGetFileOffset (File);
    if (CurrentOffset < 0)
    {
        goto OffsetError;
    }

    Status = AcpiOsSetFileOffset (File, 0, ACPI_FILE_END);
    if (ACPI_FAILURE (Status))
    {
        goto SeekError;
    }

    FileSize = AcpiOsGetFileOffset (File);
    if (FileSize < 0)
    {
        goto OffsetError;
    }

    /* Restore original file pointer */

    Status = AcpiOsSetFileOffset (File, CurrentOffset, ACPI_FILE_BEGIN);
    if (ACPI_FAILURE (Status))
    {
        goto SeekError;
    }

    return ((UINT32) FileSize);


OffsetError:
    AcpiLogError ("Could not get file offset");
    return (ACPI_UINT32_MAX);

SeekError:
    AcpiLogError ("Could not set file offset");
    return (ACPI_UINT32_MAX);
}
