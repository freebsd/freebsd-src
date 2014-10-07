/******************************************************************************
 *
 * Module Name: dttable.c - handling for specific ACPI tables
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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
 * FUNCTION:    DtCompileCsrt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile CSRT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileCsrt (
    void                    **List)
{
    ACPI_STATUS             Status = AE_OK;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    UINT32                  DescriptorCount;
    UINT32                  GroupLength;


    /* Subtables (Resource Groups) */

    while (*PFieldList)
    {
        /* Resource group subtable */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoCsrt0,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Compute the number of resource descriptors */

        GroupLength =
            (ACPI_CAST_PTR (ACPI_CSRT_GROUP,
                Subtable->Buffer))->Length -
            (ACPI_CAST_PTR (ACPI_CSRT_GROUP,
                Subtable->Buffer))->SharedInfoLength -
            sizeof (ACPI_CSRT_GROUP);

        DescriptorCount = (GroupLength  /
            sizeof (ACPI_CSRT_DESCRIPTOR));

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        /* Shared info subtable (One per resource group) */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoCsrt1,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);

        /* Sub-Subtables (Resource Descriptors) */

        while (*PFieldList && DescriptorCount)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoCsrt2,
                        &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);
            DescriptorCount--;
        }

        DtPopSubtable ();
    }

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileDbg2
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile DBG2.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileDbg2 (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    UINT32                  SubtableCount;
    ACPI_DBG2_HEADER        *Dbg2Header;
    ACPI_DBG2_DEVICE        *DeviceInfo;
    UINT16                  CurrentOffset;
    UINT32                  i;


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoDbg2, &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /* Main table fields */

    Dbg2Header = ACPI_CAST_PTR (ACPI_DBG2_HEADER, Subtable->Buffer);
    Dbg2Header->InfoOffset = sizeof (ACPI_TABLE_HEADER) + ACPI_PTR_DIFF (
        ACPI_ADD_PTR (UINT8, Dbg2Header, sizeof (ACPI_DBG2_HEADER)), Dbg2Header);

    SubtableCount = Dbg2Header->InfoCount;
    DtPushSubtable (Subtable);

    /* Process all Device Information subtables (Count = InfoCount) */

    while (*PFieldList && SubtableCount)
    {
        /* Subtable: Debug Device Information */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoDbg2Device,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DeviceInfo = ACPI_CAST_PTR (ACPI_DBG2_DEVICE, Subtable->Buffer);
        CurrentOffset = (UINT16) sizeof (ACPI_DBG2_DEVICE);

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        ParentTable = DtPeekSubtable ();

        /* BaseAddressRegister GAS array (Required, size is RegisterCount) */

        DeviceInfo->BaseAddressOffset = CurrentOffset;
        for (i = 0; *PFieldList && (i < DeviceInfo->RegisterCount); i++)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoDbg2Addr,
                        &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            CurrentOffset += (UINT16) sizeof (ACPI_GENERIC_ADDRESS);
            DtInsertSubtable (ParentTable, Subtable);
        }

        /* AddressSize array (Required, size = RegisterCount) */

        DeviceInfo->AddressSizeOffset = CurrentOffset;
        for (i = 0; *PFieldList && (i < DeviceInfo->RegisterCount); i++)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoDbg2Size,
                        &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            CurrentOffset += (UINT16) sizeof (UINT32);
            DtInsertSubtable (ParentTable, Subtable);
        }

        /* NamespaceString device identifier (Required, size = NamePathLength) */

        DeviceInfo->NamepathOffset = CurrentOffset;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoDbg2Name,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Update the device info header */

        DeviceInfo->NamepathLength = (UINT16) Subtable->Length;
        CurrentOffset += (UINT16) DeviceInfo->NamepathLength;
        DtInsertSubtable (ParentTable, Subtable);

        /* OemData - Variable-length data (Optional, size = OemDataLength) */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoDbg2OemData,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Update the device info header (zeros if no OEM data present) */

        DeviceInfo->OemDataOffset = 0;
        DeviceInfo->OemDataLength = 0;

        /* Optional subtable (OemData) */

        if (Subtable && Subtable->Length)
        {
            DeviceInfo->OemDataOffset = CurrentOffset;
            DeviceInfo->OemDataLength = (UINT16) Subtable->Length;

            DtInsertSubtable (ParentTable, Subtable);
        }

        SubtableCount--;
        DtPopSubtable (); /* Get next Device Information subtable */
    }

    DtPopSubtable ();
    return (AE_OK);
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
    ACPI_DMAR_DEVICE_SCOPE  *DmarDeviceScope;
    UINT32                  DeviceScopeLength;
    UINT32                  PciPathLength;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoDmar, &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    DtPushSubtable (Subtable);

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

        case ACPI_DMAR_TYPE_ROOT_ATS:

            InfoTable = AcpiDmTableInfoDmar2;
            break;

        case ACPI_DMAR_TYPE_HARDWARE_AFFINITY:

            InfoTable = AcpiDmTableInfoDmar3;
            break;

        case ACPI_DMAR_TYPE_NAMESPACE:

            InfoTable = AcpiDmTableInfoDmar4;
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

        /*
         * Optional Device Scope subtables
         */
        if ((DmarHeader->Type == ACPI_DMAR_TYPE_HARDWARE_AFFINITY) ||
            (DmarHeader->Type == ACPI_DMAR_TYPE_NAMESPACE))
        {
            /* These types do not support device scopes */

            DtPopSubtable ();
            continue;
        }

        DtPushSubtable (Subtable);
        DeviceScopeLength = DmarHeader->Length - Subtable->Length -
            ParentTable->Length;
        while (DeviceScopeLength)
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

            DmarDeviceScope = ACPI_CAST_PTR (ACPI_DMAR_DEVICE_SCOPE, Subtable->Buffer);

            /* Optional PCI Paths */

            PciPathLength = DmarDeviceScope->Length - Subtable->Length;
            while (PciPathLength)
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
                PciPathLength -= Subtable->Length;
            }

            DtPopSubtable ();
            DeviceScopeLength -= DmarDeviceScope->Length;
        }

        DtPopSubtable ();
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
    }

    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    DtCompileGtdt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile GTDT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileGtdt (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_SUBTABLE_HEADER    *GtdtHeader;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  GtCount;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoGtdt,
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
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoGtdtHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        GtdtHeader = ACPI_CAST_PTR (ACPI_SUBTABLE_HEADER, Subtable->Buffer);

        switch (GtdtHeader->Type)
        {
        case ACPI_GTDT_TYPE_TIMER_BLOCK:

            InfoTable = AcpiDmTableInfoGtdt0;
            break;

        case ACPI_GTDT_TYPE_WATCHDOG:

            InfoTable = AcpiDmTableInfoGtdt1;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "GTDT");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);

        /*
         * Additional GT block subtable data
         */

        switch (GtdtHeader->Type)
        {
        case ACPI_GTDT_TYPE_TIMER_BLOCK:

            DtPushSubtable (Subtable);
            ParentTable = DtPeekSubtable ();

            GtCount = (ACPI_CAST_PTR (ACPI_GTDT_TIMER_BLOCK,
                Subtable->Buffer - sizeof(ACPI_GTDT_HEADER)))->TimerCount;
            while (GtCount)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoGtdt0a,
                            &Subtable, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }


                DtInsertSubtable (ParentTable, Subtable);
                GtCount--;
            }
            DtPopSubtable ();
            break;

        default:

            break;
        }

        DtPopSubtable ();
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileFpdt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile FPDT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileFpdt (
    void                    **List)
{
    ACPI_STATUS             Status;
    ACPI_FPDT_HEADER        *FpdtHeader;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;


    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoFpdtHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        FpdtHeader = ACPI_CAST_PTR (ACPI_FPDT_HEADER, Subtable->Buffer);

        switch (FpdtHeader->Type)
        {
        case ACPI_FPDT_TYPE_BOOT:

            InfoTable = AcpiDmTableInfoFpdt0;
            break;

        case ACPI_FPDT_TYPE_S3PERF:

            InfoTable = AcpiDmTableInfoFpdt1;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "FPDT");
            return (AE_ERROR);
            break;
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

    return (AE_OK);
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
 * FUNCTION:    DtCompileLpit
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile LPIT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileLpit (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_LPIT_HEADER        *LpitHeader;


    /* Note: Main table consists only of the standard ACPI table header */

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;

        /* LPIT Subtable header */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoLpitHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        LpitHeader = ACPI_CAST_PTR (ACPI_LPIT_HEADER, Subtable->Buffer);

        switch (LpitHeader->Type)
        {
        case ACPI_LPIT_TYPE_NATIVE_CSTATE:

            InfoTable = AcpiDmTableInfoLpit0;
            break;

        case ACPI_LPIT_TYPE_SIMPLE_IO:

            InfoTable = AcpiDmTableInfoLpit1;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "LPIT");
            return (AE_ERROR);
        }

        /* LPIT Subtable */

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

        case ACPI_MADT_TYPE_GENERIC_INTERRUPT:

            InfoTable = AcpiDmTableInfoMadt11;
            break;

        case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:

            InfoTable = AcpiDmTableInfoMadt12;
            break;

        case ACPI_MADT_TYPE_GENERIC_MSI_FRAME:

            InfoTable = AcpiDmTableInfoMadt13;
            break;

        case ACPI_MADT_TYPE_GENERIC_REDISTRIBUTOR:

            InfoTable = AcpiDmTableInfoMadt14;
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
 * FUNCTION:    DtCompileMpst
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile MPST.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileMpst (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    ACPI_MPST_CHANNEL       *MpstChannelInfo;
    ACPI_MPST_POWER_NODE    *MpstPowerNode;
    ACPI_MPST_DATA_HDR      *MpstDataHeader;
    UINT16                  SubtableCount;
    UINT32                  PowerStateCount;
    UINT32                  ComponentCount;


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst, &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    DtPushSubtable (Subtable);

    MpstChannelInfo = ACPI_CAST_PTR (ACPI_MPST_CHANNEL, Subtable->Buffer);
    SubtableCount = MpstChannelInfo->PowerNodeCount;

    while (*PFieldList && SubtableCount)
    {
        /* Subtable: Memory Power Node(s) */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst0,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        MpstPowerNode = ACPI_CAST_PTR (ACPI_MPST_POWER_NODE, Subtable->Buffer);
        PowerStateCount = MpstPowerNode->NumPowerStates;
        ComponentCount = MpstPowerNode->NumPhysicalComponents;

        ParentTable = DtPeekSubtable ();

        /* Sub-subtables - Memory Power State Structure(s) */

        while (*PFieldList && PowerStateCount)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst0A,
                        &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            PowerStateCount--;
        }

        /* Sub-subtables - Physical Component ID Structure(s) */

        while (*PFieldList && ComponentCount)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst0B,
                        &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            ComponentCount--;
        }

        SubtableCount--;
        DtPopSubtable ();
    }

    /* Subtable: Count of Memory Power State Characteristic structures */

    DtPopSubtable ();

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst1, &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    DtPushSubtable (Subtable);

    MpstDataHeader = ACPI_CAST_PTR (ACPI_MPST_DATA_HDR, Subtable->Buffer);
    SubtableCount = MpstDataHeader->CharacteristicsCount;

    ParentTable = DtPeekSubtable ();

    /* Subtable: Memory Power State Characteristics structure(s) */

    while (*PFieldList && SubtableCount)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst2,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);
        SubtableCount--;
    }

    DtPopSubtable ();
    return (AE_OK);
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
 * FUNCTION:    DtCompileMtmr
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile MTMR.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileMtmr (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoMtmr, AcpiDmTableInfoMtmr0);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompilePcct
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile PCCT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompilePcct (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_SUBTABLE_HEADER    *PcctHeader;
    ACPI_DMTABLE_INFO       *InfoTable;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoPcct,
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
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoPcctHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        PcctHeader = ACPI_CAST_PTR (ACPI_SUBTABLE_HEADER, Subtable->Buffer);

        switch (PcctHeader->Type)
        {
        case ACPI_PCCT_TYPE_GENERIC_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct0;
            break;

        case ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct1;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "PCCT");
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
 * FUNCTION:    DtCompilePmtt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile PMTT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompilePmtt (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_PMTT_HEADER        *PmttHeader;
    ACPI_PMTT_CONTROLLER    *PmttController;
    UINT16                  DomainCount;
    UINT8                   PrevType = ACPI_PMTT_TYPE_SOCKET;


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt, &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    DtPushSubtable (Subtable);

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmttHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        PmttHeader = ACPI_CAST_PTR (ACPI_PMTT_HEADER, Subtable->Buffer);
        while (PrevType >= PmttHeader->Type)
        {
            DtPopSubtable ();

            if (PrevType == ACPI_PMTT_TYPE_SOCKET)
            {
                break;
            }
            PrevType--;
        }
        PrevType = PmttHeader->Type;

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        switch (PmttHeader->Type)
        {
        case ACPI_PMTT_TYPE_SOCKET:

            /* Subtable: Socket Structure */

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt0,
                    &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);
            break;

        case ACPI_PMTT_TYPE_CONTROLLER:

            /* Subtable: Memory Controller Structure */

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt1,
                    &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);

            PmttController = ACPI_CAST_PTR (ACPI_PMTT_CONTROLLER,
                (Subtable->Buffer - sizeof (ACPI_PMTT_HEADER)));
            DomainCount = PmttController->DomainCount;

            while (DomainCount)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt1a,
                    &Subtable, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                DtInsertSubtable (ParentTable, Subtable);
                DomainCount--;
            }
            break;

        case ACPI_PMTT_TYPE_DIMM:

            /* Subtable: Physical Component Structure */

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt2,
                    &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "PMTT");
            return (AE_ERROR);
        }
    }

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
 * FUNCTION:    DtCompileS3pt
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile S3PT (Pointed to by FPDT)
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileS3pt (
    DT_FIELD                **PFieldList)
{
    ACPI_STATUS             Status;
    ACPI_S3PT_HEADER        *S3ptHeader;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    DT_FIELD                *SubtableStart;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoS3pt,
                &Gbl_RootTable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    DtPushSubtable (Gbl_RootTable);

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoS3ptHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        S3ptHeader = ACPI_CAST_PTR (ACPI_S3PT_HEADER, Subtable->Buffer);

        switch (S3ptHeader->Type)
        {
        case ACPI_S3PT_TYPE_RESUME:

            InfoTable = AcpiDmTableInfoS3pt0;
            break;

        case ACPI_S3PT_TYPE_SUSPEND:

            InfoTable = AcpiDmTableInfoS3pt1;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "S3PT");
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
 * FUNCTION:    DtCompileSlic
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile SLIC.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileSlic (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_SLIC_HEADER        *SlicHeader;
    ACPI_DMTABLE_INFO       *InfoTable;


    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoSlicHdr,
                    &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        SlicHeader = ACPI_CAST_PTR (ACPI_SLIC_HEADER, Subtable->Buffer);

        switch (SlicHeader->Type)
        {
        case ACPI_SLIC_TYPE_PUBLIC_KEY:

            InfoTable = AcpiDmTableInfoSlic0;
            break;

        case ACPI_SLIC_TYPE_WINDOWS_MARKER:

            InfoTable = AcpiDmTableInfoSlic1;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "SLIC");
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

    /* Compile each locality buffer */

    FieldList = *PFieldList;
    while (FieldList)
    {
        DtCompileBuffer (LocalityBuffer,
            FieldList->Value, FieldList, Localities);

        DtCreateSubtable (LocalityBuffer, Localities, &Subtable);
        DtInsertSubtable (ParentTable, Subtable);
        FieldList = FieldList->Next;
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

        case ACPI_SRAT_TYPE_GICC_AFFINITY:

            InfoTable = AcpiDmTableInfoSrat3;
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
 * FUNCTION:    DtGetGenericTableInfo
 *
 * PARAMETERS:  Name                - Generic type name
 *
 * RETURN:      Info entry
 *
 * DESCRIPTION: Obtain table info for a generic name entry
 *
 *****************************************************************************/

ACPI_DMTABLE_INFO *
DtGetGenericTableInfo (
    char                    *Name)
{
    ACPI_DMTABLE_INFO       *Info;
    UINT32                  i;


    if (!Name)
    {
        return (NULL);
    }

    /* Search info table for name match */

    for (i = 0; ; i++)
    {
        Info = AcpiDmTableInfoGeneric[i];
        if (Info->Opcode == ACPI_DMT_EXIT)
        {
            Info = NULL;
            break;
        }

        /* Use caseless compare for generic keywords */

        if (!AcpiUtStricmp (Name, Info->Name))
        {
            break;
        }
    }

    return (Info);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileUefi
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile UEFI.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileUefi (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    UINT16                  *DataOffset;


    /* Compile the predefined portion of the UEFI table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoUefi,
                &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    DataOffset = (UINT16 *) (Subtable->Buffer + 16);
    *DataOffset = sizeof (ACPI_TABLE_UEFI);

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /*
     * Compile the "generic" portion of the UEFI table. This
     * part of the table is not predefined and any of the generic
     * operators may be used.
     */

    DtCompileGeneric ((void **) PFieldList);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileVrtc
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile VRTC.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileVrtc (
    void                    **List)
{
    ACPI_STATUS             Status;


    Status = DtCompileTwoSubtables (List,
                 AcpiDmTableInfoVrtc, AcpiDmTableInfoVrtc0);
    return (Status);
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


/******************************************************************************
 *
 * FUNCTION:    DtCompileGeneric
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile generic unknown table.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileGeneric (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    ACPI_DMTABLE_INFO       *Info;


    ParentTable = DtPeekSubtable ();

    /*
     * Compile the "generic" portion of the table. This
     * part of the table is not predefined and any of the generic
     * operators may be used.
     */

    /* Find any and all labels in the entire generic portion */

    DtDetectAllLabels (*PFieldList);

    /* Now we can actually compile the parse tree */

    while (*PFieldList)
    {
        Info = DtGetGenericTableInfo ((*PFieldList)->Name);
        if (!Info)
        {
            sprintf (MsgBuffer, "Generic data type \"%s\" not found",
                (*PFieldList)->Name);
            DtNameError (ASL_ERROR, ASL_MSG_INVALID_FIELD_NAME,
                (*PFieldList), MsgBuffer);

            *PFieldList = (*PFieldList)->Next;
            continue;
        }

        Status = DtCompileTable (PFieldList, Info,
                    &Subtable, TRUE);
        if (ACPI_SUCCESS (Status))
        {
            DtInsertSubtable (ParentTable, Subtable);
        }
        else
        {
            *PFieldList = (*PFieldList)->Next;

            if (Status == AE_NOT_FOUND)
            {
                sprintf (MsgBuffer, "Generic data type \"%s\" not found",
                    (*PFieldList)->Name);
                DtNameError (ASL_ERROR, ASL_MSG_INVALID_FIELD_NAME,
                    (*PFieldList), MsgBuffer);
            }
        }
    }

    return (AE_OK);
}
