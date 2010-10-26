
/******************************************************************************
 *
 * Module Name: aslstartup - Compiler startup routines, called from main
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslstartup")


#define ASL_MAX_FILES   256
static char             *FileList[ASL_MAX_FILES];
static BOOLEAN          AslToFile = TRUE;


/* Local prototypes */

static char **
AsDoWildcard (
    char                    *DirectoryPathname,
    char                    *FileSpecifier);

static UINT8
AslDetectSourceFileType (
    ASL_FILE_INFO           *Info);


/*******************************************************************************
 *
 * FUNCTION:    AslInitializeGlobals
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Re-initialize globals needed to restart the compiler. This
 *              allows multiple files to be disassembled and/or compiled.
 *
 ******************************************************************************/

void
AslInitializeGlobals (
    void)
{
    UINT32                  i;


    /* Init compiler globals */

    Gbl_CurrentColumn = 0;
    Gbl_CurrentLineNumber = 1;
    Gbl_LogicalLineNumber = 1;
    Gbl_CurrentLineOffset = 0;
    Gbl_InputFieldCount = 0;
    Gbl_LineBufPtr = Gbl_CurrentLineBuffer;

    Gbl_ErrorLog = NULL;
    Gbl_NextError = NULL;
    Gbl_Signature = NULL;
    Gbl_FileType = 0;

    AslGbl_NextEvent = 0;
    for (i = 0; i < ASL_NUM_REPORT_LEVELS; i++)
    {
        Gbl_ExceptionCount[i] = 0;
    }

    Gbl_Files[ASL_FILE_AML_OUTPUT].Filename = NULL;
    Gbl_Files[ASL_FILE_AML_OUTPUT].Handle = NULL;

    Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Filename = NULL;
    Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Handle = NULL;
}


/******************************************************************************
 *
 * FUNCTION:    AsDoWildcard
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Process files via wildcards. This function is for the Windows
 *              case only.
 *
 ******************************************************************************/

static char **
AsDoWildcard (
    char                    *DirectoryPathname,
    char                    *FileSpecifier)
{
#ifdef WIN32
    void                    *DirInfo;
    char                    *Filename;
    int                     FileCount;


    FileCount = 0;

    /* Open parent directory */

    DirInfo = AcpiOsOpenDirectory (DirectoryPathname, FileSpecifier, REQUEST_FILE_ONLY);
    if (!DirInfo)
    {
        /* Either the directory of file does not exist */

        Gbl_Files[ASL_FILE_INPUT].Filename = FileSpecifier;
        FlFileError (ASL_FILE_INPUT, ASL_MSG_OPEN);
        AslAbort ();
    }

    /* Process each file that matches the wildcard specification */

    while ((Filename = AcpiOsGetNextFilename (DirInfo)))
    {
        /* Add the filename to the file list */

        FileList[FileCount] = AcpiOsAllocate (strlen (Filename) + 1);
        strcpy (FileList[FileCount], Filename);
        FileCount++;

        if (FileCount >= ASL_MAX_FILES)
        {
            printf ("Max files reached\n");
            FileList[0] = NULL;
            return (FileList);
        }
    }

    /* Cleanup */

    AcpiOsCloseDirectory (DirInfo);
    FileList[FileCount] = NULL;
    return (FileList);

#else
    /*
     * Linux/Unix cases - Wildcards are expanded by the shell automatically.
     * Just return the filename in a null terminated list
     */
    FileList[0] = AcpiOsAllocate (strlen (FileSpecifier) + 1);
    strcpy (FileList[0], FileSpecifier);
    FileList[1] = NULL;

    return (FileList);
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AslDetectSourceFileType
 *
 * PARAMETERS:  Info            - Name/Handle for the file (must be open)
 *
 * RETURN:      File Type
 *
 * DESCRIPTION: Determine the type of the input file. Either binary (contains
 *              non-ASCII characters), ASL file, or an ACPI Data Table file.
 *
 ******************************************************************************/

static UINT8
AslDetectSourceFileType (
    ASL_FILE_INFO           *Info)
{
    char                    *FileChar;
    UINT8                   Type;
    ACPI_STATUS             Status;


    /* Check for 100% ASCII source file (comments are ignored) */

    Status = FlCheckForAscii (Info);
    if (ACPI_FAILURE (Status))
    {
        printf ("Non-ascii input file - %s\n", Info->Filename);
        Type = ASL_INPUT_TYPE_BINARY;
        goto Cleanup;
    }

    /*
     * File is ASCII. Determine if this is an ASL file or an ACPI data
     * table file.
     */
    while (fgets (Gbl_CurrentLineBuffer, ASL_LINE_BUFFER_SIZE, Info->Handle))
    {
        /* Uppercase the buffer for caseless compare */

        FileChar = Gbl_CurrentLineBuffer;
        while (*FileChar)
        {
            *FileChar = (char) toupper ((int) *FileChar);
            FileChar++;
        }

        /* Presence of "DefinitionBlock" indicates actual ASL code */

        if (strstr (Gbl_CurrentLineBuffer, "DEFINITIONBLOCK"))
        {
            /* Appears to be an ASL file */

            Type = ASL_INPUT_TYPE_ASCII_ASL;
            goto Cleanup;
        }
    }

    /* Not an ASL source file, default to a data table source file */

    Type = ASL_INPUT_TYPE_ASCII_DATA;

Cleanup:

    /* Must seek back to the start of the file */

    fseek (Info->Handle, 0, SEEK_SET);
    return (Type);
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoOneFile
 *
 * PARAMETERS:  Filename        - Name of the file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Process a single file - either disassemble, compile, or both
 *
 ******************************************************************************/

ACPI_STATUS
AslDoOneFile (
    char                    *Filename)
{
    ACPI_STATUS             Status;


    Gbl_Files[ASL_FILE_INPUT].Filename = Filename;

    /* Re-initialize "some" compiler globals */

    AslInitializeGlobals ();

    /*
     * AML Disassembly (Optional)
     */
    if (Gbl_DisasmFlag || Gbl_GetAllTables)
    {
        /* ACPICA subsystem initialization */

        Status = AdInitialize ();
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Status = AcpiAllocateRootTable (4);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not initialize ACPI Table Manager, %s\n",
                AcpiFormatException (Status));
            return (Status);
        }

        /* This is where the disassembly happens */

        AcpiGbl_DbOpt_disasm = TRUE;
        Status = AdAmlDisassemble (AslToFile,
                    Gbl_Files[ASL_FILE_INPUT].Filename,
                    Gbl_OutputFilenamePrefix,
                    &Gbl_Files[ASL_FILE_INPUT].Filename,
                    Gbl_GetAllTables);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Shutdown compiler and ACPICA subsystem */

        AeClearErrorLog ();
        (void) AcpiTerminate ();

        /*
         * Gbl_Files[ASL_FILE_INPUT].Filename was replaced with the
         * .DSL disassembly file, which can now be compiled if requested
         */
        if (Gbl_DoCompile)
        {
            AcpiOsPrintf ("\nCompiling \"%s\"\n",
                Gbl_Files[ASL_FILE_INPUT].Filename);
        }
        else
        {
            Gbl_Files[ASL_FILE_INPUT].Filename = NULL;
            return (AE_OK);
        }
    }

    /*
     * Open the input file. Here, this should be an ASCII source file,
     * either an ASL file or a Data Table file
     */
    Status = FlOpenInputFile (Gbl_Files[ASL_FILE_INPUT].Filename);
    if (ACPI_FAILURE (Status))
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        return (AE_ERROR);
    }

    /* Determine input file type */

    Gbl_FileType = AslDetectSourceFileType (&Gbl_Files[ASL_FILE_INPUT]);
    if (Gbl_FileType == ASL_INPUT_TYPE_BINARY)
    {
        return (AE_ERROR);
    }

    /*
     * If -p not specified, we will use the input filename as the
     * output filename prefix
     */
    if (Gbl_UseDefaultAmlFilename)
    {
        Gbl_OutputFilenamePrefix = Gbl_Files[ASL_FILE_INPUT].Filename;
    }

    /* Open the optional output files (listings, etc.) */

    Status = FlOpenMiscOutputFiles (Gbl_OutputFilenamePrefix);
    if (ACPI_FAILURE (Status))
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        return (AE_ERROR);
    }

    /*
     * Compilation of ASL source versus DataTable source uses different
     * compiler subsystems
     */
    switch (Gbl_FileType)
    {
    /*
     * Data Table Compilation
     */
    case ASL_INPUT_TYPE_ASCII_DATA:

        Status = DtDoCompile ();

        if (Gbl_Signature)
        {
            ACPI_FREE (Gbl_Signature);
            Gbl_Signature = NULL;
        }
        AeClearErrorLog ();
        return (Status);

    /*
     * ASL Compilation (Optional)
     */
    case ASL_INPUT_TYPE_ASCII_ASL:

        /* ACPICA subsystem initialization */

        Status = AdInitialize ();
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Status = CmDoCompile ();
        (void) AcpiTerminate ();

        /*
         * Return non-zero exit code if there have been errors, unless the
         * global ignore error flag has been set
         */
        if ((Gbl_ExceptionCount[ASL_ERROR] > 0) && (!Gbl_IgnoreErrors))
        {
            return (AE_ERROR);
        }

        AeClearErrorLog ();
        return (AE_OK);

    case ASL_INPUT_TYPE_BINARY:

        AePrintErrorLog (ASL_FILE_STDERR);
        return (AE_ERROR);

    default:
        printf ("Unknown file type %X\n", Gbl_FileType);
        return (AE_ERROR);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoOnePathname
 *
 * PARAMETERS:  Pathname            - Full pathname, possibly with wildcards
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Process one pathname, possible terminated with a wildcard
 *              specification. If a wildcard, it is expanded and the multiple
 *              files are processed.
 *
 ******************************************************************************/

ACPI_STATUS
AslDoOnePathname (
    char                    *Pathname,
    ASL_PATHNAME_CALLBACK   PathCallback)
{
    ACPI_STATUS             Status = AE_OK;
    char                    **WildcardList;
    char                    *Filename;
    char                    *FullPathname;


    /* Split incoming path into a directory/filename combo */

    Status = FlSplitInputPathname (Pathname, &Gbl_DirectoryPath, &Filename);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Expand possible wildcard into a file list (Windows/DOS only) */

    WildcardList = AsDoWildcard (Gbl_DirectoryPath, Filename);
    while (*WildcardList)
    {
        FullPathname = ACPI_ALLOCATE (
            strlen (Gbl_DirectoryPath) + strlen (*WildcardList) + 1);

        /* Construct a full path to the file */

        strcpy (FullPathname, Gbl_DirectoryPath);
        strcat (FullPathname, *WildcardList);

        /*
         * If -p not specified, we will use the input filename as the
         * output filename prefix
         */
        if (Gbl_UseDefaultAmlFilename)
        {
            Gbl_OutputFilenamePrefix = FullPathname;
        }

        /* Save status from all compiles */

        Status |= (*PathCallback) (FullPathname);

        ACPI_FREE (FullPathname);
        ACPI_FREE (*WildcardList);
        *WildcardList = NULL;
        WildcardList++;
    }

    ACPI_FREE (Gbl_DirectoryPath);
    ACPI_FREE (Filename);
    return (Status);
}

