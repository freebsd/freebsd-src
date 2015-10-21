/******************************************************************************
 *
 * Module Name: aslcompile - top level compile module
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
#include <contrib/dev/acpica/compiler/dtcompiler.h>
#include <contrib/dev/acpica/include/acnamesp.h>

#include <stdio.h>
#include <time.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslcompile")

/*
 * Main parser entry
 * External is here in case the parser emits the same external in the
 * generated header. (Newer versions of Bison)
 */
int
AslCompilerparse(
    void);

/* Local prototypes */

static void
CmFlushSourceCode (
    void);

static void
CmDumpAllEvents (
    void);


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
CmDoCompile (
    void)
{
    ACPI_STATUS             Status;
    UINT8                   FullCompile;
    UINT8                   Event;


    FullCompile = UtBeginEvent ("*** Total Compile time ***");
    Event = UtBeginEvent ("Open input and output files");
    UtEndEvent (Event);

    Event = UtBeginEvent ("Preprocess input file");
    if (Gbl_PreprocessFlag)
    {
        /* Enter compiler name as a #define */

        PrAddDefine (ASL_DEFINE, "", FALSE);

        /* Preprocessor */

        PrDoPreprocess ();
        Gbl_CurrentLineNumber = 1;
        Gbl_LogicalLineNumber = 1;

        if (Gbl_PreprocessOnly)
        {
            UtEndEvent (Event);
            CmCleanupAndExit ();
            return (0);
        }
    }
    UtEndEvent (Event);


    /* Build the parse tree */

    Event = UtBeginEvent ("Parse source code and build parse tree");
    AslCompilerparse();
    UtEndEvent (Event);

    /* Check for parser-detected syntax errors */

    if (Gbl_SyntaxError)
    {
        fprintf (stderr, "Compiler aborting due to parser-detected syntax error(s)\n");
        LsDumpParseTree ();
        goto ErrorExit;
    }

    /* Did the parse tree get successfully constructed? */

    if (!RootNode)
    {
        /*
         * If there are no errors, then we have some sort of
         * internal problem.
         */
        AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL,
            NULL, "- Could not resolve parse tree root node");

        goto ErrorExit;
    }

    /* Flush out any remaining source after parse tree is complete */

    Event = UtBeginEvent ("Flush source input");
    CmFlushSourceCode ();

    /* Prune the parse tree if requested (debug purposes only) */

    if (Gbl_PruneParseTree)
    {
        AslPruneParseTree (Gbl_PruneDepth, Gbl_PruneType);
    }

    /* Optional parse tree dump, compiler debug output only */

    LsDumpParseTree ();

    OpcGetIntegerWidth (RootNode);
    UtEndEvent (Event);

    /* Pre-process parse tree for any operator transforms */

    Event = UtBeginEvent ("Parse tree transforms");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nParse tree transforms\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD,
        TrAmlTransformWalk, NULL, NULL);
    UtEndEvent (Event);

    /* Generate AML opcodes corresponding to the parse tokens */

    Event = UtBeginEvent ("Generate AML opcodes");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nGenerating AML opcodes\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL,
        OpcAmlOpcodeWalk, NULL);
    UtEndEvent (Event);

    /*
     * Now that the input is parsed, we can open the AML output file.
     * Note: by default, the name of this file comes from the table descriptor
     * within the input file.
     */
    Event = UtBeginEvent ("Open AML output file");
    Status = FlOpenAmlOutputFile (Gbl_OutputFilenamePrefix);
    UtEndEvent (Event);
    if (ACPI_FAILURE (Status))
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        return (-1);
    }

    /* Interpret and generate all compile-time constants */

    Event = UtBeginEvent ("Constant folding via AML interpreter");
    DbgPrint (ASL_DEBUG_OUTPUT,
        "\nInterpreting compile-time constant expressions\n\n");

    if (Gbl_FoldConstants)
    {
        TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD,
            OpcAmlConstantWalk, NULL, NULL);
    }
    else
    {
        DbgPrint (ASL_PARSE_OUTPUT, "    Optional folding disabled\n");
    }
    UtEndEvent (Event);

    /* Update AML opcodes if necessary, after constant folding */

    Event = UtBeginEvent ("Updating AML opcodes after constant folding");
    DbgPrint (ASL_DEBUG_OUTPUT,
        "\nUpdating AML opcodes after constant folding\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD,
        NULL, OpcAmlOpcodeUpdateWalk, NULL);
    UtEndEvent (Event);

    /* Calculate all AML package lengths */

    Event = UtBeginEvent ("Generate AML package lengths");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nGenerating Package lengths\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL,
        LnPackageLengthWalk, NULL);
    UtEndEvent (Event);

    if (Gbl_ParseOnlyFlag)
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        UtDisplaySummary (ASL_FILE_STDERR);
        if (Gbl_DebugFlag)
        {
            /* Print error summary to the stdout also */

            AePrintErrorLog (ASL_FILE_STDOUT);
            UtDisplaySummary (ASL_FILE_STDOUT);
        }
        UtEndEvent (FullCompile);
        return (0);
    }

    /*
     * Create an internal namespace and use it as a symbol table
     */

    /* Namespace loading */

    Event = UtBeginEvent ("Create ACPI Namespace");
    Status = LdLoadNamespace (RootNode);
    UtEndEvent (Event);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    /* Namespace cross-reference */

    AslGbl_NamespaceEvent = UtBeginEvent ("Cross reference parse tree and Namespace");
    Status = XfCrossReferenceNamespace ();
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    /* Namespace - Check for non-referenced objects */

    LkFindUnreferencedObjects ();
    UtEndEvent (AslGbl_NamespaceEvent);

    /*
     * Semantic analysis. This can happen only after the
     * namespace has been loaded and cross-referenced.
     *
     * part one - check control methods
     */
    Event = UtBeginEvent ("Analyze control method return types");
    AnalysisWalkInfo.MethodStack = NULL;

    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - Method analysis\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_TWICE,
        MtMethodAnalysisWalkBegin,
        MtMethodAnalysisWalkEnd, &AnalysisWalkInfo);
    UtEndEvent (Event);

    /* Semantic error checking part two - typing of method returns */

    Event = UtBeginEvent ("Determine object types returned by methods");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - Method typing\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD,
        NULL, AnMethodTypingWalkEnd, NULL);
    UtEndEvent (Event);

    /* Semantic error checking part three - operand type checking */

    Event = UtBeginEvent ("Analyze AML operand types");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - Operand type checking\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD,
        NULL, AnOperandTypecheckWalkEnd, &AnalysisWalkInfo);
    UtEndEvent (Event);

    /* Semantic error checking part four - other miscellaneous checks */

    Event = UtBeginEvent ("Miscellaneous analysis");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nSemantic analysis - miscellaneous\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD,
        AnOtherSemanticAnalysisWalkBegin,
        NULL, &AnalysisWalkInfo);
    UtEndEvent (Event);

    /* Calculate all AML package lengths */

    Event = UtBeginEvent ("Finish AML package length generation");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nGenerating Package lengths\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL,
        LnInitLengthsWalk, NULL);
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_UPWARD, NULL,
        LnPackageLengthWalk, NULL);
    UtEndEvent (Event);

    /* Code generation - emit the AML */

    Event = UtBeginEvent ("Generate AML code and write output files");
    CgGenerateAmlOutput ();
    UtEndEvent (Event);

    Event = UtBeginEvent ("Write optional output files");
    CmDoOutputFiles ();
    UtEndEvent (Event);

    UtEndEvent (FullCompile);
    CmCleanupAndExit ();
    return (0);

ErrorExit:
    UtEndEvent (FullCompile);
    CmCleanupAndExit ();
    return (-1);
}


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
    char                    *UtilityName;


    /* Set line prefix depending on the destination file type */

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
        else if ((Gbl_HexOutputFlag == HEX_OUTPUT_C) ||
                 (Gbl_HexOutputFlag == HEX_OUTPUT_ASL))
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "/*\n");
            Prefix = " * ";
        }
        break;

    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_OFFSET_OUTPUT:
    case ASL_FILE_C_INCLUDE_OUTPUT:

        Prefix = " * ";
        break;

    default:

        /* No other output types supported */

        break;
    }

    /* Running compiler or disassembler? */

    if (Gbl_DisasmFlag)
    {
        UtilityName = AML_DISASSEMBLER_NAME;
    }
    else
    {
        UtilityName = ASL_COMPILER_NAME;
    }

    /* Compiler signon with copyright */

    FlPrintFile (FileId, "%s\n", Prefix);
    FlPrintFile (FileId, ACPI_COMMON_HEADER (UtilityName, Prefix));
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


    /* Set line prefix depending on the destination file type */

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
        else if ((Gbl_HexOutputFlag == HEX_OUTPUT_C) ||
                 (Gbl_HexOutputFlag == HEX_OUTPUT_ASL))
        {
            Prefix = " * ";
        }
        break;

    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_OFFSET_OUTPUT:
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
    case ASL_FILE_C_OFFSET_OUTPUT:
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

static void
CmFlushSourceCode (
    void)
{
    char                    Buffer;


    while (FlReadFile (ASL_FILE_INPUT, &Buffer, 1) != AE_ERROR)
    {
        AslInsertLineBuffer ((int) Buffer);
    }

    AslResetCurrentLineBuffer ();
}


/*******************************************************************************
 *
 * FUNCTION:    CmDoOutputFiles
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Create all "listing" type files
 *
 ******************************************************************************/

void
CmDoOutputFiles (
    void)
{

    /* Create listings and hex files */

    LsDoListings ();
    HxDoHexOutput ();

    /* Dump the namespace to the .nsp file if requested */

    (void) NsDisplayNamespace ();

    /* Dump the device mapping file */

    MpEmitMappingInfo ();
}


/*******************************************************************************
 *
 * FUNCTION:    CmDumpAllEvents
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Dump all compiler events
 *
 ******************************************************************************/

static void
CmDumpAllEvents (
    void)
{
    ASL_EVENT_INFO          *Event;
    UINT32                  Delta;
    UINT32                  USec;
    UINT32                  MSec;
    UINT32                  i;


    Event = AslGbl_Events;

    DbgPrint (ASL_DEBUG_OUTPUT, "\n\nElapsed time for major events\n\n");
    if (Gbl_CompileTimesFlag)
    {
        printf ("\nElapsed time for major events\n\n");
    }

    for (i = 0; i < AslGbl_NextEvent; i++)
    {
        if (Event->Valid)
        {
            /* Delta will be in 100-nanosecond units */

            Delta = (UINT32) (Event->EndTime - Event->StartTime);

            USec = Delta / ACPI_100NSEC_PER_USEC;
            MSec = Delta / ACPI_100NSEC_PER_MSEC;

            /* Round milliseconds up */

            if ((USec - (MSec * ACPI_USEC_PER_MSEC)) >= 500)
            {
                MSec++;
            }

            DbgPrint (ASL_DEBUG_OUTPUT, "%8u usec %8u msec - %s\n",
                USec, MSec, Event->EventName);

            if (Gbl_CompileTimesFlag)
            {
                printf ("%8u usec %8u msec - %s\n",
                    USec, MSec, Event->EventName);
            }
        }

        Event++;
    }
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
CmCleanupAndExit (
    void)
{
    UINT32                  i;
    BOOLEAN                 DeleteAmlFile = FALSE;


    AePrintErrorLog (ASL_FILE_STDERR);
    if (Gbl_DebugFlag)
    {
        /* Print error summary to stdout also */

        AePrintErrorLog (ASL_FILE_STDOUT);
    }

    /* Emit compile times if enabled */

    CmDumpAllEvents ();

    if (Gbl_CompileTimesFlag)
    {
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
        DbgPrint (ASL_DEBUG_OUTPUT,
            "\n\nMiscellaneous compile statistics\n\n");

        DbgPrint (ASL_DEBUG_OUTPUT,
            "%32s : %u\n", "Total Namespace searches",
            Gbl_NsLookupCount);

        DbgPrint (ASL_DEBUG_OUTPUT,
            "%32s : %u usec\n", "Time per search", ((UINT32)
            (AslGbl_Events[AslGbl_NamespaceEvent].EndTime -
                AslGbl_Events[AslGbl_NamespaceEvent].StartTime) / 10) /
                Gbl_NsLookupCount);
    }

    if (Gbl_ExceptionCount[ASL_ERROR] > ASL_MAX_ERROR_COUNT)
    {
        printf ("\nMaximum error count (%u) exceeded\n",
            ASL_MAX_ERROR_COUNT);
    }

    UtDisplaySummary (ASL_FILE_STDOUT);

    /*
     * We will delete the AML file if there are errors and the
     * force AML output option has not been used.
     */
    if ((Gbl_ExceptionCount[ASL_ERROR] > 0) &&
        (!Gbl_IgnoreErrors) &&
        Gbl_Files[ASL_FILE_AML_OUTPUT].Handle)
    {
        DeleteAmlFile = TRUE;
    }

    /* Close all open files */

    /*
     * Take care with the preprocessor file (.pre), it might be the same
     * as the "input" file, depending on where the compiler has terminated
     * or aborted. Prevent attempt to close the same file twice in
     * loop below.
     */
    if (Gbl_Files[ASL_FILE_PREPROCESSOR].Handle ==
        Gbl_Files[ASL_FILE_INPUT].Handle)
    {
        Gbl_Files[ASL_FILE_PREPROCESSOR].Handle = NULL;
    }

    /* Close the standard I/O files */

    for (i = ASL_FILE_INPUT; i < ASL_MAX_FILE_TYPE; i++)
    {
        FlCloseFile (i);
    }

    /* Delete AML file if there are errors */

    if (DeleteAmlFile)
    {
        FlDeleteFile (ASL_FILE_AML_OUTPUT);
    }

    /* Delete the preprocessor temp file unless full debug was specified */

    if (Gbl_PreprocessFlag && !Gbl_KeepPreprocessorTempFile)
    {
        FlDeleteFile (ASL_FILE_PREPROCESSOR);
    }

    /*
     * Delete intermediate ("combined") source file (if -ls flag not set)
     * This file is created during normal ASL/AML compiles. It is not
     * created by the data table compiler.
     *
     * If the -ls flag is set, then the .SRC file should not be deleted.
     * In this case, Gbl_SourceOutputFlag is set to TRUE.
     *
     * Note: Handles are cleared by FlCloseFile above, so we look at the
     * filename instead, to determine if the .SRC file was actually
     * created.
     */
    if (!Gbl_SourceOutputFlag)
    {
        FlDeleteFile (ASL_FILE_SOURCE_OUTPUT);
    }

    /* Final cleanup after compiling one file */

    CmDeleteCaches ();
}


/*******************************************************************************
 *
 * FUNCTION:    CmDeleteCaches
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete all local cache buffer blocks
 *
 ******************************************************************************/

void
CmDeleteCaches (
    void)
{
    UINT32                  BufferCount;
    ASL_CACHE_INFO          *Next;


    /* Parse Op cache */

    BufferCount = 0;
    while (Gbl_ParseOpCacheList)
    {
        Next = Gbl_ParseOpCacheList->Next;
        ACPI_FREE (Gbl_ParseOpCacheList);
        Gbl_ParseOpCacheList = Next;
        BufferCount++;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "%u ParseOps, Buffer size: %u ops (%u bytes), %u Buffers\n",
        Gbl_ParseOpCount, ASL_PARSEOP_CACHE_SIZE,
        (sizeof (ACPI_PARSE_OBJECT) * ASL_PARSEOP_CACHE_SIZE), BufferCount);

    Gbl_ParseOpCount = 0;
    Gbl_ParseOpCacheNext = NULL;
    Gbl_ParseOpCacheLast = NULL;
    RootNode = NULL;

    /* Generic string cache */

    BufferCount = 0;
    while (Gbl_StringCacheList)
    {
        Next = Gbl_StringCacheList->Next;
        ACPI_FREE (Gbl_StringCacheList);
        Gbl_StringCacheList = Next;
        BufferCount++;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "%u Strings (%u bytes), Buffer size: %u bytes, %u Buffers\n",
        Gbl_StringCount, Gbl_StringSize, ASL_STRING_CACHE_SIZE, BufferCount);

    Gbl_StringSize = 0;
    Gbl_StringCount = 0;
    Gbl_StringCacheNext = NULL;
    Gbl_StringCacheLast = NULL;
}
