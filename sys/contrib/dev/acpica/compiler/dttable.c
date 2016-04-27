/******************************************************************************
 *
 * Module Name: dttable.c - handling for specific ACPI tables
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

/* Compile routines for the basic ACPI tables */

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dttable")


/******************************************************************************
 *
 * FUNCTION:    DtCompileRsdp
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile RSDP.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileRsdp (
    DT_FIELD                **PFieldList)
{
    DT_SUBTABLE             *Subtable;
    ACPI_TABLE_RSDP         *Rsdp;
    ACPI_RSDP_EXTENSION     *RsdpExtension;
    ACPI_STATUS             Status;


    /* Compile the "common" RSDP (ACPI 1.0) */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoRsdp1,
        &Gbl_RootTable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, Gbl_RootTable->Buffer);
    DtSetTableChecksum (&Rsdp->Checksum);

    if (Rsdp->Revision > 0)
    {
        /* Compile the "extended" part of the RSDP as a subtable */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoRsdp2,
            &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (Gbl_RootTable, Subtable);

        /* Set length and extended checksum for entire RSDP */

        RsdpExtension = ACPI_CAST_PTR (ACPI_RSDP_EXTENSION, Subtable->Buffer);
        RsdpExtension->Length = Gbl_RootTable->Length + Subtable->Length;
        DtSetTableChecksum (&RsdpExtension->ExtendedChecksum);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileFadt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile FADT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileFadt (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    ACPI_TABLE_HEADER       *Table;
    UINT8                   Revision;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt1,
        &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    Table = ACPI_CAST_PTR (ACPI_TABLE_HEADER, ParentTable->Buffer);
    Revision = Table->Revision;

    if (Revision == 2)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt2,
            &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);
    }
    else if (Revision >= 2)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt3,
            &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);

        if (Revision >= 5)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt5,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
        }

        if (Revision >= 6)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt6,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
        }
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileFacs
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile FACS.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileFacs (
    DT_FIELD                **PFieldList)
{
    DT_SUBTABLE             *Subtable;
    UINT8                   *ReservedBuffer;
    ACPI_STATUS             Status;
    UINT32                  ReservedSize;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoFacs,
        &Gbl_RootTable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Large FACS reserved area at the end of the table */

    ReservedSize = (UINT32) sizeof (((ACPI_TABLE_FACS *) NULL)->Reserved1);
    ReservedBuffer = UtLocalCalloc (ReservedSize);

    DtCreateSubtable (ReservedBuffer, ReservedSize, &Subtable);

    ACPI_FREE (ReservedBuffer);
    DtInsertSubtable (Gbl_RootTable, Subtable);
    return (AE_OK);
}
