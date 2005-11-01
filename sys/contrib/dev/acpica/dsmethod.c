/******************************************************************************
 *
 * Module Name: dsmethod - Parser/Interpreter interface - control method parsing
 *              $Revision: 1.110 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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

#define __DSMETHOD_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acinterp.h>
#include <contrib/dev/acpica/acnamesp.h>


#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dsmethod")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsParseMethod
 *
 * PARAMETERS:  Node        - Method node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse the AML that is associated with the method.
 *
 * MUTEX:       Assumes parser is locked
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsParseMethod (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARSE_OBJECT       *Op;
    ACPI_WALK_STATE         *WalkState;


    ACPI_FUNCTION_TRACE_PTR ("DsParseMethod", Node);


    /* Parameter Validation */

    if (!Node)
    {
        return_ACPI_STATUS (AE_NULL_ENTRY);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "**** Parsing [%4.4s] **** NamedObj=%p\n",
        AcpiUtGetNodeName (Node), Node));

    /* Extract the method object from the method Node */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NULL_OBJECT);
    }

    /* Create a mutex for the method if there is a concurrency limit */

    if ((ObjDesc->Method.Concurrency != ACPI_INFINITE_CONCURRENCY) &&
        (!ObjDesc->Method.Semaphore))
    {
        Status = AcpiOsCreateSemaphore (ObjDesc->Method.Concurrency,
                                        ObjDesc->Method.Concurrency,
                                        &ObjDesc->Method.Semaphore);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Allocate a new parser op to be the root of the parsed
     * method tree
     */
    Op = AcpiPsAllocOp (AML_METHOD_OP);
    if (!Op)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Init new op with the method name and pointer back to the Node */

    AcpiPsSetName (Op, Node->Name.Integer);
    Op->Common.Node = Node;

    /*
     * Get a new OwnerId for objects created by this method.  Namespace
     * objects (such as Operation Regions) can be created during the
     * first pass parse.
     */
    Status = AcpiUtAllocateOwnerId (&ObjDesc->Method.OwnerId);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (
                    ObjDesc->Method.OwnerId, NULL, NULL, NULL);
    if (!WalkState)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup2;
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, Node,
                    ObjDesc->Method.AmlStart,
                    ObjDesc->Method.AmlLength, NULL, 1);
    if (ACPI_FAILURE (Status))
    {
        AcpiDsDeleteWalkState (WalkState);
        goto Cleanup2;
    }

    /*
     * Parse the method, first pass
     *
     * The first pass load is where newly declared named objects are added into
     * the namespace.  Actual evaluation of the named objects (what would be
     * called a "second pass") happens during the actual execution of the
     * method so that operands to the named objects can take on dynamic
     * run-time values.
     */
    Status = AcpiPsParseAml (WalkState);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup2;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
        "**** [%4.4s] Parsed **** NamedObj=%p Op=%p\n",
        AcpiUtGetNodeName (Node), Node, Op));

    /*
     * Delete the parse tree. We simply re-parse the method for every
     * execution since there isn't much overhead (compared to keeping lots
     * of parse trees around)
     */
    AcpiNsDeleteNamespaceSubtree (Node);
    AcpiNsDeleteNamespaceByOwner (ObjDesc->Method.OwnerId);

Cleanup2:
    AcpiUtReleaseOwnerId (&ObjDesc->Method.OwnerId);

Cleanup:
    AcpiPsDeleteParseTree (Op);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsBeginMethodExecution
 *
 * PARAMETERS:  MethodNode          - Node of the method
 *              ObjDesc             - The method object
 *              CallingMethodNode   - Caller of this method (if non-null)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Prepare a method for execution.  Parses the method if necessary,
 *              increments the thread count, and waits at the method semaphore
 *              for clearance to execute.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsBeginMethodExecution (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_NAMESPACE_NODE     *CallingMethodNode)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_PTR ("DsBeginMethodExecution", MethodNode);


    if (!MethodNode)
    {
        return_ACPI_STATUS (AE_NULL_ENTRY);
    }

    /* Prevent wraparound of thread count */

    if (ObjDesc->Method.ThreadCount == ACPI_UINT8_MAX)
    {
        ACPI_REPORT_ERROR ((
            "Method reached maximum reentrancy limit (255)\n"));
        return_ACPI_STATUS (AE_AML_METHOD_LIMIT);
    }

    /*
     * If there is a concurrency limit on this method, we need to
     * obtain a unit from the method semaphore.
     */
    if (ObjDesc->Method.Semaphore)
    {
        /*
         * Allow recursive method calls, up to the reentrancy/concurrency
         * limit imposed by the SERIALIZED rule and the SyncLevel method
         * parameter.
         *
         * The point of this code is to avoid permanently blocking a
         * thread that is making recursive method calls.
         */
        if (MethodNode == CallingMethodNode)
        {
            if (ObjDesc->Method.ThreadCount >= ObjDesc->Method.Concurrency)
            {
                return_ACPI_STATUS (AE_AML_METHOD_LIMIT);
            }
        }

        /*
         * Get a unit from the method semaphore. This releases the
         * interpreter if we block
         */
        Status = AcpiExSystemWaitSemaphore (ObjDesc->Method.Semaphore,
                                            ACPI_WAIT_FOREVER);
    }

    /*
     * Allocate an Owner ID for this method, only if this is the first thread
     * to begin concurrent execution. We only need one OwnerId, even if the
     * method is invoked recursively.
     */
     if (!ObjDesc->Method.OwnerId)
     {
         Status = AcpiUtAllocateOwnerId (&ObjDesc->Method.OwnerId);
         if (ACPI_FAILURE (Status))
         {
             return_ACPI_STATUS (Status);
         }
     }

    /*
     * Increment the method parse tree thread count since it has been
     * reentered one more time (even if it is the same thread)
     */
    ObjDesc->Method.ThreadCount++;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsCallControlMethod
 *
 * PARAMETERS:  Thread              - Info for this thread
 *              ThisWalkState       - Current walk state
 *              Op                  - Current Op to be walked
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Transfer execution to a called control method
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsCallControlMethod (
    ACPI_THREAD_STATE       *Thread,
    ACPI_WALK_STATE         *ThisWalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *MethodNode;
    ACPI_WALK_STATE         *NextWalkState = NULL;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARAMETER_INFO     Info;
    UINT32                  i;


    ACPI_FUNCTION_TRACE_PTR ("DsCallControlMethod", ThisWalkState);

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Execute method %p, currentstate=%p\n",
        ThisWalkState->PrevOp, ThisWalkState));

    /*
     * Get the namespace entry for the control method we are about to call
     */
    MethodNode = ThisWalkState->MethodCallNode;
    if (!MethodNode)
    {
        return_ACPI_STATUS (AE_NULL_ENTRY);
    }

    ObjDesc = AcpiNsGetAttachedObject (MethodNode);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NULL_OBJECT);
    }

    /* Init for new method, wait on concurrency semaphore */

    Status = AcpiDsBeginMethodExecution (MethodNode, ObjDesc,
                    ThisWalkState->MethodNode);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    if (!(ObjDesc->Method.MethodFlags & AML_METHOD_INTERNAL_ONLY))
    {
        /* 1) Parse: Create a new walk state for the preempting walk */

        NextWalkState = AcpiDsCreateWalkState (ObjDesc->Method.OwnerId,
                                                Op, ObjDesc, NULL);
        if (!NextWalkState)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Create and init a Root Node */

        Op = AcpiPsCreateScopeOp ();
        if (!Op)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        Status = AcpiDsInitAmlWalk (NextWalkState, Op, MethodNode,
                        ObjDesc->Method.AmlStart,  ObjDesc->Method.AmlLength,
                        NULL, 1);
        if (ACPI_FAILURE (Status))
        {
            AcpiDsDeleteWalkState (NextWalkState);
            goto Cleanup;
        }

        /* Begin AML parse */

        Status = AcpiPsParseAml (NextWalkState);
        AcpiPsDeleteParseTree (Op);
    }

    /* 2) Execute: Create a new state for the preempting walk */

    NextWalkState = AcpiDsCreateWalkState (ObjDesc->Method.OwnerId,
                                            NULL, ObjDesc, Thread);
    if (!NextWalkState)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }
    /*
     * The resolved arguments were put on the previous walk state's operand
     * stack. Operands on the previous walk state stack always
     * start at index 0. Also, null terminate the list of arguments
     */
    ThisWalkState->Operands [ThisWalkState->NumOperands] = NULL;

    Info.Parameters = &ThisWalkState->Operands[0];
    Info.ParameterType = ACPI_PARAM_ARGS;

    Status = AcpiDsInitAmlWalk (NextWalkState, NULL, MethodNode,
                    ObjDesc->Method.AmlStart, ObjDesc->Method.AmlLength,
                    &Info, 3);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /*
     * Delete the operands on the previous walkstate operand stack
     * (they were copied to new objects)
     */
    for (i = 0; i < ObjDesc->Method.ParamCount; i++)
    {
        AcpiUtRemoveReference (ThisWalkState->Operands [i]);
        ThisWalkState->Operands [i] = NULL;
    }

    /* Clear the operand stack */

    ThisWalkState->NumOperands = 0;

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "Starting nested execution, newstate=%p\n", NextWalkState));

    if (ObjDesc->Method.MethodFlags & AML_METHOD_INTERNAL_ONLY)
    {
        Status = ObjDesc->Method.Implementation (NextWalkState);
    }

    return_ACPI_STATUS (Status);


Cleanup:
    /* Decrement the thread count on the method parse tree */

    if (NextWalkState && (NextWalkState->MethodDesc))
    {
        NextWalkState->MethodDesc->Method.ThreadCount--;
    }

    /* On error, we must delete the new walk state */

    AcpiDsTerminateControlMethod (NextWalkState);
    AcpiDsDeleteWalkState (NextWalkState);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsRestartControlMethod
 *
 * PARAMETERS:  WalkState           - State for preempted method (caller)
 *              ReturnDesc          - Return value from the called method
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Restart a method that was preempted by another (nested) method
 *              invocation.  Handle the return value (if any) from the callee.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("DsRestartControlMethod", WalkState);


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "****Restart [%4.4s] Op %p ReturnValueFromCallee %p\n",
        (char *) &WalkState->MethodNode->Name, WalkState->MethodCallOp,
        ReturnDesc));

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "    ReturnFromThisMethodUsed?=%X ResStack %p Walk %p\n",
        WalkState->ReturnUsed,
        WalkState->Results, WalkState));

    /* Did the called method return a value? */

    if (ReturnDesc)
    {
        /* Are we actually going to use the return value? */

        if (WalkState->ReturnUsed)
        {
            /* Save the return value from the previous method */

            Status = AcpiDsResultPush (ReturnDesc, WalkState);
            if (ACPI_FAILURE (Status))
            {
                AcpiUtRemoveReference (ReturnDesc);
                return_ACPI_STATUS (Status);
            }

            /*
             * Save as THIS method's return value in case it is returned
             * immediately to yet another method
             */
            WalkState->ReturnDesc = ReturnDesc;
        }

        /*
         * The following code is the
         * optional support for a so-called "implicit return". Some AML code
         * assumes that the last value of the method is "implicitly" returned
         * to the caller. Just save the last result as the return value.
         * NOTE: this is optional because the ASL language does not actually
         * support this behavior.
         */
        else if (!AcpiDsDoImplicitReturn (ReturnDesc, WalkState, FALSE))
        {
            /*
             * Delete the return value if it will not be used by the
             * calling method
             */
            AcpiUtRemoveReference (ReturnDesc);
        }
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsTerminateControlMethod
 *
 * PARAMETERS:  WalkState           - State of the method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Terminate a control method.  Delete everything that the method
 *              created, delete all locals and arguments, and delete the parse
 *              tree if requested.
 *
 ******************************************************************************/

void
AcpiDsTerminateControlMethod (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *MethodNode;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("DsTerminateControlMethod", WalkState);


    if (!WalkState)
    {
        return_VOID;
    }

    /* The current method object was saved in the walk state */

    ObjDesc = WalkState->MethodDesc;
    if (!ObjDesc)
    {
        return_VOID;
    }

    /* Delete all arguments and locals */

    AcpiDsMethodDataDeleteAll (WalkState);

    /*
     * Lock the parser while we terminate this method.
     * If this is the last thread executing the method,
     * we have additional cleanup to perform
     */
    Status = AcpiUtAcquireMutex (ACPI_MTX_PARSER);
    if (ACPI_FAILURE (Status))
    {
        return_VOID;
    }

    /* Signal completion of the execution of this method if necessary */

    if (WalkState->MethodDesc->Method.Semaphore)
    {
        Status = AcpiOsSignalSemaphore (
                        WalkState->MethodDesc->Method.Semaphore, 1);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("Could not signal method semaphore\n"));

            /* Ignore error and continue cleanup */
        }
    }

    if (WalkState->MethodDesc->Method.ThreadCount)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "*** Not deleting method namespace, there are still %d threads\n",
            WalkState->MethodDesc->Method.ThreadCount));
    }
    else /* This is the last executing thread */
    {
        /*
         * Support to dynamically change a method from NotSerialized to
         * Serialized if it appears that the method is written foolishly and
         * does not support multiple thread execution.  The best example of this
         * is if such a method creates namespace objects and blocks.  A second
         * thread will fail with an AE_ALREADY_EXISTS exception
         *
         * This code is here because we must wait until the last thread exits
         * before creating the synchronization semaphore.
         */
        if ((WalkState->MethodDesc->Method.Concurrency == 1) &&
            (!WalkState->MethodDesc->Method.Semaphore))
        {
            Status = AcpiOsCreateSemaphore (1, 1,
                        &WalkState->MethodDesc->Method.Semaphore);
        }

        /*
         * There are no more threads executing this method.  Perform
         * additional cleanup.
         *
         * The method Node is stored in the walk state
         */
        MethodNode = WalkState->MethodNode;

        /*
         * Delete any namespace entries created immediately underneath
         * the method
         */
        Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
        if (ACPI_FAILURE (Status))
        {
            goto Exit;
        }

        if (MethodNode->Child)
        {
            AcpiNsDeleteNamespaceSubtree (MethodNode);
        }

        /*
         * Delete any namespace entries created anywhere else within
         * the namespace
         */
        AcpiNsDeleteNamespaceByOwner (WalkState->MethodDesc->Method.OwnerId);
        Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        AcpiUtReleaseOwnerId (&WalkState->MethodDesc->Method.OwnerId);
    }

Exit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_PARSER);
    return_VOID;
}


