/******************************************************************************
 *
 * Module Name: adisasm - Application-level disassembler routines
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
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/include/acapps.h>

#include <stdio.h>
#include <time.h>


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("adisasm")

/* Local prototypes */

static void
AdCreateTableHeader (
    char                    *Filename,
    ACPI_TABLE_HEADER       *Table);

static ACPI_STATUS
AdStoreTable (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  *TableIndex);

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


    /* ACPICA subsystem initialization */

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

    AcpiGbl_RootTableList.MaxTableCount = 1;
    AcpiGbl_RootTableList.CurrentTableCount = 0;
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
    char                    **OutFilename)
{
    ACPI_STATUS             Status;
    ACPI_STATUS             GlobalStatus = AE_OK;
    char                    *DisasmFilename = NULL;
    char                    *ExternalFilename;
    ACPI_EXTERNAL_FILE      *ExternalFileList = AcpiGbl_ExternalFileList;
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
        Status = AcpiDbGetTableFromFile (Filename, &Table, FALSE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /*
         * External filenames separated by commas
         * Example: iasl -e file1,file2,file3 -d xxx.aml
         */
        while (ExternalFileList)
        {
            ExternalFilename = ExternalFileList->Path;
            if (!strcmp (ExternalFilename, Filename))
            {
                /* Next external file */

                ExternalFileList = ExternalFileList->Next;
                continue;
            }

            Status = AcpiDbGetTableFromFile (ExternalFilename, &ExternalTable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                if (Status == AE_TYPE)
                {
                    ExternalFileList = ExternalFileList->Next;
                    GlobalStatus = AE_TYPE;
                    Status = AE_OK;
                    continue;
                }
                return (Status);
            }

            /* Load external table for symbol resolution */

            if (ExternalTable)
            {
                Status = AdParseTable (ExternalTable, &OwnerId, TRUE, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    AcpiOsPrintf ("Could not parse external ACPI tables, %s\n",
                        AcpiFormatException (Status));
                    return (Status);
                }

                /*
                 * Load namespace from names created within control methods
                 * Set owner id of nodes in external table
                 */
                AcpiDmFinishNamespaceLoad (AcpiGbl_ParseOpRoot,
                    AcpiGbl_RootNode, OwnerId);
                AcpiPsDeleteParseTree (AcpiGbl_ParseOpRoot);
            }

            /* Next external file */

            ExternalFileList = ExternalFileList->Next;
        }

        if (ACPI_FAILURE (GlobalStatus))
        {
            return (GlobalStatus);
        }

        /* Clear external list generated by Scope in external tables */

        if (AcpiGbl_ExternalFileList)
        {
            AcpiDmClearExternalList ();
        }

        /* Load any externals defined in the optional external ref file */

        AcpiDmGetExternalsFromFile ();
    }
    else
    {
        Status = AdGetLocalTables ();
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not get ACPI tables, %s\n",
                AcpiFormatException (Status));
            return (Status);
        }

        if (!AcpiGbl_DmOpt_Disasm)
        {
            return (AE_OK);
        }

        /* Obtained the local tables, just disassemble the DSDT */

        Status = AcpiGetTable (ACPI_SIG_DSDT, 0, &Table);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not get DSDT, %s\n",
                AcpiFormatException (Status));
            return (Status);
        }

        AcpiOsPrintf ("\nDisassembly of DSDT\n");
        Prefix = AdGenerateFilename ("dsdt", Table->OemTableId);
    }

    /*
     * Output: ASL code. Redirect to a file if requested
     */
    if (OutToFile)
    {
        /* Create/Open a disassembly output file */

        DisasmFilename = FlGenerateFilename (Prefix, FILE_SUFFIX_DISASSEMBLY);
        if (!DisasmFilename)
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

    /* ForceAmlDisassembly means to assume the table contains valid AML */

    if (!AcpiGbl_ForceAmlDisassembly && !AcpiUtIsAmlTable (Table))
    {
        AdDisassemblerHeader (Filename, ACPI_IS_DATA_TABLE);
        AcpiOsPrintf (" * ACPI Data Table [%4.4s]\n *\n",
            Table->Signature);
        AcpiOsPrintf (" * Format: [HexOffset DecimalOffset ByteLength]  "
            "FieldName : FieldValue\n */\n\n");

        AcpiDmDumpDataTable (Table);
        fprintf (stderr, "Acpi Data Table [%4.4s] decoded\n",
            Table->Signature);

        if (File)
        {
            fprintf (stderr, "Formatted output:  %s - %u bytes\n",
                DisasmFilename, CmGetFileSize (File));
        }
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

            if (File)
            {
                NsSetupNamespaceListing (File);
                NsDisplayNamespace ();
            }
            AcpiOsPrintf ("*****/\n");
        }

        /* Load namespace from names created within control methods */

        AcpiDmFinishNamespaceLoad (AcpiGbl_ParseOpRoot,
            AcpiGbl_RootNode, OwnerId);

        /*
         * Cross reference the namespace here, in order to
         * generate External() statements
         */
        AcpiDmCrossReferenceNamespace (AcpiGbl_ParseOpRoot,
            AcpiGbl_RootNode, OwnerId);

        if (AslCompilerdebug)
        {
            AcpiDmDumpTree (AcpiGbl_ParseOpRoot);
        }

        /* Find possible calls to external control methods */

        AcpiDmFindOrphanMethods (AcpiGbl_ParseOpRoot);

        /*
         * If we found any external control methods, we must reparse
         * the entire tree with the new information (namely, the
         * number of arguments per method)
         */
        if (AcpiDmGetExternalMethodCount ())
        {
            fprintf (stderr,
                "\nFound %u external control methods, "
                "reparsing with new information\n",
                AcpiDmGetExternalMethodCount ());

            /* Reparse, rebuild namespace */

            AcpiPsDeleteParseTree (AcpiGbl_ParseOpRoot);
            AcpiGbl_ParseOpRoot = NULL;
            AcpiNsDeleteNamespaceSubtree (AcpiGbl_RootNode);

            AcpiGbl_RootNode                    = NULL;
            AcpiGbl_RootNodeStruct.Name.Integer = ACPI_ROOT_NAME;
            AcpiGbl_RootNodeStruct.DescriptorType = ACPI_DESC_TYPE_NAMED;
            AcpiGbl_RootNodeStruct.Type         = ACPI_TYPE_DEVICE;
            AcpiGbl_RootNodeStruct.Parent       = NULL;
            AcpiGbl_RootNodeStruct.Child        = NULL;
            AcpiGbl_RootNodeStruct.Peer         = NULL;
            AcpiGbl_RootNodeStruct.Object       = NULL;
            AcpiGbl_RootNodeStruct.Flags        = 0;

            Status = AcpiNsRootInitialize ();

            /* New namespace, add the external definitions first */

            AcpiDmAddExternalsToNamespace ();

            /* Parse the table again. No need to reload it, however */

            Status = AdParseTable (Table, NULL, FALSE, FALSE);
            if (ACPI_FAILURE (Status))
            {
                AcpiOsPrintf ("Could not parse ACPI tables, %s\n",
                    AcpiFormatException (Status));
                goto Cleanup;
            }

            /* Cross reference the namespace again */

            AcpiDmFinishNamespaceLoad (AcpiGbl_ParseOpRoot,
                AcpiGbl_RootNode, OwnerId);

            AcpiDmCrossReferenceNamespace (AcpiGbl_ParseOpRoot,
                AcpiGbl_RootNode, OwnerId);

            if (AslCompilerdebug)
            {
                AcpiOsPrintf ("/**** After second load and resource conversion\n");
                if (File)
                {
                    NsSetupNamespaceListing (File);
                    NsDisplayNamespace ();
                }
                AcpiOsPrintf ("*****/\n");

                AcpiDmDumpTree (AcpiGbl_ParseOpRoot);
            }
        }

        /*
         * Now that the namespace is finalized, we can perform namespace
         * transforms.
         *
         * 1) Convert fixed-offset references to resource descriptors
         *    to symbolic references (Note: modifies namespace)
         */
        AcpiDmConvertResourceIndexes (AcpiGbl_ParseOpRoot, AcpiGbl_RootNode);

        /* Optional displays */

        if (AcpiGbl_DmOpt_Disasm)
        {
            /* This is the real disassembly */

            AdDisplayTables (Filename, Table);

            /* Dump hex table if requested (-vt) */

            AcpiDmDumpDataTable (Table);

            fprintf (stderr, "Disassembly completed\n");
            if (File)
            {
                fprintf (stderr, "ASL Output:    %s - %u bytes\n",
                    DisasmFilename, CmGetFileSize (File));
            }

            if (Gbl_MapfileFlag)
            {
                fprintf (stderr, "%14s %s - %u bytes\n",
                    Gbl_Files[ASL_FILE_MAP_OUTPUT].ShortDescription,
                    Gbl_Files[ASL_FILE_MAP_OUTPUT].Filename,
                    FlGetFileSize (ASL_FILE_MAP_OUTPUT));
            }
        }
    }

Cleanup:

    if (Table && !AcpiGbl_ForceAmlDisassembly &&!AcpiUtIsAmlTable (Table))
    {
        ACPI_FREE (Table);
    }

    if (File)
    {
        if (AslCompilerdebug) /* Display final namespace, with transforms */
        {
            NsSetupNamespaceListing (File);
            NsDisplayNamespace ();
        }

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
 *              TableType           - Either AML or DataTable
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the disassembler header, including ACPICA signon with
 *              current time and date.
 *
 *****************************************************************************/

void
AdDisassemblerHeader (
    char                    *Filename,
    UINT8                   TableType)
{
    time_t                  Timer;


    time (&Timer);

    /* Header and input table info */

    AcpiOsPrintf ("/*\n");
    AcpiOsPrintf (ACPI_COMMON_HEADER (AML_DISASSEMBLER_NAME, " * "));

    if (TableType == ACPI_IS_AML_TABLE)
    {
        if (AcpiGbl_CstyleDisassembly)
        {
            AcpiOsPrintf (
                " * Disassembling to symbolic ASL+ operators\n"
                " *\n");
        }
        else
        {
            AcpiOsPrintf (
                " * Disassembling to non-symbolic legacy ASL operators\n"
                " *\n");
        }
    }

    AcpiOsPrintf (" * Disassembly of %s, %s", Filename, ctime (&Timer));
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
 * DESCRIPTION: Create the ASL table header, including ACPICA signon with
 *              current time and date.
 *
 *****************************************************************************/

static void
AdCreateTableHeader (
    char                    *Filename,
    ACPI_TABLE_HEADER       *Table)
{
    char                    *NewFilename;
    UINT8                   Checksum;


    /*
     * Print file header and dump original table header
     */
    AdDisassemblerHeader (Filename, ACPI_IS_AML_TABLE);

    AcpiOsPrintf (" * Original Table Header:\n");
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
            AcpiOsPrintf (" **** 32-bit table (V1), no 64-bit math support");
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
        NewFilename = UtStringCacheCalloc (9);
        if (NewFilename)
        {
            strncat (NewFilename, Table->Signature, 4);
            strcat (NewFilename, ".aml");
        }
    }

    if (!NewFilename)
    {
        AcpiOsPrintf (" **** Could not generate AML output filename\n");
        return;
    }

    /* Open the ASL definition block */

    AcpiOsPrintf (
        "DefinitionBlock (\"%s\", \"%4.4s\", %hu, \"%.6s\", \"%.8s\", 0x%8.8X)\n",
        NewFilename, Table->Signature, Table->Revision,
        Table->OemId, Table->OemTableId, Table->OemRevision);
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
        return (AE_NOT_EXIST);
    }

    if (!AcpiGbl_DmOpt_Listing)
    {
        AdCreateTableHeader (Filename, Table);
    }

    AcpiDmDisassemble (NULL, AcpiGbl_ParseOpRoot, ACPI_UINT32_MAX);
    MpEmitMappingInfo ();

    if (AcpiGbl_DmOpt_Listing)
    {
        AcpiOsPrintf ("\n\nTable Header:\n");
        AcpiUtDebugDumpBuffer ((UINT8 *) Table, sizeof (ACPI_TABLE_HEADER),
            DB_BYTE_DISPLAY, ACPI_UINT32_MAX);

        AcpiOsPrintf ("Table Body (Length 0x%X)\n", Table->Length);
        AcpiUtDebugDumpBuffer (((UINT8 *) Table + sizeof (ACPI_TABLE_HEADER)),
            Table->Length, DB_BYTE_DISPLAY, ACPI_UINT32_MAX);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AdStoreTable
 *
 * PARAMETERS:  Table               - Table header
 *              TableIndex          - Where the table index is returned
 *
 * RETURN:      Status and table index.
 *
 * DESCRIPTION: Add an ACPI table to the global table list
 *
 ******************************************************************************/

static ACPI_STATUS
AdStoreTable (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  *TableIndex)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_DESC         *TableDesc;


    Status = AcpiTbGetNextTableDescriptor (TableIndex, &TableDesc);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Initialize added table */

    AcpiTbInitTableDescriptor (TableDesc, ACPI_PTR_TO_PHYSADDR (Table),
        ACPI_TABLE_ORIGIN_INTERNAL_VIRTUAL, Table);
    Status = AcpiTbValidateTable (TableDesc);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AdGetLocalTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the ACPI tables from either memory or a file
 *
 *****************************************************************************/

ACPI_STATUS
AdGetLocalTables (
    void)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       TableHeader;
    ACPI_TABLE_HEADER       *NewTable;
    UINT32                  TableIndex;


    /* Get the DSDT via table override */

    ACPI_MOVE_32_TO_32 (TableHeader.Signature, ACPI_SIG_DSDT);
    AcpiOsTableOverride (&TableHeader, &NewTable);
    if (!NewTable)
    {
        fprintf (stderr, "Could not obtain DSDT\n");
        return (AE_NO_ACPI_TABLES);
    }

    AdWriteTable (NewTable, NewTable->Length,
        ACPI_SIG_DSDT, NewTable->OemTableId);

    /* Store DSDT in the Table Manager */

    Status = AdStoreTable (NewTable, &TableIndex);
    if (ACPI_FAILURE (Status))
    {
        fprintf (stderr, "Could not store DSDT\n");
        return (AE_NO_ACPI_TABLES);
    }

    return (AE_OK);
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
        return (AE_NOT_EXIST);
    }

    /* Pass 1:  Parse everything except control method bodies */

    fprintf (stderr, "Pass 1 parse of [%4.4s]\n", (char *) Table->Signature);

    AmlLength = Table->Length - sizeof (ACPI_TABLE_HEADER);
    AmlStart = ((UINT8 *) Table + sizeof (ACPI_TABLE_HEADER));

    /* Create the root object */

    AcpiGbl_ParseOpRoot = AcpiPsCreateScopeOp (AmlStart);
    if (!AcpiGbl_ParseOpRoot)
    {
        return (AE_NO_MEMORY);
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
        return (Status);
    }

    /* If LoadTable is FALSE, we are parsing the last loaded table */

    TableIndex = AcpiGbl_RootTableList.CurrentTableCount - 1;

    /* Pass 2 */

    if (LoadTable)
    {
        Status = AdStoreTable (Table, &TableIndex);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        Status = AcpiTbAllocateOwnerId (TableIndex);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        if (OwnerId)
        {
            Status = AcpiTbGetOwnerId (TableIndex, OwnerId);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
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
        return (AE_OK);
    }

    /*
     * Pass 3: Parse control methods and link their parse trees
     * into the main parse tree
     */
    fprintf (stderr,
        "Parsing Deferred Opcodes (Methods/Buffers/Packages/Regions)\n");
    Status = AcpiDmParseDeferredOps (AcpiGbl_ParseOpRoot);
    fprintf (stderr, "\n");

    /* Process Resource Templates */

    AcpiDmFindResources (AcpiGbl_ParseOpRoot);

    fprintf (stderr, "Parsing completed\n");
    return (AE_OK);
}
