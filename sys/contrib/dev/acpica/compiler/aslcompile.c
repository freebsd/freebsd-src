
/******************************************************************************
 *
 * Module Name: aslcompile - top level compile module
 *              $Revision: 73 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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

#include <stdio.h>
#include "aslcompiler.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslcompile")


/*******************************************************************************
 *
 * FUNCTION:    AslCompilerSignon
 *
 * PARAMETERS:  FileId      - ID of the output file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display compiler signon
 *
 ******************************************************************************/

void
AslCompilerSignon (
    UINT32                  FileId)
{
    char                    *Prefix = "";


    /*
     * Set line prefix depending on the destination file type
     */
    switch (FileId)
    {
    case ASL_FILE_ASM_SOURCE_OUTPUT:
    case ASL_FILE_ASM_INCLUDE_OUTPUT:

        Prefix = "; ";
        break;

    case ASL_FILE_HEX_OUTPUT:

        if (Gbl_HexOutputFlag == HEX_OUTPUT_ASM)
        {
            Prefix = "; ";
        }
        else if (Gbl_HexOutputFlag == HEX_OUTPUT_C)
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "/*\n");
            Prefix = " * ";
        }
        break;

    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_INCLUDE_OUTPUT:

        Prefix = " * ";
        break;

    default:
        /* No other output types supported */
        break;
    }

    /* Compiler signon with copyright */

    FlPrintFile (FileId,
        "%s\n%s%s\n%s%s version %X [%s]\n%s%s\n%sSupports ACPI Specification Revision 2.0c\n%s\n",
        Prefix,
        Prefix, IntelAcpiCA,
        Prefix, CompilerId, ACPI_CA_VERSION, __DATE__,
        Prefix, CompilerCopyright,
        Prefix,
        Prefix);
}


/*******************************************************************************
 *
 * FUNCTION:    AslCompilerFileHeader
 *
 * PARAMETERS:  FileId      - ID of the output file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Header used at the beginning of output files
 *
 ******************************************************************************/

void
AslCompilerFileHeader (
    UINT32                  FileId)
{
    struct tm               *NewTime;
    time_t                  Aclock;
    char                    *Prefix = "";


    /*
     * Set line prefix depending on the destination file type
     */
    switch (FileId)
    {
    case ASL_FILE_ASM_SOURCE_OUTPUT:
    case ASL_FILE_ASM_INCLUDE_OUTPUT:

        Prefix = "; ";
        break;

    case ASL_FILE_HEX_OUTPUT:

        if (Gbl_HexOutputFlag == HEX_OUTPUT_ASM)
        {
            Prefix = "; ";
        }
        else if (Gbl_HexOutputFlag == HEX_OUTPUT_C)
        {
            Prefix = " * ";
        }
        break;

    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_INCLUDE_OUTPUT:

        Prefix = " * ";
        break;

    default:
        /* No other output types supported */
        break;
    }

    /* Compilation header with timestamp */

    (void) time (&Aclock);
    NewTime = localtime (&Aclock);

    FlPrintFile (FileId,
        "%sCompilation of \"%s\" - %s%s\n",
        Prefix, Gbl_Files[ASL_FILE_INPUT].Filename, asctime (NewTime),
        Prefix);

    switch (FileId)
    {
    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_INCLUDE_OUTPUT:
        FlPrintFile (FileId, " */\n");
        break;

    default:
        /* Nothing to do for other output types */
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CmFlushSourceCode
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read in any remaining source code after the parse tree
 *              has been constructed.
 *
 ******************************************************************************/

void
CmFlushSourceCode (void)
{
    char                    Buffer;


    while (FlReadFile (ASL_FILE_INPUT, &Buffer, 1) != AE_ERROR)
    {
        InsertLineBuffer ((int) Buffer);
    }

    ResetCurrentLineBuffer ();
}


/*******************************************************************************
 *
 * FUNCTION:    FlCheckForAscii
 *
 * PARAMETERS:  FileInfo        - Points to an open input file
 *
 * RETURN:      Status (0 = OK)
 *
 * DESCRIPTION: Verify that the input file is entirely ASCII.
 *
 ******************************************************************************/

ACPI_STATUS
FlCheckForAscii (
    ASL_FILE_INFO           *FileInfo)
{
    UINT8                   Byte;
    ACPI_SIZE               BadBytes = 0;
    ACPI_SIZE               Offset = 0;


    /* Read the entire file */

    while (fread (&Byte, 1, 1, FileInfo->Handle))
    {
        /* Check for an ASCII character */

        if (!isascii (Byte))
        {
            if (BadBytes < 10)
            {
                AcpiOsPrintf ("Non-ASCII character: 0x%2.2X at offset 0x%X\n", Byte, Offset);
            }
            BadBytes++;
        }
        Offset++;
    }

    /* Were there any non-ASCII characters in the file? */

    if (BadBytes)
    {
        AcpiOsPrintf ("%d non-ASCII characters found in input file, appears to be binary\n", BadBytes);
        AslError (ASL_ERROR, ASL_MSG_NON_ASCII, NULL, FileInfo->Filename);
        return (AE_BAD_CHARACTER);
    }

    /* File is OK, seek back to the beginning */

    fseek (FileInfo->Handle, 0, SEEK_SET);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    CmDoCompile
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status (0 = OK)
 *
 * DESCRIPTION: This procedure performs the entire compile
 *
 ******************************************************************************/

int
CmDoCompile (void)
{
    ACPI_STATUS             Status;
    UINT32                  i = 0;


    UtBeginEvent (12, "Total Compile time");
    UtBeginEvent (i, "Initialize");

    /* Open the required input and output files */

    Status = FlOpenInputFile (Gbl_Files[ASL_FILE_INPUT].Filename);
    if (ACPI_FAILURE (Status))
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        return -1;
    }

    /* Ensure that the input file is 100% ASCII text */

    Status = FlCheckForAscii (&Gbl_Files[ASL_FILE_INPUT]);
    if (ACPI_FAILURE (Status))
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        return -1;
    }

    Status = FlOpenMiscOutputFiles (Gbl_OutputFilenamePrefix);
    if (ACPI_FAILURE (Status))
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        return -1;
    }

    UtEndEvent (i++);

    /* Build the parse tree */

    UtBeginEvent (i, "Parse source code and build parse tree");
    AslCompilerparse();
    UtEndEvent (i++);

    /* Flush out any remaining source after parse tree is complete */

    CmFlushSourceCode ();

    /* Did the parse tree get successfully constructed? */

    if (!RootNode)
    {
        CmCleanupAndExit ();
        return -1;
    }

    OpcGetIntegerWidth (RootNode);

    /* Pre-process parse tree for any operator transforms */

    UtBeginEvent (i, "Generate AML opcodes");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nParse tree transforms\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD, TrAmlTransformWalk, NULL, NULL);

    /* Generate AML opcodes corresponding to the parse tokens */

    DbgPrint (ASL_DEBUG_OUTPUT, "\nGenerating AML opcodes\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL, OpcAmlOpcodeWalk, NULL);
    UtEndEvent (i++);

    /*
     * Now that the input is parsed, we can open the AML output file.
     * Note: by default, the name of this file comes from the table descriptor
     * within the input file.
     */
    Status = FlOpenAmlOutputFile (Gbl_OutputFilenamePrefix);
    if (ACPI_FAILURE (Status))
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        return -1;
    }

    /* Interpret and generate all compile-time constants */

    UtBeginEvent (i, "Constant folding via AML interpreter");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nInterpreting compile-time constant expressions\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD, OpcAmlConstantWalk, NULL, NULL);
    UtEndEvent (i++);

    /* Calculate all AML package lengths */

    UtBeginEvent (i, "Generate AML package lengths");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nGenerating Package lengths\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL, LnPackageLengthWalk, NULL);
    UtEndEvent (i++);

    if (Gbl_ParseOnlyFlag)
    {
        AePrintErrorLog (ASL_FILE_STDOUT);
        UtDisplaySummary (ASL_FILE_STDOUT);
        if (Gbl_DebugFlag)
        {
            /* Print error summary to the debug file */

            AePrintErrorLog (ASL_FILE_STDERR);
            UtDisplaySummary (ASL_FILE_STDERR);
        }
        return 0;
    }

    /*
     * Create an internal namespace and use it as a symbol table
     */

    /* Namespace loading */

    UtBeginEvent (i, "Create ACPI Namespace");
    Status = LdLoadNamespace (RootNode);
    UtEndEvent (i++);
    if (ACPI_FAILURE (Status))
    {
        return -1;
    }

    /* Namespace lookup */

    UtBeginEvent (i, "Cross reference parse tree and Namespace");
    Status = LkCrossReferenceNamespace ();
    UtEndEvent (i++);
    UtEndEvent (i++);
    if (ACPI_FAILURE (Status))
    {
        return -1;
    }

    /*
     * Semantic analysis.  This can happen only after the
     * namespace has been loaded and cross-referenced.
     *
     * part one - check control methods
     */
    UtBeginEvent (i, "Analyze control method return types");
    AnalysisWalkInfo.MethodStack = NULL;

    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - Method analysis\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_TWICE, AnMethodAnalysisWalkBegin,
                        AnMethodAnalysisWalkEnd, &AnalysisWalkInfo);
    UtEndEvent (i++);

    /* Semantic error checking part two - typing of method returns */

    UtBeginEvent (i, "Determine object types returned by methods");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - Method typing \n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_TWICE, AnMethodTypingWalkBegin,
                        AnMethodTypingWalkEnd, NULL);
    UtEndEvent (i++);

    /* Semantic error checking part three - operand type checking */

    UtBeginEvent (i, "Analyze AML operand types");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - Operand type checking \n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_TWICE, AnOperandTypecheckWalkBegin,
                        AnOperandTypecheckWalkEnd, &AnalysisWalkInfo);
    UtEndEvent (i++);

    /* Semantic error checking part four - other miscellaneous checks */

    UtBeginEvent (i, "Miscellaneous analysis");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - miscellaneous \n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_TWICE, AnOtherSemanticAnalysisWalkBegin,
                        AnOtherSemanticAnalysisWalkEnd, &AnalysisWalkInfo);
    UtEndEvent (i++);

    /* Calculate all AML package lengths */

    UtBeginEvent (i, "Finish AML package length generation");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nGenerating Package lengths\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL, LnInitLengthsWalk, NULL);
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL, LnPackageLengthWalk, NULL);
    UtEndEvent (i++);


    /* Code generation - emit the AML */

    UtBeginEvent (i, "Generate AML code and write output files");
    CgGenerateAmlOutput ();
    UtEndEvent (i++);

    UtBeginEvent (i, "Write optional output files");
    CmDoOutputFiles ();
    UtEndEvent (i++);

    UtEndEvent (13);
    CmCleanupAndExit ();
    return 0;
}

void
CmDoOutputFiles (void)
{

    /* Create listings and hex files */

    LsDoListings ();
    LsDoHexOutput ();

    /* Dump the namespace to the .nsp file if requested */

    LsDisplayNamespace ();
}


/*******************************************************************************
 *
 * FUNCTION:    CmCleanupAndExit
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Close all open files and exit the compiler
 *
 ******************************************************************************/

void
CmCleanupAndExit (void)
{
    UINT32                  i;


    AePrintErrorLog (ASL_FILE_STDOUT);
    if (Gbl_DebugFlag)
    {
        /* Print error summary to the debug file */

        AePrintErrorLog (ASL_FILE_STDERR);
    }

    DbgPrint (ASL_DEBUG_OUTPUT, "\n\nElapsed time for major events\n\n");
    for (i = 0; i < 13; i++)
    {
        if (AslGbl_Events[i].Valid)
        {
            DbgPrint (ASL_DEBUG_OUTPUT, "%8lu ms - %s\n",
                AslGbl_Events[i].EndTime -
                AslGbl_Events[i].StartTime,
                AslGbl_Events[i].EventName);
        }
    }

    if (Gbl_CompileTimesFlag)
    {
        printf ("\nElapsed time for major events\n\n");
        for (i = 0; i < 13; i++)
        {
            if (AslGbl_Events[i].Valid)
            {
                printf ("%8lu ms : %s\n",
                    AslGbl_Events[i].EndTime -
                    AslGbl_Events[i].StartTime,
                    AslGbl_Events[i].EventName);
            }
        }
        printf ("\nMiscellaneous compile statistics\n\n");
        printf ("%11u : %s\n", TotalParseNodes, "Parse nodes");
        printf ("%11u : %s\n", Gbl_NsLookupCount, "Namespace searches");
        printf ("%11u : %s\n", TotalNamedObjects, "Named objects");
        printf ("%11u : %s\n", TotalMethods, "Control methods");
        printf ("%11u : %s\n", TotalAllocations, "Memory Allocations");
        printf ("%11u : %s\n", TotalAllocated, "Total allocated memory");
        printf ("%11u : %s\n", TotalFolds, "Constant subtrees folded");
        printf ("\n");
    }

    if (Gbl_NsLookupCount)
    {
        DbgPrint (ASL_DEBUG_OUTPUT, "\n\nMiscellaneous compile statistics\n\n");
        DbgPrint (ASL_DEBUG_OUTPUT, "%32s : %d\n", "Total Namespace searches", Gbl_NsLookupCount);
        DbgPrint (ASL_DEBUG_OUTPUT, "%32s : %d\n", "Time per search",
                        ((UINT32) (AslGbl_Events[7].EndTime - AslGbl_Events[7].StartTime) * 1000) /
                        Gbl_NsLookupCount);
    }

    /* Close all open files */

    for (i = 2; i < ASL_MAX_FILE_TYPE; i++)
    {
        FlCloseFile (i);
    }

    /*
     * TBD: SourceOutput should be .TMP, then rename if we want to keep it?
     */
    if (!Gbl_SourceOutputFlag)
    {
        unlink (Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Filename);
    }

    /* Delete AML file if there are errors */

    if ((Gbl_ExceptionCount[ASL_ERROR] > 0) && (!Gbl_IgnoreErrors))
    {
        unlink (Gbl_Files[ASL_FILE_AML_OUTPUT].Filename);
    }

    if (Gbl_ExceptionCount[ASL_ERROR] > ASL_MAX_ERROR_COUNT)
    {
        printf ("\nMaximum error count (%d) exceeded.\n", ASL_MAX_ERROR_COUNT);
    }

    UtDisplaySummary (ASL_FILE_STDOUT);
    exit (0);
}


