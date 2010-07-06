/******************************************************************************
 *
 * Module Name: dttable.c - handling for specific ACPI tables
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

#define __DTTABLE_C__

/* Compile all complex data tables */

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dttable")


/* TBD: merge these into dmtbinfo.c? */

static ACPI_DMTABLE_INFO           TableInfoAsfAddress[] =
{
    {ACPI_DMT_BUFFER,   0,               "Addresses", 0},
    {ACPI_DMT_EXIT,     0,               NULL, 0}
};

static ACPI_DMTABLE_INFO           TableInfoDmarPciPath[] =
{
    {ACPI_DMT_PCI_PATH, 0,               "PCI Path", 0},
    {ACPI_DMT_EXIT,     0,               NULL, 0}
};


/* TBD: move to acmacros.h */

#define ACPI_SUB_PTR(t, a, b) \
    ACPI_CAST_PTR (t, (ACPI_CAST_PTR (UINT8, (a)) - (ACPI_SIZE)(b)))


/* Local prototypes */

static ACPI_STATUS
DtCompileTwoSubtables (
    void                    **List,
    ACPI_DMTABLE_INFO       *TableInfo1,
    ACPI_DMTABLE_INFO       *TableInfo2);


/******************************************************************************
 *
 * FUNCTION:    DtCompileTwoSubtables
 *
 * PARAMETERS:  List                - Current field list pointer
 *              TableInfo1          - Info table 1
 *              TableInfo1          - Info table 2
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile tables with a header and one or more same subtables.
 *              Include CPEP, EINJ, ERST, MCFG, MSCT, WDAT
 *
 *****************************************************************************/

static ACPI_STATUS
DtCompileTwoSubtables (
    void                    **List,
    ACPI_DMTABLE_INFO       *TableInfo1,
    ACPI_DMTABLE_INFO       *TableInfo2)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;


    Status = DtCompileTable (PFieldList, TableInfo1, &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    while (*PFieldList)
    {
        Status = DtCompileTable (PFieldList, TableInfo2, &Subtable, FALSE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);
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
 * FUNCTION:    DtCompileAsf
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile ASF!.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileAsf (
    void                    **List)
{
    ACPI_ASF_INFO           *AsfTable;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMTABLE_INFO       *DataInfoTable = NULL;
    UINT32                  DataCount = 0;
    ACPI_STATUS             Status;
    UINT32                  i;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;


    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoAsfHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        AsfTable = ACPI_CAST_PTR (ACPI_ASF_INFO, Subtable->Buffer);

        switch (AsfTable->Header.Type & 0x7F) /* Mask off top bit */
        {
        case ACPI_ASF_TYPE_INFO:
            InfoTable = AcpiDmTableInfoAsf0;
            break;

        case ACPI_ASF_TYPE_ALERT:
            InfoTable = AcpiDmTableInfoAsf1;
            break;

        case ACPI_ASF_TYPE_CONTROL:
            InfoTable = AcpiDmTableInfoAsf2;
            break;

        case ACPI_ASF_TYPE_BOOT:
            InfoTable = AcpiDmTableInfoAsf3;
            break;

        case ACPI_ASF_TYPE_ADDRESS:
            InfoTable = AcpiDmTableInfoAsf4;
            break;

        default:
            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "ASF!");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);

        switch (AsfTable->Header.Type & 0x7F) /* Mask off top bit */
        {
        case ACPI_ASF_TYPE_INFO:
            DataInfoTable = NULL;
            break;

        case ACPI_ASF_TYPE_ALERT:
            DataInfoTable = AcpiDmTableInfoAsf1a;
            DataCount = ACPI_CAST_PTR (ACPI_ASF_ALERT,
                        ACPI_SUB_PTR (UINT8, Subtable->Buffer,
                            sizeof (ACPI_ASF_HEADER)))->Alerts;
            break;

        case ACPI_ASF_TYPE_CONTROL:
            DataInfoTable = AcpiDmTableInfoAsf2a;
            DataCount = ACPI_CAST_PTR (ACPI_ASF_REMOTE,
                        ACPI_SUB_PTR (UINT8, Subtable->Buffer,
                            sizeof (ACPI_ASF_HEADER)))->Controls;
            break;

        case ACPI_ASF_TYPE_BOOT:
            DataInfoTable = NULL;
            break;

        case ACPI_ASF_TYPE_ADDRESS:
            DataInfoTable = TableInfoAsfAddress;
            DataCount = ACPI_CAST_PTR (ACPI_ASF_ADDRESS,
                        ACPI_SUB_PTR (UINT8, Subtable->Buffer,
                            sizeof (ACPI_ASF_HEADER)))->Devices;
            break;

        default:
            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "ASF!");
            return (AE_ERROR);
        }

        if (DataInfoTable)
        {
            switch (AsfTable->Header.Type & 0x7F)
            {
            case ACPI_ASF_TYPE_ADDRESS:

                while (DataCount > 0)
                {
                    Status = DtCompileTable (PFieldList, DataInfoTable,
                                &Subtable, TRUE);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }

                    DtInsertSubtable (ParentTable, Subtable);
                    DataCount = DataCount - Subtable->Length;
                }
                break;

            default:

                for (i = 0; i < DataCount; i++)
                {
                    Status = DtCompileTable (PFieldList, DataInfoTable,
                                &Subtable, TRUE);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }

                    DtInsertSubtable (ParentTable, Subtable);
                }
                break;
            }
        }

        DtPopSubtable ();
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileCpep
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile CPEP.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileCpep (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoCpep, AcpiDmTableInfoCpep0);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileDmar
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile DMAR.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileDmar (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMAR_HEADER        *DmarHeader;
    UINT8                   *ReservedBuffer;
    UINT32                  ReservedSize;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoDmar, &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /* DMAR Reserved area */

    ReservedSize = (UINT32) sizeof (((ACPI_TABLE_DMAR *) NULL)->Reserved);
    ReservedBuffer = UtLocalCalloc (ReservedSize);

    DtCreateSubtable (ReservedBuffer, ReservedSize, &Subtable);

    ACPI_FREE (ReservedBuffer);
    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    while (*PFieldList)
    {
        /* DMAR Header */

        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoDmarHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        DmarHeader = ACPI_CAST_PTR (ACPI_DMAR_HEADER, Subtable->Buffer);

        switch (DmarHeader->Type)
        {
        case ACPI_DMAR_TYPE_HARDWARE_UNIT:
            InfoTable = AcpiDmTableInfoDmar0;
            break;
        case ACPI_DMAR_TYPE_RESERVED_MEMORY:
            InfoTable = AcpiDmTableInfoDmar1;
            break;
        case ACPI_DMAR_TYPE_ATSR:
            InfoTable = AcpiDmTableInfoDmar2;
            break;
        case ACPI_DMAR_HARDWARE_AFFINITY:
            InfoTable = AcpiDmTableInfoDmar3;
            break;
        default:
            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "DMAR");
            return (AE_ERROR);
        }

        /* DMAR Subtable */

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);

        /* Optional Device Scope subtables */

        while (*PFieldList)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoDmarScope,
                        &Subtable, FALSE);
            if (Status == AE_NOT_FOUND)
            {
                break;
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);
            DtPushSubtable (Subtable);

            /* Optional PCI Paths */

            while (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, TableInfoDmarPciPath,
                            &Subtable, FALSE);
                if (Status == AE_NOT_FOUND)
                {
                    DtPopSubtable ();
                    break;
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
            }
        }

        DtPopSubtable ();
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileEinj
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile EINJ.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileEinj (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoEinj, AcpiDmTableInfoEinj0);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileErst
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile ERST.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileErst (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoErst, AcpiDmTableInfoEinj0);
    return (Status);
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
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileHest
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile HEST.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileHest (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT16                  Type;
    UINT32                  BankCount;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoHest,
                &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    while (*PFieldList)
    {
        /* Get subtable type */

        SubtableStart = *PFieldList;
        DtCompileInteger ((UINT8 *) &Type, *PFieldList, 2, 0);

        switch (Type)
        {
        case ACPI_HEST_TYPE_IA32_CHECK:
            InfoTable = AcpiDmTableInfoHest0;
            break;

        case ACPI_HEST_TYPE_IA32_CORRECTED_CHECK:
            InfoTable = AcpiDmTableInfoHest1;
            break;

        case ACPI_HEST_TYPE_IA32_NMI:
            InfoTable = AcpiDmTableInfoHest2;
            break;

        case ACPI_HEST_TYPE_AER_ROOT_PORT:
            InfoTable = AcpiDmTableInfoHest6;
            break;

        case ACPI_HEST_TYPE_AER_ENDPOINT:
            InfoTable = AcpiDmTableInfoHest7;
            break;

        case ACPI_HEST_TYPE_AER_BRIDGE:
            InfoTable = AcpiDmTableInfoHest8;
            break;

        case ACPI_HEST_TYPE_GENERIC_ERROR:
            InfoTable = AcpiDmTableInfoHest9;
            break;

        default:
            /* Cannot continue on unknown type */

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "HEST");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);

        /*
         * Additional subtable data - IA32 Error Bank(s)
         */
        BankCount = 0;
        switch (Type)
        {
        case ACPI_HEST_TYPE_IA32_CHECK:
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_MACHINE_CHECK,
                            Subtable->Buffer))->NumHardwareBanks;
            break;

        case ACPI_HEST_TYPE_IA32_CORRECTED_CHECK:
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_CORRECTED,
                            Subtable->Buffer))->NumHardwareBanks;
            break;

        default:
            break;
        }

        while (BankCount)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoHestBank,
                        &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            BankCount--;
        }
    }

    return AE_OK;
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileIvrs
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile IVRS.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileIvrs (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_IVRS_HEADER        *IvrsHeader;
    UINT8                   EntryType;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoIvrs,
                &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoIvrsHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        IvrsHeader = ACPI_CAST_PTR (ACPI_IVRS_HEADER, Subtable->Buffer);

        switch (IvrsHeader->Type)
        {
        case ACPI_IVRS_TYPE_HARDWARE:
            InfoTable = AcpiDmTableInfoIvrs0;
            break;

        case ACPI_IVRS_TYPE_MEMORY1:
        case ACPI_IVRS_TYPE_MEMORY2:
        case ACPI_IVRS_TYPE_MEMORY3:
            InfoTable = AcpiDmTableInfoIvrs1;
            break;

        default:
            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "IVRS");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);

        if (IvrsHeader->Type == ACPI_IVRS_TYPE_HARDWARE)
        {
            while (*PFieldList &&
                    !ACPI_STRCMP ((*PFieldList)->Name, "Entry Type"))
            {
                SubtableStart = *PFieldList;
                DtCompileInteger (&EntryType, *PFieldList, 1, 0);

                switch (EntryType)
                {
                /* 4-byte device entries */

                case ACPI_IVRS_TYPE_PAD4:
                case ACPI_IVRS_TYPE_ALL:
                case ACPI_IVRS_TYPE_SELECT:
                case ACPI_IVRS_TYPE_START:
                case ACPI_IVRS_TYPE_END:

                    InfoTable = AcpiDmTableInfoIvrs4;
                    break;

                /* 8-byte entries, type A */

                case ACPI_IVRS_TYPE_ALIAS_SELECT:
                case ACPI_IVRS_TYPE_ALIAS_START:

                    InfoTable = AcpiDmTableInfoIvrs8a;
                    break;

                /* 8-byte entries, type B */

                case ACPI_IVRS_TYPE_PAD8:
                case ACPI_IVRS_TYPE_EXT_SELECT:
                case ACPI_IVRS_TYPE_EXT_START:

                    InfoTable = AcpiDmTableInfoIvrs8b;
                    break;

                /* 8-byte entries, type C */

                case ACPI_IVRS_TYPE_SPECIAL:

                    InfoTable = AcpiDmTableInfoIvrs8c;
                    break;

                default:
                    DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart,
                        "IVRS Device Entry");
                    return (AE_ERROR);
                }

                Status = DtCompileTable (PFieldList, InfoTable,
                            &Subtable, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                DtInsertSubtable (ParentTable, Subtable);
            }
        }

        DtPopSubtable ();
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileMadt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile MADT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileMadt (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_SUBTABLE_HEADER    *MadtHeader;
    ACPI_DMTABLE_INFO       *InfoTable;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoMadt,
                &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoMadtHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        MadtHeader = ACPI_CAST_PTR (ACPI_SUBTABLE_HEADER, Subtable->Buffer);

        switch (MadtHeader->Type)
        {
        case ACPI_MADT_TYPE_LOCAL_APIC:
            InfoTable = AcpiDmTableInfoMadt0;
            break;
        case ACPI_MADT_TYPE_IO_APIC:
            InfoTable = AcpiDmTableInfoMadt1;
            break;
        case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE:
            InfoTable = AcpiDmTableInfoMadt2;
            break;
        case ACPI_MADT_TYPE_NMI_SOURCE:
            InfoTable = AcpiDmTableInfoMadt3;
            break;
        case ACPI_MADT_TYPE_LOCAL_APIC_NMI:
            InfoTable = AcpiDmTableInfoMadt4;
            break;
        case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE:
            InfoTable = AcpiDmTableInfoMadt5;
            break;
        case ACPI_MADT_TYPE_IO_SAPIC:
            InfoTable = AcpiDmTableInfoMadt6;
            break;
        case ACPI_MADT_TYPE_LOCAL_SAPIC:
            InfoTable = AcpiDmTableInfoMadt7;
            break;
        case ACPI_MADT_TYPE_INTERRUPT_SOURCE:
            InfoTable = AcpiDmTableInfoMadt8;
            break;
        case ACPI_MADT_TYPE_LOCAL_X2APIC:
            InfoTable = AcpiDmTableInfoMadt9;
            break;
        case ACPI_MADT_TYPE_LOCAL_X2APIC_NMI:
            InfoTable = AcpiDmTableInfoMadt10;
            break;
        default:
            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "MADT");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPopSubtable ();
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileMcfg
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile MCFG.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileMcfg (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoMcfg, AcpiDmTableInfoMcfg0);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileMsct
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile MSCT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileMsct (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoMsct, AcpiDmTableInfoMsct0);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileRsdt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile RSDT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileRsdt (
    void                    **List)
{
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                *FieldList = *(DT_FIELD **) List;
    UINT32                  Address;


    ParentTable = DtPeekSubtable ();

    while (FieldList)
    {
        DtCompileInteger ((UINT8 *) &Address, FieldList, 4, DT_NON_ZERO);

        DtCreateSubtable ((UINT8 *) &Address, 4, &Subtable);
        DtInsertSubtable (ParentTable, Subtable);
        FieldList = FieldList->Next;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileSlit
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile SLIT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileSlit (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *FieldList;
    UINT32                  Localities;
    UINT8                   *LocalityBuffer;
    UINT32                  RemainingData;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoSlit,
                &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    Localities = *ACPI_CAST_PTR (UINT32, Subtable->Buffer);
    LocalityBuffer = UtLocalCalloc (Localities);

    FieldList = *PFieldList;
    while (FieldList)
    {
        /* Handle multiple-line buffer */

        RemainingData = Localities;
        while (RemainingData && FieldList)
        {
            RemainingData = DtCompileBuffer (
                LocalityBuffer + (Localities - RemainingData),
                FieldList->Value, FieldList, RemainingData);
            FieldList = FieldList->Next;
        }

        DtCreateSubtable (LocalityBuffer, Localities, &Subtable);
        DtInsertSubtable (ParentTable, Subtable);
    }

    ACPI_FREE (LocalityBuffer);
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileSrat
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile SRAT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileSrat (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_SUBTABLE_HEADER    *SratHeader;
    ACPI_DMTABLE_INFO       *InfoTable;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoSrat,
                &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoSratHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        SratHeader = ACPI_CAST_PTR (ACPI_SUBTABLE_HEADER, Subtable->Buffer);

        switch (SratHeader->Type)
        {
        case ACPI_SRAT_TYPE_CPU_AFFINITY:
            InfoTable = AcpiDmTableInfoSrat0;
            break;
        case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
            InfoTable = AcpiDmTableInfoSrat1;
            break;
        case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY:
            InfoTable = AcpiDmTableInfoSrat2;
            break;
        default:
            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "SRAT");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPopSubtable ();
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileWdat
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile WDAT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileWdat (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoWdat, AcpiDmTableInfoWdat0);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileXsdt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile XSDT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileXsdt (
    void                    **List)
{
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                *FieldList = *(DT_FIELD **) List;
    UINT64                  Address;

    ParentTable = DtPeekSubtable ();

    while (FieldList)
    {
        DtCompileInteger ((UINT8 *) &Address, FieldList, 8, DT_NON_ZERO);

        DtCreateSubtable ((UINT8 *) &Address, 8, &Subtable);
        DtInsertSubtable (ParentTable, Subtable);
        FieldList = FieldList->Next;
    }

    return (AE_OK);
}
