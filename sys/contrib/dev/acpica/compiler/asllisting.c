/******************************************************************************
 *
 * Module Name: asllisting - Listing file generation
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
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asllisting")


/* Local prototypes */

static void
LsGenerateListing (
    UINT32                  FileId);

static ACPI_STATUS
LsAmlListingWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
LsTreeWriteWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static void
LsWriteNodeToListing (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  FileId);

static void
LsFinishSourceListing (
    UINT32                  FileId);


/*******************************************************************************
 *
 * FUNCTION:    LsDoListings
 *
 * PARAMETERS:  None. Examines the various output file global flags.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generate all requested listing files.
 *
 ******************************************************************************/

void
LsDoListings (
    void)
{

    if (Gbl_C_OutputFlag)
    {
        LsGenerateListing (ASL_FILE_C_SOURCE_OUTPUT);
    }

    if (Gbl_ListingFlag)
    {
        LsGenerateListing (ASL_FILE_LISTING_OUTPUT);
    }

    if (Gbl_AsmOutputFlag)
    {
        LsGenerateListing (ASL_FILE_ASM_SOURCE_OUTPUT);
    }

    if (Gbl_C_IncludeOutputFlag)
    {
        LsGenerateListing (ASL_FILE_C_INCLUDE_OUTPUT);
    }

    if (Gbl_AsmIncludeOutputFlag)
    {
        LsGenerateListing (ASL_FILE_ASM_INCLUDE_OUTPUT);
    }

    if (Gbl_C_OffsetTableFlag)
    {
        LsGenerateListing (ASL_FILE_C_OFFSET_OUTPUT);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsGenerateListing
 *
 * PARAMETERS:  FileId      - ID of listing file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generate a listing file. This can be one of the several types
 *              of "listings" supported.
 *
 ******************************************************************************/

static void
LsGenerateListing (
    UINT32                  FileId)
{

    /* Start at the beginning of both the source and AML files */

    FlSeekFile (ASL_FILE_SOURCE_OUTPUT, 0);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);
    Gbl_SourceLine = 0;
    Gbl_CurrentHexColumn = 0;
    LsPushNode (Gbl_Files[ASL_FILE_INPUT].Filename);

    if (FileId == ASL_FILE_C_OFFSET_OUTPUT)
    {
        Gbl_CurrentAmlOffset = 0;

        /* Offset table file has a special header and footer */

        LsDoOffsetTableHeader (FileId);

        TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD, LsAmlOffsetWalk,
            NULL, (void *) ACPI_TO_POINTER (FileId));
        LsDoOffsetTableFooter (FileId);
        return;
    }

    /* Process all parse nodes */

    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD, LsAmlListingWalk,
        NULL, (void *) ACPI_TO_POINTER (FileId));

    /* Final processing */

    LsFinishSourceListing (FileId);
}


/*******************************************************************************
 *
 * FUNCTION:    LsAmlListingWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Process one node during a listing file generation.
 *
 ******************************************************************************/

static ACPI_STATUS
LsAmlListingWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    UINT8                   FileByte;
    UINT32                  i;
    UINT32                  FileId = (UINT32) ACPI_TO_INTEGER (Context);


    LsWriteNodeToListing (Op, FileId);

    if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DATA)
    {
        /* Buffer is a resource template, don't dump the data all at once */

        return (AE_OK);
    }

    /* Write the hex bytes to the listing file(s) (if requested) */

    for (i = 0; i < Op->Asl.FinalAmlLength; i++)
    {
        if (ACPI_FAILURE (FlReadFile (ASL_FILE_AML_OUTPUT, &FileByte, 1)))
        {
            FlFileError (ASL_FILE_AML_OUTPUT, ASL_MSG_READ);
            AslAbort ();
        }
        LsWriteListingHexBytes (&FileByte, 1, FileId);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LsDumpParseTree, LsTreeWriteWalk
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump entire parse tree, for compiler debug only
 *
 ******************************************************************************/

void
LsDumpParseTree (
    void)
{

    if (!Gbl_DebugFlag)
    {
        return;
    }

    DbgPrint (ASL_TREE_OUTPUT, "\nOriginal parse tree from parser:\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD,
        LsTreeWriteWalk, NULL, NULL);
}


static ACPI_STATUS
LsTreeWriteWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    /* Debug output */

    DbgPrint (ASL_TREE_OUTPUT,
        "%5.5d [%2d]", Op->Asl.LogicalLineNumber, Level);

    UtPrintFormattedName (Op->Asl.ParseOpcode, Level);

    DbgPrint (ASL_TREE_OUTPUT, "    (%.4X) Flags %8.8X",
        Op->Asl.ParseOpcode, Op->Asl.CompileFlags);
    TrPrintNodeCompileFlags (Op->Asl.CompileFlags);
    DbgPrint (ASL_TREE_OUTPUT, "\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LsWriteNodeToListing
 *
 * PARAMETERS:  Op              - Parse node to write to the listing file.
 *              FileId          - ID of current listing file
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Write "a node" to the listing file. This means to
 *              1) Write out all of the source text associated with the node
 *              2) Write out all of the AML bytes associated with the node
 *              3) Write any compiler exceptions associated with the node
 *
 ******************************************************************************/

static void
LsWriteNodeToListing (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  FileId)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  OpClass;
    char                    *Pathname;
    UINT32                  Length;
    UINT32                  i;


    OpInfo  = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    OpClass = OpInfo->Class;

    /* TBD: clean this up with a single flag that says:
     * I start a named output block
     */
    if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
    {
        switch (Op->Asl.ParseOpcode)
        {
        case PARSEOP_DEFINITIONBLOCK:
        case PARSEOP_METHODCALL:
        case PARSEOP_INCLUDE:
        case PARSEOP_INCLUDE_END:
        case PARSEOP_DEFAULT_ARG:

            break;

        default:

            switch (OpClass)
            {
            case AML_CLASS_NAMED_OBJECT:

                switch (Op->Asl.AmlOpcode)
                {
                case AML_SCOPE_OP:
                case AML_ALIAS_OP:

                    break;

                default:

                    if (Op->Asl.ExternalName)
                    {
                        LsFlushListingBuffer (FileId);
                        FlPrintFile (FileId, "    };\n");
                    }
                    break;
                }
                break;

            default:

                /* Don't care about other objects */

                break;
            }
            break;
        }
    }

    /* These cases do not have a corresponding AML opcode */

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_DEFINITIONBLOCK:

        LsWriteSourceLines (Op->Asl.EndLine, Op->Asl.EndLogicalLine, FileId);

        /* Use the table Signature and TableId to build a unique name */

        if (FileId == ASL_FILE_ASM_SOURCE_OUTPUT)
        {
            FlPrintFile (FileId,
                "%s_%s_Header \\\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
        {
            FlPrintFile (FileId,
                "    unsigned char    %s_%s_Header [] =\n    {\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        if (FileId == ASL_FILE_ASM_INCLUDE_OUTPUT)
        {
            FlPrintFile (FileId,
                "extrn %s_%s_Header : byte\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        if (FileId == ASL_FILE_C_INCLUDE_OUTPUT)
        {
            FlPrintFile (FileId,
                "extern unsigned char    %s_%s_Header [];\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        return;


    case PARSEOP_METHODCALL:

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);
        return;


    case PARSEOP_INCLUDE:

        /* Flush everything up to and including the include source line */

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);

        /* Create a new listing node and push it */

        LsPushNode (Op->Asl.Child->Asl.Value.String);
        return;


    case PARSEOP_INCLUDE_END:

        /* Flush out the rest of the include file */

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);

        /* Pop off this listing node and go back to the parent file */

        (void) LsPopNode ();
        return;


    case PARSEOP_DEFAULT_ARG:

        if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC)
        {
            LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.EndLogicalLine,
                FileId);
        }
        return;


    default:

        /* All other opcodes have an AML opcode */

        break;
    }

    /*
     * Otherwise, we look at the AML opcode because we can
     * switch on the opcode type, getting an entire class
     * at once
     */
    switch (OpClass)
    {
    case AML_CLASS_ARGUMENT:       /* argument type only */
    case AML_CLASS_INTERNAL:

        break;

    case AML_CLASS_NAMED_OBJECT:

        switch (Op->Asl.AmlOpcode)
        {
        case AML_FIELD_OP:
        case AML_INDEX_FIELD_OP:
        case AML_BANK_FIELD_OP:
            /*
             * For fields, we want to dump all the AML after the
             * entire definition
             */
            LsWriteSourceLines (Op->Asl.EndLine, Op->Asl.EndLogicalLine,
                FileId);
            break;

        case AML_NAME_OP:

            if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC)
            {
                LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
                    FileId);
            }
            else
            {
                /*
                 * For fields, we want to dump all the AML after the
                 * entire definition
                 */
                LsWriteSourceLines (Op->Asl.EndLine, Op->Asl.EndLogicalLine,
                    FileId);
            }
            break;

        default:

            LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
                FileId);
            break;
        }

        switch (Op->Asl.AmlOpcode)
        {
        case AML_SCOPE_OP:
        case AML_ALIAS_OP:

            /* These opcodes do not declare a new object, ignore them */

            break;

        default:

            /* All other named object opcodes come here */

            switch (FileId)
            {
            case ASL_FILE_ASM_SOURCE_OUTPUT:
            case ASL_FILE_C_SOURCE_OUTPUT:
            case ASL_FILE_ASM_INCLUDE_OUTPUT:
            case ASL_FILE_C_INCLUDE_OUTPUT:
                /*
                 * For named objects, we will create a valid symbol so that the
                 * AML code can be referenced from C or ASM
                 */
                if (Op->Asl.ExternalName)
                {
                    /* Get the full pathname associated with this node */

                    Pathname = AcpiNsGetExternalPathname (Op->Asl.Node);
                    Length = strlen (Pathname);
                    if (Length >= 4)
                    {
                        /* Convert all dots in the path to underscores */

                        for (i = 0; i < Length; i++)
                        {
                            if (Pathname[i] == '.')
                            {
                                Pathname[i] = '_';
                            }
                        }

                        /* Create the appropriate symbol in the output file */

                        if (FileId == ASL_FILE_ASM_SOURCE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "%s_%s_%s  \\\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "    unsigned char    %s_%s_%s [] =\n    {\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                        if (FileId == ASL_FILE_ASM_INCLUDE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "extrn %s_%s_%s : byte\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                        if (FileId == ASL_FILE_C_INCLUDE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "extern unsigned char    %s_%s_%s [];\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                    }
                    ACPI_FREE (Pathname);
                }
                break;

            default:

                /* Nothing to do for listing file */

                break;
            }
        }
        break;

    case AML_CLASS_EXECUTE:
    case AML_CLASS_CREATE:
    default:

        if ((Op->Asl.ParseOpcode == PARSEOP_BUFFER) &&
            (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC))
        {
            return;
        }

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);
        break;

    case AML_CLASS_UNKNOWN:

        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsFinishSourceListing
 *
 * PARAMETERS:  FileId          - ID of current listing file.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Cleanup routine for the listing file. Flush the hex AML
 *              listing buffer, and flush out any remaining lines in the
 *              source input file.
 *
 ******************************************************************************/

static void
LsFinishSourceListing (
    UINT32                  FileId)
{

    if ((FileId == ASL_FILE_ASM_INCLUDE_OUTPUT) ||
        (FileId == ASL_FILE_C_INCLUDE_OUTPUT))
    {
        return;
    }

    LsFlushListingBuffer (FileId);
    Gbl_CurrentAmlOffset = 0;

    /* Flush any remaining text in the source file */

    if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
    {
        FlPrintFile (FileId, "    /*\n");
    }

    while (LsWriteOneSourceLine (FileId))
    { ; }

    if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
    {
        FlPrintFile (FileId, "\n     */\n    };\n");
    }

    FlPrintFile (FileId, "\n");

    if (FileId == ASL_FILE_LISTING_OUTPUT)
    {
        /* Print a summary of the compile exceptions */

        FlPrintFile (FileId, "\n\nSummary of errors and warnings\n\n");
        AePrintErrorLog (FileId);
        FlPrintFile (FileId, "\n");
        UtDisplaySummary (FileId);
        FlPrintFile (FileId, "\n");
    }
}
