/******************************************************************************
 *
 * Module Name: dttable2.c - handling for specific ACPI tables
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

/* Compile all complex data tables, signatures starting with L-Z */

#include <contrib/dev/acpica/compiler/aslcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dttable2")


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
            &Subtable);
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

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "LPIT");
            return (AE_ERROR);
        }

        /* LPIT Subtable */

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
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
        &Subtable);
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
            &Subtable);
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

        case ACPI_MADT_TYPE_GENERIC_TRANSLATOR:

            InfoTable = AcpiDmTableInfoMadt15;
            break;

        case ACPI_MADT_TYPE_MULTIPROC_WAKEUP:

            InfoTable = AcpiDmTableInfoMadt16;
            break;

        default:

            if (MadtHeader->Type >= ACPI_MADT_TYPE_OEM_RESERVED)
            {
                InfoTable = AcpiDmTableInfoMadt17;
            }
            else
            {
                DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "MADT");
                return (AE_ERROR);
            }

            break;
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
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
 * FUNCTION:    DtCompileMpam
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile MPAM.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileMpam (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *ParentTable;
    DT_SUBTABLE             *Subtable;
    DT_FIELD                *SubtableStart;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    ACPI_MPAM_MSC_NODE      *MpamMscNode;
    ACPI_MPAM_RESOURCE_NODE *MpamResourceNode;
    UINT32                  FuncDepsCount;
    UINT32                  RisLength;
    ACPI_DMTABLE_INFO       *InfoTable;

    ParentTable = DtPeekSubtable ();

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;

        /* Main MSC Node table */
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpam0,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        MpamMscNode = ACPI_CAST_PTR (ACPI_MPAM_MSC_NODE, Subtable->Buffer);

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        ParentTable = DtPeekSubtable ();

        /*
         * RIS(es) per MSC node have variable lengths depending on how many RISes there and
         * any how many functional dependencies per RIS. Calculate it in order
         * to properly set the overall MSC length.
         */
        RisLength = 0;

        /* Iterate over RIS subtables per MSC node */
        for (UINT32 ris = 0; ris < MpamMscNode->NumResouceNodes; ris++)
        {
            /* Compile RIS subtable */
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpam1,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            MpamResourceNode = ACPI_CAST_PTR (ACPI_MPAM_RESOURCE_NODE, Subtable->Buffer);
            DtInsertSubtable (ParentTable, Subtable);
            DtPushSubtable (Subtable);

            ParentTable = DtPeekSubtable ();

            switch (MpamResourceNode->LocatorType)
            {
                case ACPI_MPAM_LOCATION_TYPE_PROCESSOR_CACHE:
                    InfoTable = AcpiDmTableInfoMpam1A;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_MEMORY:
                    InfoTable = AcpiDmTableInfoMpam1B;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_SMMU:
                    InfoTable = AcpiDmTableInfoMpam1C;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_MEMORY_CACHE:
                    InfoTable = AcpiDmTableInfoMpam1D;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_ACPI_DEVICE:
                    InfoTable = AcpiDmTableInfoMpam1E;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_INTERCONNECT:
                    InfoTable = AcpiDmTableInfoMpam1F;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_UNKNOWN:
                    InfoTable = AcpiDmTableInfoMpam1G;
                default:
                    DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "Resource Locator Type");
                    return (AE_ERROR);
            }

            /* Compile Resource Locator Table */
            Status = DtCompileTable (PFieldList, InfoTable,
                &Subtable);

            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);

            /* Compile the number of functional dependencies per RIS */
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpam1Deps,
                &Subtable);

            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
            FuncDepsCount = *ACPI_CAST_PTR (UINT32, Subtable->Buffer);

            RisLength += sizeof(ACPI_MPAM_RESOURCE_NODE) +
                FuncDepsCount * sizeof(ACPI_MPAM_FUNC_DEPS);

            /* Iterate over functional dependencies per RIS */
            for (UINT32 funcDep = 0; funcDep < FuncDepsCount; funcDep++)
            {
                /* Compiler functional dependencies table */
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpam2,
                    &Subtable);

                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                DtInsertSubtable (ParentTable, Subtable);
            }

            DtPopSubtable ();
        }

        /* Check if the length of the MSC is correct and override with the correct length */
        if (MpamMscNode->Length != sizeof(ACPI_MPAM_MSC_NODE) + RisLength)
        {
            MpamMscNode->Length = (UINT16) (sizeof(ACPI_MPAM_MSC_NODE) + RisLength);
            DbgPrint (ASL_DEBUG_OUTPUT, "Overriding MSC->Length: %X\n", MpamMscNode->Length);
        }

        DtPopSubtable ();
    }

    return (AE_OK);
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

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst, &Subtable);
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
            &Subtable);
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
                &Subtable);
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
                &Subtable);
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

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoMpst1, &Subtable);
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
            &Subtable);
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
 * FUNCTION:    DtCompileNfit
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile NFIT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileNfit (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_NFIT_HEADER        *NfitHeader;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  Count;
    ACPI_NFIT_INTERLEAVE    *Interleave = NULL;
    ACPI_NFIT_FLUSH_ADDRESS *Hint = NULL;


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoNfit,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    DtPushSubtable (Subtable);

    /* Subtables */

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoNfitHdr,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        NfitHeader = ACPI_CAST_PTR (ACPI_NFIT_HEADER, Subtable->Buffer);

        switch (NfitHeader->Type)
        {
        case ACPI_NFIT_TYPE_SYSTEM_ADDRESS:

            InfoTable = AcpiDmTableInfoNfit0;
            break;

        case ACPI_NFIT_TYPE_MEMORY_MAP:

            InfoTable = AcpiDmTableInfoNfit1;
            break;

        case ACPI_NFIT_TYPE_INTERLEAVE:

            Interleave = ACPI_CAST_PTR (ACPI_NFIT_INTERLEAVE, Subtable->Buffer);
            InfoTable = AcpiDmTableInfoNfit2;
            break;

        case ACPI_NFIT_TYPE_SMBIOS:

            InfoTable = AcpiDmTableInfoNfit3;
            break;

        case ACPI_NFIT_TYPE_CONTROL_REGION:

            InfoTable = AcpiDmTableInfoNfit4;
            break;

        case ACPI_NFIT_TYPE_DATA_REGION:

            InfoTable = AcpiDmTableInfoNfit5;
            break;

        case ACPI_NFIT_TYPE_FLUSH_ADDRESS:

            Hint = ACPI_CAST_PTR (ACPI_NFIT_FLUSH_ADDRESS, Subtable->Buffer);
            InfoTable = AcpiDmTableInfoNfit6;
            break;

        case ACPI_NFIT_TYPE_CAPABILITIES:

            InfoTable = AcpiDmTableInfoNfit7;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "NFIT");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPopSubtable ();

        switch (NfitHeader->Type)
        {
        case ACPI_NFIT_TYPE_INTERLEAVE:

            Count = 0;
            DtPushSubtable (Subtable);
            while (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNfit2a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (!Subtable)
                {
                    DtPopSubtable ();
                    break;
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
                Count++;
            }

            Interleave->LineCount = Count;
            break;

        case ACPI_NFIT_TYPE_SMBIOS:

            if (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNfit3a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (Subtable)
                {
                    DtInsertSubtable (ParentTable, Subtable);
                }
            }
            break;

        case ACPI_NFIT_TYPE_FLUSH_ADDRESS:

            Count = 0;
            DtPushSubtable (Subtable);
            while (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNfit6a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (!Subtable)
                {
                    DtPopSubtable ();
                    break;
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
                Count++;
            }

            Hint->HintCount = (UINT16) Count;
            break;

        default:
            break;
        }
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileNhlt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile NHLT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileNhlt (
    void                    **List)
{
    ACPI_STATUS             Status;
    UINT32                  EndpointCount;
    UINT32                  MicrophoneCount;
    UINT32                  FormatsCount;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    UINT32                  CapabilitiesSize;
    UINT8                   ArrayType;
    UINT8                   ConfigType;
    UINT8                   DeviceInfoCount;
    UINT32                  i;
    UINT32                  j;
    ACPI_TABLE_NHLT_ENDPOINT_COUNT      *MainTable;
    ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A  *DevSpecific;
    ACPI_NHLT_VENDOR_MIC_COUNT          *MicCount;
    ACPI_NHLT_FORMATS_CONFIG            *FormatsConfig;
    ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_D  *ConfigSpecific;
    ACPI_NHLT_DEVICE_INFO_COUNT         *DeviceInfo;
    ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B  *Terminator;


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Get the Endpoint Descriptor count */

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    DtPushSubtable (Subtable);

    MainTable = ACPI_CAST_PTR (ACPI_TABLE_NHLT_ENDPOINT_COUNT, Subtable->Buffer);
    EndpointCount = MainTable->EndpointCount;

    /* Subtables */

    while (*PFieldList)
    {
        /* Variable number of Endpoint descriptors */

        for (i = 0; i < EndpointCount; i++)
        {
            /* Do the Endpoint Descriptor */

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt0,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);
            DtPushSubtable (Subtable);

            /* Do the Device Specific table */

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt5b,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);
            DtPushSubtable (Subtable);

            DevSpecific = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A, Subtable->Buffer);
            CapabilitiesSize = DevSpecific->CapabilitiesSize;

            ArrayType = 0;
            ConfigType = 0;

            switch (CapabilitiesSize)
            {
            case 0:
                break;

            case 1:

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt5c,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
                break;

            case 2:

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt5,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
                break;

            case 3:

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt5a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                ConfigSpecific = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_D, Subtable->Buffer);
                ArrayType = ConfigSpecific->ArrayType;
                ConfigType = ConfigSpecific->ConfigType;
                break;

            case 7:

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt5,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt6b,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                ConfigSpecific = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_D, Subtable->Buffer);
                ArrayType = ConfigSpecific->ArrayType;
                ConfigType = ConfigSpecific->ConfigType;
                break;

            default:

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt5a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                ConfigSpecific = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_D, Subtable->Buffer);
                ArrayType = ConfigSpecific->ArrayType;
                ConfigType = ConfigSpecific->ConfigType;
                break;

            } /* switch (CapabilitiesSize) */

            if (CapabilitiesSize >= 3)
            {
                /* Check for a vendor-defined mic array */

                if (ConfigType == ACPI_NHLT_CONFIG_TYPE_MIC_ARRAY)
                {
                    if ((ArrayType & ACPI_NHLT_ARRAY_TYPE_MASK) == ACPI_NHLT_VENDOR_DEFINED)
                    {
                        /* Get the microphone count */

                        Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt6a,
                            &Subtable);
                        if (ACPI_FAILURE (Status))
                        {
                            return (Status);
                        }

                        MicCount = ACPI_CAST_PTR (ACPI_NHLT_VENDOR_MIC_COUNT, Subtable->Buffer);
                        MicrophoneCount = MicCount->MicrophoneCount;

                        ParentTable = DtPeekSubtable ();
                        DtInsertSubtable (ParentTable, Subtable);

                        /* Variable number of microphones */

                        for (j = 0; j < MicrophoneCount; j++)
                        {
                            Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt6,
                                &Subtable);
                            if (ACPI_FAILURE (Status))
                            {
                                return (Status);
                            }

                            ParentTable = DtPeekSubtable ();
                            DtInsertSubtable (ParentTable, Subtable);
                        }

                        /* Do the MIC_SNR_SENSITIVITY_EXTENSION, if present */

                        if (ArrayType & ACPI_NHLT_ARRAY_TYPE_EXT_MASK)
                        {
                            Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt9,
                                &Subtable);
                            if (ACPI_FAILURE (Status))
                            {
                                return (Status);
                            }

                            ParentTable = DtPeekSubtable ();
                            DtInsertSubtable (ParentTable, Subtable);
                        }
                    }
                }
            }

            /* Get the formats count */

            DtPopSubtable ();
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt4,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);

            FormatsConfig = ACPI_CAST_PTR (ACPI_NHLT_FORMATS_CONFIG, Subtable->Buffer);
            FormatsCount = FormatsConfig->FormatsCount;

            /* Variable number of wave_format_extensible structs */

            for (j = 0; j < FormatsCount; j++)
            {
                /* Do the main wave_format_extensible structure */

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt3,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
                DtPushSubtable (Subtable);

                /* Do the capabilities list */

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt3a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                DtPopSubtable ();
                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

            } /* for (j = 0; j < FormatsCount; j++) */

            /*
             * If we are not done with the current Endpoint yet, then there must be
             * some non documented structure(s) yet to be processed. First, get
             * the count of such structure(s).
             */
            if (*PFieldList && !(strcmp ((const char *) (*PFieldList)->Name, "Device Info struct count")))
            {
                /* Get the count of non documented structures */

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt7,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                DeviceInfo = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_INFO_COUNT, Subtable->Buffer);
                DeviceInfoCount = DeviceInfo->StructureCount;

                for (j = 0; j < DeviceInfoCount; j++)
                {
                    /*
                     * Compile the following Device Info fields:
                     *  1) Device ID
                     *  2) Device Instance ID
                     *  3) Device Port ID
                     */
                    Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt7a,
                        &Subtable);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }

                    ParentTable = DtPeekSubtable ();
                    DtInsertSubtable (ParentTable, Subtable);
                } /* for (j = 0; j < LinuxSpecificCount; j++) */

                /* Undocumented data at the end of endpoint */
                if (*PFieldList && !(strcmp ((const char *) (*PFieldList)->Name, "Bytes")))
                {
                    Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt7b,
                        &Subtable);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }

                    ParentTable = DtPeekSubtable ();
                    DtInsertSubtable (ParentTable, Subtable);
                }
            }

            DtPopSubtable ();

        } /* for (i = 0; i < EndpointCount; i++) */

        /*
         * All Endpoint Descriptors are completed.
         * Do the table terminator specific config (not in NHLT spec, optional)
         */
        if (*PFieldList && !(strcmp ((const char *) (*PFieldList)->Name, "Capabilities Size")))
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt5b,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);

            Terminator = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B, Subtable->Buffer);

            if (Terminator->CapabilitiesSize)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoNhlt3a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
            }
        }

        return (AE_OK);
    }

    return (AE_OK);
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


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoPcct,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /* Subtables */

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoPcctHdr,
            &Subtable);
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

        case ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2:

            InfoTable = AcpiDmTableInfoPcct2;
            break;

        case ACPI_PCCT_TYPE_EXT_PCC_MASTER_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct3;
            break;

        case ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct4;
            break;

        case ACPI_PCCT_TYPE_HW_REG_COMM_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct5;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "PCCT");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
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
 * FUNCTION:    DtCompilePdtt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile PDTT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompilePdtt (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    ACPI_TABLE_PDTT         *PdttHeader;
    UINT32                  Count = 0;


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoPdtt, &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    PdttHeader = ACPI_CAST_PTR (ACPI_TABLE_PDTT, ParentTable->Buffer);
    PdttHeader->ArrayOffset = sizeof (ACPI_TABLE_PDTT);

    /* There is only one type of subtable at this time, no need to decode */

    while (*PFieldList)
    {
        /* List of subchannel IDs, each 2 bytes */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoPdtt0,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);
        Count++;
    }

    PdttHeader->TriggerCount = (UINT8) Count;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompilePhat
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile Phat.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompilePhat (
    void                    **List)
{
    ACPI_STATUS             Status = AE_OK;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    ACPI_PHAT_HEADER        *PhatHeader;
    ACPI_DMTABLE_INFO       *Info;
    ACPI_PHAT_VERSION_DATA  *VersionData;
    UINT32                  DeviceDataLength;
    UINT32                  RecordCount;
    DT_FIELD                *DataOffsetField;
    DT_FIELD                *DevicePathField;
    UINT32                  TableOffset = 0;
    UINT32                  DataOffsetValue;
    UINT32                  i;


    /* The table consists of subtables */

    while (*PFieldList)
    {
        /* Compile the common subtable header */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoPhatHdr, &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        TableOffset += Subtable->Length;
        DbgPrint (ASL_DEBUG_OUTPUT, "0 Subtable->Length: %X\n", Subtable->Length);

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        PhatHeader = ACPI_CAST_PTR (ACPI_PHAT_HEADER, Subtable->Buffer);

        switch (PhatHeader->Type)
        {
        case ACPI_PHAT_TYPE_FW_VERSION_DATA:

            /* Compile the middle portion of the Firmware Version Data */

            Info = AcpiDmTableInfoPhat0;
            PhatHeader->Length = sizeof (ACPI_PHAT_VERSION_DATA);
            DataOffsetField = NULL;
            break;

        case ACPI_PHAT_TYPE_FW_HEALTH_DATA:

            DbgPrint (ASL_DEBUG_OUTPUT, "1 Offset: %X, Name: \"%s\" Length: %X\n",
                (*PFieldList)->TableOffset, (*PFieldList)->Name, Subtable->Length);

            DataOffsetField = *PFieldList;

            /* Walk the field list to get to the "Device-specific data Offset" field */

            TableOffset = sizeof (ACPI_PHAT_HEALTH_DATA);
            for (i = 0; i < 3; i++)
            {
                DataOffsetField = DataOffsetField->Next;
                DbgPrint (ASL_DEBUG_OUTPUT, "2 Offset: %X, Name: \"%s\" Length: %X Value: %s:\n",
                    TableOffset, DataOffsetField->Name, DataOffsetField->StringLength, DataOffsetField->Value);
            }

            /* Convert DataOffsetField->Value (a char * string) to an integer value */

            sscanf (DataOffsetField->Value, "%X", &DataOffsetValue);

            /*
             * Get the next field (Device Path):
             * DataOffsetField points to "Device-Specific Offset", next field is
             * "Device Path".
             */
            DevicePathField = DataOffsetField->Next;

            /* Compute the size of the input ASCII string as a unicode string (*2 + 2) */

            DevicePathField->StringLength = (strlen ((const char *) DevicePathField->Value) * 2) + 2;
            TableOffset += DevicePathField->StringLength;

            DbgPrint (ASL_DEBUG_OUTPUT, "3 Offset: %X, Length: %X devicepathLength: %X\n",
                TableOffset, Subtable->Length, DevicePathField->StringLength);

            /* Set the DataOffsetField to the current TableOffset */
            /* Must set the DataOffsetField here (not later) */

            if (DataOffsetValue != 0)
            {
                snprintf (DataOffsetField->Value, Subtable->Length, "%X", TableOffset);
            }

            DbgPrint (ASL_DEBUG_OUTPUT, "4 Offset: %X, Length: %X\n", TableOffset, Subtable->Length);

            DbgPrint (ASL_DEBUG_OUTPUT, "5 TableOffset: %X, DataOffsetField->StringLength: "
                "%X DevicePathField Length: %X DevicePathField->Value: %s, DataOffsetField->Value: %s DataOffsetField->ByteOffset %X\n",
                TableOffset, DataOffsetField->StringLength, DevicePathField->StringLength,
                DevicePathField->Value, DataOffsetField->Value, DataOffsetField->ByteOffset);

            /* Compile the middle portion of the Health Data Record */

            Info = AcpiDmTableInfoPhat1;
            PhatHeader->Length = sizeof (ACPI_PHAT_HEALTH_DATA);
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, *PFieldList, "PHAT");
            return (AE_ERROR);
        }

        /* Compile either the Version Data or the Health Data */

        Status = DtCompileTable (PFieldList, Info, &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DbgPrint (ASL_DEBUG_OUTPUT, "6 Offset: %X, Name: \"%s\" SubtableLength: %X\n",
            TableOffset /* - StartTableOffset*/, (*PFieldList)->Name, Subtable->Length);

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);

        switch (PhatHeader->Type)
        {
        case ACPI_PHAT_TYPE_FW_VERSION_DATA:

            VersionData = ACPI_CAST_PTR (ACPI_PHAT_VERSION_DATA,
                (Subtable->Buffer - sizeof (ACPI_PHAT_HEADER)));
            RecordCount = VersionData->ElementCount;

            /* Compile all of the Version Elements */

            while (RecordCount)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoPhat0a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                TableOffset += Subtable->Length;
                RecordCount--;
                PhatHeader->Length += sizeof (ACPI_PHAT_VERSION_ELEMENT);
            }

            DtPopSubtable ();
            break;

        case ACPI_PHAT_TYPE_FW_HEALTH_DATA:

            /* Compile the Device Path */

            DeviceDataLength = Subtable->Length;
            TableOffset += Subtable->Length;

            DbgPrint (ASL_DEBUG_OUTPUT, "7 Device Path Length: %X FieldName: \"%s\" FieldLength: "
                "%s FieldValue: %s SubtableLength: %X TableOffset: %X\n", DeviceDataLength,
                (*PFieldList)->Name, DataOffsetField->Value, (*PFieldList)->Value,
                Subtable->Length, TableOffset);

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPhat1a, &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);

            /* *PFieldList will be null if previous field was at the end-of-ParseTree (EOF) */

            if (!*PFieldList)
            {
                DbgPrint (ASL_DEBUG_OUTPUT, "8 Exit on end-of-ParseTree\n");
                return (AE_OK);
            }

            DbgPrint (ASL_DEBUG_OUTPUT, "9 Device Data Length: %X FieldName: \"%s"
                " TableOffset: %X FieldLength: %X Field Value: %s SubtableLength: %X\n",
                DeviceDataLength, (*PFieldList)->Name, TableOffset,
                (*PFieldList)->StringLength, (*PFieldList)->Value, Subtable->Length);

            PhatHeader->Length += (UINT16) Subtable->Length;

            /* Convert DataOffsetField->Value (a hex char * string) to an integer value */

            sscanf (DataOffsetField->Value, "%X", &DataOffsetValue);

            DbgPrint (ASL_DEBUG_OUTPUT, "10 Device-Specific Offset: %X Table Offset: %X\n",
                DataOffsetValue, TableOffset);
            if (DataOffsetValue != 0)
            {
                /* Compile Device-Specific Data - only if the Data Offset is non-zero */

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoPhat1b, &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                DbgPrint (ASL_DEBUG_OUTPUT, "11 Subtable: %p Table Offset: %X\n",
                    Subtable, TableOffset);
                if (Subtable)
                {
                    DbgPrint (ASL_DEBUG_OUTPUT, "12 Device Specific Offset: "
                        "%X FieldName \"%s\" SubtableLength %X\n",
                        DeviceDataLength, DataOffsetField->Name, Subtable->Length);

                    DeviceDataLength += Subtable->Length;

                    ParentTable = DtPeekSubtable ();
                    DtInsertSubtable (ParentTable, Subtable);

                    PhatHeader->Length += (UINT16) Subtable->Length;
                }
            }

            DtPopSubtable ();

            DbgPrint (ASL_DEBUG_OUTPUT, "13 FieldName: \"%s\" FieldLength: %X Field Value: %s\n",
                DataOffsetField->Name, DataOffsetField->StringLength, DataOffsetField->Value);
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, *PFieldList, "PHAT");
            return (AE_ERROR);
        }
    }

    return (Status);
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
    UINT16                  Type;


    /* Main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt, &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    DtPushSubtable (Subtable);

    /* Subtables */

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        DtCompileInteger ((UINT8 *) &Type, *PFieldList, 2, 0);

        switch (Type)
        {
        case ACPI_PMTT_TYPE_SOCKET:

            /* Subtable: Socket Structure */

            DbgPrint (ASL_DEBUG_OUTPUT, "Compile PMTT_TYPE_SOCKET (0)\n");

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt0,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            break;

        case ACPI_PMTT_TYPE_CONTROLLER:

            /* Subtable: Memory Controller Structure */

            DbgPrint (ASL_DEBUG_OUTPUT, "Compile PMTT_TYPE_CONTROLLER (1)\n");

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt1,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            break;

        case ACPI_PMTT_TYPE_DIMM:

            /* Subtable: Physical Component (DIMM) Structure */

            DbgPrint (ASL_DEBUG_OUTPUT, "Compile PMTT_TYPE_DIMM (2)\n");
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmtt2,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            break;

        case ACPI_PMTT_TYPE_VENDOR:

            /* Subtable: Vendor-specific Structure */

            DbgPrint (ASL_DEBUG_OUTPUT, "Compile PMTT_TYPE_VENDOR(FF)\n");
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPmttVendor,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "PMTT");
            return (AE_ERROR);
        }

        DtInsertSubtable (ParentTable, Subtable);
    }

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompilePptt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile PPTT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompilePptt (
    void                    **List)
{
    ACPI_STATUS             Status;
    ACPI_SUBTABLE_HEADER    *PpttHeader;
    ACPI_PPTT_PROCESSOR     *PpttProcessor = NULL;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_TABLE_HEADER       *PpttAcpiHeader;


    ParentTable = DtPeekSubtable ();
    while (*PFieldList)
    {
        SubtableStart = *PFieldList;

        /* Compile PPTT subtable header */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoPpttHdr,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        DtInsertSubtable (ParentTable, Subtable);
        PpttHeader = ACPI_CAST_PTR (ACPI_SUBTABLE_HEADER, Subtable->Buffer);
        PpttHeader->Length = (UINT8)(Subtable->Length);

        switch (PpttHeader->Type)
        {
        case ACPI_PPTT_TYPE_PROCESSOR:

            InfoTable = AcpiDmTableInfoPptt0;
            break;

        case ACPI_PPTT_TYPE_CACHE:

            InfoTable = AcpiDmTableInfoPptt1;
            break;

        case ACPI_PPTT_TYPE_ID:

            InfoTable = AcpiDmTableInfoPptt2;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "PPTT");
            return (AE_ERROR);
        }

        /* Compile PPTT subtable body */

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        DtInsertSubtable (ParentTable, Subtable);
        PpttHeader->Length += (UINT8)(Subtable->Length);

        /* Compile PPTT subtable additional */

        switch (PpttHeader->Type)
        {
        case ACPI_PPTT_TYPE_PROCESSOR:

            PpttProcessor = ACPI_SUB_PTR (ACPI_PPTT_PROCESSOR,
                Subtable->Buffer, sizeof (ACPI_SUBTABLE_HEADER));
            if (PpttProcessor)
            {
                /* Compile initiator proximity domain list */

                PpttProcessor->NumberOfPrivResources = 0;
                while (*PFieldList)
                {
                    Status = DtCompileTable (PFieldList,
                        AcpiDmTableInfoPptt0a, &Subtable);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }
                    if (!Subtable)
                    {
                        break;
                    }

                    DtInsertSubtable (ParentTable, Subtable);
                    PpttHeader->Length += (UINT8)(Subtable->Length);
                    PpttProcessor->NumberOfPrivResources++;
                }
            }
            break;

        case ACPI_PPTT_TYPE_CACHE:

            PpttAcpiHeader = ACPI_CAST_PTR (ACPI_TABLE_HEADER,
                AslGbl_RootTable->Buffer);
            if (PpttAcpiHeader->Revision < 3)
            {
                break;
            }
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPptt1a,
                &Subtable);
            DtInsertSubtable (ParentTable, Subtable);
            PpttHeader->Length += (UINT8)(Subtable->Length);
            break;

        default:

            break;
        }
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompilePrmt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile PRMT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompilePrmt (
    void                    **List)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_PRMT_HEADER  *PrmtHeader;
    ACPI_PRMT_MODULE_INFO   *PrmtModuleInfo;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    UINT32                  i, j;

    ParentTable = DtPeekSubtable ();

    /* Compile PRMT subtable header */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoPrmtHdr,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    DtInsertSubtable (ParentTable, Subtable);
    PrmtHeader = ACPI_CAST_PTR (ACPI_TABLE_PRMT_HEADER, Subtable->Buffer);

    for (i = 0; i < PrmtHeader->ModuleInfoCount; i++)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoPrmtModule,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        DtInsertSubtable (ParentTable, Subtable);
        PrmtModuleInfo = ACPI_CAST_PTR (ACPI_PRMT_MODULE_INFO, Subtable->Buffer);

        for (j = 0; j < PrmtModuleInfo->HandlerInfoCount; j++)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoPrmtHandler,
                &Subtable);
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
 * FUNCTION:    DtCompileRgrt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile RGRT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileRgrt (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;


    /* Compile the main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoRgrt,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /* Compile the "Subtable" -- actually just the binary (PNG) image */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoRgrt0,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    DtInsertSubtable (ParentTable, Subtable);
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileRhct
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile RHCT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileRhct (
    void                    **List)
{
    ACPI_STATUS             Status;
    ACPI_RHCT_NODE_HEADER   *RhctHeader;
    ACPI_RHCT_HART_INFO     *RhctHartInfo = NULL;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;


    /* Compile the main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoRhct,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    while (*PFieldList)
    {
        SubtableStart = *PFieldList;

        /* Compile RHCT subtable header */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoRhctNodeHdr,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        DtInsertSubtable (ParentTable, Subtable);
        RhctHeader = ACPI_CAST_PTR (ACPI_RHCT_NODE_HEADER, Subtable->Buffer);
        RhctHeader->Length = (UINT16)(Subtable->Length);

        switch (RhctHeader->Type)
        {
        case ACPI_RHCT_NODE_TYPE_ISA_STRING:

            InfoTable = AcpiDmTableInfoRhctIsa1;
            break;

        case ACPI_RHCT_NODE_TYPE_HART_INFO:

            InfoTable = AcpiDmTableInfoRhctHartInfo1;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "RHCT");
            return (AE_ERROR);
        }

        /* Compile RHCT subtable body */

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        DtInsertSubtable (ParentTable, Subtable);
        RhctHeader->Length += (UINT16)(Subtable->Length);

        /* Compile RHCT subtable additionals */

        switch (RhctHeader->Type)
        {
        case ACPI_RHCT_NODE_TYPE_HART_INFO:

            RhctHartInfo = ACPI_SUB_PTR (ACPI_RHCT_HART_INFO,
                Subtable->Buffer, sizeof (ACPI_RHCT_NODE_HEADER));
            if (RhctHartInfo)
            {

                RhctHartInfo->NumOffsets = 0;
                while (*PFieldList)
                {
                    Status = DtCompileTable (PFieldList,
                        AcpiDmTableInfoRhctHartInfo2, &Subtable);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }
                    if (!Subtable)
                    {
                        break;
                    }

                    DtInsertSubtable (ParentTable, Subtable);
                    RhctHeader->Length += (UINT16)(Subtable->Length);
                    RhctHartInfo->NumOffsets++;
                }
            }
            break;

        default:

            break;
        }
    }

    return (AE_OK);
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
    ACPI_FPDT_HEADER        *S3ptHeader;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    DT_FIELD                *SubtableStart;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoS3pt,
        &AslGbl_RootTable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    DtPushSubtable (AslGbl_RootTable);

    while (*PFieldList)
    {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoS3ptHdr,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        S3ptHeader = ACPI_CAST_PTR (ACPI_FPDT_HEADER, Subtable->Buffer);

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

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
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
 * FUNCTION:    DtCompileSdev
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile SDEV.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileSdev (
    void                    **List)
{
    ACPI_STATUS                 Status;
    ACPI_SDEV_HEADER            *SdevHeader;
    ACPI_SDEV_HEADER            *SecureComponentHeader;
    DT_SUBTABLE                 *Subtable;
    DT_SUBTABLE                 *ParentTable;
    ACPI_DMTABLE_INFO           *InfoTable;
    ACPI_DMTABLE_INFO           *SecureComponentInfoTable = NULL;
    DT_FIELD                    **PFieldList = (DT_FIELD **) List;
    DT_FIELD                    *SubtableStart;
    ACPI_SDEV_PCIE              *Pcie = NULL;
    ACPI_SDEV_NAMESPACE         *Namesp = NULL;
    UINT32                      EntryCount;
    ACPI_SDEV_SECURE_COMPONENT  *SecureComponent = NULL;
    UINT16                      ComponentLength = 0;


    /* Subtables */

    while (*PFieldList)
    {
        /* Compile common SDEV subtable header */

        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoSdevHdr,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        SdevHeader = ACPI_CAST_PTR (ACPI_SDEV_HEADER, Subtable->Buffer);
        SdevHeader->Length = (UINT8)(sizeof (ACPI_SDEV_HEADER));

        switch (SdevHeader->Type)
        {
        case ACPI_SDEV_TYPE_NAMESPACE_DEVICE:

            InfoTable = AcpiDmTableInfoSdev0;
            Namesp = ACPI_CAST_PTR (ACPI_SDEV_NAMESPACE, Subtable->Buffer);
            SecureComponent = ACPI_CAST_PTR (ACPI_SDEV_SECURE_COMPONENT,
                ACPI_ADD_PTR (UINT8, Subtable->Buffer, sizeof(ACPI_SDEV_NAMESPACE)));
            break;

        case ACPI_SDEV_TYPE_PCIE_ENDPOINT_DEVICE:

            InfoTable = AcpiDmTableInfoSdev1;
            Pcie = ACPI_CAST_PTR (ACPI_SDEV_PCIE, Subtable->Buffer);
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "SDEV");
            return (AE_ERROR);
        }

        /* Compile SDEV subtable body */

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);

        /* Optional data fields are appended to the main subtable body */

        switch (SdevHeader->Type)
        {
        case ACPI_SDEV_TYPE_NAMESPACE_DEVICE:

            /*
             * Device Id Offset will be be calculated differently depending on
             * the presence of secure access components.
             */
            Namesp->DeviceIdOffset = 0;
            ComponentLength = 0;

            /* If the secure access component exists, get the structures */

            if (SdevHeader->Flags & ACPI_SDEV_SECURE_COMPONENTS_PRESENT)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoSdev0b,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                Namesp->DeviceIdOffset += sizeof (ACPI_SDEV_SECURE_COMPONENT);

                /* Compile a secure access component header */

                Status = DtCompileTable (PFieldList, AcpiDmTableInfoSdevSecCompHdr,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                /* Compile the secure access component */

                SecureComponentHeader = ACPI_CAST_PTR (ACPI_SDEV_HEADER, Subtable->Buffer);
                switch (SecureComponentHeader->Type)
                {
                case ACPI_SDEV_TYPE_ID_COMPONENT:

                    SecureComponentInfoTable = AcpiDmTableInfoSdevSecCompId;
                    Namesp->DeviceIdOffset += sizeof (ACPI_SDEV_ID_COMPONENT);
                    ComponentLength = sizeof (ACPI_SDEV_ID_COMPONENT);
                    break;

                case ACPI_SDEV_TYPE_MEM_COMPONENT:

                    SecureComponentInfoTable = AcpiDmTableInfoSdevSecCompMem;
                    Namesp->DeviceIdOffset += sizeof (ACPI_SDEV_MEM_COMPONENT);
                    ComponentLength = sizeof (ACPI_SDEV_MEM_COMPONENT);
                    break;

                default:

                    /* Any other secure component types are undefined */

                    return (AE_ERROR);
                }

                Status = DtCompileTable (PFieldList, SecureComponentInfoTable,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);

                SecureComponent->SecureComponentOffset =
                    sizeof (ACPI_SDEV_NAMESPACE) + sizeof (ACPI_SDEV_SECURE_COMPONENT);
                SecureComponent->SecureComponentLength = ComponentLength;


                /*
                 * Add the secure component to the subtable to be added for the
                 * the namespace subtable's length
                 */
                ComponentLength += sizeof (ACPI_SDEV_SECURE_COMPONENT);
            }

            /* Append DeviceId namespace string */

            Status = DtCompileTable (PFieldList, AcpiDmTableInfoSdev0a,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            if (!Subtable)
            {
                break;
            }

            ParentTable = DtPeekSubtable ();
            DtInsertSubtable (ParentTable, Subtable);

            Namesp->DeviceIdOffset += sizeof (ACPI_SDEV_NAMESPACE);

            Namesp->DeviceIdLength = (UINT16) Subtable->Length;

            /* Append Vendor data */

            Namesp->VendorDataLength = 0;
            Namesp->VendorDataOffset = 0;

            if (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoSdev1b,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (Subtable)
                {
                    ParentTable = DtPeekSubtable ();
                    DtInsertSubtable (ParentTable, Subtable);

                    Namesp->VendorDataOffset =
                        Namesp->DeviceIdOffset + Namesp->DeviceIdLength;
                    Namesp->VendorDataLength =
                        (UINT16) Subtable->Length;

                    /* Final size of entire namespace structure */

                    SdevHeader->Length = (UINT16)(sizeof(ACPI_SDEV_NAMESPACE) +
                        Subtable->Length + Namesp->DeviceIdLength) + ComponentLength;
                }
            }

            break;

        case ACPI_SDEV_TYPE_PCIE_ENDPOINT_DEVICE:

            /* Append the PCIe path info first */

            EntryCount = 0;
            while (*PFieldList && !strcmp ((*PFieldList)->Name, "Device"))
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoSdev1a,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (!Subtable)
                {
                    DtPopSubtable ();
                    break;
                }

                ParentTable = DtPeekSubtable ();
                DtInsertSubtable (ParentTable, Subtable);
                EntryCount++;
            }

            /* Path offset will point immediately after the main subtable */

            Pcie->PathOffset = sizeof (ACPI_SDEV_PCIE);
            Pcie->PathLength = (UINT16)
                (EntryCount * sizeof (ACPI_SDEV_PCIE_PATH));

            /* Append the Vendor Data last */

            Pcie->VendorDataLength = 0;
            Pcie->VendorDataOffset = 0;

            if (*PFieldList)
            {
                Status = DtCompileTable (PFieldList, AcpiDmTableInfoSdev1b,
                    &Subtable);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                if (Subtable)
                {
                    ParentTable = DtPeekSubtable ();
                    DtInsertSubtable (ParentTable, Subtable);

                    Pcie->VendorDataOffset =
                        Pcie->PathOffset + Pcie->PathLength;
                    Pcie->VendorDataLength = (UINT16)
                        Subtable->Length;
                }
            }

            SdevHeader->Length =
                sizeof (ACPI_SDEV_PCIE) +
                Pcie->PathLength + Pcie->VendorDataLength;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "SDEV");
            return (AE_ERROR);
        }

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


    while (*PFieldList)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoSlic,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);
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
    DT_FIELD                *EndOfFieldList = NULL;
    UINT32                  Localities;
    UINT32                  LocalityListLength;
    UINT8                   *LocalityBuffer;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoSlit,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    Localities = *ACPI_CAST_PTR (UINT32, Subtable->Buffer);
    LocalityBuffer = UtLocalCalloc (Localities);
    LocalityListLength = 0;

    /* Compile each locality buffer */

    FieldList = *PFieldList;
    while (FieldList)
    {
        DtCompileBuffer (LocalityBuffer,
            FieldList->Value, FieldList, Localities);

        LocalityListLength++;
        DtCreateSubtable (LocalityBuffer, Localities, &Subtable);
        DtInsertSubtable (ParentTable, Subtable);
        EndOfFieldList = FieldList;
        FieldList = FieldList->Next;
    }

    if (LocalityListLength != Localities)
    {
        sprintf(AslGbl_MsgBuffer,
            "Found %u entries, must match LocalityCount: %u",
            LocalityListLength, Localities);
        DtError (ASL_ERROR, ASL_MSG_ENTRY_LIST, EndOfFieldList, AslGbl_MsgBuffer);
        ACPI_FREE (LocalityBuffer);
        return (AE_LIMIT);
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
        &Subtable);
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
            &Subtable);
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

        case ACPI_SRAT_TYPE_GIC_ITS_AFFINITY:

            InfoTable = AcpiDmTableInfoSrat4;
            break;

        case ACPI_SRAT_TYPE_GENERIC_AFFINITY:

            InfoTable = AcpiDmTableInfoSrat5;
            break;

        case ACPI_SRAT_TYPE_GENERIC_PORT_AFFINITY:

            InfoTable = AcpiDmTableInfoSrat6;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "SRAT");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
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
 * FUNCTION:    DtCompileStao
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile STAO.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileStao (
    void                    **List)
{
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_STATUS             Status;


    /* Compile the main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoStao,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /* Compile each ASCII namestring as a subtable */

    while (*PFieldList)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoStaoStr,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileSvkl
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile SVKL.
 *
 * NOTES: SVKL is essentially a flat table, with a small main table and
 *          a variable number of a single type of subtable.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileSvkl (
    void                    **List)
{
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_STATUS             Status;


    /* Compile the main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoSvkl,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /* Compile each subtable */

    while (*PFieldList)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoSvkl0,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileTcpa
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile TCPA.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileTcpa (
    void                    **List)
{
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_SUBTABLE             *Subtable;
    ACPI_TABLE_TCPA_HDR     *TcpaHeader;
    DT_SUBTABLE             *ParentTable;
    ACPI_STATUS             Status;


    /* Compile the main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoTcpaHdr,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    /*
     * Examine the PlatformClass field to determine the table type.
     * Either a client or server table. Only one.
     */
    TcpaHeader = ACPI_CAST_PTR (ACPI_TABLE_TCPA_HDR, ParentTable->Buffer);

    switch (TcpaHeader->PlatformClass)
    {
    case ACPI_TCPA_CLIENT_TABLE:

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoTcpaClient,
            &Subtable);
        break;

    case ACPI_TCPA_SERVER_TABLE:

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoTcpaServer,
            &Subtable);
        break;

    default:

        AcpiOsPrintf ("\n**** Unknown TCPA Platform Class 0x%X\n",
            TcpaHeader->PlatformClass);
        Status = AE_ERROR;
        break;
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileTpm2Rev3
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile TPM2 revision 3
 *
 *****************************************************************************/
static ACPI_STATUS
DtCompileTpm2Rev3 (
    void                    **List)
{
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_SUBTABLE             *Subtable;
    ACPI_TABLE_TPM23        *Tpm23Header;
    DT_SUBTABLE             *ParentTable;
    ACPI_STATUS             Status = AE_OK;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoTpm23,
        &Subtable);

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    Tpm23Header = ACPI_CAST_PTR (ACPI_TABLE_TPM23, ParentTable->Buffer);

    /* Subtable type depends on the StartMethod */

    switch (Tpm23Header->StartMethod)
    {
    case ACPI_TPM23_ACPI_START_METHOD:

        /* Subtable specific to to ARM_SMC */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoTpm23a,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        break;

    default:
        break;
    }

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileTpm2
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile TPM2.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileTpm2 (
    void                    **List)
{
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_SUBTABLE             *Subtable;
    ACPI_TABLE_TPM2         *Tpm2Header;
    DT_SUBTABLE             *ParentTable;
    ACPI_STATUS             Status = AE_OK;
    ACPI_TABLE_HEADER       *Header;


    ParentTable = DtPeekSubtable ();

    Header = ACPI_CAST_PTR (ACPI_TABLE_HEADER, ParentTable->Buffer);

    if (Header->Revision == 3)
    {
        return (DtCompileTpm2Rev3 (List));
    }

    /* Compile the main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoTpm2,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    Tpm2Header = ACPI_CAST_PTR (ACPI_TABLE_TPM2, ParentTable->Buffer);

    /* Method parameters */
    /* Optional: Log area minimum length */
    /* Optional: Log area start address */
    /* TBD: Optional fields above not fully implemented (not optional at this time) */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoTpm2a,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);


    /* Subtable type depends on the StartMethod */

    switch (Tpm2Header->StartMethod)
    {
    case ACPI_TPM2_COMMAND_BUFFER_WITH_ARM_SMC:

        /* Subtable specific to to ARM_SMC */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoTpm211,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        break;

    case ACPI_TPM2_START_METHOD:
    case ACPI_TPM2_MEMORY_MAPPED:
    case ACPI_TPM2_COMMAND_BUFFER:
    case ACPI_TPM2_COMMAND_BUFFER_WITH_START_METHOD:
        break;

    case ACPI_TPM2_RESERVED1:
    case ACPI_TPM2_RESERVED3:
    case ACPI_TPM2_RESERVED4:
    case ACPI_TPM2_RESERVED5:
    case ACPI_TPM2_RESERVED9:
    case ACPI_TPM2_RESERVED10:

        AcpiOsPrintf ("\n**** Reserved TPM2 Start Method type 0x%X\n",
            Tpm2Header->StartMethod);
        Status = AE_ERROR;
        break;

    case ACPI_TPM2_NOT_ALLOWED:
    default:

        AcpiOsPrintf ("\n**** Unknown TPM2 Start Method type 0x%X\n",
            Tpm2Header->StartMethod);
        Status = AE_ERROR;
        break;
    }

    return (Status);
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
        &Subtable);
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
    DtCompileGeneric ((void **) PFieldList, NULL, NULL);
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileViot
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile VIOT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileViot (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *SubtableStart;
    ACPI_TABLE_VIOT         *Viot;
    ACPI_VIOT_HEADER        *ViotHeader;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT16                  NodeCount;

    ParentTable = DtPeekSubtable ();

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoViot, &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    DtInsertSubtable (ParentTable, Subtable);

    /*
     * Using ACPI_SUB_PTR, We needn't define a separate structure. Care
     * should be taken to avoid accessing ACPI_TABLE_HEADER fields.
     */
    Viot = ACPI_SUB_PTR (ACPI_TABLE_VIOT, Subtable->Buffer,
        sizeof (ACPI_TABLE_HEADER));

    Viot->NodeOffset = sizeof (ACPI_TABLE_VIOT);

    NodeCount = 0;
    while (*PFieldList) {
        SubtableStart = *PFieldList;
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoViotHeader,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPushSubtable (Subtable);

        ViotHeader = ACPI_CAST_PTR (ACPI_VIOT_HEADER, Subtable->Buffer);

        switch (ViotHeader->Type)
        {
        case ACPI_VIOT_NODE_PCI_RANGE:

            InfoTable = AcpiDmTableInfoViot1;
            break;

        case ACPI_VIOT_NODE_MMIO:

            InfoTable = AcpiDmTableInfoViot2;
            break;

        case ACPI_VIOT_NODE_VIRTIO_IOMMU_PCI:

            InfoTable = AcpiDmTableInfoViot3;
            break;

        case ACPI_VIOT_NODE_VIRTIO_IOMMU_MMIO:

            InfoTable = AcpiDmTableInfoViot4;
            break;

        default:

            DtFatal (ASL_MSG_UNKNOWN_SUBTABLE, SubtableStart, "VIOT");
            return (AE_ERROR);
        }

        Status = DtCompileTable (PFieldList, InfoTable, &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        ParentTable = DtPeekSubtable ();
        DtInsertSubtable (ParentTable, Subtable);
        DtPopSubtable ();
        NodeCount++;
    }

    Viot->NodeCount = NodeCount;
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
 * FUNCTION:    DtCompileWpbt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile WPBT.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileWpbt (
    void                    **List)
{
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    ACPI_TABLE_WPBT         *Table;
    ACPI_STATUS             Status;


    /* Compile the main table */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoWpbt, &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);
    Table = ACPI_CAST_PTR (ACPI_TABLE_WPBT, ParentTable->Buffer);

    /*
     * Exit now if there are no arguments specified. This is indicated by:
     * The "Command-line Arguments" field has not been specified (if specified,
     * it will be the last field in the field list -- after the main table).
     * Set the Argument Length in the main table to zero.
     */
    if (!*PFieldList)
    {
        Table->ArgumentsLength = 0;
        return (AE_OK);
    }

    /* Compile the argument list subtable */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoWpbt0, &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Extract the length of the Arguments buffer, insert into main table */

    Table->ArgumentsLength = (UINT16) Subtable->TotalLength;
    DtInsertSubtable (ParentTable, Subtable);
    return (AE_OK);
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
 *              Name                - Field name to end generic compiling
 *              Length              - Compiled table length to return
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile generic unknown table.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileGeneric (
    void                    **List,
    char                    *Name,
    UINT32                  *Length)
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

    if (Length && *Length)
    {
        *Length = 0;
    }
    while (*PFieldList)
    {
        if (Name && !strcmp ((*PFieldList)->Name, Name))
        {
            break;
        }

        Info = DtGetGenericTableInfo ((*PFieldList)->Name);
        if (!Info)
        {
            sprintf (AslGbl_MsgBuffer, "Generic data type \"%s\" not found",
                (*PFieldList)->Name);
            DtNameError (ASL_ERROR, ASL_MSG_INVALID_FIELD_NAME,
                (*PFieldList), AslGbl_MsgBuffer);

            *PFieldList = (*PFieldList)->Next;
            continue;
        }

        Status = DtCompileTable (PFieldList, Info,
            &Subtable);
        if (ACPI_SUCCESS (Status))
        {
            DtInsertSubtable (ParentTable, Subtable);
            if (Length)
            {
                *Length += Subtable->Length;
            }
        }
        else
        {
            *PFieldList = (*PFieldList)->Next;

            if (Status == AE_NOT_FOUND)
            {
                sprintf (AslGbl_MsgBuffer, "Generic data type \"%s\" not found",
                    (*PFieldList)->Name);
                DtNameError (ASL_ERROR, ASL_MSG_INVALID_FIELD_NAME,
                    (*PFieldList), AslGbl_MsgBuffer);
            }
        }
    }

    return (AE_OK);
}
