
/******************************************************************************
 *
 * Module Name: hwxface - Public ACPICA hardware interfaces
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

#include "acpi.h"
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwxface")


/******************************************************************************
 *
 * FUNCTION:    AcpiReset
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Set reset register in memory or IO space. Note: Does not
 *              support reset register in PCI config space, this must be
 *              handled separately.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiReset (
    void)
{
    ACPI_GENERIC_ADDRESS    *ResetReg;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiReset);


    ResetReg = &AcpiGbl_FADT.ResetRegister;

    /* Check if the reset register is supported */

    if (!(AcpiGbl_FADT.Flags & ACPI_FADT_RESET_REGISTER) ||
        !ResetReg->Address)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    if (ResetReg->SpaceId == ACPI_ADR_SPACE_SYSTEM_IO)
    {
        /*
         * For I/O space, write directly to the OSL. This bypasses the port
         * validation mechanism, which may block a valid write to the reset
         * register.
         */
        Status = AcpiOsWritePort ((ACPI_IO_ADDRESS) ResetReg->Address,
                    AcpiGbl_FADT.ResetValue, ResetReg->BitWidth);
    }
    else
    {
        /* Write the reset value to the reset register */

        Status = AcpiHwWrite (AcpiGbl_FADT.ResetValue, ResetReg);
    }

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiReset)


/******************************************************************************
 *
 * FUNCTION:    AcpiRead
 *
 * PARAMETERS:  Value               - Where the value is returned
 *              Reg                 - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read from either memory or IO space.
 *
 * LIMITATIONS: <These limitations also apply to AcpiWrite>
 *      BitWidth must be exactly 8, 16, 32, or 64.
 *      SpaceID must be SystemMemory or SystemIO.
 *      BitOffset and AccessWidth are currently ignored, as there has
 *          not been a need to implement these.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRead (
    UINT64                  *ReturnValue,
    ACPI_GENERIC_ADDRESS    *Reg)
{
    UINT32                  Value;
    UINT32                  Width;
    UINT64                  Address;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_NAME (AcpiRead);


    if (!ReturnValue)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Validate contents of the GAS register. Allow 64-bit transfers */

    Status = AcpiHwValidateRegister (Reg, 64, &Address);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Width = Reg->BitWidth;
    if (Width == 64)
    {
        Width = 32; /* Break into two 32-bit transfers */
    }

    /* Initialize entire 64-bit return value to zero */

    *ReturnValue = 0;
    Value = 0;

    /*
     * Two address spaces supported: Memory or IO. PCI_Config is
     * not supported here because the GAS structure is insufficient
     */
    if (Reg->SpaceId == ACPI_ADR_SPACE_SYSTEM_MEMORY)
    {
        Status = AcpiOsReadMemory ((ACPI_PHYSICAL_ADDRESS)
                    Address, &Value, Width);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        *ReturnValue = Value;

        if (Reg->BitWidth == 64)
        {
            /* Read the top 32 bits */

            Status = AcpiOsReadMemory ((ACPI_PHYSICAL_ADDRESS)
                        (Address + 4), &Value, 32);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            *ReturnValue |= ((UINT64) Value << 32);
        }
    }
    else /* ACPI_ADR_SPACE_SYSTEM_IO, validated earlier */
    {
        Status = AcpiHwReadPort ((ACPI_IO_ADDRESS)
                    Address, &Value, Width);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        *ReturnValue = Value;

        if (Reg->BitWidth == 64)
        {
            /* Read the top 32 bits */

            Status = AcpiHwReadPort ((ACPI_IO_ADDRESS)
                        (Address + 4), &Value, 32);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            *ReturnValue |= ((UINT64) Value << 32);
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_IO,
        "Read:  %8.8X%8.8X width %2d from %8.8X%8.8X (%s)\n",
        ACPI_FORMAT_UINT64 (*ReturnValue), Reg->BitWidth,
        ACPI_FORMAT_UINT64 (Address),
        AcpiUtGetRegionName (Reg->SpaceId)));

    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiRead)


/******************************************************************************
 *
 * FUNCTION:    AcpiWrite
 *
 * PARAMETERS:  Value               - Value to be written
 *              Reg                 - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to either memory or IO space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiWrite (
    UINT64                  Value,
    ACPI_GENERIC_ADDRESS    *Reg)
{
    UINT32                  Width;
    UINT64                  Address;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_NAME (AcpiWrite);


    /* Validate contents of the GAS register. Allow 64-bit transfers */

    Status = AcpiHwValidateRegister (Reg, 64, &Address);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Width = Reg->BitWidth;
    if (Width == 64)
    {
        Width = 32; /* Break into two 32-bit transfers */
    }

    /*
     * Two address spaces supported: Memory or IO. PCI_Config is
     * not supported here because the GAS structure is insufficient
     */
    if (Reg->SpaceId == ACPI_ADR_SPACE_SYSTEM_MEMORY)
    {
        Status = AcpiOsWriteMemory ((ACPI_PHYSICAL_ADDRESS)
                    Address, ACPI_LODWORD (Value), Width);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        if (Reg->BitWidth == 64)
        {
            Status = AcpiOsWriteMemory ((ACPI_PHYSICAL_ADDRESS)
                        (Address + 4), ACPI_HIDWORD (Value), 32);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }
    else /* ACPI_ADR_SPACE_SYSTEM_IO, validated earlier */
    {
        Status = AcpiHwWritePort ((ACPI_IO_ADDRESS)
                    Address, ACPI_LODWORD (Value), Width);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        if (Reg->BitWidth == 64)
        {
            Status = AcpiHwWritePort ((ACPI_IO_ADDRESS)
                        (Address + 4), ACPI_HIDWORD (Value), 32);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_IO,
        "Wrote: %8.8X%8.8X width %2d   to %8.8X%8.8X (%s)\n",
        ACPI_FORMAT_UINT64 (Value), Reg->BitWidth,
        ACPI_FORMAT_UINT64 (Address),
        AcpiUtGetRegionName (Reg->SpaceId)));

    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiWrite)


/*******************************************************************************
 *
 * FUNCTION:    AcpiReadBitRegister
 *
 * PARAMETERS:  RegisterId      - ID of ACPI Bit Register to access
 *              ReturnValue     - Value that was read from the register,
 *                                normalized to bit position zero.
 *
 * RETURN:      Status and the value read from the specified Register. Value
 *              returned is normalized to bit0 (is shifted all the way right)
 *
 * DESCRIPTION: ACPI BitRegister read function. Does not acquire the HW lock.
 *
 * SUPPORTS:    Bit fields in PM1 Status, PM1 Enable, PM1 Control, and
 *              PM2 Control.
 *
 * Note: The hardware lock is not required when reading the ACPI bit registers
 *       since almost all of them are single bit and it does not matter that
 *       the parent hardware register can be split across two physical
 *       registers. The only multi-bit field is SLP_TYP in the PM1 control
 *       register, but this field does not cross an 8-bit boundary (nor does
 *       it make much sense to actually read this field.)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiReadBitRegister (
    UINT32                  RegisterId,
    UINT32                  *ReturnValue)
{
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;
    UINT32                  RegisterValue;
    UINT32                  Value;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_U32 (AcpiReadBitRegister, RegisterId);


    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Read the entire parent register */

    Status = AcpiHwRegisterRead (BitRegInfo->ParentRegister,
                &RegisterValue);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Normalize the value that was read, mask off other bits */

    Value = ((RegisterValue & BitRegInfo->AccessBitMask)
                >> BitRegInfo->BitPosition);

    ACPI_DEBUG_PRINT ((ACPI_DB_IO,
        "BitReg %X, ParentReg %X, Actual %8.8X, ReturnValue %8.8X\n",
        RegisterId, BitRegInfo->ParentRegister, RegisterValue, Value));

    *ReturnValue = Value;
    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiReadBitRegister)


/*******************************************************************************
 *
 * FUNCTION:    AcpiWriteBitRegister
 *
 * PARAMETERS:  RegisterId      - ID of ACPI Bit Register to access
 *              Value           - Value to write to the register, in bit
 *                                position zero. The bit is automaticallly
 *                                shifted to the correct position.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: ACPI Bit Register write function. Acquires the hardware lock
 *              since most operations require a read/modify/write sequence.
 *
 * SUPPORTS:    Bit fields in PM1 Status, PM1 Enable, PM1 Control, and
 *              PM2 Control.
 *
 * Note that at this level, the fact that there may be actually two
 * hardware registers (A and B - and B may not exist) is abstracted.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiWriteBitRegister (
    UINT32                  RegisterId,
    UINT32                  Value)
{
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;
    ACPI_CPU_FLAGS          LockFlags;
    UINT32                  RegisterValue;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_U32 (AcpiWriteBitRegister, RegisterId);


    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    LockFlags = AcpiOsAcquireLock (AcpiGbl_HardwareLock);

    /*
     * At this point, we know that the parent register is one of the
     * following: PM1 Status, PM1 Enable, PM1 Control, or PM2 Control
     */
    if (BitRegInfo->ParentRegister != ACPI_REGISTER_PM1_STATUS)
    {
        /*
         * 1) Case for PM1 Enable, PM1 Control, and PM2 Control
         *
         * Perform a register read to preserve the bits that we are not
         * interested in
         */
        Status = AcpiHwRegisterRead (BitRegInfo->ParentRegister,
                    &RegisterValue);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /*
         * Insert the input bit into the value that was just read
         * and write the register
         */
        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition,
            BitRegInfo->AccessBitMask, Value);

        Status = AcpiHwRegisterWrite (BitRegInfo->ParentRegister,
                    RegisterValue);
    }
    else
    {
        /*
         * 2) Case for PM1 Status
         *
         * The Status register is different from the rest. Clear an event
         * by writing 1, writing 0 has no effect. So, the only relevant
         * information is the single bit we're interested in, all others
         * should be written as 0 so they will be left unchanged.
         */
        RegisterValue = ACPI_REGISTER_PREPARE_BITS (Value,
            BitRegInfo->BitPosition, BitRegInfo->AccessBitMask);

        /* No need to write the register if value is all zeros */

        if (RegisterValue)
        {
            Status = AcpiHwRegisterWrite (ACPI_REGISTER_PM1_STATUS,
                        RegisterValue);
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_IO,
        "BitReg %X, ParentReg %X, Value %8.8X, Actual %8.8X\n",
        RegisterId, BitRegInfo->ParentRegister, Value, RegisterValue));


UnlockAndExit:

    AcpiOsReleaseLock (AcpiGbl_HardwareLock, LockFlags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiWriteBitRegister)


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetSleepTypeData
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
AcpiGetSleepTypeData (
    UINT8                   SleepState,
    UINT8                   *SleepTypeA,
    UINT8                   *SleepTypeB)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_EVALUATE_INFO      *Info;


    ACPI_FUNCTION_TRACE (AcpiGetSleepTypeData);


    /* Validate parameters */

    if ((SleepState > ACPI_S_STATES_MAX) ||
        !SleepTypeA ||
        !SleepTypeB)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Allocate the evaluation information block */

    Info = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EVALUATE_INFO));
    if (!Info)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Info->Pathname = ACPI_CAST_PTR (char, AcpiGbl_SleepStateNames[SleepState]);

    /* Evaluate the namespace object containing the values for this state */

    Status = AcpiNsEvaluate (Info);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "%s while evaluating SleepState [%s]\n",
            AcpiFormatException (Status), Info->Pathname));

        goto Cleanup;
    }

    /* Must have a return object */

    if (!Info->ReturnObject)
    {
        ACPI_ERROR ((AE_INFO, "No Sleep State object returned from [%s]",
            Info->Pathname));
        Status = AE_NOT_EXIST;
    }

    /* It must be of type Package */

    else if (Info->ReturnObject->Common.Type != ACPI_TYPE_PACKAGE)
    {
        ACPI_ERROR ((AE_INFO, "Sleep State return object is not a Package"));
        Status = AE_AML_OPERAND_TYPE;
    }

    /*
     * The package must have at least two elements. NOTE (March 2005): This
     * goes against the current ACPI spec which defines this object as a
     * package with one encoded DWORD element. However, existing practice
     * by BIOS vendors seems to be to have 2 or more elements, at least
     * one per sleep type (A/B).
     */
    else if (Info->ReturnObject->Package.Count < 2)
    {
        ACPI_ERROR ((AE_INFO,
            "Sleep State return package does not have at least two elements"));
        Status = AE_AML_NO_OPERAND;
    }

    /* The first two elements must both be of type Integer */

    else if (((Info->ReturnObject->Package.Elements[0])->Common.Type
                != ACPI_TYPE_INTEGER) ||
             ((Info->ReturnObject->Package.Elements[1])->Common.Type
                != ACPI_TYPE_INTEGER))
    {
        ACPI_ERROR ((AE_INFO,
            "Sleep State return package elements are not both Integers "
            "(%s, %s)",
            AcpiUtGetObjectTypeName (Info->ReturnObject->Package.Elements[0]),
            AcpiUtGetObjectTypeName (Info->ReturnObject->Package.Elements[1])));
        Status = AE_AML_OPERAND_TYPE;
    }
    else
    {
        /* Valid _Sx_ package size, type, and value */

        *SleepTypeA = (UINT8)
            (Info->ReturnObject->Package.Elements[0])->Integer.Value;
        *SleepTypeB = (UINT8)
            (Info->ReturnObject->Package.Elements[1])->Integer.Value;
    }

    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "While evaluating SleepState [%s], bad Sleep object %p type %s",
            Info->Pathname, Info->ReturnObject,
            AcpiUtGetObjectTypeName (Info->ReturnObject)));
    }

    AcpiUtRemoveReference (Info->ReturnObject);

Cleanup:
    ACPI_FREE (Info);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetSleepTypeData)
