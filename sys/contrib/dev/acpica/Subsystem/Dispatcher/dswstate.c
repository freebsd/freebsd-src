/******************************************************************************
 *
 * Module Name: dswstate - Dispatcher parse tree walk management routines
 *              $Revision: 34 $
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


#define __DSWSTATE_C__

#include "acpi.h"
#include "amlcode.h"
#include "acparser.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          DISPATCHER
        MODULE_NAME         ("dswstate")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultStackClear
 *
 * PARAMETERS:  WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reset this walk's result stack pointers to zero, thus setting
 *              the stack to zero.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultStackClear (
    ACPI_WALK_STATE         *WalkState)
{

    WalkState->NumResults = 0;
    WalkState->CurrentResult = 0;

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultStackPush
 *
 * PARAMETERS:  Object              - Object to push
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push an object onto this walk's result stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultStackPush (
    void                    *Object,
    ACPI_WALK_STATE         *WalkState)
{


    if (WalkState->NumResults >= OBJ_NUM_OPERANDS)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("DsResultStackPush: overflow! Obj=%p State=%p Num=%X\n",
            Object, WalkState, WalkState->NumResults));
        return (AE_STACK_OVERFLOW);
    }

    WalkState->Results [WalkState->NumResults] = Object;
    WalkState->NumResults++;

    DEBUG_PRINT (TRACE_EXEC,
        ("DsResultStackPush: Obj=%p State=%p Num=%X Cur=%X\n",
        Object, WalkState, WalkState->NumResults, WalkState->CurrentResult));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultStackPop
 *
 * PARAMETERS:  Object              - Where to return the popped object
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop an object off the bottom of this walk's result stack.  In
 *              other words, this is a FIFO.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultStackPop (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState)
{


    /* Check for stack underflow */

    if (WalkState->NumResults == 0)
    {
        DEBUG_PRINT (TRACE_EXEC,
            ("DsResultStackPop: Underflow - State=%p Cur=%X Num=%X\n",
            WalkState, WalkState->CurrentResult, WalkState->NumResults));
        return (AE_AML_NO_OPERAND);
    }


    /* Pop the stack */

    WalkState->NumResults--;

    /* Check for a valid result object */

    if (!WalkState->Results [WalkState->NumResults])
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("DsResultStackPop: Null operand! State=%p #Ops=%X\n",
            WalkState, WalkState->NumResults));
        return (AE_AML_NO_OPERAND);
    }

    *Object = WalkState->Results [WalkState->NumResults];
    WalkState->Results [WalkState->NumResults] = NULL;

    DEBUG_PRINT (TRACE_EXEC,
        ("DsResultStackPop: Obj=%p State=%p Num=%X Cur=%X\n",
        *Object, WalkState, WalkState->NumResults, WalkState->CurrentResult));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsObjStackDeleteAll
 *
 * PARAMETERS:  WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear the object stack by deleting all objects that are on it.
 *              Should be used with great care, if at all!
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsObjStackDeleteAll (
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;


    FUNCTION_TRACE_PTR ("DsObjStackDeleteAll", WalkState);


    /* The stack size is configurable, but fixed */

    for (i = 0; i < OBJ_NUM_OPERANDS; i++)
    {
        if (WalkState->Operands[i])
        {
            AcpiCmRemoveReference (WalkState->Operands[i]);
            WalkState->Operands[i] = NULL;
        }
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsObjStackPush
 *
 * PARAMETERS:  Object              - Object to push
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push an object onto this walk's object/operand stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsObjStackPush (
    void                    *Object,
    ACPI_WALK_STATE         *WalkState)
{


    /* Check for stack overflow */

    if (WalkState->NumOperands >= OBJ_NUM_OPERANDS)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("DsObjStackPush: overflow! Obj=%p State=%p #Ops=%X\n",
            Object, WalkState, WalkState->NumOperands));
        return (AE_STACK_OVERFLOW);
    }

    /* Put the object onto the stack */

    WalkState->Operands [WalkState->NumOperands] = Object;
    WalkState->NumOperands++;

    DEBUG_PRINT (TRACE_EXEC, ("DsObjStackPush: Obj=%p State=%p #Ops=%X\n",
                    Object, WalkState, WalkState->NumOperands));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsObjStackPopObject
 *
 * PARAMETERS:  PopCount            - Number of objects/entries to pop
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop this walk's object stack.  Objects on the stack are NOT
 *              deleted by this routine.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsObjStackPopObject (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState)
{


    /* Check for stack underflow */

    if (WalkState->NumOperands == 0)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("DsObjStackPop: Missing operand/stack empty! State=%p #Ops=%X\n",
            WalkState, WalkState->NumOperands));
        return (AE_AML_NO_OPERAND);
    }


    /* Pop the stack */

    WalkState->NumOperands--;

    /* Check for a valid operand */

    if (!WalkState->Operands [WalkState->NumOperands])
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("DsObjStackPop: Null operand! State=%p #Ops=%X\n",
            WalkState, WalkState->NumOperands));
        return (AE_AML_NO_OPERAND);
    }

    /* Get operand and set stack entry to null */

    *Object = WalkState->Operands [WalkState->NumOperands];
    WalkState->Operands [WalkState->NumOperands] = NULL;

    DEBUG_PRINT (TRACE_EXEC, ("DsObjStackPopObject: State=%p #Ops=%X\n",
                    WalkState, WalkState->NumOperands));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsObjStackPop
 *
 * PARAMETERS:  PopCount            - Number of objects/entries to pop
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop this walk's object stack.  Objects on the stack are NOT
 *              deleted by this routine.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsObjStackPop (
    UINT32                  PopCount,
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;


    for (i = 0; i < PopCount; i++)
    {
        /* Check for stack underflow */

        if (WalkState->NumOperands == 0)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("DsObjStackPop: Underflow! Count=%X State=%p #Ops=%X\n",
                PopCount, WalkState, WalkState->NumOperands));
            return (AE_STACK_UNDERFLOW);
        }

        /* Just set the stack entry to null */

        WalkState->NumOperands--;
        WalkState->Operands [WalkState->NumOperands] = NULL;
    }

    DEBUG_PRINT (TRACE_EXEC, ("DsObjStackPop: Count=%X State=%p #Ops=%X\n",
                    PopCount, WalkState, WalkState->NumOperands));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsObjStackPopAndDelete
 *
 * PARAMETERS:  PopCount            - Number of objects/entries to pop
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop this walk's object stack and delete each object that is
 *              popped off.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsObjStackPopAndDelete (
    UINT32                  PopCount,
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    for (i = 0; i < PopCount; i++)
    {
        /* Check for stack underflow */

        if (WalkState->NumOperands == 0)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("DsObjStackPop: Underflow! Count=%X State=%p #Ops=%X\n",
                PopCount, WalkState, WalkState->NumOperands));
            return (AE_STACK_UNDERFLOW);
        }

        /* Pop the stack and delete an object if present in this stack entry */

        WalkState->NumOperands--;
        ObjDesc = WalkState->Operands [WalkState->NumOperands];
        if (ObjDesc)
        {
            AcpiCmRemoveReference (WalkState->Operands [WalkState->NumOperands]);
            WalkState->Operands [WalkState->NumOperands] = NULL;
        }
    }

    DEBUG_PRINT (TRACE_EXEC, ("DsObjStackPop: Count=%X State=%p #Ops=%X\n",
                    PopCount, WalkState, WalkState->NumOperands));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsObjStackGetValue
 *
 * PARAMETERS:  Index               - Stack index whose value is desired.  Based
 *                                    on the top of the stack (index=0 == top)
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Retrieve an object from this walk's object stack.  Index must
 *              be within the range of the current stack pointer.
 *
 ******************************************************************************/

void *
AcpiDsObjStackGetValue (
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState)
{

    FUNCTION_TRACE_PTR ("DsObjStackGetValue", WalkState);


    /* Can't do it if the stack is empty */

    if (WalkState->NumOperands == 0)
    {
        return_VALUE (NULL);
    }

    /* or if the index is past the top of the stack */

    if (Index > (WalkState->NumOperands - (UINT32) 1))
    {
        return_VALUE (NULL);
    }


    return_PTR (WalkState->Operands[(NATIVE_UINT)(WalkState->NumOperands - 1) -
                    Index]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsGetCurrentWalkState
 *
 * PARAMETERS:  WalkList        - Get current active state for this walk list
 *
 * RETURN:      Pointer to the current walk state
 *
 * DESCRIPTION: Get the walk state that is at the head of the list (the "current"
 *              walk state.
 *
 ******************************************************************************/

ACPI_WALK_STATE *
AcpiDsGetCurrentWalkState (
    ACPI_WALK_LIST          *WalkList)

{

    DEBUG_PRINT (TRACE_PARSE, ("DsGetCurrentWalkState, =%p\n", WalkList->WalkState));

    if (!WalkList)
    {
        return (NULL);
    }

    return (WalkList->WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsPushWalkState
 *
 * PARAMETERS:  WalkState       - State to push
 *              WalkList        - The list that owns the walk stack
 *
 * RETURN:      None
 *
 * DESCRIPTION: Place the WalkState at the head of the state list.
 *
 ******************************************************************************/

static void
AcpiDsPushWalkState (
    ACPI_WALK_STATE         *WalkState,
    ACPI_WALK_LIST          *WalkList)
{


    FUNCTION_TRACE ("DsPushWalkState");


    WalkState->Next     = WalkList->WalkState;
    WalkList->WalkState = WalkState;

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsPopWalkState
 *
 * PARAMETERS:  WalkList        - The list that owns the walk stack
 *
 * RETURN:      A WalkState object popped from the stack
 *
 * DESCRIPTION: Remove and return the walkstate object that is at the head of
 *              the walk stack for the given walk list.  NULL indicates that
 *              the list is empty.
 *
 ******************************************************************************/

ACPI_WALK_STATE *
AcpiDsPopWalkState (
    ACPI_WALK_LIST          *WalkList)
{
    ACPI_WALK_STATE         *WalkState;


    FUNCTION_TRACE ("DsPopWalkState");


    WalkState = WalkList->WalkState;

    if (WalkState)
    {
        /* Next walk state becomes the current walk state */

        WalkList->WalkState = WalkState->Next;

        /*
         * Don't clear the NEXT field, this serves as an indicator
         * that there is a parent WALK STATE
         *     WalkState->Next = NULL;
         */
    }

    return_PTR (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsCreateWalkState
 *
 * PARAMETERS:  Origin          - Starting point for this walk
 *              WalkList        - Owning walk list
 *
 * RETURN:      Pointer to the new walk state.
 *
 * DESCRIPTION: Allocate and initialize a new walk state.  The current walk state
 *              is set to this new state.
 *
 ******************************************************************************/

ACPI_WALK_STATE *
AcpiDsCreateWalkState (
    ACPI_OWNER_ID           OwnerId,
    ACPI_PARSE_OBJECT       *Origin,
    ACPI_OPERAND_OBJECT     *MthDesc,
    ACPI_WALK_LIST          *WalkList)
{
    ACPI_WALK_STATE         *WalkState;


    FUNCTION_TRACE ("DsCreateWalkState");


    AcpiCmAcquireMutex (ACPI_MTX_CACHES);
    AcpiGbl_WalkStateCacheRequests++;

    /* Check the cache first */

    if (AcpiGbl_WalkStateCache)
    {
        /* There is an object available, use it */

        WalkState = AcpiGbl_WalkStateCache;
        AcpiGbl_WalkStateCache = WalkState->Next;

        AcpiGbl_WalkStateCacheHits++;
        AcpiGbl_WalkStateCacheDepth--;

        DEBUG_PRINT (TRACE_EXEC, ("DsCreateWalkState: State %p from cache\n", WalkState));

        AcpiCmReleaseMutex (ACPI_MTX_CACHES);
   }

    else
    {
        /* The cache is empty, create a new object */

        /* Avoid deadlock with AcpiCmCallocate */

        AcpiCmReleaseMutex (ACPI_MTX_CACHES);

        WalkState = AcpiCmCallocate (sizeof (ACPI_WALK_STATE));
        if (!WalkState)
        {
            return_VALUE (NULL);
        }
    }

    WalkState->DataType         = ACPI_DESC_TYPE_WALK;
    WalkState->OwnerId          = OwnerId;
    WalkState->Origin           = Origin;
    WalkState->MethodDesc       = MthDesc;

    /* Init the method args/local */

#ifndef _ACPI_ASL_COMPILER
    AcpiDsMethodDataInit (WalkState);
#endif

    /* Put the new state at the head of the walk list */

    AcpiDsPushWalkState (WalkState, WalkList);

    return_PTR (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsDeleteWalkState
 *
 * PARAMETERS:  WalkState       - State to delete
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete a walk state including all internal data structures
 *
 ******************************************************************************/

void
AcpiDsDeleteWalkState (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *State;


    FUNCTION_TRACE_PTR ("DsDeleteWalkState", WalkState);


    if (!WalkState)
    {
        return;
    }

    if (WalkState->DataType != ACPI_DESC_TYPE_WALK)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("DsDeleteWalkState: **** %p not a valid walk state\n", WalkState));
        return;
    }

    /* Always must free any linked control states */

    while (WalkState->ControlState)
    {
        State = WalkState->ControlState;
        WalkState->ControlState = State->Common.Next;

        AcpiCmDeleteGenericState (State);
    }


    /* Always must free any linked parse states */

    while (WalkState->ScopeInfo)
    {
        State = WalkState->ScopeInfo;
        WalkState->ScopeInfo = State->Common.Next;

        AcpiCmDeleteGenericState (State);
    }

    /* If walk cache is full, just free this wallkstate object */

    if (AcpiGbl_WalkStateCacheDepth >= MAX_WALK_CACHE_DEPTH)
    {
        AcpiCmFree (WalkState);
    }

    /* Otherwise put this object back into the cache */

    else
    {
        AcpiCmAcquireMutex (ACPI_MTX_CACHES);

        /* Clear the state */

        MEMSET (WalkState, 0, sizeof (ACPI_WALK_STATE));
        WalkState->DataType = ACPI_DESC_TYPE_WALK;

        /* Put the object at the head of the global cache list */

        WalkState->Next = AcpiGbl_WalkStateCache;
        AcpiGbl_WalkStateCache = WalkState;
        AcpiGbl_WalkStateCacheDepth++;


        AcpiCmReleaseMutex (ACPI_MTX_CACHES);
    }

    return_VOID;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiDsDeleteWalkStateCache
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Purge the global state object cache.  Used during subsystem
 *              termination.
 *
 ******************************************************************************/

void
AcpiDsDeleteWalkStateCache (
    void)
{
    ACPI_WALK_STATE         *Next;


    FUNCTION_TRACE ("DsDeleteWalkStateCache");


    /* Traverse the global cache list */

    while (AcpiGbl_WalkStateCache)
    {
        /* Delete one cached state object */

        Next = AcpiGbl_WalkStateCache->Next;
        AcpiCmFree (AcpiGbl_WalkStateCache);
        AcpiGbl_WalkStateCache = Next;
        AcpiGbl_WalkStateCacheDepth--;
    }

    return_VOID;
}


