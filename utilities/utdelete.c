/*******************************************************************************
 *
 * Module Name: utdelete - object deletion and reference count utilities
 *
 ******************************************************************************/

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

#define __UTDELETE_C__

#include "acpi.h"
#include "accommon.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acevents.h"


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utdelete")

/* Local prototypes */

static void
AcpiUtDeleteInternalObj (
    ACPI_OPERAND_OBJECT     *Object);

static void
AcpiUtUpdateRefCount (
    ACPI_OPERAND_OBJECT     *Object,
    UINT32                  Action);


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteInternalObj
 *
 * PARAMETERS:  Object         - Object to be deleted
 *
 * RETURN:      None
 *
 * DESCRIPTION: Low level object deletion, after reference counts have been
 *              updated (All reference counts, including sub-objects!)
 *
 ******************************************************************************/

static void
AcpiUtDeleteInternalObj (
    ACPI_OPERAND_OBJECT     *Object)
{
    void                    *ObjPointer = NULL;
    ACPI_OPERAND_OBJECT     *HandlerDesc;
    ACPI_OPERAND_OBJECT     *SecondDesc;
    ACPI_OPERAND_OBJECT     *NextDesc;
    ACPI_OPERAND_OBJECT     **LastObjPtr;


    ACPI_FUNCTION_TRACE_PTR (UtDeleteInternalObj, Object);


    if (!Object)
    {
        return_VOID;
    }

    /*
     * Must delete or free any pointers within the object that are not
     * actual ACPI objects (for example, a raw buffer pointer).
     */
    switch (Object->Common.Type)
    {
    case ACPI_TYPE_STRING:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "**** String %p, ptr %p\n",
            Object, Object->String.Pointer));

        /* Free the actual string buffer */

        if (!(Object->Common.Flags & AOPOBJ_STATIC_POINTER))
        {
            /* But only if it is NOT a pointer into an ACPI table */

            ObjPointer = Object->String.Pointer;
        }
        break;


    case ACPI_TYPE_BUFFER:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "**** Buffer %p, ptr %p\n",
            Object, Object->Buffer.Pointer));

        /* Free the actual buffer */

        if (!(Object->Common.Flags & AOPOBJ_STATIC_POINTER))
        {
            /* But only if it is NOT a pointer into an ACPI table */

            ObjPointer = Object->Buffer.Pointer;
        }
        break;


    case ACPI_TYPE_PACKAGE:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, " **** Package of count %X\n",
            Object->Package.Count));

        /*
         * Elements of the package are not handled here, they are deleted
         * separately
         */

        /* Free the (variable length) element pointer array */

        ObjPointer = Object->Package.Elements;
        break;


    /*
     * These objects have a possible list of notify handlers.
     * Device object also may have a GPE block.
     */
    case ACPI_TYPE_DEVICE:

        if (Object->Device.GpeBlock)
        {
            (void) AcpiEvDeleteGpeBlock (Object->Device.GpeBlock);
        }

        /*lint -fallthrough */

    case ACPI_TYPE_PROCESSOR:
    case ACPI_TYPE_THERMAL:

        /* Walk the notify handler list for this object */

        HandlerDesc = Object->CommonNotify.Handler;
        while (HandlerDesc)
        {
            NextDesc = HandlerDesc->AddressSpace.Next;
            AcpiUtRemoveReference (HandlerDesc);
            HandlerDesc = NextDesc;
        }
        break;


    case ACPI_TYPE_MUTEX:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "***** Mutex %p, OS Mutex %p\n",
            Object, Object->Mutex.OsMutex));

        if (Object == AcpiGbl_GlobalLockMutex)
        {
            /* Global Lock has extra semaphore */

            (void) AcpiOsDeleteSemaphore (AcpiGbl_GlobalLockSemaphore);
            AcpiGbl_GlobalLockSemaphore = NULL;

            AcpiOsDeleteMutex (Object->Mutex.OsMutex);
            AcpiGbl_GlobalLockMutex = NULL;
        }
        else
        {
            AcpiExUnlinkMutex (Object);
            AcpiOsDeleteMutex (Object->Mutex.OsMutex);
        }
        break;


    case ACPI_TYPE_EVENT:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "***** Event %p, OS Semaphore %p\n",
            Object, Object->Event.OsSemaphore));

        (void) AcpiOsDeleteSemaphore (Object->Event.OsSemaphore);
        Object->Event.OsSemaphore = NULL;
        break;


    case ACPI_TYPE_METHOD:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "***** Method %p\n", Object));

        /* Delete the method mutex if it exists */

        if (Object->Method.Mutex)
        {
            AcpiOsDeleteMutex (Object->Method.Mutex->Mutex.OsMutex);
            AcpiUtDeleteObjectDesc (Object->Method.Mutex);
            Object->Method.Mutex = NULL;
        }
        break;


    case ACPI_TYPE_REGION:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "***** Region %p\n", Object));

        SecondDesc = AcpiNsGetSecondaryObject (Object);
        if (SecondDesc)
        {
            /*
             * Free the RegionContext if and only if the handler is one of the
             * default handlers -- and therefore, we created the context object
             * locally, it was not created by an external caller.
             */
            HandlerDesc = Object->Region.Handler;
            if (HandlerDesc)
            {
                NextDesc = HandlerDesc->AddressSpace.RegionList;
                LastObjPtr = &HandlerDesc->AddressSpace.RegionList;

                /* Remove the region object from the handler's list */

                while (NextDesc)
                {
                    if (NextDesc == Object)
                    {
                        *LastObjPtr = NextDesc->Region.Next;
                        break;
                    }

                    /* Walk the linked list of handler */

                    LastObjPtr = &NextDesc->Region.Next;
                    NextDesc = NextDesc->Region.Next;
                }

                if (HandlerDesc->AddressSpace.HandlerFlags &
                    ACPI_ADDR_HANDLER_DEFAULT_INSTALLED)
                {
                    /* Deactivate region and free region context */

                    if (HandlerDesc->AddressSpace.Setup)
                    {
                        (void) HandlerDesc->AddressSpace.Setup (Object,
                            ACPI_REGION_DEACTIVATE,
                            HandlerDesc->AddressSpace.Context,
                            &SecondDesc->Extra.RegionContext);
                    }
                }

                AcpiUtRemoveReference (HandlerDesc);
            }

            /* Now we can free the Extra object */

            AcpiUtDeleteObjectDesc (SecondDesc);
        }
        break;


    case ACPI_TYPE_BUFFER_FIELD:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "***** Buffer Field %p\n", Object));

        SecondDesc = AcpiNsGetSecondaryObject (Object);
        if (SecondDesc)
        {
            AcpiUtDeleteObjectDesc (SecondDesc);
        }
        break;


    case ACPI_TYPE_LOCAL_BANK_FIELD:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "***** Bank Field %p\n", Object));

        SecondDesc = AcpiNsGetSecondaryObject (Object);
        if (SecondDesc)
        {
            AcpiUtDeleteObjectDesc (SecondDesc);
        }
        break;


    default:
        break;
    }

    /* Free any allocated memory (pointer within the object) found above */

    if (ObjPointer)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "Deleting Object Subptr %p\n",
            ObjPointer));
        ACPI_FREE (ObjPointer);
    }

    /* Now the object can be safely deleted */

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "Deleting Object %p [%s]\n",
        Object, AcpiUtGetObjectTypeName (Object)));

    AcpiUtDeleteObjectDesc (Object);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteInternalObjectList
 *
 * PARAMETERS:  ObjList         - Pointer to the list to be deleted
 *
 * RETURN:      None
 *
 * DESCRIPTION: This function deletes an internal object list, including both
 *              simple objects and package objects
 *
 ******************************************************************************/

void
AcpiUtDeleteInternalObjectList (
    ACPI_OPERAND_OBJECT     **ObjList)
{
    ACPI_OPERAND_OBJECT     **InternalObj;


    ACPI_FUNCTION_TRACE (UtDeleteInternalObjectList);


    /* Walk the null-terminated internal list */

    for (InternalObj = ObjList; *InternalObj; InternalObj++)
    {
        AcpiUtRemoveReference (*InternalObj);
    }

    /* Free the combined parameter pointer list and object array */

    ACPI_FREE (ObjList);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtUpdateRefCount
 *
 * PARAMETERS:  Object          - Object whose ref count is to be updated
 *              Action          - What to do
 *
 * RETURN:      New ref count
 *
 * DESCRIPTION: Modify the ref count and return it.
 *
 ******************************************************************************/

static void
AcpiUtUpdateRefCount (
    ACPI_OPERAND_OBJECT     *Object,
    UINT32                  Action)
{
    UINT16                  Count;
    UINT16                  NewCount;


    ACPI_FUNCTION_NAME (UtUpdateRefCount);


    if (!Object)
    {
        return;
    }

    Count = Object->Common.ReferenceCount;
    NewCount = Count;

    /*
     * Perform the reference count action (increment, decrement, force delete)
     */
    switch (Action)
    {
    case REF_INCREMENT:

        NewCount++;
        Object->Common.ReferenceCount = NewCount;

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "Obj %p Refs=%X, [Incremented]\n",
            Object, NewCount));
        break;

    case REF_DECREMENT:

        if (Count < 1)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
                "Obj %p Refs=%X, can't decrement! (Set to 0)\n",
                Object, NewCount));

            NewCount = 0;
        }
        else
        {
            NewCount--;

            ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
                "Obj %p Refs=%X, [Decremented]\n",
                Object, NewCount));
        }

        if (Object->Common.Type == ACPI_TYPE_METHOD)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
                "Method Obj %p Refs=%X, [Decremented]\n", Object, NewCount));
        }

        Object->Common.ReferenceCount = NewCount;
        if (NewCount == 0)
        {
            AcpiUtDeleteInternalObj (Object);
        }
        break;

    case REF_FORCE_DELETE:

        ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
            "Obj %p Refs=%X, Force delete! (Set to 0)\n", Object, Count));

        NewCount = 0;
        Object->Common.ReferenceCount = NewCount;
        AcpiUtDeleteInternalObj (Object);
        break;

    default:

        ACPI_ERROR ((AE_INFO, "Unknown action (0x%X)", Action));
        break;
    }

    /*
     * Sanity check the reference count, for debug purposes only.
     * (A deleted object will have a huge reference count)
     */
    if (Count > ACPI_MAX_REFERENCE_COUNT)
    {
        ACPI_WARNING ((AE_INFO,
            "Large Reference Count (0x%X) in object %p", Count, Object));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtUpdateObjectReference
 *
 * PARAMETERS:  Object              - Increment ref count for this object
 *                                    and all sub-objects
 *              Action              - Either REF_INCREMENT or REF_DECREMENT or
 *                                    REF_FORCE_DELETE
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Increment the object reference count
 *
 * Object references are incremented when:
 * 1) An object is attached to a Node (namespace object)
 * 2) An object is copied (all subobjects must be incremented)
 *
 * Object references are decremented when:
 * 1) An object is detached from an Node
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtUpdateObjectReference (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_GENERIC_STATE      *StateList = NULL;
    ACPI_OPERAND_OBJECT     *NextObject = NULL;
    ACPI_GENERIC_STATE      *State;
    UINT32                  i;


    ACPI_FUNCTION_TRACE_PTR (UtUpdateObjectReference, Object);


    while (Object)
    {
        /* Make sure that this isn't a namespace handle */

        if (ACPI_GET_DESCRIPTOR_TYPE (Object) == ACPI_DESC_TYPE_NAMED)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
                "Object %p is NS handle\n", Object));
            return_ACPI_STATUS (AE_OK);
        }

        /*
         * All sub-objects must have their reference count incremented also.
         * Different object types have different subobjects.
         */
        switch (Object->Common.Type)
        {
        case ACPI_TYPE_DEVICE:
        case ACPI_TYPE_PROCESSOR:
        case ACPI_TYPE_POWER:
        case ACPI_TYPE_THERMAL:

            /* Update the notify objects for these types (if present) */

            AcpiUtUpdateRefCount (Object->CommonNotify.SystemNotify, Action);
            AcpiUtUpdateRefCount (Object->CommonNotify.DeviceNotify, Action);
            break;

        case ACPI_TYPE_PACKAGE:
            /*
             * We must update all the sub-objects of the package,
             * each of whom may have their own sub-objects.
             */
            for (i = 0; i < Object->Package.Count; i++)
            {
                /*
                 * Push each element onto the stack for later processing.
                 * Note: There can be null elements within the package,
                 * these are simply ignored
                 */
                Status = AcpiUtCreateUpdateStateAndPush (
                            Object->Package.Elements[i], Action, &StateList);
                if (ACPI_FAILURE (Status))
                {
                    goto ErrorExit;
                }
            }
            break;

        case ACPI_TYPE_BUFFER_FIELD:

            NextObject = Object->BufferField.BufferObj;
            break;

        case ACPI_TYPE_LOCAL_REGION_FIELD:

            NextObject = Object->Field.RegionObj;
            break;

        case ACPI_TYPE_LOCAL_BANK_FIELD:

            NextObject = Object->BankField.BankObj;
            Status = AcpiUtCreateUpdateStateAndPush (
                        Object->BankField.RegionObj, Action, &StateList);
            if (ACPI_FAILURE (Status))
            {
                goto ErrorExit;
            }
            break;

        case ACPI_TYPE_LOCAL_INDEX_FIELD:

            NextObject = Object->IndexField.IndexObj;
            Status = AcpiUtCreateUpdateStateAndPush (
                        Object->IndexField.DataObj, Action, &StateList);
            if (ACPI_FAILURE (Status))
            {
                goto ErrorExit;
            }
            break;

        case ACPI_TYPE_LOCAL_REFERENCE:
            /*
             * The target of an Index (a package, string, or buffer) or a named
             * reference must track changes to the ref count of the index or
             * target object.
             */
            if ((Object->Reference.Class == ACPI_REFCLASS_INDEX) ||
                (Object->Reference.Class== ACPI_REFCLASS_NAME))
            {
                NextObject = Object->Reference.Object;
            }
            break;

        case ACPI_TYPE_REGION:
        default:
            break; /* No subobjects for all other types */
        }

        /*
         * Now we can update the count in the main object. This can only
         * happen after we update the sub-objects in case this causes the
         * main object to be deleted.
         */
        AcpiUtUpdateRefCount (Object, Action);
        Object = NULL;

        /* Move on to the next object to be updated */

        if (NextObject)
        {
            Object = NextObject;
            NextObject = NULL;
        }
        else if (StateList)
        {
            State = AcpiUtPopGenericState (&StateList);
            Object = State->Update.Object;
            AcpiUtDeleteGenericState (State);
        }
    }

    return_ACPI_STATUS (AE_OK);


ErrorExit:

    ACPI_EXCEPTION ((AE_INFO, Status,
        "Could not update object reference count"));

    /* Free any stacked Update State objects */

    while (StateList)
    {
        State = AcpiUtPopGenericState (&StateList);
        AcpiUtDeleteGenericState (State);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAddReference
 *
 * PARAMETERS:  Object          - Object whose reference count is to be
 *                                incremented
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add one reference to an ACPI object
 *
 ******************************************************************************/

void
AcpiUtAddReference (
    ACPI_OPERAND_OBJECT     *Object)
{

    ACPI_FUNCTION_TRACE_PTR (UtAddReference, Object);


    /* Ensure that we have a valid object */

    if (!AcpiUtValidInternalObject (Object))
    {
        return_VOID;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
        "Obj %p Current Refs=%X [To Be Incremented]\n",
        Object, Object->Common.ReferenceCount));

    /* Increment the reference count */

    (void) AcpiUtUpdateObjectReference (Object, REF_INCREMENT);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtRemoveReference
 *
 * PARAMETERS:  Object         - Object whose ref count will be decremented
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decrement the reference count of an ACPI internal object
 *
 ******************************************************************************/

void
AcpiUtRemoveReference (
    ACPI_OPERAND_OBJECT     *Object)
{

    ACPI_FUNCTION_TRACE_PTR (UtRemoveReference, Object);


    /*
     * Allow a NULL pointer to be passed in, just ignore it. This saves
     * each caller from having to check. Also, ignore NS nodes.
     *
     */
    if (!Object ||
        (ACPI_GET_DESCRIPTOR_TYPE (Object) == ACPI_DESC_TYPE_NAMED))

    {
        return_VOID;
    }

    /* Ensure that we have a valid object */

    if (!AcpiUtValidInternalObject (Object))
    {
        return_VOID;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS,
        "Obj %p Current Refs=%X [To Be Decremented]\n",
        Object, Object->Common.ReferenceCount));

    /*
     * Decrement the reference count, and only actually delete the object
     * if the reference count becomes 0. (Must also decrement the ref count
     * of all subobjects!)
     */
    (void) AcpiUtUpdateObjectReference (Object, REF_DECREMENT);
    return_VOID;
}


