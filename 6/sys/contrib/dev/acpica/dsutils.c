/*******************************************************************************
 *
 * Module Name: dsutils - Dispatcher utilities
 *              $Revision: 107 $
 *
 ******************************************************************************/

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

#define __DSUTILS_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acinterp.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acdebug.h>

#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dsutils")


#ifndef ACPI_NO_METHOD_EXECUTION

/*******************************************************************************
 *
 * FUNCTION:    AcpiDsIsResultUsed
 *
 * PARAMETERS:  Op                  - Current Op
 *              WalkState           - Current State
 *
 * RETURN:      TRUE if result is used, FALSE otherwise
 *
 * DESCRIPTION: Check if a result object will be used by the parent
 *
 ******************************************************************************/

BOOLEAN
AcpiDsIsResultUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    const ACPI_OPCODE_INFO  *ParentInfo;


    ACPI_FUNCTION_TRACE_PTR ("DsIsResultUsed", Op);


    /* Must have both an Op and a Result Object */

    if (!Op)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Null Op\n"));
        return_VALUE (TRUE);
    }

    /*
     * If there is no parent, we are executing at the method level.
     * An executing method typically has no parent, since each method
     * is parsed separately.
     */
    if (!Op->Common.Parent ||
        Op->Common.Parent->Common.AmlOpcode == AML_SCOPE_OP)
    {
        /*
         * If this is the last statement in the method, we know it is not a
         * Return() operator (would not come here.) The following code is the
         * optional support for a so-called "implicit return". Some AML code
         * assumes that the last value of the method is "implicitly" returned
         * to the caller. Just save the last result as the return value.
         * NOTE: this is optional because the ASL language does not actually
         * support this behavior.
         */
        if ((AcpiGbl_EnableInterpreterSlack) &&
            (WalkState->ParserState.Aml >= WalkState->ParserState.AmlEnd))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
                    "Result of [%s] will be implicitly returned\n",
                    AcpiPsGetOpcodeName (Op->Common.AmlOpcode)));

            /* Use the top of the result stack as the implicit return value */

            WalkState->ReturnDesc = WalkState->Results->Results.ObjDesc[0];
            return_VALUE (TRUE);
        }

        /* No parent, the return value cannot possibly be used */

        return_VALUE (FALSE);
    }

    /* Get info on the parent. The RootOp is AML_SCOPE */

    ParentInfo = AcpiPsGetOpcodeInfo (Op->Common.Parent->Common.AmlOpcode);
    if (ParentInfo->Class == AML_CLASS_UNKNOWN)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown parent opcode. Op=%p\n", Op));
        return_VALUE (FALSE);
    }

    /*
     * Decide what to do with the result based on the parent.  If
     * the parent opcode will not use the result, delete the object.
     * Otherwise leave it as is, it will be deleted when it is used
     * as an operand later.
     */
    switch (ParentInfo->Class)
    {
    case AML_CLASS_CONTROL:

        switch (Op->Common.Parent->Common.AmlOpcode)
        {
        case AML_RETURN_OP:

            /* Never delete the return value associated with a return opcode */

            goto ResultUsed;

        case AML_IF_OP:
        case AML_WHILE_OP:

            /*
             * If we are executing the predicate AND this is the predicate op,
             * we will use the return value
             */
            if ((WalkState->ControlState->Common.State == ACPI_CONTROL_PREDICATE_EXECUTING) &&
                (WalkState->ControlState->Control.PredicateOp == Op))
            {
                goto ResultUsed;
            }
            break;

        default:
            /* Ignore other control opcodes */
            break;
        }

        /* The general control opcode returns no result */

        goto ResultNotUsed;


    case AML_CLASS_CREATE:

        /*
         * These opcodes allow TermArg(s) as operands and therefore
         * the operands can be method calls.  The result is used.
         */
        goto ResultUsed;


    case AML_CLASS_NAMED_OBJECT:

        if ((Op->Common.Parent->Common.AmlOpcode == AML_REGION_OP)       ||
            (Op->Common.Parent->Common.AmlOpcode == AML_DATA_REGION_OP)  ||
            (Op->Common.Parent->Common.AmlOpcode == AML_PACKAGE_OP)      ||
            (Op->Common.Parent->Common.AmlOpcode == AML_VAR_PACKAGE_OP)  ||
            (Op->Common.Parent->Common.AmlOpcode == AML_BUFFER_OP)       ||
            (Op->Common.Parent->Common.AmlOpcode == AML_INT_EVAL_SUBTREE_OP))
        {
            /*
             * These opcodes allow TermArg(s) as operands and therefore
             * the operands can be method calls.  The result is used.
             */
            goto ResultUsed;
        }

        goto ResultNotUsed;


    default:

        /*
         * In all other cases. the parent will actually use the return
         * object, so keep it.
         */
        goto ResultUsed;
    }


ResultUsed:
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Result of [%s] used by Parent [%s] Op=%p\n",
            AcpiPsGetOpcodeName (Op->Common.AmlOpcode),
            AcpiPsGetOpcodeName (Op->Common.Parent->Common.AmlOpcode), Op));

    return_VALUE (TRUE);


ResultNotUsed:
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Result of [%s] not used by Parent [%s] Op=%p\n",
            AcpiPsGetOpcodeName (Op->Common.AmlOpcode),
            AcpiPsGetOpcodeName (Op->Common.Parent->Common.AmlOpcode), Op));

    return_VALUE (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsDeleteResultIfNotUsed
 *
 * PARAMETERS:  Op              - Current parse Op
 *              ResultObj       - Result of the operation
 *              WalkState       - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Used after interpretation of an opcode.  If there is an internal
 *              result descriptor, check if the parent opcode will actually use
 *              this result.  If not, delete the result now so that it will
 *              not become orphaned.
 *
 ******************************************************************************/

void
AcpiDsDeleteResultIfNotUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     *ResultObj,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("DsDeleteResultIfNotUsed", ResultObj);


    if (!Op)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Null Op\n"));
        return_VOID;
    }

    if (!ResultObj)
    {
        return_VOID;
    }

    if (!AcpiDsIsResultUsed (Op, WalkState))
    {
        /*
         * Must pop the result stack (ObjDesc should be equal to ResultObj)
         */
        Status = AcpiDsResultPop (&ObjDesc, WalkState);
        if (ACPI_SUCCESS (Status))
        {
            AcpiUtRemoveReference (ResultObj);
        }
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsResolveOperands
 *
 * PARAMETERS:  WalkState           - Current walk state with operands on stack
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Resolve all operands to their values.  Used to prepare
 *              arguments to a control method invocation (a call from one
 *              method to another.)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsResolveOperands (
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_PTR ("DsResolveOperands", WalkState);


    /*
     * Attempt to resolve each of the valid operands
     * Method arguments are passed by reference, not by value.  This means
     * that the actual objects are passed, not copies of the objects.
     */
    for (i = 0; i < WalkState->NumOperands; i++)
    {
        Status = AcpiExResolveToValue (&WalkState->Operands[i], WalkState);
        if (ACPI_FAILURE (Status))
        {
            break;
        }
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsClearOperands
 *
 * PARAMETERS:  WalkState           - Current walk state with operands on stack
 *
 * RETURN:      None
 *
 * DESCRIPTION: Clear all operands on the current walk state operand stack.
 *
 ******************************************************************************/

void
AcpiDsClearOperands (
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;


    ACPI_FUNCTION_TRACE_PTR ("DsClearOperands", WalkState);


    /*
     * Remove a reference on each operand on the stack
     */
    for (i = 0; i < WalkState->NumOperands; i++)
    {
        /*
         * Remove a reference to all operands, including both
         * "Arguments" and "Targets".
         */
        AcpiUtRemoveReference (WalkState->Operands[i]);
        WalkState->Operands[i] = NULL;
    }

    WalkState->NumOperands = 0;
    return_VOID;
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsCreateOperand
 *
 * PARAMETERS:  WalkState       - Current walk state
 *              Arg             - Parse object for the argument
 *              ArgIndex        - Which argument (zero based)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a parse tree object that is an argument to an AML
 *              opcode to the equivalent interpreter object.  This may include
 *              looking up a name or entering a new name into the internal
 *              namespace.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsCreateOperand (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Arg,
    UINT32                  ArgIndex)
{
    ACPI_STATUS             Status = AE_OK;
    char                    *NameString;
    UINT32                  NameLength;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARSE_OBJECT       *ParentOp;
    UINT16                  Opcode;
    ACPI_INTERPRETER_MODE   InterpreterMode;
    const ACPI_OPCODE_INFO  *OpInfo;


    ACPI_FUNCTION_TRACE_PTR ("DsCreateOperand", Arg);


    /* A valid name must be looked up in the namespace */

    if ((Arg->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
        (Arg->Common.Value.String))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Getting a name: Arg=%p\n", Arg));

        /* Get the entire name string from the AML stream */

        Status = AcpiExGetNameString (ACPI_TYPE_ANY, Arg->Common.Value.Buffer,
                        &NameString, &NameLength);

        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /*
         * All prefixes have been handled, and the name is
         * in NameString
         */


        /*
         * Special handling for BufferField declarations.  This is a deferred
         * opcode that unfortunately defines the field name as the last
         * parameter instead of the first.  We get here when we are performing
         * the deferred execution, so the actual name of the field is already
         * in the namespace.  We don't want to attempt to look it up again
         * because we may be executing in a different scope than where the
         * actual opcode exists.
         */
        if ((WalkState->DeferredNode) &&
            (WalkState->DeferredNode->Type == ACPI_TYPE_BUFFER_FIELD) &&
            (ArgIndex != 0))
        {
            ObjDesc = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, WalkState->DeferredNode);
            Status = AE_OK;
        }
        else    /* All other opcodes */
        {
            /*
             * Differentiate between a namespace "create" operation
             * versus a "lookup" operation (IMODE_LOAD_PASS2 vs.
             * IMODE_EXECUTE) in order to support the creation of
             * namespace objects during the execution of control methods.
             */
            ParentOp = Arg->Common.Parent;
            OpInfo = AcpiPsGetOpcodeInfo (ParentOp->Common.AmlOpcode);
            if ((OpInfo->Flags & AML_NSNODE) &&
                (ParentOp->Common.AmlOpcode != AML_INT_METHODCALL_OP) &&
                (ParentOp->Common.AmlOpcode != AML_REGION_OP) &&
                (ParentOp->Common.AmlOpcode != AML_INT_NAMEPATH_OP))
            {
                /* Enter name into namespace if not found */

                InterpreterMode = ACPI_IMODE_LOAD_PASS2;
            }
            else
            {
                /* Return a failure if name not found */

                InterpreterMode = ACPI_IMODE_EXECUTE;
            }

            Status = AcpiNsLookup (WalkState->ScopeInfo, NameString,
                                    ACPI_TYPE_ANY, InterpreterMode,
                                    ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
                                    WalkState,
                                    ACPI_CAST_INDIRECT_PTR (ACPI_NAMESPACE_NODE, &ObjDesc));
            /*
             * The only case where we pass through (ignore) a NOT_FOUND
             * error is for the CondRefOf opcode.
             */
            if (Status == AE_NOT_FOUND)
            {
                if (ParentOp->Common.AmlOpcode == AML_COND_REF_OF_OP)
                {
                    /*
                     * For the Conditional Reference op, it's OK if
                     * the name is not found;  We just need a way to
                     * indicate this to the interpreter, set the
                     * object to the root
                     */
                    ObjDesc = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, AcpiGbl_RootNode);
                    Status = AE_OK;
                }
                else
                {
                    /*
                     * We just plain didn't find it -- which is a
                     * very serious error at this point
                     */
                    Status = AE_AML_NAME_NOT_FOUND;
                }
            }

            if (ACPI_FAILURE (Status))
            {
                ACPI_REPORT_NSERROR (NameString, Status);
            }
        }

        /* Free the namestring created above */

        ACPI_MEM_FREE (NameString);

        /* Check status from the lookup */

        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Put the resulting object onto the current object stack */

        Status = AcpiDsObjStackPush (ObjDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
        ACPI_DEBUGGER_EXEC (AcpiDbDisplayArgumentObject (ObjDesc, WalkState));
    }
    else
    {
        /* Check for null name case */

        if (Arg->Common.AmlOpcode == AML_INT_NAMEPATH_OP)
        {
            /*
             * If the name is null, this means that this is an
             * optional result parameter that was not specified
             * in the original ASL.  Create a Zero Constant for a
             * placeholder.  (Store to a constant is a Noop.)
             */
            Opcode = AML_ZERO_OP;       /* Has no arguments! */

            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Null namepath: Arg=%p\n", Arg));
        }
        else
        {
            Opcode = Arg->Common.AmlOpcode;
        }

        /* Get the object type of the argument */

        OpInfo = AcpiPsGetOpcodeInfo (Opcode);
        if (OpInfo->ObjectType == ACPI_TYPE_INVALID)
        {
            return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
        }

        if (OpInfo->Flags & AML_HAS_RETVAL)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
                "Argument previously created, already stacked \n"));

            ACPI_DEBUGGER_EXEC (AcpiDbDisplayArgumentObject (
                WalkState->Operands [WalkState->NumOperands - 1], WalkState));

            /*
             * Use value that was already previously returned
             * by the evaluation of this argument
             */
            Status = AcpiDsResultPopFromBottom (&ObjDesc, WalkState);
            if (ACPI_FAILURE (Status))
            {
                /*
                 * Only error is underflow, and this indicates
                 * a missing or null operand!
                 */
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Missing or null operand, %s\n",
                    AcpiFormatException (Status)));
                return_ACPI_STATUS (Status);
            }
        }
        else
        {
            /* Create an ACPI_INTERNAL_OBJECT for the argument */

            ObjDesc = AcpiUtCreateInternalObject (OpInfo->ObjectType);
            if (!ObjDesc)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            /* Initialize the new object */

            Status = AcpiDsInitObjectFromOp (WalkState, Arg,
                                                Opcode, &ObjDesc);
            if (ACPI_FAILURE (Status))
            {
                AcpiUtDeleteObjectDesc (ObjDesc);
                return_ACPI_STATUS (Status);
            }
        }

        /* Put the operand object on the object stack */

        Status = AcpiDsObjStackPush (ObjDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        ACPI_DEBUGGER_EXEC (AcpiDbDisplayArgumentObject (ObjDesc, WalkState));
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsCreateOperands
 *
 * PARAMETERS:  FirstArg            - First argument of a parser argument tree
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an operator's arguments from a parse tree format to
 *              namespace objects and place those argument object on the object
 *              stack in preparation for evaluation by the interpreter.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsCreateOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *FirstArg)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PARSE_OBJECT       *Arg;
    UINT32                  ArgCount = 0;


    ACPI_FUNCTION_TRACE_PTR ("DsCreateOperands", FirstArg);


    /* For all arguments in the list... */

    Arg = FirstArg;
    while (Arg)
    {
        Status = AcpiDsCreateOperand (WalkState, Arg, ArgCount);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Arg #%d (%p) done, Arg1=%p\n",
            ArgCount, Arg, FirstArg));

        /* Move on to next argument, if any */

        Arg = Arg->Common.Next;
        ArgCount++;
    }

    return_ACPI_STATUS (Status);


Cleanup:
    /*
     * We must undo everything done above; meaning that we must
     * pop everything off of the operand stack and delete those
     * objects
     */
    (void) AcpiDsObjStackPopAndDelete (ArgCount, WalkState);

    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "While creating Arg %d - %s\n",
        (ArgCount + 1), AcpiFormatException (Status)));
    return_ACPI_STATUS (Status);
}


