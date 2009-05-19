
/*******************************************************************************
 *
 * Module Name: hwregs - Read/write access functions for the various ACPI
 *                       control and status registers.
 *              $Revision: 1.187 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
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

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acevents.h>

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwregs")


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwClearAcpiStatus
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Clears all fixed and general purpose status bits
 *              THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwClearAcpiStatus (
    void)
{
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          LockFlags = 0;


    ACPI_FUNCTION_TRACE (HwClearAcpiStatus);


    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %04X to %04X\n",
        ACPI_BITMASK_ALL_FIXED_STATUS,
        (UINT16) AcpiGbl_FADT.XPm1aEventBlock.Address));

    LockFlags = AcpiOsAcquireLock (AcpiGbl_HardwareLock);

    Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                ACPI_REGISTER_PM1_STATUS,
                ACPI_BITMASK_ALL_FIXED_STATUS);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    /* Clear the fixed events */

    if (AcpiGbl_FADT.XPm1bEventBlock.Address)
    {
        Status = AcpiHwLowLevelWrite (16, ACPI_BITMASK_ALL_FIXED_STATUS,
                    &AcpiGbl_FADT.XPm1bEventBlock);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }
    }

    /* Clear the GPE Bits in all GPE registers in all GPE blocks */

    Status = AcpiEvWalkGpeList (AcpiHwClearGpeBlock);

UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_HardwareLock, LockFlags);
    return_ACPI_STATUS (Status);
}


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
        !SleepTypeA || !SleepTypeB)
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

    else if (ACPI_GET_OBJECT_TYPE (Info->ReturnObject) != ACPI_TYPE_PACKAGE)
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

    else if ((ACPI_GET_OBJECT_TYPE (Info->ReturnObject->Package.Elements[0])
                != ACPI_TYPE_INTEGER) ||
             (ACPI_GET_OBJECT_TYPE (Info->ReturnObject->Package.Elements[1])
                != ACPI_TYPE_INTEGER))
    {
        ACPI_ERROR ((AE_INFO,
            "Sleep State return package elements are not both Integers (%s, %s)",
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


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwGetRegisterBitMask
 *
 * PARAMETERS:  RegisterId          - Index of ACPI Register to access
 *
 * RETURN:      The bitmask to be used when accessing the register
 *
 * DESCRIPTION: Map RegisterId into a register bitmask.
 *
 ******************************************************************************/

ACPI_BIT_REGISTER_INFO *
AcpiHwGetBitRegisterInfo (
    UINT32                  RegisterId)
{
    ACPI_FUNCTION_ENTRY ();


    if (RegisterId > ACPI_BITREG_MAX)
    {
        ACPI_ERROR ((AE_INFO, "Invalid BitRegister ID: %X", RegisterId));
        return (NULL);
    }

    return (&AcpiGbl_BitRegisterInfo[RegisterId]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetRegister
 *
 * PARAMETERS:  RegisterId      - ID of ACPI BitRegister to access
 *              ReturnValue     - Value that was read from the register
 *
 * RETURN:      Status and the value read from specified Register. Value
 *              returned is normalized to bit0 (is shifted all the way right)
 *
 * DESCRIPTION: ACPI BitRegister read function.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetRegister (
    UINT32                  RegisterId,
    UINT32                  *ReturnValue)
{
    UINT32                  RegisterValue = 0;
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiGetRegister);


    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Read from the register */

    Status = AcpiHwRegisterRead (ACPI_MTX_LOCK,
                BitRegInfo->ParentRegister, &RegisterValue);

    if (ACPI_SUCCESS (Status))
    {
        /* Normalize the value that was read */

        RegisterValue = ((RegisterValue & BitRegInfo->AccessBitMask)
                            >> BitRegInfo->BitPosition);

        *ReturnValue = RegisterValue;

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "Read value %8.8X register %X\n",
            RegisterValue, BitRegInfo->ParentRegister));
    }

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetRegister)


/*******************************************************************************
 *
 * FUNCTION:    AcpiSetRegister
 *
 * PARAMETERS:  RegisterId      - ID of ACPI BitRegister to access
 *              Value           - (only used on write) value to write to the
 *                                Register, NOT pre-normalized to the bit pos
 *
 * RETURN:      Status
 *
 * DESCRIPTION: ACPI Bit Register write function.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetRegister (
    UINT32                  RegisterId,
    UINT32                  Value)
{
    UINT32                  RegisterValue = 0;
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          LockFlags;


    ACPI_FUNCTION_TRACE_U32 (AcpiSetRegister, RegisterId);


    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        ACPI_ERROR ((AE_INFO, "Bad ACPI HW RegisterId: %X", RegisterId));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    LockFlags = AcpiOsAcquireLock (AcpiGbl_HardwareLock);

    /* Always do a register read first so we can insert the new bits  */

    Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                BitRegInfo->ParentRegister, &RegisterValue);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    /*
     * Decode the Register ID
     * Register ID = [Register block ID] | [bit ID]
     *
     * Check bit ID to fine locate Register offset.
     * Check Mask to determine Register offset, and then read-write.
     */
    switch (BitRegInfo->ParentRegister)
    {
    case ACPI_REGISTER_PM1_STATUS:

        /*
         * Status Registers are different from the rest. Clear by
         * writing 1, and writing 0 has no effect. So, the only relevant
         * information is the single bit we're interested in, all others should
         * be written as 0 so they will be left unchanged.
         */
        Value = ACPI_REGISTER_PREPARE_BITS (Value,
                    BitRegInfo->BitPosition, BitRegInfo->AccessBitMask);
        if (Value)
        {
            Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                        ACPI_REGISTER_PM1_STATUS, (UINT16) Value);
            RegisterValue = 0;
        }
        break;


    case ACPI_REGISTER_PM1_ENABLE:

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition,
            BitRegInfo->AccessBitMask, Value);

        Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                    ACPI_REGISTER_PM1_ENABLE, (UINT16) RegisterValue);
        break;


    case ACPI_REGISTER_PM1_CONTROL:

        /*
         * Write the PM1 Control register.
         * Note that at this level, the fact that there are actually TWO
         * registers (A and B - and B may not exist) is abstracted.
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM1 control: Read %X\n",
            RegisterValue));

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition,
            BitRegInfo->AccessBitMask, Value);

        Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                    ACPI_REGISTER_PM1_CONTROL, (UINT16) RegisterValue);
        break;


    case ACPI_REGISTER_PM2_CONTROL:

        Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                    ACPI_REGISTER_PM2_CONTROL, &RegisterValue);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM2 control: Read %X from %8.8X%8.8X\n",
            RegisterValue,
            ACPI_FORMAT_UINT64 (AcpiGbl_FADT.XPm2ControlBlock.Address)));

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition,
                BitRegInfo->AccessBitMask, Value);

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %4.4X to %8.8X%8.8X\n",
            RegisterValue,
            ACPI_FORMAT_UINT64 (AcpiGbl_FADT.XPm2ControlBlock.Address)));

        Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                    ACPI_REGISTER_PM2_CONTROL, (UINT8) (RegisterValue));
        break;


    default:
        break;
    }


UnlockAndExit:

    AcpiOsReleaseLock (AcpiGbl_HardwareLock, LockFlags);

    /* Normalize the value that was read */

    ACPI_DEBUG_EXEC (RegisterValue =
        ((RegisterValue & BitRegInfo->AccessBitMask) >>
            BitRegInfo->BitPosition));

    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "Set bits: %8.8X actual %8.8X register %X\n",
        Value, RegisterValue, BitRegInfo->ParentRegister));
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiSetRegister)


/******************************************************************************
 *
 * FUNCTION:    AcpiHwRegisterRead
 *
 * PARAMETERS:  UseLock             - Lock hardware? True/False
 *              RegisterId          - ACPI Register ID
 *              ReturnValue         - Where the register value is returned
 *
 * RETURN:      Status and the value read.
 *
 * DESCRIPTION: Read from the specified ACPI register
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwRegisterRead (
    BOOLEAN                 UseLock,
    UINT32                  RegisterId,
    UINT32                  *ReturnValue)
{
    UINT32                  Value1 = 0;
    UINT32                  Value2 = 0;
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          LockFlags = 0;


    ACPI_FUNCTION_TRACE (HwRegisterRead);


    if (ACPI_MTX_LOCK == UseLock)
    {
        LockFlags = AcpiOsAcquireLock (AcpiGbl_HardwareLock);
    }

    switch (RegisterId)
    {
    case ACPI_REGISTER_PM1_STATUS:           /* 16-bit access */

        Status = AcpiHwLowLevelRead (16, &Value1, &AcpiGbl_FADT.XPm1aEventBlock);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* PM1B is optional */

        Status = AcpiHwLowLevelRead (16, &Value2, &AcpiGbl_FADT.XPm1bEventBlock);
        Value1 |= Value2;
        break;


    case ACPI_REGISTER_PM1_ENABLE:           /* 16-bit access */

        Status = AcpiHwLowLevelRead (16, &Value1, &AcpiGbl_XPm1aEnable);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* PM1B is optional */

        Status = AcpiHwLowLevelRead (16, &Value2, &AcpiGbl_XPm1bEnable);
        Value1 |= Value2;
        break;


    case ACPI_REGISTER_PM1_CONTROL:          /* 16-bit access */

        Status = AcpiHwLowLevelRead (16, &Value1, &AcpiGbl_FADT.XPm1aControlBlock);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        Status = AcpiHwLowLevelRead (16, &Value2, &AcpiGbl_FADT.XPm1bControlBlock);
        Value1 |= Value2;
        break;


    case ACPI_REGISTER_PM2_CONTROL:          /* 8-bit access */

        Status = AcpiHwLowLevelRead (8, &Value1, &AcpiGbl_FADT.XPm2ControlBlock);
        break;


    case ACPI_REGISTER_PM_TIMER:             /* 32-bit access */

        Status = AcpiHwLowLevelRead (32, &Value1, &AcpiGbl_FADT.XPmTimerBlock);
        break;

    case ACPI_REGISTER_SMI_COMMAND_BLOCK:    /* 8-bit access */

        Status = AcpiOsReadPort (AcpiGbl_FADT.SmiCommand, &Value1, 8);
        break;

    default:
        ACPI_ERROR ((AE_INFO, "Unknown Register ID: %X",
            RegisterId));
        Status = AE_BAD_PARAMETER;
        break;
    }

UnlockAndExit:
    if (ACPI_MTX_LOCK == UseLock)
    {
        AcpiOsReleaseLock (AcpiGbl_HardwareLock, LockFlags);
    }

    if (ACPI_SUCCESS (Status))
    {
        *ReturnValue = Value1;
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwRegisterWrite
 *
 * PARAMETERS:  UseLock             - Lock hardware? True/False
 *              RegisterId          - ACPI Register ID
 *              Value               - The value to write
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to the specified ACPI register
 *
 * NOTE: In accordance with the ACPI specification, this function automatically
 * preserves the value of the following bits, meaning that these bits cannot be
 * changed via this interface:
 *
 * PM1_CONTROL[0] = SCI_EN
 * PM1_CONTROL[9]
 * PM1_STATUS[11]
 *
 * ACPI References:
 * 1) Hardware Ignored Bits: When software writes to a register with ignored
 *      bit fields, it preserves the ignored bit fields
 * 2) SCI_EN: OSPM always preserves this bit position
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwRegisterWrite (
    BOOLEAN                 UseLock,
    UINT32                  RegisterId,
    UINT32                  Value)
{
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          LockFlags = 0;
    UINT32                  ReadValue;


    ACPI_FUNCTION_TRACE (HwRegisterWrite);


    if (ACPI_MTX_LOCK == UseLock)
    {
        LockFlags = AcpiOsAcquireLock (AcpiGbl_HardwareLock);
    }

    switch (RegisterId)
    {
    case ACPI_REGISTER_PM1_STATUS:           /* 16-bit access */

        /* Perform a read first to preserve certain bits (per ACPI spec) */

        Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                    ACPI_REGISTER_PM1_STATUS, &ReadValue);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* Insert the bits to be preserved */

        ACPI_INSERT_BITS (Value, ACPI_PM1_STATUS_PRESERVED_BITS, ReadValue);

        /* Now we can write the data */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT.XPm1aEventBlock);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* PM1B is optional */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT.XPm1bEventBlock);
        break;


    case ACPI_REGISTER_PM1_ENABLE:           /* 16-bit access */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_XPm1aEnable);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* PM1B is optional */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_XPm1bEnable);
        break;


    case ACPI_REGISTER_PM1_CONTROL:          /* 16-bit access */

        /*
         * Perform a read first to preserve certain bits (per ACPI spec)
         *
         * Note: This includes SCI_EN, we never want to change this bit
         */
        Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                    ACPI_REGISTER_PM1_CONTROL, &ReadValue);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* Insert the bits to be preserved */

        ACPI_INSERT_BITS (Value, ACPI_PM1_CONTROL_PRESERVED_BITS, ReadValue);

        /* Now we can write the data */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT.XPm1aControlBlock);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT.XPm1bControlBlock);
        break;


    case ACPI_REGISTER_PM1A_CONTROL:         /* 16-bit access */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT.XPm1aControlBlock);
        break;


    case ACPI_REGISTER_PM1B_CONTROL:         /* 16-bit access */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT.XPm1bControlBlock);
        break;


    case ACPI_REGISTER_PM2_CONTROL:          /* 8-bit access */

        Status = AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT.XPm2ControlBlock);
        break;


    case ACPI_REGISTER_PM_TIMER:             /* 32-bit access */

        Status = AcpiHwLowLevelWrite (32, Value, &AcpiGbl_FADT.XPmTimerBlock);
        break;


    case ACPI_REGISTER_SMI_COMMAND_BLOCK:    /* 8-bit access */

        /* SMI_CMD is currently always in IO space */

        Status = AcpiOsWritePort (AcpiGbl_FADT.SmiCommand, Value, 8);
        break;


    default:
        Status = AE_BAD_PARAMETER;
        break;
    }

UnlockAndExit:
    if (ACPI_MTX_LOCK == UseLock)
    {
        AcpiOsReleaseLock (AcpiGbl_HardwareLock, LockFlags);
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwLowLevelRead
 *
 * PARAMETERS:  Width               - 8, 16, or 32
 *              Value               - Where the value is returned
 *              Reg                 - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read from either memory or IO space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwLowLevelRead (
    UINT32                  Width,
    UINT32                  *Value,
    ACPI_GENERIC_ADDRESS    *Reg)
{
    UINT64                  Address;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_NAME (HwLowLevelRead);


    /*
     * Must have a valid pointer to a GAS structure, and
     * a non-zero address within. However, don't return an error
     * because the PM1A/B code must not fail if B isn't present.
     */
    if (!Reg)
    {
        return (AE_OK);
    }

    /* Get a local copy of the address. Handles possible alignment issues */

    ACPI_MOVE_64_TO_64 (&Address, &Reg->Address);
    if (!Address)
    {
        return (AE_OK);
    }
    *Value = 0;

    /*
     * Two address spaces supported: Memory or IO.
     * PCI_Config is not supported here because the GAS struct is insufficient
     */
    switch (Reg->SpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_MEMORY:

        Status = AcpiOsReadMemory (
                    (ACPI_PHYSICAL_ADDRESS) Address, Value, Width);
        break;


    case ACPI_ADR_SPACE_SYSTEM_IO:

        Status = AcpiOsReadPort ((ACPI_IO_ADDRESS) Address, Value, Width);
        break;


    default:
        ACPI_ERROR ((AE_INFO,
            "Unsupported address space: %X", Reg->SpaceId));
        return (AE_BAD_PARAMETER);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_IO,
        "Read:  %8.8X width %2d from %8.8X%8.8X (%s)\n",
        *Value, Width, ACPI_FORMAT_UINT64 (Address),
        AcpiUtGetRegionName (Reg->SpaceId)));

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwLowLevelWrite
 *
 * PARAMETERS:  Width               - 8, 16, or 32
 *              Value               - To be written
 *              Reg                 - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to either memory or IO space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwLowLevelWrite (
    UINT32                  Width,
    UINT32                  Value,
    ACPI_GENERIC_ADDRESS    *Reg)
{
    UINT64                  Address;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_NAME (HwLowLevelWrite);


    /*
     * Must have a valid pointer to a GAS structure, and
     * a non-zero address within. However, don't return an error
     * because the PM1A/B code must not fail if B isn't present.
     */
    if (!Reg)
    {
        return (AE_OK);
    }

    /* Get a local copy of the address. Handles possible alignment issues */

    ACPI_MOVE_64_TO_64 (&Address, &Reg->Address);
    if (!Address)
    {
        return (AE_OK);
    }

    /*
     * Two address spaces supported: Memory or IO.
     * PCI_Config is not supported here because the GAS struct is insufficient
     */
    switch (Reg->SpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_MEMORY:

        Status = AcpiOsWriteMemory (
                    (ACPI_PHYSICAL_ADDRESS) Address, Value, Width);
        break;


    case ACPI_ADR_SPACE_SYSTEM_IO:

        Status = AcpiOsWritePort (
                    (ACPI_IO_ADDRESS) Address, Value, Width);
        break;


    default:
        ACPI_ERROR ((AE_INFO,
            "Unsupported address space: %X", Reg->SpaceId));
        return (AE_BAD_PARAMETER);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_IO,
        "Wrote: %8.8X width %2d   to %8.8X%8.8X (%s)\n",
        Value, Width, ACPI_FORMAT_UINT64 (Address),
        AcpiUtGetRegionName (Reg->SpaceId)));

    return (Status);
}
