/******************************************************************************
 *
 * Module Name: tbconvrt - ACPI Table conversion utilities
 *              $Revision: 23 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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
#include "achware.h"
#include "actables.h"
#include "actbl.h"


#define _COMPONENT          ACPI_TABLES
        MODULE_NAME         ("tbconvrt")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableCount
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

UINT32
AcpiTbGetTableCount (
    RSDP_DESCRIPTOR         *RSDP,
    ACPI_TABLE_HEADER       *RSDT)
{
    UINT32                  PointerSize;



#ifndef _IA64

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
 * DESCRIPTION:
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbConvertToXsdt (
    ACPI_TABLE_DESC         *TableInfo,
    UINT32                  *NumberOfTables)
{
    UINT32                  TableSize;
    UINT32                  i;
    XSDT_DESCRIPTOR         *NewTable;


    *NumberOfTables = AcpiTbGetTableCount (AcpiGbl_RSDP, TableInfo->Pointer);


    /* Compute size of the converted XSDT */

    TableSize = (*NumberOfTables * sizeof (UINT64)) + sizeof (ACPI_TABLE_HEADER);


    /* Allocate an XSDT */

    NewTable = AcpiUtCallocate (TableSize);
    if (!NewTable)
    {
        return (AE_NO_MEMORY);
    }

    /* Copy the header and set the length */

    MEMCPY (NewTable, TableInfo->Pointer, sizeof (ACPI_TABLE_HEADER));
    NewTable->Header.Length = TableSize;

    /* Copy the table pointers */

    for (i = 0; i < *NumberOfTables; i++)
    {
        if (AcpiGbl_RSDP->Revision < 2)
        {
#ifdef _IA64
            NewTable->TableOffsetEntry[i] =
                ((RSDT_DESCRIPTOR_REV071 *) TableInfo->Pointer)->TableOffsetEntry[i];
#else
            ACPI_STORE_ADDRESS (NewTable->TableOffsetEntry[i],
                ((RSDT_DESCRIPTOR_REV1 *) TableInfo->Pointer)->TableOffsetEntry[i]);
#endif
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
 * FUNCTION:    AcpiTbConvertTableFadt
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION:
 *    Converts BIOS supplied 1.0 and 0.71 ACPI FADT to an intermediate
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

#ifdef _IA64
    FADT_DESCRIPTOR_REV071 *FADT71;
    UINT8                   Pm1AddressSpace;
    UINT8                   Pm2AddressSpace;
    UINT8                   PmTimerAddressSpace;
    UINT8                   Gpe0AddressSpace;
    UINT8                   Gpe1AddressSpace;
#else
    FADT_DESCRIPTOR_REV1   *FADT1;
#endif

    FADT_DESCRIPTOR_REV2   *FADT2;
    ACPI_TABLE_DESC        *TableDesc;


    FUNCTION_TRACE ("AcpiTbConvertTableFadt");


    /* AcpiGbl_FADT is valid */
    /* Allocate and zero the 2.0 buffer */

    FADT2 = AcpiUtCallocate (sizeof (FADT_DESCRIPTOR_REV2));
    if (FADT2 == NULL)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }


    /* The ACPI FADT revision number is FADT2_REVISION_ID=3 */
    /* So, if the current table revision is less than 3 it is type 1.0 or 0.71 */

    if (AcpiGbl_FADT->header.Revision >= FADT2_REVISION_ID)
    {
        /* We have an ACPI 2.0 FADT but we must copy it to our local buffer */

        *FADT2 = *((FADT_DESCRIPTOR_REV2*) AcpiGbl_FADT);

    }

    else
    {

#ifdef _IA64
        /*
         * For the 64-bit case only, a revision ID less than V2.0 means the
         * tables are the 0.71 extensions
         */

        /* The BIOS stored FADT should agree with Revision 0.71 */

        FADT71 = (FADT_DESCRIPTOR_REV071 *) AcpiGbl_FADT;

        /* Copy the table header*/

        FADT2->header       = FADT71->header;

        /* Copy the common fields */

        FADT2->SciInt       = FADT71->SciInt;
        FADT2->AcpiEnable   = FADT71->AcpiEnable;
        FADT2->AcpiDisable  = FADT71->AcpiDisable;
        FADT2->S4BiosReq    = FADT71->S4BiosReq;
        FADT2->Plvl2Lat     = FADT71->Plvl2Lat;
        FADT2->Plvl3Lat     = FADT71->Plvl3Lat;
        FADT2->DayAlrm      = FADT71->DayAlrm;
        FADT2->MonAlrm      = FADT71->MonAlrm;
        FADT2->Century      = FADT71->Century;
        FADT2->Gpe1Base     = FADT71->Gpe1Base;

        /*
         * We still use the block length registers even though
         * the GAS structure should obsolete them.  This is because
         * these registers are byte lengths versus the GAS which
         * contains a bit width
         */
        FADT2->Pm1EvtLen    = FADT71->Pm1EvtLen;
        FADT2->Pm1CntLen    = FADT71->Pm1CntLen;
        FADT2->Pm2CntLen    = FADT71->Pm2CntLen;
        FADT2->PmTmLen      = FADT71->PmTmLen;
        FADT2->Gpe0BlkLen   = FADT71->Gpe0BlkLen;
        FADT2->Gpe1BlkLen   = FADT71->Gpe1BlkLen;
        FADT2->Gpe1Base     = FADT71->Gpe1Base;

        /* Copy the existing 0.71 flags to 2.0. The other bits are zero.*/

        FADT2->WbInvd       = FADT71->FlushCash;
        FADT2->ProcC1       = FADT71->ProcC1;
        FADT2->Plvl2Up      = FADT71->Plvl2Up;
        FADT2->PwrButton    = FADT71->PwrButton;
        FADT2->SleepButton  = FADT71->SleepButton;
        FADT2->FixedRTC     = FADT71->FixedRTC;
        FADT2->Rtcs4        = FADT71->Rtcs4;
        FADT2->TmrValExt    = FADT71->TmrValExt;
        FADT2->DockCap      = FADT71->DockCap;


        /* We should not use these next two addresses */
        /* Since our buffer is pre-zeroed nothing to do for */
        /* the next three data items in the structure */
        /* FADT2->FirmwareCtrl = 0; */
        /* FADT2->Dsdt = 0; */

        /* System Interrupt Model isn't used in ACPI 2.0*/
        /* FADT2->Reserved1 = 0; */

        /* This field is set by the OEM to convey the preferred */
        /* power management profile to OSPM. It doesn't have any*/
        /* 0.71 equivalence.  Since we don't know what kind of  */
        /* 64-bit system this is, we will pick unspecified.     */

        FADT2->Prefer_PM_Profile = PM_UNSPECIFIED;


        /* Port address of SMI command port */
        /* We shouldn't use this port because IA64 doesn't */
        /* have or use SMI.  It has PMI. */

        FADT2->SmiCmd       = (UINT32)(FADT71->SmiCmd & 0xFFFFFFFF);


        /* processor performance state control*/
        /* The value OSPM writes to the SMI_CMD register to assume */
        /* processor performance state control responsibility. */
        /* There isn't any equivalence in 0.71 */
        /* Again this should be meaningless for IA64 */
        /* FADT2->PstateCnt = 0; */

        /* The 32-bit Power management and GPE registers are */
        /* not valid in IA-64 and we are not going to use them */
        /* so leaving them pre-zeroed. */

        /* Support for the _CST object and C States change notification.*/
        /* This data item hasn't any 0.71 equivalence so leaving it zero.*/
        /* FADT2->CstCnt = 0; */

        /* number of flush strides that need to be read */
        /* No 0.71 equivalence. Leave pre-zeroed. */
        /* FADT2->FlushSize = 0; */

        /* Processor's memory cache line width, in bytes */
        /* No 0.71 equivalence. Leave pre-zeroed. */
        /* FADT2->FlushStride = 0; */

        /* Processor's duty cycle index in processor's P_CNT reg*/
        /* No 0.71 equivalence. Leave pre-zeroed. */
        /* FADT2->DutyOffset = 0; */

        /* Processor's duty cycle value bit width in P_CNT register.*/
        /* No 0.71 equivalence. Leave pre-zeroed. */
        /* FADT2->DutyWidth = 0; */


        /* Since there isn't any equivalence in 0.71 */
        /* and since BigSur had to support legacy  */

        FADT2->IapcBootArch = BAF_LEGACY_DEVICES;

        /* Copy to ACPI 2.0 64-BIT Extended Addresses */

        FADT2->XFirmwareCtrl = FADT71->FirmwareCtrl;
        FADT2->XDsdt         = FADT71->Dsdt;


        /* Extract the address space IDs */

        Pm1AddressSpace     = (UINT8)((FADT71->AddressSpace & PM1_BLK_ADDRESS_SPACE)     >> 1);
        Pm2AddressSpace     = (UINT8)((FADT71->AddressSpace & PM2_CNT_BLK_ADDRESS_SPACE) >> 2);
        PmTimerAddressSpace = (UINT8)((FADT71->AddressSpace & PM_TMR_BLK_ADDRESS_SPACE)  >> 3);
        Gpe0AddressSpace    = (UINT8)((FADT71->AddressSpace & GPE0_BLK_ADDRESS_SPACE)    >> 4);
        Gpe1AddressSpace    = (UINT8)((FADT71->AddressSpace & GPE1_BLK_ADDRESS_SPACE)    >> 5);

        /*
         * Convert the 0.71 (non-GAS style) Block addresses to V2.0 GAS structures,
         * in this order:
         *
         * PM 1A Events
         * PM 1B Events
         * PM 1A Control
         * PM 1B Control
         * PM 2 Control
         * PM Timer Control
         * GPE Block 0
         * GPE Block 1
         */

        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XPm1aEvtBlk, FADT71->Pm1EvtLen,  FADT71->Pm1aEvtBlk, Pm1AddressSpace);
        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XPm1bEvtBlk, FADT71->Pm1EvtLen,  FADT71->Pm1bEvtBlk, Pm1AddressSpace);
        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XPm1aCntBlk, FADT71->Pm1CntLen,  FADT71->Pm1aCntBlk, Pm1AddressSpace);
        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XPm1bCntBlk, FADT71->Pm1CntLen,  FADT71->Pm1bCntBlk, Pm1AddressSpace);
        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XPm2CntBlk,  FADT71->Pm2CntLen,  FADT71->Pm2CntBlk,  Pm2AddressSpace);
        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XPmTmrBlk,   FADT71->PmTmLen,    FADT71->PmTmrBlk,   PmTimerAddressSpace);
        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XGpe0Blk,    FADT71->Gpe0BlkLen, FADT71->Gpe0Blk,    Gpe0AddressSpace);
        ASL_BUILD_GAS_FROM_ENTRY (FADT2->XGpe1Blk,    FADT71->Gpe1BlkLen, FADT71->Gpe1Blk,    Gpe1AddressSpace);

#else

        /* ACPI 1.0 FACS */


        /* The BIOS stored FADT should agree with Revision 1.0 */

        FADT1 = (FADT_DESCRIPTOR_REV1*) AcpiGbl_FADT;

        /*
         * Copy the table header and the common part of the tables
         * The 2.0 table is an extension of the 1.0 table, so the
         * entire 1.0 table can be copied first, then expand some
         * fields to 64 bits.
         */

        MEMCPY (FADT2, FADT1, sizeof (FADT_DESCRIPTOR_REV1));


        /* Convert table pointers to 64-bit fields */

        ACPI_STORE_ADDRESS (FADT2->XFirmwareCtrl, FADT1->FirmwareCtrl);
        ACPI_STORE_ADDRESS (FADT2->XDsdt, FADT1->Dsdt);

        /* System Interrupt Model isn't used in ACPI 2.0*/
        /* FADT2->Reserved1 = 0; */

        /* This field is set by the OEM to convey the preferred */
        /* power management profile to OSPM. It doesn't have any*/
        /* 1.0 equivalence.  Since we don't know what kind of   */
        /* 32-bit system this is, we will pick unspecified.     */

        FADT2->Prefer_PM_Profile = PM_UNSPECIFIED;


        /* Processor Performance State Control. This is the value  */
        /* OSPM writes to the SMI_CMD register to assume processor */
        /* performance state control responsibility. There isn't   */
        /* any equivalence in 1.0.  So leave it zeroed.            */

        FADT2->PstateCnt = 0;


        /* Support for the _CST object and C States change notification.*/
        /* This data item hasn't any 1.0 equivalence so leaving it zero.*/

        FADT2->CstCnt = 0;


        /* Since there isn't any equivalence in 1.0 and since it   */
        /* is highly likely that a 1.0 system has legacy  support. */

        FADT2->IapcBootArch = BAF_LEGACY_DEVICES;


        /*
         * Convert the V1.0 Block addresses to V2.0 GAS structures
         * in this order:
         *
         * PM 1A Events
         * PM 1B Events
         * PM 1A Control
         * PM 1B Control
         * PM 2 Control
         * PM Timer Control
         * GPE Block 0
         * GPE Block 1
         */

        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XPm1aEvtBlk, FADT1->Pm1EvtLen,  FADT1->Pm1aEvtBlk);
        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XPm1bEvtBlk, FADT1->Pm1EvtLen,  FADT1->Pm1bEvtBlk);
        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XPm1aCntBlk, FADT1->Pm1CntLen,  FADT1->Pm1aCntBlk);
        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XPm1bCntBlk, FADT1->Pm1CntLen,  FADT1->Pm1bCntBlk);
        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XPm2CntBlk,  FADT1->Pm2CntLen,  FADT1->Pm2CntBlk);
        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XPmTmrBlk,   FADT1->PmTmLen,    FADT1->PmTmrBlk);
        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XGpe0Blk,    FADT1->Gpe0BlkLen, FADT1->Gpe0Blk);
        ASL_BUILD_GAS_FROM_V1_ENTRY (FADT2->XGpe1Blk,    FADT1->Gpe1BlkLen, FADT1->Gpe1Blk);
#endif
    }


    /*
     * Global FADT pointer will point to the common V2.0 FADT
     */
    AcpiGbl_FADT = FADT2;
    AcpiGbl_FADT->header.Length = sizeof (FADT_DESCRIPTOR);


    /* Free the original table */

    TableDesc = &AcpiGbl_AcpiTables[ACPI_TABLE_FADT];
    AcpiTbDeleteSingleTable (TableDesc);


    /* Install the new table */

    TableDesc->Pointer = (ACPI_TABLE_HEADER *) AcpiGbl_FADT;
    TableDesc->BasePointer = AcpiGbl_FADT;
    TableDesc->Allocation = ACPI_MEM_ALLOCATED;
    TableDesc->Length = sizeof (FADT_DESCRIPTOR_REV2);


    /* Dump the entire FADT */

    DEBUG_PRINT (TRACE_TABLES,
        ("Hex dump of common internal FADT, size %ld (%lX)\n",
        AcpiGbl_FADT->header.Length, AcpiGbl_FADT->header.Length));
    DUMP_BUFFER ((UINT8 *) (AcpiGbl_FADT), AcpiGbl_FADT->header.Length);


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbConvertTableFacs
 *
 * PARAMETERS:
 *
 * RETURN:
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbBuildCommonFacs (
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_COMMON_FACS        *CommonFacs;

#ifdef _IA64
    FACS_DESCRIPTOR_REV071  *FACS71;
#else
    FACS_DESCRIPTOR_REV1    *FACS1;
#endif

    FACS_DESCRIPTOR_REV2    *FACS2;


    FUNCTION_TRACE ("AcpiTbBuildCommonFacs");


    /* Allocate a common FACS */

    CommonFacs = AcpiUtCallocate (sizeof (ACPI_COMMON_FACS));
    if (!CommonFacs)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }


    /* Copy fields to the new FACS */

    if (AcpiGbl_RSDP->Revision < 2)
    {
#ifdef _IA64
        /* 0.71 FACS */

        FACS71 = (FACS_DESCRIPTOR_REV071 *) AcpiGbl_FACS;

        CommonFacs->GlobalLock = (UINT32 *) &(FACS71->GlobalLock);
        CommonFacs->FirmwareWakingVector = &FACS71->FirmwareWakingVector;
        CommonFacs->VectorWidth = 64;
#else
        /* ACPI 1.0 FACS */

        FACS1 = (FACS_DESCRIPTOR_REV1 *) AcpiGbl_FACS;

        CommonFacs->GlobalLock = &(FACS1->GlobalLock);
        CommonFacs->FirmwareWakingVector = (UINT64 *) &FACS1->FirmwareWakingVector;
        CommonFacs->VectorWidth = 32;

#endif
    }

    else
    {
        /* ACPI 2.0 FACS */

        FACS2 = (FACS_DESCRIPTOR_REV2 *) AcpiGbl_FACS;

        CommonFacs->GlobalLock = &(FACS2->GlobalLock);
        CommonFacs->FirmwareWakingVector = &FACS2->XFirmwareWakingVector;
        CommonFacs->VectorWidth = 64;
    }


    /* Set the global FACS pointer to point to the common FACS */


    AcpiGbl_FACS = CommonFacs;

    return_ACPI_STATUS  (AE_OK);
}


