/******************************************************************************
 *
 * Module Name: pswalk - Parser routines to walk parsed op tree(s)
 *              $Revision: 69 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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
#include "acparser.h"
#include "acdispat.h"

#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("pswalk")


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


    ACPI_FUNCTION_TRACE_PTR ("PsGetNextWalkOp", Op);


    /* Check for a argument only if we are descending in the tree */

    if (WalkState->NextOpInfo != ACPI_NEXT_OP_UPWARD)
    {
        /* Look for an argument or child of the current op */

        Next = AcpiPsGetArg (Op, 0);
        if (Next)
        {
            /* Still going downward in tree (Op is not completed yet) */

            WalkState->PrevOp       = Op;
            WalkState->NextOp       = Next;
            WalkState->NextOpInfo   = ACPI_NEXT_OP_DOWNWARD;

            return_ACPI_STATUS (AE_OK);
        }

        /*
         * No more children, this Op is complete.  Save Next and Parent
         * in case the Op object gets deleted by the callback routine
         */
        Next    = Op->Common.Next;
        Parent  = Op->Common.Parent;

        WalkState->Op     = Op;
        WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        WalkState->Opcode = Op->Common.AmlOpcode;

        Status = AscendingCallback (WalkState);

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
            WalkState->NextOpInfo   = ACPI_NEXT_OP_DOWNWARD;

            /* Continue downward */

            return_ACPI_STATUS (Status);
        }

        /*
         * Drop into the loop below because we are moving upwards in
         * the tree
         */
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

        GrandParent = Parent->Common.Parent;
        Next        = Parent->Common.Next;

        WalkState->Op     = Parent;
        WalkState->OpInfo = AcpiPsGetOpcodeInfo (Parent->Common.AmlOpcode);
        WalkState->Opcode = Parent->Common.AmlOpcode;

        Status = AscendingCallback (WalkState);

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
            WalkState->NextOpInfo   = ACPI_NEXT_OP_DOWNWARD;

            return_ACPI_STATUS (Status);
        }

        /* No siblings, no errors, just move up one more level in the tree */

        Op                  = Parent;
        Parent              = GrandParent;
        WalkState->PrevOp   = Op;
    }


    /*
     * Got all the way to the top of the tree, we must be done!
     * However, the code should have terminated in the loop above
     */
    WalkState->NextOp       = NULL;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsDeleteCompletedOp
 *
 * PARAMETERS:  State           - Walk state
 *              Op              - Completed op
 *
 * RETURN:      AE_OK
 *
 * DESCRIPTION: Callback function for AcpiPsGetNextWalkOp().  Used during
 *              AcpiPsDeleteParse tree to delete Op objects when all sub-objects
 *              have been visited (and deleted.)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsDeleteCompletedOp (
    ACPI_WALK_STATE         *WalkState)
{

    AcpiPsFreeOp (WalkState->Op);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsDeleteParseTree
 *
 * PARAMETERS:  SubtreeRoot         - Root of tree (or subtree) to delete
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a portion of or an entire parse tree.
 *
 ******************************************************************************/

void
AcpiPsDeleteParseTree (
    ACPI_PARSE_OBJECT       *SubtreeRoot)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_THREAD_STATE       *Thread;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("PsDeleteParseTree", SubtreeRoot);


    if (!SubtreeRoot)
    {
        return_VOID;
    }

    /* Create and initialize a new walk list */

    Thread = AcpiUtCreateThreadState ();
    if (!Thread)
    {
        return_VOID;
    }

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, Thread);
    if (!WalkState)
    {
        return_VOID;
    }

    WalkState->ParseFlags           = 0;
    WalkState->DescendingCallback   = NULL;
    WalkState->AscendingCallback    = NULL;

    WalkState->Origin = SubtreeRoot;
    WalkState->NextOp = SubtreeRoot;

    /* Head downward in the tree */

    WalkState->NextOpInfo = ACPI_NEXT_OP_DOWNWARD;

    /* Visit all nodes in the subtree */

    while (WalkState->NextOp)
    {
        Status = AcpiPsGetNextWalkOp (WalkState, WalkState->NextOp,
                                AcpiPsDeleteCompletedOp);
        if (ACPI_FAILURE (Status))
        {
            break;
        }
    }

    /* We are done with this walk */

    AcpiUtDeleteGenericState (ACPI_CAST_PTR (ACPI_GENERIC_STATE, Thread));
    AcpiDsDeleteWalkState (WalkState);

    return_VOID;
}


