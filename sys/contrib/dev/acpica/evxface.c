/******************************************************************************
 *
 * Module Name: evxface - External interfaces for ACPI events
 *              $Revision: 118 $
 *
 *****************************************************************************/

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
 * Redistribution of source code of any substantial prton of the Covered
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


#define __EVXFACE_C__

#include "acpi.h"
#include "achware.h"
#include "acnamesp.h"
#include "acevents.h"
#include "amlcode.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
        MODULE_NAME         ("evxface")


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallFixedEventHandler
 *
 * PARAMETERS:  Event           - Event type to enable.
 *              Handler         - Pointer to the handler function for the
 *                                event
 *              Context         - Value passed to the handler on each GPE
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Saves the pointer to the handler function and then enables the
 *              event.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallFixedEventHandler (
    UINT32                  Event,
    ACPI_EVENT_HANDLER      Handler,
    void                    *Context)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AcpiInstallFixedEventHandler");


    /* Parameter validation */

    if (Event > ACPI_EVENT_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    AcpiUtAcquireMutex (ACPI_MTX_EVENTS);

    /* Don't allow two handlers. */

    if (NULL != AcpiGbl_FixedEventHandlers[Event].Handler)
    {
        Status = AE_ALREADY_EXISTS;
        goto Cleanup;
    }


    /* Install the handler before enabling the event */

    AcpiGbl_FixedEventHandlers[Event].Handler = Handler;
    AcpiGbl_FixedEventHandlers[Event].Context = Context;

    Status = AcpiEnableEvent (Event, ACPI_EVENT_FIXED, 0);
    if (!ACPI_SUCCESS (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_WARN, "Could not enable fixed event.\n"));

        /* Remove the handler */

        AcpiGbl_FixedEventHandlers[Event].Handler = NULL;
        AcpiGbl_FixedEventHandlers[Event].Context = NULL;
    }

    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
            "Enabled fixed event %X, Handler=%p\n", Event, Handler));
    }


Cleanup:
    AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveFixedEventHandler
 *
 * PARAMETERS:  Event           - Event type to disable.
 *              Handler         - Address of the handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disables the event and unregisters the event handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRemoveFixedEventHandler (
    UINT32                  Event,
    ACPI_EVENT_HANDLER      Handler)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiRemoveFixedEventHandler");


    /* Parameter validation */

    if (Event > ACPI_EVENT_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    AcpiUtAcquireMutex (ACPI_MTX_EVENTS);

    /* Disable the event before removing the handler */

    Status = AcpiDisableEvent(Event, ACPI_EVENT_FIXED, 0);

    /* Always Remove the handler */

    AcpiGbl_FixedEventHandlers[Event].Handler = NULL;
    AcpiGbl_FixedEventHandlers[Event].Context = NULL;

    if (!ACPI_SUCCESS (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_WARN,
            "Could not write to fixed event enable register.\n"));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Disabled fixed event %X.\n", Event));
    }

    AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallNotifyHandler
 *
 * PARAMETERS:  Device          - The device for which notifies will be handled
 *              HandlerType     - The type of handler:
 *                                  ACPI_SYSTEM_NOTIFY: SystemHandler (00-7f)
 *                                  ACPI_DEVICE_NOTIFY: DriverHandler (80-ff)
 *              Handler         - Address of the handler
 *              Context         - Value passed to the handler on each GPE
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for notifies on an ACPI device
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  HandlerType,
    ACPI_NOTIFY_HANDLER     Handler,
    void                    *Context)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *NotifyObj;
    ACPI_NAMESPACE_NODE     *DeviceNode;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiInstallNotifyHandler");


    /* Parameter validation */

    if ((!Handler) ||
        (HandlerType > ACPI_MAX_NOTIFY_HANDLER_TYPE))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

    /* Convert and validate the device handle */

    DeviceNode = AcpiNsMapHandleToNode (Device);
    if (!DeviceNode)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /*
     * Root Object:
     * Registering a notify handler on the root object indicates that the
     * caller wishes to receive notifications for all objects.  Note that
     * only one <external> global handler can be regsitered (per notify type).
     */
    if (Device == ACPI_ROOT_OBJECT)
    {
        /* Make sure the handler is not already installed */

        if (((HandlerType == ACPI_SYSTEM_NOTIFY) &&
              AcpiGbl_SysNotify.Handler) ||
            ((HandlerType == ACPI_DEVICE_NOTIFY) &&
              AcpiGbl_DrvNotify.Handler))
        {
            Status = AE_ALREADY_EXISTS;
            goto UnlockAndExit;
        }

        if (HandlerType == ACPI_SYSTEM_NOTIFY)
        {
            AcpiGbl_SysNotify.Node = DeviceNode;
            AcpiGbl_SysNotify.Handler = Handler;
            AcpiGbl_SysNotify.Context = Context;
        }
        else /* ACPI_DEVICE_NOTIFY */
        {
            AcpiGbl_DrvNotify.Node = DeviceNode;
            AcpiGbl_DrvNotify.Handler = Handler;
            AcpiGbl_DrvNotify.Context = Context;
        }

        /* Global notify handler installed */
    }

    /*
     * All Other Objects:
     * Caller will only receive notifications specific to the target object.
     * Note that only certain object types can receive notifications.
     */
    else
    {
        /*
         * These are the ONLY objects that can receive ACPI notifications
         */
        if ((DeviceNode->Type != ACPI_TYPE_DEVICE)     &&
            (DeviceNode->Type != ACPI_TYPE_PROCESSOR)  &&
            (DeviceNode->Type != ACPI_TYPE_POWER)      &&
            (DeviceNode->Type != ACPI_TYPE_THERMAL))
        {
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }

        /* Check for an existing internal object */

        ObjDesc = AcpiNsGetAttachedObject (DeviceNode);
        if (ObjDesc)
        {

            /* Object exists - make sure there's no handler */

            if (((HandlerType == ACPI_SYSTEM_NOTIFY) &&
                  ObjDesc->Device.SysHandler) ||
                ((HandlerType == ACPI_DEVICE_NOTIFY) &&
                  ObjDesc->Device.DrvHandler))
            {
                Status = AE_ALREADY_EXISTS;
                goto UnlockAndExit;
            }
        }

        else
        {
            /* Create a new object */

            ObjDesc = AcpiUtCreateInternalObject (DeviceNode->Type);
            if (!ObjDesc)
            {
                Status = AE_NO_MEMORY;
                goto UnlockAndExit;
            }

            /* Attach new object to the Node */

            Status = AcpiNsAttachObject (Device, ObjDesc, (UINT8) DeviceNode->Type);
            if (ACPI_FAILURE (Status))
            {
                goto UnlockAndExit;
            }
        }

        /* Install the handler */

        NotifyObj = AcpiUtCreateInternalObject (INTERNAL_TYPE_NOTIFY);
        if (!NotifyObj)
        {
            Status = AE_NO_MEMORY;
            goto UnlockAndExit;
        }

        NotifyObj->NotifyHandler.Node = DeviceNode;
        NotifyObj->NotifyHandler.Handler = Handler;
        NotifyObj->NotifyHandler.Context = Context;


        if (HandlerType == ACPI_SYSTEM_NOTIFY)
        {
            ObjDesc->Device.SysHandler = NotifyObj;
        }

        else /* ACPI_DEVICE_NOTIFY */
        {
            ObjDesc->Device.DrvHandler = NotifyObj;
        }
    }


UnlockAndExit:
    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveNotifyHandler
 *
 * PARAMETERS:  Device          - The device for which notifies will be handled
 *              HandlerType     - The type of handler:
 *                                  ACPI_SYSTEM_NOTIFY: SystemHandler (00-7f)
 *                                  ACPI_DEVICE_NOTIFY: DriverHandler (80-ff)
 *              Handler         - Address of the handler
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a handler for notifies on an ACPI device
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRemoveNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  HandlerType,
    ACPI_NOTIFY_HANDLER     Handler)
{
    ACPI_OPERAND_OBJECT     *NotifyObj;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *DeviceNode;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiRemoveNotifyHandler");


    /* Parameter validation */

    if ((!Handler) ||
        (HandlerType > ACPI_MAX_NOTIFY_HANDLER_TYPE))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

    /* Convert and validate the device handle */

    DeviceNode = AcpiNsMapHandleToNode (Device);
    if (!DeviceNode)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /*
     * Root Object
     */
    if (Device == ACPI_ROOT_OBJECT)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Removing notify handler for ROOT object.\n"));

        if (((HandlerType == ACPI_SYSTEM_NOTIFY) &&
              !AcpiGbl_SysNotify.Handler) ||
            ((HandlerType == ACPI_DEVICE_NOTIFY) &&
              !AcpiGbl_DrvNotify.Handler))
        {
            Status = AE_NOT_EXIST;
            goto UnlockAndExit;
        }

        if (HandlerType == ACPI_SYSTEM_NOTIFY)
        {
            AcpiGbl_SysNotify.Node    = NULL;
            AcpiGbl_SysNotify.Handler = NULL;
            AcpiGbl_SysNotify.Context = NULL;
        }
        else
        {
            AcpiGbl_DrvNotify.Node    = NULL;
            AcpiGbl_DrvNotify.Handler = NULL;
            AcpiGbl_DrvNotify.Context = NULL;
        }
    }

    /*
     * All Other Objects
     */
    else
    {
        /*
         * These are the ONLY objects that can receive ACPI notifications
         */
        if ((DeviceNode->Type != ACPI_TYPE_DEVICE)     &&
            (DeviceNode->Type != ACPI_TYPE_PROCESSOR)  &&
            (DeviceNode->Type != ACPI_TYPE_POWER)      &&
            (DeviceNode->Type != ACPI_TYPE_THERMAL))
        {
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }

        /* Check for an existing internal object */

        ObjDesc = AcpiNsGetAttachedObject (DeviceNode);
        if (!ObjDesc)
        {
            Status = AE_NOT_EXIST;
            goto UnlockAndExit;
        }

        /* Object exists - make sure there's an existing handler */

        if (HandlerType == ACPI_SYSTEM_NOTIFY)
        {
            NotifyObj = ObjDesc->Device.SysHandler;
        }
        else
        {
            NotifyObj = ObjDesc->Device.DrvHandler;
        }

        if ((!NotifyObj) ||
            (NotifyObj->NotifyHandler.Handler != Handler))
        {
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }

        /* Remove the handler */

        if (HandlerType == ACPI_SYSTEM_NOTIFY)
        {
            ObjDesc->Device.SysHandler = NULL;
        }
        else
        {
            ObjDesc->Device.DrvHandler = NULL;
        }

        AcpiUtRemoveReference (NotifyObj);
    }


UnlockAndExit:
    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallGpeHandler
 *
 * PARAMETERS:  GpeNumber       - The GPE number.  The numbering scheme is
 *                                bank 0 first, then bank 1.
 *              Type            - Whether this GPE should be treated as an
 *                                edge- or level-triggered interrupt.
 *              Handler         - Address of the handler
 *              Context         - Value passed to the handler on each GPE
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for a General Purpose Event.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallGpeHandler (
    UINT32                  GpeNumber,
    UINT32                  Type,
    ACPI_GPE_HANDLER        Handler,
    void                    *Context)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiInstallGpeHandler");


    /* Parameter validation */

    if (!Handler || (GpeNumber > ACPI_GPE_MAX))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Ensure that we have a valid GPE number */

    if (AcpiGbl_GpeValid[GpeNumber] == ACPI_GPE_INVALID)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    AcpiUtAcquireMutex (ACPI_MTX_EVENTS);

    /* Make sure that there isn't a handler there already */

    if (AcpiGbl_GpeInfo[GpeNumber].Handler)
    {
        Status = AE_ALREADY_EXISTS;
        goto Cleanup;
    }

    /* Install the handler */

    AcpiGbl_GpeInfo[GpeNumber].Handler = Handler;
    AcpiGbl_GpeInfo[GpeNumber].Context = Context;
    AcpiGbl_GpeInfo[GpeNumber].Type = (UINT8) Type;

    /* Clear the GPE (of stale events), the enable it */

    AcpiHwClearGpe (GpeNumber);
    AcpiHwEnableGpe (GpeNumber);


Cleanup:
    AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveGpeHandler
 *
 * PARAMETERS:  GpeNumber       - The event to remove a handler
 *              Handler         - Address of the handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a handler for a General Purpose AcpiEvent.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRemoveGpeHandler (
    UINT32                  GpeNumber,
    ACPI_GPE_HANDLER        Handler)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiRemoveGpeHandler");


    /* Parameter validation */

    if (!Handler || (GpeNumber > ACPI_GPE_MAX))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Ensure that we have a valid GPE number */

    if (AcpiGbl_GpeValid[GpeNumber] == ACPI_GPE_INVALID)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Disable the GPE before removing the handler */

    AcpiHwDisableGpe (GpeNumber);

    AcpiUtAcquireMutex (ACPI_MTX_EVENTS);

    /* Make sure that the installed handler is the same */

    if (AcpiGbl_GpeInfo[GpeNumber].Handler != Handler)
    {
        AcpiHwEnableGpe (GpeNumber);
        Status = AE_BAD_PARAMETER;
        goto Cleanup;
    }

    /* Remove the handler */

    AcpiGbl_GpeInfo[GpeNumber].Handler = NULL;
    AcpiGbl_GpeInfo[GpeNumber].Context = NULL;


Cleanup:
    AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAcquireGlobalLock
 *
 * PARAMETERS:  Timeout         - How long the caller is willing to wait
 *              OutHandle       - A handle to the lock if acquired
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Acquire the ACPI Global Lock
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAcquireGlobalLock (
    void)
{
    ACPI_STATUS             Status;


    Status = AcpiExEnterInterpreter ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /*
     * TBD: [Restructure] add timeout param to internal interface, and
     * perhaps INTERPRETER_LOCKED
     */
    Status = AcpiEvAcquireGlobalLock ();
    AcpiExExitInterpreter ();

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiReleaseGlobalLock
 *
 * PARAMETERS:  Handle      - Returned from AcpiAcquireGlobalLock
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release the ACPI Global Lock
 *
 ******************************************************************************/

ACPI_STATUS
AcpiReleaseGlobalLock (
    void)
{

    AcpiEvReleaseGlobalLock ();
    return (AE_OK);
}


