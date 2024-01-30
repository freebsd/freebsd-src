/******************************************************************************
 *
 * Module Name: asmain - Main module for the acpi source processor utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "acpisrc.h"

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
BOOLEAN                 Gbl_CheckAscii = FALSE;
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
#define AS_SUPPORTED_OPTIONS        "acdhilqsuv^y"


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
            ((float) Gbl_SourceLines /
            (float) (Gbl_CommentLines + Gbl_NonAnsiComments)));
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

    ACPI_OPTION ("-a <file>",   "Check entire file for non-printable characters");
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
    ACPI_OPTION ("-vd",         "Display build date and time");
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

        case 'd':

            printf (ACPI_COMMON_BUILD_TIME);
            return (0);

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

    case 'a':

        Gbl_CheckAscii = TRUE;
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

    /* This option checks the entire file for printable ascii chars */

    if (Gbl_CheckAscii)
    {
        AsProcessOneFile (NULL, NULL, NULL, 0, SourcePath, FILE_TYPE_SOURCE);
        return (0);
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

    /*
     * Set LF only support. Note ACPI_SRC_OS_LF_ONLY indicates that newlines
     * are represented as LF only rather than CR/LF
     */
    ConversionTable->Flags |= ACPI_SRC_OS_LF_ONLY;
    Gbl_IgnoreLoneLineFeeds = ACPI_SRC_OS_LF_ONLY;

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
        if (Gbl_CheckAscii)
        {
            AsProcessOneFile (NULL, NULL, NULL, 0,
                SourcePath, FILE_TYPE_SOURCE);
            return (0);
        }

        /* Process a single file */

        /* Differentiate between source and header files */

        if (strstr (SourcePath, ".h"))
        {
            AsProcessOneFile (ConversionTable, NULL, TargetPath, 0,
                SourcePath, FILE_TYPE_HEADER);
        }
        else if (strstr (SourcePath, ".c"))
        {
            AsProcessOneFile (ConversionTable, NULL, TargetPath, 0,
                SourcePath, FILE_TYPE_SOURCE);
        }
        else if (strstr (SourcePath, ".patch"))
        {
            AsProcessOneFile (ConversionTable, NULL, TargetPath, 0,
                SourcePath, FILE_TYPE_PATCH);
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
