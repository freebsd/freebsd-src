
/*******************************************************************************
 *
 * Module Name: hwregs - Read/write access functions for the various ACPI
 *                       control and status registers.
 *              $Revision: 120 $
 *
 ******************************************************************************/

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

#define __HWREGS_C__

#include "acpi.h"
#include "achware.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwregs")


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
    NATIVE_UINT             i;
    NATIVE_UINT             GpeBlock;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("HwClearAcpiStatus");


    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %04X to %04X\n",
        ACPI_BITMASK_ALL_FIXED_STATUS,
        (UINT16) ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm1aEvtBlk.Address)));


    Status = AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
    if (ACPI_FAILURE (Status))
    {
        return_VOID;
    }

    AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_STATUS,
            ACPI_BITMASK_ALL_FIXED_STATUS);

    /* Clear the fixed events */

    if (ACPI_VALID_ADDRESS (AcpiGbl_FADT->XPm1bEvtBlk.Address))
    {
        AcpiOsWritePort ((ACPI_IO_ADDRESS)
            ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm1bEvtBlk.Address),
            ACPI_BITMASK_ALL_FIXED_STATUS, 16);
    }

    /* Clear the GPE Bits */

    for (GpeBlock = 0; GpeBlock < ACPI_MAX_GPE_BLOCKS; GpeBlock++)
    {
        for (i = 0; i < AcpiGbl_GpeBlockInfo[GpeBlock].RegisterCount; i++)
        {
            AcpiOsWritePort ((ACPI_IO_ADDRESS)
                (AcpiGbl_GpeBlockInfo[GpeBlock].BlockAddress + i),
                0xFF, 8);
        }
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwGetSleepTypeData
 *
 * PARAMETERS:  SleepState          - Numeric sleep state
 *              *SleepTypeA         - Where SLP_TYPa is returned
 *              *SleepTypeB         - Where SLP_TYPb is returned
 *
 * RETURN:      Status - ACPI status
 *
 * DESCRIPTION: Obtain the SLP_TYPa and SLP_TYPb values for the requested sleep
 *              state.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwGetSleepTypeData (
    UINT8                   SleepState,
    UINT8                   *SleepTypeA,
    UINT8                   *SleepTypeB)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE ("HwGetSleepTypeData");


    /*
     *  Validate parameters
     */
    if ((SleepState > ACPI_S_STATES_MAX) ||
        !SleepTypeA || !SleepTypeB)
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
        ACPI_REPORT_ERROR (("Missing Sleep State object\n"));
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /*
     *  We got something, now ensure it is correct.  The object must
     *  be a package and must have at least 2 numeric values as the
     *  two elements
     */

    /* Even though AcpiEvaluateObject resolves package references,
     * NsEvaluate doesn't. So, we do it here.
     */
    Status = AcpiUtResolvePackageReferences(ObjDesc);

    if (ObjDesc->Package.Count < 2)
    {
        /* Must have at least two elements */

        ACPI_REPORT_ERROR (("Sleep State package does not have at least two elements\n"));
        Status = AE_AML_NO_OPERAND;
    }
    else if (((ObjDesc->Package.Elements[0])->Common.Type != ACPI_TYPE_INTEGER) ||
             ((ObjDesc->Package.Elements[1])->Common.Type != ACPI_TYPE_INTEGER))
    {
        /* Must have two  */

        ACPI_REPORT_ERROR (("Sleep State package elements are not both of type Number\n"));
        Status = AE_AML_OPERAND_TYPE;
    }
    else
    {
        /*
         *  Valid _Sx_ package size, type, and value
         */
        *SleepTypeA = (UINT8) (ObjDesc->Package.Elements[0])->Integer.Value;
        *SleepTypeB = (UINT8) (ObjDesc->Package.Elements[1])->Integer.Value;
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
 * FUNCTION:    AcpiHwGetRegisterBitMask
 *
 * PARAMETERS:  RegisterId      - index of ACPI Register to access
 *
 * RETURN:      The bit mask to be used when accessing the register
 *
 * DESCRIPTION: Map RegisterId into a register bit mask.
 *
 ******************************************************************************/

ACPI_BIT_REGISTER_INFO *
AcpiHwGetBitRegisterInfo (
    UINT32                  RegisterId)
{
    ACPI_FUNCTION_NAME ("HwGetBitRegisterInfo");


    if (RegisterId > ACPI_BITREG_MAX)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Invalid BitRegister ID: %X\n", RegisterId));
        return (NULL);
    }

    return (&AcpiGbl_BitRegisterInfo[RegisterId]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwBitRegisterRead
 *
 * PARAMETERS:  RegisterId      - index of ACPI Register to access
 *              UseLock         - Lock the hardware
 *
 * RETURN:      Value is read from specified Register.  Value returned is
 *              normalized to bit0 (is shifted all the way right)
 *
 * DESCRIPTION: ACPI BitRegister read function.
 *
 ******************************************************************************/

UINT32
AcpiHwBitRegisterRead (
    UINT32                  RegisterId,
    UINT32                  Flags)
{
    UINT32                  RegisterValue = 0;
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;


    ACPI_FUNCTION_TRACE ("HwBitRegisterRead");


    if (Flags & ACPI_MTX_LOCK)
    {
        if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_HARDWARE)))
        {
            return_VALUE (0);
        }
    }

    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, BitRegInfo->ParentRegister);

    if (Flags & ACPI_MTX_LOCK)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }

    /* Normalize the value that was read */

    RegisterValue = ((RegisterValue & BitRegInfo->AccessBitMask) >> BitRegInfo->BitPosition);

    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "ACPI RegisterRead: got %X\n", RegisterValue));
    return_VALUE (RegisterValue);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwBitRegisterWrite
 *
 * PARAMETERS:  RegisterId      - ID of ACPI BitRegister to access
 *              Value           - (only used on write) value to write to the
 *                                Register, NOT pre-normalized to the bit pos.
 *              Flags           - Lock the hardware or not
 *
 * RETURN:      Value written to from specified Register.  This value
 *              is shifted all the way right.
 *
 * DESCRIPTION: ACPI Bit Register write function.
 *
 ******************************************************************************/

UINT32
AcpiHwBitRegisterWrite (
    UINT32                  RegisterId,
    UINT32                  Value,
    UINT32                  Flags)
{
    UINT32                  RegisterValue = 0;
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;


    ACPI_FUNCTION_TRACE_U32 ("HwBitRegisterWrite", RegisterId);


    if (Flags & ACPI_MTX_LOCK)
    {
        if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_HARDWARE)))
        {
            return_VALUE (0);
        }
    }

    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Always do a register read first so we can insert the new bits  */

    RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, BitRegInfo->ParentRegister);

    /*
     * Decode the Register ID
     * Register id = Register block id | bit id
     *
     * Check bit id to fine locate Register offset.
     * Check Mask to determine Register offset, and then read-write.
     */
    switch (BitRegInfo->ParentRegister)
    {
    case ACPI_REGISTER_PM1_STATUS:

        /*
         * Status Registers are different from the rest.  Clear by
         * writing 1, writing 0 has no effect.  So, the only relevent
         * information is the single bit we're interested in, all others should
         * be written as 0 so they will be left unchanged
         */
        Value = ACPI_REGISTER_PREPARE_BITS (Value, BitRegInfo->BitPosition, BitRegInfo->AccessBitMask);
        if (Value)
        {
            AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_STATUS,
                (UINT16) Value);
            RegisterValue = 0;
        }
        break;


    case ACPI_REGISTER_PM1_ENABLE:

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition, BitRegInfo->AccessBitMask, Value);

        AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_ENABLE, (UINT16) RegisterValue);
        break;


    case ACPI_REGISTER_PM1_CONTROL:

        /*
         * Read the PM1 Control register.
         * Note that at this level, the fact that there are actually TWO
         * registers (A and B - and that B may not exist) is abstracted.
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM1 control: Read %X\n", RegisterValue));

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition, BitRegInfo->AccessBitMask, Value);

        AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, RegisterId,
                (UINT16) RegisterValue);
        break;


    case ACPI_REGISTER_PM2_CONTROL:

        RegisterValue = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM2_CONTROL);

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM2 control: Read %X from %8.8X%8.8X\n",
            RegisterValue, ACPI_HIDWORD (AcpiGbl_FADT->XPm2CntBlk.Address),
            ACPI_LODWORD (AcpiGbl_FADT->XPm2CntBlk.Address)));

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition, BitRegInfo->AccessBitMask, Value);

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %04X to %8.8X%8.8X\n",
            RegisterValue,
            ACPI_HIDWORD (AcpiGbl_FADT->XPm2CntBlk.Address),
            ACPI_LODWORD (AcpiGbl_FADT->XPm2CntBlk.Address)));

        AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                            ACPI_REGISTER_PM2_CONTROL, (UINT8) (RegisterValue));
        break;


    default:
        break;
    }

    if (Flags & ACPI_MTX_LOCK)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }

    /* Normalize the value that was read */

    RegisterValue = ((RegisterValue & BitRegInfo->AccessBitMask) >> BitRegInfo->BitPosition);

    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "ACPI RegisterWrite actual %X\n", RegisterValue));
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


    ACPI_FUNCTION_TRACE ("HwRegisterRead");


    if (ACPI_MTX_LOCK == UseLock)
    {
        if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_HARDWARE)))
        {
            return_VALUE (0);
        }
    }

    switch (RegisterId)
    {
    case ACPI_REGISTER_PM1_STATUS:           /* 16-bit access */

        Value =  AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1aEvtBlk, 0);
        Value |= AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1bEvtBlk, 0);
        break;


    case ACPI_REGISTER_PM1_ENABLE:           /* 16-bit access*/

        BankOffset  = ACPI_DIV_2 (AcpiGbl_FADT->Pm1EvtLen);
        Value =  AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1aEvtBlk, BankOffset);
        Value |= AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1bEvtBlk, BankOffset);
        break;


    case ACPI_REGISTER_PM1_CONTROL:          /* 16-bit access */

        Value =  AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1aCntBlk, 0);
        Value |= AcpiHwLowLevelRead (16, &AcpiGbl_FADT->XPm1bCntBlk, 0);
        break;


    case ACPI_REGISTER_PM2_CONTROL:          /* 8-bit access */

        Value =  AcpiHwLowLevelRead (8, &AcpiGbl_FADT->XPm2CntBlk, 0);
        break;


    case ACPI_REGISTER_PM_TIMER:             /* 32-bit access */

        Value =  AcpiHwLowLevelRead (32, &AcpiGbl_FADT->XPmTmrBlk, 0);
        break;

    case ACPI_REGISTER_SMI_COMMAND_BLOCK:    /* 8-bit access */

        AcpiOsReadPort (AcpiGbl_FADT->SmiCmd, &Value, 8);
        break;

    default:
        /* Value will be returned as 0 */
        break;
    }

    if (ACPI_MTX_LOCK == UseLock)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
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


    ACPI_FUNCTION_TRACE ("HwRegisterWrite");


    if (ACPI_MTX_LOCK == UseLock)
    {
        if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_HARDWARE)))
        {
            return_VOID;
        }
    }

    switch (RegisterId)
    {
    case ACPI_REGISTER_PM1_STATUS:           /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aEvtBlk, 0);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bEvtBlk, 0);
        break;


    case ACPI_REGISTER_PM1_ENABLE:           /* 16-bit access*/

        BankOffset = ACPI_DIV_2 (AcpiGbl_FADT->Pm1EvtLen);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aEvtBlk, BankOffset);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bEvtBlk, BankOffset);
        break;


    case ACPI_REGISTER_PM1_CONTROL:          /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aCntBlk, 0);
        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bCntBlk, 0);
        break;


    case ACPI_REGISTER_PM1A_CONTROL:         /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aCntBlk, 0);
        break;


    case ACPI_REGISTER_PM1B_CONTROL:         /* 16-bit access */

        AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bCntBlk, 0);
        break;


    case ACPI_REGISTER_PM2_CONTROL:          /* 8-bit access */

        AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT->XPm2CntBlk, 0);
        break;


    case ACPI_REGISTER_PM_TIMER:             /* 32-bit access */

        AcpiHwLowLevelWrite (32, Value, &AcpiGbl_FADT->XPmTmrBlk, 0);
        break;


    case ACPI_REGISTER_SMI_COMMAND_BLOCK:    /* 8-bit access */

        /* SMI_CMD is currently always in IO space */

        AcpiOsWritePort (AcpiGbl_FADT->SmiCmd, Value, 8);
        break;


    default:
        Value = 0;
        break;
    }

    if (ACPI_MTX_LOCK == UseLock)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
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


    ACPI_FUNCTION_ENTRY ();


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


    ACPI_FUNCTION_ENTRY ();


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
