/******************************************************************************
 *
 * Module Name: pswalk - Parser routines to walk parsed op tree(s)
 *              $Revision: 47 $
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
#include "amlcode.h"
#include "acparser.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          PARSER
        MODULE_NAME         ("pswalk")


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextWalkOp
 *
 * PARAMETERS:  WalkState           - Current state of the walk
 *              Op                  - Current Op to be walked
 *              AscendingCallback   - Procedure called when Op is complete
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the next Op in a walk of the parse tree.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsGetNextWalkOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_UPWARDS      AscendingCallback)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_PARSE_OBJECT       *Parent;
    ACPI_PARSE_OBJECT       *GrandParent;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_PTR ("PsGetNextWalkOp", Op);


    /* Check for a argument only if we are descending in the tree */

    if (WalkState->NextOpInfo != NEXT_OP_UPWARD)
    {
        /* Look for an argument or child of the current op */

        Next = AcpiPsGetArg (Op, 0);
        if (Next)
        {
            /* Still going downward in tree (Op is not completed yet) */

            WalkState->PrevOp       = Op;
            WalkState->NextOp       = Next;
            WalkState->NextOpInfo   = NEXT_OP_DOWNWARD;

            return_ACPI_STATUS (AE_OK);
        }


        /*
         * No more children, this Op is complete.  Save Next and Parent
         * in case the Op object gets deleted by the callback routine
         */

        Next    = Op->Next;
        Parent  = Op->Parent;

        Status = AscendingCallback (WalkState, Op);

        switch (Status)
        {
        case AE_CTRL_TERMINATE:

            /*
             * A control method was terminated via a RETURN statement.
             * The walk of this method is complete.
             */
            WalkState->PrevOp       = WalkState->Origin;
            WalkState->NextOp       = NULL;

            return_ACPI_STATUS (AE_OK);
            break;


        case AE_CTRL_FALSE:

            /*
             * Either an IF/WHILE Predicate was false or we encountered a BREAK
             * opcode.  In both cases, we do not execute the rest of the
             * package;  We simply close out the parent (finishing the walk of
             * this branch of the tree) and continue execution at the parent
             * level.
             */

            Next        = Parent->Next;
            Status      = AE_OK;

            /*
             * If there is a sibling to the parent, we must close out the
             * parent now, because we are going to continue to go downward (to
             * the sibling) in the parse tree.
             */
            if (Next)
            {
                Status = AscendingCallback (WalkState, Parent);

                /* The parent sibling will be next */

                WalkState->PrevOp       = Op;
                WalkState->NextOp       = Next;
                WalkState->NextOpInfo   = NEXT_OP_DOWNWARD;

                /* Continue downward */

                return_ACPI_STATUS (AE_OK);
            }

            /*
             * Drop into the loop below because we are moving upwards in
             * the tree
             */

            break;


        default:
            /*
             * If we are back to the starting point, the walk is complete.
             */
            if (Op == WalkState->Origin)
            {
                /* Reached the point of origin, the walk is complete */

                WalkState->PrevOp       = Op;
                WalkState->NextOp       = NULL;

                return_ACPI_STATUS (Status);
            }

            /*
             * Check for a sibling to the current op.  A sibling means
             * we are still going "downward" in the tree.
             */

            if (Next)
            {
                /* There is a sibling, it will be next */

                WalkState->PrevOp       = Op;
                WalkState->NextOp       = Next;
                WalkState->NextOpInfo   = NEXT_OP_DOWNWARD;

                /* Continue downward */

                return_ACPI_STATUS (Status);
            }

            /*
             * No sibling, but check status.
             * Abort on error from callback routine
             */
            if (ACPI_FAILURE (Status))
            {
                /* Next op will be the parent */

                WalkState->PrevOp       = Op;
                WalkState->NextOp       = Parent;
                WalkState->NextOpInfo   = NEXT_OP_UPWARD;

                return_ACPI_STATUS (Status);
            }

            /*
             * Drop into the loop below because we are moving upwards in
             * the tree
             */

            break;
        }
    }

    else
    {
        /*
         * We are resuming a walk, and we were (are) going upward in the tree.
         * So, we want to drop into the parent loop below.
         */

        Parent = Op;
    }


    /*
     * Look for a sibling of the current Op's parent
     * Continue moving up the tree until we find a node that has not been
     * visited, or we get back to where we started.
     */
    while (Parent)
    {
        /* We are moving up the tree, therefore this parent Op is complete */

        GrandParent = Parent->Parent;
        Next        = Parent->Next;

        Status = AscendingCallback (WalkState, Parent);


        switch (Status)
        {
        case AE_CTRL_FALSE:

            /*
             * Either an IF/WHILE Predicate was false or we encountered a
             * BREAK opcode.  In both cases, we do not execute the rest of the
             * package;  We simply close out the parent (finishing the walk of
             * this branch of the tree) and continue execution at the parent
             * level.
             */

            Parent      = GrandParent;
            Next        = GrandParent->Next;
            GrandParent = GrandParent->Parent;

            Status = AscendingCallback (WalkState, Parent);

            /* Now continue to the next node in the tree */

            break;


        case AE_CTRL_TRUE:

            /*
             * Predicate of a WHILE was true and the loop just completed an
             * execution.  Go back to the start of the loop and reevaluate the
             * predicate.
             */

            Op = WalkState->ControlState->Control.PredicateOp;

            WalkState->ControlState->Common.State = CONTROL_PREDICATE_EXECUTING;

            /*
             * AcpiEvaluate the predicate again (next)
             * Because we will traverse WHILE tree again
             */

            WalkState->PrevOp       = Op->Parent;
            WalkState->NextOp       = Op;
            WalkState->NextOpInfo   = NEXT_OP_DOWNWARD;

            return_ACPI_STATUS (AE_OK);
            break;


        case AE_CTRL_TERMINATE:

            /*
             * A control method was terminated via a RETURN statement.
             * The walk of this method is complete.
             */
            WalkState->PrevOp       = WalkState->Origin;
            WalkState->NextOp       = NULL;

            return_ACPI_STATUS (AE_OK);
            break;
        }


        /*
         * If we are back to the starting point, the walk is complete.
         */
        if (Parent == WalkState->Origin)
        {
            /* Reached the point of origin, the walk is complete */

            WalkState->PrevOp       = Parent;
            WalkState->NextOp       = NULL;

            return_ACPI_STATUS (Status);
        }


        /*
         * If there is a sibling to this parent (it is not the starting point
         * Op), then we will visit it.
         */
        if (Next)
        {
            /* found sibling of parent */

            WalkState->PrevOp       = Parent;
            WalkState->NextOp       = Next;
            WalkState->NextOpInfo   = NEXT_OP_DOWNWARD;

            return_ACPI_STATUS (Status);
        }

        /*
         * No sibling, check for an error from closing the parent
         * (Also, AE_PENDING if a method call was encountered)
         */
        if (ACPI_FAILURE (Status))
        {
            WalkState->PrevOp       = Parent;
            WalkState->NextOp       = GrandParent;
            WalkState->NextOpInfo   = NEXT_OP_UPWARD;

            return_ACPI_STATUS (Status);
        }

        /* No siblings, no errors, just move up one more level in the tree */

        Op                  = Parent;
        Parent              = GrandParent;
        WalkState->PrevOp   = Op;
    }


    /* Got all the way to the top of the tree, we must be done! */
    /* However, the code should have terminated in the loop above */

    WalkState->NextOp       = NULL;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsWalkLoop
 *
 * PARAMETERS:  WalkList            - State of the walk
 *              StartOp             - Starting Op of the subtree to be walked
 *              DescendingCallback  - Procedure called when a new Op is
 *                                    encountered
 *              AscendingCallback   - Procedure called when Op is complete
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform a walk of the parsed AML tree.  Begins and terminates at
 *              the StartOp.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsWalkLoop (
    ACPI_WALK_LIST          *WalkList,
    ACPI_PARSE_OBJECT       *StartOp,
    ACPI_PARSE_DOWNWARDS    DescendingCallback,
    ACPI_PARSE_UPWARDS      AscendingCallback)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_WALK_STATE         *WalkState;
    ACPI_PARSE_OBJECT       *Op = StartOp;


    FUNCTION_TRACE_PTR ("PsWalkLoop", StartOp);


    WalkState = AcpiDsGetCurrentWalkState (WalkList);


    /* Walk entire subtree, visiting all nodes depth-first */

    while (Op)
    {
        if (WalkState->NextOpInfo != NEXT_OP_UPWARD)
        {
            Status = DescendingCallback (Op->Opcode, Op, WalkState, NULL);
        }

        /*
         * A TRUE exception means that an ELSE was detected, but the IF
         * predicate evaluated TRUE.
         */
        if (Status == AE_CTRL_TRUE)
        {
            /*
             * Ignore the entire ELSE block by moving on to the the next opcode.
             * And we do that by simply going up in the tree (either to the next
             * sibling or to the parent) from here.
             */

            WalkState->NextOpInfo = NEXT_OP_UPWARD;
        }

        /* Get the next node (op) in the depth-first walk */

        Status = AcpiPsGetNextWalkOp (WalkState, Op, AscendingCallback);

        /*
         * A PENDING exception means that a control method invocation has been
         * detected
         */

        if (Status == AE_CTRL_PENDING)
        {
            /* Transfer control to the called control method */

            Status = AcpiDsCallControlMethod (WalkList, WalkState, Op);

            /*
             * If the transfer to the new method method call worked, a new walk
             * state was created -- get it
             */

            WalkState = AcpiDsGetCurrentWalkState (WalkList);
        }

        /* Abort the walk on any exception */

        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        Op = WalkState->NextOp;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsWalkParsedAml
 *
 * PARAMETERS:  StartOp             - Starting Op of the subtree to be walked
 *              EndOp               - Where to terminate the walk
 *              DescendingCallback  - Procedure called when a new Op is
 *                                    encountered
 *              AscendingCallback   - Procedure called when Op is complete
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Top level interface to walk the parsed AML tree.  Handles
 *              preemption of executing control methods.
 *
 *              NOTE: The EndOp is usually only different from the StartOp if
 *              we don't want to visit the StartOp during the tree descent.
 *
 ******************************************************************************/

ACPI_STATUS
mmmmAcpiPsWalkParsedAml (
    ACPI_PARSE_OBJECT       *StartOp,
    ACPI_PARSE_OBJECT       *EndOp,
    ACPI_OPERAND_OBJECT     *MthDesc,
    ACPI_NAMESPACE_NODE     *StartNode,
    ACPI_OPERAND_OBJECT     **Params,
    ACPI_OPERAND_OBJECT     **CallerReturnDesc,
    ACPI_OWNER_ID           OwnerId,
    ACPI_PARSE_DOWNWARDS    DescendingCallback,
    ACPI_PARSE_UPWARDS      AscendingCallback)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    ACPI_STATUS             Status;
    ACPI_WALK_LIST          WalkList;
    ACPI_WALK_LIST          *PrevWalkList;


    FUNCTION_TRACE_PTR ("PsWalkParsedAml", StartOp);


    /* Parameter Validation */

    if (!StartOp || !EndOp)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Initialize a new walk list */

    WalkList.WalkState = NULL;

    WalkState = AcpiDsCreateWalkState (OwnerId, EndOp, MthDesc, &WalkList);
    if (!WalkState)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* TBD: [Restructure] TEMP until we pass WalkState to the interpreter
     */
    PrevWalkList = AcpiGbl_CurrentWalkList;
    AcpiGbl_CurrentWalkList = &WalkList;

    if (StartNode)
    {
        /* Push start scope on scope stack and make it current  */

        Status = AcpiDsScopeStackPush (StartNode, ACPI_TYPE_METHOD, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

    }

    if (MthDesc)
    {
        /* Init arguments if this is a control method */
        /* TBD: [Restructure] add walkstate as a param */

        AcpiDsMethodDataInitArgs (Params, MTH_NUM_ARGS, WalkState);
    }

    Op = StartOp;
    Status = AE_OK;


    /*
     * Execute the walk loop as long as there is a valid Walk State.  This
     * handles nested control method invocations without recursion.
     */

    DEBUG_PRINT (TRACE_PARSE, ("PsWalkParsedAml: Op=%p EndOp=%p State=%p\n",
                    Op, EndOp, WalkState));

    while (WalkState)
    {
        if (ACPI_SUCCESS (Status))
        {
            Status = AcpiPsWalkLoop (&WalkList, Op, DescendingCallback,
                                    AscendingCallback);
        }

        DEBUG_PRINT (TRACE_PARSE,
            ("PsWalkParsedAml: Completed one call to walk loop, State=%p\n",
            WalkState));

        /* We are done with this walk, move on to the parent if any */

        BREAKPOINT3;

        WalkState = AcpiDsPopWalkState (&WalkList);

        /* Extract return value before we delete WalkState */

        ReturnDesc = WalkState->ReturnDesc;

        DEBUG_PRINT (TRACE_PARSE,
            ("PsWalkParsedAml: ReturnValue=%p, State=%p\n",
            WalkState->ReturnDesc, WalkState));

        /* Reset the current scope to the beginning of scope stack */

        AcpiDsScopeStackClear (WalkState);

        /*
         * If we just returned from the execution of a control method,
         * there's lots of cleanup to do
         */

        if (WalkState->MethodDesc)
        {
            AcpiDsTerminateControlMethod (WalkState);
        }

         /* Delete this walk state and all linked control states */

        AcpiDsDeleteWalkState (WalkState);

       /* Check if we have restarted a preempted walk */

        WalkState = AcpiDsGetCurrentWalkState (&WalkList);
        if (WalkState &&
            ACPI_SUCCESS (Status))
        {
            /* There is another walk state, restart it */

            /*
             * If the method returned value is not used by the parent,
             * The object is deleted
             */

            AcpiDsRestartControlMethod (WalkState, ReturnDesc);

            /* Get the next Op to process */

            Op = WalkState->NextOp;
        }

        /*
         * Just completed a 1st-level method, save the final internal return
         * value (if any)
         */

        else if (CallerReturnDesc)
        {
            *CallerReturnDesc = ReturnDesc; /* NULL if no return value */
        }

        else if (ReturnDesc)
        {
            /* Caller doesn't want it, must delete it */

            AcpiCmRemoveReference (ReturnDesc);
        }
    }


    AcpiGbl_CurrentWalkList = PrevWalkList;

    return_ACPI_STATUS (Status);
}


