/*******************************************************************************
 *
 * Module Name: cmdelete - object deletion and reference count utilities
 *              $Revision: 57 $
 *
 ******************************************************************************/

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

#define __CMDELETE_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "actables.h"
#include "acparser.h"

#define _COMPONENT          MISCELLANEOUS
        MODULE_NAME         ("cmdelete")


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteInternalObj
 *
 * PARAMETERS:  *Object        - Pointer to the list to be deleted
 *
 * RETURN:      None
 *
 * DESCRIPTION: Low level object deletion, after reference counts have been
 *              updated (All reference counts, including sub-objects!)
 *
 ******************************************************************************/

void
AcpiCmDeleteInternalObj (
    ACPI_OPERAND_OBJECT     *Object)
{
    void                    *ObjPointer = NULL;
    ACPI_OPERAND_OBJECT     *HandlerDesc;


    FUNCTION_TRACE_PTR ("CmDeleteInternalObj", Object);


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

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: **** String %p, ptr %p\n",
            Object, Object->String.Pointer));

        /* Free the actual string buffer */

        ObjPointer = Object->String.Pointer;
        break;


    case ACPI_TYPE_BUFFER:

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: **** Buffer %p, ptr %p\n",
            Object, Object->Buffer.Pointer));

        /* Free the actual buffer */

        ObjPointer = Object->Buffer.Pointer;
        break;


    case ACPI_TYPE_PACKAGE:

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: **** Package of count %d\n",
            Object->Package.Count));

        /*
         * Elements of the package are not handled here, they are deleted
         * separately
         */

        /* Free the (variable length) element pointer array */

        ObjPointer = Object->Package.Elements;
        break;


    case ACPI_TYPE_MUTEX:

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: ***** Mutex %p, Semaphore %p\n",
            Object, Object->Mutex.Semaphore));

        AcpiOsDeleteSemaphore (Object->Mutex.Semaphore);
        break;


    case ACPI_TYPE_EVENT:

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: ***** Event %p, Semaphore %p\n",
            Object, Object->Event.Semaphore));

        AcpiOsDeleteSemaphore (Object->Event.Semaphore);
        Object->Event.Semaphore = NULL;
        break;


    case ACPI_TYPE_METHOD:

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: ***** Method %p\n", Object));

        /* Delete the method semaphore if it exists */

        if (Object->Method.Semaphore)
        {
            AcpiOsDeleteSemaphore (Object->Method.Semaphore);
            Object->Method.Semaphore = NULL;
        }

        break;


    case ACPI_TYPE_REGION:

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: ***** Region %p\n",
            Object));


        if (Object->Region.Extra)
        {
            /* 
             * Free the RegionContext if and only if the handler is one of the
             * default handlers -- and therefore, we created the context object
             * locally, it was not created by an external caller.
             */
            HandlerDesc = Object->Region.AddrHandler;
            if ((HandlerDesc) &&
                (HandlerDesc->AddrHandler.Hflags == ADDR_HANDLER_DEFAULT_INSTALLED))
            {
                ObjPointer = Object->Region.Extra->Extra.RegionContext;
            }

            /* Now we can free the Extra object */

            AcpiCmDeleteObjectDesc (Object->Region.Extra);
        }
        break;


    case ACPI_TYPE_FIELD_UNIT:

        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: ***** FieldUnit %p\n",
            Object));

        if (Object->FieldUnit.Extra)
        {
            AcpiCmDeleteObjectDesc (Object->FieldUnit.Extra);
        }
        break;

    default:
        break;
    }


    /*
     * Delete any allocated memory found above
     */

    if (ObjPointer)
    {
        if (!AcpiTbSystemTablePointer (ObjPointer))
        {
            DEBUG_PRINT (ACPI_INFO,
                ("CmDeleteInternalObj: Deleting Obj Ptr %p \n", ObjPointer));

            AcpiCmFree (ObjPointer);
        }
    }


    /* Only delete the object if it was dynamically allocated */

    if (Object->Common.Flags & AOPOBJ_STATIC_ALLOCATION)
    {
        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: Object %p [%s] static allocation, no delete\n",
            Object, AcpiCmGetTypeName (Object->Common.Type)));
    }

    if (!(Object->Common.Flags & AOPOBJ_STATIC_ALLOCATION))
    {
        DEBUG_PRINT (ACPI_INFO,
            ("CmDeleteInternalObj: Deleting object %p [%s]\n",
            Object, AcpiCmGetTypeName (Object->Common.Type)));

        AcpiCmDeleteObjectDesc (Object);

    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteInternalObjectList
 *
 * PARAMETERS:  *ObjList        - Pointer to the list to be deleted
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function deletes an internal object list, including both
 *              simple objects and package objects
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmDeleteInternalObjectList (
    ACPI_OPERAND_OBJECT     **ObjList)
{
    ACPI_OPERAND_OBJECT     **InternalObj;


    FUNCTION_TRACE ("CmDeleteInternalObjectList");


    /* Walk the null-terminated internal list */

    for (InternalObj = ObjList; *InternalObj; InternalObj++)
    {
        /*
         * Check for a package
         * Simple objects are simply stored in the array and do not
         * need to be deleted separately.
         */

        if (IS_THIS_OBJECT_TYPE ((*InternalObj), ACPI_TYPE_PACKAGE))
        {
            /* Delete the package */

            /*
             * TBD: [Investigate] This might not be the right thing to do,
             * depending on how the internal package object was allocated!!!
             */
            AcpiCmDeleteInternalObj (*InternalObj);
        }

    }

    /* Free the combined parameter pointer list and object array */

    AcpiCmFree (ObjList);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmUpdateRefCount
 *
 * PARAMETERS:  *Object         - Object whose ref count is to be updated
 *              Action          - What to do
 *
 * RETURN:      New ref count
 *
 * DESCRIPTION: Modify the ref count and return it.
 *
 ******************************************************************************/

void
AcpiCmUpdateRefCount (
    ACPI_OPERAND_OBJECT     *Object,
    UINT32                  Action)
{
    UINT16                  Count;
    UINT16                  NewCount;


    if (!Object)
    {
        return;
    }


    Count = Object->Common.ReferenceCount;
    NewCount = Count;

    /*
     * Reference count action (increment, decrement, or force delete)
     */

    switch (Action)
    {

    case REF_INCREMENT:

        NewCount++;
        Object->Common.ReferenceCount = NewCount;

        DEBUG_PRINT (ACPI_INFO,
            ("CmUpdateRefCount: Obj %p Refs=%d, [Incremented]\n",
            Object, NewCount));
        break;


    case REF_DECREMENT:

        if (Count < 1)
        {
            DEBUG_PRINT (ACPI_INFO,
                ("CmUpdateRefCount: Obj %p Refs=%d, can't decrement! (Set to 0)\n",
                Object, NewCount));

            NewCount = 0;
        }

        else
        {
            NewCount--;

            DEBUG_PRINT (ACPI_INFO,
                ("CmUpdateRefCount: Obj %p Refs=%d, [Decremented]\n",
                Object, NewCount));
        }

        if (Object->Common.Type == ACPI_TYPE_METHOD)
        {
            DEBUG_PRINT (ACPI_INFO,
                ("CmUpdateRefCount: Method Obj %p Refs=%d, [Decremented]\n",
                Object, NewCount));
        }

        Object->Common.ReferenceCount = NewCount;
        if (NewCount == 0)
        {
            AcpiCmDeleteInternalObj (Object);
        }

        break;


    case REF_FORCE_DELETE:

        DEBUG_PRINT (ACPI_INFO,
            ("CmUpdateRefCount: Obj %p Refs=%d, Force delete! (Set to 0)\n",
            Object, Count));

        NewCount = 0;
        Object->Common.ReferenceCount = NewCount;
        AcpiCmDeleteInternalObj (Object);
        break;


    default:

        DEBUG_PRINT (ACPI_ERROR,
            ("CmUpdateRefCount: Unknown action (%d)\n", Action));
        break;
    }


    /*
     * Sanity check the reference count, for debug purposes only.
     * (A deleted object will have a huge reference count)
     */

    if (Count > MAX_REFERENCE_COUNT)
    {

        DEBUG_PRINT (ACPI_ERROR,
            ("CmUpdateRefCount: **** AE_ERROR **** Invalid Reference Count (0x%X) in object %p\n\n",
            Count, Object));
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmUpdateObjectReference
 *
 * PARAMETERS:  *Object             - Increment ref count for this object
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
AcpiCmUpdateObjectReference (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action)
{
    ACPI_STATUS             Status;
    UINT32                  i;
    ACPI_OPERAND_OBJECT     *Next;
    ACPI_OPERAND_OBJECT     *New;
    ACPI_GENERIC_STATE       *StateList = NULL;
    ACPI_GENERIC_STATE       *State;


    FUNCTION_TRACE_PTR ("CmUpdateObjectReference", Object);


    /* Ignore a null object ptr */

    if (!Object)
    {
        return_ACPI_STATUS (AE_OK);
    }


    /*
     * Make sure that this isn't a namespace handle or an AML pointer
     */

    if (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_NAMED))
    {
        DEBUG_PRINT (ACPI_INFO,
            ("CmUpdateObjectReference: Object %p is NS handle\n",
            Object));
        return_ACPI_STATUS (AE_OK);
    }

    if (AcpiTbSystemTablePointer (Object))
    {
        DEBUG_PRINT (ACPI_INFO,
            ("CmUpdateObjectReference: **** Object %p is Pcode Ptr\n",
            Object));
        return_ACPI_STATUS (AE_OK);
    }


    State = AcpiCmCreateUpdateState (Object, Action);

    while (State)
    {

        Object = State->Update.Object;
        Action = State->Update.Value;
        AcpiCmDeleteGenericState (State);

        /*
         * All sub-objects must have their reference count incremented also.
         * Different object types have different subobjects.
         */
        switch (Object->Common.Type)
        {

        case ACPI_TYPE_DEVICE:

            Status = AcpiCmCreateUpdateStateAndPush (Object->Device.AddrHandler,
                                                     Action, &StateList);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            AcpiCmUpdateRefCount (Object->Device.SysHandler, Action);
            AcpiCmUpdateRefCount (Object->Device.DrvHandler, Action);
            break;


        case INTERNAL_TYPE_ADDRESS_HANDLER:

            /* Must walk list of address handlers */

            Next = Object->AddrHandler.Next;
            while (Next)
            {
                New = Next->AddrHandler.Next;
                AcpiCmUpdateRefCount (Next, Action);

                Next = New;
            }
            break;


        case ACPI_TYPE_PACKAGE:

            /*
             * We must update all the sub-objects of the package
             * (Each of whom may have their own sub-objects, etc.
             */
            for (i = 0; i < Object->Package.Count; i++)
            {
                /*
                 * Push each element onto the stack for later processing.
                 * Note: There can be null elements within the package,
                 * these are simply ignored
                 */

                Status = AcpiCmCreateUpdateStateAndPush (
                            Object->Package.Elements[i], Action, &StateList);
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }
            }
            break;


        case ACPI_TYPE_FIELD_UNIT:

            Status = AcpiCmCreateUpdateStateAndPush (
                        Object->FieldUnit.Container, Action, &StateList);

            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;


        case INTERNAL_TYPE_DEF_FIELD:

            Status = AcpiCmCreateUpdateStateAndPush (
                        Object->Field.Container, Action, &StateList);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
           break;


        case INTERNAL_TYPE_BANK_FIELD:

            Status = AcpiCmCreateUpdateStateAndPush (
                        Object->BankField.BankSelect, Action, &StateList);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            Status = AcpiCmCreateUpdateStateAndPush (
                        Object->BankField.Container, Action, &StateList);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;


        case ACPI_TYPE_REGION:

    /* TBD: [Investigate]
            AcpiCmUpdateRefCount (Object->Region.AddrHandler, Action);
    */
/*
            Status =
                AcpiCmCreateUpdateStateAndPush (Object->Region.AddrHandler,
                                                Action, &StateList);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
*/
            break;


        case INTERNAL_TYPE_REFERENCE:

            break;
        }


        /*
         * Now we can update the count in the main object.  This can only
         * happen after we update the sub-objects in case this causes the
         * main object to be deleted.
         */

        AcpiCmUpdateRefCount (Object, Action);


        /* Move on to the next object to be updated */

        State = AcpiCmPopGenericState (&StateList);
    }


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmAddReference
 *
 * PARAMETERS:  *Object        - Object whose reference count is to be
 *                                  incremented
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add one reference to an ACPI object
 *
 ******************************************************************************/

void
AcpiCmAddReference (
    ACPI_OPERAND_OBJECT     *Object)
{

    FUNCTION_TRACE_PTR ("CmAddReference", Object);


    /*
     * Ensure that we have a valid object
     */

    if (!AcpiCmValidInternalObject (Object))
    {
        return_VOID;
    }

    /*
     * We have a valid ACPI internal object, now increment the reference count
     */

    AcpiCmUpdateObjectReference  (Object, REF_INCREMENT);

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmRemoveReference
 *
 * PARAMETERS:  *Object        - Object whose ref count will be decremented
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decrement the reference count of an ACPI internal object
 *
 ******************************************************************************/

void
AcpiCmRemoveReference (
    ACPI_OPERAND_OBJECT     *Object)
{

    FUNCTION_TRACE_PTR ("CmRemoveReference", Object);


    /*
     * Ensure that we have a valid object
     */

    if (!AcpiCmValidInternalObject (Object))
    {
        return_VOID;
    }

    DEBUG_PRINT (ACPI_INFO, ("CmRemoveReference: Obj %p Refs=%d\n",
                                Object, Object->Common.ReferenceCount));

    /*
     * Decrement the reference count, and only actually delete the object
     * if the reference count becomes 0.  (Must also decrement the ref count
     * of all subobjects!)
     */

    AcpiCmUpdateObjectReference  (Object, REF_DECREMENT);

    return_VOID;
}


