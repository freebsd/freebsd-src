/******************************************************************************
 *
 * Name: hwcpu32.c - CPU support for IA32 (Throttling, CxStates)
 *              $Revision: 33 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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

#include "acpi.h"
#include "acnamesp.h"
#include "achware.h"

#define _COMPONENT          HARDWARE
        MODULE_NAME         ("Hwcpu32")


#define BIT_4               0x10  /* TBD: [investigate] is this correct?  */


/****************************************************************************
 *
 * FUNCTION:    AcpiHwEnterC1
 *
 * PARAMETERS:  PblkAddress     - Address of the processor control block
 *              PmTimerTicks    - Number of PM timer ticks elapsed while asleep
 *
 * RETURN:      Function status.
 *
 * DESCRIPTION: Set C1 state on IA32 processor (halt)
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwEnterC1(
    ACPI_IO_ADDRESS         PblkAddress,
    UINT32                  *PmTimerTicks)
{
    UINT32                  Timer = 0;


    if (!PmTimerTicks)
    {
        /*
         * Enter C1:
         * ---------
         */
        enable();
        halt();
        *PmTimerTicks = ACPI_UINT32_MAX;
    }
    else
    {
        Timer = AcpiHwPmtTicks ();

        /*
         * Enter C1:
         * ---------
         */
        enable ();
        halt ();

        /*
         * Compute Time in C1:
         * -------------------
         */
        Timer = AcpiHwPmtTicks () - Timer;

        *PmTimerTicks = Timer;
    }

    return (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiHwEnterC2
 *
 * PARAMETERS:  PblkAddress     - Address of the processor control block
 *              PmTimerTicks    - Number of PM timer ticks elapsed while asleep
 *
 * RETURN:      <none>
 *
 * DESCRIPTION: Set C2 state on IA32 processor
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwEnterC2(
    ACPI_IO_ADDRESS         PblkAddress,
    UINT32                  *PmTimerTicks)
{
    UINT32                  Timer = 0;


    if (!PblkAddress || !PmTimerTicks)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * Disable interrupts before all C2/C3 transitions.
     */
    disable ();

    Timer = AcpiHwPmtTicks ();

    /*
     * Enter C2:
     * ---------
     * Read from the P_LVL2 (P_BLK+4) register to invoke a C2 transition.
     */
    AcpiOsIn8 ((ACPI_IO_ADDRESS) (PblkAddress + 4));

    /*
     * Perform Dummy Op:
     * -----------------
     * We have to do something useless after reading LVL2 because chipsets
     * cannot guarantee that STPCLK# gets asserted in time to freeze execution.
     */
    AcpiOsIn8 ((ACPI_IO_ADDRESS) AcpiGbl_FACP->Pm2CntBlk);

    /*
     * Compute Time in C2:
     * -------------------
     */
    Timer = AcpiHwPmtTicks () - Timer;

    *PmTimerTicks = Timer;

    /*
     * Re-enable interrupts after coming out of C2/C3.
     */
    enable ();

    return (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiHwEnterC3
 *
 * PARAMETERS:  PblkAddress     - Address of the processor control block
 *              PmTimerTicks    - Number of PM timer ticks elapsed while asleep
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Set C3 state on IA32 processor (UP only, cache coherency via
 *              disabling bus mastering)
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwEnterC3(
    ACPI_IO_ADDRESS         PblkAddress,
    UINT32                  *PmTimerTicks)
{
    UINT32                  Timer = 0;
    UINT8                   Pm2CntBlk = 0;
    UINT32                  BusMasterStatus = 0;


    if (!PblkAddress || !PmTimerTicks)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * Check the BM_STS bit, if it is set, do not enter C3
     *  but clear the bit (with a write) and exit, telling
     *  the calling module that we spent zero time in C3.
     *  If bus mastering continues, this action should
     *  eventually cause a demotion to C2
     */
    if (1 == (BusMasterStatus =
        AcpiHwRegisterAccess (ACPI_READ, ACPI_MTX_LOCK, BM_STS)))
    {
        /*
         * Clear the BM_STS bit by setting it.
         */
        AcpiHwRegisterAccess (ACPI_WRITE, ACPI_MTX_LOCK, BM_STS, 1);
        *PmTimerTicks = 0;
        return (AE_OK);
    }

    /*
     * Disable interrupts before all C2/C3 transitions.
     */
    disable();

    /*
     * Disable Bus Mastering:
     * ----------------------
     * Set the PM2_CNT.ARB_DIS bit (bit #0), preserving all other bits.
     */
    Pm2CntBlk = AcpiOsIn8 ((ACPI_IO_ADDRESS) AcpiGbl_FACP->Pm2CntBlk);
    Pm2CntBlk |= 0x01;
    AcpiOsOut8 ((ACPI_IO_ADDRESS) AcpiGbl_FACP->Pm2CntBlk, Pm2CntBlk);

    /*
     * Get the timer base before entering C state
     */
    Timer = AcpiHwPmtTicks ();

    /*
     * Enter C3:
     * ---------
     * Read from the P_LVL3 (P_BLK+5) register to invoke a C3 transition.
     */
    AcpiOsIn8 ((ACPI_IO_ADDRESS)(PblkAddress + 5));

    /*
     * Perform Dummy Op:
     * -----------------
     * We have to do something useless after reading LVL3 because chipsets
     * cannot guarantee that STPCLK# gets asserted in time to freeze execution.
     */
    AcpiOsIn8 ((ACPI_IO_ADDRESS) AcpiGbl_FACP->Pm2CntBlk);

    /*
     * Immediately compute the time in the C state
     */
    Timer = AcpiHwPmtTicks() - Timer;

    /*
     * Re-Enable Bus Mastering:
     * ------------------------
     * Clear the PM2_CNT.ARB_DIS bit (bit #0), preserving all other bits.
     */
    Pm2CntBlk = AcpiOsIn8 ((ACPI_IO_ADDRESS) AcpiGbl_FACP->Pm2CntBlk);
    Pm2CntBlk &= 0xFE;
    AcpiOsOut8 ((ACPI_IO_ADDRESS) AcpiGbl_FACP->Pm2CntBlk, Pm2CntBlk);

    /* TBD: [Unhandled]: Support 24-bit timers (this algorithm assumes 32-bit) */

    *PmTimerTicks = Timer;

    /*
     * Re-enable interrupts after coming out of C2/C3.
     */
    enable();

    return (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiHwEnterCx
 *
 * PARAMETERS:  ProcessorHandle     - handle of the processor
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Invoke the currently active processor Cx handler to put this
 *              processor to sleep.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwEnterCx (
    ACPI_IO_ADDRESS         PblkAddress,
    UINT32                  *PmTimerTicks)
{

    if (!AcpiHwCxHandlers[AcpiHwActiveCxState])
    {
        return (AE_SUPPORT);
    }

    return (AcpiHwCxHandlers[AcpiHwActiveCxState] (PblkAddress, PmTimerTicks));
}


/****************************************************************************
 *
 * FUNCTION:    AcpiHwSetCx
 *
 * PARAMETERS:  State               - value (1-3) of the Cx state to 'make active'
 *
 * RETURN:      Function status.
 *
 * DESCRIPTION: Sets the state to use during calls to AcpiHwEnterCx().
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwSetCx (
    UINT32                  CxState)
{
    /*
     * Supported State?
     * ----------------
     */
    if ((CxState < 1) || (CxState > 3))
    {
        return (AE_BAD_PARAMETER);
    }

    if (!AcpiHwCxHandlers[CxState])
    {
        return (AE_SUPPORT);
    }

    /*
     * New Cx State?
     * -------------
     * We only care when moving from one state to another...
     */
    if (AcpiHwActiveCxState == CxState)
    {
        return (AE_OK);
    }

    /*
     * Prepare to Use New State:
     * -------------------------
     * If the new CxState is C3, the BM_RLD bit must be set to allow
     *  the generation of a bus master requets to cause the processor
     *  in the C3 state to transition to the C0 state.
     */
    switch (CxState)
    {
    case 3:
        AcpiHwRegisterAccess (ACPI_WRITE, ACPI_MTX_LOCK, BM_RLD, 1);
        break;
    }

    /*
     * Clean up from Old State:
     * ------------------------
     * If the old CxState was C3, the BM_RLD bit is reset.  When the
     *  bit is reset, the generation of a bus master request does not
     *  effect any processor in the C3 state.
     */
    switch (AcpiHwActiveCxState)
    {
    case 3:
        AcpiHwRegisterAccess (ACPI_WRITE, ACPI_MTX_LOCK, BM_RLD, 0);
        break;
    }

    /*
     * Enable:
     * -------
     */
    AcpiHwActiveCxState = CxState;

    return (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiHwGetCxInfo
 *
 * PARAMETERS:  CxStates        - Information (latencies) on all Cx states
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: This function is called both to initialize Cx handling
 *              and retrieve the current Cx information (latency values).
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwGetCxInfo (
    UINT32                  CxStates[])
{
    BOOLEAN                 SMP_system = FALSE;


    FUNCTION_TRACE("HwGetCxInfo");


    if (!CxStates)
    {
        return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    /*
     *  TBD: [Unhandled] need to init SMP_system using info from the MAPIC
     *       table.
     */

    /*
     * Set Defaults:
     * -------------
     * C0 and C1 support is implied (but what about that PROC_C1 register
     * in the FADT?!?!).  Set C2/C3 to max. latency (not supported until
     * proven otherwise).
     */
    CxStates[0] = 0;
    CxStates[1] = 0;
    CxStates[2] = MAX_CX_STATE_LATENCY;
    CxStates[3] = MAX_CX_STATE_LATENCY;

    /*
     * C2 Supported?
     * -------------
     * We're only supporting C2 when the latency is <= 100 microseconds,
     * and on SMP systems when P_LVL2_UP (which indicates C2 only on UP)
     * is not set.
     */
    if (AcpiGbl_FACP->Plvl2Lat <= 100)
    {
        if (!SMP_system)
        {
            AcpiHwCxHandlers[2] = AcpiHwEnterC2;
            CxStates[2] = AcpiGbl_FACP->Plvl2Lat;
        }

        else if (!AcpiGbl_FACP->Plvl2Up)
        {
            AcpiHwCxHandlers[2] = AcpiHwEnterC2;
            CxStates[2] = AcpiGbl_FACP->Plvl2Lat;
        }
    }

    /*
     * C3 Supported?
     * -------------
     * We're only supporting C3 on UP systems when the latency is
     * <= 1000 microseconds and that include the ability to disable
     * Bus Mastering while in C3 (ARB_DIS) but allows Bus Mastering
     * requests to wake the system from C3 (BM_RLD).  Note his method
     * of maintaining cache coherency (disabling of bus mastering)
     * cannot be used on SMP systems, and flushing caches (e.g. WBINVD)
     * is simply too costly (at this time).
     */
    if (AcpiGbl_FACP->Plvl3Lat <= 1000)
    {
        if (!SMP_system && (AcpiGbl_FACP->Pm2CntBlk &&
            AcpiGbl_FACP->Pm2CntLen))
        {
            AcpiHwCxHandlers[3] = AcpiHwEnterC3;
            CxStates[3] = AcpiGbl_FACP->Plvl3Lat;
        }
    }

    return_ACPI_STATUS(AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiHwGetCxHandler
 *
 * PARAMETERS:  State           - the Cx state
 *              Handler         - pointer to location for the returned handler
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: This function is called to get an installed Cx state handler.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwGetCxHandler (
    UINT32                  CxState,
    ACPI_C_STATE_HANDLER    *Handler)
{
    FUNCTION_TRACE ("HwGetCxHandler");


    if ((CxState == 0) || (CxState >= MAX_CX_STATES) || !Handler)
    {
        return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    *Handler = AcpiHwCxHandlers[CxState];

    return_ACPI_STATUS(AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiHwSetCxHandler
 *
 * PARAMETERS:  CxState         - the Cx state
 *              Handler         - new Cx state handler
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: This function is called to install a new Cx state handler.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiHwSetCxHandler (
    UINT32                  CxState,
    ACPI_C_STATE_HANDLER    Handler)
{
    FUNCTION_TRACE ("HwSetCxHandler");


    if ((CxState == 0) || (CxState >= MAX_CX_STATES) || !Handler)
    {
        return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    AcpiHwCxHandlers[CxState] = Handler;

    return_ACPI_STATUS(AE_OK);
}


/**************************************************************************
 *
 *  FUNCTION:    AcpiHwLocalPow
 *
 *  PARAMETERS:  x,y operands
 *
 *  RETURN:      result
 *
 *  DESCRIPTION: Compute x ^ y
 *
 *************************************************************************/

NATIVE_UINT
AcpiHwLocalPow (
    NATIVE_UINT             x,
    NATIVE_UINT             y)
{
    NATIVE_UINT             i;
    NATIVE_UINT             Result = 1;


    for (i = 0; i < y; i++)
    {
        Result = Result * x;
    }

    return (Result);
}


/**************************************************************************
 *
 *  FUNCTION:    AcpiHwEnableThrottling
 *
 *  PARAMETERS:  PblkAddress        - Address of Pcnt (Processor Control)
 *                                      register
 *
 *  RETURN:      none
 *
 *  DESCRIPTION: Enable throttling by setting the THT_EN bit.
 *
 *************************************************************************/

void
AcpiHwEnableThrottling (
    ACPI_IO_ADDRESS         PblkAddress)
{
    UINT32                  PblkValue;


    FUNCTION_TRACE ("EnableThrottling");


    PblkValue = AcpiOsIn32 (PblkAddress);
    PblkValue = PblkValue | BIT_4;
    AcpiOsOut32 (PblkAddress, PblkValue);

    return_VOID;
}


/**************************************************************************
 *
 *  FUNCTION:   AcpiHwDisableThrottling
 *
 *  PARAMETERS: PblkAddress         - Address of Pcnt (Processor Control)
 *                                      register
 *
 *  RETURN:     none
 *
 *  DESCRIPTION:Disable throttling by clearing the THT_EN bit
 *
 *************************************************************************/

void
AcpiHwDisableThrottling (
    ACPI_IO_ADDRESS         PblkAddress)
{
    UINT32                  PblkValue;


    FUNCTION_TRACE ("DisableThrottling");


    PblkValue = AcpiOsIn32 (PblkAddress);
    PblkValue = PblkValue & (~(UINT32)BIT_4);
    AcpiOsOut32 (PblkAddress, PblkValue);

    return_VOID;
}


/**************************************************************************
 *
 *  FUNCTION:    AcpiHwGetDutyCycle
 *
 *  PARAMETERS:  DutyOffset          Pcnt register duty cycle field offset
 *               PblkAddress         Pcnt register address in chipset
 *               NumThrottleStates   # of CPU throttle states this system
 *                                      supports
 *
 *  RETURN:      none
 *
 *  DESCRIPTION: Get the duty cycle from the chipset
 *
 *************************************************************************/

UINT32
AcpiHwGetDutyCycle (
    UINT8                   DutyOffset,
    ACPI_IO_ADDRESS         PblkAddress,
    UINT32                  NumThrottleStates)
{
    NATIVE_UINT             Index;
    UINT32                  Duty32Value;
    UINT32                  PcntMaskOffDutyField;


    FUNCTION_TRACE ("GetDutyCycle");


    /*
     *  Use NumThrottleStates - 1 as mask [ex. 8 - 1 = 7 (Fh)]
     *  and then shift it into the right position
     */
    PcntMaskOffDutyField = NumThrottleStates - 1;

    /*
     *  Read in the current value from the port
     */
    Duty32Value = AcpiOsIn32 ((ACPI_IO_ADDRESS) PblkAddress);

    /*
     *  Shift the the value to LSB
     */
    for (Index = 0; Index < (NATIVE_UINT) DutyOffset; Index++)
    {
        Duty32Value = Duty32Value >> 1;
    }

    /*
     *  Get the duty field only
     */
    Duty32Value = Duty32Value & PcntMaskOffDutyField;

    return_VALUE ((UINT32) Duty32Value);
}


/**************************************************************************
 *
 * FUNCTION:    AcpiHwProgramDutyCycle
 *
 * PARAMETERS:  DutyOffset          Pcnt register duty cycle field offset
 *              DutyCycle           duty cycle to program into chipset
 *              PblkAddress         Pcnt register address in chipset
 *              NumThrottleStates   # of CPU throttle states this system
 *                                      supports
 *
 * RETURN:      none
 *
 * DESCRIPTION: Program chipset with specified duty cycle by bit-shifting the
 *              duty cycle bits to the appropriate offset, reading the duty
 *              cycle register, OR-ing in the duty cycle, and writing it to
 *              the Pcnt register.
 *
 *************************************************************************/

void
AcpiHwProgramDutyCycle (
    UINT8                   DutyOffset,
    UINT32                  DutyCycle,
    ACPI_IO_ADDRESS         PblkAddress,
    UINT32                  NumThrottleStates)
{
    NATIVE_UINT             Index;
    UINT32                  Duty32Value;
    UINT32                  PcntMaskOffDutyField;
    UINT32                  PortValue;


    FUNCTION_TRACE ("HwProgramDutyCycle");


    /*
     *  valid DutyCycle passed
     */
    Duty32Value = DutyCycle;

    /*
     *  use NumThrottleStates - 1 as mask [ex. 8 - 1 = 7 (Fh)]
     *  and then shift it into the right position
     */
    PcntMaskOffDutyField = NumThrottleStates - 1;

    /*
     *  Shift the mask
     */
    for (Index = 0; Index < (NATIVE_UINT) DutyOffset; Index++)
    {
        PcntMaskOffDutyField = PcntMaskOffDutyField << 1;
        Duty32Value = Duty32Value << 1;
    }

    /*
     *  Read in the current value from the port
     */
    PortValue = AcpiOsIn32 ((ACPI_IO_ADDRESS) PblkAddress);

    /*
     *  Mask off the duty field so we don't OR in junk!
     */
    PortValue = PortValue & (~PcntMaskOffDutyField);

    /*
     *  OR in the bits we want to write out to the port
     */
    PortValue = (PortValue | Duty32Value) & (~(UINT32)BIT_4);

    /*
     *  write it to the port
     */
    AcpiOsOut32 ((ACPI_IO_ADDRESS) PblkAddress, PortValue);

    return_VOID;
}

 