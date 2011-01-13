/******************************************************************************
 *
 * Module Name: dswload - Dispatcher namespace load callbacks
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

#define __DSWLOAD_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acevents.h>

#ifdef ACPI_ASL_COMPILER
#include <contrib/dev/acpica/include/acdisasm.h>
#endif

#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dswload")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsInitCallbacks
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              PassNumber      - 1, 2, or 3
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Init walk state callbacks
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsInitCallbacks (
    ACPI_WALK_STATE         *WalkState,
    UINT32                  PassNumber)
{

    switch (PassNumber)
    {
    case 1:
        WalkState->ParseFlags         = ACPI_PARSE_LOAD_PASS1 |
                                        ACPI_PARSE_DELETE_TREE;
        WalkState->DescendingCallback = AcpiDsLoad1BeginOp;
        WalkState->AscendingCallback  = AcpiDsLoad1EndOp;
        break;

    case 2:
        WalkState->ParseFlags         = ACPI_PARSE_LOAD_PASS1 |
                                        ACPI_PARSE_DELETE_TREE;
        WalkState->DescendingCallback = AcpiDsLoad2BeginOp;
        WalkState->AscendingCallback  = AcpiDsLoad2EndOp;
        break;

    case 3:
#ifndef ACPI_NO_METHOD_EXECUTION
        WalkState->ParseFlags        |= ACPI_PARSE_EXECUTE  |
                                        ACPI_PARSE_DELETE_TREE;
        WalkState->DescendingCallback = AcpiDsExecBeginOp;
        WalkState->AscendingCallback  = AcpiDsExecEndOp;
#endif
        break;

    default:
        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsLoad1BeginOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              OutOp           - Where to return op if a new one is created
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the loading of ACPI tables.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsLoad1BeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    ACPI_OBJECT_TYPE        ObjectType;
    char                    *Path;
    UINT32                  Flags;


    ACPI_FUNCTION_TRACE (DsLoad1BeginOp);


    Op = WalkState->Op;
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Op=%p State=%p\n", Op, WalkState));

    /* We are only interested in opcodes that have an associated name */

    if (Op)
    {
        if (!(WalkState->OpInfo->Flags & AML_NAMED))
        {
            *OutOp = Op;
            return_ACPI_STATUS (AE_OK);
        }

        /* Check if this object has already been installed in the namespace */

        if (Op->Common.Node)
        {
            *OutOp = Op;
            return_ACPI_STATUS (AE_OK);
        }
    }

    Path = AcpiPsGetNextNamestring (&WalkState->ParserState);

    /* Map the raw opcode into an internal object type */

    ObjectType = WalkState->OpInfo->ObjectType;

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "State=%p Op=%p [%s]\n", WalkState, Op, AcpiUtGetTypeName (ObjectType)));

    switch (WalkState->Opcode)
    {
    case AML_SCOPE_OP:

        /*
         * The target name of the Scope() operator must exist at this point so
         * that we can actually open the scope to enter new names underneath it.
         * Allow search-to-root for single namesegs.
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
                        ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT, WalkState, &(Node));
#ifdef ACPI_ASL_COMPILER
        if (Status == AE_NOT_FOUND)
        {
            /*
             * Table disassembly:
             * Target of Scope() not found. Generate an External for it, and
             * insert the name into the namespace.
             */
            AcpiDmAddToExternalList (Op, Path, ACPI_TYPE_DEVICE, 0);
            Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
                       ACPI_IMODE_LOAD_PASS1, ACPI_NS_SEARCH_PARENT,
                       WalkState, &Node);
        }
#endif
        if (ACPI_FAILURE (Status))
        {
            ACPI_ERROR_NAMESPACE (Path, Status);
            return_ACPI_STATUS (Status);
        }

        /*
         * Check to make sure that the target is
         * one of the opcodes that actually opens a scope
         */
        switch (Node->Type)
        {
        case ACPI_TYPE_ANY:
        case ACPI_TYPE_LOCAL_SCOPE:         /* Scope  */
        case ACPI_TYPE_DEVICE:
        case ACPI_TYPE_POWER:
        case ACPI_TYPE_PROCESSOR:
        case ACPI_TYPE_THERMAL:

            /* These are acceptable types */
            break;

        case ACPI_TYPE_INTEGER:
        case ACPI_TYPE_STRING:
        case ACPI_TYPE_BUFFER:

            /*
             * These types we will allow, but we will change the type.
             * This enables some existing code of the form:
             *
             *  Name (DEB, 0)
             *  Scope (DEB) { ... }
             *
             * Note: silently change the type here. On the second pass,
             * we will report a warning
             */
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "Type override - [%4.4s] had invalid type (%s) "
                "for Scope operator, changed to type ANY\n",
                AcpiUtGetNodeName (Node), AcpiUtGetTypeName (Node->Type)));

            Node->Type = ACPI_TYPE_ANY;
            WalkState->ScopeInfo->Common.Value = ACPI_TYPE_ANY;
            break;

        default:

            /* All other types are an error */

            ACPI_ERROR ((AE_INFO,
                "Invalid type (%s) for target of "
                "Scope operator [%4.4s] (Cannot override)",
                AcpiUtGetTypeName (Node->Type), AcpiUtGetNodeName (Node)));

            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }
        break;


    default:
        /*
         * For all other named opcodes, we will enter the name into
         * the namespace.
         *
         * Setup the search flags.
         * Since we are entering a name into the namespace, we do not want to
         * enable the search-to-root upsearch.
         *
         * There are only two conditions where it is acceptable that the name
         * already exists:
         *    1) the Scope() operator can reopen a scoping object that was
         *       previously defined (Scope, Method, Device, etc.)
         *    2) Whenever we are parsing a deferred opcode (OpRegion, Buffer,
         *       BufferField, or Package), the name of the object is already
         *       in the namespace.
         */
        if (WalkState->DeferredNode)
        {
            /* This name is already in the namespace, get the node */

            Node = WalkState->DeferredNode;
            Status = AE_OK;
            break;
        }

        /*
         * If we are executing a method, do not create any namespace objects
         * during the load phase, only during execution.
         */
        if (WalkState->MethodNode)
        {
            Node = NULL;
            Status = AE_OK;
            break;
        }

        Flags = ACPI_NS_NO_UPSEARCH;
        if ((WalkState->Opcode != AML_SCOPE_OP) &&
            (!(WalkState->ParseFlags & ACPI_PARSE_DEFERRED_OP)))
        {
            Flags |= ACPI_NS_ERROR_IF_FOUND;
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "[%s] Cannot already exist\n",
                    AcpiUtGetTypeName (ObjectType)));
        }
        else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
                "[%s] Both Find or Create allowed\n",
                    AcpiUtGetTypeName (ObjectType)));
        }

        /*
         * Enter the named type into the internal namespace. We enter the name
         * as we go downward in the parse tree. Any necessary subobjects that
         * involve arguments to the opcode must be created as we go back up the
         * parse tree later.
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
                        ACPI_IMODE_LOAD_PASS1, Flags, WalkState, &Node);
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_ALREADY_EXISTS)
            {
                /* The name already exists in this scope */

                if (Node->Flags & ANOBJ_IS_EXTERNAL)
                {
                    /*
                     * Allow one create on an object or segment that was
                     * previously declared External
                     */
                    Node->Flags &= ~ANOBJ_IS_EXTERNAL;
                    Node->Type = (UINT8) ObjectType;

                    /* Just retyped a node, probably will need to open a scope */

                    if (AcpiNsOpensScope (ObjectType))
                    {
                        Status = AcpiDsScopeStackPush (Node, ObjectType, WalkState);
                        if (ACPI_FAILURE (Status))
                        {
                            return_ACPI_STATUS (Status);
                        }
                    }

                    Status = AE_OK;
                }
            }

            if (ACPI_FAILURE (Status))
            {
                ACPI_ERROR_NAMESPACE (Path, Status);
                return_ACPI_STATUS (Status);
            }
        }
        break;
    }

    /* Common exit */

    if (!Op)
    {
        /* Create a new op */

        Op = AcpiPsAllocOp (WalkState->Opcode);
        if (!Op)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }
    }

    /* Initialize the op */

#if (defined (ACPI_NO_METHOD_EXECUTION) || defined (ACPI_CONSTANT_EVAL_ONLY))
    Op->Named.Path = ACPI_CAST_PTR (UINT8, Path);
#endif

    if (Node)
    {
        /*
         * Put the Node in the "op" object that the parser uses, so we
         * can get it again quickly when this scope is closed
         */
        Op->Common.Node = Node;
        Op->Named.Name = Node->Name.Integer;
    }

    AcpiPsAppendArg (AcpiPsGetParentScope (&WalkState->ParserState), Op);
    *OutOp = Op;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsLoad1EndOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during the loading of the namespace,
 *              both control methods and everything else.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsLoad1EndOp (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE (DsLoad1EndOp);


    Op = WalkState->Op;
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Op=%p State=%p\n", Op, WalkState));

    /* We are only interested in opcodes that have an associated name */

    if (!(WalkState->OpInfo->Flags & (AML_NAMED | AML_FIELD)))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Get the object type to determine if we should pop the scope */

    ObjectType = WalkState->OpInfo->ObjectType;

#ifndef ACPI_NO_METHOD_EXECUTION
    if (WalkState->OpInfo->Flags & AML_FIELD)
    {
        /*
         * If we are executing a method, do not create any namespace objects
         * during the load phase, only during execution.
         */
        if (!WalkState->MethodNode)
        {
            if (WalkState->Opcode == AML_FIELD_OP          ||
                WalkState->Opcode == AML_BANK_FIELD_OP     ||
                WalkState->Opcode == AML_INDEX_FIELD_OP)
            {
                Status = AcpiDsInitFieldObjects (Op, WalkState);
            }
        }
        return_ACPI_STATUS (Status);
    }

    /*
     * If we are executing a method, do not create any namespace objects
     * during the load phase, only during execution.
     */
    if (!WalkState->MethodNode)
    {
        if (Op->Common.AmlOpcode == AML_REGION_OP)
        {
            Status = AcpiExCreateRegion (Op->Named.Data, Op->Named.Length,
                        (ACPI_ADR_SPACE_TYPE) ((Op->Common.Value.Arg)->Common.Value.Integer),
                        WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }
        else if (Op->Common.AmlOpcode == AML_DATA_REGION_OP)
        {
            Status = AcpiExCreateRegion (Op->Named.Data, Op->Named.Length,
                        REGION_DATA_TABLE, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }
    }
#endif

    if (Op->Common.AmlOpcode == AML_NAME_OP)
    {
        /* For Name opcode, get the object type from the argument */

        if (Op->Common.Value.Arg)
        {
            ObjectType = (AcpiPsGetOpcodeInfo (
                (Op->Common.Value.Arg)->Common.AmlOpcode))->ObjectType;

            /* Set node type if we have a namespace node */

            if (Op->Common.Node)
            {
                Op->Common.Node->Type = (UINT8) ObjectType;
            }
        }
    }

    /*
     * If we are executing a method, do not create any namespace objects
     * during the load phase, only during execution.
     */
    if (!WalkState->MethodNode)
    {
        if (Op->Common.AmlOpcode == AML_METHOD_OP)
        {
            /*
             * MethodOp PkgLength NameString MethodFlags TermList
             *
             * Note: We must create the method node/object pair as soon as we
             * see the method declaration. This allows later pass1 parsing
             * of invocations of the method (need to know the number of
             * arguments.)
             */
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
                "LOADING-Method: State=%p Op=%p NamedObj=%p\n",
                WalkState, Op, Op->Named.Node));

            if (!AcpiNsGetAttachedObject (Op->Named.Node))
            {
                WalkState->Operands[0] = ACPI_CAST_PTR (void, Op->Named.Node);
                WalkState->NumOperands = 1;

                Status = AcpiDsCreateOperands (WalkState, Op->Common.Value.Arg);
                if (ACPI_SUCCESS (Status))
                {
                    Status = AcpiExCreateMethod (Op->Named.Data,
                                        Op->Named.Length, WalkState);
                }

                WalkState->Operands[0] = NULL;
                WalkState->NumOperands = 0;

                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }
            }
        }
    }

    /* Pop the scope stack (only if loading a table) */

    if (!WalkState->MethodNode &&
        AcpiNsOpensScope (ObjectType))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "(%s): Popping scope for Op %p\n",
            AcpiUtGetTypeName (ObjectType), Op));

        Status = AcpiDsScopeStackPop (WalkState);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsLoad2BeginOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              OutOp           - Wher to return op if a new one is created
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the loading of ACPI tables.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsLoad2BeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    ACPI_OBJECT_TYPE        ObjectType;
    char                    *BufferPtr;
    UINT32                  Flags;


    ACPI_FUNCTION_TRACE (DsLoad2BeginOp);


    Op = WalkState->Op;
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Op=%p State=%p\n", Op, WalkState));

    if (Op)
    {
        if ((WalkState->ControlState) &&
            (WalkState->ControlState->Common.State ==
                ACPI_CONTROL_CONDITIONAL_EXECUTING))
        {
            /* We are executing a while loop outside of a method */

            Status = AcpiDsExecBeginOp (WalkState, OutOp);
            return_ACPI_STATUS (Status);
        }

        /* We only care about Namespace opcodes here */

        if ((!(WalkState->OpInfo->Flags & AML_NSOPCODE)   &&
              (WalkState->Opcode != AML_INT_NAMEPATH_OP)) ||
            (!(WalkState->OpInfo->Flags & AML_NAMED)))
        {
            return_ACPI_STATUS (AE_OK);
        }

        /* Get the name we are going to enter or lookup in the namespace */

        if (WalkState->Opcode == AML_INT_NAMEPATH_OP)
        {
            /* For Namepath op, get the path string */

            BufferPtr = Op->Common.Value.String;
            if (!BufferPtr)
            {
                /* No name, just exit */

                return_ACPI_STATUS (AE_OK);
            }
        }
        else
        {
            /* Get name from the op */

            BufferPtr = ACPI_CAST_PTR (char, &Op->Named.Name);
        }
    }
    else
    {
        /* Get the namestring from the raw AML */

        BufferPtr = AcpiPsGetNextNamestring (&WalkState->ParserState);
    }

    /* Map the opcode into an internal object type */

    ObjectType = WalkState->OpInfo->ObjectType;

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "State=%p Op=%p Type=%X\n", WalkState, Op, ObjectType));

    switch (WalkState->Opcode)
    {
    case AML_FIELD_OP:
    case AML_BANK_FIELD_OP:
    case AML_INDEX_FIELD_OP:

        Node = NULL;
        Status = AE_OK;
        break;

    case AML_INT_NAMEPATH_OP:
        /*
         * The NamePath is an object reference to an existing object.
         * Don't enter the name into the namespace, but look it up
         * for use later.
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, BufferPtr, ObjectType,
                        ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
                        WalkState, &(Node));
        break;

    case AML_SCOPE_OP:

        /* Special case for Scope(\) -> refers to the Root node */

        if (Op && (Op->Named.Node == AcpiGbl_RootNode))
        {
            Node = Op->Named.Node;

            Status = AcpiDsScopeStackPush (Node, ObjectType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }
        else
        {
            /*
             * The Path is an object reference to an existing object.
             * Don't enter the name into the namespace, but look it up
             * for use later.
             */
            Status = AcpiNsLookup (WalkState->ScopeInfo, BufferPtr, ObjectType,
                        ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
                        WalkState, &(Node));
            if (ACPI_FAILURE (Status))
            {
#ifdef ACPI_ASL_COMPILER
                if (Status == AE_NOT_FOUND)
                {
                    Status = AE_OK;
                }
                else
                {
                    ACPI_ERROR_NAMESPACE (BufferPtr, Status);
                }
#else
                ACPI_ERROR_NAMESPACE (BufferPtr, Status);
#endif
                return_ACPI_STATUS (Status);
            }
        }

        /*
         * We must check to make sure that the target is
         * one of the opcodes that actually opens a scope
         */
        switch (Node->Type)
        {
        case ACPI_TYPE_ANY:
        case ACPI_TYPE_LOCAL_SCOPE:         /* Scope */
        case ACPI_TYPE_DEVICE:
        case ACPI_TYPE_POWER:
        case ACPI_TYPE_PROCESSOR:
        case ACPI_TYPE_THERMAL:

            /* These are acceptable types */
            break;

        case ACPI_TYPE_INTEGER:
        case ACPI_TYPE_STRING:
        case ACPI_TYPE_BUFFER:

            /*
             * These types we will allow, but we will change the type.
             * This enables some existing code of the form:
             *
             *  Name (DEB, 0)
             *  Scope (DEB) { ... }
             */
            ACPI_WARNING ((AE_INFO,
                "Type override - [%4.4s] had invalid type (%s) "
                "for Scope operator, changed to type ANY\n",
                AcpiUtGetNodeName (Node), AcpiUtGetTypeName (Node->Type)));

            Node->Type = ACPI_TYPE_ANY;
            WalkState->ScopeInfo->Common.Value = ACPI_TYPE_ANY;
            break;

        default:

            /* All other types are an error */

            ACPI_ERROR ((AE_INFO,
                "Invalid type (%s) for target of "
                "Scope operator [%4.4s] (Cannot override)",
                AcpiUtGetTypeName (Node->Type), AcpiUtGetNodeName (Node)));

            return (AE_AML_OPERAND_TYPE);
        }
        break;

    default:

        /* All other opcodes */

        if (Op && Op->Common.Node)
        {
            /* This op/node was previously entered into the namespace */

            Node = Op->Common.Node;

            if (AcpiNsOpensScope (ObjectType))
            {
                Status = AcpiDsScopeStackPush (Node, ObjectType, WalkState);
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }
            }

            return_ACPI_STATUS (AE_OK);
        }

        /*
         * Enter the named type into the internal namespace. We enter the name
         * as we go downward in the parse tree. Any necessary subobjects that
         * involve arguments to the opcode must be created as we go back up the
         * parse tree later.
         *
         * Note: Name may already exist if we are executing a deferred opcode.
         */
        if (WalkState->DeferredNode)
        {
            /* This name is already in the namespace, get the node */

            Node = WalkState->DeferredNode;
            Status = AE_OK;
            break;
        }

        Flags = ACPI_NS_NO_UPSEARCH;
        if (WalkState->PassNumber == ACPI_IMODE_EXECUTE)
        {
            /* Execution mode, node cannot already exist, node is temporary */

            Flags |= ACPI_NS_ERROR_IF_FOUND;

            if (!(WalkState->ParseFlags & ACPI_PARSE_MODULE_LEVEL))
            {
                Flags |= ACPI_NS_TEMPORARY;
            }
        }

        /* Add new entry or lookup existing entry */

        Status = AcpiNsLookup (WalkState->ScopeInfo, BufferPtr, ObjectType,
                    ACPI_IMODE_LOAD_PASS2, Flags, WalkState, &Node);

        if (ACPI_SUCCESS (Status) && (Flags & ACPI_NS_TEMPORARY))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
                "***New Node [%4.4s] %p is temporary\n",
                AcpiUtGetNodeName (Node), Node));
        }
        break;
    }

    if (ACPI_FAILURE (Status))
    {
        ACPI_ERROR_NAMESPACE (BufferPtr, Status);
        return_ACPI_STATUS (Status);
    }

    if (!Op)
    {
        /* Create a new op */

        Op = AcpiPsAllocOp (WalkState->Opcode);
        if (!Op)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Initialize the new op */

        if (Node)
        {
            Op->Named.Name = Node->Name.Integer;
        }
        *OutOp = Op;
    }

    /*
     * Put the Node in the "op" object that the parser uses, so we
     * can get it again quickly when this scope is closed
     */
    Op->Common.Node = Node;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsLoad2EndOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during the loading of the namespace,
 *              both control methods and everything else.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsLoad2EndOp (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_STATUS             Status = AE_OK;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_NAMESPACE_NODE     *NewNode;
#ifndef ACPI_NO_METHOD_EXECUTION
    UINT32                  i;
    UINT8                   RegionSpace;
#endif


    ACPI_FUNCTION_TRACE (DsLoad2EndOp);

    Op = WalkState->Op;
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Opcode [%s] Op %p State %p\n",
            WalkState->OpInfo->Name, Op, WalkState));

    /* Check if opcode had an associated namespace object */

    if (!(WalkState->OpInfo->Flags & AML_NSOBJECT))
    {
        return_ACPI_STATUS (AE_OK);
    }

    if (Op->Common.AmlOpcode == AML_SCOPE_OP)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "Ending scope Op=%p State=%p\n", Op, WalkState));
    }

    ObjectType = WalkState->OpInfo->ObjectType;

    /*
     * Get the Node/name from the earlier lookup
     * (It was saved in the *op structure)
     */
    Node = Op->Common.Node;

    /*
     * Put the Node on the object stack (Contains the ACPI Name of
     * this object)
     */
    WalkState->Operands[0] = (void *) Node;
    WalkState->NumOperands = 1;

    /* Pop the scope stack */

    if (AcpiNsOpensScope (ObjectType) &&
       (Op->Common.AmlOpcode != AML_INT_METHODCALL_OP))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "(%s) Popping scope for Op %p\n",
            AcpiUtGetTypeName (ObjectType), Op));

        Status = AcpiDsScopeStackPop (WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
    }

    /*
     * Named operations are as follows:
     *
     * AML_ALIAS
     * AML_BANKFIELD
     * AML_CREATEBITFIELD
     * AML_CREATEBYTEFIELD
     * AML_CREATEDWORDFIELD
     * AML_CREATEFIELD
     * AML_CREATEQWORDFIELD
     * AML_CREATEWORDFIELD
     * AML_DATA_REGION
     * AML_DEVICE
     * AML_EVENT
     * AML_FIELD
     * AML_INDEXFIELD
     * AML_METHOD
     * AML_METHODCALL
     * AML_MUTEX
     * AML_NAME
     * AML_NAMEDFIELD
     * AML_OPREGION
     * AML_POWERRES
     * AML_PROCESSOR
     * AML_SCOPE
     * AML_THERMALZONE
     */

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "Create-Load [%s] State=%p Op=%p NamedObj=%p\n",
        AcpiPsGetOpcodeName (Op->Common.AmlOpcode), WalkState, Op, Node));

    /* Decode the opcode */

    Arg = Op->Common.Value.Arg;

    switch (WalkState->OpInfo->Type)
    {
#ifndef ACPI_NO_METHOD_EXECUTION

    case AML_TYPE_CREATE_FIELD:
        /*
         * Create the field object, but the field buffer and index must
         * be evaluated later during the execution phase
         */
        Status = AcpiDsCreateBufferField (Op, WalkState);
        break;


     case AML_TYPE_NAMED_FIELD:
        /*
         * If we are executing a method, initialize the field
         */
        if (WalkState->MethodNode)
        {
            Status = AcpiDsInitFieldObjects (Op, WalkState);
        }

        switch (Op->Common.AmlOpcode)
        {
        case AML_INDEX_FIELD_OP:

            Status = AcpiDsCreateIndexField (Op, (ACPI_HANDLE) Arg->Common.Node,
                        WalkState);
            break;

        case AML_BANK_FIELD_OP:

            Status = AcpiDsCreateBankField (Op, Arg->Common.Node, WalkState);
            break;

        case AML_FIELD_OP:

            Status = AcpiDsCreateField (Op, Arg->Common.Node, WalkState);
            break;

        default:
            /* All NAMED_FIELD opcodes must be handled above */
            break;
        }
        break;


     case AML_TYPE_NAMED_SIMPLE:

        Status = AcpiDsCreateOperands (WalkState, Arg);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        switch (Op->Common.AmlOpcode)
        {
        case AML_PROCESSOR_OP:

            Status = AcpiExCreateProcessor (WalkState);
            break;

        case AML_POWER_RES_OP:

            Status = AcpiExCreatePowerResource (WalkState);
            break;

        case AML_MUTEX_OP:

            Status = AcpiExCreateMutex (WalkState);
            break;

        case AML_EVENT_OP:

            Status = AcpiExCreateEvent (WalkState);
            break;


        case AML_ALIAS_OP:

            Status = AcpiExCreateAlias (WalkState);
            break;

        default:
            /* Unknown opcode */

            Status = AE_OK;
            goto Cleanup;
        }

        /* Delete operands */

        for (i = 1; i < WalkState->NumOperands; i++)
        {
            AcpiUtRemoveReference (WalkState->Operands[i]);
            WalkState->Operands[i] = NULL;
        }

        break;
#endif /* ACPI_NO_METHOD_EXECUTION */

    case AML_TYPE_NAMED_COMPLEX:

        switch (Op->Common.AmlOpcode)
        {
#ifndef ACPI_NO_METHOD_EXECUTION
        case AML_REGION_OP:
        case AML_DATA_REGION_OP:

            if (Op->Common.AmlOpcode == AML_REGION_OP)
            {
                RegionSpace = (ACPI_ADR_SPACE_TYPE)
                      ((Op->Common.Value.Arg)->Common.Value.Integer);
            }
            else
            {
                RegionSpace = REGION_DATA_TABLE;
            }

            /*
             * The OpRegion is not fully parsed at this time. The only valid
             * argument is the SpaceId. (We must save the address of the
             * AML of the address and length operands)
             *
             * If we have a valid region, initialize it. The namespace is
             * unlocked at this point.
             *
             * Need to unlock interpreter if it is locked (if we are running
             * a control method), in order to allow _REG methods to be run
             * during AcpiEvInitializeRegion.
             */
            if (WalkState->MethodNode)
            {
                /*
                 * Executing a method: initialize the region and unlock
                 * the interpreter
                 */
                Status = AcpiExCreateRegion (Op->Named.Data, Op->Named.Length,
                            RegionSpace, WalkState);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                AcpiExExitInterpreter ();
            }

            Status = AcpiEvInitializeRegion (AcpiNsGetAttachedObject (Node),
                        FALSE);
            if (WalkState->MethodNode)
            {
                AcpiExEnterInterpreter ();
            }

            if (ACPI_FAILURE (Status))
            {
                /*
                 *  If AE_NOT_EXIST is returned, it is not fatal
                 *  because many regions get created before a handler
                 *  is installed for said region.
                 */
                if (AE_NOT_EXIST == Status)
                {
                    Status = AE_OK;
                }
            }
            break;


        case AML_NAME_OP:

            Status = AcpiDsCreateNode (WalkState, Node, Op);
            break;


        case AML_METHOD_OP:
            /*
             * MethodOp PkgLength NameString MethodFlags TermList
             *
             * Note: We must create the method node/object pair as soon as we
             * see the method declaration. This allows later pass1 parsing
             * of invocations of the method (need to know the number of
             * arguments.)
             */
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
                "LOADING-Method: State=%p Op=%p NamedObj=%p\n",
                WalkState, Op, Op->Named.Node));

            if (!AcpiNsGetAttachedObject (Op->Named.Node))
            {
                WalkState->Operands[0] = ACPI_CAST_PTR (void, Op->Named.Node);
                WalkState->NumOperands = 1;

                Status = AcpiDsCreateOperands (WalkState, Op->Common.Value.Arg);
                if (ACPI_SUCCESS (Status))
                {
                    Status = AcpiExCreateMethod (Op->Named.Data,
                                        Op->Named.Length, WalkState);
                }
                WalkState->Operands[0] = NULL;
                WalkState->NumOperands = 0;

                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }
            }
            break;

#endif /* ACPI_NO_METHOD_EXECUTION */

        default:
            /* All NAMED_COMPLEX opcodes must be handled above */
            break;
        }
        break;


    case AML_CLASS_INTERNAL:

        /* case AML_INT_NAMEPATH_OP: */
        break;


    case AML_CLASS_METHOD_CALL:

        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "RESOLVING-MethodCall: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        /*
         * Lookup the method name and save the Node
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, Arg->Common.Value.String,
                        ACPI_TYPE_ANY, ACPI_IMODE_LOAD_PASS2,
                        ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
                        WalkState, &(NewNode));
        if (ACPI_SUCCESS (Status))
        {
            /*
             * Make sure that what we found is indeed a method
             * We didn't search for a method on purpose, to see if the name
             * would resolve
             */
            if (NewNode->Type != ACPI_TYPE_METHOD)
            {
                Status = AE_AML_OPERAND_TYPE;
            }

            /* We could put the returned object (Node) on the object stack for
             * later, but for now, we will put it in the "op" object that the
             * parser uses, so we can get it again at the end of this scope
             */
            Op->Common.Node = NewNode;
        }
        else
        {
            ACPI_ERROR_NAMESPACE (Arg->Common.Value.String, Status);
        }
        break;


    default:
        break;
    }

Cleanup:

    /* Remove the Node pushed at the very beginning */

    WalkState->Operands[0] = NULL;
    WalkState->NumOperands = 0;
    return_ACPI_STATUS (Status);
}


