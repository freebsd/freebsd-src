/******************************************************************************
 *
 * Module Name: evxface - External interfaces for ACPI events
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


#define __EVXFACE_C__

#include "acpi.h"
#include "accommon.h"
#include "acnamesp.h"
#include "acevents.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evxface")


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallExceptionHandler
 *
 * PARAMETERS:  Handler         - Pointer to the handler function for the
 *                                event
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Saves the pointer to the handler function
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallExceptionHandler (
    ACPI_EXCEPTION_HANDLER  Handler)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiInstallExceptionHandler);


    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Don't allow two handlers. */

    if (AcpiGbl_ExceptionHandler)
    {
        Status = AE_ALREADY_EXISTS;
        goto Cleanup;
    }

    /* Install the handler */

    AcpiGbl_ExceptionHandler = Handler;

Cleanup:
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInstallExceptionHandler)


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallGlobalEventHandler
 *
 * PARAMETERS:  Handler         - Pointer to the global event handler function
 *              Context         - Value passed to the handler on each event
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Saves the pointer to the handler function. The global handler
 *              is invoked upon each incoming GPE and Fixed Event. It is
 *              invoked at interrupt level at the time of the event dispatch.
 *              Can be used to update event counters, etc.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallGlobalEventHandler (
    ACPI_GBL_EVENT_HANDLER  Handler,
    void                    *Context)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiInstallGlobalEventHandler);


    /* Parameter validation */

    if (!Handler)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Don't allow two handlers. */

    if (AcpiGbl_GlobalEventHandler)
    {
        Status = AE_ALREADY_EXISTS;
        goto Cleanup;
    }

    AcpiGbl_GlobalEventHandler = Handler;
    AcpiGbl_GlobalEventHandlerContext = Context;


Cleanup:
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInstallGlobalEventHandler)


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


    ACPI_FUNCTION_TRACE (AcpiInstallFixedEventHandler);


    /* Parameter validation */

    if (Event > ACPI_EVENT_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Don't allow two handlers. */

    if (NULL != AcpiGbl_FixedEventHandlers[Event].Handler)
    {
        Status = AE_ALREADY_EXISTS;
        goto Cleanup;
    }

    /* Install the handler before enabling the event */

    AcpiGbl_FixedEventHandlers[Event].Handler = Handler;
    AcpiGbl_FixedEventHandlers[Event].Context = Context;

    Status = AcpiEnableEvent (Event, 0);
    if (ACPI_FAILURE (Status))
    {
        ACPI_WARNING ((AE_INFO, "Could not enable fixed event 0x%X", Event));

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
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInstallFixedEventHandler)


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


    ACPI_FUNCTION_TRACE (AcpiRemoveFixedEventHandler);


    /* Parameter validation */

    if (Event > ACPI_EVENT_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Disable the event before removing the handler */

    Status = AcpiDisableEvent (Event, 0);

    /* Always Remove the handler */

    AcpiGbl_FixedEventHandlers[Event].Handler = NULL;
    AcpiGbl_FixedEventHandlers[Event].Context = NULL;

    if (ACPI_FAILURE (Status))
    {
        ACPI_WARNING ((AE_INFO,
            "Could not write to fixed event enable register 0x%X", Event));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Disabled fixed event %X\n", Event));
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiRemoveFixedEventHandler)


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallNotifyHandler
 *
 * PARAMETERS:  Device          - The device for which notifies will be handled
 *              HandlerType     - The type of handler:
 *                                  ACPI_SYSTEM_NOTIFY: SystemHandler (00-7f)
 *                                  ACPI_DEVICE_NOTIFY: DriverHandler (80-ff)
 *                                  ACPI_ALL_NOTIFY:  both system and device
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
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiInstallNotifyHandler);


    /* Parameter validation */

    if ((!Device)  ||
        (!Handler) ||
        (HandlerType > ACPI_MAX_NOTIFY_HANDLER_TYPE))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsValidateHandle (Device);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /*
     * Root Object:
     * Registering a notify handler on the root object indicates that the
     * caller wishes to receive notifications for all objects. Note that
     * only one <external> global handler can be regsitered (per notify type).
     */
    if (Device == ACPI_ROOT_OBJECT)
    {
        /* Make sure the handler is not already installed */

        if (((HandlerType & ACPI_SYSTEM_NOTIFY) &&
                AcpiGbl_SystemNotify.Handler)       ||
            ((HandlerType & ACPI_DEVICE_NOTIFY) &&
                AcpiGbl_DeviceNotify.Handler))
        {
            Status = AE_ALREADY_EXISTS;
            goto UnlockAndExit;
        }

        if (HandlerType & ACPI_SYSTEM_NOTIFY)
        {
            AcpiGbl_SystemNotify.Node    = Node;
            AcpiGbl_SystemNotify.Handler = Handler;
            AcpiGbl_SystemNotify.Context = Context;
        }

        if (HandlerType & ACPI_DEVICE_NOTIFY)
        {
            AcpiGbl_DeviceNotify.Node    = Node;
            AcpiGbl_DeviceNotify.Handler = Handler;
            AcpiGbl_DeviceNotify.Context = Context;
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
        /* Notifies allowed on this object? */

        if (!AcpiEvIsNotifyObject (Node))
        {
            Status = AE_TYPE;
            goto UnlockAndExit;
        }

        /* Check for an existing internal object */

        ObjDesc = AcpiNsGetAttachedObject (Node);
        if (ObjDesc)
        {
            /* Object exists - make sure there's no handler */

            if (((HandlerType & ACPI_SYSTEM_NOTIFY) &&
                    ObjDesc->CommonNotify.SystemNotify)   ||
                ((HandlerType & ACPI_DEVICE_NOTIFY) &&
                    ObjDesc->CommonNotify.DeviceNotify))
            {
                Status = AE_ALREADY_EXISTS;
                goto UnlockAndExit;
            }
        }
        else
        {
            /* Create a new object */

            ObjDesc = AcpiUtCreateInternalObject (Node->Type);
            if (!ObjDesc)
            {
                Status = AE_NO_MEMORY;
                goto UnlockAndExit;
            }

            /* Attach new object to the Node */

            Status = AcpiNsAttachObject (Device, ObjDesc, Node->Type);

            /* Remove local reference to the object */

            AcpiUtRemoveReference (ObjDesc);
            if (ACPI_FAILURE (Status))
            {
                goto UnlockAndExit;
            }
        }

        /* Install the handler */

        NotifyObj = AcpiUtCreateInternalObject (ACPI_TYPE_LOCAL_NOTIFY);
        if (!NotifyObj)
        {
            Status = AE_NO_MEMORY;
            goto UnlockAndExit;
        }

        NotifyObj->Notify.Node    = Node;
        NotifyObj->Notify.Handler = Handler;
        NotifyObj->Notify.Context = Context;

        if (HandlerType & ACPI_SYSTEM_NOTIFY)
        {
            ObjDesc->CommonNotify.SystemNotify = NotifyObj;
        }

        if (HandlerType & ACPI_DEVICE_NOTIFY)
        {
            ObjDesc->CommonNotify.DeviceNotify = NotifyObj;
        }

        if (HandlerType == ACPI_ALL_NOTIFY)
        {
            /* Extra ref if installed in both */

            AcpiUtAddReference (NotifyObj);
        }
    }


UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInstallNotifyHandler)


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveNotifyHandler
 *
 * PARAMETERS:  Device          - The device for which notifies will be handled
 *              HandlerType     - The type of handler:
 *                                  ACPI_SYSTEM_NOTIFY: SystemHandler (00-7f)
 *                                  ACPI_DEVICE_NOTIFY: DriverHandler (80-ff)
 *                                  ACPI_ALL_NOTIFY:  both system and device
 *              Handler         - Address of the handler
 *
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
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiRemoveNotifyHandler);


    /* Parameter validation */

    if ((!Device)  ||
        (!Handler) ||
        (HandlerType > ACPI_MAX_NOTIFY_HANDLER_TYPE))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsValidateHandle (Device);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Root Object */

    if (Device == ACPI_ROOT_OBJECT)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
            "Removing notify handler for namespace root object\n"));

        if (((HandlerType & ACPI_SYSTEM_NOTIFY) &&
              !AcpiGbl_SystemNotify.Handler)        ||
            ((HandlerType & ACPI_DEVICE_NOTIFY) &&
              !AcpiGbl_DeviceNotify.Handler))
        {
            Status = AE_NOT_EXIST;
            goto UnlockAndExit;
        }

        if (HandlerType & ACPI_SYSTEM_NOTIFY)
        {
            AcpiGbl_SystemNotify.Node    = NULL;
            AcpiGbl_SystemNotify.Handler = NULL;
            AcpiGbl_SystemNotify.Context = NULL;
        }

        if (HandlerType & ACPI_DEVICE_NOTIFY)
        {
            AcpiGbl_DeviceNotify.Node    = NULL;
            AcpiGbl_DeviceNotify.Handler = NULL;
            AcpiGbl_DeviceNotify.Context = NULL;
        }
    }

    /* All Other Objects */

    else
    {
        /* Notifies allowed on this object? */

        if (!AcpiEvIsNotifyObject (Node))
        {
            Status = AE_TYPE;
            goto UnlockAndExit;
        }

        /* Check for an existing internal object */

        ObjDesc = AcpiNsGetAttachedObject (Node);
        if (!ObjDesc)
        {
            Status = AE_NOT_EXIST;
            goto UnlockAndExit;
        }

        /* Object exists - make sure there's an existing handler */

        if (HandlerType & ACPI_SYSTEM_NOTIFY)
        {
            NotifyObj = ObjDesc->CommonNotify.SystemNotify;
            if (!NotifyObj)
            {
                Status = AE_NOT_EXIST;
                goto UnlockAndExit;
            }

            if (NotifyObj->Notify.Handler != Handler)
            {
                Status = AE_BAD_PARAMETER;
                goto UnlockAndExit;
            }

            /* Remove the handler */

            ObjDesc->CommonNotify.SystemNotify = NULL;
            AcpiUtRemoveReference (NotifyObj);
        }

        if (HandlerType & ACPI_DEVICE_NOTIFY)
        {
            NotifyObj = ObjDesc->CommonNotify.DeviceNotify;
            if (!NotifyObj)
            {
                Status = AE_NOT_EXIST;
                goto UnlockAndExit;
            }

            if (NotifyObj->Notify.Handler != Handler)
            {
                Status = AE_BAD_PARAMETER;
                goto UnlockAndExit;
            }

            /* Remove the handler */

            ObjDesc->CommonNotify.DeviceNotify = NULL;
            AcpiUtRemoveReference (NotifyObj);
        }
    }


UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiRemoveNotifyHandler)


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallGpeHandler
 *
 * PARAMETERS:  GpeDevice       - Namespace node for the GPE (NULL for FADT
 *                                defined GPEs)
 *              GpeNumber       - The GPE number within the GPE block
 *              Type            - Whether this GPE should be treated as an
 *                                edge- or level-triggered interrupt.
 *              Address         - Address of the handler
 *              Context         - Value passed to the handler on each GPE
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for a General Purpose Event.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT32                  Type,
    ACPI_GPE_HANDLER        Address,
    void                    *Context)
{
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_GPE_HANDLER_INFO   *Handler;
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiInstallGpeHandler);


    /* Parameter validation */

    if ((!Address) || (Type & ~ACPI_GPE_XRUPT_TYPE_MASK))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Allocate and init handler object (before lock) */

    Handler = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_GPE_HANDLER_INFO));
    if (!Handler)
    {
        Status = AE_NO_MEMORY;
        goto UnlockAndExit;
    }

    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto FreeAndExit;
    }

    /* Make sure that there isn't a handler there already */

    if ((GpeEventInfo->Flags & ACPI_GPE_DISPATCH_MASK) ==
            ACPI_GPE_DISPATCH_HANDLER)
    {
        Status = AE_ALREADY_EXISTS;
        goto FreeAndExit;
    }

    Handler->Address = Address;
    Handler->Context = Context;
    Handler->MethodNode = GpeEventInfo->Dispatch.MethodNode;
    Handler->OriginalFlags = (UINT8) (GpeEventInfo->Flags &
        (ACPI_GPE_XRUPT_TYPE_MASK | ACPI_GPE_DISPATCH_MASK));

    /*
     * If the GPE is associated with a method, it may have been enabled
     * automatically during initialization, in which case it has to be
     * disabled now to avoid spurious execution of the handler.
     */
    if (((Handler->OriginalFlags & ACPI_GPE_DISPATCH_METHOD) ||
         (Handler->OriginalFlags & ACPI_GPE_DISPATCH_NOTIFY)) &&
        GpeEventInfo->RuntimeCount)
    {
        Handler->OriginallyEnabled = TRUE;
        (void) AcpiEvRemoveGpeReference (GpeEventInfo);

        /* Sanity check of original type against new type */

        if (Type != (UINT32) (GpeEventInfo->Flags & ACPI_GPE_XRUPT_TYPE_MASK))
        {
            ACPI_WARNING ((AE_INFO, "GPE type mismatch (level/edge)"));
        }
    }

    /* Install the handler */

    GpeEventInfo->Dispatch.Handler = Handler;

    /* Setup up dispatch flags to indicate handler (vs. method/notify) */

    GpeEventInfo->Flags &= ~(ACPI_GPE_XRUPT_TYPE_MASK | ACPI_GPE_DISPATCH_MASK);
    GpeEventInfo->Flags |= (UINT8) (Type | ACPI_GPE_DISPATCH_HANDLER);

    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);


UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);

FreeAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    ACPI_FREE (Handler);
    goto UnlockAndExit;
}

ACPI_EXPORT_SYMBOL (AcpiInstallGpeHandler)


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveGpeHandler
 *
 * PARAMETERS:  GpeDevice       - Namespace node for the GPE (NULL for FADT
 *                                defined GPEs)
 *              GpeNumber       - The event to remove a handler
 *              Address         - Address of the handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a handler for a General Purpose AcpiEvent.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRemoveGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    ACPI_GPE_HANDLER        Address)
{
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_GPE_HANDLER_INFO   *Handler;
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiRemoveGpeHandler);


    /* Parameter validation */

    if (!Address)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Make sure that a handler is indeed installed */

    if ((GpeEventInfo->Flags & ACPI_GPE_DISPATCH_MASK) !=
            ACPI_GPE_DISPATCH_HANDLER)
    {
        Status = AE_NOT_EXIST;
        goto UnlockAndExit;
    }

    /* Make sure that the installed handler is the same */

    if (GpeEventInfo->Dispatch.Handler->Address != Address)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Remove the handler */

    Handler = GpeEventInfo->Dispatch.Handler;

    /* Restore Method node (if any), set dispatch flags */

    GpeEventInfo->Dispatch.MethodNode = Handler->MethodNode;
    GpeEventInfo->Flags &=
        ~(ACPI_GPE_XRUPT_TYPE_MASK | ACPI_GPE_DISPATCH_MASK);
    GpeEventInfo->Flags |= Handler->OriginalFlags;

    /*
     * If the GPE was previously associated with a method and it was
     * enabled, it should be enabled at this point to restore the
     * post-initialization configuration.
     */
    if ((Handler->OriginalFlags & ACPI_GPE_DISPATCH_METHOD) &&
        Handler->OriginallyEnabled)
    {
        (void) AcpiEvAddGpeReference (GpeEventInfo);
    }

    /* Now we can free the handler object */

    ACPI_FREE (Handler);


UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiRemoveGpeHandler)


/*******************************************************************************
 *
 * FUNCTION:    AcpiAcquireGlobalLock
 *
 * PARAMETERS:  Timeout         - How long the caller is willing to wait
 *              Handle          - Where the handle to the lock is returned
 *                                (if acquired)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Acquire the ACPI Global Lock
 *
 * Note: Allows callers with the same thread ID to acquire the global lock
 * multiple times. In other words, externally, the behavior of the global lock
 * is identical to an AML mutex. On the first acquire, a new handle is
 * returned. On any subsequent calls to acquire by the same thread, the same
 * handle is returned.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAcquireGlobalLock (
    UINT16                  Timeout,
    UINT32                  *Handle)
{
    ACPI_STATUS             Status;


    if (!Handle)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Must lock interpreter to prevent race conditions */

    AcpiExEnterInterpreter ();

    Status = AcpiExAcquireMutexObject (Timeout,
                AcpiGbl_GlobalLockMutex, AcpiOsGetThreadId ());

    if (ACPI_SUCCESS (Status))
    {
        /* Return the global lock handle (updated in AcpiEvAcquireGlobalLock) */

        *Handle = AcpiGbl_GlobalLockHandle;
    }

    AcpiExExitInterpreter ();
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiAcquireGlobalLock)


/*******************************************************************************
 *
 * FUNCTION:    AcpiReleaseGlobalLock
 *
 * PARAMETERS:  Handle      - Returned from AcpiAcquireGlobalLock
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release the ACPI Global Lock. The handle must be valid.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiReleaseGlobalLock (
    UINT32                  Handle)
{
    ACPI_STATUS             Status;


    if (!Handle || (Handle != AcpiGbl_GlobalLockHandle))
    {
        return (AE_NOT_ACQUIRED);
    }

    Status = AcpiExReleaseMutexObject (AcpiGbl_GlobalLockMutex);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiReleaseGlobalLock)

