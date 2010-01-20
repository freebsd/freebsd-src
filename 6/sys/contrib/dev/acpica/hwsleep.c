
/******************************************************************************
 *
 * Name: hwsleep.c - ACPI Hardware Sleep/Wake Interface
 *              $Revision: 70 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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

#include <contrib/dev/acpica/acpi.h>

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwsleep")


#define METHOD_NAME__BFS        "\\_BFS"
#define METHOD_NAME__GTS        "\\_GTS"
#define METHOD_NAME__PTS        "\\_PTS"
#define METHOD_NAME__SST        "\\_SI._SST"
#define METHOD_NAME__WAK        "\\_WAK"

#define ACPI_SST_INDICATOR_OFF  0
#define ACPI_SST_WORKING        1
#define ACPI_SST_WAKING         2
#define ACPI_SST_SLEEPING       3
#define ACPI_SST_SLEEP_CONTEXT  4


/******************************************************************************
 *
 * FUNCTION:    AcpiSetFirmwareWakingVector
 *
 * PARAMETERS:  PhysicalAddress     - Physical address of ACPI real mode
 *                                    entry point.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Access function for dFirmwareWakingVector field in FACS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetFirmwareWakingVector (
    ACPI_PHYSICAL_ADDRESS PhysicalAddress)
{

    ACPI_FUNCTION_TRACE ("AcpiSetFirmwareWakingVector");


    /* Set the vector */

    if (AcpiGbl_CommonFACS.VectorWidth == 32)
    {
        *(ACPI_CAST_PTR (UINT32, AcpiGbl_CommonFACS.FirmwareWakingVector))
                = (UINT32) PhysicalAddress;
    }
    else
    {
        *AcpiGbl_CommonFACS.FirmwareWakingVector
                = PhysicalAddress;
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiGetFirmwareWakingVector
 *
 * PARAMETERS:  *PhysicalAddress    - Output buffer where contents of
 *                                    the FirmwareWakingVector field of
 *                                    the FACS will be stored.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Access function for FirmwareWakingVector field in FACS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetFirmwareWakingVector (
    ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{

    ACPI_FUNCTION_TRACE ("AcpiGetFirmwareWakingVector");


    if (!PhysicalAddress)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Get the vector */

    if (AcpiGbl_CommonFACS.VectorWidth == 32)
    {
        *PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)
            *(ACPI_CAST_PTR (UINT32, AcpiGbl_CommonFACS.FirmwareWakingVector));
    }
    else
    {
        *PhysicalAddress =
            *AcpiGbl_CommonFACS.FirmwareWakingVector;
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
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
    UINT8               SleepState)
{
    ACPI_STATUS         Status;
    ACPI_OBJECT_LIST    ArgList;
    ACPI_OBJECT         Arg;


    ACPI_FUNCTION_TRACE ("AcpiEnterSleepStatePrep");


    /*
     * _PSW methods could be run here to enable wake-on keyboard, LAN, etc.
     */
    Status = AcpiGetSleepTypeData (SleepState,
                    &AcpiGbl_SleepTypeA, &AcpiGbl_SleepTypeB);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Setup parameter object */

    ArgList.Count = 1;
    ArgList.Pointer = &Arg;

    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = SleepState;

    /* Run the _PTS and _GTS methods */

    Status = AcpiEvaluateObject (NULL, METHOD_NAME__PTS, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiEvaluateObject (NULL, METHOD_NAME__GTS, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        return_ACPI_STATUS (Status);
    }

    /* Setup the argument to _SST */

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
        Arg.Integer.Value = ACPI_SST_INDICATOR_OFF; /* Default is indicator off */
        break;
    }

    /* Set the system indicators to show the desired sleep state. */

    Status = AcpiEvaluateObject (NULL, METHOD_NAME__SST, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
         ACPI_REPORT_ERROR (("Method _SST failed, %s\n", AcpiFormatException (Status)));
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEnterSleepState
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter a system sleep state (see ACPI 2.0 spec p 231)
 *              THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnterSleepState (
    UINT8                   SleepState)
{
    UINT32                  PM1AControl;
    UINT32                  PM1BControl;
    ACPI_BIT_REGISTER_INFO  *SleepTypeRegInfo;
    ACPI_BIT_REGISTER_INFO  *SleepEnableRegInfo;
    UINT32                  InValue;
    UINT32                  Retry;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiEnterSleepState");


    if ((AcpiGbl_SleepTypeA > ACPI_SLEEP_TYPE_MAX) ||
        (AcpiGbl_SleepTypeB > ACPI_SLEEP_TYPE_MAX))
    {
        ACPI_REPORT_ERROR (("Sleep values out of range: A=%X B=%X\n",
            AcpiGbl_SleepTypeA, AcpiGbl_SleepTypeB));
        return_ACPI_STATUS (AE_AML_OPERAND_VALUE);
    }

    SleepTypeRegInfo   = AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_TYPE_A);
    SleepEnableRegInfo = AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_ENABLE);

    /* Clear wake status */

    Status = AcpiSetRegister (ACPI_BITREG_WAKE_STATUS, 1, ACPI_MTX_DO_NOT_LOCK);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Clear all fixed and general purpose status bits */

    Status = AcpiHwClearAcpiStatus (ACPI_MTX_DO_NOT_LOCK);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (SleepState != ACPI_STATE_S5)
    {
        /* Disable BM arbitration */

        Status = AcpiSetRegister (ACPI_BITREG_ARB_DISABLE, 1, ACPI_MTX_DO_NOT_LOCK);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * 1) Disable/Clear all GPEs
     * 2) Enable all wakeup GPEs
     */
    Status = AcpiHwDisableAllGpes (ACPI_ISR);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    AcpiGbl_SystemAwakeAndRunning = FALSE;

    Status = AcpiHwEnableAllWakeupGpes (ACPI_ISR);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get current value of PM1A control */

    Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_CONTROL, &PM1AControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    ACPI_DEBUG_PRINT ((ACPI_DB_INIT, "Entering sleep state [S%d]\n", SleepState));

    /* Clear SLP_EN and SLP_TYP fields */

    PM1AControl &= ~(SleepTypeRegInfo->AccessBitMask | SleepEnableRegInfo->AccessBitMask);
    PM1BControl = PM1AControl;

    /* Insert SLP_TYP bits */

    PM1AControl |= (AcpiGbl_SleepTypeA << SleepTypeRegInfo->BitPosition);
    PM1BControl |= (AcpiGbl_SleepTypeB << SleepTypeRegInfo->BitPosition);

    /*
     * We split the writes of SLP_TYP and SLP_EN to workaround
     * poorly implemented hardware.
     */

    /* Write #1: fill in SLP_TYP data */

    Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1A_CONTROL, PM1AControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1B_CONTROL, PM1BControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Insert SLP_ENABLE bit */

    PM1AControl |= SleepEnableRegInfo->AccessBitMask;
    PM1BControl |= SleepEnableRegInfo->AccessBitMask;

    /* Write #2: SLP_TYP + SLP_EN */

    ACPI_FLUSH_CPU_CACHE ();

    Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1A_CONTROL, PM1AControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1B_CONTROL, PM1BControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (SleepState > ACPI_STATE_S3)
    {
        /*
         * We wanted to sleep > S3, but it didn't happen (by virtue of the fact that
         * we are still executing!)
         *
         * Wait ten seconds, then try again. This is to get S4/S5 to work on all machines.
         *
         * We wait so long to allow chipsets that poll this reg very slowly to
         * still read the right value. Ideally, this block would go
         * away entirely.
         */
        AcpiOsStall (10000000);

        Status = AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_CONTROL,
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
        Status = AcpiGetRegister (ACPI_BITREG_WAKE_STATUS, &InValue, ACPI_MTX_DO_NOT_LOCK);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /*
         * Some BIOSs don't set WAK_STS at all.  Give up waiting after
         * 1000 retries if it still isn't set.
         */
        if (Retry-- == 0)
        {
            break;
        }

        /* Spin until we wake */

    } while (!InValue);

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
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


    ACPI_FUNCTION_TRACE ("AcpiEnterSleepStateS4bios");


    Status = AcpiSetRegister (ACPI_BITREG_WAKE_STATUS, 1, ACPI_MTX_DO_NOT_LOCK);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiHwClearAcpiStatus (ACPI_MTX_DO_NOT_LOCK);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * 1) Disable/Clear all GPEs
     * 2) Enable all wakeup GPEs
     */
    Status = AcpiHwDisableAllGpes (ACPI_ISR);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    AcpiGbl_SystemAwakeAndRunning = FALSE;

    Status = AcpiHwEnableAllWakeupGpes (ACPI_ISR);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    ACPI_FLUSH_CPU_CACHE ();

    Status = AcpiOsWritePort (AcpiGbl_FADT->SmiCmd, (UINT32) AcpiGbl_FADT->S4BiosReq, 8);

    do {
        AcpiOsStall(1000);
        Status = AcpiGetRegister (ACPI_BITREG_WAKE_STATUS, &InValue, ACPI_MTX_DO_NOT_LOCK);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    } while (!InValue);

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
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
    UINT32                  PM1AControl;
    UINT32                  PM1BControl;


    ACPI_FUNCTION_TRACE ("AcpiLeaveSleepState");


    /*
     * Set SLP_TYPE and SLP_EN to state S0.
     * This is unclear from the ACPI Spec, but it is required
     * by some machines.
     */
    Status = AcpiGetSleepTypeData (ACPI_STATE_S0,
                    &AcpiGbl_SleepTypeA, &AcpiGbl_SleepTypeB);
    if (ACPI_SUCCESS (Status))
    {
        SleepTypeRegInfo   = AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_TYPE_A);
        SleepEnableRegInfo = AcpiHwGetBitRegisterInfo (ACPI_BITREG_SLEEP_ENABLE);

        /* Get current value of PM1A control */

        Status = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK,
                    ACPI_REGISTER_PM1_CONTROL, &PM1AControl);
        if (ACPI_SUCCESS (Status))
        {
            /* Clear SLP_EN and SLP_TYP fields */

            PM1AControl &= ~(SleepTypeRegInfo->AccessBitMask |
                             SleepEnableRegInfo->AccessBitMask);
            PM1BControl = PM1AControl;

            /* Insert SLP_TYP bits */

            PM1AControl |= (AcpiGbl_SleepTypeA << SleepTypeRegInfo->BitPosition);
            PM1BControl |= (AcpiGbl_SleepTypeB << SleepTypeRegInfo->BitPosition);

            /* Just ignore any errors */

            (void) AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                            ACPI_REGISTER_PM1A_CONTROL, PM1AControl);
            (void) AcpiHwRegisterWrite (ACPI_MTX_DO_NOT_LOCK,
                            ACPI_REGISTER_PM1B_CONTROL, PM1BControl);
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
        ACPI_REPORT_ERROR (("Method _SST failed, %s\n", AcpiFormatException (Status)));
    }

    Arg.Integer.Value = SleepState;
    Status = AcpiEvaluateObject (NULL, METHOD_NAME__BFS, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_REPORT_ERROR (("Method _BFS failed, %s\n", AcpiFormatException (Status)));
    }

    Status = AcpiEvaluateObject (NULL, METHOD_NAME__WAK, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_REPORT_ERROR (("Method _WAK failed, %s\n", AcpiFormatException (Status)));
    }
    /* TBD: _WAK "sometimes" returns stuff - do we want to look at it? */

    /*
     * Restore the GPEs:
     * 1) Disable/Clear all GPEs
     * 2) Enable all runtime GPEs
     */
    Status = AcpiHwDisableAllGpes (ACPI_NOT_ISR);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }
    AcpiGbl_SystemAwakeAndRunning = TRUE;

    Status = AcpiHwEnableAllRuntimeGpes (ACPI_NOT_ISR);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Enable power button */

    (void) AcpiSetRegister(AcpiGbl_FixedEventInfo[ACPI_EVENT_POWER_BUTTON].EnableRegisterId,
            1, ACPI_MTX_DO_NOT_LOCK);
    (void) AcpiSetRegister(AcpiGbl_FixedEventInfo[ACPI_EVENT_POWER_BUTTON].StatusRegisterId,
            1, ACPI_MTX_DO_NOT_LOCK);

    /* Enable BM arbitration */

    Status = AcpiSetRegister (ACPI_BITREG_ARB_DISABLE, 0, ACPI_MTX_LOCK);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Arg.Integer.Value = ACPI_SST_WORKING;
    Status = AcpiEvaluateObject (NULL, METHOD_NAME__SST, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_REPORT_ERROR (("Method _SST failed, %s\n", AcpiFormatException (Status)));
    }

    return_ACPI_STATUS (Status);
}
