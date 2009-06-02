
/******************************************************************************
 *
 * Module Name: asfile - Main module for the acpi source processor utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#include "acpisrc.h"

/* Local prototypes */

void
AsDoWildcard (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *SourcePath,
    char                    *TargetPath,
    int                     MaxPathLength,
    int                     FileType,
    char                    *WildcardSpec);

BOOLEAN
AsDetectLoneLineFeeds (
    char                    *Filename,
    char                    *Buffer);

static inline int
AsMaxInt (int a, int b)
{
    return (a > b ? a : b);
}


/******************************************************************************
 *
 * FUNCTION:    AsDoWildcard
 *
 * DESCRIPTION: Process files via wildcards
 *
 ******************************************************************************/

void
AsDoWildcard (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *SourcePath,
    char                    *TargetPath,
    int                     MaxPathLength,
    int                     FileType,
    char                    *WildcardSpec)
{
    void                    *DirInfo;
    char                    *Filename;
    char                    *SourceDirPath;
    char                    *TargetDirPath;
    char                    RequestedFileType;


    if (FileType == FILE_TYPE_DIRECTORY)
    {
        RequestedFileType = REQUEST_DIR_ONLY;
    }
    else
    {
        RequestedFileType = REQUEST_FILE_ONLY;
    }

    VERBOSE_PRINT (("Checking for %s source files in directory \"%s\"\n",
            WildcardSpec, SourcePath));

    /* Open the directory for wildcard search */

    DirInfo = AcpiOsOpenDirectory (SourcePath, WildcardSpec, RequestedFileType);
    if (DirInfo)
    {
        /*
         * Get all of the files that match both the
         * wildcard and the requested file type
         */
        while ((Filename = AcpiOsGetNextFilename (DirInfo)))
        {
            /* Looking for directory files, must check file type */

            switch (RequestedFileType)
            {
            case REQUEST_DIR_ONLY:

                /* If we actually have a dir, process the subtree */

                if (!AsCheckForDirectory (SourcePath, TargetPath, Filename,
                        &SourceDirPath, &TargetDirPath))
                {
                    VERBOSE_PRINT (("Subdirectory: %s\n", Filename));

                    AsProcessTree (ConversionTable, SourceDirPath, TargetDirPath);
                    free (SourceDirPath);
                    free (TargetDirPath);
                }
                break;

            case REQUEST_FILE_ONLY:

                /* Otherwise, this is a file, not a directory */

                VERBOSE_PRINT (("File: %s\n", Filename));

                AsProcessOneFile (ConversionTable, SourcePath, TargetPath,
                        MaxPathLength, Filename, FileType);
                break;

            default:
                break;
            }
        }

        /* Cleanup */

        AcpiOsCloseDirectory (DirInfo);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsProcessTree
 *
 * DESCRIPTION: Process the directory tree.  Files with the extension ".C" and
 *              ".H" are processed as the tree is traversed.
 *
 ******************************************************************************/

ACPI_NATIVE_INT
AsProcessTree (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *SourcePath,
    char                    *TargetPath)
{
    int                     MaxPathLength;


    MaxPathLength = AsMaxInt (strlen (SourcePath), strlen (TargetPath));

    if (!(ConversionTable->Flags & FLG_NO_FILE_OUTPUT))
    {
        if (ConversionTable->Flags & FLG_LOWERCASE_DIRNAMES)
        {
            strlwr (TargetPath);
        }

        VERBOSE_PRINT (("Creating Directory \"%s\"\n", TargetPath));
        if (mkdir (TargetPath))
        {
            if (errno != EEXIST)
            {
                printf ("Could not create target directory\n");
                return -1;
            }
        }
    }

    /* Do the C source files */

    AsDoWildcard (ConversionTable, SourcePath, TargetPath, MaxPathLength,
            FILE_TYPE_SOURCE, "*.c");

    /* Do the C header files */

    AsDoWildcard (ConversionTable, SourcePath, TargetPath, MaxPathLength,
            FILE_TYPE_HEADER, "*.h");

    /* Do the Lex file(s) */

    AsDoWildcard (ConversionTable, SourcePath, TargetPath, MaxPathLength,
            FILE_TYPE_SOURCE, "*.l");

    /* Do the yacc file(s) */

    AsDoWildcard (ConversionTable, SourcePath, TargetPath, MaxPathLength,
            FILE_TYPE_SOURCE, "*.y");

    /* Do any ASL files */

    AsDoWildcard (ConversionTable, SourcePath, TargetPath, MaxPathLength,
            FILE_TYPE_HEADER, "*.asl");

    /* Do any subdirectories */

    AsDoWildcard (ConversionTable, SourcePath, TargetPath, MaxPathLength,
            FILE_TYPE_DIRECTORY, "*");

    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    AsDetectLoneLineFeeds
 *
 * DESCRIPTION: Find LF without CR.
 *
 ******************************************************************************/

BOOLEAN
AsDetectLoneLineFeeds (
    char                    *Filename,
    char                    *Buffer)
{
    UINT32                  i = 1;
    UINT32                  LfCount = 0;
    UINT32                  LineCount = 0;


    if (!Buffer[0])
    {
        return FALSE;
    }

    while (Buffer[i])
    {
        if (Buffer[i] == 0x0A)
        {
            if (Buffer[i-1] != 0x0D)
            {
                LfCount++;
            }
            LineCount++;
        }
        i++;
    }

    if (LfCount)
    {
        if (LineCount == LfCount)
        {
            if (!Gbl_IgnoreLoneLineFeeds)
            {
                printf ("%s: ****File has UNIX format**** (LF only, not CR/LF) %d lines\n",
                    Filename, LfCount);
            }
        }
        else
        {
            printf ("%s: %d lone linefeeds in file\n", Filename, LfCount);
        }
        return TRUE;
    }

    return (FALSE);
}


/******************************************************************************
 *
 * FUNCTION:    AsConvertFile
 *
 * DESCRIPTION: Perform the requested transforms on the file buffer (as
 *              determined by the ConversionTable and the FileType).
 *
 ******************************************************************************/

void
AsConvertFile (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *FileBuffer,
    char                    *Filename,
    ACPI_NATIVE_INT         FileType)
{
    UINT32                  i;
    UINT32                  Functions;
    ACPI_STRING_TABLE       *StringTable;
    ACPI_IDENTIFIER_TABLE   *ConditionalTable;
    ACPI_IDENTIFIER_TABLE   *LineTable;
    ACPI_IDENTIFIER_TABLE   *MacroTable;
    ACPI_TYPED_IDENTIFIER_TABLE *StructTable;


    switch (FileType)
    {
    case FILE_TYPE_SOURCE:
        Functions           = ConversionTable->SourceFunctions;
        StringTable         = ConversionTable->SourceStringTable;
        LineTable           = ConversionTable->SourceLineTable;
        ConditionalTable    = ConversionTable->SourceConditionalTable;
        MacroTable          = ConversionTable->SourceMacroTable;
        StructTable         = ConversionTable->SourceStructTable;
       break;

    case FILE_TYPE_HEADER:
        Functions           = ConversionTable->HeaderFunctions;
        StringTable         = ConversionTable->HeaderStringTable;
        LineTable           = ConversionTable->HeaderLineTable;
        ConditionalTable    = ConversionTable->HeaderConditionalTable;
        MacroTable          = ConversionTable->HeaderMacroTable;
        StructTable         = ConversionTable->HeaderStructTable;
        break;

    default:
        printf ("Unknown file type, cannot process\n");
        return;
    }


    Gbl_StructDefs = strstr (FileBuffer, "/* acpisrc:StructDefs");
    Gbl_Files++;
    VERBOSE_PRINT (("Processing %u bytes\n", strlen (FileBuffer)));

    if (ConversionTable->LowerCaseTable)
    {
        for (i = 0; ConversionTable->LowerCaseTable[i].Identifier; i++)
        {
            AsLowerCaseString (ConversionTable->LowerCaseTable[i].Identifier,
                                FileBuffer);
        }
    }

    /* Process all the string replacements */

    if (StringTable)
    {
        for (i = 0; StringTable[i].Target; i++)
        {
            AsReplaceString (StringTable[i].Target, StringTable[i].Replacement,
                    StringTable[i].Type, FileBuffer);
        }
    }

    if (LineTable)
    {
        for (i = 0; LineTable[i].Identifier; i++)
        {
            AsRemoveLine (FileBuffer, LineTable[i].Identifier);
        }
    }

    if (ConditionalTable)
    {
        for (i = 0; ConditionalTable[i].Identifier; i++)
        {
            AsRemoveConditionalCompile (FileBuffer, ConditionalTable[i].Identifier);
        }
    }

    if (MacroTable)
    {
        for (i = 0; MacroTable[i].Identifier; i++)
        {
            AsRemoveMacro (FileBuffer, MacroTable[i].Identifier);
        }
    }

    if (StructTable)
    {
        for (i = 0; StructTable[i].Identifier; i++)
        {
            AsInsertPrefix (FileBuffer, StructTable[i].Identifier, StructTable[i].Type);
        }
    }

    /* Process the function table */

    for (i = 0; i < 32; i++)
    {
        /* Decode the function bitmap */

        switch ((1 << i) & Functions)
        {
        case 0:
            /* This function not configured */
            break;


        case CVT_COUNT_TABS:

            AsCountTabs (FileBuffer, Filename);
            break;


        case CVT_COUNT_NON_ANSI_COMMENTS:

            AsCountNonAnsiComments (FileBuffer, Filename);
            break;


        case CVT_CHECK_BRACES:

            AsCheckForBraces (FileBuffer, Filename);
            break;


        case CVT_TRIM_LINES:

            AsTrimLines (FileBuffer, Filename);
            break;


        case CVT_COUNT_LINES:

            AsCountSourceLines (FileBuffer, Filename);
            break;


        case CVT_BRACES_ON_SAME_LINE:

            AsBracesOnSameLine (FileBuffer);
            break;


        case CVT_MIXED_CASE_TO_UNDERSCORES:

            AsMixedCaseToUnderscores (FileBuffer);
            break;


        case CVT_LOWER_CASE_IDENTIFIERS:

            AsLowerCaseIdentifiers (FileBuffer);
            break;


        case CVT_REMOVE_DEBUG_MACROS:

            AsRemoveDebugMacros (FileBuffer);
            break;


        case CVT_TRIM_WHITESPACE:

            AsTrimWhitespace (FileBuffer);
            break;


        case CVT_REMOVE_EMPTY_BLOCKS:

            AsRemoveEmptyBlocks (FileBuffer, Filename);
            break;


        case CVT_REDUCE_TYPEDEFS:

            AsReduceTypedefs (FileBuffer, "typedef union");
            AsReduceTypedefs (FileBuffer, "typedef struct");
            break;


        case CVT_SPACES_TO_TABS4:

            AsTabify4 (FileBuffer);
            break;


        case CVT_SPACES_TO_TABS8:

            AsTabify8 (FileBuffer);
            break;

        case CVT_COUNT_SHORTMULTILINE_COMMENTS:

#ifdef ACPI_FUTURE_IMPLEMENTATION
            AsTrimComments (FileBuffer, Filename);
#endif
            break;

        default:

            printf ("Unknown conversion subfunction opcode\n");
            break;
        }
    }

    if (ConversionTable->NewHeader)
    {
        AsReplaceHeader (FileBuffer, ConversionTable->NewHeader);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsProcessOneFile
 *
 * DESCRIPTION: Process one source file.  The file is opened, read entirely
 *              into a buffer, converted, then written to a new file.
 *
 ******************************************************************************/

ACPI_NATIVE_INT
AsProcessOneFile (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *SourcePath,
    char                    *TargetPath,
    int                     MaxPathLength,
    char                    *Filename,
    ACPI_NATIVE_INT         FileType)
{
    char                    *Pathname;
    char                    *OutPathname = NULL;


    /* Allocate a file pathname buffer for both source and target */

    Pathname = calloc (MaxPathLength + strlen (Filename) + 2, 1);
    if (!Pathname)
    {
        printf ("Could not allocate buffer for file pathnames\n");
        return -1;
    }

    Gbl_FileType = FileType;

    /* Generate the source pathname and read the file */

    if (SourcePath)
    {
        strcpy (Pathname, SourcePath);
        strcat (Pathname, "/");
    }

    strcat (Pathname, Filename);

    if (AsGetFile (Pathname, &Gbl_FileBuffer, &Gbl_FileSize))
    {
        return -1;
    }

    Gbl_HeaderSize = 0;
    if (strstr (Filename, ".asl"))
    {
        Gbl_HeaderSize = LINES_IN_ASL_HEADER; /* Lines in default ASL header */
    }
    else if (strstr (Gbl_FileBuffer, LEGAL_HEADER_SIGNATURE))
    {
        Gbl_HeaderSize = LINES_IN_LEGAL_HEADER; /* Normal C file and H header */
    }
    else if (strstr (Gbl_FileBuffer, LINUX_HEADER_SIGNATURE))
    {
        Gbl_HeaderSize = LINES_IN_LINUX_HEADER; /* Linuxized C file and H header */
    }

    /* Process the file in the buffer */

    Gbl_MadeChanges = FALSE;
    if (!Gbl_IgnoreLoneLineFeeds && Gbl_HasLoneLineFeeds)
    {
        /*
         * All lone LFs will be converted to CR/LF
         * (when file is written, Windows version only)
         */
        printf ("Converting lone linefeeds\n");
        Gbl_MadeChanges = TRUE;
    }

    AsConvertFile (ConversionTable, Gbl_FileBuffer, Pathname, FileType);

    if (!(ConversionTable->Flags & FLG_NO_FILE_OUTPUT))
    {
        if (!(Gbl_Overwrite && !Gbl_MadeChanges))
        {
            /* Generate the target pathname and write the file */

            OutPathname = calloc (MaxPathLength + strlen (Filename) + 2 + strlen (TargetPath), 1);
            if (!OutPathname)
            {
                printf ("Could not allocate buffer for file pathnames\n");
                return -1;
            }

            strcpy (OutPathname, TargetPath);
            if (SourcePath)
            {
                strcat (OutPathname, "/");
                strcat (OutPathname, Filename);
            }

            AsPutFile (OutPathname, Gbl_FileBuffer, ConversionTable->Flags);
        }
    }

    free (Gbl_FileBuffer);
    free (Pathname);
    if (OutPathname)
    {
        free (OutPathname);
    }

    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    AsCheckForDirectory
 *
 * DESCRIPTION: Check if the current file is a valid directory.  If not,
 *              construct the full pathname for the source and target paths.
 *              Checks for the dot and dot-dot files (they are ignored)
 *
 ******************************************************************************/

ACPI_NATIVE_INT
AsCheckForDirectory (
    char                    *SourceDirPath,
    char                    *TargetDirPath,
    char                    *Filename,
    char                    **SourcePath,
    char                    **TargetPath)
{
    char                    *SrcPath;
    char                    *TgtPath;


    if (!(strcmp (Filename, ".")) ||
        !(strcmp (Filename, "..")))
    {
        return -1;
    }

    SrcPath = calloc (strlen (SourceDirPath) + strlen (Filename) + 2, 1);
    if (!SrcPath)
    {
        printf ("Could not allocate buffer for directory source pathname\n");
        return -1;
    }

    TgtPath = calloc (strlen (TargetDirPath) + strlen (Filename) + 2, 1);
    if (!TgtPath)
    {
        printf ("Could not allocate buffer for directory target pathname\n");
        free (SrcPath);
        return -1;
    }

    strcpy (SrcPath, SourceDirPath);
    strcat (SrcPath, "/");
    strcat (SrcPath, Filename);

    strcpy (TgtPath, TargetDirPath);
    strcat (TgtPath, "/");
    strcat (TgtPath, Filename);

    *SourcePath = SrcPath;
    *TargetPath = TgtPath;
    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    AsGetFile
 *
 * DESCRIPTION: Open a file and read it entirely into a an allocated buffer
 *
 ******************************************************************************/

int
AsGetFile (
    char                    *Filename,
    char                    **FileBuffer,
    UINT32                  *FileSize)
{

    int                     FileHandle;
    UINT32                  Size;
    char                    *Buffer;


    /* Binary mode leaves CR/LF pairs */

    FileHandle = open (Filename, O_BINARY | O_RDONLY);
    if (!FileHandle)
    {
        printf ("Could not open %s\n", Filename);
        return -1;
    }

    if (fstat (FileHandle, &Gbl_StatBuf))
    {
        printf ("Could not get file status for %s\n", Filename);
        goto ErrorExit;
    }

    /*
     * Create a buffer for the entire file
     * Add plenty extra buffer to accomodate string replacements
     */
    Size = Gbl_StatBuf.st_size;
    Gbl_TotalSize += Size;

    Buffer = calloc (Size * 2, 1);
    if (!Buffer)
    {
        printf ("Could not allocate buffer of size %d\n", Size * 2);
        goto ErrorExit;
    }

    /* Read the entire file */

    Size = read (FileHandle, Buffer, Size);
    if (Size == -1)
    {
        printf ("Could not read the input file %s\n", Filename);
        goto ErrorExit;
    }

    Buffer [Size] = 0;         /* Null terminate the buffer */
    close (FileHandle);

    /* Check for unix contamination */

    Gbl_HasLoneLineFeeds = AsDetectLoneLineFeeds (Filename, Buffer);

    /*
     * Convert all CR/LF pairs to LF only.  We do this locally so that
     * this code is portable across operating systems.
     */
    AsConvertToLineFeeds (Buffer);

    *FileBuffer = Buffer;
    *FileSize = Size;

    return 0;


ErrorExit:

    close (FileHandle);
    return -1;
}


/******************************************************************************
 *
 * FUNCTION:    AsPutFile
 *
 * DESCRIPTION: Create a new output file and write the entire contents of the
 *              buffer to the new file.  Buffer must be a zero terminated string
 *
 ******************************************************************************/

int
AsPutFile (
    char                    *Pathname,
    char                    *FileBuffer,
    UINT32                  SystemFlags)
{
    UINT32                  FileSize;
    int                     DestHandle;
    int                     OpenFlags;


    /* Create the target file */

    OpenFlags = O_TRUNC | O_CREAT | O_WRONLY | O_BINARY;

    if (!(SystemFlags & FLG_NO_CARRIAGE_RETURNS))
    {
        /* Put back the CR before each LF */

        AsInsertCarriageReturns (FileBuffer);
    }

    DestHandle = open (Pathname, OpenFlags, S_IREAD | S_IWRITE);
    if (DestHandle == -1)
    {
        perror ("Could not create destination file");
        printf ("Could not create destination file \"%s\"\n", Pathname);
        return -1;
    }

    /* Write the buffer to the file */

    FileSize = strlen (FileBuffer);
    write (DestHandle, FileBuffer, FileSize);

    close (DestHandle);

    return 0;
}


