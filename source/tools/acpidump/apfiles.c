/******************************************************************************
 *
 * Module Name: apfiles - File-related functions for acpidump utility
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

#include "acpidump.h"
#include "acapps.h"


/* Local prototypes */

static int
ApIsExistingFile (
    char                    *Pathname);


static int
ApIsExistingFile (
    char                    *Pathname)
{
#ifndef _GNU_EFI
    struct stat             StatInfo;


    if (!stat (Pathname, &StatInfo))
    {
        AcpiLogError ("Target path already exists, overwrite? [y|n] ");

        if (getchar () != 'y')
        {
            return (-1);
        }
    }
#endif

    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    ApOpenOutputFile
 *
 * PARAMETERS:  Pathname            - Output filename
 *
 * RETURN:      Open file handle
 *
 * DESCRIPTION: Open a text output file for acpidump. Checks if file already
 *              exists.
 *
 ******************************************************************************/

int
ApOpenOutputFile (
    char                    *Pathname)
{
    ACPI_FILE               File;


    /* If file exists, prompt for overwrite */

    if (ApIsExistingFile (Pathname) != 0)
    {
        return (-1);
    }

    /* Point stdout to the file */

    File = AcpiOsOpenFile (Pathname, ACPI_FILE_WRITING);
    if (!File)
    {
        AcpiLogError ("Could not open output file: %s\n", Pathname);
        return (-1);
    }

    /* Save the file and path */

    Gbl_OutputFile = File;
    Gbl_OutputFilename = Pathname;
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApWriteToBinaryFile
 *
 * PARAMETERS:  Table               - ACPI table to be written
 *              Instance            - ACPI table instance no. to be written
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write an ACPI table to a binary file. Builds the output
 *              filename from the table signature.
 *
 ******************************************************************************/

int
ApWriteToBinaryFile (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Instance)
{
    char                    Filename[ACPI_NAME_SIZE + 16];
    char                    InstanceStr [16];
    ACPI_FILE               File;
    size_t                  Actual;
    UINT32                  TableLength;


    /* Obtain table length */

    TableLength = ApGetTableLength (Table);

    /* Construct lower-case filename from the table local signature */

    if (ACPI_VALIDATE_RSDP_SIG (Table->Signature))
    {
        ACPI_MOVE_NAME (Filename, ACPI_RSDP_NAME);
    }
    else
    {
        ACPI_MOVE_NAME (Filename, Table->Signature);
    }
    Filename[0] = (char) tolower ((int) Filename[0]);
    Filename[1] = (char) tolower ((int) Filename[1]);
    Filename[2] = (char) tolower ((int) Filename[2]);
    Filename[3] = (char) tolower ((int) Filename[3]);
    Filename[ACPI_NAME_SIZE] = 0;

    /* Handle multiple SSDTs - create different filenames for each */

    if (Instance > 0)
    {
        AcpiUtSnprintf (InstanceStr, sizeof (InstanceStr), "%u", Instance);
        strcat (Filename, InstanceStr);
    }

    strcat (Filename, ACPI_TABLE_FILE_SUFFIX);

    if (Gbl_VerboseMode)
    {
        AcpiLogError (
            "Writing [%4.4s] to binary file: %s 0x%X (%u) bytes\n",
            Table->Signature, Filename, Table->Length, Table->Length);
    }

    /* Open the file and dump the entire table in binary mode */

    File = AcpiOsOpenFile (Filename,
        ACPI_FILE_WRITING | ACPI_FILE_BINARY);
    if (!File)
    {
        AcpiLogError ("Could not open output file: %s\n", Filename);
        return (-1);
    }

    Actual = AcpiOsWriteFile (File, Table, 1, TableLength);
    if (Actual != TableLength)
    {
        AcpiLogError ("Error writing binary output file: %s\n", Filename);
        AcpiOsCloseFile (File);
        return (-1);
    }

    AcpiOsCloseFile (File);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApGetTableFromFile
 *
 * PARAMETERS:  Pathname            - File containing the binary ACPI table
 *              OutFileSize         - Where the file size is returned
 *
 * RETURN:      Buffer containing the ACPI table. NULL on error.
 *
 * DESCRIPTION: Open a file and read it entirely into a new buffer
 *
 ******************************************************************************/

ACPI_TABLE_HEADER *
ApGetTableFromFile (
    char                    *Pathname,
    UINT32                  *OutFileSize)
{
    ACPI_TABLE_HEADER       *Buffer = NULL;
    ACPI_FILE               File;
    UINT32                  FileSize;
    size_t                  Actual;


    /* Must use binary mode */

    File = AcpiOsOpenFile (Pathname, ACPI_FILE_READING | ACPI_FILE_BINARY);
    if (!File)
    {
        AcpiLogError ("Could not open input file: %s\n", Pathname);
        return (NULL);
    }

    /* Need file size to allocate a buffer */

    FileSize = CmGetFileSize (File);
    if (FileSize == ACPI_UINT32_MAX)
    {
        AcpiLogError (
            "Could not get input file size: %s\n", Pathname);
        goto Cleanup;
    }

    /* Allocate a buffer for the entire file */

    Buffer = ACPI_ALLOCATE_ZEROED (FileSize);
    if (!Buffer)
    {
        AcpiLogError (
            "Could not allocate file buffer of size: %u\n", FileSize);
        goto Cleanup;
    }

    /* Read the entire file */

    Actual = AcpiOsReadFile (File, Buffer, 1, FileSize);
    if (Actual != FileSize)
    {
        AcpiLogError (
            "Could not read input file: %s\n", Pathname);
        ACPI_FREE (Buffer);
        Buffer = NULL;
        goto Cleanup;
    }

    *OutFileSize = FileSize;

Cleanup:
    AcpiOsCloseFile (File);
    return (Buffer);
}
