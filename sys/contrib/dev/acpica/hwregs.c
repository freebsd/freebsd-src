
/*******************************************************************************
 *
 * Module Name: hwregs - Read/write access functions for the various ACPI
 *                       control and status registers.
 *              $Revision: 110 $
 *
 ******************************************************************************/

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

#define __HWREGS_C__

#include "acpi.h"
#include "achware.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_HARDWARE
        MODULE_NAME         ("hwregs")


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwGetBitShift
 *
 * PARAMETERS:  Mask            - Input mask to determine bit shift from.
 *                                Must have at least 1 bit set.
 *
 * RETURN:      Bit location of the lsb of the mask
 *
 * DESCRIPTION: Returns the bit number for the low order bit that's set.
 *
 ******************************************************************************/

UINT32
AcpiHwGetBitShift (
    UINT32                  Mask)
{
    UINT32                  Shift;


    FUNCTION_TRACE ("HwGetBitShift");


    for (Shift = 0; ((Mask >> Shift) & 1) == 0; Shift++)
    { ; }

    return_VALUE (Shift);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwClearAcpiStatus
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Clears all fixed and general purpose status bits
 *
 ******************************************************************************/

void
AcpiHwClearAcpiStatus (void)
{
    UINT16                  GpeLength;
    UINT16                  Index;


    FUNCTION_TRACE ("HwClearAcpiStatus");


    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %04X to %04X\n",
        ALL_FIXED_STS_BITS,
        (UINT16) ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm1aEvtBlk.Address)));


    AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);

    AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, PM1_STS, ALL_FIXED_STS_BITS);


    if (ACPI_VALID_ADDRESS (AcpiGbl_FADT->XPm1bEvtBlk.Address))
    {
        AcpiOsWritePort ((ACPI_IO_ADDRESS)
            ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm1bEvtBlk.Address),
            ALL_FIXED_STS_BITS, 16);
    }

    /* now clear the GPE Bits */

    if (AcpiGbl_FADT->Gpe0BlkLen)
    {
        GpeLength = (UINT16) DIV_2 (AcpiGbl_FADT->Gpe0BlkLen);

        for (Index = 0; Index < GpeLength; Index++)
        {
            AcpiOsWritePort ((ACPI_IO_ADDRESS) (
                ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe0Blk.Address) + Index),
                    0xFF, 8);
        }
    }

    if (AcpiGbl_FADT->Gpe1BlkLen)
    {
        GpeLength = (UINT16) DIV_2 (AcpiGbl_FADT->Gpe1BlkLen);

        for (Index = 0; Index < GpeLength; Index++)
        {
            AcpiOsWritePort ((ACPI_IO_ADDRESS) (
                ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe1Blk.Address) + Index),
                0xFF, 8);
        }
    }

    AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwObtainSleepTypeRegisterData
 *
 * PARAMETERS:  SleepState        - Numeric state requested
 *              *Slp_TypA         - Pointer to byte to receive SLP_TYPa value
 *              *Slp_TypB         - Pointer to byte to receive SLP_TYPb value
 *
 * RETURN:      Status - ACPI status
 *
 * DESCRIPTION: AcpiHwObtainSleepTypeRegisterData() obtains the SLP_TYP and
 *              SLP_TYPb values for the sleep state requested.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwObtainSleepTypeRegisterData (
    UINT8                   SleepState,
    UINT8                   *Slp_TypA,
    UINT8                   *Slp_TypB)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE ("HwObtainSleepTypeRegisterData");


    /*
     *  Validate parameters
     */
    if ((SleepState > ACPI_S_STATES_MAX) ||
        !Slp_TypA || !Slp_TypB)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     *  AcpiEvaluate the namespace object containing the values for this state
     */
    Status = AcpiNsEvaluateByName ((NATIVE_CHAR *) AcpiGbl_DbSleepStates[SleepState],
                    NULL, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (!ObjDesc)
    {
        REPORT_ERROR (("Missing Sleep State object\n"));
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /*
     *  We got something, now ensure it is correct.  The object must
     *  be a package and must have at least 2 numeric values as the
     *  two elements
     */

    /* Even though AcpiEvaluateObject resolves package references,
     * NsEvaluate dpesn't. So, we do it here.
     */
    Status = AcpiUtResolvePackageReferences(ObjDesc);

    if (ObjDesc->Package.Count < 2)
    {
        /* Must have at least two elements */

        REPORT_ERROR (("Sleep State package does not have at least two elements\n"));
        Status = AE_ERROR;
    }

    else if (((ObjDesc->Package.Elements[0])->Common.Type !=
                ACPI_TYPE_INTEGER) ||
             ((ObjDesc->Package.Elements[1])->Common.Type !=
                ACPI_TYPE_INTEGER))
    {
        /* Must have two  */

        REPORT_ERROR (("Sleep State package elements are not both of type Number\n"));
        Status = AE_ERROR;
    }

    else
    {
        /*
         *  Valid _Sx_ package size, type, and value
         */
        *Slp_TypA = (UINT8) (ObjDesc->Package.Elements[0])->Integer.Value;

        *Slp_TypB = (UINT8) (ObjDesc->Package.Elements[1])->Integer.Value;
    }


    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Bad Sleep object %p type %X\n",
            ObjDesc, ObjDesc->Common.Type));
    }

    AcpiUtRemoveReference (ObjDesc);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwRegisterBitAccess
 *
 * PARAMETERS:  ReadWrite       - Either ACPI_READ or ACPI_WRITE.
 *              UseLock         - Lock the hardware
 *              RegisterId      - index of ACPI Register to access
 *              Value           - (only used on write) value to write to the
 *                                Register.  Shifted all the way right.
 *
 * RETURN:      Value written to or read from specified Register.  This value
 *              is shifted all the way right.
 *
 * DESCRIPTION: Generic ACPI Register read/write function.
 *
 ******************************************************************************/

UINT32
AcpiHwRegisterBitAccess (
    NATIVE_UINT             ReadWrite,
    BOOLEAN                 UseLock,
    UINT32                  RegisterId,
    ...)                    /* Value (only used on write) */
{
    UINT32                  RegisterValue = 0;
    UINT32                  Mask = 0;
    UINT32                  Value = 0;
    va_list                 marker;


    FUNCTION_TRACE ("HwRegisterBitAccess");


    if (ReadWrite == ACPI_WRITE)
    {
        va_start (marker, RegisterId);
        Value = va_arg (marker, UINT32);
        va_end (marker);
    }

    if (ACPI_MTX_LOCK == UseLock)
    {
        AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
    }

    /*
     * Decode the Register ID
     * Register id = Register block id | bit id
     *
     * Check bit id to fine locate Register offset.
     * Check Mask to determine Register offset, and then read-write.
     */
    switch (REGISTER_BLOCK_ID (RegisterId))
    {
    case PM1_STS:

        switch (RegisterId)
        {
        case TMR_STS:
            Mask = TMR_STS_MASK;
            break;

        case BM_STS:
            Mask = BM_STS_MASK;
            break;

        case GBL_STS:
            Mask = GBL_STS_MASK;
            break;

        case PWRBTN_STS:
            Mask = PWRBTN_STS_MASK;
            break;

        case SLPBTN_STS:
            Mask = SLPBTN_STS_MASK;
            break;

        case RTC_STS:
            Mask = RTC_STS_MASK;
            break;

        case WAK_STS:
            Mask = WAK_STS_MASK;
            break;

        default:
            Mask = 0;
            break;
        }

        RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, PM1_STS);

        if (ReadWrite == ACPI_WRITE)
        {
            /*
             * Status Registers are different from the rest.  Clear by
             * writing 1, writing 0 has no effect.  So, the only relevent
             * information is the single bit we're interested in, all
             * others should be written as 0 so they will be left
             * unchanged
             */
            Value <<= AcpiHwGetBitShift (Mask);
            Value &= Mask;

            if (Value)
            {
                AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, PM1_STS,
                    (UINT16) Value);
                RegisterValue = 0;
            }
        }

        break;


    case PM1_EN:

        switch (RegisterId)
        {
        case TMR_EN:
            Mask = TMR_EN_MASK;
            break;

        case GBL_EN:
            Mask = GBL_EN_MASK;
            break;

        case PWRBTN_EN:
            Mask = PWRBTN_EN_MASK;
            break;

        case SLPBTN_EN:
            Mask = SLPBTN_EN_MASK;
            break;

        case RTC_EN:
            Mask = RTC_EN_MASK;
            break;

        default:
            Mask = 0;
            break;
        }

        RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, PM1_EN);

        if (ReadWrite == ACPI_WRITE)
        {
            RegisterValue &= ~Mask;
            Value          <<= AcpiHwGetBitShift (Mask);
            Value          &= Mask;
            RegisterValue |= Value;

            AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, PM1_EN, (UINT16) RegisterValue);
        }

        break;


    case PM1_CONTROL:

        switch (RegisterId)
        {
        case SCI_EN:
            Mask = SCI_EN_MASK;
            break;

        case BM_RLD:
            Mask = BM_RLD_MASK;
            break;

        case GBL_RLS:
            Mask = GBL_RLS_MASK;
            break;

        case SLP_TYPE_A:
        case SLP_TYPE_B:
            Mask = SLP_TYPE_X_MASK;
            break;

        case SLP_EN:
            Mask = SLP_EN_MASK;
            break;

        default:
            Mask = 0;
            break;
        }


        /*
         * Read the PM1 Control register.
         * Note that at this level, the fact that there are actually TWO
         * registers (A and B) and that B may not exist, are abstracted.
         */
        RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, PM1_CONTROL);

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM1 control: Read %X\n", RegisterValue));

        if (ReadWrite == ACPI_WRITE)
        {
            RegisterValue  &= ~Mask;
            Value          <<= AcpiHwGetBitShift (Mask);
            Value          &= Mask;
            RegisterValue  |= Value;

            /*
             * SLP_TYPE_x Registers are written differently
             * than any other control Registers with
             * respect to A and B Registers.  The value
             * for A may be different than the value for B
             *
             * Therefore, pass the RegisterId, not just generic PM1_CONTROL,
             * because we need to do different things. Yuck.
             */
            AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, RegisterId,
                    (UINT16) RegisterValue);
        }
        break;


    case PM2_CONTROL:

        switch (RegisterId)
        {
        case ARB_DIS:
            Mask = ARB_DIS_MASK;
            break;

        default:
            Mask = 0;
            break;
        }

        RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, PM2_CONTROL);

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM2 control: Read %X from %8.8X%8.8X\n",
            RegisterValue, HIDWORD(AcpiGbl_FADT->XPm2CntBlk.Address),
            LODWORD(AcpiGbl_FADT->XPm2CntBlk.Address)));

        if (ReadWrite == ACPI_WRITE)
        {
            RegisterValue  &= ~Mask;
            Value          <<= AcpiHwGetBitShift (Mask);
            Value          &= Mask;
            RegisterValue  |= Value;

            ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %04X to %8.8X%8.8X\n",
                RegisterValue,
                HIDWORD(AcpiGbl_FADT->XPm2CntBlk.Address),
                LODWORD(AcpiGbl_FADT->XPm2CntBlk.Address)));

            AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                                PM2_CONTROL, (UINT8) (RegisterValue));
        }
        break;


    case PM_TIMER:

        Mask = TMR_VAL_MASK;
        RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                                            PM_TIMER);
        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM_TIMER: Read %X from %8.8X%8.8X\n",
            RegisterValue,
            HIDWORD(AcpiGbl_FADT->XPmTmrBlk.Address),
            LODWORD(AcpiGbl_FADT->XPmTmrBlk.Address)));

        break;


    case GPE1_EN_BLOCK:
    case GPE1_STS_BLOCK:
    case GPE0_EN_BLOCK:
    case GPE0_STS_BLOCK:

        /* Determine the bit to be accessed
         *
         *  (UINT32) RegisterId:
         *      31      24       16       8        0
         *      +--------+--------+--------+--------+
         *      |  gpe_block_id   |  gpe_bit_number |
         *      +--------+--------+--------+--------+
         *
         *     gpe_block_id is one of GPE[01]_EN_BLOCK and GPE[01]_STS_BLOCK
         *     gpe_bit_number is relative from the gpe_block (0x00~0xFF)
         */
        Mask = REGISTER_BIT_ID(RegisterId); /* gpe_bit_number */
        RegisterId = REGISTER_BLOCK_ID(RegisterId) | (Mask >> 3);
        Mask = AcpiGbl_DecodeTo8bit [Mask % 8];

        /*
         * The base address of the GPE 0 Register Block
         * Plus 1/2 the length of the GPE 0 Register Block
         * The enable Register is the Register following the Status Register
         * and each Register is defined as 1/2 of the total Register Block
         */

        /*
         * This sets the bit within EnableBit that needs to be written to
         * the Register indicated in Mask to a 1, all others are 0
         */

        /* Now get the current Enable Bits in the selected Reg */

        RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, RegisterId);
        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "GPE Enable bits: Read %X from %X\n",
            RegisterValue, RegisterId));

        if (ReadWrite == ACPI_WRITE)
        {
            RegisterValue  &= ~Mask;
            Value          <<= AcpiHwGetBitShift (Mask);
            Value          &= Mask;
            RegisterValue  |= Value;

            /*
             * This write will put the Action state into the General Purpose
             * Enable Register indexed by the value in Mask
             */
            ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %04X to %04X\n",
                RegisterValue, RegisterId));
            AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, RegisterId,
                (UINT8) RegisterValue);
            RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                                RegisterId);
        }
        break;


    case SMI_CMD_BLOCK:
    case PROCESSOR_BLOCK:

        /* Not used by any callers at this time - therefore, not implemented */

    default:

        Mask = 0;
        break;
    }

    if (ACPI_MTX_LOCK == UseLock) {
        AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }


    RegisterValue &= Mask;
    RegisterValue >>= AcpiHwGetBitShift (Mask);

    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "Register I/O: returning %X\n", RegisterValue));
    return_VALUE (RegisterValue);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwRegisterRead
 *
 * PARAMETERS:  UseLock                - Mutex hw access.
 *              RegisterId             - RegisterID + Offset.
 *
 * RETURN:      Value read or written.
 *
 * DESCRIPTION: Acpi register read function.  Registers are read at the
 *              given offset.
 *
 ******************************************************************************/

UINT32
AcpiHwRegisterRead (
    BOOLEAN                 UseLock,
    UINT32                  RegisterId)
{
    UINT32                  Value = 0;
    UINT32                  BankOffset;


    FUNCTION_TRACE ("HwRegisterRead");


    if (ACPI_MTX_LOCK == UseLock)
    {
        AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
    }


    switch (REGISTER_BLOCK_ID(RegisterId))
    {
    case PM1_STS: /* 16-bit access */

        Value =  AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1aEvtBlk, 0);
        Value |= AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1bEvtBlk, 0);
        break;


    case PM1_EN: /* 16-bit access*/

        BankOffset  = DIV_2 (AcpiGbl_FADT->Pm1EvtLen);
        Value =  AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1aEvtBlk, BankOffset);
        Value |= AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1bEvtBlk, BankOffset);
        break;


    case PM1_CONTROL: /* 16-bit access */

        Value =  AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1aCntBlk, 0);
        Value |= AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1bCntBlk, 0);
        break;


    case PM2_CONTROL: /* 8-bit access */

        Value =  AcpiHwLowLevelRead (8, &AcpiGbl_FADT->XPm2CntBlk, 0);
        break;


    case PM_TIMER: /* 32-bit access */

        Value =  AcpiHwLowLevelRead (32, &AcpiGbl_FADT->XPmTmrBlk, 0);
        break;


    /*
     * For the GPE? Blocks, the lower word of RegisterId contains the
     * byte offset for which to read, as each part of each block may be
     * several bytes long.
     */
    case GPE0_STS_BLOCK: /* 8-bit access */

        BankOffset = REGISTER_BIT_ID(RegisterId);
        Value = AcpiHwLowLevelRead (8, &AcpiGbl_FADT->XGpe0Blk, BankOffset);
        break;

    case GPE0_EN_BLOCK: /* 8-bit access */

        BankOffset = DIV_2 (AcpiGbl_FADT->Gpe0BlkLen) + REGISTER_BIT_ID(RegisterId);
        Value = AcpiHwLowLevelRead (8, &AcpiGbl_FADT->XGpe0Blk, BankOffset);
        break;

    case GPE1_STS_BLOCK: /* 8-bit access */

        BankOffset = REGISTER_BIT_ID(RegisterId);
        Value = AcpiHwLowLevelRead (8, &AcpiGbl_FADT->XGpe1Blk, BankOffset);
        break;

    case GPE1_EN_BLOCK: /* 8-bit access */

        BankOffset  = DIV_2 (AcpiGbl_FADT->Gpe1BlkLen) + REGISTER_BIT_ID(RegisterId);
        Value = AcpiHwLowLevelRead (8, &AcpiGbl_FADT->XGpe1Blk, BankOffset);
        break;

    case SMI_CMD_BLOCK: /* 8bit */

        AcpiOsReadPort (AcpiGbl_FADT->SmiCmd, &Value, 8);
        break;

    default:
        /* Value will be returned as 0 */
        break;
    }


    if (ACPI_MTX_LOCK == UseLock)
    {
        AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }

    return_VALUE (Value);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwRegisterWrite
 *
 * PARAMETERS:  UseLock                - Mutex hw access.
 *              RegisterId             - RegisterID + Offset.
 *
 * RETURN:      Value read or written.
 *
 * DESCRIPTION: Acpi register Write function.  Registers are written at the
 *              given offset.
 *
 ******************************************************************************/

void
AcpiHwRegisterWrite (
    BOOLEAN                 UseLock,
    UINT32                  RegisterId,
    UINT32                  Value)
{
    UINT32                  BankOffset;


    FUNCTION_TRACE ("HwRegisterWrite");


    if (ACPI_MTX_LOCK == UseLock)
    {
        AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
    }


    switch (REGISTER_BLOCK_ID (RegisterId))
    {
    case PM1_STS: /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aEvtBlk, 0);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bEvtBlk, 0);
        break;


    case PM1_EN: /* 16-bit access*/

        BankOffset = DIV_2 (AcpiGbl_FADT->Pm1EvtLen);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aEvtBlk, BankOffset);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bEvtBlk, BankOffset);
        break;


    case PM1_CONTROL: /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aCntBlk, 0);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bCntBlk, 0);
        break;


    case PM1A_CONTROL: /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aCntBlk, 0);
        break;


    case PM1B_CONTROL: /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bCntBlk, 0);
        break;


    case PM2_CONTROL: /* 8-bit access */

        AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT->XPm2CntBlk, 0);
        break;


    case PM_TIMER: /* 32-bit access */

        AcpiHwLowLevelWrite (32, Value, &AcpiGbl_FADT->XPmTmrBlk, 0);
        break;


    case GPE0_STS_BLOCK: /* 8-bit access */

        BankOffset = REGISTER_BIT_ID(RegisterId);
        AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT->XGpe0Blk, BankOffset);
        break;


    case GPE0_EN_BLOCK: /* 8-bit access */

        BankOffset  = DIV_2 (AcpiGbl_FADT->Gpe0BlkLen) + REGISTER_BIT_ID(RegisterId);
        AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT->XGpe0Blk, BankOffset);
        break;


    case GPE1_STS_BLOCK: /* 8-bit access */

        BankOffset = REGISTER_BIT_ID(RegisterId);
        AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT->XGpe1Blk, BankOffset);
        break;


    case GPE1_EN_BLOCK: /* 8-bit access */

        BankOffset  = DIV_2 (AcpiGbl_FADT->Gpe1BlkLen) + REGISTER_BIT_ID(RegisterId);
        AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT->XGpe1Blk, BankOffset);
        break;


    case SMI_CMD_BLOCK: /* 8bit */

        /* For 2.0, SMI_CMD is always in IO space */
        /* TBD: what about 1.0? 0.71? */

        AcpiOsWritePort (AcpiGbl_FADT->SmiCmd, Value, 8);
        break;


    default:
        Value = 0;
        break;
    }


    if (ACPI_MTX_LOCK == UseLock)
    {
        AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }

    return_VOID;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwLowLevelRead
 *
 * PARAMETERS:  Register            - GAS register structure
 *              Offset              - Offset from the base address in the GAS
 *              Width               - 8, 16, or 32
 *
 * RETURN:      Value read
 *
 * DESCRIPTION: Read from either memory, IO, or PCI config space.
 *
 ******************************************************************************/

UINT32
AcpiHwLowLevelRead (
    UINT32                  Width,
    ACPI_GENERIC_ADDRESS    *Reg,
    UINT32                  Offset)
{
    UINT32                  Value = 0;
    ACPI_PHYSICAL_ADDRESS   MemAddress;
    ACPI_IO_ADDRESS         IoAddress;
    ACPI_PCI_ID             PciId;
    UINT16                  PciRegister;


    FUNCTION_ENTRY ();


    /*
     * Must have a valid pointer to a GAS structure, and
     * a non-zero address within
     */
    if ((!Reg) ||
        (!ACPI_VALID_ADDRESS (Reg->Address)))
    {
        return 0;
    }


    /*
     * Three address spaces supported:
     * Memory, Io, or PCI config.
     */
    switch (Reg->AddressSpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_MEMORY:

        MemAddress = (ACPI_PHYSICAL_ADDRESS) (ACPI_GET_ADDRESS (Reg->Address) + Offset);

        AcpiOsReadMemory (MemAddress, &Value, Width);
        break;


    case ACPI_ADR_SPACE_SYSTEM_IO:

        IoAddress = (ACPI_IO_ADDRESS) (ACPI_GET_ADDRESS (Reg->Address) + Offset);

        AcpiOsReadPort (IoAddress, &Value, Width);
        break;


    case ACPI_ADR_SPACE_PCI_CONFIG:

        PciId.Segment  = 0;
        PciId.Bus      = 0;
        PciId.Device   = ACPI_PCI_DEVICE (ACPI_GET_ADDRESS (Reg->Address));
        PciId.Function = ACPI_PCI_FUNCTION (ACPI_GET_ADDRESS (Reg->Address));
        PciRegister    = (UINT16) (ACPI_PCI_REGISTER (ACPI_GET_ADDRESS (Reg->Address)) + Offset);

        AcpiOsReadPciConfiguration  (&PciId, PciRegister, &Value, Width);
        break;
    }

    return Value;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwLowLevelWrite
 *
 * PARAMETERS:  Width               - 8, 16, or 32
 *              Value               - To be written
 *              Register            - GAS register structure
 *              Offset              - Offset from the base address in the GAS
 *
 *
 * RETURN:      Value read
 *
 * DESCRIPTION: Read from either memory, IO, or PCI config space.
 *
 ******************************************************************************/

void
AcpiHwLowLevelWrite (
    UINT32                  Width,
    UINT32                  Value,
    ACPI_GENERIC_ADDRESS    *Reg,
    UINT32                  Offset)
{
    ACPI_PHYSICAL_ADDRESS   MemAddress;
    ACPI_IO_ADDRESS         IoAddress;
    ACPI_PCI_ID             PciId;
    UINT16                  PciRegister;


    FUNCTION_ENTRY ();


    /*
     * Must have a valid pointer to a GAS structure, and
     * a non-zero address within
     */
    if ((!Reg) ||
        (!ACPI_VALID_ADDRESS (Reg->Address)))
    {
        return;
    }


    /*
     * Three address spaces supported:
     * Memory, Io, or PCI config.
     */
    switch (Reg->AddressSpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_MEMORY:

        MemAddress = (ACPI_PHYSICAL_ADDRESS) (ACPI_GET_ADDRESS (Reg->Address) + Offset);

        AcpiOsWriteMemory (MemAddress, Value, Width);
        break;


    case ACPI_ADR_SPACE_SYSTEM_IO:

        IoAddress = (ACPI_IO_ADDRESS) (ACPI_GET_ADDRESS (Reg->Address) + Offset);

        AcpiOsWritePort (IoAddress, Value, Width);
        break;


    case ACPI_ADR_SPACE_PCI_CONFIG:

        PciId.Segment  = 0;
        PciId.Bus      = 0;
        PciId.Device   = ACPI_PCI_DEVICE (ACPI_GET_ADDRESS (Reg->Address));
        PciId.Function = ACPI_PCI_FUNCTION (ACPI_GET_ADDRESS (Reg->Address));
        PciRegister    = (UINT16) (ACPI_PCI_REGISTER (ACPI_GET_ADDRESS (Reg->Address)) + Offset);

        AcpiOsWritePciConfiguration (&PciId, PciRegister, Value, Width);
        break;
    }
}
