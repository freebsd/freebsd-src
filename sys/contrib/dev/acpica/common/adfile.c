/******************************************************************************
 *
 * Module Name: adfile - Application-level disassembler file support routines
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acapps.h>

#include <stdio.h>


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("adfile")

/* Local prototypes */

static INT32
AdWriteBuffer (
    char                    *Filename,
    char                    *Buffer,
    UINT32                  Length);

static char                 FilenameBuf[20];


/******************************************************************************
 *
 * FUNCTION:    AfGenerateFilename
 *
 * PARAMETERS:  Prefix              - prefix string
 *              TableId             - The table ID
 *
 * RETURN:      Pointer to the completed string
 *
 * DESCRIPTION: Build an output filename from an ACPI table ID string
 *
 ******************************************************************************/

char *
AdGenerateFilename (
    char                    *Prefix,
    char                    *TableId)
{
    UINT32                  i;
    UINT32                  j;


    for (i = 0; Prefix[i]; i++)
    {
        FilenameBuf[i] = Prefix[i];
    }

    FilenameBuf[i] = '_';
    i++;

    for (j = 0; j < 8 && (TableId[j] != ' ') && (TableId[j] != 0); i++, j++)
    {
        FilenameBuf[i] = TableId[j];
    }

    FilenameBuf[i] = 0;
    strcat (FilenameBuf, ACPI_TABLE_FILE_SUFFIX);
    return (FilenameBuf);
}


/******************************************************************************
 *
 * FUNCTION:    AfWriteBuffer
 *
 * PARAMETERS:  Filename            - name of file
 *              Buffer              - data to write
 *              Length              - length of data
 *
 * RETURN:      Actual number of bytes written
 *
 * DESCRIPTION: Open a file and write out a single buffer
 *
 ******************************************************************************/

static INT32
AdWriteBuffer (
    char                    *Filename,
    char                    *Buffer,
    UINT32                  Length)
{
    FILE                    *File;
    ACPI_SIZE               Actual;


    File = fopen (Filename, "wb");
    if (!File)
    {
        printf ("Could not open file %s\n", Filename);
        return (-1);
    }

    Actual = fwrite (Buffer, 1, (size_t) Length, File);
    if (Actual != Length)
    {
        printf ("Could not write to file %s\n", Filename);
    }

    fclose (File);
    return ((INT32) Actual);
}


/******************************************************************************
 *
 * FUNCTION:    AfWriteTable
 *
 * PARAMETERS:  Table               - pointer to the ACPI table
 *              Length              - length of the table
 *              TableName           - the table signature
 *              OemTableID          - from the table header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the loaded tables to a file (or files)
 *
 ******************************************************************************/

void
AdWriteTable (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length,
    char                    *TableName,
    char                    *OemTableId)
{
    char                    *Filename;


    Filename = AdGenerateFilename (TableName, OemTableId);
    AdWriteBuffer (Filename, (char *) Table, Length);

    AcpiOsPrintf ("Table [%s] written to \"%s\"\n", TableName, Filename);
}


/*******************************************************************************
 *
 * FUNCTION:    FlGenerateFilename
 *
 * PARAMETERS:  InputFilename       - Original ASL source filename
 *              Suffix              - New extension.
 *
 * RETURN:      New filename containing the original base + the new suffix
 *
 * DESCRIPTION: Generate a new filename from the ASL source filename and a new
 *              extension. Used to create the *.LST, *.TXT, etc. files.
 *
 ******************************************************************************/

char *
FlGenerateFilename (
    char                    *InputFilename,
    char                    *Suffix)
{
    char                    *Position;
    char                    *NewFilename;
    char                    *DirectoryPosition;


    /*
     * Copy the original filename to a new buffer. Leave room for the worst
     * case where we append the suffix, an added dot and the null terminator.
     */
    NewFilename = UtStringCacheCalloc ((ACPI_SIZE)
        strlen (InputFilename) + strlen (Suffix) + 2);
    if (!NewFilename)
    {
        return (NULL);
    }

    strcpy (NewFilename, InputFilename);

    /* Try to find the last dot in the filename */

    DirectoryPosition = strrchr (NewFilename, '/');
    Position = strrchr (NewFilename, '.');

    if (Position && (Position > DirectoryPosition))
    {
        /* Tack on the new suffix */

        Position++;
        *Position = 0;
        strcat (Position, Suffix);
    }
    else
    {
        /* No dot, add one and then the suffix */

        strcat (NewFilename, ".");
        strcat (NewFilename, Suffix);
    }

    return (NewFilename);
}


/*******************************************************************************
 *
 * FUNCTION:    FlStrdup
 *
 * DESCRIPTION: Local strdup function
 *
 ******************************************************************************/

static char *
FlStrdup (
    char                *String)
{
    char                *NewString;


    NewString = UtStringCacheCalloc ((ACPI_SIZE) strlen (String) + 1);
    if (!NewString)
    {
        return (NULL);
    }

    strcpy (NewString, String);
    return (NewString);
}


/*******************************************************************************
 *
 * FUNCTION:    FlSplitInputPathname
 *
 * PARAMETERS:  InputFilename       - The user-specified ASL source file to be
 *                                    compiled
 *              OutDirectoryPath    - Where the directory path prefix is
 *                                    returned
 *              OutFilename         - Where the filename part is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Split the input path into a directory and filename part
 *              1) Directory part used to open include files
 *              2) Filename part used to generate output filenames
 *
 ******************************************************************************/

ACPI_STATUS
FlSplitInputPathname (
    char                    *InputPath,
    char                    **OutDirectoryPath,
    char                    **OutFilename)
{
    char                    *Substring;
    char                    *DirectoryPath;
    char                    *Filename;


    if (OutDirectoryPath)
    {
        *OutDirectoryPath = NULL;
    }

    if (!InputPath)
    {
        return (AE_OK);
    }

    /* Get the path to the input filename's directory */

    DirectoryPath = FlStrdup (InputPath);
    if (!DirectoryPath)
    {
        return (AE_NO_MEMORY);
    }

    /* Convert backslashes to slashes in the entire path */

    UtConvertBackslashes (DirectoryPath);

    /* Backup to last slash or colon */

    Substring = strrchr (DirectoryPath, '/');
    if (!Substring)
    {
        Substring = strrchr (DirectoryPath, ':');
    }

    /* Extract the simple filename */

    if (!Substring)
    {
        Filename = FlStrdup (DirectoryPath);
        DirectoryPath[0] = 0;
    }
    else
    {
        Filename = FlStrdup (Substring + 1);
        *(Substring+1) = 0;
    }

    if (!Filename)
    {
        return (AE_NO_MEMORY);
    }

    if (OutDirectoryPath)
    {
        *OutDirectoryPath = DirectoryPath;
    }

    if (OutFilename)
    {
        *OutFilename = Filename;
        return (AE_OK);
    }

    return (AE_OK);
}
