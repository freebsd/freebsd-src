/******************************************************************************
 *
 * Module Name: adisasm - Application-level disassembler routines
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
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/acapps.h>


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("adisasm")

/* Local prototypes */

static ACPI_STATUS
AdDoExternalFileList (
    char                    *Filename);

static ACPI_STATUS
AdDisassembleOneTable (
    ACPI_TABLE_HEADER       *Table,
    FILE                    *File,
    char                    *Filename,
    char                    *DisasmFilename);

static ACPI_STATUS
AdReparseOneTable (
    ACPI_TABLE_HEADER       *Table,
    FILE                    *File,
    ACPI_OWNER_ID           OwnerId);


ACPI_TABLE_DESC             LocalTables[1];
ACPI_PARSE_OBJECT           *AcpiGbl_ParseOpRoot;


/* Stubs for everything except ASL compiler */

#ifndef ACPI_ASL_COMPILER
BOOLEAN
AcpiDsIsResultUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    return (TRUE);
}

ACPI_STATUS
AcpiDsMethodError (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState)
{
    return (Status);
}
#endif


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
 * DESCRIPTION: Disassembler entry point. Disassemble an entire ACPI table.
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
    char                    *DisasmFilename = NULL;
    FILE                    *File = NULL;
    ACPI_TABLE_HEADER       *Table = NULL;
    ACPI_NEW_TABLE_DESC     *ListHead = NULL;


    /*
     * Input: AML code from either a file or via GetTables (memory or
     * registry)
     */
    if (Filename)
    {
        /* Get the list of all AML tables in the file */

        Status = AcGetAllTablesFromFile (Filename,
            ACPI_GET_ALL_TABLES, &ListHead);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not get ACPI tables from %s, %s\n",
                Filename, AcpiFormatException (Status));
            return (Status);
        }

        /* Process any user-specified files for external objects */

        Status = AdDoExternalFileList (Filename);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
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
            fprintf (stderr, "Could not open output file %s\n",
                DisasmFilename);
            Status = AE_ERROR;
            goto Cleanup;
        }

        AcpiOsRedirectOutput (File);
    }

    *OutFilename = DisasmFilename;

    /* Disassemble all AML tables within the file */

    while (ListHead)
    {
        Status = AdDisassembleOneTable (ListHead->Table,
            File, Filename, DisasmFilename);
        if (ACPI_FAILURE (Status))
        {
            break;
        }

        ListHead = ListHead->Next;
    }

Cleanup:

    if (Table &&
        !AcpiGbl_ForceAmlDisassembly &&
        !AcpiUtIsAmlTable (Table))
    {
        ACPI_FREE (Table);
    }

    if (File)
    {
        fclose (File);
        AcpiOsRedirectOutput (stdout);
    }

    AcpiPsDeleteParseTree (AcpiGbl_ParseOpRoot);
    AcpiGbl_ParseOpRoot = NULL;
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AdDisassembleOneTable
 *
 * PARAMETERS:  Table               - Raw AML table
 *              File                - Pointer for the input file
 *              Filename            - AML input filename
 *              DisasmFilename      - Output filename
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disassemble a single ACPI table. AML or data table.
 *
 *****************************************************************************/

static ACPI_STATUS
AdDisassembleOneTable (
    ACPI_TABLE_HEADER       *Table,
    FILE                    *File,
    char                    *Filename,
    char                    *DisasmFilename)
{
    ACPI_STATUS             Status;
    ACPI_OWNER_ID           OwnerId;


    /* ForceAmlDisassembly means to assume the table contains valid AML */

    if (!AcpiGbl_ForceAmlDisassembly && !AcpiUtIsAmlTable (Table))
    {
        AdDisassemblerHeader (Filename, ACPI_IS_DATA_TABLE);

        /* This is a "Data Table" (non-AML table) */

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

        return (AE_OK);
    }

    /*
     * This is an AML table (DSDT or SSDT).
     * Always parse the tables, only option is what to display
     */
    Status = AdParseTable (Table, &OwnerId, TRUE, FALSE);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not parse ACPI tables, %s\n",
            AcpiFormatException (Status));
        return (Status);
    }

    /* Debug output, namespace and parse tree */

    if (AslCompilerdebug && File)
    {
        AcpiOsPrintf ("/**** Before second load\n");

        NsSetupNamespaceListing (File);
        NsDisplayNamespace ();

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
        Status = AdReparseOneTable (Table, File, OwnerId);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
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

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AdReparseOneTable
 *
 * PARAMETERS:  Table               - Raw AML table
 *              File                - Pointer for the input file
 *              OwnerId             - ID for this table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reparse a table that has already been loaded. Used to
 *              integrate information about external control methods.
 *              These methods may have been previously parsed incorrectly.
 *
 *****************************************************************************/

static ACPI_STATUS
AdReparseOneTable (
    ACPI_TABLE_HEADER       *Table,
    FILE                    *File,
    ACPI_OWNER_ID           OwnerId)
{
    ACPI_STATUS             Status;


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
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* New namespace, add the external definitions first */

    AcpiDmAddExternalsToNamespace ();

    /* Parse the table again. No need to reload it, however */

    Status = AdParseTable (Table, NULL, FALSE, FALSE);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not parse ACPI tables, %s\n",
            AcpiFormatException (Status));
        return (Status);
    }

    /* Cross reference the namespace again */

    AcpiDmFinishNamespaceLoad (AcpiGbl_ParseOpRoot,
        AcpiGbl_RootNode, OwnerId);

    AcpiDmCrossReferenceNamespace (AcpiGbl_ParseOpRoot,
        AcpiGbl_RootNode, OwnerId);

    /* Debug output - namespace and parse tree */

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

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AdDoExternalFileList
 *
 * PARAMETERS:  Filename            - Input file for the table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Process all tables found in the -e external files list
 *
 *****************************************************************************/

static ACPI_STATUS
AdDoExternalFileList (
    char                    *Filename)
{
    ACPI_EXTERNAL_FILE      *ExternalFileList;
    char                    *ExternalFilename;
    ACPI_NEW_TABLE_DESC     *ExternalListHead = NULL;
    ACPI_STATUS             Status;
    ACPI_STATUS             GlobalStatus = AE_OK;
    ACPI_OWNER_ID           OwnerId;


    /*
     * External filenames are specified on the command line like this:
     * Example: iasl -e file1,file2,file3 -d xxx.aml
     */
    ExternalFileList = AcpiGbl_ExternalFileList;

    /* Process each external file */

    while (ExternalFileList)
    {
        ExternalFilename = ExternalFileList->Path;
        if (!strcmp (ExternalFilename, Filename))
        {
            /* Next external file */

            ExternalFileList = ExternalFileList->Next;
            continue;
        }

        AcpiOsPrintf ("External object resolution file %16s\n",
            ExternalFilename);

        Status = AcGetAllTablesFromFile (
            ExternalFilename, ACPI_GET_ONLY_AML_TABLES, &ExternalListHead);
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

        /* Load external tables for symbol resolution */

        while (ExternalListHead)
        {
            Status = AdParseTable (
                ExternalListHead->Table, &OwnerId, TRUE, TRUE);
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

            ExternalListHead = ExternalListHead->Next;
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
    return (AE_OK);
}
