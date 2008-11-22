/******************************************************************************
 *
 * Module Name: dswstate - Dispatcher parse tree walk management routines
 *              $Revision: 81 $
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


#define __DSWSTATE_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acnamesp.h>

#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dswstate")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultInsert
 *
 * PARAMETERS:  Object              - Object to push
 *              Index               - Where to insert the object
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Insert an object onto this walk's result stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultInsert (
    void                    *Object,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_NAME ("DsResultInsert");


    State = WalkState->Results;
    if (!State)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No result object pushed! State=%p\n",
            WalkState));
        return (AE_NOT_EXIST);
    }

    if (Index >= ACPI_OBJ_NUM_OPERANDS)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Index out of range: %X Obj=%p State=%p Num=%X\n",
            Index, Object, WalkState, State->Results.NumResults));
        return (AE_BAD_PARAMETER);
    }

    if (!Object)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Null Object! Index=%X Obj=%p State=%p Num=%X\n",
            Index, Object, WalkState, State->Results.NumResults));
        return (AE_BAD_PARAMETER);
    }

    State->Results.ObjDesc [Index] = Object;
    State->Results.NumResults++;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
        "Obj=%p [%s] State=%p Num=%X Cur=%X\n",
        Object, Object ? AcpiUtGetObjectTypeName ((ACPI_OPERAND_OBJECT *) Object) : "NULL",
        WalkState, State->Results.NumResults, WalkState->CurrentResult));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultRemove
 *
 * PARAMETERS:  Object              - Where to return the popped object
 *              Index               - Where to extract the object
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop an object off the bottom of this walk's result stack.  In
 *              other words, this is a FIFO.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultRemove (
    ACPI_OPERAND_OBJECT     **Object,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_NAME ("DsResultRemove");


    State = WalkState->Results;
    if (!State)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No result object pushed! State=%p\n",
            WalkState));
        return (AE_NOT_EXIST);
    }

    if (Index >= ACPI_OBJ_MAX_OPERAND)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Index out of range: %X State=%p Num=%X\n",
            Index, WalkState, State->Results.NumResults));
    }

    /* Check for a valid result object */

    if (!State->Results.ObjDesc [Index])
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Null operand! State=%p #Ops=%X, Index=%X\n",
            WalkState, State->Results.NumResults, Index));
        return (AE_AML_NO_RETURN_VALUE);
    }

    /* Remove the object */

    State->Results.NumResults--;

    *Object = State->Results.ObjDesc [Index];
    State->Results.ObjDesc [Index] = NULL;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
        "Obj=%p [%s] Index=%X State=%p Num=%X\n",
        *Object, (*Object) ? AcpiUtGetObjectTypeName (*Object) : "NULL",
        Index, WalkState, State->Results.NumResults));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultPop
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
AcpiDsResultPop (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_NATIVE_UINT        Index;
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_NAME ("DsResultPop");


    State = WalkState->Results;
    if (!State)
    {
        return (AE_OK);
    }

    if (!State->Results.NumResults)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Result stack is empty! State=%p\n",
            WalkState));
        return (AE_AML_NO_RETURN_VALUE);
    }

    /* Remove top element */

    State->Results.NumResults--;

    for (Index = ACPI_OBJ_NUM_OPERANDS; Index; Index--)
    {
        /* Check for a valid result object */

        if (State->Results.ObjDesc [Index -1])
        {
            *Object = State->Results.ObjDesc [Index -1];
            State->Results.ObjDesc [Index -1] = NULL;

            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Obj=%p [%s] Index=%X State=%p Num=%X\n",
                *Object, (*Object) ? AcpiUtGetObjectTypeName (*Object) : "NULL",
                (UINT32) Index -1, WalkState, State->Results.NumResults));

            return (AE_OK);
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No result objects! State=%p\n", WalkState));
    return (AE_AML_NO_RETURN_VALUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultPopFromBottom
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
AcpiDsResultPopFromBottom (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_NATIVE_UINT        Index;
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_NAME ("DsResultPopFromBottom");


    State = WalkState->Results;
    if (!State)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Warning: No result object pushed! State=%p\n", WalkState));
        return (AE_NOT_EXIST);
    }

    if (!State->Results.NumResults)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No result objects! State=%p\n", WalkState));
        return (AE_AML_NO_RETURN_VALUE);
    }

    /* Remove Bottom element */

    *Object = State->Results.ObjDesc [0];

    /* Push entire stack down one element */

    for (Index = 0; Index < State->Results.NumResults; Index++)
    {
        State->Results.ObjDesc [Index] = State->Results.ObjDesc [Index + 1];
    }

    State->Results.NumResults--;

    /* Check for a valid result object */

    if (!*Object)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Null operand! State=%p #Ops=%X, Index=%X\n",
            WalkState, State->Results.NumResults, (UINT32) Index));
        return (AE_AML_NO_RETURN_VALUE);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Obj=%p [%s], Results=%p State=%p\n",
        *Object, (*Object) ? AcpiUtGetObjectTypeName (*Object) : "NULL",
        State, WalkState));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultPush
 *
 * PARAMETERS:  Object              - Where to return the popped object
 *              WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push an object onto the current result stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultPush (
    ACPI_OPERAND_OBJECT     *Object,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_NAME ("DsResultPush");


    State = WalkState->Results;
    if (!State)
    {
        ACPI_REPORT_ERROR (("No result stack frame during push\n"));
        return (AE_AML_INTERNAL);
    }

    if (State->Results.NumResults == ACPI_OBJ_NUM_OPERANDS)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Result stack overflow: Obj=%p State=%p Num=%X\n",
            Object, WalkState, State->Results.NumResults));
        return (AE_STACK_OVERFLOW);
    }

    if (!Object)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Null Object! Obj=%p State=%p Num=%X\n",
            Object, WalkState, State->Results.NumResults));
        return (AE_BAD_PARAMETER);
    }

    State->Results.ObjDesc [State->Results.NumResults] = Object;
    State->Results.NumResults++;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Obj=%p [%s] State=%p Num=%X Cur=%X\n",
        Object, Object ? AcpiUtGetObjectTypeName ((ACPI_OPERAND_OBJECT *) Object) : "NULL",
        WalkState, State->Results.NumResults, WalkState->CurrentResult));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultStackPush
 *
 * PARAMETERS:  WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push an object onto the WalkState result stack.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultStackPush (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *State;

    ACPI_FUNCTION_NAME ("DsResultStackPush");


    State = AcpiUtCreateGenericState ();
    if (!State)
    {
        return (AE_NO_MEMORY);
    }

    State->Common.DataType  = ACPI_DESC_TYPE_STATE_RESULT;
    AcpiUtPushGenericState (&WalkState->Results, State);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Results=%p State=%p\n",
        State, WalkState));

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResultStackPop
 *
 * PARAMETERS:  WalkState           - Current Walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop an object off of the WalkState result stack.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResultStackPop (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *State;

    ACPI_FUNCTION_NAME ("DsResultStackPop");


    /* Check for stack underflow */

    if (WalkState->Results == NULL)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Underflow - State=%p\n",
            WalkState));
        return (AE_AML_NO_OPERAND);
    }

    State = AcpiUtPopGenericState (&WalkState->Results);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
        "Result=%p RemainingResults=%X State=%p\n",
        State, State->Results.NumResults, WalkState));

    AcpiUtDeleteGenericState (State);

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


    ACPI_FUNCTION_TRACE_PTR ("DsObjStackDeleteAll", WalkState);


    /* The stack size is configurable, but fixed */

    for (i = 0; i < ACPI_OBJ_NUM_OPERANDS; i++)
    {
        if (WalkState->Operands[i])
        {
            AcpiUtRemoveReference (WalkState->Operands[i]);
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
    ACPI_FUNCTION_NAME ("DsObjStackPush");


    /* Check for stack overflow */

    if (WalkState->NumOperands >= ACPI_OBJ_NUM_OPERANDS)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "overflow! Obj=%p State=%p #Ops=%X\n",
            Object, WalkState, WalkState->NumOperands));
        return (AE_STACK_OVERFLOW);
    }

    /* Put the object onto the stack */

    WalkState->Operands [WalkState->NumOperands] = Object;
    WalkState->NumOperands++;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Obj=%p [%s] State=%p #Ops=%X\n",
                    Object, AcpiUtGetObjectTypeName ((ACPI_OPERAND_OBJECT *) Object),
                    WalkState, WalkState->NumOperands));

    return (AE_OK);
}


#if 0
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
    ACPI_FUNCTION_NAME ("DsObjStackPopObject");


    /* Check for stack underflow */

    if (WalkState->NumOperands == 0)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Missing operand/stack empty! State=%p #Ops=%X\n",
            WalkState, WalkState->NumOperands));
        *Object = NULL;
        return (AE_AML_NO_OPERAND);
    }

    /* Pop the stack */

    WalkState->NumOperands--;

    /* Check for a valid operand */

    if (!WalkState->Operands [WalkState->NumOperands])
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Null operand! State=%p #Ops=%X\n",
            WalkState, WalkState->NumOperands));
        *Object = NULL;
        return (AE_AML_NO_OPERAND);
    }

    /* Get operand and set stack entry to null */

    *Object = WalkState->Operands [WalkState->NumOperands];
    WalkState->Operands [WalkState->NumOperands] = NULL;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Obj=%p [%s] State=%p #Ops=%X\n",
                    *Object, AcpiUtGetObjectTypeName (*Object),
                    WalkState, WalkState->NumOperands));

    return (AE_OK);
}
#endif


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

    ACPI_FUNCTION_NAME ("DsObjStackPop");


    for (i = 0; i < PopCount; i++)
    {
        /* Check for stack underflow */

        if (WalkState->NumOperands == 0)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Underflow! Count=%X State=%p #Ops=%X\n",
                PopCount, WalkState, WalkState->NumOperands));
            return (AE_STACK_UNDERFLOW);
        }

        /* Just set the stack entry to null */

        WalkState->NumOperands--;
        WalkState->Operands [WalkState->NumOperands] = NULL;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Count=%X State=%p #Ops=%X\n",
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


    ACPI_FUNCTION_NAME ("DsObjStackPopAndDelete");


    for (i = 0; i < PopCount; i++)
    {
        /* Check for stack underflow */

        if (WalkState->NumOperands == 0)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Underflow! Count=%X State=%p #Ops=%X\n",
                PopCount, WalkState, WalkState->NumOperands));
            return (AE_STACK_UNDERFLOW);
        }

        /* Pop the stack and delete an object if present in this stack entry */

        WalkState->NumOperands--;
        ObjDesc = WalkState->Operands [WalkState->NumOperands];
        if (ObjDesc)
        {
            AcpiUtRemoveReference (WalkState->Operands [WalkState->NumOperands]);
            WalkState->Operands [WalkState->NumOperands] = NULL;
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Count=%X State=%p #Ops=%X\n",
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

    ACPI_FUNCTION_TRACE_PTR ("DsObjStackGetValue", WalkState);


    /* Can't do it if the stack is empty */

    if (WalkState->NumOperands == 0)
    {
        return_PTR (NULL);
    }

    /* or if the index is past the top of the stack */

    if (Index > (WalkState->NumOperands - (UINT32) 1))
    {
        return_PTR (NULL);
    }

    return_PTR (WalkState->Operands[(ACPI_NATIVE_UINT)(WalkState->NumOperands - 1) -
                    Index]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsGetCurrentWalkState
 *
 * PARAMETERS:  Thread          - Get current active state for this Thread
 *
 * RETURN:      Pointer to the current walk state
 *
 * DESCRIPTION: Get the walk state that is at the head of the list (the "current"
 *              walk state.)
 *
 ******************************************************************************/

ACPI_WALK_STATE *
AcpiDsGetCurrentWalkState (
    ACPI_THREAD_STATE       *Thread)

{
    ACPI_FUNCTION_NAME ("DsGetCurrentWalkState");


    if (!Thread)
    {
        return (NULL);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Current WalkState %p\n",
        Thread->WalkStateList));

    return (Thread->WalkStateList);
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

void
AcpiDsPushWalkState (
    ACPI_WALK_STATE         *WalkState,
    ACPI_THREAD_STATE       *Thread)
{
    ACPI_FUNCTION_TRACE ("DsPushWalkState");


    WalkState->Next       = Thread->WalkStateList;
    Thread->WalkStateList = WalkState;

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
    ACPI_THREAD_STATE       *Thread)
{
    ACPI_WALK_STATE         *WalkState;


    ACPI_FUNCTION_TRACE ("DsPopWalkState");


    WalkState = Thread->WalkStateList;

    if (WalkState)
    {
        /* Next walk state becomes the current walk state */

        Thread->WalkStateList = WalkState->Next;

        /*
         * Don't clear the NEXT field, this serves as an indicator
         * that there is a parent WALK STATE
         *     NO: WalkState->Next = NULL;
         */
    }

    return_PTR (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsCreateWalkState
 *
 * PARAMETERS:  Origin          - Starting point for this walk
 *              Thread          - Current thread state
 *
 * RETURN:      Pointer to the new walk state.
 *
 * DESCRIPTION: Allocate and initialize a new walk state.  The current walk
 *              state is set to this new state.
 *
 ******************************************************************************/

ACPI_WALK_STATE *
AcpiDsCreateWalkState (
    ACPI_OWNER_ID           OwnerId,
    ACPI_PARSE_OBJECT       *Origin,
    ACPI_OPERAND_OBJECT     *MthDesc,
    ACPI_THREAD_STATE       *Thread)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("DsCreateWalkState");


    WalkState = AcpiUtAcquireFromCache (ACPI_MEM_LIST_WALK);
    if (!WalkState)
    {
        return_PTR (NULL);
    }

    WalkState->DataType         = ACPI_DESC_TYPE_WALK;
    WalkState->OwnerId          = OwnerId;
    WalkState->Origin           = Origin;
    WalkState->MethodDesc       = MthDesc;
    WalkState->Thread           = Thread;

    WalkState->ParserState.StartOp = Origin;

    /* Init the method args/local */

#if (!defined (ACPI_NO_METHOD_EXECUTION) && !defined (ACPI_CONSTANT_EVAL_ONLY))
    AcpiDsMethodDataInit (WalkState);
#endif

    /* Create an initial result stack entry */

    Status = AcpiDsResultStackPush (WalkState);
    if (ACPI_FAILURE (Status))
    {
        AcpiUtReleaseToCache (ACPI_MEM_LIST_WALK, WalkState);
        return_PTR (NULL);
    }

    /* Put the new state at the head of the walk list */

    if (Thread)
    {
        AcpiDsPushWalkState (WalkState, Thread);
    }

    return_PTR (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsInitAmlWalk
 *
 * PARAMETERS:  WalkState       - New state to be initialized
 *              Op              - Current parse op
 *              MethodNode      - Control method NS node, if any
 *              AmlStart        - Start of AML
 *              AmlLength       - Length of AML
 *              Params          - Method args, if any
 *              ReturnObjDesc   - Where to store a return object, if any
 *              PassNumber      - 1, 2, or 3
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize a walk state for a pass 1 or 2 parse tree walk
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsInitAmlWalk (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *MethodNode,
    UINT8                   *AmlStart,
    UINT32                  AmlLength,
    ACPI_PARAMETER_INFO     *Info,
    UINT32                  PassNumber)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_STATE        *ParserState = &WalkState->ParserState;
    ACPI_PARSE_OBJECT       *ExtraOp;


    ACPI_FUNCTION_TRACE ("DsInitAmlWalk");


    WalkState->ParserState.Aml      =
    WalkState->ParserState.AmlStart = AmlStart;
    WalkState->ParserState.AmlEnd   =
    WalkState->ParserState.PkgEnd   = AmlStart + AmlLength;

    /* The NextOp of the NextWalk will be the beginning of the method */

    WalkState->NextOp               = NULL;

    if (Info)
    {
        if (Info->ParameterType == ACPI_PARAM_GPE)
        {
            WalkState->GpeEventInfo = ACPI_CAST_PTR (ACPI_GPE_EVENT_INFO,
                                            Info->Parameters);
        }
        else
        {
            WalkState->Params               = Info->Parameters;
            WalkState->CallerReturnDesc     = &Info->ReturnObject;
        }
    }

    Status = AcpiPsInitScope (&WalkState->ParserState, Op);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (MethodNode)
    {
        WalkState->ParserState.StartNode = MethodNode;
        WalkState->WalkType              = ACPI_WALK_METHOD;
        WalkState->MethodNode            = MethodNode;
        WalkState->MethodDesc            = AcpiNsGetAttachedObject (MethodNode);

        /* Push start scope on scope stack and make it current  */

        Status = AcpiDsScopeStackPush (MethodNode, ACPI_TYPE_METHOD, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Init the method arguments */

        Status = AcpiDsMethodDataInitArgs (WalkState->Params, ACPI_METHOD_NUM_ARGS, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }
    else
    {
        /*
         * Setup the current scope.
         * Find a Named Op that has a namespace node associated with it.
         * search upwards from this Op.  Current scope is the first
         * Op with a namespace node.
         */
        ExtraOp = ParserState->StartOp;
        while (ExtraOp && !ExtraOp->Common.Node)
        {
            ExtraOp = ExtraOp->Common.Parent;
        }

        if (!ExtraOp)
        {
            ParserState->StartNode = NULL;
        }
        else
        {
            ParserState->StartNode = ExtraOp->Common.Node;
        }

        if (ParserState->StartNode)
        {
            /* Push start scope on scope stack and make it current  */

            Status = AcpiDsScopeStackPush (ParserState->StartNode,
                            ParserState->StartNode->Type, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }
    }

    Status = AcpiDsInitCallbacks (WalkState, PassNumber);
    return_ACPI_STATUS (Status);
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


    ACPI_FUNCTION_TRACE_PTR ("DsDeleteWalkState", WalkState);


    if (!WalkState)
    {
        return;
    }

    if (WalkState->DataType != ACPI_DESC_TYPE_WALK)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%p is not a valid walk state\n", WalkState));
        return;
    }

    if (WalkState->ParserState.Scope)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%p walk still has a scope list\n", WalkState));
    }

    /* Always must free any linked control states */

    while (WalkState->ControlState)
    {
        State = WalkState->ControlState;
        WalkState->ControlState = State->Common.Next;

        AcpiUtDeleteGenericState (State);
    }

    /* Always must free any linked parse states */

    while (WalkState->ScopeInfo)
    {
        State = WalkState->ScopeInfo;
        WalkState->ScopeInfo = State->Common.Next;

        AcpiUtDeleteGenericState (State);
    }

    /* Always must free any stacked result states */

    while (WalkState->Results)
    {
        State = WalkState->Results;
        WalkState->Results = State->Common.Next;

        AcpiUtDeleteGenericState (State);
    }

    AcpiUtReleaseToCache (ACPI_MEM_LIST_WALK, WalkState);
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
    ACPI_FUNCTION_TRACE ("DsDeleteWalkStateCache");


    AcpiUtDeleteGenericCache (ACPI_MEM_LIST_WALK);
    return_VOID;
}


