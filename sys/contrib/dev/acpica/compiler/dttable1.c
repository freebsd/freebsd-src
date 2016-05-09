/******************************************************************************
 *
 * Module Name: dttable1.c - handling for specific ACPI tables
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

/* Compile all complex data tables, signatures starting with A-I */

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dttable1")


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

    ParentTable = DtPeekSubtable ();
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

        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);
        ParentTable = DtPeekSubtable ();

        /* Shared info subtable (One per resource group) */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoCsrt1,
            &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

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

            DtInsertSubtable (ParentTable, Subtable);

            DtPushSubtable (Subtable);
            ParentTable = DtPeekSubtable ();
            if (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoCsrt2a,
                    &Subtable, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                if (Subtable)
                {
                    DtInsertSubtable (ParentTable, Subtable);
                }
            }

            DtPopSubtable ();
            ParentTable = DtPeekSubtable ();
            DescriptorCount--;
        }

        DtPopSubtable ();
        ParentTable = DtPeekSubtable ();
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
 * FUNCTION:    DtCompileDrtm
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile DRTM.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileDrtm (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    UINT32                  Count;
    /* ACPI_TABLE_DRTM         *Drtm; */
    ACPI_DRTM_VTABLE_LIST   *DrtmVtl;
    ACPI_DRTM_RESOURCE_LIST *DrtmRl;
    /* ACPI_DRTM_DPS_ID        *DrtmDps; */


    ParentTable = DtPeekSubtable ();

    /* Compile DRTM header */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoDrtm,
        &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    DtInsertSubtable (ParentTable, Subtable);

    /*
     * Using ACPI_SUB_PTR, We needn't define a seperate structure. Care
     * should be taken to avoid accessing ACPI_TABLE_HADER fields.
     */
#if 0
    Drtm = ACPI_SUB_PTR (ACPI_TABLE_DRTM,
        Subtable->Buffer, sizeof (ACPI_TABLE_HEADER));
#endif
    /* Compile VTL */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoDrtm0,
        &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    DtInsertSubtable (ParentTable, Subtable);
    DrtmVtl = ACPI_CAST_PTR (ACPI_DRTM_VTABLE_LIST, Subtable->Buffer);

    DtPushSubtable (Subtable);
    ParentTable = DtPeekSubtable ();
    Count = 0;

    while (*PFieldList)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoDrtm0a,
            &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        if (!Subtable)
        {
            break;
        }
        DtInsertSubtable (ParentTable, Subtable);
        Count++;
    }

    DrtmVtl->ValidatedTableCount = Count;
    DtPopSubtable ();
    ParentTable = DtPeekSubtable ();

    /* Compile RL */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoDrtm1,
        &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    DtInsertSubtable (ParentTable, Subtable);
    DrtmRl = ACPI_CAST_PTR (ACPI_DRTM_RESOURCE_LIST, Subtable->Buffer);

    DtPushSubtable (Subtable);
    ParentTable = DtPeekSubtable ();
    Count = 0;

    while (*PFieldList)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoDrtm1a,
            &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        if (!Subtable)
        {
            break;
        }

        DtInsertSubtable (ParentTable, Subtable);
        Count++;
    }

    DrtmRl->ResourceCount = Count;
    DtPopSubtable ();
    ParentTable = DtPeekSubtable ();

    /* Compile DPS */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoDrtm2,
        &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    DtInsertSubtable (ParentTable, Subtable);
    /* DrtmDps = ACPI_CAST_PTR (ACPI_DRTM_DPS_ID, Subtable->Buffer);*/


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
 * FUNCTION:    DtCompileIort
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile IORT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileIort (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_TABLE_IORT         *Iort;
    ACPI_IORT_NODE          *IortNode;
    ACPI_IORT_ITS_GROUP     *IortItsGroup;
    ACPI_IORT_SMMU          *IortSmmu;
    UINT32                  NodeNumber;
    UINT32                  NodeLength;
    UINT32                  IdMappingNumber;
    UINT32                  ItsNumber;
    UINT32                  ContextIrptNumber;
    UINT32                  PmuIrptNumber;
    UINT32                  PaddingLength;


    ParentTable = DtPeekSubtable ();

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort,
        &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    DtInsertSubtable (ParentTable, Subtable);

    /*
     * Using ACPI_SUB_PTR, We needn't define a separate structure. Care
     * should be taken to avoid accessing ACPI_TABLE_HEADER fields.
     */
    Iort = ACPI_SUB_PTR (ACPI_TABLE_IORT,
        Subtable->Buffer, sizeof (ACPI_TABLE_HEADER));

    /*
     * OptionalPadding - Variable-length data
     * (Optional, size = OffsetToNodes - sizeof (ACPI_TABLE_IORT))
     * Optionally allows the generic data types to be used for filling
     * this field.
     */
    Iort->NodeOffset = sizeof (ACPI_TABLE_IORT);
    Status = DtCompileTable (PFieldList, AcpiDmTableInfoIortPad,
        &Subtable, TRUE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    if (Subtable)
    {
        DtInsertSubtable (ParentTable, Subtable);
        Iort->NodeOffset += Subtable->Length;
    }
    else
    {
        Status = DtCompileGeneric (ACPI_CAST_PTR (void *, PFieldList),
            AcpiDmTableInfoIortHdr[0].Name, &PaddingLength);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        Iort->NodeOffset += PaddingLength;
    }

    NodeNumber = 0;
    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoIortHdr,
            &Subtable, TRUE);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);
        IortNode = ACPI_CAST_PTR (ACPI_IORT_NODE, Subtable->Buffer);
        NodeLength = ACPI_OFFSET (ACPI_IORT_NODE, NodeData);

        DtPushSubtable (Subtable);
        ParentTable = DtPeekSubtable ();

        switch (IortNode->Type)
        {
        case ACPI_IORT_NODE_ITS_GROUP:

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort0,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            IortItsGroup = ACPI_CAST_PTR (ACPI_IORT_ITS_GROUP, Subtable->Buffer);
            NodeLength += Subtable->Length;

            ItsNumber = 0;
            while (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort0a,
                    &Subtable, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                if (!Subtable)
                {
                    break;
                }

                DtInsertSubtable (ParentTable, Subtable);
                NodeLength += Subtable->Length;
                ItsNumber++;
            }

            IortItsGroup->ItsCount = ItsNumber;
            break;

        case ACPI_IORT_NODE_NAMED_COMPONENT:

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort1,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            NodeLength += Subtable->Length;

            /*
             * Padding - Variable-length data
             * Optionally allows the offset of the ID mappings to be used
             * for filling this field.
             */
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort1a,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            if (Subtable)
            {
                DtInsertSubtable (ParentTable, Subtable);
                NodeLength += Subtable->Length;
            }
            else
            {
                if (NodeLength > IortNode->MappingOffset)
                {
                    return (AE_BAD_DATA);
                }

                if (NodeLength < IortNode->MappingOffset)
                {
                    Status = DtCompilePadding (
                        IortNode->MappingOffset - NodeLength,
                        &Subtable);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }

                    DtInsertSubtable (ParentTable, Subtable);
                    NodeLength = IortNode->MappingOffset;
                }
            }
            break;

        case ACPI_IORT_NODE_PCI_ROOT_COMPLEX:

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort2,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            NodeLength += Subtable->Length;
            break;

        case ACPI_IORT_NODE_SMMU:

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort3,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            IortSmmu = ACPI_CAST_PTR (ACPI_IORT_SMMU, Subtable->Buffer);
            NodeLength += Subtable->Length;

            /* Compile global interrupt array */

            IortSmmu->GlobalInterruptOffset = NodeLength;
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort3a,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            NodeLength += Subtable->Length;

            /* Compile context interrupt array */

            ContextIrptNumber = 0;
            IortSmmu->ContextInterruptOffset = NodeLength;
            while (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort3b,
                    &Subtable, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (!Subtable)
                {
                    break;
                }

                DtInsertSubtable (ParentTable, Subtable);
                NodeLength += Subtable->Length;
                ContextIrptNumber++;
            }

            IortSmmu->ContextInterruptCount = ContextIrptNumber;

            /* Compile PMU interrupt array */

            PmuIrptNumber = 0;
            IortSmmu->PmuInterruptOffset = NodeLength;
            while (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort3c,
                    &Subtable, TRUE);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (!Subtable)
                {
                    break;
                }

                DtInsertSubtable (ParentTable, Subtable);
                NodeLength += Subtable->Length;
                PmuIrptNumber++;
            }

            IortSmmu->PmuInterruptCount = PmuIrptNumber;
            break;

        case ACPI_IORT_NODE_SMMU_V3:

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIort4,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            NodeLength += Subtable->Length;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "IORT");
            return (AE_ERROR);
        }

        /* Compile Array of ID mappings */

        IortNode->MappingOffset = NodeLength;
        IdMappingNumber = 0;
        while (*PFieldList)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoIortMap,
                &Subtable, TRUE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            if (!Subtable)
            {
                break;
            }

            DtInsertSubtable (ParentTable, Subtable);
            NodeLength += sizeof (ACPI_IORT_ID_MAPPING);
            IdMappingNumber++;
        }

        IortNode->MappingCount = IdMappingNumber;

        /*
         * Node length can be determined by DT_LENGTH option
         * IortNode->Length = NodeLength;
         */
        DtPopSubtable ();
        ParentTable = DtPeekSubtable ();
        NodeNumber++;
    }

    Iort->NodeCount = NodeNumber;
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
                !strcmp ((*PFieldList)->Name, "Entry Type"))
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
