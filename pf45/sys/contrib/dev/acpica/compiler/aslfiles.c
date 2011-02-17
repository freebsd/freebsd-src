
/******************************************************************************
 *
 * Module Name: aslfiles - file I/O suppoert
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslfiles")

/* Local prototypes */

static FILE *
FlOpenIncludeWithPrefix (
    char                    *PrefixDir,
    char                    *Filename);


#ifdef ACPI_OBSOLETE_FUNCTIONS
ACPI_STATUS
FlParseInputPathname (
    char                    *InputFilename);
#endif


/*******************************************************************************
 *
 * FUNCTION:    AslAbort
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the error log and abort the compiler.  Used for serious
 *              I/O errors
 *
 ******************************************************************************/

void
AslAbort (
    void)
{

    AePrintErrorLog (ASL_FILE_STDOUT);
    if (Gbl_DebugFlag)
    {
        /* Print error summary to the debug file */

        AePrintErrorLog (ASL_FILE_STDERR);
    }

    exit (1);
}


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

    sprintf (MsgBuffer, "\"%s\" (%s)", Gbl_Files[FileId].Filename,
        strerror (errno));
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


    File = fopen (Filename, Mode);

    Gbl_Files[FileId].Filename = Filename;
    Gbl_Files[FileId].Handle   = File;

    if (!File)
    {
        FlFileError (FileId, ASL_MSG_OPEN);
        AslAbort ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlGetFileSize
 *
 * PARAMETERS:  FileId              - Index into file info array
 *
 * RETURN:      File Size
 *
 * DESCRIPTION: Get current file size. Uses seek-to-EOF. File must be open.
 *
 ******************************************************************************/

UINT32
FlGetFileSize (
    UINT32                  FileId)
{
    FILE                    *fp;
    UINT32                  FileSize;


    fp = Gbl_Files[FileId].Handle;

    fseek (fp, 0, SEEK_END);
    FileSize = (UINT32) ftell (fp);
    fseek (fp, 0, SEEK_SET);

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
 * RETURN:      Status.  AE_ERROR indicates EOF.
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
    if (Actual != Length)
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
 * DESCRIPTION: Seek to absolute offset
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
 * DESCRIPTION: Close an open file.  Aborts compiler on error
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
    Gbl_Files[FileId].Handle = NULL;

    if (Error)
    {
        FlFileError (FileId, ASL_MSG_CLOSE);
        AslAbort ();
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    FlSetLineNumber
 *
 * PARAMETERS:  Op        - Parse node for the LINE asl statement
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Set the current line number
 *
 ******************************************************************************/

void
FlSetLineNumber (
    ACPI_PARSE_OBJECT       *Op)
{

    Gbl_CurrentLineNumber = (UINT32) Op->Asl.Value.Integer;
    Gbl_LogicalLineNumber = (UINT32) Op->Asl.Value.Integer;
}


/*******************************************************************************
 *
 * FUNCTION:    FlAddIncludeDirectory
 *
 * PARAMETERS:  Dir             - Directory pathname string
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add a directory the list of include prefix directories.
 *
 ******************************************************************************/

void
FlAddIncludeDirectory (
    char                    *Dir)
{
    ASL_INCLUDE_DIR         *NewDir;
    ASL_INCLUDE_DIR         *NextDir;
    ASL_INCLUDE_DIR         *PrevDir = NULL;
    UINT32                  NeedsSeparator = 0;
    size_t                  DirLength;


    DirLength = strlen (Dir);
    if (!DirLength)
    {
        return;
    }

    /* Make sure that the pathname ends with a path separator */

    if ((Dir[DirLength-1] != '/') &&
        (Dir[DirLength-1] != '\\'))
    {
        NeedsSeparator = 1;
    }

    NewDir = ACPI_ALLOCATE_ZEROED (sizeof (ASL_INCLUDE_DIR));
    NewDir->Dir = ACPI_ALLOCATE (DirLength + 1 + NeedsSeparator);
    strcpy (NewDir->Dir, Dir);
    if (NeedsSeparator)
    {
        strcat (NewDir->Dir, "/");
    }

    /*
     * Preserve command line ordering of -I options by adding new elements
     * at the end of the list
     */
    NextDir = Gbl_IncludeDirList;
    while (NextDir)
    {
        PrevDir = NextDir;
        NextDir = NextDir->Next;
    }

    if (PrevDir)
    {
        PrevDir->Next = NewDir;
    }
    else
    {
        Gbl_IncludeDirList = NewDir;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenIncludeWithPrefix
 *
 * PARAMETERS:  PrefixDir       - Prefix directory pathname. Can be a zero
 *                                length string.
 *              Filename        - The include filename from the source ASL.
 *
 * RETURN:      Valid file descriptor if successful. Null otherwise.
 *
 * DESCRIPTION: Open an include file and push it on the input file stack.
 *
 ******************************************************************************/

static FILE *
FlOpenIncludeWithPrefix (
    char                    *PrefixDir,
    char                    *Filename)
{
    FILE                    *IncludeFile;
    char                    *Pathname;


    /* Build the full pathname to the file */

    Pathname = ACPI_ALLOCATE (strlen (PrefixDir) + strlen (Filename) + 1);

    strcpy (Pathname, PrefixDir);
    strcat (Pathname, Filename);

    DbgPrint (ASL_PARSE_OUTPUT, "\nAttempt to open include file: path %s\n\n",
        Pathname);

    /* Attempt to open the file, push if successful */

    IncludeFile = fopen (Pathname, "r");
    if (IncludeFile)
    {
        /* Push the include file on the open input file stack */

        AslPushInputFileStack (IncludeFile, Pathname);
        return (IncludeFile);
    }

    ACPI_FREE (Pathname);
    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenIncludeFile
 *
 * PARAMETERS:  Op        - Parse node for the INCLUDE ASL statement
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Open an include file and push it on the input file stack.
 *
 ******************************************************************************/

void
FlOpenIncludeFile (
    ACPI_PARSE_OBJECT       *Op)
{
    FILE                    *IncludeFile;
    ASL_INCLUDE_DIR         *NextDir;


    /* Op must be valid */

    if (!Op)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_INCLUDE_FILE_OPEN,
            Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
            Gbl_InputByteCount, Gbl_CurrentColumn,
            Gbl_Files[ASL_FILE_INPUT].Filename, " - Null parse node");

        return;
    }

    /*
     * Flush out the "include ()" statement on this line, start
     * the actual include file on the next line
     */
    ResetCurrentLineBuffer ();
    FlPrintFile (ASL_FILE_SOURCE_OUTPUT, "\n");
    Gbl_CurrentLineOffset++;


    /* Attempt to open the include file */

    /* If the file specifies an absolute path, just open it */

    if ((Op->Asl.Value.String[0] == '/')  ||
        (Op->Asl.Value.String[0] == '\\') ||
        (Op->Asl.Value.String[1] == ':'))
    {
        IncludeFile = FlOpenIncludeWithPrefix ("", Op->Asl.Value.String);
        if (!IncludeFile)
        {
            goto ErrorExit;
        }
        return;
    }

    /*
     * The include filename is not an absolute path.
     *
     * First, search for the file within the "local" directory -- meaning
     * the same directory that contains the source file.
     *
     * Construct the file pathname from the global directory name.
     */
    IncludeFile = FlOpenIncludeWithPrefix (Gbl_DirectoryPath, Op->Asl.Value.String);
    if (IncludeFile)
    {
        return;
    }

    /*
     * Second, search for the file within the (possibly multiple) directories
     * specified by the -I option on the command line.
     */
    NextDir = Gbl_IncludeDirList;
    while (NextDir)
    {
        IncludeFile = FlOpenIncludeWithPrefix (NextDir->Dir, Op->Asl.Value.String);
        if (IncludeFile)
        {
            return;
        }

        NextDir = NextDir->Next;
    }

    /* We could not open the include file after trying very hard */

ErrorExit:
    sprintf (MsgBuffer, "%s, %s", Op->Asl.Value.String, strerror (errno));
    AslError (ASL_ERROR, ASL_MSG_INCLUDE_FILE_OPEN, Op, MsgBuffer);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenInputFile
 *
 * PARAMETERS:  InputFilename       - The user-specified ASL source file to be
 *                                    compiled
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open the specified input file, and save the directory path to
 *              the file so that include files can be opened in
 *              the same directory.
 *
 ******************************************************************************/

ACPI_STATUS
FlOpenInputFile (
    char                    *InputFilename)
{

    /* Open the input ASL file, text mode */

    FlOpenFile (ASL_FILE_INPUT, InputFilename, "r");
    AslCompilerin = Gbl_Files[ASL_FILE_INPUT].Handle;

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenAmlOutputFile
 *
 * PARAMETERS:  FilenamePrefix       - The user-specified ASL source file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create the output filename (*.AML) and open the file.  The file
 *              is created in the same directory as the parent input file.
 *
 ******************************************************************************/

ACPI_STATUS
FlOpenAmlOutputFile (
    char                    *FilenamePrefix)
{
    char                    *Filename;


    /* Output filename usually comes from the ASL itself */

    Filename = Gbl_Files[ASL_FILE_AML_OUTPUT].Filename;
    if (!Filename)
    {
        /* Create the output AML filename */

        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_AML_CODE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_OUTPUT_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }
    }

    /* Open the output AML file in binary mode */

    FlOpenFile (ASL_FILE_AML_OUTPUT, Filename, "w+b");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenMiscOutputFiles
 *
 * PARAMETERS:  FilenamePrefix       - The user-specified ASL source file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create and open the various output files needed, depending on
 *              the command line options
 *
 ******************************************************************************/

ACPI_STATUS
FlOpenMiscOutputFiles (
    char                    *FilenamePrefix)
{
    char                    *Filename;


    /* Create/Open a hex output file if asked */

    if (Gbl_HexOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_HEX_DUMP);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the hex file, text mode */

        FlOpenFile (ASL_FILE_HEX_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_HEX_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_HEX_OUTPUT);
    }

    /* Create/Open a debug output file if asked */

    if (Gbl_DebugFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_DEBUG);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_DEBUG_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the debug file as STDERR, text mode */

        /* TBD: hide this behind a FlReopenFile function */

        Gbl_Files[ASL_FILE_DEBUG_OUTPUT].Filename = Filename;
        Gbl_Files[ASL_FILE_DEBUG_OUTPUT].Handle =
            freopen (Filename, "w+t", stderr);

        AslCompilerSignon (ASL_FILE_DEBUG_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_DEBUG_OUTPUT);
    }

    /* Create/Open a listing output file if asked */

    if (Gbl_ListingFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_LISTING);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the listing file, text mode */

        FlOpenFile (ASL_FILE_LISTING_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_LISTING_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_LISTING_OUTPUT);
    }

    if (Gbl_FileType == ASL_INPUT_TYPE_ASCII_DATA)
    {
        return (AE_OK);
    }

    /* Create/Open a combined source output file */

    Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_SOURCE);
    if (!Filename)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
            0, 0, 0, 0, NULL, NULL);
        return (AE_ERROR);
    }

    /*
     * Open the source output file, binary mode (so that LF does not get
     * expanded to CR/LF on some systems, messing up our seek
     * calculations.)
     */
    FlOpenFile (ASL_FILE_SOURCE_OUTPUT, Filename, "w+b");

    /* Create/Open a assembly code source output file if asked */

    if (Gbl_AsmOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_ASM_SOURCE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the assembly code source file, text mode */

        FlOpenFile (ASL_FILE_ASM_SOURCE_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_ASM_SOURCE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_ASM_SOURCE_OUTPUT);
    }

    /* Create/Open a C code source output file if asked */

    if (Gbl_C_OutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_C_SOURCE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the C code source file, text mode */

        FlOpenFile (ASL_FILE_C_SOURCE_OUTPUT, Filename, "w+");

        FlPrintFile (ASL_FILE_C_SOURCE_OUTPUT, "/*\n");
        AslCompilerSignon (ASL_FILE_C_SOURCE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_C_SOURCE_OUTPUT);
    }

    /* Create/Open a assembly include output file if asked */

    if (Gbl_AsmIncludeOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_ASM_INCLUDE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the assembly include file, text mode */

        FlOpenFile (ASL_FILE_ASM_INCLUDE_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_ASM_INCLUDE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_ASM_INCLUDE_OUTPUT);
    }

    /* Create/Open a C include output file if asked */

    if (Gbl_C_IncludeOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_C_INCLUDE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the C include file, text mode */

        FlOpenFile (ASL_FILE_C_INCLUDE_OUTPUT, Filename, "w+");

        FlPrintFile (ASL_FILE_C_INCLUDE_OUTPUT, "/*\n");
        AslCompilerSignon (ASL_FILE_C_INCLUDE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_C_INCLUDE_OUTPUT);
    }

    /* Create a namespace output file if asked */

    if (Gbl_NsOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_NAMESPACE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the namespace file, text mode */

        FlOpenFile (ASL_FILE_NAMESPACE_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_NAMESPACE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_NAMESPACE_OUTPUT);
    }

    return (AE_OK);
}


#ifdef ACPI_OBSOLETE_FUNCTIONS
/*******************************************************************************
 *
 * FUNCTION:    FlParseInputPathname
 *
 * PARAMETERS:  InputFilename       - The user-specified ASL source file to be
 *                                    compiled
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Split the input path into a directory and filename part
 *              1) Directory part used to open include files
 *              2) Filename part used to generate output filenames
 *
 ******************************************************************************/

ACPI_STATUS
FlParseInputPathname (
    char                    *InputFilename)
{
    char                    *Substring;


    if (!InputFilename)
    {
        return (AE_OK);
    }

    /* Get the path to the input filename's directory */

    Gbl_DirectoryPath = strdup (InputFilename);
    if (!Gbl_DirectoryPath)
    {
        return (AE_NO_MEMORY);
    }

    Substring = strrchr (Gbl_DirectoryPath, '\\');
    if (!Substring)
    {
        Substring = strrchr (Gbl_DirectoryPath, '/');
        if (!Substring)
        {
            Substring = strrchr (Gbl_DirectoryPath, ':');
        }
    }

    if (!Substring)
    {
        Gbl_DirectoryPath[0] = 0;
        if (Gbl_UseDefaultAmlFilename)
        {
            Gbl_OutputFilenamePrefix = strdup (InputFilename);
        }
    }
    else
    {
        if (Gbl_UseDefaultAmlFilename)
        {
            Gbl_OutputFilenamePrefix = strdup (Substring + 1);
        }
        *(Substring+1) = 0;
    }

    return (AE_OK);
}
#endif


