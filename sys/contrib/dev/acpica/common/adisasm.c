/******************************************************************************
 *
 * Module Name: adisasm - Application-level disassembler routines
 *              $Revision: 58 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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


#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdebug.h"
#include "acdisasm.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acapps.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("adisasm")


ACPI_PARSE_OBJECT       *AcpiGbl_ParsedNamespaceRoot;


#ifndef _ACPI_ASL_COMPILER
BOOLEAN
AcpiDsIsResultUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    return TRUE;
}
#endif


ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsTerminateControlMethod (
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_OK);
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


#define FILE_SUFFIX_DISASSEMBLY     "dsl"
#define ACPI_TABLE_FILE_SUFFIX      ".dat"
char                        FilenameBuf[20];

/******************************************************************************
 *
 * FUNCTION:    AfGenerateFilename
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION: Build an output filename from an ACPI table ID string
 *
 ******************************************************************************/

char *
AdGenerateFilename (
    char                    *Prefix,
    char                    *TableId)
{
    ACPI_NATIVE_UINT         i;
    ACPI_NATIVE_UINT         j;


    for (i = 0; Prefix[i]; i++)
    {
        FilenameBuf[i] = Prefix[i];
    }

    FilenameBuf[i] = '_';
    i++;

    for (j = 0; j < 8 && (TableId[j] != ' ') && (TableId[j] != 0); i++, j++)
    {
        FilenameBuf[i] = TableId[j];
    }

    FilenameBuf[i] = 0;
    strcat (FilenameBuf, ACPI_TABLE_FILE_SUFFIX);
    return FilenameBuf;
}


/******************************************************************************
 *
 * FUNCTION:    AfWriteBuffer
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION: Open a file and write out a single buffer
 *
 ******************************************************************************/

ACPI_NATIVE_INT
AdWriteBuffer (
    char                *Filename,
    char                *Buffer,
    UINT32              Length)
{
    FILE                *fp;
    ACPI_NATIVE_INT     Actual;


    fp = fopen (Filename, "wb");
    if (!fp)
    {
        printf ("Couldn't open %s\n", Filename);
        return -1;
    }

    Actual = fwrite (Buffer, (size_t) Length, 1, fp);
    fclose (fp);
    return Actual;
}


/******************************************************************************
 *
 * FUNCTION:    AfWriteTable
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION: Dump the loaded tables to a file (or files)
 *
 ******************************************************************************/

void
AdWriteTable (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length,
    char                    *TableName,
    char                    *OemTableId)
{
    char                    *Filename;


    Filename = AdGenerateFilename (TableName, OemTableId);
    AdWriteBuffer (Filename, (char *) Table, Length);

    AcpiOsPrintf ("Table [%s] written to \"%s\"\n", TableName, Filename);
}


/*******************************************************************************
 *
 * FUNCTION:    AdInitialize
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: CA initialization
 *
 ******************************************************************************/

ACPI_STATUS
AdInitialize (
    void)
{
    ACPI_STATUS             Status;


    /* ACPI CA subsystem initialization */

    AcpiUtInitGlobals ();
    Status = AcpiUtMutexInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return Status;
    }

    Status = AcpiNsRootInitialize ();
    return Status;
}


/*******************************************************************************
 *
 * FUNCTION:    FlGenerateFilename
 *
 * PARAMETERS:  InputFilename       - Original ASL source filename
 *              Suffix              - New extension.
 *
 * RETURN:      New filename containing the original base + the new suffix
 *
 * DESCRIPTION: Generate a new filename from the ASL source filename and a new
 *              extension.  Used to create the *.LST, *.TXT, etc. files.
 *
 ******************************************************************************/

char *
FlGenerateFilename (
    char                    *InputFilename,
    char                    *Suffix)
{
    char                    *Position;
    char                    *NewFilename;


    /* Copy the original filename to a new buffer */

    NewFilename = ACPI_MEM_CALLOCATE (strlen (InputFilename) + strlen (Suffix));
    strcpy (NewFilename, InputFilename);

    /* Try to find the last dot in the filename */

    Position = strrchr (NewFilename, '.');
    if (Position)
    {
        /* Tack on the new suffix */

        Position++;
        *Position = 0;
        strcat (Position, Suffix);
    }
    else
    {
        /* No dot, add one and then the suffix */

        strcat (NewFilename, ".");
        strcat (NewFilename, Suffix);
    }

    return NewFilename;
}


/*******************************************************************************
 *
 * FUNCTION:    FlSplitInputPathname
 *
 * PARAMETERS:  InputFilename       - The user-specified ASL source file to be
 *                                    compiled
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Split the input path into a directory and filename part
 *              1) Directory part used to open include files
 *              2) Filename part used to generate output filenames
 *
 ******************************************************************************/

ACPI_STATUS
FlSplitInputPathname (
    char                    *InputPath,
    char                    **OutDirectoryPath,
    char                    **OutFilename)
{
    char                    *Substring;
    char                    *DirectoryPath;
    char                    *Filename;


    *OutDirectoryPath = NULL;
    *OutFilename = NULL;

    if (!InputPath)
    {
        return (AE_OK);
    }

    /* Get the path to the input filename's directory */

    DirectoryPath = strdup (InputPath);
    if (!DirectoryPath)
    {
        return (AE_NO_MEMORY);
    }

    Substring = strrchr (DirectoryPath, '\\');
    if (!Substring)
    {
        Substring = strrchr (DirectoryPath, '/');
        if (!Substring)
        {
            Substring = strrchr (DirectoryPath, ':');
        }
    }

    if (!Substring)
    {
        DirectoryPath[0] = 0;
        Filename = strdup (InputPath);
    }
    else
    {
        Filename = strdup (Substring + 1);
        *(Substring+1) = 0;
    }

    if (!Filename)
    {
        return (AE_NO_MEMORY);
    }

    *OutDirectoryPath = DirectoryPath;
    *OutFilename = Filename;

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AdAmlDisassemble
 *
 * PARAMETERS:  OutToFile       - TRUE if output should go to a file
 *              Filename        - AML input filename
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
    FILE                    *File = NULL;
    ACPI_TABLE_HEADER       *Table;


    /*
     * Input:  AML Code from either a file,
     *         or via GetTables (memory or registry)
     */
    if (Filename)
    {
        Status = AcpiDbGetTableFromFile (Filename, &Table);
        if (ACPI_FAILURE (Status))
        {
            return Status;
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

        /* Obtained the local tables, just disassmeble the DSDT */

        Table = AcpiGbl_DSDT;
        AcpiOsPrintf ("\nDisassembly of DSDT\n");
        Prefix = AdGenerateFilename ("dsdt", AcpiGbl_DSDT->OemTableId);
    }

    /*
     * Output:  ASL code.
     *          Redirect to a file if requested
     */
    if (OutToFile)
    {
        /* Create/Open a disassembly output file */

        DisasmFilename = FlGenerateFilename (Prefix, FILE_SUFFIX_DISASSEMBLY);
        if (!OutFilename)
        {
            fprintf (stderr, "Could not generate output filename\n");
        }

        File = fopen (DisasmFilename, "w+");
        if (!File)
        {
            fprintf (stderr, "Could not open output file\n");
        }

        AcpiOsRedirectOutput (File);
    }

    *OutFilename = DisasmFilename;

    /* Always parse the tables, only option is what to display */

    Status = AdParseTable (Table);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not parse ACPI tables, %s\n",
            AcpiFormatException (Status));
        goto Cleanup;
    }

    /* Optional displays */

    if (AcpiGbl_DbOpt_disasm)
    {
        AdDisplayTables (Filename, Table);
        fprintf (stderr, "Disassembly completed, written to \"%s\"\n", DisasmFilename);
    }

Cleanup:
    if (OutToFile)
    {
        fclose (File);
        AcpiOsRedirectOutput (stdout);
    }

    AcpiPsDeleteParseTree (AcpiGbl_ParsedNamespaceRoot);
    return AE_OK;
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
    time_t                  Timer;


    time (&Timer);

    AcpiOsPrintf ("/*\n * Intel ACPI Component Architecture\n");
    AcpiOsPrintf (" * AML Disassembler version %8.8X\n", ACPI_CA_VERSION);
    AcpiOsPrintf (" *\n * Disassembly of %s, %s */\n", Filename, ctime (&Timer));

    AcpiOsPrintf (
        "DefinitionBlock (\"%4.4s.aml\", \"%4.4s\", %hd, \"%.6s\", \"%.8s\", %d)\n",
        Table->Signature, Table->Signature, Table->Revision,
        Table->OemId, Table->OemTableId, Table->OemRevision);
}


/******************************************************************************
 *
 * FUNCTION:    AdDisplayTables
 *
 * PARAMETERS:  Filename            - Input file for the table
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


    if (!AcpiGbl_ParsedNamespaceRoot)
    {
        return AE_NOT_EXIST;
    }

    if (!AcpiGbl_DbOpt_verbose)
    {
        AdCreateTableHeader (Filename, Table);
    }

    AcpiDmDisassemble (NULL, AcpiGbl_ParsedNamespaceRoot, ACPI_UINT32_MAX);

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
 * PARAMETERS:  Op              - Root Op of the deferred opcode
 *              Aml             - Pointer to the raw AML
 *              AmlLength       - Length of the AML
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


    ACPI_FUNCTION_TRACE ("AdDeferredParse");


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
                    AmlLength, NULL, NULL, 1);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Parse the method */

    WalkState->ParseFlags &= ~ACPI_PARSE_DELETE_TREE;
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
        case AML_VAR_PACKAGE_OP:
            ExtraOp = Op->Common.Value.Arg;
            ExtraOp = ExtraOp->Common.Next;
            Op->Common.Value.Arg = ExtraOp->Common.Value.Arg;
            break;

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
 * PARAMETERS:  Root            - Root of the parse tree
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


    ACPI_FUNCTION_NAME ("AdParseDeferredOps");
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

            /* Nothing to do in these cases */

            break;

        default:
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unhandled deferred opcode [%s]\n",
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
 * PARAMETERS:
 *
 * RETURN:      None
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


    if (GetAllTables)
    {
        ACPI_STRNCPY (TableHeader.Signature, "RSDT", 4);
        AcpiOsTableOverride (&TableHeader, &NewTable);

#if ACPI_MACHINE_WIDTH != 64

        if (!ACPI_STRNCMP (NewTable->Signature, "RSDT", 4))
        {
            PointerSize = sizeof (UINT32);
        }
        else
#endif
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

        ACPI_STRNCPY (TableHeader.Signature, "FADT", 4);
        AcpiOsTableOverride (&TableHeader, &NewTable);
        if (NewTable)
        {
            AcpiGbl_FADT = (void *) NewTable;
            AdWriteTable (NewTable, NewTable->Length,
                "FADT", NewTable->OemTableId);
        }
        AcpiOsPrintf ("\n");

        /* Get the FACS */

        ACPI_STRNCPY (TableHeader.Signature, "FACS", 4);
        AcpiOsTableOverride (&TableHeader, &NewTable);
        if (NewTable)
        {
            AcpiGbl_FACS = (void *) NewTable;
            AdWriteTable (NewTable, AcpiGbl_FACS->Length,
                "FACS", AcpiGbl_FADT->Header.OemTableId);
        }
        AcpiOsPrintf ("\n");
    }

    /* Always get the DSDT */

    ACPI_STRNCPY (TableHeader.Signature, DSDT_SIG, 4);
    AcpiOsTableOverride (&TableHeader, &NewTable);
    if (NewTable)
    {
        Status = AE_OK;
        AcpiGbl_DSDT = NewTable;
        AdWriteTable (AcpiGbl_DSDT, AcpiGbl_DSDT->Length,
            "DSDT", AcpiGbl_DSDT->OemTableId);
    }
    else
    {
        fprintf (stderr, "Could not obtain DSDT\n");
        Status = AE_NO_ACPI_TABLES;
    }

    AcpiOsPrintf ("\n");

    /* Get all SSDTs */

    ACPI_STRNCPY (TableHeader.Signature, SSDT_SIG, 4);
    Status = AcpiOsTableOverride (&TableHeader, &NewTable);
    if (NewTable)
    {
        while (NewTable)
        {
            Status = AcpiOsTableOverride (&TableHeader, &NewTable);
        }
    }

#ifdef _HPET
    AfGetHpet ();
#endif

    return AE_OK;
}

/******************************************************************************
 *
 * FUNCTION:    AdParseTable
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse the DSDT.
 *
 *****************************************************************************/

ACPI_STATUS
AdParseTable (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_WALK_STATE         *WalkState;
    ACPI_TABLE_DESC         TableDesc;
    UINT8                   *AmlStart;
    UINT32                  AmlLength;


    if (!Table)
    {
        return AE_NOT_EXIST;
    }

    /* Pass 1:  Parse everything except control method bodies */

    fprintf (stderr, "Pass 1 parse of [%4.4s]\n", (char *) Table->Signature);

    AmlLength  = Table->Length  - sizeof (ACPI_TABLE_HEADER);
    AmlStart   = ((UINT8 *) Table + sizeof (ACPI_TABLE_HEADER));

    /* Create the root object */

    AcpiGbl_ParsedNamespaceRoot = AcpiPsCreateScopeOp ();
    if (!AcpiGbl_ParsedNamespaceRoot)
    {
        return AE_NO_MEMORY;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0,
                        AcpiGbl_ParsedNamespaceRoot, NULL, NULL);
    if (!WalkState)
    {
        return (AE_NO_MEMORY);
    }

    Status = AcpiDsInitAmlWalk (WalkState, AcpiGbl_ParsedNamespaceRoot,
                NULL, AmlStart, AmlLength, NULL, NULL, 1);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    WalkState->ParseFlags &= ~ACPI_PARSE_DELETE_TREE;

    Status = AcpiPsParseAml (WalkState);
    if (ACPI_FAILURE (Status))
    {
        return Status;
    }

    /* Pass 2 */

    TableDesc.AmlStart = AmlStart;
    TableDesc.AmlLength = AmlLength;
    fprintf (stderr, "Pass 2 parse of [%4.4s]\n", (char *) Table->Signature);

    Status = AcpiNsOneCompleteParse (2, &TableDesc);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Pass 3: Parse control methods and link their parse trees into the main parse tree */

    Status = AdParseDeferredOps (AcpiGbl_ParsedNamespaceRoot);

    fprintf (stderr, "Parsing completed\n");
    return AE_OK;
}


