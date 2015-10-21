/******************************************************************************
 *
 * Module Name: aslfileio - File I/O support
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
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslfileio")


/*******************************************************************************
 *
 * FUNCTION:    FlFileError
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              ErrorId             - Index into error message array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode errno to an error message and add the entire error
 *              to the error log.
 *
 ******************************************************************************/

void
FlFileError (
    UINT32                  FileId,
    UINT8                   ErrorId)
{

    sprintf (MsgBuffer, "\"%s\" (%s) - %s", Gbl_Files[FileId].Filename,
        Gbl_Files[FileId].Description, strerror (errno));
    AslCommonError (ASL_ERROR, ErrorId, 0, 0, 0, 0, NULL, MsgBuffer);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Filename            - file pathname to open
 *              Mode                - Open mode for fopen
 *
 * RETURN:      None
 *
 * DESCRIPTION: Open a file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlOpenFile (
    UINT32                  FileId,
    char                    *Filename,
    char                    *Mode)
{
    FILE                    *File;


    Gbl_Files[FileId].Filename = Filename;
    Gbl_Files[FileId].Handle = NULL;

    File = fopen (Filename, Mode);
    if (!File)
    {
        FlFileError (FileId, ASL_MSG_OPEN);
        AslAbort ();
    }

    Gbl_Files[FileId].Handle = File;
}


/*******************************************************************************
 *
 * FUNCTION:    FlGetFileSize
 *
 * PARAMETERS:  FileId              - Index into file info array
 *
 * RETURN:      File Size
 *
 * DESCRIPTION: Get current file size. Uses common seek-to-EOF function.
 *              File must be open. Aborts compiler on error.
 *
 ******************************************************************************/

UINT32
FlGetFileSize (
    UINT32                  FileId)
{
    UINT32                  FileSize;


    FileSize = CmGetFileSize (Gbl_Files[FileId].Handle);
    if (FileSize == ACPI_UINT32_MAX)
    {
        AslAbort();
    }

    return (FileSize);
}


/*******************************************************************************
 *
 * FUNCTION:    FlReadFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Buffer              - Where to place the data
 *              Length              - Amount to read
 *
 * RETURN:      Status. AE_ERROR indicates EOF.
 *
 * DESCRIPTION: Read data from an open file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

ACPI_STATUS
FlReadFile (
    UINT32                  FileId,
    void                    *Buffer,
    UINT32                  Length)
{
    UINT32                  Actual;


    /* Read and check for error */

    Actual = fread (Buffer, 1, Length, Gbl_Files[FileId].Handle);
    if (Actual < Length)
    {
        if (feof (Gbl_Files[FileId].Handle))
        {
            /* End-of-file, just return error */

            return (AE_ERROR);
        }

        FlFileError (FileId, ASL_MSG_READ);
        AslAbort ();
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlWriteFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Buffer              - Data to write
 *              Length              - Amount of data to write
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write data to an open file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlWriteFile (
    UINT32                  FileId,
    void                    *Buffer,
    UINT32                  Length)
{
    UINT32                  Actual;


    /* Write and check for error */

    Actual = fwrite ((char *) Buffer, 1, Length, Gbl_Files[FileId].Handle);
    if (Actual != Length)
    {
        FlFileError (FileId, ASL_MSG_WRITE);
        AslAbort ();
    }

    if ((FileId == ASL_FILE_PREPROCESSOR) && Gbl_PreprocessorOutputFlag)
    {
        /* Duplicate the output to the user preprocessor (.i) file */

        Actual = fwrite ((char *) Buffer, 1, Length,
            Gbl_Files[ASL_FILE_PREPROCESSOR_USER].Handle);
        if (Actual != Length)
        {
            FlFileError (FileId, ASL_MSG_WRITE);
            AslAbort ();
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlPrintFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Format              - Printf format string
 *              ...                 - Printf arguments
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted write to an open file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlPrintFile (
    UINT32                  FileId,
    char                    *Format,
    ...)
{
    INT32                   Actual;
    va_list                 Args;


    va_start (Args, Format);
    Actual = vfprintf (Gbl_Files[FileId].Handle, Format, Args);
    va_end (Args);

    if (Actual == -1)
    {
        FlFileError (FileId, ASL_MSG_WRITE);
        AslAbort ();
    }

    if ((FileId == ASL_FILE_PREPROCESSOR) && Gbl_PreprocessorOutputFlag)
    {
        /*
         * Duplicate the output to the user preprocessor (.i) file,
         * except: no #line directives.
         */
        if (!strncmp (Format, "#line", 5))
        {
            return;
        }

        va_start (Args, Format);
        Actual = vfprintf (Gbl_Files[ASL_FILE_PREPROCESSOR_USER].Handle,
            Format, Args);
        va_end (Args);

        if (Actual == -1)
        {
            FlFileError (FileId, ASL_MSG_WRITE);
            AslAbort ();
        }
    }

}


/*******************************************************************************
 *
 * FUNCTION:    FlSeekFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Offset              - Absolute byte offset in file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Seek to absolute offset.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlSeekFile (
    UINT32                  FileId,
    long                    Offset)
{
    int                     Error;


    Error = fseek (Gbl_Files[FileId].Handle, Offset, SEEK_SET);
    if (Error)
    {
        FlFileError (FileId, ASL_MSG_SEEK);
        AslAbort ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlCloseFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Close an open file. Aborts compiler on error
 *
 ******************************************************************************/

void
FlCloseFile (
    UINT32                  FileId)
{
    int                     Error;


    if (!Gbl_Files[FileId].Handle)
    {
        return;
    }

    Error = fclose (Gbl_Files[FileId].Handle);
    if (Error)
    {
        FlFileError (FileId, ASL_MSG_CLOSE);
        AslAbort ();
    }

    /* Do not clear/free the filename string */

    Gbl_Files[FileId].Handle = NULL;
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    FlDeleteFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a file.
 *
 ******************************************************************************/

void
FlDeleteFile (
    UINT32                  FileId)
{
    ASL_FILE_INFO           *Info = &Gbl_Files[FileId];


    if (!Info->Filename)
    {
        return;
    }

    if (remove (Info->Filename))
    {
        printf ("%s (%s file) ",
            Info->Filename, Info->Description);
        perror ("Could not delete");
    }

    Info->Filename = NULL;
    return;
}
