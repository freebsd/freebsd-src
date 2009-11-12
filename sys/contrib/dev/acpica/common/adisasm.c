/******************************************************************************
 *
 * Module Name: adisasm - Application-level disassembler routines
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


#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/include/acapps.h>

#include <stdio.h>
#include <time.h>


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("adisasm")


extern int                  AslCompilerdebug;
extern char                 *Gbl_ExternalFilename;


ACPI_STATUS
LsDisplayNamespace (
    void);

void
LsSetupNsList (
    void                    *Handle);


/* Local prototypes */

void
AdCreateTableHeader (
    char                    *Filename,
    ACPI_TABLE_HEADER       *Table);

void
AdDisassemblerHeader (
    char                    *Filename);

ACPI_STATUS
AdDeferredParse (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *Aml,
    UINT32                  AmlLength);

ACPI_STATUS
AdParseDeferredOps (
    ACPI_PARSE_OBJECT       *Root);


/* Stubs for ASL compiler */

#ifndef ACPI_ASL_COMPILER
BOOLEAN
AcpiDsIsResultUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    return TRUE;
}

ACPI_STATUS
AcpiDsMethodError (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState)
{
    return (Status);
}
#endif

ACPI_STATUS
AcpiNsLoadTable (
    UINT32                  TableIndex,
    ACPI_NAMESPACE_NODE     *Node)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc)
{
    return (AE_OK);
}

void
AcpiDsTerminateControlMethod (
    ACPI_OPERAND_OBJECT     *MethodDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

ACPI_STATUS
AcpiDsCallControlMethod (
    ACPI_THREAD_STATE       *Thread,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsMethodDataInitArgs (
    ACPI_OPERAND_OBJECT     **Params,
    UINT32                  MaxParamCount,
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_OK);
}


static ACPI_TABLE_DESC      LocalTables[1];
static ACPI_PARSE_OBJECT    *AcpiGbl_ParseOpRoot;


/*******************************************************************************
 *
 * FUNCTION:    AdInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: ACPICA and local initialization
 *
 ******************************************************************************/

ACPI_STATUS
AdInitialize (
    void)
{
    ACPI_STATUS             Status;


    /* ACPI CA subsystem initialization */

    Status = AcpiOsInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiUtInitGlobals ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiUtMutexInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiNsRootInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Setup the Table Manager (cheat - there is no RSDT) */

    AcpiGbl_RootTableList.Size = 1;
    AcpiGbl_RootTableList.Count = 0;
    AcpiGbl_RootTableList.Tables = LocalTables;

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AdAmlDisassemble
 *
 * PARAMETERS:  Filename            - AML input filename
 *              OutToFile           - TRUE if output should go to a file
 *              Prefix              - Path prefix for output
 *              OutFilename         - where the filename is returned
 *              GetAllTables        - TRUE if all tables are desired
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disassemble an entire ACPI table
 *
 *****************************************************************************/

ACPI_STATUS
AdAmlDisassemble (
    BOOLEAN                 OutToFile,
    char                    *Filename,
    char                    *Prefix,
    char                    **OutFilename,
    BOOLEAN                 GetAllTables)
{
    ACPI_STATUS             Status;
    char                    *DisasmFilename = NULL;
    char                    *ExternalFilename;
    FILE                    *File = NULL;
    ACPI_TABLE_HEADER       *Table = NULL;
    ACPI_TABLE_HEADER       *ExternalTable;
    ACPI_OWNER_ID           OwnerId;


    /*
     * Input: AML code from either a file or via GetTables (memory or
     * registry)
     */
    if (Filename)
    {
        Status = AcpiDbGetTableFromFile (Filename, &Table);
        if (ACPI_FAILURE (Status))
        {
            return Status;
        }

        /*
         * External filenames separated by commas
         * Example: iasl -e file1,file2,file3 -d xxx.aml
         */
        if (Gbl_ExternalFilename)
        {
            ExternalFilename = strtok (Gbl_ExternalFilename, ",");

            while (ExternalFilename)
            {
                Status = AcpiDbGetTableFromFile (ExternalFilename, &ExternalTable);
                if (ACPI_FAILURE (Status))
                {
                    return Status;
                }

                /* Load external table for symbol resolution */

                if (ExternalTable)
                {
                    Status = AdParseTable (ExternalTable, &OwnerId, TRUE, TRUE);
                    if (ACPI_FAILURE (Status))
                    {
                        AcpiOsPrintf ("Could not parse external ACPI tables, %s\n",
                            AcpiFormatException (Status));
                        return Status;
                    }

                    /*
                     * Load namespace from names created within control methods
                     * Set owner id of nodes in external table
                     */
                    AcpiDmFinishNamespaceLoad (AcpiGbl_ParseOpRoot,
                        AcpiGbl_RootNode, OwnerId);
                    AcpiPsDeleteParseTree (AcpiGbl_ParseOpRoot);
                }

                /* Next external file name */

                ExternalFilename = strtok (NULL, ",");
            }

            /* Clear external list generated by Scope in external tables */

            AcpiDmClearExternalList ();
        }
    }
    else
    {
        Status = AdGetLocalTables (Filename, GetAllTables);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not get ACPI tables, %s\n",
                AcpiFormatException (Status));
            return Status;
        }

        if (!AcpiGbl_DbOpt_disasm)
        {
            return AE_OK;
        }

        /* Obtained the local tables, just disassemble the DSDT */

        Status = AcpiGetTable (ACPI_SIG_DSDT, 0, &Table);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not get DSDT, %s\n",
                AcpiFormatException (Status));
            return Status;
        }

        AcpiOsPrintf ("\nDisassembly of DSDT\n");
        Prefix = AdGenerateFilename ("dsdt", Table->OemTableId);
    }

    /*
     * Output:  ASL code. Redirect to a file if requested
     */
    if (OutToFile)
    {
        /* Create/Open a disassembly output file */

        DisasmFilename = FlGenerateFilename (Prefix, FILE_SUFFIX_DISASSEMBLY);
        if (!OutFilename)
        {
            fprintf (stderr, "Could not generate output filename\n");
            Status = AE_ERROR;
            goto Cleanup;
        }

        File = fopen (DisasmFilename, "w+");
        if (!File)
        {
            fprintf (stderr, "Could not open output file %s\n", DisasmFilename);
            Status = AE_ERROR;
            goto Cleanup;
        }

        AcpiOsRedirectOutput (File);
    }

    *OutFilename = DisasmFilename;

    if (!AcpiUtIsAmlTable (Table))
    {
        AdDisassemblerHeader (Filename);
        AcpiOsPrintf (" * ACPI Data Table [%4.4s]\n *\n",
            Table->Signature);
        AcpiOsPrintf (" * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue\n */\n\n");

        AcpiDmDumpDataTable (Table);
        fprintf (stderr, "Acpi Data Table [%4.4s] decoded, written to \"%s\"\n",
            Table->Signature, DisasmFilename);
    }
    else
    {
        /* Always parse the tables, only option is what to display */

        Status = AdParseTable (Table, &OwnerId, TRUE, FALSE);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not parse ACPI tables, %s\n",
                AcpiFormatException (Status));
            goto Cleanup;
        }

        if (AslCompilerdebug)
        {
            AcpiOsPrintf ("/**** Before second load\n");

            LsSetupNsList (File);
            LsDisplayNamespace ();
            AcpiOsPrintf ("*****/\n");
        }

        /*
         * Load namespace from names created within control methods
         */
        AcpiDmFinishNamespaceLoad (AcpiGbl_ParseOpRoot, AcpiGbl_RootNode, OwnerId);

        /*
         * Cross reference the namespace here, in order to generate External() statements
         */
        AcpiDmCrossReferenceNamespace (AcpiGbl_ParseOpRoot, AcpiGbl_RootNode, OwnerId);

        if (AslCompilerdebug)
        {
            AcpiDmDumpTree (AcpiGbl_ParseOpRoot);
        }

        /* Find possible calls to external control methods */

        AcpiDmFindOrphanMethods (AcpiGbl_ParseOpRoot);

        /* Convert fixed-offset references to resource descriptors to symbolic references */

        AcpiDmConvertResourceIndexes (AcpiGbl_ParseOpRoot, AcpiGbl_RootNode);

        /*
         * If we found any external control methods, we must reparse the entire
         * tree with the new information (namely, the number of arguments per
         * method)
         */
        if (AcpiDmGetExternalMethodCount ())
        {
            fprintf (stderr,
                "\nFound %d external control methods, reparsing with new information\n",
                AcpiDmGetExternalMethodCount ());

            /*
             * Reparse, rebuild namespace. no need to xref namespace
             */
            AcpiPsDeleteParseTree (AcpiGbl_ParseOpRoot);
            AcpiNsDeleteNamespaceSubtree (AcpiGbl_RootNode);

            AcpiGbl_RootNode                    = NULL;
            AcpiGbl_RootNodeStruct.Name.Integer = ACPI_ROOT_NAME;
            AcpiGbl_RootNodeStruct.DescriptorType = ACPI_DESC_TYPE_NAMED;
            AcpiGbl_RootNodeStruct.Type         = ACPI_TYPE_DEVICE;
            AcpiGbl_RootNodeStruct.Child        = NULL;
            AcpiGbl_RootNodeStruct.Peer         = NULL;
            AcpiGbl_RootNodeStruct.Object       = NULL;
            AcpiGbl_RootNodeStruct.Flags        = ANOBJ_END_OF_PEER_LIST;

            Status = AcpiNsRootInitialize ();
            AcpiDmAddExternalsToNamespace ();

            /* Parse table. No need to reload it, however (FALSE) */

            Status = AdParseTable (Table, NULL, FALSE, FALSE);
            if (ACPI_FAILURE (Status))
            {
                AcpiOsPrintf ("Could not parse ACPI tables, %s\n",
                    AcpiFormatException (Status));
                goto Cleanup;
            }

            if (AslCompilerdebug)
            {
                AcpiOsPrintf ("/**** After second load and resource conversion\n");
                LsSetupNsList (File);
                LsDisplayNamespace ();
                AcpiOsPrintf ("*****/\n");

                AcpiDmDumpTree (AcpiGbl_ParseOpRoot);
            }
        }

        /* Optional displays */

        if (AcpiGbl_DbOpt_disasm)
        {
            AdDisplayTables (Filename, Table);
            fprintf (stderr,
                "Disassembly completed, written to \"%s\"\n",
                DisasmFilename);
        }
    }

Cleanup:

    if (Table && !AcpiUtIsAmlTable (Table))
    {
        ACPI_FREE (Table);
    }

    if (DisasmFilename)
    {
        ACPI_FREE (DisasmFilename);
    }

    if (OutToFile && File)
    {

#ifdef ASL_DISASM_DEBUG
        LsSetupNsList (File);
        LsDisplayNamespace ();
#endif
        fclose (File);
        AcpiOsRedirectOutput (stdout);
    }

    AcpiPsDeleteParseTree (AcpiGbl_ParseOpRoot);
    AcpiGbl_ParseOpRoot = NULL;
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AdDisassemblerHeader
 *
 * PARAMETERS:  Filename            - Input file for the table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the disassembler header, including ACPI CA signon with
 *              current time and date.
 *
 *****************************************************************************/

void
AdDisassemblerHeader (
    char                    *Filename)
{
    time_t                  Timer;

    time (&Timer);

    /* Header and input table info */

    AcpiOsPrintf ("/*\n * Intel ACPI Component Architecture\n");
    AcpiOsPrintf (" * AML Disassembler version %8.8X\n", ACPI_CA_VERSION);

    AcpiOsPrintf (" *\n * Disassembly of %s, %s", Filename, ctime (&Timer));
    AcpiOsPrintf (" *\n");
}


/******************************************************************************
 *
 * FUNCTION:    AdCreateTableHeader
 *
 * PARAMETERS:  Filename            - Input file for the table
 *              Table               - Pointer to the raw table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the ASL table header, including ACPI CA signon with
 *              current time and date.
 *
 *****************************************************************************/

void
AdCreateTableHeader (
    char                    *Filename,
    ACPI_TABLE_HEADER       *Table)
{
    char                    *NewFilename;
    UINT8                   Checksum;


    /*
     * Print file header and dump original table header
     */
    AdDisassemblerHeader (Filename);

    AcpiOsPrintf (" *\n * Original Table Header:\n");
    AcpiOsPrintf (" *     Signature        \"%4.4s\"\n",    Table->Signature);
    AcpiOsPrintf (" *     Length           0x%8.8X (%u)\n", Table->Length, Table->Length);

    /* Print and validate the revision */

    AcpiOsPrintf (" *     Revision         0x%2.2X",      Table->Revision);

    switch (Table->Revision)
    {
    case 0:
        AcpiOsPrintf (" **** Invalid Revision");
        break;

    case 1:
        /* Revision of DSDT controls the ACPI integer width */

        if (ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_DSDT))
        {
            AcpiOsPrintf (" **** ACPI 1.0, no 64-bit math support");
        }
        break;

    default:
        break;
    }
    AcpiOsPrintf ("\n");

    /* Print and validate the table checksum */

    AcpiOsPrintf (" *     Checksum         0x%2.2X",        Table->Checksum);

    Checksum = AcpiTbChecksum (ACPI_CAST_PTR (UINT8, Table), Table->Length);
    if (Checksum)
    {
        AcpiOsPrintf (" **** Incorrect checksum, should be 0x%2.2X",
            (UINT8) (Table->Checksum - Checksum));
    }
    AcpiOsPrintf ("\n");

    AcpiOsPrintf (" *     OEM ID           \"%.6s\"\n",     Table->OemId);
    AcpiOsPrintf (" *     OEM Table ID     \"%.8s\"\n",     Table->OemTableId);
    AcpiOsPrintf (" *     OEM Revision     0x%8.8X (%u)\n", Table->OemRevision, Table->OemRevision);
    AcpiOsPrintf (" *     Compiler ID      \"%.4s\"\n",     Table->AslCompilerId);
    AcpiOsPrintf (" *     Compiler Version 0x%8.8X (%u)\n", Table->AslCompilerRevision, Table->AslCompilerRevision);
    AcpiOsPrintf (" */\n");

    /* Create AML output filename based on input filename */

    if (Filename)
    {
        NewFilename = FlGenerateFilename (Filename, "aml");
    }
    else
    {
        NewFilename = ACPI_ALLOCATE_ZEROED (9);
        strncat (NewFilename, Table->Signature, 4);
        strcat (NewFilename, ".aml");
    }

    /* Open the ASL definition block */

    AcpiOsPrintf (
        "DefinitionBlock (\"%s\", \"%4.4s\", %hd, \"%.6s\", \"%.8s\", 0x%8.8X)\n",
        NewFilename, Table->Signature, Table->Revision,
        Table->OemId, Table->OemTableId, Table->OemRevision);

    ACPI_FREE (NewFilename);
}


/******************************************************************************
 *
 * FUNCTION:    AdDisplayTables
 *
 * PARAMETERS:  Filename            - Input file for the table
 *              Table               - Pointer to the raw table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display (disassemble) loaded tables and dump raw tables
 *
 *****************************************************************************/

ACPI_STATUS
AdDisplayTables (
    char                    *Filename,
    ACPI_TABLE_HEADER       *Table)
{


    if (!AcpiGbl_ParseOpRoot)
    {
        return AE_NOT_EXIST;
    }

    if (!AcpiGbl_DbOpt_verbose)
    {
        AdCreateTableHeader (Filename, Table);
    }

    AcpiDmDisassemble (NULL, AcpiGbl_ParseOpRoot, ACPI_UINT32_MAX);

    if (AcpiGbl_DbOpt_verbose)
    {
        AcpiOsPrintf ("\n\nTable Header:\n");
        AcpiUtDumpBuffer ((UINT8 *) Table, sizeof (ACPI_TABLE_HEADER),
            DB_BYTE_DISPLAY, ACPI_UINT32_MAX);

        AcpiOsPrintf ("Table Body (Length 0x%X)\n", Table->Length);
        AcpiUtDumpBuffer (((UINT8 *) Table + sizeof (ACPI_TABLE_HEADER)), Table->Length,
            DB_BYTE_DISPLAY, ACPI_UINT32_MAX);
    }

    return AE_OK;
}


/******************************************************************************
 *
 * FUNCTION:    AdDeferredParse
 *
 * PARAMETERS:  Op                  - Root Op of the deferred opcode
 *              Aml                 - Pointer to the raw AML
 *              AmlLength           - Length of the AML
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse one deferred opcode
 *              (Methods, operation regions, etc.)
 *
 *****************************************************************************/

ACPI_STATUS
AdDeferredParse (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *Aml,
    UINT32                  AmlLength)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *SearchOp;
    ACPI_PARSE_OBJECT       *StartOp;
    UINT32                  BaseAmlOffset;
    ACPI_PARSE_OBJECT       *ExtraOp;


    ACPI_FUNCTION_TRACE (AdDeferredParse);


    fprintf (stderr, ".");

    if (!Aml || !AmlLength)
    {
        return_ACPI_STATUS (AE_OK);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Parsing %s [%4.4s]\n",
        Op->Common.AmlOpName, (char *) &Op->Named.Name));

    WalkState = AcpiDsCreateWalkState (0, Op, NULL, NULL);
    if (!WalkState)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, NULL, Aml,
                    AmlLength, NULL, ACPI_IMODE_LOAD_PASS1);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Parse the method */

    WalkState->ParseFlags &= ~ACPI_PARSE_DELETE_TREE;
    WalkState->ParseFlags |= ACPI_PARSE_DISASSEMBLE;
    Status = AcpiPsParseAml (WalkState);

    /*
     * We need to update all of the Aml offsets, since the parser thought
     * that the method began at offset zero.  In reality, it began somewhere
     * within the ACPI table, at the BaseAmlOffset.  Walk the entire tree that
     * was just created and update the AmlOffset in each Op
     */
    BaseAmlOffset = (Op->Common.Value.Arg)->Common.AmlOffset + 1;
    StartOp = (Op->Common.Value.Arg)->Common.Next;
    SearchOp = StartOp;

    /* Walk the parse tree */

    while (SearchOp)
    {
        SearchOp->Common.AmlOffset += BaseAmlOffset;
        SearchOp = AcpiPsGetDepthNext (StartOp, SearchOp);
    }

    /*
     * Link the newly parsed subtree into the main parse tree
     */
    switch (Op->Common.AmlOpcode)
    {
    case AML_BUFFER_OP:
    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP:

        switch (Op->Common.AmlOpcode)
        {
        case AML_PACKAGE_OP:
            ExtraOp = Op->Common.Value.Arg;
            ExtraOp = ExtraOp->Common.Next;
            Op->Common.Value.Arg = ExtraOp->Common.Value.Arg;
            break;

        case AML_VAR_PACKAGE_OP:
        case AML_BUFFER_OP:
        default:
            ExtraOp = Op->Common.Value.Arg;
            Op->Common.Value.Arg = ExtraOp->Common.Value.Arg;
            break;
        }

        /* Must point all parents to the main tree */

        StartOp = Op;
        SearchOp = StartOp;
        while (SearchOp)
        {
            if (SearchOp->Common.Parent == ExtraOp)
            {
                SearchOp->Common.Parent = Op;
            }
            SearchOp = AcpiPsGetDepthNext (StartOp, SearchOp);
        }
        break;

    default:
        break;
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AdParseDeferredOps
 *
 * PARAMETERS:  Root                - Root of the parse tree
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse the deferred opcodes (Methods, regions, etc.)
 *
 *****************************************************************************/

ACPI_STATUS
AdParseDeferredOps (
    ACPI_PARSE_OBJECT       *Root)
{
    ACPI_PARSE_OBJECT       *Op = Root;
    ACPI_STATUS             Status = AE_OK;
    const ACPI_OPCODE_INFO  *OpInfo;


    ACPI_FUNCTION_NAME (AdParseDeferredOps);
    fprintf (stderr, "Parsing Deferred Opcodes (Methods/Buffers/Packages/Regions)\n");

    while (Op)
    {
        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (!(OpInfo->Flags & AML_DEFER))
        {
            Op = AcpiPsGetDepthNext (Root, Op);
            continue;
        }

        switch (Op->Common.AmlOpcode)
        {
        case AML_METHOD_OP:
        case AML_BUFFER_OP:
        case AML_PACKAGE_OP:
        case AML_VAR_PACKAGE_OP:

            Status = AdDeferredParse (Op, Op->Named.Data, Op->Named.Length);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;

        case AML_REGION_OP:
        case AML_CREATE_QWORD_FIELD_OP:
        case AML_CREATE_DWORD_FIELD_OP:
        case AML_CREATE_WORD_FIELD_OP:
        case AML_CREATE_BYTE_FIELD_OP:
        case AML_CREATE_BIT_FIELD_OP:
        case AML_CREATE_FIELD_OP:
        case AML_BANK_FIELD_OP:

            /* Nothing to do in these cases */

            break;

        default:
            ACPI_ERROR ((AE_INFO, "Unhandled deferred opcode [%s]",
                Op->Common.AmlOpName));
            break;
        }

        Op = AcpiPsGetDepthNext (Root, Op);
    }

    fprintf (stderr, "\n");
    return Status;
}


/******************************************************************************
 *
 * FUNCTION:    AdGetLocalTables
 *
 * PARAMETERS:  Filename            - Not used
 *              GetAllTables        - TRUE if all tables are desired
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the ACPI tables from either memory or a file
 *
 *****************************************************************************/

ACPI_STATUS
AdGetLocalTables (
    char                    *Filename,
    BOOLEAN                 GetAllTables)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       TableHeader;
    ACPI_TABLE_HEADER       *NewTable;
    UINT32                  NumTables;
    UINT32                  PointerSize;
    UINT32                  TableIndex;


    if (GetAllTables)
    {
        ACPI_MOVE_32_TO_32 (TableHeader.Signature, ACPI_SIG_RSDT);
        AcpiOsTableOverride (&TableHeader, &NewTable);
        if (!NewTable)
        {
            fprintf (stderr, "Could not obtain RSDT\n");
            return AE_NO_ACPI_TABLES;
        }
        else
        {
            AdWriteTable (NewTable, NewTable->Length,
                ACPI_SIG_RSDT, NewTable->OemTableId);
        }

        if (ACPI_COMPARE_NAME (NewTable->Signature, ACPI_SIG_RSDT))
        {
            PointerSize = sizeof (UINT32);
        }
        else
        {
            PointerSize = sizeof (UINT64);
        }

        /*
         * Determine the number of tables pointed to by the RSDT/XSDT.
         * This is defined by the ACPI Specification to be the number of
         * pointers contained within the RSDT/XSDT.  The size of the pointers
         * is architecture-dependent.
         */
        NumTables = (NewTable->Length - sizeof (ACPI_TABLE_HEADER)) / PointerSize;
        AcpiOsPrintf ("There are %d tables defined in the %4.4s\n\n",
            NumTables, NewTable->Signature);

        /* Get the FADT */

        ACPI_MOVE_32_TO_32 (TableHeader.Signature, ACPI_SIG_FADT);
        AcpiOsTableOverride (&TableHeader, &NewTable);
        if (NewTable)
        {
            AdWriteTable (NewTable, NewTable->Length,
                ACPI_SIG_FADT, NewTable->OemTableId);
        }
        AcpiOsPrintf ("\n");

        /* Don't bother with FACS, it is usually all zeros */
    }

    /* Always get the DSDT */

    ACPI_MOVE_32_TO_32 (TableHeader.Signature, ACPI_SIG_DSDT);
    AcpiOsTableOverride (&TableHeader, &NewTable);
    if (NewTable)
    {
        AdWriteTable (NewTable, NewTable->Length,
            ACPI_SIG_DSDT, NewTable->OemTableId);

        /* Store DSDT in the Table Manager */

        Status = AcpiTbStoreTable (0, NewTable, NewTable->Length,
                    0, &TableIndex);
        if (ACPI_FAILURE (Status))
        {
            fprintf (stderr, "Could not store DSDT\n");
            return AE_NO_ACPI_TABLES;
        }
    }
    else
    {
        fprintf (stderr, "Could not obtain DSDT\n");
        return AE_NO_ACPI_TABLES;
    }

#if 0
    /* TBD: Future implementation */

    AcpiOsPrintf ("\n");

    /* Get all SSDTs */

    ACPI_MOVE_32_TO_32 (TableHeader.Signature, ACPI_SIG_SSDT);
    do
    {
        NewTable = NULL;
        Status = AcpiOsTableOverride (&TableHeader, &NewTable);

    } while (NewTable);
#endif

    return AE_OK;
}


/******************************************************************************
 *
 * FUNCTION:    AdParseTable
 *
 * PARAMETERS:  Table               - Pointer to the raw table
 *              OwnerId             - Returned OwnerId of the table
 *              LoadTable           - If add table to the global table list
 *              External            - If this is an external table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse the DSDT.
 *
 *****************************************************************************/

ACPI_STATUS
AdParseTable (
    ACPI_TABLE_HEADER       *Table,
    ACPI_OWNER_ID           *OwnerId,
    BOOLEAN                 LoadTable,
    BOOLEAN                 External)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_WALK_STATE         *WalkState;
    UINT8                   *AmlStart;
    UINT32                  AmlLength;
    UINT32                  TableIndex;


    if (!Table)
    {
        return AE_NOT_EXIST;
    }

    /* Pass 1:  Parse everything except control method bodies */

    fprintf (stderr, "Pass 1 parse of [%4.4s]\n", (char *) Table->Signature);

    AmlLength = Table->Length - sizeof (ACPI_TABLE_HEADER);
    AmlStart = ((UINT8 *) Table + sizeof (ACPI_TABLE_HEADER));

    /* Create the root object */

    AcpiGbl_ParseOpRoot = AcpiPsCreateScopeOp ();
    if (!AcpiGbl_ParseOpRoot)
    {
        return AE_NO_MEMORY;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0,
                        AcpiGbl_ParseOpRoot, NULL, NULL);
    if (!WalkState)
    {
        return (AE_NO_MEMORY);
    }

    Status = AcpiDsInitAmlWalk (WalkState, AcpiGbl_ParseOpRoot,
                NULL, AmlStart, AmlLength, NULL, ACPI_IMODE_LOAD_PASS1);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    WalkState->ParseFlags &= ~ACPI_PARSE_DELETE_TREE;
    WalkState->ParseFlags |= ACPI_PARSE_DISASSEMBLE;

    Status = AcpiPsParseAml (WalkState);
    if (ACPI_FAILURE (Status))
    {
        return Status;
    }

    /* If LoadTable is FALSE, we are parsing the last loaded table */

    TableIndex = AcpiGbl_RootTableList.Count - 1;

    /* Pass 2 */

    if (LoadTable)
    {
        Status = AcpiTbStoreTable ((ACPI_PHYSICAL_ADDRESS) Table, Table,
                    Table->Length, ACPI_TABLE_ORIGIN_ALLOCATED, &TableIndex);
        if (ACPI_FAILURE (Status))
        {
            return Status;
        }
        Status = AcpiTbAllocateOwnerId (TableIndex);
        if (ACPI_FAILURE (Status))
        {
            return Status;
        }
        if (OwnerId)
        {
            Status = AcpiTbGetOwnerId (TableIndex, OwnerId);
            if (ACPI_FAILURE (Status))
            {
                return Status;
            }
        }
    }

    fprintf (stderr, "Pass 2 parse of [%4.4s]\n", (char *) Table->Signature);

    Status = AcpiNsOneCompleteParse (ACPI_IMODE_LOAD_PASS2, TableIndex, NULL);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* No need to parse control methods of external table */

    if (External)
    {
        return AE_OK;
    }

    /* Pass 3: Parse control methods and link their parse trees into the main parse tree */

    Status = AdParseDeferredOps (AcpiGbl_ParseOpRoot);

    /* Process Resource Templates */

    AcpiDmFindResources (AcpiGbl_ParseOpRoot);

    fprintf (stderr, "Parsing completed\n");
    return AE_OK;
}


