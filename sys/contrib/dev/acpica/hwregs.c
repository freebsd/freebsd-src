
/*******************************************************************************
 *
 * Module Name: hwregs - Read/write access functions for the various ACPI
 *                       control and status registers.
 *              $Revision: 149 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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
#include "acnamesp.h"
#include "acevents.h"

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwregs")


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwClearAcpiStatus
 *
 * PARAMETERS:  Flags           - Lock the hardware or not
 *
 * RETURN:      none
 *
 * DESCRIPTION: Clears all fixed and general purpose status bits
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwClearAcpiStatus (
    UINT32                  Flags)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("HwClearAcpiStatus");


    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %04X to %04X\n",
        ACPI_BITMASK_ALL_FIXED_STATUS,
        (UINT16) ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm1aEvtBlk.Address)));

    if (Flags & ACPI_MTX_LOCK)
    {
        Status = AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_STATUS,
                    ACPI_BITMASK_ALL_FIXED_STATUS);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    /* Clear the fixed events */

    if (ACPI_VALID_ADDRESS (AcpiGbl_FADT->XPm1bEvtBlk.Address))
    {
        Status = AcpiHwLowLevelWrite (16, ACPI_BITMASK_ALL_FIXED_STATUS,
                    &AcpiGbl_FADT->XPm1bEvtBlk);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }
    }

    /* Clear the GPE Bits in all GPE registers in all GPE blocks */

    Status = AcpiEvWalkGpeList (AcpiHwClearGpeBlock);

UnlockAndExit:
    if (Flags & ACPI_MTX_LOCK)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }
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
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE ("AcpiGetSleepTypeData");


    /*
     * Validate parameters
     */
    if ((SleepState > ACPI_S_STATES_MAX) ||
        !SleepTypeA || !SleepTypeB)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Evaluate the namespace object containing the values for this state
     */
    Status = AcpiNsEvaluateByName ((char *) AcpiGbl_DbSleepStates[SleepState],
                    NULL, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "%s while evaluating SleepState [%s]\n",
            AcpiFormatException (Status), AcpiGbl_DbSleepStates[SleepState]));

        return_ACPI_STATUS (Status);
    }

    /* Must have a return object */

    if (!ObjDesc)
    {
        ACPI_REPORT_ERROR (("Missing Sleep State object\n"));
        Status = AE_NOT_EXIST;
    }

    /* It must be of type Package */

    else if (ACPI_GET_OBJECT_TYPE (ObjDesc) != ACPI_TYPE_PACKAGE)
    {
        ACPI_REPORT_ERROR (("Sleep State object not a Package\n"));
        Status = AE_AML_OPERAND_TYPE;
    }

    /* The package must have at least two elements */

    else if (ObjDesc->Package.Count < 2)
    {
        ACPI_REPORT_ERROR (("Sleep State package does not have at least two elements\n"));
        Status = AE_AML_NO_OPERAND;
    }

    /* The first two elements must both be of type Integer */

    else if ((ACPI_GET_OBJECT_TYPE (ObjDesc->Package.Elements[0]) != ACPI_TYPE_INTEGER) ||
             (ACPI_GET_OBJECT_TYPE (ObjDesc->Package.Elements[1]) != ACPI_TYPE_INTEGER))
    {
        ACPI_REPORT_ERROR (("Sleep State package elements are not both Integers (%s, %s)\n",
            AcpiUtGetObjectTypeName (ObjDesc->Package.Elements[0]),
            AcpiUtGetObjectTypeName (ObjDesc->Package.Elements[1])));
        Status = AE_AML_OPERAND_TYPE;
    }
    else
    {
        /*
         * Valid _Sx_ package size, type, and value
         */
        *SleepTypeA = (UINT8) (ObjDesc->Package.Elements[0])->Integer.Value;
        *SleepTypeB = (UINT8) (ObjDesc->Package.Elements[1])->Integer.Value;
    }

    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "While evaluating SleepState [%s], bad Sleep object %p type %s\n",
            AcpiGbl_DbSleepStates[SleepState], ObjDesc, AcpiUtGetObjectTypeName (ObjDesc)));
    }

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwGetRegisterBitMask
 *
 * PARAMETERS:  RegisterId          - Index of ACPI Register to access
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
 * FUNCTION:    AcpiGetRegister
 *
 * PARAMETERS:  RegisterId      - ID of ACPI BitRegister to access
 *              ReturnValue     - Value that was read from the register
 *              Flags           - Lock the hardware or not
 *
 * RETURN:      Value is read from specified Register.  Value returned is
 *              normalized to bit0 (is shifted all the way right)
 *
 * DESCRIPTION: ACPI BitRegister read function.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetRegister (
    UINT32                  RegisterId,
    UINT32                  *ReturnValue,
    UINT32                  Flags)
{
    UINT32                  RegisterValue = 0;
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiGetRegister");


    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (Flags & ACPI_MTX_LOCK)
    {
        Status = AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                    BitRegInfo->ParentRegister, &RegisterValue);

    if (Flags & ACPI_MTX_LOCK)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }

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


/*******************************************************************************
 *
 * FUNCTION:    AcpiSetRegister
 *
 * PARAMETERS:  RegisterId      - ID of ACPI BitRegister to access
 *              Value           - (only used on write) value to write to the
 *                                Register, NOT pre-normalized to the bit pos.
 *              Flags           - Lock the hardware or not
 *
 * RETURN:      None
 *
 * DESCRIPTION: ACPI Bit Register write function.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetRegister (
    UINT32                  RegisterId,
    UINT32                  Value,
    UINT32                  Flags)
{
    UINT32                  RegisterValue = 0;
    ACPI_BIT_REGISTER_INFO  *BitRegInfo;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_U32 ("AcpiSetRegister", RegisterId);


    /* Get the info structure corresponding to the requested ACPI Register */

    BitRegInfo = AcpiHwGetBitRegisterInfo (RegisterId);
    if (!BitRegInfo)
    {
        ACPI_REPORT_ERROR (("Bad ACPI HW RegisterId: %X\n", RegisterId));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (Flags & ACPI_MTX_LOCK)
    {
        Status = AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /* Always do a register read first so we can insert the new bits  */

    Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                    BitRegInfo->ParentRegister, &RegisterValue);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

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
         * writing 1, writing 0 has no effect.  So, the only relevant
         * information is the single bit we're interested in, all others should
         * be written as 0 so they will be left unchanged
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
         * Read the PM1 Control register.
         * Note that at this level, the fact that there are actually TWO
         * registers (A and B - and that B may not exist) is abstracted.
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "PM1 control: Read %X\n", RegisterValue));

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition,
                BitRegInfo->AccessBitMask, Value);

        Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, RegisterId,
                (UINT16) RegisterValue);
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
            ACPI_HIDWORD (ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm2CntBlk.Address)),
            ACPI_LODWORD (ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm2CntBlk.Address))));

        ACPI_REGISTER_INSERT_VALUE (RegisterValue, BitRegInfo->BitPosition,
                BitRegInfo->AccessBitMask, Value);

        ACPI_DEBUG_PRINT ((ACPI_DB_IO, "About to write %4.4X to %8.8X%8.8X\n",
            RegisterValue,
            ACPI_HIDWORD (ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm2CntBlk.Address)),
            ACPI_LODWORD (ACPI_GET_ADDRESS (AcpiGbl_FADT->XPm2CntBlk.Address))));

        Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                            ACPI_REGISTER_PM2_CONTROL, (UINT8) (RegisterValue));
        break;


    default:
        break;
    }


UnlockAndExit:

    if (Flags & ACPI_MTX_LOCK)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }

    /* Normalize the value that was read */

    ACPI_DEBUG_EXEC (RegisterValue = ((RegisterValue & BitRegInfo->AccessBitMask) >> BitRegInfo->BitPosition));

    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "Set bits: %8.8X actual %8.8X register %X\n",
            Value, RegisterValue, BitRegInfo->ParentRegister));
    return_ACPI_STATUS (Status);
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

ACPI_STATUS
AcpiHwRegisterRead (
    BOOLEAN                 UseLock,
    UINT32                  RegisterId,
    UINT32                  *ReturnValue)
{
    UINT32                  Value1 = 0;
    UINT32                  Value2 = 0;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("HwRegisterRead");


    if (ACPI_MTX_LOCK == UseLock)
    {
        Status = AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    switch (RegisterId)
    {
    case ACPI_REGISTER_PM1_STATUS:           /* 16-bit access */

        Status = AcpiHwLowLevelRead (16, &Value1, &AcpiGbl_FADT->XPm1aEvtBlk);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* PM1B is optional */

        Status = AcpiHwLowLevelRead (16, &Value2, &AcpiGbl_FADT->XPm1bEvtBlk);
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

        Status = AcpiHwLowLevelRead (16, &Value1, &AcpiGbl_FADT->XPm1aCntBlk);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        Status = AcpiHwLowLevelRead (16, &Value2, &AcpiGbl_FADT->XPm1bCntBlk);
        Value1 |= Value2;
        break;


    case ACPI_REGISTER_PM2_CONTROL:          /* 8-bit access */

        Status = AcpiHwLowLevelRead (8, &Value1, &AcpiGbl_FADT->XPm2CntBlk);
        break;


    case ACPI_REGISTER_PM_TIMER:             /* 32-bit access */

        Status = AcpiHwLowLevelRead (32, &Value1, &AcpiGbl_FADT->XPmTmrBlk);
        break;

    case ACPI_REGISTER_SMI_COMMAND_BLOCK:    /* 8-bit access */

        Status = AcpiOsReadPort (AcpiGbl_FADT->SmiCmd, &Value1, 8);
        break;

    default:
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown Register ID: %X\n", RegisterId));
        Status = AE_BAD_PARAMETER;
        break;
    }

UnlockAndExit:
    if (ACPI_MTX_LOCK == UseLock)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
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
 * PARAMETERS:  UseLock                - Mutex hw access.
 *              RegisterId             - RegisterID + Offset.
 *
 * RETURN:      Value read or written.
 *
 * DESCRIPTION: Acpi register Write function.  Registers are written at the
 *              given offset.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwRegisterWrite (
    BOOLEAN                 UseLock,
    UINT32                  RegisterId,
    UINT32                  Value)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("HwRegisterWrite");


    if (ACPI_MTX_LOCK == UseLock)
    {
        Status = AcpiUtAcquireMutex (ACPI_MTX_HARDWARE);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    switch (RegisterId)
    {
    case ACPI_REGISTER_PM1_STATUS:           /* 16-bit access */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aEvtBlk);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* PM1B is optional */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bEvtBlk);
        break;


    case ACPI_REGISTER_PM1_ENABLE:           /* 16-bit access*/

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_XPm1aEnable);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        /* PM1B is optional */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_XPm1bEnable);
        break;


    case ACPI_REGISTER_PM1_CONTROL:          /* 16-bit access */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aCntBlk);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bCntBlk);
        break;


    case ACPI_REGISTER_PM1A_CONTROL:         /* 16-bit access */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1aCntBlk);
        break;


    case ACPI_REGISTER_PM1B_CONTROL:         /* 16-bit access */

        Status = AcpiHwLowLevelWrite (16, Value, &AcpiGbl_FADT->XPm1bCntBlk);
        break;


    case ACPI_REGISTER_PM2_CONTROL:          /* 8-bit access */

        Status = AcpiHwLowLevelWrite (8, Value, &AcpiGbl_FADT->XPm2CntBlk);
        break;


    case ACPI_REGISTER_PM_TIMER:             /* 32-bit access */

        Status = AcpiHwLowLevelWrite (32, Value, &AcpiGbl_FADT->XPmTmrBlk);
        break;


    case ACPI_REGISTER_SMI_COMMAND_BLOCK:    /* 8-bit access */

        /* SMI_CMD is currently always in IO space */

        Status = AcpiOsWritePort (AcpiGbl_FADT->SmiCmd, Value, 8);
        break;


    default:
        Status = AE_BAD_PARAMETER;
        break;
    }

UnlockAndExit:
    if (ACPI_MTX_LOCK == UseLock)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_HARDWARE);
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwLowLevelRead
 *
 * PARAMETERS:  Width               - 8, 16, or 32
 *              Value               - Where the value is returned
 *              Register            - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read from either memory, IO, or PCI config space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwLowLevelRead (
    UINT32                  Width,
    UINT32                  *Value,
    ACPI_GENERIC_ADDRESS    *Reg)
{
    ACPI_PCI_ID             PciId;
    UINT16                  PciRegister;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_NAME ("HwLowLevelRead");


    /*
     * Must have a valid pointer to a GAS structure, and
     * a non-zero address within. However, don't return an error
     * because the PM1A/B code must not fail if B isn't present.
     */
    if ((!Reg) ||
        (!ACPI_VALID_ADDRESS (Reg->Address)))
    {
        return (AE_OK);
    }
    *Value = 0;

    /*
     * Three address spaces supported:
     * Memory, Io, or PCI config.
     */
    switch (Reg->AddressSpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_MEMORY:

        Status = AcpiOsReadMemory (
                    (ACPI_PHYSICAL_ADDRESS) ACPI_GET_ADDRESS (Reg->Address),
                    Value, Width);
        break;


    case ACPI_ADR_SPACE_SYSTEM_IO:

        Status = AcpiOsReadPort ((ACPI_IO_ADDRESS) ACPI_GET_ADDRESS (Reg->Address),
                    Value, Width);
        break;


    case ACPI_ADR_SPACE_PCI_CONFIG:

        PciId.Segment  = 0;
        PciId.Bus      = 0;
        PciId.Device   = ACPI_PCI_DEVICE (ACPI_GET_ADDRESS (Reg->Address));
        PciId.Function = ACPI_PCI_FUNCTION (ACPI_GET_ADDRESS (Reg->Address));
        PciRegister    = (UINT16) ACPI_PCI_REGISTER (ACPI_GET_ADDRESS (Reg->Address));

        Status = AcpiOsReadPciConfiguration  (&PciId, PciRegister,
                    Value, Width);
        break;


    default:
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Unsupported address space: %X\n", Reg->AddressSpaceId));
        return (AE_BAD_PARAMETER);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "Read:  %8.8X width %2d from %8.8X%8.8X (%s)\n",
            *Value, Width, 
            ACPI_HIDWORD (ACPI_GET_ADDRESS (Reg->Address)), 
            ACPI_LODWORD (ACPI_GET_ADDRESS (Reg->Address)), 
            AcpiUtGetRegionName (Reg->AddressSpaceId)));

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwLowLevelWrite
 *
 * PARAMETERS:  Width               - 8, 16, or 32
 *              Value               - To be written
 *              Register            - GAS register structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to either memory, IO, or PCI config space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwLowLevelWrite (
    UINT32                  Width,
    UINT32                  Value,
    ACPI_GENERIC_ADDRESS    *Reg)
{
    ACPI_PCI_ID             PciId;
    UINT16                  PciRegister;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_NAME ("HwLowLevelWrite");


    /*
     * Must have a valid pointer to a GAS structure, and
     * a non-zero address within. However, don't return an error
     * because the PM1A/B code must not fail if B isn't present.
     */
    if ((!Reg) ||
        (!ACPI_VALID_ADDRESS (Reg->Address)))
    {
        return (AE_OK);
    }
    /*
     * Three address spaces supported:
     * Memory, Io, or PCI config.
     */
    switch (Reg->AddressSpaceId)
    {
    case ACPI_ADR_SPACE_SYSTEM_MEMORY:

        Status = AcpiOsWriteMemory (
                    (ACPI_PHYSICAL_ADDRESS) ACPI_GET_ADDRESS (Reg->Address),
                    Value, Width);
        break;


    case ACPI_ADR_SPACE_SYSTEM_IO:

        Status = AcpiOsWritePort ((ACPI_IO_ADDRESS) ACPI_GET_ADDRESS (Reg->Address),
                    Value, Width);
        break;


    case ACPI_ADR_SPACE_PCI_CONFIG:

        PciId.Segment  = 0;
        PciId.Bus      = 0;
        PciId.Device   = ACPI_PCI_DEVICE (ACPI_GET_ADDRESS (Reg->Address));
        PciId.Function = ACPI_PCI_FUNCTION (ACPI_GET_ADDRESS (Reg->Address));
        PciRegister    = (UINT16) ACPI_PCI_REGISTER (ACPI_GET_ADDRESS (Reg->Address));

        Status = AcpiOsWritePciConfiguration (&PciId, PciRegister,
                    (ACPI_INTEGER) Value, Width);
        break;


    default:
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Unsupported address space: %X\n", Reg->AddressSpaceId));
        return (AE_BAD_PARAMETER);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_IO, "Wrote: %8.8X width %2d   to %8.8X%8.8X (%s)\n",
            Value, Width, 
            ACPI_HIDWORD (ACPI_GET_ADDRESS (Reg->Address)), 
            ACPI_LODWORD (ACPI_GET_ADDRESS (Reg->Address)), 
            AcpiUtGetRegionName (Reg->AddressSpaceId)));

    return (Status);
}
