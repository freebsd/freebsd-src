/******************************************************************************
 *
 * Module Name: asmain - Main module for the acpi source processor utility
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

#include "acpisrc.h"
#include "acapps.h"

/* Local prototypes */

int
AsExaminePaths (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *Source,
    char                    *Target,
    UINT32                  *SourceFileType);

void
AsDisplayStats (
    void);

void
AsDisplayUsage (
    void);

/* Globals */

UINT32                  Gbl_Tabs = 0;
UINT32                  Gbl_MissingBraces = 0;
UINT32                  Gbl_NonAnsiComments = 0;
UINT32                  Gbl_Files = 0;
UINT32                  Gbl_WhiteLines = 0;
UINT32                  Gbl_CommentLines = 0;
UINT32                  Gbl_SourceLines = 0;
UINT32                  Gbl_LongLines = 0;
UINT32                  Gbl_TotalLines = 0;
UINT32                  Gbl_TotalSize = 0;
UINT32                  Gbl_HeaderLines = 0;
UINT32                  Gbl_HeaderSize = 0;
void                    *Gbl_StructDefs = NULL;

struct stat             Gbl_StatBuf;
char                    *Gbl_FileBuffer;
UINT32                  Gbl_FileSize;
UINT32                  Gbl_FileType;
BOOLEAN                 Gbl_VerboseMode = FALSE;
BOOLEAN                 Gbl_QuietMode = FALSE;
BOOLEAN                 Gbl_BatchMode = FALSE;
BOOLEAN                 Gbl_DebugStatementsMode = FALSE;
BOOLEAN                 Gbl_MadeChanges = FALSE;
BOOLEAN                 Gbl_Overwrite = FALSE;
BOOLEAN                 Gbl_WidenDeclarations = FALSE;
BOOLEAN                 Gbl_IgnoreLoneLineFeeds = FALSE;
BOOLEAN                 Gbl_HasLoneLineFeeds = FALSE;
BOOLEAN                 Gbl_Cleanup = FALSE;
BOOLEAN                 Gbl_IgnoreTranslationEscapes = FALSE;

#define AS_UTILITY_NAME             "ACPI Source Code Conversion Utility"
#define AS_SUPPORTED_OPTIONS        "cdhilqsuv^y"


/******************************************************************************
 *
 * FUNCTION:    AsExaminePaths
 *
 * DESCRIPTION: Source and Target pathname verification and handling
 *
 ******************************************************************************/

int
AsExaminePaths (
    ACPI_CONVERSION_TABLE   *ConversionTable,
    char                    *Source,
    char                    *Target,
    UINT32                  *SourceFileType)
{
    int                     Status;
    int                     Response;


    Status = stat (Source, &Gbl_StatBuf);
    if (Status)
    {
        printf ("Source path \"%s\" does not exist\n", Source);
        return (-1);
    }

    /* Return the filetype -- file or a directory */

    *SourceFileType = 0;
    if (Gbl_StatBuf.st_mode & S_IFDIR)
    {
        *SourceFileType = S_IFDIR;
    }

    /*
     * If we are in no-output mode or in batch mode, we are done
     */
    if ((ConversionTable->Flags & FLG_NO_FILE_OUTPUT) ||
        (Gbl_BatchMode))
    {
        return (0);
    }

    if (!AcpiUtStricmp (Source, Target))
    {
        printf ("Target path is the same as the source path, overwrite?\n");
        Response = getchar ();

        /* Check response */

        if (Response != 'y')
        {
            return (-1);
        }

        Gbl_Overwrite = TRUE;
    }
    else
    {
        Status = stat (Target, &Gbl_StatBuf);
        if (!Status)
        {
            printf ("Target path already exists, overwrite?\n");
            Response = getchar ();

            /* Check response */

            if (Response != 'y')
            {
                return (-1);
            }
        }
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AsDisplayStats
 *
 * DESCRIPTION: Display global statistics gathered during translation
 *
 ******************************************************************************/

void
AsDisplayStats (
    void)
{

    if (Gbl_QuietMode)
    {
        return;
    }

    printf ("\nAcpiSrc statistics:\n\n");
    printf ("%8u Files processed\n", Gbl_Files);

    if (!Gbl_Files)
    {
        return;
    }

    printf ("%8u Total bytes (%.1fK/file)\n",
        Gbl_TotalSize, ((double) Gbl_TotalSize/Gbl_Files)/1024);
    printf ("%8u Tabs found\n", Gbl_Tabs);
    printf ("%8u Missing if/else/while braces\n", Gbl_MissingBraces);
    printf ("%8u Non-ANSI // comments found\n", Gbl_NonAnsiComments);
    printf ("%8u Total Lines\n", Gbl_TotalLines);
    printf ("%8u Lines of code\n", Gbl_SourceLines);
    printf ("%8u Lines of non-comment whitespace\n", Gbl_WhiteLines);
    printf ("%8u Lines of comments\n", Gbl_CommentLines);
    printf ("%8u Long lines found\n", Gbl_LongLines);

    if (Gbl_WhiteLines > 0)
    {
        printf ("%8.1f Ratio of code to whitespace\n",
            ((float) Gbl_SourceLines / (float) Gbl_WhiteLines));
    }

    if ((Gbl_CommentLines + Gbl_NonAnsiComments) > 0)
    {
        printf ("%8.1f Ratio of code to comments\n",
            ((float) Gbl_SourceLines / (float) (Gbl_CommentLines + Gbl_NonAnsiComments)));
    }

    if (!Gbl_TotalLines)
    {
        return;
    }

    printf ("         %u%% code, %u%% comments, %u%% whitespace, %u%% headers\n",
        (Gbl_SourceLines * 100) / Gbl_TotalLines,
        (Gbl_CommentLines * 100) / Gbl_TotalLines,
        (Gbl_WhiteLines * 100) / Gbl_TotalLines,
        (Gbl_HeaderLines * 100) / Gbl_TotalLines);
    return;
}


/******************************************************************************
 *
 * FUNCTION:    AsDisplayUsage
 *
 * DESCRIPTION: Usage message
 *
 ******************************************************************************/

void
AsDisplayUsage (
    void)
{

    ACPI_USAGE_HEADER ("acpisrc [-c|l|u] [-dsvy] <SourceDir> <DestinationDir>");

    ACPI_OPTION ("-c",          "Generate cleaned version of the source");
    ACPI_OPTION ("-h",          "Insert dual-license header into all modules");
    ACPI_OPTION ("-i",          "Cleanup macro indentation");
    ACPI_OPTION ("-l",          "Generate Linux version of the source");
    ACPI_OPTION ("-u",          "Generate Custom source translation");

    ACPI_USAGE_TEXT ("\n");
    ACPI_OPTION ("-d",          "Leave debug statements in code");
    ACPI_OPTION ("-s",          "Generate source statistics only");
    ACPI_OPTION ("-v",          "Display version information");
    ACPI_OPTION ("-vb",         "Verbose mode");
    ACPI_OPTION ("-y",          "Suppress file overwrite prompts");
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * DESCRIPTION: C main function
 *
 ******************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    *argv[])
{
    int                     j;
    ACPI_CONVERSION_TABLE   *ConversionTable = NULL;
    char                    *SourcePath;
    char                    *TargetPath;
    UINT32                  FileType;


    ACPI_DEBUG_INITIALIZE (); /* For debug version only */
    AcpiOsInitialize ();
    printf (ACPI_COMMON_SIGNON (AS_UTILITY_NAME));

    if (argc < 2)
    {
        AsDisplayUsage ();
        return (0);
    }

    /* Command line options */

    while ((j = AcpiGetopt (argc, argv, AS_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch(j)
    {
    case 'l':

        /* Linux code generation */

        printf ("Creating Linux source code\n");
        ConversionTable = &LinuxConversionTable;
        Gbl_WidenDeclarations = TRUE;
        Gbl_IgnoreLoneLineFeeds = TRUE;
        break;

    case 'c':

        /* Cleanup code */

        printf ("Code cleanup\n");
        ConversionTable = &CleanupConversionTable;
        Gbl_Cleanup = TRUE;
        break;

    case 'h':

        /* Inject Dual-license header */

        printf ("Inserting Dual-license header to all modules\n");
        ConversionTable = &LicenseConversionTable;
        break;

    case 'i':

        /* Cleanup wrong indent result */

        printf ("Cleaning up macro indentation\n");
        ConversionTable = &IndentConversionTable;
        Gbl_IgnoreLoneLineFeeds = TRUE;
        Gbl_IgnoreTranslationEscapes = TRUE;
        break;

    case 's':

        /* Statistics only */

        break;

    case 'u':

        /* custom conversion  */

        printf ("Custom source translation\n");
        ConversionTable = &CustomConversionTable;
        break;

    case 'v':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':  /* -v: (Version): signon already emitted, just exit */

            exit (0);

        case 'b':

            /* Verbose mode */

            Gbl_VerboseMode = TRUE;
            break;

        default:

            printf ("Unknown option: -v%s\n", AcpiGbl_Optarg);
            return (-1);
        }

        break;

    case 'y':

        /* Batch mode */

        Gbl_BatchMode = TRUE;
        break;

    case 'd':

        /* Leave debug statements in */

        Gbl_DebugStatementsMode = TRUE;
        break;

    case 'q':

        /* Quiet mode */

        Gbl_QuietMode = TRUE;
        break;

    default:

        AsDisplayUsage ();
        return (-1);
    }


    SourcePath = argv[AcpiGbl_Optind];
    if (!SourcePath)
    {
        printf ("Missing source path\n");
        AsDisplayUsage ();
        return (-1);
    }

    TargetPath = argv[AcpiGbl_Optind+1];

    if (!ConversionTable)
    {
        /* Just generate statistics. Ignore target path */

        TargetPath = SourcePath;

        printf ("Source code statistics only\n");
        ConversionTable = &StatsConversionTable;
    }
    else if (!TargetPath)
    {
        TargetPath = SourcePath;
    }

    if (Gbl_DebugStatementsMode)
    {
        ConversionTable->SourceFunctions &= ~CVT_REMOVE_DEBUG_MACROS;
    }

    /* Check source and target paths and files */

    if (AsExaminePaths (ConversionTable, SourcePath, TargetPath, &FileType))
    {
        return (-1);
    }

    /* Source/target can be either directories or a files */

    if (FileType == S_IFDIR)
    {
        /* Process the directory tree */

        AsProcessTree (ConversionTable, SourcePath, TargetPath);
    }
    else
    {
        /* Process a single file */

        /* Differentiate between source and header files */

        if (strstr (SourcePath, ".h"))
        {
            AsProcessOneFile (ConversionTable, NULL, TargetPath, 0, SourcePath, FILE_TYPE_HEADER);
        }
        else if (strstr (SourcePath, ".c"))
        {
            AsProcessOneFile (ConversionTable, NULL, TargetPath, 0, SourcePath, FILE_TYPE_SOURCE);
        }
        else if (strstr (SourcePath, ".patch"))
        {
            AsProcessOneFile (ConversionTable, NULL, TargetPath, 0, SourcePath, FILE_TYPE_PATCH);
        }
        else
        {
            printf ("Unknown file type - %s\n", SourcePath);
        }
    }

    /* Always display final summary and stats */

    AsDisplayStats ();

    return (0);
}
