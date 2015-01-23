/******************************************************************************
 *
 * Module Name: oslibcfs - C library OSL for file I/O
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
#include <stdio.h>
#include <stdarg.h>

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("oslibcfs")


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsOpenFile
 *
 * PARAMETERS:  Path                - File path
 *              Modes               - File operation type
 *
 * RETURN:      File descriptor.
 *
 * DESCRIPTION: Open a file for reading (ACPI_FILE_READING) or/and writing
 *              (ACPI_FILE_WRITING).
 *
 ******************************************************************************/

ACPI_FILE
AcpiOsOpenFile (
    const char              *Path,
    UINT8                   Modes)
{
    ACPI_FILE               File;
    UINT32                  i = 0;
    char                    ModesStr[4];


    if (Modes & ACPI_FILE_READING)
    {
        ModesStr[i++] = 'r';
    }
    if (Modes & ACPI_FILE_WRITING)
    {
        ModesStr[i++] = 'w';
    }
    if (Modes & ACPI_FILE_BINARY)
    {
        ModesStr[i++] = 'b';
    }

    ModesStr[i++] = '\0';

    File = fopen (Path, ModesStr);
    if (!File)
    {
        perror ("Could not open file");
    }

    return (File);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsCloseFile
 *
 * PARAMETERS:  File                - An open file descriptor
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Close a file opened via AcpiOsOpenFile.
 *
 ******************************************************************************/

void
AcpiOsCloseFile (
    ACPI_FILE               File)
{
    fclose (File);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsReadFile
 *
 * PARAMETERS:  File                - An open file descriptor
 *              Buffer              - Data buffer
 *              Size                - Data block size
 *              Count               - Number of data blocks
 *
 * RETURN:      Number of bytes actually read.
 *
 * DESCRIPTION: Read from a file.
 *
 ******************************************************************************/

int
AcpiOsReadFile (
    ACPI_FILE               File,
    void                    *Buffer,
    ACPI_SIZE               Size,
    ACPI_SIZE               Count)
{
    int                     Length;


    Length = fread (Buffer, Size, Count, File);
    if (Length < 0)
    {
        perror ("Error reading file");
    }

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsWriteFile
 *
 * PARAMETERS:  File                - An open file descriptor
 *              Buffer              - Data buffer
 *              Size                - Data block size
 *              Count               - Number of data blocks
 *
 * RETURN:      Number of bytes actually written.
 *
 * DESCRIPTION: Write to a file.
 *
 ******************************************************************************/

int
AcpiOsWriteFile (
    ACPI_FILE               File,
    void                    *Buffer,
    ACPI_SIZE               Size,
    ACPI_SIZE               Count)
{
    int                     Length;


    Length = fwrite (Buffer, Size, Count, File);
    if (Length < 0)
    {
        perror ("Error writing file");
    }

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsGetFileOffset
 *
 * PARAMETERS:  File                - An open file descriptor
 *
 * RETURN:      Current file pointer position.
 *
 * DESCRIPTION: Get current file offset.
 *
 ******************************************************************************/

long
AcpiOsGetFileOffset (
    ACPI_FILE               File)
{
    long                    Offset;


    Offset = ftell (File);
    return (Offset);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsSetFileOffset
 *
 * PARAMETERS:  File                - An open file descriptor
 *              Offset              - New file offset
 *              From                - From begin/end of file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Set current file offset.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiOsSetFileOffset (
    ACPI_FILE               File,
    long                    Offset,
    UINT8                   From)
{
    int                     Ret = 0;


    if (From == ACPI_FILE_BEGIN)
    {
        Ret = fseek (File, Offset, SEEK_SET);
    }
    if (From == ACPI_FILE_END)
    {
        Ret = fseek (File, Offset, SEEK_END);
    }

    if (Ret < 0)
    {
        return (AE_ERROR);
    }
    else
    {
        return (AE_OK);
    }
}
