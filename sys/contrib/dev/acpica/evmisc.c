/******************************************************************************
 *
 * Module Name: evmisc - ACPI device notification handler dispatch
 *                       and ACPI Global Lock support
 *              $Revision: 47 $
 *
 *****************************************************************************/

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

#include "acpi.h"
#include "acevents.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "achware.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evmisc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGetGpeRegisterIndex
 *
 * PARAMETERS:  GpeNumber       - Raw GPE number
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Returns the register index (index into the GPE register info
 *              table) associated with this GPE.
 *
 ******************************************************************************/

UINT32
AcpiEvGetGpeRegisterIndex (
    UINT32                  GpeNumber)
{

    if (GpeNumber > AcpiGbl_GpeNumberMax)
    {
        return (ACPI_GPE_INVALID);
    }

    return (ACPI_DIV_8 (AcpiGbl_GpeNumberToIndex[GpeNumber].NumberIndex));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGetGpeNumberIndex
 *
 * PARAMETERS:  GpeNumber       - Raw GPE number
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Returns the number index (index into the GPE number info table)
 *              associated with this GPE.
 *
 ******************************************************************************/

UINT32
AcpiEvGetGpeNumberIndex (
    UINT32                  GpeNumber)
{

    if (GpeNumber > AcpiGbl_GpeNumberMax)
    {
        return (ACPI_GPE_INVALID);
    }

    return (AcpiGbl_GpeNumberToIndex[GpeNumber].NumberIndex);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvQueueNotifyRequest
 *
 * PARAMETERS:
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Dispatch a device notification event to a previously
 *              installed handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvQueueNotifyRequest (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  NotifyValue)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *HandlerObj = NULL;
    ACPI_GENERIC_STATE      *NotifyInfo;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_NAME ("EvQueueNotifyRequest");


    /*
     * For value 1 (Ejection Request), some device method may need to be run.
     * For value 2 (Device Wake) if _PRW exists, the _PS0 method may need to be run.
     * For value 0x80 (Status Change) on the power button or sleep button,
     * initiate soft-off or sleep operation?
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "Dispatching Notify(%X) on node %p\n", NotifyValue, Node));

    switch (NotifyValue)
    {
    case 0:
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Notify value: Re-enumerate Devices\n"));
        break;

    case 1:
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Notify value: Ejection Request\n"));
        break;

    case 2:
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Notify value: Device Wake\n"));
        break;

    case 0x80:
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Notify value: Status Change\n"));
        break;

    default:
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Unknown Notify Value: %X \n", NotifyValue));
        break;
    }

    /*
     * Get the notify object attached to the device Node
     */
    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (ObjDesc)
    {
        /* We have the notify object, Get the right handler */

        switch (Node->Type)
        {
        case ACPI_TYPE_DEVICE:

            if (NotifyValue <= ACPI_MAX_SYS_NOTIFY)
            {
                HandlerObj = ObjDesc->Device.SysHandler;
            }
            else
            {
                HandlerObj = ObjDesc->Device.DrvHandler;
            }
            break;


        case ACPI_TYPE_THERMAL:

            if (NotifyValue <= ACPI_MAX_SYS_NOTIFY)
            {
                HandlerObj = ObjDesc->ThermalZone.SysHandler;
            }
            else
            {
                HandlerObj = ObjDesc->ThermalZone.DrvHandler;
            }
            break;
        }
    }

    /* If there is any handler to run, schedule the dispatcher */

    if ((AcpiGbl_SysNotify.Handler && (NotifyValue <= ACPI_MAX_SYS_NOTIFY)) ||
        (AcpiGbl_DrvNotify.Handler && (NotifyValue > ACPI_MAX_SYS_NOTIFY))  ||
        HandlerObj)
    {
        NotifyInfo = AcpiUtCreateGenericState ();
        if (!NotifyInfo)
        {
            return (AE_NO_MEMORY);
        }

        NotifyInfo->Common.DataType   = ACPI_DESC_TYPE_STATE_NOTIFY;
        NotifyInfo->Notify.Node       = Node;
        NotifyInfo->Notify.Value      = (UINT16) NotifyValue;
        NotifyInfo->Notify.HandlerObj = HandlerObj;

        Status = AcpiOsQueueForExecution (OSD_PRIORITY_HIGH,
                        AcpiEvNotifyDispatch, NotifyInfo);
        if (ACPI_FAILURE (Status))
        {
            AcpiUtDeleteGenericState (NotifyInfo);
        }
    }

    if (!HandlerObj)
    {
        /* There is no per-device notify handler for this device */

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "No notify handler for node %p \n", Node));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvNotifyDispatch
 *
 * PARAMETERS:
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Dispatch a device notification event to a previously
 *              installed handler.
 *
 ******************************************************************************/

void ACPI_SYSTEM_XFACE
AcpiEvNotifyDispatch (
    void                    *Context)
{
    ACPI_GENERIC_STATE      *NotifyInfo = (ACPI_GENERIC_STATE *) Context;
    ACPI_NOTIFY_HANDLER     GlobalHandler = NULL;
    void                    *GlobalContext = NULL;
    ACPI_OPERAND_OBJECT     *HandlerObj;


    ACPI_FUNCTION_ENTRY ();


    /*
     * We will invoke a global notify handler if installed.
     * This is done _before_ we invoke the per-device handler attached to the device.
     */
    if (NotifyInfo->Notify.Value <= ACPI_MAX_SYS_NOTIFY)
    {
        /* Global system notification handler */

        if (AcpiGbl_SysNotify.Handler)
        {
            GlobalHandler = AcpiGbl_SysNotify.Handler;
            GlobalContext = AcpiGbl_SysNotify.Context;
        }
    }
    else
    {
        /* Global driver notification handler */

        if (AcpiGbl_DrvNotify.Handler)
        {
            GlobalHandler = AcpiGbl_DrvNotify.Handler;
            GlobalContext = AcpiGbl_DrvNotify.Context;
        }
    }

    /* Invoke the system handler first, if present */

    if (GlobalHandler)
    {
        GlobalHandler (NotifyInfo->Notify.Node, NotifyInfo->Notify.Value, GlobalContext);
    }

    /* Now invoke the per-device handler, if present */

    HandlerObj = NotifyInfo->Notify.HandlerObj;
    if (HandlerObj)
    {
        HandlerObj->NotifyHandler.Handler (NotifyInfo->Notify.Node, NotifyInfo->Notify.Value,
                        HandlerObj->NotifyHandler.Context);
    }

    /* All done with the info object */

    AcpiUtDeleteGenericState (NotifyInfo);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGlobalLockThread
 *
 * RETURN:      None
 *
 * DESCRIPTION: Invoked by SCI interrupt handler upon acquisition of the
 *              Global Lock.  Simply signal all threads that are waiting
 *              for the lock.
 *
 ******************************************************************************/

static void ACPI_SYSTEM_XFACE
AcpiEvGlobalLockThread (
    void                    *Context)
{

    /* Signal threads that are waiting for the lock */

    if (AcpiGbl_GlobalLockThreadCount)
    {
        /* Send sufficient units to the semaphore */

        AcpiOsSignalSemaphore (AcpiGbl_GlobalLockSemaphore,
                                AcpiGbl_GlobalLockThreadCount);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGlobalLockHandler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Invoked directly from the SCI handler when a global lock
 *              release interrupt occurs.  Grab the global lock and queue
 *              the global lock thread for execution
 *
 ******************************************************************************/

static UINT32
AcpiEvGlobalLockHandler (
    void                    *Context)
{
    BOOLEAN                 Acquired = FALSE;


    /*
     * Attempt to get the lock
     * If we don't get it now, it will be marked pending and we will
     * take another interrupt when it becomes free.
     */
    ACPI_ACQUIRE_GLOBAL_LOCK (AcpiGbl_CommonFACS.GlobalLock, Acquired);
    if (Acquired)
    {
        /* Got the lock, now wake all threads waiting for it */

        AcpiGbl_GlobalLockAcquired = TRUE;

        /* Run the Global Lock thread which will signal all waiting threads */

        AcpiOsQueueForExecution (OSD_PRIORITY_HIGH, AcpiEvGlobalLockThread,
                                    Context);
    }

    return (ACPI_INTERRUPT_HANDLED);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInitGlobalLockHandler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for the global lock release event
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInitGlobalLockHandler (void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("EvInitGlobalLockHandler");


    AcpiGbl_GlobalLockPresent = TRUE;
    Status = AcpiInstallFixedEventHandler (ACPI_EVENT_GLOBAL,
                                            AcpiEvGlobalLockHandler, NULL);

    /*
     * If the global lock does not exist on this platform, the attempt
     * to enable GBL_STATUS will fail (the GBL_ENABLE bit will not stick)
     * Map to AE_OK, but mark global lock as not present.
     * Any attempt to actually use the global lock will be flagged
     * with an error.
     */
    if (Status == AE_NO_HARDWARE_RESPONSE)
    {
        AcpiGbl_GlobalLockPresent = FALSE;
        Status = AE_OK;
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEvAcquireGlobalLock
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Attempt to gain ownership of the Global Lock.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiEvAcquireGlobalLock (
    UINT32                  Timeout)
{
    ACPI_STATUS             Status = AE_OK;
    BOOLEAN                 Acquired = FALSE;


    ACPI_FUNCTION_TRACE ("EvAcquireGlobalLock");


    /* Make sure that we actually have a global lock */

    if (!AcpiGbl_GlobalLockPresent)
    {
        return_ACPI_STATUS (AE_NO_GLOBAL_LOCK);
    }

    /* One more thread wants the global lock */

    AcpiGbl_GlobalLockThreadCount++;

    /* If we (OS side vs. BIOS side) have the hardware lock already, we are done */

    if (AcpiGbl_GlobalLockAcquired)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* We must acquire the actual hardware lock */

    ACPI_ACQUIRE_GLOBAL_LOCK (AcpiGbl_CommonFACS.GlobalLock, Acquired);
    if (Acquired)
    {
       /* We got the lock */

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Acquired the HW Global Lock\n"));

        AcpiGbl_GlobalLockAcquired = TRUE;
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Did not get the lock.  The pending bit was set above, and we must now
     * wait until we get the global lock released interrupt.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Waiting for the HW Global Lock\n"));

    /*
     * Acquire the global lock semaphore first.
     * Since this wait will block, we must release the interpreter
     */
    Status = AcpiExSystemWaitSemaphore (AcpiGbl_GlobalLockSemaphore,
                                            Timeout);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvReleaseGlobalLock
 *
 * DESCRIPTION: Releases ownership of the Global Lock.
 *
 ******************************************************************************/

void
AcpiEvReleaseGlobalLock (void)
{
    BOOLEAN                 Pending = FALSE;


    ACPI_FUNCTION_TRACE ("EvReleaseGlobalLock");


    if (!AcpiGbl_GlobalLockThreadCount)
    {
        ACPI_REPORT_WARNING(("Cannot release HW Global Lock, it has not been acquired\n"));
        return_VOID;
    }

    /* One fewer thread has the global lock */

    AcpiGbl_GlobalLockThreadCount--;
    if (AcpiGbl_GlobalLockThreadCount)
    {
        /* There are still some threads holding the lock, cannot release */

        return_VOID;
    }

    /*
     * No more threads holding lock, we can do the actual hardware
     * release
     */
    ACPI_RELEASE_GLOBAL_LOCK (AcpiGbl_CommonFACS.GlobalLock, Pending);
    AcpiGbl_GlobalLockAcquired = FALSE;

    /*
     * If the pending bit was set, we must write GBL_RLS to the control
     * register
     */
    if (Pending)
    {
        AcpiHwBitRegisterWrite (ACPI_BITREG_GLOBAL_LOCK_RELEASE, 1, ACPI_MTX_LOCK);
    }

    return_VOID;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEvTerminate
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: free memory allocated for table storage.
 *
 ******************************************************************************/

void
AcpiEvTerminate (void)
{

    ACPI_FUNCTION_TRACE ("EvTerminate");


    /*
     * Free global tables, etc.
     */
    if (AcpiGbl_GpeRegisterInfo)
    {
        ACPI_MEM_FREE (AcpiGbl_GpeRegisterInfo);
        AcpiGbl_GpeRegisterInfo = NULL;
    }

    if (AcpiGbl_GpeNumberInfo)
    {
        ACPI_MEM_FREE (AcpiGbl_GpeNumberInfo);
        AcpiGbl_GpeNumberInfo = NULL;
    }

    if (AcpiGbl_GpeNumberToIndex)
    {
        ACPI_MEM_FREE (AcpiGbl_GpeNumberToIndex);
        AcpiGbl_GpeNumberToIndex = NULL;
    }

    return_VOID;
}

