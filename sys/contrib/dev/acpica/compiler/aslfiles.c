/******************************************************************************
 *
 * Module Name: aslfiles - File support functions
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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
#include <contrib/dev/acpica/compiler/dtcompiler.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslfiles")

/* Local prototypes */

static FILE *
FlOpenIncludeWithPrefix (
    char                    *PrefixDir,
    ACPI_PARSE_OBJECT       *Op,
    char                    *Filename);

#ifdef ACPI_OBSOLETE_FUNCTIONS
ACPI_STATUS
FlParseInputPathname (
    char                    *InputFilename);
#endif


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
    UINT32                  LineNumber)
{

    DbgPrint (ASL_PARSE_OUTPUT, "\n#line: New line number %u (old %u)\n",
         LineNumber, Gbl_LogicalLineNumber);

    Gbl_CurrentLineNumber = LineNumber;
}


/*******************************************************************************
 *
 * FUNCTION:    FlSetFilename
 *
 * PARAMETERS:  Op        - Parse node for the LINE asl statement
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Set the current filename
 *
 ******************************************************************************/

void
FlSetFilename (
    char                    *Filename)
{

    DbgPrint (ASL_PARSE_OUTPUT, "\n#line: New filename %s (old %s)\n",
         Filename, Gbl_Files[ASL_FILE_INPUT].Filename);

    /* No need to free any existing filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = Filename;
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
 * FUNCTION:    FlMergePathnames
 *
 * PARAMETERS:  PrefixDir       - Prefix directory pathname. Can be NULL or
 *                                a zero length string.
 *              FilePathname    - The include filename from the source ASL.
 *
 * RETURN:      Merged pathname string
 *
 * DESCRIPTION: Merge two pathnames that (probably) have common elements, to
 *              arrive at a minimal length string. Merge can occur if the
 *              FilePathname is relative to the PrefixDir.
 *
 ******************************************************************************/

char *
FlMergePathnames (
    char                    *PrefixDir,
    char                    *FilePathname)
{
    char                    *CommonPath;
    char                    *Pathname;
    char                    *LastElement;


    DbgPrint (ASL_PARSE_OUTPUT, "Include: Prefix path - \"%s\"\n"
        "Include: FilePathname - \"%s\"\n",
         PrefixDir, FilePathname);

    /*
     * If there is no prefix directory or if the file pathname is absolute,
     * just return the original file pathname
     */
    if (!PrefixDir || (!*PrefixDir) ||
        (*FilePathname == '/') ||
         (FilePathname[1] == ':'))
    {
        Pathname = UtStringCacheCalloc (strlen (FilePathname) + 1);
        strcpy (Pathname, FilePathname);
        goto ConvertBackslashes;
    }

    /* Need a local copy of the prefix directory path */

    CommonPath = UtStringCacheCalloc (strlen (PrefixDir) + 1);
    strcpy (CommonPath, PrefixDir);

    /*
     * Walk forward through the file path, and simultaneously backward
     * through the prefix directory path until there are no more
     * relative references at the start of the file path.
     */
    while (*FilePathname && (!strncmp (FilePathname, "../", 3)))
    {
        /* Remove last element of the prefix directory path */

        LastElement = strrchr (CommonPath, '/');
        if (!LastElement)
        {
            goto ConcatenatePaths;
        }

        *LastElement = 0;   /* Terminate CommonPath string */
        FilePathname += 3;  /* Point to next path element */
    }

    /*
     * Remove the last element of the prefix directory path (it is the same as
     * the first element of the file pathname), and build the final merged
     * pathname.
     */
    LastElement = strrchr (CommonPath, '/');
    if (LastElement)
    {
        *LastElement = 0;
    }

    /* Build the final merged pathname */

ConcatenatePaths:
    Pathname = UtStringCacheCalloc (
        strlen (CommonPath) + strlen (FilePathname) + 2);
    if (LastElement && *CommonPath)
    {
        strcpy (Pathname, CommonPath);
        strcat (Pathname, "/");
    }
    strcat (Pathname, FilePathname);

    /* Convert all backslashes to normal slashes */

ConvertBackslashes:
    UtConvertBackslashes (Pathname);

    DbgPrint (ASL_PARSE_OUTPUT, "Include: Merged Pathname - \"%s\"\n",
         Pathname);
    return (Pathname);
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
    ACPI_PARSE_OBJECT       *Op,
    char                    *Filename)
{
    FILE                    *IncludeFile;
    char                    *Pathname;
    UINT32                  OriginalLineNumber;


    /* Build the full pathname to the file */

    Pathname = FlMergePathnames (PrefixDir, Filename);

    DbgPrint (ASL_PARSE_OUTPUT, "Include: Opening file - \"%s\"\n\n",
        Pathname);

    /* Attempt to open the file, push if successful */

    IncludeFile = fopen (Pathname, "r");
    if (!IncludeFile)
    {
        fprintf (stderr, "Could not open include file %s\n", Pathname);
        ACPI_FREE (Pathname);
        return (NULL);
    }

    /*
     * Check the entire include file for any # preprocessor directives.
     * This is because there may be some confusion between the #include
     * preprocessor directive and the ASL Include statement. A file included
     * by the ASL include cannot contain preprocessor directives because
     * the preprocessor has already run by the time the ASL include is
     * recognized (by the compiler, not the preprocessor.)
     *
     * Note: DtGetNextLine strips/ignores comments.
     * Save current line number since DtGetNextLine modifies it.
     */
    Gbl_CurrentLineNumber--;
    OriginalLineNumber = Gbl_CurrentLineNumber;

    while (DtGetNextLine (IncludeFile, DT_ALLOW_MULTILINE_QUOTES) != ASL_EOF)
    {
        if (Gbl_CurrentLineBuffer[0] == '#')
        {
            AslError (ASL_ERROR, ASL_MSG_INCLUDE_FILE,
                Op, "use #include instead");
        }
    }

    Gbl_CurrentLineNumber = OriginalLineNumber;

    /* Must seek back to the start of the file */

    fseek (IncludeFile, 0, SEEK_SET);

    /* Push the include file on the open input file stack */

    AslPushInputFileStack (IncludeFile, Pathname);
    return (IncludeFile);
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
    AslResetCurrentLineBuffer ();
    FlPrintFile (ASL_FILE_SOURCE_OUTPUT, "\n");
    Gbl_CurrentLineOffset++;


    /* Attempt to open the include file */

    /* If the file specifies an absolute path, just open it */

    if ((Op->Asl.Value.String[0] == '/')  ||
        (Op->Asl.Value.String[0] == '\\') ||
        (Op->Asl.Value.String[1] == ':'))
    {
        IncludeFile = FlOpenIncludeWithPrefix ("", Op, Op->Asl.Value.String);
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
    IncludeFile = FlOpenIncludeWithPrefix (
        Gbl_DirectoryPath, Op, Op->Asl.Value.String);
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
        IncludeFile = FlOpenIncludeWithPrefix (
            NextDir->Dir, Op, Op->Asl.Value.String);
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

    FlOpenFile (ASL_FILE_INPUT, InputFilename, "rt");
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
 * DESCRIPTION: Create the output filename (*.AML) and open the file. The file
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

        Gbl_Files[ASL_FILE_AML_OUTPUT].Filename = Filename;
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


     /* Create/Open a map file if requested */

    if (Gbl_MapfileFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_MAP);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the hex file, text mode (closed at compiler exit) */

        FlOpenFile (ASL_FILE_MAP_OUTPUT, Filename, "w+t");

        AslCompilerSignon (ASL_FILE_MAP_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_MAP_OUTPUT);
    }

    /* All done for disassembler */

    if (Gbl_FileType == ASL_INPUT_TYPE_BINARY_ACPI_TABLE)
    {
        return (AE_OK);
    }

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

        FlOpenFile (ASL_FILE_HEX_OUTPUT, Filename, "w+t");

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

        Gbl_Files[ASL_FILE_DEBUG_OUTPUT].Filename = Filename;
        Gbl_Files[ASL_FILE_DEBUG_OUTPUT].Handle =
            freopen (Filename, "w+t", stderr);

        if (!Gbl_Files[ASL_FILE_DEBUG_OUTPUT].Handle)
        {
            /*
             * A problem with freopen is that on error, we no longer
             * have stderr and cannot emit normal error messages.
             * Emit error to stdout, close files, and exit.
             */
            fprintf (stdout,
                "\nCould not open debug output file: %s\n\n", Filename);

            CmCleanupAndExit ();
            exit (1);
        }

        AslCompilerSignon (ASL_FILE_DEBUG_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_DEBUG_OUTPUT);
    }

    /* Create/Open a cross-reference output file if asked */

    if (Gbl_CrossReferenceOutput)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_XREF);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_DEBUG_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        FlOpenFile (ASL_FILE_XREF_OUTPUT, Filename, "w+t");

        AslCompilerSignon (ASL_FILE_XREF_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_XREF_OUTPUT);
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

        FlOpenFile (ASL_FILE_LISTING_OUTPUT, Filename, "w+t");

        AslCompilerSignon (ASL_FILE_LISTING_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_LISTING_OUTPUT);
    }

    /* Create the preprocessor output temp file if preprocessor enabled */

    if (Gbl_PreprocessFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_PREPROCESSOR);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_PREPROCESSOR_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        FlOpenFile (ASL_FILE_PREPROCESSOR, Filename, "w+t");
    }

    /*
     * Create the "user" preprocessor output file if -li flag set.
     * Note, this file contains no embedded #line directives.
     */
    if (Gbl_PreprocessorOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_PREPROC_USER);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_PREPROCESSOR_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        FlOpenFile (ASL_FILE_PREPROCESSOR_USER, Filename, "w+t");
    }

    /* All done for data table compiler */

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

/*
// TBD: TEMP
//    AslCompilerin = Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Handle;
*/
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

        FlOpenFile (ASL_FILE_ASM_SOURCE_OUTPUT, Filename, "w+t");

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

        FlOpenFile (ASL_FILE_C_SOURCE_OUTPUT, Filename, "w+t");

        FlPrintFile (ASL_FILE_C_SOURCE_OUTPUT, "/*\n");
        AslCompilerSignon (ASL_FILE_C_SOURCE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_C_SOURCE_OUTPUT);
    }

    /* Create/Open a C code source output file for the offset table if asked */

    if (Gbl_C_OffsetTableFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_C_OFFSET);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the C code source file, text mode */

        FlOpenFile (ASL_FILE_C_OFFSET_OUTPUT, Filename, "w+t");

        FlPrintFile (ASL_FILE_C_OFFSET_OUTPUT, "/*\n");
        AslCompilerSignon (ASL_FILE_C_OFFSET_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_C_OFFSET_OUTPUT);
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

        FlOpenFile (ASL_FILE_ASM_INCLUDE_OUTPUT, Filename, "w+t");

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

        FlOpenFile (ASL_FILE_C_INCLUDE_OUTPUT, Filename, "w+t");

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

        FlOpenFile (ASL_FILE_NAMESPACE_OUTPUT, Filename, "w+t");

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

    UtConvertBackslashes (Gbl_OutputFilenamePrefix);
    return (AE_OK);
}
#endif
