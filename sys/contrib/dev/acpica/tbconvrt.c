/******************************************************************************
 *
 * Module Name: tbconvrt - ACPI Table conversion utilities
 *              $Revision: 42 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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

#define __TBCONVRT_C__

#include "acpi.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbconvrt")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableCount
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION: Calculate the number of tables
 *
 ******************************************************************************/

UINT32
AcpiTbGetTableCount (
    RSDP_DESCRIPTOR         *RSDP,
    ACPI_TABLE_HEADER       *RSDT)
{
    UINT32                  PointerSize;


    ACPI_FUNCTION_ENTRY ();


#if ACPI_MACHINE_WIDTH != 64

    if (RSDP->Revision < 2)
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
    return ((RSDT->Length - sizeof (ACPI_TABLE_HEADER)) / PointerSize);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbConvertToXsdt
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION: Convert an RSDT to an XSDT (internal common format)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbConvertToXsdt (
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_SIZE               TableSize;
    UINT32                  i;
    XSDT_DESCRIPTOR         *NewTable;


    ACPI_FUNCTION_ENTRY ();


    /* Compute size of the converted XSDT */

    TableSize = ((ACPI_SIZE) AcpiGbl_RsdtTableCount * sizeof (UINT64)) + 
                    sizeof (ACPI_TABLE_HEADER);

    /* Allocate an XSDT */

    NewTable = ACPI_MEM_CALLOCATE (TableSize);
    if (!NewTable)
    {
        return (AE_NO_MEMORY);
    }

    /* Copy the header and set the length */

    ACPI_MEMCPY (NewTable, TableInfo->Pointer, sizeof (ACPI_TABLE_HEADER));
    NewTable->Header.Length = (UINT32) TableSize;

    /* Copy the table pointers */

    for (i = 0; i < AcpiGbl_RsdtTableCount; i++)
    {
        if (AcpiGbl_RSDP->Revision < 2)
        {
            ACPI_STORE_ADDRESS (NewTable->TableOffsetEntry[i],
                ((RSDT_DESCRIPTOR_REV1 *) TableInfo->Pointer)->TableOffsetEntry[i]);
        }
        else
        {
            NewTable->TableOffsetEntry[i] =
                ((XSDT_DESCRIPTOR *) TableInfo->Pointer)->TableOffsetEntry[i];
        }
    }

    /* Delete the original table (either mapped or in a buffer) */

    AcpiTbDeleteSingleTable (TableInfo);

    /* Point the table descriptor to the new table */

    TableInfo->Pointer      = (ACPI_TABLE_HEADER *) NewTable;
    TableInfo->BasePointer  = (ACPI_TABLE_HEADER *) NewTable;
    TableInfo->Length       = TableSize;
    TableInfo->Allocation   = ACPI_MEM_ALLOCATED;

    return (AE_OK);
}



/*******************************************************************************
 *
 * FUNCTION:    AcpiTbConvertFadt1
 *
 * PARAMETERS:  LocalFadt       - Pointer to new FADT
 *              OriginalFadt    - Pointer to old FADT
 *
 * RETURN:      Populates LocalFadt
 *
 * DESCRIPTION: Convert an ACPI 1.0 FADT to common internal format
 *
 ******************************************************************************/

static void
AcpiTbConvertFadt1 (
    FADT_DESCRIPTOR_REV2   *LocalFadt,
    FADT_DESCRIPTOR_REV1   *OriginalFadt)
{


    /* ACPI 1.0 FACS */
    /* The BIOS stored FADT should agree with Revision 1.0 */

    /*
     * Copy the table header and the common part of the tables.
     *
     * The 2.0 table is an extension of the 1.0 table, so the entire 1.0
     * table can be copied first, then expand some fields to 64 bits.
     */
    ACPI_MEMCPY (LocalFadt, OriginalFadt, sizeof (FADT_DESCRIPTOR_REV1));

    /* Convert table pointers to 64-bit fields */

    ACPI_STORE_ADDRESS (LocalFadt->XFirmwareCtrl, LocalFadt->V1_FirmwareCtrl);
    ACPI_STORE_ADDRESS (LocalFadt->XDsdt, LocalFadt->V1_Dsdt);

    /*
     * System Interrupt Model isn't used in ACPI 2.0 (LocalFadt->Reserved1 = 0;)
     */

    /*
     * This field is set by the OEM to convey the preferred power management
     * profile to OSPM. It doesn't have any 1.0 equivalence.  Since we don't
     * know what kind of 32-bit system this is, we will use "unspecified".
     */
    LocalFadt->Prefer_PM_Profile = PM_UNSPECIFIED;

    /*
     * Processor Performance State Control. This is the value OSPM writes to
     * the SMI_CMD register to assume processor performance state control
     * responsibility. There isn't any equivalence in 1.0, leave it zeroed.
     */
    LocalFadt->PstateCnt = 0;

    /*
     * Support for the _CST object and C States change notification.
     * This data item hasn't any 1.0 equivalence so leave it zero.
     */
    LocalFadt->CstCnt = 0;

    /*
     * Since there isn't any equivalence in 1.0 and since it highly likely
     * that a 1.0 system has legacy support.
     */
    LocalFadt->IapcBootArch = BAF_LEGACY_DEVICES;

    /*
     * Convert the V1.0 block addresses to V2.0 GAS structures
     */
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1aEvtBlk, LocalFadt->Pm1EvtLen,  LocalFadt->V1_Pm1aEvtBlk);
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1bEvtBlk, LocalFadt->Pm1EvtLen,  LocalFadt->V1_Pm1bEvtBlk);
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1aCntBlk, LocalFadt->Pm1CntLen,  LocalFadt->V1_Pm1aCntBlk);
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1bCntBlk, LocalFadt->Pm1CntLen,  LocalFadt->V1_Pm1bCntBlk);
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm2CntBlk,  LocalFadt->Pm2CntLen,  LocalFadt->V1_Pm2CntBlk);
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPmTmrBlk,   LocalFadt->PmTmLen,    LocalFadt->V1_PmTmrBlk);
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XGpe0Blk,    LocalFadt->Gpe0BlkLen, LocalFadt->V1_Gpe0Blk);
    ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XGpe1Blk,    LocalFadt->Gpe1BlkLen, LocalFadt->V1_Gpe1Blk);

}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbConvertFadt2
 *
 * PARAMETERS:  LocalFadt       - Pointer to new FADT
 *              OriginalFadt    - Pointer to old FADT
 *
 * RETURN:      Populates LocalFadt
 *
 * DESCRIPTION: Convert an ACPI 2.0 FADT to common internal format.
 *              Handles optional "X" fields.
 *
 ******************************************************************************/

static void
AcpiTbConvertFadt2 (
    FADT_DESCRIPTOR_REV2   *LocalFadt,
    FADT_DESCRIPTOR_REV2   *OriginalFadt)
{

    /* We have an ACPI 2.0 FADT but we must copy it to our local buffer */

    ACPI_MEMCPY (LocalFadt, OriginalFadt, sizeof (FADT_DESCRIPTOR_REV2));

    /*
     * "X" fields are optional extensions to the original V1.0 fields, so
     * we must selectively expand V1.0 fields if the corresponding X field
     * is zero.
     */
    if (!(ACPI_GET_ADDRESS (LocalFadt->XFirmwareCtrl)))
    {
        ACPI_STORE_ADDRESS (LocalFadt->XFirmwareCtrl, LocalFadt->V1_FirmwareCtrl);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XDsdt)))
    {
        ACPI_STORE_ADDRESS (LocalFadt->XDsdt, LocalFadt->V1_Dsdt);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XPm1aEvtBlk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1aEvtBlk, 
            LocalFadt->Pm1EvtLen,  LocalFadt->V1_Pm1aEvtBlk);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XPm1bEvtBlk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1bEvtBlk, 
            LocalFadt->Pm1EvtLen,  LocalFadt->V1_Pm1bEvtBlk);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XPm1aCntBlk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1aCntBlk, 
            LocalFadt->Pm1CntLen,  LocalFadt->V1_Pm1aCntBlk);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XPm1bCntBlk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm1bCntBlk, 
            LocalFadt->Pm1CntLen,  LocalFadt->V1_Pm1bCntBlk);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XPm2CntBlk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPm2CntBlk,  
            LocalFadt->Pm2CntLen,  LocalFadt->V1_Pm2CntBlk);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XPmTmrBlk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XPmTmrBlk,   
            LocalFadt->PmTmLen,    LocalFadt->V1_PmTmrBlk);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XGpe0Blk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XGpe0Blk,    
            LocalFadt->Gpe0BlkLen, LocalFadt->V1_Gpe0Blk);
    }

    if (!(ACPI_GET_ADDRESS (LocalFadt->XGpe1Blk.Address)))
    {
        ASL_BUILD_GAS_FROM_V1_ENTRY (LocalFadt->XGpe1Blk,    
            LocalFadt->Gpe1BlkLen, LocalFadt->V1_Gpe1Blk);
    }
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiTbConvertTableFadt
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION:
 *    Converts a BIOS supplied ACPI 1.0 FADT to an intermediate
 *    ACPI 2.0 FADT. If the BIOS supplied a 2.0 FADT then it is simply
 *    copied to the intermediate FADT.  The ACPI CA software uses this
 *    intermediate FADT. Thus a significant amount of special #ifdef
 *    type codeing is saved. This intermediate FADT will need to be
 *    freed at some point.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbConvertTableFadt (void)
{
    FADT_DESCRIPTOR_REV2   *LocalFadt;
    ACPI_TABLE_DESC        *TableDesc;


    ACPI_FUNCTION_TRACE ("TbConvertTableFadt");


    /*
     * AcpiGbl_FADT is valid
     * Allocate and zero the 2.0 FADT buffer
     */
    LocalFadt = ACPI_MEM_CALLOCATE (sizeof (FADT_DESCRIPTOR_REV2));
    if (LocalFadt == NULL)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }
    
    /*
     * FADT length and version validation.  The table must be at least as
     * long as the version 1.0 FADT
     */
    if (AcpiGbl_FADT->Header.Length < sizeof (FADT_DESCRIPTOR_REV1))
    {
        ACPI_REPORT_ERROR (("Invalid FADT table length: 0x%X\n", AcpiGbl_FADT->Header.Length));
        return_ACPI_STATUS (AE_INVALID_TABLE_LENGTH);
    }

    if (AcpiGbl_FADT->Header.Revision >= FADT2_REVISION_ID)
    {
        if (AcpiGbl_FADT->Header.Length < sizeof (FADT_DESCRIPTOR_REV2))
        {
            /* Length is too short to be a V2.0 table */

            ACPI_REPORT_WARNING (("Inconsistent FADT length (0x%X) and revision (0x%X), using FADT V1.0 portion of table\n", 
                        AcpiGbl_FADT->Header.Length, AcpiGbl_FADT->Header.Revision));

            AcpiTbConvertFadt1 (LocalFadt, (void *) AcpiGbl_FADT);
        }
        else
        {
            /* Valid V2.0 table */

            AcpiTbConvertFadt2 (LocalFadt, AcpiGbl_FADT);
        }
    }
    else
    {
        /* Valid V1.0 table */

        AcpiTbConvertFadt1 (LocalFadt, (void *) AcpiGbl_FADT);
    }

    /*
     * Global FADT pointer will point to the new common V2.0 FADT
     */
    AcpiGbl_FADT = LocalFadt;
    AcpiGbl_FADT->Header.Length = sizeof (FADT_DESCRIPTOR);

    /* Free the original table */

    TableDesc = &AcpiGbl_AcpiTables[ACPI_TABLE_FADT];
    AcpiTbDeleteSingleTable (TableDesc);

    /* Install the new table */

    TableDesc->Pointer      = (ACPI_TABLE_HEADER *) AcpiGbl_FADT;
    TableDesc->BasePointer  = AcpiGbl_FADT;
    TableDesc->Allocation   = ACPI_MEM_ALLOCATED;
    TableDesc->Length       = sizeof (FADT_DESCRIPTOR_REV2);

    /* Dump the entire FADT */

    ACPI_DEBUG_PRINT ((ACPI_DB_TABLES,
        "Hex dump of common internal FADT, size %d (%X)\n",
        AcpiGbl_FADT->Header.Length, AcpiGbl_FADT->Header.Length));
    ACPI_DUMP_BUFFER ((UINT8 *) (AcpiGbl_FADT), AcpiGbl_FADT->Header.Length);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbConvertTableFacs
 *
 * PARAMETERS:  TableInfo       - Info for currently installad FACS
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert ACPI 1.0 and ACPI 2.0 FACS to a common internal
 *              table format.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbBuildCommonFacs (
    ACPI_TABLE_DESC         *TableInfo)
{

    ACPI_FUNCTION_TRACE ("TbBuildCommonFacs");


    /* Absolute minimum length is 24, but the ACPI spec says 64 */

    if (AcpiGbl_FACS->Length < 24)
    {
        ACPI_REPORT_ERROR (("Invalid FACS table length: 0x%X\n", AcpiGbl_FACS->Length));
        return_ACPI_STATUS (AE_INVALID_TABLE_LENGTH);
    }

    if (AcpiGbl_FACS->Length < 64)
    {
        ACPI_REPORT_WARNING (("FACS is shorter than the ACPI specification allows: 0x%X, using anyway\n", 
            AcpiGbl_FACS->Length));
    }

    /* Copy fields to the new FACS */

    AcpiGbl_CommonFACS.GlobalLock = &(AcpiGbl_FACS->GlobalLock);

    if ((AcpiGbl_RSDP->Revision < 2) ||
        (AcpiGbl_FACS->Length < 32)  ||
        (!(ACPI_GET_ADDRESS (AcpiGbl_FACS->XFirmwareWakingVector))))
    {
        /* ACPI 1.0 FACS or short table or optional X_ field is zero */

        AcpiGbl_CommonFACS.FirmwareWakingVector = ACPI_CAST_PTR (UINT64, &(AcpiGbl_FACS->FirmwareWakingVector));
        AcpiGbl_CommonFACS.VectorWidth = 32;
    }
    else
    {
        /* ACPI 2.0 FACS with valid X_ field */

        AcpiGbl_CommonFACS.FirmwareWakingVector = &AcpiGbl_FACS->XFirmwareWakingVector;
        AcpiGbl_CommonFACS.VectorWidth = 64;
    }

    return_ACPI_STATUS (AE_OK);
}


