/******************************************************************************
 *
 * Module Name: dtcompile.c - Front-end for data table compiler
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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

#define __DTCOMPILE_C__
#define _DECLARE_DT_GLOBALS

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtcompile")

static char                 VersionString[9];


/* Local prototypes */

static ACPI_STATUS
DtInitialize (
    void);

static ACPI_STATUS
DtCompileDataTable (
    DT_FIELD                **Field);

static void
DtInsertCompilerIds (
    DT_FIELD                *FieldList);


/******************************************************************************
 *
 * FUNCTION:    DtDoCompile
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Main entry point for the data table compiler.
 *
 * Note: Assumes Gbl_Files[ASL_FILE_INPUT] is initialized and the file is
 *          open at seek offset zero.
 *
 *****************************************************************************/

ACPI_STATUS
DtDoCompile (
    void)
{
    ACPI_STATUS             Status;
    UINT8                   Event;
    DT_FIELD                *FieldList;


    /* Initialize globals */

    Status = DtInitialize ();
    if (ACPI_FAILURE (Status))
    {
        printf ("Error during compiler initialization, 0x%X\n", Status);
        return (Status);
    }

    /*
     * Scan the input file (file is already open) and
     * build the parse tree
     */
    Event = UtBeginEvent ("Scan and parse input file");
    FieldList = DtScanFile (Gbl_Files[ASL_FILE_INPUT].Handle);
    UtEndEvent (Event);

    /* Did the parse tree get successfully constructed? */

    if (!FieldList)
    {
        /* TBD: temporary error message. Msgs should come from function above */

        DtError (ASL_ERROR, ASL_MSG_SYNTAX, NULL,
            "Input file does not appear to be an ASL or data table source file");

        Status = AE_ERROR;
        goto CleanupAndExit;
    }

    Event = UtBeginEvent ("Compile parse tree");

    /*
     * Compile the parse tree
     */
    Status = DtCompileDataTable (&FieldList);
    UtEndEvent (Event);

    DtFreeFieldList ();

    if (ACPI_FAILURE (Status))
    {
        /* TBD: temporary error message. Msgs should come from function above */

        DtError (ASL_ERROR, ASL_MSG_SYNTAX, NULL,
            "Could not compile input file");

        goto CleanupAndExit;
    }

    /* Create/open the binary output file */

    Gbl_Files[ASL_FILE_AML_OUTPUT].Filename = NULL;
    Status = FlOpenAmlOutputFile (Gbl_OutputFilenamePrefix);
    if (ACPI_FAILURE (Status))
    {
        goto CleanupAndExit;
    }

    /* Write the binary, then the optional hex file */

    DtOutputBinary (Gbl_RootTable);
    LsDoHexOutput ();
    DtWriteTableToListing ();

CleanupAndExit:

    CmCleanupAndExit ();
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize data table compiler globals. Enables multiple
 *              compiles per invocation.
 *
 *****************************************************************************/

static ACPI_STATUS
DtInitialize (
    void)
{
    ACPI_STATUS             Status;


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

    Gbl_FieldList = NULL;
    Gbl_RootTable = NULL;
    Gbl_SubtableStack = NULL;

    sprintf (VersionString, "%X", (UINT32) ACPI_CA_VERSION);
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtInsertCompilerIds
 *
 * PARAMETERS:  FieldList           - Current field list pointer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert the IDs (Name, Version) of the current compiler into
 *              the original ACPI table header.
 *
 *****************************************************************************/

static void
DtInsertCompilerIds (
    DT_FIELD                *FieldList)
{
    DT_FIELD                *Next;
    UINT32                  i;


    /*
     * Don't insert current compiler ID if requested. Used for compiler
     * debug/validation only.
     */
    if (Gbl_UseOriginalCompilerId)
    {
        return;
    }

    /* Walk to the Compiler fields at the end of the header */

    Next = FieldList;
    for (i = 0; i < 7; i++)
    {
        Next = Next->Next;
    }

    Next->Value = ASL_CREATOR_ID;
    Next->Flags = DT_FIELD_NOT_ALLOCATED;

    Next = Next->Next;
    Next->Value = VersionString;
    Next->Flags = DT_FIELD_NOT_ALLOCATED;
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileDataTable
 *
 * PARAMETERS:  FieldList           - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Entry point to compile one data table
 *
 *****************************************************************************/

static ACPI_STATUS
DtCompileDataTable (
    DT_FIELD                **FieldList)
{
    ACPI_DMTABLE_DATA       *TableData;
    DT_SUBTABLE             *Subtable;
    char                    *Signature;
    ACPI_TABLE_HEADER       *AcpiTableHeader;
    ACPI_STATUS             Status;


    /* Verify that we at least have a table signature and save it */

    Signature = DtGetFieldValue (*FieldList, "Signature");
    if (!Signature)
    {
        sprintf (MsgBuffer, "Expected \"%s\"", "Signature");
        DtNameError (ASL_ERROR, ASL_MSG_INVALID_FIELD_NAME,
            *FieldList, MsgBuffer);
        return (AE_ERROR);
    }

    Gbl_Signature = UtLocalCalloc (ACPI_STRLEN (Signature) + 1);
    strcpy (Gbl_Signature, Signature);

    /*
     * Handle tables that don't use the common ACPI table header structure.
     * Currently, these are the FACS and RSDP. Also check for an OEMx table,
     * these tables have user-defined contents.
     */
    if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_FACS))
    {
        Status = DtCompileFacs (FieldList);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtSetTableLength ();
        return (Status);
    }
    else if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_RSDP))
    {
        Status = DtCompileRsdp (FieldList);
        return (Status);
    }
    else if (!ACPI_STRNCMP (Signature, "OEM", 3))
    {
        DtFatal (ASL_MSG_OEM_TABLE, *FieldList, Signature);
        return (AE_ERROR);
    }

    /* Validate the signature via the ACPI table list */

    TableData = AcpiDmGetTableData (Signature);
    if (!TableData)
    {
        DtFatal (ASL_MSG_UNKNOWN_TABLE, *FieldList, Signature);
        return (AE_ERROR);
    }

    /*
     * All other tables must use the common ACPI table header. Insert the
     * current iASL IDs (name, version), and compile the header now.
     */
    DtInsertCompilerIds (*FieldList);

    Status = DtCompileTable (FieldList, AcpiDmTableInfoHeader,
                &Gbl_RootTable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    DtPushSubtable (Gbl_RootTable);

    /* Dispatch to per-table compile */

    if (TableData->CmTableHandler)
    {
        /* Complex table, has a handler */

        Status = TableData->CmTableHandler ((void **) FieldList);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
    }
    else if (TableData->TableInfo)
    {
        /* Simple table, just walk the info table */

        Subtable = NULL;
        Status = DtCompileTable (FieldList, TableData->TableInfo,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (Gbl_RootTable, Subtable);
        DtPopSubtable ();
    }
    else
    {
        DtFatal (ASL_MSG_COMPILER_INTERNAL, *FieldList,
            "Missing table dispatch info");
        return (AE_ERROR);
    }

    /* Set the final table length and then the checksum */

    DtSetTableLength ();
    AcpiTableHeader = ACPI_CAST_PTR (
        ACPI_TABLE_HEADER, Gbl_RootTable->Buffer);
    DtSetTableChecksum (&AcpiTableHeader->Checksum);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileTable
 *
 * PARAMETERS:  Field               - Current field list pointer
 *              Info                - Info table for this ACPI table
 *              RetSubtable         - Compile result of table
 *              Required            - If this subtable must exist
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile a subtable
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileTable (
    DT_FIELD                **Field,
    ACPI_DMTABLE_INFO       *Info,
    DT_SUBTABLE             **RetSubtable,
    BOOLEAN                 Required)
{
    DT_FIELD                *LocalField;
    UINT32                  Length;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *InlineSubtable;
    UINT32                  FieldLength = 0;
    UINT8                   FieldType;
    UINT8                   *Buffer;
    UINT8                   *FlagBuffer = NULL;
    ACPI_STATUS             Status;


    if (!Field || !*Field)
    {
        return (AE_BAD_PARAMETER);
    }

    Length = DtGetSubtableLength (*Field, Info);
    Subtable = UtLocalCalloc (sizeof (DT_SUBTABLE));

    Subtable->Buffer = UtLocalCalloc (Length);
    Subtable->Length = Length;
    Subtable->TotalLength = Length;
    Buffer = Subtable->Buffer;

    LocalField = *Field;

    /*
     * Main loop walks the info table for this ACPI table or subtable
     */
    for (; Info->Name; Info++)
    {
        if (!LocalField)
        {
            sprintf (MsgBuffer, "Found NULL field - Field name \"%s\" needed",
                Info->Name);
            DtFatal (ASL_MSG_COMPILER_INTERNAL, NULL, MsgBuffer);
            Status = AE_BAD_DATA;
            goto Error;
        }

        /* Does input field name match what is expected? */

        if (ACPI_STRCMP (LocalField->Name, Info->Name))
        {
            /*
             * If Required = TRUE, the subtable must exist.
             * If Required = FALSE, the subtable is optional
             * (For example, AcpiDmTableInfoDmarScope in DMAR table is
             * optional)
             */
            if (Required)
            {
                sprintf (MsgBuffer, "Expected \"%s\"", Info->Name);
                DtNameError (ASL_ERROR, ASL_MSG_INVALID_FIELD_NAME,
                    LocalField, MsgBuffer);
            }
            else
            {
                Status = AE_NOT_FOUND;
                goto Error;
            }
        }

        FieldLength = DtGetFieldLength (LocalField, Info);
        FieldType = DtGetFieldType (Info);
        Gbl_InputFieldCount++;

        switch (FieldType)
        {
        case DT_FIELD_TYPE_FLAGS_INTEGER:
            /*
             * Start of the definition of a flags field.
             * This master flags integer starts at value zero, in preparation
             * to compile and insert the flag fields from the individual bits
             */
            LocalField = LocalField->Next;
            *Field = LocalField;

            FlagBuffer = Buffer;
            break;

        case DT_FIELD_TYPE_FLAG:

            /* Individual Flag field, can be multiple bits */

            if (FlagBuffer)
            {
                DtCompileFlag (FlagBuffer, LocalField, Info);
            }
            else
            {
                /* TBD - this is an internal error */
            }

            LocalField = LocalField->Next;
            *Field = LocalField;
            break;

        case DT_FIELD_TYPE_INLINE_SUBTABLE:
            /*
             * Recursion (one level max): compile GAS (Generic Address)
             * or Notify in-line subtable
             */
            LocalField = LocalField->Next;
            *Field = LocalField;

            if (Info->Opcode == ACPI_DMT_GAS)
            {
                Status = DtCompileTable (Field, AcpiDmTableInfoGas,
                    &InlineSubtable, TRUE);
            }
            else
            {
                Status = DtCompileTable (Field, AcpiDmTableInfoHestNotify,
                    &InlineSubtable, TRUE);
            }

            if (ACPI_FAILURE (Status))
            {
                goto Error;
            }

            DtSetSubtableLength (InlineSubtable);

            ACPI_MEMCPY (Buffer, InlineSubtable->Buffer, FieldLength);
            ACPI_FREE (InlineSubtable->Buffer);
            ACPI_FREE (InlineSubtable);
            LocalField = *Field;
            break;

        default:

            /* Normal case for most field types (Integer, String, etc.) */

            DtCompileOneField (Buffer, LocalField,
                FieldLength, FieldType, Info->Flags);

            DtWriteFieldToListing (Buffer, LocalField, FieldLength);
            LocalField = LocalField->Next;

            if (Info->Flags & DT_LENGTH)
            {
                /* Field is an Integer that will contain a subtable length */

                Subtable->LengthField = Buffer;
                Subtable->SizeOfLengthField = FieldLength;
            }

            break;
        }

        Buffer += FieldLength;
    }

    *Field = LocalField;
    *RetSubtable = Subtable;
    return (AE_OK);

Error:
    ACPI_FREE (Subtable->Buffer);
    ACPI_FREE (Subtable);
    return (Status);
}
