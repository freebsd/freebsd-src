
/******************************************************************************
 *
 * Name: hwxface.c - Hardware access external interfaces
 *              $Revision: 32 $
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
        MODULE_NAME         ("hwxface")


/******************************************************************************
 *
 * Hardware globals
 *
 ******************************************************************************/


ACPI_C_STATE_HANDLER        AcpiHwCxHandlers[MAX_CX_STATES] =
                                            {NULL, AcpiHwEnterC1, NULL, NULL};

UINT32                      AcpiHwActiveCxState = 1;


/****************************************************************************
 *
 * FUNCTION:    AcpiGetProcessorId
 *
 * PARAMETERS:  ProcessorHandle     - handle for the cpu to get info about
 *              Id                  - location to return the processor ID
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Get the ACPI processor ID
 *
 ****************************************************************************/

ACPI_STATUS
AcpiGetProcessorId (
    ACPI_HANDLE             ProcessorHandle,
    UINT32                  *Id)
{
    ACPI_NAMESPACE_NODE     *CpuNode;
    ACPI_OPERAND_OBJECT     *CpuObj;


    FUNCTION_TRACE ("AcpiGetProcessorId");


    /*
     *  Have to at least have somewhere to return the ID
     */
    if (!Id)
    {
        return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    /*
     *  Convert and validate the device handle
     */

    CpuNode = AcpiNsConvertHandleToEntry (ProcessorHandle);
    if (!CpuNode)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

   /*
    *   Check for an existing internal object
    */

    CpuObj = AcpiNsGetAttachedObject ((ACPI_HANDLE) CpuNode);
    if (!CpuObj)
    {
        return_ACPI_STATUS (AE_NOT_FOUND);
    }

    /*
     * Return the ID
     */
    *Id = CpuObj->Processor.ProcId;

    return_ACPI_STATUS (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiGetProcessorThrottlingInfo
 *
 * PARAMETERS:  ProcessorHandle     - handle for the cpu to get info about
 *              UserBuffer          - caller supplied buffer
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Get throttling capabilities for the processor, this routine
 *              builds the data directly into the callers buffer
 *
 ****************************************************************************/

ACPI_STATUS
AcpiGetProcessorThrottlingInfo (
    ACPI_HANDLE             ProcessorHandle,
    ACPI_BUFFER             *UserBuffer)
{
    NATIVE_UINT             PercentStep;
    NATIVE_UINT             NextPercent;
    NATIVE_UINT             NumThrottleStates;
    NATIVE_UINT             BufferSpaceNeeded;
    NATIVE_UINT             i;
    UINT8                   DutyWidth = 0;
    ACPI_NAMESPACE_NODE     *CpuNode;
    ACPI_OPERAND_OBJECT     *CpuObj;
    ACPI_CPU_THROTTLING_STATE *StatePtr;


    FUNCTION_TRACE ("AcpiGetProcessorThrottlingInfo");


    /*
     *  Have to at least have a buffer to return info in
     */
    if (!UserBuffer)
    {
        return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    /*
     *  Convert and validate the device handle
     */

    CpuNode = AcpiNsConvertHandleToEntry (ProcessorHandle);
    if (!CpuNode)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

   /*
    *   Check for an existing internal object
    */

    CpuObj = AcpiNsGetAttachedObject ((ACPI_HANDLE) CpuNode);
    if (!CpuObj)
    {
        return_ACPI_STATUS (AE_NOT_FOUND);
    }

#ifndef _IA64
    /*
     * No Duty fields in IA64 tables
     */
    DutyWidth  = AcpiGbl_FACP->DutyWidth;
#endif

    /*
     *  P0 must always have a P_BLK all others may be null
     *  in either case, we can't throttle a processor that has no P_BLK
     *
     *  Also if no Duty width, one state and it is 100%
     *
     */
    if (!CpuObj->Processor.Length || !DutyWidth ||
        (ACPI_UINT16_MAX < CpuObj->Processor.Address))
    {
        /*
         *  AcpiEven though we can't throttle, we still have one state (100%)
         */
        NumThrottleStates = 1;
    }

    else
    {
        NumThrottleStates = (int) AcpiHwLocalPow (2,DutyWidth);
    }

    BufferSpaceNeeded = NumThrottleStates * sizeof (ACPI_CPU_THROTTLING_STATE);

    if ((UserBuffer->Length < BufferSpaceNeeded) || !UserBuffer->Pointer)
    {
        UserBuffer->Length = BufferSpaceNeeded;
        return_ACPI_STATUS (AE_BUFFER_OVERFLOW);
    }

    UserBuffer->Length  = BufferSpaceNeeded;
    StatePtr            = (ACPI_CPU_THROTTLING_STATE *) UserBuffer->Pointer;
    PercentStep         = 1000 / NumThrottleStates;

    /*
     * Build each entry in the buffer.  Note that we're using the value
     * 1000 and dividing each state by 10 to better avoid round-off
     * accumulation.  Also note that the throttling STATES are ordered
     * sequentially from 100% (state 0) on down (e.g. 87.5% = state 1),
     * which is exactly opposite from duty cycle values (12.5% = state 1).
     */
    for (i = 0, NextPercent = 1000; i < NumThrottleStates; i++)
    {
        StatePtr[i].StateNumber = i;
        StatePtr[i].PercentOfClock = NextPercent / 10;
        NextPercent -= PercentStep;
    }

    return_ACPI_STATUS(AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiGetProcessorThrottlingState
 *
 * PARAMETERS:  ProcessorHandle     - handle for the cpu to throttle
 *              ThrottleState       - throttling state to enter
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Get current hardware throttling state
 *
 ****************************************************************************/

ACPI_STATUS
AcpiGetProcessorThrottlingState (
    ACPI_HANDLE             ProcessorHandle,
    UINT32                  *ThrottleState)
{
    ACPI_NAMESPACE_NODE     *CpuNode;
    ACPI_OPERAND_OBJECT     *CpuObj;
    UINT32                  NumThrottleStates;
    UINT32                  DutyCycle;
    UINT8                   DutyOffset = 0;
    UINT8                   DutyWidth = 0;


    FUNCTION_TRACE ("AcpiGetProcessorThrottlingState");


    /* Convert and validate the device handle */

    CpuNode = AcpiNsConvertHandleToEntry (ProcessorHandle);
    if (!CpuNode || !ThrottleState)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

   /* Check for an existing internal object */

    CpuObj = AcpiNsGetAttachedObject ((ACPI_HANDLE) CpuNode);
    if (!CpuObj)
    {
        return_ACPI_STATUS (AE_NOT_FOUND);
    }

#ifndef _IA64
    /*
     * No Duty fields in IA64 tables
     */
    DutyOffset = AcpiGbl_FACP->DutyOffset;
    DutyWidth  = AcpiGbl_FACP->DutyWidth;
#endif

    /*
     *  Must have a valid P_BLK P0 must have a P_BLK all others may be null
     *  in either case, we can't thottle a processor that has no P_BLK
     *  that means we are in the only supported state (0 - 100%)
     *
     *  also, if DutyWidth is zero there are no additional states
     */
    if (!CpuObj->Processor.Length || !DutyWidth ||
        (ACPI_UINT16_MAX < CpuObj->Processor.Address))
    {
        *ThrottleState = 0;
        return_ACPI_STATUS(AE_OK);
    }

    NumThrottleStates = (UINT32) AcpiHwLocalPow (2,DutyWidth);

    /*
     *  Get the current duty cycle value.
     */
    DutyCycle = AcpiHwGetDutyCycle (DutyOffset,
                                    CpuObj->Processor.Address,
                                    NumThrottleStates);

    /*
     * Convert duty cycle to throttling state (invert).
     */
    if (DutyCycle == 0)
    {
        *ThrottleState = 0;
    }

    else
    {
        *ThrottleState = NumThrottleStates - DutyCycle;
    }

    return_ACPI_STATUS(AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiSetProcessorThrottlingState
 *
 * PARAMETERS:  ProcessorHandle     - handle for the cpu to throttle
 *              ThrottleState       - throttling state to enter
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Set hardware into requested throttling state, the handle
 *              passed in must have a valid P_BLK
 *
 ****************************************************************************/

ACPI_STATUS
AcpiSetProcessorThrottlingState (
    ACPI_HANDLE             ProcessorHandle,
    UINT32                  ThrottleState)
{
    ACPI_NAMESPACE_NODE    *CpuNode;
    ACPI_OPERAND_OBJECT    *CpuObj;
    UINT32                  NumThrottleStates = 0;
    UINT8                   DutyOffset = 0;
    UINT8                   DutyWidth = 0;
    UINT32                  DutyCycle = 0;


    FUNCTION_TRACE ("AcpiSetProcessorThrottlingState");


    /* Convert and validate the device handle */

    CpuNode = AcpiNsConvertHandleToEntry (ProcessorHandle);
    if (!CpuNode)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

   /* Check for an existing internal object */

    CpuObj = AcpiNsGetAttachedObject ((ACPI_HANDLE) CpuNode);
    if (!CpuObj)
    {
        return_ACPI_STATUS (AE_NOT_FOUND);
    }

#ifndef _IA64
    /*
     * No Duty fields in IA64 tables
     */
    DutyOffset = AcpiGbl_FACP->DutyOffset;
    DutyWidth  = AcpiGbl_FACP->DutyWidth;
#endif

    /*
     *  Must have a valid P_BLK P0 must have a P_BLK all others may be null
     *  in either case, we can't thottle a processor that has no P_BLK
     *  that means we are in the only supported state (0 - 100%)
     *
     *  also, if DutyWidth is zero there are no additional states
     */
    if (!CpuObj->Processor.Length || !DutyWidth ||
        (ACPI_UINT16_MAX < CpuObj->Processor.Address))
    {
        /*
         *  If caller wants to set the state to the only state we handle
         *  we're done.
         */
        if (ThrottleState == 0)
        {
            return_ACPI_STATUS (AE_OK);
        }

        /*
         *  Can't set this state
         */
        return_ACPI_STATUS (AE_SUPPORT);
    }

    NumThrottleStates = (int) AcpiHwLocalPow (2,DutyWidth);

    /*
     * Convert throttling state to duty cycle (invert).
     */
    if (ThrottleState > 0)
    {
        DutyCycle = NumThrottleStates - ThrottleState;
    }

    /*
     *  Turn off throttling (don't muck with the h/w while throttling).
     */
    AcpiHwDisableThrottling (CpuObj->Processor.Address);

    /*
     *  Program the throttling state.
     */
    AcpiHwProgramDutyCycle (DutyOffset, DutyCycle,
                            CpuObj->Processor.Address, NumThrottleStates);

    /*
     *  Only enable throttling for non-zero states (0 - 100%)
     */
    if (ThrottleState)
    {
        AcpiHwEnableThrottling (CpuObj->Processor.Address);
    }

    return_ACPI_STATUS(AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiGetProcessorCxInfo
 *
 * PARAMETERS:  ProcessorHandle     - handle for the cpu return info about
 *              UserBuffer          - caller supplied buffer
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Get Cx state latencies, this routine
 *              builds the data directly into the callers buffer
 *
 *
 ****************************************************************************/

ACPI_STATUS
AcpiGetProcessorCxInfo (
    ACPI_HANDLE             ProcessorHandle,
    ACPI_BUFFER             *UserBuffer)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  CxStateLatencies[4] = {0, 0, 0, 0};
    NATIVE_UINT             BufferSpaceNeeded = 0;
    ACPI_CX_STATE           *StatePtr = NULL;
    NATIVE_UINT             i = 0;


    FUNCTION_TRACE ("AcpiGetProcessorCxInfo");


    /*
     *  Have to at least have a buffer to return info in
     */
    if (!UserBuffer)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiHwGetCxInfo (CxStateLatencies);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    BufferSpaceNeeded = 4 * sizeof (ACPI_CX_STATE);

    if ((UserBuffer->Length < BufferSpaceNeeded) || !UserBuffer->Pointer)
    {
        UserBuffer->Length = BufferSpaceNeeded;
        return_ACPI_STATUS (AE_BUFFER_OVERFLOW);
    }

    UserBuffer->Length = BufferSpaceNeeded;

    StatePtr = (ACPI_CX_STATE *) UserBuffer->Pointer;

    for (i = 0; i < 4; i++)
    {
        StatePtr[i].StateNumber = i;
        StatePtr[i].Latency = CxStateLatencies[i];
    }

    return_ACPI_STATUS (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiSetProcessorSleepState
 *
 * PARAMETERS:  ProcessorHandle     - handle for the cpu return info about
 *              CxState             - the Cx sleeping state (C1-C3) to make
 *                                      'active'
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Sets which Cx state will be used during calls to
 *              AcpiProcessorSleep ()
 *
 ****************************************************************************/

ACPI_STATUS
AcpiSetProcessorSleepState (
    ACPI_HANDLE             ProcessorHandle,
    UINT32                  CxState)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AcpiSetProcessorSleepState");


    Status = AcpiHwSetCx (CxState);

    return_ACPI_STATUS (Status);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiProcessorSleep
 *
 * PARAMETERS:  ProcessorHandle     - handle for the cpu to put to sleep (Cx)
 *              TimeSleeping        - time (in microseconds) elapsed while
 *                                      sleeping
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Puts the processor into the currently active sleep state (Cx)
 *
 ****************************************************************************/

ACPI_STATUS
AcpiProcessorSleep (
    ACPI_HANDLE             ProcessorHandle,
    UINT32                  *PmTimerTicks)
{
    ACPI_NAMESPACE_NODE     *CpuNode = NULL;
    ACPI_OPERAND_OBJECT     *CpuObj = NULL;
    ACPI_IO_ADDRESS         Address = 0;


    /*
     * Convert ProcessorHandle to PblkAddres...
     */

    /* Convert and validate the device handle */

    CpuNode = AcpiNsConvertHandleToEntry (ProcessorHandle);
    if (!CpuNode)
    {
        return (AE_BAD_PARAMETER);
    }

   /* Check for an existing internal object */

    CpuObj = AcpiNsGetAttachedObject ((ACPI_HANDLE) CpuNode);
    if (!CpuObj)
    {
        return (AE_NOT_FOUND);
    }

    /* Get the processor register block (P_BLK) address */

    Address = CpuObj->Processor.Address;
    if (!CpuObj->Processor.Length)
    {
        /* Ensure a NULL addresss (note that P_BLK isn't required for C1) */

        Address = 0;
    }

    /*
     * Enter the currently active Cx sleep state.
     */
    return (AcpiHwEnterCx (Address, PmTimerTicks));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiGetTimer
 *
 * PARAMETERS:  none
 *
 * RETURN:      Current value of the ACPI PMT (timer)
 *
 * DESCRIPTION: Obtains current value of ACPI PMT
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTimer (
    UINT32                  *OutTicks)
{
    FUNCTION_TRACE ("AcpiGetTimer");


    if (!OutTicks)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    *OutTicks = AcpiHwPmtTicks ();

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiSetFirmwareWakingVector
 *
 * PARAMETERS:  PhysicalAddress     - Physical address of ACPI real mode
 *                                    entry point.
 *
 * RETURN:      AE_OK or AE_ERROR
 *
 * DESCRIPTION: Access function for dFirmwareWakingVector field in FACS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetFirmwareWakingVector (
    void                    *PhysicalAddress)
{
    FUNCTION_TRACE ("AcpiSetFirmwareWakingVector");


    /* Make sure that we have an FACS */

    if (!AcpiGbl_FACS)
    {
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /* Set the vector */

    * ((void **) AcpiGbl_FACS->FirmwareWakingVector) = PhysicalAddress;

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiGetFirmwareWakingVector
 *
 * PARAMETERS:  *PhysicalAddress    - Output buffer where contents of
 *                                    the dFirmwareWakingVector field of
 *                                    the FACS will be stored.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Access function for dFirmwareWakingVector field in FACS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetFirmwareWakingVector (
    void                    **PhysicalAddress)
{
    FUNCTION_TRACE ("AcpiGetFirmwareWakingVector");


    if (!PhysicalAddress)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Make sure that we have an FACS */

    if (!AcpiGbl_FACS)
    {
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /* Get the vector */

    *PhysicalAddress = * ((void **) AcpiGbl_FACS->FirmwareWakingVector);


    return_ACPI_STATUS (AE_OK);
}

/****************************************************************************
 *
 * FUNCTION:    AcpiSetSystemSleepState
 *
 * PARAMETERS:  SleepState           - the Sx sleeping state (S1-S5)
 *
 * RETURN:      Status of function
 *
 * DESCRIPTION: Puts the system into the specified sleeping state.
 *              Note that currently supports only S1 and S5.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiSetSystemSleepState (
    UINT8                   SleepState)
{
    UINT8                   Slp_TypA, Slp_TypB;
    UINT16                  Count;
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        Arg_list;
    ACPI_OBJECT             Arg;
    ACPI_OBJECT             Objects[3];	/* package plus 2 number objects */
    ACPI_BUFFER             ReturnBuffer;

    FUNCTION_TRACE ("AcpiSetSystemSxState");

    Slp_TypA = Slp_TypB = 0;

    if (SleepState > ACPI_STATE_S5)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiHwObtainSleepTypeRegisterData (SleepState, &Slp_TypA, &Slp_TypB);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * The value for ACPI_STATE_S5 is not 5 actually, so adjust it.
     */
    if (SleepState > ACPI_STATE_S4) 
    {
        SleepState--;
    }

    /*
     * Evaluate the _PTS method
     */
    MEMSET(&Arg_list, 0, sizeof(Arg_list));
    Arg_list.Count = 1;
    Arg_list.Pointer = &Arg;

    MEMSET(&Arg, 0, sizeof(Arg));
    Arg.Type = ACPI_TYPE_NUMBER;
    Arg.Number.Value = SleepState;

    AcpiEvaluateObject (NULL, "\\_PTS", &Arg_list, NULL);

    /*
     * Clear wake status
     */
    AcpiHwRegisterAccess (ACPI_WRITE, ACPI_MTX_DO_NOT_LOCK, WAK_STS, 1);

    /*
     * Set ACPI_SLP_TYPA/b and ACPI_SLP_EN
     */
    AcpiHwRegisterAccess (ACPI_WRITE, ACPI_MTX_DO_NOT_LOCK, SLP_TYPE_A, Slp_TypA);
    AcpiHwRegisterAccess (ACPI_WRITE, ACPI_MTX_DO_NOT_LOCK, SLP_TYPE_B, Slp_TypB);
    AcpiHwRegisterAccess (ACPI_WRITE, ACPI_MTX_DO_NOT_LOCK, SLP_EN, 1);

    /* 
     *  For S0 we don't wait for the WAK_STS bit.
     */
    if (SleepState != ACPI_STATE_S0)
    {
	/*
	 * Wait for WAK_STS bit
	 */

	Count = 0;
	while (!(AcpiHwRegisterAccess (ACPI_READ, ACPI_MTX_DO_NOT_LOCK, WAK_STS)))
	{
#if 1
	    AcpiOsSleepUsec(1000);	/* should we have OsdFunc for sleep or halt? */
#endif
	    /*
	     * Some BIOSes don't set WAK_STS at all,
	     * give up waiting for wakeup if we time out.
	     */

	    if (Count > 1000)
	    {
		break;		/* giving up */
	    }
	    Count++;
	}
    }

    /*
     * Evaluate the _WAK method
     */
    MEMSET(&Arg_list, 0, sizeof(Arg_list));
    Arg_list.Count = 1;
    Arg_list.Pointer = &Arg;

    MEMSET(&Arg, 0, sizeof(Arg));
    Arg.Type = ACPI_TYPE_NUMBER;
    Arg.Number.Value = SleepState;

    /* Set up _WAK result code buffer */
    MEMSET(Objects, 0, sizeof(Objects));
    ReturnBuffer.Length = sizeof(Objects);
    ReturnBuffer.Pointer = Objects;

    AcpiEvaluateObject (NULL, "\\_WAK", &Arg_list, &ReturnBuffer);

    Status = AE_OK;
    /* Check result code for _WAK */
    if (Objects[0].Type != ACPI_TYPE_PACKAGE ||
        Objects[1].Type != ACPI_TYPE_NUMBER  ||
        Objects[2].Type != ACPI_TYPE_NUMBER)
    {
        /*
         * In many BIOSes, _WAK doesn't return a result code.
         * We don't need to worry about it too much :-).
         */
        DEBUG_PRINT (ACPI_INFO,
            ("AcpiSetSystemSleepState: _WAK result code is corrupted, "
             "but should be OK.\n"));
    }

    else
    {
        /* evaluate status code */
        switch (Objects[1].Number.Value)
        {
        case 0x00000001:
            DEBUG_PRINT (ACPI_ERROR,
                ("AcpiSetSystemSleepState: Wake was signaled but failed "
                 "due to lack of power.\n"));
            Status = AE_ERROR;
            break;

        case 0x00000002:
            DEBUG_PRINT (ACPI_ERROR,
                ("AcpiSetSystemSleepState: Wake was signaled but failed "
                 "due to thermal condition.\n"));
            Status = AE_ERROR;
            break;
        }
        /* evaluate PSS code */
        if (Objects[2].Number.Value == 0)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("AcpiSetSystemSleepState: The targeted S-state was not "
                 "entered because of too much current being drawn from "
                 "the power supply.\n"));
            Status = AE_ERROR;
        }
    }

    return_ACPI_STATUS (Status);
}

