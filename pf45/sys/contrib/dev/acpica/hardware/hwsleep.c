
/******************************************************************************
 *
 * Name: hwsleep.c - ACPI Hardware Sleep/Wake Interface
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwsleep")


/*******************************************************************************
 *
 * FUNCTION:    AcpiSetFirmwareWakingVector
 *
 * PARAMETERS:  PhysicalAddress     - 32-bit physical address of ACPI real mode
 *                                    entry point.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Sets the 32-bit FirmwareWakingVector field of the FACS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetFirmwareWakingVector (
    UINT32                  PhysicalAddress)
{
    ACPI_FUNCTION_TRACE (AcpiSetFirmwareWakingVector);


    /* Set the 32-bit vector */

    AcpiGbl_FACS->FirmwareWakingVector = PhysicalAddress;

    /* Clear the 64-bit vector if it exists */

    if ((AcpiGbl_FACS->Length > 32) && (AcpiGbl_FACS->Version >= 1))
    {
        AcpiGbl_FACS->XFirmwareWakingVector = 0;
    }

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiSetFirmwareWakingVector)


#if ACPI_MACHINE_WIDTH == 64
/*******************************************************************************
 *
 * FUNCTION:    AcpiSetFirmwareWakingVector64
 *
 * PARAMETERS:  PhysicalAddress     - 64-bit physical address of ACPI protected
 *                                    mode entry point.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Sets the 64-bit X_FirmwareWakingVector field of the FACS, if
 *              it exists in the table. This function is intended for use with
 *              64-bit host operating systems.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetFirmwareWakingVector64 (
    UINT64                  PhysicalAddress)
{
    ACPI_FUNCTION_TRACE (AcpiSetFirmwareWakingVector64);


    /* Determine if the 64-bit vector actually exists */

    if ((AcpiGbl_FACS->Length <= 32) || (AcpiGbl_FACS->Version < 1))
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Clear 32-bit vector, set the 64-bit X_ vector */

    AcpiGbl_FACS->FirmwareWakingVector = 0;
    AcpiGbl_FACS->XFirmwareWakingVector = PhysicalAddress;
    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiSetFirmwareWakingVector64)
#endif

/*******************************************************************************
 *
 * FUNCTION:    AcpiEnterSleepStatePrep
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Prepare to enter a system sleep state (see ACPI 2.0 spec p 231)
 *              This function must execute with interrupts enabled.
 *              We break sleeping into 2 stages so that OSPM can handle
 *              various OS-specific tasks between the two steps.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnterSleepStatePrep (
    UINT8                   SleepState)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg;


    ACPI_FUNCTION_TRACE (AcpiEnterSleepStatePrep);


    /* _PSW methods could be run here to enable wake-on keyboard, LAN, etc. */

    Status = AcpiGetSleepTypeData (SleepState,
                    &AcpiGbl_SleepTypeA, &AcpiGbl_SleepTypeB);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Execute the _PTS method (Prepare To Sleep) */

    ArgList.Count = 1;
    ArgList.Pointer = &Arg;
    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = SleepState;

    Status = AcpiEvaluateObject (NULL, METHOD_NAME__PTS, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        return_ACPI_STATUS (Status);
    }

    /* Setup the argument to the _SST method (System STatus) */

    switch (SleepState)
    {
    case ACPI_STATE_S0:
        Arg.Integer.Value = ACPI_SST_WORKING;
        break;

    case ACPI_STATE_S1:
    case ACPI_STATE_S2:
    case ACPI_STATE_S3:
        Arg.Integer.Value = ACPI_SST_SLEEPING;
        break;

    case ACPI_STATE_S4:
        Arg.Integer.Value = ACPI_SST_SLEEP_CONTEXT;
        break;

    default:
        Arg.Integer.Value = ACPI_SST_INDICATOR_OFF; /* Default is off */
        break;
    }

    /*
     * Set the system indicators to show the desired sleep state.
     * _SST is an optional method (return no error if not found)
     */
    Status = AcpiEvaluateObject (NULL, METHOD_NAME__SST, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While executing method _SST"));
    }

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiEnterSleepStatePrep)


/*******************************************************************************
 *
 * FUNCTION:    AcpiEnterSleepState
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter a system sleep state
 *              THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnterSleepState (
    UINT8                   SleepState)
{
    UINT32                  Pm1aControl;
    UINT32                  Pm1bControl;
    ACPI_BIT_REGISTER_INFO  *SleepTypeRegInfo;
    ACPI_BIT_REGISTER_INFO  *SleepEnableRegInfo;
    UINT32                  InValue;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg;
    UINT32                  Retry;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiEnterSleepState);


    if ((AcpiGbl_SleepTypeA > ACPI_SLEEP_TYPE_MAX) ||
        (AcpiGbl_SleepTypeB > ACPI_SLEEP_TYPE_MAX))
    {
        ACPI_ERROR ((AE_INFO, "Sleep values out of range: A=0x%X B=0x%X",
            AcpiGbl_SleepTypeA, AcpiGbl_SleepTypeB));
        return_ACPI_STATUS (AE_AML_OPERAND_VALUE);
    }

    SleepTypeRegInfo   = AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_TYPE);
    SleepEnableRegInfo = AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_ENABLE);

    /* Clear wake status */

    Status = AcpiWriteBitRegister (ACPI_BITREG_WAKE_STATUS, ACPI_CLEAR_STATUS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Clear all fixed and general purpose status bits */

    Status = AcpiHwClearAcpiStatus ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (SleepState != ACPI_STATE_S5)
    {
        /*
         * Disable BM arbitration. This feature is contained within an
         * optional register (PM2 Control), so ignore a BAD_ADDRESS
         * exception.
         */
        Status = AcpiWriteBitRegister (ACPI_BITREG_ARB_DISABLE, 1);
        if (ACPI_FAILURE (Status) && (Status != AE_BAD_ADDRESS))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * 1) Disable/Clear all GPEs
     * 2) Enable all wakeup GPEs
     */
    Status = AcpiHwDisableAllGpes ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    AcpiGbl_SystemAwakeAndRunning = FALSE;

    Status = AcpiHwEnableAllWakeupGpes ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Execute the _GTS method (Going To Sleep) */

    ArgList.Count = 1;
    ArgList.Pointer = &Arg;
    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = SleepState;

    Status = AcpiEvaluateObject (NULL, METHOD_NAME__GTS, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        return_ACPI_STATUS (Status);
    }

    /* Get current value of PM1A control */

    Status = AcpiHwRegisterRead (ACPI_REGISTER_PM1_CONTROL,
                &Pm1aControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    ACPI_DEBUG_PRINT ((ACPI_DB_INIT,
        "Entering sleep state [S%u]\n", SleepState));

    /* Clear the SLP_EN and SLP_TYP fields */

    Pm1aControl &= ~(SleepTypeRegInfo->AccessBitMask |
                     SleepEnableRegInfo->AccessBitMask);
    Pm1bControl = Pm1aControl;

    /* Insert the SLP_TYP bits */

    Pm1aControl |= (AcpiGbl_SleepTypeA << SleepTypeRegInfo->BitPosition);
    Pm1bControl |= (AcpiGbl_SleepTypeB << SleepTypeRegInfo->BitPosition);

    /*
     * We split the writes of SLP_TYP and SLP_EN to workaround
     * poorly implemented hardware.
     */

    /* Write #1: write the SLP_TYP data to the PM1 Control registers */

    Status = AcpiHwWritePm1Control (Pm1aControl, Pm1bControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Insert the sleep enable (SLP_EN) bit */

    Pm1aControl |= SleepEnableRegInfo->AccessBitMask;
    Pm1bControl |= SleepEnableRegInfo->AccessBitMask;

    /* Flush caches, as per ACPI specification */

    ACPI_FLUSH_CPU_CACHE ();

    /* Write #2: Write both SLP_TYP + SLP_EN */

    Status = AcpiHwWritePm1Control (Pm1aControl, Pm1bControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (SleepState > ACPI_STATE_S3)
    {
        /*
         * We wanted to sleep > S3, but it didn't happen (by virtue of the
         * fact that we are still executing!)
         *
         * Wait ten seconds, then try again. This is to get S4/S5 to work on
         * all machines.
         *
         * We wait so long to allow chipsets that poll this reg very slowly
         * to still read the right value. Ideally, this block would go
         * away entirely.
         */
        AcpiOsStall (10000000);

        Status = AcpiHwRegisterWrite (ACPI_REGISTER_PM1_CONTROL,
                    SleepEnableRegInfo->AccessBitMask);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /* Wait until we enter sleep state */

    Retry = 1000;
    do
    {
        Status = AcpiReadBitRegister (ACPI_BITREG_WAKE_STATUS, &InValue);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        if (AcpiGbl_EnableInterpreterSlack)
        {
            /*
             * Some BIOSs don't set WAK_STS at all.  Give up waiting after
             * 1000 retries if it still isn't set.
             */
            if (Retry-- == 0)
            {
                break;
            }
        }

        /* Spin until we wake */

    } while (!InValue);

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiEnterSleepState)


/*******************************************************************************
 *
 * FUNCTION:    AcpiEnterSleepStateS4bios
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform a S4 bios request.
 *              THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnterSleepStateS4bios (
    void)
{
    UINT32                  InValue;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiEnterSleepStateS4bios);


    /* Clear the wake status bit (PM1) */

    Status = AcpiWriteBitRegister (ACPI_BITREG_WAKE_STATUS, ACPI_CLEAR_STATUS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiHwClearAcpiStatus ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * 1) Disable/Clear all GPEs
     * 2) Enable all wakeup GPEs
     */
    Status = AcpiHwDisableAllGpes ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    AcpiGbl_SystemAwakeAndRunning = FALSE;

    Status = AcpiHwEnableAllWakeupGpes ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    ACPI_FLUSH_CPU_CACHE ();

    Status = AcpiHwWritePort (AcpiGbl_FADT.SmiCommand,
                (UINT32) AcpiGbl_FADT.S4BiosRequest, 8);

    do {
        AcpiOsStall(1000);
        Status = AcpiReadBitRegister (ACPI_BITREG_WAKE_STATUS, &InValue);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    } while (!InValue);

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiEnterSleepStateS4bios)


/*******************************************************************************
 *
 * FUNCTION:    AcpiLeaveSleepState
 *
 * PARAMETERS:  SleepState          - Which sleep state we just exited
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform OS-independent ACPI cleanup after a sleep
 *              Called with interrupts ENABLED.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiLeaveSleepState (
    UINT8                   SleepState)
{
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg;
    ACPI_STATUS             Status;
    ACPI_BIT_REGISTER_INFO  *SleepTypeRegInfo;
    ACPI_BIT_REGISTER_INFO  *SleepEnableRegInfo;
    UINT32                  Pm1aControl;
    UINT32                  Pm1bControl;


    ACPI_FUNCTION_TRACE (AcpiLeaveSleepState);


    /*
     * Set SLP_TYPE and SLP_EN to state S0.
     * This is unclear from the ACPI Spec, but it is required
     * by some machines.
     */
    Status = AcpiGetSleepTypeData (ACPI_STATE_S0,
                    &AcpiGbl_SleepTypeA, &AcpiGbl_SleepTypeB);
    if (ACPI_SUCCESS (Status))
    {
        SleepTypeRegInfo =
            AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_TYPE);
        SleepEnableRegInfo =
            AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_ENABLE);

        /* Get current value of PM1A control */

        Status = AcpiHwRegisterRead (ACPI_REGISTER_PM1_CONTROL,
                    &Pm1aControl);
        if (ACPI_SUCCESS (Status))
        {
            /* Clear the SLP_EN and SLP_TYP fields */

            Pm1aControl &= ~(SleepTypeRegInfo->AccessBitMask |
                SleepEnableRegInfo->AccessBitMask);
            Pm1bControl = Pm1aControl;

            /* Insert the SLP_TYP bits */

            Pm1aControl |= (AcpiGbl_SleepTypeA <<
                SleepTypeRegInfo->BitPosition);
            Pm1bControl |= (AcpiGbl_SleepTypeB <<
                SleepTypeRegInfo->BitPosition);

            /* Write the control registers and ignore any errors */

            (void) AcpiHwWritePm1Control (Pm1aControl, Pm1bControl);
        }
    }

    /* Ensure EnterSleepStatePrep -> EnterSleepState ordering */

    AcpiGbl_SleepTypeA = ACPI_SLEEP_TYPE_INVALID;

    /* Setup parameter object */

    ArgList.Count = 1;
    ArgList.Pointer = &Arg;
    Arg.Type = ACPI_TYPE_INTEGER;

    /* Ignore any errors from these methods */

    Arg.Integer.Value = ACPI_SST_WAKING;
    Status = AcpiEvaluateObject (NULL, METHOD_NAME__SST, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Method _SST"));
    }

    Arg.Integer.Value = SleepState;
    Status = AcpiEvaluateObject (NULL, METHOD_NAME__BFS, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Method _BFS"));
    }

    Status = AcpiEvaluateObject (NULL, METHOD_NAME__WAK, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Method _WAK"));
    }
    /* TBD: _WAK "sometimes" returns stuff - do we want to look at it? */

    /*
     * Restore the GPEs:
     * 1) Disable/Clear all GPEs
     * 2) Enable all runtime GPEs
     */
    Status = AcpiHwDisableAllGpes ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    AcpiGbl_SystemAwakeAndRunning = TRUE;

    Status = AcpiHwEnableAllRuntimeGpes ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Enable power button */

    (void) AcpiWriteBitRegister(
            AcpiGbl_FixedEventInfo[ACPI_EVENT_POWER_BUTTON].EnableRegisterId,
            ACPI_ENABLE_EVENT);

    (void) AcpiWriteBitRegister(
            AcpiGbl_FixedEventInfo[ACPI_EVENT_POWER_BUTTON].StatusRegisterId,
            ACPI_CLEAR_STATUS);

    /*
     * Enable BM arbitration. This feature is contained within an
     * optional register (PM2 Control), so ignore a BAD_ADDRESS
     * exception.
     */
    Status = AcpiWriteBitRegister (ACPI_BITREG_ARB_DISABLE, 0);
    if (ACPI_FAILURE (Status) && (Status != AE_BAD_ADDRESS))
    {
        return_ACPI_STATUS (Status);
    }

    Arg.Integer.Value = ACPI_SST_WORKING;
    Status = AcpiEvaluateObject (NULL, METHOD_NAME__SST, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Method _SST"));
    }

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiLeaveSleepState)

